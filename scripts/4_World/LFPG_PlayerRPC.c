// =========================================================
// LF_PowerGrid - PlayerBase RPC routing (server authoritative)
// v0.7.20: Sprint 4.2 S2 — H2 fix: correct bulk mutation order in CutWires/CutPort
// v0.7.34: Bloque E — Atomic graph mutations in FinishWiring replace phase.
//   Graph notified on wire removal (was missing → stale edges).
//   Begin/EndGraphMutation wraps remove+add for same-target safety.
// v0.7.38 (Race Condition fixes):
//   RC-01: Port locking prevents concurrent FinishWiring on same dest port.
//   RC-06: Pre-check capacity before replacement phase (transactional safety).
//   RC-07: Reject wiring RPCs during first ~5s startup validation window.
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
            Print("[LF_PowerGrid] OnSelectPlayer skipped (CCTV spectator transition)");
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
        if (subId == LFPG_RPC_SubId.FINISH_WIRING)
        {
            HandleLFPG_FinishWiring(sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.CUT_WIRES)
        {
            HandleLFPG_CutWires(sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.CUT_PORT)
        {
            HandleLFPG_CutPort(sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.REQUEST_FULL_SYNC)
        {
            HandleLFPG_RequestFullSync(sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.DIAG_CLIENT_LOG)
        {
            HandleLFPG_DiagClientLog(sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.REQUEST_DEVICE_SYNC)
        {
            HandleLFPG_RequestDeviceSync(sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.INSPECT_DEVICE)
        {
            HandleLFPG_InspectDevice(sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.CAMERA_CYCLE)
        {
            HandleLFPG_CameraLink(sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.CAMERA_UNLINK)
        {
            HandleLFPG_CameraUnlink(sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.REQUEST_CAMERA_LIST)
        {
            HandleLFPG_RequestCameraList(sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.CCTV_EXIT_REQUEST)
        {
            HandleLFPG_CCTVExitRequest(sender);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_CONFIG_REQUEST)
        {
            HandleLFPG_SorterConfigRequest(sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_CONFIG_SAVE)
        {
            HandleLFPG_SorterConfigSave(sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_REQUEST_SORT)
        {
            HandleLFPG_SorterRequestSort(sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_RESYNC)
        {
            HandleLFPG_SorterResync(sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_PREVIEW_REQUEST)
        {
            HandleLFPG_SorterPreviewRequest(sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.SEARCHLIGHT_AIM)
        {
            HandleLFPG_SearchlightAim(sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.SEARCHLIGHT_ENTER)
        {
            HandleLFPG_SearchlightEnter(sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.SEARCHLIGHT_EXIT_REQUEST)
        {
            HandleLFPG_SearchlightExit(sender, ctx);
        }
        #else
        if (subId == LFPG_RPC_SubId.SYNC_OWNER_WIRES)
        {
            HandleLFPG_SyncOwnerWires(sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.CLIENT_MSG)
        {
            HandleLFPG_ClientMsg(ctx);
        }
        else if (subId == LFPG_RPC_SubId.INSPECT_RESPONSE)
        {
            HandleLFPG_InspectResponse(ctx);
        }
        else if (subId == LFPG_RPC_SubId.CAMERA_LIST_RESPONSE)
        {
            HandleLFPG_CameraListResponse(ctx);
        }
        else if (subId == LFPG_RPC_SubId.CCTV_EXIT_CONFIRM)
        {
            HandleLFPG_CCTVExitConfirm();
        }
        else if (subId == LFPG_RPC_SubId.SORTER_CONFIG_RESPONSE)
        {
            HandleLFPG_SorterConfigResponse(ctx);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_SAVE_ACK)
        {
            HandleLFPG_SorterSaveAck(ctx);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_RESYNC_ACK)
        {
            HandleLFPG_SorterResyncAck(ctx);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_PREVIEW_RESPONSE)
        {
            HandleLFPG_SorterPreviewResponse(ctx);
        }
        else if (subId == LFPG_RPC_SubId.SORTER_SORT_ACK)
        {
            HandleLFPG_SorterSortAck(ctx);
        }
        else if (subId == LFPG_RPC_SubId.SEARCHLIGHT_ENTER_CONFIRM)
        {
            HandleLFPG_SearchlightEnterConfirm(ctx);
        }
        else if (subId == LFPG_RPC_SubId.SEARCHLIGHT_EXIT_CONFIRM)
        {
            HandleLFPG_SearchlightExitConfirm();
        }
        #endif
    }

    // =====================================
    // CLIENT: show server message to player
    // =====================================
    protected void HandleLFPG_ClientMsg(ParamsReadContext ctx)
    {
        string msg;
        if (!ctx.Read(msg)) return;

        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (player)
        {
            player.MessageStatus("[LFPG] " + msg);
        }
    }

    // =====================================
    // SERVER: send message to a specific player
    // =====================================
    static void LFPG_SendClientMsg(PlayerBase target, string msg)
    {
        if (!target) return;

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.CLIENT_MSG);
        rpc.Write(msg);
        rpc.Send(target, LFPG_RPC_CHANNEL, true, null);
    }

    // =====================================
    // SERVER: Finish wiring request
    // =====================================
    protected void HandleLFPG_FinishWiring(PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender) return;

        LFPG_Util.Debug("[FinishWiring-Server] RPC received from pid=" + sender.GetPlainId());

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (rate limited)");
            LFPG_SendClientMsg(this, "Too fast! Wait a moment.");
            return;
        }

        // v0.7.38 (RC-07): Reject during startup validation window.
        // ValidateAllWiresAndPropagate runs at T+5s and does a full rebuild.
        // Wires created before that would be overwritten, causing flicker.
        if (!LFPG_NetworkManager.Get().IsStartupValidationDone())
        {
            LFPG_Util.Info("[FinishWiring-Server] denied (startup validation pending)");
            LFPG_SendClientMsg(this, "Server starting, please wait...");
            return;
        }

        if (!LFPG_WorldUtil.PlayerHasCableReelInHands(this))
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (no cable reel)");
            LFPG_SendClientMsg(this, "You need a cable reel in your hands.");
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
                LFPG_SendClientMsg(this, "Too many waypoints.");
                return;
            }

            if (!LFPG_WireHelper.ValidateWaypoints(waypoints, "FinishWiring-RPC", dstDeviceId))
            {
                LFPG_Util.Warn("[FinishWiring-Server] denied (corrupt waypoints from RPC)");
                LFPG_SendClientMsg(this, "Invalid wire path.");
                return;
            }
        }

        // Resolve objects by network ID
        EntityAI srcObj = EntityAI.Cast(GetGame().GetObjectByNetworkId(srcLow, srcHigh));
        EntityAI dstObj = EntityAI.Cast(GetGame().GetObjectByNetworkId(dstLow, dstHigh));
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
            LFPG_SendClientMsg(this, "Source device must be placed in the world.");
            return;
        }
        if (dstObj.GetHierarchyParent())
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (dst in inventory/cargo)");
            LFPG_SendClientMsg(this, "Target device must be placed in the world.");
            return;
        }

        // Distance check: player must be near at least one end of the wire
        float distToSrc = vector.Distance(this.GetPosition(), srcObj.GetPosition());
        float distToDst = vector.Distance(this.GetPosition(), dstObj.GetPosition());
        float nearestDist = distToSrc;
        if (distToDst < nearestDist)
        {
            nearestDist = distToDst;
        }
        if (nearestDist > 4.0)
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (too far nearest=" + nearestDist.ToString() + "m)");
            LFPG_SendClientMsg(this, "Too far from device.");
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
            LFPG_SendClientMsg(this, "Too far from remote device.");
            return;
        }

        // Universal validation: source must be energy source, dest must be consumer
        if (!LFPG_DeviceAPI.IsEnergySource(srcObj))
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (src not energy source) type=" + srcObj.GetType());
            LFPG_SendClientMsg(this, "Source is not a power generator.");
            return;
        }
        if (!LFPG_DeviceAPI.IsEnergyConsumer(dstObj))
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (dst not consumer) type=" + dstObj.GetType());
            LFPG_SendClientMsg(this, "Target is not an electrical device.");
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
            LFPG_SendClientMsg(this, "Cannot connect device to itself.");
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
            LFPG_SendClientMsg(this, preResult.m_Reason);
            return;
        }

        // Quota check
        string quotaReason;
        if (!LFPG_NetworkManager.Get().CanPlayerCreateAnotherWire(sender, quotaReason))
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (" + quotaReason + ")");
            LFPG_SendClientMsg(this, "Wire limit reached: " + quotaReason);
            return;
        }

        // Port validation: only for LFPG-native devices (vanilla has no ports)
        bool srcIsLFPG = (LFPG_DeviceAPI.GetDeviceId(srcObj) != "");
        bool dstIsLFPG = (LFPG_DeviceAPI.GetDeviceId(dstObj) != "");

        if (srcIsLFPG)
        {
            if (!LFPG_DeviceAPI.HasPort(srcObj, srcPort, LFPG_PortDir.OUT))
            {
                LFPG_Util.Warn("[FinishWiring-Server] denied (src missing port " + srcPort + ")");
                LFPG_SendClientMsg(this, "Invalid source port.");
                return;
            }
        }
        if (dstIsLFPG)
        {
            if (!LFPG_DeviceAPI.HasPort(dstObj, dstPort, LFPG_PortDir.IN))
            {
                LFPG_Util.Warn("[FinishWiring-Server] denied (dst missing port " + dstPort + ")");
                LFPG_SendClientMsg(this, "Invalid target port.");
                return;
            }
        }

        // CanConnectTo only for LFPG sources (vanilla sources skip this)
        if (srcIsLFPG)
        {
            if (!LFPG_DeviceAPI.CanConnectTo(srcObj, dstObj, srcPort, dstPort))
            {
                LFPG_Util.Warn("[FinishWiring-Server] denied (CanConnectTo false)");
                LFPG_SendClientMsg(this, "Cannot connect these devices.");
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
            LFPG_SendClientMsg(this, "Invalid wire: " + reason);
            LFPG_ServerSettings st = LFPG_Settings.Get();
            if (st && st.KickOnInvalidWire)
            {
                GetGame().DisconnectPlayer(sender);
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
            LFPG_SendClientMsg(this, "Network too large. Cannot add more connections to this grid.");
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
            LFPG_SendClientMsg(this, "Connection rejected: would create an electrical loop.");
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
            LFPG_SendClientMsg(this, "Port busy, try again.");
            return;
        }
        LFPG_NetworkManager.Get().LockPort(portLockKey);

        // ============================================================
        // REPLACEMENT PHASE: remove ALL conflicting wires BEFORE adding
        // v0.7.34 (Bloque E): Atomic mutation — prevents premature node
        // deletion between remove + add (same-target replace bug).
        // ============================================================

        // v0.7.34: Begin atomic mutation batch
        LFPG_NetworkManager.Get().BeginGraphMutation();

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
                        srcWires.Remove(sw);
                        anyRemoved = true;
                    }
                    sw = sw - 1;
                }
            }
            if (anyRemoved)
            {
                srcObj.SetSynchDirty();
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

            // v0.7.38 (RC-06): If replacement removed wires but AddWire failed,
            // the graph and reverse index are inconsistent. Force a full rebuild
            // to restore data integrity from the authoritative wire arrays.
            if (anyRemoved)
            {
                LFPG_Util.Warn("[FinishWiring-Server] RC-06: store failed after replacement — forcing rebuild");
                LFPG_NetworkManager.Get().PostBulkRebuildAndPropagate();
            }

            LFPG_Util.Warn("[FinishWiring-Server] wire storage failed (duplicate or cap)");
            LFPG_SendClientMsg(this, "Wire already exists or device is full.");
            return;
        }

        LFPG_Util.Info("[FinishWiring-Server] SUCCESS: " + srcRealId + ":" + srcPort + " -> " + dstRealId + ":" + dstPort + " wps=" + wpCount.ToString());

        // Reverse index already updated incrementally above

        // Sync wire data to clients for cable rendering
        if (isLfpgOwner)
        {
            LFPG_NetworkManager.Get().BroadcastOwnerWires(EntityAI.Cast(srcObj));
        }
        else
        {
            LFPG_NetworkManager.Get().BroadcastVanillaWires(srcRealId, srcObj);
            // v0.7.33 (Fix #18): Mark vanilla store dirty for persistence.
            // Was missing — CutWires and CutPort both call this but FinishWiring didn't.
            // Without this, new vanilla wire is lost if server restarts before periodic flush.
            LFPG_NetworkManager.Get().MarkVanillaDirty();
        }

        // Propagate power to all consumers (LFPG and vanilla via SetPowered)
        // Sprint 4.2 S2: graph update first, then request propagation.
        // NotifyGraphWireAdded adds the edge and marks both endpoints dirty.
        // RequestPropagate additionally refreshes source state from entity.
        bool edgeAdded = LFPG_NetworkManager.Get().NotifyGraphWireAdded(srcRealId, dstRealId, srcPort, dstPort, wd);

        // v0.7.34 (Bloque E): Close atomic mutation batch.
        // All removes + the add are now committed atomically.
        // Deferred orphan cleanup runs here — nodes that lost edges
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
            LFPG_Util.Warn("[FinishWiring-Server] Graph edge not inserted (limit or missing node) — forcing rebuild");
            LFPG_NetworkManager.Get().PostBulkRebuildAndPropagate();
        }
        else
        {
            LFPG_NetworkManager.Get().RequestPropagate(srcRealId);
        }
    }

    // =====================================
    // SERVER: Cut wires request
    // =====================================
    protected void HandleLFPG_CutWires(PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender) return;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            LFPG_SendClientMsg(this, "Too fast! Wait a moment.");
            return;
        }

        // v0.7.38 (RC-07): Reject during startup validation window.
        if (!LFPG_NetworkManager.Get().IsStartupValidationDone())
        {
            LFPG_SendClientMsg(this, "Server starting, please wait...");
            return;
        }

        if (!LFPG_WorldUtil.PlayerHasPliersInHands(this))
        {
            LFPG_Util.Info("CutWires: denied (no pliers)");
            LFPG_SendClientMsg(this, "You need pliers in your hands.");
            return;
        }

        int low = 0;
        int high = 0;
        if (!ctx.Read(low)) return;
        if (!ctx.Read(high)) return;

        EntityAI obj = EntityAI.Cast(GetGame().GetObjectByNetworkId(low, high));
        if (!obj) return;

        if (vector.Distance(this.GetPosition(), obj.GetPosition()) > 4.0)
        {
            LFPG_SendClientMsg(this, "Too far from device.");
            return;
        }

        string deviceId = LFPG_DeviceAPI.GetOrCreateDeviceId(obj);
        if (deviceId == "") return;

        bool changed = false;
        LFPG_ServerSettings st = LFPG_Settings.Get();

        // Try LFPG wire-owning device first (Generator, Splitter, etc.)
        if (LFPG_DeviceAPI.HasWireStore(obj))
        {
            // Pre-scan wires for incremental reverse index + player count updates.
            // Must mirror exactly what ClearDeviceWires / ClearDeviceWiresForCreator will remove.
            ref array<ref LFPG_WireData> preWires = LFPG_DeviceAPI.GetDeviceWires(obj);
            if (preWires)
            {
                string cutPid = sender.GetPlainId();
                int pw;
                for (pw = 0; pw < preWires.Count(); pw = pw + 1)
                {
                    LFPG_WireData pwd = preWires[pw];
                    if (!pwd) continue;
                    // If restricted to own wires, process own + unclaimed (empty CreatorId)
                    // (mirrors ClearForCreator which removes matching CreatorId + empty)
                    if (st && !st.AllowCutOthersWires && pwd.m_CreatorId != "" && pwd.m_CreatorId != cutPid)
                        continue;
                    LFPG_NetworkManager.Get().ReverseIdxRemove(pwd.m_TargetDeviceId, pwd.m_TargetPort, deviceId);
                    LFPG_NetworkManager.Get().PlayerWireCountAdd(pwd.m_CreatorId, -1);
                }
            }

            if (st && !st.AllowCutOthersWires)
            {
                changed = LFPG_DeviceAPI.ClearDeviceWiresForCreator(obj, sender.GetPlainId());
            }
            else
            {
                changed = LFPG_DeviceAPI.ClearDeviceWires(obj);
            }

            if (changed)
            {
                LFPG_Util.Info("Wires cleared LFPG " + deviceId);
                LFPG_NetworkManager.Get().BroadcastOwnerWires(obj);
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
                    string pid = sender.GetPlainId();
                    int vw = vWires.Count() - 1;
                    while (vw >= 0)
                    {
                        LFPG_WireData vwd = vWires[vw];
                        if (vwd)
                        {
                            if (vwd.m_CreatorId == "" || vwd.m_CreatorId == pid)
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

        // v0.7.23 (Bug 6): Also remove wires TARGETING this device's IN ports.
        // ClearDeviceWires only removes OWNED wires (output side).
        // Input wires are owned by the upstream device, so we must
        // call RemoveWiresTargeting for each IN port to cut them too.
        int inRemovedTotal = 0;
        int portCount = LFPG_DeviceAPI.GetPortCount(obj);
        int pi;
        for (pi = 0; pi < portCount; pi = pi + 1)
        {
            int portDir = LFPG_DeviceAPI.GetPortDir(obj, pi);
            if (portDir == LFPG_PortDir.IN)
            {
                string inPort = LFPG_DeviceAPI.GetPortName(obj, pi);
                int inRemoved = LFPG_NetworkManager.Get().RemoveWiresTargeting(deviceId, inPort);
                if (inRemoved > 0)
                {
                    changed = true;
                    inRemovedTotal = inRemovedTotal + inRemoved;
                    LFPG_Util.Info("CutWires: removed " + inRemoved.ToString() + " incoming wire(s) on " + deviceId + ":" + inPort);
                }
            }
        }

        // v0.7.25 (Bug 3): FALLBACK brute-force scan if reverse index missed wires.
        // The reverse index can become stale after migration, persistence load,
        // or incremental update edge cases. This direct scan catches any wires
        // targeting this device that RemoveWiresTargeting couldn't find via index.
        if (inRemovedTotal == 0 && portCount > 0)
        {
            array<EntityAI> allDevs = new array<EntityAI>;
            LFPG_DeviceRegistry.Get().GetAll(allDevs);
            int di;
            for (di = 0; di < allDevs.Count(); di = di + 1)
            {
                EntityAI srcDev = allDevs[di];
                if (!srcDev) continue;
                if (srcDev == obj) continue;
                if (!LFPG_DeviceAPI.HasWireStore(srcDev)) continue;

                string srcId = LFPG_DeviceAPI.GetDeviceId(srcDev);
                ref array<ref LFPG_WireData> srcWires = LFPG_DeviceAPI.GetDeviceWires(srcDev);
                if (!srcWires) continue;

                bool srcChanged = false;
                int sw = srcWires.Count() - 1;
                while (sw >= 0)
                {
                    LFPG_WireData swd = srcWires[sw];
                    if (swd && swd.m_TargetDeviceId == deviceId)
                    {
                        LFPG_Util.Warn("[CutWires-Fallback] Found stale wire: " + srcId + ":" + swd.m_SourcePort + " -> " + deviceId + ":" + swd.m_TargetPort);
                        LFPG_NetworkManager.Get().PlayerWireCountAdd(swd.m_CreatorId, -1);
                        srcWires.Remove(sw);
                        srcChanged = true;
                        changed = true;
                    }
                    sw = sw - 1;
                }

                if (srcChanged)
                {
                    srcDev.SetSynchDirty();
                    LFPG_NetworkManager.Get().BroadcastOwnerWires(srcDev);
                    LFPG_NetworkManager.Get().RequestPropagate(srcId);
                }
            }

            // Also scan vanilla wire store
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
                    if (vswd && vswd.m_TargetDeviceId == deviceId)
                    {
                        LFPG_Util.Warn("[CutWires-Fallback] Found stale vanilla wire: " + vOwnId + " -> " + deviceId + ":" + vswd.m_TargetPort);
                        LFPG_NetworkManager.Get().PlayerWireCountAdd(vswd.m_CreatorId, -1);
                        vwScan.Remove(vsw);
                        vSrcChanged = true;
                        changed = true;
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

            // If fallback found stale wires, rebuild reverse index to fix it
            if (changed && inRemovedTotal == 0)
            {
                LFPG_Util.Warn("[CutWires-Fallback] Reverse index was stale — rebuilding");
                LFPG_NetworkManager.Get().RebuildReverseIdx();
            }
        }

        if (changed)
        {
            // PostBulkRebuildAndPropagate: Rebuild → PopulateStates → MarkSourcesDirty
            LFPG_NetworkManager.Get().PostBulkRebuildAndPropagate();
            LFPG_SendClientMsg(this, "Wires cut.");
        }
        else
        {
            LFPG_SendClientMsg(this, "No wires to cut.");
        }
    }

    // =====================================
    // SERVER: Ciclar camara enlazada a un monitor
    // =====================================
    protected void HandleLFPG_CameraLink(PlayerIdentity sender, ParamsReadContext ctx)
    {
        // v0.9.1: DEPRECATED — camera linking is now physical (cables).
        // Read params to drain the stream (avoid corruption).
        int discardLow = 0;
        int discardHigh = 0;
        ctx.Read(discardLow);
        ctx.Read(discardHigh);
        LFPG_Util.Warn("[CameraLink] DEPRECATED RPC received — ignoring");
    }

    // =====================================
    // SERVER: Desvincular camara de un monitor
    // =====================================
    protected void HandleLFPG_CameraUnlink(PlayerIdentity sender, ParamsReadContext ctx)
    {
        // v0.9.1: DEPRECATED — camera unlinking is now physical (cut cable).
        // Read params to drain the stream (avoid corruption).
        int discardLow = 0;
        int discardHigh = 0;
        ctx.Read(discardLow);
        ctx.Read(discardHigh);
        LFPG_Util.Warn("[CameraUnlink] DEPRECATED RPC received — ignoring");
    }

    // =====================================
    // SERVER: Sprint B — Request camera list for a monitor
    // Resolves wired cameras, filters powered, sends list to client.
    // =====================================
    protected void HandleLFPG_RequestCameraList(PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender)
            return;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            LFPG_SendClientMsg(this, "Too fast! Wait a moment.");
            return;
        }

        int monNetLow = 0;
        int monNetHigh = 0;
        if (!ctx.Read(monNetLow))
            return;
        if (!ctx.Read(monNetHigh))
            return;

        // Resolve monitor entity by NetworkID
        EntityAI monEnt = EntityAI.Cast(GetGame().GetObjectByNetworkId(monNetLow, monNetHigh));
        if (!monEnt)
        {
            LFPG_Util.Warn("[RequestCameraList] monitor entity not found");
            LFPG_SendClientMsg(this, "Monitor not found.");
            return;
        }

        LF_Monitor monitor = LF_Monitor.Cast(monEnt);
        if (!monitor)
        {
            LFPG_Util.Warn("[RequestCameraList] entity is not LF_Monitor");
            return;
        }

        if (!monitor.LFPG_IsPowered())
        {
            LFPG_SendClientMsg(this, "El monitor no tiene alimentacion.");
            return;
        }

        // Collect cameras from monitor's wire store
        array<ref LFPG_WireData> wires = monitor.LFPG_GetWires();
        if (!wires || wires.Count() == 0)
        {
            string noWireLog = "[RequestCameraList] monitor " + monitor.LFPG_GetDeviceId();
            noWireLog = noWireLog + " has 0 wires";
            LFPG_Util.Info(noWireLog);
            LFPG_SendClientMsg(this, "No hay camaras conectadas.");
            return;
        }

        string wireCountLog = "[RequestCameraList] monitor " + monitor.LFPG_GetDeviceId();
        wireCountLog = wireCountLog + " wire count=" + wires.Count().ToString();
        LFPG_Util.Info(wireCountLog);

        // Build camera list — up to LFPG_MONITOR_MAX_CAMERAS entries
        // v1.3.1: Per-camera power check REMOVED. The monitor is PASSTHROUGH:
        // if the monitor itself is powered (checked above), cameras on its
        // outputs WILL receive power once graph propagation completes.
        // After server restart, propagation runs asynchronously — cameras
        // may still have m_PoweredNet=false (derived state, not persisted).
        // Requiring powered cameras caused "no cameras" on every restart.
        // Hoist all variables before loop (Enforce Script)
        int camCount = 0;
        int unresolvedCount = 0;
        ref array<vector> camPositions = new array<vector>;
        ref array<vector> camOrientations = new array<vector>;
        ref array<string> camLabels = new array<string>;

        EntityAI camEnt = null;
        LF_Camera cam = null;
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

            cam = LF_Camera.Cast(camEnt);
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
            // v1.0.1: Camera model lens points 90° right of entity forward.
            // Apply +90° yaw so the viewport aligns with the optic.
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
            LFPG_SendClientMsg(this, "No hay camaras detectables.");
            return;
        }

        // COT pattern: engine spectator system for camera lifecycle.
        // 1. Set skip flag to prevent vanilla OnSelectPlayer side effects
        // 2. SelectPlayer(sender, NULL) → desasociar player del identity
        // 3. SelectSpectator(sender, cls, pos) → engine crea+trackea cámara
        vector firstCamPos = camPositions[0];

        LFPG_SetSkipOnSelectPlayer(true);
        GetGame().SelectPlayer(sender, null);
        GetGame().SelectSpectator(sender, "staticcamera", firstCamPos);

        string specLog = "[RequestCameraList] SelectPlayer(null) + SelectSpectator at ";
        specLog = specLog + firstCamPos.ToString();
        LFPG_Util.Info(specLog);

        // Send CAMERA_LIST_RESPONSE to the requesting player only (sender).
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.CAMERA_LIST_RESPONSE);
        rpc.Write(camCount);

        int ci = 0;
        vector writePos = "0 0 0";
        vector writeOri = "0 0 0";
        float wf = 0.0;
        while (ci < camCount)
        {
            writePos = camPositions[ci];
            wf = writePos[0];
            rpc.Write(wf);
            wf = writePos[1];
            rpc.Write(wf);
            wf = writePos[2];
            rpc.Write(wf);
            writeOri = camOrientations[ci];
            wf = writeOri[0];
            rpc.Write(wf);
            wf = writeOri[1];
            rpc.Write(wf);
            wf = writeOri[2];
            rpc.Write(wf);
            rpc.Write(camLabels[ci]);
            ci = ci + 1;
        }

        rpc.Send(this, LFPG_RPC_CHANNEL, true, sender);

        string logMsg = "[RequestCameraList] Sent " + camCount.ToString() + " cameras to player";
        LFPG_Util.Info(logMsg);
    }

    // =====================================
    // SERVER: CCTV Exit Request — round-trip RPC.
    // COT pattern: server calls SelectPlayer FIRST, then sends
    // confirmation back to client. Client only cleans up camera
    // AFTER confirmation (guarantees engine pointer updated).
    // =====================================
    protected void HandleLFPG_CCTVExitRequest(PlayerIdentity sender)
    {
        if (!sender)
            return;

        PlayerBase player = PlayerBase.Cast(sender.GetPlayer());
        if (!player)
            return;

        // Restore player camera — engine updates internal pointer
        GetGame().SelectPlayer(sender, player);

        // Send confirmation back to client — NOW safe to cleanup camera
        ScriptRPC confirmRpc = new ScriptRPC();
        int confirmSubId = LFPG_RPC_SubId.CCTV_EXIT_CONFIRM;
        confirmRpc.Write(confirmSubId);
        confirmRpc.Send(player, LFPG_RPC_CHANNEL, true, sender);

        string logMsg = "[CCTV_EXIT] SelectPlayer + confirm sent for ";
        logMsg = logMsg + sender.GetName();
        LFPG_Util.Info(logMsg);
    }

    // =====================================
    // CLIENT: CCTV Exit Confirmed — server has called SelectPlayer.
    // NOW safe to cleanup the spectator camera on client side.
    // =====================================
    protected void HandleLFPG_CCTVExitConfirm()
    {
        LFPG_CameraViewport vp = LFPG_CameraViewport.Get();
        if (vp)
        {
            vp.DoExitCleanup();
        }
    }

    // =====================================
    // SERVER: Searchlight Enter — validate, mark operator, confirm
    // v1.5.0: No COT, no spectator. Player stays in normal mode.
    // =====================================
    protected void HandleLFPG_SearchlightEnter(PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender)
            return;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            LFPG_SendClientMsg(this, "Too fast! Wait a moment.");
            return;
        }

        int netLow = 0;
        int netHigh = 0;
        if (!ctx.Read(netLow))
            return;
        if (!ctx.Read(netHigh))
            return;

        // Resolve searchlight by NetworkID
        Object slObj = GetGame().GetObjectByNetworkId(netLow, netHigh);
        if (!slObj)
        {
            LFPG_Util.Warn("[Searchlight_Enter] Cannot resolve NetworkID");
            return;
        }

        LF_Searchlight sl = LF_Searchlight.Cast(slObj);
        if (!sl)
        {
            LFPG_Util.Warn("[Searchlight_Enter] Object is not LF_Searchlight");
            return;
        }

        if (!sl.LFPG_IsPowered())
        {
            LFPG_SendClientMsg(this, "Searchlight is not powered.");
            return;
        }

        // Block if already operated by someone else
        if (sl.LFPG_HasOperator())
        {
            LFPG_SendClientMsg(this, "Searchlight is already being operated.");
            return;
        }

        // Server-side distance validation (anti-exploit)
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

        // Mark operator on the searchlight (using PLAYER's NetworkID)
        int playerNetLow  = 0;
        int playerNetHigh = 0;
        playerCheck.GetNetworkID(playerNetLow, playerNetHigh);
        sl.LFPG_SetOperator(playerNetLow, playerNetHigh);

        // Send ENTER_CONFIRM with current yaw/pitch
        float curYaw = sl.LFPG_GetAimYaw();
        float curPitch = sl.LFPG_GetAimPitch();

        ScriptRPC rpc = new ScriptRPC();
        int confirmSubId = LFPG_RPC_SubId.SEARCHLIGHT_ENTER_CONFIRM;
        rpc.Write(confirmSubId);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Write(curYaw);
        rpc.Write(curPitch);
        rpc.Send(this, LFPG_RPC_CHANNEL, true, sender);

        string logMsg = "[Searchlight_Enter] Grab confirmed yaw=";
        logMsg = logMsg + curYaw.ToString();
        logMsg = logMsg + " pitch=" + curPitch.ToString();
        LFPG_Util.Info(logMsg);
    }

    // =====================================
    // SERVER: Searchlight Aim — receive yaw/pitch, update SyncVars, splash raycast
    // =====================================
    protected void HandleLFPG_SearchlightAim(PlayerIdentity sender, ParamsReadContext ctx)
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

        Object slObj = GetGame().GetObjectByNetworkId(netLow, netHigh);
        if (!slObj)
            return;

        LF_Searchlight sl = LF_Searchlight.Cast(slObj);
        if (!sl)
            return;

        // v1.5.0: Validate sender is the current operator (anti-cheat)
        PlayerBase aimPlayer = PlayerBase.Cast(sender.GetPlayer());
        if (!aimPlayer)
            return;
        int aimPlayerNetLow  = 0;
        int aimPlayerNetHigh = 0;
        aimPlayer.GetNetworkID(aimPlayerNetLow, aimPlayerNetHigh);
        if (!sl.LFPG_IsOperator(aimPlayerNetLow, aimPlayerNetHigh))
            return;

        // Server-side validation (anti-cheat)
        // Yaw: normalize to [-180, 180] (no clamp — full 360 rotation)
        while (aimYaw > 180.0)
        {
            aimYaw = aimYaw - 360.0;
        }
        while (aimYaw < -180.0)
        {
            aimYaw = aimYaw + 360.0;
        }
        // Pitch: clamp to allowed range
        if (aimPitch < LFPG_SEARCHLIGHT_PITCH_MIN)
            aimPitch = LFPG_SEARCHLIGHT_PITCH_MIN;
        if (aimPitch > LFPG_SEARCHLIGHT_PITCH_MAX)
            aimPitch = LFPG_SEARCHLIGHT_PITCH_MAX;

        // Write SyncVars
        sl.LFPG_SetAim(aimYaw, aimPitch);

        // Splash raycast — beam direction in WORLD space.
        // aimYaw is LOCAL to searchlight, must add BASE yaw for world direction.
        // NOT GetOrientation()[0] which changes with SetOrientation.
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

        // Single SetSynchDirty for aim + splash (batched)
        sl.LFPG_FlushSyncVars();
    }

    // =====================================
    // SERVER: Searchlight Exit — clear operator, send confirm
    // v1.5.0: No COT round-trip. Simple cleanup.
    // =====================================
    protected void HandleLFPG_SearchlightExit(PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender)
            return;

        int netLow = 0;
        int netHigh = 0;
        if (!ctx.Read(netLow))
            return;
        if (!ctx.Read(netHigh))
            return;

        Object slObj = GetGame().GetObjectByNetworkId(netLow, netHigh);
        if (slObj)
        {
            LF_Searchlight sl = LF_Searchlight.Cast(slObj);
            if (sl)
            {
                // Validate sender is the current operator (anti-cheat)
                PlayerBase exitPlayer = PlayerBase.Cast(sender.GetPlayer());
                if (exitPlayer)
                {
                    int exitNetLow  = 0;
                    int exitNetHigh = 0;
                    exitPlayer.GetNetworkID(exitNetLow, exitNetHigh);
                    if (sl.LFPG_IsOperator(exitNetLow, exitNetHigh))
                    {
                        sl.LFPG_ClearOperator();
                    }
                }
            }
        }

        // Send EXIT_CONFIRM to client (in case client-side cleanup hasn't happened)
        PlayerBase player = PlayerBase.Cast(sender.GetPlayer());
        if (player)
        {
            ScriptRPC confirmRpc = new ScriptRPC();
            int confirmSubId = LFPG_RPC_SubId.SEARCHLIGHT_EXIT_CONFIRM;
            confirmRpc.Write(confirmSubId);
            confirmRpc.Send(player, LFPG_RPC_CHANNEL, true, sender);
        }

        string logMsg = "[Searchlight_Exit] Operator released for ";
        logMsg = logMsg + sender.GetName();
        LFPG_Util.Info(logMsg);
    }

    // =====================================
    // CLIENT: Searchlight Enter Confirmed
    // =====================================
    protected void HandleLFPG_SearchlightEnterConfirm(ParamsReadContext ctx)
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

    // =====================================
    // CLIENT: Searchlight Exit Confirmed (server-initiated kick)
    // =====================================
    protected void HandleLFPG_SearchlightExitConfirm()
    {
        LFPG_SearchlightController ctrl = LFPG_SearchlightController.Get();
        if (ctrl)
        {
            ctrl.DoCleanup();
        }
    }

    // =====================================
    // CLIENT: Sprint B — Receive camera list from server
    // Passes data to CameraViewport for viewport activation.
    // =====================================
    protected void HandleLFPG_CameraListResponse(ParamsReadContext ctx)
    {
        int camCount = 0;
        if (!ctx.Read(camCount))
            return;

        if (camCount <= 0)
        {
            PlayerBase pLocal = PlayerBase.Cast(GetGame().GetPlayer());
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
        PlayerBase pGuard = PlayerBase.Cast(GetGame().GetPlayer());
        if (!pGuard || !pGuard.IsAlive() || pGuard.IsUnconscious())
        {
            LFPG_Util.Warn("[CameraListResponse] player dead/unconscious — ignoring");
            return;
        }

        vp.EnterFromList(entries);
    }

    // =====================================
    // SERVER: Cut single port wire
    // =====================================
    protected void HandleLFPG_CutPort(PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender) return;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            LFPG_SendClientMsg(this, "Too fast! Wait a moment.");
            return;
        }

        // v0.7.38 (RC-07): Reject during startup validation window.
        if (!LFPG_NetworkManager.Get().IsStartupValidationDone())
        {
            LFPG_SendClientMsg(this, "Server starting, please wait...");
            return;
        }

        if (!LFPG_WorldUtil.PlayerHasPliersInHands(this))
        {
            LFPG_SendClientMsg(this, "You need pliers in your hands.");
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

        EntityAI obj = EntityAI.Cast(GetGame().GetObjectByNetworkId(low, high));
        if (!obj) return;

        if (vector.Distance(this.GetPosition(), obj.GetPosition()) > 4.0)
        {
            LFPG_SendClientMsg(this, "Too far from device.");
            return;
        }

        string deviceId = LFPG_DeviceAPI.GetOrCreateDeviceId(obj);
        if (deviceId == "") return;

        bool changed = false;

        if (portDir == LFPG_PortDir.OUT)
        {
            // Remove wire(s) from this device's specific output port
            // Generic: works for Generator, Splitter, or any wire-owning device
            LFPG_ServerSettings st = LFPG_Settings.Get();
            string pid = sender.GetPlainId();

            if (LFPG_DeviceAPI.HasWireStore(obj))
            {
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
                            if (st && !st.AllowCutOthersWires && wd.m_CreatorId != "" && wd.m_CreatorId != pid)
                            {
                                LFPG_Util.Info("[CutPort] Skipped wire on " + portName + " (not creator)");
                            }
                            else
                            {
                                LFPG_Util.Info("[CutPort] Removed OUT wire " + deviceId + ":" + portName + " -> " + wd.m_TargetDeviceId);
                                // Incremental reverse index and player count update
                                LFPG_NetworkManager.Get().ReverseIdxRemove(wd.m_TargetDeviceId, wd.m_TargetPort, deviceId);
                                LFPG_NetworkManager.Get().PlayerWireCountAdd(wd.m_CreatorId, -1);
                                ownerWires.Remove(ow);
                                changed = true;
                            }
                        }
                        ow = ow - 1;
                    }
                }
                if (changed)
                {
                    obj.SetSynchDirty();
                    LFPG_NetworkManager.Get().BroadcastOwnerWires(obj);
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
                                if (st && !st.AllowCutOthersWires && vwd.m_CreatorId != "" && vwd.m_CreatorId != pid)
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
            // Remove all wires targeting this device+port from ANY source
            int removed = LFPG_NetworkManager.Get().RemoveWiresTargeting(deviceId, portName);
            if (removed > 0)
            {
                changed = true;
                LFPG_Util.Info("[CutPort] Removed " + removed.ToString() + " IN wire(s) on " + deviceId + ":" + portName);
            }
        }

        if (changed)
        {
            // PostBulkRebuildAndPropagate: Rebuild → PopulateStates → MarkSourcesDirty.
            // For IN port cuts, this also replaces RequestGlobalSelfHeal since it
            // achieves the same result (full rebuild + re-propagation from all sources).
            LFPG_NetworkManager.Get().PostBulkRebuildAndPropagate();

            LFPG_SendClientMsg(this, "Wire cut on " + portName + ".");
        }
        else
        {
            LFPG_SendClientMsg(this, "No wire on that port.");
        }
    }

    // =====================================
    // SERVER: Full sync request
    // =====================================
    protected void HandleLFPG_RequestFullSync(PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender) return;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
            return;

        LFPG_Util.Info("FullSync requested by pid=" + sender.GetPlainId());
        PlayerBase player = this;
        LFPG_NetworkManager.Get().SendFullSyncTo(player);
    }

    // =====================================
    // SERVER: Device-specific sync request (v0.7.35 D1)
    // Client sends deviceId when entering range of a device
    // whose wires are missing from the renderer.
    // =====================================
    protected void HandleLFPG_RequestDeviceSync(PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender) return;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
            return;

        // v0.7.45 (H7): Read NetworkID first, then clientDeviceId.
        // Same pattern as InspectDevice (v0.7.43 fix). This ensures
        // authoritative resolution even during SyncVar lag window.
        int netLow = 0;
        if (!ctx.Read(netLow))
        {
            LFPG_Util.Warn("[SERVER] RequestDeviceSync: read netLow FAIL pid=" + sender.GetPlainId());
            return;
        }
        int netHigh = 0;
        if (!ctx.Read(netHigh))
        {
            LFPG_Util.Warn("[SERVER] RequestDeviceSync: read netHigh FAIL pid=" + sender.GetPlainId());
            return;
        }
        string clientDeviceId = "";
        if (!ctx.Read(clientDeviceId))
        {
            LFPG_Util.Warn("[SERVER] RequestDeviceSync: read clientDeviceId FAIL pid=" + sender.GetPlainId());
            return;
        }

        if (clientDeviceId == "")
            return;

        // Resolve entity authoritatively via NetworkID (same as InspectDevice)
        // v0.7.45 review fix: use GetDeviceId (read-only), NOT GetOrCreateDeviceId.
        // If NetworkID resolves to a non-LFPG entity (edge case: ID reuse post-restart),
        // GetOrCreateDeviceId would generate a garbage "vp:Type:X:Y:Z" ID and
        // SendDeviceSyncTo would find 0 wires for that ID. GetDeviceId returns ""
        // which falls through to clientDeviceId fallback — correct behavior.
        string serverDeviceId = clientDeviceId;
        if (netLow != 0 || netHigh != 0)
        {
            EntityAI resolvedObj = EntityAI.Cast(GetGame().GetObjectByNetworkId(netLow, netHigh));
            if (resolvedObj)
            {
                string resolvedId = LFPG_DeviceAPI.GetDeviceId(resolvedObj);
                if (resolvedId != "")
                {
                    serverDeviceId = resolvedId;
                    // Re-register to heal stale DeviceRegistry refs.
                    // Only when we have the confirmed server-side ID.
                    LFPG_DeviceRegistry.Get().Register(resolvedObj, resolvedId);

                    // v1.1 (JIP Fix): Force SyncVar re-replication.
                    // When a JIP client enters a device's bubble, the engine
                    // may deliver default SyncVar values (m_SourceOn=false,
                    // m_PoweredNet=false). This causes: no CompEM effects
                    // (smoke/sound) on generators, IDLE cable colors on
                    // CableRenderer. SetSynchDirty forces the engine to
                    // re-queue ALL SyncVars on the next network tick.
                    // Each device in the bubble sends its own RequestDeviceSync,
                    // so this covers generators, consumers, and passthroughs.
                    resolvedObj.SetSynchDirty();
                }
            }
        }

        string rdsLog = "[SERVER] RequestDeviceSync serverDeviceId=" + serverDeviceId;
        rdsLog = rdsLog + " clientDeviceId=" + clientDeviceId;
        rdsLog = rdsLog + " net=" + netLow.ToString() + ":" + netHigh.ToString();
        rdsLog = rdsLog + " pid=" + sender.GetPlainId();
        LFPG_Util.Info(rdsLog);

        PlayerBase player = this;
        LFPG_NetworkManager.Get().SendDeviceSyncTo(player, serverDeviceId);
    }

    // =====================================
    // CLIENT: receive owner wires blob (for rendering)
    // =====================================
    protected void HandleLFPG_SyncOwnerWires(PlayerIdentity sender, ParamsReadContext ctx)
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

    // =====================================
    // SERVER: receive client diagnostic log
    // =====================================
    protected void HandleLFPG_DiagClientLog(PlayerIdentity sender, ParamsReadContext ctx)
    {
        string msg;
        if (!ctx.Read(msg))
            return;

        string pid = "unknown";
        if (sender)
            pid = sender.GetPlainId();

        LFPG_Util.Info("[CLI-ECHO:" + pid + "] " + msg);
    }

    // =====================================
    // SERVER: handle INSPECT_DEVICE request
    // v0.7.43 (Fix 2): Resolve device via NetworkID instead of trusting
    // the client's deviceId string. Client deviceId may not match server's
    // due to SyncVar race during kit placement. NetworkID is the engine's
    // authoritative identity and always matches. Same pattern as FinishWiring.
    // =====================================
    protected void HandleLFPG_InspectDevice(PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender)
            return;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
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

        // Resolve entity authoritatively via NetworkID
        string serverDeviceId = clientDeviceId;
        if (netLow != 0 || netHigh != 0)
        {
            EntityAI resolvedObj = EntityAI.Cast(GetGame().GetObjectByNetworkId(netLow, netHigh));
            if (resolvedObj)
            {
                string resolvedId = LFPG_DeviceAPI.GetDeviceId(resolvedObj);
                if (resolvedId != "")
                {
                    serverDeviceId = resolvedId;
                    // Re-register to heal stale DeviceRegistry refs.
                    // Only when we have the confirmed server-side ID.
                    LFPG_DeviceRegistry.Get().Register(resolvedObj, resolvedId);
                }
            }
        }

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
                    entry.m_RemoteDeviceId = oEdge.m_TargetNodeId;
                    entry.m_RemotePort = oEdge.m_TargetPort;
                    entry.m_RemoteTypeName = LFPG_ResolveTypeName(oEdge.m_TargetNodeId);

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
                    entry.m_RemoteDeviceId = iEdge.m_SourceNodeId;
                    entry.m_RemotePort = iEdge.m_SourcePort;
                    entry.m_RemoteTypeName = LFPG_ResolveTypeName(iEdge.m_SourceNodeId);

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
        rpc.Write(clientDeviceId);

        int wireCount = entries.Count();
        rpc.Write(wireCount);

        int wi;
        for (wi = 0; wi < wireCount; wi = wi + 1)
        {
            LFPG_InspectWireEntry we = entries[wi];
            rpc.Write(we.m_Direction);
            rpc.Write(we.m_LocalPort);
            rpc.Write(we.m_RemoteDeviceId);
            rpc.Write(we.m_RemotePort);
            rpc.Write(we.m_RemoteTypeName);
            rpc.Write(we.m_AllocatedPower);
            rpc.Write(we.m_EdgeState);
        }

        rpc.Send(this, LFPG_RPC_CHANNEL, true, null);

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

    protected static string LFPG_ResolveTypeName(string deviceId)
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

    // =====================================
    // CLIENT: handle INSPECT_RESPONSE
    // =====================================
    protected void HandleLFPG_InspectResponse(ParamsReadContext ctx)
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

    // =====================================
    // SERVER: Sorter CONFIG_REQUEST (SubId 19)
    // Client requests Sorter config for UI panel.
    // Resolves filterJSON, container name, and 6 dest names.
    // =====================================
    protected void HandleLFPG_SorterConfigRequest(PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender)
            return;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            LFPG_SendClientMsg(this, "Too fast! Wait a moment.");
            return;
        }

        int netLow = 0;
        int netHigh = 0;
        if (!ctx.Read(netLow))
            return;
        if (!ctx.Read(netHigh))
            return;

        // Resolve sorter by NetworkID
        EntityAI devEnt = EntityAI.Cast(GetGame().GetObjectByNetworkId(netLow, netHigh));
        if (!devEnt)
        {
            LFPG_Util.Warn("[SorterConfigRequest] entity not found");
            return;
        }

        LF_Sorter sorter = LF_Sorter.Cast(devEnt);
        if (!sorter)
        {
            LFPG_Util.Warn("[SorterConfigRequest] entity is not LF_Sorter");
            return;
        }

        // Proximity check (match ActionCondition distance)
        float dist = vector.Distance(this.GetPosition(), devEnt.GetPosition());
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
            LFPG_SendClientMsg(this, "Sorter has no power.");
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
            LF_Sorter targetSorter = null;
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

                    // Found wire for this output port — resolve target
                    targetEnt = LFPG_DeviceAPI.ResolveByNetworkId(wd.m_TargetNetLow, wd.m_TargetNetHigh);
                    if (!targetEnt)
                    {
                        // Fallback: try DeviceRegistry by ID
                        targetEnt = LFPG_DeviceRegistry.Get().FindById(wd.m_TargetDeviceId);
                    }
                    if (!targetEnt)
                        break;

                    targetSorter = LF_Sorter.Cast(targetEnt);
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
        rpc.Write((int)LFPG_RPC_SubId.SORTER_CONFIG_RESPONSE);
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
        rpc.Send(this, LFPG_RPC_CHANNEL, true, sender);

        string logMsg = "[SorterConfigRequest] Sent config for ";
        logMsg = logMsg + sorter.LFPG_GetDeviceId();
        logMsg = logMsg + " container=" + containerName;
        LFPG_Util.Info(logMsg);
    }

    // =====================================
    // SERVER: Sorter CONFIG_SAVE (SubId 21)
    // Client saves updated filterJSON.
    // =====================================
    protected void HandleLFPG_SorterConfigSave(PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender)
            return;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            LFPG_SendClientMsg(this, "Too fast! Wait a moment.");
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
        EntityAI devEnt = EntityAI.Cast(GetGame().GetObjectByNetworkId(netLow, netHigh));
        if (!devEnt)
        {
            LFPG_Util.Warn("[SorterConfigSave] entity not found");
            return;
        }

        LF_Sorter sorter = LF_Sorter.Cast(devEnt);
        if (!sorter)
        {
            LFPG_Util.Warn("[SorterConfigSave] entity is not LF_Sorter");
            return;
        }

        // Proximity check (match ActionCondition distance)
        float dist = vector.Distance(this.GetPosition(), devEnt.GetPosition());
        if (dist > LFPG_INTERACT_DIST_M)
        {
            LFPG_Util.Warn("[SorterConfigSave] player too far");
            return;
        }

        // Powered check — don't allow config changes on unpowered device
        if (!sorter.LFPG_IsPowered())
        {
            LFPG_Util.Warn("[SorterConfigSave] sorter not powered");
            return;
        }

        // Store config — returns false if JSON is malformed (M3 validation)
        bool saveOk = sorter.LFPG_SetFilterJSON(filterJSON);

        // H4: Send ACK back to client
        ScriptRPC ackRpc = new ScriptRPC();
        int ackSubId = LFPG_RPC_SubId.SORTER_SAVE_ACK;
        ackRpc.Write(ackSubId);
        ackRpc.Write(saveOk);
        ackRpc.Send(this, LFPG_RPC_CHANNEL, true, sender);

        if (!saveOk)
        {
            LFPG_Util.Warn("[SorterConfigSave] rejected malformed JSON from client");
            return;
        }

        string logMsg = "[SorterConfigSave] Updated config for ";
        logMsg = logMsg + sorter.LFPG_GetDeviceId();
        LFPG_Util.Info(logMsg);
    }

    // =====================================
    // SERVER: Sorter REQUEST_SORT (SubId 22)
    // Client requests manual sort (BinPack).
    // Resolves Sorter, validates, delegates to NetworkManager.
    // =====================================
    protected void HandleLFPG_SorterRequestSort(PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender)
            return;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            LFPG_SendClientMsg(this, "Too fast! Wait a moment.");
            return;
        }

        int netLow = 0;
        int netHigh = 0;
        if (!ctx.Read(netLow))
            return;
        if (!ctx.Read(netHigh))
            return;

        // Resolve sorter
        EntityAI devEnt = EntityAI.Cast(GetGame().GetObjectByNetworkId(netLow, netHigh));
        if (!devEnt)
        {
            LFPG_Util.Warn("[SorterRequestSort] entity not found");
            return;
        }

        LF_Sorter sorter = LF_Sorter.Cast(devEnt);
        if (!sorter)
        {
            LFPG_Util.Warn("[SorterRequestSort] entity is not LF_Sorter");
            return;
        }

        // Proximity check
        float dist = vector.Distance(this.GetPosition(), devEnt.GetPosition());
        if (dist > LFPG_INTERACT_DIST_M)
        {
            LFPG_Util.Warn("[SorterRequestSort] player too far");
            return;
        }

        // Delegate to NetworkManager (handles powered + container checks)
        int sortMoved = LFPG_NetworkManager.Get().HandleSorterRequestSort(sorter);
        bool sortOk = (sortMoved >= 0);

        // Send ACK only to requesting player (not broadcast)
        ScriptRPC sortAckRpc = new ScriptRPC();
        int sortAckSubId = LFPG_RPC_SubId.SORTER_SORT_ACK;
        sortAckRpc.Write(sortAckSubId);
        sortAckRpc.Write(sortOk);
        sortAckRpc.Write(sortMoved);
        sortAckRpc.Send(this, LFPG_RPC_CHANNEL, true, sender);
    }

    // =====================================
    // CLIENT: Sorter CONFIG_RESPONSE (SubId 20)
    // Server sends config payload → open SorterUI.
    // =====================================
    protected void HandleLFPG_SorterConfigResponse(ParamsReadContext ctx)
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

    // =====================================
    // CLIENT: Sorter SAVE_ACK (SubId 23)
    // Server confirms config save result.
    // =====================================
    protected void HandleLFPG_SorterSaveAck(ParamsReadContext ctx)
    {
        bool success = false;
        if (!ctx.Read(success))
            return;

        LFPG_SorterView.OnSaveAck(success);
    }

    // =====================================
    // CLIENT: Sorter SORT_ACK (SubId 33)
    // Server confirms sort result + moved count.
    // =====================================
    protected void HandleLFPG_SorterSortAck(ParamsReadContext ctx)
    {
        bool success = false;
        int movedCount = 0;
        if (!ctx.Read(success))
            return;
        if (!ctx.Read(movedCount))
            return;

        LFPG_SorterView.OnSortAck(success, movedCount);
    }

    // =====================================
    // SERVER: Sorter RESYNC (SubId 29)
    // Client requests re-link nearest container.
    // v2.4 Bug B
    // =====================================
    protected void HandleLFPG_SorterResync(PlayerIdentity sender, ParamsReadContext ctx)
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

        EntityAI devEnt = EntityAI.Cast(GetGame().GetObjectByNetworkId(netLow, netHigh));
        if (!devEnt)
        {
            string warnNotFound = "[SorterResync] entity not found";
            LFPG_Util.Warn(warnNotFound);
            return;
        }

        LF_Sorter sorter = LF_Sorter.Cast(devEnt);
        if (!sorter)
        {
            string warnNotSorter = "[SorterResync] entity is not LF_Sorter";
            LFPG_Util.Warn(warnNotSorter);
            return;
        }

        // Proximity check
        float dist = vector.Distance(this.GetPosition(), devEnt.GetPosition());
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

        // Unlink old container (if any)
        sorter.LFPG_UnlinkContainer();

        // Re-scan for nearest container
        sorter.LFPG_LinkNearestContainer(sorter.GetPosition());

        // Resolve new linked container name
        string containerName = "";
        EntityAI linkedCont = sorter.LFPG_GetLinkedContainer();
        if (linkedCont)
        {
            containerName = linkedCont.GetDisplayName();
        }

        // Send ACK to client
        ScriptRPC ackRpc = new ScriptRPC();
        int ackSubId = LFPG_RPC_SubId.SORTER_RESYNC_ACK;
        ackRpc.Write(ackSubId);
        ackRpc.Write(containerName);
        ackRpc.Send(this, LFPG_RPC_CHANNEL, true, sender);

        string logMsg = "[SorterResync] result=";
        logMsg = logMsg + containerName;
        LFPG_Util.Info(logMsg);
    }

    // =====================================
    // CLIENT: Sorter RESYNC_ACK (SubId 30)
    // Server confirms re-link result.
    // v2.4 Bug B
    // =====================================
    protected void HandleLFPG_SorterResyncAck(ParamsReadContext ctx)
    {
        string containerName = "";
        if (!ctx.Read(containerName))
            return;

        if (!GetGame())
            return;

        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
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

    // =====================================
    // SERVER: Sorter PREVIEW_REQUEST (SubId 31)
    // Client requests matching items for preview panel.
    // v2.6
    // =====================================
    protected void HandleLFPG_SorterPreviewRequest(PlayerIdentity sender, ParamsReadContext ctx)
    {
        if (!sender)
            return;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
            return;

        int netLow = 0;
        int netHigh = 0;
        int selectedOutput = 0;
        if (!ctx.Read(netLow))
            return;
        if (!ctx.Read(netHigh))
            return;
        if (!ctx.Read(selectedOutput))
            return;

        // Validate output index
        if (selectedOutput < 0 || selectedOutput >= 6)
            return;

        // From this point, always send a response (even if empty).
        // Silent return would leave the client waiting with stale data.
        int totalMatched = 0;
        int sentCount = 0;
        array<string> matchNames = new array<string>;
        array<string> matchCats = new array<string>;
        array<int> matchSlots = new array<int>;

        // Resolve sorter
        EntityAI devEnt = EntityAI.Cast(GetGame().GetObjectByNetworkId(netLow, netHigh));
        LF_Sorter sorter = null;
        if (devEnt)
        {
            sorter = LF_Sorter.Cast(devEnt);
        }

        bool canProceed = false;
        if (sorter)
        {
            float dist = vector.Distance(this.GetPosition(), devEnt.GetPosition());
            if (dist <= LFPG_INTERACT_DIST_M)
            {
                canProceed = true;
            }
        }

        if (canProceed)
        {
            // Parse filter config from sorter
            string filterJSON = sorter.LFPG_GetFilterJSON();
            LFPG_SortConfig config = new LFPG_SortConfig();
            config.FromJSON(filterJSON);
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
                        int slotSize = 0;

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
                                slotSize = LFPG_SorterLogic.GetItemSlotSize(cItem);
                                matchNames.Insert(typeName);
                                matchCats.Insert(cat);
                                matchSlots.Insert(slotSize);
                            }
                        }
                    }
                }
            }
        }

        // Always send response (empty if guards failed)
        sentCount = matchNames.Count();
        ScriptRPC rpc = new ScriptRPC();
        int respSubId = LFPG_RPC_SubId.SORTER_PREVIEW_RESPONSE;
        rpc.Write(respSubId);
        rpc.Write(selectedOutput);
        rpc.Write(totalMatched);
        rpc.Write(sentCount);

        int si = 0;
        for (si = 0; si < sentCount; si = si + 1)
        {
            rpc.Write(matchNames[si]);
            rpc.Write(matchCats[si]);
            rpc.Write(matchSlots[si]);
        }

        bool bRpcGuaranteed = true;
        rpc.Send(this, LFPG_RPC_CHANNEL, bRpcGuaranteed, sender);

        string logMsg = "[SorterPreviewRequest] output=";
        logMsg = logMsg + selectedOutput.ToString();
        logMsg = logMsg + " matched=";
        logMsg = logMsg + totalMatched.ToString();
        logMsg = logMsg + " sent=";
        logMsg = logMsg + sentCount.ToString();
        LFPG_Util.Info(logMsg);
    }

    // =====================================
    // CLIENT: Sorter PREVIEW_RESPONSE (SubId 32)
    // Server sends matching items for preview panel.
    // v2.6
    // =====================================
    protected void HandleLFPG_SorterPreviewResponse(ParamsReadContext ctx)
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
        array<int> slots = new array<int>;

        int si = 0;
        string itemName = "";
        string itemCat = "";
        int itemSlot = 0;
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
            if (!ctx.Read(itemSlot))
            {
                readOk = false;
                break;
            }
            names.Insert(itemName);
            cats.Insert(itemCat);
            slots.Insert(itemSlot);
        }

        if (!readOk)
        {
            string errRead = "[SorterPreviewResponse] item read FAIL at index ";
            errRead = errRead + si.ToString();
            LFPG_Util.Warn(errRead);
            return;
        }

        LFPG_SorterView.OnPreviewData(outputIdx, totalMatched, names, cats, slots);

        string logMsg = "[SorterPreviewResponse] output=";
        logMsg = logMsg + outputIdx.ToString();
        logMsg = logMsg + " total=";
        logMsg = logMsg + totalMatched.ToString();
        logMsg = logMsg + " received=";
        logMsg = logMsg + sentCount.ToString();
        LFPG_Util.Info(logMsg);
    }
};