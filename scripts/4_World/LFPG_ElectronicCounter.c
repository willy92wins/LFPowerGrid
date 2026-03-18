// =========================================================
// LF_PowerGrid - Electronic Counter (v2.0.0)
//
// LFPG_ElectronicCounter_Kit: Holdable, deployable (same-model pattern).
// LFPG_ElectronicCounter:     PASSTHROUGH, 2 IN + 1 OUT.
//
// Ports:
//   input_0  — Main power. Counter ON when this port has power.
//   input_1  — Toggle signal. Rising edge increments the digit.
//   output_0 — Momentary pulse output (like PushButton) on 9→wrap.
//
// Capacity: 20 u/s.   Self-consumption: 5 u/s.
//
// Model: electronic_counter.p3d
//   Memory points: port_input_0, port_output_0
//   Skeleton bones: seg_1..seg_7 (7-segment display), light_led
//   Animations: show_0 .. show_9 (user source, memory=1)
//     IMPORTANT: disable current animation BEFORE enabling next.
//   Sections: camo (idx 0), camo2 (idx 1), light_led (idx 2)
//
// Behavior:
//   1. Power ON (input_0 powered) → counter starts at 0, show_0.
//   2. Toggle rising edge (input_1 off→on while powered) → increment.
//      Only increments ONCE per off→on transition. Must lose toggle
//      power and regain it to increment again (edge detection).
//   3. At show_9, toggle rising edge → disable show_9, pulse output
//      for LFPG_COUNTER_PULSE_MS, then reset counter to 0 / show_0.
//   4. Power OFF (input_0 loses power) → all animations off, LED off.
//      Debounce: if power returns within LFPG_COUNTER_DEBOUNCE_MS (200ms),
//      counter value is preserved (prevents false resets from graph
//      re-propagation cycles).
//
// LED states (hiddenSelections[2] = "light_led"):
//   Red  = powered (counter active, showing a digit)
//   Off  = not powered
//
// Persistence: DeviceIdLow, DeviceIdHigh + wires JSON + m_CounterValue.
//   m_CounterValue persisted (v2.0.1) — survives server restart.
//   m_PoweredNet NOT persisted (derived by graph propagation).
//   m_PulseActive NOT persisted (momentary).
// =========================================================

// ---- LED rvmat paths (reuse shared materials) ----
static const string LFPG_COUNTER_RVMAT_OFF = "\\LFPowerGrid\\data\\button\\materials\\led_off.rvmat";
static const string LFPG_COUNTER_RVMAT_RED = "\\LFPowerGrid\\data\\electronic_counter\\electronic_counter_red.rvmat";
static const string LFPG_COUNTER_RVMAT_BASE = "\\LFPowerGrid\\data\\electronic_counter\\electronic_counter.rvmat";

// Duration in milliseconds that the output pulse stays active after 9→wrap.
static const int LFPG_COUNTER_PULSE_MS = 2000;

// Minimum power-off duration (ms) before a power-ON resets the counter.
// Prevents false resets from graph re-propagation cycles within the same frame.
static const int LFPG_COUNTER_DEBOUNCE_MS = 200;

// Capacity and consumption
static const float LFPG_COUNTER_CAPACITY    = 20.0;
static const float LFPG_COUNTER_CONSUMPTION = 5.0;

// ---------------------------------------------------------
// KIT — deployable item (same-model pattern as LogicGate)
// ---------------------------------------------------------
class LFPG_ElectronicCounter_Kit : Inventory_Base
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

    // Prevent orphan loop sound: ObjectDelete during OnPlacementComplete
    // interrupts the action callback before stopping the sound.
    override string GetLoopDeploySoundset()
    {
        return "";
    }

    override void SetActions()
    {
        super.SetActions();
        AddAction(ActionTogglePlaceObject);
        AddAction(LFPG_ActionPlaceElectronicCounter);
    }

    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[Counter_Kit] OnPlacementComplete: param=";
        tLog = tLog + position.ToString();
        tLog = tLog + " kitPos=";
        tLog = tLog + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        // No ECE_PLACE_ON_SURFACE — kills wall placement.
        string className = "LFPG_ElectronicCounter";
        EntityAI counter = GetGame().CreateObjectEx(className, finalPos, ECE_CREATEPHYSICS);
        if (counter)
        {
            counter.SetPosition(finalPos);
            counter.SetOrientation(finalOri);
            counter.Update();

            string deployMsg = "[Counter_Kit] Deployed LFPG_ElectronicCounter at ";
            deployMsg = deployMsg + finalPos.ToString();
            deployMsg = deployMsg + " ori=";
            deployMsg = deployMsg + finalOri.ToString();
            LFPG_Util.Info(deployMsg);

            // Only delete kit on successful spawn.
            GetGame().ObjectDelete(this);
        }
        else
        {
            string errCreate = "[Counter_Kit] Failed to create LFPG_ElectronicCounter! Kit preserved.";
            LFPG_Util.Error(errCreate);
            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                string failMsg = "[LFPG] Counter placement failed. Kit preserved.";
                pb.MessageStatus(failMsg);
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// DEVICE: PASSTHROUGH (2 IN + 1 OUT), gated counter
// ---------------------------------------------------------
class LFPG_ElectronicCounter : Inventory_Base
{
    // ---- Device identity ----
    protected int m_DeviceIdLow = 0;
    protected int m_DeviceIdHigh = 0;
    protected string m_DeviceId;

    // ---- Wires owned (output side) ----
    protected ref array<ref LFPG_WireData> m_Wires;

    // ---- Power state (set by graph propagation) ----
    protected bool m_PoweredNet = false;

    // ---- Counter value (0-9, synced to client for animation) ----
    protected int m_CounterValue = 0;

    // ---- Pulse state (gate open during output pulse) ----
    protected bool m_PulseActive = false;

    // ---- Per-input power tracking (for client LED visuals) ----
    protected bool m_Input0Powered = false;
    protected bool m_Input1Powered = false;

    // ---- Edge detection for toggle input (server only, NOT synced) ----
    protected bool m_ToggleWasHigh = false;

    // ---- Debounce: game time (ms) when power was last lost (server only) ----
    protected int m_PowerOffTime = 0;

    // ---- Flag: counter was restored from persistence, skip first power-on reset ----
    protected bool m_RestoredFromSave = false;

    // ---- Overload state ----
    protected bool m_Overloaded = false;

    // ---- Deletion guard ----
    protected bool m_LFPG_Deleting = false;

    // ---- Previous counter value for animation transitions (client only) ----
    protected int m_PrevCounterValue = -1;

    // ============================================
    // Constructor - SyncVars registered here, NOT EEInit
    // ============================================
    void LFPG_ElectronicCounter()
    {
        m_Wires = new array<ref LFPG_WireData>;
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
        RegisterNetSyncVariableInt("m_CounterValue");
        RegisterNetSyncVariableBool("m_PulseActive");
        RegisterNetSyncVariableBool("m_Input0Powered");
        RegisterNetSyncVariableBool("m_Input1Powered");
        RegisterNetSyncVariableBool("m_Overloaded");
    }

    // ============================================
    // Actions
    // ============================================
    override void SetActions()
    {
        super.SetActions();
        RemoveAction(ActionTakeItem);
        RemoveAction(ActionTakeItemToHands);
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
    // Lifecycle
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

        #ifdef SERVER
        LFPG_NetworkManager.Get().BroadcastOwnerWires(this);
        #endif
    }

    override void EEKilled(Object killer)
    {
        LFPG_DeviceLifecycle.OnDeviceKilled(this, m_DeviceId);

        #ifdef SERVER
        // Cancel pending pulse timer
        if (GetGame())
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_PulseOff);
        }

        bool dirty = false;
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            dirty = true;
        }
        if (m_PulseActive)
        {
            m_PulseActive = false;
            dirty = true;
        }
        if (m_CounterValue != 0)
        {
            m_CounterValue = 0;
            dirty = true;
        }
        if (m_Input0Powered)
        {
            m_Input0Powered = false;
            dirty = true;
        }
        if (m_Input1Powered)
        {
            m_Input1Powered = false;
            dirty = true;
        }
        if (dirty)
        {
            SetSynchDirty();
        }
        #endif

        super.EEKilled(killer);
    }

    override void EEDelete(EntityAI parent)
    {
        m_LFPG_Deleting = true;

        // Cancel pending pulse timer
        if (GetGame())
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_PulseOff);
        }

        LFPG_DeviceLifecycle.OnDeviceDeleted(this, m_DeviceId);
        super.EEDelete(parent);
    }

    override void EEItemLocationChanged(notnull InventoryLocation oldLoc, notnull InventoryLocation newLoc)
    {
        super.EEItemLocationChanged(oldLoc, newLoc);

        #ifdef SERVER
        if (m_DeviceId == "")
            return;

        bool wiresCut = LFPG_DeviceLifecycle.OnDeviceMoved(this, m_DeviceId, oldLoc, newLoc);
        if (wiresCut)
        {
            if (GetGame())
            {
                GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_PulseOff);
            }

            bool locDirty = false;
            if (m_PoweredNet)
            {
                m_PoweredNet = false;
                locDirty = true;
            }
            if (m_PulseActive)
            {
                m_PulseActive = false;
                locDirty = true;
            }
            if (m_CounterValue != 0)
            {
                m_CounterValue = 0;
                locDirty = true;
            }
            if (m_Input0Powered)
            {
                m_Input0Powered = false;
                locDirty = true;
            }
            if (m_Input1Powered)
            {
                m_Input1Powered = false;
                locDirty = true;
            }
            m_ToggleWasHigh = false;
            if (locDirty)
            {
                SetSynchDirty();
            }
        }
        #endif
    }

    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();
        LFPG_TryRegister();

        #ifndef SERVER
        LFPG_UpdateVisuals();

        // CableRenderer sync
        if (m_DeviceId != "")
        {
            LFPG_CableRenderer r = LFPG_CableRenderer.Get();
            if (r)
            {
                r.RequestDeviceSync(m_DeviceId, this);
                if (r.HasOwnerData(m_DeviceId))
                {
                    r.NotifyOwnerVisualChanged(m_DeviceId);
                }
            }
        }
        #endif
    }

    // ============================================
    // Visual update (client only)
    // ============================================
    protected void LFPG_UpdateVisuals()
    {
        #ifndef SERVER
        // LED rvmat swap (index 2 = light_led)
        if (m_PoweredNet)
        {
            SetObjectMaterial(2, LFPG_COUNTER_RVMAT_RED);
            // Segments (index 1 = camo2): emissive red when powered
            SetObjectMaterial(1, LFPG_COUNTER_RVMAT_RED);
        }
        else
        {
            SetObjectMaterial(2, LFPG_COUNTER_RVMAT_OFF);
            // Segments (index 1 = camo2): restore base (non-emissive) when off
            SetObjectMaterial(1, LFPG_COUNTER_RVMAT_BASE);
        }

        // 7-segment display animation
        if (m_PoweredNet)
        {
            // Disable previous animation first (MANDATORY — breaks otherwise)
            if (m_PrevCounterValue >= 0 && m_PrevCounterValue != m_CounterValue)
            {
                string prevIdx = m_PrevCounterValue.ToString();
                string prevAnim = "show_";
                prevAnim = prevAnim + prevIdx;
                SetAnimationPhase(prevAnim, 0.0);
            }

            // Enable current digit animation
            string curIdx = m_CounterValue.ToString();
            string curAnim = "show_";
            curAnim = curAnim + curIdx;
            SetAnimationPhase(curAnim, 1.0);

            m_PrevCounterValue = m_CounterValue;
        }
        else
        {
            // Power off — disable all animations
            LFPG_DisableAllDigits();
            m_PrevCounterValue = -1;
        }
        #endif
    }

    // Disable all 10 digit animations (client only)
    protected void LFPG_DisableAllDigits()
    {
        #ifndef SERVER
        int i;
        for (i = 0; i < 10; i = i + 1)
        {
            string idxStr = i.ToString();
            string animName = "show_";
            animName = animName + idxStr;
            SetAnimationPhase(animName, 0.0);
        }
        #endif
    }

    // ============================================
    // Counter logic (server only)
    // ============================================

    // Called when toggle rising edge detected while powered.
    protected void LFPG_IncrementCounter()
    {
        #ifdef SERVER
        if (m_CounterValue < 9)
        {
            // Normal increment: 0→1, 1→2, ... 8→9
            m_CounterValue = m_CounterValue + 1;
            SetSynchDirty();

            string incMsg = "[Counter] Increment to ";
            incMsg = incMsg + m_CounterValue.ToString();
            incMsg = incMsg + " id=";
            incMsg = incMsg + m_DeviceId;
            LFPG_Util.Debug(incMsg);
        }
        else
        {
            // At 9 → wrap: pulse output, then reset to 0
            LFPG_TriggerOutputPulse();
        }
        #endif
    }

    // Trigger a momentary output pulse (9→wrap behavior).
    protected void LFPG_TriggerOutputPulse()
    {
        #ifdef SERVER
        if (m_PulseActive)
        {
            // Already pulsing — ignore
            return;
        }

        // Reset counter to 0 and activate pulse
        m_CounterValue = 0;
        m_PulseActive = true;
        SetSynchDirty();

        string pulseMsg = "[Counter] 9->wrap PULSE ON id=";
        pulseMsg = pulseMsg + m_DeviceId;
        LFPG_Util.Info(pulseMsg);

        // Propagate immediately so downstream gets power
        LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);

        // Schedule auto-off after pulse duration
        if (GetGame())
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_PulseOff, LFPG_COUNTER_PULSE_MS, false);
        }
        #endif
    }

    // Timer callback: auto-reset pulse after duration.
    void LFPG_PulseOff()
    {
        #ifdef SERVER
        if (!m_PulseActive)
            return;

        m_PulseActive = false;
        SetSynchDirty();

        string offMsg = "[Counter] Pulse OFF id=";
        offMsg = offMsg + m_DeviceId;
        LFPG_Util.Info(offMsg);

        // Propagate to cut downstream power
        LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
        #endif
    }

    // ============================================
    // LFPG_IDevice interface
    // ============================================
    string LFPG_GetDeviceId()
    {
        return m_DeviceId;
    }

    // 3 ports: 2 input + 1 output
    int LFPG_GetPortCount()
    {
        return 3;
    }

    string LFPG_GetPortName(int idx)
    {
        if (idx == 0) return "input_0";
        if (idx == 1) return "input_1";
        if (idx == 2) return "output_0";
        return "";
    }

    int LFPG_GetPortDir(int idx)
    {
        if (idx == 0) return LFPG_PortDir.IN;
        if (idx == 1) return LFPG_PortDir.IN;
        if (idx == 2) return LFPG_PortDir.OUT;
        return -1;
    }

    string LFPG_GetPortLabel(int idx)
    {
        if (idx == 0) return "Power";
        if (idx == 1) return "Toggle";
        if (idx == 2) return "Output";
        return "";
    }

    bool LFPG_HasPort(string portName, int dir)
    {
        if (dir == LFPG_PortDir.IN && portName == "input_0") return true;
        if (dir == LFPG_PortDir.IN && portName == "input_1") return true;
        if (dir == LFPG_PortDir.OUT && portName == "output_0") return true;
        return false;
    }

    // Port positions: electronic_counter.p3d has port_input_0 + port_output_0.
    // input_1 (toggle) uses virtual offset (no memory point in model).
    vector LFPG_GetPortWorldPos(string portName)
    {
        // Map LFPG port names → p3d memory point names
        string memPoint;
        if (portName == "input_0")
        {
            memPoint = "port_input_0";
        }
        else if (portName == "output_0")
        {
            memPoint = "port_output_0";
        }
        else
        {
            memPoint = "";
        }

        // Try physical memory point first
        if (memPoint != "")
        {
            if (MemoryPointExists(memPoint))
            {
                return ModelToWorld(GetMemoryPointPos(memPoint));
            }
        }

        // Fallback: virtual offsets
        vector offset = "0 0.02 0";
        if (portName == "input_0")
        {
            offset = "0 0.02 -0.04";
        }
        else if (portName == "input_1")
        {
            // Toggle port — offset to the side of input_0
            offset = "0.04 0.02 -0.04";
        }
        else if (portName == "output_0")
        {
            offset = "0 0.02 0.04";
        }

        return ModelToWorld(offset);
    }

    // ---- PASSTHROUGH device type ----
    int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.PASSTHROUGH;
    }

    // ---- Source behavior ----
    bool LFPG_IsSource()
    {
        return true;
    }

    bool LFPG_GetSourceOn()
    {
        return m_PoweredNet;
    }

    // Gate: open only during output pulse (9→wrap).
    bool LFPG_IsGateOpen()
    {
        return m_PulseActive;
    }

    // Signal to ElecGraph that this device has gate logic.
    bool LFPG_IsGateCapable()
    {
        return true;
    }

    // Self-consumption: 5 u/s for display operation.
    // PASSTHROUGH with non-zero consumption follows CeilingLight pattern.
    // Without this, demand signal is 0 when no downstream is connected,
    // causing the source to allocate 0 → counter loses power after cold-start.
    float LFPG_GetConsumption()
    {
        return LFPG_COUNTER_CONSUMPTION;
    }

    // Throughput capacity.
    float LFPG_GetCapacity()
    {
        return LFPG_COUNTER_CAPACITY;
    }

    // Expose powered state for inspector.
    bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    // Expose counter value (for inspector or debug).
    int LFPG_GetCounterValue()
    {
        return m_CounterValue;
    }

    // Expose pulse state.
    bool LFPG_GetPulseActive()
    {
        return m_PulseActive;
    }

    // ============================================
    // Consumer behavior — called by graph propagation
    // ============================================
    void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        // Query per-port power from the graph
        bool newIn0 = false;
        bool newIn1 = false;

        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm)
        {
            string portIn0 = "input_0";
            string portIn1 = "input_1";
            newIn0 = nm.IsPortReceivingPower(m_DeviceId, portIn0);
            newIn1 = nm.IsPortReceivingPower(m_DeviceId, portIn1);
        }

        // Main power comes from input_0 only
        bool mainPower = newIn0;
        bool wasPowered = m_PoweredNet;
        bool changed = false;

        // ---- Power state transition ----
        if (!wasPowered && mainPower)
        {
            // Just turned ON — check if power was off long enough to be real
            int nowMs = 0;
            if (GetGame())
            {
                nowMs = GetGame().GetTime();
            }
            int offDuration = nowMs - m_PowerOffTime;

            if (offDuration > LFPG_COUNTER_DEBOUNCE_MS || m_PowerOffTime == 0)
            {
                // Check if value was just restored from persistence
                if (m_RestoredFromSave)
                {
                    // First power-on after server restart — preserve loaded value
                    m_RestoredFromSave = false;
                    m_PulseActive = false;

                    string restMsg = "[Counter] Power ON (restored from save, val=";
                    restMsg = restMsg + m_CounterValue.ToString();
                    restMsg = restMsg + ") id=";
                    restMsg = restMsg + m_DeviceId;
                    LFPG_Util.Info(restMsg);
                }
                else
                {
                    // Real power-on (genuine outage or first boot) → reset counter
                    m_CounterValue = 0;
                    m_PulseActive = false;

                    string onMsg = "[Counter] Power ON (real), reset to 0 id=";
                    onMsg = onMsg + m_DeviceId;
                    onMsg = onMsg + " offDur=";
                    onMsg = onMsg + offDuration.ToString();
                    LFPG_Util.Debug(onMsg);
                }
            }
            else
            {
                // False power cycle from graph re-propagation → preserve counter
                m_RestoredFromSave = false;
                // Still cancel any stale pulse (safety)
                if (m_PulseActive)
                {
                    if (GetGame())
                    {
                        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_PulseOff);
                    }
                    m_PulseActive = false;
                }

                string debMsg = "[Counter] Power ON (debounced, preserved val=";
                debMsg = debMsg + m_CounterValue.ToString();
                debMsg = debMsg + ") id=";
                debMsg = debMsg + m_DeviceId;
                debMsg = debMsg + " offDur=";
                debMsg = debMsg + offDuration.ToString();
                LFPG_Util.Info(debMsg);
            }

            // Capture current toggle state to prevent false rising edge
            m_ToggleWasHigh = newIn1;
            changed = true;
        }
        else if (wasPowered && !mainPower)
        {
            // Just turned OFF → record timestamp, cancel pulse, reset toggle
            if (GetGame())
            {
                m_PowerOffTime = GetGame().GetTime();
            }

            if (m_PulseActive)
            {
                if (GetGame())
                {
                    GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_PulseOff);
                }
                m_PulseActive = false;
            }
            m_ToggleWasHigh = false;
            changed = true;

            string offMsg2 = "[Counter] Power OFF id=";
            offMsg2 = offMsg2 + m_DeviceId;
            LFPG_Util.Debug(offMsg2);
        }
        else if (mainPower)
        {
            // Still powered — check toggle edge detection
            if (newIn1 && !m_ToggleWasHigh)
            {
                // Rising edge on toggle input → increment
                LFPG_IncrementCounter();
                // changed will be set by IncrementCounter via SetSynchDirty
            }
        }

        // Update toggle tracking (always, regardless of power state)
        m_ToggleWasHigh = newIn1;

        // Update SyncVars
        if (m_PoweredNet != mainPower)
        {
            m_PoweredNet = mainPower;
            changed = true;
        }

        if (m_Input0Powered != newIn0)
        {
            m_Input0Powered = newIn0;
            changed = true;
        }

        if (m_Input1Powered != newIn1)
        {
            m_Input1Powered = newIn1;
            changed = true;
        }

        if (changed)
        {
            SetSynchDirty();
        }

        string dbgLog = "[Counter] SetPowered(";
        dbgLog = dbgLog + powered.ToString();
        dbgLog = dbgLog + ")";
        dbgLog = dbgLog + " in0=";
        dbgLog = dbgLog + newIn0.ToString();
        dbgLog = dbgLog + " in1=";
        dbgLog = dbgLog + newIn1.ToString();
        dbgLog = dbgLog + " val=";
        dbgLog = dbgLog + m_CounterValue.ToString();
        dbgLog = dbgLog + " pulse=";
        dbgLog = dbgLog + m_PulseActive.ToString();
        dbgLog = dbgLog + " id=";
        dbgLog = dbgLog + m_DeviceId;
        LFPG_Util.Debug(dbgLog);
        #endif
    }

    // Overload state
    bool LFPG_GetOverloaded()
    {
        return m_Overloaded;
    }

    void LFPG_SetOverloaded(bool val)
    {
        #ifdef SERVER
        if (m_Overloaded != val)
        {
            m_Overloaded = val;
            SetSynchDirty();
        }
        #endif
    }

    // ---- Connection validation ----
    bool LFPG_CanConnectTo(Object other, string myPort, string otherPort)
    {
        if (!other) return false;
        if (!LFPG_HasPort(myPort, LFPG_PortDir.OUT)) return false;

        EntityAI otherEntity = EntityAI.Cast(other);
        if (!otherEntity) return false;

        string otherId = LFPG_DeviceAPI.GetDeviceId(otherEntity);
        if (otherId != "")
        {
            return LFPG_DeviceAPI.HasPort(other, otherPort, LFPG_PortDir.IN);
        }

        return LFPG_DeviceAPI.IsEnergyConsumer(otherEntity);
    }

    // ============================================
    // Wire ownership API (Splitter parity)
    // ============================================
    bool LFPG_HasWireStore()
    {
        return true;
    }

    array<ref LFPG_WireData> LFPG_GetWires()
    {
        return m_Wires;
    }

    string LFPG_GetWiresJSON()
    {
        return LFPG_WireHelper.GetJSON(m_Wires);
    }

    bool LFPG_AddWire(LFPG_WireData wd)
    {
        if (!wd) return false;

        if (wd.m_SourcePort == "")
        {
            string defaultPort = "output_0";
            wd.m_SourcePort = defaultPort;
        }

        if (!LFPG_HasPort(wd.m_SourcePort, LFPG_PortDir.OUT))
        {
            string warnMsg = "[Counter] AddWire rejected: not an output port: ";
            warnMsg = warnMsg + wd.m_SourcePort;
            LFPG_Util.Warn(warnMsg);
            return false;
        }

        bool result = LFPG_WireHelper.AddWire(m_Wires, wd);
        if (result)
        {
            #ifdef SERVER
            SetSynchDirty();
            #endif
        }
        return result;
    }

    bool LFPG_ClearWires()
    {
        bool result = LFPG_WireHelper.ClearAll(m_Wires);
        if (result)
        {
            #ifdef SERVER
            SetSynchDirty();
            #endif
        }
        return result;
    }

    bool LFPG_ClearWiresForCreator(string creatorId)
    {
        bool result = LFPG_WireHelper.ClearForCreator(m_Wires, creatorId);
        if (result)
        {
            #ifdef SERVER
            SetSynchDirty();
            #endif
        }
        return result;
    }

    bool LFPG_PruneMissingTargets()
    {
        ref map<string, bool> validIds = LFPG_NetworkManager.Get().GetCachedValidIds();
        if (!validIds)
        {
            validIds = new map<string, bool>;
            array<EntityAI> all = new array<EntityAI>;
            LFPG_DeviceRegistry.Get().GetAll(all);
            int vi;
            for (vi = 0; vi < all.Count(); vi = vi + 1)
            {
                string did = LFPG_DeviceAPI.GetOrCreateDeviceId(all[vi]);
                if (did != "")
                {
                    validIds[did] = true;
                }
            }
        }

        bool result = LFPG_WireHelper.PruneMissingTargets(m_Wires, validIds);
        if (result)
        {
            #ifdef SERVER
            SetSynchDirty();
            #endif
        }
        return result;
    }

    // ============================================
    // Persistence
    // ============================================
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);

        ctx.Write(m_DeviceIdLow);
        ctx.Write(m_DeviceIdHigh);
        // m_PoweredNet: NOT persisted (derived state)
        // m_PulseActive: NOT persisted (momentary)

        string json = "";
        LFPG_WireHelper.SerializeJSON(m_Wires, json);
        ctx.Write(json);

        // v2.0.1: Persist counter value across restarts
        ctx.Write(m_CounterValue);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version))
            return false;

        if (!ctx.Read(m_DeviceIdLow))
        {
            string errLow = "[Counter] OnStoreLoad: failed to read m_DeviceIdLow";
            LFPG_Util.Error(errLow);
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            string errHigh = "[Counter] OnStoreLoad: failed to read m_DeviceIdHigh";
            LFPG_Util.Error(errHigh);
            return false;
        }

        LFPG_UpdateDeviceIdString();

        string json = "";
        if (!ctx.Read(json))
        {
            string errMsg = "[Counter] OnStoreLoad: failed to read wires json for ";
            errMsg = errMsg + m_DeviceId;
            LFPG_Util.Error(errMsg);
            return false;
        }
        string tag = "LFPG_ElectronicCounter";
        LFPG_WireHelper.DeserializeJSON(m_Wires, json, tag);

        // v2.0.1: Read persisted counter value (soft read for backward compat)
        int loadedCounter = 0;
        if (ctx.Read(loadedCounter))
        {
            // Clamp to valid range 0-9
            if (loadedCounter < 0 || loadedCounter > 9)
            {
                loadedCounter = 0;
            }
            m_CounterValue = loadedCounter;
            m_RestoredFromSave = true;

            string cntMsg = "[Counter] OnStoreLoad: restored counter=";
            cntMsg = cntMsg + m_CounterValue.ToString();
            cntMsg = cntMsg + " id=";
            cntMsg = cntMsg + m_DeviceId;
            LFPG_Util.Debug(cntMsg);
        }
        else
        {
            // Old save format without counter — default to 0
            m_CounterValue = 0;
        }

        return true;
    }
};
