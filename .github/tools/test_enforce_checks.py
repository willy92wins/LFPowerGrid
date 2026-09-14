#!/usr/bin/env python3
"""Negative controls for enforce_checks.py.

A checker reporting zero failures proves nothing until it is shown to fire. Each
test plants one defect and asserts the corresponding code appears; the last two
assert the checker stays quiet on constructs that are legitimate here and were
false positives in earlier revisions.

Run: python test_enforce_checks.py
"""

import os
import subprocess
import sys
import tempfile
import unittest

CHECKER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "enforce_checks.py")

CLEAN = b"""class LFPG_Thing
{
    void LFPG_Thing()
    {
        int x = 1;
    }
};
"""


def run_on(files):
    """Write {relative path: bytes} into a temp tree and run the checker on it."""
    with tempfile.TemporaryDirectory() as tmp:
        for rel, data in files.items():
            path = os.path.join(tmp, rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "wb") as fh:
                fh.write(data)
        proc = subprocess.run([sys.executable, CHECKER, "--root", tmp],
                              capture_output=True, text=True)
        return proc.returncode, proc.stdout


class DetectsDefects(unittest.TestCase):
    def assert_fires(self, code, files):
        rc, out = run_on(files)
        self.assertIn(code, out, "expected %s in:\n%s" % (code, out))
        self.assertEqual(rc, 1, "a planted defect must fail the run")

    def test_bom(self):
        self.assert_fires("BOM", {"a.c": b"\xef\xbb\xbf" + CLEAN})

    def test_nul_bytes(self):
        self.assert_fires("NUL_BYTES", {"a.c": CLEAN + b"\x00\x00"})

    def test_unbalanced_braces(self):
        self.assert_fires("BALANCE", {"a.c": b"class A\n{\n    void f()\n    {\n};\n"})

    def test_truncated_file(self):
        self.assert_fires("BALANCE", {"a.c": CLEAN[:len(CLEAN) // 2]})

    def test_duplicate_class_same_branch(self):
        self.assert_fires("DUP_CLASS", {"a.c": CLEAN, "b.c": CLEAN})

    def test_duplicate_class_twice_in_one_file(self):
        self.assert_fires("DUP_CLASS", {"a.c": CLEAN + CLEAN})

    def test_duplicate_class_outside_and_inside_server(self):
        """Unconditional class plus the same name under #ifdef SERVER.

        Both compile when SERVER is defined. Fixture:
        checker-counterexamples.json negative_duplicate_outside_and_inside_server.
        """
        self.assert_fires(
            "DUP_CLASS",
            {"a.c": b"class AuditSame { void F() {} };\n"
                    b"#ifdef SERVER\n"
                    b"class AuditSame { void F() {} };\n"
                    b"#endif\n"})

    def test_duplicate_class_separate_server_blocks(self):
        """Two #ifdef SERVER blocks declaring the same class.

        Fixture: checker-counterexamples.json
        negative_duplicate_separate_server_blocks.
        """
        self.assert_fires(
            "DUP_CLASS",
            {"a.c": b"#ifdef SERVER\n"
                    b"class AuditSame { void F() {} };\n"
                    b"#endif\n"
                    b"#ifdef SERVER\n"
                    b"class AuditSame { void F() {} };\n"
                    b"#endif\n"})

    def test_duplicate_class_after_define_in_nested_path(self):
        """A #define in nested #ifndef/#ifdef can compile a second class."""
        self.assert_fires(
            "DUP_CLASS",
            {"a.c": b"class AuditSame {};\n"
                    b"#ifndef LFPG_SWITCH\n"
                    b"#define LFPG_SWITCH\n"
                    b"#ifdef LFPG_SWITCH\n"
                    b"class AuditSame {};\n"
                    b"#endif\n"
                    b"#endif\n"})

    def test_filehandle_numeric_init(self):
        self.assert_fires("FILEHANDLE_INIT",
                          {"a.c": b"class A\n{\n    FileHandle h = 0;\n};\n"})

    def test_chained_replace(self):
        self.assert_fires(
            "CHAINED_REPLACE",
            {"a.c": b'class A\n{\n    void f()\n    {\n'
                    b'        string s = t.Replace("a", "b").Replace("c", "d");\n'
                    b"    }\n};\n"})


class StaysQuiet(unittest.TestCase):
    def assert_clean(self, files):
        rc, out = run_on(files)
        self.assertEqual(rc, 0, "unexpected failure:\n%s" % out)
        # "FAIL |" is the finding prefix; the summary line always says "FAIL=0".
        self.assertNotIn("FAIL |", out, "unexpected finding:\n%s" % out)
        self.assertIn("FAIL=0", out)

    def test_clean_file(self):
        self.assert_clean({"a.c": CLEAN})

    def test_url_in_string_does_not_break_brace_counting(self):
        """Regression: stripping // before string literals truncated any line
        holding a URL at the scheme separator, dropping the closing quote and
        corrupting every subsequent count. Cost: a false BALANCE failure on
        LFPG_BTCConfig.c, whose only sin was "https://api.binance.com"."""
        self.assert_clean({"a.c": b'class A\n{\n    void f()\n    {\n'
                                  b'        string u = "https://api.example.com";\n'
                                  b"    }\n};\n"})

    def test_same_class_in_exclusive_preprocessor_branches(self):
        """The client/server split declares one class per side. Legitimate."""
        self.assert_clean({"a.c": b"#ifndef SERVER\n" + CLEAN + b"#endif\n"
                                  b"#ifdef SERVER\n" + CLEAN + b"#endif\n"})

    def test_same_class_ifdef_server_else(self):
        """#ifdef SERVER / #else is exclusive. Fixture:
        checker-counterexamples.json positive_exclusive_server_client.
        """
        self.assert_clean(
            {"a.c": b"#ifdef SERVER\n"
                    b"class AuditSame { void F() {} };\n"
                    b"#else\n"
                    b"class AuditSame { void F() {} };\n"
                    b"#endif\n"})

    def test_config_cpp_is_not_enforce_script(self):
        """config.cpp repeats nested class names by design (one per entity)."""
        self.assert_clean({"config.cpp": b"class CfgVehicles\n{\n"
                                         b"    class A { class Health {}; };\n"
                                         b"    class B { class Health {}; };\n};\n"})


if __name__ == "__main__":
    unittest.main(verbosity=2)
