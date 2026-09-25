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

// Placement geometry is shared with the wired sibling: same skeleton, same
// LeverToggle animation in model.cfg. Inheriting it keeps pitch/yaw/surface in one
// place -- declaring them here once cost a 90 deg pitch that the remote kit silently
// dropped, so it spawned flat against walls.
class LFPG_SwitchV2Remote_Kit : LFPG_SwitchV2_Kit
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_SwitchV2Remote";
    }
};

// ---------------------------------------------------------
// DEVICE: PASSTHROUGH, latching toggle, RF-capable
// ---------------------------------------------------------
class LFPG_SwitchV2Remote : LFPG_SwitchDeviceBase
{

    override void SetActions()
    {
        super.SetActions();
        AddAction(LFPG_ActionToggleSwitchV2Remote);
    }

    // ---- DeviceAPI ----

    override void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;
        SetSynchDirty();

        if (LFPG_LOG_LEVEL >= 2)
        {
            string pwrMsg = "[LFPG_SwitchV2Remote] SetPowered(";
            pwrMsg = pwrMsg + powered.ToString();
            pwrMsg = pwrMsg + ") id=";
            pwrMsg = pwrMsg + m_DeviceId;
            LFPG_Util.Debug(pwrMsg);
        }
        #endif
    }

    // ---- Device-specific ----


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

        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm) nm.RequestPropagate(m_DeviceId);
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
        return LFPG_GetMappedPortWorldPos(portName, 0.02, 0.025, 0.02, true);
    }

    // ---- Lifecycle hooks ----

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
