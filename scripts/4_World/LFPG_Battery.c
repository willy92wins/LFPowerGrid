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
// LFPG_BatteryMedium:   720,000 u capacity, 50 chg, 70 dis, 90% eff
// LFPG_BatteryLarge:    3,600,000 u capacity, 80 chg, 120 dis, 88% eff
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
    // F6 B1: idempotent re-registration point for the OnInit sweep
    // (devices restored during super.OnInit() registered against the
    // inert fallback). RegisterX dedups; this replicates only the
    // registration condition, never init side effects.
    override void LFPG_RegisterWithNetworkManager(LFPG_NetworkManager nm)
    {
        if (nm) nm.RegisterBattery(this);
    }

    // ---- Device-specific SyncVars ----
    protected bool  m_PoweredNet       = false;
    protected bool  m_Overloaded       = false;
    protected bool  m_OutputEnabled    = true;
	// v4.5: Synced as int (×10) for reliable SyncVar delivery — same
    // pattern as m_ChargeRateX10. See Bohemia ticket T198078 (two
    // RegisterNetSyncVariableFloat calls with mismatched bit-widths on the
    // same entity class corrupt the second float on the client). Keeping
    // zero floats in the SyncVar bitstream eliminates the bug by
    // construction. 0.1 u resolution; storage up to 3600000 u × 10 fits in
    // int32 by two orders of magnitude.
    protected int   m_StoredEnergyX10  = 0;
    protected int   m_ChargeRateX10    = 0;

    // ---- Battery state (persisted, not SyncVars) ----
	// Authoritative float; the X10 SyncVar is only a client display snapshot.
	protected float m_StoredEnergy = 0.0;
    protected bool m_DischargeEnabled = true;

    // ---- Sync tracking (server-only, not persisted) ----
    // STORED_ENERGY_SYNC_MIN_MS is a floor for a faster future caller.
    // The only accounting path today ticks ~5 s, so this constant does
    // not set publish cadence. Charge travels by SyncVar; the inspect
    // panel polls it every 500 ms (LFPG_INSPECT_REFRESH_MS). The 2000 ms
    // inspector interval is the topology RPC, not this value.
    static const int STORED_ENERGY_SYNC_MIN_MS = 2000;
    protected float m_LastSyncedStored = -1.0;
    protected int m_LastStoredSyncMs = -1;

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
        string varStored      = "m_StoredEnergyX10";
        string varChargeRate  = "m_ChargeRateX10";

        RegisterNetSyncVariableBool(varPowered);
        RegisterNetSyncVariableBool(varOverloaded);
        RegisterNetSyncVariableBool(varOutput);
        // v4.5: Both stored energy and charge rate are Int SyncVars (×10).
        // Ticket T198078: registering any Float SyncVar alongside another
        // Float of different bit-width (e.g. vanilla Inventory_Base floats)
        // corrupts the second float on the client. Using only Int+Bool
        // SyncVars on this class removes the precondition entirely.
        //
        // v4.5.1: Explicit ranges required. Unranged RegisterNetSyncVariableInt
        // defaults to a narrow bit-width (observed ~16 bits) → values above
        // 65535 wrap (e.g. BatteryMedium at 60% stored = 4320000 X10).
        // Large battery X10 max = 36000000.
        RegisterNetSyncVariableInt(varStored, 0, 50000000);
        RegisterNetSyncVariableInt(varChargeRate, -2000, 2000);
    }

    // The kit carries no stored energy; empty the device before dismantling.
    override bool LFPG_BlocksDismantle()
    {
        return m_StoredEnergyX10 > 0;
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
        return LFPG_GetMappedPortWorldPos(portName, 0.02, 0.05, 0.02, true);
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
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm) nm.RegisterBattery(this);

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
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm) nm.UnregisterBattery(this);
        #endif
    }

    override void LFPG_OnDeleted()
    {
        #ifdef SERVER
        LFPG_NetworkManager nm = LFPG_NetworkManager.GetExisting();
        if (nm) nm.UnregisterBattery(this);
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
    // Battery schema v3: v2 energy is scaled by 36. v1 is rejected.
    // ============================================
    static const int LFPG_BATTERY_PERSIST_VERSION = 3;
    static const int LFPG_BATTERY_PRE_X36_VERSION = 2;
    static const float LFPG_BATTERY_MIGRATION_FACTOR = 36.0;

    override int LFPG_GetDevicePersistVersion()
    {
        return LFPG_BATTERY_PERSIST_VERSION;
    }

    override void LFPG_OnStoreSaveDevice(ParamsWriteContext ctx)
    {
        // Disk format stays float for backward compat with existing saves.
		ctx.Write(m_StoredEnergy);
        ctx.Write(m_DischargeEnabled);
        ctx.Write(m_OutputEnabled);
    }

    override bool LFPG_OnStoreLoadDevice(ParamsReadContext ctx, int deviceVer)
    {
        bool dischargeEnabled = true;
        bool outputEnabled = true;
        float storedFromSave = 0.0;
        string errStored = "[LFPG_Battery] OnStoreLoad failed: m_StoredEnergy";
        string errDisch = "[LFPG_Battery] OnStoreLoad failed: m_DischargeEnabled";
        string errOutput = "[LFPG_Battery] OnStoreLoad failed: m_OutputEnabled";
        string logMsg = "";
        string className = "";
        string deviceId = "";
        float maxStored = 0.0;
        float legacyMax = 0.0;
        bool didCorrect = false;

        if (!ctx.Read(storedFromSave))
        {
            LFPG_Util.Error(errStored);
            return false;
        }
        // Non-finite persisted energy survives the cast as garbage; reset to
        // empty instead. Same policy as the persisted aim guard in LFPG_Searchlight.
        if (LFPG_Searchlight.LFPG_IsInvalidAimValue(storedFromSave))
        {
            LFPG_Util.Warn("[LFPG_Battery] Non-finite persisted energy reset to zero");
            storedFromSave = 0.0;
        }

        if (!ctx.Read(dischargeEnabled))
        {
            LFPG_Util.Error(errDisch);
            return false;
        }

        if (!ctx.Read(outputEnabled))
        {
            LFPG_Util.Error(errOutput);
            return false;
        }

        className = GetType();
        deviceId = m_DeviceId;

        if (deviceVer < 1)
        {
            logMsg = "[LFPG_Battery] Rejecting persist version=";
            logMsg = logMsg + deviceVer.ToString();
            logMsg = logMsg + " type=";
            logMsg = logMsg + className;
            logMsg = logMsg + " id=";
            logMsg = logMsg + deviceId;
            LFPG_Util.Warn(logMsg);
            return false;
        }

        if (deviceVer == 1)
        {
            logMsg = "[LFPG_Battery] Rejecting ambiguous persist v1 type=";
            logMsg = logMsg + className;
            logMsg = logMsg + " id=";
            logMsg = logMsg + deviceId;
            LFPG_Util.Warn(logMsg);
            return false;
        }

        if (deviceVer == LFPG_BATTERY_PRE_X36_VERSION)
        {
            maxStored = LFPG_GetMaxStoredEnergy();
            legacyMax = maxStored / LFPG_BATTERY_MIGRATION_FACTOR;
            if (storedFromSave < 0.0)
            {
                storedFromSave = 0.0;
                didCorrect = true;
            }
            if (storedFromSave > legacyMax)
            {
                storedFromSave = legacyMax;
                didCorrect = true;
            }
            if (didCorrect)
            {
                logMsg = "[LFPG_Battery] Clamped v2 energy to 0..legacyMax type=";
                logMsg = logMsg + className;
                logMsg = logMsg + " id=";
                logMsg = logMsg + deviceId;
                LFPG_Util.Warn(logMsg);
            }
            storedFromSave = storedFromSave * LFPG_BATTERY_MIGRATION_FACTOR;
            logMsg = "[LFPG_Battery] battery_migration v2->v3 type=";
            logMsg = logMsg + className;
            logMsg = logMsg + " id=";
            logMsg = logMsg + deviceId;
            logMsg = logMsg + " energy=";
            logMsg = logMsg + storedFromSave.ToString();
            LFPG_Util.Info(logMsg);
        }
        else if (deviceVer == LFPG_BATTERY_PERSIST_VERSION)
        {
            maxStored = LFPG_GetMaxStoredEnergy();
            if (storedFromSave < 0.0)
            {
                storedFromSave = 0.0;
            }
            if (storedFromSave > maxStored)
            {
                storedFromSave = maxStored;
            }
        }
        else
        {
            logMsg = "[LFPG_Battery] Rejecting unsupported persist version=";
            logMsg = logMsg + deviceVer.ToString();
            logMsg = logMsg + " type=";
            logMsg = logMsg + className;
            logMsg = logMsg + " id=";
            logMsg = logMsg + deviceId;
            LFPG_Util.Warn(logMsg);
            return false;
        }

        m_DischargeEnabled = dischargeEnabled;
        m_OutputEnabled = outputEnabled;
        m_LoadedFromPersistence = true;
        LFPG_SetStoredEnergy(storedFromSave);
        return true;
    }

    // ============================================
    // Battery API — read by NetworkManager timer
    // ============================================
	float LFPG_GetStoredEnergy()
	{
		#ifdef SERVER
		return m_StoredEnergy;
		#else
		float result = m_StoredEnergyX10;
		result = result / 10.0;
		return result;
		#endif
	}

    void LFPG_SetStoredEnergy(float val)
    {
        #ifdef SERVER
		m_StoredEnergy = val;
        int newX10 = val * 10.0;
        m_StoredEnergyX10 = newX10;

        LFPG_SyncCompEM();

        float maxStored = LFPG_GetMaxStoredEnergy();
        float threshold = maxStored * LFPG_BATTERY_SYNC_THRESHOLD_PCT;
        bool needsSync = false;
        int nowMs = g_Game.GetTime();
        int elapsedMs = 0;
        int lastSyncedInt = 0;
        int newStoredInt = val;

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

            // Percentage threshold scales with capacity. Temporal publish
            // follows the visible integer unit, not the X10 tenth: the
            // inspector prints ints and no other client consumer shows
            // tenths. Accounting ~5 s is the effective cadence; the 2000 ms
            // floor does not bite on that caller.
            lastSyncedInt = m_LastSyncedStored;
            if (newStoredInt != lastSyncedInt)
            {
                elapsedMs = nowMs - m_LastStoredSyncMs;
                if (elapsedMs < 0)
                {
                    elapsedMs = STORED_ENERGY_SYNC_MIN_MS;
                }
                if (elapsedMs >= STORED_ENERGY_SYNC_MIN_MS)
                {
                    needsSync = true;
                }
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
            m_LastStoredSyncMs = nowMs;
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
            em.SetEnergy(LFPG_GetStoredEnergy());
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
        // v4.4: Store as int (rate × 10) for reliable SyncVar delivery.
        // The quantized rate is synchronized only when its integer value changes.
        int rateX10 = val * 10.0;
        if (m_ChargeRateX10 == rateX10)
            return;

        m_ChargeRateX10 = rateX10;
        SetSynchDirty();
        #endif
    }

    float LFPG_GetChargeRateCurrent()
    {
        float result = m_ChargeRateX10;
        result = result / 10.0;
        return result;
    }

    void LFPG_InitFreshSpawn()
    {
		m_StoredEnergy = 0.0;
        m_StoredEnergyX10 = 0;
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
        float storedFloat = LFPG_GetStoredEnergy();
        float ratio = 0.0;
        if (maxStored > 0.1)
        {
            ratio = storedFloat / maxStored;
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
        if (numLit < 1 && storedFloat > 0.1)
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
        float storedFloat = LFPG_GetStoredEnergy();
        float ratio = 0.0;
        if (maxStored > 0.1)
        {
            ratio = storedFloat / maxStored;
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
        if (numLit < 1 && storedFloat > 0.1)
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
