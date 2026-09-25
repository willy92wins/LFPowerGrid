// =========================================================
// LF_PowerGrid - Switch V2 / Latching Toggle (v2.0.0 — Refactor)
//
// LFPG_SwitchV2_Kit: Holdable, deployable (same-model pattern).
// LFPG_SwitchV2:     PASSTHROUGH, 1 IN + 1 OUT.
//                     Zero self-consumption. Latching toggle (stays ON/OFF).
//                     Extends LFPG_WireOwnerBase (Refactor v4.1).
//
// Model: switch_v2.p3d (lever switch with LED indicator)
//   Memory points: port_input_0, port_output_0
//   Animation: "switch" (rotation 0→-3.0 rad via model.cfg)
//   Hidden selection index 1: light_led
//
// LED states (hiddenSelections[1] = "light_led"):
//   Green = m_PoweredNet && m_SwitchOn
//   Red   = m_PoweredNet && !m_SwitchOn
//   Off   = !m_PoweredNet
//
// Persistence: [base: DeviceId + ver + wireJSON] + m_SwitchOn
// =========================================================

static const string LFPG_SWITCHV2_RVMAT_OFF    = "\LFPowerGrid\data\switch_v2\data\led_off.rvmat";
static const string LFPG_SWITCHV2_RVMAT_GREEN   = "\LFPowerGrid\data\switch_v2\data\led_green.rvmat";
static const string LFPG_SWITCHV2_RVMAT_RED     = "\LFPowerGrid\data\switch_v2\data\led_red.rvmat";

class LFPG_SwitchV2_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_SwitchV2";
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
        return 90.0;
    }

    override float LFPG_GetWallYawOffset()
    {
        return 180.0;
    }
};

// ---------------------------------------------------------
// DEVICE: PASSTHROUGH (1 IN + 1 OUT), latching toggle
// ---------------------------------------------------------
class LFPG_SwitchV2 : LFPG_SwitchDeviceBase
{
    // ---- Device-specific SyncVars ----

    // ============================================
    // Constructor
    // ============================================


    // ============================================
    // Actions
    // ============================================
    override void SetActions()
    {
        super.SetActions();
        AddAction(LFPG_ActionToggleSwitchV2);
    }

    // ============================================
    // DeviceAPI overrides
    // ============================================

    override void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;
        SetSynchDirty();

        if (LFPG_LOG_LEVEL >= 2)
        {
            string pwrMsg = "[LFPG_SwitchV2] SetPowered(";
            pwrMsg = pwrMsg + powered.ToString();
            pwrMsg = pwrMsg + ") id=";
            pwrMsg = pwrMsg + m_DeviceId;
            LFPG_Util.Debug(pwrMsg);
        }
        #endif
    }

    // ============================================
    // Device-specific methods
    // ============================================


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

        string togMsg = "[LFPG_SwitchV2] Toggle ";
        if (m_SwitchOn)
        {
            togMsg = togMsg + "ON";
        }
        else
        {
            togMsg = togMsg + "OFF";
        }
        togMsg = togMsg + " id=";
        togMsg = togMsg + m_DeviceId;
        LFPG_Util.Info(togMsg);

        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm) nm.RequestPropagate(m_DeviceId);
        #endif
    }

    // ============================================
    // Port world position (p3d uses _0 numbering)
    // ============================================
    override vector LFPG_GetPortWorldPos(string portName)
    {
        return LFPG_GetMappedPortWorldPos(portName, 0.02, 0.025, 0.02, true);
    }

    // ============================================
    // Hooks: lifecycle
    // ============================================

    // ============================================
    // Hook: visual sync (client)
    // ============================================
    override void LFPG_OnVarSyncDevice()
    {
        LFPG_UpdateVisuals();
    }

    protected void LFPG_UpdateVisuals()
    {
        #ifndef SERVER
        if (m_PoweredNet && m_SwitchOn)
        {
            SetObjectMaterial(1, LFPG_SWITCHV2_RVMAT_GREEN);
        }
        else if (m_PoweredNet)
        {
            SetObjectMaterial(1, LFPG_SWITCHV2_RVMAT_RED);
        }
        else
        {
            SetObjectMaterial(1, LFPG_SWITCHV2_RVMAT_OFF);
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

    // ============================================
    // Persistence hooks (m_SwitchOn is latching)
    // ============================================
    override void LFPG_OnStoreSaveDevice(ParamsWriteContext ctx)
    {
        ctx.Write(m_SwitchOn);
    }

    override bool LFPG_OnStoreLoadDevice(ParamsReadContext ctx, int deviceVer)
    {
        if (!ctx.Read(m_SwitchOn))
        {
            string err = "[LFPG_SwitchV2] OnStoreLoad: failed to read m_SwitchOn";
            LFPG_Util.Error(err);
            return false;
        }
        return true;
    }
};
