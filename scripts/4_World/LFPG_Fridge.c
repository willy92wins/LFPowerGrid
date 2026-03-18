// =========================================================
// LF_PowerGrid - Fridge (v1.0.0)
//
// LF_Fridge_Kit:  DeployableContainer_Base pattern (box in hand,
//                 hologram shows fridge model). Requires entry in
//                 LFPG_HologramMod.c for model swap.
//
// LF_Fridge:      CONSUMER, 1 IN (input_1), 20 u/s.
//                 500-slot cargo (10x50 in config), food/drink only.
//                 Cooling: 5C target, every 10s, only when
//                 m_PoweredNet && !m_IsOpen.
//                 Door animation via model.cfg "door" selection.
//                 LED: green = powered, off = unpowered.
//
// Persistence: DeviceIdLow, DeviceIdHigh, m_IsOpen.
//   m_PoweredNet NOT persisted (derived by graph propagation).
//   Save wipe required (new device).
//
// ENFORCE SCRIPT NOTES:
//   No ternary, No ++/--, No foreach, No +=/-=
//   Explicit typing, m_ prefix, variables hoisted before conditionals
//   No string literals as direct function params
//   No multiline expressions
// =========================================================

static const string LFPG_FRIDGE_RVMAT_ON   = "\\LFPowerGrid\\data\\fridge\\fridge_green.rvmat";
static const string LFPG_FRIDGE_RVMAT_OFF  = "\\LFPowerGrid\\data\\fridge\\led_off.rvmat";
static const float  LFPG_FRIDGE_TEMP       = 5.0;
static const float  LFPG_FRIDGE_CONSUMPTION = 20.0;

// ---------------------------------------------------------
// KIT: DeployableContainer_Base pattern (different-model hologram)
// Box model in hand -> hologram shows fridge model.
// Config: Inventory_Base (script overrides to DeployableContainer_Base).
// Hologram: requires LFPG_HologramMod.c entries.
// ---------------------------------------------------------
class LF_Fridge_Kit : DeployableContainer_Base
{
    string GetDeployedClassname()
    {
        return "LF_Fridge";
    }

    vector GetDeployPositionOffset()
    {
        return "0 0 0";
    }

    vector GetDeployOrientationOffset()
    {
        return "0 0 0";
    }

    override bool IsBasebuildingKit()
    {
        return true;
    }

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

    // Prevent orphan loop sound — ObjectDelete during
    // OnPlacementComplete interrupts callback cleanup.
    override string GetLoopDeploySoundset()
    {
        return "";
    }

    override void SetActions()
    {
        super.SetActions();
        AddAction(ActionTogglePlaceObject);
        AddAction(ActionPlaceObject);
    }

    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[Fridge_Kit] OnPlacementComplete: param=";
        tLog = tLog + position.ToString();
        tLog = tLog + " kitPos=";
        tLog = tLog + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        string spawnType = "LF_Fridge";
        EntityAI device = GetGame().CreateObjectEx(spawnType, finalPos, ECE_CREATEPHYSICS);
        if (device)
        {
            device.SetPosition(finalPos);
            device.SetOrientation(finalOri);
            device.Update();

            string deployMsg = "[Fridge_Kit] Deployed LF_Fridge at ";
            deployMsg = deployMsg + finalPos.ToString();
            deployMsg = deployMsg + " ori=";
            deployMsg = deployMsg + finalOri.ToString();
            LFPG_Util.Info(deployMsg);

            // Only delete kit on successful spawn.
            GetGame().ObjectDelete(this);
        }
        else
        {
            string errMsg = "[Fridge_Kit] Failed to create LF_Fridge! Kit preserved.";
            LFPG_Util.Error(errMsg);
            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                string failMsg = "[LFPG] Fridge placement failed. Kit preserved.";
                pb.MessageStatus(failMsg);
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// DEVICE: CONSUMER, 1 IN (input_1), 20 u/s
// 500-slot cargo, door animation, cooling timer.
// ---------------------------------------------------------
class LF_Fridge : Inventory_Base
{
    // ---- SyncVars ----
    protected int  m_DeviceIdLow  = 0;
    protected int  m_DeviceIdHigh = 0;
    protected bool m_PoweredNet   = false;
    protected bool m_IsOpen       = false;

    // ---- Local state ----
    protected string m_DeviceId      = "";
    protected bool   m_LFPG_Deleting = false;

    // ---- Previous visual state (avoid redundant SetObjectMaterial) ----
    protected int m_PrevLedState = -1;  // -1 = unset, 0 = off, 1 = green

    // ============================================
    // Constructor — SyncVar registration
    // MUST be constructor, NOT EEInit.
    // ============================================
    void LF_Fridge()
    {
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
        RegisterNetSyncVariableBool("m_IsOpen");
    }

    // ============================================
    // Device ID helpers
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
    // Lifecycle: EEInit
    // ============================================
    override void EEInit()
    {
        super.EEInit();

        #ifdef SERVER
        if (m_DeviceIdLow == 0 && m_DeviceIdHigh == 0)
        {
            LFPG_Util.GenerateDeviceId(m_DeviceIdLow, m_DeviceIdHigh);
        }
        SetSynchDirty();
        #endif

        LFPG_UpdateDeviceIdString();
        LFPG_TryRegister();

        // CONSUMER: no BroadcastOwnerWires (no OUT port, no wire store)

        #ifdef SERVER
        LFPG_NetworkManager.Get().RegisterFridge(this);
        #endif

        // Apply initial visual state
        LFPG_UpdateVisuals();
    }

    // ============================================
    // Lifecycle: EEKilled
    // ============================================
    override void EEKilled(Object killer)
    {
        LFPG_DeviceLifecycle.OnDeviceKilled(this, m_DeviceId);

        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterFridge(this);

        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        #endif

        super.EEKilled(killer);
    }

    // ============================================
    // Lifecycle: EEDelete (with anti-ghost guard)
    // ============================================
    override void EEDelete(EntityAI parent)
    {
        m_LFPG_Deleting = true;

        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterFridge(this);
        #endif

        LFPG_DeviceLifecycle.OnDeviceDeleted(this, m_DeviceId);
        super.EEDelete(parent);
    }

    // ============================================
    // Lifecycle: EEItemLocationChanged
    // ============================================
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

        LFPG_UpdateVisuals();

        #ifndef SERVER
        if (m_DeviceId != "")
        {
            LFPG_CableRenderer r = LFPG_CableRenderer.Get();
            if (r)
            {
                r.RequestDeviceSync(m_DeviceId, this);
            }
        }
        #endif
    }

    // ============================================
    // Visual state: LED + door animation
    // ============================================
    protected void LFPG_UpdateVisuals()
    {
        // Door animation
        float doorPhase = 0.0;
        if (m_IsOpen)
        {
            doorPhase = 1.0;
        }
        string doorSel = "door";
        SetAnimationPhase(doorSel, doorPhase);

        // LED material (hiddenSelections index 0 = "light_led")
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

    // ============================================
    // Door open/close toggle (called from action)
    // ============================================
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

        string toggleMsg = "[LF_Fridge] ToggleDoor: open=";
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

    // ============================================
    // Cargo: visibility + food-only filter
    // ============================================
    override bool CanDisplayCargo()
    {
        return m_IsOpen;
    }

    override bool CanReceiveItemIntoCargo(EntityAI item)
    {
        if (!item)
            return false;

        // Must be open to receive items
        if (!m_IsOpen)
            return false;

        // Food and drink only
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

    // ============================================
    // Cooling tick (called by NetworkManager centralized timer)
    // ============================================
    void LFPG_OnCoolTick()
    {
        #ifdef SERVER
        // Only cool when powered AND door closed
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

            // Only adjust if temperature is outside tolerance band
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
            string coolMsg = "[LF_Fridge] CoolTick: cooled=";
            coolMsg = coolMsg + cooled.ToString();
            coolMsg = coolMsg + " id=";
            coolMsg = coolMsg + m_DeviceId;
            LFPG_Util.Debug(coolMsg);
        }
        #endif
    }

    // ============================================
    // Inventory guards (prevent pickup — breaks wires)
    // ============================================
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

    // Block vanilla CompEM entirely — power via LFPG graph only.
    override bool IsElectricAppliance()
    {
        return false;
    }

    // Fridge participates in vanilla temperature system
    override bool CanHaveTemperature()
    {
        return true;
    }

    // When cooling is active, tell vanilla env-temp system to NOT
    // adjust items inside this container. Without this, vanilla
    // would warm items toward ambient temp between our cooling ticks,
    // causing temperature oscillation.
    override bool IsSelfAdjustingTemperature()
    {
        if (!m_PoweredNet)
            return false;

        if (m_IsOpen)
            return false;

        return true;
    }

    // ============================================
    // Actions
    // ============================================
    override void SetActions()
    {
        super.SetActions();
        RemoveAction(ActionTakeItem);
        RemoveAction(ActionTakeItemToHands);
        AddAction(LFPG_ActionToggleFridgeDoor);
    }

    // ============================================
    // LFPG_IDevice interface — CONSUMER
    // ============================================
    string LFPG_GetDeviceId()
    {
        return m_DeviceId;
    }

    int LFPG_GetDeviceIdLow()
    {
        return m_DeviceIdLow;
    }

    int LFPG_GetDeviceIdHigh()
    {
        return m_DeviceIdHigh;
    }

    // ---- Port definition: 1x IN ----
    int LFPG_GetPortCount()
    {
        return 1;
    }

    string LFPG_GetPortName(int idx)
    {
        if (idx == 0)
            return "input_1";
        return "";
    }

    int LFPG_GetPortDir(int idx)
    {
        if (idx == 0)
            return LFPG_PortDir.IN;
        return -1;
    }

    string LFPG_GetPortLabel(int idx)
    {
        if (idx == 0)
            return "IN";
        return "";
    }

    bool LFPG_HasPort(string portName, int dir)
    {
        string inName = "input_1";
        if (portName == inName && dir == LFPG_PortDir.IN)
            return true;
        return false;
    }

    vector LFPG_GetPortWorldPos(string portName)
    {
        string memPoint = "port_input_0";

        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

        // Fallback: slightly below device position
        string warnMsg = "[LF_Fridge] Missing memory point for port: ";
        warnMsg = warnMsg + portName;
        LFPG_Util.Warn(warnMsg);
        vector p = GetPosition();
        p[1] = p[1] - 0.1;
        return p;
    }

    // ---- Device type ----
    int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.CONSUMER;
    }

    bool LFPG_IsSource()
    {
        return false;
    }

    bool LFPG_GetSourceOn()
    {
        return false;
    }

    float LFPG_GetConsumption()
    {
        return LFPG_FRIDGE_CONSUMPTION;
    }

    bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;
        SetSynchDirty();

        string msg = "[LF_Fridge] SetPowered(";
        msg = msg + powered.ToString();
        msg = msg + ") id=";
        msg = msg + m_DeviceId;
        LFPG_Util.Debug(msg);
        #endif
    }

    // CONSUMER — no output port, cannot initiate connections.
    bool LFPG_CanConnectTo(Object other, string myPort, string otherPort)
    {
        return false;
    }

    // No wire store (IN-only).
    bool LFPG_HasWireStore()
    {
        return false;
    }

    // ============================================
    // Persistence — CONSUMER: ids + open state.
    // m_PoweredNet NOT persisted (derived by graph).
    // ============================================
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);
        ctx.Write(m_DeviceIdLow);
        ctx.Write(m_DeviceIdHigh);
        ctx.Write(m_IsOpen);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version))
            return false;

        if (!ctx.Read(m_DeviceIdLow))
        {
            string errLow = "[LF_Fridge] OnStoreLoad: failed to read m_DeviceIdLow";
            LFPG_Util.Error(errLow);
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            string errHigh = "[LF_Fridge] OnStoreLoad: failed to read m_DeviceIdHigh";
            LFPG_Util.Error(errHigh);
            return false;
        }

        if (!ctx.Read(m_IsOpen))
        {
            string errOpen = "[LF_Fridge] OnStoreLoad: failed to read m_IsOpen";
            LFPG_Util.Error(errOpen);
            return false;
        }

        return true;
    }
};
