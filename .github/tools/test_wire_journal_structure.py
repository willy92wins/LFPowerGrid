"""Structural pins for the wire journal (assignments, hooks, conventions)."""
from pathlib import Path
import re
import unittest
from enforce_scalar_slice import method

ROOT = Path(__file__).resolve().parents[2]
WJ = ROOT / "scripts/4_World/LFPG_WireJournal.c"
WO = ROOT / "scripts/4_World/LFPG_WireOwnerBase.c"
GEN = ROOT / "scripts/4_World/LFPG_TestDevices.c"
NM = ROOT / "scripts/5_Mission/LFPG_NetworkManagerImpl.c"
ST = ROOT / "scripts/3_Game/LFPG_Settings.c"
DF = ROOT / "scripts/3_Game/LFPG_Defines.c"
DT = ROOT / "scripts/3_Game/LFPG_Data.c"
FU = ROOT / "scripts/3_Game/LFPG_FileUtil.c"
HOOKS = [
    "BroadcastOwnerWires",
    "BroadcastOwnerWireDelta",
    "QueueBroadcastOwnerSnapshot",
    "QueueBroadcastOwnerSnapshotFromWires",
]
BEFORE = ("CoalescePendingOwnerSnapshot(", "StorePendingOwnerSnapshot(",
          "CaptureOwnerBroadcastSnapshot(", ".Send(")


def src(path):
    return path.read_text(encoding="utf-8")


class WireJournalStructure(unittest.TestCase):
    def test_settings_data_fileutil(self):
        settings = src(ST)
        self.assertIn("bool WireJournal = true;", settings)
        self.assertIn("Restoring a storage backup: delete", settings)
        self.assertIn("wire_journal.json first, or set this to false for that boot", settings)
        loaded = method(settings, "Load") if "Settings loaded:" in settings else settings
        self.assertIn("WireJournal=", settings)
        self.assertIn("NotifyWireReplaced=", settings)
        data = src(DT)
        self.assertIn("string m_OwnerDeviceId;", data)
        self.assertIn("string m_WiresJSON;", data)
        self.assertIn("string m_SavedJSON;", data)
        self.assertIn("string m_SavedPrevJSON;", data)
        self.assertIn("string m_LoadedJSON;", data)
        self.assertIn("int ver;", data)
        self.assertIn("ref array<ref LFPG_WireJournalEntry> entries;", data)
        fu = src(FU)
        self.assertIn("static bool AtomicSaveWireJournal", fu)
        self.assertNotIn("EnsureWireJournalFileOrRestore", fu)
        self.assertNotIn("LFPG_WireJournalVersionProbe", fu + data + src(DF) + src(WJ))
        self.assertNotIn("PASS_GAP", src(DF) + src(WJ))

    def test_d5_hooks(self):
        nm = src(NM)
        for name in HOOKS:
            body = method(nm, name)
            mark = body.find("LFPG_WireJournal.Get().MarkOwnerDirty(owner)")
            self.assertGreaterEqual(mark, 0, name)
            later = [body.find(tok, mark) for tok in BEFORE if tok in body]
            self.assertTrue(later, name)
            self.assertTrue(all(pos > mark for pos in later if pos >= 0), name)
        self.assertEqual(len(re.findall(r"ReplayAtBoot\(", nm)), 1)
        val = method(nm, "ValidateAllWiresAndPropagate")
        self.assertIn("!m_StartupValidationDone", val)
        self.assertIn("ReplayAtBoot()", val)
        self.assertLess(val.find("m_ValidationActive"), val.find("ReplayAtBoot"))
        fin = method(nm, "LFPG_ValidationFinalize")
        pend = fin.find("if (wasStartupPending)")
        done = fin.find("OnStartupValidationDone(")
        self.assertGreater(done, pend)
        tick = method(nm, "LFPG_ServerSchedulerTick")
        self.assertLess(tick.find("FlushVanillaIfDirty();"), tick.find("LFPG_WireJournal.Get().Tick("))

    def test_owners(self):
        wo = src(WO)
        extra = method(wo, "LFPG_OnStoreSaveExtra")
        self.assertIn("if (json != m_LastStoredWireJSON)", extra)
        self.assertIn("OnOwnerSaved(m_DeviceId, json)", extra)
        self.assertIn("!m_WireStoreFrozen", extra)
        writes = re.findall(r"ctx\.Write\(([^)]+)\)", extra)
        self.assertEqual(writes, ["json"])
        loadx = method(wo, "LFPG_OnStoreLoadExtra")
        self.assertIn("m_LoadedWireJSON = json;", loadx)
        self.assertIn("m_LastStoredWireJSON = json;", loadx)
        self.assertIn("m_PrevStoredWireJSON = \"\";", loadx)
        self.assertIn("m_LoadedWireJSON = json;", loadx)
        self.assertIn("m_LoadedWireJSON = \"\";", loadx)
        gen = src(GEN)
        gsave = method(gen, "OnStoreSave")
        self.assertIn("if (json != m_LastStoredWireJSON)", gsave)
        self.assertIn("OnOwnerSaved(m_DeviceId, json)", gsave)
        gwrites = re.findall(r"ctx\.Write\(([^)]+)\)", gsave)
        self.assertEqual(gwrites, ["m_DeviceIdLow", "m_DeviceIdHigh", "m_SourceOn", "json"])
        gload = method(gen, "OnStoreLoad")
        self.assertIn("m_LoadedWireJSON = json;", gload)

    def test_manager_assignments(self):
        wj = src(WJ)
        tick = method(wj, "Tick")
        self.assertIn("live = LFPG_DeviceAPI.GetWiresJSON(obj);", tick)
        self.assertIn("rec.m_WiresJSON = live;", tick)
        self.assertIn("rec.m_SavedJSON = saved;", tick)
        self.assertIn("saved = OwnerLastJSON(obj);", tick)
        self.assertIn("rec.m_SavedPrevJSON = prev;", tick)
        self.assertIn("prev = OwnerPrevJSON(obj);", tick)
        self.assertIn("rec.m_LoadedJSON = loaded;", tick)
        self.assertIn("loaded = OwnerLoadedJSON(obj);", tick)
        self.assertIn("keepIds.Insert(id);", tick)
        self.assertIn("m_Dirty.Set(keepIds[di], true);", tick)
        self.assertIn("LFPG_InvalidateWireJSONCache()", tick)
        self.assertLess(tick.find("if (!IsActive())"), tick.find("AtomicSaveWireJournal("))
        self.assertIn("IsValidationActive()", tick)
        mark = method(wj, "MarkOwnerDirty")
        self.assertNotIn("IsValidationActive", mark)
        boot = method(wj, "ReplayAtBoot")
        self.assertLess(boot.find("if (m_Loaded)"), boot.find("TryReadRawJsonVersion("))
        self.assertIn("LFPG_WireJournalFileVerdict(", boot)
        self.assertLess(boot.find("TryReadRawJsonVersion("), boot.find("LoadFile("))
        self.assertLess(boot.find("TryReadRawJsonVersion("), boot.find("DeleteFile("))
        self.assertIn(".tmp", boot)
        self.assertIn(".bak.new", boot)
        tmp_i = boot.find('LFPG_WIRE_JOURNAL_FILE + ".tmp"')
        main_i = boot.find("candidates.Insert(LFPG_WIRE_JOURNAL_FILE);")
        bakn_i = boot.find('LFPG_WIRE_JOURNAL_FILE + ".bak.new"')
        bak_i = boot.find('LFPG_WIRE_JOURNAL_FILE + ".bak"')
        self.assertTrue(tmp_i < main_i < bakn_i < bak_i)
        probe_for = re.search(r"for \(ci = ([^;]+); ci < ([^;]+); ci = ci \+ 1\)", boot)
        self.assertIsNotNone(probe_for)
        self.assertEqual(probe_for.group(1).strip(), "0")
        self.assertEqual(probe_for.group(2).strip(), "candidates.Count()")
        path_m = re.search(r"string path = ([^;]+);", boot)
        self.assertIsNotNone(path_m)
        self.assertEqual(path_m.group(1).strip(), "candidates[ci]")
        self.assertNotIn("CopyFile(", boot)
        self.assertIn("OwnerLoadedJSON(obj)", boot)
        self.assertIn("new array<ref LFPG_WireData>", boot)
        self.assertNotIn("GetDeviceWires(", method(wj, "ReplayAtBoot"))
        deser = re.findall(r"DeserializeJSON\(([^,]+),", boot)
        self.assertTrue(all("restoredWires" in d for d in deser))
        for pat in (r"\+=", r"-=", r"\+\+", r"--", r"\bforeach\b", r"\bPrint\("):
            self.assertIsNone(re.search(pat, wj))
        self.assertIsNone(re.search(r"\bref\s+\w+\s+\w+\s*=", method(wj, "Tick")))
        self.assertIsNone(re.search(r"\bref\s+\w+\s+\w+\s*=", method(wj, "ReplayAtBoot")))

    def test_mutants(self):
        wj = src(WJ)
        m1, n = re.subn(r"rec\.m_SavedPrevJSON = prev;", "rec.m_SavedPrevJSON = saved;", wj, count=1)
        self.assertEqual(n, 1)
        self.assertNotIn("rec.m_SavedPrevJSON = prev;", method(m1, "Tick"))
        m2, n = re.subn(r"string L = OwnerLoadedJSON\(obj\);", "string L = OwnerLastJSON(obj);", wj, count=1)
        self.assertEqual(n, 1)
        self.assertNotIn("OwnerLoadedJSON(obj)", method(m2, "ReplayAtBoot"))
        m3, n = re.subn(r"keepIds\.Insert\(id\);", "m_Dirty.Remove(id);", wj, count=1)
        self.assertEqual(n, 1)
        self.assertNotIn("keepIds.Insert(id);", method(m3, "Tick"))
        m3b, n = re.subn(
            r"m_Dirty\.Set\(keepIds\[di\], true\);",
            "m_Dirty.Remove(keepIds[di]);",
            wj, count=1)
        self.assertEqual(n, 1)
        self.assertNotIn("m_Dirty.Set(keepIds[di], true);", method(m3b, "Tick"))
        m_probe, n = re.subn(
            r"for \(ci = 0; ci < candidates\.Count\(\); ci = ci \+ 1\)",
            "for (ci = 1; ci < 2; ci = ci + 1)",
            wj, count=1)
        self.assertEqual(n, 1)
        boot_p = method(m_probe, "ReplayAtBoot")
        pf = re.search(r"for \(ci = ([^;]+); ci < ([^;]+); ci = ci \+ 1\)", boot_p)
        self.assertNotEqual((pf.group(1).strip(), pf.group(2).strip()), ("0", "candidates.Count()"))
        m_path, n = re.subn(
            r"string path = candidates\[ci\];",
            "string path = candidates[1];",
            wj, count=1)
        self.assertEqual(n, 1)
        boot_path = method(m_path, "ReplayAtBoot")
        self.assertNotIn("string path = candidates[ci];", boot_path)

        def boot_probe(path_expr, future_at, enabled):
            n_cand = 4
            vers = [1] * n_cand
            vers[future_at] = 2
            loads = 0
            deletes = 0
            writes = 0
            for ci in range(n_cand):
                idx = ci if path_expr == "candidates[ci]" else 1
                ver = vers[idx]
                if ver > 1:
                    return True, loads, deletes, writes
            if not enabled:
                deletes = n_cand
            loads = 1
            return False, loads, deletes, writes

        for future_i in range(4):
            for enabled in (True, False):
                ro, loads, deletes, writes = boot_probe("candidates[ci]", future_i, enabled)
                self.assertTrue(ro)
                self.assertEqual((loads, deletes, writes), (0, 0, 0))
        mut_ro, mut_loads, mut_del, mut_wr = boot_probe("candidates[1]", 0, False)
        self.assertFalse(mut_ro)
        self.assertGreater(mut_del, 0)
        m4, n = re.subn(
            r"candidates\.Insert\(LFPG_WIRE_JOURNAL_FILE \+ \"\.tmp\"\);",
            "",
            wj, count=1)
        self.assertEqual(n, 1)
        boot4 = method(m4, "ReplayAtBoot")
        self.assertGreater(boot4.find("TryReadRawJsonVersion("), boot4.find("candidates.Insert(LFPG_WIRE_JOURNAL_FILE);"))
        m5, n = re.subn(
            r"candidates\.Insert\(LFPG_WIRE_JOURNAL_FILE \+ \"\.tmp\"\);\s*"
            r"candidates\.Insert\(LFPG_WIRE_JOURNAL_FILE\);",
            "candidates.Insert(LFPG_WIRE_JOURNAL_FILE);\n\t\tcandidates.Insert(LFPG_WIRE_JOURNAL_FILE + \".tmp\");",
            wj, count=1)
        self.assertEqual(n, 1)
        boot5 = method(m5, "ReplayAtBoot")
        self.assertLess(boot5.find("candidates.Insert(LFPG_WIRE_JOURNAL_FILE);"),
                        boot5.find('LFPG_WIRE_JOURNAL_FILE + ".tmp"'))
        m6, n = re.subn(
            r"void Tick\(int nowMs\)\s*\{\s*#ifdef SERVER\s*if \(!IsActive\(\)\)\s*(?:\{\s*)?return;\s*\}?",
            "void Tick(int nowMs)\n\t{\n\t\t#ifdef SERVER\n\t\t",
            wj, count=1)
        self.assertEqual(n, 1)
        self.assertNotIn("if (!IsActive())", method(m6, "Tick")[:40])
        m7, n = re.subn(
            r"array<ref LFPG_WireData> restoredWires = new array<ref LFPG_WireData>;",
            "array<ref LFPG_WireData> restoredWires = LFPG_DeviceAPI.GetDeviceWires(obj);",
            wj, count=1)
        self.assertEqual(n, 1)
        self.assertIn("GetDeviceWires(", method(m7, "ReplayAtBoot"))
        m8, n = re.subn(
            r"if \(id == \"\"\)\s*(?:\{\s*)?return;\s*\}?",
            "if (id == \"\")\n\t\t\treturn;\n\t\tif (LFPG_NetworkManager.Get().IsValidationActive())\n\t\t\treturn;",
            wj, count=1)
        self.assertEqual(n, 1)
        self.assertIn("IsValidationActive", method(m8, "MarkOwnerDirty"))


if __name__ == "__main__":
    unittest.main()
