// =========================================================
// LF_PowerGrid - Sprinkler device (v4.1 Registry Refactor)
//
// LFPG_Sprinkler_Kit:  Holdable, deployable (same-model pattern).
// LFPG_Sprinkler:      CONSUMER, 1 IN (input_0), 5 u/s, no wire store.
//
// v4.0: Migrated from Inventory_Base to LFPG_DeviceBase.
// v4.1: RegisterSprinkler/UnregisterSprinkler in NM.
//   Added LFPG_OnInit override (bug fix: was never registered).
// =========================================================

// ---------------------------------------------------------
// KIT (unchanged)
// ---------------------------------------------------------

class LFPG_Sprinkler_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_Sprinkler";
    }
};

// ---------------------------------------------------------
// DEVICE - CONSUMER : LFPG_DeviceBase
// ---------------------------------------------------------
class LFPG_Sprinkler : LFPG_DeviceBase
{
    // ---- Device-specific SyncVars ----
    protected bool m_PoweredNet      = false;
    protected bool m_SprinklerActive = false;

    // ---- Server-only: upstream tracking (set by NetworkManager tick) ----
    protected bool   m_HasWaterSource = false;
    protected string m_WaterSourceId  = "";
    protected string m_SourcePort     = "";

    // ---- Client: sound ----
    protected EffectSound m_LoopSound;

    void LFPG_Sprinkler()
    {
        string pIn = "input_0";
        string lIn = "Power Input";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, lIn);

        string varPowered = "m_PoweredNet";
        RegisterNetSyncVariableBool(varPowered);
        string varActive = "m_SprinklerActive";
        RegisterNetSyncVariableBool(varActive);
    }

    // ---- Actions (add sprinkler-specific) ----
    override void SetActions()
    {
        super.SetActions();
        AddAction(LFPG_ActionCheckSprinkler);
    }

    // ---- Virtual interface ----
    override int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.CONSUMER;
    }

    override float LFPG_GetConsumption()
    {
        return LFPG_SPRINKLER_CONSUMPTION;
    }

    override bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    override void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;
        SetSynchDirty();

        string msg = "[LFPG_Sprinkler] SetPowered(";
        msg = msg + powered.ToString();
        msg = msg + ") id=";
        msg = msg + m_DeviceId;
        LFPG_Util.Debug(msg);
        #endif
    }

    // ---- Sprinkler-specific state accessors (NM tick) ----
    bool LFPG_GetSprinklerActive()
    {
        return m_SprinklerActive;
    }

    void LFPG_SetSprinklerActive(bool active)
    {
        #ifdef SERVER
        if (m_SprinklerActive == active)
            return;

        m_SprinklerActive = active;
        SetSynchDirty();

        string msg = "[LFPG_Sprinkler] SetSprinklerActive(";
        msg = msg + active.ToString();
        msg = msg + ") id=";
        msg = msg + m_DeviceId;
        LFPG_Util.Debug(msg);
        #endif
    }

    bool LFPG_GetHasWaterSource()
    {
        return m_HasWaterSource;
    }

    void LFPG_SetHasWaterSource(bool has)
    {
        #ifdef SERVER
        m_HasWaterSource = has;
        #endif
    }

    string LFPG_GetWaterSourceId()
    {
        return m_WaterSourceId;
    }

    void LFPG_SetWaterSourceId(string id)
    {
        #ifdef SERVER
        m_WaterSourceId = id;
        #endif
    }

    string LFPG_GetSourcePort()
    {
        return m_SourcePort;
    }

    void LFPG_SetSourcePort(string port)
    {
        #ifdef SERVER
        m_SourcePort = port;
        #endif
    }

    bool LFPG_GetPoweredNet()
    {
        return m_PoweredNet;
    }

    // ---- Lifecycle hooks ----
    // v4.1: LFPG_OnInit called from DeviceBase.EEInit after TryRegister.
    // Sprinkler extends DeviceBase (not WireOwnerBase), so LFPG_OnInit
    // is the correct hook (not LFPG_OnInitDevice which is WireOwnerBase chain).
    override void LFPG_OnInit()
    {
        LFPG_NetworkManager.Get().RegisterSprinkler(this);
    }

    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterSprinkler(this);
        bool dirty = false;
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            dirty = true;
        }
        if (m_SprinklerActive)
        {
            m_SprinklerActive = false;
            dirty = true;
        }
        if (dirty)
        {
            SetSynchDirty();
        }
        // v5.1: Notify parent pump to rescan (skip this dying sprinkler)
        if (m_WaterSourceId != "")
        {
            LFPG_NetworkManager.Get().LFPG_RefreshPumpSprinklerLink(m_WaterSourceId, m_DeviceId);
        }
        #endif

        #ifndef SERVER
        LFPG_CleanupClientFX();
        #endif
    }

    override void LFPG_OnDeleted()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterSprinkler(this);
        // v5.1: Notify parent pump to rescan (skip this dying sprinkler)
        if (m_WaterSourceId != "")
        {
            LFPG_NetworkManager.Get().LFPG_RefreshPumpSprinklerLink(m_WaterSourceId, m_DeviceId);
        }
        #endif

        #ifndef SERVER
        LFPG_CleanupClientFX();
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

    // ---- VarSync: sound toggle ----
    override void LFPG_OnVarSync()
    {
        #ifndef SERVER
        if (m_SprinklerActive && !m_LoopSound)
        {
            string soundSet = LFPG_SPRINKLER_LOOP_SOUNDSET;
            m_LoopSound = SEffectManager.PlaySound(soundSet, GetPosition());
            if (m_LoopSound)
            {
                m_LoopSound.SetAutodestroy(false);
            }
        }
        if (!m_SprinklerActive && m_LoopSound)
        {
            m_LoopSound.SoundStop();
            m_LoopSound = null;
        }
        // TODO S4: particle toggle
        #endif
    }

    // ---- Client FX cleanup ----
    protected void LFPG_CleanupClientFX()
    {
        if (m_LoopSound)
        {
            m_LoopSound.SoundStop();
            m_LoopSound = null;
        }
    }

    // ---- No extra persistence (CONSUMER: ids + deviceVer only) ----
};
