"""Structural pins for the wire journal (assignments, hooks, conventions)."""
from pathlib import Path
from types import SimpleNamespace
import re
import unittest
from enforce_scalar_slice import load, method

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


class Arr(list):
    def Insert(self, item):
        self.append(item)

    def Count(self):
        return len(self)


class JMap(dict):
    def Contains(self, k):
        return k in self

    def Get(self, k):
        return dict.get(self, k)

    def Set(self, k, v):
        self[k] = v

    def Remove(self, k):
        self.pop(k, None)

    def Count(self):
        return len(self)

    def GetKey(self, i):
        return list(self.keys())[i]


class VerTok:
    def __init__(self):
        self.n = 0

    def __int__(self):
        return self.n

    def __index__(self):
        return self.n

    def __gt__(self, other):
        return self.n > other

    def __eq__(self, other):
        return self.n == other


def replay_python(body):
    held = []

    def hold(m):
        held.append(m.group(0))
        return "STRLIT" + str(len(held) - 1)

    code = re.sub(r'"(?:\\.|[^"\\])*"', hold, body)
    code = re.sub(
        r"for \((\w+) = ([^;]+); \1 < ([^;]+); \1 = \1 \+ 1\)",
        r"for \1 in range(\2, \3)",
        code)
    code = re.sub(r"\bnew array<[^>]+>", "Arr()", code)
    code = re.sub(r"\bnew LFPG_WireJournalStore\(\)", "Store()", code)
    code = re.sub(
        r"JsonFileLoader<[^>]+>\.LoadFile\(",
        "JsonLoad(",
        code)
    code = re.sub(
        r"LFPG_FileUtil\.TryReadRawJsonVersion\((\w+), (\w+)\)",
        r"TryRead(\1, \2)",
        code)
    code = re.sub(r"\bint ver = 0;", "ver = VerTok();", code)
    code = re.sub(
        r"LFPG_WireJournalFileVerdict\((\w+), (\w+), (\w+),",
        r"LFPG_WireJournalFileVerdict(\1, \2, int(\3),",
        code)
    code = re.sub(r"(?<![.\w])(m_[A-Za-z0-9_]+)\b", r"cell.\1", code)
    tokens = re.findall(r"STRLIT\d+|[^{};]+|[{};]", code)
    tokens = [token.strip() for token in tokens if token.strip() and token != ";"]
    cursor = 0
    lines = ["def run(cell):"]

    def expression(value):
        value = value.replace("&&", " and ").replace("||", " or ")
        value = re.sub(r"!(?!=)", "not ", value)
        value = re.sub(r"\btrue\b", "True", value)
        value = re.sub(r"\bfalse\b", "False", value)
        value = re.sub(r"\bnull\b", "None", value)
        value = re.sub(r"(\w+)\.ToString\(\)", r"str(\1)", value)
        value = re.sub(
            r"\b(?:ref\s+)?(?:array<[^>]+>|string|int|bool|EntityAI|LFPG_(?![A-Z0-9_]+\b)\w+)\s+",
            "",
            value)
        return re.sub(r"STRLIT(\d+)", lambda m: held[int(m.group(1))], value)

    def statement(level):
        nonlocal cursor
        token = tokens[cursor]
        cursor = cursor + 1
        indent = "    " * level
        if token == "{":
            while tokens[cursor] != "}":
                statement(level)
            cursor = cursor + 1
            return
        if re.match(r"^for\s+\w+\s+in\s+range\(", token):
            lines.append(indent + expression(token) + ":")
            statement(level + 1)
            return
        if re.match(r"^(if|else if)\s*\(", token):
            opening, depth = token.index("("), 1
            end = opening + 1
            while depth:
                depth += (token[end] == "(") - (token[end] == ")")
                end += 1
            keyword = "elif" if token.startswith("else if") else "if"
            lines.append(indent + keyword + " " + expression(token[opening + 1:end - 1]) + ":")
            tail = token[end:].strip()
            if tail:
                tokens.insert(cursor, tail)
            statement(level + 1)
            return
        if token == "else":
            lines.append(indent + "else:")
            statement(level + 1)
            return
        expr = expression(token)
        if expr not in ("return", "continue", "break") and re.match(r"^[A-Za-z_]\w*$", expr):
            return
        lines.append(indent + expr)

    while cursor < len(tokens):
        statement(1)
    return "\n".join(lines) + "\n"


def run_replay_at_boot(source, future_at, enabled):
    defines = src(DF)
    journal_file = re.search(r'LFPG_WIRE_JOURNAL_FILE = "([^"]+)"', defines).group(1)
    journal_ver = int(re.search(r"LFPG_WIRE_JOURNAL_VER = (\d+)", defines).group(1))
    wj_names = {name: int(val) for name, val in re.findall(r"\b(LFPG_WJ_[A-Z0-9_]+)\s*=\s*(-?\d+)", defines)}
    candidates = [
        journal_file + ".tmp",
        journal_file,
        journal_file + ".bak.new",
        journal_file + ".bak",
    ]
    versions = {}
    ci = 0
    while ci < 4:
        versions[candidates[ci]] = journal_ver
        ci = ci + 1
    versions[candidates[future_at]] = journal_ver + 1
    io = SimpleNamespace(loads=0, deletes=0, writes=0)

    def file_exist(path):
        return path in versions

    def try_read(path, ver_tok):
        if path not in versions:
            return False
        ver_tok.n = versions[path]
        return True

    def delete_file(path):
        io.deletes = io.deletes + 1
        versions.pop(path, None)
        return True

    def json_load(path, store, err=None):
        io.loads = io.loads + 1
        return False

    def write_touch(*_a, **_k):
        io.writes = io.writes + 1
        return False

    cell = SimpleNamespace(
        m_Loaded=False, m_Enabled=True, m_ReadOnly=False, m_FileDirty=False,
        m_Entries=JMap(), m_Dirty=JMap(), m_SeenEmit=JMap(), m_CapturedMs=JMap(),
        m_Restored=Arr(), m_CapWarned=False, m_LastSaveWarnMs=0, m_LastKeepWarnMs=0)

    def drop_entry(oid):
        cell.m_Entries.Remove(oid)

    env = {
        "Arr": Arr, "Store": lambda: SimpleNamespace(ver=0, entries=Arr()),
        "VerTok": VerTok, "cell": cell,
        "FileExist": file_exist, "TryRead": try_read, "DeleteFile": delete_file,
        "JsonLoad": json_load, "CopyFile": write_touch,
        "LFPG_WIRE_JOURNAL_FILE": journal_file,
        "LFPG_WIRE_JOURNAL_VER": journal_ver,
        "LFPG_WireJournalFileVerdict": load(
            source, "LFPG_WireJournalFileVerdict",
            ["exists", "versionRead", "version", "currentVer"], wj_names),
        "LFPG_WJ_FILE_FUTURE": wj_names["LFPG_WJ_FILE_FUTURE"],
        "LFPG_WJ_REPLAY_RESTORE": wj_names["LFPG_WJ_REPLAY_RESTORE"],
        "LFPG_WJ_REPLAY_STALE": wj_names["LFPG_WJ_REPLAY_STALE"],
        "LFPG_WJ_REPLAY_MISSING": wj_names["LFPG_WJ_REPLAY_MISSING"],
        "LFPG_WJ_REPLAY_UNUSABLE": wj_names["LFPG_WJ_REPLAY_UNUSABLE"],
        "LFPG_Settings": SimpleNamespace(Get=lambda: SimpleNamespace(WireJournal=enabled)),
        "LFPG_Util": SimpleNamespace(Warn=lambda *_a: None, Info=lambda *_a: None, Debug=lambda *_a: None),
        "g_Game": SimpleNamespace(GetTime=lambda: 0),
        "LFPG_DeviceRegistry": SimpleNamespace(Get=lambda: SimpleNamespace(FindById=lambda _i: None)),
        "OwnerLoadedJSON": lambda _o: "",
        "OwnerUsable": lambda _o: False,
        "LFPG_WireJournalReplayVerdict": lambda *_a: wj_names["LFPG_WJ_REPLAY_UNUSABLE"],
        "LFPG_WireHelper": SimpleNamespace(DeserializeJSON=lambda *_a: False),
        "LFPG_DeviceAPI": SimpleNamespace(
            ClearDeviceWires=write_touch, AddDeviceWire=write_touch),
        "DropEntry": drop_entry,
        "str": str, "range": range, "int": int, "len": len,
    }
    # Hasta la carga: pasar por ella ya cuenta como carga y el resto no hace falta traducirlo.
    body = method(source, "ReplayAtBoot")
    body = body[:body.index("LFPG_WireJournalStore loadedStore")]
    body = body + 'JsonFileLoader<LFPG_WireJournalStore>.LoadFile("", null, null);'
    compiled = replay_python(body)
    exec(compile(compiled, "<slice:ReplayAtBoot>", "exec"), env)
    env["run"](cell)
    return io.loads, io.deletes, io.writes, cell.m_ReadOnly


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

        for future_i in range(4):
            for enabled in (True, False):
                loads, deletes, writes, ro = run_replay_at_boot(wj, future_i, enabled)
                self.assertTrue(ro)
                self.assertEqual((loads, deletes, writes), (0, 0, 0))
        mut_src, n = re.subn(
            r"(futMsg = futMsg \+ \"; journal off this boot, files left untouched\";\s*"
            r"LFPG_Util\.Warn\(futMsg\);\s*)return;",
            r"\1",
            wj, count=1)
        self.assertEqual(n, 1)
        mut_hit = False
        for future_i in range(4):
            for enabled in (True, False):
                loads, deletes, writes, ro = run_replay_at_boot(mut_src, future_i, enabled)
                if loads + deletes + writes > 0:
                    mut_hit = True
        self.assertTrue(mut_hit)
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
