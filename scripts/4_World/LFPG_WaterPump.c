// =========================================================
// LF_PowerGrid - Water Pump device (v1.1.0)
//
// LF_WaterPump_Kit:  DeployableContainer_Base pattern (like Solar Panel Kit).
//                    Box model in hands -> hologram shows T1 pump model.
//                    Config: SingleUseActions={527}, ContinuousActions={231}
//                    Hologram: 5 overrides in LFPG_HologramMod.c
//
// LF_WaterPump (T1): PASSTHROUGH, 1 IN + 1 OUT, 50 u/s, cap 100 u/s
// LF_WaterPump_T2:   PASSTHROUGH, 1 IN + 3 OUT, 50 u/s, cap 100 u/s + 50L tank
//
// ENFORCE SCRIPT NOTES:
//   - No ternary operators, No ++ / --, Explicit typing, No foreach
// =========================================================

// ---------------------------------------------------------
// KIT: DeployableContainer_Base pattern (different-model hologram)
// ---------------------------------------------------------
class LF_WaterPump_Kit : DeployableContainer_Base
{
    string GetDeployedClassname()
    {
        return "LF_WaterPump";
    }

    vector GetDeployPositionOffset()
    {
        return "0 0 0";
    }

    vector GetDeployOrientationOffset()
    {
        return "0 0 0";
    }

    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        if (!GetGame().IsDedicatedServer())
            return;

        PlayerBase pb = PlayerBase.Cast(player);
        if (!pb)
            return;

        LF_WaterPump pump = LF_WaterPump.Cast(GetGame().CreateObject(GetDeployedClassname(), pb.GetLocalProjectionPosition(), false));

        if (!pump)
        {
            LFPG_Util.Error("[WaterPump_Kit] Failed to create LF_WaterPump! Kit preserved.");
            pb.MessageStatus("[LFPG] Water Pump placement failed. Kit preserved.");
            return;
        }

        pump.SetPosition(position);
        pump.SetOrientation(orientation);

        SetIsDeploySound(true);

        string tLog = "[WaterPump_Kit] Deployed LF_WaterPump at pos=" + position.ToString();
        tLog = tLog + " ori=" + orientation.ToString();
        LFPG_Util.Info(tLog);

        this.DeleteSafe();
    }

    override bool IsBasebuildingKit()
    {
        return true;
    }

    override bool IsDeployable()
    {
        return true;
    }

    override bool CanDisplayCargo()
    {
        return false;
    }

    override string GetDeploySoundset()
    {
        return "placeBarbedWire_SoundSet";
    }

    override string GetLoopDeploySoundset()
    {
        return "";
    }
};

// ---------------------------------------------------------
// WATER PUMP T1: PASSTHROUGH (1 IN + 1 OUT)
// Self-consumption: 50 u/s, Throughput cap: 100 u/s
// ---------------------------------------------------------
class LF_WaterPump : Inventory_Base
{
    protected int m_DeviceIdLow = 0;
    protected int m_DeviceIdHigh = 0;
    protected string m_DeviceId;
    protected ref array<ref LFPG_WireData> m_Wires;
    protected bool m_PoweredNet = false;
    protected bool m_Overloaded = false;
    protected bool m_LFPG_Deleting = false;
    protected float m_TabletLastRealMs = 0.0;
    protected EffectSound m_PumpLoopSound;

    void LF_WaterPump()
    {
        m_Wires = new array<ref LFPG_WireData>;
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
        RegisterNetSyncVariableBool("m_Overloaded");
    }

    void ~LF_WaterPump()
    {
        if (m_PumpLoopSound)
        {
            m_PumpLoopSound.SoundStop();
            m_PumpLoopSound = null;
        }
    }

    override void SetActions()
    {
        super.SetActions();
        RemoveAction(ActionTakeItem);
        RemoveAction(ActionTakeItemToHands);
        AddAction(LFPG_ActionDrinkPump);
        AddAction(LFPG_ActionWashHandsPump);
    }

    bool LFPG_IsGateOpen() { return true; }

    override bool CanPutInCargo(EntityAI parent) { return false; }
    override bool CanPutIntoHands(EntityAI parent) { return false; }
    override bool CanBePlaced(Man player, vector position) { return false; }
    override bool IsHeavyBehaviour() { return false; }

    override bool CanReleaseAttachment(EntityAI attachment)
    {
        if (!attachment) return true;
        if (attachment.IsKindOf("PurificationTablets")) return false;
        return super.CanReleaseAttachment(attachment);
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
        m_TabletLastRealMs = GetGame().GetTime();
        #endif
        LFPG_UpdateDeviceIdString();
        LFPG_TryRegister();
        #ifdef SERVER
        LFPG_NetworkManager.Get().BroadcastOwnerWires(this);
        #endif
    }

    override void EEKilled(Object killer)
    {
        LFPG_DeviceLifecycle.OnDeviceKilled(this, m_DeviceId);
        #ifdef SERVER
        if (m_PoweredNet) { m_PoweredNet = false; SetSynchDirty(); }
        #endif
        super.EEKilled(killer);
    }

    override void EEDelete(EntityAI parent)
    {
        m_LFPG_Deleting = true;
        LFPG_DeviceLifecycle.OnDeviceDeleted(this, m_DeviceId);
        if (m_PumpLoopSound) { m_PumpLoopSound.SoundStop(); m_PumpLoopSound = null; }
        super.EEDelete(parent);
    }

    override void EEItemLocationChanged(notnull InventoryLocation oldLoc, notnull InventoryLocation newLoc)
    {
        super.EEItemLocationChanged(oldLoc, newLoc);
        #ifdef SERVER
        if (m_DeviceId == "") return;
        bool wiresCut = LFPG_DeviceLifecycle.OnDeviceMoved(this, m_DeviceId, oldLoc, newLoc);
        if (wiresCut)
        {
            if (m_PoweredNet) { m_PoweredNet = false; SetSynchDirty(); }
        }
        #endif
    }

    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();
        LFPG_TryRegister();
        #ifndef SERVER
        if (m_PoweredNet) { SetObjectMaterial(LFPG_PUMP_LED_SELECTION_IDX, LFPG_PUMP_LED_RVMAT_ON); }
        else { SetObjectMaterial(LFPG_PUMP_LED_SELECTION_IDX, LFPG_PUMP_LED_RVMAT_OFF); }
        if (m_PoweredNet && !m_PumpLoopSound)
        {
            m_PumpLoopSound = SEffectManager.PlaySound(LFPG_PUMP_LOOP_SOUNDSET, GetPosition());
            if (m_PumpLoopSound) { m_PumpLoopSound.SetAutodestroy(true); }
        }
        if (!m_PoweredNet && m_PumpLoopSound) { m_PumpLoopSound.SoundStop(); m_PumpLoopSound = null; }
        if (m_DeviceId != "")
        {
            LFPG_CableRenderer r = LFPG_CableRenderer.Get();
            if (r)
            {
                r.RequestDeviceSync(m_DeviceId, this);
                if (r.HasOwnerData(m_DeviceId)) { r.NotifyOwnerVisualChanged(m_DeviceId); }
            }
        }
        #endif
    }

    protected void LFPG_UpdateDeviceIdString() { m_DeviceId = LFPG_Util.MakeDeviceKey(m_DeviceIdLow, m_DeviceIdHigh); }
    protected void LFPG_TryRegister()
    {
        if (m_LFPG_Deleting) return;
        string oldId = m_DeviceId;
        LFPG_UpdateDeviceIdString();
        if (oldId != "" && oldId != m_DeviceId) { LFPG_DeviceRegistry.Get().Unregister(oldId, this); }
        if (m_DeviceId != "") { LFPG_DeviceRegistry.Get().Register(this, m_DeviceId); }
    }

    float LFPG_GetTabletLastMs() { return m_TabletLastRealMs; }
    void LFPG_SetTabletLastMs(float ms) { m_TabletLastRealMs = ms; }

    void LFPG_ConsumeFilterTablet()
    {
        #ifdef SERVER
        EntityAI filter = FindAttachmentBySlotName("LF_PumpFilter");
        if (!filter) return;
        int qty = filter.GetQuantity();
        if (qty <= 1) { GetGame().ObjectDelete(filter); }
        else
        {
            ItemBase filterItem = ItemBase.Cast(filter);
            if (filterItem) { filterItem.SetQuantity(qty - 1); }
        }
        #endif
    }

    bool LFPG_HasActiveFilter() { return LFPG_PumpHelper.HasActiveFilter(this); }

    // ---- Device interface (2 ports) ----
    string LFPG_GetDeviceId() { return m_DeviceId; }
    int LFPG_GetPortCount() { return 2; }
    string LFPG_GetPortName(int idx)
    {
        if (idx == 0) return "input_1";
        if (idx == 1) return "output_1";
        return "";
    }
    int LFPG_GetPortDir(int idx)
    {
        if (idx == 0) return LFPG_PortDir.IN;
        if (idx == 1) return LFPG_PortDir.OUT;
        return -1;
    }
    string LFPG_GetPortLabel(int idx)
    {
        if (idx == 0) return "Input";
        if (idx == 1) return "Output";
        return "";
    }
    bool LFPG_HasPort(string portName, int dir)
    {
        if (dir == LFPG_PortDir.IN && portName == "input_1") return true;
        if (dir == LFPG_PortDir.OUT && portName == "output_1") return true;
        return false;
    }

    vector LFPG_GetPortWorldPos(string portName)
    {
        string memPoint = "port_" + portName;
        if (MemoryPointExists(memPoint)) { return ModelToWorld(GetMemoryPointPos(memPoint)); }
        int len = portName.Length();
        if (len >= 3)
        {
            string lastChar = portName.Substring(len - 1, 1);
            string beforeLast = portName.Substring(len - 2, 1);
            if (beforeLast == "_")
            {
                string compact = "port_" + portName.Substring(0, len - 2) + lastChar;
                if (MemoryPointExists(compact)) { return ModelToWorld(GetMemoryPointPos(compact)); }
            }
        }
        if (MemoryPointExists(portName)) { return ModelToWorld(GetMemoryPointPos(portName)); }
        LFPG_Util.Warn("[LF_WaterPump] Missing memory point for port: " + portName);
        vector p = GetPosition(); p[1] = p[1] + 0.5; return p;
    }

    int LFPG_GetDeviceType() { return LFPG_DeviceType.PASSTHROUGH; }
    bool LFPG_IsSource() { return true; }
    bool LFPG_GetSourceOn() { return m_PoweredNet; }
    float LFPG_GetConsumption() { return LFPG_PUMP_CONSUMPTION; }
    float LFPG_GetCapacity() { return LFPG_PUMP_CAPACITY; }
    bool LFPG_IsPowered() { return m_PoweredNet; }
    bool LFPG_GetPoweredNet() { return m_PoweredNet; }

    void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered) return;
        m_PoweredNet = powered;
        SetSynchDirty();
        LFPG_Util.Debug("[LF_WaterPump] SetPowered(" + powered.ToString() + ") id=" + m_DeviceId);
        #endif
    }

    bool LFPG_GetOverloaded() { return m_Overloaded; }
    void LFPG_SetOverloaded(bool val)
    {
        #ifdef SERVER
        if (m_Overloaded != val) { m_Overloaded = val; SetSynchDirty(); }
        #endif
    }

    bool LFPG_CanConnectTo(Object other, string myPort, string otherPort)
    {
        if (!other) return false;
        if (!LFPG_HasPort(myPort, LFPG_PortDir.OUT)) return false;
        EntityAI otherEntity = EntityAI.Cast(other);
        if (!otherEntity) return false;
        string otherId = LFPG_DeviceAPI.GetDeviceId(otherEntity);
        if (otherId != "") { return LFPG_DeviceAPI.HasPort(other, otherPort, LFPG_PortDir.IN); }
        return LFPG_DeviceAPI.IsEnergyConsumer(otherEntity);
    }

    // ---- Vanilla water overrides (server: graph-verified, client: SyncVar) ----
    override int GetLiquidSourceType()
    {
        if (!LFPG_PumpHelper.VerifyPowered(this)) return LIQUID_NONE;
        if (LFPG_HasActiveFilter()) return LIQUID_CLEANWATER;
        return LIQUID_RIVERWATER;
    }
    override int GetWaterSourceObjectType()
    {
        if (!LFPG_PumpHelper.VerifyPowered(this)) return EWaterSourceObjectType.NONE;
        return EWaterSourceObjectType.WELL;
    }
    override bool IsWell() { return LFPG_PumpHelper.VerifyPowered(this); }
    override float GetLiquidThroughputCoef() { return LIQUID_THROUGHPUT_WELL; }

    // ---- Wire ownership API ----
    bool LFPG_HasWireStore() { return true; }
    array<ref LFPG_WireData> LFPG_GetWires() { return m_Wires; }
    string LFPG_GetWiresJSON() { return LFPG_WireHelper.GetJSON(m_Wires); }

    bool LFPG_AddWire(LFPG_WireData wd)
    {
        if (!wd) return false;
        if (wd.m_SourcePort == "") wd.m_SourcePort = "output_1";
        if (!LFPG_HasPort(wd.m_SourcePort, LFPG_PortDir.OUT))
        { LFPG_Util.Warn("[LF_WaterPump] AddWire rejected: " + wd.m_SourcePort); return false; }
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
        bool r = LFPG_WireHelper.ClearAll(m_Wires);
        if (r)
        {
            #ifdef SERVER
            SetSynchDirty();
            #endif
        }
        return r;
    }
    bool LFPG_ClearWiresForCreator(string cid)
    {
        bool r = LFPG_WireHelper.ClearForCreator(m_Wires, cid);
        if (r)
        {
            #ifdef SERVER
            SetSynchDirty();
            #endif
        }
        return r;
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
                if (did != "") { validIds[did] = true; }
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

    // ---- Persistence ----
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);
        ctx.Write(m_DeviceIdLow);
        ctx.Write(m_DeviceIdHigh);
        string json;
        LFPG_WireHelper.SerializeJSON(m_Wires, json);
        ctx.Write(json);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version)) return false;
        if (!ctx.Read(m_DeviceIdLow)) { LFPG_Util.Error("[LF_WaterPump] Load: m_DeviceIdLow"); return false; }
        if (!ctx.Read(m_DeviceIdHigh)) { LFPG_Util.Error("[LF_WaterPump] Load: m_DeviceIdHigh"); return false; }
        LFPG_UpdateDeviceIdString();
        string json;
        if (!ctx.Read(json)) { LFPG_Util.Error("[LF_WaterPump] Load: wires " + m_DeviceId); return false; }
        LFPG_WireHelper.DeserializeJSON(m_Wires, json, "LF_WaterPump");
        return true;
    }
};

// ---------------------------------------------------------
// WATER PUMP T2: PASSTHROUGH (1 IN + 3 OUT) + 50L tank
// Independent class (NOT inherited from T1).
// ---------------------------------------------------------
class LF_WaterPump_T2 : Inventory_Base
{
    protected int m_DeviceIdLow = 0;
    protected int m_DeviceIdHigh = 0;
    protected string m_DeviceId;
    protected ref array<ref LFPG_WireData> m_Wires;
    protected bool m_PoweredNet = false;
    protected bool m_Overloaded = false;
    protected bool m_LFPG_Deleting = false;
    protected float m_TabletLastRealMs = 0.0;
    protected float m_TankLevel = 0.0;
    protected int m_TankLiquidType = 0;
    protected EffectSound m_PumpLoopSound;

    void LF_WaterPump_T2()
    {
        m_Wires = new array<ref LFPG_WireData>;
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
        RegisterNetSyncVariableBool("m_Overloaded");
        RegisterNetSyncVariableFloat("m_TankLevel", 0.0, 50.0, 8);
        RegisterNetSyncVariableInt("m_TankLiquidType");
    }

    void ~LF_WaterPump_T2()
    {
        if (m_PumpLoopSound) { m_PumpLoopSound.SoundStop(); m_PumpLoopSound = null; }
    }

    override void SetActions()
    {
        super.SetActions();
        RemoveAction(ActionTakeItem);
        RemoveAction(ActionTakeItemToHands);
        AddAction(LFPG_ActionDrinkPump);
        AddAction(LFPG_ActionWashHandsPump);
        AddAction(LFPG_ActionFillPump);
    }

    bool LFPG_IsGateOpen() { return true; }

    override bool CanPutInCargo(EntityAI parent) { return false; }
    override bool CanPutIntoHands(EntityAI parent) { return false; }
    override bool CanBePlaced(Man player, vector position) { return false; }
    override bool IsHeavyBehaviour() { return false; }

    override bool CanReleaseAttachment(EntityAI attachment)
    {
        if (!attachment) return true;
        if (attachment.IsKindOf("PurificationTablets")) return false;
        return super.CanReleaseAttachment(attachment);
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
        m_TabletLastRealMs = GetGame().GetTime();
        #endif
        LFPG_UpdateDeviceIdString();
        LFPG_TryRegister();
        #ifdef SERVER
        LFPG_NetworkManager.Get().BroadcastOwnerWires(this);
        #endif
    }

    override void EEKilled(Object killer)
    {
        LFPG_DeviceLifecycle.OnDeviceKilled(this, m_DeviceId);
        #ifdef SERVER
        if (m_PoweredNet) { m_PoweredNet = false; SetSynchDirty(); }
        #endif
        super.EEKilled(killer);
    }

    override void EEDelete(EntityAI parent)
    {
        m_LFPG_Deleting = true;
        LFPG_DeviceLifecycle.OnDeviceDeleted(this, m_DeviceId);
        if (m_PumpLoopSound) { m_PumpLoopSound.SoundStop(); m_PumpLoopSound = null; }
        super.EEDelete(parent);
    }

    override void EEItemLocationChanged(notnull InventoryLocation oldLoc, notnull InventoryLocation newLoc)
    {
        super.EEItemLocationChanged(oldLoc, newLoc);
        #ifdef SERVER
        if (m_DeviceId == "") return;
        bool wiresCut = LFPG_DeviceLifecycle.OnDeviceMoved(this, m_DeviceId, oldLoc, newLoc);
        if (wiresCut)
        {
            if (m_PoweredNet) { m_PoweredNet = false; SetSynchDirty(); }
        }
        #endif
    }

    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();
        LFPG_TryRegister();
        #ifndef SERVER
        if (m_PoweredNet) { SetObjectMaterial(LFPG_PUMP_LED_SELECTION_IDX, LFPG_PUMP_LED_RVMAT_ON); }
        else { SetObjectMaterial(LFPG_PUMP_LED_SELECTION_IDX, LFPG_PUMP_LED_RVMAT_OFF); }
        if (m_PoweredNet && !m_PumpLoopSound)
        {
            m_PumpLoopSound = SEffectManager.PlaySound(LFPG_PUMP_LOOP_SOUNDSET, GetPosition());
            if (m_PumpLoopSound) { m_PumpLoopSound.SetAutodestroy(true); }
        }
        if (!m_PoweredNet && m_PumpLoopSound) { m_PumpLoopSound.SoundStop(); m_PumpLoopSound = null; }
        if (m_DeviceId != "")
        {
            LFPG_CableRenderer r = LFPG_CableRenderer.Get();
            if (r)
            {
                r.RequestDeviceSync(m_DeviceId, this);
                if (r.HasOwnerData(m_DeviceId)) { r.NotifyOwnerVisualChanged(m_DeviceId); }
            }
        }
        #endif
    }

    protected void LFPG_UpdateDeviceIdString() { m_DeviceId = LFPG_Util.MakeDeviceKey(m_DeviceIdLow, m_DeviceIdHigh); }
    protected void LFPG_TryRegister()
    {
        if (m_LFPG_Deleting) return;
        string oldId = m_DeviceId;
        LFPG_UpdateDeviceIdString();
        if (oldId != "" && oldId != m_DeviceId) { LFPG_DeviceRegistry.Get().Unregister(oldId, this); }
        if (m_DeviceId != "") { LFPG_DeviceRegistry.Get().Register(this, m_DeviceId); }
    }

    // ---- Tablet timer ----
    float LFPG_GetTabletLastMs() { return m_TabletLastRealMs; }
    void LFPG_SetTabletLastMs(float ms) { m_TabletLastRealMs = ms; }

    void LFPG_ConsumeFilterTablet()
    {
        #ifdef SERVER
        EntityAI filter = FindAttachmentBySlotName("LF_PumpFilter");
        if (!filter) return;
        int qty = filter.GetQuantity();
        if (qty <= 1) { GetGame().ObjectDelete(filter); }
        else
        {
            ItemBase filterItem = ItemBase.Cast(filter);
            if (filterItem) { filterItem.SetQuantity(qty - 1); }
        }
        #endif
    }

    bool LFPG_HasActiveFilter() { return LFPG_PumpHelper.HasActiveFilter(this); }

    // ---- Tank accessors ----
    float LFPG_GetTankLevel() { return m_TankLevel; }
    void LFPG_SetTankLevel(float level)
    {
        #ifdef SERVER
        m_TankLevel = level;
        SetSynchDirty();
        #endif
    }
    int LFPG_GetTankLiquidType() { return m_TankLiquidType; }
    void LFPG_SetTankLiquidType(int liqType)
    {
        #ifdef SERVER
        m_TankLiquidType = liqType;
        SetSynchDirty();
        #endif
    }

    // ---- Device interface (4 ports: 1 IN + 3 OUT) ----
    string LFPG_GetDeviceId() { return m_DeviceId; }
    int LFPG_GetPortCount() { return 4; }

    string LFPG_GetPortName(int idx)
    {
        if (idx == 0) return "input_1";
        if (idx == 1) return "output_1";
        if (idx == 2) return "output_2";
        if (idx == 3) return "output_3";
        return "";
    }

    int LFPG_GetPortDir(int idx)
    {
        if (idx == 0) return LFPG_PortDir.IN;
        if (idx >= 1 && idx <= 3) return LFPG_PortDir.OUT;
        return -1;
    }

    string LFPG_GetPortLabel(int idx)
    {
        if (idx == 0) return "Input 1";
        if (idx == 1) return "Output 1";
        if (idx == 2) return "Output 2";
        if (idx == 3) return "Output 3";
        return "";
    }

    bool LFPG_HasPort(string portName, int dir)
    {
        if (dir == LFPG_PortDir.IN && portName == "input_1") return true;
        if (dir == LFPG_PortDir.OUT)
        {
            if (portName == "output_1") return true;
            if (portName == "output_2") return true;
            if (portName == "output_3") return true;
        }
        return false;
    }

    vector LFPG_GetPortWorldPos(string portName)
    {
        string memPoint = "port_" + portName;
        if (MemoryPointExists(memPoint)) { return ModelToWorld(GetMemoryPointPos(memPoint)); }
        int len = portName.Length();
        if (len >= 3)
        {
            string lastChar = portName.Substring(len - 1, 1);
            string beforeLast = portName.Substring(len - 2, 1);
            if (beforeLast == "_")
            {
                string compact = "port_" + portName.Substring(0, len - 2) + lastChar;
                if (MemoryPointExists(compact)) { return ModelToWorld(GetMemoryPointPos(compact)); }
            }
        }
        if (MemoryPointExists(portName)) { return ModelToWorld(GetMemoryPointPos(portName)); }
        LFPG_Util.Warn("[LF_WaterPump_T2] Missing memory point: " + portName);
        vector p = GetPosition(); p[1] = p[1] + 0.5; return p;
    }

    int LFPG_GetDeviceType() { return LFPG_DeviceType.PASSTHROUGH; }
    bool LFPG_IsSource() { return true; }
    bool LFPG_GetSourceOn() { return m_PoweredNet; }
    float LFPG_GetConsumption() { return LFPG_PUMP_CONSUMPTION; }
    float LFPG_GetCapacity() { return LFPG_PUMP_CAPACITY; }
    bool LFPG_IsPowered() { return m_PoweredNet; }
    bool LFPG_GetPoweredNet() { return m_PoweredNet; }

    void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered) return;
        m_PoweredNet = powered;
        SetSynchDirty();
        LFPG_Util.Debug("[LF_WaterPump_T2] SetPowered(" + powered.ToString() + ") id=" + m_DeviceId);
        #endif
    }

    bool LFPG_GetOverloaded() { return m_Overloaded; }
    void LFPG_SetOverloaded(bool val)
    {
        #ifdef SERVER
        if (m_Overloaded != val) { m_Overloaded = val; SetSynchDirty(); }
        #endif
    }

    bool LFPG_CanConnectTo(Object other, string myPort, string otherPort)
    {
        if (!other) return false;
        if (!LFPG_HasPort(myPort, LFPG_PortDir.OUT)) return false;
        EntityAI otherEntity = EntityAI.Cast(other);
        if (!otherEntity) return false;
        string otherId = LFPG_DeviceAPI.GetDeviceId(otherEntity);
        if (otherId != "") { return LFPG_DeviceAPI.HasPort(other, otherPort, LFPG_PortDir.IN); }
        return LFPG_DeviceAPI.IsEnergyConsumer(otherEntity);
    }

    // ---- Vanilla water overrides (server: graph-verified, client: SyncVar) ----
    override int GetLiquidSourceType()
    {
        if (!LFPG_PumpHelper.VerifyPowered(this)) return LIQUID_NONE;
        if (LFPG_HasActiveFilter()) return LIQUID_CLEANWATER;
        return LIQUID_RIVERWATER;
    }
    override int GetWaterSourceObjectType()
    {
        if (!LFPG_PumpHelper.VerifyPowered(this)) return EWaterSourceObjectType.NONE;
        return EWaterSourceObjectType.WELL;
    }
    override bool IsWell() { return LFPG_PumpHelper.VerifyPowered(this); }
    override float GetLiquidThroughputCoef() { return LIQUID_THROUGHPUT_WELL; }

    // ---- Wire ownership API ----
    bool LFPG_HasWireStore() { return true; }
    array<ref LFPG_WireData> LFPG_GetWires() { return m_Wires; }
    string LFPG_GetWiresJSON() { return LFPG_WireHelper.GetJSON(m_Wires); }

    bool LFPG_AddWire(LFPG_WireData wd)
    {
        if (!wd) return false;
        if (wd.m_SourcePort == "") wd.m_SourcePort = "output_1";
        if (!LFPG_HasPort(wd.m_SourcePort, LFPG_PortDir.OUT))
        { LFPG_Util.Warn("[LF_WaterPump_T2] AddWire rejected: " + wd.m_SourcePort); return false; }
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
        bool r = LFPG_WireHelper.ClearAll(m_Wires);
        if (r)
        {
            #ifdef SERVER
            SetSynchDirty();
            #endif
        }
        return r;
    }
    bool LFPG_ClearWiresForCreator(string cid)
    {
        bool r = LFPG_WireHelper.ClearForCreator(m_Wires, cid);
        if (r)
        {
            #ifdef SERVER
            SetSynchDirty();
            #endif
        }
        return r;
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
                if (did != "") { validIds[did] = true; }
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

    // ---- Persistence (T2: adds m_TankLevel + m_TankLiquidType) ----
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);
        ctx.Write(m_DeviceIdLow);
        ctx.Write(m_DeviceIdHigh);
        string json;
        LFPG_WireHelper.SerializeJSON(m_Wires, json);
        ctx.Write(json);
        ctx.Write(m_TankLevel);
        ctx.Write(m_TankLiquidType);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version)) return false;
        if (!ctx.Read(m_DeviceIdLow)) { LFPG_Util.Error("[LF_WaterPump_T2] Load: m_DeviceIdLow"); return false; }
        if (!ctx.Read(m_DeviceIdHigh)) { LFPG_Util.Error("[LF_WaterPump_T2] Load: m_DeviceIdHigh"); return false; }
        LFPG_UpdateDeviceIdString();
        string json;
        if (!ctx.Read(json)) { LFPG_Util.Error("[LF_WaterPump_T2] Load: wires " + m_DeviceId); return false; }
        LFPG_WireHelper.DeserializeJSON(m_Wires, json, "LF_WaterPump_T2");
        if (!ctx.Read(m_TankLevel)) { LFPG_Util.Error("[LF_WaterPump_T2] Load: m_TankLevel"); return false; }
        if (!ctx.Read(m_TankLiquidType)) { LFPG_Util.Error("[LF_WaterPump_T2] Load: m_TankLiquidType"); return false; }
        return true;
    }
};
