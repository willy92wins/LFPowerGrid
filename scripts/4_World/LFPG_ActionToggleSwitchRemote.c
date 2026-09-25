// =========================================================
// LF_PowerGrid - Action: Toggle Switch Remote (v1.0.0)
//
// Latching toggle: flips switch ON->OFF or OFF->ON.
// No item required (CCINone) — player walks up and interacts.
//
// Conditions:
//   - Target must be LFPG_SwitchRemote
//   - Within interact distance
//
// IMPORTANTE: Registrar en ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionToggleSwitchRemote).
// =========================================================

class LFPG_ActionToggleSwitchRemote : LFPG_ActionToggleSwitchBase
{
    void LFPG_ActionToggleSwitchRemote()
    {
        m_LFPG_MessageOn = "[LFPG] Switch Remote ON";
        m_LFPG_MessageOff = "[LFPG] Switch Remote OFF";
    }
    override protected LFPG_SwitchDeviceBase LFPG_SelectSwitch(Object targetObj)
    {
        return LFPG_SwitchRemote.Cast(targetObj);
    }
    override protected void LFPG_ToggleTarget(LFPG_SwitchDeviceBase device)
    {
        LFPG_SwitchRemote sw = LFPG_SwitchRemote.Cast(device);
        if (sw)
            sw.LFPG_ToggleSwitch();
    }
};
