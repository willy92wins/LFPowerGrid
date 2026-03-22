// =========================================================
// LF_PowerGrid - Searchlight device (v4.0 Refactor)
//
// LF_Searchlight_Kit:  Holdable, deployable (same-model pattern).
// LF_Searchlight:      CONSUMER, 1 IN (input_0), 25 u/s, no wire store.
//
// v4.0: Migrated from Inventory_Base to LFPG_DeviceBase.
//   All boilerplate (SyncVars DeviceId, lifecycle, persistence,
//   guards, CompEM block, port world pos) now in DeviceBase.
//   Searchlight declares: ports, SyncVars, lights, operator, aim.
//
// Memory points (LOD Memory in p3d):
//   port_input_0      — cable anchor (base of tripod)
//   port_input_0_dir  — cable direction
//   light_main        — lens center (light/beam origin)
//   light_main_dir    — beam direction (1m forward)
//   light_main_axis   — pitch rotation axis (2 points, X-aligned)
//
// Rotation model:
//   YAW:   entity SetOrientation (whole model rotates)
//   PITCH: SetAnimationPhase("light_main", phase) — head tilts
// =========================================================

static const string LFPG_SL_RVMAT_OFF = "\\LFPowerGrid\\data\\searchlight\\lf_searchlight_lens_off.rvmat";
static const string LFPG_SL_RVMAT_ON  = "\\LFPowerGrid\\data\\searchlight\\lf_searchlight_lens_on.rvmat";

static const float LFPG_SL_SPLASH_Y_OFFSET = 0.05;

// ---------------------------------------------------------
// KIT — same-model deploy pattern
// ---------------------------------------------------------
class LF_Searchlight_Kit : Inventory_Base
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

        string tLog = "[Searchlight_Kit] OnPlacementComplete: param=" + position.ToString();
        tLog = tLog + " kitPos=";
        tLog = tLog + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        string className = "LF_Searchlight";
        EntityAI sl = GetGame().CreateObjectEx(className, finalPos, ECE_CREATEPHYSICS);
        if (sl)
        {
            sl.SetPosition(finalPos);
            sl.SetOrientation(finalOri);
            sl.Update();

            string deployMsg = "[Searchlight_Kit] Deployed LF_Searchlight at ";
            deployMsg = deployMsg + finalPos.ToString();
            deployMsg = deployMsg + " ori=";
            deployMsg = deployMsg + finalOri.ToString();
            LFPG_Util.Info(deployMsg);

            GetGame().ObjectDelete(this);
        }
        else
        {
            string errKit = "[Searchlight_Kit] Failed to create LF_Searchlight! Kit preserved.";
            LFPG_Util.Error(errKit);
            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                string errPlayer = "[LFPG] Searchlight placement failed. Kit preserved.";
                pb.MessageStatus(errPlayer);
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// DEVICE — CONSUMER : LFPG_DeviceBase
// 1 IN (input_0), 25 u/s, no wire store
// ---------------------------------------------------------
class LF_Searchlight : LFPG_DeviceBase
{
    // ---- Device-specific SyncVars ----
    protected bool  m_PoweredNet = false;
    protected bool  m_Overloaded = false;
    protected float m_AimYaw     = 0.0;
    protected float m_AimPitch   = 0.0;
    protected bool  m_SplashHit  = false;
    protected float m_SplashX    = 0.0;
    protected float m_SplashY    = 0.0;
    protected float m_SplashZ    = 0.0;

    // ---- Server-only: operator tracking (NOT SyncVars, NOT persisted) ----
    protected int m_OperatorNetLow  = 0;
    protected int m_OperatorNetHigh = 0;

    // ---- Client lights (NOT ref — ScriptedLightBase is engine object) ----
    protected ScriptedLightBase m_LightBeamCore;
    protected ScriptedLightBase m_LightBeamSpill;
    protected ScriptedLightBase m_LightHalo;
    protected ScriptedLightBase m_LightSplash;

    // ---- Base orientation yaw (set once in LFPG_OnInit, never synced/persisted) ----
    protected float m_BaseOriYaw = 0.0;

    // ============================================
    // Constructor — port + SyncVars
    // ============================================
    void LF_Searchlight()
    {
        string pIn = "input_0";
        string lIn = "Power Input";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, lIn);

        string varPowered    = "m_PoweredNet";
        string varOverloaded = "m_Overloaded";
        string varAimYaw     = "m_AimYaw";
        string varAimPitch   = "m_AimPitch";
        string varSplashHit  = "m_SplashHit";
        string varSplashX    = "m_SplashX";
        string varSplashY    = "m_SplashY";
        string varSplashZ    = "m_SplashZ";

        RegisterNetSyncVariableBool(varPowered);
        RegisterNetSyncVariableBool(varOverloaded);
        RegisterNetSyncVariableFloat(varAimYaw, -180.0, 180.0, 9);
        RegisterNetSyncVariableFloat(varAimPitch, -90.0, 90.0, 8);
        RegisterNetSyncVariableBool(varSplashHit);
        RegisterNetSyncVariableFloat(varSplashX, -500.0, 20500.0, 16);
        RegisterNetSyncVariableFloat(varSplashY, -10.0, 500.0, 12);
        RegisterNetSyncVariableFloat(varSplashZ, -500.0, 20500.0, 16);
    }

    // ============================================
    // SetActions — DeviceBase already removes TakeItem/TakeItemToHands
    // ============================================
    override void SetActions()
    {
        super.SetActions();
        AddAction(LFPG_ActionOperateSearchlight);
    }

    // ============================================
    // Virtual interface
    // ============================================
    override int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.CONSUMER;
    }

    override float LFPG_GetConsumption()
    {
        return 25.0;
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

        if (!powered)
        {
            LFPG_KickOperator();
        }

        string msg = "[LF_Searchlight] SetPowered(";
        msg = msg + powered.ToString();
        msg = msg + ") id=";
        msg = msg + m_DeviceId;
        LFPG_Util.Debug(msg);
        #endif
    }

    override bool LFPG_GetOverloaded()
    {
        return m_Overloaded;
    }

    override void LFPG_SetOverloaded(bool val)
    {
        #ifdef SERVER
        if (m_Overloaded == val)
            return;

        m_Overloaded = val;
        SetSynchDirty();
        #endif
    }

    // ============================================
    // Lifecycle hooks
    // ============================================
    override void LFPG_OnInit()
    {
        vector deployOri = GetOrientation();
        m_BaseOriYaw = deployOri[0];
    }

    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        LFPG_KickOperator();

        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        #endif

        #ifndef SERVER
        DestroyAllLights();
        LFPG_SearchlightController slCtrl = LFPG_SearchlightController.Get();
        if (slCtrl && slCtrl.IsOperatingEntity(this))
        {
            slCtrl.DoCleanup();
        }
        #endif
    }

    override void LFPG_OnDeleted()
    {
        #ifdef SERVER
        LFPG_KickOperator();
        #endif

        #ifndef SERVER
        DestroyAllLights();
        LFPG_SearchlightController slCtrl = LFPG_SearchlightController.Get();
        if (slCtrl && slCtrl.IsOperatingEntity(this))
        {
            slCtrl.DoCleanup();
        }
        #endif
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        LFPG_KickOperator();

        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        #endif
    }

    // ============================================
    // VarSync: lights + rvmat swap
    // ============================================
    override void LFPG_OnVarSync()
    {
        #ifndef SERVER
        bool hasLights = (m_LightBeamCore != null);

        if (m_PoweredNet && !hasLights)
        {
            CreateAllLights();
            ApplyOrientation();
            UpdateSplashFromSync();
        }
        else if (!m_PoweredNet && hasLights)
        {
            DestroyAllLights();
        }
        else if (m_PoweredNet && hasLights)
        {
            ApplyOrientation();
            UpdateSplashFromSync();
        }

        if (m_PoweredNet)
        {
            SetObjectMaterial(0, LFPG_SL_RVMAT_ON);
        }
        else
        {
            SetObjectMaterial(0, LFPG_SL_RVMAT_OFF);
        }
        #endif
    }

    // ============================================
    // Persistence: AimYaw + AimPitch
    // ============================================
    override void LFPG_OnStoreSaveExtra(ParamsWriteContext ctx)
    {
        ctx.Write(m_AimYaw);
        ctx.Write(m_AimPitch);
    }

    override bool LFPG_OnStoreLoadExtra(ParamsReadContext ctx, int ver)
    {
        if (!ctx.Read(m_AimYaw))
        {
            string errYaw = "[LF_Searchlight] OnStoreLoad failed: m_AimYaw";
            LFPG_Util.Error(errYaw);
            return false;
        }

        if (!ctx.Read(m_AimPitch))
        {
            string errPitch = "[LF_Searchlight] OnStoreLoad failed: m_AimPitch";
            LFPG_Util.Error(errPitch);
            return false;
        }

        return true;
    }

    // ============================================
    // Client lights — create / destroy
    // ============================================
    protected void CreateAllLights()
    {
        #ifndef SERVER
        if (m_LightBeamCore)
            return;

        ScriptedLightBase tmpLight;

        string mpName = "light_main";
        vector mpStart = ModelToWorld(GetMemoryPointPos(mpName));

        tmpLight = ScriptedLightBase.CreateLight(LFPG_SearchlightBeamCore, mpStart);
        m_LightBeamCore = LFPG_SearchlightBeamCore.Cast(tmpLight);

        tmpLight = ScriptedLightBase.CreateLight(LFPG_SearchlightBeamSpill, mpStart);
        m_LightBeamSpill = LFPG_SearchlightBeamSpill.Cast(tmpLight);

        vector mpStartLocal = GetMemoryPointPos(mpName);
        vector zeroVec = "0 0 0";
        tmpLight = ScriptedLightBase.CreateLight(LFPG_SearchlightHalo, zeroVec);
        m_LightHalo = LFPG_SearchlightHalo.Cast(tmpLight);
        if (m_LightHalo)
        {
            m_LightHalo.AttachOnObject(this, mpStartLocal, zeroVec);
        }

        tmpLight = ScriptedLightBase.CreateLight(LFPG_SearchlightSplash, mpStart);
        m_LightSplash = LFPG_SearchlightSplash.Cast(tmpLight);
        if (m_LightSplash)
        {
            m_LightSplash.SetBrightnessTo(0.0);
        }

        string logMsg = "[LF_Searchlight] CreateAllLights id=";
        logMsg = logMsg + m_DeviceId;
        LFPG_Util.Debug(logMsg);
        #endif
    }

    protected void DestroyAllLights()
    {
        #ifndef SERVER
        if (m_LightBeamCore)
        {
            m_LightBeamCore.Destroy();
            m_LightBeamCore = null;
        }
        if (m_LightBeamSpill)
        {
            m_LightBeamSpill.Destroy();
            m_LightBeamSpill = null;
        }
        if (m_LightHalo)
        {
            m_LightHalo.Destroy();
            m_LightHalo = null;
        }
        if (m_LightSplash)
        {
            m_LightSplash.Destroy();
            m_LightSplash = null;
        }

        string logDest = "[LF_Searchlight] DestroyAllLights";
        LFPG_Util.Debug(logDest);
        #endif
    }

    // ============================================
    // Client: apply light orientation from SyncVars
    // ============================================
    protected void ApplyOrientation()
    {
        #ifndef SERVER
        if (!m_LightBeamCore)
            return;

        float totalYaw = m_BaseOriYaw + m_AimYaw;

        vector entOri = Vector(totalYaw, 0, 0);
        SetOrientation(entOri);

        float pitchPhase = 0.5 - (m_AimPitch / 180.0);
        string animName = "light_main";
        SetAnimationPhase(animName, pitchPhase);

        float totalYawRad = totalYaw * Math.DEG2RAD;
        float pitchRad = m_AimPitch * Math.DEG2RAD;
        float cosPitch = Math.Cos(pitchRad);

        float dirX = Math.Sin(totalYawRad) * cosPitch;
        float dirY = Math.Sin(pitchRad);
        float dirZ = Math.Cos(totalYawRad) * cosPitch;

        vector beamDir = Vector(dirX, dirY, dirZ);
        beamDir.Normalize();
        vector beamOri = beamDir.VectorToAngles();

        string mpAxis = "light_main_axis";
        string mpBeam = "light_main";
        vector axisP0 = GetMemoryPointPos(mpAxis);
        vector beamStatic = GetMemoryPointPos(mpBeam);

        float offY = beamStatic[1] - axisP0[1];
        float offZ = beamStatic[2] - axisP0[2];

        float cosPitchR = Math.Cos(pitchRad);
        float sinPitchR = Math.Sin(pitchRad);
        float rotY = offY * cosPitchR - offZ * sinPitchR;
        float rotZ = offY * sinPitchR + offZ * cosPitchR;

        float localX = beamStatic[0];
        float localY = axisP0[1] + rotY;
        float localZ = axisP0[2] + rotZ;
        vector beamLocal = Vector(localX, localY, localZ);

        vector mpWorld = ModelToWorld(beamLocal);

        m_LightBeamCore.SetPosition(mpWorld);
        m_LightBeamCore.SetOrientation(beamOri);

        if (m_LightBeamSpill)
        {
            m_LightBeamSpill.SetPosition(mpWorld);
            m_LightBeamSpill.SetOrientation(beamOri);
        }
        #endif
    }

    // ============================================
    // Client: update splash light from SyncVars
    // ============================================
    protected void UpdateSplashFromSync()
    {
        #ifndef SERVER
        if (!m_LightSplash)
            return;

        if (m_SplashHit)
        {
            vector splashPos = Vector(m_SplashX, m_SplashY, m_SplashZ);
            m_LightSplash.SetPosition(splashPos);
            m_LightSplash.SetBrightnessTo(10.0);
        }
        else
        {
            m_LightSplash.SetBrightnessTo(0.0);
        }
        #endif
    }

    // ============================================
    // Server: aim setters (called from PlayerRPC)
    // ============================================
    float LFPG_GetBaseYaw()
    {
        return m_BaseOriYaw;
    }

    float LFPG_GetAimYaw()
    {
        return m_AimYaw;
    }

    float LFPG_GetAimPitch()
    {
        return m_AimPitch;
    }

    void LFPG_SetAim(float yaw, float pitch)
    {
        #ifdef SERVER
        m_AimYaw   = yaw;
        m_AimPitch = pitch;
        #endif
    }

    // ============================================
    // Client: local prediction (visual only)
    // ============================================
    void LFPG_ApplyAimLocal(float yaw, float pitch)
    {
        #ifndef SERVER
        if (!m_LightBeamCore)
            return;

        float totalYaw = m_BaseOriYaw + yaw;

        vector entOri = Vector(totalYaw, 0, 0);
        SetOrientation(entOri);

        float pitchPhase = 0.5 - (pitch / 180.0);
        string animName = "light_main";
        SetAnimationPhase(animName, pitchPhase);

        float totalYawRad = totalYaw * Math.DEG2RAD;
        float pitchRad = pitch * Math.DEG2RAD;
        float cosPitch = Math.Cos(pitchRad);

        float dirX = Math.Sin(totalYawRad) * cosPitch;
        float dirY = Math.Sin(pitchRad);
        float dirZ = Math.Cos(totalYawRad) * cosPitch;

        vector beamDir = Vector(dirX, dirY, dirZ);
        beamDir.Normalize();
        vector beamOri = beamDir.VectorToAngles();

        string mpAxis = "light_main_axis";
        string mpBeam = "light_main";
        vector axisP0 = GetMemoryPointPos(mpAxis);
        vector beamStatic = GetMemoryPointPos(mpBeam);

        float offY = beamStatic[1] - axisP0[1];
        float offZ = beamStatic[2] - axisP0[2];

        float cosPitchR = Math.Cos(pitchRad);
        float sinPitchR = Math.Sin(pitchRad);
        float rotY = offY * cosPitchR - offZ * sinPitchR;
        float rotZ = offY * sinPitchR + offZ * cosPitchR;

        float localX = beamStatic[0];
        float localY = axisP0[1] + rotY;
        float localZ = axisP0[2] + rotZ;
        vector beamLocal = Vector(localX, localY, localZ);

        vector mpWorld = ModelToWorld(beamLocal);

        m_LightBeamCore.SetPosition(mpWorld);
        m_LightBeamCore.SetOrientation(beamOri);

        if (m_LightBeamSpill)
        {
            m_LightBeamSpill.SetPosition(mpWorld);
            m_LightBeamSpill.SetOrientation(beamOri);
        }
        #endif
    }

    void LFPG_SetSplash(bool hit, float sx, float sy, float sz)
    {
        #ifdef SERVER
        m_SplashHit = hit;
        m_SplashX   = sx;
        m_SplashY   = sy;
        m_SplashZ   = sz;
        #endif
    }

    void LFPG_FlushSyncVars()
    {
        #ifdef SERVER
        SetSynchDirty();
        #endif
    }

    // ============================================
    // Server: operator tracking (grab system v1.5.0)
    // ============================================
    bool LFPG_HasOperator()
    {
        if (m_OperatorNetLow == 0 && m_OperatorNetHigh == 0)
            return false;

        #ifdef SERVER
        Object opObj = GetGame().GetObjectByNetworkId(m_OperatorNetLow, m_OperatorNetHigh);
        if (!opObj)
        {
            string warnGone = "[LF_Searchlight] Operator entity gone (disconnect?) — clearing lock";
            LFPG_Util.Warn(warnGone);
            m_OperatorNetLow  = 0;
            m_OperatorNetHigh = 0;
            return false;
        }
        #endif

        return true;
    }

    bool LFPG_IsOperator(int netLow, int netHigh)
    {
        if (m_OperatorNetLow == netLow && m_OperatorNetHigh == netHigh)
            return true;
        return false;
    }

    void LFPG_SetOperator(int netLow, int netHigh)
    {
        #ifdef SERVER
        m_OperatorNetLow  = netLow;
        m_OperatorNetHigh = netHigh;
        #endif
    }

    void LFPG_ClearOperator()
    {
        #ifdef SERVER
        m_OperatorNetLow  = 0;
        m_OperatorNetHigh = 0;

        // Force SyncVar push so all clients receive the final
        // aim position (m_AimYaw/m_AimPitch) after operator exits.
        SetSynchDirty();
        #endif
    }

    void LFPG_KickOperator()
    {
        #ifdef SERVER
        if (m_OperatorNetLow == 0 && m_OperatorNetHigh == 0)
            return;

        Object opObj = GetGame().GetObjectByNetworkId(m_OperatorNetLow, m_OperatorNetHigh);
        if (opObj)
        {
            PlayerBase opPlayer = PlayerBase.Cast(opObj);
            if (opPlayer)
            {
                PlayerIdentity opId = opPlayer.GetIdentity();
                if (opId)
                {
                    ScriptRPC kickRpc = new ScriptRPC();
                    int kickSubId = LFPG_RPC_SubId.SEARCHLIGHT_EXIT_CONFIRM;
                    kickRpc.Write(kickSubId);
                    kickRpc.Send(opPlayer, LFPG_RPC_CHANNEL, true, opId);
                    string kickMsg = "[LF_Searchlight] Kicked operator";
                    LFPG_Util.Info(kickMsg);
                }
            }
        }

        m_OperatorNetLow  = 0;
        m_OperatorNetHigh = 0;
        #endif
    }
};
