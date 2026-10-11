"""Independent oracle: Edmonds-Karp max-flow on a split-node residual graph.

Hard demand is a sink capacity; sources are edges from a super-source.
Feasibility is residual-graph search. This is not a port of AllocateOutput
or ProcessDirtyQueue (those split demand equally and zero an overloaded
source). Consumers in production are binary (powered iff input >= demand);
max_hard_servable is the integer measure over subsets. max_hard_flow_bound
is the continuous flow cota.
"""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field
from itertools import combinations

from .model import Graph, Node

EPS = 1e-6
SUPER_SRC = "__super_src__"
SUPER_SNK = "__super_snk__"
MAX_BINARY_CONSUMERS = 12


def _in(nid: str) -> str:
    return nid + "#in"


def _out(nid: str) -> str:
    return nid + "#out"


@dataclass
class Violation:
    rule: str
    where: str
    detail: str


@dataclass
class VerifyReport:
    ok: bool
    violations: list[Violation] = field(default_factory=list)

    def add(self, rule: str, where: str, detail: str) -> None:
        self.violations.append(Violation(rule, where, detail))
        self.ok = False


class Residual:
    def __init__(self) -> None:
        self.cap: dict[tuple[str, str], float] = {}
        self.adj: dict[str, list[str]] = {}

    def add_edge(self, u: str, v: str, c: float) -> None:
        if c <= 0.0:
            return
        key = (u, v)
        self.cap[key] = self.cap.get(key, 0.0) + c
        if u not in self.adj:
            self.adj[u] = []
        if v not in self.adj:
            self.adj[v] = []
        if v not in self.adj[u]:
            self.adj[u].append(v)
        if u not in self.adj[v]:
            self.adj[v].append(u)

    def bfs(self, s: str, t: str) -> list[str] | None:
        parent: dict[str, str | None] = {s: None}
        q = deque([s])
        while q:
            u = q.popleft()
            for v in self.adj.get(u, []):
                if v in parent:
                    continue
                if self.cap.get((u, v), 0.0) > EPS:
                    parent[v] = u
                    if v == t:
                        path = [t]
                        cur = t
                        while parent[cur] is not None:
                            cur = parent[cur]
                            path.append(cur)
                        path.reverse()
                        return path
                    q.append(v)
        return None

    def max_flow(self, s: str, t: str) -> float:
        total = 0.0
        while True:
            path = self.bfs(s, t)
            if path is None:
                break
            bottleneck = None
            for i in range(len(path) - 1):
                u = path[i]
                v = path[i + 1]
                c = self.cap.get((u, v), 0.0)
                if bottleneck is None or c < bottleneck:
                    bottleneck = c
            if bottleneck is None or bottleneck <= EPS:
                break
            for i in range(len(path) - 1):
                u = path[i]
                v = path[i + 1]
                self.cap[(u, v)] = self.cap.get((u, v), 0.0) - bottleneck
                self.cap[(v, u)] = self.cap.get((v, u), 0.0) + bottleneck
            total = total + bottleneck
        return total


def reachable_from(graph: Graph, start_ids: list[str]) -> set[str]:
    """Nodes reachable from start_ids along enabled edges, not through a closed gate.

    A closed PASSTHROUGH is itself reachable (self_consumption / probe). Its
    downstream is not. Disabled edges are skipped.
    """
    seen: set[str] = set()
    q = deque()
    for sid in start_ids:
        if sid not in graph.nodes:
            continue
        if sid not in seen:
            seen.add(sid)
            q.append(sid)
    while q:
        uid = q.popleft()
        node = graph.nodes[uid]
        if node.type == "PASSTHROUGH" and node.gate_closed:
            continue
        for e in graph.outgoing(uid):
            if not e.enabled:
                continue
            if e.dst not in graph.nodes:
                continue
            if e.dst not in seen:
                seen.add(e.dst)
                q.append(e.dst)
    return seen


def reachable_ids(graph: Graph) -> set[str]:
    starts = [n.id for n in graph.nodes.values() if n.type == "SOURCE"]
    return reachable_from(graph, starts)


def sources_reaching(graph: Graph, nid: str) -> list[str]:
    out = []
    for n in graph.nodes.values():
        if n.type != "SOURCE":
            continue
        if nid in reachable_from(graph, [n.id]):
            out.append(n.id)
    return out


def _node_through_cap(n: Node) -> float:
    if n.type == "SOURCE":
        return 1e12
    if n.type in ("CONSUMER", "CAMERA"):
        return 1e12
    if n.gate_closed:
        return 0.0
    if n.pass_limit > EPS:
        return n.pass_limit
    return 1e12


def _build_hard_residual(graph: Graph, consumer_ids: set[str] | None) -> Residual:
    reach = reachable_ids(graph)
    r = Residual()
    for n in graph.nodes.values():
        r.add_edge(_in(n.id), _out(n.id), _node_through_cap(n))
        if n.type == "SOURCE":
            r.add_edge(SUPER_SRC, _out(n.id), n.available)
        if n.virtual_generation > EPS:
            r.add_edge(SUPER_SRC, _out(n.id), n.virtual_generation)
        if n.id not in reach:
            continue
        if n.type in ("CONSUMER", "CAMERA"):
            if consumer_ids is None or n.id in consumer_ids:
                r.add_edge(_in(n.id), SUPER_SNK, n.hard_demand)
        if n.type == "PASSTHROUGH" and n.self_consumption > EPS:
            r.add_edge(_in(n.id), SUPER_SNK, n.self_consumption)
    for e in graph.edges.values():
        if not e.enabled:
            continue
        r.add_edge(_out(e.src), _in(e.dst), e.capacity)
    return r


def _soft_absorbed(n: Node, inn: float, out: float) -> float:
    raw = inn + n.virtual_generation - out - n.self_consumption
    if raw < 0.0:
        raw = 0.0
    if raw > n.soft_demand:
        raw = n.soft_demand
    return raw


class Oracle:
    def max_hard_flow_bound(self, graph: Graph) -> float:
        return _build_hard_residual(graph, None).max_flow(SUPER_SRC, SUPER_SNK)

    def reachable_consumers(self, graph: Graph) -> list[Node]:
        reach = reachable_ids(graph)
        out = []
        for n in graph.nodes.values():
            if n.type in ("CONSUMER", "CAMERA") and n.id in reach:
                out.append(n)
        return out

    def reachable_hard_demand(self, graph: Graph) -> float:
        reach = reachable_ids(graph)
        total = 0.0
        for n in graph.nodes.values():
            if n.id not in reach:
                continue
            if n.type in ("CONSUMER", "CAMERA"):
                total = total + n.hard_demand
            if n.type == "PASSTHROUGH":
                total = total + n.self_consumption
        return total

    def _reachable_self_need(self, graph: Graph) -> float:
        reach = reachable_ids(graph)
        total = 0.0
        for n in graph.nodes.values():
            if n.id in reach and n.type == "PASSTHROUGH":
                total = total + n.self_consumption
        return total

    def max_hard_servable(self, graph: Graph) -> float:
        consumers = self.reachable_consumers(graph)
        if len(consumers) > MAX_BINARY_CONSUMERS:
            raise ValueError(
                "max_hard_servable exhaustive search capped at %s reachable consumers, got %s"
                % (MAX_BINARY_CONSUMERS, len(consumers))
            )
        self_need = self._reachable_self_need(graph)
        best = 0.0
        n = len(consumers)
        for k in range(0, n + 1):
            for combo in combinations(consumers, k):
                ids: set[str] = set()
                consumer_need = 0.0
                for c in combo:
                    ids.add(c.id)
                    consumer_need = consumer_need + c.hard_demand
                flow = _build_hard_residual(graph, ids).max_flow(SUPER_SRC, SUPER_SNK)
                if flow + EPS >= consumer_need + self_need and consumer_need > best:
                    best = consumer_need
        return best

    def hard_feasible(self, graph: Graph) -> bool:
        need = self.reachable_hard_demand(graph)
        got = self.max_hard_servable(graph)
        reach = reachable_ids(graph)
        self_need = 0.0
        for n in graph.nodes.values():
            if n.id in reach and n.type == "PASSTHROUGH":
                self_need = self_need + n.self_consumption
        consumer_need = need - self_need
        return got + EPS >= consumer_need

    def verify(self, graph: Graph, allocation: dict[str, float]) -> VerifyReport:
        report = VerifyReport(ok=True)
        reach = reachable_ids(graph)
        flows = {eid: float(allocation.get(eid, 0.0)) for eid in graph.edges}
        for eid, e in graph.edges.items():
            f = flows[eid]
            if f < -EPS:
                report.add("non_negative", eid, "flow %s < 0" % f)
            if not e.enabled and abs(f) > EPS:
                report.add("edge_disabled", eid, "disabled edge carries %s" % f)
            if f > e.capacity + EPS:
                report.add("edge_limit", eid, "flow %s exceeds capacity %s" % (f, e.capacity))

        inflow = {nid: 0.0 for nid in graph.nodes}
        outflow = {nid: 0.0 for nid in graph.nodes}
        for eid, e in graph.edges.items():
            f = flows[eid]
            outflow[e.src] = outflow[e.src] + f
            inflow[e.dst] = inflow[e.dst] + f

        absorbed = {}
        for nid, n in graph.nodes.items():
            inn = inflow[nid]
            out = outflow[nid]
            if n.type == "SOURCE":
                if out > n.available + EPS:
                    report.add(
                        "source_limit",
                        nid,
                        "outflow %s exceeds available %s" % (out, n.available),
                    )
                if inn > EPS:
                    report.add("conservation", nid, "SOURCE has inflow %s" % inn)
            elif n.type == "PASSTHROUGH":
                soft = _soft_absorbed(n, inn, out)
                absorbed[nid] = soft
                lhs = inn + n.virtual_generation
                rhs = out + n.self_consumption + soft
                delta = lhs - rhs
                if delta < 0.0:
                    delta = -delta
                if delta > EPS:
                    report.add(
                        "conservation",
                        nid,
                        "in+virt %s != out+self+soft %s" % (lhs, rhs),
                    )
                limit = 0.0 if n.gate_closed else n.pass_limit
                if n.pass_limit > EPS or n.gate_closed:
                    if out > limit + EPS:
                        report.add(
                            "passthrough_limit",
                            nid,
                            "outflow %s exceeds pass_limit %s" % (out, limit),
                        )
            elif n.type in ("CONSUMER", "CAMERA"):
                if out > EPS:
                    report.add("conservation", nid, "consumer has outflow %s" % out)

        unmet_hard = 0.0
        deficit_nodes: list[str] = []
        for nid, n in graph.nodes.items():
            if n.type not in ("CONSUMER", "CAMERA"):
                if n.type == "PASSTHROUGH" and n.id in reach and n.self_consumption > EPS:
                    got = inflow[nid]
                    if got + EPS < n.self_consumption:
                        report.add(
                            "hard_unmet",
                            nid,
                            "gate/self received %s of %s" % (got, n.self_consumption),
                        )
                        unmet_hard = unmet_hard + (n.self_consumption - got)
                        deficit_nodes.append(nid)
                continue
            got = inflow[nid]
            if n.id not in reach:
                continue
            if got > n.hard_demand + EPS:
                report.add(
                    "over_allocation",
                    nid,
                    "received %s > demand %s" % (got, n.hard_demand),
                )
            elif got + EPS < n.hard_demand:
                deficit = n.hard_demand - got
                unmet_hard = unmet_hard + deficit
                report.add(
                    "hard_unmet",
                    nid,
                    "received %s of hard %s (deficit %s)" % (got, n.hard_demand, deficit),
                )
                deficit_nodes.append(nid)
                if got > EPS:
                    report.add(
                        "partial_allocation",
                        nid,
                        "received %s of binary demand %s" % (got, n.hard_demand),
                    )

        for nid, n in graph.nodes.items():
            if n.type != "PASSTHROUGH":
                continue
            if absorbed.get(nid, 0.0) <= EPS:
                continue
            bat_sources = sources_reaching(graph, nid)
            shared_hit = None
            for deficit_id in deficit_nodes:
                def_sources = sources_reaching(graph, deficit_id)
                shared = [s for s in bat_sources if s in def_sources]
                if shared:
                    shared_hit = (deficit_id, shared[0])
                    break
            if shared_hit is None:
                continue
            report.add(
                "hard_priority",
                nid,
                "battery absorbed %s while %s has hard deficit sharing SOURCE %s"
                % (absorbed[nid], shared_hit[0], shared_hit[1]),
            )

        if unmet_hard > EPS and self.hard_feasible(graph):
            report.add(
                "feasible_but_underfed",
                "*",
                "reachable hard is integer-feasible but this assignment leaves deficit %s"
                % unmet_hard,
            )
        return report
