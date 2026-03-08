// =========================================================
// LF_PowerGrid - Action: Open Sorter Panel (v1.2.0 Sprint S4)
//
// Opens the Sorter configuration UI by sending a
// CONFIG_REQUEST RPC to the server.
//
// Conditions:
//   - No item in hand (CCINone)
//   - Target is LF_Sorter
//   - Target is powered
//   - Target is not ruined
//   - Player within LFPG_INTERACT_DIST_M
//
// Flow:
//   Client: send CONFIG_REQUEST(sorterNetLow, sorterNetHigh)
//   Server: resolve → validate → build response payload
//   Server: send CONFIG_RESPONSE(filterJSON, containerName, destNames[6])
//   Client: receive → LFPG_SorterUI.Open(...)
//
// Pattern: ActionInteractBase (same as LFPG_ActionWatchMonitor)
// Register in LFPG_ActionRegistration + LF_Sorter.SetActions
// =========================================================

class LFPG_ActionOpenSorterPanel : ActionInteractBase
{
    void LFPG_ActionOpenSorterPanel()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text       = "#STR_LFPG_ACTION_OPEN_SORTER";
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

        // Don't open if UI is already showing
        if (LFPG_SorterUI.IsOpen())
            return false;

        return true;
    }

    // RPC in OnExecuteClient (after animation completes, same pattern
    // as LFPG_ActionWatchMonitor v0.9.6 crash fix).
    override void OnExecuteClient(ActionData action_data)
    {
        super.OnExecuteClient(action_data);

        if (!action_data.m_Target)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        string checkType = "LF_Sorter";
        if (!targetObj.IsKindOf(checkType))
            return;

        int netLow  = 0;
        int netHigh = 0;
        targetObj.GetNetworkID(netLow, netHigh);

        ScriptRPC rpc = new ScriptRPC();
        int subId = LFPG_RPC_SubId.SORTER_CONFIG_REQUEST;
        rpc.Write(subId);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Send(action_data.m_Player, LFPG_RPC_CHANNEL, true, null);
    }

    override void OnExecuteServer(ActionData action_data) {}
};
