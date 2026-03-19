// =========================================================
// LF_PowerGrid - Monitor device (v0.9.3 - Sync Audit fixes)
//
// LF_Monitor_Kit: Holdable, deployable (same-model pattern).
// LF_Monitor:     PASSTHROUGH, 1 IN (input_1) + 4 OUT (output_1..4).
//                 Self-consumption: 10 u/s. Throughput cap: 70 u/s.
//                 OUT ports restricted to LF_Camera only (CanConnectTo).
//                 Owns wires on output side (same pattern as Splitter).
//
// v0.9.2 Sprint B: ActionWatchMonitor replaces ActionViewCamera.
//   Camera view is now server-authoritative (RPC REQUEST_CAMERA_LIST).
//   ActionCycleCamera and ActionUnlinkCamera removed (deprecated).
//
// v0.9.3 (Sync Audit):
//   S1: EEInit was calling nonexistent LFPG_WireHelper.BroadcastWires().
//       Fixed to LFPG_NetworkManager.Get().BroadcastOwnerWires(this).
//       Monitor wires were never broadcast to clients on startup/JIP.
//   S2: OnVarSync was calling nonexistent nm.RequestDeviceSync(this, id).
//       Fixed to CableRenderer.RequestDeviceSync(id, this) + visual notify.
//       Monitor cables were invisible on JIP until ReconcileTick (60s).
//
// ⚠ SAVE WIPE REQUERIDA — esquema incompatible con v0.9.0.
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
// DEVICE - PASSTHROUGH, 1 IN (input_1) + 4 OUT (output_1..4)
// Self-consumption: 10 u/s. Throughput cap: 70 u/s.
// Owns wires on output side (same pattern as Splitter).
// OUT ports restricted to LF_Camera only via CanConnectTo.
// ---------------------------------------------------------
class LF_Monitor : Inventory_Base
{
    // ---- SyncVars: identidad del dispositivo ----
    protected int  m_DeviceIdLow  = 0;
    protected int  m_DeviceIdHigh = 0;
    protected bool m_PoweredNet   = false;

    // ---- Wires owned (output side, same as Splitter/Generator) ----
    protected ref array<ref LFPG_WireData> m_Wires;

    // ---- SyncVars: overload/warning bitmasks (output wires) ----
    protected bool m_Overloaded = false;

    // ---- Estado local (no sincronizado directamente) ----
    protected string m_DeviceId      = "";
    protected bool   m_LFPG_Deleting = false;

    // ============================================
    // Constructor - registro de SyncVars
    // Todos los RegisterNetSyncVariable* deben estar aqui, NO en EEInit.
    // ============================================
    void LF_Monitor()
    {
        m_Wires = new array<ref LFPG_WireData>;
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
        RegisterNetSyncVariableBool("m_Overloaded");
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
        // v0.9.3 (Audit Fix #2): Unconditional SetSynchDirty for persistence load.
        SetSynchDirty();
        #endif

        LFPG_UpdateDeviceIdString();
        LFPG_TryRegister();

        #ifdef SERVER
        // PASSTHROUGH con wire store: broadcast wires persistidos a todos los clientes.
        // v0.9.3 (S1 fix): Was calling nonexistent LFPG_WireHelper.BroadcastWires().
        // Correct API is NetworkManager.BroadcastOwnerWires (handles empty arrays internally).
        LFPG_NetworkManager.Get().BroadcastOwnerWires(this);
        #endif
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
        LFPG_CameraViewport.SafeAbort();
        #endif

        super.EEKilled(killer);
    }

    override void EEDelete(EntityAI parent)
    {
        m_LFPG_Deleting = true;
        LFPG_DeviceLifecycle.OnDeviceDeleted(this, m_DeviceId);
        #ifndef SERVER
        LFPG_CameraViewport.SafeAbort();
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

        // Material swap segun estado del monitor.
        // ON: monitor alimentado (pantalla activa).
        // OFF: sin alimentacion (pantalla apagada).
        #ifndef SERVER
        if (m_PoweredNet)
        {
            SetObjectMaterial(0, LFPG_MONITOR_RVMAT_ON);
        }
        else
        {
            SetObjectMaterial(0, LFPG_MONITOR_RVMAT_OFF);
        }

        // v0.9.3 (S2 fix): Was calling nonexistent nm.RequestDeviceSync(this, m_DeviceId).
        // Correct API: CableRenderer.RequestDeviceSync(deviceId, entity).
        // Pattern matches Splitter/Combiner/CeilingLight parity.
        if (m_DeviceId != "")
        {
            LFPG_CableRenderer r = LFPG_CableRenderer.Get();
            if (r)
            {
                r.RequestDeviceSync(m_DeviceId, this);

                if (r.HasOwnerData(m_DeviceId))
                {
                    r.NotifyOwnerVisualChanged(m_DeviceId);
                }
            }
        }
        #endif
    }

    // ============================================
    // Persistence - PASSTHROUGH: ids + wires JSON
    // m_PoweredNet es estado derivado (propagacion lo recalcula).
    // ⚠ SAVE WIPE REQUERIDA — esquema incompatible con v0.9.0 anterior.
    // ============================================
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);
        ctx.Write(m_DeviceIdLow);
        ctx.Write(m_DeviceIdHigh);

        string json = "";
        LFPG_WireHelper.SerializeJSON(m_Wires, json);
        ctx.Write(json);

        // v1.3.1: Diagnostic logging for wire persistence investigation.
        string saveLog = "[LF_Monitor] OnStoreSave id=" + m_DeviceId;
        saveLog = saveLog + " wireCount=" + m_Wires.Count().ToString();
        saveLog = saveLog + " jsonLen=" + json.Length().ToString();
        LFPG_Util.Info(saveLog);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version))
            return false;

        if (!ctx.Read(m_DeviceIdLow))
        {
            LFPG_Util.Error("[LF_Monitor] OnStoreLoad: failed to read m_DeviceIdLow");
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            LFPG_Util.Error("[LF_Monitor] OnStoreLoad: failed to read m_DeviceIdHigh");
            return false;
        }

        LFPG_UpdateDeviceIdString();

        string json = "";
        if (!ctx.Read(json))
        {
            LFPG_Util.Error("[LF_Monitor] OnStoreLoad: failed to read wires json for " + m_DeviceId);
            return false;
        }
        LFPG_WireHelper.DeserializeJSON(m_Wires, json, "LF_Monitor");

        // v1.3.1: Diagnostic logging for wire persistence investigation.
        string loadLog = "[LF_Monitor] OnStoreLoad id=" + m_DeviceId;
        loadLog = loadLog + " wireCount=" + m_Wires.Count().ToString();
        loadLog = loadLog + " jsonLen=" + json.Length().ToString();
        LFPG_Util.Info(loadLog);

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

        // v0.9.2 Sprint B: ActionWatchMonitor (RPC-based, replaces ActionViewCamera).
        AddAction(LFPG_ActionWatchMonitor);
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
        return 5;
    }

    string LFPG_GetPortName(int idx)
    {
        if (idx == 0)
            return "input_1";
        if (idx == 1)
            return "output_1";
        if (idx == 2)
            return "output_2";
        if (idx == 3)
            return "output_3";
        if (idx == 4)
            return "output_4";
        return "";
    }

    int LFPG_GetPortDir(int idx)
    {
        if (idx == 0)
            return LFPG_PortDir.IN;
        if (idx >= 1 && idx <= 4)
            return LFPG_PortDir.OUT;
        return -1;
    }

    string LFPG_GetPortLabel(int idx)
    {
        if (idx == 0)
            return "Power Input";
        if (idx == 1)
            return "Camera 1";
        if (idx == 2)
            return "Camera 2";
        if (idx == 3)
            return "Camera 3";
        if (idx == 4)
            return "Camera 4";
        return "";
    }

    bool LFPG_HasPort(string portName, int dir)
    {
        if (dir == LFPG_PortDir.IN && portName == "input_1")
            return true;
        if (dir == LFPG_PortDir.OUT)
        {
            if (portName == "output_1")
                return true;
            if (portName == "output_2")
                return true;
            if (portName == "output_3")
                return true;
            if (portName == "output_4")
                return true;
        }
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
        return LFPG_DeviceType.PASSTHROUGH;
    }

    // PASSTHROUGH: retransmite energia hacia outputs.
    bool LFPG_IsSource()
    {
        return true;
    }

    // Source is "on" when receiving power from upstream.
    bool LFPG_GetSourceOn()
    {
        return m_PoweredNet;
    }
	
	float LFPG_GetCapacity()
    {
        return LFPG_MONITOR_THROUGHPUT;
    }
	
    // Autoconsumo del monitor.
    // ElecGraph v0.7.47 soporta m_Consumption > 0 en PASSTHROUGH.
    // Se descuenta del input antes de propagar a los outputs.
    float LFPG_GetConsumption()
    {
        return LFPG_MONITOR_CONSUMPTION;
    }

    bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    // Called by graph propagation when upstream power state changes.
    // PASSTHROUGH: no RequestPropagate here — graph handles dirty
    // propagation automatically via DIRTY_INPUT on downstream nodes.
    void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
        {
            string skipMsg = "[LF_Monitor] SetPowered(" + powered.ToString() + ") SKIP (no change) id=" + m_DeviceId;
            LFPG_Util.Debug(skipMsg);
            return;
        }

        m_PoweredNet = powered;
        SetSynchDirty();

        string msg = "[LF_Monitor] SetPowered(" + powered.ToString() + ") id=" + m_DeviceId;
        msg = msg + " wires=" + m_Wires.Count().ToString();
        LFPG_Util.Debug(msg);
        #endif
    }

    // Overload bitmask (which output wires exceed capacity)
    bool LFPG_GetOverloaded()
    {
        return m_Overloaded;
    }

    void LFPG_SetOverloaded(bool val)
    {
        #ifdef SERVER
        if (m_Overloaded != val)
        {
            m_Overloaded = val;
            SetSynchDirty();
        }
        #endif
    }


    // ---- Connection validation ----
    // OUT ports del monitor solo pueden conectarse a LF_Camera.
    bool LFPG_CanConnectTo(Object other, string myPort, string otherPort)
    {
        if (!other)
            return false;

        // Solo output ports pueden iniciar conexiones.
        if (!LFPG_HasPort(myPort, LFPG_PortDir.OUT))
            return false;

        // Restriccion: solo LF_Camera como destino.
        if (!other.IsKindOf("LF_Camera"))
            return false;

        EntityAI otherEntity = EntityAI.Cast(other);
        if (!otherEntity)
            return false;

        // Verificar que el destino tenga el puerto IN especificado.
        return LFPG_DeviceAPI.HasPort(other, otherPort, LFPG_PortDir.IN);
    }

    // ============================================
    // Wire ownership API (delegates to WireHelper)
    // Patron identico a LF_Splitter.
    // ============================================
    bool LFPG_HasWireStore()
    {
        return true;
    }

    array<ref LFPG_WireData> LFPG_GetWires()
    {
        return m_Wires;
    }

    string LFPG_GetWiresJSON()
    {
        return LFPG_WireHelper.GetJSON(m_Wires);
    }

    bool LFPG_AddWire(LFPG_WireData wd)
    {
        if (!wd)
            return false;

        // Solo permitir wires desde puertos output.
        if (wd.m_SourcePort == "")
        {
            wd.m_SourcePort = "output_1";
        }

        if (!LFPG_HasPort(wd.m_SourcePort, LFPG_PortDir.OUT))
        {
            LFPG_Util.Warn("[LF_Monitor] AddWire rejected: not an output port: " + wd.m_SourcePort);
            return false;
        }

        bool result = LFPG_WireHelper.AddWire(m_Wires, wd);
        if (result)
        {
            #ifdef SERVER
            SetSynchDirty();
            #endif
        }
        return result;
    }

    bool LFPG_ClearWires()
    {
        bool result = LFPG_WireHelper.ClearAll(m_Wires);
        if (result)
        {
            #ifdef SERVER
            SetSynchDirty();
            #endif
        }
        return result;
    }

    bool LFPG_ClearWiresForCreator(string creatorId)
    {
        bool result = LFPG_WireHelper.ClearForCreator(m_Wires, creatorId);
        if (result)
        {
            #ifdef SERVER
            SetSynchDirty();
            #endif
        }
        return result;
    }

    bool LFPG_PruneMissingTargets()
    {
        // Use cached map from NetworkManager if available (during self-heal),
        // otherwise build our own (standalone calls like wire creation).
        ref map<string, bool> validIds = LFPG_NetworkManager.Get().GetCachedValidIds();
        if (!validIds)
        {
            validIds = new map<string, bool>;
            array<EntityAI> all = new array<EntityAI>;
            LFPG_DeviceRegistry.Get().GetAll(all);
            int vi;
            for (vi = 0; vi < all.Count(); vi = vi + 1)
            {
                string did = LFPG_DeviceAPI.GetOrCreateDeviceId(all[vi]);
                if (did != "")
                {
                    validIds[did] = true;
                }
            }
        }

        bool result = LFPG_WireHelper.PruneMissingTargets(m_Wires, validIds);
        if (result)
        {
            #ifdef SERVER
            SetSynchDirty();
            #endif
        }
        return result;
    }
};
