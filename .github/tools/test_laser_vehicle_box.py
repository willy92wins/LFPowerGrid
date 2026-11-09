"""Laser occupied-vehicle AABB vs beam segment; production slice."""
from pathlib import Path
from types import SimpleNamespace
import re
import sys
import unittest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from enforce_scalar_slice import load, method

LASER = ROOT / "scripts/4_World/LFPG_LaserDetector.c"
ARGS = ["ax", "ay", "az", "bx", "by", "bz", "minX", "minY", "minZ", "maxX", "maxY", "maxZ"]
BOX = (-1.0, -1.0, -1.0, 1.0, 1.0, 1.0)
SWAP_ONE = r"if \(t1 > t2\)\s*\{\s*swap = t1;\s*t1 = t2;\s*t2 = swap;\s*\}"


def hits_fn(source=None):
    text = source if source is not None else LASER.read_text(encoding="utf-8")
    stubs = {"Math": SimpleNamespace(AbsFloat=lambda value: abs(value))}
    return load(text, "LFPG_LaserSegmentHitsBox", ARGS, stubs)


def run(fn, a, b):
    return fn(a[0], a[1], a[2], b[0], b[1], b[2], *BOX)


class LaserVehicleBox(unittest.TestCase):
    def test_positive_and_negative_cases(self):
        fn = hits_fn()
        self.assertTrue(run(fn, (-3.0, 0.0, 0.0), (3.0, 0.0, 0.0)))
        self.assertTrue(run(fn, (3.0, 0.0, 0.0), (0.0, 0.0, 0.0)))
        self.assertTrue(run(fn, (-0.2, 0.1, 0.0), (0.3, -0.1, 0.2)))
        self.assertTrue(run(fn, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)))
        self.assertFalse(run(fn, (-3.0, 0.0, 0.0), (-1.5, 0.0, 0.0)))
        self.assertFalse(run(fn, (-3.0, 2.0, 0.0), (3.0, 2.0, 0.0)))
        self.assertFalse(run(fn, (-3.0, 1.5, 0.0), (3.0, 1.5, 0.0)))
        self.assertFalse(run(fn, (2.0, 1.05, 0.0), (1.05, 2.0, 0.0)))
        self.assertFalse(run(fn, (2.0, 0.0, 0.0), (2.0, 0.0, 0.0)))

    def test_mutant_without_axis_swap_fails_a_case(self):
        source = LASER.read_text(encoding="utf-8")
        body = method(source, "LFPG_LaserSegmentHitsBox")
        mutant_body, count = re.subn(SWAP_ONE, "", body, count=1)
        if count != 1:
            raise AssertionError("x-axis t1/t2 swap missing")
        mutant = source.replace(body, mutant_body, 1)
        fn = hits_fn(mutant)
        self.assertFalse(run(fn, (3.0, 0.0, 0.0), (0.0, 0.0, 0.0)))
        self.assertTrue(run(hits_fn(source), (3.0, 0.0, 0.0), (0.0, 0.0, 0.0)))

    def test_mutant_without_vehicle_raycast_refresh_fails(self):
        source = LASER.read_text(encoding="utf-8")
        crossing = method(source, "LFPG_EvaluateCrossing")
        self.assertEqual(crossing.count("LFPG_UpdateBeamRaycast()"), 1)
        self.assertIn("m_BeamLength < LFPG_LASER_BEAM_RANGE_M", crossing)
        self.assertIn("refreshedRaycast", crossing)
        mutant, count = re.subn(
            r"LFPG_UpdateBeamRaycast\(\);\s*refreshedRaycast = true;",
            "refreshedRaycast = true;",
            source,
            count=1)
        self.assertEqual(count, 1)
        self.assertEqual(method(mutant, "LFPG_EvaluateCrossing").count("LFPG_UpdateBeamRaycast()"), 0)
        self.assertEqual(method(source, "LFPG_EvaluateCrossing").count("LFPG_UpdateBeamRaycast()"), 1)

    def test_short_beam_still_reaches_vehicle_box(self):
        source = LASER.read_text(encoding="utf-8")
        crossing = method(source, "LFPG_EvaluateCrossing")
        short_at = crossing.index("m_BeamLength < 0.1")
        loop_at = crossing.index("for (i = 0; i < pCount;")
        self.assertLess(short_at, loop_at)
        self.assertIn("beamTooShort", crossing[short_at:loop_at])
        self.assertNotIn("return false;", crossing[short_at:loop_at])
        self.assertIn("beamTooShort && !detected", crossing)
        self.assertNotIn("segmentLen = 0.1", crossing)
        refresh_short = (
            r"LFPG_UpdateBeamRaycast\(\);\s*refreshedRaycast = true;"
            r"\s*segmentLen = m_BeamLength;\s*beamTooShort = false;"
            r"\s*if \(m_BeamLength < 0\.1\)\s*\{\s*beamTooShort = true;\s*\}"
        )
        self.assertRegex(source, refresh_short)
        mutant, count = re.subn(refresh_short, r"LFPG_UpdateBeamRaycast();\n                    refreshedRaycast = true;\n                    segmentLen = m_BeamLength;", source, count=1)
        self.assertEqual(count, 1)
        self.assertRegex(source, r"segmentLen = m_BeamLength;\s*beamTooShort = false;")
        self.assertIsNone(re.search(r"segmentLen = m_BeamLength;\s*beamTooShort = false;", mutant))


if __name__ == "__main__":
    unittest.main()
