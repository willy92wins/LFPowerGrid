"""Memory Cell Toggle rising-edge predicate; production slice plus source contracts."""
from pathlib import Path
import re
import sys
import unittest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from enforce_scalar_slice import load, method

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


if __name__ == "__main__":
    unittest.main()
