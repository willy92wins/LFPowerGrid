"""CutWires rebuilds the reverse index only when rescue finds stale wires."""
from pathlib import Path
from types import SimpleNamespace
import hashlib
import re
import sys
import unittest

from enforce_scalar_slice import method, scalar_function

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
RPC = ROOT / "scripts/5_Mission/LFPG_RPCServerHandlerImpl.c"
WIRE_HELPER = ROOT / "scripts/3_Game/LFPG_WireHelper.c"
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from test_graph_multifeed_split import GRAPHS, _assert_match, _recon, helpers

# Gate before the fix (4e897f2). T1 puts it back into the current block as the
# base, so no test reads git history (CI checks out a single commit).
OLD_GATE = "if (rescued > 0 || (changed && !anyInRemovedByIndex))"

# HandleCutWires probes at 4e897f2; the R-01 in-game measurement reads them.
PROBE_CALLS = [
    'LFPG_PerfProbe.Begin("R01");',
    'LFPG_PerfProbe.AddAlloc(2);',
    'LFPG_PerfProbe.Phase("index");',
    'LFPG_PerfProbe.Phase("rescue");',
    'LFPG_PerfProbe.Phase("rebuild");',
    'LFPG_PerfProbe.End();',
]

# RescueStaleIncomingWires at 4e897f2 (LF line endings). The gate trusts this
# scan; if it changes on purpose, re-check #78 R-01 and update the hash.
RESCUE_SHA256 = "e0aa43336e8196c2b41e1984065e506f8a087c2a058bb9c5b89b0302c004499e"


def rpc_text(path=None):
    return (path or RPC).read_text(encoding="utf-8")


def extract_block(src, start_token, closer="}"):
    idx = src.find(start_token)
    if idx < 0:
        raise ValueError("block start missing: " + start_token)
    brace = src.find("{", idx)
    depth, pos = 1, brace + 1
    while depth:
        if src[pos] == "{":
            depth = depth + 1
        elif src[pos] == "}":
            depth = depth - 1
        pos = pos + 1
    return src[idx:pos]


def gate_block(src):
    return extract_block(
        src, "if (anyPortMissedIndex || (portCount > 0 && inPortCount == 0))")


def old_gate_block(src):
    """Current block with the 4e897f2 rebuild condition put back."""
    block, count = re.subn(
        r"if \(rescued > 0\)(\s*\{)", lambda m: OLD_GATE + m.group(1),
        gate_block(src))
    if count != 1:
        raise ValueError("rebuild gate missing")
    return block


def gate_fn(block):
    block = re.sub(r"LFPG_Util\.Warn\([^;]*\);", "WarnStub();", block)
    block = block.replace('""', "EMPTY")
    wrapped = (
        "void Gate(bool anyPortMissedIndex, int portCount, int inPortCount, "
        "bool changed, bool anyInRemovedByIndex, EntityAI obj, "
        "string deviceId, string cutPid, bool allowOthers)\n{\n"
        + block
        + "\n}\n"
    )
    stubs = {
        "RescueStaleIncomingWires": lambda *a: stubs["_rescued"],
        "WarnStub": lambda: None,
        "EMPTY": "",
        "LFPG_NetworkManager": SimpleNamespace(
            Get=lambda: SimpleNamespace(
                RebuildReverseIdx=lambda: stubs["_rebuilds"].append(1))),
        "_rebuilds": [],
        "_rescued": 0,
    }
    compiled = scalar_function(method(wrapped, "Gate"), [
        "anyPortMissedIndex", "portCount", "inPortCount", "changed",
        "anyInRemovedByIndex", "obj", "deviceId", "cutPid", "allowOthers"])
    env = {"__builtins__": {}, **stubs}
    exec(compile(compiled, "<source-slice:CutWiresGate>", "exec"), env)
    run = env["run"]

    def call(rescued, **kwargs):
        stubs["_rebuilds"].clear()
        stubs["_rescued"] = rescued
        kwargs.setdefault("obj", None)
        kwargs.setdefault("deviceId", "dev")
        kwargs.setdefault("cutPid", "pid")
        kwargs.setdefault("allowOthers", True)
        run(**kwargs)
        return len(stubs["_rebuilds"])

    return call


def skip_cond_fn(src):
    """Pre-scan skip at HandleCutWires: skip other players' wires."""
    token = (
        "if (st && !st.AllowCutOthersWires && pwd.m_CreatorId != \"\" "
        "&& pwd.m_CreatorId != cutPid)"
    )
    if token not in src:
        raise ValueError("pre-scan skip missing")
    wrapped = (
        "void Skip(bool stPresent, bool allowCutOthers, string creatorId, "
        "string cutPid)\n{\n"
        "bool skipped = false;\n"
        "if (stPresent && !allowCutOthers && creatorId != EMPTY "
        "&& creatorId != cutPid)\n"
        "skipped = true;\n"
        "return skipped;\n"
        "}\n"
    )
    compiled = scalar_function(method(wrapped, "Skip"), [
        "stPresent", "allowCutOthers", "creatorId", "cutPid"])
    env = {"__builtins__": {}, "EMPTY": ""}
    exec(compile(compiled, "<source-slice:prescan-skip>", "exec"), env)
    return env["run"]


def clear_for_creator_removes(creator_on_wire, cut_pid, allow_others, st_present):
    """Same set as ClearForCreator / ClearAll given the HandleCutWires branch."""
    if not st_present or allow_others:
        return True
    if cut_pid == "":
        return False
    return creator_on_wire == "" or creator_on_wire == cut_pid


class T1Decision(unittest.TestCase):
    def setUp(self):
        src = rpc_text()
        self.work = gate_fn(gate_block(src))
        self.base = gate_fn(old_gate_block(src))

    def test_t1_no_in_ports_rescued_zero_no_rebuild(self):
        kwargs = dict(
            anyPortMissedIndex=False, portCount=1, inPortCount=0,
            changed=True, anyInRemovedByIndex=False)
        self.assertEqual(self.work(0, **kwargs), 0)
        self.assertNotEqual(self.base(0, **kwargs), 0)

    def test_t1_missed_in_index_rescued_zero_no_rebuild(self):
        kwargs = dict(
            anyPortMissedIndex=True, portCount=4, inPortCount=1,
            changed=True, anyInRemovedByIndex=False)
        self.assertEqual(self.work(0, **kwargs), 0)
        self.assertNotEqual(self.base(0, **kwargs), 0)

    def test_t1_rescued_one_no_in_rebuilds(self):
        kwargs = dict(
            anyPortMissedIndex=False, portCount=1, inPortCount=0,
            changed=False, anyInRemovedByIndex=False)
        self.assertEqual(self.work(1, **kwargs), 1)
        self.assertEqual(self.base(1, **kwargs), 1)

    def test_t1_rescued_one_missed_in_rebuilds(self):
        kwargs = dict(
            anyPortMissedIndex=True, portCount=4, inPortCount=1,
            changed=False, anyInRemovedByIndex=False)
        self.assertEqual(self.work(1, **kwargs), 1)
        self.assertEqual(self.base(1, **kwargs), 1)

    def test_t1_unchanged_rescued_zero_no_rebuild(self):
        kwargs = dict(
            anyPortMissedIndex=False, portCount=1, inPortCount=0,
            changed=False, anyInRemovedByIndex=False)
        self.assertEqual(self.work(0, **kwargs), 0)
        self.assertEqual(self.base(0, **kwargs), 0)

    def test_t1_in_hit_skips_rescue_block(self):
        kwargs = dict(
            anyPortMissedIndex=False, portCount=4, inPortCount=1,
            changed=True, anyInRemovedByIndex=True)
        self.assertEqual(self.work(0, **kwargs), 0)
        self.assertEqual(self.base(0, **kwargs), 0)
        self.assertEqual(self.work(1, **kwargs), 0)
        self.assertEqual(self.base(1, **kwargs), 0)


class T2Static(unittest.TestCase):
    def test_single_rebuild_gated_by_rescued(self):
        src = rpc_text()
        handle = extract_block(src, "static void HandleCutWires(")
        self.assertEqual(handle.count("RebuildReverseIdx()"), 1)
        self.assertIn("if (rescued > 0)", handle)
        self.assertNotIn("changed && !anyInRemovedByIndex", handle)
        cut_port = extract_block(src, "static void HandleCutPort(")
        self.assertIn("if (rescued > 0)", cut_port)
        rescue = extract_block(src, "protected static int RescueStaleIncomingWires(")
        self.assertEqual(
            hashlib.sha256(rescue.encode("utf-8")).hexdigest(), RESCUE_SHA256)
        self.assertEqual(
            re.findall(r"LFPG_PerfProbe\.\w+\([^;]*\);", handle), PROBE_CALLS)


class T3Mirror(unittest.TestCase):
    def test_prescan_skip_matches_clear_for_creator(self):
        src = rpc_text()
        helper = WIRE_HELPER.read_text(encoding="utf-8")
        self.assertIn("if (wd.m_CreatorId == \"\" || wd.m_CreatorId == creatorId)", helper)
        token = (
            "if (st && !st.AllowCutOthersWires && pwd.m_CreatorId != \"\" "
            "&& pwd.m_CreatorId != cutPid)"
        )
        self.assertIn(token, src)
        skip = skip_cond_fn(src)
        creators = ["", "pid", "other"]
        cut_pids = ["pid", ""]
        for st_present in (True, False):
            for allow in (True, False):
                for creator in creators:
                    for cut_pid in cut_pids:
                        skipped = skip(st_present, allow, creator, cut_pid)
                        pre_removes = not skipped
                        store_removes = clear_for_creator_removes(
                            creator, cut_pid, allow, st_present)
                        empty_pid_restricted = (
                            st_present and not allow and cut_pid == "")
                        # Known, pre-existing mismatch (INFORME, hallazgos):
                        # ClearForCreator returns early on an empty creatorId
                        # but the pre-scan still drops ownerless wires from the
                        # index. Needs an empty PlainId; R-01 does not touch it.
                        if empty_pid_restricted:
                            continue
                        self.assertEqual(
                            pre_removes, store_removes,
                            (st_present, allow, creator, cut_pid))


def powered(q):
    return {nid: n.m_Powered for nid, n in q.nodes.items()}


class T4Recovery(unittest.TestCase):
    """Cutting every wire of a device without inputs (a generator) settles on
    the incremental path to the same state as a rebuild without those wires,
    and putting them back one by one returns to the original rebuild."""

    def setUp(self):
        self.h = helpers()

    def test_cut_all_outgoing_then_restore(self):
        ncases = 0
        for name, fn in GRAPHS:
            nodes, edges = fn()
            full = _recon(self.h, nodes, edges)
            for node in nodes:
                nid = node["id"]
                outs = [e for e in edges if e[1] == nid]
                has_inputs = any(e[2] == nid for e in edges)
                if not outs or has_inputs:
                    continue
                msg = name + " " + nid
                q = _recon(self.h, nodes, edges)
                seeds = [nid]
                for ed in outs:
                    q.remove_edge(ed[0])
                    seeds.append(ed[2])
                q.run(seeds)
                cut = _recon(self.h, nodes, [e for e in edges if e[1] != nid])
                _assert_match(self, q, cut, msg + " cut")
                self.assertEqual(powered(q), powered(cut), msg + " cut powered")
                for ed in outs:
                    q.add_edge(*ed)
                    q.run([ed[1], ed[2]])
                _assert_match(self, q, full, msg + " restore")
                self.assertEqual(powered(q), powered(full), msg + " restore powered")
                ncases = ncases + 1
        self.assertGreaterEqual(ncases, 13)


if __name__ == "__main__":
    unittest.main()
