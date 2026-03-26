// =========================================================
// LF_PowerGrid - Memory Cell / SR Latch (v2.0.0 — Refactor)
//
// LFPG_MemoryCell: PASSTHROUGH, 4 IN + 2 OUT. GATE.
//   input_0 = power, input_1 = toggle, input_2 = reset, input_3 = set
//   output_0 = active (Q), output_1 = inverted (!Q)
//   Extends LFPG_WireOwnerBase (Refactor v4.1).
//
// Persistence: [base: DeviceId + ver + wireJSON] + m_CellActive
// =========================================================

static const string LFPG_MCELL_RVMAT_OFF    = "\LFPowerGrid\data\button\materials\led_off.rvmat";
static const string LFPG_MCELL_RVMAT_GREEN   = "\LFPowerGrid\data\button\materials\led_green.rvmat";
static const string LFPG_MCELL_RVMAT_RED     = "\LFPowerGrid\data\button\materials\led_red.rvmat";
static const float  LFPG_MCELL_CAPACITY      = 20.0;

// ---------------------------------------------------------
// KIT — unchanged (inherits LFPG_LogicGate_Kit)
// ---------------------------------------------------------
class LFPG_MemoryCell_Kit : LFPG_LogicGate_Kit
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_MemoryCell";
    }
};

// ---------------------------------------------------------
// DEVICE: PASSTHROUGH, SR latch with rising edge detection
// ---------------------------------------------------------
class LFPG_MemoryCell : LFPG_WireOwnerBase
{
    // ---- SyncVars ----
    protected bool m_PoweredNet = false;
    protected bool m_Overloaded = false;
    protected bool m_CellActive = false;

    // ---- Non-synced edge detection ----
    protected bool m_PrevToggle = false;
    protected bool m_PrevReset  = false;
    protected bool m_PrevSet    = false;
    protected bool m_RoutingApplied = false;

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
    override bool LFPG_IsGateCapable() { return true; }
    override bool LFPG_IsGateOpen() { return m_CellActive; }
    override float LFPG_GetConsumption() { return 0.0; }
    override float LFPG_GetCapacity() { return LFPG_MCELL_CAPACITY; }
    override bool LFPG_IsPowered() { return m_PoweredNet; }
    override bool LFPG_GetOverloaded() { return m_Overloaded; }

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
    // SetPowered — rising edge detection on control inputs
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

        // --- Rising-edge detection on control inputs ---
        // Priority: Set > Reset > Toggle
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm)
        {
            string portToggle = "input_1";
            string portReset  = "input_2";
            string portSet    = "input_3";

            bool curToggle = nm.IsPortReceivingPower(m_DeviceId, portToggle);
            bool curReset  = nm.IsPortReceivingPower(m_DeviceId, portReset);
            bool curSet    = nm.IsPortReceivingPower(m_DeviceId, portSet);

            bool newState = m_CellActive;

            if (curSet && !m_PrevSet)
            {
                newState = true;
            }
            else if (curReset && !m_PrevReset)
            {
                newState = false;
            }
            else if (curToggle && !m_PrevToggle)
            {
                if (m_CellActive)
                {
                    newState = false;
                }
                else
                {
                    newState = true;
                }
            }

            m_PrevToggle = curToggle;
            m_PrevReset  = curReset;
            m_PrevSet    = curSet;

            if (newState != m_CellActive)
            {
                m_CellActive = newState;
                changed = true;

                LFPG_ApplyRouting();

                string scLog = "[MemoryCell] State changed: active=";
                scLog = scLog + m_CellActive.ToString();
                scLog = scLog + " id=";
                scLog = scLog + m_DeviceId;
                LFPG_Util.Info(scLog);
            }
        }

        if (!m_RoutingApplied)
        {
            LFPG_ApplyRouting();
        }

        if (changed)
        {
            SetSynchDirty();
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

        m_RoutingApplied = true;

        string rLog = "[MemoryCell] ApplyRouting: active=";
        rLog = rLog + m_CellActive.ToString();
        rLog = rLog + " id=";
        rLog = rLog + m_DeviceId;
        LFPG_Util.Debug(rLog);
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

    // ---- Init hook: deferred routing after graph edges exist ----
    override void LFPG_OnInitDevice()
    {
        #ifdef SERVER
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_DeferredRouting, 500, false);
        #endif

        // Force correct symbol texture on client at spawn.
        // Same shared-p3d cache bug as LogicGateBase.
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
        string symTex = "\LFPowerGrid\data\logic_gate\data\memory_cell_symbol_mem.paa";
        SetObjectTexture(0, symTex);

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

        if (desiredState == 2)
        {
            SetObjectMaterial(1, LFPG_MCELL_RVMAT_RED);
        }
        else if (desiredState == 1)
        {
            SetObjectMaterial(1, LFPG_MCELL_RVMAT_GREEN);
        }
        else
        {
            SetObjectMaterial(1, LFPG_MCELL_RVMAT_OFF);
        }

        SetObjectMaterial(2, LFPG_MCELL_RVMAT_OFF);
        SetObjectMaterial(3, LFPG_MCELL_RVMAT_OFF);
        #endif
    }

    // ---- Persistence (m_CellActive) ----
    override void LFPG_OnStoreSaveDevice(ParamsWriteContext ctx)
    {
        ctx.Write(m_CellActive);
    }

    override bool LFPG_OnStoreLoadDevice(ParamsReadContext ctx, int deviceVer)
    {
        if (!ctx.Read(m_CellActive))
        {
            string err = "[LFPG_MemoryCell] OnStoreLoad: failed to read m_CellActive";
            LFPG_Util.Error(err);
            return false;
        }
        return true;
    }
};
