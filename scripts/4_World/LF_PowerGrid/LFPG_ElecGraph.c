// =========================================================
// LF_PowerGrid - Electrical Graph (Sprint 4.2 S3, v0.7.21)
//
// In-memory directed graph of the electrical network.
// Nodes = devices, edges = wires. Rebuilt from wire data at
// startup, maintained incrementally during runtime.
//
// NOT persisted — wires are the source of truth.
//
// Sprint 4.2 S2 fixes:
//   H1: AddEdgeInternal returns bool, OnWireAdded propagates it
//   H3: Per-epoch requeue reset (not only when queue empties)
//   H4: Head-index dirty queue (no array copy per tick)
//   PostBulkRebuild: correct order for bulk mutations
//
// Sprint 4.2 S2b fixes (audit #2):
//   H2: Dirty mask differentiation (INTERNAL/TOPOLOGY/INPUT)
//   H6: Consumer respects m_Consumption (not just epsilon)
//
// Sprint 4.2 S3: Dead code removal (legacy BFS propagation removed
//   from NetworkManager; this graph is now the sole propagation path).
//
// Server-only: all public methods are guarded by #ifdef SERVER.
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

        int startMs = GetGame().GetTickCount();

        // Clear everything
        m_Nodes.Clear();
        m_Outgoing.Clear();
        m_Incoming.Clear();
        m_DirtyQueue.Clear();
        m_DirtyQueueHead = 0;
        m_NodeCount = 0;
        m_EdgeCount = 0;

        // Step 1: Iterate all registered devices to create nodes
        // (only those that have wires will persist — lazy pruning later)
        // We still create them here to resolve types correctly.
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

            // Pre-create node (may be pruned if no edges after wire pass)
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

        // Step 4: Prune nodes with no edges (lazy: keep only connected nodes)
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
        }
        m_NodeCount = m_Nodes.Count();

        // Step 5: Rebuild component IDs
        m_ComponentsDirty = true;
        RebuildComponents();

        int elapsed = GetGame().GetTickCount() - startMs;
        m_LastRebuildMs = elapsed;

        LFPG_Util.Info("[ElecGraph] Rebuilt: " + m_NodeCount.ToString() + " nodes, "
            + m_EdgeCount.ToString() + " edges, "
            + m_NextComponentId.ToString() + " components in "
            + elapsed.ToString() + "ms");
        #endif
    }

    // ===========================
    // Incremental operations
    // ===========================

    // Called after a wire is successfully added.
    // Sprint 4.2 S2 (H1): returns true only if edge was actually inserted.
    // Only marks endpoints dirty when insertion succeeds.
    bool OnWireAdded(string sourceId, string targetId, string sourcePort, string targetPort, LFPG_WireData wireRef)
    {
        #ifdef SERVER
        if (sourceId == "" || targetId == "")
            return false;

        // Resolve entities for type detection
        EntityAI srcObj = LFPG_DeviceRegistry.Get().FindById(sourceId);
        EntityAI tgtObj = LFPG_DeviceRegistry.Get().FindById(targetId);

        EnsureNode(sourceId, srcObj);
        EnsureNode(targetId, tgtObj);

        bool inserted = AddEdgeInternal(sourceId, targetId, sourcePort, targetPort, wireRef);
        if (!inserted)
        {
            LFPG_Util.Warn("[ElecGraph] OnWireAdded: edge not inserted " + sourceId + " -> " + targetId);
            return false;
        }

        m_ComponentsDirty = true;

        // Sprint 4.2: Mark both endpoints dirty for propagation
        MarkNodeDirty(sourceId, LFPG_DIRTY_TOPOLOGY);
        MarkNodeDirty(targetId, LFPG_DIRTY_TOPOLOGY);

        return true;
        #else
        return false;
        #endif
    }

    // Called after a wire is removed.
    // Sprint 4.2: marks both endpoints dirty for propagation.
    void OnWireRemoved(string sourceId, string targetId, string sourcePort, string targetPort)
    {
        #ifdef SERVER
        RemoveEdgeInternal(sourceId, targetId, sourcePort, targetPort);
        CleanupOrphanNode(sourceId);
        CleanupOrphanNode(targetId);
        m_ComponentsDirty = true;

        // Sprint 4.2: Mark both endpoints dirty
        MarkNodeDirty(sourceId, LFPG_DIRTY_TOPOLOGY);
        MarkNodeDirty(targetId, LFPG_DIRTY_TOPOLOGY);
        #endif
    }

    // Called when a device is destroyed.
    // Removes the node and all its edges from the graph.
    void OnDeviceRemoved(string deviceId)
    {
        #ifdef SERVER
        if (deviceId == "")
            return;

        // Collect neighbors that will be affected
        ref array<string> affectedNeighbors = new array<string>;

        // Remove outgoing edges
        ref array<ref LFPG_ElecEdge> outEdges;
        if (m_Outgoing.Find(deviceId, outEdges) && outEdges)
        {
            int oi = outEdges.Count() - 1;
            while (oi >= 0)
            {
                ref LFPG_ElecEdge oEdge = outEdges[oi];
                if (oEdge)
                {
                    // Remove from target's incoming
                    RemoveFromIncoming(oEdge.m_TargetNodeId, deviceId, oEdge.m_SourcePort, oEdge.m_TargetPort);
                    affectedNeighbors.Insert(oEdge.m_TargetNodeId);
                    m_EdgeCount = m_EdgeCount - 1;
                }
                oi = oi - 1;
            }
        }

        // Remove incoming edges
        ref array<ref LFPG_ElecEdge> inEdges;
        if (m_Incoming.Find(deviceId, inEdges) && inEdges)
        {
            int ii = inEdges.Count() - 1;
            while (ii >= 0)
            {
                ref LFPG_ElecEdge iEdge = inEdges[ii];
                if (iEdge)
                {
                    // Remove from source's outgoing
                    RemoveFromOutgoing(iEdge.m_SourceNodeId, deviceId, iEdge.m_SourcePort, iEdge.m_TargetPort);
                    affectedNeighbors.Insert(iEdge.m_SourceNodeId);
                    m_EdgeCount = m_EdgeCount - 1;
                }
                ii = ii - 1;
            }
        }

        // Remove the node itself
        m_Nodes.Remove(deviceId);
        m_Outgoing.Remove(deviceId);
        m_Incoming.Remove(deviceId);
        m_NodeCount = m_Nodes.Count();

        // Cleanup any neighbors that became orphans
        int ai;
        for (ai = 0; ai < affectedNeighbors.Count(); ai = ai + 1)
        {
            CleanupOrphanNode(affectedNeighbors[ai]);
            // Sprint 4.2: Mark surviving neighbors dirty
            MarkNodeDirty(affectedNeighbors[ai], LFPG_DIRTY_TOPOLOGY);
        }

        m_ComponentsDirty = true;
        #endif
    }

    // ===========================
    // Cycle detection
    // ===========================

    // Returns true if adding an edge sourceId→targetId would create
    // a directed cycle. Uses iterative DFS from targetId following
    // outgoing edges — if it reaches sourceId, a cycle exists.
    bool DetectCycleIfAdded(string sourceId, string targetId)
    {
        #ifdef SERVER
        // Self-loop
        if (sourceId == targetId)
            return true;

        // DFS from targetId following outgoing edges
        ref array<string> stack = new array<string>;
        ref map<string, bool> visited = new map<string, bool>;

        stack.Insert(targetId);

        while (stack.Count() > 0)
        {
            int topIdx = stack.Count() - 1;
            string current = stack[topIdx];
            stack.Remove(topIdx);

            if (current == sourceId)
                return true;

            bool alreadyVisited = false;
            visited.Find(current, alreadyVisited);
            if (alreadyVisited)
                continue;

            visited.Set(current, true);

            ref array<ref LFPG_ElecEdge> edges;
            if (m_Outgoing.Find(current, edges) && edges)
            {
                int ei;
                for (ei = 0; ei < edges.Count(); ei = ei + 1)
                {
                    ref LFPG_ElecEdge edge = edges[ei];
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

    // Assigns component IDs to all nodes via undirected BFS.
    // Only runs when m_ComponentsDirty is set.
    void RebuildComponents()
    {
        #ifdef SERVER
        if (!m_ComponentsDirty)
            return;

        // Reset all component IDs
        int ri;
        for (ri = 0; ri < m_Nodes.Count(); ri = ri + 1)
        {
            ref LFPG_ElecNode rNode = m_Nodes.GetElement(ri);
            if (rNode)
                rNode.m_ComponentId = -1;
        }

        int nextId = 0;

        // BFS from each unvisited node
        int ni;
        for (ni = 0; ni < m_Nodes.Count(); ni = ni + 1)
        {
            ref LFPG_ElecNode startNode = m_Nodes.GetElement(ni);
            if (!startNode)
                continue;
            if (startNode.m_ComponentId != -1)
                continue;

            // BFS queue
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

                // Follow outgoing (undirected: treat as neighbors)
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

                // Follow incoming (undirected: treat as neighbors)
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

            nextId = nextId + 1;
        }

        m_NextComponentId = nextId;
        m_ComponentsDirty = false;
        #endif
    }

    // ===========================
    // Internal helpers
    // ===========================

    // Creates a node if it doesn't exist.
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
        }

        m_Nodes.Set(deviceId, node);
        m_NodeCount = m_Nodes.Count();
        #endif
    }

    // Adds an edge to the graph (no cycle check — internal use).
    // Validates that source and target nodes exist or can be created.
    // Sprint 4.2 S2 (H1): returns bool — true if edge was actually inserted.
    // Returns false on: empty IDs, missing nodes, edge limits exceeded.
    protected bool AddEdgeInternal(string sourceId, string targetId, string srcPort, string tgtPort, LFPG_WireData wireRef)
    {
        #ifdef SERVER
        if (sourceId == "" || targetId == "")
            return false;

        // Validate nodes exist — create if needed during rebuild
        ref LFPG_ElecNode srcNode;
        if (!m_Nodes.Find(sourceId, srcNode))
        {
            // Node not registered — try to resolve from DeviceRegistry
            EntityAI srcObj = LFPG_DeviceRegistry.Get().FindById(sourceId);
            if (!srcObj)
            {
                LFPG_Util.Warn("[ElecGraph] AddEdge rejected: source " + sourceId + " not in registry");
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
                LFPG_Util.Warn("[ElecGraph] AddEdge rejected: target " + targetId + " not in registry");
                return false;
            }
            EnsureNode(targetId, tgtObj);
        }

        // Sprint 4.2 (Audit #1): Check edge limits on source outgoing
        ref array<ref LFPG_ElecEdge> existOut;
        if (m_Outgoing.Find(sourceId, existOut) && existOut)
        {
            if (existOut.Count() >= LFPG_MAX_EDGES_PER_NODE)
            {
                LFPG_Util.Warn("[ElecGraph] AddEdge rejected: source limit " + sourceId + " (out=" + existOut.Count().ToString() + ")");
                return false;
            }
        }

        // Sprint 4.2 (Audit #1): Check edge limits on target incoming
        ref array<ref LFPG_ElecEdge> existIn;
        if (m_Incoming.Find(targetId, existIn) && existIn)
        {
            if (existIn.Count() >= LFPG_MAX_EDGES_PER_NODE)
            {
                LFPG_Util.Warn("[ElecGraph] AddEdge rejected: target limit " + targetId + " (in=" + existIn.Count().ToString() + ")");
                return false;
            }
        }

        // Create edge
        ref LFPG_ElecEdge edge = new LFPG_ElecEdge();
        edge.m_SourceNodeId = sourceId;
        edge.m_TargetNodeId = targetId;
        edge.m_SourcePort = srcPort;
        edge.m_TargetPort = tgtPort;
        edge.m_WireRef = wireRef;

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

    // Removes a specific edge matching all four identifiers.
    // Sprint 4.2 (Audit #2): Only decrements m_EdgeCount if both
    // Remove operations confirmed the edge existed.
    protected void RemoveEdgeInternal(string sourceId, string targetId, string srcPort, string tgtPort)
    {
        #ifdef SERVER
        // Remove from outgoing[sourceId]
        bool removedOut = RemoveFromOutgoing(sourceId, targetId, srcPort, tgtPort);
        // Remove from incoming[targetId]
        bool removedIn = RemoveFromIncoming(targetId, sourceId, srcPort, tgtPort);

        // Only decrement if at least one side confirmed removal
        if (removedOut || removedIn)
        {
            m_EdgeCount = m_EdgeCount - 1;
            if (m_EdgeCount < 0)
                m_EdgeCount = 0;
        }
        #endif
    }

    // Remove matching edge from m_Outgoing[ownerId].
    // Returns true if an edge was actually removed.
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

    // Remove matching edge from m_Incoming[targetId] where source == sourceId.
    // Returns true if an edge was actually removed.
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

    // Remove a node if it has no edges remaining.
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
            m_Nodes.Remove(deviceId);
            m_Outgoing.Remove(deviceId);
            m_Incoming.Remove(deviceId);
            m_NodeCount = m_Nodes.Count();
        }
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

    // Sprint 4.2 S2 (H2): Correct order for bulk mutations.
    // After any bulk wire removal (CutWires, CutPort), callers must
    // rebuild the graph FIRST, then populate states, then mark dirty.
    // This method encapsulates the correct sequence.
    void PostBulkRebuild(LFPG_NetworkManager mgr)
    {
        #ifdef SERVER
        if (!mgr)
            return;

        RebuildFromWires(mgr);
        PopulateAllNodeElecStates();
        MarkSourcesDirty();

        LFPG_Util.Info("[ElecGraph] PostBulkRebuild: rebuilt + populated + sources dirty");
        #endif
    }

    // ===========================
    // Sprint 4.2: Dirty marking
    // ===========================

    // Mark a node as needing re-evaluation during the next propagation tick.
    // mask: combination of LFPG_DIRTY_TOPOLOGY, LFPG_DIRTY_INPUT, LFPG_DIRTY_INTERNAL.
    // If the node is already in the queue, the mask is OR'd with the existing mask.
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
        }
        #endif
    }

    // Mark all nodes in a connected component as dirty.
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

    // Mark all SOURCE nodes as dirty (used at server warmup).
    // This triggers a full propagation cascade through the entire network.
    void MarkSourcesDirty()
    {
        #ifdef SERVER
        int ni;
        for (ni = 0; ni < m_Nodes.Count(); ni = ni + 1)
        {
            ref LFPG_ElecNode node = m_Nodes.GetElement(ni);
            if (node && node.m_DeviceType == LFPG_DeviceType.SOURCE)
            {
                MarkNodeDirty(m_Nodes.GetKey(ni), LFPG_DIRTY_INPUT);
            }
        }

        LFPG_Util.Info("[ElecGraph] MarkSourcesDirty: queued " + m_DirtyQueue.Count().ToString() + " sources");
        #endif
    }

    // ===========================
    // Sprint 4.2: Budgeted propagation
    // ===========================

    // Process up to 'budget' dirty nodes per call.
    // Returns the number of dirty nodes remaining in the queue.
    // Called periodically by NetworkManager.TickPropagation().
    //
    // Sprint 4.2 S2 fixes:
    //   H3-prev: Requeue counts reset per-epoch (not only on empty queue)
    //   H4-prev: Head-index dequeue avoids array copy churn
    //
    // Sprint 4.2 S2b fixes (audit #2):
    //   H2: Dirty mask differentiation:
    //       DIRTY_INTERNAL on SOURCE → skip input eval, just recalc output from state
    //       DIRTY_TOPOLOGY → full re-evaluation (shares may have changed)
    //       DIRTY_INPUT → normal input re-evaluation
    //   H6: Consumer respects m_Consumption threshold (not just epsilon)
    //   H7: epoch = tick-batch (one ProcessDirtyQueue call), NOT global wave
    //
    // Algorithm:
    //   For each dirty node (up to budget):
    //     1. Evaluate inputs based on dirty mask
    //     2. Compute this node's output based on device type
    //     3. If output changed beyond epsilon: mark downstream dirty
    //     4. Sync state to game entity
    int ProcessDirtyQueue(int budget)
    {
        #ifdef SERVER
        int queueLen = m_DirtyQueue.Count() - m_DirtyQueueHead;
        if (queueLen <= 0)
        {
            // Queue is logically empty — compact if needed
            if (m_DirtyQueue.Count() > 0)
            {
                m_DirtyQueue.Clear();
                m_DirtyQueueHead = 0;
            }
            return 0;
        }

        int startMs = GetGame().GetTickCount();

        // Ensure components are current (needed for warmup)
        if (m_ComponentsDirty)
            RebuildComponents();

        // H7: m_CurrentEpoch increments per ProcessDirtyQueue call (= per tick-batch).
        // It does NOT represent a full propagation wave (which may span multiple ticks).
        m_CurrentEpoch = m_CurrentEpoch + 1;

        // H3-prev: Reset requeue counts at start of each new epoch
        if (m_LastRequeueResetEpoch != m_CurrentEpoch)
        {
            ResetRequeueCounts();
            m_LastRequeueResetEpoch = m_CurrentEpoch;
        }

        int processed = 0;

        while (m_DirtyQueueHead < m_DirtyQueue.Count() && processed < budget)
        {
            string nodeId = m_DirtyQueue[m_DirtyQueueHead];
            m_DirtyQueueHead = m_DirtyQueueHead + 1;
            processed = processed + 1;

            ref LFPG_ElecNode node;
            if (!m_Nodes.Find(nodeId, node) || !node)
                continue;

            // Skip if already processed this epoch (dedup)
            if (node.m_LastEpoch == m_CurrentEpoch)
                continue;

            // Cycle protection: limit re-enqueues per epoch
            if (node.m_RequeueCount > LFPG_MAX_REQUEUE_PER_EPOCH)
            {
                LFPG_Util.Warn("[ElecGraph] Requeue limit reached for " + nodeId + " epoch=" + m_CurrentEpoch.ToString());
                node.m_Dirty = false;
                node.m_InQueue = false;
                node.m_DirtyMask = 0;
                continue;
            }

            // Capture dirty mask before clearing
            int dirtyMask = node.m_DirtyMask;

            // --- Step 1: Evaluate inputs ---
            // H2: Sources with ONLY DIRTY_INTERNAL skip input evaluation.
            // Their state was already refreshed by RefreshSourceState().
            // For all other cases: re-evaluate inputs from incoming edges.
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

                        // Skip disabled edges
                        if ((inEdge.m_Flags & LFPG_EDGE_ENABLED) == 0)
                            continue;

                        ref LFPG_ElecNode srcNode;
                        if (m_Nodes.Find(inEdge.m_SourceNodeId, srcNode) && srcNode)
                        {
                            // Each outgoing edge from source delivers a share of output.
                            // Simple: split evenly across all enabled outgoing edges.
                            float srcOutput = srcNode.m_OutputPower;
                            if (srcOutput > 0.0)
                            {
                                int enabledOutCount = CountEnabledOutgoing(inEdge.m_SourceNodeId);
                                if (enabledOutCount > 0)
                                {
                                    inputSum = inputSum + (srcOutput / enabledOutCount);
                                }
                            }
                        }
                    }
                }
                node.m_InputPower = inputSum;
            }
            else
            {
                // Source with DIRTY_INTERNAL: keep existing m_InputPower (irrelevant for sources)
                inputSum = node.m_InputPower;
            }

            // --- Step 2: Compute output based on device type ---
            float newOutput = 0.0;
            bool newPowered = false;

            if (node.m_DeviceType == LFPG_DeviceType.SOURCE)
            {
                // Source: output = capacity if turned on, 0 if off.
                // Source state was refreshed by RefreshSourceState or warmup.
                if (node.m_Powered)
                {
                    newOutput = node.m_MaxOutput;
                }
                newPowered = node.m_Powered;  // Source powered state is external
            }
            else if (node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
            {
                // Passthrough (splitter): powered if has any input, passes it through
                if (inputSum > LFPG_PROPAGATION_EPSILON)
                {
                    newPowered = true;
                    newOutput = inputSum;
                }
            }
            else if (node.m_DeviceType == LFPG_DeviceType.CONSUMER)
            {
                // H6: Consumer powered check uses m_Consumption when declared.
                // If m_Consumption > 0: powered only if input meets demand.
                // If m_Consumption == 0 (undeclared): powered if any input above epsilon.
                // This enables Sprint 4.3 load modeling while staying backward-compatible.
                if (node.m_Consumption > LFPG_PROPAGATION_EPSILON)
                {
                    // Real consumption check: input must meet or exceed demand
                    if (inputSum + LFPG_PROPAGATION_EPSILON >= node.m_Consumption)
                    {
                        newPowered = true;
                    }
                }
                else
                {
                    // Fallback: any input above epsilon powers the consumer
                    if (inputSum > LFPG_PROPAGATION_EPSILON)
                    {
                        newPowered = true;
                    }
                }
                newOutput = 0.0;  // Consumers don't output
            }
            else
            {
                // UNKNOWN: treat as consumer with no declared consumption
                if (inputSum > LFPG_PROPAGATION_EPSILON)
                    newPowered = true;
            }

            node.m_Powered = newPowered;

            // --- Step 3: If output changed, mark downstream dirty ---
            float outputDelta = newOutput - node.m_LastStableOutput;
            if (outputDelta < 0.0)
                outputDelta = -outputDelta;

            if (outputDelta > LFPG_PROPAGATION_EPSILON)
            {
                node.m_OutputPower = newOutput;
                node.m_LastStableOutput = newOutput;

                // Mark all downstream nodes dirty
                ref array<ref LFPG_ElecEdge> outEdges;
                if (m_Outgoing.Find(nodeId, outEdges) && outEdges)
                {
                    int oi;
                    for (oi = 0; oi < outEdges.Count(); oi = oi + 1)
                    {
                        ref LFPG_ElecEdge outEdge = outEdges[oi];
                        if (outEdge && outEdge.m_TargetNodeId != "")
                        {
                            // Increment target requeue count for cycle protection
                            ref LFPG_ElecNode tgtNode;
                            if (m_Nodes.Find(outEdge.m_TargetNodeId, tgtNode) && tgtNode)
                            {
                                tgtNode.m_RequeueCount = tgtNode.m_RequeueCount + 1;
                            }
                            MarkNodeDirty(outEdge.m_TargetNodeId, LFPG_DIRTY_INPUT);
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

        // H4: Compact the queue only when head passes threshold
        // This avoids per-tick array copies while preventing unbounded growth.
        int remaining = m_DirtyQueue.Count() - m_DirtyQueueHead;
        if (remaining <= 0)
        {
            // Queue fully drained
            m_DirtyQueue.Clear();
            m_DirtyQueueHead = 0;
        }
        else if (m_DirtyQueueHead >= LFPG_DIRTY_QUEUE_COMPACT_THRESHOLD)
        {
            // Compact: shift remaining entries to front
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

        int elapsed = GetGame().GetTickCount() - startMs;
        m_LastProcessMs = elapsed;

        if (processed > 0)
        {
            LFPG_Util.Debug("[ElecGraph] ProcessDirtyQueue: processed=" + processed.ToString()
                + " remaining=" + remaining.ToString()
                + " epoch=" + m_CurrentEpoch.ToString()
                + " ms=" + elapsed.ToString());
        }

        return remaining;
        #else
        return 0;
        #endif
    }

    // ===========================
    // Sprint 4.2: Entity sync
    // ===========================

    // Apply the graph node's electrical state to its game entity.
    // Only meaningful on server. Calls LFPG_DeviceAPI.SetPowered().
    protected void SyncNodeToEntity(string nodeId, LFPG_ElecNode node)
    {
        #ifdef SERVER
        if (!node)
            return;

        // Sources manage their own powered state externally — skip sync
        if (node.m_DeviceType == LFPG_DeviceType.SOURCE)
            return;

        EntityAI entObj = LFPG_DeviceRegistry.Get().FindById(nodeId);
        if (!entObj)
        {
            entObj = LFPG_DeviceAPI.ResolveVanillaDevice(nodeId);
        }
        if (!entObj)
            return;

        LFPG_DeviceAPI.SetPowered(entObj, node.m_Powered);
        #endif
    }

    // ===========================
    // Sprint 4.2: Warmup helpers
    // ===========================

    // Populate electrical properties for ALL nodes from their entities.
    // Called once at server startup after graph rebuild.
    // Reads MaxOutput and Consumption from entities via DeviceAPI.
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

            // Populate capacity (sources and passthroughs)
            if (node.m_DeviceType == LFPG_DeviceType.SOURCE)
            {
                node.m_MaxOutput = LFPG_DeviceAPI.GetCapacity(obj);
                // Read current on/off state from entity
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
                // Passthroughs don't have their own capacity limit (yet)
                node.m_MaxOutput = 0.0;
            }

            // Populate consumption (consumers)
            if (node.m_DeviceType == LFPG_DeviceType.CONSUMER)
            {
                node.m_Consumption = LFPG_DeviceAPI.GetConsumption(obj);
            }
        }

        LFPG_Util.Info("[ElecGraph] PopulateAllNodeElecStates: " + m_Nodes.Count().ToString() + " nodes");
        #endif
    }

    // Refresh a single source node's on/off state from its entity.
    // Called when a source is toggled (OnWorkStart/OnWorkStop).
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

        // Mark dirty so propagation picks up the state change
        MarkNodeDirty(nodeId, LFPG_DIRTY_INTERNAL);
        #endif
    }

    // ===========================
    // Sprint 4.2: Internal helpers
    // ===========================

    // Count enabled outgoing edges for a node.
    // Used to calculate per-edge power share.
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

    // Reset requeue counts on all nodes (called at end of full queue drain).
    protected void ResetRequeueCounts()
    {
        #ifdef SERVER
        int ni;
        for (ni = 0; ni < m_Nodes.Count(); ni = ni + 1)
        {
            ref LFPG_ElecNode node = m_Nodes.GetElement(ni);
            if (node)
                node.m_RequeueCount = 0;
        }
        #endif
    }
};
