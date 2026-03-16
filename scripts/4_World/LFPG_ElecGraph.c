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

class LFPG_ElecGraph
{
    // --- Nodes ---
    protected ref map<string, ref LFPG_ElecNode> m_Nodes;

    // --- Dual adjacency ---
    protected ref map<string, ref array<ref LFPG_ElecEdge>> m_Outgoing;
    protected ref map<string, ref array<ref LFPG_ElecEdge>> m_Incoming;

    // --- Connected components ---
    protected int m_NextComponentId;
    protected bool m_ComponentsDirty;

    // --- Propagation (Sprint 4.2 active) ---
    protected ref array<string> m_DirtyQueue;
    protected int m_DirtyQueueHead;      // H4: head index for O(1) dequeue without array copy
    protected int m_CurrentEpoch;
    protected int m_LastRequeueResetEpoch; // H3: epoch at which requeue counts were last reset

    // --- Telemetry ---
    protected int m_NodeCount;
    protected int m_EdgeCount;
    protected int m_LastRebuildMs;
    protected int m_LastProcessMs;        // Sprint 4.2 S2: time spent in ProcessDirtyQueue

    // --- Sprint 4.3: Targeted requeue tracking ---
    // v0.7.33 (Fix #15): Changed from array<string> to map<string, bool>.
    // Prevents duplicates when a node is dequeued, re-dirtied, and re-enqueued
    // within the same epoch (feedback topologies). Map provides O(1) dedup
    // vs O(n) linear scan on array. ResetRequeueCounts no longer iterates dupes.
    protected ref map<string, bool> m_EnqueuedThisEpoch;

    // --- Sprint 4.3: Edge budget tracking ---
    protected int m_EdgesVisitedThisEpoch;

    // v0.7.46: Flag set by AllocateOutput when any edge's
    // m_AllocatedPower changed. ProcessDirtyQueue Step 3 uses this to
    // re-enqueue downstream even when total output is unchanged.
    protected bool m_AllocChanged;

    // v2.0: Soft demand total from last AllocateOutput call.
    // Set by AllocateOutput, read by PDQ demand signal section.
    // Avoids redundant outgoing edge iteration in demand signal.
    // Reset to 0.0 per-node in PDQ (alongside m_AllocChanged).
    protected float m_LastAllocSoftDemand;

    // --- v0.7.31 (Bloque B): Component Watchdog ---
    // m_ComponentSizes: populated in RebuildComponents(), keyed by componentId.
    // m_WdgQueue/m_WdgVisited: reusable BFS buffers for CountComponentLimited().
    // Max 256 entries → Clear() is negligible cost.
    protected ref map<int, int>     m_ComponentSizes;
    protected ref array<string>     m_WdgQueue;
    protected ref map<string, bool> m_WdgVisited;

    // --- v0.7.32 (Bloque C): Consumer Zombie Validation ---
    // Periodic sweep of consumer nodes to detect and fix "zombie" powered state.
    // m_ValidateTickCount: counts ProcessDirtyQueue calls (advances even when idle).
    // m_LastValidateTick: tick count when last validation batch ran.
    // m_ValidateNodeIdx: round-robin index into m_Nodes for budgeted batching.
    // m_ValidateFixCount: telemetry — total zombies fixed since startup.
    protected int m_ValidateTickCount;
    protected int m_LastValidateTick;
    protected int m_ValidateNodeIdx;
    protected int m_ValidateFixCount;

    // --- v0.7.34 (Bloque E): Atomic Graph Mutations ---
    // When m_MutationActive is true, CleanupOrphanNode is deferred to
    // EndGraphMutation. Prevents premature node deletion during multi-op
    // sequences (e.g. replace wire = remove old + add new).
    // m_MutationDepth supports safe nesting (Begin can be called N times,
    // only the outermost End triggers cleanup).
    // m_DeferredOrphanCleanup: reusable buffer, cleared on End.
    protected bool m_MutationActive;
    protected int  m_MutationDepth;
    protected ref array<string> m_DeferredOrphanCleanup;

    // --- v0.8.3: Deferred requeue for requeue-limit orphans ---
    // When a node hits LFPG_MAX_REQUEUE_PER_EPOCH and is skipped, its dirty
    // state is preserved and the nodeId is added here. At the end of the epoch,
    // these nodes are re-inserted into the dirty queue for next-epoch processing
    // with reset requeue counts. Prevents permanent orphaning when downstream
    // converges while the node is limit-skipped.
    protected ref array<string> m_DeferredRequeue;

    // --- v0.7.43 (Fix 3): NetworkID backup for entity re-resolution ---
    // When DeviceRegistry ref goes stale (entity streamed/recreated),
    // SyncNodeToEntity can re-resolve via GetObjectByNetworkId.
    // Populated in EnsureNode when entity is available. Session-local
    // (NetworkIDs change on restart, graph is rebuilt anyway).
    protected ref map<string, int> m_NodeNetLow;
    protected ref map<string, int> m_NodeNetHigh;

    void LFPG_ElecGraph()
    {
        m_Nodes = new map<string, ref LFPG_ElecNode>;
        m_Outgoing = new map<string, ref array<ref LFPG_ElecEdge>>;
        m_Incoming = new map<string, ref array<ref LFPG_ElecEdge>>;
        m_DirtyQueue = new array<string>;
        m_DirtyQueueHead = 0;
        m_NextComponentId = 0;
        m_ComponentsDirty = true;
        m_CurrentEpoch = 0;
        m_LastRequeueResetEpoch = 0;
        m_NodeCount = 0;
        m_EdgeCount = 0;
        m_LastRebuildMs = 0;
        m_LastProcessMs = 0;
        m_EnqueuedThisEpoch = new map<string, bool>;
        m_EdgesVisitedThisEpoch = 0;
        m_AllocChanged = false;
        m_LastAllocSoftDemand = 0.0;

        // v0.7.31 (Bloque B): Component Watchdog buffers
        m_ComponentSizes = new map<int, int>;
        m_WdgQueue = new array<string>;
        m_WdgVisited = new map<string, bool>;

        // v0.7.32 (Bloque C): Consumer Zombie Validation
        m_ValidateTickCount = 0;
        m_LastValidateTick = 0;
        m_ValidateNodeIdx = 0;
        m_ValidateFixCount = 0;

        // v0.7.34 (Bloque E): Atomic Graph Mutations
        m_MutationActive = false;
        m_MutationDepth = 0;
        m_DeferredOrphanCleanup = new array<string>;

        // v0.8.3: Deferred requeue
        m_DeferredRequeue = new array<string>;

        // v0.7.43 (Fix 3): NetworkID backup maps
        m_NodeNetLow = new map<string, int>;
        m_NodeNetHigh = new map<string, int>;
    }

    // ===========================
    // Full rebuild from wires
    // ===========================

    // Reconstructs the entire graph from existing wire data.
    // Called once at server startup after all loads complete.
    // Does NOT modify the wire data — read only.
    void RebuildFromWires(LFPG_NetworkManager mgr)
    {
        #ifdef SERVER
        if (!mgr)
            return;

        int startMs = GetGame().GetTime();

        // Clear everything
        m_Nodes.Clear();
        m_Outgoing.Clear();
        m_Incoming.Clear();
        m_DirtyQueue.Clear();
        m_DirtyQueueHead = 0;
        m_NodeCount = 0;
        m_EdgeCount = 0;
        m_NodeNetLow.Clear();
        m_NodeNetHigh.Clear();

        // v0.7.34 (Bloque E): Full rebuild invalidates any active mutation
        if (m_MutationActive)
        {
            m_MutationActive = false;
            m_MutationDepth = 0;
            m_DeferredOrphanCleanup.Clear();
        }
        m_DeferredRequeue.Clear();

        // Step 1: Iterate all registered devices to create nodes
        ref array<EntityAI> allDevices = new array<EntityAI>;
        LFPG_DeviceRegistry.Get().GetAll(allDevices);

        int di;
        for (di = 0; di < allDevices.Count(); di = di + 1)
        {
            EntityAI devObj = allDevices[di];
            if (!devObj)
                continue;

            string devId = LFPG_DeviceAPI.GetOrCreateDeviceId(devObj);
            if (devId == "")
                continue;

            EnsureNode(devId, devObj);
        }

        // Step 2: Iterate wire sources — LFPG devices with wire stores
        for (di = 0; di < allDevices.Count(); di = di + 1)
        {
            EntityAI srcObj = allDevices[di];
            if (!srcObj)
                continue;

            if (!LFPG_DeviceAPI.HasWireStore(srcObj))
                continue;

            string srcId = LFPG_DeviceAPI.GetOrCreateDeviceId(srcObj);
            if (srcId == "")
                continue;

            ref array<ref LFPG_WireData> wires = LFPG_DeviceAPI.GetDeviceWires(srcObj);
            if (!wires)
                continue;

            int wi;
            for (wi = 0; wi < wires.Count(); wi = wi + 1)
            {
                LFPG_WireData wd = wires[wi];
                if (!wd)
                    continue;

                AddEdgeInternal(srcId, wd.m_TargetDeviceId, wd.m_SourcePort, wd.m_TargetPort, wd);
            }
        }

        // Step 3: Iterate vanilla wires
        int vCount = mgr.GetVanillaWireOwnerCount();
        int vi;
        for (vi = 0; vi < vCount; vi = vi + 1)
        {
            string vOwnerId = mgr.GetVanillaWireOwnerKey(vi);
            ref array<ref LFPG_WireData> vWires = mgr.GetVanillaWires(vOwnerId);
            if (!vWires)
                continue;

            int vwi;
            for (vwi = 0; vwi < vWires.Count(); vwi = vwi + 1)
            {
                LFPG_WireData vwd = vWires[vwi];
                if (!vwd)
                    continue;

                string srcPort = vwd.m_SourcePort;
                if (srcPort == "")
                    srcPort = LFPG_PORT_OUTPUT_1;

                AddEdgeInternal(vOwnerId, vwd.m_TargetDeviceId, srcPort, vwd.m_TargetPort, vwd);
            }
        }

        // Step 4: Prune nodes with no edges
        ref array<string> emptyNodes = new array<string>;
        int ni;
        for (ni = 0; ni < m_Nodes.Count(); ni = ni + 1)
        {
            string nid = m_Nodes.GetKey(ni);
            bool hasOut = false;
            bool hasIn = false;

            ref array<ref LFPG_ElecEdge> outEdges;
            if (m_Outgoing.Find(nid, outEdges) && outEdges && outEdges.Count() > 0)
                hasOut = true;

            ref array<ref LFPG_ElecEdge> inEdges;
            if (m_Incoming.Find(nid, inEdges) && inEdges && inEdges.Count() > 0)
                hasIn = true;

            if (!hasOut && !hasIn)
                emptyNodes.Insert(nid);
        }

        int ei;
        for (ei = 0; ei < emptyNodes.Count(); ei = ei + 1)
        {
            m_Nodes.Remove(emptyNodes[ei]);
            m_Outgoing.Remove(emptyNodes[ei]);
            m_Incoming.Remove(emptyNodes[ei]);
            // v0.7.45 (H5): Consistent with OnDeviceRemoved and CleanupOrphanNode.
            m_NodeNetLow.Remove(emptyNodes[ei]);
            m_NodeNetHigh.Remove(emptyNodes[ei]);
        }
        m_NodeCount = m_Nodes.Count();

        // Step 5: Rebuild component IDs
        m_ComponentsDirty = true;
        RebuildComponents();

        int elapsed = GetGame().GetTime() - startMs;
        m_LastRebuildMs = elapsed;

        string rbMsg = "[ElecGraph] Rebuilt: " + m_NodeCount.ToString() + " nodes, ";
        rbMsg = rbMsg + m_EdgeCount.ToString() + " edges, ";
        rbMsg = rbMsg + m_NextComponentId.ToString() + " components in ";
        rbMsg = rbMsg + elapsed.ToString() + "ms";
        LFPG_Util.Info(rbMsg);
        #endif
    }

    // ===========================
    // Incremental operations
    // ===========================

    // v0.7.31 (Bloque B): BFS acotada para watchdog por componente.
    // Counts nodes in the connected component containing startId.
    // Early exits when count exceeds limit (returns limit+1).
    // Uses reusable buffers m_WdgQueue/m_WdgVisited — zero alloc per call.
    // Undirected traversal: walks both m_Outgoing and m_Incoming.
    protected int CountComponentLimited(string startId, int limit)
    {
        #ifdef SERVER
        if (startId == "" || limit <= 0)
            return 0;

        m_WdgQueue.Clear();
        m_WdgVisited.Clear();

        bool bTrue = true;
        m_WdgQueue.Insert(startId);
        m_WdgVisited.Set(startId, bTrue);

        int count = 0;
        int headIdx = 0;

        while (headIdx < m_WdgQueue.Count())
        {
            string currId = m_WdgQueue[headIdx];
            headIdx = headIdx + 1;
            count = count + 1;

            // Early exit: component exceeds limit
            if (count > limit)
                return count;

            // Explore outgoing neighbors
            ref array<ref LFPG_ElecEdge> outEdges;
            if (m_Outgoing.Find(currId, outEdges) && outEdges)
            {
                int oi;
                for (oi = 0; oi < outEdges.Count(); oi = oi + 1)
                {
                    ref LFPG_ElecEdge oEdge = outEdges[oi];
                    if (oEdge && oEdge.m_TargetNodeId != "")
                    {
                        bool oVisited = false;
                        m_WdgVisited.Find(oEdge.m_TargetNodeId, oVisited);
                        if (!oVisited)
                        {
                            m_WdgVisited.Set(oEdge.m_TargetNodeId, bTrue);
                            m_WdgQueue.Insert(oEdge.m_TargetNodeId);
                        }
                    }
                }
            }

            // Explore incoming neighbors (undirected traversal)
            ref array<ref LFPG_ElecEdge> inEdges;
            if (m_Incoming.Find(currId, inEdges) && inEdges)
            {
                int ii;
                for (ii = 0; ii < inEdges.Count(); ii = ii + 1)
                {
                    ref LFPG_ElecEdge iEdge = inEdges[ii];
                    if (iEdge && iEdge.m_SourceNodeId != "")
                    {
                        bool iVisited = false;
                        m_WdgVisited.Find(iEdge.m_SourceNodeId, iVisited);
                        if (!iVisited)
                        {
                            m_WdgVisited.Set(iEdge.m_SourceNodeId, bTrue);
                            m_WdgQueue.Insert(iEdge.m_SourceNodeId);
                        }
                    }
                }
            }
        }

        return count;
        #else
        return 0;
        #endif
    }

    bool OnWireAdded(string sourceId, string targetId, string sourcePort, string targetPort, LFPG_WireData wireRef)
    {
        #ifdef SERVER
        // ==========================================
        // PASO 0: Null guards + self-loop
        // ==========================================
        if (sourceId == "" || targetId == "")
            return false;

        if (sourceId == targetId)
            return false;

        // ==========================================
        // PASO 1: Global hard-cap O(1)
        // ==========================================
        if (m_NodeCount >= LFPG_MAX_NODES_GLOBAL)
        {
            string capMsg = "[ElecGraph] OnWireAdded REJECTED: global cap (" + m_NodeCount.ToString() + "/" + LFPG_MAX_NODES_GLOBAL.ToString() + ")";
            LFPG_Util.Warn(capMsg);
            return false;
        }

        // ==========================================
        // PASO 2: Component Watchdog (v0.7.31)
        // ==========================================
        ref LFPG_ElecNode nodeA;
        ref LFPG_ElecNode nodeB;
        bool hasA = m_Nodes.Find(sourceId, nodeA);
        bool hasB = m_Nodes.Find(targetId, nodeB);

        // Fast-paths: only when components are clean (already rebuilt)
        int limit = LFPG_MAX_NODES_PER_COMPONENT;
        int sizeA = 0;
        int sizeB = 0;
        int remaining = 0;
        int totalSize = 0;
        if (!m_ComponentsDirty)
        {
            int compA = -1;
            int compB = -1;
            if (hasA && nodeA)
                compA = nodeA.m_ComponentId;
            if (hasB && nodeB)
                compB = nodeB.m_ComponentId;

            // 2a: Same component — internal cable, size unchanged → ALLOW
            if (compA >= 0 && compA == compB)
            {
                // Skip watchdog, proceed directly to insert
            }
            // 2b: Different known components — O(1) size lookup
            else if (compA >= 0 && compB >= 0)
            {
                sizeA = 0;
                sizeB = 0;
                m_ComponentSizes.Find(compA, sizeA);
                m_ComponentSizes.Find(compB, sizeB);

                int mergedSize = sizeA + sizeB;
                if (mergedSize > LFPG_MAX_NODES_PER_COMPONENT)
                {
                    string mergeMsg = "[ElecGraph] OnWireAdded REJECTED: merge exceeds component limit (" + mergedSize.ToString() + "/" + LFPG_MAX_NODES_PER_COMPONENT.ToString() + ")";
                    LFPG_Util.Warn(mergeMsg);
                    return false;
                }
                // Merged size OK, proceed to insert
            }
            // 2c: One or both nodes are new (compId == -1) — BFS fallback
            else
            {
                limit = LFPG_MAX_NODES_PER_COMPONENT;
                int bfsSizeA = 1;
                int bfsSizeB = 1;

                if (hasA && nodeA)
                {
                    if (compA >= 0)
                    {
                        m_ComponentSizes.Find(compA, bfsSizeA);
                    }
                    else
                    {
                        bfsSizeA = CountComponentLimited(sourceId, limit);
                    }
                }

                if (bfsSizeA > limit)
                {
                    string wMsg = "[ElecGraph] OnWireAdded REJECTED: source component exceeds limit";
                    LFPG_Util.Warn(wMsg);
                    return false;
                }

                remaining = limit - bfsSizeA;
                if (remaining <= 0)
                {
                    string wMsg2 = "[ElecGraph] OnWireAdded REJECTED: no budget for target";
                    LFPG_Util.Warn(wMsg2);
                    return false;
                }

                if (hasB && nodeB)
                {
                    if (compB >= 0)
                    {
                        m_ComponentSizes.Find(compB, bfsSizeB);
                    }
                    else
                    {
                        bfsSizeB = CountComponentLimited(targetId, remaining);
                    }
                }

                totalSize = bfsSizeA + bfsSizeB;
                if (totalSize > limit)
                {
                    string szMsg = "[ElecGraph] OnWireAdded REJECTED: merged size (" + totalSize.ToString() + "/" + limit.ToString() + ")";
                    LFPG_Util.Warn(szMsg);
                    return false;
                }
            }
        }
        else
        {
            // ==========================================
            // PASO 3: Components dirty — BFS fallback
            // ==========================================
            limit = LFPG_MAX_NODES_PER_COMPONENT;
            sizeA = 1;
            bool ranBfsA = false;

            if (hasA && nodeA)
            {
                sizeA = CountComponentLimited(sourceId, limit);
                ranBfsA = true;
            }

            if (sizeA > limit)
            {
                string wMsgD = "[ElecGraph] OnWireAdded REJECTED: source exceeds limit (dirty)";
                LFPG_Util.Warn(wMsgD);
                return false;
            }

            // If we ran BFS for A, check if B was already visited (= same component).
            // This avoids double-counting that would cause false rejections.
            // Only safe to check m_WdgVisited when it was freshly populated by BFS-A.
            bool bInA = false;
            if (ranBfsA && hasB && nodeB)
            {
                m_WdgVisited.Find(targetId, bInA);
            }

            if (!bInA)
            {
                // Different components (or A was new) — count B with remaining budget
                remaining = limit - sizeA;
                if (remaining <= 0)
                {
                    string wMsgD2 = "[ElecGraph] OnWireAdded REJECTED: no budget for target (dirty)";
                    LFPG_Util.Warn(wMsgD2);
                    return false;
                }

                sizeB = 1;
                if (hasB && nodeB)
                {
                    sizeB = CountComponentLimited(targetId, remaining);
                }

                totalSize = sizeA + sizeB;
                if (totalSize > limit)
                {
                    string szMsgD = "[ElecGraph] OnWireAdded REJECTED: merged size (" + totalSize.ToString() + "/" + limit.ToString() + ") (dirty)";
                    LFPG_Util.Warn(szMsgD);
                    return false;
                }
            }
            // else: bInA — same component, size doesn't grow, sizeA <= limit already checked
        }

        // ==========================================
        // PASO 4: Insert edge (original logic preserved)
        // ==========================================
        EntityAI srcObj = LFPG_DeviceRegistry.Get().FindById(sourceId);
        EntityAI tgtObj = LFPG_DeviceRegistry.Get().FindById(targetId);

        EnsureNode(sourceId, srcObj);
        EnsureNode(targetId, tgtObj);

        bool inserted = AddEdgeInternal(sourceId, targetId, sourcePort, targetPort, wireRef);
        if (!inserted)
        {
            string wInsMsg = "[ElecGraph] OnWireAdded: edge not inserted " + sourceId + " -> " + targetId;
            LFPG_Util.Warn(wInsMsg);
            return false;
        }

        // [DIAG PT-CHAIN] Punto 1: Post-edge insert verification
        if (LFPG_DIAG_PT_CHAIN)
        {
            ref LFPG_ElecNode diagSrc;
            ref LFPG_ElecNode diagTgt;
            string dSrcType = "?";
            string dTgtType = "?";
            string dSrcOut = "0";
            string dSrcPow = "0";
            string dTgtOut = "0";
            string dTgtPow = "0";
            if (m_Nodes.Find(sourceId, diagSrc) && diagSrc)
            {
                dSrcType = diagSrc.m_DeviceType.ToString();
                dSrcOut = diagSrc.m_OutputPower.ToString();
                dSrcPow = diagSrc.m_Powered.ToString();
            }
            if (m_Nodes.Find(targetId, diagTgt) && diagTgt)
            {
                dTgtType = diagTgt.m_DeviceType.ToString();
                dTgtOut = diagTgt.m_OutputPower.ToString();
                dTgtPow = diagTgt.m_Powered.ToString();
            }
            string ptLog1 = "[PT-CHAIN] OnWireAdded OK: ";
            ptLog1 = ptLog1 + sourceId;
            ptLog1 = ptLog1 + "(type=" + dSrcType;
            ptLog1 = ptLog1 + " out=" + dSrcOut;
            ptLog1 = ptLog1 + " pow=" + dSrcPow + ")";
            ptLog1 = ptLog1 + " -> " + targetId;
            ptLog1 = ptLog1 + "(type=" + dTgtType;
            ptLog1 = ptLog1 + " out=" + dTgtOut;
            ptLog1 = ptLog1 + " pow=" + dTgtPow + ")";
            ptLog1 = ptLog1 + " port=" + sourcePort + "->" + targetPort;
            LFPG_Util.Info(ptLog1);
        }

        m_ComponentsDirty = true;

        MarkNodeDirty(sourceId, LFPG_DIRTY_TOPOLOGY);
        MarkNodeDirty(targetId, LFPG_DIRTY_TOPOLOGY);

        return true;
        #else
        return false;
        #endif
    }

    void OnWireRemoved(string sourceId, string targetId, string sourcePort, string targetPort)
    {
        #ifdef SERVER
        RemoveEdgeInternal(sourceId, targetId, sourcePort, targetPort);

        // v0.7.34 (Bloque E): Defer orphan cleanup during atomic mutations.
        // In a replace sequence (remove old + add new), the target node
        // temporarily has no incoming edges after remove. Immediate cleanup
        // would delete it, losing m_Consumption/m_MaxOutput state. The new
        // OnWireAdded would recreate it via EnsureNode, but with default
        // values — causing a stale-state propagation bug.
        if (m_MutationActive)
        {
            m_DeferredOrphanCleanup.Insert(sourceId);
            m_DeferredOrphanCleanup.Insert(targetId);
        }
        else
        {
            // v0.7.37 (Audit 6, H3): Force target powered=false BEFORE orphan cleanup.
            // CleanupOrphanNode may remove the target from the graph if it has
            // no remaining edges. Once removed, MarkNodeDirty below is a no-op
            // and the entity's m_PoweredNet stays stale (true). Forcing false
            // here ensures it goes dark. Propagation re-enables it next tick
            // if an alternate power path exists.
            // Only needed outside mutations: during atomic ops, cleanup is
            // deferred so the node survives and MarkNodeDirty works normally.
            // Skipping here also avoids an unnecessary entity resolve + RPC
            // and prevents visible powered→unpowered→powered flicker during
            // replace wire sequences.
            EntityAI tgtObj = LFPG_DeviceRegistry.Get().FindById(targetId);
            if (!tgtObj)
            {
                tgtObj = LFPG_DeviceAPI.ResolveVanillaDevice(targetId);
            }
            if (tgtObj)
            {
                LFPG_DeviceAPI.SetPowered(tgtObj, false);
            }
            CleanupOrphanNode(sourceId);
            CleanupOrphanNode(targetId);
        }

        m_ComponentsDirty = true;

        MarkNodeDirty(sourceId, LFPG_DIRTY_TOPOLOGY);
        MarkNodeDirty(targetId, LFPG_DIRTY_TOPOLOGY);
        #endif
    }

    void OnDeviceRemoved(string deviceId)
    {
        #ifdef SERVER
        if (deviceId == "")
            return;

        ref array<string> affectedNeighbors = new array<string>;

        ref array<ref LFPG_ElecEdge> outEdges;
        if (m_Outgoing.Find(deviceId, outEdges) && outEdges)
        {
            int oi = outEdges.Count() - 1;
            while (oi >= 0)
            {
                ref LFPG_ElecEdge oEdge = outEdges[oi];
                if (oEdge)
                {
                    RemoveFromIncoming(oEdge.m_TargetNodeId, deviceId, oEdge.m_SourcePort, oEdge.m_TargetPort);
                    affectedNeighbors.Insert(oEdge.m_TargetNodeId);
                    m_EdgeCount = m_EdgeCount - 1;
                }
                oi = oi - 1;
            }
        }

        ref array<ref LFPG_ElecEdge> inEdges;
        if (m_Incoming.Find(deviceId, inEdges) && inEdges)
        {
            int ii = inEdges.Count() - 1;
            while (ii >= 0)
            {
                ref LFPG_ElecEdge iEdge = inEdges[ii];
                if (iEdge)
                {
                    RemoveFromOutgoing(iEdge.m_SourceNodeId, deviceId, iEdge.m_SourcePort, iEdge.m_TargetPort);
                    affectedNeighbors.Insert(iEdge.m_SourceNodeId);
                    m_EdgeCount = m_EdgeCount - 1;
                }
                ii = ii - 1;
            }
        }

        m_Nodes.Remove(deviceId);
        m_Outgoing.Remove(deviceId);
        m_Incoming.Remove(deviceId);
        // v0.7.45 (H5): Clean up cached NetworkIDs for the removed node.
        // Without this, m_NodeNetLow/High grow unbounded on servers with
        // device turnover. CleanupOrphanNode handles neighbors, but the
        // primary removed node never passes through that path.
        m_NodeNetLow.Remove(deviceId);
        m_NodeNetHigh.Remove(deviceId);
        m_NodeCount = m_Nodes.Count();

        int ai;
        for (ai = 0; ai < affectedNeighbors.Count(); ai = ai + 1)
        {
            // v0.7.34 (Bloque E): Defer orphan cleanup during atomic mutations.
            // Neighbors may be targets of subsequent operations in the same batch.
            if (m_MutationActive)
            {
                m_DeferredOrphanCleanup.Insert(affectedNeighbors[ai]);
            }
            else
            {
                CleanupOrphanNode(affectedNeighbors[ai]);
            }
            MarkNodeDirty(affectedNeighbors[ai], LFPG_DIRTY_TOPOLOGY);
        }

        m_ComponentsDirty = true;
        #endif
    }

    // ===========================
    // v0.7.34 (Bloque E): Atomic Graph Mutations
    // ===========================

    // Begin a batch of graph mutations. While active, CleanupOrphanNode
    // is deferred to EndGraphMutation to prevent premature node deletion
    // during multi-op sequences (replace wire, multi-port cut, etc.).
    //
    // Nesting-safe: Begin can be called N times; only the outermost End
    // triggers deferred cleanups. This allows helper methods to wrap
    // their own Begin/End without conflicting with caller batches.
    //
    // Usage (caller in NetworkManager or PlayerRPC):
    //   m_Graph.BeginGraphMutation();
    //   m_Graph.OnWireRemoved(oldSrc, oldTgt, srcP, tgtP);
    //   m_Graph.OnWireAdded(newSrc, newTgt, srcP, tgtP, wireRef);
    //   m_Graph.EndGraphMutation();
    //
    // Cost: zero when not in mutation (single bool check in OnWireRemoved).
    void BeginGraphMutation()
    {
        #ifdef SERVER
        m_MutationDepth = m_MutationDepth + 1;
        m_MutationActive = true;
        #endif
    }

    // End a batch of graph mutations. When the outermost batch closes
    // (depth reaches 0), processes all deferred orphan cleanups.
    //
    // Dirty marks were already accumulated during the mutation via
    // MarkNodeDirty (idempotent — m_InQueue prevents duplicates).
    // Components will be rebuilt on next ProcessDirtyQueue if needed
    // (m_ComponentsDirty was set by each OnWireAdded/Removed).
    void EndGraphMutation()
    {
        #ifdef SERVER
        if (m_MutationDepth <= 0)
        {
            string wMutMsg = "[ElecGraph] EndGraphMutation called without matching Begin";
            LFPG_Util.Warn(wMutMsg);
            m_MutationActive = false;
            m_MutationDepth = 0;
            // v0.7.34: Safety clear — prevent stale deferred entries from leaking
            m_DeferredOrphanCleanup.Clear();
            return;
        }

        m_MutationDepth = m_MutationDepth - 1;

        if (m_MutationDepth > 0)
            return;  // Still inside nested mutation

        m_MutationActive = false;

        // Process deferred orphan cleanups.
        // Some nodes may have gained new edges during the mutation,
        // so CleanupOrphanNode correctly skips them (checks hasOut/hasIn).
        int deferredCount = m_DeferredOrphanCleanup.Count();
        int ci;
        for (ci = 0; ci < deferredCount; ci = ci + 1)
        {
            CleanupOrphanNode(m_DeferredOrphanCleanup[ci]);
        }

        if (deferredCount > 0)
        {
            string dbgFlush = "[ElecGraph] EndGraphMutation: flushed " + deferredCount.ToString() + " deferred orphan checks";
            LFPG_Util.Debug(dbgFlush);
        }

        m_DeferredOrphanCleanup.Clear();
        #endif
    }

    // Check if a graph mutation batch is currently active.
    // Used by callers to avoid redundant Begin/End wrapping.
    bool IsGraphMutationActive()
    {
        return m_MutationActive;
    }

    // ===========================
    // Cycle detection
    // ===========================

    bool DetectCycleIfAdded(string sourceId, string targetId)
    {
        #ifdef SERVER
        if (sourceId == targetId)
            return true;

        ref array<string> stack = new array<string>;
        ref map<string, bool> visited = new map<string, bool>;

        stack.Insert(targetId);
        int visitedCount = 0;
        bool bVisTrue = true;

        while (stack.Count() > 0)
        {
            // v0.7.26 (Audit 4): Depth limit guard for very dense graphs.
            // Conservatively assumes cycle if limit reached (safe: rejects wire).
            if (visitedCount >= LFPG_DFS_MAX_VISITED)
            {
                string wDfsMsg = "[ElecGraph] DetectCycle: visited limit reached (" + visitedCount.ToString() + "), assuming cycle";
                LFPG_Util.Warn(wDfsMsg);
                return true;
            }

            int topIdx = stack.Count() - 1;
            string current = stack[topIdx];
            stack.Remove(topIdx);

            if (current == sourceId)
                return true;

            bool alreadyVisited = false;
            visited.Find(current, alreadyVisited);
            if (alreadyVisited)
                continue;

            visited.Set(current, bVisTrue);
            visitedCount = visitedCount + 1;

            ref array<ref LFPG_ElecEdge> edges;
            if (m_Outgoing.Find(current, edges) && edges)
            {
                int edgeI;
                for (edgeI = 0; edgeI < edges.Count(); edgeI = edgeI + 1)
                {
                    ref LFPG_ElecEdge edge = edges[edgeI];
                    if (edge && edge.m_TargetNodeId != "")
                    {
                        bool tgtVisited = false;
                        visited.Find(edge.m_TargetNodeId, tgtVisited);
                        if (!tgtVisited)
                        {
                            stack.Insert(edge.m_TargetNodeId);
                        }
                    }
                }
            }
        }

        return false;
        #else
        return false;
        #endif
    }

    // ===========================
    // Connected components
    // ===========================

    void RebuildComponents()
    {
        #ifdef SERVER
        if (!m_ComponentsDirty)
            return;

        int ri;
        for (ri = 0; ri < m_Nodes.Count(); ri = ri + 1)
        {
            ref LFPG_ElecNode rNode = m_Nodes.GetElement(ri);
            if (rNode)
                rNode.m_ComponentId = -1;
        }

        // v0.7.31: Clear component sizes for rebuild
        m_ComponentSizes.Clear();

        int nextId = 0;

        int ni;
        for (ni = 0; ni < m_Nodes.Count(); ni = ni + 1)
        {
            ref LFPG_ElecNode startNode = m_Nodes.GetElement(ni);
            if (!startNode)
                continue;
            if (startNode.m_ComponentId != -1)
                continue;

            // v0.7.31: Count nodes per component during BFS
            int compSize = 0;

            ref array<string> queue = new array<string>;
            queue.Insert(m_Nodes.GetKey(ni));
            int head = 0;

            while (head < queue.Count())
            {
                string curId = queue[head];
                head = head + 1;

                ref LFPG_ElecNode curNode;
                if (!m_Nodes.Find(curId, curNode) || !curNode)
                    continue;
                if (curNode.m_ComponentId != -1)
                    continue;

                curNode.m_ComponentId = nextId;
                compSize = compSize + 1;

                ref array<ref LFPG_ElecEdge> outE;
                if (m_Outgoing.Find(curId, outE) && outE)
                {
                    int oi;
                    for (oi = 0; oi < outE.Count(); oi = oi + 1)
                    {
                        ref LFPG_ElecEdge oEdge = outE[oi];
                        if (oEdge)
                        {
                            ref LFPG_ElecNode tgtNode;
                            if (m_Nodes.Find(oEdge.m_TargetNodeId, tgtNode) && tgtNode)
                            {
                                if (tgtNode.m_ComponentId == -1)
                                    queue.Insert(oEdge.m_TargetNodeId);
                            }
                        }
                    }
                }

                ref array<ref LFPG_ElecEdge> inE;
                if (m_Incoming.Find(curId, inE) && inE)
                {
                    int ii;
                    for (ii = 0; ii < inE.Count(); ii = ii + 1)
                    {
                        ref LFPG_ElecEdge iEdge = inE[ii];
                        if (iEdge)
                        {
                            ref LFPG_ElecNode srcNode;
                            if (m_Nodes.Find(iEdge.m_SourceNodeId, srcNode) && srcNode)
                            {
                                if (srcNode.m_ComponentId == -1)
                                    queue.Insert(iEdge.m_SourceNodeId);
                            }
                        }
                    }
                }
            }

            // v0.7.31: Store component size for O(1) watchdog lookups
            m_ComponentSizes.Set(nextId, compSize);

            nextId = nextId + 1;
        }

        m_NextComponentId = nextId;
        m_ComponentsDirty = false;
        #endif
    }

    // ===========================
    // Internal helpers
    // ===========================

    protected void EnsureNode(string deviceId, EntityAI obj)
    {
        #ifdef SERVER
        if (deviceId == "")
            return;

        ref LFPG_ElecNode existing;
        if (m_Nodes.Find(deviceId, existing))
            return;

        ref LFPG_ElecNode node = new LFPG_ElecNode();
        node.m_DeviceId = deviceId;

        if (obj)
        {
            node.m_DeviceType = LFPG_DeviceAPI.GetDeviceType(obj);

            // v0.7.43 (Fix 3): Cache NetworkID for fallback resolution.
            // If DeviceRegistry ref goes stale, SyncNodeToEntity can
            // re-resolve via GetObjectByNetworkId.
            int nLow = 0;
            int nHigh = 0;
            obj.GetNetworkID(nLow, nHigh);
            if (nLow != 0 || nHigh != 0)
            {
                m_NodeNetLow.Set(deviceId, nLow);
                m_NodeNetHigh.Set(deviceId, nHigh);
            }

            // v0.7.38 (BugFix): Populate electrical properties on creation.
            // Previously only set by PopulateAllNodeElecStates (bulk rebuild).
            // Without this, runtime wire-adds created nodes with consumption=0
            // and maxOutput=0 — causing no-overload and always-powered bugs.
            if (node.m_DeviceType == LFPG_DeviceType.SOURCE)
            {
                node.m_MaxOutput = LFPG_DeviceAPI.GetCapacity(obj);
                bool sourceOn = false;
                if (LFPG_DeviceAPI.IsSource(obj))
                {
                    sourceOn = LFPG_DeviceAPI.GetSourceOn(obj);
                }
                else
                {
                    ComponentEnergyManager emSrc = obj.GetCompEM();
                    if (emSrc)
                    {
                        sourceOn = emSrc.IsWorking();
                    }
                }
                node.m_Powered = sourceOn;
            }
            else if (node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
            {
                node.m_MaxOutput = LFPG_DeviceAPI.GetCapacity(obj);
                if (node.m_MaxOutput < LFPG_PROPAGATION_EPSILON)
                {
                    node.m_MaxOutput = LFPG_DEFAULT_PASSTHROUGH_CAPACITY;
                }
                // v0.7.47: PASSTHROUGH self-consumption (CeilingLight pattern).
                // Splitter returns 0.0 explicitly → no regression.
                node.m_Consumption = LFPG_DeviceAPI.GetConsumption(obj);
                // P1: Cache gate capability to avoid entity lookup every tick.
                node.m_IsGated = LFPG_DeviceAPI.IsGateCapable(obj);

                // v2.1: Initialize gate-closed state at rebuild time so the
                // first AllocateOutput pass uses probe demand instead of
                // m_MaxOutput for closed gates. Without this, the first epoch
                // has a 1-pass inflation before converging.
                if (node.m_IsGated)
                {
                    bool initGateOpen = LFPG_DeviceAPI.IsGateOpen(obj);
                    if (!initGateOpen)
                    {
                        node.m_GateClosed = true;
                    }
                }
            }
            else if (node.m_DeviceType == LFPG_DeviceType.CONSUMER || node.m_DeviceType == LFPG_DeviceType.CAMERA)
            {
                node.m_Consumption = LFPG_DeviceAPI.GetConsumption(obj);
            }
        }

        m_Nodes.Set(deviceId, node);
        m_NodeCount = m_Nodes.Count();
        #endif
    }

    protected bool AddEdgeInternal(string sourceId, string targetId, string srcPort, string tgtPort, LFPG_WireData wireRef)
    {
        #ifdef SERVER
        if (sourceId == "" || targetId == "")
            return false;

        ref LFPG_ElecNode srcNode;
        if (!m_Nodes.Find(sourceId, srcNode))
        {
            EntityAI srcObj = LFPG_DeviceRegistry.Get().FindById(sourceId);
            if (!srcObj)
            {
                string wSrcReg = "[ElecGraph] AddEdge rejected: source " + sourceId + " not in registry";
                LFPG_Util.Warn(wSrcReg);
                return false;
            }
            EnsureNode(sourceId, srcObj);
        }

        ref LFPG_ElecNode tgtNode;
        if (!m_Nodes.Find(targetId, tgtNode))
        {
            EntityAI tgtObj = LFPG_DeviceRegistry.Get().FindById(targetId);
            if (!tgtObj)
            {
                string wTgtReg = "[ElecGraph] AddEdge rejected: target " + targetId + " not in registry";
                LFPG_Util.Warn(wTgtReg);
                return false;
            }
            EnsureNode(targetId, tgtObj);
        }

        ref array<ref LFPG_ElecEdge> existOut;
        if (m_Outgoing.Find(sourceId, existOut) && existOut)
        {
            if (existOut.Count() >= LFPG_MAX_EDGES_PER_NODE)
            {
                string wOutLim = "[ElecGraph] AddEdge rejected: source limit " + sourceId + " (out=" + existOut.Count().ToString() + ")";
                LFPG_Util.Warn(wOutLim);
                return false;
            }
        }

        ref array<ref LFPG_ElecEdge> existIn;
        if (m_Incoming.Find(targetId, existIn) && existIn)
        {
            if (existIn.Count() >= LFPG_MAX_EDGES_PER_NODE)
            {
                string wInLim = "[ElecGraph] AddEdge rejected: target limit " + targetId + " (in=" + existIn.Count().ToString() + ")";
                LFPG_Util.Warn(wInLim);
                return false;
            }
        }

        // v0.9.3: Duplicate edge guard — skip if edge with same src+tgt+ports exists.
        // Can happen if DeviceRegistry returns same entity under multiple keys,
        // causing RebuildFromWires to iterate the same wire store twice.
        if (existOut)
        {
            int dupCheck;
            for (dupCheck = 0; dupCheck < existOut.Count(); dupCheck = dupCheck + 1)
            {
                LFPG_ElecEdge dupE = existOut[dupCheck];
                if (!dupE) continue;
                if (dupE.m_TargetNodeId == targetId && dupE.m_SourcePort == srcPort && dupE.m_TargetPort == tgtPort)
                {
                    return false;
                }
            }
        }

        // Create edge
        ref LFPG_ElecEdge edge = new LFPG_ElecEdge();
        edge.m_SourceNodeId = sourceId;
        edge.m_TargetNodeId = targetId;
        edge.m_SourcePort = srcPort;
        edge.m_TargetPort = tgtPort;
        edge.m_WireRef = wireRef;
        // Edges must start ENABLED. Without this, every check that
        // filters by LFPG_EDGE_ENABLED skips the edge entirely.
        edge.m_Flags = LFPG_EDGE_ENABLED;

        // Insert into outgoing
        ref array<ref LFPG_ElecEdge> outArr;
        if (!m_Outgoing.Find(sourceId, outArr) || !outArr)
        {
            outArr = new array<ref LFPG_ElecEdge>;
            m_Outgoing.Set(sourceId, outArr);
        }
        outArr.Insert(edge);

        // Insert into incoming
        ref array<ref LFPG_ElecEdge> inArr;
        if (!m_Incoming.Find(targetId, inArr) || !inArr)
        {
            inArr = new array<ref LFPG_ElecEdge>;
            m_Incoming.Set(targetId, inArr);
        }
        inArr.Insert(edge);

        m_EdgeCount = m_EdgeCount + 1;
        return true;
        #else
        return false;
        #endif
    }

    protected void RemoveEdgeInternal(string sourceId, string targetId, string srcPort, string tgtPort)
    {
        #ifdef SERVER
        bool removedOut = RemoveFromOutgoing(sourceId, targetId, srcPort, tgtPort);
        bool removedIn = RemoveFromIncoming(targetId, sourceId, srcPort, tgtPort);

        if (removedOut || removedIn)
        {
            m_EdgeCount = m_EdgeCount - 1;
            if (m_EdgeCount < 0)
                m_EdgeCount = 0;

            // B6 fix: Detect asymmetric edge state (present in one list but
            // not the other). This indicates a prior bug that left the graph
            // inconsistent. Log it for diagnosis.
            if (removedOut != removedIn)
            {
                string asymMsg = "[ElecGraph] WARN: asymmetric edge removal ";
                asymMsg = asymMsg + sourceId + " -> " + targetId;
                asymMsg = asymMsg + " out=" + removedOut.ToString();
                asymMsg = asymMsg + " in=" + removedIn.ToString();
                LFPG_Util.Warn(asymMsg);
            }
        }
        #endif
    }

    protected bool RemoveFromOutgoing(string ownerId, string targetId, string srcPort, string tgtPort)
    {
        #ifdef SERVER
        ref array<ref LFPG_ElecEdge> arr;
        if (!m_Outgoing.Find(ownerId, arr) || !arr)
            return false;

        int i = arr.Count() - 1;
        while (i >= 0)
        {
            ref LFPG_ElecEdge e = arr[i];
            if (e && e.m_TargetNodeId == targetId && e.m_SourcePort == srcPort && e.m_TargetPort == tgtPort)
            {
                arr.Remove(i);
                return true;
            }
            i = i - 1;
        }
        return false;
        #else
        return false;
        #endif
    }

    protected bool RemoveFromIncoming(string targetId, string sourceId, string srcPort, string tgtPort)
    {
        #ifdef SERVER
        ref array<ref LFPG_ElecEdge> arr;
        if (!m_Incoming.Find(targetId, arr) || !arr)
            return false;

        int i = arr.Count() - 1;
        while (i >= 0)
        {
            ref LFPG_ElecEdge e = arr[i];
            if (e && e.m_SourceNodeId == sourceId && e.m_SourcePort == srcPort && e.m_TargetPort == tgtPort)
            {
                arr.Remove(i);
                return true;
            }
            i = i - 1;
        }
        return false;
        #else
        return false;
        #endif
    }

     protected void CleanupOrphanNode(string deviceId)
    {
        #ifdef SERVER
        if (deviceId == "")
            return;

        ref LFPG_ElecNode node;
        if (!m_Nodes.Find(deviceId, node))
            return;

        bool hasOut = false;
        ref array<ref LFPG_ElecEdge> outE;
        if (m_Outgoing.Find(deviceId, outE) && outE && outE.Count() > 0)
            hasOut = true;

        bool hasIn = false;
        ref array<ref LFPG_ElecEdge> inE;
        if (m_Incoming.Find(deviceId, inE) && inE && inE.Count() > 0)
            hasIn = true;

        if (!hasOut && !hasIn)
        {
            // v0.7.49: Reset entity SyncVars BEFORE deleting node.
            // After removal, ProcessDirtyQueue skips the missing nodeId
            // so SyncNodeToEntity never fires. The entity retains stale
            // m_LoadRatio / m_PoweredNet / mask SyncVars forever.
            // Node still in m_Nodes here, so m_DeviceType is available.
            int orphanType = LFPG_DeviceType.CONSUMER;
            if (node)
            {
                orphanType = node.m_DeviceType;
            }
            ResetOrphanSyncVars(deviceId, orphanType);

            m_Nodes.Remove(deviceId);
            m_Outgoing.Remove(deviceId);
            m_Incoming.Remove(deviceId);
            m_NodeNetLow.Remove(deviceId);
            m_NodeNetHigh.Remove(deviceId);
            m_NodeCount = m_Nodes.Count();
        }
        #endif
    }

	// v0.7.49: Shared helper for resetting entity SyncVars when a graph
    // node becomes orphaned (zero edges). Called from:
    //   - CleanupOrphanNode (incremental path, type from live node)
    //   - PostBulkRebuild   (bulk path, type from pre-rebuild snapshot)
    //
    // Entity resolution: 3-tier (Registry -> Vanilla -> NetworkID).
    // NOTE: NetworkID fallback only effective in CleanupOrphanNode path.
    // PostBulkRebuild clears m_NodeNetLow/High during RebuildFromWires
    // (line ~300), so the map lookups return false in that context.
    // This is harmless (2 map misses) and correct: all devices are
    // resolved via Registry or Vanilla after a full rebuild.
    //
    // Returns true if entity was resolved and SyncVars were reset.
    protected bool ResetOrphanSyncVars(string deviceId, int deviceType)
    {
        #ifdef SERVER
        EntityAI orphanObj = LFPG_DeviceRegistry.Get().FindById(deviceId);
        if (!orphanObj)
        {
            orphanObj = LFPG_DeviceAPI.ResolveVanillaDevice(deviceId);
        }
        if (!orphanObj)
        {
            // NetworkID fallback (same pattern as SyncNodeToEntity).
            // Only effective in CleanupOrphanNode path where
            // m_NodeNetLow/High still exist pre-deletion.
            int cachedNetLow = 0;
            int cachedNetHigh = 0;
            bool hasNetLow = m_NodeNetLow.Find(deviceId, cachedNetLow);
            bool hasNetHigh = m_NodeNetHigh.Find(deviceId, cachedNetHigh);
            if (hasNetLow && hasNetHigh)
            {
                if (cachedNetLow != 0 || cachedNetHigh != 0)
                {
                    Object rawObj = GetGame().GetObjectByNetworkId(cachedNetLow, cachedNetHigh);
                    orphanObj = EntityAI.Cast(rawObj);
                }
            }
        }
        if (!orphanObj)
        {
            string missMsg = "[CleanupOrphan] Entity not found for " + deviceId;
            LFPG_Util.Debug(missMsg);
            return false;
        }

        if (deviceType == LFPG_DeviceType.SOURCE)
        {
            // Reset load state. m_SourceOn NOT reset (sun/fuel independent).
            LFPG_DeviceAPI.SetLoadRatio(orphanObj, 0.0);
            LFPG_DeviceAPI.SetOverloaded(orphanObj, false);
        }
        else if (deviceType == LFPG_DeviceType.CONSUMER || deviceType == LFPG_DeviceType.CAMERA)
        {
            LFPG_DeviceAPI.SetPowered(orphanObj, false);
        }
        else if (deviceType == LFPG_DeviceType.PASSTHROUGH)
        {
            LFPG_DeviceAPI.SetPowered(orphanObj, false);
            LFPG_DeviceAPI.SetOverloaded(orphanObj, false);
        }

        string resetMsg = "[CleanupOrphan] Reset SyncVars type=" + deviceType.ToString() + " id=" + deviceId;
        LFPG_Util.Info(resetMsg);
        return true;
        #else
        return false;
        #endif
    }

	
	
	
	
	
    // ===========================
    // Public accessors
    // ===========================

    LFPG_ElecNode GetNode(string deviceId)
    {
        ref LFPG_ElecNode node;
        if (m_Nodes.Find(deviceId, node))
            return node;
        return null;
    }

    array<ref LFPG_ElecEdge> GetOutgoing(string deviceId)
    {
        ref array<ref LFPG_ElecEdge> arr;
        if (m_Outgoing.Find(deviceId, arr))
            return arr;
        return null;
    }

    // v2.0: Sum allocated power on all outgoing edges for a node.
    // Used by battery timer to compute actual downstream energy consumption.
    // O(K) where K = outgoing edges (typically 1-3 for batteries).
    float SumOutgoingAllocations(string nodeId)
    {
        float total = 0.0;
        ref array<ref LFPG_ElecEdge> outEdges;
        if (!m_Outgoing.Find(nodeId, outEdges) || !outEdges)
            return 0.0;

        int oi;
        for (oi = 0; oi < outEdges.Count(); oi = oi + 1)
        {
            ref LFPG_ElecEdge edge = outEdges[oi];
            if (!edge)
                continue;
            if ((edge.m_Flags & LFPG_EDGE_ENABLED) == 0)
                continue;
            total = total + edge.m_AllocatedPower;
        }
        return total;
    }

    // v0.7.36 (Audit Feb2026): Pre-check component size before wire storage.
    // Returns true if adding a wire between sourceId and targetId would
    // cause the merged component to exceed LFPG_MAX_NODES_PER_COMPONENT.
    // Called from FinishWiring BEFORE the replacement phase so the player
    // gets clear feedback without any data mutation.
    // Logic mirrors OnWireAdded watchdog but is read-only.
    bool WouldExceedComponentLimit(string sourceId, string targetId)
    {
        #ifdef SERVER
        if (sourceId == "" || targetId == "")
            return false;

        // Global hard-cap
        if (m_NodeCount >= LFPG_MAX_NODES_GLOBAL)
            return true;

        int limit = LFPG_MAX_NODES_PER_COMPONENT;

        ref LFPG_ElecNode nodeA;
        ref LFPG_ElecNode nodeB;
        bool hasA = m_Nodes.Find(sourceId, nodeA);
        bool hasB = m_Nodes.Find(targetId, nodeB);

        // Both nodes are new (not in graph yet) → merged size = 2, always OK
        if (!hasA && !hasB)
            return false;

        if (!m_ComponentsDirty)
        {
            int compA = -1;
            int compB = -1;
            if (hasA && nodeA)
                compA = nodeA.m_ComponentId;
            if (hasB && nodeB)
                compB = nodeB.m_ComponentId;

            // Same component → no size growth
            if (compA >= 0 && compA == compB)
                return false;

            // Different known components → O(1) size lookup
            if (compA >= 0 && compB >= 0)
            {
                int sizeA = 0;
                int sizeB = 0;
                m_ComponentSizes.Find(compA, sizeA);
                m_ComponentSizes.Find(compB, sizeB);
                int mergedSize = sizeA + sizeB;
                if (mergedSize > limit)
                    return true;
                return false;
            }
        }

        // Fallback: BFS count (handles dirty components or new nodes)
        int bfsSizeA = 1;
        if (hasA && nodeA)
        {
            bfsSizeA = CountComponentLimited(sourceId, limit);
        }
        if (bfsSizeA > limit)
            return true;

        // Check if B is already in A's component (same component, no growth)
        bool bInA = false;
        if (hasA && nodeA && hasB && nodeB)
        {
            m_WdgVisited.Find(targetId, bInA);
        }
        if (bInA)
            return false;

        int remaining = limit - bfsSizeA;
        if (remaining <= 0)
            return true;

        int bfsSizeB = 1;
        if (hasB && nodeB)
        {
            bfsSizeB = CountComponentLimited(targetId, remaining);
        }

        int totalSize = bfsSizeA + bfsSizeB;
        if (totalSize > limit)
            return true;

        return false;
        #else
        return false;
        #endif
    }

    array<ref LFPG_ElecEdge> GetIncoming(string deviceId)
    {
        ref array<ref LFPG_ElecEdge> arr;
        if (m_Incoming.Find(deviceId, arr))
            return arr;
        return null;
    }

    int GetNodeCount()
    {
        return m_NodeCount;
    }

    int GetEdgeCount()
    {
        return m_EdgeCount;
    }

    int GetComponentCount()
    {
        if (m_ComponentsDirty)
            RebuildComponents();
        return m_NextComponentId;
    }

    int GetLastRebuildMs()
    {
        return m_LastRebuildMs;
    }

    int GetLastProcessMs()
    {
        return m_LastProcessMs;
    }

    int GetCurrentEpoch()
    {
        return m_CurrentEpoch;
    }

    int GetDirtyQueueSize()
    {
        return m_DirtyQueue.Count() - m_DirtyQueueHead;
    }

    // Sprint 4.3: Get count of sources currently in overload state.
    int GetOverloadedSourceCount()
    {
        #ifdef SERVER
        int count = 0;
        int ni;
        for (ni = 0; ni < m_Nodes.Count(); ni = ni + 1)
        {
            ref LFPG_ElecNode node = m_Nodes.GetElement(ni);
            if (node && node.m_DeviceType == LFPG_DeviceType.SOURCE)
            {
                if (node.m_Overloaded)
                {
                    count = count + 1;
                }
            }
        }
        return count;
        #else
        return 0;
        #endif
    }

    // Sprint 4.3: Get edges visited in last ProcessDirtyQueue call.
    int GetLastEdgesVisited()
    {
        return m_EdgesVisitedThisEpoch;
    }

    // v1.1.0: Independent verification of PASSTHROUGH power state.
    // Recalculates inputSum from edge allocations, not from cached m_Powered.
    // Used by water pump actions to guard against stale SyncVar state.
    bool VerifyPassthroughPowered(string nodeId)
    {
        #ifdef SERVER
        ref LFPG_ElecNode node;
        if (!m_Nodes.Find(nodeId, node) || !node)
            return false;

        if (node.m_DeviceType != LFPG_DeviceType.PASSTHROUGH)
            return node.m_Powered;

        float inputSum = 0.0;
        ref array<ref LFPG_ElecEdge> inEdges;
        if (m_Incoming.Find(nodeId, inEdges) && inEdges)
        {
            int ei;
            for (ei = 0; ei < inEdges.Count(); ei = ei + 1)
            {
                ref LFPG_ElecEdge edge = inEdges[ei];
                if (!edge)
                    continue;
                if ((edge.m_Flags & LFPG_EDGE_ENABLED) == 0)
                    continue;
                inputSum = inputSum + edge.m_AllocatedPower;
            }
        }

        if (node.m_Consumption > LFPG_PROPAGATION_EPSILON)
        {
            return (inputSum + LFPG_PROPAGATION_EPSILON >= node.m_Consumption);
        }

        return (inputSum > LFPG_PROPAGATION_EPSILON);
        #else
        return false;
        #endif
    }

    // ===========================
    // Bulk rebuild helpers
    // ===========================

    void PostBulkRebuild(LFPG_NetworkManager mgr)
    {
        #ifdef SERVER
        if (!mgr)
            return;

        // v0.7.49: Snapshot old node IDs AND types BEFORE rebuild.
        // After RebuildFromWires, disconnected devices are pruned from graph.
        // Propagation is additive (source->down) so orphans never get visited
        // and their entity SyncVars stay stale. We detect them here.
        // Types are snapshotted in parallel array so the orphan loop can do
        // type-aware reset (SOURCE needs LoadRatio+overloaded, CONSUMER needs
        // powered, PASSTHROUGH needs powered+overloaded).
        ref array<string> oldNodeIds = new array<string>;
        ref array<int> oldNodeTypes = new array<int>;
        int sni;
        for (sni = 0; sni < m_Nodes.Count(); sni = sni + 1)
        {
            oldNodeIds.Insert(m_Nodes.GetKey(sni));
            ref LFPG_ElecNode snapNode = m_Nodes.GetElement(sni);
            int snapType = LFPG_DeviceType.CONSUMER;
            if (snapNode)
            {
                snapType = snapNode.m_DeviceType;
            }
            oldNodeTypes.Insert(snapType);
        }

        RebuildFromWires(mgr);
        PopulateAllNodeElecStates();
        MarkSourcesDirty();

        // v0.7.49: Full SyncVar reset on orphaned devices.
        // v0.7.41 only called SetPowered(false), which is a no-op for SOURCE
        // (LFPG_SetPowered is empty on sources). Left SOURCE m_LoadRatio and
        // masks stale. Now uses type-aware ResetOrphanSyncVars.
        int orphanCount = 0;
        int oni;
        for (oni = 0; oni < oldNodeIds.Count(); oni = oni + 1)
        {
            string orphanId = oldNodeIds[oni];
            ref LFPG_ElecNode testNode;
            if (!m_Nodes.Find(orphanId, testNode))
            {
                int orphanType = oldNodeTypes[oni];
                bool resolved = ResetOrphanSyncVars(orphanId, orphanType);
                if (resolved)
                {
                    orphanCount = orphanCount + 1;
                }
            }
        }


        string infoRebuild = "[ElecGraph] PostBulkRebuild: rebuilt + populated + sources dirty";
        if (orphanCount > 0)
        {
            infoRebuild = infoRebuild + " orphans=" + orphanCount.ToString();
        }
        LFPG_Util.Info(infoRebuild);
        #endif
    }

    // ===========================
    // Sprint 4.2+4.3: Dirty marking
    // ===========================

    // Sprint 4.3: Now tracks enqueued nodes for targeted requeue reset.
    void MarkNodeDirty(string nodeId, int mask)
    {
        #ifdef SERVER
        if (nodeId == "")
            return;

        ref LFPG_ElecNode node;
        if (!m_Nodes.Find(nodeId, node) || !node)
            return;

        node.m_DirtyMask = node.m_DirtyMask | mask;
        node.m_Dirty = true;

        if (!node.m_InQueue)
        {
            node.m_InQueue = true;
            m_DirtyQueue.Insert(nodeId);
            bool bEnq = true;
            m_EnqueuedThisEpoch.Set(nodeId, bEnq);
        }
        #endif
    }

    void MarkComponentDirty(int componentId, int mask)
    {
        #ifdef SERVER
        if (componentId < 0)
            return;

        if (m_ComponentsDirty)
            RebuildComponents();

        int ni;
        for (ni = 0; ni < m_Nodes.Count(); ni = ni + 1)
        {
            ref LFPG_ElecNode node = m_Nodes.GetElement(ni);
            if (node && node.m_ComponentId == componentId)
            {
                MarkNodeDirty(m_Nodes.GetKey(ni), mask);
            }
        }
        #endif
    }

    void MarkSourcesDirty()
    {
        #ifdef SERVER
        int ni;
        for (ni = 0; ni < m_Nodes.Count(); ni = ni + 1)
        {
            ref LFPG_ElecNode node = m_Nodes.GetElement(ni);
            if (!node)
                continue;

            if (node.m_DeviceType == LFPG_DeviceType.SOURCE)
            {
                // v0.7.37 (Audit 6, M4): Use DIRTY_INTERNAL, not DIRTY_INPUT.
                // Sources manage their own powered state — they don't need input
                // re-evaluation on startup. DIRTY_INTERNAL skips the incoming edge
                // loop and directly computes output from m_Powered + m_MaxOutput.
                MarkNodeDirty(m_Nodes.GetKey(ni), LFPG_DIRTY_INTERNAL);
            }
            else if (node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH && node.m_VirtualGeneration > LFPG_PROPAGATION_EPSILON)
            {
                // v2.0: Battery PASSTHROUGH with stored energy can produce power
                // without upstream input. Must be marked dirty on startup so
                // downstream consumers wake up. Uses DIRTY_INPUT (not INTERNAL)
                // because PASSTHROUGH needs to evaluate incoming edges to combine
                // inputSum + virtualGeneration.
                MarkNodeDirty(m_Nodes.GetKey(ni), LFPG_DIRTY_INPUT);
            }
        }

        string infoSrcDirty = "[ElecGraph] MarkSourcesDirty: queued " + m_DirtyQueue.Count().ToString() + " sources";
        LFPG_Util.Info(infoSrcDirty);
        #endif
    }

    // ===========================
    // Sprint 4.3: Budgeted propagation with load allocation
    // ===========================

    int ProcessDirtyQueue(int nodeBudget, int edgeBudget)
    {
        #ifdef SERVER

        // v0.7.32 (Bloque C): Tick counter advances on every call,
        // including when queue is empty. Used for validation gating.
        m_ValidateTickCount = m_ValidateTickCount + 1;

        // v0.7.34 (Bloque E): Auto-close stale mutation if caller forgot
        // EndGraphMutation. This is a safety net — should never trigger
        // in normal operation. If it does, the log helps diagnose the
        // caller that forgot to close its batch.
        if (m_MutationActive)
        {
            string mutMsg = "[ElecGraph] ProcessDirtyQueue: mutation still active (depth=" + m_MutationDepth.ToString() + "), force-closing";
            LFPG_Util.Warn(mutMsg);
            m_MutationActive = false;
            m_MutationDepth = 0;
            int sci;
            for (sci = 0; sci < m_DeferredOrphanCleanup.Count(); sci = sci + 1)
            {
                CleanupOrphanNode(m_DeferredOrphanCleanup[sci]);
            }
            m_DeferredOrphanCleanup.Clear();
        }

        int queueLen = m_DirtyQueue.Count() - m_DirtyQueueHead;
        if (queueLen <= 0)
        {
            if (m_DirtyQueue.Count() > 0)
            {
                m_DirtyQueue.Clear();
                m_DirtyQueueHead = 0;
            }

            // v0.7.32 (Bloque C): Also validate during idle periods.
            // Queue is empty — all propagation is complete, safe to check.
            ValidateConsumerStates();

            return 0;
        }

        int startMs = GetGame().GetTime();

        if (m_ComponentsDirty)
            RebuildComponents();

        m_CurrentEpoch = m_CurrentEpoch + 1;

        if (m_LastRequeueResetEpoch != m_CurrentEpoch)
        {
            ResetRequeueCounts();
            m_LastRequeueResetEpoch = m_CurrentEpoch;
        }

        int processed = 0;
        m_EdgesVisitedThisEpoch = 0;

        while (m_DirtyQueueHead < m_DirtyQueue.Count())
        {
            if (processed >= nodeBudget)
                break;
            if (m_EdgesVisitedThisEpoch >= edgeBudget)
                break;

            string nodeId = m_DirtyQueue[m_DirtyQueueHead];
            m_DirtyQueueHead = m_DirtyQueueHead + 1;

            ref LFPG_ElecNode node;
            if (!m_Nodes.Find(nodeId, node) || !node)
                continue;

            // Skip if already processed this epoch (dedup)
            if (node.m_LastEpoch == m_CurrentEpoch)
            {
                // v0.7.40: Clear m_InQueue so future MarkNodeDirty can re-enqueue.
                // Without this, nodes consumed by epoch-skip retain m_InQueue=true
                // even though they are no longer in the queue, permanently blocking
                // re-enqueue and leaving stale dirty state (zombie node).
                node.m_InQueue = false;
                continue;  // Sprint 4.3 fix: processed NOT incremented here
            }

            // NOW increment processed (after dedup check)
            processed = processed + 1;

            if (node.m_RequeueCount > LFPG_MAX_REQUEUE_PER_EPOCH)
            {
                string wReqMsg = "[ElecGraph] Requeue limit reached for " + nodeId + " epoch=" + m_CurrentEpoch.ToString();
                LFPG_Util.Warn(wReqMsg);
                // v0.8.3: Preserve dirty state for next-epoch recovery.
                // Previous behavior cleared m_Dirty+m_DirtyMask, permanently
                // orphaning the node when all downstream converged and stopped
                // sending re-dirty signals (Bug: 3rd CeilingLight in chain at
                // zero consumption). Now: keep dirty, defer to next epoch.
                // m_InQueue=false allows future MarkNodeDirty to re-enqueue if
                // an upstream re-dirty arrives before the deferred sweep runs.
                node.m_InQueue = false;
                m_DeferredRequeue.Insert(nodeId);
                continue;
            }

            int dirtyMask = node.m_DirtyMask;

            // v0.7.46: Reset per-node (not per-branch) to prevent stale flag
            // from a previous SOURCE/PASSTHROUGH leaking into a CONSUMER iteration.
            m_AllocChanged = false;
            m_LastAllocSoftDemand = 0.0;

            // --- Step 1: Evaluate inputs ---
            bool skipInputEval = false;
            if (node.m_DeviceType == LFPG_DeviceType.SOURCE && dirtyMask == LFPG_DIRTY_INTERNAL)
            {
                skipInputEval = true;
            }

            float inputSum = 0.0;
            if (!skipInputEval)
            {
                ref array<ref LFPG_ElecEdge> inEdges;
                if (m_Incoming.Find(nodeId, inEdges) && inEdges)
                {
                    int ii;
                    for (ii = 0; ii < inEdges.Count(); ii = ii + 1)
                    {
                        ref LFPG_ElecEdge inEdge = inEdges[ii];
                        if (!inEdge)
                            continue;

                        if ((inEdge.m_Flags & LFPG_EDGE_ENABLED) == 0)
                            continue;

                        m_EdgesVisitedThisEpoch = m_EdgesVisitedThisEpoch + 1;

                        // Sprint 4.3: Use priority-aware allocated power
                        float edgePower = GetEdgeAllocatedPower(inEdge);
                        // v0.7.26 (Audit 4): Guard against NaN/negative from floating point corruption
                        if (edgePower < 0.0)
                        {
                            edgePower = 0.0;
                        }
                        // [DIAG PT-CHAIN] Point 3: Per input edge detail (PASSTHROUGH only)
                        if (LFPG_DIAG_PT_CHAIN && node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
                        {
                            string ptLog3 = "[PT-CHAIN] InputEdge: tgt=";
                            ptLog3 = ptLog3 + nodeId;
                            ptLog3 = ptLog3 + " src=" + inEdge.m_SourceNodeId;
                            ptLog3 = ptLog3 + " alloc=" + inEdge.m_AllocatedPower.ToString();
                            ptLog3 = ptLog3 + " resolved=" + edgePower.ToString();
                            ptLog3 = ptLog3 + " flags=" + inEdge.m_Flags.ToString();
                            LFPG_Util.Info(ptLog3);
                        }
                        inputSum = inputSum + edgePower;
                    }
                }
                // v0.7.27 (Audit 5): Final guard on accumulated inputSum.
                // Individual edgePower is guarded, but accumulated sum could
                // theoretically go negative from floating point corruption.
                if (inputSum < 0.0)
                {
                    inputSum = 0.0;
                }
                node.m_InputPower = inputSum;
            }
            else
            {
                inputSum = node.m_InputPower;
            }

            // --- Step 2: Compute output based on device type ---
            float newOutput = 0.0;
            bool newPowered = false;

            if (node.m_DeviceType == LFPG_DeviceType.SOURCE)
            {
                if (node.m_Powered)
                {
                    newOutput = node.m_MaxOutput;
                }
                newPowered = node.m_Powered;
            }
            else if (node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
            {
                // v2.0: Battery support — m_VirtualGeneration adds discharge
                // power from storage to effective input. For non-battery
                // PASSTHROUGH devices this is 0.0 (zero regression).
                float effectiveInput = inputSum + node.m_VirtualGeneration;

                if (effectiveInput > LFPG_PROPAGATION_EPSILON)
                {
                    // v0.7.47: Subtract self-consumption before passing downstream.
                    // CeilingLight consumes 10 u/s for its own light, rest goes out.
                    // Splitter has consumption=0 → afterSelf = effectiveInput (no regression).
                    float selfCons = node.m_Consumption;
                    if (selfCons > LFPG_PROPAGATION_EPSILON)
                    {
                        // Has self-consumption: check if input covers it
                        if (effectiveInput + LFPG_PROPAGATION_EPSILON >= selfCons)
                        {
                            // Enough for self → powered, pass remainder downstream
                            newPowered = true;
                            float afterSelf = effectiveInput - selfCons;
                            if (afterSelf < 0.0)
                            {
                                afterSelf = 0.0;
                            }
                            newOutput = afterSelf;
                        }
                        else
                        {
                            // Insufficient for self → unpowered, nothing downstream
                            // Matches CONSUMER logic: input must cover consumption.
                            newPowered = false;
                            newOutput = 0.0;
                        }
                    }
                    else
                    {
                        // Zero self-consumption (Splitter pattern) → pass everything
                        // v1.1.0: Only powered if actually receiving input.
                        if (effectiveInput > LFPG_PROPAGATION_EPSILON)
                        {
                            newPowered = true;
                        }
                        newOutput = effectiveInput;
                    }

                    // v0.7.33 (Fix #22): Cap output to max throughput capacity.
                    // Without this, passthrough relayed infinite power.
                    if (node.m_MaxOutput > LFPG_PROPAGATION_EPSILON && newOutput > node.m_MaxOutput)
                    {
                        newOutput = node.m_MaxOutput;
                    }
                }
                // [DIAG PT-CHAIN] Punto 2: PASSTHROUGH evaluation result
                if (LFPG_DIAG_PT_CHAIN)
                {
                    string ptLog2 = "[PT-CHAIN] PDQ PASSTHROUGH: ";
                    ptLog2 = ptLog2 + nodeId;
                    ptLog2 = ptLog2 + " mask=" + dirtyMask.ToString();
                    ptLog2 = ptLog2 + " inSum=" + inputSum.ToString();
                    ptLog2 = ptLog2 + " selfCons=" + node.m_Consumption.ToString();
                    ptLog2 = ptLog2 + " newOut=" + newOutput.ToString();
                    ptLog2 = ptLog2 + " powered=" + newPowered.ToString();
                    ptLog2 = ptLog2 + " lastStable=" + node.m_LastStableOutput.ToString();
                    ptLog2 = ptLog2 + " maxOut=" + node.m_MaxOutput.ToString();
                    ptLog2 = ptLog2 + " epoch=" + m_CurrentEpoch.ToString();
                    ptLog2 = ptLog2 + " requeue=" + node.m_RequeueCount.ToString();
                    LFPG_Util.Info(ptLog2);
                }
            }
            else if (node.m_DeviceType == LFPG_DeviceType.CONSUMER || node.m_DeviceType == LFPG_DeviceType.CAMERA)
            {
                if (node.m_Consumption > LFPG_PROPAGATION_EPSILON)
                {
                    if (inputSum + LFPG_PROPAGATION_EPSILON >= node.m_Consumption)
                    {
                        newPowered = true;
                    }
                }
                else
                {
                    if (inputSum > LFPG_PROPAGATION_EPSILON)
                    {
                        newPowered = true;
                    }
                }
                newOutput = 0.0;
            }
            else
            {
                if (inputSum > LFPG_PROPAGATION_EPSILON)
                    newPowered = true;
            }

            node.m_Powered = newPowered;

            // --- Step 2b (v1.0): Binary power allocation + demand signaling ---
            // AllocateOutput always runs for SOURCE/PASSTHROUGH (even with newOutput=0)
            // to compute totalDemand for upstream demand signaling.
            if (node.m_DeviceType == LFPG_DeviceType.SOURCE || node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
            {
                // v2.1: Pre-gate check — determine if gate blocks downstream.
                // Must run BEFORE AllocateOutput so we pass availableOutput=0
                // for closed gates. This prevents the allocate→zero→requeue
                // ping-pong: AllocateOutput with 0 produces overload→all-edges-0,
                // matching steady-state for closed gates → no allocDelta →
                // no m_AllocChanged → no wasted requeue cycles per epoch.
                // Non-gated devices (Splitter, Combiner, CeilingLight, Monitor):
                // m_IsGated=false → gateIsClosed stays false → zero regression.
                bool gateIsClosed = false;
                bool gateEntityResolved = false;
                if (node.m_IsGated)
                {
                    EntityAI gateEnt = LFPG_DeviceRegistry.Get().FindById(nodeId);
                    if (gateEnt)
                    {
                        gateEntityResolved = true;
                        bool gateOpen = LFPG_DeviceAPI.IsGateOpen(gateEnt);
                        if (!gateOpen)
                        {
                            gateIsClosed = true;
                        }
                    }
                    else
                    {
                        // Entity unresolvable (streamed out / registry stale).
                        // Preserve last known gate state to avoid phantom
                        // transition (closed→open surge or open→closed blackout).
                        gateIsClosed = node.m_GateClosed;
                    }
                }

                float allocAvail = newOutput;
                if (gateIsClosed)
                {
                    allocAvail = 0.0;
                }

                float downstreamDemand = AllocateOutput(nodeId, allocAvail);

                // Off source/passthrough: clear overload state.
                // "Off" is not "overloaded" — cables should show IDLE, not CRITICAL.
                if (newOutput < LFPG_PROPAGATION_EPSILON)
                {
                    node.m_Overloaded = false;
                    node.m_LoadRatio = 0.0;
                }

                // v2.1: Closed gate is not "overloaded". AllocateOutput with 0
                // as available marks the node overloaded (0 < demand), but a
                // closed gate is blocking by design, not overloaded. Clear the
                // stale overload state so cables show IDLE, not CRITICAL.
                if (gateIsClosed)
                {
                    node.m_Overloaded = false;
                    node.m_LoadRatio = 0.0;
                }

                // PASSTHROUGH: always report real demand (self + downstream)
                // via m_LastStableOutput so upstream sources allocate correctly.
                // This replaces Step 2c demand probe — demand is always signaled,
                // even when unpowered or overloaded, preventing oscillation.
                if (node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
                {
                    // v2.0: Read downstream soft demand from AllocateOutput cache.
                    // AllocateOutput already iterated all outgoing edges and computed
                    // totalSoftDemand → stored in m_LastAllocSoftDemand.
                    // Eliminates redundant O(K) edge iteration + map lookups.
                    float downstreamSoft = m_LastAllocSoftDemand;

                    // v2.0: Demand signal includes:
                    //   hard = downstreamDemand - downstreamSoft + selfConsumption - virtualGen
                    //   soft = downstreamSoft + node.m_SoftDemand (charge want)
                    //   virtualGen subtracted (storage covers part of demand)
                    float totalSoft = downstreamSoft + node.m_SoftDemand;
                    float demandSignal = downstreamDemand + node.m_Consumption + node.m_SoftDemand - node.m_VirtualGeneration;
                    if (demandSignal < 0.0)
                    {
                        demandSignal = 0.0;
                    }

                    if (demandSignal > LFPG_PROPAGATION_EPSILON)
                    {
                        newOutput = demandSignal;

                        // v2.0 (Fix C1): MaxOutput cap with hard-priority.
                        // When throughput bottleneck forces a cap, reduce soft FIRST
                        // so hard consumers retain full demand signal upstream.
                        // Without this, uniform scaling starves hard demand when
                        // soft demand is large relative to total.
                        if (node.m_MaxOutput > LFPG_PROPAGATION_EPSILON && newOutput > node.m_MaxOutput)
                        {
                            float excess = newOutput - node.m_MaxOutput;
                            newOutput = node.m_MaxOutput;
                            // Reduce soft by the excess amount (cap soft first).
                            totalSoft = totalSoft - excess;
                            if (totalSoft < 0.0)
                            {
                                totalSoft = 0.0;
                            }
                        }

                        // Compute soft ratio for upstream propagation.
                        // Non-battery PASSTHROUGH: totalSoft=0 → ratio=0 (no regression).
                        float ratioVal = totalSoft / newOutput;
                        if (ratioVal < 0.0)
                        {
                            ratioVal = 0.0;
                        }
                        if (ratioVal > 1.0)
                        {
                            ratioVal = 1.0;
                        }
                        node.m_SoftDemandRatio = ratioVal;
                    }
                    else
                    {
                        node.m_SoftDemandRatio = 0.0;

                        if (node.m_Consumption > LFPG_PROPAGATION_EPSILON)
                        {
                            newOutput = node.m_Consumption;
                        }
                        else
                        {
                            // B1 fix: No downstream demand, no self-consumption.
                            // Without this, newOutput retains effectiveInput from Step 2a,
                            // causing phantom load on upstream source via stale
                            // m_LastStableOutput. Zero it explicitly.
                            newOutput = 0.0;
                        }
                    }

                    // v2.1: Post-gate — override demand signal for closed gates
                    // and track gate state transitions for re-evaluation.
                    // This runs AFTER the demand signal section so it can override
                    // newOutput cleanly. The pre-gate check already prevented
                    // AllocateOutput from producing non-zero allocations.
                    if (node.m_IsGated)
                    {
                        bool prevGateClosed = node.m_GateClosed;

                        // Only update m_GateClosed when entity was resolved.
                        // If stale (streamed out), preserve last known state —
                        // gateIsClosed already copied from node.m_GateClosed in
                        // pre-gate, so behavior is consistent but no false transition.
                        if (gateEntityResolved)
                        {
                            if (gateIsClosed)
                            {
                                node.m_GateClosed = true;
                            }
                            else
                            {
                                node.m_GateClosed = false;
                            }
                        }

                        if (gateIsClosed)
                        {
                            // Override demand signal: closed gate cannot serve
                            // downstream, so only report self needs.
                            // selfConsumption: 0 for PushButton/LogicGates, 5 for
                            //   Counter/Laser (keeps device itself running).
                            // selfSoftDemand: 0 for most, >0 for Battery
                            //   (continues charging from surplus with output off).
                            float gatedDemand = node.m_Consumption + node.m_SoftDemand;
                            newOutput = gatedDemand;
                            if (gatedDemand > LFPG_PROPAGATION_EPSILON)
                            {
                                node.m_SoftDemandRatio = node.m_SoftDemand / gatedDemand;
                            }
                            else
                            {
                                node.m_SoftDemandRatio = 0.0;
                            }
                        }

                        // Force upstream re-evaluation on gate state transition.
                        // Without this, opening a gate after steady-state (demand=0)
                        // produces no outputDelta, so upstream never re-allocates
                        // and the chain stays dead.
                        // Only fire on real transitions (entity resolved), not
                        // stale reads where m_GateClosed is preserved unchanged.
                        if (gateEntityResolved && prevGateClosed != node.m_GateClosed)
                        {
                            m_AllocChanged = true;
                        }
                    }
                }
            }

            // --- Step 3: If output changed, mark downstream dirty ---
            float outputDelta = newOutput - node.m_LastStableOutput;
            if (outputDelta < 0.0)
                outputDelta = -outputDelta;

            // v0.7.38 (BugFix B1): Force downstream re-evaluation when topology
            // changed on a source/passthrough, even if total output is unchanged.
            // Wire replace creates fresh edges with m_AllocatedPower=0.
            // If a consumer processes BEFORE the source in the same epoch,
            // it reads stale allocation via equal-split fallback (e.g. 50/2=25
            // for a 50W consumer → incorrectly powers off).
            // AllocateOutput on the source DOES set correct per-edge
            // allocations, but outputDelta=0 means Step 3 never re-queues
            // consumers to read them.
            // Fix: always re-queue downstream when DIRTY_TOPOLOGY on a producer.
            // Additionally, reset m_LastEpoch on consumers that already processed
            // this epoch so they can re-evaluate in the SAME epoch with correct
            // allocations. Both SetPowered calls (stale→correct) land in the same
            // frame, so DayZ SyncVar batching sends only the final value to clients
            // — zero visible flicker. Safe: m_RequeueCount prevents infinite loops.
            bool forceDownstream = false;
            if ((dirtyMask & LFPG_DIRTY_TOPOLOGY) != 0)
            {
                if (node.m_DeviceType == LFPG_DeviceType.SOURCE || node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
                {
                    forceDownstream = true;
                }
            }

            // v0.7.46: m_AllocChanged — per-edge allocation changed even if
            // total output (outputDelta) is unchanged. Example: SOURCE always
            // outputs 50, but splits 10→20 for a splitter after demand increase.
            // Without this, downstream never re-reads the new allocation.
            if (outputDelta > LFPG_PROPAGATION_EPSILON || forceDownstream || m_AllocChanged)
            {
                node.m_OutputPower = newOutput;
                node.m_LastStableOutput = newOutput;

                ref array<ref LFPG_ElecEdge> outEdges;
                if (m_Outgoing.Find(nodeId, outEdges) && outEdges)
                {
                    int oi;
                    for (oi = 0; oi < outEdges.Count(); oi = oi + 1)
                    {
                        ref LFPG_ElecEdge outEdge = outEdges[oi];
                        if (outEdge && outEdge.m_TargetNodeId != "")
                        {
                            m_EdgesVisitedThisEpoch = m_EdgesVisitedThisEpoch + 1;

                            ref LFPG_ElecNode tgtNode;
                            if (m_Nodes.Find(outEdge.m_TargetNodeId, tgtNode) && tgtNode)
                            {
                                tgtNode.m_RequeueCount = tgtNode.m_RequeueCount + 1;

                                // B1: Allow same-epoch reprocessing for consumers
                                // that already ran this epoch with stale allocations.
                                // Only reset if they were actually processed this epoch
                                // (m_LastEpoch == current); otherwise they haven't run
                                // yet and don't need the reset.
                                // v0.7.46: Also reset when m_AllocChanged — per-edge
                                // allocations changed but outputDelta=0, downstream
                                // already processed with old allocations this epoch.
                                if ((forceDownstream || m_AllocChanged) && tgtNode.m_LastEpoch == m_CurrentEpoch)
                                {
                                    int prevEpoch = m_CurrentEpoch - 1;
                                    tgtNode.m_LastEpoch = prevEpoch;
                                }
                            }
                            MarkNodeDirty(outEdge.m_TargetNodeId, LFPG_DIRTY_INPUT);
                        }
                    }
                }

                // v0.7.40: Upstream demand propagation for PASSTHROUGH nodes.
                // When a PASSTHROUGH output changes, upstream sources must
                // re-evaluate because they use m_LastStableOutput as demand.
                // Without this, the source processes first during warmup with
                // cold-start fallback demand (inflated), the passthrough caps
                // to real demand, but the source never re-evaluates — its
                // loadRatio stays permanently inflated, causing false
                // WARNING/CRITICAL cable colors on the upstream wire.
                // Mirrors the B1 pattern: reset m_LastEpoch so upstream can
                // re-process in the SAME epoch with corrected demand values.
                // Safe: bounded by m_RequeueCount (LFPG_MAX_REQUEUE_PER_EPOCH).
                // Convergence: SOURCE output is fixed (m_MaxOutput), so re-processing
                // only updates loadRatio/masks — no cascading downstream changes.
                if (node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
                {
                    ref array<ref LFPG_ElecEdge> upEdges;
                    if (m_Incoming.Find(nodeId, upEdges) && upEdges)
                    {
                        int ui;
                        for (ui = 0; ui < upEdges.Count(); ui = ui + 1)
                        {
                            ref LFPG_ElecEdge upEdge = upEdges[ui];
                            if (upEdge && upEdge.m_SourceNodeId != "")
                            {
                                ref LFPG_ElecNode upNode;
                                if (m_Nodes.Find(upEdge.m_SourceNodeId, upNode) && upNode)
                                {
                                    upNode.m_RequeueCount = upNode.m_RequeueCount + 1;
                                    // Allow same-epoch reprocessing (B1 pattern).
                                    // Without this reset, epoch-skip (line ~1647)
                                    // consumes the node without clearing m_InQueue,
                                    // leaving a zombie that blocks future re-enqueue.
                                    if (upNode.m_LastEpoch == m_CurrentEpoch)
                                    {
                                        int prevUp = m_CurrentEpoch - 1;
                                        upNode.m_LastEpoch = prevUp;
                                    }
                                }
                                MarkNodeDirty(upEdge.m_SourceNodeId, LFPG_DIRTY_INPUT);
                            }
                        }
                    }
                }
            }
            else
            {
                node.m_OutputPower = newOutput;
            }

            // --- Step 4: Mark as processed ---
            node.m_LastEpoch = m_CurrentEpoch;
            node.m_Dirty = false;
            node.m_InQueue = false;
            node.m_DirtyMask = 0;

            // --- Step 5: Sync state to entity ---
            SyncNodeToEntity(nodeId, node);
        }

        // v0.8.3: Re-enqueue nodes deferred by requeue limit.
        // Dirty state was preserved in Edit 4. These nodes will process in
        // the next epoch with reset requeue counts (ResetRequeueCounts runs
        // at epoch start). O(K) where K = deferred nodes (typically 1-3).
        // Convergence: each deferred epoch makes at least one node's worth
        // of progress, so topologies with N layers converge in ≤N extra epochs.
        if (m_DeferredRequeue.Count() > 0)
        {
            int dri;
            for (dri = 0; dri < m_DeferredRequeue.Count(); dri = dri + 1)
            {
                string drNodeId = m_DeferredRequeue[dri];
                ref LFPG_ElecNode drNode;
                if (m_Nodes.Find(drNodeId, drNode) && drNode)
                {
                    if (drNode.m_Dirty && !drNode.m_InQueue)
                    {
                        drNode.m_InQueue = true;
                        m_DirtyQueue.Insert(drNodeId);
                        bool bDrEnq = true;
                        m_EnqueuedThisEpoch.Set(drNodeId, bDrEnq);
                    }
                }
            }
            m_DeferredRequeue.Clear();
        }

        // H4: Compact the queue only when head passes threshold
        int remaining = m_DirtyQueue.Count() - m_DirtyQueueHead;
        if (remaining <= 0)
        {
            m_DirtyQueue.Clear();
            m_DirtyQueueHead = 0;

            // v0.7.32 (Bloque C): Validate consumers in steady-state.
            // Only runs when no pending propagation (queue fully drained).
            ValidateConsumerStates();
        }
        else if (m_DirtyQueueHead >= LFPG_DIRTY_QUEUE_COMPACT_THRESHOLD)
        {
            ref array<string> compacted = new array<string>;
            int ci;
            for (ci = m_DirtyQueueHead; ci < m_DirtyQueue.Count(); ci = ci + 1)
            {
                compacted.Insert(m_DirtyQueue[ci]);
            }
            m_DirtyQueue.Clear();
            int cc;
            for (cc = 0; cc < compacted.Count(); cc = cc + 1)
            {
                m_DirtyQueue.Insert(compacted[cc]);
            }
            m_DirtyQueueHead = 0;
            remaining = m_DirtyQueue.Count();
        }

        int elapsed = GetGame().GetTime() - startMs;
        m_LastProcessMs = elapsed;

        if (processed > 0)
        {
            string dbgProc = "[ElecGraph] ProcessDirtyQueue: processed=" + processed.ToString() + " edges=" + m_EdgesVisitedThisEpoch.ToString() + " remaining=" + remaining.ToString() + " epoch=" + m_CurrentEpoch.ToString() + " ms=" + elapsed.ToString();
            LFPG_Util.Debug(dbgProc);
        }

        return remaining;
        #else
        return 0;
        #endif
    }

    // ===========================
    // Sprint 4.3: Entity sync
    // ===========================

    protected void SyncNodeToEntity(string nodeId, LFPG_ElecNode node)
    {
        #ifdef SERVER
        if (!node)
            return;

        EntityAI entObj = LFPG_DeviceRegistry.Get().FindById(nodeId);
        if (!entObj)
        {
            entObj = LFPG_DeviceAPI.ResolveVanillaDevice(nodeId);
        }

        // v0.7.43 (Fix 3): NetworkID fallback when registry ref is stale.
        // DeviceRegistry may lose valid refs when DayZ recreates the C++
        // backing of an entity (streaming, initialization race).
        // NetworkID (engine identity) survives this. If re-resolved,
        // auto-register to prevent future misses.
        if (!entObj)
        {
            int cachedNetLow = 0;
            int cachedNetHigh = 0;
            bool hasNetLow = m_NodeNetLow.Find(nodeId, cachedNetLow);
            bool hasNetHigh = m_NodeNetHigh.Find(nodeId, cachedNetHigh);
            if (hasNetLow && hasNetHigh)
            {
                if (cachedNetLow != 0 || cachedNetHigh != 0)
                {
                    Object rawObj = GetGame().GetObjectByNetworkId(cachedNetLow, cachedNetHigh);
                    entObj = EntityAI.Cast(rawObj);
                    if (entObj)
                    {
                        LFPG_DeviceRegistry.Get().Register(entObj, nodeId);
                    }
                }
            }
        }

        if (!entObj)
        {
            // [DIAG PT-CHAIN] Punto 5a: Entity resolution failed
            if (LFPG_DIAG_PT_CHAIN && node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
            {
                string ptLog5a = "[PT-CHAIN] SyncToEntity FAILED: entity NULL for ";
                ptLog5a = ptLog5a + nodeId;
                ptLog5a = ptLog5a + " type=PASSTHROUGH";
                ptLog5a = ptLog5a + " powered=" + node.m_Powered.ToString();
                ptLog5a = ptLog5a + " input=" + node.m_InputPower.ToString();
                ptLog5a = ptLog5a + " output=" + node.m_OutputPower.ToString();
                LFPG_Util.Warn(ptLog5a);
            }
            return;
        }

        if (node.m_DeviceType == LFPG_DeviceType.SOURCE)
        {
            // v1.0: Sync load ratio + overloaded bool to source entity
            float loadDelta = node.m_LoadRatio - node.m_LastSyncedLoadRatio;
            if (loadDelta < 0.0)
            {
                loadDelta = -loadDelta;
            }
            if (loadDelta > 0.01)
            {
                LFPG_DeviceAPI.SetLoadRatio(entObj, node.m_LoadRatio);

                if (loadDelta > LFPG_LOAD_TELEM_DELTA)
                {
                    string loadState = "NORMAL";
                    if (node.m_LoadRatio >= LFPG_LOAD_CRITICAL_THRESHOLD)
                    {
                        loadState = "OVERLOADED";
                    }
                    string telemMsg = "[LoadTelem] " + nodeId;
                    telemMsg = telemMsg + " load=" + node.m_LoadRatio.ToString();
                    telemMsg = telemMsg + " prev=" + node.m_LastSyncedLoadRatio.ToString();
                    telemMsg = telemMsg + " cap=" + node.m_MaxOutput.ToString();
                    telemMsg = telemMsg + " state=" + loadState;
                    LFPG_Util.Info(telemMsg);
                }

                node.m_LastSyncedLoadRatio = node.m_LoadRatio;
            }
            LFPG_DeviceAPI.SetOverloaded(entObj, node.m_Overloaded);
            return;
        }

        // v1.0: PASSTHROUGH nodes sync powered + overloaded for cable visuals.
        if (node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
        {
            // [DIAG PT-CHAIN] Punto 5b: PASSTHROUGH entity sync
            if (LFPG_DIAG_PT_CHAIN)
            {
                string ptLog5b = "[PT-CHAIN] SyncToEntity: ";
                ptLog5b = ptLog5b + nodeId;
                ptLog5b = ptLog5b + " powered=" + node.m_Powered.ToString();
                ptLog5b = ptLog5b + " input=" + node.m_InputPower.ToString();
                ptLog5b = ptLog5b + " output=" + node.m_OutputPower.ToString();
                ptLog5b = ptLog5b + " entity=" + entObj.GetType();
                LFPG_Util.Info(ptLog5b);
            }
            LFPG_DeviceAPI.SetPowered(entObj, node.m_Powered);
            LFPG_DeviceAPI.SetOverloaded(entObj, node.m_Overloaded);
            return;
        }

        LFPG_DeviceAPI.SetPowered(entObj, node.m_Powered);
        #endif
    }

    // ===========================
    // v0.7.32 (Bloque C): Consumer Zombie Validation
    // ===========================

    // Periodic sweep to detect consumers claiming m_Powered=true without
    // sufficient incoming power. This catches edge cases where the graph
    // changes without propagating to a downstream consumer (timing gaps,
    // entity deletion races, or hypothetical propagation bugs).
    //
    // Design:
    //   - Runs when queue is empty (either post-drain or idle).
    //   - Throttled by LFPG_CONSUMER_VALIDATE_TICK_INTERVAL ticks.
    //     Uses m_ValidateTickCount (increments on every ProcessDirtyQueue call,
    //     including early-returns) so validation fires even during idle periods.
    //   - Budgeted: checks LFPG_VALIDATE_BATCH_SIZE (32) nodes per call.
    //   - Round-robin via m_ValidateNodeIdx — full sweep of N nodes takes
    //     ceil(N/32) invocations × interval each = predictable spread.
    //   - When a zombie is found: sets m_Powered=false, syncs to entity,
    //     and logs for telemetry. Does NOT re-enqueue (avoids cascading).
    //
    // Power source: reads inEdge.m_AllocatedPower directly (NOT via
    //   GetEdgeAllocatedPower). Intentional: the helper's equal-split
    //   fallback can mask brownout edges (m_AllocatedPower=0 but fallback
    //   returns non-zero). Direct read gives ground truth in steady-state.
    //
    // Cost: O(batch * avg_incoming_edges). At 32 nodes/tick with avg 2
    //       incoming edges = ~64 edge checks per tick. Negligible.
    //
    // Returns number of zombies fixed in this batch.
    protected int ValidateConsumerStates()
    {
        #ifdef SERVER
        int nodeTotal = m_Nodes.Count();
        if (nodeTotal <= 0)
            return 0;

        // Tick interval gate (advances even when queue is idle)
        int tickDelta = m_ValidateTickCount - m_LastValidateTick;
        if (tickDelta < LFPG_CONSUMER_VALIDATE_TICK_INTERVAL)
            return 0;

        // Clamp round-robin index if graph shrank
        if (m_ValidateNodeIdx >= nodeTotal)
            m_ValidateNodeIdx = 0;

        int batchSize = LFPG_VALIDATE_BATCH_SIZE;
        if (batchSize > nodeTotal)
            batchSize = nodeTotal;

        int checked = 0;
        int fixed = 0;

        while (checked < batchSize)
        {
            // Bounds check before access
            if (m_ValidateNodeIdx >= nodeTotal)
                m_ValidateNodeIdx = 0;

            string nodeId = m_Nodes.GetKey(m_ValidateNodeIdx);
            ref LFPG_ElecNode node = m_Nodes.GetElement(m_ValidateNodeIdx);

            m_ValidateNodeIdx = m_ValidateNodeIdx + 1;
            checked = checked + 1;

            if (!node)
                continue;

            // Only validate non-source nodes (CONSUMER, CAMERA, PASSTHROUGH)
            // B5 fix: Previously excluded PASSTHROUGH — a zombie passthrough
            // (m_Powered=true without sufficient input) was never autocorrected.
            if (node.m_DeviceType == LFPG_DeviceType.SOURCE || node.m_DeviceType == LFPG_DeviceType.UNKNOWN)
                continue;

            // Skip nodes currently in the dirty queue — they have pending updates
            if (node.m_InQueue || node.m_Dirty)
                continue;

            // Sum actual incoming power from enabled edges.
            // Reads m_AllocatedPower directly — see method doc above for rationale.
            float incomingPower = 0.0;
            bool hasAnyIncoming = false;

            ref array<ref LFPG_ElecEdge> inEdges;
            if (m_Incoming.Find(nodeId, inEdges) && inEdges)
            {
                int ii;
                for (ii = 0; ii < inEdges.Count(); ii = ii + 1)
                {
                    ref LFPG_ElecEdge inEdge = inEdges[ii];
                    if (!inEdge)
                        continue;

                    if ((inEdge.m_Flags & LFPG_EDGE_ENABLED) == 0)
                        continue;

                    hasAnyIncoming = true;
                    float edgePower = inEdge.m_AllocatedPower;
                    if (edgePower < 0.0)
                    {
                        edgePower = 0.0;
                    }
                    incomingPower = incomingPower + edgePower;
                }
            }

            // v0.7.32: Final NaN/negative guard on accumulated sum.
            // Matches ProcessDirtyQueue pattern (v0.7.27 Audit 5).
            if (incomingPower < 0.0)
            {
                incomingPower = 0.0;
            }

            // Determine if this consumer should actually be powered
            bool shouldBePowered = false;

            if (hasAnyIncoming)
            {
                if (node.m_Consumption > LFPG_PROPAGATION_EPSILON)
                {
                    // Declared consumption: needs enough power to meet demand
                    if (incomingPower + LFPG_PROPAGATION_EPSILON >= node.m_Consumption)
                    {
                        shouldBePowered = true;
                    }
                }
                else
                {
                    // Legacy consumer (consumption=0): any power suffices
                    if (incomingPower > LFPG_PROPAGATION_EPSILON)
                    {
                        shouldBePowered = true;
                    }
                }
            }

            // v0.7.38 (RC-09 safety net): Bidirectional zombie detection.
            // Original: only caught powered=true when shouldBePowered=false (zombie).
            // Added: also catch powered=false when shouldBePowered=true (dark consumer).
            // Dark consumers arise from race conditions like B1 (topology change
            // with stale per-edge allocations during same-epoch processing).
            if (node.m_Powered && !shouldBePowered)
            {
                // Classic zombie: powered but shouldn't be
                node.m_Powered = false;
                node.m_InputPower = incomingPower;

                SyncNodeToEntity(nodeId, node);

                fixed = fixed + 1;
                m_ValidateFixCount = m_ValidateFixCount + 1;

                string zombMsg = "[ElecGraph] Zombie node fixed: " + nodeId;
                zombMsg = zombMsg + " type=" + node.m_DeviceType.ToString();
                zombMsg = zombMsg + " inPower=" + incomingPower.ToString();
                zombMsg = zombMsg + " consumption=" + node.m_Consumption.ToString();
                zombMsg = zombMsg + " totalFixes=" + m_ValidateFixCount.ToString();
                LFPG_Util.Warn(zombMsg);
            }
            else if (!node.m_Powered && shouldBePowered)
            {
                // Inverse zombie (dark consumer): should be powered but isn't.
                // Caused by topology race conditions (B1) or stale allocation reads.
                node.m_Powered = true;
                node.m_InputPower = incomingPower;

                SyncNodeToEntity(nodeId, node);

                fixed = fixed + 1;
                m_ValidateFixCount = m_ValidateFixCount + 1;

                string darkMsg = "[ElecGraph] Dark node fixed: " + nodeId;
                darkMsg = darkMsg + " type=" + node.m_DeviceType.ToString();
                darkMsg = darkMsg + " inPower=" + incomingPower.ToString();
                darkMsg = darkMsg + " consumption=" + node.m_Consumption.ToString();
                darkMsg = darkMsg + " totalFixes=" + m_ValidateFixCount.ToString();
                LFPG_Util.Warn(darkMsg);
            }
        }

        // Wrap index for next invocation
        if (m_ValidateNodeIdx >= nodeTotal)
            m_ValidateNodeIdx = 0;

        // Update tick gate — next batch runs after interval elapses
        m_LastValidateTick = m_ValidateTickCount;

        if (fixed > 0)
        {
            string valMsg = "[ElecGraph] ValidateConsumers: ";
            valMsg = valMsg + fixed.ToString() + " zombies fixed this batch, tick=" + m_ValidateTickCount.ToString();
            LFPG_Util.Info(valMsg);
        }

        return fixed;
        #else
        return 0;
        #endif
    }

    // ===========================
    // Sprint 4.2+4.3: Warmup helpers
    // ===========================

    void PopulateAllNodeElecStates()
    {
        #ifdef SERVER
        int ni;
        for (ni = 0; ni < m_Nodes.Count(); ni = ni + 1)
        {
            string nid = m_Nodes.GetKey(ni);
            ref LFPG_ElecNode node = m_Nodes.GetElement(ni);
            if (!node)
                continue;

            EntityAI obj = LFPG_DeviceRegistry.Get().FindById(nid);
            if (!obj)
            {
                obj = LFPG_DeviceAPI.ResolveVanillaDevice(nid);
            }
            if (!obj)
                continue;

            if (node.m_DeviceType == LFPG_DeviceType.SOURCE)
            {
                node.m_MaxOutput = LFPG_DeviceAPI.GetCapacity(obj);
                bool sourceOn = false;
                if (LFPG_DeviceAPI.IsSource(obj))
                {
                    sourceOn = LFPG_DeviceAPI.GetSourceOn(obj);
                }
                else
                {
                    ComponentEnergyManager em = obj.GetCompEM();
                    if (em)
                        sourceOn = em.IsWorking();
                }
                node.m_Powered = sourceOn;
            }
            else if (node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
            {
                // v0.7.33 (Fix #22): Read max throughput capacity from device.
                // Previously hardcoded to 0.0 (infinite passthrough).
                // Now uses LFPG_GetCapacity if available, else default constant.
                node.m_MaxOutput = LFPG_DeviceAPI.GetCapacity(obj);
                if (node.m_MaxOutput < LFPG_PROPAGATION_EPSILON)
                {
                    node.m_MaxOutput = LFPG_DEFAULT_PASSTHROUGH_CAPACITY;
                }
                // v0.7.47: PASSTHROUGH self-consumption (CeilingLight pattern).
                // Splitter returns 0.0 explicitly → no regression.
                node.m_Consumption = LFPG_DeviceAPI.GetConsumption(obj);
                // P1: Cache gate capability for bulk warmup path.
                node.m_IsGated = LFPG_DeviceAPI.IsGateCapable(obj);
                // v2.0: Battery fields (m_VirtualGeneration, m_SoftDemand) are
                // set by NetworkManager battery timer on first tick (~5s).
                // Warmup gap is acceptable (same pattern as solar panels).
                // Sprint 2 timer calls RefreshBatteryNodeState() which reads
                // stored energy from entity and computes virtualGen + softDemand.
                // For non-battery PASSTHROUGH, fields remain 0.0 (default).
            }
            else if (node.m_DeviceType == LFPG_DeviceType.CONSUMER || node.m_DeviceType == LFPG_DeviceType.CAMERA)
            {
                node.m_Consumption = LFPG_DeviceAPI.GetConsumption(obj);
            }
        }

        string infoPopulate = "[ElecGraph] PopulateAllNodeElecStates: " + m_Nodes.Count().ToString() + " nodes";
        LFPG_Util.Info(infoPopulate);
        #endif
    }

    void RefreshSourceState(string nodeId)
    {
        #ifdef SERVER
        ref LFPG_ElecNode node;
        if (!m_Nodes.Find(nodeId, node) || !node)
            return;

        if (node.m_DeviceType != LFPG_DeviceType.SOURCE)
            return;

        EntityAI obj = LFPG_DeviceRegistry.Get().FindById(nodeId);
        if (!obj)
        {
            obj = LFPG_DeviceAPI.ResolveVanillaDevice(nodeId);
        }
        if (!obj)
            return;

        bool sourceOn = false;
        if (LFPG_DeviceAPI.IsSource(obj))
        {
            sourceOn = LFPG_DeviceAPI.GetSourceOn(obj);
        }
        else
        {
            ComponentEnergyManager em = obj.GetCompEM();
            if (em)
                sourceOn = em.IsWorking();
        }

        node.m_Powered = sourceOn;
        node.m_MaxOutput = LFPG_DeviceAPI.GetCapacity(obj);

        MarkNodeDirty(nodeId, LFPG_DIRTY_INTERNAL);
        #endif
    }

    // ===========================
    // Sprint 4.2+4.3: Internal helpers
    // ===========================

    protected int CountEnabledOutgoing(string nodeId)
    {
        ref array<ref LFPG_ElecEdge> outEdges;
        if (!m_Outgoing.Find(nodeId, outEdges) || !outEdges)
            return 0;

        int count = 0;
        int oi;
        for (oi = 0; oi < outEdges.Count(); oi = oi + 1)
        {
            ref LFPG_ElecEdge edge = outEdges[oi];
            if (edge && (edge.m_Flags & LFPG_EDGE_ENABLED) != 0)
            {
                count = count + 1;
            }
        }
        return count;
    }

    // v0.8.3: Count powered incoming edges for multi-source demand sharing.
    // Used in AllocateOutput to proportionally divide
    // PASSTHROUGH demand among active suppliers for LoadRatio calculation.
    // Returns 0 if node has no incoming edges or none are powered.
    // Cost: O(K) where K = incoming edge count (typically 1-2 for Combiner).
    protected int CountPoweredIncoming(string nodeId)
    {
        ref array<ref LFPG_ElecEdge> inEdges;
        if (!m_Incoming.Find(nodeId, inEdges) || !inEdges)
            return 0;

        int count = 0;
        int cpi;
        for (cpi = 0; cpi < inEdges.Count(); cpi = cpi + 1)
        {
            ref LFPG_ElecEdge cpEdge = inEdges[cpi];
            if (!cpEdge)
                continue;
            if ((cpEdge.m_Flags & LFPG_EDGE_ENABLED) == 0)
                continue;

            ref LFPG_ElecNode cpSrcNode;
            if (m_Nodes.Find(cpEdge.m_SourceNodeId, cpSrcNode) && cpSrcNode)
            {
                if (cpSrcNode.m_OutputPower > LFPG_PROPAGATION_EPSILON)
                {
                    count = count + 1;
                }
            }
        }
        return count;
    }

    // v1.0: Binary power allocation (all-off policy).
    // If totalDemand > availableOutput → ALL edges get 0 (overloaded).
    // If totalDemand <= availableOutput → each edge gets its full demand.
    // Returns totalDemand (always — even when overloaded, for upstream demand signaling).
    protected float AllocateOutput(string nodeId, float availableOutput)
    {
        #ifdef SERVER
        ref array<ref LFPG_ElecEdge> outEdges;
        if (!m_Outgoing.Find(nodeId, outEdges) || !outEdges)
            return 0.0;

        int edgeCount = outEdges.Count();
        if (edgeCount <= 0)
            return 0.0;

        // Pass 1: Collect demands and compute total.
        // Store per-edge demand in edge.m_Demand for pass 2.
        // v2.0: Also track totalSoftDemand via target node m_SoftDemandRatio.
        // For non-battery networks, all ratios are 0.0 → totalSoftDemand stays 0.0.
        float totalDemand = 0.0;
        float totalSoftDemand = 0.0;
        float edgeDemand = 0.0;
        float edgeSoftPortion = 0.0;
        int ei;
        for (ei = 0; ei < edgeCount; ei = ei + 1)
        {
            ref LFPG_ElecEdge edge = outEdges[ei];
            if (!edge)
                continue;
            if ((edge.m_Flags & LFPG_EDGE_ENABLED) == 0)
                continue;

            edgeDemand = 0.0;
            edgeSoftPortion = 0.0;
            ref LFPG_ElecNode targetNode;
            if (m_Nodes.Find(edge.m_TargetNodeId, targetNode) && targetNode)
            {
                if (targetNode.m_DeviceType == LFPG_DeviceType.CONSUMER || targetNode.m_DeviceType == LFPG_DeviceType.CAMERA)
                {
                    edgeDemand = targetNode.m_Consumption;
                }
                else if (targetNode.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
                {
                    edgeDemand = targetNode.m_LastStableOutput;
                    if (edgeDemand < LFPG_PROPAGATION_EPSILON)
                    {
                        // Cold-start fallback: bootstrap demand estimate.
                        // Only use m_MaxOutput if the passthrough has downstream
                        // consumers to serve. A passthrough with no outgoing edges
                        // demands only its self-consumption (0 for Splitter/Combiner,
                        // N for CeilingLight). Without this check, an empty Combiner
                        // (cap=500) causes permanent false overload on a 50 u/s source.
                        // B3 fix: Only count ENABLED outgoing edges.
                        // Without this, disabled edges make ptHasDown=true
                        // and the fallback uses m_MaxOutput (200) as demand
                        // instead of consumption (0), inflating upstream load.

                        // v2.1: Gated PASSTHROUGH with gate closed cannot
                        // serve downstream, so cold-start must NOT inflate
                        // demand to m_MaxOutput. Use probe demand instead:
                        // max(selfConsumption, LFPG_GATE_PROBE_DEMAND).
                        // Probe keeps a trickle flowing so the gate can
                        // re-evaluate when toggled. For non-zero consumption
                        // devices (Laser=5, Counter=5) the consumption itself
                        // is sufficient; for zero-consumption (PushButton,
                        // LogicGates) the 1.0 probe prevents a 0-demand
                        // deadlock. Non-gated PASSthrough (Splitter, Combiner,
                        // CeilingLight, Monitor) has m_GateClosed=false always
                        // → zero regression.
                        if (targetNode.m_GateClosed)
                        {
                            float probeDemand = targetNode.m_Consumption;
                            if (probeDemand < LFPG_GATE_PROBE_DEMAND)
                            {
                                probeDemand = LFPG_GATE_PROBE_DEMAND;
                            }
                            edgeDemand = probeDemand;
                        }
                        else
                        {
                            bool ptHasDown = false;
                            ref array<ref LFPG_ElecEdge> ptOutEdges;
                            if (m_Outgoing.Find(edge.m_TargetNodeId, ptOutEdges) && ptOutEdges)
                            {
                                int pti;
                                for (pti = 0; pti < ptOutEdges.Count(); pti = pti + 1)
                                {
                                    if (!ptHasDown)
                                    {
                                        ref LFPG_ElecEdge ptEdge = ptOutEdges[pti];
                                        if (ptEdge && (ptEdge.m_Flags & LFPG_EDGE_ENABLED) != 0)
                                        {
                                            ptHasDown = true;
                                        }
                                    }
                                }
                            }

                            if (ptHasDown && targetNode.m_MaxOutput > LFPG_PROPAGATION_EPSILON)
                            {
                                edgeDemand = targetNode.m_MaxOutput;
                            }
                            else
                            {
                                edgeDemand = targetNode.m_Consumption;
                            }
                        }
                    }

                    // v0.9.3: Multi-source demand split for Combiner pattern.
                    int ptPoweredIn = CountPoweredIncoming(edge.m_TargetNodeId);
                    if (ptPoweredIn > 1)
                    {
                        edgeDemand = edgeDemand / ptPoweredIn;
                    }

                    // v2.0: Track soft portion of this edge's demand.
                    // SoftDemandRatio is 0.0 for all non-battery PASSTHROUGH
                    // (Splitter, Combiner, CeilingLight, etc.) → no regression.
                    if (targetNode.m_SoftDemandRatio > LFPG_PROPAGATION_EPSILON)
                    {
                        edgeSoftPortion = edgeDemand * targetNode.m_SoftDemandRatio;
                    }
                }
            }

            edge.m_Demand = edgeDemand;
            totalDemand = totalDemand + edgeDemand;
            totalSoftDemand = totalSoftDemand + edgeSoftPortion;
        }

        // v2.0: Cache soft demand total for PDQ demand signal section.
        // Eliminates redundant outgoing edge iteration in PDQ.
        m_LastAllocSoftDemand = totalSoftDemand;

        // v2.0: Overload decision uses hard demand only.
        // Soft demand (battery charging) NEVER causes overload.
        // For non-battery networks: totalSoftDemand=0 → identical to before.
        float totalHardDemand = totalDemand - totalSoftDemand;
        if (totalHardDemand < 0.0)
        {
            totalHardDemand = 0.0;
        }
        bool overloaded = false;
        if (totalHardDemand > availableOutput + LFPG_PROPAGATION_EPSILON)
        {
            overloaded = true;
        }

        // Pass 2: Set allocations + detect changes.
        // v2.0: When soft demand exists and not overloaded, allocate only
        // the hard portion per edge. Soft surplus handled in Pass 3.
        // When totalSoftDemand=0 (99.9% of nodes), newAlloc = edge.m_Demand
        // (unchanged behavior).
        float totalAllocated = 0.0;
        int ai;
        for (ai = 0; ai < edgeCount; ai = ai + 1)
        {
            ref LFPG_ElecEdge allocEdge = outEdges[ai];
            if (!allocEdge)
                continue;
            if ((allocEdge.m_Flags & LFPG_EDGE_ENABLED) == 0)
                continue;

            float oldAlloc = allocEdge.m_AllocatedPower;
            float newAlloc = 0.0;
            if (!overloaded)
            {
                if (totalSoftDemand > LFPG_PROPAGATION_EPSILON)
                {
                    // Has soft demand in this node's edges: allocate hard portion only.
                    // Soft portion deferred to Pass 3 (surplus distribution).
                    ref LFPG_ElecNode allocTarget;
                    float allocTargetRatio = 0.0;
                    if (m_Nodes.Find(allocEdge.m_TargetNodeId, allocTarget) && allocTarget)
                    {
                        allocTargetRatio = allocTarget.m_SoftDemandRatio;
                    }
                    float edgeHard = allocEdge.m_Demand * (1.0 - allocTargetRatio);
                    if (edgeHard < 0.0)
                    {
                        edgeHard = 0.0;
                    }
                    newAlloc = edgeHard;
                }
                else
                {
                    // No soft demand anywhere → full demand (existing behavior).
                    newAlloc = allocEdge.m_Demand;
                }
            }
            allocEdge.m_AllocatedPower = newAlloc;
            totalAllocated = totalAllocated + newAlloc;

            // Track allocation change for Step 3 downstream re-enqueue.
            if (!m_AllocChanged)
            {
                float allocDelta = newAlloc - oldAlloc;
                if (allocDelta < 0.0)
                {
                    allocDelta = -allocDelta;
                }
                if (allocDelta > LFPG_PROPAGATION_EPSILON)
                {
                    m_AllocChanged = true;
                }
            }
        }

        // v2.0 Pass 3: Distribute surplus to soft demand edges proportionally.
        // Only runs when: not overloaded, totalSoftDemand > 0, and surplus exists.
        // For non-battery networks this block is skipped entirely (totalSoftDemand=0).
        if (!overloaded && totalSoftDemand > LFPG_PROPAGATION_EPSILON)
        {
            float surplus = availableOutput - totalAllocated;
            if (surplus > LFPG_PROPAGATION_EPSILON)
            {
                // Cap surplus to total soft demand (don't over-allocate).
                if (surplus > totalSoftDemand)
                {
                    surplus = totalSoftDemand;
                }
                int si;
                for (si = 0; si < edgeCount; si = si + 1)
                {
                    ref LFPG_ElecEdge softEdge = outEdges[si];
                    if (!softEdge)
                        continue;
                    if ((softEdge.m_Flags & LFPG_EDGE_ENABLED) == 0)
                        continue;

                    ref LFPG_ElecNode softTarget;
                    if (!m_Nodes.Find(softEdge.m_TargetNodeId, softTarget))
                        continue;
                    if (!softTarget)
                        continue;
                    if (softTarget.m_SoftDemandRatio < LFPG_PROPAGATION_EPSILON)
                        continue;

                    // Proportional share: this edge's soft / totalSoft * surplus
                    float thisEdgeSoft = softEdge.m_Demand * softTarget.m_SoftDemandRatio;
                    float softBonus = surplus * thisEdgeSoft / totalSoftDemand;
                    if (softBonus < 0.0)
                    {
                        softBonus = 0.0;
                    }

                    float prevAlloc = softEdge.m_AllocatedPower;
                    softEdge.m_AllocatedPower = prevAlloc + softBonus;
                    totalAllocated = totalAllocated + softBonus;

                    if (!m_AllocChanged)
                    {
                        if (softBonus > LFPG_PROPAGATION_EPSILON)
                        {
                            m_AllocChanged = true;
                        }
                    }
                }
            }
        }

        // Update node load metrics.
        // v2.0: LoadRatio uses totalAllocated (hard+soft) for accurate display.
        // Overloaded flag uses totalHardDemand (soft never causes overload).
        ref LFPG_ElecNode srcNode;
        if (m_Nodes.Find(nodeId, srcNode) && srcNode)
        {
            if (srcNode.m_DeviceType == LFPG_DeviceType.SOURCE || srcNode.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
            {
                // LoadRatio: actual usage / capacity (for inspector display + cable color)
                float capacity = srcNode.m_MaxOutput;
                if (srcNode.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
                {
                    capacity = availableOutput;
                }

                if (capacity > LFPG_PROPAGATION_EPSILON)
                {
                    float rawRatio = totalAllocated / capacity;
                    if (rawRatio < 0.0)
                    {
                        rawRatio = 0.0;
                    }
                    if (rawRatio > 100.0)
                    {
                        rawRatio = 100.0;
                    }
                    srcNode.m_LoadRatio = rawRatio;
                }
                else
                {
                    if (totalHardDemand > LFPG_PROPAGATION_EPSILON)
                    {
                        srcNode.m_LoadRatio = 100.0;
                    }
                    else
                    {
                        srcNode.m_LoadRatio = 0.0;
                    }
                }
                srcNode.m_Overloaded = overloaded;
            }
        }

        // v2.0: Return totalDemand (hard + soft) for upstream demand signaling.
        // The demand signal carries the full picture; the ratio separates them.
        return totalDemand;
        #else
        return 0.0;
        #endif
    }

    // v1.0: Get allocated power for a specific incoming edge.
    // If source is overloaded (all-off), returns 0 immediately.
    // Otherwise returns per-edge allocation, with equal-split fallback for cold-start.
    protected float GetEdgeAllocatedPower(LFPG_ElecEdge inEdge)
    {
        #ifdef SERVER
        if (!inEdge)
            return 0.0;

        ref LFPG_ElecNode srcNode;
        if (!m_Nodes.Find(inEdge.m_SourceNodeId, srcNode) || !srcNode)
            return 0.0;

        // v1.0: Source in overload → all downstream gets 0.
        if (srcNode.m_Overloaded)
            return 0.0;

        if (inEdge.m_AllocatedPower > LFPG_PROPAGATION_EPSILON)
            return inEdge.m_AllocatedPower;

        // v2.0.1: Gated PASSTHROUGH nodes (PushButton, PressurePad, Laser,
        // Counter) use m_OutputPower as demand signal, NOT real power.
        // When gate closes or upstream has no power, AllocateOutput zeroes
        // edge allocations. The fallback below would read the demand signal
        // from m_OutputPower and leak phantom power to downstream consumers,
        // causing a 1-frame flash. For gated devices, AllocateOutput always
        // runs (they are PASSTHROUGH), so m_AllocatedPower=0 is authoritative.
        if (srcNode.m_IsGated)
            return 0.0;

        // Fallback: equal split (cold-start / first pass before AllocateOutput runs)
        float srcOutput = srcNode.m_OutputPower;
        if (srcOutput < LFPG_PROPAGATION_EPSILON)
            return 0.0;

        int enabledOutCount = CountEnabledOutgoing(inEdge.m_SourceNodeId);
        if (enabledOutCount <= 0)
            return 0.0;

        return srcOutput / enabledOutCount;
        #else
        return 0.0;
        #endif
    }

    // Sprint 4.3: Targeted reset — only resets nodes that were actually enqueued.
    // v0.7.33 (Fix #15): m_EnqueuedThisEpoch is now a map — no duplicates to iterate.
    // v0.7.38 (RC-09): Also reset carryover nodes still pending in the dirty queue
    // from previous epochs. Without this, nodes that span multiple epochs (budget
    // exhaustion) accumulate m_RequeueCount across epochs and can hit
    // LFPG_MAX_REQUEUE_PER_EPOCH prematurely, causing permanent stuck state.
    protected void ResetRequeueCounts()
    {
        #ifdef SERVER
        // Phase 1: Reset nodes enqueued this epoch (existing behavior)
        int ri;
        for (ri = 0; ri < m_EnqueuedThisEpoch.Count(); ri = ri + 1)
        {
            string rNodeId = m_EnqueuedThisEpoch.GetKey(ri);
            ref LFPG_ElecNode rNode;
            if (m_Nodes.Find(rNodeId, rNode) && rNode)
            {
                rNode.m_RequeueCount = 0;
            }
        }
        m_EnqueuedThisEpoch.Clear();

        // Phase 2 (RC-09): Reset carryover nodes still in queue from prior epochs.
        // These were not in m_EnqueuedThisEpoch (they were enqueued in epoch N-1)
        // but still sit in m_DirtyQueue waiting to be processed. Without this reset,
        // any re-enqueue during the current epoch adds to their stale count.
        int qi;
        for (qi = m_DirtyQueueHead; qi < m_DirtyQueue.Count(); qi = qi + 1)
        {
            string qNodeId = m_DirtyQueue[qi];
            ref LFPG_ElecNode qNode;
            if (m_Nodes.Find(qNodeId, qNode) && qNode)
            {
                qNode.m_RequeueCount = 0;
            }
        }
        #endif
    }

    // =========================================================
    // Port-level power query (v1.3.1)
    //
    // Returns true if any incoming edge targeting the given port
    // on the given device has allocated power > 0 this epoch.
    //
    // Used by devices that need per-port awareness (e.g., RaidAlarm
    // Station uses input_2 as a trigger port distinct from the
    // always-on power feed on input_1).
    //
    // Safe to call from LFPG_SetPowered or any server-side context
    // after ProcessDirtyQueue has run for the current epoch.
    // =========================================================
    bool IsPortReceivingPower(string deviceId, string portName)
    {
        ref array<ref LFPG_ElecEdge> inEdges;
        if (!m_Incoming.Find(deviceId, inEdges))
            return false;

        if (!inEdges)
            return false;

        int i;
        int count = inEdges.Count();
        for (i = 0; i < count; i = i + 1)
        {
            ref LFPG_ElecEdge edge = inEdges[i];
            if (!edge)
                continue;

            // B4 fix: Skip disabled edges. Without this, a stale
            // m_AllocatedPower from a previous epoch on a disabled
            // edge returns a false positive.
            if ((edge.m_Flags & LFPG_EDGE_ENABLED) == 0)
                continue;

            if (edge.m_TargetPort != portName)
                continue;

            // B4 fix: Use EPSILON for consistency with rest of graph.
            if (edge.m_AllocatedPower > LFPG_PROPAGATION_EPSILON)
                return true;
        }

        return false;
    }
};
