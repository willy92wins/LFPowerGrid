class LFPG_ElecGraphImpl : LFPG_ElecGraph
{
    protected ref map<string, ref LFPG_ElecNode> m_Nodes;
    protected ref map<string, ref array<ref LFPG_ElecEdge>> m_Outgoing;
    protected ref map<string, ref array<ref LFPG_ElecEdge>> m_Incoming;
    protected int m_NextComponentId;
    protected bool m_ComponentsDirty;
    protected ref array<string> m_DirtyQueue;
    protected int m_DirtyQueueHead;      // H4: head index for O(1) dequeue without array copy
    protected int m_CurrentEpoch;
    protected ref map<string, int> m_RequeueEpoch;
    protected int m_NodeCount;
    protected int m_EdgeCount;
    protected int m_LastRebuildMs;
    protected int m_LastProcessMs;        // Sprint 4.2 S2: time spent in ProcessDirtyQueue
    protected int m_EdgesVisitedThisEpoch;
    protected bool m_PropagationEdgeAccountingActive;
    protected bool m_AllocChanged;
	protected ref array<float> m_PreviousAllocations;
    protected float m_LastAllocSoftDemand;
    protected ref map<int, int>     m_ComponentSizes;
    protected ref array<string>     m_WdgQueue;
    protected ref map<string, bool> m_WdgVisited;
    protected int m_ValidateTickCount;
    protected int m_LastValidateTick;
    protected int m_ValidateNodeIdx;
    protected int m_ValidateFixCount;
    protected ref map<string, float> m_ChargerLastChargeSec;
    protected bool m_MutationActive;
    protected int  m_MutationDepth;
    protected ref array<string> m_DeferredOrphanCleanup;
    protected ref array<string> m_DeferredRequeue;
    protected ref map<string, int> m_NodeNetLow;
    protected ref map<string, int> m_NodeNetHigh;
    protected ref map<string, bool> m_LastSyncPowered;
    protected ref map<string, bool> m_LastSyncOverloaded;
    protected ref TStringManagedMap m_LastSyncEntity;
    protected ref map<string, int> m_PoweredIncomingMemo;
    protected ref map<string, bool> m_HasEnabledDownstreamMemo;
    void LFPG_ElecGraphImpl()
    {
        m_Nodes = new map<string, ref LFPG_ElecNode>;
        m_Outgoing = new map<string, ref array<ref LFPG_ElecEdge>>;
        m_Incoming = new map<string, ref array<ref LFPG_ElecEdge>>;
        m_DirtyQueue = new array<string>;
        m_DirtyQueueHead = 0;
        m_NextComponentId = 0;
        m_ComponentsDirty = true;
        m_CurrentEpoch = 0;
        m_RequeueEpoch = new map<string, int>;
        m_NodeCount = 0;
        m_EdgeCount = 0;
        m_LastRebuildMs = 0;
        m_LastProcessMs = 0;
        m_EdgesVisitedThisEpoch = 0;
        m_PropagationEdgeAccountingActive = false;
        m_AllocChanged = false;
		m_PreviousAllocations = new array<float>;
        m_LastAllocSoftDemand = 0.0;
        m_ComponentSizes = new map<int, int>;
        m_WdgQueue = new array<string>;
        m_WdgVisited = new map<string, bool>;
        m_ValidateTickCount = 0;
        m_LastValidateTick = 0;
        m_ValidateNodeIdx = 0;
        m_ValidateFixCount = 0;
        m_ChargerLastChargeSec = new map<string, float>;
        m_MutationActive = false;
        m_MutationDepth = 0;
        m_DeferredOrphanCleanup = new array<string>;
        m_DeferredRequeue = new array<string>;
        m_NodeNetLow = new map<string, int>;
        m_NodeNetHigh = new map<string, int>;
        m_LastSyncPowered = new map<string, bool>;
        m_LastSyncOverloaded = new map<string, bool>;
        m_LastSyncEntity = new TStringManagedMap;
        m_PoweredIncomingMemo = new map<string, int>;
        m_HasEnabledDownstreamMemo = new map<string, bool>;
    }
	protected void CollectWiredNodeIds(LFPG_NetworkManager mgr, array<EntityAI> devices, map<string, bool> nodeIds)
	{
		#ifdef SERVER
		for (int deviceIdx = 0; deviceIdx < devices.Count(); deviceIdx = deviceIdx + 1)
		{
			EntityAI device = devices[deviceIdx];
			if (!device || !LFPG_DeviceAPI.HasWireStore(device))
				continue;
			string ownerId = LFPG_DeviceAPI.GetOrCreateDeviceId(device);
			if (ownerId == "")
				continue;
			array<ref LFPG_WireData> wires = LFPG_DeviceAPI.GetDeviceWires(device);
			if (!wires)
				continue;
			for (int wireIdx = 0; wireIdx < wires.Count(); wireIdx = wireIdx + 1)
			{
				LFPG_WireData wire = wires[wireIdx];
				if (!wire || wire.m_TargetDeviceId == "")
					continue;
				nodeIds.Set(ownerId, true);
				nodeIds.Set(wire.m_TargetDeviceId, true);
			}
		}
		int vanillaCount = mgr.GetVanillaWireOwnerCount();
		for (int vanillaIdx = 0; vanillaIdx < vanillaCount; vanillaIdx = vanillaIdx + 1)
		{
			string vanillaOwnerId = mgr.GetVanillaWireOwnerKey(vanillaIdx);
			if (vanillaOwnerId == "")
				continue;
			array<ref LFPG_WireData> vanillaWires = mgr.GetVanillaWires(vanillaOwnerId);
			if (!vanillaWires)
				continue;
			for (int vanillaWireIdx = 0; vanillaWireIdx < vanillaWires.Count(); vanillaWireIdx = vanillaWireIdx + 1)
			{
				LFPG_WireData vanillaWire = vanillaWires[vanillaWireIdx];
				if (!vanillaWire || vanillaWire.m_TargetDeviceId == "")
					continue;
				nodeIds.Set(vanillaOwnerId, true);
				nodeIds.Set(vanillaWire.m_TargetDeviceId, true);
			}
		}
		#endif
	}
    override void RebuildFromWires(LFPG_NetworkManager mgr)
    {
        #ifdef SERVER
        if (!mgr)
            return;
        int startMs = g_Game.GetTime();
        m_Nodes.Clear();
        m_Outgoing.Clear();
        m_Incoming.Clear();
        m_DirtyQueue.Clear();
        m_DirtyQueueHead = 0;
        m_NodeCount = 0;
        m_EdgeCount = 0;
        m_NodeNetLow.Clear();
        m_NodeNetHigh.Clear();
        m_RequeueEpoch.Clear();
        m_LastSyncPowered.Clear();
        m_LastSyncOverloaded.Clear();
        m_LastSyncEntity.Clear();
        ClearPropagationMemos();
        if (m_MutationActive)
        {
            m_MutationActive = false;
            m_MutationDepth = 0;
            m_DeferredOrphanCleanup.Clear();
        }
        m_DeferredRequeue.Clear();
        ref array<EntityAI> allDevices = new array<EntityAI>;
        LFPG_DeviceRegistry.Get().GetAll(allDevices);
		map<string, bool> wiredNodeIds = new map<string, bool>;
		CollectWiredNodeIds(mgr, allDevices, wiredNodeIds);
        int di;
        for (di = 0; di < allDevices.Count(); di = di + 1)
        {
            EntityAI devObj = allDevices[di];
            if (!devObj)
                continue;
            string devId = LFPG_DeviceAPI.GetOrCreateDeviceId(devObj);
            if (devId == "")
                continue;
			if (!wiredNodeIds.Contains(devId))
			{
				m_ChargerLastChargeSec.Remove(devId);
				continue;
			}
            EnsureNode(devId, devObj);
        }
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
            m_NodeNetLow.Remove(emptyNodes[ei]);
            m_NodeNetHigh.Remove(emptyNodes[ei]);
            m_ChargerLastChargeSec.Remove(emptyNodes[ei]);
        }
        m_NodeCount = m_Nodes.Count();
        m_ComponentsDirty = true;
        RebuildComponents();
        int elapsed = g_Game.GetTime() - startMs;
        m_LastRebuildMs = elapsed;
        string rbMsg = "[ElecGraph] Rebuilt: " + m_NodeCount.ToString() + " nodes, ";
        rbMsg = rbMsg + m_EdgeCount.ToString() + " edges, ";
        rbMsg = rbMsg + m_ComponentSizes.Count().ToString() + " components in ";
        rbMsg = rbMsg + elapsed.ToString() + "ms";
        LFPG_Util.Info(rbMsg);
        #endif
    }
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
            if (count > limit)
                return count;
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
	protected bool WouldExceedGlobalNodeLimit(string sourceId, string targetId)
	{
		int projectedCount = m_NodeCount;
		if (!m_Nodes.Contains(sourceId))
			projectedCount = projectedCount + 1;
		if (targetId != sourceId && !m_Nodes.Contains(targetId))
			projectedCount = projectedCount + 1;
		return projectedCount > LFPG_MAX_NODES_GLOBAL;
	}
    override bool OnWireAdded(string sourceId, string targetId, string sourcePort, string targetPort, LFPG_WireData wireRef)
    {
        #ifdef SERVER
        if (sourceId == "" || targetId == "")
            return false;
        if (sourceId == targetId)
            return false;
		if (WouldExceedGlobalNodeLimit(sourceId, targetId))
        {
            string capMsg = "[ElecGraph] OnWireAdded REJECTED: global cap (" + m_NodeCount.ToString() + "/" + LFPG_MAX_NODES_GLOBAL.ToString() + ")";
            LFPG_Util.Warn(capMsg);
            return false;
        }
        ref LFPG_ElecNode nodeA;
        ref LFPG_ElecNode nodeB;
        bool hasA = m_Nodes.Find(sourceId, nodeA);
        bool hasB = m_Nodes.Find(targetId, nodeB);
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
            if (compA >= 0 && compA == compB)
            {
            }
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
            }
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
            bool bInA = false;
            if (ranBfsA && hasB && nodeB)
            {
                m_WdgVisited.Find(targetId, bInA);
            }
            if (!bInA)
            {
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
        }
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
        bool addComponentsUpdated = false;
        if (!m_ComponentsDirty)
        {
            ref LFPG_ElecNode addSourceNode;
            ref LFPG_ElecNode addTargetNode;
            if (m_Nodes.Find(sourceId, addSourceNode) && addSourceNode && m_Nodes.Find(targetId, addTargetNode) && addTargetNode)
            {
                int addSourceComponent = addSourceNode.m_ComponentId;
                int addTargetComponent = addTargetNode.m_ComponentId;
                int addSourceSize = 0;
                int addTargetSize = 0;
                bool addSizesValid = true;
                if (addSourceComponent >= 0 && !m_ComponentSizes.Find(addSourceComponent, addSourceSize))
                    addSizesValid = false;
                if (addTargetComponent >= 0 && !m_ComponentSizes.Find(addTargetComponent, addTargetSize))
                    addSizesValid = false;
                if (addSizesValid && addSourceComponent >= 0 && addSourceComponent == addTargetComponent)
                {
                    addComponentsUpdated = true;
                }
                else if (addSizesValid && addSourceComponent < 0 && addTargetComponent < 0)
                {
                    int addNewComponent = m_NextComponentId;
                    m_NextComponentId = m_NextComponentId + 1;
                    addSourceNode.m_ComponentId = addNewComponent;
                    addTargetNode.m_ComponentId = addNewComponent;
                    m_ComponentSizes.Set(addNewComponent, 2);
                    addComponentsUpdated = true;
                }
                else if (addSizesValid && addSourceComponent < 0)
                {
                    addSourceNode.m_ComponentId = addTargetComponent;
                    m_ComponentSizes.Set(addTargetComponent, addTargetSize + 1);
                    addComponentsUpdated = true;
                }
                else if (addSizesValid && addTargetComponent < 0)
                {
                    addTargetNode.m_ComponentId = addSourceComponent;
                    m_ComponentSizes.Set(addSourceComponent, addSourceSize + 1);
                    addComponentsUpdated = true;
                }
                else if (addSizesValid)
                {
                    int addFromComponent = addTargetComponent;
                    int addToComponent = addSourceComponent;
                    string addRelabelStart = targetId;
                    int addExpectedRelabel = addTargetSize;
                    if (addSourceSize < addTargetSize)
                    {
                        addFromComponent = addSourceComponent;
                        addToComponent = addTargetComponent;
                        addRelabelStart = sourceId;
                        addExpectedRelabel = addSourceSize;
                    }
                    m_WdgQueue.Clear();
                    m_WdgVisited.Clear();
                    m_WdgQueue.Insert(addRelabelStart);
                    int addRelabelHead = 0;
                    int addRelabeled = 0;
                    while (addRelabelHead < m_WdgQueue.Count())
                    {
                        string addCurrentId = m_WdgQueue[addRelabelHead];
                        addRelabelHead = addRelabelHead + 1;
                        bool addWasVisited = false;
                        m_WdgVisited.Find(addCurrentId, addWasVisited);
                        if (addWasVisited)
                            continue;
                        m_WdgVisited.Set(addCurrentId, true);
                        ref LFPG_ElecNode addCurrentNode;
                        if (!m_Nodes.Find(addCurrentId, addCurrentNode) || !addCurrentNode || addCurrentNode.m_ComponentId != addFromComponent)
                            continue;
                        addCurrentNode.m_ComponentId = addToComponent;
                        addRelabeled = addRelabeled + 1;
                        ref array<ref LFPG_ElecEdge> addOutgoing;
                        if (m_Outgoing.Find(addCurrentId, addOutgoing) && addOutgoing)
                        {
                            int addOutIndex;
                            for (addOutIndex = 0; addOutIndex < addOutgoing.Count(); addOutIndex = addOutIndex + 1)
                            {
                                LFPG_ElecEdge addOutEdge = addOutgoing[addOutIndex];
                                ref LFPG_ElecNode addOutNode;
                                if (addOutEdge && m_Nodes.Find(addOutEdge.m_TargetNodeId, addOutNode) && addOutNode && addOutNode.m_ComponentId == addFromComponent)
                                    m_WdgQueue.Insert(addOutEdge.m_TargetNodeId);
                            }
                        }
                        ref array<ref LFPG_ElecEdge> addIncoming;
                        if (m_Incoming.Find(addCurrentId, addIncoming) && addIncoming)
                        {
                            int addInIndex;
                            for (addInIndex = 0; addInIndex < addIncoming.Count(); addInIndex = addInIndex + 1)
                            {
                                LFPG_ElecEdge addInEdge = addIncoming[addInIndex];
                                ref LFPG_ElecNode addInNode;
                                if (addInEdge && m_Nodes.Find(addInEdge.m_SourceNodeId, addInNode) && addInNode && addInNode.m_ComponentId == addFromComponent)
                                    m_WdgQueue.Insert(addInEdge.m_SourceNodeId);
                            }
                        }
                    }
                    if (addRelabeled == addExpectedRelabel)
                    {
                        m_ComponentSizes.Remove(addFromComponent);
                        m_ComponentSizes.Set(addToComponent, addSourceSize + addTargetSize);
                        addComponentsUpdated = true;
                    }
                }
            }
        }
        if (!addComponentsUpdated)
            m_ComponentsDirty = true;
        MarkNodeDirty(sourceId, LFPG_DIRTY_TOPOLOGY);
        MarkNodeDirty(targetId, LFPG_DIRTY_TOPOLOGY);
        return true;
        #else
        return false;
        #endif
    }
    override void OnWireRemoved(string sourceId, string targetId, string sourcePort, string targetPort)
    {
        #ifdef SERVER
        bool componentsWereClean = !m_ComponentsDirty;
        int oldComponentId = -1;
        ref LFPG_ElecNode oldSourceNode;
        if (componentsWereClean && m_Nodes.Find(sourceId, oldSourceNode) && oldSourceNode)
            oldComponentId = oldSourceNode.m_ComponentId;
        RemoveEdgeInternal(sourceId, targetId, sourcePort, targetPort);
        if (m_MutationActive)
        {
            m_DeferredOrphanCleanup.Insert(sourceId);
            m_DeferredOrphanCleanup.Insert(targetId);
        }
        else
        {
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
        bool removeComponentsUpdated = false;
        if (componentsWereClean && oldComponentId >= 0)
        {
            m_ComponentSizes.Remove(oldComponentId);
            m_WdgQueue.Clear();
            m_WdgVisited.Clear();
            int removeSourceSize = 0;
            ref LFPG_ElecNode removeSourceNode;
            if (m_Nodes.Find(sourceId, removeSourceNode) && removeSourceNode && removeSourceNode.m_ComponentId == oldComponentId)
                removeSourceSize = CountComponentLimited(sourceId, m_NodeCount + 1);
            bool removeTargetInSource = false;
            m_WdgVisited.Find(targetId, removeTargetInSource);
            int removeTargetSize = 0;
            ref LFPG_ElecNode removeTargetNode;
            if (!removeTargetInSource && m_Nodes.Find(targetId, removeTargetNode) && removeTargetNode && removeTargetNode.m_ComponentId == oldComponentId)
            {
                removeTargetSize = CountComponentLimited(targetId, m_NodeCount + 1);
                if (removeSourceSize > 0)
                {
                    int removeSplitComponent = m_NextComponentId;
                    m_NextComponentId = m_NextComponentId + 1;
                    int removeVisitedIndex;
                    for (removeVisitedIndex = 0; removeVisitedIndex < m_WdgVisited.Count(); removeVisitedIndex = removeVisitedIndex + 1)
                    {
                        string removeVisitedId = m_WdgVisited.GetKey(removeVisitedIndex);
                        ref LFPG_ElecNode removeVisitedNode;
                        if (m_Nodes.Find(removeVisitedId, removeVisitedNode) && removeVisitedNode)
                            removeVisitedNode.m_ComponentId = removeSplitComponent;
                    }
                    if (removeTargetSize > 0)
                        m_ComponentSizes.Set(removeSplitComponent, removeTargetSize);
                }
            }
            if (removeSourceSize > 0)
                m_ComponentSizes.Set(oldComponentId, removeSourceSize);
            else if (removeTargetSize > 0)
                m_ComponentSizes.Set(oldComponentId, removeTargetSize);
            removeComponentsUpdated = true;
        }
        if (!removeComponentsUpdated)
            m_ComponentsDirty = true;
        MarkNodeDirty(sourceId, LFPG_DIRTY_TOPOLOGY);
        MarkNodeDirty(targetId, LFPG_DIRTY_TOPOLOGY);
        #endif
    }
    override void OnDeviceRemoved(string deviceId)
    {
        #ifdef SERVER
        if (deviceId == "")
            return;
        ref LFPG_ElecNode removedNode;
        ref array<ref LFPG_ElecEdge> outEdges;
        ref array<ref LFPG_ElecEdge> inEdges;
        bool hadNode = m_Nodes.Find(deviceId, removedNode);
        bool hadOutgoing = m_Outgoing.Find(deviceId, outEdges);
        bool hadIncoming = m_Incoming.Find(deviceId, inEdges);
        if (!hadNode && !hadOutgoing && !hadIncoming)
            return;
        ref array<string> affectedNeighbors = new array<string>;
        if (hadOutgoing && outEdges)
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
        if (hadIncoming && inEdges)
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
        m_NodeNetLow.Remove(deviceId);
        m_NodeNetHigh.Remove(deviceId);
        m_RequeueEpoch.Remove(deviceId);
        m_LastSyncPowered.Remove(deviceId);
        m_LastSyncOverloaded.Remove(deviceId);
        m_LastSyncEntity.Remove(deviceId);
        m_ChargerLastChargeSec.Remove(deviceId);
        m_NodeCount = m_Nodes.Count();
        int ai;
        for (ai = 0; ai < affectedNeighbors.Count(); ai = ai + 1)
        {
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
    override void BeginGraphMutation()
    {
        #ifdef SERVER
        m_MutationDepth = m_MutationDepth + 1;
        m_MutationActive = true;
        #endif
    }
    override void EndGraphMutation()
    {
        #ifdef SERVER
        if (m_MutationDepth <= 0)
        {
            string wMutMsg = "[ElecGraph] EndGraphMutation called without matching Begin";
            LFPG_Util.Warn(wMutMsg);
            m_MutationActive = false;
            m_MutationDepth = 0;
            m_DeferredOrphanCleanup.Clear();
            return;
        }
        m_MutationDepth = m_MutationDepth - 1;
        if (m_MutationDepth > 0)
            return;  // Still inside nested mutation
        m_MutationActive = false;
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
    override bool DetectCycleIfAdded(string sourceId, string targetId)
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
            m_ComponentSizes.Set(nextId, compSize);
            nextId = nextId + 1;
        }
        m_NextComponentId = nextId;
        m_ComponentsDirty = false;
        #endif
    }
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
            int nLow = 0;
            int nHigh = 0;
            obj.GetNetworkID(nLow, nHigh);
            if (nLow != 0 || nHigh != 0)
            {
                m_NodeNetLow.Set(deviceId, nLow);
                m_NodeNetHigh.Set(deviceId, nHigh);
            }
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
                node.m_Consumption = LFPG_DeviceAPI.GetConsumption(obj);
                node.m_IsGated = LFPG_DeviceAPI.IsGateCapable(obj);
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
        ref LFPG_ElecEdge edge = new LFPG_ElecEdge();
        edge.m_SourceNodeId = sourceId;
        edge.m_TargetNodeId = targetId;
        edge.m_SourcePort = srcPort;
        edge.m_TargetPort = tgtPort;
        edge.m_WireRef = wireRef;
        edge.m_Flags = LFPG_EDGE_ENABLED;
        ref array<ref LFPG_ElecEdge> outArr;
        if (!m_Outgoing.Find(sourceId, outArr) || !outArr)
        {
            outArr = new array<ref LFPG_ElecEdge>;
            m_Outgoing.Set(sourceId, outArr);
        }
        outArr.Insert(edge);
        ref array<ref LFPG_ElecEdge> inArr;
        if (!m_Incoming.Find(targetId, inArr) || !inArr)
        {
            inArr = new array<ref LFPG_ElecEdge>;
            m_Incoming.Set(targetId, inArr);
        }
        inArr.Insert(edge);
        m_EdgeCount = m_EdgeCount + 1;
        ClearPropagationMemos();
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
            if (removedOut != removedIn)
            {
                string asymMsg = "[ElecGraph] WARN: asymmetric edge removal ";
                asymMsg = asymMsg + sourceId + " -> " + targetId;
                asymMsg = asymMsg + " out=" + removedOut.ToString();
                asymMsg = asymMsg + " in=" + removedIn.ToString();
                LFPG_Util.Warn(asymMsg);
            }
            ClearPropagationMemos();
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
            int orphanType = LFPG_DeviceType.CONSUMER;
            if (node)
            {
                orphanType = node.m_DeviceType;
            }
            ResetOrphanSyncVars(deviceId, orphanType);
            int orphanComponent = -1;
            int orphanComponentSize = 0;
            if (node)
                orphanComponent = node.m_ComponentId;
            if (orphanComponent >= 0 && m_ComponentSizes.Find(orphanComponent, orphanComponentSize))
            {
                orphanComponentSize = orphanComponentSize - 1;
                if (orphanComponentSize > 0)
                    m_ComponentSizes.Set(orphanComponent, orphanComponentSize);
                else
                    m_ComponentSizes.Remove(orphanComponent);
            }
            m_Nodes.Remove(deviceId);
            m_Outgoing.Remove(deviceId);
            m_Incoming.Remove(deviceId);
            m_NodeNetLow.Remove(deviceId);
            m_NodeNetHigh.Remove(deviceId);
            m_RequeueEpoch.Remove(deviceId);
            m_LastSyncPowered.Remove(deviceId);
            m_LastSyncOverloaded.Remove(deviceId);
            m_LastSyncEntity.Remove(deviceId);
            m_ChargerLastChargeSec.Remove(deviceId);
            m_NodeCount = m_Nodes.Count();
        }
        #endif
    }
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
            int cachedNetLow = 0;
            int cachedNetHigh = 0;
            bool hasNetLow = m_NodeNetLow.Find(deviceId, cachedNetLow);
            bool hasNetHigh = m_NodeNetHigh.Find(deviceId, cachedNetHigh);
            if (hasNetLow && hasNetHigh)
            {
                if (cachedNetLow != 0 || cachedNetHigh != 0)
                {
                    Object rawObj = g_Game.GetObjectByNetworkId(cachedNetLow, cachedNetHigh);
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
    override LFPG_ElecNode GetNode(string deviceId)
    {
        ref LFPG_ElecNode node;
        if (m_Nodes.Find(deviceId, node))
            return node;
        return null;
    }
    override array<ref LFPG_ElecEdge> GetOutgoing(string deviceId)
    {
        ref array<ref LFPG_ElecEdge> arr;
        if (m_Outgoing.Find(deviceId, arr))
            return arr;
        return null;
    }
    override float SumOutgoingAllocations(string nodeId)
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
    override bool WouldExceedComponentLimit(string sourceId, string targetId)
    {
        #ifdef SERVER
        if (sourceId == "" || targetId == "")
            return false;
		if (WouldExceedGlobalNodeLimit(sourceId, targetId))
            return true;
        int limit = LFPG_MAX_NODES_PER_COMPONENT;
        ref LFPG_ElecNode nodeA;
        ref LFPG_ElecNode nodeB;
        bool hasA = m_Nodes.Find(sourceId, nodeA);
        bool hasB = m_Nodes.Find(targetId, nodeB);
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
            if (compA >= 0 && compA == compB)
                return false;
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
        int bfsSizeA = 1;
        if (hasA && nodeA)
        {
            bfsSizeA = CountComponentLimited(sourceId, limit);
        }
        if (bfsSizeA > limit)
            return true;
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
    override array<ref LFPG_ElecEdge> GetIncoming(string deviceId)
    {
        ref array<ref LFPG_ElecEdge> arr;
        if (m_Incoming.Find(deviceId, arr))
            return arr;
        return null;
    }
    override int GetNodeCount()
    {
        return m_NodeCount;
    }
    override int GetEdgeCount()
    {
        return m_EdgeCount;
    }
    override int GetComponentCount()
    {
        if (m_ComponentsDirty)
            RebuildComponents();
        return m_ComponentSizes.Count();
    }
    override int GetLastRebuildMs()
    {
        return m_LastRebuildMs;
    }
    override int GetLastProcessMs()
    {
        return m_LastProcessMs;
    }
    override int GetCurrentEpoch()
    {
        return m_CurrentEpoch;
    }
    override int GetDirtyQueueSize()
    {
        return m_DirtyQueue.Count() - m_DirtyQueueHead;
    }
    override int GetOverloadedSourceCount()
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
    override int GetLastEdgesVisited()
    {
        return m_EdgesVisitedThisEpoch;
    }
    override bool VerifyPassthroughPowered(string nodeId)
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
    override void PostBulkRebuild(LFPG_NetworkManager mgr)
    {
        #ifdef SERVER
        if (!mgr)
            return;
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
    override void MarkNodeDirty(string nodeId, int mask)
    {
        #ifdef SERVER
        if (nodeId == "")
            return;
        ref LFPG_ElecNode node;
        if (!m_Nodes.Find(nodeId, node) || !node)
            return;
        node.m_DirtyMask = node.m_DirtyMask | mask;
        node.m_Dirty = true;
        if (m_CurrentEpoch > 0 && node.m_LastEpoch == m_CurrentEpoch)
        {
            EnsureRequeueEpoch(nodeId, node);
            node.m_RequeueCount = node.m_RequeueCount + 1;
            int prevEpochMark = m_CurrentEpoch - 1;
            node.m_LastEpoch = prevEpochMark;
        }
        if (!node.m_InQueue)
        {
            node.m_InQueue = true;
            m_DirtyQueue.Insert(nodeId);
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
    override void MarkSourcesDirty()
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
                MarkNodeDirty(m_Nodes.GetKey(ni), LFPG_DIRTY_INTERNAL);
            }
            else if (node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH && node.m_VirtualGeneration > LFPG_PROPAGATION_EPSILON)
            {
                MarkNodeDirty(m_Nodes.GetKey(ni), LFPG_DIRTY_INPUT);
            }
        }
        string infoSrcDirty = "[ElecGraph] MarkSourcesDirty: queued " + m_DirtyQueue.Count().ToString() + " sources";
        LFPG_Util.Info(infoSrcDirty);
        #endif
    }
    override int ProcessDirtyQueue(int nodeBudget, int edgeBudget)
    {
        #ifdef SERVER
        string dbgProc;
        EntityAI resolvedEnt;
		int startMs = g_Game.GetTime();
        m_PropagationEdgeAccountingActive = true;
        m_EdgesVisitedThisEpoch = 0;
        ClearPropagationMemos();
        m_ValidateTickCount = m_ValidateTickCount + 1;
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
            ValidateConsumerStates(edgeBudget);
			m_LastProcessMs = g_Game.GetTime() - startMs;
            m_PropagationEdgeAccountingActive = false;
            return 0;
        }
        if (m_ComponentsDirty)
            RebuildComponents();
        m_CurrentEpoch = m_CurrentEpoch + 1;
        int processed = 0;
        while (m_DirtyQueueHead < m_DirtyQueue.Count())
        {
            resolvedEnt = null;
            if (processed >= nodeBudget)
                break;
            if (m_EdgesVisitedThisEpoch >= edgeBudget)
                break;
            string nodeId = m_DirtyQueue[m_DirtyQueueHead];
            m_DirtyQueueHead = m_DirtyQueueHead + 1;
            ref LFPG_ElecNode node;
            if (!m_Nodes.Find(nodeId, node) || !node)
                continue;
            EnsureRequeueEpoch(nodeId, node);
            if (node.m_LastEpoch == m_CurrentEpoch)
            {
                node.m_InQueue = false;
                continue;  // Sprint 4.3 fix: processed NOT incremented here
            }
            processed = processed + 1;
            if (node.m_RequeueCount > LFPG_MAX_REQUEUE_PER_EPOCH)
            {
                string wReqMsg = "[ElecGraph] Requeue limit reached for " + nodeId + " epoch=" + m_CurrentEpoch.ToString();
                LFPG_Util.Warn(wReqMsg);
                node.m_InQueue = false;
                m_DeferredRequeue.Insert(nodeId);
                continue;
            }
            int dirtyMask = node.m_DirtyMask;
            m_AllocChanged = false;
            m_LastAllocSoftDemand = 0.0;
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
                        m_EdgesVisitedThisEpoch = m_EdgesVisitedThisEpoch + 1;
                        ref LFPG_ElecEdge inEdge = inEdges[ii];
                        if (!inEdge)
                            continue;
                        if ((inEdge.m_Flags & LFPG_EDGE_ENABLED) == 0)
                            continue;
                        float edgePower = GetEdgeAllocatedPower(inEdge);
                        if (edgePower < 0.0)
                        {
                            edgePower = 0.0;
                        }
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
            float newOutput = 0.0;
            bool newPowered = false;
            bool demandBecameKnown = false;
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
                float effectiveInput = inputSum + node.m_VirtualGeneration;
                if (effectiveInput > LFPG_PROPAGATION_EPSILON)
                {
                    float selfCons = node.m_Consumption;
                    if (selfCons > LFPG_PROPAGATION_EPSILON)
                    {
                        if (effectiveInput + LFPG_PROPAGATION_EPSILON >= selfCons)
                        {
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
                            newPowered = false;
                            newOutput = 0.0;
                        }
                    }
                    else
                    {
                        if (effectiveInput > LFPG_PROPAGATION_EPSILON)
                        {
                            newPowered = true;
                        }
                        newOutput = effectiveInput;
                    }
                    if (node.m_MaxOutput > LFPG_PROPAGATION_EPSILON && newOutput > node.m_MaxOutput)
                    {
                        newOutput = node.m_MaxOutput;
                    }
                }
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
            if (node.m_DeviceType == LFPG_DeviceType.SOURCE || node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
            {
                bool gateIsClosed = false;
                bool gateEntityResolved = false;
                if (node.m_IsGated)
                {
                    resolvedEnt = LFPG_DeviceRegistry.Get().FindById(nodeId);
                    if (resolvedEnt)
                    {
                        gateEntityResolved = true;
                        bool gateOpen = LFPG_DeviceAPI.IsGateOpen(resolvedEnt);
                        if (!gateOpen)
                        {
                            gateIsClosed = true;
                        }
                    }
                    else
                    {
                        gateIsClosed = node.m_GateClosed;
                    }
                }
                if (node.m_DeviceType == LFPG_DeviceType.SOURCE)
                {
                    node.m_OutputPower = newOutput;
                }
                float allocAvail = newOutput;
                if (gateIsClosed)
                {
                    allocAvail = 0.0;
                }
                float downstreamDemand = AllocateOutput(nodeId, allocAvail);
                if (newOutput < LFPG_PROPAGATION_EPSILON)
                {
                    node.m_Overloaded = false;
                    node.m_LoadRatio = 0.0;
                }
                if (gateIsClosed)
                {
                    node.m_Overloaded = false;
                    node.m_LoadRatio = 0.0;
                }
                if (node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
                {
                    float downstreamSoft = m_LastAllocSoftDemand;
                    float totalSoft = downstreamSoft + node.m_SoftDemand;
                    float hardBase = downstreamDemand - downstreamSoft + node.m_Consumption;
                    if (hardBase < 0.0)
                    {
                        hardBase = 0.0;
                    }
                    float virtualOffset = node.m_VirtualGeneration;
                    if (virtualOffset > hardBase)
                    {
                        virtualOffset = hardBase;
                    }
                    float demandSignal = hardBase - virtualOffset + totalSoft;
                    if (demandSignal < 0.0)
                    {
                        demandSignal = 0.0;
                    }
                    if (demandSignal > LFPG_PROPAGATION_EPSILON)
                    {
                        newOutput = demandSignal;
                        if (node.m_MaxOutput > LFPG_PROPAGATION_EPSILON && newOutput > node.m_MaxOutput)
                        {
                            float excess = newOutput - node.m_MaxOutput;
                            newOutput = node.m_MaxOutput;
                            totalSoft = totalSoft - excess;
                            if (totalSoft < 0.0)
                            {
                                totalSoft = 0.0;
                            }
                        }
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
                            newOutput = 0.0;
                        }
                    }
                    if (node.m_IsGated)
                    {
                        bool prevGateClosed = node.m_GateClosed;
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
                            float selfForGate = node.m_Consumption;
                            float baseDemand = LFPG_GATE_PROBE_DEMAND;
                            if (selfForGate > baseDemand)
                            {
                                baseDemand = selfForGate;
                            }
                            float gatedDemand = baseDemand + node.m_SoftDemand;
                            newOutput = gatedDemand;
                            if (node.m_SoftDemand > LFPG_PROPAGATION_EPSILON)
                            {
                                node.m_SoftDemandRatio = node.m_SoftDemand / gatedDemand;
                            }
                            else
                            {
                                node.m_SoftDemandRatio = 0.0;
                            }
                        }
                        if (gateEntityResolved && prevGateClosed != node.m_GateClosed)
                        {
                            m_AllocChanged = true;
                            ref array<ref LFPG_ElecEdge> gateInEdges;
                            if (m_Incoming.Find(nodeId, gateInEdges) && gateInEdges)
                            {
                                int gi;
                                for (gi = 0; gi < gateInEdges.Count(); gi = gi + 1)
                                {
                                    m_EdgesVisitedThisEpoch = m_EdgesVisitedThisEpoch + 1;
                                    ref LFPG_ElecEdge gInEdge = gateInEdges[gi];
                                    if (gInEdge && gInEdge.m_SourceNodeId != "")
                                    {
                                        MarkNodeDirty(gInEdge.m_SourceNodeId, LFPG_DIRTY_INPUT);
                                    }
                                }
                            }
                        }
                    }
                    if (!node.m_DemandKnown)
                    {
                        node.m_DemandKnown = true;
                        demandBecameKnown = true;
                    }
                }
            }
            float outputDelta = newOutput - node.m_LastStableOutput;
            if (outputDelta < 0.0)
                outputDelta = -outputDelta;
            bool inputChanged = false;
            if (node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
            {
                float inputDelta = inputSum - node.m_PrevInputPower;
                if (inputDelta < 0.0)
                {
                    inputDelta = 0.0 - inputDelta;
                }
                if (inputDelta > LFPG_PROPAGATION_EPSILON)
                {
                    inputChanged = true;
                }
                node.m_PrevInputPower = inputSum;
            }
            bool forceDownstream = false;
            if ((dirtyMask & LFPG_DIRTY_TOPOLOGY) != 0)
            {
                if (node.m_DeviceType == LFPG_DeviceType.SOURCE || node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
                {
                    forceDownstream = true;
                }
            }
            if (outputDelta > LFPG_PROPAGATION_EPSILON || forceDownstream || m_AllocChanged || inputChanged || demandBecameKnown)
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
                        m_EdgesVisitedThisEpoch = m_EdgesVisitedThisEpoch + 1;
                        if (outEdge && outEdge.m_TargetNodeId != "")
                        {
                            ref LFPG_ElecNode tgtNode;
                            if (m_Nodes.Find(outEdge.m_TargetNodeId, tgtNode) && tgtNode)
                            {
                                EnsureRequeueEpoch(outEdge.m_TargetNodeId, tgtNode);
                                tgtNode.m_RequeueCount = tgtNode.m_RequeueCount + 1;
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
                if (node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
                {
                    ref array<ref LFPG_ElecEdge> upEdges;
                    if (m_Incoming.Find(nodeId, upEdges) && upEdges)
                    {
                        int ui;
                        for (ui = 0; ui < upEdges.Count(); ui = ui + 1)
                        {
                            ref LFPG_ElecEdge upEdge = upEdges[ui];
                            m_EdgesVisitedThisEpoch = m_EdgesVisitedThisEpoch + 1;
                            if (upEdge && upEdge.m_SourceNodeId != "")
                            {
                                ref LFPG_ElecNode upNode;
                                if (m_Nodes.Find(upEdge.m_SourceNodeId, upNode) && upNode)
                                {
                                    EnsureRequeueEpoch(upEdge.m_SourceNodeId, upNode);
                                    upNode.m_RequeueCount = upNode.m_RequeueCount + 1;
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
            node.m_LastEpoch = m_CurrentEpoch;
            node.m_Dirty = false;
            node.m_InQueue = false;
            node.m_DirtyMask = 0;
            SyncNodeToEntity(nodeId, node, resolvedEnt, dirtyMask);
        }
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
                    }
                }
            }
            m_DeferredRequeue.Clear();
        }
        int remaining = m_DirtyQueue.Count() - m_DirtyQueueHead;
        if (remaining <= 0)
        {
            m_DirtyQueue.Clear();
            m_DirtyQueueHead = 0;
            ValidateConsumerStates(edgeBudget);
        }
        else if (m_DirtyQueueHead >= LFPG_DIRTY_QUEUE_COMPACT_THRESHOLD)
        {
			int ci;
			for (ci = 0; ci < remaining; ci = ci + 1)
			{
				string pendingNodeId = m_DirtyQueue[m_DirtyQueueHead + ci];
				m_DirtyQueue[ci] = pendingNodeId;
			}
			m_DirtyQueue.Resize(remaining);
            m_DirtyQueueHead = 0;
            remaining = m_DirtyQueue.Count();
        }
        int elapsed = g_Game.GetTime() - startMs;
        m_LastProcessMs = elapsed;
        if (processed > 0)
        {
            if (LFPG_LOG_LEVEL >= 2)
            {
                dbgProc = "[ElecGraph] ProcessDirtyQueue: processed=" + processed.ToString() + " edges=" + m_EdgesVisitedThisEpoch.ToString() + " remaining=" + remaining.ToString() + " epoch=" + m_CurrentEpoch.ToString() + " ms=" + elapsed.ToString();
                LFPG_Util.Debug(dbgProc);
            }
        }
        m_PropagationEdgeAccountingActive = false;
        return remaining;
        #else
        return 0;
        #endif
    }
    protected bool NodeEntitySyncUnchanged(string nodeId, LFPG_ElecNode node, EntityAI entObj, int dirtyMask)
    {
        Managed lastEntRaw;
        EntityAI lastEnt;
        bool lastPowered;
        bool lastOverloaded;
        float loadDelta;
        lastEnt = null;
        lastPowered = false;
        lastOverloaded = false;
        loadDelta = 0.0;
        if (!node || !entObj)
            return false;
        if ((dirtyMask & LFPG_DIRTY_TOPOLOGY) != 0)
            return false;
        if ((dirtyMask & LFPG_DIRTY_INPUT) != 0)
            return false;
        if (!m_LastSyncEntity.Find(nodeId, lastEntRaw))
            return false;
        lastEnt = EntityAI.Cast(lastEntRaw);
        if (!lastEnt)
            return false;
        if (lastEnt != entObj)
            return false;
        if (node.m_DeviceType == LFPG_DeviceType.SOURCE)
        {
            if (node.m_LastSyncedLoadRatio < 0.0)
                return false;
            if (!m_LastSyncPowered.Find(nodeId, lastPowered))
                return false;
            if (lastPowered != node.m_Powered)
                return false;
            if (!m_LastSyncOverloaded.Find(nodeId, lastOverloaded))
                return false;
            if (lastOverloaded != node.m_Overloaded)
                return false;
            loadDelta = node.m_LoadRatio - node.m_LastSyncedLoadRatio;
            if (loadDelta < 0.0)
                loadDelta = -loadDelta;
            if (loadDelta > 0.01)
                return false;
            return true;
        }
        if (!m_LastSyncPowered.Find(nodeId, lastPowered))
            return false;
        if (lastPowered != node.m_Powered)
            return false;
        if (node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
        {
            if (!m_LastSyncOverloaded.Find(nodeId, lastOverloaded))
                return false;
            if (lastOverloaded != node.m_Overloaded)
                return false;
        }
        return true;
    }
    protected void RememberNodeEntitySync(string nodeId, LFPG_ElecNode node, EntityAI entObj)
    {
        if (!node || !entObj)
            return;
        m_LastSyncEntity[nodeId] = entObj;
        m_LastSyncPowered.Set(nodeId, node.m_Powered);
        m_LastSyncOverloaded.Set(nodeId, node.m_Overloaded);
    }
    protected void SyncNodeToEntity(string nodeId, LFPG_ElecNode node, EntityAI knownEnt = null, int dirtyMask = 0)
    {
        #ifdef SERVER
        EntityAI entObj;
        int cachedNetLow;
        int cachedNetHigh;
        bool hasNetLow;
        bool hasNetHigh;
        Object rawObj;
        string ptLog5a;
        string ptLog5b;
        float loadDelta;
        string loadState;
        string telemMsg;
        Managed lastLoadEntRaw;
        EntityAI lastLoadEnt;
        bool forceLoadWrite;
        entObj = null;
        cachedNetLow = 0;
        cachedNetHigh = 0;
        hasNetLow = false;
        hasNetHigh = false;
        rawObj = null;
        ptLog5a = "";
        ptLog5b = "";
        loadDelta = 0.0;
        loadState = "";
        telemMsg = "";
        lastLoadEnt = null;
        forceLoadWrite = false;
        if (!node)
            return;
        if (knownEnt)
        {
            entObj = LFPG_DeviceRegistry.Get().FindById(nodeId, knownEnt);
        }
        if (!entObj)
        {
            entObj = LFPG_DeviceRegistry.Get().FindById(nodeId);
        }
        if (!entObj)
        {
            entObj = LFPG_DeviceAPI.ResolveVanillaDevice(nodeId);
        }
        if (!entObj)
        {
            hasNetLow = m_NodeNetLow.Find(nodeId, cachedNetLow);
            hasNetHigh = m_NodeNetHigh.Find(nodeId, cachedNetHigh);
            if (hasNetLow && hasNetHigh)
            {
                if (cachedNetLow != 0 || cachedNetHigh != 0)
                {
                    rawObj = g_Game.GetObjectByNetworkId(cachedNetLow, cachedNetHigh);
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
            m_LastSyncEntity.Remove(nodeId);
            if (LFPG_DIAG_PT_CHAIN && node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
            {
                ptLog5a = "[PT-CHAIN] SyncToEntity FAILED: entity NULL for ";
                ptLog5a = ptLog5a + nodeId;
                ptLog5a = ptLog5a + " type=PASSTHROUGH";
                ptLog5a = ptLog5a + " powered=" + node.m_Powered.ToString();
                ptLog5a = ptLog5a + " input=" + node.m_InputPower.ToString();
                ptLog5a = ptLog5a + " output=" + node.m_OutputPower.ToString();
                LFPG_Util.Warn(ptLog5a);
            }
            return;
        }
        if (NodeEntitySyncUnchanged(nodeId, node, entObj, dirtyMask))
            return;
        if (node.m_DeviceType == LFPG_DeviceType.SOURCE)
        {
            forceLoadWrite = false;
            if (node.m_LastSyncedLoadRatio < 0.0)
            {
                forceLoadWrite = true;
            }
            else if (!m_LastSyncEntity.Find(nodeId, lastLoadEntRaw))
            {
                forceLoadWrite = true;
            }
            else
            {
                lastLoadEnt = EntityAI.Cast(lastLoadEntRaw);
                if (!lastLoadEnt)
                {
                    forceLoadWrite = true;
                }
                else if (lastLoadEnt != entObj)
                {
                    forceLoadWrite = true;
                }
            }
            loadDelta = node.m_LoadRatio - node.m_LastSyncedLoadRatio;
            if (loadDelta < 0.0)
            {
                loadDelta = -loadDelta;
            }
            if (forceLoadWrite || loadDelta > 0.01)
            {
                LFPG_DeviceAPI.SetLoadRatio(entObj, node.m_LoadRatio);
                if (loadDelta > LFPG_LOAD_TELEM_DELTA)
                {
                    loadState = "NORMAL";
                    if (node.m_LoadRatio >= LFPG_LOAD_CRITICAL_THRESHOLD)
                    {
                        loadState = "OVERLOADED";
                    }
                    telemMsg = "[LoadTelem] " + nodeId;
                    telemMsg = telemMsg + " load=" + node.m_LoadRatio.ToString();
                    telemMsg = telemMsg + " prev=" + node.m_LastSyncedLoadRatio.ToString();
                    telemMsg = telemMsg + " cap=" + node.m_MaxOutput.ToString();
                    telemMsg = telemMsg + " state=" + loadState;
                    LFPG_Util.Info(telemMsg);
                }
                node.m_LastSyncedLoadRatio = node.m_LoadRatio;
            }
            LFPG_DeviceAPI.SetOverloaded(entObj, node.m_Overloaded);
            RememberNodeEntitySync(nodeId, node, entObj);
            return;
        }
        if (node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
        {
            if (LFPG_DIAG_PT_CHAIN)
            {
                ptLog5b = "[PT-CHAIN] SyncToEntity: ";
                ptLog5b = ptLog5b + nodeId;
                ptLog5b = ptLog5b + " powered=" + node.m_Powered.ToString();
                ptLog5b = ptLog5b + " input=" + node.m_InputPower.ToString();
                ptLog5b = ptLog5b + " output=" + node.m_OutputPower.ToString();
                ptLog5b = ptLog5b + " entity=" + entObj.GetType();
                LFPG_Util.Info(ptLog5b);
            }
            LFPG_DeviceAPI.SetPowered(entObj, node.m_Powered);
            LFPG_DeviceAPI.SetOverloaded(entObj, node.m_Overloaded);
            RememberNodeEntitySync(nodeId, node, entObj);
            return;
        }
        LFPG_DeviceAPI.SetPowered(entObj, node.m_Powered);
        RememberNodeEntitySync(nodeId, node, entObj);
        #endif
    }
    protected int ValidateConsumerStates(int edgeBudget)
    {
        #ifdef SERVER
        float afterEnergy;
        string chgLog;
        string skipLog;
        bool hasBat;
        int nodeTotal = m_Nodes.Count();
        if (nodeTotal <= 0)
            return 0;
        int tickDelta = m_ValidateTickCount - m_LastValidateTick;
        if (tickDelta < LFPG_CONSUMER_VALIDATE_TICK_INTERVAL)
            return 0;
        if (m_ValidateNodeIdx >= nodeTotal)
            m_ValidateNodeIdx = 0;
        int batchSize = LFPG_VALIDATE_BATCH_SIZE;
        if (batchSize > nodeTotal)
            batchSize = nodeTotal;
        int checked = 0;
        int fixed = 0;
        while (checked < batchSize)
        {
            if (m_EdgesVisitedThisEpoch >= edgeBudget)
                break;
            if (m_ValidateNodeIdx >= nodeTotal)
                m_ValidateNodeIdx = 0;
            string nodeId = m_Nodes.GetKey(m_ValidateNodeIdx);
            ref LFPG_ElecNode node = m_Nodes.GetElement(m_ValidateNodeIdx);
            m_ValidateNodeIdx = m_ValidateNodeIdx + 1;
            checked = checked + 1;
            if (!node)
                continue;
            if (node.m_DeviceType == LFPG_DeviceType.SOURCE || node.m_DeviceType == LFPG_DeviceType.UNKNOWN)
                continue;
            if (node.m_InQueue || node.m_Dirty)
                continue;
            float incomingPower = 0.0;
            bool hasAnyIncoming = false;
            ref array<ref LFPG_ElecEdge> inEdges;
            if (m_Incoming.Find(nodeId, inEdges) && inEdges)
            {
                int ii;
                for (ii = 0; ii < inEdges.Count(); ii = ii + 1)
                {
                    m_EdgesVisitedThisEpoch = m_EdgesVisitedThisEpoch + 1;
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
            if (incomingPower < 0.0)
            {
                incomingPower = 0.0;
            }
			float effectivePower = incomingPower;
			bool canEvaluatePower = hasAnyIncoming;
			if (node.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
			{
				effectivePower = effectivePower + node.m_VirtualGeneration;
				canEvaluatePower = effectivePower > LFPG_PROPAGATION_EPSILON;
			}
            bool shouldBePowered = false;
			if (canEvaluatePower)
            {
                if (node.m_Consumption > LFPG_PROPAGATION_EPSILON)
                {
					if (effectivePower + LFPG_PROPAGATION_EPSILON >= node.m_Consumption)
                    {
                        shouldBePowered = true;
                    }
                }
                else
                {
					if (effectivePower > LFPG_PROPAGATION_EPSILON)
                    {
                        shouldBePowered = true;
                    }
                }
            }
            if (nodeId.IndexOf("BatteryCharger") >= 0)
            {
                if (LFPG_LOG_LEVEL >= 2)
                {
                    string dbgMsg = "[Charger] Validate " + nodeId;
                    dbgMsg = dbgMsg + " powered=" + node.m_Powered.ToString();
                    dbgMsg = dbgMsg + " shouldBe=" + shouldBePowered.ToString();
                    dbgMsg = dbgMsg + " inPower=" + incomingPower.ToString();
                    dbgMsg = dbgMsg + " consumption=" + node.m_Consumption.ToString();
                    dbgMsg = dbgMsg + " hasIncoming=" + hasAnyIncoming.ToString();
                    LFPG_Util.Info(dbgMsg);
                }
            }
            if (node.m_Powered && !shouldBePowered)
            {
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
            else if (node.m_Powered && shouldBePowered)
            {
                if (nodeId.IndexOf("vp:") == 0)
                {
                    EntityAI vanEnt = LFPG_DeviceRegistry.Get().FindById(nodeId);
                    if (!vanEnt)
                    {
                        vanEnt = LFPG_DeviceAPI.ResolveVanillaDevice(nodeId);
                    }
                    if (vanEnt)
                    {
                        ComponentEnergyManager vanEm = vanEnt.GetCompEM();
                        if (vanEm)
                        {
                            float vanEnergy = vanEm.GetEnergy();
                            if (vanEnergy < LFPG_VANILLA_ENERGY_POOL * 0.5)
                            {
                                vanEm.SetEnergy(LFPG_VANILLA_ENERGY_POOL);
                            }
                            string battChargerCls = "BatteryCharger";
                            if (vanEnt.IsKindOf(battChargerCls))
                            {
                                bool chargerOn = vanEm.IsSwitchedOn();
                                EntityAI carBat = vanEnt.FindAttachmentBySlotName("LargeBattery");
                                if (chargerOn && carBat)
                                {
                                    ComponentEnergyManager batEm = carBat.GetCompEM();
                                    if (batEm)
                                    {
                                        float batEnergy = batEm.GetEnergy();
                                        float batMax = batEm.GetEnergyMax();
                                        if (batEnergy < batMax)
                                        {
                                            float nowSec = g_Game.GetTime() * 0.001;
                                            float lastSec = 0.0;
                                            bool hasLast = m_ChargerLastChargeSec.Find(nodeId, lastSec);
                                            float deltaSec = nowSec - lastSec;
                                            if (!hasLast || deltaSec < 0.1)
                                            {
                                                m_ChargerLastChargeSec.Set(nodeId, nowSec);
                                            }
                                            else
                                            {
                                                if (deltaSec > 10.0)
                                                    deltaSec = 10.0;
                                                float chargeAmount = LFPG_CHARGER_ENERGY_PER_SEC * deltaSec;
                                                batEm.AddEnergy(chargeAmount);
                                                ItemBase batItem = ItemBase.Cast(carBat);
                                                if (batItem)
                                                {
                                                    float eNorm = batEm.GetEnergy0To1();
                                                    batItem.SetQuantityNormalized(eNorm);
                                                }
                                                m_ChargerLastChargeSec.Set(nodeId, nowSec);
                                                if (LFPG_LOG_LEVEL >= 2)
                                                {
                                                    afterEnergy = batEm.GetEnergy();
                                                    chgLog = "[Charger] Charged ";
                                                    chgLog = chgLog + nodeId;
                                                    chgLog = chgLog + ": ";
                                                    chgLog = chgLog + batEnergy.ToString();
                                                    chgLog = chgLog + " -> ";
                                                    chgLog = chgLog + afterEnergy.ToString();
                                                    chgLog = chgLog + " / ";
                                                    chgLog = chgLog + batMax.ToString();
                                                    chgLog = chgLog + " dt=";
                                                    chgLog = chgLog + deltaSec.ToString();
                                                    LFPG_Util.Info(chgLog);
                                                }
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    m_ChargerLastChargeSec.Remove(nodeId);
                                    if (LFPG_LOG_LEVEL >= 2)
                                    {
                                        skipLog = "[Charger] Skip ";
                                        skipLog = skipLog + nodeId;
                                        skipLog = skipLog + " switchedOn=";
                                        skipLog = skipLog + chargerOn.ToString();
                                        hasBat = carBat != null;
                                        skipLog = skipLog + " hasBat=";
                                        skipLog = skipLog + hasBat.ToString();
                                        LFPG_Util.Info(skipLog);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        if (m_ValidateNodeIdx >= nodeTotal)
            m_ValidateNodeIdx = 0;
        if (checked > 0)
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
    override void PopulateAllNodeElecStates()
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
                node.m_MaxOutput = LFPG_DeviceAPI.GetCapacity(obj);
                if (node.m_MaxOutput < LFPG_PROPAGATION_EPSILON)
                {
                    node.m_MaxOutput = LFPG_DEFAULT_PASSTHROUGH_CAPACITY;
                }
                node.m_Consumption = LFPG_DeviceAPI.GetConsumption(obj);
                node.m_IsGated = LFPG_DeviceAPI.IsGateCapable(obj);
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
    override void RefreshSourceState(string nodeId)
    {
        #ifdef SERVER
        LFPG_ElecNode node = GetNode(nodeId);
        if (!node)
            return;
        if (node.m_DeviceType != LFPG_DeviceType.SOURCE && node.m_DeviceType != LFPG_DeviceType.CONSUMER && node.m_DeviceType != LFPG_DeviceType.CAMERA)
            return;
        EntityAI obj = LFPG_DeviceRegistry.Get().FindById(nodeId);
        if (!obj)
        {
            obj = LFPG_DeviceAPI.ResolveVanillaDevice(nodeId);
        }
        if (!obj)
            return;
        if (node.m_DeviceType == LFPG_DeviceType.SOURCE)
        {
            bool sourceOn = false;
            ComponentEnergyManager em;
            if (LFPG_DeviceAPI.IsSource(obj))
            {
                sourceOn = LFPG_DeviceAPI.GetSourceOn(obj);
            }
            else
            {
                em = obj.GetCompEM();
                if (em)
                    sourceOn = em.IsWorking();
            }
            node.m_Powered = sourceOn;
            node.m_MaxOutput = LFPG_DeviceAPI.GetCapacity(obj);
            MarkNodeDirty(nodeId, LFPG_DIRTY_INTERNAL);
            return;
        }
        node.m_Consumption = LFPG_DeviceAPI.GetConsumption(obj);
        MarkNodeDirty(nodeId, LFPG_DIRTY_INTERNAL);
        MarkUpstreamNodesDirty(nodeId);
        #endif
    }
    protected void MarkUpstreamNodesDirty(string nodeId)
    {
        #ifdef SERVER
        array<string> upstreamQueue = new array<string>;
        map<string, bool> upstreamVisited = new map<string, bool>;
        int queueHead = 0;
        string currentId;
        array<ref LFPG_ElecEdge> incomingEdges;
        int edgeIndex;
        LFPG_ElecEdge incomingEdge;
        string upstreamId;
        bool alreadyVisited;
        LFPG_ElecNode upstreamNode;
        upstreamQueue.Insert(nodeId);
        upstreamVisited.Set(nodeId, true);
        while (queueHead < upstreamQueue.Count())
        {
            currentId = upstreamQueue[queueHead];
            queueHead = queueHead + 1;
            incomingEdges = GetIncoming(currentId);
            if (!incomingEdges)
                continue;
            for (edgeIndex = 0; edgeIndex < incomingEdges.Count(); edgeIndex = edgeIndex + 1)
            {
                incomingEdge = incomingEdges[edgeIndex];
                if (!incomingEdge || incomingEdge.m_SourceNodeId == "")
                    continue;
                upstreamId = incomingEdge.m_SourceNodeId;
                alreadyVisited = false;
                upstreamVisited.Find(upstreamId, alreadyVisited);
                if (alreadyVisited)
                    continue;
                upstreamVisited.Set(upstreamId, true);
                upstreamNode = GetNode(upstreamId);
                if (!upstreamNode)
                    continue;
                MarkNodeDirty(upstreamId, LFPG_DIRTY_INPUT);
                if (upstreamNode.m_DeviceType != LFPG_DeviceType.SOURCE)
                    upstreamQueue.Insert(upstreamId);
            }
        }
        #endif
    }
    protected void ClearPropagationMemos()
    {
        if (m_PoweredIncomingMemo)
            m_PoweredIncomingMemo.Clear();
        if (m_HasEnabledDownstreamMemo)
            m_HasEnabledDownstreamMemo.Clear();
    }
    protected int CountEnabledOutgoing(string nodeId)
    {
        ref array<ref LFPG_ElecEdge> outEdges;
        if (!m_Outgoing.Find(nodeId, outEdges) || !outEdges)
            return 0;
        int count = 0;
        int oi;
        for (oi = 0; oi < outEdges.Count(); oi = oi + 1)
        {
            m_EdgesVisitedThisEpoch = m_EdgesVisitedThisEpoch + 1;
            ref LFPG_ElecEdge edge = outEdges[oi];
            if (edge && (edge.m_Flags & LFPG_EDGE_ENABLED) != 0)
            {
                count = count + 1;
            }
        }
        return count;
    }
    protected bool HasEnabledDownstream(string nodeId)
    {
        bool cachedHasDown;
        if (m_HasEnabledDownstreamMemo)
        {
            if (m_HasEnabledDownstreamMemo.Contains(nodeId))
            {
                cachedHasDown = m_HasEnabledDownstreamMemo.Get(nodeId);
                return cachedHasDown;
            }
        }
        bool hasDown = false;
        ref array<ref LFPG_ElecEdge> ptOutEdges;
        if (m_Outgoing.Find(nodeId, ptOutEdges) && ptOutEdges)
        {
            int pti;
            for (pti = 0; pti < ptOutEdges.Count(); pti = pti + 1)
            {
                m_EdgesVisitedThisEpoch = m_EdgesVisitedThisEpoch + 1;
                if (!hasDown)
                {
                    LFPG_ElecEdge ptEdge = ptOutEdges[pti];
                    if (ptEdge && (ptEdge.m_Flags & LFPG_EDGE_ENABLED) != 0)
                    {
                        hasDown = true;
                        break;
                    }
                }
            }
        }
        if (m_HasEnabledDownstreamMemo)
            m_HasEnabledDownstreamMemo.Set(nodeId, hasDown);
        return hasDown;
    }
    protected int CountPoweredIncoming(string nodeId)
    {
        int memoCount;
        if (m_PoweredIncomingMemo)
        {
            if (m_PoweredIncomingMemo.Find(nodeId, memoCount))
                return memoCount;
        }
        ref array<ref LFPG_ElecEdge> inEdges;
        if (!m_Incoming.Find(nodeId, inEdges) || !inEdges)
        {
            if (m_PoweredIncomingMemo)
                m_PoweredIncomingMemo.Set(nodeId, 0);
            return 0;
        }
        int count = 0;
        int cpi;
        for (cpi = 0; cpi < inEdges.Count(); cpi = cpi + 1)
        {
            m_EdgesVisitedThisEpoch = m_EdgesVisitedThisEpoch + 1;
            ref LFPG_ElecEdge cpEdge = inEdges[cpi];
            if (!cpEdge)
                continue;
            if ((cpEdge.m_Flags & LFPG_EDGE_ENABLED) == 0)
                continue;
            ref LFPG_ElecNode cpSrcNode;
            if (m_Nodes.Find(cpEdge.m_SourceNodeId, cpSrcNode) && cpSrcNode)
            {
				float supplierPower = cpSrcNode.m_OutputPower;
				if (cpSrcNode.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
				{
					supplierPower = cpSrcNode.m_InputPower + cpSrcNode.m_VirtualGeneration;
					if (cpSrcNode.m_Consumption > LFPG_PROPAGATION_EPSILON)
					{
						supplierPower = supplierPower - cpSrcNode.m_Consumption;
					}
					if (cpSrcNode.m_GateClosed)
					{
						supplierPower = 0.0;
					}
				}
				if (supplierPower > LFPG_PROPAGATION_EPSILON)
                {
                    count = count + 1;
                }
            }
        }
        if (m_PoweredIncomingMemo)
            m_PoweredIncomingMemo.Set(nodeId, count);
        return count;
    }
    protected float AllocateOutput(string nodeId, float availableOutput)
    {
        #ifdef SERVER
        if (m_PoweredIncomingMemo)
            m_PoweredIncomingMemo.Clear();
        ref array<ref LFPG_ElecEdge> outEdges;
        if (!m_Outgoing.Find(nodeId, outEdges) || !outEdges)
            return 0.0;
        int edgeCount = outEdges.Count();
        if (edgeCount <= 0)
            return 0.0;
        float totalDemand = 0.0;
        float totalSoftDemand = 0.0;
        float edgeDemand = 0.0;
        float edgeSoftPortion = 0.0;
        int ei;
        for (ei = 0; ei < edgeCount; ei = ei + 1)
        {
            m_EdgesVisitedThisEpoch = m_EdgesVisitedThisEpoch + 1;
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
                    if (edgeDemand < LFPG_PROPAGATION_EPSILON && !targetNode.m_DemandKnown)
                    {
                        if (targetNode.m_GateClosed)
                        {
                            float gateSelf = targetNode.m_Consumption;
                            if (gateSelf > LFPG_GATE_PROBE_DEMAND)
                            {
                                edgeDemand = gateSelf;
                            }
                            else
                            {
                                edgeDemand = LFPG_GATE_PROBE_DEMAND;
                            }
                        }
                        else
                        {
                            bool ptHasDown = HasEnabledDownstream(edge.m_TargetNodeId);
                            if (ptHasDown && targetNode.m_MaxOutput > LFPG_PROPAGATION_EPSILON)
                            {
                                edgeDemand = targetNode.m_MaxOutput;
                                if (edgeDemand > availableOutput)
                                {
                                    edgeDemand = availableOutput;
                                }
                            }
                            else
                            {
                                edgeDemand = targetNode.m_Consumption;
                            }
                        }
                    }
                    int ptPoweredIn = CountPoweredIncoming(edge.m_TargetNodeId);
                    if (ptPoweredIn > 1)
                    {
                        edgeDemand = edgeDemand / ptPoweredIn;
                    }
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
        m_LastAllocSoftDemand = totalSoftDemand;
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
		m_PreviousAllocations.Clear();
        float totalAllocated = 0.0;
        int ai;
        for (ai = 0; ai < edgeCount; ai = ai + 1)
        {
            m_EdgesVisitedThisEpoch = m_EdgesVisitedThisEpoch + 1;
			m_PreviousAllocations.Insert(0.0);
            ref LFPG_ElecEdge allocEdge = outEdges[ai];
            if (!allocEdge)
                continue;
            if ((allocEdge.m_Flags & LFPG_EDGE_ENABLED) == 0)
                continue;
			m_PreviousAllocations[ai] = allocEdge.m_AllocatedPower;
            float newAlloc = 0.0;
            if (!overloaded)
            {
                if (totalSoftDemand > LFPG_PROPAGATION_EPSILON)
                {
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
                    newAlloc = allocEdge.m_Demand;
                }
            }
            allocEdge.m_AllocatedPower = newAlloc;
            totalAllocated = totalAllocated + newAlloc;
        }
        if (!overloaded && totalSoftDemand > LFPG_PROPAGATION_EPSILON)
        {
            float surplus = availableOutput - totalAllocated;
            if (surplus > LFPG_PROPAGATION_EPSILON)
            {
                if (surplus > totalSoftDemand)
                {
                    surplus = totalSoftDemand;
                }
                int si;
                for (si = 0; si < edgeCount; si = si + 1)
                {
                    m_EdgesVisitedThisEpoch = m_EdgesVisitedThisEpoch + 1;
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
                    float thisEdgeSoft = softEdge.m_Demand * softTarget.m_SoftDemandRatio;
                    float softBonus = surplus * thisEdgeSoft / totalSoftDemand;
                    if (softBonus < 0.0)
                    {
                        softBonus = 0.0;
                    }
                    float prevAlloc = softEdge.m_AllocatedPower;
                    softEdge.m_AllocatedPower = prevAlloc + softBonus;
                    totalAllocated = totalAllocated + softBonus;
                }
            }
        }
		LFPG_ElecEdge finalEdge;
		float finalDelta;
		for (int ci = 0; ci < edgeCount; ci = ci + 1)
		{
			if (m_AllocChanged)
				break;
			m_EdgesVisitedThisEpoch = m_EdgesVisitedThisEpoch + 1;
			finalEdge = outEdges[ci];
			if (!finalEdge)
				continue;
			if ((finalEdge.m_Flags & LFPG_EDGE_ENABLED) == 0)
				continue;
			finalDelta = finalEdge.m_AllocatedPower - m_PreviousAllocations[ci];
			if (finalDelta < 0.0)
			{
				finalDelta = -finalDelta;
			}
			if (finalDelta > LFPG_PROPAGATION_EPSILON)
			{
				m_AllocChanged = true;
			}
		}
        ref LFPG_ElecNode srcNode;
        if (m_Nodes.Find(nodeId, srcNode) && srcNode)
        {
            if (srcNode.m_DeviceType == LFPG_DeviceType.SOURCE || srcNode.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
            {
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
        return totalDemand;
        #else
        return 0.0;
        #endif
    }
    protected float GetEdgeAllocatedPower(LFPG_ElecEdge inEdge)
    {
        #ifdef SERVER
        if (!inEdge)
            return 0.0;
        ref LFPG_ElecNode srcNode;
        if (!m_Nodes.Find(inEdge.m_SourceNodeId, srcNode) || !srcNode)
            return 0.0;
        if (srcNode.m_Overloaded)
            return 0.0;
        if (inEdge.m_AllocatedPower > LFPG_PROPAGATION_EPSILON)
            return inEdge.m_AllocatedPower;
        if (srcNode.m_DeviceType == LFPG_DeviceType.PASSTHROUGH)
            return 0.0;
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
    protected void EnsureRequeueEpoch(string nodeId, LFPG_ElecNode node)
    {
        #ifdef SERVER
        if (!node)
            return;
        int nodeEpoch = -1;
        bool hasEpoch = m_RequeueEpoch.Find(nodeId, nodeEpoch);
        if (!hasEpoch || nodeEpoch != m_CurrentEpoch)
        {
            node.m_RequeueCount = 0;
            m_RequeueEpoch.Set(nodeId, m_CurrentEpoch);
        }
        #endif
    }
    override bool IsPortReceivingPower(string deviceId, string portName)
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
            if (m_PropagationEdgeAccountingActive)
            {
                m_EdgesVisitedThisEpoch = m_EdgesVisitedThisEpoch + 1;
            }
            ref LFPG_ElecEdge edge = inEdges[i];
            if (!edge)
                continue;
            if ((edge.m_Flags & LFPG_EDGE_ENABLED) == 0)
                continue;
            if (edge.m_TargetPort != portName)
                continue;
            if (edge.m_AllocatedPower > LFPG_PROPAGATION_EPSILON)
                return true;
        }
        return false;
    }
    override void SetOutputPortEnabled(string deviceId, string portName, bool enabled)
    {
        #ifdef SERVER
        ref array<ref LFPG_ElecEdge> outEdges;
        if (!m_Outgoing.Find(deviceId, outEdges))
            return;
        if (!outEdges)
            return;
        int i;
        int count = outEdges.Count();
        bool anyChanged = false;
        for (i = 0; i < count; i = i + 1)
        {
            ref LFPG_ElecEdge edge = outEdges[i];
            if (!edge)
                continue;
            if (edge.m_SourcePort != portName)
                continue;
            bool wasEnabled = ((edge.m_Flags & LFPG_EDGE_ENABLED) != 0);
            if (enabled && !wasEnabled)
            {
                edge.m_Flags = edge.m_Flags | LFPG_EDGE_ENABLED;
                anyChanged = true;
            }
            else if (!enabled && wasEnabled)
            {
                edge.m_Flags = 0;
                anyChanged = true;
            }
        }
        if (anyChanged)
        {
            MarkNodeDirty(deviceId, LFPG_DIRTY_TOPOLOGY);
            string dbg = "[ElecGraph] SetOutputPortEnabled: ";
            dbg = dbg + deviceId;
            dbg = dbg + " port=" + portName;
            dbg = dbg + " enabled=" + enabled.ToString();
            LFPG_Util.Debug(dbg);
        }
        #endif
    }
};