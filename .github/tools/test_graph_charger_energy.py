"""Actual charger clock statements with stubs; no Enforce or native callbacks.

Find(out) expands into membership/default lookup, reference locals start null,
and the single slot literal becomes a stub constant. Remaining if/assignment/
calls come directly from production. Native EM clamping is stubbed to its
documented contract. Independent event durations provide the energy oracle.
"""
from pathlib import Path
from types import SimpleNamespace
import re
import unittest
from enforce_scalar_slice import load, method, scalar_function

ROOT = Path(__file__).resolve().parents[2]
GRAPH = ROOT / "scripts/5_Mission/LFPG_ElecGraphImpl.c"
MANAGER = ROOT / "scripts/5_Mission/LFPG_NetworkManagerImpl.c"
DEFINES = ROOT / "scripts/3_Game/LFPG_Defines.c"


class FieldMap(dict):
    def Get(self, key, default=None):
        return self.get(key, default)

    def Contains(self, key):
        return key in self

    def Set(self, key, value):
        self[key] = value

    def Remove(self, key):
        if key in self:
            del self[key]


class Battery:
    def __init__(self, energy=0.0, maximum=1500.0):
        self.energy, self.maximum = energy, maximum
        self.added, self.quantity = [], None
        self.on_add = None

    def GetCompEM(self):
        return self

    def GetEnergy(self):
        return self.energy

    def GetEnergyMax(self):
        return self.maximum

    def GetEnergy0To1(self):
        return self.energy / self.maximum

    def AddEnergy(self, amount):
        self.added.append(amount)
        self.energy = min(self.maximum, max(0.0, self.energy + amount))
        if self.on_add:
            self.on_add()

    def SetQuantityNormalized(self, amount):
        self.quantity = amount


class Charger:
    def __init__(self, battery):
        self.battery, self.switched = battery, True

    def GetCompEM(self):
        return self

    def IsSwitchedOn(self):
        return self.switched

    def FindAttachmentBySlotName(self, name):
        assert name == "LargeBattery"
        return self.battery


class Clock:
    def __init__(self, source=None, battery=None):
        self.source = source or GRAPH.read_text(encoding="utf-8")
        self.battery = battery if battery is not None else Battery()
        self.charger, self.now = Charger(self.battery), 0.0
        self.maps = {name: FieldMap() for name in ["m_ChargerEntities",
            "m_ChargerBatteries", "m_ChargerLastChargeSec", "m_ChargerCharging"]}
        self.maps["m_ChargerEntities"]["charger"] = self.charger
        self.maps["m_ChargerLastChargeSec"]["charger"] = 0.0
        self.maps["m_ChargerCharging"]["charger"] = False
        rate = re.search(r"LFPG_CHARGER_ENERGY_PER_SEC\s*=\s*([\d.]+)",
                         DEFINES.read_text(encoding="utf-8"))
        self.rate = float(rate[1])
        cast = SimpleNamespace(Cast=lambda obj: obj)
        stubs = {**self.maps, "g_Game": SimpleNamespace(GetTime=lambda: self.now * 1000),
                 "EntityAI": cast, "ItemBase": cast, "BATTERY_SLOT": "LargeBattery",
                 "Math": SimpleNamespace(Min=min), "LFPG_CHARGER_ENERGY_PER_SEC": self.rate}
        stubs["ChargerChargeAmount"] = load(self.source, "ChargerChargeAmount",
            ["nowSec", "lastSec", "wasCharging", "sameBattery"], stubs)
        body = method(self.source, "UpdateVanillaChargerPower")
        old = "if (!m_ChargerEntities.Find(nodeId, chargerRaw))"
        assert body.count(old) == 1
        body = body.replace(old, "chargerRaw = m_ChargerEntities.Get(nodeId);\n"
                            "if (!m_ChargerEntities.Contains(nodeId))")
        for field, local in [("m_ChargerLastChargeSec", "lastSec"),
                             ("m_ChargerCharging", "wasCharging"),
                             ("m_ChargerBatteries", "lastBattery")]:
            old = field + ".Find(nodeId, " + local + ");"
            assert body.count(old) == 1
            body = body.replace(old, local + " = " + field + ".Get(nodeId, " + local + ");")
        body = body.replace('"LargeBattery"', "BATTERY_SLOT")
        body = re.sub(r"\b(Managed|EntityAI|ComponentEnergyManager|ItemBase)\s+(\w+)\s*;",
                      r"\2 = null;", body)
        body = re.sub(r"\b(Managed|ComponentEnergyManager|ItemBase)\s+(\w+)\s*=", r"\2 =", body)
        environment = {"__builtins__": {}, **stubs}
        exec(compile(scalar_function(body, ["nodeId", "powered"]),
                     "<source-slice:UpdateVanillaChargerPower>", "exec"), environment)
        self.update = environment["run"]
        self.node_powered = False
        stubs["GetNode"] = lambda nid: SimpleNamespace(m_Powered=self.node_powered)
        stubs["UpdateVanillaChargerPower"] = lambda nid, pwr: self.update(nid, pwr)
        self.close_attach = None
        try:
            self.close_attach = load(self.source, "CloseVanillaChargerAttachment",
                                     ["nodeId", "detached"], stubs)
        except ValueError:
            pass

    def visit(self, at, powered):
        self.now = at
        self.update("charger", powered)

    def hook_notify(self, at, powered, detached, slot_empty_at_call):
        self.now = at
        self.node_powered = powered
        if slot_empty_at_call:
            self.charger.battery = None
        self.close_attach("charger", detached)
        if detached and not slot_empty_at_call:
            self.charger.battery = None


class ChargerClock(unittest.TestCase):
    def test_equal_energy_at_different_visit_cadences(self):
        # Independent oracle: 640 powered seconds at the declared 1u/s rate.
        for cadence in [0.1, 1, 16, 64]:
            with self.subTest(cadence=cadence):
                clock = Clock()
                self.assertEqual(clock.rate, 1.0)
                clock.visit(0, True)
                for index in range(1, round(640 / cadence) + 1):
                    clock.visit(index * cadence, True)
                self.assertAlmostEqual(clock.battery.energy, 640.0, places=7)

    def test_outage_closes_previous_interval_without_crediting_off_time(self):
        clock = Clock()
        clock.visit(1, True)
        clock.visit(2, False)
        clock.visit(50, False)
        clock.visit(62, True)
        clock.visit(63, True)
        # Powered [1,2] and [62,63]: two seconds, independent of visit gaps.
        self.assertEqual(clock.battery.energy, 2.0)

    def test_first_visit_and_unchanged_timestamp_add_nothing(self):
        clock = Clock()
        clock.visit(100, True)
        clock.visit(100, True)
        self.assertEqual(clock.battery.added, [])

    def test_battery_replacement_does_not_inherit_previous_battery_time(self):
        clock = Clock()
        clock.visit(0, True)
        replacement = Battery()
        clock.charger.battery = replacement
        clock.visit(64, True)
        self.assertEqual(replacement.energy, 0.0)
        clock.visit(65, True)
        self.assertEqual(replacement.energy, 1.0)
        self.assertEqual(clock.battery.energy, 0.0)

    def test_detach_and_later_attach_rebase_the_clock(self):
        clock = Clock()
        clock.visit(1, True)
        clock.charger.battery = None
        clock.visit(2, True)
        clock.visit(62, True)
        clock.charger.battery = clock.battery
        clock.visit(63, True)
        clock.visit(64, True)
        self.assertEqual(clock.battery.energy, 1.0)

    def test_full_battery_clamps_and_quantity_matches_actual_energy(self):
        clock = Clock(battery=Battery(499.5, 500))
        clock.visit(0, True)
        clock.visit(64, True)
        self.assertEqual(clock.battery.energy, 500.0)
        self.assertEqual(clock.battery.quantity, 1.0)
        clock.visit(128, True)
        self.assertEqual(len(clock.battery.added), 1)

    def test_clock_reversal_does_not_remove_energy(self):
        clock = Clock()
        clock.visit(100, True)
        clock.visit(90, True)
        self.assertEqual(clock.battery.added, [])
        clock.visit(91, True)
        self.assertEqual(clock.battery.energy, 1.0)

    def test_missing_charger_and_missing_energy_manager_do_not_charge(self):
        clock = Clock()
        clock.maps["m_ChargerEntities"].clear()
        clock.visit(64, True)
        self.assertEqual(clock.battery.added, [])
        clock = Clock()
        clock.charger.GetCompEM = lambda: None
        clock.visit(0, True)
        clock.visit(64, True)
        self.assertEqual(clock.battery.added, [])

    def test_clock_is_published_before_energy_callback_reenters(self):
        clock = Clock()
        clock.visit(1, True)
        clock.battery.on_add = lambda: clock.update("charger", False)
        clock.visit(2, True)
        self.assertEqual(clock.battery.energy, 1.0)
        self.assertFalse(clock.maps["m_ChargerCharging"]["charger"])

    def test_clock_is_closed_in_all_graph_poweroff_and_cleanup_paths(self):
        graph, manager = GRAPH.read_text(encoding="utf-8"), MANAGER.read_text(encoding="utf-8")
        pairs = [(method(graph, "OnWireRemoved"), "targetId", "tgtObj"),
                 (method(graph, "ResetOrphanSyncVars"), "deviceId", "orphanObj"),
                 (method(manager, "PostBulkRebuildAndPropagate"), "pendingPowerId", "pendingPowerDevice"),
                 (method(manager, "CleanDisappearedVanillaDevice"), "neighborIds[ni]", "neighborDev")]
        for body, key, entity in pairs:
            self.assertLess(body.index("UpdateVanillaChargerPower(" + key + ", false)"),
                            body.index("LFPG_DeviceAPI.SetPowered(" + entity + ", false)"))
        self.assertLess(method(graph, "RebuildFromWires").index("ClearVanillaChargers()"),
                        method(graph, "RebuildFromWires").index("m_Nodes.Clear()"))
        self.assertLess(method(graph, "UntrackVanillaCharger").index("UpdateVanillaChargerPower"),
                        method(graph, "UntrackVanillaCharger").index("m_ChargerLastChargeSec.Remove"))

    def test_scheduler_uses_bounded_charger_registry_separate_from_consumer_sweep(self):
        graph = GRAPH.read_text(encoding="utf-8")
        tick = method(graph, "TickVanillaChargers")
        self.assertIn("visits = LFPG_VALIDATE_BATCH_SIZE", tick)
        self.assertIn("m_ChargerIds[m_ChargerCursor]", tick)
        for forbidden in ["m_Nodes.GetKey", "m_Nodes.GetElement", "new "]:
            self.assertNotIn(forbidden, tick)
        self.assertNotIn("AddEnergy", method(graph, "ValidateConsumerStates"))
        manager = MANAGER.read_text(encoding="utf-8")
        self.assertRegex(manager, r"if \(m_SchedSimpleMs >= 1000\)[\s\S]*?m_Graph.TickVanillaChargers\(\)")

    def test_negative_controls_ten_second_cap_and_lost_poweroff_state(self):
        source = GRAPH.read_text(encoding="utf-8")
        capped, count = re.subn(r"return \(nowSec - lastSec\) \* LFPG_CHARGER_ENERGY_PER_SEC;",
                               "return Math.Min(nowSec - lastSec, 10.0) * LFPG_CHARGER_ENERGY_PER_SEC;", source)
        self.assertEqual(count, 1)
        clock = Clock(capped)
        clock.visit(0, True)
        clock.visit(64, True)
        self.assertNotEqual(clock.battery.energy, 64.0)
        lost, count = re.subn(r"m_ChargerCharging.Set\(nodeId, charging\);",
                             "m_ChargerCharging.Set(nodeId, true);", source)
        self.assertEqual(count, 1)
        clock = Clock(lost)
        clock.visit(1, True)
        clock.visit(2, False)
        clock.visit(62, True)
        clock.visit(63, True)
        self.assertNotEqual(clock.battery.energy, 2.0)

    def test_g02_energy_per_fed_time_is_independent_of_node_count_and_dirty_queue(self):
        # Node count cannot change charger energy: the tick never walks m_Nodes.
        graph = GRAPH.read_text(encoding="utf-8")
        tick = method(graph, "TickVanillaChargers")
        self.assertNotIn("ProcessDirtyQueue", tick)
        self.assertNotIn("m_Nodes", tick)
        self.assertNotIn("m_DirtyQueue", tick)
        self.assertNotIn("GetDirtyQueueSize", tick)
        fed = 640.0
        for chargers in [1, 64]:
            cadence = max(1, (chargers + 31) // 32)
            clock = Clock()
            clock.visit(0, True)
            steps = round(fed / cadence)
            for index in range(1, steps + 1):
                clock.visit(index * cadence, True)
            self.assertAlmostEqual(clock.battery.energy, fed, places=7)
        manager = MANAGER.read_text(encoding="utf-8")
        simple = method(manager, "LFPG_ServerSchedulerTick")
        self.assertIn("TickVanillaChargers()", simple)

    def test_g02_tick_visits_and_latency_bound_from_production_constants(self):
        defines = DEFINES.read_text(encoding="utf-8")
        batch = int(re.search(r"LFPG_VALIDATE_BATCH_SIZE\s*=\s*(\d+)", defines)[1])
        self.assertEqual(batch, 32)
        manager = MANAGER.read_text(encoding="utf-8")
        period_ms = int(re.search(r"m_SchedSimpleMs >= (\d+)", manager)[1])
        self.assertEqual(period_ms, 1000)
        tick = method(GRAPH.read_text(encoding="utf-8"), "TickVanillaChargers")
        self.assertIn("visits = m_ChargerIds.Count()", tick)
        self.assertIn("if (visits > LFPG_VALIDATE_BATCH_SIZE)", tick)
        self.assertIn("visits = LFPG_VALIDATE_BATCH_SIZE", tick)
        period_s = period_ms / 1000.0
        charger_count = 512
        visits_per_call = charger_count if charger_count < batch else batch
        max_gap_s = ((charger_count + batch - 1) // batch) * period_s
        self.assertEqual(visits_per_call, batch)
        self.assertEqual(max_gap_s, 16.0)

    def test_g03a_switch_poll_error_bound_from_constants(self):
        rate = float(re.search(r"LFPG_CHARGER_ENERGY_PER_SEC\s*=\s*([\d.]+)",
                               DEFINES.read_text(encoding="utf-8"))[1])
        batch = int(re.search(r"LFPG_VALIDATE_BATCH_SIZE\s*=\s*(\d+)",
                              DEFINES.read_text(encoding="utf-8"))[1])
        period_s = int(re.search(r"m_SchedSimpleMs >= (\d+)",
                                 MANAGER.read_text(encoding="utf-8"))[1]) / 1000.0
        clock = Clock()
        clock.visit(0, True)
        clock.charger.switched = False
        clock.visit(period_s, True)
        self.assertAlmostEqual(clock.battery.energy, period_s * rate, places=7)
        charger_count = 32
        max_error_u = ((charger_count + batch - 1) // batch) * period_s * rate
        self.assertEqual(max_error_u, 1.0)

    def test_g03b_detach_attach_poll_both_engine_orders(self):
        hook = (ROOT / "scripts/4_World/LFPG_BatteryChargerMod.c").read_text(encoding="utf-8")
        attach = method(hook, "EEItemAttached")
        detach = method(hook, "EEItemDetached")
        self.assertIn("NotifyVanillaChargerAttachment(this, false)", attach)
        self.assertIn("NotifyVanillaChargerAttachment(this, true)", detach)
        graph = GRAPH.read_text(encoding="utf-8")
        notify = method(graph, "NotifyVanillaChargerAttachment")
        self.assertIn("CloseVanillaChargerAttachment(nodeId, detached)", notify)
        close = method(graph, "CloseVanillaChargerAttachment")
        self.assertIn("UpdateVanillaChargerPower(nodeId, powered)", close)
        self.assertIn("if (detached)", close)
        self.assertIn("m_ChargerBatteries.Remove(nodeId)", close)
        clock = Clock()
        clock.visit(1, True)
        clock.hook_notify(2, True, True, True)
        clock.charger.battery = clock.battery
        clock.hook_notify(40, True, False, False)
        clock.visit(63, True)
        self.assertEqual(clock.battery.energy, 23.0)
        clock = Clock()
        clock.visit(1, True)
        clock.hook_notify(2, True, True, False)
        clock.charger.battery = clock.battery
        clock.hook_notify(40, True, False, False)
        clock.visit(63, True)
        self.assertEqual(clock.battery.energy, 24.0)

    def test_g03b_negatives_no_hook_and_r1_full_slot_credit_the_gap(self):
        clock = Clock()
        clock.visit(1, True)
        clock.visit(63, True)
        self.assertEqual(clock.battery.energy, 62.0)
        clock = Clock()
        clock.visit(1, True)
        clock.visit(2, True)
        clock.charger.battery = None
        clock.charger.battery = clock.battery
        clock.visit(40, True)
        clock.visit(63, True)
        self.assertEqual(clock.battery.energy, 62.0)

    def test_g03c_reentry_at_same_timestamp_does_not_double_credit(self):
        clock = Clock()
        clock.visit(1, True)
        nested = {"count": 0}

        def reenter():
            nested["count"] = nested["count"] + 1
            if nested["count"] == 1:
                clock.update("charger", True)
                clock.update("charger", False)

        clock.battery.on_add = reenter
        clock.visit(2, True)
        self.assertEqual(clock.battery.energy, 1.0)
        self.assertEqual(len(clock.battery.added), 1)
        self.assertFalse(clock.maps["m_ChargerCharging"]["charger"])

    def test_g03d_replacement_does_not_inherit_and_drops_pending_of_removed(self):
        clock = Clock()
        clock.visit(0, True)
        replacement = Battery()
        clock.charger.battery = replacement
        clock.visit(64, True)
        self.assertEqual(replacement.energy, 0.0)
        self.assertEqual(clock.battery.energy, 0.0)
        clock.visit(65, True)
        self.assertEqual(replacement.energy, 1.0)


if __name__ == "__main__":
    unittest.main()
