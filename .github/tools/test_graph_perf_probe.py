"""Offline checks for R-01/R-02/R-04/R-05 measurement probes (issue #78).

Does not execute Enforce. Positives require the four call sites and a default-off
flag. Negatives fail when any site is stripped or the flag is true.
No test reads git HEAD (that turns red after the probe commit).
"""
from pathlib import Path
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
DEFINES = ROOT / "scripts/3_Game/LFPG_Defines.c"
GRAPH = ROOT / "scripts/5_Mission/LFPG_ElecGraphImpl.c"
RPC = ROOT / "scripts/5_Mission/LFPG_RPCServerHandlerImpl.c"
PROBE = ROOT / "scripts/3_Game/LFPG_PerfProbe.c"
CHECKS = ROOT / ".github/workflows/checks.yml"
SCRIPTS = ROOT / "scripts"

DIR_RE = re.compile(r"^\s*#\s*(ifdef|ifndef|else|endif)\b(?:\s+(\w+))?")
CALL_RE = re.compile(r"LFPG_PerfProbe\.")
CLASS_RE = re.compile(r"\bclass\s+LFPG_PerfProbe\b")


def _src(path):
    return path.read_text(encoding="utf-8")


def probe_off(text):
    match = re.search(r"static const bool\s+LFPG_PERF_PROBE\s*=\s*(true|false)\s*;", text)
    return bool(match) and match.group(1) == "false"


def has_r01(text):
    start = text.find("static void HandleCutWires")
    if start < 0:
        return False
    end = text.find("static void HandleRequestCameraList", start)
    body = text[start:end]
    return (
        'LFPG_PerfProbe.Begin("R01")' in body
        and "LFPG_PerfProbe.End()" in body
        and 'LFPG_PerfProbe.Phase("index")' in body
        and 'LFPG_PerfProbe.Phase("rescue")' in body
        and 'LFPG_PerfProbe.Phase("rebuild")' in body
    )


def has_r02(text):
    start = text.find("protected void MarkUpstreamNodesDirty")
    if start < 0:
        return False
    end = text.find("protected void ClearPropagationMemos", start)
    body = text[start:end]
    return (
        'LFPG_PerfProbe.Begin("R02")' in body
        and "LFPG_PerfProbe.AddAlloc(2)" in body
        and "LFPG_PerfProbe.End()" in body
    )


def has_r04(text):
    start = text.find("static void HandleCutPort")
    if start < 0:
        return False
    end = text.find("protected static string IncomingPortIndexKey", start)
    body = text[start:end]
    return (
        'LFPG_PerfProbe.Begin("R04")' in body
        and 'LFPG_PerfProbe.Phase("index")' in body
        and 'LFPG_PerfProbe.Phase("rescue")' in body
        and "LFPG_PerfProbe.End()" in body
    )


def has_r05(text):
    return (
        text.count("LFPG_PerfProbe.CountMapSweep") >= 8
        and 'CountMapSweep("m_Nodes"' in text
        and 'CountMapSweep("m_WdgVisited"' in text
    )


def _active_lines(text, server_defined):
    """Yield (lineno, line) compiled when SERVER is defined/undefined."""
    stack = [True]
    for lineno, raw in enumerate(text.splitlines(), 1):
        stripped = raw.strip()
        if stripped.startswith("//"):
            continue
        match = DIR_RE.match(stripped)
        if match:
            kind, name = match.group(1), match.group(2)
            parent = stack[-1]
            if kind == "ifdef":
                if name == "SERVER":
                    stack.append(parent and server_defined)
                else:
                    stack.append(parent)
            elif kind == "ifndef":
                if name == "SERVER":
                    stack.append(parent and not server_defined)
                else:
                    stack.append(parent)
            elif kind == "else":
                if len(stack) < 2:
                    continue
                parent = stack[-2]
                stack[-1] = parent and not stack[-1]
            elif kind == "endif":
                if len(stack) > 1:
                    stack.pop()
            continue
        if stack[-1]:
            yield lineno, raw


def class_builds(text):
    server = client = False
    for _, line in _active_lines(text, True):
        if CLASS_RE.search(line):
            server = True
    for _, line in _active_lines(text, False):
        if CLASS_RE.search(line):
            client = True
    return server, client


def call_builds(text):
    """List (lineno, line, server, client) for LFPG_PerfProbe. call sites."""
    server_set = {n for n, line in _active_lines(text, True) if CALL_RE.search(line)}
    client_set = {n for n, line in _active_lines(text, False) if CALL_RE.search(line)}
    lines = text.splitlines()
    found = []
    for n, line in enumerate(lines, 1):
        if not CALL_RE.search(line):
            continue
        found.append((n, line, n in server_set, n in client_set))
    return found


def probe_visible_in_all_call_builds(probe_text, call_files):
    class_server, class_client = class_builds(probe_text)
    problems = []
    for path, text in call_files:
        for lineno, line, in_server, in_client in call_builds(text):
            if in_server and not class_server:
                problems.append("%s:%d server call without class: %s" % (path, lineno, line.strip()))
            if in_client and not class_client:
                problems.append("%s:%d client call without class: %s" % (path, lineno, line.strip()))
    return problems


def nested_begin_guard(text):
    start = text.find("static void Begin(string route)")
    end = text.find("static void Phase(string name)", start)
    return start >= 0 and "if (s_Active)" in text[start:end]


def nested_phase_guard(text):
    start = text.find("static void Phase(string name)")
    end = text.find("static void AddAlloc(int n)", start)
    return start >= 0 and "if (s_NestDepth > 0)" in text[start:end]


def nested_end_guard(text):
    start = text.find("static void End()")
    end = text.find("static void OnProcessDirtyQueue", start)
    return start >= 0 and "if (s_NestDepth > 0)" in text[start:end]


class GraphPerfProbe(unittest.TestCase):
    def test_probe_is_off_by_default(self):
        self.assertTrue(probe_off(_src(DEFINES)))

    def test_r01_cut_all_has_probe(self):
        self.assertTrue(has_r01(_src(RPC)))

    def test_r02_upstream_has_probe(self):
        self.assertTrue(has_r02(_src(GRAPH)))

    def test_r04_cut_in_port_has_probe(self):
        self.assertTrue(has_r04(_src(RPC)))

    def test_r05_indexed_sweeps_counted_in_graph(self):
        self.assertTrue(has_r05(_src(GRAPH)))

    def test_probe_class_logs_clave_valor(self):
        text = _src(PROBE)
        self.assertIn("LFPG_PERF event=1 route=", text)
        self.assertIn("kind=summary", text)
        self.assertIn("nested=", text)
        self.assertIn("LFPG_Util.Info", text)
        self.assertNotIn("Print(", text)
        self.assertNotIn("g_Game.GetTime()", text)
        self.assertIn("GetGame().GetTime()", text)

    def test_workflow_runs_this_file(self):
        self.assertIn("test_graph_perf_probe.py", _src(CHECKS))

    def test_negative_missing_each_route(self):
        self.assertFalse(has_r01(_src(RPC).replace('LFPG_PerfProbe.Begin("R01")', "")))
        self.assertFalse(has_r02(_src(GRAPH).replace('LFPG_PerfProbe.Begin("R02")', "")))
        self.assertFalse(has_r04(_src(RPC).replace('LFPG_PerfProbe.Begin("R04")', "")))
        self.assertFalse(has_r05(_src(GRAPH).replace("LFPG_PerfProbe.CountMapSweep", "")))

    def test_negative_probe_enabled_is_rejected(self):
        mutant = _src(DEFINES).replace(
            "static const bool   LFPG_PERF_PROBE = false;",
            "static const bool   LFPG_PERF_PROBE = true;",
        )
        self.assertFalse(probe_off(mutant))

    def test_m1_class_visible_where_calls_compile(self):
        probe = _src(PROBE)
        calls = []
        for path in sorted(SCRIPTS.rglob("*.c")):
            if path.resolve() == PROBE.resolve():
                continue
            text = path.read_text(encoding="utf-8")
            if "LFPG_PerfProbe." not in text:
                continue
            calls.append((str(path.relative_to(ROOT)).replace("\\", "/"), text))
        self.assertTrue(calls)
        problems = probe_visible_in_all_call_builds(probe, calls)
        self.assertEqual(problems, [])

    def test_m1_negative_server_wrapped_class(self):
        wrapped = "#ifdef SERVER\n" + _src(PROBE) + "\n#endif\n"
        rpc = _src(RPC)
        problems = probe_visible_in_all_call_builds(
            wrapped, [("scripts/5_Mission/LFPG_RPCServerHandlerImpl.c", rpc)]
        )
        self.assertTrue(problems)

    def test_m1_b66dee8_fails_when_commit_present(self):
        probe = subprocess.run(
            ["git", "show", "b66dee8:scripts/3_Game/LFPG_PerfProbe.c"],
            cwd=ROOT, capture_output=True, text=True, encoding="utf-8",
        )
        rpc = subprocess.run(
            ["git", "show", "b66dee8:scripts/5_Mission/LFPG_RPCServerHandlerImpl.c"],
            cwd=ROOT, capture_output=True, text=True, encoding="utf-8",
        )
        if probe.returncode != 0 or rpc.returncode != 0:
            wrapped = "#ifdef SERVER\n" + _src(PROBE) + "\n#endif\n"
            problems = probe_visible_in_all_call_builds(
                wrapped, [("rpc", _src(RPC))]
            )
            self.assertTrue(problems)
            return
        problems = probe_visible_in_all_call_builds(
            probe.stdout, [("scripts/5_Mission/LFPG_RPCServerHandlerImpl.c", rpc.stdout)]
        )
        self.assertTrue(problems)

    def test_m4_nested_begin_guards(self):
        text = _src(PROBE)
        self.assertTrue(nested_begin_guard(text))
        self.assertTrue(nested_phase_guard(text))
        self.assertTrue(nested_end_guard(text))

    def test_m4_negative_missing_nested_guards(self):
        text = _src(PROBE)
        self.assertFalse(nested_begin_guard(text.replace("if (s_Active)", "if (false)")))
        self.assertFalse(nested_phase_guard(text.replace("if (s_NestDepth > 0)", "if (false)")))
        self.assertFalse(nested_end_guard(text.replace("if (s_NestDepth > 0)", "if (false)")))


if __name__ == "__main__":
    unittest.main()
