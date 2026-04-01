// =========================================================
// LF_PowerGrid - Electric Stove (v1.0.0)
//
// LFPG_ElectricStove_Kit:  Same-model kit, heavy object (itemBehaviour=2).
// LFPG_ElectricStove:      CONSUMER device, 1 IN (input_0).
//                           4 independent burners with DirectCooking slots.
//                           Consumption: 0 base + 10 u/s per active burner.
//
// Pattern: PortableGasStove (Cooking class, no FireplaceBase).
// Tick: Centralized in NetworkManager (SimpleDevices ~3s).
//
// SyncVars: m_DeviceIdLow/High (from DeviceBase), m_PoweredNet,
//           m_BurnerMask (int, 4 bits = burner on/off state).
//
// Persistence: DeviceIdLow/High + m_BurnerMask (int).
//   m_PoweredNet is derived state (NOT persisted).
//
// Visuals per burner (client-side, LFPG_OnVarSync + LFPG_OnInit):
//   - button_N animation phase: 1.0 if burner ON, 0.0 if OFF
//     (persisted, independent of power)
//   - stove_N material swap: emissive red if burner ON AND powered,
//     default material if OFF or unpowered
//   Re-applied on every VarSync (no diff tracking).
//
// Enforce Script rules: no ternary, no ++/--, no foreach,
//   no inline concat, explicit typing, m_ prefix on members.
// =========================================================

// ---------------------------------------------------------
// KIT: Same-model, heavy object
// ---------------------------------------------------------
class LFPG_ElectricStove_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_ElectricStove";
    }
};

// ---------------------------------------------------------
// DEVICE: CONSUMER, 1 IN, 4 burners
// ---------------------------------------------------------
class LFPG_ElectricStove : LFPG_DeviceBase
{
    // ---- Constants ----
    static const int    STOVE_BURNER_COUNT          = 4;
    static const float  STOVE_CONSUMPTION_PER_BURNER = 10.0;
    static const float  STOVE_COOKING_TARGET_TEMP   = 400.0;
    static const float  STOVE_COOKING_TIME_COEF     = 0.5;
    static const float  STOVE_HEAT_MULTIPLIER       = 2.0;  // x2 faster than gas stove (helps cold maps)

    // ---- Rvmat paths (assigned to local vars before use) ----
    static const string RVMAT_BURNER_ON  = "\LFPowerGrid\data\electric_stove\electric_stove_burner_on.rvmat";
    static const string RVMAT_BURNER_OFF = "\LFPowerGrid\data\electric_stove\electric_stove.rvmat";

    // ---- SyncVars ----
    protected bool m_PoweredNet   = false;
    protected int  m_BurnerMask   = 0;       // bits 0..3 = burner 0..3 on/off

    // ---- Server-only: cooking ----
    protected ref Cooking m_CookingProcess;

    // ---- Server-only: tracked cookware per slot ----
    protected ItemBase m_SlotCookware0;  // DirectCookingA
    protected ItemBase m_SlotCookware1;  // DirectCookingB
    protected ItemBase m_SlotCookware2;  // DirectCookingC
    protected ItemBase m_SlotCookware3;  // DirectCookingD

    // ============================================
    // Constructor
    // ============================================
    void LFPG_ElectricStove()
    {
        // Port: 1 input
        string pIn = "input_0";
        string lIn = "Power Input";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, lIn);

        // SyncVars
        string varPowered = "m_PoweredNet";
        RegisterNetSyncVariableBool(varPowered);
        string varBurner = "m_BurnerMask";
        RegisterNetSyncVariableInt(varBurner, 0, 15);
    }

    // ============================================
    // DeviceAPI overrides
    // ============================================
    override float LFPG_GetConsumption()
    {
        // Dynamic: 10 u/s per active burner
        int activeBurners = LFPG_CountActiveBurners();
        float consumption = activeBurners * STOVE_CONSUMPTION_PER_BURNER;
        return consumption;
    }

    override void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet != powered)
        {
            m_PoweredNet = powered;
            SetSynchDirty();

            // When power is lost, terminate cooking sounds on all active slots
            if (!powered)
            {
                LFPG_TerminateAllCookingSounds();
            }
        }
        #endif
    }

    protected void LFPG_TerminateAllCookingSounds()
    {
        int i = 0;
        while (i < STOVE_BURNER_COUNT)
        {
            ItemBase cw = LFPG_GetSlotCookware(i);
            if (cw)
            {
                LFPG_TerminateCookwareSounds(cw);
            }
            i = i + 1;
        }
    }

    protected void LFPG_TerminateCookwareSounds(ItemBase cookware)
    {
        if (!cookware)
            return;

        // Terminate cooking process sounds
        if (m_CookingProcess)
        {
            m_CookingProcess.TerminateCookingSounds(cookware);
        }

        // Remove audio visuals (steam/boil effects)
        bool isCW = cookware.IsCookware();
        bool isLC = cookware.IsLiquidContainer();
        if (isCW || isLC)
        {
            cookware.RemoveAudioVisualsOnClient();
        }
    }

    override bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    // ============================================
    // Burner state management (server)
    // ============================================
    int LFPG_GetBurnerMask()
    {
        return m_BurnerMask;
    }

    bool LFPG_IsBurnerOn(int index)
    {
        if (index < 0)
            return false;
        if (index >= STOVE_BURNER_COUNT)
            return false;

        int bit = 1 << index;
        int masked = m_BurnerMask & bit;
        if (masked != 0)
            return true;

        return false;
    }

    void LFPG_ToggleBurner(int index)
    {
        #ifdef SERVER
        if (index < 0)
            return;
        if (index >= STOVE_BURNER_COUNT)
            return;

        int bit = 1 << index;
        int masked = m_BurnerMask & bit;
        if (masked != 0)
        {
            // Turn OFF
            int invBit = ~bit;
            m_BurnerMask = m_BurnerMask & invBit;

            // Terminate cooking sounds on this slot's cookware
            ItemBase cw = LFPG_GetSlotCookware(index);
            if (cw)
            {
                LFPG_TerminateCookwareSounds(cw);
            }
        }
        else
        {
            // Turn ON
            m_BurnerMask = m_BurnerMask | bit;
        }

        SetSynchDirty();

        // Re-propagate so graph sees new consumption
        string devId = LFPG_GetDeviceId();
        if (devId != "")
        {
            LFPG_NetworkManager.Get().RequestPropagate(devId);
        }

        string msg = "[LFPG_ElectricStove] ToggleBurner idx=";
        msg = msg + index.ToString();
        msg = msg + " mask=";
        msg = msg + m_BurnerMask.ToString();
        LFPG_Util.Info(msg);
        #endif
    }

    int LFPG_CountActiveBurners()
    {
        int count = 0;
        int i = 0;
        while (i < STOVE_BURNER_COUNT)
        {
            int bit = 1 << i;
            int masked = m_BurnerMask & bit;
            if (masked != 0)
            {
                count = count + 1;
            }
            i = i + 1;
        }
        return count;
    }

    // ============================================
    // Cooking tick (called by NetworkManager)
    // ============================================
    void LFPG_TickCooking(float deltaTime)
    {
        #ifdef SERVER
        if (!m_PoweredNet)
            return;

        if (m_BurnerMask == 0)
            return;

        if (!m_CookingProcess)
            return;

        m_CookingProcess.SetCookingUpdateTime(deltaTime);

        int i = 0;
        while (i < STOVE_BURNER_COUNT)
        {
            int bit = 1 << i;
            int masked = m_BurnerMask & bit;
            if (masked != 0)
            {
                ItemBase cookware = LFPG_GetSlotCookware(i);
                if (cookware)
                {
                    // Heat the cookware
                    LFPG_HeatCookware(cookware, deltaTime);

                    // Cook contents
                    m_CookingProcess.CookWithEquipment(cookware, STOVE_COOKING_TIME_COEF);

                    // Damage cookware
                    float dmg = GameConstants.FIRE_ATTACHMENT_DAMAGE_PER_SECOND * deltaTime;
                    cookware.DecreaseHealth(dmg, false);
                }
            }
            i = i + 1;
        }
        #endif
    }

    protected void LFPG_HeatCookware(ItemBase cookware, float deltaTime)
    {
        #ifdef SERVER
        if (!cookware)
            return;

        if (!cookware.CanHaveTemperature())
            return;

        float currentTemp = cookware.GetTemperature();
        float targetTemp = STOVE_COOKING_TARGET_TEMP;
        float diff = targetTemp - currentTemp;

        if (diff > 0)
        {
            float heatPermCoef = cookware.GetHeatPermeabilityCoef();
            float heatCoef = GameConstants.TEMP_COEF_GAS_STOVE * STOVE_HEAT_MULTIPLIER;
            cookware.SetTemperatureEx(new TemperatureDataInterpolated(targetTemp, ETemperatureAccessTypes.ACCESS_FIREPLACE, deltaTime, heatCoef, heatPermCoef));
        }
        #endif
    }

    // ============================================
    // Cookware slot tracking
    // ============================================
    ItemBase LFPG_GetSlotCookware(int index)
    {
        if (index == 0)
            return m_SlotCookware0;
        if (index == 1)
            return m_SlotCookware1;
        if (index == 2)
            return m_SlotCookware2;
        if (index == 3)
            return m_SlotCookware3;
        return null;
    }

    protected void LFPG_SetSlotCookware(int index, ItemBase item)
    {
        if (index == 0)
        {
            m_SlotCookware0 = item;
        }
        else if (index == 1)
        {
            m_SlotCookware1 = item;
        }
        else if (index == 2)
        {
            m_SlotCookware2 = item;
        }
        else if (index == 3)
        {
            m_SlotCookware3 = item;
        }
    }

    protected int LFPG_SlotNameToIndex(string slotName)
    {
        // v1.0.1: Case-insensitive comparison. Some DayZ versions
        // normalize slot names to lowercase in EEItemAttached/Detached.
        // Same pattern as v0.7.28 sparkplug fix in LF_TestGenerator.
        string slotLower = slotName;
        slotLower.ToLower();
        if (slotLower == "directcookinga")
            return 0;
        if (slotLower == "directcookingb")
            return 1;
        if (slotLower == "directcookingc")
            return 2;
        if (slotLower == "directcookingd")
            return 3;
        return -1;
    }

    // ============================================
    // Attachment hooks
    // ============================================
    override void EEItemAttached(EntityAI item, string slot_name)
    {
        super.EEItemAttached(item, slot_name);

        int slotIdx = LFPG_SlotNameToIndex(slot_name);
        if (slotIdx >= 0)
        {
            ItemBase ib = ItemBase.Cast(item);
            LFPG_SetSlotCookware(slotIdx, ib);

            // Reset cooking time on the cookware (anti-exploit, same as vanilla)
            #ifdef SERVER
            if (ib)
            {
                Edible_Base edBase = Edible_Base.Cast(ib);
                if (edBase)
                {
                    if (edBase.GetFoodStage())
                    {
                        edBase.SetCookingTime(0);
                    }
                }
            }
            #endif
        }
    }

    override void EEItemDetached(EntityAI item, string slot_name)
    {
        super.EEItemDetached(item, slot_name);

        int slotIdx = LFPG_SlotNameToIndex(slot_name);
        if (slotIdx >= 0)
        {
            // Terminate cooking sounds on detached cookware
            ItemBase ib = ItemBase.Cast(item);
            if (ib)
            {
                LFPG_TerminateCookwareSounds(ib);
            }

            LFPG_SetSlotCookware(slotIdx, null);
        }
    }

    override bool CanReceiveAttachment(EntityAI attachment, int slotId)
    {
        if (!attachment)
            return false;

        // Only accept cookware in DirectCooking slots
        string slotName = InventorySlots.GetSlotName(slotId);
        int slotIdx = LFPG_SlotNameToIndex(slotName);
        if (slotIdx >= 0)
        {
            ItemBase ib = ItemBase.Cast(attachment);
            if (!ib)
                return false;
            if (!ib.IsCookware())
                return false;
        }

        return super.CanReceiveAttachment(attachment, slotId);
    }

    // ============================================
    // GetCookingTargetTemperature (vanilla override)
    // Used by Cooking class to determine heat target
    // ============================================
    override bool GetCookingTargetTemperature(out float temperature)
    {
        temperature = STOVE_COOKING_TARGET_TEMP;
        return true;
    }

    // ============================================
    // Visuals (client-side)
    // ============================================
    override void LFPG_OnVarSync()
    {
        LFPG_UpdateVisuals();
    }

    protected void LFPG_UpdateVisuals()
    {
        // hiddenSelections in config.cpp order:
        // [0] = stove_1, [1] = stove_2, [2] = stove_3, [3] = stove_4
        string rvmatOn = RVMAT_BURNER_ON;
        string rvmatOff = RVMAT_BURNER_OFF;

        int i = 0;
        while (i < STOVE_BURNER_COUNT)
        {
            int bit = 1 << i;
            int masked = m_BurnerMask & bit;
            bool burnerOn = false;
            if (masked != 0)
            {
                burnerOn = true;
            }

            // Button animation: reflects burner state (persisted, power-independent)
            string btnName = LFPG_GetButtonName(i);
            float btnPhase = 0.0;
            if (burnerOn)
            {
                btnPhase = 1.0;
            }
            SetAnimationPhase(btnName, btnPhase);

            // Stove surface: emissive red ONLY if burner ON AND powered
            bool showHeat = false;
            if (burnerOn && m_PoweredNet)
            {
                showHeat = true;
            }

            if (showHeat)
            {
                SetObjectMaterial(i, rvmatOn);
            }
            else
            {
                SetObjectMaterial(i, rvmatOff);
            }

            i = i + 1;
        }
    }

    protected string LFPG_GetButtonName(int index)
    {
        if (index == 0)
            return "button_1";
        if (index == 1)
            return "button_2";
        if (index == 2)
            return "button_3";
        if (index == 3)
            return "button_4";
        return "";
    }

    // ============================================
    // Lifecycle hooks
    // ============================================
    override void LFPG_OnInit()
    {
        LFPG_UpdateVisuals();
    }

    override void EEInit()
    {
        super.EEInit();

        // Pre-create cooking process (avoid alloc in tick)
        if (!m_CookingProcess)
        {
            m_CookingProcess = new Cooking();
        }

        #ifdef SERVER
        LFPG_NetworkManager.Get().RegisterStove(this);
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
        // Terminate all cooking sounds
        LFPG_TerminateAllCookingSounds();
        LFPG_NetworkManager.Get().UnregisterStove(this);
        #endif
    }

    override void LFPG_OnDeleted()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterStove(this);
        #endif
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();

            // Same as power loss: terminate cooking sounds
            LFPG_TerminateAllCookingSounds();
        }
        #endif
    }

    // ============================================
    // Persistence (hooks called by DeviceBase)
    // ============================================
    override void LFPG_OnStoreSaveExtra(ParamsWriteContext ctx)
    {
        ctx.Write(m_BurnerMask);
    }

    override bool LFPG_OnStoreLoadExtra(ParamsReadContext ctx, int ver)
    {
        int loadedMask = 0;
        if (!ctx.Read(loadedMask))
            return false;
        m_BurnerMask = loadedMask;

        return true;
    }

    // ============================================
    // Actions
    // ============================================
    override void SetActions()
    {
        super.SetActions();

        AddAction(LFPG_ActionToggleBurner0);
        AddAction(LFPG_ActionToggleBurner1);
        AddAction(LFPG_ActionToggleBurner2);
        AddAction(LFPG_ActionToggleBurner3);
    }

    // NOTE: CanPutInCargo, CanPutIntoHands, IsHeavyBehaviour
    // already handled by LFPG_DeviceBase (all return false).
    // The KIT gets heavy behavior from itemBehaviour=2 in config.

    // Prevent CE temperature system from messing with our device
    override bool IsSelfAdjustingTemperature()
    {
        return m_PoweredNet;
    }
};
