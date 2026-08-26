#ifndef SERVER
// Client-only compilation boundary
// =========================================================
// LF_PowerGrid - Client RPC Handler (v5.0 Refactor)
//
// All client-side RPC handlers extracted from modded PlayerBase
// into static methods. Part of the crash fix refactor.
// =========================================================

class LFPG_RPCClientHandler
{
    // =========================================================
    // Dispatch: routes subId to individual client handlers.
    // Called from modded PlayerBase.OnRPC inside #else (client).
    // =========================================================
    static void Dispatch(PlayerBase player, int subId, ParamsReadContext ctx)
    {
        if (subId == LFPG_RPC_SubId.SYNC_OWNER_WIRES)
        {
            HandleSyncOwnerWires(ctx);
        }
        else if (subId == LFPG_RPC_SubId.SYNC_OWNER_WIRES_V2)
        {
            HandleSyncOwnerWiresV2(ctx);
        }
        else if (subId == LFPG_RPC_SubId.SYNC_OWNER_WIRES_DELTA)
        {
            HandleSyncOwnerWiresDelta(ctx);
        }
        else if (subId == LFPG_RPC_SubId.CLIENT_MSG)
        {
            HandleClientMsg(ctx);
        }
        else if (subId == LFPG_RPC_SubId.INSPECT_RESPONSE)
        {
            HandleInspectResponse(ctx);
        }
        else if (subId == LFPG_RPC_SubId.CAMERA_LIST_RESPONSE)
        {
            HandleCameraListResponse(player, ctx);
        }
        else if (subId == LFPG_RPC_SubId.CCTV_EXIT_CONFIRM)
        {
            HandleCCTVExitConfirm();
        }
        else if (subId == LFPG_RPC_SubId.SORTER_CONFIG_RESPONSE)
        {
            HandleSorterConfigResponse(ctx);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_SAVE_ACK)
        {
            HandleSorterSaveAck(ctx);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_RESYNC_ACK)
        {
            HandleSorterResyncAck(ctx);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_PREVIEW_RESPONSE)
        {
            HandleSorterPreviewResponse(ctx);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_SORT_ACK)
        {
            HandleSorterSortAck(player, ctx);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_CARGO_REFRESH)
        {
            HandleSorterCargoRefresh(player);
        }
        // ---- V4 TEST sorter dispatch (Sprint 0, 2026-04-26) ----
        else if (subId == LFPG_RPC_SubId.SORTER_TEST_CONFIG_RESPONSE)
        {
            HandleSorterTestConfigResponse(ctx);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_TEST_SAVE_ACK)
        {
            HandleSorterTestSaveAck(ctx);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_TEST_RESYNC_ACK)
        {
            HandleSorterTestResyncAck(ctx);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_TEST_PREVIEW_RESPONSE)
        {
            HandleSorterTestPreviewResponse(ctx);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_TEST_SORT_ACK)
        {
            HandleSorterTestSortAck(player, ctx);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_TEST_CARGO_REFRESH)
        {
            HandleSorterTestCargoRefresh(player);
        }
        else if (subId == LFPG_RPC_SubId.SEARCHLIGHT_ENTER_CONFIRM)
        {
            HandleSearchlightEnterConfirm(player, ctx);
        }
        else if (subId == LFPG_RPC_SubId.SEARCHLIGHT_EXIT_CONFIRM)
        {
            HandleSearchlightExitConfirm();
        }
        else if (subId == LFPG_RPC_SubId.BTC_OPEN_RESPONSE)
        {
            HandleBTCOpenResponse(ctx);
        }
        else if (subId == LFPG_RPC_SubId.BTC_TX_RESULT)
        {
            HandleBTCTxResult(ctx);
        }
        else if (subId == LFPG_RPC_SubId.BTC_PRICE_UNAVAILABLE)
        {
            HandleBTCPriceUnavailable();
        }
        else if (subId == LFPG_RPC_SubId.SYNC_SERVER_SETTINGS)
        {
            HandleSyncServerSettings(ctx);
        }
    }

    // =========================================================
    // Individual client handlers
    // =========================================================

    static void HandleSyncServerSettings(ParamsReadContext ctx)
    {
        bool hideFlag = false;
        if (!ctx.Read(hideFlag)) return;

        LFPG_CableRenderer.SetServerHideCablesNoReel(hideFlag);

        string logMsg = "[LFPG] Server settings received: HideCablesWithoutReel=";
        logMsg = logMsg + hideFlag.ToString();
        Print(logMsg);
    }

    static void HandleClientMsg(ParamsReadContext ctx)
    {
        string msg;
        if (!ctx.Read(msg)) return;

        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (player)
        {
            player.MessageStatus("[LFPG] " + msg);
        }
    }

    static void HandleCCTVExitConfirm()
    {
        LFPG_CameraViewport vp = LFPG_CameraViewport.Get();
        if (vp)
        {
            vp.DoExitCleanup();
        }
    }

    static void HandleSearchlightEnterConfirm(PlayerBase player, ParamsReadContext ctx)
    {
        int netLow = 0;
        int netHigh = 0;
        float yaw = 0.0;
        float pitch = 0.0;

        if (!ctx.Read(netLow))
            return;
        if (!ctx.Read(netHigh))
            return;
        if (!ctx.Read(yaw))
            return;
        if (!ctx.Read(pitch))
            return;

        LFPG_SearchlightController ctrl = LFPG_SearchlightController.Get();
        if (!ctrl)
            return;

        int enterResult = ctrl.Enter(netLow, netHigh, yaw, pitch);
        // Do not collapse this to !enterOk: an active other target is not a resolution failure.
        if (enterResult == LFPG_SEARCHLIGHT_ENTER_TARGET_UNRESOLVED && player)
        {
            // Existing compensation RPC; no protocol or payload change.
            ScriptRPC exitRpc = new ScriptRPC();
            exitRpc.Write((int)LFPG_RPC_SubId.SEARCHLIGHT_EXIT_REQUEST);
            exitRpc.Write(netLow);
            exitRpc.Write(netHigh);
            exitRpc.Send(player, LFPG_RPC_CHANNEL, true, null);
        }
    }

    static void HandleSearchlightExitConfirm()
    {
        LFPG_SearchlightController ctrl = LFPG_SearchlightController.Get();
        if (ctrl)
        {
            ctrl.DoCleanup();
        }
    }

    static void HandleCameraListResponse(PlayerBase player, ParamsReadContext ctx)
    {
        LFPG_CameraViewport vp = LFPG_CameraViewport.Get();
        if (!vp)
            return;

        int camCount = 0;
        if (!ctx.Read(camCount))
        {
            vp.EnterFromList(player, null);
            return;
        }

        if (camCount <= 0)
        {
            if (player)
                player.MessageStatus("[LFPG] No hay camaras activas conectadas.");
            vp.EnterFromList(player, null);
            return;
        }

        // v0.9.2 (Safety): Cap camCount against malformed/malicious RPC payloads.
        // Server sends at most LFPG_MONITOR_MAX_CAMERAS entries.
        if (camCount > LFPG_MONITOR_MAX_CAMERAS)
        {
            camCount = LFPG_MONITOR_MAX_CAMERAS;
        }

        ref array<ref LFPG_CameraListEntry> entries = new array<ref LFPG_CameraListEntry>;

        // Read per-camera data: position, base orientation, label, entity
        // NetworkID, and the entity-owned persisted PTZ offsets.
        float readPx = 0.0;
        float readPy = 0.0;
        float readPz = 0.0;
        float readOx = 0.0;
        float readOy = 0.0;
        float readOz = 0.0;
        string readLabel = "";
        int readNetLow = 0;
        int readNetHigh = 0;
        float readYawOffset = 0.0;
        float readPitchOffset = 0.0;
        vector assembledPos = "0 0 0";
        vector assembledOri = "0 0 0";
        int ri = 0;

        while (ri < camCount)
        {
            if (!ctx.Read(readPx))
                break;
            if (!ctx.Read(readPy))
                break;
            if (!ctx.Read(readPz))
                break;
            if (!ctx.Read(readOx))
                break;
            if (!ctx.Read(readOy))
                break;
            if (!ctx.Read(readOz))
                break;
            if (!ctx.Read(readLabel))
                break;
            if (!ctx.Read(readNetLow))
                break;
            if (!ctx.Read(readNetHigh))
                break;
            if (!ctx.Read(readYawOffset))
                break;
            if (!ctx.Read(readPitchOffset))
                break;

            assembledPos[0] = readPx;
            assembledPos[1] = readPy;
            assembledPos[2] = readPz;
            assembledOri[0] = readOx;
            assembledOri[1] = readOy;
            assembledOri[2] = readOz;

            ref LFPG_CameraListEntry entry = new LFPG_CameraListEntry();
            entry.m_Pos   = assembledPos;
            entry.m_Ori   = assembledOri;
            entry.m_Label = readLabel;
            entry.m_NetLow = readNetLow;
            entry.m_NetHigh = readNetHigh;
            entry.m_YawOffset = readYawOffset;
            entry.m_PitchOffset = readPitchOffset;
            entries.Insert(entry);

            ri = ri + 1;
        }

        // The viewport owns all enter-abort and vital-state transitions.
        vp.EnterFromList(player, entries);
    }

    static void HandleSyncOwnerWires(ParamsReadContext ctx)
    {
        string ownerDeviceId;
        int low = 0;
        int high = 0;
        string json;

        if (!ctx.Read(ownerDeviceId)) { LFPG_Util.Warn("[CLIENT] SyncOwnerWires: read ownerDeviceId FAIL"); return; }
        if (!ctx.Read(low)) { LFPG_Util.Warn("[CLIENT] SyncOwnerWires: read low FAIL"); return; }
        if (!ctx.Read(high)) { LFPG_Util.Warn("[CLIENT] SyncOwnerWires: read high FAIL"); return; }
        if (!ctx.Read(json)) { LFPG_Util.Warn("[CLIENT] SyncOwnerWires: read json FAIL"); return; }

        LFPG_Util.Info("[CLIENT] SyncOwnerWires: owner=" + ownerDeviceId + " net=" + low.ToString() + ":" + high.ToString() + " jsonLen=" + json.Length().ToString());
        LFPG_Diag.ServerEcho("[CLIENT] SyncOwnerWires owner=" + ownerDeviceId + " jsonLen=" + json.Length().ToString());

        LFPG_CableRenderer r = LFPG_CableRenderer.Get();
        if (r)
        {
            r.UpsertOwnerBlob(ownerDeviceId, low, high, json);
        }
        else
        {
            LFPG_Util.Warn("[CLIENT] SyncOwnerWires: CableRenderer NULL (server?)");
        }

        if (LFPG_PERFDIAG_ENABLED)
        {
            string perfSnapshot = "LFPG_PERFDIAG t=";
            perfSnapshot = perfSnapshot + g_Game.GetTickTime().ToString();
            perfSnapshot = perfSnapshot + " deviceId=";
            perfSnapshot = perfSnapshot + ownerDeviceId;
            perfSnapshot = perfSnapshot + " snapshot_receive jsonLen=";
            perfSnapshot = perfSnapshot + json.Length().ToString();
            Print(perfSnapshot);
        }
    }

    static void HandleSyncOwnerWiresV2(ParamsReadContext ctx)
    {
        string ownerDeviceId = "";
        int low = 0;
        int high = 0;
        string json = "";
        int generation = -1;

        if (!ctx.Read(ownerDeviceId))
            return;
        if (!ctx.Read(low))
            return;
        if (!ctx.Read(high))
            return;
        if (!ctx.Read(json))
            return;
        if (!ctx.Read(generation))
            return;

        string syncMsg = "[CLIENT] SyncOwnerWiresV2 owner=";
        syncMsg = syncMsg + ownerDeviceId;
        syncMsg = syncMsg + " generation=";
        syncMsg = syncMsg + generation.ToString();
        syncMsg = syncMsg + " jsonLen=";
        syncMsg = syncMsg + json.Length().ToString();
        LFPG_Util.Info(syncMsg);

        LFPG_CableRenderer renderer = LFPG_CableRenderer.Get();
        if (renderer)
        {
            renderer.UpsertOwnerBlobV2(ownerDeviceId, low, high, json, generation);
        }
        else
        {
            LFPG_Util.Warn("[CLIENT] SyncOwnerWiresV2: CableRenderer NULL (server?)");
        }

        if (LFPG_PERFDIAG_ENABLED)
        {
            string perfSnapshot = "LFPG_PERFDIAG t=";
            perfSnapshot = perfSnapshot + g_Game.GetTickTime().ToString();
            perfSnapshot = perfSnapshot + " deviceId=";
            perfSnapshot = perfSnapshot + ownerDeviceId;
            perfSnapshot = perfSnapshot + " snapshot_receive jsonLen=";
            perfSnapshot = perfSnapshot + json.Length().ToString();
            perfSnapshot = perfSnapshot + " generation=";
            perfSnapshot = perfSnapshot + generation.ToString();
            Print(perfSnapshot);
        }
    }

    static void HandleSyncOwnerWiresDelta(ParamsReadContext ctx)
    {
        string ownerDeviceId = "";
        int low = 0;
        int high = 0;
        int generation = -1;
        int entryCount = 0;

        if (!ctx.Read(ownerDeviceId))
            return;
        if (!ctx.Read(low))
            return;
        if (!ctx.Read(high))
            return;
        if (!ctx.Read(generation))
            return;
        if (!ctx.Read(entryCount))
            return;
        if (ownerDeviceId == "" || generation < 0)
            return;
        if (entryCount <= 0 || entryCount > LFPG_WIRE_DELTA_MAX_ENTRIES)
            return;

        ref array<int> operations = new array<int>;
        ref array<string> wireJsons = new array<string>;
        int i;
        for (i = 0; i < entryCount; i = i + 1)
        {
            int operation = 0;
            string wireJson = "";
            if (!ctx.Read(operation))
                return;
            if (!ctx.Read(wireJson))
                return;
            operations.Insert(operation);
            wireJsons.Insert(wireJson);
        }

        LFPG_CableRenderer renderer = LFPG_CableRenderer.Get();
        if (renderer)
        {
            renderer.ApplyOwnerDelta(ownerDeviceId, low, high, generation, operations, wireJsons);
        }
    }

    static void HandleInspectResponse(ParamsReadContext ctx)
    {
        int schemaVersion;
        if (!ctx.Read(schemaVersion))
        {
            LFPG_Util.Warn("[CLIENT] InspectResponse: read schemaVersion FAIL");
            return;
        }

        if (schemaVersion != LFPG_InspectWireEntry.SCHEMA_VERSION)
        {
            LFPG_Util.Warn("[CLIENT] InspectResponse: unsupported schema=" + schemaVersion.ToString());
            return;
        }

        string deviceId;
        if (!ctx.Read(deviceId))
        {
            LFPG_Util.Warn("[CLIENT] InspectResponse: read deviceId FAIL");
            return;
        }

        int wireCount;
        if (!ctx.Read(wireCount))
        {
            LFPG_Util.Warn("[CLIENT] InspectResponse: read wireCount FAIL");
            return;
        }

        if (wireCount < 0 || wireCount > LFPG_MAX_WIRES_PER_DEVICE)
        {
            LFPG_Util.Warn("[CLIENT] InspectResponse: invalid wireCount=" + wireCount.ToString());
            return;
        }

        ref array<ref LFPG_InspectWireEntry> wires = new array<ref LFPG_InspectWireEntry>;
        int ri;
        for (ri = 0; ri < wireCount; ri = ri + 1)
        {
            int dir;
            string localPort;
            string remoteType;
            float allocPower;
            int edgeState;

            // A truncated wire invalidates the response; return prevents partial UI state.
            if (!ctx.Read(dir))
            {
                LFPG_Util.Warn("[CLIENT] InspectResponse: read direction FAIL");
                return;
            }
            if (!ctx.Read(localPort))
            {
                LFPG_Util.Warn("[CLIENT] InspectResponse: read localPort FAIL");
                return;
            }
            if (!ctx.Read(remoteType))
            {
                LFPG_Util.Warn("[CLIENT] InspectResponse: read remoteTypeName FAIL");
                return;
            }
            if (!ctx.Read(allocPower))
            {
                LFPG_Util.Warn("[CLIENT] InspectResponse: read allocatedPower FAIL");
                return;
            }
            if (!ctx.Read(edgeState))
            {
                LFPG_Util.Warn("[CLIENT] InspectResponse: read edgeState FAIL");
                return;
            }

            LFPG_InspectWireEntry entry = new LFPG_InspectWireEntry();
            entry.m_Direction = dir;
            entry.m_LocalPort = localPort;
            entry.m_RemoteTypeName = remoteType;
            entry.m_AllocatedPower = allocPower;
            entry.m_EdgeState = edgeState;

            wires.Insert(entry);
        }

        LFPG_DeviceInspector.OnInspectResponse(deviceId, wires);
    }

    static void HandleSorterConfigResponse(ParamsReadContext ctx)
    {
        int netLow = 0;
        int netHigh = 0;
        string filterJSON = "";
        string containerName = "";
        string destName0 = "";
        string destName1 = "";
        string destName2 = "";
        string destName3 = "";
        string destName4 = "";
        string destName5 = "";

        if (!ctx.Read(netLow))
        {
            LFPG_Util.Warn("[SorterConfigResponse] read netLow FAIL");
            return;
        }
        if (!ctx.Read(netHigh))
        {
            LFPG_Util.Warn("[SorterConfigResponse] read netHigh FAIL");
            return;
        }
        if (!ctx.Read(filterJSON))
        {
            LFPG_Util.Warn("[SorterConfigResponse] read filterJSON FAIL");
            return;
        }
        if (!ctx.Read(containerName))
        {
            LFPG_Util.Warn("[SorterConfigResponse] read containerName FAIL");
            return;
        }
        if (!ctx.Read(destName0))
        {
            LFPG_Util.Warn("[SorterConfigResponse] read destName0 FAIL");
            return;
        }
        if (!ctx.Read(destName1))
        {
            LFPG_Util.Warn("[SorterConfigResponse] read destName1 FAIL");
            return;
        }
        if (!ctx.Read(destName2))
        {
            LFPG_Util.Warn("[SorterConfigResponse] read destName2 FAIL");
            return;
        }
        if (!ctx.Read(destName3))
        {
            LFPG_Util.Warn("[SorterConfigResponse] read destName3 FAIL");
            return;
        }
        if (!ctx.Read(destName4))
        {
            LFPG_Util.Warn("[SorterConfigResponse] read destName4 FAIL");
            return;
        }
        if (!ctx.Read(destName5))
        {
            LFPG_Util.Warn("[SorterConfigResponse] read destName5 FAIL");
            return;
        }

        // Open the Sorter UI with full data
        LFPG_SorterView.Open(filterJSON, containerName, destName0, destName1, destName2, destName3, destName4, destName5, netLow, netHigh);

        string logMsg = "[SorterConfigResponse] Opened UI, container=" + containerName;
        LFPG_Util.Info(logMsg);
    }

    static void HandleSorterSaveAck(ParamsReadContext ctx)
    {
        bool success = false;
        if (!ctx.Read(success))
            return;

        LFPG_SorterView.OnSaveAck(success);
    }

    static void HandleSorterSortAck(PlayerBase player, ParamsReadContext ctx)
    {
        bool success = false;
        int movedCount = 0;
        if (!ctx.Read(success))
            return;
        if (!ctx.Read(movedCount))
            return;

        LFPG_SorterView.OnSortAck(success, movedCount);

        // v3.2: Force client inventory UI refresh.
        // LocationSyncMoveEntity on server moves items but client
        // may not refresh cargo view until relog. UpdateInventoryMenu
        // is vanilla EntityAI method called after every inventory op.
        if (success && movedCount > 0)
        {
            player.UpdateInventoryMenu();

            // v5.0: Signal 5_Mission to refresh vicinity containers
            LFPG_CargoRefreshSignal.Request();
        }
    }

    static void HandleSorterCargoRefresh(PlayerBase player)
    {
        player.UpdateInventoryMenu();

        // v5.0: Signal 5_Mission to refresh vicinity containers
        LFPG_CargoRefreshSignal.Request();
    }

    static void HandleSorterResyncAck(ParamsReadContext ctx)
    {
        int ackStatus = LFPG_SORTER_ACK_NONE;
        string containerName = "";
        if (!ctx.Read(ackStatus))
            return;
        if (!ctx.Read(containerName))
            return;

        if (!g_Game)
            return;

        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player)
            return;

        string msg = "";
        if (ackStatus == LFPG_SORTER_ACK_REPLACED)
        {
            msg = "[LFPG] Sorter linked: ";
            msg = msg + containerName;
        }
        else if (ackStatus == LFPG_SORTER_ACK_KEPT_OLD)
        {
            msg = "[LFPG] Sorter already linked: ";
            msg = msg + containerName;
        }
        else
        {
            if (containerName != "")
            {
                msg = "[LFPG] No new container found; kept: ";
                msg = msg + containerName;
            }
            else
            {
                msg = "[LFPG] No container found nearby";
            }
        }
        player.MessageStatus(msg);
    }

    static void HandleSorterPreviewResponse(ParamsReadContext ctx)
    {
        int outputIdx = 0;
        int totalMatched = 0;
        int sentCount = 0;

        if (!ctx.Read(outputIdx))
        {
            string errOut = "[SorterPreviewResponse] read outputIdx FAIL";
            LFPG_Util.Warn(errOut);
            return;
        }
        if (!ctx.Read(totalMatched))
        {
            string errTotal = "[SorterPreviewResponse] read totalMatched FAIL";
            LFPG_Util.Warn(errTotal);
            return;
        }
        if (!ctx.Read(sentCount))
        {
            string errSent = "[SorterPreviewResponse] read sentCount FAIL";
            LFPG_Util.Warn(errSent);
            return;
        }

        // Sanity cap
        if (sentCount > LFPG_SORTER_PREVIEW_CAP)
        {
            sentCount = LFPG_SORTER_PREVIEW_CAP;
        }

        array<string> names = new array<string>;
        array<string> cats = new array<string>;
        // v4.3: Changed from array<int> to string (formatted "WxH" / "WxH xQ")
        array<string> infos = new array<string>;

        int si = 0;
        string itemName = "";
        string itemCat = "";
        string itemInfo = "";
        bool readOk = true;

        for (si = 0; si < sentCount; si = si + 1)
        {
            if (!ctx.Read(itemName))
            {
                readOk = false;
                break;
            }
            if (!ctx.Read(itemCat))
            {
                readOk = false;
                break;
            }
            if (!ctx.Read(itemInfo))
            {
                readOk = false;
                break;
            }
            names.Insert(itemName);
            cats.Insert(itemCat);
            infos.Insert(itemInfo);
        }

        if (!readOk)
        {
            string errRead = "[SorterPreviewResponse] item read FAIL at index ";
            errRead = errRead + si.ToString();
            LFPG_Util.Warn(errRead);
            return;
        }

        LFPG_SorterView.OnPreviewData(outputIdx, totalMatched, names, cats, infos);

        string logMsg = "[SorterPreviewResponse] output=";
        logMsg = logMsg + outputIdx.ToString();
        logMsg = logMsg + " total=";
        logMsg = logMsg + totalMatched.ToString();
        logMsg = logMsg + " received=";
        logMsg = logMsg + sentCount.ToString();
        LFPG_Util.Info(logMsg);
    }

    static void HandleBTCOpenResponse(ParamsReadContext ctx)
    {
        float price = 0.0;
        int stock = 0;
        int balance = 0;
        int cashOnInv = 0;
        bool withdrawOnly = false;

        if (!ctx.Read(price))
            return;
        if (!ctx.Read(stock))
            return;
        if (!ctx.Read(balance))
            return;
        if (!ctx.Read(cashOnInv))
            return;
        if (!ctx.Read(withdrawOnly))
            return;

        // Optional legacy fields remain readable from older servers.
        int btcOnInv = 0;
        ctx.Read(btcOnInv);

        float priceChange24h = 0.0;
        ctx.Read(priceChange24h);

        // Protocol fields are append-only. Their absence identifies an old server.
        int protocolVersion = 0;
        int serverSessionLow = 0;
        int serverSessionHigh = 0;
        int highWatermark = 0;
        if (ctx.Read(protocolVersion))
        {
            if (!ctx.Read(serverSessionLow))
                protocolVersion = 0;
            if (!ctx.Read(serverSessionHigh))
                protocolVersion = 0;
            if (!ctx.Read(highWatermark))
                protocolVersion = 0;
        }

        LFPG_BTCAtmClientData.OnOpenResponse(price, stock, balance, cashOnInv, withdrawOnly, btcOnInv, priceChange24h, protocolVersion, serverSessionLow, serverSessionHigh, highWatermark);

        string logResp = "[BTCOpenResponse] price=";
        logResp = logResp + price.ToString();
        logResp = logResp + " stock=";
        logResp = logResp + stock.ToString();
        logResp = logResp + " bal=";
        logResp = logResp + balance.ToString();
        logResp = logResp + " cash=";
        logResp = logResp + cashOnInv.ToString();
        logResp = logResp + " wo=";
        logResp = logResp + withdrawOnly.ToString();
        logResp = logResp + " protocol=";
        logResp = logResp + protocolVersion.ToString();
        LFPG_Util.Info(logResp);

        LFPG_BTCAtmView.Open();
    }
    static void HandleBTCTxResult(ParamsReadContext ctx)
    {
        int txType = 0;
        int errCode = 0;
        int newStock = 0;
        int newBalance = 0;
        int btcMoved = 0;
        float eurAmount = 0.0;
        int cashOnInv = 0;

        if (!ctx.Read(txType))
            return;
        if (!ctx.Read(errCode))
            return;
        if (!ctx.Read(newStock))
            return;
        if (!ctx.Read(newBalance))
            return;
        if (!ctx.Read(btcMoved))
            return;
        if (!ctx.Read(eurAmount))
            return;
        if (!ctx.Read(cashOnInv))
            return;

        int btcOnInv = 0;
        ctx.Read(btcOnInv);

        int serverSessionLow = 0;
        int serverSessionHigh = 0;
        int sequence = 0;
        if (!ctx.Read(serverSessionLow))
            return;
        if (!ctx.Read(serverSessionHigh))
            return;
        if (!ctx.Read(sequence))
            return;

        if (!LFPG_BTCAtmClientData.OnTxResult(txType, errCode, newStock, newBalance, btcMoved, eurAmount, cashOnInv, btcOnInv, serverSessionLow, serverSessionHigh, sequence))
            return;

        string logTx = "[BTCTxResult] type=";
        logTx = logTx + txType.ToString();
        logTx = logTx + " err=";
        logTx = logTx + errCode.ToString();
        logTx = logTx + " stock=";
        logTx = logTx + newStock.ToString();
        logTx = logTx + " bal=";
        logTx = logTx + newBalance.ToString();
        logTx = logTx + " btc=";
        logTx = logTx + btcMoved.ToString();
        logTx = logTx + " cash=";
        logTx = logTx + cashOnInv.ToString();
        LFPG_Util.Info(logTx);

        LFPG_BTCAtmView.OnTxResult();
    }
    static void HandleBTCPriceUnavailable()
    {
        LFPG_BTCAtmClientData.OnPriceUnavailable();

        string logNA = "[BTCPriceUnavailable] price not available from API";
        LFPG_Util.Info(logNA);

        LFPG_BTCAtmView.OnPriceUnavailable();
    }

    // ============================================================
    // V4 TEST handlers (Sprint 0, 2026-04-26)
    // Clones of V3 sorter handlers, routed to LFPG_SorterView_TEST.
    // Generated programmatically Ã¢â‚¬â€ edits should propagate to V3
    // first, then re-run sprint0_patches.py to regenerate.
    // ============================================================
    static void HandleSorterTestConfigResponse(ParamsReadContext ctx)
    {
        int netLow = 0;
        int netHigh = 0;
        string filterJSON = "";
        string containerName = "";
        string destName0 = "";
        string destName1 = "";
        string destName2 = "";
        string destName3 = "";
        string destName4 = "";
        string destName5 = "";

        if (!ctx.Read(netLow))
        {
            LFPG_Util.Warn("[SorterConfigResponse] read netLow FAIL");
            return;
        }
        if (!ctx.Read(netHigh))
        {
            LFPG_Util.Warn("[SorterConfigResponse] read netHigh FAIL");
            return;
        }
        if (!ctx.Read(filterJSON))
        {
            LFPG_Util.Warn("[SorterConfigResponse] read filterJSON FAIL");
            return;
        }
        if (!ctx.Read(containerName))
        {
            LFPG_Util.Warn("[SorterConfigResponse] read containerName FAIL");
            return;
        }
        if (!ctx.Read(destName0))
        {
            LFPG_Util.Warn("[SorterConfigResponse] read destName0 FAIL");
            return;
        }
        if (!ctx.Read(destName1))
        {
            LFPG_Util.Warn("[SorterConfigResponse] read destName1 FAIL");
            return;
        }
        if (!ctx.Read(destName2))
        {
            LFPG_Util.Warn("[SorterConfigResponse] read destName2 FAIL");
            return;
        }
        if (!ctx.Read(destName3))
        {
            LFPG_Util.Warn("[SorterConfigResponse] read destName3 FAIL");
            return;
        }
        if (!ctx.Read(destName4))
        {
            LFPG_Util.Warn("[SorterConfigResponse] read destName4 FAIL");
            return;
        }
        if (!ctx.Read(destName5))
        {
            LFPG_Util.Warn("[SorterConfigResponse] read destName5 FAIL");
            return;
        }

        // Open the Sorter UI with full data
        LFPG_SorterView_TEST.Open(filterJSON, containerName, destName0, destName1, destName2, destName3, destName4, destName5, netLow, netHigh);

        string logMsg = "[SorterConfigResponse] Opened UI, container=" + containerName;
        LFPG_Util.Info(logMsg);
    }

    static void HandleSorterTestSaveAck(ParamsReadContext ctx)
    {
        bool success = false;
        if (!ctx.Read(success))
            return;

        LFPG_SorterView_TEST.OnSaveAck(success);
    }

    static void HandleSorterTestSortAck(PlayerBase player, ParamsReadContext ctx)
    {
        bool success = false;
        int movedCount = 0;
        if (!ctx.Read(success))
            return;
        if (!ctx.Read(movedCount))
            return;

        LFPG_SorterView_TEST.OnSortAck(success, movedCount);

        // v3.2: Force client inventory UI refresh.
        // LocationSyncMoveEntity on server moves items but client
        // may not refresh cargo view until relog. UpdateInventoryMenu
        // is vanilla EntityAI method called after every inventory op.
        if (success && movedCount > 0)
        {
            player.UpdateInventoryMenu();

            // v5.0: Signal 5_Mission to refresh vicinity containers
            LFPG_CargoRefreshSignal.Request();
        }
    }

    static void HandleSorterTestCargoRefresh(PlayerBase player)
    {
        player.UpdateInventoryMenu();

        // v5.0: Signal 5_Mission to refresh vicinity containers
        LFPG_CargoRefreshSignal.Request();
    }

    static void HandleSorterTestResyncAck(ParamsReadContext ctx)
    {
        int ackStatus = LFPG_SORTER_ACK_NONE;
        string containerName = "";
        if (!ctx.Read(ackStatus))
            return;
        if (!ctx.Read(containerName))
            return;

        if (!g_Game)
            return;

        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player)
            return;

        string msg = "";
        if (ackStatus == LFPG_SORTER_ACK_REPLACED)
        {
            msg = "[LFPG] Sorter linked: ";
            msg = msg + containerName;
        }
        else if (ackStatus == LFPG_SORTER_ACK_KEPT_OLD)
        {
            msg = "[LFPG] Sorter already linked: ";
            msg = msg + containerName;
        }
        else
        {
            if (containerName != "")
            {
                msg = "[LFPG] No new container found; kept: ";
                msg = msg + containerName;
            }
            else
            {
                msg = "[LFPG] No container found nearby";
            }
        }
        player.MessageStatus(msg);
    }

    static void HandleSorterTestPreviewResponse(ParamsReadContext ctx)
    {
        int outputIdx = 0;
        int totalMatched = 0;
        int sentCount = 0;

        if (!ctx.Read(outputIdx))
        {
            string errOut = "[SorterPreviewResponse] read outputIdx FAIL";
            LFPG_Util.Warn(errOut);
            return;
        }
        if (!ctx.Read(totalMatched))
        {
            string errTotal = "[SorterPreviewResponse] read totalMatched FAIL";
            LFPG_Util.Warn(errTotal);
            return;
        }
        if (!ctx.Read(sentCount))
        {
            string errSent = "[SorterPreviewResponse] read sentCount FAIL";
            LFPG_Util.Warn(errSent);
            return;
        }

        // Sanity cap
        if (sentCount > LFPG_SORTER_PREVIEW_CAP)
        {
            sentCount = LFPG_SORTER_PREVIEW_CAP;
        }

        array<string> names = new array<string>;
        array<string> cats = new array<string>;
        // v4.3: Changed from array<int> to string (formatted "WxH" / "WxH xQ")
        array<string> infos = new array<string>;

        int si = 0;
        string itemName = "";
        string itemCat = "";
        string itemInfo = "";
        bool readOk = true;

        for (si = 0; si < sentCount; si = si + 1)
        {
            if (!ctx.Read(itemName))
            {
                readOk = false;
                break;
            }
            if (!ctx.Read(itemCat))
            {
                readOk = false;
                break;
            }
            if (!ctx.Read(itemInfo))
            {
                readOk = false;
                break;
            }
            names.Insert(itemName);
            cats.Insert(itemCat);
            infos.Insert(itemInfo);
        }

        if (!readOk)
        {
            string errRead = "[SorterPreviewResponse] item read FAIL at index ";
            errRead = errRead + si.ToString();
            LFPG_Util.Warn(errRead);
            return;
        }

        LFPG_SorterView_TEST.OnPreviewData(outputIdx, totalMatched, names, cats, infos);

        string logMsg = "[SorterPreviewResponse] output=";
        logMsg = logMsg + outputIdx.ToString();
        logMsg = logMsg + " total=";
        logMsg = logMsg + totalMatched.ToString();
        logMsg = logMsg + " received=";
        logMsg = logMsg + sentCount.ToString();
        LFPG_Util.Info(logMsg);
    }
};
#endif
