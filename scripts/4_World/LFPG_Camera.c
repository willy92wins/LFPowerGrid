// =========================================================
// LF_PowerGrid - Camera device (v4.0 Refactor)
//
// LF_Camera_Kit:  Holdable, deployable (same-model pattern).
// LF_Camera:      CONSUMER, 1 IN (input_1), 15 u/s, no wire store.
//
// v4.0: Migrated from Inventory_Base to LFPG_DeviceBase.
//   All boilerplate (SyncVars DeviceId, lifecycle, persistence,
//   guards, CompEM block, port world pos) now in DeviceBase.
//   Camera only declares: ports, m_PoweredNet, consumption, visuals.
// =========================================================

static const string LFPG_CAMERA_RVMAT_OFF = "\\LFPowerGrid\\data\\cctv\\lf_camera_led_off.rvmat";
static const string LFPG_CAMERA_RVMAT_ON  = "\\LFPowerGrid\\data\\cctv\\lf_camera_led_on.rvmat";

class LF_Camera_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LF_Camera";
    }

    override int LFPG_GetPlacementModes()
    {
        return 1;
    }

    override float LFPG_GetWallSurfaceOffset()
    {
        return 0.22;
    }
};

// ---------------------------------------------------------
// DEVICE - CONSUMER : LFPG_DeviceBase
// ---------------------------------------------------------
class LF_Camera : LFPG_DeviceBase
{
    // ---- Device-specific SyncVars ----
    protected bool m_PoweredNet = false;

    void LF_Camera()
    {
        string pIn = "input_1";
        string lIn = "Power Input";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, lIn);

        string varPowered = "m_PoweredNet";
        RegisterNetSyncVariableBool(varPowered);
    }

    // ---- Virtual interface ----
    override int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.CAMERA;
    }

    override float LFPG_GetConsumption()
    {
        return 15.0;
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

        string msg = "[LF_Camera] SetPowered(";
        msg = msg + powered.ToString();
        msg = msg + ") id=";
        msg = msg + m_DeviceId;
        LFPG_Util.Debug(msg);
        #endif
    }

    // ---- Lifecycle hooks ----
    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        #endif

        #ifndef SERVER
        LFPG_CameraViewport.SafeAbort();
        #endif
    }

    override void LFPG_OnDeleted()
    {
        #ifndef SERVER
        LFPG_CameraViewport.SafeAbort();
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

    // ---- VarSync: LED visual ----
    override void LFPG_OnVarSync()
    {
        #ifndef SERVER
        if (m_PoweredNet)
        {
            SetObjectMaterial(0, LFPG_CAMERA_RVMAT_ON);
        }
        else
        {
            SetObjectMaterial(0, LFPG_CAMERA_RVMAT_OFF);
        }
        #endif
    }
};
