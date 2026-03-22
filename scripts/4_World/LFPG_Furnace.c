class LF_Furnace_Kit : LFPG_KitBaseDeployable
{
    override string LFPG_GetSpawnClassname()
    {
        return "LF_Furnace";
    }
};

// ---------------------------------------------------------
// DEVICE — SOURCE : LFPG_WireOwnerBase
// 1 OUT (output_1), 50 u/s while burning
// ---------------------------------------------------------
class LF_Furnace : LFPG_WireOwnerBase
{
    // ---- Device-specific SyncVars ----
    protected bool  m_SourceOn     = false;
    protected float m_LoadRatio    = 0.0;
    protected bool  m_Overloaded   = false;
    protected int   m_FuelCurrent  = 0;

    // ---- Burn timing (server-only, not persisted) ----
    // NM ticks every 5s, burn fires every 30s.
    // BurnTick checks: if now < m_BurnNextMs, skip.
    protected int m_BurnNextMs = 0;

    // ============================================
    // Constructor — port + SyncVars
    // ============================================
    void LF_Furnace()
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
        if (m_SourceOn && m_FuelCurrent > 0)
        {
            int now = GetGame().GetTime();
            m_BurnNextMs = now + LFPG_FURNACE_BURN_INTERVAL_MS;
            LFPG_NetworkManager.Get().RegisterFurnace(this);
        }

        // Safety: source on but no fuel → try auto-consume
        if (m_SourceOn && m_FuelCurrent <= 0)
        {
            bool restoreConsumed = LFPG_AutoConsumeLargestItem();
            if (restoreConsumed)
            {
                int now2 = GetGame().GetTime();
                m_BurnNextMs = now2 + LFPG_FURNACE_BURN_INTERVAL_MS;
                LFPG_NetworkManager.Get().RegisterFurnace(this);
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
            LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
        }
        #endif
    }

    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        if (m_SourceOn)
        {
            m_SourceOn = false;
            LFPG_NetworkManager.Get().UnregisterFurnace(this);
            SetSynchDirty();
        }
        #endif
    }

    override void LFPG_OnDeleted()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterFurnace(this);
        #endif
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        if (m_SourceOn)
        {
            m_SourceOn = false;
            LFPG_NetworkManager.Get().UnregisterFurnace(this);
            SetSynchDirty();
        }
        #endif
    }

    // ============================================
    // Persistence: m_SourceOn + m_FuelCurrent
    // (after wireJSON from WireOwnerBase)
    // ============================================
    override void LFPG_OnStoreSaveDevice(ParamsWriteContext ctx)
    {
        ctx.Write(m_SourceOn);
        ctx.Write(m_FuelCurrent);
    }

    override bool LFPG_OnStoreLoadDevice(ParamsReadContext ctx, int deviceVer)
    {
        if (!ctx.Read(m_SourceOn))
        {
            string errSrc = "[LF_Furnace] OnStoreLoad failed: m_SourceOn";
            LFPG_Util.Error(errSrc);
            return false;
        }

        if (!ctx.Read(m_FuelCurrent))
        {
            string errFuel = "[LF_Furnace] OnStoreLoad failed: m_FuelCurrent";
            LFPG_Util.Error(errFuel);
            return false;
        }

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
        int now = GetGame().GetTime();
        if (now < m_BurnNextMs)
            return;

        m_BurnNextMs = now + LFPG_FURNACE_BURN_INTERVAL_MS;

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
                LFPG_NetworkManager.Get().UnregisterFurnace(this);
                SetSynchDirty();
                if (m_DeviceId != "")
                {
                    LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
                }
                string offMsg = "[LF_Furnace] Fuel exhausted + cargo empty, auto-off. id=";
                offMsg = offMsg + m_DeviceId;
                LFPG_Util.Info(offMsg);
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
            m_SourceOn = false;
            LFPG_NetworkManager.Get().UnregisterFurnace(this);
            SetSynchDirty();
            if (m_DeviceId != "")
            {
                LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
            }
            string offMsg = "[LF_Furnace] Toggled OFF. fuel=";
            offMsg = offMsg + m_FuelCurrent.ToString();
            offMsg = offMsg + " id=";
            offMsg = offMsg + m_DeviceId;
            LFPG_Util.Info(offMsg);
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
                    canIgnite = true;
                }
            }

            if (canIgnite)
            {
                m_SourceOn = true;
                int now = GetGame().GetTime();
                m_BurnNextMs = now + LFPG_FURNACE_BURN_INTERVAL_MS;
                LFPG_NetworkManager.Get().RegisterFurnace(this);
                SetSynchDirty();
                if (m_DeviceId != "")
                {
                    LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
                }
                string onMsg = "[LF_Furnace] Toggled ON. fuel=";
                onMsg = onMsg + m_FuelCurrent.ToString();
                onMsg = onMsg + " id=";
                onMsg = onMsg + m_DeviceId;
                LFPG_Util.Info(onMsg);
            }
        }
        #endif
    }

    // ============================================
    // Fuel system
    // ============================================
    int LFPG_CalcFuelRecursive(EntityAI item)
    {
        if (!item)
            return 0;

        string itemType = item.GetType();
        string cfgPath = "CfgVehicles ";
        cfgPath = cfgPath + itemType;
        cfgPath = cfgPath + " itemSize";

        int w = 0;
        int h = 0;
        int qty = 0;
        int fuel = 0;

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

        qty = 1;
        string splitPath = "CfgVehicles ";
        splitPath = splitPath + itemType;
        splitPath = splitPath + " canBeSplit";
        int splitVal = GetGame().ConfigGetInt(splitPath);
        if (splitVal > 0)
        {
            int rawQty = item.GetQuantity();
            if (rawQty > 1)
            {
                qty = rawQty;
            }
        }

        fuel = w * h * qty;

        CargoBase cargo = item.GetInventory().GetCargo();
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

        string fuelMsg = "[LF_Furnace] Fuel added: +";
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

        int count = cargo.GetItemCount();
        if (count > 0)
            return true;
        return false;
    }

    int LFPG_GetCargoItemCount()
    {
        CargoBase cargo = GetInventory().GetCargo();
        if (!cargo)
            return 0;

        return cargo.GetItemCount();
    }

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

    bool LFPG_AutoConsumeLargestItem()
    {
        #ifdef SERVER
        CargoBase cargo = GetInventory().GetCargo();
        if (!cargo)
            return false;

        int count = cargo.GetItemCount();
        if (count <= 0)
            return false;

        int bestFuel = 0;
        int bestIdx = -1;
        int si;
        for (si = 0; si < count; si = si + 1)
        {
            EntityAI scanItem = cargo.GetItem(si);
            if (!scanItem)
                continue;

            int scanFuel = LFPG_CalcFuelRecursive(scanItem);
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
        GetGame().ObjectDelete(bestItem);

        SetSynchDirty();

        string acLog = "[LF_Furnace] Auto-consumed ";
        acLog = acLog + burnedType;
        acLog = acLog + " +";
        acLog = acLog + bestFuel.ToString();
        acLog = acLog + " fuel=";
        acLog = acLog + m_FuelCurrent.ToString();
        acLog = acLog + " id=";
        acLog = acLog + m_DeviceId;
        LFPG_Util.Info(acLog);

        return true;
        #else
        return false;
        #endif
    }
};
