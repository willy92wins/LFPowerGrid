// =========================================================
// LF_PowerGrid - Laser Detector (v1.9.0)
//
// LFPG_LaserDetector_Kit: Holdable, deployable (same-model pattern).
// LFPG_LaserDetector:     PASSTHROUGH, 1 IN (input_1) + 1 OUT (output_1).
//                          Self-consumption: 5 u/s. Gated passthrough.
//                          Throughput cap: 20 u/s.
//
// Behavior:
//   Wall-mounted device emitting a red laser beam from the LED point.
//   Beam extends up to 5m in forward direction, stops on wall/object.
//
//   Two centralized ticks in NetworkManager:
//     Slow tick (7s) — server raycast from LED to find beam end point.
//       Updates m_BeamLength SyncVar for client rendering.
//     Fast tick (300ms) — server checks if any player crosses the beam.
//       Point-to-segment distance < 0.35m = detected.
//
//   If any alive player crosses the beam, gate opens (m_GateOpen=true)
//   and power passes through. Otherwise gate closes.
//
// LED states (hiddenSelections[0] = "light_led"):
//   Red emissive = powered (active, scanning)
//   Off          = not powered / disconnected
//
// Port positions: Memory points port_input_0, port_output_0 in p3d.
//
// Persistence: DeviceIdLow, DeviceIdHigh, wiresJSON.
//   m_GateOpen NOT persisted (derived by scan tick).
//   m_PoweredNet NOT persisted (derived by graph propagation).
//   m_BeamLength NOT persisted (derived by raycast).
//
// SAVE WIPE REQUIRED — new persistence schema.
// =========================================================

// LED rvmat paths
static const string LFPG_LASER_RVMAT_OFF = "\\LFPowerGrid\\data\\button\\materials\\led_off.rvmat";
static const string LFPG_LASER_RVMAT_RED = "\\LFPowerGrid\\data\\laser_detector\\laser_detector_red.rvmat";

// ---------------------------------------------------------
// KIT - same-model deploy pattern (PressurePad parity)
// ---------------------------------------------------------
class LFPG_LaserDetector_Kit : Inventory_Base
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
        AddAction(LFPG_ActionPlaceLaserDetector);
    }

    // Usar parametro position/orientation, NUNCA GetPosition().
    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[LaserDetector_Kit] OnPlacementComplete: param=";
        tLog = tLog + position.ToString();
        tLog = tLog + " kitPos=" + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        string spawnType = "LFPG_LaserDetector";
        EntityAI device = GetGame().CreateObjectEx(spawnType, finalPos, ECE_CREATEPHYSICS);
        if (device)
        {
            device.SetPosition(finalPos);
            device.SetOrientation(finalOri);
            device.Update();

            string deployMsg = "[LaserDetector_Kit] Deployed LFPG_LaserDetector at ";
            deployMsg = deployMsg + finalPos.ToString();
            deployMsg = deployMsg + " ori=" + finalOri.ToString();
            LFPG_Util.Info(deployMsg);

            // Solo borrar kit si spawn exitoso.
            GetGame().ObjectDelete(this);
        }
        else
        {
            string errMsg = "[LaserDetector_Kit] Failed to create LFPG_LaserDetector! Kit preserved.";
            LFPG_Util.Error(errMsg);
            PlayerBase pbFail = PlayerBase.Cast(player);
            if (pbFail)
            {
                string failMsg = "[LFPG] Laser Detector placement failed. Kit preserved.";
                pbFail.MessageStatus(failMsg);
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// DEVICE: PASSTHROUGH (1 IN + 1 OUT), gated by laser beam crossing
// ---------------------------------------------------------
class LFPG_LaserDetector : Inventory_Base
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

    // ---- Overload state ----
    protected bool m_Overloaded = false;

    // ---- Beam length (set by slow raycast tick, synced to clients) ----
    protected float m_BeamLength = 0.0;

    // ---- Deletion guard ----
    protected bool m_LFPG_Deleting = false;

    // ---- Reusable raycast result set (avoids alloc per LOS check) ----
    protected ref set<Object> m_RayResults;

    // ============================================
    // Constructor - SyncVars en constructor, NO EEInit
    // ============================================
    void LFPG_LaserDetector()
    {
        m_Wires = new array<ref LFPG_WireData>;
        m_RayResults = new set<Object>;
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
        RegisterNetSyncVariableBool("m_GateOpen");
        RegisterNetSyncVariableBool("m_Overloaded");
        RegisterNetSyncVariableFloat("m_BeamLength", 0.0, 5.5, 3);
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

            // Client: register with beam renderer for visual drawing
            #ifndef SERVER
            LFPG_LaserBeamRenderer beamR = LFPG_LaserBeamRenderer.Get();
            if (beamR)
            {
                beamR.RegisterDetector(this);
            }
            #endif
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
        // Register with centralized detection ticks
        LFPG_NetworkManager.Get().RegisterLaserDetector(this);
        #endif
    }

    override void EEKilled(Object killer)
    {
        LFPG_DeviceLifecycle.OnDeviceKilled(this, m_DeviceId);

        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterLaserDetector(this);

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
        if (m_BeamLength > 0.0)
        {
            m_BeamLength = 0.0;
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
        // Visual update (LED material)
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
        // LED: red emissive when powered, off otherwise
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

    // ============================================
    // Beam raycast — called by LFPG_TickLaserBeams (slow, 7s)
    // ============================================
    // Server only. Updates m_BeamLength based on forward raycast.
    // Returns true if beam length changed (needs SyncDirty).
    bool LFPG_UpdateBeamRaycast()
    {
        #ifdef SERVER
        // Only compute beam if powered
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

        // Beam origin: light_led memory point
        vector beamStart = LFPG_GetBeamStart();

        // Beam direction: forward of the device
        vector beamDir = GetDirection();

        // Beam end: max range
        float maxRange = LFPG_LASER_BEAM_RANGE_M;
        float endX = beamStart[0] + beamDir[0] * maxRange;
        float endY = beamStart[1] + beamDir[1] * maxRange;
        float endZ = beamStart[2] + beamDir[2] * maxRange;
        vector beamEnd = Vector(endX, endY, endZ);

        // Raycast to find first obstacle
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
            float dx = hitPos[0] - beamStart[0];
            float dy = hitPos[1] - beamStart[1];
            float dz = hitPos[2] - beamStart[2];
            float hitDist = Math.Sqrt(dx * dx + dy * dy + dz * dz);
            if (hitDist < maxRange)
            {
                newLength = hitDist;
            }
        }

        // Only sync if changed by more than 5cm (avoid chatter)
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
    // Player crossing detection — called by LFPG_TickLaserCrossing (fast, 300ms)
    // ============================================
    // Returns true if gate state changed (needs propagation).
    bool LFPG_EvaluateCrossing(array<Man> players)
    {
        #ifdef SERVER
        // Only detect if powered and beam is active
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

        // Beam segment: start → end
        vector beamStart = LFPG_GetBeamStart();
        vector beamDir = GetDirection();
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

            // Check TWO points per player for reliable detection at any beam height:
            //   Low point: hips at +0.5m from feet
            //   High point: chest at +1.4m from feet
            // If either point is within crossing radius, player is detected.
            playerLow = playerPos;
            playerLow[1] = playerLow[1] + 0.5;

            playerHigh = playerPos;
            playerHigh[1] = playerHigh[1] + 1.4;

            // Point-to-segment distance squared for low point
            distSqLow = LFPG_PointToSegDistSq(playerLow, beamStart, bEndX, bEndY, bEndZ, beamDir);
            if (distSqLow < crossRadiusSq)
            {
                detected = true;
            }

            // Only check high point if low point didn't trigger
            if (!detected)
            {
                distSqHigh = LFPG_PointToSegDistSq(playerHigh, beamStart, bEndX, bEndY, bEndZ, beamDir);
                if (distSqHigh < crossRadiusSq)
                {
                    detected = true;
                }
            }
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

    // Point-to-segment distance squared.
    // Segment: beamStart to (bEndX, bEndY, bEndZ).
    // beamDir = normalized direction of segment.
    // Returns distance squared from point p to the closest point on segment.
    protected float LFPG_PointToSegDistSq(vector p, vector segStart, float segEndX, float segEndY, float segEndZ, vector segDir)
    {
        // Vector from segStart to point
        float apX = p[0] - segStart[0];
        float apY = p[1] - segStart[1];
        float apZ = p[2] - segStart[2];

        // Project onto segment direction (dot product)
        float t = apX * segDir[0] + apY * segDir[1] + apZ * segDir[2];

        // Clamp t to [0, beamLength]
        if (t < 0.0)
        {
            t = 0.0;
        }
        if (t > m_BeamLength)
        {
            t = m_BeamLength;
        }

        // Closest point on segment
        float closestX = segStart[0] + segDir[0] * t;
        float closestY = segStart[1] + segDir[1] * t;
        float closestZ = segStart[2] + segDir[2] * t;

        // Distance squared
        float dxC = p[0] - closestX;
        float dyC = p[1] - closestY;
        float dzC = p[2] - closestZ;

        return dxC * dxC + dyC * dyC + dzC * dzC;
    }

    // ============================================
    // Beam geometry helpers (shared by server + client)
    // ============================================

    // Returns beam origin in world space (light_led memory point)
    // NOTE: Current P3D has NO light_led memory point in Memory LOD.
    // Fallback uses device center + forward offset to approximate LED position.
    // If light_led is added later, MemoryPointExists path activates automatically.
    vector LFPG_GetBeamStart()
    {
        string memPoint = "light_led";
        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }
        // Fallback: model is ~7cm deep (X axis in model space).
        // LED is on the front face. Device forward = GetDirection().
        // Offset from center: ~3cm forward + 3cm up (model center is near base).
        // Model space: front face is at negative X (-0.029), height center ~0.028.
        // Use a model-space offset that places origin on the front LED face.
        vector ledOffset = "-0.025 0.028 0.0";
        return ModelToWorld(ledOffset);
    }

    // Returns beam end in world space
    vector LFPG_GetBeamEnd()
    {
        vector beamStart = LFPG_GetBeamStart();
        vector beamDir = GetDirection();
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

    // Public getter for beam length (used by renderer)
    float LFPG_GetBeamLength()
    {
        return m_BeamLength;
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

    // Port positions from p3d memory points
    vector LFPG_GetPortWorldPos(string portName)
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
            memPoint = "port_" + portName;
        }

        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

        // Fallback: virtual offsets if memory points missing
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

    // Gated passthrough: gate controlled by beam crossing detection
    bool LFPG_IsGateOpen()
    {
        return m_GateOpen;
    }

    bool LFPG_IsGateCapable()
    {
        return true;
    }

    // 5 u/s self-consumption
    float LFPG_GetConsumption()
    {
        return LFPG_LASER_CONSUMPTION;
    }

    // 20 u/s max throughput
    float LFPG_GetCapacity()
    {
        return LFPG_LASER_CAPACITY;
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

        // When power is lost, close gate and zero beam
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
            // Power just came on: immediate beam raycast so beam appears
            // without waiting for the slow 7s tick.
            LFPG_UpdateBeamRaycast();
        }

        SetSynchDirty();

        string pwrMsg = "[LFPG_LaserDetector] SetPowered(";
        pwrMsg = pwrMsg + powered.ToString();
        pwrMsg = pwrMsg + ") id=" + m_DeviceId;
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
            string warnMsg = "[LFPG_LaserDetector] AddWire rejected: not an output port: ";
            warnMsg = warnMsg + wd.m_SourcePort;
            LFPG_Util.Warn(warnMsg);
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
            string did;
            for (vi = 0; vi < all.Count(); vi = vi + 1)
            {
                did = LFPG_DeviceAPI.GetOrCreateDeviceId(all[vi]);
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
        // m_PoweredNet: NOT persisted (derived by graph propagation)
        // m_GateOpen:   NOT persisted (derived by scan tick)
        // m_BeamLength: NOT persisted (derived by raycast)

        string json = "";
        LFPG_WireHelper.SerializeJSON(m_Wires, json);
        ctx.Write(json);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version))
            return false;

        string loadErr;

        if (!ctx.Read(m_DeviceIdLow))
        {
            loadErr = "[LFPG_LaserDetector] OnStoreLoad: failed to read m_DeviceIdLow";
            LFPG_Util.Error(loadErr);
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            loadErr = "[LFPG_LaserDetector] OnStoreLoad: failed to read m_DeviceIdHigh";
            LFPG_Util.Error(loadErr);
            return false;
        }

        LFPG_UpdateDeviceIdString();

        string json = "";
        if (!ctx.Read(json))
        {
            loadErr = "[LFPG_LaserDetector] OnStoreLoad: failed to read wires json for " + m_DeviceId;
            LFPG_Util.Error(loadErr);
            return false;
        }
        string deserTag = "LFPG_LaserDetector";
        LFPG_WireHelper.DeserializeJSON(m_Wires, json, deserTag);

        return true;
    }
};
