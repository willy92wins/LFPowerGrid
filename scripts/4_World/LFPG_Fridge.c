// =========================================================
// LF_PowerGrid - Fridge device (v4.0 Refactor)
//
// LFPG_Fridge_Kit:  DeployableContainer_Base (box model + hologram).
// LFPG_Fridge:      CONSUMER, 1 IN (input_1), 20 u/s, no wire store.
//                 Cooling tick, door toggle, cargo filter.
//
// v4.0: Migrated from Inventory_Base to LFPG_DeviceBase.
//   Persists m_IsOpen via LFPG_OnStoreSaveExtra hook.
// =========================================================

static const string LFPG_FRIDGE_RVMAT_ON   = "\\LFPowerGrid\\data\\fridge\\fridge_green.rvmat";
static const string LFPG_FRIDGE_RVMAT_OFF  = "\\LFPowerGrid\\data\\fridge\\led_off.rvmat";
static const float  LFPG_FRIDGE_TEMP       = 5.0;
static const float  LFPG_FRIDGE_CONSUMPTION = 20.0;

class LFPG_Fridge_Kit : LFPG_KitBaseDeployable
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_Fridge";
    }
};

// ---------------------------------------------------------
// DEVICE - CONSUMER : LFPG_DeviceBase
// ---------------------------------------------------------
class LFPG_Fridge : LFPG_DeviceBase
{
    // ---- Device-specific SyncVars ----
    protected bool m_PoweredNet = false;
    protected bool m_IsOpen     = false;

    // ---- Client-local ----
    protected int m_PrevLedState = -1;

    void LFPG_Fridge()
    {
        string pIn = "input_1";
        string lIn = "Power Input";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, lIn);

        string varPowered = "m_PoweredNet";
        RegisterNetSyncVariableBool(varPowered);
        string varOpen = "m_IsOpen";
        RegisterNetSyncVariableBool(varOpen);
    }

    // ---- Actions ----
    override void SetActions()
    {
        super.SetActions();
        AddAction(LFPG_ActionToggleFridgeDoor);
    }

    // ---- Virtual interface ----
    override int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.CONSUMER;
    }

    override float LFPG_GetConsumption()
    {
        return LFPG_FRIDGE_CONSUMPTION;
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

        string msg = "[LFPG_Fridge] SetPowered(";
        msg = msg + powered.ToString();
        msg = msg + ") id=";
        msg = msg + m_DeviceId;
        LFPG_Util.Debug(msg);
        #endif
    }

    // ---- Lifecycle hooks ----
    override void LFPG_OnInit()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().RegisterFridge(this);
        #endif
    }

    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterFridge(this);
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
        LFPG_NetworkManager.Get().UnregisterFridge(this);
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

    // ---- VarSync: door + LED visuals ----
    override void LFPG_OnVarSync()
    {
        #ifndef SERVER
        LFPG_UpdateVisuals();
        #endif
    }

    protected void LFPG_UpdateVisuals()
    {
        float doorPhase = 0.0;
        if (m_IsOpen)
        {
            doorPhase = 1.0;
        }
        string doorSel = "door";
        SetAnimationPhase(doorSel, doorPhase);

        int ledTarget = 0;
        if (m_PoweredNet)
        {
            ledTarget = 1;
        }

        if (ledTarget != m_PrevLedState)
        {
            m_PrevLedState = ledTarget;
            if (ledTarget == 1)
            {
                SetObjectMaterial(0, LFPG_FRIDGE_RVMAT_ON);
            }
            else
            {
                SetObjectMaterial(0, LFPG_FRIDGE_RVMAT_OFF);
            }
        }
    }

    // ---- Extra persistence: m_IsOpen ----
    override void LFPG_OnStoreSaveExtra(ParamsWriteContext ctx)
    {
        ctx.Write(m_IsOpen);
    }

    override bool LFPG_OnStoreLoadExtra(ParamsReadContext ctx, int ver)
    {
        if (!ctx.Read(m_IsOpen))
        {
            string errOpen = "[LFPG_Fridge] OnStoreLoad: failed to read m_IsOpen";
            LFPG_Util.Error(errOpen);
            return false;
        }
        return true;
    }

    // ---- Door toggle (action callback) ----
    void LFPG_ToggleDoor()
    {
        #ifdef SERVER
        if (m_IsOpen)
        {
            m_IsOpen = false;
        }
        else
        {
            m_IsOpen = true;
        }
        SetSynchDirty();

        string toggleMsg = "[LFPG_Fridge] ToggleDoor: open=";
        toggleMsg = toggleMsg + m_IsOpen.ToString();
        toggleMsg = toggleMsg + " id=";
        toggleMsg = toggleMsg + m_DeviceId;
        LFPG_Util.Debug(toggleMsg);
        #endif
    }

    bool LFPG_IsOpen()
    {
        return m_IsOpen;
    }

    // ---- Cargo: visibility + food-only filter ----
    override bool CanDisplayCargo()
    {
        return m_IsOpen;
    }

    override bool CanReceiveItemIntoCargo(EntityAI item)
    {
        if (!item)
            return false;

        if (!m_IsOpen)
            return false;

        string kEdible = "Edible_Base";
        string kSoda = "SodaCan_ColorBase";
        string kBottle = "Bottle_Base";

        if (item.IsKindOf(kEdible))
            return true;

        if (item.IsKindOf(kSoda))
            return true;

        if (item.IsKindOf(kBottle))
            return true;

        return false;
    }

    // ---- Cooling tick (NM centralized timer) ----
    void LFPG_OnCoolTick()
    {
        #ifdef SERVER
        if (!m_PoweredNet)
            return;

        if (m_IsOpen)
            return;

        CargoBase cargo = GetInventory().GetCargo();
        if (!cargo)
            return;

        int itemCount = cargo.GetItemCount();
        if (itemCount <= 0)
            return;

        int cooled = 0;
        float targetTemp = LFPG_FRIDGE_TEMP;
        float tolerance = 0.5;
        int i = 0;
        float curTemp = 0.0;
        float diffHigh = 0.0;
        float diffLow = 0.0;
        bool outsideBand = false;

        for (i = 0; i < itemCount; i = i + 1)
        {
            ItemBase it = ItemBase.Cast(cargo.GetItem(i));
            if (!it)
                continue;

            if (it.IsDamageDestroyed())
                continue;

            if (!it.CanHaveTemperature())
                continue;

            curTemp = it.GetTemperature();
            diffHigh = curTemp - targetTemp;
            diffLow = targetTemp - curTemp;

            outsideBand = false;
            if (diffHigh > tolerance)
            {
                outsideBand = true;
            }
            if (diffLow > tolerance)
            {
                outsideBand = true;
            }

            if (outsideBand)
            {
                it.SetTemperature(targetTemp);
                cooled = cooled + 1;
            }
        }

        if (cooled > 0)
        {
            string coolMsg = "[LFPG_Fridge] CoolTick: cooled=";
            coolMsg = coolMsg + cooled.ToString();
            coolMsg = coolMsg + " id=";
            coolMsg = coolMsg + m_DeviceId;
            LFPG_Util.Debug(coolMsg);
        }
        #endif
    }

    // ---- Temperature system overrides ----
    override bool CanHaveTemperature()
    {
        return true;
    }

    override bool IsSelfAdjustingTemperature()
    {
        if (!m_PoweredNet)
            return false;

        if (m_IsOpen)
            return false;

        return true;
    }
};
