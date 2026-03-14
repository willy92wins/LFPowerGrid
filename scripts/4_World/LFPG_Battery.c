// =========================================================
// LF_PowerGrid - Battery device (v2.0)
//
// LF_Battery_Kit:     Deployable kit. Player places via hologram
//                     → spawns LF_BatterySmall/Medium/Large.
//
// LF_BatteryBase:     PASSTHROUGH (1 IN + 1 OUT) with energy storage.
//                     Charges from surplus, discharges to supplement.
//                     Graph integration via m_VirtualGeneration + m_SoftDemand
//                     fields on ElecNode (set by NetworkManager timer).
//                     Owns wires on output side (same pattern as Splitter).
//
// LF_BatterySmall:    2,000 u capacity, 30 chg, 40 dis, 92% eff
// LF_BatteryMedium:   10,000 u capacity, 50 chg, 70 dis, 90% eff
// LF_BatteryLarge:    50,000 u capacity, 80 chg, 120 dis, 88% eff
//
// Memory points (LOD Memory):
//   port_input_1   - upstream cable
//   port_output_1  - downstream cable
//
// Persistence: ids + m_StoredEnergy + m_DischargeEnabled + wiresJSON
//   m_PoweredNet NOT persisted (derived state).
//   ⚠ SAVE WIPE REQUIRED — new entity schema.
//
// NetworkManager timer (LFPG_TickBatteries) reads/writes entity state
// via dynamic dispatch every ~5s. See SPRINT2_CHANGELOG.md.
// =========================================================

// ---------------------------------------------------------
// KIT: deployable item that spawns the battery.
// Same-model pattern (Splitter_Kit).
// ---------------------------------------------------------
class LF_Battery_Kit : Inventory_Base
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

    // v0.7.48 (Bug 5): Empty loop sound prevents orphan audio.
    override string GetLoopDeploySoundset()
    {
        string empty = "";
        return empty;
    }
};

// ---------------------------------------------------------
// DEVICE BASE: PASSTHROUGH with energy storage
// Tiers inherit and override capacity/rates.
// ---------------------------------------------------------
class LF_BatteryBase : Inventory_Base
{
    // ---- Device identity ----
    protected int m_DeviceIdLow = 0;
    protected int m_DeviceIdHigh = 0;
    protected string m_DeviceId;

    // ---- Wires owned (output side) ----
    protected ref array<ref LFPG_WireData> m_Wires;

    // ---- Power state (set by graph propagation) ----
    protected bool m_PoweredNet = false;

    // ---- Overload state ----
    protected bool m_Overloaded = false;

    // ---- Battery state (persisted) ----
    protected float m_StoredEnergy = 0.0;
    protected bool m_DischargeEnabled = true;

    // ---- Battery UI state (SyncVar, set by timer) ----
    // Positive = charging, negative = discharging, 0 = idle.
    protected float m_ChargeRateCurrent = 0.0;

    // ---- Sync tracking (server-only, not persisted) ----
    // Tracks last m_StoredEnergy value that was synced to client.
    // Delta is computed against this, not against per-tick change.
    // Prevents slow drain from never syncing to client.
    protected float m_LastSyncedStored = -1.0;

    // ---- Deletion guard ----
    protected bool m_LFPG_Deleting = false;

    // ============================================
    // Constructor - SyncVars en constructor, NO EEInit
    // ============================================
    void LF_BatteryBase()
    {
        m_Wires = new array<ref LFPG_WireData>;
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
        RegisterNetSyncVariableBool("m_Overloaded");
        // 12 bits = 4096 steps across 100000 = ~24.4u/step.
        // Small(2000u): ~82 distinct levels. Medium(10000u): ~410. Large(50000u): ~2048.
        // All smooth enough for charge bar display.
        RegisterNetSyncVariableFloat("m_StoredEnergy", 0.0, 100000.0, 12);
        RegisterNetSyncVariableFloat("m_ChargeRateCurrent", -200.0, 200.0, 8);
    }

    // ============================================
    // Battery API — read by NetworkManager timer via dynamic dispatch
    // ============================================
    float LFPG_GetStoredEnergy()
    {
        return m_StoredEnergy;
    }

    void LFPG_SetStoredEnergy(float val)
    {
        #ifdef SERVER
        m_StoredEnergy = val;

        // Sync vanilla CompEM energy bar (inventory UI).
        // CompEM handles its own client sync for the bar display.
        // Our m_StoredEnergy SyncVar is for LFPG systems (inspector, cable HUD).
        LFPG_SyncCompEM();

        // Cumulative sync: compare against last SYNCED value, not last tick.
        // Prevents slow drain from never reaching the per-tick threshold.
        float maxStored = LFPG_GetMaxStoredEnergy();
        float threshold = maxStored * LFPG_BATTERY_SYNC_THRESHOLD_PCT;
        bool needsSync = false;

        // First call after spawn/load: always sync.
        if (m_LastSyncedStored < 0.0)
        {
            needsSync = true;
        }
        else
        {
            // Cumulative delta since last actual sync.
            float cumDelta = val - m_LastSyncedStored;
            if (cumDelta < 0.0)
            {
                cumDelta = -cumDelta;
            }
            if (cumDelta > threshold)
            {
                needsSync = true;
            }

            // Boundary crossing: sync when hitting 0 or max.
            if (val < LFPG_PROPAGATION_EPSILON && m_LastSyncedStored > LFPG_PROPAGATION_EPSILON)
            {
                needsSync = true;
            }
            float effectiveMax = maxStored;
            if (val > effectiveMax - LFPG_PROPAGATION_EPSILON && m_LastSyncedStored < effectiveMax - LFPG_PROPAGATION_EPSILON)
            {
                needsSync = true;
            }
        }

        if (needsSync)
        {
            m_LastSyncedStored = val;
            SetSynchDirty();
        }
        #endif
    }

    // Sync m_StoredEnergy → vanilla CompEM for inventory energy bar.
    // CompEM handles its own client replication — bar updates automatically.
    // Called from SetStoredEnergy (timer writes) and EEInit (persistence load).
    // Guard: CompEM may not exist if config.cpp is missing EnergyManager block.
    protected void LFPG_SyncCompEM()
    {
        #ifdef SERVER
        ComponentEnergyManager em = GetCompEM();
        if (em)
        {
            em.SetEnergy(m_StoredEnergy);
        }
        #endif
    }

    float LFPG_GetMaxStoredEnergy()
    {
        return LFPG_BATTERY_SMALL_CAPACITY;
    }

    float LFPG_GetMaxChargeRate()
    {
        return LFPG_BATTERY_SMALL_CHARGE_RATE;
    }

    float LFPG_GetMaxDischargeRate()
    {
        return LFPG_BATTERY_SMALL_DISCHARGE_RATE;
    }

    float LFPG_GetEfficiency()
    {
        return LFPG_BATTERY_SMALL_EFFICIENCY;
    }

    float LFPG_GetSelfDischargeRate()
    {
        return LFPG_BATTERY_SELF_DISCHARGE_RATE;
    }

    bool LFPG_IsDischargeEnabled()
    {
        return m_DischargeEnabled;
    }

    void LFPG_SetDischargeEnabled(bool val)
    {
        #ifdef SERVER
        m_DischargeEnabled = val;
        // No SetSynchDirty here — discharge state is internal.
        // UI shows charge rate (+/-/0) which implicitly reflects this.
        #endif
    }

    void LFPG_SetChargeRateCurrent(float val)
    {
        #ifdef SERVER
        // Determine sign category: +1 charging, -1 discharging, 0 idle.
        int oldSign = 0;
        if (m_ChargeRateCurrent > LFPG_PROPAGATION_EPSILON)
        {
            oldSign = 1;
        }
        else if (m_ChargeRateCurrent < -LFPG_PROPAGATION_EPSILON)
        {
            oldSign = -1;
        }

        int newSign = 0;
        if (val > LFPG_PROPAGATION_EPSILON)
        {
            newSign = 1;
        }
        else if (val < -LFPG_PROPAGATION_EPSILON)
        {
            newSign = -1;
        }

        m_ChargeRateCurrent = val;

        // Only sync to client when sign category changes.
        // Avoids per-tick syncs for minor rate fluctuations.
        if (oldSign != newSign)
        {
            SetSynchDirty();
        }
        #endif
    }

    // ============================================
    // LFPG Device Interface (PASSTHROUGH)
    // ============================================
    string LFPG_GetDeviceId()
    {
        return m_DeviceId;
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

    // Zero self-consumption: pure relay + storage.
    // Without this, DeviceAPI fallback returns 10.0 (IsEnergyConsumer=true).
    float LFPG_GetConsumption()
    {
        return 0.0;
    }

    float LFPG_GetCapacity()
    {
        return LFPG_BATTERY_SMALL_MAX_OUTPUT;
    }

    bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    bool LFPG_GetOverloaded()
    {
        return m_Overloaded;
    }

    void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;
        m_PoweredNet = powered;
        SetSynchDirty();
        #endif
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

    // ---- Ports: 1 IN + 1 OUT ----
    int LFPG_GetPortCount()
    {
        return 2;
    }

    string LFPG_GetPortName(int idx)
    {
        if (idx == 0)
            return LFPG_PORT_INPUT_1;
        if (idx == 1)
            return LFPG_PORT_OUTPUT_1;
        string empty = "";
        return empty;
    }

    int LFPG_GetPortDir(int idx)
    {
        if (idx == 0)
            return LFPG_PortDir.IN;
        if (idx == 1)
            return LFPG_PortDir.OUT;
        return -1;
    }

    string LFPG_GetPortLabel(int idx)
    {
        if (idx == 0)
            return LFPG_PORT_INPUT_1;
        if (idx == 1)
            return LFPG_PORT_OUTPUT_1;
        string empty = "";
        return empty;
    }

    bool LFPG_HasPort(string portName, int dir)
    {
        if (portName == LFPG_PORT_INPUT_1 && dir == LFPG_PortDir.IN)
            return true;
        if (portName == LFPG_PORT_OUTPUT_1 && dir == LFPG_PortDir.OUT)
            return true;
        return false;
    }

    vector LFPG_GetPortWorldPos(string portName)
    {
        string memPoint = "";
        if (portName == LFPG_PORT_INPUT_1)
        {
            memPoint = "port_input_0";
        }
        else if (portName == LFPG_PORT_OUTPUT_1)
        {
            memPoint = "port_output_0";
        }
        else
        {
            string portPrefix = "port_";
            memPoint = portPrefix + portName;
        }

        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

        // Fallback: virtual offsets if memory points missing.
        vector offset = "0 0.02 0";
        if (portName == LFPG_PORT_INPUT_1)
        {
            offset = "0 0.02 -0.05";
        }
        else if (portName == LFPG_PORT_OUTPUT_1)
        {
            offset = "0 0.02 0.05";
        }
        return ModelToWorld(offset);
    }

    // ---- Connection validation ----
    bool LFPG_CanConnectTo(Object other, string myPort, string otherPort)
    {
        if (!other)
            return false;

        // Only output ports can initiate connections.
        if (!LFPG_HasPort(myPort, LFPG_PortDir.OUT))
            return false;

        EntityAI otherEntity = EntityAI.Cast(other);
        if (!otherEntity)
            return false;

        // Accept any device with matching input port.
        string otherId = LFPG_DeviceAPI.GetDeviceId(otherEntity);
        if (otherId != "")
        {
            return LFPG_DeviceAPI.HasPort(other, otherPort, LFPG_PortDir.IN);
        }

        // Vanilla device: accept if it's a consumer.
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
        if (!wd)
            return false;

        if (wd.m_SourcePort == "")
        {
            wd.m_SourcePort = LFPG_PORT_OUTPUT_1;
        }

        if (!LFPG_HasPort(wd.m_SourcePort, LFPG_PortDir.OUT))
        {
            string warnMsg = "[LF_Battery] AddWire rejected: not an output port: ";
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
    // Lifecycle
    // ============================================
    override void EEInit()
    {
        super.EEInit();

        // Detect fresh spawn: IDs are 0 only when entity was just created
        // (not loaded from persistence — OnStoreLoad sets IDs before EEInit).
        bool isFreshSpawn = false;
        #ifdef SERVER
        if (m_DeviceIdLow == 0 && m_DeviceIdHigh == 0)
        {
            LFPG_Util.GenerateDeviceId(m_DeviceIdLow, m_DeviceIdHigh);
            isFreshSpawn = true;
        }
        SetSynchDirty();
        #endif

        LFPG_UpdateDeviceIdString();
        LFPG_TryRegister();

        #ifdef SERVER
        LFPG_NetworkManager.Get().BroadcastOwnerWires(this);
        // Register with battery timer.
        LFPG_NetworkManager.Get().RegisterBattery(this);
        // Allow tiers to set initial charge on fresh spawn only.
        if (isFreshSpawn)
        {
            LFPG_InitFreshSpawn();
        }
        // Sync CompEM bar on load or fresh spawn.
        // OnStoreLoad set m_StoredEnergy, CompEM still has energyAtSpawn=0.
        LFPG_SyncCompEM();
        #endif
    }

    // Tiers inherit this. LFPG_GetMaxStoredEnergy() resolves to tier override.
    // If a future tier should start empty (crafted), override with no-op.
    void LFPG_InitFreshSpawn()
    {
        float maxCap = LFPG_GetMaxStoredEnergy();
        float randomPct = Math.RandomFloatInclusive(0.1, 0.8);
        m_StoredEnergy = maxCap * randomPct;
        SetSynchDirty();
    }

    // ---- Block vanilla CompEM callbacks ----
    // EnergyManager exists in config.cpp ONLY for the inventory energy bar.
    // All energy logic is handled by LFPG timer. These empty overrides
    // prevent CompEM from draining, switching, or interfering.
    override void OnSwitchOn()
    {
    }

    override void OnSwitchOff()
    {
    }

    override void OnWorkStart()
    {
    }

    override void OnWorkStop()
    {
    }

    override void OnWork(float consumed_energy)
    {
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
        LFPG_NetworkManager.Get().UnregisterBattery(this);
        #endif

        super.EEKilled(killer);
    }

    override void EEDelete(EntityAI parent)
    {
        m_LFPG_Deleting = true;

        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterBattery(this);
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

        #ifndef SERVER
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
    // ID helpers
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
    // Inventory guards
    // ============================================
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

    override bool IsElectricAppliance()
    {
        return false;
    }

    // ============================================
    // Persistence
    // ============================================
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);

        ctx.Write(m_DeviceIdLow);
        ctx.Write(m_DeviceIdHigh);
        ctx.Write(m_StoredEnergy);
        ctx.Write(m_DischargeEnabled);

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
            string errLow = "[LF_Battery] OnStoreLoad: failed to read m_DeviceIdLow";
            LFPG_Util.Error(errLow);
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            string errHigh = "[LF_Battery] OnStoreLoad: failed to read m_DeviceIdHigh";
            LFPG_Util.Error(errHigh);
            return false;
        }

        LFPG_UpdateDeviceIdString();

        if (!ctx.Read(m_StoredEnergy))
        {
            string errStored = "[LF_Battery] OnStoreLoad: failed to read m_StoredEnergy for ";
            errStored = errStored + m_DeviceId;
            LFPG_Util.Error(errStored);
            return false;
        }

        if (!ctx.Read(m_DischargeEnabled))
        {
            string errDisch = "[LF_Battery] OnStoreLoad: failed to read m_DischargeEnabled for ";
            errDisch = errDisch + m_DeviceId;
            LFPG_Util.Error(errDisch);
            return false;
        }

        string json = "";
        if (!ctx.Read(json))
        {
            string errJson = "[LF_Battery] OnStoreLoad: failed to read wires json for ";
            errJson = errJson + m_DeviceId;
            LFPG_Util.Error(errJson);
            return false;
        }
        string loaderTag = "LF_Battery";
        LFPG_WireHelper.DeserializeJSON(m_Wires, json, loaderTag);

        return true;
    }
};

// =========================================================
// TIER 1: Small (portable backup)
// =========================================================
class LF_BatterySmall : LF_BatteryBase
{
    // Uses base class defaults (SMALL constants).
    // LFPG_InitFreshSpawn inherited from base → random charge 10-80%.
};

// =========================================================
// TIER 2: Medium (base standard)
// =========================================================
class LF_BatteryMedium : LF_BatteryBase
{
    override float LFPG_GetMaxStoredEnergy()
    {
        return LFPG_BATTERY_MEDIUM_CAPACITY;
    }

    override float LFPG_GetMaxChargeRate()
    {
        return LFPG_BATTERY_MEDIUM_CHARGE_RATE;
    }

    override float LFPG_GetMaxDischargeRate()
    {
        return LFPG_BATTERY_MEDIUM_DISCHARGE_RATE;
    }

    override float LFPG_GetEfficiency()
    {
        return LFPG_BATTERY_MEDIUM_EFFICIENCY;
    }

    override float LFPG_GetCapacity()
    {
        return LFPG_BATTERY_MEDIUM_MAX_OUTPUT;
    }
};

// =========================================================
// TIER 3: Large (industrial grid bank)
// =========================================================
class LF_BatteryLarge : LF_BatteryBase
{
    override float LFPG_GetMaxStoredEnergy()
    {
        return LFPG_BATTERY_LARGE_CAPACITY;
    }

    override float LFPG_GetMaxChargeRate()
    {
        return LFPG_BATTERY_LARGE_CHARGE_RATE;
    }

    override float LFPG_GetMaxDischargeRate()
    {
        return LFPG_BATTERY_LARGE_DISCHARGE_RATE;
    }

    override float LFPG_GetEfficiency()
    {
        return LFPG_BATTERY_LARGE_EFFICIENCY;
    }

    override float LFPG_GetCapacity()
    {
        return LFPG_BATTERY_LARGE_MAX_OUTPUT;
    }
};
