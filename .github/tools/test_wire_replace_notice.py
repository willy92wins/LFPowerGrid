"""FinishWiring replacement notice sits after deletions and before ADD publish."""
from pathlib import Path
import sys
import unittest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from enforce_scalar_slice import method

HANDLER = ROOT / "scripts/5_Mission/LFPG_RPCServerHandlerImpl.c"
SETTINGS = ROOT / "scripts/3_Game/LFPG_Settings.c"
ADD_PUBLISH = "finish.m_Source.m_DeltaOps.Insert(LFPG_WireDeltaOp.ADD)"
MSG_ONE = "Replaced 1 existing cable: each port holds only one cable."
MSG_N = "Replaced \" + replacedCount.ToString() + \" existing cables: each port holds only one cable."
GUARD = "replaceSettings && replaceSettings.NotifyWireReplaced"
SEND = "PlayerBase.LFPG_SendClientMsg(player, replaceMsg);"
ACCUM = "replacedCount = replacedCount + ownerState.m_RemovedWires.Count();"


def notice_contract(handler_src, settings_src):
    if "bool NotifyWireReplaced = true;" not in settings_src:
        return False
    handler = method(handler_src, "HandleFinishWiring")
    if MSG_ONE not in handler or MSG_N not in handler:
        return False
    delete_mark = "manager.PlayerWireCountAdd(removedWire.m_CreatorId, -1)"
    delete_at = handler.rfind(delete_mark)
    add_at = handler.find(ADD_PUBLISH)
    reroute_at = handler.find("if (rerouteWire)")
    if delete_at < 0 or add_at < 0 or reroute_at < 0:
        return False
    if not (delete_at < add_at):
        return False
    notice = handler[delete_at:add_at]
    if GUARD not in notice:
        return False
    if SEND not in notice:
        return False
    if ACCUM not in notice:
        return False
    if reroute_at >= delete_at:
        return False
    reroute_block = handler[reroute_at:handler.find("return;", reroute_at)]
    if MSG_ONE in reroute_block:
        return False
    return True


class WireReplaceNotice(unittest.TestCase):
    def test_source_places_notice_after_delete_before_add(self):
        handler_src = HANDLER.read_text(encoding="utf-8")
        settings_src = SETTINGS.read_text(encoding="utf-8")
        self.assertTrue(notice_contract(handler_src, settings_src))
        handler = method(handler_src, "HandleFinishWiring")
        self.assertIn(MSG_ONE, handler)
        self.assertIn("each port holds only one cable.", handler)

    def test_mutant_without_setting_guard_fails(self):
        handler_src = HANDLER.read_text(encoding="utf-8")
        settings_src = SETTINGS.read_text(encoding="utf-8")
        self.assertTrue(notice_contract(handler_src, settings_src))
        mutant = handler_src.replace(GUARD, "true", 1)
        self.assertNotEqual(mutant, handler_src)
        self.assertFalse(notice_contract(mutant, settings_src))

    def test_mutant_without_send_or_zero_count_fails(self):
        handler_src = HANDLER.read_text(encoding="utf-8")
        settings_src = SETTINGS.read_text(encoding="utf-8")
        self.assertTrue(notice_contract(handler_src, settings_src))
        no_send = handler_src.replace(SEND, "", 1)
        self.assertNotEqual(no_send, handler_src)
        self.assertFalse(notice_contract(no_send, settings_src))
        zeroed = handler_src.replace(ACCUM, "replacedCount = 0;", 1)
        self.assertNotEqual(zeroed, handler_src)
        self.assertFalse(notice_contract(zeroed, settings_src))


if __name__ == "__main__":
    unittest.main()
