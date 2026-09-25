// =========================================================
// LF_PowerGrid - Action: Toggle Switch V2 Remote (v1.0.0)
//
// Latching toggle: flips switch ON->OFF or OFF->ON.
// No item required (CCINone) - player walks up and interacts.
//
// Conditions:
//   - Target must be LFPG_SwitchV2Remote
//   - Within interact distance
//   - No debounce: action always available (bidirectional toggle)
//
// IMPORTANTE: Registrar en ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionToggleSwitchV2Remote).
// =========================================================

class LFPG_ActionToggleSwitchV2Remote : LFPG_ActionToggleSwitchBase
{
    void LFPG_ActionToggleSwitchV2Remote()
    {
        m_LFPG_MessageOn = "[LFPG] Switch V2 Remote ON";
        m_LFPG_MessageOff = "[LFPG] Switch V2 Remote OFF";
    }
    override protected LFPG_SwitchDeviceBase LFPG_SelectSwitch(Object targetObj)
    {
        return LFPG_SwitchV2Remote.Cast(targetObj);
    }
    override protected void LFPG_ToggleTarget(LFPG_SwitchDeviceBase device)
    {
        LFPG_SwitchV2Remote sw = LFPG_SwitchV2Remote.Cast(device);
        if (sw)
            sw.LFPG_ToggleSwitch();
    }
};
