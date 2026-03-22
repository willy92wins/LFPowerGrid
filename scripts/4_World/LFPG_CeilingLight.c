// =========================================================
// LF_PowerGrid - CeilingLight device (v4.0 Refactor)
//
// LF_CeilingLight_Kit:  Holdable (ceiling mount via HologramMod).
// LF_CeilingLight:      PASSTHROUGH, 1 IN + 1 OUT, 10 u/s self, cap 50.
//                       Light effect on/off via m_PoweredNet.
//
// v4.0: Migrated from Inventory_Base to LFPG_WireOwnerBase.
// =========================================================

static const string LFPG_CEILING_RVMAT_OFF = "\\LFPowerGrid\\data\\ceiling_light\\lf_ceiling_light.rvmat";
static const string LFPG_CEILING_RVMAT_ON  = "\\LFPowerGrid\\data\\ceiling_light\\lf_ceiling_light_on.rvmat";

// ---------------------------------------------------------
// KIT (unchanged)
// ---------------------------------------------------------
class LF_CeilingLight_Kit : Inventory_Base
{
    override bool IsDeployable() { return true; }
    override bool CanDisplayCargo() { return false; }
    override bool CanBePlaced(Man player, vector position) { return true; }
    override bool DoPlacingHeightCheck() { return false; }
    override string GetDeploySoundset() { return "placeBarbedWire_SoundSet"; }
    override string GetLoopDeploySoundset() { return ""; }

    override void SetActions()
    {
        super.SetActions();
        AddAction(ActionTogglePlaceObject);
        AddAction(LFPG_ActionPlaceGeneric);
    }

    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        string logPre = "[CeilingLight_Kit] OnPlacementComplete: pos=";
        logPre = logPre + finalPos.ToString();
        logPre = logPre + " ori=";
        logPre = logPre + finalOri.ToString();
        LFPG_Util.Info(logPre);

        EntityAI light = GetGame().CreateObjectEx("LF_CeilingLight", finalPos, ECE_CREATEPHYSICS);
        if (light)
        {
            light.SetPosition(finalPos);
            light.SetOrientation(finalOri);
            light.SetOrientation(light.GetOrientation());
            light.Update();
            string depMsg = "[CeilingLight_Kit] Deployed at ";
            depMsg = depMsg + finalPos.ToString();
            LFPG_Util.Info(depMsg);
            GetGame().ObjectDelete(this);
        }
        else
        {
            string errKit = "[CeilingLight_Kit] Failed! Kit preserved.";
            LFPG_Util.Error(errKit);
            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                pb.MessageStatus("[LFPG] Ceiling light placement failed.");
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// DEVICE - PASSTHROUGH : LFPG_WireOwnerBase
// ---------------------------------------------------------
class LF_CeilingLight : LFPG_WireOwnerBase
{
    protected bool m_PoweredNet = false;
    protected bool m_Overloaded = false;

    // Client-side light effect (NOT ref — engine object)
    protected ScriptedLightBase m_LFPG_Light;

    void LF_CeilingLight()
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
        string dbgMsg = "[LF_CeilingLight] SetPowered(";
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
            m_LFPG_Light = LFPG_CeilingLightEffect.Cast(ScriptedLightBase.CreateLightAtObjMemoryPoint(LFPG_CeilingLightEffect, this, memLight));
        }
        else
        {
            vector lightPos = GetPosition();
            lightPos[1] = lightPos[1] - 0.15;
            m_LFPG_Light = LFPG_CeilingLightEffect.Cast(ScriptedLightBase.CreateLight(LFPG_CeilingLightEffect, lightPos));
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
        SetObjectMaterial(0, LFPG_CEILING_RVMAT_ON);
    }

    protected void LFPG_SetRvmatOff()
    {
        SetObjectMaterial(0, LFPG_CEILING_RVMAT_OFF);
    }
};
