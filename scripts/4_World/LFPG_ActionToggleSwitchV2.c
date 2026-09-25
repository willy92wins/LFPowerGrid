// =========================================================
// LF_PowerGrid - Action: Toggle Switch V2 (v1.6.0)
//
// Latching toggle: flips switch ON→OFF or OFF→ON.
// No item required (CCINone) — player walks up and interacts.
//
// Conditions:
//   - Target must be LFPG_SwitchV2
//   - Within interact distance
//   - No debounce: action always available (bidirectional toggle)
//
// IMPORTANTE: Registrar en ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionToggleSwitchV2).
// =========================================================

class LFPG_ActionToggleSwitchV2 : LFPG_ActionToggleSwitchBase
{
    void LFPG_ActionToggleSwitchV2()
    {
        m_LFPG_MessageOn = "[LFPG] Switch ON";
        m_LFPG_MessageOff = "[LFPG] Switch OFF";
    }
    override protected LFPG_SwitchDeviceBase LFPG_SelectSwitch(Object targetObj)
    {
        return LFPG_SwitchV2.Cast(targetObj);
    }
    override protected void LFPG_ToggleTarget(LFPG_SwitchDeviceBase device)
    {
        LFPG_SwitchV2 sw = LFPG_SwitchV2.Cast(device);
        if (sw)
            sw.LFPG_ToggleSwitch();
    }
};
