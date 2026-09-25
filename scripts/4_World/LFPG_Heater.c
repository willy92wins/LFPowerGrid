// =========================================================
// LF_PowerGrid - Heater device
//
// LFPG_Heater_Kit:  Holdable, same model kit. Floor placement (mode 0).
// LFPG_Heater:      CONSUMER, 1 IN (input_1), 15 u/s, no wire store.
//                   Warms nearby players while powered AND switched on,
//                   through the same UniversalTemperatureSource path the
//                   furnace uses. Coils glow and a warm point light
//                   appears on the client while heating.
//
// Server-only bodies live in 5_Mission behind LFPG_ServerActions: World
// keeps the shell, Mission does the work. Client visuals stay here -
// they sit behind #ifndef SERVER and cost nothing in the server view.
// =========================================================

static const string LFPG_HEATER_RVMAT_OFF   = "\LFPowerGrid\data\heater\heater.rvmat";
static const string LFPG_HEATER_RVMAT_ON    = "\LFPowerGrid\data\heater\heater_on.rvmat";
static const float  LFPG_HEATER_CONSUMPTION = 15.0;

class LFPG_Heater_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_Heater";
    }
};

// ---------------------------------------------------------
// DEVICE - CONSUMER : LFPG_DeviceBase
// ---------------------------------------------------------
class LFPG_Heater : LFPG_DeviceBase
{
    // ---- SyncVars ----
    bool m_PoweredNet = false;
    bool m_HeaterOn   = false;

    // ---- Heat source (built server-side by the Mission implementation) ----
    ref UniversalTemperatureSource m_UTSource;
    ref UniversalTemperatureSourceSettings m_UTSSettings;

#ifndef SERVER
    // Client-side light effect (NOT ref -- engine object)
    protected ScriptedLightBase m_LFPG_Light;
    protected int m_PrevGlowState = -1;
#endif

    void LFPG_Heater()
    {
        string pIn = "input_1";
        string lIn = "Power Input";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, lIn);

        string varPowered = "m_PoweredNet";
        RegisterNetSyncVariableBool(varPowered);
        string varOn = "m_HeaterOn";
        RegisterNetSyncVariableBool(varOn);
    }

    // ---- Actions ----
    override void SetActions()
    {
        super.SetActions();
        AddAction(LFPG_ActionToggleHeater);
    }

    override void EEInit()
    {
        super.EEInit();
        if (m_LFPG_IsHologramProjection)
            return;

        LFPG_ServerActions.Get().Heater_EEInit(this);
    }

    // ---- Virtual interface ----
    override int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.CONSUMER;
    }

    override float LFPG_GetConsumption()
    {
        return LFPG_HEATER_CONSUMPTION;
    }

    override bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    override void LFPG_SetPowered(bool powered)
    {
        LFPG_ServerActions.Get().Heater_LFPG_SetPowered(this, powered);
    }

    // ---- Heat ----
    void LFPG_SetHeatActive(bool active)
    {
        LFPG_ServerActions.Get().Heater_LFPG_SetHeatActive(this, active);
    }

    // Heat is emitted only while powered and switched on; keep external
    // sources from driving this entity's own temperature meanwhile.
    override bool IsSelfAdjustingTemperature()
    {
        if (!m_PoweredNet)
            return false;

        if (!m_HeaterOn)
            return false;

        return true;
    }

    // ---- Lifecycle hooks ----
    override void LFPG_OnKilled()
    {
        LFPG_ServerActions.Get().Heater_LFPG_OnKilled(this);
        #ifndef SERVER
        LFPG_DestroyLight();
        #endif
    }

    override void LFPG_OnDeleted()
    {
        LFPG_ServerActions.Get().Heater_LFPG_OnDeleted(this);
        #ifndef SERVER
        LFPG_DestroyLight();
        #endif
    }

    override void LFPG_OnWiresCut()
    {
        LFPG_ServerActions.Get().Heater_LFPG_OnWiresCut(this);
    }

    // ---- Toggle (action callback) ----
    void LFPG_ToggleHeater()
    {
        LFPG_ServerActions.Get().Heater_LFPG_ToggleHeater(this);
    }

    bool LFPG_IsHeaterOn()
    {
        return m_HeaterOn;
    }

    // ---- Extra persistence: m_HeaterOn ----
    // Kept in World next to the field it writes: splitting a save format
    // across modules buys ~300 B of arena and costs the reviewer the one
    // place where the read and the write can be compared side by side.
    override void LFPG_OnStoreSaveExtra(ParamsWriteContext ctx)
    {
        ctx.Write(m_HeaterOn);
    }

    override bool LFPG_OnStoreLoadExtra(ParamsReadContext ctx, int ver)
    {
        if (!ctx.Read(m_HeaterOn))
        {
            string errOn = "[LFPG_Heater] OnStoreLoad: failed to read m_HeaterOn";
            LFPG_Util.Error(errOn);
            return false;
        }
        return true;
    }

    // ---- VarSync: coils + light ----
    override void LFPG_OnVarSync()
    {
        #ifndef SERVER
        LFPG_UpdateVisuals();
        #endif
    }

#ifndef SERVER
    protected void LFPG_UpdateVisuals()
    {
        int glowTarget = 0;
        if (m_PoweredNet && m_HeaterOn)
        {
            glowTarget = 1;
        }

        if (glowTarget == m_PrevGlowState)
            return;

        m_PrevGlowState = glowTarget;

        if (glowTarget == 1)
        {
            SetObjectMaterial(0, LFPG_HEATER_RVMAT_ON);
            LFPG_CreateLight();
        }
        else
        {
            SetObjectMaterial(0, LFPG_HEATER_RVMAT_OFF);
            LFPG_DestroyLight();
        }
    }
#endif

#ifndef SERVER
    protected void LFPG_CreateLight()
    {
        if (m_LFPG_Light)
            return;

        string memLight = "light";
        if (MemoryPointExists(memLight))
        {
            m_LFPG_Light = LFPG_HeaterEffect.Cast(ScriptedLightBase.CreateLightAtObjMemoryPoint(LFPG_HeaterEffect, this, memLight));
        }
        else
        {
            vector lightPos = GetPosition();
            m_LFPG_Light = LFPG_HeaterEffect.Cast(ScriptedLightBase.CreateLight(LFPG_HeaterEffect, lightPos));
            if (m_LFPG_Light)
            {
                m_LFPG_Light.AttachOnObject(this);
            }
        }

        if (m_LFPG_Light)
        {
            m_LFPG_Light.SetLifetime(1000000);
            m_LFPG_Light.SetEnabled(true);
        }
    }
#endif

#ifndef SERVER
    protected void LFPG_DestroyLight()
    {
        if (!m_LFPG_Light)
            return;

        m_LFPG_Light.FadeOut();
        m_LFPG_Light = null;
    }
#endif
};
