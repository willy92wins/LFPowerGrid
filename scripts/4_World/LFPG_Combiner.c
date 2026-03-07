// =========================================================
// LF_PowerGrid - Combiner device (v0.8.2)
//
// LF_Combiner_Kit: Holdable item (same-model deployment).
//                  Player places it via hologram -> spawns LF_Combiner.
//
// LF_Combiner:     2 inputs  (input_1, input_2)
//                  1 output  (output_1)
//                  Sums incoming power from both inputs and delivers
//                  to a single output. Inverse of Splitter.
//                  output = min(input_1 + input_2, 500 u/s)
//                  Owns wires on its output side (same pattern as Generator).
//
// Memory points (LOD Memory):
//   port_input_1   - upstream cable 1
//   port_input_2   - upstream cable 2
//   port_output_1  - downstream cable
//
// Wire manipulation delegated to LFPG_WireHelper (3_Game).
// =========================================================

// ---------------------------------------------------------
// KIT: deployable item that spawns the actual Combiner
// Uses the combiner model directly — hologram matches final
// result, no cargo, no orientation mismatch.
// Same-model pattern (identical to Splitter_Kit).
// ---------------------------------------------------------
class LF_Combiner_Kit : Inventory_Base
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

    // Prevent orphan loop sound — ObjectDelete during
    // OnPlacementComplete interrupts ActionContinuousBase cleanup.
    override string GetLoopDeploySoundset()
    {
        return "";
    }

    override void SetActions()
    {
        super.SetActions();
        // ActionTogglePlaceObject enters hologram mode.
        // LFPG_ActionPlaceCombiner confirms placement (extends ActionPlaceObject).
        // ActionPlaceObject pipeline passes hologram pos as parameter to
        // OnPlacementComplete. Use parameter, NOT GetPosition().
        AddAction(ActionTogglePlaceObject);
        AddAction(LFPG_ActionPlaceCombiner);
    }

    // ActionPlaceObject inherits ActionDeployObject pipeline.
    // OnFinishProgressClient passes GetLocalProjectionPosition() (hologram
    // pos) as the position parameter. Use it directly.
    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[Combiner_Kit] OnPlacementComplete: param=" + position.ToString();
        tLog = tLog + " kitPos=" + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        // Do NOT use ECE_PLACE_ON_SURFACE — it forces ground snap, killing wall placement.
        // Do NOT zero pitch/roll — the hologram orientation already includes wall alignment.
        EntityAI combiner = GetGame().CreateObjectEx("LF_Combiner", finalPos, ECE_CREATEPHYSICS);
        if (combiner)
        {
            combiner.SetPosition(finalPos);
            combiner.SetOrientation(finalOri);
            combiner.Update();
            LFPG_Util.Info("[Combiner_Kit] Deployed LF_Combiner at " + finalPos.ToString() + " ori=" + finalOri.ToString());

            // Only delete kit on successful spawn.
            // If CreateObjectEx fails, the player keeps the kit.
            GetGame().ObjectDelete(this);
        }
        else
        {
            LFPG_Util.Error("[Combiner_Kit] Failed to create LF_Combiner! Kit preserved.");
            // Kit remains on ground — player can pick it up and try again.
            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                pb.MessageStatus("[LFPG] Combiner placement failed. Kit preserved.");
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// COMBINER: pass-through device (2 inputs + 1 output)
// Inverse of Splitter: sums two upstream sources into one
// downstream output. Throughput capped at 500 u/s.
// ---------------------------------------------------------
class LF_Combiner : Inventory_Base
{
    // ---- Device identity ----
    protected int m_DeviceIdLow = 0;
    protected int m_DeviceIdHigh = 0;
    protected string m_DeviceId;

    // ---- Wires owned (output side, same as Generator) ----
    protected ref array<ref LFPG_WireData> m_Wires;

    // ---- Power state ----
    // True when upstream source is providing power via input port(s)
    protected bool m_PoweredNet = false;

    // Deletion guard — prevents OnVariablesSynchronized from
    // re-registering a dying device (RC-04 parity).
    protected bool m_LFPG_Deleting = false;

    // Bitmask of overloaded output wires.
    protected bool m_Overloaded = false;

    // Bitmask of warning-level output wires.

    void LF_Combiner()
    {
        m_Wires = new array<ref LFPG_WireData>;
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
        RegisterNetSyncVariableBool("m_Overloaded");
    }

    override void SetActions()
    {
        super.SetActions();

        // Block vanilla take/carry actions.
        RemoveAction(ActionTakeItem);
        RemoveAction(ActionTakeItemToHands);
    }

    // Prevent players from picking up a placed combiner.
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

    // Audit fix: prevent heavy-item carry behavior
    override bool IsHeavyBehaviour()
    {
        return false;
    }

    // Delegates to DeviceLifecycle for movement detection.
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
        // Combiner is PASSTHROUGH — does not initiate propagation.
        // Upstream source will propagate down and set m_PoweredNet=true
        // via LFPG_SetPowered if connected and powered.
        #endif
    }

    // Delegates to DeviceLifecycle, then handles combiner-specific state.
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

    // Delegates to DeviceLifecycle helper.
    override void EEDelete(EntityAI parent)
    {
        // Set deletion flag BEFORE unregistration.
        // Prevents OnVariablesSynchronized from re-registering a dying device.
        m_LFPG_Deleting = true;

        LFPG_DeviceLifecycle.OnDeviceDeleted(this, m_DeviceId);
        super.EEDelete(parent);
    }

    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();
        LFPG_TryRegister();

        #ifndef SERVER
        // Combiner is both owner (has output wire) and consumer
        // (receives input from upstream). Always request sync because a
        // distant owner may have wires targeting this combiner.
        if (m_DeviceId != "")
        {
            LFPG_CableRenderer r = LFPG_CableRenderer.Get();
            if (r)
            {
                // Request sync for any missing wire data (cooldown-throttled)
                r.RequestDeviceSync(m_DeviceId, this);

                // If we already have our own owner data, immediately refresh
                // visual state (Overloaded) to eliminate CullTick delay.
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
        // Don't register if device is being deleted.
        if (m_LFPG_Deleting)
            return;

        // Capture old ID before recalculating.
        // If OnVarSync brings a different DeviceIdLow/High (partial SyncVar,
        // engine recycling), the old ID must be unregistered to prevent
        // ghost entries in DeviceRegistry.
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
        if (idx == 0) return "input_1";
        if (idx == 1) return "input_2";
        if (idx == 2) return "output_1";
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
        if (idx == 0) return "Input 1";
        if (idx == 1) return "Input 2";
        if (idx == 2) return "Output 1";
        return "";
    }

    bool LFPG_HasPort(string portName, int dir)
    {
        if (dir == LFPG_PortDir.IN)
        {
            if (portName == "input_1") return true;
            if (portName == "input_2") return true;
        }
        if (dir == LFPG_PortDir.OUT)
        {
            if (portName == "output_1") return true;
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
        LFPG_Util.Warn("[LF_Combiner] Missing memory point for port: " + portName);
        vector p = GetPosition();
        p[1] = p[1] + 0.5;
        return p;
    }

    // ---- Source behavior ----
    // The combiner IS a source for downstream devices.
    bool LFPG_IsSource()
    {
        return true;
    }

    // Throughput capacity: 500 u/s (higher than Splitter's 200 u/s).
    // Combiner merges two inputs so needs higher cap for combined output.
    float LFPG_GetCapacity()
    {
        return 500.0;
    }

    // Electrical graph node type.
    // Combiner has both IN and OUT ports -> PASSTHROUGH.
    int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.PASSTHROUGH;
    }

    // OBLIGATORY: Explicit zero self-consumption.
    // Combiner retransmits power, it does not consume any.
    // Without this, LFPG_DeviceAPI.GetConsumption() falls through to
    // IsEnergyConsumer() (true, has IN port) and returns
    // LFPG_DEFAULT_CONSUMER_CONSUMPTION (10.0).
    float LFPG_GetConsumption()
    {
        return 0.0;
    }

    // Source is "on" when receiving power from upstream.
    bool LFPG_GetSourceOn()
    {
        return m_PoweredNet;
    }

    // Expose powered state for inspector display.
    // m_PoweredNet is SyncVar so available on both client and server.
    bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    // ---- Consumer behavior ----
    // Called by graph propagation when upstream power state changes.
    // NOTE: No RequestPropagate here. The graph handles propagation
    // automatically: when upstream changes, this PASSTHROUGH node is
    // marked dirty via DIRTY_INPUT and re-evaluated by ProcessDirtyQueue,
    // which then marks downstream nodes dirty in turn.
    void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
        {
            LFPG_Util.Debug("[LF_Combiner] SetPowered(" + powered.ToString() + ") SKIP (no change) id=" + m_DeviceId);
            return;
        }

        m_PoweredNet = powered;
        SetSynchDirty();

        LFPG_Util.Debug("[LF_Combiner] SetPowered(" + powered.ToString() + ") id=" + m_DeviceId + " wires=" + m_Wires.Count().ToString());
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
            LFPG_Util.Warn("[LF_Combiner] AddWire rejected: not an output port: " + wd.m_SourcePort);
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
        // m_PoweredNet NOT persisted — derived state, propagation recalculates.

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
            LFPG_Util.Error("[LF_Combiner] OnStoreLoad: failed to read m_DeviceIdLow");
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            LFPG_Util.Error("[LF_Combiner] OnStoreLoad: failed to read m_DeviceIdHigh");
            return false;
        }

        LFPG_UpdateDeviceIdString();

        // m_PoweredNet not persisted — field default (false) is correct;
        // propagation re-derives it from upstream state.

        string json;
        if (!ctx.Read(json))
        {
            LFPG_Util.Error("[LF_Combiner] OnStoreLoad: failed to read wires json for " + m_DeviceId);
            return false;
        }
        LFPG_WireHelper.DeserializeJSON(m_Wires, json, "LF_Combiner");

        return true;
    }
};
