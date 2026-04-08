// =========================================================
// LF_PowerGrid - Wire Owner Base Class (v4.0, Refactor)
//
// Base class for ~15 devices that own wires (output side).
// Extends LFPG_DeviceBase with wire store, wire API,
// wire persistence (JSON), and CableRenderer sync.
//
// Hierarchy:
//   Inventory_Base → LFPG_DeviceBase → LFPG_WireOwnerBase
//
// Includes m_WireGeneration SyncVar (Sync Hardening layer A).
// Incremented on every wire mutation (server). Client compares
// against CableRenderer's cached generation for auto-resync.
//
// Hook chain:
//   DeviceBase.LFPG_OnInit()     → WireOwnerBase: BroadcastOwnerWires + LFPG_OnInitDevice()
//   DeviceBase.LFPG_OnVarSync()  → WireOwnerBase: NotifyOwnerVisualChanged + LFPG_OnVarSyncDevice()
//   DeviceBase.LFPG_OnStoreSaveExtra() → WireOwnerBase: wireJSON + LFPG_OnStoreSaveDevice()
//   DeviceBase.LFPG_OnStoreLoadExtra() → WireOwnerBase: wireJSON + LFPG_OnStoreLoadDevice()
//
// Persistence format (v3):
//   [Inventory_Base super]
//   [m_DeviceIdLow : int]
//   [m_DeviceIdHigh : int]
//   [devicePersistVer : int]
//   [wireJSON : string]       ← WireOwnerBase
//   [...device extras...]     ← LFPG_OnStoreSaveDevice hook
//
// v4.0: Initial implementation. Centralizes wire boilerplate
//   from Splitter, Combiner, CeilingLight, Monitor, SolarPanel, etc.
// =========================================================

class LFPG_WireOwnerBase : LFPG_DeviceBase
{
    // ---- Wire store ----
    protected ref array<ref LFPG_WireData> m_Wires;

    // ---- Sync Hardening A: wire generation counter ----
    // Incremented server-side on every wire mutation.
    // Client compares against CableRenderer cache for auto-resync.
    // NOT persisted — session-only, starts at 0 each restart.
    protected int m_WireGeneration = 0;

    // ============================================
    // Constructor
    // ============================================
    void LFPG_WireOwnerBase()
    {
        m_Wires = new array<ref LFPG_WireData>;

        string varWireGen = "m_WireGeneration";
        RegisterNetSyncVariableInt(varWireGen);
    }

    // ============================================
    // DeviceBase overrides
    // ============================================
    override bool LFPG_HasWireStore()
    {
        return true;
    }

    // ---- Init hook: broadcast wires + device init ----
    override void LFPG_OnInit()
    {
        #ifdef SERVER
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm) nm.BroadcastOwnerWires(this);
        #endif

        LFPG_OnInitDevice();
    }

    // ---- VarSync hook: notify CableRenderer + device sync ----
    override void LFPG_OnVarSync()
    {
        #ifndef SERVER
        if (m_DeviceId != "")
        {
            LFPG_CableRenderer r = LFPG_CableRenderer.Get();
            if (r)
            {
                if (r.HasOwnerData(m_DeviceId))
                {
                    r.NotifyOwnerVisualChanged(m_DeviceId);
                }
            }
        }
        #endif

        LFPG_OnVarSyncDevice();
    }

    // ---- Persistence: save wireJSON + device extras ----
    override void LFPG_OnStoreSaveExtra(ParamsWriteContext ctx)
    {
        string json;
        LFPG_WireHelper.SerializeJSON(m_Wires, json);
        ctx.Write(json);

        LFPG_OnStoreSaveDevice(ctx);
    }

    // ---- Persistence: load wireJSON + device extras ----
    override bool LFPG_OnStoreLoadExtra(ParamsReadContext ctx, int ver)
    {
        string json;
        if (!ctx.Read(json))
        {
            string errJson = "[LFPG_WireOwnerBase] OnStoreLoad failed: wireJSON on " + GetType();
            LFPG_Util.Error(errJson);
            return false;
        }

        string debugLabel = GetType();
        LFPG_WireHelper.DeserializeJSON(m_Wires, json, debugLabel);

        return LFPG_OnStoreLoadDevice(ctx, ver);
    }

    // ============================================
    // Wire API (centralized — was duplicated in every wire owner)
    // ============================================
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
        if (!wd)
            return false;

        if (wd.m_SourcePort == "")
        {
            wd.m_SourcePort = LFPG_PORT_OUTPUT_1;
        }

        if (!LFPG_HasPort(wd.m_SourcePort, LFPG_PortDir.OUT))
        {
            string warnPort = "[";
            warnPort = warnPort + GetType();
            warnPort = warnPort + "] AddWire rejected: not an output port: ";
            warnPort = warnPort + wd.m_SourcePort;
            LFPG_Util.Warn(warnPort);
            return false;
        }

        bool result = LFPG_WireHelper.AddWire(m_Wires, wd);
        if (result)
        {
            #ifdef SERVER
            m_WireGeneration = m_WireGeneration + 1;
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
            m_WireGeneration = m_WireGeneration + 1;
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
            m_WireGeneration = m_WireGeneration + 1;
            SetSynchDirty();
            #endif
        }
        return result;
    }

    bool LFPG_PruneMissingTargets()
    {
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        ref map<string, bool> validIds;
        if (nm) validIds = nm.GetCachedValidIds();
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
            m_WireGeneration = m_WireGeneration + 1;
            SetSynchDirty();
            #endif
        }
        return result;
    }

    // ============================================
    // Default CanConnectTo (wire owners can connect OUT→IN)
    // Subclass can override for custom logic (e.g. Monitor camera-only).
    // ============================================
    override bool LFPG_CanConnectTo(Object other, string myPort, string otherPort)
    {
        if (!other)
            return false;

        if (!LFPG_HasPort(myPort, LFPG_PortDir.OUT))
            return false;

        EntityAI otherEntity = EntityAI.Cast(other);
        if (!otherEntity)
            return false;

        string otherId = LFPG_DeviceAPI.GetDeviceId(otherEntity);
        if (otherId != "")
        {
            return LFPG_DeviceAPI.HasPort(other, otherPort, LFPG_PortDir.IN);
        }

        // Vanilla fallback
        return LFPG_DeviceAPI.IsEnergyConsumer(otherEntity);
    }

    // ============================================
    // Hooks for concrete device (empty — subclass overrides)
    // ============================================
    void LFPG_OnInitDevice() {}
    void LFPG_OnVarSyncDevice() {}
    void LFPG_OnStoreSaveDevice(ParamsWriteContext ctx) {}
    bool LFPG_OnStoreLoadDevice(ParamsReadContext ctx, int deviceVer) { return true; }
};
