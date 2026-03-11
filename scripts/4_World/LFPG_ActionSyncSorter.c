// =========================================================
// LF_PowerGrid - Action: Sync Sorter Container (v2.4)
//
// Re-scans for nearest container within LFPG_SORTER_LINK_RADIUS
// and links it. Works whether already linked or not (re-link).
//
// Conditions:
//   - No item in hand (CCINone)
//   - Target is LF_Sorter
//   - Target is powered
//   - Target is not ruined
//   - Player within LFPG_INTERACT_DIST_M
//   - Sorter UI not already open
//
// Flow:
//   Client: send SORTER_RESYNC(sorterNetLow, sorterNetHigh)
//   Server: unlink old → re-scan → link nearest → resolve name
//   Server: send SORTER_RESYNC_ACK(containerName)
//   Client: MessageStatus with result
//
// Pattern: ActionInteractBase (CCINone, same as ActionOpenSorterPanel)
// =========================================================

class LFPG_ActionSyncSorter : ActionInteractBase
{
    void LFPG_ActionSyncSorter()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text       = "#STR_LFPG_ACTION_SYNC_SORTER";
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

        string sorterType = "LF_Sorter";
        if (!targetObj.IsKindOf(sorterType))
            return false;

        LF_Sorter sorter = LF_Sorter.Cast(targetObj);
        if (!sorter)
            return false;

        // Must be powered
        if (!sorter.LFPG_IsPowered())
            return false;

        // Must not be ruined
        if (sorter.IsRuined())
            return false;

        // Don't allow if UI is open
        if (LFPG_SorterView.IsOpen())
            return false;

        return true;
    }

    override void OnExecuteClient(ActionData action_data)
    {
        super.OnExecuteClient(action_data);

        if (!action_data.m_Target)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        EntityAI targetEnt = EntityAI.Cast(targetObj);
        if (!targetEnt)
            return;

        int netLow = 0;
        int netHigh = 0;
        targetEnt.GetNetworkID(netLow, netHigh);

        PlayerBase player = action_data.m_Player;
        if (!player)
            return;

        ScriptRPC rpc = new ScriptRPC();
        int subId = LFPG_RPC_SubId.SORTER_RESYNC;
        rpc.Write(subId);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);

        string logMsg = "[ActionSyncSorter] RPC sent netId=";
        logMsg = logMsg + netLow.ToString();
        logMsg = logMsg + ":";
        logMsg = logMsg + netHigh.ToString();
        LFPG_Util.Info(logMsg);
    }
};
