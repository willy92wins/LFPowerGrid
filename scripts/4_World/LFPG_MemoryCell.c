// =========================================================
// LF_PowerGrid - Memory Cell / SR Latch (v3.1.0 — Routing fix)
//
// LFPG_MemoryCell: PASSTHROUGH, 4 IN + 2 OUT.
//   input_0 = power, input_1 = toggle, input_2 = reset, input_3 = set
//   output_0 = active (Q), output_1 = inverted (!Q)
//   Extends LFPG_WireOwnerBase (Refactor v4.1).
//
// v3.1.0: ApplyRouting runs every SetPowered call. Fixes edge
//   flags for cables connected after init (edges start ENABLED,
//   routing must re-disable the correct port each pass).
//   Diagnostic log every SetPowered call for troubleshooting.
//
// Persistence: [base: DeviceId + ver + wireJSON] + m_LatchState
// =========================================================

static const string LFPG_MCELL_RVMAT_OFF   = "\LFPowerGrid\data\button\materials\led_off.rvmat";
static const string LFPG_MCELL_RVMAT_GREEN  = "\LFPowerGrid\data\button\materials\led_green.rvmat";
static const string LFPG_MCELL_RVMAT_RED    = "\LFPowerGrid\data\button\materials\led_red.rvmat";
static const float  LFPG_MCELL_CAPACITY     = 20.0;

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
// DEVICE: PASSTHROUGH, SR latch + toggle override
// ---------------------------------------------------------
class LFPG_MemoryCell : LFPG_WireOwnerBase
{
    // ---- SyncVars ----
    protected bool m_PoweredNet = false;
    protected bool m_Overloaded = false;
    protected bool m_CellActive = false;

    // ---- Server-only: persisted latch ----
    protected bool m_LatchState = false;

    // ---- Client visual cache ----
    protected int m_LastVisualState = -1;

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
    override float LFPG_GetConsumption() { return 0.0; }
    override float LFPG_GetCapacity() { return LFPG_MCELL_CAPACITY; }
    override bool LFPG_IsPowered() { return m_PoweredNet; }
    override bool LFPG_GetOverloaded() { return m_Overloaded; }

    // Public getter for DeviceInspector UI
    bool LFPG_GetCellActive() { return m_CellActive; }

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
    // SetPowered — level-sensitive, no edge detection
    //
    // Priority: TOGGLE > SET/RESET > latch hold
    //   TOGGLE energized   -> effective ON (override)
    //   SET && !RESET       -> latch ON
    //   RESET && !SET       -> latch OFF
    //   SET && RESET        -> latch unchanged
    //   neither             -> latch unchanged
    //   TOGGLE off          -> effective = latch
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

        // --- Read control inputs (level-sensitive) ---
        bool hasToggle = false;
        bool hasSet    = false;
        bool hasReset  = false;

        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm)
        {
            string portToggle = "input_1";
            string portReset  = "input_2";
            string portSet    = "input_3";
            hasToggle = nm.IsPortReceivingPower(m_DeviceId, portToggle);
            hasReset  = nm.IsPortReceivingPower(m_DeviceId, portReset);
            hasSet    = nm.IsPortReceivingPower(m_DeviceId, portSet);
        }

        // --- Update latch from SET/RESET (always, even under TOGGLE) ---
        if (hasSet && !hasReset)
        {
            m_LatchState = true;
        }
        else if (hasReset && !hasSet)
        {
            m_LatchState = false;
        }
        // Both or neither: latch unchanged

        // --- Compute effective state: TOGGLE overrides ---
        bool newActive = m_LatchState;
        if (hasToggle)
        {
            newActive = true;
        }

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
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_DeferredRouting, deferDelay, deferRepeat);
        }

        if (changed)
        {
            SetSynchDirty();
        }

        // --- Diagnostic log (every call, remove after confirmed working) ---
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
        dLog = dLog + " chg=";
        dLog = dLog + stateChanged.ToString();
        dLog = dLog + " id=";
        dLog = dLog + m_DeviceId;
        LFPG_Util.Info(dLog);
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
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_DeferredRouting, initDelay, initRepeat);
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
        return true;
    }
};
