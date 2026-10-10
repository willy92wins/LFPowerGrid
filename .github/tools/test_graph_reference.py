"""Independent graph oracle: positives, negatives, and fixture hand-calcs."""
from pathlib import Path
import json
import sys
import unittest

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from graph_reference import Oracle, load_graph

FIX = HERE / "graph_reference" / "fixtures"


def _fixture(name):
    return load_graph(FIX / name)


class CombinerG01(unittest.TestCase):
    def setUp(self):
        self.g = _fixture("combiner_20_50_hard50.json")
        self.o = Oracle()

    def test_equal_split_overload_is_violation(self):
        # Today's solver: source 20 overloads on equal 25, assigns 0;
        # source 50 assigns 25; consumer gets 25 although 20+30 is feasible.
        report = self.o.verify(
            self.g, {"e_s20": 0.0, "e_s50": 25.0, "e_out": 25.0}
        )
        rules = [v.rule for v in report.violations]
        self.assertFalse(report.ok)
        self.assertIn("hard_unmet", rules)
        self.assertIn("feasible_but_underfed", rules)

    def test_accepts_20_plus_30(self):
        report = self.o.verify(
            self.g, {"e_s20": 20.0, "e_s50": 30.0, "e_out": 50.0}
        )
        self.assertTrue(report.ok, [v.detail for v in report.violations])

    def test_non_conservation_is_violation(self):
        report = self.o.verify(
            self.g, {"e_s20": 20.0, "e_s50": 30.0, "e_out": 80.0}
        )
        self.assertFalse(report.ok)
        self.assertIn("conservation", [v.rule for v in report.violations])

    def test_source_limit_exceeded_is_violation(self):
        report = self.o.verify(
            self.g, {"e_s20": 40.0, "e_s50": 10.0, "e_out": 50.0}
        )
        self.assertFalse(report.ok)
        self.assertIn("source_limit", [v.rule for v in report.violations])
        self.assertEqual(
            [v.where for v in report.violations if v.rule == "source_limit"],
            ["s20"],
        )


class FixtureExpectations(unittest.TestCase):
    def test_every_fixture_matches_hand_calc_expected(self):
        oracle = Oracle()
        files = sorted(FIX.glob("*.json"))
        self.assertGreaterEqual(len(files), 6)
        for path in files:
            with self.subTest(fixture=path.name):
                payload = json.loads(path.read_text(encoding="utf-8"))
                expected = payload["expected"]
                self.assertIn("hand_calc", expected)
                self.assertTrue(expected["hand_calc"].strip())
                g = load_graph(payload)
                max_hard = oracle.max_hard_servable(g)
                feasible = oracle.hard_feasible(g)
                self.assertAlmostEqual(
                    g.total_hard_demand(),
                    float(expected["total_hard_demand"]),
                    places=6,
                )
                self.assertAlmostEqual(
                    max_hard, float(expected["max_hard_servable"]), places=6
                )
                self.assertEqual(feasible, bool(expected["hard_feasible"]))


class ExtraNegatives(unittest.TestCase):
    def test_cut_edge_flow_is_violation(self):
        g = _fixture("cut_edge.json")
        report = Oracle().verify(g, {"e_cut": 40.0})
        self.assertFalse(report.ok)
        self.assertIn("edge_disabled", [v.rule for v in report.violations])

    def test_hard_priority_soft_while_unmet(self):
        g = _fixture("gates_hard_soft.json")
        # Feed the battery (soft) and starve the reachable hard load.
        report = Oracle().verify(
            g,
            {
                "e_closed": 0.0,
                "e_blocked": 0.0,
                "e_open": 0.0,
                "e_hard": 0.0,
                "e_bat_in": 20.0,
            },
        )
        self.assertFalse(report.ok)
        self.assertIn("hard_priority", [v.rule for v in report.violations])
        self.assertIn("hard_unmet", [v.rule for v in report.violations])

    def test_battery_full_accepts_hard_only(self):
        g = _fixture("battery_full.json")
        report = Oracle().verify(g, {"e_load": 10.0, "e_bat": 0.0})
        self.assertTrue(report.ok, [v.detail for v in report.violations])

    def test_shared_source_starves_one_island(self):
        g = _fixture("shared_source_two_islands.json")
        report = Oracle().verify(g, {"e_a": 40.0, "e_b": 0.0})
        self.assertFalse(report.ok)
        self.assertIn("hard_unmet", [v.rule for v in report.violations])
        self.assertIn("feasible_but_underfed", [v.rule for v in report.violations])


if __name__ == "__main__":
    unittest.main()
