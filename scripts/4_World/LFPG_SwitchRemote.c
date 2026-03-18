// =========================================================
// LF_PowerGrid - Switch V1 Remote / RF Toggle (v1.0.0)
//
// LFPG_SwitchRemote_Kit: Holdable, deployable (same-model pattern).
// LFPG_SwitchRemote:     PASSTHROUGH, 1 IN (input_1) + 1 OUT (output_1).
//                      Zero self-consumption. Momentary pulse (ON for 2s, then OFF).
//                      RF-CAPABLE: toggleable remotely via LF_Intercom broadcast.
//
// Model: switch_v1_remote.p3d (toggle switch with LED + antenna)
//   Memory points: port_input_0, port_output_0
//   Animation: "switch" (translation 0→0.5 via model.cfg)
//   Hidden selection index 2: light_led
//
// Behavior:
//   Toggle ON:  m_SwitchOn=true  -> power passes through -> LED green -> switch up
//   Toggle OFF: m_SwitchOn=false -> power blocked -> LED red -> switch down
//   No input:   LED off (disconnected)
//
// RF Remote:
//   LFPG_IsRFCapable() -> true
//   LFPG_RemoteToggle() -> calls LFPG_ToggleSwitch()
//   Intercom broadcast triggers toggle identically to physical action.
//
// LED states (hiddenSelections[2] = "light_led"):
//   Green = m_PoweredNet && m_SwitchOn  (passing power)
//   Red   = m_PoweredNet && !m_SwitchOn (has input, blocking)
//   Off   = !m_PoweredNet               (no upstream / disconnected)
//
// Port names: input_1/output_1 (LFPG logical names).
// GetPortWorldPos maps to p3d memory points port_input_0/port_output_0.
//
// Persistence: DeviceIdLow, DeviceIdHigh + wires JSON.
//   m_SwitchOn NOT persisted (momentary, defaults false on restart).
//   m_PoweredNet NOT persisted (derived by graph propagation).
//   Save wipe required (persistence format changed).
// =========================================================

static const string LFPG_SWREMOTE_RVMAT_OFF   = "\\LFPowerGrid\\data\\switch_v1_remote\\data\\led_off.rvmat";
static const string LFPG_SWREMOTE_RVMAT_GREEN  = "\\LFPowerGrid\\data\\switch_v1_remote\\data\\led_green.rvmat";
static const string LFPG_SWREMOTE_RVMAT_RED    = "\\LFPowerGrid\\data\\switch_v1_remote\\data\\switch_v1_remote_red.rvmat";

// ---------------------------------------------------------
// KIT - same-model deploy pattern
// ---------------------------------------------------------
class LFPG_SwitchRemote_Kit : Inventory_Base
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
        AddAction(LFPG_ActionPlaceSwitchRemote);
    }

    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[SwitchRemote_Kit] OnPlacementComplete: param=";
        tLog = tLog + position.ToString();
        tLog = tLog + " kitPos=";
        tLog = tLog + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        string spawnType = "LFPG_SwitchRemote";
        EntityAI sw = GetGame().CreateObjectEx(spawnType, finalPos, ECE_CREATEPHYSICS);
        if (sw)
        {
            sw.SetPosition(finalPos);
            sw.SetOrientation(finalOri);
            sw.Update();

            string deployMsg = "[SwitchRemote_Kit] Deployed LFPG_SwitchRemote at ";
            deployMsg = deployMsg + finalPos.ToString();
            deployMsg = deployMsg + " ori=";
            deployMsg = deployMsg + finalOri.ToString();
            LFPG_Util.Info(deployMsg);

            GetGame().ObjectDelete(this);
        }
        else
        {
            LFPG_Util.Error("[SwitchRemote_Kit] Failed to create LFPG_SwitchRemote! Kit preserved.");
            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                string failMsg = "[LFPG] Switch Remote placement failed. Kit preserved.";
                pb.MessageStatus(failMsg);
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// DEVICE: PASSTHROUGH (1 IN + 1 OUT), latching toggle, RF-capable
// ---------------------------------------------------------
class LFPG_SwitchRemote : Inventory_Base
{
    // ---- Device identity ----
    protected int m_DeviceIdLow = 0;
    protected int m_DeviceIdHigh = 0;
    protected string m_DeviceId;

    // ---- Wires owned (output side) ----
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
    void LFPG_SwitchRemote()
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
        AddAction(LFPG_ActionToggleSwitchRemote);
    }

    // ============================================
    // Inventory guards (prevent pickup)
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
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_PulseOff);
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
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_PulseOff);

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
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_PulseOff);
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
    // Visual update (client only)
    // ============================================
    protected void LFPG_UpdateVisuals()
    {
        #ifndef SERVER
        // LED rvmat swap (index 2: camo=0, camoswitch=1, light_led=2)
        if (m_PoweredNet && m_SwitchOn)
        {
            SetObjectMaterial(2, LFPG_SWREMOTE_RVMAT_GREEN);
        }
        else if (m_PoweredNet)
        {
            SetObjectMaterial(2, LFPG_SWREMOTE_RVMAT_RED);
        }
        else
        {
            SetObjectMaterial(2, LFPG_SWREMOTE_RVMAT_OFF);
        }

        // Switch animation: translation via model.cfg "switch" source
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
    // Toggle logic (called by action OR RF remote, server only)
    // ============================================
    void LFPG_ToggleSwitch()
    {
        #ifdef SERVER
        if (m_SwitchOn)
        {
            // Already pulsing — ignore (timer will reset it)
            return;
        }
        m_SwitchOn = true;
        SetSynchDirty();

        string togMsg = "[LFPG_SwitchRemote] Pulse ON id=";
        togMsg = togMsg + m_DeviceId;
        LFPG_Util.Info(togMsg);

        LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_PulseOff, LFPG_BUTTON_PULSE_MS, false);
        #endif
    }

    void LFPG_PulseOff()
    {
        #ifdef SERVER
        if (!m_SwitchOn)
            return;
        m_SwitchOn = false;
        SetSynchDirty();

        string offMsg = "[LFPG_SwitchRemote] Pulse OFF id=";
        offMsg = offMsg + m_DeviceId;
        LFPG_Util.Info(offMsg);

        LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
        #endif
    }

    // ============================================
    // RF Remote capability
    // ============================================
    bool LFPG_IsRFCapable()
    {
        return true;
    }

    // Called by LFPG_DeviceAPI.RemoteToggle() from Intercom broadcast.
    // Identical effect to physical toggle action.
    bool LFPG_RemoteToggle()
    {
        #ifdef SERVER
        LFPG_ToggleSwitch();

        string rfMsg = "[LFPG_SwitchRemote] RF RemoteToggle id=";
        rfMsg = rfMsg + m_DeviceId;
        LFPG_Util.Info(rfMsg);
        #endif

        return true;
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

    vector LFPG_GetPortWorldPos(string portName)
    {
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
            memPoint = "port_";
            memPoint = memPoint + portName;
        }

        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

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

    int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.PASSTHROUGH;
    }

    bool LFPG_IsSource()
    {
        return true;
    }

    bool LFPG_GetSourceOn()
    {
        return m_PoweredNet;
    }

    bool LFPG_IsGateOpen()
    {
        return m_SwitchOn;
    }

    bool LFPG_IsGateCapable()
    {
        return true;
    }

    float LFPG_GetConsumption()
    {
        return 0.0;
    }

    float LFPG_GetCapacity()
    {
        return LFPG_DEFAULT_PASSTHROUGH_CAPACITY;
    }

    bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    bool LFPG_GetSwitchOn()
    {
        return m_SwitchOn;
    }

    void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;

        if (!powered && m_SwitchOn)
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_PulseOff);
            m_SwitchOn = false;
        }

        SetSynchDirty();

        string pwrMsg = "[LFPG_SwitchRemote] SetPowered(";
        pwrMsg = pwrMsg + powered.ToString();
        pwrMsg = pwrMsg + ") id=";
        pwrMsg = pwrMsg + m_DeviceId;
        LFPG_Util.Debug(pwrMsg);
        #endif
    }

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
            wd.m_SourcePort = "output_1";
        }

        if (!LFPG_HasPort(wd.m_SourcePort, LFPG_PortDir.OUT))
        {
            string warnMsg = "[LFPG_SwitchRemote] AddWire rejected: not an output port: ";
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
    // Persistence
    // ============================================
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);

        ctx.Write(m_DeviceIdLow);
        ctx.Write(m_DeviceIdHigh);

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
            LFPG_Util.Error("[LFPG_SwitchRemote] OnStoreLoad: failed to read m_DeviceIdLow");
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            LFPG_Util.Error("[LFPG_SwitchRemote] OnStoreLoad: failed to read m_DeviceIdHigh");
            return false;
        }

        LFPG_UpdateDeviceIdString();

        string json = "";
        if (!ctx.Read(json))
        {
            LFPG_Util.Error("[LFPG_SwitchRemote] OnStoreLoad: failed to read wires json for " + m_DeviceId);
            return false;
        }
        LFPG_WireHelper.DeserializeJSON(m_Wires, json, "LFPG_SwitchRemote");

        return true;
    }
};
