"""Independent graph oracle: positives, negatives, and fixture hand-calcs."""
from pathlib import Path
import json
import sys
import unittest

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from graph_reference import Oracle, load_graph
from graph_reference.model import Edge, Graph, Node

FIX = HERE / "graph_reference" / "fixtures"


def _fixture(name):
    return load_graph(FIX / name)


def _h2_graph():
    g = Graph()
    for n in [
        Node("src", "SOURCE", available=30),
        Node("bat", "PASSTHROUGH", pass_limit=100, soft_demand=20, soft_fraction=1.0),
        Node("l1", "CONSUMER", hard_demand=10),
        Node("l2", "CONSUMER", hard_demand=25),
    ]:
        g.nodes[n.id] = n
    for e in [Edge("a", "src", "bat"), Edge("b", "bat", "l1"), Edge("c", "src", "l2")]:
        g.edges[e.id] = e
    return g


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
        self.assertIn("partial_allocation", rules)
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


class H1ReachableDemand(unittest.TestCase):
    def test_closed_gate_correct_assignment_is_ok(self):
        # Reproductor H1: serving reachable hard + soft, ignoring blocked.
        g = _fixture("gates_hard_soft.json")
        report = Oracle().verify(
            g, {"e_open": 10.0, "e_hard": 10.0, "e_bat_in": 20.0}
        )
        rules = [v.rule for v in report.violations]
        self.assertTrue(report.ok, [(v.rule, v.where, v.detail) for v in report.violations])
        self.assertNotIn("hard_unmet", rules)
        self.assertNotIn("hard_priority", rules)


class H2SoftAbsorption(unittest.TestCase):
    def test_passthrough_battery_does_not_absorb_when_in_equals_out(self):
        # Reproductor H2: bat in 10 out 10; l2 underfed 20/25. No hard_priority.
        report = Oracle().verify(_h2_graph(), {"a": 10.0, "b": 10.0, "c": 20.0})
        rules = [v.rule for v in report.violations]
        self.assertFalse(report.ok)
        self.assertNotIn("hard_priority", rules)
        self.assertIn("hard_unmet", rules)
        self.assertIn("partial_allocation", rules)
        self.assertEqual(
            [v.where for v in report.violations if v.rule == "partial_allocation"],
            ["l2"],
        )


class H3TwoSidedConservation(unittest.TestCase):
    def test_combiner_waste_is_conservation(self):
        g = _fixture("combiner_20_50_hard50.json")
        report = Oracle().verify(g, {"e_s20": 20.0, "e_s50": 50.0, "e_out": 50.0})
        self.assertFalse(report.ok)
        self.assertIn("conservation", [v.rule for v in report.violations])
        self.assertEqual(
            [v.where for v in report.violations if v.rule == "conservation"],
            ["comb"],
        )

    def test_consumer_over_allocation(self):
        g = _fixture("combiner_20_50_hard50.json")
        report = Oracle().verify(g, {"e_s20": 20.0, "e_s50": 50.0, "e_out": 70.0})
        self.assertFalse(report.ok)
        self.assertIn("over_allocation", [v.rule for v in report.violations])
        self.assertEqual(
            [v.where for v in report.violations if v.rule == "over_allocation"],
            ["load"],
        )


class H4BinaryConsumers(unittest.TestCase):
    def test_heterogeneous_integer_measure_is_zero(self):
        g = _fixture("heterogeneous_deficit.json")
        o = Oracle()
        self.assertAlmostEqual(o.max_hard_servable(g), 0.0, places=6)
        self.assertAlmostEqual(o.max_hard_flow_bound(g), 25.0, places=6)
        self.assertFalse(o.hard_feasible(g))

    def test_partial_allocation_negative(self):
        g = _fixture("heterogeneous_deficit.json")
        report = Oracle().verify(
            g, {"e_s10": 10.0, "e_s15": 15.0, "e_out": 25.0}
        )
        self.assertFalse(report.ok)
        self.assertIn("partial_allocation", [v.rule for v in report.violations])
        self.assertIn("hard_unmet", [v.rule for v in report.violations])

    def test_full_feed_is_not_partial(self):
        g = _fixture("combiner_20_50_hard50.json")
        report = Oracle().verify(
            g, {"e_s20": 20.0, "e_s50": 30.0, "e_out": 50.0}
        )
        self.assertTrue(report.ok, [v.detail for v in report.violations])
        self.assertNotIn("partial_allocation", [v.rule for v in report.violations])


class H5ForcedSharedSource(unittest.TestCase):
    def test_unique_feasible_assignment(self):
        g = _fixture("shared_source_forced_split.json")
        report = Oracle().verify(
            g,
            {
                "e_s50_split": 50.0,
                "e_split_comb": 30.0,
                "e_split_l2": 20.0,
                "e_s20_comb": 20.0,
                "e_comb_l1": 50.0,
            },
        )
        self.assertTrue(report.ok, [(v.rule, v.detail) for v in report.violations])
        self.assertAlmostEqual(Oracle().max_hard_servable(g), 70.0, places=6)

    def test_equal_split_at_comb_leaves_l1_unfed(self):
        g = _fixture("shared_source_forced_split.json")
        report = Oracle().verify(
            g,
            {
                "e_s50_split": 45.0,
                "e_split_comb": 25.0,
                "e_split_l2": 20.0,
                "e_s20_comb": 0.0,
                "e_comb_l1": 25.0,
            },
        )
        rules = [v.rule for v in report.violations]
        self.assertFalse(report.ok)
        self.assertIn("hard_unmet", rules)
        self.assertIn("partial_allocation", rules)
        self.assertIn("feasible_but_underfed", rules)
        self.assertIn("l1", [v.where for v in report.violations if v.rule == "hard_unmet"])


class FixtureExpectations(unittest.TestCase):
    def test_every_fixture_matches_hand_calc_expected(self):
        oracle = Oracle()
        files = sorted(FIX.glob("*.json"))
        self.assertGreaterEqual(len(files), 7)
        for path in files:
            with self.subTest(fixture=path.name):
                payload = json.loads(path.read_text(encoding="utf-8"))
                expected = payload["expected"]
                self.assertIn("hand_calc", expected)
                self.assertTrue(expected["hand_calc"].strip())
                g = load_graph(payload)
                max_hard = oracle.max_hard_servable(g)
                bound = oracle.max_hard_flow_bound(g)
                feasible = oracle.hard_feasible(g)
                self.assertAlmostEqual(
                    oracle.reachable_hard_demand(g),
                    float(expected["total_hard_demand"]),
                    places=6,
                )
                self.assertAlmostEqual(
                    max_hard, float(expected["max_hard_servable"]), places=6
                )
                self.assertEqual(feasible, bool(expected["hard_feasible"]))
                if "max_hard_flow_bound" in expected:
                    self.assertAlmostEqual(
                        bound, float(expected["max_hard_flow_bound"]), places=6
                    )


class P1HardPriorityIslands(unittest.TestCase):
    def test_disjoint_islands_do_not_raise_hard_priority(self):
        g = Graph()
        for n in [
            Node("srcA", "SOURCE", available=30),
            Node("bat", "PASSTHROUGH", pass_limit=100, soft_demand=20, soft_fraction=1.0),
            Node("srcB", "SOURCE", available=10),
            Node("l", "CONSUMER", hard_demand=25),
        ]:
            g.nodes[n.id] = n
        for e in [Edge("a", "srcA", "bat"), Edge("b", "srcB", "l")]:
            g.edges[e.id] = e
        report = Oracle().verify(g, {"a": 20.0, "b": 0.0})
        rules = [v.rule for v in report.violations]
        self.assertEqual(set(rules), {"hard_unmet"})
        self.assertEqual(
            [v.where for v in report.violations],
            ["l"],
        )

    def test_shared_source_raises_hard_priority_on_battery(self):
        g = Graph()
        for n in [
            Node("src", "SOURCE", available=30),
            Node("bat", "PASSTHROUGH", pass_limit=100, soft_demand=20, soft_fraction=1.0),
            Node("l2", "CONSUMER", hard_demand=25),
        ]:
            g.nodes[n.id] = n
        for e in [Edge("a", "src", "bat"), Edge("b", "src", "l2")]:
            g.edges[e.id] = e
        report = Oracle().verify(g, {"a": 20.0, "b": 0.0})
        rules = [v.rule for v in report.violations]
        self.assertFalse(report.ok)
        self.assertIn("hard_unmet", rules)
        self.assertIn("feasible_but_underfed", rules)
        self.assertIn("hard_priority", rules)
        pri = [v for v in report.violations if v.rule == "hard_priority"]
        self.assertEqual(len(pri), 1)
        self.assertEqual(pri[0].where, "bat")
        self.assertIn("l2", pri[0].detail)
        self.assertIn("src", pri[0].detail)


class ExtraNegatives(unittest.TestCase):
    def test_cut_edge_flow_is_violation(self):
        g = _fixture("cut_edge.json")
        report = Oracle().verify(g, {"e_cut": 40.0})
        self.assertFalse(report.ok)
        self.assertIn("edge_disabled", [v.rule for v in report.violations])

    def test_hard_priority_soft_while_unmet(self):
        g = _fixture("gates_hard_soft.json")
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
        self.assertEqual(
            [v.where for v in report.violations if v.rule == "hard_priority"],
            ["battery"],
        )
        self.assertIn("hard_unmet", [v.rule for v in report.violations])
        self.assertIn(
            "hard_load",
            [v.where for v in report.violations if v.rule == "hard_unmet"],
        )
        self.assertNotIn(
            "blocked",
            [v.where for v in report.violations if v.rule == "hard_unmet"],
        )

    def test_battery_full_accepts_hard_only(self):
        g = _fixture("battery_full.json")
        report = Oracle().verify(g, {"e_load": 10.0, "e_bat": 0.0})
        self.assertTrue(report.ok, [v.detail for v in report.violations])

    def test_shared_source_starves_one_island(self):
        g = _fixture("shared_source_two_islands.json")
        report = Oracle().verify(g, {"e_a": 40.0, "e_b": 0.0})
        self.assertFalse(report.ok)
        self.assertIn("hard_unmet", [v.rule for v in report.violations])
        self.assertIn("over_allocation", [v.rule for v in report.violations])
        self.assertIn("feasible_but_underfed", [v.rule for v in report.violations])


if __name__ == "__main__":
    unittest.main()
