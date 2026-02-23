// =========================================================
// LF_PowerGrid - data structures (v0.7.22, Sprint 4.3)
//
// Pure data classes. No logic. No entity references.
// Instantiated by LFPG_ElecGraph (4_World) or persistence (3_Game).
//
// The LFPG_ElecGraph class (4_World) owns and manipulates these.
//
// Sprint 4.3 additions:
//   - m_LoadRatio, m_OverloadMask on ElecNode (source telemetry)
//   - m_Demand on ElecEdge (downstream demand for priority allocation)
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

    // --- Sprint 4.3: load telemetry (source nodes only) ---
    float  m_LoadRatio;        // totalDemand / m_MaxOutput (0.0 = idle, >1.0 = overloaded)
    int    m_OverloadMask;     // Bitmask: bit N = 1 means outgoing edge N is overloaded/brownout
    float  m_LastSyncedLoadRatio;  // Last load ratio synced to entity (avoids redundant syncs)

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
        m_OverloadMask = 0;
        m_LastSyncedLoadRatio = -1.0;
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

    // --- Sprint 4.2+4.3: active (used in propagation + load allocation) ---
    int             m_Priority;       // Higher = served first during overload
    int             m_Flags;          // LFPG_EDGE_ENABLED | LFPG_EDGE_OVERLOADED | LFPG_EDGE_BROWNOUT

    // --- Sprint 4.3: load allocation ---
    float           m_Demand;         // Downstream demand seen through this edge
    float           m_AllocatedPower; // Power actually allocated to this edge this epoch
    int             m_EdgeIndex;      // Index within source's outgoing array (for overload mask bit)

    void LFPG_ElecEdge()
    {
        m_SourceNodeId = "";
        m_TargetNodeId = "";
        m_SourcePort = "";
        m_TargetPort = "";
        m_WireRef = null;
        m_Priority = 0;
        m_Flags = LFPG_EDGE_ENABLED;
        m_Demand = 0.0;
        m_AllocatedPower = 0.0;
        m_EdgeIndex = 0;
    }
};
