// =========================================================
// LF_PowerGrid - Camera device (v4.0 Refactor)
//
// LFPG_Camera_Kit:  Holdable, deployable (same-model pattern).
// LFPG_Camera:      CONSUMER, 1 IN (input_1), 15 u/s, no wire store.
//
// v4.0: Migrated from Inventory_Base to LFPG_DeviceBase.
//   All boilerplate (SyncVars DeviceId, lifecycle, persistence,
//   guards, CompEM block, port world pos) now in DeviceBase.
//   Camera only declares: ports, m_PoweredNet, consumption, visuals.
// =========================================================

static const string LFPG_CAMERA_RVMAT_OFF = "\LFPowerGrid\data\cctv\lf_camera_led_off.rvmat";
static const string LFPG_CAMERA_RVMAT_ON  = "\LFPowerGrid\data\cctv\lf_camera_led_on.rvmat";

class LFPG_Camera_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_Camera";
    }

    override int LFPG_GetPlacementModes()
    {
        return 1;
    }

    override float LFPG_GetWallSurfaceOffset()
    {
        return 0.30;
    }
};

// ---------------------------------------------------------
// DEVICE - CONSUMER : LFPG_DeviceBase
// ---------------------------------------------------------
class LFPG_Camera : LFPG_DeviceBase
{
    // ---- Device-specific SyncVars ----
    protected bool m_PoweredNet = false;
    protected float m_PTZYaw = 0.0;
    protected float m_PTZPitch = 0.0;
    protected static bool s_InvalidPTZLoadWarned = false;

    static bool LFPG_IsInvalidPTZValue(float value)
    {
        float witness = value - value;
        if (value != value || witness != witness)
            return true;
        return false;
    }

    void LFPG_Camera()
    {
        string pIn = "input_1";
        string lIn = "Power Input";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, lIn);

        string varPowered = "m_PoweredNet";
        string varPTZYaw = "m_PTZYaw";
        string varPTZPitch = "m_PTZPitch";
        RegisterNetSyncVariableBool(varPowered);
        RegisterNetSyncVariableFloat(varPTZYaw, -LFPG_CCTV_YAW_LIMIT, LFPG_CCTV_YAW_LIMIT, 8);
        RegisterNetSyncVariableFloat(varPTZPitch, -LFPG_CCTV_PITCH_LIMIT, LFPG_CCTV_PITCH_LIMIT, 7);
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

    float LFPG_GetPTZYaw()
    {
        return m_PTZYaw;
    }

    float LFPG_GetPTZPitch()
    {
        return m_PTZPitch;
    }

    void LFPG_SetPTZ(float yaw, float pitch)
    {
        #ifdef SERVER
        if (yaw > LFPG_CCTV_YAW_LIMIT)
            yaw = LFPG_CCTV_YAW_LIMIT;
        if (yaw < -LFPG_CCTV_YAW_LIMIT)
            yaw = -LFPG_CCTV_YAW_LIMIT;
        if (pitch > LFPG_CCTV_PITCH_LIMIT)
            pitch = LFPG_CCTV_PITCH_LIMIT;
        if (pitch < -LFPG_CCTV_PITCH_LIMIT)
            pitch = -LFPG_CCTV_PITCH_LIMIT;

        if (m_PTZYaw == yaw && m_PTZPitch == pitch)
            return;

        m_PTZYaw = yaw;
        m_PTZPitch = pitch;
        SetSynchDirty();
        #endif
    }

    override int LFPG_GetDevicePersistVersion()
    {
        return 3;
    }

    override void LFPG_OnStoreSaveExtra(ParamsWriteContext ctx)
    {
        ctx.Write(m_PTZYaw);
        ctx.Write(m_PTZPitch);
    }

    override bool LFPG_OnStoreLoadExtra(ParamsReadContext ctx, int ver)
    {
        // Camera PTZ was introduced in device persistence v3. Existing v1/v2
        // camera saves have no extra payload and remain centered.
        if (ver < 3)
        {
            m_PTZYaw = 0.0;
            m_PTZPitch = 0.0;
            return true;
        }

        float loadedYaw = 0.0;
        float loadedPitch = 0.0;
        if (!ctx.Read(loadedYaw))
        {
            LFPG_Util.Error("[LFPG_Camera] OnStoreLoad failed: m_PTZYaw");
            return false;
        }
        if (!ctx.Read(loadedPitch))
        {
            LFPG_Util.Error("[LFPG_Camera] OnStoreLoad failed: m_PTZPitch");
            return false;
        }

        if (LFPG_IsInvalidPTZValue(loadedYaw) || LFPG_IsInvalidPTZValue(loadedPitch))
        {
            m_PTZYaw = 0.0;
            m_PTZPitch = 0.0;
            if (!s_InvalidPTZLoadWarned)
            {
                s_InvalidPTZLoadWarned = true;
                LFPG_Util.Warn("[LFPG_Camera] Non-finite persisted PTZ reset to center");
            }
            return true;
        }

        if (loadedYaw > LFPG_CCTV_YAW_LIMIT)
            loadedYaw = LFPG_CCTV_YAW_LIMIT;
        if (loadedYaw < -LFPG_CCTV_YAW_LIMIT)
            loadedYaw = -LFPG_CCTV_YAW_LIMIT;
        if (loadedPitch > LFPG_CCTV_PITCH_LIMIT)
            loadedPitch = LFPG_CCTV_PITCH_LIMIT;
        if (loadedPitch < -LFPG_CCTV_PITCH_LIMIT)
            loadedPitch = -LFPG_CCTV_PITCH_LIMIT;

        m_PTZYaw = loadedYaw;
        m_PTZPitch = loadedPitch;
        return true;
    }

    override void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;
        SetSynchDirty();

        string msg = "[LFPG_Camera] SetPowered(";
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
