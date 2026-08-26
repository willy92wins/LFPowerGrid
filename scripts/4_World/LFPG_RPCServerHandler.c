// =========================================================
// LF_PowerGrid - Server RPC Handler (World-arena facade)
//
// Handler bodies are hosted by the mission layer; this facade only
// forwards the single dispatch seam so the World arena carries the
// entry point, not the ~3k lines of handlers.
// Mirrors the LFPG_ElecGraph factory idiom (LFPG_NetworkManager.c:559).
// =========================================================

class LFPG_RPCServerHandler
{
    static void Dispatch(PlayerBase player, PlayerIdentity sender, int subId, ParamsReadContext ctx)
    {
        MissionBaseWorld mw = MissionBaseWorld.Cast(g_Game.GetMission());
        if (mw)
        {
            mw.LFPG_DispatchServerRPC(player, sender, subId, ctx);
            return;
        }
        LFPG_Util.Error("[LFPG_RPCServerHandler] Mission dispatcher unavailable - server RPC dropped");
    }
};
