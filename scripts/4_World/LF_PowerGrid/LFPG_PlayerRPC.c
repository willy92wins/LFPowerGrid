// =========================================================
// LF_PowerGrid - PlayerBase RPC routing (server authoritative)
// v0.7.20: Sprint 4.2 S2 — H2 fix: correct bulk mutation order in CutWires/CutPort
// v0.7.34: Bloque E — Atomic graph mutations in FinishWiring replace phase.
//   Graph notified on wire removal (was missing → stale edges).
//   Begin/EndGraphMutation wraps remove+add for same-target safety.
// =========================================================

modded class PlayerBase
{
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
        #else
        if (subId == LFPG_RPC_SubId.SYNC_OWNER_WIRES)
        {
            HandleLFPG_SyncOwnerWires(sender, ctx);
        }
        else if (subId == LFPG_RPC_SubId.CLIENT_MSG)
        {
            HandleLFPG_ClientMsg(ctx);
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

            // v0.7.33 (Fix #18b): If vanilla wires were removed during replacement phase
            // but the new wire failed to store, we must still persist the removal.
            // Without this, server restart would resurrect the removed wire.
            if (anyRemoved && !isLfpgOwner)
            {
                LFPG_NetworkManager.Get().MarkVanillaDirty();
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
};
