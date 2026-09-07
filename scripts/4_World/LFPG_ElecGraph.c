// =========================================================
// LF_PowerGrid - Electrical Graph (v1.0)
//
// In-memory directed graph of the electrical network.
// Nodes = devices, edges = wires. Rebuilt from wire data at
// startup, maintained incrementally during runtime.
//
// NOT persisted — wires are the source of truth.
// Server-only: all public methods are guarded by #ifdef SERVER.
//
// === POWER ALLOCATION (v1.0) ===
// Binary all-off policy: if totalDemand > availableOutput on any
// distributor node, ALL downstream of that node receives 0.
// AllocateOutput (~90 lines) is the sole allocation function.
// Overload state is per-node (m_Overloaded bool), not per-edge.
// PASSTHROUGH always reports real demand (self + downstream) via
// m_LastStableOutput, even when unpowered — prevents oscillation.
//
// === KEY SUBSYSTEMS ===
// - ProcessDirtyQueue: BFS propagation with node+edge budgets,
//   requeue limits, and deferred requeue for deep chains.
// - AllocateOutput: binary demand/allocation on outgoing edges.
//   Multi-source split via CountPoweredIncoming (Combiner pattern).
// - SyncNodeToEntity: syncs LoadRatio+Overloaded (SOURCE),
//   Powered+Overloaded (PASSTHROUGH), Powered (CONSUMER/CAMERA).
// - ValidateConsumerStates: bidirectional zombie/dark detection.
// - PostBulkRebuild: type-aware orphan SyncVar reset.
//
// === ANTI-OSCILLATION ===
// PASSTHROUGH demand signal = downstreamDemand + selfConsumption,
// always written to m_LastStableOutput regardless of power state.
// Demand is a topology property, not a power-flow property.
// Cold-start fallback: m_MaxOutput when m_LastStableOutput=0.
//
// === SAFETY NETS ===
// - Component Watchdog: per-subnet node limit (v0.7.31)
// - Atomic Graph Mutations: deferred cleanup (v0.7.34)
// - Topology-aware downstream propagation (v0.7.38 B1)
// - Carryover requeue count reset (v0.7.38 RC-09)
// - Deferred requeue for requeue-limit orphans (v0.8.3)
// =========================================================

// The 4_World facade exposes the graph contract required by world callers.
// Runtime state and implementation bodies are hosted by the mission layer.
// A neutral fallback is returned when the mission factory is unavailable.

class LFPG_ElecGraph
{
    void RebuildFromWires(LFPG_NetworkManager mgr)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] RebuildFromWires called without mission factory (fallback base)");
        #endif
        return;
    }


    bool OnWireAdded(string sourceId, string targetId, string sourcePort, string targetPort, LFPG_WireData wireRef)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] OnWireAdded called without mission factory (fallback base)");
        #endif
        return false;
    }


    void OnWireRemoved(string sourceId, string targetId, string sourcePort, string targetPort)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] OnWireRemoved called without mission factory (fallback base)");
        #endif
        return;
    }


    void OnDeviceRemoved(string deviceId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] OnDeviceRemoved called without mission factory (fallback base)");
        #endif
        return;
    }

    void BeginGraphMutation()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] BeginGraphMutation called without mission factory (fallback base)");
        #endif
        return;
    }

    void EndGraphMutation()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] EndGraphMutation called without mission factory (fallback base)");
        #endif
        return;
    }


    bool DetectCycleIfAdded(string sourceId, string targetId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] DetectCycleIfAdded called without mission factory (fallback base)");
        #endif
        return false;
    }


    LFPG_ElecNode GetNode(string deviceId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] GetNode called without mission factory (fallback base)");
        #endif
        return null;
    }


    array<ref LFPG_ElecEdge> GetOutgoing(string deviceId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] GetOutgoing called without mission factory (fallback base)");
        #endif
        return null;
    }

    float SumOutgoingAllocations(string nodeId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] SumOutgoingAllocations called without mission factory (fallback base)");
        #endif
        return 0.0;
    }

    bool WouldExceedComponentLimit(string sourceId, string targetId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] WouldExceedComponentLimit called without mission factory (fallback base)");
        #endif
        return false;
    }


    array<ref LFPG_ElecEdge> GetIncoming(string deviceId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] GetIncoming called without mission factory (fallback base)");
        #endif
        return null;
    }


    int GetNodeCount()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] GetNodeCount called without mission factory (fallback base)");
        #endif
        return 0;
    }


    int GetEdgeCount()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] GetEdgeCount called without mission factory (fallback base)");
        #endif
        return 0;
    }


    int GetComponentCount()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] GetComponentCount called without mission factory (fallback base)");
        #endif
        return 0;
    }


    int GetLastRebuildMs()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] GetLastRebuildMs called without mission factory (fallback base)");
        #endif
        return 0;
    }


    int GetLastProcessMs()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] GetLastProcessMs called without mission factory (fallback base)");
        #endif
        return 0;
    }


    int GetCurrentEpoch()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] GetCurrentEpoch called without mission factory (fallback base)");
        #endif
        return 0;
    }


    int GetDirtyQueueSize()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] GetDirtyQueueSize called without mission factory (fallback base)");
        #endif
        return 0;
    }

    int GetOverloadedSourceCount()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] GetOverloadedSourceCount called without mission factory (fallback base)");
        #endif
        return 0;
    }

    int GetLastEdgesVisited()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] GetLastEdgesVisited called without mission factory (fallback base)");
        #endif
        return 0;
    }

    bool VerifyPassthroughPowered(string nodeId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] VerifyPassthroughPowered called without mission factory (fallback base)");
        #endif
        return false;
    }


    void PostBulkRebuild(LFPG_NetworkManager mgr)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] PostBulkRebuild called without mission factory (fallback base)");
        #endif
        return;
    }

    void MarkNodeDirty(string nodeId, int mask)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] MarkNodeDirty called without mission factory (fallback base)");
        #endif
        return;
    }


    void MarkSourcesDirty()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] MarkSourcesDirty called without mission factory (fallback base)");
        #endif
        return;
    }


    int ProcessDirtyQueue(int nodeBudget, int edgeBudget)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] ProcessDirtyQueue called without mission factory (fallback base)");
        #endif
        return 0;
    }


    void PopulateAllNodeElecStates()
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] PopulateAllNodeElecStates called without mission factory (fallback base)");
        #endif
        return;
    }


    void RefreshSourceState(string nodeId)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] RefreshSourceState called without mission factory (fallback base)");
        #endif
        return;
    }

    bool IsPortReceivingPower(string deviceId, string portName)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] IsPortReceivingPower called without mission factory (fallback base)");
        #endif
        return false;
    }

    void SetOutputPortEnabled(string deviceId, string portName, bool enabled)
    {
        #ifdef SERVER
        LFPG_Util.Error("[LFPG_ElecGraph] SetOutputPortEnabled called without mission factory (fallback base)");
        #endif
        return;
    }
};