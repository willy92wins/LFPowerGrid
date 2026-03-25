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
    protected ref array<LFPG_Sorter> m_RegisteredSorters;

    // v1.2.0 (Sprint S5): Reusable item cache for TickSorters (GC reduction)
    protected ref array<EntityAI> m_SorterItemCache;

    // v1.5.0: Motion Sensor dedicated registry
    protected ref array<LFPG_MotionSensor> m_RegisteredSensors;

    // v1.8.0: Pressure Pad dedicated registry
    protected ref array<LFPG_PressurePad> m_RegisteredPads;

    // v1.9.0: Laser Detector dedicated registry
    protected ref array<LFPG_LaserDetector> m_RegisteredLasers;

    // v2.0: Battery energy accounting state.
    // Iterated from LFPG_TickSimpleDevices (offset 4, ~5s effective).
    // EntityAI typed — LF_Battery methods resolved via dynamic dispatch.
    protected ref array<EntityAI> m_RegisteredBatteries;
    protected float m_BatteryLastTickMs;

    // v3.0: Intercom toggle input evaluation registry
    protected ref array<LFPG_Intercom> m_RegisteredIntercoms;

    // v3.1: Furnace centralized burn timer registry
    // Replaces per-device CallLater (N timers → 1 timer)
    protected ref array<LFPG_Furnace> m_RegisteredFurnaces;

    // v4.0: Fridge centralized cooling timer registry
    protected ref array<LFPG_Fridge> m_RegisteredFridges;

    // v1.0.0: Electric Stove centralized cooking timer registry
    protected ref array<LFPG_ElectricStove> m_RegisteredStoves;

    // v4.0: DoorController centralized poll timer registry
    protected ref array<LFPG_DoorController> m_RegisteredDoorControllers;

    // v4.1: Solar panel dedicated registry (replaces GetAll+Cast scan)
    protected ref array<LFPG_SolarPanel> m_RegisteredSolars;

    // v4.1: Water pump + sprinkler dedicated registries (replaces GetAll+Cast scan)
    // T1 and T2 are separate classes (T2 does NOT inherit T1).
    protected ref array<LFPG_WaterPump>    m_RegisteredT1Pumps;
    protected ref array<LFPG_WaterPump_T2> m_RegisteredT2Pumps;
    protected ref array<LFPG_Sprinkler>    m_RegisteredSprinklers;

    // v4.1: Player Detection consolidated tick sub-counters.
    // One timer at 300ms drives lasers (every tick), pads (every 2nd), sensors (every 10th).
    // LaserBeamCounter starts at 22 so first beam fires at tick 1 (300ms post-init).
    protected int m_PlayerDetectCounter;
    protected int m_LaserBeamCounter;

    // v4.1: Simple Devices consolidated tick sub-counter.
    // One timer at 1,000ms drives intercoms/DC/furnaces/batteries/fridges with stagger offsets.
    // Cycle 1-10, reset at 10. Stagger ensures Batteries and Furnaces never fire same tick.
    protected int m_SimpleTickCounter;

    // Cached valid device IDs for PruneMissingTargets (built once per self-heal cycle)
    protected ref map<string, bool> m_CachedValidIds;
	
    // v5.0: BTC ATM price fetcher (server-only)
    protected ref LFPG_BTCPriceFetcher m_BTCPriceFetcher;

    // v3.1 (GC reduction): Reusable arrays for high-frequency tick functions.
    // Hoisted from local scope to class members. .Clear() each tick instead of new.
    // Prevents heap fragmentation on long-running servers (>44K abandoned objects/hr).
    protected ref array<Man>      m_ReusablePlayers;
    protected ref array<string>   m_ReusableMovedIds;
    protected ref array<EntityAI> m_ReusableMovedDevs;
    protected ref array<string>   m_ReusableDisappearedIds;
    protected ref Param1<float>   m_ReusableParamFloat;
    protected ref Param1<bool>    m_ReusableParamBool;

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
        m_RegisteredSorters = new array<LFPG_Sorter>;
        m_SorterItemCache = new array<EntityAI>;
        m_RegisteredSensors = new array<LFPG_MotionSensor>;
        m_RegisteredPads = new array<LFPG_PressurePad>;
        m_RegisteredLasers = new array<LFPG_LaserDetector>;
        m_RegisteredBatteries = new array<EntityAI>;
        m_RegisteredIntercoms = new array<LFPG_Intercom>;
        m_RegisteredFurnaces = new array<LFPG_Furnace>;
        m_RegisteredFridges = new array<LFPG_Fridge>;
        m_RegisteredStoves = new array<LFPG_ElectricStove>;
        m_RegisteredDoorControllers = new array<LFPG_DoorController>;
        m_RegisteredSolars = new array<LFPG_SolarPanel>;
        m_RegisteredT1Pumps = new array<LFPG_WaterPump>;
        m_RegisteredT2Pumps = new array<LFPG_WaterPump_T2>;
        m_RegisteredSprinklers = new array<LFPG_Sprinkler>;

        // v3.1 (GC reduction): Initialize reusable tick arrays
        m_ReusablePlayers = new array<Man>;
        m_ReusableMovedIds = new array<string>;
        m_ReusableMovedDevs = new array<EntityAI>;
        m_ReusableDisappearedIds = new array<string>;
        m_ReusableParamFloat = new Param1<float>(0.0);
        m_ReusableParamBool = new Param1<bool>(false);

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

        // v1.1.0: Water Pump filter degradation + tank timer
        LFPG_InitTankFillTime();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_TickWaterPumps, LFPG_PUMP_CHECK_MS, bTrue);

        // v1.2.0 (Sprint S3): Sorter tick — round-robin batch sorting
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_TickSorters, LFPG_SORTER_TICK_MS, bTrue);

        // v4.1: Consolidated player detection tick (lasers 300ms + pads 600ms + sensors 3s).
        // Replaces 4 separate timers. Sub-counters gate slower devices.
        m_PlayerDetectCounter = 0;
        m_LaserBeamCounter = 22;
        int pdTickMs = 300;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_TickPlayerDetection, pdTickMs, bTrue);

        // v4.1: Consolidated simple devices tick (intercoms/DC/furnaces/batteries/fridges).
        // Replaces 5 separate timers. Stagger offsets prevent spike alignment.
        // Intercoms=every tick, DC=%2==1, Furnaces=%5==2, Batteries=%5==4, Fridges=%10==6.
        m_SimpleTickCounter = 0;
        m_BatteryLastTickMs = GetGame().GetTime();
        int simpleTickMs = 1000;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_TickSimpleDevices, simpleTickMs, bTrue);
		
		
		// v5.0: BTC ATM price fetcher
        LFPG_BTCConfig.Load();
        if (LFPG_BTCConfig.IsEnabled())
        {
            LFPG_BTCPriceFetcher.Create();
            m_BTCPriceFetcher = LFPG_BTCPriceFetcher.Get();
            if (m_BTCPriceFetcher)
            {
                m_BTCPriceFetcher.Init();
                int btcTickMs = LFPG_BTC_PRICE_CHECK_MS;
                bool bTrueBtc = true;
                GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_TickBTCPrice, btcTickMs, bTrueBtc);
                string btcInitMsg = "[NM] BTC Price fetcher initialized, tick every ";
                btcInitMsg = btcInitMsg + btcTickMs.ToString();
                btcInitMsg = btcInitMsg + "ms";
                LFPG_Util.Info(btcInitMsg);
            }
        }
        else
        {
            string btcOffMsg = "[NM] BTC ATM system DISABLED by config";
            LFPG_Util.Info(btcOffMsg);
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
        // v5.1: Instant sprinkler link refresh on wire connect
        string noRemoved = "";
        LFPG_RefreshPumpSprinklerLink(sourceId, noRemoved);
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
        // v5.1: Instant sprinkler link refresh on wire disconnect
        LFPG_RefreshPumpSprinklerLink(sourceId, targetId);
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

    // v1.3.1: Port-level power query.
    // Returns true if any incoming edge targeting the given port
    // on the given device has allocated power > 0.
    // Convenience wrapper around ElecGraph.IsPortReceivingPower().
    bool IsPortReceivingPower(string deviceId, string portName)
    {
        if (!m_Graph)
            return false;

        return m_Graph.IsPortReceivingPower(deviceId, portName);
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
        m_ReusableMovedIds.Clear();
        m_ReusableMovedDevs.Clear();
        m_ReusableDisappearedIds.Clear();

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
                m_ReusableDisappearedIds.Insert(devId);
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
                    m_ReusableMovedIds.Insert(devId);
                    m_ReusableMovedDevs.Insert(dev);
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
        for (di = 0; di < m_ReusableDisappearedIds.Count(); di = di + 1)
        {
            string goneId = m_ReusableDisappearedIds[di];

            if (goneId.IndexOf("vp:") == 0)
            {
                LFPG_Util.Warn("[Movement] Vanilla device disappeared id=" + goneId + " — cleaning orphan wires");
                CleanDisappearedVanillaDevice(goneId);
            }

            UntrackDeviceFromPolling(goneId);
        }

        // Process moved devices
        int mi;
        for (mi = 0; mi < m_ReusableMovedIds.Count(); mi = mi + 1)
        {
            EntityAI movedDev = m_ReusableMovedDevs[mi];
            string movedId = m_ReusableMovedIds[mi];

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
        if (m_ReusableMovedIds.Count() > 0)
        {
            string mvCntMsg = "[Movement] " + m_ReusableMovedIds.Count().ToString() + " devices moved, requesting self-heal";
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

	// ===========================
    // v5.0: BTC Price getters (for RPC handlers in Sprint 3)
    // ===========================
    float LFPG_GetBTCPrice()
    {
        if (m_BTCPriceFetcher)
        {
            return m_BTCPriceFetcher.GetCachedPrice();
        }
        return LFPG_BTC_PRICE_UNAVAILABLE;
    }

    bool LFPG_IsBTCPriceAvailable()
    {
        if (m_BTCPriceFetcher)
        {
            return m_BTCPriceFetcher.IsPriceAvailable();
        }
        return false;
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
	
	// ===========================
    // v5.0: BTC Price Tick
    // ===========================
    protected void LFPG_TickBTCPrice()
    {
        #ifdef SERVER
        if (m_BTCPriceFetcher)
        {
            m_BTCPriceFetcher.Tick();
        }
        #endif
    }
	
    // Periodic tick (every LFPG_SOLAR_CHECK_MS = 15s).
    // Recomputes sun state; if unchanged, returns immediately (O(1)).
    // If changed, iterates registered solar panels.
    // LFPG_SolarPanel_T2 inherits LFPG_SolarPanel → auto-registered via base.
    protected void LFPG_TickSolarPanels()
    {
        #ifdef SERVER
        bool prevSun = m_SolarHasSun;
        LFPG_ComputeSunState();

        // No transition → nothing to do. This is the common case
        // (dawn/dusk only happens twice per in-game day).
        if (m_SolarHasSun == prevSun)
            return;

        // Sun state changed — update all registered solar panels
        int total = m_RegisteredSolars.Count();
        if (total == 0)
            return;

        int i;
        int updated = 0;
        LFPG_SolarPanel panel;
        for (i = 0; i < total; i = i + 1)
        {
            if (i >= m_RegisteredSolars.Count())
                break;

            panel = m_RegisteredSolars[i];
            if (!panel)
                continue;

            panel.LFPG_UpdateSunState(m_SolarHasSun);
            updated = updated + 1;
        }

        string msg = "[Solar] Sun changed to ";
        msg = msg + m_SolarHasSun.ToString();
        msg = msg + ", updated ";
        msg = msg + updated.ToString();
        msg = msg + " panels";
        LFPG_Util.Info(msg);
        #endif
    }

    // ===========================
    // v1.1.0: Water Pump Timer
    // ===========================
    // v5.1: Instant pump↔sprinkler link refresh.
    // Called from NotifyGraphWireAdded/Removed so sprinkler state
    // updates immediately on wire connect/disconnect instead of
    // waiting up to 60s for the periodic tick.
    // removedTargetId: "" on add, actual targetId on remove.
    // ===========================
    void LFPG_RefreshPumpSprinklerLink(string sourceId, string removedTargetId)
    {
        #ifdef SERVER
        LFPG_DeviceRegistry reg = LFPG_DeviceRegistry.Get();
        EntityAI srcEnt = reg.FindById(sourceId);
        if (!srcEnt)
            return;

        // Check if source is a pump (T1 or T2)
        LFPG_WaterPump rp1 = LFPG_WaterPump.Cast(srcEnt);
        LFPG_WaterPump_T2 rp2 = LFPG_WaterPump_T2.Cast(srcEnt);
        if (!rp1 && !rp2)
            return;

        // Handle removed target: reset sprinkler that was just disconnected
        if (removedTargetId != "")
        {
            EntityAI removedEnt = reg.FindById(removedTargetId);
            if (removedEnt)
            {
                LFPG_Sprinkler removedSpr = LFPG_Sprinkler.Cast(removedEnt);
                if (removedSpr)
                {
                    string curSource = removedSpr.LFPG_GetWaterSourceId();
                    if (curSource == sourceId)
                    {
                        removedSpr.LFPG_SetHasWaterSource(false);
                        removedSpr.LFPG_SetSprinklerActive(false);
                        string emptyId = "";
                        removedSpr.LFPG_SetWaterSourceId(emptyId);
                    }
                }
            }
        }

        // Rescan pump's current wires for sprinkler connections
        array<ref LFPG_WireData> rpWires;
        bool rpPowered;
        float rpTank = 0.0;

        if (rp1)
        {
            rpWires = rp1.LFPG_GetWires();
            rpPowered = rp1.LFPG_GetPoweredNet();
        }
        else
        {
            rpWires = rp2.LFPG_GetWires();
            rpPowered = rp2.LFPG_GetPoweredNet();
            rpTank = rp2.LFPG_GetTankLevel();
        }

        int rpSprCount = 0;
        int rwi;
        int rpWireCount = rpWires.Count();
        LFPG_WireData rpWd;
        string rpTid;
        EntityAI rpTEnt;
        LFPG_Sprinkler rpTSpr;
        bool rpSprActive;

        // Pass 1: Count sprinklers + set water source + T1 activation
        for (rwi = 0; rwi < rpWireCount; rwi = rwi + 1)
        {
            rpWd = rpWires[rwi];
            if (!rpWd)
                continue;

            rpTid = rpWd.m_TargetDeviceId;
            if (rpTid == "")
                continue;

            // v5.1: Skip the sprinkler being deleted/disconnected
            if (rpTid == removedTargetId)
                continue;

            rpTEnt = reg.FindById(rpTid);
            if (!rpTEnt)
                continue;

            rpTSpr = LFPG_Sprinkler.Cast(rpTEnt);
            if (!rpTSpr)
                continue;

            rpSprCount = rpSprCount + 1;
            rpTSpr.LFPG_SetHasWaterSource(true);
            rpTSpr.LFPG_SetWaterSourceId(sourceId);

            // T1: activate immediately (no tank dependency)
            if (rp1)
            {
                rpTSpr.LFPG_SetSprinklerActive(rpPowered);
            }
        }

        // Update pump state
        bool rpHasSpr = false;
        if (rp1)
        {
            if (rpSprCount > 0)
            {
                rpHasSpr = true;
            }
            rp1.LFPG_SetHasSprinklerOutput(rpHasSpr);
        }
        else
        {
            rp2.LFPG_SetConnectedSprinklerCount(rpSprCount);

            // T2 Pass 2: Activate sprinklers based on final count.
            // 1-2 sprinklers: always active if powered (sustainable flow).
            // 3+: require tank > 0.
            rpSprActive = false;
            if (rpPowered)
            {
                if (rpSprCount <= 2)
                {
                    rpSprActive = true;
                }
                else if (rpTank > 0.0)
                {
                    rpSprActive = true;
                }
            }

            for (rwi = 0; rwi < rpWireCount; rwi = rwi + 1)
            {
                rpWd = rpWires[rwi];
                if (!rpWd)
                    continue;

                rpTid = rpWd.m_TargetDeviceId;
                if (rpTid == "")
                    continue;

                if (rpTid == removedTargetId)
                    continue;

                rpTEnt = reg.FindById(rpTid);
                if (!rpTEnt)
                    continue;

                rpTSpr = LFPG_Sprinkler.Cast(rpTEnt);
                if (!rpTSpr)
                    continue;

                rpTSpr.LFPG_SetSprinklerActive(rpSprActive);
            }
        }
        #endif
    }

    // ===========================
    // Two sub-systems:
    //   1. Filter degradation: real-time (ms), 1 qty point per LFPG_PUMP_FILTER_INTERVAL_MS (2%/h)
    //   2. Tank fill: in-game hour based, LFPG_PUMP_TANK_FILL_PER_HOUR per hour

    // Seed tank fill hour from world time
    protected void LFPG_InitTankFillTime()
    {
        #ifdef SERVER
        m_TankFillLastMs = GetGame().GetTime();
        #endif
    }

    // Periodic tick (every LFPG_PUMP_CHECK_MS = 60s)
    // v4.1: Uses dedicated registries instead of GetAll+Cast.
    protected void LFPG_TickWaterPumps()
    {
        #ifdef SERVER
        float nowMs = GetGame().GetTime();
        float thresholdMs = LFPG_PUMP_FILTER_INTERVAL_MS;

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

        // ============================================================
        // Phase A: Reset sprinklers + filter degradation (registries).
        // Replaces GetAll+Cast full scan with direct registry iteration.
        // ============================================================
        int i;
        int sprTotal = m_RegisteredSprinklers.Count();
        LFPG_Sprinkler castSpr;
        float elapsed;

        // Reset all registered sprinklers (Phase B/C re-activates if connected)
        for (i = 0; i < sprTotal; i = i + 1)
        {
            if (i >= m_RegisteredSprinklers.Count())
                break;

            castSpr = m_RegisteredSprinklers[i];
            if (!castSpr)
                continue;

            castSpr.LFPG_SetHasWaterSource(false);
            castSpr.LFPG_SetSprinklerActive(false);
        }

        // T1 filter degradation
        int t1Total = m_RegisteredT1Pumps.Count();
        LFPG_WaterPump castT1;
        for (i = 0; i < t1Total; i = i + 1)
        {
            if (i >= m_RegisteredT1Pumps.Count())
                break;

            castT1 = m_RegisteredT1Pumps[i];
            if (!castT1)
                continue;

            elapsed = nowMs - castT1.LFPG_GetFilterLastMs();
            if (elapsed >= thresholdMs)
            {
                castT1.LFPG_DegradeFilter();
                castT1.LFPG_SetFilterLastMs(nowMs);
            }
        }

        // T2 filter degradation
        int t2Total = m_RegisteredT2Pumps.Count();
        LFPG_WaterPump_T2 castT2;
        for (i = 0; i < t2Total; i = i + 1)
        {
            if (i >= m_RegisteredT2Pumps.Count())
                break;

            castT2 = m_RegisteredT2Pumps[i];
            if (!castT2)
                continue;

            elapsed = nowMs - castT2.LFPG_GetFilterLastMs();
            if (elapsed >= thresholdMs)
            {
                castT2.LFPG_DegradeFilter();
                castT2.LFPG_SetFilterLastMs(nowMs);
            }
        }

        // ============================================================
        // Phase B: T1 pumps — wire scan → activate connected sprinklers,
        //          set m_HasSprinklerOutput.
        // ============================================================
        LFPG_DeviceRegistry reg = LFPG_DeviceRegistry.Get();
        int t1Count = m_RegisteredT1Pumps.Count();
        int pi;
        int wi;
        int wireCount;
        int sprCount;
        LFPG_WaterPump curT1;
        array<ref LFPG_WireData> wires;
        LFPG_WireData wd;
        string targetId;
        EntityAI targetEnt;
        LFPG_Sprinkler targetSpr;
        bool pumpPowered;
        string pumpId;
        bool hasSprOut;

        for (pi = 0; pi < t1Count; pi = pi + 1)
        {
            if (pi >= m_RegisteredT1Pumps.Count())
                break;

            curT1 = m_RegisteredT1Pumps[pi];
            if (!curT1)
                continue;

            wires = curT1.LFPG_GetWires();
            wireCount = wires.Count();
            sprCount = 0;
            pumpPowered = curT1.LFPG_GetPoweredNet();
            pumpId = curT1.LFPG_GetDeviceId();

            for (wi = 0; wi < wireCount; wi = wi + 1)
            {
                wd = wires[wi];
                if (!wd)
                    continue;

                targetId = wd.m_TargetDeviceId;
                if (targetId == "")
                    continue;

                targetEnt = reg.FindById(targetId);
                if (!targetEnt)
                    continue;

                targetSpr = LFPG_Sprinkler.Cast(targetEnt);
                if (!targetSpr)
                    continue;

                // Sprinkler found on this T1 output
                sprCount = sprCount + 1;
                targetSpr.LFPG_SetHasWaterSource(true);
                targetSpr.LFPG_SetWaterSourceId(pumpId);
                targetSpr.LFPG_SetSprinklerActive(pumpPowered);
            }

            hasSprOut = false;
            if (sprCount > 0)
            {
                hasSprOut = true;
            }
            curT1.LFPG_SetHasSprinklerOutput(hasSprOut);
        }

        // ============================================================
        // Phase C: T2 pumps — wire scan → activate connected sprinklers,
        //          set m_ConnectedSprinklerCount, adjusted tank fill.
        // ============================================================
        int t2Count = m_RegisteredT2Pumps.Count();
        LFPG_WaterPump_T2 curT2B;
        bool sprActive;
        float curTank;
        float level;
        float sprDrainFactor;
        float netFactor;
        float netFill;
        int incomingType;
        int currentType;

        for (pi = 0; pi < t2Count; pi = pi + 1)
        {
            if (pi >= m_RegisteredT2Pumps.Count())
                break;

            curT2B = m_RegisteredT2Pumps[pi];
            if (!curT2B)
                continue;

            wires = curT2B.LFPG_GetWires();
            wireCount = wires.Count();
            sprCount = 0;
            pumpPowered = curT2B.LFPG_GetPoweredNet();
            pumpId = curT2B.LFPG_GetDeviceId();
            curTank = curT2B.LFPG_GetTankLevel();

            // Pass 1: Count sprinklers + set water source (no activation yet)
            for (wi = 0; wi < wireCount; wi = wi + 1)
            {
                wd = wires[wi];
                if (!wd)
                    continue;

                targetId = wd.m_TargetDeviceId;
                if (targetId == "")
                    continue;

                targetEnt = reg.FindById(targetId);
                if (!targetEnt)
                    continue;

                targetSpr = LFPG_Sprinkler.Cast(targetEnt);
                if (!targetSpr)
                    continue;

                sprCount = sprCount + 1;
                targetSpr.LFPG_SetHasWaterSource(true);
                targetSpr.LFPG_SetWaterSourceId(pumpId);
            }

            curT2B.LFPG_SetConnectedSprinklerCount(sprCount);

            // Determine activation: 1-2 sprinklers always work if powered
            // (net flow >= 0, system is sustainable). 3+ require tank > 0.
            sprActive = false;
            if (pumpPowered)
            {
                if (sprCount <= 2)
                {
                    sprActive = true;
                }
                else if (curTank > 0.0)
                {
                    sprActive = true;
                }
            }

            // Pass 2: Activate sprinklers with final decision
            for (wi = 0; wi < wireCount; wi = wi + 1)
            {
                wd = wires[wi];
                if (!wd)
                    continue;

                targetId = wd.m_TargetDeviceId;
                if (targetId == "")
                    continue;

                targetEnt = reg.FindById(targetId);
                if (!targetEnt)
                    continue;

                targetSpr = LFPG_Sprinkler.Cast(targetEnt);
                if (!targetSpr)
                    continue;

                targetSpr.LFPG_SetSprinklerActive(sprActive);
            }

            // --- T2 tank fill with sprinkler drain adjustment ---
            if (doTankFill && pumpPowered)
            {
                level = curT2B.LFPG_GetTankLevel();

                // netFill = fillAmount * (1.0 - sprCount * 0.5)
                // 0 spr → +fill, 1 → +0.5*fill, 2 → 0, 3 → -0.5*fill
                sprDrainFactor = sprCount * 0.5;
                netFactor = 1.0 - sprDrainFactor;
                netFill = fillAmount * netFactor;

                level = level + netFill;

                // Clamp to [0, max]
                if (level < 0.0)
                {
                    level = 0.0;
                }
                if (level > LFPG_PUMP_TANK_MAX)
                {
                    level = LFPG_PUMP_TANK_MAX;
                }

                // Determine incoming water type (only when net positive)
                if (netFill > 0.0)
                {
                    incomingType = LIQUID_RIVERWATER;
                    if (LFPG_PumpHelper.HasActiveFilter(curT2B))
                    {
                        incomingType = LIQUID_CLEANWATER;
                    }

                    currentType = curT2B.LFPG_GetTankLiquidType();

                    if (level < 0.01)
                    {
                        curT2B.LFPG_SetTankLiquidType(incomingType);
                    }
                    else if (incomingType != currentType)
                    {
                        curT2B.LFPG_SetTankLiquidType(LIQUID_RIVERWATER);
                    }
                }

                curT2B.LFPG_SetTankLevel(level);
            }
        }
        #endif
    }

    // ===========================
    // v4.1: Solar Panel Registry
    // ===========================
    // Replaces GetAll+Cast full scan in TickSolarPanels.
    // LFPG_SolarPanel_T2 inherits LFPG_SolarPanel → registered via base class.

    void RegisterSolar(LFPG_SolarPanel panel)
    {
        if (!panel)
            return;
        if (m_RegisteredSolars.Find(panel) < 0)
        {
            m_RegisteredSolars.Insert(panel);
        }
    }

    void UnregisterSolar(LFPG_SolarPanel panel)
    {
        if (!panel)
            return;
        int idx = m_RegisteredSolars.Find(panel);
        if (idx >= 0)
        {
            m_RegisteredSolars.Remove(idx);
        }
    }

    // ===========================
    // v4.1: Water Pump + Sprinkler Registries
    // ===========================
    // Replaces GetAll+Cast full scan in TickWaterPumps Phase A.
    // T1 and T2 are separate classes (T2 does NOT inherit T1).

    void RegisterT1Pump(LFPG_WaterPump pump)
    {
        if (!pump)
            return;
        if (m_RegisteredT1Pumps.Find(pump) < 0)
        {
            m_RegisteredT1Pumps.Insert(pump);
        }
    }

    void UnregisterT1Pump(LFPG_WaterPump pump)
    {
        if (!pump)
            return;
        int idx = m_RegisteredT1Pumps.Find(pump);
        if (idx >= 0)
        {
            m_RegisteredT1Pumps.Remove(idx);
        }
    }

    void RegisterT2Pump(LFPG_WaterPump_T2 pump)
    {
        if (!pump)
            return;
        if (m_RegisteredT2Pumps.Find(pump) < 0)
        {
            m_RegisteredT2Pumps.Insert(pump);
        }
    }

    void UnregisterT2Pump(LFPG_WaterPump_T2 pump)
    {
        if (!pump)
            return;
        int idx = m_RegisteredT2Pumps.Find(pump);
        if (idx >= 0)
        {
            m_RegisteredT2Pumps.Remove(idx);
        }
    }

    void RegisterSprinkler(LFPG_Sprinkler spr)
    {
        if (!spr)
            return;
        if (m_RegisteredSprinklers.Find(spr) < 0)
        {
            m_RegisteredSprinklers.Insert(spr);
        }
    }

    void UnregisterSprinkler(LFPG_Sprinkler spr)
    {
        if (!spr)
            return;
        int idx = m_RegisteredSprinklers.Find(spr);
        if (idx >= 0)
        {
            m_RegisteredSprinklers.Remove(idx);
        }
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
    // Called from LFPG_Sorter.EEInit / EEDelete / EEKilled.
    void RegisterSorter(LFPG_Sorter sorter)
    {
        if (!sorter)
            return;
        if (m_RegisteredSorters.Find(sorter) < 0)
        {
            m_RegisteredSorters.Insert(sorter);
        }
    }

    void UnregisterSorter(LFPG_Sorter sorter)
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
        LFPG_Sorter sorter;
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

            // v2.4 Bug B: Auto-unlink if container moved beyond link radius
            inputContainer = sorter.LFPG_GetLinkedContainer();
            if (inputContainer)
            {
                float linkDist = vector.Distance(sorter.GetPosition(), inputContainer.GetPosition());
                if (linkDist > LFPG_SORTER_LINK_RADIUS)
                {
                    sorter.LFPG_UnlinkContainer();
                    string unlinkMsg = "[Sorter] Auto-unlink: container beyond ";
                    unlinkMsg = unlinkMsg + LFPG_SORTER_LINK_RADIUS.ToString();
                    unlinkMsg = unlinkMsg + "m (was ";
                    unlinkMsg = unlinkMsg + linkDist.ToString();
                    unlinkMsg = unlinkMsg + "m)";
                    LFPG_Util.Info(unlinkMsg);
                    continue;
                }
            }

            // Must have linked container
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
                    // Dirty dest so client refreshes cargo view.
                    // SetSynchDirty is idempotent — safe to call per-item.
                    destContainer.SetSynchDirty();
                }
            }

            // Dirty source container if any items left it
            if (moved > 0)
            {
                inputContainer.SetSynchDirty();
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

    // v3.2: Returns moved count (-1 = error/skip, 0+ = items moved).
    // Caller (PlayerRPC) sends SORT_ACK with the result.
    int HandleSorterRequestSort(LFPG_Sorter sorter)
    {
        #ifdef SERVER
        if (!sorter)
        {
            string w0 = "[Sorter] REQUEST_SORT: null sorter";
            LFPG_Util.Warn(w0);
            return -1;
        }

        // Must be powered
        if (!sorter.LFPG_IsPowered())
        {
            string d0 = "[Sorter] REQUEST_SORT: sorter not powered";
            LFPG_Util.Debug(d0);
            return -1;
        }

        // Resolve source container
        EntityAI container = sorter.LFPG_GetLinkedContainer();
        if (!container)
        {
            string w1 = "[Sorter] REQUEST_SORT: no linked container";
            LFPG_Util.Warn(w1);
            return -1;
        }

        // S3.1: Source container must be accessible
        if (!LFPG_SorterLogic.CanTakeFromContainer(container, null))
        {
            string w2 = "[Sorter] REQUEST_SORT: container not accessible";
            LFPG_Util.Warn(w2);
            return -1;
        }

        // Must have filter config
        LFPG_SortConfig filterConfig = sorter.LFPG_GetFilterConfig();
        if (!filterConfig)
        {
            string w3 = "[Sorter] REQUEST_SORT: no filter config";
            LFPG_Util.Warn(w3);
            return -1;
        }

        // Must have cargo
        if (!container.GetInventory())
        {
            string w4 = "[Sorter] REQUEST_SORT: no inventory";
            LFPG_Util.Warn(w4);
            return -1;
        }
        CargoBase srcCargo = container.GetInventory().GetCargo();
        if (!srcCargo)
        {
            string w5 = "[Sorter] REQUEST_SORT: no cargo";
            LFPG_Util.Warn(w5);
            return -1;
        }

        int itemCount = srcCargo.GetItemCount();
        if (itemCount <= 0)
        {
            string d1 = "[Sorter] REQUEST_SORT: cargo empty";
            LFPG_Util.Debug(d1);
            return 0;
        }

        // Build wire mask — which outputs have wires connected
        int hasWireMask = 0;
        int portBit = 1;
        int pi = 0;
        EntityAI wireCheck = null;
        for (pi = 0; pi < 6; pi = pi + 1)
        {
            wireCheck = LFPG_SorterLogic.ResolveOutputContainer(sorter, pi);
            if (wireCheck)
            {
                hasWireMask = hasWireMask | portBit;
            }
            portBit = portBit * 2;
        }

        if (hasWireMask == 0)
        {
            string d2 = "[Sorter] REQUEST_SORT: no wired outputs";
            LFPG_Util.Debug(d2);
            // Still do BinPack even without outputs
            int bpOnly = LFPG_SorterLogic.BinPackCargo(container);
            string bpLog = "[Sorter] BinPack only (no wires): ";
            bpLog = bpLog + bpOnly.ToString();
            LFPG_Util.Info(bpLog);
            return 0;
        }

        // Collect items into cache (index mutation safe)
        array<EntityAI> sortCache = new array<EntityAI>;
        int ci = 0;
        EntityAI sortItem = null;
        for (ci = 0; ci < itemCount; ci = ci + 1)
        {
            sortItem = srcCargo.GetItem(ci);
            if (sortItem)
            {
                sortCache.Insert(sortItem);
            }
        }

        // Sort pass: evaluate all items, move matched ones
        int moved = 0;
        int evaluated = 0;
        int maxEval = 200;
        int outputIdx = 0;
        EntityAI destContainer = null;
        bool moveResult = false;

        // Collect unique dest containers to dirty after sort
        array<EntityAI> dirtiedDests = new array<EntityAI>;

        for (ci = 0; ci < sortCache.Count(); ci = ci + 1)
        {
            if (evaluated >= maxEval)
                break;

            sortItem = sortCache[ci];
            if (!sortItem)
                continue;

            evaluated = evaluated + 1;

            // Per-item source release check
            if (!container.CanReleaseCargo(sortItem))
                continue;

            // Evaluate filter rules
            outputIdx = LFPG_SorterLogic.EvaluateItem(sortItem, filterConfig, hasWireMask);
            if (outputIdx < 0)
                continue;

            // Resolve destination via wire topology
            destContainer = LFPG_SorterLogic.ResolveOutputContainer(sorter, outputIdx);
            if (!destContainer)
                continue;

            // Skip self
            if (destContainer == container)
                continue;

            // Move item
            moveResult = LFPG_SorterLogic.MoveItemToContainer(sortItem, destContainer);
            if (moveResult)
            {
                moved = moved + 1;

                // Track dest for batch dirty (skip duplicates)
                if (dirtiedDests.Find(destContainer) < 0)
                {
                    dirtiedDests.Insert(destContainer);
                }
            }
        }

        // Force network sync on all affected destination containers
        // so clients refresh their cached cargo views.
        int di = 0;
        for (di = 0; di < dirtiedDests.Count(); di = di + 1)
        {
            dirtiedDests[di].SetSynchDirty();
        }

        // Source container is dirtied by BinPackCargo below (Phase 5d).

        string sortLog = "[Sorter] REQUEST_SORT: evaluated=";
        sortLog = sortLog + evaluated.ToString();
        sortLog = sortLog + " moved=";
        sortLog = sortLog + moved.ToString();
        LFPG_Util.Info(sortLog);

        // Bin-pack remaining items in source container
        int packed = LFPG_SorterLogic.BinPackCargo(container);
        string packLog = "[Sorter] BinPack after sort: repositioned=";
        packLog = packLog + packed.ToString();
        LFPG_Util.Info(packLog);
        return moved;
        #endif
        return -1;
    }

    // ===========================
    // v1.5.0: Motion Sensor Registration
    // v1.8.0: Pressure Pad Registration
    // ===========================

    void RegisterMotionSensor(LFPG_MotionSensor sensor)
    {
        if (!sensor)
            return;
        if (m_RegisteredSensors.Find(sensor) < 0)
        {
            m_RegisteredSensors.Insert(sensor);
        }
    }

    void UnregisterMotionSensor(LFPG_MotionSensor sensor)
    {
        if (!sensor)
            return;
        int idx = m_RegisteredSensors.Find(sensor);
        if (idx >= 0)
        {
            m_RegisteredSensors.Remove(idx);
        }
    }

    void RegisterPressurePad(LFPG_PressurePad pad)
    {
        if (!pad)
            return;
        if (m_RegisteredPads.Find(pad) < 0)
        {
            m_RegisteredPads.Insert(pad);
        }
    }

    void UnregisterPressurePad(LFPG_PressurePad pad)
    {
        if (!pad)
            return;
        int idx = m_RegisteredPads.Find(pad);
        if (idx >= 0)
        {
            m_RegisteredPads.Remove(idx);
        }
    }

    // v1.9.0: Laser Detector Registration
    void RegisterLaserDetector(LFPG_LaserDetector laser)
    {
        if (!laser)
            return;
        if (m_RegisteredLasers.Find(laser) < 0)
        {
            m_RegisteredLasers.Insert(laser);
        }
    }

    void UnregisterLaserDetector(LFPG_LaserDetector laser)
    {
        if (!laser)
            return;
        int idx = m_RegisteredLasers.Find(laser);
        if (idx >= 0)
        {
            m_RegisteredLasers.Remove(idx);
        }
    }

    // v3.0: Intercom Registration (for toggle input evaluation)
    void RegisterIntercom(LFPG_Intercom ic)
    {
        if (!ic)
            return;
        if (m_RegisteredIntercoms.Find(ic) < 0)
        {
            m_RegisteredIntercoms.Insert(ic);
        }
    }

    void UnregisterIntercom(LFPG_Intercom ic)
    {
        if (!ic)
            return;
        int idx = m_RegisteredIntercoms.Find(ic);
        if (idx >= 0)
        {
            m_RegisteredIntercoms.Remove(idx);
        }
    }

    // ===========================
    // v3.1: Furnace Registration
    // ===========================
    // Tick absorbed into LFPG_TickSimpleDevices (offset 2, ~5s effective).
    // Only active furnaces (m_SourceOn) are registered.

    void RegisterFurnace(LFPG_Furnace furnace)
    {
        if (!furnace)
            return;
        if (m_RegisteredFurnaces.Find(furnace) < 0)
        {
            m_RegisteredFurnaces.Insert(furnace);
        }
    }

    void UnregisterFurnace(LFPG_Furnace furnace)
    {
        if (!furnace)
            return;
        int idx = m_RegisteredFurnaces.Find(furnace);
        if (idx >= 0)
        {
            m_RegisteredFurnaces.Remove(idx);
        }
    }

    // ===========================
    // v4.0: Fridge Registration
    // ===========================
    // Tick absorbed into LFPG_TickSimpleDevices (offset 6, ~10s effective).

    void RegisterFridge(LFPG_Fridge fridge)
    {
        if (!fridge)
            return;
        if (m_RegisteredFridges.Find(fridge) < 0)
        {
            m_RegisteredFridges.Insert(fridge);
        }
    }

    void UnregisterFridge(LFPG_Fridge fridge)
    {
        if (!fridge)
            return;
        int idx = m_RegisteredFridges.Find(fridge);
        if (idx >= 0)
        {
            m_RegisteredFridges.Remove(idx);
        }
    }

    // ===========================
    // v1.0.0: Electric Stove Registration
    // ===========================
    // Tick absorbed into LFPG_TickSimpleDevices (offset 1, every 3rd tick = ~3s).

    void RegisterStove(LFPG_ElectricStove stove)
    {
        if (!stove)
            return;
        if (m_RegisteredStoves.Find(stove) < 0)
        {
            m_RegisteredStoves.Insert(stove);
        }
    }

    void UnregisterStove(LFPG_ElectricStove stove)
    {
        if (!stove)
            return;
        int idx = m_RegisteredStoves.Find(stove);
        if (idx >= 0)
        {
            m_RegisteredStoves.Remove(idx);
        }
    }

    // ===========================
    // v4.0: DoorController Registration
    // ===========================
    // Tick absorbed into LFPG_TickSimpleDevices (offset 1, ~2s effective).

    void RegisterDoorController(LFPG_DoorController dc)
    {
        if (!dc)
            return;
        if (m_RegisteredDoorControllers.Find(dc) < 0)
        {
            m_RegisteredDoorControllers.Insert(dc);
        }
    }

    void UnregisterDoorController(LFPG_DoorController dc)
    {
        if (!dc)
            return;
        int idx = m_RegisteredDoorControllers.Find(dc);
        if (idx >= 0)
        {
            m_RegisteredDoorControllers.Remove(idx);
        }
    }

    // ===========================
    // v4.1: Consolidated Simple Devices Tick
    // ===========================
    // Single 1,000ms timer drives 6 device subsystems via staggered sub-counters.
    // Cycle 1→10, reset at 10. Stagger ensures Batteries and Furnaces never fire same tick.
    //   - Intercoms:       every tick       (1,000ms)  — ticks 1,2,3,4,5,6,7,8,9,10
    //   - DoorControllers: counter % 2 == 1 (2,000ms)  — ticks 1,3,5,7,9
    //   - Furnaces:        counter % 5 == 2 (5,000ms)  — ticks 2,7
    //   - Batteries:       counter % 5 == 4 (5,000ms)  — ticks 4,9
    //   - Fridges:         counter % 10 == 6 (10,000ms) — tick 6
    //   - ElectricStoves:  counter % 3 == 1 (3,000ms)  — ticks 1,4,7
    // OPT-2: Early-out when all registries empty.
    protected void LFPG_TickSimpleDevices()
    {
        #ifdef SERVER
        int totalIc = m_RegisteredIntercoms.Count();
        int totalDc = m_RegisteredDoorControllers.Count();
        int totalFur = m_RegisteredFurnaces.Count();
        int totalBat = m_RegisteredBatteries.Count();
        int totalFri = m_RegisteredFridges.Count();
        int totalStv = m_RegisteredStoves.Count();
        int totalSimple = totalIc + totalDc + totalFur + totalBat + totalFri + totalStv;
        if (totalSimple == 0)
            return;

        m_SimpleTickCounter = m_SimpleTickCounter + 1;
        if (m_SimpleTickCounter >= 10)
        {
            m_SimpleTickCounter = 0;
        }

        // --- Intercoms: every tick (1s) ---
        if (totalIc > 0)
        {
            int ii;
            LFPG_Intercom ic;

            for (ii = 0; ii < totalIc; ii = ii + 1)
            {
                if (ii >= m_RegisteredIntercoms.Count())
                    break;

                ic = m_RegisteredIntercoms[ii];
                if (!ic)
                    continue;

                ic.LFPG_EvaluateToggleInput();
            }
        }

        // --- DoorControllers: every 2nd tick (2s) ---
        int dcMod = m_SimpleTickCounter % 2;
        if (dcMod == 1 && totalDc > 0)
        {
            int di;
            LFPG_DoorController dc;

            for (di = 0; di < totalDc; di = di + 1)
            {
                if (di >= m_RegisteredDoorControllers.Count())
                    break;

                dc = m_RegisteredDoorControllers[di];
                if (!dc)
                    continue;

                dc.LFPG_OnDoorPoll();
            }
        }

        // --- Furnaces: every 5th tick, offset 2 (ticks 2,7) ---
        int furMod = m_SimpleTickCounter % 5;
        if (furMod == 2 && totalFur > 0)
        {
            int fi;
            LFPG_Furnace furnace;

            for (fi = 0; fi < totalFur; fi = fi + 1)
            {
                if (fi >= m_RegisteredFurnaces.Count())
                    break;

                furnace = m_RegisteredFurnaces[fi];
                if (!furnace)
                    continue;

                furnace.LFPG_BurnTick();
            }
        }

        // --- Batteries: every 5th tick, offset 4 (ticks 4,9) — never overlaps Furnaces ---
        int batMod = m_SimpleTickCounter % 5;
        if (batMod == 4 && totalBat > 0)
        {
            LFPG_TickBatteriesInternal();
        }

        // --- Fridges: every 10th tick, offset 6 (tick 6) ---
        if (m_SimpleTickCounter == 6 && totalFri > 0)
        {
            int ri;
            LFPG_Fridge fridge;

            for (ri = 0; ri < totalFri; ri = ri + 1)
            {
                if (ri >= m_RegisteredFridges.Count())
                    break;

                fridge = m_RegisteredFridges[ri];
                if (!fridge)
                    continue;

                fridge.LFPG_OnCoolTick();
            }
        }

        // --- Electric Stoves: every 3rd tick, offset 1 (ticks 1,4,7) = ~3s ---
        // NOTE: offset 0 is avoided because counter wraps 9→0 creating a 1s gap.
        int stoveMod = m_SimpleTickCounter % 3;
        if (stoveMod == 1 && totalStv > 0)
        {
            int si;
            LFPG_ElectricStove stove;
            float stoveDelta = 3.0;

            for (si = 0; si < totalStv; si = si + 1)
            {
                if (si >= m_RegisteredStoves.Count())
                    break;

                stove = m_RegisteredStoves[si];
                if (!stove)
                    continue;

                stove.LFPG_TickCooking(stoveDelta);
            }
        }
        #endif
    }

    // ===========================
    // v4.1: Consolidated Player Detection Tick
    // ===========================
    // Single 300ms timer drives three detection subsystems via sub-counters:
    //   - Lasers: crossing every tick (300ms), beam raycast every 23 ticks (~6.9s)
    //   - Pads: every 2nd tick (600ms)
    //   - Sensors: every 10th tick (3,000ms)
    // One GetPlayers call shared by all. OPT-1: early-out when no detection devices.
    // Beam fires BEFORE crossing in same iteration for immediate m_BeamLength use.
    protected void LFPG_TickPlayerDetection()
    {
        #ifdef SERVER
        int totalLasers = m_RegisteredLasers.Count();
        int totalPads = m_RegisteredPads.Count();
        int totalSensors = m_RegisteredSensors.Count();
        int totalDetect = totalLasers + totalPads + totalSensors;
        if (totalDetect == 0)
            return;

        m_PlayerDetectCounter = m_PlayerDetectCounter + 1;
        m_LaserBeamCounter = m_LaserBeamCounter + 1;

        // Pre-compute which subsystems fire this tick to avoid unnecessary GetPlayers.
        // Lasers: crossing every tick needs players.
        // Pads: every 2nd tick. Sensors: every 10th tick.
        int padMod = m_PlayerDetectCounter % 2;
        bool needPlayers = false;
        if (totalLasers > 0)
        {
            needPlayers = true;
        }
        if (padMod == 0 && totalPads > 0)
        {
            needPlayers = true;
        }
        if (m_PlayerDetectCounter >= 10 && totalSensors > 0)
        {
            needPlayers = true;
        }

        if (needPlayers)
        {
            m_ReusablePlayers.Clear();
            GetGame().GetPlayers(m_ReusablePlayers);
        }

        // --- Lasers: crossing every tick, beam every 23 ticks (~6.9s) ---
        if (totalLasers > 0)
        {
            bool doBeam = false;
            if (m_LaserBeamCounter >= 23)
            {
                m_LaserBeamCounter = 0;
                doBeam = true;
            }

            int li;
            int laserChanged = 0;
            LFPG_LaserDetector laser;
            bool lcChanged;
            string laserId;

            for (li = 0; li < totalLasers; li = li + 1)
            {
                if (li >= m_RegisteredLasers.Count())
                    break;

                laser = m_RegisteredLasers[li];
                if (!laser)
                    continue;

                if (doBeam)
                {
                    laser.LFPG_UpdateBeamRaycast();
                }

                lcChanged = laser.LFPG_EvaluateCrossing(m_ReusablePlayers);
                if (lcChanged)
                {
                    laserId = laser.LFPG_GetDeviceId();
                    if (laserId != "")
                    {
                        RequestPropagate(laserId);
                    }
                    laserChanged = laserChanged + 1;
                }
            }

            if (laserChanged > 0)
            {
                string laserMsg = "[PlayerDetect] Lasers: ";
                laserMsg = laserMsg + laserChanged.ToString();
                laserMsg = laserMsg + " changed state";
                LFPG_Util.Info(laserMsg);
            }
        }

        // --- Pads: every 2nd tick (600ms) ---
        if (padMod == 0 && totalPads > 0)
        {
            int pi;
            int padChanged = 0;
            LFPG_PressurePad pad;
            bool pcChanged;
            string padId;

            for (pi = 0; pi < totalPads; pi = pi + 1)
            {
                if (pi >= m_RegisteredPads.Count())
                    break;

                pad = m_RegisteredPads[pi];
                if (!pad)
                    continue;

                pcChanged = pad.LFPG_EvaluatePresence(m_ReusablePlayers);
                if (pcChanged)
                {
                    padId = pad.LFPG_GetDeviceId();
                    if (padId != "")
                    {
                        RequestPropagate(padId);
                    }
                    padChanged = padChanged + 1;
                }
            }

            if (padChanged > 0)
            {
                string padMsg = "[PlayerDetect] Pads: ";
                padMsg = padMsg + padChanged.ToString();
                padMsg = padMsg + " changed state";
                LFPG_Util.Info(padMsg);
            }
        }

        // --- Sensors: every 10th tick (3,000ms) ---
        if (m_PlayerDetectCounter >= 10)
        {
            m_PlayerDetectCounter = 0;

            if (totalSensors > 0)
            {
                int si;
                int sensorChanged = 0;
                LFPG_MotionSensor sensor;
                bool scChanged;
                string sensorId;

                for (si = 0; si < totalSensors; si = si + 1)
                {
                    if (si >= m_RegisteredSensors.Count())
                        break;

                    sensor = m_RegisteredSensors[si];
                    if (!sensor)
                        continue;

                    scChanged = sensor.LFPG_EvaluateDetection(m_ReusablePlayers);
                    if (scChanged)
                    {
                        sensorId = sensor.LFPG_GetDeviceId();
                        if (sensorId != "")
                        {
                            RequestPropagate(sensorId);
                        }
                        sensorChanged = sensorChanged + 1;
                    }
                }

                if (sensorChanged > 0)
                {
                    string sensorMsg = "[PlayerDetect] Sensors: ";
                    sensorMsg = sensorMsg + sensorChanged.ToString();
                    sensorMsg = sensorMsg + " changed state";
                    LFPG_Util.Info(sensorMsg);
                }
            }
        }
        #endif
    }

    // ===========================
    // v2.0: Battery Registration + Energy Accounting
    // ===========================
    // Pattern: identical to Sensor/Laser registration.
    // EntityAI typed — LF_Battery methods resolved via dynamic dispatch.
    // v4.1: Timer absorbed into LFPG_TickSimpleDevices (offset 4, ~5s effective).
    //
    // Energy accounting per battery per tick:
    //   1. Read node.m_InputPower (actual received from upstream)
    //   2. Sum outgoing edge allocations (actual delivered downstream)
    //   3. netFlow = received - delivered (+ = charging, - = discharging)
    //   4. Apply efficiency (charge only), self-discharge, health cap
    //   5. Hysteresis: toggle m_DischargeEnabled at 1%/5% thresholds
    //   6. Recompute m_VirtualGeneration + m_SoftDemand on graph node
    //   7. MarkNodeDirty if changed significantly

    void RegisterBattery(EntityAI battery)
    {
        if (!battery)
            return;
        if (m_RegisteredBatteries.Find(battery) < 0)
        {
            m_RegisteredBatteries.Insert(battery);
        }
    }

    void UnregisterBattery(EntityAI battery)
    {
        if (!battery)
            return;
        int idx = m_RegisteredBatteries.Find(battery);
        if (idx >= 0)
        {
            m_RegisteredBatteries.Remove(idx);
        }
    }

    // v4.1: Battery energy accounting (called from LFPG_TickSimpleDevices, offset 4).
    // Uses real delta time via m_BatteryLastTickMs. ~5s effective interval.
    protected void LFPG_TickBatteriesInternal()
    {
        #ifdef SERVER
        int batCount = m_RegisteredBatteries.Count();
        if (batCount <= 0)
            return;

        if (!m_Graph)
            return;

        // Real delta time (prevents drift on laggy servers).
        // GetGame().GetTime() returns milliseconds (same as water pump / tank timers).
        float nowMs = GetGame().GetTime();
        float deltaMs = nowMs - m_BatteryLastTickMs;
        m_BatteryLastTickMs = nowMs;

        // Guard: skip if delta is nonsensical (first tick, time travel, etc.)
        if (deltaMs < 100.0)
            return;
        if (deltaMs > 30000.0)
        {
            deltaMs = 30000.0;
        }

        float deltaSec = deltaMs / 1000.0;

        // Enforce Script: hoist all string function names BEFORE the loop.
        // Avoids 9 string allocations per battery per tick.
        string fnGetStored = "LFPG_GetStoredEnergy";
        string fnGetMaxStored = "LFPG_GetMaxStoredEnergy";
        string fnGetMaxCharge = "LFPG_GetMaxChargeRate";
        string fnGetMaxDisch = "LFPG_GetMaxDischargeRate";
        string fnGetEfficiency = "LFPG_GetEfficiency";
        string fnGetSelfDisch = "LFPG_GetSelfDischargeRate";
        string fnIsDischEnabled = "LFPG_IsDischargeEnabled";
        string fnIsOutputEnabled = "LFPG_IsOutputEnabled";
        string fnSetStored = "LFPG_SetStoredEnergy";
        string fnSetDisch = "LFPG_SetDischargeEnabled";
        string fnSetChargeRate = "LFPG_SetChargeRateCurrent";
        string hpZone = "";
        string hpPart = "";

        // Iterate all registered batteries.
        int bi;
        int dirtyCount = 0;
        for (bi = 0; bi < batCount; bi = bi + 1)
        {
            EntityAI batEnt = m_RegisteredBatteries[bi];
            if (!batEnt)
                continue;

            // Get deviceId via dynamic dispatch.
            string batId = LFPG_DeviceAPI.GetDeviceId(batEnt);
            if (batId == "")
                continue;

            // Get graph node. Battery must be wired to have a node.
            ref LFPG_ElecNode node = m_Graph.GetNode(batId);
            if (!node)
                continue;

            // --- Read battery entity state via dynamic dispatch ---
            // Uses GameScript.CallFunctionParams directly (DeviceAPI.CallFloat is protected).
            // String function names hoisted before loop (GC reduction).
            float storedEnergy = 0.0;
            float maxStored = 0.0;
            float maxCharge = 0.0;
            float maxDischarge = 0.0;
            float efficiency = 1.0;
            float selfDischargeRate = 0.0;
            bool dischargeEnabled = true;
            bool outputEnabled = true;

            GetGame().GameScript.CallFunctionParams(batEnt, fnGetStored, storedEnergy, null);
            GetGame().GameScript.CallFunctionParams(batEnt, fnGetMaxStored, maxStored, null);
            GetGame().GameScript.CallFunctionParams(batEnt, fnGetMaxCharge, maxCharge, null);
            GetGame().GameScript.CallFunctionParams(batEnt, fnGetMaxDisch, maxDischarge, null);
            GetGame().GameScript.CallFunctionParams(batEnt, fnGetEfficiency, efficiency, null);
            GetGame().GameScript.CallFunctionParams(batEnt, fnGetSelfDisch, selfDischargeRate, null);
            GetGame().GameScript.CallFunctionParams(batEnt, fnIsDischEnabled, dischargeEnabled, null);
            GetGame().GameScript.CallFunctionParams(batEnt, fnIsOutputEnabled, outputEnabled, null);

            // Skip if not a real battery (maxStored = 0 means entity doesn't implement battery API).
            if (maxStored < LFPG_PROPAGATION_EPSILON)
                continue;

            // --- Health-based capacity reduction ---
            float healthRatio = 1.0;
            float maxHP = batEnt.GetMaxHealth(hpZone, hpPart);
            if (maxHP > 0.1)
            {
                float curHP = batEnt.GetHealth(hpZone, hpPart);
                healthRatio = curHP / maxHP;
                if (healthRatio < 0.0)
                {
                    healthRatio = 0.0;
                }
                if (healthRatio > 1.0)
                {
                    healthRatio = 1.0;
                }
            }
            float effectiveMax = maxStored * healthRatio;

            // --- Compute net energy flow ---
            // Input: what the battery actually received from upstream.
            float inputReceived = node.m_InputPower;

            // Output: sum of allocated power on outgoing edges (actual downstream delivery).
            float outputDelivered = m_Graph.SumOutgoingAllocations(batId);

            // Net flow: positive = surplus (charge), negative = deficit (discharge).
            // Subtract selfConsumption: input that was consumed by the battery device
            // itself (e.g. monitoring circuits). For current tiers consumption=0,
            // but architecturally correct for future self-consuming battery variants.
            float selfCons = node.m_Consumption;
            float netFlow = inputReceived - outputDelivered - selfCons;

            // v2.4 (Battery oscillation fix): Clamp netFlow to physical limits.
            // Defensive cap: even if graph has transient desync between epochs,
            // stored energy never corrupts. Also fixes chargeRateDisplay which
            // reads netFlow directly (previously showed -171 u/s uncapped).
            if (netFlow > maxCharge)
            {
                netFlow = maxCharge;
            }
            float negMaxDischarge = -maxDischarge;
            if (netFlow < negMaxDischarge)
            {
                netFlow = negMaxDischarge;
            }

            // --- Apply energy delta ---
            float energyDelta = 0.0;
            if (netFlow > LFPG_PROPAGATION_EPSILON)
            {
                // Charging: apply efficiency loss.
                // Cap by maxChargeRate.
                float chargeWatts = netFlow;
                if (chargeWatts > maxCharge)
                {
                    chargeWatts = maxCharge;
                }
                energyDelta = chargeWatts * efficiency * deltaSec;
            }
            else if (netFlow < -LFPG_PROPAGATION_EPSILON)
            {
                // Discharging: 1:1 from storage (loss was on charge side).
                // Cap by maxDischargeRate.
                float dischargeWatts = -netFlow;
                if (dischargeWatts > maxDischarge)
                {
                    dischargeWatts = maxDischarge;
                }
                energyDelta = -dischargeWatts * deltaSec;
            }

            // Self-discharge (idle drain).
            float selfDrain = storedEnergy * selfDischargeRate * deltaSec / 3600.0;
            energyDelta = energyDelta - selfDrain;

            // Apply delta and clamp.
            float newStored = storedEnergy + energyDelta;
            if (newStored < 0.0)
            {
                newStored = 0.0;
            }
            if (newStored > effectiveMax)
            {
                newStored = effectiveMax;
            }

            // --- Hysteresis: toggle discharge enable ---
            float offThreshold = effectiveMax * LFPG_BATTERY_DISCHARGE_OFF_PCT;
            float onThreshold = effectiveMax * LFPG_BATTERY_DISCHARGE_ON_PCT;
            bool newDischargeEnabled = dischargeEnabled;

            if (dischargeEnabled && newStored < offThreshold)
            {
                // Depleted below 1% → disable discharge.
                newDischargeEnabled = false;
            }
            else if (!dischargeEnabled && newStored > onThreshold)
            {
                // Recovered above 5% → re-enable discharge.
                newDischargeEnabled = true;
            }

            // --- Compute new graph node fields ---
            // v2.0: outputEnabled gates discharge. When switch is OFF,
            // virtualGen=0 (battery doesn't offer power to grid).
            // softDemand is NOT gated — battery charges even with switch OFF.
            float newVirtualGen = 0.0;
            if (outputEnabled && newDischargeEnabled && newStored > LFPG_PROPAGATION_EPSILON)
            {
                // Cap by energy budget: can't promise more than storage can sustain
                // for the duration of one tick.
                float energyBudgetW = newStored / deltaSec;
                newVirtualGen = maxDischarge;
                if (newVirtualGen > energyBudgetW)
                {
                    newVirtualGen = energyBudgetW;
                }
            }

            float newSoftDemand = 0.0;
            float freeSpace = effectiveMax - newStored;
            if (freeSpace > LFPG_PROPAGATION_EPSILON)
            {
                // Cap by charge rate AND by what can be stored in one tick.
                float spaceBudgetW = freeSpace / deltaSec;
                newSoftDemand = maxCharge;
                if (newSoftDemand > spaceBudgetW)
                {
                    newSoftDemand = spaceBudgetW;
                }
            }

            // --- Write back to entity (Sprint 3 implements these) ---
            // String function names hoisted before loop.
            // v3.1 (GC reduction): Reuse Param1 objects instead of allocating per battery.
            m_ReusableParamFloat.param1 = newStored;
            GetGame().GameScript.CallFunctionParams(batEnt, fnSetStored, null, m_ReusableParamFloat);

            if (newDischargeEnabled != dischargeEnabled)
            {
                m_ReusableParamBool.param1 = newDischargeEnabled;
                GetGame().GameScript.CallFunctionParams(batEnt, fnSetDisch, null, m_ReusableParamBool);
            }

            // Write charge rate for UI SyncVar (positive = charging, negative = discharging).
            // v4.2: Derived from actual stored energy delta, NOT from graph netFlow.
            // netFlow reads transient graph state (m_InputPower, SumOutgoingAllocations)
            // which can desync between propagation epochs, producing values like -164
            // that exceed physical limits. The real delta (newStored - storedEnergy)
            // is already clamped by charge/discharge rates and storage bounds,
            // so it always reflects what actually happened to the battery.
            float chargeRateDisplay = (newStored - storedEnergy) / deltaSec;
            m_ReusableParamFloat.param1 = chargeRateDisplay;
            GetGame().GameScript.CallFunctionParams(batEnt, fnSetChargeRate, null, m_ReusableParamFloat);

            // --- Update graph node + mark dirty if changed ---
            float vgDelta = newVirtualGen - node.m_VirtualGeneration;
            if (vgDelta < 0.0)
            {
                vgDelta = -vgDelta;
            }
            float sdDelta = newSoftDemand - node.m_SoftDemand;
            if (sdDelta < 0.0)
            {
                sdDelta = -sdDelta;
            }

            bool needsDirty = false;
            if (vgDelta > LFPG_PROPAGATION_EPSILON)
            {
                needsDirty = true;
            }
            if (sdDelta > LFPG_PROPAGATION_EPSILON)
            {
                needsDirty = true;
            }

            node.m_VirtualGeneration = newVirtualGen;
            node.m_SoftDemand = newSoftDemand;

            if (needsDirty)
            {
                m_Graph.MarkNodeDirty(batId, LFPG_DIRTY_INPUT);
                dirtyCount = dirtyCount + 1;
            }
        }

        if (dirtyCount > 0)
        {
            string batMsg = "[SimpleDevices] Batteries: ";
            batMsg = batMsg + dirtyCount.ToString();
            batMsg = batMsg + "/";
            batMsg = batMsg + batCount.ToString();
            batMsg = batMsg + " triggered propagation";
            LFPG_Util.Info(batMsg);
        }
        #endif
    }

};