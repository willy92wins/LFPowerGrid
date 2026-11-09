"""Memory Cell settled Toggle: production slices plus a simulated CallLater queue."""
from pathlib import Path
from types import SimpleNamespace
import random
import re
import sys
import unittest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from enforce_scalar_slice import load, method, scalar_function

CELL = ROOT / "scripts/4_World/LFPG_MemoryCell.c"
FLIP_ARGS = ["lowSinceMs", "powerSinceMs", "graceUntilMs", "nowMs", "rearmMs"]
DUE_ARGS = ["raw", "settled", "changedMs", "nowMs", "debounceMs"]
FIELDS = [
    "m_PoweredNet", "m_LatchState", "m_CellActive", "m_RawPower", "m_RawToggle",
    "m_RawPowerChangedMs", "m_RawToggleChangedMs", "m_SettledPower",
    "m_SettledToggle", "m_FlipPending", "m_ToggleLowSinceMs", "m_PowerSinceMs",
    "m_ToggleGraceUntilMs", "m_DeviceId",
]
GAPS = [10, 50, 73, 100, 300, 399, 400, 401, 600, 999, 1000, 1001, 1400, 2000, 5000]
PORT_POWER = object()
PORT_TOGGLE = object()
PORT_RESET = object()
PORT_SET = object()
CB_POWER = object()
CB_TOGGLE = object()
CB_DEFER = object()


def src_text():
    return CELL.read_text(encoding="utf-8")


def const_int(source, name):
    match = re.search(r"\b" + name + r"\s*=\s*(-?\d+)", source)
    if not match:
        raise AssertionError("const missing: " + name)
    return int(match.group(1))


def member_init(source, name):
    match = re.search(r"\b" + re.escape(name) + r"\s*=\s*([^;]+);", source)
    if not match:
        raise AssertionError("member missing: " + name)
    raw = match.group(1).strip()
    if raw == "true":
        return True
    if raw == "false":
        return False
    return int(raw)


def neutralize(code):
    code = re.sub(r"#ifndef\s+SERVER.*?#endif", "", code, flags=re.S)
    code = re.sub(
        r"if\s*\(\s*LFPG_LOG_LEVEL\s*>=\s*2\s*\)\s*\{(?:[^{}]|\{[^{}]*\})*\}",
        "",
        code)
    code = re.sub(r"LFPG_Util\.\w+\s*\([^;]*\);", "", code)
    code = code.replace('"input_0"', "PORT_POWER")
    code = code.replace('"input_1"', "PORT_TOGGLE")
    code = code.replace('"input_2"', "PORT_RESET")
    code = code.replace('"input_3"', "PORT_SET")
    code = re.sub(r'"(?:\\.|[^"\\])*"', "EMPTY", code)
    code = re.sub(r"(\w+)\.ToString\(\)", r"str(\1)", code)
    code = re.sub(
        r"\b(?:string|vector|ref|ParamsReadContext|ParamsWriteContext|LFPG_[A-Za-z0-9_]+)\s+(?=[A-Za-z_]\w*\s*(?:=|;))",
        "",
        code)
    return code


def qualify_members(code):
    return re.sub(r"(?<![.\w])(m_[A-Za-z0-9_]+)\b", r"cell.\1", code)


def bind(source, name, extra):
    body = qualify_members(neutralize(method(source, name)))
    generated = scalar_function(body, extra)
    inner = generated.split("\n", 1)[1]
    header = "def run(cell"
    if extra:
        header = header + ", " + ", ".join(extra)
    header = header + "):\n"
    return header + inner + "\n"


class Queue:
    def __init__(self, delay_power, delay_toggle):
        self.pending = []
        self.delay_power = delay_power
        self.delay_toggle = delay_toggle
        self.now = 0

    def Remove(self, callback):
        self.pending = [item for item in self.pending if item[1] is not callback]

    def CallLater(self, callback, delay, repeat):
        extra = 0
        if callback is CB_POWER:
            extra = self.delay_power
        elif callback is CB_TOGGLE:
            extra = self.delay_toggle
        self.pending.append((self.now + delay + extra, callback))

    def next_due(self):
        if not self.pending:
            return None
        return min(self.pending, key=lambda item: item[0])


class Graph:
    def __init__(self, ports):
        self.ports = ports

    def IsPortReceivingSignal(self, device_id, port):
        return self.ports[port]


class NM:
    def __init__(self):
        self.ports = {
            PORT_POWER: False,
            PORT_TOGGLE: False,
            PORT_RESET: False,
            PORT_SET: False,
        }

    def IsPortReceivingPower(self, device_id, port):
        return self.ports[port]

    def GetGraph(self):
        return Graph(self.ports)


class Game:
    def __init__(self, queue):
        self.queue = queue
        self.now = 0

    def GetTime(self):
        return self.queue.now

    def GetCallQueue(self, category):
        return self.queue


def env_for(source, queue, nm):
    game = Game(queue)
    stubs = {
        "PORT_POWER": PORT_POWER,
        "PORT_TOGGLE": PORT_TOGGLE,
        "PORT_RESET": PORT_RESET,
        "PORT_SET": PORT_SET,
        "EMPTY": "",
        "str": str,
        "LFPG_MCellToggleShouldFlip": load(source, "LFPG_MCellToggleShouldFlip", FLIP_ARGS, {}),
        "LFPG_MCellSettleDue": load(source, "LFPG_MCellSettleDue", DUE_ARGS, {}),
        "LFPG_MCELL_DEBOUNCE_MS": const_int(source, "LFPG_MCELL_DEBOUNCE_MS"),
        "LFPG_MCELL_TOGGLE_REARM_MS": const_int(source, "LFPG_MCELL_TOGGLE_REARM_MS"),
        "LFPG_MCELL_TOGGLE_LOAD_GRACE_MS": const_int(source, "LFPG_MCELL_TOGGLE_LOAD_GRACE_MS"),
        "LFPG_LOG_LEVEL": 0,
        "CALL_CATEGORY_SYSTEM": 0,
        "g_Game": game,
        "LFPG_NetworkManager": SimpleNamespace(Get=lambda: nm),
        "LFPG_MCellPowerSettleTimer": CB_POWER,
        "LFPG_MCellToggleSettleTimer": CB_TOGGLE,
        "LFPG_DeferredRouting": CB_DEFER,
        "LFPG_Util": SimpleNamespace(Debug=lambda *_a: None, Error=lambda *_a: None),
    }
    return stubs, game


def new_cell(source):
    cell = SimpleNamespace()
    for field in FIELDS:
        if field == "m_DeviceId":
            cell.m_DeviceId = "cell"
            continue
        cell.__dict__[field] = member_init(source, field)
    return cell


class Harness:
    def __init__(self, source, delay_power=0, delay_toggle=0, same_ms="read"):
        self.source = source
        self.queue = Queue(delay_power, delay_toggle)
        self.nm = NM()
        self.same_ms = same_ms
        self.env, self.game = env_for(source, self.queue, self.nm)
        self.env["__builtins__"] = {"str": str}
        self.cell = new_cell(source)
        self.cell.m_ToggleGraceUntilMs = 0
        self.env["LFPG_ApplyRouting"] = lambda: None
        self.env["SetSynchDirty"] = lambda: None
        self.env["LFPG_MCellApplyDue"] = lambda nowMs: self.env["_apply"](self.cell, nowMs)
        self.env["LFPG_MCellCommitPower"] = lambda: self.env["_commit_p"](self.cell)
        self.env["LFPG_MCellCommitToggle"] = lambda: self.env["_commit_t"](self.cell)
        self.env["LFPG_MCellSettle"] = lambda: self.env["_settle"](self.cell)
        self.env["LFPG_SetPowered"] = lambda powered: self.env["_setpowered"](self.cell, powered)

        def compile_into(name, extra, key):
            wrapped = bind(source, name, extra)
            exec(compile(wrapped, "<slice:" + name + ">", "exec"), self.env)
            self.env[key] = self.env["run"]

        compile_into("LFPG_MCellCommitPower", [], "_commit_p")
        compile_into("LFPG_MCellCommitToggle", [], "_commit_t")
        compile_into("LFPG_MCellApplyDue", ["nowMs"], "_apply")
        compile_into("LFPG_SetPowered", ["powered"], "_setpowered")
        compile_into("LFPG_MCellSettle", [], "_settle")
        compile_into("LFPG_MCellPowerSettleTimer", [], "_power_timer")
        compile_into("LFPG_MCellToggleSettleTimer", [], "_toggle_timer")
        compile_into("LFPG_OnStoreLoadDevice", ["ctx", "deviceVer"], "_load")
        compile_into("LFPG_OnDeleted", [], "_deleted")
        self.setpowered = self.env["_setpowered"]
        self.apply_due = self.env["_apply"]
        self.settle = self.env["_settle"]
        self.power_timer = self.env["_power_timer"]
        self.toggle_timer = self.env["_toggle_timer"]
        self.load_device = self.env["_load"]
        self.on_deleted = self.env["_deleted"]
        self.flips = []
        self.last_latch = self.cell.m_LatchState
        self.latch_log = []

    def _note(self):
        if self.cell.m_LatchState != self.last_latch:
            self.flips.append(self.queue.now)
            self.last_latch = self.cell.m_LatchState
        self.latch_log.append((self.queue.now, self.cell.m_LatchState))

    def _fire(self, item):
        self.queue.pending.remove(item)
        callback = item[1]
        if callback is CB_POWER:
            self.power_timer(self.cell)
        elif callback is CB_TOGGLE:
            self.toggle_timer(self.cell)
        self._note()

    def _drain_until(self, limit, before_read):
        while True:
            nxt = self.queue.next_due()
            if nxt is None or nxt[0] > limit:
                return
            if nxt[0] == limit and before_read:
                return
            self.queue.now = nxt[0]
            self._fire(nxt)

    def read(self, now, power, toggle, reset=False, set_on=False):
        if self.same_ms == "timer":
            self._drain_until(now, before_read=False)
        else:
            self._drain_until(now, before_read=True)
        self.queue.now = now
        self.nm.ports[PORT_POWER] = power
        self.nm.ports[PORT_TOGGLE] = toggle
        self.nm.ports[PORT_RESET] = reset
        self.nm.ports[PORT_SET] = set_on
        self.setpowered(self.cell, power)
        self._note()
        if self.same_ms == "timer":
            return
        self._drain_until(now, before_read=False)

    def run_reads(self, reads, set_on=False, reset=False):
        for item in reads:
            now, power, toggle = item[0], bool(item[1]), bool(item[2])
            self.read(now, power, toggle, reset=reset, set_on=set_on)
        nxt = self.queue.next_due()
        while nxt is not None:
            self.queue.now = nxt[0]
            self._fire(nxt)
            nxt = self.queue.next_due()
        return len(self.flips), self.cell.m_LatchState, list(self.flips)

    def do_load(self, at=0):
        self.queue.now = at
        ctx = SimpleNamespace(Read=lambda _value: True)
        self.load_device(self.cell, ctx, 1)


def latch_at(log, t):
    last = None
    for now, latch in log:
        if now <= t:
            last = latch
    return last


def should_flip(low_since, power_since, grace, now, rearm):
    if low_since < 0 or power_since < 0:
        return False
    if now < grace:
        return False
    if now - low_since < rearm:
        return False
    if now - power_since < rearm:
        return False
    return True


def oracle(reads, debounce, rearm, grace, set_on=False, reset=False):
    def raw_changes(index, initial):
        val = initial
        out = []
        for item in reads:
            v = bool(item[index])
            if v != val:
                out.append((item[0], v))
                val = v
        return out

    def accepted_holds(chgs):
        events = []
        for i, (start, val) in enumerate(chgs):
            if i + 1 < len(chgs):
                dur = chgs[i + 1][0] - start
            else:
                dur = debounce
            if dur >= debounce:
                events.append((start + debounce, start, val))
        return events

    power_ev = [(t, 0, start, val) for t, start, val in accepted_holds(raw_changes(1, False))]
    tog_ev = [(t, 1, start, val) for t, start, val in accepted_holds(raw_changes(2, True))]
    events = sorted(power_ev + tog_ev)
    settled_p = False
    settled_t = True
    power_since, low_since = -1, -1
    latch = False
    flips = []
    for settle_t, kind, ch, val in events:
        if kind == 0:
            if val == settled_p:
                continue
            settled_p = val
            power_since = ch if val else -1
        else:
            if val == settled_t:
                continue
            if val and (not settled_t):
                if should_flip(low_since, power_since, grace, ch, rearm):
                    latch = not latch
                    flips.append(settle_t)
            if not val:
                low_since = ch
            settled_t = val
        if set_on and not reset:
            latch = True
        elif reset and not set_on:
            latch = False
    return len(flips), latch, flips


def f2_reads():
    reads = [(0, 1, 0)]
    for k in range(5):
        a = 10000 + 5000 * k
        reads.extend([
            (a, 1, 1),
            (a + 10, 0, 0),
            (a + 83, 1, 1),
            (a + 2000, 1, 0),
            (a + 2010, 0, 0),
            (a + 2083, 1, 0),
        ])
    return reads


class MemoryCellToggle(unittest.TestCase):
    def test_flip_predicate(self):
        source = src_text()
        fn = load(source, "LFPG_MCellToggleShouldFlip", FLIP_ARGS, {})
        now = 20000
        rearm = const_int(source, "LFPG_MCELL_TOGGLE_REARM_MS")
        self.assertTrue(fn(now - 1500, now - 10000, 0, now, rearm))
        self.assertTrue(fn(now - 1000, now - 1000, 0, now, rearm))
        self.assertFalse(fn(now - 200, now - 10000, 0, now, rearm))
        self.assertFalse(fn(now - 1500, now - 300, 0, now, rearm))
        self.assertFalse(fn(now - 1500, now - 10000, now + 1, now, rearm))
        self.assertFalse(fn(-1, now - 10000, 0, now, rearm))
        self.assertFalse(fn(now - 1500, -1, 0, now, rearm))

    def test_settle_due_predicate(self):
        source = src_text()
        fn = load(source, "LFPG_MCellSettleDue", DUE_ARGS, {})
        debounce = const_int(source, "LFPG_MCELL_DEBOUNCE_MS")
        self.assertTrue(fn(True, False, 0, debounce, debounce))
        self.assertTrue(fn(True, False, 100, 100 + debounce, debounce))
        self.assertFalse(fn(True, True, 0, debounce, debounce))
        self.assertFalse(fn(True, False, -1, debounce, debounce))
        self.assertFalse(fn(True, False, 0, debounce - 1, debounce))

    def test_source_no_toggle_override_and_self_consumption(self):
        source = src_text()
        powered = method(source, "LFPG_SetPowered")
        self.assertNotIn("newActive = true;", powered)
        cons = method(source, "LFPG_GetConsumption")
        self.assertIn("return LFPG_MCELL_CONSUMPTION;", cons)
        self.assertRegex(source, r"LFPG_MCELL_CONSUMPTION\s*=\s*1\.0")
        self.assertIn("LFPG_MCellApplyDue(nowMs)", powered)

    def _expect(self, reads, flips, latch=None, instants=None, **kwargs):
        source = src_text()
        harness = Harness(source, **kwargs)
        count, final, times = harness.run_reads(reads)
        self.assertEqual(count, flips)
        if latch is not None:
            self.assertEqual(final, latch)
        if instants is not None:
            self.assertEqual(times, instants)
        return times

    def test_fixtures_punctual(self):
        self._expect([(0, 1, 0), (10000, 1, 1), (10010, 0, 0), (10083, 1, 1), (12000, 1, 0)], 1, instants=[10483])
        self._expect(f2_reads(), 5)
        for tail in (
            [(30000, 0, 1), (30050, 1, 1)],
            [(30000, 1, 0), (30050, 1, 1)],
            [(30000, 1, 1)],
        ):
            self._expect([(0, 1, 1), (20000, 0, 0)] + tail, 0)
        self._expect([(0, 1, 1), (5000, 0, 0), (7500, 0, 1), (8000, 1, 1)], 0)
        self._expect([(0, 1, 1), (5000, 0, 0), (5600, 1, 0), (5700, 0, 0), (6300, 1, 1)], 0)
        self._expect([(0, 1, 1), (5000, 0, 0), (5600, 1, 0), (5700, 0, 0), (6600, 1, 1)], 0)
        self._expect([(0, 1, 1), (5000, 1, 0), (5050, 0, 0), (5950, 1, 0), (6000, 1, 1)], 0)
        self._expect([(0, 1, 0), (1500, 1, 1), (3500, 0, 0), (3600, 1, 0), (4550, 1, 1), (6000, 1, 1)], 2, instants=[1900, 4950])
        self._expect([(0, 1, 0), (5000, 0, 0), (5050, 0, 0), (5100, 1, 1)], 1)
        f10 = [(0, 1, 0), (5000, 1, 1), (7000, 1, 0)]
        source = src_text()
        set_h = Harness(source)
        _, final, _ = set_h.run_reads(f10, set_on=True)
        self.assertEqual(final, True)
        self.assertTrue(latch_at(set_h.latch_log, 0))
        self.assertTrue(latch_at(set_h.latch_log, 5400))
        self.assertTrue(latch_at(set_h.latch_log, 7000))
        rst = Harness(source)
        _, rfinal, _ = rst.run_reads(f10, reset=True)
        self.assertFalse(rfinal)
        self.assertFalse(latch_at(rst.latch_log, 0))
        self.assertFalse(latch_at(rst.latch_log, 5400))
        self.assertFalse(latch_at(rst.latch_log, 7000))
        both = Harness(source)
        _, bfinal, _ = both.run_reads(f10, set_on=True, reset=True)
        self.assertEqual(bfinal, True)
        self.assertFalse(latch_at(both.latch_log, 0))
        self.assertTrue(latch_at(both.latch_log, 5400))
        self.assertTrue(latch_at(both.latch_log, 7000))
        self._expect([(0, 1, 0), (5000, 1, 1), (7000, 1, 0), (7600, 1, 1), (9600, 1, 0)], 1)
        self._expect([(0, 1, 0), (5000, 0, 1), (8000, 0, 0)], 0)
        self._expect([(0, 1, 0), (5000, 1, 1), (5300, 0, 1), (5600, 1, 1), (5800, 1, 0)], 1, instants=[5400])
        self._expect([(0, 1, 0), (5000, 1, 1), (5100, 0, 1), (6000, 0, 1)], 1)
        self._expect([(0, 1, 0), (5000, 1, 1), (5800, 1, 0)], 1)
        self._expect([(0, 1, 0), (5000, 1, 1), (5600, 1, 0), (7000, 1, 1), (7600, 1, 0)], 2)
        self._expect([(0, 1, 0), (5000, 1, 1), (5400, 1, 0)], 1, same_ms="read")
        self._expect([(0, 1, 0), (5000, 1, 1), (5400, 1, 0)], 1, same_ms="timer")
        self._expect([(0, 1, 0), (5000, 0, 1), (5400, 1, 1)], 0)
        self._expect([(0, 1, 0), (5000, 1, 1), (5100, 0, 1), (5500, 0, 0)], 1)

    def test_f15_grace_via_load(self):
        source = src_text()
        early = Harness(source)
        early.do_load(0)
        count, _, _ = early.run_reads([(0, 1, 0), (5000, 1, 1), (7000, 1, 0)])
        self.assertEqual(count, 0)
        late = Harness(source)
        late.do_load(0)
        count, _, _ = late.run_reads([(0, 1, 0), (31000, 1, 1), (33000, 1, 0)])
        self.assertEqual(count, 1)

    def test_late_timers_match_punctual(self):
        f13 = [(0, 1, 0), (5000, 1, 1), (5300, 0, 1), (5600, 1, 1), (5800, 1, 0)]
        self._expect(f13, 1, delay_toggle=401)
        self._expect([(0, 1, 0), (5000, 1, 1), (5800, 1, 0)], 1, delay_toggle=401)
        self._expect([(0, 1, 0), (5000, 1, 1), (5600, 1, 0), (7000, 1, 1), (7600, 1, 0)], 2, delay_toggle=3000)
        self._expect([(0, 1, 1), (5000, 1, 0), (5050, 0, 0), (5950, 1, 0), (6000, 1, 1)], 0, delay_power=1000)
        self._expect([(0, 1, 0), (5000, 1, 1), (5100, 0, 1), (6000, 0, 1)], 1, delay_toggle=200)

    def test_deleted_drops_timers(self):
        source = src_text()
        harness = Harness(source)
        harness.read(0, True, False)
        harness.read(100, True, True)
        self.assertTrue(harness.queue.pending)
        harness.on_deleted(harness.cell)
        self.assertFalse(harness.queue.pending)
        harness.queue.now = 100000
        self.assertIsNone(harness.queue.next_due())

    def test_random_matches_oracle(self):
        source = src_text()
        debounce = const_int(source, "LFPG_MCELL_DEBOUNCE_MS")
        rearm = const_int(source, "LFPG_MCELL_TOGGLE_REARM_MS")
        rng = random.Random(131)
        for _n in range(1000):
            nread = rng.randint(3, 10)
            t = 0
            reads = []
            for _i in range(nread):
                t = t + rng.choice(GAPS)
                reads.append((t, rng.randint(0, 1), rng.randint(0, 1)))
            delay_p = rng.randint(0, 3000)
            delay_t = rng.randint(0, 3000)
            harness = Harness(source, delay_power=delay_p, delay_toggle=delay_t)
            got = harness.run_reads(reads)
            exp = oracle(reads, debounce, rearm, 0)
            self.assertEqual(got[0], exp[0])
            self.assertEqual(got[1], exp[1])

    def test_swapped_power_toggle_ports_break_f1(self):
        source = src_text().replace(
            "hasPower  = nm.IsPortReceivingPower(m_DeviceId, portPower);",
            "hasPower  = nm.IsPortReceivingPower(m_DeviceId, portToggle);",
            1)
        harness = Harness(source)
        count, _, _ = harness.run_reads([(0, 1, 0), (10000, 1, 1), (10010, 0, 0), (10083, 1, 1), (12000, 1, 0)])
        self.assertNotEqual(count, 1)

    def test_mutants(self):
        source = src_text()
        f8 = [(0, 1, 0), (1500, 1, 1), (3500, 0, 0), (3600, 1, 0), (4550, 1, 1), (6000, 1, 1)]
        f9 = [(0, 1, 0), (5000, 0, 0), (5050, 0, 0), (5100, 1, 1)]
        f13 = [(0, 1, 0), (5000, 1, 1), (5300, 0, 1), (5600, 1, 1), (5800, 1, 0)]
        f14 = [(0, 1, 0), (5000, 1, 1), (5100, 0, 1), (6000, 0, 1)]
        f12 = [(0, 1, 0), (5000, 0, 1), (8000, 0, 0)]
        f19 = [(0, 1, 0), (5000, 0, 1), (5400, 1, 1)]
        f11 = [(0, 1, 0), (5000, 1, 1), (7000, 1, 0), (7600, 1, 1), (9600, 1, 0)]
        f3 = [(0, 1, 1), (20000, 0, 0), (30000, 1, 1)]
        zero_db, n = re.subn(r"LFPG_MCELL_DEBOUNCE_MS = 400", "LFPG_MCELL_DEBOUNCE_MS = 0", source, count=1)
        self.assertEqual(n, 1)
        self.assertNotEqual(Harness(zero_db).run_reads(f8)[0], 2)
        self.assertNotEqual(Harness(zero_db).run_reads(f9)[0], 1)
        shared, n = re.subn(r"m_RawToggleChangedMs = nowMs", "m_RawToggleChangedMs = m_RawPowerChangedMs", source, count=1)
        self.assertEqual(n, 1)
        self.assertNotEqual(Harness(shared).run_reads(f8)[0], 2)
        one_timer, n = re.subn(
            r"CallLater\(LFPG_MCellToggleSettleTimer, LFPG_MCELL_DEBOUNCE_MS, false\)",
            "CallLater(LFPG_MCellPowerSettleTimer, LFPG_MCELL_DEBOUNCE_MS, false)",
            source, count=1)
        self.assertEqual(n, 1)
        self.assertNotEqual(Harness(one_timer).run_reads(f13)[2], [5400])
        power_first, n = re.subn(
            r"if \(m_RawPowerChangedMs <= m_RawToggleChangedMs\)",
            "if (true)",
            source, count=1)
        self.assertEqual(n, 1)
        self.assertEqual(Harness(power_first, delay_toggle=200).run_reads(f14)[0], 0)
        tog_first, n = re.subn(
            r"if \(m_RawPowerChangedMs <= m_RawToggleChangedMs\)",
            "if (m_RawPowerChangedMs < m_RawToggleChangedMs)",
            source, count=1)
        self.assertEqual(n, 1)
        self.assertNotEqual(Harness(tog_first).run_reads(f12)[0], 0)
        self.assertNotEqual(Harness(tog_first).run_reads(f19)[0], 0)
        no_r1, n = re.subn(
            r"if \(nowMs - lowSinceMs < rearmMs\)\s*\{\s*return false;\s*\}",
            "", source, count=1)
        self.assertEqual(n, 1)
        self.assertNotEqual(Harness(no_r1).run_reads(f11)[0], 1)
        no_r2, n = re.subn(
            r"if \(powerSinceMs < 0\)\s*\{\s*return false;\s*\}",
            "", source, count=1)
        self.assertEqual(n, 1)
        no_r2, n = re.subn(
            r"if \(nowMs - powerSinceMs < rearmMs\)\s*\{\s*return false;\s*\}",
            "", no_r2, count=1)
        self.assertEqual(n, 1)
        self.assertNotEqual(Harness(no_r2).run_reads(f3)[0], 0)
        self.assertNotEqual(Harness(no_r2).run_reads(f12)[0], 0)
        self.assertNotEqual(Harness(no_r2).run_reads(f19)[0], 0)
        rearm0, n = re.subn(r"LFPG_MCELL_TOGGLE_REARM_MS = 1000", "LFPG_MCELL_TOGGLE_REARM_MS = 0", source, count=1)
        self.assertEqual(n, 1)
        self.assertNotEqual(Harness(rearm0).run_reads(f11)[0], 1)
        no_grace, n = re.subn(
            r"if \(nowMs < graceUntilMs\)\s*\{\s*return false;\s*\}",
            "", source, count=1)
        self.assertEqual(n, 1)
        early = Harness(no_grace)
        early.do_load(0)
        self.assertNotEqual(early.run_reads([(0, 1, 0), (5000, 1, 1), (7000, 1, 0)])[0], 0)
        grace0, n = re.subn(
            r"m_ToggleGraceUntilMs = g_Game.GetTime\(\) \+ LFPG_MCELL_TOGGLE_LOAD_GRACE_MS",
            "m_ToggleGraceUntilMs = 0",
            source, count=1)
        self.assertEqual(n, 1)
        loaded = Harness(grace0)
        loaded.do_load(0)
        self.assertNotEqual(loaded.run_reads([(0, 1, 0), (5000, 1, 1), (7000, 1, 0)])[0], 0)
        no_apply, n = re.subn(r"LFPG_MCellApplyDue\(nowMs\);", "", source, count=1)
        self.assertEqual(n, 1)
        self.assertNotEqual(Harness(no_apply, delay_toggle=401).run_reads(f13)[0], 1)
        self.assertNotEqual(Harness(no_apply, delay_toggle=401).run_reads([(0, 1, 0), (5000, 1, 1), (5800, 1, 0)])[0], 1)
        self.assertNotEqual(Harness(no_apply, delay_power=1000).run_reads([(0, 1, 1), (5000, 1, 0), (5050, 0, 0), (5950, 1, 0), (6000, 1, 1)])[0], 0)
        deleted_pair = (
            r"Remove\(LFPG_MCellPowerSettleTimer\);\s*"
            r"g_Game.GetCallQueue\(CALL_CATEGORY_SYSTEM\).Remove\(LFPG_MCellToggleSettleTimer\);"
        )
        no_rm_p, n = re.subn(
            deleted_pair,
            "Remove(LFPG_MCellToggleSettleTimer);",
            source, count=1)
        self.assertEqual(n, 1)
        gone = Harness(no_rm_p)
        gone.read(0, True, False)
        gone.on_deleted(gone.cell)
        self.assertTrue(any(cb is CB_POWER for _t, cb in gone.queue.pending))
        no_rm_t, n = re.subn(
            deleted_pair,
            "Remove(LFPG_MCellPowerSettleTimer);",
            source, count=1)
        self.assertEqual(n, 1)
        gone_t = Harness(no_rm_t)
        gone_t.read(0, True, False)
        gone_t.read(100, True, True)
        gone_t.on_deleted(gone_t.cell)
        self.assertTrue(any(cb is CB_TOGGLE for _t, cb in gone_t.queue.pending))
        f10 = [(0, 1, 0), (5000, 1, 1), (7000, 1, 0)]
        sr_block = (
            r"if \(m_FlipPending\)\s*\{\s*"
            r"m_LatchState = !m_LatchState;\s*"
            r"m_FlipPending = false;\s*"
            r"flip = true;\s*"
            r"\}\s*"
            r"if \(hasSet && !hasReset\)\s*\{\s*"
            r"m_LatchState = true;\s*"
            r"\}\s*"
            r"else if \(hasReset && !hasSet\)\s*\{\s*"
            r"m_LatchState = false;\s*"
            r"\}"
        )
        swapped, n = re.subn(
            sr_block,
            "if (hasSet && !hasReset)\n"
            "        {\n"
            "            m_LatchState = true;\n"
            "        }\n"
            "        else if (hasReset && !hasSet)\n"
            "        {\n"
            "            m_LatchState = false;\n"
            "        }\n"
            "        if (m_FlipPending)\n"
            "        {\n"
            "            m_LatchState = !m_LatchState;\n"
            "            m_FlipPending = false;\n"
            "            flip = true;\n"
            "        }",
            source, count=1)
        self.assertEqual(n, 1)
        sw = Harness(swapped)
        sw.run_reads(f10, set_on=True)
        self.assertNotEqual(latch_at(sw.latch_log, 5400), True)
        no_sr, n = re.subn(
            r"if \(hasSet && !hasReset\)\s*\{\s*m_LatchState = true;\s*\}\s*"
            r"else if \(hasReset && !hasSet\)\s*\{\s*m_LatchState = false;\s*\}",
            "", source, count=1)
        self.assertEqual(n, 1)
        ns = Harness(no_sr)
        ns.run_reads(f10, set_on=True)
        self.assertNotEqual(latch_at(ns.latch_log, 0), True)
        no_rm_pwr, n = re.subn(
            r"g_Game.GetCallQueue\(CALL_CATEGORY_SYSTEM\).Remove\(LFPG_MCellPowerSettleTimer\);\s*"
            r"g_Game.GetCallQueue\(CALL_CATEGORY_SYSTEM\).CallLater\(LFPG_MCellPowerSettleTimer",
            "g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_MCellPowerSettleTimer",
            source, count=1)
        self.assertEqual(n, 1)
        hp = Harness(no_rm_pwr)
        hp.read(0, True, False)
        hp.read(100, False, False)
        self.assertIn(400, [t for t, cb in hp.queue.pending if cb is CB_POWER])
        no_rm_tog, n = re.subn(
            r"g_Game.GetCallQueue\(CALL_CATEGORY_SYSTEM\).Remove\(LFPG_MCellToggleSettleTimer\);\s*"
            r"g_Game.GetCallQueue\(CALL_CATEGORY_SYSTEM\).CallLater\(LFPG_MCellToggleSettleTimer",
            "g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_MCellToggleSettleTimer",
            source, count=1)
        self.assertEqual(n, 1)
        ht = Harness(no_rm_tog)
        ht.read(0, True, False)
        ht.read(100, True, True)
        self.assertIn(400, [t for t, cb in ht.queue.pending if cb is CB_TOGGLE])


if __name__ == "__main__":
    unittest.main()
