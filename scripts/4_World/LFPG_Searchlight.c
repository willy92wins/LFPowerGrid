// =========================================================
// LF_PowerGrid - Searchlight device (v1.4.0)
//
// LF_Searchlight_Kit:  Holdable, deployable (same-model pattern = Splitter/Camera).
// LF_Searchlight:      CONSUMER, 1 IN (input_1), 25 u/s, no OUT, no wire store.
//
// Memory points (LOD Memory in p3d):
//   port_input_1  — cable anchor (base of tripod)
//   beamStart     — lens center (light origin)
//   beamEnd       — 1m forward (beam direction)
//
// Named selections (LOD Visual 0 in p3d):
//   lens_glow     — hiddenSelections[0]: rvmat swap on/off
//   beamEnd       — for AttachOnMemoryPoint (View LOD named selection)
//
// Phase 1: static CONSUMER — lights on when powered, no rotation.
// Phase 2 (next sprint): rotation via SyncVars + spectator COT + splash.
//
// SyncVars reserved for Phase 2 are registered here but unused.
// Persistence includes yaw/pitch for Phase 2 restore.
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

    // ---- Client lights (NOT ref — ScriptedLightBase is engine object) ----
    protected ScriptedLightBase m_LightBeamCore;
    protected ScriptedLightBase m_LightBeamSpill;
    protected ScriptedLightBase m_LightHalo;
    protected ScriptedLightBase m_LightSplash;

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

        // Phase 2: rotation SyncVars
        RegisterNetSyncVariableFloat("m_AimYaw", -120.0, 120.0, 8);
        RegisterNetSyncVariableFloat("m_AimPitch", -10.0, 45.0, 7);

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
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        #endif

        #ifndef SERVER
        DestroyAllLights();
        #endif

        super.EEKilled(killer);
    }

    override void EEDelete(EntityAI parent)
    {
        m_LFPG_Deleting = true;
        LFPG_DeviceLifecycle.OnDeviceDeleted(this, m_DeviceId);

        #ifndef SERVER
        DestroyAllLights();
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
        }
        else if (!m_PoweredNet && hasLights)
        {
            DestroyAllLights();
        }
        else if (m_PoweredNet && hasLights)
        {
            // Phase 2: ApplyOrientation + UpdateSplashFromSync here
        }

        // Rvmat swap: lens_glow = hiddenSelections[0]
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

        // BeamCore: attached at beamStart, oriented toward beamEnd
        vector mpStart = GetMemoryPointPos("beamStart");
        vector mpEnd   = GetMemoryPointPos("beamEnd");
        vector beamDir = vector.Direction(mpStart, mpEnd);
        beamDir.Normalize();
        vector beamOri = beamDir.VectorToAngles();

        m_LightBeamCore = LFPG_SearchlightBeamCore.Cast(
            ScriptedLightBase.CreateLight(LFPG_SearchlightBeamCore, "0 0 0")
        );
        if (m_LightBeamCore)
        {
            m_LightBeamCore.AttachOnObject(this, mpStart, beamOri);
        }

        // BeamSpill: same attachment as core (wider cone)
        m_LightBeamSpill = LFPG_SearchlightBeamSpill.Cast(
            ScriptedLightBase.CreateLight(LFPG_SearchlightBeamSpill, "0 0 0")
        );
        if (m_LightBeamSpill)
        {
            m_LightBeamSpill.AttachOnObject(this, mpStart, beamOri);
        }

        // Halo: glow at lens center (no orientation needed)
        m_LightHalo = LFPG_SearchlightHalo.Cast(
            ScriptedLightBase.CreateLight(LFPG_SearchlightHalo, "0 0 0")
        );
        if (m_LightHalo)
        {
            m_LightHalo.AttachOnObject(this, mpStart, "0 0 0");
        }

        // Splash: Phase 1 = positioned at beamStart + some forward offset (static)
        // Phase 2 will reposition via SyncVars from server raycast.
        // For now, place it ahead of the beam at ground level as a placeholder.
        m_LightSplash = LFPG_SearchlightSplash.Cast(
            ScriptedLightBase.CreateLight(LFPG_SearchlightSplash, "0 0 0")
        );
        if (m_LightSplash)
        {
            // Phase 1: attach near beamEnd (1m forward)
            // Phase 2: detach and SetPosition from SyncVars
            m_LightSplash.AttachOnObject(this, mpEnd, "0 0 0");
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
        // Phase 2: AddAction(LFPG_ActionOperateSearchlight);
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
        string memPoint = "port_" + portName;
        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

        // Compact fallback: "port_input1" vs "port_input_1"
        int len = portName.Length();
        if (len >= 3)
        {
            string lastChar   = portName.Substring(len - 1, 1);
            string beforeLast = portName.Substring(len - 2, 1);
            if (beforeLast == "_")
            {
                string compact = "port_" + portName.Substring(0, len - 2) + lastChar;
                if (MemoryPointExists(compact))
                {
                    return ModelToWorld(GetMemoryPointPos(compact));
                }
            }
        }

        if (MemoryPointExists(portName))
        {
            return ModelToWorld(GetMemoryPointPos(portName));
        }

        LFPG_Util.Warn("[LF_Searchlight] Missing memory point for port: " + portName);
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
