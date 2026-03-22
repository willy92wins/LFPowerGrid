// =========================================================
// LF_PowerGrid - Door Controller device (v4.0 Refactor)
//
// LF_DoorController_Kit:  Holdable, deployable (same-model pattern).
// LF_DoorController:      CONSUMER, 1 IN (input_1), 5 u/s, no wire store.
//
// v4.0: Migrated from Inventory_Base to LFPG_DeviceBase.
//   Boilerplate removed. Door logic preserved unchanged.
// =========================================================

static const string LFPG_DC_RVMAT_OFF = "\\LFPowerGrid\\data\\door_controller\\data\\door_controller_red.rvmat";
static const string LFPG_DC_RVMAT_ON  = "\\LFPowerGrid\\data\\door_controller\\data\\door_controller_green.rvmat";

static const int LFPG_DOORTYPE_NONE     = 0;
static const int LFPG_DOORTYPE_FENCE    = 1;
static const int LFPG_DOORTYPE_BUILDING = 2;

static const float LFPG_DC_PAIR_RADIUS     = 1.0;
static const float LFPG_DC_PAIR_RADIUS_SQ  = 1.0;
static const int LFPG_DC_MAX_DOOR_INDEX    = 5;

class LF_DoorController_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LF_DoorController";
    }
};

// ---------------------------------------------------------
// DEVICE - CONSUMER : LFPG_DeviceBase
// ---------------------------------------------------------
class LF_DoorController : LFPG_DeviceBase
{
    // ---- Device-specific SyncVars ----
    protected bool m_PoweredNet = false;

    // ---- Door pairing (server only, NOT persisted) ----
    protected Object m_PairedDoor   = null;
    protected int    m_DoorType     = 0;
    protected int    m_DoorIndex    = -1;

    void LF_DoorController()
    {
        string pIn = "input_1";
        string lIn = "IN";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, lIn);

        string varPowered = "m_PoweredNet";
        RegisterNetSyncVariableBool(varPowered);
    }

    // ---- Custom port world pos (p3d uses "port_input_0") ----
    override vector LFPG_GetPortWorldPos(string portName)
    {
        string memPoint = "port_input_0";
        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

        string warnMsg = "[LF_DoorController] Missing memory point for port: " + portName;
        LFPG_Util.Warn(warnMsg);
        vector p = GetPosition();
        p[1] = p[1] - 0.1;
        return p;
    }

    // ---- Virtual interface ----
    override int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.CONSUMER;
    }

    override float LFPG_GetConsumption()
    {
        return 5.0;
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
        SetSynchDirty();

        string msg = "[LF_DoorController] SetPowered(";
        msg = msg + powered.ToString();
        msg = msg + ") id=";
        msg = msg + m_DeviceId;
        LFPG_Util.Debug(msg);

        LFPG_ApplyDoorState();
        #endif
    }

    // ---- Lifecycle hooks ----
    override void LFPG_OnInit()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().RegisterDoorController(this);
        LFPG_SearchAndPairDoor();
        #endif
    }

    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterDoorController(this);
        LFPG_UnpairDoor();

        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        #endif
    }

    override void LFPG_OnDeleted()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterDoorController(this);
        LFPG_UnpairDoor();
        #endif
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        LFPG_UnpairDoor();

        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        #endif
    }

    // ---- VarSync: LED visual ----
    override void LFPG_OnVarSync()
    {
        #ifndef SERVER
        if (m_PoweredNet)
        {
            SetObjectMaterial(1, LFPG_DC_RVMAT_ON);
        }
        else
        {
            SetObjectMaterial(1, LFPG_DC_RVMAT_OFF);
        }
        #endif
    }

    // ============================================
    // Door pairing management (server only)
    // ============================================
    protected void LFPG_UnpairDoor()
    {
        m_PairedDoor = null;
        m_DoorType = LFPG_DOORTYPE_NONE;
        m_DoorIndex = -1;
    }

    void LFPG_OnDoorPoll()
    {
        #ifdef SERVER
        if (!GetGame().IsServer())
            return;

        if (m_PairedDoor)
        {
            EntityAI pairedEntity = EntityAI.Cast(m_PairedDoor);
            bool isAlive = false;
            if (pairedEntity)
            {
                isAlive = pairedEntity.IsAlive();
            }
            else
            {
                isAlive = (m_PairedDoor != null);
            }

            if (!isAlive)
            {
                LFPG_UnpairDoor();
                return;
            }

            vector myPos = GetPosition();
            vector doorPos = m_PairedDoor.GetPosition();
            float dx = myPos[0] - doorPos[0];
            float dy = myPos[1] - doorPos[1];
            float dz = myPos[2] - doorPos[2];
            float distSq = (dx * dx) + (dy * dy) + (dz * dz);

            if (distSq > LFPG_DC_PAIR_RADIUS_SQ)
            {
                string unpairMsg = "[LF_DoorController] Door moved out of range, unpairing. id=" + m_DeviceId;
                LFPG_Util.Debug(unpairMsg);
                LFPG_UnpairDoor();
                return;
            }
        }

        if (!m_PairedDoor)
        {
            LFPG_SearchAndPairDoor();
            if (!m_PairedDoor)
                return;
        }

        LFPG_ApplyDoorState();
        #endif
    }

    protected void LFPG_ApplyDoorState()
    {
        #ifdef SERVER
        if (!m_PairedDoor)
            return;

        bool doorOpen = LFPG_IsPairedDoorOpen();

        if (m_PoweredNet)
        {
            if (!doorOpen)
            {
                LFPG_ForceOpenDoor();
            }
        }
        else
        {
            if (doorOpen)
            {
                LFPG_ForceCloseDoor();
            }
        }
        #endif
    }

    protected void LFPG_SearchAndPairDoor()
    {
        #ifdef SERVER
        LFPG_UnpairDoor();

        array<Object> objects = new array<Object>;
        GetGame().GetObjectsAtPosition(GetPosition(), LFPG_DC_PAIR_RADIUS, objects, null);

        float bestDistSq = 9999.0;
        Object bestDoor = null;
        int bestType = LFPG_DOORTYPE_NONE;
        int bestIndex = -1;

        vector myPos = GetPosition();

        int count = objects.Count();
        int i = 0;

        Object obj;
        float dx;
        float dy;
        float dz;
        float distSq;
        Fence fence;
        bool isFenceValid;
        Building bld;
        int doorIdx;
        vector objPos;

        #ifdef BBP
        BBP_BASE bbpBase;
        bool isBBPType;
        #endif

        for (i = 0; i < count; i = i + 1)
        {
            obj = objects[i];
            if (!obj)
                continue;

            if (obj == this)
                continue;

            objPos = obj.GetPosition();
            dx = myPos[0] - objPos[0];
            dy = myPos[1] - objPos[1];
            dz = myPos[2] - objPos[2];
            distSq = (dx * dx) + (dy * dy) + (dz * dz);

            fence = Fence.Cast(obj);
            if (fence)
            {
                isFenceValid = false;

                if (fence.HasHinges())
                {
                    isFenceValid = true;
                }

                #ifdef BBP
                if (!isFenceValid)
                {
                    bbpBase = BBP_BASE.Cast(obj);
                    if (bbpBase)
                    {
                        isBBPType = false;
                        if (bbpBase.isBBPDoor())
                        {
                            isBBPType = true;
                        }
                        if (bbpBase.IsBBPGate())
                        {
                            isBBPType = true;
                        }
                        if (isBBPType)
                        {
                            if (bbpBase.BBP_HasDoor())
                            {
                                isFenceValid = true;
                            }
                        }
                    }
                }
                #endif

                if (isFenceValid && distSq < bestDistSq)
                {
                    bestDistSq = distSq;
                    bestDoor = obj;
                    bestType = LFPG_DOORTYPE_FENCE;
                    bestIndex = -1;
                }

                continue;
            }

            bld = Building.Cast(obj);
            if (bld)
            {
                doorIdx = LFPG_FindFirstValidDoorIndex(bld);
                if (doorIdx >= 0 && distSq < bestDistSq)
                {
                    bestDistSq = distSq;
                    bestDoor = obj;
                    bestType = LFPG_DOORTYPE_BUILDING;
                    bestIndex = doorIdx;
                }

                continue;
            }
        }

        if (bestDoor)
        {
            m_PairedDoor = bestDoor;
            m_DoorType = bestType;
            m_DoorIndex = bestIndex;

            float bestDist = Math.Sqrt(bestDistSq);
            string pairMsg = "[LF_DoorController] Paired to door type=" + bestType.ToString();
            pairMsg = pairMsg + " idx=";
            pairMsg = pairMsg + bestIndex.ToString();
            pairMsg = pairMsg + " dist=";
            pairMsg = pairMsg + bestDist.ToString();
            pairMsg = pairMsg + " id=";
            pairMsg = pairMsg + m_DeviceId;
            LFPG_Util.Info(pairMsg);
        }
        #endif
    }

    protected int LFPG_FindFirstValidDoorIndex(Building bld)
    {
        if (!bld)
            return -1;

        int i = 0;
        bool canOpen = false;

        for (i = 0; i <= LFPG_DC_MAX_DOOR_INDEX; i = i + 1)
        {
            if (bld.IsDoorOpen(i))
                return i;

            canOpen = bld.CanDoorBeOpened(i, false);
            if (canOpen)
                return i;
        }

        return -1;
    }

    protected bool LFPG_IsPairedDoorOpen()
    {
        if (!m_PairedDoor)
            return false;

        if (m_DoorType == LFPG_DOORTYPE_FENCE)
        {
            Fence f = Fence.Cast(m_PairedDoor);
            if (f)
            {
                return f.IsOpened();
            }
            return false;
        }

        if (m_DoorType == LFPG_DOORTYPE_BUILDING)
        {
            Building b = Building.Cast(m_PairedDoor);
            if (b)
            {
                return b.IsDoorOpen(m_DoorIndex);
            }
            return false;
        }

        return false;
    }

    protected void LFPG_ForceOpenDoor()
    {
        #ifdef SERVER
        if (!m_PairedDoor)
            return;

        if (m_DoorType == LFPG_DOORTYPE_FENCE)
        {
            Fence f = Fence.Cast(m_PairedDoor);
            if (f)
            {
                if (!f.IsOpened())
                {
                    f.OpenFence();

                    string openFenceMsg = "[LF_DoorController] Opened fence. id=" + m_DeviceId;
                    LFPG_Util.Debug(openFenceMsg);
                }
            }
            return;
        }

        if (m_DoorType == LFPG_DOORTYPE_BUILDING)
        {
            Building b = Building.Cast(m_PairedDoor);
            if (b)
            {
                if (!b.IsDoorOpen(m_DoorIndex))
                {
                    b.OpenDoor(m_DoorIndex);

                    string openDoorMsg = "[LF_DoorController] Opened building door idx=" + m_DoorIndex.ToString();
                    openDoorMsg = openDoorMsg + " id=";
                    openDoorMsg = openDoorMsg + m_DeviceId;
                    LFPG_Util.Debug(openDoorMsg);
                }
            }
            return;
        }
        #endif
    }

    protected void LFPG_ForceCloseDoor()
    {
        #ifdef SERVER
        if (!m_PairedDoor)
            return;

        if (m_DoorType == LFPG_DOORTYPE_FENCE)
        {
            Fence f = Fence.Cast(m_PairedDoor);
            if (f)
            {
                if (f.IsOpened())
                {
                    f.CloseFence();

                    string closeFenceMsg = "[LF_DoorController] Closed fence. id=" + m_DeviceId;
                    LFPG_Util.Debug(closeFenceMsg);
                }
            }
            return;
        }

        if (m_DoorType == LFPG_DOORTYPE_BUILDING)
        {
            Building b = Building.Cast(m_PairedDoor);
            if (b)
            {
                if (b.IsDoorOpen(m_DoorIndex))
                {
                    b.CloseDoor(m_DoorIndex);

                    string closeDoorMsg = "[LF_DoorController] Closed building door idx=" + m_DoorIndex.ToString();
                    closeDoorMsg = closeDoorMsg + " id=";
                    closeDoorMsg = closeDoorMsg + m_DeviceId;
                    LFPG_Util.Debug(closeDoorMsg);
                }
            }
            return;
        }
        #endif
    }
};
