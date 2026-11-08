"""Memory Cell Toggle rising-edge predicate; production slice plus source contracts."""
from pathlib import Path
import re
import sys
import unittest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from enforce_scalar_slice import load, method, scalar_function

CELL = ROOT / "scripts/4_World/LFPG_MemoryCell.c"
ARGS = ["lowSinceMs", "powerSinceMs", "graceUntilMs", "nowMs", "rearmMs"]
REARM = 1000
NOW = 20000


def flip_fn(source=None):
    text = source if source is not None else CELL.read_text(encoding="utf-8")
    return load(text, "LFPG_MCellToggleShouldFlip", ARGS, {})


def without_rearm(source):
    body = method(source, "LFPG_MCellToggleShouldFlip")
    stripped, n1 = re.subn(
        r"if \(nowMs - lowSinceMs < rearmMs\)\s*\{\s*return false;\s*\}",
        "", body, count=1)
    stripped, n2 = re.subn(
        r"if \(nowMs - powerSinceMs < rearmMs\)\s*\{\s*return false;\s*\}",
        "", stripped, count=1)
    if n1 != 1 or n2 != 1:
        raise AssertionError("rearm guards missing")
    return source.replace(body, stripped, 1)


class MemoryCellToggle(unittest.TestCase):
    def test_positive_and_negative_cases(self):
        fn = flip_fn()
        self.assertTrue(fn(NOW - 1500, NOW - 10000, 0, NOW, REARM))
        self.assertTrue(fn(NOW - 1000, NOW - 1000, 0, NOW, REARM))
        self.assertFalse(fn(NOW - 200, NOW - 10000, 0, NOW, REARM))
        self.assertFalse(fn(NOW - 1500, NOW - 300, 0, NOW, REARM))
        self.assertFalse(fn(NOW - 1500, NOW - 10000, NOW + 1, NOW, REARM))
        self.assertFalse(fn(-1, NOW - 10000, 0, NOW, REARM))
        self.assertFalse(fn(NOW - 1500, -1, 0, NOW, REARM))

    def test_mutant_without_rearm_makes_n1_true(self):
        source = CELL.read_text(encoding="utf-8")
        mutant = without_rearm(source)
        fn = flip_fn(mutant)
        self.assertTrue(fn(NOW - 200, NOW - 10000, 0, NOW, REARM))
        self.assertFalse(flip_fn(source)(NOW - 200, NOW - 10000, 0, NOW, REARM))

    def test_source_no_toggle_override_and_self_consumption(self):
        source = CELL.read_text(encoding="utf-8")
        powered = method(source, "LFPG_SetPowered")
        self.assertNotIn("newActive = true;", powered)
        self.assertNotRegex(powered, r"if\s*\(\s*hasToggle\s*\)\s*\{[^}]*newActive")
        cons = method(source, "LFPG_GetConsumption")
        self.assertIn("return LFPG_MCELL_CONSUMPTION;", cons)
        self.assertRegex(source, r"LFPG_MCELL_CONSUMPTION\s*=\s*1\.0")

    def test_setpowered_sequence_and_mutant_without_last_high_false(self):
        source = CELL.read_text(encoding="utf-8")
        self.assertIn("m_ToggleLastHigh = false;", method(source, "LFPG_SetPowered"))
        latch, last_high = run_toggle_sequence(source)
        self.assertTrue(latch)
        self.assertFalse(last_high)
        mutant = source.replace("m_ToggleLastHigh = false;", "", 1)
        self.assertNotEqual(mutant, source)
        mutant_latch, _ = run_toggle_sequence(mutant)
        self.assertFalse(mutant_latch)


def run_toggle_sequence(source):
    powered = method(source, "LFPG_SetPowered")
    start = powered.index("if (!hasPower)")
    end = powered.index("bool newActive = m_LatchState")
    body = powered[start:end]
    arguments = [
        "hasPower", "hasToggle", "hasSet", "hasReset", "nowMs", "flip",
        "m_ToggleLastHigh", "m_ToggleLowSinceMs", "m_PowerSinceMs",
        "m_ToggleGraceUntilMs", "m_LatchState",
    ]
    stubs = {
        "LFPG_MCellToggleShouldFlip": flip_fn(source),
        "LFPG_MCELL_TOGGLE_REARM_MS": 1000,
    }
    compiled = scalar_function(body, arguments) + (
        "    return m_LatchState, m_ToggleLastHigh, m_ToggleLowSinceMs, m_PowerSinceMs\n"
    )
    environment = {"__builtins__": {}, **stubs}
    exec(compile(compiled, "<source-slice:LFPG_SetPowered-toggle>", "exec"), environment)
    step = environment["run"]
    last_high = True
    low_since = -1
    power_since = -1
    latch = False
    grace = 0
    # baja, sube, baja 300 ms, sube, Power cae
    reads = [
        (True, False, 0),
        (True, True, 1500),
        (True, False, 1800),
        (True, True, 2100),
        (False, False, 4000),
    ]
    for has_power, has_toggle, now_ms in reads:
        latch, last_high, low_since, power_since = step(
            has_power, has_toggle, False, False, now_ms, False,
            last_high, low_since, power_since, grace, latch)
    return latch, last_high


if __name__ == "__main__":
    unittest.main()
