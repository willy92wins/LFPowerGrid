// =========================================================
// LF_PowerGrid - Action: Open BTC ATM Panel (Sprint BTC-2)
//
// Opens the BTC ATM interface by sending a BTC_OPEN_REQUEST
// RPC to the server. Server responds with price + stock + balance.
//
// Works on BOTH LF_BTCAtm and LF_BTCAtmAdmin (via base class check).
//
// Conditions:
//   - No item in hand (CCINone)
//   - Target is LFPG_BTCAtmBase (or subclass)
//   - Target is powered (consumer must have power, admin always true)
//   - Target is not ruined
//   - Player within LFPG_INTERACT_DIST_M
//
// Flow:
//   Client: send BTC_OPEN_REQUEST(atmNetLow, atmNetHigh)
//   Server: resolve → validate → read price/stock/balance
//   Server: send BTC_OPEN_RESPONSE(price, stock, balance, withdrawOnly)
//   Client: receive → open UI (Sprint BTC-4)
//
// Pattern: ActionInteractBase (same as LFPG_ActionOpenSorterPanel)
// Register in ActionConstructor + both ATM device SetActions.
// =========================================================

class LFPG_ActionOpenBTCAtm : ActionInteractBase
{
    void LFPG_ActionOpenBTCAtm()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text       = "#STR_LFPG_ACTION_OPEN_BTC_ATM";
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

        // Check for base class (covers both LF_BTCAtm and LF_BTCAtmAdmin)
        LFPG_BTCAtmBase atm = LFPG_BTCAtmBase.Cast(targetObj);
        if (!atm)
            return false;

        // Must be powered (consumer checks m_PoweredNet, admin returns true)
        if (!atm.LFPG_IsATMPowered())
            return false;

        // Must not be ruined
        if (atm.IsRuined())
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

        LFPG_BTCAtmBase atm = LFPG_BTCAtmBase.Cast(targetObj);
        if (!atm)
            return;

        int netLow  = 0;
        int netHigh = 0;
        targetObj.GetNetworkID(netLow, netHigh);

        ScriptRPC rpc = new ScriptRPC();
        int subId = LFPG_RPC_SubId.BTC_OPEN_REQUEST;
        rpc.Write(subId);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Send(action_data.m_Player, LFPG_RPC_CHANNEL, true, null);

        string logMsg = "[ActionOpenBTCAtm] RPC sent netId=";
        logMsg = logMsg + netLow.ToString();
        logMsg = logMsg + ":";
        logMsg = logMsg + netHigh.ToString();
        LFPG_Util.Info(logMsg);
    }

    override void OnExecuteServer(ActionData action_data)
    {
        // Server-side handled via LFPG_PlayerRPC (Sprint BTC-3)
    }
};
