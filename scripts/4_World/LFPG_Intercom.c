// =========================================================
// LF_PowerGrid - Intercom / RF Broadcaster (v4.1 PAS)
//
// LFPG_Intercom_Kit: Holdable, deployable (same-model pattern).
// LFPG_Intercom:     CONSUMER, 2 IN ports (input_1 + input_toggle).
//                   No output, no wire store.
//                   T1: 10 u/s, gate toggle on/off, RF toggle.
//                   T2: 20 u/s, ghost radio bidirectional VOIP
//                       + ghost PAS broadcaster (megaphone).
//
// v4.1: Added GhostPASBroadcaster alongside GhostRadio.
//   When T2 broadcast is ON, both ghosts spawn simultaneously:
//   - GhostRadio: VOIP by frequency (PersonalRadio system)
//   - GhostPASBroadcaster: megaphone to all PASReceivers
//   Same conditions: m_RadioInstalled && m_PoweredNet &&
//                    m_SwitchOn && m_BroadcastEnabled.
//   No persistence change — ghosts re-created at runtime.
//
// v4.0: Migrated from Inventory_Base to LFPG_DeviceBase.
//   All boilerplate (SyncVars DeviceId, lifecycle, persistence,
//   guards, CompEM block) now in DeviceBase.
//   Intercom declares: ports, SyncVars, RF, ghost radio, T2, gate.
//   GetPortWorldPos override required: p3d uses port_input_0/port_output_0
//   but LFPG ports are input_1/input_toggle (non-standard mapping).
//
// Model: rf_broadcaster.p3d
//   Memory points: port_input_0 (input_1), port_output_0 (input_toggle)
//   Animations: knob_freq, knob_input, knob_vol
//   Hidden selections: camo(0), camoscreen(1), light_led(2),
//                       light_led2(3), microphone(4)
// =========================================================

// ---------------------------------------------------------
// RVMAT paths for LED states
// ---------------------------------------------------------
static const string LFPG_INTERCOM_RVMAT_OFF    = "\LFPowerGrid\data\rf_broadcaster\data\led_off.rvmat";
static const string LFPG_INTERCOM_RVMAT_GREEN  = "\LFPowerGrid\data\rf_broadcaster\data\rf_broadcaster_green.rvmat";
static const string LFPG_INTERCOM_RVMAT_RED    = "\LFPowerGrid\data\rf_broadcaster\data\rf_broadcaster_red.rvmat";
static const string LFPG_INTERCOM_RVMAT_BLUE   = "\LFPowerGrid\data\rf_broadcaster\data\rf_broadcaster_blue.rvmat";

static const string LFPG_INTERCOM_MIC_TEX      = "\LFPowerGrid\data\rf_broadcaster\data\rf_broadcaster_microphone_co.paa";
static const string LFPG_INTERCOM_MIC_RVMAT    = "\LFPowerGrid\data\rf_broadcaster\data\rf_broadcaster.rvmat";

// ---------------------------------------------------------
// Constants
// ---------------------------------------------------------
static const float LFPG_INTERCOM_CONSUMPTION_T1 = 10.0;
static const float LFPG_INTERCOM_CONSUMPTION_T2 = 20.0;

static const int LFPG_INTERCOM_HS_CAMO       = 0;
static const int LFPG_INTERCOM_HS_SCREEN     = 1;
static const int LFPG_INTERCOM_HS_LED1       = 2;
static const int LFPG_INTERCOM_HS_LED2       = 3;
static const int LFPG_INTERCOM_HS_MIC        = 4;

static const string LFPG_INTERCOM_SND_RF_BEEP      = "LFPG_Intercom_RFBeep_SoundSet";
static const string LFPG_INTERCOM_SND_KNOB_CLICK   = "LFPG_Intercom_KnobClick_SoundSet";
static const string LFPG_INTERCOM_SND_STATIC_BURST  = "LFPG_Intercom_Static_SoundSet";

class LFPG_Intercom_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_Intercom";
    }

    override int LFPG_GetPlacementModes()
    {
        return 1;
    }
};

// ---------------------------------------------------------
// DEVICE — CONSUMER : LFPG_DeviceBase
// 2 IN (input_1 + input_toggle), no output, no wire store
// ---------------------------------------------------------
class LFPG_Intercom : LFPG_DeviceBase
{
    // ---- Device-specific SyncVars ----
    protected bool m_PoweredNet      = false;
    protected bool m_SwitchOn        = false;
    protected bool m_RadioInstalled  = false;
    protected bool m_BroadcastEnabled = false;
    protected int  m_FrequencyIndex  = 0;
    protected bool m_Overloaded      = false;

    // ---- RF toggle state (not SyncVars) ----
    protected bool m_PrevToggleInput   = false;
    protected int  m_LastRFToggleTime  = 0;

    // ---- Ghost radio (T2 bidirectional VOIP) ----
    protected LFPG_GhostRadio m_GhostRadio;

    // ---- Ghost PAS broadcaster (T2 megaphone) ----
    protected LFPG_GhostPASBroadcaster m_GhostPAS;

    // ============================================
    // Constructor — ports + SyncVars
    // ============================================
    void LFPG_Intercom()
    {
        string pIn1 = "input_1";
        string lIn1 = "Power In";
        LFPG_AddPort(pIn1, LFPG_PortDir.IN, lIn1);

        string pTog = "input_toggle";
        string lTog = "Toggle Signal";
        LFPG_AddPort(pTog, LFPG_PortDir.IN, lTog);

        string varPowered    = "m_PoweredNet";
        string varSwitchOn   = "m_SwitchOn";
        string varRadio      = "m_RadioInstalled";
        string varBroadcast  = "m_BroadcastEnabled";
        string varFreq       = "m_FrequencyIndex";
        string varOverloaded = "m_Overloaded";

        RegisterNetSyncVariableBool(varPowered);
        RegisterNetSyncVariableBool(varSwitchOn);
        RegisterNetSyncVariableBool(varRadio);
        RegisterNetSyncVariableBool(varBroadcast);
        RegisterNetSyncVariableInt(varFreq);
        RegisterNetSyncVariableBool(varOverloaded);
    }

    // ============================================
    // SetActions — DeviceBase already removes TakeItem/TakeItemToHands
    // ============================================
    override void SetActions()
    {
        super.SetActions();
        AddAction(LFPG_ActionToggleIntercom);
        AddAction(LFPG_ActionRFToggle);
        AddAction(LFPG_ActionToggleBroadcast);
        AddAction(LFPG_ActionCycleFrequency);
    }

    // ============================================
    // Attachment slot control (radio slot)
    // ============================================
    override bool CanReceiveAttachment(EntityAI attachment, int slotId)
    {
        if (!attachment)
            return false;

        string typeName = attachment.GetType();
        string ghostType = "LFPG_GhostRadio";
        if (typeName == ghostType)
            return false;

        string kindRadio = "PersonalRadio";
        if (!attachment.IsKindOf(kindRadio))
            return false;

        return super.CanReceiveAttachment(attachment, slotId);
    }

    override bool CanReleaseAttachment(EntityAI attachment)
    {
        if (!attachment)
            return false;

        if (m_RadioInstalled)
        {
            string kindRadio = "PersonalRadio";
            if (attachment.IsKindOf(kindRadio))
                return false;
        }

        return super.CanReleaseAttachment(attachment);
    }

    // ============================================
    // Port world pos override — non-standard p3d mapping
    // input_1 → port_input_0, input_toggle → port_output_0
    // ============================================
    override vector LFPG_GetPortWorldPos(string portName)
    {
        string memPoint;
        string pInput1 = "input_1";
        string pToggle = "input_toggle";

        if (portName == pInput1)
        {
            memPoint = "port_input_0";
        }
        else if (portName == pToggle)
        {
            memPoint = "port_output_0";
        }
        else
        {
            memPoint = "port_input_0";
        }

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
        if (m_RadioInstalled)
        {
            return LFPG_INTERCOM_CONSUMPTION_T2;
        }
        return LFPG_INTERCOM_CONSUMPTION_T1;
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

        if (!powered)
        {
            m_PrevToggleInput = false;
        }

        LFPG_UpdateGhostRadio();
        LFPG_UpdateGhostPAS();

        string pwrMsg = "[LFPG_Intercom] SetPowered(";
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

    override bool LFPG_IsGateCapable()
    {
        return true;
    }

    override bool LFPG_IsGateOpen()
    {
        return m_SwitchOn;
    }

    // ============================================
    // Lifecycle hooks
    // ============================================
    override void LFPG_OnInit()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().RegisterIntercom(this);
        #endif

        LFPG_UpdateVisuals();
    }

    override void LFPG_OnKilled()
    {
        LFPG_DestroyGhostRadio();
        LFPG_DestroyGhostPAS();

        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterIntercom(this);

        bool dirty = false;
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            dirty = true;
        }
        if (m_SwitchOn)
        {
            m_SwitchOn = false;
            dirty = true;
        }
        if (dirty)
        {
            SetSynchDirty();
        }
        #endif
    }

    override void LFPG_OnDeleted()
    {
        LFPG_DestroyGhostRadio();
        LFPG_DestroyGhostPAS();

        LFPG_NetworkManager.Get().UnregisterIntercom(this);
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        bool locDirty = false;
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            locDirty = true;
        }
        if (m_SwitchOn)
        {
            m_SwitchOn = false;
            locDirty = true;
        }
        if (locDirty)
        {
            SetSynchDirty();
            LFPG_UpdateGhostRadio();
            LFPG_UpdateGhostPAS();
        }
        #endif
    }

    // ============================================
    // VarSync: LED + animation + microphone
    // ============================================
    override void LFPG_OnVarSync()
    {
        #ifndef SERVER
        LFPG_UpdateVisuals();
        #endif
    }

    // ============================================
    // Persistence
    // ============================================
    override void LFPG_OnStoreSaveExtra(ParamsWriteContext ctx)
    {
        ctx.Write(m_SwitchOn);
        ctx.Write(m_RadioInstalled);
        ctx.Write(m_BroadcastEnabled);
        ctx.Write(m_FrequencyIndex);
    }

    override bool LFPG_OnStoreLoadExtra(ParamsReadContext ctx, int ver)
    {
        if (!ctx.Read(m_SwitchOn))
        {
            string errSwitch = "[LFPG_Intercom] OnStoreLoad failed: m_SwitchOn";
            LFPG_Util.Error(errSwitch);
            return false;
        }

        if (!ctx.Read(m_RadioInstalled))
        {
            string errRadio = "[LFPG_Intercom] OnStoreLoad failed: m_RadioInstalled";
            LFPG_Util.Error(errRadio);
            return false;
        }

        if (!ctx.Read(m_BroadcastEnabled))
        {
            string errBcast = "[LFPG_Intercom] OnStoreLoad failed: m_BroadcastEnabled";
            LFPG_Util.Error(errBcast);
            return false;
        }

        if (!ctx.Read(m_FrequencyIndex))
        {
            string errFreq = "[LFPG_Intercom] OnStoreLoad failed: m_FrequencyIndex";
            LFPG_Util.Error(errFreq);
            return false;
        }

        string loadMsg = "[LFPG_Intercom] Loaded id=";
        loadMsg = loadMsg + m_DeviceId;
        loadMsg = loadMsg + " switchOn=";
        loadMsg = loadMsg + m_SwitchOn.ToString();
        loadMsg = loadMsg + " radioInstalled=";
        loadMsg = loadMsg + m_RadioInstalled.ToString();
        loadMsg = loadMsg + " freq=";
        loadMsg = loadMsg + m_FrequencyIndex.ToString();
        LFPG_Util.Info(loadMsg);

        return true;
    }

    // ============================================
    // AfterStoreLoad — ghost radio re-creation is deferred
    // ============================================
    override void AfterStoreLoad()
    {
        super.AfterStoreLoad();

        #ifdef SERVER
        string afterMsg = "[LFPG_Intercom] AfterStoreLoad id=";
        afterMsg = afterMsg + m_DeviceId;
        afterMsg = afterMsg + " radio=";
        afterMsg = afterMsg + m_RadioInstalled.ToString();
        afterMsg = afterMsg + " broadcast=";
        afterMsg = afterMsg + m_BroadcastEnabled.ToString();
        LFPG_Util.Debug(afterMsg);
        #endif
    }

    // ============================================
    // Visual update (client only)
    // ============================================
    protected void LFPG_UpdateVisuals()
    {
        #ifndef SERVER
        // LED1 (light_led, index 2): Power/Gate state
        if (m_PoweredNet && m_SwitchOn)
        {
            SetObjectMaterial(LFPG_INTERCOM_HS_LED1, LFPG_INTERCOM_RVMAT_GREEN);
        }
        else if (m_PoweredNet)
        {
            SetObjectMaterial(LFPG_INTERCOM_HS_LED1, LFPG_INTERCOM_RVMAT_RED);
        }
        else
        {
            SetObjectMaterial(LFPG_INTERCOM_HS_LED1, LFPG_INTERCOM_RVMAT_OFF);
        }

        // LED2 (light_led2, index 3): T2 broadcast state
        if (m_RadioInstalled && m_BroadcastEnabled && m_PoweredNet)
        {
            SetObjectMaterial(LFPG_INTERCOM_HS_LED2, LFPG_INTERCOM_RVMAT_BLUE);
        }
        else
        {
            SetObjectMaterial(LFPG_INTERCOM_HS_LED2, LFPG_INTERCOM_RVMAT_OFF);
        }

        // Knob_input: 0.0 = off, 1.0 = on
        if (m_SwitchOn)
        {
            string animOn = "knob_input";
            SetAnimationPhase(animOn, 1.0);
        }
        else
        {
            string animOff = "knob_input";
            SetAnimationPhase(animOff, 0.0);
        }

        // Knob_freq: frequency dial position (0.0 to 1.0)
        float freqPhase = 0.0;
        if (m_FrequencyIndex > 0)
        {
            freqPhase = m_FrequencyIndex / 6.0;
        }
        string animFreq = "knob_freq";
        SetAnimationPhase(animFreq, freqPhase);

        // Microphone visibility: show if radio installed
        if (m_RadioInstalled)
        {
            SetObjectTexture(LFPG_INTERCOM_HS_MIC, LFPG_INTERCOM_MIC_TEX);
            SetObjectMaterial(LFPG_INTERCOM_HS_MIC, LFPG_INTERCOM_MIC_RVMAT);
        }
        else
        {
            string emptyTex = "";
            SetObjectTexture(LFPG_INTERCOM_HS_MIC, emptyTex);
            string emptyMat = "";
            SetObjectMaterial(LFPG_INTERCOM_HS_MIC, emptyMat);
        }
        #endif
    }

    // ============================================
    // Toggle logic (called by action, server only)
    // ============================================
    void LFPG_ToggleIntercom()
    {
        #ifdef SERVER
        if (m_SwitchOn)
        {
            m_SwitchOn = false;
        }
        else
        {
            m_SwitchOn = true;
        }
        SetSynchDirty();

        string togMsg = "[LFPG_Intercom] Toggle ";
        if (m_SwitchOn)
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

        LFPG_UpdateGhostRadio();
        LFPG_UpdateGhostPAS();

        LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);

        SEffectManager.PlaySound(LFPG_INTERCOM_SND_KNOB_CLICK, GetPosition());
        #endif
    }

    // ============================================
    // State accessors
    // ============================================
    bool LFPG_GetSwitchOn()
    {
        return m_SwitchOn;
    }

    bool LFPG_GetRadioInstalled()
    {
        return m_RadioInstalled;
    }

    bool LFPG_GetBroadcastEnabled()
    {
        return m_BroadcastEnabled;
    }

    int LFPG_GetFrequencyIndex()
    {
        return m_FrequencyIndex;
    }

    int LFPG_GetLastRFToggleTime()
    {
        return m_LastRFToggleTime;
    }

    // ============================================
    // RF Toggle
    // ============================================
    void LFPG_ExecuteRFToggle()
    {
        #ifdef SERVER
        int now = GetGame().GetTime();
        int elapsed = now - m_LastRFToggleTime;
        if (elapsed < LFPG_INTERCOM_RF_COOLDOWN_MS)
            return;

        m_LastRFToggleTime = now;

        array<EntityAI> allDevices = new array<EntityAI>;
        LFPG_DeviceRegistry.Get().GetAll(allDevices);

        vector myPos = GetPosition();
        int i;
        int count = allDevices.Count();
        int toggled = 0;

        for (i = 0; i < count; i = i + 1)
        {
            EntityAI device = allDevices[i];
            if (!device)
                continue;

            if (device == this)
                continue;

            float distSq = LFPG_WorldUtil.DistSq(myPos, device.GetPosition());
            if (distSq > LFPG_INTERCOM_RF_RANGE_SQ)
                continue;

            if (!LFPG_DeviceAPI.IsRFCapable(device))
                continue;

            bool result = LFPG_DeviceAPI.RemoteToggle(device);
            if (result)
            {
                toggled = toggled + 1;
            }
        }

        string rfMsg = "[LFPG_Intercom] RF toggle executed: ";
        rfMsg = rfMsg + toggled.ToString();
        rfMsg = rfMsg + " devices toggled, id=";
        rfMsg = rfMsg + m_DeviceId;
        LFPG_Util.Info(rfMsg);

        SEffectManager.PlaySound(LFPG_INTERCOM_SND_RF_BEEP, GetPosition());
        #endif
    }

    void LFPG_EvaluateToggleInput()
    {
        #ifdef SERVER
        if (!m_PoweredNet || !m_SwitchOn)
        {
            m_PrevToggleInput = false;
            return;
        }

        float togglePower = 0.0;
        LFPG_ElecGraph graph = LFPG_NetworkManager.Get().GetGraph();
        if (graph)
        {
            array<ref LFPG_ElecEdge> inEdges = graph.GetIncoming(m_DeviceId);
            if (inEdges)
            {
                int i;
                int edgeCount = inEdges.Count();
                for (i = 0; i < edgeCount; i = i + 1)
                {
                    LFPG_ElecEdge edge = inEdges[i];
                    if (!edge)
                        continue;

                    string targetPort = edge.m_TargetPort;
                    string togglePort = "input_toggle";
                    if (targetPort == togglePort)
                    {
                        togglePower = togglePower + edge.m_AllocatedPower;
                    }
                }
            }
        }

        bool currentInput = false;
        if (togglePower >= LFPG_INTERCOM_TOGGLE_INPUT_MIN)
        {
            currentInput = true;
        }

        if (currentInput && !m_PrevToggleInput)
        {
            string edgeMsg = "[LFPG_Intercom] Toggle input rising edge detected, id=";
            edgeMsg = edgeMsg + m_DeviceId;
            LFPG_Util.Info(edgeMsg);
            LFPG_ExecuteRFToggle();
        }

        m_PrevToggleInput = currentInput;
        #endif
    }

    // ============================================
    // T2: Ghost Radio Lifecycle
    // ============================================
    protected void LFPG_SpawnGhostRadio()
    {
        #ifdef SERVER
        if (m_GhostRadio)
            return;

        vector pos = GetPosition();
        string ghostClass = "LFPG_GhostRadio";
        Object ghostObj = GetGame().CreateObjectEx(ghostClass, pos, ECE_CREATEPHYSICS);
        m_GhostRadio = LFPG_GhostRadio.Cast(ghostObj);
        if (!m_GhostRadio)
        {
            string errGhost = "[LFPG_Intercom] Failed to spawn GhostRadio at ";
            errGhost = errGhost + pos.ToString();
            LFPG_Util.Error(errGhost);
            return;
        }

        ComponentEnergyManager ghostCEM = m_GhostRadio.GetCompEM();
        if (ghostCEM)
        {
            ghostCEM.SetEnergy(9999);
            ghostCEM.SwitchOn();
        }
        m_GhostRadio.EnableBroadcast(true);
        m_GhostRadio.EnableReceive(true);
        m_GhostRadio.SetFrequencyByIndex(m_FrequencyIndex);
        m_GhostRadio.SetSynchDirty();

        string spawnMsg = "[LFPG_Intercom] GhostRadio spawned at ";
        spawnMsg = spawnMsg + pos.ToString();
        spawnMsg = spawnMsg + " freq=";
        spawnMsg = spawnMsg + m_FrequencyIndex.ToString();
        spawnMsg = spawnMsg + " id=";
        spawnMsg = spawnMsg + m_DeviceId;
        LFPG_Util.Info(spawnMsg);
        #endif
    }

    protected void LFPG_DestroyGhostRadio()
    {
        #ifdef SERVER
        if (m_GhostRadio)
        {
            GetGame().ObjectDelete(m_GhostRadio);
            m_GhostRadio = null;

            string destroyMsg = "[LFPG_Intercom] GhostRadio destroyed, id=";
            destroyMsg = destroyMsg + m_DeviceId;
            LFPG_Util.Info(destroyMsg);
        }
        #endif
    }

    protected void LFPG_UpdateGhostRadio()
    {
        #ifdef SERVER
        bool shouldExist = false;
        if (m_RadioInstalled && m_PoweredNet && m_SwitchOn && m_BroadcastEnabled)
        {
            shouldExist = true;
        }

        if (shouldExist && !m_GhostRadio)
        {
            LFPG_SpawnGhostRadio();
        }
        else if (!shouldExist && m_GhostRadio)
        {
            LFPG_DestroyGhostRadio();
        }
        #endif
    }

    // ============================================
    // T2: Ghost PAS Broadcaster Lifecycle
    // ============================================
    protected void LFPG_SpawnGhostPAS()
    {
        #ifdef SERVER
        if (m_GhostPAS)
            return;

        vector pasPos = GetPosition();
        string pasClass = "LFPG_GhostPASBroadcaster";
        Object pasObj = GetGame().CreateObjectEx(pasClass, pasPos, ECE_CREATEPHYSICS);
        m_GhostPAS = LFPG_GhostPASBroadcaster.Cast(pasObj);
        if (!m_GhostPAS)
        {
            string errPas = "[LFPG_Intercom] Failed to spawn GhostPASBroadcaster at ";
            errPas = errPas + pasPos.ToString();
            LFPG_Util.Error(errPas);
            return;
        }

        ComponentEnergyManager pasCEM = m_GhostPAS.GetCompEM();
        if (pasCEM)
        {
            pasCEM.SetEnergy(9999);
            pasCEM.SwitchOn();
        }

        string pasSpawnMsg = "[LFPG_Intercom] GhostPASBroadcaster spawned at ";
        pasSpawnMsg = pasSpawnMsg + pasPos.ToString();
        pasSpawnMsg = pasSpawnMsg + " id=";
        pasSpawnMsg = pasSpawnMsg + m_DeviceId;
        LFPG_Util.Info(pasSpawnMsg);
        #endif
    }

    protected void LFPG_DestroyGhostPAS()
    {
        #ifdef SERVER
        if (m_GhostPAS)
        {
            GetGame().ObjectDelete(m_GhostPAS);
            m_GhostPAS = null;

            string pasDestroyMsg = "[LFPG_Intercom] GhostPASBroadcaster destroyed, id=";
            pasDestroyMsg = pasDestroyMsg + m_DeviceId;
            LFPG_Util.Info(pasDestroyMsg);
        }
        #endif
    }

    protected void LFPG_UpdateGhostPAS()
    {
        #ifdef SERVER
        bool pasShouldExist = false;
        if (m_RadioInstalled && m_PoweredNet && m_SwitchOn && m_BroadcastEnabled)
        {
            pasShouldExist = true;
        }

        if (pasShouldExist && !m_GhostPAS)
        {
            LFPG_SpawnGhostPAS();
        }
        else if (!pasShouldExist && m_GhostPAS)
        {
            LFPG_DestroyGhostPAS();
        }
        #endif
    }

    // ============================================
    // T2: Install Radio
    // ============================================
    void LFPG_InstallRadio()
    {
        #ifdef SERVER
        m_RadioInstalled = true;
        m_FrequencyIndex = 0;
        SetSynchDirty();

        LFPG_UpdateGhostRadio();
        LFPG_UpdateGhostPAS();

        string installMsg = "[LFPG_Intercom] Radio installed, id=";
        installMsg = installMsg + m_DeviceId;
        LFPG_Util.Info(installMsg);
        #endif
    }

    // ============================================
    // T2: Toggle Broadcast
    // ============================================
    void LFPG_ToggleBroadcast()
    {
        #ifdef SERVER
        if (m_BroadcastEnabled)
        {
            m_BroadcastEnabled = false;
        }
        else
        {
            m_BroadcastEnabled = true;
        }
        SetSynchDirty();

        LFPG_UpdateGhostRadio();
        LFPG_UpdateGhostPAS();

        string bcMsg = "[LFPG_Intercom] Broadcast ";
        if (m_BroadcastEnabled)
        {
            bcMsg = bcMsg + "ENABLED";
        }
        else
        {
            bcMsg = bcMsg + "DISABLED";
        }
        bcMsg = bcMsg + " id=";
        bcMsg = bcMsg + m_DeviceId;
        LFPG_Util.Info(bcMsg);

        if (m_BroadcastEnabled)
        {
            SEffectManager.PlaySound(LFPG_INTERCOM_SND_STATIC_BURST, GetPosition());
        }
        #endif
    }

    // ============================================
    // T2: Cycle Frequency
    // ============================================
    void LFPG_CycleFrequency()
    {
        #ifdef SERVER
        m_FrequencyIndex = m_FrequencyIndex + 1;
        if (m_FrequencyIndex >= LFPG_INTERCOM_FREQ_COUNT)
        {
            m_FrequencyIndex = 0;
        }
        SetSynchDirty();

        if (m_GhostRadio)
        {
            m_GhostRadio.SetFrequencyByIndex(m_FrequencyIndex);
            m_GhostRadio.SetSynchDirty();
        }

        string freqMsg = "[LFPG_Intercom] Frequency changed to index=";
        freqMsg = freqMsg + m_FrequencyIndex.ToString();
        freqMsg = freqMsg + " id=";
        freqMsg = freqMsg + m_DeviceId;
        LFPG_Util.Info(freqMsg);

        SEffectManager.PlaySound(LFPG_INTERCOM_SND_KNOB_CLICK, GetPosition());
        #endif
    }
};
