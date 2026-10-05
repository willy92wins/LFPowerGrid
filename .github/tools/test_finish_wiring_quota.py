"""Q-01: FinishWiring quota admission with stubs; no Enforce or native callbacks.

FinishWiringSamePair and FinishWiringNeedsQuotaSlot run as production slices.
The quota guard is the actual condition that follows the same-pair lookup in
HandleFinishWiring. Typed locals lose their type, FinishWiringSourcePort and
IncomingPortIndexKey are stubbed. Ordering against the early gates, the port
lock and the store mutation is checked on the source text.
"""
from pathlib import Path
from types import SimpleNamespace
import re
import unittest
from enforce_scalar_slice import method, scalar_function

ROOT = Path(__file__).resolve().parents[2]
HANDLER = ROOT / "scripts/5_Mission/LFPG_RPCServerHandlerImpl.c"
QUOTA_CALL = "bool quotaFree = LFPG_NetworkManager.Get().CanPlayerCreateAnotherWire(sender, quotaReason);"
SAME_PAIR_CALL = "LFPG_WireData rerouteWire = FinishWiringSamePair(finish, srcPort, dstRealId, dstPort);"


class List(list):
    def Count(self):
        return len(self)


def definition(source, name):
    # Skip call sites, which method() would also accept as a header.
    return method(source[source.index("static " + name):], name.split()[1])


def untyped(body):
    return re.sub(r"\bLFPG_WireData\s+(\w+)\s*=", r"\1 =", body)


class Admission:
    def __init__(self, source=None):
        self.source = source or HANDLER.read_text(encoding="utf-8")
        stubs = {"FinishWiringSourcePort": lambda wire, isNative: wire.m_SourcePort,
                 "IncomingPortIndexKey": lambda port: port}
        for header, arguments in [("bool FinishWiringNeedsQuotaSlot", ["rerouteWire", "creatorId"]),
                                  ("LFPG_WireData FinishWiringSamePair", ["finish", "srcPort", "dstId", "dstPort"])]:
            name = header.split()[1]
            environment = {"__builtins__": {}, **stubs}
            body = untyped(definition(self.source, header))
            exec(compile(scalar_function(body, arguments), "<source-slice:" + name + ">", "exec"), environment)
            stubs[name] = environment["run"]
        self.stubs = stubs
        handler = method(self.source, "HandleFinishWiring")
        start = handler.index(SAME_PAIR_CALL)
        end = handler.index("if (rerouteWire)", start)
        guards = [g for g in re.findall(r"if \((.*)\)\s*\{", handler[start:end]) if "quotaFree" in g]
        assert len(guards) == 1, guards
        body = untyped(SAME_PAIR_CALL) + "\nif (" + guards[0] + ") { return false; }\nreturn true;"
        environment = {"__builtins__": {}, **stubs}
        exec(compile(scalar_function(body, ["finish", "srcPort", "dstRealId", "dstPort", "creatorId", "quotaFree"]),
                     "<source-slice:HandleFinishWiring-quota>", "exec"), environment)
        self.admit = environment["run"]


def wire(creator, target="dst", sourcePort="out1", targetPort="in1"):
    return SimpleNamespace(m_CreatorId=creator, m_TargetDeviceId=target,
                           m_SourcePort=sourcePort, m_TargetPort=targetPort)


def conflicts(*rows):
    # FinishWiringCollect result: the source owner and the rows it would remove.
    source = SimpleNamespace(m_IsNative=True, m_RemovedWires=List(rows))
    return SimpleNamespace(m_Source=source, m_Owners=List([source]))


class FinishWiringQuota(unittest.TestCase):
    def test_quota_full_same_pair_reroute_of_own_wire_is_allowed(self):
        admission = Admission()
        self.assertTrue(admission.admit(conflicts(wire("alice")), "out1", "dst", "in1", "alice", False))

    def test_quota_full_new_wire_is_still_blocked(self):
        admission = Admission()
        self.assertFalse(admission.admit(conflicts(), "out1", "dst", "in1", "alice", False))
        # A replacement on the same output to another target is a new row, not a reroute.
        self.assertFalse(admission.admit(conflicts(wire("alice", target="other")), "out1", "dst", "in1", "alice", False))
        # Two conflicting rows never take the reroute path.
        self.assertFalse(admission.admit(conflicts(wire("alice"), wire("alice", sourcePort="out2")),
                                         "out1", "dst", "in1", "alice", False))

    def test_quota_full_reroute_that_takes_over_another_creator_is_blocked(self):
        # FinishWiringReroute moves the row's count to the sender: that is a new slot.
        admission = Admission()
        self.assertFalse(admission.admit(conflicts(wire("bob")), "out1", "dst", "in1", "alice", False))

    def test_free_quota_admits_every_shape(self):
        admission = Admission()
        for finish in [conflicts(), conflicts(wire("alice")), conflicts(wire("bob")),
                       conflicts(wire("alice", target="other"))]:
            self.assertTrue(admission.admit(finish, "out1", "dst", "in1", "alice", True))

    def test_needs_quota_slot_contract(self):
        needs = Admission().stubs["FinishWiringNeedsQuotaSlot"]
        self.assertTrue(needs(None, "alice"))
        self.assertTrue(needs(wire("bob"), "alice"))
        self.assertFalse(needs(wire("alice"), "alice"))

    def test_quota_denial_is_deferred_to_after_same_pair_and_before_any_mutation(self):
        handler = method(HANDLER.read_text(encoding="utf-8"), "HandleFinishWiring")
        quota = handler.index(QUOTA_CALL)
        same = handler.index(SAME_PAIR_CALL)
        guard = handler.index("if (!quotaFree", same)
        # Nothing between the quota query and the same-pair lookup may act on it.
        self.assertNotIn("quotaFree", handler[quota + len(QUOTA_CALL):same])
        self.assertEqual(handler.count("CanPlayerCreateAnotherWire"), 1)
        deny = handler[guard:handler.index("}", guard)]
        self.assertIn("manager.UnlockPort(portLockKey)", deny)
        self.assertIn("return", deny)
        for mutation in ["FinishWiringReroute(", "graph.OnWireAdded(", "AddDeviceWire(",
                         "AddVanillaWire(", "PlayerWireCountAdd("]:
            self.assertLess(guard, handler.index(mutation))
        self.assertLess(handler.index("LockPort(portLockKey)"), guard)

    def test_negative_controls_early_gate_and_unconditional_slot(self):
        source = HANDLER.read_text(encoding="utf-8")
        # The pre-fix shape: denial before the same-pair lookup.
        early, count = re.subn(re.escape(QUOTA_CALL), QUOTA_CALL + "\n        if (!quotaFree)\n"
                               "        {\n            return;\n        }", source)
        self.assertEqual(count, 1)
        handler = method(early, "HandleFinishWiring")
        self.assertIn("quotaFree", handler[handler.index(QUOTA_CALL) + len(QUOTA_CALL):handler.index(SAME_PAIR_CALL)])
        # A helper that always asks for a slot rejects the own-wire reroute again.
        body = definition(source, "bool FinishWiringNeedsQuotaSlot")
        always = source.replace(body, body.replace("return false;", "return true;"), 1)
        self.assertNotEqual(always, source)
        admission = Admission(always)
        self.assertFalse(admission.admit(conflicts(wire("alice")), "out1", "dst", "in1", "alice", False))
        # A helper that never asks for one lets a quota-full new wire through.
        never = source.replace(body, re.sub(r"return true;", "return false;", body), 1)
        admission = Admission(never)
        self.assertTrue(admission.admit(conflicts(), "out1", "dst", "in1", "alice", False))


if __name__ == "__main__":
    unittest.main()
