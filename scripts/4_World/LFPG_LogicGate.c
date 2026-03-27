// =========================================================
// LF_PowerGrid - Logic Gates (v2.0.0 — Refactor)
//
// LFPG_LogicGateBase: PASSTHROUGH, 2 IN + 1 OUT. GATE.
//   Extends LFPG_WireOwnerBase (Refactor v4.1).
//   Gate logic via virtual LFPG_EvaluateGateLogic(in0, in1).
//
// LFPG_AND_Gate, LFPG_OR_Gate, LFPG_XOR_Gate: override gate logic + symbol texture.
//
// Ports: input_0 (IN), input_1 (IN), output_0 (OUT)
// LEDs: 0=symbol texture, 1=input0, 2=input1, 3=output
// Persistence: [base: DeviceId + ver + wireJSON] — no extras
// =========================================================

static const string LFPG_GATE_RVMAT_OFF   = "\LFPowerGrid\data\button\materials\led_off.rvmat";
static const string LFPG_GATE_RVMAT_GREEN  = "\LFPowerGrid\data\button\materials\led_green.rvmat";
static const string LFPG_GATE_RVMAT_RED    = "\LFPowerGrid\data\button\materials\led_red.rvmat";
static const float  LFPG_GATE_CAPACITY     = 20.0;

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

    // ---- Virtual: symbol texture for shared p3d cache fix ----
    string LFPG_GetSymbolTexturePath()
    {
        string empty = "";
        return empty;
    }

    // ---- Deferred texture fix: engine caches first texture for shared p3d ----
    override void EEInit()
    {
        super.EEInit();
        #ifndef SERVER
        int delay = 100;
        bool repeat = false;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_DeferredSymbolTexture, delay, repeat);
        #endif
    }

    protected void LFPG_DeferredSymbolTexture()
    {
        string tex = LFPG_GetSymbolTexturePath();
        if (tex != "")
        {
            int idxCamo = 0;
            int idxSymbol = 1;
            SetObjectTexture(idxCamo, tex);
            SetObjectTexture(idxSymbol, tex);
        }
    }
};

class LFPG_AND_Gate_Kit : LFPG_LogicGate_Kit
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_AND_Gate";
    }

    override string LFPG_GetSymbolTexturePath()
    {
        string path = "\LFPowerGrid\data\logic_gate\data\memory_cell_symbol_and.paa";
        return path;
    }
};
class LFPG_OR_Gate_Kit : LFPG_LogicGate_Kit
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_OR_Gate";
    }

    override string LFPG_GetSymbolTexturePath()
    {
        string path = "\LFPowerGrid\data\logic_gate\data\memory_cell_symbol_or.paa";
        return path;
    }
};
class LFPG_XOR_Gate_Kit : LFPG_LogicGate_Kit
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_XOR_Gate";
    }

    override string LFPG_GetSymbolTexturePath()
    {
        string path = "\LFPowerGrid\data\logic_gate\data\memory_cell_symbol_xor.paa";
        return path;
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

    // ---- Virtual symbol texture (subclass overrides) ----
    string LFPG_GetSymbolTexturePath()
    {
        string empty = "";
        return empty;
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
        if (m_GateOpen != newGate)
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
        // Force correct symbol texture on client at spawn time.
        // DayZ caches the first texture loaded for a shared p3d,
        // so all gates (AND/OR/XOR/Mem) show the same (OR) symbol
        // unless we explicitly SetObjectTexture on init.
        // OnVariablesSynchronized only fires on SyncVar CHANGE,
        // which never happens on fresh spawn (all defaults = false).
        // Immediate call may be too early (engine overwrites),
        // so also schedule a deferred call after model is loaded.
        LFPG_UpdateVisuals();

        #ifndef SERVER
        int delay = 100;
        bool repeat = false;
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_DeferredSymbolTexture, delay, repeat);
        #endif
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
        // hiddenSelections: 0=camo, 1=led_input0, 2=led_input1, 3=led_output0, 4=camosymbol

        // Force correct symbol texture (MLOD cache bug + proxy lid)
        string symTex = LFPG_GetSymbolTexturePath();
        if (symTex != "")
        {
            int idxCamo = 0;
            int idxSymbol = 4;
            SetObjectTexture(idxCamo, symTex);
            SetObjectTexture(idxSymbol, symTex);
        }

        // Input 0 LED
        if (m_Input0Powered)
        {
            SetObjectMaterial(1, LFPG_GATE_RVMAT_GREEN);
        }
        else
        {
            SetObjectMaterial(1, LFPG_GATE_RVMAT_OFF);
        }

        // Input 1 LED
        if (m_Input1Powered)
        {
            SetObjectMaterial(2, LFPG_GATE_RVMAT_GREEN);
        }
        else
        {
            SetObjectMaterial(2, LFPG_GATE_RVMAT_OFF);
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
            SetObjectMaterial(3, LFPG_GATE_RVMAT_GREEN);
        }
        else if (hasAnyInput)
        {
            SetObjectMaterial(3, LFPG_GATE_RVMAT_RED);
        }
        else
        {
            SetObjectMaterial(3, LFPG_GATE_RVMAT_OFF);
        }
        #endif
    }

    protected void LFPG_DeferredSymbolTexture()
    {
        #ifndef SERVER
        string tex = LFPG_GetSymbolTexturePath();
        if (tex != "")
        {
            int idxCamo = 0;
            int idxSymbol = 4;
            SetObjectTexture(idxCamo, tex);
            SetObjectTexture(idxSymbol, tex);
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

    override string LFPG_GetSymbolTexturePath()
    {
        string path = "\LFPowerGrid\data\logic_gate\data\memory_cell_symbol_and.paa";
        return path;
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

    override string LFPG_GetSymbolTexturePath()
    {
        string path = "\LFPowerGrid\data\logic_gate\data\memory_cell_symbol_or.paa";
        return path;
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

    override string LFPG_GetSymbolTexturePath()
    {
        string path = "\LFPowerGrid\data\logic_gate\data\memory_cell_symbol_xor.paa";
        return path;
    }
};
