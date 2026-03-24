// =========================================================
// LF_PowerGrid - Battery Adapter (v1.0)
//
// LF_BatteryAdapter_Kit: same-model kit → spawns LF_BatteryAdapter.
//
// LF_BatteryAdapter:  PASSTHROUGH (1 IN + 1 OUT) with energy storage
//                     proxied through vanilla battery CompEM.
//
// Accepts CarBattery (500 CompEM → 1000 LFPG) or
//         TruckBattery (1500 CompEM → 3000 LFPG) as attachment.
// Factor: CompEM × 2.0 = LFPG energy.
//
// When NM TickBatteries writes energy back, it's divided by
// factor and written to the vanilla battery's CompEM.
// If the player removes the battery, it retains its charge
// and can be used in a vehicle normally.
//
// No LEDs, no toggle, no gate. Basic adapter only.
// =========================================================

// ---------------------------------------------------------
// Constants
// ---------------------------------------------------------
static const float LFPG_ADAPTER_FACTOR             = 2.0;
static const float LFPG_ADAPTER_EFFICIENCY          = 0.92;
static const float LFPG_ADAPTER_SELF_DISCHARGE      = 0.0005;

// CarBattery (500 CompEM → 1000 LFPG)
static const float LFPG_ADAPTER_CAR_CHARGE_RATE     = 15.0;
static const float LFPG_ADAPTER_CAR_DISCHARGE_RATE  = 20.0;
static const float LFPG_ADAPTER_CAR_MAX_OUTPUT      = 40.0;

// TruckBattery (1500 CompEM → 3000 LFPG)
static const float LFPG_ADAPTER_TRUCK_CHARGE_RATE   = 25.0;
static const float LFPG_ADAPTER_TRUCK_DISCHARGE_RATE = 35.0;
static const float LFPG_ADAPTER_TRUCK_MAX_OUTPUT    = 60.0;

// ---------------------------------------------------------
// KIT: same-model deploy
// ---------------------------------------------------------
class LF_BatteryAdapter_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LF_BatteryAdapter";
    }
};

// ---------------------------------------------------------
// DEVICE: PASSTHROUGH : LFPG_WireOwnerBase
// ---------------------------------------------------------
class LF_BatteryAdapter : LFPG_WireOwnerBase
{
    // ---- SyncVars ----
    protected bool  m_PoweredNet        = false;
    protected bool  m_Overloaded        = false;
    protected float m_StoredEnergy      = 0.0;
    protected float m_ChargeRateCurrent = 0.0;

    // ---- Internal state (server-only, not synced) ----
    protected EntityAI m_AttachedBattery;
    protected int   m_BatteryType       = 0;  // 0=none, 1=car, 2=truck
    protected bool  m_DischargeEnabled  = true;
    protected float m_LastSyncedStored  = -1.0;

    // ============================================
    // Constructor — ports + SyncVars
    // ============================================
    void LF_BatteryAdapter()
    {
        string pIn = "input_1";
        string lIn = "input_1";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, lIn);

        string pOut = "output_1";
        string lOut = "output_1";
        LFPG_AddPort(pOut, LFPG_PortDir.OUT, lOut);

        string varPowered    = "m_PoweredNet";
        string varOverloaded = "m_Overloaded";
        string varStored     = "m_StoredEnergy";
        string varChargeRate = "m_ChargeRateCurrent";

        RegisterNetSyncVariableBool(varPowered);
        RegisterNetSyncVariableBool(varOverloaded);
        RegisterNetSyncVariableFloat(varStored, 0.0, 10000.0, 12);
        RegisterNetSyncVariableFloat(varChargeRate, -200.0, 200.0, 8);
    }

    // ============================================
    // Port world pos — p3d uses port_input_0 / port_output_0
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
            offset = "0 0.047 -0.1";
        }
        else if (portName == LFPG_PORT_OUTPUT_1)
        {
            offset = "0 0.047 0.1";
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
        if (m_BatteryType == 1)
        {
            return LFPG_ADAPTER_CAR_MAX_OUTPUT;
        }
        if (m_BatteryType == 2)
        {
            return LFPG_ADAPTER_TRUCK_MAX_OUTPUT;
        }
        return 0.0;
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

        // Check if a battery is already attached (e.g., after server restart).
        // Vanilla persistence restores attachments before EEInit.
        LFPG_DetectExistingBattery();
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
    // Attachment hooks — detect vanilla battery
    // ============================================
    override void EEItemAttached(EntityAI item, string slot_name)
    {
        super.EEItemAttached(item, slot_name);

        #ifdef SERVER
        if (!item)
            return;

        int batType = LFPG_ClassifyBattery(item);
        if (batType == 0)
            return;

        m_AttachedBattery = item;
        m_BatteryType = batType;

        string attachMsg = "[LF_BatteryAdapter] Battery attached type=";
        attachMsg = attachMsg + batType.ToString();
        string itemType = item.GetType();
        attachMsg = attachMsg + " class=";
        attachMsg = attachMsg + itemType;
        LFPG_Util.Info(attachMsg);

        // Read initial energy from CompEM
        LFPG_RefreshStoredFromCompEM();

        // Trigger propagation so grid sees new capacity
        string devId = LFPG_GetDeviceId();
        if (devId != "")
        {
            LFPG_NetworkManager.Get().RequestPropagate(devId);
        }
        #endif
    }

    override void EEItemDetached(EntityAI item, string slot_name)
    {
        super.EEItemDetached(item, slot_name);

        #ifdef SERVER
        if (!item)
            return;

        int batType = LFPG_ClassifyBattery(item);
        if (batType == 0)
            return;

        string detachMsg = "[LF_BatteryAdapter] Battery detached type=";
        detachMsg = detachMsg + batType.ToString();
        LFPG_Util.Info(detachMsg);

        m_AttachedBattery = null;
        m_BatteryType = 0;
        m_StoredEnergy = 0.0;
        m_ChargeRateCurrent = 0.0;
        m_LastSyncedStored = -1.0;
        m_DischargeEnabled = true;
        SetSynchDirty();

        // Trigger propagation so grid sees zero capacity
        string devId = LFPG_GetDeviceId();
        if (devId != "")
        {
            LFPG_NetworkManager.Get().RequestPropagate(devId);
        }
        #endif
    }

    override bool CanReceiveAttachment(EntityAI attachment, int slotId)
    {
        if (!attachment)
            return false;

        int batType = LFPG_ClassifyBattery(attachment);
        if (batType == 0)
        {
            return false;
        }

        // Only one battery at a time — check actual slots (works on client + server).
        // m_BatteryType is server-only, can't be used here.
        string slotCar = "CarBattery";
        EntityAI existCar = FindAttachmentBySlotName(slotCar);
        if (existCar)
        {
            return false;
        }

        string slotTruck = "TruckBattery";
        EntityAI existTruck = FindAttachmentBySlotName(slotTruck);
        if (existTruck)
        {
            return false;
        }

        return super.CanReceiveAttachment(attachment, slotId);
    }

    // ============================================
    // Battery classification helper
    // ============================================
    protected int LFPG_ClassifyBattery(EntityAI item)
    {
        if (!item)
            return 0;

        string kindCar = "CarBattery";
        if (item.IsKindOf(kindCar))
        {
            return 1;
        }

        string kindTruck = "TruckBattery";
        if (item.IsKindOf(kindTruck))
        {
            return 2;
        }

        return 0;
    }

    // ============================================
    // Detect battery already attached (after persistence load)
    // ============================================
    protected void LFPG_DetectExistingBattery()
    {
        #ifdef SERVER
        // Try CarBattery slot
        string slotCar = "CarBattery";
        EntityAI carBat = FindAttachmentBySlotName(slotCar);
        if (carBat)
        {
            m_AttachedBattery = carBat;
            m_BatteryType = 1;
            LFPG_RefreshStoredFromCompEM();
            string carMsg = "[LF_BatteryAdapter] Detected existing CarBattery on init";
            LFPG_Util.Info(carMsg);
            return;
        }

        // Try TruckBattery slot
        string slotTruck = "TruckBattery";
        EntityAI truckBat = FindAttachmentBySlotName(slotTruck);
        if (truckBat)
        {
            m_AttachedBattery = truckBat;
            m_BatteryType = 2;
            LFPG_RefreshStoredFromCompEM();
            string truckMsg = "[LF_BatteryAdapter] Detected existing TruckBattery on init";
            LFPG_Util.Info(truckMsg);
            return;
        }
        #endif
    }

    // ============================================
    // CompEM proxy helpers
    // ============================================
    protected void LFPG_RefreshStoredFromCompEM()
    {
        #ifdef SERVER
        if (!m_AttachedBattery)
        {
            m_StoredEnergy = 0.0;
            SetSynchDirty();
            return;
        }

        ComponentEnergyManager em = m_AttachedBattery.GetCompEM();
        if (!em)
        {
            m_StoredEnergy = 0.0;
            SetSynchDirty();
            return;
        }

        float compemEnergy = em.GetEnergy();
        m_StoredEnergy = compemEnergy * LFPG_ADAPTER_FACTOR;
        m_LastSyncedStored = m_StoredEnergy;
        SetSynchDirty();
        #endif
    }

    protected void LFPG_WriteToCompEM(float lfpgEnergy)
    {
        #ifdef SERVER
        if (!m_AttachedBattery)
            return;

        ComponentEnergyManager em = m_AttachedBattery.GetCompEM();
        if (!em)
            return;

        float compemVal = lfpgEnergy / LFPG_ADAPTER_FACTOR;
        em.SetEnergy(compemVal);
        #endif
    }

    // ============================================
    // Battery API — called by NM TickBatteries via dynamic dispatch
    // ============================================
    float LFPG_GetStoredEnergy()
    {
        if (!m_AttachedBattery)
            return 0.0;

        ComponentEnergyManager em = m_AttachedBattery.GetCompEM();
        if (!em)
            return 0.0;

        float compemEnergy = em.GetEnergy();
        float lfpgEnergy = compemEnergy * LFPG_ADAPTER_FACTOR;
        return lfpgEnergy;
    }

    void LFPG_SetStoredEnergy(float val)
    {
        #ifdef SERVER
        // Write to vanilla battery CompEM
        LFPG_WriteToCompEM(val);

        // Update SyncVar for client display
        m_StoredEnergy = val;

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

    float LFPG_GetMaxStoredEnergy()
    {
        if (!m_AttachedBattery)
            return 0.0;

        ComponentEnergyManager em = m_AttachedBattery.GetCompEM();
        if (!em)
            return 0.0;

        float compemMax = em.GetEnergyMax();
        float lfpgMax = compemMax * LFPG_ADAPTER_FACTOR;
        return lfpgMax;
    }

    float LFPG_GetMaxChargeRate()
    {
        if (m_BatteryType == 1)
        {
            return LFPG_ADAPTER_CAR_CHARGE_RATE;
        }
        if (m_BatteryType == 2)
        {
            return LFPG_ADAPTER_TRUCK_CHARGE_RATE;
        }
        return 0.0;
    }

    float LFPG_GetMaxDischargeRate()
    {
        if (m_BatteryType == 1)
        {
            return LFPG_ADAPTER_CAR_DISCHARGE_RATE;
        }
        if (m_BatteryType == 2)
        {
            return LFPG_ADAPTER_TRUCK_DISCHARGE_RATE;
        }
        return 0.0;
    }

    float LFPG_GetEfficiency()
    {
        return LFPG_ADAPTER_EFFICIENCY;
    }

    float LFPG_GetSelfDischargeRate()
    {
        return LFPG_ADAPTER_SELF_DISCHARGE;
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
        return true;
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

    // ============================================
    // Persistence — no extra fields beyond base
    // (vanilla battery persists its own CompEM energy)
    // ============================================
    // Intentionally empty: wireJSON from WireOwnerBase is sufficient.
    // On load, LFPG_DetectExistingBattery reads CompEM from attached battery.
};
