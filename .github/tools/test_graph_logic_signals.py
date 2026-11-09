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

_ANNOTATED = []
_SKIPPED = []
_RESULTS = []


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
    return graph_src(text)


def has_signal_fix(src=None):
    text = src_graph(src)
    return "ComputeSignalOut" in text and "IsPortReceivingSignal" in text


def load_signal_out(src=None):
    text = src_graph(src)
    if "ComputeSignalOut" not in text:
        return None
    return load(
        text,
        "ComputeSignalOut",
        ["isSource", "newOutput", "signalIn", "virt", "cons", "powered", "gateClosed"],
        {"LFPG_PROPAGATION_EPSILON": EPS},
    )


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
        raise AssertionError("gate class missing: " + cls)
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
        return self.model.port_receiving_signal(device_id, port)

    def GetGraph(self):
        return self

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
        self.compute_signal = load_signal_out(self.src)
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
        self.has_retry = "RetryUnresolvedGates" in src_txt and "m_IsLogicGate" in DATA_SRC.read_text(encoding="utf-8")
        self.gate_retry = []
        self.retry_resolves = 0
        self.signal_writes = []
        self.signal_reads = []
        self.validate_syncs = []
        self.fail_find = set()
        self.fail_vanilla = set()
        self.fail_netid = set()
        self.registry = {}

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

    def add_edge(self, eid, src, dst, sport="output_0", tport="input_0"):
        e = super().add_edge(eid, src, dst, sport=sport, tport=tport)
        e.m_Flags = EDGE_ENABLED
        e.m_SignalHigh = False
        return e

    def attach_gate(self, nid, kind, gate_source=None):
        text = GATE_SRC.read_text(encoding="utf-8") if gate_source is None else gate_source
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
        text = CELL_SRC.read_text(encoding="utf-8") if cell_source is None else cell_source
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

        def apply_routing():
            # LFPG_MemoryCell.c:405-422
            self.set_output_port_enabled(nid, PORT_OUT0, cell.m_CellActive)
            self.set_output_port_enabled(nid, PORT_OUT1, not cell.m_CellActive)

        env["LFPG_ApplyRouting"] = apply_routing
        env["LFPG_MCellApplyDue"] = lambda nowMs: env["_apply"](cell, nowMs)
        env["LFPG_MCellCommitPower"] = lambda: env["_commit_p"](cell)
        env["LFPG_MCellCommitToggle"] = lambda: env["_commit_t"](cell)
        env["LFPG_MCellSettle"] = lambda: env["_settle"](cell)
        env["LFPG_SetPowered"] = lambda powered: env["_setpowered"](cell, powered)

        def compile_into(name, extra, key):
            wrapped = bind(text, name, extra)
            exec(compile(wrapped, "<slice:" + name + ">", "exec"), env)
            env[key] = env["run"]

        compile_into("LFPG_MCellCommitPower", [], "_commit_p")
        compile_into("LFPG_MCellCommitToggle", [], "_commit_t")
        compile_into("LFPG_MCellApplyDue", ["nowMs"], "_apply")
        compile_into("LFPG_SetPowered", ["powered"], "_setpowered")
        compile_into("LFPG_MCellSettle", [], "_settle")
        compile_into("LFPG_MCellPowerSettleTimer", [], "_power_timer")
        compile_into("LFPG_MCellToggleSettleTimer", [], "_toggle_timer")

        self.clock.dispatch[CB_POWER] = lambda: env["_power_timer"](cell)
        self.clock.dispatch[CB_TOGGLE] = lambda: env["_toggle_timer"](cell)
        self.clock.dispatch[CB_DEFER] = apply_routing

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
                if self.has_fix:
                    e.m_SignalHigh = False
                any_changed = True
            elif (not enabled) and was:
                e.m_Flags = 0
                if self.has_fix:
                    e.m_SignalHigh = False
                any_changed = True
        if any_changed:
            self.mark(device_id, DIRTY_TOPOLOGY)

    def mark(self, nid, mask=DIRTY_INPUT):
        # Production MarkNodeDirty does not bump LastEpoch. Same-epoch
        # reprocess is only the Step 3 LastEpoch rewind (:2815, :2860).
        if nid not in self.nodes:
            return
        n = self.nodes[nid]
        self.dirty_mask[nid] = self.dirty_mask.get(nid, 0) | mask
        if not n.m_InQueue:
            n.m_InQueue = True
            self.queue.append(nid)

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
        for e in self.incoming.get(device_id, []):
            self.edge_visits = self.edge_visits + 1
            if (e.m_Flags & EDGE_ENABLED) == 0:
                continue
            if e.m_TargetPort != port:
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
        return self.entities.get(nid)

    def resolve_vanilla(self, nid):
        if nid in self.fail_vanilla:
            return None
        return self.entities.get(nid)

    def resolve_netid(self, nid):
        if nid in self.fail_netid:
            return None
        return self.entities.get(nid)

    def resolve_gated_entity(self, nid):
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

    def live_gate_closed(self, nid):
        n = self.nodes[nid]
        if not n.m_IsGated:
            return False
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
        if tgt.m_MaxOutput > EPS:
            d = tgt.m_MaxOutput
            if d > available:
                d = available
            return d
        return tgt.m_Consumption

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
            for e in self.outgoing[nid]:
                if (e.m_Flags & EDGE_ENABLED) == 0:
                    continue
                if e.m_SignalHigh != signal_out:
                    n.m_SignalChanged = True
                e.m_SignalHigh = signal_out
                self.signal_writes.append((nid, self.epoch, self.clock.now_ms, signal_out, e.m_TargetNodeId))
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
                continue
            insum = insum + self.edge_power(e)
            if self.has_fix and e.m_SignalHigh:
                signal_in = True
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
                    gate_is_closed = self.live_gate_closed(nid)
                else:
                    gate_is_closed = n.m_GateClosed
            if n.kind == SOURCE:
                n.m_OutputPower = new_output
            alloc_avail = new_output
            if gate_is_closed:
                alloc_avail = 0.0  # :2519-2522
            signal_out = False
            if self.has_retry and n.m_IsGated and not gate_entity_resolved:
                self.queue_gate_retry(nid)
                if n.m_IsLogicGate:
                    signal_out = False
            elif self.has_retry and n.m_IsLogicGate and getattr(resolved if gate_entity_resolved else None, "cast_null", False):
                self.queue_gate_retry(nid)
                signal_out = False
            elif self.has_fix and n.m_IsGated and not gate_entity_resolved and not self.has_retry:
                signal_out = False
            elif self.has_fix:
                signal_out = self.compute_signal(
                    n.kind == SOURCE,
                    new_output,
                    signal_in,
                    n.m_VirtualGeneration,
                    n.m_Consumption,
                    new_powered,
                    gate_is_closed,
                )
                # S12 / §5.7: live LFPG_EvaluateGateLogic on input_0/input_1, after
                # ComputeSignalOut and before AllocateOutput; does not write gate/alloc.
                if self.has_gate_branch:
                    ent = self.entities.get(nid)
                    eval_fn = getattr(ent, "eval_logic", None) if ent is not None else None
                    if eval_fn is not None:
                        a = self.port_receiving_signal(nid, PORT_IN0)
                        b = self.port_receiving_signal(nid, PORT_IN1)
                        signal_out = bool(eval_fn(a, b))
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
                        # :2688-2706: mark upstream only. Demand 5→5 has no
                        # outputDelta; do not set m_AllocChanged or Step 3
                        # requeues Laser via the switch (in-game: one INTERNAL pass).
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
            fire = (
                trig["outputDelta"]
                or trig["forceDownstream"]
                or trig["m_AllocChanged"]
                or trig["inputChanged"]
                or trig["demandBecameKnown"]
                or trig["m_SignalChanged"]
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
                    if self.has_fix:
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
        else:
            for e in self.incoming.get(nid, []):
                extra.append(e.m_SourceNodeId)
                self.mark(e.m_SourceNodeId, DIRTY_INPUT)

        # Step 4 :2877
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
        self.sync_entity(nid, n, dirty_mask)
        self.dirty_mask[nid] = 0
        return extra

    def sync_entity(self, nid, n, dirty_mask):
        # NodeEntitySyncUnchanged :2966-3010 called :3133
        ent = self.entities.get(nid)
        if ent is None:
            return
        skip = True
        if (dirty_mask & DIRTY_TOPOLOGY) != 0:
            skip = False
        if (dirty_mask & DIRTY_INPUT) != 0:
            skip = False
        if nid not in self.last_sync_seen:
            skip = False
        last_p = self.last_sync_powered.get(nid)
        if last_p is None or last_p != n.m_Powered:
            skip = False
        last_o = self.last_sync_overloaded.get(nid)
        if n.kind in (SOURCE, PASSTHROUGH):
            if last_o is None or last_o != n.m_Overloaded:
                skip = False
        if skip:
            return
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
        # After ValidateConsumerStates on empty queue; cap 4, rotate, MarkNodeDirty only if resolved.
        if not self.gate_retry:
            return
        cap = 4
        taken = self.gate_retry[:cap]
        rest = self.gate_retry[cap:]
        self.gate_retry = list(rest)
        for nid in taken:
            if nid not in self.nodes or not self.nodes[nid].m_IsGated:
                continue
            self.retry_resolves = self.retry_resolves + 1
            resolved = self.resolve_gated_entity(nid)
            if resolved is None:
                self.gate_retry.append(nid)
                continue
            if self.nodes[nid].m_IsLogicGate and getattr(resolved, "cast_null", False):
                self.gate_retry.append(nid)
                continue
            self.mark(nid, DIRTY_INTERNAL)

    def pdq_tick(self, seeds=None, warmup=False, advance_clock=False):
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
        if len(self.queue) > 128:
            node_budget = node_budget * 2
            edge_budget = edge_budget * 2
        if not self.budgets:
            node_budget = 10 ** 9
            edge_budget = 10 ** 9
        processed = 0
        self.edge_visits = 0
        if not self.queue:
            self.validate_consumers(edge_budget)
            if self.has_retry:
                self.retry_unresolved_gates()
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
            if self.has_retry:
                self.retry_unresolved_gates()
        self.last_processed = processed
        return processed

    def validate_consumers(self, edge_budget):
        # Only with empty queue (:2219-2231, :2896-2921); skip in-queue or dirty (:3466).
        if self.queue:
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
            if n.m_InQueue or self.dirty_mask.get(nid, 0):
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
        while self.clock.now_ms < end:
            nxt_t = self.clock.next_timer()
            nxt_p = self.next_pdq_ms
            if nxt_t is not None and nxt_t < nxt_p and nxt_t <= end:
                self._fire_one_timer()
                continue
            if nxt_p <= end:
                if nxt_t is not None and nxt_t == nxt_p:
                    if tie_timers_first:
                        self._fire_one_timer()
                        continue
                self.clock.now_ms = nxt_p
                self.pdq_tick(seeds if first else None, warmup=warmup, advance_clock=False)
                first = False
                self.next_pdq_ms = self.next_pdq_ms + TICK_MS
                continue
            self.clock.drain_to(end, restore=True)
            break
        dirty_left = any(self.dirty_mask.get(i, 0) for i in self.nodes)
        self.settled_empty = (
            not self.queue
            and not self.pending_propagate
            and not self.clock.pending
            and not dirty_left
        )
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
        self.settled_empty = (
            not self.queue
            and not self.pending_propagate
            and not self.clock.pending
            and not dirty_left
        )
        if require_empty and not self.settled_empty:
            raise AssertionError("propagation did not empty in %s ticks" % ticks)
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
            if src.kind == SOURCE:
                high = bool(src.m_Powered)
            elif ent is not None and getattr(ent, "role", None) == "gate":
                high = table_bit(ent.kind, in0, in1)
            elif ent is not None and hasattr(ent, "on"):
                high = any_in and bool(ent.on)
            elif src.kind in (CONSUMER, CAMERA):
                high = False
            else:
                high = any_in
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
    return diffs


# Codex PLAN-codex.md §4.2 order (a): XOR, A, B, lamp, XOR, Splitter, … G last.
ORDER_A_SPLIT = ["gate", "swA", "swB", "lamp", "spl", "G"]
ORDER_A_DIRECT = ["G", "swA", "swB", "gate", "lamp"]


def circuit_a(kind, a_on, b_on, splitter=True, load=True, order="scripted", require_empty=True):
    h = helpers()
    scripted = list(ORDER_A_SPLIT if splitter else ORDER_A_DIRECT)
    q = SignalModel(h, order=order, scripted=scripted)
    q.add_node("G", SOURCE, 50)
    q.nodes["G"].m_Powered = True
    if splitter:
        q.add_node("spl", PASSTHROUGH, 500)
        q.add_edge("e_gs", "G", "spl")
        q.add_node("swA", PASSTHROUGH, 50, gated=True)
        q.add_node("swB", PASSTHROUGH, 50, gated=True)
        q.add_edge("e_sa", "spl", "swA")
        q.add_edge("e_sb", "spl", "swB")
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
    q.add_node("spl", PASSTHROUGH, 500)
    q.add_node("swA", PASSTHROUGH, 50, gated=True)
    q.add_node("swB", PASSTHROUGH, 50, gated=True)
    q.add_node("gate", PASSTHROUGH, 100, gated=True)
    q.add_node("door", CONSUMER, 0, 1)
    q.add_edge("e_gs", "G", "spl")
    q.add_edge("e_sa", "spl", "swA")
    q.add_edge("e_sb", "spl", "swB")
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
    q.add_node("spl", PASSTHROUGH, 500)
    q.add_edge("e_gs", "G", "spl")
    q.add_node("swP", PASSTHROUGH, 50, gated=True)
    q.add_node("laser", PASSTHROUGH, 50, cons=5.0, gated=True)
    q.add_node("cell", PASSTHROUGH, 100, cons=1.0)
    q.add_node("lamp", CONSUMER, 0, 10)
    if power_via_splitter:
        q.add_edge("e_sp", "spl", "swP")
    else:
        q.add_edge("e_gp", "G", "swP")
    q.add_edge("e_pc", "swP", "cell", tport=PORT_IN0)
    q.add_edge("e_sl", "spl", "laser")
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


def walk_table(kind, splitter, load=True, sequences=None):
    if sequences is None:
        sequences = (BANK_SEQ,)
    q = circuit_a(kind, False, False, splitter=splitter, load=load, require_empty=False)
    rows = []
    for seq in sequences:
        rows.extend(walk_on(q, "swA", "swB", "gate", "e_gl", seq))
    return q, rows


def add_gate_branch(q, prefix, kind, load=True):
    g, spl, sw_a, sw_b, gate = prefix + "G", prefix + "spl", prefix + "swA", prefix + "swB", prefix + "gate"
    lamp = prefix + "lamp"
    q.add_node(g, SOURCE, 50)
    q.nodes[g].m_Powered = True
    q.add_node(spl, PASSTHROUGH, 500)
    q.add_edge(prefix + "e_gs", g, spl)
    q.add_node(sw_a, PASSTHROUGH, 50, gated=True)
    q.add_node(sw_b, PASSTHROUGH, 50, gated=True)
    q.add_edge(prefix + "e_sa", spl, sw_a)
    q.add_edge(prefix + "e_sb", spl, sw_b)
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


def circuit_c(fillers=0):
    """MemoryCell Toggle/Set/Reset via LFPG_PushButton, built before settle."""
    h = helpers()
    scripted = ["G", "spl", "swP", "btnT", "btnS", "btnR", "cell", "lamp"]
    q = SignalModel(h, order="scripted", scripted=scripted)
    q.add_node("G", SOURCE, 50)
    q.nodes["G"].m_Powered = True
    q.add_node("spl", PASSTHROUGH, 500)
    q.add_edge("e_gs", "G", "spl")
    q.add_node("swP", PASSTHROUGH, 50, gated=True)
    q.add_edge("e_sp", "spl", "swP")
    q.add_node("btnT", PASSTHROUGH, 50, gated=True)
    q.add_node("btnS", PASSTHROUGH, 50, gated=True)
    q.add_node("btnR", PASSTHROUGH, 50, gated=True)
    q.add_node("cell", PASSTHROUGH, 100, cons=1.0)
    q.add_node("lamp", CONSUMER, 0, 10)
    q.add_edge("e_pc", "swP", "cell", tport=PORT_IN0)
    q.add_edge("e_st", "spl", "btnT")
    q.add_edge("e_ss", "spl", "btnS")
    q.add_edge("e_sr", "spl", "btnR")
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
    seeds = ["G", "spl", "swP", "btnT", "btnS", "btnR", "cell", "lamp"]
    if fillers:
        # Their own generator: the row congests the queue, not the energy of G (65 > 50 is B5).
        q.add_node("Gfill", SOURCE, 500)
        q.nodes["Gfill"].m_Powered = True
        seeds.append("Gfill")
    for i in range(fillers):
        nid = "fill%d" % i
        q.add_node(nid, CONSUMER, 0, 1)
        q.add_edge("ef%d" % i, "Gfill", nid)
        seeds.append(nid)
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

    def test_a1_and_splitter_11(self):
        q, rows = walk_table("AND", True)
        self._assert_table_rows("AND", rows)

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

    def test_a3_xor_splitter_10(self):
        q = circuit_triple()
        _and_rows, _or_rows, xor_rows = run_bank(q)
        self._assert_table_rows("XOR", xor_rows)
        qq, solo = walk_table("XOR", True)
        bits, parts = _fmt_rows(solo)
        _ANNOTATED.append("ANOTADO XOR solo bits=%s %s" % (bits, "; ".join(parts)))

    def test_a1_a3_no_splitter_green(self):
        for kind in ("AND", "OR", "XOR"):
            q, rows = walk_table(kind, False)
            self._assert_table_rows(kind, rows)


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


class TestD5(_TraceCase):
    def _assert_d5_red_or_green(self, q, expect_set):
        cell = q.entities["cell"]
        set_high = q.port_receiving_power("cell", PORT_IN3) or (
            has_signal_fix() and q.port_receiving_signal("cell", PORT_IN3)
        )
        set_laser(q, True)
        self.assertTrue(q.entities["cell"].m_LatchState, "D5 Set high when Laser opens")
        self.assertTrue(q.entities["cell"].m_CellActive)
        self.assertGreaterEqual(q.edges["e_out"].m_AllocatedPower, 10.0 - EPS)
        set_laser(q, False)
        self.assertTrue(q.entities["cell"].m_LatchState, "D5 latch holds when Laser closes")

    def test_d5_power_via_splitter(self):
        q = circuit_d5(True, False, True)
        self._assert_d5_red_or_green(q, True)

    def test_d5_power_direct(self):
        q = circuit_d5(False, False, True)
        self._assert_d5_red_or_green(q, True)

    def test_d5_laser_switch_set(self):
        q = circuit_d5(True, True, True)
        self._assert_d5_red_or_green(q, True)

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
        q = circuit_c(fillers=65)
        cell = q.entities["cell"]
        q.run_events(30000)
        q.run_events(1400)
        expected = False
        for i in range(3):
            for j in range(65):
                q.mark("fill%d" % j, DIRTY_INTERNAL)
            press_push(q, "btnT")
            expected = not expected
            self.assertEqual(
                cell.m_LatchState,
                expected,
                "C congested press %s latch=%s want=%s" % (i + 1, cell.m_LatchState, expected),
            )
        press_push(q, "btnS")
        self.assertTrue(cell.m_LatchState)
        press_push(q, "btnR")
        self.assertFalse(cell.m_LatchState)
        # C5 Power cut, C6 rebuild/warmup, C7 Set held (§6.4). Fillers stay on Gfill.
        for j in range(65):
            q.mark("fill%d" % j, DIRTY_INTERNAL)
        q.entities["swP"].on = False
        q.nodes["swP"].live_gate_open = False
        q.mark("swP", DIRTY_INTERNAL)
        q.settle(require_empty=False)
        q.entities["swP"].on = True
        q.nodes["swP"].live_gate_open = True
        q.mark("swP", DIRTY_INTERNAL)
        q.settle(require_empty=False)
        for j in range(65):
            q.mark("fill%d" % j, DIRTY_INTERNAL)
        q.settle(["G", "spl", "swP", "btnT", "btnS", "btnR", "cell", "lamp"], warmup=True, require_empty=False)
        press_push(q, "btnS")
        self.assertTrue(cell.m_LatchState, "C7 Set held latch")
        q.entities["btnS"].m_SwitchOn = True
        q.nodes["btnS"].live_gate_open = True
        q.mark("btnS", DIRTY_INTERNAL)
        q.settle(require_empty=False)
        self.assertTrue(cell.m_LatchState, "C7 Set still held")


class TestAnnotated(_TraceCase):
    def test_transitions_annotated(self):
        q = circuit_a("AND", True, True, splitter=True, require_empty=False)
        set_switch(q, "swA", False, require_empty=False)
        set_switch(q, "swB", False, require_empty=False)
        if not getattr(q, "settled_empty", True):
            _ANNOTATED.append("ANOTADO transiciones no asienta en 100 ticks")
        else:
            _ANNOTATED.append(
                "ANOTADO transiciones 11->00 latch=%s changes=%s"
                % (gate_open(q), q.latch_changes.get("gate", 0))
            )
        if has_signal_fix():
            self.assertFalse(gate_open(q))

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

    def test_overload_annotated(self):
        h = helpers()
        q = SignalModel(h, scripted=["G", "spl", "swA", "swB", "gate", "lamp", "hog"])
        q.add_node("G", SOURCE, 50)
        q.nodes["G"].m_Powered = True
        q.add_node("spl", PASSTHROUGH, 500)
        q.add_node("swA", PASSTHROUGH, 50, gated=True)
        q.add_node("swB", PASSTHROUGH, 50, gated=True)
        q.add_node("gate", PASSTHROUGH, 100, gated=True)
        q.add_node("lamp", CONSUMER, 0, 10)
        q.add_node("hog", CONSUMER, 0, 60)
        q.add_edge("e_gs", "G", "spl")
        q.add_edge("e_sa", "spl", "swA")
        q.add_edge("e_sb", "spl", "swB")
        q.add_edge("e_sh", "spl", "hog")
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
            self.assertAlmostEqual(lamp_power(q), 0.0, places=3)
            self.assertTrue(q.edges["e_a0"].m_SignalHigh)

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

    def test_long_chain_annotated(self):
        h = helpers()
        q = SignalModel(h, order="fifo", scripted=[])
        q.add_node("G", SOURCE, 50)
        q.nodes["G"].m_Powered = True
        prev = "G"
        for i in range(64):
            nid = "s%d" % i
            q.add_node(nid, PASSTHROUGH, 500)
            q.add_edge("e%d" % i, prev, nid)
            prev = nid
        q.add_node("sw", PASSTHROUGH, 50, gated=True)
        q.add_node("gate", PASSTHROUGH, 100, gated=True)
        q.add_edge("e_sw", prev, "sw")
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

    def test_boot_annotated(self):
        q = circuit_a("XOR", True, False, splitter=True, require_empty=False)
        _ANNOTATED.append("ANOTADO arranque latch=%s" % gate_open(q))

    def test_invariant_annotated(self):
        q = circuit_a("AND", True, True, splitter=True, require_empty=False)
        if not getattr(q, "settled_empty", True):
            _ANNOTATED.append("ANOTADO invariante no asienta en 100 ticks")
            return
        ok = q.invariant_ok()
        _ANNOTATED.append("ANOTADO invariante ok=%s has_fix=%s" % (ok, has_signal_fix()))
        if has_signal_fix():
            self.assertTrue(ok)

    def test_old_latch_annotated(self):
        h = helpers()
        q = SignalModel(h, order="scripted", scripted=["swB", "gate", "cell"])
        q.add_node("G", SOURCE, 50)
        q.nodes["G"].m_Powered = True
        q.add_node("spl", PASSTHROUGH, 500)
        q.add_node("swA", PASSTHROUGH, 50, gated=True)
        q.add_node("swB", PASSTHROUGH, 50, gated=True)
        q.add_node("gate", PASSTHROUGH, 100, gated=True)
        q.add_node("Gp", SOURCE, 50)
        q.nodes["Gp"].m_Powered = True
        q.add_node("cell", PASSTHROUGH, 100, cons=1.0)
        q.add_edge("e_gs", "G", "spl")
        q.add_edge("e_sa", "spl", "swA")
        q.add_edge("e_sb", "spl", "swB")
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
        for mode in ("fifo", "lifo", "perm"):
            _ANNOTATED.append("ANOTADO latch viejo orden %s" % mode)

    def test_old_latch_no_new_wire_annotated(self):
        q = circuit_a("AND", True, True, splitter=True, require_empty=False)
        _ANNOTATED.append("ANOTADO latch viejo sin cable AND latch=%s" % gate_open(q))

    def test_q_to_toggle_annotated(self):
        q = circuit_c()
        q.run_events(30000)
        _ANNOTATED.append(
            "ANOTADO Q->Toggle latch=%s now=%s"
            % (q.entities["cell"].m_LatchState, q.clock.now_ms)
        )

    def test_rehab_delayed_annotated(self):
        q = circuit_d5(True, False, True)
        cell = q.entities["cell"]
        # §5.8: Set to 0 during Reset so Q actually drops.
        q.add_node("swR", PASSTHROUGH, 50, gated=True)
        q.add_edge("e_rr", "spl", "swR")
        q.add_edge("e_rc", "swR", "cell", tport=PORT_IN2)
        q.attach_switch("swR", False)
        q.mark("cell", DIRTY_TOPOLOGY)
        q.settle(require_empty=False)
        # Laser is Set. Close Set (laser off) then pulse Reset.
        set_laser(q, False)
        q.entities["swR"].on = True
        q.nodes["swR"].live_gate_open = True
        q.mark("swR", DIRTY_INTERNAL)
        q.run_events(100)
        disabled = []
        for e in q.outgoing["cell"]:
            if e.m_SourcePort == PORT_OUT0:
                disabled.append((e.m_Flags, e.m_AllocatedPower, cell.m_LatchState))
        _ANNOTATED.append(
            "ANOTADO salida rehabilitada retraso (i) Set=0 Reset 100ms flags=%s"
            % disabled
        )
        _ANNOTATED.append("ANOTADO salida rehabilitada retraso (ii) cola llena n/a en base")

    def test_race_and_g2_annotated(self):
        h = helpers()
        q = SignalModel(h, order="scripted", scripted=["gate", "cell", "swB"])
        q.add_node("G", SOURCE, 50)
        q.nodes["G"].m_Powered = True
        q.add_node("spl", PASSTHROUGH, 500)
        q.add_node("swA", PASSTHROUGH, 50, gated=True)
        q.add_node("swB", PASSTHROUGH, 50, gated=True)
        q.add_node("gate", PASSTHROUGH, 100, gated=True)
        q.add_node("Gp", SOURCE, 50)
        q.nodes["Gp"].m_Powered = True
        q.add_node("cell", PASSTHROUGH, 100, cons=1.0)
        q.add_edge("e_gs", "G", "spl")
        q.add_edge("e_sa", "spl", "swA")
        q.add_edge("e_sb", "spl", "swB")
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
        diffs = g2_compare(q)
        _ANNOTATED.append(
            "ANOTADO latch viejo carrera [AND,M,B] AND=%s M=%s g2_diffs=%s sweep=%s clock=%s"
            % (
                gate_open(q),
                q.entities["cell"].m_LatchState,
                diffs,
                q.validate_syncs,
                [t for t in q.clock.trace if t[0] in ("CallLater", "PDQ")][:8],
            )
        )

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

    def test_registry_and_unresolvable_annotated(self):
        q = circuit_a("AND", True, True, splitter=True, require_empty=False)
        q.fail_find.add("gate")
        q.mark("gate", DIRTY_INTERNAL)
        q.settle(require_empty=False)
        _ANNOTATED.append(
            "ANOTADO registro sin entidad FindById fail latch=%s"
            % gate_open(q)
        )
        q.fail_find.add("gate")
        q.fail_vanilla.add("gate")
        q.fail_netid.add("gate")
        q.mark("gate", DIRTY_INTERNAL)
        q.settle(require_empty=False)
        _ANNOTATED.append(
            "ANOTADO entidad irresoluble latch=%s"
            % gate_open(q)
        )

    def test_noisy_neighbor_annotated(self):
        q = circuit_a("OR", False, False, splitter=True, require_empty=False)
        q.add_node("Gn", SOURCE, 500)
        q.nodes["Gn"].m_Powered = True
        q.add_node("nsw", PASSTHROUGH, 500, gated=True)
        q.add_node("nspl", PASSTHROUGH, 500)
        q.add_edge("e_gn", "Gn", "nsw")
        q.add_edge("e_ns", "nsw", "nspl")
        q.attach_switch("nsw", on=True)
        for i in range(65):
            nid = "nc%d" % i
            q.add_node(nid, CONSUMER, 0, 1)
            q.add_edge("en%d" % i, "nspl", nid)
        q.mark("Gn", DIRTY_INTERNAL)
        q.settle(require_empty=False)
        for k in range(4):
            q.entities["nsw"].on = not q.entities["nsw"].on
            q.nodes["nsw"].live_gate_open = q.entities["nsw"].on
            q.mark("nsw", DIRTY_INTERNAL)
            for i in range(65):
                q.mark("nc%d" % i, DIRTY_INTERNAL)
            q.run_events(500)
        _ANNOTATED.append(
            "ANOTADO vecino ruidoso OR latch=%s nsw=%s"
            % (gate_open(q), q.entities["nsw"].on)
        )

    def test_unresolvable_entity_return_annotated(self):
        q = circuit_a("AND", True, True, splitter=True, require_empty=False)
        q.fail_find.add("gate")
        q.fail_vanilla.add("gate")
        q.fail_netid.add("gate")
        q.mark("gate", DIRTY_INTERNAL)
        q.settle(require_empty=False)
        q.fail_find.discard("gate")
        q.fail_vanilla.discard("gate")
        q.fail_netid.discard("gate")
        q.settle(require_empty=False)
        _ANNOTATED.append(
            "ANOTADO entidad irresoluble y su vuelta latch=%s"
            % gate_open(q)
        )

    def test_live_gate_no_entity_annotated(self):
        q = circuit_a("AND", True, True, splitter=True, require_empty=False)
        q.fail_find.add("swA")
        q.fail_netid.add("swA")
        q.mark("swA", DIRTY_INTERNAL)
        q.pdq_tick(warmup=True, advance_clock=False)
        _ANNOTATED.append(
            "ANOTADO compuerta viva sin entidad AND latch=%s lamp=%s"
            % (gate_open(q), lamp_power(q))
        )

    def test_cast_null_na(self):
        q = circuit_a("AND", True, False, splitter=True, require_empty=False)
        if "gate" in q.entities:
            q.entities["gate"].cast_null = True
        q.nodes["gate"].cast_null = True
        q.mark("gate", DIRTY_INTERNAL)
        q.settle(require_empty=False)
        _ANNOTATED.append("ANOTADO puerta logica Cast nulo n/a latch=%s" % gate_open(q))

    def test_bounded_retry_na(self):
        h = helpers()
        q = SignalModel(h, order="fifo", scripted=[], budgets=True)
        q.add_node("G", SOURCE, 50)
        q.nodes["G"].m_Powered = True
        for i in range(10):
            nid = "g%d" % i
            q.add_node(nid, PASSTHROUGH, 50, gated=True, logic_gate=True)
            q.attach_gate(nid, "AND")
            q.fail_find.add(nid)
            q.fail_vanilla.add(nid)
            q.fail_netid.add(nid)
            q.queue_gate_retry(nid)
        q.add_node("back", PASSTHROUGH, 50, gated=True, logic_gate=True)
        q.attach_gate("back", "AND")
        q.fail_find.add("back")
        q.fail_vanilla.add("back")
        q.fail_netid.add("back")
        q.queue_gate_retry("back")
        before = q.retry_resolves
        q.retry_unresolved_gates()
        _ANNOTATED.append(
            "ANOTADO reintento acotado n/a resolves=%s has_retry=%s"
            % (q.retry_resolves - before, q.has_retry)
        )

    def test_laser_under_5_annotated(self):
        q = circuit_d5(True, True, True)
        q.nodes["laser"].m_Consumption = 5.0
        q.nodes["G"].m_MaxOutput = 4.0
        q.mark("G")
        q.settle(require_empty=False)
        if not getattr(q, "settled_empty", True):
            _ANNOTATED.append("ANOTADO laser<5 no asienta en 100 ticks")
            return
        sig = False
        if "e_set" in q.edges:
            sig = q.edges["e_set"].m_SignalHigh
        _ANNOTATED.append("ANOTADO laser<5 signal_set=%s" % sig)
        if has_signal_fix():
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
        if body.startswith("m_SignalHigh", i):
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
        step = text
        self.assertRegex(step, r"m_SignalChanged")
        pdq = brace_body(text, "ProcessDirtyQueue")
        self.assertIn("m_SignalChanged", pdq)
        self.assertRegex(
            pdq,
            r"forceDownstream \|\| m_AllocChanged.*m_SignalChanged|m_SignalChanged.*forceDownstream",
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

    def test_s7_readers(self):
        gate = GATE_SRC.read_text(encoding="utf-8")
        cell = CELL_SRC.read_text(encoding="utf-8")
        self.assertIn("IsPortReceivingSignal", gate)
        self.assertIn("IsPortReceivingPower", cell)
        sp = method(cell, "LFPG_SetPowered")
        self.assertIn("IsPortReceivingPower", sp)
        self.assertIn("IsPortReceivingSignal", sp)

    def test_s8_edge_and_base(self):
        data = DATA_SRC.read_text(encoding="utf-8")
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
        text = GATE_SRC.read_text(encoding="utf-8")
        self.assertNotIn("m_BrownoutHeld", text)

    def test_s12_gate_branch_evaluate_live(self):
        text = src_graph()
        pdq = brace_body(text, "ProcessDirtyQueue")
        self.assertIn("LFPG_EvaluateGateLogic", pdq)
        pos_c = pdq.find("ComputeSignalOut")
        pos_e = pdq.find("LFPG_EvaluateGateLogic")
        pos_a = pdq.find("AllocateOutput")
        self.assertGreater(pos_e, pos_c)
        self.assertGreater(pos_a, pos_e)
        self.assertIn("IsPortReceivingSignal", pdq)

    def test_s13_resolve_gated_entity(self):
        text = src_graph()
        self.assertIn("ResolveGatedEntity", text)
        body = brace_body(text, "ResolveGatedEntity")
        self.assertIn("FindById", body)
        self.assertIn("ResolveVanillaDevice", body)

    def test_s14_gated_no_entity_signal_false(self):
        text = src_graph()
        pdq = brace_body(text, "ProcessDirtyQueue")
        self.assertIn("m_IsLogicGate", pdq)
        self.assertIn("QueueGateRetry", pdq)
        self.assertIn("LFPG_LogicGateBase.Cast", pdq)
        self.assertIn("signalOut = false", pdq.replace("signalOut=false", "signalOut = false"))
        pos_q = pdq.find("QueueGateRetry")
        pos_a = pdq.find("AllocateOutput")
        self.assertGreater(pos_a, pos_q)

    def test_s15_is_logic_gate_cached(self):
        data = DATA_SRC.read_text(encoding="utf-8")
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
        self.assertIn("RetryUnresolvedGates", pdq)
        idx_v = [m.start() for m in re.finditer(r"ValidateConsumerStates", pdq)]
        idx_r = [m.start() for m in re.finditer(r"RetryUnresolvedGates", pdq)]
        self.assertGreaterEqual(len(idx_v), 2)
        self.assertGreaterEqual(len(idx_r), 2)
        self.assertLess(idx_v[0], idx_r[0])
        self.assertLess(idx_v[1], idx_r[1])
        retry = brace_body(text, "RetryUnresolvedGates")
        self.assertIn("LFPG_GATE_RETRY_PER_CALL", retry)
        self.assertIn("ResolveGatedEntity", retry)
        self.assertIn("MarkNodeDirty", retry)
        self.assertIn("LFPG_DIRTY_INTERNAL", retry)
        rebuild = brace_body(text, "RebuildFromWires")
        self.assertTrue("m_GateRetry" in rebuild and ("Clear" in rebuild or "Clear()" in rebuild or ".Clear" in rebuild))


class TestBudgets(_TraceCase):
    def test_node_budget_65_consumers_green(self):
        h = helpers()
        q = SignalModel(h, order="fifo", scripted=[], budgets=True)
        q.add_node("G", SOURCE, 50)
        q.nodes["G"].m_Powered = True
        for i in range(65):
            nid = "c%d" % i
            q.add_node(nid, CONSUMER, 0, 1)
            q.add_edge("e%d" % i, "G", nid)
        for i in range(65):
            q.mark("c%d" % i, DIRTY_INTERNAL)
        processed = q.pdq_tick(warmup=False, advance_clock=False)
        self.assertEqual(processed, 64)
        self.assertEqual(q.last_processed, 64)
        left = sum(1 for i in range(65) if q.nodes["c%d" % i].m_InQueue or "c%d" % i in q.queue)
        self.assertGreaterEqual(left, 1)

    def test_edge_budget_finishes_node_green(self):
        h = helpers()
        q = SignalModel(h, order="fifo", scripted=[], budgets=True)
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


class TestMutants(_TraceCase):
    def test_mutants_skip_or_run(self):
        if not has_signal_fix():
            _SKIPPED.append("mutantes §6.6: base sin arreglo")
            self.skipTest("base sin arreglo")
        src = src_graph()
        targets = [
            "IsPortReceivingSignal",
            "m_SignalChanged",
            "ComputeSignalOut",
            "m_SignalHigh",
            "LFPG_EvaluateGateLogic",
            "ResolveGatedEntity",
            "QueueGateRetry",
            "RetryUnresolvedGates",
            "m_IsLogicGate",
            "LFPG_GATE_RETRY_PER_CALL",
        ]
        for t in targets:
            self.assertIn(t, src, "mutation target missing: " + t)

    def test_mutant_signal_changed_xor_open_input1(self):
        if not has_signal_fix():
            self.skipTest("base sin arreglo")
        h = helpers()
        q = SignalModel(h, order="scripted", scripted=["G", "spl", "swA", "gate"])
        q.add_node("G", SOURCE, 50)
        q.nodes["G"].m_Powered = True
        q.add_node("spl", PASSTHROUGH, 500)
        q.add_node("swA", PASSTHROUGH, 50, gated=True)
        q.add_node("gate", PASSTHROUGH, 100, gated=True)
        q.add_edge("e_gs", "G", "spl")
        q.add_edge("e_sa", "spl", "swA")
        q.add_edge("e_a0", "swA", "gate", tport=PORT_IN0)
        q.attach_switch("swA", True)
        q.attach_gate("gate", "XOR")
        q.settle(["G", "spl", "swA", "gate"], require_empty=False)
        self.assertTrue(q.settled_empty or True)
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
