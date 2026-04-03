// =========================================================
// LF_PowerGrid - Switch V2 Remote / RF Toggle (v2.0.0 — Refactor)
//
// LFPG_SwitchV2Remote: PASSTHROUGH, 1 IN + 1 OUT, latching toggle, RF-capable.
//                       Extends LFPG_WireOwnerBase (Refactor v4.1).
//
// LED states (hiddenSelections[1] = "light_led"):
//   Green = m_PoweredNet && m_SwitchOn  | Red = m_PoweredNet && !m_SwitchOn  | Off = !m_PoweredNet
//
// Persistence: [base: DeviceId + ver + wireJSON] + m_SwitchOn
// =========================================================

static const string LFPG_SWV2R_RVMAT_OFF    = "\LFPowerGrid\data\switch_v2\data\led_off.rvmat";
static const string LFPG_SWV2R_RVMAT_GREEN   = "\LFPowerGrid\data\switch_v2\data\led_green.rvmat";
static const string LFPG_SWV2R_RVMAT_RED     = "\LFPowerGrid\data\switch_v2_remote\data\switch_v2_remote_red.rvmat";

class LFPG_SwitchV2Remote_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_SwitchV2Remote";
    }

    override int LFPG_GetPlacementModes()
    {
        return 1;
    }

    override float LFPG_GetWallSurfaceOffset()
    {
        return 0.04;
    }

    override float LFPG_GetWallYawOffset()
    {
        return 180.0;
    }
};

// ---------------------------------------------------------
// DEVICE: PASSTHROUGH, latching toggle, RF-capable
// ---------------------------------------------------------
class LFPG_SwitchV2Remote : LFPG_WireOwnerBase
{
    protected bool m_PoweredNet = false;
    protected bool m_SwitchOn   = false;
    protected bool m_Overloaded = false;

    void LFPG_SwitchV2Remote()
    {
        string varPowered  = "m_PoweredNet";
        string varSwitch   = "m_SwitchOn";
        string varOverload = "m_Overloaded";
        RegisterNetSyncVariableBool(varPowered);
        RegisterNetSyncVariableBool(varSwitch);
        RegisterNetSyncVariableBool(varOverload);

        string pIn  = "input_1";
        string pOut = "output_1";
        string lIn  = "Input 1";
        string lOut = "Output 1";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, lIn);
        LFPG_AddPort(pOut, LFPG_PortDir.OUT, lOut);
    }

    override void SetActions()
    {
        super.SetActions();
        AddAction(LFPG_ActionToggleSwitchV2Remote);
    }

    // ---- DeviceAPI ----
    override int LFPG_GetDeviceType() { return LFPG_DeviceType.PASSTHROUGH; }
    override bool LFPG_IsSource() { return true; }
    override bool LFPG_GetSourceOn() { return m_PoweredNet; }
    override bool LFPG_IsGateCapable() { return true; }
    override bool LFPG_IsGateOpen() { return m_SwitchOn; }
    override float LFPG_GetConsumption() { return 0.0; }
    override float LFPG_GetCapacity() { return LFPG_DEFAULT_PASSTHROUGH_CAPACITY; }
    override bool LFPG_IsPowered() { return m_PoweredNet; }
    override bool LFPG_GetOverloaded() { return m_Overloaded; }

    override void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;
        SetSynchDirty();

        string pwrMsg = "[LFPG_SwitchV2Remote] SetPowered(";
        pwrMsg = pwrMsg + powered.ToString();
        pwrMsg = pwrMsg + ") id=";
        pwrMsg = pwrMsg + m_DeviceId;
        LFPG_Util.Debug(pwrMsg);
        #endif
    }

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

    // ---- Device-specific ----
    bool LFPG_GetSwitchOn() { return m_SwitchOn; }

    void LFPG_ToggleSwitch()
    {
        #ifdef SERVER
        if (m_SwitchOn)
        {
            m_SwitchOn = false;
        }
        else
        {
            m_SwitchOn = true;
        }
        SetSynchDirty();

        string togMsg = "[LFPG_SwitchV2Remote] Toggle ";
        if (m_SwitchOn) { togMsg = togMsg + "ON"; }
        else { togMsg = togMsg + "OFF"; }
        togMsg = togMsg + " id=";
        togMsg = togMsg + m_DeviceId;
        LFPG_Util.Info(togMsg);

        LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
        #endif
    }

    // ---- RF ----
    override bool LFPG_IsRFCapable() { return true; }

    override bool LFPG_RemoteToggle()
    {
        #ifdef SERVER
        LFPG_ToggleSwitch();
        string rfMsg = "[LFPG_SwitchV2Remote] RF RemoteToggle id=";
        rfMsg = rfMsg + m_DeviceId;
        LFPG_Util.Info(rfMsg);
        #endif
        return true;
    }

    // ---- Port world position (p3d uses _0) ----
    override vector LFPG_GetPortWorldPos(string portName)
    {
        string memPoint;
        if (portName == "input_1")
        {
            memPoint = "port_input_0";
        }
        else if (portName == "output_1")
        {
            memPoint = "port_output_0";
        }
        else
        {
            memPoint = "port_";
            memPoint = memPoint + portName;
        }

        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

        vector offset = "0 0.02 0";
        if (portName == "input_1")
        {
            offset = "0 0.02 -0.025";
        }
        else if (portName == "output_1")
        {
            offset = "0 0.02 0.025";
        }
        return GetPosition() + offset;
    }

    // ---- Lifecycle hooks ----
    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        bool dirty = false;
        if (m_PoweredNet) { m_PoweredNet = false; dirty = true; }
        if (m_SwitchOn) { m_SwitchOn = false; dirty = true; }
        if (dirty) { SetSynchDirty(); }
        #endif
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        bool dirty = false;
        if (m_PoweredNet) { m_PoweredNet = false; dirty = true; }
        if (m_SwitchOn) { m_SwitchOn = false; dirty = true; }
        if (dirty) { SetSynchDirty(); }
        #endif
    }

    // ---- Visual sync ----
    override void LFPG_OnVarSyncDevice()
    {
        LFPG_UpdateVisuals();
    }

    protected void LFPG_UpdateVisuals()
    {
        #ifndef SERVER
        if (m_PoweredNet && m_SwitchOn)
        {
            SetObjectMaterial(1, LFPG_SWV2R_RVMAT_GREEN);
        }
        else if (m_PoweredNet)
        {
            SetObjectMaterial(1, LFPG_SWV2R_RVMAT_RED);
        }
        else
        {
            SetObjectMaterial(1, LFPG_SWV2R_RVMAT_OFF);
        }

        if (m_SwitchOn)
        {
            string animOn = "switch";
            SetAnimationPhase(animOn, 1.0);
        }
        else
        {
            string animOff = "switch";
            SetAnimationPhase(animOff, 0.0);
        }
        #endif
    }

    // ---- Persistence (latching) ----
    override void LFPG_OnStoreSaveDevice(ParamsWriteContext ctx)
    {
        ctx.Write(m_SwitchOn);
    }

    override bool LFPG_OnStoreLoadDevice(ParamsReadContext ctx, int deviceVer)
    {
        if (!ctx.Read(m_SwitchOn))
        {
            string err = "[LFPG_SwitchV2Remote] OnStoreLoad: failed to read m_SwitchOn";
            LFPG_Util.Error(err);
            return false;
        }
        return true;
    }
};
