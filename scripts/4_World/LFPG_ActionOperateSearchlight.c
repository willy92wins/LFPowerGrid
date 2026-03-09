// =========================================================
// LF_PowerGrid - Action: Operate Searchlight (v1.4.0)
//
// Appears when looking at a powered LF_Searchlight without
// item in hand and controller not already active.
//
// Pattern: LFPG_ActionWatchMonitor (CCINone, RPC on OnExecuteClient).
//
// Flow:
//   Client: send RPC SEARCHLIGHT_ENTER(netLow, netHigh)
//   Server: validate + SelectSpectator + ENTER_CONFIRM(yaw, pitch)
//   Client: SearchlightController.Enter()
//
// Register in LF_Searchlight.SetActions() and ActionRegistration.
// =========================================================

class LFPG_ActionOperateSearchlight : ActionInteractBase
{
    void LFPG_ActionOperateSearchlight()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text       = "#STR_LFPG_ACTION_OPERATE_SEARCHLIGHT";
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

        string slType = "LF_Searchlight";
        if (!targetObj.IsKindOf(slType))
            return false;

        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), targetObj.GetPosition());
        float maxDist = LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M;
        if (distSq > maxDist)
            return false;

        LF_Searchlight sl = LF_Searchlight.Cast(targetObj);
        if (!sl)
            return false;

        if (!sl.LFPG_IsPowered())
            return false;

        // Block if already in searchlight or CCTV mode
        LFPG_SearchlightController ctrl = LFPG_SearchlightController.Get();
        if (ctrl && ctrl.IsActive())
            return false;

        LFPG_CameraViewport vp = LFPG_CameraViewport.Get();
        if (vp && vp.IsActive())
            return false;

        return true;
    }

    // RPC in OnExecuteClient — fires AFTER animation completes.
    // Pattern: LFPG_ActionWatchMonitor v0.9.6 crash fix.
    override void OnExecuteClient(ActionData action_data)
    {
        super.OnExecuteClient(action_data);

        if (!action_data.m_Target)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        string slCheck = "LF_Searchlight";
        if (!targetObj.IsKindOf(slCheck))
            return;

        int netLow  = 0;
        int netHigh = 0;
        targetObj.GetNetworkID(netLow, netHigh);

        ScriptRPC rpc = new ScriptRPC();
        int subId = LFPG_RPC_SubId.SEARCHLIGHT_ENTER;
        rpc.Write(subId);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Send(action_data.m_Player, LFPG_RPC_CHANNEL, true, null);
    }

    override void OnExecuteServer(ActionData action_data) {}
};
