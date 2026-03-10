// =========================================================
// LF_PowerGrid - Action: Cycle Motion Sensor Detect Mode (v1.5.0)
//
// Tap F on sensor to cycle: ALL → TEAM → ENEMY → ALL
// Without LBmaster_Groups, stays on ALL and shows feedback.
//
// CCINone (no item in hand) → ActionInteractBase.
//
// IMPORTANTE: Registrar en ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionCycleDetectMode).
// =========================================================

class LFPG_ActionCycleDetectMode : ActionInteractBase
{
    void LFPG_ActionCycleDetectMode()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_CYCLE_DETECT_MODE";
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

        sensor.LFPG_CycleDetectMode();

        // Feedback to player
        PlayerBase pb = PlayerBase.Cast(action_data.m_Player);
        if (pb)
        {
            int mode = sensor.LFPG_GetDetectMode();
            string modeStr = "";
            if (mode == LFPG_SENSOR_MODE_ALL)
            {
                modeStr = "ALL";
            }
            else if (mode == LFPG_SENSOR_MODE_TEAM)
            {
                modeStr = "TEAM";
            }
            else if (mode == LFPG_SENSOR_MODE_ENEMY)
            {
                modeStr = "ENEMY";
            }

            string feedbackMsg = "[LFPG] Detect mode: " + modeStr;

            #ifndef LBmaster_Groups
            if (mode != LFPG_SENSOR_MODE_ALL)
            {
                feedbackMsg = "[LFPG] LBmaster_Groups required for TEAM/ENEMY modes";
            }
            #endif

            pb.MessageStatus(feedbackMsg);
        }
    }
};
