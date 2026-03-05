// =========================================================
// LF_PowerGrid - Camera device (v0.9.3 - Sync Audit fixes)
//
// LF_Camera_Kit:  Holdable, deployable (same-model pattern = Splitter/Combiner).
// LF_Camera:      CONSUMER, 1 IN (input_1), 15 u/s, no OUT, no wire store.
//
// Memory points (LOD Memory in p3d):
//   port_input_1  - upstream cable anchor
//
// Named selections (LOD Resolution in p3d):
//   cam_led       - LED indicator (hiddenSelections[0])
//
// Etapa 1: CONSUMER puro, deploy funcional, persistence, wiring OK.
// Etapa 3: rvmat swap activado en OnVariablesSynchronized.
//   LED rojo (lf_camera_led_on.rvmat)  cuando m_PoweredNet=true.
//   LED apagado (lf_camera_led_off.rvmat) cuando m_PoweredNet=false.
//   Assets en data/camera/ (NO data/cctv/).
//
// v0.9.3 (Sync Audit):
//   S3: m_PoweredNet removed from persistence — derived state.
//       Same "Ghost Lamp" bug as v0.7.42. Camera was the only device
//       still persisting m_PoweredNet, causing LED ON after restart
//       when source no longer exists.
//       ⚠ SAVE WIPE REQUIRED — schema change (field removed from stream).
// =========================================================

static const string LFPG_CAMERA_RVMAT_OFF = "\\LFPowerGrid\\data\\camera\\lf_camera_led_off.rvmat";
static const string LFPG_CAMERA_RVMAT_ON  = "\\LFPowerGrid\\data\\camera\\lf_camera_led_on.rvmat";

// ---------------------------------------------------------
// KIT - patron identico a LF_Splitter_Kit / LF_Combiner_Kit
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

    // Previene loop sound huerfano: ObjectDelete durante OnPlacementComplete
    // interrumpe el cleanup del action callback antes de detener el sonido.
    override string GetLoopDeploySoundset()
    {
        return "";
    }

    override void SetActions()
    {
        super.SetActions();
        AddAction(ActionTogglePlaceObject);
        AddAction(LFPG_ActionPlaceCamera);
    }

    // Usar parametro position/orientation, NUNCA GetPosition().
    // GetPosition() devuelve la pos del kit (cerca del player), no el hologram.
    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[Camera_Kit] OnPlacementComplete: param=" + position.ToString();
        tLog = tLog + " kitPos=" + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        EntityAI cam = GetGame().CreateObjectEx("LF_Camera", finalPos, ECE_CREATEPHYSICS);
        if (cam)
        {
            cam.SetPosition(finalPos);
            cam.SetOrientation(finalOri);
            cam.Update();

            string deployMsg = "[Camera_Kit] Deployed LF_Camera at " + finalPos.ToString();
            deployMsg = deployMsg + " ori=" + finalOri.ToString();
            LFPG_Util.Info(deployMsg);

            GetGame().ObjectDelete(this);
        }
        else
        {
            LFPG_Util.Error("[Camera_Kit] Failed to create LF_Camera! Kit preserved.");
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
// DEVICE - CONSUMER, 1 IN (input_1), 15 u/s
// ---------------------------------------------------------
class LF_Camera : Inventory_Base
{
    // ---- SyncVars ----
    protected int  m_DeviceIdLow  = 0;
    protected int  m_DeviceIdHigh = 0;
    protected bool m_PoweredNet   = false;

    // ---- Estado local ----
    protected string m_DeviceId      = "";
    protected bool   m_LFPG_Deleting = false;

    // ============================================
    // Constructor - registro de SyncVars
    // MUST be constructor, NOT EEInit.
    // ============================================
    void LF_Camera()
    {
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
    }

    // ============================================
    // Helpers de ID
    // ============================================
    protected void LFPG_UpdateDeviceIdString()
    {
        // LFPG_Util.MakeDeviceKey es la unica funcion correcta del codebase.
        m_DeviceId = LFPG_Util.MakeDeviceKey(m_DeviceIdLow, m_DeviceIdHigh);
    }

    protected void LFPG_TryRegister()
    {
        if (m_LFPG_Deleting)
            return;

        // Capturar el ID anterior antes de recalcular.
        // Si OnVariablesSynchronized llega con DeviceIdLow/High distintos
        // (SyncVar parcial / reciclado del motor), el ID antiguo debe
        // desregistrarse para evitar entradas fantasma en DeviceRegistry.
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
            // LFPG_Util.GenerateDeviceId es la funcion correcta del codebase.
            LFPG_Util.GenerateDeviceId(m_DeviceIdLow, m_DeviceIdHigh);
            SetSynchDirty();
        }
        #endif

        LFPG_UpdateDeviceIdString();
        LFPG_TryRegister();
        // CONSUMER: no BroadcastOwnerWires (no OUT port, no wire store)
    }

    override void EEKilled(Object killer)
    {
        // LFPG_DeviceLifecycle.OnDeviceKilled requiere (this, m_DeviceId).
        LFPG_DeviceLifecycle.OnDeviceKilled(this, m_DeviceId);

        #ifdef SERVER
        // Guard: evita SetSynchDirty() innecesario si ya estaba apagado.
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
        // Guard ANTES de OnDeviceDeleted.
        // Previene re-registro post-mortem via OnVariablesSynchronized.
        m_LFPG_Deleting = true;

        // OnDeviceDeleted cubre: wire cut + graph notification + unregister.
        // NO llamar solo a Unregister — perderiamos el cleanup del grafo.
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
        // Guard: evitar lifecycle antes de que el ID este asignado.
        if (m_DeviceId == "")
            return;

        // OnDeviceMoved requiere (this, m_DeviceId, oldLoc, newLoc).
        bool wiresCut = LFPG_DeviceLifecycle.OnDeviceMoved(this, m_DeviceId, oldLoc, newLoc);
        if (wiresCut)
        {
            // Guard: evita SetSynchDirty() innecesario.
            if (m_PoweredNet)
            {
                m_PoweredNet = false;
                SetSynchDirty();
            }
        }
        #endif
    }

    // ============================================
    // Client sync - delegar a LFPG_TryRegister (patron CeilingLight/Combiner)
    // v0.9.1 (H4 JIP Fix): Added RequestDeviceSync for cables
    // targeting this camera. Without it, cables from upstream
    // owners (Generator/Splitter) don't appear on JIP clients.
    // Pattern: LF_TestLamp parity (CONSUMER, no wire store).
    // ============================================
    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();
        LFPG_TryRegister();

        // Etapa 3: swap rvmat del LED segun estado de alimentacion.
        // cam_led = hiddenSelections[0] en config.cpp.
        // Guard #ifndef SERVER: SetObjectMaterial es exclusivamente client-side.
        #ifndef SERVER
        if (m_PoweredNet)
        {
            SetObjectMaterial(0, LFPG_CAMERA_RVMAT_ON);
        }
        else
        {
            SetObjectMaterial(0, LFPG_CAMERA_RVMAT_OFF);
        }

        // v0.9.1 (H4): Request wire data from server so cables
        // TOWARDS this camera render on JIP. Cooldown-throttled.
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
    // Persistence - CONSUMER: ids only.
    // v0.9.3 (S3 fix): m_PoweredNet removed from persistence.
    // It is a derived state from the electrical graph; persisting it
    // caused cameras to show LED ON after restart when their source
    // no longer exists (same "Ghost Lamp" bug fixed in v0.7.42).
    // Field default (false) is correct; propagation re-derives it.
    // ⚠ SAVE WIPE REQUIRED — schema change (field removed from stream).
    // Orden DEBE coincidir exactamente entre Save y Load.
    // ============================================
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);
        ctx.Write(m_DeviceIdLow);
        ctx.Write(m_DeviceIdHigh);
        // v0.9.3: m_PoweredNet no longer persisted — derived state.
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version))
            return false;

        if (!ctx.Read(m_DeviceIdLow))
            return false;
        if (!ctx.Read(m_DeviceIdHigh))
            return false;

        // v0.9.3: m_PoweredNet no longer persisted.
        // Field default (false) is correct; propagation re-derives it.

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
    }

    // Safety: bloquea CompEM vanilla.
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

    // Getters de los campos low/high — necesarios porque m_DeviceIdLow/High son
    // protected. PlayerRPC.HandleLFPG_CameraLink llama estos metodos directamente:
    //   monitor.LFPG_SetLinkedCamera(nextCam.LFPG_GetDeviceIdLow(), nextCam.LFPG_GetDeviceIdHigh())
    // Sin estos getters el script no compila.
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

        LFPG_Util.Warn("[LF_Camera] Missing memory point for port: " + portName);
        vector p = GetPosition();
        p[1] = p[1] + 0.3;
        return p;
    }

    int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.CAMERA;
    }

    bool LFPG_IsSource()
    {
        return false;
    }

    // OBLIGATORIO declarar explicitamente.
    // Sin esto DeviceAPI.GetConsumption() cae en IsEnergyConsumer()+10.0 por defecto.
    float LFPG_GetConsumption()
    {
        return 15.0;
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

        string msg = "[LF_Camera] SetPowered(" + powered.ToString() + ") id=" + m_DeviceId;
        LFPG_Util.Debug(msg);
        #endif
    }

    // CONSUMER - no tiene puerto OUT, no puede ser origen de conexion.
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
