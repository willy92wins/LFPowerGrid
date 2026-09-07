// =========================================================
// LF_PowerGrid - Device Lifecycle Helpers (v0.7.27)
//
// Centralized static helpers for LFPG device lifecycle events.
// Replaces duplicated EEKilled/EEDelete/EEItemLocationChanged
// logic across LFPG_Generator, LF_TestLamp, and LFPG_Splitter.
//
// WHY STATIC HELPER INSTEAD OF BASE CLASS:
//   LFPG_Generator : PowerGenerator
//   LF_TestLamp      : Spotlight
//   LFPG_Splitter      : Inventory_Base
//   Enforce Script has no multiple inheritance, so a common base
//   class is impossible without breaking vanilla parent chains.
//   Static helpers achieve the same goal: single source of truth
//   for wire cleanup, graph notification, and registry management.
//
// USAGE:
//   override void EEKilled(Object killer) {
//       LFPG_DeviceLifecycle.OnDeviceKilled(this, m_DeviceId);
//       super.EEKilled(killer);
//   }
//
// v0.7.27: Initial implementation. Consolidates Bug 1/2 fixes
//          from v0.7.26 patches into single-call helpers.
// =========================================================

class LFPG_DeviceLifecycle
{
    // ============================================
    // EEKilled handler
    // ============================================
    // Called when a device reaches RUINED state (before deletion).
    // Cuts all wires and cleans graph. Device-specific cleanup
    // (m_SourceOn, m_PoweredNet, CompEM, lights) is done by caller.
    static void OnDeviceKilled(EntityAI device, string deviceId)
    {
        #ifdef SERVER
        if (!device)
            return;

        if (deviceId == "")
            return;

        LFPG_Util.Warn("[DeviceLifecycle] OnDeviceKilled: id=" + deviceId + " type=" + device.GetType());
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm) nm.CutAllWiresFromDevice(device, deviceId);
        #endif
    }

    // ============================================
    // EEDelete handler
    // ============================================
    // Called when a device is being deleted from the world.
    // Cuts wires, notifies graph, and unregisters from registry.
    // Must be called BEFORE super.EEDelete().
    static void OnDeviceDeleted(EntityAI device, string deviceId)
	{
		#ifdef SERVER
		// GetExisting, not Get: Get() builds the manager when absent, and its
		// constructor installs the graph or a degraded fallback, loads vanilla wires,
		// schedules validation and starts the scheduler. A manager that does not
		// exist holds no wires to cut and no graph node to drop.
		if (device && deviceId != "")
		{
			LFPG_NetworkManager nm = LFPG_NetworkManager.GetExisting();
			if (nm) nm.CutAllWiresFromDevice(device, deviceId);
		}

		// Inside the guard now: NotifyGraphDeviceRemoved is server-only in its
		// entirety (LFPG_NetworkManager.c:1200-1208), so on the client this built a
		// whole manager to call a no-op.
		LFPG_NetworkManager nmGraph = LFPG_NetworkManager.GetExisting();
		if (nmGraph) nmGraph.NotifyGraphDeviceRemoved(deviceId);
		#endif

		// Outside the guard, creating factory kept: the registry is read on the
		// client too (LFPG_CableRenderer.c:1005, :1069), it is a bare map with no
		// side effects, and Unregister already refuses to drop an entry whose entity
		// is not the expected one (LFPG_DeviceRegistry.c:44).
		LFPG_DeviceRegistry.Get().Unregister(deviceId, device);
	}

    // ============================================
    // EEItemLocationChanged handler
    // ============================================
    // Two-layer movement detection:
    //   1. Primary: GROUND → non-GROUND transition (pickup)
    //   2. Secondary: distance-based (admin teleport, physics)
    //
    // Returns true if wires were cut (caller should handle
    // device-specific state like m_PoweredNet, m_SourceOn).
    static bool OnDeviceMoved(EntityAI device, string deviceId, notnull InventoryLocation oldLoc, notnull InventoryLocation newLoc)
	{
		#ifdef SERVER
		if (!device)
			return false;

		if (deviceId == "")
			return false;

		LFPG_NetworkManager nm = LFPG_NetworkManager.Get();

		bool wasGround = (oldLoc.GetType() == InventoryLocationType.GROUND);
		bool nowGround = (newLoc.GetType() == InventoryLocationType.GROUND);
		vector oldPos = oldLoc.GetPos();
		vector newPos = newLoc.GetPos();

		if (wasGround && !nowGround)
		{
			LFPG_Util.Warn("[DeviceLifecycle] Picked up (GROUND->" + newLoc.GetType().ToString() + ") id=" + deviceId);
			if (nm)
			{
				if (oldPos != vector.Zero)
					nm.CutAllWiresFromMovedDevice(device, oldPos, deviceId);
				else
					nm.CutAllWiresFromDevice(device, deviceId);
				nm.RequestGlobalSelfHeal(true);
			}
			return true;
		}

		if (oldPos == vector.Zero)
			return false;

		float distSq = LFPG_WorldUtil.DistSq(oldPos, newPos);
		if (distSq <= LFPG_MOVE_DETECT_THRESHOLD_SQ)
			return false;

		float dist = Math.Sqrt(distSq);
		LFPG_Util.Warn("[DeviceLifecycle] Moved " + dist.ToString() + "m id=" + deviceId);
		if (nm)
		{
			nm.CutAllWiresFromMovedDevice(device, oldPos, deviceId);
			nm.RequestGlobalSelfHeal(true);
		}
		return true;
		#else
		return false;
		#endif
	}

    // ============================================
    // SparkPlug validation (for generators)
    // ============================================
    // Centralized check: exists AND not RUINED.
    // A ruined sparkplug physically exists but cannot function.
    static bool IsSparkPlugValid(EntityAI device)
    {
        if (!device)
            return false;

        EntityAI sp = device.FindAttachmentBySlotName("SparkPlug");
        if (!sp)
            return false;

        if (sp.GetHealthLevel() >= GameConstants.STATE_RUINED)
            return false;

        return true;
    }
};
