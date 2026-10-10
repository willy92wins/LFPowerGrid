"""Independent oracle: max-flow by hard-priority phases (Edmonds-Karp).

This is not a port of AllocateOutput / ProcessDirtyQueue. Those split demand
equally and zero an overloaded source. Here hard demand is a sink capacity and
sources are edges from a super-source; feasibility is residual-graph search.
"""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field

from .model import Graph

EPS = 1e-6
SUPER_SRC = "__super_src__"
SUPER_SNK = "__super_snk__"


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


def _build_hard_residual(graph: Graph) -> Residual:
    r = Residual()
    for n in graph.nodes.values():
        r.add_edge(_in(n.id), _out(n.id), _node_through_cap(n))
        if n.type == "SOURCE":
            r.add_edge(SUPER_SRC, _out(n.id), n.available)
        if n.type in ("CONSUMER", "CAMERA"):
            r.add_edge(_in(n.id), SUPER_SNK, n.hard_demand)
        if n.type == "PASSTHROUGH" and n.self_consumption > EPS:
            r.add_edge(_in(n.id), SUPER_SNK, n.self_consumption)
        if n.virtual_generation > EPS:
            r.add_edge(SUPER_SRC, _out(n.id), n.virtual_generation)
    for e in graph.edges.values():
        if not e.enabled:
            continue
        r.add_edge(_out(e.src), _in(e.dst), e.capacity)
    return r


def _node_through_cap(n) -> float:
    if n.type == "SOURCE":
        return 1e12
    if n.type in ("CONSUMER", "CAMERA"):
        return 1e12
    if n.gate_closed:
        return 0.0
    if n.pass_limit > EPS:
        return n.pass_limit
    return 1e12


class Oracle:
    def max_hard_servable(self, graph: Graph) -> float:
        return _build_hard_residual(graph).max_flow(SUPER_SRC, SUPER_SNK)

    def hard_feasible(self, graph: Graph) -> bool:
        need = graph.total_hard_demand()
        got = self.max_hard_servable(graph)
        return got + EPS >= need

    def verify(self, graph: Graph, allocation: dict[str, float]) -> VerifyReport:
        report = VerifyReport(ok=True)
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
                created = out - (inn + n.virtual_generation)
                if created > EPS:
                    report.add(
                        "conservation",
                        nid,
                        "outflow %s exceeds inflow %s + virt %s"
                        % (out, inn, n.virtual_generation),
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
        for nid, n in graph.nodes.items():
            if n.type in ("CONSUMER", "CAMERA"):
                got = inflow[nid]
                if got + EPS < n.hard_demand:
                    deficit = n.hard_demand - got
                    unmet_hard = unmet_hard + deficit
                    report.add(
                        "hard_unmet",
                        nid,
                        "received %s of hard %s (deficit %s)" % (got, n.hard_demand, deficit),
                    )
            if n.type == "PASSTHROUGH" and n.self_consumption > EPS:
                got = inflow[nid]
                if got + EPS < n.self_consumption:
                    report.add(
                        "hard_unmet",
                        nid,
                        "gate/self received %s of %s" % (got, n.self_consumption),
                    )

        soft_flow = 0.0
        for nid, n in graph.nodes.items():
            if n.type == "PASSTHROUGH" and n.soft_demand > EPS:
                extra = inflow[nid] - n.self_consumption
                if extra > EPS:
                    credited = extra
                    if credited > n.soft_demand:
                        credited = n.soft_demand
                    soft_flow = soft_flow + credited
        if unmet_hard > EPS and soft_flow > EPS:
            report.add(
                "hard_priority",
                "*",
                "soft %s allocated while hard deficit %s remains" % (soft_flow, unmet_hard),
            )

        if unmet_hard > EPS and self.hard_feasible(graph):
            report.add(
                "feasible_but_underfed",
                "*",
                "hard demand is feasible (max-flow) but this assignment leaves deficit %s"
                % unmet_hard,
            )
        return report
