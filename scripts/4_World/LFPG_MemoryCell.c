// =========================================================
// LF_PowerGrid — Memory Cell  v3.1.0
//
// PASSTHROUGH device: 4 inputs + 2 outputs.
//
//   input_0  — Power feed (standard PASSTHROUGH input).
//   input_1  — Toggle (rising-edge flips m_CellActive).
//   input_2  — Reset  (rising-edge forces m_CellActive=false).
//   input_3  — Set    (rising-edge forces m_CellActive=true).
//
//   output_0 — Normal output (powered when m_CellActive=true).
//   output_1 — Inverted output (powered when m_CellActive=false).
//
// All incoming power from input_0 is routed to whichever
// output is active.  The inactive output's edges are disabled
// (LFPG_EDGE_ENABLED cleared) so downstream sees zero power.
//
// Priority: Set > Reset > Toggle (within same tick).
//
// LED visual (single LED, hiddenSelection index 1 = light_led):
//   Red   = m_CellActive AND powered (input_0 receiving power)
//   Green = !m_CellActive AND powered
//   Off   = not powered (no input_0)
//
// Model: AND_OR_XOR_Memory_cell.p3d (shared with gates).
//   Memory points: port_input_0..3, port_output_0..1
//   HiddenSelections: camo (symbol), light_led_input0
//     (reused as single central LED)
//
// Capacity: 20 u/s.
// Self-consumption: 0.0 (pure passthrough).
//
// Persistence: DeviceId + wires JSON + m_CellActive.
//   Power states are derived, not persisted.
//   ⚠ Save wipe required (new entity type).
// =========================================================

// LED rvmat paths
static const string LFPG_MCELL_RVMAT_OFF   = "\\LFPowerGrid\\data\\button\\materials\\led_off.rvmat";
static const string LFPG_MCELL_RVMAT_GREEN  = "\\LFPowerGrid\\data\\button\\materials\\led_green.rvmat";
static const string LFPG_MCELL_RVMAT_RED    = "\\LFPowerGrid\\data\\button\\materials\\led_red.rvmat";

static const float LFPG_MCELL_CAPACITY = 20.0;

// =========================================================
// KIT
// =========================================================
class LFPG_MemoryCell_Kit : LFPG_LogicGate_Kit {};

// =========================================================
// ENTITY
// =========================================================
class LFPG_MemoryCell : Inventory_Base
{
    // ---- Device identity ----
    protected int m_DeviceIdLow = 0;
    protected int m_DeviceIdHigh = 0;
    protected string m_DeviceId;

    // ---- Wires owned (output side) ----
    protected ref array<ref LFPG_WireData> m_Wires;

    // ---- Power state (SyncVars) ----
    protected bool m_PoweredNet = false;
    protected bool m_Overloaded = false;

    // ---- Cell internal state (SyncVar + persisted) ----
    protected bool m_CellActive = false;

    // ---- Control input previous states (rising-edge detect) ----
    protected bool m_PrevToggle = false;
    protected bool m_PrevReset  = false;
    protected bool m_PrevSet    = false;

    // ---- Deletion guard ----
    protected bool m_LFPG_Deleting = false;

    // ---- Port routing applied flag ----
    protected bool m_RoutingApplied = false;

    // ---- Client visual cache ----
    protected int m_LastVisualState = -1;

    void LFPG_MemoryCell()
    {
        m_Wires = new array<ref LFPG_WireData>;
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
        RegisterNetSyncVariableBool("m_Overloaded");
        RegisterNetSyncVariableBool("m_CellActive");
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

        // Apply initial port routing after graph is built.
        // Deferred via CallLater to ensure graph edges exist.
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_DeferredRouting, 500, false);
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
    // LFPG_IDevice interface — 6 ports
    // ============================================
    string LFPG_GetDeviceId()
    {
        return m_DeviceId;
    }

    int LFPG_GetPortCount()
    {
        return 6;
    }

    string LFPG_GetPortName(int idx)
    {
        if (idx == 0) return "input_0";
        if (idx == 1) return "input_1";
        if (idx == 2) return "input_2";
        if (idx == 3) return "input_3";
        if (idx == 4) return "output_0";
        if (idx == 5) return "output_1";
        return "";
    }

    int LFPG_GetPortDir(int idx)
    {
        if (idx == 0) return LFPG_PortDir.IN;
        if (idx == 1) return LFPG_PortDir.IN;
        if (idx == 2) return LFPG_PortDir.IN;
        if (idx == 3) return LFPG_PortDir.IN;
        if (idx == 4) return LFPG_PortDir.OUT;
        if (idx == 5) return LFPG_PortDir.OUT;
        return -1;
    }

    string LFPG_GetPortLabel(int idx)
    {
        if (idx == 0) return "Power";
        if (idx == 1) return "Toggle";
        if (idx == 2) return "Reset";
        if (idx == 3) return "Set";
        if (idx == 4) return "Output";
        if (idx == 5) return "Inverted";
        return "";
    }

    bool LFPG_HasPort(string portName, int dir)
    {
        if (dir == LFPG_PortDir.IN)
        {
            if (portName == "input_0") return true;
            if (portName == "input_1") return true;
            if (portName == "input_2") return true;
            if (portName == "input_3") return true;
        }
        if (dir == LFPG_PortDir.OUT)
        {
            if (portName == "output_0") return true;
            if (portName == "output_1") return true;
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

        // Compact fallback (port_inputX without underscore before number)
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

        if (MemoryPointExists(portName))
        {
            return ModelToWorld(GetMemoryPointPos(portName));
        }

        string warnMsg = "[MemoryCell] Missing memory point for port: " + portName;
        LFPG_Util.Warn(warnMsg);
        vector p = GetPosition();
        p[1] = p[1] + 0.5;
        return p;
    }

    // ---- Source behavior (PASSTHROUGH) ----
    bool LFPG_IsSource()
    {
        return true;
    }

    float LFPG_GetCapacity()
    {
        return LFPG_MCELL_CAPACITY;
    }

    int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.PASSTHROUGH;
    }

    // OBLIGATORY: Explicit zero self-consumption.
    float LFPG_GetConsumption()
    {
        return 0.0;
    }

    bool LFPG_GetSourceOn()
    {
        return m_PoweredNet;
    }

    bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    // ---- NOT gated: routing is done via edge enable/disable ----
    bool LFPG_IsGateCapable()
    {
        return false;
    }

    bool LFPG_IsGateOpen()
    {
        return true;
    }

    // ============================================
    // Port routing — enable/disable output edges
    // ============================================
    protected void LFPG_ApplyRouting()
    {
        #ifdef SERVER
        if (m_DeviceId == "")
            return;

        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (!nm)
            return;

        LFPG_ElecGraph graph = nm.GetGraph();
        if (!graph)
            return;

        // output_0 enabled when active, output_1 enabled when inactive
        string portOut0 = "output_0";
        string portOut1 = "output_1";
        graph.SetOutputPortEnabled(m_DeviceId, portOut0, m_CellActive);
        graph.SetOutputPortEnabled(m_DeviceId, portOut1, !m_CellActive);

        m_RoutingApplied = true;

        string rLog = "[MemoryCell] ApplyRouting: active=";
        rLog = rLog + m_CellActive.ToString();
        rLog = rLog + " id=" + m_DeviceId;
        LFPG_Util.Debug(rLog);
        #endif
    }

    // ============================================
    // State change — deferred callback for post-wire routing.
    // Runs OUTSIDE the PDQ loop (via CallLater), so
    // RequestPropagate is safe and needed to kick the graph.
    // ============================================
    protected void LFPG_DeferredRouting()
    {
        #ifdef SERVER
        LFPG_ApplyRouting();

        if (m_DeviceId != "")
        {
            LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
            if (nm)
            {
                nm.RequestPropagate(m_DeviceId);
            }
        }
        #endif
    }

    // ============================================
    // Consumer behavior — called by graph PDQ
    // ============================================
    void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        bool changed = false;

        if (m_PoweredNet != powered)
        {
            m_PoweredNet = powered;
            changed = true;
        }

        // --- Rising-edge detection on control inputs ---
        // Priority: Set > Reset > Toggle (absolute — highest-priority
        // input wins even if current state already matches).
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm)
        {
            string portToggle = "input_1";
            string portReset  = "input_2";
            string portSet    = "input_3";

            bool curToggle = nm.IsPortReceivingPower(m_DeviceId, portToggle);
            bool curReset  = nm.IsPortReceivingPower(m_DeviceId, portReset);
            bool curSet    = nm.IsPortReceivingPower(m_DeviceId, portSet);

            // Compute desired state: highest-priority rising edge wins.
            // If none fire, newState stays equal to m_CellActive (no change).
            bool newState = m_CellActive;

            if (curSet && !m_PrevSet)
            {
                // Set rising edge → always active
                newState = true;
            }
            else if (curReset && !m_PrevReset)
            {
                // Reset rising edge → always inactive
                newState = false;
            }
            else if (curToggle && !m_PrevToggle)
            {
                // Toggle rising edge → flip
                if (m_CellActive)
                {
                    newState = false;
                }
                else
                {
                    newState = true;
                }
            }

            // Update edge memory BEFORE state change check
            m_PrevToggle = curToggle;
            m_PrevReset  = curReset;
            m_PrevSet    = curSet;

            // Apply state change if different
            if (newState != m_CellActive)
            {
                m_CellActive = newState;
                changed = true;

                // Update edge routing synchronously.
                // SetOutputPortEnabled calls MarkNodeDirty, which
                // re-queues this node in the PDQ dirty queue.
                // The NEXT PDQ iteration will use updated edge flags
                // for AllocateOutput — converges in 1-2 cycles.
                // Do NOT call RequestPropagate here — we are inside
                // PDQ and MarkNodeDirty is sufficient.
                LFPG_ApplyRouting();

                string scLog = "[MemoryCell] State changed: active=";
                scLog = scLog + m_CellActive.ToString();
                scLog = scLog + " id=" + m_DeviceId;
                LFPG_Util.Info(scLog);
            }
        }

        // Ensure routing is applied on first propagation
        if (!m_RoutingApplied)
        {
            LFPG_ApplyRouting();
        }

        if (changed)
        {
            SetSynchDirty();
        }
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
    // Connection validation
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
    // Wire ownership API
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
            // Default to output_0 if not specified
            string defaultPort = "output_0";
            wd.m_SourcePort = defaultPort;
        }

        if (!LFPG_HasPort(wd.m_SourcePort, LFPG_PortDir.OUT))
        {
            string warnMsg = "[MemoryCell] AddWire rejected: not an output port: ";
            warnMsg = warnMsg + wd.m_SourcePort;
            LFPG_Util.Warn(warnMsg);
            return false;
        }

        bool result = LFPG_WireHelper.AddWire(m_Wires, wd);
        if (result)
        {
            #ifdef SERVER
            SetSynchDirty();

            // Defer routing so graph edge is created first
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_DeferredRouting, 100, false);
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
    // ============================================
    // Visual update (client only)
    // ============================================
    protected void LFPG_UpdateVisuals()
    {
        #ifndef SERVER
        // hiddenSelections indices:
        //   0 = camo (symbol texture — force per-subclass to fix MLOD cache)
        //   1 = light_led_input0 (reused as single central LED)
        //   2 = light_led_input1 (unused)
        //   3 = light_led_output0 (unused)

        // Force correct symbol texture (MLOD cache bug: shared p3d with gates)
        string symTex = "\\LFPowerGrid\\data\\logic_gate\\data\\memory_cell_symbol_mem.paa";
        SetObjectTexture(0, symTex);

        int desiredState = 0; // off
        if (m_PoweredNet)
        {
            if (m_CellActive)
            {
                desiredState = 2; // red
            }
            else
            {
                desiredState = 1; // green
            }
        }

        if (desiredState == m_LastVisualState)
            return;

        m_LastVisualState = desiredState;

        if (desiredState == 2)
        {
            SetObjectMaterial(1, LFPG_MCELL_RVMAT_RED);
        }
        else if (desiredState == 1)
        {
            SetObjectMaterial(1, LFPG_MCELL_RVMAT_GREEN);
        }
        else
        {
            SetObjectMaterial(1, LFPG_MCELL_RVMAT_OFF);
        }

        // Unused LED slots — set once (first update only, via -1 init)
        SetObjectMaterial(2, LFPG_MCELL_RVMAT_OFF);
        SetObjectMaterial(3, LFPG_MCELL_RVMAT_OFF);
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
        ctx.Write(m_CellActive);

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
            LFPG_Util.Error("[MemoryCell] OnStoreLoad: failed to read m_DeviceIdLow");
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            LFPG_Util.Error("[MemoryCell] OnStoreLoad: failed to read m_DeviceIdHigh");
            return false;
        }

        if (!ctx.Read(m_CellActive))
        {
            LFPG_Util.Error("[MemoryCell] OnStoreLoad: failed to read m_CellActive");
            return false;
        }

        LFPG_UpdateDeviceIdString();

        string json;
        if (!ctx.Read(json))
        {
            string errMsg = "[MemoryCell] OnStoreLoad: failed to read wires json for ";
            errMsg = errMsg + m_DeviceId;
            LFPG_Util.Error(errMsg);
            return false;
        }
        string tag = "MemoryCell";
        LFPG_WireHelper.DeserializeJSON(m_Wires, json, tag);

        return true;
    }
};
