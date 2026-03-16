// =========================================================
// LF_PowerGrid - data structures (v1.0, Sprint Overload Simplification)
//
// Pure data classes. No logic. No entity references.
// Instantiated by LFPG_ElecGraph (4_World) or persistence (3_Game).
//
// The LFPG_ElecGraph class (4_World) owns and manipulates these.
//
// v1.0: Overload simplification — all-off binary policy.
//   - ElecNode: m_OverloadMask/m_WarningMask → m_Overloaded (bool)
//   - ElecEdge: removed m_Priority (always 0), m_EdgeIndex (bitmask dead)
//   - ElecEdge: m_Flags reduced to LFPG_EDGE_ENABLED only
//
// v0.7.28 (Refactor): Consolidated missing data structures:
//   - LFPG_WireData: single wire endpoint descriptor
//   - LFPG_PersistBlob: persistence envelope for per-device wires
//   - LFPG_VanillaWireEntry: single vanilla wire endpoint
//   - LFPG_VanillaWireStore: persistence envelope for vanilla wires
//
// v0.7.44 (Level 3): Added m_TargetNetLow/m_TargetNetHigh to WireData.
//   Enables client-side CableRenderer to resolve target entities via
//   NetworkID when DeviceId has not yet stabilized (SyncVar lag).
//   Session-only: persisted as 0, re-populated at runtime by server.
//   Forward/backward compatible (no schema bump needed).
// =========================================================

// ---- Wire data (per-wire, serialized to JSON via PersistBlob) ----
class LFPG_WireData
{
    // Target device identity
    string m_TargetDeviceId;
    string m_TargetPort;
    string m_SourcePort;

    // Creator tracking (for per-player cleanup)
    string m_CreatorId;

    // Route geometry
    ref array<vector> m_Waypoints;

    // Sprint 4.3: priority for load allocation (0 = default)
    int m_Priority;

    // Sprint 4.3: bitfield for future states (0 = default)
    int m_Flags;

    // v0.7.44 (Level 3): NetworkID of target device.
    // Session-only (NetworkIDs change across server restarts).
    // 0,0 means "not available" (e.g. just loaded from disk).
    // Populated by server when wire is created or re-populated
    // during ValidateAllWiresAndPropagate self-heal.
    // Used by CableRenderer as fallback when FindById(m_TargetDeviceId)
    // fails due to client-side SyncVar lag after Kit placement.
    int m_TargetNetLow;
    int m_TargetNetHigh;

    void LFPG_WireData()
    {
        m_TargetDeviceId = "";
        m_TargetPort = "";
        m_SourcePort = "";
        m_CreatorId = "";
        m_Waypoints = new array<vector>;
        m_Priority = 0;
        m_Flags = 0;
        m_TargetNetLow = 0;
        m_TargetNetHigh = 0;
    }
};

// ---- Persistence envelope for per-device wires ----
class LFPG_PersistBlob
{
    int ver;
    ref array<ref LFPG_WireData> wires;

    void LFPG_PersistBlob()
    {
        ver = LFPG_PERSIST_VER;
        wires = new array<ref LFPG_WireData>;
    }
};

// ---- Single vanilla wire entry ----
class LFPG_VanillaWireEntry
{
    string m_OwnerDeviceId;
    string m_TargetDeviceId;
    string m_SourcePort;
    string m_TargetPort;
    string m_CreatorId;
    ref array<vector> m_Waypoints;

    void LFPG_VanillaWireEntry()
    {
        m_OwnerDeviceId = "";
        m_TargetDeviceId = "";
        m_SourcePort = "";
        m_TargetPort = "";
        m_CreatorId = "";
        m_Waypoints = new array<vector>;
    }
};

// ---- Persistence envelope for vanilla wires ----
class LFPG_VanillaWireStore
{
    int ver;
    ref array<ref LFPG_VanillaWireEntry> entries;

    void LFPG_VanillaWireStore()
    {
        ver = LFPG_VANILLA_PERSIST_VER;
        entries = new array<ref LFPG_VanillaWireEntry>;
    }
};

// =========================================================
// Electrical graph data structures
// =========================================================

class LFPG_ElecNode
{
    // --- Sprint 4.1: active fields ---
    string m_DeviceId;         // Primary key (string, consistent with codebase)
    int    m_DeviceType;       // LFPG_DeviceType enum value
    int    m_ComponentId;      // Connected subgraph ID (-1 = unassigned)

    // --- Sprint 4.2: electrical state (now active) ---
    bool   m_Powered;          // True if node receives sufficient power
    float  m_InputPower;       // Sum of incoming edge power
    float  m_OutputPower;      // Power delivered to outgoing edges
    float  m_LastStableOutput; // Previous output for change detection (epsilon)

    // --- Sprint 4.2: propagation (now active) ---
    bool   m_Dirty;            // Has pending changes to process
    bool   m_InQueue;          // Currently in dirty queue (dedup guard)
    int    m_DirtyMask;        // Bitmask: DIRTY_TOPOLOGY | DIRTY_INPUT | DIRTY_INTERNAL
    int    m_RequeueCount;     // Times re-enqueued this epoch (cycle protection)
    int    m_LastEpoch;        // Last epoch this node was processed

    // --- Sprint 4.2+4.3: capacity ---
    float  m_MaxOutput;        // Source capacity or passthrough limit
    float  m_Consumption;      // Consumer demand (populated at warmup)

    // --- Sprint 4.3 + v1.0 simplification: load telemetry ---
    float  m_LoadRatio;        // totalDemand / m_MaxOutput (0.0 = idle, >1.0 = overloaded)
    bool   m_Overloaded;       // v1.0: true when totalDemand > available (all-off policy)
    float  m_LastSyncedLoadRatio;  // Last load ratio synced to entity (avoids redundant syncs)

    // P1: Cached gate capability. True only for PASSTHROUGH devices that
    // implement LFPG_IsGateOpen (e.g. PushButton). False for Splitter,
    // Combiner, CeilingLight, Monitor. Avoids entity lookup + dynamic
    // dispatch on every ProcessDirtyQueue tick for non-gated devices.
    bool   m_IsGated;

    // v2.1: Cached gate-closed state. Updated each time a gated node
    // is processed in PDQ. Read by AllocateOutput to suppress cold-start
    // inflation for closed-gate targets (prevents phantom demand / false
    // overload). When gate opens, cleared → cold-start resumes normally
    // for bootstrap. Default false (non-gated devices ignore this).
    bool   m_GateClosed;

    // --- v2.0: Battery (PASSTHROUGH + energy storage) ---
    // Set by NetworkManager battery timer (~5s). Graph reads only.
    // Default 0.0 = zero regression for all non-battery devices.
    //
    // m_VirtualGeneration: discharge power from storage (W).
    //   Added to inputSum in PDQ PASSTHROUGH evaluation.
    //   Subtracted from demand signal (upstream sees reduced need).
    //   Capped by timer: min(maxDischargeRate, storedEnergy * 3600 / delta).
    //
    // m_SoftDemand: charge absorption want (W).
    //   Added to demand signal but NEVER causes overload.
    //   AllocateOutput Pass 3 distributes surplus proportionally.
    //   Set to 0 when battery is full or discharge-only mode.
    //
    // m_SoftDemandRatio: fraction of m_LastStableOutput that is soft.
    //   Computed in PDQ demand signal section.
    //   Read by upstream AllocateOutput to split hard/soft per edge.
    //   Propagates through PASSTHROUGH chains automatically.
    float  m_VirtualGeneration;
    float  m_SoftDemand;
    float  m_SoftDemandRatio;

    void LFPG_ElecNode()
    {
        m_DeviceId = "";
        m_DeviceType = LFPG_DeviceType.UNKNOWN;
        m_ComponentId = -1;
        m_Powered = false;
        m_InputPower = 0.0;
        m_OutputPower = 0.0;
        m_LastStableOutput = 0.0;
        m_Dirty = false;
        m_InQueue = false;
        m_DirtyMask = 0;
        m_RequeueCount = 0;
        m_LastEpoch = 0;
        m_MaxOutput = 0.0;
        m_Consumption = 0.0;
        m_LoadRatio = 0.0;
        m_Overloaded = false;
        m_LastSyncedLoadRatio = -1.0;
        m_IsGated = false;
        m_GateClosed = false;
        m_VirtualGeneration = 0.0;
        m_SoftDemand = 0.0;
        m_SoftDemandRatio = 0.0;
    }
};

class LFPG_ElecEdge
{
    // --- Sprint 4.1: active fields ---
    string          m_SourceNodeId;   // DeviceId of wire owner
    string          m_TargetNodeId;   // DeviceId of wire target
    string          m_SourcePort;     // Output port on source
    string          m_TargetPort;     // Input port on target
    ref LFPG_WireData m_WireRef;     // Reference to original WireData (not a copy)

    // --- Sprint 4.2: active (used in propagation) ---
    int             m_Flags;          // LFPG_EDGE_ENABLED

    // --- Sprint 4.3 + v1.0: load allocation ---
    float           m_Demand;         // Downstream demand seen through this edge
    float           m_AllocatedPower; // Power actually allocated to this edge this epoch

    void LFPG_ElecEdge()
    {
        m_SourceNodeId = "";
        m_TargetNodeId = "";
        m_SourcePort = "";
        m_TargetPort = "";
        m_WireRef = null;
        m_Flags = LFPG_EDGE_ENABLED;
        m_Demand = 0.0;
        m_AllocatedPower = 0.0;
    }
};
