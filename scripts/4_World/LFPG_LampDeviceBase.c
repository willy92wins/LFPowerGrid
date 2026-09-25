// =========================================================
// LF_PowerGrid - Lamp device base (script-only, no config class)
//
// Shared lifecycle for LFPG_CeilingLight and LFPG_WallLamp:
//   PASSTHROUGH, 1 IN + 1 OUT, 10 u/s self, cap 50.
//   Light effect + rvmat on/off via m_PoweredNet.
//
// Leaves keep their classnames and only supply what really
// differs: light effect type, fallback Y drop, rvmat pair.
// Ports and SyncVars are registered here, in the same order
// the leaves used before, so the net sync layout is unchanged.
// =========================================================

class LFPG_LampDeviceBase : LFPG_WireOwnerBase
{
    protected bool m_PoweredNet = false;
    protected bool m_Overloaded = false;

#ifndef SERVER
    // Client-side light effect (NOT ref — engine object)
    protected ScriptedLightBase m_LFPG_Light;
#endif

    void LFPG_LampDeviceBase()
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
        if (LFPG_LOG_LEVEL >= 2)
        {
            // GetType() is the leaf classname: same "[LFPG_CeilingLight]" / "[LFPG_WallLamp]" prefix as before.
            string dbgMsg = "[" + GetType() + "] SetPowered(";
            dbgMsg = dbgMsg + powered.ToString();
            dbgMsg = dbgMsg + ") id=";
            dbgMsg = dbgMsg + m_DeviceId;
            LFPG_Util.Debug(dbgMsg);
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
            SetObjectMaterial(0, LFPG_GetRvmatOn());
        }
        else
        {
            LFPG_DestroyLight();
            SetObjectMaterial(0, LFPG_GetRvmatOff());
        }
        #endif
    }

    // ---- Client-side leaf hooks + light effects ----
#ifndef SERVER
    // Effect classes only exist on the client (#ifndef SERVER in their files).
    protected typename LFPG_GetLightEffectType() { return ScriptedLightBase; }
    // Y drop applied only when the model has no "light" memory point.
    protected float LFPG_GetLightFallbackDropY() { return 0.0; }
    protected string LFPG_GetRvmatOn() { return ""; }
    protected string LFPG_GetRvmatOff() { return ""; }

    protected void LFPG_CreateLight()
    {
        if (m_LFPG_Light)
            return;

        typename effectType = LFPG_GetLightEffectType();
        string memLight = "light";
        if (MemoryPointExists(memLight))
        {
            m_LFPG_Light = ScriptedLightBase.CreateLightAtObjMemoryPoint(effectType, this, memLight);
        }
        else
        {
            vector lightPos = GetPosition();
            lightPos[1] = lightPos[1] - LFPG_GetLightFallbackDropY();
            m_LFPG_Light = ScriptedLightBase.CreateLight(effectType, lightPos);
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
#endif
};
