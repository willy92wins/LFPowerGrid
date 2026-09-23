// =========================================================
// LF_PowerGrid - Furnace (Incinerator) SOURCE device (v4.0 Refactor)
//
// LFPG_Furnace_Kit:  DeployableContainer_Base pattern (different-model).
// LFPG_Furnace:      SOURCE device (50 u/s constant while burning).
//                  Burns any item for fuel. 1 output (output_1).
//
// v4.0: Migrated from Inventory_Base to LFPG_WireOwnerBase.
//   Wire store, wire API, persistence wireJSON, CanConnectTo — all in base.
//   FIX: Per-instance CallLater(30s) replaced with RegisterFurnace/
//   UnregisterFurnace in NM. BurnTick now checks m_BurnNextMs timing
//   (NM polls every 5s, burn fires every 30s).
//
// Memory point: port_output_1 (matches base pattern, no override needed).
// =========================================================

// ---------------------------------------------------------
// KIT: DeployableContainer_Base pattern
// ---------------------------------------------------------

class LFPG_Furnace_Kit : LFPG_KitBaseDeployable
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_Furnace";
    }
};

// ---------------------------------------------------------
// EFFECT: Furnace smoke (EffectParticle subclass for SEffectManager)
// ---------------------------------------------------------
#ifndef SERVER
class EffLFPGFurnaceSmoke : EffectParticle
{
    void EffLFPGFurnaceSmoke()
    {
        SetParticleID(ParticleList.LFPG_FURNACE_SMOKE);
    }
};
#endif

// ---------------------------------------------------------
// DEVICE — SOURCE : LFPG_WireOwnerBase
// 1 OUT (output_1), 50 u/s while burning
// ---------------------------------------------------------
// Immutable config properties shared by both fuel formulas, cached per classname.
class LFPG_FurnaceFuelConfig : Managed
{
	int m_Area = 0;
	bool m_CanBeSplit = false;
};

class LFPG_Furnace : LFPG_WireOwnerBase
{
    // F6 B1: idempotent re-registration point for the OnInit sweep
    // (devices restored during super.OnInit() registered against the
    // inert fallback). RegisterX dedups; this replicates only the
    // registration condition, never init side effects.
    override void LFPG_RegisterWithNetworkManager(LFPG_NetworkManager nm)
    {
        if (nm && m_SourceOn && m_FuelCurrent > 0) nm.RegisterFurnace(this);
    }

    // ---- Device-specific SyncVars ----
    protected bool  m_SourceOn     = false;
    protected float m_LoadRatio    = 0.0;
    protected bool  m_Overloaded   = false;
    protected int   m_FuelCurrent  = 0;
	protected static ref TStringManagedRefMap s_FuelConfig;

	// Server burn clock: persist the remaining duration, never mission time.
	// The duration pauses while off; NM polls the running deadline every 5s.
	protected int m_BurnNextMs = 0;
	protected int m_BurnRemainingMs = LFPG_FURNACE_BURN_INTERVAL_MS;

    // ---- Client: sound + particle ----
#ifndef SERVER
    protected EffectSound m_FurnaceLoopSound;
    protected ref Effect m_SmokeEffect;
#endif

    // ---- v4.7: Heat emission (UTS) ----
    protected ref UniversalTemperatureSource m_UTSource;
    protected ref UniversalTemperatureSourceSettings m_UTSSettings;

    // ============================================
    // Constructor — port + SyncVars
    // ============================================
    void LFPG_Furnace()
    {
        string pOut = "output_1";
        string lOut = "Output";
        LFPG_AddPort(pOut, LFPG_PortDir.OUT, lOut);

        string varSourceOn  = "m_SourceOn";
        string varLoadRatio = "m_LoadRatio";
        string varOverload  = "m_Overloaded";
        string varFuel      = "m_FuelCurrent";

        RegisterNetSyncVariableBool(varSourceOn);
        RegisterNetSyncVariableFloat(varLoadRatio, 0.0, 5.0, 2);
        RegisterNetSyncVariableBool(varOverload);
        RegisterNetSyncVariableInt(varFuel);
    }

    // The kit carries no stored fuel; empty the device before dismantling.
    override bool LFPG_BlocksDismantle()
    {
        return m_FuelCurrent > 0;
    }

    // ============================================
    // SetActions
    // ============================================
    override void SetActions()
    {
        super.SetActions();
        AddAction(LFPG_ActionToggleFurnace);
        AddAction(LFPG_ActionFeedFurnace);
    }

    // ============================================
    // Cargo display (furnace has 10x10 hopper cargo)
    // ============================================
    override bool CanDisplayCargo()
    {
        return true;
    }

    // ============================================
    // EEInit — v4.7: Create UTS for heat emission
    // ============================================
    override void EEInit()
    {
        super.EEInit();
		if (m_LFPG_IsHologramProjection)
			return;

        #ifdef SERVER
        LFPG_ServerSettings st = LFPG_Settings.Get();
        if (st.FurnaceHeatEnabled)
        {
            m_UTSSettings = new UniversalTemperatureSourceSettings();
            m_UTSSettings.m_Updateable = true;
            m_UTSSettings.m_ManualUpdate = false;
            float utsInterval = 3.0;
            m_UTSSettings.m_UpdateInterval = utsInterval;
            m_UTSSettings.m_RangeFull = st.FurnaceHeatFullWarmthRadiusM;
            m_UTSSettings.m_RangeMax = st.FurnaceHeatFadeOutRadiusM;
            float heatCap = LFPG_CAMPFIRE_HEAT_CAP * st.FurnaceHeatStrengthMultiplier;
            m_UTSSettings.m_TemperatureCap = heatCap;
            m_UTSSettings.m_TemperatureItemCap = GameConstants.ITEM_TEMPERATURE_NEUTRAL_ZONE_MIDDLE;

            UniversalTemperatureSourceLambdaConstant utsLambda = new UniversalTemperatureSourceLambdaConstant();
            m_UTSource = new UniversalTemperatureSource(this, m_UTSSettings, utsLambda);
			// Device init ran through super before this source existed.
			LFPG_SetHeatActive(m_SourceOn);
        }
        #endif
    }

    // v4.7: Helper to toggle UTS heat on/off
    protected void LFPG_SetHeatActive(bool active)
    {
        #ifdef SERVER
        if (m_UTSource)
        {
            m_UTSource.SetActive(active);
        }
        #endif
    }

    // v4.7: Prevent external heat sources from overriding furnace temp
    override bool IsSelfAdjustingTemperature()
    {
        return m_SourceOn;
    }

    // ============================================
    // Virtual interface — SOURCE
    // ============================================
    override int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.SOURCE;
    }

    override float LFPG_GetConsumption()
    {
        return 0.0;
    }

    override float LFPG_GetCapacity()
    {
        return LFPG_FURNACE_CAPACITY;
    }

    override bool LFPG_IsSource()
    {
        return true;
    }

    override bool LFPG_GetSourceOn()
    {
        return m_SourceOn;
    }

    override bool LFPG_IsPowered()
    {
        return m_SourceOn;
    }

    // SOURCE: SetPowered is no-op (power is self-driven via m_SourceOn)
    override void LFPG_SetPowered(bool powered)
    {
    }

    override float LFPG_GetLoadRatio()
    {
        return m_LoadRatio;
    }

    override void LFPG_SetLoadRatio(float ratio)
    {
        #ifdef SERVER
        if (ratio < 0.0)
        {
            ratio = 0.0;
        }

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

    bool LFPG_GetSwitchState()
    {
        return m_SourceOn;
    }

    // ============================================
    // Lifecycle hooks
    // ============================================
    override void LFPG_OnInitDevice()
    {
        #ifdef SERVER
        // Post-load restore: if furnace was on with fuel, register with NM
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (m_SourceOn && m_FuelCurrent > 0)
        {
            int now = g_Game.GetTime();
			m_BurnNextMs = now + m_BurnRemainingMs;
			LFPG_BurnTick();
			if (m_SourceOn && nm) nm.RegisterFurnace(this);
        }

        // Safety: source on but no fuel → try auto-consume
        if (m_SourceOn && m_FuelCurrent <= 0)
        {
            bool restoreConsumed = LFPG_AutoConsumeLargestItem();
            if (restoreConsumed)
            {
                int now2 = g_Game.GetTime();
				m_BurnRemainingMs = LFPG_FURNACE_BURN_INTERVAL_MS;
				m_BurnNextMs = now2 + m_BurnRemainingMs;
                if (nm) nm.RegisterFurnace(this);
            }
            else
            {
                m_SourceOn = false;
                m_FuelCurrent = 0;
                SetSynchDirty();
            }
        }

        // Propagate on init to rebuild graph edge allocations
        if (m_SourceOn && m_DeviceId != "")
        {
            if (nm) nm.RequestPropagate(m_DeviceId);
        }

        #endif
    }

    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        if (m_SourceOn)
        {
			m_BurnRemainingMs = LFPG_GetBurnRemainingMs();
            m_SourceOn = false;
            LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
            if (nm) nm.UnregisterFurnace(this);
            SetSynchDirty();
        }
        LFPG_SetHeatActive(false);
        #endif

        LFPG_CleanupClientFX();
    }

    override void LFPG_OnDeleted()
    {
        #ifdef SERVER
        LFPG_NetworkManager nm = LFPG_NetworkManager.GetExisting();
        if (nm) nm.UnregisterFurnace(this);
        LFPG_SetHeatActive(false);
        #endif

        LFPG_CleanupClientFX();
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        if (m_SourceOn)
        {
			m_BurnRemainingMs = LFPG_GetBurnRemainingMs();
            m_SourceOn = false;
            LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
            if (nm) nm.UnregisterFurnace(this);
            SetSynchDirty();

            LFPG_SetHeatActive(false);
        }
        #endif
    }

    // ============================================
    // Smoke position (relative to model)
    // ============================================
    #ifndef SERVER
    protected vector LFPG_GetSmokePosition()
    {
        string pos = "0.1 1.4 -0.1";
        return pos.ToVector();
    }
    #endif

    // ============================================
    // Client FX cleanup (sound + smoke)
    // ============================================
    protected void LFPG_CleanupClientFX()
    {
#ifndef SERVER
        SEffectManager.DestroyEffect(m_SmokeEffect);
        if (m_FurnaceLoopSound)
        {
            m_FurnaceLoopSound.SoundStop();
            m_FurnaceLoopSound = null;
        }
#endif
    }

    // ============================================
    // Destructor — safety net for SEffectManager
    // ============================================
    void ~LFPG_Furnace()
    {
#ifndef SERVER
        SEffectManager.DestroyEffect(m_SmokeEffect);
#endif

        // v4.7: Deactivate UTS before GC
        if (m_UTSource)
        {
            m_UTSource.SetActive(false);
        }
    }

    // ============================================
    // VarSync: loop sound + smoke particle (client-side)
    // ============================================
    override void LFPG_OnVarSyncDevice()
    {
        #ifndef SERVER
        // ---- Sound toggle ----
        if (m_SourceOn && !m_FurnaceLoopSound)
        {
            string soundSet = LFPG_FURNACE_LOOP_SOUNDSET;
            m_FurnaceLoopSound = SEffectManager.PlaySound(soundSet, GetPosition());
            if (m_FurnaceLoopSound)
            {
                m_FurnaceLoopSound.SetAutodestroy(false);
            }
        }

        if (!m_SourceOn && m_FurnaceLoopSound)
        {
            m_FurnaceLoopSound.SoundStop();
            m_FurnaceLoopSound = null;
        }

        // ---- Smoke toggle ----
        if (m_SourceOn && !m_SmokeEffect)
        {
            m_SmokeEffect = new EffLFPGFurnaceSmoke();
            vector smokePos = LFPG_GetSmokePosition();
            vector smokeOri = "0 0 0".ToVector();
            SEffectManager.PlayOnObject(m_SmokeEffect, this, smokePos, smokeOri);
        }

        if (!m_SourceOn && m_SmokeEffect)
        {
            SEffectManager.DestroyEffect(m_SmokeEffect);
        }
        #endif
    }

    // ============================================
	// Persistence v3: source state, fuel, remaining burn duration in ms.
	// v1/v2 had no duration; their first restored interval starts in full.
	// ============================================
	override int LFPG_GetDevicePersistVersion()
	{
		return 3;
	}

	protected int LFPG_GetBurnRemainingMs()
	{
		int remainingMs = m_BurnRemainingMs;
		if (m_SourceOn)
		{
			remainingMs = m_BurnNextMs - g_Game.GetTime();
		}
		if (remainingMs < 0)
		{
			remainingMs = 0;
		}
		return remainingMs;
	}

	override void LFPG_OnStoreSaveDevice(ParamsWriteContext ctx)
	{
		int remainingMs = LFPG_GetBurnRemainingMs();
		ctx.Write(m_SourceOn);
		ctx.Write(m_FuelCurrent);
		ctx.Write(remainingMs);
	}

	override bool LFPG_OnStoreLoadDevice(ParamsReadContext ctx, int deviceVer)
	{
		if (deviceVer < 1 || deviceVer > 3)
		{
			LFPG_Util.Error("[LFPG_Furnace] Unsupported persistence version");
			return false;
		}

		// v1/v2: bool sourceOn, int fuelCurrent (after WireOwnerBase wireJSON).
		// v3 appends int remainingMs. No fields are skipped in legacy records.
		bool sourceOn = false;
		int fuelCurrent = 0;
		int remainingMs = LFPG_FURNACE_BURN_INTERVAL_MS;
		if (!ctx.Read(sourceOn))
		{
			LFPG_Util.Error("[LFPG_Furnace] OnStoreLoad failed: m_SourceOn");
			return false;
		}
		if (!ctx.Read(fuelCurrent))
		{
			LFPG_Util.Error("[LFPG_Furnace] OnStoreLoad failed: m_FuelCurrent");
			return false;
		}
		if (deviceVer == 3)
		{
			if (!ctx.Read(remainingMs))
			{
				LFPG_Util.Error("[LFPG_Furnace] OnStoreLoad failed: remaining burn duration");
				return false;
			}
			if (remainingMs < 0 || remainingMs > LFPG_FURNACE_BURN_INTERVAL_MS)
			{
				LFPG_Util.Error("[LFPG_Furnace] Invalid remaining burn duration");
				return false;
			}
		}

		// AddFuel and AutoConsume cap live fuel at MAX_FUEL in every schema.
		// Repair invalid values without dropping the entity or its wire payload.
		if (fuelCurrent < 0 || fuelCurrent > LFPG_FURNACE_MAX_FUEL)
		{
			LFPG_Util.Warn("[LFPG_Furnace] Persisted fuel outside 0..MAX_FUEL; clamping");
			if (fuelCurrent < 0)
				fuelCurrent = 0;
			else
				fuelCurrent = LFPG_FURNACE_MAX_FUEL;
		}
		m_SourceOn = sourceOn;
		m_FuelCurrent = fuelCurrent;
		m_BurnRemainingMs = remainingMs;
		m_BurnNextMs = 0;
		return true;
	}

    // ============================================
    // Burn tick (called by NM every ~5s, fires burn every 30s)
    // FIX v4.0: Replaces per-instance CallLater(30s, repeat)
    // which caused heap fragmentation crash after 4.5h.
    // ============================================
    void LFPG_BurnTick()
    {
        #ifdef SERVER
        if (!m_SourceOn)
            return;

        // Timing gate: NM polls every 5s, but burn happens every 30s
        int now = g_Game.GetTime();
        if (now < m_BurnNextMs)
            return;

		m_BurnRemainingMs = LFPG_FURNACE_BURN_INTERVAL_MS;
		m_BurnNextMs = now + m_BurnRemainingMs;

        if (m_FuelCurrent > 0)
        {
            m_FuelCurrent = m_FuelCurrent - 1;
            SetSynchDirty();
        }

        if (m_FuelCurrent <= 0)
        {
            m_FuelCurrent = 0;

            bool consumed = LFPG_AutoConsumeLargestItem();
            if (!consumed)
            {
                m_SourceOn = false;
                LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
                if (nm) nm.UnregisterFurnace(this);
                SetSynchDirty();
                if (m_DeviceId != "")
                {
                    if (nm) nm.RequestPropagate(m_DeviceId);
                }
                if (LFPG_LOG_LEVEL >= 1)
                {
                    string offMsg = "[LFPG_Furnace] Fuel exhausted + cargo empty, auto-off. id=";
                    offMsg = offMsg + m_DeviceId;
                    LFPG_Util.Info(offMsg);
                }

                LFPG_SetHeatActive(false);
            }
        }
        #endif
    }

    // ============================================
    // Toggle power (server only, called from ActionToggleFurnace)
    // ============================================
    void LFPG_ToggleFurnace()
    {
        #ifdef SERVER
        if (m_SourceOn)
        {
			m_BurnRemainingMs = LFPG_GetBurnRemainingMs();
            m_SourceOn = false;
            LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
            if (nm) nm.UnregisterFurnace(this);
            SetSynchDirty();
            if (m_DeviceId != "")
            {
                if (nm) nm.RequestPropagate(m_DeviceId);
            }
            string offMsg = "[LFPG_Furnace] Toggled OFF. fuel=";
            offMsg = offMsg + m_FuelCurrent.ToString();
            offMsg = offMsg + " id=";
            offMsg = offMsg + m_DeviceId;
            LFPG_Util.Info(offMsg);

            LFPG_SetHeatActive(false);
        }
        else
        {
            bool canIgnite = false;
            if (m_FuelCurrent > 0)
            {
                canIgnite = true;
            }
            else
            {
                bool igniteConsumed = LFPG_AutoConsumeLargestItem();
                if (igniteConsumed)
                {
					m_BurnRemainingMs = LFPG_FURNACE_BURN_INTERVAL_MS;
                    canIgnite = true;
                }
            }

            if (canIgnite)
            {
                m_SourceOn = true;
                int now = g_Game.GetTime();
				m_BurnNextMs = now + m_BurnRemainingMs;
				// Settle an overdue interval before exposing power again.
				LFPG_BurnTick();
				if (!m_SourceOn)
					return;
                LFPG_NetworkManager nm2 = LFPG_NetworkManager.Get();
                if (nm2) nm2.RegisterFurnace(this);
                SetSynchDirty();
                if (m_DeviceId != "")
                {
                    if (nm2) nm2.RequestPropagate(m_DeviceId);
                }
                string onMsg = "[LFPG_Furnace] Toggled ON. fuel=";
                onMsg = onMsg + m_FuelCurrent.ToString();
                onMsg = onMsg + " id=";
                onMsg = onMsg + m_DeviceId;
                LFPG_Util.Info(onMsg);

                LFPG_SetHeatActive(true);
            }
        }
        #endif
    }

    // ============================================
    // Fuel system
    // ============================================
	protected static LFPG_FurnaceFuelConfig LFPG_GetFuelConfig(string itemType)
	{
		if (!s_FuelConfig)
			s_FuelConfig = new TStringManagedRefMap;
		Managed cachedConfig;
		if (s_FuelConfig.Find(itemType, cachedConfig))
			return LFPG_FurnaceFuelConfig.Cast(cachedConfig);

		LFPG_FurnaceFuelConfig fuelConfig = new LFPG_FurnaceFuelConfig();
		string cfgPath = "CfgVehicles " + itemType + " itemSize";
		if (g_Game.ConfigIsExisting(cfgPath))
		{
			TIntArray sizeArr = new TIntArray;
			g_Game.ConfigGetIntArray(cfgPath, sizeArr);
			if (sizeArr.Count() >= 2)
				fuelConfig.m_Area = sizeArr[0] * sizeArr[1];
		}
		string splitPath = "CfgVehicles " + itemType + " canBeSplit";
		fuelConfig.m_CanBeSplit = g_Game.ConfigGetInt(splitPath) > 0;
		s_FuelConfig.Insert(itemType, fuelConfig);
		return fuelConfig;
	}

    int LFPG_CalcFuelRecursive(EntityAI item)
    {
        if (!item)
            return 0;

		LFPG_FurnaceFuelConfig fuelConfig = LFPG_GetFuelConfig(item.GetType());
		int qty = 1;
		int fuel = 0;
		if (fuelConfig.m_CanBeSplit)
        {
            int rawQty = item.GetQuantity();
            if (rawQty > 1)
            {
                qty = rawQty;
            }
        }

		fuel = fuelConfig.m_Area * qty;

        GameInventory inv = item.GetInventory();
        if (!inv) return fuel;
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

        int attCount = item.GetInventory().AttachmentCount();
        int ai = 0;
        for (ai = 0; ai < attCount; ai = ai + 1)
        {
            EntityAI att = item.GetInventory().GetAttachmentFromIndex(ai);
            fuel = fuel + LFPG_CalcFuelRecursive(att);
        }

        return fuel;
    }

    // v4.7: Calculate fuel for a single item using whitelist config.
    // Only checks the top-level item type (no recursive contents).
    // Returns 0 if item is not in the whitelist.
    // Fuel units = (burnTimeSec / 30) * quantity.
    int LFPG_CalcFuelWhitelist(EntityAI item)
    {
        if (!item)
            return 0;

        string itemType = item.GetType();
        int burnSec = LFPG_Settings.GetWhitelistFuelSec(itemType);
        if (burnSec <= 0)
            return 0;

        int fuelPerUnit = burnSec / 30;
        if (fuelPerUnit <= 0)
        {
            fuelPerUnit = 1;
        }

        int qty = 1;
		LFPG_FurnaceFuelConfig fuelConfig = LFPG_GetFuelConfig(itemType);
		if (fuelConfig.m_CanBeSplit)
        {
            int rawQty = item.GetQuantity();
            if (rawQty > 1)
            {
                qty = rawQty;
            }
        }

        return fuelPerUnit * qty;
    }

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

        string fuelMsg = "[LFPG_Furnace] Fuel added: +";
        fuelMsg = fuelMsg + amount.ToString();
        fuelMsg = fuelMsg + " total=";
        fuelMsg = fuelMsg + m_FuelCurrent.ToString();
        fuelMsg = fuelMsg + " id=";
        fuelMsg = fuelMsg + m_DeviceId;
        LFPG_Util.Info(fuelMsg);
        #endif
    }

    int LFPG_GetFuelCurrent()
    {
        return m_FuelCurrent;
    }

    bool LFPG_HasCargoItems()
    {
        CargoBase cargo = GetInventory().GetCargo();
        if (!cargo)
            return false;

        return cargo.GetItemCount() > 0;
    }

    #ifndef SERVER
    int LFPG_GetCargoItemCount()
    {
        CargoBase cargo = GetInventory().GetCargo();
        if (!cargo)
            return 0;

        return cargo.GetItemCount();
    }
    #endif

    #ifndef SERVER
    int LFPG_GetCargoFuelEstimate()
    {
        CargoBase cargo = GetInventory().GetCargo();
        if (!cargo)
            return 0;

        int total = 0;
        int count = cargo.GetItemCount();
        int ei;
        for (ei = 0; ei < count; ei = ei + 1)
        {
            EntityAI cargoItem = cargo.GetItem(ei);
            if (cargoItem)
            {
                total = total + LFPG_CalcFuelRecursive(cargoItem);
            }
        }
        return total;
    }
    #endif

    bool LFPG_AutoConsumeLargestItem()
    {
        #ifdef SERVER
        CargoBase cargo = GetInventory().GetCargo();
        if (!cargo)
            return false;

        int count = cargo.GetItemCount();
        if (count <= 0)
            return false;

        LFPG_ServerSettings st = LFPG_Settings.Get();
        bool whitelistMode = st.FurnaceFuelWhitelistOnly;

        int bestFuel = 0;
        int bestIdx = -1;
        int si;
        for (si = 0; si < count; si = si + 1)
        {
            EntityAI scanItem = cargo.GetItem(si);
            if (!scanItem)
                continue;

            int scanFuel = 0;
            if (whitelistMode)
            {
                // v4.7: Only consider whitelisted items
                scanFuel = LFPG_CalcFuelWhitelist(scanItem);
            }
            else
            {
                scanFuel = LFPG_CalcFuelRecursive(scanItem);
            }

            if (scanFuel > bestFuel)
            {
                bestFuel = scanFuel;
                bestIdx = si;
            }
        }

        if (bestIdx < 0 || bestFuel <= 0)
            return false;

        EntityAI bestItem = cargo.GetItem(bestIdx);
        if (!bestItem)
            return false;

        int fuelAfter = m_FuelCurrent + bestFuel;
        if (fuelAfter > LFPG_FURNACE_MAX_FUEL)
        {
            fuelAfter = LFPG_FURNACE_MAX_FUEL;
        }
        m_FuelCurrent = fuelAfter;

        string burnedType = bestItem.GetType();
        g_Game.ObjectDelete(bestItem);

        SetSynchDirty();

        if (LFPG_LOG_LEVEL >= 2)
        {
            string acLog = "[LFPG_Furnace] Auto-consumed ";
            acLog = acLog + burnedType;
            acLog = acLog + " +";
            acLog = acLog + bestFuel.ToString();
            acLog = acLog + " fuel=";
            acLog = acLog + m_FuelCurrent.ToString();
            acLog = acLog + " id=";
            acLog = acLog + m_DeviceId;
            LFPG_Util.Debug(acLog);
        }

        return true;
        #else
        return false;
        #endif
    }
};
