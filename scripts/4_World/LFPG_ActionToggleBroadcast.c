// =========================================================
// LF_PowerGrid - Action: Toggle Broadcast (v3.0.0 - Sprint 3)
//
// Enables/disables T2 broadcast mode on the Intercom.
// When enabled + powered: ghost radio transmits and receives VOIP.
// When disabled: ghost radio destroyed, LED2 off.
//
// Conditions:
//   - Target must be LFPG_Intercom
//   - m_RadioInstalled == true (T2 upgrade completed)
//
// IMPORTANTE: Registrar en ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionToggleBroadcast).
// =========================================================

class LFPG_ActionToggleBroadcast : ActionInteractBase
{
    void LFPG_ActionToggleBroadcast()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_TOGGLE_BROADCAST";
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

        LFPG_Intercom ic = LFPG_Intercom.Cast(targetObj);
        if (!ic)
            return false;

        // T2 only — radio must be installed
        if (!ic.LFPG_GetRadioInstalled())
            return false;

        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), ic.GetPosition());
        float maxDistSq = LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M;
        if (distSq > maxDistSq)
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

        LFPG_Intercom ic = LFPG_Intercom.Cast(targetObj);
        if (!ic)
            return;

        ic.LFPG_ToggleBroadcast();

        PlayerBase pb = PlayerBase.Cast(action_data.m_Player);
        if (pb)
        {
            if (ic.LFPG_GetBroadcastEnabled())
            {
                string onMsg = "[LFPG] Broadcast ON";
                pb.MessageStatus(onMsg);
            }
            else
            {
                string offMsg = "[LFPG] Broadcast OFF";
                pb.MessageStatus(offMsg);
            }
        }
    }
};
