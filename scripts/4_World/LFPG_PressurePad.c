// =========================================================
// LF_PowerGrid - Pressure Pad (v1.8.0)
//
// LFPG_PressurePad_Kit: Holdable, deployable (same-model pattern).
// LFPG_PressurePad:     PASSTHROUGH, 1 IN (input_1) + 1 OUT (output_1).
//                        Self-consumption: 5 u/s. Gated passthrough.
//                        Throughput cap: 20 u/s.
//
// Behavior:
//   Centralized tick in NetworkManager scans nearby players.
//   If any alive player is standing on the pad (XZ radius 0.40m,
//   Y range -0.1 to +0.3 above pad), gate opens (m_GateOpen=true)
//   and power passes through. Otherwise gate closes.
//
//   No LOS raycast (physical contact only).
//   No group filter (any player activates).
//
// Animation:
//   "pad_press" (translation via model.cfg pressure_pad_axis).
//   GateOpen=true → phase 1.0 (pad depressed).
//   GateOpen=false → phase 0.0 (pad raised).
//
// Sound:
//   One-shot "LFPG_PressurePad_Press_SoundSet" on gate open
//   (client-side, triggered by OnVariablesSynchronized transition).
//
// Port positions: Memory points port_input_0, port_output_0 in p3d.
//
// Persistence: DeviceIdLow, DeviceIdHigh, wiresJSON.
//   m_GateOpen NOT persisted (derived by scan tick).
//   m_PoweredNet NOT persisted (derived by graph propagation).
//
// SAVE WIPE REQUIRED — new persistence schema.
// =========================================================

// Pad detection constants
static const float LFPG_PAD_RADIUS_SQ      = 0.16;  // 0.40m radius squared
static const float LFPG_PAD_Y_MIN          = -0.1;  // player can be slightly below pad
static const float LFPG_PAD_Y_MAX          = 0.3;   // player standing on pad
static const float LFPG_PAD_CONSUMPTION    = 5.0;   // self-consumption (u/s)
static const float LFPG_PAD_CAPACITY       = 20.0;  // max throughput (u/s)

// Sound
static const string LFPG_PAD_PRESS_SOUNDSET = "LFPG_PressurePad_Press_SoundSet";

// ---------------------------------------------------------
// KIT - same-model deploy pattern (Splitter/PushButton parity)
// ---------------------------------------------------------
class LFPG_PressurePad_Kit : Inventory_Base
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
        AddAction(LFPG_ActionPlacePressurePad);
    }

    // Usar parametro position/orientation, NUNCA GetPosition().
    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[PressurePad_Kit] OnPlacementComplete: param=";
        tLog = tLog + position.ToString();
        tLog = tLog + " kitPos=" + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        EntityAI pad = GetGame().CreateObjectEx("LFPG_PressurePad", finalPos, ECE_CREATEPHYSICS);
        if (pad)
        {
            pad.SetPosition(finalPos);
            pad.SetOrientation(finalOri);
            pad.Update();

            string deployMsg = "[PressurePad_Kit] Deployed LFPG_PressurePad at ";
            deployMsg = deployMsg + finalPos.ToString();
            deployMsg = deployMsg + " ori=" + finalOri.ToString();
            LFPG_Util.Info(deployMsg);

            // Solo borrar kit si spawn exitoso.
            GetGame().ObjectDelete(this);
        }
        else
        {
            LFPG_Util.Error("[PressurePad_Kit] Failed to create LFPG_PressurePad! Kit preserved.");
            PlayerBase pbFail = PlayerBase.Cast(player);
            if (pbFail)
            {
                pbFail.MessageStatus("[LFPG] Pressure Pad placement failed. Kit preserved.");
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// DEVICE: PASSTHROUGH (1 IN + 1 OUT), gated by player presence
// ---------------------------------------------------------
class LFPG_PressurePad : Inventory_Base
{
    // ---- Device identity ----
    protected int m_DeviceIdLow = 0;
    protected int m_DeviceIdHigh = 0;
    protected string m_DeviceId;

    // ---- Wires owned (output side) ----
    protected ref array<ref LFPG_WireData> m_Wires;

    // ---- Power state (set by graph propagation) ----
    protected bool m_PoweredNet = false;

    // ---- Gate state (set by scan tick) ----
    protected bool m_GateOpen = false;

    // ---- Overload state ----
    protected bool m_Overloaded = false;

    // ---- Deletion guard ----
    protected bool m_LFPG_Deleting = false;

    // ---- Client-side previous gate state (for sound trigger) ----
    protected bool m_PrevGateOpen = false;

    // ============================================
    // Constructor - SyncVars en constructor, NO EEInit
    // ============================================
    void LFPG_PressurePad()
    {
        m_Wires = new array<ref LFPG_WireData>;
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
        RegisterNetSyncVariableBool("m_GateOpen");
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
        // Register with centralized detection tick
        LFPG_NetworkManager.Get().RegisterPressurePad(this);
        #endif
    }

    override void EEKilled(Object killer)
    {
        LFPG_DeviceLifecycle.OnDeviceKilled(this, m_DeviceId);

        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterPressurePad(this);

        bool dirty = false;
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            dirty = true;
        }
        if (m_GateOpen)
        {
            m_GateOpen = false;
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

        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterPressurePad(this);
        #endif

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
            if (m_GateOpen)
            {
                m_GateOpen = false;
                locDirty = true;
            }
            if (locDirty)
            {
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

        #ifndef SERVER
        // Detect gate transition for one-shot press sound
        bool gateJustOpened = false;
        if (m_GateOpen && !m_PrevGateOpen)
        {
            gateJustOpened = true;
        }
        m_PrevGateOpen = m_GateOpen;

        // Visual + animation update
        LFPG_UpdateVisuals();

        // One-shot press sound on gate open transition
        if (gateJustOpened)
        {
            SEffectManager.PlaySound(LFPG_PAD_PRESS_SOUNDSET, GetPosition());
        }

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
        // Animation: pad depression
        string phaseName = "pad_press";
        if (m_GateOpen)
        {
            SetAnimationPhase(phaseName, 1.0);
        }
        else
        {
            SetAnimationPhase(phaseName, 0.0);
        }
        #endif
    }

    // ============================================
    // Detection — called by LFPG_TickPressurePads
    // ============================================

    // Called by centralized tick in NetworkManager (server only).
    // Receives shared player list to avoid N GetPlayers() calls per tick.
    // Returns true if gate state changed (needs propagation).
    bool LFPG_EvaluatePresence(array<Man> players)
    {
        #ifdef SERVER
        // Presence detection runs ALWAYS — gate is a physical sensor.
        // Sound plays on gate transition regardless of power state.
        // Power only affects whether the graph propagates through the gate.

        vector padPos = GetPosition();
        float padX = padPos[0];
        float padY = padPos[1];
        float padZ = padPos[2];

        bool detected = false;

        int i;
        int pCount = players.Count();
        Man man;
        PlayerBase pb;
        vector playerPos;
        float dx;
        float dz;
        float distXZsq;
        float dy;

        for (i = 0; i < pCount; i = i + 1)
        {
            if (detected)
                break;

            man = players[i];
            if (!man)
                continue;

            if (!man.IsAlive())
                continue;

            pb = PlayerBase.Cast(man);
            if (!pb)
                continue;

            // 1. XZ plane distance check (squared, no sqrt)
            playerPos = pb.GetPosition();
            dx = playerPos[0] - padX;
            dz = playerPos[2] - padZ;
            distXZsq = dx * dx + dz * dz;
            if (distXZsq > LFPG_PAD_RADIUS_SQ)
                continue;

            // 2. Height check — player must be "standing on" the pad
            dy = playerPos[1] - padY;
            if (dy < LFPG_PAD_Y_MIN)
                continue;
            if (dy > LFPG_PAD_Y_MAX)
                continue;

            // Player is on the pad
            detected = true;
        }

        // Update gate state
        bool oldGate = m_GateOpen;
        m_GateOpen = detected;

        if (m_GateOpen != oldGate)
        {
            SetSynchDirty();
            return true;
        }

        return false;
        #else
        return false;
        #endif
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

    // Port positions from p3d memory points
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
            offset = "0 0.02 -0.25";
        }
        else if (portName == "output_1")
        {
            offset = "0 0.02 0.25";
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

    bool LFPG_GetSourceOn()
    {
        return m_PoweredNet;
    }

    // Gated passthrough: gate controlled by presence detection
    bool LFPG_IsGateOpen()
    {
        return m_GateOpen;
    }

    bool LFPG_IsGateCapable()
    {
        return true;
    }

    // 5 u/s self-consumption
    float LFPG_GetConsumption()
    {
        return LFPG_PAD_CONSUMPTION;
    }

    // 20 u/s max throughput
    float LFPG_GetCapacity()
    {
        return LFPG_PAD_CAPACITY;
    }

    bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    // Called by graph propagation when upstream power state changes.
    void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;

        // Gate is NOT closed on power loss — it is a physical sensor.
        // Power only affects whether the graph propagates through.

        SetSynchDirty();

        string pwrMsg = "[LFPG_PressurePad] SetPowered(";
        pwrMsg = pwrMsg + powered.ToString();
        pwrMsg = pwrMsg + ") id=" + m_DeviceId;
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
            string warnMsg = "[LFPG_PressurePad] AddWire rejected: not an output port: ";
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
        // m_PoweredNet: NOT persisted (derived by graph propagation)
        // m_GateOpen:   NOT persisted (derived by scan tick)

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
            LFPG_Util.Error("[LFPG_PressurePad] OnStoreLoad: failed to read m_DeviceIdLow");
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            LFPG_Util.Error("[LFPG_PressurePad] OnStoreLoad: failed to read m_DeviceIdHigh");
            return false;
        }

        LFPG_UpdateDeviceIdString();

        string json = "";
        if (!ctx.Read(json))
        {
            LFPG_Util.Error("[LFPG_PressurePad] OnStoreLoad: failed to read wires json for " + m_DeviceId);
            return false;
        }
        LFPG_WireHelper.DeserializeJSON(m_Wires, json, "LFPG_PressurePad");

        return true;
    }
};
