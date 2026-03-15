// =========================================================
// LF_PowerGrid - Action: Toggle Intercom On/Off (v3.0.0)
//
// Latching toggle: flips intercom ON/OFF (gate control).
// No item required (CCINone) — player walks up and interacts.
//
// Conditions:
//   - Target must be LF_Intercom
//   - Within interact distance
//   - Must have power (m_PoweredNet == true)
//
// IMPORTANTE: Registrar en ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionToggleIntercom).
// =========================================================

class LFPG_ActionToggleIntercom : ActionInteractBase
{
    void LFPG_ActionToggleIntercom()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_TOGGLE_INTERCOM";
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

        LF_Intercom ic = LF_Intercom.Cast(targetObj);
        if (!ic)
            return false;

        // Must have power to toggle
        if (!ic.LFPG_IsPowered())
            return false;

        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), ic.GetPosition());
        float maxDistSq = LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M;
        if (distSq > maxDistSq)
            return false;

        // Dynamic text: show current → target state
        if (ic.LFPG_GetSwitchOn())
        {
            m_Text = "Turn OFF Intercom";
        }
        else
        {
            m_Text = "Turn ON Intercom";
        }

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

        LF_Intercom ic = LF_Intercom.Cast(targetObj);
        if (!ic)
            return;

        ic.LFPG_ToggleIntercom();

        PlayerBase pb = PlayerBase.Cast(action_data.m_Player);
        if (pb)
        {
            if (ic.LFPG_GetSwitchOn())
            {
                string onMsg = "[LFPG] Intercom ON";
                pb.MessageStatus(onMsg);
            }
            else
            {
                string offMsg = "[LFPG] Intercom OFF";
                pb.MessageStatus(offMsg);
            }
        }
    }
};
