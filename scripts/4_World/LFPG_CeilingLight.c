// =========================================================
// LF_PowerGrid - CeilingLight device (v0.8.3)
//
// LF_CeilingLight_Kit:  Holdable item (deployable).
//                       Player places via hologram -> spawns LF_CeilingLight.
//                       Ceiling placement via HologramMod pitch=180.
//
// LF_CeilingLight:      PASSTHROUGH with self-consumption.
//                       1 input  (input_1)  - power from upstream
//                       1 output (output_1) - power to downstream
//                       Consumes 10 u/s for its own light.
//                       Passes remainder downstream (cap 50 u/s).
//                       Owns wires on output side (same as Splitter).
//
// Memory points (LOD Memory):
//   light          - where the PointLight attaches
//   port_input_1   - upstream cable connection
//   port_output_1  - downstream cable connection
//
// Named selections (LOD Resolution):
//   light_emit     - faces that glow (hiddenSelections[0])
//
// Visual feedback:
//   Powered ON:  rvmat swap to emmisive + PointLight attached
//   Powered OFF: rvmat swap to dark + PointLight destroyed
//
// Persistence: DeviceIdLow, DeviceIdHigh, wiresJSON.
//   m_PoweredNet is NOT persisted — derived from graph propagation
//   on server load. Avoids "ghost lamp" bug (Hallazgo 6).
// v0.8.3 (Audit Parity): Added CanBePickedUp()->false and
//   IsHeavyBehaviour()->false. Prevents F-key pick-up and shoulder
//   carry that silently break wire connections.
// =========================================================

// ---------------------------------------------------------
// RVMAT paths (must match hiddenSelectionsMaterials in config.cpp)
// ---------------------------------------------------------
static const string LFPG_CEILING_RVMAT_OFF = "\\LFPowerGrid\\data\\ceiling_light\\lf_ceiling_light.rvmat";
static const string LFPG_CEILING_RVMAT_ON  = "\\LFPowerGrid\\data\\ceiling_light\\lf_ceiling_light_on.rvmat";

// ---------------------------------------------------------
// KIT: deployable item that spawns the actual CeilingLight
// ---------------------------------------------------------
class LF_CeilingLight_Kit : Inventory_Base
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

    // v0.7.48 (Bug 4): Disabled loop sound during placement.
    // ObjectDelete(this) in OnPlacementComplete destroys the kit
    // server-side during the action callback. The loop sound is
    // client-side and bound to the action lifecycle — the entity
    // deletion aborts the action cleanup cycle before the sound
    // stop/fadeout runs, leaving an orphaned loop with no owner.
    // The one-shot from GetDeploySoundset() provides sufficient
    // auditory feedback for the placement action.
    override string GetLoopDeploySoundset()
    {
        return "";
    }

    override void SetActions()
    {
        super.SetActions();
        AddAction(ActionTogglePlaceObject);
        AddAction(LFPG_ActionPlaceCeilingLight);
    }

    // Hallazgo 2: Use PARAMETERS, not GetPosition().
    // GetPosition() returns kit position (next to player), not hologram.
    // Double-set orientation forces physics update for pitch=180 (ceiling).
    // No ECE_PLACE_ON_SURFACE — that snaps to ground, killing ceiling mount.
    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        LFPG_Util.Info("[CeilingLight_Kit] OnPlacementComplete: pos=" + finalPos.ToString() + " ori=" + finalOri.ToString());

        EntityAI light = GetGame().CreateObjectEx("LF_CeilingLight", finalPos, ECE_CREATEPHYSICS);
        if (light)
        {
            light.SetPosition(finalPos);
            light.SetOrientation(finalOri);
            // Double-set: forces physics engine to accept pitch=180
            light.SetOrientation(light.GetOrientation());
            light.Update();
            LFPG_Util.Info("[CeilingLight_Kit] Deployed LF_CeilingLight at " + finalPos.ToString() + " ori=" + finalOri.ToString());

            // v0.7.48: Only delete kit on successful spawn (Splitter parity).
            // If CreateObjectEx fails, the player keeps the kit.
            GetGame().ObjectDelete(this);
        }
        else
        {
            LFPG_Util.Error("[CeilingLight_Kit] Failed to create LF_CeilingLight! Kit preserved.");
            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                pb.MessageStatus("[LFPG] Ceiling light placement failed. Kit preserved.");
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// ENTITY: ceiling-mounted passthrough light
// ---------------------------------------------------------
class LF_CeilingLight : Inventory_Base
{
    // ---- Device identity ----
    protected int m_DeviceIdLow = 0;
    protected int m_DeviceIdHigh = 0;
    protected string m_DeviceId;

    // ---- Wires owned (output side) ----
    protected ref array<ref LFPG_WireData> m_Wires;

    // ---- Power state (synced, NOT persisted) ----
    protected bool m_PoweredNet = false;

    // ---- Overload/warning masks for cable coloring ----
    protected int m_OverloadMask = 0;
    protected int m_WarningMask = 0;

    // ---- Anti-ghost guard (Hallazgo 5) ----
    protected bool m_LFPG_Deleting = false;

    // ---- Client-side light effect ----
    // ScriptedLightBase is engine object (not Managed). Do NOT store as ref.
    protected ScriptedLightBase m_LFPG_Light;

    // ============================================
    // Constructor: register SyncVars
    // ============================================
    void LF_CeilingLight()
    {
        m_Wires = new array<ref LFPG_WireData>;
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
        RegisterNetSyncVariableInt("m_OverloadMask");
        RegisterNetSyncVariableInt("m_WarningMask");
    }

    // ============================================
    // Actions
    // ============================================
    override void SetActions()
    {
        super.SetActions();
        // Block vanilla take/carry actions — deployed device
        RemoveAction(ActionTakeItem);
        RemoveAction(ActionTakeItemToHands);
    }

    // ============================================
    // Block pickup/move — deployed device
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

    // v0.8.3 (Audit Fix 2): Prevent pick-up via F-key.
    // Without this, player can grab the device, silently breaking all
    // wire connections and causing orphaned wires in the graph.
    override bool CanBePickedUp()
    {
        return false;
    }

    // v0.8.3 (Audit Fix 2): Prevent heavy-item carry behavior.
    override bool IsHeavyBehaviour()
    {
        return false;
    }

    // ============================================
    // Lifecycle: EEInit
    // ============================================
    override void EEInit()
    {
        super.EEInit();

        #ifdef SERVER
        // Generate DeviceId if new (not loaded from persistence)
        if (m_DeviceIdLow == 0 && m_DeviceIdHigh == 0)
        {
            LFPG_Util.GenerateDeviceId(m_DeviceIdLow, m_DeviceIdHigh);
            SetSynchDirty();
        }
        #endif

        LFPG_UpdateDeviceIdString();
        LFPG_TryRegister();

        #ifdef SERVER
        LFPG_NetworkManager.Get().BroadcastOwnerWires(this);
        // PASSTHROUGH does NOT initiate propagation — upstream does.
        #endif
    }

    // ============================================
    // Lifecycle: EEKilled
    // ============================================
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

    // ============================================
    // Lifecycle: EEDelete (with anti-ghost guard)
    // ============================================
    override void EEDelete(EntityAI parent)
    {
        m_LFPG_Deleting = true;
        LFPG_DeviceLifecycle.OnDeviceDeleted(this, m_DeviceId);

        #ifndef SERVER
        LFPG_DestroyLight();
        #endif

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
            if (m_PoweredNet)
            {
                m_PoweredNet = false;
                SetSynchDirty();
            }
        }
        #endif
    }

    // ============================================
    // OnVariablesSynchronized
    // ============================================
    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();
        LFPG_TryRegister();

        #ifndef SERVER
        if (m_PoweredNet)
        {
            LFPG_CreateLight();
            LFPG_SetRvmatOn();
        }
        else
        {
            LFPG_DestroyLight();
            LFPG_SetRvmatOff();
        }
        #endif
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

    // 2 ports: 1 input + 1 output
    int LFPG_GetPortCount()
    {
        return 2;
    }

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
        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

        // Compact fallback (port_output1 vs port_output_1)
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

        if (MemoryPointExists(portName))
        {
            return ModelToWorld(GetMemoryPointPos(portName));
        }

        LFPG_Util.Warn("[LF_CeilingLight] Missing memory point for port: " + portName);
        vector p = GetPosition();
        p[1] = p[1] - 0.3;
        return p;
    }

    // ---- Device type ----
    int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.PASSTHROUGH;
    }

    // ---- Source behavior (PASSTHROUGH acts as source for downstream) ----
    bool LFPG_IsSource()
    {
        return true;
    }

    bool LFPG_GetSourceOn()
    {
        return m_PoweredNet;
    }

    // ---- Self-consumption: 10 u/s (EXPLICIT — avoids DeviceAPI fallback pitfall) ----
    float LFPG_GetConsumption()
    {
        return 10.0;
    }

    // ---- Throughput cap: 50 u/s ----
    float LFPG_GetCapacity()
    {
        return 50.0;
    }

    // ---- Block vanilla CompEM entirely ----
    override bool IsElectricAppliance()
    {
        return false;
    }

    override void OnWorkStart() {}
    override void OnWorkStop() {}
    override void OnWork(float consumed_energy) {}
    override void OnSwitchOn() {}
    override void OnSwitchOff() {}

    // ---- Powered state (called by graph propagation) ----
    void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;
        SetSynchDirty();

        LFPG_Util.Debug("[LF_CeilingLight] SetPowered(" + powered.ToString() + ") id=" + m_DeviceId);
        #endif
    }

    // ---- Overload/Warning masks ----
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

    int LFPG_GetWarningMask()
    {
        return m_WarningMask;
    }

    void LFPG_SetWarningMask(int mask)
    {
        #ifdef SERVER
        if (m_WarningMask != mask)
        {
            m_WarningMask = mask;
            SetSynchDirty();
        }
        #endif
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
    // Wire ownership API (output side)
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
            LFPG_Util.Warn("[LF_CeilingLight] AddWire rejected: not an output port: " + wd.m_SourcePort);
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
    // Persistence (Hallazgo 6: NO m_PoweredNet)
    // ============================================
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
        if (!super.OnStoreLoad(ctx, version))
            return false;

        if (!ctx.Read(m_DeviceIdLow))
        {
            LFPG_Util.Error("[LF_CeilingLight] OnStoreLoad: failed to read m_DeviceIdLow");
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            LFPG_Util.Error("[LF_CeilingLight] OnStoreLoad: failed to read m_DeviceIdHigh");
            return false;
        }

        LFPG_UpdateDeviceIdString();

        string json;
        if (!ctx.Read(json))
        {
            LFPG_Util.Error("[LF_CeilingLight] OnStoreLoad: failed to read wires json for " + m_DeviceId);
            return false;
        }
        LFPG_WireHelper.DeserializeJSON(m_Wires, json, "LF_CeilingLight");

        return true;
    }

    // ============================================
    // Client-side visual effects
    // ============================================
    protected void LFPG_CreateLight()
    {
        if (m_LFPG_Light)
            return;

        // Attach to "light" memory point if available
        if (MemoryPointExists("light"))
        {
            m_LFPG_Light = LFPG_CeilingLightEffect.Cast(ScriptedLightBase.CreateLightAtObjMemoryPoint(LFPG_CeilingLightEffect, this, "light"));
        }
        else
        {
            // Fallback: offset below device center (light hangs down)
            vector lightPos = GetPosition();
            lightPos[1] = lightPos[1] - 0.15;
            m_LFPG_Light = LFPG_CeilingLightEffect.Cast(ScriptedLightBase.CreateLight(LFPG_CeilingLightEffect, lightPos));
            if (m_LFPG_Light)
            {
                m_LFPG_Light.AttachOnObject(this);
            }
        }

        // Prevent auto-expiry and ensure immediate activation.
        // Without SetLifetime, ScriptedLightBase uses a short default
        // and the light silently disappears after a few seconds.
        if (m_LFPG_Light)
        {
            m_LFPG_Light.SetLifetime(1000000);
            m_LFPG_Light.SetEnabled(true);
        }
    }

    protected void LFPG_DestroyLight()
    {
        if (!m_LFPG_Light)
            return;

        m_LFPG_Light.FadeOut();
        m_LFPG_Light = null;
    }

    protected void LFPG_SetRvmatOn()
    {
        SetObjectMaterial(0, LFPG_CEILING_RVMAT_ON);
    }

    protected void LFPG_SetRvmatOff()
    {
        SetObjectMaterial(0, LFPG_CEILING_RVMAT_OFF);
    }
};
