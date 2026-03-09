// =========================================================
// LF_PowerGrid - NetworkManager (v0.7.44, Sprint 4.3+Bloque E+RC fixes)
//
// Server singleton: validation, wire storage, propagation.
//
// v0.7.44 (Level 3): Re-populate target NetworkIDs on wire data during
//   self-heal. Ensures BroadcastOwnerWires sends valid session-specific
//   NetworkIDs to clients for CableRenderer fallback resolution.
//
// v0.7.38 (Race Condition fixes):
//   RC-01: Port locking for concurrent FinishWiring (IsPortLocked/LockPort/UnlockPort)
//   RC-02: Immediate ProcessDirtyQueue flush after PostBulkRebuild (zero-flicker)
//   RC-03: CheckDeviceMovement safety invariant (documented, no code change)
//   RC-05: FullSync mutex — defers BroadcastOwnerWires/VanillaWires during SendFullSyncTo
//   RC-07: m_StartupValidationDone flag — RPCs rejected until initial validation completes
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
// v0.7.34 (Bloque E): Atomic graph mutations
//   - New wrappers: BeginGraphMutation, EndGraphMutation,
//     NotifyGraphWireRemoved (encapsulate graph access for callers)
//   - RemoveWiresTargeting now notifies graph via OnWireRemoved
//     (was missing → stale edges after port replacement)
//   - CutAllWiresFromDevice: NO incremental graph updates needed
//     (OnDeviceRemoved + PostBulkRebuild handles full cleanup)
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

    // v0.7.38 (RC-01): Port locking for concurrent FinishWiring protection.
    // Key: "targetDeviceId|targetPort" → true while a FinishWiring is in-flight
    // for that destination port. Prevents two RPCs from both passing the
    // occupancy check on the same port in the same tick.
    protected ref map<string, bool> m_PortLocks;

    // v0.7.38 (RC-07): Startup validation flag.
    // False until ValidateAllWiresAndPropagate completes (5s after init).
    // RPC handlers reject wiring requests during this window to prevent
    // transient flicker from rebuild overwriting incremental state.
    protected bool m_StartupValidationDone = false;

    // v0.7.38 (RC-05): FullSync mutex.
    // True while SendFullSyncTo is iterating devices. BroadcastOwnerWires
    // and BroadcastVanillaWires enqueue instead of sending immediately,
    // preventing reorder between FullSync RPCs and mutation Broadcasts.
    protected bool m_FullSyncInProgress = false;
    protected ref array<EntityAI> m_DeferredBroadcastLFPG;
    protected ref array<string>   m_DeferredBroadcastVanillaIds;
    protected ref array<EntityAI> m_DeferredBroadcastVanillaObjs;

    // v0.7.4: deferred vanilla wire persistence.
    // MarkVanillaDirty() sets flag; FlushVanillaIfDirty() writes to disk.
    // Periodic timer (LFPG_VANILLA_FLUSH_S) flushes automatically.
    // Eliminates synchronous I/O on every wire mutation.
    protected bool m_VanillaDirty = false;

    // v0.7.16 H6: Version guard — track loaded schema version.
    // If file was saved by a newer mod version, block saves to prevent data loss.
    protected int m_VanillaLoadedVer = 0;
    protected bool m_VanillaReadOnly = false;

    // v0.8.0: Centralized solar timer cached state.
    // Single timer reads GetDate() once per tick, updates all panels atomically.
    // Eliminates N per-panel CallLater timers and prevents race conditions.
    protected bool m_SolarHasSun = false;

    // v1.1.0: Water Pump tank fill tracking (in-game hour based)
    protected float m_TankFillLastMs = -1.0;

    // v1.2.0 (Sprint S3): Sorter round-robin cursor
    protected int m_SorterCursor = 0;

    // v1.2.0 (Sprint S5): Dedicated sorter registry — avoids iterating all devices
    protected ref array<LF_Sorter> m_RegisteredSorters;

    // v1.2.0 (Sprint S5): Reusable item cache for TickSorters (GC reduction)
    protected ref array<EntityAI> m_SorterItemCache;

    // Cached valid device IDs for PruneMissingTargets (built once per self-heal cycle)
    protected ref map<string, bool> m_CachedValidIds;

    // Vanilla wire persistence path
    protected static const string VANILLA_WIRES_DIR  = "$profile:LF_PowerGrid";
    protected static const string VANILLA_WIRES_FILE = "$profile:LF_PowerGrid\\vanilla_wires.json";

    // Rate limiter stale threshold: entries idle for > 10 minutes are purged
    protected static const float RATE_LIMITER_STALE_SEC = 600.0;

    // S7-4: Sliding window rate limiter — max ops per player per 1-second window.
    // Prevents RPC spam from malicious or bugged clients even if cooldown is small.
    protected static const int LFPG_RPC_MAX_OPS_PER_SEC = 5;

    // S7-4: Per-player sliding window state (keyed by plain player ID).
    // m_RateWindowStart: timestamp when the current 1s window began.
    // m_RateOpsInWindow: number of ops in the current window.
    protected ref map<string, float> m_RateWindowStart;
    protected ref map<string, int>   m_RateOpsInWindow;

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
        m_RateWindowStart = new map<string, float>;
        m_RateOpsInWindow = new map<string, int>;
        m_VanillaWires = new map<string, ref array<ref LFPG_WireData>>;
        m_ReverseIdx = new map<string, int>;
        m_ReverseOwners = new map<string, ref array<string>>;
        m_WiresByPlayer = new map<string, int>;
        m_PendingBroadcastLFPG = new map<string, EntityAI>;
        m_PendingBroadcastVanilla = new map<string, EntityAI>;
        m_LastKnownPos = new map<string, vector>;
        m_PortLocks = new map<string, bool>;
        m_DeferredBroadcastLFPG = new array<EntityAI>;
        m_DeferredBroadcastVanillaIds = new array<string>;
        m_DeferredBroadcastVanillaObjs = new array<EntityAI>;

        // v1.2.0: Always allocate (Register/Unregister not guarded with #ifdef)
        m_RegisteredSorters = new array<LF_Sorter>;
        m_SorterItemCache = new array<EntityAI>;

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
        string initMsg = "NetworkManager init (server).";
        LFPG_Util.Info(initMsg);
        LoadVanillaWires();
        bool bFalse = false;
        bool bTrue = true;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ValidateAllWiresAndPropagate, 5000, bFalse);
        // Periodic rate limiter cleanup (every 5 minutes)
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PurgeStaleRateLimiters, 300000, bTrue);
        // v0.7.4: periodic vanilla wire flush (deferred persistence)
        int flushMs = (int)(LFPG_VANILLA_FLUSH_S * 1000.0);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(FlushVanillaIfDirty, flushMs, bTrue);
        // Sprint 4.2: periodic propagation tick (event-driven via graph dirty queue)
        int propTickMs = (int)LFPG_PROPAGATE_TICK_MS;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(TickPropagation, propTickMs, bTrue);
        // v0.7.30 (Audit 1+2): Centralized position polling with round-robin batching.
        // Replaces per-device timers. Processes LFPG_MOVE_DETECT_BATCH_SIZE devices per tick.
        // Runtime guard: prevents timer registration in SP/local-host hybrid contexts
        // where #ifdef SERVER is active but the instance isn't a true dedicated server.
        if (GetGame().IsServer())
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CheckDeviceMovement, LFPG_MOVE_DETECT_TICK_MS, bTrue);
        }

        // v0.8.0: Centralized solar timer — 1 timer for all solar panels.
        // Seed cached sun state immediately (panels may init before first tick).
        LFPG_ComputeSunState();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_TickSolarPanels, LFPG_SOLAR_CHECK_MS, bTrue);

        // v1.1.0: Water Pump tablet + tank timer
        LFPG_InitTankFillTime();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_TickWaterPumps, LFPG_PUMP_CHECK_MS, bTrue);

        // v1.2.0 (Sprint S3): Sorter tick — round-robin batch sorting
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_TickSorters, LFPG_SORTER_TICK_MS, bTrue);
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
    // S7-4: Two-layer rate limiting:
    //   Layer 1 (sliding window): max LFPG_RPC_MAX_OPS_PER_SEC ops/s per player.
    //     Blocks burst spam regardless of individual op cooldown.
    //   Layer 2 (per-op cooldown): existing LFPG_RateLimiter with RpcCooldownSeconds.
    //     Enforces minimum gap between consecutive ops.
    bool AllowPlayerAction(PlayerIdentity ident)
    {
        if (!ident) return false;

        string pid = ident.GetPlainId();
        LFPG_ServerSettings st = LFPG_Settings.Get();
        float now = GetGame().GetTime() * 0.001;

        // --- Layer 1: sliding window ---
        float windowStart = 0.0;
        int opsInWindow = 0;

        if (m_RateWindowStart.Find(pid, windowStart))
        {
            float elapsed = now - windowStart;
            if (elapsed >= 1.0)
            {
                // Window expired — start a new one
                m_RateWindowStart[pid] = now;
                m_RateOpsInWindow[pid] = 0;
                opsInWindow = 0;
            }
            else
            {
                if (!m_RateOpsInWindow.Find(pid, opsInWindow))
                    opsInWindow = 0;

                if (opsInWindow >= LFPG_RPC_MAX_OPS_PER_SEC)
                {
                    string swLog = "[RateLimiter] Sliding window exceeded for " + pid;
                    swLog = swLog + " ops=" + opsInWindow.ToString();
                    LFPG_Util.Warn(swLog);
                    return false;
                }
            }
        }
        else
        {
            m_RateWindowStart[pid] = now;
            m_RateOpsInWindow[pid] = 0;
            opsInWindow = 0;
        }

        // Count this op in the window BEFORE Layer 2 check.
        // Intentional: we count the attempt, not the success. A spammer hitting
        // the cooldown on every op is still attempting spam — the window fills
        // up and they get hard-blocked. Legitimate players (1-2 ops/s) never
        // approach LFPG_RPC_MAX_OPS_PER_SEC=5.
        m_RateOpsInWindow[pid] = opsInWindow + 1;

        // --- Layer 2: per-op cooldown (existing) ---
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
    // S7-3: Also calls PruneNullEntries on DeviceRegistry — covers sessions
    //        with heavy destruction that don't trigger a full self-heal.
    // S7-4: Also purges sliding window maps for the same stale players.
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
        string staleKey;
        for (k = 0; k < removed; k = k + 1)
        {
            staleKey = staleKeys[k];
            m_RateByPlayer.Remove(staleKey);
            // S7-4: Keep sliding window maps in sync with cooldown map
            m_RateWindowStart.Remove(staleKey);
            m_RateOpsInWindow.Remove(staleKey);
        }

        if (removed > 0)
        {
            string purgeMsg = "[RateLimiter] Purged " + removed.ToString() + " stale entries";
            LFPG_Util.Info(purgeMsg);
        }

        // S7-3: Periodic null-entry prune on DeviceRegistry.
        // Self-heal already calls this, but long sessions with many device
        // destructions that don't trigger self-heal can accumulate stale refs.
        int pruned = LFPG_DeviceRegistry.Get().PruneNullEntries();
        if (pruned > 0)
        {
            string pruneMsg = "[PurgeStale] DeviceRegistry pruned " + pruned.ToString() + " null entries";
            LFPG_Util.Info(pruneMsg);
        }
        #endif
    }

    // ===========================
    // Vanilla wire storage
    // ===========================

    // v0.7.38 (RC-01): Port locking for concurrent FinishWiring.
    // Prevents two RPCs from simultaneously modifying the same destination port.
    // Lock key format: "targetDeviceId|targetPort".
    bool IsPortLocked(string lockKey)
    {
        bool locked = false;
        if (m_PortLocks.Find(lockKey, locked))
        {
            return locked;
        }
        return false;
    }

    void LockPort(string lockKey)
    {
        m_PortLocks.Set(lockKey, true);
    }

    void UnlockPort(string lockKey)
    {
        m_PortLocks.Remove(lockKey);
    }

    // v0.7.38 (RC-07): Startup validation check.
    // Returns false during the first ~5 seconds while the server
    // runs ValidateAllWiresAndPropagate. RPC handlers should reject
    // wiring requests during this window.
    bool IsStartupValidationDone()
    {
        return m_StartupValidationDone;
    }
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

    // v0.7.36 (Audit Feb2026): Pre-check component size before wire storage.
    // Returns true if the merged component would exceed the node limit.
    // Called from FinishWiring before any mutations.
    bool CheckComponentSizeBeforeWire(string sourceId, string targetId)
    {
        #ifdef SERVER
        if (!m_Graph)
            return false;
        return m_Graph.WouldExceedComponentLimit(sourceId, targetId);
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

    // v0.7.34 (Bloque E): Notify the graph that a wire was removed.
    // Called BEFORE or AFTER the wire is removed from the data store.
    // Removes the directed edge from the graph and marks endpoints dirty.
    void NotifyGraphWireRemoved(string sourceId, string targetId, string sourcePort, string targetPort)
    {
        #ifdef SERVER
        if (!m_Graph)
            return;
        m_Graph.OnWireRemoved(sourceId, targetId, sourcePort, targetPort);
        #endif
    }

    // v0.7.34 (Bloque E): Begin an atomic graph mutation batch.
    // While active, orphan node cleanup is deferred to EndGraphMutation.
    // Use when multiple wires are removed+added in a single operation
    // (e.g. replace wire = remove old + add new on same target).
    void BeginGraphMutation()
    {
        #ifdef SERVER
        if (!m_Graph)
            return;
        m_Graph.BeginGraphMutation();
        #endif
    }

    // v0.7.34 (Bloque E): End an atomic graph mutation batch.
    // Flushes deferred orphan cleanup. Nesting-safe: only the
    // outermost End triggers the flush.
    void EndGraphMutation()
    {
        #ifdef SERVER
        if (!m_Graph)
            return;
        m_Graph.EndGraphMutation();
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

        // v0.7.38 (RC-02): Immediate flush after rebuild.
        // If TickPropagation ran earlier in this frame with budget exhaustion,
        // it may have sync'd transient states to entities. The rebuild just
        // reconstructed the graph with correct states. Flush immediately
        // so correct SyncNodeToEntity calls land in the SAME frame.
        // DayZ SyncVar batching sends only the final value to clients → no flicker.
        int flushBudget = LFPG_PROPAGATE_WARMUP_BUDGET;
        int flushEdge = LFPG_PROPAGATE_EDGE_WARMUP_BUDGET;
        m_Graph.ProcessDirtyQueue(flushBudget, flushEdge);
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

    // Returns true if the specified IN port has at least one wire
    // from a source device that is currently providing power.
    // Uses m_ReverseOwners for O(owners) lookup (same index as CountWiresTargeting).
    // Works for SOURCE (generator on + sparkplug) and PASSTHROUGH (splitter powered).
    // Used by Zen_RaidAlarmRadar for per-port trigger detection.
    bool IsPortTargetedByPoweredSource(string targetDeviceId, string targetPort)
    {
        #ifdef SERVER
        if (targetDeviceId == "" || targetPort == "")
            return false;

        string rKey = targetDeviceId + "|" + targetPort;

        // Quick check: any wires at all? (O(1) via reverse index)
        int count = 0;
        m_ReverseIdx.Find(rKey, count);
        if (count <= 0)
            return false;

        // Get owner device IDs that have wires targeting this port
        ref array<string> owners;
        if (!m_ReverseOwners.Find(rKey, owners))
            return false;

        if (!owners)
            return false;

        int i;
        for (i = 0; i < owners.Count(); i = i + 1)
        {
            string ownerId = owners[i];
            if (ownerId == "")
                continue;

            EntityAI srcEntity = LFPG_DeviceRegistry.Get().FindById(ownerId);
            if (!srcEntity)
            {
                // Try vanilla resolution as fallback
                srcEntity = LFPG_DeviceAPI.ResolveVanillaDevice(ownerId);
            }

            if (!srcEntity)
                continue;

            // GetSourceOn works for both device types:
            //   SOURCE:      switch on + sparkplug valid
            //   PASSTHROUGH: m_PoweredNet (upstream provides power)
            bool srcOn = LFPG_DeviceAPI.GetSourceOn(srcEntity);
            if (srcOn)
                return true;
        }
        #endif

        return false;
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
                            // v0.7.34 (Bloque E): Notify graph before removing wire data.
                            // Without this, replaced edges stay stale in the graph.
                            // LFPG wires: use m_SourcePort as-is (no normalization).
                            // This matches RebuildFromWires which also does NOT normalize
                            // LFPG wire source ports.
                            if (m_Graph)
                            {
                                m_Graph.OnWireRemoved(ownerId, targetDeviceId, wd.m_SourcePort, targetPort);
                            }

                            PlayerWireCountAdd(wd.m_CreatorId, -1);
                            gWires.Remove(gw);
                            removed = removed + 1;
                            ownerChanged = true;
                            string prMsg = "[PortReplace] Removed wire from " + ownerId + " -> " + targetDeviceId + ":" + normPort;
                            LFPG_Util.Info(prMsg);
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
                        // v0.7.34 (Bloque E): Notify graph before removing wire data.
                        // Vanilla wires: normalize empty sourcePort to "output_1".
                        // This matches RebuildFromWires which normalizes vanilla ports.
                        if (m_Graph)
                        {
                            string vSrcP = vwd.m_SourcePort;
                            if (vSrcP == "")
                            {
                                vSrcP = "output_1";
                            }
                            m_Graph.OnWireRemoved(ownerId, targetDeviceId, vSrcP, targetPort);
                        }

                        PlayerWireCountAdd(vwd.m_CreatorId, -1);
                        vWires.Remove(vw);
                        removed = removed + 1;
                        vChanged = true;
                        string prVMsg = "[PortReplace] Removed vanilla wire from " + ownerId + " -> " + targetDeviceId + ":" + normPort;
                        LFPG_Util.Info(prVMsg);
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

        // v0.7.38 (RC-05): Defer broadcast if FullSync is in progress.
        // Prevents reordering where a Broadcast arrives at client BEFORE
        // the FullSync RPC for the same owner, leaving stale state.
        if (m_FullSyncInProgress)
        {
            m_DeferredBroadcastLFPG.Insert(owner);
            return;
        }

        string json = LFPG_DeviceAPI.GetWiresJSON(owner);
        string ownerId = LFPG_DeviceAPI.GetDeviceId(owner);

        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);

        int low = 0;
        int high = 0;
        owner.GetNetworkID(low, high);

        string bcastMsg = "[BroadcastOwnerWires] owner=" + ownerId + " net=" + low.ToString() + ":" + high.ToString() + " type=" + owner.GetType() + " jsonLen=" + json.Length().ToString();
        LFPG_Util.Info(bcastMsg);

        // v0.7.35 D8: Warn if blob approaching practical RPC size limit
        if (json.Length() > 12000)
        {
            string bcastWarn = "[BroadcastOwnerWires] LARGE BLOB owner=" + ownerId + " jsonLen=" + json.Length().ToString() + " — approaching RPC limit";
            LFPG_Util.Warn(bcastWarn);
        }

        // v0.7.11 (A3): Precompute squared threshold for player distance culling.
        float syncMaxDist = LFPG_CULL_DISTANCE_M + 20.0;
        float syncMaxDistSq = syncMaxDist * syncMaxDist;
        vector ownerPos = owner.GetPosition();

        // v0.7.35 B-CRIT2: Collect unique target device positions.
        // Players near ANY target also need the owner's wire blob.
        array<vector> targetPositions = new array<vector>;
        ref array<ref LFPG_WireData> ownerWires = LFPG_DeviceAPI.GetDeviceWires(owner);
        if (ownerWires)
        {
            LFPG_DeviceRegistry reg = LFPG_DeviceRegistry.Get();
            int tw;
            for (tw = 0; tw < ownerWires.Count(); tw = tw + 1)
            {
                if (!ownerWires[tw]) continue;
                if (ownerWires[tw].m_TargetDeviceId == "") continue;

                EntityAI targetObj = reg.FindById(ownerWires[tw].m_TargetDeviceId);
                if (targetObj)
                {
                    targetPositions.Insert(targetObj.GetPosition());
                }
            }
        }

        int i;
        for (i = 0; i < players.Count(); i = i + 1)
        {
            PlayerBase pb = PlayerBase.Cast(players[i]);
            if (!pb) continue;

            vector playerPos = pb.GetPosition();

            // v0.7.11 (A3): Compare in squared domain — eliminates sqrt per player.
            bool inRange = (LFPG_WorldUtil.DistSq(playerPos, ownerPos) <= syncMaxDistSq);

            // v0.7.35 B-CRIT2: Also check distance to each target device
            if (!inRange)
            {
                int tp;
                for (tp = 0; tp < targetPositions.Count(); tp = tp + 1)
                {
                    if (LFPG_WorldUtil.DistSq(playerPos, targetPositions[tp]) <= syncMaxDistSq)
                    {
                        inRange = true;
                        break;
                    }
                }
            }

            if (!inRange) continue;

            ScriptRPC rpc = new ScriptRPC();
            rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES);
            rpc.Write(ownerId);
            rpc.Write(low);
            rpc.Write(high);
            rpc.Write(json);
            bool bRpcGuaranteed = true;
            PlayerIdentity noExclude = null;
            rpc.Send(pb, LFPG_RPC_CHANNEL, bRpcGuaranteed, noExclude);
        }
    }

    // ===========================
    // Sync: vanilla source -> clients
    // ===========================
    void BroadcastVanillaWires(string ownerDeviceId, EntityAI ownerObj)
    {
        if (ownerDeviceId == "" || !ownerObj) return;

        // v0.7.38 (RC-05): Defer broadcast if FullSync is in progress.
        if (m_FullSyncInProgress)
        {
            m_DeferredBroadcastVanillaIds.Insert(ownerDeviceId);
            m_DeferredBroadcastVanillaObjs.Insert(ownerObj);
            return;
        }

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

        // v0.7.35 B-CRIT2: Collect target positions from vanilla wires
        array<vector> vTargetPositions = new array<vector>;
        if (wires)
        {
            LFPG_DeviceRegistry reg = LFPG_DeviceRegistry.Get();
            int tw;
            for (tw = 0; tw < wires.Count(); tw = tw + 1)
            {
                if (!wires[tw]) continue;
                if (wires[tw].m_TargetDeviceId == "") continue;

                EntityAI targetObj = reg.FindById(wires[tw].m_TargetDeviceId);
                if (targetObj)
                {
                    vTargetPositions.Insert(targetObj.GetPosition());
                }
            }
        }

        int i;
        for (i = 0; i < players.Count(); i = i + 1)
        {
            PlayerBase pb = PlayerBase.Cast(players[i]);
            if (!pb) continue;

            vector playerPos = pb.GetPosition();

            // v0.7.11 (A3): Compare in squared domain — eliminates sqrt per player.
            bool inRange = (LFPG_WorldUtil.DistSq(playerPos, ownerObjPos) <= vSyncMaxDistSq);

            // v0.7.35 B-CRIT2: Also check distance to target devices
            if (!inRange)
            {
                int tp;
                for (tp = 0; tp < vTargetPositions.Count(); tp = tp + 1)
                {
                    if (LFPG_WorldUtil.DistSq(playerPos, vTargetPositions[tp]) <= vSyncMaxDistSq)
                    {
                        inRange = true;
                        break;
                    }
                }
            }

            if (!inRange) continue;

            ScriptRPC rpc = new ScriptRPC();
            rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES);
            rpc.Write(ownerDeviceId);
            rpc.Write(low);
            rpc.Write(high);
            rpc.Write(json);
            bool bRpcGuaranteed = true;
            PlayerIdentity noExclude = null;
            rpc.Send(pb, LFPG_RPC_CHANNEL, bRpcGuaranteed, noExclude);
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
        bool bRpcGuaranteed = true;
        PlayerIdentity noExclude = null;
        rpc.Send(player, LFPG_RPC_CHANNEL, bRpcGuaranteed, noExclude);
    }

    // ===========================
    // Full sync to joining player
    // ===========================
    void SendFullSyncTo(PlayerBase player)
    {
        if (!player) return;

        // v0.7.38 (RC-05): Set FullSync mutex.
        // While true, BroadcastOwnerWires/BroadcastVanillaWires defer instead
        // of sending immediately. Prevents reordering where a Broadcast arrives
        // at client BEFORE the FullSync RPC for the same owner.
        m_FullSyncInProgress = true;

        vector pp = player.GetPosition();
        float maxDist = LFPG_CULL_DISTANCE_M + 20.0;
        // v0.7.11 (A3): Precompute squared threshold for distance culling.
        float maxDistSq = maxDist * maxDist;

        array<EntityAI> all = new array<EntityAI>;
        LFPG_DeviceRegistry.Get().GetAll(all);

        string fsMsg = "[FullSync] devices=" + all.Count().ToString() + " playerPos=" + pp.ToString();
        LFPG_Util.Info(fsMsg);

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

            string fsDevMsg = "[FullSync] LFPG dev=" + devId + " net=" + low.ToString() + ":" + high.ToString() + " type=" + all[i].GetType() + " jsonLen=" + json.Length().ToString();
            LFPG_Util.Info(fsDevMsg);

            // v0.7.35 D8: Warn if blob approaching practical RPC size limit
            if (json.Length() > 12000)
            {
                string fsWarn = "[FullSync] LARGE BLOB dev=" + devId + " jsonLen=" + json.Length().ToString() + " — approaching RPC limit";
                LFPG_Util.Warn(fsWarn);
            }

            rpc.Write(json);

            bool bRpcGuaranteed = true;
            PlayerIdentity noExclude = null;
            rpc.Send(player, LFPG_RPC_CHANNEL, bRpcGuaranteed, noExclude);
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

        // v0.7.38 (RC-05): Release mutex and flush deferred broadcasts.
        // Any mutation that happened during the FullSync iteration is now
        // sent with the latest state, ensuring the client receives it AFTER
        // all FullSync RPCs for ordered consistency.
        m_FullSyncInProgress = false;
        FlushDeferredBroadcasts();
    }

    // v0.7.38 (RC-05): Flush broadcasts that were deferred during FullSync.
    // Called immediately after m_FullSyncInProgress is cleared.
    // Re-broadcasts the LATEST state for each deferred owner, not the state
    // at deferral time, ensuring clients always receive current data.
    protected void FlushDeferredBroadcasts()
    {
        // Flush LFPG deferred broadcasts
        int li;
        for (li = 0; li < m_DeferredBroadcastLFPG.Count(); li = li + 1)
        {
            EntityAI lfpgOwner = m_DeferredBroadcastLFPG[li];
            if (lfpgOwner)
            {
                BroadcastOwnerWires(lfpgOwner);
            }
        }
        m_DeferredBroadcastLFPG.Clear();

        // Flush vanilla deferred broadcasts
        int vi;
        for (vi = 0; vi < m_DeferredBroadcastVanillaIds.Count(); vi = vi + 1)
        {
            string vId = m_DeferredBroadcastVanillaIds[vi];
            EntityAI vObj = m_DeferredBroadcastVanillaObjs[vi];
            if (vId != "" && vObj)
            {
                BroadcastVanillaWires(vId, vObj);
            }
        }
        m_DeferredBroadcastVanillaIds.Clear();
        m_DeferredBroadcastVanillaObjs.Clear();
    }

    // ===========================
    // v0.7.35 D1: Device-specific sync (unicast)
    // Sends wire blobs for all owners relevant to deviceId:
    //   1. If deviceId is a wire-owner → send its own blob
    //   2. Any wire-owner whose wires target deviceId → send those blobs
    //   3. Vanilla wires owned by or targeting deviceId
    // ===========================
    void SendDeviceSyncTo(PlayerBase player, string deviceId)
    {
        if (!player || deviceId == "") return;

        LFPG_DeviceRegistry reg = LFPG_DeviceRegistry.Get();
        if (!reg) return;

        // 1. If the device itself is a wire-owner, send its blob
        EntityAI deviceObj = reg.FindById(deviceId);
        if (deviceObj && LFPG_DeviceAPI.HasWireStore(deviceObj))
        {
            SendOwnerBlobTo(player, deviceObj, deviceId);
        }

        // 2. Iterate all wire-owning devices to find owners targeting this device
        array<EntityAI> all = new array<EntityAI>;
        reg.GetAll(all);

        int i;
        for (i = 0; i < all.Count(); i = i + 1)
        {
            if (!all[i]) continue;
            if (!LFPG_DeviceAPI.HasWireStore(all[i])) continue;

            string ownerId = LFPG_DeviceAPI.GetDeviceId(all[i]);
            if (ownerId == deviceId) continue;  // Already sent above

            ref array<ref LFPG_WireData> wires = LFPG_DeviceAPI.GetDeviceWires(all[i]);
            if (!wires) continue;

            bool targetsDevice = false;
            int w;
            for (w = 0; w < wires.Count(); w = w + 1)
            {
                if (wires[w] && wires[w].m_TargetDeviceId == deviceId)
                {
                    targetsDevice = true;
                    break;
                }
            }

            if (targetsDevice)
            {
                SendOwnerBlobTo(player, all[i], ownerId);
            }
        }

        // 3. Vanilla wires: check if deviceId is owner or target
        int vk;
        for (vk = 0; vk < m_VanillaWires.Count(); vk = vk + 1)
        {
            string vOwnerId = m_VanillaWires.GetKey(vk);

            bool shouldSend = false;
            if (vOwnerId == deviceId)
            {
                shouldSend = true;
            }
            else
            {
                ref array<ref LFPG_WireData> vWires = m_VanillaWires.GetElement(vk);
                if (vWires)
                {
                    int vw;
                    for (vw = 0; vw < vWires.Count(); vw = vw + 1)
                    {
                        if (vWires[vw] && vWires[vw].m_TargetDeviceId == deviceId)
                        {
                            shouldSend = true;
                            break;
                        }
                    }
                }
            }

            if (shouldSend)
            {
                EntityAI vObj = reg.FindById(vOwnerId);
                if (vObj)
                {
                    SendVanillaWiresTo(player, vOwnerId, vObj);
                }
            }
        }

        string sdMsg = "[SendDeviceSyncTo] Completed for deviceId=" + deviceId;
        LFPG_Util.Info(sdMsg);
    }

    // Helper: unicast a single owner's wire blob to one player
    void SendOwnerBlobTo(PlayerBase player, EntityAI ownerObj, string ownerId)
    {
        if (!player || !ownerObj || ownerId == "") return;

        string json = LFPG_DeviceAPI.GetWiresJSON(ownerObj);

        int low = 0;
        int high = 0;
        ownerObj.GetNetworkID(low, high);

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES);
        rpc.Write(ownerId);
        rpc.Write(low);
        rpc.Write(high);
        rpc.Write(json);
        bool bRpcGuaranteed = true;
        PlayerIdentity noExclude = null;
        rpc.Send(player, LFPG_RPC_CHANNEL, bRpcGuaranteed, noExclude);

        string sobMsg = "[SendOwnerBlobTo] owner=" + ownerId + " net=" + low.ToString() + ":" + high.ToString() + " jsonLen=" + json.Length().ToString();
        LFPG_Util.Info(sobMsg);

        // v0.7.35 D8: Warn if blob approaching practical RPC size limit
        if (json.Length() > 12000)
        {
            string sobWarn = "[SendOwnerBlobTo] LARGE BLOB owner=" + ownerId + " jsonLen=" + json.Length().ToString() + " — approaching RPC limit";
            LFPG_Util.Warn(sobWarn);
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
            string gNullMsg = "[Propagate] Graph null, cannot propagate " + sourceDeviceId;
            LFPG_Util.Warn(gNullMsg);
            return;
        }

        // Refresh source on/off state in the graph from the entity
        m_Graph.RefreshSourceState(sourceDeviceId);

        // Mark dirty — will be picked up by next TickPropagation
        m_Graph.MarkNodeDirty(sourceDeviceId, LFPG_DIRTY_INTERNAL);

        string qdMsg = "[Propagate] Queued dirty: " + sourceDeviceId;
        LFPG_Util.Debug(qdMsg);
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
            string wdMsg = "[Propagate] Warmup drain complete";
            LFPG_Util.Info(wdMsg);
        }

        if (remaining > 0)
        {
            string tickMsg = "[Propagate] Tick: " + remaining.ToString() + " remaining" + " nodeBudget=" + nodeBudget.ToString() + " edgeBudget=" + edgeBudget.ToString() + " edgesUsed=" + m_Graph.GetLastEdgesVisited().ToString();
            LFPG_Util.Debug(tickMsg);
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
        float nowMs = GetGame().GetTime();
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

            string tLog = "[Telemetry-Propagation]";
            tLog = tLog + " ticks=" + m_TelemTickCount.ToString();
            tLog = tLog + " avgMs=" + avgMs.ToString();
            tLog = tLog + " peakMs=" + m_TelemPeakProcessMs.ToString();
            tLog = tLog + " avgEdges=" + avgEdges.ToString();
            tLog = tLog + " nodes=" + m_Graph.GetNodeCount().ToString();
            tLog = tLog + " edges=" + m_Graph.GetEdgeCount().ToString();
            tLog = tLog + " queueRemain=" + remaining.ToString();
            tLog = tLog + " overloadedSources=" + overloadCount.ToString();
            tLog = tLog + " epoch=" + m_Graph.GetCurrentEpoch().ToString();
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
    //
    // v0.7.38 (RC-03): Safety invariant — concurrent CutAllWiresFromDevice.
    // If an RPC calls CutAllWiresFromDevice (→ UntrackDeviceFromPolling with
    // swap-and-pop) between the batch read and process phases, some devices
    // may be skipped in the current batch. This is harmless:
    //   - Skipped devices are checked in the next round-robin cycle
    //   - UntrackDeviceFromPolling is idempotent (m_TrackedDeviceIndex guard)
    //   - Cursor is advanced AFTER all untracking, with post-mutation clamp
    // No fix needed; invariant holds by design.

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

        string rtdMsg = "[Movement] RebuildTrackedDevices: tracking " + m_TrackedDeviceIds.Count().ToString() + " wired devices";
        LFPG_Util.Info(rtdMsg);
        #endif
    }

    // v0.7.48 (Bug 2): Clean wires for a vanilla device that disappeared.
    // Entity is gone — works by deviceId only. Handles:
    //   1. Collect neighbors from graph (before edges are removed)
    //   2. Owned wires in m_VanillaWires: reverse index + player quota
    //   3. Incoming wires via reverse index scan
    //   4. Graph cleanup via OnDeviceRemoved (single call, all edges + node)
    //   5. SetPowered(false) on neighbor devices
    //   6. Flush broadcasts + schedule self-heal for client ConnCache resync
    //   7. Untrack neighbors that lost all wires
    //
    // Cannot broadcast the disappeared device's wires directly (entity is gone,
    // no NetworkID for RPC). Client-side cleanup happens via:
    //   - CableRenderer.CullTick: nullOwnerTicks destroys visual cables (~5s)
    //   - Self-heal: full broadcast refreshes ConnCache (500ms deferred)
    protected void CleanDisappearedVanillaDevice(string deviceId)
    {
        #ifdef SERVER
        if (deviceId == "" || deviceId.IndexOf("vp:") != 0)
            return;

        bool anyChanged = false;

        // --- 1. Collect neighbor IDs from graph BEFORE removing edges ---
        // Graph has the authoritative topology. Using graph edges is more
        // reliable than scanning m_VanillaWires alone (catches both directions).
        ref array<string> neighborIds = new array<string>;

        if (m_Graph)
        {
            ref array<ref LFPG_ElecEdge> outEdges = m_Graph.GetOutgoing(deviceId);
            if (outEdges)
            {
                int oe;
                for (oe = 0; oe < outEdges.Count(); oe = oe + 1)
                {
                    ref LFPG_ElecEdge oEdge = outEdges[oe];
                    if (oEdge && oEdge.m_TargetNodeId != "")
                    {
                        neighborIds.Insert(oEdge.m_TargetNodeId);
                    }
                }
            }

            ref array<ref LFPG_ElecEdge> inEdges = m_Graph.GetIncoming(deviceId);
            if (inEdges)
            {
                int ie;
                for (ie = 0; ie < inEdges.Count(); ie = ie + 1)
                {
                    ref LFPG_ElecEdge iEdge = inEdges[ie];
                    if (iEdge && iEdge.m_SourceNodeId != "")
                    {
                        neighborIds.Insert(iEdge.m_SourceNodeId);
                    }
                }
            }
        }

        // --- 2. Clear owned wires: reverse index + player quota ---
        // Do NOT call OnWireRemoved per wire — OnDeviceRemoved in step 4
        // handles all graph edges in a single pass (avoids double removal).
        ref array<ref LFPG_WireData> vWires;
        if (m_VanillaWires.Find(deviceId, vWires) && vWires)
        {
            int vw = vWires.Count() - 1;
            while (vw >= 0)
            {
                LFPG_WireData vwd = vWires[vw];
                if (vwd)
                {
                    ReverseIdxRemove(vwd.m_TargetDeviceId, vwd.m_TargetPort, deviceId);
                    PlayerWireCountAdd(vwd.m_CreatorId, -1);
                }
                vw = vw - 1;
            }
            vWires.Clear();
            anyChanged = true;
        }
        m_VanillaWires.Remove(deviceId);

        // --- 3. Remove incoming wires targeting this device ---
        // Entity is gone so we cannot iterate ports. Scan reverse index
        // for any key starting with "deviceId|" to find affected ports.
        string keyPrefix = deviceId + "|";
        ref array<string> keysToClean = new array<string>;
        int rk;
        for (rk = 0; rk < m_ReverseIdx.Count(); rk = rk + 1)
        {
            string rKey = m_ReverseIdx.GetKey(rk);
            if (rKey.IndexOf(keyPrefix) == 0)
            {
                keysToClean.Insert(rKey);
            }
        }

        int ik;
        for (ik = 0; ik < keysToClean.Count(); ik = ik + 1)
        {
            string fullKey = keysToClean[ik];
            int pipePos = fullKey.IndexOf("|");
            string portName = "input_main";
            if (pipePos >= 0)
            {
                int afterPipe = pipePos + 1;
                int portLen = fullKey.Length() - afterPipe;
                if (portLen > 0)
                {
                    portName = fullKey.Substring(afterPipe, portLen);
                }
            }

            int removed = RemoveWiresTargeting(deviceId, portName);
            if (removed > 0)
            {
                anyChanged = true;
            }
        }

        // --- 4. Graph: single OnDeviceRemoved call ---
        // Removes ALL edges (in+out) and the node in one pass.
        // Individual OnWireRemoved calls are NOT needed — OnDeviceRemoved
        // handles the full topology cleanup more efficiently.
        if (anyChanged && m_Graph)
        {
            m_Graph.OnDeviceRemoved(deviceId);
        }

        // --- 5. SetPowered(false) on neighbor devices ---
        int ni;
        for (ni = 0; ni < neighborIds.Count(); ni = ni + 1)
        {
            EntityAI neighborDev = LFPG_DeviceRegistry.Get().FindById(neighborIds[ni]);
            if (!neighborDev)
            {
                neighborDev = LFPG_DeviceAPI.ResolveVanillaDevice(neighborIds[ni]);
            }
            if (neighborDev)
            {
                LFPG_DeviceAPI.SetPowered(neighborDev, false);
            }
        }

        // --- 6. Persistence, propagation, broadcasts, self-heal ---
        if (anyChanged)
        {
            MarkVanillaDirty();
            PostBulkRebuildAndPropagate();
            FlushBroadcasts();

            // v0.7.48: Immediate vanilla flush for crash safety.
            // Same pattern as CutAllWiresFromDevice (v0.7.32 Audit P2).
            // MarkVanillaDirty defers write to 30s timer. If server crashes
            // before that timer fires, deleted wires reappear on restart —
            // re-creating the phantom port this fix is meant to solve.
            // Device disappearance is infrequent; synchronous I/O is negligible.
            if (m_VanillaDirty)
            {
                FlushVanillaIfDirty();
            }

            LFPG_Util.Warn("[VanillaGone] Cleaned wires for disappeared device " + deviceId);
        }

        // Clean position tracking
        m_LastKnownPos.Remove(deviceId);

        // Schedule self-heal for client ConnCache resync.
        // Cannot send targeted RPC without entity's NetworkID.
        // Self-heal broadcasts fresh wire data to all clients (500ms deferred).
        RequestGlobalSelfHeal();

        // --- 7. Untrack neighbors that lost all wires ---
        int nui;
        for (nui = 0; nui < neighborIds.Count(); nui = nui + 1)
        {
            string nId = neighborIds[nui];
            EntityAI nDev = LFPG_DeviceRegistry.Get().FindById(nId);
            if (nDev)
            {
                if (!DeviceHasAnyWires(nDev, nId))
                {
                    UntrackDeviceFromPolling(nId);
                }
            }
            else
            {
                UntrackDeviceFromPolling(nId);
            }
        }
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
                // v0.7.33 (Fix #21): Do NOT update baseline position every tick.
                // Previous behavior reset m_LastKnownPos each tick, so micro-drift
                // (e.g., 0.1m/tick from physics jitter) never accumulated past
                // the 0.3m threshold. Now the baseline stays at the position when
                // the device was first tracked (wire connected). Drift accumulates
                // until it crosses the threshold, triggering wire disconnect.
                // Baseline is reset when device is untracked+retracked (new wire).
            }
            else
            {
                // First time tracking this device — record initial baseline
                m_LastKnownPos.Set(devId, currentPos);
            }
        }

        // Advance cursor (wraps naturally on next tick)
        // v0.7.36 (Audit Feb2026): Cursor advancement deferred to AFTER all
        // untracking. Previously set here before untracking, which caused the
        // cursor to point past the shrunk array when multiple devices were
        // removed in the same batch. Each UntrackDeviceFromPolling call would
        // independently clamp to 0, creating uneven scan rates.
        // Now we advance once at the end after all array mutations are done.
        int newCursor = batchEnd;

        // v0.7.48 (Bug 2): Process disappeared devices.
        // LFPG devices have EEDelete/EEItemLocationChanged hooks that call
        // CutAllWiresFromDevice. Vanilla devices (vp: prefix) do NOT have
        // these hooks, so their wires become orphans when they disappear.
        // Clean vanilla wires immediately instead of waiting for self-heal.
        int di;
        for (di = 0; di < disappearedIds.Count(); di = di + 1)
        {
            string goneId = disappearedIds[di];

            if (goneId.IndexOf("vp:") == 0)
            {
                LFPG_Util.Warn("[Movement] Vanilla device disappeared id=" + goneId + " — cleaning orphan wires");
                CleanDisappearedVanillaDevice(goneId);
            }

            UntrackDeviceFromPolling(goneId);
        }

        // Process moved devices
        int mi;
        for (mi = 0; mi < movedIds.Count(); mi = mi + 1)
        {
            EntityAI movedDev = movedDevs[mi];
            string movedId = movedIds[mi];

            string mvMsg = "[Movement] Device " + movedId + " type=" + movedDev.GetType() + " moved — disconnecting wires";
            LFPG_Util.Warn(mvMsg);

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
            string mvCntMsg = "[Movement] " + movedIds.Count().ToString() + " devices moved, requesting self-heal";
            LFPG_Util.Info(mvCntMsg);
            RequestGlobalSelfHeal();
        }

        // v0.7.36 (Audit Feb2026): Final cursor update after all array mutations.
        // Single clamp ensures cursor is valid for the post-untrack array size.
        int trackedCount = m_TrackedDeviceIds.Count();
        if (trackedCount == 0)
        {
            m_TrackCursor = 0;
        }
        else if (newCursor >= trackedCount)
        {
            m_TrackCursor = 0;
        }
        else
        {
            m_TrackCursor = newCursor;
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
        bool bOnce = false;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DoGlobalSelfHeal, 500, bOnce);
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
        string shScanMsg = "SelfHeal: scanning devices=" + deviceCount.ToString();
        LFPG_Util.Debug(shScanMsg);

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

        // Step 3b (v0.7.44 Level 3): Re-populate target NetworkIDs on wire data.
        // NetworkIDs are session-only (change every server restart).
        // After restart, deserialized WireData contains STALE NetworkIDs
        // from the previous session — these MUST be overwritten unconditionally.
        // If target entity is not in registry yet, zero out the fields so
        // BroadcastOwnerWires does not send stale IDs to clients.
        // Cost: GetNetworkID is a field read, trivial even for 200+ wires.
        int rn;
        for (rn = 0; rn < all.Count(); rn = rn + 1)
        {
            ref array<ref LFPG_WireData> rnWires = LFPG_DeviceAPI.GetDeviceWires(all[rn]);
            if (!rnWires) continue;
            int rw;
            int rnLow;
            int rnHigh;
            for (rw = 0; rw < rnWires.Count(); rw = rw + 1)
            {
                LFPG_WireData rnWd = rnWires[rw];
                if (!rnWd) continue;
                EntityAI rnTgt = LFPG_DeviceRegistry.Get().FindById(rnWd.m_TargetDeviceId);
                if (rnTgt)
                {
                    rnLow = 0;
                    rnHigh = 0;
                    rnTgt.GetNetworkID(rnLow, rnHigh);
                    rnWd.m_TargetNetLow = rnLow;
                    rnWd.m_TargetNetHigh = rnHigh;
                }
                else
                {
                    // Target not in registry — zero out stale values so
                    // BroadcastOwnerWires does not send garbage to clients.
                    // Next self-heal tick will re-populate when resolved.
                    rnWd.m_TargetNetLow = 0;
                    rnWd.m_TargetNetHigh = 0;
                }
            }
        }

        // Step 3c (v0.7.45 H8): Same repopulation for vanilla wires.
        // Step 3b only covers LFPG device wires (stored ON the entity).
        // Vanilla wires are stored centrally in m_VanillaWires and also
        // need their target NetworkIDs refreshed after server restart.
        // Without this, CableRenderer client-side fallback resolution
        // for vanilla-sourced wires has no NetworkID to work with.
        int vnri;
        for (vnri = 0; vnri < m_VanillaWires.Count(); vnri = vnri + 1)
        {
            ref array<ref LFPG_WireData> vnrWires = m_VanillaWires.GetElement(vnri);
            if (!vnrWires) continue;
            int vnrw;
            int vnrLow;
            int vnrHigh;
            for (vnrw = 0; vnrw < vnrWires.Count(); vnrw = vnrw + 1)
            {
                LFPG_WireData vnrWd = vnrWires[vnrw];
                if (!vnrWd) continue;
                EntityAI vnrTgt = LFPG_DeviceRegistry.Get().FindById(vnrWd.m_TargetDeviceId);
                if (vnrTgt)
                {
                    vnrLow = 0;
                    vnrHigh = 0;
                    vnrTgt.GetNetworkID(vnrLow, vnrHigh);
                    vnrWd.m_TargetNetLow = vnrLow;
                    vnrWd.m_TargetNetHigh = vnrHigh;
                }
                else
                {
                    vnrWd.m_TargetNetLow = 0;
                    vnrWd.m_TargetNetHigh = 0;
                }
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
        }

        // Clear cache after all prune calls are done
        m_CachedValidIds = null;

        // v0.9.3 (Audit): Removed RequestPropagate loops from steps 4+5.
        // These called RequestPropagate on every active source, but the
        // graph is rebuilt from scratch in step 6 (RebuildFromWires clears
        // m_DirtyQueue). MarkSourcesDirty then re-marks all sources dirty
        // on the NEW graph. The old RequestPropagate calls were pure dead
        // work: RefreshSourceState + MarkNodeDirty → all discarded.

        string shDoneMsg = "SelfHeal: done. pruned=" + ownersPruned.ToString();
        LFPG_Util.Debug(shDoneMsg);

        // Full rebuild of indexes on self-heal (authoritative baseline)
        RebuildReverseIdx();
        RecountAllPlayerWires();

        // v0.7.4: prune vanilla wires whose owner or target can't be resolved.
        // Devices that were moved/destroyed leave orphaned wires in the
        // central store. Persist the pruned state on next flush.
        PruneUnresolvableVanillaWires();

        // v0.7.32 (Audit P2): Flush vanilla immediately after prune.
        // Self-heal runs once at startup and after critical failures.
        // If server crashes before the periodic timer flush (30s),
        // stale wires reappear on next restart → repeat prune cycle.
        if (m_VanillaDirty)
        {
            FlushVanillaIfDirty();
        }

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

            // v0.9.3 (Audit Fix #4): Immediate flush after graph warmup.
            // Without this, the first propagation tick is delayed until the
            // next TickPropagation call (100ms). PostBulkRebuildAndPropagate
            // already does this (line ~618) but ValidateAllWiresAndPropagate
            // did not — causing a 100ms+ blackout window where consumers stay
            // unpowered and cables show IDLE after restart.
            // Flushing here ensures SyncNodeToEntity runs in the same frame
            // as the rebuild, so DayZ SyncVar batching delivers correct state
            // to clients without visible flicker.
            int flushBudget = LFPG_PROPAGATE_WARMUP_BUDGET;
            int flushEdge = LFPG_PROPAGATE_EDGE_WARMUP_BUDGET;
            m_Graph.ProcessDirtyQueue(flushBudget, flushEdge);

            string shWarmMsg = "[SelfHeal] Graph warmup: rebuilt + populated + sources dirty + flushed (warmup budget active)";
            LFPG_Util.Info(shWarmMsg);
        }

        // v0.7.26 (Audit 4): Prune stale entries from m_LastKnownPos.
        // Devices that were deleted/destroyed leave orphan entries.
        // Without cleanup, this map grows unboundedly on long-running servers.
        PruneStaleLastKnownPositions();

        // v0.7.30: Rebuild tracked device set from validated wire state.
        // Must happen after graph rebuild + prune so the tracked set matches
        // the authoritative wire topology.
        RebuildTrackedDevices();

        // v0.7.38 (RC-07): Mark startup validation complete.
        // RPC handlers can now accept wiring requests.
        m_StartupValidationDone = true;
        LFPG_Util.Info("[SelfHeal] Startup validation done — RPCs enabled");
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
                    string vpMsg = "[VanillaPrune] Removed wire " + ownerId + " -> " + wd.m_TargetDeviceId + " (target unresolvable)";
                    LFPG_Util.Debug(vpMsg);
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
            string shPruneMsg = "[SelfHeal] Pruned " + totalPruned.ToString() + " unresolvable vanilla wire(s), " + emptyOwners.Count().ToString() + " empty owner(s)";
            LFPG_Util.Info(shPruneMsg);
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
            string shStaleMsg = "[SelfHeal] Pruned " + staleIds.Count().ToString() + " stale position tracking entries";
            LFPG_Util.Debug(shStaleMsg);
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
            string vFlushMsg = "[VanillaWires] Flushing on shutdown...";
            LFPG_Util.Info(vFlushMsg);
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
            string saveBlockMsg = "[VanillaWires] SAVE BLOCKED: loaded from schema v" + m_VanillaLoadedVer.ToString() + " > current v" + LFPG_VANILLA_PERSIST_VER.ToString() + ". Upgrade the mod to save changes.";
            LFPG_Util.Warn(saveBlockMsg);
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
                entry.m_OwnerDeviceId = ownerId;
                entry.m_TargetDeviceId = wd.m_TargetDeviceId;
                entry.m_TargetPort = wd.m_TargetPort;
                entry.m_SourcePort = wd.m_SourcePort;
                entry.m_CreatorId = wd.m_CreatorId;

                // Persist waypoints (v0.7.3)
                if (wd.m_Waypoints && wd.m_Waypoints.Count() > 0)
                {
                    int wp;
                    for (wp = 0; wp < wd.m_Waypoints.Count(); wp = wp + 1)
                    {
                        entry.m_Waypoints.Insert(wd.m_Waypoints[wp]);
                    }
                }

                store.entries.Insert(entry);
            }
        }

        // v0.7.15 (Sprint 3 P2b): Atomic save with backup rotation
        if (LFPG_FileUtil.AtomicSaveVanillaWires(VANILLA_WIRES_FILE, store))
        {
            string vSaveMsg = "[VanillaWires] Saved " + store.entries.Count().ToString() + " entries (atomic)";
            LFPG_Util.Info(vSaveMsg);
        }
        else
        {
            string vSaveErr = "[VanillaWires] Atomic save failed!";
            LFPG_Util.Error(vSaveErr);
        }
        #endif
    }

    protected void LoadVanillaWires()
    {
        #ifdef SERVER
        // v0.7.15 (Sprint 3 P2b): Attempt backup restore if main file missing
        if (!LFPG_FileUtil.EnsureFileOrRestore(VANILLA_WIRES_FILE))
        {
            string vFreshMsg = "[VanillaWires] No saved file found, starting fresh.";
            LFPG_Util.Info(vFreshMsg);
            return;
        }

        LFPG_VanillaWireStore store = new LFPG_VanillaWireStore();
        string err;
        if (!JsonFileLoader<LFPG_VanillaWireStore>.LoadFile(VANILLA_WIRES_FILE, store, err))
        {
            string vLoadErr = "[VanillaWires] Load failed: " + err;
            LFPG_Util.Warn(vLoadErr);
            return;
        }

        if (!store.entries)
        {
            string vEmptyMsg = "[VanillaWires] Loaded empty store.";
            LFPG_Util.Info(vEmptyMsg);
            return;
        }

        // v0.7.16 H6: Track loaded version for save guard
        m_VanillaLoadedVer = store.ver;

        // v0.7.16 H6: If loaded from a newer schema, enter read-only mode
        if (m_VanillaLoadedVer > LFPG_VANILLA_PERSIST_VER)
        {
            m_VanillaReadOnly = true;
            string vSchemaMsg = "[VanillaWires] Schema v" + m_VanillaLoadedVer.ToString() + " > current v" + LFPG_VANILLA_PERSIST_VER.ToString() + ". Entering READ-ONLY mode to protect data. Upgrade the mod.";
            LFPG_Util.Warn(vSchemaMsg);
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
            if (entry.m_OwnerDeviceId == "" || entry.m_TargetDeviceId == "")
            {
                discarded = discarded + 1;
                continue;
            }

            LFPG_WireData wd = new LFPG_WireData();
            wd.m_TargetDeviceId = entry.m_TargetDeviceId;
            wd.m_TargetPort = entry.m_TargetPort;
            wd.m_SourcePort = entry.m_SourcePort;
            wd.m_CreatorId = entry.m_CreatorId;

            // Restore waypoints (v0.7.3)
            if (entry.m_Waypoints && entry.m_Waypoints.Count() > 0)
            {
                int wp;
                for (wp = 0; wp < entry.m_Waypoints.Count(); wp = wp + 1)
                {
                    wd.m_Waypoints.Insert(entry.m_Waypoints[wp]);
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
            if (!dedupByOwner.Find(entry.m_OwnerDeviceId, ownerDedup) || !ownerDedup)
            {
                ownerDedup = new map<string, bool>;
                dedupByOwner.Set(entry.m_OwnerDeviceId, ownerDedup);
            }

            string dedupKey = wd.m_TargetDeviceId + "|" + wd.m_TargetPort + "|" + wd.m_SourcePort;
            bool isDup = false;
            ownerDedup.Find(dedupKey, isDup);
            if (isDup)
            {
                duplicates = duplicates + 1;
                continue;
            }
            bool bDedup = true;
            ownerDedup.Set(dedupKey, bDedup);

            // Insert into wire map
            ref array<ref LFPG_WireData> wires;
            if (!m_VanillaWires.Find(entry.m_OwnerDeviceId, wires) || !wires)
            {
                wires = new array<ref LFPG_WireData>;
                m_VanillaWires[entry.m_OwnerDeviceId] = wires;
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
        // NOTE: Individual OnWireRemoved calls are NOT needed here.
        // Section 6 calls OnDeviceRemoved + PostBulkRebuildAndPropagate,
        // which does a full graph rebuild from wire data. Any incremental
        // graph updates would be immediately discarded.
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
                    string cutMsg = "[CutAll] Removed " + removed.ToString() + " incoming wire(s) on " + deviceId + ":" + portName;
                    LFPG_Util.Info(cutMsg);
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
                        string cutFbMsg = "[CutAll-Fallback] Found stale wire: " + srcId + " -> " + deviceId;
                        LFPG_Util.Warn(cutFbMsg);
                        PlayerWireCountAdd(swd.m_CreatorId, -1);
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
        EntityAI neighborDev;
        if (anyChanged)
        {
            // Force self off (covers consumers and passthroughs)
            bool bPwrOff = false;
            LFPG_DeviceAPI.SetPowered(device, bPwrOff);

            // Force all neighbors off — propagation will re-enable
            // any that still have an alternate power path.
            int nbi;
            for (nbi = 0; nbi < neighborIds.Count(); nbi = nbi + 1)
            {
                neighborDev = LFPG_DeviceRegistry.Get().FindById(neighborIds[nbi]);
                if (neighborDev)
                {
                    LFPG_DeviceAPI.SetPowered(neighborDev, bPwrOff);
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
            string cutAllMsg = "[CutAll] All wires removed for device " + deviceId + " type=" + device.GetType();
            LFPG_Util.Info(cutAllMsg);

            // v0.7.28: Safety flush — ensure all queued RPCs reach clients
            // immediately. Sections 1-4 use direct BroadcastOwnerWires and
            // RemoveWiresTargeting (which flushes internally), but this
            // covers any edge case where a broadcast was queued but not sent.
            FlushBroadcasts();

            // v0.7.32 (Audit P2): Immediate vanilla flush after critical cut.
            // MarkVanillaDirty() was called in section 2, but FlushVanillaIfDirty
            // runs on a 30s timer. If server crashes in that window, vanilla
            // wires are lost. CutAll is infrequent enough that synchronous
            // flush has negligible perf impact.
            if (m_VanillaDirty)
            {
                FlushVanillaIfDirty();
            }
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
            neighborDev = LFPG_DeviceRegistry.Get().FindById(neighborId);
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

    // ===========================
    // v0.8.0: Centralized Solar Timer
    // ===========================
    // Single timer replaces N per-panel CallLater timers.
    // Benefits:
    //   - 1 GetDate() call instead of N (100 panels = 100x savings)
    //   - Atomic state change (all panels transition in same frame)
    //   - No timer leak on panel delete (no per-panel timer to stop)
    //   - Eliminates race condition where panels in the same tick
    //     see different sun states during dawn/dusk transition

    // Public getter: panels read cached sun state on EEInit
    // (avoids per-panel GetDate call during initialization).
    bool LFPG_GetCachedSunState()
    {
        return m_SolarHasSun;
    }

    // Read world time once, update cached sun state.
    // Called by constructor (seed) and by LFPG_TickSolarPanels (periodic).
    protected void LFPG_ComputeSunState()
    {
        #ifdef SERVER
        if (!GetGame())
            return;

        World world = GetGame().GetWorld();
        if (!world)
            return;

        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        world.GetDate(year, month, day, hour, minute);

        bool hasSun = false;
        if (hour >= LFPG_SOLAR_DAWN_HOUR && hour < LFPG_SOLAR_DUSK_HOUR)
        {
            hasSun = true;
        }

        m_SolarHasSun = hasSun;
        #endif
    }

    // Periodic tick (every LFPG_SOLAR_CHECK_MS = 15s).
    // Recomputes sun state; if unchanged, returns immediately (O(1)).
    // If changed, iterates all registered devices, updates solar panels.
    // LF_SolarPanel.Cast catches both T1 and T2 (T2 inherits T1).
    protected void LFPG_TickSolarPanels()
    {
        #ifdef SERVER
        bool prevSun = m_SolarHasSun;
        LFPG_ComputeSunState();

        // No transition → nothing to do. This is the common case
        // (dawn/dusk only happens twice per in-game day).
        if (m_SolarHasSun == prevSun)
            return;

        // Sun state changed — update all solar panels
        array<EntityAI> allDevs = new array<EntityAI>;
        LFPG_DeviceRegistry.Get().GetAll(allDevs);

        int i;
        int updated = 0;
        for (i = 0; i < allDevs.Count(); i = i + 1)
        {
            LF_SolarPanel panel = LF_SolarPanel.Cast(allDevs[i]);
            if (!panel)
                continue;

            panel.LFPG_UpdateSunState(m_SolarHasSun);
            updated = updated + 1;
        }

        string msg = "[Solar] Sun changed to " + m_SolarHasSun.ToString() + ", updated " + updated.ToString() + " panels";
        LFPG_Util.Info(msg);
        #endif
    }

    // ===========================
    // v1.1.0: Water Pump Timer
    // ===========================
    // Two sub-systems:
    //   1. Tablet consumption: real-time (ms), 1 tablet per LFPG_PUMP_TABLET_INTERVAL_MS
    //   2. Tank fill: in-game hour based, LFPG_PUMP_TANK_FILL_PER_HOUR per hour

    // Seed tank fill hour from world time
    protected void LFPG_InitTankFillTime()
    {
        #ifdef SERVER
        m_TankFillLastMs = GetGame().GetTime();
        #endif
    }

    // Periodic tick (every LFPG_PUMP_CHECK_MS = 60s)
    protected void LFPG_TickWaterPumps()
    {
        #ifdef SERVER
        float nowMs = GetGame().GetTime();
        float thresholdMs = LFPG_PUMP_TABLET_INTERVAL_MS;

        // --- Compute tank fill amount from real elapsed ms ---
        float fillAmount = 0.0;
        bool doTankFill = false;

        if (m_TankFillLastMs >= 0.0)
        {
            float elapsedFillMs = nowMs - m_TankFillLastMs;
            if (elapsedFillMs > 0.0)
            {
                fillAmount = (elapsedFillMs / 3600000.0) * LFPG_PUMP_TANK_FILL_PER_HOUR;
                m_TankFillLastMs = nowMs;
                if (fillAmount > 0.001)
                {
                    doTankFill = true;
                }
            }
        }

        // --- Single iteration: tablet consumption + tank fill ---
        array<EntityAI> allDevs = new array<EntityAI>;
        LFPG_DeviceRegistry.Get().GetAll(allDevs);

        int i;
        bool isPump;
        LF_WaterPump pump1;
        LF_WaterPump_T2 pump2;
        float elapsed;

        for (i = 0; i < allDevs.Count(); i = i + 1)
        {
            // --- T1: tablet consumption only ---
            isPump = false;
            pump1 = LF_WaterPump.Cast(allDevs[i]);
            if (pump1)
            {
                isPump = true;
                elapsed = nowMs - pump1.LFPG_GetTabletLastMs();
                if (elapsed >= thresholdMs)
                {
                    pump1.LFPG_ConsumeFilterTablet();
                    pump1.LFPG_SetTabletLastMs(nowMs);
                }
            }

            // --- T2: tablet consumption + tank fill (separate class) ---
            if (!isPump)
            {
                pump2 = LF_WaterPump_T2.Cast(allDevs[i]);
                if (pump2)
                {
                    // T2 tablet consumption (same logic as T1)
                    elapsed = nowMs - pump2.LFPG_GetTabletLastMs();
                    if (elapsed >= thresholdMs)
                    {
                        pump2.LFPG_ConsumeFilterTablet();
                        pump2.LFPG_SetTabletLastMs(nowMs);
                    }

                    // T2 tank fill (only if hour changed and pump is powered)
                    if (doTankFill)
                    {
                        bool powered = pump2.LFPG_GetPoweredNet();
                        if (powered)
                        {
                            float level = pump2.LFPG_GetTankLevel();
                            if (level < LFPG_PUMP_TANK_MAX)
                            {
                                // Determine incoming water type
                                int incomingType = LIQUID_RIVERWATER;
                                if (LFPG_PumpHelper.HasActiveFilter(pump2))
                                {
                                    incomingType = LIQUID_CLEANWATER;
                                }

                                int currentType = pump2.LFPG_GetTankLiquidType();

                                // Mixing rules
                                if (level < 0.01)
                                {
                                    pump2.LFPG_SetTankLiquidType(incomingType);
                                }
                                else if (incomingType != currentType)
                                {
                                    pump2.LFPG_SetTankLiquidType(LIQUID_RIVERWATER);
                                }

                                level = level + fillAmount;
                                if (level > LFPG_PUMP_TANK_MAX)
                                {
                                    level = LFPG_PUMP_TANK_MAX;
                                }
                                pump2.LFPG_SetTankLevel(level);
                            }
                        }
                    }
                }
            }
        }
        #endif
    }

    // ===========================
    // v1.2.0 (Sprint S3): Sorter Tick
    // ===========================
    // Round-robin batch processing. Each tick processes up to
    // LFPG_SORTER_BATCH_SIZE Sorters. Each Sorter moves up to
    // LFPG_SORTER_ITEMS_PER_TICK items from its linked container
    // to downstream containers based on filter rules.
    //
    // Pattern: identical to CheckDeviceMovement round-robin.
    // Timer: 5000ms (LFPG_SORTER_TICK_MS).

    // v1.2.0 (Sprint S5): Dedicated registry — avoids iterating all devices.
    // Called from LF_Sorter.EEInit / EEDelete / EEKilled.
    void RegisterSorter(LF_Sorter sorter)
    {
        if (!sorter)
            return;
        if (m_RegisteredSorters.Find(sorter) < 0)
        {
            m_RegisteredSorters.Insert(sorter);
        }
    }

    void UnregisterSorter(LF_Sorter sorter)
    {
        if (!sorter)
            return;
        int idx = m_RegisteredSorters.Find(sorter);
        if (idx >= 0)
        {
            m_RegisteredSorters.Remove(idx);
        }
    }

    protected void LFPG_TickSorters()
    {
        #ifdef SERVER
        // 1. Use dedicated registry (no GetAll + Cast filtering)
        int total = m_RegisteredSorters.Count();
        if (total == 0)
        {
            m_SorterCursor = 0;
            return;
        }

        // Wrap cursor
        if (m_SorterCursor >= total)
        {
            m_SorterCursor = 0;
        }

        // 2. Determine batch range
        int batchEnd = m_SorterCursor + LFPG_SORTER_BATCH_SIZE;
        if (batchEnd > total)
        {
            batchEnd = total;
        }

        // 3. Process batch
        int bi;
        LF_Sorter sorter;
        EntityAI inputContainer;
        CargoBase inputCargo;
        LFPG_SortConfig filterConfig;
        int itemCount;
        int moved;
        int evaluated;
        int ci;
        int outputIdx;
        EntityAI destContainer;
        EntityAI sortItem;
        bool moveResult;
        int hasWireMask;
        int portBit;
        int pi;
        EntityAI wireCheck;

        for (bi = m_SorterCursor; bi < batchEnd; bi = bi + 1)
        {
            // Safety: array may shrink if sorter destroyed externally
            if (bi >= m_RegisteredSorters.Count())
                break;

            sorter = m_RegisteredSorters[bi];
            if (!sorter)
                continue;

            // Must be powered
            if (!sorter.LFPG_IsPowered())
                continue;

            // Must not be ruined
            if (sorter.IsRuined())
                continue;

            // Must have filter config
            filterConfig = sorter.LFPG_GetFilterConfig();
            if (!filterConfig)
                continue;

            // Must have linked container
            inputContainer = sorter.LFPG_GetLinkedContainer();
            if (!inputContainer)
                continue;

            // S3.1: Source container must be accessible (not locked/closed/virtualized)
            if (!LFPG_SorterLogic.CanTakeFromContainer(inputContainer, null))
                continue;

            // Must have cargo
            if (!inputContainer.GetInventory())
                continue;

            inputCargo = inputContainer.GetInventory().GetCargo();
            if (!inputCargo)
                continue;

            itemCount = inputCargo.GetItemCount();
            if (itemCount <= 0)
                continue;

            // E14: Build wire mask once per sorter — skip wireless outputs in eval
            hasWireMask = 0;
            portBit = 1;
            for (pi = 0; pi < 6; pi = pi + 1)
            {
                wireCheck = LFPG_SorterLogic.ResolveOutputContainer(sorter, pi);
                if (wireCheck)
                {
                    hasWireMask = hasWireMask | portBit;
                }
                portBit = portBit * 2;
            }

            // No outputs have wires — nothing to sort
            if (hasWireMask == 0)
                continue;

            // Collect items into reusable cache to avoid index mutation
            // during moves (removing from cargo shifts indices)
            m_SorterItemCache.Clear();
            for (ci = 0; ci < itemCount; ci = ci + 1)
            {
                sortItem = inputCargo.GetItem(ci);
                if (sortItem)
                {
                    m_SorterItemCache.Insert(sortItem);
                }
            }

            // Process items (up to rate limit)
            // Two caps: moved items (ITEMS_PER_TICK) and total evaluations (MAX_EVAL).
            // MAX_EVAL prevents unbounded iteration when items match rules
            // but can't be moved (no wire, dest full, etc.).
            moved = 0;
            evaluated = 0;
            for (ci = 0; ci < m_SorterItemCache.Count(); ci = ci + 1)
            {
                if (moved >= LFPG_SORTER_ITEMS_PER_TICK)
                    break;

                if (evaluated >= LFPG_SORTER_MAX_EVAL)
                    break;

                sortItem = m_SorterItemCache[ci];
                if (!sortItem)
                    continue;

                evaluated = evaluated + 1;

                // Per-item source release check — some mods block
                // specific items from being released from containers
                if (!inputContainer.CanReleaseCargo(sortItem))
                    continue;

                // Evaluate filter rules
                outputIdx = LFPG_SorterLogic.EvaluateItem(sortItem, filterConfig, hasWireMask);
                if (outputIdx < 0)
                    continue;

                // Resolve destination container via wire topology
                destContainer = LFPG_SorterLogic.ResolveOutputContainer(sorter, outputIdx);
                if (!destContainer)
                    continue;

                // Skip if destination is same as source
                if (destContainer == inputContainer)
                    continue;

                // Move item
                moveResult = LFPG_SorterLogic.MoveItemToContainer(sortItem, destContainer);
                if (moveResult)
                {
                    moved = moved + 1;
                }
            }
        }

        // 4. Advance cursor
        m_SorterCursor = batchEnd;
        if (m_SorterCursor >= total)
        {
            m_SorterCursor = 0;
        }
        #endif
    }

    // ===========================
    // v1.2.0 (Sprint S3): Sorter Bin-Pack (RPC handler)
    // ===========================
    // Called by PlayerRPC after validating proximity + type.
    // Sorter already resolved and validated upstream.

    void HandleSorterRequestSort(LF_Sorter sorter)
    {
        #ifdef SERVER
        if (!sorter)
        {
            LFPG_Util.Warn("[Sorter] REQUEST_SORT: null sorter");
            return;
        }

        // Must be powered
        if (!sorter.LFPG_IsPowered())
        {
            LFPG_Util.Debug("[Sorter] REQUEST_SORT: sorter not powered");
            return;
        }

        // Resolve container
        EntityAI container = sorter.LFPG_GetLinkedContainer();
        if (!container)
        {
            LFPG_Util.Warn("[Sorter] REQUEST_SORT: no linked container");
            return;
        }

        // Bin-pack
        int moved = LFPG_SorterLogic.BinPackCargo(container);
        string sortLog = "[Sorter] BinPack complete: repositioned " + moved.ToString() + " items";
        LFPG_Util.Info(sortLog);
        #endif
    }

};