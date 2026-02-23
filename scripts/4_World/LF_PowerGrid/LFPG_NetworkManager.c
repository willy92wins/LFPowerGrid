// =========================================================
// LF_PowerGrid v0.7.26 — PATCH: LFPG_NetworkManager.c
//
// ADD this function inside class LFPG_NetworkManager.
// Place after the existing RemoveWiresTargeting() method.
//
// CutAllWiresFromDevice: centralizada, usada por EEKilled,
// EEItemLocationChanged (GROUND→inventory), y EEDelete como
// single point of cleanup.
// =========================================================

    // v0.7.26: Centralized device wire cleanup.
    // Removes ALL wires connected to a device (both owned and targeting).
    // Updates reverse index, player counts, graph, and broadcasts changes.
    // Called from EEKilled, EEItemLocationChanged, EEDelete.
    void CutAllWiresFromDevice(EntityAI device)
    {
        #ifdef SERVER
        if (!device)
            return;

        string deviceId = LFPG_DeviceAPI.GetDeviceId(device);
        if (deviceId == "")
            return;

        bool anyChanged = false;

        // --- 1. Clear OWNED wires (output side) ---
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

                        // Notify graph of each wire removal
                        if (m_Graph)
                        {
                            m_Graph.OnWireRemoved(deviceId, wd.m_TargetDeviceId, wd.m_SourcePort, wd.m_TargetPort);
                        }
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

                        if (m_Graph)
                        {
                            m_Graph.OnWireRemoved(deviceId, vwd.m_TargetDeviceId, vwd.m_SourcePort, vwd.m_TargetPort);
                        }
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
                    LFPG_Util.Info("[CutAll] Removed " + removed.ToString() + " incoming wire(s) on " + deviceId + ":" + portName);
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
                        LFPG_Util.Warn("[CutAll-Fallback] Found stale wire: " + srcId + " -> " + deviceId);
                        PlayerWireCountAdd(swd.m_CreatorId, -1);

                        if (m_Graph)
                        {
                            m_Graph.OnWireRemoved(srcId, deviceId, swd.m_SourcePort, swd.m_TargetPort);
                        }

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

        // --- 6. Notify graph and propagate ---
        if (anyChanged)
        {
            if (m_Graph)
            {
                m_Graph.OnDeviceRemoved(deviceId);
            }
            PostBulkRebuildAndPropagate();
            LFPG_Util.Info("[CutAll] All wires removed for device " + deviceId + " type=" + device.GetType());
        }
        #endif
    }
