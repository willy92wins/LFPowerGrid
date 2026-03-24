// =========================================================
// LF_PowerGrid - Laser Detector / Beam Sensor (v2.0.0 — Refactor)
//
// LFPG_LaserDetector: PASSTHROUGH, 1 IN + 1 OUT. GATE.
//   Emits a visible laser beam (raycast for wall occlusion).
//   Gate opens when a player crosses the beam.
//   Extends LFPG_WireOwnerBase (Refactor v4.1).
//
// NM Registration: RegisterLaserDetector / UnregisterLaserDetector
//   (centralized detection tick, NOT per-device CallLater)
//
// Special: m_BeamLength is a SyncVar float (0-5.5, 3 bits precision)
//   for client-side beam rendering via LFPG_LaserBeamRenderer.
//
// Persistence: [base: DeviceId + ver + wireJSON] — no extras
// =========================================================

static const string LFPG_LASER_RVMAT_OFF = "\\LFPowerGrid\\data\\button\\materials\\led_off.rvmat";
static const string LFPG_LASER_RVMAT_RED = "\\LFPowerGrid\\data\\laser_detector\\laser_detector_red.rvmat";

class LFPG_LaserDetector_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_LaserDetector";
    }

    override int LFPG_GetPlacementModes()
    {
        return 1;
    }

    override float LFPG_GetPitchOffset()
    {
        return 90.0;
    }
};

// ---------------------------------------------------------
// DEVICE: PASSTHROUGH, beam sensor GATE
// ---------------------------------------------------------
class LFPG_LaserDetector : LFPG_WireOwnerBase
{
    // ---- SyncVars ----
    protected bool  m_PoweredNet = false;
    protected bool  m_GateOpen   = false;
    protected bool  m_Overloaded = false;
    protected float m_BeamLength = 0.0;

    // ---- Non-synced ----
    protected ref set<Object> m_RayResults;

    void LFPG_LaserDetector()
    {
        m_RayResults = new set<Object>;

        string varPowered  = "m_PoweredNet";
        string varGate     = "m_GateOpen";
        string varOverload = "m_Overloaded";
        string varBeam     = "m_BeamLength";
        RegisterNetSyncVariableBool(varPowered);
        RegisterNetSyncVariableBool(varGate);
        RegisterNetSyncVariableBool(varOverload);
        RegisterNetSyncVariableFloat(varBeam, 0.0, 5.5, 3);

        string pIn  = "input_1";
        string pOut = "output_1";
        string lIn  = "Input 1";
        string lOut = "Output 1";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, lIn);
        LFPG_AddPort(pOut, LFPG_PortDir.OUT, lOut);
    }

    // ---- DeviceAPI ----
    override int LFPG_GetDeviceType() { return LFPG_DeviceType.PASSTHROUGH; }
    override bool LFPG_IsSource() { return true; }
    override bool LFPG_GetSourceOn() { return m_PoweredNet; }
    override bool LFPG_IsGateCapable() { return true; }
    override bool LFPG_IsGateOpen() { return m_GateOpen; }
    override float LFPG_GetConsumption() { return LFPG_LASER_CONSUMPTION; }
    override float LFPG_GetCapacity() { return LFPG_LASER_CAPACITY; }
    override bool LFPG_IsPowered() { return m_PoweredNet; }
    override bool LFPG_GetOverloaded() { return m_Overloaded; }

    override void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;

        if (!powered)
        {
            if (m_GateOpen)
            {
                m_GateOpen = false;
            }
            if (m_BeamLength > 0.0)
            {
                m_BeamLength = 0.0;
            }
        }
        else
        {
            LFPG_UpdateBeamRaycast();
        }

        SetSynchDirty();

        string pwrMsg = "[LFPG_LaserDetector] SetPowered(";
        pwrMsg = pwrMsg + powered.ToString();
        pwrMsg = pwrMsg + ") id=";
        pwrMsg = pwrMsg + m_DeviceId;
        LFPG_Util.Debug(pwrMsg);
        #endif
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

    // ---- Beam accessors ----
    float LFPG_GetBeamLength()
    {
        return m_BeamLength;
    }

    vector LFPG_GetBeamStart()
    {
        string memPoint = "light_led";
        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }
        vector ledOffset = "0.025 0.028 0.0";
        return ModelToWorld(ledOffset);
    }

    vector LFPG_GetBeamDirection()
    {
        vector origin = ModelToWorld("0 0 0");
        vector fwd = ModelToWorld("1 0 0");
        float dx = fwd[0] - origin[0];
        float dy = fwd[1] - origin[1];
        float dz = fwd[2] - origin[2];
        float len = Math.Sqrt(dx * dx + dy * dy + dz * dz);
        if (len < 0.001)
        {
            return Vector(0, 0, 1);
        }
        float invLen = 1.0 / len;
        return Vector(dx * invLen, dy * invLen, dz * invLen);
    }

    vector LFPG_GetBeamEnd()
    {
        vector beamStart = LFPG_GetBeamStart();
        vector beamDir = LFPG_GetBeamDirection();
        float len = m_BeamLength;
        if (len < 0.01)
        {
            len = 0.01;
        }
        float eX = beamStart[0] + beamDir[0] * len;
        float eY = beamStart[1] + beamDir[1] * len;
        float eZ = beamStart[2] + beamDir[2] * len;
        return Vector(eX, eY, eZ);
    }

    // ============================================
    // Beam raycast (called by NM centralized tick)
    // ============================================
    bool LFPG_UpdateBeamRaycast()
    {
        #ifdef SERVER
        if (!m_PoweredNet)
        {
            if (m_BeamLength > 0.0)
            {
                m_BeamLength = 0.0;
                SetSynchDirty();
                return true;
            }
            return false;
        }

        vector beamStart = LFPG_GetBeamStart();
        vector beamDir = LFPG_GetBeamDirection();

        float maxRange = LFPG_LASER_BEAM_RANGE_M;
        float endX = beamStart[0] + beamDir[0] * maxRange;
        float endY = beamStart[1] + beamDir[1] * maxRange;
        float endZ = beamStart[2] + beamDir[2] * maxRange;
        vector beamEnd = Vector(endX, endY, endZ);

        vector hitPos;
        vector hitNormal;
        int contactComponent;
        m_RayResults.Clear();
        Object hitWith = null;
        bool bSorted = true;
        bool bGround = false;
        float rayRadius = 0.01;

        bool hit = DayZPhysics.RaycastRV(beamStart, beamEnd, hitPos, hitNormal, contactComponent, m_RayResults, hitWith, this, bSorted, bGround, ObjIntersectFire, rayRadius);

        float newLength = maxRange;
        if (hit)
        {
            float hdx = hitPos[0] - beamStart[0];
            float hdy = hitPos[1] - beamStart[1];
            float hdz = hitPos[2] - beamStart[2];
            float hitDist = Math.Sqrt(hdx * hdx + hdy * hdy + hdz * hdz);
            if (hitDist < maxRange)
            {
                newLength = hitDist;
            }
        }

        float diff = newLength - m_BeamLength;
        if (diff < 0.0)
        {
            diff = diff * -1.0;
        }
        if (diff > 0.05)
        {
            m_BeamLength = newLength;
            SetSynchDirty();
            return true;
        }

        return false;
        #else
        return false;
        #endif
    }

    // ============================================
    // Crossing detection (called by NM centralized tick)
    // ============================================
    bool LFPG_EvaluateCrossing(array<Man> players)
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

        if (m_BeamLength < 0.1)
        {
            if (m_GateOpen)
            {
                m_GateOpen = false;
                SetSynchDirty();
                return true;
            }
            return false;
        }

        vector beamStart = LFPG_GetBeamStart();
        vector beamDir = LFPG_GetBeamDirection();
        float bEndX = beamStart[0] + beamDir[0] * m_BeamLength;
        float bEndY = beamStart[1] + beamDir[1] * m_BeamLength;
        float bEndZ = beamStart[2] + beamDir[2] * m_BeamLength;

        bool detected = false;
        float crossRadiusSq = LFPG_LASER_CROSS_RADIUS_SQ;

        int i;
        int pCount = players.Count();
        Man man;
        PlayerBase pb;
        vector playerPos;
        vector playerLow;
        vector playerHigh;
        float distSqLow;
        float distSqHigh;

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

            playerLow = playerPos;
            playerLow[1] = playerLow[1] + 0.5;

            playerHigh = playerPos;
            playerHigh[1] = playerHigh[1] + 1.4;

            distSqLow = LFPG_PointToSegDistSq(playerLow, beamStart, bEndX, bEndY, bEndZ, beamDir);
            if (distSqLow < crossRadiusSq)
            {
                detected = true;
            }

            if (!detected)
            {
                distSqHigh = LFPG_PointToSegDistSq(playerHigh, beamStart, bEndX, bEndY, bEndZ, beamDir);
                if (distSqHigh < crossRadiusSq)
                {
                    detected = true;
                }
            }
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

    protected float LFPG_PointToSegDistSq(vector p, vector segStart, float segEndX, float segEndY, float segEndZ, vector segDir)
    {
        float apX = p[0] - segStart[0];
        float apY = p[1] - segStart[1];
        float apZ = p[2] - segStart[2];

        float t = apX * segDir[0] + apY * segDir[1] + apZ * segDir[2];

        if (t < 0.0)
        {
            t = 0.0;
        }
        if (t > m_BeamLength)
        {
            t = m_BeamLength;
        }

        float closestX = segStart[0] + segDir[0] * t;
        float closestY = segStart[1] + segDir[1] * t;
        float closestZ = segStart[2] + segDir[2] * t;

        float dxC = p[0] - closestX;
        float dyC = p[1] - closestY;
        float dzC = p[2] - closestZ;

        return dxC * dxC + dyC * dyC + dzC * dzC;
    }

    // ---- Port world position (p3d uses _0) ----
    override vector LFPG_GetPortWorldPos(string portName)
    {
        string memPoint;
        if (portName == "input_1")
        {
            memPoint = "port_input_0";
        }
        else if (portName == "output_1")
        {
            memPoint = "port_output_0";
        }
        else
        {
            memPoint = "port_";
            memPoint = memPoint + portName;
        }

        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

        vector offset = "0 0.05 0";
        if (portName == "input_1")
        {
            offset = "0 0.05 -0.08";
        }
        else if (portName == "output_1")
        {
            offset = "0 0.05 0.08";
        }
        return ModelToWorld(offset);
    }

    // ---- NM registration + beam renderer ----
    override void LFPG_OnInitDevice()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().RegisterLaserDetector(this);
        #endif

        // Client: register with beam renderer
        #ifndef SERVER
        if (m_DeviceId != "")
        {
            LFPG_LaserBeamRenderer beamR = LFPG_LaserBeamRenderer.Get();
            if (beamR)
            {
                beamR.RegisterDetector(this);
            }
        }
        #endif
    }

    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterLaserDetector(this);
        bool dirty = false;
        if (m_PoweredNet) { m_PoweredNet = false; dirty = true; }
        if (m_GateOpen) { m_GateOpen = false; dirty = true; }
        if (m_BeamLength > 0.0) { m_BeamLength = 0.0; dirty = true; }
        if (dirty) { SetSynchDirty(); }
        #endif
    }

    override void LFPG_OnDeleted()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterLaserDetector(this);
        #endif

        // Client: unregister from beam renderer
        #ifndef SERVER
        LFPG_LaserBeamRenderer beamR = LFPG_LaserBeamRenderer.Get();
        if (beamR)
        {
            beamR.UnregisterDetector(this);
        }
        #endif
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        bool dirty = false;
        if (m_PoweredNet) { m_PoweredNet = false; dirty = true; }
        if (m_GateOpen) { m_GateOpen = false; dirty = true; }
        if (m_BeamLength > 0.0) { m_BeamLength = 0.0; dirty = true; }
        if (dirty) { SetSynchDirty(); }
        #endif
    }

    // ---- Visual sync + beam renderer registration ----
    override void LFPG_OnVarSyncDevice()
    {
        #ifndef SERVER
        LFPG_UpdateVisuals();

        // Ensure beam renderer knows about this detector (covers late-join)
        if (m_DeviceId != "")
        {
            LFPG_LaserBeamRenderer beamR = LFPG_LaserBeamRenderer.Get();
            if (beamR)
            {
                beamR.RegisterDetector(this);
            }
        }
        #endif
    }

    protected void LFPG_UpdateVisuals()
    {
        #ifndef SERVER
        if (m_PoweredNet)
        {
            SetObjectMaterial(0, LFPG_LASER_RVMAT_RED);
        }
        else
        {
            SetObjectMaterial(0, LFPG_LASER_RVMAT_OFF);
        }
        #endif
    }

    // ---- No persist extras ----
};
