"""Run a restricted scalar source slice with Python stubs, not the Enforce VM.

Tests exercise the actual statements instead of a second implementation. Engine
callbacks, types, preprocessing, side effects and numeric precision need DayZ.
"""
import re


def method(source, name):
    source = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)
    match = re.search(r"\b" + re.escape(name) + r"\([^;\n]*\)\s*\{", source)
    if not match:
        raise ValueError("method missing: " + name)
    start = match.end()
    depth, pos = 1, start
    while depth:
        depth += (source[pos] == "{") - (source[pos] == "}")
        pos += 1
    return re.sub(r"^\s*#.*$", "", source[start:pos - 1], flags=re.M)


def scalar_function(code, arguments):
    """Only if/else, declarations, assignment, return and stubbed calls."""
    tokens = re.findall(r'"(?:\\.|[^"\\])*"|[^{};]+|[{};]', code)
    # for/continue allowed. while/switch/new/delete still fail closed.
    if re.search(r"\b(while|switch|new|delete)\b", code):
        raise ValueError("unsupported scalar slice")
    # Keep ";" so a for-header split on semicolons can be reassembled.
    tokens = [token.strip() for token in tokens if token.strip()]
    cursor = 0
    lines = ["def run(" + ", ".join(arguments) + "):"]

    def expression(value):
        value = value.replace("&&", " and ").replace("||", " or ")
        value = re.sub(r"!(?!=)", "not ", value)
        value = re.sub(r"\btrue\b", "True", value)
        value = re.sub(r"\bfalse\b", "False", value)
        return re.sub(r"\bnull\b", "None", value)

    def statement(level):
        nonlocal cursor
        try:
            token = tokens[cursor]
        except IndexError:
            raise ValueError("unsupported construct: statement past end of tokens")
        cursor += 1
        indent = "    " * level
        if token == ";":
            return
        if token == "{":
            while True:
                if cursor >= len(tokens):
                    raise ValueError("unsupported construct: unclosed brace")
                if tokens[cursor] == "}":
                    break
                statement(level)
            cursor += 1
            return
        if re.match(r"^(if|else if)\s*\(", token):
            # Match the first balanced guard, not a later call's closing paren.
            opening, depth = token.index("("), 1
            end = opening + 1
            while depth:
                if end >= len(token):
                    raise ValueError("unsupported if: unclosed parenthesis")
                depth += (token[end] == "(") - (token[end] == ")")
                end += 1
            keyword = "elif" if token.startswith("else if") else "if"
            lines.append(indent + keyword + " " + expression(token[opening + 1:end - 1]) + ":")
            tail = token[end:].strip()
            if tail:
                tokens.insert(cursor, tail)
            statement(level + 1)
            return
        if token == "else":
            lines.append(indent + "else:")
            statement(level + 1)
            return
        if token == "continue":
            lines.append(indent + "continue")
            return
        if re.match(r"^for\s*\(", token):
            depth = token.count("(") - token.count(")")
            while depth > 0:
                if cursor >= len(tokens):
                    raise ValueError("unsupported for: unclosed parenthesis")
                nxt = tokens[cursor]
                cursor += 1
                token = token + (" " if nxt != ";" else "") + nxt
                depth += nxt.count("(") - nxt.count(")")
            try:
                opening, depth = token.index("("), 1
                end = opening + 1
                while depth:
                    if end >= len(token):
                        raise ValueError("unsupported for: unclosed parenthesis")
                    depth += (token[end] == "(") - (token[end] == ")")
                    end += 1
            except (IndexError, ValueError) as ex:
                raise ValueError("unsupported for: %s" % ex)
            inner = token[opening + 1:end - 1]
            parts = [p.strip() for p in inner.split(";")]
            if len(parts) != 3:
                raise ValueError("unsupported for")
            init, cond, incr = parts
            range_for = re.match(
                r"^(\w+)\s*=\s*0$",
                init,
            )
            range_cond = re.match(r"^(\w+)\s*<\s*(.+)$", cond) if range_for else None
            range_incr = re.match(
                r"^(\w+)\s*=\s*\1\s*\+\s*1$",
                re.sub(r"\s+", "", incr),
            ) if range_for else None
            if range_for and range_cond and range_incr and range_for.group(1) == range_cond.group(1):
                lines.append(
                    indent
                    + "for "
                    + range_for.group(1)
                    + " in range("
                    + expression(range_cond.group(2))
                    + "):"
                )
                statement(level + 1)
                return
            lines.append(indent + expression(init))
            lines.append(indent + "while " + expression(cond) + ":")
            statement(level + 1)
            lines.append("    " * (level + 1) + expression(incr))
            return
        token = re.sub(
            r"^(?:ref\s+)?(?:float|int|bool|EntityAI|LFPG_ElecNode|array<[^>]+>)\s+",
            "",
            token,
        )
        lines.append(indent + expression(token))

    while cursor < len(tokens):
        statement(1)
    return "\n".join(lines) + "\n"


def load(source, name, arguments, stubs):
    environment = {"__builtins__": {}, **stubs}
    try:
        compiled = scalar_function(method(source, name), arguments)
        exec(compile(compiled, "<source-slice:" + name + ">", "exec"), environment)
    except (ValueError, IndexError, SyntaxError) as ex:
        raise ValueError("unsupported construct in %s: %s" % (name, ex))
    return environment["run"]
