"""Regression of actual refresh statements with stubs; not Enforce/in-game."""
from pathlib import Path
from types import SimpleNamespace
import re
import unittest
from enforce_scalar_slice import load

ROOT = Path(__file__).resolve().parents[2]
GRAPH = ROOT / "scripts/5_Mission/LFPG_ElecGraphImpl.c"


class RefreshCapacity(unittest.TestCase):
    def run_source(self, kind, cached, live, consumption=0.0, old_consumption=0.0,
                   entity_present=True, source=None):
        node = SimpleNamespace(m_DeviceType=kind, m_MaxOutput=cached,
                               m_Consumption=old_consumption, m_Powered=False)
        device = SimpleNamespace(capacity=live, consumption=consumption, on=True)
        dirty, upstream = [], []
        registry = SimpleNamespace(FindById=lambda _: device if entity_present else None)
        stubs = {
            "LFPG_DeviceType": SimpleNamespace(SOURCE=1, CONSUMER=2, PASSTHROUGH=3, CAMERA=4),
            "LFPG_DeviceAPI": SimpleNamespace(GetCapacity=lambda d: d.capacity,
                                             GetConsumption=lambda d: d.consumption,
                                             ResolveVanillaDevice=lambda _: None),
            "LFPG_DeviceRegistry": SimpleNamespace(Get=lambda: registry),
            "GetNode": lambda _: node, "ReadLiveSourceOn": lambda d: d.on,
            "MarkNodeDirty": lambda key, mask: dirty.append((key, mask)),
            "MarkUpstreamNodesDirty": upstream.append,
            "LFPG_PROPAGATION_EPSILON": 0.001,
            "LFPG_DEFAULT_PASSTHROUGH_CAPACITY": 200.0,
            "LFPG_DIRTY_INTERNAL": 4,
        }
        load(source or GRAPH.read_text(encoding="utf-8"), "RefreshSourceState", ["nodeId"], stubs)("adapter")
        return node, dirty, upstream

    def test_attachment_capacity_tracks_current_device(self):
        for cached, actual, expected in [(200, 40, 40), (60, 40, 40), (40, 60, 60), (40, 0, 200)]:
            with self.subTest(cached=cached, actual=actual):
                node, dirty, upstream = self.run_source(3, cached, actual)
                self.assertEqual(node.m_MaxOutput, expected)
                self.assertEqual(dirty, [("adapter", 4)])
                self.assertEqual(upstream, ["adapter"])

    def test_unchanged_properties_do_not_traverse_upstream(self):
        node, dirty, upstream = self.run_source(3, 40, 40)
        self.assertEqual(node.m_MaxOutput, 40)
        self.assertEqual(upstream, [])
        self.assertEqual(dirty, [("adapter", 4)])

    def test_passthrough_consumption_change_is_not_lost(self):
        node, _, upstream = self.run_source(3, 40, 40, consumption=10)
        self.assertEqual(node.m_Consumption, 10)
        self.assertEqual(upstream, ["adapter"])

    def test_source_and_consumers_keep_their_refresh_contract(self):
        node, _, upstream = self.run_source(1, 20, 50)
        self.assertEqual(node.m_MaxOutput, 50)
        self.assertTrue(node.m_Powered)
        self.assertEqual(upstream, [])
        for kind in [2, 4]:
            node, _, upstream = self.run_source(kind, 40, 60, consumption=10)
            self.assertEqual(node.m_MaxOutput, 40)
            self.assertEqual(node.m_Consumption, 10)
            self.assertEqual(upstream, ["adapter"])

    def test_missing_entity_and_unknown_type_are_noops(self):
        for kind, present in [(3, False), (99, True)]:
            node, dirty, upstream = self.run_source(kind, 40, 60, entity_present=present)
            self.assertEqual(node.m_MaxOutput, 40)
            self.assertEqual((dirty, upstream), ([], []))

    def test_negative_control_missing_passthrough_assignment(self):
        source = GRAPH.read_text(encoding="utf-8")
        mutant, count = re.subn(r"node.m_MaxOutput = capacity;", "node.m_MaxOutput = node.m_MaxOutput;", source)
        self.assertEqual(count, 1)
        node, _, _ = self.run_source(3, 200, 40, source=mutant)
        self.assertNotEqual(node.m_MaxOutput, 40)


if __name__ == "__main__":
    unittest.main()
