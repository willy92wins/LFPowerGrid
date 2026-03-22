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

// ---------------------------------------------------------
// KIT (unchanged — stays Inventory_Base)
// ---------------------------------------------------------
class LF_Camera_Kit : Inventory_Base
{
    override bool IsDeployable()
    {
        return true;
    }

    override bool CanDisplayCargo()
    {
        return false;
    }

    override bool CanBePlaced(Man player, vector position)
    {
        return true;
    }

    override bool DoPlacingHeightCheck()
    {
        return false;
    }

    override string GetDeploySoundset()
    {
        return "placeBarbedWire_SoundSet";
    }

    override string GetLoopDeploySoundset()
    {
        return "";
    }

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

        string tLog = "[Camera_Kit] OnPlacementComplete: param=" + position.ToString();
        tLog = tLog + " kitPos=";
        tLog = tLog + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        EntityAI cam = GetGame().CreateObjectEx("LF_Camera", finalPos, ECE_CREATEPHYSICS);
        if (cam)
        {
            cam.SetPosition(finalPos);
            cam.SetOrientation(finalOri);
            cam.Update();

            string deployMsg = "[Camera_Kit] Deployed LF_Camera at " + finalPos.ToString();
            deployMsg = deployMsg + " ori=";
            deployMsg = deployMsg + finalOri.ToString();
            LFPG_Util.Info(deployMsg);

            GetGame().ObjectDelete(this);
        }
        else
        {
            string errKit = "[Camera_Kit] Failed to create LF_Camera! Kit preserved.";
            LFPG_Util.Error(errKit);
            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                pb.MessageStatus("[LFPG] Camera placement failed. Kit preserved.");
            }
        }
        #endif
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
