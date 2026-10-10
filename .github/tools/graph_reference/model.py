"""Network model for the independent graph oracle.

Semantics (from LFPG_ElecNode / LFPG_ElecEdge, not from AllocateOutput):
- SOURCE: available output (W).
- PASSTHROUGH: pass_limit and optional soft_fraction (battery charge share).
- CONSUMER / CAMERA: hard demand.
- Gate closed: no through-flow to downstream (pass_limit treated as 0).
- Disabled edge: no flow.
- Full battery: soft_demand 0.
"""
from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


NODE_TYPES = ("SOURCE", "PASSTHROUGH", "CONSUMER", "CAMERA")


@dataclass
class Node:
    id: str
    type: str
    available: float = 0.0
    pass_limit: float = 0.0
    hard_demand: float = 0.0
    soft_demand: float = 0.0
    soft_fraction: float = 0.0
    gate_closed: bool = False
    self_consumption: float = 0.0
    virtual_generation: float = 0.0

    def __post_init__(self) -> None:
        if self.type not in NODE_TYPES:
            raise ValueError("unknown node type %s on %s" % (self.type, self.id))
        if self.type == "PASSTHROUGH" and self.gate_closed:
            self.pass_limit = 0.0
        if self.soft_demand < 0.0:
            self.soft_demand = 0.0
        if self.soft_fraction < 0.0:
            self.soft_fraction = 0.0
        if self.soft_fraction > 1.0:
            self.soft_fraction = 1.0


@dataclass
class Edge:
    id: str
    src: str
    dst: str
    enabled: bool = True
    capacity: float = 1e12


@dataclass
class Graph:
    nodes: dict[str, Node] = field(default_factory=dict)
    edges: dict[str, Edge] = field(default_factory=dict)
    expected: dict[str, Any] = field(default_factory=dict)
    fixture_id: str = ""
    description: str = ""

    def node(self, nid: str) -> Node:
        return self.nodes[nid]

    def outgoing(self, nid: str) -> list[Edge]:
        return [e for e in self.edges.values() if e.src == nid]

    def incoming(self, nid: str) -> list[Edge]:
        return [e for e in self.edges.values() if e.dst == nid]

    def total_hard_demand(self) -> float:
        total = 0.0
        for n in self.nodes.values():
            if n.type in ("CONSUMER", "CAMERA"):
                total = total + n.hard_demand
            if n.type == "PASSTHROUGH":
                total = total + n.self_consumption
        return total


def load_graph(data: Any) -> Graph:
    if isinstance(data, (str, Path)):
        path = Path(data)
        payload = json.loads(path.read_text(encoding="utf-8"))
    else:
        payload = data
    g = Graph(
        fixture_id=payload.get("id", ""),
        description=payload.get("description", ""),
        expected=payload.get("expected") or {},
    )
    for raw in payload.get("nodes") or []:
        n = Node(
            id=raw["id"],
            type=raw["type"],
            available=float(raw.get("available", 0.0)),
            pass_limit=float(raw.get("pass_limit", 0.0)),
            hard_demand=float(raw.get("hard_demand", 0.0)),
            soft_demand=float(raw.get("soft_demand", 0.0)),
            soft_fraction=float(raw.get("soft_fraction", 0.0)),
            gate_closed=bool(raw.get("gate_closed", False)),
            self_consumption=float(raw.get("self_consumption", 0.0)),
            virtual_generation=float(raw.get("virtual_generation", 0.0)),
        )
        g.nodes[n.id] = n
    for raw in payload.get("edges") or []:
        e = Edge(
            id=raw["id"],
            src=raw["src"],
            dst=raw["dst"],
            enabled=bool(raw.get("enabled", True)),
            capacity=float(raw.get("capacity", 1e12)),
        )
        g.edges[e.id] = e
    return g
