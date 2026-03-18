// =========================================================
// LF_PowerGrid - Searchlight device (v2.0.0)
//
// LF_Searchlight_Kit:  Holdable, deployable (same-model pattern = Splitter/Camera).
// LF_Searchlight:      CONSUMER, 1 IN (input_1), 25 u/s, no OUT, no wire store.
//
// Memory points (LOD Memory in p3d):
//   port_input_0      — cable anchor (base of tripod)
//   port_input_0_dir  — cable direction
//   light_main        — lens center (light/beam origin)
//   light_main_dir    — beam direction (1m forward)
//   light_main_axis   — pitch rotation axis (2 points, X-aligned)
//
// Named selections (LOD Visual 0 in p3d):
//   light       — hiddenSelections[0]: rvmat swap on/off (lens glow)
//   light_main  — animated bone (pitch ±90°)
//   camo        — body texture
//
// Rotation model:
//   YAW:   entity SetOrientation (whole model rotates)
//   PITCH: SetAnimationPhase("light_main", phase) — head tilts
//
// Enforce Script: no ternaries, no ++/--, no foreach, no +=/-=.
// =========================================================

static const string LFPG_SL_RVMAT_OFF = "\\LFPowerGrid\\data\\searchlight\\lf_searchlight_lens_off.rvmat";
static const string LFPG_SL_RVMAT_ON  = "\\LFPowerGrid\\data\\searchlight\\lf_searchlight_lens_on.rvmat";

// Splash Y offset above ground surface
static const float LFPG_SL_SPLASH_Y_OFFSET = 0.05;

// ---------------------------------------------------------
// KIT — patron identico a LF_Camera_Kit / LF_Splitter_Kit
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

    // Previene loop sound huerfano: ObjectDelete durante OnPlacementComplete
    // interrumpe cleanup del action callback antes de detener sonido.
    override string GetLoopDeploySoundset()
    {
        return "";
    }

    override void SetActions()
    {
        super.SetActions();
        AddAction(ActionTogglePlaceObject);
        AddAction(LFPG_ActionPlaceSearchlight);
    }

    // Usar parametro position/orientation, NUNCA GetPosition().
    // GetPosition() devuelve la pos del kit (cerca del player), no el hologram.
    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[Searchlight_Kit] OnPlacementComplete: param=" + position.ToString();
        tLog = tLog + " kitPos=" + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        EntityAI sl = GetGame().CreateObjectEx("LF_Searchlight", finalPos, ECE_CREATEPHYSICS);
        if (sl)
        {
            sl.SetPosition(finalPos);
            sl.SetOrientation(finalOri);
            sl.Update();

            string deployMsg = "[Searchlight_Kit] Deployed LF_Searchlight at " + finalPos.ToString();
            deployMsg = deployMsg + " ori=" + finalOri.ToString();
            LFPG_Util.Info(deployMsg);

            GetGame().ObjectDelete(this);
        }
        else
        {
            LFPG_Util.Error("[Searchlight_Kit] Failed to create LF_Searchlight! Kit preserved.");
            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                pb.MessageStatus("[LFPG] Searchlight placement failed. Kit preserved.");
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// DEVICE — CONSUMER, 1 IN (input_1), 25 u/s
// ---------------------------------------------------------
class LF_Searchlight : Inventory_Base
{
    // ---- SyncVars ----
    protected int   m_DeviceIdLow  = 0;
    protected int   m_DeviceIdHigh = 0;
    protected bool  m_PoweredNet   = false;
    protected bool  m_Overloaded   = false;

    // Phase 2: rotation (registered now, used later)
    protected float m_AimYaw   = 0.0;
    protected float m_AimPitch = 0.0;

    // Phase 2: splash position (registered now, used later)
    protected bool  m_SplashHit = false;
    protected float m_SplashX   = 0.0;
    protected float m_SplashY   = 0.0;
    protected float m_SplashZ   = 0.0;

    // ---- Estado local ----
    protected string m_DeviceId      = "";
    protected bool   m_LFPG_Deleting = false;

    // ---- Server-only: operator tracking (grab system v1.5.0) ----
    // NOT SyncVars, NOT persisted. Only valid while someone is grabbing.
    protected int    m_OperatorNetLow  = 0;
    protected int    m_OperatorNetHigh = 0;

    // ---- Client lights (NOT ref — ScriptedLightBase is engine object) ----
    protected ScriptedLightBase m_LightBeamCore;
    protected ScriptedLightBase m_LightBeamSpill;
    protected ScriptedLightBase m_LightHalo;
    protected ScriptedLightBase m_LightSplash;

    // ---- Base orientation yaw (set once in EEInit, never synced/persisted) ----
    // This is the entity's deployment orientation BEFORE any aim is applied.
    // Used by controller to compute local yaw, and by ApplyOrientation
    // to reconstruct world orientation = m_BaseOriYaw + m_AimYaw.
    protected float m_BaseOriYaw = 0.0;

    // ============================================
    // Constructor — SyncVar registration
    // MUST be constructor, NOT EEInit.
    // ============================================
    void LF_Searchlight()
    {
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
        RegisterNetSyncVariableBool("m_Overloaded");

        // v2.0: yaw full 360, pitch +-90
        RegisterNetSyncVariableFloat("m_AimYaw", -180.0, 180.0, 9);
        RegisterNetSyncVariableFloat("m_AimPitch", -90.0, 90.0, 8);

        // Phase 2: splash SyncVars
        RegisterNetSyncVariableBool("m_SplashHit");
        RegisterNetSyncVariableFloat("m_SplashX", -500.0, 20500.0, 16);
        RegisterNetSyncVariableFloat("m_SplashY", -10.0, 500.0, 12);
        RegisterNetSyncVariableFloat("m_SplashZ", -500.0, 20500.0, 16);
    }

    // ============================================
    // Helpers de ID
    // ============================================
    protected void LFPG_UpdateDeviceIdString()
    {
        m_DeviceId = LFPG_Util.MakeDeviceKey(m_DeviceIdLow, m_DeviceIdHigh);
    }

    protected void LFPG_TryRegister()
    {
        if (m_LFPG_Deleting)
            return;

        string oldId = m_DeviceId;
        LFPG_UpdateDeviceIdString();

        if (oldId != "" && oldId != m_DeviceId)
        {
            LFPG_DeviceRegistry.Get().Unregister(oldId, this);
        }

        if (m_DeviceId != "")
        {
            LFPG_DeviceRegistry.Get().Register(this, m_DeviceId);
        }
    }

    // ============================================
    // Lifecycle
    // ============================================
    override void EEInit()
    {
        super.EEInit();

        // Capture deployment orientation (set once, never changes).
        // Server never calls SetOrientation, so GetOrientation() is always
        // the original deployment yaw — on fresh spawn AND persistence load.
        vector deployOri = GetOrientation();
        m_BaseOriYaw = deployOri[0];

        #ifdef SERVER
        if (m_DeviceIdLow == 0 && m_DeviceIdHigh == 0)
        {
            LFPG_Util.GenerateDeviceId(m_DeviceIdLow, m_DeviceIdHigh);
        }
        SetSynchDirty();
        #endif

        LFPG_UpdateDeviceIdString();
        LFPG_TryRegister();
    }

    override void EEKilled(Object killer)
    {
        LFPG_DeviceLifecycle.OnDeviceKilled(this, m_DeviceId);

        #ifdef SERVER
        // Kick any operator before destroying
        LFPG_KickOperator();

        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        #endif

        #ifndef SERVER
        DestroyAllLights();
        // If this searchlight was being operated, release the grab
        LFPG_SearchlightController slCtrl = LFPG_SearchlightController.Get();
        if (slCtrl && slCtrl.IsOperatingEntity(this))
        {
            slCtrl.DoCleanup();
        }
        #endif

        super.EEKilled(killer);
    }

    override void EEDelete(EntityAI parent)
    {
        m_LFPG_Deleting = true;

        #ifdef SERVER
        LFPG_KickOperator();
        #endif

        LFPG_DeviceLifecycle.OnDeviceDeleted(this, m_DeviceId);

        #ifndef SERVER
        DestroyAllLights();
        LFPG_SearchlightController slCtrl = LFPG_SearchlightController.Get();
        if (slCtrl && slCtrl.IsOperatingEntity(this))
        {
            slCtrl.DoCleanup();
        }
        #endif

        super.EEDelete(parent);
    }

    override void EEItemLocationChanged(notnull InventoryLocation oldLoc, notnull InventoryLocation newLoc)
    {
        super.EEItemLocationChanged(oldLoc, newLoc);

        #ifdef SERVER
        if (m_DeviceId == "")
            return;

        bool wiresCut = LFPG_DeviceLifecycle.OnDeviceMoved(this, m_DeviceId, oldLoc, newLoc);
        if (wiresCut)
        {
            // Kick operator BEFORE changing power state
            LFPG_KickOperator();

            if (m_PoweredNet)
            {
                m_PoweredNet = false;
                SetSynchDirty();
            }
        }
        #endif
    }

    // ============================================
    // Client sync — lights + rvmat swap
    // ============================================
    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();
        LFPG_TryRegister();

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

        // Rvmat swap: light = hiddenSelections[0]
        if (m_PoweredNet)
        {
            SetObjectMaterial(0, LFPG_SL_RVMAT_ON);
        }
        else
        {
            SetObjectMaterial(0, LFPG_SL_RVMAT_OFF);
        }

        // JIP cable parity
        if (m_DeviceId != "")
        {
            LFPG_CableRenderer r = LFPG_CableRenderer.Get();
            if (r)
            {
                r.RequestDeviceSync(m_DeviceId, this);
            }
        }
        #endif
    }

    // ============================================
    // Client lights — create / destroy
    // ============================================
    protected void CreateAllLights()
    {
        #ifndef SERVER
        if (m_LightBeamCore)
            return;

        // BeamCore and BeamSpill: created free-floating (NOT attached).
        // Position and orientation set in ApplyOrientation() via world-space
        // SetPosition + SetOrientation. This avoids DetachFromParent/AttachOnObject
        // cycling which causes 1-frame light disappearance on each aim update.
        ScriptedLightBase tmpLight;

        vector mpStart = ModelToWorld(GetMemoryPointPos("light_main"));

        tmpLight = ScriptedLightBase.CreateLight(LFPG_SearchlightBeamCore, mpStart);
        m_LightBeamCore = LFPG_SearchlightBeamCore.Cast(tmpLight);

        tmpLight = ScriptedLightBase.CreateLight(LFPG_SearchlightBeamSpill, mpStart);
        m_LightBeamSpill = LFPG_SearchlightBeamSpill.Cast(tmpLight);

        // Halo: point light at lens center — attached to entity (follows position,
        // no orientation changes needed for omnidirectional light).
        vector mpStartLocal = GetMemoryPointPos("light_main");
        tmpLight = ScriptedLightBase.CreateLight(LFPG_SearchlightHalo, "0 0 0");
        m_LightHalo = LFPG_SearchlightHalo.Cast(tmpLight);
        if (m_LightHalo)
        {
            m_LightHalo.AttachOnObject(this, mpStartLocal, "0 0 0");
        }

        // Splash: created free-floating, positioned from SyncVars in UpdateSplashFromSync.
        // Starts hidden via brightness=0 (NOT SetEnabled — engine can recycle disabled lights).
        tmpLight = ScriptedLightBase.CreateLight(LFPG_SearchlightSplash, mpStart);
        m_LightSplash = LFPG_SearchlightSplash.Cast(tmpLight);
        if (m_LightSplash)
        {
            m_LightSplash.SetBrightnessTo(0.0);
        }

        string logMsg = "[LF_Searchlight] CreateAllLights id=" + m_DeviceId;
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

        LFPG_Util.Debug("[LF_Searchlight] DestroyAllLights");
        #endif
    }

    // ============================================
    // Client: apply light orientation from SyncVars
    // v2.0: YAW = entity SetOrientation (whole model)
    //        PITCH = SetAnimationPhase("light_main") (head bone)
    //        Beam origin computed from axis rotation math
    // ============================================
    protected void ApplyOrientation()
    {
        #ifndef SERVER
        if (!m_LightBeamCore)
            return;

        // Total world yaw = base deployment yaw + local aim offset.
        float totalYaw = m_BaseOriYaw + m_AimYaw;

        // ---- Rotate entity model (yaw only, pitch=0, roll=0) ----
        vector entOri = Vector(totalYaw, 0, 0);
        SetOrientation(entOri);

        // ---- Pitch: animate the head bone ----
        // phase = 0.5 - (pitch / 180) maps [-90,+90] to [1.0, 0.0]
        // angle0=+pi/2 (phase 0 = looking up), angle1=-pi/2 (phase 1 = looking down)
        float pitchPhase = 0.5 - (m_AimPitch / 180.0);
        string animName = "light_main";
        SetAnimationPhase(animName, pitchPhase);

        // ---- Compute beam direction in world space ----
        float totalYawRad = totalYaw * Math.DEG2RAD;
        float pitchRad = m_AimPitch * Math.DEG2RAD;
        float cosPitch = Math.Cos(pitchRad);

        float dirX = Math.Sin(totalYawRad) * cosPitch;
        float dirY = Math.Sin(pitchRad);
        float dirZ = Math.Cos(totalYawRad) * cosPitch;

        vector beamDir = Vector(dirX, dirY, dirZ);
        beamDir.Normalize();
        vector beamOri = beamDir.VectorToAngles();

        // ---- Compute animated beam origin ----
        // GetMemoryPointPos returns STATIC position (ignores animation).
        // The axis center does not move, but the beam origin orbits
        // around it when pitched. Compute the offset rotation manually.
        //
        // light_main_axis: pitch rotation axis center (Y~1.356)
        // light_main: beam origin (offset ~0.16m in local +Z)
        //
        // At pitch=0:   beam origin is forward (+Z from axis)
        // At pitch=+90: beam origin moves up (+Y from axis)
        // At pitch=-90: beam origin moves down (-Y from axis)

        vector axisP0 = GetMemoryPointPos("light_main_axis");
        vector beamStatic = GetMemoryPointPos("light_main");

        // Offset from axis center in local model space (Y and Z)
        float offY = beamStatic[1] - axisP0[1];
        float offZ = beamStatic[2] - axisP0[2];

        // Rotate offset by pitch around X axis
        float cosPitchR = Math.Cos(pitchRad);
        float sinPitchR = Math.Sin(pitchRad);
        float rotY = offY * cosPitchR - offZ * sinPitchR;
        float rotZ = offY * sinPitchR + offZ * cosPitchR;

        // Reconstruct local position
        float localX = beamStatic[0];
        float localY = axisP0[1] + rotY;
        float localZ = axisP0[2] + rotZ;
        vector beamLocal = Vector(localX, localY, localZ);

        // Transform to world (accounts for entity yaw via SetOrientation)
        vector mpWorld = ModelToWorld(beamLocal);

        m_LightBeamCore.SetPosition(mpWorld);
        m_LightBeamCore.SetOrientation(beamOri);

        if (m_LightBeamSpill)
        {
            m_LightBeamSpill.SetPosition(mpWorld);
            m_LightBeamSpill.SetOrientation(beamOri);
        }

        // Halo stays attached to entity (omnidirectional, follows via parent)
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
            // Restore full brightness (not SetEnabled — engine can recycle
            // disabled free-floating lights, causing permanent disappearance)
            m_LightSplash.SetBrightnessTo(10.0);
        }
        else
        {
            // Hide without disabling — zero brightness keeps light alive
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
        // Rotation is client-side only (ApplyOrientation in OnVariablesSynchronized).
        // Server entity keeps deployment orientation — simplifies persistence + JIP.
        // NOTE: caller (HandleSearchlightAim) calls SetSynchDirty once after SetSplash
        #endif
    }

    void LFPG_SetSplash(bool hit, float sx, float sy, float sz)
    {
        #ifdef SERVER
        m_SplashHit = hit;
        m_SplashX   = sx;
        m_SplashY   = sy;
        m_SplashZ   = sz;
        // NOTE: caller (HandleSearchlightAim) calls SetSynchDirty once after this
        #endif
    }

    // Batch sync: called once after SetAim + SetSplash
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

        // Lazy validation: check operator entity still exists.
        // If player disconnected, GetObjectByNetworkId returns null.
        // Clear the lock so the searchlight becomes available.
        #ifdef SERVER
        Object opObj = GetGame().GetObjectByNetworkId(m_OperatorNetLow, m_OperatorNetHigh);
        if (!opObj)
        {
            LFPG_Util.Warn("[LF_Searchlight] Operator entity gone (disconnect?) — clearing lock");
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
        #endif
    }

    // Kick the operator (server-initiated exit). Sends EXIT_CONFIRM RPC.
    void LFPG_KickOperator()
    {
        #ifdef SERVER
        if (m_OperatorNetLow == 0 && m_OperatorNetHigh == 0)
            return;

        // Resolve player to send the kick RPC
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
                    LFPG_Util.Info("[LF_Searchlight] Kicked operator");
                }
            }
        }

        m_OperatorNetLow  = 0;
        m_OperatorNetHigh = 0;
        #endif
    }

    // ============================================
    // Persistence — CONSUMER: ids + yaw/pitch
    // NO m_PoweredNet (derived state).
    // NO m_Overloaded, m_SplashHit/X/Y/Z (transient).
    // Orden DEBE coincidir exactamente entre Save y Load.
    // ============================================
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);
        ctx.Write(m_DeviceIdLow);
        ctx.Write(m_DeviceIdHigh);
        ctx.Write(m_AimYaw);
        ctx.Write(m_AimPitch);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version))
            return false;

        if (!ctx.Read(m_DeviceIdLow))
            return false;
        if (!ctx.Read(m_DeviceIdHigh))
            return false;
        if (!ctx.Read(m_AimYaw))
            return false;
        if (!ctx.Read(m_AimPitch))
            return false;

        return true;
    }

    // ============================================
    // Guards de inventario y colocacion
    // ============================================
    override bool CanPutInCargo(EntityAI parent)
    {
        return false;
    }

    override bool CanPutIntoHands(EntityAI parent)
    {
        return false;
    }

    override bool CanBePlaced(Man player, vector position)
    {
        return false;
    }

    override bool IsHeavyBehaviour()
    {
        return false;
    }

    override void SetActions()
    {
        super.SetActions();
        RemoveAction(ActionTakeItem);
        RemoveAction(ActionTakeItemToHands);
        AddAction(LFPG_ActionOperateSearchlight);
    }

    // Safety: bloquea CompEM vanilla
    override bool IsElectricAppliance()
    {
        return false;
    }

    // ============================================
    // LFPG_IDevice interface
    // ============================================
    string LFPG_GetDeviceId()
    {
        return m_DeviceId;
    }

    int LFPG_GetDeviceIdLow()
    {
        return m_DeviceIdLow;
    }

    int LFPG_GetDeviceIdHigh()
    {
        return m_DeviceIdHigh;
    }

    int LFPG_GetPortCount()
    {
        return 1;
    }

    string LFPG_GetPortName(int idx)
    {
        if (idx == 0)
            return "input_1";
        return "";
    }

    int LFPG_GetPortDir(int idx)
    {
        if (idx == 0)
            return LFPG_PortDir.IN;
        return -1;
    }

    string LFPG_GetPortLabel(int idx)
    {
        if (idx == 0)
            return "Power Input";
        return "";
    }

    bool LFPG_HasPort(string portName, int dir)
    {
        if (dir == LFPG_PortDir.IN && portName == "input_1")
            return true;
        return false;
    }

    vector LFPG_GetPortWorldPos(string portName)
    {
        // p3d memory point is "port_input_0" regardless of portName convention.
        // Old model used "port_input_1"; new model uses "port_input_0".
        string memPoint = "port_input_0";

        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

        LFPG_Util.Warn("[LF_Searchlight] Missing memory point: " + memPoint);
        vector p = GetPosition();
        p[1] = p[1] + 0.3;
        return p;
    }

    int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.CONSUMER;
    }

    bool LFPG_IsSource()
    {
        return false;
    }

    float LFPG_GetConsumption()
    {
        return 25.0;
    }

    bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;
        SetSynchDirty();

        // Kick operator if power lost
        if (!powered)
        {
            LFPG_KickOperator();
        }

        string msg = "[LF_Searchlight] SetPowered(" + powered.ToString() + ") id=" + m_DeviceId;
        LFPG_Util.Debug(msg);
        #endif
    }

    void LFPG_SetOverloaded(bool overloaded)
    {
        #ifdef SERVER
        if (m_Overloaded == overloaded)
            return;

        m_Overloaded = overloaded;
        SetSynchDirty();
        #endif
    }

    // CONSUMER — no tiene puerto OUT, no puede ser origen de conexion.
    bool LFPG_CanConnectTo(Object other, string myPort, string otherPort)
    {
        return false;
    }

    // No wire store (IN-only).
    bool LFPG_HasWireStore()
    {
        return false;
    }
};
