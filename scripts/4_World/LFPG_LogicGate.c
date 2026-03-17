// =========================================================
// LF_PowerGrid - Logic Gates (AND / OR / XOR)  v1.7.0
//
// Three PASSTHROUGH devices sharing one model with different
// symbol textures.  Each gate has 2 inputs + 1 output and
// evaluates a boolean condition to allow or block power.
//
//   AND  — output active only when BOTH inputs receive power
//   OR   — output active when ANY input receives power
//   XOR  — output active when EXACTLY ONE input receives power
//
// Model: AND_OR_XOR_Memory_cell.p3d (shared by kit + entity)
//   Memory points: port_input_0, port_input_1, port_output_0
//   Skeleton: open_lid animation (latches + lid)
//   Sections: camo (symbol), light_led_input0/input1/output0
//
// Port names (LFPG logical):
//   input_0, input_1, output_0
//   Maps to memory points via "port_" + portName convention.
//
// Gate logic:
//   Uses LFPG_IsGateOpen() + LFPG_IsGateCapable() = true.
//   The ElecGraph calls IsGateOpen during ProcessDirtyQueue.
//   The gate queries per-port power via NetworkManager's
//   IsPortReceivingPower() and evaluates the boolean condition.
//
// Capacity: 20 u/s (user-requested).
// Self-consumption: 0 (pure passthrough).
//
// LED visual states (per hiddenSelection index):
//   idx 0 = camo (symbol texture, set by config, never changed)
//   idx 1 = light_led_input0  → green if input_0 powered, else off
//   idx 2 = light_led_input1  → green if input_1 powered, else off
//   idx 3 = light_led_output0 → green if gate open + powered,
//                                red if has input but gate closed,
//                                off if no input
//
// Persistence: DeviceIdLow, DeviceIdHigh + wires JSON.
//   m_PoweredNet NOT persisted (derived by graph propagation).
//   m_Input0Powered / m_Input1Powered NOT persisted (derived).
//   ⚠ Save wipe required (new entity types).
// =========================================================

// ---------------------------------------------------------
// LED rvmat paths (reuse button LED materials)
// ---------------------------------------------------------
static const string LFPG_GATE_RVMAT_OFF   = "\\LFPowerGrid\\data\\button\\materials\\led_off.rvmat";
static const string LFPG_GATE_RVMAT_GREEN = "\\LFPowerGrid\\data\\button\\materials\\led_green.rvmat";
static const string LFPG_GATE_RVMAT_RED   = "\\LFPowerGrid\\data\\button\\materials\\led_red.rvmat";

// Gate capacity (20 u/s as requested)
static const float LFPG_GATE_CAPACITY = 20.0;

// =========================================================
// KIT BASE — shared deployment logic for all 3 gate kits.
// OnPlacementComplete uses GetType() to determine which
// entity class to spawn.  Subclasses are empty.
// =========================================================
class LFPG_LogicGate_Kit : Inventory_Base
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

    // Prevent orphan loop sound — ObjectDelete during
    // OnPlacementComplete interrupts ActionContinuousBase cleanup.
    override string GetLoopDeploySoundset()
    {
        return "";
    }

    override void SetActions()
    {
        super.SetActions();
        AddAction(ActionTogglePlaceObject);
        AddAction(LFPG_ActionPlaceLogicGate);
    }

    // Determine which entity class to spawn based on kit type.
    // Uses GetType() to avoid virtual dispatch uncertainty.
    protected string LFPG_GetEntityClass()
    {
        string kitType = GetType();

        if (kitType == "LFPG_AND_Gate_Kit")
        {
            return "LFPG_AND_Gate";
        }

        if (kitType == "LFPG_OR_Gate_Kit")
        {
            return "LFPG_OR_Gate";
        }

        if (kitType == "LFPG_XOR_Gate_Kit")
        {
            return "LFPG_XOR_Gate";
        }

        if (kitType == "LFPG_MemoryCell_Kit")
        {
            return "LFPG_MemoryCell";
        }

        LFPG_Util.Error("[LogicGate_Kit] Unknown kit type: " + kitType);
        return "";
    }

    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        string entityClass = LFPG_GetEntityClass();
        if (entityClass == "")
        {
            LFPG_Util.Error("[LogicGate_Kit] Empty entity class — cannot spawn.");
            return;
        }

        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[LogicGate_Kit] OnPlacementComplete: type=";
        tLog = tLog + GetType();
        tLog = tLog + " entity=" + entityClass;
        tLog = tLog + " pos=" + position.ToString();
        LFPG_Util.Info(tLog);

        EntityAI gate = GetGame().CreateObjectEx(entityClass, finalPos, ECE_CREATEPHYSICS);
        if (gate)
        {
            gate.SetPosition(finalPos);
            gate.SetOrientation(finalOri);
            gate.Update();

            string successLog = "[LogicGate_Kit] Deployed ";
            successLog = successLog + entityClass;
            successLog = successLog + " at " + finalPos.ToString();
            LFPG_Util.Info(successLog);

            // Only delete kit on successful spawn.
            GetGame().ObjectDelete(this);
        }
        else
        {
            string failLog = "[LogicGate_Kit] Failed to create ";
            failLog = failLog + entityClass;
            failLog = failLog + "! Kit preserved.";
            LFPG_Util.Error(failLog);

            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                string msg = "[LFPG] Logic gate placement failed. Kit preserved.";
                pb.MessageStatus(msg);
            }
        }
        #endif
    }
};

// Empty subclasses — needed so DayZ script resolver maps each
// config class to a valid script class. All logic is in the base.
class LFPG_AND_Gate_Kit : LFPG_LogicGate_Kit {};
class LFPG_OR_Gate_Kit  : LFPG_LogicGate_Kit {};
class LFPG_XOR_Gate_Kit : LFPG_LogicGate_Kit {};


// =========================================================
// GATE BASE — PASSTHROUGH device (2 IN + 1 OUT) with gate
// logic based on per-input-port power state.
//
// Subclasses are empty; gate evaluation is dispatched via
// GetType() in LFPG_IsGateOpen().
// =========================================================
class LFPG_LogicGateBase : Inventory_Base
{
    // ---- Device identity ----
    protected int m_DeviceIdLow = 0;
    protected int m_DeviceIdHigh = 0;
    protected string m_DeviceId;

    // ---- Wires owned (output side) ----
    protected ref array<ref LFPG_WireData> m_Wires;

    // ---- Power state (SyncVars) ----
    protected bool m_PoweredNet = false;
    protected bool m_Input0Powered = false;
    protected bool m_Input1Powered = false;
    protected bool m_Overloaded = false;

    // ---- Deletion guard (RC-04 parity) ----
    protected bool m_LFPG_Deleting = false;

    void LFPG_LogicGateBase()
    {
        m_Wires = new array<ref LFPG_WireData>;
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
        RegisterNetSyncVariableBool("m_Input0Powered");
        RegisterNetSyncVariableBool("m_Input1Powered");
        RegisterNetSyncVariableBool("m_Overloaded");
    }

    override void SetActions()
    {
        super.SetActions();
        RemoveAction(ActionTakeItem);
        RemoveAction(ActionTakeItemToHands);
    }

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

    // ============================================
    // Lifecycle
    // ============================================
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

        #ifdef SERVER
        LFPG_NetworkManager.Get().BroadcastOwnerWires(this);
        #endif
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
        if (m_Input0Powered)
        {
            m_Input0Powered = false;
            dirty = true;
        }
        if (m_Input1Powered)
        {
            m_Input1Powered = false;
            dirty = true;
        }
        if (dirty)
        {
            SetSynchDirty();
        }
        #endif

        super.EEKilled(killer);
    }

    override void EEDelete(EntityAI parent)
    {
        m_LFPG_Deleting = true;
        LFPG_DeviceLifecycle.OnDeviceDeleted(this, m_DeviceId);
        super.EEDelete(parent);
    }

    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();
        LFPG_TryRegister();

        #ifndef SERVER
        LFPG_UpdateVisuals();

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
    // Identity helpers
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
    // LFPG_IDevice interface
    // ============================================
    string LFPG_GetDeviceId()
    {
        return m_DeviceId;
    }

    // 3 ports: 2 inputs + 1 output
    int LFPG_GetPortCount()
    {
        return 3;
    }

    string LFPG_GetPortName(int idx)
    {
        if (idx == 0) return "input_0";
        if (idx == 1) return "input_1";
        if (idx == 2) return "output_0";
        return "";
    }

    int LFPG_GetPortDir(int idx)
    {
        if (idx == 0) return LFPG_PortDir.IN;
        if (idx == 1) return LFPG_PortDir.IN;
        if (idx == 2) return LFPG_PortDir.OUT;
        return -1;
    }

    string LFPG_GetPortLabel(int idx)
    {
        if (idx == 0) return "Input A";
        if (idx == 1) return "Input B";
        if (idx == 2) return "Output";
        return "";
    }

    bool LFPG_HasPort(string portName, int dir)
    {
        if (dir == LFPG_PortDir.IN)
        {
            if (portName == "input_0") return true;
            if (portName == "input_1") return true;
        }
        if (dir == LFPG_PortDir.OUT)
        {
            if (portName == "output_0") return true;
        }
        return false;
    }

    vector LFPG_GetPortWorldPos(string portName)
    {
        // Primary: "port_" + portName convention
        string memPoint = "port_" + portName;
        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

        // Secondary: try without underscore before number
        int len = portName.Length();
        if (len >= 3)
        {
            string lastChar = portName.Substring(len - 1, 1);
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

        // Tertiary: exact portName
        if (MemoryPointExists(portName))
        {
            return ModelToWorld(GetMemoryPointPos(portName));
        }

        // Fallback: device center + offset
        string warnMsg = "[LogicGate] Missing memory point for port: " + portName;
        LFPG_Util.Warn(warnMsg);
        vector p = GetPosition();
        p[1] = p[1] + 0.5;
        return p;
    }

    // ---- Source behavior (PASSTHROUGH acts as source for downstream) ----
    bool LFPG_IsSource()
    {
        return true;
    }

    float LFPG_GetCapacity()
    {
        return LFPG_GATE_CAPACITY;
    }

    int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.PASSTHROUGH;
    }

    // OBLIGATORY: Explicit zero self-consumption.
    // Without this, LFPG_DeviceAPI.GetConsumption() falls through to
    // IsEnergyConsumer() (true, has IN port) and returns 10.0.
    float LFPG_GetConsumption()
    {
        return 0.0;
    }

    // Source is "on" when receiving power AND gate is open.
    bool LFPG_GetSourceOn()
    {
        return m_PoweredNet;
    }

    bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    // ============================================
    // Gate logic — dispatched via GetType()
    // ============================================

    // Called by ElecGraph ProcessDirtyQueue (server-side only).
    // Queries per-port power from the graph's incoming edges
    // and evaluates the boolean condition for the specific gate type.
    bool LFPG_IsGateOpen()
    {
        #ifdef SERVER
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (!nm)
            return false;

        string portIn0 = "input_0";
        string portIn1 = "input_1";
        bool in0 = nm.IsPortReceivingPower(m_DeviceId, portIn0);
        bool in1 = nm.IsPortReceivingPower(m_DeviceId, portIn1);

        string gateType = GetType();

        // AND: both inputs must have power
        if (gateType == "LFPG_AND_Gate")
        {
            if (in0 && in1)
            {
                return true;
            }
            return false;
        }

        // OR: any input with power
        if (gateType == "LFPG_OR_Gate")
        {
            if (in0 || in1)
            {
                return true;
            }
            return false;
        }

        // XOR: exactly one input with power
        if (gateType == "LFPG_XOR_Gate")
        {
            if (in0 && !in1)
            {
                return true;
            }
            if (!in0 && in1)
            {
                return true;
            }
            return false;
        }
        #endif

        return false;
    }

    bool LFPG_IsGateCapable()
    {
        return true;
    }

    // ============================================
    // Consumer behavior (upstream power state)
    // ============================================
    void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        // Update per-input SyncVars for client-side LED visuals.
        // At this point the graph has already computed allocations
        // for incoming edges, so IsPortReceivingPower is accurate.
        bool newIn0 = false;
        bool newIn1 = false;

        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm)
        {
            string portIn0 = "input_0";
            string portIn1 = "input_1";
            newIn0 = nm.IsPortReceivingPower(m_DeviceId, portIn0);
            newIn1 = nm.IsPortReceivingPower(m_DeviceId, portIn1);
        }

        bool changed = false;

        if (m_PoweredNet != powered)
        {
            m_PoweredNet = powered;
            changed = true;
        }

        if (m_Input0Powered != newIn0)
        {
            m_Input0Powered = newIn0;
            changed = true;
        }

        if (m_Input1Powered != newIn1)
        {
            m_Input1Powered = newIn1;
            changed = true;
        }

        if (changed)
        {
            SetSynchDirty();
        }

        string dbgLog = "[LogicGate] SetPowered(";
        dbgLog = dbgLog + powered.ToString();
        dbgLog = dbgLog + ") in0=" + newIn0.ToString();
        dbgLog = dbgLog + " in1=" + newIn1.ToString();
        dbgLog = dbgLog + " id=" + m_DeviceId;
        dbgLog = dbgLog + " type=" + GetType();
        LFPG_Util.Debug(dbgLog);
        #endif
    }

    // ---- Overload state ----
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

    // ============================================
    // Connection validation (Combiner parity)
    // ============================================
    bool LFPG_CanConnectTo(Object other, string myPort, string otherPort)
    {
        if (!other) return false;

        if (!LFPG_HasPort(myPort, LFPG_PortDir.OUT)) return false;

        EntityAI otherEntity = EntityAI.Cast(other);
        if (!otherEntity) return false;

        string otherId = LFPG_DeviceAPI.GetDeviceId(otherEntity);
        if (otherId != "")
        {
            return LFPG_DeviceAPI.HasPort(other, otherPort, LFPG_PortDir.IN);
        }

        return LFPG_DeviceAPI.IsEnergyConsumer(otherEntity);
    }

    // ============================================
    // Wire ownership API (Combiner parity)
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
        if (!wd) return false;

        if (wd.m_SourcePort == "")
        {
            string defaultPort = "output_0";
            wd.m_SourcePort = defaultPort;
        }

        if (!LFPG_HasPort(wd.m_SourcePort, LFPG_PortDir.OUT))
        {
            string warnMsg = "[LogicGate] AddWire rejected: not an output port: ";
            warnMsg = warnMsg + wd.m_SourcePort;
            LFPG_Util.Warn(warnMsg);
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

    // ============================================
    // Visual update (client only)
    // ============================================
    protected void LFPG_UpdateVisuals()
    {
        #ifndef SERVER
        // hiddenSelections indices:
        //   0 = camo (symbol texture, never changed)
        //   1 = light_led_input0
        //   2 = light_led_input1
        //   3 = light_led_output0

        // Input 0 LED: green if powered, off otherwise
        if (m_Input0Powered)
        {
            SetObjectMaterial(1, LFPG_GATE_RVMAT_GREEN);
        }
        else
        {
            SetObjectMaterial(1, LFPG_GATE_RVMAT_OFF);
        }

        // Input 1 LED: green if powered, off otherwise
        if (m_Input1Powered)
        {
            SetObjectMaterial(2, LFPG_GATE_RVMAT_GREEN);
        }
        else
        {
            SetObjectMaterial(2, LFPG_GATE_RVMAT_OFF);
        }

        // Output LED: green if gate condition met, red if has input but blocked, off if no input.
        // Evaluate gate condition client-side from synced per-input states.
        // m_PoweredNet cannot be used here because it equals "has any input"
        // regardless of gate state (graph sets newPowered before gate check).
        bool hasAnyInput = false;
        if (m_Input0Powered || m_Input1Powered)
        {
            hasAnyInput = true;
        }

        bool gateOpen = false;
        string gateType = GetType();

        if (gateType == "LFPG_AND_Gate")
        {
            if (m_Input0Powered && m_Input1Powered)
            {
                gateOpen = true;
            }
        }
        else if (gateType == "LFPG_OR_Gate")
        {
            if (m_Input0Powered || m_Input1Powered)
            {
                gateOpen = true;
            }
        }
        else if (gateType == "LFPG_XOR_Gate")
        {
            if (m_Input0Powered && !m_Input1Powered)
            {
                gateOpen = true;
            }
            if (!m_Input0Powered && m_Input1Powered)
            {
                gateOpen = true;
            }
        }

        if (gateOpen && hasAnyInput)
        {
            SetObjectMaterial(3, LFPG_GATE_RVMAT_GREEN);
        }
        else if (hasAnyInput)
        {
            SetObjectMaterial(3, LFPG_GATE_RVMAT_RED);
        }
        else
        {
            SetObjectMaterial(3, LFPG_GATE_RVMAT_OFF);
        }
        #endif
    }

    // ============================================
    // Persistence
    // ============================================
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);

        ctx.Write(m_DeviceIdLow);
        ctx.Write(m_DeviceIdHigh);
        // m_PoweredNet NOT persisted — derived by graph propagation.
        // m_Input0Powered / m_Input1Powered NOT persisted — derived.

        string json;
        LFPG_WireHelper.SerializeJSON(m_Wires, json);
        ctx.Write(json);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version))
            return false;

        if (!ctx.Read(m_DeviceIdLow))
        {
            LFPG_Util.Error("[LogicGate] OnStoreLoad: failed to read m_DeviceIdLow");
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            LFPG_Util.Error("[LogicGate] OnStoreLoad: failed to read m_DeviceIdHigh");
            return false;
        }

        LFPG_UpdateDeviceIdString();

        string json;
        if (!ctx.Read(json))
        {
            string errMsg = "[LogicGate] OnStoreLoad: failed to read wires json for ";
            errMsg = errMsg + m_DeviceId;
            LFPG_Util.Error(errMsg);
            return false;
        }
        string tag = "LogicGate";
        LFPG_WireHelper.DeserializeJSON(m_Wires, json, tag);

        return true;
    }
};

// =========================================================
// Concrete gate subclasses — empty, needed for DayZ config
// class → script class resolution.  All logic is in the base
// class, dispatched via GetType().
// =========================================================
class LFPG_AND_Gate : LFPG_LogicGateBase {};
class LFPG_OR_Gate  : LFPG_LogicGateBase {};
class LFPG_XOR_Gate : LFPG_LogicGateBase {};
