"""Momentary pulse survives a splitter blackout: SetPowered does not cancel it."""
from pathlib import Path
import re
import sys
import unittest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from enforce_scalar_slice import method

BUTTON = ROOT / "scripts/4_World/LFPG_PushButton.c"
REMOTE = ROOT / "scripts/4_World/LFPG_SwitchRemote.c"
CANCEL = """        if (!powered && m_SwitchOn)
        {
            g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_PulseOff);
            m_SwitchOn = false;
        }
"""


def setpowered_ok(source):
    body = method(source, "LFPG_SetPowered")
    if "Remove(LFPG_PulseOff)" in body:
        return False
    if "m_SwitchOn = false" in body:
        return False
    return True


class PulseSurvivesBlackout(unittest.TestCase):
    def test_setpowered_does_not_cancel_pulse(self):
        button = BUTTON.read_text(encoding="utf-8")
        remote = REMOTE.read_text(encoding="utf-8")
        self.assertTrue(setpowered_ok(button))
        self.assertTrue(setpowered_ok(remote))

    def test_mutant_restoring_cancel_block_fails(self):
        button = BUTTON.read_text(encoding="utf-8")
        remote = REMOTE.read_text(encoding="utf-8")
        mutant_button, n_b = re.subn(
            r"(m_PoweredNet = powered;)",
            r"\1\n" + CANCEL,
            button,
            count=1)
        mutant_remote, n_r = re.subn(
            r"(m_PoweredNet = powered;)",
            r"\1\n" + CANCEL,
            remote,
            count=1)
        self.assertEqual(n_b, 1)
        self.assertEqual(n_r, 1)
        self.assertFalse(setpowered_ok(mutant_button))
        self.assertFalse(setpowered_ok(mutant_remote))
        self.assertTrue(setpowered_ok(button))
        self.assertTrue(setpowered_ok(remote))

    def test_toggle_schedules_pulseoff(self):
        button = BUTTON.read_text(encoding="utf-8")
        remote = REMOTE.read_text(encoding="utf-8")
        self.assertTrue(pulse_armed(button, "LFPG_ToggleButton"))
        self.assertTrue(pulse_armed(remote, "LFPG_ToggleSwitch"))
        mutant_button, n_b = re.subn(
            r"CallLater\(LFPG_PulseOff",
            "CallLater(LFPG_Unused",
            button,
            count=1)
        mutant_remote, n_r = re.subn(
            r"CallLater\(LFPG_PulseOff",
            "CallLater(LFPG_Unused",
            remote,
            count=1)
        self.assertEqual(n_b, 1)
        self.assertEqual(n_r, 1)
        self.assertFalse(pulse_armed(mutant_button, "LFPG_ToggleButton"))
        self.assertFalse(pulse_armed(mutant_remote, "LFPG_ToggleSwitch"))


def pulse_armed(source, toggle_name):
    tog = method(source, toggle_name)
    if "CallLater(LFPG_PulseOff" not in tog:
        return False
    off = method(source, "LFPG_PulseOff")
    if "m_SwitchOn = false" not in off:
        return False
    return True


if __name__ == "__main__":
    unittest.main()
