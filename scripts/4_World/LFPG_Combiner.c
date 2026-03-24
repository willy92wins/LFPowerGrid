// =========================================================
// LF_PowerGrid - Combiner device (v4.0 Refactor)
//
// LFPG_Combiner_Kit:  Holdable (same-model deployment).
// LFPG_Combiner:      PASSTHROUGH, 2 IN + 1 OUT, 0 u/s self-consumption.
//                   Capacity 500 u/s.
//
// v4.0: Migrated from Inventory_Base to LFPG_WireOwnerBase.
// =========================================================

// ---------------------------------------------------------
// KIT (unchanged)
// ---------------------------------------------------------

class LFPG_Combiner_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_Combiner";
    }

    override int LFPG_GetPlacementModes()
    {
        return 1;
    }

    override float LFPG_GetFloorPitchOffset()
    {
        return -90.0;
    }

    override float LFPG_GetFloorYOffset()
    {
        return 0.12;
    }

    override float LFPG_GetWallYawOffset()
    {
        return 180.0;
    }
};

// ---------------------------------------------------------
// DEVICE - PASSTHROUGH : LFPG_WireOwnerBase
// ---------------------------------------------------------
class LFPG_Combiner : LFPG_WireOwnerBase
{
    protected bool m_PoweredNet = false;
    protected bool m_Overloaded = false;

    void LFPG_Combiner()
    {
        string pI1 = "input_1";
        LFPG_AddPort(pI1, LFPG_PortDir.IN, "Input 1");
        string pI2 = "input_2";
        LFPG_AddPort(pI2, LFPG_PortDir.IN, "Input 2");
        string pO1 = "output_1";
        LFPG_AddPort(pO1, LFPG_PortDir.OUT, "Output 1");

        string varP = "m_PoweredNet";
        RegisterNetSyncVariableBool(varP);
        string varO = "m_Overloaded";
        RegisterNetSyncVariableBool(varO);
    }

    override int LFPG_GetDeviceType() { return LFPG_DeviceType.PASSTHROUGH; }
    override float LFPG_GetConsumption() { return 0.0; }
    override float LFPG_GetCapacity() { return 500.0; }
    override bool LFPG_IsSource() { return true; }
    override bool LFPG_GetSourceOn() { return m_PoweredNet; }
    override bool LFPG_IsPowered() { return m_PoweredNet; }

    override void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;
        m_PoweredNet = powered;
        SetSynchDirty();
        string dbgMsg = "[LFPG_Combiner] SetPowered(";
        dbgMsg = dbgMsg + powered.ToString();
        dbgMsg = dbgMsg + ") id=";
        dbgMsg = dbgMsg + m_DeviceId;
        LFPG_Util.Debug(dbgMsg);
        #endif
    }

    override bool LFPG_GetOverloaded() { return m_Overloaded; }

    override void LFPG_SetOverloaded(bool val)
    {
        #ifdef SERVER
        if (m_Overloaded != val)
        {
            m_Overloaded = val;
            SetSynchDirty();
        }
        #endif
    }

    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        if (m_PoweredNet) { m_PoweredNet = false; SetSynchDirty(); }
        #endif
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        if (m_PoweredNet) { m_PoweredNet = false; SetSynchDirty(); }
        #endif
    }
};
