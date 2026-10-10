"""Offline checks for R-01/R-02/R-04/R-05 measurement probes (issue #78).

Does not execute Enforce. Positives require the four call sites and a default-off
flag. Negatives fail when any site is stripped or the flag is true (including
the tree at HEAD before this change).
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


def _src(path):
    return path.read_text(encoding="utf-8")


def _git_show(rel):
    result = subprocess.run(
        ["git", "show", "HEAD:" + rel.replace("\\", "/")],
        cwd=ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    return result.returncode, result.stdout


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
        self.assertIn("LFPG_Util.Info", text)
        self.assertNotIn("Print(", text)

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

    def test_negative_head_base_lacks_probes(self):
        code, rpc = _git_show("scripts/5_Mission/LFPG_RPCServerHandlerImpl.c")
        self.assertEqual(code, 0)
        self.assertFalse(has_r01(rpc))
        self.assertFalse(has_r04(rpc))
        code, graph = _git_show("scripts/5_Mission/LFPG_ElecGraphImpl.c")
        self.assertEqual(code, 0)
        self.assertFalse(has_r02(graph))
        self.assertFalse(has_r05(graph))
        code, defines = _git_show("scripts/3_Game/LFPG_Defines.c")
        self.assertEqual(code, 0)
        self.assertFalse(probe_off(defines))


if __name__ == "__main__":
    unittest.main()
