"""Pure WireJournal predicates from production slices."""
from pathlib import Path
import re
import unittest
from enforce_scalar_slice import load

ROOT = Path(__file__).resolve().parents[2]
SRC = (ROOT / "scripts/4_World/LFPG_WireJournal.c").read_text(encoding="utf-8")
DEFINES = (ROOT / "scripts/3_Game/LFPG_Defines.c").read_text(encoding="utf-8")
WJ = {name: int(val) for name, val in re.findall(r"\b(LFPG_WJ_[A-Z0-9_]+)\s*=\s*(-?\d+)", DEFINES)}
N = 128
T = 3600000
REMOVE, KEEP, STORE = WJ["LFPG_WJ_CAPTURE_REMOVE"], WJ["LFPG_WJ_CAPTURE_KEEP"], WJ["LFPG_WJ_CAPTURE_STORE"]
IGNORE, FIRST, DURABLE = WJ["LFPG_WJ_SAVE_IGNORE"], WJ["LFPG_WJ_SAVE_FIRST_EMIT"], WJ["LFPG_WJ_SAVE_DURABLE"]
MISSING, UNUSABLE, UNCHANGED, RESTORE, STALE = (
    WJ["LFPG_WJ_REPLAY_MISSING"], WJ["LFPG_WJ_REPLAY_UNUSABLE"], WJ["LFPG_WJ_REPLAY_UNCHANGED"],
    WJ["LFPG_WJ_REPLAY_RESTORE"], WJ["LFPG_WJ_REPLAY_STALE"])
ABSENT, FUTURE, CANDIDATE = WJ["LFPG_WJ_FILE_ABSENT"], WJ["LFPG_WJ_FILE_FUTURE"], WJ["LFPG_WJ_FILE_CANDIDATE"]


def fn(name, args, source=None):
    return load(source or SRC, name, args, dict(WJ))


class WireJournalRules(unittest.TestCase):
    def test_mark_allowed(self):
        f = fn("LFPG_WireJournalMarkAllowed", ["active", "hasEntry", "entryCount", "maxEntries"])
        self.assertTrue(f(True, False, 0, N))
        self.assertTrue(f(True, True, N, N))
        self.assertTrue(f(True, False, N - 1, N))
        self.assertFalse(f(False, True, 0, N))
        self.assertFalse(f(True, False, N, N))
        mutant, n = re.subn(
            r"if \(hasEntry\)\s*(?:\{\s*)?return true;\s*\}?",
            "if (hasEntry)\n\t\treturn false;",
            SRC, count=1)
        self.assertEqual(n, 1)
        mf = fn("LFPG_WireJournalMarkAllowed", ["active", "hasEntry", "entryCount", "maxEntries"], mutant)
        self.assertFalse(mf(True, True, N, N))

    def test_capture_kind(self):
        f = fn("LFPG_WireJournalCaptureKind",
               ["usable", "liveEmpty", "savedEmpty", "liveEqualsSaved", "prevEmpty", "loadedEmpty"])
        self.assertEqual(f(False, False, False, False, False, False), REMOVE)
        self.assertEqual(f(True, True, False, False, False, False), KEEP)
        self.assertEqual(f(True, False, True, False, True, True), REMOVE)
        self.assertEqual(f(True, False, False, True, True, False), REMOVE)
        self.assertEqual(f(True, False, False, True, False, False), STORE)
        self.assertEqual(f(True, False, False, False, True, False), STORE)
        self.assertEqual(f(True, False, True, False, False, False), STORE)
        self.assertEqual(f(True, False, True, False, True, False), STORE)
        no_eq, n = re.subn(
            r"if \(liveEqualsSaved && prevEmpty\)\s*(?:\{\s*)?return LFPG_WJ_CAPTURE_REMOVE;\s*\}?",
            "", SRC, count=1)
        self.assertEqual(n, 1)
        nf = fn("LFPG_WireJournalCaptureKind",
                ["usable", "liveEmpty", "savedEmpty", "liveEqualsSaved", "prevEmpty", "loadedEmpty"], no_eq)
        self.assertEqual(nf(True, False, False, True, True, False), STORE)
        old, n = re.subn(
            r"if \(savedEmpty && prevEmpty && loadedEmpty\)",
            "if (savedEmpty)",
            SRC, count=1)
        self.assertEqual(n, 1)
        of = fn("LFPG_WireJournalCaptureKind",
                ["usable", "liveEmpty", "savedEmpty", "liveEqualsSaved", "prevEmpty", "loadedEmpty"], old)
        self.assertEqual(of(True, False, True, False, False, False), REMOVE)
        self.assertEqual(of(True, False, True, False, True, False), REMOVE)

    def test_save_verdict(self):
        f = fn("LFPG_WireJournalSaveVerdict", ["hasEntry", "isDirty", "savedEqualsNewest", "seenOnce"])
        self.assertEqual(f(False, False, True, False), IGNORE)
        self.assertEqual(f(True, True, True, True), IGNORE)
        self.assertEqual(f(True, False, False, True), IGNORE)
        self.assertEqual(f(True, False, True, False), FIRST)
        self.assertEqual(f(True, False, True, True), DURABLE)
        dur_first, n = re.subn(
            r"if \(!seenOnce\)\s*(?:\{\s*)?return LFPG_WJ_SAVE_FIRST_EMIT;\s*\}?",
            "if (!seenOnce)\n\t\treturn LFPG_WJ_SAVE_DURABLE;",
            SRC, count=1)
        self.assertEqual(n, 1)
        df = fn("LFPG_WireJournalSaveVerdict", ["hasEntry", "isDirty", "savedEqualsNewest", "seenOnce"], dur_first)
        self.assertEqual(df(True, False, True, False), DURABLE)
        no_dirty, n = re.subn(
            r"if \(isDirty\)\s*(?:\{\s*)?return LFPG_WJ_SAVE_IGNORE;\s*\}?",
            "", SRC, count=1)
        self.assertEqual(n, 1)
        nd = fn("LFPG_WireJournalSaveVerdict", ["hasEntry", "isDirty", "savedEqualsNewest", "seenOnce"], no_dirty)
        self.assertEqual(nd(True, True, True, True), DURABLE)

    def test_ttl(self):
        f = fn("LFPG_WireJournalTtlExpired", ["capturedMs", "nowMs", "ttlMs"])
        self.assertFalse(f(-1, 10 ** 9, T))
        self.assertFalse(f(0, T - 1, T))
        self.assertTrue(f(0, T, T))

    def test_replay_verdict(self):
        f = fn("LFPG_WireJournalReplayVerdict",
               ["ownerFound", "usable", "storedEqualsNewest", "storedEqualsSaved", "storedEqualsPrev", "storedEqualsLoaded"])
        self.assertEqual(f(False, True, False, False, False, False), MISSING)
        self.assertEqual(f(True, False, False, False, False, False), UNUSABLE)
        self.assertEqual(f(True, True, True, True, True, True), UNCHANGED)
        self.assertEqual(f(True, True, True, False, False, False), UNCHANGED)
        self.assertEqual(f(True, True, False, True, False, False), RESTORE)
        self.assertEqual(f(True, True, False, False, True, False), RESTORE)
        self.assertEqual(f(True, True, False, False, False, True), RESTORE)
        self.assertEqual(f(True, True, False, False, False, False), STALE)
        no_prev, n = re.subn(
            r"if \(storedEqualsPrev\)\s*(?:\{\s*)?return LFPG_WJ_REPLAY_RESTORE;\s*\}?",
            "", SRC, count=1)
        self.assertEqual(n, 1)
        np = fn("LFPG_WireJournalReplayVerdict",
                ["ownerFound", "usable", "storedEqualsNewest", "storedEqualsSaved", "storedEqualsPrev", "storedEqualsLoaded"], no_prev)
        self.assertEqual(np(True, True, False, False, True, False), STALE)
        no_load, n = re.subn(
            r"if \(storedEqualsLoaded\)\s*(?:\{\s*)?return LFPG_WJ_REPLAY_RESTORE;\s*\}?",
            "", SRC, count=1)
        self.assertEqual(n, 1)
        nl = fn("LFPG_WireJournalReplayVerdict",
                ["ownerFound", "usable", "storedEqualsNewest", "storedEqualsSaved", "storedEqualsPrev", "storedEqualsLoaded"], no_load)
        self.assertEqual(nl(True, True, False, False, False, True), STALE)
        saved_first, n = re.subn(
            r"if \(storedEqualsNewest\)\s*(?:\{\s*)?return LFPG_WJ_REPLAY_UNCHANGED;\s*\}?",
            "if (storedEqualsSaved)\n\t\treturn LFPG_WJ_REPLAY_RESTORE;\n\tif (storedEqualsNewest)\n\t\treturn LFPG_WJ_REPLAY_UNCHANGED;",
            SRC, count=1)
        self.assertEqual(n, 1)
        sf = fn("LFPG_WireJournalReplayVerdict",
                ["ownerFound", "usable", "storedEqualsNewest", "storedEqualsSaved", "storedEqualsPrev", "storedEqualsLoaded"], saved_first)
        self.assertEqual(sf(True, True, True, True, True, True), RESTORE)

    def test_file_verdict(self):
        f = fn("LFPG_WireJournalFileVerdict", ["exists", "versionRead", "version", "currentVer"])
        self.assertEqual(f(False, False, 0, 1), ABSENT)
        self.assertEqual(f(True, True, 2, 1), FUTURE)
        self.assertEqual(f(True, True, 1, 1), CANDIDATE)
        self.assertEqual(f(True, True, 0, 1), CANDIDATE)
        self.assertEqual(f(True, False, 0, 1), CANDIDATE)
        ge, n = re.subn(r"version > currentVer", "version >= currentVer", SRC, count=1)
        self.assertEqual(n, 1)
        gf = fn("LFPG_WireJournalFileVerdict", ["exists", "versionRead", "version", "currentVer"], ge)
        self.assertEqual(gf(True, True, 1, 1), FUTURE)


if __name__ == "__main__":
    unittest.main()
