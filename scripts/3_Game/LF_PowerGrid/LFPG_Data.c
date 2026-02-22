// =========================================================
// LF_PowerGrid - serializable data
// =========================================================

class LFPG_WireData
{
    // Versioned fields (Phase 1)
    string m_TargetDeviceId;
    string m_TargetPort;

    // Optional/forward-compatible fields
    // Source OUT port on the owner (Phase1 default is "output_1").
    // If empty on old saves, treat as "output_1".
    // Note: uses literal here (not LFPG_PORT_OUTPUT_1) to avoid
    // initialization order dependency between Data and Defines.
    string m_SourcePort = "output_1";

    // Creator PlayerIdentity plainId (for quotas/permissions). May be empty on old saves.
    string m_CreatorId;

    ref array<vector> m_Waypoints;

    void LFPG_WireData()
    {
        m_Waypoints = new array<vector>;
    }
};

// Per-device wire persistence blob (serialized to JSON string in OnStoreSave).
// Schema version history:
//   v1 (original): ver, wires[]
//   v2 (Sprint 3): no field changes, establishes migration chain + sanitization
//   v3 (Sprint 4, planned): may add m_Priority, m_Flags per wire
class LFPG_PersistBlob
{
    int ver = LFPG_PERSIST_VER;
    ref array<ref LFPG_WireData> wires;

    void LFPG_PersistBlob()
    {
        wires = new array<ref LFPG_WireData>;
    }
};

// Vanilla wire persistence (central store, saved to profile JSON)
class LFPG_VanillaWireEntry
{
    string ownerDeviceId;
    string targetDeviceId;
    string targetPort;
    string sourcePort;
    string creatorId;
    ref array<vector> waypoints;

    void LFPG_VanillaWireEntry()
    {
        waypoints = new array<vector>;
    }
};

// Schema version history:
//   v1 (original): ver, entries[]
//   v2 (Sprint 3): no field changes, establishes migration chain + sanitization
class LFPG_VanillaWireStore
{
    int ver = LFPG_VANILLA_PERSIST_VER;
    ref array<ref LFPG_VanillaWireEntry> entries;

    void LFPG_VanillaWireStore()
    {
        entries = new array<ref LFPG_VanillaWireEntry>;
    }
};

// =========================================================
// Electrical graph data structures (Sprint 4.1)
//
// Pure data classes — no behavior, no side effects.
// The LFPG_ElecGraph class (4_World) owns and manipulates these.
//
// Fields marked "Sprint 4.2/4.3 reserved" are declared now for
// documentation but not initialized or used until their sprint.
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

    // --- Sprint 4.3 reserved: capacity ---
    float  m_MaxOutput;        // Source capacity or passthrough limit
    float  m_Consumption;      // Consumer demand (populated at warmup)

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
    int             m_Priority;
    int             m_Flags;         // LFPG_EDGE_ENABLED | LFPG_EDGE_OVERLOADED

    void LFPG_ElecEdge()
    {
        m_SourceNodeId = "";
        m_TargetNodeId = "";
        m_SourcePort = "";
        m_TargetPort = "";
        m_WireRef = null;
        m_Priority = 0;
        m_Flags = LFPG_EDGE_ENABLED;
    }
};
