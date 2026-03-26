// =========================================================
// LF_PowerGrid - Switch V1 Remote / RF Toggle (v2.0.0 — Refactor)
//
// LFPG_SwitchRemote_Kit: Holdable, deployable.
// LFPG_SwitchRemote:     PASSTHROUGH, 1 IN + 1 OUT.
//                      Momentary pulse (ON for LFPG_BUTTON_PULSE_MS, then OFF).
//                      RF-CAPABLE: toggleable via LFPG_Intercom.
//                      Extends LFPG_WireOwnerBase (Refactor v4.1).
//
// LED states (hiddenSelections[2] = "light_led"):
//   Green = m_PoweredNet && m_SwitchOn  | Red = m_PoweredNet && !m_SwitchOn  | Off = !m_PoweredNet
//
// Persistence: [base: DeviceId + ver + wireJSON] — m_SwitchOn NOT persisted (momentary)
// =========================================================

static const string LFPG_SWREMOTE_RVMAT_OFF    = "\\LFPowerGrid\\data\\switch_v1_remote\\data\\led_off.rvmat";
static const string LFPG_SWREMOTE_RVMAT_GREEN   = "\\LFPowerGrid\\data\\switch_v1_remote\\data\\led_green.rvmat";
static const string LFPG_SWREMOTE_RVMAT_RED     = "\\LFPowerGrid\\data\\switch_v1_remote\\data\\switch_v1_remote_red.rvmat";

class LFPG_SwitchRemote_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_SwitchRemote";
    }

    override int LFPG_GetPlacementModes()
    {
        return 1;
    }

    override float LFPG_GetWallSurfaceOffset()
    {
        return 0.04;
    }

    override float LFPG_GetWallPitchOffset()
    {
        return 0.0;
    }

    override float LFPG_GetWallYawOffset()
    {
        return 180.0;
    }
};

// ---------------------------------------------------------
// DEVICE: PASSTHROUGH, momentary pulse, RF-capable
// ---------------------------------------------------------
class LFPG_SwitchRemote : LFPG_WireOwnerBase
{
    protected bool m_PoweredNet = false;
    protected bool m_SwitchOn   = false;
    protected bool m_Overloaded = false;

    void LFPG_SwitchRemote()
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
        AddAction(LFPG_ActionToggleSwitchRemote);
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

        if (!powered && m_SwitchOn)
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_PulseOff);
            m_SwitchOn = false;
        }

        SetSynchDirty();

        string pwrMsg = "[LFPG_SwitchRemote] SetPowered(";
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
            return;
        }
        m_SwitchOn = true;
        SetSynchDirty();

        string togMsg = "[LFPG_SwitchRemote] Pulse ON id=";
        togMsg = togMsg + m_DeviceId;
        LFPG_Util.Info(togMsg);

        LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_PulseOff, LFPG_BUTTON_PULSE_MS, false);
        #endif
    }

    void LFPG_PulseOff()
    {
        #ifdef SERVER
        if (!m_SwitchOn)
            return;
        m_SwitchOn = false;
        SetSynchDirty();

        string offMsg = "[LFPG_SwitchRemote] Pulse OFF id=";
        offMsg = offMsg + m_DeviceId;
        LFPG_Util.Info(offMsg);

        LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
        #endif
    }

    // ---- RF ----
    bool LFPG_IsRFCapable() { return true; }

    bool LFPG_RemoteToggle()
    {
        #ifdef SERVER
        LFPG_ToggleSwitch();
        string rfMsg = "[LFPG_SwitchRemote] RF RemoteToggle id=";
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
        return ModelToWorld(offset);
    }

    // ---- Lifecycle hooks ----
    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        if (GetGame())
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_PulseOff);
        }
        bool dirty = false;
        if (m_PoweredNet) { m_PoweredNet = false; dirty = true; }
        if (m_SwitchOn) { m_SwitchOn = false; dirty = true; }
        if (dirty) { SetSynchDirty(); }
        #endif
    }

    override void LFPG_OnDeleted()
    {
        if (GetGame())
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_PulseOff);
        }
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        if (GetGame())
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_PulseOff);
        }
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
            SetObjectMaterial(2, LFPG_SWREMOTE_RVMAT_GREEN);
        }
        else if (m_PoweredNet)
        {
            SetObjectMaterial(2, LFPG_SWREMOTE_RVMAT_RED);
        }
        else
        {
            SetObjectMaterial(2, LFPG_SWREMOTE_RVMAT_OFF);
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

    // ---- No persist extras (momentary, m_SwitchOn defaults false) ----
};
