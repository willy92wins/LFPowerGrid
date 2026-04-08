// =========================================================
// LF_PowerGrid - Water Pump helper (static, shared T1/T2)
//
// v1.1.0: Sprint W1 — Core helper methods.
//   Centralizes logic that T1 and T2 share without forcing
//   inheritance. All methods are static — no instantiation.
//
// DetermineLiquidType:
//   Priority: filter > powered > tank liquid type.
//   - Has filter → LIQUID_CLEANWATER (always)
//   - Powered, no filter → LIQUID_RIVERWATER
//   - Not powered, no filter → tank liquid type (T2 only)
//   - Fallback → LIQUID_RIVERWATER
//
// ENFORCE SCRIPT NOTES:
//   - No ternary operators
//   - No ++ / --
//   - Explicit typing
//   - No foreach
// =========================================================

class LFPG_PumpHelper
{
    // Determine liquid type for any pump
    // powered:        whether the device has LFPG power
    // tankLiquidType: only relevant for T2 without power, pass 0 for T1
    static int DetermineLiquidType(EntityAI device, bool powered, int tankLiquidType)
    {
        bool hasFilter = HasActiveFilter(device);

        if (hasFilter)
        {
            return LIQUID_CLEANWATER;
        }

        // No filter: powered pump -> river water (unfiltered source)
        if (powered)
        {
            return LIQUID_RIVERWATER;
        }

        // Not powered, no filter -> use tank liquid type (T2 only)
        if (tankLiquidType > 0)
        {
            return tankLiquidType;
        }

        return LIQUID_RIVERWATER;
    }

    // Check if a device has an active NBC filter (GasMaskFilter slot with qty > 0)
    static bool HasActiveFilter(EntityAI device)
    {
        if (!device)
            return false;

        string slotName = "GasMaskFilter";
        EntityAI filter = device.FindAttachmentBySlotName(slotName);
        if (!filter)
            return false;

        int qty = filter.GetQuantity();
        if (qty > 0)
        {
            return true;
        }

        return false;
    }

    // Get tank level from a T2 pump (returns 0.0 for T1 or null)
    static float GetTankLevel(EntityAI device)
    {
        if (!device)
            return 0.0;

        LFPG_WaterPump_T2 t2 = LFPG_WaterPump_T2.Cast(device);
        if (!t2)
            return 0.0;

        return t2.LFPG_GetTankLevel();
    }

    // Decrement tank level with clamp to 0.0 (T2 only, server only)
    static void DecrementTank(EntityAI device, float amount)
    {
        #ifdef SERVER
        if (!device)
            return;

        LFPG_WaterPump_T2 t2 = LFPG_WaterPump_T2.Cast(device);
        if (!t2)
            return;

        float level = t2.LFPG_GetTankLevel();
        level = level - amount;
        if (level < 0.0)
        {
            level = 0.0;
        }
        t2.LFPG_SetTankLevel(level);
        #endif
    }
    // v1.1.0: Server-side power verification via graph edge data.
    // Guards against stale m_PoweredNet SyncVar during graph timing gaps.
    // Returns true if actual power is flowing to the device.
    // On client, falls back to m_PoweredNet SyncVar.
    static bool VerifyPowered(EntityAI device)
    {
        if (!device)
            return false;

        #ifdef SERVER
        string devId = LFPG_DeviceAPI.GetDeviceId(device);
        if (devId == "")
            return false;

        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (!nm)
            return false;

        LFPG_ElecGraph graph = nm.GetGraph();
        if (!graph)
            return false;

        return graph.VerifyPassthroughPowered(devId);
        #else
        // Client: trust SyncVar (no graph access)
        LFPG_WaterPump pump1 = LFPG_WaterPump.Cast(device);
        if (pump1)
            return pump1.LFPG_GetPoweredNet();

        LFPG_WaterPump_T2 pump2 = LFPG_WaterPump_T2.Cast(device);
        if (pump2)
            return pump2.LFPG_GetPoweredNet();

        return false;
        #endif
    }
};
