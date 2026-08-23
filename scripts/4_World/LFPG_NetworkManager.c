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
//   - LFPG sources (LFPG_Generator): wires stored ON the object
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

// T5 R21-T5-004: value-only owner state that survives EEDelete until the
// coalesced broadcast flush. It deliberately retains no entity reference.
class LFPG_OwnerBroadcastSnapshot
{
    string m_OwnerDeviceId;
    int m_OwnerLow;
    int m_OwnerHigh;
    string m_JSON;
    int m_Generation;
    vector m_OwnerPosition;
    ref array<vector> m_TargetPositions;
    bool m_BroadcastAll;

    void LFPG_OwnerBroadcastSnapshot(string ownerDeviceId, int ownerLow, int ownerHigh, string json, int generation, vector ownerPosition, array<vector> targetPositions)
    {
        m_OwnerDeviceId = ownerDeviceId;
        m_OwnerLow = ownerLow;
        m_OwnerHigh = ownerHigh;
        m_JSON = json;
        m_Generation = generation;
        m_OwnerPosition = ownerPosition;
        m_TargetPositions = new array<vector>;
        m_BroadcastAll = false;

        int targetIndex;
        if (targetPositions)
        {
            for (targetIndex = 0; targetIndex < targetPositions.Count(); targetIndex = targetIndex + 1)
                m_TargetPositions.Insert(targetPositions[targetIndex]);
        }
    }
}

class LFPG_NetworkManager
{
    protected static ref LFPG_NetworkManager s_Instance;
    protected ref LFPG_ControlSessionRegistry m_ControlSessions;

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
    // T5 W4-F06: same-tick CutAll calls share one graph rebuild.
    protected bool m_CutGraphRebuildQueued = false;
    // A detected reverse-index mismatch requires the atomic validation heal.
    protected bool m_IndexHealAfterCut = false;
    // Fail-closed provenance bit: true only after a complete synchronous
    // RebuildReverseIdx and while incremental mutations remain provable.
    protected bool m_ReverseIndexTrusted = false;
    // T5 W1-F07: scope graph-rebuild dedupe to the validation cycle that
    // follows a graph rebuild; index rebuild/recount still always execute.
    protected bool m_ValidationOnlyHealPending = false;
    protected bool m_ValidationSkipGraphRebuild = false;
    protected bool m_ValidationAfterCutRequested = false;
    protected int m_GraphRebuildGeneration = 0;
    protected int m_ValidationGraphGeneration = 0;
    // True only when an incremental graph mutation could not prove that the
    // in-memory graph still matches the authoritative wire stores.
    protected bool m_GraphFullRebuildRequired = true;
    // One-shot credit for the existing replacement path, which explicitly
    // removes the graph edge immediately before ReverseIdxRemove.
    protected string m_ExplicitGraphRemovalCredit = "";
    // Suppresses redundant per-wire graph edits while CutAll is about to
    // remove the whole node and schedule the coalesced rebuild.
    protected bool m_CutAllGraphBatchActive = false;
    protected bool m_CutAllHasPreviousOwnerPosition = false;
    protected vector m_CutAllPreviousOwnerPosition;
    // Devices forced off by a CutAll batch. Applied in the same callback as
    // the final rebuild so SyncVar batching cannot expose an intermediate off.
    protected ref map<string, bool> m_CutPendingPowerOff;

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
    protected bool m_ValidationActive = false;
    protected bool m_ValidationRerunRequested = false;
    protected int m_ValidationPhase;
    protected int m_ValidationCursor;
    protected int m_ValidationStartMs;
    protected int m_ValidationOwnersPruned;
    protected ref array<EntityAI> m_ValidationDevices;
    protected ref map<string, bool> m_ValidationValidIds;
    protected ref array<string> m_ValidationVanillaIds;

    // v0.7.38 (RC-05): FullSync mutex.
    // True while SendFullSyncTo is iterating devices. BroadcastOwnerWires
    // and BroadcastVanillaWires enqueue instead of sending immediately,
    // preventing reorder between FullSync RPCs and mutation Broadcasts.
    protected bool m_FullSyncInProgress = false;
    protected PlayerBase m_FullSyncPlayer;
    protected ref array<PlayerBase> m_FullSyncPendingPlayers;
    protected ref array<EntityAI> m_FullSyncOwners;
    protected ref array<string> m_FullSyncVanillaIds;
    protected int m_FullSyncOwnerCursor;
    protected int m_FullSyncVanillaCursor;
    protected vector m_FullSyncPlayerPos;
    protected float m_FullSyncMaxDistSq;
    protected static const int LFPG_OWNER_SNAPSHOT_MAX_INTEREST_POSITIONS = 128;
    protected ref map<string, ref LFPG_OwnerBroadcastSnapshot> m_DeferredOwnerSnapshots;
    protected ref array<string>   m_DeferredBroadcastVanillaIds;
    protected ref array<EntityAI> m_DeferredBroadcastVanillaObjs;

    // v0.7.4: deferred vanilla wire persistence.
    // MarkVanillaDirty() sets flag; FlushVanillaIfDirty() writes to disk.
    // Periodic timer (LFPG_VANILLA_FLUSH_S) flushes automatically.
    // Eliminates synchronous I/O on every wire mutation.
    protected bool m_VanillaDirty = false;
    protected int m_LastVanillaSaveFailureWarnMs = 0;
    protected int m_VanillaSaveFailureCount = 0;
    protected static const int LFPG_VANILLA_SAVE_WARN_INTERVAL_MS = 60000;

    // v0.7.16 H6: Version guard — track loaded schema version.
    // If file was saved by a newer mod version, block saves to prevent data loss.
    protected int m_VanillaLoadedVer = 0;
    protected bool m_VanillaReadOnly = false;

    // v4.7: Deferred vanilla wire pruning flag.
    // Pruning is deferred to 35s post-init to allow late-loading entities
    // to be resolved before wires are permanently removed.
    protected bool m_DeferredPruneScheduled = false;
    protected bool m_DeferredPruneCompleted = false;

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

    // v5.0: Reusable container list for TickSorters cargo refresh broadcast
    protected ref array<EntityAI> m_TickAffectedContainers;
    protected ref array<EntityAI> m_TickDirtyDestinations;
    protected ref array<EntityAI> m_TickDirtySources;
    protected ref InventoryLocation m_SorterMoveSourceLocation;
    protected ref InventoryLocation m_SorterMoveDestinationLocation;
    protected int m_PerfDiagSorterDestDirtyCount;
    protected int m_PerfDiagSorterSourceDirtyCount;

    // T2: per-sorter deferral state for the real rule-check budget.
    protected ref array<EntityAI> m_SorterResumeItems;
    protected ref array<int> m_SorterResumeItemIndices;
    protected ref array<int> m_SorterResumeOutputs;
    protected ref array<int> m_SorterResumeRules;
    protected ref array<ref LFPG_SortConfig> m_SorterResumeConfigs;
    protected int m_PerfDiagSorterRuleChecks;
    protected int m_PerfDiagSorterConfigMisses;
    protected int m_PerfDiagSorterDeferrals;

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
    // Registration-stable phases use parallel arrays removed at the same index.
    protected ref array<LFPG_Fridge> m_RegisteredFridges;
    protected ref array<int> m_RegisteredFridgePhases;
    protected int m_NextFridgePhase = 0;

    // v1.0.0: Electric Stove centralized cooking timer registry
    protected ref array<LFPG_ElectricStove> m_RegisteredStoves;
    protected ref array<int> m_RegisteredStovePhases;
    protected int m_NextStovePhase = 0;

    // v4.0: DoorController centralized poll timer registry
    protected ref array<LFPG_DoorController> m_RegisteredDoorControllers;

    // v4.1: Solar panel dedicated registry (replaces GetAll+Cast scan)
    protected ref array<LFPG_SolarPanel> m_RegisteredSolars;

    // v4.1: Water pump + sprinkler dedicated registries (replaces GetAll+Cast scan)
    // T1 and T2 are separate classes (T2 does NOT inherit T1).
    protected ref array<LFPG_WaterPump>    m_RegisteredT1Pumps;
    protected ref array<LFPG_WaterPump_T2> m_RegisteredT2Pumps;
    protected ref array<LFPG_Sprinkler>    m_RegisteredSprinklers;
    protected ref array<int> m_RegisteredSprinklerPhases;
    protected int m_NextSprinklerPhase = 0;

    // v4.1/T2: one 300ms timer evaluates every laser, with pads and sensors
    // on bounded sub-cadences, and maintains laser beams round-robin.
    protected int m_PlayerDetectCounter;

    // T2: one coarse player-cell index per detection tick, plus rotating cursors.
    protected static const float LFPG_PLAYER_CELL_SIZE_M = 10.0;
    protected ref array<Man> m_PlayerCellPlayers;
    protected ref array<int> m_PlayerCellMembership;
    protected ref array<int> m_PlayerCellX;
    protected ref array<int> m_PlayerCellZ;
    protected ref array<int> m_PlayerCellStart;
    protected ref array<int> m_PlayerCellCount;
    protected ref array<int> m_PlayerCellWrite;
    protected ref array<Man> m_PlayerCellOrdered;
    protected ref array<Man> m_PlayerCandidates;
    protected int m_LaserDetectCursor;
    protected int m_PadDetectCursor;
    protected int m_SensorDetectCursor;
    protected int m_LaserRaycastCursor;
    protected int m_PerfDiagLaserEvaluations;
    protected int m_PerfDiagPadEvaluations;
    protected int m_PerfDiagSensorEvaluations;
    protected int m_PerfDiagLaserDormant;
    protected int m_PerfDiagPadDormant;
    protected int m_PerfDiagSensorDormant;
    protected int m_PerfDiagLaserChanges;
    protected int m_PerfDiagPadChanges;
    protected int m_PerfDiagSensorChanges;

    // v4.1: Simple Devices consolidated tick sub-counter.
    // One timer at 1,000ms drives intercoms/DC/furnaces/batteries/fridges with stagger offsets.
    // Cycle 1-10, reset at 10. Stagger ensures Batteries and Furnaces never fire same tick.
    protected int m_SimpleTickCounter;
    protected int m_FridgePhaseCursor;
    protected int m_StovePhaseCursor;
    protected int m_SprinklerPhaseCursor;
    protected ref array<Man> m_SprinklerWetPlayers;
    protected int m_PerfDiagWetApplied;
    protected int m_PerfDiagWetCoalesced;
    protected int m_PerfDiagWetPreGateSkips;

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
    protected ref array<vector>   m_ReusableMovedOldPositions;
    protected ref array<string>   m_ReusableDisappearedIds;

    static const int LFPG_SERVER_SCHEDULER_TICK_MS = 100;
    protected static const int LFPG_VALIDATE_RESOLVE_VANILLA = 1;
    protected static const int LFPG_VALIDATE_SNAPSHOT_PRE = 2;
    protected static const int LFPG_VALIDATE_RESOLVE_LFPG = 3;
    protected static const int LFPG_VALIDATE_SNAPSHOT_FINAL = 4;
    protected static const int LFPG_VALIDATE_BUILD_VALID = 5;
    protected static const int LFPG_VALIDATE_REFRESH_LFPG = 6;
    protected static const int LFPG_VALIDATE_REFRESH_VANILLA = 7;
    protected static const int LFPG_VALIDATE_PRUNE_LFPG = 8;
    protected static const int LFPG_VALIDATE_INDEX_REBUILD = 9;
    protected static const int LFPG_VALIDATE_SCHEDULE_PRUNE = 12;
    protected static const int LFPG_VALIDATE_GRAPH_REBUILD = 13;
    protected static const int LFPG_VALIDATE_GRAPH_POPULATE = 14;
    protected static const int LFPG_VALIDATE_GRAPH_MARK = 15;
    protected static const int LFPG_VALIDATE_PRUNE_POSITIONS = 16;
    protected static const int LFPG_VALIDATE_REBUILD_TRACKED = 17;
    protected static const int LFPG_VALIDATE_FINALIZE = 18;
    protected ref Timer m_ServerScheduler;
    protected ref array<string> m_StaleRateLimiterKeys;
    protected int m_SchedPurgeMs;
    protected int m_SchedFlushMs;
    protected int m_SchedPropagationMs;
    protected int m_SchedMovementMs;
    protected int m_SchedSolarMs;
    protected int m_SchedPumpMs;
    protected int m_SchedSorterMs;
    protected int m_SchedPlayerDetectionMs;
    protected int m_SchedSimpleMs;
    protected int m_SchedBtcMs;
    protected int m_SchedBtcIntervalMs;

    // v3.2 (GC reduction): Reusable arrays for BroadcastOwnerWires/BroadcastVanillaWires.
    // Avoids per-call heap allocation of player list and target position list.
    protected ref array<Man>      m_ReusableBroadcastPlayers;
    protected ref array<vector>   m_ReusableBroadcastPositions;
    protected int m_PerfDiagOwnerSnapshotUnicastCount;
    protected int m_PerfDiagOwnerDeltaSendCount;

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
    protected ref map<string, ref LFPG_OwnerBroadcastSnapshot> m_PendingOwnerSnapshots;
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
        m_PendingOwnerSnapshots = new map<string, ref LFPG_OwnerBroadcastSnapshot>;
        m_PendingBroadcastVanilla = new map<string, EntityAI>;
        m_LastKnownPos = new map<string, vector>;
        m_PortLocks = new map<string, bool>;
        m_FullSyncPendingPlayers = new array<PlayerBase>;
        m_FullSyncOwners = new array<EntityAI>;
        m_FullSyncVanillaIds = new array<string>;
        m_DeferredOwnerSnapshots = new map<string, ref LFPG_OwnerBroadcastSnapshot>;
        m_DeferredBroadcastVanillaIds = new array<string>;
        m_DeferredBroadcastVanillaObjs = new array<EntityAI>;

        // v1.2.0: Always allocate (Register/Unregister not guarded with #ifdef)
        m_RegisteredSorters = new array<LFPG_Sorter>;
        m_SorterItemCache = new array<EntityAI>;
        m_TickAffectedContainers = new array<EntityAI>;
        m_TickDirtyDestinations = new array<EntityAI>;
        m_TickDirtySources = new array<EntityAI>;
        m_SorterResumeItems = new array<EntityAI>;
        m_SorterResumeItemIndices = new array<int>;
        m_SorterResumeOutputs = new array<int>;
        m_SorterResumeRules = new array<int>;
        m_SorterResumeConfigs = new array<ref LFPG_SortConfig>;
        m_RegisteredSensors = new array<LFPG_MotionSensor>;
        m_RegisteredPads = new array<LFPG_PressurePad>;
        m_RegisteredLasers = new array<LFPG_LaserDetector>;
        m_RegisteredBatteries = new array<EntityAI>;
        m_RegisteredIntercoms = new array<LFPG_Intercom>;
        m_RegisteredFurnaces = new array<LFPG_Furnace>;
        m_RegisteredFridges = new array<LFPG_Fridge>;
        m_RegisteredFridgePhases = new array<int>;
        m_RegisteredStoves = new array<LFPG_ElectricStove>;
        m_RegisteredStovePhases = new array<int>;
        m_RegisteredDoorControllers = new array<LFPG_DoorController>;
        m_RegisteredSolars = new array<LFPG_SolarPanel>;
        m_RegisteredT1Pumps = new array<LFPG_WaterPump>;
        m_RegisteredT2Pumps = new array<LFPG_WaterPump_T2>;
        m_RegisteredSprinklers = new array<LFPG_Sprinkler>;
        m_RegisteredSprinklerPhases = new array<int>;

        // v3.1 (GC reduction): Initialize reusable tick arrays
        m_ReusablePlayers = new array<Man>;
        m_ReusableMovedIds = new array<string>;
        m_ReusableMovedDevs = new array<EntityAI>;
        m_ReusableMovedOldPositions = new array<vector>;
        m_ReusableDisappearedIds = new array<string>;
        m_ReusableBroadcastPlayers = new array<Man>;
        m_ReusableBroadcastPositions = new array<vector>;
        m_StaleRateLimiterKeys = new array<string>;
        m_SchedPurgeMs = 0;
        m_SchedFlushMs = 0;
        m_SchedPropagationMs = 0;
        m_SchedMovementMs = 0;
        m_SchedSolarMs = 0;
        m_SchedPumpMs = 0;
        m_SchedSorterMs = 0;
        m_SchedPlayerDetectionMs = 0;
        m_SchedSimpleMs = 0;
        m_SchedBtcMs = 0;
        m_SchedBtcIntervalMs = 0;

        #ifdef SERVER
        m_ControlSessions = new LFPG_ControlSessionRegistry();

        // v0.7.30: Tracked device set for centralized polling.
        // Always allocated when compiled as server (dedicated + SP host).
        // Methods have runtime IsServer() guards for extra safety.
        m_TrackedDeviceIds = new array<string>;
        m_ValidationDevices = new array<EntityAI>;
        m_ValidationValidIds = new map<string, bool>;
        m_ValidationVanillaIds = new array<string>;
        m_CutPendingPowerOff = new map<string, bool>;
        m_TrackedDeviceIndex = new map<string, int>;
        m_TrackCursor = 0;
        m_SorterMoveSourceLocation = new InventoryLocation;
        m_SorterMoveDestinationLocation = new InventoryLocation;
        m_PlayerCellPlayers = new array<Man>;
        m_PlayerCellMembership = new array<int>;
        m_PlayerCellX = new array<int>;
        m_PlayerCellZ = new array<int>;
        m_PlayerCellStart = new array<int>;
        m_PlayerCellCount = new array<int>;
        m_PlayerCellWrite = new array<int>;
        m_PlayerCellOrdered = new array<Man>;
        m_PlayerCandidates = new array<Man>;
        m_SprinklerWetPlayers = new array<Man>;
        LFPG_SorterLogic.InitCaches();

        MissionBaseWorld mw = MissionBaseWorld.Cast(g_Game.GetMission());
        if (mw) m_Graph = mw.LFPG_CreateElecGraph();
        if (!m_Graph)
        {
            LFPG_Util.Error("[LFPG_NetworkManager] Mission factory unavailable - fallback base graph (sim degraded)");
            m_Graph = new LFPG_ElecGraph();
        }
        m_WarmupActive = false;
        m_TelemTickCount = 0;
        m_TelemTotalProcessMs = 0;
        m_TelemPeakProcessMs = 0;
        m_TelemTotalEdgesVisited = 0;
        m_TelemLastDumpMs = -99999.0;
        string initMsg = "NetworkManager init (server).";
        LFPG_Util.Info(initMsg);
        LoadVanillaWires();
        bool bFalse = false;
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ValidateAllWiresAndPropagate, 5000, bFalse);
        // Periodic rate limiter cleanup (every 5 minutes)
        // v0.7.4: periodic vanilla wire flush (deferred persistence)
        // Sprint 4.2: periodic propagation tick (event-driven via graph dirty queue)
        // v0.7.30 (Audit 1+2): Centralized position polling with round-robin batching.
        // Replaces per-device timers. Processes LFPG_MOVE_DETECT_BATCH_SIZE devices per tick.
        // Runtime guard: prevents timer registration in SP/local-host hybrid contexts
        // where #ifdef SERVER is active but the instance isn't a true dedicated server.

        // v0.8.0: Centralized solar timer — 1 timer for all solar panels.
        // Seed cached sun state immediately (panels may init before first tick).
        LFPG_ComputeSunState();

        // v1.1.0: Water Pump filter degradation + tank timer
        LFPG_InitTankFillTime();

        // v1.2.0 (Sprint S3): Sorter tick — round-robin batch sorting

        // v4.1: Consolidated player detection tick (lasers 300ms + pads/sensors 600ms).
        // Replaces 4 separate timers. Sub-counters gate slower devices.
        m_PlayerDetectCounter = 0;

        // v4.1: Consolidated simple devices tick (intercoms/DC/furnaces/batteries/fridges).
        // Replaces 5 separate timers. Stagger offsets prevent spike alignment.
        // Intercoms=every tick, DC=%2==1, Furnaces=%5==2, Batteries=%5==4, Fridges=%10==6.
        m_SimpleTickCounter = 0;
        m_BatteryLastTickMs = g_Game.GetTime();
		
		
		// v5.0: BTC ATM price fetcher
        LFPG_BTCConfig.Load();

        // v5.1: Balance provider registry
        LFPG_BalanceProvider_Native nativeProv = new LFPG_BalanceProvider_Native();
        LFPG_BalanceRegistry.Register(nativeProv);
        #ifdef LBmaster_Core
        LFPG_BalanceProvider_LBmaster lbProv = new LFPG_BalanceProvider_LBmaster();
        LFPG_BalanceRegistry.Register(lbProv);
        #endif
        string balMode = LFPG_BTCConfig.GetBalanceMode();
        LFPG_BalanceRegistry.Init(balMode);

        if (LFPG_BTCConfig.IsEnabled())
        {
            LFPG_BTCPriceFetcher.Create();
            m_BTCPriceFetcher = LFPG_BTCPriceFetcher.Get();
            if (m_BTCPriceFetcher)
            {
                m_BTCPriceFetcher.Init();
                m_SchedBtcIntervalMs = LFPG_BTC_PRICE_CHECK_MS;
                string btcInitMsg = "[NM] BTC Price fetcher initialized, tick every ";
                btcInitMsg = btcInitMsg + m_SchedBtcIntervalMs.ToString();
                btcInitMsg = btcInitMsg + "ms";
                LFPG_Util.Info(btcInitMsg);
            }
        }
        else
        {
            string btcOffMsg = "[NM] BTC ATM system DISABLED by config";
            LFPG_Util.Info(btcOffMsg);
        }

        StartServerScheduler();
        #endif
    }

    static LFPG_NetworkManager Get()
    {
        if (!s_Instance)
            s_Instance = new LFPG_NetworkManager();
        return s_Instance;
    }

    static LFPG_NetworkManager GetExisting()
    {
        return s_Instance;
    }

    // Release all mission-scoped state after MissionServer has finished its
    // shutdown sequence. Keeping this singleton across a soft mission restart
    // retains stale entity refs, graph nodes, registries, and pending work.
    static void Reset()
    {
        if (s_Instance)
        {
            s_Instance.Shutdown();
            s_Instance = null;
        }
    }

    void Shutdown()
    {
        #ifdef SERVER
        StopServerScheduler();

        if (g_Game)
        {
            ScriptCallQueue systemQueue = g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM);
            if (systemQueue)
            {
                systemQueue.Remove(ValidateAllWiresAndPropagate);
                systemQueue.Remove(DoGlobalSelfHeal);
                systemQueue.Remove(DeferredVanillaPruneAndRebuild);
                systemQueue.Remove(PostBulkRebuildAndPropagate);
            }
        }

        m_ValidationActive = false;
        m_ValidationRerunRequested = false;
        m_SelfHealQueued = false;
        m_DeferredPruneScheduled = false;
        m_DeferredPruneCompleted = false;
        m_CutGraphRebuildQueued = false;
        #endif
    }

    LFPG_ControlSessionRegistry GetControlSessionRegistry()
    {
        return m_ControlSessions;
    }

    void StartServerScheduler()
    {
        #ifdef SERVER
        if (!g_Game || !g_Game.IsServer())
            return;
        if (m_ServerScheduler)
            return;

        // A stopped-to-started transition is a new mission generation.
        // Runtime control records must never survive that boundary.
        m_ControlSessions = new LFPG_ControlSessionRegistry();

        m_SchedPurgeMs = 0;
        m_SchedFlushMs = 0;
        m_SchedPropagationMs = 0;
        m_SchedMovementMs = 0;
        m_SchedSolarMs = 0;
        m_SchedPumpMs = 0;
        m_SchedSorterMs = 0;
        m_SchedPlayerDetectionMs = 0;
        m_SchedSimpleMs = 0;
        m_SchedBtcMs = 0;
        m_ServerScheduler = new Timer(CALL_CATEGORY_SYSTEM);
        float tickSeconds = LFPG_SERVER_SCHEDULER_TICK_MS / 1000.0;
        m_ServerScheduler.Run(tickSeconds, this, "LFPG_ServerSchedulerTick", NULL, true);
        LFPG_Util.Info("[Scheduler] start tick_ms=100");
        #endif
    }

    void StopServerScheduler()
    {
        #ifdef SERVER
        if (m_ServerScheduler)
        {
            m_ServerScheduler.Stop();
            m_ServerScheduler = null;
            LFPG_Util.Info("[Scheduler] stop");
        }
        #endif
    }

    // Called by LFPG_WireOwnerBase after identity/registry initialization.
    // Broadcasts client state and guarantees that an owner arriving after a
    // validation snapshot is eventually represented in the server graph.
    void RegisterInitializedWireOwner(EntityAI owner)
    {
        #ifdef SERVER
        if (!owner)
            return;

        string ownerId = LFPG_DeviceAPI.GetDeviceId(owner);
        if (ownerId == "")
            return;

        TrackDeviceForPolling(ownerId);
        BroadcastOwnerWires(owner);

        m_GraphFullRebuildRequired = true;
        if (m_ValidationActive)
        {
            m_ValidationRerunRequested = true;
            return;
        }

        if (m_StartupValidationDone)
        {
            RequestGlobalSelfHeal();
        }
        #endif
    }

    protected void LFPG_ServerSchedulerTick()
    {
        #ifdef SERVER
        if (m_ControlSessions)
            m_ControlSessions.Tick();

        m_SchedPurgeMs += LFPG_SERVER_SCHEDULER_TICK_MS;
        if (m_SchedPurgeMs >= 300000)
        {
            m_SchedPurgeMs = 0;
            PurgeStaleRateLimiters();
        }

        m_SchedFlushMs += LFPG_SERVER_SCHEDULER_TICK_MS;
        if (m_SchedFlushMs >= LFPG_VANILLA_FLUSH_S * 1000)
        {
            m_SchedFlushMs = 0;
            FlushVanillaIfDirty();
        }

        m_SchedPropagationMs += LFPG_SERVER_SCHEDULER_TICK_MS;
        if (m_SchedPropagationMs >= LFPG_PROPAGATE_TICK_MS)
        {
            m_SchedPropagationMs = 0;
            TickPropagation();
        }

        m_SchedMovementMs += LFPG_SERVER_SCHEDULER_TICK_MS;
        if (m_SchedMovementMs >= LFPG_MOVE_DETECT_TICK_MS)
        {
            m_SchedMovementMs = 0;
            CheckDeviceMovement();
        }

        m_SchedSolarMs += LFPG_SERVER_SCHEDULER_TICK_MS;
        if (m_SchedSolarMs >= LFPG_SOLAR_CHECK_MS)
        {
            m_SchedSolarMs = 0;
            LFPG_TickSolarPanels();
        }

        m_SchedPumpMs += LFPG_SERVER_SCHEDULER_TICK_MS;
        if (m_SchedPumpMs >= LFPG_PUMP_CHECK_MS)
        {
            m_SchedPumpMs = 0;
            LFPG_TickWaterPumps();
        }

        m_SchedSorterMs += LFPG_SERVER_SCHEDULER_TICK_MS;
        if (m_SchedSorterMs >= LFPG_SORTER_TICK_MS)
        {
            m_SchedSorterMs = 0;
            LFPG_TickSorters();
        }

        m_SchedPlayerDetectionMs += LFPG_SERVER_SCHEDULER_TICK_MS;
        if (m_SchedPlayerDetectionMs >= 300)
        {
            m_SchedPlayerDetectionMs = 0;
            LFPG_TickPlayerDetection();
        }

        m_SchedSimpleMs += LFPG_SERVER_SCHEDULER_TICK_MS;
        if (m_SchedSimpleMs >= 1000)
        {
            m_SchedSimpleMs = 0;
            LFPG_TickSimpleDevices();
        }

        if (m_SchedBtcIntervalMs > 0)
        {
            m_SchedBtcMs += LFPG_SERVER_SCHEDULER_TICK_MS;
        }
        if (m_SchedBtcIntervalMs > 0 && m_SchedBtcMs >= m_SchedBtcIntervalMs)
        {
            m_SchedBtcMs = 0;
            LFPG_TickBTCPrice();
        }

        LFPG_ProcessStartupValidationSlice();
        LFPG_ProcessFullSyncSpread();
        #endif
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
        float now = g_Game.GetTime() * 0.001;

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
        float now = g_Game.GetTime() * 0.001;
        m_StaleRateLimiterKeys.Clear();

        int i;
        for (i = 0; i < m_RateByPlayer.Count(); i = i + 1)
        {
            LFPG_RateLimiter rl = m_RateByPlayer.GetElement(i);
            if (!rl) continue;

            // If NextAllowed is far in the past, player is idle/disconnected
            float idleSec = now - rl.GetNextAllowed();
            if (idleSec > RATE_LIMITER_STALE_SEC)
            {
                m_StaleRateLimiterKeys.Insert(m_RateByPlayer.GetKey(i));
            }
        }

        int removed = m_StaleRateLimiterKeys.Count();
        int k;
        string staleKey;
        for (k = 0; k < removed; k = k + 1)
        {
            staleKey = m_StaleRateLimiterKeys[k];
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

    bool IsValidationActive()
    {
        return m_ValidationActive;
    }

    bool IsVanillaStoreReadOnly()
    {
        return m_VanillaReadOnly;
    }

    bool AddVanillaWire(string ownerDeviceId, LFPG_WireData wd)
    {
        if (ownerDeviceId == "" || !wd)
            return false;
        if (m_VanillaReadOnly)
        {
            LFPG_Util.Warn("[VanillaWires] Add rejected while persisted store is read-only");
            return false;
        }

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
        if (!inserted)
            m_GraphFullRebuildRequired = true;
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
        string creditTargetPort = targetPort;
        if (creditTargetPort == "")
            creditTargetPort = "input_main";
        int explicitMatchCount = 0;
        ref array<ref LFPG_ElecEdge> explicitOutEdges = m_Graph.GetOutgoing(sourceId);
        if (explicitOutEdges)
        {
            int explicitEdgeIndex;
            for (explicitEdgeIndex = 0; explicitEdgeIndex < explicitOutEdges.Count(); explicitEdgeIndex = explicitEdgeIndex + 1)
            {
                LFPG_ElecEdge explicitEdge = explicitOutEdges[explicitEdgeIndex];
                if (!explicitEdge || explicitEdge.m_TargetNodeId != targetId || explicitEdge.m_SourcePort != sourcePort)
                    continue;
                string explicitTargetPort = explicitEdge.m_TargetPort;
                if (explicitTargetPort == "")
                    explicitTargetPort = "input_main";
                if (explicitTargetPort == creditTargetPort)
                    explicitMatchCount = explicitMatchCount + 1;
            }
        }
        if (explicitMatchCount != 1)
        {
            m_GraphFullRebuildRequired = true;
            RequestGlobalSelfHeal();
        }
        m_ExplicitGraphRemovalCredit = sourceId + "|" + targetId + "|" + creditTargetPort;
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
        // A direct bulk rebuild also satisfies a queued CutAll rebuild.
        bool cutGraphRebuild = m_CutGraphRebuildQueued;
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(PostBulkRebuildAndPropagate);
        m_CutGraphRebuildQueued = false;
        if (cutGraphRebuild)
        {
            int pendingPowerIndex;
            for (pendingPowerIndex = 0; pendingPowerIndex < m_CutPendingPowerOff.Count(); pendingPowerIndex = pendingPowerIndex + 1)
            {
                string pendingPowerId = m_CutPendingPowerOff.GetKey(pendingPowerIndex);
                EntityAI pendingPowerDevice = LFPG_DeviceRegistry.Get().FindById(pendingPowerId);
                if (!pendingPowerDevice)
                    pendingPowerDevice = LFPG_DeviceAPI.ResolveVanillaDevice(pendingPowerId);
                if (pendingPowerDevice)
                    LFPG_DeviceAPI.SetPowered(pendingPowerDevice, false);
            }
            m_CutPendingPowerOff.Clear();
        }
        if (!m_Graph)
        {
            if (cutGraphRebuild)
                FlushBroadcasts();
            m_ValidationAfterCutRequested = false;
            if (m_IndexHealAfterCut)
            {
                m_IndexHealAfterCut = false;
                RequestGlobalSelfHeal();
            }
            return;
        }
        if (cutGraphRebuild || m_GraphFullRebuildRequired)
        {
            m_Graph.PostBulkRebuild(this);
            m_GraphFullRebuildRequired = false;
            m_GraphRebuildGeneration = m_GraphRebuildGeneration + 1;
        }
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
        if (cutGraphRebuild)
            FlushBroadcasts();

        if (cutGraphRebuild && m_ValidationAfterCutRequested)
        {
            m_ValidationAfterCutRequested = false;
            m_ValidationOnlyHealPending = true;
            RequestGlobalSelfHeal();
        }

        if (m_IndexHealAfterCut)
        {
            m_IndexHealAfterCut = false;
            m_ValidationOnlyHealPending = true;
            RequestGlobalSelfHeal();
        }
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
        // A caller that discovered stale index state cannot assume the graph
        // is also current unless the scoped movement validation proves it.
        m_GraphFullRebuildRequired = true;
        m_ReverseIndexTrusted = false;
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
        m_ReverseIndexTrusted = true;
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

        // T5 W1-F06: callers that remove directly from an owner store already
        // carry the source owner here. Mirror an unambiguous single edge into
        // the graph so their subsequent PostBulk call can stay scoped.
        string explicitRemovalKey = ownerDeviceId + "|" + targetDeviceId + "|" + targetPort;
        bool graphAlreadyNotified = false;
        if (m_ExplicitGraphRemovalCredit != "")
        {
            if (m_ExplicitGraphRemovalCredit == explicitRemovalKey)
                graphAlreadyNotified = true;
            m_ExplicitGraphRemovalCredit = "";
        }
        if (m_Graph && ownerDeviceId != "" && !graphAlreadyNotified && !m_CutAllGraphBatchActive)
        {
            ref array<ref LFPG_ElecEdge> reverseOutEdges = m_Graph.GetOutgoing(ownerDeviceId);
            int reverseMatchCount = 0;
            string reverseSourcePort = "";
            string reverseGraphTargetPort = "";
            if (reverseOutEdges)
            {
                int reverseEdgeIndex;
                for (reverseEdgeIndex = 0; reverseEdgeIndex < reverseOutEdges.Count(); reverseEdgeIndex = reverseEdgeIndex + 1)
                {
                    LFPG_ElecEdge reverseEdge = reverseOutEdges[reverseEdgeIndex];
                    if (!reverseEdge || reverseEdge.m_TargetNodeId != targetDeviceId)
                        continue;

                    string reverseTargetPort = reverseEdge.m_TargetPort;
                    if (reverseTargetPort == "")
                        reverseTargetPort = "input_main";
                    if (reverseTargetPort != targetPort)
                        continue;

                    reverseMatchCount = reverseMatchCount + 1;
                    reverseSourcePort = reverseEdge.m_SourcePort;
                    reverseGraphTargetPort = reverseEdge.m_TargetPort;
                }
            }

            if (reverseMatchCount == 1)
            {
                m_Graph.OnWireRemoved(ownerDeviceId, targetDeviceId, reverseSourcePort, reverseGraphTargetPort);
                LFPG_RefreshPumpSprinklerLink(ownerDeviceId, targetDeviceId);
            }
            else if (reverseMatchCount > 1)
                m_GraphFullRebuildRequired = true;
        }

        string rKey = targetDeviceId + "|" + targetPort;
        int prev = 0;
        if (m_ReverseIdx.Find(rKey, prev))
        {
            if (prev <= 0)
                m_ReverseIndexTrusted = false;
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
        else
        {
            // The authoritative store caller is removing a wire that the
            // index did not contain. Keep cleanup fail-closed until rebuild.
            m_ReverseIndexTrusted = false;
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
    int RemoveWiresTargeting(string targetDeviceId, string targetPort, string creatorId = "", bool allowOthers = true)
    {
        #ifdef SERVER
        int removed = 0;
        bool filterByCreator = !allowOthers;
        if (filterByCreator && creatorId == "")
            return 0;

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
                    ref array<int> ownerDeltaOps = new array<int>;
                    ref array<ref LFPG_WireData> ownerDeltaWires = new array<ref LFPG_WireData>;
                    int gw = gWires.Count() - 1;
                    while (gw >= 0)
                    {
                        LFPG_WireData wd = gWires[gw];
                        if (wd && wd.m_TargetDeviceId == targetDeviceId && wd.m_TargetPort == targetPort && (!filterByCreator || LFPG_WireHelper.CanCreatorCutWire(wd, creatorId, allowOthers)))
                        {
                            // v0.7.34 (Bloque E): Notify graph before removing wire data.
                            // Without this, replaced edges stay stale in the graph.
                            // LFPG wires: use m_SourcePort as-is (no normalization).
                            // This matches RebuildFromWires which also does NOT normalize
                            // LFPG wire source ports.
                            if (filterByCreator)
                            {
                                NotifyGraphWireRemoved(ownerId, targetDeviceId, wd.m_SourcePort, targetPort);
                                ReverseIdxRemove(targetDeviceId, targetPort, ownerId);
                            }
                            else if (m_Graph && !m_CutAllGraphBatchActive)
                            {
                                m_Graph.OnWireRemoved(ownerId, targetDeviceId, wd.m_SourcePort, targetPort);
                            }

                            PlayerWireCountAdd(wd.m_CreatorId, -1);
                            ownerDeltaOps.Insert(LFPG_WireDeltaOp.REMOVE);
                            ownerDeltaWires.Insert(wd);
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
                        LFPG_WireOwnerBase wireOwner = LFPG_WireOwnerBase.Cast(ownerObj);
                        if (wireOwner)
                        {
                            wireOwner.LFPG_CommitWireMutation();
                            if (m_CutAllGraphBatchActive)
                                QueueBroadcastOwnerSnapshotFromWires(ownerObj, ownerDeltaWires);
                            else
                                BroadcastOwnerWireDelta(ownerObj, ownerDeltaOps, ownerDeltaWires);
                        }
                        else
                        {
                            ownerObj.SetSynchDirty();
                            QueueBroadcastOwnerSnapshotFromWires(ownerObj, ownerDeltaWires);
                        }
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
                    if (vwd && vwd.m_TargetDeviceId == targetDeviceId && vwd.m_TargetPort == targetPort && (!filterByCreator || LFPG_WireHelper.CanCreatorCutWire(vwd, creatorId, allowOthers)))
                    {
                        // v0.7.34 (Bloque E): Notify graph before removing wire data.
                        // Vanilla wires: normalize empty sourcePort to "output_1".
                        // This matches RebuildFromWires which normalizes vanilla ports.
                        string vSrcP = vwd.m_SourcePort;
                        if (vSrcP == "")
                        {
                            vSrcP = "output_1";
                        }
                        if (filterByCreator)
                        {
                            NotifyGraphWireRemoved(ownerId, targetDeviceId, vSrcP, targetPort);
                            ReverseIdxRemove(targetDeviceId, targetPort, ownerId);
                        }
                        else if (m_Graph && !m_CutAllGraphBatchActive)
                        {
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
            if (!filterByCreator)
            {
                m_ReverseIdx.Remove(rKey);
                m_ReverseOwners.Remove(rKey);
            }
            MarkVanillaDirty();
            if (!m_CutAllGraphBatchActive)
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
        if (CoalescePendingOwnerSnapshot(owner, null))
            return;
        m_PendingOwnerSnapshots.Remove(devId);
        m_PendingBroadcastLFPG[devId] = owner;
    }

    protected bool AppendOwnerSnapshotPosition(array<vector> positions, vector position)
    {
        if (!positions)
            return false;
        if (positions.Find(position) >= 0)
            return true;
        if (positions.Count() >= LFPG_OWNER_SNAPSHOT_MAX_INTEREST_POSITIONS)
            return false;

        positions.Insert(position);
        return true;
    }

    protected void StorePendingOwnerSnapshot(LFPG_OwnerBroadcastSnapshot snapshot)
    {
        if (!snapshot || snapshot.m_OwnerDeviceId == "")
            return;

        array<vector> combinedPositions = new array<vector>;
        LFPG_OwnerBroadcastSnapshot previousSnapshot;
        LFPG_OwnerBroadcastSnapshot mergedSnapshot;
        int snapshotIndex;
        int previousIndex;
        bool broadcastAll = snapshot.m_BroadcastAll;

        if (m_CutAllGraphBatchActive && m_CutAllHasPreviousOwnerPosition)
        {
            if (!AppendOwnerSnapshotPosition(combinedPositions, m_CutAllPreviousOwnerPosition))
                broadcastAll = true;
        }
        for (snapshotIndex = 0; snapshotIndex < snapshot.m_TargetPositions.Count() && !broadcastAll; snapshotIndex = snapshotIndex + 1)
        {
            if (!AppendOwnerSnapshotPosition(combinedPositions, snapshot.m_TargetPositions[snapshotIndex]))
                broadcastAll = true;
        }

        if (m_PendingOwnerSnapshots.Find(snapshot.m_OwnerDeviceId, previousSnapshot) && previousSnapshot)
        {
            if (previousSnapshot.m_BroadcastAll)
                broadcastAll = true;
            if (!broadcastAll && !AppendOwnerSnapshotPosition(combinedPositions, previousSnapshot.m_OwnerPosition))
                broadcastAll = true;
            for (previousIndex = 0; previousIndex < previousSnapshot.m_TargetPositions.Count() && !broadcastAll; previousIndex = previousIndex + 1)
            {
                if (!AppendOwnerSnapshotPosition(combinedPositions, previousSnapshot.m_TargetPositions[previousIndex]))
                    broadcastAll = true;
            }
        }

        if (broadcastAll)
            combinedPositions.Clear();
        mergedSnapshot = new LFPG_OwnerBroadcastSnapshot(snapshot.m_OwnerDeviceId, snapshot.m_OwnerLow, snapshot.m_OwnerHigh, snapshot.m_JSON, snapshot.m_Generation, snapshot.m_OwnerPosition, combinedPositions);
        mergedSnapshot.m_BroadcastAll = broadcastAll;
        m_PendingBroadcastLFPG.Remove(snapshot.m_OwnerDeviceId);
        m_PendingOwnerSnapshots[snapshot.m_OwnerDeviceId] = mergedSnapshot;
    }

    void QueueBroadcastOwnerSnapshot(EntityAI owner, array<vector> targetPositions, bool broadcastAll)
    {
        if (!owner)
            return;

        string ownerId = LFPG_DeviceAPI.GetDeviceId(owner);
        if (ownerId == "")
            return;

        int ownerLow = 0;
        int ownerHigh = 0;
        string json = LFPG_DeviceAPI.GetWiresJSON(owner);
        int generation = -1;
        vector ownerPosition = owner.GetPosition();
        LFPG_WireOwnerBase wireOwner = LFPG_WireOwnerBase.Cast(owner);
        LFPG_OwnerBroadcastSnapshot snapshot;

        owner.GetNetworkID(ownerLow, ownerHigh);
        if (wireOwner)
            generation = wireOwner.LFPG_GetWireGeneration();

        snapshot = new LFPG_OwnerBroadcastSnapshot(ownerId, ownerLow, ownerHigh, json, generation, ownerPosition, targetPositions);
        snapshot.m_BroadcastAll = broadcastAll;
        StorePendingOwnerSnapshot(snapshot);
    }

    protected void QueueBroadcastOwnerSnapshotFromWires(EntityAI owner, array<ref LFPG_WireData> interestWires)
    {
        LFPG_OwnerBroadcastSnapshot snapshot;
        snapshot = CaptureOwnerBroadcastSnapshot(owner, interestWires);
        if (snapshot)
            StorePendingOwnerSnapshot(snapshot);
    }

    protected bool CoalescePendingOwnerSnapshot(EntityAI owner, array<ref LFPG_WireData> extraInterestWires)
    {
        if (!owner)
            return false;

        string ownerId = LFPG_DeviceAPI.GetDeviceId(owner);
        if (ownerId == "")
            return false;
        if (!m_PendingOwnerSnapshots.Contains(ownerId))
            return false;

        LFPG_OwnerBroadcastSnapshot snapshot;
        snapshot = CaptureOwnerBroadcastSnapshot(owner, extraInterestWires);
        if (snapshot)
            StorePendingOwnerSnapshot(snapshot);
        return true;
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

        // Flush value-only LFPG owner snapshots after live owners. Queue-time
        // exclusion guarantees that only the latest state exists for an ID.
        int snapshotIndex;
        for (snapshotIndex = 0; snapshotIndex < m_PendingOwnerSnapshots.Count(); snapshotIndex = snapshotIndex + 1)
        {
            LFPG_OwnerBroadcastSnapshot snapshot = m_PendingOwnerSnapshots.GetElement(snapshotIndex);
            if (snapshot)
                BroadcastOwnerSnapshot(snapshot);
        }
        m_PendingOwnerSnapshots.Clear();

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
        string ownerId = LFPG_DeviceAPI.GetDeviceId(owner);
        if (ownerId == "") return;
        if (CoalescePendingOwnerSnapshot(owner, null))
            return;
        m_PendingOwnerSnapshots.Remove(ownerId);

        // v0.7.38 (RC-05): Defer broadcast if FullSync is in progress.
        // Prevents reordering where a Broadcast arrives at client BEFORE
        // the FullSync RPC for the same owner, leaving stale state.
        if (m_FullSyncInProgress)
        {
            DeferOwnerSnapshot(owner, null);
            return;
        }

        // Avoid serialization unless at least one current player is interested.
        m_ReusableBroadcastPlayers.Clear();
        g_Game.GetPlayers(m_ReusableBroadcastPlayers);
        if (m_ReusableBroadcastPlayers.Count() == 0)
            return;

        float preSyncMaxDist = LFPG_CULL_DISTANCE_M + 20.0;
        float preSyncMaxDistSq = preSyncMaxDist * preSyncMaxDist;
        vector preOwnerPos = owner.GetPosition();
        m_ReusableBroadcastPositions.Clear();
        ref array<ref LFPG_WireData> preOwnerWires = LFPG_DeviceAPI.GetDeviceWires(owner);
        if (preOwnerWires)
        {
            LFPG_DeviceRegistry preReg = LFPG_DeviceRegistry.Get();
            int preTw;
            for (preTw = 0; preTw < preOwnerWires.Count(); preTw = preTw + 1)
            {
                if (!preOwnerWires[preTw]) continue;
                if (preOwnerWires[preTw].m_TargetDeviceId == "") continue;
                EntityAI preTarget = preReg.FindById(preOwnerWires[preTw].m_TargetDeviceId);
                if (preTarget)
                    m_ReusableBroadcastPositions.Insert(preTarget.GetPosition());
            }
        }

        bool hasRecipient = false;
        int prePi;
        for (prePi = 0; prePi < m_ReusableBroadcastPlayers.Count(); prePi = prePi + 1)
        {
            PlayerBase prePlayer = PlayerBase.Cast(m_ReusableBroadcastPlayers[prePi]);
            if (!prePlayer) continue;
            vector prePlayerPos = prePlayer.GetPosition();
            if (LFPG_WorldUtil.DistSq(prePlayerPos, preOwnerPos) <= preSyncMaxDistSq)
            {
                hasRecipient = true;
                break;
            }
            int preTp;
            for (preTp = 0; preTp < m_ReusableBroadcastPositions.Count(); preTp = preTp + 1)
            {
                if (LFPG_WorldUtil.DistSq(prePlayerPos, m_ReusableBroadcastPositions[preTp]) <= preSyncMaxDistSq)
                {
                    hasRecipient = true;
                    break;
                }
            }
            if (hasRecipient)
                break;
        }
        if (!hasRecipient)
            return;
        string json = LFPG_DeviceAPI.GetWiresJSON(owner);
        LFPG_WireOwnerBase snapshotWireOwner = LFPG_WireOwnerBase.Cast(owner);
        int snapshotGeneration = -1;
        if (snapshotWireOwner)
        {
            snapshotGeneration = snapshotWireOwner.LFPG_GetWireGeneration();
        }
        m_ReusableBroadcastPlayers.Clear();
        g_Game.GetPlayers(m_ReusableBroadcastPlayers);

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
        m_ReusableBroadcastPositions.Clear();
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
                    m_ReusableBroadcastPositions.Insert(targetObj.GetPosition());
                }
            }
        }

        int i;
        for (i = 0; i < m_ReusableBroadcastPlayers.Count(); i = i + 1)
        {
            PlayerBase pb = PlayerBase.Cast(m_ReusableBroadcastPlayers[i]);
            if (!pb) continue;

            vector playerPos = pb.GetPosition();

            // v0.7.11 (A3): Compare in squared domain — eliminates sqrt per player.
            bool inRange = (LFPG_WorldUtil.DistSq(playerPos, ownerPos) <= syncMaxDistSq);

            // v0.7.35 B-CRIT2: Also check distance to each target device
            if (!inRange)
            {
                int tp;
                for (tp = 0; tp < m_ReusableBroadcastPositions.Count(); tp = tp + 1)
                {
                    if (LFPG_WorldUtil.DistSq(playerPos, m_ReusableBroadcastPositions[tp]) <= syncMaxDistSq)
                    {
                        inRange = true;
                        break;
                    }
                }
            }

            if (!inRange) continue;

            ScriptRPC rpc = new ScriptRPC();
            rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES_V2);
            rpc.Write(ownerId);
            rpc.Write(low);
            rpc.Write(high);
            rpc.Write(json);
            rpc.Write(snapshotGeneration);
            bool bRpcGuaranteed = true;
            PlayerIdentity noExclude = null;
            rpc.Send(pb, LFPG_RPC_CHANNEL, bRpcGuaranteed, noExclude);
        }
    }

    protected EntityAI ResolveOwnerSnapshotTarget(LFPG_WireData wire)
    {
        if (!wire)
            return null;

        EntityAI target = null;
        if (wire.m_TargetDeviceId != "")
            target = LFPG_DeviceRegistry.Get().FindById(wire.m_TargetDeviceId);
        if (!target)
            target = LFPG_DeviceAPI.ResolveByNetworkId(wire.m_TargetNetLow, wire.m_TargetNetHigh);
        return target;
    }

    protected LFPG_OwnerBroadcastSnapshot CaptureOwnerBroadcastSnapshot(EntityAI owner, array<ref LFPG_WireData> extraInterestWires)
    {
        if (!owner)
            return null;

        string ownerId = LFPG_DeviceAPI.GetDeviceId(owner);
        if (ownerId == "")
            return null;

        array<vector> targetPositions = new array<vector>;
        array<ref LFPG_WireData> currentWires = LFPG_DeviceAPI.GetDeviceWires(owner);
        LFPG_WireData interestWire;
        EntityAI interestTarget;
        int currentIndex;
        int extraIndex;
        bool broadcastAll = false;

        if (currentWires)
        {
            for (currentIndex = 0; currentIndex < currentWires.Count(); currentIndex = currentIndex + 1)
            {
                interestWire = currentWires[currentIndex];
                if (!interestWire)
                    continue;
                interestTarget = ResolveOwnerSnapshotTarget(interestWire);
                if (interestTarget)
                    targetPositions.Insert(interestTarget.GetPosition());
                else
                    broadcastAll = true;
            }
        }

        if (extraInterestWires)
        {
            for (extraIndex = 0; extraIndex < extraInterestWires.Count(); extraIndex = extraIndex + 1)
            {
                interestWire = extraInterestWires[extraIndex];
                if (!interestWire)
                    continue;
                interestTarget = ResolveOwnerSnapshotTarget(interestWire);
                if (interestTarget)
                    targetPositions.Insert(interestTarget.GetPosition());
                else
                    broadcastAll = true;
            }
        }

        int ownerLow = 0;
        int ownerHigh = 0;
        string json = LFPG_DeviceAPI.GetWiresJSON(owner);
        int generation = -1;
        vector ownerPosition = owner.GetPosition();
        LFPG_WireOwnerBase wireOwner = LFPG_WireOwnerBase.Cast(owner);

        owner.GetNetworkID(ownerLow, ownerHigh);
        if (wireOwner)
            generation = wireOwner.LFPG_GetWireGeneration();

        LFPG_OwnerBroadcastSnapshot snapshot = new LFPG_OwnerBroadcastSnapshot(ownerId, ownerLow, ownerHigh, json, generation, ownerPosition, targetPositions);
        snapshot.m_BroadcastAll = broadcastAll;
        return snapshot;
    }

    protected void StoreDeferredOwnerSnapshot(LFPG_OwnerBroadcastSnapshot snapshot)
    {
        if (!snapshot || snapshot.m_OwnerDeviceId == "")
            return;

        array<vector> combinedPositions = new array<vector>;
        LFPG_OwnerBroadcastSnapshot previousSnapshot;
        LFPG_OwnerBroadcastSnapshot mergedSnapshot;
        int snapshotIndex;
        int previousIndex;
        bool broadcastAll = snapshot.m_BroadcastAll;

        for (snapshotIndex = 0; snapshotIndex < snapshot.m_TargetPositions.Count() && !broadcastAll; snapshotIndex = snapshotIndex + 1)
        {
            if (!AppendOwnerSnapshotPosition(combinedPositions, snapshot.m_TargetPositions[snapshotIndex]))
                broadcastAll = true;
        }

        if (m_DeferredOwnerSnapshots.Find(snapshot.m_OwnerDeviceId, previousSnapshot) && previousSnapshot)
        {
            if (previousSnapshot.m_BroadcastAll)
                broadcastAll = true;
            if (!broadcastAll && !AppendOwnerSnapshotPosition(combinedPositions, previousSnapshot.m_OwnerPosition))
                broadcastAll = true;
            for (previousIndex = 0; previousIndex < previousSnapshot.m_TargetPositions.Count() && !broadcastAll; previousIndex = previousIndex + 1)
            {
                if (!AppendOwnerSnapshotPosition(combinedPositions, previousSnapshot.m_TargetPositions[previousIndex]))
                    broadcastAll = true;
            }
        }

        if (broadcastAll)
            combinedPositions.Clear();
        mergedSnapshot = new LFPG_OwnerBroadcastSnapshot(snapshot.m_OwnerDeviceId, snapshot.m_OwnerLow, snapshot.m_OwnerHigh, snapshot.m_JSON, snapshot.m_Generation, snapshot.m_OwnerPosition, combinedPositions);
        mergedSnapshot.m_BroadcastAll = broadcastAll;
        m_DeferredOwnerSnapshots[snapshot.m_OwnerDeviceId] = mergedSnapshot;
    }

    protected void DeferOwnerSnapshot(EntityAI owner, array<ref LFPG_WireData> extraInterestWires)
    {
        LFPG_OwnerBroadcastSnapshot snapshot;
        snapshot = CaptureOwnerBroadcastSnapshot(owner, extraInterestWires);
        if (snapshot)
            StoreDeferredOwnerSnapshot(snapshot);
    }

    protected void BroadcastOwnerSnapshot(LFPG_OwnerBroadcastSnapshot snapshot)
    {
        if (!snapshot || snapshot.m_OwnerDeviceId == "")
            return;

        if (m_FullSyncInProgress)
        {
            StoreDeferredOwnerSnapshot(snapshot);
            return;
        }

        m_ReusableBroadcastPlayers.Clear();
        g_Game.GetPlayers(m_ReusableBroadcastPlayers);

        string snapshotMsg = "[BroadcastOwnerSnapshot] owner=" + snapshot.m_OwnerDeviceId + " generation=" + snapshot.m_Generation.ToString() + " jsonLen=" + snapshot.m_JSON.Length().ToString();
        LFPG_Util.Info(snapshotMsg);

        float syncMaxDist = LFPG_CULL_DISTANCE_M + 20.0;
        float syncMaxDistSq = syncMaxDist * syncMaxDist;
        int playerIndex;
        int targetIndex;
        PlayerBase player;
        vector playerPosition;
        bool inRange;
        ScriptRPC rpc;
        bool guaranteed = true;
        PlayerIdentity noExclude = null;

        for (playerIndex = 0; playerIndex < m_ReusableBroadcastPlayers.Count(); playerIndex = playerIndex + 1)
        {
            player = PlayerBase.Cast(m_ReusableBroadcastPlayers[playerIndex]);
            if (!player)
                continue;

            playerPosition = player.GetPosition();
            inRange = snapshot.m_BroadcastAll;
            if (!inRange)
                inRange = LFPG_WorldUtil.DistSq(playerPosition, snapshot.m_OwnerPosition) <= syncMaxDistSq;
            if (!inRange)
            {
                for (targetIndex = 0; targetIndex < snapshot.m_TargetPositions.Count(); targetIndex = targetIndex + 1)
                {
                    if (LFPG_WorldUtil.DistSq(playerPosition, snapshot.m_TargetPositions[targetIndex]) <= syncMaxDistSq)
                    {
                        inRange = true;
                        break;
                    }
                }
            }
            if (!inRange)
                continue;

            rpc = new ScriptRPC();
            rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES_V2);
            rpc.Write(snapshot.m_OwnerDeviceId);
            rpc.Write(snapshot.m_OwnerLow);
            rpc.Write(snapshot.m_OwnerHigh);
            rpc.Write(snapshot.m_JSON);
            rpc.Write(snapshot.m_Generation);
            rpc.Send(player, LFPG_RPC_CHANNEL, guaranteed, noExclude);
        }
    }

    // ===========================
    // Sync: LFPG source mutation delta -> current interested clients
    // ===========================
    void BroadcastOwnerWireDelta(EntityAI owner, array<int> operations, array<ref LFPG_WireData> deltaWires)
    {
        if (!owner || !operations || !deltaWires)
            return;

        int entryCount = operations.Count();
        if (entryCount <= 0 || entryCount > LFPG_WIRE_DELTA_MAX_ENTRIES)
            return;
        if (deltaWires.Count() != entryCount)
            return;

        string ownerId = LFPG_DeviceAPI.GetDeviceId(owner);
        if (ownerId == "")
            return;
        if (CoalescePendingOwnerSnapshot(owner, deltaWires))
            return;
        m_PendingOwnerSnapshots.Remove(ownerId);

        if (m_FullSyncInProgress)
        {
            DeferOwnerSnapshot(owner, deltaWires);
            return;
        }

        LFPG_WireOwnerBase wireOwner = LFPG_WireOwnerBase.Cast(owner);
        if (!wireOwner)
        {
            BroadcastOwnerWires(owner);
            return;
        }

        ref array<string> entryJsons = new array<string>;
        int payloadChars = 0;
        int e;
        for (e = 0; e < entryCount; e = e + 1)
        {
            int operation = operations[e];
            if (operation != LFPG_WireDeltaOp.ADD && operation != LFPG_WireDeltaOp.REMOVE && operation != LFPG_WireDeltaOp.UPDATE)
                return;

            LFPG_WireData deltaWire = deltaWires[e];
            if (!deltaWire)
                return;

            LFPG_PersistBlob entryBlob = new LFPG_PersistBlob();
            entryBlob.wires.Insert(deltaWire);
            string entryJson = "";
            string entryErr = "";
            if (!JsonFileLoader<LFPG_PersistBlob>.MakeData(entryBlob, entryJson, entryErr, false))
            {
                BroadcastOwnerWires(owner);
                return;
            }
            entryJsons.Insert(entryJson);
            payloadChars = payloadChars + entryJson.Length();
        }

        int low = 0;
        int high = 0;
        owner.GetNetworkID(low, high);
        int generation = wireOwner.LFPG_GetWireGeneration();

        m_ReusableBroadcastPlayers.Clear();
        g_Game.GetPlayers(m_ReusableBroadcastPlayers);
        m_ReusableBroadcastPositions.Clear();

        LFPG_DeviceRegistry reg = LFPG_DeviceRegistry.Get();
        ref array<ref LFPG_WireData> currentWires = LFPG_DeviceAPI.GetDeviceWires(owner);
        if (currentWires)
        {
            int cw;
            for (cw = 0; cw < currentWires.Count(); cw = cw + 1)
            {
                LFPG_WireData currentWire = currentWires[cw];
                if (!currentWire || currentWire.m_TargetDeviceId == "")
                    continue;
                EntityAI currentTarget = reg.FindById(currentWire.m_TargetDeviceId);
                if (currentTarget)
                {
                    m_ReusableBroadcastPositions.Insert(currentTarget.GetPosition());
                }
            }
        }

        // Removed targets are no longer in currentWires but still need the delta.
        for (e = 0; e < entryCount; e = e + 1)
        {
            LFPG_WireData interestWire = deltaWires[e];
            if (!interestWire || interestWire.m_TargetDeviceId == "")
                continue;
            EntityAI interestTarget = reg.FindById(interestWire.m_TargetDeviceId);
            if (interestTarget)
            {
                m_ReusableBroadcastPositions.Insert(interestTarget.GetPosition());
            }
        }

        float syncMaxDist = LFPG_CULL_DISTANCE_M + 20.0;
        float syncMaxDistSq = syncMaxDist * syncMaxDist;
        vector ownerPos = owner.GetPosition();

        int i;
        for (i = 0; i < m_ReusableBroadcastPlayers.Count(); i = i + 1)
        {
            PlayerBase pb = PlayerBase.Cast(m_ReusableBroadcastPlayers[i]);
            if (!pb)
                continue;

            vector playerPos = pb.GetPosition();
            bool inRange = (LFPG_WorldUtil.DistSq(playerPos, ownerPos) <= syncMaxDistSq);
            if (!inRange)
            {
                int tp;
                for (tp = 0; tp < m_ReusableBroadcastPositions.Count(); tp = tp + 1)
                {
                    if (LFPG_WorldUtil.DistSq(playerPos, m_ReusableBroadcastPositions[tp]) <= syncMaxDistSq)
                    {
                        inRange = true;
                        break;
                    }
                }
            }
            if (!inRange)
                continue;

            ScriptRPC rpc = new ScriptRPC();
            rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES_DELTA);
            rpc.Write(ownerId);
            rpc.Write(low);
            rpc.Write(high);
            rpc.Write(generation);
            rpc.Write(entryCount);
            for (e = 0; e < entryCount; e = e + 1)
            {
                rpc.Write(operations[e]);
                rpc.Write(entryJsons[e]);
            }
            rpc.Send(pb, LFPG_RPC_CHANNEL, true, null);

            if (LFPG_PERFDIAG_ENABLED)
            {
                m_PerfDiagOwnerDeltaSendCount = m_PerfDiagOwnerDeltaSendCount + 1;
                string perfDelta = "LFPG_PERFDIAG delta_send count=";
                perfDelta = perfDelta + m_PerfDiagOwnerDeltaSendCount.ToString();
                perfDelta = perfDelta + " deviceId=";
                perfDelta = perfDelta + ownerId;
                perfDelta = perfDelta + " entries=";
                perfDelta = perfDelta + entryCount.ToString();
                perfDelta = perfDelta + " generation=";
                perfDelta = perfDelta + generation.ToString();
                perfDelta = perfDelta + " payload_chars=";
                perfDelta = perfDelta + payloadChars.ToString();
                Print(perfDelta);
            }
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
            int vanillaDeferredIndex = m_DeferredBroadcastVanillaIds.Find(ownerDeviceId);
            if (vanillaDeferredIndex < 0)
            {
                m_DeferredBroadcastVanillaIds.Insert(ownerDeviceId);
                m_DeferredBroadcastVanillaObjs.Insert(ownerObj);
            }
            else
            {
                m_DeferredBroadcastVanillaObjs[vanillaDeferredIndex] = ownerObj;
            }
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
        int vanillaSnapshotGeneration = -1;

        m_ReusableBroadcastPlayers.Clear();
        g_Game.GetPlayers(m_ReusableBroadcastPlayers);

        int low = 0;
        int high = 0;
        ownerObj.GetNetworkID(low, high);

        // v0.7.11 (A3): Precompute squared threshold for player distance culling.
        float vSyncMaxDist = LFPG_CULL_DISTANCE_M + 20.0;
        float vSyncMaxDistSq = vSyncMaxDist * vSyncMaxDist;
        vector ownerObjPos = ownerObj.GetPosition();

        // v0.7.35 B-CRIT2: Collect target positions from vanilla wires
        m_ReusableBroadcastPositions.Clear();
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
                    m_ReusableBroadcastPositions.Insert(targetObj.GetPosition());
                }
            }
        }

        int i;
        for (i = 0; i < m_ReusableBroadcastPlayers.Count(); i = i + 1)
        {
            PlayerBase pb = PlayerBase.Cast(m_ReusableBroadcastPlayers[i]);
            if (!pb) continue;

            vector playerPos = pb.GetPosition();

            // v0.7.11 (A3): Compare in squared domain — eliminates sqrt per player.
            bool inRange = (LFPG_WorldUtil.DistSq(playerPos, ownerObjPos) <= vSyncMaxDistSq);

            // v0.7.35 B-CRIT2: Also check distance to target devices
            if (!inRange)
            {
                int tp;
                for (tp = 0; tp < m_ReusableBroadcastPositions.Count(); tp = tp + 1)
                {
                    if (LFPG_WorldUtil.DistSq(playerPos, m_ReusableBroadcastPositions[tp]) <= vSyncMaxDistSq)
                    {
                        inRange = true;
                        break;
                    }
                }
            }

            if (!inRange) continue;

            ScriptRPC rpc = new ScriptRPC();
            rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES_V2);
            rpc.Write(ownerDeviceId);
            rpc.Write(low);
            rpc.Write(high);
            rpc.Write(json);
            rpc.Write(vanillaSnapshotGeneration);
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
        int vanillaUnicastGeneration = -1;

        int low = 0;
        int high = 0;
        ownerObj.GetNetworkID(low, high);

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES_V2);
        rpc.Write(ownerDeviceId);
        rpc.Write(low);
        rpc.Write(high);
        rpc.Write(json);
        rpc.Write(vanillaUnicastGeneration);
        bool bRpcGuaranteed = true;
        PlayerIdentity noExclude = null;
        rpc.Send(player, LFPG_RPC_CHANNEL, bRpcGuaranteed, noExclude);

        if (LFPG_PERFDIAG_ENABLED)
        {
            m_PerfDiagOwnerSnapshotUnicastCount = m_PerfDiagOwnerSnapshotUnicastCount + 1;
            string perfSnapshot = "LFPG_PERFDIAG snapshot_unicast count=";
            perfSnapshot = perfSnapshot + m_PerfDiagOwnerSnapshotUnicastCount.ToString();
            perfSnapshot = perfSnapshot + " deviceId=";
            perfSnapshot = perfSnapshot + ownerDeviceId;
            perfSnapshot = perfSnapshot + " jsonLen=";
            perfSnapshot = perfSnapshot + json.Length().ToString();
            Print(perfSnapshot);
        }
    }

    // ===========================
    // Full sync to joining player
    // ===========================
    void SendFullSyncTo(PlayerBase player)
    {
        if (!player) return;
        if (m_FullSyncPlayer == player) return;
        if (m_FullSyncPendingPlayers.Find(player) >= 0) return;

        bool wasIdle = !m_FullSyncInProgress;
        m_FullSyncPendingPlayers.Insert(player);
        m_FullSyncInProgress = true;
        if (!m_StartupValidationDone || m_ValidationActive)
            return;
        if (wasIdle && m_StartupValidationDone && !m_ValidationActive)
            LFPG_StartNextFullSync();
    }

    protected void LFPG_StartNextFullSync()
    {
        m_FullSyncPlayer = null;
        m_FullSyncInProgress = false;
        FlushDeferredBroadcasts();
        while (m_FullSyncPendingPlayers.Count() > 0 && !m_FullSyncPlayer)
        {
            PlayerBase candidate = m_FullSyncPendingPlayers[0];
            m_FullSyncPendingPlayers.Remove(0);
            if (candidate && candidate.GetIdentity())
                m_FullSyncPlayer = candidate;
        }

        if (!m_FullSyncPlayer)
        {
            return;
        }

        m_FullSyncInProgress = true;
        m_FullSyncOwners.Clear();
        LFPG_DeviceRegistry.Get().GetAll(m_FullSyncOwners);
        m_FullSyncVanillaIds.Clear();
        int vanillaSnapshotIndex;
        for (vanillaSnapshotIndex = 0; vanillaSnapshotIndex < m_VanillaWires.Count(); vanillaSnapshotIndex = vanillaSnapshotIndex + 1)
            m_FullSyncVanillaIds.Insert(m_VanillaWires.GetKey(vanillaSnapshotIndex));
        m_FullSyncOwnerCursor = 0;
        m_FullSyncVanillaCursor = 0;
        m_FullSyncPlayerPos = m_FullSyncPlayer.GetPosition();
        float maxDist = LFPG_CULL_DISTANCE_M + 20.0;
        m_FullSyncMaxDistSq = maxDist * maxDist;

        if (LFPG_PERFDIAG_ENABLED)
        {
            string startMsg = "[FullSync] queued devices=";
            startMsg = startMsg + m_FullSyncOwners.Count().ToString();
            startMsg = startMsg + " playerPos=";
            startMsg = startMsg + m_FullSyncPlayerPos.ToString();
            LFPG_Util.Info(startMsg);
        }
    }
    protected void LFPG_SendFullSyncOwner(EntityAI owner)
    {
        if (!owner) return;
        if (!LFPG_DeviceAPI.HasWireStore(owner)) return;
        if (LFPG_WorldUtil.DistSq(m_FullSyncPlayerPos, owner.GetPosition()) > m_FullSyncMaxDistSq) return;

        string devId = LFPG_DeviceAPI.GetDeviceId(owner);
        string json = LFPG_DeviceAPI.GetWiresJSON(owner);
        LFPG_WireOwnerBase wireOwner = LFPG_WireOwnerBase.Cast(owner);
        int generation = -1;
        if (wireOwner)
            generation = wireOwner.LFPG_GetWireGeneration();

        int low = 0;
        int high = 0;
        owner.GetNetworkID(low, high);

        if (LFPG_PERFDIAG_ENABLED)
        {
            string devMsg = "[FullSync] LFPG dev=";
            devMsg = devMsg + devId;
            devMsg = devMsg + " net=" + low.ToString() + ":" + high.ToString();
            devMsg = devMsg + " type=" + owner.GetType();
            devMsg = devMsg + " jsonLen=" + json.Length().ToString();
            LFPG_Util.Info(devMsg);
        }
        if (json.Length() > 12000)
        {
            string fsWarn = "[FullSync] LARGE BLOB dev=" + devId + " jsonLen=" + json.Length().ToString() + " — approaching RPC limit";
            LFPG_Util.Warn(fsWarn);
        }

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES_V2);
        rpc.Write(devId);
        rpc.Write(low);
        rpc.Write(high);
        rpc.Write(json);
        rpc.Write(generation);
        bool guaranteed = true;
        PlayerIdentity noExclude = null;
        rpc.Send(m_FullSyncPlayer, LFPG_RPC_CHANNEL, guaranteed, noExclude);
    }
    protected void LFPG_ProcessFullSyncSpread()
    {
        if (!m_StartupValidationDone || m_ValidationActive)
            return;

        if (!m_FullSyncInProgress)
        {
            if (m_StartupValidationDone && !m_ValidationActive && m_FullSyncPendingPlayers.Count() > 0)
                LFPG_StartNextFullSync();
            return;
        }

        if (!m_FullSyncPlayer || !m_FullSyncPlayer.GetIdentity())
        {
            LFPG_StartNextFullSync();
            return;
        }

        int examined = 0;
        while (examined < LFPG_FULLSYNC_SENDS_PER_TICK && m_FullSyncOwnerCursor < m_FullSyncOwners.Count())
        {
            EntityAI owner = m_FullSyncOwners[m_FullSyncOwnerCursor];
            m_FullSyncOwnerCursor = m_FullSyncOwnerCursor + 1;
            examined = examined + 1;
            LFPG_SendFullSyncOwner(owner);
        }

        while (examined < LFPG_FULLSYNC_SENDS_PER_TICK && m_FullSyncOwnerCursor >= m_FullSyncOwners.Count() && m_FullSyncVanillaCursor < m_FullSyncVanillaIds.Count())
        {
            string vanillaId = m_FullSyncVanillaIds[m_FullSyncVanillaCursor];
            m_FullSyncVanillaCursor = m_FullSyncVanillaCursor + 1;
            examined = examined + 1;
            EntityAI vanillaObj = LFPG_DeviceRegistry.Get().FindById(vanillaId);
            if (!vanillaObj) continue;
            if (LFPG_WorldUtil.DistSq(m_FullSyncPlayerPos, vanillaObj.GetPosition()) > m_FullSyncMaxDistSq) continue;
            SendVanillaWiresTo(m_FullSyncPlayer, vanillaId, vanillaObj);
        }

        if (m_FullSyncOwnerCursor >= m_FullSyncOwners.Count() && m_FullSyncVanillaCursor >= m_FullSyncVanillaIds.Count())
            LFPG_StartNextFullSync();
    }
    // v0.7.38 (RC-05): Flush broadcasts that were deferred during FullSync.
    // Called immediately after m_FullSyncInProgress is cleared.
    // Re-broadcasts the LATEST state for each deferred owner, not the state
    // at deferral time, ensuring clients always receive current data.
    protected void FlushDeferredBroadcasts()
    {
        int snapshotIndex;
        LFPG_OwnerBroadcastSnapshot snapshot;
        bool pendingOwner;
        for (snapshotIndex = 0; snapshotIndex < m_DeferredOwnerSnapshots.Count(); snapshotIndex = snapshotIndex + 1)
        {
            snapshot = m_DeferredOwnerSnapshots.GetElement(snapshotIndex);
            if (snapshot)
            {
                pendingOwner = m_PendingOwnerSnapshots.Contains(snapshot.m_OwnerDeviceId);
                if (!pendingOwner)
                    pendingOwner = m_PendingBroadcastLFPG.Contains(snapshot.m_OwnerDeviceId);
                if (!pendingOwner)
                    BroadcastOwnerSnapshot(snapshot);
            }
        }
        m_DeferredOwnerSnapshots.Clear();

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
        if (!player || deviceId == "")
            return;

        map<string, bool> sentOwners = new map<string, bool>;
        SendDeviceSyncToBatched(player, deviceId, sentOwners);
    }

    void SendDeviceSyncToBatched(PlayerBase player, string deviceId, map<string, bool> sentOwners)
    {
        if (!player || deviceId == "" || !sentOwners)
            return;

        LFPG_DeviceRegistry reg = LFPG_DeviceRegistry.Get();
        if (!reg)
            return;

        bool hasRelevantState = false;

        // 1. Send state owned directly by the requested device.
        EntityAI deviceObj = reg.FindById(deviceId);
        if (deviceObj && LFPG_DeviceAPI.HasWireStore(deviceObj))
        {
            hasRelevantState = true;
            if (!sentOwners.Contains(deviceId))
            {
                SendOwnerBlobTo(player, deviceObj, deviceId);
                sentOwners[deviceId] = true;
            }
        }
        else if (deviceObj && m_VanillaWires.Contains(deviceId))
        {
            hasRelevantState = true;
            if (!sentOwners.Contains(deviceId))
            {
                SendVanillaWiresTo(player, deviceId, deviceObj);
                sentOwners[deviceId] = true;
            }
        }

        // 2. Resolve incoming owners directly from the graph reverse relationship.
        // No registry-wide or wire-owner-wide scan is performed here.
        if (m_Graph)
        {
            ref array<ref LFPG_ElecEdge> incomingEdges = m_Graph.GetIncoming(deviceId);
            if (incomingEdges)
            {
                int i;
                for (i = 0; i < incomingEdges.Count(); i = i + 1)
                {
                    LFPG_ElecEdge edge = incomingEdges[i];
                    if (!edge || edge.m_SourceNodeId == "")
                        continue;

                    string ownerId = edge.m_SourceNodeId;
                    if (sentOwners.Contains(ownerId))
                    {
                        hasRelevantState = true;
                        continue;
                    }

                    EntityAI ownerObj = reg.FindById(ownerId);
                    if (!ownerObj)
                        continue;

                    if (LFPG_DeviceAPI.HasWireStore(ownerObj))
                    {
                        hasRelevantState = true;
                        SendOwnerBlobTo(player, ownerObj, ownerId);
                        sentOwners[ownerId] = true;
                    }
                    else if (m_VanillaWires.Contains(ownerId))
                    {
                        hasRelevantState = true;
                        SendVanillaWiresTo(player, ownerId, ownerObj);
                        sentOwners[ownerId] = true;
                    }
                }
            }
        }

        if (deviceObj && !hasRelevantState)
        {
            SendEmptyDeviceCableStateTo(player, deviceObj, deviceId);
        }

        string sdMsg = "[SendDeviceSyncTo] Completed for deviceId=" + deviceId;
        LFPG_Util.Info(sdMsg);
    }

    // Helper: authoritative empty state for a known device with no cable relationship.
    void SendEmptyDeviceCableStateTo(PlayerBase player, EntityAI deviceObj, string deviceId)
    {
        if (!player || !deviceObj || deviceId == "")
            return;

        int low = 0;
        int high = 0;
        deviceObj.GetNetworkID(low, high);
        string json = LFPG_WireHelper.GetJSON(null);
        int emptyGeneration = -1;

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES_V2);
        rpc.Write(deviceId);
        rpc.Write(low);
        rpc.Write(high);
        rpc.Write(json);
        rpc.Write(emptyGeneration);
        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);

        if (LFPG_PERFDIAG_ENABLED)
        {
            m_PerfDiagOwnerSnapshotUnicastCount = m_PerfDiagOwnerSnapshotUnicastCount + 1;
            string perfSnapshot = "LFPG_PERFDIAG snapshot_unicast count=";
            perfSnapshot = perfSnapshot + m_PerfDiagOwnerSnapshotUnicastCount.ToString();
            perfSnapshot = perfSnapshot + " deviceId=";
            perfSnapshot = perfSnapshot + deviceId;
            perfSnapshot = perfSnapshot + " jsonLen=";
            perfSnapshot = perfSnapshot + json.Length().ToString();
            Print(perfSnapshot);
        }
    }

    // Helper: unicast a single owner's wire blob to one player
    void SendOwnerBlobTo(PlayerBase player, EntityAI ownerObj, string ownerId)
    {
        if (!player || !ownerObj || ownerId == "") return;

        string json = LFPG_DeviceAPI.GetWiresJSON(ownerObj);
        LFPG_WireOwnerBase ownerWireState = LFPG_WireOwnerBase.Cast(ownerObj);
        int ownerGeneration = -1;
        if (ownerWireState)
        {
            ownerGeneration = ownerWireState.LFPG_GetWireGeneration();
        }

        int low = 0;
        int high = 0;
        ownerObj.GetNetworkID(low, high);

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES_V2);
        rpc.Write(ownerId);
        rpc.Write(low);
        rpc.Write(high);
        rpc.Write(json);
        rpc.Write(ownerGeneration);
        bool bRpcGuaranteed = true;
        PlayerIdentity noExclude = null;
        rpc.Send(player, LFPG_RPC_CHANNEL, bRpcGuaranteed, noExclude);

        if (LFPG_PERFDIAG_ENABLED)
        {
            m_PerfDiagOwnerSnapshotUnicastCount = m_PerfDiagOwnerSnapshotUnicastCount + 1;
            string perfSnapshot = "LFPG_PERFDIAG snapshot_unicast count=";
            perfSnapshot = perfSnapshot + m_PerfDiagOwnerSnapshotUnicastCount.ToString();
            perfSnapshot = perfSnapshot + " deviceId=";
            perfSnapshot = perfSnapshot + ownerId;
            perfSnapshot = perfSnapshot + " jsonLen=";
            perfSnapshot = perfSnapshot + json.Length().ToString();
            Print(perfSnapshot);
        }

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

        if (remaining > 0 || m_Graph.GetLastEdgesVisited() > 0)
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
        float nowMs = g_Game.GetTime();
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
        if (!g_Game.IsServer())
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
        if (!g_Game.IsServer())
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
        if (!g_Game.IsServer())
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
        if (!g_Game.IsServer())
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

        // --- 2. Clear owned wires: reverse index + player quota + graph ---
        // ReverseIdxRemove mirrors an unambiguous outgoing edge into the graph.
        // Ambiguous duplicates set m_GraphFullRebuildRequired instead.
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

        // --- 4. Graph node cleanup ---
        // Bug #18 fix: OnDeviceRemoved was removed. Outgoing edges are now
        // cleaned in step 2 via OnWireRemoved; incoming edges in step 3 via
        // RemoveWiresTargeting. OnWireRemoved calls CleanupOrphanNode which
        // removes the node once all edges are gone — no separate call needed.

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
            // MarkVanillaDirty defers writes for at most 5s. If server crashes
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
        if (!g_Game.IsServer())
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
        m_ReusableMovedOldPositions.Clear();
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
                    m_ReusableMovedOldPositions.Insert(lastPos);
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
            vector movedOldPosition = m_ReusableMovedOldPositions[mi];

            string mvMsg = "[Movement] Device " + movedId + " type=" + movedDev.GetType() + " moved — disconnecting wires";
            LFPG_Util.Warn(mvMsg);

            // CutAllWiresFromDevice handles: owned wires, vanilla wires,
            // incoming wires, graph cleanup, SetPowered(false) on neighbors,
            // and auto-untrack via the hook at the end of CutAllWiresFromDevice.
            // movedId is the pre-move index key; vanilla cannot re-derive it.
            CutAllWiresFromMovedDevice(movedDev, movedOldPosition, movedId);

            // Generator-specific: force source off when physically moved.
            // CutAllWiresFromDevice handles consumers/passthroughs via
            // SetPowered(false), but generators produce (not consume).
            LFPG_Generator gen = LFPG_Generator.Cast(movedDev);
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
            RequestGlobalSelfHeal(true);
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
    void RequestGlobalSelfHeal(bool validationOnlyAfterCut = false)
    {
        #ifdef SERVER
        if (validationOnlyAfterCut && m_CutGraphRebuildQueued)
            m_ValidationAfterCutRequested = true;
        else if (validationOnlyAfterCut && !m_IndexHealAfterCut)
            return;
        if (m_SelfHealQueued) return;
        m_SelfHealQueued = true;
        bool bOnce = false;
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DoGlobalSelfHeal, 500, bOnce);
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
        if (m_ValidationActive)
        {
            m_ValidationRerunRequested = true;
            return;
        }

        m_ValidationActive = true;
        m_ValidationRerunRequested = false;
        m_ValidationPhase = LFPG_VALIDATE_RESOLVE_VANILLA;
        m_ValidationCursor = 0;
        m_ValidationStartMs = g_Game.GetTime();
        m_ValidationOwnersPruned = 0;
        m_ValidationSkipGraphRebuild = m_ValidationOnlyHealPending;
        m_ValidationOnlyHealPending = false;
        m_ValidationGraphGeneration = m_GraphRebuildGeneration;
        m_ValidationDevices.Clear();
        m_ValidationValidIds.Clear();
        m_ValidationVanillaIds.Clear();
        int vanillaSnapshotIndex;
        for (vanillaSnapshotIndex = 0; vanillaSnapshotIndex < m_VanillaWires.Count(); vanillaSnapshotIndex = vanillaSnapshotIndex + 1)
            m_ValidationVanillaIds.Insert(m_VanillaWires.GetKey(vanillaSnapshotIndex));
        m_CachedValidIds = null;
        LFPG_Util.Info("[SelfHeal] Phased validation scheduled");
        #endif
    }
    protected void LFPG_ValidationResolveVanillaOwners()
    {
        int end = Math.Min(m_ValidationCursor + LFPG_STARTUP_VALIDATE_OWNERS_PER_TICK, m_ValidationVanillaIds.Count());
        int vi;
        for (vi = m_ValidationCursor; vi < end; vi = vi + 1)
        {
            string ownerId = m_ValidationVanillaIds[vi];
            array<ref LFPG_WireData> wires;
            if (!m_VanillaWires.Find(ownerId, wires) || !wires)
                continue;
            if (!LFPG_DeviceRegistry.Get().FindById(ownerId))
                LFPG_DeviceAPI.ResolveVanillaDevice(ownerId);
            int wi;
            for (wi = 0; wi < wires.Count(); wi = wi + 1)
            {
                LFPG_WireData wd = wires[wi];
                if (!wd || wd.m_TargetDeviceId == "") continue;
                if (!LFPG_DeviceRegistry.Get().FindById(wd.m_TargetDeviceId))
                    LFPG_DeviceAPI.ResolveVanillaDevice(wd.m_TargetDeviceId);
            }
        }
        m_ValidationCursor = end;
        if (m_ValidationCursor >= m_ValidationVanillaIds.Count())
        {
            m_ValidationCursor = 0;
            m_ValidationPhase = LFPG_VALIDATE_SNAPSHOT_PRE;
        }
    }

    protected void LFPG_ValidationSnapshotPre()
    {
        LFPG_DeviceRegistry.Get().GetAll(m_ValidationDevices);
        m_ValidationCursor = 0;
        m_ValidationPhase = LFPG_VALIDATE_RESOLVE_LFPG;
    }

    protected void LFPG_ValidationResolveLFPGTargets()
    {
        int end = Math.Min(m_ValidationCursor + LFPG_STARTUP_VALIDATE_OWNERS_PER_TICK, m_ValidationDevices.Count());
        int di;
        for (di = m_ValidationCursor; di < end; di = di + 1)
        {
            ref array<ref LFPG_WireData> wires = LFPG_DeviceAPI.GetDeviceWires(m_ValidationDevices[di]);
            if (!wires) continue;
            int wi;
            for (wi = 0; wi < wires.Count(); wi = wi + 1)
            {
                LFPG_WireData wd = wires[wi];
                if (!wd) continue;
                if (wd.m_TargetDeviceId.IndexOf("vp:") != 0) continue;
                if (!LFPG_DeviceRegistry.Get().FindById(wd.m_TargetDeviceId))
                    LFPG_DeviceAPI.ResolveVanillaDevice(wd.m_TargetDeviceId);
            }
        }
        m_ValidationCursor = end;
        if (m_ValidationCursor >= m_ValidationDevices.Count())
        {
            m_ValidationCursor = 0;
            m_ValidationPhase = LFPG_VALIDATE_SNAPSHOT_FINAL;
        }
    }

    protected void LFPG_ValidationSnapshotFinal()
    {
        LFPG_DeviceRegistry.Get().PruneNullEntries();
        LFPG_DeviceRegistry.Get().GetAll(m_ValidationDevices);
        m_ValidationValidIds.Clear();
        m_CachedValidIds = m_ValidationValidIds;
        m_ValidationCursor = 0;
        m_ValidationPhase = LFPG_VALIDATE_BUILD_VALID;
    }
    protected void LFPG_ValidationBuildValidIds()
    {
        int end = Math.Min(m_ValidationCursor + LFPG_STARTUP_VALIDATE_OWNERS_PER_TICK, m_ValidationDevices.Count());
        int vi;
        for (vi = m_ValidationCursor; vi < end; vi = vi + 1)
        {
            string deviceId = LFPG_DeviceAPI.GetOrCreateDeviceId(m_ValidationDevices[vi]);
            if (deviceId != "")
                m_ValidationValidIds[deviceId] = true;
        }
        m_ValidationCursor = end;
        if (m_ValidationCursor >= m_ValidationDevices.Count())
        {
            m_ValidationCursor = 0;
            m_ValidationPhase = LFPG_VALIDATE_REFRESH_LFPG;
        }
    }

    protected void LFPG_ValidationRefreshLFPGNetworkIds()
    {
        int end = Math.Min(m_ValidationCursor + LFPG_STARTUP_VALIDATE_OWNERS_PER_TICK, m_ValidationDevices.Count());
        int di;
        for (di = m_ValidationCursor; di < end; di = di + 1)
        {
            EntityAI owner = m_ValidationDevices[di];
            ref array<ref LFPG_WireData> wires = LFPG_DeviceAPI.GetDeviceWires(owner);
            if (!wires) continue;
            int wi;
            for (wi = 0; wi < wires.Count(); wi = wi + 1)
            {
                LFPG_WireData wd = wires[wi];
                if (!wd) continue;
                EntityAI target = LFPG_DeviceRegistry.Get().FindById(wd.m_TargetDeviceId);
                if (target)
                {
                    int low = 0;
                    int high = 0;
                    target.GetNetworkID(low, high);
                    wd.m_TargetNetLow = low;
                    wd.m_TargetNetHigh = high;
                }
                else
                {
                    wd.m_TargetNetLow = 0;
                    wd.m_TargetNetHigh = 0;
                }
            }
            LFPG_WireOwnerBase cacheOwner = LFPG_WireOwnerBase.Cast(owner);
            if (cacheOwner)
                cacheOwner.LFPG_InvalidateWireJSONCache();
        }
        m_ValidationCursor = end;
        if (m_ValidationCursor >= m_ValidationDevices.Count())
        {
            m_ValidationCursor = 0;
            m_ValidationPhase = LFPG_VALIDATE_REFRESH_VANILLA;
        }
    }
    protected void LFPG_ValidationRefreshVanillaNetworkIds()
    {
        int end = Math.Min(m_ValidationCursor + LFPG_STARTUP_VALIDATE_OWNERS_PER_TICK, m_ValidationVanillaIds.Count());
        int vi;
        for (vi = m_ValidationCursor; vi < end; vi = vi + 1)
        {
            string ownerId = m_ValidationVanillaIds[vi];
            array<ref LFPG_WireData> wires;
            if (!m_VanillaWires.Find(ownerId, wires) || !wires)
                continue;
            int wi;
            for (wi = 0; wi < wires.Count(); wi = wi + 1)
            {
                LFPG_WireData wd = wires[wi];
                if (!wd) continue;
                EntityAI target = LFPG_DeviceRegistry.Get().FindById(wd.m_TargetDeviceId);
                if (target)
                {
                    int low = 0;
                    int high = 0;
                    target.GetNetworkID(low, high);
                    wd.m_TargetNetLow = low;
                    wd.m_TargetNetHigh = high;
                }
                else
                {
                    wd.m_TargetNetLow = 0;
                    wd.m_TargetNetHigh = 0;
                }
            }
        }
        m_ValidationCursor = end;
        if (m_ValidationCursor >= m_ValidationVanillaIds.Count())
        {
            m_ValidationCursor = 0;
            m_ValidationPhase = LFPG_VALIDATE_PRUNE_LFPG;
        }
    }

    protected void LFPG_ValidationPruneLFPGOwners()
    {
        // Phased validation starts about five seconds after mission init and
        // may rerun as late owners arrive. Until the dedicated delayed prune
        // completes, absence from any one registry snapshot is not proof of
        // deletion.
        if (!m_DeferredPruneCompleted)
        {
            m_CachedValidIds = null;
            m_ValidationCursor = 0;
            m_ValidationPhase = LFPG_VALIDATE_INDEX_REBUILD;
            LFPG_Util.Info("[SelfHeal] Initial LFPG owner pruning deferred for late-loading targets");
            return;
        }

        int end = Math.Min(m_ValidationCursor + LFPG_STARTUP_VALIDATE_OWNERS_PER_TICK, m_ValidationDevices.Count());
        int di;
        for (di = m_ValidationCursor; di < end; di = di + 1)
        {
            EntityAI owner = m_ValidationDevices[di];
            if (!LFPG_DeviceAPI.HasWireStore(owner)) continue;
            bool changed = LFPG_DeviceAPI.PruneDeviceMissingTargets(owner);
            if (changed)
            {
                m_ValidationOwnersPruned = m_ValidationOwnersPruned + 1;
                BroadcastOwnerWires(owner);
            }
        }
        m_ValidationCursor = end;
        if (m_ValidationCursor >= m_ValidationDevices.Count())
        {
            m_CachedValidIds = null;
            m_ValidationCursor = 0;
            m_ValidationPhase = LFPG_VALIDATE_INDEX_REBUILD;
        }
    }
    protected void LFPG_ValidationRebuildIndexes()
    {
        bool fullRequiredBeforeIndex = m_GraphFullRebuildRequired;
        RebuildReverseIdx();
        RecountAllPlayerWires();
        if (!fullRequiredBeforeIndex && (m_ValidationSkipGraphRebuild || m_GraphRebuildGeneration != m_ValidationGraphGeneration) && m_ValidationOwnersPruned <= 0)
            m_GraphFullRebuildRequired = false;
        m_ValidationPhase = LFPG_VALIDATE_SCHEDULE_PRUNE;
    }

    protected void LFPG_ValidationScheduleDeferredPrune()
    {
        if (!m_DeferredPruneScheduled)
        {
            m_DeferredPruneScheduled = true;
            bool noRepeat = false;
            g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DeferredVanillaPruneAndRebuild, 30000, noRepeat);
            LFPG_Util.Info("[SelfHeal] Vanilla wire pruning deferred until after phased validation");
        }
        m_ValidationPhase = LFPG_VALIDATE_GRAPH_REBUILD;
    }

    protected void LFPG_ValidationRebuildGraph()
    {
        bool graphRebuiltAfterValidationStart = m_GraphRebuildGeneration != m_ValidationGraphGeneration;
        if (m_Graph && ((!m_ValidationSkipGraphRebuild && !graphRebuiltAfterValidationStart) || m_ValidationOwnersPruned > 0 || m_GraphFullRebuildRequired))
        {
            m_Graph.RebuildFromWires(this);
            m_GraphFullRebuildRequired = false;
            m_GraphRebuildGeneration = m_GraphRebuildGeneration + 1;
        }
        else if (m_Graph)
            LFPG_Util.Info("[SelfHeal] Graph rebuild skipped; preceding CutAll rebuild remains authoritative");
        m_ValidationSkipGraphRebuild = false;
        m_ValidationPhase = LFPG_VALIDATE_GRAPH_POPULATE;
    }

    protected void LFPG_ValidationPopulateGraph()
    {
        if (m_Graph)
            m_Graph.PopulateAllNodeElecStates();
        m_ValidationPhase = LFPG_VALIDATE_GRAPH_MARK;
    }

    protected void LFPG_ValidationMarkGraph()
    {
        if (m_Graph)
        {
            m_Graph.MarkSourcesDirty();
            m_WarmupActive = true;
            int flushBudget = LFPG_PROPAGATE_WARMUP_BUDGET;
            int flushEdge = LFPG_PROPAGATE_EDGE_WARMUP_BUDGET;
            m_Graph.ProcessDirtyQueue(flushBudget, flushEdge);
            LFPG_Util.Info("[SelfHeal] Graph warmup phase complete");
        }
        m_ValidationPhase = LFPG_VALIDATE_PRUNE_POSITIONS;
    }

    protected void LFPG_ValidationPrunePositions()
    {
        PruneStaleLastKnownPositions();
        m_ValidationPhase = LFPG_VALIDATE_REBUILD_TRACKED;
    }

    protected void LFPG_ValidationRebuildTracked()
    {
        RebuildTrackedDevices();
        m_ValidationPhase = LFPG_VALIDATE_FINALIZE;
    }
    protected void LFPG_ValidationFinalize()
    {
        int durationMs = g_Game.GetTime() - m_ValidationStartMs;
        int deviceCount = m_ValidationDevices.Count();
        m_ValidationActive = false;

        if (m_ValidationRerunRequested)
        {
            m_ValidationRerunRequested = false;
            LFPG_Util.Info("[SelfHeal] Changes arrived during validation; scheduling one more phased pass");
            ValidateAllWiresAndPropagate();
            return;
        }

        bool wasStartupPending = !m_StartupValidationDone;
        m_StartupValidationDone = true;
        if (wasStartupPending)
            LFPG_Util.Info("[SelfHeal] Startup validation done — RPCs enabled");

        string summary = "[SelfHeal] phased summary duration_ms=";
        summary = summary + durationMs.ToString();
        summary = summary + " devices=" + deviceCount.ToString();
        summary = summary + " owners_pruned=" + m_ValidationOwnersPruned.ToString();
        LFPG_Util.Info(summary);

        if (m_FullSyncPendingPlayers.Count() > 0)
            LFPG_StartNextFullSync();
    }

    protected void LFPG_ProcessStartupValidationSlice()
    {
        if (!m_ValidationActive)
            return;

        if (m_ValidationPhase == LFPG_VALIDATE_RESOLVE_VANILLA)
        {
            LFPG_ValidationResolveVanillaOwners();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_SNAPSHOT_PRE)
        {
            LFPG_ValidationSnapshotPre();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_RESOLVE_LFPG)
        {
            LFPG_ValidationResolveLFPGTargets();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_SNAPSHOT_FINAL)
        {
            LFPG_ValidationSnapshotFinal();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_BUILD_VALID)
        {
            LFPG_ValidationBuildValidIds();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_REFRESH_LFPG)
        {
            LFPG_ValidationRefreshLFPGNetworkIds();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_REFRESH_VANILLA)
        {
            LFPG_ValidationRefreshVanillaNetworkIds();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_PRUNE_LFPG)
        {
            LFPG_ValidationPruneLFPGOwners();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_INDEX_REBUILD)
        {
            LFPG_ValidationRebuildIndexes();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_SCHEDULE_PRUNE)
        {
            LFPG_ValidationScheduleDeferredPrune();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_GRAPH_REBUILD)
        {
            LFPG_ValidationRebuildGraph();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_GRAPH_POPULATE)
        {
            LFPG_ValidationPopulateGraph();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_GRAPH_MARK)
        {
            LFPG_ValidationMarkGraph();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_PRUNE_POSITIONS)
        {
            LFPG_ValidationPrunePositions();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_REBUILD_TRACKED)
        {
            LFPG_ValidationRebuildTracked();
            return;
        }
        LFPG_ValidationFinalize();
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
    protected int PruneUnresolvableVanillaWires()
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
        return totalPruned;
        #else
        return 0;
        #endif
    }

    // v4.7: Deferred vanilla wire prune + rebuild.
    // Fires ~35s post-init (30s after initial validation at 5s).
    // Re-resolves ALL vanilla wire endpoints, prunes truly dead wires,
    // and rebuilds the graph with the clean state.
    // Reason: At 5s post-init, DayZ may not have loaded all entities yet.
    // Pruning at 5s permanently deletes wires to late-loading devices.
    // By 35s, all entities should be positioned and resolvable.
    protected void DeferredVanillaPruneAndRebuild()
    {
        #ifdef SERVER
        int deferredStartMs = g_Game.GetTime();
        int newlyResolved = 0;
        LFPG_Util.Info("[DeferredPrune] Starting deferred vanilla wire validation...");

        // Step 1: Re-resolve all vanilla wire endpoints (same as Step 1 in ValidateAllWiresAndPropagate)
        int vr;
        for (vr = 0; vr < m_VanillaWires.Count(); vr = vr + 1)
        {
            string vrOwnerId = m_VanillaWires.GetKey(vr);
            if (!LFPG_DeviceRegistry.Get().FindById(vrOwnerId))
            {
                EntityAI deferredOwnerResolved = LFPG_DeviceAPI.ResolveVanillaDevice(vrOwnerId);
                if (deferredOwnerResolved)
                {
                    newlyResolved = newlyResolved + 1;
                }
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
                            EntityAI deferredTargetResolved = LFPG_DeviceAPI.ResolveVanillaDevice(vrWd.m_TargetDeviceId);
                            if (deferredTargetResolved)
                            {
                                newlyResolved = newlyResolved + 1;
                            }
                        }
                    }
                }
            }
        }

        // Also re-resolve LFPG device wire targets that are vanilla
        array<EntityAI> allDev = new array<EntityAI>;
        LFPG_DeviceRegistry.Get().GetAll(allDev);
        int pa;
        for (pa = 0; pa < allDev.Count(); pa = pa + 1)
        {
            ref array<ref LFPG_WireData> lfWires = LFPG_DeviceAPI.GetDeviceWires(allDev[pa]);
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
                        EntityAI deferredLFPGTargetResolved = LFPG_DeviceAPI.ResolveVanillaDevice(tid);
                        if (deferredLFPGTargetResolved)
                        {
                            newlyResolved = newlyResolved + 1;
                        }
                    }
                }
            }
        }

        // Step 2: Now prune — entities that STILL can't be resolved are truly gone
        int vanillaPruneCount = PruneUnresolvableVanillaWires();

        // Step 3: Also prune LFPG device wires targeting unresolvable vanilla devices
        LFPG_DeviceRegistry.Get().PruneNullEntries();
        array<EntityAI> allAfterPrune = new array<EntityAI>;
        LFPG_DeviceRegistry.Get().GetAll(allAfterPrune);

        m_CachedValidIds = new map<string, bool>;
        int vi;
        for (vi = 0; vi < allAfterPrune.Count(); vi = vi + 1)
        {
            string did = LFPG_DeviceAPI.GetOrCreateDeviceId(allAfterPrune[vi]);
            if (did != "")
            {
                m_CachedValidIds[did] = true;
            }
        }

        int pruneCount = 0;
        int pi;
        for (pi = 0; pi < allAfterPrune.Count(); pi = pi + 1)
        {
            if (!LFPG_DeviceAPI.HasWireStore(allAfterPrune[pi])) continue;
            bool changed = LFPG_DeviceAPI.PruneDeviceMissingTargets(allAfterPrune[pi]);
            if (changed)
            {
                pruneCount = pruneCount + 1;
                BroadcastOwnerWires(allAfterPrune[pi]);
            }
        }
        m_CachedValidIds = null;
        m_DeferredPruneCompleted = true;

        bool vanillaDirtyBeforeFlush = m_VanillaDirty;
        bool needsDeferredRebuild = false;
        if (newlyResolved > 0 || pruneCount > 0 || vanillaPruneCount > 0 || vanillaDirtyBeforeFlush)
        {
            needsDeferredRebuild = true;
        }

        // Step 4: Flush vanilla wires if anything changed
        if (m_VanillaDirty)
        {
            FlushVanillaIfDirty();
        }

        string deferredRebuildResult = "skipped";
        if (needsDeferredRebuild)
        {
            deferredRebuildResult = "executed";

            // Step 5: Rebuild graph with clean state (includes newly-resolved vanilla devices)
            if (m_Graph)
            {
                m_Graph.RebuildFromWires(this);
                m_GraphRebuildGeneration = m_GraphRebuildGeneration + 1;
                m_Graph.PopulateAllNodeElecStates();
                m_Graph.MarkSourcesDirty();

                int flushBudget = LFPG_PROPAGATE_WARMUP_BUDGET;
                int flushEdge = LFPG_PROPAGATE_EDGE_WARMUP_BUDGET;
                m_Graph.ProcessDirtyQueue(flushBudget, flushEdge);
            }

            // Step 6: Rebuild indexes
            RebuildReverseIdx();
            m_GraphFullRebuildRequired = false;
            RebuildTrackedDevices();
        }
        else
        {
            LFPG_Util.Info("[DeferredPrune] rebuild=skipped newly_resolved=0 lfpg_pruned=0 vanilla_pruned=0 vanilla_dirty=0");
        }

        int deferredDurationMs = g_Game.GetTime() - deferredStartMs;
        int vanillaDirtySummary = 0;
        if (vanillaDirtyBeforeFlush)
        {
            vanillaDirtySummary = 1;
        }
        string deferredSummary = "[DeferredPrune] summary duration_ms=" + deferredDurationMs.ToString() + " newly_resolved=" + newlyResolved.ToString() + " lfpg_pruned=" + pruneCount.ToString() + " vanilla_pruned=" + vanillaPruneCount.ToString() + " vanilla_dirty_before_flush=" + vanillaDirtySummary.ToString() + " rebuild=" + deferredRebuildResult;
        LFPG_Util.Info(deferredSummary);
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
    // Called every LFPG_VANILLA_FLUSH_S seconds (5s).
    // v4.7: Promoted from protected to public — also called from
    // RPCServerHandler.HandleFinishWiring for immediate vanilla persistence.
    void FlushVanillaIfDirty()
    {
        #ifdef SERVER
        if (!m_VanillaDirty)
            return;

        bool saveOk = SaveVanillaWires();
        if (saveOk)
        {
            m_VanillaDirty = false;
            m_VanillaSaveFailureCount = 0;
            return;
        }

        m_VanillaDirty = true;
        m_VanillaSaveFailureCount = m_VanillaSaveFailureCount + 1;
        int now = g_Game.GetTime();
        if (m_VanillaSaveFailureCount == 1 || now - m_LastVanillaSaveFailureWarnMs >= LFPG_VANILLA_SAVE_WARN_INTERVAL_MS)
        {
            string retryWarn = "[VanillaWires] Save failed; dirty state retained for retry. Cumulative failed flushes: ";
            retryWarn = retryWarn + m_VanillaSaveFailureCount.ToString();
            LFPG_Util.Warn(retryWarn);
            m_LastVanillaSaveFailureWarnMs = now;
        }
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
            bool saveOk = SaveVanillaWires();
            if (saveOk)
            {
                m_VanillaDirty = false;
            }
            else
            {
                m_VanillaDirty = true;
                LFPG_Util.Warn("[VanillaWires] Shutdown flush failed; dirty state remains in memory.");
            }
        }
        #endif
    }

    protected bool SaveVanillaWires()
    {
        #ifdef SERVER
        // v0.7.16 H6: Don't overwrite if file was from a newer schema version.
        // Saving would strip unknown fields and downgrade the version marker.
        if (m_VanillaReadOnly)
        {
            string saveBlockMsg = "[VanillaWires] SAVE BLOCKED: loaded from schema v" + m_VanillaLoadedVer.ToString() + " > current v" + LFPG_VANILLA_PERSIST_VER.ToString() + ". Upgrade the mod to save changes.";
            LFPG_Util.Warn(saveBlockMsg);
            // Failure keeps m_VanillaDirty set. Returning success here falsely
            // acknowledged data that was never written and suppressed shutdown
            // warnings/retries.
            return false;
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
        bool saveOk = LFPG_FileUtil.AtomicSaveVanillaWires(VANILLA_WIRES_FILE, store);
        if (saveOk)
        {
            string vSaveMsg = "[VanillaWires] Saved " + store.entries.Count().ToString() + " entries (atomic)";
            LFPG_Util.Info(vSaveMsg);
        }
        else
        {
            string vSaveErr = "[VanillaWires] Atomic save failed!";
            LFPG_Util.Error(vSaveErr);
        }
        return saveOk;
        #endif
        return true;
    }

    protected void LoadVanillaWires()
    {
        #ifdef SERVER
        // PR-A: typed recovery prefers parseable orphan .tmp over .bak.new/.bak.
        if (!LFPG_FileUtil.EnsureVanillaWiresFileOrRestore(VANILLA_WIRES_FILE))
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
        else
        {
            m_VanillaLoadedVer = LFPG_Migrators.MigrateVanillaStore(store);
        }

        int loaded = 0;
        int discarded = 0;
        int duplicates = 0;

        // v0.7.16 H3: Map-based O(N) dedup per owner instead of O(N²) IsDuplicate
        ref map<string, ref map<string, bool>> dedupByOwner = new map<string, ref map<string, bool>>;

        int storedEntryCount = store.entries.Count();
        int loadEntryCount = storedEntryCount;
        if (loadEntryCount > LFPG_VANILLA_PERSIST_MAX_ENTRIES)
        {
            loadEntryCount = LFPG_VANILLA_PERSIST_MAX_ENTRIES;
            m_VanillaReadOnly = true;
            string capWarn = "[VanillaWires] Entry cap applied: file=" + storedEntryCount.ToString();
            capWarn = capWarn + " cap=" + LFPG_VANILLA_PERSIST_MAX_ENTRIES.ToString();
            capWarn = capWarn + ". Store is read-only to prevent truncating the profile file.";
            LFPG_Util.Warn(capWarn);
        }

        int i;
        for (i = 0; i < loadEntryCount; i = i + 1)
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

        string loadMsg = "[VanillaWires] Loaded " + loaded.ToString() + " entries from " + storedEntryCount.ToString();
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
	
    // previousOwnerPosition is snapshot interest only. knownDeviceId is the
    // pre-move index key and must not be recomputed from the new position.
    void CutAllWiresFromMovedDevice(EntityAI device, vector previousOwnerPosition, string knownDeviceId = "")
    {
        m_CutAllHasPreviousOwnerPosition = true;
        m_CutAllPreviousOwnerPosition = previousOwnerPosition;
        CutAllWiresFromDevice(device, knownDeviceId);
        m_CutAllHasPreviousOwnerPosition = false;
    }

	void CutAllWiresFromDevice(EntityAI device, string knownDeviceId = "")
    {
        #ifdef SERVER
        if (!device)
            return;

        // knownDeviceId is required when the live entity cannot yield the
        // indexed id (vanilla vp: after a move, or a tearing-down object).
        string deviceId = knownDeviceId;
        if (deviceId == "")
            deviceId = LFPG_DeviceAPI.GetDeviceId(device);
        if (deviceId == "")
            return;

        bool anyChanged = false;
        bool reverseIndexConsistent = false;
        ref map<string, int> graphIncomingByPort = new map<string, int>;
        int portCount = LFPG_DeviceAPI.GetPortCount(device);
        ref map<string, bool> declaredInputPorts = new map<string, bool>;
        int declaredPortIndex;
        for (declaredPortIndex = 0; declaredPortIndex < portCount; declaredPortIndex = declaredPortIndex + 1)
        {
            if (LFPG_DeviceAPI.GetPortDir(device, declaredPortIndex) != LFPG_PortDir.IN)
                continue;

            string declaredPortName = LFPG_DeviceAPI.GetPortName(device, declaredPortIndex);
            if (declaredPortName == "")
                declaredPortName = "input_main";
            declaredInputPorts.Set(deviceId + "|" + declaredPortName, true);
        }

        // --- v0.7.28 (Bug 2+3): Collect all graph neighbors BEFORE cutting ---
        // When wires are cut, the graph node for this device gets removed.
        // If we don't force SetPowered(false) on neighbors first, they
        // become orphan nodes that never receive a powered=false update.
        // Collect now while the graph still has the edges.
        ref array<string> neighborIds = new array<string>;
        if (m_Graph)
        {
            reverseIndexConsistent = m_ReverseIndexTrusted;
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

                        string incomingPort = piEdge.m_TargetPort;
                        if (incomingPort == "")
                        {
                            incomingPort = "input_main";
                            // A raw legacy empty port cannot be removed by the
                            // exact-port directed path; force the targetId scan.
                            reverseIndexConsistent = false;
                        }
                        string incomingKey = deviceId + "|" + incomingPort;
                        bool declaredInputPort = false;
                        if (!declaredInputPorts.Find(incomingKey, declaredInputPort) || !declaredInputPort)
                            reverseIndexConsistent = false;
                        int graphPortCount = 0;
                        graphIncomingByPort.Find(incomingKey, graphPortCount);
                        graphIncomingByPort.Set(incomingKey, graphPortCount + 1);

                        ref array<string> indexedOwners;
                        if (!m_ReverseOwners.Find(incomingKey, indexedOwners) || !indexedOwners)
                        {
                            reverseIndexConsistent = false;
                        }
                        else
                        {
                            bool ownerFound = false;
                            int ownerIndex;
                            for (ownerIndex = 0; ownerIndex < indexedOwners.Count(); ownerIndex = ownerIndex + 1)
                            {
                                if (indexedOwners[ownerIndex] == piEdge.m_SourceNodeId)
                                {
                                    ownerFound = true;
                                    break;
                                }
                            }
                            if (!ownerFound)
                                reverseIndexConsistent = false;
                        }
                    }
                }
            }
        }

        // T5 W4-F06: the graph and reverse index jointly provide a cheap
        // per-device health signal. A mismatch alone enables the global backstop.
        int graphPortIndex;
        for (graphPortIndex = 0; graphPortIndex < graphIncomingByPort.Count(); graphPortIndex = graphPortIndex + 1)
        {
            string graphPortKey = graphIncomingByPort.GetKey(graphPortIndex);
            int graphPortTotal = graphIncomingByPort.GetElement(graphPortIndex);
            int indexedPortTotal = 0;
            m_ReverseIdx.Find(graphPortKey, indexedPortTotal);
            if (indexedPortTotal != graphPortTotal)
                reverseIndexConsistent = false;
        }

        int healthPortIndex;
        for (healthPortIndex = 0; healthPortIndex < portCount; healthPortIndex = healthPortIndex + 1)
        {
            if (LFPG_DeviceAPI.GetPortDir(device, healthPortIndex) != LFPG_PortDir.IN)
                continue;

            string healthPortName = LFPG_DeviceAPI.GetPortName(device, healthPortIndex);
            if (healthPortName == "")
                healthPortName = "input_main";
            string healthPortKey = deviceId + "|" + healthPortName;
            int graphHealthCount = 0;
            graphIncomingByPort.Find(healthPortKey, graphHealthCount);
            int indexedHealthCount = CountWiresTargeting(deviceId, healthPortName);
            if (indexedHealthCount != graphHealthCount)
                reverseIndexConsistent = false;
        }

        m_CutAllGraphBatchActive = true;

        // --- 1. Clear OWNED wires (output side) ---
        // Individual graph removals are suppressed for the duration of this
        // CutAll batch. Section 6 removes the node once, then the shared
        // callback rebuilds after all same-tick CutAll calls have settled.
        if (LFPG_DeviceAPI.HasWireStore(device))
        {
            ref array<ref LFPG_WireData> ownedWires = LFPG_DeviceAPI.GetDeviceWires(device);
            if (ownedWires && ownedWires.Count() > 0)
            {
                // Update reverse index and player counts before clearing
                array<vector> ownedTargetPositions = new array<vector>;
                EntityAI ownedTarget;
                bool ownedBroadcastAll = false;
                int ow = ownedWires.Count() - 1;
                while (ow >= 0)
                {
                    LFPG_WireData wd = ownedWires[ow];
                    if (wd)
                    {
                        ownedTarget = ResolveOwnerSnapshotTarget(wd);
                        if (ownedTarget)
                            ownedTargetPositions.Insert(ownedTarget.GetPosition());
                        else
                            ownedBroadcastAll = true;
                        ReverseIdxRemove(wd.m_TargetDeviceId, wd.m_TargetPort, deviceId);
                        PlayerWireCountAdd(wd.m_CreatorId, -1);
                    }
                    ow = ow - 1;
                }

                LFPG_DeviceAPI.ClearDeviceWires(device);
                QueueBroadcastOwnerSnapshot(device, ownedTargetPositions, ownedBroadcastAll);
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
                QueueBroadcastVanilla(deviceId, device);
                anyChanged = true;
            }
        }

        // --- 3. Remove wires TARGETING this device's IN ports ---
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
        // T5 W4-F06: only pay O(V) when the graph/index comparison above
        // proves the per-device reverse index inconsistent.
        if (!m_ReverseIndexTrusted)
            reverseIndexConsistent = false;
        if (!reverseIndexConsistent)
        {
            LFPG_Util.Warn("[CutAll] Reverse index mismatch detected; running global fallback scan");
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
                ref array<ref LFPG_WireData> fallbackRemovedWires = new array<ref LFPG_WireData>;
                int sw = srcWires.Count() - 1;
                while (sw >= 0)
                {
                    LFPG_WireData swd = srcWires[sw];
                    if (swd && swd.m_TargetDeviceId == deviceId)
                    {
                        string cutFbMsg = "[CutAll-Fallback] Found stale wire: " + srcId + " -> " + deviceId;
                        LFPG_Util.Warn(cutFbMsg);
                        PlayerWireCountAdd(swd.m_CreatorId, -1);
                        fallbackRemovedWires.Insert(swd);
                        srcWires.Remove(sw);
                        srcChanged = true;
                        anyChanged = true;
                    }
                    sw = sw - 1;
                }

                if (srcChanged)
                {
                    LFPG_WireOwnerBase srcWireOwner = LFPG_WireOwnerBase.Cast(srcDev);
                    if (srcWireOwner)
                    {
                        srcWireOwner.LFPG_CommitWireMutation();
                    }
                    srcDev.SetSynchDirty();
                    QueueBroadcastOwnerSnapshotFromWires(srcDev, fallbackRemovedWires);
                }
            }

            // Vanilla owners are stored in m_VanillaWires rather than on an
            // entity with HasWireStore, so the registry loop above cannot see
            // them. Cover them in the same mismatch-only backstop.
            int fallbackVanillaOwnerIndex;
            for (fallbackVanillaOwnerIndex = 0; fallbackVanillaOwnerIndex < m_VanillaWires.Count(); fallbackVanillaOwnerIndex = fallbackVanillaOwnerIndex + 1)
            {
                string fallbackVanillaOwnerId = m_VanillaWires.GetKey(fallbackVanillaOwnerIndex);
                ref array<ref LFPG_WireData> fallbackVanillaWires = m_VanillaWires.GetElement(fallbackVanillaOwnerIndex);
                if (!fallbackVanillaWires)
                    continue;

                bool fallbackVanillaChanged = false;
                int fallbackVanillaWireIndex = fallbackVanillaWires.Count() - 1;
                while (fallbackVanillaWireIndex >= 0)
                {
                    LFPG_WireData fallbackVanillaWire = fallbackVanillaWires[fallbackVanillaWireIndex];
                    if (fallbackVanillaWire && fallbackVanillaWire.m_TargetDeviceId == deviceId)
                    {
                        string fallbackVanillaMsg = "[CutAll-Fallback] Found stale vanilla wire: " + fallbackVanillaOwnerId + " -> " + deviceId;
                        LFPG_Util.Warn(fallbackVanillaMsg);
                        PlayerWireCountAdd(fallbackVanillaWire.m_CreatorId, -1);
                        fallbackVanillaWires.Remove(fallbackVanillaWireIndex);
                        fallbackVanillaChanged = true;
                        anyChanged = true;
                    }
                    fallbackVanillaWireIndex = fallbackVanillaWireIndex - 1;
                }

                if (fallbackVanillaChanged)
                {
                    MarkVanillaDirty();
                    EntityAI fallbackVanillaOwner = LFPG_DeviceRegistry.Get().FindById(fallbackVanillaOwnerId);
                    if (!fallbackVanillaOwner)
                        fallbackVanillaOwner = LFPG_DeviceAPI.ResolveVanillaDevice(fallbackVanillaOwnerId);
                    if (fallbackVanillaOwner)
                        QueueBroadcastVanilla(fallbackVanillaOwnerId, fallbackVanillaOwner);
                }
            }
        }

        if (!reverseIndexConsistent)
        {
            m_ReverseIndexTrusted = false;
            m_IndexHealAfterCut = true;
        }

        m_CutAllGraphBatchActive = false;

        // --- 5. Cleanup tracking state ---
        m_LastKnownPos.Remove(deviceId);

        // --- 5b. Queue powered=false on device and all neighbors (v0.7.28) ---
        // Apply these in the coalesced rebuild callback, immediately before
        // rebuild+propagation, so alternate paths are restored in one frame.
        EntityAI neighborDev;
        if (anyChanged)
        {
            m_CutPendingPowerOff.Set(deviceId, true);

            // Propagation will re-enable neighbors with an alternate path.
            int nbi;
            for (nbi = 0; nbi < neighborIds.Count(); nbi = nbi + 1)
                m_CutPendingPowerOff.Set(neighborIds[nbi], true);
        }

        // --- 6. Notify graph and propagate ---

        if (anyChanged)
        {
            if (m_Graph)
            {
                m_Graph.OnDeviceRemoved(deviceId);
            }
            // T5 W4-F06: defer to the next system-queue turn so a same-tick
            // kill/delete or K-device cascade shares one final graph rebuild.
            if (!m_CutGraphRebuildQueued)
            {
                m_CutGraphRebuildQueued = true;
                bool cutRebuildOnce = false;
                g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PostBulkRebuildAndPropagate, 1, cutRebuildOnce);
            }
            string cutAllMsg = "[CutAll] All wires removed for device " + deviceId + " type=" + device.GetType();
            LFPG_Util.Info(cutAllMsg);

            // T5 R21-T5-003: broadcasts stay queued until the coalesced
            // PostBulk callback. A same-tick K-device cascade therefore emits
            // one deduplicated final batch after graph rebuild+propagation.

            // v0.7.32 (Audit P2): Immediate vanilla flush after critical cut.
            // MarkVanillaDirty() was called in section 2, but FlushVanillaIfDirty
            // runs on a 5s timer. Integrity cuts still flush immediately, so vanilla
            // wires are lost. CutAll is infrequent enough that synchronous
            // flush has negligible perf impact.
            if (m_VanillaDirty)
            {
                FlushVanillaIfDirty();
            }
        }
        else if (m_IndexHealAfterCut && !m_CutGraphRebuildQueued)
        {
            // No wire store changed, so there is no graph rebuild to wait for.
            // Run the full atomic index/recount heal and do not skip its graph pass.
            m_IndexHealAfterCut = false;
            RequestGlobalSelfHeal();
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

    float LFPG_GetBTC24hChange()
    {
        if (m_BTCPriceFetcher)
        {
            return m_BTCPriceFetcher.Get24hChangePercent();
        }
        return 0.0;
    }

    // Read world time once, update cached sun state.
    // Called by constructor (seed) and by LFPG_TickSolarPanels (periodic).
    protected void LFPG_ComputeSunState()
    {
        #ifdef SERVER
        if (!g_Game)
            return;

        World world = g_Game.GetWorld();
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
        m_TankFillLastMs = g_Game.GetTime();
        #endif
    }

    // Periodic tick (every LFPG_PUMP_CHECK_MS = 60s)
    // v4.1: Uses dedicated registries instead of GetAll+Cast.
    protected void LFPG_TickWaterPumps()
    {
        #ifdef SERVER
        float nowMs = g_Game.GetTime();
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
            m_RegisteredSprinklerPhases.Insert(m_NextSprinklerPhase);
            m_NextSprinklerPhase = m_NextSprinklerPhase + 1;
            if (m_NextSprinklerPhase >= 10)
                m_NextSprinklerPhase = 0;
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
            m_RegisteredSprinklerPhases.Remove(idx);
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
            m_SorterResumeItems.Insert(null);
            m_SorterResumeItemIndices.Insert(0);
            m_SorterResumeOutputs.Insert(0);
            m_SorterResumeRules.Insert(0);
            m_SorterResumeConfigs.Insert(null);
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
            m_SorterResumeItems.Remove(idx);
            m_SorterResumeItemIndices.Remove(idx);
            m_SorterResumeOutputs.Remove(idx);
            m_SorterResumeRules.Remove(idx);
            m_SorterResumeConfigs.Remove(idx);
            if (m_SorterCursor > idx)
                m_SorterCursor = m_SorterCursor - 1;
        }
    }

    protected void LFPG_ClearSorterResume(int sorterIndex)
    {
        if (sorterIndex < 0)
            return;
        if (sorterIndex >= m_SorterResumeItems.Count())
            return;
        m_SorterResumeItems[sorterIndex] = null;
        m_SorterResumeItemIndices[sorterIndex] = 0;
        m_SorterResumeOutputs[sorterIndex] = 0;
        m_SorterResumeRules[sorterIndex] = 0;
        m_SorterResumeConfigs[sorterIndex] = null;
    }

    protected void LFPG_TickSorters()
    {
        #ifdef SERVER
        int total;
        int batchEnd;
        int sorterIndex;
        int itemIndex;
        int outputIndex;
        int portIndex;
        int portBit;
        int hasWireMask;
        int itemCount;
        int moved;
        int evaluated;
        int resumeItemIndex;
        int resumeOutput;
        int resumeRule;
        int nextOutput;
        int nextRule;
        int itemRuleChecks;
        int itemConfigMisses;
        int budgetRemaining;
        int ruleChecksTick;
        int configMissesTick;
        int deferralsTick;
        int dirtyDestCommitCount;
        int dirtySourceCommitCount;
        int dirtyDestIndex;
        int dirtySourceIndex;
        bool moveResult;
        bool itemDeferred;
        bool budgetExhausted;
        bool storedResumeValid;
        LFPG_Sorter sorter;
        LFPG_SortConfig filterConfig;
        LFPG_SortConfig resumeConfig;
        EntityAI inputContainer;
        CargoBase inputCargo;
        EntityAI destinationContainer;
        EntityAI sortItem;
        EntityAI wireCheck;
        EntityAI resumeItem;
        EntityAI dirtyDestination;
        EntityAI dirtySource;
        float linkDistance;

        total = m_RegisteredSorters.Count();
        if (total == 0)
        {
            m_SorterCursor = 0;
            return;
        }
        if (m_SorterCursor >= total)
            m_SorterCursor = 0;

        batchEnd = m_SorterCursor + LFPG_SORTER_BATCH_SIZE;
        if (batchEnd > total)
            batchEnd = total;

        m_TickAffectedContainers.Clear();
        m_TickDirtyDestinations.Clear();
        m_TickDirtySources.Clear();
        ruleChecksTick = 0;
        configMissesTick = 0;
        deferralsTick = 0;
        budgetExhausted = false;

        for (sorterIndex = m_SorterCursor; sorterIndex < batchEnd; sorterIndex = sorterIndex + 1)
        {
            if (sorterIndex >= m_RegisteredSorters.Count())
                break;

            sorter = m_RegisteredSorters[sorterIndex];
            if (!sorter)
            {
                LFPG_ClearSorterResume(sorterIndex);
                continue;
            }
            if (!sorter.LFPG_IsPowered())
            {
                LFPG_ClearSorterResume(sorterIndex);
                continue;
            }
            if (sorter.IsRuined())
            {
                LFPG_ClearSorterResume(sorterIndex);
                continue;
            }

            filterConfig = sorter.LFPG_GetFilterConfig();
            if (!filterConfig)
            {
                LFPG_ClearSorterResume(sorterIndex);
                continue;
            }

            inputContainer = sorter.LFPG_GetLinkedContainer();
            if (inputContainer)
            {
                linkDistance = vector.Distance(sorter.GetPosition(), inputContainer.GetPosition());
                if (linkDistance > LFPG_SORTER_LINK_RADIUS)
                {
                    sorter.LFPG_UnlinkContainer();
                    LFPG_ClearSorterResume(sorterIndex);
                    string unlinkMsg = "[Sorter] Auto-unlink: container beyond ";
                    unlinkMsg = unlinkMsg + LFPG_SORTER_LINK_RADIUS.ToString();
                    unlinkMsg = unlinkMsg + "m (was ";
                    unlinkMsg = unlinkMsg + linkDistance.ToString();
                    unlinkMsg = unlinkMsg + "m)";
                    LFPG_Util.Info(unlinkMsg);
                    continue;
                }
            }
            if (!inputContainer)
            {
                LFPG_ClearSorterResume(sorterIndex);
                continue;
            }
            if (!LFPG_SorterLogic.CanTakeFromContainer(inputContainer, null))
            {
                LFPG_ClearSorterResume(sorterIndex);
                continue;
            }
            if (!inputContainer.GetInventory())
            {
                LFPG_ClearSorterResume(sorterIndex);
                continue;
            }

            inputCargo = inputContainer.GetInventory().GetCargo();
            if (!inputCargo)
            {
                LFPG_ClearSorterResume(sorterIndex);
                continue;
            }
            itemCount = inputCargo.GetItemCount();
            if (itemCount <= 0)
            {
                LFPG_ClearSorterResume(sorterIndex);
                continue;
            }

            hasWireMask = 0;
            portBit = 1;
            for (portIndex = 0; portIndex < 6; portIndex = portIndex + 1)
            {
                wireCheck = LFPG_SorterLogic.ResolveOutputContainer(sorter, portIndex);
                if (wireCheck)
                    hasWireMask = hasWireMask | portBit;
                portBit = portBit * 2;
            }
            if (hasWireMask == 0)
            {
                LFPG_ClearSorterResume(sorterIndex);
                continue;
            }

            m_SorterItemCache.Clear();
            for (itemIndex = 0; itemIndex < itemCount; itemIndex = itemIndex + 1)
            {
                sortItem = inputCargo.GetItem(itemIndex);
                if (sortItem)
                    m_SorterItemCache.Insert(sortItem);
            }

            resumeItem = m_SorterResumeItems[sorterIndex];
            resumeItemIndex = m_SorterResumeItemIndices[sorterIndex];
            resumeOutput = m_SorterResumeOutputs[sorterIndex];
            resumeRule = m_SorterResumeRules[sorterIndex];
            resumeConfig = m_SorterResumeConfigs[sorterIndex];
            storedResumeValid = false;
            if (resumeItem && resumeConfig == filterConfig)
            {
                if (resumeItemIndex >= 0 && resumeItemIndex < m_SorterItemCache.Count())
                {
                    if (m_SorterItemCache[resumeItemIndex] == resumeItem)
                        storedResumeValid = true;
                }
                if (!storedResumeValid)
                {
                    resumeItemIndex = m_SorterItemCache.Find(resumeItem);
                    if (resumeItemIndex >= 0)
                        storedResumeValid = true;
                }
            }
            if (!storedResumeValid)
            {
                LFPG_ClearSorterResume(sorterIndex);
                resumeItem = null;
                resumeItemIndex = 0;
                resumeOutput = 0;
                resumeRule = 0;
            }

            moved = 0;
            evaluated = 0;
            for (itemIndex = resumeItemIndex; itemIndex < m_SorterItemCache.Count(); itemIndex = itemIndex + 1)
            {
                if (moved >= LFPG_SORTER_ITEMS_PER_TICK || evaluated >= LFPG_SORTER_MAX_EVAL)
                {
                    sortItem = m_SorterItemCache[itemIndex];
                    m_SorterResumeItems[sorterIndex] = sortItem;
                    m_SorterResumeItemIndices[sorterIndex] = itemIndex;
                    m_SorterResumeOutputs[sorterIndex] = 0;
                    m_SorterResumeRules[sorterIndex] = 0;
                    m_SorterResumeConfigs[sorterIndex] = filterConfig;
                    break;
                }

                sortItem = m_SorterItemCache[itemIndex];
                if (!sortItem)
                    continue;
                evaluated = evaluated + 1;
                if (!inputContainer.CanReleaseCargo(sortItem))
                {
                    if (sortItem == resumeItem)
                        LFPG_ClearSorterResume(sorterIndex);
                    continue;
                }

                nextOutput = 0;
                nextRule = 0;
                itemRuleChecks = 0;
                itemConfigMisses = 0;
                itemDeferred = false;
                if (sortItem != resumeItem)
                {
                    resumeOutput = 0;
                    resumeRule = 0;
                }
                budgetRemaining = LFPG_SORTER_RULECHECK_BUDGET - ruleChecksTick - configMissesTick;
                outputIndex = LFPG_SorterLogic.EvaluateItemBudgeted(sortItem, filterConfig, hasWireMask, resumeOutput, resumeRule, budgetRemaining, nextOutput, nextRule, itemRuleChecks, itemConfigMisses, itemDeferred);
                ruleChecksTick = ruleChecksTick + itemRuleChecks;
                configMissesTick = configMissesTick + itemConfigMisses;

                if (itemDeferred)
                {
                    m_SorterResumeItems[sorterIndex] = sortItem;
                    m_SorterResumeItemIndices[sorterIndex] = itemIndex;
                    m_SorterResumeOutputs[sorterIndex] = nextOutput;
                    m_SorterResumeRules[sorterIndex] = nextRule;
                    m_SorterResumeConfigs[sorterIndex] = filterConfig;
                    deferralsTick = deferralsTick + 1;
                    budgetExhausted = true;
                    break;
                }

                LFPG_ClearSorterResume(sorterIndex);
                resumeItem = null;
                resumeOutput = 0;
                resumeRule = 0;
                if (outputIndex < 0)
                    continue;

                destinationContainer = LFPG_SorterLogic.ResolveOutputContainer(sorter, outputIndex);
                if (!destinationContainer)
                    continue;
                if (destinationContainer == inputContainer)
                    continue;

                moveResult = LFPG_SorterLogic.MoveItemToContainerReusable(sortItem, destinationContainer, m_SorterMoveSourceLocation, m_SorterMoveDestinationLocation);
                if (moveResult)
                {
                    moved = moved + 1;
                    if (m_TickDirtyDestinations.Find(destinationContainer) < 0)
                        m_TickDirtyDestinations.Insert(destinationContainer);
                    if (m_TickAffectedContainers.Find(destinationContainer) < 0)
                        m_TickAffectedContainers.Insert(destinationContainer);
                }
            }

            if (itemIndex >= m_SorterItemCache.Count() && !budgetExhausted)
                LFPG_ClearSorterResume(sorterIndex);

            if (moved > 0)
            {
                if (m_TickDirtySources.Find(inputContainer) < 0)
                    m_TickDirtySources.Insert(inputContainer);
                if (m_TickAffectedContainers.Find(inputContainer) < 0)
                    m_TickAffectedContainers.Insert(inputContainer);

                string tickDiag = "[TickSorters] moved=";
                tickDiag = tickDiag + moved.ToString();
                tickDiag = tickDiag + " evaluated=";
                tickDiag = tickDiag + evaluated.ToString();
                tickDiag = tickDiag + " src=";
                tickDiag = tickDiag + inputContainer.GetType();
                LFPG_Util.Info(tickDiag);
            }

            if (budgetExhausted)
            {
                m_SorterCursor = sorterIndex;
                break;
            }
        }

        dirtyDestCommitCount = 0;
        dirtySourceCommitCount = 0;
        for (dirtyDestIndex = 0; dirtyDestIndex < m_TickDirtyDestinations.Count(); dirtyDestIndex = dirtyDestIndex + 1)
        {
            dirtyDestination = m_TickDirtyDestinations[dirtyDestIndex];
            if (dirtyDestination)
            {
                dirtyDestination.SetSynchDirty();
                dirtyDestCommitCount = dirtyDestCommitCount + 1;
            }
        }
        for (dirtySourceIndex = 0; dirtySourceIndex < m_TickDirtySources.Count(); dirtySourceIndex = dirtySourceIndex + 1)
        {
            dirtySource = m_TickDirtySources[dirtySourceIndex];
            if (!dirtySource)
                continue;
            if (m_TickDirtyDestinations.Find(dirtySource) >= 0)
                continue;
            dirtySource.SetSynchDirty();
            dirtySourceCommitCount = dirtySourceCommitCount + 1;
        }

        if (LFPG_PERFDIAG_ENABLED)
        {
            m_PerfDiagSorterDestDirtyCount = m_PerfDiagSorterDestDirtyCount + dirtyDestCommitCount;
            m_PerfDiagSorterSourceDirtyCount = m_PerfDiagSorterSourceDirtyCount + dirtySourceCommitCount;
            string perfSorter = "LFPG_PERFDIAG sorter_dirty dest_tick=";
            perfSorter = perfSorter + dirtyDestCommitCount.ToString();
            perfSorter = perfSorter + " source_tick=";
            perfSorter = perfSorter + dirtySourceCommitCount.ToString();
            perfSorter = perfSorter + " dest_total=";
            perfSorter = perfSorter + m_PerfDiagSorterDestDirtyCount.ToString();
            perfSorter = perfSorter + " source_total=";
            perfSorter = perfSorter + m_PerfDiagSorterSourceDirtyCount.ToString();
            Print(perfSorter);
        }

        if (LFPG_PERFDIAG_ENABLED)
        {
            m_PerfDiagSorterRuleChecks = m_PerfDiagSorterRuleChecks + ruleChecksTick;
            m_PerfDiagSorterConfigMisses = m_PerfDiagSorterConfigMisses + configMissesTick;
            m_PerfDiagSorterDeferrals = m_PerfDiagSorterDeferrals + deferralsTick;
            int budgetUsed = ruleChecksTick + configMissesTick;
            string perfBudget = "LFPG_PERFDIAG sorter_budget rule_checks=";
            perfBudget = perfBudget + ruleChecksTick.ToString();
            perfBudget = perfBudget + " config_misses=";
            perfBudget = perfBudget + configMissesTick.ToString();
            perfBudget = perfBudget + " used=";
            perfBudget = perfBudget + budgetUsed.ToString();
            perfBudget = perfBudget + " budget=";
            perfBudget = perfBudget + LFPG_SORTER_RULECHECK_BUDGET.ToString();
            perfBudget = perfBudget + " deferrals=";
            perfBudget = perfBudget + deferralsTick.ToString();
            perfBudget = perfBudget + " checks_total=";
            perfBudget = perfBudget + m_PerfDiagSorterRuleChecks.ToString();
            perfBudget = perfBudget + " misses_total=";
            perfBudget = perfBudget + m_PerfDiagSorterConfigMisses.ToString();
            perfBudget = perfBudget + " deferrals_total=";
            perfBudget = perfBudget + m_PerfDiagSorterDeferrals.ToString();
            Print(perfBudget);
        }

        if (m_TickAffectedContainers.Count() > 0)
        {
            string emptyExclude = "";
            BroadcastCargoRefreshToNearby(m_TickAffectedContainers, emptyExclude);
        }

        if (!budgetExhausted)
        {
            m_SorterCursor = batchEnd;
            if (m_SorterCursor >= total)
                m_SorterCursor = 0;
        }
        #endif
    }

    // ===========================
    // v5.0: Cargo Refresh Broadcast
    // Sends SORTER_CARGO_REFRESH RPC to players within 15m of any
    // affected container. Fixes Bug 2: other players viewing containers
    // now get explicit UI refresh after sort operations.
    // ===========================
    void BroadcastCargoRefreshToNearby(array<EntityAI> containers, string excludePlayerId)
    {
        #ifdef SERVER
        if (!containers)
            return;

        int containerCount = containers.Count();
        if (containerCount <= 0)
            return;

        // Reuse class-level player array (avoid GC in periodic ticks)
        m_ReusablePlayers.Clear();
        g_Game.GetPlayers(m_ReusablePlayers);
        int playerCount = m_ReusablePlayers.Count();
        if (playerCount <= 0)
            return;

        float maxDistSq = 225.0;
        int pi = 0;
        int ci = 0;
        Man man = null;
        PlayerBase pb = null;
        PlayerIdentity pid = null;
        string pidStr = "";
        float distSq = 0.0;
        vector playerPos = vector.Zero;
        vector containerPos = vector.Zero;
        bool isNear = false;
        int notified = 0;
        int refreshSubId = LFPG_RPC_SubId.SORTER_CARGO_REFRESH;

        for (pi = 0; pi < playerCount; pi = pi + 1)
        {
            man = m_ReusablePlayers[pi];
            if (!man)
                continue;

            pb = PlayerBase.Cast(man);
            if (!pb)
                continue;

            pid = pb.GetIdentity();
            if (!pid)
                continue;

            // Exclude the requester (they already got SORT_ACK)
            pidStr = pid.GetId();
            if (excludePlayerId != "" && pidStr == excludePlayerId)
                continue;

            playerPos = pb.GetPosition();
            isNear = false;

            // Check distance to any affected container
            for (ci = 0; ci < containerCount; ci = ci + 1)
            {
                if (!containers[ci])
                    continue;

                containerPos = containers[ci].GetPosition();
                distSq = LFPG_WorldUtil.DistSq(playerPos, containerPos);
                if (distSq <= maxDistSq)
                {
                    isNear = true;
                    break;
                }
            }

            if (!isNear)
                continue;

            // Send lightweight refresh RPC (no payload beyond SubId)
            ScriptRPC refreshRpc = new ScriptRPC();
            refreshRpc.Write(refreshSubId);
            refreshRpc.Send(pb, LFPG_RPC_CHANNEL, true, pid);
            notified = notified + 1;
        }

        if (notified > 0)
        {
            string bcastLog = "[Sorter] CargoRefresh broadcast: notified=";
            bcastLog = bcastLog + notified.ToString();
            bcastLog = bcastLog + " containers=";
            bcastLog = bcastLog + containerCount.ToString();
            LFPG_Util.Debug(bcastLog);
        }
        #endif
    }

    // ===========================
    // v1.2.0 (Sprint S3): Sorter Sort (RPC handler)
    // ===========================
    // Called by PlayerRPC after validating proximity + type.
    // Sorter already resolved and validated upstream.

    // v3.2: Returns moved count (-1 = error/skip, 0+ = items moved).
    // Caller (PlayerRPC) sends SORT_ACK with the result.
    // v5.0: excludePlayerId — player ID string of the requester to skip
    //        in the cargo refresh broadcast (they get SORT_ACK instead).
    int HandleSorterRequestSort(LFPG_Sorter sorter, string excludePlayerId)
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
            string d2 = "[Sorter] REQUEST_SORT: no wired outputs, bin-pack only";
            LFPG_Util.Debug(d2);
            // v5.0: Repack in-place (no ground round-trip)
            LFPG_SorterLogic.RepackCargoInPlace(container);
            // v5.0: Broadcast to nearby players
            array<EntityAI> repackOnly = new array<EntityAI>;
            repackOnly.Insert(container);
            BroadcastCargoRefreshToNearby(repackOnly, excludePlayerId);
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

        // v4.2: Dirty source container so client refreshes cargo view.
        if (moved > 0)
        {
            container.SetSynchDirty();
        }

        // v5.0: Repack remaining items in source (in-place, no ground round-trip)
        LFPG_SorterLogic.RepackCargoInPlace(container);

        // v5.0: Broadcast cargo refresh to nearby players (excluding requester)
        // Requester already gets SORT_ACK which triggers their own UI refresh.
        array<EntityAI> affectedContainers = new array<EntityAI>;
        affectedContainers.Insert(container);
        for (di = 0; di < dirtiedDests.Count(); di = di + 1)
        {
            if (affectedContainers.Find(dirtiedDests[di]) < 0)
            {
                affectedContainers.Insert(dirtiedDests[di]);
            }
        }
        BroadcastCargoRefreshToNearby(affectedContainers, excludePlayerId);

        string sortLog = "[Sorter] REQUEST_SORT: evaluated=";
        sortLog = sortLog + evaluated.ToString();
        sortLog = sortLog + " moved=";
        sortLog = sortLog + moved.ToString();
        LFPG_Util.Info(sortLog);

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
            laser.LFPG_UpdateBeamRaycast();
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
            m_RegisteredFridgePhases.Insert(m_NextFridgePhase);
            m_NextFridgePhase = m_NextFridgePhase + 1;
            if (m_NextFridgePhase >= 10)
                m_NextFridgePhase = 0;
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
            m_RegisteredFridgePhases.Remove(idx);
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
            m_RegisteredStovePhases.Insert(m_NextStovePhase);
            m_NextStovePhase = m_NextStovePhase + 1;
            if (m_NextStovePhase >= 3)
                m_NextStovePhase = 0;
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
            m_RegisteredStovePhases.Remove(idx);
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
    // Single 1,000ms timer drives the simple-device registries.
    // Intercom/door/furnace/battery cadence is preserved; T2 assigns stable
    // 10-phase fridge/sprinkler buckets and 3-phase stove buckets.
    // OPT-2: Early-out when all registries empty.
    protected void LFPG_TickSimpleDevices()
    {
        #ifdef SERVER
        int totalIc;
        int totalDc;
        int totalFur;
        int totalBat;
        int totalFri;
        int totalStv;
        int totalSpr;
        int totalSimple;
        int intercomIndex;
        int doorIndex;
        int furnaceIndex;
        int fridgeIndex;
        int stoveIndex;
        int sprinklerIndex;
        int doorMod;
        int furnaceMod;
        int batteryMod;
        int wetAppliedTick;
        int wetCoalescedTick;
        int wetPreGateTick;
        int wetAppliedOne;
        int wetCoalescedOne;
        float stoveDelta;
        bool hasPlayerCell;
        bool sprinklerPhaseActive;
        LFPG_Intercom intercomDevice;
        LFPG_DoorController doorController;
        LFPG_Furnace furnaceDevice;
        LFPG_Fridge fridgeDevice;
        LFPG_ElectricStove stoveDevice;
        LFPG_Sprinkler sprinklerDevice;

        totalIc = m_RegisteredIntercoms.Count();
        totalDc = m_RegisteredDoorControllers.Count();
        totalFur = m_RegisteredFurnaces.Count();
        totalBat = m_RegisteredBatteries.Count();
        totalFri = m_RegisteredFridges.Count();
        totalStv = m_RegisteredStoves.Count();
        totalSpr = m_RegisteredSprinklers.Count();
        totalSimple = totalIc + totalDc + totalFur + totalBat + totalFri + totalStv + totalSpr;
        if (totalSimple == 0)
            return;

        m_SimpleTickCounter = m_SimpleTickCounter + 1;
        if (m_SimpleTickCounter >= 10)
            m_SimpleTickCounter = 0;

        for (intercomIndex = 0; intercomIndex < totalIc; intercomIndex = intercomIndex + 1)
        {
            if (intercomIndex >= m_RegisteredIntercoms.Count())
                break;
            intercomDevice = m_RegisteredIntercoms[intercomIndex];
            if (intercomDevice)
                intercomDevice.LFPG_EvaluateToggleInput();
        }

        doorMod = m_SimpleTickCounter % 2;
        if (doorMod == 1)
        {
            for (doorIndex = 0; doorIndex < totalDc; doorIndex = doorIndex + 1)
            {
                if (doorIndex >= m_RegisteredDoorControllers.Count())
                    break;
                doorController = m_RegisteredDoorControllers[doorIndex];
                if (doorController)
                    doorController.LFPG_OnDoorPoll();
            }
        }

        furnaceMod = m_SimpleTickCounter % 5;
        if (furnaceMod == 2)
        {
            for (furnaceIndex = 0; furnaceIndex < totalFur; furnaceIndex = furnaceIndex + 1)
            {
                if (furnaceIndex >= m_RegisteredFurnaces.Count())
                    break;
                furnaceDevice = m_RegisteredFurnaces[furnaceIndex];
                if (furnaceDevice)
                    furnaceDevice.LFPG_BurnTick();
            }
        }

        batteryMod = m_SimpleTickCounter % 5;
        if (batteryMod == 4 && totalBat > 0)
            LFPG_TickBatteriesInternal();

        for (fridgeIndex = 0; fridgeIndex < totalFri; fridgeIndex = fridgeIndex + 1)
        {
            if (fridgeIndex >= m_RegisteredFridges.Count())
                break;
            if (m_RegisteredFridgePhases[fridgeIndex] != m_FridgePhaseCursor)
                continue;
            fridgeDevice = m_RegisteredFridges[fridgeIndex];
            if (fridgeDevice)
                fridgeDevice.LFPG_OnCoolTick();
        }
        m_FridgePhaseCursor = m_FridgePhaseCursor + 1;
        if (m_FridgePhaseCursor >= 10)
            m_FridgePhaseCursor = 0;

        stoveDelta = 3.0;
        for (stoveIndex = 0; stoveIndex < totalStv; stoveIndex = stoveIndex + 1)
        {
            if (stoveIndex >= m_RegisteredStoves.Count())
                break;
            if (m_RegisteredStovePhases[stoveIndex] != m_StovePhaseCursor)
                continue;
            stoveDevice = m_RegisteredStoves[stoveIndex];
            if (stoveDevice)
                stoveDevice.LFPG_TickCooking(stoveDelta);
        }
        m_StovePhaseCursor = m_StovePhaseCursor + 1;
        if (m_StovePhaseCursor >= 3)
            m_StovePhaseCursor = 0;

        wetAppliedTick = 0;
        wetCoalescedTick = 0;
        wetPreGateTick = 0;
        if (m_SprinklerPhaseCursor == 0)
            m_SprinklerWetPlayers.Clear();
        sprinklerPhaseActive = false;
        for (sprinklerIndex = 0; sprinklerIndex < totalSpr; sprinklerIndex = sprinklerIndex + 1)
        {
            if (sprinklerIndex >= m_RegisteredSprinklers.Count())
                break;
            if (m_RegisteredSprinklerPhases[sprinklerIndex] != m_SprinklerPhaseCursor)
                continue;
            sprinklerDevice = m_RegisteredSprinklers[sprinklerIndex];
            if (sprinklerDevice)
            {
                if (sprinklerDevice.LFPG_GetSprinklerActive())
                {
                    sprinklerPhaseActive = true;
                    break;
                }
            }
        }
        if (sprinklerPhaseActive)
            LFPG_RebuildPlayerCells();
        for (sprinklerIndex = 0; sprinklerIndex < totalSpr; sprinklerIndex = sprinklerIndex + 1)
        {
            if (sprinklerIndex >= m_RegisteredSprinklers.Count())
                break;
            if (m_RegisteredSprinklerPhases[sprinklerIndex] != m_SprinklerPhaseCursor)
                continue;
            sprinklerDevice = m_RegisteredSprinklers[sprinklerIndex];
            if (!sprinklerDevice)
                continue;
            if (!sprinklerDevice.LFPG_GetSprinklerActive())
                continue;

            hasPlayerCell = LFPG_HasPlayerCellNear(sprinklerDevice.GetPosition(), LFPG_SPRINKLER_RADIUS);
            if (!hasPlayerCell)
                wetPreGateTick = wetPreGateTick + 1;
            wetAppliedOne = 0;
            wetCoalescedOne = 0;
            sprinklerDevice.LFPG_TickWatering(hasPlayerCell, m_SprinklerWetPlayers, wetAppliedOne, wetCoalescedOne);
            wetAppliedTick = wetAppliedTick + wetAppliedOne;
            wetCoalescedTick = wetCoalescedTick + wetCoalescedOne;
        }
        m_SprinklerPhaseCursor = m_SprinklerPhaseCursor + 1;
        if (m_SprinklerPhaseCursor >= 10)
            m_SprinklerPhaseCursor = 0;

        if (LFPG_PERFDIAG_ENABLED)
        {
            m_PerfDiagWetApplied = m_PerfDiagWetApplied + wetAppliedTick;
            m_PerfDiagWetCoalesced = m_PerfDiagWetCoalesced + wetCoalescedTick;
            m_PerfDiagWetPreGateSkips = m_PerfDiagWetPreGateSkips + wetPreGateTick;
            string wetDiag = "LFPG_PERFDIAG sprinkler_wet applied=";
            wetDiag = wetDiag + wetAppliedTick.ToString();
            wetDiag = wetDiag + " coalesced=";
            wetDiag = wetDiag + wetCoalescedTick.ToString();
            wetDiag = wetDiag + " pregate_skips=";
            wetDiag = wetDiag + wetPreGateTick.ToString();
            wetDiag = wetDiag + " applied_total=";
            wetDiag = wetDiag + m_PerfDiagWetApplied.ToString();
            wetDiag = wetDiag + " coalesced_total=";
            wetDiag = wetDiag + m_PerfDiagWetCoalesced.ToString();
            wetDiag = wetDiag + " pregate_total=";
            wetDiag = wetDiag + m_PerfDiagWetPreGateSkips.ToString();
            Print(wetDiag);
        }
        #endif
    }

    // ===========================
    // v4.1: Consolidated Player Detection Tick
    // ===========================
    // One 300ms callback builds a coarse player index once, then evaluates
    // every due device (pads and sensors every 600ms).
    // Beam transforms refresh immediately; maintenance uses a bounded full cycle.
    protected void LFPG_RebuildPlayerCells()
    {
        int playerIndex;
        int playerTotal;
        int cellIndex;
        int cellTotal;
        int searchIndex;
        int memberIndex;
        int writeIndex;
        int prefix;
        Man playerMan;
        PlayerBase playerBase;
        vector playerPos;
        int cellX;
        int cellZ;

        m_ReusablePlayers.Clear();
        g_Game.GetPlayers(m_ReusablePlayers);
        m_PlayerCellPlayers.Clear();
        m_PlayerCellMembership.Clear();
        m_PlayerCellX.Clear();
        m_PlayerCellZ.Clear();
        m_PlayerCellStart.Clear();
        m_PlayerCellCount.Clear();
        m_PlayerCellWrite.Clear();
        m_PlayerCellOrdered.Clear();

        playerTotal = m_ReusablePlayers.Count();
        for (playerIndex = 0; playerIndex < playerTotal; playerIndex = playerIndex + 1)
        {
            playerMan = m_ReusablePlayers[playerIndex];
            if (!playerMan)
                continue;
            if (!playerMan.IsAlive())
                continue;
            playerBase = PlayerBase.Cast(playerMan);
            if (!playerBase)
                continue;

            playerPos = playerBase.GetPosition();
            cellX = Math.Floor(playerPos[0] / LFPG_PLAYER_CELL_SIZE_M);
            cellZ = Math.Floor(playerPos[2] / LFPG_PLAYER_CELL_SIZE_M);
            cellIndex = -1;
            cellTotal = m_PlayerCellX.Count();
            for (searchIndex = 0; searchIndex < cellTotal; searchIndex = searchIndex + 1)
            {
                if (m_PlayerCellX[searchIndex] == cellX && m_PlayerCellZ[searchIndex] == cellZ)
                {
                    cellIndex = searchIndex;
                    break;
                }
            }

            if (cellIndex < 0)
            {
                cellIndex = m_PlayerCellX.Count();
                m_PlayerCellX.Insert(cellX);
                m_PlayerCellZ.Insert(cellZ);
                m_PlayerCellStart.Insert(0);
                m_PlayerCellCount.Insert(0);
                m_PlayerCellWrite.Insert(0);
            }

            m_PlayerCellPlayers.Insert(playerMan);
            m_PlayerCellMembership.Insert(cellIndex);
            m_PlayerCellCount[cellIndex] = m_PlayerCellCount[cellIndex] + 1;
        }

        prefix = 0;
        cellTotal = m_PlayerCellX.Count();
        for (cellIndex = 0; cellIndex < cellTotal; cellIndex = cellIndex + 1)
        {
            m_PlayerCellStart[cellIndex] = prefix;
            m_PlayerCellWrite[cellIndex] = prefix;
            prefix = prefix + m_PlayerCellCount[cellIndex];
        }
        for (memberIndex = 0; memberIndex < prefix; memberIndex = memberIndex + 1)
        {
            m_PlayerCellOrdered.Insert(null);
        }
        for (memberIndex = 0; memberIndex < m_PlayerCellPlayers.Count(); memberIndex = memberIndex + 1)
        {
            cellIndex = m_PlayerCellMembership[memberIndex];
            writeIndex = m_PlayerCellWrite[cellIndex];
            m_PlayerCellOrdered[writeIndex] = m_PlayerCellPlayers[memberIndex];
            m_PlayerCellWrite[cellIndex] = writeIndex + 1;
        }
    }

    protected int LFPG_CollectPlayerCandidates(vector center, float radius)
    {
        int minCellX;
        int maxCellX;
        int minCellZ;
        int maxCellZ;
        int cellIndex;
        int cellTotal;
        int memberIndex;
        int memberEnd;
        Man candidate;

        m_PlayerCandidates.Clear();
        minCellX = Math.Floor((center[0] - radius) / LFPG_PLAYER_CELL_SIZE_M);
        maxCellX = Math.Floor((center[0] + radius) / LFPG_PLAYER_CELL_SIZE_M);
        minCellZ = Math.Floor((center[2] - radius) / LFPG_PLAYER_CELL_SIZE_M);
        maxCellZ = Math.Floor((center[2] + radius) / LFPG_PLAYER_CELL_SIZE_M);
        cellTotal = m_PlayerCellX.Count();

        for (cellIndex = 0; cellIndex < cellTotal; cellIndex = cellIndex + 1)
        {
            if (m_PlayerCellX[cellIndex] < minCellX)
                continue;
            if (m_PlayerCellX[cellIndex] > maxCellX)
                continue;
            if (m_PlayerCellZ[cellIndex] < minCellZ)
                continue;
            if (m_PlayerCellZ[cellIndex] > maxCellZ)
                continue;

            memberIndex = m_PlayerCellStart[cellIndex];
            memberEnd = memberIndex + m_PlayerCellCount[cellIndex];
            while (memberIndex < memberEnd)
            {
                candidate = m_PlayerCellOrdered[memberIndex];
                if (candidate)
                    m_PlayerCandidates.Insert(candidate);
                memberIndex = memberIndex + 1;
            }
        }

        return m_PlayerCandidates.Count();
    }

    protected bool LFPG_HasPlayerCellNear(vector center, float radius)
    {
        if (LFPG_CollectPlayerCandidates(center, radius) > 0)
            return true;
        return false;
    }

    protected void LFPG_TickPlayerDetection()
    {
        #ifdef SERVER
        int totalLasers;
        int totalPads;
        int totalSensors;
        int totalDetect;
        int padMod;
        bool padDue;
        bool sensorDue;
        bool needPlayers;
        int nowMs;
        int scanIndex;
        int processCount;
        int processIndex;
        int registryIndex;
        int candidateCount;
        int laserEvaluated;
        int padEvaluated;
        int sensorEvaluated;
        int laserDormant;
        int padDormant;
        int sensorDormant;
        int laserChanged;
        int padChanged;
        int sensorChanged;
        int raycasts;
        int rayChecked;
        int maintenanceBudget;
        int maintenanceRaycasts;
        LFPG_LaserDetector laser;
        LFPG_PressurePad pad;
        LFPG_MotionSensor sensor;
        bool stateChanged;
        string deviceId;

        totalLasers = m_RegisteredLasers.Count();
        totalPads = m_RegisteredPads.Count();
        totalSensors = m_RegisteredSensors.Count();
        totalDetect = totalLasers + totalPads + totalSensors;
        if (totalDetect == 0)
            return;

        m_PlayerDetectCounter = m_PlayerDetectCounter + 1;
        padMod = m_PlayerDetectCounter % 2;
        padDue = false;
        sensorDue = false;
        if (padMod == 0 && totalPads > 0)
            padDue = true;
        if (m_PlayerDetectCounter >= LFPG_SENSOR_SCAN_TICK_DIVISOR)
        {
            m_PlayerDetectCounter = 0;
            if (totalSensors > 0)
                sensorDue = true;
        }

        needPlayers = false;
        if (totalLasers > 0)
            needPlayers = true;
        if (padDue)
            needPlayers = true;
        if (sensorDue)
            needPlayers = true;
        if (needPlayers)
            LFPG_RebuildPlayerCells();

        nowMs = g_Game.GetTime();
        raycasts = 0;
        for (scanIndex = 0; scanIndex < totalLasers; scanIndex = scanIndex + 1)
        {
            if (scanIndex >= m_RegisteredLasers.Count())
                break;
            laser = m_RegisteredLasers[scanIndex];
            if (!laser)
                continue;
            if (laser.LFPG_HasBeamTransformChanged())
            {
                laser.LFPG_UpdateBeamRaycast();
                raycasts = raycasts + 1;
            }
        }

        if (m_LaserRaycastCursor >= totalLasers)
            m_LaserRaycastCursor = 0;
        // ceil(N / 23) per 300ms slice covers N in at most 23 slices:
        // 23 * 300ms = 6.9s, matching the pre-T2 global maintenance cycle.
        maintenanceBudget = (totalLasers + 22) / 23;
        if (maintenanceBudget < 4)
            maintenanceBudget = 4;
        maintenanceRaycasts = 0;
        rayChecked = 0;
        while (rayChecked < totalLasers && maintenanceRaycasts < maintenanceBudget)
        {
            registryIndex = m_LaserRaycastCursor;
            m_LaserRaycastCursor = m_LaserRaycastCursor + 1;
            if (m_LaserRaycastCursor >= totalLasers)
                m_LaserRaycastCursor = 0;
            rayChecked = rayChecked + 1;
            if (registryIndex >= m_RegisteredLasers.Count())
                continue;
            laser = m_RegisteredLasers[registryIndex];
            if (!laser)
                continue;
            if (laser.LFPG_IsBeamMaintenanceDue(nowMs))
            {
                laser.LFPG_UpdateBeamRaycast();
                raycasts = raycasts + 1;
                maintenanceRaycasts = maintenanceRaycasts + 1;
            }
        }

        laserEvaluated = 0;
        laserDormant = 0;
        laserChanged = 0;
        if (totalLasers > 0)
        {
            if (m_LaserDetectCursor >= totalLasers)
                m_LaserDetectCursor = 0;
            processCount = totalLasers;
            for (processIndex = 0; processIndex < processCount; processIndex = processIndex + 1)
            {
                registryIndex = m_LaserDetectCursor;
                m_LaserDetectCursor = m_LaserDetectCursor + 1;
                if (m_LaserDetectCursor >= totalLasers)
                    m_LaserDetectCursor = 0;
                if (registryIndex >= m_RegisteredLasers.Count())
                    continue;
                laser = m_RegisteredLasers[registryIndex];
                if (!laser)
                    continue;

                candidateCount = LFPG_CollectPlayerCandidates(laser.GetPosition(), LFPG_LASER_BEAM_RANGE_M + 1.0);
                laserEvaluated = laserEvaluated + 1;
                if (candidateCount == 0)
                    laserDormant = laserDormant + 1;
                stateChanged = laser.LFPG_EvaluateCrossing(m_PlayerCandidates);
                if (stateChanged)
                {
                    deviceId = laser.LFPG_GetDeviceId();
                    if (deviceId != "")
                        RequestPropagate(deviceId);
                    laserChanged = laserChanged + 1;
                }
            }
        }
        if (laserChanged > 0)
        {
            string laserMsg = "[PlayerDetect] Lasers: ";
            laserMsg = laserMsg + laserChanged.ToString();
            laserMsg = laserMsg + " changed state";
            LFPG_Util.Info(laserMsg);
        }

        padEvaluated = 0;
        padDormant = 0;
        padChanged = 0;
        if (padDue)
        {
            if (m_PadDetectCursor >= totalPads)
                m_PadDetectCursor = 0;
            processCount = totalPads;
            for (processIndex = 0; processIndex < processCount; processIndex = processIndex + 1)
            {
                registryIndex = m_PadDetectCursor;
                m_PadDetectCursor = m_PadDetectCursor + 1;
                if (m_PadDetectCursor >= totalPads)
                    m_PadDetectCursor = 0;
                if (registryIndex >= m_RegisteredPads.Count())
                    continue;
                pad = m_RegisteredPads[registryIndex];
                if (!pad)
                    continue;

                candidateCount = LFPG_CollectPlayerCandidates(pad.GetPosition(), 1.0);
                padEvaluated = padEvaluated + 1;
                if (candidateCount == 0)
                    padDormant = padDormant + 1;
                stateChanged = pad.LFPG_EvaluatePresence(m_PlayerCandidates);
                if (stateChanged)
                {
                    deviceId = pad.LFPG_GetDeviceId();
                    if (deviceId != "")
                        RequestPropagate(deviceId);
                    padChanged = padChanged + 1;
                }
            }
        }
        if (padChanged > 0)
        {
            string padMsg = "[PlayerDetect] Pads: ";
            padMsg = padMsg + padChanged.ToString();
            padMsg = padMsg + " changed state";
            LFPG_Util.Info(padMsg);
        }

        sensorEvaluated = 0;
        sensorDormant = 0;
        sensorChanged = 0;
        if (sensorDue)
        {
            if (m_SensorDetectCursor >= totalSensors)
                m_SensorDetectCursor = 0;
            processCount = totalSensors;
            for (processIndex = 0; processIndex < processCount; processIndex = processIndex + 1)
            {
                registryIndex = m_SensorDetectCursor;
                m_SensorDetectCursor = m_SensorDetectCursor + 1;
                if (m_SensorDetectCursor >= totalSensors)
                    m_SensorDetectCursor = 0;
                if (registryIndex >= m_RegisteredSensors.Count())
                    continue;
                sensor = m_RegisteredSensors[registryIndex];
                if (!sensor)
                    continue;

                candidateCount = LFPG_CollectPlayerCandidates(sensor.GetPosition(), LFPG_SENSOR_RANGE_M);
                sensorEvaluated = sensorEvaluated + 1;
                if (candidateCount == 0)
                    sensorDormant = sensorDormant + 1;
                stateChanged = sensor.LFPG_EvaluateDetection(m_PlayerCandidates);
                if (stateChanged)
                {
                    deviceId = sensor.LFPG_GetDeviceId();
                    if (deviceId != "")
                        RequestPropagate(deviceId);
                    sensorChanged = sensorChanged + 1;
                }
            }
        }
        if (sensorChanged > 0)
        {
            string sensorMsg = "[PlayerDetect] Sensors: ";
            sensorMsg = sensorMsg + sensorChanged.ToString();
            sensorMsg = sensorMsg + " changed state";
            LFPG_Util.Info(sensorMsg);
        }

        if (LFPG_PERFDIAG_ENABLED)
        {
            m_PerfDiagLaserEvaluations = m_PerfDiagLaserEvaluations + laserEvaluated;
            m_PerfDiagPadEvaluations = m_PerfDiagPadEvaluations + padEvaluated;
            m_PerfDiagSensorEvaluations = m_PerfDiagSensorEvaluations + sensorEvaluated;
            m_PerfDiagLaserDormant = m_PerfDiagLaserDormant + laserDormant;
            m_PerfDiagPadDormant = m_PerfDiagPadDormant + padDormant;
            m_PerfDiagSensorDormant = m_PerfDiagSensorDormant + sensorDormant;
            m_PerfDiagLaserChanges = m_PerfDiagLaserChanges + laserChanged;
            m_PerfDiagPadChanges = m_PerfDiagPadChanges + padChanged;
            m_PerfDiagSensorChanges = m_PerfDiagSensorChanges + sensorChanged;
            string detectDiag = "LFPG_PERFDIAG detection laser_eval=";
            detectDiag = detectDiag + laserEvaluated.ToString();
            detectDiag = detectDiag + " pad_eval=";
            detectDiag = detectDiag + padEvaluated.ToString();
            detectDiag = detectDiag + " sensor_eval=";
            detectDiag = detectDiag + sensorEvaluated.ToString();
            detectDiag = detectDiag + " laser_dormant=";
            detectDiag = detectDiag + laserDormant.ToString();
            detectDiag = detectDiag + " pad_dormant=";
            detectDiag = detectDiag + padDormant.ToString();
            detectDiag = detectDiag + " sensor_dormant=";
            detectDiag = detectDiag + sensorDormant.ToString();
            detectDiag = detectDiag + " laser_changed=";
            detectDiag = detectDiag + laserChanged.ToString();
            detectDiag = detectDiag + " pad_changed=";
            detectDiag = detectDiag + padChanged.ToString();
            detectDiag = detectDiag + " sensor_changed=";
            detectDiag = detectDiag + sensorChanged.ToString();
            detectDiag = detectDiag + " raycasts=";
            detectDiag = detectDiag + raycasts.ToString();
            detectDiag = detectDiag + " laser_eval_total=";
            detectDiag = detectDiag + m_PerfDiagLaserEvaluations.ToString();
            detectDiag = detectDiag + " pad_eval_total=";
            detectDiag = detectDiag + m_PerfDiagPadEvaluations.ToString();
            detectDiag = detectDiag + " sensor_eval_total=";
            detectDiag = detectDiag + m_PerfDiagSensorEvaluations.ToString();
            Print(detectDiag);
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
        // g_Game.GetTime() returns milliseconds (same as water pump / tank timers).
        float nowMs = g_Game.GetTime();
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

        // v4.3 (Audit fix F5): Direct Cast replaces CallFunctionParams for reads.
        // Eliminates 8 string-resolved dispatches per battery per tick.
        // Both BatteryBase and BatteryAdapter expose identical API names.
        // Writes also use direct Cast (no hoisted strings needed).
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

            // --- Read battery entity state via direct Cast (v4.3) ---
            // Try BatteryBase first (Medium/Large — most common), then Adapter.
            float storedEnergy = 0.0;
            float maxStored = 0.0;
            float maxCharge = 0.0;
            float maxDischarge = 0.0;
            float efficiency = 1.0;
            float selfDischargeRate = 0.0;
            bool dischargeEnabled = true;
            bool outputEnabled = true;

            LFPG_BatteryBase batBase = LFPG_BatteryBase.Cast(batEnt);
            LFPG_BatteryAdapter batAdapt = null;
            if (batBase)
            {
                storedEnergy = batBase.LFPG_GetStoredEnergy();
                maxStored = batBase.LFPG_GetMaxStoredEnergy();
                maxCharge = batBase.LFPG_GetMaxChargeRate();
                maxDischarge = batBase.LFPG_GetMaxDischargeRate();
                efficiency = batBase.LFPG_GetEfficiency();
                selfDischargeRate = batBase.LFPG_GetSelfDischargeRate();
                dischargeEnabled = batBase.LFPG_IsDischargeEnabled();
                outputEnabled = batBase.LFPG_IsOutputEnabled();
            }
            else
            {
                batAdapt = LFPG_BatteryAdapter.Cast(batEnt);
                if (batAdapt)
                {
                    storedEnergy = batAdapt.LFPG_GetStoredEnergy();
                    maxStored = batAdapt.LFPG_GetMaxStoredEnergy();
                    maxCharge = batAdapt.LFPG_GetMaxChargeRate();
                    maxDischarge = batAdapt.LFPG_GetMaxDischargeRate();
                    efficiency = batAdapt.LFPG_GetEfficiency();
                    selfDischargeRate = batAdapt.LFPG_GetSelfDischargeRate();
                    dischargeEnabled = batAdapt.LFPG_IsDischargeEnabled();
                    outputEnabled = batAdapt.LFPG_IsOutputEnabled();
                }
                else
                {
                    continue;
                }
            }

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

            // v4.2: chargeRateDisplay derived from actual stored delta, NOT netFlow.
            // v4.3 (Audit fix F1): Post-clamp guard. The effectiveMax clamp on
            // newStored can produce a larger delta than physical rates allow
            // (e.g. healthRatio drops → effectiveMax < storedEnergy → snap down).
            float chargeRateDisplay = (newStored - storedEnergy) / deltaSec;
            if (chargeRateDisplay > maxCharge)
            {
                chargeRateDisplay = maxCharge;
            }
            float negMaxDischDisplay = -maxDischarge;
            if (chargeRateDisplay < negMaxDischDisplay)
            {
                chargeRateDisplay = negMaxDischDisplay;
            }

            // --- Write back to entity (v4.3: direct Cast, no CallFunctionParams) ---
            if (batBase)
            {
                batBase.LFPG_SetStoredEnergy(newStored);
                if (newDischargeEnabled != dischargeEnabled)
                {
                    batBase.LFPG_SetDischargeEnabled(newDischargeEnabled);
                }
                batBase.LFPG_SetChargeRateCurrent(chargeRateDisplay);
            }
            else if (batAdapt)
            {
                batAdapt.LFPG_SetStoredEnergy(newStored);
                if (newDischargeEnabled != dischargeEnabled)
                {
                    batAdapt.LFPG_SetDischargeEnabled(newDischargeEnabled);
                }
                batAdapt.LFPG_SetChargeRateCurrent(chargeRateDisplay);
            }

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
