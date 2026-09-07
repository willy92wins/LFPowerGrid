// =========================================================
// LF_PowerGrid - Native balance provider (World-arena facade)
//
// The implementation lives in the mission arena as
// LFPG_BalanceProvider_NativeImpl. This facade exists so the block published
// as "Static API for external mods" keeps resolving from scripts/4_World:
// a third-party mod compiled against those two statics would otherwise fail to
// compile, and its server would not start.
//
// Kept in the LFPG_BalanceProvider chain so an external cast still resolves.
// =========================================================

class LFPG_BalanceProvider_Native extends LFPG_BalanceProvider
{
    // ---- Static API for external mods ----
    //
    // Contract since the arena split: the ledger answers only while a
    // MissionServer exists. Pre-mission, menu and client-side calls return
    // 0 / false instead of loading the Native ledger from this arena.

    static int GetPlayerBalance(string uid)
    {
        if (!g_Game)
            return 0;
        MissionBaseWorld mw = MissionBaseWorld.Cast(g_Game.GetMission());
        if (!mw)
            return 0;
        return mw.LFPG_NativeGetPlayerBalance(uid);
    }

    static bool SetPlayerBalance(string uid, int balance)
    {
        if (!g_Game)
            return false;
        MissionBaseWorld mw = MissionBaseWorld.Cast(g_Game.GetMission());
        if (!mw)
            return false;
        return mw.LFPG_NativeSetPlayerBalance(uid, balance);
    }
};
