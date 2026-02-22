// =========================================================
// LF_PowerGrid - diagnostic helpers (4_World)
//
// ServerEcho: sends a message from CLIENT to SERVER via RPC
// so it appears in the server RPT log. Invaluable when you
// only have access to the dedicated server log.
// =========================================================

class LFPG_Diag
{
    // Throttle: avoid spamming RPC
    protected static int s_EchoCount = 0;
    protected static float s_LastEchoTime = 0;

    // Send a diagnostic string from client to server via RPC.
    // On server, just prints directly.
    static void ServerEcho(string msg)
    {
        if (!LFPG_DIAG_ENABLED)
            return;

        if (GetGame().IsDedicatedServer())
        {
            Print(LFPG_LOG_PREFIX + "[CLI-ECHO] " + msg);
            return;
        }

        // Also print locally for client RPT
        Print(LFPG_LOG_PREFIX + "[DIAG] " + msg);

        // Rate limit: max ~10 per second
        float now = GetGame().GetTickTime();
        float elapsed = now - s_LastEchoTime;
        if (elapsed < 0.1)
        {
            s_EchoCount = s_EchoCount + 1;
            if (s_EchoCount > 10)
                return;
        }
        else
        {
            s_EchoCount = 0;
            s_LastEchoTime = now;
        }

        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player)
            return;

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.DIAG_CLIENT_LOG);
        rpc.Write(msg);
        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);
    }
};
