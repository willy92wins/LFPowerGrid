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
            HandleCameraListResponse(ctx);
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
        else if (subId == LFPG_RPC_SubId.SEARCHLIGHT_ENTER_CONFIRM)
        {
            HandleSearchlightEnterConfirm(ctx);
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

    static void HandleSearchlightEnterConfirm(ParamsReadContext ctx)
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
        if (ctrl)
        {
            ctrl.Enter(netLow, netHigh, yaw, pitch);
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

    static void HandleCameraListResponse(ParamsReadContext ctx)
    {
        int camCount = 0;
        if (!ctx.Read(camCount))
            return;

        if (camCount <= 0)
        {
            PlayerBase pLocal = PlayerBase.Cast(g_Game.GetPlayer());
            if (pLocal)
                pLocal.MessageStatus("[LFPG] No hay camaras activas conectadas.");
            return;
        }

        // v0.9.2 (Safety): Cap camCount against malformed/malicious RPC payloads.
        // Server sends at most LFPG_MONITOR_MAX_CAMERAS entries.
        if (camCount > LFPG_MONITOR_MAX_CAMERAS)
        {
            camCount = LFPG_MONITOR_MAX_CAMERAS;
        }

        ref array<ref LFPG_CameraListEntry> entries = new array<ref LFPG_CameraListEntry>;

        // Read per-camera data: 3 floats pos + 3 floats ori + 1 string label
        float readPx = 0.0;
        float readPy = 0.0;
        float readPz = 0.0;
        float readOx = 0.0;
        float readOy = 0.0;
        float readOz = 0.0;
        string readLabel = "";
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
            entries.Insert(entry);

            ri = ri + 1;
        }

        if (entries.Count() == 0)
            return;

        LFPG_CameraViewport vp = LFPG_CameraViewport.Get();
        if (!vp)
            return;

        // v0.9.8: Guard de re-entrada. Si el viewport ya esta activo
        // (otro RPC response llego antes, o doble-click rapido), ignorar.
        if (vp.IsActive())
        {
            LFPG_Util.Warn("[CameraListResponse] viewport already active — ignoring");
            return;
        }

        // v0.9.8: Guard de jugador valido. Si el jugador murio o esta
        // inconsciente entre el RPC request y la respuesta, no activar.
        PlayerBase pGuard = PlayerBase.Cast(g_Game.GetPlayer());
        if (!pGuard || !pGuard.IsAlive() || pGuard.IsUnconscious())
        {
            LFPG_Util.Warn("[CameraListResponse] player dead/unconscious — ignoring");
            return;
        }

        vp.EnterFromList(entries);
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
    }

    static void HandleInspectResponse(ParamsReadContext ctx)
    {
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

        if (wireCount < 0)
        {
            wireCount = 0;
        }
        if (wireCount > LFPG_MAX_WIRES_PER_DEVICE)
        {
            wireCount = LFPG_MAX_WIRES_PER_DEVICE;
        }

        ref array<ref LFPG_InspectWireEntry> wires = new array<ref LFPG_InspectWireEntry>;
        int ri;
        for (ri = 0; ri < wireCount; ri = ri + 1)
        {
            int dir;
            string localPort;
            string remoteId;
            string remotePort;
            string remoteType;
            float allocPower;
            int edgeState;

            if (!ctx.Read(dir)) break;
            if (!ctx.Read(localPort)) break;
            if (!ctx.Read(remoteId)) break;
            if (!ctx.Read(remotePort)) break;
            if (!ctx.Read(remoteType)) break;
            if (!ctx.Read(allocPower)) break;
            if (!ctx.Read(edgeState)) break;

            LFPG_InspectWireEntry entry = new LFPG_InspectWireEntry();
            entry.m_Direction = dir;
            entry.m_LocalPort = localPort;
            entry.m_RemoteDeviceId = remoteId;
            entry.m_RemotePort = remotePort;
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
        string containerName = "";
        if (!ctx.Read(containerName))
            return;

        if (!g_Game)
            return;

        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player)
            return;

        if (containerName != "")
        {
            string okMsg = "[LFPG] Sorter linked: ";
            okMsg = okMsg + containerName;
            player.MessageStatus(okMsg);
        }
        else
        {
            string failMsg = "[LFPG] No container found nearby";
            player.MessageStatus(failMsg);
        }
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

        // A3: btcOnInv — optional read (retrocompatible)
        int btcOnInv = 0;
        ctx.Read(btcOnInv);

        // 24h price change — optional read (retrocompatible)
        float priceChange24h = 0.0;
        ctx.Read(priceChange24h);

        LFPG_BTCAtmClientData.OnOpenResponse(price, stock, balance, cashOnInv, withdrawOnly, btcOnInv, priceChange24h);

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

        // A3: btcOnInv — optional read (retrocompatible)
        int btcOnInv = 0;
        ctx.Read(btcOnInv);

        LFPG_BTCAtmClientData.OnTxResult(txType, errCode, newStock, newBalance, btcMoved, eurAmount, cashOnInv, btcOnInv);

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

};
