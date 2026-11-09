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
POWER_ARGS = ["powerSinceMs", "powerLowSinceMs", "nowMs", "rearmMs"]
REARM = 1000
NOW = 20000
CUT_CHECK = "nowMs - powerLowSinceMs >= rearmMs"


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

    def test_power_restart_counter_cases_and_mutant(self):
        source = CELL.read_text(encoding="utf-8")
        fn = load(source, "LFPG_MCellPowerRestartCounter", POWER_ARGS, {})
        self.assertTrue(fn(-1, -1, 5000, 1000))
        self.assertTrue(fn(1000, 3000, 4000, 1000))
        self.assertFalse(fn(1000, -1, 9000, 1000))
        self.assertFalse(fn(1000, 8900, 9000, 1000))
        mutant, count = re.subn(re.escape(CUT_CHECK), "true", source, count=1)
        self.assertEqual(count, 1)
        mutant_fn = load(mutant, "LFPG_MCellPowerRestartCounter", POWER_ARGS, {})
        self.assertTrue(mutant_fn(1000, 8900, 9000, 1000))
        self.assertFalse(fn(1000, 8900, 9000, 1000))

    def test_power_hysteresis_sequences_and_mutant(self):
        source = CELL.read_text(encoding="utf-8")
        short_cut = [
            (True, False, 0),
            (True, False, 1500),
            (False, False, 1600),
            (True, True, 1700),
        ]
        long_cut = [
            (True, True, 0),
            (False, False, 10000),
            (True, True, 20000),
        ]
        self.assertTrue(last_flip(source, short_cut))
        self.assertFalse(last_flip(source, long_cut))
        mutant, count = re.subn(
            r"if \(!hasPower\)\s*\{\s*if \(m_PowerLowSinceMs < 0\)\s*\{\s*m_PowerLowSinceMs = nowMs;\s*\}\s*else if \(nowMs - m_PowerLowSinceMs >= LFPG_MCELL_TOGGLE_REARM_MS\)\s*\{\s*m_PowerSinceMs = -1;\s*\}\s*\}",
            "if (!hasPower)\n        {\n            m_PowerSinceMs = -1;\n        }",
            source,
            count=1)
        self.assertEqual(count, 1)
        self.assertFalse(last_flip(mutant, short_cut))
        self.assertTrue(last_flip(source, short_cut))

    def test_long_cut_toggle_before_power_stays_on(self):
        source = CELL.read_text(encoding="utf-8")
        seq_a = [
            (True, False, 0),
            (True, True, 1500),
            (False, False, 5000),
            (False, True, 15000),
            (True, True, 15050),
        ]
        seq_b = [
            (True, False, 0),
            (True, True, 1500),
            (False, False, 5000),
            (True, False, 15000),
            (True, True, 15050),
        ]
        seq_c = [
            (True, False, 0),
            (True, True, 1500),
            (False, False, 5000),
            (True, True, 15000),
        ]
        seq_d = [
            (True, False, 0),
            (False, False, 1450),
            (False, True, 1500),
            (True, True, 1523),
        ]
        self.assertTrue(run_reads(source, seq_a)[0])
        self.assertTrue(run_reads(source, seq_b)[0])
        self.assertTrue(run_reads(source, seq_c)[0])
        self.assertTrue(run_reads(source, seq_d)[0])
        mutant, count = re.subn(
            r"else if \(nowMs - m_PowerLowSinceMs >= LFPG_MCELL_TOGGLE_REARM_MS\)\s*\{\s*m_PowerSinceMs = -1;\s*\}",
            "",
            source,
            count=1)
        self.assertEqual(count, 1)
        self.assertFalse(run_reads(mutant, seq_a)[0])
        self.assertTrue(run_reads(source, seq_a)[0])

    def test_outage_does_not_arm_fixed_toggle(self):
        source = CELL.read_text(encoding="utf-8")
        seq_a = [
            (True, True, 1),
            (False, False, 5000),
            (True, False, 5600),
            (False, False, 5700),
            (True, True, 6300),
        ]
        seq_b = [
            (True, True, 1),
            (False, False, 10000),
            (True, False, 10900),
            (True, True, 11000),
        ]
        seq_c = [
            (True, False, 0),
            (True, True, 1500),
            (False, False, 3000),
            (True, False, 3100),
            (True, True, 5200),
        ]
        seq7_a = [
            (True, False, 0),
            (True, True, 1500),
            (False, False, 5000),
            (False, True, 15000),
            (True, True, 15050),
        ]
        seq7_b = [
            (True, False, 0),
            (True, True, 1500),
            (False, False, 5000),
            (True, False, 15000),
            (True, True, 15050),
        ]
        seq7_c = [
            (True, False, 0),
            (True, True, 1500),
            (False, False, 5000),
            (True, True, 15000),
        ]
        seq7_d = [
            (True, False, 0),
            (False, False, 1450),
            (False, True, 1500),
            (True, True, 1523),
        ]
        self.assertFalse(run_reads(source, seq_a)[0])
        self.assertFalse(run_reads(source, seq_b)[0])
        self.assertFalse(run_reads(source, seq_c)[0])
        self.assertTrue(run_reads(source, seq7_a)[0])
        self.assertTrue(run_reads(source, seq7_b)[0])
        self.assertTrue(run_reads(source, seq7_c)[0])
        self.assertTrue(run_reads(source, seq7_d)[0])
        mutant, count = re.subn(
            r"(if \(m_ToggleLowInOutage\)\s*\{\s*)m_ToggleLowSinceMs = nowMs;",
            r"\1",
            source,
            count=1)
        self.assertEqual(count, 1)
        self.assertTrue(run_reads(mutant, seq_a)[0])
        self.assertTrue(run_reads(mutant, seq_b)[0])
        self.assertFalse(run_reads(source, seq_a)[0])
        self.assertFalse(run_reads(source, seq_b)[0])


def make_step(source):
    powered = method(source, "LFPG_SetPowered")
    start = powered.index("if (!hasPower)")
    end = powered.index("bool newActive = m_LatchState")
    body = powered[start:end]
    arguments = [
        "hasPower", "hasToggle", "hasSet", "hasReset", "nowMs", "flip",
        "m_ToggleLastHigh", "m_ToggleLowSinceMs", "m_PowerSinceMs",
        "m_PowerLowSinceMs", "m_ToggleLowInOutage", "m_ToggleGraceUntilMs",
        "m_LatchState",
    ]
    stubs = {
        "LFPG_MCellToggleShouldFlip": flip_fn(source),
        "LFPG_MCellPowerRestartCounter": load(
            source, "LFPG_MCellPowerRestartCounter", POWER_ARGS, {}),
        "LFPG_MCELL_TOGGLE_REARM_MS": 1000,
    }
    compiled = scalar_function(body, arguments) + (
        "    return m_LatchState, m_ToggleLastHigh, m_ToggleLowSinceMs, "
        "m_PowerSinceMs, m_PowerLowSinceMs, m_ToggleLowInOutage, flip\n"
    )
    environment = {"__builtins__": {}, **stubs}
    exec(compile(compiled, "<source-slice:LFPG_SetPowered-toggle>", "exec"), environment)
    return environment["run"]


def run_reads(source, reads):
    step = make_step(source)
    last_high = True
    low_since = -1
    power_since = -1
    power_low = -1
    toggle_outage = False
    latch = False
    grace = 0
    flip = False
    for has_power, has_toggle, now_ms in reads:
        latch, last_high, low_since, power_since, power_low, toggle_outage, flip = step(
            has_power, has_toggle, False, False, now_ms, False,
            last_high, low_since, power_since, power_low, toggle_outage, grace, latch)
    return latch, last_high, flip


def run_toggle_sequence(source):
    reads = [
        (True, False, 0),
        (True, True, 1500),
        (True, False, 1800),
        (True, True, 2100),
        (False, False, 4000),
    ]
    latch, last_high, _ = run_reads(source, reads)
    return latch, last_high


def last_flip(source, reads):
    _, _, flip = run_reads(source, reads)
    return flip


if __name__ == "__main__":
    unittest.main()
