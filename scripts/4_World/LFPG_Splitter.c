// =========================================================
// LF_PowerGrid - Splitter device (v4.0 Refactor)
//
// LFPG_Splitter_Kit:  Holdable (same-model deployment).
// LFPG_Splitter:      PASSTHROUGH, 1 IN + 3 OUT, 0 u/s self-consumption.
//
// v4.0: Migrated from Inventory_Base to LFPG_WireOwnerBase.
//   Wire store, wire API, persistence wireJSON, CanConnectTo — all in base.
// =========================================================

// ---------------------------------------------------------
// KIT (unchanged)
// ---------------------------------------------------------

class LFPG_Splitter_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_Splitter";
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
class LFPG_Splitter : LFPG_WireOwnerBase
{
    protected bool m_PoweredNet = false;
    protected bool m_Overloaded = false;

    void LFPG_Splitter()
    {
        string pIn = "input_1";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, "Input");
        string pO1 = "output_1";
        LFPG_AddPort(pO1, LFPG_PortDir.OUT, "Output 1");
        string pO2 = "output_2";
        LFPG_AddPort(pO2, LFPG_PortDir.OUT, "Output 2");
        string pO3 = "output_3";
        LFPG_AddPort(pO3, LFPG_PortDir.OUT, "Output 3");

        string varP = "m_PoweredNet";
        RegisterNetSyncVariableBool(varP);
        string varO = "m_Overloaded";
        RegisterNetSyncVariableBool(varO);
    }

    override int LFPG_GetDeviceType() { return LFPG_DeviceType.PASSTHROUGH; }
    override float LFPG_GetConsumption() { return 0.0; }
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
        string msg = "[LFPG_Splitter] SetPowered(";
        msg = msg + powered.ToString();
        msg = msg + ") id=";
        msg = msg + m_DeviceId;
        LFPG_Util.Debug(msg);
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
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        #endif
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        #endif
    }

    // No extra persistence (PASSTHROUGH: ids + deviceVer + wireJSON from base)
};
