// =========================================================
// LF_PowerGrid - Battery device (v4.0 Refactor)
//
// LF_Battery_Kit:     Deployable kit → spawns BatterySmall.
// LF_BatteryMedium_Kit: Deployable kit → spawns BatteryMedium.
// LF_BatteryLarge_Kit:  DeployableContainer_Base → spawns BatteryLarge.
//
// LF_BatteryBase:     PASSTHROUGH (1 IN + 1 OUT) with energy storage.
//                     Charges from surplus, discharges to supplement.
//
// v4.0: Migrated from Inventory_Base to LFPG_WireOwnerBase.
//   Wire store, wire API, persistence wireJSON, CanConnectTo — all in base.
//   GetPortWorldPos override: p3d uses port_input_0/port_output_0.
//
// LF_BatterySmall:    2,000 u capacity, 30 chg, 40 dis, 92% eff
// LF_BatteryMedium:   10,000 u capacity, 50 chg, 70 dis, 90% eff
// LF_BatteryLarge:    50,000 u capacity, 80 chg, 120 dis, 88% eff
// =========================================================

// ---------------------------------------------------------
// KIT (SMALL): same-model deploy pattern
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

    override string GetLoopDeploySoundset()
    {
        string empty = "";
        return empty;
    }
};

// ---------------------------------------------------------
// KIT (MEDIUM)
// ---------------------------------------------------------
class LF_BatteryMedium_Kit : Inventory_Base
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
        string snd = "placeBarbedWire_SoundSet";
        return snd;
    }

    override string GetLoopDeploySoundset()
    {
        string empty = "";
        return empty;
    }

    override void SetActions()
    {
        super.SetActions();
        AddAction(ActionTogglePlaceObject);
        AddAction(LFPG_ActionPlaceBatteryMedium);
    }

    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[BatteryMedium_Kit] OnPlacementComplete: param=";
        tLog = tLog + position.ToString();
        tLog = tLog + " kitPos=";
        tLog = tLog + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        string spawnClass = "LF_BatteryMedium";
        EntityAI battery = GetGame().CreateObjectEx(spawnClass, finalPos, ECE_CREATEPHYSICS);
        if (battery)
        {
            battery.SetPosition(finalPos);
            battery.SetOrientation(finalOri);
            battery.Update();

            string okLog = "[BatteryMedium_Kit] Deployed LF_BatteryMedium at ";
            okLog = okLog + finalPos.ToString();
            okLog = okLog + " ori=";
            okLog = okLog + finalOri.ToString();
            LFPG_Util.Info(okLog);

            GetGame().ObjectDelete(this);
        }
        else
        {
            string failLog = "[BatteryMedium_Kit] Failed to create LF_BatteryMedium! Kit preserved.";
            LFPG_Util.Error(failLog);
            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                string errMsg = "[LFPG] Battery placement failed. Kit preserved.";
                pb.MessageStatus(errMsg);
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// KIT (LARGE): DeployableContainer_Base pattern
// ---------------------------------------------------------
class LF_BatteryLarge_Kit : DeployableContainer_Base
{
    string GetDeployedClassname()
    {
        string cls = "LF_BatteryLarge";
        return cls;
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

        if (!GetGame().IsDedicatedServer())
            return;

        PlayerBase pb = PlayerBase.Cast(player);
        if (!pb)
            return;

        string spawnClass = GetDeployedClassname();
        vector spawnPos = pb.GetLocalProjectionPosition();

        string tLog = "[BatteryLarge_Kit] OnPlacementComplete: param=";
        tLog = tLog + position.ToString();
        tLog = tLog + " kitPos=";
        tLog = tLog + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        EntityAI battery = GetGame().CreateObject(spawnClass, spawnPos, false);
        if (!battery)
        {
            string errLog = "[BatteryLarge_Kit] Failed to create LF_BatteryLarge! Kit preserved.";
            LFPG_Util.Error(errLog);
            string errMsg = "[LFPG] Battery placement failed. Kit preserved.";
            pb.MessageStatus(errMsg);
            return;
        }

        battery.SetPosition(position);
        battery.SetOrientation(orientation);

        SetIsDeploySound(true);

        string okLog = "[BatteryLarge_Kit] Deployed LF_BatteryLarge at pos=";
        okLog = okLog + position.ToString();
        okLog = okLog + " ori=";
        okLog = okLog + orientation.ToString();
        LFPG_Util.Info(okLog);

        this.DeleteSafe();
    }

    override bool IsBasebuildingKit()
    {
        return true;
    }

    override bool IsDeployable()
    {
        return true;
    }

    override string GetLoopDeploySoundset()
    {
        string empty = "";
        return empty;
    }

    override void SetActions()
    {
        super.SetActions();
        AddAction(ActionTogglePlaceObject);
        AddAction(ActionPlaceObject);
    }
};

// ---------------------------------------------------------
// DEVICE BASE: PASSTHROUGH : LFPG_WireOwnerBase
// Tiers inherit and override capacity/rates.
// ---------------------------------------------------------
class LF_BatteryBase : LFPG_WireOwnerBase
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
    void LF_BatteryBase()
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
            string errStored = "[LF_Battery] OnStoreLoad failed: m_StoredEnergy";
            LFPG_Util.Error(errStored);
            return false;
        }

        if (!ctx.Read(m_DischargeEnabled))
        {
            string errDisch = "[LF_Battery] OnStoreLoad failed: m_DischargeEnabled";
            LFPG_Util.Error(errDisch);
            return false;
        }

        if (!ctx.Read(m_OutputEnabled))
        {
            string errOutput = "[LF_Battery] OnStoreLoad failed: m_OutputEnabled";
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

        if (oldSign != newSign)
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
    // Base no-op: BatterySmall has no LED selections.
    void LFPG_UpdateLEDs()
    {
    }
};

// =========================================================
// TIER 1: Small (portable backup)
// =========================================================
class LF_BatterySmall : LF_BatteryBase
{
    // Uses base class defaults (SMALL constants).
};

// =========================================================
// TIER 2: Medium (base standard) — UPS model with 7 LEDs
// =========================================================

static const string LFPG_BAT_MED_LED_GREEN = "\\LFPowerGrid\\data\\battery_medium\\ups_led_green.rvmat";
static const string LFPG_BAT_MED_LED_OFF   = "\\LFPowerGrid\\data\\battery_medium\\ups_led_off.rvmat";
static const int    LFPG_BAT_MED_LED_COUNT  = 7;

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
