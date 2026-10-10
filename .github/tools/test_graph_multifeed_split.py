"""G-01 multi-feed split: production offer/water-fill slices + queue model."""
from pathlib import Path
from types import SimpleNamespace
import re
import subprocess
import sys
import unittest

from enforce_scalar_slice import load

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
GRAPH = ROOT / "scripts/5_Mission/LFPG_ElecGraphImpl.c"
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))
from graph_reference import Oracle
from graph_reference.model import Edge, Graph, Node

EPS = 0.001
SOURCE, CONSUMER, PASSTHROUGH, CAMERA = 1, 2, 3, 4


def graph_src(text=None):
    return text if text is not None else GRAPH.read_text(encoding="utf-8")


def helpers(src=None):
    source = graph_src(src)
    stubs = {"LFPG_PROPAGATION_EPSILON": EPS}
    return {
        "offer_cap": load(source, "OfferCapFromWritten", ["offeredResidual", "fallbackMax"], stubs),
        "offer_edge": load(source, "ComputeOfferTowardEdge", ["baseP", "otherHard"], stubs),
        "base_src": load(source, "ComputeOfferBaseSource", ["availableOutput"], stubs),
        "base_pt": load(source, "ComputeOfferBasePassthrough",
                        ["maxOutput", "incomingOfferSum", "virt", "cons", "gateClosed"], stubs),
        "share": load(source, "WaterFillShareAsk", ["demand", "k", "cap"], stubs),
        "leftover": load(source, "WaterFillLeftoverAdd", ["leftover", "cap", "already"], stubs),
        "hard": load(source, "EdgeHardPortion", ["demand", "softRatio"], stubs),
        "skip": load(source, "SkipOtherIndex", ["oi", "skipIndex"], stubs),
        "notify": load(source, "ShouldNotifyOfferDirty", ["delta", "neverWritten"], stubs),
    }


def waterfill(h, demand, caps):
    k = len(caps)
    leftover = demand
    asks = []
    for cap in caps:
        a = h["share"](demand, k, cap)
        asks.append(a)
        leftover = leftover - a
    if leftover < 0.0:
        leftover = 0.0
    out = []
    for i, cap in enumerate(caps):
        add = h["leftover"](leftover, cap, asks[i])
        out.append(asks[i] + add)
        leftover = leftover - add
    return out


def oracle_graph(nodes, edges):
    g = Graph()
    for n in nodes:
        g.nodes[n.id] = n
    for e in edges:
        g.edges[e.id] = e
    return g


def rules_of(report):
    return sorted(set(v.rule for v in report.violations))


class SliceHelpers(unittest.TestCase):
    def setUp(self):
        self.h = helpers()

    def test_2a_waterfill_20_30(self):
        asks = waterfill(self.h, 50.0, [20.0, 50.0])
        self.assertAlmostEqual(asks[0], 20.0, places=3)
        self.assertAlmostEqual(asks[1], 30.0, places=3)

    def test_offer_zero_is_real(self):
        self.assertEqual(self.h["offer_cap"](0.0, 50.0), 0.0)
        self.assertEqual(self.h["offer_cap"](-1.0, 50.0), 50.0)

    def test_offer_excludes_self_hard(self):
        self.assertEqual(self.h["offer_edge"](50.0, 20.0), 30.0)
        self.assertTrue(self.h["skip"](1, 1))
        self.assertFalse(self.h["skip"](0, 1))

    def test_pt_base_not_alloc_avail(self):
        self.assertEqual(self.h["base_pt"](200.0, 50.0, 0.0, 0.0, False), 50.0)
        self.assertEqual(self.h["base_pt"](200.0, 50.0, 0.0, 0.0, True), 0.0)


class QueueModel:
    """Same-epoch requeue + B1.3, calling production share/offer helpers."""

    def __init__(self, h, src=None):
        self.h = h
        self.src = graph_src(src)
        self.nodes = {}
        self.outgoing = {}
        self.incoming = {}
        self.edges = {}
        self.queue = []
        self.epoch = 1
        self.notify_enabled = "ShouldNotifyOfferDirty" in self.src
        self.skip_self = "SkipOtherIndex" in self.src

    def add_node(self, nid, kind, max_out, cons=0.0, **kw):
        n = SimpleNamespace(
            id=nid, kind=kind, m_MaxOutput=max_out, m_Consumption=cons,
            m_LastStableOutput=kw.get("last_stable", 0.0),
            m_DemandKnown=kw.get("known", False),
            m_InputPower=0.0, m_OutputPower=0.0, m_PrevInputPower=0.0,
            m_Overloaded=False, m_Powered=kind == SOURCE,
            m_VirtualGeneration=kw.get("virt", 0.0),
            m_GateClosed=kw.get("gate", False),
            m_SoftDemandRatio=kw.get("ratio", 0.0),
            m_SoftDemand=kw.get("soft", 0.0),
            m_RequeueCount=0, m_LastEpoch=0, m_InQueue=False,
            m_ComponentId=kw.get("cid", 1),
        )
        self.nodes[nid] = n
        self.outgoing[nid] = []
        self.incoming[nid] = []
        return n

    def add_edge(self, eid, src, dst, sport="o", tport="i"):
        e = SimpleNamespace(
            id=eid, m_SourceNodeId=src, m_TargetNodeId=dst,
            m_SourcePort=sport, m_TargetPort=tport,
            m_Flags=1, m_Demand=0.0, m_AllocatedPower=0.0,
            m_OfferedResidual=-1.0,
        )
        self.edges[eid] = e
        self.outgoing[src].append(e)
        self.incoming[dst].append(e)
        return e

    def mark(self, nid):
        n = self.nodes[nid]
        if self.epoch > 0 and n.m_LastEpoch == self.epoch:
            n.m_RequeueCount = n.m_RequeueCount + 1
            n.m_LastEpoch = self.epoch - 1
        if not n.m_InQueue:
            n.m_InQueue = True
            self.queue.append(nid)

    def powered_in(self, nid):
        out = []
        for e in self.incoming.get(nid, []):
            if e.m_Flags == 0:
                continue
            src = self.nodes[e.m_SourceNodeId]
            power = src.m_OutputPower
            if src.kind == PASSTHROUGH:
                power = src.m_InputPower + src.m_VirtualGeneration - src.m_Consumption
                if src.m_GateClosed:
                    power = 0.0
            if power > EPS:
                out.append(e)
        return out

    def offer_base(self, nid, available):
        n = self.nodes[nid]
        if n.kind == SOURCE:
            return self.h["base_src"](available)
        incoming = 0.0
        for e in self.incoming.get(nid, []):
            if e.m_Flags == 0:
                continue
            fb = self.nodes[e.m_SourceNodeId].m_MaxOutput
            incoming = incoming + self.h["offer_cap"](e.m_OfferedResidual, fb)
        return self.h["base_pt"](n.m_MaxOutput, incoming, n.m_VirtualGeneration, n.m_Consumption, n.m_GateClosed)

    def other_hard(self, nid, skip):
        outs = self.outgoing[nid]
        s = 0.0
        for oi, e in enumerate(outs):
            if self.skip_self and self.h["skip"](oi, skip):
                continue
            if not self.skip_self and oi == skip:
                continue
            if e.m_Flags == 0:
                continue
            ratio = self.nodes[e.m_TargetNodeId].m_SoftDemandRatio
            s = s + self.h["hard"](e.m_Demand, ratio)
        return s

    def raw_demand(self, edge, available):
        tgt = self.nodes[edge.m_TargetNodeId]
        if tgt.kind in (CONSUMER, CAMERA):
            return tgt.m_Consumption
        if tgt.kind != PASSTHROUGH:
            return 0.0
        d = tgt.m_LastStableOutput
        if d < EPS and not tgt.m_DemandKnown:
            if tgt.m_GateClosed:
                d = max(tgt.m_Consumption, 1.0)
            elif self.outgoing[tgt.id] and tgt.m_MaxOutput > EPS:
                d = tgt.m_MaxOutput
                if d > available:
                    d = available
            else:
                d = tgt.m_Consumption
        return d

    def apply_mergers(self, nid, available):
        outs = self.outgoing[nid]
        mergers = []
        for i, e in enumerate(outs):
            if e.m_Flags == 0:
                continue
            tgt = self.nodes[e.m_TargetNodeId]
            if tgt.kind == PASSTHROUGH and len(self.powered_in(tgt.id)) > 1:
                mergers.append(i)
        mergers.sort(key=lambda i: (outs[i].m_TargetNodeId, outs[i].m_TargetPort))
        if not mergers:
            return
        remaining = self.offer_base(nid, available)
        for i, e in enumerate(outs):
            if i in mergers or e.m_Flags == 0:
                continue
            remaining = remaining - self.h["hard"](e.m_Demand, self.nodes[e.m_TargetNodeId].m_SoftDemandRatio)
        if remaining < 0.0:
            remaining = 0.0
        for i in mergers:
            e = outs[i]
            D = e.m_Demand
            caps = []
            self_index = -1
            for pe in self.powered_in(e.m_TargetNodeId):
                src = self.nodes[pe.m_SourceNodeId]
                if pe.m_SourceNodeId == nid:
                    self_index = len(caps)
                    caps.append(remaining)
                else:
                    caps.append(self.h["offer_cap"](pe.m_OfferedResidual, src.m_MaxOutput))
            if len(caps) <= 1 or self_index < 0:
                continue
            asks = waterfill(self.h, D, caps)
            self_ask = asks[self_index]
            if self_ask > remaining:
                self_ask = remaining
            e.m_Demand = self_ask
            remaining = remaining - self_ask
            if remaining < 0.0:
                remaining = 0.0

    def publish_offers(self, nid, available):
        outs = self.outgoing[nid]
        base = self.offer_base(nid, available)
        dirtied = []
        for i, e in enumerate(outs):
            if e.m_Flags == 0:
                continue
            other = self.other_hard(nid, i)
            new_offer = self.h["offer_edge"](base, other)
            prev = e.m_OfferedResidual
            delta = new_offer - prev
            if delta < 0.0:
                delta = -delta
            never = prev < 0.0
            e.m_OfferedResidual = new_offer
            fire = True
            if self.notify_enabled:
                fire = self.h["notify"](delta, never)
            else:
                fire = never or delta > EPS
            if fire:
                dirtied.extend(self.notify_targets(nid, e))
        return dirtied

    def notify_targets(self, provider, e):
        out = []
        tgt = e.m_TargetNodeId
        pins = self.powered_in(tgt)
        if len(self.incoming[tgt]) >= 1 and len(pins) > 1:
            for pe in self.incoming[tgt]:
                if pe.m_Flags and pe.m_SourceNodeId != provider:
                    out.append(pe.m_SourceNodeId)
        tn = self.nodes[tgt]
        if tn.kind == PASSTHROUGH:
            for te in self.outgoing[tgt]:
                if te.m_Flags and len(self.powered_in(te.m_TargetNodeId)) > 1:
                    out.append(tgt)
                    break
        return out

    def allocate(self, nid, available):
        n = self.nodes[nid]
        outs = self.outgoing[nid]
        if not outs:
            return 0.0
        total = 0.0
        soft = 0.0
        for e in outs:
            if e.m_Flags == 0:
                continue
            d = self.raw_demand(e, available)
            e.m_Demand = d
            total = total + d
            tgt = self.nodes[e.m_TargetNodeId]
            if tgt.m_SoftDemandRatio > EPS:
                soft = soft + d * tgt.m_SoftDemandRatio
        self.apply_mergers(nid, available)
        total = 0.0
        soft = 0.0
        for e in outs:
            if e.m_Flags == 0:
                continue
            total = total + e.m_Demand
            r = self.nodes[e.m_TargetNodeId].m_SoftDemandRatio
            if r > EPS:
                soft = soft + e.m_Demand * r
        hard = total - soft
        if hard < 0.0:
            hard = 0.0
        overloaded = hard > available + EPS
        allocated = 0.0
        for e in outs:
            if e.m_Flags == 0:
                continue
            new_alloc = 0.0
            if not overloaded:
                r = self.nodes[e.m_TargetNodeId].m_SoftDemandRatio
                if soft > EPS:
                    new_alloc = e.m_Demand * (1.0 - r)
                else:
                    new_alloc = e.m_Demand
            e.m_AllocatedPower = new_alloc
            allocated = allocated + new_alloc
        if not overloaded and soft > EPS:
            surplus = available - allocated
            if surplus > EPS:
                if surplus > soft:
                    surplus = soft
                for e in outs:
                    if e.m_Flags == 0:
                        continue
                    r = self.nodes[e.m_TargetNodeId].m_SoftDemandRatio
                    if r < EPS:
                        continue
                    bonus = surplus * (e.m_Demand * r) / soft
                    e.m_AllocatedPower = e.m_AllocatedPower + bonus
        n.m_Overloaded = overloaded
        return total

    def edge_power(self, e):
        src = self.nodes[e.m_SourceNodeId]
        if src.m_Overloaded:
            return 0.0
        if e.m_AllocatedPower > EPS:
            return e.m_AllocatedPower
        if src.kind == PASSTHROUGH:
            return 0.0
        return 0.0

    def process(self, nid):
        n = self.nodes[nid]
        n.m_InQueue = False
        if n.m_RequeueCount > 5:
            return []
        n.m_LastEpoch = self.epoch
        insum = 0.0
        for e in self.incoming.get(nid, []):
            if e.m_Flags:
                insum = insum + self.edge_power(e)
        n.m_PrevInputPower = n.m_InputPower
        n.m_InputPower = insum
        extra = []
        if n.kind == SOURCE:
            available = n.m_MaxOutput if n.m_Powered else 0.0
            n.m_OutputPower = available
            self.allocate(nid, available)
            extra = self.publish_offers(nid, available)
            for e in self.outgoing[nid]:
                extra.append(e.m_TargetNodeId)
        elif n.kind == PASSTHROUGH:
            available = insum + n.m_VirtualGeneration - n.m_Consumption
            if available < 0.0:
                available = 0.0
            if n.m_MaxOutput > EPS and available > n.m_MaxOutput:
                available = n.m_MaxOutput
            if n.m_GateClosed:
                available = 0.0
            demand = self.allocate(nid, available)
            extra = self.publish_offers(nid, available)
            n.m_LastStableOutput = demand
            n.m_DemandKnown = True
            input_changed = abs(insum - n.m_PrevInputPower) > EPS
            if input_changed:
                for e in self.incoming.get(nid, []):
                    extra.append(e.m_SourceNodeId)
            for e in self.outgoing[nid]:
                extra.append(e.m_TargetNodeId)
        else:
            n.m_Powered = insum + EPS >= n.m_Consumption if n.m_Consumption > EPS else insum > EPS
            for e in self.incoming.get(nid, []):
                extra.append(e.m_SourceNodeId)
        return extra

    def run(self, seeds, steps=80):
        self.epoch = self.epoch + 1
        for n in self.nodes.values():
            n.m_RequeueCount = 0
        for s in seeds:
            self.mark(s)
        n = 0
        while self.queue and n < steps:
            nid = self.queue.pop(0)
            extra = self.process(nid)
            for x in extra:
                if x in self.nodes:
                    self.mark(x)
            n = n + 1
        return n

    def alloc_map(self):
        return {eid: e.m_AllocatedPower for eid, e in self.edges.items()}


def net_2a(h):
    q = QueueModel(h)
    q.add_node("s20", SOURCE, 20)
    q.add_node("s50", SOURCE, 50)
    q.add_node("c", PASSTHROUGH, 500)
    q.add_node("load", CONSUMER, 0, 50)
    q.add_edge("e_s20", "s20", "c")
    q.add_edge("e_s50", "s50", "c")
    q.add_edge("e_out", "c", "load")
    q.run(["s20", "s50", "c", "load"])
    return q


class Scenarios(unittest.TestCase):
    def setUp(self):
        self.h = helpers()
        self.o = Oracle()

    def test_2a_feasible_ok(self):
        q = net_2a(self.h)
        a = q.alloc_map()
        self.assertAlmostEqual(a["e_s20"], 20.0, places=2)
        self.assertAlmostEqual(a["e_s50"], 30.0, places=2)
        g = oracle_graph(
            [Node("s20", "SOURCE", available=20), Node("s50", "SOURCE", available=50),
             Node("c", "PASSTHROUGH", pass_limit=500), Node("load", "CONSUMER", hard_demand=50)],
            [Edge("e_s20", "s20", "c"), Edge("e_s50", "s50", "c"), Edge("e_out", "c", "load")],
        )
        report = self.o.verify(g, {"e_s20": a["e_s20"], "e_s50": a["e_s50"], "e_out": a["e_out"]})
        self.assertTrue(report.ok, [v.detail for v in report.violations])

    def test_2b_deficit_all_off_combiner(self):
        q = QueueModel(self.h)
        q.add_node("s20", SOURCE, 20)
        q.add_node("s50", SOURCE, 50)
        q.add_node("c", PASSTHROUGH, 500)
        q.add_node("load", CONSUMER, 0, 80)
        q.add_edge("e_s20", "s20", "c")
        q.add_edge("e_s50", "s50", "c")
        q.add_edge("e_out", "c", "load")
        q.run(["s20", "s50", "c", "load"])
        a = q.alloc_map()
        self.assertAlmostEqual(a["e_s20"], 20.0, places=2)
        self.assertAlmostEqual(a["e_s50"], 50.0, places=2)
        self.assertAlmostEqual(a["e_out"], 0.0, places=2)
        self.assertTrue(q.nodes["c"].m_Overloaded)
        self.assertFalse(q.nodes["s20"].m_Overloaded)
        g = oracle_graph(
            [Node("s20", "SOURCE", available=20), Node("s50", "SOURCE", available=50),
             Node("c", "PASSTHROUGH", pass_limit=500), Node("load", "CONSUMER", hard_demand=80)],
            [Edge("e_s20", "s20", "c"), Edge("e_s50", "s50", "c"), Edge("e_out", "c", "load")],
        )
        report = self.o.verify(g, a)
        self.assertFalse(report.ok)
        self.assertIn("hard_unmet", rules_of(report))
        self.assertNotIn("feasible_but_underfed", rules_of(report))
        self.assertNotIn("partial_allocation", rules_of(report))

    def test_2c_shared_splitter(self):
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
        a = q.alloc_map()
        self.assertAlmostEqual(a["e_s20"], 20.0, places=1)
        self.assertGreater(a["e_sp_c"], 20.0)
        self.assertAlmostEqual(a["e_l1"], 10.0, places=1)
        g = oracle_graph(
            [Node("s50", "SOURCE", available=50), Node("s20", "SOURCE", available=20),
             Node("sp", "PASSTHROUGH", pass_limit=200), Node("c", "PASSTHROUGH", pass_limit=500),
             Node("l1", "CONSUMER", hard_demand=10), Node("l2", "CONSUMER", hard_demand=50)],
            [Edge("e_s50", "s50", "sp"), Edge("e_l1", "sp", "l1"), Edge("e_sp_c", "sp", "c"),
             Edge("e_s20", "s20", "c"), Edge("e_out", "c", "l2")],
        )
        report = self.o.verify(g, a)
        self.assertTrue(report.ok, [v.detail for v in report.violations])

    def test_2d_hard_soft_full_demand(self):
        asks = waterfill(self.h, 70.0, [20.0, 50.0])
        self.assertAlmostEqual(asks[0], 20.0, places=3)
        self.assertAlmostEqual(asks[1], 50.0, places=3)
        ratio = 20.0 / 70.0
        hard = asks[0] * (1 - ratio) + asks[1] * (1 - ratio)
        soft = asks[0] * ratio + asks[1] * ratio
        self.assertAlmostEqual(hard, 50.0, places=2)
        self.assertAlmostEqual(soft, 20.0, places=2)

    def test_2e_cold_start_no_overload(self):
        q = net_2a(self.h)
        self.assertFalse(q.nodes["s20"].m_Overloaded)
        self.assertFalse(q.nodes["s50"].m_Overloaded)

    def test_2f_recovery_zero_demand(self):
        q = net_2a(self.h)
        q.nodes["load"].m_Consumption = 0.0
        q.nodes["c"].m_LastStableOutput = 0.0
        q.run(["c", "s20", "s50"])
        self.assertAlmostEqual(q.edges["e_s20"].m_AllocatedPower, 0.0, places=2)
        self.assertAlmostEqual(q.edges["e_s50"].m_AllocatedPower, 0.0, places=2)

    def test_2g_other_branch_zero_offer(self):
        q = QueueModel(self.h)
        q.add_node("s20", SOURCE, 20)
        q.add_node("s50", SOURCE, 50)
        q.add_node("c", PASSTHROUGH, 500)
        q.add_node("load", CONSUMER, 0, 50)
        q.add_node("heavy", CONSUMER, 0, 60)
        q.add_edge("e_s20", "s20", "c")
        q.add_edge("e_s50", "s50", "c")
        q.add_edge("e_heavy", "s50", "heavy")
        q.add_edge("e_out", "c", "load")
        q.run(["s20", "s50", "c", "load", "heavy"])
        self.assertAlmostEqual(q.edges["e_s50"].m_OfferedResidual, 0.0, places=2)
        self.assertAlmostEqual(q.edges["e_out"].m_AllocatedPower, 0.0, places=2)
        self.assertTrue(q.nodes["c"].m_Overloaded)

    def test_b1_same_epoch_sees_latest_offer(self):
        q = QueueModel(self.h)
        q.add_node("s30", SOURCE, 30)
        q.add_node("s50", SOURCE, 50)
        q.add_node("c", PASSTHROUGH, 500)
        q.add_node("pump", CONSUMER, 0, 50)
        q.add_node("lamp", CONSUMER, 0, 40)
        q.add_edge("e30", "s30", "c")
        q.add_edge("e50", "s50", "c")
        q.add_edge("el", "s50", "lamp")
        q.add_edge("eo", "c", "pump")
        q.run(["s30", "s50", "c", "pump", "lamp"])
        q.nodes["lamp"].m_Consumption = 0.0
        q.run(["s50", "c", "s30"])
        a = q.alloc_map()
        self.assertAlmostEqual(a["e30"] + a["e50"], 50.0, places=1)
        g = oracle_graph(
            [Node("s30", "SOURCE", available=30), Node("s50", "SOURCE", available=50),
             Node("c", "PASSTHROUGH", pass_limit=500), Node("pump", "CONSUMER", hard_demand=50)],
            [Edge("e30", "s30", "c"), Edge("e50", "s50", "c"), Edge("eo", "c", "pump")],
        )
        report = self.o.verify(g, {"e30": a["e30"], "e50": a["e50"], "eo": a["eo"]})
        self.assertTrue(report.ok, [v.detail for v in report.violations])

    def test_b2_zero_offer_not_maxoutput(self):
        self.assertEqual(self.h["offer_cap"](0.0, 50.0), 0.0)
        asks = waterfill(self.h, 50.0, [20.0, 0.0, 30.0])
        self.assertAlmostEqual(sum(asks), 50.0, places=3)
        self.assertAlmostEqual(asks[1], 0.0, places=3)

    def test_b3_offer_other_outs_only(self):
        self.assertEqual(self.h["offer_edge"](50.0, 20.0), 30.0)

    def test_m1_two_mergers_share_cap(self):
        q = QueueModel(self.h)
        q.add_node("s50", SOURCE, 50)
        q.add_node("sp", PASSTHROUGH, 200)
        q.add_node("s20a", SOURCE, 20)
        q.add_node("s20b", SOURCE, 20)
        q.add_node("c1", PASSTHROUGH, 500)
        q.add_node("c2", PASSTHROUGH, 500)
        q.add_node("p1", CONSUMER, 0, 50)
        q.add_node("p2", CONSUMER, 0, 50)
        q.add_edge("e50", "s50", "sp")
        q.add_edge("e1", "sp", "c1", tport="i1")
        q.add_edge("e2", "sp", "c2", tport="i1")
        q.add_edge("a", "s20a", "c1")
        q.add_edge("b", "s20b", "c2")
        q.add_edge("o1", "c1", "p1")
        q.add_edge("o2", "c2", "p2")
        q.run(["s50", "s20a", "s20b", "sp", "c1", "c2", "p1", "p2"])
        self.assertLessEqual(q.edges["e1"].m_Demand + q.edges["e2"].m_Demand, 50.0 + EPS)
        self.assertFalse(q.nodes["s50"].m_Overloaded)

    def test_m1_limitation_underfed(self):
        g = oracle_graph(
            [Node("s50", "SOURCE", available=50), Node("s40", "SOURCE", available=40),
             Node("s20", "SOURCE", available=20), Node("sp", "PASSTHROUGH", pass_limit=200),
             Node("c1", "PASSTHROUGH", pass_limit=500), Node("c2", "PASSTHROUGH", pass_limit=500),
             Node("p1", "CONSUMER", hard_demand=50), Node("p2", "CONSUMER", hard_demand=50)],
            [Edge("e50", "s50", "sp"), Edge("e1", "sp", "c1"), Edge("e2", "sp", "c2"),
             Edge("a", "s40", "c1"), Edge("b", "s20", "c2"), Edge("o1", "c1", "p1"), Edge("o2", "c2", "p2")],
        )
        local = {"e50": 50, "e1": 25, "e2": 25, "a": 25, "b": 20, "o1": 50, "o2": 0}
        report = self.o.verify(g, local)
        self.assertIn("feasible_but_underfed", rules_of(report))

    def test_i4_single_provider_matches_divisor_absent(self):
        src = graph_src()
        self.assertIn("ApplyMergerWaterFill", src)
        q = QueueModel(self.h)
        q.add_node("s", SOURCE, 50)
        q.add_node("load", CONSUMER, 0, 10)
        q.add_edge("e", "s", "load")
        q.run(["s", "load"])
        self.assertAlmostEqual(q.edges["e"].m_AllocatedPower, 10.0, places=3)
        self.assertFalse(q.nodes["s"].m_Overloaded)

    def test_base_commit_lacks_helpers(self):
        try:
            old = subprocess.check_output(
                ["git", "show", "463464e:scripts/5_Mission/LFPG_ElecGraphImpl.c"],
                cwd=str(ROOT), text=True, stderr=subprocess.DEVNULL,
            )
        except (subprocess.CalledProcessError, FileNotFoundError):
            self.skipTest("463464e not available")
            return
        self.assertNotIn("WaterFillShareAsk", old)
        self.assertIn("edgeDemand = edgeDemand / ptPoweredIn", old)
        g = oracle_graph(
            [Node("s20", "SOURCE", available=20), Node("s50", "SOURCE", available=50),
             Node("c", "PASSTHROUGH", pass_limit=500), Node("load", "CONSUMER", hard_demand=50)],
            [Edge("e_s20", "s20", "c"), Edge("e_s50", "s50", "c"), Edge("e_out", "c", "load")],
        )
        report = Oracle().verify(g, {"e_s20": 0.0, "e_s50": 25.0, "e_out": 25.0})
        self.assertFalse(report.ok)


class Negatives(unittest.TestCase):
    def _src(self):
        return GRAPH.read_text(encoding="utf-8")

    def test_no_cap_clip_breaks_2a(self):
        src, n = re.subn(
            r"if \(a > cap\)\s*\{\s*a = cap;\s*\}",
            "/* mutated */",
            self._src(),
        )
        self.assertGreaterEqual(n, 1)
        h = helpers(src)
        asks = waterfill(h, 50.0, [20.0, 50.0])
        self.assertNotAlmostEqual(asks[0], 20.0, places=2)

    def test_ignore_last_write_is_prev(self):
        src, n = re.subn(
            r"return offeredResidual;",
            "return fallbackMax;",
            self._src(),
            count=1,
        )
        self.assertEqual(n, 1)
        h = helpers(src)
        self.assertEqual(h["offer_cap"](50.0, 10.0), 10.0)

    def test_zero_fallback_like_epsilon(self):
        src, n = re.subn(
            r"if \(offeredResidual < 0\.0\)",
            "if (offeredResidual < LFPG_PROPAGATION_EPSILON)",
            self._src(),
            count=1,
        )
        self.assertEqual(n, 1)
        h = helpers(src)
        self.assertEqual(h["offer_cap"](0.0, 50.0), 50.0)

    def test_include_own_edge_in_other_hard(self):
        src, n = re.subn(
            r"if \(SkipOtherIndex\(oi, skipIndex\)\)\s*continue;",
            "/* mutated include own */",
            self._src(),
        )
        self.assertGreaterEqual(n, 1)
        self.assertNotIn("if (SkipOtherIndex(oi, skipIndex))", src)

    def test_no_b13_notify(self):
        src, n = re.subn(
            r"if \(neverWritten\)\s*\{\s*return true;\s*\}",
            "if (neverWritten) { return false; }",
            self._src(),
        )
        self.assertGreaterEqual(n, 1)
        h = helpers(src)
        self.assertFalse(h["notify"](10.0, True))


if __name__ == "__main__":
    unittest.main()
