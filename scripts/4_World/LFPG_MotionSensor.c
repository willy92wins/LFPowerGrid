// =========================================================
// LF_PowerGrid - Motion Sensor (v4.2)
//
// LFPG_MotionSensor_Kit: Holdable, deployable (same-model pattern).
// LFPG_MotionSensor:     PASSTHROUGH, 1 IN (input_1) + 1 OUT (output_1).
//                         Self-consumption: 5 u/s. Gated passthrough.
//
// v4.0: Migrated from Inventory_Base to LFPG_WireOwnerBase.
//   Wire store, wire API, persistence wireJSON, CanConnectTo — all in base.
//   GetPortWorldPos override required: p3d uses port_input_0/port_output_0
//   but LFPG ports are input_1/output_1.
//
// v4.1 (Audit Fix):
//   - Dual-ray LOS (standing 1.0m + crouching 0.4m)
//   - Linear LOS margin (0.3m fixed, was 3mm@15m)
//   - Gate hold timer (5s anti-flicker)
//   - SyncVar int range (0-2 for DetectMode)
//   - LBmaster_Groups guard on persistence load
//
// v4.2:
//   - 360° omnidirectional detection (removed 120° FOV cone)
//   - Walls and objects block LOS via raycast
//   - Pipeline: Range sphere → Group filter → LOS raycast
//
// Behavior:
//   Centralized tick in NetworkManager scans nearby players.
//   Gate opens on detection → power passes through.
//
// Detect modes: 0=ALL, 1=TEAM, 2=ENEMY
// LED: Green=powered+open, Red=powered+closed, Off=unpowered
// =========================================================

static const int LFPG_SENSOR_MODE_ALL   = 0;
static const int LFPG_SENSOR_MODE_TEAM  = 1;
static const int LFPG_SENSOR_MODE_ENEMY = 2;
static const int LFPG_SENSOR_MODE_COUNT = 3;

static const string LFPG_SENSOR_RVMAT_OFF    = "\LFPowerGrid\data\sensor\motion_sensor_off.rvmat";
static const string LFPG_SENSOR_RVMAT_GREEN  = "\LFPowerGrid\data\sensor\motion_sensor_green.rvmat";
static const string LFPG_SENSOR_RVMAT_RED    = "\LFPowerGrid\data\sensor\motion_sensor_red.rvmat";

class LFPG_MotionSensor_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_MotionSensor";
    }

    override int LFPG_GetPlacementModes()
    {
        return 2;
    }

    override float LFPG_GetWallSurfaceOffset()
    {
        return 0.08;
    }

    override float LFPG_GetWallPitchOffset()
    {
        return -90.0;
    }
};

// ---------------------------------------------------------
// DEVICE — PASSTHROUGH : LFPG_WireOwnerBase
// 1 IN (input_1) + 1 OUT (output_1), gated by motion detection
// ---------------------------------------------------------
class LFPG_MotionSensor : LFPG_WireOwnerBase
{
    // ---- Device-specific SyncVars ----
    protected bool m_PoweredNet = false;
    protected bool m_GateOpen   = false;
    protected int  m_DetectMode = 0;
    protected bool m_Overloaded = false;

    // ---- Server-only (NOT SyncVar, strings cannot be SyncVars) ----
    protected string m_PairedGroupName = "";

    // ---- Server-only: gate hold timer (seconds, game time) ----
    protected float m_GateHoldUntil = 0.0;

    // ---- Reusable raycast result set (avoids alloc per LOS check) ----
    protected ref set<Object> m_RayResults;

    // ============================================
    // Constructor — ports + SyncVars
    // ============================================
    void LFPG_MotionSensor()
    {
        m_RayResults = new set<Object>;

        string pIn = "input_1";
        string lIn = "Input 1";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, lIn);

        string pOut = "output_1";
        string lOut = "Output 1";
        LFPG_AddPort(pOut, LFPG_PortDir.OUT, lOut);

        string varPowered    = "m_PoweredNet";
        string varGateOpen   = "m_GateOpen";
        string varDetectMode = "m_DetectMode";
        string varOverloaded = "m_Overloaded";

        RegisterNetSyncVariableBool(varPowered);
        RegisterNetSyncVariableBool(varGateOpen);
        RegisterNetSyncVariableInt(varDetectMode, 0, 2);
        RegisterNetSyncVariableBool(varOverloaded);
    }

    // ============================================
    // SetActions
    // ============================================
    override void SetActions()
    {
        super.SetActions();
        AddAction(LFPG_ActionCycleDetectMode);
        AddAction(LFPG_ActionPairSensor);
    }

    // ============================================
    // Port world pos override — p3d uses _0, LFPG uses _1
    // ============================================
    override vector LFPG_GetPortWorldPos(string portName)
    {
        string memPoint = "";
        string inName = "input_1";
        string outName = "output_1";

        if (portName == inName)
        {
            memPoint = "port_input_0";
        }
        else if (portName == outName)
        {
            memPoint = "port_output_0";
        }

        if (memPoint != "")
        {
            if (MemoryPointExists(memPoint))
            {
                return ModelToWorld(GetMemoryPointPos(memPoint));
            }
        }

        // Fallback: hardcoded offsets
        vector offset = "0 0.02 0";
        if (portName == inName)
        {
            offset = "0 0.02 -0.03";
        }
        else if (portName == outName)
        {
            offset = "0 0.02 0.03";
        }

        return ModelToWorld(offset);
    }

    // ============================================
    // Virtual interface — PASSTHROUGH
    // ============================================
    override int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.PASSTHROUGH;
    }

    override float LFPG_GetConsumption()
    {
        return LFPG_SENSOR_CONSUMPTION;
    }

    override float LFPG_GetCapacity()
    {
        return LFPG_DEFAULT_PASSTHROUGH_CAPACITY;
    }

    override bool LFPG_IsSource()
    {
        return true;
    }

    override bool LFPG_GetSourceOn()
    {
        return m_PoweredNet;
    }

    override bool LFPG_IsGateCapable()
    {
        return true;
    }

    override bool LFPG_IsGateOpen()
    {
        return m_GateOpen;
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

        if (!powered && m_GateOpen)
        {
            m_GateOpen = false;
            m_GateHoldUntil = 0.0;
        }

        SetSynchDirty();

        string pwrMsg = "[LFPG_MotionSensor] SetPowered(";
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

    // ============================================
    // Lifecycle hooks
    // ============================================
    override void LFPG_OnInitDevice()
    {
        #ifdef SERVER
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm) nm.RegisterMotionSensor(this);
        #endif
    }

    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm) nm.UnregisterMotionSensor(this);

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
    }

    override void LFPG_OnDeleted()
    {
        #ifdef SERVER
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm) nm.UnregisterMotionSensor(this);
        #endif
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
        if (m_GateOpen)
        {
            m_GateOpen = false;
            locDirty = true;
        }
        m_GateHoldUntil = 0.0;
        if (locDirty)
        {
            SetSynchDirty();
        }
        #endif
    }

    // ============================================
    // VarSync: LED visual
    // ============================================
    override void LFPG_OnVarSyncDevice()
    {
        #ifndef SERVER
        LFPG_UpdateVisuals();
        #endif
    }

    // ============================================
    // Persistence: DetectMode + PairedGroupName
    // ============================================
    override void LFPG_OnStoreSaveDevice(ParamsWriteContext ctx)
    {
        ctx.Write(m_DetectMode);
        ctx.Write(m_PairedGroupName);
    }

    override bool LFPG_OnStoreLoadDevice(ParamsReadContext ctx, int deviceVer)
    {
        if (!ctx.Read(m_DetectMode))
        {
            string errMode = "[LFPG_MotionSensor] OnStoreLoad failed: m_DetectMode";
            LFPG_Util.Error(errMode);
            return false;
        }

        // v4.1: If LBmaster_Groups is not compiled in, force ALL mode.
        // Prevents stale TEAM/ENEMY mode from a previous server config.
        #ifndef LBmaster_Groups
        m_DetectMode = 0;
        #endif

        if (!ctx.Read(m_PairedGroupName))
        {
            string errGroup = "[LFPG_MotionSensor] OnStoreLoad failed: m_PairedGroupName";
            LFPG_Util.Error(errGroup);
            return false;
        }

        return true;
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
    void LFPG_CycleDetectMode()
    {
        #ifdef SERVER
        int newMode = m_DetectMode + 1;
        if (newMode >= LFPG_SENSOR_MODE_COUNT)
        {
            newMode = 0;
        }

        #ifndef LBmaster_Groups
        newMode = LFPG_SENSOR_MODE_ALL;
        #endif

        m_DetectMode = newMode;
        SetSynchDirty();

        string modeMsg = "[LFPG_MotionSensor] CycleDetectMode → ";
        modeMsg = modeMsg + m_DetectMode.ToString();
        modeMsg = modeMsg + " id=";
        modeMsg = modeMsg + m_DeviceId;
        LFPG_Util.Info(modeMsg);
        #endif
    }

    void LFPG_SetPairedGroupName(string groupName)
    {
        #ifdef SERVER
        m_PairedGroupName = groupName;

        string pairMsg = "[LFPG_MotionSensor] Paired to group: '";
        pairMsg = pairMsg + m_PairedGroupName;
        pairMsg = pairMsg + "' id=";
        pairMsg = pairMsg + m_DeviceId;
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

    // ============================================
    // Detection evaluation (called by NM tick, server only)
    //
    // Pipeline per player (360° omnidirectional):
    //   1. Range sphere (15m)
    //   2. Group filter (ALL / TEAM / ENEMY)
    //   3. LOS raycast (dual height: standing + crouching)
    //
    // Walls and objects block LOS naturally via raycast.
    //
    // Gate hold timer: once opened, stays open for
    // LFPG_SENSOR_HOLD_SEC even if no player detected,
    // preventing downstream power flickering.
    //
    // Returns true if gate state changed (needs propagate).
    // ============================================
    bool LFPG_EvaluateDetection(array<Man> players)
    {
        #ifdef SERVER
        if (!m_PoweredNet)
        {
            if (m_GateOpen)
            {
                m_GateOpen = false;
                m_GateHoldUntil = 0.0;
                SetSynchDirty();
                return true;
            }
            return false;
        }

        vector sensorPos = GetPosition();

        // ---- Compute sensorEye (raycast origin in open space) ----
        // mountProbeDir (0,-1,0) is a reference vector to determine
        // mounting surface. NOT the dome direction (dome is +Z).
        vector mountProbe = Vector(0, -1, 0);
        vector probePt = ModelToWorld(mountProbe);
        float probeX = probePt[0] - sensorPos[0];
        float probeY = probePt[1] - sensorPos[1];
        float probeZ = probePt[2] - sensorPos[2];

        float horizLenSq = probeX * probeX + probeZ * probeZ;
        float horizLen = Math.Sqrt(horizLenSq);

        vector sensorEye = sensorPos;

        if (horizLen > 0.1)
        {
            // Wall mount: push eye outward from wall surface
            float hNorm = 0.4 / horizLen;
            sensorEye[0] = sensorEye[0] + probeX * hNorm;
            sensorEye[2] = sensorEye[2] + probeZ * hNorm;
            sensorEye[1] = sensorEye[1] + 0.15;
        }
        else if (probeY > 0.5)
        {
            // Ceiling mount: eye below sensor
            sensorEye[1] = sensorEye[1] - 0.5;
        }
        else
        {
            // Floor mount: eye above sensor
            sensorEye[1] = sensorEye[1] + 0.5;
        }

        // ---- Scan players (360° omnidirectional) ----
        bool detected = false;
        float nowSec = g_Game.GetTickTime();

        int i;
        int pCount = players.Count();
        Man man;
        PlayerBase pb;
        vector playerPos;
        float distSq;
        bool passesFilter;
        vector targetHigh;
        vector targetLow;
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

            // 1. Range sphere (15m)
            playerPos = pb.GetPosition();
            distSq = LFPG_WorldUtil.DistSq(sensorPos, playerPos);
            if (distSq > LFPG_SENSOR_RANGE_SQ)
                continue;

            // 2. Group filter
            passesFilter = LFPG_CheckGroupFilter(pb);
            if (!passesFilter)
                continue;

            // 3. LOS dual-ray (standing torso + crouching center)
            targetHigh = playerPos;
            targetHigh[1] = targetHigh[1] + LFPG_SENSOR_TARGET_HIGH;
            hasLOS = LFPG_CheckLineOfSight(sensorEye, targetHigh);
            if (!hasLOS)
            {
                targetLow = playerPos;
                targetLow[1] = targetLow[1] + LFPG_SENSOR_TARGET_LOW;
                hasLOS = LFPG_CheckLineOfSight(sensorEye, targetLow);
            }
            if (!hasLOS)
                continue;

            detected = true;
        }

        // ---- Gate hold timer ----
        if (detected)
        {
            m_GateHoldUntil = nowSec + LFPG_SENSOR_HOLD_SEC;
        }

        bool shouldBeOpen = detected;
        if (!shouldBeOpen && nowSec < m_GateHoldUntil)
        {
            shouldBeOpen = true;
        }

        bool oldGate = m_GateOpen;
        m_GateOpen = shouldBeOpen;

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

    // ============================================
    // Group filter check
    // ============================================
    protected bool LFPG_CheckGroupFilter(PlayerBase player)
    {
        #ifdef SERVER
        if (m_DetectMode == LFPG_SENSOR_MODE_ALL)
            return true;

        if (m_PairedGroupName == "")
            return true;

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

    // ============================================
    // LOS raycast check
    // ============================================
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
            return true;
        }

        // v4.1: Linear margin (fixed 0.3m regardless of distance).
        // Previous squared margin (0.09) gave only 3mm at 15m.
        float hitDist = Math.Sqrt(LFPG_WorldUtil.DistSq(from, hitPos));
        float targetDist = Math.Sqrt(LFPG_WorldUtil.DistSq(from, to));

        if (hitDist >= targetDist - LFPG_SENSOR_LOS_MARGIN)
        {
            return true;
        }

        return false;
        #else
        return false;
        #endif
    }
};
