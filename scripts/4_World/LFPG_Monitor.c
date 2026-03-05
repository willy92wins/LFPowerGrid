// =========================================================
// LF_PowerGrid - Monitor device (v0.9.0 - Etapa 3)
//
// LF_Monitor_Kit: Holdable, deployable (same-model pattern).
// LF_Monitor:     CONSUMER, 1 IN (input_1), 20 u/s, no OUT, no wire store.
//
// Etapa 2: Sistema de emparejamiento Monitor<->Camera.
//   m_LinkedCamIdLow/High: SyncVars del DeviceId de la camara enlazada.
//
// Etapa 3: Viewport CCTV + material swap.
//   ActionLFPG_ViewCamera: entra en POV de la camara enlazada (client-only).
//   SetObjectMaterial: rvmat ON cuando monitor encendido + camara enlazada,
//   OFF en cualquier otro estado. Swap en OnVariablesSynchronized.
//
// ⚠ SAVE WIPE REQUERIDA al subir a v0.9.0 — la persistencia cambia
//   de orden (2 campos nuevos). No hay migrador por diseno del proyecto.
//
// Etapa 3: OnVariablesSynchronized amplia con viewport render.
// =========================================================

static const string LFPG_MONITOR_RVMAT_OFF = "\\LFPowerGrid\\data\\cctv\\lf_monitor_off.rvmat";
static const string LFPG_MONITOR_RVMAT_ON  = "\\LFPowerGrid\\data\\cctv\\lf_monitor_on.rvmat";

// ---------------------------------------------------------
// KIT - patron identico a LF_Splitter_Kit / LF_Combiner_Kit
// ---------------------------------------------------------
class LF_Monitor_Kit : Inventory_Base
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
        AddAction(LFPG_ActionPlaceMonitor);
    }

    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[Monitor_Kit] OnPlacementComplete: param=" + position.ToString();
        tLog = tLog + " kitPos=" + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        EntityAI mon = GetGame().CreateObjectEx("LF_Monitor", finalPos, ECE_CREATEPHYSICS);
        if (mon)
        {
            mon.SetPosition(finalPos);
            mon.SetOrientation(finalOri);
            mon.Update();

            string deployMsg = "[Monitor_Kit] Deployed LF_Monitor at " + finalPos.ToString();
            deployMsg = deployMsg + " ori=" + finalOri.ToString();
            LFPG_Util.Info(deployMsg);

            GetGame().ObjectDelete(this);
        }
        else
        {
            LFPG_Util.Error("[Monitor_Kit] Failed to create LF_Monitor! Kit preserved.");
            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                pb.MessageStatus("[LFPG] Monitor placement failed. Kit preserved.");
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// DEVICE - CONSUMER, 1 IN (input_1), 20 u/s
// ---------------------------------------------------------
class LF_Monitor : Inventory_Base
{
    // ---- SyncVars: identidad del dispositivo ----
    protected int  m_DeviceIdLow  = 0;
    protected int  m_DeviceIdHigh = 0;
    protected bool m_PoweredNet   = false;

    // ---- SyncVars: camara enlazada (DeviceId del LF_Camera) ----
    // Mismo patron que m_DeviceIdLow/High: par de int que forma "low:high".
    // 0:0 = sin enlace.
    protected int m_LinkedCamIdLow  = 0;
    protected int m_LinkedCamIdHigh = 0;

    // ---- Estado local (no sincronizado directamente) ----
    protected string m_DeviceId      = "";
    protected string m_LinkedCameraId = "";   // derivado de m_LinkedCamIdLow/High
    protected bool   m_LFPG_Deleting = false;

    // ============================================
    // Constructor - registro de SyncVars
    // Todos los RegisterNetSyncVariable* deben estar aqui, NO en EEInit.
    // ============================================
    void LF_Monitor()
    {
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
        RegisterNetSyncVariableInt("m_LinkedCamIdLow");
        RegisterNetSyncVariableInt("m_LinkedCamIdHigh");
    }

    // ============================================
    // Helpers de ID
    // ============================================
    protected void LFPG_UpdateDeviceIdString()
    {
        m_DeviceId = LFPG_Util.MakeDeviceKey(m_DeviceIdLow, m_DeviceIdHigh);
    }

    protected void LFPG_UpdateLinkedCameraIdString()
    {
        m_LinkedCameraId = LFPG_Util.MakeDeviceKey(m_LinkedCamIdLow, m_LinkedCamIdHigh);
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

        // Mantener la string derivada del enlace siempre actualizada.
        LFPG_UpdateLinkedCameraIdString();
    }

    // ============================================
    // API publica: enlace de camara
    // ============================================

    // Getter: devuelve el DeviceId de la camara enlazada, o "" si no hay.
    // Usado por ActionUnlinkCamera para la condicion de visibilidad.
    string LFPG_GetLinkedCameraId()
    {
        return m_LinkedCameraId;
    }

    // Setter: solo llamar desde el servidor (RPC handler CAMERA_CYCLE / CAMERA_UNLINK).
    // low=0, high=0 significa desvincular.
    void LFPG_SetLinkedCamera(int low, int high)
    {
        #ifdef SERVER
        if (m_LinkedCamIdLow == low && m_LinkedCamIdHigh == high)
            return;

        m_LinkedCamIdLow  = low;
        m_LinkedCamIdHigh = high;
        LFPG_UpdateLinkedCameraIdString();
        SetSynchDirty();

        string msg = "[LF_Monitor] SetLinkedCamera(" + low.ToString() + "," + high.ToString() + ")";
        msg = msg + " -> linkedId=" + m_LinkedCameraId + " monitorId=" + m_DeviceId;
        LFPG_Util.Info(msg);
        #endif
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
            SetSynchDirty();
        }
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
        LFPG_CameraViewport.Reset();
        #endif

        super.EEKilled(killer);
    }

    override void EEDelete(EntityAI parent)
    {
        m_LFPG_Deleting = true;
        LFPG_DeviceLifecycle.OnDeviceDeleted(this, m_DeviceId);
        #ifndef SERVER
        LFPG_CameraViewport.Reset();
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
    // Client sync
    // ============================================
    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();
        LFPG_TryRegister();

        // Etapa 2: la string derivada m_LinkedCameraId ya se actualiza
        // en LFPG_TryRegister() via LFPG_UpdateLinkedCameraIdString().

        // Etapa 3: material swap segun estado del monitor.
        // ON: encendido Y con camara enlazada (pantalla activa).
        // OFF: cualquier otro estado (pantalla apagada/estatica).
        #ifndef SERVER
        if (m_PoweredNet && m_LinkedCameraId != "")
        {
            SetObjectMaterial(0, LFPG_MONITOR_RVMAT_ON);
        }
        else
        {
            SetObjectMaterial(0, LFPG_MONITOR_RVMAT_OFF);
        }
        #endif
    }

    // ============================================
    // Persistence - CONSUMER: ids + m_PoweredNet + camara enlazada
    // Orden DEBE coincidir exactamente entre Save y Load.
    // ⚠ SAVE WIPE: campos m_LinkedCamIdLow/High son nuevos en v0.9.0.
    // ============================================
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);
        ctx.Write(m_DeviceIdLow);
        ctx.Write(m_DeviceIdHigh);
        ctx.Write(m_PoweredNet);
        ctx.Write(m_LinkedCamIdLow);
        ctx.Write(m_LinkedCamIdHigh);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version))
            return false;

        if (!ctx.Read(m_DeviceIdLow))
            return false;
        if (!ctx.Read(m_DeviceIdHigh))
            return false;
        if (!ctx.Read(m_PoweredNet))
            return false;
        if (!ctx.Read(m_LinkedCamIdLow))
            return false;
        if (!ctx.Read(m_LinkedCamIdHigh))
            return false;

        LFPG_UpdateLinkedCameraIdString();
        return true;
    }

    // ============================================
    // Guards de inventario
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

        // Etapa 2: acciones de emparejamiento.
        AddAction(LFPG_ActionCycleCamera);
        AddAction(LFPG_ActionUnlinkCamera);

        // Etapa 3: accion de vista de camara.
        AddAction(LFPG_ActionViewCamera);
    }

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

        LFPG_Util.Warn("[LF_Monitor] Missing memory point for port: " + portName);
        vector p = GetPosition();
        p[1] = p[1] + 0.5;
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
        return 20.0;
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

        string msg = "[LF_Monitor] SetPowered(" + powered.ToString() + ") id=" + m_DeviceId;
        LFPG_Util.Debug(msg);
        #endif
    }

    bool LFPG_CanConnectTo(Object other, string myPort, string otherPort)
    {
        return false;
    }

    bool LFPG_HasWireStore()
    {
        return false;
    }
};
