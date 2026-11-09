"""Momentary pulse survives a splitter blackout: execute the real SetPowered path."""
from pathlib import Path
from types import SimpleNamespace
import re
import sys
import unittest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from enforce_scalar_slice import method, scalar_function

BUTTON = ROOT / "scripts/4_World/LFPG_PushButton.c"
REMOTE = ROOT / "scripts/4_World/LFPG_SwitchRemote.c"
DEFINES = ROOT / "scripts/3_Game/LFPG_Defines.c"
CB_PULSE = object()


def const_int(source, name):
    match = re.search(r"\b" + name + r"\s*=\s*(-?\d+)", source)
    if not match:
        raise AssertionError("const missing: " + name)
    return int(match.group(1))


def neutralize(code):
    code = re.sub(r"#ifndef\s+SERVER.*?#endif", "", code, flags=re.S)
    code = re.sub(
        r"if\s*\(\s*LFPG_LOG_LEVEL\s*>=\s*2\s*\)\s*\{(?:[^{}]|\{[^{}]*\})*\}",
        "",
        code)
    code = re.sub(r"LFPG_Util\.\w+\s*\([^;]*\);", "", code)
    code = re.sub(r'"(?:\\.|[^"\\])*"', "EMPTY", code)
    code = re.sub(r"(\w+)\.ToString\(\)", r"str(\1)", code)
    code = re.sub(
        r"\b(?:string|vector|ref|ParamsReadContext|ParamsWriteContext|LFPG_[A-Za-z0-9_]+)\s+(?=[A-Za-z_]\w*\s*(?:=|;))",
        "",
        code)
    return code


FIELDS = ["m_PoweredNet", "m_SwitchOn", "m_DeviceId"]


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
    def __init__(self):
        self.pending = {}
        self.now = 0

    def Remove(self, callback):
        self.pending.pop(id(callback), None)

    def CallLater(self, callback, delay, repeat):
        self.pending[id(callback)] = (self.now + delay, callback)

    def next_due(self):
        if not self.pending:
            return None
        return min(self.pending.values(), key=lambda item: item[0])


class PulseHarness:
    def __init__(self, path, toggle_name, source=None):
        self.source = source or path.read_text(encoding="utf-8")
        self.queue = Queue()
        self.game = SimpleNamespace(
            GetTime=lambda: self.queue.now,
            GetCallQueue=lambda _cat: self.queue,
        )
        self.cell = SimpleNamespace(m_PoweredNet=False, m_SwitchOn=False, m_DeviceId="btn")
        self.env = {
            "__builtins__": {"str": str},
            "EMPTY": "",
            "str": str,
            "g_Game": self.game,
            "CALL_CATEGORY_SYSTEM": 0,
            "LFPG_LOG_LEVEL": 0,
            "LFPG_BUTTON_PULSE_MS": const_int(DEFINES.read_text(encoding="utf-8"), "LFPG_BUTTON_PULSE_MS"),
            "LFPG_PulseOff": CB_PULSE,
            "LFPG_NetworkManager": SimpleNamespace(Get=lambda: SimpleNamespace(RequestPropagate=lambda *_a: None)),
            "LFPG_Util": SimpleNamespace(Info=lambda *_a: None, Debug=lambda *_a: None),
            "SetSynchDirty": lambda: None,
        }

        def compile_into(name, extra, key):
            wrapped = bind(self.source, name, extra)
            exec(compile(wrapped, "<slice:" + name + ">", "exec"), self.env)
            self.env[key] = self.env["run"]

        compile_into("LFPG_SetPowered", ["powered"], "_set")
        compile_into(toggle_name, [], "_toggle")
        compile_into("LFPG_PulseOff", [], "_off")
        compile_into("LFPG_OnDeleted", [], "_deleted")
        self.env["LFPG_PulseOff"] = CB_PULSE
        self.setpowered = self.env["_set"]
        self.toggle = self.env["_toggle"]
        self.pulse_off = self.env["_off"]
        self.on_deleted = self.env["_deleted"]

    def drain(self, now):
        self.queue.now = now
        while True:
            nxt = self.queue.next_due()
            if nxt is None or nxt[0] > now:
                return
            self.queue.Remove(nxt[1])
            if nxt[1] is CB_PULSE:
                self.pulse_off(self.cell)


def run_lifecycle(path, toggle_name, source=None):
    harness = PulseHarness(path, toggle_name, source)
    harness.queue.now = 0
    harness.setpowered(harness.cell, True)
    harness.toggle(harness.cell)
    harness.drain(0)
    if not harness.cell.m_SwitchOn:
        return "early-off"
    if not harness.queue.pending:
        return "no-timer"
    harness.drain(100)
    harness.setpowered(harness.cell, False)
    if not harness.cell.m_SwitchOn:
        return "cut-cleared"
    harness.drain(100000)
    if harness.cell.m_SwitchOn:
        return "still-on"
    harness.on_deleted(harness.cell)
    if harness.queue.pending:
        return "leaked"
    return "ok"


class PulseSurvivesBlackout(unittest.TestCase):
    def test_lifecycle_button_and_remote(self):
        self.assertEqual(run_lifecycle(BUTTON, "LFPG_ToggleButton"), "ok")
        self.assertEqual(run_lifecycle(REMOTE, "LFPG_ToggleSwitch"), "ok")

    def test_mutant_inverted_pulseoff_guard_fails(self):
        for path, name in ((BUTTON, "LFPG_ToggleButton"), (REMOTE, "LFPG_ToggleSwitch")):
            source = path.read_text(encoding="utf-8")
            mutant, n = re.subn(r"if \(!m_SwitchOn\)", "if (m_SwitchOn)", source, count=1)
            self.assertEqual(n, 1)
            self.assertNotEqual(run_lifecycle(path, name, mutant), "ok")

    def test_mutant_zero_pulse_ms_fails(self):
        for path, name in ((BUTTON, "LFPG_ToggleButton"), (REMOTE, "LFPG_ToggleSwitch")):
            source = path.read_text(encoding="utf-8")
            mutant, n = re.subn(
                r"CallLater\(LFPG_PulseOff, LFPG_BUTTON_PULSE_MS, false\)",
                "CallLater(LFPG_PulseOff, 0, false)",
                source, count=1)
            self.assertEqual(n, 1)
            self.assertNotEqual(run_lifecycle(path, name, mutant), "ok")

    def test_mutant_no_delete_cleanup_fails(self):
        for path, name in ((BUTTON, "LFPG_ToggleButton"), (REMOTE, "LFPG_ToggleSwitch")):
            source = path.read_text(encoding="utf-8")
            self.assertEqual(run_delete_pending(path, name, source), "ok")
            mutant, n = re.subn(
                r"(override void LFPG_OnDeleted\(\)\s*\{\s*if \(g_Game\)\s*\{\s*)g_Game.GetCallQueue\(CALL_CATEGORY_SYSTEM\).Remove\(LFPG_PulseOff\);",
                r"\1int unusedMutant = 0;",
                source,
                count=1)
            self.assertEqual(n, 1)
            self.assertEqual(run_delete_pending(path, name, mutant), "leaked")


def run_delete_pending(path, toggle_name, source):
    harness = PulseHarness(path, toggle_name, source)
    harness.queue.now = 0
    harness.setpowered(harness.cell, True)
    harness.toggle(harness.cell)
    harness.drain(100)
    harness.setpowered(harness.cell, False)
    harness.on_deleted(harness.cell)
    if harness.queue.pending:
        return "leaked"
    harness.drain(100000)
    return "ok"


if __name__ == "__main__":
    unittest.main()
