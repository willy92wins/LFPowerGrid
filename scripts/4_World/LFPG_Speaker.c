// =========================================================
// LF_PowerGrid - Speaker (v1.0.0)
//
// LFPG_Speaker_Kit: Holdable, deployable (same-model, floor+wall).
// LFPG_Speaker:     CONSUMER, 1 IN port (input_1).
//                   5 u/s consumption.
//                   Spawns GhostPASReceiver when powered + on.
//                   Receives PAS megaphone audio from any
//                   PASBroadcaster (vanilla or LFPG_Intercom).
//
// Knob: 2 positions (off=0.0, on=0.25 phase).
//   Future expansion: 0.0/0.25/0.5/0.75/1.0 for volume levels.
//
// LED states (light_led, hiddenSelection index 1):
//   - Green: powered + on (receiving audio)
//   - Red:   powered + off (muted at 0%)
//   - Off:   unpowered
//
// Model: speaker.p3d
//   Memory points: port_input_0, port_input_0_dir
//   Animations: knob (rotation, user source)
//   Hidden selections: knob(0), light_led(1)
//   Note: port_input_0 maps to LFPG port "input_1"
//
// Lifecycle: DeviceBase v4.0 hooks (LFPG_OnInit, LFPG_OnKilled,
//   LFPG_OnDeleted, LFPG_OnVarSync, LFPG_OnStoreSaveExtra/LoadExtra).
//
// Persistence: DeviceIdLow/High + ver (base) + m_SpeakerOn (bool).
//   m_PoweredNet is derived state (NOT persisted).
//
// Enforce Script rules: no ternary, no ++/--, no foreach,
//   no inline concat, explicit typing, m_ prefix on members.
// =========================================================

// ---------------------------------------------------------
// RVMAT paths for LED states
// ---------------------------------------------------------
static const string LFPG_SPEAKER_RVMAT_LED_OFF   = "\LFPowerGrid\data\speaker\data\speaker_led_off.rvmat";
static const string LFPG_SPEAKER_RVMAT_LED_GREEN  = "\LFPowerGrid\data\speaker\data\speaker_green.rvmat";
static const string LFPG_SPEAKER_RVMAT_LED_RED    = "\LFPowerGrid\data\speaker\data\speaker_red.rvmat";

// ---------------------------------------------------------
// Constants
// ---------------------------------------------------------
static const float LFPG_SPEAKER_CONSUMPTION = 5.0;

static const int LFPG_SPEAKER_HS_KNOB = 0;
static const int LFPG_SPEAKER_HS_LED  = 1;

static const float LFPG_SPEAKER_KNOB_OFF = 0.0;
static const float LFPG_SPEAKER_KNOB_ON  = 0.25;

// =========================================================
// KIT — same-model, floor + wall placement
// =========================================================
class LFPG_Speaker_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_Speaker";
    }

    override int LFPG_GetPlacementModes()
    {
        return 1;
    }
};

// =========================================================
// DEVICE — CONSUMER : LFPG_DeviceBase
// 1 IN (input_1), no output, no wire store
// =========================================================
class LFPG_Speaker : LFPG_DeviceBase
{
    // ---- Device-specific SyncVars ----
    protected bool m_PoweredNet  = false;
    protected bool m_SpeakerOn   = false;
    protected bool m_Overloaded  = false;

    // ---- Ghost PAS receiver ----
    protected LFPG_GhostPASReceiver m_GhostPAS;

    // ============================================
    // Constructor — port + SyncVars
    // ============================================
    void LFPG_Speaker()
    {
        string pIn1 = "input_1";
        string lIn1 = "Power In";
        LFPG_AddPort(pIn1, LFPG_PortDir.IN, lIn1);

        string varPowered    = "m_PoweredNet";
        string varSpeakerOn  = "m_SpeakerOn";
        string varOverloaded = "m_Overloaded";

        RegisterNetSyncVariableBool(varPowered);
        RegisterNetSyncVariableBool(varSpeakerOn);
        RegisterNetSyncVariableBool(varOverloaded);
    }

    // ============================================
    // SetActions
    // ============================================
    override void SetActions()
    {
        super.SetActions();
        AddAction(LFPG_ActionSpeakerOn);
        AddAction(LFPG_ActionSpeakerOff);
    }

    // ============================================
    // Port world pos override
    // p3d: port_input_0 -> LFPG port: input_1
    // ============================================
    override vector LFPG_GetPortWorldPos(string portName)
    {
        string memPoint = "port_input_0";

        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

        return GetPosition();
    }

    // ============================================
    // Virtual interface
    // ============================================
    override int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.CONSUMER;
    }

    override float LFPG_GetConsumption()
    {
        return LFPG_SPEAKER_CONSUMPTION;
    }

    override bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    override void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;
        SetSynchDirty();

        LFPG_UpdateGhostPAS();

        string pwrMsg = "[LFPG_Speaker] SetPowered(";
        pwrMsg = pwrMsg + powered.ToString();
        pwrMsg = pwrMsg + ") id=";
        pwrMsg = pwrMsg + m_DeviceId;
        LFPG_Util.Debug(pwrMsg);
        #endif
    }

    override bool LFPG_GetOverloaded()
    {
        return m_Overloaded;
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
    // Toggle speaker on/off
    // ============================================
    void LFPG_ToggleSpeaker(bool turnOn)
    {
        #ifdef SERVER
        if (m_SpeakerOn == turnOn)
            return;

        m_SpeakerOn = turnOn;
        SetSynchDirty();

        LFPG_UpdateGhostPAS();

        string togMsg = "[LFPG_Speaker] Toggle ";
        if (m_SpeakerOn)
        {
            togMsg = togMsg + "ON";
        }
        else
        {
            togMsg = togMsg + "OFF";
        }
        togMsg = togMsg + " id=";
        togMsg = togMsg + m_DeviceId;
        LFPG_Util.Info(togMsg);
        #endif
    }

    // ============================================
    // State accessor
    // ============================================
    bool LFPG_GetSpeakerOn()
    {
        return m_SpeakerOn;
    }

    // ============================================
    // Ghost PAS Receiver lifecycle
    // ============================================
    protected void LFPG_SpawnGhostPAS()
    {
        #ifdef SERVER
        if (m_GhostPAS)
            return;

        vector pos = GetPosition();
        string ghostClass = "LFPG_GhostPASReceiver";
        Object ghostObj = GetGame().CreateObjectEx(ghostClass, pos, ECE_CREATEPHYSICS);
        m_GhostPAS = LFPG_GhostPASReceiver.Cast(ghostObj);
        if (!m_GhostPAS)
        {
            string errMsg = "[LFPG_Speaker] Failed to spawn GhostPASReceiver at ";
            errMsg = errMsg + pos.ToString();
            LFPG_Util.Error(errMsg);
            return;
        }

        string spawnMsg = "[LFPG_Speaker] GhostPASReceiver spawned at ";
        spawnMsg = spawnMsg + pos.ToString();
        spawnMsg = spawnMsg + " id=";
        spawnMsg = spawnMsg + m_DeviceId;
        LFPG_Util.Info(spawnMsg);
        #endif
    }

    protected void LFPG_DestroyGhostPAS()
    {
        #ifdef SERVER
        if (m_GhostPAS)
        {
            GetGame().ObjectDelete(m_GhostPAS);
            m_GhostPAS = null;

            string destroyMsg = "[LFPG_Speaker] GhostPASReceiver destroyed, id=";
            destroyMsg = destroyMsg + m_DeviceId;
            LFPG_Util.Info(destroyMsg);
        }
        #endif
    }

    protected void LFPG_UpdateGhostPAS()
    {
        #ifdef SERVER
        bool shouldExist = false;
        if (m_PoweredNet && m_SpeakerOn)
        {
            shouldExist = true;
        }

        if (shouldExist && !m_GhostPAS)
        {
            LFPG_SpawnGhostPAS();
        }
        else if (!shouldExist && m_GhostPAS)
        {
            LFPG_DestroyGhostPAS();
        }
        #endif
    }

    // ============================================
    // DeviceBase v4.0 Lifecycle Hooks
    // ============================================
    override void LFPG_OnInit()
    {
        LFPG_UpdateVisuals();
    }

    override void LFPG_OnKilled()
    {
        LFPG_DestroyGhostPAS();

        #ifdef SERVER
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        #endif
    }

    override void LFPG_OnDeleted()
    {
        LFPG_DestroyGhostPAS();
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
            LFPG_UpdateGhostPAS();
        }
        #endif
    }

    override void LFPG_OnVarSync()
    {
        #ifndef SERVER
        LFPG_UpdateVisuals();
        #endif
    }

    // ============================================
    // Visual update (client only)
    // ============================================
    protected void LFPG_UpdateVisuals()
    {
        #ifndef SERVER
        // LED state
        if (m_PoweredNet && m_SpeakerOn)
        {
            SetObjectMaterial(LFPG_SPEAKER_HS_LED, LFPG_SPEAKER_RVMAT_LED_GREEN);
        }
        else if (m_PoweredNet)
        {
            SetObjectMaterial(LFPG_SPEAKER_HS_LED, LFPG_SPEAKER_RVMAT_LED_RED);
        }
        else
        {
            SetObjectMaterial(LFPG_SPEAKER_HS_LED, LFPG_SPEAKER_RVMAT_LED_OFF);
        }

        // Knob animation phase
        string animKnob = "knob";
        if (m_SpeakerOn)
        {
            SetAnimationPhase(animKnob, LFPG_SPEAKER_KNOB_ON);
        }
        else
        {
            SetAnimationPhase(animKnob, LFPG_SPEAKER_KNOB_OFF);
        }
        #endif
    }

    // ============================================
    // Persistence (DeviceBase v4.0 hooks)
    // ============================================
    override void LFPG_OnStoreSaveExtra(ParamsWriteContext ctx)
    {
        ctx.Write(m_SpeakerOn);

        string saveMsg = "[LFPG_Speaker] Saved id=";
        saveMsg = saveMsg + m_DeviceId;
        saveMsg = saveMsg + " speakerOn=";
        saveMsg = saveMsg + m_SpeakerOn.ToString();
        LFPG_Util.Info(saveMsg);
    }

    override bool LFPG_OnStoreLoadExtra(ParamsReadContext ctx, int ver)
    {
        if (!ctx.Read(m_SpeakerOn))
        {
            string errMsg = "[LFPG_Speaker] OnStoreLoad failed: m_SpeakerOn";
            LFPG_Util.Error(errMsg);
            return false;
        }

        string loadMsg = "[LFPG_Speaker] Loaded id=";
        loadMsg = loadMsg + m_DeviceId;
        loadMsg = loadMsg + " speakerOn=";
        loadMsg = loadMsg + m_SpeakerOn.ToString();
        LFPG_Util.Info(loadMsg);

        return true;
    }
};
