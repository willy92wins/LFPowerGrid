#!/usr/bin/env python3
"""Offline pre-compile sweeps for the LFPowerGrid addon.

Enforce Script only compiles inside DayZ, so a real build gate is impossible in
CI. What this catches instead are failure classes that otherwise cost a server
boot to discover, plus the file-corruption modes this tree is exposed to by
living on a synced drive.

Checks
  BOM              EF BB BF at byte 0. The Enforce parser reports an unclosed
                   quoted string on line 1, pointing at a quote that does not
                   exist, which sends debugging in the wrong direction.
  NUL_BYTES        NUL inside a text file: an interrupted or racing write.
  BALANCE          Unbalanced {} () [] after removing strings and comments.
                   Detects truncation, which is silent otherwise.
  DUP_CLASS        The same class declared twice under conditions that can
                   hold at once. Exclusive branches (#ifdef SERVER / #else,
                   or #ifndef SERVER vs #ifdef SERVER) are the client/server
                   split and are legitimate. Two #ifdef SERVER blocks, or an
                   unconditional declaration plus one inside #ifdef SERVER,
                   are duplicates: both compile when SERVER is defined.
                   #define and #undef are not modelled, so a nested #ifndef X
                   inside #ifdef X is still compared and can report a duplicate
                   that never compiles (dead code; fails closed).
  PREPROC-BALANCE  #else/#elif/#endif with no open #if* before it, or an #if*
                   still open at end of file. A stray #endif after an edit
                   silently reshapes which code each side compiles; the brace
                   BALANCE check does not see it.
  PREPROC-CONTRA   (warning) #ifndef X nested inside #ifdef X: dead code,
                   never compiled. Skipped when the file #defines or #undefs X.
  FILEHANDLE_INIT  FileHandle initialized to a numeric literal; diag rejects it.
  CHAINED_REPLACE  Replace is not chainable in Enforce.

LFCOM's script also warns on identifiers beginning with keyword+digit. That is
not carried over: LFPG_LogicGate.c uses in0/in1 as parameter names throughout a
shipped, working build, so the rule is too broad here. Whatever narrower
condition produced it upstream has not been established, so the check is dropped
rather than kept as permanent noise.

Only .c files are treated as Enforce Script. config.cpp is a config, not a
script: the same class name legitimately recurs there (one AnimationSources per
vehicle class), and a config class and a script class of the same name are
different namespaces.

Ported from LFCOM_dev/tools/enforce-checks.ps1, with one correction: string
literals must be removed BEFORE line comments. Stripping // first truncates any
line holding a URL ("https://...") at the scheme separator, taking the closing
quote with it and corrupting every count that follows.

Usage: python enforce_checks.py [--root DIR] [--strict]
Exit: 0 clean, 1 failures, 2 bad invocation. --strict also fails on warnings.
"""

import argparse
import os
import re
import sys

ENFORCE_EXT = {".c"}
TEXT_EXT = ENFORCE_EXT | {".cpp", ".layout", ".rvmat", ".csv", ".txt", ".xml", ".cfg"}

BOM = b"\xef\xbb\xbf"

STRING_LITERAL = re.compile(r'"(?:[^"\\\n]|\\.)*"')
BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.S)
LINE_COMMENT = re.compile(r"//.*$", re.M)
CLASS_DECL = re.compile(r"^\s*(modded\s+)?class\s+([A-Za-z0-9_]+)", re.M)
PREPROC = re.compile(r"^\s*#\s*(ifdef|ifndef|else|elif|endif)\b\s*(\w*)", re.M)
PREPROC_ANY = re.compile(r"^\s*#\s*(if|ifdef|ifndef|else|elif|endif|define|undef)\b\s*(\w*)")
FILEHANDLE_NUM_INIT = re.compile(r"^\s*FileHandle\s+\w+\s*=\s*\d", re.M)
CHAINED_REPLACE = re.compile(r"\.Replace\([^)]*\)\s*\.\s*Replace\(")


class Report:
    def __init__(self):
        self.fails = []
        self.warns = []

    def fail(self, kind, where, detail):
        self.fails.append((kind, where, detail))
        print("FAIL | %-16s | %s | %s" % (kind, where, detail))

    def warn(self, kind, where, detail):
        self.warns.append((kind, where, detail))
        print("WARN | %-16s | %s | %s" % (kind, where, detail))


def strip_noncode(raw):
    """Blank out string literals, then comments. Order matters: see module docstring.

    Newlines are preserved so reported line numbers stay meaningful.
    """
    code = STRING_LITERAL.sub('""', raw)
    code = BLOCK_COMMENT.sub(lambda m: "\n" * m.group(0).count("\n"), code)
    code = LINE_COMMENT.sub("", code)
    return code


def _is_literal(item):
    """True when item is a (symbol, defined) preprocessor literal."""
    return isinstance(item, tuple) and len(item) == 2 and isinstance(item[1], bool)


def _is_literal_path(path):
    for item in path:
        if not _is_literal(item):
            return False
    return True


def conditions_compatible(path_a, path_b):
    """True when both preprocessor conditions can hold at the same time.

    A symbol that is defined in one path and undefined in the other makes
    the pair exclusive. #elif and other non-literal markers keep the old
    exact-path comparison (they are not modelled as SAT literals).
    """
    if not _is_literal_path(path_a) or not _is_literal_path(path_b):
        return path_a == path_b
    polar_a = {}
    polar_b = {}
    for sym, defined in path_a:
        polar_a.setdefault(sym, set()).add(defined)
    for sym, defined in path_b:
        polar_b.setdefault(sym, set()).add(defined)
    for sym in polar_a:
        if sym not in polar_b:
            continue
        if True in polar_a[sym] and False in polar_b[sym]:
            return False
        if False in polar_a[sym] and True in polar_b[sym]:
            return False
    return True


def branch_at_line(code):
    """Map each line number to the active preprocessor condition.

    The path is a tuple of literals (symbol, defined). #ifdef X pushes
    (X, True); #ifndef X pushes (X, False); #else inverts the last literal;
    #endif pops. No per-block counter: two #ifdef SERVER blocks share the
    same condition. #elif (and #else after a non-literal) keep the historical
    unique-suffix mutation so complex branches stay exclusive by identity.
    """
    path = []
    out = {}
    for lineno, line in enumerate(code.split("\n"), 1):
        m = PREPROC.match(line)
        if m:
            kind, sym = m.group(1), m.group(2)
            if kind == "ifdef":
                path.append((sym, True))
            elif kind == "ifndef":
                path.append((sym, False))
            elif kind == "else" and path:
                last = path[-1]
                if _is_literal(last):
                    path[-1] = (last[0], not last[1])
                else:
                    path[-1] = str(last) + "|else"
            elif kind == "elif" and path:
                path[-1] = str(path[-1]) + "|elif"
            elif kind == "endif" and path:
                path.pop()
        out[lineno] = tuple(path)
    return out


def preproc_balance(code):
    """Return (errors, contradictions) as lists of (lineno, detail).

    errors: orphan #else/#elif/#endif, and #if* left open at end of file.
    contradictions: #ifndef X opened while an enclosing #ifdef X is still in
    its true branch, unless the file #defines or #undefs X anywhere.
    """
    errors = []
    contra = []
    stack = []
    redefined = set()
    lines = code.split("\n")
    for line in lines:
        m = PREPROC_ANY.match(line)
        if m and m.group(1) in ("define", "undef"):
            redefined.add(m.group(2))
    for lineno, line in enumerate(lines, 1):
        m = PREPROC_ANY.match(line)
        if not m:
            continue
        kind, sym = m.group(1), m.group(2)
        if kind in ("if", "ifdef", "ifndef"):
            if kind == "ifndef" and sym not in redefined:
                for entry in stack:
                    if entry[0] == "ifdef" and entry[1] == sym and not entry[3]:
                        contra.append((lineno, "#ifndef %s inside #ifdef %s at line %d"
                                       % (sym, sym, entry[2])))
                        break
            stack.append([kind, sym, lineno, False])
        elif kind in ("else", "elif"):
            if not stack:
                errors.append((lineno, "#%s with no open #if*" % kind))
            else:
                stack[-1][3] = True
        elif kind == "endif":
            if not stack:
                errors.append((lineno, "#endif with no open #if*"))
            else:
                stack.pop()
    for entry in stack:
        errors.append((entry[2], "#%s %s not closed before end of file"
                       % (entry[0], entry[1])))
    return errors, contra


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.abspath(__file__)))))
    ap.add_argument("--strict", action="store_true", help="treat warnings as failures")
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    if not os.path.isdir(root):
        print("bad --root: %s" % root)
        return 2

    rep = Report()
    declarations = {}
    n_text = n_enforce = 0

    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in (".git", ".github")]
        for fn in sorted(filenames):
            path = os.path.join(dirpath, fn)
            rel = os.path.relpath(path, root).replace(os.sep, "/")
            ext = os.path.splitext(fn)[1].lower()
            if ext not in TEXT_EXT:
                continue

            with open(path, "rb") as fh:
                data = fh.read()
            n_text += 1

            if ext in ENFORCE_EXT and data.startswith(BOM):
                rep.fail("BOM", rel, "EF BB BF at byte 0 breaks CParser")
            if b"\x00" in data:
                rep.fail("NUL_BYTES", rel,
                         "%d NUL bytes: interrupted write" % data.count(b"\x00"))

            if ext not in ENFORCE_EXT:
                continue
            n_enforce += 1

            code = strip_noncode(data.decode("utf-8", errors="replace"))

            for opener, closer in (("{", "}"), ("(", ")"), ("[", "]")):
                n_open, n_close = code.count(opener), code.count(closer)
                if n_open != n_close:
                    rep.fail("BALANCE", rel, "%s=%d %s=%d (truncated or unclosed)"
                             % (opener, n_open, closer, n_close))

            pp_errors, pp_contra = preproc_balance(code)
            for lineno, detail in pp_errors:
                rep.fail("PREPROC-BALANCE", "%s:%d" % (rel, lineno), detail)
            for lineno, detail in pp_contra:
                rep.warn("PREPROC-CONTRA", "%s:%d" % (rel, lineno), detail)

            branches = branch_at_line(code)
            for m in CLASS_DECL.finditer(code):
                lineno = code.count("\n", 0, m.start()) + 1
                path = branches.get(lineno, ())
                key = (m.group(2), bool(m.group(1)))
                site = "%s:%d" % (rel, lineno)
                declarations.setdefault(key, []).append((path, site))

            if FILEHANDLE_NUM_INIT.search(code):
                rep.fail("FILEHANDLE_INIT", rel,
                         "FileHandle = <num> is rejected by diag; declare it uninitialized")
            if CHAINED_REPLACE.search(code):
                rep.fail("CHAINED_REPLACE", rel, "Replace is not chainable in Enforce")

    for (name, is_modded), entries in sorted(declarations.items()):
        n = len(entries)
        used = [False] * n
        for i in range(n):
            for j in range(i + 1, n):
                if conditions_compatible(entries[i][0], entries[j][0]):
                    used[i] = True
                    used[j] = True
        sites = []
        for k in range(n):
            if used[k]:
                sites.append(entries[k][1])
        if len(sites) > 1:
            rep.fail("DUP_CLASS", name, "%sclass declared at: %s"
                     % ("modded " if is_modded else "", ", ".join(sites)))

    print()
    print("checked %d text files (%d Enforce) under %s" % (n_text, n_enforce, root))
    print("FAIL=%d  WARN=%d" % (len(rep.fails), len(rep.warns)))
    if rep.fails or (args.strict and rep.warns):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
