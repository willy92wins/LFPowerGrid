// =========================================================
// LF_PowerGrid - Splitter device (v0.7.26)
//
// LF_Splitter_Kit:  Holdable item (wooden crate model).
//                   Player places it via hologram -> spawns LF_Splitter.
//
// LF_Splitter:      1 input  (input_1)
//                   3 outputs (output_1, output_2, output_3)
//                   Each output delivers 1/3 of the incoming power.
//                   Owns wires on its output side (same pattern as Generator).
//
// Memory points (LOD Memory):
//   port_input_1   - where the upstream cable connects
//   port_output_1  - downstream cable 1
//   port_output_2  - downstream cable 2
//   port_output_3  - downstream cable 3
//
// Wire manipulation delegated to LFPG_WireHelper (3_Game).
//
// v0.7.41 (BugFix): OnPlacementComplete used GetPosition() which returns
//   the kit's physical pos (near player), not hologram pos. Fixed to use
//   the position parameter from ActionPlaceObject→ActionDeployObject pipeline.
// =========================================================

// ---------------------------------------------------------
// KIT: deployable item that spawns the actual Splitter
// Uses the splitter model directly — hologram matches final
// result, no cargo, no orientation mismatch.
// ---------------------------------------------------------
class LF_Splitter_Kit : Inventory_Base
{
    override bool IsDeployable()
    {
        return true;
    }

    // Prevent any cargo display inherited from model proxies
    override bool CanDisplayCargo()
    {
        return false;
    }

    // Allow placement near walls and on surfaces
    override bool CanBePlaced(Man player, vector position)
    {
        return true;
    }

    // Disable height restriction so it can go on tables/shelves
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
        return "barbedwire_deploy_SoundSet";
    }

    override void SetActions()
    {
        super.SetActions();
        // v0.7.26: ActionTogglePlaceObject enters hologram mode.
        // LFPG_ActionPlaceSplitter confirms placement (extends ActionPlaceObject).
        // ActionPlaceObject→ActionDeployObject pipeline passes hologram pos as
        // parameter to OnPlacementComplete. Use parameter, NOT GetPosition().
        AddAction(ActionTogglePlaceObject);
        AddAction(LFPG_ActionPlaceSplitter);
    }

    // v0.7.26: ActionPlaceObject inherits ActionDeployObject pipeline.
    //   OnFinishProgressClient passes GetLocalProjectionPosition() (hologram
    //   pos) as the position parameter. Use it directly.
    // v0.7.41 (BugFix): Use position/orientation PARAMETERS, not GetPosition().
    //   GetPosition() returns the kit's physical position (near the player).
    //   The position parameter carries the hologram position from the action
    //   system — same pattern used by rag_baseitems and vanilla kits.
    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[Splitter_Kit] OnPlacementComplete: param=" + position.ToString();
        tLog = tLog + " kitPos=" + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        // Do NOT use ECE_PLACE_ON_SURFACE — it forces ground snap, killing wall placement.
        // Do NOT zero pitch/roll — the hologram orientation already includes wall alignment.
        EntityAI splitter = GetGame().CreateObjectEx("LF_Splitter", finalPos, ECE_CREATEPHYSICS);
        if (splitter)
        {
            splitter.SetPosition(finalPos);
            splitter.SetOrientation(finalOri);
            splitter.Update();
            LFPG_Util.Info("[Splitter_Kit] Deployed LF_Splitter at " + finalPos.ToString() + " ori=" + finalOri.ToString());

            // v0.7.32 (Audit): Only delete kit on successful spawn.
            // If CreateObjectEx fails, the player keeps the kit.
            GetGame().ObjectDelete(this);
        }
        else
        {
            LFPG_Util.Error("[Splitter_Kit] Failed to create LF_Splitter! Kit preserved.");
            // Kit remains on ground — player can pick it up and try again.
            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                pb.MessageStatus("[LFPG] Splitter placement failed. Kit preserved.");
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// SPLITTER: pass-through device (consumer + source)
// ---------------------------------------------------------
class LF_Splitter : Inventory_Base
{
    // ---- Device identity ----
    protected int m_DeviceIdLow = 0;
    protected int m_DeviceIdHigh = 0;
    protected string m_DeviceId;

    // ---- Wires owned (output side, same as Generator) ----
    protected ref array<ref LFPG_WireData> m_Wires;

    // ---- Power state ----
    // True when upstream source is providing power via input port
    protected bool m_PoweredNet = false;

    // v0.7.8: Bitmask of overloaded output wires.
    protected int m_OverloadMask = 0;

    // v0.7.35 (F1.3): Bitmask of warning-level output wires.
    protected int m_WarningMask = 0;

    void LF_Splitter()
    {
        m_Wires = new array<ref LFPG_WireData>;
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
        RegisterNetSyncVariableInt("m_OverloadMask");
        RegisterNetSyncVariableInt("m_WarningMask");
    }

    override void SetActions()
    {
        super.SetActions();

        // v0.7.28 (Bug 2): Block vanilla take/carry actions.
        RemoveAction(ActionTakeItem);
        RemoveAction(ActionTakeItemToHands);
    }

    // Prevent players from picking up a placed splitter.
    // Moving it would break all wire connections and cause orphaned wires.
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

    // v0.7.28 (Refactor): Delegates to DeviceLifecycle for movement detection.
    // Replaces duplicated inline logic with centralized helper.
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
            SetSynchDirty();
        }
        #endif

        LFPG_UpdateDeviceIdString();
        LFPG_TryRegister();

        #ifdef SERVER
        LFPG_NetworkManager.Get().BroadcastOwnerWires(this);

        // FIX: if Splitter was saved as powered (e.g. server restart),
        // propagate immediately so downstream consumers receive power
        // without waiting for the 5s ValidateAllWiresAndPropagate timer.
        if (m_PoweredNet && m_DeviceId != "")
        {
            LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
        }
        #endif
    }

    // v0.7.28 (Refactor): Delegates to DeviceLifecycle, then handles splitter-specific state.
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

    // v0.7.28 (Refactor): Delegates to DeviceLifecycle helper.
    override void EEDelete(EntityAI parent)
    {
        LFPG_DeviceLifecycle.OnDeviceDeleted(this, m_DeviceId);
        super.EEDelete(parent);
    }

    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();
        LFPG_TryRegister();

        #ifndef SERVER
        // v0.7.35 D1+D4: Splitter is both owner (has output wires) and consumer
        // (receives input from Generator). Always request sync because a distant
        // owner may have wires targeting this splitter that aren't synced yet.
        if (m_DeviceId != "")
        {
            LFPG_CableRenderer r = LFPG_CableRenderer.Get();
            if (r)
            {
                // D1: Request sync for any missing wire data (cooldown-throttled)
                r.RequestDeviceSync(m_DeviceId);

                // D4: If we already have our own owner data, immediately refresh
                // visual state (OverloadMask, WarningMask) to eliminate CullTick delay.
                if (r.HasOwnerData(m_DeviceId))
                {
                    r.NotifyOwnerVisualChanged(m_DeviceId);
                }
            }
        }
        #endif
    }

    protected void LFPG_UpdateDeviceIdString()
    {
        m_DeviceId = LFPG_Util.MakeDeviceKey(m_DeviceIdLow, m_DeviceIdHigh);
    }

    protected void LFPG_TryRegister()
    {
        LFPG_UpdateDeviceIdString();
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

    // 4 ports: 1 input + 3 outputs
    int LFPG_GetPortCount()
    {
        return 4;
    }

    string LFPG_GetPortName(int idx)
    {
        if (idx == 0) return "input_1";
        if (idx == 1) return "output_1";
        if (idx == 2) return "output_2";
        if (idx == 3) return "output_3";
        return "";
    }

    int LFPG_GetPortDir(int idx)
    {
        if (idx == 0) return LFPG_PortDir.IN;
        if (idx >= 1 && idx <= 3) return LFPG_PortDir.OUT;
        return -1;
    }

    string LFPG_GetPortLabel(int idx)
    {
        if (idx == 0) return "Input 1";
        if (idx == 1) return "Output 1";
        if (idx == 2) return "Output 2";
        if (idx == 3) return "Output 3";
        return "";
    }

    bool LFPG_HasPort(string portName, int dir)
    {
        if (dir == LFPG_PortDir.IN && portName == "input_1") return true;
        if (dir == LFPG_PortDir.OUT)
        {
            if (portName == "output_1") return true;
            if (portName == "output_2") return true;
            if (portName == "output_3") return true;
        }
        return false;
    }

    vector LFPG_GetPortWorldPos(string portName)
    {
        // Primary: memory point with "port_" prefix convention
        // e.g. portName "input_1" -> memory point "port_input_1"
        string memPoint = "port_" + portName;
        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

        // Secondary: try without underscore before number.
        // Handles inconsistent p3d naming (e.g. "port_output1" vs "port_output_1").
        // Strip last underscore+digit and rejoin without underscore:
        // "output_1" -> try "port_output1"
        int len = portName.Length();
        if (len >= 3)
        {
            string lastChar = portName.Substring(len - 1, 1);
            string beforeLast = portName.Substring(len - 2, 1);
            if (beforeLast == "_")
            {
                // e.g. "output_1" -> "output" + "1" -> "port_output1"
                string compact = "port_" + portName.Substring(0, len - 2) + lastChar;
                if (MemoryPointExists(compact))
                {
                    return ModelToWorld(GetMemoryPointPos(compact));
                }
            }
        }

        // Tertiary: exact portName as memory point
        if (MemoryPointExists(portName))
        {
            return ModelToWorld(GetMemoryPointPos(portName));
        }

        // Fallback: device center + vertical offset
        // Only reached if p3d lacks the expected memory points
        LFPG_Util.Warn("[LF_Splitter] Missing memory point for port: " + portName);
        vector p = GetPosition();
        p[1] = p[1] + 0.5;
        return p;
    }

    // ---- Source behavior ----
    // The splitter IS a source for downstream devices.
    bool LFPG_IsSource()
    {
        return true;
    }

    // v0.7.33 (Fix #22): Max throughput capacity for this passthrough.
    // Caps how much power the splitter can relay to its 3 outputs.
    // Without this, a passthrough had infinite capacity — any input
    // passed through without limit. Now capped to default passthrough
    // capacity (200 units/s). Future: configurable per-device via settings.
    float LFPG_GetCapacity()
    {
        return LFPG_DEFAULT_PASSTHROUGH_CAPACITY;
    }

    // Sprint 4.1: Electrical graph node type.
    // Splitter has both IN and OUT ports → PASSTHROUGH.
    int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.PASSTHROUGH;
    }

    // Source is "on" when receiving power from upstream.
    bool LFPG_GetSourceOn()
    {
        return m_PoweredNet;
    }

    // ---- Consumer behavior ----
    // Called by graph propagation when upstream power state changes.
    // NOTE: No RequestPropagate here. The graph handles propagation
    // automatically: when upstream changes, this PASSTHROUGH node is
    // marked dirty via DIRTY_INPUT and re-evaluated by ProcessDirtyQueue,
    // which then marks downstream nodes dirty in turn.
    // ValidateAllWiresAndPropagate (self-heal) handles edge cases.
    void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
        {
            LFPG_Util.Debug("[LF_Splitter] SetPowered(" + powered.ToString() + ") SKIP (no change) id=" + m_DeviceId);
            return;
        }

        m_PoweredNet = powered;
        SetSynchDirty();

        LFPG_Util.Debug("[LF_Splitter] SetPowered(" + powered.ToString() + ") id=" + m_DeviceId + " wires=" + m_Wires.Count().ToString());
        #endif
    }

    // v0.7.8: Overload bitmask (which output wires exceed capacity)
    int LFPG_GetOverloadMask()
    {
        return m_OverloadMask;
    }

    void LFPG_SetOverloadMask(int mask)
    {
        #ifdef SERVER
        if (m_OverloadMask != mask)
        {
            m_OverloadMask = mask;
            SetSynchDirty();
        }
        #endif
    }

    // v0.7.35 (F1.3): Warning bitmask (partial allocation)
    int LFPG_GetWarningMask()
    {
        return m_WarningMask;
    }

    void LFPG_SetWarningMask(int mask)
    {
        #ifdef SERVER
        if (m_WarningMask != mask)
        {
            m_WarningMask = mask;
            SetSynchDirty();
        }
        #endif
    }

    // ---- Connection validation ----
    bool LFPG_CanConnectTo(Object other, string myPort, string otherPort)
    {
        if (!other) return false;

        // Only output ports can initiate connections
        if (!LFPG_HasPort(myPort, LFPG_PortDir.OUT)) return false;

        EntityAI otherEntity = EntityAI.Cast(other);
        if (!otherEntity) return false;

        // Check if other device has the specified input port
        string otherId = LFPG_DeviceAPI.GetDeviceId(otherEntity);
        if (otherId != "")
        {
            return LFPG_DeviceAPI.HasPort(other, otherPort, LFPG_PortDir.IN);
        }

        // Vanilla device: accept if it's a consumer
        return LFPG_DeviceAPI.IsEnergyConsumer(otherEntity);
    }

    // ============================================
    // Wire ownership API (delegates to WireHelper)
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

        // Only allow wires from output ports
        if (wd.m_SourcePort == "")
            wd.m_SourcePort = "output_1";

        if (!LFPG_HasPort(wd.m_SourcePort, LFPG_PortDir.OUT))
        {
            LFPG_Util.Warn("[LF_Splitter] AddWire rejected: not an output port: " + wd.m_SourcePort);
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

    // ============================================
    // Persistence
    // ============================================
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);

        ctx.Write(m_DeviceIdLow);
        ctx.Write(m_DeviceIdHigh);
        ctx.Write(m_PoweredNet);

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
            LFPG_Util.Error("[LF_Splitter] OnStoreLoad: failed to read m_DeviceIdLow");
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            LFPG_Util.Error("[LF_Splitter] OnStoreLoad: failed to read m_DeviceIdHigh");
            return false;
        }

        LFPG_UpdateDeviceIdString();

        if (!ctx.Read(m_PoweredNet))
        {
            LFPG_Util.Error("[LF_Splitter] OnStoreLoad: failed to read m_PoweredNet for " + m_DeviceId);
            return false;
        }

        string json;
        if (!ctx.Read(json))
        {
            LFPG_Util.Error("[LF_Splitter] OnStoreLoad: failed to read wires json for " + m_DeviceId);
            return false;
        }
        LFPG_WireHelper.DeserializeJSON(m_Wires, json, "LF_Splitter");

        return true;
    }
};
