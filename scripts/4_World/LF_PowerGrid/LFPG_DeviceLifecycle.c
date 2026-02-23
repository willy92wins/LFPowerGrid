// =========================================================
// LF_PowerGrid - Device Lifecycle Helpers (v0.7.27)
//
// Centralized static helpers for LFPG device lifecycle events.
// Replaces duplicated EEKilled/EEDelete/EEItemLocationChanged
// logic across LF_TestGenerator, LF_TestLamp, and LF_Splitter.
//
// WHY STATIC HELPER INSTEAD OF BASE CLASS:
//   LF_TestGenerator : PowerGenerator
//   LF_TestLamp      : Spotlight
//   LF_Splitter      : Inventory_Base
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
        LFPG_NetworkManager.Get().CutAllWiresFromDevice(device);
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
        if (device && deviceId != "")
        {
            LFPG_NetworkManager.Get().CutAllWiresFromDevice(device);
        }
        #endif

        // These calls are safe on both client and server
        LFPG_NetworkManager.Get().NotifyGraphDeviceRemoved(deviceId);
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

        // --- Primary: inventory type transition ---
        // GROUND → anything else = picked up
        bool wasGround = (oldLoc.GetType() == InventoryLocationType.GROUND);
        bool nowGround = (newLoc.GetType() == InventoryLocationType.GROUND);

        if (wasGround && !nowGround)
        {
            LFPG_Util.Warn("[DeviceLifecycle] Picked up (GROUND->" + newLoc.GetType().ToString() + ") id=" + deviceId);
            LFPG_NetworkManager.Get().CutAllWiresFromDevice(device);
            return true;
        }

        // --- Secondary: distance-based for GROUND→GROUND moves ---
        // Catches admin teleport, physics push, building destruction.
        vector oldPos = oldLoc.GetPos();
        vector newPos = newLoc.GetPos();

        if (oldPos == vector.Zero)
            return false;

        float dist = vector.Distance(oldPos, newPos);
        // Threshold 0.1m — physics bumps can be 0.2-0.4m
        if (dist < 0.1)
            return false;

        LFPG_Util.Warn("[DeviceLifecycle] Moved " + dist.ToString() + "m id=" + deviceId);
        LFPG_NetworkManager.Get().CutAllWiresFromDevice(device);
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
