// =========================================================
// LF_PowerGrid - Battery device (v4.1)
//
// LFPG_BatteryMedium_Kit: Deployable kit → spawns BatteryMedium.
// LFPG_BatteryLarge_Kit:  DeployableContainer_Base → spawns BatteryLarge.
//
// LFPG_BatteryBase:     PASSTHROUGH (1 IN + 1 OUT) with energy storage.
//                     Charges from surplus, discharges to supplement.
//
// v4.1: Removed LF_BatterySmall / LF_Battery_Kit (dead code,
//   no config.cpp entry). Small battery role covered by
//   LFPG_BatteryAdapter + vanilla CarBattery/TruckBattery.
//   Added LFPG_IsGateCapable + LFPG_IsGateOpen + LFPG_UpdateLEDs
//   to LFPG_BatteryLarge (was missing toggle + LED support).
//
// v4.0: Migrated from Inventory_Base to LFPG_WireOwnerBase.
//   Wire store, wire API, persistence wireJSON, CanConnectTo — all in base.
//   GetPortWorldPos override: p3d uses port_input_0/port_output_0.
//
// LFPG_BatteryMedium:   10,000 u capacity, 50 chg, 70 dis, 90% eff
// LFPG_BatteryLarge:    50,000 u capacity, 80 chg, 120 dis, 88% eff
// =========================================================

// ---------------------------------------------------------
// KITS
// ---------------------------------------------------------

class LFPG_BatteryMedium_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_BatteryMedium";
    }
};

class LFPG_BatteryLarge_Kit : LFPG_KitBaseDeployable
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_BatteryLarge";
    }
};

// ---------------------------------------------------------
// DEVICE BASE: PASSTHROUGH : LFPG_WireOwnerBase
// Tiers inherit and override capacity/rates.
// ---------------------------------------------------------
class LFPG_BatteryBase : LFPG_WireOwnerBase
{
    // ---- Device-specific SyncVars ----
    protected bool  m_PoweredNet       = false;
    protected bool  m_Overloaded       = false;
    protected bool  m_OutputEnabled    = true;
    protected float m_StoredEnergy     = 0.0;
    protected float m_ChargeRateCurrent = 0.0;

    // ---- Battery state (persisted, not SyncVars) ----
    protected bool m_DischargeEnabled = true;

    // ---- Sync tracking (server-only, not persisted) ----
    protected float m_LastSyncedStored = -1.0;

    // ---- Fresh spawn detection ----
    protected bool m_LoadedFromPersistence = false;

    // ============================================
    // Constructor — ports + SyncVars
    // ============================================
    void LFPG_BatteryBase()
    {
        string pIn = "input_1";
        string lIn = "input_1";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, lIn);

        string pOut = "output_1";
        string lOut = "output_1";
        LFPG_AddPort(pOut, LFPG_PortDir.OUT, lOut);

        string varPowered     = "m_PoweredNet";
        string varOverloaded  = "m_Overloaded";
        string varOutput      = "m_OutputEnabled";
        string varStored      = "m_StoredEnergy";
        string varChargeRate  = "m_ChargeRateCurrent";

        RegisterNetSyncVariableBool(varPowered);
        RegisterNetSyncVariableBool(varOverloaded);
        RegisterNetSyncVariableBool(varOutput);
        RegisterNetSyncVariableFloat(varStored, 0.0, 100000.0, 12);
        RegisterNetSyncVariableFloat(varChargeRate, -200.0, 200.0, 8);
    }

    // ============================================
    // SetActions — DeviceBase removes TakeItem/TakeItemToHands
    // ============================================
    override void SetActions()
    {
        super.SetActions();
        AddAction(LFPG_ActionToggleBatteryOutput);
    }

    // ============================================
    // Port world pos override — p3d uses _0, LFPG uses _1
    // ============================================
    override vector LFPG_GetPortWorldPos(string portName)
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

    // ============================================
    // Virtual interface — PASSTHROUGH
    // ============================================
    override int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.PASSTHROUGH;
    }

    override float LFPG_GetConsumption()
    {
        return 0.0;
    }

    override float LFPG_GetCapacity()
    {
        return LFPG_BATTERY_SMALL_MAX_OUTPUT;
    }

    override bool LFPG_IsSource()
    {
        return true;
    }

    override bool LFPG_GetSourceOn()
    {
        return m_PoweredNet;
    }

    override bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    override void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;
        m_PoweredNet = powered;
        SetSynchDirty();
        #endif
    }

    override bool LFPG_GetOverloaded()
    {
        return m_Overloaded;
    }

    override void LFPG_SetOverloaded(bool val)
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
    // Lifecycle hooks
    // ============================================
    override void LFPG_OnInitDevice()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().RegisterBattery(this);

        if (!m_LoadedFromPersistence)
        {
            LFPG_InitFreshSpawn();
        }

        LFPG_SyncCompEM();
        #endif
    }

    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        LFPG_NetworkManager.Get().UnregisterBattery(this);
        #endif
    }

    override void LFPG_OnDeleted()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterBattery(this);
        #endif
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        #endif
    }

    // ============================================
    // VarSync: LEDs (overridden by tier subclasses)
    // ============================================
    override void LFPG_OnVarSyncDevice()
    {
        #ifndef SERVER
        LFPG_UpdateLEDs();
        #endif
    }

    // ============================================
    // Persistence: StoredEnergy + DischargeEnabled + OutputEnabled
    // (after wireJSON from WireOwnerBase)
    // ============================================
    override void LFPG_OnStoreSaveDevice(ParamsWriteContext ctx)
    {
        ctx.Write(m_StoredEnergy);
        ctx.Write(m_DischargeEnabled);
        ctx.Write(m_OutputEnabled);
    }

    override bool LFPG_OnStoreLoadDevice(ParamsReadContext ctx, int deviceVer)
    {
        m_LoadedFromPersistence = true;

        if (!ctx.Read(m_StoredEnergy))
        {
            string errStored = "[LFPG_Battery] OnStoreLoad failed: m_StoredEnergy";
            LFPG_Util.Error(errStored);
            return false;
        }

        if (!ctx.Read(m_DischargeEnabled))
        {
            string errDisch = "[LFPG_Battery] OnStoreLoad failed: m_DischargeEnabled";
            LFPG_Util.Error(errDisch);
            return false;
        }

        if (!ctx.Read(m_OutputEnabled))
        {
            string errOutput = "[LFPG_Battery] OnStoreLoad failed: m_OutputEnabled";
            LFPG_Util.Error(errOutput);
            return false;
        }

        return true;
    }

    // ============================================
    // Battery API — read by NetworkManager timer
    // ============================================
    float LFPG_GetStoredEnergy()
    {
        return m_StoredEnergy;
    }

    void LFPG_SetStoredEnergy(float val)
    {
        #ifdef SERVER
        m_StoredEnergy = val;

        LFPG_SyncCompEM();

        float maxStored = LFPG_GetMaxStoredEnergy();
        float threshold = maxStored * LFPG_BATTERY_SYNC_THRESHOLD_PCT;
        bool needsSync = false;

        if (m_LastSyncedStored < 0.0)
        {
            needsSync = true;
        }
        else
        {
            float cumDelta = val - m_LastSyncedStored;
            if (cumDelta < 0.0)
            {
                cumDelta = -cumDelta;
            }
            if (cumDelta > threshold)
            {
                needsSync = true;
            }

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
            // v4.2: Sync quantity bar alongside SyncVars.
            // With isPassiveDevice=1 + canWork=0, vanilla CompEM never
            // runs convertEnergyToQuantity. Explicit SetQuantity drives
            // the white charge bar in inventory item preview.
            SetQuantity(val, false, false, false);
        }
        #endif
    }

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
        #endif
    }

    bool LFPG_IsOutputEnabled()
    {
        return m_OutputEnabled;
    }

    void LFPG_SetOutputEnabled(bool val)
    {
        #ifdef SERVER
        m_OutputEnabled = val;
        SetSynchDirty();
        #endif
    }

    void LFPG_SetChargeRateCurrent(float val)
    {
        #ifdef SERVER
        // v4.2: Sync on sign change OR significant magnitude delta.
        // Previous sign-only sync left the client frozen at a stale value
        // when the rate changed within the same sign (e.g. -164 → -30).
        bool needsSync = false;

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

        if (oldSign != newSign)
        {
            needsSync = true;
        }

        // Magnitude delta threshold: sync if rate changed by > 2 u/s
        float rateDelta = val - m_ChargeRateCurrent;
        if (rateDelta < 0.0)
        {
            rateDelta = -rateDelta;
        }
        if (rateDelta > 2.0)
        {
            needsSync = true;
        }

        m_ChargeRateCurrent = val;

        if (needsSync)
        {
            SetSynchDirty();
        }
        #endif
    }

    float LFPG_GetChargeRateCurrent()
    {
        return m_ChargeRateCurrent;
    }

    void LFPG_InitFreshSpawn()
    {
        m_StoredEnergy = 0.0;
        SetSynchDirty();
    }

    // Virtual hook for tier LED updates (client only).
    // Base no-op: subclasses override for their LED patterns.
    void LFPG_UpdateLEDs()
    {
    }
};

// =========================================================
// TIER 2: Medium (base standard) — UPS model with 7 LEDs
// =========================================================

static const string LFPG_BAT_MED_LED_GREEN = "\LFPowerGrid\data\battery_medium\ups_led_green.rvmat";
static const string LFPG_BAT_MED_LED_OFF   = "\LFPowerGrid\data\battery_medium\ups_led_off.rvmat";
static const int    LFPG_BAT_MED_LED_COUNT  = 7;

class LFPG_BatteryMedium : LFPG_BatteryBase
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

    override bool LFPG_IsGateCapable()
    {
        return true;
    }

    override bool LFPG_IsGateOpen()
    {
        return m_OutputEnabled;
    }

    override void LFPG_UpdateLEDs()
    {
        #ifndef SERVER
        if (m_OutputEnabled)
        {
            string animSwOn = "switch";
            SetAnimationPhase(animSwOn, 1.0);
        }
        else
        {
            string animSwOff = "switch";
            SetAnimationPhase(animSwOff, 0.0);
        }

        float maxStored = LFPG_GetMaxStoredEnergy();
        float ratio = 0.0;
        if (maxStored > 0.1)
        {
            ratio = m_StoredEnergy / maxStored;
        }
        if (ratio > 1.0)
        {
            ratio = 1.0;
        }

        float litFloat = ratio * 7.0;
        int numLit = litFloat;
        if (numLit < 0)
        {
            numLit = 0;
        }
        if (numLit < 1 && m_StoredEnergy > 0.1)
        {
            numLit = 1;
        }
        if (numLit > LFPG_BAT_MED_LED_COUNT)
        {
            numLit = LFPG_BAT_MED_LED_COUNT;
        }

        int i = 0;
        for (i = 0; i < LFPG_BAT_MED_LED_COUNT; i = i + 1)
        {
            if (i < numLit)
            {
                SetObjectMaterial(i, LFPG_BAT_MED_LED_GREEN);
            }
            else
            {
                SetObjectMaterial(i, LFPG_BAT_MED_LED_OFF);
            }
        }
        #endif
    }
};

// =========================================================
// TIER 3: Large (industrial grid bank) — transformer model with 7 LEDs
// =========================================================

static const string LFPG_BAT_LRG_LED_GREEN = "\LFPowerGrid\data\battery_large\substation_transformer_led_green.rvmat";
static const string LFPG_BAT_LRG_LED_OFF   = "\LFPowerGrid\data\battery_large\substation_transformer_led_off.rvmat";
static const int    LFPG_BAT_LRG_LED_COUNT  = 7;

class LFPG_BatteryLarge : LFPG_BatteryBase
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

    override bool LFPG_IsGateCapable()
    {
        return true;
    }

    override bool LFPG_IsGateOpen()
    {
        return m_OutputEnabled;
    }

    override void LFPG_UpdateLEDs()
    {
        #ifndef SERVER
        if (m_OutputEnabled)
        {
            string animSwOn = "switch";
            SetAnimationPhase(animSwOn, 1.0);
        }
        else
        {
            string animSwOff = "switch";
            SetAnimationPhase(animSwOff, 0.0);
        }

        float maxStored = LFPG_GetMaxStoredEnergy();
        float ratio = 0.0;
        if (maxStored > 0.1)
        {
            ratio = m_StoredEnergy / maxStored;
        }
        if (ratio > 1.0)
        {
            ratio = 1.0;
        }

        float litFloat = ratio * 7.0;
        int numLit = litFloat;
        if (numLit < 0)
        {
            numLit = 0;
        }
        if (numLit < 1 && m_StoredEnergy > 0.1)
        {
            numLit = 1;
        }
        if (numLit > LFPG_BAT_LRG_LED_COUNT)
        {
            numLit = LFPG_BAT_LRG_LED_COUNT;
        }

        int i = 0;
        for (i = 0; i < LFPG_BAT_LRG_LED_COUNT; i = i + 1)
        {
            if (i < numLit)
            {
                SetObjectMaterial(i, LFPG_BAT_LRG_LED_GREEN);
            }
            else
            {
                SetObjectMaterial(i, LFPG_BAT_LRG_LED_OFF);
            }
        }
        #endif
    }
};
