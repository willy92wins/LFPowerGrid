// =========================================================
// LF_PowerGrid - Pressure Pad / Player Sensor (v2.0.0 — Refactor)
//
// LFPG_PressurePad: PASSTHROUGH, 1 IN + 1 OUT. GATE.
//   Physical sensor: gate opens when player stands on pad.
//   Detection runs ALWAYS (even without power).
//   Power only affects whether the graph propagates.
//   Extends LFPG_WireOwnerBase (Refactor v4.1).
//
// NM Registration: RegisterPressurePad / UnregisterPressurePad
//   (centralized detection tick, NOT per-device CallLater)
//
// Persistence: [base: DeviceId + ver + wireJSON] — no extras
// =========================================================

static const float  LFPG_PAD_RADIUS_SQ      = 0.16;
static const float  LFPG_PAD_Y_MIN          = -0.1;
static const float  LFPG_PAD_Y_MAX          = 0.3;
static const float  LFPG_PAD_CONSUMPTION    = 5.0;
static const float  LFPG_PAD_CAPACITY       = 20.0;
static const string LFPG_PAD_PRESS_SOUNDSET  = "LFPG_PressurePad_Press_SoundSet";

// ---------------------------------------------------------
// KIT — unchanged
// ---------------------------------------------------------
class LFPG_PressurePad_Kit : Inventory_Base
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
        AddAction(LFPG_ActionPlacePressurePad);
    }

    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);
        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;
        string tLog = "[PressurePad_Kit] OnPlacementComplete: param=";
        tLog = tLog + position.ToString();
        tLog = tLog + " kitPos=";
        tLog = tLog + GetPosition().ToString();
        LFPG_Util.Info(tLog);
        string spawnType = "LFPG_PressurePad";
        EntityAI pad = GetGame().CreateObjectEx(spawnType, finalPos, ECE_CREATEPHYSICS);
        if (pad)
        {
            pad.SetPosition(finalPos);
            pad.SetOrientation(finalOri);
            pad.Update();
            string deployMsg = "[PressurePad_Kit] Deployed at ";
            deployMsg = deployMsg + finalPos.ToString();
            LFPG_Util.Info(deployMsg);
            GetGame().ObjectDelete(this);
        }
        else
        {
            LFPG_Util.Error("[PressurePad_Kit] Failed to create! Kit preserved.");
            PlayerBase pbFail = PlayerBase.Cast(player);
            if (pbFail)
            {
                string failMsg = "[LFPG] Pressure Pad placement failed. Kit preserved.";
                pbFail.MessageStatus(failMsg);
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// DEVICE: PASSTHROUGH, physical sensor GATE
// ---------------------------------------------------------
class LFPG_PressurePad : LFPG_WireOwnerBase
{
    protected bool m_PoweredNet = false;
    protected bool m_GateOpen   = false;
    protected bool m_Overloaded = false;

    // Client-side edge detection for sound
    protected bool m_PrevGateOpen = false;

    void LFPG_PressurePad()
    {
        string varPowered  = "m_PoweredNet";
        string varGate     = "m_GateOpen";
        string varOverload = "m_Overloaded";
        RegisterNetSyncVariableBool(varPowered);
        RegisterNetSyncVariableBool(varGate);
        RegisterNetSyncVariableBool(varOverload);

        string pIn  = "input_1";
        string pOut = "output_1";
        string lIn  = "Input 1";
        string lOut = "Output 1";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, lIn);
        LFPG_AddPort(pOut, LFPG_PortDir.OUT, lOut);
    }

    // ---- DeviceAPI ----
    override int LFPG_GetDeviceType() { return LFPG_DeviceType.PASSTHROUGH; }
    override bool LFPG_IsSource() { return true; }
    override bool LFPG_GetSourceOn() { return m_PoweredNet; }
    override bool LFPG_IsGateCapable() { return true; }
    override bool LFPG_IsGateOpen() { return m_GateOpen; }
    override float LFPG_GetConsumption() { return LFPG_PAD_CONSUMPTION; }
    override float LFPG_GetCapacity() { return LFPG_PAD_CAPACITY; }
    override bool LFPG_IsPowered() { return m_PoweredNet; }
    override bool LFPG_GetOverloaded() { return m_Overloaded; }

    override void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;
        SetSynchDirty();

        string pwrMsg = "[LFPG_PressurePad] SetPowered(";
        pwrMsg = pwrMsg + powered.ToString();
        pwrMsg = pwrMsg + ") id=";
        pwrMsg = pwrMsg + m_DeviceId;
        LFPG_Util.Debug(pwrMsg);
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

    // ============================================
    // Port world position (p3d uses _0 numbering)
    // ============================================
    override vector LFPG_GetPortWorldPos(string portName)
    {
        string memPoint;
        if (portName == "input_1")
        {
            memPoint = "port_input_0";
        }
        else if (portName == "output_1")
        {
            memPoint = "port_output_0";
        }
        else
        {
            memPoint = "port_";
            memPoint = memPoint + portName;
        }

        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

        vector offset = "0 0.02 0";
        if (portName == "input_1")
        {
            offset = "0 0.02 -0.25";
        }
        else if (portName == "output_1")
        {
            offset = "0 0.02 0.25";
        }

        return ModelToWorld(offset);
    }

    // ============================================
    // Sensor logic (called by NM centralized tick)
    // ============================================
    bool LFPG_EvaluatePresence(array<Man> players)
    {
        #ifdef SERVER
        vector padPos = GetPosition();
        float padX = padPos[0];
        float padY = padPos[1];
        float padZ = padPos[2];

        bool detected = false;

        int i;
        int pCount = players.Count();
        Man man;
        PlayerBase pb;
        vector playerPos;
        float dx;
        float dz;
        float distXZsq;
        float dy;

        for (i = 0; i < pCount; i = i + 1)
        {
            if (detected)
                break;

            man = players[i];
            if (!man)
                continue;

            if (!man.IsAlive())
                continue;

            pb = PlayerBase.Cast(man);
            if (!pb)
                continue;

            playerPos = pb.GetPosition();
            dx = playerPos[0] - padX;
            dz = playerPos[2] - padZ;
            distXZsq = dx * dx + dz * dz;
            if (distXZsq > LFPG_PAD_RADIUS_SQ)
                continue;

            dy = playerPos[1] - padY;
            if (dy < LFPG_PAD_Y_MIN)
                continue;
            if (dy > LFPG_PAD_Y_MAX)
                continue;

            detected = true;
        }

        bool oldGate = m_GateOpen;
        m_GateOpen = detected;

        if (m_GateOpen != oldGate)
        {
            SetSynchDirty();
            return true;
        }

        return false;
        #else
        return false;
        #endif
    }

    // ---- NM registration ----
    override void LFPG_OnInitDevice()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().RegisterPressurePad(this);
        #endif
    }

    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterPressurePad(this);
        bool dirty = false;
        if (m_PoweredNet) { m_PoweredNet = false; dirty = true; }
        if (m_GateOpen) { m_GateOpen = false; dirty = true; }
        if (dirty) { SetSynchDirty(); }
        #endif
    }

    override void LFPG_OnDeleted()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterPressurePad(this);
        #endif
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        bool dirty = false;
        if (m_PoweredNet) { m_PoweredNet = false; dirty = true; }
        if (m_GateOpen) { m_GateOpen = false; dirty = true; }
        if (dirty) { SetSynchDirty(); }
        #endif
    }

    // ---- Visual sync (sound + animation) ----
    override void LFPG_OnVarSyncDevice()
    {
        #ifndef SERVER
        // Gate transition sound (one-shot)
        bool gateJustOpened = false;
        if (m_GateOpen && !m_PrevGateOpen)
        {
            gateJustOpened = true;
        }
        m_PrevGateOpen = m_GateOpen;

        LFPG_UpdateVisuals();

        if (gateJustOpened)
        {
            SEffectManager.PlaySound(LFPG_PAD_PRESS_SOUNDSET, GetPosition());
        }
        #endif
    }

    protected void LFPG_UpdateVisuals()
    {
        #ifndef SERVER
        string phaseName = "pad_press";
        if (m_GateOpen)
        {
            SetAnimationPhase(phaseName, 1.0);
        }
        else
        {
            SetAnimationPhase(phaseName, 0.0);
        }
        #endif
    }

    // ---- No persist extras ----
};
