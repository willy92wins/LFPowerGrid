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
    protected bool m_LFPG_SkipResetGUI = false;

    void LFPG_SetSkipResetGUI(bool skip)
    {
        m_LFPG_SkipResetGUI = skip;
    }
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
        MissionBaseWorld mission = MissionBaseWorld.Cast(GetGame().GetMission());
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

        ScriptRPC rpc = new ScriptRPC();
        int subWriteId = LFPG_RPC_SubId.CLIENT_MSG;
        rpc.Write(subWriteId);
        rpc.Write(msg);
        rpc.Send(target, LFPG_RPC_CHANNEL, true, null);
    }
};
