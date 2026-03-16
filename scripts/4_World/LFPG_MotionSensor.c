// =========================================================
// LF_PowerGrid - Motion Sensor (v1.5.0)
//
// LFPG_MotionSensor_Kit: Holdable, deployable (same-model pattern).
// LFPG_MotionSensor:     PASSTHROUGH, 1 IN (input_1) + 1 OUT (output_1).
//                         Self-consumption: 5 u/s. Gated passthrough.
//
// Behavior:
//   Centralized tick in NetworkManager scans nearby players.
//   If a player matching the detect mode is within range + LOS,
//   gate opens (m_GateOpen=true) → power passes through.
//   Otherwise gate closes → power blocked.
//
// Detect modes (m_DetectMode):
//   0 = DETECT_ALL   — any player in range+LOS opens gate
//   1 = DETECT_TEAM  — only players in same group as paired
//   2 = DETECT_ENEMY — only players NOT in paired group (or no group)
//
// Group pairing:
//   m_PairedGroupName captured from placing player on deployment.
//   Overwritten via hold-F ActionPairSensor.
//   Requires LBmaster_Groups (#ifdef). Without it, only DETECT_ALL works.
//
// LED states (hiddenSelections[0] = "sensor_led"):
//   Green = m_PoweredNet && m_GateOpen  (detecting, passing power)
//   Red   = m_PoweredNet && !m_GateOpen (armed, no detection)
//   Off   = !m_PoweredNet               (no upstream / disconnected)
//
// Port positions: Virtual (no memory points in p3d).
//
// Persistence: DeviceIdLow, DeviceIdHigh, m_DetectMode,
//              m_PairedGroupName, wiresJSON.
//   m_GateOpen NOT persisted (derived by scan tick).
//   m_PoweredNet NOT persisted (derived by graph propagation).
// =========================================================

// Detect mode constants
static const int LFPG_SENSOR_MODE_ALL   = 0;
static const int LFPG_SENSOR_MODE_TEAM  = 1;
static const int LFPG_SENSOR_MODE_ENEMY = 2;
static const int LFPG_SENSOR_MODE_COUNT = 3;

// LED rvmat paths (placeholder — reuse button rvmats until sensor-specific assets exist)
static const string LFPG_SENSOR_RVMAT_OFF   = "\\LFPowerGrid\\data\\button\\materials\\led_off.rvmat";
static const string LFPG_SENSOR_RVMAT_GREEN  = "\\LFPowerGrid\\data\\button\\materials\\led_green.rvmat";
static const string LFPG_SENSOR_RVMAT_RED    = "\\LFPowerGrid\\data\\button\\materials\\led_red.rvmat";

// ---------------------------------------------------------
// KIT - same-model deploy pattern (Splitter/PushButton parity)
// ---------------------------------------------------------
class LFPG_MotionSensor_Kit : Inventory_Base
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
        AddAction(LFPG_ActionPlaceMotionSensor);
    }

    // Usar parametro position/orientation, NUNCA GetPosition().
    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[MotionSensor_Kit] OnPlacementComplete: param=" + position.ToString();
        tLog = tLog + " kitPos=" + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        EntityAI sensor = GetGame().CreateObjectEx("LFPG_MotionSensor", finalPos, ECE_CREATEPHYSICS);
        if (sensor)
        {
            sensor.SetPosition(finalPos);
            sensor.SetOrientation(finalOri);
            sensor.Update();

            // Capture placing player's group name
            LFPG_MotionSensor ms = LFPG_MotionSensor.Cast(sensor);
            if (ms)
            {
                string groupName = "";
                #ifdef LBmaster_Groups
                PlayerBase pb = PlayerBase.Cast(player);
                if (pb)
                {
                    LBGroup grp = pb.GetLBGroup();
                    if (grp)
                    {
                        groupName = grp.name;
                    }
                }
                #endif
                ms.LFPG_SetPairedGroupName(groupName);
            }

            string deployMsg = "[MotionSensor_Kit] Deployed LFPG_MotionSensor at " + finalPos.ToString();
            deployMsg = deployMsg + " ori=" + finalOri.ToString();
            LFPG_Util.Info(deployMsg);

            // Solo borrar kit si spawn exitoso (Splitter parity).
            GetGame().ObjectDelete(this);
        }
        else
        {
            LFPG_Util.Error("[MotionSensor_Kit] Failed to create LFPG_MotionSensor! Kit preserved.");
            PlayerBase pbFail = PlayerBase.Cast(player);
            if (pbFail)
            {
                pbFail.MessageStatus("[LFPG] Motion Sensor placement failed. Kit preserved.");
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// DEVICE: PASSTHROUGH (1 IN + 1 OUT), gated by motion detection
// ---------------------------------------------------------
class LFPG_MotionSensor : Inventory_Base
{
    // ---- Device identity ----
    protected int m_DeviceIdLow = 0;
    protected int m_DeviceIdHigh = 0;
    protected string m_DeviceId;

    // ---- Wires owned (output side) ----
    protected ref array<ref LFPG_WireData> m_Wires;

    // ---- Power state (set by graph propagation) ----
    protected bool m_PoweredNet = false;

    // ---- Gate state (set by scan tick) ----
    protected bool m_GateOpen = false;

    // ---- Detection mode (SyncVar, 0=ALL, 1=TEAM, 2=ENEMY) ----
    protected int m_DetectMode = 0;

    // ---- Paired group name (server only, persisted) ----
    // NOT a SyncVar — strings cannot be SyncVars in Enforce.
    // Sent to client via inspector RPC if needed.
    protected string m_PairedGroupName = "";

    // ---- Overload state ----
    protected bool m_Overloaded = false;

    // ---- Deletion guard ----
    protected bool m_LFPG_Deleting = false;

    // ---- Reusable raycast result set (avoids alloc per LOS check) ----
    protected ref set<Object> m_RayResults;

    // ============================================
    // Constructor - SyncVars en constructor, NO EEInit
    // ============================================
    void LFPG_MotionSensor()
    {
        m_Wires = new array<ref LFPG_WireData>;
        m_RayResults = new set<Object>;
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
        RegisterNetSyncVariableBool("m_GateOpen");
        RegisterNetSyncVariableInt("m_DetectMode");
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
        AddAction(LFPG_ActionCycleDetectMode);
        AddAction(LFPG_ActionPairSensor);
    }

    // ============================================
    // Inventory guards (prevent pickup — breaks wires)
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
        #endif

        LFPG_UpdateDeviceIdString();
        LFPG_TryRegister();

        #ifdef SERVER
        LFPG_NetworkManager.Get().BroadcastOwnerWires(this);
        // Register with centralized motion sensor tick
        LFPG_NetworkManager.Get().RegisterMotionSensor(this);
        #endif
    }

    override void EEKilled(Object killer)
    {
        LFPG_DeviceLifecycle.OnDeviceKilled(this, m_DeviceId);

        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterMotionSensor(this);

        bool dirty = false;
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            dirty = true;
        }
        if (m_GateOpen)
        {
            m_GateOpen = false;
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

        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterMotionSensor(this);
        #endif

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
            if (m_GateOpen)
            {
                m_GateOpen = false;
                locDirty = true;
            }
            if (locDirty)
            {
                SetSynchDirty();
            }
        }
        #endif
    }

    // ============================================
    // Client sync
    // ============================================
    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();
        LFPG_TryRegister();

        #ifndef SERVER
        LFPG_UpdateVisuals();

        // CableRenderer sync (Splitter parity)
        if (m_DeviceId != "")
        {
            LFPG_CableRenderer r = LFPG_CableRenderer.Get();
            if (r)
            {
                r.RequestDeviceSync(m_DeviceId, this);
                if (r.HasOwnerData(m_DeviceId))
                {
                    r.NotifyOwnerVisualChanged(m_DeviceId);
                }
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
        if (m_PoweredNet && m_GateOpen)
        {
            SetObjectMaterial(0, LFPG_SENSOR_RVMAT_GREEN);
        }
        else if (m_PoweredNet)
        {
            SetObjectMaterial(0, LFPG_SENSOR_RVMAT_RED);
        }
        else
        {
            SetObjectMaterial(0, LFPG_SENSOR_RVMAT_OFF);
        }
        #endif
    }

    // ============================================
    // Sensor-specific API
    // ============================================

    // Called by ActionCycleDetectMode (server only)
    void LFPG_CycleDetectMode()
    {
        #ifdef SERVER
        int newMode = m_DetectMode + 1;
        if (newMode >= LFPG_SENSOR_MODE_COUNT)
        {
            newMode = 0;
        }

        // Without LBmaster_Groups, only DETECT_ALL is available
        #ifndef LBmaster_Groups
        newMode = LFPG_SENSOR_MODE_ALL;
        #endif

        m_DetectMode = newMode;
        SetSynchDirty();

        string modeMsg = "[LFPG_MotionSensor] CycleDetectMode → " + m_DetectMode.ToString();
        modeMsg = modeMsg + " id=" + m_DeviceId;
        LFPG_Util.Info(modeMsg);
        #endif
    }

    // Called by ActionPairSensor + Kit.OnPlacementComplete (server only)
    void LFPG_SetPairedGroupName(string groupName)
    {
        #ifdef SERVER
        m_PairedGroupName = groupName;

        string pairMsg = "[LFPG_MotionSensor] Paired to group: '" + m_PairedGroupName + "'";
        pairMsg = pairMsg + " id=" + m_DeviceId;
        LFPG_Util.Info(pairMsg);
        #endif
    }

    string LFPG_GetPairedGroupName()
    {
        return m_PairedGroupName;
    }

    int LFPG_GetDetectMode()
    {
        return m_DetectMode;
    }

    bool LFPG_GetGateOpen()
    {
        return m_GateOpen;
    }

    // Called by LFPG_TickMotionSensors in NetworkManager (server only).
    // Receives shared player list to avoid N GetPlayers() calls per tick.
    // Returns true if gate state changed (needs propagation).
    bool LFPG_EvaluateDetection(array<Man> players)
    {
        #ifdef SERVER
        // Only scan if powered — unpowered sensor cannot detect
        if (!m_PoweredNet)
        {
            if (m_GateOpen)
            {
                m_GateOpen = false;
                SetSynchDirty();
                return true;
            }
            return false;
        }

        vector sensorPos = GetPosition();

        // Compute ray origin that is clearly in open space (room-side).
        // Old code just added +0.5 Y, which on a wall mount leaves the
        // eye at ~3cm from the wall surface — inside the collision hull.
        //
        // Strategy: use the model's dome direction (-Y local) transformed
        // to world space. The horizontal component pushes the eye away
        // from the wall, while a vertical baseline ensures floor mounts
        // still work (dome points down → no horizontal component).
        vector localDomeDir = Vector(0, -1, 0);
        vector domePt = ModelToWorld(localDomeDir);
        float domeX = domePt[0] - sensorPos[0];
        float domeY = domePt[1] - sensorPos[1];
        float domeZ = domePt[2] - sensorPos[2];

        // Horizontal component of dome direction (XZ plane)
        float horizLenSq = domeX * domeX + domeZ * domeZ;
        float horizLen = Math.Sqrt(horizLenSq);

        vector sensorEye = sensorPos;

        if (horizLen > 0.1)
        {
            // Wall mount: significant horizontal dome component.
            // Push eye 0.4m horizontally into the room (away from wall).
            float hNorm = 0.4 / horizLen;
            sensorEye[0] = sensorEye[0] + domeX * hNorm;
            sensorEye[2] = sensorEye[2] + domeZ * hNorm;
            // Small vertical offset above sensor center
            sensorEye[1] = sensorEye[1] + 0.15;
        }
        else if (domeY > 0.5)
        {
            // Ceiling mount: pitch=180 flips model, dome(-Y local) maps
            // to +Y world → domeY is positive. Offset downward into room.
            sensorEye[1] = sensorEye[1] - 0.5;
        }
        else
        {
            // Floor mount: dome faces down, no horizontal component.
            // +0.5 Y above ground clears terrain collision.
            sensorEye[1] = sensorEye[1] + 0.5;
        }

        bool detected = false;

        int i;
        int pCount = players.Count();
        Man man;
        PlayerBase pb;
        vector playerPos;
        float distSq;
        bool passesFilter;
        vector playerCenter;
        bool hasLOS;

        for (i = 0; i < pCount; i = i + 1)
        {
            if (detected)
                break;

            man = players[i];
            if (!man)
                continue;

            if (!man.IsAlive())
                continue;

            pb = PlayerBase.Cast(man);
            if (!pb)
                continue;

            // 1. Distance check (squared, no sqrt — precomputed constant)
            playerPos = pb.GetPosition();
            distSq = LFPG_WorldUtil.DistSq(sensorPos, playerPos);
            if (distSq > LFPG_SENSOR_RANGE_SQ)
                continue;

            // 2. Group filter BEFORE raycast (cheaper than ray)
            passesFilter = LFPG_CheckGroupFilter(pb);
            if (!passesFilter)
                continue;

            // 3. LOS raycast — sensor eye → player center of mass
            playerCenter = playerPos;
            playerCenter[1] = playerCenter[1] + 1.0;

            hasLOS = LFPG_CheckLineOfSight(sensorEye, playerCenter);
            if (!hasLOS)
                continue;

            // Player passed all checks
            detected = true;
        }

        // Update gate state
        bool oldGate = m_GateOpen;
        m_GateOpen = detected;

        if (m_GateOpen != oldGate)
        {
            SetSynchDirty();
            return true;
        }

        return false;
        #else
        return false;
        #endif
    }

    // Check if player passes the group filter for current detect mode.
    // Returns true if player should trigger the sensor.
    protected bool LFPG_CheckGroupFilter(PlayerBase player)
    {
        #ifdef SERVER
        // DETECT_ALL: any player triggers
        if (m_DetectMode == LFPG_SENSOR_MODE_ALL)
            return true;

        // No paired group: fallback to DETECT_ALL behavior
        if (m_PairedGroupName == "")
            return true;

        // Get player's group name
        string playerGroup = "";
        #ifdef LBmaster_Groups
        LBGroup grp = player.GetLBGroup();
        if (grp)
        {
            playerGroup = grp.name;
        }
        #endif

        bool sameGroup = false;
        if (playerGroup != "" && playerGroup == m_PairedGroupName)
        {
            sameGroup = true;
        }

        if (m_DetectMode == LFPG_SENSOR_MODE_TEAM)
        {
            return sameGroup;
        }

        if (m_DetectMode == LFPG_SENSOR_MODE_ENEMY)
        {
            return !sameGroup;
        }

        return true;
        #else
        return false;
        #endif
    }

    // Line-of-sight check: raycast from sensor to player.
    // Returns true if no geometry blocks the path.
    // Uses m_RayResults member to avoid allocation per call.
    protected bool LFPG_CheckLineOfSight(vector from, vector to)
    {
        #ifdef SERVER
        vector hitPos;
        vector hitNormal;
        int contactComponent;
        m_RayResults.Clear();
        Object hitWith = null;
        bool bSorted = true;
        bool bGround = false;
        float rayRadius = 0.02;

        bool hit = DayZPhysics.RaycastRV(from, to, hitPos, hitNormal, contactComponent, m_RayResults, hitWith, this, bSorted, bGround, ObjIntersectFire, rayRadius);

        if (!hit)
        {
            // No collision at all → clear LOS
            return true;
        }

        // Check if the first hit is the player or beyond the player
        float hitDistSq = LFPG_WorldUtil.DistSq(from, hitPos);
        float targetDistSq = LFPG_WorldUtil.DistSq(from, to);

        // If hit is at or beyond target distance → LOS clear
        // Small margin (0.3m) to account for player collision geometry
        float marginSq = 0.09;
        if (hitDistSq >= targetDistSq - marginSq)
        {
            return true;
        }

        // Something is blocking the path before reaching the player
        return false;
        #else
        return false;
        #endif
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
        if (idx == 0) return "Input 1";
        if (idx == 1) return "Output 1";
        return "";
    }

    bool LFPG_HasPort(string portName, int dir)
    {
        if (dir == LFPG_PortDir.IN && portName == "input_1") return true;
        if (dir == LFPG_PortDir.OUT && portName == "output_1") return true;
        return false;
    }

    // Virtual port positions
    vector LFPG_GetPortWorldPos(string portName)
    {
        string memPoint = "port_" + portName;
        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

        vector offset = "0 0.02 0";
        if (portName == "input_1")
        {
            offset = "0 0.02 -0.03";
        }
        else if (portName == "output_1")
        {
            offset = "0 0.02 0.03";
        }

        return ModelToWorld(offset);
    }

    // ---- PASSTHROUGH device type ----
    int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.PASSTHROUGH;
    }

    // ---- Source behavior ----
    bool LFPG_IsSource()
    {
        return true;
    }

    bool LFPG_GetSourceOn()
    {
        return m_PoweredNet;
    }

    // Gated passthrough: gate controlled by detection scan
    bool LFPG_IsGateOpen()
    {
        return m_GateOpen;
    }

    bool LFPG_IsGateCapable()
    {
        return true;
    }

    // 5 u/s self-consumption (active sensor draws power)
    float LFPG_GetConsumption()
    {
        return LFPG_SENSOR_CONSUMPTION;
    }

    float LFPG_GetCapacity()
    {
        return LFPG_DEFAULT_PASSTHROUGH_CAPACITY;
    }

    bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    // Called by graph propagation when upstream power state changes.
    void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;

        // When power is lost, close gate immediately
        if (!powered && m_GateOpen)
        {
            m_GateOpen = false;
        }

        SetSynchDirty();

        string pwrMsg = "[LFPG_MotionSensor] SetPowered(" + powered.ToString() + ") id=" + m_DeviceId;
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
    // Wire ownership API (Splitter parity)
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
            LFPG_Util.Warn("[LFPG_MotionSensor] AddWire rejected: not an output port: " + wd.m_SourcePort);
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
        ctx.Write(m_DetectMode);
        ctx.Write(m_PairedGroupName);
        // m_PoweredNet: NOT persisted (derived by graph propagation)
        // m_GateOpen:   NOT persisted (derived by scan tick)

        string json = "";
        LFPG_WireHelper.SerializeJSON(m_Wires, json);
        ctx.Write(json);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version))
            return false;

        if (!ctx.Read(m_DeviceIdLow))
        {
            LFPG_Util.Error("[LFPG_MotionSensor] OnStoreLoad: failed to read m_DeviceIdLow");
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            LFPG_Util.Error("[LFPG_MotionSensor] OnStoreLoad: failed to read m_DeviceIdHigh");
            return false;
        }

        if (!ctx.Read(m_DetectMode))
        {
            LFPG_Util.Error("[LFPG_MotionSensor] OnStoreLoad: failed to read m_DetectMode");
            return false;
        }

        if (!ctx.Read(m_PairedGroupName))
        {
            LFPG_Util.Error("[LFPG_MotionSensor] OnStoreLoad: failed to read m_PairedGroupName");
            return false;
        }

        LFPG_UpdateDeviceIdString();

        string json = "";
        if (!ctx.Read(json))
        {
            LFPG_Util.Error("[LFPG_MotionSensor] OnStoreLoad: failed to read wires json for " + m_DeviceId);
            return false;
        }
        LFPG_WireHelper.DeserializeJSON(m_Wires, json, "LFPG_MotionSensor");

        return true;
    }
};
