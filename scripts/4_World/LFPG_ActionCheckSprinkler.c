// =========================================================
// LF_PowerGrid - Action: Check Sprinkler (v1.0.0)
//
// Instant action (no progress bar). Tap F on sprinkler to
// see its current status (Active / No power / No water / etc).
//
// CCINone (no item in hand) → ActionInteractBase.
//
// IMPORTANTE: Registrar en ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionCheckSprinkler).
// =========================================================

class LFPG_ActionCheckSprinkler : ActionInteractBase
{
    void LFPG_ActionCheckSprinkler()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_CHECK_SPRINKLER";
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

        LFPG_Sprinkler spr = LFPG_Sprinkler.Cast(targetObj);
        if (!spr)
            return false;

        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), spr.GetPosition());
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

        LFPG_Sprinkler spr = LFPG_Sprinkler.Cast(targetObj);
        if (!spr)
            return;

        PlayerBase pb = PlayerBase.Cast(action_data.m_Player);
        if (!pb)
            return;

        // Determine status message
        string statusMsg = "";

        if (spr.IsRuined())
        {
            statusMsg = "#STR_LFPG_SPRINKLER_STATUS_DAMAGED";
        }
        else if (!spr.LFPG_GetPoweredNet())
        {
            statusMsg = "#STR_LFPG_SPRINKLER_STATUS_NO_POWER";
        }
        else if (!spr.LFPG_GetHasWaterSource())
        {
            statusMsg = "#STR_LFPG_SPRINKLER_STATUS_NO_WATER";
        }
        else if (spr.LFPG_GetSprinklerActive())
        {
            statusMsg = "#STR_LFPG_SPRINKLER_STATUS_ACTIVE";
        }
        else
        {
            // Powered + has water source but not active → tank exhausted (T2 output_3)
            statusMsg = "#STR_LFPG_SPRINKLER_STATUS_EXHAUSTED";
        }

        pb.MessageStatus(statusMsg);
    }
};
