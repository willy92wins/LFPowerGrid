// =========================================================
// LF_PowerGrid - WallLamp device
//
// LFPG_WallLamp_Kit:  Holdable. Wall + floor placement (mode 1).
// LFPG_WallLamp:      PASSTHROUGH, 1 IN + 1 OUT, 10 u/s self, cap 50.
//                     Light effect on/off via m_PoweredNet.
//
// Same electrical contract as LFPG_CeilingLight, wall-mount placement.
// =========================================================

static const string LFPG_WALLLAMP_RVMAT_OFF = "\LFPowerGrid\data\wall_lamp\lf_wall_lamp.rvmat";
static const string LFPG_WALLLAMP_RVMAT_ON  = "\LFPowerGrid\data\wall_lamp\lf_wall_lamp_on.rvmat";

class LFPG_WallLamp_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_WallLamp";
    }

    override int LFPG_GetPlacementModes()
    {
        // 1 = floor + wall (no ceiling). Wall is the primary mount.
        return 1;
    }
};

// ---------------------------------------------------------
// DEVICE - PASSTHROUGH : LFPG_WireOwnerBase
// ---------------------------------------------------------
class LFPG_WallLamp : LFPG_WireOwnerBase
{
    protected bool m_PoweredNet = false;
    protected bool m_Overloaded = false;

    // Client-side light effect (NOT ref -- engine object)
    protected ScriptedLightBase m_LFPG_Light;

    void LFPG_WallLamp()
    {
        string pIn = "input_1";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, "Input");
        string pOut = "output_1";
        LFPG_AddPort(pOut, LFPG_PortDir.OUT, "Output");

        string varP = "m_PoweredNet";
        RegisterNetSyncVariableBool(varP);
        string varO = "m_Overloaded";
        RegisterNetSyncVariableBool(varO);
    }

    override int LFPG_GetDeviceType() { return LFPG_DeviceType.PASSTHROUGH; }
    override float LFPG_GetConsumption() { return 10.0; }
    override float LFPG_GetCapacity() { return 50.0; }
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
        string dbgMsg = "[LFPG_WallLamp] SetPowered(";
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

    // ---- Lifecycle hooks ----
    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        if (m_PoweredNet) { m_PoweredNet = false; SetSynchDirty(); }
        #endif
        #ifndef SERVER
        LFPG_DestroyLight();
        #endif
    }

    override void LFPG_OnDeleted()
    {
        #ifndef SERVER
        LFPG_DestroyLight();
        #endif
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        if (m_PoweredNet) { m_PoweredNet = false; SetSynchDirty(); }
        #endif
    }

    // ---- VarSync: light + rvmat (WireOwnerBase hook) ----
    override void LFPG_OnVarSyncDevice()
    {
        #ifndef SERVER
        if (m_PoweredNet)
        {
            LFPG_CreateLight();
            LFPG_SetRvmatOn();
        }
        else
        {
            LFPG_DestroyLight();
            LFPG_SetRvmatOff();
        }
        #endif
    }

    // ---- Client-side light effects ----
    protected void LFPG_CreateLight()
    {
        if (m_LFPG_Light)
            return;

        string memLight = "light";
        if (MemoryPointExists(memLight))
        {
            m_LFPG_Light = LFPG_WallLampEffect.Cast(ScriptedLightBase.CreateLightAtObjMemoryPoint(LFPG_WallLampEffect, this, memLight));
        }
        else
        {
            vector lightPos = GetPosition();
            m_LFPG_Light = LFPG_WallLampEffect.Cast(ScriptedLightBase.CreateLight(LFPG_WallLampEffect, lightPos));
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

    protected void LFPG_DestroyLight()
    {
        if (!m_LFPG_Light)
            return;

        m_LFPG_Light.FadeOut();
        m_LFPG_Light = null;
    }

    protected void LFPG_SetRvmatOn()
    {
        SetObjectMaterial(0, LFPG_WALLLAMP_RVMAT_ON);
    }

    protected void LFPG_SetRvmatOff()
    {
        SetObjectMaterial(0, LFPG_WALLLAMP_RVMAT_OFF);
    }
};
