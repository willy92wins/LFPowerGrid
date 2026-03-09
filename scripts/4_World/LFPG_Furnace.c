// =========================================================
// LF_PowerGrid - Furnace (Incinerator) SOURCE device (v1.2.0)
//
// LF_Furnace_Kit:  DeployableContainer_Base pattern (different-model).
//                  Uses shared box model (lf_kit_box.p3d).
//                  Hologram shows Furnace model during placement
//                  via LFPG_HologramMod.c overrides (6 methods).
//                  On confirm, spawns LF_Furnace and deletes kit.
//
// LF_Furnace:      SOURCE device (50 u/s constant while burning).
//                  Burns any item for fuel. Fuel = inventory squares
//                  calculated recursively (item + cargo + attachments).
//                  1 output port (output_1). Owns wires.
//                  Burn timer: -1 fuel every 30s. Auto-off at 0.
//
// ENFORCE SCRIPT NOTES:
//   - No ternary operators, No ++ / --, Explicit typing, No foreach
//   - Variables hoisted before conditionals
//   - No literals directly in params -> local var first
//
// Memory points required in Furnace.p3d (LOD Memory):
//   port_output_1 -- cable connection point
//
// Wire manipulation delegated to LFPG_WireHelper (3_Game).
// =========================================================

// ---------------------------------------------------------
// KIT: DeployableContainer_Base pattern (different-model hologram)
// Box model in hands -> hologram shows Furnace model.
// On confirm, spawns LF_Furnace at hologram position.
// ---------------------------------------------------------
class LF_Furnace_Kit : DeployableContainer_Base
{
    string GetDeployedClassname()
    {
        return "LF_Furnace";
    }

    vector GetDeployPositionOffset()
    {
        return "0 0 0";
    }

    vector GetDeployOrientationOffset()
    {
        return "0 0 0";
    }

    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[Furnace_Kit] OnPlacementComplete: param=";
        tLog = tLog + position.ToString();
        tLog = tLog + " kitPos=" + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        EntityAI furnace = GetGame().CreateObjectEx("LF_Furnace", finalPos, ECE_CREATEPHYSICS);
        if (furnace)
        {
            furnace.SetPosition(finalPos);
            furnace.SetOrientation(finalOri);
            furnace.Update();

            string deployMsg = "[Furnace_Kit] Deployed LF_Furnace at " + finalPos.ToString();
            LFPG_Util.Info(deployMsg);
            GetGame().ObjectDelete(this);
        }
        else
        {
            LFPG_Util.Error("[Furnace_Kit] Failed to create LF_Furnace! Kit preserved.");
            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                pb.MessageStatus("[LFPG] Furnace placement failed. Kit preserved.");
            }
        }
        #endif
    }

    override bool IsBasebuildingKit()
    {
        return true;
    }

    override bool IsDeployable()
    {
        return true;
    }

    // Prevent orphan loop sound — DeleteSafe during OnPlacementComplete
    // interrupts callback cleanup before the sound system detaches the loop.
    override string GetLoopDeploySoundset()
    {
        return "";
    }

    override void SetActions()
    {
        super.SetActions();
        AddAction(ActionTogglePlaceObject);
        AddAction(ActionPlaceObject);
    }
};

// ---------------------------------------------------------
// FURNACE: SOURCE device (50 u/s while burning)
// ---------------------------------------------------------
class LF_Furnace : Inventory_Base
{
    // ---- Device identity ----
    protected int m_DeviceIdLow = 0;
    protected int m_DeviceIdHigh = 0;
    protected string m_DeviceId;

    // ---- Wires owned (output side) ----
    protected ref array<ref LFPG_WireData> m_Wires;

    // ---- Source state (replicated) ----
    protected bool m_SourceOn = false;

    // ---- Anti-ghost guard ----
    protected bool m_LFPG_Deleting = false;

    // ---- Load telemetry (replicated to clients) ----
    protected float m_LoadRatio = 0.0;
    protected bool m_Overloaded = false;

    // ---- Fuel system (replicated) ----
    protected int m_FuelCurrent = 0;

    void LF_Furnace()
    {
        m_Wires = new array<ref LFPG_WireData>;
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_SourceOn");
        RegisterNetSyncVariableFloat("m_LoadRatio", 0.0, 5.0, 2);
        RegisterNetSyncVariableBool("m_Overloaded");
        RegisterNetSyncVariableInt("m_FuelCurrent");
    }

    // ============================================
    // Actions
    // ============================================
    override void SetActions()
    {
        super.SetActions();
        RemoveAction(ActionTakeItem);
        RemoveAction(ActionTakeItemToHands);
        // Toggle furnace on/off (no item required)
        AddAction(LFPG_ActionToggleFurnace);
        // Feed item into furnace (any item in hand, registered on target)
        AddAction(LFPG_ActionFeedFurnace);
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

    override bool IsElectricAppliance()
    {
        return false;
    }

    // ============================================
    // Fuel system
    // ============================================

    // Calculate fuel value recursively: item + cargo + attachments
    // Returns total inventory squares (w * h * qty) at all depths.
    // Realistic DayZ nesting is 3-4 levels max — no stack risk.
    int LFPG_CalcFuelRecursive(EntityAI item)
    {
        if (!item)
            return 0;

        string itemType = item.GetType();
        string cfgPath = "CfgVehicles " + itemType + " itemSize";

        int w = 0;
        int h = 0;
        int qty = 0;
        int fuel = 0;

        // Read item dimensions from config array
        if (GetGame().ConfigIsExisting(cfgPath))
        {
            TIntArray sizeArr = new TIntArray;
            GetGame().ConfigGetIntArray(cfgPath, sizeArr);
            if (sizeArr.Count() >= 2)
            {
                w = sizeArr[0];
                h = sizeArr[1];
            }
        }

        // Stack quantity (non-stackeable items report 0 or -1)
        qty = item.GetQuantity();
        if (qty <= 0)
        {
            qty = 1;
        }

        fuel = w * h * qty;

        // Guard: GetInventory() can be null on entities mid-deletion
        GameInventory inv = item.GetInventory();
        if (!inv)
            return fuel;

        // Recurse into cargo (items inside containers)
        CargoBase cargo = inv.GetCargo();
        if (cargo)
        {
            int cargoCount = cargo.GetItemCount();
            int ci = 0;
            for (ci = 0; ci < cargoCount; ci = ci + 1)
            {
                EntityAI cargoItem = cargo.GetItem(ci);
                fuel = fuel + LFPG_CalcFuelRecursive(cargoItem);
            }
        }

        // Recurse into attachments (clothing, magazines on weapons, etc.)
        int attCount = inv.AttachmentCount();
        int ai = 0;
        for (ai = 0; ai < attCount; ai = ai + 1)
        {
            EntityAI att = inv.GetAttachmentFromIndex(ai);
            fuel = fuel + LFPG_CalcFuelRecursive(att);
        }

        return fuel;
    }

    // Add fuel (server only, called from ActionFeedFurnace)
    void LFPG_AddFuel(int amount)
    {
        #ifdef SERVER
        if (amount <= 0)
            return;

        m_FuelCurrent = m_FuelCurrent + amount;
        if (m_FuelCurrent > LFPG_FURNACE_MAX_FUEL)
        {
            m_FuelCurrent = LFPG_FURNACE_MAX_FUEL;
        }
        SetSynchDirty();
        LFPG_Util.Info("[LF_Furnace] Fuel added: +" + amount.ToString() + " total=" + m_FuelCurrent.ToString() + " id=" + m_DeviceId);
        #endif
    }

    int LFPG_GetFuelCurrent()
    {
        return m_FuelCurrent;
    }

    // ---- Burn timer tick (server, called every 30s via CallLater) ----
    void LFPG_BurnTick()
    {
        #ifdef SERVER
        if (m_LFPG_Deleting)
            return;

        // Defense: if timer leaked while source is off, clean up and exit
        if (!m_SourceOn)
        {
            LFPG_StopBurnTimer();
            return;
        }

        // Consume one unit of fuel
        if (m_FuelCurrent > 0)
        {
            m_FuelCurrent = m_FuelCurrent - 1;
        }

        // Check exhaustion
        if (m_FuelCurrent <= 0)
        {
            m_FuelCurrent = 0;
            m_SourceOn = false;
            LFPG_StopBurnTimer();
            if (m_DeviceId != "")
            {
                LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
            }
            LFPG_Util.Info("[LF_Furnace] Fuel exhausted, auto-off. id=" + m_DeviceId);
        }

        // Single dirty for all state changes in this tick
        SetSynchDirty();
        #endif
    }

    // ---- Start burn timer (idempotent: removes any existing timer first) ----
    void LFPG_StartBurnTimer()
    {
        #ifdef SERVER
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_BurnTick);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_BurnTick, LFPG_FURNACE_BURN_INTERVAL_MS, true);
        #endif
    }

    // ---- Stop burn timer ----
    void LFPG_StopBurnTimer()
    {
        #ifdef SERVER
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_BurnTick);
        #endif
    }

    // ---- Toggle power (server only, called from ActionToggleFurnace) ----
    void LFPG_ToggleFurnace()
    {
        #ifdef SERVER
        if (m_SourceOn)
        {
            // Turn OFF
            m_SourceOn = false;
            LFPG_StopBurnTimer();
            SetSynchDirty();
            if (m_DeviceId != "")
            {
                LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
            }
            LFPG_Util.Info("[LF_Furnace] Toggled OFF. fuel=" + m_FuelCurrent.ToString() + " id=" + m_DeviceId);
        }
        else
        {
            // Turn ON (only if fuel > 0)
            if (m_FuelCurrent > 0)
            {
                m_SourceOn = true;
                LFPG_StartBurnTimer();
                SetSynchDirty();
                if (m_DeviceId != "")
                {
                    LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
                }
                LFPG_Util.Info("[LF_Furnace] Toggled ON. fuel=" + m_FuelCurrent.ToString() + " id=" + m_DeviceId);
            }
        }
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

        #ifdef SERVER
        // Post-load restore: if furnace was on with fuel, restart burn timer
        if (m_SourceOn && m_FuelCurrent > 0)
        {
            LFPG_StartBurnTimer();
        }
        // Safety: if source was on but no fuel, force off
        if (m_SourceOn && m_FuelCurrent <= 0)
        {
            m_SourceOn = false;
            m_FuelCurrent = 0;
            SetSynchDirty();
        }

        LFPG_NetworkManager.Get().BroadcastOwnerWires(this);

        // Propagate on init to rebuild graph edge allocations
        if (m_SourceOn && m_DeviceId != "")
        {
            LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
        }
        #endif
    }

    override void EEDelete(EntityAI parent)
    {
        m_LFPG_Deleting = true;
        LFPG_StopBurnTimer();
        LFPG_DeviceLifecycle.OnDeviceDeleted(this, m_DeviceId);
        super.EEDelete(parent);
    }

    override void EEKilled(Object killer)
    {
        LFPG_DeviceLifecycle.OnDeviceKilled(this, m_DeviceId);

        #ifdef SERVER
        if (m_SourceOn)
        {
            m_SourceOn = false;
            LFPG_StopBurnTimer();
            SetSynchDirty();
        }
        #endif

        super.EEKilled(killer);
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
            if (m_SourceOn)
            {
                m_SourceOn = false;
                LFPG_StopBurnTimer();
                SetSynchDirty();
            }
        }
        #endif
    }

    // JIP Fix: CableRenderer sync block (parity with SolarPanel/Splitter/Generator)
    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();
        LFPG_TryRegister();

        #ifndef SERVER
        if (m_DeviceId != "")
        {
            LFPG_CableRenderer r = LFPG_CableRenderer.Get();
            if (r)
            {
                if (!r.HasOwnerData(m_DeviceId))
                {
                    r.RequestDeviceSync(m_DeviceId, this);
                }
                else
                {
                    r.NotifyOwnerVisualChanged(m_DeviceId);
                }
            }
        }
        #endif
    }

    // ============================================
    // Device identity helpers
    // ============================================
    protected void LFPG_UpdateDeviceIdString()
    {
        m_DeviceId = LFPG_Util.MakeDeviceKey(m_DeviceIdLow, m_DeviceIdHigh);
    }

    protected void LFPG_TryRegister()
    {
        if (m_LFPG_Deleting)
            return;

        // Capture old ID before recalculating (anti-ghost, v0.9.3 S4 parity)
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

    int LFPG_GetPortCount()
    {
        return 1;
    }

    string LFPG_GetPortName(int idx)
    {
        if (idx == 0) return "output_1";
        return "";
    }

    int LFPG_GetPortDir(int idx)
    {
        if (idx == 0) return LFPG_PortDir.OUT;
        return -1;
    }

    string LFPG_GetPortLabel(int idx)
    {
        if (idx == 0) return "Output";
        return "";
    }

    bool LFPG_HasPort(string portName, int dir)
    {
        if (dir == LFPG_PortDir.OUT && portName == "output_1") return true;
        return false;
    }

    vector LFPG_GetPortWorldPos(string portName)
    {
        string memPoint = "port_" + portName;
        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

        // Fallback: try compact naming (port_output1)
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

        // Fallback: device center + offset
        LFPG_Util.Warn("[LF_Furnace] Missing memory point for port: " + portName);
        vector p = GetPosition();
        p[1] = p[1] + 0.5;
        return p;
    }

    int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.SOURCE;
    }

    bool LFPG_IsSource()
    {
        return true;
    }

    bool LFPG_GetSourceOn()
    {
        return m_SourceOn;
    }

    // Capacity: 50 u/s constant while burning
    float LFPG_GetCapacity()
    {
        return LFPG_FURNACE_CAPACITY;
    }

    // SOURCE: consumption is 0 (explicit — prevents DeviceAPI fallback of 10.0)
    float LFPG_GetConsumption()
    {
        return 0.0;
    }

    float LFPG_GetLoadRatio()
    {
        return m_LoadRatio;
    }

    void LFPG_SetLoadRatio(float ratio)
    {
        #ifdef SERVER
        if (ratio < 0.0)
        {
            ratio = 0.0;
        }

        // Delta threshold to avoid SetSynchDirty from float jitter (v0.9.3 S5 parity)
        float diff = ratio - m_LoadRatio;
        if (diff < 0.0)
        {
            diff = -diff;
        }
        if (diff > 0.01)
        {
            m_LoadRatio = ratio;
            SetSynchDirty();
        }
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

    // SOURCE: SetPowered is a no-op (we drive power via m_SourceOn)
    void LFPG_SetPowered(bool powered)
    {
        // No-op for SOURCE devices
    }

    // SwitchState for ToggleSource compatibility
    bool LFPG_GetSwitchState()
    {
        return m_SourceOn;
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

        if (wd.m_SourcePort == "")
        {
            wd.m_SourcePort = "output_1";
        }

        if (!LFPG_HasPort(wd.m_SourcePort, LFPG_PortDir.OUT))
        {
            LFPG_Util.Warn("[LF_Furnace] AddWire rejected: not an output port: " + wd.m_SourcePort);
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
            int vi = 0;
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
        ctx.Write(m_SourceOn);
        ctx.Write(m_FuelCurrent);

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
            LFPG_Util.Error("[LF_Furnace] OnStoreLoad: failed to read m_DeviceIdLow");
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            LFPG_Util.Error("[LF_Furnace] OnStoreLoad: failed to read m_DeviceIdHigh");
            return false;
        }

        LFPG_UpdateDeviceIdString();

        if (!ctx.Read(m_SourceOn))
        {
            LFPG_Util.Error("[LF_Furnace] OnStoreLoad: failed to read m_SourceOn for " + m_DeviceId);
            return false;
        }

        if (!ctx.Read(m_FuelCurrent))
        {
            LFPG_Util.Error("[LF_Furnace] OnStoreLoad: failed to read m_FuelCurrent for " + m_DeviceId);
            return false;
        }

        string json;
        if (!ctx.Read(json))
        {
            LFPG_Util.Error("[LF_Furnace] OnStoreLoad: failed to read wires json for " + m_DeviceId);
            return false;
        }
        LFPG_WireHelper.DeserializeJSON(m_Wires, json, "LF_Furnace");

        return true;
    }
};
