// =========================================================
// LF_PowerGrid - Action: Speaker ON (v1.0.0)
//
// Turns the speaker knob to ON position.
// Visible only when speaker is currently OFF.
// Does NOT require power — knob is physical, always usable.
// When powered + on, GhostPASReceiver spawns and audio plays.
//
// IMPORTANTE: Registrar en ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionSpeakerOn).
// =========================================================

class LFPG_ActionSpeakerOn : ActionInteractBase
{
    void LFPG_ActionSpeakerOn()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_SPEAKER_ON";
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

        LFPG_Speaker spk = LFPG_Speaker.Cast(targetObj);
        if (!spk)
            return false;

        // Only show when speaker is OFF
        if (spk.LFPG_GetSpeakerOn())
            return false;

        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), spk.GetPosition());
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

        LFPG_Speaker spk = LFPG_Speaker.Cast(targetObj);
        if (!spk)
            return;

        spk.LFPG_ToggleSpeaker(true);

        PlayerBase pb = PlayerBase.Cast(action_data.m_Player);
        if (pb)
        {
            string msg = "[LFPG] Speaker ON";
            pb.MessageStatus(msg);
        }
    }
};
