// =========================================================
// LF_PowerGrid - NetworkManager (v0.7.30, Sprint 4.3+audit closure)
//
// Server singleton: validation, wire storage, propagation.
//
// Wire storage:
//   - LFPG sources (LF_TestGenerator): wires stored ON the object
//   - Vanilla sources (PowerGenerator etc): wires stored centrally
//     in m_VanillaWires keyed by position-based device ID ("vp:TYPE:QX:QY:QZ")
//
// Reverse index: m_ReverseIdx maps "targetId|port" -> wire count
//   Updated incrementally on add/remove. Full rebuild on self-heal.
//   Graph m_Incoming used for propagation (Sprint 4.2).
//   Still active for RemoveWiresTargeting and CountWiresTargeting.
//
// Player quota: m_WiresByPlayer[pid] -> wire count (incremental O(1))
//
// Propagation (Sprint 4.2): event-driven via ElecGraph dirty queue.
//   RequestPropagate() marks graph nodes dirty.
//   TickPropagation() processes dirty queue with budget per tick.
//
// Sprint 4.3: Load telemetry accumulators in TickPropagation.
//   Periodic dump of propagation latency + load metrics.
//
// v0.7.30 (Audit 1+2): Centralized position polling with round-robin
//   batching. Replaces per-device timers + full-scan approach.
//   m_TrackedDeviceIds auto-registers on wire add, unregisters on cut.
//   CheckDeviceMovement processes BATCH_SIZE devices per 500ms tick.
//
// Vanilla wire persistence: saved/loaded via profile JSON.
//   Position-based IDs survive server restarts.
// =========================================================

class LFPG_NetworkManager
{
    protected static ref LFPG_NetworkManager s_Instance;

    // Per-player anti-spam
    protected ref map<string, ref LFPG_RateLimiter> m_RateByPlayer;

    // Central wire storage for vanilla sources (keyed by position-based device ID)
    protected ref map<string, ref array<ref LFPG_WireData>> m_VanillaWires;

    // Reverse index: "targetDeviceId|targetPort" -> number of wires targeting it
    // Updated incrementally via ReverseIdxAdd/Remove. Full rebuild on self-heal.
    protected ref map<string, int> m_ReverseIdx;

    // Reverse owner map: "targetDeviceId|targetPort" -> array of ownerDeviceIds
    // Enables directed removal without full device scan.
    protected ref map<string, ref array<string>> m_ReverseOwners;

    // Per-player wire count (incremental O(1) quota check)
    // Updated on add/remove/cut. Full recount on self-heal.
    protected ref map<string, int> m_WiresByPlayer;

    // Coalesced self-heal scheduling
    protected bool m_SelfHealQueued = false;

    // v0.7.4: deferred vanilla wire persistence.
    // MarkVanillaDirty() sets flag; FlushVanillaIfDirty() writes to disk.
    // Periodic timer (LFPG_VANILLA_FLUSH_S) flushes automatically.
    // Eliminates synchronous I/O on every wire mutation.
    protected bool m_VanillaDirty = false;

    // v0.7.16 H6: Version guard — track loaded schema version.
    // If file was saved by a newer mod version, block saves to prevent data loss.
    protected int m_VanillaLoadedVer = 0;
    protected bool m_VanillaReadOnly = false;

    // Cached valid device IDs for PruneMissingTargets (built once per self-heal cycle)
    protected ref map<string, bool> m_CachedValidIds;

    // Vanilla wire persistence path
    protected static const string VANILLA_WIRES_DIR  = "$profile:LF_PowerGrid";
    protected static const string VANILLA_WIRES_FILE = "$profile:LF_PowerGrid\\vanilla_wires.json";

    // Rate limiter stale threshold: entries idle for > 10 minutes are purged
    protected static const float RATE_LIMITER_STALE_SEC = 600.0;

    protected ref map<string, EntityAI> m_PendingBroadcastLFPG;
    protected ref map<string, EntityAI> m_PendingBroadcastVanilla;

    // Sprint 4.1: Electrical graph (server-only).
    // Mirrors the wire topology for cycle detection and future propagation.
    protected ref LFPG_ElecGraph m_Graph;

    // Sprint 4.2 S2b (H3): Warmup mode flag.
    // Set true after PostBulkRebuild / ValidateAllWires; cleared when queue drains.
    // While active, TickPropagation uses WARMUP_BUDGET instead of NODE_BUDGET.
    protected bool m_WarmupActive;

    // Sprint 4.3: Propagation telemetry accumulators (server-side).
    // Accumulated between dumps; reset every LFPG_TELEM_INTERVAL_MS.
    protected int m_TelemTickCount;
    protected int m_TelemTotalProcessMs;
    protected int m_TelemPeakProcessMs;
    protected int m_TelemTotalNodesProcessed;
    protected int m_TelemTotalEdgesVisited;
    protected float m_TelemLastDumpMs;

    // v0.7.30 (Audit 1+2): Centralized position polling with round-robin batching.
    // Replaces N per-device timers (v0.7.29) with a single global timer.
    // m_TrackedDeviceIds: only devices with active wires (auto-register/unregister).
    // m_TrackedDeviceIndex: ID → array index for O(1) swap-and-pop removal.
    // m_TrackCursor: round-robin cursor for batched processing.
    // m_LastKnownPos: reused from v0.7.23, position snapshot per tracked device.
    protected ref map<string, vector> m_LastKnownPos;
    protected ref array<string>       m_TrackedDeviceIds;
    protected ref map<string, int>    m_TrackedDeviceIndex;
    protected int                     m_TrackCursor;

    void LFPG_NetworkManager()
    {
        m_RateByPlayer = new map<string, ref LFPG_RateLimiter>;
        m_VanillaWires = new map<string, ref array<ref LFPG_WireData>>;
        m_ReverseIdx = new map<string, int>;
        m_ReverseOwners = new map<string, ref array<string>>;
        m_WiresByPlayer = new map<string, int>;
        m_PendingBroadcastLFPG = new map<string, EntityAI>;
        m_PendingBroadcastVanilla = new map<string, EntityAI>;
        m_LastKnownPos = new map<string, vector>;

        #ifdef SERVER
        // v0.7.30: Tracked device set for centralized polling.
        // Always allocated when compiled as server (dedicated + SP host).
        // Methods have runtime IsServer() guards for extra safety.
        m_TrackedDeviceIds = new array<string>;
        m_TrackedDeviceIndex = new map<string, int>;
        m_TrackCursor = 0;

        m_Graph = new LFPG_ElecGraph();
        m_WarmupActive = false;
        m_TelemTickCount = 0;
        m_TelemTotalProcessMs = 0;
        m_TelemPeakProcessMs = 0;
        m_TelemTotalNodesProcessed = 0;
        m_TelemTotalEdgesVisited = 0;
        m_TelemLastDumpMs = -99999.0;
        LFPG_Util.Info("NetworkManager init (server).");
        LoadVanillaWires();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ValidateAllWiresAndPropagate, 5000, false);
        // Periodic rate limiter cleanup (every 5 minutes)
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PurgeStaleRateLimiters, 300000, true);
        // v0.7.4: periodic vanilla wire flush (deferred persistence)
        int flushMs = (int)(LFPG_VANILLA_FLUSH_S * 1000.0);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(FlushVanillaIfDirty, flushMs, true);
        // Sprint 4.2: periodic propagation tick (event-driven via graph dirty queue)
        int propTickMs = (int)LFPG_PROPAGATE_TICK_MS;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(TickPropagation, propTickMs, true);
        // v0.7.30 (Audit 1+2): Centralized position polling with round-robin batching.
        // Replaces per-device timers. Processes LFPG_MOVE_DETECT_BATCH_SIZE devices per tick.
        // Runtime guard: prevents timer registration in SP/local-host hybrid contexts
        // where #ifdef SERVER is active but the instance isn't a true dedicated server.
        if (GetGame().IsServer())
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CheckDeviceMovement, LFPG_MOVE_DETECT_TICK_MS, true);
        }
        #endif
    }

    static LFPG_NetworkManager Get()
    {
        if (!s_Instance)
            s_Instance = new LFPG_NetworkManager();
        return s_Instance;
    }

    // ===========================
    // Rate limit
    // ===========================
    bool AllowPlayerAction(PlayerIdentity ident)
    {
        if (!ident) return false;

        string pid = ident.GetPlainId();
        LFPG_ServerSettings st = LFPG_Settings.Get();
        float now = GetGame().GetTime() * 0.001;

        ref LFPG_RateLimiter rl;
        if (!m_RateByPlayer.Find(pid, rl) || !rl)
        {
            rl = new LFPG_RateLimiter();
            m_RateByPlayer[pid] = rl;
        }

        return rl.Allow(now, st.RpcCooldownSeconds);
    }

    // Periodic cleanup: remove rate limiters for disconnected/idle players.
    // Runs every 5 minutes via CallLater. Prevents unbounded map growth.
    protected void PurgeStaleRateLimiters()
    {
        #ifdef SERVER
        float now = GetGame().GetTime() * 0.001;
        ref array<string> staleKeys = new array<string>;

        int i;
        for (i = 0; i < m_RateByPlayer.Count(); i = i + 1)
        {
            LFPG_RateLimiter rl = m_RateByPlayer.GetElement(i);
            if (!rl) continue;

            // If NextAllowed is far in the past, player is idle/disconnected
            float idleSec = now - rl.GetNextAllowed();
            if (idleSec > RATE_LIMITER_STALE_SEC)
            {
                staleKeys.Insert(m_RateByPlayer.GetKey(i));
            }
        }

        int removed = staleKeys.Count();
        int k;
        for (k = 0; k < removed; k = k + 1)
        {
            m_RateByPlayer.Remove(staleKeys[k]);
        }

        if (removed > 0)
        {
            LFPG_Util.Info("[RateLimiter] Purged " + removed.ToString() + " stale entries");
        }
        #endif
    }

    // ===========================
    // Vanilla wire storage
    // ===========================
    bool AddVanillaWire(string ownerDeviceId, LFPG_WireData wd)
    {
        if (ownerDeviceId == "" || !wd)
            return false;

        if (wd.m_SourcePort == "")
            wd.m_SourcePort = "output_1";

        ref array<ref LFPG_WireData> wires;
        if (!m_VanillaWires.Find(ownerDeviceId, wires) || !wires)
        {
            wires = new array<ref LFPG_WireData>;
            m_VanillaWires[ownerDeviceId] = wires;
        }

        LFPG_ServerSettings st = LFPG_Settings.Get();
        int maxWires = LFPG_MAX_WIRES_PER_DEVICE;
        if (st && st.MaxWiresPerDevice > 0)
        {
            maxWires = st.MaxWiresPerDevice;
        }

        if (wires.Count() >= maxWires)
            return false;

        // Deduplicate
        int i;
        for (i = 0; i < wires.Count(); i = i + 1)
        {
            LFPG_WireData e = wires[i];
            if (!e) continue;
            if (e.m_TargetDeviceId == wd.m_TargetDeviceId && e.m_TargetPort == wd.m_TargetPort && e.m_SourcePort == wd.m_SourcePort)
                return false;
        }

        wires.Insert(wd);

        // Incremental updates
        ReverseIdxAdd(wd.m_TargetDeviceId, wd.m_TargetPort, ownerDeviceId);
        PlayerWireCountAdd(wd.m_CreatorId, 1);
        MarkVanillaDirty();

        return true;
    }

    array<ref LFPG_WireData> GetVanillaWires(string ownerDeviceId)
    {
        ref array<ref LFPG_WireData> wires;
        if (m_VanillaWires.Find(ownerDeviceId, wires))
            return wires;
        return null;
    }

    // Get wires for ANY source device (LFPG or vanilla)
    array<ref LFPG_WireData> GetWiresForDevice(string deviceId)
    {
        // Try LFPG device first (generic: works for Generator, Splitter, etc.)
        EntityAI obj = LFPG_DeviceRegistry.Get().FindById(deviceId);
        if (obj)
        {
            if (LFPG_DeviceAPI.HasWireStore(obj))
            {
                return LFPG_DeviceAPI.GetDeviceWires(obj);
            }
        }

        // Try vanilla store
        return GetVanillaWires(deviceId);
    }

    // ===========================
    // Vanilla wire map accessors (Sprint 4.1)
    // Used by LFPG_ElecGraph.RebuildFromWires to iterate all vanilla wire owners.
    // ===========================

    int GetVanillaWireOwnerCount()
    {
        return m_VanillaWires.Count();
    }

    string GetVanillaWireOwnerKey(int idx)
    {
        if (idx < 0 || idx >= m_VanillaWires.Count())
            return "";
        return m_VanillaWires.GetKey(idx);
    }

    // ===========================
    // Electrical graph API (Sprint 4.1)
    // ===========================

    // Check if a proposed wire would create a directed cycle.
    // Returns true if cycle detected (wire should be rejected).
    // Must be called BEFORE the wire is stored.
    bool CheckCycleBeforeWire(string sourceId, string targetId)
    {
        #ifdef SERVER
        if (!m_Graph)
            return false;
        return m_Graph.DetectCycleIfAdded(sourceId, targetId);
        #else
        return false;
        #endif
    }

    // Notify the graph that a wire was successfully added.
    // Called AFTER the wire is stored in the device or vanilla store.
    // Sprint 4.2 S2 (H1): returns false if edge was not actually inserted.
    bool NotifyGraphWireAdded(string sourceId, string targetId, string sourcePort, string targetPort, LFPG_WireData wireRef)
    {
        #ifdef SERVER
        if (!m_Graph)
            return false;
        bool inserted = m_Graph.OnWireAdded(sourceId, targetId, sourcePort, targetPort, wireRef);
        if (inserted)
        {
            // v0.7.30: Auto-track both endpoints for centralized position polling
            TrackDeviceForPolling(sourceId);
            TrackDeviceForPolling(targetId);
        }
        return inserted;
        #else
        return false;
        #endif
    }

    // Sprint 4.2 S2 (H2): Correct bulk mutation sequence.
    // After CutWires/CutPort, the graph must be rebuilt BEFORE
    // marking nodes dirty. This method guarantees the correct order:
    //   1. Rebuild graph from wire data (clears dirty queue)
    //   2. Populate electrical states from entities
    //   3. Mark all sources dirty (re-populates dirty queue)
    // Sprint 4.2 S2b (H3): Activates warmup budget mode for faster drain.
    void PostBulkRebuildAndPropagate()
    {
        #ifdef SERVER
        if (!m_Graph)
            return;
        m_Graph.PostBulkRebuild(this);
        m_WarmupActive = true;
        #endif
    }

    // Notify the graph that a device has been removed.
    // Called from device EEDelete handlers.
    void NotifyGraphDeviceRemoved(string deviceId)
    {
        #ifdef SERVER
        if (!m_Graph)
            return;
        m_Graph.OnDeviceRemoved(deviceId);
        // v0.7.26 (Audit 4): Clean up position tracking for removed device
        m_LastKnownPos.Remove(deviceId);
        // v0.7.30: Untrack from centralized polling (EEDelete path)
        UntrackDeviceFromPolling(deviceId);
        #endif
    }

    // Get the graph reference for telemetry / debug.
    LFPG_ElecGraph GetGraph()
    {
        return m_Graph;
    }

    // ===========================
    // Reverse index: "targetId|port" -> wire count + owner list
    // O(1) lookup instead of full scan.
    // ===========================
    void RebuildReverseIdx()
    {
        #ifdef SERVER
        m_ReverseIdx.Clear();
        m_ReverseOwners.Clear();

        // Scan LFPG wire-owning devices (Generator, Splitter, etc.)
        array<EntityAI> all = new array<EntityAI>;
        LFPG_DeviceRegistry.Get().GetAll(all);

        int i;
        for (i = 0; i < all.Count(); i = i + 1)
        {
            if (!LFPG_DeviceAPI.HasWireStore(all[i])) continue;

            string ownerId = LFPG_DeviceAPI.GetDeviceId(all[i]);
            ref array<ref LFPG_WireData> gWires = LFPG_DeviceAPI.GetDeviceWires(all[i]);
            if (!gWires) continue;

            int gw;
            for (gw = 0; gw < gWires.Count(); gw = gw + 1)
            {
                LFPG_WireData wd = gWires[gw];
                if (!wd) continue;

                string tPort = wd.m_TargetPort;
                if (tPort == "")
                {
                    tPort = "input_main";
                }

                string rKey = wd.m_TargetDeviceId + "|" + tPort;
                int prev = 0;
                m_ReverseIdx.Find(rKey, prev);
                m_ReverseIdx[rKey] = prev + 1;

                // Track owner reference
                ReverseOwnersInsert(rKey, ownerId);
            }
        }

        // Scan vanilla stores
        int vk;
        for (vk = 0; vk < m_VanillaWires.Count(); vk = vk + 1)
        {
            string vOwnerId = m_VanillaWires.GetKey(vk);
            ref array<ref LFPG_WireData> vWires = m_VanillaWires.GetElement(vk);
            if (!vWires) continue;

            int vw;
            for (vw = 0; vw < vWires.Count(); vw = vw + 1)
            {
                LFPG_WireData vwd = vWires[vw];
                if (!vwd) continue;

                string vtPort = vwd.m_TargetPort;
                if (vtPort == "")
                {
                    vtPort = "input_main";
                }

                string vrKey = vwd.m_TargetDeviceId + "|" + vtPort;
                int vprev = 0;
                m_ReverseIdx.Find(vrKey, vprev);
                m_ReverseIdx[vrKey] = vprev + 1;

                // Track owner reference
                ReverseOwnersInsert(vrKey, vOwnerId);
            }
        }
        #endif
    }

    // Helper: insert ownerDeviceId into m_ReverseOwners[rKey] (deduplicated)
    protected void ReverseOwnersInsert(string rKey, string ownerDeviceId)
    {
        ref array<string> owners;
        if (!m_ReverseOwners.Find(rKey, owners) || !owners)
        {
            owners = new array<string>;
            m_ReverseOwners[rKey] = owners;
        }
        int oi;
        for (oi = 0; oi < owners.Count(); oi = oi + 1)
        {
            if (owners[oi] == ownerDeviceId)
                return;
        }
        owners.Insert(ownerDeviceId);
    }

    // O(1) lookup via reverse index
    int CountWiresTargeting(string targetDeviceId, string targetPort)
    {
        #ifdef SERVER
        if (targetPort == "")
        {
            targetPort = "input_main";
        }

        string rKey = targetDeviceId + "|" + targetPort;
        int count = 0;
        m_ReverseIdx.Find(rKey, count);
        return count;
        #else
        return 0;
        #endif
    }

    // Incremental reverse index: add one wire entry + track owner
    void ReverseIdxAdd(string targetDeviceId, string targetPort, string ownerDeviceId = "")
    {
        #ifdef SERVER
        if (targetPort == "")
        {
            targetPort = "input_main";
        }
        string rKey = targetDeviceId + "|" + targetPort;
        int prev = 0;
        m_ReverseIdx.Find(rKey, prev);
        m_ReverseIdx[rKey] = prev + 1;

        // Track owner reference for directed removal
        if (ownerDeviceId != "")
        {
            ref array<string> owners;
            if (!m_ReverseOwners.Find(rKey, owners) || !owners)
            {
                owners = new array<string>;
                m_ReverseOwners[rKey] = owners;
            }
            // Deduplicate (same owner can have multiple wires to same target)
            bool found = false;
            int oi;
            for (oi = 0; oi < owners.Count(); oi = oi + 1)
            {
                if (owners[oi] == ownerDeviceId)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                owners.Insert(ownerDeviceId);
            }
        }
        #endif
    }

    // Incremental reverse index: remove one wire entry
    void ReverseIdxRemove(string targetDeviceId, string targetPort, string ownerDeviceId = "")
    {
        #ifdef SERVER
        if (targetPort == "")
        {
            targetPort = "input_main";
        }
        string rKey = targetDeviceId + "|" + targetPort;
        int prev = 0;
        if (m_ReverseIdx.Find(rKey, prev))
        {
            if (prev <= 1)
            {
                m_ReverseIdx.Remove(rKey);
                m_ReverseOwners.Remove(rKey);
            }
            else
            {
                m_ReverseIdx[rKey] = prev - 1;
                // Note: owner ref stays until count reaches 0 or full rebuild.
                // This is safe because RemoveWiresTargeting validates ownership.
            }
        }
        #endif
    }

    // ===========================
    // Per-player wire counter (incremental O(1))
    // ===========================
    void PlayerWireCountAdd(string creatorId, int delta)
    {
        #ifdef SERVER
        if (creatorId == "") return;
        int prev = 0;
        m_WiresByPlayer.Find(creatorId, prev);
        int next = prev + delta;
        if (next < 0)
        {
            next = 0;
        }
        m_WiresByPlayer[creatorId] = next;
        #endif
    }

    // Full recount from all wire stores (used on self-heal)
    protected void RecountAllPlayerWires()
    {
        #ifdef SERVER
        m_WiresByPlayer.Clear();

        // Count LFPG device wires
        array<EntityAI> all = new array<EntityAI>;
        LFPG_DeviceRegistry.Get().GetAll(all);

        int i;
        for (i = 0; i < all.Count(); i = i + 1)
        {
            if (!LFPG_DeviceAPI.HasWireStore(all[i])) continue;

            ref array<ref LFPG_WireData> wires = LFPG_DeviceAPI.GetDeviceWires(all[i]);
            if (!wires) continue;

            int w;
            for (w = 0; w < wires.Count(); w = w + 1)
            {
                LFPG_WireData wd = wires[w];
                if (!wd || wd.m_CreatorId == "") continue;
                PlayerWireCountAdd(wd.m_CreatorId, 1);
            }
        }

        // Count vanilla wires
        int vk;
        for (vk = 0; vk < m_VanillaWires.Count(); vk = vk + 1)
        {
            ref array<ref LFPG_WireData> vWires = m_VanillaWires.GetElement(vk);
            if (!vWires) continue;

            int vw;
            for (vw = 0; vw < vWires.Count(); vw = vw + 1)
            {
                LFPG_WireData vwd = vWires[vw];
                if (!vwd || vwd.m_CreatorId == "") continue;
                PlayerWireCountAdd(vwd.m_CreatorId, 1);
            }
        }
        #endif
    }

    // Remove all wires targeting a specific device+port from known sources.
    // Uses m_ReverseOwners for directed lookup: only scans owners that
    // actually have wires to this target. O(affected_owners) instead of O(all_devices).
    int RemoveWiresTargeting(string targetDeviceId, string targetPort)
    {
        #ifdef SERVER
        int removed = 0;

        string normPort = targetPort;
        if (normPort == "")
        {
            normPort = "input_main";
        }
        string rKey = targetDeviceId + "|" + normPort;

        // Get list of owners that have wires to this target
        ref array<string> owners;
        if (!m_ReverseOwners.Find(rKey, owners) || !owners || owners.Count() == 0)
        {
            // No owners known — nothing to remove
            return 0;
        }

        // Copy owner list (we'll modify m_ReverseOwners during iteration)
        ref array<string> ownersCopy = new array<string>;
        int oc;
        for (oc = 0; oc < owners.Count(); oc = oc + 1)
        {
            ownersCopy.Insert(owners[oc]);
        }

        // Process each known owner
        int oi;
        for (oi = 0; oi < ownersCopy.Count(); oi = oi + 1)
        {
            string ownerId = ownersCopy[oi];

            // Try LFPG device
            EntityAI ownerObj = LFPG_DeviceRegistry.Get().FindById(ownerId);
            if (ownerObj && LFPG_DeviceAPI.HasWireStore(ownerObj))
            {
                ref array<ref LFPG_WireData> gWires = LFPG_DeviceAPI.GetDeviceWires(ownerObj);
                if (gWires)
                {
                    bool ownerChanged = false;
                    int gw = gWires.Count() - 1;
                    while (gw >= 0)
                    {
                        LFPG_WireData wd = gWires[gw];
                        if (wd && wd.m_TargetDeviceId == targetDeviceId && wd.m_TargetPort == targetPort)
                        {
                            PlayerWireCountAdd(wd.m_CreatorId, -1);
                            gWires.Remove(gw);
                            removed = removed + 1;
                            ownerChanged = true;
                            LFPG_Util.Info("[PortReplace] Removed wire from " + ownerId + " -> " + targetDeviceId + ":" + normPort);
                        }
                        gw = gw - 1;
                    }
                    if (ownerChanged)
                    {
                        ownerObj.SetSynchDirty();
                        QueueBroadcastOwner(ownerObj);
                        RequestPropagate(ownerId);
                    }
                }
                continue;
            }

            // Try vanilla store
            ref array<ref LFPG_WireData> vWires;
            if (m_VanillaWires.Find(ownerId, vWires) && vWires)
            {
                bool vChanged = false;
                int vw = vWires.Count() - 1;
                while (vw >= 0)
                {
                    LFPG_WireData vwd = vWires[vw];
                    if (vwd && vwd.m_TargetDeviceId == targetDeviceId && vwd.m_TargetPort == targetPort)
                    {
                        PlayerWireCountAdd(vwd.m_CreatorId, -1);
                        vWires.Remove(vw);
                        removed = removed + 1;
                        vChanged = true;
                        LFPG_Util.Info("[PortReplace] Removed vanilla wire from " + ownerId + " -> " + targetDeviceId + ":" + normPort);
                    }
                    vw = vw - 1;
                }
                if (vChanged)
                {
                    EntityAI vObj = LFPG_DeviceRegistry.Get().FindById(ownerId);
                    if (vObj)
                    {
                        QueueBroadcastVanilla(ownerId, vObj);
                    }
                    RequestPropagate(ownerId);
                }
            }
        }

        // Clean up reverse index for this target
        if (removed > 0)
        {
            m_ReverseIdx.Remove(rKey);
            m_ReverseOwners.Remove(rKey);
            MarkVanillaDirty();
            FlushBroadcasts();
        }

        return removed;
        #else
        return 0;
        #endif
    }

    // ===========================
    // Quotas / anti-grief (O(1) via incremental counter)
    // ===========================
    bool CanPlayerCreateAnotherWire(PlayerIdentity ident, out string reason)
    {
        reason = "";
        #ifdef SERVER
        if (!ident)
        {
            reason = "no identity";
            return false;
        }

        LFPG_ServerSettings st = LFPG_Settings.Get();
        int limit = LFPG_MAX_WIRES_PER_PLAYER;
        if (st && st.MaxWiresPerPlayer > 0)
        {
            limit = st.MaxWiresPerPlayer;
        }

        if (limit <= 0)
            return true;

        string pid = ident.GetPlainId();
        int count = 0;
        m_WiresByPlayer.Find(pid, count);

        if (count >= limit)
        {
            reason = "MaxWiresPerPlayer reached (" + count.ToString() + "/" + limit.ToString() + ")";
            return false;
        }
        #endif

        return true;
    }

    // ===========================
    // Wire geometry validation
    // ===========================
    bool ValidateWire(vector startPos, vector endPos, array<vector> waypoints, out string reason)
    {
        reason = "";

        int wpCount = 0;
        if (waypoints)
        {
            wpCount = waypoints.Count();
        }

        if (wpCount > LFPG_MAX_WAYPOINTS)
        {
            reason = "Too many waypoints";
            return false;
        }

        vector prev = startPos;
        float total = 0.0;

        int i;
        for (i = 0; i < wpCount; i = i + 1)
        {
            float seg = vector.Distance(prev, waypoints[i]);
            if (seg > LFPG_MAX_SEGMENT_LEN_M)
            {
                reason = "Segment too long";
                return false;
            }
            total = total + seg;
            prev = waypoints[i];
        }

        float lastSeg = vector.Distance(prev, endPos);
        if (lastSeg > LFPG_MAX_SEGMENT_LEN_M)
        {
            reason = "Last segment too long";
            return false;
        }
        total = total + lastSeg;

        if (total > LFPG_MAX_WIRE_LEN_M)
        {
            reason = "Wire too long total";
            return false;
        }

        return true;
    }

    // ===========================
    // Broadcast batching
    // ===========================
    // Queue a broadcast instead of sending immediately.
    // Call FlushBroadcasts() when all mutations are done.
    void QueueBroadcastOwner(EntityAI owner)
    {
        if (!owner) return;
        string devId = LFPG_DeviceAPI.GetDeviceId(owner);
        if (devId == "") return;
        m_PendingBroadcastLFPG[devId] = owner;
    }

    void QueueBroadcastVanilla(string ownerDeviceId, EntityAI ownerObj)
    {
        if (ownerDeviceId == "" || !ownerObj) return;
        m_PendingBroadcastVanilla[ownerDeviceId] = ownerObj;
    }

    // Flush all queued broadcasts (deduplicated by owner).
    // Called once after a batch of mutations finishes.
    void FlushBroadcasts()
    {
        // Flush LFPG owners
        int i;
        for (i = 0; i < m_PendingBroadcastLFPG.Count(); i = i + 1)
        {
            EntityAI owner = m_PendingBroadcastLFPG.GetElement(i);
            if (owner)
            {
                BroadcastOwnerWires(owner);
            }
        }
        m_PendingBroadcastLFPG.Clear();

        // Flush vanilla owners
        int v;
        for (v = 0; v < m_PendingBroadcastVanilla.Count(); v = v + 1)
        {
            string vId = m_PendingBroadcastVanilla.GetKey(v);
            EntityAI vObj = m_PendingBroadcastVanilla.GetElement(v);
            if (vObj)
            {
                BroadcastVanillaWires(vId, vObj);
            }
        }
        m_PendingBroadcastVanilla.Clear();
    }

    // ===========================
    // Sync: LFPG source -> clients
    // ===========================
    void BroadcastOwnerWires(EntityAI owner)
    {
        if (!owner) return;

        string json = LFPG_DeviceAPI.GetWiresJSON(owner);
        string ownerId = LFPG_DeviceAPI.GetDeviceId(owner);

        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);

        int low = 0;
        int high = 0;
        owner.GetNetworkID(low, high);

        LFPG_Util.Info("[BroadcastOwnerWires] owner=" + ownerId + " net=" + low.ToString() + ":" + high.ToString() + " type=" + owner.GetType() + " jsonLen=" + json.Length().ToString());

        // v0.7.11 (A3): Precompute squared threshold for player distance culling.
        float syncMaxDist = LFPG_CULL_DISTANCE_M + 20.0;
        float syncMaxDistSq = syncMaxDist * syncMaxDist;
        vector ownerPos = owner.GetPosition();

        int i;
        for (i = 0; i < players.Count(); i = i + 1)
        {
            PlayerBase pb = PlayerBase.Cast(players[i]);
            if (!pb) continue;

            // v0.7.11 (A3): Compare in squared domain — eliminates sqrt per player.
            if (LFPG_WorldUtil.DistSq(pb.GetPosition(), ownerPos) > syncMaxDistSq)
                continue;

            ScriptRPC rpc = new ScriptRPC();
            rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES);
            rpc.Write(ownerId);
            rpc.Write(low);
            rpc.Write(high);
            rpc.Write(json);
            rpc.Send(pb, LFPG_RPC_CHANNEL, true, null);
        }
    }

    // ===========================
    // Sync: vanilla source -> clients
    // ===========================
    void BroadcastVanillaWires(string ownerDeviceId, EntityAI ownerObj)
    {
        if (ownerDeviceId == "" || !ownerObj) return;

        ref array<ref LFPG_WireData> wires = GetVanillaWires(ownerDeviceId);

        // Serialize using PersistBlob format (same as LFPG)
        LFPG_PersistBlob blob = new LFPG_PersistBlob();
        blob.ver = LFPG_PERSIST_VER;
        if (wires)
        {
            int w;
            for (w = 0; w < wires.Count(); w = w + 1)
            {
                blob.wires.Insert(wires[w]);
            }
        }

        string json;
        string err;
        if (!JsonFileLoader<LFPG_PersistBlob>.MakeData(blob, json, err, false))
        {
            json = "";
        }

        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);

        int low = 0;
        int high = 0;
        ownerObj.GetNetworkID(low, high);

        // v0.7.11 (A3): Precompute squared threshold for player distance culling.
        float vSyncMaxDist = LFPG_CULL_DISTANCE_M + 20.0;
        float vSyncMaxDistSq = vSyncMaxDist * vSyncMaxDist;
        vector ownerObjPos = ownerObj.GetPosition();

        int i;
        for (i = 0; i < players.Count(); i = i + 1)
        {
            PlayerBase pb = PlayerBase.Cast(players[i]);
            if (!pb) continue;

            // v0.7.11 (A3): Compare in squared domain — eliminates sqrt per player.
            if (LFPG_WorldUtil.DistSq(pb.GetPosition(), ownerObjPos) > vSyncMaxDistSq)
                continue;

            ScriptRPC rpc = new ScriptRPC();
            rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES);
            rpc.Write(ownerDeviceId);
            rpc.Write(low);
            rpc.Write(high);
            rpc.Write(json);
            rpc.Send(pb, LFPG_RPC_CHANNEL, true, null);
        }
    }

    // ===========================
    // Sync: vanilla source -> single player (unicast)
    // ===========================
    void SendVanillaWiresTo(PlayerBase player, string ownerDeviceId, EntityAI ownerObj)
    {
        if (!player || ownerDeviceId == "" || !ownerObj) return;

        ref array<ref LFPG_WireData> wires = GetVanillaWires(ownerDeviceId);

        LFPG_PersistBlob blob = new LFPG_PersistBlob();
        blob.ver = LFPG_PERSIST_VER;
        if (wires)
        {
            int w;
            for (w = 0; w < wires.Count(); w = w + 1)
            {
                blob.wires.Insert(wires[w]);
            }
        }

        string json;
        string err;
        if (!JsonFileLoader<LFPG_PersistBlob>.MakeData(blob, json, err, false))
        {
            json = "";
        }

        int low = 0;
        int high = 0;
        ownerObj.GetNetworkID(low, high);

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES);
        rpc.Write(ownerDeviceId);
        rpc.Write(low);
        rpc.Write(high);
        rpc.Write(json);
        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);
    }

    // ===========================
    // Full sync to joining player
    // ===========================
    void SendFullSyncTo(PlayerBase player)
    {
        if (!player) return;

        vector pp = player.GetPosition();
        float maxDist = LFPG_CULL_DISTANCE_M + 20.0;
        // v0.7.11 (A3): Precompute squared threshold for distance culling.
        float maxDistSq = maxDist * maxDist;

        array<EntityAI> all = new array<EntityAI>;
        LFPG_DeviceRegistry.Get().GetAll(all);

        LFPG_Util.Info("[FullSync] devices=" + all.Count().ToString() + " playerPos=" + pp.ToString());

        // Sync LFPG wire-owning devices (Generator, Splitter, etc.)
        int i;
        for (i = 0; i < all.Count(); i = i + 1)
        {
            if (!LFPG_DeviceAPI.HasWireStore(all[i])) continue;

            // v0.7.11 (A3): Compare in squared domain — eliminates sqrt per device.
            if (LFPG_WorldUtil.DistSq(pp, all[i].GetPosition()) > maxDistSq)
                continue;

            string devId = LFPG_DeviceAPI.GetDeviceId(all[i]);
            string json = LFPG_DeviceAPI.GetWiresJSON(all[i]);

            ScriptRPC rpc = new ScriptRPC();
            rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES);
            rpc.Write(devId);

            int low = 0;
            int high = 0;
            all[i].GetNetworkID(low, high);
            rpc.Write(low);
            rpc.Write(high);

            LFPG_Util.Info("[FullSync] LFPG dev=" + devId + " net=" + low.ToString() + ":" + high.ToString() + " type=" + all[i].GetType() + " jsonLen=" + json.Length().ToString());

            rpc.Write(json);

            rpc.Send(player, LFPG_RPC_CHANNEL, true, null);
        }

        // Sync vanilla source wires (UNICAST to this player only)
        int vk;
        for (vk = 0; vk < m_VanillaWires.Count(); vk = vk + 1)
        {
            string vId = m_VanillaWires.GetKey(vk);
            EntityAI vObj = LFPG_DeviceRegistry.Get().FindById(vId);
            if (!vObj) continue;

            // v0.7.11 (A3): Compare in squared domain — eliminates sqrt per vanilla device.
            if (LFPG_WorldUtil.DistSq(pp, vObj.GetPosition()) > maxDistSq)
                continue;

            SendVanillaWiresTo(player, vId, vObj);
        }
    }

    // ===========================
    // Propagation (Sprint 4.2: event-driven via graph)
    // ===========================

    // Request propagation from a source device.
    // Sprint 4.2: Marks the source node dirty in the graph.
    // The periodic TickPropagation() will process it via ProcessDirtyQueue.
    void RequestPropagate(string sourceDeviceId)
    {
        #ifdef SERVER
        if (sourceDeviceId == "") return;

        if (!m_Graph)
        {
            LFPG_Util.Warn("[Propagate] Graph null, cannot propagate " + sourceDeviceId);
            return;
        }

        // Refresh source on/off state in the graph from the entity
        m_Graph.RefreshSourceState(sourceDeviceId);

        // Mark dirty — will be picked up by next TickPropagation
        m_Graph.MarkNodeDirty(sourceDeviceId, LFPG_DIRTY_INTERNAL);

        LFPG_Util.Debug("[Propagate] Queued dirty: " + sourceDeviceId);
        #endif
    }

    // Sprint 4.2+4.3: Periodic propagation tick.
    // Called every LFPG_PROPAGATE_TICK_MS (100ms = 10Hz).
    // Sprint 4.2 S2b (H3): Uses WARMUP_BUDGET during startup/self-heal drain.
    // Sprint 4.3: Accumulates telemetry, dumps every LFPG_TELEM_INTERVAL_MS.
   protected void TickPropagation()
    {
        #ifdef SERVER
        if (!m_Graph)
            return;

        int nodeBudget = LFPG_PROPAGATE_NODE_BUDGET;
        int edgeBudget = LFPG_PROPAGATE_EDGE_BUDGET;
        if (m_WarmupActive)
        {
            nodeBudget = LFPG_PROPAGATE_WARMUP_BUDGET;
            edgeBudget = LFPG_PROPAGATE_EDGE_WARMUP_BUDGET;
        }
        else
        {
            // v0.7.26 (Audit 4): Dynamic budget scaling for large dirty queues.
            // When queue is deep, double the edge budget to prevent "phantom brownout"
            // where propagation can't keep up and consumers flicker.
            int queueSize = m_Graph.GetDirtyQueueSize();
            if (queueSize > LFPG_DYNAMIC_BUDGET_QUEUE_THRESHOLD)
            {
                edgeBudget = edgeBudget * 2;
                nodeBudget = nodeBudget * 2;
            }
        }

        int remaining = m_Graph.ProcessDirtyQueue(nodeBudget, edgeBudget);

        if (remaining <= 0 && m_WarmupActive)
        {
            m_WarmupActive = false;
            LFPG_Util.Info("[Propagate] Warmup drain complete");
        }

        if (remaining > 0)
        {
            LFPG_Util.Debug("[Propagate] Tick: " + remaining.ToString() + " remaining"
                + " nodeBudget=" + nodeBudget.ToString()
                + " edgeBudget=" + edgeBudget.ToString()
                + " edgesUsed=" + m_Graph.GetLastEdgesVisited().ToString());
        }

        // Sprint 4.3: Accumulate propagation telemetry
        int processMs = m_Graph.GetLastProcessMs();
        int edgesUsed = m_Graph.GetLastEdgesVisited();

        m_TelemTickCount = m_TelemTickCount + 1;
        m_TelemTotalProcessMs = m_TelemTotalProcessMs + processMs;
        m_TelemTotalEdgesVisited = m_TelemTotalEdgesVisited + edgesUsed;
        if (processMs > m_TelemPeakProcessMs)
        {
            m_TelemPeakProcessMs = processMs;
        }

        // Periodic telemetry dump
        float nowMs = GetGame().GetTickCount();
        float elapsed = nowMs - m_TelemLastDumpMs;
        if (m_TelemLastDumpMs < 0.0)
        {
            m_TelemLastDumpMs = nowMs;
        }
        else if (elapsed >= LFPG_TELEM_INTERVAL_MS && m_TelemTickCount > 0)
        {
            // v0.7.26 (Audit 4): Division-by-zero guard for telemetry averages.
            int avgMs = 0;
            int avgEdges = 0;
            if (m_TelemTickCount > 0)
            {
                avgMs = m_TelemTotalProcessMs / m_TelemTickCount;
                avgEdges = m_TelemTotalEdgesVisited / m_TelemTickCount;
            }
            int overloadCount = m_Graph.GetOverloadedSourceCount();

            string tLog = "[Telemetry-Propagation]"
                + " ticks=" + m_TelemTickCount.ToString()
                + " avgMs=" + avgMs.ToString()
                + " peakMs=" + m_TelemPeakProcessMs.ToString()
                + " avgEdges=" + avgEdges.ToString()
                + " nodes=" + m_Graph.GetNodeCount().ToString()
                + " edges=" + m_Graph.GetEdgeCount().ToString()
                + " queueRemain=" + remaining.ToString()
                + " overloadedSources=" + overloadCount.ToString()
                + " epoch=" + m_Graph.GetCurrentEpoch().ToString();
            LFPG_Util.Info(tLog);

            // Reset accumulators
            m_TelemTickCount = 0;
            m_TelemTotalProcessMs = 0;
            m_TelemPeakProcessMs = 0;
            m_TelemTotalNodesProcessed = 0;
            m_TelemTotalEdgesVisited = 0;
            m_TelemLastDumpMs = nowMs;
        }
        #endif
    }


    // ===========================
    // v0.7.30 (Audit 1+2): Centralized position polling
    // ===========================
    // Round-robin batched: processes LFPG_MOVE_DETECT_BATCH_SIZE devices per
    // tick from m_TrackedDeviceIds. Only devices with active wires are tracked.
    // Replaces: (a) full-scan CheckDeviceMovement every 3s (v0.7.23-0.7.29)
    //           (b) N per-device timers in generator/lamp (v0.7.29)
    // Uses DistSq to avoid sqrt per check.

    // ---- Track / Untrack ----
    // Called when wires are added or removed. Maintains the set of devices
    // that need position monitoring. O(1) insert, O(1) swap-and-pop removal.

    void TrackDeviceForPolling(string deviceId)
    {
        #ifdef SERVER
        if (!GetGame().IsServer())
            return;

        if (deviceId == "")
            return;

        int existingIdx;
        if (m_TrackedDeviceIndex.Find(deviceId, existingIdx))
            return;  // already tracked

        int idx = m_TrackedDeviceIds.Count();
        m_TrackedDeviceIds.Insert(deviceId);
        m_TrackedDeviceIndex.Set(deviceId, idx);

        // Initialize position snapshot
        EntityAI dev = LFPG_DeviceRegistry.Get().FindById(deviceId);
        if (dev)
        {
            m_LastKnownPos.Set(deviceId, dev.GetPosition());
        }
        #endif
    }

    void UntrackDeviceFromPolling(string deviceId)
    {
        #ifdef SERVER
        if (!GetGame().IsServer())
            return;

        if (deviceId == "")
            return;

        int idx;
        if (!m_TrackedDeviceIndex.Find(deviceId, idx))
            return;  // not tracked

        int lastIdx = m_TrackedDeviceIds.Count() - 1;

        if (idx < lastIdx)
        {
            // Swap with last element to keep array compact
            string lastId = m_TrackedDeviceIds[lastIdx];
            m_TrackedDeviceIds[idx] = lastId;
            m_TrackedDeviceIndex.Set(lastId, idx);
        }

        // Pop last
        m_TrackedDeviceIds.Remove(lastIdx);
        m_TrackedDeviceIndex.Remove(deviceId);

        // Clean up position tracking
        m_LastKnownPos.Remove(deviceId);

        // Clamp cursor to valid range
        if (m_TrackCursor >= m_TrackedDeviceIds.Count())
        {
            m_TrackCursor = 0;
        }
        #endif
    }

    // ---- DeviceHasAnyWires ----
    // Returns true if the device has any owned wires (output side) or
    // any incoming wires (input side via reverse index).
    // Used by CutAllWiresFromDevice to decide whether to untrack.
    protected bool DeviceHasAnyWires(EntityAI device, string deviceId)
    {
        #ifdef SERVER
        if (!GetGame().IsServer())
            return false;

        if (!device || deviceId == "")
            return false;

        // Check owned wires (sources, splitters)
        if (LFPG_DeviceAPI.HasWireStore(device))
        {
            ref array<ref LFPG_WireData> ownedWires = LFPG_DeviceAPI.GetDeviceWires(device);
            if (ownedWires && ownedWires.Count() > 0)
            {
                return true;
            }
        }

        // Check incoming wires via reverse index (consumers, passthroughs)
        int portCount = LFPG_DeviceAPI.GetPortCount(device);
        int pci;
        for (pci = 0; pci < portCount; pci = pci + 1)
        {
            int pdChk = LFPG_DeviceAPI.GetPortDir(device, pci);
            if (pdChk == LFPG_PortDir.IN)
            {
                string pnChk = LFPG_DeviceAPI.GetPortName(device, pci);
                if (CountWiresTargeting(deviceId, pnChk) > 0)
                {
                    return true;
                }
            }
        }
        #endif

        return false;
    }

    // ---- RebuildTrackedDevices ----
    // Rebuilds the tracked set from current wire state. Called after
    // ValidateAllWiresAndPropagate (startup, self-heal) to ensure
    // the tracked set matches the validated wire topology.
    protected void RebuildTrackedDevices()
    {
        #ifdef SERVER
        if (!GetGame().IsServer())
            return;

        m_TrackedDeviceIds.Clear();
        m_TrackedDeviceIndex.Clear();
        m_TrackCursor = 0;

        ref array<EntityAI> allDevs = new array<EntityAI>;
        LFPG_DeviceRegistry.Get().GetAll(allDevs);

        int i;
        for (i = 0; i < allDevs.Count(); i = i + 1)
        {
            EntityAI dev = allDevs[i];
            if (!dev)
                continue;

            string devId = LFPG_DeviceAPI.GetOrCreateDeviceId(dev);
            if (devId == "")
                continue;

            if (DeviceHasAnyWires(dev, devId))
            {
                int insertIdx = m_TrackedDeviceIds.Count();
                m_TrackedDeviceIds.Insert(devId);
                m_TrackedDeviceIndex.Set(devId, insertIdx);
                m_LastKnownPos.Set(devId, dev.GetPosition());
            }
        }

        LFPG_Util.Info("[Movement] RebuildTrackedDevices: tracking " + m_TrackedDeviceIds.Count().ToString() + " wired devices");
        #endif
    }

    // ---- CheckDeviceMovement (round-robin batched) ----
    // Processes LFPG_MOVE_DETECT_BATCH_SIZE devices per tick.
    // Uses LFPG_WorldUtil.DistSq to avoid sqrt per check.
    // Devices that moved have all wires cut and are untracked.
    // Disappeared devices (null in registry) are silently untracked.
    protected void CheckDeviceMovement()
    {
        #ifdef SERVER
        if (!GetGame().IsServer())
            return;

        int totalTracked = m_TrackedDeviceIds.Count();
        if (totalTracked == 0)
            return;

        // Clamp cursor
        if (m_TrackCursor >= totalTracked)
        {
            m_TrackCursor = 0;
        }

        int batchEnd = m_TrackCursor + LFPG_MOVE_DETECT_BATCH_SIZE;
        if (batchEnd > totalTracked)
        {
            batchEnd = totalTracked;
        }

        // Collect moved/disappeared devices (can't modify tracked array during iteration)
        ref array<string> movedIds = new array<string>;
        ref array<EntityAI> movedDevs = new array<EntityAI>;
        ref array<string> disappearedIds = new array<string>;

        int i;
        for (i = m_TrackCursor; i < batchEnd; i = i + 1)
        {
            string devId = m_TrackedDeviceIds[i];
            if (devId == "")
                continue;

            EntityAI dev = LFPG_DeviceRegistry.Get().FindById(devId);
            if (!dev)
            {
                // Device disappeared from registry — mark for untrack
                disappearedIds.Insert(devId);
                continue;
            }

            vector currentPos = dev.GetPosition();
            vector lastPos;
            bool hadPos = m_LastKnownPos.Find(devId, lastPos);

            if (hadPos)
            {
                float distSq = LFPG_WorldUtil.DistSq(currentPos, lastPos);
                if (distSq > LFPG_MOVE_DETECT_THRESHOLD_SQ)
                {
                    movedIds.Insert(devId);
                    movedDevs.Insert(dev);
                }
            }

            // Always update position snapshot
            m_LastKnownPos.Set(devId, currentPos);
        }

        // Advance cursor (wraps naturally on next tick)
        m_TrackCursor = batchEnd;

        // Process disappeared devices (just untrack, no wire cut needed)
        int di;
        for (di = 0; di < disappearedIds.Count(); di = di + 1)
        {
            UntrackDeviceFromPolling(disappearedIds[di]);
        }

        // Process moved devices
        int mi;
        for (mi = 0; mi < movedIds.Count(); mi = mi + 1)
        {
            EntityAI movedDev = movedDevs[mi];
            string movedId = movedIds[mi];

            LFPG_Util.Warn("[Movement] Device " + movedId + " type=" + movedDev.GetType() + " moved — disconnecting wires");

            // CutAllWiresFromDevice handles: owned wires, vanilla wires,
            // incoming wires, graph cleanup, SetPowered(false) on neighbors,
            // and auto-untrack via the hook at the end of CutAllWiresFromDevice.
            CutAllWiresFromDevice(movedDev);

            // Generator-specific: force source off when physically moved.
            // CutAllWiresFromDevice handles consumers/passthroughs via
            // SetPowered(false), but generators produce (not consume).
            LF_TestGenerator gen = LF_TestGenerator.Cast(movedDev);
            if (gen && gen.LFPG_GetSwitchState())
            {
                gen.LFPG_ToggleSource();
            }
        }

        // Trigger self-heal if any devices moved
        if (movedIds.Count() > 0)
        {
            LFPG_Util.Info("[Movement] " + movedIds.Count().ToString() + " devices moved, requesting self-heal");
            RequestGlobalSelfHeal();
        }
        #endif
    }


    // ===========================
    // Self-healing
    // ===========================
    void RequestGlobalSelfHeal()
    {
        #ifdef SERVER
        if (m_SelfHealQueued) return;
        m_SelfHealQueued = true;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DoGlobalSelfHeal, 500, false);
        #endif
    }

    protected void DoGlobalSelfHeal()
    {
        #ifdef SERVER
        m_SelfHealQueued = false;
        ValidateAllWiresAndPropagate();
        #endif
    }

    void ValidateAllWiresAndPropagate()
    {
        #ifdef SERVER
        int ownersPruned = 0;
        int sourcesQueued = 0;

        // Step 1: Resolve all vanilla wire endpoints FIRST.
        // After restart, vanilla devices aren't in DeviceRegistry.
        // Spatial scan re-registers them so they appear in validIds below.
        // We scan: (a) m_VanillaWires owners and targets, and
        //          (b) LFPG device wire targets that use "vp:" IDs.

        // 1a) Vanilla wire owners and their targets
        int vr;
        for (vr = 0; vr < m_VanillaWires.Count(); vr = vr + 1)
        {
            string vrOwnerId = m_VanillaWires.GetKey(vr);
            if (!LFPG_DeviceRegistry.Get().FindById(vrOwnerId))
            {
                LFPG_DeviceAPI.ResolveVanillaDevice(vrOwnerId);
            }
            ref array<ref LFPG_WireData> vrWires = m_VanillaWires.GetElement(vr);
            if (vrWires)
            {
                int vrw;
                for (vrw = 0; vrw < vrWires.Count(); vrw = vrw + 1)
                {
                    LFPG_WireData vrWd = vrWires[vrw];
                    if (vrWd && vrWd.m_TargetDeviceId != "")
                    {
                        if (!LFPG_DeviceRegistry.Get().FindById(vrWd.m_TargetDeviceId))
                        {
                            LFPG_DeviceAPI.ResolveVanillaDevice(vrWd.m_TargetDeviceId);
                        }
                    }
                }
            }
        }

        // 1b) LFPG device wire targets that are vanilla ("vp:" prefix)
        array<EntityAI> preAll = new array<EntityAI>;
        LFPG_DeviceRegistry.Get().GetAll(preAll);
        int pa;
        for (pa = 0; pa < preAll.Count(); pa = pa + 1)
        {
            ref array<ref LFPG_WireData> lfWires = LFPG_DeviceAPI.GetDeviceWires(preAll[pa]);
            if (!lfWires) continue;
            int lw;
            for (lw = 0; lw < lfWires.Count(); lw = lw + 1)
            {
                LFPG_WireData lwd = lfWires[lw];
                if (!lwd) continue;
                string tid = lwd.m_TargetDeviceId;
                if (tid.IndexOf("vp:") == 0)
                {
                    if (!LFPG_DeviceRegistry.Get().FindById(tid))
                    {
                        LFPG_DeviceAPI.ResolveVanillaDevice(tid);
                    }
                }
            }
        }

        // Step 2: Get all registered devices (now includes resolved vanilla)
        // v0.7.4: prune null entries first (engine invalidation, streaming out)
        LFPG_DeviceRegistry.Get().PruneNullEntries();
        array<EntityAI> all = new array<EntityAI>;
        LFPG_DeviceRegistry.Get().GetAll(all);

        int deviceCount = all.Count();
        LFPG_Util.Debug("SelfHeal: scanning devices=" + deviceCount.ToString());

        // Step 3: Build valid device IDs ONCE for all PruneMissingTargets calls
        m_CachedValidIds = new map<string, bool>;
        int vi;
        for (vi = 0; vi < all.Count(); vi = vi + 1)
        {
            string did = LFPG_DeviceAPI.GetOrCreateDeviceId(all[vi]);
            if (did != "")
            {
                m_CachedValidIds[did] = true;
            }
        }

        // Step 4: Prune and propagate LFPG devices
        int i;
        for (i = 0; i < all.Count(); i = i + 1)
        {
            if (!LFPG_DeviceAPI.HasWireStore(all[i])) continue;

            bool changed = LFPG_DeviceAPI.PruneDeviceMissingTargets(all[i]);
            if (changed)
            {
                ownersPruned = ownersPruned + 1;
                BroadcastOwnerWires(all[i]);
            }

            string devId = LFPG_DeviceAPI.GetDeviceId(all[i]);
            if (LFPG_DeviceAPI.IsSource(all[i]) && LFPG_DeviceAPI.GetSourceOn(all[i]))
            {
                RequestPropagate(devId);
                sourcesQueued = sourcesQueued + 1;
            }
        }

        // Clear cache after all prune calls are done
        m_CachedValidIds = null;

        // Step 5: Propagate from vanilla sources
        int vk;
        for (vk = 0; vk < m_VanillaWires.Count(); vk = vk + 1)
        {
            string vId = m_VanillaWires.GetKey(vk);
            EntityAI vObj = LFPG_DeviceRegistry.Get().FindById(vId);
            if (!vObj) continue;

            if (LFPG_DeviceAPI.IsEnergySource(vObj))
            {
                RequestPropagate(vId);
                sourcesQueued = sourcesQueued + 1;
            }
        }

        LFPG_Util.Debug("SelfHeal: done. pruned=" + ownersPruned.ToString() + " propagated=" + sourcesQueued.ToString());

        // Full rebuild of indexes on self-heal (authoritative baseline)
        RebuildReverseIdx();
        RecountAllPlayerWires();

        // v0.7.4: prune vanilla wires whose owner or target can't be resolved.
        // Devices that were moved/destroyed leave orphaned wires in the
        // central store. Persist the pruned state on next flush.
        PruneUnresolvableVanillaWires();

        // Sprint 4.1→4.2: rebuild electrical graph from wire data,
        // then populate node electrical states and mark sources dirty.
        // Must happen after all prune/resolve passes so graph reflects
        // the clean, validated wire state.
        if (m_Graph)
        {
            m_Graph.RebuildFromWires(this);
            m_Graph.PopulateAllNodeElecStates();
            m_Graph.MarkSourcesDirty();
            m_WarmupActive = true;
            LFPG_Util.Info("[SelfHeal] Graph warmup: rebuilt + populated + sources dirty (warmup budget active)");
        }

        // v0.7.26 (Audit 4): Prune stale entries from m_LastKnownPos.
        // Devices that were deleted/destroyed leave orphan entries.
        // Without cleanup, this map grows unboundedly on long-running servers.
        PruneStaleLastKnownPositions();

        // v0.7.30: Rebuild tracked device set from validated wire state.
        // Must happen after graph rebuild + prune so the tracked set matches
        // the authoritative wire topology.
        RebuildTrackedDevices();
        #endif
    }

    // Returns cached valid IDs map if available (during self-heal cycle).
    // Devices call this from LFPG_PruneMissingTargets to avoid building
    // the same map N times. Returns null outside of self-heal.
    map<string, bool> GetCachedValidIds()
    {
        return m_CachedValidIds;
    }

    // v0.7.4: prune vanilla wire entries whose owner or target device
    // can no longer be resolved. After devices are moved or destroyed,
    // their position-based IDs change and wires become orphans.
    // Also removes empty owner entries from m_VanillaWires.
    protected void PruneUnresolvableVanillaWires()
    {
        #ifdef SERVER
        int totalPruned = 0;
        ref array<string> emptyOwners = new array<string>;

        int vk;
        for (vk = 0; vk < m_VanillaWires.Count(); vk = vk + 1)
        {
            string ownerId = m_VanillaWires.GetKey(vk);

            // Check if owner can be resolved
            EntityAI ownerObj = LFPG_DeviceRegistry.Get().FindById(ownerId);
            if (!ownerObj)
            {
                ownerObj = LFPG_DeviceAPI.ResolveVanillaDevice(ownerId);
            }
            if (!ownerObj)
            {
                // Owner gone: remove all wires for this owner
                ref array<ref LFPG_WireData> ownerWires = m_VanillaWires.GetElement(vk);
                int ownerCount = 0;
                if (ownerWires)
                {
                    ownerCount = ownerWires.Count();
                }
                totalPruned = totalPruned + ownerCount;
                emptyOwners.Insert(ownerId);
                continue;
            }

            // Owner exists: check each wire's target
            ref array<ref LFPG_WireData> wires = m_VanillaWires.GetElement(vk);
            if (!wires)
                continue;

            int w = wires.Count() - 1;
            while (w >= 0)
            {
                LFPG_WireData wd = wires[w];
                if (!wd || wd.m_TargetDeviceId == "")
                {
                    wires.Remove(w);
                    totalPruned = totalPruned + 1;
                    w = w - 1;
                    continue;
                }

                EntityAI tObj = LFPG_DeviceRegistry.Get().FindById(wd.m_TargetDeviceId);
                if (!tObj)
                {
                    tObj = LFPG_DeviceAPI.ResolveVanillaDevice(wd.m_TargetDeviceId);
                }
                if (!tObj)
                {
                    LFPG_Util.Debug("[VanillaPrune] Removed wire " + ownerId + " -> " + wd.m_TargetDeviceId + " (target unresolvable)");
                    wires.Remove(w);
                    totalPruned = totalPruned + 1;
                }

                w = w - 1;
            }

            // If owner has no remaining wires, mark for removal
            if (wires.Count() == 0)
            {
                emptyOwners.Insert(ownerId);
            }
        }

        // Remove empty owner entries
        int eo;
        for (eo = 0; eo < emptyOwners.Count(); eo = eo + 1)
        {
            m_VanillaWires.Remove(emptyOwners[eo]);
        }

        if (totalPruned > 0)
        {
            LFPG_Util.Info("[SelfHeal] Pruned " + totalPruned.ToString() + " unresolvable vanilla wire(s), " + emptyOwners.Count().ToString() + " empty owner(s)");
            MarkVanillaDirty();
        }
        #endif
    }

    // v0.7.26 (Audit 4): Remove stale entries from m_LastKnownPos.
    // Called during self-heal. Only keeps entries for devices currently
    // registered. Prevents unbounded map growth on long-running servers
    // where devices are placed and destroyed over time.
    protected void PruneStaleLastKnownPositions()
    {
        #ifdef SERVER
        if (m_LastKnownPos.Count() == 0)
            return;

        ref array<string> staleIds = new array<string>;

        int pk;
        for (pk = 0; pk < m_LastKnownPos.Count(); pk = pk + 1)
        {
            string posKey = m_LastKnownPos.GetKey(pk);
            EntityAI posObj = LFPG_DeviceRegistry.Get().FindById(posKey);
            if (!posObj)
            {
                // Also check vanilla resolution
                posObj = LFPG_DeviceAPI.ResolveVanillaDevice(posKey);
            }
            if (!posObj)
            {
                staleIds.Insert(posKey);
            }
        }

        int sk;
        for (sk = 0; sk < staleIds.Count(); sk = sk + 1)
        {
            m_LastKnownPos.Remove(staleIds[sk]);
        }

        if (staleIds.Count() > 0)
        {
            LFPG_Util.Debug("[SelfHeal] Pruned " + staleIds.Count().ToString() + " stale position tracking entries");
        }
        #endif
    }

    // ===========================
    // Vanilla wire persistence (v0.7.4: deferred)
    // ===========================

    // Mark vanilla wire store as needing a save.
    // Actual disk I/O deferred to the next periodic flush.
    void MarkVanillaDirty()
    {
        #ifdef SERVER
        m_VanillaDirty = true;
        #endif
    }

    // Periodic callback: flush to disk only if dirty.
    // Called every LFPG_VANILLA_FLUSH_S seconds (default 30s).
    protected void FlushVanillaIfDirty()
    {
        #ifdef SERVER
        if (!m_VanillaDirty)
            return;

        SaveVanillaWires();
        m_VanillaDirty = false;
        #endif
    }

    // Explicit flush for shutdown/mission finish.
    // Call from MissionServer cleanup to ensure no data loss.
    void FlushVanillaOnShutdown()
    {
        #ifdef SERVER
        if (m_VanillaDirty)
        {
            LFPG_Util.Info("[VanillaWires] Flushing on shutdown...");
            SaveVanillaWires();
            m_VanillaDirty = false;
        }
        #endif
    }

    protected void SaveVanillaWires()
    {
        #ifdef SERVER
        // v0.7.16 H6: Don't overwrite if file was from a newer schema version.
        // Saving would strip unknown fields and downgrade the version marker.
        if (m_VanillaReadOnly)
        {
            LFPG_Util.Warn("[VanillaWires] SAVE BLOCKED: loaded from schema v" + m_VanillaLoadedVer.ToString() + " > current v" + LFPG_VANILLA_PERSIST_VER.ToString() + ". Upgrade the mod to save changes.");
            return;
        }

        if (!FileExist(VANILLA_WIRES_DIR))
            MakeDirectory(VANILLA_WIRES_DIR);

        // Build a flat array of owner+wire pairs for serialization
        LFPG_VanillaWireStore store = new LFPG_VanillaWireStore();

        int vk;
        for (vk = 0; vk < m_VanillaWires.Count(); vk = vk + 1)
        {
            string ownerId = m_VanillaWires.GetKey(vk);
            ref array<ref LFPG_WireData> wires = m_VanillaWires.GetElement(vk);
            if (!wires) continue;

            int w;
            for (w = 0; w < wires.Count(); w = w + 1)
            {
                LFPG_WireData wd = wires[w];
                if (!wd) continue;

                LFPG_VanillaWireEntry entry = new LFPG_VanillaWireEntry();
                entry.ownerDeviceId = ownerId;
                entry.targetDeviceId = wd.m_TargetDeviceId;
                entry.targetPort = wd.m_TargetPort;
                entry.sourcePort = wd.m_SourcePort;
                entry.creatorId = wd.m_CreatorId;

                // Persist waypoints (v0.7.3)
                if (wd.m_Waypoints && wd.m_Waypoints.Count() > 0)
                {
                    int wp;
                    for (wp = 0; wp < wd.m_Waypoints.Count(); wp = wp + 1)
                    {
                        entry.waypoints.Insert(wd.m_Waypoints[wp]);
                    }
                }

                store.entries.Insert(entry);
            }
        }

        // v0.7.15 (Sprint 3 P2b): Atomic save with backup rotation
        if (LFPG_FileUtil.AtomicSaveVanillaWires(VANILLA_WIRES_FILE, store))
        {
            LFPG_Util.Info("[VanillaWires] Saved " + store.entries.Count().ToString() + " entries (atomic)");
        }
        else
        {
            LFPG_Util.Error("[VanillaWires] Atomic save failed!");
        }
        #endif
    }

    protected void LoadVanillaWires()
    {
        #ifdef SERVER
        // v0.7.15 (Sprint 3 P2b): Attempt backup restore if main file missing
        if (!LFPG_FileUtil.EnsureFileOrRestore(VANILLA_WIRES_FILE))
        {
            LFPG_Util.Info("[VanillaWires] No saved file found, starting fresh.");
            return;
        }

        LFPG_VanillaWireStore store = new LFPG_VanillaWireStore();
        string err;
        if (!JsonFileLoader<LFPG_VanillaWireStore>.LoadFile(VANILLA_WIRES_FILE, store, err))
        {
            LFPG_Util.Warn("[VanillaWires] Load failed: " + err);
            return;
        }

        if (!store.entries)
        {
            LFPG_Util.Info("[VanillaWires] Loaded empty store.");
            return;
        }

        // v0.7.16 H6: Track loaded version for save guard
        m_VanillaLoadedVer = store.ver;

        // v0.7.15 (Sprint 3 P1): Apply migration chain
        int oldVer = store.ver;
        int finalVer = LFPG_Migrators.MigrateVanillaStore(store);
        if (finalVer != oldVer)
        {
            LFPG_Util.Info("[VanillaWires] Migrated schema v" + oldVer.ToString() + " → v" + finalVer.ToString());
        }

        // v0.7.16 H6: If loaded from a newer schema, enter read-only mode
        if (m_VanillaLoadedVer > LFPG_VANILLA_PERSIST_VER)
        {
            m_VanillaReadOnly = true;
            LFPG_Util.Warn("[VanillaWires] Schema v" + m_VanillaLoadedVer.ToString() + " > current v" + LFPG_VANILLA_PERSIST_VER.ToString() + ". Entering READ-ONLY mode to protect data. Upgrade the mod.");
        }

        int loaded = 0;
        int discarded = 0;
        int duplicates = 0;

        // v0.7.16 H3: Map-based O(N) dedup per owner instead of O(N²) IsDuplicate
        ref map<string, ref map<string, bool>> dedupByOwner = new map<string, ref map<string, bool>>;

        int i;
        for (i = 0; i < store.entries.Count(); i = i + 1)
        {
            LFPG_VanillaWireEntry entry = store.entries[i];
            if (!entry) continue;
            if (entry.ownerDeviceId == "" || entry.targetDeviceId == "")
            {
                discarded = discarded + 1;
                continue;
            }

            LFPG_WireData wd = new LFPG_WireData();
            wd.m_TargetDeviceId = entry.targetDeviceId;
            wd.m_TargetPort = entry.targetPort;
            wd.m_SourcePort = entry.sourcePort;
            wd.m_CreatorId = entry.creatorId;

            // Restore waypoints (v0.7.3)
            if (entry.waypoints && entry.waypoints.Count() > 0)
            {
                int wp;
                for (wp = 0; wp < entry.waypoints.Count(); wp = wp + 1)
                {
                    wd.m_Waypoints.Insert(entry.waypoints[wp]);
                }
            }

            // v0.7.15 (Sprint 3 P2): Exhaustive per-wire validation
            if (!LFPG_WireHelper.ValidateWireData(wd, "VanillaWires"))
            {
                discarded = discarded + 1;
                continue;
            }

            // v0.7.16 H3: O(1) dedup via map per owner
            ref map<string, bool> ownerDedup;
            if (!dedupByOwner.Find(entry.ownerDeviceId, ownerDedup) || !ownerDedup)
            {
                ownerDedup = new map<string, bool>;
                dedupByOwner.Set(entry.ownerDeviceId, ownerDedup);
            }

            string dedupKey = wd.m_TargetDeviceId + "|" + wd.m_TargetPort + "|" + wd.m_SourcePort;
            bool isDup = false;
            ownerDedup.Find(dedupKey, isDup);
            if (isDup)
            {
                duplicates = duplicates + 1;
                continue;
            }
            ownerDedup.Set(dedupKey, true);

            // Insert into wire map
            ref array<ref LFPG_WireData> wires;
            if (!m_VanillaWires.Find(entry.ownerDeviceId, wires) || !wires)
            {
                wires = new array<ref LFPG_WireData>;
                m_VanillaWires[entry.ownerDeviceId] = wires;
            }

            wires.Insert(wd);
            loaded = loaded + 1;
        }

        string loadMsg = "[VanillaWires] Loaded " + loaded.ToString() + " entries from " + store.entries.Count().ToString();
        if (discarded > 0)
        {
            loadMsg = loadMsg + " (discarded " + discarded.ToString() + " corrupt)";
        }
        if (duplicates > 0)
        {
            loadMsg = loadMsg + " (removed " + duplicates.ToString() + " duplicates)";
        }
        LFPG_Util.Info(loadMsg);
        #endif
    }
	
	void CutAllWiresFromDevice(EntityAI device)
    {
        #ifdef SERVER
        if (!device)
            return;

        string deviceId = LFPG_DeviceAPI.GetDeviceId(device);
        if (deviceId == "")
            return;

        bool anyChanged = false;

        // --- v0.7.28 (Bug 2+3): Collect all graph neighbors BEFORE cutting ---
        // When wires are cut, the graph node for this device gets removed.
        // If we don't force SetPowered(false) on neighbors first, they
        // become orphan nodes that never receive a powered=false update.
        // Collect now while the graph still has the edges.
        ref array<string> neighborIds = new array<string>;
        if (m_Graph)
        {
            ref array<ref LFPG_ElecEdge> preOutEdges = m_Graph.GetOutgoing(deviceId);
            if (preOutEdges)
            {
                int poi;
                for (poi = 0; poi < preOutEdges.Count(); poi = poi + 1)
                {
                    ref LFPG_ElecEdge poEdge = preOutEdges[poi];
                    if (poEdge && poEdge.m_TargetNodeId != "")
                    {
                        neighborIds.Insert(poEdge.m_TargetNodeId);
                    }
                }
            }
            ref array<ref LFPG_ElecEdge> preInEdges = m_Graph.GetIncoming(deviceId);
            if (preInEdges)
            {
                int pii;
                for (pii = 0; pii < preInEdges.Count(); pii = pii + 1)
                {
                    ref LFPG_ElecEdge piEdge = preInEdges[pii];
                    if (piEdge && piEdge.m_SourceNodeId != "")
                    {
                        neighborIds.Insert(piEdge.m_SourceNodeId);
                    }
                }
            }
        }

        // --- 1. Clear OWNED wires (output side) ---
        if (LFPG_DeviceAPI.HasWireStore(device))
        {
            ref array<ref LFPG_WireData> ownedWires = LFPG_DeviceAPI.GetDeviceWires(device);
            if (ownedWires && ownedWires.Count() > 0)
            {
                // Update reverse index and player counts before clearing
                int ow = ownedWires.Count() - 1;
                while (ow >= 0)
                {
                    LFPG_WireData wd = ownedWires[ow];
                    if (wd)
                    {
                        ReverseIdxRemove(wd.m_TargetDeviceId, wd.m_TargetPort, deviceId);
                        PlayerWireCountAdd(wd.m_CreatorId, -1);

                        // Notify graph of each wire removal
                        if (m_Graph)
                        {
                            m_Graph.OnWireRemoved(deviceId, wd.m_TargetDeviceId, wd.m_SourcePort, wd.m_TargetPort);
                        }
                    }
                    ow = ow - 1;
                }

                LFPG_DeviceAPI.ClearDeviceWires(device);
                BroadcastOwnerWires(device);
                anyChanged = true;
            }
        }

        // --- 2. Clear vanilla store wires (if vanilla source) ---
        if (deviceId.IndexOf("vp:") == 0)
        {
            ref array<ref LFPG_WireData> vWires;
            if (m_VanillaWires.Find(deviceId, vWires) && vWires && vWires.Count() > 0)
            {
                int vw = vWires.Count() - 1;
                while (vw >= 0)
                {
                    LFPG_WireData vwd = vWires[vw];
                    if (vwd)
                    {
                        ReverseIdxRemove(vwd.m_TargetDeviceId, vwd.m_TargetPort, deviceId);
                        PlayerWireCountAdd(vwd.m_CreatorId, -1);

                        if (m_Graph)
                        {
                            m_Graph.OnWireRemoved(deviceId, vwd.m_TargetDeviceId, vwd.m_SourcePort, vwd.m_TargetPort);
                        }
                    }
                    vw = vw - 1;
                }
                vWires.Clear();
                MarkVanillaDirty();
                BroadcastVanillaWires(deviceId, device);
                anyChanged = true;
            }
        }

        // --- 3. Remove wires TARGETING this device's IN ports ---
        int portCount = LFPG_DeviceAPI.GetPortCount(device);
        int pi;
        for (pi = 0; pi < portCount; pi = pi + 1)
        {
            int portDir = LFPG_DeviceAPI.GetPortDir(device, pi);
            if (portDir == LFPG_PortDir.IN)
            {
                string portName = LFPG_DeviceAPI.GetPortName(device, pi);
                int removed = RemoveWiresTargeting(deviceId, portName);
                if (removed > 0)
                {
                    anyChanged = true;
                    LFPG_Util.Info("[CutAll] Removed " + removed.ToString() + " incoming wire(s) on " + deviceId + ":" + portName);
                }
            }
        }

        // --- 4. Brute-force fallback for stale reverse index ---
        // Same pattern as v0.7.25 Bug 3 fix in PlayerRPC CUT_WIRES.
        // Scans all devices for wires targeting us that the index missed.
        {
            array<EntityAI> allDevs = new array<EntityAI>;
            LFPG_DeviceRegistry.Get().GetAll(allDevs);
            int di;
            for (di = 0; di < allDevs.Count(); di = di + 1)
            {
                EntityAI srcDev = allDevs[di];
                if (!srcDev)
                    continue;
                if (srcDev == device)
                    continue;
                if (!LFPG_DeviceAPI.HasWireStore(srcDev))
                    continue;

                string srcId = LFPG_DeviceAPI.GetDeviceId(srcDev);
                ref array<ref LFPG_WireData> srcWires = LFPG_DeviceAPI.GetDeviceWires(srcDev);
                if (!srcWires)
                    continue;

                bool srcChanged = false;
                int sw = srcWires.Count() - 1;
                while (sw >= 0)
                {
                    LFPG_WireData swd = srcWires[sw];
                    if (swd && swd.m_TargetDeviceId == deviceId)
                    {
                        LFPG_Util.Warn("[CutAll-Fallback] Found stale wire: " + srcId + " -> " + deviceId);
                        PlayerWireCountAdd(swd.m_CreatorId, -1);

                        if (m_Graph)
                        {
                            m_Graph.OnWireRemoved(srcId, deviceId, swd.m_SourcePort, swd.m_TargetPort);
                        }

                        srcWires.Remove(sw);
                        srcChanged = true;
                        anyChanged = true;
                    }
                    sw = sw - 1;
                }

                if (srcChanged)
                {
                    srcDev.SetSynchDirty();
                    BroadcastOwnerWires(srcDev);
                }
            }
        }

        // --- 5. Cleanup tracking state ---
        m_LastKnownPos.Remove(deviceId);

        // --- 5b. Force powered=false on device and all neighbors (v0.7.28) ---
        // Must happen BEFORE graph removal (section 6). Once the graph
        // removes the node, orphaned neighbors never get a propagation
        // update and their m_PoweredNet stays stale.
        if (anyChanged)
        {
            // Force self off (covers consumers and passthroughs)
            LFPG_DeviceAPI.SetPowered(device, false);

            // Force all neighbors off — propagation will re-enable
            // any that still have an alternate power path.
            int nbi;
            for (nbi = 0; nbi < neighborIds.Count(); nbi = nbi + 1)
            {
                EntityAI neighborDev = LFPG_DeviceRegistry.Get().FindById(neighborIds[nbi]);
                if (neighborDev)
                {
                    LFPG_DeviceAPI.SetPowered(neighborDev, false);
                }
            }
        }

        // --- 6. Notify graph and propagate ---
        if (anyChanged)
        {
            if (m_Graph)
            {
                m_Graph.OnDeviceRemoved(deviceId);
            }
            PostBulkRebuildAndPropagate();
            LFPG_Util.Info("[CutAll] All wires removed for device " + deviceId + " type=" + device.GetType());

            // v0.7.28: Safety flush — ensure all queued RPCs reach clients
            // immediately. Sections 1-4 use direct BroadcastOwnerWires and
            // RemoveWiresTargeting (which flushes internally), but this
            // covers any edge case where a broadcast was queued but not sent.
            FlushBroadcasts();
        }

        // --- 7. Untrack from centralized polling (v0.7.30) ---
        // CutAll removes all owned + incoming wires, so device no longer
        // needs position monitoring. UntrackDeviceFromPolling is a no-op
        // if the device wasn't tracked.
        // Also untrack neighbors that may have lost all their wires
        // as a result of this cut (e.g. consumer whose only source was cut).
        UntrackDeviceFromPolling(deviceId);

        int nui;
        for (nui = 0; nui < neighborIds.Count(); nui = nui + 1)
        {
            string neighborId = neighborIds[nui];
            EntityAI neighborDev = LFPG_DeviceRegistry.Get().FindById(neighborId);
            if (neighborDev)
            {
                if (!DeviceHasAnyWires(neighborDev, neighborId))
                {
                    UntrackDeviceFromPolling(neighborId);
                }
            }
            else
            {
                // Neighbor disappeared — clean up
                UntrackDeviceFromPolling(neighborId);
            }
        }
        #endif
    }
};