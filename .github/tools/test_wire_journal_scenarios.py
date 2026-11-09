"""Wire journal owner+manager model composed with production predicates."""
from pathlib import Path
from types import SimpleNamespace
import random
import re
import unittest
from enforce_scalar_slice import load, method, scalar_function

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


def fns(source=None):
    s = source or SRC
    stubs = dict(WJ)
    return {
        "mark": load(s, "LFPG_WireJournalMarkAllowed", ["active", "hasEntry", "entryCount", "maxEntries"], stubs),
        "cap": load(s, "LFPG_WireJournalCaptureKind",
                    ["usable", "liveEmpty", "savedEmpty", "liveEqualsSaved", "prevEmpty", "loadedEmpty"], stubs),
        "save": load(s, "LFPG_WireJournalSaveVerdict",
                     ["hasEntry", "isDirty", "savedEqualsNewest", "seenOnce"], stubs),
        "ttl": load(s, "LFPG_WireJournalTtlExpired", ["capturedMs", "nowMs", "ttlMs"], stubs),
        "rep": load(s, "LFPG_WireJournalReplayVerdict",
                    ["ownerFound", "usable", "storedEqualsNewest", "storedEqualsSaved",
                     "storedEqualsPrev", "storedEqualsLoaded"], stubs),
    }


class Map(dict):
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

    def Clear(self):
        self.clear()


def neutralize(code):
    code = re.sub(r"LFPG_Util\.\w+\s*\([^;]*\);", "", code)
    code = re.sub(r'"(?:\\.|[^"\\])*"', "EMPTY", code)
    code = re.sub(r"(\w+)\.ToString\(\)", r"str(\1)", code)
    code = re.sub(
        r"\b(?:string|vector|ref|LFPG_WireJournalEntry)\s+(?=[A-Za-z_]\w*\s*(?:=|;))",
        "",
        code)
    return code


def qualify_members(code):
    return re.sub(r"(?<![.\w])(m_[A-Za-z0-9_]+)\b", r"cell.\1", code)


def bind_saved(source):
    body = qualify_members(neutralize(method(source, "OnOwnerSaved")))
    generated = scalar_function(body, [])
    inner = generated.split("\n", 1)[1]
    return "def run(cell, ownerId, savedJSON):\n" + inner + "\n"


class Owner:
    def __init__(self, oid="d1"):
        self.id = oid
        self.loaded = ""
        self.last = ""
        self.prev = ""
        self.live = ""
        self.ser_ok = True
        self.emits = []
        self.present = True
        self.usable = True

    def boot(self, ce):
        self.loaded = ce
        self.last = ce
        self.prev = ""
        self.live = ce
        self.present = True
        self.usable = True

    def change(self, w):
        self.live = w

    def emit(self):
        j = self.live if self.ser_ok else ""
        if j != self.last:
            self.prev = self.last
            self.last = j
        if j != "":
            self.emits.append(j)
        return j


class Journal:
    def __init__(self, rules):
        self.r = rules
        self.entries = {}
        self.dirty = set()
        self.seen = set()
        self.captured = {}
        self.restored = []
        self.loaded = True
        self.enabled = True
        self.readonly = False
        self.file = None
        self.file_dirty = False
        self.validation = False
        self.now = 0
        self.keeps = 0

    def active(self):
        return self.loaded and self.enabled and not self.readonly

    def mark(self, oid):
        if self.r["mark"](self.active(), oid in self.entries, len(self.entries), N):
            self.dirty.add(oid)

    def on_saved(self, oid, saved):
        if not self.active():
            return
        has = oid in self.entries
        eq = has and saved == self.entries[oid][0]
        v = self.r["save"](has, oid in self.dirty, eq, oid in self.seen)
        if v == FIRST:
            self.seen.add(oid)
        elif v == DURABLE:
            self.entries.pop(oid, None)
            self.dirty.discard(oid)
            self.seen.discard(oid)
            self.captured.pop(oid, None)
            self.file_dirty = True

    def tick(self, owner):
        if not self.active():
            return
        if not self.validation:
            keep = set()
            for oid in list(self.dirty):
                live = ""
                saved = ""
                prev = ""
                loaded = ""
                if owner.present:
                    if owner.ser_ok:
                        live = owner.live
                    saved = owner.last
                    prev = owner.prev
                    loaded = owner.loaded
                usable = owner.present and owner.usable
                kind = self.r["cap"](usable, live == "", saved == "", live == saved, prev == "", loaded == "")
                if kind == REMOVE:
                    if oid in self.entries:
                        self.entries.pop(oid, None)
                        self.file_dirty = True
                    self.dirty.discard(oid)
                    self.seen.discard(oid)
                    self.captured.pop(oid, None)
                elif kind == KEEP:
                    keep.add(oid)
                    self.keeps = self.keeps + 1
                else:
                    old = self.entries.get(oid)
                    rec = (live, saved, prev, loaded)
                    if old != rec:
                        self.file_dirty = True
                    if old is None or old[0] != live:
                        self.seen.discard(oid)
                    self.entries[oid] = rec
                    self.captured[oid] = self.now
            self.dirty = keep
        for oid in list(self.captured):
            if self.r["ttl"](self.captured[oid], self.now, T):
                self.entries.pop(oid, None)
                self.dirty.discard(oid)
                self.seen.discard(oid)
                self.captured.pop(oid, None)
                self.file_dirty = True
        if self.file_dirty:
            self.file = dict(self.entries)
            self.file_dirty = False

    def replay(self, owner, file_ent):
        self.entries = dict(file_ent or {})
        oid = owner.id
        dropped = []
        if oid not in self.entries:
            return None
        w, s, p, l = self.entries[oid]
        L = owner.loaded
        usable = owner.present and owner.usable and L != "" and w != ""
        rv = self.r["rep"](owner.present, usable, L == w, s != "" and L == s, p != "" and L == p, l != "" and L == l)
        if rv == RESTORE:
            owner.live = w
            self.seen.discard(oid)
            self.captured[oid] = self.now
            self.restored.append(oid)
            return "restore"
        if rv == STALE:
            dropped.append(oid)
            self.entries.pop(oid, None)
            self.file_dirty = True
            return "stale"
        self.entries.pop(oid, None)
        self.file_dirty = True
        if rv == UNCHANGED:
            return "unchanged"
        if rv == MISSING:
            return "missing"
        return "unusable"

    def validation_done(self):
        for oid in self.restored:
            self.dirty.add(oid)
        self.restored = []


def die(j, owner, ce):
    snap = dict(j.file or j.entries)
    owner.boot(ce)
    owner.emits = [ce] if ce else []
    return j.replay(owner, snap)


def restart_session(j, owner, ce):
    snap = dict(j.file or {})
    now = j.now
    rules = j.r
    owner.boot(ce)
    owner.emits = [ce] if ce else []
    nxt = Journal(rules)
    nxt.now = now
    nxt.file = dict(snap)
    nxt.replay(owner, snap)
    return nxt


def run_saved_slice(source, cell, owner_id, saved):
    env = {
        "LFPG_WireJournalSaveVerdict": load(source, "LFPG_WireJournalSaveVerdict",
                                            ["hasEntry", "isDirty", "savedEqualsNewest", "seenOnce"], dict(WJ)),
        "LFPG_WJ_SAVE_FIRST_EMIT": WJ["LFPG_WJ_SAVE_FIRST_EMIT"],
        "LFPG_WJ_SAVE_DURABLE": WJ["LFPG_WJ_SAVE_DURABLE"],
        "LFPG_WJ_SAVE_IGNORE": WJ["LFPG_WJ_SAVE_IGNORE"],
        "IsActive": lambda: cell.m_Loaded and cell.m_Enabled and (not cell.m_ReadOnly),
        "EMPTY": "",
        "str": str,
        "LFPG_Util": SimpleNamespace(Debug=lambda *_a: None),
    }
    exec(compile(bind_saved(source), "<slice:OnOwnerSaved>", "exec"), env)
    env["run"](cell, owner_id, saved)


class WireJournalScenarios(unittest.TestCase):
    def _j(self, source=None):
        return Journal(fns(source))

    def test_s1(self):
        for ce in ("A", "B"):
            o = Owner()
            o.boot("A")
            j = self._j()
            o.change("B")
            j.mark(o.id)
            j.on_saved(o.id, o.emit())
            j.tick(o)
            die(j, o, ce)
            self.assertEqual(o.live, "B")

    def test_s2(self):
        for ce in ("A", "B"):
            o = Owner()
            o.boot("A")
            j = self._j()
            o.change("B")
            j.mark(o.id)
            j.tick(o)
            j.on_saved(o.id, o.emit())
            o.change("C")
            j.mark(o.id)
            j.tick(o)
            die(j, o, ce)
            self.assertEqual(o.live, "C")

    def test_s3(self):
        for ce in ("A", "Bp"):
            o = Owner()
            o.boot("A")
            j = self._j()
            j.file = {o.id: ("B", "A", "", "A")}
            die(j, o, "A")
            self.assertEqual(o.live, "B")
            j.on_saved(o.id, o.emit())
            o.change("Bp")
            j.validation_done()
            j.tick(o)
            die(j, o, ce)
            self.assertEqual(o.live, "Bp")

    def test_s4(self):
        o = Owner()
        o.boot("A")
        j = self._j()
        o.change("B")
        j.mark(o.id)
        j.tick(o)
        j.on_saved(o.id, o.emit())
        j.tick(o)
        j.on_saved(o.id, o.emit())
        j.tick(o)
        self.assertNotIn(o.id, j.entries)
        die(j, o, "B")
        self.assertEqual(o.live, "B")

    def test_s5(self):
        o = Owner()
        o.boot("A")
        j = self._j()
        o.change("B")
        j.mark(o.id)
        j.tick(o)
        die(j, o, "X")
        self.assertEqual(o.live, "X")

    def test_s6(self):
        o = Owner()
        o.boot("A")
        j = self._j()
        o.change("B")
        j.mark(o.id)
        j.tick(o)
        o.change("C")
        j.mark(o.id)
        o.ser_ok = False
        j.tick(o)
        self.assertIn(o.id, j.dirty)
        o.ser_ok = True
        j.tick(o)
        die(j, o, "A")
        self.assertEqual(o.live, "C")

    def test_s7(self):
        o = Owner()
        o.boot("")
        o.loaded = ""
        o.last = ""
        o.prev = ""
        o.live = "P"
        j = self._j()
        j.mark(o.id)
        j.tick(o)
        self.assertNotIn(o.id, j.entries)
        o.present = False
        die(j, o, "")
        self.assertEqual(j.entries, {})

    def test_s8(self):
        o = Owner()
        o.boot("A")
        j = self._j()
        o.change("B")
        j.mark(o.id)
        j.validation = True
        j.tick(o)
        self.assertNotIn(o.id, j.entries)
        j.validation = False
        j.tick(o)
        die(j, o, "A")
        self.assertEqual(o.live, "B")

    def test_s9(self):
        for ce in ("A", "B", "C"):
            o = Owner()
            o.boot("A")
            j = self._j()
            o.change("B")
            j.mark(o.id)
            j.on_saved(o.id, o.emit())
            o.change("C")
            j.mark(o.id)
            j.on_saved(o.id, o.emit())
            j.tick(o)
            die(j, o, ce)
            self.assertEqual(o.live, "C", ce)

    def test_s10(self):
        o = Owner()
        o.boot("A")
        j = self._j()
        for st in ("B", "C", "D"):
            o.change(st)
            j.on_saved(o.id, o.emit())
        o.change("E")
        j.mark(o.id)
        j.tick(o)
        die(j, o, "B")
        self.assertEqual(o.live, "B")
        for ce in ("C", "D", "A"):
            o2 = Owner()
            o2.boot("A")
            j2 = self._j()
            for st in ("B", "C", "D"):
                o2.change(st)
                j2.on_saved(o2.id, o2.emit())
            o2.change("E")
            j2.mark(o2.id)
            j2.tick(o2)
            die(j2, o2, ce)
            self.assertEqual(o2.live, "E", ce)

    def test_s11(self):
        o = Owner()
        o.loaded = ""
        o.last = ""
        o.prev = ""
        o.live = "P"
        j = self._j()
        j.on_saved(o.id, o.emit())
        o.change("B")
        j.mark(o.id)
        j.tick(o)
        self.assertEqual(j.entries[o.id][0], "B")
        self.assertEqual(j.entries[o.id][1], "P")
        o.present = False
        rv = j.replay(o, dict(j.entries))
        self.assertEqual(rv, "missing")
        o.present = True
        o.boot("P")
        j.file = {o.id: ("B", "P", "", "")}
        die(j, o, "P")
        self.assertEqual(o.live, "B")

    def test_s12(self):
        for ce in ("A", "B"):
            o = Owner()
            o.boot("A")
            j = self._j()
            o.change("B")
            j.mark(o.id)
            j.tick(o)
            j.on_saved(o.id, o.emit())
            j.tick(o)
            die(j, o, ce)
            self.assertEqual(o.live, "B")

    def test_s13(self):
        o = Owner()
        o.boot("A")
        o.last = "Astar"
        j = self._j()
        j.file = {o.id: ("B", "A", "", "A")}
        j.replay(o, j.file)
        self.assertEqual(o.live, "B")

    def test_s14(self):
        o = Owner()
        o.boot("A")
        j = self._j()
        j.file = {o.id: ("B", "A", "", "A")}
        die(j, o, "A")
        self.assertEqual(o.live, "B")
        j.validation_done()
        o.ser_ok = False
        j.on_saved(o.id, o.emit())
        o.ser_ok = True
        j.tick(o)
        self.assertEqual(j.entries[o.id], ("B", "", "A", "A"))
        die(j, o, "A")
        self.assertEqual(o.live, "B")

    def test_s15(self):
        o = Owner()
        o.boot("A")
        j = self._j()
        o.change("B")
        j.mark(o.id)
        o.ser_ok = False
        j.on_saved(o.id, o.emit())
        o.ser_ok = True
        j.tick(o)
        die(j, o, "A")
        self.assertEqual(o.live, "B")

    def test_mutants(self):
        old, n = re.subn(r"if \(savedEmpty && prevEmpty && loadedEmpty\)", "if (savedEmpty)", SRC, count=1)
        self.assertEqual(n, 1)
        o = Owner()
        o.boot("A")
        j = Journal(fns(old))
        o.change("B")
        j.mark(o.id)
        o.ser_ok = False
        j.on_saved(o.id, o.emit())
        o.ser_ok = True
        j.tick(o)
        die(j, o, "A")
        self.assertNotEqual(o.live, "B")
        no_prev, n = re.subn(
            r"if \(storedEqualsPrev\)\s*(?:\{\s*)?return LFPG_WJ_REPLAY_RESTORE;\s*\}?", "", SRC, count=1)
        self.assertEqual(n, 1)
        o = Owner()
        o.boot("A")
        j = Journal(fns(no_prev))
        o.change("B")
        j.mark(o.id)
        j.on_saved(o.id, o.emit())
        o.change("C")
        j.mark(o.id)
        j.on_saved(o.id, o.emit())
        j.tick(o)
        die(j, o, "B")
        self.assertNotEqual(o.live, "C")
        no_load, n = re.subn(
            r"if \(storedEqualsLoaded\)\s*(?:\{\s*)?return LFPG_WJ_REPLAY_RESTORE;\s*\}?", "", SRC, count=1)
        self.assertEqual(n, 1)
        o = Owner()
        o.boot("A")
        j = Journal(fns(no_load))
        o.change("B")
        j.mark(o.id)
        j.on_saved(o.id, o.emit())
        o.change("C")
        j.mark(o.id)
        j.on_saved(o.id, o.emit())
        j.tick(o)
        die(j, o, "A")
        self.assertNotEqual(o.live, "C")
        dur1, n = re.subn(
            r"if \(!seenOnce\)\s*(?:\{\s*)?return LFPG_WJ_SAVE_FIRST_EMIT;\s*\}?",
            "if (!seenOnce)\n\t\treturn LFPG_WJ_SAVE_DURABLE;",
            SRC, count=1)
        self.assertEqual(n, 1)
        o = Owner()
        o.boot("A")
        j = Journal(fns(dur1))
        o.change("B")
        j.mark(o.id)
        j.tick(o)
        j.on_saved(o.id, o.emit())
        j.tick(o)
        die(j, o, "A")
        self.assertNotEqual(o.live, "B")
        j = self._j()
        o = Owner()
        o.boot("A")
        o.change("B")
        j.mark(o.id)
        j.tick(o)
        o.change("C")
        j.mark(o.id)
        o.live = ""
        j.tick(o)
        j.dirty.discard(o.id)
        o.live = "C"
        j.tick(o)
        die(j, o, "A")
        self.assertNotEqual(o.live, "C")
        o = Owner()
        o.boot("A")
        j = self._j()
        o.change("B")
        j.mark(o.id)
        j.on_saved(o.id, o.emit())
        o.change("C")
        j.mark(o.id)
        j.on_saved(o.id, o.emit())
        j.tick(o)
        w, s, p, l = j.entries[o.id]
        j.entries[o.id] = (w, s, s, l)
        j.file = dict(j.entries)
        die(j, o, "B")
        self.assertNotEqual(o.live, "C")
        o = Owner()
        o.boot("A")
        o.last = "Astar"
        j = self._j()
        j.file = {o.id: ("B", "A", "", "A")}
        L = o.last
        w, s, p, l = j.file[o.id]
        rv = j.r["rep"](True, True, L == w, s != "" and L == s, p != "" and L == p, l != "" and L == l)
        self.assertEqual(rv, STALE)

    def test_seeded_histories(self):
        rng = random.Random(131)
        old, nold = re.subn(r"if \(savedEmpty && prevEmpty && loadedEmpty\)", "if (savedEmpty)", SRC, count=1)
        self.assertEqual(nold, 1)
        old_rules = fns(old)
        checked = 0
        old_b_miss = 0
        hid = 0
        n_eval = 0
        n_g5 = 0
        n_b = 0
        n_inject = 0
        keep_total = 0

        def capture_flags(journal, owner):
            cap = (owner.id in journal.entries
                   and journal.entries[owner.id][0] == owner.live
                   and owner.live != "")
            wr = (journal.file is not None
                  and owner.id in journal.file
                  and journal.file[owner.id][0] == owner.live)
            good = owner.ser_ok and owner.live != ""
            return cap, wr, good

        while checked < 10000:
            hid = hid + 1
            log = ["h=" + str(hid)]
            states = ["A", "B", "C", "D"]
            start = rng.choice(states)
            o = Owner()
            o.boot(start)
            o.ser_ok = True
            j = self._j()
            last_alive = {start: 0}
            for sess in range(rng.randint(2, 4)):
                last_change_captured = True
                last_written = True
                last_good = True
                session_emits = [o.loaded]
                log.append("boot:" + o.loaded)
                steps = rng.randint(4, 10)
                si = 0
                while si < steps:
                    j.now = j.now + rng.choice([1, 5, 10])
                    act = rng.choice(["chg", "chg", "emit", "tick", "tick", "ser"])
                    if act == "chg":
                        st = rng.choice(states)
                        o.change(st)
                        j.mark(o.id)
                        last_alive[st] = j.now
                        last_change_captured = False
                        last_written = False
                        log.append("chg:" + st)
                    elif act == "emit":
                        jsn = o.emit()
                        j.on_saved(o.id, jsn)
                        session_emits.append(jsn)
                        log.append("emit:" + jsn)
                    elif act == "tick":
                        if o.id in j.dirty and rng.random() < 0.4:
                            o.ser_ok = False
                            log.append("ser-fail-tick")
                        j.tick(o)
                        last_change_captured, last_written, last_good = capture_flags(j, o)
                        log.append("tick")
                        o.ser_ok = True
                    else:
                        o.ser_ok = rng.choice([True, True, False])
                        log.append("ser:" + str(o.ser_ok))
                    si = si + 1
                if len(session_emits) == 1:
                    o.ser_ok = True
                    jsn = o.emit()
                    j.on_saved(o.id, jsn)
                    session_emits.append(jsn)
                    log.append("force-emit:" + jsn)
                o.ser_ok = True
                inject_g5 = rng.random() < 0.2
                older = None
                ei = 0
                while ei < len(session_emits):
                    em = session_emits[ei]
                    if em != "" and em != o.live:
                        older = em
                    ei = ei + 1
                if inject_g5:
                    if older is None:
                        older = rng.choice(states)
                        if older == o.live:
                            older = states[(states.index(o.live) + 1) % len(states)]
                        o.change(older)
                        j.mark(o.id)
                        last_alive[older] = j.now
                        jsn = o.emit()
                        j.on_saved(o.id, jsn)
                        session_emits.append(jsn)
                        log.append("g5-seed-emit:" + jsn)
                        other = states[(states.index(older) + 1) % len(states)]
                        o.change(other)
                        j.mark(o.id)
                        last_alive[other] = j.now
                        log.append("g5-seed-away:" + other)
                    j.tick(o)
                    last_change_captured, last_written, last_good = capture_flags(j, o)
                    log.append("tick-before-g5")
                    o.change(older)
                    j.mark(o.id)
                    last_alive[older] = j.now
                    last_change_captured = False
                    last_written = False
                    log.append("g5-revert:" + older + "-no-tick")
                    live_at_death = o.live
                    ce = older
                else:
                    j.tick(o)
                    last_change_captured, last_written, last_good = capture_flags(j, o)
                    live_at_death = o.live
                    last_alive[live_at_death] = j.now
                    if rng.random() < 0.45 and len(session_emits) >= 2:
                        ce = session_emits[-2]
                    else:
                        ce = session_emits[-1]
                if ce == "":
                    keep_total = keep_total + j.keeps
                    log.append("skip-empty-ce")
                    continue
                w_file = None
                if j.file and o.id in j.file:
                    w_file = j.file[o.id][0]
                log.append("die ce=" + ce + " live=" + live_at_death + " W=" + str(w_file)
                           + " cap=" + str(last_change_captured) + " wr=" + str(last_written)
                           + " good=" + str(last_good))
                keep_total = keep_total + j.keeps
                j = restart_session(j, o, ce)
                result = o.live
                comparable = ce in last_alive and result in last_alive
                if comparable:
                    n_eval = n_eval + 1
                    age_ce = j.now - last_alive[ce]
                    age_res = j.now - last_alive[result]
                    if age_res > age_ce:
                        n_g5 = n_g5 + 1
                        if w_file == live_at_death:
                            self.fail("propiedad (a) fuera de G5: " + " ".join(log)
                                      + " result=" + result + " age_res=" + str(age_res)
                                      + " age_ce=" + str(age_ce))
                if last_change_captured and last_written and last_good:
                    n_b = n_b + 1
                    if w_file == live_at_death and ce != live_at_death:
                        n_inject = n_inject + 1
                        injected = ce
                        if injected == live_at_death:
                            self.fail("inyeccion G2 tautologica: " + " ".join(log))
                    if result != live_at_death:
                        self.fail("propiedad (b) G2: " + " ".join(log) + " result=" + result)
            checked = checked + 1
            jo = Journal(old_rules)
            o2 = Owner()
            o2.boot(start)
            o2.change("Z")
            jo.mark(o2.id)
            o2.ser_ok = False
            jo.on_saved(o2.id, o2.emit())
            o2.ser_ok = True
            o2.live = "Z"
            jo.tick(o2)
            restart_session(jo, o2, start)
            if o2.live != "Z":
                old_b_miss = old_b_miss + 1
        self.assertEqual(checked, 10000)
        self.assertGreater(old_b_miss, 0)
        self.assertGreater(keep_total, 0)
        self.assertGreaterEqual(n_eval, 100, "n_eval=" + str(n_eval))
        self.assertGreater(n_g5, 0, "n_g5=" + str(n_g5))
        self.assertGreaterEqual(n_b, 10000, "n_b=" + str(n_b))
        self.assertGreater(n_inject, 0)

    def test_on_owner_saved_slice(self):
        cell = SimpleNamespace(
            m_Loaded=True, m_Enabled=True, m_ReadOnly=False, m_FileDirty=False,
            m_Entries=Map(), m_Dirty=Map(), m_SeenEmit=Map(), m_CapturedMs=Map())
        rec = SimpleNamespace(m_WiresJSON="B")
        cell.m_Entries.Set("d1", rec)
        run_saved_slice(SRC, cell, "d1", "B")
        self.assertIn("d1", cell.m_Entries)
        self.assertTrue(cell.m_SeenEmit.Contains("d1"))
        self.assertFalse(cell.m_FileDirty)
        seen_after_first = cell.m_SeenEmit.Contains("d1")
        run_saved_slice(SRC, cell, "d1", "A")
        self.assertIn("d1", cell.m_Entries)
        self.assertEqual(cell.m_SeenEmit.Contains("d1"), seen_after_first)
        self.assertFalse(cell.m_FileDirty)
        run_saved_slice(SRC, cell, "d1", "B")
        self.assertNotIn("d1", cell.m_Entries)
        seen_any, n = re.subn(
            r"if \(savedJSON == entry\.m_WiresJSON\)",
            "if (savedJSON == entry.m_WiresJSON || m_SeenEmit.Contains(ownerId))",
            SRC, count=1)
        self.assertEqual(n, 1)
        bad = SimpleNamespace(
            m_Loaded=True, m_Enabled=True, m_ReadOnly=False, m_FileDirty=False,
            m_Entries=Map(), m_Dirty=Map(), m_SeenEmit=Map(), m_CapturedMs=Map())
        bad.m_Entries.Set("d1", SimpleNamespace(m_WiresJSON="B"))
        run_saved_slice(seen_any, bad, "d1", "B")
        run_saved_slice(seen_any, bad, "d1", "A")
        self.assertNotIn("d1", bad.m_Entries)
        swapped, n = re.subn(
            r"if \(v == LFPG_WJ_SAVE_FIRST_EMIT\)",
            "if (v == __SWAP_DURABLE__)",
            SRC, count=1)
        self.assertEqual(n, 1)
        swapped, n = re.subn(
            r"if \(v == LFPG_WJ_SAVE_DURABLE\)",
            "if (v == LFPG_WJ_SAVE_FIRST_EMIT)",
            swapped, count=1)
        self.assertEqual(n, 1)
        swapped = swapped.replace("__SWAP_DURABLE__", "LFPG_WJ_SAVE_DURABLE")
        cell = SimpleNamespace(
            m_Loaded=True, m_Enabled=True, m_ReadOnly=False, m_FileDirty=False,
            m_Entries=Map(), m_Dirty=Map(), m_SeenEmit=Map(), m_CapturedMs=Map())
        rec = SimpleNamespace(m_WiresJSON="W")
        cell.m_Entries.Set("d1", rec)
        run_saved_slice(swapped, cell, "d1", "W")
        self.assertNotIn("d1", cell.m_Entries)


if __name__ == "__main__":
    unittest.main()
