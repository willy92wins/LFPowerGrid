"""Logic-signal channel: PDQ model + production gate/cell callbacks (R26)."""
from __future__ import print_function

from pathlib import Path
from types import SimpleNamespace
import os
import random
import re
import sys
import traceback
import unittest

CURRENT_TEST = ""

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from enforce_scalar_slice import load, method, scalar_function
from test_graph_multifeed_split import (
    EPS,
    SOURCE,
    CONSUMER,
    CAMERA,
    PASSTHROUGH,
    QueueModel,
    helpers,
    graph_src,
)
from test_memorycell_toggle import (
    PORT_POWER,
    PORT_TOGGLE,
    PORT_RESET,
    PORT_SET,
    CB_POWER,
    CB_TOGGLE,
    CB_DEFER,
    FIELDS,
    member_init,
    bind,
    env_for,
    const_int,
)

GATE_SRC = ROOT / "scripts/4_World/LFPG_LogicGate.c"
CELL_SRC = ROOT / "scripts/4_World/LFPG_MemoryCell.c"
BUTTON_SRC = ROOT / "scripts/4_World/LFPG_PushButton.c"
DATA_SRC = ROOT / "scripts/3_Game/LFPG_Data.c"
GRAPH_BASE = ROOT / "scripts/4_World/LFPG_ElecGraph.c"
DEFINES = ROOT / "scripts/3_Game/LFPG_Defines.c"
CHECKS = ROOT / ".github/workflows/checks.yml"

TICK_MS = 100
MAX_TICKS = 100
PROBE = 1.0
NODE_BUDGET = 64
EDGE_BUDGET = 256
MAX_REQUEUE = 5
DIRTY_TOPOLOGY = 1
DIRTY_INPUT = 2
DIRTY_INTERNAL = 4
EDGE_ENABLED = 1

PORT_IN0 = "input_0"
PORT_IN1 = "input_1"
PORT_IN2 = "input_2"
PORT_IN3 = "input_3"
PORT_OUT0 = "output_0"
PORT_OUT1 = "output_1"
PORT_OUT2 = "output_2"
PORT_OUT3 = "output_3"
VALIDATE_INTERVAL = 10
GATE_RETRY_PER_CALL = 4
MAX_EDGES_PER_NODE = 12
SPLITTER_OUT_PORTS = (PORT_OUT1, PORT_OUT2, PORT_OUT3)

_ANNOTATED = []
_SKIPPED = []
_RESULTS = []
_SRC_OVERRIDE = None
_SRC_FILES = {}
MUTANT = None


class HarnessError(Exception):
    """Extraction or fixture assembly failed; not a behavioural assertion."""


def trace_on():
    needle = os.environ.get("LFPG_TEST_TRACE") or ""
    if not needle:
        return False
    return needle in CURRENT_TEST


def tlog(msg):
    if trace_on():
        sys.stderr.write(msg + "\n")
        sys.stderr.flush()


def src_graph(text=None):
    if text is not None:
        return graph_src(text)
    if _SRC_OVERRIDE is not None:
        return _SRC_OVERRIDE
    impl = ROOT / "scripts/5_Mission/LFPG_ElecGraphImpl.c"
    if impl in _SRC_FILES:
        return _SRC_FILES[impl]
    return graph_src(None)


def src_data():
    if DATA_SRC in _SRC_FILES:
        return _SRC_FILES[DATA_SRC]
    return DATA_SRC.read_text(encoding="utf-8")


def src_gate():
    if GATE_SRC in _SRC_FILES:
        return _SRC_FILES[GATE_SRC]
    return GATE_SRC.read_text(encoding="utf-8")


def src_cell():
    if CELL_SRC in _SRC_FILES:
        return _SRC_FILES[CELL_SRC]
    return CELL_SRC.read_text(encoding="utf-8")


def has_signal_fix(src=None):
    text = src_graph(src)
    return "ComputeSignalOut" in text and "IsPortReceivingSignal" in text


def _norm_nl(s):
    return s.replace("\r\n", "\n").replace("\r", "\n")


def apply_mutant_replace(text, old, new, count, name):
    text = _norm_nl(text)
    old = _norm_nl(old)
    new = _norm_nl(new)
    n = text.count(old)
    if n != count:
        raise HarnessError(
            "error de arnes: mutant %s count %s want %s" % (name, n, count)
        )
    mutated = text.replace(old, new)
    if mutated == text:
        raise HarnessError("error de arnes: mutant %s replace no aplico" % name)
    return mutated


def load_signal_out(src=None):
    # Prefer live src_graph() so MUTANT / _SRC_OVERRIDE apply; QueueModel.src is a
    # snapshot of the file taken before the mutant runner sets the flag.
    text = _norm_nl(src_graph(None) if src is None else src_graph(src))
    if MUTANT == "compute_no_autoconsumo":
        text = _norm_nl(src_graph(None))
        old = (
            "        if (cons > LFPG_PROPAGATION_EPSILON)\n        {\n            if (powered)\n            {\n                return true;\n            }\n            return false;"
        )
        new = (
            "        if (false)\n        {\n            if (powered)\n            {\n                return true;\n            }\n            return false;"
        )
        body_pre = brace_body(text, "ComputeSignalOut")
        if "if (cons > LFPG_PROPAGATION_EPSILON)" in body_pre:
            n = text.count(old)
            if n >= 1:
                text = text.replace(old, new, 1)
            else:
                pat = re.compile(
                    r"if\s*\(\s*cons\s*>\s*LFPG_PROPAGATION_EPSILON\s*\)\s*\{",
                )
                text = pat.sub("if (false) {", text, count=1)
        elif "if (false)" not in body_pre:
            raise HarnessError("error de arnes: compute_no_autoconsumo no esta en el texto")
        body = brace_body(text, "ComputeSignalOut")
        if "if (cons > LFPG_PROPAGATION_EPSILON)" in body:
            raise HarnessError("error de arnes: compute_no_autoconsumo no se aplico")
    if "ComputeSignalOut" not in text:
        return None
    return load(
        text,
        "ComputeSignalOut",
        ["isSource", "newOutput", "signalIn", "virt", "cons", "powered", "gateClosed"],
        {"LFPG_PROPAGATION_EPSILON": EPS},
    )


class _OutArr:
    def __init__(self, items=None):
        self._v = items

    def Count(self):
        if self._v is None:
            return 0
        return len(self._v)

    def __getitem__(self, i):
        return self._v[i]

    def __bool__(self):
        return bool(self._v)


class _IncomingMap:
    def __init__(self, model):
        self.model = model

    def Find(self, device_id, box):
        items = self.model.incoming.get(device_id)
        box._v = items
        return items is not None


def load_is_port_receiving_signal(src=None):
    """Execute the extracted IsPortReceivingSignal body, including for-loops."""
    text = _norm_nl(src_graph(None) if src is None else src_graph(src))
    if "IsPortReceivingSignal" not in text:
        return None
    try:
        body = method(text, "IsPortReceivingSignal")
    except ValueError as ex:
        raise HarnessError("error de arnes: IsPortReceivingSignal: %s" % ex)
    raw = body
    raw = re.sub(r"array<ref LFPG_ElecEdge>\s+inEdges\s*;", "inEdges = OutArr();", raw)
    raw = re.sub(r"\bint\s+(\w+)\s*;", r"\1 = 0;", raw)
    raw = re.sub(r"\bint\s+(\w+)\s*=", r"\1 =", raw)
    raw = re.sub(r"LFPG_ElecEdge\s+edge\s*=", "edge =", raw)
    raw = raw.replace("LFPG_EDGE_ENABLED", "EDGE_ENABLED")
    raw = qualify_members(raw, "g")
    try:
        generated = scalar_function(raw, ["g", "deviceId", "portName"])
    except (ValueError, IndexError, SyntaxError) as ex:
        raise HarnessError(
            "error de arnes: IsPortReceivingSignal construccion no soportada: %s" % ex
        )
    env = {
        "__builtins__": {"range": range, "int": int, "bool": bool, "len": len},
        "OutArr": _OutArr,
        "EDGE_ENABLED": EDGE_ENABLED,
        "True": True,
        "False": False,
        "None": None,
    }
    exec(compile(generated, "<isport>", "exec"), env)
    inner = env["run"]

    def run(model, device_id, port_name):
        g = SimpleNamespace(
            m_Incoming=_IncomingMap(model),
            m_PropagationEdgeAccountingActive=True,
            m_EdgesVisitedThisEpoch=model.edge_visits,
        )
        result = inner(g, device_id, port_name)
        model.edge_visits = g.m_EdgesVisitedThisEpoch
        if result:
            for e in model.incoming.get(device_id, []):
                if e is None:
                    continue
                model.signal_reads.append(
                    (
                        device_id,
                        port_name,
                        bool(e.m_SignalHigh),
                        e.m_SourceNodeId,
                        model.epoch,
                        model.clock.now_ms,
                    )
                )
                if e.m_SignalHigh and e.m_TargetPort == port_name:
                    break
        return bool(result)

    run.body = body
    return run


def brace_body(source, name):
    match = re.search(r"\b" + re.escape(name) + r"\([^;{]*\)\s*\{", source)
    if not match:
        return ""
    start = match.end() - 1
    depth = 0
    pos = start
    while pos < len(source):
        ch = source[pos]
        if ch == "{":
            depth = depth + 1
        elif ch == "}":
            depth = depth - 1
            if depth == 0:
                return source[start:pos + 1]
        pos = pos + 1
    return ""


def neutralize_c(code):
    code = re.sub(r"#ifdef\s+SERVER", "", code)
    code = re.sub(r"#ifndef\s+SERVER.*?#endif", "", code, flags=re.S)
    code = re.sub(r"#endif", "", code)
    code = re.sub(
        r"if\s*\(\s*LFPG_LOG_LEVEL\s*>=\s*2\s*\)\s*\{(?:[^{}]|\{[^{}]*\})*\}",
        "",
        code,
    )
    code = re.sub(r"LFPG_Util\.\w+\s*\([^;]*\);", "", code)
    # Log messages built in a local: `string togMsg = "..."; togMsg = togMsg + x;`.
    # The slice rejects literals and logging has no behaviour to keep.
    for name in re.findall(r"\bstring\s+(\w+)\s*=\s*\"", code):
        code = re.sub(r"\bstring\s+" + name + r"\s*=[^;]*;", "", code)
        code = re.sub(r"\b" + name + r"\s*=\s*" + name + r"\b[^;]*;", "", code)
    code = re.sub(r"SetSynchDirty\s*\(\s*\)\s*;", "pass;", code)
    code = re.sub(
        r"\b(?:string|vector|ref|ParamsReadContext|ParamsWriteContext|LFPG_[A-Za-z0-9_]+)\s+(?=[A-Za-z_]\w*\s*(?:=|;))",
        "",
        code,
    )
    code = re.sub(r'"(?:\\.|[^"\\])*"', '""', code)
    code = re.sub(r"(\w+)\.ToString\(\)", r"str(\1)", code)
    return code


def qualify_members(code, holder):
    return re.sub(r"(?<![.\w])(m_[A-Za-z0-9_]+)\b", holder + r".\1", code)


def compile_slice_no_quotes(raw, extra, holder):
    """Replace string literals so scalar_function sees none; leftover quotes are HarnessError."""
    raw = raw.replace('"output_0"', "PORT_OUT0")
    raw = raw.replace('"output_1"', "PORT_OUT1")
    raw = raw.replace('""', "EMPTY_ID")
    body = qualify_members(neutralize_c(raw), holder)
    body = body.replace('""', "EMPTY_ID")
    if '"' in body:
        raise HarnessError("error de arnes: quotes remain in slice")
    try:
        return scalar_function(body, extra)
    except ValueError as ex:
        raise HarnessError("error de arnes: scalar slice: %s" % ex)


def with_pass(inner):
    """scalar_function drops empty `{ SetSynchDirty(); }` bodies; Python still needs a block."""
    lines = inner.splitlines()
    out = []
    for i, line in enumerate(lines):
        out.append(line)
        stripped = line.rstrip()
        if not stripped.endswith(":"):
            continue
        indent = len(line) - len(line.lstrip())
        nxt = lines[i + 1] if i + 1 < len(lines) else ""
        if not nxt.strip():
            out.append((" " * (indent + 4)) + "pass")
            continue
        nindent = len(nxt) - len(nxt.lstrip())
        if nindent <= indent:
            out.append((" " * (indent + 4)) + "pass")
    return "\n".join(out)


def bind_method(source, name, extra, holder):
    body = qualify_members(neutralize_c(method(source, name)), holder)
    generated = scalar_function(body, extra)
    inner = generated.split("\n", 1)[1]
    header = "def run(" + holder
    if extra:
        header = header + ", " + ", ".join(extra)
    header = header + "):\n"
    return header + inner + "\n"


def eval_gate_fn(kind, source=None):
    text = GATE_SRC.read_text(encoding="utf-8") if source is None else source
    names = {"AND": "LFPG_AND_Gate", "OR": "LFPG_OR_Gate", "XOR": "LFPG_XOR_Gate"}
    cls = names[kind]
    # Kits are `class LFPG_AND_Gate_Kit`; the device is `class LFPG_AND_Gate :`.
    match = re.search(r"class\s+" + re.escape(cls) + r"\s*:", text)
    if not match:
        raise HarnessError("gate class missing: " + cls)
    chunk = text[match.start():]
    nxt = re.search(r"\nclass\s+", chunk[1:])
    if nxt:
        chunk = chunk[: nxt.start() + 1]
    body = method(chunk, "LFPG_EvaluateGateLogic")
    compiled = scalar_function(body, ["in0", "in1"])
    env = {"__builtins__": {}}
    exec(compile(compiled, "<gate:" + kind + ">", "exec"), env)
    return env["run"]


class FakeNM:
    def __init__(self, model):
        self.model = model

    def _norm(self, port):
        mapping = {
            PORT_POWER: PORT_IN0,
            PORT_TOGGLE: PORT_IN1,
            PORT_RESET: PORT_IN2,
            PORT_SET: PORT_IN3,
        }
        return mapping.get(port, port)

    def IsPortReceivingPower(self, device_id, port):
        port = self._norm(port)
        self.model.note_port_read(device_id, port, "power")
        return self.model.port_receiving_power(device_id, port)

    def IsPortReceivingSignal(self, device_id, port):
        port = self._norm(port)
        self.model.note_port_read(device_id, port, "signal")
        fn = getattr(self.model, "isport_fn", None)
        if fn is not None:
            return fn(self.model, device_id, port)
        return self.model.port_receiving_signal(device_id, port)

    def GetGraph(self):
        return self

    def SetOutputPortEnabled(self, device_id, port, enabled):
        self.model.set_output_port_enabled(device_id, port, enabled)

    def RequestPropagate(self, device_id):
        self.model.request_propagate(device_id)


class FakeGame:
    def __init__(self, clock):
        self.clock = clock

    def GetTime(self):
        return self.clock.now_ms

    def GetCallQueue(self, _category):
        return self.clock


class Clock:
    def __init__(self):
        self.now_ms = 0
        self.pending = []
        self.dispatch = {}
        self.trace = []

    @property
    def now(self):
        return self.now_ms

    @now.setter
    def now(self, value):
        self.now_ms = value

    def Remove(self, callback):
        self.pending = [p for p in self.pending if p[1] is not callback]

    def CallLater(self, callback, delay, _repeat):
        due = self.now_ms + delay
        self.trace.append(("CallLater", self.now_ms, delay, due))
        self.pending.append((due, callback))

    def drain_to(self, limit, restore=True):
        while True:
            due = [p for p in self.pending if p[0] <= limit]
            if not due:
                if restore:
                    self.now_ms = limit
                return
            due.sort(key=lambda p: p[0])
            item = due[0]
            self.pending.remove(item)
            self.now_ms = item[0]
            callback = item[1]
            if callback in self.dispatch:
                self.dispatch[callback]()
            elif callable(callback):
                callback()
        if restore:
            self.now_ms = limit

    def next_timer(self):
        if not self.pending:
            return None
        return min(p[0] for p in self.pending)


class SignalModel(QueueModel):
    """QueueModel plus PDQ Step 2b/3/5 and signal bits. Line cites in comments."""

    def __init__(self, h, src=None, order="scripted", scripted=None, budgets=True):
        super().__init__(h, src=src)
        self.order_mode = order
        self.scripted = list(scripted or [])
        self.budgets = budgets
        self.clock = Clock()
        self.nm = FakeNM(self)
        self.entities = {}
        self.dirty_mask = {}
        self.edge_visits = 0
        self.node_budget = NODE_BUDGET
        self.edge_budget = EDGE_BUDGET
        self.deferred = []
        self.step3_log = []
        self.port_reads = []
        self.latch_changes = {}
        self.cell_flips = {}
        self.compute_signal = load_signal_out()
        self.isport_fn = load_is_port_receiving_signal()
        self.has_fix = self.compute_signal is not None
        self.last_sync_powered = {}
        self.last_sync_overloaded = {}
        self.last_sync_seen = set()
        self.pending_propagate = []
        self.rng = random.Random(20261109)
        self.tick = 0
        self.next_pdq_ms = TICK_MS
        self.tie_timers_first = True
        self.validate_tick = 0
        self.last_validate_tick = 0
        self.validate_idx = 0
        self.last_processed = 0
        self.has_gate_branch = "LFPG_EvaluateGateLogic" in src_graph(self.src) and "ComputeSignalOut" in src_graph(self.src)
        src_txt = src_graph(self.src)
        self.has_retry = "RetryUnresolvedGates" in src_txt and "m_IsLogicGate" in src_data()
        self.gate_retry = []
        self.retry_resolves = 0
        self.last_gate_retry_tick = 0
        self.skip_admit = False
        self.warmup_active = False
        self.probe_settled = False
        self.last_alloc_avail = {}
        self.last_signal_out = {}
        self.last_signal_in = {}
        self.mutant = MUTANT
        self.tick_processed = []
        self.last_sync_entity = {}
        self.resolve_alias = {}
        self.signal_writes = []
        self.signal_reads = []
        self.validate_syncs = []
        self.fail_find = set()
        self.fail_vanilla = set()
        self.g2_calls = 0
        self.fail_netid = set()
        self.registry = {}
        self.pdq_calls = 0
        self.skip_sync_nids = set()

    def add_node(self, nid, kind, max_out, cons=0.0, **kw):
        n = super().add_node(nid, kind, max_out, cons=cons, **kw)
        n.m_IsGated = kw.get("gated", False)
        n.m_IsLogicGate = kw.get("logic_gate", False)
        n.cast_null = False
        n.m_DirtyMask = 0
        n.m_SignalChanged = False
        n.live_gate_open = not n.m_GateClosed
        n.role = kw.get("role", None)
        self.dirty_mask[nid] = 0
        self.latch_changes[nid] = 0
        return n

    def _reaches(self, start, goal, seen=None):
        if seen is None:
            seen = set()
        if start == goal:
            return True
        if start in seen:
            return False
        seen.add(start)
        for e in self.outgoing.get(start, []):
            if self._reaches(e.m_TargetNodeId, goal, seen):
                return True
        return False

    def add_edge(self, eid, src, dst, sport="output_0", tport="input_0"):
        if not self.skip_admit:
            if src == dst:
                raise AssertionError("admission: self-wire %s" % src)
            if self._reaches(dst, src):
                raise AssertionError("admission: cycle %s -> %s" % (src, dst))
            src_deg = len(self.outgoing.get(src, [])) + len(self.incoming.get(src, []))
            dst_deg = len(self.outgoing.get(dst, [])) + len(self.incoming.get(dst, []))
            if src_deg + 1 > MAX_EDGES_PER_NODE or dst_deg + 1 > MAX_EDGES_PER_NODE:
                raise AssertionError("admission: >12 edges on %s or %s" % (src, dst))
            sn = self.nodes.get(src)
            dn = self.nodes.get(dst)
            if sn is not None and getattr(sn, "role", None) == "splitter":
                if sport not in SPLITTER_OUT_PORTS:
                    raise AssertionError("admission: splitter port %s" % sport)
            if dn is not None and getattr(dn, "role", None) == "splitter":
                if tport != "input_1":
                    raise AssertionError("admission: splitter in %s" % tport)
            if sn is not None and getattr(sn, "role", None) == "gate":
                if sport != PORT_OUT0:
                    raise AssertionError("admission: gate out %s" % sport)
            if dn is not None and getattr(dn, "role", None) == "gate":
                if tport not in (PORT_IN0, PORT_IN1):
                    raise AssertionError("admission: gate in %s" % tport)
            if sn is not None and getattr(sn, "role", None) == "cell":
                if sport not in (PORT_OUT0, PORT_OUT1):
                    raise AssertionError("admission: cell out %s" % sport)
            if dn is not None and getattr(dn, "role", None) == "cell":
                if tport not in (PORT_IN0, PORT_IN1, PORT_IN2, PORT_IN3):
                    raise AssertionError("admission: cell in %s" % tport)
        e = super().add_edge(eid, src, dst, sport=sport, tport=tport)
        e.m_Flags = EDGE_ENABLED
        e.m_SignalHigh = True if MUTANT == "ctor_signal_true" else False
        return e

    def attach_gate(self, nid, kind, gate_source=None):
        text = src_gate() if gate_source is None else gate_source
        eval_fn = eval_gate_fn(kind, text)
        ent = SimpleNamespace(
            m_DeviceId=nid,
            m_PoweredNet=False,
            m_Input0Powered=False,
            m_Input1Powered=False,
            m_GateOpen=False,
            m_Overloaded=False,
            m_BrownoutHeld=False,
            kind=kind,
            role="gate",
        )
        raw = method(text, "LFPG_SetPowered")
        raw = raw.replace('"input_0"', "PORT_IN0")
        raw = raw.replace('"input_1"', "PORT_IN1")
        body = qualify_members(neutralize_c(raw), "gate")
        generated = scalar_function(body, ["powered"])
        inner = with_pass(generated.split("\n", 1)[1])
        env = {
            "__builtins__": {"str": str},
            "PORT_IN0": PORT_IN0,
            "PORT_IN1": PORT_IN1,
            "LFPG_NetworkManager": SimpleNamespace(Get=lambda: self.nm),
            "LFPG_EvaluateGateLogic": lambda a, b: eval_fn(a, b),
            "LFPG_LOG_LEVEL": 0,
            "str": str,
        }
        exec(compile("def run(gate, powered):\n" + inner, "<gate-setpowered>", "exec"), env)
        run = env["run"]

        def setpowered(powered, _run=run, _ent=ent):
            before_open = _ent.m_GateOpen
            _run(_ent, powered)
            self.nodes[nid].live_gate_open = _ent.m_GateOpen
            tlog(
                "SET tick=%s epoch=%s node=%s powered=%s in0=%s in1=%s latch=%s brownoutHeld=%s"
                % (
                    self.tick,
                    self.epoch,
                    nid,
                    powered,
                    _ent.m_Input0Powered,
                    _ent.m_Input1Powered,
                    _ent.m_GateOpen,
                    _ent.m_BrownoutHeld,
                )
            )
            return _ent.m_GateOpen

        ent.setpowered = setpowered
        ent.eval_logic = eval_fn
        ent.cast_null = False
        self.entities[nid] = ent
        self.nodes[nid].m_IsGated = True
        self.nodes[nid].m_IsLogicGate = True
        self.nodes[nid].m_GateClosed = True
        self.nodes[nid].live_gate_open = False
        self.nodes[nid].role = "gate"
        return ent

    def attach_switch(self, nid, on=True):
        ent = SimpleNamespace(m_DeviceId=nid, role="switch", on=on)

        def is_open():
            return ent.on

        ent.is_open = is_open
        self.entities[nid] = ent
        n = self.nodes[nid]
        n.m_IsGated = True
        n.live_gate_open = on
        n.m_GateClosed = not on
        n.role = "switch"
        return ent

    def attach_laser(self, nid, on=False):
        ent = SimpleNamespace(m_DeviceId=nid, role="laser", on=on)
        ent.is_open = lambda: ent.on
        self.entities[nid] = ent
        n = self.nodes[nid]
        n.m_IsGated = True
        n.m_Consumption = 5.0
        n.live_gate_open = on
        n.m_GateClosed = not on
        n.role = "laser"
        return ent

    def attach_pushbutton(self, nid):
        # LFPG_PushButton.c:76-112 — pulse falls by CallLater, not by unpowering G.
        text = BUTTON_SRC.read_text(encoding="utf-8")
        pulse_ms = const_int(DEFINES.read_text(encoding="utf-8"), "LFPG_BUTTON_PULSE_MS")
        cb_pulse = object()
        ent = SimpleNamespace(
            m_DeviceId=nid,
            m_SwitchOn=False,
            m_PoweredNet=False,
            role="push",
        )

        def _request(did):
            self.request_propagate(did)

        env = {
            "__builtins__": {"str": str},
            "str": str,
            "g_Game": FakeGame(self.clock),
            "CALL_CATEGORY_SYSTEM": 0,
            "LFPG_LOG_LEVEL": 0,
            "LFPG_BUTTON_PULSE_MS": pulse_ms,
            "LFPG_PulseOff": cb_pulse,
            "LFPG_NetworkManager": SimpleNamespace(
                Get=lambda: SimpleNamespace(RequestPropagate=_request)
            ),
            "SetSynchDirty": lambda: None,
        }
        exec(compile(bind_method(text, "LFPG_ToggleButton", [], "btn"), "<btn-toggle>", "exec"), env)
        toggle_fn = env["run"]
        exec(compile(bind_method(text, "LFPG_PulseOff", [], "btn"), "<btn-pulseoff>", "exec"), env)
        off_fn = env["run"]

        def sync_gate():
            n = self.nodes[nid]
            n.live_gate_open = bool(ent.m_SwitchOn)
            n.m_GateClosed = not ent.m_SwitchOn

        def on_pulse_off():
            off_fn(ent)
            sync_gate()

        self.clock.dispatch[cb_pulse] = on_pulse_off

        def press():
            toggle_fn(ent)
            sync_gate()

        ent.press = press
        self.entities[nid] = ent
        n = self.nodes[nid]
        n.m_IsGated = True
        n.live_gate_open = False
        n.m_GateClosed = True
        n.role = "push"
        return ent

    def attach_cell(self, nid, cell_source=None):
        text = src_cell() if cell_source is None else cell_source
        cell = SimpleNamespace()
        for field in FIELDS:
            if field == "m_DeviceId":
                cell.m_DeviceId = nid
                continue
            cell.__dict__[field] = member_init(text, field)
        cell.m_ToggleGraceUntilMs = 0
        cell.role = "cell"
        env, game = env_for(text, self.clock, self.nm)
        env["__builtins__"] = {"str": str}
        env["LFPG_NetworkManager"] = SimpleNamespace(Get=lambda: self.nm)
        env["SetSynchDirty"] = lambda: None
        cb_power = object()
        cb_toggle = object()
        cb_defer = object()
        env["LFPG_MCellPowerSettleTimer"] = cb_power
        env["LFPG_MCellToggleSettleTimer"] = cb_toggle
        env["LFPG_DeferredRouting"] = cb_defer
        env["LFPG_MCellApplyDue"] = lambda nowMs: env["_apply"](cell, nowMs)
        env["LFPG_MCellCommitPower"] = lambda: env["_commit_p"](cell)
        env["LFPG_MCellCommitToggle"] = lambda: env["_commit_t"](cell)
        env["LFPG_MCellSettle"] = lambda: env["_settle"](cell)
        env["LFPG_SetPowered"] = lambda powered: env["_setpowered"](cell, powered)

        def compile_into(name, extra, key):
            try:
                wrapped = bind(text, name, extra)
                exec(compile(wrapped, "<slice:" + name + ">", "exec"), env)
            except ValueError as ex:
                raise HarnessError("error de arnes: scalar slice %s: %s" % (name, ex))
            env[key] = env["run"]

        compile_into("LFPG_MCellCommitPower", [], "_commit_p")
        compile_into("LFPG_MCellCommitToggle", [], "_commit_t")
        compile_into("LFPG_MCellApplyDue", ["nowMs"], "_apply")
        compile_into("LFPG_SetPowered", ["powered"], "_setpowered")
        compile_into("LFPG_MCellSettle", [], "_settle")
        compile_into("LFPG_MCellPowerSettleTimer", [], "_power_timer")
        compile_into("LFPG_MCellToggleSettleTimer", [], "_toggle_timer")
        env["EMPTY_ID"] = ""
        env["PORT_OUT0"] = PORT_OUT0
        env["PORT_OUT1"] = PORT_OUT1
        raw_apply = method(text, "LFPG_ApplyRouting")
        gen_apply = compile_slice_no_quotes(raw_apply, [], "cell")
        env_apply = dict(env)
        exec(compile("def run(cell):\n" + with_pass(gen_apply.split("\n", 1)[1]), "<apply-routing>", "exec"), env_apply)
        env["_apply_r"] = env_apply["run"]

        def apply_routing():
            env["_apply_r"](cell)

        env["LFPG_ApplyRouting"] = apply_routing
        raw_defer = method(text, "LFPG_DeferredRouting")
        gen_defer = compile_slice_no_quotes(raw_defer, [], "cell")
        exec(compile("def run(cell):\n" + with_pass(gen_defer.split("\n", 1)[1]), "<deferred-routing>", "exec"), env)
        env["_defer"] = env["run"]

        def counted(fn):
            def run_timer():
                before = cell.m_LatchState
                fn(cell)
                if cell.m_LatchState != before:
                    self.latch_changes[nid] = self.latch_changes.get(nid, 0) + 1
            return run_timer

        self.clock.dispatch[cb_power] = counted(lambda c: env["_power_timer"](c))
        self.clock.dispatch[cb_toggle] = counted(lambda c: env["_toggle_timer"](c))
        self.clock.dispatch[cb_defer] = lambda: env["_defer"](cell)
        cell.cb_power = cb_power
        cell.cb_toggle = cb_toggle
        cell.cb_defer = cb_defer

        def call_set(powered):
            before = cell.m_LatchState
            env["_setpowered"](cell, powered)
            tlog(
                "SET tick=%s epoch=%s node=%s powered=%s P=%s T=%s R=%s S=%s latch=%s active=%s"
                % (
                    self.tick,
                    self.epoch,
                    nid,
                    powered,
                    self.port_receiving_power(nid, PORT_IN0),
                    self.port_receiving_power(nid, PORT_IN1),
                    self.port_receiving_power(nid, PORT_IN2),
                    self.port_receiving_power(nid, PORT_IN3),
                    cell.m_LatchState,
                    cell.m_CellActive,
                )
            )
            if cell.m_LatchState != before:
                self.latch_changes[nid] = self.latch_changes.get(nid, 0) + 1
            return cell.m_LatchState

        self.nm.GetGraph = lambda: self.nm
        cell.setpowered = call_set
        cell.apply_routing = apply_routing
        cell.env = env
        cell.raw = cell
        self.entities[nid] = cell
        self.nodes[nid].role = "cell"
        apply_routing()
        return cell

    def set_output_port_enabled(self, device_id, port_name, enabled):
        # LFPG_ElecGraphImpl.c:5075-5098
        any_changed = False
        for e in self.outgoing.get(device_id, []):
            if e.m_SourcePort != port_name:
                continue
            was = (e.m_Flags & EDGE_ENABLED) != 0
            if enabled and not was:
                e.m_Flags = e.m_Flags | EDGE_ENABLED
                if self.has_fix and self.mutant != "no_clear_setoutput":
                    e.m_SignalHigh = False
                any_changed = True
            elif (not enabled) and was:
                e.m_Flags = 0
                if self.has_fix and self.mutant != "no_clear_setoutput":
                    e.m_SignalHigh = False
                any_changed = True
        if any_changed:
            self.mark(device_id, DIRTY_TOPOLOGY)

    def mark(self, nid, mask=DIRTY_INPUT):
        # MarkNodeDirty :2109-2115: a node that already ran this epoch is
        # rewound unconditionally, with its requeue counted.
        if nid not in self.nodes:
            return
        n = self.nodes[nid]
        self.dirty_mask[nid] = self.dirty_mask.get(nid, 0) | mask
        if self.epoch > 0 and n.m_LastEpoch == self.epoch:
            n.m_RequeueCount = n.m_RequeueCount + 1
            n.m_LastEpoch = self.epoch - 1
        if not n.m_InQueue:
            n.m_InQueue = True
            self.queue.append(nid)
        if self.mutant == "enqueue_switch_bit" and getattr(n, "role", None) == "switch":
            ent = self.entities.get(nid)
            bit = bool(getattr(ent, "on", False)) if ent is not None else False
            for e in self.outgoing.get(nid, []):
                e.m_SignalHigh = bit

    def request_propagate(self, nid):
        self.pending_propagate.append(nid)
        self.mark(nid, DIRTY_INTERNAL)

    def note_port_read(self, device_id, port, kind):
        self.port_reads.append((device_id, port, kind, self.epoch, self.clock.now_ms))

    def port_receiving_power(self, device_id, port):
        # LFPG_ElecGraphImpl.c:5013-5048
        for e in self.incoming.get(device_id, []):
            self.edge_visits = self.edge_visits + 1
            if (e.m_Flags & EDGE_ENABLED) == 0:
                continue
            if e.m_TargetPort != port:
                continue
            if e.m_AllocatedPower > EPS:
                return True
        return False

    def port_receiving_signal(self, device_id, port):
        if self.isport_fn is not None:
            return self.isport_fn(self, device_id, port)
        for e in self.incoming.get(device_id, []):
            self.edge_visits = self.edge_visits + 1
            if (e.m_Flags & EDGE_ENABLED) == 0:
                continue
            if self.mutant != "isport_no_port" and e.m_TargetPort != port:
                continue
            writer = e.m_SourceNodeId
            self.signal_reads.append(
                (device_id, port, bool(e.m_SignalHigh), writer, self.epoch, self.clock.now_ms)
            )
            if e.m_SignalHigh:
                return True
        return False

    def resolve_find(self, nid):
        if nid in self.fail_find:
            return None
        if nid in self.resolve_alias:
            return self.resolve_alias[nid]
        return self.entities.get(nid)

    def resolve_vanilla(self, nid):
        if nid in self.fail_vanilla:
            return None
        return self.entities.get(nid)

    def resolve_netid(self, nid):
        if self.mutant == "netid_always_none":
            return None
        if nid in self.fail_netid:
            return None
        return self.entities.get(nid)

    def resolve_gated_entity(self, nid):
        if self.mutant == "pregated_find_only":
            return self.resolve_find(nid)
        ent = self.resolve_find(nid)
        if ent is not None:
            return ent
        ent = self.resolve_vanilla(nid)
        if ent is not None:
            return ent
        ent = self.resolve_netid(nid)
        if ent is not None:
            self.registry[nid] = True
            return ent
        return None

    def live_gate_closed(self, nid, ent=None):
        n = self.nodes[nid]
        if not n.m_IsGated:
            return False
        if ent is None:
            ent = self.entities.get(nid)
        if ent is None:
            return n.m_GateClosed
        if getattr(ent, "role", None) == "gate":
            return not ent.m_GateOpen
        if hasattr(ent, "is_open"):
            return not ent.is_open()
        return n.m_GateClosed

    def pop_next(self):
        if not self.queue:
            return None
        if self.order_mode == "scripted" and self.scripted:
            for nid in self.scripted:
                if nid in self.queue:
                    self.queue.remove(nid)
                    return nid
            return self.queue.pop(0)
        if self.order_mode == "lifo":
            return self.queue.pop()
        if self.order_mode == "fifo":
            return self.queue.pop(0)
        if self.order_mode == "perm":
            idx = self.rng.randrange(0, len(self.queue))
            return self.queue.pop(idx)
        return self.queue.pop(0)

    def _target_closed(self, tgt):
        if not tgt.m_IsGated:
            return False
        if tgt.id in self.entities:
            return self.live_gate_closed(tgt.id)
        return tgt.m_GateClosed

    def raw_demand(self, edge, available, _stack=None):
        # Pass 1 :4603-4676. Closed gated PT: probe. Open PT that only
        # feeds a closed gate asks for that probe, not m_MaxOutput (50+50
        # on G was a false overload).
        tgt = self.nodes[edge.m_TargetNodeId]
        if tgt.kind in (CONSUMER, CAMERA):
            return tgt.m_Consumption
        if tgt.kind != PASSTHROUGH:
            return 0.0
        d = tgt.m_LastStableOutput
        if d >= EPS or tgt.m_DemandKnown:
            return d
        if self._target_closed(tgt):
            d = PROBE
            if tgt.m_Consumption > d:
                d = tgt.m_Consumption
            return d + tgt.m_SoftDemand
        stack = _stack or set()
        if tgt.id in stack:
            return tgt.m_Consumption
        stack.add(tgt.id)
        child_sum = 0.0
        has_down = False
        for ce in self.outgoing.get(tgt.id, []):
            if (ce.m_Flags & EDGE_ENABLED) == 0:
                continue
            has_down = True
            child_sum = child_sum + self.raw_demand(ce, available, stack)
        if has_down:
            if child_sum > EPS:
                return child_sum
            return PROBE
        # AllocateOutput Pass 1 :4888-4890 (no enabled downstream).
        src = _norm_nl(src_graph())
        m = re.search(
            r"else\s*\{\s*edgeDemand = targetNode\.m_Consumption([^;]*);",
            src,
        )
        extra = 0.0
        if m and "+ 1.0" in m.group(0):
            extra = 1.0
        elif m and m.group(1).strip():
            raise HarnessError("error de arnes: fallback Splitter no parseado: %s" % m.group(0))
        return tgt.m_Consumption + extra

    def allocate_with_signal(self, nid, available, signal_out):
        prev = []
        for e in self.outgoing[nid]:
            prev.append(e.m_AllocatedPower)
        total = self.allocate(nid, available)
        n = self.nodes[nid]
        for i, e in enumerate(self.outgoing[nid]):
            delta = e.m_AllocatedPower - prev[i]
            if delta < 0.0:
                delta = -delta
            if delta > EPS:
                self.m_AllocChanged = True  # :4788-4794
        if self.has_fix:
            overloaded = bool(n.m_Overloaded)
            for e in self.outgoing[nid]:
                if (e.m_Flags & EDGE_ENABLED) == 0:
                    continue
                if self.mutant == "signal_in_overloaded" and overloaded:
                    continue
                write_bit = signal_out
                if self.mutant == "signal_tied_overloaded":
                    write_bit = not overloaded
                if e.m_SignalHigh != write_bit:
                    n.m_SignalChanged = True
                e.m_SignalHigh = write_bit
                self.signal_writes.append((nid, self.epoch, self.clock.now_ms, write_bit, e.m_TargetNodeId))
        return total

    def process(self, nid):
        n = self.nodes[nid]
        n.m_InQueue = False
        dirty_mask = self.dirty_mask.get(nid, 0)
        n.m_AllocChanged = False
        n.m_SignalChanged = False
        self.m_AllocChanged = False
        extra = []
        alloc_avail = 0.0
        trig = {}
        self.edge_visits = self.edge_visits + 0

        # Step 1 :2308-2349
        insum = 0.0
        signal_in = False
        for e in self.incoming.get(nid, []):
            self.edge_visits = self.edge_visits + 1
            if (e.m_Flags & EDGE_ENABLED) == 0:
                if self.mutant == "step1_no_enabled":
                    if self.has_fix and e.m_SignalHigh:
                        signal_in = True
                continue
            insum = insum + self.edge_power(e)
            if self.has_fix and e.m_SignalHigh:
                signal_in = True
        self.last_signal_in[nid] = signal_in
        n.m_PrevInputPower = n.m_InputPower
        n.m_InputPower = insum

        # Step 2 SOURCE/PT :2366-2426 including newPowered :2379-2399
        new_output = 0.0
        new_powered = False
        if n.kind == SOURCE:
            if n.m_Powered:
                new_output = n.m_MaxOutput
            new_powered = n.m_Powered
        elif n.kind == PASSTHROUGH:
            effective = insum + n.m_VirtualGeneration
            if effective > EPS:
                self_cons = n.m_Consumption
                if self_cons > EPS:
                    if effective + EPS >= self_cons:
                        new_powered = True
                        after = effective - self_cons
                        if after < 0.0:
                            after = 0.0
                        new_output = after
                    else:
                        new_powered = False
                        new_output = 0.0
                else:
                    if effective > EPS:
                        new_powered = True
                    new_output = effective
                if n.m_MaxOutput > EPS and new_output > n.m_MaxOutput:
                    new_output = n.m_MaxOutput
        else:
            if n.m_Consumption > EPS:
                new_powered = insum + EPS >= n.m_Consumption
            else:
                new_powered = insum > EPS
            new_output = 0.0
        n.m_Powered = new_powered

        gate_is_closed = False
        gate_entity_resolved = False
        if n.kind in (SOURCE, PASSTHROUGH):
            if n.m_IsGated:
                resolved = self.resolve_gated_entity(nid) if self.has_fix else self.resolve_find(nid)
                gate_entity_resolved = resolved is not None
                if gate_entity_resolved:
                    gate_is_closed = self.live_gate_closed(nid, resolved)
                else:
                    gate_is_closed = n.m_GateClosed
                if self.mutant == "compute_gate_cached":
                    gate_is_closed = n.m_GateClosed
            if n.kind == SOURCE:
                n.m_OutputPower = new_output
            alloc_avail = new_output
            if gate_is_closed:
                alloc_avail = 0.0  # :2519-2522
            self.last_alloc_avail[nid] = alloc_avail
            signal_out = False
            self.last_signal_out[nid] = signal_out
            unresolved_gated = n.m_IsGated and not gate_entity_resolved
            if self.has_retry and unresolved_gated:
                if self.mutant != "cached_latch_v3":
                    self.queue_gate_retry(nid)
                if n.m_IsLogicGate:
                    if self.mutant == "cached_latch_v3":
                        signal_out = not n.m_GateClosed
                    else:
                        signal_out = False
                else:
                    src_txt = src_graph()
                    all_gated_low = bool(
                        re.search(
                            r"m_IsGated\s*&&\s*!gateEntityResolved\)\s*\{\s*signalOut\s*=\s*false",
                            _norm_nl(src_txt),
                        )
                    )
                    if all_gated_low:
                        signal_out = False
                    else:
                        signal_out = self.compute_signal(
                            n.kind == SOURCE,
                            new_output,
                            signal_in,
                            n.m_VirtualGeneration,
                            n.m_Consumption,
                            new_powered,
                            n.m_GateClosed,
                        )
            elif self.has_retry and n.m_IsLogicGate and getattr(resolved if gate_entity_resolved else None, "cast_null", False):
                if self.mutant == "no_cast_null":
                    signal_out = self.compute_signal(
                        n.kind == SOURCE, new_output, signal_in,
                        n.m_VirtualGeneration, n.m_Consumption, new_powered, gate_is_closed,
                    ) if self.has_fix else False
                else:
                    self.queue_gate_retry(nid)
                    signal_out = False
            elif self.has_fix and n.m_IsGated and not gate_entity_resolved and not self.has_retry and self.mutant != "all_gated_low":
                if n.m_IsLogicGate:
                    if self.mutant == "cached_latch_v3":
                        signal_out = not n.m_GateClosed
                    else:
                        signal_out = False
                else:
                    signal_out = self.compute_signal(
                        n.kind == SOURCE,
                        new_output,
                        signal_in,
                        n.m_VirtualGeneration,
                        n.m_Consumption,
                        new_powered,
                        n.m_GateClosed,
                    )
            elif self.has_fix:
                closed_arg = n.m_GateClosed if self.mutant == "compute_gate_cached" else gate_is_closed
                signal_out = self.compute_signal(
                    n.kind == SOURCE,
                    new_output,
                    signal_in,
                    n.m_VirtualGeneration,
                    n.m_Consumption,
                    new_powered,
                    closed_arg,
                )
                # S12 / §5.7: live LFPG_EvaluateGateLogic on input_0/input_1, after
                # ComputeSignalOut and before AllocateOutput; does not write gate/alloc.
                if self.has_gate_branch and self.mutant != "no_gate_branch":
                    ent = self.entities.get(nid)
                    eval_fn = getattr(ent, "eval_logic", None) if ent is not None else None
                    if eval_fn is not None:
                        a = self.nm.IsPortReceivingSignal(nid, PORT_IN0)
                        b = self.nm.IsPortReceivingSignal(nid, PORT_IN1)
                        signal_out = bool(eval_fn(a, b))
                elif self.mutant == "no_gate_branch" and n.m_IsLogicGate:
                    signal_out = signal_in
            self.last_signal_out[nid] = signal_out
            downstream = self.allocate_with_signal(nid, alloc_avail, signal_out)
            if new_output < EPS:
                n.m_Overloaded = False
            if gate_is_closed:
                n.m_Overloaded = False
            demand_became_known = False
            if n.kind == PASSTHROUGH:
                prev_sig = n.m_LastStableOutput
                downstream_soft = getattr(self, "last_alloc_soft", 0.0)
                total_soft = downstream_soft + n.m_SoftDemand
                hard_base = downstream - downstream_soft + n.m_Consumption
                if hard_base < 0.0:
                    hard_base = 0.0
                virt = n.m_VirtualGeneration
                if virt > hard_base:
                    virt = hard_base
                demand_signal = hard_base - virt + total_soft
                if demand_signal < 0.0:
                    demand_signal = 0.0
                if demand_signal > EPS:
                    new_output = demand_signal
                    if n.m_MaxOutput > EPS and new_output > n.m_MaxOutput:
                        new_output = n.m_MaxOutput
                    n.m_SoftDemandRatio = total_soft / new_output if new_output > EPS else 0.0
                else:
                    n.m_SoftDemandRatio = 0.0
                    if n.m_Consumption > EPS:
                        new_output = n.m_Consumption
                    else:
                        new_output = 0.0  # :2614-2629 open, no load
                if n.m_IsGated:
                    prev_gate_closed = n.m_GateClosed
                    if gate_entity_resolved:
                        n.m_GateClosed = gate_is_closed
                    if gate_is_closed:
                        base_d = PROBE
                        if n.m_Consumption > base_d:
                            base_d = n.m_Consumption
                        new_output = base_d + n.m_SoftDemand  # :2658-2671
                    if gate_entity_resolved and prev_gate_closed != n.m_GateClosed:
                        txt = _norm_nl(src_graph())
                        idx = txt.find("prevGateClosed != node.m_GateClosed")
                        window = txt[idx:idx + 500] if idx >= 0 else ""
                        sets_alloc = "m_AllocChanged = true" in window
                        if sets_alloc:
                            self.m_AllocChanged = True
                            n.m_AllocChanged = True
                        for e in self.incoming.get(nid, []):
                            self.edge_visits = self.edge_visits + 1
                            extra.append(e.m_SourceNodeId)
                            self.mark(e.m_SourceNodeId, DIRTY_INPUT)
                if not n.m_DemandKnown:
                    n.m_DemandKnown = True
                    demand_became_known = True
                extra.extend(self.publish_offers(nid, alloc_avail))
            else:
                extra.extend(self.publish_offers(nid, alloc_avail))
                demand_became_known = False

            output_delta = new_output - n.m_LastStableOutput
            if output_delta < 0.0:
                output_delta = -output_delta
            input_changed = False
            if n.kind == PASSTHROUGH:
                input_delta = insum - n.m_PrevInputPower
                if input_delta < 0.0:
                    input_delta = -input_delta
                if input_delta > EPS:
                    input_changed = True
            force_downstream = (dirty_mask & DIRTY_TOPOLOGY) != 0 and n.kind in (SOURCE, PASSTHROUGH)
            trig = {
                "outputDelta": output_delta > EPS,
                "forceDownstream": force_downstream,
                "m_AllocChanged": bool(self.m_AllocChanged or n.m_Overloaded is not None and getattr(self, "m_AllocChanged", False)),
                "inputChanged": input_changed,
                "demandBecameKnown": demand_became_known,
                "m_SignalChanged": n.m_SignalChanged if self.has_fix else False,
            }
            trig["m_AllocChanged"] = bool(self.m_AllocChanged)
            self.step3_log.append((nid, dict(trig), self.epoch))
            step3_if = re.search(
                r"if \(outputDelta > LFPG_PROPAGATION_EPSILON \|\| forceDownstream \|\| m_AllocChanged \|\| ([^)]*)\)",
                _norm_nl(src_graph()),
            )
            sig_in_step3 = bool(step3_if and "m_SignalChanged" in step3_if.group(0))
            fire = (
                trig["outputDelta"]
                or trig["forceDownstream"]
                or trig["m_AllocChanged"]
                or trig["inputChanged"]
                or trig["demandBecameKnown"]
                or (trig["m_SignalChanged"] and sig_in_step3)
            )
            if fire:
                n.m_OutputPower = new_output
                n.m_LastStableOutput = new_output
                for e in self.outgoing[nid]:
                    self.edge_visits = self.edge_visits + 1
                    tgt = e.m_TargetNodeId
                    tn = self.nodes[tgt]
                    tn.m_RequeueCount = tn.m_RequeueCount + 1
                    # :2815 same-epoch only for forceDownstream / m_AllocChanged
                    same_epoch = force_downstream or self.m_AllocChanged
                    if self.has_fix and self.mutant != "drop_signal_same_epoch":
                        same_epoch = same_epoch or n.m_SignalChanged
                    if same_epoch and tn.m_LastEpoch == self.epoch:
                        tn.m_LastEpoch = self.epoch - 1
                    extra.append(tgt)
                    self.mark(tgt, DIRTY_INPUT)
                if n.kind == PASSTHROUGH:
                    for e in self.incoming.get(nid, []):
                        self.edge_visits = self.edge_visits + 1
                        up = self.nodes[e.m_SourceNodeId]
                        up.m_RequeueCount = up.m_RequeueCount + 1
                        # :2860-2864 always rewind upstream when Step 3 fires
                        if up.m_LastEpoch == self.epoch:
                            up.m_LastEpoch = self.epoch - 1
                        extra.append(e.m_SourceNodeId)
                        self.mark(e.m_SourceNodeId, DIRTY_INPUT)
        # A CONSUMER marks nothing upstream in the PDQ: the only marks are :2706, :2821, :2866.

        # Step 4 :2877
        if self.mutant == "no_edge_budget" and self.edge_visits > 8:
            return extra
        n.m_LastEpoch = self.epoch
        trig_on = [k for k, v in trig.items() if v]
        outs = []
        for eid, e in self.edges.items():
            if e.m_SourceNodeId != nid:
                continue
            outs.append("%s:d=%.3f/a=%.3f/en=%s" % (
                eid,
                e.m_Demand,
                e.m_AllocatedPower,
                int((e.m_Flags & EDGE_ENABLED) != 0),
            ))
        tlog(
            "PDQ tick=%s epoch=%s node=%s mask=%s inSum=%.3f allocAvail=%.3f pub=%.3f liveClosed=%s overloaded=%s step3=%s rq=%s outs=[%s] q=%s"
            % (
                self.tick,
                self.epoch,
                nid,
                dirty_mask,
                insum,
                alloc_avail,
                n.m_LastStableOutput,
                int(self.live_gate_closed(nid)) if n.m_IsGated else "-",
                int(bool(n.m_Overloaded)),
                ",".join(trig_on) if trig_on else "-",
                n.m_RequeueCount,
                " ".join(outs),
                ",".join(self.queue),
            )
        )
        self.tick_processed.append(nid)
        # :2881 clears the mask before Step 5, so a mark made inside Step 5 survives.
        self.dirty_mask[nid] = 0
        self.sync_entity(nid, n, dirty_mask)
        return extra

    def sync_entity(self, nid, n, dirty_mask):
        # NodeEntitySyncUnchanged :2966-3029; miss clears last registry :3117
        if self.has_fix:
            ent = self.resolve_gated_entity(nid)
        else:
            ent = self.resolve_find(nid)
            if ent is None:
                ent = self.resolve_vanilla(nid)
            if ent is None:
                ent = self.resolve_netid(nid)
        if ent is None:
            self.last_sync_entity.pop(nid, None)
            self.last_sync_seen.discard(nid)
            return
        skip = True
        if (dirty_mask & DIRTY_TOPOLOGY) != 0:
            skip = False
        elif (dirty_mask & DIRTY_INPUT) != 0:
            skip = False
        elif nid not in self.last_sync_entity:
            skip = False
        elif self.last_sync_entity.get(nid) is not ent:
            skip = False
        else:
            last_p = self.last_sync_powered.get(nid)
            if last_p is None or last_p != n.m_Powered:
                skip = False
            last_o = self.last_sync_overloaded.get(nid)
            if n.kind in (SOURCE, PASSTHROUGH):
                if last_o is None or last_o != n.m_Overloaded:
                    skip = False
        if self.mutant == "skip_recovered_sync" and nid in self.skip_sync_nids:
            return
        if skip:
            return
        self.last_sync_entity[nid] = ent
        self.last_sync_seen.add(nid)
        self.last_sync_powered[nid] = n.m_Powered
        self.last_sync_overloaded[nid] = n.m_Overloaded
        if hasattr(ent, "setpowered"):
            before_gate = None
            if getattr(ent, "role", None) == "gate":
                before_gate = ent.m_GateOpen
            ent.setpowered(n.m_Powered)
            if getattr(ent, "role", None) == "gate" and ent.m_GateOpen != before_gate:
                self.latch_changes[nid] = self.latch_changes.get(nid, 0) + 1

    def _fire_one_timer(self):
        due = min(self.clock.pending, key=lambda p: p[0])
        self.clock.pending.remove(due)
        self.clock.now_ms = due[0]
        callback = due[1]
        if callback in self.clock.dispatch:
            self.clock.dispatch[callback]()
        elif callable(callback):
            callback()

    def queue_gate_retry(self, nid):
        if nid not in self.gate_retry:
            self.gate_retry.append(nid)

    def retry_unresolved_gates(self):
        # Cadence on m_ValidateTickCount; RemoveOrdered(0)+Insert; DIRTY_INPUT; count at start.
        if not self.gate_retry:
            return
        if self.mutant != "retry_no_cadence":
            if self.validate_tick - self.last_gate_retry_tick < VALIDATE_INTERVAL:
                return
        self.last_gate_retry_tick = self.validate_tick
        n = len(self.gate_retry)
        if self.mutant != "retry_no_cap" and n > GATE_RETRY_PER_CALL:
            n = GATE_RETRY_PER_CALL
        if self.mutant == "retry_no_cap":
            n = len(self.gate_retry)
        if self.mutant == "retry_no_rotate":
            n = min(n, GATE_RETRY_PER_CALL)
        i = 0
        while i < n and self.gate_retry:
            if self.mutant == "retry_remove":
                nid = self.gate_retry[0]
                self.gate_retry[0] = self.gate_retry[-1]
                self.gate_retry.pop()
            else:
                nid = self.gate_retry.pop(0)
            i = i + 1
            if nid not in self.nodes or not self.nodes[nid].m_IsGated:
                continue
            self.retry_resolves = self.retry_resolves + 1
            resolved = self.resolve_gated_entity(nid)
            if resolved is None:
                if self.mutant == "retry_no_rotate":
                    self.gate_retry.insert(0, nid)
                else:
                    self.gate_retry.append(nid)
                continue
            if self.nodes[nid].m_IsLogicGate and getattr(resolved, "cast_null", False):
                if self.mutant == "retry_no_rotate":
                    self.gate_retry.insert(0, nid)
                else:
                    self.gate_retry.append(nid)
                continue
            mask = DIRTY_INTERNAL if self.mutant == "retry_dirty_internal" else DIRTY_INPUT
            self.mark(nid, mask)

    def pdq_tick(self, seeds=None, warmup=False, advance_clock=False):
        self.pdq_calls = self.pdq_calls + 1
        self.tick = self.tick + 1
        self.epoch = self.epoch + 1
        self.validate_tick = self.validate_tick + 1
        self.clock.trace.append(("PDQ", self.clock.now_ms, self.epoch))
        tlog("TICK %s epoch=%s now=%s queue=%s" % (self.tick, self.epoch, self.clock.now_ms, ",".join(self.queue)))
        if advance_clock:
            self.clock.now_ms = self.clock.now_ms + TICK_MS
            self.clock.drain_to(self.clock.now_ms, restore=True)
        for nid, n in self.nodes.items():
            n.m_RequeueCount = 0
            if self.dirty_mask.get(nid, 0) and not n.m_InQueue:
                n.m_InQueue = True
                self.queue.append(nid)
        if seeds:
            for s in seeds:
                self.mark(s, DIRTY_INTERNAL)
        for p in self.pending_propagate:
            self.mark(p, DIRTY_INTERNAL)
        self.pending_propagate = []
        node_budget = self.node_budget
        edge_budget = self.edge_budget
        if warmup:
            node_budget = 128
            edge_budget = 512
        elif len(self.queue) > 128:
            node_budget = node_budget * 2
            edge_budget = edge_budget * 2
        if not self.budgets:
            node_budget = 10 ** 9
            edge_budget = 10 ** 9
        processed = 0
        self.edge_visits = 0
        self.tick_processed = []
        if warmup:
            self.warmup_active = True
        if self.mutant == "no_node_budget":
            node_budget = 10 ** 9
        if self.mutant == "no_edge_budget":
            edge_budget = 10 ** 9
        if not self.queue:
            # :2220-2238: starts empty, sweeps and returns; what the retry queued waits for the next call.
            self.validate_consumers(edge_budget)
            if self.mutant == "count_before_retry":
                rem = self._finish_tick(0)
                if self.has_retry and self.mutant != "no_retry_empty":
                    self.retry_unresolved_gates()
                return rem
            if self.has_retry and self.mutant != "no_retry_empty":
                self.retry_unresolved_gates()
            return self._finish_tick(0)
        while self.queue:
            # LFPG_ElecGraphImpl.c:2250-2253 — budgets before taking a node.
            if processed >= node_budget:
                break
            if self.edge_visits >= edge_budget:
                break
            nid = self.pop_next()
            n = self.nodes[nid]
            if n.m_LastEpoch == self.epoch:
                n.m_InQueue = False
                continue
            processed = processed + 1
            if n.m_RequeueCount > MAX_REQUEUE:
                n.m_InQueue = False
                self.deferred.append(nid)
                continue
            self.process(nid)
        for dr in self.deferred:
            n = self.nodes[dr]
            if not n.m_InQueue:
                n.m_InQueue = True
                self.queue.append(dr)
        self.deferred = []
        if not self.queue:
            self.validate_consumers(edge_budget)
            if self.mutant == "count_before_retry":
                rem = self._finish_tick(processed)
                if self.has_retry and self.mutant != "no_retry_empty":
                    self.retry_unresolved_gates()
                return rem
            if self.has_retry and self.mutant != "no_retry_empty":
                self.retry_unresolved_gates()
        elif self.mutant == "uncond_sweep":
            self.validate_consumers(edge_budget)
        return self._finish_tick(processed)

    def _finish_tick(self, processed):
        # LFPG_NetworkManagerImpl.c:2462-2468 and LFPG_PerfProbe.c:171 read this value.
        self.last_processed = processed
        remaining = len(self.queue)
        if remaining <= 0:
            self.warmup_active = False
            self.probe_settled = True
        else:
            self.probe_settled = False
        if getattr(self, "record_mixed_pdq", False):
            if not hasattr(self, "pulse_calls"):
                self.pulse_calls = []
            self.pulse_calls.append((list(self.tick_processed), list(self.queue)))
        return remaining

    def validate_consumers(self, edge_budget):
        # Only with empty queue (:2219-2231, :2896-2921); skip in-queue or dirty (:3466).
        if self.mutant != "uncond_sweep" and self.queue:
            return
        if self.validate_tick - self.last_validate_tick < 10:
            return
        self.last_validate_tick = self.validate_tick
        ids = list(self.nodes.keys())
        if not ids:
            return
        batch = 32
        checked = 0
        while checked < batch and checked < len(ids):
            if self.edge_visits >= edge_budget:
                break
            if self.validate_idx >= len(ids):
                self.validate_idx = 0
            nid = ids[self.validate_idx]
            self.validate_idx = self.validate_idx + 1
            checked = checked + 1
            n = self.nodes[nid]
            if n.kind == SOURCE:
                continue
            if self.mutant != "uncond_sweep" and (n.m_InQueue or self.dirty_mask.get(nid, 0)):
                continue
            incoming = 0.0
            has_in = False
            for e in self.incoming.get(nid, []):
                self.edge_visits = self.edge_visits + 1
                if (e.m_Flags & EDGE_ENABLED) == 0:
                    continue
                has_in = True
                incoming = incoming + e.m_AllocatedPower
            effective = incoming
            can_eval = has_in
            if n.kind == PASSTHROUGH:
                effective = effective + n.m_VirtualGeneration
                can_eval = effective > EPS
            should = False
            if can_eval:
                if n.m_Consumption > EPS:
                    should = effective + EPS >= n.m_Consumption
                else:
                    should = effective > EPS
            if n.m_Powered != should:
                n.m_Powered = should
                self.validate_syncs.append(nid)
                self.sync_entity(nid, n, 0)

    def run_events(self, ms, seeds=None, warmup=False, tie_timers_first=None):
        """Advance the event clock by ms: timers at their instant, PDQ every 100 ms."""
        if tie_timers_first is None:
            tie_timers_first = self.tie_timers_first
        end = self.clock.now_ms + ms
        first = True
        self.event_trace = []
        while True:
            nxt_t = self.clock.next_timer()
            nxt_p = self.next_pdq_ms
            due_t = nxt_t is not None and nxt_t <= end
            due_p = nxt_p <= end
            if not due_t and not due_p:
                self.clock.drain_to(end, restore=True)
                break
            if due_t and due_p and nxt_t == nxt_p:
                if tie_timers_first:
                    self.event_trace.append("t")
                    self._fire_one_timer()
                    self.clock.now_ms = nxt_p
                    self.event_trace.append("pdq")
                    self.pdq_tick(seeds if first else None, warmup=warmup, advance_clock=False)
                    first = False
                    self.next_pdq_ms = self.next_pdq_ms + TICK_MS
                else:
                    self.clock.now_ms = nxt_p
                    self.event_trace.append("pdq")
                    self.pdq_tick(seeds if first else None, warmup=warmup, advance_clock=False)
                    first = False
                    self.next_pdq_ms = self.next_pdq_ms + TICK_MS
                    if self.clock.next_timer() == nxt_p or (
                        self.clock.next_timer() is not None and self.clock.next_timer() <= nxt_p
                    ):
                        self.event_trace.append("t")
                        self._fire_one_timer()
                continue
            if due_t and (not due_p or nxt_t < nxt_p):
                self._fire_one_timer()
                continue
            self.clock.now_ms = nxt_p
            self.pdq_tick(seeds if first else None, warmup=warmup, advance_clock=False)
            first = False
            self.next_pdq_ms = self.next_pdq_ms + TICK_MS
        dirty_left = any(self.dirty_mask.get(i, 0) for i in self.nodes)
        self.queue_empty = not self.queue and not self.pending_propagate and not dirty_left
        self.settled_no_retry = self.queue_empty and not self.gate_retry
        self.settled_empty = self.queue_empty and not self.clock.pending
        return ms

    def settle(self, seeds=None, ticks=MAX_TICKS, warmup=True, require_empty=True, idle=False):
        first = True
        n = 0
        if warmup:
            while n < 5 and n < ticks:
                self.pdq_tick(seeds if first else None, warmup=True, advance_clock=False)
                first = False
                n = n + 1
        remain = ticks - n
        if remain > 0:
            self.run_events(remain * TICK_MS, seeds if first else None, warmup=False)
            n = ticks
        dirty_left = any(self.dirty_mask.get(i, 0) for i in self.nodes)
        self.queue_empty = not self.queue and not self.pending_propagate and not dirty_left
        self.settled_no_retry = self.queue_empty and not self.gate_retry
        self.settled_empty = self.queue_empty and not self.clock.pending
        if require_empty and not self.settled_empty:
            raise AssertionError("propagation did not empty in %s ticks" % ticks)
        if self.has_fix and self.mutant is None and self.queue_empty and not self.gate_retry:
            self.g2_calls = self.g2_calls + 1
            diffs = g2_compare(self)
            if diffs:
                raise AssertionError("G2 after empty: %s" % diffs)
        return n

    def edge_alloc(self, eid):
        return self.edges[eid].m_AllocatedPower

    def edge_signal(self, eid):
        return self.edges[eid].m_SignalHigh

    def invariant_ok(self):
        for e in self.edges.values():
            if (e.m_Flags & EDGE_ENABLED) == 0:
                continue
            if e.m_AllocatedPower > EPS and not e.m_SignalHigh:
                return False
        return True


def table_bit(kind, a, b):
    if kind == "AND":
        return bool(a and b)
    if kind == "OR":
        return bool(a or b)
    if kind == "XOR":
        return bool(a) != bool(b)
    raise AssertionError("unknown gate")


def g2_oracle(q):
    """G2 from the §2.1 table. Does not call the source or ComputeSignalOut."""
    expected = {}
    for eid in q.edges:
        expected[eid] = False
    for _i in range(len(q.nodes) + 2):
        for eid, e in q.edges.items():
            if (e.m_Flags & EDGE_ENABLED) == 0:
                continue
            src_id = e.m_SourceNodeId
            src = q.nodes[src_id]
            ent = q.entities.get(src_id)
            ins = []
            for ie in q.incoming.get(src_id, []):
                if (ie.m_Flags & EDGE_ENABLED) == 0:
                    continue
                for k, ev in q.edges.items():
                    if ev is ie:
                        ins.append((ie.m_TargetPort, expected.get(k, False)))
            any_in = any(v for _p, v in ins)
            in0 = any(v for p, v in ins if p == PORT_IN0)
            in1 = any(v for p, v in ins if p == PORT_IN1)
            high = False
            virt = src.m_VirtualGeneration
            cons = src.m_Consumption
            if src.kind == SOURCE:
                high = bool(src.m_Powered) and src.m_MaxOutput > EPS
            elif src.kind in (CONSUMER, CAMERA):
                high = False
            elif ent is not None and getattr(ent, "role", None) == "gate":
                high = table_bit(ent.kind, in0, in1)
            elif ent is not None and hasattr(ent, "on"):
                high = (any_in or virt > EPS) and bool(ent.on)
                # §2.1 PASSTHROUGH con autoconsumo: newPowered (entrada >= consumo).
                if cons > EPS:
                    high = high and bool(src.m_Powered)
            elif src.m_IsGated and q.live_gate_closed(src_id, ent):
                high = False
            else:
                live_in = any_in or virt > EPS
                if not live_in:
                    high = False
                elif cons > EPS:
                    high = bool(src.m_Powered)
                else:
                    high = True
            expected[eid] = high
    return expected


def g2_compare(q):
    expected = g2_oracle(q)
    diffs = []
    for eid, e in q.edges.items():
        if (e.m_Flags & EDGE_ENABLED) == 0:
            continue
        if bool(e.m_SignalHigh) != bool(expected[eid]):
            diffs.append((eid, bool(e.m_SignalHigh), bool(expected[eid])))
    for nid, ent in q.entities.items():
        if getattr(ent, "role", None) != "gate":
            continue
        ins = []
        for ie in q.incoming.get(nid, []):
            if (ie.m_Flags & EDGE_ENABLED) == 0:
                continue
            for k, ev in q.edges.items():
                if ev is ie:
                    ins.append((ie.m_TargetPort, expected.get(k, False)))
        in0 = any(v for p, v in ins if p == PORT_IN0)
        in1 = any(v for p, v in ins if p == PORT_IN1)
        want = table_bit(ent.kind, in0, in1)
        if bool(ent.m_GateOpen) != bool(want):
            diffs.append(("latch", nid, bool(ent.m_GateOpen), bool(want)))
    return diffs


# Codex PLAN-codex.md §4.2 order (a): XOR, A, B, lamp, XOR, Splitter, … G last.
ORDER_A_SPLIT = ["gate", "swA", "swB", "lamp", "spl", "G"]
ORDER_A_DIRECT = ["G", "swA", "swB", "gate", "lamp"]


def add_splitter(q, nid, cap=500):
    q.add_node(nid, PASSTHROUGH, cap, role="splitter")
    q.nodes[nid].role = "splitter"
    return q.nodes[nid]


def circuit_a(kind, a_on, b_on, splitter=True, load=True, order="scripted", require_empty=True):
    h = helpers()
    scripted = list(ORDER_A_SPLIT if splitter else ORDER_A_DIRECT)
    q = SignalModel(h, order=order, scripted=scripted)
    q.add_node("G", SOURCE, 50)
    q.nodes["G"].m_Powered = True
    if splitter:
        add_splitter(q, "spl")
        q.add_edge("e_gs", "G", "spl", tport="input_1")
        q.add_node("swA", PASSTHROUGH, 50, gated=True)
        q.add_node("swB", PASSTHROUGH, 50, gated=True)
        q.add_edge("e_sa", "spl", "swA", sport=PORT_OUT1)
        q.add_edge("e_sb", "spl", "swB", sport=PORT_OUT2)
    else:
        q.add_node("swA", PASSTHROUGH, 50, gated=True)
        q.add_node("swB", PASSTHROUGH, 50, gated=True)
        q.add_edge("e_sa", "G", "swA")
        q.add_edge("e_sb", "G", "swB")
    q.add_node("gate", PASSTHROUGH, 100, gated=True)
    q.add_edge("e_a0", "swA", "gate", sport=PORT_OUT0, tport=PORT_IN0)
    q.add_edge("e_b1", "swB", "gate", sport=PORT_OUT0, tport=PORT_IN1)
    if load:
        q.add_node("lamp", CONSUMER, 0, 10)
        q.add_edge("e_gl", "gate", "lamp")
    q.attach_switch("swA", on=a_on)
    q.attach_switch("swB", on=b_on)
    q.attach_gate("gate", kind)
    seeds = ["G", "swA", "swB", "gate"]
    if splitter:
        seeds.insert(1, "spl")
    if load:
        seeds.append("lamp")
    q.settle(seeds, require_empty=require_empty)
    return q


def circuit_b1(a_on, b_on, order="scripted"):
    h = helpers()
    q = SignalModel(h, order=order, scripted=["G", "spl", "swA", "swB", "gate", "door"])
    q.add_node("G", SOURCE, 50)
    q.nodes["G"].m_Powered = True
    add_splitter(q, "spl")
    q.add_node("swA", PASSTHROUGH, 50, gated=True)
    q.add_node("swB", PASSTHROUGH, 50, gated=True)
    q.add_node("gate", PASSTHROUGH, 100, gated=True)
    q.add_node("door", CONSUMER, 0, 1)
    q.add_edge("e_gs", "G", "spl", tport="input_1")
    q.add_edge("e_sa", "spl", "swA", sport=PORT_OUT1)
    q.add_edge("e_sb", "spl", "swB", sport=PORT_OUT2)
    q.add_edge("e_a0", "swA", "gate", tport=PORT_IN0)
    q.add_edge("e_b1", "swB", "gate", tport=PORT_IN1)
    q.add_edge("e_gd", "gate", "door")
    q.attach_switch("swA", on=a_on)
    q.attach_switch("swB", on=b_on)
    q.attach_gate("gate", "XOR")
    q.settle(["G", "spl", "swA", "swB", "gate", "door"], require_empty=False)
    return q


def circuit_d5(power_via_splitter=True, laser_via_switch=True, power_on=True, order="scripted"):
    h = helpers()
    scripted = ["G", "spl", "swP", "laser", "swL", "cell", "lamp"]
    q = SignalModel(h, order=order, scripted=scripted)
    q.add_node("G", SOURCE, 50)
    q.nodes["G"].m_Powered = True
    add_splitter(q, "spl")
    q.add_edge("e_gs", "G", "spl", tport="input_1")
    q.add_node("swP", PASSTHROUGH, 50, gated=True)
    q.add_node("laser", PASSTHROUGH, 50, cons=5.0, gated=True)
    q.add_node("cell", PASSTHROUGH, 100, cons=1.0)
    q.add_node("lamp", CONSUMER, 0, 10)
    if power_via_splitter:
        q.add_edge("e_sp", "spl", "swP", sport=PORT_OUT1)
    else:
        q.add_edge("e_gp", "G", "swP")
    q.add_edge("e_pc", "swP", "cell", tport=PORT_IN0)
    q.add_edge("e_sl", "spl", "laser", sport=PORT_OUT2)
    if laser_via_switch:
        q.add_node("swL", PASSTHROUGH, 50, gated=True)
        q.add_edge("e_ls", "laser", "swL")
        q.add_edge("e_set", "swL", "cell", tport=PORT_IN3)
        q.attach_switch("swL", on=True)
    else:
        q.add_edge("e_set", "laser", "cell", tport=PORT_IN3)
    q.add_edge("e_out", "cell", "lamp", sport=PORT_OUT0)
    q.attach_switch("swP", on=power_on)
    q.attach_laser("laser", on=False)
    q.attach_cell("cell")
    seeds = ["G", "spl", "swP", "laser", "cell", "lamp"]
    if laser_via_switch:
        seeds.insert(4, "swL")
    q.settle(seeds, require_empty=False)
    return q


WAIT_COMBOS = 30  # 3 s at 100 ms / tick (n131 case_a)
GAP_AB_MS = 250
BANK_SEQ = [
    (False, False),
    (False, True),
    (True, False),
    (True, True),
]


def set_switch(q, nid, on, require_empty=True, ticks=MAX_TICKS):
    q.entities[nid].on = on
    q.nodes[nid].live_gate_open = on
    q.mark(nid, DIRTY_INTERNAL)
    q.settle(require_empty=require_empty, ticks=ticks)


def flip_switch(q, nid, on):
    q.entities[nid].on = on
    q.nodes[nid].live_gate_open = on
    q.mark(nid, DIRTY_INTERNAL)


def wait_combos(q):
    q.settle(require_empty=False, ticks=WAIT_COMBOS, warmup=False)


def combo_window(q, gate_id="gate", lamp_eid="e_gl"):
    """Latch, lamp input and latch-change count over a 3 s window (§6.4)."""
    before = q.latch_changes.get(gate_id, 0)
    wait_combos(q)
    latch = bool(q.entities[gate_id].m_GateOpen)
    lamp = lamp_power(q, lamp_eid)
    changes = q.latch_changes.get(gate_id, 0) - before
    return latch, lamp, changes


def walk_on(q, sw_a, sw_b, gate_id, lamp_eid, sequence, gap_ab_ms=GAP_AB_MS):
    """A, 250 ms, B, then 3 s window. Sequence is the bank 00,01,10,11."""
    rows = []
    for a_on, b_on in sequence:
        if q.entities[sw_a].on != a_on:
            flip_switch(q, sw_a, a_on)
            q.run_events(gap_ab_ms)
        if q.entities[sw_b].on != b_on:
            flip_switch(q, sw_b, b_on)
        latch, lamp, changes = combo_window(q, gate_id, lamp_eid)
        rows.append((a_on, b_on, latch, lamp, changes))
    return rows


def walk_table(kind, splitter, load=True, sequences=None, order="scripted", rng_seed=None):
    if sequences is None:
        sequences = (BANK_SEQ,)
    q = circuit_a(
        kind, False, False, splitter=splitter, load=load, order=order, require_empty=False
    )
    if rng_seed is not None:
        q.rng = random.Random(rng_seed)
    rows = []
    for seq in sequences:
        rows.extend(walk_on(q, "swA", "swB", "gate", "e_gl", seq))
    return q, rows


def add_gate_branch(q, prefix, kind, load=True):
    g, spl, sw_a, sw_b, gate = prefix + "G", prefix + "spl", prefix + "swA", prefix + "swB", prefix + "gate"
    lamp = prefix + "lamp"
    q.add_node(g, SOURCE, 50)
    q.nodes[g].m_Powered = True
    add_splitter(q, spl)
    q.add_edge(prefix + "e_gs", g, spl, tport="input_1")
    q.add_node(sw_a, PASSTHROUGH, 50, gated=True)
    q.add_node(sw_b, PASSTHROUGH, 50, gated=True)
    q.add_edge(prefix + "e_sa", spl, sw_a, sport=PORT_OUT1)
    q.add_edge(prefix + "e_sb", spl, sw_b, sport=PORT_OUT2)
    q.add_node(gate, PASSTHROUGH, 100, gated=True)
    q.add_edge(prefix + "e_a0", sw_a, gate, sport=PORT_OUT0, tport=PORT_IN0)
    q.add_edge(prefix + "e_b1", sw_b, gate, sport=PORT_OUT0, tport=PORT_IN1)
    if load:
        q.add_node(lamp, CONSUMER, 0, 10)
        q.add_edge(prefix + "e_gl", gate, lamp)
    q.attach_switch(sw_a, on=False)
    q.attach_switch(sw_b, on=False)
    q.attach_gate(gate, kind)
    return g, spl, sw_a, sw_b, gate, lamp


def circuit_triple():
    """AND+OR+XOR, each with its own G 50 + Splitter, one PDQ (banco)."""
    h = helpers()
    scripted = []
    for p in ("and_", "or_", "xor_"):
        scripted.extend([p + "gate", p + "swA", p + "swB", p + "lamp", p + "spl", p + "G"])
    q = SignalModel(h, order="scripted", scripted=scripted)
    add_gate_branch(q, "and_", "AND")
    add_gate_branch(q, "or_", "OR")
    add_gate_branch(q, "xor_", "XOR")
    q.settle(list(q.nodes.keys()), require_empty=False)
    return q


def run_bank(q):
    """AND 00,01,10,11 (stays at 11); then OR; then XOR. 250 ms A→B, 3 s window."""
    and_rows = walk_on(q, "and_swA", "and_swB", "and_gate", "and_e_gl", BANK_SEQ)
    or_rows = walk_on(q, "or_swA", "or_swB", "or_gate", "or_e_gl", BANK_SEQ)
    xor_rows = walk_on(q, "xor_swA", "xor_swB", "xor_gate", "xor_e_gl", BANK_SEQ)
    return and_rows, or_rows, xor_rows


def _fmt_rows(rows):
    bits = []
    parts = []
    for a_on, b_on, latch, lamp, changes in rows:
        bits.append(1 if latch else 0)
        parts.append("combo %s%s latch=%s lamp=%s changes=%s" % (
            int(a_on), int(b_on), latch, lamp, changes))
    return bits, parts


def add_admissible_fill(q, src, count, prefix="fill"):
    """3-out splitter tree on its own G so the delay queue does not overload src.

    NODE_BUDGET 64 / 100 ms: count pending nodes >= 720 so A's pass is >1100 ms late.
    """
    g = prefix + "G"
    q.add_node(g, SOURCE, 50)
    q.nodes[g].m_Powered = True
    q.mark(g, DIRTY_INTERNAL)
    root = prefix + "s0"
    add_splitter(q, root)
    q.add_edge(prefix + "e_src", g, root, tport="input_1")
    q.mark(root, DIRTY_INTERNAL)
    leaves = [root]
    n_s = 1
    n_c = 0
    while n_c < count:
        parent = leaves[0]
        used = 0
        for e in q.outgoing.get(parent, []):
            if e.m_SourcePort in SPLITTER_OUT_PORTS:
                used = used + 1
        if used >= 3:
            leaves.pop(0)
            continue
        sport = SPLITTER_OUT_PORTS[used]
        grow = (count - n_c) > (3 - used) and n_s < 400
        if grow:
            sid = prefix + "s%d" % n_s
            n_s = n_s + 1
            add_splitter(q, sid)
            q.add_edge(prefix + "e_" + parent + "_" + sid, parent, sid, sport=sport, tport="input_1")
            q.mark(sid, DIRTY_INTERNAL)
            leaves.append(sid)
        else:
            cid = prefix + "c%d" % n_c
            q.add_node(cid, CONSUMER, 0, 1)
            q.add_edge(prefix + "ec%d" % n_c, parent, cid, sport=sport)
            q.mark(cid, DIRTY_INTERNAL)
            n_c = n_c + 1
    return q


def attach_noise_tree(q, prefix="n"):
    """3-out splitters, 4 levels, 40 splitters, 81 consumers of 10, own G, 122 cables."""
    g = prefix + "G"
    sw = prefix + "sw"
    q.add_node(g, SOURCE, 50)
    q.nodes[g].m_Powered = True
    q.add_node(sw, PASSTHROUGH, 50, gated=True)
    q.attach_switch(sw, on=True)
    q.add_edge(prefix + "e_gsw", g, sw)
    noise = [g, sw]
    levels = []
    root = prefix + "s0"
    q.add_node(root, PASSTHROUGH, 500, role="splitter")
    q.nodes[root].role = "splitter"
    q.add_edge(prefix + "e_sws", sw, root, tport="input_1")
    levels.append([root])
    noise.append(root)
    n_spl = 1
    for lv in range(3):
        nxt = []
        for parent in levels[-1]:
            for k, sport in enumerate(SPLITTER_OUT_PORTS):
                sid = prefix + "s%d" % n_spl
                n_spl = n_spl + 1
                q.add_node(sid, PASSTHROUGH, 500, role="splitter")
                q.nodes[sid].role = "splitter"
                q.add_edge(prefix + "e_" + parent + "_" + sid, parent, sid, sport=sport, tport="input_1")
                nxt.append(sid)
                noise.append(sid)
        levels.append(nxt)
    n_c = 0
    for parent in levels[-1]:
        for k, sport in enumerate(SPLITTER_OUT_PORTS):
            cid = prefix + "c%d" % n_c
            n_c = n_c + 1
            q.add_node(cid, CONSUMER, 0, 10)
            q.add_edge(prefix + "ec_" + cid, parent, cid, sport=sport)
            noise.append(cid)
    q.noise_ids = noise
    q.noise_sw = sw
    q.noise_g = g
    return q


def pulse_noise_tree(q, drain=False):
    sw = q.noise_sw
    if not hasattr(q, "pulse_calls"):
        q.pulse_calls = []
    q.entities[sw].on = not q.entities[sw].on
    q.nodes[sw].live_gate_open = q.entities[sw].on
    q.mark(sw, DIRTY_INTERNAL)
    noise_pending = sum(1 for n in q.queue if n in q.noise_ids)
    q.last_noise_pending = noise_pending
    if not drain:
        return []
    noise_n = []
    calls = 0
    while calls < 16:
        rem = q.pdq_tick(warmup=False, advance_clock=False)
        got = sum(1 for n in q.tick_processed if n in q.noise_ids)
        if got:
            noise_n.append(got)
        q.pulse_calls.append((list(q.tick_processed), list(q.queue)))
        calls = calls + 1
        if rem <= 0:
            break
    return noise_n


def assert_noise_budget(test, got, where):
    # Base: the tree is in overload (50 < 810), every allocation stays at 0 and a
    # toggle dies after a few nodes; only the signal bit carries it further.
    if not has_signal_fix():
        _ANNOTATED.append("ANOTADO cruce del ruido en la base %s nodos=%s" % (where, got))
        return
    total = sum(got)
    test.assertGreaterEqual(total, 65, "%s noise nodes=%s calls=%s" % (where, got, len(got)))
    test.assertGreaterEqual(len(got), 2, "%s pdq calls=%s got=%s" % (where, len(got), got))


def circuit_c(fillers=0):
    """MemoryCell Toggle/Set/Reset via LFPG_PushButton, built before settle."""
    h = helpers()
    scripted = ["G", "spl", "spl2", "swP", "btnT", "btnS", "btnR", "cell", "lamp"]
    q = SignalModel(h, order="scripted", scripted=scripted)
    q.add_node("G", SOURCE, 50)
    q.nodes["G"].m_Powered = True
    add_splitter(q, "spl")
    q.add_edge("e_gs", "G", "spl", tport="input_1")
    q.add_node("swP", PASSTHROUGH, 50, gated=True)
    q.add_edge("e_sp", "spl", "swP", sport=PORT_OUT1)
    add_splitter(q, "spl2")
    q.add_edge("e_s2", "spl", "spl2", sport=PORT_OUT2, tport="input_1")
    q.add_node("btnT", PASSTHROUGH, 50, gated=True)
    q.add_node("btnS", PASSTHROUGH, 50, gated=True)
    q.add_node("btnR", PASSTHROUGH, 50, gated=True)
    q.add_node("cell", PASSTHROUGH, 100, cons=1.0)
    q.add_node("lamp", CONSUMER, 0, 10)
    q.add_edge("e_pc", "swP", "cell", tport=PORT_IN0)
    q.add_edge("e_st", "spl2", "btnT", sport=PORT_OUT1)
    q.add_edge("e_ss", "spl2", "btnS", sport=PORT_OUT2)
    q.add_edge("e_sr", "spl2", "btnR", sport=PORT_OUT3)
    q.add_edge("e_tc", "btnT", "cell", tport=PORT_IN1)
    q.add_edge("e_sc", "btnS", "cell", tport=PORT_IN3)
    q.add_edge("e_rc", "btnR", "cell", tport=PORT_IN2)
    q.add_edge("e_out", "cell", "lamp", sport=PORT_OUT0)
    q.attach_switch("swP", on=True)
    q.attach_pushbutton("btnT")
    q.attach_pushbutton("btnS")
    q.attach_pushbutton("btnR")
    q.attach_cell("cell")
    # LFPG_MemoryCell.c:573 — 30 s grace after load.
    grace = const_int(CELL_SRC.read_text(encoding="utf-8"), "LFPG_MCELL_TOGGLE_LOAD_GRACE_MS")
    q.entities["cell"].m_ToggleGraceUntilMs = q.clock.now_ms + grace
    seeds = ["G", "spl", "spl2", "swP", "btnT", "btnS", "btnR", "cell", "lamp"]
    q.settle(seeds, require_empty=False)
    return q


def press_push(q, nid, pulse_ms=2000, gap_ms=3000):
    """2000 ms pulse (timer), then 3 s between presses (§6.4)."""
    q.entities[nid].press()
    q.run_events(pulse_ms)
    q.run_events(gap_ms)


def set_laser(q, on):
    q.entities["laser"].on = on
    q.nodes["laser"].live_gate_open = on
    q.mark("laser", DIRTY_INTERNAL)
    q.settle(require_empty=False, ticks=WAIT_COMBOS)


def gate_open(q):
    return q.entities["gate"].m_GateOpen


def lamp_power(q, eid="e_gl"):
    if eid not in q.edges:
        return 0.0
    return q.edges[eid].m_AllocatedPower


class _TraceCase(unittest.TestCase):
    def setUp(self):
        global CURRENT_TEST
        CURRENT_TEST = self.id()
        tlog("BEGIN %s" % CURRENT_TEST)

    def tearDown(self):
        tlog("END %s" % CURRENT_TEST)


class TestASplit(_TraceCase):
    def _assert_table_rows(self, kind, rows):
        for a_on, b_on, latch, lamp, changes in rows:
            want = table_bit(kind, a_on, b_on)
            msg = (
                "%s combo %s%s latch=%s lamp=%s changes=%s want_latch=%s"
                % (kind, int(a_on), int(b_on), latch, lamp, changes, want)
            )
            self.assertEqual(latch, want, msg)
            if want:
                self.assertGreaterEqual(lamp, 10.0 - EPS, msg)
            else:
                self.assertLess(lamp, 10.0, msg)
            self.assertLessEqual(changes, 1, msg)

    def _walk_orders(self, kind, splitter):
        q, rows = walk_table(kind, splitter, order="scripted")
        self._assert_table_rows(kind, rows)
        if not has_signal_fix():
            return
        for order in ("fifo", "lifo"):
            qq, extra = walk_table(kind, splitter, order=order)
            self._assert_table_rows(kind, extra)
        for i in range(20):
            qq, extra = walk_table(kind, splitter, order="perm", rng_seed=20261109 + i)
            self._assert_table_rows(kind, extra)

    def test_a1_and_splitter_11(self):
        self._walk_orders("AND", True)

    def test_a2_or_splitter_10(self):
        q = circuit_triple()
        _and_rows, or_rows, _xor_rows = run_bank(q)
        if has_signal_fix():
            self._assert_table_rows("OR", or_rows)
        else:
            # Base: annotated, not gated (plan v4). The bench measured OR 0001 with the AND
            # oscillating in the same queue, but this model does not reproduce it. Hypothesis
            # GL-06, measured in game in phase 3 with traces.
            bits, parts = _fmt_rows(or_rows)
            _ANNOTATED.append(
                "ANOTADO OR con vecino AND (partida 0001; hipotesis GL-06, fase 3) bits=%s %s"
                % (bits, "; ".join(parts))
            )
        qq, solo = walk_table("OR", True)
        bits, parts = _fmt_rows(solo)
        _ANNOTATED.append("ANOTADO OR solo bits=%s %s" % (bits, "; ".join(parts)))
        if has_signal_fix():
            self._walk_orders("OR", True)

    def test_a3_xor_splitter_10(self):
        q = circuit_triple()
        _and_rows, _or_rows, xor_rows = run_bank(q)
        self._assert_table_rows("XOR", xor_rows)
        qq, solo = walk_table("XOR", True)
        bits, parts = _fmt_rows(solo)
        _ANNOTATED.append("ANOTADO XOR solo bits=%s %s" % (bits, "; ".join(parts)))
        if has_signal_fix():
            self._walk_orders("XOR", True)
        # §6.6 F same-epoch: convergencia en (a) ORDER_A_SPLIT. XOR 10 sin carga:
        # apagar A no mueve alloc (ya 0); solo m_SignalChanged rebobina la puerta.
        q_a = circuit_a("XOR", True, False, splitter=True, load=False, require_empty=False)
        q_a.entities["swA"].on = False
        q_a.nodes["swA"].live_gate_open = False
        q_a.mark("swA", DIRTY_INTERNAL)
        q_a.pdq_tick(warmup=False, advance_clock=False)
        if has_signal_fix():
            self.assertFalse(
                gate_open(q_a),
                "convergencia (a) XOR 10->00 en la misma PDQ latch=%s processed=%s"
                % (gate_open(q_a), q_a.tick_processed),
            )

    def test_a1_a3_no_splitter_green(self):
        for kind in ("AND", "OR", "XOR"):
            self._walk_orders(kind, False)


class TestA4(_TraceCase):
    def _oscillates(self, splitter):
        q = circuit_a("XOR", True, False, splitter=splitter, load=False, require_empty=False)
        changes = q.latch_changes.get("gate", 0)
        last = gate_open(q)
        for _i in range(6):
            q.mark("gate", DIRTY_INPUT)
            q.mark("swA", DIRTY_INPUT)
            q.settle(require_empty=False)
            changes = changes + q.latch_changes.get("gate", 0)
        return q, changes, gate_open(q)

    def test_a4_with_splitter_must_oscillate_on_base(self):
        q, changes, last = self._oscillates(True)
        if (not has_signal_fix()) and q.latch_changes.get("gate", 0) <= 1:
            self.fail("A4 must oscillate on this base; model incomplete")
        self.assertLessEqual(q.latch_changes.get("gate", 0), 1)
        self.assertTrue(gate_open(q))
        if has_signal_fix():
            self.assertAlmostEqual(q.edges["e_a0"].m_AllocatedPower, 0.0, places=3)
            self.assertTrue(q.edges["e_a0"].m_SignalHigh)

    def test_a4_without_splitter_must_oscillate_on_base(self):
        q, changes, last = self._oscillates(False)
        if (not has_signal_fix()) and q.latch_changes.get("gate", 0) <= 1:
            self.fail("A4 without Splitter must oscillate on this base; model incomplete")
        self.assertLessEqual(q.latch_changes.get("gate", 0), 1)
        self.assertTrue(gate_open(q))


class TestB1(_TraceCase):
    def test_b1_xor_door_splitter(self):
        q = circuit_b1(False, False)
        steps = [(True, False), (True, True), (False, True), (False, False)]
        for a_on, b_on in steps:
            set_switch(q, "swA", a_on, require_empty=False)
            set_switch(q, "swB", b_on, require_empty=False)
            before = q.latch_changes.get("gate", 0)
            wait_combos(q)
            want = table_bit("XOR", a_on, b_on)
            latch = bool(gate_open(q))
            door = q.edges["e_gd"].m_AllocatedPower
            changes = q.latch_changes.get("gate", 0) - before
            msg = "B1 XOR(%s,%s) latch=%s door=%s changes=%s want=%s" % (
                int(a_on),
                int(b_on),
                latch,
                door,
                changes,
                want,
            )
            if not has_signal_fix():
                # Base: annotated, not gated (plan v4). In the bench the door opened late with
                # no oscillating neighbour; this model has no door delay. Measured in phase 3.
                _ANNOTATED.append("ANOTADO B1 (partida fallo solo, sin vecino; fase 3) " + msg)
                continue
            self.assertEqual(latch, want, msg)
            if want:
                self.assertGreaterEqual(door, 1.0 - EPS, msg)
            else:
                self.assertLess(door, 1.0, msg)
            self.assertLessEqual(changes, 1, msg)
            if has_signal_fix() and a_on and not b_on:
                # Power vs signal: door load keeps them aligned; force alloc 0
                # with the bit still high so B1 reads XOR(1,0) only by signal.
                q.edges["e_a0"].m_AllocatedPower = 0.0
                q.edges["e_b1"].m_AllocatedPower = 0.0
                q.process("gate")
                self.assertTrue(
                    gate_open(q),
                    "B1 XOR(1,0) latch por senal con alloc 0",
                )
        if has_signal_fix():
            self.assertFalse(gate_open(q), "B1 tras B off XOR(0,0) latch cerrado")


class TestD5(_TraceCase):
    def _assert_d5_set_path(self, q, expect_latch, label):
        set_laser(q, True)
        latch = q.entities["cell"].m_LatchState
        if expect_latch:
            self.assertTrue(latch, "%s Set high when Laser opens" % label)
            self.assertTrue(q.entities["cell"].m_CellActive, label)
            self.assertGreaterEqual(q.edges["e_out"].m_AllocatedPower, 10.0 - EPS, label)
            set_laser(q, False)
            self.assertTrue(q.entities["cell"].m_LatchState, "%s latch holds when Laser closes" % label)
        else:
            self.assertFalse(latch, "%s base: Set no asienta (rojo)" % label)

    def test_d5_power_via_splitter(self):
        q = circuit_d5(True, False, True)
        self._assert_d5_set_path(q, True, "D5 splitter")

    def test_d5_power_direct(self):
        q = circuit_d5(False, False, True)
        self._assert_d5_set_path(q, True, "D5 directo")

    def test_d5_laser_switch_set(self):
        q = circuit_d5(True, True, True)
        self._assert_d5_set_path(q, True, "D5 laser-switch")

    def test_d5_no_power_green(self):
        q = circuit_d5(True, True, False)
        set_laser(q, True)
        cell = q.entities["cell"]
        # In-game control: Laser alone sets the cell and lights the lamp
        # (gate 1, set 1, active 1). Set is the only feed, so inputSum
        # covers cons + lamp.
        self.assertTrue(cell.m_LatchState, "D5 no-Power latch")
        self.assertTrue(cell.m_CellActive, "D5 no-Power active")
        self.assertGreaterEqual(q.edges["e_out"].m_AllocatedPower, 10.0 - EPS, "D5 no-Power lamp")


class TestCSplitter(_TraceCase):
    def test_c_toggle_set_reset_green(self):
        q = circuit_c()
        cell = q.entities["cell"]
        self.assertFalse(cell.m_LatchState)
        q.run_events(30000)
        q.run_events(1400)
        expected = False
        for i in range(3):
            press_push(q, "btnT")
            expected = not expected
            self.assertEqual(
                cell.m_LatchState,
                expected,
                "C toggle press %s latch=%s want=%s" % (i + 1, cell.m_LatchState, expected),
            )
        press_push(q, "btnS")
        self.assertTrue(cell.m_LatchState, "C Set latch=%s" % cell.m_LatchState)
        press_push(q, "btnR")
        self.assertFalse(cell.m_LatchState, "C Reset latch=%s" % cell.m_LatchState)

    def test_c_congested_green(self):
        q = circuit_c()
        attach_noise_tree(q)
        q.mark(q.noise_g, DIRTY_INTERNAL)
        q.settle(require_empty=False)
        cell = q.entities["cell"]
        q.run_events(30000)
        q.run_events(1400)
        expected = False
        for i in range(3):
            pulse_noise_tree(q, drain=False)
            self.assertGreater(
                q.last_noise_pending,
                0,
                "C congested press %s noise pending=0" % (i + 1,),
            )
            press_push(q, "btnT")
            expected = not expected
            self.assertEqual(
                cell.m_LatchState,
                expected,
                "C congested press %s latch=%s want=%s" % (i + 1, cell.m_LatchState, expected),
            )
        pulse_noise_tree(q, drain=False)
        self.assertGreater(q.last_noise_pending, 0)
        press_push(q, "btnS")
        self.assertTrue(cell.m_LatchState)
        pulse_noise_tree(q, drain=False)
        self.assertGreater(q.last_noise_pending, 0)
        press_push(q, "btnR")
        self.assertFalse(cell.m_LatchState)
        pulse_noise_tree(q, drain=False)
        q.entities["swP"].on = False
        q.nodes["swP"].live_gate_open = False
        q.mark("swP", DIRTY_INTERNAL)
        q.settle(require_empty=False)
        q.entities["swP"].on = True
        q.nodes["swP"].live_gate_open = True
        q.mark("swP", DIRTY_INTERNAL)
        q.settle(require_empty=False)
        pulse_noise_tree(q, drain=False)
        self.assertGreater(q.last_noise_pending, 0, "C7 ruido pendiente")
        q.settle(["G", "spl", "swP", "btnT", "btnS", "btnR", "cell", "lamp"], warmup=True, require_empty=False)
        pulse_noise_tree(q, drain=False)
        self.assertGreater(q.last_noise_pending, 0, "C7 Set con congestion")
        press_push(q, "btnS")
        self.assertTrue(cell.m_LatchState, "C7 Set held latch")
        q.entities["btnS"].m_SwitchOn = True
        q.nodes["btnS"].live_gate_open = True
        q.mark("btnS", DIRTY_INTERNAL)
        q.settle(require_empty=False)
        self.assertTrue(cell.m_LatchState, "C7 Set still held")


class TestAnnotated(_TraceCase):
    def test_transitions_annotated(self):
        # 10->00: XOR was open at 10; both inputs 0 in one SetPowered pass
        # (brownout_back holds the latch open).
        q10 = circuit_a("XOR", True, False, splitter=True, require_empty=False)
        q10.entities["swA"].on = False
        q10.nodes["swA"].live_gate_open = False
        q10.entities["swB"].on = False
        q10.nodes["swB"].live_gate_open = False
        q10.mark("swA", DIRTY_INTERNAL)
        q10.mark("swB", DIRTY_INTERNAL)
        q10.process("swA")
        q10.process("swB")
        q10.process("gate")
        _ANNOTATED.append("ANOTADO transiciones 10->00 latch=%s" % gate_open(q10))
        q = circuit_a("AND", True, True, splitter=True, require_empty=False)
        q.add_node("Gp", SOURCE, 50)
        q.nodes["Gp"].m_Powered = True
        q.add_node("cell", PASSTHROUGH, 100, cons=1.0)
        q.add_edge("e_pc", "Gp", "cell", tport=PORT_IN0)
        q.add_edge("e_set", "gate", "cell", tport=PORT_IN3)
        q.attach_cell("cell")
        q.mark("gate", DIRTY_TOPOLOGY)
        q.mark("cell", DIRTY_TOPOLOGY)
        q.settle(require_empty=False)
        if has_signal_fix():
            q.entities["cell"].m_LatchState = False
            q.entities["cell"].m_CellActive = False
        # 11->00 in the same epoch so SetPowered sees both inputs 0 while open.
        q.entities["swA"].on = False
        q.nodes["swA"].live_gate_open = False
        q.entities["swB"].on = False
        q.nodes["swB"].live_gate_open = False
        q.mark("swA", DIRTY_INTERNAL)
        q.mark("swB", DIRTY_INTERNAL)
        q.settle(require_empty=False)
        set_switch(q, "swA", True, require_empty=False)
        _ANNOTATED.append(
            "ANOTADO transiciones 11->00->10 latch=%s cell=%s"
            % (gate_open(q), q.entities["cell"].m_LatchState)
        )
        if has_signal_fix():
            self.assertFalse(gate_open(q10), "10->00 latch=tabla")
            self.assertFalse(gate_open(q), "11->00->10 AND 10 latch=tabla")
            self.assertFalse(q.entities["cell"].m_LatchState, "celda no se activa en 11->00->10")
            self.assertEqual(g2_compare(q10), [])

    def test_rehab_output_annotated(self):
        q = circuit_d5(True, False, True)
        q.set_output_port_enabled("cell", PORT_OUT0, False)
        for e in q.outgoing["cell"]:
            if e.m_SourcePort == PORT_OUT0:
                e.m_SignalHigh = True
        msg = "ANOTADO salida rehabilitada flags=%s signal=%s" % (
            [(e.m_Flags, e.m_SignalHigh) for e in q.outgoing["cell"]],
            True,
        )
        _ANNOTATED.append(msg)
        q.mark("lamp", DIRTY_INPUT)
        q.process("lamp")
        if has_signal_fix():
            self.assertFalse(q.last_signal_in.get("lamp"), "Output deshabilitada: signalIn aguas abajo falso")
        q.set_output_port_enabled("cell", PORT_OUT0, True)
        if has_signal_fix():
            for e in q.outgoing["cell"]:
                if e.m_SourcePort == PORT_OUT0:
                    self.assertFalse(e.m_SignalHigh)

    def test_overload_annotated(self):
        h = helpers()
        q = SignalModel(h, scripted=["G", "spl", "swA", "swB", "gate", "lamp", "hog"])
        q.add_node("G", SOURCE, 50)
        q.nodes["G"].m_Powered = True
        add_splitter(q, "spl")
        q.add_node("swA", PASSTHROUGH, 50, gated=True)
        q.add_node("swB", PASSTHROUGH, 50, gated=True)
        q.add_node("gate", PASSTHROUGH, 100, gated=True)
        q.add_node("lamp", CONSUMER, 0, 10)
        q.add_node("hog", CONSUMER, 0, 60)
        q.add_edge("e_gs", "G", "spl", tport="input_1")
        q.add_edge("e_sa", "spl", "swA", sport=PORT_OUT1)
        q.add_edge("e_sb", "spl", "swB", sport=PORT_OUT2)
        q.add_edge("e_sh", "spl", "hog", sport=PORT_OUT3)
        q.add_edge("e_a0", "swA", "gate", tport=PORT_IN0)
        q.add_edge("e_b1", "swB", "gate", tport=PORT_IN1)
        q.add_edge("e_gl", "gate", "lamp")
        q.attach_switch("swA", True)
        q.attach_switch("swB", False)
        q.attach_gate("gate", "XOR")
        q.settle(["G", "spl", "swA", "swB", "gate", "lamp", "hog"], require_empty=False)
        msg = "ANOTADO sobrecarga overloaded=%s latch=%s alloc_lamp=%s" % (
            q.nodes["G"].m_Overloaded or q.nodes["spl"].m_Overloaded,
            gate_open(q),
            lamp_power(q),
        )
        _ANNOTATED.append(msg)
        if has_signal_fix():
            for e in q.edges.values():
                self.assertAlmostEqual(e.m_AllocatedPower, 0.0, places=3)
            self.assertTrue(q.edges["e_a0"].m_SignalHigh)
            self.assertLessEqual(q.latch_changes.get("gate", 0), 1)

    def test_cut_annotated(self):
        q = circuit_a("XOR", True, False, splitter=True)
        q.nodes["G"].m_Powered = False
        q.mark("G", DIRTY_INTERNAL)
        q.settle(require_empty=False)
        if not getattr(q, "settled_empty", True):
            _ANNOTATED.append("ANOTADO corte no asienta en 100 ticks")
        else:
            _ANNOTATED.append(
                "ANOTADO corte latch=%s alloc=%s" % (gate_open(q), lamp_power(q))
            )
        if has_signal_fix():
            self.assertFalse(gate_open(q))
            self.assertAlmostEqual(lamp_power(q), 0.0, places=3)
            for e in q.edges.values():
                if (e.m_Flags & EDGE_ENABLED) != 0:
                    self.assertAlmostEqual(e.m_AllocatedPower, 0.0, places=3)
                    self.assertFalse(e.m_SignalHigh)

    def test_long_chain_annotated(self):
        h = helpers()
        q = SignalModel(h, order="fifo", scripted=[])
        q.add_node("G", SOURCE, 50)
        q.nodes["G"].m_Powered = True
        prev = "G"
        for i in range(64):
            nid = "s%d" % i
            add_splitter(q, nid)
            if prev == "G":
                q.add_edge("e%d" % i, prev, nid, tport="input_1")
            else:
                q.add_edge("e%d" % i, prev, nid, sport=PORT_OUT1, tport="input_1")
            prev = nid
        q.add_node("sw", PASSTHROUGH, 50, gated=True)
        q.add_node("gate", PASSTHROUGH, 100, gated=True)
        q.add_edge("e_sw", prev, "sw", sport=PORT_OUT1)
        q.add_edge("e_g", "sw", "gate", tport=PORT_IN0)
        q.attach_switch("sw", True)
        q.attach_gate("gate", "XOR")
        ticks = q.settle(["G"], ticks=MAX_TICKS, warmup=True, require_empty=False)
        q.nodes["G"].m_Powered = False
        q.mark("G")
        later = q.settle(ticks=MAX_TICKS, require_empty=False)
        if not getattr(q, "settled_empty", True):
            _ANNOTATED.append("ANOTADO cadena no asienta en 100 ticks")
        else:
            _ANNOTATED.append("ANOTADO cadena ticks_off=%s latch=%s" % (later, gate_open(q)))
        if has_signal_fix():
            self.assertFalse(gate_open(q))
            self.assertEqual(g2_compare(q), [])

    def test_boot_annotated(self):
        q = circuit_a("XOR", True, False, splitter=True, require_empty=False)
        _ANNOTATED.append("ANOTADO arranque latch=%s" % gate_open(q))
        if has_signal_fix():
            self.assertTrue(gate_open(q))
            self.assertLessEqual(q.latch_changes.get("gate", 0), 1)
            self.assertEqual(g2_compare(q), [])

    def test_invariant_annotated(self):
        q = circuit_a("AND", True, True, splitter=True, require_empty=False)
        if not getattr(q, "settled_empty", True):
            _ANNOTATED.append("ANOTADO invariante no asienta en 100 ticks")
            if has_signal_fix():
                self.fail("invariante: cola no vacia donde la fila pide vaciado")
            return
        ok = q.invariant_ok()
        _ANNOTATED.append("ANOTADO invariante ok=%s has_fix=%s" % (ok, has_signal_fix()))
        if has_signal_fix():
            self.assertTrue(ok)
            self.assertEqual(g2_compare(q), [])

    def test_old_latch_annotated(self):
        h = helpers()
        q = SignalModel(h, order="scripted", scripted=["swB", "gate", "cell"])
        q.add_node("G", SOURCE, 50)
        q.nodes["G"].m_Powered = True
        add_splitter(q, "spl")
        q.add_node("swA", PASSTHROUGH, 50, gated=True)
        q.add_node("swB", PASSTHROUGH, 50, gated=True)
        q.add_node("gate", PASSTHROUGH, 100, gated=True)
        q.add_node("Gp", SOURCE, 50)
        q.nodes["Gp"].m_Powered = True
        q.add_node("cell", PASSTHROUGH, 100, cons=1.0)
        q.add_edge("e_gs", "G", "spl", tport="input_1")
        q.add_edge("e_sa", "spl", "swA", sport=PORT_OUT1)
        q.add_edge("e_sb", "spl", "swB", sport=PORT_OUT2)
        q.add_edge("e_a0", "swA", "gate", tport=PORT_IN0)
        q.add_edge("e_b1", "swB", "gate", tport=PORT_IN1)
        q.add_edge("e_pc", "Gp", "cell", tport=PORT_IN0)
        q.attach_switch("swA", True)
        q.attach_switch("swB", True)
        q.attach_gate("gate", "AND")
        q.attach_cell("cell")
        q.settle(list(q.nodes.keys()), require_empty=False)
        q.entities["swB"].on = False
        q.nodes["swB"].live_gate_open = False
        q.mark("swB", DIRTY_INTERNAL)
        q.add_edge("e_set", "gate", "cell", tport=PORT_IN3)
        q.mark("gate", DIRTY_TOPOLOGY)
        q.mark("cell", DIRTY_TOPOLOGY)
        q.settle(require_empty=False)
        _ANNOTATED.append(
            "ANOTADO latch viejo AND=%s cell=%s set_alloc=%s"
            % (
                gate_open(q),
                q.entities["cell"].m_LatchState,
                q.edges["e_set"].m_AllocatedPower if "e_set" in q.edges else None,
            )
        )
        def _race_order(order, seed):
            h = helpers()
            qm = SignalModel(h, order=order, scripted=["gate", "cell", "swB"])
            qm.rng = random.Random(seed)
            qm.add_node("G", SOURCE, 50)
            qm.nodes["G"].m_Powered = True
            add_splitter(qm, "spl")
            qm.add_node("swA", PASSTHROUGH, 50, gated=True)
            qm.add_node("swB", PASSTHROUGH, 50, gated=True)
            qm.add_node("gate", PASSTHROUGH, 100, gated=True)
            qm.add_node("Gp", SOURCE, 50)
            qm.nodes["Gp"].m_Powered = True
            qm.add_node("cell", PASSTHROUGH, 100, cons=1.0)
            qm.add_edge("e_gs", "G", "spl", tport="input_1")
            qm.add_edge("e_sa", "spl", "swA", sport=PORT_OUT1)
            qm.add_edge("e_sb", "spl", "swB", sport=PORT_OUT2)
            qm.add_edge("e_a0", "swA", "gate", tport=PORT_IN0)
            qm.add_edge("e_b1", "swB", "gate", tport=PORT_IN1)
            qm.add_edge("e_pc", "Gp", "cell", tport=PORT_IN0)
            qm.attach_switch("swA", True)
            qm.attach_switch("swB", True)
            qm.attach_gate("gate", "AND")
            qm.attach_cell("cell")
            qm.settle(list(qm.nodes.keys()), require_empty=has_signal_fix())
            qm.entities["swB"].on = False
            qm.nodes["swB"].live_gate_open = False
            qm.mark("swB", DIRTY_INTERNAL)
            # §6.4 frontera L-RACE: vaciar entre B off y el cable Set.
            qm.settle(require_empty=has_signal_fix())
            qm.add_edge("e_set", "gate", "cell", tport=PORT_IN3)
            qm.mark("gate", DIRTY_TOPOLOGY)
            qm.mark("cell", DIRTY_TOPOLOGY)
            qm.settle(require_empty=has_signal_fix())
            _ANNOTATED.append(
                "ANOTADO latch viejo traza orden=%s processed=%s AND=%s cell=%s e_set=%s"
                % (
                    order,
                    list(qm.tick_processed),
                    gate_open(qm),
                    qm.entities["cell"].m_LatchState,
                    qm.edges["e_set"].m_SignalHigh,
                )
            )
            return qm

        for mode in ("fifo", "lifo"):
            qm = _race_order(mode, 20261109)
            if has_signal_fix():
                self.assertFalse(gate_open(qm), "latch viejo orden %s" % mode)
                self.assertFalse(qm.entities["cell"].m_LatchState)
            _ANNOTATED.append("ANOTADO latch viejo orden %s latch=%s" % (mode, gate_open(qm)))
        for i in range(20):
            qm = _race_order("perm", 20261109 + i)
            if has_signal_fix():
                self.assertFalse(gate_open(qm), "perm %s" % i)
                self.assertFalse(qm.entities["cell"].m_LatchState)
        if has_signal_fix():
            self.assertFalse(gate_open(q), "latch viejo (a) AND cerrado")
            self.assertFalse(q.edges["e_set"].m_SignalHigh)
            self.assertFalse(q.entities["cell"].m_LatchState)
            self.assertEqual(g2_compare(q), [])

    def test_old_latch_no_new_wire_annotated(self):
        h = helpers()
        q = SignalModel(h, order="scripted", scripted=["swB", "gate", "swM", "cell"])
        q.add_node("G", SOURCE, 50)
        q.nodes["G"].m_Powered = True
        add_splitter(q, "spl")
        q.add_node("swA", PASSTHROUGH, 50, gated=True)
        q.add_node("swB", PASSTHROUGH, 50, gated=True)
        q.add_node("gate", PASSTHROUGH, 100, gated=True)
        q.add_node("swM", PASSTHROUGH, 50, gated=True)
        q.add_node("Gp", SOURCE, 50)
        q.nodes["Gp"].m_Powered = True
        q.add_node("cell", PASSTHROUGH, 100, cons=1.0)
        q.add_edge("e_gs", "G", "spl", tport="input_1")
        q.add_edge("e_sa", "spl", "swA", sport=PORT_OUT1)
        q.add_edge("e_sb", "spl", "swB", sport=PORT_OUT2)
        q.add_edge("e_a0", "swA", "gate", tport=PORT_IN0)
        q.add_edge("e_b1", "swB", "gate", tport=PORT_IN1)
        q.add_edge("e_gm", "gate", "swM")
        q.add_edge("e_pc", "Gp", "cell", tport=PORT_IN0)
        q.add_edge("e_set", "swM", "cell", tport=PORT_IN3)
        q.attach_switch("swA", True)
        q.attach_switch("swB", True)
        q.attach_switch("swM", False)
        q.attach_gate("gate", "AND")
        q.attach_cell("cell")
        q.settle(list(q.nodes.keys()), require_empty=False)
        q.entities["swB"].on = False
        q.nodes["swB"].live_gate_open = False
        q.entities["swM"].on = True
        q.nodes["swM"].live_gate_open = True
        q.mark("swB", DIRTY_INTERNAL)
        q.mark("swM", DIRTY_INTERNAL)
        q.settle(require_empty=False)
        _ANNOTATED.append(
            "ANOTADO latch viejo sin cable AND=%s M=%s set=%s"
            % (gate_open(q), q.entities["cell"].m_LatchState, q.edges["e_set"].m_SignalHigh)
        )
        if has_signal_fix():
            self.assertFalse(q.entities["cell"].m_LatchState, "topologico: M desactivada")
            self.assertFalse(q.edges["e_set"].m_SignalHigh, "ninguna lectura alta en Set")

    def test_q_to_toggle_annotated(self):
        h = helpers()
        q = SignalModel(h, order="fifo", scripted=[])
        q.add_node("Ga", SOURCE, 50)
        q.nodes["Ga"].m_Powered = True
        q.add_node("Gb", SOURCE, 50)
        q.nodes["Gb"].m_Powered = True
        q.add_node("swS", PASSTHROUGH, 50, gated=True)
        q.add_node("cellA", PASSTHROUGH, 100, cons=1.0)
        q.add_node("cellB", PASSTHROUGH, 100, cons=1.0)
        q.add_edge("e_pa", "Ga", "cellA", tport=PORT_IN0)
        q.add_edge("e_ss", "Ga", "swS")
        q.add_edge("e_seta", "swS", "cellA", tport=PORT_IN3)
        q.add_edge("e_pb", "Gb", "cellB", tport=PORT_IN0)
        q.add_edge("e_q", "cellA", "cellB", sport=PORT_OUT0, tport=PORT_IN1)
        q.attach_switch("swS", False)
        q.attach_cell("cellA")
        q.attach_cell("cellB")
        q.entities["cellA"].m_ToggleGraceUntilMs = 0
        q.entities["cellB"].m_ToggleGraceUntilMs = 0
        q.settle(["Ga", "Gb", "swS", "cellA", "cellB"], require_empty=False)
        q.run_events(1000)
        q.entities["cellB"].m_ToggleLowSinceMs = max(0, q.clock.now_ms - 1000)
        q.entities["cellB"].m_SettledToggle = False
        q.entities["cellB"].m_RawToggle = False
        before = q.entities["cellB"].m_LatchState
        flips0 = q.latch_changes.get("cellB", 0)
        set_switch(q, "swS", True, require_empty=False)
        q.run_events(1000)
        after_on = q.entities["cellB"].m_LatchState
        flips_on = q.latch_changes.get("cellB", 0) - flips0
        set_switch(q, "swS", False, require_empty=False)
        q.run_events(1000)
        after_off = q.entities["cellB"].m_LatchState
        flips_off = q.latch_changes.get("cellB", 0) - flips0
        pdq_ms = [t[1] for t in q.clock.trace if t[0] == "PDQ"]
        later_due = []
        for t in q.clock.trace:
            if t[0] != "CallLater":
                continue
            due = t[3] if len(t) > 3 else t[1] + t[2]
            if t[2] == 50:
                later_due.append(due)
        between = False
        for lm in later_due:
            if any(pdq_ms[i] < lm < pdq_ms[i + 1] for i in range(len(pdq_ms) - 1)):
                between = True
        _ANNOTATED.append(
            "ANOTADO Q->Toggle before=%s on=%s off=%s flips_on=%s flips_off=%s between=%s now=%s"
            % (before, after_on, after_off, flips_on, flips_off, between, q.clock.now_ms)
        )
        if has_signal_fix():
            self.assertNotEqual(after_on, before, "A off->on: B voltea")
            self.assertEqual(flips_on, 1, "A off->on: B voltea exactamente una vez")
            self.assertEqual(after_off, after_on, "A on->off: B no voltea")
            self.assertEqual(flips_off, 1, "A on->off: sin volteo extra")
            self.assertTrue(between, "routing 50 ms entre dos PDQ")

    def test_rehab_delayed_annotated(self):
        h = helpers()
        q = SignalModel(h, order="fifo", scripted=[])
        q.add_node("Ga", SOURCE, 50)
        q.nodes["Ga"].m_Powered = True
        q.add_node("Gb", SOURCE, 50)
        q.nodes["Gb"].m_Powered = True
        q.add_node("swS", PASSTHROUGH, 50, gated=True)
        q.add_node("swR", PASSTHROUGH, 50, gated=True)
        q.add_node("cellA", PASSTHROUGH, 100, cons=1.0)
        q.add_node("cellB", PASSTHROUGH, 100, cons=1.0)
        q.add_edge("e_pa", "Ga", "cellA", tport=PORT_IN0)
        q.add_edge("e_ss", "Ga", "swS")
        q.add_edge("e_sr", "Ga", "swR")
        q.add_edge("e_seta", "swS", "cellA", tport=PORT_IN3)
        q.add_edge("e_ra", "swR", "cellA", tport=PORT_IN2)
        q.add_edge("e_pb", "Gb", "cellB", tport=PORT_IN0)
        q.add_edge("e_q", "cellA", "cellB", sport=PORT_OUT0, tport=PORT_IN1)
        q.attach_switch("swS", True)
        q.attach_switch("swR", False)
        q.attach_cell("cellA")
        q.attach_cell("cellB")
        q.entities["cellA"].m_ToggleGraceUntilMs = 0
        q.entities["cellB"].m_ToggleGraceUntilMs = 0
        q.settle(["Ga", "Gb", "swS", "swR", "cellA", "cellB"], require_empty=False)
        q.run_events(1400)
        q.entities["cellB"].m_ToggleLowSinceMs = max(0, q.clock.now_ms - 1000)
        q.entities["cellB"].m_SettledToggle = False
        q.entities["cellB"].m_RawToggle = False
        self.assertTrue(q.entities["cellA"].m_LatchState or not has_signal_fix())
        flips0 = q.latch_changes.get("cellB", 0)

        def _short(nid, on):
            q.entities[nid].on = on
            q.nodes[nid].live_gate_open = on
            q.mark(nid, DIRTY_INTERNAL)
            q.pdq_tick(warmup=False, advance_clock=False)

        _short("swS", False)
        _short("swR", True)
        q.run_events(100)
        _short("swR", False)
        q1 = q.entities["cellA"].m_LatchState
        disabled = [
            (e.m_Flags & EDGE_ENABLED) != 0
            for e in q.outgoing.get("cellA", [])
            if e.m_SourcePort == PORT_OUT0
        ]
        _short("swS", True)
        q.run_events(200)
        q2 = q.entities["cellA"].m_LatchState
        flips_i = q.latch_changes.get("cellB", 0) - flips0
        _ANNOTATED.append(
            "ANOTADO rehab delayed Q %s->%s->%s disabled=%s flips_i=%s now=%s latchB=%s step3=%s"
            % (
                True,
                q1,
                q2,
                disabled,
                flips_i,
                q.clock.now_ms,
                q.latch_changes.get("cellB", 0),
                [t[0] for t in q.step3_log[-12:]],
            )
        )
        if has_signal_fix():
            self.assertFalse(q1, "rehab delayed Q 1->0")
            self.assertTrue(q2, "rehab delayed Q 0->1")
            self.assertEqual(flips_i, 0, "carga normal: B no voltea")
        flips1 = q.latch_changes.get("cellB", 0)
        # (ii) Q 1->0 first so B sees Toggle low, then fill delays A's enable pass.
        _short("swS", False)
        _short("swR", True)
        q.run_events(100)
        _short("swR", False)
        q.settle(require_empty=False, ticks=20)
        t_low = q.clock.now_ms
        b = q.entities["cellB"]
        if has_signal_fix():
            self.assertFalse(
                q.port_receiving_signal("cellB", PORT_IN1),
                "B lee Toggle baja durante el retraso",
            )
        add_admissible_fill(q, None, 720, "fill")
        pending_before = len(q.queue)
        _short("swS", True)
        q.run_events(1100)
        low_ms = q.clock.now_ms - t_low
        b_toggle = q.port_receiving_signal("cellB", PORT_IN1)
        toggle_due = [p[0] - q.clock.now_ms for p in q.clock.pending if p[1] is b.cb_toggle]
        _ANNOTATED.append(
            "ANOTADO rehab (ii) traza low_ms=%s pending_fill=%s B_toggle=%s "
            "RawT=%s SettledT=%s LowSince=%s PowerSince=%s RawTchanged=%s "
            "now=%s toggle_timers=%s A_q=%s A_out0=%s"
            % (
                low_ms,
                pending_before,
                b_toggle,
                b.m_RawToggle,
                b.m_SettledToggle,
                b.m_ToggleLowSinceMs,
                b.m_PowerSinceMs,
                b.m_RawToggleChangedMs,
                q.clock.now_ms,
                toggle_due,
                q.entities["cellA"].m_LatchState,
                [
                    ((e.m_Flags & EDGE_ENABLED) != 0, e.m_SignalHigh)
                    for e in q.outgoing.get("cellA", [])
                    if e.m_SourcePort == PORT_OUT0
                ],
            )
        )
        if has_signal_fix():
            self.assertGreaterEqual(low_ms, 1100)
            # §6.4 (ii): Toggle of B read low for 1100 ms or more before it rises again.
            low_span = b.m_RawToggleChangedMs - b.m_ToggleLowSinceMs
            if not b_toggle:
                low_span = q.clock.now_ms - b.m_ToggleLowSinceMs
            self.assertGreaterEqual(
                low_span,
                1100,
                "A retrasada: B tiene que leer Toggle baja 1100 ms o mas (lectura=%s)" % low_span,
            )
        q.run_events(800)
        q.settle(require_empty=has_signal_fix(), ticks=200)
        flips_ii = q.latch_changes.get("cellB", 0) - flips1
        _ANNOTATED.append(
            "ANOTADO rehab delayed (ii) low_ms=%s flips=%s empty=%s LowSince=%s PowerSince=%s edge=%s"
            % (
                q.clock.now_ms - t_low,
                flips_ii,
                getattr(q, "settled_empty", None),
                b.m_ToggleLowSinceMs,
                b.m_PowerSinceMs,
                b.m_RawToggleChangedMs,
            )
        )
        if has_signal_fix():
            self.assertEqual(flips_ii, 1, "retraso 1100 ms: B voltea una vez")
            self.assertTrue(q.settled_empty, "rehab (ii) vaciado")

    def test_race_and_g2_annotated(self):
        h = helpers()
        q = SignalModel(h, order="scripted", scripted=["gate", "cell", "swB"])
        q.add_node("G", SOURCE, 50)
        q.nodes["G"].m_Powered = True
        add_splitter(q, "spl")
        q.add_node("swA", PASSTHROUGH, 50, gated=True)
        q.add_node("swB", PASSTHROUGH, 50, gated=True)
        q.add_node("gate", PASSTHROUGH, 100, gated=True)
        q.add_node("Gp", SOURCE, 50)
        q.nodes["Gp"].m_Powered = True
        q.add_node("cell", PASSTHROUGH, 100, cons=1.0)
        q.add_edge("e_gs", "G", "spl", tport="input_1")
        q.add_edge("e_sa", "spl", "swA", sport=PORT_OUT1)
        q.add_edge("e_sb", "spl", "swB", sport=PORT_OUT2)
        q.add_edge("e_a0", "swA", "gate", tport=PORT_IN0)
        q.add_edge("e_b1", "swB", "gate", tport=PORT_IN1)
        q.add_edge("e_pc", "Gp", "cell", tport=PORT_IN0)
        q.attach_switch("swA", True)
        q.attach_switch("swB", True)
        q.attach_gate("gate", "AND")
        q.attach_cell("cell")
        q.settle(list(q.nodes.keys()), require_empty=False)
        q.entities["swB"].on = False
        q.nodes["swB"].live_gate_open = False
        q.mark("swB", DIRTY_INTERNAL)
        q.add_edge("e_set", "gate", "cell", tport=PORT_IN3)
        q.mark("gate", DIRTY_TOPOLOGY)
        q.mark("cell", DIRTY_TOPOLOGY)
        q.pdq_tick(warmup=False, advance_clock=False)
        processed = list(q.tick_processed)
        if has_signal_fix():
            # [AND, M, B]: first pass of each in that order; the AND may run again after B.
            firsts = [processed.index(x) for x in ("gate", "cell", "swB")]
            self.assertEqual(firsts, sorted(firsts), "orden [AND,M,B] processed=%s" % processed)
            self.assertFalse(gate_open(q), "convergencia (a) [AND,M,B] en la misma llamada")
            self.assertFalse(q.edges["e_set"].m_SignalHigh, "Set baja en esa misma llamada")
            self.assertGreater(
                processed.count("gate"),
                1,
                "AND reprocesado en la misma PDQ tras B processed=%s" % processed,
            )
            self.assertEqual(g2_compare(q), [], "G2 en la misma llamada de PDQ")
        q.settle(require_empty=False)
        diffs = g2_compare(q)
        _ANNOTATED.append(
            "ANOTADO latch viejo carrera [AND,M,B] AND=%s M=%s g2_diffs=%s sweep=%s processed=%s"
            % (
                gate_open(q),
                q.entities["cell"].m_LatchState,
                diffs,
                q.validate_syncs,
                processed,
            )
        )
        if has_signal_fix():
            self.assertFalse(gate_open(q), "carrera [AND,M,B] AND cerrado al vaciar")
            self.assertFalse(q.edges["e_set"].m_SignalHigh)
            self.assertEqual(g2_compare(q), [])

    def test_empty_between_changes_annotated(self):
        q = circuit_a("AND", True, True, splitter=True, require_empty=False)
        set_switch(q, "swB", False, require_empty=False)
        q.settle(require_empty=False)
        q.add_node("Gp", SOURCE, 50)
        q.nodes["Gp"].m_Powered = True
        q.add_node("cell", PASSTHROUGH, 100, cons=1.0)
        q.add_edge("e_pc", "Gp", "cell", tport=PORT_IN0)
        q.add_edge("e_set", "gate", "cell", tport=PORT_IN3)
        q.attach_cell("cell")
        q.mark("gate", DIRTY_TOPOLOGY)
        q.settle(require_empty=False)
        _ANNOTATED.append(
            "ANOTADO cambios separados por vaciado M=%s g2=%s"
            % (q.entities["cell"].m_LatchState, g2_compare(q))
        )
        if has_signal_fix():
            self.assertFalse(q.entities["cell"].m_LatchState)
            self.assertEqual(g2_compare(q), [])

    def test_registry_and_unresolvable_annotated(self):
        q = circuit_a("AND", True, False, splitter=True, require_empty=False)
        q.add_node("Gp", SOURCE, 50)
        q.nodes["Gp"].m_Powered = True
        q.add_node("cell", PASSTHROUGH, 100, cons=1.0)
        q.add_edge("e_pc", "Gp", "cell", tport=PORT_IN0)
        q.add_edge("e_set", "gate", "cell", tport=PORT_IN3)
        q.attach_cell("cell")
        q.mark("gate", DIRTY_TOPOLOGY)
        q.mark("cell", DIRTY_TOPOLOGY)
        q.settle(require_empty=False)
        q.fail_find.add("gate")
        q.fail_vanilla.add("gate")
        set_switch(q, "swB", True, require_empty=False)
        _ANNOTATED.append(
            "ANOTADO registro sin entidad FindById fail latch=%s M=%s net=%s"
            % (gate_open(q), q.entities["cell"].m_LatchState, q.registry.get("gate"))
        )
        if has_signal_fix():
            self.assertTrue(gate_open(q), "10->11 FindById y vanilla fallan, NetworkID acierta")
            self.assertTrue(q.entities["cell"].m_LatchState, "M activada en esa llamada")
            self.assertTrue(q.registry.get("gate"), "registro obtenido por NetworkID")
        q.fail_find.add("gate")
        q.fail_vanilla.add("gate")
        q.fail_netid.add("gate")
        q.mark("gate", DIRTY_INTERNAL)
        q.settle(require_empty=False)
        _ANNOTATED.append(
            "ANOTADO entidad irresoluble latch=%s"
            % gate_open(q)
        )
        if has_signal_fix():
            self.assertFalse(q.edges["e_gl"].m_SignalHigh, "puerta irresoluble baja la senal")

    def test_noisy_neighbor_annotated(self):
        q = circuit_triple()
        attach_noise_tree(q)
        q.mark(q.noise_g, DIRTY_INTERNAL)
        q.settle(require_empty=False)
        traces = []
        mixed = False
        leftover_circuit = False
        circuit = set()
        for p in ("or_", "xor_"):
            circuit.update([p + "G", p + "spl", p + "swA", p + "swB", p + "gate", p + "lamp"])
        for k in range(4):
            got = pulse_noise_tree(q, drain=True)
            traces.append(got)
            if k == 0:
                assert_noise_budget(self, got, "vecino ruidoso")
            q.run_events(500)
        def _noise_tick():
            pulse_noise_tree(q, drain=False)
            q.clock.CallLater(_noise_tick, 500, False)

        _noise_tick()
        if has_signal_fix():
            q.record_mixed_pdq = True
            for kind, sw_a, sw_b, gid, lamp in (
                ("OR", "or_swA", "or_swB", "or_gate", "or_e_gl"),
                ("XOR", "xor_swA", "xor_swB", "xor_gate", "xor_e_gl"),
            ):
                steps = []
                cur_a, cur_b = q.entities[sw_a].on, q.entities[sw_b].on
                for a_on, b_on in BANK_SEQ:
                    if cur_a != a_on:
                        cur_a = a_on
                        steps.append((sw_a, a_on, cur_a, cur_b))
                    if cur_b != b_on:
                        cur_b = b_on
                        steps.append((sw_b, b_on, cur_a, cur_b))
                for sw, val, a_on, b_on in steps:
                    before = q.latch_changes.get(gid, 0)
                    self.assertGreater(q.last_noise_pending, 0, "vecino ruido pendiente")
                    flip_switch(q, sw, val)
                    ok = False
                    start_calls = q.pdq_calls
                    start_ms = q.clock.now_ms
                    q.run_events(500)
                    q.pulse_calls.append((list(q.tick_processed), list(q.queue)))
                    want = table_bit(kind, a_on, b_on)
                    if q.entities[gid].m_GateOpen == want:
                        ok = True
                    used = q.pdq_calls - start_calls
                    self.assertLessEqual(used, 5, "%s used %s PDQ calls" % (kind, used))
                    self.assertGreaterEqual(q.clock.now_ms - start_ms, 500)
                    self.assertTrue(ok, "%s combo not in 5 PDQ calls used=%s" % (kind, used))
                    self.assertLessEqual(q.latch_changes.get(gid, 0) - before, 1)
            q.clock.Remove(_noise_tick)
            q.entities[q.noise_sw].on = False
            q.nodes[q.noise_sw].live_gate_open = False
            q.mark(q.noise_sw, DIRTY_INTERNAL)
            q.settle(require_empty=True, ticks=100)
            for done, left in q.pulse_calls:
                mixed = mixed or (
                    any(n in q.noise_ids for n in done) and any(n in circuit for n in done)
                )
                leftover_circuit = leftover_circuit or any(n in circuit for n in left)
            _ANNOTATED.append(
                "ANOTADO vecino ruidoso con arreglo cruce=%s llamadas=%s mixtas=%s con_circuito_en_cola=%s"
                % (traces[0], len(q.pulse_calls), mixed, leftover_circuit)
            )
            self.assertTrue(mixed, "noise and circuit in the same PDQ call")
            self.assertTrue(leftover_circuit, "a call ends with circuit nodes queued")
            self.assertEqual(g2_compare(q), [])
        else:
            _ANNOTATED.append(
                "ANOTADO vecino ruidoso OR latch=%s nsw=%s noise_pdq=%s"
                % (q.entities["or_gate"].m_GateOpen, q.entities[q.noise_sw].on, traces)
            )

    def test_unresolvable_entity_return_annotated(self):
        h = helpers()
        q = SignalModel(h, order="scripted", scripted=["G", "orA", "orB"])
        q.add_node("G", SOURCE, 50)
        q.nodes["G"].m_Powered = True
        q.add_node("orA", PASSTHROUGH, 100, gated=True, logic_gate=True)
        q.add_node("orB", PASSTHROUGH, 100, gated=True, logic_gate=True)
        q.add_edge("e_ga", "G", "orA", tport=PORT_IN0)
        q.add_edge("e_ab", "orA", "orB", tport=PORT_IN0)
        q.attach_gate("orA", "OR")
        q.attach_gate("orB", "OR")
        q.fail_find.add("orA")
        q.fail_vanilla.add("orA")
        q.fail_netid.add("orA")
        q.skip_sync_nids.add("orA")
        q.settle(["G", "orA", "orB"], require_empty=False)
        if has_signal_fix():
            self.assertFalse(q.entities["orA"].m_GateOpen, "A cerrado e irresoluble")
            self.assertFalse(q.entities["orB"].m_GateOpen, "B cerrado")
        q.fail_find.discard("orA")
        q.fail_vanilla.discard("orA")
        q.fail_netid.discard("orA")
        if MUTANT != "skip_recovered_sync":
            q.skip_sync_nids.discard("orA")
        for _i in range(11):
            q.pdq_tick(warmup=False, advance_clock=False)
        _ANNOTATED.append(
            "ANOTADO OR A->B vuelta A=%s B=%s ab=%s"
            % (
                q.entities["orA"].m_GateOpen,
                q.entities["orB"].m_GateOpen,
                q.edges["e_ab"].m_SignalHigh,
            )
        )
        if has_signal_fix():
            self.assertTrue(q.entities["orA"].m_GateOpen, "A abierto al volver el registro")
            self.assertTrue(q.edges["e_ab"].m_SignalHigh, "A->B alta")
            self.assertTrue(q.entities["orB"].m_GateOpen, "B abierto")

    def test_live_gate_no_entity_annotated(self):
        q = circuit_a("AND", True, True, splitter=True, require_empty=False)
        q.fail_find.add("swA")
        q.fail_vanilla.add("swA")
        q.fail_netid.add("swA")
        q.mark("swA", DIRTY_INTERNAL)
        q.pdq_tick(warmup=True, advance_clock=False)
        _ANNOTATED.append(
            "ANOTADO compuerta viva sin entidad AND latch=%s lamp=%s"
            % (gate_open(q), lamp_power(q))
        )
        if has_signal_fix():
            self.assertTrue(q.edges["e_a0"].m_SignalHigh, "swA irresoluble conserva senal alta")
            self.assertTrue(gate_open(q))
            self.assertGreaterEqual(lamp_power(q), 10.0 - EPS)

    def test_cast_null_na(self):
        h = helpers()
        q = SignalModel(h, order="fifo", scripted=[], budgets=True)
        q.add_node("G", SOURCE, 50)
        q.nodes["G"].m_Powered = True
        q.add_node("gate", PASSTHROUGH, 100, gated=True, logic_gate=True)
        q.add_node("lamp", CONSUMER, 0, 10)
        q.add_node("Gp", SOURCE, 50)
        q.nodes["Gp"].m_Powered = True
        q.add_node("cell", PASSTHROUGH, 100, cons=1.0)
        q.add_edge("e_g", "G", "gate")
        q.add_edge("e_gl", "gate", "lamp")
        q.add_edge("e_pc", "Gp", "cell", tport=PORT_IN0)
        q.add_edge("e_set", "gate", "cell", tport=PORT_IN3)
        q.attach_gate("gate", "AND")
        q.attach_cell("cell")
        q.settle(["G", "gate", "lamp", "Gp", "cell"], require_empty=False)
        for e in q.incoming.get("gate", []):
            e.m_AllocatedPower = 10.0
        spl = SimpleNamespace(role="splitter", on=True, kind="SPL", cast_null=True)
        spl.is_open = lambda: True
        q.resolve_alias["gate"] = spl
        q.nodes["gate"].m_GateClosed = True
        q.nodes["gate"].m_IsLogicGate = True
        q.process("gate")
        alloc = q.last_alloc_avail.get("gate", 0.0)
        set_bit = q.edges["e_set"].m_SignalHigh
        q.process("cell")
        _ANNOTATED.append(
            "ANOTADO puerta logica Cast nulo n/a latch=%s allocAvail=%s set=%s M=%s"
            % (q.entities["gate"].m_GateOpen, alloc, set_bit, q.entities["cell"].m_LatchState)
        )
        self.assertAlmostEqual(alloc, 10.0, delta=EPS)
        if q.has_retry:
            self.assertFalse(q.last_signal_out.get("gate", True))
            self.assertIn("gate", q.gate_retry)
            self.assertFalse(set_bit, "Set baja tras Cast nulo")
            self.assertFalse(q.edges["e_set"].m_SignalHigh)
            self.assertFalse(q.entities["cell"].m_LatchState, "H3 M desactivada")
            self.assertFalse(q.entities["cell"].m_CellActive, "H3 M sin activar")

    def test_bounded_retry_na(self):
        notes = []
        for pos in range(11):
            h = helpers()
            q = SignalModel(h, order="fifo", scripted=[], budgets=True)
            stuck = []
            for i in range(10):
                nid = "g%d" % i
                q.add_node(nid, PASSTHROUGH, 50, gated=True, logic_gate=True)
                q.attach_gate(nid, "AND")
                q.fail_find.add(nid)
                q.fail_vanilla.add(nid)
                q.fail_netid.add(nid)
                stuck.append(nid)
            q.add_node("back", PASSTHROUGH, 50, gated=True, logic_gate=True)
            q.attach_gate("back", "AND")
            q.fail_find.add("back")
            q.fail_vanilla.add("back")
            q.fail_netid.add("back")
            q.gate_retry = stuck[:pos] + ["back"] + stuck[pos:]
            q.last_gate_retry_tick = 0
            q.validate_tick = 0
            rounds = []
            marked = []
            for call in range(30):
                before = q.retry_resolves
                qlen = len(q.queue)
                q.pdq_tick(warmup=False, advance_clock=False)
                delta = q.retry_resolves - before
                if delta:
                    rounds.append((call, delta, len(q.queue) - qlen))
                if "back" in q.queue:
                    marked.append(call)
            q.fail_find.discard("back")
            q.fail_vanilla.discard("back")
            q.fail_netid.discard("back")
            found = None
            rounds_after = 0
            for call in range(40):
                before = q.retry_resolves
                q.pdq_tick(warmup=False, advance_clock=False)
                if q.retry_resolves > before:
                    rounds_after = rounds_after + 1
                if "back" in q.queue:
                    found = call
                    break
            if q.has_retry:
                self.assertIsNotNone(found, "pos %s never came back" % pos)
                self.assertLessEqual(rounds_after, 3, "pos %s rounds=%s" % (pos, rounds_after))
                self.assertTrue(q.dirty_mask.get("back", 0) & DIRTY_INPUT)
                q.pdq_tick(warmup=False, advance_clock=False)
                self.assertIn("back", q.tick_processed, "pos %s reprocess next call" % pos)
                self.assertNotIn("back", q.gate_retry)
            extra = []
            for call in range(200):
                before = q.retry_resolves
                q.pdq_tick(warmup=False, advance_clock=False)
                extra.append(q.retry_resolves - before)
            notes.append((pos, rounds[:4], found, extra[:12]))
            if q.has_retry:
                for i in range(0, 30, 10):
                    chunk = [d for c, d, _q in rounds if i <= c < i + 10]
                    self.assertLessEqual(sum(chunk), 4)
                self.assertTrue(all(q == 0 for _c, _d, q in rounds if _d))
                self.assertLessEqual(len([x for x in extra if x]), 20)
                self.assertLessEqual(max(extra), 4)
                spaced = [c for c, _d, _q in rounds]
                for a, b in zip(spaced, spaced[1:]):
                    self.assertGreaterEqual(b - a, VALIDATE_INTERVAL)
        _ANNOTATED.append("ANOTADO reintento acotado n/a pos=%s" % notes[:2])

    def test_retry_return_value_na(self):
        h = helpers()
        q = SignalModel(h, order="fifo", scripted=[], budgets=True)
        q.add_node("A", PASSTHROUGH, 50, gated=True, logic_gate=True)
        q.attach_gate("A", "AND")
        q.queue_gate_retry("A")
        q.last_gate_retry_tick = 0
        q.validate_tick = VALIDATE_INTERVAL
        q.warmup_active = True
        rem_idle = q.pdq_tick(warmup=True, advance_clock=False)
        q2 = SignalModel(h, order="fifo", scripted=[], budgets=True)
        q2.add_node("A", PASSTHROUGH, 50, gated=True, logic_gate=True)
        q2.attach_gate("A", "AND")
        q2.add_node("tmp", CONSUMER, 0, 1)
        q2.add_edge("e_t", "A", "tmp")
        q2.queue_gate_retry("A")
        q2.last_gate_retry_tick = 0
        q2.validate_tick = VALIDATE_INTERVAL
        q2.mark("tmp", DIRTY_INTERNAL)
        rem_drain = q2.pdq_tick(warmup=False, advance_clock=False)
        q3 = SignalModel(h, order="fifo", scripted=[], budgets=True)
        q3.add_node("A", PASSTHROUGH, 50, gated=True, logic_gate=True)
        q3.attach_gate("A", "AND")
        q3.queue_gate_retry("A")
        q3.last_gate_retry_tick = 0
        q3.validate_tick = VALIDATE_INTERVAL
        q3.probe_settled = False
        rem_probe = q3.pdq_tick(warmup=False, advance_clock=False)
        _ANNOTATED.append(
            "ANOTADO reintento y valor devuelto idle=%s drain=%s probe=%s warmup=%s settled=%s"
            % (rem_idle, rem_drain, rem_probe, q.warmup_active, q3.probe_settled)
        )
        if q.has_retry:
            self.assertEqual(rem_idle, 1)
            self.assertTrue(q.warmup_active)
            self.assertFalse(q.probe_settled)
            self.assertTrue(q.dirty_mask.get("A", 0) & DIRTY_INPUT)
            self.assertEqual(q.pdq_tick(warmup=False, advance_clock=False), 0)
            self.assertEqual(rem_drain, 1)
            self.assertTrue(q2.dirty_mask.get("A", 0) & DIRTY_INPUT)
            self.assertEqual(rem_probe, 1)
            self.assertFalse(q3.probe_settled)

    def test_laser_under_5_annotated(self):
        q = circuit_d5(True, True, True)
        set_laser(q, True)
        q.nodes["laser"].m_Consumption = 5.0
        q.nodes["G"].m_MaxOutput = 4.0
        q.mark("G")
        q.mark("laser", DIRTY_INTERNAL)
        q.settle(require_empty=False)
        if not getattr(q, "settled_empty", True):
            _ANNOTATED.append("ANOTADO laser<5 no asienta en 100 ticks")
        sig = False
        if "e_ls" in q.edges:
            sig = q.edges["e_ls"].m_SignalHigh
        elif "e_set" in q.edges:
            sig = q.edges["e_set"].m_SignalHigh
        _ANNOTATED.append("ANOTADO laser<5 signal_set=%s" % sig)
        if has_signal_fix():
            self.assertTrue(getattr(q, "settled_empty", False))
            self.assertFalse(sig)


def _outside_if_overloaded(body):
    """S1: m_SignalHigh write after ENABLED filter and outside if (!overloaded)."""
    if "m_SignalHigh" not in body:
        return False
    depth_if = 0
    i = 0
    overloaded_depths = []
    writes_ok = False
    while i < len(body):
        if body.startswith("if (!overloaded)", i) or body.startswith("if(!overloaded)", i):
            overloaded_depths.append(depth_if)
        if body[i] == "{":
            depth_if = depth_if + 1
        elif body[i] == "}":
            depth_if = depth_if - 1
            overloaded_depths = [d for d in overloaded_depths if d < depth_if]
        if body.startswith("allocEdge.m_SignalHigh", i):
            in_over = any(depth_if > d for d in overloaded_depths)
            after_en = "LFPG_EDGE_ENABLED" in body[:i]
            if after_en and not in_over:
                writes_ok = True
            elif in_over:
                return False
        i = i + 1
    return writes_ok


class TestStructural(_TraceCase):
    def test_s1_allocate_writes_signal_outside_overload(self):
        body = brace_body(src_graph(), "AllocateOutput")
        self.assertTrue(_outside_if_overloaded(body))

    def test_s2_signal_changed_in_step3_and_reprocess(self):
        text = src_graph()
        self.assertIn("m_SignalChanged", text)
        pdq = brace_body(text, "ProcessDirtyQueue")
        self.assertIn("m_SignalChanged", pdq)
        self.assertIn(
            "|| m_AllocChanged || m_SignalChanged || inputChanged || demandBecameKnown",
            pdq,
        )
        self.assertIn(
            "(forceDownstream || m_AllocChanged || m_SignalChanged) && tgtNode.m_LastEpoch",
            pdq,
        )

    def test_s3_signal_changed_reset_with_alloc(self):
        text = src_graph()
        self.assertRegex(
            text,
            r"m_AllocChanged\s*=\s*false;\s*\n\s*m_SignalChanged\s*=\s*false;",
        )

    def test_s4_step1_reads_signal_bit(self):
        text = src_graph()
        pdq = brace_body(text, "ProcessDirtyQueue")
        self.assertIn("inEdge.m_SignalHigh", pdq)
        self.assertIn("LFPG_EDGE_ENABLED", pdq)
        idx = pdq.find("inEdge.m_SignalHigh")
        window = pdq[max(0, idx - 200):idx]
        self.assertNotIn("GetEdgeAllocatedPower", window)
        self.assertIn(
            "if ((inEdge.m_Flags & LFPG_EDGE_ENABLED) == 0)\n"
            "                            continue;\n\n"
            "                        if (inEdge.m_SignalHigh)",
            pdq,
        )

    def test_s5_compute_uses_live_gate(self):
        text = src_graph()
        self.assertIn("ComputeSignalOut", text)
        call = re.search(r"ComputeSignalOut\s*\(([^)]*)\)", text)
        self.assertIsNotNone(call)
        self.assertIn("gateIsClosed", call.group(1))
        self.assertNotIn("node.m_GateClosed", call.group(1))
        pos_c = text.find("ComputeSignalOut")
        pos_a = text.find("AllocateOutput(nodeId")
        self.assertGreater(pos_a, pos_c)

    def test_s6_is_port_receiving_signal(self):
        text = src_graph()
        body = brace_body(text, "IsPortReceivingSignal")
        self.assertTrue(body)
        self.assertIn("LFPG_EDGE_ENABLED", body)
        self.assertIn("m_SignalHigh", body)
        self.assertNotIn("m_AllocatedPower", body)
        self.assertIn("if (edge.m_TargetPort != portName)", body)
        pos_p = body.find("if (edge.m_TargetPort != portName)")
        pos_s = body.find("if (edge.m_SignalHigh)")
        self.assertGreater(pos_s, pos_p)

    def test_s7_readers(self):
        gate = src_gate()
        cell = src_cell()
        self.assertIn("newIn0 = graph.IsPortReceivingSignal", gate)
        self.assertIn("newIn1 = graph.IsPortReceivingSignal", gate)
        self.assertNotIn("IsPortReceivingPower(m_DeviceId, portIn0)", gate)
        sp = method(cell, "LFPG_SetPowered")
        self.assertIn("hasPower  = nm.IsPortReceivingPower(m_DeviceId, portPower);", sp)
        self.assertIn("hasSet    = graph.IsPortReceivingSignal(m_DeviceId, portSet);", sp)
        self.assertIn("hasToggle = graph.IsPortReceivingSignal", sp)

    def test_s8_edge_and_base(self):
        data = src_data()
        base = GRAPH_BASE.read_text(encoding="utf-8")
        self.assertIn("m_SignalHigh", data)
        self.assertIn("m_SignalHigh = false", data)
        self.assertIn("IsPortReceivingSignal", base)
        body = brace_body(base, "IsPortReceivingSignal")
        self.assertIn("return false", body)

    def test_s9_workflow_runs_this_file(self):
        self.assertIn("test_graph_logic_signals.py", CHECKS.read_text(encoding="utf-8"))

    def test_s10_set_output_clears_signal(self):
        body = brace_body(src_graph(), "SetOutputPortEnabled")
        self.assertEqual(body.count("m_SignalHigh = false"), 2)
        self.assertNotIn("m_AllocatedPower =", body.replace("m_AllocatedPower here", ""))

    def test_s11_no_brownout_held(self):
        text = src_gate()
        self.assertNotIn("m_BrownoutHeld", text)

    def test_s12_gate_branch_evaluate_live(self):
        text = src_graph()
        pdq = brace_body(text, "ProcessDirtyQueue")
        self.assertIn("LFPG_EvaluateGateLogic", pdq)
        pos_c = pdq.find("ComputeSignalOut")
        pos_e = pdq.find("LFPG_EvaluateGateLogic")
        pos_a = pdq.find("AllocateOutput(nodeId")
        self.assertGreater(pos_e, pos_c)
        self.assertGreater(pos_a, pos_e)
        self.assertIn("IsPortReceivingSignal", pdq)

    def test_s13_resolve_gated_entity(self):
        text = src_graph()
        pdq = brace_body(text, "ProcessDirtyQueue")
        self.assertIn("resolvedEnt = ResolveGatedEntity(nodeId);", pdq)
        self.assertNotIn("resolvedEnt = LFPG_DeviceRegistry.Get().FindById(nodeId);", pdq)
        body = brace_body(text, "ResolveGatedEntity")
        self.assertIn("FindById", body)
        self.assertIn("ResolveVanillaDevice", body)

    def test_s14_gated_no_entity_signal_false(self):
        text = src_graph()
        pdq = brace_body(text, "ProcessDirtyQueue")
        self.assertIn("m_IsLogicGate", pdq)
        self.assertIn("QueueGateRetry", pdq)
        self.assertRegex(
            pdq,
            r"if \(node.m_IsGated && !gateEntityResolved\)\s*\n\s*\{",
        )
        self.assertIsNone(
            re.search(
                r"m_IsGated\s*&&\s*!gateEntityResolved\)\s*\{\s*signalOut\s*=\s*false",
                _norm_nl(pdq),
            ),
            "S14: no-logic unresolved branch must not force signalOut=false",
        )
        self.assertIn("LFPG_LogicGateBase.Cast", pdq)
        self.assertIn("logicGateLive = LFPG_LogicGateBase.Cast(resolvedEnt)", pdq)
        self.assertIn("signalOut = false", pdq.replace("signalOut=false", "signalOut = false"))
        pos_q = pdq.find("QueueGateRetry")
        pos_a = pdq.find("AllocateOutput(nodeId")
        self.assertGreater(pos_a, pos_q)

    def test_s15_is_logic_gate_cached(self):
        data = src_data()
        self.assertIn("m_IsLogicGate", data)
        self.assertIn("m_IsLogicGate = false", data)
        text = src_graph()
        self.assertIn("EnsureNode", text)
        self.assertIn("PopulateAllNodeElecStates", text)
        ens = brace_body(text, "EnsureNode")
        pop = brace_body(text, "PopulateAllNodeElecStates")
        self.assertIn("m_IsLogicGate", ens)
        self.assertIn("m_IsLogicGate", pop)
        self.assertIn("LFPG_LogicGateBase.Cast", ens)
        self.assertIn("LFPG_LogicGateBase.Cast", pop)

    def test_s16_retry_after_validate_empty(self):
        text = src_graph()
        pdq = brace_body(text, "ProcessDirtyQueue")
        pair = "ValidateConsumerStates(edgeBudget);\n            RetryUnresolvedGates();"
        self.assertEqual(pdq.count("RetryUnresolvedGates();"), 2)
        self.assertEqual(pdq.count(pair), 2)
        retry = brace_body(text, "RetryUnresolvedGates")
        self.assertIn(
            "if (m_ValidateTickCount - m_LastGateRetryTick < LFPG_CONSUMER_VALIDATE_TICK_INTERVAL)",
            retry,
        )
        self.assertIn("retryRoundLimit = LFPG_GATE_RETRY_PER_CALL;", retry)
        self.assertIn("m_GateRetry.RemoveOrdered(0);", retry)
        self.assertNotIn("Remove(0)", retry.replace("RemoveOrdered(0)", ""))
        self.assertIn("m_GateRetry.Insert(retryNodeId);", retry)
        self.assertNotIn("InsertAt(0", retry)
        self.assertIn("ResolveGatedEntity", retry)
        self.assertIn("MarkNodeDirty(retryNodeId, LFPG_DIRTY_INPUT)", retry)
        self.assertIn("m_LastGateRetryTick", text)
        rebuild = brace_body(text, "RebuildFromWires")
        self.assertTrue("m_GateRetry" in rebuild and ("Clear" in rebuild or "Clear()" in rebuild or ".Clear" in rebuild))

    def test_s17_remaining_after_retry(self):
        text = src_graph()
        pdq = brace_body(text, "ProcessDirtyQueue")
        idxs = [m.start() for m in re.finditer(r"RetryUnresolvedGates", pdq)]
        self.assertGreaterEqual(len(idxs), 2)
        first = pdq[idxs[0]:idxs[1]]
        self.assertIn("idleRemaining = m_DirtyQueue.Count() - m_DirtyQueueHead", first)
        self.assertIn("return idleRemaining;", first)
        self.assertIn("OnProcessDirtyQueue(idleRemaining", first)
        second = pdq[idxs[1]:]
        self.assertIn("RetryUnresolvedGates();\n            remaining = m_DirtyQueue.Count() - m_DirtyQueueHead;", second)
        pos_rem = second.find("remaining = m_DirtyQueue.Count() - m_DirtyQueueHead;")
        pos_probe = second.find("OnProcessDirtyQueue")
        self.assertGreater(pos_probe, pos_rem)


class TestBudgets(_TraceCase):
    def test_node_budget_65_consumers_green(self):
        h = helpers()
        q = SignalModel(h, order="fifo", scripted=[], budgets=True)
        q.skip_admit = True
        q.add_node("G", SOURCE, 50)
        q.nodes["G"].m_Powered = True
        for i in range(65):
            nid = "c%d" % i
            q.add_node(nid, CONSUMER, 0, 1)
            q.add_edge("e%d" % i, "G", nid)
        for i in range(65):
            q.mark("c%d" % i, DIRTY_INTERNAL)
        remaining = q.pdq_tick(warmup=False, advance_clock=False)
        self.assertEqual(q.last_processed, 64)
        self.assertEqual(remaining, 1)
        left = sum(1 for i in range(65) if q.nodes["c%d" % i].m_InQueue or "c%d" % i in q.queue)
        self.assertGreaterEqual(left, 1)

    def test_edge_budget_finishes_node_green(self):
        h = helpers()
        q = SignalModel(h, order="fifo", scripted=[], budgets=True)
        q.skip_admit = True
        q.add_node("hub", PASSTHROUGH, 500)
        for i in range(300):
            sid = "s%d" % i
            q.add_node(sid, SOURCE, 50)
            q.nodes[sid].m_Powered = True
            q.add_edge("e%d" % i, sid, "hub")
        q.mark("hub", DIRTY_INTERNAL)
        q.edge_budget = 256
        q.node_budget = 64
        q.pdq_tick(warmup=False, advance_clock=False)
        self.assertGreater(q.edge_visits, 256)
        self.assertEqual(q.nodes["hub"].m_LastEpoch, q.epoch)
        taken = [nid for nid in q.nodes if q.nodes[nid].m_LastEpoch == q.epoch]
        self.assertEqual(taken, ["hub"])

    def test_sweep_skips_queued_and_dirty_green(self):
        h = helpers()
        q = SignalModel(h, order="fifo", scripted=[], budgets=True)
        q.skip_admit = True
        q.add_node("G", SOURCE, 50)
        q.nodes["G"].m_Powered = True
        q.add_node("queued", CONSUMER, 0, 1)
        q.add_node("dirty", CONSUMER, 0, 1)
        q.add_node("mis", CONSUMER, 0, 1)
        q.add_node("ok", CONSUMER, 0, 1)
        q.add_edge("e_q", "G", "queued")
        q.add_edge("e_d", "G", "dirty")
        q.add_edge("e_o", "G", "ok")
        q.nodes["mis"].m_Powered = True
        q.nodes["ok"].m_Powered = False
        q.mark("queued", DIRTY_INTERNAL)
        q.dirty_mask["dirty"] = DIRTY_INPUT
        q.nodes["dirty"].m_InQueue = False
        q.validate_tick = 10
        q.last_validate_tick = 0
        q.validate_syncs = []
        q.validate_consumers(10 ** 9)
        self.assertEqual(q.validate_syncs, [])
        q.validate_tick = 20
        q.last_validate_tick = 0
        q.validate_syncs = []
        q.pdq_tick(warmup=False, advance_clock=False)
        self.assertEqual(q.queue, [])
        self.assertIn("mis", q.validate_syncs)
        self.assertNotIn("dirty", q.validate_syncs)
        self.assertFalse(q.nodes["mis"].m_Powered)


GRAPH_IMPL = ROOT / "scripts/5_Mission/LFPG_ElecGraphImpl.c"

# §6.6. file+old are exact source strings. count is occurrences in that file.
MUTANT_ROWS = [
    {"name": "Puertas de vuelta a IsPortReceivingPower",
     "file": GATE_SRC, "count": 2,
     "old": "graph.IsPortReceivingSignal", "new": "graph.IsPortReceivingPower",
     "s": ["test_s7_readers"],
     "f": ["test_a3_xor_splitter_10", "test_a4_with_splitter_must_oscillate_on_base", "test_b1_xor_door_splitter"],
     "flag": "gates_to_power"},
    {"name": "Set de la celda de vuelta a flujo",
     "file": CELL_SRC, "count": 1,
     "old": "hasSet    = graph.IsPortReceivingSignal(m_DeviceId, portSet);",
     "new": "hasSet    = nm.IsPortReceivingPower(m_DeviceId, portSet);",
     "s": ["test_s7_readers"], "f": ["test_d5_power_via_splitter", "test_d5_power_direct"],
     "flag": "cell_set_to_flow"},
    {"name": "Power de la celda a senal",
     "file": CELL_SRC, "count": 1,
     "old": "hasPower  = nm.IsPortReceivingPower(m_DeviceId, portPower);",
     "new": "hasPower  = nm.IsPortReceivingSignal(m_DeviceId, portPower);",
     "s": ["test_s7_readers"], "f": ["test_cell_power_vs_signal"],
     "flag": "cell_power_to_signal"},
    {"name": "Escribir la senal solo dentro de if (!overloaded)",
     "file": GRAPH_IMPL, "count": 1,
     "old": "            if (allocEdge.m_SignalHigh != signalOut)\n            {\n                m_SignalChanged = true;\n            }\n            allocEdge.m_SignalHigh = signalOut;\n\n			if (!compareInline)\n				m_PreviousAllocations[ai] = allocEdge.m_AllocatedPower;\n            float newAlloc = 0.0;\n            if (!overloaded)\n            {\n                if (totalSoftDemand > LFPG_PROPAGATION_EPSILON)",
     "new": "			if (!compareInline)\n				m_PreviousAllocations[ai] = allocEdge.m_AllocatedPower;\n            float newAlloc = 0.0;\n            if (!overloaded)\n            {\n            if (allocEdge.m_SignalHigh != signalOut)\n            {\n                m_SignalChanged = true;\n            }\n            allocEdge.m_SignalHigh = signalOut;\n                if (totalSoftDemand > LFPG_PROPAGATION_EPSILON)",
     "s": ["test_s1_allocate_writes_signal_outside_overload"],
     "f": ["test_overload_annotated"],
     "flag": "signal_in_overloaded"},
    {"name": "Quitar m_SignalChanged de Step 3",
     "file": GRAPH_IMPL, "count": 1,
     "old": "if (outputDelta > LFPG_PROPAGATION_EPSILON || forceDownstream || m_AllocChanged || m_SignalChanged || inputChanged || demandBecameKnown)",
     "new": "if (outputDelta > LFPG_PROPAGATION_EPSILON || forceDownstream || m_AllocChanged || inputChanged || demandBecameKnown)",
     "s": ["test_s2_signal_changed_in_step3_and_reprocess"],
     "f": ["test_mutant_signal_changed_xor_open_input1", "test_signal_changed_laser_switch_set"],
     "flag": "drop_signal_step3"},
    {"name": "Quitar m_SignalChanged del reproceso en la misma epoca",
     "file": GRAPH_IMPL, "count": 1,
     "old": "if ((forceDownstream || m_AllocChanged || m_SignalChanged) && tgtNode.m_LastEpoch == m_CurrentEpoch)",
     "new": "if ((forceDownstream || m_AllocChanged) && tgtNode.m_LastEpoch == m_CurrentEpoch)",
     "s": ["test_s2_signal_changed_in_step3_and_reprocess"],
     "f": ["test_a3_xor_splitter_10"],
     "flag": "drop_signal_same_epoch",
     "annotate_f": True,
     "annotate_why": "equivalente en F (H2): la unica diferencia del mutante es un incremento mas de m_RequeueCount, porque MarkNodeDirty (:2109-2115) rebobina y cuenta cuando el reset de :2883 no corrio; solo muerde S2"},
    {"name": "ComputeSignalOut con node.m_GateClosed",
     "file": GRAPH_IMPL, "count": 1,
     "old": "signalOut = ComputeSignalOut(isSourceForSignal, newOutput, signalIn, node.m_VirtualGeneration, node.m_Consumption, newPowered, gateIsClosed);",
     "new": "signalOut = ComputeSignalOut(isSourceForSignal, newOutput, signalIn, node.m_VirtualGeneration, node.m_Consumption, newPowered, node.m_GateClosed);",
     "s": ["test_s5_compute_uses_live_gate"],
     "f": ["test_d5_first_pass_open"],
     "flag": "compute_gate_cached"},
    {"name": "ComputeSignalOut sin la condicion de autoconsumo",
     "file": GRAPH_IMPL, "count": 1,
     "old": "        if (cons > LFPG_PROPAGATION_EPSILON)\n        {\n            if (powered)\n            {\n                return true;\n            }\n            return false;",
     "new": "        if (false)\n        {\n            if (powered)\n            {\n                return true;\n            }\n            return false;",
     "s": [],
     "f": ["test_laser_under_5_annotated"],
     "flag": "compute_no_autoconsumo"},
    {"name": "Senal atada a m_Overloaded",
     "file": GRAPH_IMPL, "count": 1,
     "old": "            allocEdge.m_SignalHigh = signalOut;",
     "new": "            allocEdge.m_SignalHigh = !overloaded;",
     "s": [],
     "f": ["test_overload_annotated"],
     "flag": "signal_tied_overloaded"},
    {"name": "Step 1 sin filtro de habilitada para la senal",
     "file": GRAPH_IMPL, "count": 1,
     "old": "                        if ((inEdge.m_Flags & LFPG_EDGE_ENABLED) == 0)\n                            continue;\n\n                        if (inEdge.m_SignalHigh)\n                            signalIn = true;",
     "new": "                        if (inEdge.m_SignalHigh)\n                            signalIn = true;\n\n                        if ((inEdge.m_Flags & LFPG_EDGE_ENABLED) == 0)\n                            continue;",
     "s": ["test_s4_step1_reads_signal_bit"],
     "f": ["test_rehab_output_annotated"],
     "flag": "step1_no_enabled"},
    {"name": "IsPortReceivingSignal sin filtro de puerto",
     "file": GRAPH_IMPL, "count": 1,
     "old": "            if (edge.m_TargetPort != portName)\n                continue;\n\n            if (edge.m_SignalHigh)\n                return true;",
     "new": "            if (false)\n                continue;\n\n            if (edge.m_SignalHigh)\n                return true;",
     "s": ["test_s6_is_port_receiving_signal"],
     "f": ["test_xor_port_filter"],
     "flag": "isport_no_port"},
    {"name": "Constructor con m_SignalHigh = true",
     "file": DATA_SRC, "count": 1,
     "old": "        m_SignalHigh = false;",
     "new": "        m_SignalHigh = true;",
     "s": ["test_s8_edge_and_base"],
     "f": ["test_boot_ctor_signal"],
     "flag": "ctor_signal_true"},
    {"name": "No limpiar el bit en SetOutputPortEnabled",
     "file": GRAPH_IMPL, "count": 2,
     "old": "                edge.m_SignalHigh = false;",
     "new": "                /* no clear */",
     "s": ["test_s10_set_output_clears_signal"],
     "f": ["test_rehab_output_annotated"],
     "flag": "no_clear_setoutput"},
    {"name": "Volver a poner la tolerancia de un apagon",
     "file": GATE_SRC, "count": 1,
     "old": "        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();\n        if (nm)\n        {\n            string portIn0 = \"input_0\";\n            string portIn1 = \"input_1\";\n            LFPG_ElecGraph graph = nm.GetGraph();\n            if (graph)\n            {\n                newIn0 = graph.IsPortReceivingSignal(m_DeviceId, portIn0);\n                newIn1 = graph.IsPortReceivingSignal(m_DeviceId, portIn1);\n            }\n        }\n\n        bool newGate = LFPG_EvaluateGateLogic(newIn0, newIn1);\n        bool gateChanged = false;\n\n        // Inputs are read by signal, which does not go dark during a\n        // distribution ramp, so the latch follows the logic on every reading.\n        if (m_GateOpen != newGate)\n        {\n            m_GateOpen = newGate;\n            gateChanged = true;\n        }\n\n        bool changed = false;\n\n        if (m_PoweredNet != powered)\n        {\n            m_PoweredNet = powered;\n            changed = true;\n        }\n\n        if (m_Input0Powered != newIn0)",
     "new": "        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();\n        if (nm)\n        {\n            string portIn0 = \"input_0\";\n            string portIn1 = \"input_1\";\n            newIn0 = nm.IsPortReceivingPower(m_DeviceId, portIn0);\n            newIn1 = nm.IsPortReceivingPower(m_DeviceId, portIn1);\n        }\n\n        bool newGate = LFPG_EvaluateGateLogic(newIn0, newIn1);\n        bool gateChanged = false;\n\n        // m_GateOpen is a latch, and IsPortReceivingPower answers with THIS\n        // epoch's allocation. AllocateOutput zeroes every outgoing edge of a\n        // distributor whose demand exceeds what it currently holds\n        // (LFPG_ElecGraphImpl.c:3836-4161), so on the epoch right after this\n        // gate opens - when its demand signal jumps from the closed-gate\n        // probe (LFPG_GATE_PROBE_DEMAND, 1.0) to what the load downstream\n        // really asks for - both input ports read dark while the upstream\n        // ramps. Re-latching on that reading closes the gate, the raised\n        // demand collapses back to the probe, the ramp restarts, and the\n        // gate never converges. Measured in-game as in0=1 in1=1 output=0\n        // powered=0 held for 24.6 s (B5, gs02-ev/B5-20260912/issue1).\n        //\n        // A single total brownout carries no truth-table information, so the\n        // latch keeps its value for exactly one evaluation. The SECOND\n        // consecutive dark reading is believed: a real all-off keeps\n        // arriving, a ramp does not, and one evaluation is all the ramp\n        // needs. The per-input flags below still follow the flow, so the\n        // client LEDs are not held.\n        bool totalBrownout = false;\n        if (!newIn0 && !newIn1)\n        {\n            totalBrownout = true;\n        }\n\n        bool holdLatch = false;\n        if (totalBrownout)\n        {\n            if (!m_BrownoutHeld)\n            {\n                m_BrownoutHeld = true;\n                holdLatch = true;\n            }\n        }\n        else\n        {\n            m_BrownoutHeld = false;\n        }\n\n        if (!holdLatch && m_GateOpen != newGate)\n        {\n            m_GateOpen = newGate;\n            gateChanged = true;\n        }\n\n        bool changed = false;\n\n        if (m_PoweredNet != powered)\n        {\n            m_PoweredNet = powered;\n            changed = true;\n        }\n\n        if (m_Input0Powered != newIn0)",
     "old2": "    protected bool m_Overloaded    = false;\n\n    void LFPG_LogicGateBase()",
     "new2": "    protected bool m_Overloaded    = false;\n    // One evaluation of slack for the convergence brownout in\n    // LFPG_SetPowered. Not net-synced and not persisted: it is always\n    // false while the gate is open, because opening requires a live\n    // reading and a live reading clears it.\n    protected bool m_BrownoutHeld  = false;\n\n    void LFPG_LogicGateBase()",
     "s": ["test_s11_no_brownout_held"],
     "f": ["test_transitions_annotated", "test_b1_xor_door_splitter"],
     "flag": "brownout_back"},
    {"name": "Quitar la rama de puerta",
     "file": GRAPH_IMPL, "count": 1,
     "old": "                        signalOut = logicGateLive.LFPG_EvaluateGateLogic(gateSig0, gateSig1);",
     "new": "                        signalOut = signalIn;",
     "s": ["test_s12_gate_branch_evaluate_live"],
     "f": ["test_old_latch_annotated", "test_race_and_g2_annotated", "test_old_latch_no_new_wire_annotated"],
     "flag": "no_gate_branch"},
    {"name": "Modelo sin el limite de nodos",
     "file": None, "count": 0, "old": None, "new": None,
     "s": [], "f": ["test_node_budget_65_consumers_green"],
     "flag": "no_node_budget"},
    {"name": "Modelo sin el limite de aristas",
     "file": None, "count": 0, "old": None, "new": None,
     "s": [], "f": ["test_edge_budget_finishes_node_green"],
     "flag": "no_edge_budget"},
    {"name": "Pre-gate solo con FindById",
     "file": GRAPH_IMPL, "count": 1,
     "old": "                    resolvedEnt = ResolveGatedEntity(nodeId);",
     "new": "                    resolvedEnt = LFPG_DeviceRegistry.Get().FindById(nodeId);",
     "s": ["test_s13_resolve_gated_entity"],
     "f": ["test_registry_and_unresolvable_annotated"],
     "flag": "pregated_find_only"},
    {"name": "Sin entidad, publicar con el latch cacheado",
     "file": GRAPH_IMPL, "count": 1,
     "old": "                        signalOut = false;\n                        QueueGateRetry(nodeId);",
     "new": "                        signalOut = !node.m_GateClosed;\n                        QueueGateRetry(nodeId);",
     "s": ["test_s14_gated_no_entity_signal_false"],
     "f": ["test_unresolvable_entity_return_annotated"],
     "flag": "cached_latch_v3"},
    {"name": "Sin reintento en vacio",
     "file": GRAPH_IMPL, "count": 2,
     "old": "            RetryUnresolvedGates();\n",
     "new": "            \n",
     "s": ["test_s16_retry_after_validate_empty"],
     "f": ["test_unresolvable_entity_return_annotated"],
     "flag": "no_retry_empty"},
    {"name": "Baja en todo m_IsGated sin entidad",
     "file": GRAPH_IMPL, "count": 1,
     "old": "                    if (node.m_IsGated && !gateEntityResolved)\n                    {\n                        QueueGateRetry(nodeId);\n                    }",
     "new": "                    if (node.m_IsGated && !gateEntityResolved)\n                    {\n                        signalOut = false;\n                        QueueGateRetry(nodeId);\n                    }",
     "s": ["test_s14_gated_no_entity_signal_false"],
     "f": ["test_live_gate_no_entity_annotated"],
     "flag": "all_gated_low"},
    {"name": "Sin la rama de Cast nulo",
     "file": GRAPH_IMPL, "count": 1,
     "old": "                        logicGateLive = LFPG_LogicGateBase.Cast(resolvedEnt);",
     "new": "                        logicGateLive = resolvedEnt;",
     "s": ["test_s14_gated_no_entity_signal_false"],
     "f": ["test_cast_null_na"],
     "flag": "no_cast_null"},
    {"name": "Reintento sin cadencia",
     "file": GRAPH_IMPL, "count": 1,
     "old": "        if (m_ValidateTickCount - m_LastGateRetryTick < LFPG_CONSUMER_VALIDATE_TICK_INTERVAL)\n            return;",
     "new": "        /* no cadence */",
     "s": ["test_s16_retry_after_validate_empty"],
     "f": ["test_bounded_retry_na"],
     "flag": "retry_no_cadence"},
    {"name": "Reintento sin tope por ronda",
     "file": GRAPH_IMPL, "count": 1,
     "old": "        retryRoundLimit = LFPG_GATE_RETRY_PER_CALL;",
     "new": "        retryRoundLimit = 999;",
     "s": ["test_s16_retry_after_validate_empty"],
     "f": ["test_bounded_retry_na"],
     "flag": "retry_no_cap"},
    {"name": "Reintento sin rotacion",
     "file": GRAPH_IMPL, "count": 2,
     "old": "m_GateRetry.Insert(retryNodeId);",
     "new": "m_GateRetry.InsertAt(0, retryNodeId);",
     "s": ["test_s16_retry_after_validate_empty"],
     "f": ["test_bounded_retry_na"],
     "flag": "retry_no_rotate"},
    {"name": "Rotacion con Remove(0) en vez de RemoveOrdered(0)",
     "file": GRAPH_IMPL, "count": 1,
     "old": "            m_GateRetry.RemoveOrdered(0);",
     "new": "            m_GateRetry.Remove(0);",
     "s": ["test_s16_retry_after_validate_empty"],
     "f": ["test_bounded_retry_na"],
     "flag": "retry_remove"},
    {"name": "Reintento con LFPG_DIRTY_INTERNAL",
     "file": GRAPH_IMPL, "count": 1,
     "old": "                MarkNodeDirty(retryNodeId, LFPG_DIRTY_INPUT);",
     "new": "                MarkNodeDirty(retryNodeId, LFPG_DIRTY_INTERNAL);",
     "s": ["test_s16_retry_after_validate_empty"],
     "f": [],
     "flag": "retry_dirty_internal",
     "annotate_f": True,
     "annotate_why": "F no muerde con el modelo fiel: el Step 5 que fallo borro el registro (:3117) y la pasada del reintento escribe igual (§5.10); solo muerde S16"},
    {"name": "Contar lo pendiente antes del reintento",
     "file": GRAPH_IMPL, "count": 1,
     "old": "            idleRemaining = m_DirtyQueue.Count() - m_DirtyQueueHead;",
     "new": "            idleRemaining = 0;",
     "old2": "            RetryUnresolvedGates();\n            remaining = m_DirtyQueue.Count() - m_DirtyQueueHead;",
     "new2": "            RetryUnresolvedGates();\n            remaining = remaining;",
     "s": ["test_s17_remaining_after_retry"],
     "f": ["test_retry_return_value_na"],
     "flag": "count_before_retry"},
    {"name": "Modelo que escribe el bit de un interruptor al encolarlo",
     "file": None, "count": 0, "old": None, "new": None,
     "s": [], "f": ["test_race_and_mb_enqueue_bit"],
     "flag": "enqueue_switch_bit"},
    {"name": "Modelo con barrido incondicional",
     "file": None, "count": 0, "old": None, "new": None,
     "s": [], "f": ["test_sweep_skips_queued_and_dirty_green"],
     "flag": "uncond_sweep"},
    {"name": "IsPortReceivingSignal if (true) return false tras inEdges",
     "file": GRAPH_IMPL, "count": 1,
     "old": "        array<ref LFPG_ElecEdge> inEdges;\n        if (!m_Incoming.Find(deviceId, inEdges))",
     "new": "        array<ref LFPG_ElecEdge> inEdges;\n        if (true) return false;\n        if (!m_Incoming.Find(deviceId, inEdges))",
     "s": [],
     "f": ["test_isport_extracted_reader", "test_a3_xor_splitter_10"],
     "flag": "isport_true_return_false"},
    {"name": "IsPortReceivingSignal return false si deviceId no vacio",
     "file": GRAPH_IMPL, "count": 1,
     "old": "    override bool IsPortReceivingSignal(string deviceId, string portName)\n    {\n        array<ref LFPG_ElecEdge> inEdges;",
     "new": "    override bool IsPortReceivingSignal(string deviceId, string portName)\n    {\n        if (deviceId != \"\") return false;\n        array<ref LFPG_ElecEdge> inEdges;",
     "s": [],
     "f": ["test_isport_extracted_reader", "test_a3_xor_splitter_10"],
     "flag": "isport_early_false"},
    {"name": "Suprimir sincronizacion del nodo recuperado",
     "file": None, "count": 0, "old": None, "new": None,
     "s": [], "f": ["test_unresolvable_entity_return_annotated"],
     "flag": "skip_recovered_sync"},
    {"name": "resolve_netid siempre None",
     "file": None, "count": 0, "old": None, "new": None,
     "s": [], "f": ["test_registry_and_unresolvable_annotated"],
     "flag": "netid_always_none"},
]


def _all_test_classes():
    out = []
    for obj in list(globals().values()):
        if isinstance(obj, type) and issubclass(obj, unittest.TestCase) and obj is not unittest.TestCase:
            out.append(obj)
    return out


def _run_named_test(name):
    if MUTANT == "arnes_all_f":
        raise HarnessError("error de arnes: extraction failed before behavior")
    for cls in _all_test_classes():
        if hasattr(cls, name) and name.startswith("test_"):
            inst = cls(name)
            getattr(inst, name)()
            return
    raise HarnessError("no test method %s" % name)


class TestFidelityRows(_TraceCase):
    def test_isport_extracted_reader(self):
        if not has_signal_fix():
            self.skipTest("base sin arreglo")
        h = helpers()
        q = SignalModel(h)
        q.add_node("src", SOURCE, 50)
        q.nodes["src"].m_Powered = True
        q.add_node("dst", CONSUMER, 0, 10)
        q.add_edge("e", "src", "dst", tport=PORT_IN0)
        q.edges["e"].m_SignalHigh = True
        fn = q.isport_fn
        self.assertIsNotNone(fn)
        self.assertTrue(fn(q, "dst", PORT_IN0), "positivo puerto y bit")
        self.assertFalse(fn(q, "dst", PORT_IN1), "negativo puerto")
        q.edges["e"].m_Flags = 0
        self.assertFalse(fn(q, "dst", PORT_IN0), "negativo habilitacion")
        q.edges["e"].m_Flags = EDGE_ENABLED
        q.edges["e"].m_SignalHigh = False
        self.assertFalse(fn(q, "dst", PORT_IN0), "negativo bit")
        q.edges["e"].m_SignalHigh = True
        self.assertTrue(q.nm.IsPortReceivingSignal("dst", PORT_IN0))

    def test_two_cells_independent_timers(self):
        if not has_signal_fix():
            self.skipTest("base sin arreglo")
        h = helpers()
        q = SignalModel(h, order="fifo")
        q.add_node("Ga", SOURCE, 50)
        q.nodes["Ga"].m_Powered = True
        q.add_node("Gb", SOURCE, 50)
        q.nodes["Gb"].m_Powered = True
        q.add_node("swA", PASSTHROUGH, 50, gated=True)
        q.add_node("swB", PASSTHROUGH, 50, gated=True)
        q.add_node("cellA", PASSTHROUGH, 100, cons=1.0)
        q.add_node("cellB", PASSTHROUGH, 100, cons=1.0)
        q.add_edge("e_pa", "Ga", "cellA", tport=PORT_IN0)
        q.add_edge("e_sa", "Ga", "swA")
        q.add_edge("e_ta", "swA", "cellA", tport=PORT_IN1)
        q.add_edge("e_pb", "Gb", "cellB", tport=PORT_IN0)
        q.add_edge("e_sb", "Gb", "swB")
        q.add_edge("e_tb", "swB", "cellB", tport=PORT_IN1)
        q.attach_switch("swA", False)
        q.attach_switch("swB", False)
        a = q.attach_cell("cellA")
        b = q.attach_cell("cellB")
        self.assertIsNot(a.cb_toggle, b.cb_toggle)
        self.assertIsNot(a.cb_power, b.cb_power)
        q.settle(list(q.nodes.keys()), require_empty=False)
        q.run_events(1400)
        a.m_ToggleGraceUntilMs = 0
        b.m_ToggleGraceUntilMs = 0
        a.m_ToggleLowSinceMs = 0
        b.m_ToggleLowSinceMs = 0
        a.m_PowerSinceMs = 0
        b.m_PowerSinceMs = 0

        def _short(nid, on):
            q.entities[nid].on = on
            q.nodes[nid].live_gate_open = on
            q.mark(nid, DIRTY_INTERNAL)
            q.pdq_tick(warmup=False, advance_clock=False)

        _short("swA", True)
        _short("swB", True)
        pending_a = [p for p in q.clock.pending if p[1] is a.cb_toggle]
        pending_b = [p for p in q.clock.pending if p[1] is b.cb_toggle]
        self.assertTrue(pending_a, "A tiene Toggle pendiente")
        self.assertTrue(pending_b, "B tiene Toggle pendiente")
        q.clock.Remove(b.cb_toggle)
        self.assertTrue(any(p[1] is a.cb_toggle for p in q.clock.pending), "A sigue pendiente")
        self.assertFalse(any(p[1] is b.cb_toggle for p in q.clock.pending), "B cancelado")
        q.run_events(400)
        self.assertTrue(a.m_LatchState, "A voltea")
        self.assertFalse(b.m_LatchState, "B no voltea")

    def test_warmup_128_leaves_12(self):
        h = helpers()
        q = SignalModel(h, order="fifo", budgets=True)
        q.skip_admit = True
        q.add_node("G", SOURCE, 50)
        q.nodes["G"].m_Powered = True
        for i in range(140):
            nid = "c%d" % i
            q.add_node(nid, CONSUMER, 0, 1)
            q.add_edge("e%d" % i, "G", nid)
            q.mark(nid, DIRTY_INTERNAL)
        rem = q.pdq_tick(warmup=True, advance_clock=False)
        self.assertEqual(q.last_processed, 128, "warmup 128 nodos")
        self.assertEqual(rem, 12, "140 pendientes dejan 12")

    def test_cold_start_empty_splitter(self):
        h = helpers()
        q = SignalModel(h)
        q.add_node("G", SOURCE, 50)
        q.nodes["G"].m_Powered = True
        add_splitter(q, "spl")
        q.add_edge("e_gs", "G", "spl", tport="input_1")
        e = q.edges["e_gs"]
        d = q.raw_demand(e, 50.0)
        self.assertEqual(d, 0.0, "Splitter vacio demanda 0 (fuente :2614)")

    def test_gate_transition_sets_alloc_changed(self):
        if not has_signal_fix():
            self.skipTest("base sin arreglo")
        q = circuit_d5(True, False, True)
        q.nodes["laser"].m_GateClosed = True
        q.entities["laser"].on = True
        q.nodes["laser"].live_gate_open = True
        q.nodes["laser"].m_LastStableOutput = 5.0
        q.nodes["laser"].m_DemandKnown = True
        q.m_AllocChanged = False
        q.process("laser")
        self.assertTrue(q.m_AllocChanged, "m_AllocChanged en transicion :2758")

    def test_timer_pdq_tie_both_orders(self):
        if not has_signal_fix():
            self.skipTest("base sin arreglo")
        h = helpers()
        q = SignalModel(h, order="fifo")
        q.tie_timers_first = True
        q.add_node("G", SOURCE, 50)
        q.nodes["G"].m_Powered = True
        q.add_node("c", CONSUMER, 0, 1)
        q.add_edge("e", "G", "c")
        q.mark("G", DIRTY_INTERNAL)
        fired = []
        q.clock.CallLater(lambda: fired.append("t"), 100, False)
        q.next_pdq_ms = 100
        q.clock.now_ms = 0
        q.run_events(100, tie_timers_first=True)
        self.assertEqual(q.event_trace[:2], ["t", "pdq"], "empate timer-primero orden t,pdq")
        self.assertEqual(fired, ["t"], "empate timer-primero ejecuta el callback")
        self.assertGreaterEqual(q.pdq_calls, 1, "empate timer-primero tambien PDQ")
        self.assertFalse(q.clock.pending)
        q2 = SignalModel(h, order="fifo")
        q2.tie_timers_first = False
        q2.add_node("G", SOURCE, 50)
        q2.nodes["G"].m_Powered = True
        q2.add_node("c", CONSUMER, 0, 1)
        q2.add_edge("e", "G", "c")
        q2.mark("G", DIRTY_INTERNAL)
        fired2 = []
        q2.clock.CallLater(lambda: fired2.append("t"), 100, False)
        q2.next_pdq_ms = 100
        q2.run_events(100, tie_timers_first=False)
        self.assertEqual(q2.event_trace[:2], ["pdq", "t"], "empate PDQ-primero orden pdq,t")
        self.assertGreaterEqual(q2.pdq_calls, 1, "empate PDQ-primero ejecuta PDQ")
        self.assertEqual(fired2, ["t"], "empate PDQ-primero tambien el callback")
        self.assertFalse(q2.clock.pending)

    def test_harness_error_row_demonstrates_runner(self):
        row = {
            "name": "Todas las F lanzan error de arnes",
            "file": None, "s": [], "f": ["test_a3_xor_splitter_10"],
            "flag": "arnes_all_f",
        }
        global MUTANT
        MUTANT = "arnes_all_f"
        failed = False
        try:
            try:
                _run_named_test(row["f"][0])
            except HarnessError:
                failed = True
            except AssertionError:
                self.fail("HarnessError no debe confundirse con AssertionError")
        finally:
            MUTANT = None
        self.assertTrue(failed, "el runner trata el error de arnes como fallo, no como F")


class TestMutants(_TraceCase):
    def test_mutants_skip_or_run(self):
        if not has_signal_fix():
            _SKIPPED.append("mutantes §6.6: base sin arreglo")
            self.skipTest("base sin arreglo")
        global _SRC_OVERRIDE, _SRC_FILES, MUTANT
        missing = []
        for row in MUTANT_ROWS:
            s_bit = []
            f_bit = []
            try:
                if row["file"] is not None:
                    text = row["file"].read_text(encoding="utf-8")
                    mutated = apply_mutant_replace(
                        text, row["old"], row["new"], row["count"], row["name"],
                    )
                    if row.get("old2"):
                        mutated = apply_mutant_replace(
                            mutated, row["old2"], row["new2"], 1, row["name"] + " old2",
                        )
                    _SRC_FILES[row["file"]] = mutated
                    if row["file"] == GRAPH_IMPL:
                        _SRC_OVERRIDE = mutated
                MUTANT = row["flag"]
                for tname in row["s"]:
                    try:
                        _run_named_test(tname)
                        missing.append("%s S %s no muerde" % (row["name"], tname))
                    except HarnessError as ex:
                        self.fail("mutant %s S %s error de arnes: %s" % (row["name"], tname, ex))
                    except AssertionError:
                        s_bit.append(tname)
                    except Exception as ex:
                        self.fail("mutant %s S %s error de arnes: %s" % (row["name"], tname, ex))
                for tname in row["f"]:
                    try:
                        _run_named_test(tname)
                        if row.get("annotate_f"):
                            pass
                        else:
                            missing.append("%s F %s no muerde" % (row["name"], tname))
                    except HarnessError as ex:
                        self.fail("mutant %s F %s error de arnes: %s" % (row["name"], tname, ex))
                    except AssertionError:
                        f_bit.append(tname)
                    except Exception as ex:
                        self.fail("mutant %s F %s error de arnes: %s" % (row["name"], tname, ex))
                if row.get("annotate_f"):
                    _ANNOTATED.append(
                        "ANOTADO mutante %s F anotado: %s; F que mordio igualmente=%s"
                        % (row["name"], row.get("annotate_why", "-"), f_bit)
                    )
            finally:
                _SRC_OVERRIDE = None
                _SRC_FILES.clear()
                MUTANT = None
            line = "ANOTADO MUT %s S=%s F=%s" % (
                row["name"], ",".join(s_bit) or "-", ",".join(f_bit) or "-",
            )
            print(line)
            _ANNOTATED.append(line)
        if missing:
            self.fail("; ".join(missing))

    def test_mutant_signal_changed_xor_open_input1(self):
        if not has_signal_fix():
            self.skipTest("base sin arreglo")
        h = helpers()
        q = SignalModel(h, order="scripted", scripted=["G", "spl", "swA", "gate"])
        q.add_node("G", SOURCE, 50)
        q.nodes["G"].m_Powered = True
        add_splitter(q, "spl")
        q.add_node("swA", PASSTHROUGH, 50, gated=True)
        q.add_node("gate", PASSTHROUGH, 100, gated=True)
        q.add_edge("e_gs", "G", "spl", tport="input_1")
        q.add_edge("e_sa", "spl", "swA", sport=PORT_OUT1)
        q.add_edge("e_a0", "swA", "gate", tport=PORT_IN0)
        q.attach_switch("swA", True)
        q.attach_gate("gate", "XOR")
        q.settle(["G", "spl", "swA", "gate"], require_empty=False)
        self.assertTrue(q.settled_empty, "XOR asentado antes de apagar G")
        xor_trig0 = [t for t in q.step3_log if t[0] == "gate"]
        self.assertTrue(xor_trig0, "precondicion XOR procesado")
        self.assertEqual(len([e for e in q.incoming.get("gate", []) if e.m_TargetPort == PORT_IN1]), 0)
        self.assertFalse(q.outgoing.get("gate"))
        q.nodes["G"].m_Powered = False
        q.mark("G", DIRTY_INTERNAL)
        q.settle(require_empty=False)
        swa_trig = [t for t in q.step3_log if t[0] == "swA"]
        if swa_trig:
            trig = swa_trig[-1][1]
            self.assertFalse(trig.get("inputChanged"), "ancla (i) inputChanged swA")
        xor_trig = [t for t in q.step3_log if t[0] == "gate"]
        if xor_trig:
            trig = xor_trig[-1][1]
            self.assertFalse(trig.get("inputChanged"), "ancla (i) XOR inputChanged")
        self.assertFalse(gate_open(q), "mutant (i) XOR open input_1: latch=%s" % gate_open(q))

    def test_signal_changed_laser_switch_set(self):
        if not has_signal_fix():
            self.skipTest("base sin arreglo")
        q = circuit_d5(True, True, True)
        set_laser(q, True)
        q.settle(require_empty=False)
        e_ls = q.edges["e_ls"]
        e_ls.m_SignalHigh = False
        q.mark("swL", DIRTY_INTERNAL)
        q.process("swL")
        q.step3_log = []
        q.entities["cell"].m_LatchState = False
        q.entities["cell"].m_CellActive = False
        q.dirty_mask["cell"] = 0
        q.nodes["cell"].m_InQueue = False
        if "cell" in q.queue:
            q.queue.remove("cell")
        e_ls.m_SignalHigh = True
        q.mark("swL", DIRTY_INTERNAL)
        q.process("swL")
        sw_passes = [t for t in q.step3_log if t[0] == "swL"]
        self.assertTrue(sw_passes, "ancla (ii) swL no corre log=%s" % (q.step3_log,))
        found = None
        for _nid, trig, _ep in sw_passes:
            others = (
                trig.get("outputDelta")
                or trig.get("forceDownstream")
                or trig.get("m_AllocChanged")
                or trig.get("inputChanged")
                or trig.get("demandBecameKnown")
            )
            if trig.get("m_SignalChanged") and not others:
                found = trig
                break
        self.assertIsNotNone(
            found,
            "ancla (ii) m_SignalChanged en swL con los otros cinco apagados step3=%s"
            % (sw_passes,),
        )
        self.assertTrue(
            (q.dirty_mask.get("cell", 0) & DIRTY_INPUT) or "cell" in q.queue,
            "ancla (ii) swL marca la celda por m_SignalChanged",
        )
        q.process("cell")
        self.assertTrue(q.entities["cell"].m_LatchState)

    def test_cell_power_vs_signal(self):
        if not has_signal_fix():
            self.skipTest("base sin arreglo")
        q = circuit_d5(True, True, True)
        set_laser(q, True)
        self.assertTrue(q.port_receiving_signal("cell", PORT_IN3))
        q.edges["e_pc"].m_AllocatedPower = 0.0
        self.assertFalse(q.port_receiving_power("cell", PORT_IN0))
        self.assertTrue(q.port_receiving_signal("cell", PORT_IN0))
        cell = q.entities["cell"]
        cell.setpowered(True)
        self.assertTrue(cell.m_LatchState)
        self.assertFalse(cell.m_RawPower, "Power con asignacion 0 no da permiso")

    def test_d5_first_pass_open(self):
        if not has_signal_fix():
            self.skipTest("base sin arreglo")
        q = circuit_d5(True, False, True)
        q.nodes["laser"].m_GateClosed = True
        q.entities["laser"].on = True
        q.nodes["laser"].live_gate_open = True
        q.process("laser")
        self.assertTrue(q.last_signal_out.get("laser"), "primera pasada abierta publica alta")
        self.assertTrue(q.edges["e_set"].m_SignalHigh)

    def test_xor_port_filter(self):
        if not has_signal_fix():
            self.skipTest("base sin arreglo")
        q = circuit_a("XOR", True, False, splitter=True, load=False, require_empty=False)
        self.assertTrue(q.port_receiving_signal("gate", PORT_IN0))
        self.assertFalse(q.port_receiving_signal("gate", PORT_IN1))
        self.assertTrue(gate_open(q))

    def test_boot_ctor_signal(self):
        if not has_signal_fix():
            self.skipTest("base sin arreglo")
        h = helpers()
        q = SignalModel(h, order="fifo", scripted=[])
        q.add_node("G", SOURCE, 50)
        q.add_node("c", CONSUMER, 0, 1)
        q.add_edge("e", "G", "c")
        self.assertFalse(q.edges["e"].m_SignalHigh)

    def test_race_and_mb_enqueue_bit(self):
        if not has_signal_fix():
            self.skipTest("base sin arreglo")
        h = helpers()
        q = SignalModel(h, order="scripted", scripted=["gate", "cell", "swB"])
        q.add_node("G", SOURCE, 50)
        q.nodes["G"].m_Powered = True
        add_splitter(q, "spl")
        q.add_node("swA", PASSTHROUGH, 50, gated=True)
        q.add_node("swB", PASSTHROUGH, 50, gated=True)
        q.add_node("gate", PASSTHROUGH, 100, gated=True)
        q.add_node("Gp", SOURCE, 50)
        q.nodes["Gp"].m_Powered = True
        q.add_node("cell", PASSTHROUGH, 100, cons=1.0)
        q.add_edge("e_gs", "G", "spl", tport="input_1")
        q.add_edge("e_sa", "spl", "swA", sport=PORT_OUT1)
        q.add_edge("e_sb", "spl", "swB", sport=PORT_OUT2)
        q.add_edge("e_a0", "swA", "gate", tport=PORT_IN0)
        q.add_edge("e_b1", "swB", "gate", tport=PORT_IN1)
        q.add_edge("e_pc", "Gp", "cell", tport=PORT_IN0)
        q.attach_switch("swA", True)
        q.attach_switch("swB", True)
        q.attach_gate("gate", "AND")
        q.attach_cell("cell")
        q.order_mode = "scripted"
        q.scripted = ["gate", "cell", "swB"]
        q.settle(list(q.nodes.keys()), require_empty=False)
        q.signal_reads = []
        q.entities["swB"].on = False
        q.nodes["swB"].live_gate_open = False
        q.mark("swB", DIRTY_INTERNAL)
        q.add_edge("e_set", "gate", "cell", tport=PORT_IN3)
        q.mark("gate", DIRTY_TOPOLOGY)
        q.mark("cell", DIRTY_TOPOLOGY)
        q.settle(require_empty=False)
        b_reads = [
            r for r in q.signal_reads
            if r[0] == "gate" and r[1] == PORT_IN1
        ]
        self.assertTrue(b_reads, "AND tiene que leer B orden [AND,M,B]")
        self.assertTrue(b_reads[0][2], "AND lee B alto: el proveedor aun no ha escrito")


def _run():
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromModule(sys.modules[__name__])
    count = suite.countTestCases()
    if count == 0:
        print("FAIL: 0 tests discovered")
        return 1
    runner = unittest.TextTestRunner(verbosity=2, stream=sys.stderr)
    result = runner.run(suite)
    print("tests=%s failures=%s errors=%s skipped=%s" % (
        count, len(result.failures), len(result.errors), len(result.skipped)))
    for msg in _ANNOTATED:
        print(msg)
    for msg in _SKIPPED:
        print("SKIP:", msg)
    if result.errors:
        print("ERROR: harness/extraction raised; treat as invalid, exit 1")
        return 1
    if result.failures:
        return 1
    return 0


if __name__ == "__main__":
    try:
        sys.exit(_run())
    except Exception:
        traceback.print_exc()
        sys.exit(1)
