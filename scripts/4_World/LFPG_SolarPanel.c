class LF_SolarPanel_Kit : LFPG_KitBaseDeployable
{
    override string LFPG_GetSpawnClassname()
    {
        return "LF_SolarPanel";
    }

    override vector GetDeployOrientationOffset()
    {
        return "0 -90 0";
    }
};

// ---------------------------------------------------------
// DEVICE - SOURCE : LFPG_WireOwnerBase
// ---------------------------------------------------------
class LF_SolarPanel : LFPG_WireOwnerBase
{
    // ---- Device-specific SyncVars ----
    protected bool  m_SourceOn   = false;
    protected float m_LoadRatio  = 0.0;
    protected bool  m_Overloaded = false;

    void LF_SolarPanel()
    {
        string pOut = "output_1";
        LFPG_AddPort(pOut, LFPG_PortDir.OUT, "Output");

        string varSrc = "m_SourceOn";
        RegisterNetSyncVariableBool(varSrc);
        string varLoad = "m_LoadRatio";
        RegisterNetSyncVariableFloat(varLoad, 0.0, 5.0, 2);
        string varOver = "m_Overloaded";
        RegisterNetSyncVariableBool(varOver);
    }

    // ---- Virtual interface ----
    override int LFPG_GetDeviceType() { return LFPG_DeviceType.SOURCE; }
    override bool LFPG_IsSource() { return true; }
    override bool LFPG_GetSourceOn() { return m_SourceOn; }
    override float LFPG_GetConsumption() { return 0.0; }
    override float LFPG_GetCapacity() { return 20.0; }

    override float LFPG_GetLoadRatio() { return m_LoadRatio; }

    override void LFPG_SetLoadRatio(float ratio)
    {
        #ifdef SERVER
        if (ratio < 0.0)
        {
            ratio = 0.0;
        }

        float diff = ratio - m_LoadRatio;
        if (diff < 0.0)
        {
            diff = -diff;
        }
        if (diff > 0.01)
        {
            m_LoadRatio = ratio;
            SetSynchDirty();
        }
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

    // SOURCE: SetPowered is no-op (driven by m_SourceOn / sun)
    override void LFPG_SetPowered(bool powered) {}
    override bool LFPG_IsPowered() { return m_SourceOn; }

    bool LFPG_GetSwitchState()
    {
        return m_SourceOn;
    }

    // ---- Sun state (called by NM centralized timer) ----
    void LFPG_UpdateSunState(bool hasSun)
    {
        #ifdef SERVER
        if (hasSun == m_SourceOn)
            return;

        m_SourceOn = hasSun;
        SetSynchDirty();

        if (m_DeviceId != "")
        {
            LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
        }

        string sunMsg = "[LF_SolarPanel] Sun state updated: m_SourceOn=" + m_SourceOn.ToString();
        sunMsg = sunMsg + " id=";
        sunMsg = sunMsg + m_DeviceId;
        LFPG_Util.Info(sunMsg);
        #endif
    }

    // ---- Lifecycle hooks ----
    override void LFPG_OnInitDevice()
    {
        #ifdef SERVER
        bool preState = m_SourceOn;
        bool cachedSun = LFPG_NetworkManager.Get().LFPG_GetCachedSunState();
        LFPG_UpdateSunState(cachedSun);

        // Persistence restore: if loaded m_SourceOn matched cached sun,
        // UpdateSunState was a no-op. Must propagate explicitly.
        if (preState == m_SourceOn && m_SourceOn && m_DeviceId != "")
        {
            LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
        }
        #endif
    }

    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        if (m_SourceOn)
        {
            m_SourceOn = false;
            SetSynchDirty();
        }
        #endif
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        if (m_SourceOn)
        {
            m_SourceOn = false;
            SetSynchDirty();
        }
        #endif
    }

    // ---- Extra persistence: m_SourceOn ----
    override void LFPG_OnStoreSaveDevice(ParamsWriteContext ctx)
    {
        ctx.Write(m_SourceOn);
    }

    override bool LFPG_OnStoreLoadDevice(ParamsReadContext ctx, int deviceVer)
    {
        if (!ctx.Read(m_SourceOn))
        {
            string errSrc = "[LF_SolarPanel] OnStoreLoad: failed to read m_SourceOn for " + m_DeviceId;
            LFPG_Util.Error(errSrc);
            return false;
        }
        return true;
    }
};

// ---------------------------------------------------------
// T2: Upgraded Solar Panel SOURCE (50 u/s during daylight)
// ---------------------------------------------------------
class LF_SolarPanel_T2 : LF_SolarPanel
{
    override float LFPG_GetCapacity()
    {
        return 50.0;
    }

    override bool CanReceiveAttachment(EntityAI attachment, int slotId)
    {
        return false;
    }
};
