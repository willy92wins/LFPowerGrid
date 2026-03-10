// =========================================================
// LF_PowerGrid - Action: Pair Motion Sensor (v1.5.0)
//
// Tap F (scroll to select) on sensor to pair it with player's group.
// Overwrites the existing paired group name.
// Requires LBmaster_Groups — without it, shows feedback and does nothing.
//
// CCINone (no item in hand) → ActionInteractBase.
// Player scrolls mouse wheel to choose between CycleDetectMode and PairSensor.
//
// IMPORTANTE: Registrar en ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionPairSensor).
// =========================================================

class LFPG_ActionPairSensor : ActionInteractBase
{
    void LFPG_ActionPairSensor()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_PAIR_SENSOR";
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

        LFPG_MotionSensor sensor = LFPG_MotionSensor.Cast(targetObj);
        if (!sensor)
            return false;

        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), sensor.GetPosition());
        float maxDistSq = LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M;
        if (distSq > maxDistSq)
            return false;

        return true;
    }

    override bool HasTarget()
    {
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

        LFPG_MotionSensor sensor = LFPG_MotionSensor.Cast(targetObj);
        if (!sensor)
            return;

        PlayerBase pb = PlayerBase.Cast(action_data.m_Player);
        if (!pb)
            return;

        string groupName = "";
        string feedbackMsg = "";

        #ifdef LBmaster_Groups
        LBGroup grp = pb.GetLBGroup();
        if (grp)
        {
            groupName = grp.name;
            feedbackMsg = "[LFPG] Sensor paired to group: " + groupName;
        }
        else
        {
            feedbackMsg = "[LFPG] You have no group. Sensor will detect all players.";
        }
        #else
        feedbackMsg = "[LFPG] LBmaster_Groups not installed. Pairing unavailable.";
        #endif

        sensor.LFPG_SetPairedGroupName(groupName);
        pb.MessageStatus(feedbackMsg);
    }
};
