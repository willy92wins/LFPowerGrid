"""CutWires rebuilds the reverse index only when rescue finds stale wires."""
from pathlib import Path
from types import SimpleNamespace
import copy
import re
import subprocess
import sys
import unittest

from enforce_scalar_slice import method, scalar_function

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
RPC = ROOT / "scripts/5_Mission/LFPG_RPCServerHandlerImpl.c"
WIRE_HELPER = ROOT / "scripts/3_Game/LFPG_WireHelper.c"
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from test_graph_multifeed_split import (
    CONSUMER,
    PASSTHROUGH,
    SOURCE,
    QueueModel,
    helpers,
    net_2a,
)


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


def gate_fn(src):
    block = extract_block(
        src, "if (anyPortMissedIndex || (portCount > 0 && inPortCount == 0))")
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


def base_rpc_text():
    raw = subprocess.check_output(
        ["git", "show", "HEAD:scripts/5_Mission/LFPG_RPCServerHandlerImpl.c"],
        cwd=str(ROOT))
    return raw.decode("utf-8")


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
        self.work = gate_fn(rpc_text())
        self.base = gate_fn(base_rpc_text())

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
        rescue_work = extract_block(src, "protected static int RescueStaleIncomingWires(")
        rescue_base = extract_block(
            base_rpc_text(), "protected static int RescueStaleIncomingWires(")
        self.assertEqual(rescue_work, rescue_base)
        probe_calls = re.findall(r"LFPG_PerfProbe\.\w+", handle)
        probe_base = re.findall(
            r"LFPG_PerfProbe\.\w+",
            extract_block(base_rpc_text(), "static void HandleCutWires("))
        self.assertEqual(probe_calls, probe_base)


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
                        if empty_pid_restricted:
                            continue
                        self.assertEqual(
                            pre_removes, store_removes,
                            (st_present, allow, creator, cut_pid))


def snapshot(q):
    return {
        "alloc": {eid: e.m_AllocatedPower for eid, e in q.edges.items()},
        "in": {nid: n.m_InputPower for nid, n in q.nodes.items()},
        "out": {nid: n.m_OutputPower for nid, n in q.nodes.items()},
        "powered": {nid: n.m_Powered for nid, n in q.nodes.items()},
        "over": {nid: n.m_Overloaded for nid, n in q.nodes.items()},
    }


def clone_model(src):
    q = QueueModel(src.h)
    for nid, n in src.nodes.items():
        q.add_node(
            nid, n.kind, n.m_MaxOutput, n.m_Consumption,
            last_stable=n.m_LastStableOutput, known=n.m_DemandKnown,
            virt=n.m_VirtualGeneration, gate=n.m_GateClosed,
            ratio=n.m_SoftDemandRatio, soft=n.m_SoftDemand, cid=n.m_ComponentId)
        dst = q.nodes[nid]
        dst.m_InputPower = n.m_InputPower
        dst.m_OutputPower = n.m_OutputPower
        dst.m_PrevInputPower = n.m_PrevInputPower
        dst.m_Overloaded = n.m_Overloaded
        dst.m_Powered = n.m_Powered
    for eid, e in src.edges.items():
        q.add_edge(eid, e.m_SourceNodeId, e.m_TargetNodeId,
                   e.m_SourcePort, e.m_TargetPort)
        qe = q.edges[eid]
        qe.m_Flags = e.m_Flags
        qe.m_Demand = e.m_Demand
        qe.m_AllocatedPower = e.m_AllocatedPower
        qe.m_OfferedResidual = e.m_OfferedResidual
    return q


def rebuild_without(src, drop_eids):
    q = QueueModel(src.h)
    for nid, n in src.nodes.items():
        q.add_node(nid, n.kind, n.m_MaxOutput, n.m_Consumption,
                   virt=n.m_VirtualGeneration, gate=n.m_GateClosed)
    for eid, e in src.edges.items():
        if eid in drop_eids:
            continue
        q.add_edge(eid, e.m_SourceNodeId, e.m_TargetNodeId,
                   e.m_SourcePort, e.m_TargetPort)
    q.run(list(q.nodes.keys()), steps=200)
    return q


def almost(a, b, places=2):
    if set(a) != set(b):
        return False
    for k in a:
        if round(a[k] - b[k], places) != 0:
            return False
    return True


class T4Recovery(unittest.TestCase):
    def setUp(self):
        self.h = helpers()

    def _nets(self):
        nets = [net_2a(self.h)]
        q = QueueModel(self.h)
        q.add_node("s50", SOURCE, 50)
        q.add_node("s20", SOURCE, 20)
        q.add_node("sp", PASSTHROUGH, 200)
        q.add_node("c", PASSTHROUGH, 500)
        q.add_node("l1", CONSUMER, 0, 10)
        q.add_node("l2", CONSUMER, 0, 50)
        q.add_edge("e_s50", "s50", "sp")
        q.add_edge("e_l1", "sp", "l1")
        q.add_edge("e_sp_c", "sp", "c")
        q.add_edge("e_s20", "s20", "c")
        q.add_edge("e_out", "c", "l2")
        q.run(["s50", "s20", "sp", "c", "l1", "l2"])
        nets.append(q)
        return nets

    def test_cut_all_outgoing_then_restore(self):
        for q0 in self._nets():
            original = snapshot(q0)
            for nid, outs in list(q0.outgoing.items()):
                if not outs:
                    continue
                drop = [e.id for e in outs]
                live = clone_model(q0)
                seeds = [nid]
                for e in list(live.outgoing[nid]):
                    seeds.append(e.m_TargetNodeId)
                    live.remove_edge(e.id)
                live.run(seeds, steps=200)
                rebuilt = rebuild_without(q0, set(drop))
                s_live = snapshot(live)
                s_reb = snapshot(rebuilt)
                self.assertTrue(almost(s_live["alloc"], s_reb["alloc"]), (nid, s_live["alloc"], s_reb["alloc"]))
                self.assertEqual(s_live["powered"], s_reb["powered"])
                restored = clone_model(live)
                for eid in drop:
                    e0 = q0.edges[eid]
                    restored.add_edge(
                        eid, e0.m_SourceNodeId, e0.m_TargetNodeId,
                        e0.m_SourcePort, e0.m_TargetPort)
                restored.run(list(restored.nodes.keys()), steps=200)
                s_rest = snapshot(restored)
                full = rebuild_without(q0, set())
                s_full = snapshot(full)
                self.assertTrue(almost(s_rest["alloc"], s_full["alloc"]))
                self.assertTrue(almost(s_rest["alloc"], original["alloc"]))


if __name__ == "__main__":
    unittest.main()
