// =========================================================
// LF_PowerGrid - Motion Sensor (v4.0 Refactor)
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

static const string LFPG_SENSOR_RVMAT_OFF    = "\\LFPowerGrid\\data\\sensor\\motion_sensor_off.rvmat";
static const string LFPG_SENSOR_RVMAT_GREEN  = "\\LFPowerGrid\\data\\sensor\\motion_sensor_green.rvmat";
static const string LFPG_SENSOR_RVMAT_RED    = "\\LFPowerGrid\\data\\sensor\\motion_sensor_red.rvmat";

// ---------------------------------------------------------
// KIT — same-model deploy pattern
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

    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[MotionSensor_Kit] OnPlacementComplete: param=";
        tLog = tLog + position.ToString();
        tLog = tLog + " kitPos=";
        tLog = tLog + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        string className = "LFPG_MotionSensor";
        EntityAI sensor = GetGame().CreateObjectEx(className, finalPos, ECE_CREATEPHYSICS);
        if (sensor)
        {
            sensor.SetPosition(finalPos);
            sensor.SetOrientation(finalOri);
            sensor.Update();

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

            string deployMsg = "[MotionSensor_Kit] Deployed LFPG_MotionSensor at ";
            deployMsg = deployMsg + finalPos.ToString();
            deployMsg = deployMsg + " ori=";
            deployMsg = deployMsg + finalOri.ToString();
            LFPG_Util.Info(deployMsg);

            GetGame().ObjectDelete(this);
        }
        else
        {
            string errKit = "[MotionSensor_Kit] Failed to create LFPG_MotionSensor! Kit preserved.";
            LFPG_Util.Error(errKit);
            PlayerBase pbFail = PlayerBase.Cast(player);
            if (pbFail)
            {
                string errPlayer = "[LFPG] Motion Sensor placement failed. Kit preserved.";
                pbFail.MessageStatus(errPlayer);
            }
        }
        #endif
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
        RegisterNetSyncVariableInt(varDetectMode);
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
        LFPG_NetworkManager.Get().RegisterMotionSensor(this);
        #endif
    }

    override void LFPG_OnKilled()
    {
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
    }

    override void LFPG_OnDeleted()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterMotionSensor(this);
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
    // ============================================
    bool LFPG_EvaluateDetection(array<Man> players)
    {
        #ifdef SERVER
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

        // Compute ray origin in open space (room-side)
        vector localDomeDir = Vector(0, -1, 0);
        vector domePt = ModelToWorld(localDomeDir);
        float domeX = domePt[0] - sensorPos[0];
        float domeY = domePt[1] - sensorPos[1];
        float domeZ = domePt[2] - sensorPos[2];

        float horizLenSq = domeX * domeX + domeZ * domeZ;
        float horizLen = Math.Sqrt(horizLenSq);

        vector sensorEye = sensorPos;

        if (horizLen > 0.1)
        {
            float hNorm = 0.4 / horizLen;
            sensorEye[0] = sensorEye[0] + domeX * hNorm;
            sensorEye[2] = sensorEye[2] + domeZ * hNorm;
            sensorEye[1] = sensorEye[1] + 0.15;
        }
        else if (domeY > 0.5)
        {
            sensorEye[1] = sensorEye[1] - 0.5;
        }
        else
        {
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

            playerPos = pb.GetPosition();
            distSq = LFPG_WorldUtil.DistSq(sensorPos, playerPos);
            if (distSq > LFPG_SENSOR_RANGE_SQ)
                continue;

            passesFilter = LFPG_CheckGroupFilter(pb);
            if (!passesFilter)
                continue;

            playerCenter = playerPos;
            playerCenter[1] = playerCenter[1] + 1.0;

            hasLOS = LFPG_CheckLineOfSight(sensorEye, playerCenter);
            if (!hasLOS)
                continue;

            detected = true;
        }

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

        float hitDistSq = LFPG_WorldUtil.DistSq(from, hitPos);
        float targetDistSq = LFPG_WorldUtil.DistSq(from, to);

        float marginSq = 0.09;
        if (hitDistSq >= targetDistSq - marginSq)
        {
            return true;
        }

        return false;
        #else
        return false;
        #endif
    }
};
