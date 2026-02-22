// =========================================================
// LF_PowerGrid - Example devices (v0.7.14)
// - LF_TestGenerator: source (output_1..4) + owns wires + persistence
// - LF_TestLamp: consumer (input_main) + visible client light
//
// v0.7.14: Generator LFPG_GetSourceOn uses m_SourceOn only (no em.IsWorking fallback).
//          Generator EEInit syncs CompEM with m_SourceOn.
//          Lamp EEInit forces CompEM off (LFPG manages via m_PoweredNet).
//
// Wire manipulation delegated to LFPG_WireHelper (3_Game).
// =========================================================

class LF_TestGenerator : PowerGenerator
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
    protected int m_OverloadMask = 0;

    void LF_TestGenerator()
    {
        m_Wires = new array<ref LFPG_WireData>;
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_SourceOn");
        RegisterNetSyncVariableFloat("m_LoadRatio", 0.0, 5.0, 2);
        RegisterNetSyncVariableInt("m_OverloadMask");
    }

    override void SetActions()
    {
        super.SetActions();
        AddAction(ActionLFPG_ToggleSource);
    }

    override void EEInit()
    {
        super.EEInit();

        #ifdef SERVER
        if (m_DeviceIdLow == 0 && m_DeviceIdHigh == 0)
        {
            LFPG_Util.GenerateDeviceId(m_DeviceIdLow, m_DeviceIdHigh);
            SetSynchDirty();
        }
        #endif

        LFPG_UpdateDeviceIdString();
        LFPG_TryRegister();

        // v0.7.14: Force CompEM to match m_SourceOn after init/load.
        // Prevents vanilla CompEM auto-starting when generator has fuel,
        // which caused LFPG_GetSourceOn desync before the fallback was removed.
        #ifdef SERVER
        ComponentEnergyManager emInit = GetCompEM();
        if (emInit)
        {
            if (m_SourceOn)
            {
                emInit.SwitchOn();
            }
            else
            {
                emInit.SwitchOff();
            }
        }
        #endif

        #ifdef SERVER
        LFPG_NetworkManager.Get().BroadcastOwnerWires(this);

        if (m_SourceOn)
            LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
        #endif
    }

    override void EEDelete(EntityAI parent)
    {
        // Sprint 4.1: notify graph before unregistering
        LFPG_NetworkManager.Get().NotifyGraphDeviceRemoved(m_DeviceId);
        LFPG_DeviceRegistry.Get().Unregister(m_DeviceId, this);
        LFPG_NetworkManager.Get().RequestGlobalSelfHeal();
        super.EEDelete(parent);
    }

    // Vanilla CompEM hooks: detect when user toggles via vanilla action
    override void OnWorkStart()
    {
        super.OnWorkStart();

        #ifdef SERVER
        m_SourceOn = true;
        SetSynchDirty();

        LFPG_Util.Info("[LF_TestGenerator] OnWorkStart id=" + m_DeviceId + " SourceOn=true");
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

        LFPG_Util.Info("[LF_TestGenerator] OnWorkStop id=" + m_DeviceId + " SourceOn=false");
        if (m_DeviceId != "")
        {
            LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
        }
        #endif
    }

    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();
        LFPG_TryRegister();
    }

    protected void LFPG_UpdateDeviceIdString()
    {
        m_DeviceId = LFPG_Util.MakeDeviceKey(m_DeviceIdLow, m_DeviceIdHigh);
    }

    protected void LFPG_TryRegister()
    {
        LFPG_UpdateDeviceIdString();
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

    // v0.7.14: m_SourceOn is the sole authority.
    // OnWorkStart/OnWorkStop already sync it with CompEM state.
    // The old em.IsWorking() fallback caused false positives when
    // vanilla CompEM auto-started a generator with fuel on spawn.
    bool LFPG_GetSourceOn()
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
            LFPG_Util.Debug("[LF_TestGenerator] LoadRatio=" + m_LoadRatio.ToString() + " id=" + m_DeviceId);
        }
        #endif
    }

    // v0.7.8: Overload bitmask (which output wires exceed capacity)
    int LFPG_GetOverloadMask()
    {
        return m_OverloadMask;
    }

    void LFPG_SetOverloadMask(int mask)
    {
        #ifdef SERVER
        if (m_OverloadMask != mask)
        {
            m_OverloadMask = mask;
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
        // Use cached map from NetworkManager if available (during self-heal),
        // otherwise build our own (standalone calls like wire creation).
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
            LFPG_Util.Error("OnStoreLoad: failed to read m_DeviceIdLow");
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            LFPG_Util.Error("OnStoreLoad: failed to read m_DeviceIdHigh");
            return false;
        }

        LFPG_UpdateDeviceIdString();

        if (!ctx.Read(m_SourceOn))
        {
            LFPG_Util.Error("OnStoreLoad: failed to read m_SourceOn for " + m_DeviceId);
            return false;
        }

        string json;
        if (!ctx.Read(json))
        {
            LFPG_Util.Error("OnStoreLoad: failed to read wires json for " + m_DeviceId);
            return false;
        }
        LFPG_WireHelper.DeserializeJSON(m_Wires, json, "LF_TestGenerator");

        return true;
    }

    // ============================================
    // Source toggle
    // ============================================
    void LFPG_ToggleSource()
    {
        #ifdef SERVER
        m_SourceOn = !m_SourceOn;
        SetSynchDirty();

        ComponentEnergyManager em = GetCompEM();
        if (em)
        {
            if (m_SourceOn)
            {
                em.SwitchOn();
            }
            else
            {
                em.SwitchOff();
            }
        }

        LFPG_Util.Info("Generator " + m_DeviceId + " SourceOn=" + m_SourceOn.ToString());
        LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
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

    // NOTE: Spotlight base class already defines `SpotlightLight m_Light;`
    // so we must not redeclare it here.
    // ScriptedLightBase is an engine object (not Managed). Do NOT store as `ref`.
    protected ScriptedLightBase m_LFPG_Light;

    void LF_TestLamp()
    {
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
    }

    override void SetActions()
    {
        super.SetActions();
        RemoveAction(ActionTurnOnSpotlight);
        RemoveAction(ActionTurnOffSpotlight);
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

        #ifdef SERVER
        if (m_DeviceIdLow == 0 && m_DeviceIdHigh == 0)
        {
            LFPG_Util.GenerateDeviceId(m_DeviceIdLow, m_DeviceIdHigh);
            SetSynchDirty();
        }

        // v0.7.14: Force CompEM off on init. LFPG manages power
        // exclusively via m_PoweredNet. Without this, vanilla Spotlight
        // auto-starts CompEM when placed, interfering with LFPG state.
        ComponentEnergyManager emInit = GetCompEM();
        if (emInit)
        {
            emInit.SwitchOff();
        }
        #endif

        LFPG_UpdateDeviceIdString();
        LFPG_TryRegister();
    }

    override void EEDelete(EntityAI parent)
    {
        // Sprint 4.1: notify graph before unregistering
        LFPG_NetworkManager.Get().NotifyGraphDeviceRemoved(m_DeviceId);
        LFPG_DeviceRegistry.Get().Unregister(m_DeviceId, this);
        LFPG_NetworkManager.Get().RequestGlobalSelfHeal();

        #ifndef SERVER
        LFPG_DestroyLight();
        #endif

        super.EEDelete(parent);
    }

    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();
        LFPG_TryRegister();

        #ifndef SERVER
        bool hasLight = (m_LFPG_Light != null);
        LFPG_Util.Debug("[LF_TestLamp] OnVarSync powered=" + m_PoweredNet.ToString() + " id=" + m_DeviceId + " hasLight=" + hasLight.ToString());

        if (m_PoweredNet)
        {
            LFPG_CreateLight();
        }
        else
        {
            LFPG_DestroyLight();
        }
        #endif
    }

    protected void LFPG_UpdateDeviceIdString()
    {
        m_DeviceId = LFPG_Util.MakeDeviceKey(m_DeviceIdLow, m_DeviceIdHigh);
    }

    protected void LFPG_TryRegister()
    {
        LFPG_UpdateDeviceIdString();
        if (m_DeviceId != "")
            LFPG_DeviceRegistry.Get().Register(this, m_DeviceId);
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
        LFPG_Util.Debug("[LF_TestLamp] LFPG_SetPowered(" + powered.ToString() + ") current=" + m_PoweredNet.ToString() + " id=" + m_DeviceId);

        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;
        SetSynchDirty();

        LFPG_Util.Debug("[LF_TestLamp] m_PoweredNet set to " + m_PoweredNet.ToString());
        #endif
    }

    // v0.7.8: Energy consumption rate (units/s)
    float LFPG_GetConsumption()
    {
        return LFPG_DEFAULT_CONSUMER_CONSUMPTION;  // 10.0 units/s
    }

    // ============================================
    // Persistence
    // ============================================
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);
        ctx.Write(m_DeviceIdLow);
        ctx.Write(m_DeviceIdHigh);
        ctx.Write(m_PoweredNet);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version))
            return false;

        if (!ctx.Read(m_DeviceIdLow))
        {
            LFPG_Util.Error("LF_TestLamp.OnStoreLoad: failed to read m_DeviceIdLow");
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            LFPG_Util.Error("LF_TestLamp.OnStoreLoad: failed to read m_DeviceIdHigh");
            return false;
        }

        LFPG_UpdateDeviceIdString();

        if (!ctx.Read(m_PoweredNet))
        {
            LFPG_Util.Error("LF_TestLamp.OnStoreLoad: failed to read m_PoweredNet for " + m_DeviceId);
            return false;
        }

        return true;
    }

    // ============================================
    // Client visuals
    // ============================================
    protected void LFPG_CreateLight()
    {
        if (m_LFPG_Light)
        {
            LFPG_Util.Info("[LF_TestLamp] CreateLight: already exists");
            return;
        }

        vector lightPos = GetPosition();
        lightPos[1] = lightPos[1] + 1.1;

        m_LFPG_Light = ScriptedLightBase.CreateLight(LFPG_LampLight, lightPos);
        if (m_LFPG_Light)
        {
            m_LFPG_Light.AttachOnObject(this, "0 1.1 0");
            m_LFPG_Light.SetLifetime(1000000);
            m_LFPG_Light.SetEnabled(true);

            LFPG_Util.Info("[LF_TestLamp] CreateLight: OK at " + lightPos.ToString());
        }
        else
        {
            LFPG_Util.Error("[LF_TestLamp] CreateLight: ScriptedLightBase.CreateLight FAILED");
        }
    }

    protected void LFPG_DestroyLight()
    {
        if (!m_LFPG_Light)
            return;

        LFPG_Util.Info("[LF_TestLamp] DestroyLight: removing light");
        m_LFPG_Light.FadeOut();
        m_LFPG_Light = null;
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
