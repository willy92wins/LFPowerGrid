// =========================================================
// LF_PowerGrid - Memory Cell / SR Latch (v3.1.1 — rising-edge Toggle)
//
// LFPG_MemoryCell: PASSTHROUGH, 4 IN + 2 OUT.
//   input_0 = power, input_1 = toggle, input_2 = reset, input_3 = set
//   output_0 = active (Q), output_1 = inverted (!Q)
//   Extends LFPG_WireOwnerBase (Refactor v4.1).
//
// Toggle is a rising-edge pulse: each qualifying rising edge inverts the
// latch. Set/Reset remain level-sensitive and apply after the edge. The
// active output is always the latch (no Toggle override). Self-consumption
// keeps input edges live when the active output has no downstream load.
//
// Persistence: [base: DeviceId + ver + wireJSON] + m_LatchState
// =========================================================

static const string LFPG_MCELL_RVMAT_OFF   = "LFPowerGrid\\data\\button\\materials\\led_off.rvmat";
static const string LFPG_MCELL_RVMAT_GREEN  = "LFPowerGrid\\data\\button\\materials\\led_green.rvmat";
static const string LFPG_MCELL_RVMAT_RED    = "LFPowerGrid\\data\\button\\materials\\led_red.rvmat";
static const float  LFPG_MCELL_CAPACITY     = 100.0;
static const float  LFPG_MCELL_CONSUMPTION  = 1.0;
static const int    LFPG_MCELL_TOGGLE_REARM_MS = 1000;
static const int    LFPG_MCELL_TOGGLE_LOAD_GRACE_MS = 30000;

static bool LFPG_MCellToggleShouldFlip(int lowSinceMs, int powerSinceMs, int graceUntilMs, int nowMs, int rearmMs)
{
	if (lowSinceMs < 0)
	{
		return false;
	}
	if (powerSinceMs < 0)
	{
		return false;
	}
	if (nowMs < graceUntilMs)
	{
		return false;
	}
	if (nowMs - lowSinceMs < rearmMs)
	{
		return false;
	}
	if (nowMs - powerSinceMs < rearmMs)
	{
		return false;
	}
	return true;
}

static bool LFPG_MCellPowerRestartCounter(int powerSinceMs, int powerLowSinceMs, int nowMs, int rearmMs)
{
	if (powerSinceMs < 0)
	{
		return true;
	}
	if (powerLowSinceMs >= 0)
	{
		if (nowMs - powerLowSinceMs >= rearmMs)
		{
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------
// KIT
// ---------------------------------------------------------
class LFPG_MemoryCell_Kit : LFPG_LogicGate_Kit
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_MemoryCell";
    }
};

// ---------------------------------------------------------
// DEVICE: PASSTHROUGH, SR latch + rising-edge toggle
// ---------------------------------------------------------
class LFPG_MemoryCell : LFPG_WireOwnerBase
{
    // ---- SyncVars ----
    protected bool m_PoweredNet = false;
    protected bool m_Overloaded = false;
    protected bool m_CellActive = false;

    // ---- Server-only: persisted latch ----
    protected bool m_LatchState = false;

    // ---- Server-only: Toggle edge state (not persisted, not synced) ----
    protected bool m_ToggleLastHigh = true;
    protected int m_ToggleLowSinceMs = -1;
    protected int m_PowerSinceMs = -1;
    protected int m_PowerLowSinceMs = -1;
    protected int m_ToggleGraceUntilMs = 0;

    // ---- Client visual cache ----
    #ifndef SERVER
    protected int m_LastVisualState = -1;
    #endif

    void LFPG_MemoryCell()
    {
        string varPowered  = "m_PoweredNet";
        string varOverload = "m_Overloaded";
        string varCell     = "m_CellActive";
        RegisterNetSyncVariableBool(varPowered);
        RegisterNetSyncVariableBool(varOverload);
        RegisterNetSyncVariableBool(varCell);

        string pIn0  = "input_0";
        string pIn1  = "input_1";
        string pIn2  = "input_2";
        string pIn3  = "input_3";
        string pOut0 = "output_0";
        string pOut1 = "output_1";
        string lIn0  = "Power";
        string lIn1  = "Toggle";
        string lIn2  = "Reset";
        string lIn3  = "Set";
        string lOut0 = "Output";
        string lOut1 = "Inverted";
        LFPG_AddPort(pIn0, LFPG_PortDir.IN, lIn0);
        LFPG_AddPort(pIn1, LFPG_PortDir.IN, lIn1);
        LFPG_AddPort(pIn2, LFPG_PortDir.IN, lIn2);
        LFPG_AddPort(pIn3, LFPG_PortDir.IN, lIn3);
        LFPG_AddPort(pOut0, LFPG_PortDir.OUT, lOut0);
        LFPG_AddPort(pOut1, LFPG_PortDir.OUT, lOut1);
    }

    // ---- DeviceAPI ----
    override int LFPG_GetDeviceType() { return LFPG_DeviceType.PASSTHROUGH; }
    override bool LFPG_IsSource() { return true; }
    override bool LFPG_GetSourceOn() { return m_PoweredNet; }
    override bool LFPG_IsGateCapable() { return false; }
    override float LFPG_GetConsumption() { return LFPG_MCELL_CONSUMPTION; }
    override float LFPG_GetCapacity() { return LFPG_MCELL_CAPACITY; }
    override bool LFPG_IsPowered() { return m_PoweredNet; }
    override bool LFPG_GetOverloaded() { return m_Overloaded; }

    // Public getter for DeviceInspector UI
    #ifndef SERVER
    bool LFPG_GetCellActive() { return m_CellActive; }
    #endif

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
    // SetPowered — Toggle rising edge, then Set/Reset by level
    //
    // R1: Toggle must have been low for LFPG_MCELL_TOGGLE_REARM_MS.
    // R2: Power must have been high for the same rearm window. A Power
    //     drop shorter than that window does not restart the counter;
    //     after a drop of 1 s or more, Power and Toggle returning together
    //     do not count.
    // R3: after OnStoreLoad, no edge until LFPG_MCELL_TOGGLE_LOAD_GRACE_MS.
    // R4: a Toggle never seen low does not fire (starts last-high).
    // R5: apply the edge first, then Set/Reset by level.
    // Active output is always m_LatchState.
    // ============================================
    override void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        bool changed = false;

        if (m_PoweredNet != powered)
        {
            m_PoweredNet = powered;
            changed = true;
        }

        bool hasToggle = false;
        bool hasSet    = false;
        bool hasReset  = false;
        bool hasPower  = false;
        bool flip      = false;
        int nowMs = 0;
        if (g_Game)
        {
            nowMs = g_Game.GetTime();
        }

        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm)
        {
            string portPower  = "input_0";
            string portToggle = "input_1";
            string portReset  = "input_2";
            string portSet    = "input_3";
            hasPower  = nm.IsPortReceivingPower(m_DeviceId, portPower);
            hasToggle = nm.IsPortReceivingPower(m_DeviceId, portToggle);
            hasReset  = nm.IsPortReceivingPower(m_DeviceId, portReset);
            hasSet    = nm.IsPortReceivingPower(m_DeviceId, portSet);
        }

        if (!hasPower)
        {
            if (m_PowerLowSinceMs < 0)
            {
                m_PowerLowSinceMs = nowMs;
            }
        }
        else
        {
            if (LFPG_MCellPowerRestartCounter(m_PowerSinceMs, m_PowerLowSinceMs, nowMs, LFPG_MCELL_TOGGLE_REARM_MS))
            {
                m_PowerSinceMs = nowMs;
            }
            m_PowerLowSinceMs = -1;
        }

        if (hasToggle)
        {
            if (!m_ToggleLastHigh)
            {
                flip = LFPG_MCellToggleShouldFlip(m_ToggleLowSinceMs, m_PowerSinceMs, m_ToggleGraceUntilMs, nowMs, LFPG_MCELL_TOGGLE_REARM_MS);
                if (flip)
                {
                    m_LatchState = !m_LatchState;
                }
            }
            m_ToggleLastHigh = true;
        }
        else
        {
            if (m_ToggleLastHigh || m_ToggleLowSinceMs < 0)
            {
                m_ToggleLowSinceMs = nowMs;
            }
            m_ToggleLastHigh = false;
        }

        if (hasSet && !hasReset)
        {
            m_LatchState = true;
        }
        else if (hasReset && !hasSet)
        {
            m_LatchState = false;
        }

        bool newActive = m_LatchState;

        // --- State change ---
        bool stateChanged = false;
        if (m_CellActive != newActive)
        {
            m_CellActive = newActive;
            changed = true;
            stateChanged = true;
        }

        // --- ALWAYS re-apply routing ---
        // Edges start ENABLED when wires are connected. If routing
        // was applied before the wire existed, the edge bypasses it.
        // Calling every SetPowered ensures edges match current state.
        // SetOutputPortEnabled is idempotent (no dirty mark if no change).
        LFPG_ApplyRouting();

        // If state actually changed, schedule deferred re-propagation
        // so downstream picks up the new routing in the next epoch.
        if (stateChanged)
        {
            int deferDelay = 50;
            bool deferRepeat = false;
            g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_DeferredRouting);
            g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_DeferredRouting, deferDelay, deferRepeat);
        }

        if (changed)
        {
            SetSynchDirty();
        }

        // Diagnostic log, gated so the multi-part string is not built when logging is
        // below Debug (level 2). Behavior-identical to the prior unconditional call.
        if (LFPG_LOG_LEVEL >= 2)
        {
            string dLog = "[MemoryCell] SetPowered v3.1: pw=";
            dLog = dLog + powered.ToString();
            dLog = dLog + " net=";
            dLog = dLog + m_PoweredNet.ToString();
            dLog = dLog + " T=";
            dLog = dLog + hasToggle.ToString();
            dLog = dLog + " S=";
            dLog = dLog + hasSet.ToString();
            dLog = dLog + " R=";
            dLog = dLog + hasReset.ToString();
            dLog = dLog + " latch=";
            dLog = dLog + m_LatchState.ToString();
            dLog = dLog + " active=";
            dLog = dLog + m_CellActive.ToString();
            dLog = dLog + " flip=";
            dLog = dLog + flip.ToString();
            dLog = dLog + " chg=";
            dLog = dLog + stateChanged.ToString();
            dLog = dLog + " id=";
            dLog = dLog + m_DeviceId;
            LFPG_Util.Debug(dLog);
        }
        #endif
    }

    // ============================================
    // Port routing: output_0 = Q, output_1 = !Q
    // ============================================
    protected void LFPG_ApplyRouting()
    {
        #ifdef SERVER
        if (m_DeviceId == "")
            return;

        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (!nm)
            return;

        LFPG_ElecGraph graph = nm.GetGraph();
        if (!graph)
            return;

        string portOut0 = "output_0";
        string portOut1 = "output_1";
        graph.SetOutputPortEnabled(m_DeviceId, portOut0, m_CellActive);
        graph.SetOutputPortEnabled(m_DeviceId, portOut1, !m_CellActive);
        #endif
    }

    protected void LFPG_DeferredRouting()
    {
        #ifdef SERVER
        LFPG_ApplyRouting();

        if (m_DeviceId != "")
        {
            LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
            if (nm)
            {
                nm.RequestPropagate(m_DeviceId);
            }
        }
        #endif
    }

    // ---- Init: deferred routing after graph edges exist ----
    override void LFPG_OnInitDevice()
    {
        #ifdef SERVER
        m_CellActive = m_LatchState;

        int initDelay = 500;
        bool initRepeat = false;
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_DeferredRouting);
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_DeferredRouting, initDelay, initRepeat);
        #endif

        LFPG_UpdateVisuals();
    }

    // ---- Lifecycle hooks ----
    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        #endif
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        bool dirty = false;
        if (m_PoweredNet) { m_PoweredNet = false; dirty = true; }
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
        // hiddenSelections from config.cpp (memory_cell.p3d):
        //   0 = light_led_input0  (Power)
        //   1 = light_led_input1  (Toggle)
        //   2 = light_led_output0 (Output Q)
        //   3 = light_led_input2  (Reset)
        //   4 = light_led_input3  (Set)

        int desiredState = 0;
        if (m_PoweredNet)
        {
            if (m_CellActive)
            {
                desiredState = 2;
            }
            else
            {
                desiredState = 1;
            }
        }

        if (desiredState == m_LastVisualState)
            return;

        m_LastVisualState = desiredState;

        // LED 0 (Power): green when device has power
        if (m_PoweredNet)
        {
            SetObjectMaterial(0, LFPG_MCELL_RVMAT_GREEN);
        }
        else
        {
            SetObjectMaterial(0, LFPG_MCELL_RVMAT_OFF);
        }

        // LED 1 (Toggle): off (runtime only, no SyncVar)
        SetObjectMaterial(1, LFPG_MCELL_RVMAT_OFF);

        // LED 2 (Output Q): green = active, red = powered but inactive
        if (m_PoweredNet && m_CellActive)
        {
            SetObjectMaterial(2, LFPG_MCELL_RVMAT_GREEN);
        }
        else if (m_PoweredNet)
        {
            SetObjectMaterial(2, LFPG_MCELL_RVMAT_RED);
        }
        else
        {
            SetObjectMaterial(2, LFPG_MCELL_RVMAT_OFF);
        }

        // LED 3 (Reset): off (momentary)
        SetObjectMaterial(3, LFPG_MCELL_RVMAT_OFF);

        // LED 4 (Set): off (momentary)
        SetObjectMaterial(4, LFPG_MCELL_RVMAT_OFF);
        #endif
    }

    // ---- Persistence: m_LatchState (1 bool, same binary format as v2) ----
    override void LFPG_OnStoreSaveDevice(ParamsWriteContext ctx)
    {
        ctx.Write(m_LatchState);
    }

    override bool LFPG_OnStoreLoadDevice(ParamsReadContext ctx, int deviceVer)
    {
        if (!ctx.Read(m_LatchState))
        {
            string err = "[LFPG_MemoryCell] OnStoreLoad: failed to read m_LatchState";
            LFPG_Util.Error(err);
            return false;
        }
        m_CellActive = m_LatchState;
        if (g_Game)
        {
            m_ToggleGraceUntilMs = g_Game.GetTime() + LFPG_MCELL_TOGGLE_LOAD_GRACE_MS;
        }
        return true;
    }
};
