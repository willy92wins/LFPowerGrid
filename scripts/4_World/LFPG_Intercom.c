// =========================================================
// LF_PowerGrid - Intercom / RF Broadcaster (v3.0.0)
//
// LF_Intercom_Kit: Holdable, deployable (same-model pattern).
// LF_Intercom:     CONSUMER, 2 IN ports (input_1 + input_toggle).
//                   No output, no wire store.
//                   T1: 10 u/s, gate toggle on/off, RF toggle (Sprint 2).
//                   T2: 20 u/s, ghost radio bidirectional VOIP (Sprint 3).
//
// Model: rf_broadcaster.p3d
//   Memory points: port_input_0 (input_1), port_output_0 (input_toggle)
//   Animations: knob_freq, knob_input, knob_vol (rotation via model.cfg)
//   Hidden selections:
//     0: camo (body texture)
//     1: camoscreen (screen/display)
//     2: light_led (LED1 power/gate)
//     3: light_led2 (LED2 broadcast T2)
//     4: microphone (proxy, hidden default, visible after T2 install)
//
// LED1 states (hiddenSelections index 2 = "light_led"):
//   Green = m_PoweredNet && m_SwitchOn
//   Red   = m_PoweredNet && !m_SwitchOn
//   Off   = !m_PoweredNet
//
// LED2 states (hiddenSelections index 3 = "light_led2"):
//   Blue  = m_RadioInstalled && m_BroadcastEnabled && m_PoweredNet
//   Off   = otherwise
//
// Persistence: DeviceIdLow, DeviceIdHigh, m_SwitchOn,
//              m_RadioInstalled, m_BroadcastEnabled, m_FrequencyIndex.
//   m_PoweredNet NOT persisted (derived by graph).
//   Ghost radio NOT persisted (re-created in AfterStoreLoad).
//   Save wipe required (new device, new persistence format).
// =========================================================

// ---------------------------------------------------------
// RVMAT paths for LED states
// ---------------------------------------------------------
static const string LFPG_INTERCOM_RVMAT_OFF    = "\\LFPowerGrid\\data\\rf_broadcaster\\data\\led_off.rvmat";
static const string LFPG_INTERCOM_RVMAT_GREEN  = "\\LFPowerGrid\\data\\rf_broadcaster\\data\\rf_broadcaster_green.rvmat";
static const string LFPG_INTERCOM_RVMAT_RED    = "\\LFPowerGrid\\data\\rf_broadcaster\\data\\rf_broadcaster_red.rvmat";
static const string LFPG_INTERCOM_RVMAT_BLUE   = "\\LFPowerGrid\\data\\rf_broadcaster\\data\\rf_broadcaster_blue.rvmat";

// Microphone texture path (hiddenSelection index 4)
static const string LFPG_INTERCOM_MIC_TEX      = "\\LFPowerGrid\\data\\rf_broadcaster\\data\\rf_broadcaster_microphone_co.paa";
static const string LFPG_INTERCOM_MIC_RVMAT    = "\\LFPowerGrid\\data\\rf_broadcaster\\data\\rf_broadcaster.rvmat";

// ---------------------------------------------------------
// Constants
// ---------------------------------------------------------
static const float LFPG_INTERCOM_CONSUMPTION_T1 = 10.0;
static const float LFPG_INTERCOM_CONSUMPTION_T2 = 20.0;

// Hidden selection indices (must match config.cpp order)
static const int LFPG_INTERCOM_HS_CAMO       = 0;
static const int LFPG_INTERCOM_HS_SCREEN     = 1;
static const int LFPG_INTERCOM_HS_LED1       = 2;
static const int LFPG_INTERCOM_HS_LED2       = 3;
static const int LFPG_INTERCOM_HS_MIC        = 4;

// Sound effect soundsets (must match config.cpp CfgSoundSets)
static const string LFPG_INTERCOM_SND_RF_BEEP      = "LFPG_Intercom_RFBeep_SoundSet";
static const string LFPG_INTERCOM_SND_KNOB_CLICK   = "LFPG_Intercom_KnobClick_SoundSet";
static const string LFPG_INTERCOM_SND_STATIC_BURST  = "LFPG_Intercom_Static_SoundSet";

// ---------------------------------------------------------
// KIT - same-model deploy pattern (SwitchV2/PushButton parity)
// ---------------------------------------------------------
class LF_Intercom_Kit : Inventory_Base
{
    override bool IsDeployable()
    {
        return true;
    }

    override bool CanDisplayCargo()
    {
        return false;
    }

    override bool CanBePlaced(Man player, vector position)
    {
        return true;
    }

    override bool DoPlacingHeightCheck()
    {
        return false;
    }

    override string GetDeploySoundset()
    {
        return "placeBarbedWire_SoundSet";
    }

    // Previene loop sound huerfano: ObjectDelete durante OnPlacementComplete
    // interrumpe el cleanup del action callback antes de detener el sonido.
    override string GetLoopDeploySoundset()
    {
        return "";
    }

    override void SetActions()
    {
        super.SetActions();
        AddAction(ActionTogglePlaceObject);
        AddAction(LFPG_ActionPlaceIntercom);
    }

    // Usar parametro position/orientation, NUNCA GetPosition().
    // GetPosition() devuelve la pos del kit (cerca del player), no el hologram.
    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[Intercom_Kit] OnPlacementComplete: param=" + position.ToString();
        tLog = tLog + " kitPos=" + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        // No ECE_PLACE_ON_SURFACE — mata wall placement.
        string className = "LF_Intercom";
        EntityAI device = GetGame().CreateObjectEx(className, finalPos, ECE_CREATEPHYSICS);
        if (device)
        {
            device.SetPosition(finalPos);
            device.SetOrientation(finalOri);
            device.Update();

            string deployMsg = "[Intercom_Kit] Deployed LF_Intercom at " + finalPos.ToString();
            deployMsg = deployMsg + " ori=" + finalOri.ToString();
            LFPG_Util.Info(deployMsg);

            // Solo borrar kit si spawn exitoso.
            GetGame().ObjectDelete(this);
        }
        else
        {
            LFPG_Util.Error("[Intercom_Kit] Failed to create LF_Intercom! Kit preserved.");
            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                string errMsg = "[LFPG] Intercom placement failed. Kit preserved.";
                pb.MessageStatus(errMsg);
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// DEVICE: CONSUMER (2 IN ports, no output, no wire store)
// ---------------------------------------------------------
class LF_Intercom : Inventory_Base
{
    // ---- Device identity ----
    protected int m_DeviceIdLow = 0;
    protected int m_DeviceIdHigh = 0;
    protected string m_DeviceId;

    // ---- Power state (set by graph propagation) ----
    protected bool m_PoweredNet = false;

    // ---- Switch state (latching: stays ON until toggled OFF) ----
    protected bool m_SwitchOn = false;

    // ---- T2 upgrade state ----
    protected bool m_RadioInstalled = false;
    protected bool m_BroadcastEnabled = false;
    protected int m_FrequencyIndex = 0;

    // ---- Overload state ----
    protected bool m_Overloaded = false;

    // ---- RF toggle state (Sprint 2) ----
    protected bool m_PrevToggleInput = false;
    protected int m_LastRFToggleTime = 0;

    // ---- Ghost radio (Sprint 3: T2 bidirectional VOIP) ----
    protected LF_GhostRadio m_GhostRadio;

    // ---- Deletion guard ----
    protected bool m_LFPG_Deleting = false;

    // ============================================
    // Constructor - SyncVars en constructor, NO EEInit
    // ============================================
    void LF_Intercom()
    {
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
        RegisterNetSyncVariableBool("m_SwitchOn");
        RegisterNetSyncVariableBool("m_RadioInstalled");
        RegisterNetSyncVariableBool("m_BroadcastEnabled");
        RegisterNetSyncVariableInt("m_FrequencyIndex");
        RegisterNetSyncVariableBool("m_Overloaded");
    }

    // ============================================
    // Actions
    // ============================================
    override void SetActions()
    {
        super.SetActions();
        RemoveAction(ActionTakeItem);
        RemoveAction(ActionTakeItemToHands);
        AddAction(LFPG_ActionToggleIntercom);
        AddAction(LFPG_ActionRFToggle);
        AddAction(LFPG_ActionToggleBroadcast);
        AddAction(LFPG_ActionCycleFrequency);
    }

    // ============================================
    // Inventory guards (prevent pickup — placed device)
    // ============================================
    override bool CanPutInCargo(EntityAI parent)
    {
        return false;
    }

    override bool CanPutIntoHands(EntityAI parent)
    {
        return false;
    }

    override bool CanBePlaced(Man player, vector position)
    {
        return false;
    }

    override bool IsHeavyBehaviour()
    {
        return false;
    }

    // ============================================
    // Device ID helpers
    // ============================================
    protected void LFPG_UpdateDeviceIdString()
    {
        m_DeviceId = LFPG_Util.MakeDeviceKey(m_DeviceIdLow, m_DeviceIdHigh);
    }

    protected void LFPG_TryRegister()
    {
        if (m_LFPG_Deleting)
            return;

        string oldId = m_DeviceId;
        LFPG_UpdateDeviceIdString();

        if (oldId != "" && oldId != m_DeviceId)
        {
            LFPG_DeviceRegistry.Get().Unregister(oldId, this);
        }

        if (m_DeviceId != "")
        {
            LFPG_DeviceRegistry.Get().Register(this, m_DeviceId);
        }
    }

    // ============================================
    // Lifecycle
    // ============================================
    override void EEInit()
    {
        super.EEInit();

        #ifdef SERVER
        if (m_DeviceIdLow == 0 && m_DeviceIdHigh == 0)
        {
            LFPG_Util.GenerateDeviceId(m_DeviceIdLow, m_DeviceIdHigh);
        }
        SetSynchDirty();

        // Register for toggle input evaluation ticks
        LFPG_NetworkManager.Get().RegisterIntercom(this);
        #endif

        LFPG_UpdateDeviceIdString();
        LFPG_TryRegister();

        // Apply initial visual state (client-side).
        // Ensures microphone proxy is hidden at spawn even if
        // config "" material falls back to baked p3d material.
        LFPG_UpdateVisuals();
    }

    override void EEKilled(Object killer)
    {
        LFPG_DestroyGhostRadio();
        LFPG_DeviceLifecycle.OnDeviceKilled(this, m_DeviceId);

        #ifdef SERVER
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

        super.EEKilled(killer);
    }

    override void EEDelete(EntityAI parent)
    {
        m_LFPG_Deleting = true;

        LFPG_DestroyGhostRadio();

        // Unregister from toggle input evaluation ticks
        LFPG_NetworkManager.Get().UnregisterIntercom(this);

        LFPG_DeviceLifecycle.OnDeviceDeleted(this, m_DeviceId);
        super.EEDelete(parent);
    }

    override void EEItemLocationChanged(notnull InventoryLocation oldLoc, notnull InventoryLocation newLoc)
    {
        super.EEItemLocationChanged(oldLoc, newLoc);

        #ifdef SERVER
        if (m_DeviceId == "")
            return;

        bool wiresCut = LFPG_DeviceLifecycle.OnDeviceMoved(this, m_DeviceId, oldLoc, newLoc);
        if (wiresCut)
        {
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
                // Ghost radio: conditions now false → destroy immediately
                LFPG_UpdateGhostRadio();
            }
        }
        #endif
    }

    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();
        LFPG_TryRegister();

        #ifndef SERVER
        // LED + animation update
        LFPG_UpdateVisuals();

        // CableRenderer sync: this device is a target (CONSUMER, no owner wires)
        // but cables pointing at it need entity resolution for rendering.
        if (m_DeviceId != "")
        {
            LFPG_CableRenderer r = LFPG_CableRenderer.Get();
            if (r)
            {
                r.RequestDeviceSync(m_DeviceId, this);
            }
        }
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
        // Latching toggle: flip state
        if (m_SwitchOn)
        {
            m_SwitchOn = false;
        }
        else
        {
            m_SwitchOn = true;
        }
        SetSynchDirty();

        string togMsg = "[LF_Intercom] Toggle ";
        if (m_SwitchOn)
        {
            togMsg = togMsg + "ON";
        }
        else
        {
            togMsg = togMsg + "OFF";
        }
        togMsg = togMsg + " id=" + m_DeviceId;
        LFPG_Util.Info(togMsg);

        // Ghost radio lifecycle: may need spawn/destroy on toggle
        LFPG_UpdateGhostRadio();

        // Propagate immediately so graph updates
        LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);

        // Play knob click sound
        SEffectManager.PlaySound(LFPG_INTERCOM_SND_KNOB_CLICK, GetPosition());
        #endif
    }

    // ============================================
    // LFPG_IDevice interface
    // ============================================
    string LFPG_GetDeviceId()
    {
        return m_DeviceId;
    }

    // 2 ports: both IN
    int LFPG_GetPortCount()
    {
        return 2;
    }

    string LFPG_GetPortName(int idx)
    {
        if (idx == 0) return "input_1";
        if (idx == 1) return "input_toggle";
        return "";
    }

    int LFPG_GetPortDir(int idx)
    {
        if (idx == 0) return LFPG_PortDir.IN;
        if (idx == 1) return LFPG_PortDir.IN;
        return -1;
    }

    string LFPG_GetPortLabel(int idx)
    {
        if (idx == 0) return "Power In";
        if (idx == 1) return "Toggle Signal";
        return "";
    }

    bool LFPG_HasPort(string portName, int dir)
    {
        if (dir == LFPG_PortDir.IN && portName == "input_1") return true;
        if (dir == LFPG_PortDir.IN && portName == "input_toggle") return true;
        return false;
    }

    // Port positions: rf_broadcaster.p3d memory points
    // port_input_0 -> input_1 (main power)
    // port_output_0 -> input_toggle (RF trigger signal)
    vector LFPG_GetPortWorldPos(string portName)
    {
        // Map LFPG port names -> p3d memory point names
        string memPoint;
        if (portName == "input_1")
        {
            memPoint = "port_input_0";
        }
        else if (portName == "input_toggle")
        {
            // Despite physical name port_output_0, this is logically input_toggle
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

        // Fallback: center of device
        return GetPosition();
    }

    // ---- CONSUMER device type ----
    int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.CONSUMER;
    }

    // ---- Not a source — pure consumer ----
    bool LFPG_IsSource()
    {
        return false;
    }

    bool LFPG_GetSourceOn()
    {
        return false;
    }

    // Gate: when true, device "accepts" power (powered state)
    bool LFPG_IsGateOpen()
    {
        return m_SwitchOn;
    }

    bool LFPG_IsGateCapable()
    {
        return true;
    }

    // Consumption depends on tier
    float LFPG_GetConsumption()
    {
        if (m_RadioInstalled)
        {
            return LFPG_INTERCOM_CONSUMPTION_T2;
        }
        return LFPG_INTERCOM_CONSUMPTION_T1;
    }

    // Expose powered state
    bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    // Expose switch state for action text
    bool LFPG_GetSwitchOn()
    {
        return m_SwitchOn;
    }

    // Called by graph propagation when upstream power state changes
    void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;
        SetSynchDirty();

        // Reset rising edge when power is lost
        if (!powered)
        {
            m_PrevToggleInput = false;
        }

        // Ghost radio lifecycle: spawn/destroy based on power state
        LFPG_UpdateGhostRadio();

        string pwrMsg = "[LF_Intercom] SetPowered(" + powered.ToString() + ") id=" + m_DeviceId;
        LFPG_Util.Debug(pwrMsg);
        #endif
    }

    // Overload state
    bool LFPG_GetOverloaded()
    {
        return m_Overloaded;
    }

    void LFPG_SetOverloaded(bool val)
    {
        #ifdef SERVER
        if (m_Overloaded != val)
        {
            m_Overloaded = val;
            SetSynchDirty();
        }
        #endif
    }

    // ---- No wire store (CONSUMER, no output ports) ----
    bool LFPG_HasWireStore()
    {
        return false;
    }

    // ---- Connection validation: no outgoing connections ----
    bool LFPG_CanConnectTo(Object other, string myPort, string otherPort)
    {
        // CONSUMER with no OUT ports — cannot initiate connections
        return false;
    }

    // ---- T2 state accessors (for Sprint 3 actions) ----
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

    // ============================================
    // RF Toggle — Sprint 2
    // ============================================

    // Execute RF toggle: scan for RF-capable devices within 50m and toggle them.
    // Called by manual action (LFPG_ActionRFToggle) or by rising edge on input_toggle.
    // Server only. Respects 2s cooldown.
    void LFPG_ExecuteRFToggle()
    {
        #ifdef SERVER
        // Cooldown guard
        int now = GetGame().GetTime();
        int elapsed = now - m_LastRFToggleTime;
        if (elapsed < LFPG_INTERCOM_RF_COOLDOWN_MS)
            return;

        m_LastRFToggleTime = now;

        // Spatial scan: get all registered devices
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

            // Skip self
            if (device == this)
                continue;

            // Distance check (50m radius)
            float distSq = LFPG_WorldUtil.DistSq(myPos, device.GetPosition());
            if (distSq > LFPG_INTERCOM_RF_RANGE_SQ)
                continue;

            // Check if RF-capable
            if (!LFPG_DeviceAPI.IsRFCapable(device))
                continue;

            // Toggle it
            bool result = LFPG_DeviceAPI.RemoteToggle(device);
            if (result)
            {
                toggled = toggled + 1;
            }
        }

        string rfMsg = "[LF_Intercom] RF toggle executed: ";
        rfMsg = rfMsg + toggled.ToString();
        rfMsg = rfMsg + " devices toggled, id=";
        rfMsg = rfMsg + m_DeviceId;
        LFPG_Util.Info(rfMsg);

        // Play RF beep sound on intercom
        SEffectManager.PlaySound(LFPG_INTERCOM_SND_RF_BEEP, GetPosition());
        #endif
    }

    // Evaluate input_toggle port for rising edge detection.
    // Called after graph propagation resolves allocated power.
    // Queries the graph for the edge targeting our input_toggle port
    // and checks if allocated power >= threshold (20 u/s).
    // Fires RF toggle ONCE on rising edge (false→true transition).
    void LFPG_EvaluateToggleInput()
    {
        #ifdef SERVER
        if (!m_PoweredNet || !m_SwitchOn)
        {
            m_PrevToggleInput = false;
            return;
        }

        // Query allocated power on the input_toggle port from the graph
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

                    // Only check edges targeting our input_toggle port
                    if (edge.m_TargetPort == "input_toggle")
                    {
                        togglePower = togglePower + edge.m_AllocatedPower;
                    }
                }
            }
        }

        // Rising edge detection
        bool currentInput = false;
        if (togglePower >= LFPG_INTERCOM_TOGGLE_INPUT_MIN)
        {
            currentInput = true;
        }

        // Fire on rising edge only (was false, now true)
        if (currentInput && !m_PrevToggleInput)
        {
            LFPG_Util.Info("[LF_Intercom] Toggle input rising edge detected, id=" + m_DeviceId);
            LFPG_ExecuteRFToggle();
        }

        m_PrevToggleInput = currentInput;
        #endif
    }

    // Cooldown accessor for action condition check
    int LFPG_GetLastRFToggleTime()
    {
        return m_LastRFToggleTime;
    }

    // ============================================
    // T2: Ghost Radio Lifecycle (Sprint 3)
    // ============================================

    // Spawn ghost radio at intercom position. Server only.
    // Configures as bidirectional transceiver on current frequency.
    protected void LFPG_SpawnGhostRadio()
    {
        #ifdef SERVER
        if (m_GhostRadio)
            return;

        vector pos = GetPosition();
        string ghostClass = "LF_GhostRadio";
        Object ghostObj = GetGame().CreateObjectEx(ghostClass, pos, ECE_CREATEPHYSICS);
        m_GhostRadio = LF_GhostRadio.Cast(ghostObj);
        if (!m_GhostRadio)
        {
            LFPG_Util.Error("[LF_Intercom] Failed to spawn GhostRadio at " + pos.ToString());
            return;
        }

        // Configure as bidirectional transceiver
        // Belt-and-suspenders: ensure CompEM has energy and is active
        // before activating the transmitter. EEInit sets energy=9999,
        // but we re-set here in case of timing edge cases.
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

        string spawnMsg = "[LF_Intercom] GhostRadio spawned at ";
        spawnMsg = spawnMsg + pos.ToString();
        spawnMsg = spawnMsg + " freq=";
        spawnMsg = spawnMsg + m_FrequencyIndex.ToString();
        spawnMsg = spawnMsg + " id=";
        spawnMsg = spawnMsg + m_DeviceId;
        LFPG_Util.Info(spawnMsg);
        #endif
    }

    // Destroy ghost radio. Server only. Safe to call if null.
    protected void LFPG_DestroyGhostRadio()
    {
        #ifdef SERVER
        if (m_GhostRadio)
        {
            GetGame().ObjectDelete(m_GhostRadio);
            m_GhostRadio = null;

            string destroyMsg = "[LF_Intercom] GhostRadio destroyed, id=";
            destroyMsg = destroyMsg + m_DeviceId;
            LFPG_Util.Info(destroyMsg);
        }
        #endif
    }

    // Evaluate conditions and spawn/destroy ghost radio as needed.
    // Called from LFPG_SetPowered, ToggleIntercom, ToggleBroadcast, InstallRadio.
    // Conditions for ghost alive: radioInstalled + powered + switchOn + broadcastEnabled.
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
    // T2: Install Radio (called by ActionInstallMic)
    // ============================================
    void LFPG_InstallRadio()
    {
        #ifdef SERVER
        m_RadioInstalled = true;
        m_FrequencyIndex = 0;
        SetSynchDirty();

        // Ghost radio update (may spawn if conditions met)
        LFPG_UpdateGhostRadio();

        LFPG_Util.Info("[LF_Intercom] Radio installed, id=" + m_DeviceId);
        #endif
    }

    // ============================================
    // T2: Toggle Broadcast (called by ActionToggleBroadcast)
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

        // Ghost radio lifecycle update
        LFPG_UpdateGhostRadio();

        string bcMsg = "[LF_Intercom] Broadcast ";
        if (m_BroadcastEnabled)
        {
            bcMsg = bcMsg + "ENABLED";
        }
        else
        {
            bcMsg = bcMsg + "DISABLED";
        }
        bcMsg = bcMsg + " id=" + m_DeviceId;
        LFPG_Util.Info(bcMsg);

        // Play static burst when broadcast is enabled
        if (m_BroadcastEnabled)
        {
            SEffectManager.PlaySound(LFPG_INTERCOM_SND_STATIC_BURST, GetPosition());
        }
        #endif
    }

    // ============================================
    // T2: Cycle Frequency (called by ActionCycleFrequency)
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

        // Update ghost radio frequency if it exists
        if (m_GhostRadio)
        {
            m_GhostRadio.SetFrequencyByIndex(m_FrequencyIndex);
            m_GhostRadio.SetSynchDirty();
        }

        string freqMsg = "[LF_Intercom] Frequency changed to index=";
        freqMsg = freqMsg + m_FrequencyIndex.ToString();
        freqMsg = freqMsg + " id=";
        freqMsg = freqMsg + m_DeviceId;
        LFPG_Util.Info(freqMsg);

        // Play knob click sound
        SEffectManager.PlaySound(LFPG_INTERCOM_SND_KNOB_CLICK, GetPosition());
        #endif
    }

    // ============================================
    // Persistence
    // ============================================
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);

        ctx.Write(m_DeviceIdLow);
        ctx.Write(m_DeviceIdHigh);
        ctx.Write(m_SwitchOn);
        ctx.Write(m_RadioInstalled);
        ctx.Write(m_BroadcastEnabled);
        ctx.Write(m_FrequencyIndex);

        // m_PoweredNet: NOT persisted (derived by graph)
        // Ghost radio: NOT persisted (re-created in AfterStoreLoad)
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version))
            return false;

        if (!ctx.Read(m_DeviceIdLow))
        {
            LFPG_Util.Error("[LF_Intercom] OnStoreLoad: failed to read m_DeviceIdLow");
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            LFPG_Util.Error("[LF_Intercom] OnStoreLoad: failed to read m_DeviceIdHigh");
            return false;
        }

        if (!ctx.Read(m_SwitchOn))
        {
            LFPG_Util.Error("[LF_Intercom] OnStoreLoad: failed to read m_SwitchOn");
            return false;
        }

        if (!ctx.Read(m_RadioInstalled))
        {
            LFPG_Util.Error("[LF_Intercom] OnStoreLoad: failed to read m_RadioInstalled");
            return false;
        }

        if (!ctx.Read(m_BroadcastEnabled))
        {
            LFPG_Util.Error("[LF_Intercom] OnStoreLoad: failed to read m_BroadcastEnabled");
            return false;
        }

        if (!ctx.Read(m_FrequencyIndex))
        {
            LFPG_Util.Error("[LF_Intercom] OnStoreLoad: failed to read m_FrequencyIndex");
            return false;
        }

        LFPG_UpdateDeviceIdString();

        string loadMsg = "[LF_Intercom] Loaded id=" + m_DeviceId;
        loadMsg = loadMsg + " switchOn=" + m_SwitchOn.ToString();
        loadMsg = loadMsg + " radioInstalled=" + m_RadioInstalled.ToString();
        loadMsg = loadMsg + " freq=" + m_FrequencyIndex.ToString();
        LFPG_Util.Info(loadMsg);

        return true;
    }

    // ============================================
    // AfterStoreLoad — re-create ghost radio if conditions met
    // Ghost is NOT persisted — re-spawned from persisted state.
    // ============================================
    override void AfterStoreLoad()
    {
        super.AfterStoreLoad();

        #ifdef SERVER
        // Ghost radio re-creation deferred: at this point m_PoweredNet
        // is still false (graph hasn't propagated yet). The ghost will be
        // spawned when LFPG_SetPowered(true) is called by the graph.
        // However, we log the loaded state for debugging.
        string afterMsg = "[LF_Intercom] AfterStoreLoad id=";
        afterMsg = afterMsg + m_DeviceId;
        afterMsg = afterMsg + " radio=";
        afterMsg = afterMsg + m_RadioInstalled.ToString();
        afterMsg = afterMsg + " broadcast=";
        afterMsg = afterMsg + m_BroadcastEnabled.ToString();
        LFPG_Util.Debug(afterMsg);
        #endif
    }
};
