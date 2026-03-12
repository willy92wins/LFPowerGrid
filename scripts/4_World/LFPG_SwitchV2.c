// =========================================================
// LF_PowerGrid - Switch V2 / Latching Toggle (v1.6.0)
//
// LFPG_SwitchV2_Kit: Holdable, deployable (same-model pattern = Splitter).
// LFPG_SwitchV2:     PASSTHROUGH, 1 IN (input_1) + 1 OUT (output_1).
//                     Zero self-consumption. Latching toggle (stays ON/OFF).
//
// Model: switch_v2.p3d (lever switch with LED indicator)
//   Memory points: port_input_0, port_output_0
//   Animation: "switch" (rotation 0→-3.0 rad via model.cfg)
//   Hidden selection index 0: light_led
//
// Behavior:
//   Toggle ON:  m_SwitchOn=true  → power passes through → LED green → lever up
//   Toggle OFF: m_SwitchOn=false → power blocked → LED red → lever down
//   No input:   LED off (disconnected)
//
// LED states (hiddenSelections[0] = "light_led"):
//   Green = m_PoweredNet && m_SwitchOn  (passing power)
//   Red   = m_PoweredNet && !m_SwitchOn (has input, blocking)
//   Off   = !m_PoweredNet               (no upstream / disconnected)
//
// Port names: input_1/output_1 (LFPG logical names).
// GetPortWorldPos maps to p3d memory points port_input_0/port_output_0.
//
// Persistence: DeviceIdLow, DeviceIdHigh, m_SwitchOn + wires JSON.
//   m_SwitchOn IS persisted (latching state survives restart).
//   m_PoweredNet NOT persisted (derived by graph propagation).
//   ⚠ Save wipe required (new persistence format).
// =========================================================

static const string LFPG_SWITCHV2_RVMAT_OFF   = "\\LFPowerGrid\\switch_v2\\data\\led_off.rvmat";
static const string LFPG_SWITCHV2_RVMAT_GREEN  = "\\LFPowerGrid\\switch_v2\\data\\led_green.rvmat";
static const string LFPG_SWITCHV2_RVMAT_RED    = "\\LFPowerGrid\\switch_v2\\data\\led_red.rvmat";

// ---------------------------------------------------------
// KIT - same-model deploy pattern (Splitter/Combiner parity)
// ---------------------------------------------------------
class LFPG_SwitchV2_Kit : Inventory_Base
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
        AddAction(LFPG_ActionPlaceSwitchV2);
    }

    // Usar parametro position/orientation, NUNCA GetPosition().
    // GetPosition() devuelve la pos del kit (cerca del player), no el hologram.
    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[SwitchV2_Kit] OnPlacementComplete: param=" + position.ToString();
        tLog = tLog + " kitPos=" + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        // No ECE_PLACE_ON_SURFACE — mata wall placement.
        EntityAI sw = GetGame().CreateObjectEx("LFPG_SwitchV2", finalPos, ECE_CREATEPHYSICS);
        if (sw)
        {
            sw.SetPosition(finalPos);
            sw.SetOrientation(finalOri);
            sw.Update();

            string deployMsg = "[SwitchV2_Kit] Deployed LFPG_SwitchV2 at " + finalPos.ToString();
            deployMsg = deployMsg + " ori=" + finalOri.ToString();
            LFPG_Util.Info(deployMsg);

            // Solo borrar kit si spawn exitoso.
            GetGame().ObjectDelete(this);
        }
        else
        {
            LFPG_Util.Error("[SwitchV2_Kit] Failed to create LFPG_SwitchV2! Kit preserved.");
            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                pb.MessageStatus("[LFPG] Switch placement failed. Kit preserved.");
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// DEVICE: PASSTHROUGH (1 IN + 1 OUT), latching toggle
// ---------------------------------------------------------
class LFPG_SwitchV2 : Inventory_Base
{
    // ---- Device identity ----
    protected int m_DeviceIdLow = 0;
    protected int m_DeviceIdHigh = 0;
    protected string m_DeviceId;

    // ---- Wires owned (output side, same as Splitter/Generator) ----
    protected ref array<ref LFPG_WireData> m_Wires;

    // ---- Power state (set by graph propagation) ----
    protected bool m_PoweredNet = false;

    // ---- Switch state (latching: stays ON until toggled OFF) ----
    protected bool m_SwitchOn = false;

    // ---- Overload state ----
    protected bool m_Overloaded = false;

    // ---- Deletion guard ----
    protected bool m_LFPG_Deleting = false;

    // ============================================
    // Constructor - SyncVars en constructor, NO EEInit
    // ============================================
    void LFPG_SwitchV2()
    {
        m_Wires = new array<ref LFPG_WireData>;
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
        RegisterNetSyncVariableBool("m_SwitchOn");
        RegisterNetSyncVariableBool("m_Overloaded");
    }

    // ============================================
    // Actions
    // ============================================
    override void SetActions()
    {
        super.SetActions();
        RemoveAction(ActionTakeItem);
        RemoveAction(ActionTakeItemToHands);
        AddAction(LFPG_ActionToggleSwitchV2);
    }

    // ============================================
    // Inventory guards (prevent pickup — breaks wires)
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

    // ============================================
    // Device ID helpers
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
        if (m_SwitchOn)
        {
            m_SwitchOn = false;
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

    override void EEItemLocationChanged(notnull InventoryLocation oldLoc, notnull InventoryLocation newLoc)
    {
        super.EEItemLocationChanged(oldLoc, newLoc);

        #ifdef SERVER
        if (m_DeviceId == "")
            return;

        bool wiresCut = LFPG_DeviceLifecycle.OnDeviceMoved(this, m_DeviceId, oldLoc, newLoc);
        if (wiresCut)
        {
            bool locDirty = false;
            if (m_PoweredNet)
            {
                m_PoweredNet = false;
                locDirty = true;
            }
            if (m_SwitchOn)
            {
                m_SwitchOn = false;
                locDirty = true;
            }
            if (locDirty)
            {
                SetSynchDirty();
            }
        }
        #endif
    }

    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();
        LFPG_TryRegister();

        #ifndef SERVER
        // LED + animation update
        LFPG_UpdateVisuals();

        // CableRenderer sync (Splitter parity)
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
    // Visual update (client only)
    // ============================================
    protected void LFPG_UpdateVisuals()
    {
        #ifndef SERVER
        // LED rvmat swap (index 1: camo=0, light_led=1)
        if (m_PoweredNet && m_SwitchOn)
        {
            SetObjectMaterial(1, LFPG_SWITCHV2_RVMAT_GREEN);
        }
        else if (m_PoweredNet)
        {
            SetObjectMaterial(1, LFPG_SWITCHV2_RVMAT_RED);
        }
        else
        {
            SetObjectMaterial(1, LFPG_SWITCHV2_RVMAT_OFF);
        }

        // Lever animation: rotation via model.cfg "switch" source
        if (m_SwitchOn)
        {
            SetAnimationPhase("switch", 1.0);
        }
        else
        {
            SetAnimationPhase("switch", 0.0);
        }
        #endif
    }

    // ============================================
    // Toggle logic (called by action, server only)
    // ============================================
    void LFPG_ToggleSwitch()
    {
        #ifdef SERVER
        // Latching toggle: flip state
        if (m_SwitchOn)
        {
            m_SwitchOn = false;
        }
        else
        {
            m_SwitchOn = true;
        }
        SetSynchDirty();

        string togMsg = "[LFPG_SwitchV2] Toggle ";
        if (m_SwitchOn)
        {
            togMsg = togMsg + "ON";
        }
        else
        {
            togMsg = togMsg + "OFF";
        }
        togMsg = togMsg + " id=" + m_DeviceId;
        LFPG_Util.Info(togMsg);

        // Propagate immediately so downstream updates
        LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
        #endif
    }

    // ============================================
    // LFPG_IDevice interface
    // ============================================
    string LFPG_GetDeviceId()
    {
        return m_DeviceId;
    }

    // 2 ports: 1 input + 1 output
    int LFPG_GetPortCount()
    {
        return 2;
    }

    string LFPG_GetPortName(int idx)
    {
        if (idx == 0) return "input_1";
        if (idx == 1) return "output_1";
        return "";
    }

    int LFPG_GetPortDir(int idx)
    {
        if (idx == 0) return LFPG_PortDir.IN;
        if (idx == 1) return LFPG_PortDir.OUT;
        return -1;
    }

    string LFPG_GetPortLabel(int idx)
    {
        if (idx == 0) return "Input 1";
        if (idx == 1) return "Output 1";
        return "";
    }

    bool LFPG_HasPort(string portName, int dir)
    {
        if (dir == LFPG_PortDir.IN && portName == "input_1") return true;
        if (dir == LFPG_PortDir.OUT && portName == "output_1") return true;
        return false;
    }

    // Port positions: switch_v2.p3d memory points
    // port_input_0 / port_output_0 in p3d
    // LFPG port names remain input_1/output_1
    vector LFPG_GetPortWorldPos(string portName)
    {
        // Map LFPG port names → p3d memory point names
        string memPoint;
        if (portName == "input_1")
        {
            memPoint = "port_input_0";
        }
        else if (portName == "output_1")
        {
            memPoint = "port_output_0";
        }
        else
        {
            memPoint = "port_" + portName;
        }

        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

        // Fallback: virtual offsets if memory points missing
        vector offset = "0 0.02 0";
        if (portName == "input_1")
        {
            offset = "0 0.02 -0.025";
        }
        else if (portName == "output_1")
        {
            offset = "0 0.02 0.025";
        }

        return ModelToWorld(offset);
    }

    // ---- PASSTHROUGH device type ----
    int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.PASSTHROUGH;
    }

    // ---- Source behavior ----
    bool LFPG_IsSource()
    {
        return true;
    }

    // Source ON when receiving power from upstream.
    // Gate logic handled by LFPG_IsGateOpen(), not here.
    bool LFPG_GetSourceOn()
    {
        return m_PoweredNet;
    }

    // Gated passthrough: when false, ElecGraph sets newOutput=0
    // even if inputSum > 0.
    bool LFPG_IsGateOpen()
    {
        return m_SwitchOn;
    }

    // Signal to ElecGraph that this device has gate logic.
    // Cached in LFPG_ElecNode.m_IsGated.
    bool LFPG_IsGateCapable()
    {
        return true;
    }

    // Zero self-consumption: pure relay.
    // Without this, DeviceAPI fallback returns 10.0 (IsEnergyConsumer=true).
    float LFPG_GetConsumption()
    {
        return 0.0;
    }

    // Throughput capacity (same as default passthrough).
    float LFPG_GetCapacity()
    {
        return LFPG_DEFAULT_PASSTHROUGH_CAPACITY;
    }

    // Expose powered state for inspector / graph queries.
    bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    // Expose switch state for toggle action text.
    bool LFPG_GetSwitchOn()
    {
        return m_SwitchOn;
    }

    // Called by graph propagation when upstream power state changes.
    void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;
        SetSynchDirty();

        string pwrMsg = "[LFPG_SwitchV2] SetPowered(" + powered.ToString() + ") id=" + m_DeviceId;
        LFPG_Util.Debug(pwrMsg);
        #endif
    }

    // Overload state
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
    // Wire ownership API (Splitter parity)
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
            wd.m_SourcePort = "output_1";
        }

        if (!LFPG_HasPort(wd.m_SourcePort, LFPG_PortDir.OUT))
        {
            LFPG_Util.Warn("[LFPG_SwitchV2] AddWire rejected: not an output port: " + wd.m_SourcePort);
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
    // Persistence
    // ============================================
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);

        ctx.Write(m_DeviceIdLow);
        ctx.Write(m_DeviceIdHigh);

        // m_SwitchOn IS persisted (latching state survives restart)
        ctx.Write(m_SwitchOn);

        // m_PoweredNet: NOT persisted (derived state, graph recalculates)

        string json = "";
        LFPG_WireHelper.SerializeJSON(m_Wires, json);
        ctx.Write(json);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version))
            return false;

        if (!ctx.Read(m_DeviceIdLow))
        {
            LFPG_Util.Error("[LFPG_SwitchV2] OnStoreLoad: failed to read m_DeviceIdLow");
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            LFPG_Util.Error("[LFPG_SwitchV2] OnStoreLoad: failed to read m_DeviceIdHigh");
            return false;
        }

        // Latching state
        if (!ctx.Read(m_SwitchOn))
        {
            LFPG_Util.Error("[LFPG_SwitchV2] OnStoreLoad: failed to read m_SwitchOn");
            return false;
        }

        LFPG_UpdateDeviceIdString();

        string json = "";
        if (!ctx.Read(json))
        {
            LFPG_Util.Error("[LFPG_SwitchV2] OnStoreLoad: failed to read wires json for " + m_DeviceId);
            return false;
        }
        LFPG_WireHelper.DeserializeJSON(m_Wires, json, "LFPG_SwitchV2");

        return true;
    }
};
