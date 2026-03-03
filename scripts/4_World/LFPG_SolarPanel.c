// =========================================================
// LF_PowerGrid - Solar Panel devices (v0.7.47, Sprint 6)
//
// LF_SolarPanel_Kit:  Holdable kit (shared box model).
//                     Player places via hologram -> spawns LF_SolarPanel.
//                     Uses Inventory_Base + isDeployable=1 pattern.
//                     Hologram projection swap handled by LFPG_HologramMod.c
//                     (kit model = box, deployed model = panel).
//
// LF_SolarPanel:      T1 SOURCE device (20 u/s during daylight).
//                     1 output port (output_1). Owns wires.
//                     Solar timer checks daylight every 15s.
//                     Accepts MetalPlate + Nail attachments for upgrade.
//
// LF_SolarPanel_T2:   T2 SOURCE device (50 u/s during daylight).
//                     Inherits LF_SolarPanel. No attachments.
//                     Created by LFPG_ActionUpgradeSolarPanel.
//
// Memory points required in T1/T2 p3d (LOD Memory):
//   port_output_1 — cable connection point
//
// Wire manipulation delegated to LFPG_WireHelper (3_Game).
// =========================================================

// ---------------------------------------------------------
// KIT: holdable item that spawns the actual Solar Panel
// Uses shared box model — hologram shows panel via overrides.
// ---------------------------------------------------------
class LF_SolarPanel_Kit : Inventory_Base
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

    // v0.7.48: Disabled loop sound during placement.
    // ObjectDelete(this) in OnPlacementComplete destroys the kit
    // server-side during the action callback. The loop sound is
    // client-side and bound to the action lifecycle — the entity
    // deletion aborts the cleanup cycle before sound stop runs,
    // leaving an orphaned loop with no owner.
    override string GetLoopDeploySoundset()
    {
        return "";
    }

    // Returns the classname that the hologram should project.
    // Called by LFPG_HologramMod overrides to swap box → panel.
    string GetDeployedClassname()
    {
        return "LF_SolarPanel";
    }

    vector GetDeployPositionOffset()
    {
        return "0 0 0";
    }

    vector GetDeployOrientationOffset()
    {
        return "0 0 0";
    }

    override void SetActions()
    {
        super.SetActions();
        AddAction(ActionTogglePlaceObject);
        AddAction(LFPG_ActionPlaceSolarPanel);
    }

    // v0.7.48: Use PARAMETERS, not GetPosition().
    // GetPosition() returns kit physical pos (near player), not hologram.
    // Same fix as Splitter v0.7.41 and CeilingLight.
    // Only delete kit on successful spawn (Splitter v0.7.32 pattern).
    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        LFPG_Util.Info("[SolarPanel_Kit] OnPlacementComplete: pos=" + finalPos.ToString() + " ori=" + finalOri.ToString());

        EntityAI panel = GetGame().CreateObjectEx("LF_SolarPanel", finalPos, ECE_CREATEPHYSICS);
        if (panel)
        {
            panel.SetPosition(finalPos);
            panel.SetOrientation(finalOri);
            panel.Update();
            LFPG_Util.Info("[SolarPanel_Kit] Deployed LF_SolarPanel at " + finalPos.ToString());

            // Only delete kit on successful spawn (v0.7.32 parity).
            // If CreateObjectEx fails, player keeps the kit.
            GetGame().ObjectDelete(this);
        }
        else
        {
            LFPG_Util.Error("[SolarPanel_Kit] Failed to create LF_SolarPanel! Kit preserved.");
            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                pb.MessageStatus("[LFPG] Solar panel placement failed. Kit preserved.");
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// T1: Solar Panel SOURCE (20 u/s during daylight)
// ---------------------------------------------------------
class LF_SolarPanel : Inventory_Base
{
    // ---- Device identity ----
    protected int m_DeviceIdLow = 0;
    protected int m_DeviceIdHigh = 0;
    protected string m_DeviceId;

    // ---- Wires owned (output side) ----
    protected ref array<ref LFPG_WireData> m_Wires;

    // ---- Source state (replicated) ----
    protected bool m_SourceOn = false;

    // ---- Anti-ghost guard (RC-04 parity) ----
    // Prevents OnVariablesSynchronized from re-registering after EEDelete.
    protected bool m_LFPG_Deleting = false;

    // ---- Load telemetry (replicated to clients) ----
    protected float m_LoadRatio = 0.0;
    protected int m_OverloadMask = 0;

    // ---- Solar timer constants ----
    static const int LFPG_SOLAR_CHECK_MS = 15000;   // 15s interval
    static const int LFPG_SOLAR_DAWN_HOUR = 6;      // daylight start
    static const int LFPG_SOLAR_DUSK_HOUR = 20;     // daylight end

    void LF_SolarPanel()
    {
        m_Wires = new array<ref LFPG_WireData>;
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_SourceOn");
        RegisterNetSyncVariableFloat("m_LoadRatio", 0.0, 5.0, 2);
        RegisterNetSyncVariableInt("m_OverloadMask");
    }

    // ============================================
    // Actions
    // ============================================
    override void SetActions()
    {
        super.SetActions();
        RemoveAction(ActionTakeItem);
        RemoveAction(ActionTakeItemToHands);
    }

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

    override bool IsElectricAppliance()
    {
        return false;
    }

    // ============================================
    // Solar timer (server-only)
    // ============================================
    protected void StartSolarTimer()
    {
        #ifdef SERVER
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(CheckSunlight, LFPG_SOLAR_CHECK_MS, true);
        #endif
    }

    protected void StopSolarTimer()
    {
        #ifdef SERVER
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(CheckSunlight);
        #endif
    }

    protected void CheckSunlight()
    {
        #ifdef SERVER
        if (!GetGame())
            return;

        World world = GetGame().GetWorld();
        if (!world)
            return;

        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        world.GetDate(year, month, day, hour, minute);

        bool hasSun = false;
        if (hour >= LFPG_SOLAR_DAWN_HOUR && hour < LFPG_SOLAR_DUSK_HOUR)
        {
            hasSun = true;
        }

        if (hasSun != m_SourceOn)
        {
            m_SourceOn = hasSun;
            SetSynchDirty();

            if (m_DeviceId != "")
            {
                LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
            }

            LFPG_Util.Info("[LF_SolarPanel] Sunlight changed: m_SourceOn=" + m_SourceOn.ToString() + " hour=" + hour.ToString() + " id=" + m_DeviceId);
        }
        #endif
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
            SetSynchDirty();
        }
        #endif

        LFPG_UpdateDeviceIdString();
        LFPG_TryRegister();

        #ifdef SERVER
        StartSolarTimer();

        // Immediate sunlight check on init (don't wait 15s).
        // CheckSunlight() calls RequestPropagate() internally if
        // m_SourceOn changes. No additional propagate needed here
        // for fresh spawns (m_SourceOn starts false, CheckSunlight
        // flips it to true during daytime and propagates).
        //
        // For persistence restores: m_SourceOn is loaded from save.
        // If it's already true and CheckSunlight sees daytime → no change
        // → no propagation. We must propagate explicitly in that case.
        bool preCheckState = m_SourceOn;
        CheckSunlight();
        bool postCheckState = m_SourceOn;

        LFPG_NetworkManager.Get().BroadcastOwnerWires(this);

        // Only propagate if CheckSunlight didn't already trigger it
        if (preCheckState == postCheckState && m_SourceOn && m_DeviceId != "")
        {
            LFPG_NetworkManager.Get().RequestPropagate(m_DeviceId);
        }
        #endif
    }

    override void EEDelete(EntityAI parent)
    {
        m_LFPG_Deleting = true;
        StopSolarTimer();
        LFPG_DeviceLifecycle.OnDeviceDeleted(this, m_DeviceId);
        super.EEDelete(parent);
    }

    override void EEKilled(Object killer)
    {
        StopSolarTimer();
        LFPG_DeviceLifecycle.OnDeviceKilled(this, m_DeviceId);

        #ifdef SERVER
        if (m_SourceOn)
        {
            m_SourceOn = false;
            SetSynchDirty();
        }
        #endif

        super.EEKilled(killer);
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
            if (m_SourceOn)
            {
                m_SourceOn = false;
                SetSynchDirty();
            }
        }
        #endif
    }

    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();
        LFPG_TryRegister();
    }

    // ============================================
    // Device identity helpers
    // ============================================
    protected void LFPG_UpdateDeviceIdString()
    {
        m_DeviceId = LFPG_Util.MakeDeviceKey(m_DeviceIdLow, m_DeviceIdHigh);
    }

    protected void LFPG_TryRegister()
    {
        // v0.7.48 (RC-04 parity): Don't register if device is being deleted.
        if (m_LFPG_Deleting)
            return;

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
        return 1;
    }

    string LFPG_GetPortName(int idx)
    {
        if (idx == 0) return "output_1";
        return "";
    }

    int LFPG_GetPortDir(int idx)
    {
        if (idx == 0) return LFPG_PortDir.OUT;
        return -1;
    }

    string LFPG_GetPortLabel(int idx)
    {
        if (idx == 0) return "Output";
        return "";
    }

    bool LFPG_HasPort(string portName, int dir)
    {
        if (dir == LFPG_PortDir.OUT && portName == "output_1") return true;
        return false;
    }

    vector LFPG_GetPortWorldPos(string portName)
    {
        string memPoint = "port_" + portName;
        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

        // Fallback: try compact naming (port_output1)
        int len = portName.Length();
        if (len >= 3)
        {
            string lastChar = portName.Substring(len - 1, 1);
            string beforeLast = portName.Substring(len - 2, 1);
            if (beforeLast == "_")
            {
                string compact = "port_" + portName.Substring(0, len - 2) + lastChar;
                if (MemoryPointExists(compact))
                {
                    return ModelToWorld(GetMemoryPointPos(compact));
                }
            }
        }

        // Fallback: device center + offset
        LFPG_Util.Warn("[LF_SolarPanel] Missing memory point for port: " + portName);
        vector p = GetPosition();
        p[1] = p[1] + 0.3;
        return p;
    }

    int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.SOURCE;
    }

    bool LFPG_IsSource()
    {
        return true;
    }

    bool LFPG_GetSourceOn()
    {
        return m_SourceOn;
    }

    // Capacity: 20 u/s for T1 (overridden by T2)
    float LFPG_GetCapacity()
    {
        return 20.0;
    }

    // SOURCE devices: consumption is 0
    float LFPG_GetConsumption()
    {
        return 0.0;
    }

    float LFPG_GetLoadRatio()
    {
        return m_LoadRatio;
    }

    void LFPG_SetLoadRatio(float ratio)
    {
        #ifdef SERVER
        if (m_LoadRatio != ratio)
        {
            m_LoadRatio = ratio;
            SetSynchDirty();
        }
        #endif
    }

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

    // SOURCE: SetPowered is a no-op (we drive power via m_SourceOn)
    void LFPG_SetPowered(bool powered)
    {
        // No-op for SOURCE devices
    }

    // SwitchState for ToggleSource compatibility (solar has no manual toggle)
    bool LFPG_GetSwitchState()
    {
        return m_SourceOn;
    }

    // ---- Connection validation ----
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

        if (wd.m_SourcePort == "")
        {
            wd.m_SourcePort = "output_1";
        }

        if (!LFPG_HasPort(wd.m_SourcePort, LFPG_PortDir.OUT))
        {
            LFPG_Util.Warn("[LF_SolarPanel] AddWire rejected: not an output port: " + wd.m_SourcePort);
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
            LFPG_Util.Error("[LF_SolarPanel] OnStoreLoad: failed to read m_DeviceIdLow");
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            LFPG_Util.Error("[LF_SolarPanel] OnStoreLoad: failed to read m_DeviceIdHigh");
            return false;
        }

        LFPG_UpdateDeviceIdString();

        if (!ctx.Read(m_SourceOn))
        {
            LFPG_Util.Error("[LF_SolarPanel] OnStoreLoad: failed to read m_SourceOn for " + m_DeviceId);
            return false;
        }

        string json;
        if (!ctx.Read(json))
        {
            LFPG_Util.Error("[LF_SolarPanel] OnStoreLoad: failed to read wires json for " + m_DeviceId);
            return false;
        }
        LFPG_WireHelper.DeserializeJSON(m_Wires, json, "LF_SolarPanel");

        return true;
    }
};

// ---------------------------------------------------------
// T2: Upgraded Solar Panel SOURCE (50 u/s during daylight)
// Inherits T1 — only overrides capacity and blocks attachments.
// ---------------------------------------------------------
class LF_SolarPanel_T2 : LF_SolarPanel
{
    // T2 generates 50 u/s instead of 20 u/s
    override float LFPG_GetCapacity()
    {
        return 50.0;
    }

    // T2 does not accept upgrade materials
    override bool CanReceiveAttachment(EntityAI attachment, int slotId)
    {
        return false;
    }
};
