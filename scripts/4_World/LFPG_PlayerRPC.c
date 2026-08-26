// =========================================================
// LF_PowerGrid - PlayerBase RPC routing (server authoritative)
// v5.0: Crash fix refactor — thin dispatcher only.
//   All handler logic moved to static helper classes:
//   - LFPG_RPCServerHandler.c (server handlers)
//   - LFPG_RPCClientHandler.c (client handlers)
//   - LFPG_BTCHelper.c (BTC ATM server handlers + utilities)
//
// Reason: ~56 methods on modded PlayerBase combined with 8+ other
// mods overflowed Enforce VM internal method table, causing
// ACCESS_VIOLATION during CDPCreateServer on 57+ mod servers.
// =========================================================

// COT pattern: flag on MissionBaseWorld (4_World layer).
// MissionGameplay (5_Mission) inherits this and overrides ResetGUI.
// PlayerBase.LFPG_SetSkipOnSelectPlayer propagates flag here.
modded class MissionBaseWorld
{
    #ifndef SERVER
    protected bool m_LFPG_SkipResetGUI = false;
    #endif

    #ifndef SERVER
    void LFPG_SetSkipResetGUI(bool skip)
    {
        m_LFPG_SkipResetGUI = skip;
    }
    #endif

    // Vanilla missionbaseworld.c:3-6 uses a base factory for mission-owned services.
    LFPG_ElecGraph LFPG_CreateElecGraph() { return null; }

    // The network manager lives in the mission arena as well. Unlike the graph
    // this one is reached from client code, so both missions override it.
    LFPG_NetworkManager LFPG_CreateNetworkManager() { return null; }

    // Server RPC handlers live in the mission arena; the World arena only
    // carries this seam. Base is a no-op so a client mission drops silently.
    void LFPG_DispatchServerRPC(PlayerBase player, PlayerIdentity sender, int subId, ParamsReadContext ctx) { }

    // ATM stock belongs to the native balance implementation, which lives in the
    // mission arena. Routed here and NOT through LFPG_BalanceRegistry: the active
    // balance provider answers who holds the player EUR, which is a different
    // question and is LBmaster on servers that run it.
    bool LFPG_AtmCanPrepareStockMutation(string deviceId, int stockBefore, int stockTarget) { return false; }
    bool LFPG_AtmPrepareStockMutation(string deviceId, int stockBefore, int stockTarget) { return false; }
    void LFPG_AtmReconcileLoaded(LFPG_BTCAtmBase atm) { }

    // Published to external mods from the World arena; implemented in Mission.
    int LFPG_NativeGetPlayerBalance(string uid) { return 0; }
    bool LFPG_NativeSetPlayerBalance(string uid, int balance) { return false; }
};

modded class PlayerBase
{
    // COT pattern: prevent vanilla OnSelectPlayer + ResetGUI side effects
    // during SelectPlayer(sender, NULL). Flag on BOTH PlayerBase AND Mission.
    // Without Mission flag, ResetGUI crashes when player is null.
    protected bool m_LFPG_SkipOnSelectPlayer = false;

    void LFPG_SetSkipOnSelectPlayer(bool skip)
    {
        m_LFPG_SkipOnSelectPlayer = skip;

        #ifndef SERVER
        MissionBaseWorld mission = MissionBaseWorld.Cast(g_Game.GetMission());
        if (mission)
        {
            mission.LFPG_SetSkipResetGUI(skip);
        }
        #endif
    }

    override void OnSelectPlayer()
    {
        if (m_LFPG_SkipOnSelectPlayer)
        {
            m_LFPG_SkipOnSelectPlayer = false;
            string skipMsg = "[LF_PowerGrid] OnSelectPlayer skipped (CCTV spectator transition)";
            Print(skipMsg);
            return;
        }
        super.OnSelectPlayer();
    }

    override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
    {
        super.OnRPC(sender, rpc_type, ctx);

        if (rpc_type != LFPG_RPC_CHANNEL)
            return;

        int subId;
        if (!ctx.Read(subId))
            return;

        #ifdef SERVER
        LFPG_RPCServerHandler.Dispatch(this, sender, subId, ctx);
        #else
        LFPG_RPCClientHandler.Dispatch(this, subId, ctx);
        #endif
    }

    // =====================================
    // SERVER: send message to a specific player
    // Static so other files CAN call it if needed in the future.
    // =====================================
    static void LFPG_SendClientMsg(PlayerBase target, string msg)
    {
        if (!target) return;
        PlayerIdentity ident = target.GetIdentity();
        if (!ident) return;

        ScriptRPC rpc = new ScriptRPC();
        int subWriteId = LFPG_RPC_SubId.CLIENT_MSG;
        rpc.Write(subWriteId);
        rpc.Write(msg);
        rpc.Send(null, LFPG_RPC_CHANNEL, true, ident);
    }
};
