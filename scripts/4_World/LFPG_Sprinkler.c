// =========================================================
// LF_PowerGrid - Sprinkler device (v1.0.0)
//
// LF_Sprinkler_Kit:  Holdable, deployable (same-model pattern).
// LF_Sprinkler:      CONSUMER, 1 IN (input_0), 5 u/s, no OUT, no wire store.
//
// Memory points (LOD Memory in p3d):
//   port_input_0      — cable anchor
//   unit              — placement reference
//
// Sprinkler activates when:
//   1. Powered (electricity from graph)
//   2. Upstream direct is WaterPump (T1 or T2)
//   3. Pump is powered
//   4. If T2 + output_3: tank not empty
//   5. Not ruined
//
// m_SprinklerActive is server-derived, synced to client for FX.
// Persistence: only DeviceId (m_SprinklerActive derived post-restart).
//
// Enforce Script: no ternaries, no ++/--, no foreach, no +=/-=.
// =========================================================

// ---------------------------------------------------------
// KIT — same-model deployment (patron Searchlight/Splitter)
// ---------------------------------------------------------
class LF_Sprinkler_Kit : Inventory_Base
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
        AddAction(LFPG_ActionPlaceSprinkler);
    }

    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[Sprinkler_Kit] OnPlacementComplete: param=";
        tLog = tLog + position.ToString();
        tLog = tLog + " kitPos=";
        tLog = tLog + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        string typeName = "LF_Sprinkler";
        EntityAI spr = GetGame().CreateObjectEx(typeName, finalPos, ECE_CREATEPHYSICS);
        if (spr)
        {
            spr.SetPosition(finalPos);
            spr.SetOrientation(finalOri);
            spr.Update();

            string deployMsg = "[Sprinkler_Kit] Deployed LF_Sprinkler at ";
            deployMsg = deployMsg + finalPos.ToString();
            deployMsg = deployMsg + " ori=";
            deployMsg = deployMsg + finalOri.ToString();
            LFPG_Util.Info(deployMsg);

            GetGame().ObjectDelete(this);
        }
        else
        {
            string errMsg = "[Sprinkler_Kit] Failed to create LF_Sprinkler! Kit preserved.";
            LFPG_Util.Error(errMsg);
            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                string failMsg = "[LFPG] Sprinkler placement failed. Kit preserved.";
                pb.MessageStatus(failMsg);
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// DEVICE — CONSUMER, 1 IN (input_0), 5 u/s
// ---------------------------------------------------------
class LF_Sprinkler : Inventory_Base
{
    // ---- SyncVars ----
    protected int   m_DeviceIdLow    = 0;
    protected int   m_DeviceIdHigh   = 0;
    protected bool  m_PoweredNet     = false;
    protected bool  m_SprinklerActive = false;

    // ---- Estado local ----
    protected string m_DeviceId       = "";
    protected bool   m_LFPG_Deleting  = false;

    // ---- Server-only: upstream tracking (set by NetworkManager tick) ----
    protected bool   m_HasWaterSource  = false;
    protected string m_WaterSourceId   = "";
    protected string m_SourcePort      = "";

    // ---- Client: sound (NOT ref — engine object) ----
    protected EffectSound m_LoopSound;

    // ============================================
    // Constructor — SyncVar registration
    // MUST be constructor, NOT EEInit.
    // ============================================
    void LF_Sprinkler()
    {
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
        RegisterNetSyncVariableBool("m_SprinklerActive");
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
        bool dirty = false;
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            dirty = true;
        }
        if (m_SprinklerActive)
        {
            m_SprinklerActive = false;
            dirty = true;
        }
        if (dirty)
        {
            SetSynchDirty();
        }
        #endif

        #ifndef SERVER
        LFPG_CleanupClientFX();
        #endif

        super.EEKilled(killer);
    }

    override void EEDelete(EntityAI parent)
    {
        m_LFPG_Deleting = true;

        LFPG_DeviceLifecycle.OnDeviceDeleted(this, m_DeviceId);

        #ifndef SERVER
        LFPG_CleanupClientFX();
        #endif

        super.EEDelete(parent);
    }

    // ============================================
    // Client FX cleanup
    // ============================================
    protected void LFPG_CleanupClientFX()
    {
        if (m_LoopSound)
        {
            m_LoopSound.SoundStop();
            m_LoopSound = null;
        }
        // TODO S4: particle cleanup here
    }

    // ============================================
    // Client sync — sound + particle toggle
    // ============================================
    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();

        LFPG_UpdateDeviceIdString();
        LFPG_TryRegister();

        // Sound toggle based on m_SprinklerActive
        if (m_SprinklerActive && !m_LoopSound)
        {
            string soundSet = LFPG_SPRINKLER_LOOP_SOUNDSET;
            m_LoopSound = SEffectManager.PlaySound(soundSet, GetPosition());
            if (m_LoopSound)
            {
                m_LoopSound.SetAutodestroy(false);
            }
        }
        if (!m_SprinklerActive && m_LoopSound)
        {
            m_LoopSound.SoundStop();
            m_LoopSound = null;
        }

        // TODO S4: particle toggle here
    }

    // ============================================
    // Server: sprinkler state accessors
    // (called by NetworkManager tick)
    // ============================================
    bool LFPG_GetSprinklerActive()
    {
        return m_SprinklerActive;
    }

    void LFPG_SetSprinklerActive(bool active)
    {
        #ifdef SERVER
        if (m_SprinklerActive == active)
            return;

        m_SprinklerActive = active;
        SetSynchDirty();

        string msg = "[LF_Sprinkler] SetSprinklerActive(";
        msg = msg + active.ToString();
        msg = msg + ") id=";
        msg = msg + m_DeviceId;
        LFPG_Util.Debug(msg);
        #endif
    }

    bool LFPG_GetHasWaterSource()
    {
        return m_HasWaterSource;
    }

    void LFPG_SetHasWaterSource(bool has)
    {
        #ifdef SERVER
        m_HasWaterSource = has;
        #endif
    }

    string LFPG_GetWaterSourceId()
    {
        return m_WaterSourceId;
    }

    void LFPG_SetWaterSourceId(string id)
    {
        #ifdef SERVER
        m_WaterSourceId = id;
        #endif
    }

    string LFPG_GetSourcePort()
    {
        return m_SourcePort;
    }

    void LFPG_SetSourcePort(string port)
    {
        #ifdef SERVER
        m_SourcePort = port;
        #endif
    }

    bool LFPG_GetPoweredNet()
    {
        return m_PoweredNet;
    }

    // ============================================
    // Persistence — CONSUMER: ids only
    // m_PoweredNet + m_SprinklerActive = derived state.
    // ============================================
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);
        ctx.Write(m_DeviceIdLow);
        ctx.Write(m_DeviceIdHigh);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version))
            return false;

        if (!ctx.Read(m_DeviceIdLow))
            return false;
        if (!ctx.Read(m_DeviceIdHigh))
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
        AddAction(LFPG_ActionCheckSprinkler);
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
            return "input_0";
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
        string inName = "input_0";
        if (dir == LFPG_PortDir.IN && portName == inName)
            return true;
        return false;
    }

    vector LFPG_GetPortWorldPos(string portName)
    {
        string memPoint = "port_input_0";

        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

        string warnMsg = "[LF_Sprinkler] Missing memory point: ";
        warnMsg = warnMsg + memPoint;
        LFPG_Util.Warn(warnMsg);
        vector p = GetPosition();
        p[1] = p[1] + 0.1;
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
        return LFPG_SPRINKLER_CONSUMPTION;
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

        string msg = "[LF_Sprinkler] SetPowered(";
        msg = msg + powered.ToString();
        msg = msg + ") id=";
        msg = msg + m_DeviceId;
        LFPG_Util.Debug(msg);
        #endif
    }

    void LFPG_SetOverloaded(bool overloaded)
    {
        // Sprinkler does not track overloaded state (CONSUMER, no output)
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
