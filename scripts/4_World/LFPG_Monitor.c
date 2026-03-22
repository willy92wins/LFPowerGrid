// =========================================================
// LF_PowerGrid - Monitor device (v4.0 Refactor)
//
// LF_Monitor_Kit:  Holdable (same-model deployment).
// LF_Monitor:      PASSTHROUGH, 1 IN + 4 OUT (camera ports).
//                  Self-consumption 10 u/s, throughput cap 70 u/s.
//                  OUT ports restricted to LF_Camera only.
//
// v4.0: Migrated from Inventory_Base to LFPG_WireOwnerBase.
// =========================================================

static const string LFPG_MONITOR_RVMAT_OFF = "\\LFPowerGrid\\data\\cctv\\lf_monitor_off.rvmat";
static const string LFPG_MONITOR_RVMAT_ON  = "\\LFPowerGrid\\data\\cctv\\lf_monitor_on.rvmat";

class LF_Monitor_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LF_Monitor";
    }
};

// ---------------------------------------------------------
// DEVICE - PASSTHROUGH : LFPG_WireOwnerBase
// ---------------------------------------------------------
class LF_Monitor : LFPG_WireOwnerBase
{
    protected bool m_PoweredNet = false;
    protected bool m_Overloaded = false;

    void LF_Monitor()
    {
        string pIn = "input_1";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, "Power Input");
        string pO1 = "output_1";
        LFPG_AddPort(pO1, LFPG_PortDir.OUT, "Camera 1");
        string pO2 = "output_2";
        LFPG_AddPort(pO2, LFPG_PortDir.OUT, "Camera 2");
        string pO3 = "output_3";
        LFPG_AddPort(pO3, LFPG_PortDir.OUT, "Camera 3");
        string pO4 = "output_4";
        LFPG_AddPort(pO4, LFPG_PortDir.OUT, "Camera 4");

        string varP = "m_PoweredNet";
        RegisterNetSyncVariableBool(varP);
        string varO = "m_Overloaded";
        RegisterNetSyncVariableBool(varO);
    }

    override int LFPG_GetDeviceType() { return LFPG_DeviceType.PASSTHROUGH; }
    override float LFPG_GetConsumption() { return LFPG_MONITOR_CONSUMPTION; }
    override float LFPG_GetCapacity() { return LFPG_MONITOR_THROUGHPUT; }
    override bool LFPG_IsSource() { return true; }
    override bool LFPG_GetSourceOn() { return m_PoweredNet; }
    override bool LFPG_IsPowered() { return m_PoweredNet; }

    // ---- Actions (lost in v4.0 refactor) ----
    override void SetActions()
    {
        super.SetActions();
        AddAction(LFPG_ActionWatchMonitor);
    }

    override void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;
        m_PoweredNet = powered;
        SetSynchDirty();
        string dbgMsg = "[LF_Monitor] SetPowered(";
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

    // ---- Camera-only CanConnectTo ----
    override bool LFPG_CanConnectTo(Object other, string myPort, string otherPort)
    {
        if (!other)
            return false;

        if (!LFPG_HasPort(myPort, LFPG_PortDir.OUT))
            return false;

        string kCamera = "LF_Camera";
        if (!other.IsKindOf(kCamera))
            return false;

        EntityAI otherEntity = EntityAI.Cast(other);
        if (!otherEntity)
            return false;

        return LFPG_DeviceAPI.HasPort(other, otherPort, LFPG_PortDir.IN);
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

    // ---- VarSync: screen material swap (lost in v4.0 refactor) ----
    override void LFPG_OnVarSyncDevice()
    {
        #ifndef SERVER
        if (m_PoweredNet)
        {
            SetObjectMaterial(0, LFPG_MONITOR_RVMAT_ON);
        }
        else
        {
            SetObjectMaterial(0, LFPG_MONITOR_RVMAT_OFF);
        }
        #endif
    }
};
