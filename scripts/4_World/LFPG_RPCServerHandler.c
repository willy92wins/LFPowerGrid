// =========================================================
// LF_PowerGrid - Server RPC Handler (v5.0 Refactor)
//
// All server-side RPC handlers extracted from modded PlayerBase
// into static methods. Reduces PlayerBase method table to prevent
// Enforce VM overflow crash on 57+ mod servers.
//
// All methods are static. First parameter is always PlayerBase player
// (was 'this' in the original modded class).
// =========================================================

class LFPG_RPCServerHandler
{
    protected static int s_PerfDiagDeviceSyncBatchCount;
    protected static int s_PerfDiagPreviewResponseCount;
    // =========================================================
    // Dispatch: routes subId to individual server handlers.
    // Called from modded PlayerBase.OnRPC inside #ifdef SERVER.
    //
    // PR-C 2026-05-26 â€” A1 RPC sender authority fix.
    // `player` arrives bound to whatever PlayerBase the sender's RPC was
    // addressed to. A malicious client can choose another player's body
    // as target (rpc.Send(victimPB, ...)), so handlers downstream were
    // executing with the victim's GetPosition / GetInventory / balance.
    // Resolve the real owning player from the sender identity and rebind
    // `player` to it before any handler runs. Policy: fail-open with
    // rate-limited warn (matches v2 remediation spec) so legitimate
    // engine flows that route through a foreign target are not killed â€”
    // we only drop if the sender cannot be resolved to any PlayerBase.
    // =========================================================
    static void Dispatch(PlayerBase player, PlayerIdentity sender, int subId, ParamsReadContext ctx)
    {
        if (!sender)
            return;

        // Classify inbound before any ctx.Read. Missing classification = deny.
        if (LFPG_RPCGuard.PolicyForSubId(subId) == 0)
        {
            LFPG_Util.RateLimitedWarn(sender, "rpc_guard_unknown_subid", "[LFPG_RPCGuard] Dispatch denied: unknown inbound subId");
            return;
        }

        // CCTV exit must remain routable while SelectPlayer(null) makes GetPlayer unavailable.
        if (subId == LFPG_RPC_SubId.CCTV_EXIT_REQUEST)
        {
            HandleCCTVExitRequest(player, sender);
            return;
        }

        PlayerBase realPlayer = PlayerBase.Cast(sender.GetPlayer());
        if (!realPlayer)
        {
            // Replay-only: do not exempt REQUEST_CAMERA_LIST from A1 rebind.
            // The full handler authorizes by player distance; a client-chosen
            // player would reopen the PR-C target hole. This path has no PlayerBase.
            if (subId == LFPG_RPC_SubId.REQUEST_CAMERA_LIST)
            {
                HandleCameraListReplayOnly(sender, ctx);
                return;
            }
            string nullMsg = "[LFPG_RPC] Dispatch: sender.GetPlayer() null, dropping subId=" + subId.ToString() + " sender=" + sender.GetPlainId();
            LFPG_Util.RateLimitedWarn(sender, "rpc_null_player", nullMsg);
            return;
        }
        if (player != realPlayer)
        {
            string mismatchMsg = "[LFPG_RPC] target mismatch from " + sender.GetPlainId() + " subId=" + subId.ToString() + " â€” rebinding to realPlayer";
            LFPG_Util.RateLimitedWarn(sender, "rpc_target_mismatch", mismatchMsg);
            player = realPlayer;
        }

        if (subId == LFPG_RPC_SubId.FINISH_WIRING)
        {
            HandleFinishWiring(player, sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.CUT_WIRES)
        {
            HandleCutWires(player, sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.CUT_PORT)
        {
            HandleCutPort(player, sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.REQUEST_FULL_SYNC)
        {
            HandleRequestFullSync(player, sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.DIAG_CLIENT_LOG)
        {
            HandleDiagClientLog(player, sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.REQUEST_DEVICE_SYNC)
        {
            HandleRequestDeviceSync(player, sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.REQUEST_DEVICE_SYNC_BATCH)
        {
            HandleRequestDeviceSyncBatch(player, sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.INSPECT_DEVICE)
        {
            HandleInspectDevice(player, sender, ctx, LFPG_RPCGuard.POLICY_INSPECT_READ);
        }
        else if (subId == LFPG_RPC_SubId.CAMERA_CYCLE)
        {
            HandleCameraLink(player, sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.CAMERA_UNLINK)
        {
            HandleCameraUnlink(player, sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.REQUEST_CAMERA_LIST)
        {
            HandleRequestCameraList(player, sender, ctx);
        }

        else if (subId == LFPG_RPC_SubId.SORTER_CONFIG_REQUEST)
        {
            int srvCfgRespId = LFPG_RPC_SubId.SORTER_CONFIG_RESPONSE;
            HandleSorterConfigRequest(player, sender, ctx, srvCfgRespId);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_TEST_CONFIG_REQUEST)
        {
            // Sprint 0 (2026-04-26): V4 routes to same handler with V4 response SubId
            int srvCfgRespIdT = LFPG_RPC_SubId.SORTER_TEST_CONFIG_RESPONSE;
            HandleSorterConfigRequest(player, sender, ctx, srvCfgRespIdT);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_CONFIG_SAVE)
        {
            int srvSaveAckId = LFPG_RPC_SubId.SORTER_SAVE_ACK;
            HandleSorterConfigSave(player, sender, ctx, srvSaveAckId);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_TEST_CONFIG_SAVE)
        {
            int srvSaveAckIdT = LFPG_RPC_SubId.SORTER_TEST_SAVE_ACK;
            HandleSorterConfigSave(player, sender, ctx, srvSaveAckIdT);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_REQUEST_SORT)
        {
            int srvSortAckId = LFPG_RPC_SubId.SORTER_SORT_ACK;
            HandleSorterRequestSort(player, sender, ctx, srvSortAckId);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_TEST_REQUEST_SORT)
        {
            int srvSortAckIdT = LFPG_RPC_SubId.SORTER_TEST_SORT_ACK;
            HandleSorterRequestSort(player, sender, ctx, srvSortAckIdT);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_RESYNC)
        {
            int srvResyncAckId = LFPG_RPC_SubId.SORTER_RESYNC_ACK;
            HandleSorterResync(player, sender, ctx, srvResyncAckId);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_TEST_RESYNC)
        {
            int srvResyncAckIdT = LFPG_RPC_SubId.SORTER_TEST_RESYNC_ACK;
            HandleSorterResync(player, sender, ctx, srvResyncAckIdT);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_PREVIEW_REQUEST)
        {
            int srvPrevRespId = LFPG_RPC_SubId.SORTER_PREVIEW_RESPONSE;
            HandleSorterPreviewRequest(player, sender, ctx, srvPrevRespId);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_TEST_PREVIEW_REQUEST)
        {
            int srvPrevRespIdT = LFPG_RPC_SubId.SORTER_TEST_PREVIEW_RESPONSE;
            HandleSorterPreviewRequest(player, sender, ctx, srvPrevRespIdT);
        }
        else if (subId == LFPG_RPC_SubId.SEARCHLIGHT_AIM)
        {
            HandleSearchlightAim(player, sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.SEARCHLIGHT_ENTER)
        {
            HandleSearchlightEnter(player, sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.SEARCHLIGHT_EXIT_REQUEST)
        {
            HandleSearchlightExit(player, sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.SEARCHLIGHT_EXIT_V2)
        {
            HandleSearchlightExitV2(player, sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.BTC_OPEN_REQUEST)
        {
            LFPG_BTCHelper.HandleBTCOpenRequest(player, sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.BTC_BUY)
        {
            LFPG_BTCHelper.HandleBTCBuy(player, sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.BTC_SELL)
        {
            LFPG_BTCHelper.HandleBTCSell(player, sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.BTC_WITHDRAW)
        {
            LFPG_BTCHelper.HandleBTCWithdraw(player, sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.BTC_DEPOSIT)
        {
            LFPG_BTCHelper.HandleBTCDeposit(player, sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.BTC_WITHDRAW_CASH)
        {
            LFPG_BTCHelper.HandleBTCWithdrawCash(player, sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.BTC_DEPOSIT_CASH)
        {
            LFPG_BTCHelper.HandleBTCDepositCash(player, sender, ctx);
        }
    }

    // =========================================================
    // Individual server handlers (extracted from modded PlayerBase)
    // =========================================================

    static void HandleFinishWiring(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender) return;

        LFPG_Util.Debug("[FinishWiring-Server] RPC received from pid=" + sender.GetPlainId());

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (rate limited)");
            PlayerBase.LFPG_SendClientMsg(player, "Too fast! Wait a moment.");
            return;
        }

        // v0.7.38 (RC-07): Reject during startup validation window.
        // ValidateAllWiresAndPropagate runs at T+5s and does a full rebuild.
        // Wires created before that would be overwritten, causing flicker.
        if (!LFPG_NetworkManager.Get().IsStartupValidationDone() || LFPG_NetworkManager.Get().IsValidationActive())
        {
            LFPG_Util.Info("[FinishWiring-Server] denied (startup validation pending)");
            PlayerBase.LFPG_SendClientMsg(player, "Server starting, please wait...");
            return;
        }

        if (!LFPG_WorldUtil.PlayerHasCableReelInHands(player))
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (no cable reel)");
            PlayerBase.LFPG_SendClientMsg(player, "You need a cable reel in your hands.");
            return;
        }

        int srcLow = 0;
        int srcHigh = 0;
        int dstLow = 0;
        int dstHigh = 0;
        string srcDeviceId;
        string dstDeviceId;
        string srcPort;
        string dstPort;

        array<vector> waypoints = new array<vector>;

        if (!ctx.Read(srcLow)) return;
        if (!ctx.Read(srcHigh)) return;
        if (!ctx.Read(dstLow)) return;
        if (!ctx.Read(dstHigh)) return;
        if (!ctx.Read(srcDeviceId)) return;
        if (!ctx.Read(dstDeviceId)) return;
        if (!ctx.Read(srcPort)) return;
        if (!ctx.Read(dstPort)) return;

        string pl1 = "[FinishWiring-Server] payload: src=" + srcDeviceId + " net=" + srcLow.ToString() + "," + srcHigh.ToString();
        string pl2 = "  dst=" + dstDeviceId + " net=" + dstLow.ToString() + "," + dstHigh.ToString();
        string pl3 = "  srcPort=" + srcPort + "  dstPort=" + dstPort;
        LFPG_Util.Debug(pl1 + pl2 + pl3);

        // Input hardening
        if (srcDeviceId.Length() > 64 || dstDeviceId.Length() > 64)
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (deviceId too long)");
            return;
        }
        if (srcPort.Length() > 32 || dstPort.Length() > 32)
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (port too long)");
            return;
        }

        if (!ctx.Read(waypoints)) return;

        int wpCount = 0;
        if (waypoints)
        {
            wpCount = waypoints.Count();
        }
        LFPG_Util.Debug("[FinishWiring-Server] waypoints=" + wpCount.ToString());

        // v0.7.32 (Audit): Validate RPC waypoints for NaN, range, and inter-wp distance.
        // ValidateWaypoints is called during deserialization but was missing from the
        // creation path. A modified client could inject extreme/NaN coordinates that
        // corrupt persistence and downstream calculations.
        if (waypoints && waypoints.Count() > 0)
        {
            if (waypoints.Count() > LFPG_MAX_WAYPOINTS)
            {
                LFPG_Util.Warn("[FinishWiring-Server] denied (too many waypoints: " + waypoints.Count().ToString() + ")");
                PlayerBase.LFPG_SendClientMsg(player, "Too many waypoints.");
                return;
            }

            if (!LFPG_WireHelper.ValidateWaypoints(waypoints, "FinishWiring-RPC", dstDeviceId))
            {
                LFPG_Util.Warn("[FinishWiring-Server] denied (corrupt waypoints from RPC)");
                PlayerBase.LFPG_SendClientMsg(player, "Invalid wire path.");
                return;
            }
        }

        // Resolve objects by network ID
        EntityAI srcObj = EntityAI.Cast(g_Game.GetObjectByNetworkId(srcLow, srcHigh));
        EntityAI dstObj = EntityAI.Cast(g_Game.GetObjectByNetworkId(dstLow, dstHigh));
        if (!srcObj || !dstObj)
        {
            LFPG_Util.Warn("[FinishWiring-Server] invalid net objects");
            return;
        }

        LFPG_Util.Debug("[FinishWiring-Server] resolved: src=" + srcObj.GetType() + " dst=" + dstObj.GetType());

        // v0.7.4: reject endpoints that aren't world-placed.
        // Devices in inventory, cargo, or attached to another entity
        // will change position (and thus vanilla ID), creating orphan
        // wires and persistence garbage. Block them server-side.
        if (srcObj.GetHierarchyParent())
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (src in inventory/cargo)");
            PlayerBase.LFPG_SendClientMsg(player, "Source device must be placed in the world.");
            return;
        }
        if (dstObj.GetHierarchyParent())
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (dst in inventory/cargo)");
            PlayerBase.LFPG_SendClientMsg(player, "Target device must be placed in the world.");
            return;
        }

        // Distance check: player must be near at least one end of the wire
        float distToSrc = vector.Distance(player.GetPosition(), srcObj.GetPosition());
        float distToDst = vector.Distance(player.GetPosition(), dstObj.GetPosition());
        float nearestDist = distToSrc;
        if (distToDst < nearestDist)
        {
            nearestDist = distToDst;
        }
        if (nearestDist > 4.0)
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (too far nearest=" + nearestDist.ToString() + "m)");
            PlayerBase.LFPG_SendClientMsg(player, "Too far from device.");
            return;
        }

        // v0.7.4: far endpoint hardening.
        // Prevent remote wiring exploits where a player with a spoofed
        // networkId connects to a device they never physically visited.
        // The far endpoint must be within max wire length of the player.
        // Normal gameplay: player walks from source to destination,
        // so they are near the destination and source is at most
        // wire-length away. This check blocks cross-map spoofing.
        float farthestDist = distToSrc;
        if (distToDst > farthestDist)
        {
            farthestDist = distToDst;
        }
        if (farthestDist > LFPG_MAX_WIRE_LEN_M)
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (far endpoint=" + farthestDist.ToString() + "m exceeds max wire len)");
            PlayerBase.LFPG_SendClientMsg(player, "Too far from remote device.");
            return;
        }

        // Universal validation: source must be energy source, dest must be consumer
        if (!LFPG_DeviceAPI.IsEnergySource(srcObj))
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (src not energy source) type=" + srcObj.GetType());
            PlayerBase.LFPG_SendClientMsg(player, "Source is not a power generator.");
            return;
        }
        if (!LFPG_DeviceAPI.IsEnergyConsumer(dstObj))
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (dst not consumer) type=" + dstObj.GetType());
            PlayerBase.LFPG_SendClientMsg(player, "Target is not an electrical device.");
            return;
        }

        // Generate/verify device IDs (vanilla gets position-based IDs)
        string srcRealId = LFPG_DeviceAPI.GetOrCreateDeviceId(srcObj);
        string dstRealId = LFPG_DeviceAPI.GetOrCreateDeviceId(dstObj);

        if (srcRealId == "" || dstRealId == "")
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (empty device IDs)");
            return;
        }

        // Register vanilla devices in DeviceRegistry so propagation can find them
        LFPG_DeviceRegistry.Get().Register(srcObj, srcRealId);
        LFPG_DeviceRegistry.Get().Register(dstObj, dstRealId);

        // v0.7.12 (B4): Self-connection check (client should catch this via B3,
        // but server must enforce it independently for anti-exploit)
        if (srcRealId == dstRealId)
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (self-connection) devId=" + srcRealId);
            PlayerBase.LFPG_SendClientMsg(player, "Cannot connect device to itself.");
            return;
        }

        // v0.7.12 (B4): Shared pre-connection validation via CanPreConnect.
        // Uses the same rules as client (B2/B3) for parity. Server-only checks
        // (quotas, rate-limit, permissions, anti-exploit distance) are above/below.
        vector preStartPos = srcObj.GetPosition();
        if (LFPG_DeviceAPI.GetDeviceId(srcObj) != "")
        {
            preStartPos = LFPG_DeviceAPI.GetPortWorldPos(srcObj, srcPort);
        }
        vector preEndPos = dstObj.GetPosition();
        if (LFPG_DeviceAPI.GetDeviceId(dstObj) != "")
        {
            preEndPos = LFPG_DeviceAPI.GetPortWorldPos(dstObj, dstPort);
        }

        // Resolve port directions for CanPreConnect
        LFPG_PreConnectParams pcp = new LFPG_PreConnectParams();
        pcp.srcEntity = srcObj;
        pcp.srcDeviceId = srcRealId;
        pcp.srcPort = srcPort;
        pcp.srcPortDir = LFPG_PortDir.OUT;
        pcp.dstEntity = dstObj;
        pcp.dstDeviceId = dstRealId;
        pcp.dstPort = dstPort;
        pcp.dstPortDir = LFPG_PortDir.IN;
        pcp.waypoints = waypoints;
        pcp.startPos = preStartPos;
        pcp.endPos = preEndPos;

        LFPG_PreConnectResult preResult = LFPG_ConnectionRules.CanPreConnect(pcp);

        if (!preResult.IsValid())
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied by CanPreConnect: " + preResult.m_Reason + " status=" + preResult.m_Status.ToString());
            PlayerBase.LFPG_SendClientMsg(player, preResult.m_Reason);
            return;
        }

        // Quota check
        string quotaReason;
        if (!LFPG_NetworkManager.Get().CanPlayerCreateAnotherWire(sender, quotaReason))
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (" + quotaReason + ")");
            PlayerBase.LFPG_SendClientMsg(player, "Wire limit reached: " + quotaReason);
            return;
        }

        // Port validation: only for LFPG-native devices (vanilla has no ports)
        bool srcIsLFPG = (LFPG_DeviceAPI.GetDeviceId(srcObj) != "");
        bool dstIsLFPG = (LFPG_DeviceAPI.GetDeviceId(dstObj) != "");

        if (!srcIsLFPG && LFPG_NetworkManager.Get().IsVanillaStoreReadOnly())
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied: vanilla wire store is read-only");
            PlayerBase.LFPG_SendClientMsg(player, "Vanilla wiring is temporarily read-only. Check the server log.");
            return;
        }

        if (srcIsLFPG)
        {
            if (!LFPG_DeviceAPI.HasPort(srcObj, srcPort, LFPG_PortDir.OUT))
            {
                LFPG_Util.Warn("[FinishWiring-Server] denied (src missing port " + srcPort + ")");
                PlayerBase.LFPG_SendClientMsg(player, "Invalid source port.");
                return;
            }
        }
        if (dstIsLFPG)
        {
            if (!LFPG_DeviceAPI.HasPort(dstObj, dstPort, LFPG_PortDir.IN))
            {
                LFPG_Util.Warn("[FinishWiring-Server] denied (dst missing port " + dstPort + ")");
                PlayerBase.LFPG_SendClientMsg(player, "Invalid target port.");
                return;
            }
        }

        // CanConnectTo only for LFPG sources (vanilla sources skip this)
        if (srcIsLFPG)
        {
            if (!LFPG_DeviceAPI.CanConnectTo(srcObj, dstObj, srcPort, dstPort))
            {
                LFPG_Util.Warn("[FinishWiring-Server] denied (CanConnectTo false)");
                PlayerBase.LFPG_SendClientMsg(player, "Cannot connect these devices.");
                return;
            }
        }

        // Validate wire geometry
        vector startPos = srcObj.GetPosition();
        if (srcIsLFPG)
        {
            startPos = LFPG_DeviceAPI.GetPortWorldPos(srcObj, srcPort);
        }
        vector endPos = dstObj.GetPosition();
        if (dstIsLFPG)
        {
            endPos = LFPG_DeviceAPI.GetPortWorldPos(dstObj, dstPort);
        }

        string reason;
        if (!LFPG_NetworkManager.Get().ValidateWire(startPos, endPos, waypoints, reason))
        {
            LFPG_Util.Warn("[FinishWiring-Server] Invalid wire: " + reason);
            PlayerBase.LFPG_SendClientMsg(player, "Invalid wire: " + reason);
            LFPG_ServerSettings st = LFPG_Settings.Get();
            if (st && st.KickOnInvalidWire)
            {
                g_Game.DisconnectPlayer(sender);
            }
            return;
        }

        // Create wire data
        LFPG_WireData wd = new LFPG_WireData();
        wd.m_TargetDeviceId = dstRealId;
        wd.m_TargetPort = dstPort;
        wd.m_SourcePort = srcPort;
        wd.m_CreatorId = sender.GetPlainId();
        // v0.7.45 (Patch 3B): Populate target NetworkID for CableRenderer fallback.
        // Without this, wires created after startup have m_TargetNetLow/High = 0
        // and CableRenderer cannot use NetworkID fallback during SyncVar lag.
        wd.m_TargetNetLow = dstLow;
        wd.m_TargetNetHigh = dstHigh;
        wd.m_Waypoints = new array<vector>;
        int i;
        for (i = 0; i < wpCount; i = i + 1)
        {
            wd.m_Waypoints.Insert(waypoints[i]);
        }

        // Resolve source as LFPG or vanilla
        bool isLfpgOwner = LFPG_DeviceAPI.HasWireStore(srcObj);
        bool anyRemoved = false;

        // ============================================================
        // COMPONENT SIZE CHECK (v0.7.36, Audit Feb2026): reject wire
        // if it would merge two components into one exceeding the
        // per-component node limit. Must run BEFORE any modifications.
        // ============================================================
        if (LFPG_NetworkManager.Get().CheckComponentSizeBeforeWire(srcRealId, dstRealId))
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (component size limit) " + srcRealId + " -> " + dstRealId);
            PlayerBase.LFPG_SendClientMsg(player, "Network too large. Cannot add more connections to this grid.");
            return;
        }

        // ============================================================
        // CYCLE CHECK (Sprint 4.1): reject wire if it would create a
        // directed cycle in the electrical graph.
        // Must run BEFORE any modifications (replacement phase).
        // ============================================================
        if (LFPG_NetworkManager.Get().CheckCycleBeforeWire(srcRealId, dstRealId))
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (cycle detected) " + srcRealId + " -> " + dstRealId);
            PlayerBase.LFPG_SendClientMsg(player, "Connection rejected: would create an electrical loop.");
            return;
        }

        // ============================================================
        // v0.7.38 (RC-01): Lock destination port to prevent concurrent
        // FinishWiring RPCs from both passing occupancy check on the same
        // port in the same tick. Two RPCs arriving simultaneously could
        // both read count=0, both proceed to AddWire, creating duplicate
        // edges and corrupted reverse index. The lock is released at all
        // exit points below.
        // ============================================================
        string portLockKey = dstRealId + "|" + dstPort;
        if (LFPG_NetworkManager.Get().IsPortLocked(portLockKey))
        {
            LFPG_Util.Info("[FinishWiring-Server] denied (port locked) " + portLockKey);
            PlayerBase.LFPG_SendClientMsg(player, "Port busy, try again.");
            return;
        }
        LFPG_NetworkManager.Get().LockPort(portLockKey);

        // ============================================================
        // REPLACEMENT PHASE: remove ALL conflicting wires BEFORE adding
        // v0.7.34 (Bloque E): Atomic mutation â€” prevents premature node
        // deletion between remove + add (same-target replace bug).
        // ============================================================

        // v0.7.34: Begin atomic mutation batch
        LFPG_NetworkManager.Get().BeginGraphMutation();

        ref array<int> srcDeltaOps = new array<int>;
        ref array<ref LFPG_WireData> srcDeltaWires = new array<ref LFPG_WireData>;
        bool lfpgSourceRemoved = false;
        LFPG_WireOwnerBase srcWireOwner = LFPG_WireOwnerBase.Cast(srcObj);

        // 1) Source port replacement: 1 wire per output port.
        //    Remove any existing wire from this source:port.
        if (isLfpgOwner)
        {
            array<ref LFPG_WireData> srcWires = LFPG_DeviceAPI.GetDeviceWires(srcObj);
            if (srcWires)
            {
                int sw = srcWires.Count() - 1;
                while (sw >= 0)
                {
                    LFPG_WireData srcExisting = srcWires[sw];
                    if (srcExisting && srcExisting.m_SourcePort == srcPort)
                    {
                        LFPG_Util.Info("[Replace-Src] Removed " + srcRealId + ":" + srcPort + " -> " + srcExisting.m_TargetDeviceId + ":" + srcExisting.m_TargetPort);

                        // v0.7.34 (Bloque E): Notify graph of wire removal.
                        // Without this, old edge stays stale in the graph.
                        LFPG_NetworkManager.Get().NotifyGraphWireRemoved(
                            srcRealId, srcExisting.m_TargetDeviceId,
                            srcPort, srcExisting.m_TargetPort);

                        // Incremental reverse index and player count update
                        LFPG_NetworkManager.Get().ReverseIdxRemove(srcExisting.m_TargetDeviceId, srcExisting.m_TargetPort, srcRealId);
                        LFPG_NetworkManager.Get().PlayerWireCountAdd(srcExisting.m_CreatorId, -1);
                        srcDeltaOps.Insert(LFPG_WireDeltaOp.REMOVE);
                        srcDeltaWires.Insert(srcExisting);
                        srcWires.Remove(sw);
                        anyRemoved = true;
                        lfpgSourceRemoved = true;
                    }
                    sw = sw - 1;
                }
            }
        }
        else
        {
            array<ref LFPG_WireData> vSrcWires = LFPG_NetworkManager.Get().GetVanillaWires(srcRealId);
            if (vSrcWires)
            {
                int vsw = vSrcWires.Count() - 1;
                while (vsw >= 0)
                {
                    LFPG_WireData vExisting = vSrcWires[vsw];
                    if (vExisting)
                    {
                        string vExistPort = vExisting.m_SourcePort;
                        if (vExistPort == "")
                        {
                            vExistPort = "output_1";
                        }
                        if (vExistPort == srcPort)
                        {
                            LFPG_Util.Info("[Replace-Src] Removed vanilla " + srcRealId + ":" + srcPort + " -> " + vExisting.m_TargetDeviceId);

                            // v0.7.34 (Bloque E): Notify graph of wire removal.
                            LFPG_NetworkManager.Get().NotifyGraphWireRemoved(
                                srcRealId, vExisting.m_TargetDeviceId,
                                vExistPort, vExisting.m_TargetPort);

                            // Incremental reverse index and player count update
                            LFPG_NetworkManager.Get().ReverseIdxRemove(vExisting.m_TargetDeviceId, vExisting.m_TargetPort, srcRealId);
                            LFPG_NetworkManager.Get().PlayerWireCountAdd(vExisting.m_CreatorId, -1);
                            vSrcWires.Remove(vsw);
                            anyRemoved = true;
                        }
                    }
                    vsw = vsw - 1;
                }
            }
        }

        // 2) Input port replacement: 1 wire per input port.
        //    Remove any wire from ANY source that targets this input port.
        //    Reverse index already updated incrementally above (no full rebuild needed).
        //    v0.7.3: removed redundant SaveVanillaWires() here. AddVanillaWire()
        //    saves on success, and any in-memory removal will be captured by the
        //    next save event (wire mutation, self-heal, or server shutdown).
        int existingIn = LFPG_NetworkManager.Get().CountWiresTargeting(dstRealId, dstPort);
        if (existingIn > 0)
        {
            int removedIn = LFPG_NetworkManager.Get().RemoveWiresTargeting(dstRealId, dstPort);
            LFPG_Util.Info("[Replace-In] Removed " + removedIn.ToString() + " wire(s) targeting " + dstRealId + ":" + dstPort);
            anyRemoved = true;
        }

        // ============================================================
        // STORE the new wire
        // ============================================================
        bool stored = false;
        if (isLfpgOwner)
        {
            stored = LFPG_DeviceAPI.AddDeviceWire(srcObj, wd);
            if (stored)
            {
                // Incremental updates for LFPG wire (vanilla handled inside AddVanillaWire)
                LFPG_NetworkManager.Get().ReverseIdxAdd(dstRealId, dstPort, srcRealId);
                LFPG_NetworkManager.Get().PlayerWireCountAdd(wd.m_CreatorId, 1);
            }
        }
        else
        {
            stored = LFPG_NetworkManager.Get().AddVanillaWire(srcRealId, wd);
        }

        if (!stored)
        {
            // v0.7.34 (Bloque E): Close mutation batch on early exit
            LFPG_NetworkManager.Get().EndGraphMutation();

            // v0.7.38 (RC-01): Release port lock
            LFPG_NetworkManager.Get().UnlockPort(portLockKey);

            // v0.7.33 (Fix #18b): If vanilla wires were removed during replacement phase
            // but the new wire failed to store, we must still persist the removal.
            // Without this, server restart would resurrect the removed wire.
            if (anyRemoved && !isLfpgOwner)
            {
                LFPG_NetworkManager.Get().MarkVanillaDirty();
            }
            if (anyRemoved)
            {
                LFPG_NetworkManager.Get().FlushVanillaIfDirty();
            }
            if (lfpgSourceRemoved && srcWireOwner)
            {
                srcWireOwner.LFPG_CommitWireMutation();
                LFPG_NetworkManager.Get().BroadcastOwnerWireDelta(srcObj, srcDeltaOps, srcDeltaWires);
            }

            // v0.7.38 (RC-06): If replacement removed wires but AddWire failed,
            // the graph and reverse index are inconsistent. Force a full rebuild
            // to restore data integrity from the authoritative wire arrays.
            if (anyRemoved)
            {
                LFPG_Util.Warn("[FinishWiring-Server] RC-06: store failed after replacement â€” forcing rebuild");
                LFPG_NetworkManager.Get().PostBulkRebuildAndPropagate();
            }

            LFPG_Util.Warn("[FinishWiring-Server] wire storage failed (duplicate or cap)");
            PlayerBase.LFPG_SendClientMsg(player, "Wire already exists or device is full.");
            return;
        }

        LFPG_Util.Info("[FinishWiring-Server] SUCCESS: " + srcRealId + ":" + srcPort + " -> " + dstRealId + ":" + dstPort + " wps=" + wpCount.ToString());

        // Reverse index already updated incrementally above

        // Sync wire data to clients for cable rendering
        if (isLfpgOwner)
        {
            srcDeltaOps.Insert(LFPG_WireDeltaOp.ADD);
            srcDeltaWires.Insert(wd);
            LFPG_NetworkManager.Get().BroadcastOwnerWireDelta(EntityAI.Cast(srcObj), srcDeltaOps, srcDeltaWires);
        }
        else
        {
            LFPG_NetworkManager.Get().BroadcastVanillaWires(srcRealId, srcObj);
            // Vanilla stores use a bounded 5s write-behind window.
            LFPG_NetworkManager.Get().MarkVanillaDirty();
        }

        // Propagate power to all consumers (LFPG and vanilla via SetPowered)
        // Sprint 4.2 S2: graph update first, then request propagation.
        // NotifyGraphWireAdded adds the edge and marks both endpoints dirty.
        // RequestPropagate additionally refreshes source state from entity.
        bool edgeAdded = LFPG_NetworkManager.Get().NotifyGraphWireAdded(srcRealId, dstRealId, srcPort, dstPort, wd);

        // v0.7.34 (Bloque E): Close atomic mutation batch.
        // All removes + the add are now committed atomically.
        // Deferred orphan cleanup runs here â€” nodes that lost edges
        // during remove but gained new ones during add are preserved.
        LFPG_NetworkManager.Get().EndGraphMutation();

        // v0.7.38 (RC-01): Release port lock
        LFPG_NetworkManager.Get().UnlockPort(portLockKey);

        if (!edgeAdded)
        {
            // Edge not inserted (node cap or missing node). Wire data is stored
            // but graph doesn't have the edge. Deferred orphan cleanup in
            // EndGraphMutation above may have deleted the target node (it had
            // no incoming edge from our perspective). Force full rebuild to
            // reconcile graph with wire data. This is a rare edge case
            // (requires saturating LFPG_MAX_NODES_GLOBAL).
            LFPG_Util.Warn("[FinishWiring-Server] Graph edge not inserted (limit or missing node) â€” forcing rebuild");
            LFPG_NetworkManager.Get().PostBulkRebuildAndPropagate();
        }
        else
        {
            LFPG_NetworkManager.Get().RequestPropagate(srcRealId);
        }
    }

    static void HandleCutWires(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender) return;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            PlayerBase.LFPG_SendClientMsg(player, "Too fast! Wait a moment.");
            return;
        }

        // v0.7.38 (RC-07): Reject during startup validation window.
        if (!LFPG_NetworkManager.Get().IsStartupValidationDone() || LFPG_NetworkManager.Get().IsValidationActive())
        {
            PlayerBase.LFPG_SendClientMsg(player, "Server starting, please wait...");
            return;
        }

        if (!LFPG_WorldUtil.PlayerHasPliersInHands(player))
        {
            LFPG_Util.Info("CutWires: denied (no pliers)");
            PlayerBase.LFPG_SendClientMsg(player, "You need pliers in your hands.");
            return;
        }

        int low = 0;
        int high = 0;
        if (!ctx.Read(low)) return;
        if (!ctx.Read(high)) return;

        EntityAI obj = EntityAI.Cast(g_Game.GetObjectByNetworkId(low, high));
        if (!obj) return;

        if (vector.Distance(player.GetPosition(), obj.GetPosition()) > 4.0)
        {
            PlayerBase.LFPG_SendClientMsg(player, "Too far from device.");
            return;
        }

        string deviceId = LFPG_DeviceAPI.GetOrCreateDeviceId(obj);
        if (deviceId == "") return;

        bool changed = false;
        LFPG_ServerSettings st = LFPG_Settings.Get();
        string cutPid = sender.GetPlainId();
        bool allowOthers = false;
        if (st)
            allowOthers = st.AllowCutOthersWires;

        // Try LFPG wire-owning device first (Generator, Splitter, etc.)
        if (LFPG_DeviceAPI.HasWireStore(obj))
        {
            // Pre-scan wires for incremental reverse index + player count updates.
            // Must mirror exactly what ClearDeviceWires / ClearDeviceWiresForCreator will remove.
            ref array<ref LFPG_WireData> preWires = LFPG_DeviceAPI.GetDeviceWires(obj);
            ref array<int> cutDeltaOps = new array<int>;
            ref array<ref LFPG_WireData> cutDeltaWires = new array<ref LFPG_WireData>;
            if (preWires)
            {
                int pw;
                for (pw = 0; pw < preWires.Count(); pw = pw + 1)
                {
                    LFPG_WireData pwd = preWires[pw];
                    if (!pwd) continue;
                    // If restricted to own wires, process own + unclaimed (empty CreatorId)
                    // (mirrors ClearForCreator which removes matching CreatorId + empty)
                    if (st && !st.AllowCutOthersWires && pwd.m_CreatorId != "" && pwd.m_CreatorId != cutPid)
                        continue;
                    cutDeltaOps.Insert(LFPG_WireDeltaOp.REMOVE);
                    cutDeltaWires.Insert(pwd);
                    LFPG_NetworkManager.Get().ReverseIdxRemove(pwd.m_TargetDeviceId, pwd.m_TargetPort, deviceId);
                    LFPG_NetworkManager.Get().PlayerWireCountAdd(pwd.m_CreatorId, -1);
                }
            }

            if (st && !st.AllowCutOthersWires)
            {
                changed = LFPG_DeviceAPI.ClearDeviceWiresForCreator(obj, cutPid);
            }
            else
            {
                changed = LFPG_DeviceAPI.ClearDeviceWires(obj);
            }

            if (changed)
            {
                LFPG_Util.Info("Wires cleared LFPG " + deviceId);
                LFPG_NetworkManager.Get().BroadcastOwnerWireDelta(obj, cutDeltaOps, cutDeltaWires);
            }
        }
        else
        {
            // Vanilla source: clear from central store
            array<ref LFPG_WireData> vWires = LFPG_NetworkManager.Get().GetVanillaWires(deviceId);
            if (vWires && vWires.Count() > 0)
            {
                if (st && !st.AllowCutOthersWires)
                {
                    // Cut own wires + unclaimed wires (empty CreatorId)
                    int vw = vWires.Count() - 1;
                    while (vw >= 0)
                    {
                        LFPG_WireData vwd = vWires[vw];
                        if (vwd)
                        {
                            if (vwd.m_CreatorId == "" || vwd.m_CreatorId == cutPid)
                            {
                                LFPG_NetworkManager.Get().ReverseIdxRemove(vwd.m_TargetDeviceId, vwd.m_TargetPort, deviceId);
                                LFPG_NetworkManager.Get().PlayerWireCountAdd(vwd.m_CreatorId, -1);
                                vWires.Remove(vw);
                                changed = true;
                            }
                        }
                        vw = vw - 1;
                    }
                }
                else
                {
                    // Update reverse index for all wires before clearing
                    int va;
                    for (va = 0; va < vWires.Count(); va = va + 1)
                    {
                        LFPG_WireData vawd = vWires[va];
                        if (vawd)
                        {
                            LFPG_NetworkManager.Get().ReverseIdxRemove(vawd.m_TargetDeviceId, vawd.m_TargetPort, deviceId);
                            LFPG_NetworkManager.Get().PlayerWireCountAdd(vawd.m_CreatorId, -1);
                        }
                    }
                    vWires.Clear();
                    changed = true;
                }
                
                if (changed)
                {
                    LFPG_Util.Info("Wires cleared vanilla " + deviceId);
                    LFPG_NetworkManager.Get().BroadcastVanillaWires(deviceId, obj);
                    LFPG_NetworkManager.Get().MarkVanillaDirty();
                }
            }
        }

        // Propagate: graph rebuilds from clean wire state, then marks sources dirty.
        // Reverse index already updated incrementally above (no full rebuild needed).

        // Also remove wires TARGETING this device's IN ports.
        // ClearDeviceWires only removes OWNED wires (output side).
        // Rescue fires if any IN port missed the index, then one unfiltered scan.
        // Devices with ports but no IN (vanilla source) also fire: the frozen
        // tree scanned on portCount > 0 even when the IN total stayed zero.
        bool anyPortMissedIndex = false;
        bool anyInRemovedByIndex = false;
        int portCount = LFPG_DeviceAPI.GetPortCount(obj);
        int inPortCount = 0;
        int pi;
        for (pi = 0; pi < portCount; pi = pi + 1)
        {
            int portDir = LFPG_DeviceAPI.GetPortDir(obj, pi);
            if (portDir == LFPG_PortDir.IN)
            {
                inPortCount = inPortCount + 1;
                string inPort = LFPG_DeviceAPI.GetPortName(obj, pi);
                int inRemoved = LFPG_NetworkManager.Get().RemoveWiresTargeting(deviceId, inPort, cutPid, allowOthers);
                if (inRemoved > 0)
                {
                    changed = true;
                    anyInRemovedByIndex = true;
                    LFPG_Util.Info("CutWires: removed " + inRemoved.ToString() + " incoming wire(s) on " + deviceId + ":" + inPort);
                }
                else
                {
                    anyPortMissedIndex = true;
                }
            }
        }

        if (anyPortMissedIndex || (portCount > 0 && inPortCount == 0))
        {
            int rescued = RescueStaleIncomingWires(obj, deviceId, "", cutPid, allowOthers);
            if (rescued > 0)
                changed = true;
            // Frozen tree rebuilt when changed && no IN index hits, even if
            // the scan found nothing (phantom reverse-index owners).
            if (rescued > 0 || (changed && !anyInRemovedByIndex))
            {
                LFPG_Util.Warn("[CutWires-Fallback] Reverse index was stale â€” rebuilding");
                LFPG_NetworkManager.Get().RebuildReverseIdx();
            }
        }

        if (changed)
        {
            // PostBulkRebuildAndPropagate: Rebuild â†’ PopulateStates â†’ MarkSourcesDirty
            LFPG_NetworkManager.Get().PostBulkRebuildAndPropagate();
            LFPG_NetworkManager.Get().FlushVanillaIfDirty();
            PlayerBase.LFPG_SendClientMsg(player, "Wires cut.");
        }
        else
        {
            PlayerBase.LFPG_SendClientMsg(player, "No wires to cut.");
        }
    }

    static void HandleCameraLink(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        // v0.9.1: DEPRECATED â€” camera linking is now physical (cables).
        // Read params to drain the stream (avoid corruption).
        int discardLow = 0;
        int discardHigh = 0;
        ctx.Read(discardLow);
        ctx.Read(discardHigh);
        LFPG_Util.Warn("[CameraLink] DEPRECATED RPC received â€” ignoring");
    }

    static void HandleCameraUnlink(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        // v0.9.1: DEPRECATED â€” camera unlinking is now physical (cut cable).
        // Read params to drain the stream (avoid corruption).
        int discardLow = 0;
        int discardHigh = 0;
        ctx.Read(discardLow);
        ctx.Read(discardHigh);
        LFPG_Util.Warn("[CameraUnlink] DEPRECATED RPC received â€” ignoring");
    }

    static void HandleRequestCameraList(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender)
            return;

        LFPG_NetworkManager manager = LFPG_NetworkManager.Get();
        LFPG_ControlSessionRegistry sessions = manager.GetControlSessionRegistry();
        if (!sessions)
            return;

        int monNetLow = 0;
        int monNetHigh = 0;
        if (!ctx.Read(monNetLow))
            return;
        if (!ctx.Read(monNetHigh))
            return;

        // Matching CCTV retry is identified before the global bucket.
        // A retry that already holds the session must not pay rate.
        // Any other outcome, including the "already active" reject, consumes
        // rate first so that path cannot be used to amplify traffic.
        LFPG_ControlSessionRecord currentSession = sessions.Get(sender);
        if (sessions.Matches(currentSession, LFPG_CONTROL_KIND_CCTV, monNetLow, monNetHigh))
        {
            if (!sessions.AllowCCTVReplay(currentSession, g_Game.GetTime() * 0.001))
                return;

            sessions.SendCCTVEnterResponse(currentSession);
            LFPG_Util.Info("[RequestCameraList] Replayed cached camera response");
            return;
        }

        if (!manager.AllowPlayerAction(sender))
        {
            PlayerBase.LFPG_SendClientMsg(player, "Too fast! Wait a moment.");
            return;
        }

        if (currentSession)
        {
            PlayerBase.LFPG_SendClientMsg(player, "Another control session is already active.");
            return;
        }

        // Resolve monitor entity by NetworkID
        EntityAI monEnt = EntityAI.Cast(g_Game.GetObjectByNetworkId(monNetLow, monNetHigh));
        if (!monEnt)
        {
            LFPG_Util.Warn("[RequestCameraList] monitor entity not found");
            PlayerBase.LFPG_SendClientMsg(player, "Monitor not found.");
            return;
        }

        LFPG_Monitor monitor = LFPG_Monitor.Cast(monEnt);
        if (!monitor)
        {
            LFPG_Util.Warn("[RequestCameraList] entity is not LFPG_Monitor");
            return;
        }

        if (!monitor.LFPG_IsPowered())
        {
            PlayerBase.LFPG_SendClientMsg(player, "El monitor no tiene alimentacion.");
            return;
        }

        if (vector.Distance(player.GetPosition(), monEnt.GetPosition()) > LFPG_INTERACT_DIST_M)
            return;

        // Collect cameras from monitor's wire store
        array<ref LFPG_WireData> wires = monitor.LFPG_GetWires();
        if (!wires || wires.Count() == 0)
        {
            string noWireLog = "[RequestCameraList] monitor " + monitor.LFPG_GetDeviceId();
            noWireLog = noWireLog + " has 0 wires";
            LFPG_Util.Info(noWireLog);
            PlayerBase.LFPG_SendClientMsg(player, "No hay camaras conectadas.");
            return;
        }

        string wireCountLog = "[RequestCameraList] monitor " + monitor.LFPG_GetDeviceId();
        wireCountLog = wireCountLog + " wire count=" + wires.Count().ToString();
        LFPG_Util.Info(wireCountLog);

        // Build camera list â€” up to LFPG_MONITOR_MAX_CAMERAS entries
        // v1.3.1: Per-camera power check REMOVED. The monitor is PASSTHROUGH:
        // if the monitor itself is powered (checked above), cameras on its
        // outputs WILL receive power once graph propagation completes.
        // After server restart, propagation runs asynchronously â€” cameras
        // may still have m_PoweredNet=false (derived state, not persisted).
        // Requiring powered cameras caused "no cameras" on every restart.
        // Hoist all variables before loop (Enforce Script)
        int camCount = 0;
        int unresolvedCount = 0;
        ref array<vector> camPositions = new array<vector>;
        ref array<vector> camOrientations = new array<vector>;
        ref array<string> camLabels = new array<string>;

        EntityAI camEnt = null;
        LFPG_Camera cam = null;
        string camDevId = "";
        int idLen = 0;
        string camLabel = "";
        int wi = 0;
        vector rawOri = "0 0 0";
        float adjYaw = 0.0;
        vector adjOri = "0 0 0";

        while (wi < wires.Count())
        {
            LFPG_WireData wd = wires[wi];
            wi = wi + 1;

            if (!wd)
                continue;

            camDevId = wd.m_TargetDeviceId;
            if (camDevId == "")
                continue;

            camEnt = LFPG_DeviceRegistry.Get().FindById(camDevId);
            if (!camEnt)
            {
                unresolvedCount = unresolvedCount + 1;
                string missLog = "[RequestCameraList] wire target not in registry: " + camDevId;
                LFPG_Util.Warn(missLog);
                continue;
            }

            cam = LFPG_Camera.Cast(camEnt);
            if (!cam)
                continue;

            // Build label: CAM-XXXXXX (last 6 chars of deviceId)
            idLen = camDevId.Length();
            if (idLen > 6)
            {
                camLabel = "CAM-" + camDevId.Substring(idLen - 6, 6);
            }
            else
            {
                camLabel = "CAM-" + camDevId;
            }

            camPositions.Insert(cam.GetPosition());
            // v1.0.1: Camera model lens points 90Â° right of entity forward.
            // Apply +90Â° yaw so the viewport aligns with the optic.
            // DayZ yaw: positive = clockwise from above = right.
            rawOri = cam.GetOrientation();
            adjYaw = rawOri[0] + 90.0;
            adjOri = Vector(adjYaw, rawOri[1], rawOri[2]);
            camOrientations.Insert(adjOri);
            camLabels.Insert(camLabel);
            camCount = camCount + 1;

            if (camCount >= LFPG_MONITOR_MAX_CAMERAS)
                break;
        }

        if (camCount == 0)
        {
            string noResolveLog = "[RequestCameraList] 0 cameras resolved. wires=" + wires.Count().ToString();
            noResolveLog = noResolveLog + " unresolved=" + unresolvedCount.ToString();
            LFPG_Util.Warn(noResolveLog);
            PlayerBase.LFPG_SendClientMsg(player, "No hay camaras detectables.");
            return;
        }

        LFPG_ControlSessionRecord cameraSession = sessions.BeginCCTV(sender, player, monitor, monNetLow, monNetHigh, camCount, camPositions, camOrientations, camLabels);
        if (!cameraSession)
        {
            LFPG_Util.Warn("[RequestCameraList] control session registration failed");
            return;
        }

        // COT pattern: engine spectator system for camera lifecycle.
        // 1. Set skip flag to prevent vanilla OnSelectPlayer side effects
        // 2. SelectPlayer(sender, NULL) â†’ desasociar player del identity
        // 3. SelectSpectator(sender, cls, pos) â†’ engine crea+trackea cÃ¡mara
        vector firstCamPos = camPositions[0];

        player.LFPG_SetSkipOnSelectPlayer(true);
        g_Game.SelectPlayer(sender, null);
        g_Game.SelectSpectator(sender, "staticcamera", firstCamPos);

        string specLog = "[RequestCameraList] SelectPlayer(null) + SelectSpectator at ";
        specLog = specLog + firstCamPos.ToString();
        LFPG_Util.Info(specLog);

        // Cache was frozen before the switch; every identical retry reuses this payload.
        sessions.MarkActive(cameraSession);
        sessions.SendCCTVEnterResponse(cameraSession);

        string logMsg = "[RequestCameraList] Sent " + camCount.ToString() + " cameras to player";
        LFPG_Util.Info(logMsg);
    }

    // Reachable while sender.GetPlayer() is null after SelectPlayer(null).
    // No PlayerBase parameter: this path must not authorize by a client-chosen body.
    static void HandleCameraListReplayOnly(PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender)
            return;

        int monNetLow = 0;
        int monNetHigh = 0;
        if (!ctx.Read(monNetLow))
            return;
        if (!ctx.Read(monNetHigh))
            return;

        LFPG_NetworkManager manager = LFPG_NetworkManager.Get();
        LFPG_ControlSessionRegistry sessions = manager.GetControlSessionRegistry();
        if (!sessions)
            return;

        LFPG_ControlSessionRecord record = sessions.Get(sender);
        if (!sessions.Matches(record, LFPG_CONTROL_KIND_CCTV, monNetLow, monNetHigh))
        {
            LFPG_Util.RateLimitedWarn(sender, "camera_list_replay_denied", "[RequestCameraList] Replay-only denied: no matching CCTV session");
            return;
        }

        if (!sessions.AllowCCTVReplay(record, g_Game.GetTime() * 0.001))
            return;

        sessions.SendCCTVEnterResponse(record);
    }

    static void HandleCCTVExitRequest(PlayerBase player, PlayerIdentity sender)
    {
        if (!sender)
            return;

        LFPG_NetworkManager manager = LFPG_NetworkManager.Get();
        LFPG_ControlSessionRegistry sessions = manager.GetControlSessionRegistry();
        if (!sessions)
            return;

        // The original PlayerBase comes exclusively from the server-owned session.
        if (!sessions.EndCCTV(sender, true))
            return;

        string logMsg = "[CCTV_EXIT] SelectPlayer + confirm sent for ";
        logMsg = logMsg + sender.GetName();
        LFPG_Util.Info(logMsg);
    }

    static bool RefreshSearchlightSplash(LFPG_Searchlight sl, float aimYaw, float aimPitch, bool forceRefresh)
    {
        if (!sl)
            return false;

        bool cadenceDue = sl.LFPG_ShouldRefreshSplash(g_Game.GetTime());
        if (!forceRefresh && !cadenceDue)
            return false;

        // Splash raycast -- beam direction in world space; aimYaw is local to the searchlight.
        vector beamStart = sl.ModelToWorld(sl.GetMemoryPointPos("light_main"));
        float worldYaw = sl.LFPG_GetBaseYaw() + aimYaw;
        float yawRad = worldYaw * Math.DEG2RAD;
        float pitchRad = aimPitch * Math.DEG2RAD;
        float cosPitch = Math.Cos(pitchRad);
        float dirX = Math.Sin(yawRad) * cosPitch;
        float dirY = Math.Sin(pitchRad);
        float dirZ = Math.Cos(yawRad) * cosPitch;

        float rayToX = beamStart[0] + dirX * LFPG_SEARCHLIGHT_SPLASH_RANGE_M;
        float rayToY = beamStart[1] + dirY * LFPG_SEARCHLIGHT_SPLASH_RANGE_M;
        float rayToZ = beamStart[2] + dirZ * LFPG_SEARCHLIGHT_SPLASH_RANGE_M;
        vector rayTo = Vector(rayToX, rayToY, rayToZ);

        vector hitPos;
        vector hitNormal;
        int hitComp;
        set<Object> hitResults = null;
        Object hitWith = null;
        bool sorted = false;
        bool groundOnly = false;
        float radius = 0.0;

        bool hit = DayZPhysics.RaycastRV(beamStart, rayTo, hitPos, hitNormal, hitComp, hitResults, hitWith, sl, sorted, groundOnly, ObjIntersectFire, radius);
        if (hit)
        {
            float splashY = hitPos[1] + 0.05;
            sl.LFPG_SetSplash(true, hitPos[0], splashY, hitPos[2]);
        }
        else
        {
            sl.LFPG_SetSplash(false, 0.0, 0.0, 0.0);
        }
        return true;
    }

    static void HandleSearchlightEnter(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender)
            return;

        LFPG_NetworkManager manager = LFPG_NetworkManager.Get();
        LFPG_ControlSessionRegistry sessions = manager.GetControlSessionRegistry();
        if (!sessions)
            return;

        if (!manager.AllowPlayerAction(sender))
        {
            PlayerBase.LFPG_SendClientMsg(player, "Too fast! Wait a moment.");
            return;
        }

        int netLow = 0;
        int netHigh = 0;
        if (!ctx.Read(netLow))
            return;
        if (!ctx.Read(netHigh))
            return;

        // Resolve searchlight by NetworkID
        Object slObj = g_Game.GetObjectByNetworkId(netLow, netHigh);
        if (!slObj)
        {
            LFPG_Util.Warn("[Searchlight_Enter] Cannot resolve NetworkID");
            return;
        }

        LFPG_Searchlight sl = LFPG_Searchlight.Cast(slObj);
        if (!sl)
        {
            LFPG_Util.Warn("[Searchlight_Enter] Object is not LFPG_Searchlight");
            return;
        }

        if (!sl.LFPG_IsPowered())
        {
            PlayerBase.LFPG_SendClientMsg(player, "Searchlight is not powered.");
            return;
        }

        // Resolve player and its NetworkID before any operator comparison.
        PlayerBase playerCheck = PlayerBase.Cast(sender.GetPlayer());
        if (!playerCheck)
            return;

        float distSq = LFPG_WorldUtil.DistSq(playerCheck.GetPosition(), sl.GetPosition());
        float maxDistSq = LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M;
        if (distSq > maxDistSq)
        {
            LFPG_Util.Warn("[Searchlight_Enter] Distance check failed");
            return;
        }

        int playerNetLow  = 0;
        int playerNetHigh = 0;
        playerCheck.GetNetworkID(playerNetLow, playerNetHigh);
        if (playerNetLow == 0 && playerNetHigh == 0)
        {
            LFPG_Util.RateLimitedWarn(sender, "searchlight_invalid_operator_id", "[Searchlight_Enter] Invalid player NetworkID");
            return;
        }

        LFPG_ControlSessionRecord currentSession = sessions.Get(sender);
        if (currentSession && !sessions.Matches(currentSession, LFPG_CONTROL_KIND_SEARCHLIGHT, netLow, netHigh))
        {
            PlayerBase.LFPG_SendClientMsg(player, "Another control session is already active.");
            return;
        }

        bool hasOperator = sl.LFPG_HasOperator();
        if (hasOperator)
        {
            if (!sl.LFPG_IsOperator(playerNetLow, playerNetHigh))
            {
                PlayerBase.LFPG_SendClientMsg(player, "Searchlight is already being operated.");
                return;
            }

            if (!currentSession)
            {
                float retryYaw = sl.LFPG_GetAimYaw();
                float retryPitch = sl.LFPG_GetAimPitch();
                currentSession = sessions.BeginSearchlight(sender, playerCheck, sl, netLow, netHigh, playerNetLow, playerNetHigh, retryYaw, retryPitch);
                if (!currentSession)
                    return;
                sessions.MarkActive(currentSession);
            }

            sessions.SendSearchlightEnterConfirm(currentSession);
            LFPG_Util.Info("[Searchlight_Enter] Replayed cached enter confirm");
            return;
        }

        // A matching stale record whose lock was already released is terminal.
        if (currentSession)
        {
            sessions.EndSearchlight(sender, netLow, netHigh, false);
            currentSession = null;
        }

        float curYaw = sl.LFPG_GetAimYaw();
        float curPitch = sl.LFPG_GetAimPitch();
        LFPG_ControlSessionRecord searchlightSession = sessions.BeginSearchlight(sender, playerCheck, sl, netLow, netHigh, playerNetLow, playerNetHigh, curYaw, curPitch);
        if (!searchlightSession)
            return;

        sl.LFPG_SetOperator(playerNetLow, playerNetHigh);
        bool initialSplashRaycasted = RefreshSearchlightSplash(sl, curYaw, curPitch, true);
        sl.LFPG_FlushSyncVars(initialSplashRaycasted);

        sessions.MarkActive(searchlightSession);
        sessions.SendSearchlightEnterConfirm(searchlightSession);

        string logMsg = "[Searchlight_Enter] Grab confirmed yaw=";
        logMsg = logMsg + curYaw.ToString();
        logMsg = logMsg + " pitch=" + curPitch.ToString();
        LFPG_Util.Info(logMsg);
    }

    static void HandleSearchlightAim(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender)
            return;

        int netLow = 0;
        int netHigh = 0;
        float aimYaw = 0.0;
        float aimPitch = 0.0;

        if (!ctx.Read(netLow))
            return;
        if (!ctx.Read(netHigh))
            return;
        if (!ctx.Read(aimYaw))
            return;
        if (!ctx.Read(aimPitch))
            return;

        // Reject invalid numeric input before object resolution or authorization.
        if (LFPG_Searchlight.LFPG_IsInvalidAimValue(aimYaw) || LFPG_Searchlight.LFPG_IsInvalidAimValue(aimPitch))
        {
            LFPG_Util.RateLimitedWarn(sender, "searchlight_non_finite_aim", "[Searchlight] Rejected non-finite aim input");
            return;
        }

        // Normalize and clamp without work proportional to the input magnitude.
        aimYaw = LFPG_Searchlight.LFPG_NormalizeAimYaw(aimYaw);
        if (aimPitch < LFPG_SEARCHLIGHT_PITCH_MIN)
            aimPitch = LFPG_SEARCHLIGHT_PITCH_MIN;
        if (aimPitch > LFPG_SEARCHLIGHT_PITCH_MAX)
            aimPitch = LFPG_SEARCHLIGHT_PITCH_MAX;

        LFPG_NetworkManager manager = LFPG_NetworkManager.Get();
        LFPG_ControlSessionRegistry sessions = manager.GetControlSessionRegistry();
        if (!sessions)
            return;

        LFPG_ControlSessionRecord record = sessions.Get(sender);
        if (!sessions.Matches(record, LFPG_CONTROL_KIND_SEARCHLIGHT, netLow, netHigh))
        {
            // No session: charge the global attempt budget. B-22 removes the shared
            // bucket for the operator's aim stream only, not for unsolicited AIM.
            manager.AllowPlayerAction(sender);
            return;
        }

        LFPG_Searchlight sl = record.m_Searchlight;
        if (!sl)
            return;

        if (!sl.LFPG_IsPowered())
        {
            sessions.EndSearchlight(sender, netLow, netHigh, true);
            return;
        }

        // AIM uses the grab radius (2.5 m) as HORIZONTAL distance, matching the
        // client auto-exit (dx*dx+dz*dz). Height does not count. Enter stays 3D
        // at LFPG_INTERACT_DIST_M (5.0 m); that wider check is a different gate.
        if (!record.m_Player)
            return;

        vector aimPlayerPos = record.m_Player.GetPosition();
        vector aimSlPos = sl.GetPosition();
        float dxAim = aimPlayerPos[0] - aimSlPos[0];
        float dzAim = aimPlayerPos[2] - aimSlPos[2];
        float distSq = dxAim * dxAim + dzAim * dzAim;
        float maxDistSq = LFPG_SEARCHLIGHT_GRAB_RADIUS_M * LFPG_SEARCHLIGHT_GRAB_RADIUS_M;
        if (distSq > maxDistSq)
        {
            sessions.EndSearchlight(sender, netLow, netHigh, true);
            return;
        }

        if (!sl.LFPG_IsOperator(record.m_PlayerNetLow, record.m_PlayerNetHigh))
            return;

        float nowSeconds = g_Game.GetTime() * 0.001;
        if (!sessions.AllowSearchlightAim(record, nowSeconds))
            return;

        sl.LFPG_SetAim(aimYaw, aimPitch);

        bool splashRaycasted = RefreshSearchlightSplash(sl, aimYaw, aimPitch, false);

        // Single SetSynchDirty for aim plus the latest splash state.
        sl.LFPG_FlushSyncVars(splashRaycasted);
    }

    static void HandleSearchlightExit(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender)
            return;

        int netLow = 0;
        int netHigh = 0;
        if (!ctx.Read(netLow))
            return;
        if (!ctx.Read(netHigh))
            return;

        LFPG_NetworkManager manager = LFPG_NetworkManager.Get();
        LFPG_ControlSessionRegistry sessions = manager.GetControlSessionRegistry();
        if (!sessions)
            return;

        sessions.ArmSearchlightExitDeadline(sender, netLow, netHigh);
        if (!sessions.EndSearchlight(sender, netLow, netHigh, true))
            return;

        string logMsg = "[Searchlight_Exit] Operator released for ";
        logMsg = logMsg + sender.GetName();
        LFPG_Util.Info(logMsg);
    }

    static void HandleSearchlightExitV2(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender)
            return;

        int netLow = 0;
        int netHigh = 0;
        float aimYaw = 0.0;
        float aimPitch = 0.0;
        if (!ctx.Read(netLow))
            return;
        if (!ctx.Read(netHigh))
            return;
        if (!ctx.Read(aimYaw))
            return;
        if (!ctx.Read(aimPitch))
            return;

        LFPG_NetworkManager manager = LFPG_NetworkManager.Get();
        LFPG_ControlSessionRegistry sessions = manager.GetControlSessionRegistry();
        if (!sessions)
            return;

        // Receipt of an exit arms recovery; a healthy session otherwise has no deadline.
        sessions.ArmSearchlightExitDeadline(sender, netLow, netHigh);

        // Reject invalid numeric input before object resolution or authorization.
        if (LFPG_Searchlight.LFPG_IsInvalidAimValue(aimYaw) || LFPG_Searchlight.LFPG_IsInvalidAimValue(aimPitch))
        {
            LFPG_Util.RateLimitedWarn(sender, "searchlight_non_finite_aim", "[Searchlight] Rejected non-finite aim input");
            return;
        }

        // Normalize and clamp without work proportional to the input magnitude.
        aimYaw = LFPG_Searchlight.LFPG_NormalizeAimYaw(aimYaw);
        if (aimPitch < LFPG_SEARCHLIGHT_PITCH_MIN)
            aimPitch = LFPG_SEARCHLIGHT_PITCH_MIN;
        if (aimPitch > LFPG_SEARCHLIGHT_PITCH_MAX)
            aimPitch = LFPG_SEARCHLIGHT_PITCH_MAX;

        LFPG_ControlSessionRecord record = sessions.Get(sender);
        LFPG_Searchlight sl = null;
        int operatorNetLow = 0;
        int operatorNetHigh = 0;

        if (sessions.Matches(record, LFPG_CONTROL_KIND_SEARCHLIGHT, netLow, netHigh))
        {
            sl = record.m_Searchlight;
            operatorNetLow = record.m_PlayerNetLow;
            operatorNetHigh = record.m_PlayerNetHigh;
        }
        else
        {
            Object searchlightObject = g_Game.GetObjectByNetworkId(netLow, netHigh);
            sl = LFPG_Searchlight.Cast(searchlightObject);
            if (!sl)
                return;

            PlayerBase operatorPlayer = PlayerBase.Cast(sender.GetPlayer());
            if (!operatorPlayer)
                return;
            operatorPlayer.GetNetworkID(operatorNetLow, operatorNetHigh);
        }

        if (!sl)
            return;
        if (operatorNetLow == 0 && operatorNetHigh == 0)
            return;
        if (!sl.LFPG_IsOperator(operatorNetLow, operatorNetHigh))
            return;

        // The global action cooldown remains bypassed for this one-shot final value.
        sl.LFPG_SetAim(aimYaw, aimPitch);
        RefreshSearchlightSplash(sl, aimYaw, aimPitch, true);

        if (!sessions.EndSearchlight(sender, netLow, netHigh, true))
            return;

        string logMsg = "[Searchlight_ExitV2] Final aim applied and operator released for ";
        logMsg = logMsg + sender.GetName();
        LFPG_Util.Info(logMsg);
    }

    static void HandleCutPort(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender) return;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            PlayerBase.LFPG_SendClientMsg(player, "Too fast! Wait a moment.");
            return;
        }

        // v0.7.38 (RC-07): Reject during startup validation window.
        if (!LFPG_NetworkManager.Get().IsStartupValidationDone() || LFPG_NetworkManager.Get().IsValidationActive())
        {
            PlayerBase.LFPG_SendClientMsg(player, "Server starting, please wait...");
            return;
        }

        if (!LFPG_WorldUtil.PlayerHasPliersInHands(player))
        {
            PlayerBase.LFPG_SendClientMsg(player, "You need pliers in your hands.");
            return;
        }

        int low = 0;
        int high = 0;
        string portName;
        int portDir = 0;

        if (!ctx.Read(low)) return;
        if (!ctx.Read(high)) return;
        if (!ctx.Read(portName)) return;
        if (!ctx.Read(portDir)) return;

        // v0.7.4: validate RPC parameters from client.
        // portDir must be a known enum value (IN=0, OUT=1).
        // portName must be reasonable length and non-empty.
        if (portDir != LFPG_PortDir.IN && portDir != LFPG_PortDir.OUT)
        {
            LFPG_Util.Warn("[CutPort] denied (invalid portDir=" + portDir.ToString() + ")");
            return;
        }
        if (portName == "" || portName.Length() > 32)
        {
            LFPG_Util.Warn("[CutPort] denied (invalid portName len=" + portName.Length().ToString() + ")");
            return;
        }

        EntityAI obj = EntityAI.Cast(g_Game.GetObjectByNetworkId(low, high));
        if (!obj) return;

        if (vector.Distance(player.GetPosition(), obj.GetPosition()) > 4.0)
        {
            PlayerBase.LFPG_SendClientMsg(player, "Too far from device.");
            return;
        }

        string deviceId = LFPG_DeviceAPI.GetOrCreateDeviceId(obj);
        if (deviceId == "") return;

        bool changed = false;
        LFPG_ServerSettings st = LFPG_Settings.Get();
        string cutPid = sender.GetPlainId();
        bool allowOthers = false;
        if (st)
            allowOthers = st.AllowCutOthersWires;

        if (portDir == LFPG_PortDir.OUT)
        {
            // Remove wire(s) from this device's specific output port
            // Generic: works for Generator, Splitter, or any wire-owning device
            if (LFPG_DeviceAPI.HasWireStore(obj))
            {
                ref array<int> portDeltaOps = new array<int>;
                ref array<ref LFPG_WireData> portDeltaWires = new array<ref LFPG_WireData>;
                ref array<ref LFPG_WireData> ownerWires = LFPG_DeviceAPI.GetDeviceWires(obj);
                if (ownerWires)
                {
                    int ow = ownerWires.Count() - 1;
                    while (ow >= 0)
                    {
                        LFPG_WireData wd = ownerWires[ow];
                        if (wd && wd.m_SourcePort == portName)
                        {
                            // Respect AllowCutOthersWires setting
                            if (st && !st.AllowCutOthersWires && wd.m_CreatorId != "" && wd.m_CreatorId != cutPid)
                            {
                                LFPG_Util.Info("[CutPort] Skipped wire on " + portName + " (not creator)");
                            }
                            else
                            {
                                LFPG_Util.Info("[CutPort] Removed OUT wire " + deviceId + ":" + portName + " -> " + wd.m_TargetDeviceId);
                                // Incremental reverse index and player count update
                                LFPG_NetworkManager.Get().ReverseIdxRemove(wd.m_TargetDeviceId, wd.m_TargetPort, deviceId);
                                LFPG_NetworkManager.Get().PlayerWireCountAdd(wd.m_CreatorId, -1);
                                portDeltaOps.Insert(LFPG_WireDeltaOp.REMOVE);
                                portDeltaWires.Insert(wd);
                                ownerWires.Remove(ow);
                                changed = true;
                            }
                        }
                        ow = ow - 1;
                    }
                }
                if (changed)
                {
                    LFPG_WireOwnerBase portWireOwner = LFPG_WireOwnerBase.Cast(obj);
                    if (portWireOwner)
                    {
                        portWireOwner.LFPG_CommitWireMutation();
                        LFPG_NetworkManager.Get().BroadcastOwnerWireDelta(obj, portDeltaOps, portDeltaWires);
                    }
                    else
                    {
                        obj.SetSynchDirty();
                        LFPG_NetworkManager.Get().BroadcastOwnerWires(obj);
                    }
                }
            }
            else
            {
                // Vanilla source
                ref array<ref LFPG_WireData> vWires = LFPG_NetworkManager.Get().GetVanillaWires(deviceId);
                if (vWires)
                {
                    int vw = vWires.Count() - 1;
                    while (vw >= 0)
                    {
                        LFPG_WireData vwd = vWires[vw];
                        if (vwd)
                        {
                            string sp = vwd.m_SourcePort;
                            if (sp == "")
                            {
                                sp = "output_1";
                            }
                            if (sp == portName)
                            {
                                // Respect AllowCutOthersWires setting
                                if (st && !st.AllowCutOthersWires && vwd.m_CreatorId != "" && vwd.m_CreatorId != cutPid)
                                {
                                    LFPG_Util.Info("[CutPort] Skipped vanilla wire on " + portName + " (not creator)");
                                }
                                else
                                {
                                    // Incremental reverse index and player count update
                                    LFPG_NetworkManager.Get().ReverseIdxRemove(vwd.m_TargetDeviceId, vwd.m_TargetPort, deviceId);
                                    LFPG_NetworkManager.Get().PlayerWireCountAdd(vwd.m_CreatorId, -1);
                                    vWires.Remove(vw);
                                    changed = true;
                                }
                            }
                        }
                        vw = vw - 1;
                    }
                }
                if (changed)
                {
                    LFPG_NetworkManager.Get().BroadcastVanillaWires(deviceId, obj);
                    LFPG_NetworkManager.Get().MarkVanillaDirty();
                }
            }
        }
        else if (portDir == LFPG_PortDir.IN)
        {
            // Remove all wires targeting this device+port from ANY source.
            // Always scan: an index hit on one owner must not leave the others.
            int removed = LFPG_NetworkManager.Get().RemoveWiresTargeting(deviceId, portName, cutPid, allowOthers);
            int rescued = RescueStaleIncomingWires(obj, deviceId, portName, cutPid, allowOthers);
            int cutTotal = removed + rescued;
            if (cutTotal > 0)
            {
                changed = true;
                LFPG_Util.Info("[CutPort] Removed " + cutTotal.ToString() + " IN wire(s) on " + deviceId + ":" + portName);
            }
            if (rescued > 0)
            {
                LFPG_Util.Warn("[CutWires-Fallback] Reverse index was stale â€” rebuilding");
                LFPG_NetworkManager.Get().RebuildReverseIdx();
            }
        }

        if (changed)
        {
            // PostBulkRebuildAndPropagate: Rebuild â†’ PopulateStates â†’ MarkSourcesDirty.
            // For IN port cuts, this also replaces RequestGlobalSelfHeal since it
            // achieves the same result (full rebuild + re-propagation from all sources).
            LFPG_NetworkManager.Get().PostBulkRebuildAndPropagate();

            LFPG_NetworkManager.Get().FlushVanillaIfDirty();

            PlayerBase.LFPG_SendClientMsg(player, "Wire cut on " + portName + ".");
        }
        else
        {
            PlayerBase.LFPG_SendClientMsg(player, "No wire on that port.");
        }
    }

    // Same key rule as RemoveWiresTargeting / ReverseIdxAdd: empty incoming
    // port indexes as input_main. No shared helper exists on NetworkManager.
    protected static string IncomingPortIndexKey(string port)
    {
        if (port == "")
            return "input_main";
        return port;
    }

    // Scan every owner store for incoming wires the reverse index missed.
    // targetPort == "" matches any port: undeclared / renamed / empty names
    // after migration still get cut. A non-empty port filters to that port,
    // treating "" and input_main as the same key (see IncomingPortIndexKey).
    // Returns how many wires were removed. Caller rebuilds the reverse index.
    protected static int RescueStaleIncomingWires(EntityAI targetObj, string targetDeviceId, string targetPort, string cutPid, bool allowOthers)
    {
        if (targetDeviceId == "")
            return 0;

        int rescued = 0;
        array<EntityAI> allDevs = new array<EntityAI>;
        LFPG_DeviceRegistry.Get().GetAll(allDevs);
        int di;
        for (di = 0; di < allDevs.Count(); di = di + 1)
        {
            EntityAI srcDev = allDevs[di];
            if (!srcDev) continue;
            if (srcDev == targetObj) continue;
            if (!LFPG_DeviceAPI.HasWireStore(srcDev)) continue;

            string srcId = LFPG_DeviceAPI.GetDeviceId(srcDev);
            ref array<ref LFPG_WireData> srcWires = LFPG_DeviceAPI.GetDeviceWires(srcDev);
            if (!srcWires) continue;

            bool srcChanged = false;
            ref array<int> fallbackDeltaOps = new array<int>;
            ref array<ref LFPG_WireData> fallbackDeltaWires = new array<ref LFPG_WireData>;
            int sw = srcWires.Count() - 1;
            while (sw >= 0)
            {
                LFPG_WireData swd = srcWires[sw];
                if (swd && swd.m_TargetDeviceId == targetDeviceId && (targetPort == "" || swd.m_TargetPort == targetPort || IncomingPortIndexKey(swd.m_TargetPort) == IncomingPortIndexKey(targetPort)) && (allowOthers || LFPG_WireHelper.CanCreatorCutWire(swd, cutPid, allowOthers)))
                {
                    LFPG_Util.Warn("[CutWires-Fallback] Found stale wire: " + srcId + ":" + swd.m_SourcePort + " -> " + targetDeviceId + ":" + swd.m_TargetPort);
                    LFPG_NetworkManager.Get().PlayerWireCountAdd(swd.m_CreatorId, -1);
                    fallbackDeltaOps.Insert(LFPG_WireDeltaOp.REMOVE);
                    fallbackDeltaWires.Insert(swd);
                    srcWires.Remove(sw);
                    srcChanged = true;
                    rescued = rescued + 1;
                }
                sw = sw - 1;
            }

            if (srcChanged)
            {
                LFPG_WireOwnerBase fallbackWireOwner = LFPG_WireOwnerBase.Cast(srcDev);
                if (fallbackWireOwner)
                {
                    fallbackWireOwner.LFPG_CommitWireMutation();
                    LFPG_NetworkManager.Get().BroadcastOwnerWireDelta(srcDev, fallbackDeltaOps, fallbackDeltaWires);
                }
                else
                {
                    srcDev.SetSynchDirty();
                    LFPG_NetworkManager.Get().BroadcastOwnerWires(srcDev);
                }
                LFPG_NetworkManager.Get().RequestPropagate(srcId);
            }
        }

        int vkScan;
        int vkCount = LFPG_NetworkManager.Get().GetVanillaWireOwnerCount();
        for (vkScan = 0; vkScan < vkCount; vkScan = vkScan + 1)
        {
            string vOwnId = LFPG_NetworkManager.Get().GetVanillaWireOwnerKey(vkScan);
            array<ref LFPG_WireData> vwScan = LFPG_NetworkManager.Get().GetVanillaWires(vOwnId);
            if (!vwScan) continue;

            bool vSrcChanged = false;
            int vsw = vwScan.Count() - 1;
            while (vsw >= 0)
            {
                LFPG_WireData vswd = vwScan[vsw];
                if (vswd && vswd.m_TargetDeviceId == targetDeviceId && (targetPort == "" || vswd.m_TargetPort == targetPort || IncomingPortIndexKey(vswd.m_TargetPort) == IncomingPortIndexKey(targetPort)) && (allowOthers || LFPG_WireHelper.CanCreatorCutWire(vswd, cutPid, allowOthers)))
                {
                    LFPG_Util.Warn("[CutWires-Fallback] Found stale vanilla wire: " + vOwnId + " -> " + targetDeviceId + ":" + vswd.m_TargetPort);
                    LFPG_NetworkManager.Get().PlayerWireCountAdd(vswd.m_CreatorId, -1);
                    vwScan.Remove(vsw);
                    vSrcChanged = true;
                    rescued = rescued + 1;
                }
                vsw = vsw - 1;
            }

            if (vSrcChanged)
            {
                EntityAI vOwnerObj = LFPG_DeviceRegistry.Get().FindById(vOwnId);
                if (vOwnerObj)
                {
                    LFPG_NetworkManager.Get().BroadcastVanillaWires(vOwnId, vOwnerObj);
                }
                LFPG_NetworkManager.Get().MarkVanillaDirty();
            }
        }

        return rescued;
    }

    static void HandleRequestFullSync(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender) return;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
            return;

        LFPG_Util.Info("FullSync requested by pid=" + sender.GetPlainId());
        
        LFPG_NetworkManager.Get().SendFullSyncTo(player);

        // v4.5: Send server settings to client after FullSync.
        SendServerSettingsTo(player);
    }

    static void SendServerSettingsTo(PlayerBase target)
    {
        if (!target) return;

        LFPG_ServerSettings st = LFPG_Settings.Get();
        bool hideFlag = false;
        if (st)
        {
            hideFlag = st.HideCablesWithoutReel;
        }

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.SYNC_SERVER_SETTINGS);
        rpc.Write(hideFlag);
        rpc.Send(target, LFPG_RPC_CHANNEL, true, null);

        string logMsg = "[LFPG] Sent server settings: HideCablesWithoutReel=";
        logMsg = logMsg + hideFlag.ToString();
        LFPG_Util.Debug(logMsg);
    }

    // B-01: identity comes only from NetworkID. clientDeviceId is unused (read-compat).
    // out resolvedTarget keeps the same object for B-03 dirty (one resolve).
    static string ResolveDeviceSyncId(int netLow, int netHigh, string clientDeviceId, out EntityAI resolvedTarget)
    {
        resolvedTarget = null;
        string serverDeviceId = "";
        if (netLow == 0 && netHigh == 0)
            return serverDeviceId;

        EntityAI resolvedObj = EntityAI.Cast(g_Game.GetObjectByNetworkId(netLow, netHigh));
        if (!resolvedObj)
            return serverDeviceId;

        string resolvedId = LFPG_DeviceAPI.GetDeviceId(resolvedObj);
        if (resolvedId == "")
            return serverDeviceId;

        resolvedTarget = resolvedObj;
        serverDeviceId = resolvedId;
        return serverDeviceId;
    }

    // B-02: SYNC uses cull+20. Direct owner check first; wire targets only if that fails
    // (same interest as BroadcastOwnerWiresDelta via LFPG_DeviceAPI.GetDeviceWires).
    static bool AuthorizeDeviceSync(PlayerBase player, int netLow, int netHigh, out EntityAI target, out string canonicalDeviceId)
    {
        target = null;
        canonicalDeviceId = "";
        if (!player)
            return false;

        EntityAI resolvedTarget;
        string unusedClientDeviceId = "";
        string serverDeviceId = ResolveDeviceSyncId(netLow, netHigh, unusedClientDeviceId, resolvedTarget);
        if (serverDeviceId == "")
            return false;
        if (!resolvedTarget)
            return false;
        if (resolvedTarget.IsRuined())
            return false;

        float syncRadius = LFPG_CULL_DISTANCE_M + 20.0;
        float syncRadiusSq = syncRadius * syncRadius;
        float distanceSq = LFPG_WorldUtil.DistSq(player.GetPosition(), resolvedTarget.GetPosition());
        if (distanceSq > syncRadiusSq)
        {
            if (!DeviceSyncEndpointInRange(player, resolvedTarget, syncRadiusSq))
                return false;
        }

        LFPG_DeviceRegistry.Get().Register(resolvedTarget, serverDeviceId);
        target = resolvedTarget;
        canonicalDeviceId = serverDeviceId;
        return true;
    }

    // Delta interest fallback: player <-> any GetDeviceWires target of the requested owner.
    protected static bool DeviceSyncEndpointInRange(PlayerBase player, EntityAI owner, float syncRadiusSq)
    {
        if (!player)
            return false;
        if (!owner)
            return false;

        array<ref LFPG_WireData> ownerWires = LFPG_DeviceAPI.GetDeviceWires(owner);
        if (!ownerWires)
            return false;

        LFPG_DeviceRegistry reg = LFPG_DeviceRegistry.Get();
        if (!reg)
            return false;

        vector playerPos = player.GetPosition();
        int tw;
        for (tw = 0; tw < ownerWires.Count(); tw = tw + 1)
        {
            LFPG_WireData wire = ownerWires[tw];
            if (!wire)
                continue;
            if (wire.m_TargetDeviceId == "")
                continue;

            EntityAI endpoint = reg.FindById(wire.m_TargetDeviceId);
            if (!endpoint)
                continue;
            if (LFPG_WorldUtil.DistSq(playerPos, endpoint.GetPosition()) <= syncRadiusSq)
                return true;
        }
        return false;
    }

    static void HandleRequestDeviceSync(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender)
            return;
        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
            return;

        int netLow = 0;
        int netHigh = 0;
        string clientDeviceId = "";
        if (!ctx.Read(netLow))
            return;
        if (!ctx.Read(netHigh))
            return;
        if (!ctx.Read(clientDeviceId))
            return;

        EntityAI syncTarget;
        string serverDeviceId;
        if (!AuthorizeDeviceSync(player, netLow, netHigh, syncTarget, serverDeviceId))
            return;

        LFPG_NetworkManager.Get().SendDeviceSyncTo(player, serverDeviceId);
    }

    static void HandleRequestDeviceSyncBatch(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender)
            return;
        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
            return;

        int requestCount = 0;
        if (!ctx.Read(requestCount))
            return;
        if (requestCount <= 0 || requestCount > LFPG_DEVICE_SYNC_BATCH_MAX)
            return;

        ref array<int> lows = new array<int>;
        ref array<int> highs = new array<int>;
        int i;
        for (i = 0; i < requestCount; i = i + 1)
        {
            int netLow = 0;
            int netHigh = 0;
            string clientDeviceId = "";
            if (!ctx.Read(netLow))
                return;
            if (!ctx.Read(netHigh))
                return;
            if (!ctx.Read(clientDeviceId))
                return;
            lows.Insert(netLow);
            highs.Insert(netHigh);
        }

        if (LFPG_PERFDIAG_ENABLED)
        {
            s_PerfDiagDeviceSyncBatchCount = s_PerfDiagDeviceSyncBatchCount + 1;
            string perfBatch = "LFPG_PERFDIAG resync_batch_accept count=";
            perfBatch = perfBatch + s_PerfDiagDeviceSyncBatchCount.ToString();
            perfBatch = perfBatch + " devices=";
            perfBatch = perfBatch + requestCount.ToString();
            perfBatch = perfBatch + " pid=";
            perfBatch = perfBatch + sender.GetPlainId();
            Print(perfBatch);
        }

        ref map<string, bool> sentDeviceIds = new map<string, bool>;
        ref map<string, bool> sentOwners = new map<string, bool>;
        int dirtyCount = 0;
        for (i = 0; i < requestCount; i = i + 1)
        {
            EntityAI batchEntity;
            string serverDeviceId;
            if (!AuthorizeDeviceSync(player, lows[i], highs[i], batchEntity, serverDeviceId))
                continue;
            if (serverDeviceId == "")
                continue;
            if (sentDeviceIds.Contains(serverDeviceId))
                continue;
            sentDeviceIds[serverDeviceId] = true;
            LFPG_NetworkManager.Get().SendDeviceSyncToBatched(player, serverDeviceId, sentOwners);

            // AMENDMENT-1 (accept W3-F02 rev): one-shot SyncVar re-push for the JIP
            // batch. Bubble entry may deliver default SyncVars (v1.1 JIP bug) and T1
            // removed the periodic dirties that masked it. Bounded by
            // LFPG_DEVICE_SYNC_BATCH_MAX and deduped per device; the client-side
            // generation predicate keeps the resync feedback loop from re-forming.
            // Steady-state resync remains replication-free.
            // B-03: dirty only the EntityAI already authorized for this entry.
            if (batchEntity)
            {
                batchEntity.SetSynchDirty();
                dirtyCount = dirtyCount + 1;
            }
        }

        if (LFPG_PERFDIAG_ENABLED)
        {
            string perfDirty = "LFPG_PERFDIAG resync_batch_dirty count=";
            perfDirty = perfDirty + dirtyCount.ToString();
            perfDirty = perfDirty + " pid=";
            perfDirty = perfDirty + sender.GetPlainId();
            Print(perfDirty);
        }
    }

    static void HandleDiagClientLog(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!LFPG_DIAG_ENABLED)
            return;

        if (!sender)
            return;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
            return;

        string msg;
        if (!ctx.Read(msg))
            return;

        if (msg.Length() > 512)
            msg = msg.Substring(0, 512);

        string sanitized = SanitizeDiagClientLog(msg);
        LFPG_Util.Info("[CLI-ECHO] " + sanitized);
    }

    // B-23: drop control by range. ToAscii is first-char ASCII (enstring.c). Cap already applied.
    protected static string SanitizeDiagClientLog(string msg)
    {
        if (msg == "")
            return "";

        string sanitized = "";
        int msgLen = msg.Length();
        int ci;
        for (ci = 0; ci < msgLen; ci = ci + 1)
        {
            string ch = msg.Substring(ci, 1);
            int code = ch.ToAscii();
            if (code < 32)
                continue;
            if (code == 127)
                continue;
            sanitized = sanitized + ch;
        }
        return sanitized;
    }

    static void HandleInspectDevice(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx, int policyId)
    {
        if (!LFPG_RPCGuard.Admit(policyId, player, sender))
            return;

        // v0.7.43: Read NetworkID + client deviceId (correlation)
        int netLow = 0;
        if (!ctx.Read(netLow))
        {
            LFPG_Util.Warn("[SERVER] InspectDevice: read netLow FAIL pid=" + sender.GetPlainId());
            return;
        }
        int netHigh = 0;
        if (!ctx.Read(netHigh))
        {
            LFPG_Util.Warn("[SERVER] InspectDevice: read netHigh FAIL pid=" + sender.GetPlainId());
            return;
        }
        string clientDeviceId = "";
        if (!ctx.Read(clientDeviceId))
        {
            LFPG_Util.Warn("[SERVER] InspectDevice: read clientDeviceId FAIL pid=" + sender.GetPlainId());
            return;
        }

        if (clientDeviceId == "")
            return;

        EntityAI resolvedTarget;
        string serverDeviceId;
        if (!LFPG_RPCGuard.Authorize(policyId, player, sender, netLow, netHigh, resolvedTarget, serverDeviceId))
            return;

        // Re-register only after the server-side identity is authorized.
        LFPG_DeviceRegistry.Get().Register(resolvedTarget, serverDeviceId);

        ref array<ref LFPG_InspectWireEntry> entries = new array<ref LFPG_InspectWireEntry>;
        LFPG_InspectWireEntry entry;

        LFPG_ElecGraph graph = LFPG_NetworkManager.Get().GetGraph();
        if (graph)
        {
            // Query graph with SERVER's authoritative deviceId
            array<ref LFPG_ElecEdge> outEdges = graph.GetOutgoing(serverDeviceId);
            if (outEdges)
            {
                int oi;
                for (oi = 0; oi < outEdges.Count(); oi = oi + 1)
                {
                    LFPG_ElecEdge oEdge = outEdges[oi];
                    if (!oEdge)
                        continue;

                    entry = new LFPG_InspectWireEntry();
                    entry.m_Direction = LFPG_PortDir.OUT;
                    entry.m_LocalPort = oEdge.m_SourcePort;
                    entry.m_RemoteTypeName = ResolveTypeName(oEdge.m_TargetNodeId);

                    // v1.0: Binary edge state for inspector
                    entry.m_AllocatedPower = oEdge.m_AllocatedPower;
                    if (oEdge.m_AllocatedPower < LFPG_PROPAGATION_EPSILON && oEdge.m_Demand > LFPG_PROPAGATION_EPSILON)
                    {
                        entry.m_EdgeState = 2;
                    }
                    else
                    {
                        entry.m_EdgeState = 0;
                    }

                    entries.Insert(entry);
                }
            }

            // Incoming edges: this device is TARGET, remote is SOURCE
            array<ref LFPG_ElecEdge> inEdges = graph.GetIncoming(serverDeviceId);
            if (inEdges)
            {
                int ii;
                for (ii = 0; ii < inEdges.Count(); ii = ii + 1)
                {
                    LFPG_ElecEdge iEdge = inEdges[ii];
                    if (!iEdge)
                        continue;

                    entry = new LFPG_InspectWireEntry();
                    entry.m_Direction = LFPG_PortDir.IN;
                    entry.m_LocalPort = iEdge.m_TargetPort;
                    entry.m_RemoteTypeName = ResolveTypeName(iEdge.m_SourceNodeId);

                    // v1.0: Binary edge state for inspector
                    entry.m_AllocatedPower = iEdge.m_AllocatedPower;
                    if (iEdge.m_AllocatedPower < LFPG_PROPAGATION_EPSILON && iEdge.m_Demand > LFPG_PROPAGATION_EPSILON)
                    {
                        entry.m_EdgeState = 2;
                    }
                    else
                    {
                        entry.m_EdgeState = 0;
                    }

                    entries.Insert(entry);
                }
            }
        }

        // Send response with CLIENT's deviceId as correlation key
        // (client uses this to detect stale responses)
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.INSPECT_RESPONSE);
        rpc.Write(LFPG_InspectWireEntry.SCHEMA_VERSION);
        rpc.Write(clientDeviceId);

        int wireCount = entries.Count();
        rpc.Write(wireCount);

        int wi;
        for (wi = 0; wi < wireCount; wi = wi + 1)
        {
            LFPG_InspectWireEntry we = entries[wi];
            rpc.Write(we.m_Direction);
            rpc.Write(we.m_LocalPort);
            rpc.Write(we.m_RemoteTypeName);
            rpc.Write(we.m_AllocatedPower);
            rpc.Write(we.m_EdgeState);
        }

        rpc.Send(player, LFPG_RPC_CHANNEL, true, sender);

        string dbgSent = "[SERVER] InspectDevice: sent ";
        dbgSent = dbgSent + wireCount.ToString();
        dbgSent = dbgSent + " wires for ";
        dbgSent = dbgSent + clientDeviceId;
        if (serverDeviceId != clientDeviceId)
        {
            dbgSent = dbgSent + " (resolved=" + serverDeviceId + ")";
        }
        LFPG_Util.Debug(dbgSent);
    }

    static string ResolveTypeName(string deviceId)
    {
        if (deviceId == "")
            return "";

        EntityAI remoteObj = LFPG_DeviceRegistry.Get().FindById(deviceId);
        if (remoteObj)
        {
            return remoteObj.GetType();
        }
        return "";
    }

    static void HandleSorterConfigRequest(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx, int responseSubId)
    {
        if (!sender)
            return;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            PlayerBase.LFPG_SendClientMsg(player, "Too fast! Wait a moment.");
            return;
        }

        int netLow = 0;
        int netHigh = 0;
        if (!ctx.Read(netLow))
            return;
        if (!ctx.Read(netHigh))
            return;

        // Resolve sorter by NetworkID
        EntityAI devEnt = EntityAI.Cast(g_Game.GetObjectByNetworkId(netLow, netHigh));
        if (!devEnt)
        {
            LFPG_Util.Warn("[SorterConfigRequest] entity not found");
            return;
        }

        LFPG_Sorter sorter = LFPG_Sorter.Cast(devEnt);
        if (!sorter)
        {
            LFPG_Util.Warn("[SorterConfigRequest] entity is not LFPG_Sorter");
            return;
        }

        // Proximity check (match ActionCondition distance)
        float dist = vector.Distance(player.GetPosition(), devEnt.GetPosition());
        if (dist > LFPG_INTERACT_DIST_M)
        {
            LFPG_Util.Warn("[SorterConfigRequest] player too far");
            return;
        }

        // Ruined check
        if (sorter.IsRuined())
        {
            LFPG_Util.Warn("[SorterConfigRequest] sorter is ruined");
            return;
        }

        // Powered check
        if (!sorter.LFPG_IsPowered())
        {
            PlayerBase.LFPG_SendClientMsg(player, "Sorter has no power.");
            return;
        }

        // Gather payload: filterJSON
        string filterJSON = sorter.LFPG_GetFilterJSON();

        // Resolve linked container name
        string containerName = "";
        EntityAI linkedCont = sorter.LFPG_GetLinkedContainer();
        if (linkedCont)
        {
            containerName = linkedCont.GetDisplayName();
        }

        // Resolve dest container names via wire topology (6 outputs)
        // For each output port, find the wire, follow to target Sorter,
        // then get that Sorter's linked container type name.
        // Hoist all loop variables before the loop (Enforce Script).
        string destName0 = "";
        string destName1 = "";
        string destName2 = "";
        string destName3 = "";
        string destName4 = "";
        string destName5 = "";

        array<ref LFPG_WireData> wires = sorter.LFPG_GetWires();
        if (wires)
        {
            int wi = 0;
            int wCount = wires.Count();
            int oi = 0;
            string portName = "";
            int portNum = 0;
            EntityAI targetEnt = null;
            LFPG_Sorter targetSorter = null;
            EntityAI destCont = null;
            string resolvedName = "";
            LFPG_WireData wd = null;

            for (oi = 0; oi < 6; oi = oi + 1)
            {
                portNum = oi + 1;
                portName = "output_" + portNum.ToString();
                resolvedName = "";

                for (wi = 0; wi < wCount; wi = wi + 1)
                {
                    wd = wires[wi];
                    if (!wd)
                        continue;

                    if (wd.m_SourcePort != portName)
                        continue;

                    // Found wire for this output port â€” resolve target
                    targetEnt = LFPG_DeviceAPI.ResolveByNetworkId(wd.m_TargetNetLow, wd.m_TargetNetHigh);
                    if (!targetEnt)
                    {
                        // Fallback: try DeviceRegistry by ID
                        targetEnt = LFPG_DeviceRegistry.Get().FindById(wd.m_TargetDeviceId);
                    }
                    if (!targetEnt)
                        break;

                    targetSorter = LFPG_Sorter.Cast(targetEnt);
                    if (!targetSorter)
                        break;

                    destCont = targetSorter.LFPG_GetLinkedContainer();
                    if (destCont)
                    {
                        resolvedName = destCont.GetDisplayName();
                    }
                    break;
                }

                // Assign to the correct dest slot
                if (oi == 0) { destName0 = resolvedName; }
                else if (oi == 1) { destName1 = resolvedName; }
                else if (oi == 2) { destName2 = resolvedName; }
                else if (oi == 3) { destName3 = resolvedName; }
                else if (oi == 4) { destName4 = resolvedName; }
                else if (oi == 5) { destName5 = resolvedName; }
            }
        }

        // Build and send CONFIG_RESPONSE
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)responseSubId);  // Sprint 0: parametrized â€” V3=SORTER_CONFIG_RESPONSE / V4=SORTER_TEST_CONFIG_RESPONSE
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Write(filterJSON);
        rpc.Write(containerName);
        rpc.Write(destName0);
        rpc.Write(destName1);
        rpc.Write(destName2);
        rpc.Write(destName3);
        rpc.Write(destName4);
        rpc.Write(destName5);
        rpc.Send(player, LFPG_RPC_CHANNEL, true, sender);

        string logMsg = "[SorterConfigRequest] Sent config for ";
        logMsg = logMsg + sorter.LFPG_GetDeviceId();
        logMsg = logMsg + " container=" + containerName;
        LFPG_Util.Info(logMsg);
    }

    static void HandleSorterConfigSave(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx, int responseSubId)
    {
        if (!sender)
            return;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            PlayerBase.LFPG_SendClientMsg(player, "Too fast! Wait a moment.");
            return;
        }

        int netLow = 0;
        int netHigh = 0;
        string filterJSON = "";
        if (!ctx.Read(netLow))
            return;
        if (!ctx.Read(netHigh))
            return;
        if (!ctx.Read(filterJSON))
            return;

        // Input hardening: reject oversized JSON
        if (filterJSON.Length() > 4096)
        {
            LFPG_Util.Warn("[SorterConfigSave] rejected: JSON too large (" + filterJSON.Length().ToString() + ")");
            return;
        }

        // Resolve sorter
        EntityAI devEnt = EntityAI.Cast(g_Game.GetObjectByNetworkId(netLow, netHigh));
        if (!devEnt)
        {
            LFPG_Util.Warn("[SorterConfigSave] entity not found");
            return;
        }

        LFPG_Sorter sorter = LFPG_Sorter.Cast(devEnt);
        if (!sorter)
        {
            LFPG_Util.Warn("[SorterConfigSave] entity is not LFPG_Sorter");
            return;
        }

        // Proximity check (match ActionCondition distance)
        float dist = vector.Distance(player.GetPosition(), devEnt.GetPosition());
        if (dist > LFPG_INTERACT_DIST_M)
        {
            LFPG_Util.Warn("[SorterConfigSave] player too far");
            return;
        }

        // Powered check â€” don't allow config changes on unpowered device
        if (!sorter.LFPG_IsPowered())
        {
            LFPG_Util.Warn("[SorterConfigSave] sorter not powered");
            return;
        }

        // Store config â€” returns false if JSON is malformed (M3 validation)
        bool saveOk = sorter.LFPG_SetFilterJSON(filterJSON);

        // H4: Send ACK back to client
        ScriptRPC ackRpc = new ScriptRPC();
        int ackSubId = responseSubId;  // Sprint 0: parametrized â€” V3=SORTER_SAVE_ACK / V4=SORTER_TEST_SAVE_ACK
        ackRpc.Write(ackSubId);
        ackRpc.Write(saveOk);
        ackRpc.Send(player, LFPG_RPC_CHANNEL, true, sender);

        if (!saveOk)
        {
            LFPG_Util.Warn("[SorterConfigSave] rejected malformed JSON from client");
            return;
        }

        string logMsg = "[SorterConfigSave] Updated config for ";
        logMsg = logMsg + sorter.LFPG_GetDeviceId();
        LFPG_Util.Info(logMsg);
    }

    static void HandleSorterRequestSort(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx, int responseSubId)
    {
        if (!sender)
            return;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            PlayerBase.LFPG_SendClientMsg(player, "Too fast! Wait a moment.");
            return;
        }

        int netLow = 0;
        int netHigh = 0;
        if (!ctx.Read(netLow))
            return;
        if (!ctx.Read(netHigh))
            return;

        // Resolve sorter
        EntityAI devEnt = EntityAI.Cast(g_Game.GetObjectByNetworkId(netLow, netHigh));
        if (!devEnt)
        {
            LFPG_Util.Warn("[SorterRequestSort] entity not found");
            return;
        }

        LFPG_Sorter sorter = LFPG_Sorter.Cast(devEnt);
        if (!sorter)
        {
            LFPG_Util.Warn("[SorterRequestSort] entity is not LFPG_Sorter");
            return;
        }

        // Proximity check
        float dist = vector.Distance(player.GetPosition(), devEnt.GetPosition());
        if (dist > LFPG_INTERACT_DIST_M)
        {
            LFPG_Util.Warn("[SorterRequestSort] player too far");
            return;
        }

        // Delegate to NetworkManager (handles powered + container checks)
        // v5.0: Pass sender ID so cargo refresh broadcast excludes requester
        string senderPlayerId = "";
        if (sender)
        {
            senderPlayerId = sender.GetId();
        }
        int sortMoved = LFPG_NetworkManager.Get().HandleSorterRequestSort(sorter, senderPlayerId);
        bool sortOk = (sortMoved >= 0);

        // Send ACK only to requesting player (not broadcast)
        ScriptRPC sortAckRpc = new ScriptRPC();
        int sortAckSubId = responseSubId;  // Sprint 0: parametrized â€” V3=SORTER_SORT_ACK / V4=SORTER_TEST_SORT_ACK
        sortAckRpc.Write(sortAckSubId);
        sortAckRpc.Write(sortOk);
        sortAckRpc.Write(sortMoved);
        sortAckRpc.Send(player, LFPG_RPC_CHANNEL, true, sender);
    }

    static void HandleSorterResync(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx, int responseSubId)
    {
        if (!sender)
            return;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
            return;

        int netLow = 0;
        int netHigh = 0;
        if (!ctx.Read(netLow))
            return;
        if (!ctx.Read(netHigh))
            return;

        EntityAI devEnt = EntityAI.Cast(g_Game.GetObjectByNetworkId(netLow, netHigh));
        if (!devEnt)
        {
            string warnNotFound = "[SorterResync] entity not found";
            LFPG_Util.Warn(warnNotFound);
            return;
        }

        LFPG_Sorter sorter = LFPG_Sorter.Cast(devEnt);
        if (!sorter)
        {
            string warnNotSorter = "[SorterResync] entity is not LFPG_Sorter";
            LFPG_Util.Warn(warnNotSorter);
            return;
        }

        // Proximity check
        float dist = vector.Distance(player.GetPosition(), devEnt.GetPosition());
        if (dist > LFPG_INTERACT_DIST_M)
        {
            string warnFar = "[SorterResync] player too far";
            LFPG_Util.Warn(warnFar);
            return;
        }

        if (sorter.IsRuined())
            return;

        if (!sorter.LFPG_IsPowered())
            return;

        EntityAI currentLinked = sorter.LFPG_GetLinkedContainer();
        EntityAI candidate = sorter.LFPG_FindNearestContainerCandidate(LFPG_SORTER_LINK_RADIUS);
        int ackStatus = LFPG_SORTER_ACK_NONE;
        string containerName = "";

        if (!candidate)
        {
            if (currentLinked)
            {
                containerName = currentLinked.GetDisplayName();
            }
            ackStatus = LFPG_SORTER_ACK_NONE;
        }
        else if (candidate == currentLinked)
        {
            containerName = candidate.GetDisplayName();
            ackStatus = LFPG_SORTER_ACK_KEPT_OLD;
        }
        else
        {
            sorter.LFPG_UnlinkContainer();
            sorter.LFPG_LinkContainer(candidate);
            containerName = candidate.GetDisplayName();
            ackStatus = LFPG_SORTER_ACK_REPLACED;
        }

        ScriptRPC ackRpc = new ScriptRPC();
        int ackSubId = responseSubId;  // Sprint 0: parametrized V3/V4 ACK subId
        ackRpc.Write(ackSubId);
        ackRpc.Write(ackStatus);
        ackRpc.Write(containerName);
        ackRpc.Send(player, LFPG_RPC_CHANNEL, true, sender);

        string logMsg = "[SorterResync] status=";
        logMsg = logMsg + ackStatus.ToString();
        logMsg = logMsg + " result=";
        logMsg = logMsg + containerName;
        LFPG_Util.Info(logMsg);
    }

    static void HandleSorterPreviewRequest(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx, int responseSubId)
    {
        if (!sender)
            return;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
            return;

        int netLow = 0;
        int netHigh = 0;
        int selectedOutput = 0;
        string clientJSON = "";
        if (!ctx.Read(netLow))
            return;
        if (!ctx.Read(netHigh))
            return;
        if (!ctx.Read(selectedOutput))
            return;
        // v4.1: Client sends current UI config for live preview
        if (!ctx.Read(clientJSON))
            return;

        // Validate output index
        if (selectedOutput < 0 || selectedOutput >= 6)
            return;

        // v4.1: Validate client JSON length (prevent oversized payloads)
        int clientJSONLen = clientJSON.Length();
        if (clientJSONLen > LFPG_SORT_MAX_JSON_BYTES)
            return;

        // From this point, always send a response (even if empty).
        // Silent return would leave the client waiting with stale data.
        int totalMatched = 0;
        int sentCount = 0;
        array<string> matchNames = new array<string>;
        array<string> matchCats = new array<string>;
        // v4.3: Changed from array<int> slotSize to array<string> formatted info
        // Format: "WxH" for qty<=1, "WxH xQ" for qty>1
        array<string> matchInfo = new array<string>;

        // Resolve sorter
        EntityAI devEnt = EntityAI.Cast(g_Game.GetObjectByNetworkId(netLow, netHigh));
        LFPG_Sorter sorter = null;
        if (devEnt)
        {
            sorter = LFPG_Sorter.Cast(devEnt);
        }

        bool canProceed = false;
        if (sorter)
        {
            float dist = vector.Distance(player.GetPosition(), devEnt.GetPosition());
            if (dist <= LFPG_INTERACT_DIST_M)
            {
                canProceed = true;
            }
        }

        if (canProceed)
        {
            // v4.1: Parse filter config from CLIENT payload (live UI rules)
            // instead of sorter.LFPG_GetFilterJSON() (persisted, requires SAVE).
            // If parse fails, config stays empty â†’ hasRules=false â†’ 0 items sent.
            LFPG_SortConfig config = new LFPG_SortConfig();
            bool parseOk = config.FromJSON(clientJSON);
            if (!parseOk)
            {
                string wParse = "[SorterPreviewRequest] client JSON parse failed";
                LFPG_Util.Warn(wParse);
            }
            LFPG_SortOutputConfig outCfg = config.GetOutput(selectedOutput);

            bool isCatchAll = false;
            bool hasRules = false;
            if (outCfg)
            {
                isCatchAll = outCfg.m_IsCatchAll;
                int ruleCount = outCfg.GetRuleCount();
                hasRules = (ruleCount > 0 || isCatchAll);
            }

            // Resolve linked container
            EntityAI container = sorter.LFPG_GetLinkedContainer();

            if (container && hasRules)
            {
                GameInventory inv = container.GetInventory();
                if (inv)
                {
                    CargoBase cargo = inv.GetCargo();
                    if (cargo)
                    {
                        int cargoCount = cargo.GetItemCount();
                        int ci = 0;
                        EntityAI cItem = null;
                        bool matched = false;
                        string typeName = "";
                        string cat = "";
                        // v4.3: Preview sends formatted info string instead of int slotSize
                        int infoW = 0;
                        int infoH = 0;
                        int infoQty = 0;
                        string infoStr = "";

                        for (ci = 0; ci < cargoCount; ci = ci + 1)
                        {
                            cItem = cargo.GetItem(ci);
                            if (!cItem)
                                continue;

                            matched = false;
                            if (isCatchAll)
                            {
                                matched = true;
                            }
                            else
                            {
                                matched = LFPG_SorterLogic.MatchesAnyRule(cItem, outCfg);
                            }

                            if (!matched)
                                continue;

                            totalMatched = totalMatched + 1;

                            // Only collect up to cap for the RPC payload
                            if (matchNames.Count() < LFPG_SORTER_PREVIEW_CAP)
                            {
                                typeName = cItem.GetType();
                                cat = LFPG_SorterLogic.ResolveCategory(cItem);
                                // v4.3: Compute dimensions + quantity
                                LFPG_SorterLogic.GetItemSlotDimensions(cItem, infoW, infoH);
                                float fQty = cItem.GetQuantity();
                                infoQty = fQty;
                                if (infoQty < 1)
                                {
                                    infoQty = 1;
                                }
                                // Format: "WxH" or "WxH xQ"
                                infoStr = infoW.ToString();
                                infoStr = infoStr + "x";
                                infoStr = infoStr + infoH.ToString();
                                if (infoQty > 1)
                                {
                                    infoStr = infoStr + " x";
                                    infoStr = infoStr + infoQty.ToString();
                                }
                                matchNames.Insert(typeName);
                                matchCats.Insert(cat);
                                matchInfo.Insert(infoStr);
                            }
                        }
                    }
                }
            }
        }

        // Always send response (empty if guards failed)
        sentCount = matchNames.Count();
        ScriptRPC rpc = new ScriptRPC();
        int respSubId = responseSubId;  // Sprint 0: parametrized â€” V3=SORTER_PREVIEW_RESPONSE / V4=SORTER_TEST_PREVIEW_RESPONSE
        rpc.Write(respSubId);
        rpc.Write(selectedOutput);
        rpc.Write(totalMatched);
        rpc.Write(sentCount);

        int si = 0;
        for (si = 0; si < sentCount; si = si + 1)
        {
            rpc.Write(matchNames[si]);
            rpc.Write(matchCats[si]);
            rpc.Write(matchInfo[si]);
        }

        bool bRpcGuaranteed = true;
        rpc.Send(player, LFPG_RPC_CHANNEL, bRpcGuaranteed, sender);

        if (LFPG_PERFDIAG_ENABLED)
        {
            s_PerfDiagPreviewResponseCount = s_PerfDiagPreviewResponseCount + 1;
            string sorterId = "";
            if (sorter)
            {
                sorterId = LFPG_DeviceAPI.GetDeviceId(sorter);
            }
            string perfPreview = "LFPG_PERFDIAG preview_response count=";
            perfPreview = perfPreview + s_PerfDiagPreviewResponseCount.ToString();
            perfPreview = perfPreview + " deviceId=";
            perfPreview = perfPreview + sorterId;
            perfPreview = perfPreview + " matched=";
            perfPreview = perfPreview + totalMatched.ToString();
            perfPreview = perfPreview + " sent=";
            perfPreview = perfPreview + sentCount.ToString();
            Print(perfPreview);
        }

        string logMsg = "[SorterPreviewRequest] output=";
        logMsg = logMsg + selectedOutput.ToString();
        logMsg = logMsg + " matched=";
        logMsg = logMsg + totalMatched.ToString();
        logMsg = logMsg + " sent=";
        logMsg = logMsg + sentCount.ToString();
        LFPG_Util.Info(logMsg);
    }

};
