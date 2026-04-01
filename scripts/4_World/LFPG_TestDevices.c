// =========================================================
// LF_PowerGrid - Example devices (v0.9.4)
// - LFPG_Generator: source (output_1..4) + owns wires + persistence
// - LF_TestLamp: consumer (input_main) + visible client light
// - LF_TestLampHeavy: high-consumption consumer for load testing
//
// v0.9.4 (JIP Fix): Generator sound/particles after reconnect.
//          Root cause: EEInit killed CompEM on client, and SwitchOn()
//          from script cannot restart the C++ energy manager (469 failed
//          retries confirmed in production logs, 0 successes).
//          Fix: EEInit SwitchOff is now SERVER-ONLY. Client relies on
//          engine C++ CompEM replication for correct working state.
//          OnVariablesSynchronized simplified: only SwitchOff path kept.
//          Entire retry mechanism removed (~80 lines, eliminates ~1200
//          spam log lines per JIP session). No save wipe needed.
// v0.7.14: Generator LFPG_GetSourceOn uses m_SourceOn only.
// v0.7.23: SparkPlug validation on toggle, EEInit sync.
// v0.7.25: Remove vanilla actions, lower movement threshold.
// v0.7.26: Centralized CutAllWiresFromDevice, IsSparkPlugValid,
//          EEKilled, dual-layer EEItemLocationChanged, block
//          vanilla plug/unplug on lamps.
// v0.7.27: Refactored to LFPG_DeviceLifecycle static helpers.
//          Zero duplicated lifecycle code across device classes.
// v0.7.29: External audit integration (Audit 1/2/3):
//          - CompEM SwitchOff on both client+server (visual effects fix)
//          - CanBePickedUp/IsHeavyBehaviour overrides for lamp
//          - Position polling timer for heavy carry bypass detection
// v0.7.30: Removed per-device position polling timers (Audit 1+2 closure).
//          Movement detection is now centralized in NetworkManager with
//          round-robin batching. See LFPG_MOVE_DETECT_* constants.
// v0.7.38 (RC-04): m_LFPG_Deleting flag in EEDelete prevents post-mortem
//          re-registration via OnVariablesSynchronized. Blocks stale refs
//          in DeviceRegistry when engine delivers final SyncVar updates
//          for devices leaving the network bubble after deletion starts.
// v0.7.42 (BugFix): Ghost lamp — removed m_PoweredNet from persistence.
//          m_PoweredNet is a derived state from the electrical graph;
//          persisting it caused lamps to appear lit after restart when
//          their source no longer exists. Now starts false (field default)
//          and only propagation sets it true. REQUIRES SAVE WIPE.
// v0.7.47 (Desync Fix): Generator CompEM client-server desynchronization.
//          - P0: OnVariablesSynchronized now reconciles CompEM with
//            m_SourceOn on client (analogous to lamp's light sync).
//          - P1: OnSwitchOn/OnWorkStart/OnWork sparkplug validation
//            restricted to server-only. Client attachment sync can lag
//            behind CompEM replication, causing permanent false-negative.
//            Server is authority; client reconciles via OnVarSync.
//          - P2: Auto-correction is implicit via P0 fix — any future
//            desync self-heals on next SyncVar update.
//
// Wire manipulation delegated to LFPG_WireHelper (3_Game).
// =========================================================

class LFPG_Generator : PowerGenerator
{
    protected int m_DeviceIdLow = 0;

    protected int m_DeviceIdHigh = 0;

    protected string m_DeviceId;

    // Wires owned by this device (output side)
    protected ref array<ref LFPG_WireData> m_Wires;

    // Source state (replicated)
    protected bool m_SourceOn = false;

    // v0.7.8: Load ratio (replicated to clients for visual state).
    // 0.0 = idle, 0.5 = 50% load, 1.0 = full, >1.0 = overloaded.
    protected float m_LoadRatio = 0.0;

    // v0.7.8: Bitmask of overloaded output wires.
    // Bit N = 1 means wire at index N exceeded available capacity.
    // Synced to clients for per-wire CRITICAL_LOAD cable state.
    protected bool m_Overloaded = false;

    // v0.7.35 (F1.3): Bitmask of warning-level output wires.
    // Bit N = 1 means wire at index N is getting power but less than demanded.
    // Synced to clients for per-wire WARNING_LOAD cable state.

    // v0.7.38 (RC-04): Deletion guard.
    // Set true at the start of EEDelete to prevent post-mortem re-registration
    // via OnVariablesSynchronized. Engine may deliver a final SyncVar update
    // for a device leaving the network bubble after EEDelete has started.
    protected bool m_LFPG_Deleting = false;

    // v0.9.4: CompEM retry mechanism REMOVED.
    // Client-side SwitchOn() from script cannot restart the C++ energy
    // manager — the retry loop ran 469+ times with 0 successes.
    // Fix: EEInit no longer kills CompEM on client (see EEInit comment).

    // v0.7.30: Per-device position polling removed.
    // Movement detection is now centralized in NetworkManager.CheckDeviceMovement()
    // with round-robin batching (Audit 1+2 closure).

    void LFPG_Generator()
    {
        m_Wires = new array<ref LFPG_WireData>;
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_SourceOn");
        float syncMin = 0.0;
        float syncMax = 5.0;
        RegisterNetSyncVariableFloat("m_LoadRatio", syncMin, syncMax, 2);
        RegisterNetSyncVariableBool("m_Overloaded");
    }

    override void SetActions()
    {
        super.SetActions();

        // v0.7.25 (Bug 1): Remove vanilla turn on/off actions.
        // These bypass LFPG_ToggleSource's sparkplug validation and
        // can trigger CompEM via paths that CanTurnOn() might not catch
        // on all vanilla DayZ versions. Only LFPG_ToggleSource should
        // control this generator.
        RemoveAction(ActionTurnOnPowerGenerator);
        RemoveAction(ActionTurnOffPowerGenerator);

        // v0.7.27 (Audit 5): Remove vanilla electrical connection actions.
        // PowerGenerator inherits plug/unplug from its base class hierarchy.
        // Without explicit removal, players with vanilla CableReel near
        // an LFPG_Generator see "Plug in" / "Unplug" vanilla actions.
        RemoveAction(ActionPlugIn);
        RemoveAction(ActionUnplugThisByCord);

        // v0.7.38: Vanilla take/carry actions restored.
        // Picking up a wired generator breaks its connections (position-based
        // IDs change). This is by design — data integrity over convenience.

        AddAction(ActionLFPG_ToggleSource);
    }

    // v0.7.27 (Audit 5): Prevent vanilla electrical system from treating
    // this as a pluggable device. Returns false to block all vanilla
    // plug/unplug action condition checks from the item side.
    override bool IsElectricAppliance()
    {
        return false;
    }

    override void EEInit()
    {
        super.EEInit();

        // v0.9.4 (JIP Fix): SwitchOff moved to SERVER-ONLY.
        // Previous v0.7.29 killed CompEM on BOTH sides, but client-side
        // SwitchOn() from script cannot restart the C++ energy manager
        // after EEInit kills it — confirmed by 469 failed retry attempts
        // in production logs. On the client, the engine's C++ CompEM
        // replication delivers the correct working state from the server.
        // Killing it here on client permanently breaks sound/particles
        // with no script-level recovery path.
        #ifdef SERVER
        ComponentEnergyManager emKillVanilla = GetCompEM();
        if (emKillVanilla)
        {
            emKillVanilla.SwitchOff();
        }
        #endif

        #ifdef SERVER
        if (m_DeviceIdLow == 0 && m_DeviceIdHigh == 0)
        {
            LFPG_Util.GenerateDeviceId(m_DeviceIdLow, m_DeviceIdHigh);
        }
        #endif

        LFPG_UpdateDeviceIdString();
        LFPG_TryRegister();

        // v0.7.27: Uses centralized sparkplug validation (includes health check)
        #ifdef SERVER
        ComponentEnergyManager emInit = GetCompEM();
        if (emInit)
        {
            if (m_SourceOn && LFPG_DeviceLifecycle.IsSparkPlugValid(this))
            {
                emInit.SwitchOn();
            }
            else
            {
                emInit.SwitchOff();
                if (m_SourceOn && !LFPG_DeviceLifecycle.IsSparkPlugValid(this))
                {
                    // Persisted ON but sparkplug invalid: clear m_SourceOn
                    m_SourceOn = false;
                    SetSynchDirty();
                    string eeMsg = "[LFPG_Generator] EEInit: cleared m_SourceOn (invalid SparkPlug) id=" + m_DeviceId;
                    LFPG_Util.Info(eeMsg);
                }
            }
        }
        #endif

        #ifdef SERVER
        LFPG_NetworkManager.Get().BroadcastOwnerWires(this);

        if (m_SourceOn)
            LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);

        // v0.9.3 (Audit Fix #2): Force SyncVar replication after persistence load.
        // For persisted devices (m_DeviceIdLow/High != 0), the conditional
        // SetSynchDirty above is never reached. DayZ engine SHOULD send
        // all SyncVars when an entity enters a client's bubble, but this
        // is not guaranteed across all engine versions. Without this,
        // the client may never receive m_SourceOn=true after restart.
        SetSynchDirty();
        #endif
    }

    // v0.7.27: Delegates to DeviceLifecycle helper
    override void EEDelete(EntityAI parent)
    {
        // v0.7.38 (RC-04): Set deletion flag BEFORE unregistration.
        // Prevents OnVariablesSynchronized from re-registering a dying device.
        m_LFPG_Deleting = true;

        LFPG_DeviceLifecycle.OnDeviceDeleted(this, m_DeviceId);
        super.EEDelete(parent);
    }

    // v0.7.26 (Bug 1+2): Cut all wires when generator is destroyed.
    // v0.7.27: Delegates to DeviceLifecycle, then handles generator-specific state.
    override void EEKilled(Object killer)
    {
        LFPG_DeviceLifecycle.OnDeviceKilled(this, m_DeviceId);

        #ifdef SERVER
        // Generator-specific: force off
        if (m_SourceOn)
        {
            m_SourceOn = false;
            SetSynchDirty();
        }

        ComponentEnergyManager emKill = GetCompEM();
        if (emKill)
        {
            emKill.SwitchOff();
        }
        #endif

        super.EEKilled(killer);
    }

    // Vanilla CompEM hooks: sync m_SourceOn with vanilla state.
    // v0.7.27: Uses LFPG_DeviceLifecycle.IsSparkPlugValid (includes health check)
    // v0.7.29 (Audit fix): SwitchOff on BOTH client+server to kill effects.
    // v0.7.47 (Desync Fix P1): Sparkplug validation SERVER-ONLY.
    // Same rationale as OnSwitchOn — client attachment sync may lag.
    override void OnWorkStart()
    {
        #ifdef SERVER
        if (!LFPG_DeviceLifecycle.IsSparkPlugValid(this))
        {
            string wsBlkMsg = "[LFPG_Generator] OnWorkStart blocked: invalid SparkPlug id=" + m_DeviceId;
            LFPG_Util.Info(wsBlkMsg);
            ComponentEnergyManager emForce = GetCompEM();
            if (emForce)
            {
                emForce.SwitchOff();
            }
            return;
        }
        #endif

        super.OnWorkStart();

        #ifdef SERVER
        m_SourceOn = true;
        SetSynchDirty();

        string wsMsg = "[LFPG_Generator] OnWorkStart id=" + m_DeviceId + " SourceOn=true";
        LFPG_Util.Info(wsMsg);
        if (m_DeviceId != "")
        {
            LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
        }
        #endif
    }

    override void OnWorkStop()
    {
        super.OnWorkStop();

        #ifdef SERVER
        m_SourceOn = false;
        SetSynchDirty();

        string wstpMsg = "[LFPG_Generator] OnWorkStop id=" + m_DeviceId + " SourceOn=false";
        LFPG_Util.Info(wstpMsg);
        if (m_DeviceId != "")
        {
            LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
        }
        #endif
    }

    // v0.7.23 (Bug 2): SparkPlug attach/detach controls both propagation
    // AND vanilla CompEM (animations/sound).
    override void EEItemAttached(EntityAI item, string slot_name)
    {
        super.EEItemAttached(item, slot_name);

        #ifdef SERVER
        // v0.7.28: Case-insensitive comparison. Some DayZ versions
        // normalize slot names to lowercase in EEItemAttached/Detached.
        string slotLower = slot_name;
        slotLower.ToLower();
        if (slotLower == "sparkplug" && m_SourceOn && m_DeviceId != "")
        {
            // v0.7.27: Re-validate after attachment (checks health too)
            if (LFPG_DeviceLifecycle.IsSparkPlugValid(this))
            {
                string spAMsg = "[LFPG_Generator] SparkPlug attached while ON, starting CompEM + propagating";
                LFPG_Util.Info(spAMsg);
                ComponentEnergyManager emAttach = GetCompEM();
                if (emAttach)
                {
                    emAttach.SwitchOn();
                }
                LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
            }
        }
        #endif
    }

    override void EEItemDetached(EntityAI item, string slot_name)
    {
        super.EEItemDetached(item, slot_name);

        #ifdef SERVER
        // v0.7.28: Case-insensitive comparison (see EEItemAttached).
        string slotLowerD = slot_name;
        slotLowerD.ToLower();
        if (slotLowerD == "sparkplug" && m_SourceOn && m_DeviceId != "")
        {
            string spDMsg = "[LFPG_Generator] SparkPlug detached while ON, stopping CompEM + propagating";
            LFPG_Util.Info(spDMsg);

            // v0.7.25 (Bug 1): Clear m_SourceOn FIRST — the generator cannot
            // run without sparkplug, period.
            m_SourceOn = false;
            SetSynchDirty();

            ComponentEnergyManager emDetach = GetCompEM();
            if (emDetach)
            {
                emDetach.SwitchOff();
            }
            LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
        }
        #endif
    }

    // v0.7.27: Centralized sparkplug validation (includes ruined health).
    // NOTE: CanTurnOn does NOT exist in the PowerGeneratorBase/ItemBase hierarchy —
    // removed to fix compile error. Sparkplug validation is already enforced by
    // OnWorkStart, OnWork, and OnSwitchOn guards above and below.

    // v0.7.27: Block vanilla periodic work tick without valid sparkplug.
    // v0.7.29 (Audit fix): SwitchOff on BOTH sides; m_SourceOn logic server-only.
    // v0.7.47 (Desync Fix P1): Sparkplug validation SERVER-ONLY.
    // Client CompEM state is reconciled by OnVariablesSynchronized.
    override void OnWork(float consumed_energy)
    {
        #ifdef SERVER
        if (!LFPG_DeviceLifecycle.IsSparkPlugValid(this))
        {
            string owBlkMsg = "[LFPG_Generator] OnWork blocked: invalid SparkPlug id=" + m_DeviceId;
            LFPG_Util.Info(owBlkMsg);
            ComponentEnergyManager emKill = GetCompEM();
            if (emKill)
            {
                emKill.SwitchOff();
            }
            if (m_SourceOn)
            {
                m_SourceOn = false;
                SetSynchDirty();
                if (m_DeviceId != "")
                {
                    LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
                }
            }
            return;
        }
        #endif
        super.OnWork(consumed_energy);
    }

    // v0.7.27: Block vanilla SwitchOn without valid sparkplug.
    // v0.7.29 (Audit fix): SwitchOff on BOTH client+server to kill
    // visual effects (smoke, noise) that CompEM triggers on client.
    // v0.7.47 (Desync Fix P1): Sparkplug validation SERVER-ONLY.
    // On client, sparkplug attachment may not have synced yet when
    // CompEM replication triggers OnSwitchOn. False-negative kills
    // CompEM permanently with no retry. Server is the authority;
    // client reconciles via OnVariablesSynchronized.
    override void OnSwitchOn()
    {
        #ifdef SERVER
        if (!LFPG_DeviceLifecycle.IsSparkPlugValid(this))
        {
            string soBlkMsg = "[LFPG_Generator] OnSwitchOn blocked: invalid SparkPlug id=" + m_DeviceId;
            LFPG_Util.Info(soBlkMsg);
            ComponentEnergyManager emBlock = GetCompEM();
            if (emBlock)
            {
                emBlock.SwitchOff();
            }
            return;
        }
        #endif
        super.OnSwitchOn();
    }

    override void OnSwitchOff()
    {
        super.OnSwitchOff();

        #ifdef SERVER
        if (m_SourceOn)
        {
            m_SourceOn = false;
            SetSynchDirty();
            string soffMsg = "[LFPG_Generator] OnSwitchOff: cleared m_SourceOn id=" + m_DeviceId;
            LFPG_Util.Info(soffMsg);
            if (m_DeviceId != "")
            {
                LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
            }
        }
        #endif
    }

    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();
        LFPG_TryRegister();

        #ifndef SERVER
        // v0.9.4 (JIP Fix): Client CompEM reconciliation SIMPLIFIED.
        // Previous approach tried SwitchOn() from script when m_SourceOn=true
        // but CompEM wasn't working. This NEVER succeeds — the C++ energy
        // manager cannot be restarted from script after EEInit kills it.
        // Fix: EEInit no longer kills CompEM on client (server-only now).
        // The engine's C++ CompEM replication delivers the correct state.
        // We only need the SwitchOff path here (which DOES work from script).
        if (!m_LFPG_Deleting)
        {
            ComponentEnergyManager emSync = GetCompEM();
            if (emSync)
            {
                bool emWorking = emSync.IsWorking();
                if (!m_SourceOn && emWorking)
                {
                    emSync.SwitchOff();
                }
            }
        }

        // v0.7.35 D1+D4: Ensure renderer has wire data + immediate visual refresh
        if (m_DeviceId != "")
        {
            LFPG_CableRenderer r = LFPG_CableRenderer.Get();
            if (r)
            {
                if (!r.HasOwnerData(m_DeviceId))
                {
                    // D1: Device just entered bubble — renderer lacks wire data.
                    // Request sync from server (cooldown-throttled).
                    // v0.7.45 (H7): Pass entity for NetworkID-first resolution.
                    r.RequestDeviceSync(m_DeviceId, this);
                }
                else
                {
                    // D4: Already have wire data — immediately refresh visual state
                    // (LoadRatio, Overloaded) to eliminate CullTick delay.
                    r.NotifyOwnerVisualChanged(m_DeviceId);
                }
            }
        }
        #endif
    }

    protected void LFPG_UpdateDeviceIdString()
    {
        m_DeviceId = LFPG_Util.MakeDeviceKey(m_DeviceIdLow, m_DeviceIdHigh);
    }

    protected void LFPG_TryRegister()
    {
        // v0.7.38 (RC-04): Don't register if device is being deleted.
        if (m_LFPG_Deleting)
            return;

        // v0.7.45 (Patch 4B): Capture old ID before recalculating.
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
    // LFPG_IDevice interface
    // ============================================
    string LFPG_GetDeviceId()
    {
        return m_DeviceId;
    }

    int LFPG_GetPortCount()
    {
        return 4;
    }

    string LFPG_GetPortName(int idx)
    {
        if (idx == 0) return "output_1";
        if (idx == 1) return "output_2";
        if (idx == 2) return "output_3";
        if (idx == 3) return "output_4";
        return "";
    }

    int LFPG_GetPortDir(int idx)
    {
        if (idx >= 0 && idx <= 3) return LFPG_PortDir.OUT;
        return -1;
    }

    string LFPG_GetPortLabel(int idx)
    {
        if (idx == 0) return "Output 1";
        if (idx == 1) return "Output 2";
        if (idx == 2) return "Output 3";
        if (idx == 3) return "Output 4";
        return "";
    }

    bool LFPG_HasPort(string portName, int dir)
    {
        if (dir != LFPG_PortDir.OUT) return false;
        if (portName == "output_1") return true;
        if (portName == "output_2") return true;
        if (portName == "output_3") return true;
        if (portName == "output_4") return true;
        return false;
    }

    vector LFPG_GetPortWorldPos(string portName)
    {
        vector p = GetPosition();
        p[1] = p[1] + 1.0;
        return p;
    }

    bool LFPG_IsSource()
    {
        return true;
    }

    // Sprint 4.1: Electrical graph node type
    int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.SOURCE;
    }

    // v0.7.27: Uses centralized sparkplug validation
    // v0.9.3 (Audit Fix #3): Sparkplug check moved to server-only.
    // On client, m_SourceOn is a SyncVar set exclusively by the server
    // after sparkplug validation. Re-checking on client introduces a
    // race condition with attachment replication — sparkplug may not
    // have synced yet, causing false-negative that makes cables grey.
    // Consistent with v0.7.47 pattern: OnSwitchOn/OnWorkStart/OnWork
    // already have sparkplug validation as #ifdef SERVER only.
    bool LFPG_GetSourceOn()
    {
        if (!m_SourceOn)
        {
            return false;
        }

        #ifdef SERVER
        if (!LFPG_DeviceLifecycle.IsSparkPlugValid(this))
        {
            return false;
        }
        #endif

        return true;
    }

    // v0.7.23b: Raw switch state (true = player turned it on).
    // Used for UI feedback to distinguish "OFF" vs "ON but not producing".
    bool LFPG_GetSwitchState()
    {
        return m_SourceOn;
    }

    void LFPG_SetPowered(bool powered)
    {
        // Source device ignores SetPowered (it generates, doesn't consume)
    }

    // v0.7.8: Energy capacity (units/s this source can deliver)
    float LFPG_GetCapacity()
    {
        return LFPG_DEFAULT_SOURCE_CAPACITY;  // 50.0 units/s
    }

    // v0.7.38 (BugFix): Expose powered state for status display.
    bool LFPG_IsPowered()
    {
        return LFPG_GetSourceOn();
    }

    // v0.7.8: Current load ratio (synced to clients for cable state visuals)
    float LFPG_GetLoadRatio()
    {
        return m_LoadRatio;
    }

    // v0.7.8: Set load ratio (called by PropagateFrom on server)
    void LFPG_SetLoadRatio(float ratio)
    {
        #ifdef SERVER
        if (ratio < 0.0)
        {
            ratio = 0.0;
        }

        // Only sync if changed significantly (avoid needless network traffic)
        float diff = ratio - m_LoadRatio;
        if (diff < 0.0)
        {
            diff = -diff;
        }
        if (diff > 0.01)
        {
            m_LoadRatio = ratio;
            SetSynchDirty();
            string lrMsg = "[LFPG_Generator] LoadRatio=" + m_LoadRatio.ToString() + " id=" + m_DeviceId;
            LFPG_Util.Debug(lrMsg);
        }
        #endif
    }

    // v0.7.8: Overload bitmask (which output wires exceed capacity)
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


    bool LFPG_CanConnectTo(Object other, string myPort, string otherPort)
    {
        if (!other) return false;
        if (!LFPG_HasPort(myPort, LFPG_PortDir.OUT)) return false;

        EntityAI otherEntity = EntityAI.Cast(other);
        if (!otherEntity) return false;

        string otherId = LFPG_DeviceAPI.GetDeviceId(otherEntity);
        if (otherId != "")
        {
            return LFPG_DeviceAPI.HasPort(other, otherPort, LFPG_PortDir.IN);
        }

        return LFPG_DeviceAPI.IsEnergyConsumer(otherEntity);
    }

    // ============================================
    // Wire ownership API (delegates to WireHelper)
    // ============================================
    bool LFPG_HasWireStore()
    {
        return true;
    }

    array<ref LFPG_WireData> LFPG_GetWires()
    {
        return m_Wires;
    }

    string LFPG_GetWiresJSON()
    {
        return LFPG_WireHelper.GetJSON(m_Wires);
    }

    bool LFPG_AddWire(LFPG_WireData wd)
    {
        if (!wd) return false;

        // v0.7.32 (Audit): Port validation (defense-in-depth).
        // The RPC handler validates before calling, but this guard protects
        // against future callers (migration, self-heal, other mods).
        if (wd.m_SourcePort == "")
            wd.m_SourcePort = LFPG_PORT_OUTPUT_1;

        if (!LFPG_HasPort(wd.m_SourcePort, LFPG_PortDir.OUT))
        {
            string awMsg = "[LFPG_Generator] AddWire rejected: invalid port: " + wd.m_SourcePort;
            LFPG_Util.Warn(awMsg);
            return false;
        }

        bool result = LFPG_WireHelper.AddWire(m_Wires, wd);
        if (result)
        {
            #ifdef SERVER
            SetSynchDirty();
            #endif
        }
        return result;
    }

    bool LFPG_ClearWires()
    {
        bool result = LFPG_WireHelper.ClearAll(m_Wires);
        if (result)
        {
            #ifdef SERVER
            SetSynchDirty();
            #endif
        }
        return result;
    }

    bool LFPG_ClearWiresForCreator(string creatorId)
    {
        bool result = LFPG_WireHelper.ClearForCreator(m_Wires, creatorId);
        if (result)
        {
            #ifdef SERVER
            SetSynchDirty();
            #endif
        }
        return result;
    }

    bool LFPG_PruneMissingTargets()
    {
        ref map<string, bool> validIds = LFPG_NetworkManager.Get().GetCachedValidIds();
        if (!validIds)
        {
            validIds = new map<string, bool>;
            array<EntityAI> all = new array<EntityAI>;
            LFPG_DeviceRegistry.Get().GetAll(all);
            int vi;
            for (vi = 0; vi < all.Count(); vi = vi + 1)
            {
                string did = LFPG_DeviceAPI.GetOrCreateDeviceId(all[vi]);
                if (did != "")
                {
                    validIds[did] = true;
                }
            }
        }

        bool result = LFPG_WireHelper.PruneMissingTargets(m_Wires, validIds);
        if (result)
        {
            #ifdef SERVER
            SetSynchDirty();
            #endif
        }
        return result;
    }

    // ============================================
    // Persistence
    // ============================================
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);

        ctx.Write(m_DeviceIdLow);
        ctx.Write(m_DeviceIdHigh);
        ctx.Write(m_SourceOn);

        string json;
        LFPG_WireHelper.SerializeJSON(m_Wires, json);
        ctx.Write(json);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version))
            return false;

        if (!ctx.Read(m_DeviceIdLow))
        {
            string slErrLow = "OnStoreLoad: failed to read m_DeviceIdLow";
            LFPG_Util.Error(slErrLow);
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            string slErrHigh = "OnStoreLoad: failed to read m_DeviceIdHigh";
            LFPG_Util.Error(slErrHigh);
            return false;
        }

        LFPG_UpdateDeviceIdString();

        if (!ctx.Read(m_SourceOn))
        {
            string slErrSrc = "OnStoreLoad: failed to read m_SourceOn for " + m_DeviceId;
            LFPG_Util.Error(slErrSrc);
            return false;
        }

        string json;
        if (!ctx.Read(json))
        {
            string slErrJson = "OnStoreLoad: failed to read wires json for " + m_DeviceId;
            LFPG_Util.Error(slErrJson);
            return false;
        }
        LFPG_WireHelper.DeserializeJSON(m_Wires, json, "LFPG_Generator");

        return true;
    }

    // ============================================
    // Source toggle
    // ============================================
    // v0.7.27: Uses centralized sparkplug validation
    void LFPG_ToggleSource()
    {
        #ifdef SERVER
        if (m_SourceOn)
        {
            m_SourceOn = false;
            SetSynchDirty();

            ComponentEnergyManager emOff = GetCompEM();
            if (emOff)
            {
                emOff.SwitchOff();
            }
        }
        else
        {
            if (!LFPG_DeviceLifecycle.IsSparkPlugValid(this))
            {
                string togBlk = "[LFPG_Generator] Toggle ON blocked: invalid SparkPlug id=" + m_DeviceId;
                LFPG_Util.Info(togBlk);
                return;
            }

            m_SourceOn = true;
            SetSynchDirty();

            ComponentEnergyManager emOn = GetCompEM();
            if (emOn)
            {
                emOn.SwitchOn();
            }
        }

        string genDbg = "Generator " + m_DeviceId + " m_SourceOn=" + m_SourceOn.ToString() + " GetSourceOn=" + LFPG_GetSourceOn().ToString();
        LFPG_Util.Info(genDbg);
        LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
        #endif
    }

    // v0.7.23 (Bug 5): Prevent picking up / moving placed generators.
    // v0.7.38: Vanilla carry/cargo restored.
    // Picking up breaks wire connections (position-based IDs).

    // v0.7.27: Delegates to DeviceLifecycle for movement detection.
    override void EEItemLocationChanged(notnull InventoryLocation oldLoc, notnull InventoryLocation newLoc)
    {
        super.EEItemLocationChanged(oldLoc, newLoc);

        #ifdef SERVER
        // v0.7.45 (H9): Guard consistent with Splitter. OnDeviceMoved has
        // its own empty-check, but defense-in-depth prevents entering
        // lifecycle logic before DeviceId is assigned.
        if (m_DeviceId == "")
            return;
        bool wiresCut = LFPG_DeviceLifecycle.OnDeviceMoved(this, m_DeviceId, oldLoc, newLoc);
        if (wiresCut)
        {
            // Generator-specific: force off if running
            if (m_SourceOn)
            {
                m_SourceOn = false;
                SetSynchDirty();
                ComponentEnergyManager emPickup = GetCompEM();
                if (emPickup)
                {
                    emPickup.SwitchOff();
                }
            }
        }
        #endif
    }
};

// =========================================================
// LF_TestLamp - consumer (input_main) + visible client light
// =========================================================

class LF_TestLamp : Spotlight
{
    protected int m_DeviceIdLow = 0;

    protected int m_DeviceIdHigh = 0;

    protected string m_DeviceId;

    protected bool m_PoweredNet = false;

    // v0.7.38 (RC-04): Deletion guard (same as Generator).
    protected bool m_LFPG_Deleting = false;

    // NOTE: Spotlight base class already defines `SpotlightLight m_Light;`
    // so we must not redeclare it here.
    // ScriptedLightBase is an engine object (not Managed). Do NOT store as `ref`.
    protected ScriptedLightBase m_LFPG_Light;

    // v0.7.30: Per-device position polling removed.
    // Movement detection is now centralized in NetworkManager.CheckDeviceMovement()
    // with round-robin batching (Audit 1+2 closure).

    void LF_TestLamp()
    {
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
    }

    override void SetActions()
    {
        super.SetActions();

        // v0.7.25: Remove vanilla on/off
        RemoveAction(ActionTurnOnSpotlight);
        RemoveAction(ActionTurnOffSpotlight);

        // v0.7.38: Defensive removal of other vanilla turn-on actions
        // that may be inherited from Spotlight's parent class chain.
        // Prevents "Turn On" appearing when lamp is held in hands.
        RemoveAction(ActionTurnOnWhileInHands);
        RemoveAction(ActionTurnOffWhileInHands);

        // v0.7.26 (Bug 3): Remove vanilla electrical connection actions.
        // Spotlight inherits these from its base class hierarchy.
        // Without explicit removal, players with vanilla CableReel near
        // an LF_TestLamp see "Plug in" / "Unplug" vanilla actions.
        RemoveAction(ActionPlugIn);
        RemoveAction(ActionUnplugThisByCord);

        // v0.7.38: Vanilla take/carry actions restored.
        // Picking up a wired lamp breaks its connections (position-based
        // IDs change). This is by design — data integrity over convenience.
    }

    // v0.7.26 (Bug 3): Prevent vanilla electrical system from treating
    // this as a pluggable appliance. Returns false to block all vanilla
    // plug/unplug action condition checks from the item side.
    override bool IsElectricAppliance()
    {
        return false;
    }

    // ---- Block vanilla Spotlight CompEM hooks ----
    // LFPG manages light exclusively via m_PoweredNet.
    override void OnWorkStart()  { /* Blocked: LFPG handles power */ }
    override void OnWorkStop()   { /* Blocked */ }
    override void OnWork(float consumed_energy) { /* Blocked */ }
    override void OnSwitchOn()   { /* Blocked */ }
    override void OnSwitchOff()  { /* Blocked */ }

    override void EEInit()
    {
        super.EEInit();

        // v0.7.29 (Audit fix): Force CompEM off on BOTH client+server.
        // LFPG manages power exclusively via m_PoweredNet.
        // Spotlight base class may activate CompEM via C++ EEInit paths.
        ComponentEnergyManager emInit = GetCompEM();
        if (emInit)
        {
            emInit.SwitchOff();
        }

        #ifdef SERVER
        if (m_DeviceIdLow == 0 && m_DeviceIdHigh == 0)
        {
            LFPG_Util.GenerateDeviceId(m_DeviceIdLow, m_DeviceIdHigh);
        }
        // v0.9.3 (Audit Fix #2): Unconditional SetSynchDirty for persistence load.
        // Ensures clients receive m_DeviceIdLow/High and m_PoweredNet even
        // if engine doesn't send automatic SyncVar burst for persisted entities.
        SetSynchDirty();
        #endif

        LFPG_UpdateDeviceIdString();
        LFPG_TryRegister();
    }

    // v0.7.27: Delegates to DeviceLifecycle helper
    override void EEDelete(EntityAI parent)
    {
        // v0.7.38 (RC-04): Set deletion flag BEFORE unregistration.
        m_LFPG_Deleting = true;

        LFPG_DeviceLifecycle.OnDeviceDeleted(this, m_DeviceId);

        #ifndef SERVER
        LFPG_DestroyLight();
        #endif

        super.EEDelete(parent);
    }

    // v0.7.26 (Bug 2): Cut all wires when lamp is destroyed.
    // v0.7.27: Delegates to DeviceLifecycle, then handles lamp-specific state.
    override void EEKilled(Object killer)
    {
        LFPG_DeviceLifecycle.OnDeviceKilled(this, m_DeviceId);

        #ifdef SERVER
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        #endif

        #ifndef SERVER
        LFPG_DestroyLight();
        #endif

        super.EEKilled(killer);
    }

    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();
        LFPG_TryRegister();

        #ifndef SERVER
        bool hasLight = (m_LFPG_Light != null);
        string vsMsg = "[LF_TestLamp] OnVarSync powered=" + m_PoweredNet.ToString() + " id=" + m_DeviceId + " hasLight=" + hasLight.ToString();
        LFPG_Util.Debug(vsMsg);

        if (m_PoweredNet)
        {
            LFPG_CreateLight();
        }
        else
        {
            LFPG_DestroyLight();
        }

        // v0.7.35 D1: Consumer entered bubble — request sync so owner wires
        // targeting this device become visible. Cooldown-throttled per deviceId.
        if (m_DeviceId != "")
        {
            LFPG_CableRenderer r = LFPG_CableRenderer.Get();
            if (r)
            {
                // v0.7.45 (H7): Pass entity for NetworkID-first resolution.
                r.RequestDeviceSync(m_DeviceId, this);
            }
        }
        #endif
    }

    protected void LFPG_UpdateDeviceIdString()
    {
        m_DeviceId = LFPG_Util.MakeDeviceKey(m_DeviceIdLow, m_DeviceIdHigh);
    }

    protected void LFPG_TryRegister()
    {
        // v0.7.38 (RC-04): Don't register if device is being deleted.
        if (m_LFPG_Deleting)
            return;

        // v0.7.45 (Patch 4C): Capture old ID before recalculating.
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
    // LFPG_IDevice interface
    // ============================================
    string LFPG_GetDeviceId()
    {
        return m_DeviceId;
    }

    int LFPG_GetPortCount()
    {
        return 1;
    }

    string LFPG_GetPortName(int idx)
    {
        if (idx == 0) return "input_main";
        return "";
    }

    int LFPG_GetPortDir(int idx)
    {
        if (idx == 0) return LFPG_PortDir.IN;
        return -1;
    }

    string LFPG_GetPortLabel(int idx)
    {
        if (idx == 0) return "Input 1";
        return "";
    }

    bool LFPG_IsSource()
    {
        return false;
    }

    // Sprint 4.1: Electrical graph node type
    int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.CONSUMER;
    }

    bool LFPG_GetSourceOn()
    {
        return false;
    }

    bool LFPG_CanConnectTo(Object other, string myPort, string otherPort)
    {
        return false;
    }

    bool LFPG_HasPort(string portName, int dir)
    {
        if (dir == LFPG_PortDir.IN && portName == "input_main") return true;
        return false;
    }

    vector LFPG_GetPortWorldPos(string portName)
    {
        vector p = GetPosition();
        p[1] = p[1] + 1.0;
        return p;
    }

    void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        string spMsg = "[LF_TestLamp] LFPG_SetPowered(" + powered.ToString() + ") current=" + m_PoweredNet.ToString() + " id=" + m_DeviceId;
        LFPG_Util.Debug(spMsg);

        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;
        SetSynchDirty();

        string pnMsg = "[LF_TestLamp] m_PoweredNet set to " + m_PoweredNet.ToString();
        LFPG_Util.Debug(pnMsg);
        #endif
    }

    // v0.7.8: Energy consumption rate (units/s)
    float LFPG_GetConsumption()
    {
        return LFPG_DEFAULT_CONSUMER_CONSUMPTION;  // 10.0 units/s
    }

    // v0.7.38 (BugFix): Expose powered state for status display.
    // m_PoweredNet is SyncVar so available on both client and server.
    bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    // ============================================
    // Persistence
    // ============================================
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);
        ctx.Write(m_DeviceIdLow);
        ctx.Write(m_DeviceIdHigh);
        // v0.7.42: m_PoweredNet removed from persistence.
        // It is a derived state from the electrical graph — only
        // propagation should set it. Field default (false) is correct.
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version))
            return false;

        if (!ctx.Read(m_DeviceIdLow))
        {
            string tlErrLow = "LF_TestLamp.OnStoreLoad: failed to read m_DeviceIdLow";
            LFPG_Util.Error(tlErrLow);
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            string tlErrHigh = "LF_TestLamp.OnStoreLoad: failed to read m_DeviceIdHigh";
            LFPG_Util.Error(tlErrHigh);
            return false;
        }

        LFPG_UpdateDeviceIdString();

        // v0.7.42: m_PoweredNet no longer persisted.
        // Field default (false) is correct; propagation re-derives it.

        return true;
    }

    // ============================================
    // Client visuals
    // ============================================
    protected void LFPG_CreateLight()
    {
        if (m_LFPG_Light)
        {
            string clExMsg = "[LF_TestLamp] CreateLight: already exists";
            LFPG_Util.Info(clExMsg);
            return;
        }

        vector lightPos = GetPosition();
        lightPos[1] = lightPos[1] + 1.1;

        m_LFPG_Light = ScriptedLightBase.CreateLight(LFPG_LampLight, lightPos);
        if (m_LFPG_Light)
        {
            m_LFPG_Light.AttachOnObject(this, "0 1.1 0");
            m_LFPG_Light.SetLifetime(1000000);
            bool bEnable = true;
            m_LFPG_Light.SetEnabled(bEnable);

            string clOkMsg = "[LF_TestLamp] CreateLight: OK at " + lightPos.ToString();
            LFPG_Util.Info(clOkMsg);
        }
        else
        {
            string clFailMsg = "[LF_TestLamp] CreateLight: ScriptedLightBase.CreateLight FAILED";
            LFPG_Util.Error(clFailMsg);
        }
    }

    protected void LFPG_DestroyLight()
    {
        if (!m_LFPG_Light)
            return;

        string dlMsg = "[LF_TestLamp] DestroyLight: removing light";
        LFPG_Util.Info(dlMsg);
        m_LFPG_Light.FadeOut();
        m_LFPG_Light = null;
    }

    // v0.7.23 (Bug 5): Prevent picking up / moving placed lamps.
    // v0.7.38: Vanilla carry/cargo restored.
    // Picking up breaks wire connections (position-based IDs).

    // v0.7.27: Delegates to DeviceLifecycle for movement detection.
    override void EEItemLocationChanged(notnull InventoryLocation oldLoc, notnull InventoryLocation newLoc)
    {
        super.EEItemLocationChanged(oldLoc, newLoc);

        #ifdef SERVER
        // v0.7.45 (H9): Guard consistent with Splitter. OnDeviceMoved has
        // its own empty-check, but defense-in-depth prevents entering
        // lifecycle logic before DeviceId is assigned.
        if (m_DeviceId == "")
            return;
        bool wiresCut = LFPG_DeviceLifecycle.OnDeviceMoved(this, m_DeviceId, oldLoc, newLoc);
        if (wiresCut)
        {
            if (m_PoweredNet)
            {
                m_PoweredNet = false;
                SetSynchDirty();
            }
        }
        #endif
    }
};

// =========================================================
// LF_TestLampHeavy - high-consumption consumer for load testing (v0.7.8)
// Identical to LF_TestLamp except consumes 50 units/s (vs 10).
// One of these on a 50/s generator = 100% load = CRITICAL.
// =========================================================

class LF_TestLampHeavy : LF_TestLamp
{
    // Override consumption to 50.0 units/s (same as generator capacity)
    override float LFPG_GetConsumption()
    {
        return 50.0;
    }
};
