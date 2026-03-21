// =========================================================
// LF_PowerGrid - Electronic Counter (v2.0.0 — Refactor)
//
// LFPG_ElectronicCounter: PASSTHROUGH, 2 IN + 1 OUT. GATE.
//   input_0 = power supply, input_1 = toggle (edge-triggered counter)
//   output_0 = pulse output on 9→0 wrap
//   Extends LFPG_WireOwnerBase (Refactor v4.1).
//
// Persistence: [base: DeviceId + ver + wireJSON] + m_CounterValue
// =========================================================

static const string LFPG_COUNTER_RVMAT_OFF  = "\\LFPowerGrid\\data\\button\\materials\\led_off.rvmat";
static const string LFPG_COUNTER_RVMAT_RED  = "\\LFPowerGrid\\data\\electronic_counter\\electronic_counter_red.rvmat";
static const string LFPG_COUNTER_RVMAT_BASE = "\\LFPowerGrid\\data\\electronic_counter\\electronic_counter.rvmat";
static const int    LFPG_COUNTER_PULSE_MS   = 2000;
static const int    LFPG_COUNTER_DEBOUNCE_MS = 200;
static const float  LFPG_COUNTER_CAPACITY    = 20.0;
static const float  LFPG_COUNTER_CONSUMPTION = 5.0;

// ---------------------------------------------------------
// KIT — unchanged
// ---------------------------------------------------------
class LFPG_ElectronicCounter_Kit : Inventory_Base
{
    override bool IsDeployable() { return true; }
    override bool CanDisplayCargo() { return false; }
    override bool CanBePlaced(Man player, vector position) { return true; }
    override bool DoPlacingHeightCheck() { return false; }
    override string GetDeploySoundset() { return "placeBarbedWire_SoundSet"; }
    override string GetLoopDeploySoundset() { return ""; }

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
        LFPG_Util.Info(tLog);
        string spawnType = "LFPG_ElectronicCounter";
        EntityAI ent = GetGame().CreateObjectEx(spawnType, finalPos, ECE_CREATEPHYSICS);
        if (ent)
        {
            ent.SetPosition(finalPos);
            ent.SetOrientation(finalOri);
            ent.Update();
            string deployMsg = "[Counter_Kit] Deployed at ";
            deployMsg = deployMsg + finalPos.ToString();
            LFPG_Util.Info(deployMsg);
            GetGame().ObjectDelete(this);
        }
        else
        {
            LFPG_Util.Error("[Counter_Kit] Failed to create! Kit preserved.");
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
// DEVICE: PASSTHROUGH, counter + edge detection + pulse output
// ---------------------------------------------------------
class LFPG_ElectronicCounter : LFPG_WireOwnerBase
{
    // ---- SyncVars ----
    protected bool m_PoweredNet    = false;
    protected int  m_CounterValue  = 0;
    protected bool m_PulseActive   = false;
    protected bool m_Input0Powered = false;
    protected bool m_Input1Powered = false;
    protected bool m_Overloaded    = false;

    // ---- Non-synced state ----
    protected bool m_ToggleWasHigh    = false;
    protected int  m_PowerOffTime     = 0;
    protected bool m_RestoredFromSave = false;

    // ---- Client visual state ----
    protected int m_PrevCounterValue = -1;

    void LFPG_ElectronicCounter()
    {
        string varPowered  = "m_PoweredNet";
        string varCounter  = "m_CounterValue";
        string varPulse    = "m_PulseActive";
        string varIn0      = "m_Input0Powered";
        string varIn1      = "m_Input1Powered";
        string varOverload = "m_Overloaded";
        RegisterNetSyncVariableBool(varPowered);
        RegisterNetSyncVariableInt(varCounter);
        RegisterNetSyncVariableBool(varPulse);
        RegisterNetSyncVariableBool(varIn0);
        RegisterNetSyncVariableBool(varIn1);
        RegisterNetSyncVariableBool(varOverload);

        string pIn0  = "input_0";
        string pIn1  = "input_1";
        string pOut0 = "output_0";
        string lIn0  = "Power";
        string lIn1  = "Toggle";
        string lOut0 = "Output";
        LFPG_AddPort(pIn0, LFPG_PortDir.IN, lIn0);
        LFPG_AddPort(pIn1, LFPG_PortDir.IN, lIn1);
        LFPG_AddPort(pOut0, LFPG_PortDir.OUT, lOut0);
    }

    // ---- DeviceAPI ----
    override int LFPG_GetDeviceType() { return LFPG_DeviceType.PASSTHROUGH; }
    override bool LFPG_IsSource() { return true; }
    override bool LFPG_GetSourceOn() { return m_PoweredNet; }
    override bool LFPG_IsGateCapable() { return true; }
    override bool LFPG_IsGateOpen() { return m_PulseActive; }
    override float LFPG_GetConsumption() { return LFPG_COUNTER_CONSUMPTION; }
    override float LFPG_GetCapacity() { return LFPG_COUNTER_CAPACITY; }
    override bool LFPG_IsPowered() { return m_PoweredNet; }
    override bool LFPG_GetOverloaded() { return m_Overloaded; }

    bool LFPG_GetPulseActive() { return m_PulseActive; }
    int LFPG_GetCounterValue() { return m_CounterValue; }

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
    // SetPowered — complex: debounce, edge, restore
    // ============================================
    override void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
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

        bool mainPower = newIn0;
        bool wasPowered = m_PoweredNet;
        bool changed = false;

        // ---- Power state transition ----
        if (!wasPowered && mainPower)
        {
            int nowMs = 0;
            if (GetGame())
            {
                nowMs = GetGame().GetTime();
            }
            int offDuration = nowMs - m_PowerOffTime;

            if (offDuration > LFPG_COUNTER_DEBOUNCE_MS || m_PowerOffTime == 0)
            {
                if (m_RestoredFromSave)
                {
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
                m_RestoredFromSave = false;
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

            m_ToggleWasHigh = newIn1;
            changed = true;
        }
        else if (wasPowered && !mainPower)
        {
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
            if (newIn1 && !m_ToggleWasHigh)
            {
                LFPG_IncrementCounter();
            }
        }

        m_ToggleWasHigh = newIn1;

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

    // ============================================
    // Counter logic
    // ============================================
    protected void LFPG_IncrementCounter()
    {
        #ifdef SERVER
        if (m_CounterValue < 9)
        {
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
            LFPG_TriggerOutputPulse();
        }
        #endif
    }

    protected void LFPG_TriggerOutputPulse()
    {
        #ifdef SERVER
        if (m_PulseActive)
        {
            return;
        }

        m_CounterValue = 0;
        m_PulseActive = true;
        SetSynchDirty();

        string pulseMsg = "[Counter] 9->wrap PULSE ON id=";
        pulseMsg = pulseMsg + m_DeviceId;
        LFPG_Util.Info(pulseMsg);

        LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);

        if (GetGame())
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_PulseOff, LFPG_COUNTER_PULSE_MS, false);
        }
        #endif
    }

    void LFPG_PulseOff()
    {
        #ifdef SERVER
        if (!m_PulseActive)
            return;
        m_PulseActive = false;
        SetSynchDirty();

        string offMsg = "[Counter] PULSE OFF id=";
        offMsg = offMsg + m_DeviceId;
        LFPG_Util.Info(offMsg);

        LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
        #endif
    }

    // ---- Lifecycle hooks ----
    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        if (GetGame())
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_PulseOff);
        }

        bool dirty = false;
        if (m_PoweredNet) { m_PoweredNet = false; dirty = true; }
        if (m_PulseActive) { m_PulseActive = false; dirty = true; }
        if (m_Input0Powered) { m_Input0Powered = false; dirty = true; }
        if (m_Input1Powered) { m_Input1Powered = false; dirty = true; }
        if (dirty) { SetSynchDirty(); }
        #endif
    }

    override void LFPG_OnDeleted()
    {
        if (GetGame())
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_PulseOff);
        }
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        if (GetGame())
        {
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_PulseOff);
        }
        bool dirty = false;
        if (m_PoweredNet) { m_PoweredNet = false; dirty = true; }
        if (m_PulseActive) { m_PulseActive = false; dirty = true; }
        if (m_Input0Powered) { m_Input0Powered = false; dirty = true; }
        if (m_Input1Powered) { m_Input1Powered = false; dirty = true; }
        if (dirty) { SetSynchDirty(); }
        #endif
    }

    // ---- Visual sync ----
    override void LFPG_OnVarSyncDevice()
    {
        LFPG_UpdateVisuals();
    }

    protected void LFPG_UpdateVisuals()
    {
        #ifndef SERVER
        if (m_PoweredNet)
        {
            SetObjectMaterial(2, LFPG_COUNTER_RVMAT_RED);
            SetObjectMaterial(1, LFPG_COUNTER_RVMAT_RED);
        }
        else
        {
            SetObjectMaterial(2, LFPG_COUNTER_RVMAT_OFF);
            SetObjectMaterial(1, LFPG_COUNTER_RVMAT_BASE);
        }

        if (m_PoweredNet)
        {
            if (m_PrevCounterValue >= 0 && m_PrevCounterValue != m_CounterValue)
            {
                string prevIdx = m_PrevCounterValue.ToString();
                string prevAnim = "show_";
                prevAnim = prevAnim + prevIdx;
                SetAnimationPhase(prevAnim, 0.0);
            }

            string curIdx = m_CounterValue.ToString();
            string curAnim = "show_";
            curAnim = curAnim + curIdx;
            SetAnimationPhase(curAnim, 1.0);

            m_PrevCounterValue = m_CounterValue;
        }
        else
        {
            LFPG_DisableAllDigits();
            m_PrevCounterValue = -1;
        }
        #endif
    }

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

    // ---- Persistence (m_CounterValue) ----
    override void LFPG_OnStoreSaveDevice(ParamsWriteContext ctx)
    {
        ctx.Write(m_CounterValue);
    }

    override bool LFPG_OnStoreLoadDevice(ParamsReadContext ctx, int deviceVer)
    {
        int loadedCounter = 0;
        if (ctx.Read(loadedCounter))
        {
            m_CounterValue = loadedCounter;
            m_RestoredFromSave = true;
        }
        return true;
    }
};
