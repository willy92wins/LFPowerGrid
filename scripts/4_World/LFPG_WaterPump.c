// =========================================================
// LF_PowerGrid - Water Pump device (v4.1 Registry Refactor)
//
// LFPG_WaterPump_Kit:  LFPG_KitBaseDeployable (box → hologram).
// LFPG_WaterPump (T1): PASSTHROUGH, 1 IN + 1 OUT, 50 u/s, cap 100 u/s
// LFPG_WaterPump_T2:   PASSTHROUGH, 1 IN + 3 OUT, 50 u/s, cap 100 u/s + 50L tank
// LFPG_WaterPump_T2_Kit: LFPG_KitBaseDeployable, deploys the T2 back.
//   Screwdriver dismantle of a deployed T2 returns this kit, not the T1 one;
//   the guard below refuses while the 50L tank still holds water.
//
// v4.0: Migrated from Inventory_Base to LFPG_WireOwnerBase.
//   Wire store, wire API, persistence wireJSON, CanConnectTo — all in base.
//   WaterPump_T2 is independent (NOT inherited from T1).
// v4.1: RegisterT1Pump/RegisterT2Pump in NM. Eliminates GetAll+Cast.
// =========================================================

// ---------------------------------------------------------
// KIT: LFPG_KitBaseDeployable (box → hologram of pump model)
// v4.2: Migrated from DeployableContainer_Base to fix
//   hologram detection (Cast to LFPG_KitBaseDeployable).
// ---------------------------------------------------------
class LFPG_WaterPump_Kit : LFPG_KitBaseDeployable
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_WaterPump";
    }
};

// ---------------------------------------------------------
// WATER PUMP T1: PASSTHROUGH : LFPG_WireOwnerBase
// 1 IN + 1 OUT, 50 u/s self-consumption, 100 u/s cap
// ---------------------------------------------------------
// Shared pump behavior. Leaf constructors retain their original SyncVar order.
class LFPG_PumpDeviceBase : LFPG_WireOwnerBase
{
    protected bool m_PoweredNet = false;
    protected bool m_Overloaded = false;
    protected float m_FilterLastRealMs = 0.0;
    protected EffectSound m_PumpLoopSound;

    override int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.PASSTHROUGH;
    }

    override float LFPG_GetConsumption()
    {
        return LFPG_PUMP_CONSUMPTION;
    }

    override float LFPG_GetCapacity()
    {
        return LFPG_PUMP_CAPACITY;
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

    bool LFPG_GetPoweredNet()
    {
        return m_PoweredNet;
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

    override void LFPG_OnVarSyncDevice()
    {
        #ifndef SERVER
        if (m_PoweredNet)
        {
            SetObjectMaterial(LFPG_PUMP_LED_SELECTION_IDX, LFPG_PUMP_LED_RVMAT_ON);
        }
        else
        {
            SetObjectMaterial(LFPG_PUMP_LED_SELECTION_IDX, LFPG_PUMP_LED_RVMAT_OFF);
        }

        if (m_PoweredNet && !m_PumpLoopSound)
        {
            m_PumpLoopSound = SEffectManager.PlaySound(LFPG_PUMP_LOOP_SOUNDSET, GetPosition());
            if (m_PumpLoopSound)
            {
                m_PumpLoopSound.SetAutodestroy(false);
            }
        }

        if (!m_PoweredNet && m_PumpLoopSound)
        {
            m_PumpLoopSound.SoundStop();
            m_PumpLoopSound = null;
        }
        #endif
    }

    float LFPG_GetFilterLastMs()
    {
        return m_FilterLastRealMs;
    }

    void LFPG_SetFilterLastMs(float ms)
    {
        m_FilterLastRealMs = ms;
    }

    void LFPG_DegradeFilter()
    {
        #ifdef SERVER
        string slotName = "GasMaskFilter";
        EntityAI filter = FindAttachmentBySlotName(slotName);
        if (!filter)
            return;

        int qty = filter.GetQuantity();
        if (qty <= 0)
            return;

        int newQty = qty - 1;
        if (newQty < 0)
        {
            newQty = 0;
        }

        ItemBase filterItem = ItemBase.Cast(filter);
        if (filterItem)
        {
            filterItem.SetQuantity(newQty);
        }
        #endif
    }

    bool LFPG_HasActiveFilter()
    {
        return LFPG_PumpHelper.HasActiveFilter(this);
    }

    override int GetLiquidSourceType()
    {
        if (!LFPG_PumpHelper.VerifyPowered(this))
            return LIQUID_NONE;
        if (LFPG_HasActiveFilter())
            return LIQUID_CLEANWATER;
        return LIQUID_RIVERWATER;
    }

    override int GetWaterSourceObjectType()
    {
        if (!LFPG_PumpHelper.VerifyPowered(this))
            return EWaterSourceObjectType.NONE;
        return EWaterSourceObjectType.WELL;
    }

    override bool IsWell()
    {
        return LFPG_PumpHelper.VerifyPowered(this);
    }

    override float GetLiquidThroughputCoef()
    {
        return LIQUID_THROUGHPUT_WELL;
    }
}

class LFPG_WaterPump : LFPG_PumpDeviceBase
{
    // F6 B1: idempotent re-registration point for the OnInit sweep
    // (devices restored during super.OnInit() registered against the
    // inert fallback). RegisterX dedups; this replicates only the
    // registration condition, never init side effects.
    override void LFPG_RegisterWithNetworkManager(LFPG_NetworkManager nm)
    {
        if (nm) nm.RegisterT1Pump(this);
    }

    // ---- Device-specific SyncVars ----
    protected bool m_HasSprinklerOutput = false;

    // ---- Server-only (not SyncVars, not persisted) ----

    // ---- Client sound ----

    // ============================================
    // Constructor — ports + SyncVars
    // ============================================
    void LFPG_WaterPump()
    {
        string pIn = "input_1";
        string lIn = "Input";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, lIn);

        string pOut = "output_1";
        string lOut = "Output";
        LFPG_AddPort(pOut, LFPG_PortDir.OUT, lOut);

        string varPowered = "m_PoweredNet";
        string varOverloaded = "m_Overloaded";
        string varSprOut = "m_HasSprinklerOutput";

        RegisterNetSyncVariableBool(varPowered);
        RegisterNetSyncVariableBool(varOverloaded);
        RegisterNetSyncVariableBool(varSprOut);
    }

    void ~LFPG_WaterPump()
    {
        if (m_PumpLoopSound)
        {
            m_PumpLoopSound.SoundStop();
            m_PumpLoopSound = null;
        }
    }

    // ============================================
    // SetActions
    // ============================================
    override void SetActions()
    {
        super.SetActions();
        AddAction(LFPG_ActionDrinkPump);
        AddAction(LFPG_ActionWashHandsPump);
    }

    // ============================================
    // Virtual interface — PASSTHROUGH
    // ============================================

    override void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;
        SetSynchDirty();

        if (LFPG_LOG_LEVEL >= 2)
        {
            string msg = "[LFPG_WaterPump] SetPowered(";
            msg = msg + powered.ToString();
            msg = msg + ") id=";
            msg = msg + m_DeviceId;
            LFPG_Util.Debug(msg);
        }

        string noRemoved = "";
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm) nm.LFPG_RefreshPumpSprinklerLink(m_DeviceId, noRemoved);
        #endif
    }

    // ============================================
    // Lifecycle hooks (T1)
    // ============================================
    override void LFPG_OnInitDevice()
    {
        #ifdef SERVER
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm) nm.RegisterT1Pump(this);
        m_FilterLastRealMs = g_Game.GetTime();
        #endif
    }

    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm) nm.UnregisterT1Pump(this);
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        #endif
    }

    override void LFPG_OnDeleted()
    {
        #ifdef SERVER
        LFPG_NetworkManager nm = LFPG_NetworkManager.GetExisting();
        if (nm) nm.UnregisterT1Pump(this);
        #endif

        if (m_PumpLoopSound)
        {
            m_PumpLoopSound.SoundStop();
            m_PumpLoopSound = null;
        }
    }

    // ============================================
    // VarSync: LED + pump loop sound
    // ============================================


    // No extra persistence (PASSTHROUGH: ids + deviceVer + wireJSON from base)

    // ============================================
    // Filter degradation
    // ============================================

    // ============================================
    // Sprinkler output state
    // ============================================
    bool LFPG_GetHasSprinklerOutput()
    {
        return m_HasSprinklerOutput;
    }

    void LFPG_SetHasSprinklerOutput(bool val)
    {
        #ifdef SERVER
        if (m_HasSprinklerOutput != val)
        {
            m_HasSprinklerOutput = val;
            SetSynchDirty();
        }
        #endif
    }

    // ============================================
    // Vanilla water overrides
    // ============================================

};

// ---------------------------------------------------------
// WATER PUMP T2: PASSTHROUGH : LFPG_WireOwnerBase
// 1 IN + 3 OUT, 50 u/s, cap 100 u/s + 50L tank
// Independent class (NOT inherited from T1)
// ---------------------------------------------------------
class LFPG_WaterPump_T2 : LFPG_PumpDeviceBase
{
    // F6 B1: idempotent re-registration point for the OnInit sweep
    // (devices restored during super.OnInit() registered against the
    // inert fallback). RegisterX dedups; this replicates only the
    // registration condition, never init side effects.
    override void LFPG_RegisterWithNetworkManager(LFPG_NetworkManager nm)
    {
        if (nm) nm.RegisterT2Pump(this);
    }

    // ---- Device-specific SyncVars ----
    protected float m_TankLevel              = 0.0;
    protected int   m_TankLiquidType         = 0;
    protected int   m_ConnectedSprinklerCount = 0;

    // ---- Server-only ----

    // ---- Client sound ----

    // ============================================
    // Constructor — ports + SyncVars
    // ============================================
    void LFPG_WaterPump_T2()
    {
        string pIn = "input_1";
        string lIn = "Input 1";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, lIn);

        string pO1 = "output_1";
        string lO1 = "Output 1";
        LFPG_AddPort(pO1, LFPG_PortDir.OUT, lO1);

        string pO2 = "output_2";
        string lO2 = "Output 2";
        LFPG_AddPort(pO2, LFPG_PortDir.OUT, lO2);

        string pO3 = "output_3";
        string lO3 = "Output 3";
        LFPG_AddPort(pO3, LFPG_PortDir.OUT, lO3);

        string varPowered    = "m_PoweredNet";
        string varOverloaded = "m_Overloaded";
        string varTankLevel  = "m_TankLevel";
        string varTankLiq    = "m_TankLiquidType";
        string varSprCnt     = "m_ConnectedSprinklerCount";

        RegisterNetSyncVariableBool(varPowered);
        RegisterNetSyncVariableBool(varOverloaded);
        RegisterNetSyncVariableFloat(varTankLevel, 0.0, 50.0, 8);
        RegisterNetSyncVariableInt(varTankLiq);
        RegisterNetSyncVariableInt(varSprCnt);
    }

    void ~LFPG_WaterPump_T2()
    {
        if (m_PumpLoopSound)
        {
            m_PumpLoopSound.SoundStop();
            m_PumpLoopSound = null;
        }
    }

    // ============================================
    // SetActions
    // ============================================
    override void SetActions()
    {
        super.SetActions();
        AddAction(LFPG_ActionDrinkPump);
        AddAction(LFPG_ActionWashHandsPump);
        AddAction(LFPG_ActionFillPump);
    }

    // Dismantles into its own kit, not the T1 one: the MetalPlate and
    // Nails spent on the upgrade stay inside the kit and are not refunded.
    override string LFPG_GetKitClassname()
    {
        string kitClass = "LFPG_WaterPump_T2_Kit";
        return kitClass;
    }

    // The kit carries no stored water; empty the 50L tank before dismantling.
    // Same contract as Furnace fuel, Battery charge and ATM stock: the tank
    // lives in a script field, so the ObjectDelete in the dismantle action
    // would destroy it with no refund and no warning.
    override bool LFPG_BlocksDismantle()
    {
        return m_TankLevel > 0.0;
    }

    // ============================================
    // Virtual interface — PASSTHROUGH
    // ============================================

    override void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;
        SetSynchDirty();

        if (LFPG_LOG_LEVEL >= 2)
        {
            string msg = "[LFPG_WaterPump_T2] SetPowered(";
            msg = msg + powered.ToString();
            msg = msg + ") id=";
            msg = msg + m_DeviceId;
            LFPG_Util.Debug(msg);
        }

        string noRemoved = "";
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm) nm.LFPG_RefreshPumpSprinklerLink(m_DeviceId, noRemoved);
        #endif
    }

    // ============================================
    // Lifecycle hooks (T2)
    // ============================================
    override void LFPG_OnInitDevice()
    {
        #ifdef SERVER
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm) nm.RegisterT2Pump(this);
        m_FilterLastRealMs = g_Game.GetTime();
        #endif
    }

    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm) nm.UnregisterT2Pump(this);
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        #endif
    }

    override void LFPG_OnDeleted()
    {
        #ifdef SERVER
        LFPG_NetworkManager nm = LFPG_NetworkManager.GetExisting();
        if (nm) nm.UnregisterT2Pump(this);
        #endif

        if (m_PumpLoopSound)
        {
            m_PumpLoopSound.SoundStop();
            m_PumpLoopSound = null;
        }
    }

    // ============================================
    // VarSync: LED + pump loop sound
    // ============================================


    // ============================================
    // Persistence: TankLevel + TankLiquidType (after wireJSON from base)
    // ============================================
    override void LFPG_OnStoreSaveDevice(ParamsWriteContext ctx)
    {
        ctx.Write(m_TankLevel);
        ctx.Write(m_TankLiquidType);
    }

    override bool LFPG_OnStoreLoadDevice(ParamsReadContext ctx, int deviceVer)
    {
        if (!ctx.Read(m_TankLevel))
        {
            string errTank = "[LFPG_WaterPump_T2] OnStoreLoad failed: m_TankLevel";
            LFPG_Util.Error(errTank);
            return false;
        }

        if (!ctx.Read(m_TankLiquidType))
        {
            string errLiq = "[LFPG_WaterPump_T2] OnStoreLoad failed: m_TankLiquidType";
            LFPG_Util.Error(errLiq);
            return false;
        }

        return true;
    }

    // ============================================
    // Filter degradation
    // ============================================

    // ============================================
    // Tank accessors
    // ============================================
    float LFPG_GetTankLevel()
    {
        return m_TankLevel;
    }

    void LFPG_SetTankLevel(float level)
    {
        #ifdef SERVER
        float clampedLevel = level;
        if (clampedLevel < 0.0)
        {
            clampedLevel = 0.0;
        }
        if (clampedLevel > LFPG_PUMP_TANK_MAX)
        {
            clampedLevel = LFPG_PUMP_TANK_MAX;
        }
        if (m_TankLevel == clampedLevel)
            return;

        m_TankLevel = clampedLevel;
        SetSynchDirty();
        #endif
    }

    int LFPG_GetTankLiquidType()
    {
        return m_TankLiquidType;
    }

    void LFPG_SetTankLiquidType(int liqType)
    {
        #ifdef SERVER
        if (m_TankLiquidType == liqType)
            return;

        m_TankLiquidType = liqType;
        SetSynchDirty();
        #endif
    }

    // ============================================
    // Sprinkler count state
    // ============================================
    int LFPG_GetConnectedSprinklerCount()
    {
        return m_ConnectedSprinklerCount;
    }

    void LFPG_SetConnectedSprinklerCount(int cnt)
    {
        #ifdef SERVER
        if (m_ConnectedSprinklerCount != cnt)
        {
            m_ConnectedSprinklerCount = cnt;
            SetSynchDirty();
        }
        #endif
    }

    // ============================================
    // Vanilla water overrides
    // ============================================

};

// ---------------------------------------------------------
// T2 KIT: LFPG_KitBaseDeployable (box → hologram of T2 model)
// Own kit, so dismantling a T2 never refunds it as a T1.
// No GetDeployOrientationOffset override: the T1 kit declares
// none either, and the T1 → T2 upgrade reuses the T1 orientation
// verbatim (LFPG_ActionUpgradeWaterPump.c), so both pump models
// share one deploy frame.
// ---------------------------------------------------------
class LFPG_WaterPump_T2_Kit : LFPG_KitBaseDeployable
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_WaterPump_T2";
    }
};
