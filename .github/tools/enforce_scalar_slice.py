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
    # Slices used here have no string literals; fail closed on unsupported syntax.
    if '"' in code or re.search(r"\b(for|while|switch|new|delete)\b", code):
        raise ValueError("unsupported scalar slice")
    tokens = [token.strip() for token in tokens if token.strip() and token != ";"]
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
        token = tokens[cursor]
        cursor += 1
        indent = "    " * level
        if token == "{":
            while tokens[cursor] != "}":
                statement(level)
            cursor += 1
            return
        if re.match(r"^(if|else if)\s*\(", token):
            # Match the first balanced guard, not a later call's closing paren.
            opening, depth = token.index("("), 1
            end = opening + 1
            while depth:
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
        token = re.sub(r"^(?:ref\s+)?(?:float|int|bool|EntityAI|LFPG_ElecNode)\s+", "", token)
        lines.append(indent + expression(token))

    while cursor < len(tokens):
        statement(1)
    return "\n".join(lines) + "\n"


def load(source, name, arguments, stubs):
    environment = {"__builtins__": {}, **stubs}
    compiled = scalar_function(method(source, name), arguments)
    exec(compile(compiled, "<source-slice:" + name + ">", "exec"), environment)
    return environment["run"]
