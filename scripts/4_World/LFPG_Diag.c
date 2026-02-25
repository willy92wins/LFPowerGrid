// =========================================================
// LF_PowerGrid - diagnostic helpers (4_World)
//
// ServerEcho: sends a message from CLIENT to SERVER via RPC
// so it appears in the server RPT log. Invaluable when you
// only have access to the dedicated server log.
//
// v0.7.26 (Audit 4): Added DumpGraphFull for admin diagnostics.
// v0.7.36: Pre-build strings for Enforce compat (no long
//          concat inside function params).
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
            string srvMsg = LFPG_LOG_PREFIX + "[CLI-ECHO] " + msg;
            Print(srvMsg);
            return;
        }

        // Also print locally for client RPT
        string cliMsg = LFPG_LOG_PREFIX + "[DIAG] " + msg;
        Print(cliMsg);

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

    // v0.7.26 (Audit 4): Full graph diagnostic dump.
    // Prints all nodes, edges, and load metrics to server RPT.
    // Called via admin command or debug. Server-only.
    static void DumpGraphFull()
    {
        #ifdef SERVER
        LFPG_NetworkManager mgr = LFPG_NetworkManager.Get();
        if (!mgr)
        {
            LFPG_Util.Warn("[Diag] DumpGraphFull: NetworkManager null");
            return;
        }

        LFPG_ElecGraph graph = mgr.GetGraph();
        if (!graph)
        {
            LFPG_Util.Warn("[Diag] DumpGraphFull: Graph null");
            return;
        }

        LFPG_Util.Info("=== LFPG GRAPH DUMP START ===");

        // v0.7.36: Pre-build summary string to avoid Enforce parser errors
        string sum = "[Diag] Nodes=" + graph.GetNodeCount().ToString();
        sum = sum + " Edges=" + graph.GetEdgeCount().ToString();
        sum = sum + " Components=" + graph.GetComponentCount().ToString();
        sum = sum + " DirtyQueue=" + graph.GetDirtyQueueSize().ToString();
        sum = sum + " Epoch=" + graph.GetCurrentEpoch().ToString();
        sum = sum + " LastRebuild=" + graph.GetLastRebuildMs().ToString() + "ms";
        sum = sum + " LastProcess=" + graph.GetLastProcessMs().ToString() + "ms";
        sum = sum + " OverloadedSources=" + graph.GetOverloadedSourceCount().ToString();
        LFPG_Util.Info(sum);

        // Enumerate registered devices for cross-reference
        ref array<EntityAI> allDevices = new array<EntityAI>;
        LFPG_DeviceRegistry.Get().GetAll(allDevices);

        string countMsg = "[Diag] DeviceRegistry count=" + allDevices.Count().ToString();
        LFPG_Util.Info(countMsg);

        int di;
        for (di = 0; di < allDevices.Count(); di = di + 1)
        {
            EntityAI dObj = allDevices[di];
            if (!dObj) continue;

            string dId = LFPG_DeviceAPI.GetDeviceId(dObj);
            if (dId == "") continue;

            string devType = "UNK";
            int dType = LFPG_DeviceAPI.GetDeviceType(dObj);
            if (dType == LFPG_DeviceType.SOURCE)
            {
                devType = "SRC";
            }
            else if (dType == LFPG_DeviceType.CONSUMER)
            {
                devType = "CON";
            }
            else if (dType == LFPG_DeviceType.PASSTHROUGH)
            {
                devType = "PAS";
            }

            LFPG_ElecNode node = graph.GetNode(dId);
            string nodeInfo = " (not in graph)";
            if (node)
            {
                nodeInfo = " powered=" + node.m_Powered.ToString();
                nodeInfo = nodeInfo + " outPow=" + node.m_OutputPower.ToString();
                nodeInfo = nodeInfo + " inPow=" + node.m_InputPower.ToString();
                nodeInfo = nodeInfo + " loadR=" + node.m_LoadRatio.ToString();
                nodeInfo = nodeInfo + " comp=" + node.m_ComponentId.ToString();
            }

            string devMsg = "[Diag] " + devType + " id=" + dId;
            devMsg = devMsg + " type=" + dObj.GetType();
            devMsg = devMsg + " pos=" + dObj.GetPosition().ToString();
            devMsg = devMsg + nodeInfo;
            LFPG_Util.Info(devMsg);
        }

        LFPG_Util.Info("=== LFPG GRAPH DUMP END ===");
        #endif
    }
};
