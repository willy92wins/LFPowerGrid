// =========================================================
// LF_PowerGrid - Action: Toggle Battery Output (v2.0)
//
// Toggles m_OutputEnabled on batteries with a switch button.
// When OFF: battery stops discharging (no virtual generation),
//           gate closes (no passthrough), but still charges.
// When ON:  normal operation (charge + discharge + passthrough).
//
// Conditions:
//   - Target must be LFPG_BatteryBase (any tier)
//   - Tier must be gate-capable (Medium/Large have switch)
//   - Within interact distance
//
// IMPORTANTE: Registrar en ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionToggleBatteryOutput).
// =========================================================

class LFPG_ActionToggleBatteryOutput : ActionInteractBase
{
    void LFPG_ActionToggleBatteryOutput()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_TOGGLE_BATTERY_OUTPUT";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem   = new CCINone;
        m_ConditionTarget = new CCTCursor(LFPG_INTERACT_DIST_M);
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!player)
            return false;

        if (!target)
            return false;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return false;

        // Must be a battery device (any tier)
        LFPG_BatteryBase bat = LFPG_BatteryBase.Cast(targetObj);
        if (!bat)
            return false;

        // Manual proximity check (CCTCursor does not enforce by type)
        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), bat.GetPosition());
        float maxDistSq = LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M;
        if (distSq > maxDistSq)
            return false;

        // Only show action for tiers with switch button (Medium/Large).
        // Small has no switch → IsGateCapable returns false (base default).
        bool gateCapable = LFPG_DeviceAPI.IsGateCapable(bat);
        if (!gateCapable)
            return false;

        return true;
    }

    override void OnExecuteServer(ActionData action_data)
    {
        super.OnExecuteServer(action_data);

        if (!action_data)
            return;

        if (!action_data.m_Target)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        LFPG_BatteryBase bat = LFPG_BatteryBase.Cast(targetObj);
        if (!bat)
            return;

        // Toggle output state
        bool wasEnabled = bat.LFPG_IsOutputEnabled();
        bool newEnabled = !wasEnabled;
        bat.LFPG_SetOutputEnabled(newEnabled);

        // Log
        string togMsg = "[LFPG_Battery] Toggle output ";
        if (newEnabled)
        {
            togMsg = togMsg + "ON";
        }
        else
        {
            togMsg = togMsg + "OFF";
        }
        string devId = bat.LFPG_GetDeviceId();
        togMsg = togMsg + " id=";
        togMsg = togMsg + devId;
        LFPG_Util.Info(togMsg);

        // Propagate immediately so downstream sees gate change
        LFPG_NetworkManager.Get().RequestPropagate(devId);

        // Player feedback
        PlayerBase pb = PlayerBase.Cast(action_data.m_Player);
        if (pb)
        {
            if (newEnabled)
            {
                string onMsg = "[LFPG] Battery output ON";
                pb.MessageStatus(onMsg);
            }
            else
            {
                string offMsg = "[LFPG] Battery output OFF (charging only)";
                pb.MessageStatus(offMsg);
            }
        }
    }
};
