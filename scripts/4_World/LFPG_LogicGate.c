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

static const string LFPG_GATE_RVMAT_OFF   = "\\LFPowerGrid\\data\\button\\materials\\led_off.rvmat";
static const string LFPG_GATE_RVMAT_GREEN  = "\\LFPowerGrid\\data\\button\\materials\\led_green.rvmat";
static const string LFPG_GATE_RVMAT_RED    = "\\LFPowerGrid\\data\\button\\materials\\led_red.rvmat";
static const float  LFPG_GATE_CAPACITY     = 20.0;

// ---------------------------------------------------------
// KIT — unchanged (includes shared LFPG_GetEntityClass for AND/OR/XOR/MemoryCell)
// ---------------------------------------------------------
class LFPG_LogicGate_Kit : Inventory_Base
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
        AddAction(LFPG_ActionPlaceLogicGate);
    }

    protected string LFPG_GetEntityClass()
    {
        string kitType = GetType();

        if (kitType == "LFPG_AND_Gate_Kit")
        {
            return "LFPG_AND_Gate";
        }

        if (kitType == "LFPG_OR_Gate_Kit")
        {
            return "LFPG_OR_Gate";
        }

        if (kitType == "LFPG_XOR_Gate_Kit")
        {
            return "LFPG_XOR_Gate";
        }

        if (kitType == "LFPG_MemoryCell_Kit")
        {
            return "LFPG_MemoryCell";
        }

        string errMsg = "[LogicGate_Kit] Unknown kit type: ";
        errMsg = errMsg + kitType;
        LFPG_Util.Error(errMsg);
        return "";
    }

    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        string entityClass = LFPG_GetEntityClass();
        if (entityClass == "")
        {
            LFPG_Util.Error("[LogicGate_Kit] Empty entity class — cannot spawn.");
            return;
        }

        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[LogicGate_Kit] OnPlacementComplete: type=";
        tLog = tLog + GetType();
        tLog = tLog + " entity=";
        tLog = tLog + entityClass;
        tLog = tLog + " pos=";
        tLog = tLog + position.ToString();
        LFPG_Util.Info(tLog);

        EntityAI gate = GetGame().CreateObjectEx(entityClass, finalPos, ECE_CREATEPHYSICS);
        if (gate)
        {
            gate.SetPosition(finalPos);
            gate.SetOrientation(finalOri);
            gate.Update();

            string successLog = "[LogicGate_Kit] Deployed ";
            successLog = successLog + entityClass;
            successLog = successLog + " at ";
            successLog = successLog + finalPos.ToString();
            LFPG_Util.Info(successLog);

            GetGame().ObjectDelete(this);
        }
        else
        {
            string failLog = "[LogicGate_Kit] Failed to create ";
            failLog = failLog + entityClass;
            failLog = failLog + "! Kit preserved.";
            LFPG_Util.Error(failLog);

            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                string msg = "[LFPG] Logic gate placement failed. Kit preserved.";
                pb.MessageStatus(msg);
            }
        }
        #endif
    }
};

class LFPG_AND_Gate_Kit : LFPG_LogicGate_Kit {};
class LFPG_OR_Gate_Kit  : LFPG_LogicGate_Kit {};
class LFPG_XOR_Gate_Kit : LFPG_LogicGate_Kit {};

// ---------------------------------------------------------
// DEVICE BASE: PASSTHROUGH, 2 IN + 1 OUT, GATE
// ---------------------------------------------------------
class LFPG_LogicGateBase : LFPG_WireOwnerBase
{
    protected bool m_PoweredNet    = false;
    protected bool m_Input0Powered = false;
    protected bool m_Input1Powered = false;
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
        return "";
    }

    // ---- Gate evaluation via virtual dispatch (replaces GetType() string comparison) ----
    override bool LFPG_IsGateOpen()
    {
        #ifdef SERVER
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (!nm)
            return false;

        string portIn0 = "input_0";
        string portIn1 = "input_1";
        bool in0 = nm.IsPortReceivingPower(m_DeviceId, portIn0);
        bool in1 = nm.IsPortReceivingPower(m_DeviceId, portIn1);

        return LFPG_EvaluateGateLogic(in0, in1);
        #else
        return false;
        #endif
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

        string dbgLog = "[LogicGate] SetPowered(";
        dbgLog = dbgLog + powered.ToString();
        dbgLog = dbgLog + ") in0=";
        dbgLog = dbgLog + newIn0.ToString();
        dbgLog = dbgLog + " in1=";
        dbgLog = dbgLog + newIn1.ToString();
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
    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        bool dirty = false;
        if (m_PoweredNet) { m_PoweredNet = false; dirty = true; }
        if (m_Input0Powered) { m_Input0Powered = false; dirty = true; }
        if (m_Input1Powered) { m_Input1Powered = false; dirty = true; }
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
        // hiddenSelections: 0=symbol, 1=led_input0, 2=led_input1, 3=led_output0

        // Force correct symbol texture (MLOD cache bug)
        string symTex = LFPG_GetSymbolTexturePath();
        if (symTex != "")
        {
            SetObjectTexture(0, symTex);
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
        return "\\LFPowerGrid\\data\\logic_gate\\data\\memory_cell_symbol_and.paa";
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
        return "\\LFPowerGrid\\data\\logic_gate\\data\\memory_cell_symbol_or.paa";
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
        return "\\LFPowerGrid\\data\\logic_gate\\data\\memory_cell_symbol_xor.paa";
    }
};
