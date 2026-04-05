// =========================================================
// LF_PowerGrid - Device Base Class (v4.0, Refactor)
//
// Base class for ALL 26 production LFPG devices.
// Centralizes: SyncVar registration (DeviceId), lifecycle hooks,
// persistence (with per-device version), inventory guards,
// CompEM blocking, port system, and DeviceAPI virtual interface.
//
// Hierarchy:
//   Inventory_Base (DayZ vanilla)
//     └── LFPG_DeviceBase          ← this
//           └── LFPG_WireOwnerBase ← devices with wire store
//
// Subclass pattern:
//   - CONSUMER (no wire store): extend LFPG_DeviceBase directly
//   - PASSTHROUGH/SOURCE (wire store): extend LFPG_WireOwnerBase
//
// Hook chain (each level calls the next at the end):
//   DeviceBase      → LFPG_OnInit()       → LFPG_OnVarSync()
//   WireOwnerBase   → LFPG_OnInitDevice() → LFPG_OnVarSyncDevice()
//   Concrete device → (implements hooks)
//
// Persistence format (v3):
//   [Inventory_Base super data]
//   [m_DeviceIdLow : int]
//   [m_DeviceIdHigh : int]
//   [devicePersistVer : int]
//   [...device extras via LFPG_OnStoreSaveExtra hook...]
//
// v4.0: Initial implementation. Replaces per-device boilerplate.
// =========================================================

class LFPG_DeviceBase : Inventory_Base
{
    // ---- SyncVars ----
    protected int    m_DeviceIdLow   = 0;
    protected int    m_DeviceIdHigh  = 0;

    // ---- Local derived state ----
    protected string m_DeviceId      = "";
    protected bool   m_LFPG_Deleting = false;

    // ---- Port system ----
    protected ref array<ref LFPG_PortDef> m_Ports;

    // ---- Hologram projection guard (v4.4) ----
    // Static flag: set by HologramMod.ProjectionBasedOnParent() just before
    // CreateObjectEx spawns the projection entity. Checked + cleared in EEInit.
    // Prevents hologram projections from registering as real LFPG devices.
    static bool s_LFPG_SkipHologramInit;
    protected bool m_LFPG_IsHologramProjection;

    static void LFPG_FlagHologramCreation()
    {
        s_LFPG_SkipHologramInit = true;
    }

    static void LFPG_ClearHologramFlag()
    {
        s_LFPG_SkipHologramInit = false;
    }

    // ============================================
    // Constructor
    // ============================================
    void LFPG_DeviceBase()
    {
        string varLow  = "m_DeviceIdLow";
        string varHigh = "m_DeviceIdHigh";
        RegisterNetSyncVariableInt(varLow);
        RegisterNetSyncVariableInt(varHigh);

        m_Ports = new array<ref LFPG_PortDef>;
    }

    // ============================================
    // Port system helpers
    // ============================================
    void LFPG_AddPort(string name, int dir, string label)
    {
        LFPG_PortDef pd = new LFPG_PortDef();
        pd.m_Name  = name;
        pd.m_Dir   = dir;
        pd.m_Label = label;
        m_Ports.Insert(pd);
    }

    int LFPG_GetPortCount()
    {
        return m_Ports.Count();
    }

    string LFPG_GetPortName(int idx)
    {
        if (idx < 0 || idx >= m_Ports.Count())
            return "";
        return m_Ports[idx].m_Name;
    }

    int LFPG_GetPortDir(int idx)
    {
        if (idx < 0 || idx >= m_Ports.Count())
            return -1;
        return m_Ports[idx].m_Dir;
    }

    string LFPG_GetPortLabel(int idx)
    {
        if (idx < 0 || idx >= m_Ports.Count())
            return "";
        return m_Ports[idx].m_Label;
    }

    bool LFPG_HasPort(string name, int dir)
    {
        int i;
        for (i = 0; i < m_Ports.Count(); i = i + 1)
        {
            LFPG_PortDef pd = m_Ports[i];
            if (pd.m_Name == name && pd.m_Dir == dir)
                return true;
        }
        return false;
    }

    vector LFPG_GetPortWorldPos(string portName)
    {
        // Primary: "port_input_1", "port_output_1"
        string memPoint = "port_" + portName;
        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

        // Compact fallback: "port_input1" vs "port_input_1"
        int len = portName.Length();
        if (len >= 3)
        {
            string lastChar   = portName.Substring(len - 1, 1);
            string beforeLast = portName.Substring(len - 2, 1);
            if (beforeLast == "_")
            {
                string compact = "port_";
                compact = compact + portName.Substring(0, len - 2);
                compact = compact + lastChar;
                if (MemoryPointExists(compact))
                {
                    return ModelToWorld(GetMemoryPointPos(compact));
                }
            }
        }

        // Bare fallback: just the port name
        if (MemoryPointExists(portName))
        {
            return ModelToWorld(GetMemoryPointPos(portName));
        }

        string warnMsg = "[LFPG_DeviceBase] Missing memory point for port: ";
        warnMsg = warnMsg + portName;
        warnMsg = warnMsg + " on ";
        warnMsg = warnMsg + GetType();
        LFPG_Util.Warn(warnMsg);
        vector p = GetPosition();
        p[1] = p[1] + 0.3;
        return p;
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
    // Lifecycle: EEInit
    // ============================================
    override void EEInit()
    {
        super.EEInit();

        // v4.4: Hologram projection guard.
        // When Hologram constructor spawns a projection entity of a device
        // type, skip all LFPG init (DeviceId, registry, NM registration).
        if (s_LFPG_SkipHologramInit)
        {
            s_LFPG_SkipHologramInit = false;
            m_LFPG_IsHologramProjection = true;
            return;
        }

        #ifdef SERVER
        if (m_DeviceIdLow == 0 && m_DeviceIdHigh == 0)
        {
            LFPG_Util.GenerateDeviceId(m_DeviceIdLow, m_DeviceIdHigh);
        }
        SetSynchDirty();
        #endif

        LFPG_UpdateDeviceIdString();
        LFPG_TryRegister();
        // Future: LFPG_SpatialGrid.Get().Insert(this)

        LFPG_OnInit();
    }

    // ============================================
    // Lifecycle: EEKilled
    // ============================================
    override void EEKilled(Object killer)
    {
        if (m_LFPG_IsHologramProjection)
        {
            super.EEKilled(killer);
            return;
        }

        LFPG_DeviceLifecycle.OnDeviceKilled(this, m_DeviceId);
        LFPG_OnKilled();
        super.EEKilled(killer);
    }

    // ============================================
    // Lifecycle: EEDelete
    // ============================================
    override void EEDelete(EntityAI parent)
    {
        if (m_LFPG_IsHologramProjection)
        {
            super.EEDelete(parent);
            return;
        }

        m_LFPG_Deleting = true;
        LFPG_OnDeleted();
        // Future: LFPG_SpatialGrid.Get().Remove(this)
        LFPG_DeviceLifecycle.OnDeviceDeleted(this, m_DeviceId);
        super.EEDelete(parent);
    }

    // ============================================
    // Lifecycle: EEItemLocationChanged
    // ============================================
    override void EEItemLocationChanged(notnull InventoryLocation oldLoc, notnull InventoryLocation newLoc)
    {
        super.EEItemLocationChanged(oldLoc, newLoc);

        #ifdef SERVER
        if (m_DeviceId == "")
            return;

        bool wiresCut = LFPG_DeviceLifecycle.OnDeviceMoved(this, m_DeviceId, oldLoc, newLoc);
        if (wiresCut)
        {
            LFPG_OnWiresCut();
        }
        #endif
    }

    // ============================================
    // OnVariablesSynchronized
    // RequestDeviceSync at this level because ALL devices need it.
    // ============================================
    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();

        if (m_LFPG_IsHologramProjection)
            return;

        LFPG_TryRegister();

        #ifndef SERVER
        if (m_DeviceId != "")
        {
            LFPG_CableRenderer r = LFPG_CableRenderer.Get();
            if (r)
            {
                r.RequestDeviceSync(m_DeviceId, this);
            }
        }
        #endif

        LFPG_OnVarSync();
    }

    // ============================================
    // Persistence (v3 format with per-device version)
    // ============================================
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);
        ctx.Write(m_DeviceIdLow);
        ctx.Write(m_DeviceIdHigh);

        int ver = LFPG_GetDevicePersistVersion();
        ctx.Write(ver);

        LFPG_OnStoreSaveExtra(ctx);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version))
            return false;

        if (!ctx.Read(m_DeviceIdLow))
        {
            string errLow = "[LFPG_DeviceBase] OnStoreLoad failed: m_DeviceIdLow on " + GetType();
            LFPG_Util.Error(errLow);
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            string errHigh = "[LFPG_DeviceBase] OnStoreLoad failed: m_DeviceIdHigh on " + GetType();
            LFPG_Util.Error(errHigh);
            return false;
        }

        LFPG_UpdateDeviceIdString();

        int deviceVer = 0;
        if (!ctx.Read(deviceVer))
        {
            string errVer = "[LFPG_DeviceBase] OnStoreLoad failed: deviceVer on " + GetType();
            LFPG_Util.Error(errVer);
            return false;
        }

        return LFPG_OnStoreLoadExtra(ctx, deviceVer);
    }

    // ============================================
    // Inventory guards
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

    override void SetActions()
    {
        super.SetActions();
        RemoveAction(ActionTakeItem);
        RemoveAction(ActionTakeItemToHands);
    }

    // ============================================
    // CompEM block (prevent vanilla interference)
    // ============================================
    override bool IsElectricAppliance()
    {
        return false;
    }

    override void OnWorkStart() {}
    override void OnWorkStop() {}
    override void OnWork(float consumed_energy) {}
    override void OnSwitchOn() {}
    override void OnSwitchOff() {}

    // ============================================
    // Virtual interface (fast path for DeviceAPI)
    // ============================================
    string LFPG_GetDeviceId()
    {
        return m_DeviceId;
    }

    int LFPG_GetDeviceIdLow()
    {
        return m_DeviceIdLow;
    }

    int LFPG_GetDeviceIdHigh()
    {
        return m_DeviceIdHigh;
    }

    int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.CONSUMER;
    }

    float LFPG_GetConsumption()
    {
        return 0.0;
    }

    float LFPG_GetCapacity()
    {
        return 0.0;
    }

    bool LFPG_IsSource()
    {
        return false;
    }

    bool LFPG_GetSourceOn()
    {
        return false;
    }

    void LFPG_SetPowered(bool powered)
    {
    }

    bool LFPG_IsPowered()
    {
        return false;
    }

    float LFPG_GetLoadRatio()
    {
        return 0.0;
    }

    void LFPG_SetLoadRatio(float ratio)
    {
    }

    bool LFPG_GetOverloaded()
    {
        return false;
    }

    void LFPG_SetOverloaded(bool val)
    {
    }

    bool LFPG_IsGateCapable()
    {
        return false;
    }

    bool LFPG_IsGateOpen()
    {
        return true;
    }

    bool LFPG_HasWireStore()
    {
        return false;
    }

    bool LFPG_CanConnectTo(Object other, string myPort, string otherPort)
    {
        return false;
    }

    // ---- RF capability (v4.7) ----
    bool LFPG_IsRFCapable()
    {
        return false;
    }

    bool LFPG_RemoteToggle()
    {
        return false;
    }

    // ============================================
    // Hooks (empty — subclass overrides)
    // ============================================
    void LFPG_OnInit() {}
    void LFPG_OnKilled() {}
    void LFPG_OnDeleted() {}
    void LFPG_OnVarSync() {}
    void LFPG_OnWiresCut() {}
    void LFPG_OnStoreSaveExtra(ParamsWriteContext ctx) {}
    bool LFPG_OnStoreLoadExtra(ParamsReadContext ctx, int ver) { return true; }
    int  LFPG_GetDevicePersistVersion() { return 1; }

    // ============================================
    // Dismantle support (v4.5)
    // Returns kit classname to spawn on dismantle.
    // Default: GetType() + "_Kit" (convention).
    // Override to return "" for non-dismantlable devices
    // (T2 upgrades, BatteryAdapter, etc.)
    // ============================================
    string LFPG_GetKitClassname()
    {
        string kitClass = GetType();
        kitClass = kitClass + "_Kit";
        return kitClass;
    }
};
