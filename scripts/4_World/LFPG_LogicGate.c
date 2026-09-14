// =========================================================
// LF_PowerGrid - Logic Gates (v2.0.0 — Refactor)
//
// LFPG_LogicGateBase: PASSTHROUGH, 2 IN + 1 OUT. GATE.
//   Extends LFPG_WireOwnerBase (Refactor v4.1).
//   Gate logic via virtual LFPG_EvaluateGateLogic(in0, in1).
//
// LFPG_AND_Gate, LFPG_OR_Gate, LFPG_XOR_Gate: override gate logic.
//
// Ports: input_0 (IN), input_1 (IN), output_0 (OUT)
// LEDs: 0=input0, 1=input1, 2=output
// Persistence: [base: DeviceId + ver + wireJSON] — no extras
// =========================================================

static const string LFPG_GATE_RVMAT_OFF   = "\LFPowerGrid\data\button\materials\led_off.rvmat";
static const string LFPG_GATE_RVMAT_GREEN  = "\LFPowerGrid\data\button\materials\led_green.rvmat";
static const string LFPG_GATE_RVMAT_RED    = "\LFPowerGrid\data\button\materials\led_red.rvmat";
static const float  LFPG_GATE_CAPACITY     = 100.0;

class LFPG_LogicGate_Kit : LFPG_KitBase
{
    override void LFPG_AddPlaceAction()
    {
        AddAction(LFPG_ActionPlaceLogicGate);
    }

    override int LFPG_GetPlacementModes()
    {
        return 1;
    }

    override float LFPG_GetWallSurfaceOffset()
    {
        return 0.05;
    }

    override float LFPG_GetWallPitchOffset()
    {
        return 90.0;
    }

    override float LFPG_GetWallYawOffset()
    {
        return 180.0;
    }
};

class LFPG_AND_Gate_Kit : LFPG_LogicGate_Kit
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_AND_Gate";
    }
};
class LFPG_OR_Gate_Kit : LFPG_LogicGate_Kit
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_OR_Gate";
    }
};
class LFPG_XOR_Gate_Kit : LFPG_LogicGate_Kit
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_XOR_Gate";
    }
};

// ---------------------------------------------------------
// DEVICE BASE: PASSTHROUGH, 2 IN + 1 OUT, GATE
// ---------------------------------------------------------
class LFPG_LogicGateBase : LFPG_WireOwnerBase
{
    protected bool m_PoweredNet    = false;
    protected bool m_Input0Powered = false;
    protected bool m_Input1Powered = false;
    protected bool m_GateOpen      = false;
    protected bool m_Overloaded    = false;
    // One evaluation of slack for the convergence brownout in
    // LFPG_SetPowered. Not net-synced and not persisted: it is always
    // false while the gate is open, because opening requires a live
    // reading and a live reading clears it.
    protected bool m_BrownoutHeld  = false;

    void LFPG_LogicGateBase()
    {
        string varPowered  = "m_PoweredNet";
        string varIn0      = "m_Input0Powered";
        string varIn1      = "m_Input1Powered";
        string varOverload = "m_Overloaded";
        RegisterNetSyncVariableBool(varPowered);
        RegisterNetSyncVariableBool(varIn0);
        RegisterNetSyncVariableBool(varIn1);
        RegisterNetSyncVariableBool(varOverload);

        string pIn0  = "input_0";
        string pIn1  = "input_1";
        string pOut0 = "output_0";
        string lIn0  = "Input 0";
        string lIn1  = "Input 1";
        string lOut0 = "Output 0";
        LFPG_AddPort(pIn0, LFPG_PortDir.IN, lIn0);
        LFPG_AddPort(pIn1, LFPG_PortDir.IN, lIn1);
        LFPG_AddPort(pOut0, LFPG_PortDir.OUT, lOut0);
    }

    // ---- DeviceAPI ----
    override int LFPG_GetDeviceType() { return LFPG_DeviceType.PASSTHROUGH; }
    override bool LFPG_IsSource() { return true; }
    override bool LFPG_GetSourceOn() { return m_PoweredNet; }
    override bool LFPG_IsGateCapable() { return true; }
    override float LFPG_GetConsumption() { return 0.0; }
    override float LFPG_GetCapacity() { return LFPG_GATE_CAPACITY; }
    override bool LFPG_IsPowered() { return m_PoweredNet; }
    override bool LFPG_GetOverloaded() { return m_Overloaded; }

    // ---- Virtual gate logic (subclass overrides) ----
    bool LFPG_EvaluateGateLogic(bool in0, bool in1)
    {
        return false;
    }

    // ---- Gate state: latched in SetPowered, read by ElecGraph ----
    override bool LFPG_IsGateOpen()
    {
        return m_GateOpen;
    }

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

        bool newGate = LFPG_EvaluateGateLogic(newIn0, newIn1);
        bool gateChanged = false;

        // m_GateOpen is a latch, and IsPortReceivingPower answers with THIS
        // epoch's allocation. AllocateOutput zeroes every outgoing edge of a
        // distributor whose demand exceeds what it currently holds
        // (LFPG_ElecGraphImpl.c:3836-4161), so on the epoch right after this
        // gate opens - when its demand signal jumps from the closed-gate
        // probe (LFPG_GATE_PROBE_DEMAND, 1.0) to what the load downstream
        // really asks for - both input ports read dark while the upstream
        // ramps. Re-latching on that reading closes the gate, the raised
        // demand collapses back to the probe, the ramp restarts, and the
        // gate never converges. Measured in-game as in0=1 in1=1 output=0
        // powered=0 held for 24.6 s (B5, gs02-ev/B5-20260912/issue1).
        //
        // A single total brownout carries no truth-table information, so the
        // latch keeps its value for exactly one evaluation. The SECOND
        // consecutive dark reading is believed: a real all-off keeps
        // arriving, a ramp does not, and one evaluation is all the ramp
        // needs. The per-input flags below still follow the flow, so the
        // client LEDs are not held.
        bool totalBrownout = false;
        if (!newIn0 && !newIn1)
        {
            totalBrownout = true;
        }

        bool holdLatch = false;
        if (totalBrownout)
        {
            if (!m_BrownoutHeld)
            {
                m_BrownoutHeld = true;
                holdLatch = true;
            }
        }
        else
        {
            m_BrownoutHeld = false;
        }

        if (!holdLatch && m_GateOpen != newGate)
        {
            m_GateOpen = newGate;
            gateChanged = true;
        }

        bool changed = false;

        if (m_PoweredNet != powered)
        {
            m_PoweredNet = powered;
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

        if (gateChanged && nm)
        {
            nm.RequestPropagate(m_DeviceId);
        }

        // Gated so the multi-part string (incl. GetType()) is not built when logging is
        // below Debug (2). Behavior-identical to the prior unconditional call.
        if (LFPG_LOG_LEVEL >= 2)
        {
            string dbgLog = "[LogicGate] SetPowered(";
            dbgLog = dbgLog + powered.ToString();
            dbgLog = dbgLog + ") in0=";
            dbgLog = dbgLog + newIn0.ToString();
            dbgLog = dbgLog + " in1=";
            dbgLog = dbgLog + newIn1.ToString();
            dbgLog = dbgLog + " gate=";
            dbgLog = dbgLog + m_GateOpen.ToString();
            dbgLog = dbgLog + " id=";
            dbgLog = dbgLog + m_DeviceId;
            dbgLog = dbgLog + " type=";
            dbgLog = dbgLog + GetType();
            LFPG_Util.Debug(dbgLog);
        }
        #endif
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

    // ---- Lifecycle ----
    override void LFPG_OnInitDevice()
    {
        LFPG_UpdateVisuals();
    }

    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        bool dirty = false;
        if (m_PoweredNet) { m_PoweredNet = false; dirty = true; }
        if (m_Input0Powered) { m_Input0Powered = false; dirty = true; }
        if (m_Input1Powered) { m_Input1Powered = false; dirty = true; }
        if (m_GateOpen) { m_GateOpen = false; dirty = true; }
        if (dirty) { SetSynchDirty(); }
        #endif
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        bool dirty = false;
        if (m_PoweredNet) { m_PoweredNet = false; dirty = true; }
        if (m_Input0Powered) { m_Input0Powered = false; dirty = true; }
        if (m_Input1Powered) { m_Input1Powered = false; dirty = true; }
        if (m_GateOpen) { m_GateOpen = false; dirty = true; }
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
        // hiddenSelections: 0=led_input0, 1=led_input1, 2=led_output0

        // Input 0 LED
        if (m_Input0Powered)
        {
            SetObjectMaterial(0, LFPG_GATE_RVMAT_GREEN);
        }
        else
        {
            SetObjectMaterial(0, LFPG_GATE_RVMAT_OFF);
        }

        // Input 1 LED
        if (m_Input1Powered)
        {
            SetObjectMaterial(1, LFPG_GATE_RVMAT_GREEN);
        }
        else
        {
            SetObjectMaterial(1, LFPG_GATE_RVMAT_OFF);
        }

        // Output LED: evaluate gate logic client-side from synced per-input states
        bool hasAnyInput = false;
        if (m_Input0Powered || m_Input1Powered)
        {
            hasAnyInput = true;
        }

        bool gateOpen = LFPG_EvaluateGateLogic(m_Input0Powered, m_Input1Powered);

        if (gateOpen && hasAnyInput)
        {
            SetObjectMaterial(2, LFPG_GATE_RVMAT_GREEN);
        }
        else if (hasAnyInput)
        {
            SetObjectMaterial(2, LFPG_GATE_RVMAT_RED);
        }
        else
        {
            SetObjectMaterial(2, LFPG_GATE_RVMAT_OFF);
        }
        #endif
    }

    // ---- No persist extras ----
};

// ---------------------------------------------------------
// AND Gate: output ON when BOTH inputs are powered
// ---------------------------------------------------------
class LFPG_AND_Gate : LFPG_LogicGateBase
{
    override bool LFPG_EvaluateGateLogic(bool in0, bool in1)
    {
        if (in0 && in1)
        {
            return true;
        }
        return false;
    }
};

// ---------------------------------------------------------
// OR Gate: output ON when ANY input is powered
// ---------------------------------------------------------
class LFPG_OR_Gate : LFPG_LogicGateBase
{
    override bool LFPG_EvaluateGateLogic(bool in0, bool in1)
    {
        if (in0 || in1)
        {
            return true;
        }
        return false;
    }
};

// ---------------------------------------------------------
// XOR Gate: output ON when EXACTLY ONE input is powered
// ---------------------------------------------------------
class LFPG_XOR_Gate : LFPG_LogicGateBase
{
    override bool LFPG_EvaluateGateLogic(bool in0, bool in1)
    {
        if (in0 && !in1)
        {
            return true;
        }
        if (!in0 && in1)
        {
            return true;
        }
        return false;
    }
};
