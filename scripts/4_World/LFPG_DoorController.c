// =========================================================
// LF_PowerGrid - Door Controller device (v4.8)
//
// LFPG_DoorController_Kit:  Holdable, deployable (same-model pattern).
// LFPG_DoorController:      CONSUMER, 1 IN (input_1), 5 u/s, no wire store.
//
// v4.0: Migrated from Inventory_Base to LFPG_DeviceBase.
// v4.8: Building door pairing fix (GetDoorSoundPos-based search),
//        LockDoor/UnlockDoor support via DoorControllerLockBuildingDoors,
//        persist DoorType + DoorIndex (ver 2, no save wipe).
// =========================================================

static const string LFPG_DC_RVMAT_OFF = "\LFPowerGrid\data\door_controller\data\door_controller_red.rvmat";
static const string LFPG_DC_RVMAT_ON  = "\LFPowerGrid\data\door_controller\data\door_controller_green.rvmat";

static const int LFPG_DOORTYPE_NONE     = 0;
static const int LFPG_DOORTYPE_FENCE    = 1;
static const int LFPG_DOORTYPE_BUILDING = 2;

// Search radius for GetObjectsAtPosition (large to catch building origins)
static const float LFPG_DC_SEARCH_RADIUS = 50.0;

// Max distance squared from controller to Fence position (2m)
static const float LFPG_DC_PAIR_DIST_SQ_FENCE = 4.0;

// Max distance squared from controller to building door via GetDoorSoundPos (2.5m)
static const float LFPG_DC_PAIR_DIST_SQ_DOOR = 6.25;

class LFPG_DoorController_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_DoorController";
    }

    override int LFPG_GetPlacementModes()
    {
        return 1;
    }

    override float LFPG_GetWallPitchOffset()
    {
        return 90.0;
    }

    override float LFPG_GetWallYawOffset()
    {
        return 180.0;
    }

    override float LFPG_GetFloorYawOffset()
    {
        return 180.0;
    }
};

// ---------------------------------------------------------
// DEVICE - CONSUMER : LFPG_DeviceBase
// ---------------------------------------------------------
class LFPG_DoorController : LFPG_DeviceBase
{
    // ---- Device-specific SyncVars ----
    protected bool m_PoweredNet = false;

    // ---- Door pairing (server only, persisted via Extra) ----
    protected Object m_PairedDoor   = null;
    protected int    m_DoorType     = 0;
    protected int    m_DoorIndex    = -1;

    // ---- Persistence hints (loaded before EEInit, used by OnInit) ----
    protected int m_SavedDoorType  = 0;
    protected int m_SavedDoorIndex = -1;

    void LFPG_DoorController()
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

        string warnMsg = "[LFPG_DoorController] Missing memory point for port: " + portName;
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

        string msg = "[LFPG_DoorController] SetPowered(";
        msg = msg + powered.ToString();
        msg = msg + ") id=";
        msg = msg + m_DeviceId;
        LFPG_Util.Debug(msg);

        LFPG_ApplyDoorState();
        #endif
    }

    // ============================================
    // Persistence (v4.8: persist DoorType + DoorIndex)
    // ============================================
    override int LFPG_GetDevicePersistVersion()
    {
        return 2;
    }

    override void LFPG_OnStoreSaveExtra(ParamsWriteContext ctx)
    {
        ctx.Write(m_DoorType);
        ctx.Write(m_DoorIndex);
    }

    override bool LFPG_OnStoreLoadExtra(ParamsReadContext ctx, int ver)
    {
        if (ver >= 2)
        {
            if (!ctx.Read(m_SavedDoorType))
            {
                string errType = "[LFPG_DoorController] OnStoreLoad failed: m_DoorType";
                LFPG_Util.Error(errType);
                return false;
            }

            if (!ctx.Read(m_SavedDoorIndex))
            {
                string errIdx = "[LFPG_DoorController] OnStoreLoad failed: m_DoorIndex";
                LFPG_Util.Error(errIdx);
                return false;
            }
        }

        return true;
    }

    // ---- Lifecycle hooks ----
    override void LFPG_OnInit()
    {
        #ifdef SERVER
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm) nm.RegisterDoorController(this);

        if (m_SavedDoorType > 0 && m_SavedDoorIndex >= 0)
        {
            LFPG_SearchAndPairDoorWithHint(m_SavedDoorType, m_SavedDoorIndex);
        }
        else
        {
            LFPG_SearchAndPairDoor();
        }
        #endif
    }

    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm) nm.UnregisterDoorController(this);
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
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm) nm.UnregisterDoorController(this);
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
        #ifdef SERVER
        // Unlock building door before unpairing so it is not left
        // permanently locked after the controller is destroyed/cut.
        if (m_DoorType == LFPG_DOORTYPE_BUILDING && m_PairedDoor)
        {
            Building bUnpair = Building.Cast(m_PairedDoor);
            if (bUnpair)
            {
                if (bUnpair.IsDoorLocked(m_DoorIndex))
                {
                    bUnpair.UnlockDoor(m_DoorIndex);

                    string unlockMsg = "[LFPG_DoorController] Unlocked door on unpair idx=";
                    unlockMsg = unlockMsg + m_DoorIndex.ToString();
                    unlockMsg = unlockMsg + " id=";
                    unlockMsg = unlockMsg + m_DeviceId;
                    LFPG_Util.Debug(unlockMsg);
                }
            }
        }
        #endif

        m_PairedDoor = null;
        m_DoorType = LFPG_DOORTYPE_NONE;
        m_DoorIndex = -1;
    }

    void LFPG_OnDoorPoll()
    {
        #ifdef SERVER
        if (!g_Game.IsServer())
            return;

        if (m_PairedDoor)
        {
            // ---- Check paired object is still alive ----
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

            // ---- Distance check: type-specific ----
            vector myPos = GetPosition();
            float distSq = 0.0;
            float maxDistSq = 0.0;

            if (m_DoorType == LFPG_DOORTYPE_FENCE)
            {
                distSq = vector.DistanceSq(myPos, m_PairedDoor.GetPosition());
                maxDistSq = LFPG_DC_PAIR_DIST_SQ_FENCE;
            }
            else if (m_DoorType == LFPG_DOORTYPE_BUILDING)
            {
                Building bPoll = Building.Cast(m_PairedDoor);
                if (bPoll)
                {
                    vector doorSoundPos = bPoll.GetDoorSoundPos(m_DoorIndex);
                    distSq = vector.DistanceSq(myPos, doorSoundPos);
                }
                else
                {
                    distSq = 9999.0;
                }
                maxDistSq = LFPG_DC_PAIR_DIST_SQ_DOOR;
            }

            if (distSq > maxDistSq)
            {
                string unpairMsg = "[LFPG_DoorController] Door out of range, unpairing. id=";
                unpairMsg = unpairMsg + m_DeviceId;
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
            else
            {
                // Door already closed — ensure it is locked if setting enabled
                LFPG_EnsureDoorLocked();
            }
        }
        #endif
    }

    // ============================================
    // Search and pair: main entry (no hint)
    // ============================================
    protected void LFPG_SearchAndPairDoor()
    {
        #ifdef SERVER
        LFPG_UnpairDoor();
        LFPG_DoSearchAndPair(-1, -1);
        #endif
    }

    // ============================================
    // Search and pair: with persistence hint
    // ============================================
    protected void LFPG_SearchAndPairDoorWithHint(int hintType, int hintIndex)
    {
        #ifdef SERVER
        LFPG_UnpairDoor();
        LFPG_DoSearchAndPair(hintType, hintIndex);
        #endif
    }

    // ============================================
    // Core search logic (shared between normal and hint paths)
    //
    // hintType / hintIndex: if >= 0, prefer a Building door with
    // that index (from persisted state). Falls back to best match.
    // ============================================
    protected void LFPG_DoSearchAndPair(int hintType, int hintIndex)
    {
        #ifdef SERVER
        array<Object> objects = new array<Object>;
        g_Game.GetObjectsAtPosition(GetPosition(), LFPG_DC_SEARCH_RADIUS, objects, null);

        float bestDistSq = 9999.0;
        Object bestDoor = null;
        int bestType = LFPG_DOORTYPE_NONE;
        int bestIndex = -1;

        vector myPos = GetPosition();

        int count = objects.Count();
        int i = 0;

        Object obj;
        float distSq;
        Fence fence;
        bool isFenceValid;
        Building bld;
        vector objPos;
        vector doorSoundPos;
        int doorCount;
        int di;
        float doorDistSq;

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

            // ---- FENCE / BBP ----
            fence = Fence.Cast(obj);
            if (fence)
            {
                objPos = fence.GetPosition();
                distSq = vector.DistanceSq(myPos, objPos);

                if (distSq > LFPG_DC_PAIR_DIST_SQ_FENCE)
                    continue;

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

            // ---- BUILDING ----
            bld = Building.Cast(obj);
            if (bld)
            {
                doorCount = bld.GetDoorCount();

                // If we have a hint for this building, try hinted index first
                if (hintType == LFPG_DOORTYPE_BUILDING && hintIndex >= 0 && hintIndex < doorCount)
                {
                    doorSoundPos = bld.GetDoorSoundPos(hintIndex);
                    doorDistSq = vector.DistanceSq(myPos, doorSoundPos);

                    if (doorDistSq <= LFPG_DC_PAIR_DIST_SQ_DOOR && doorDistSq < bestDistSq)
                    {
                        bestDistSq = doorDistSq;
                        bestDoor = obj;
                        bestType = LFPG_DOORTYPE_BUILDING;
                        bestIndex = hintIndex;
                        // Hinted door found within range — skip other doors
                        continue;
                    }
                }

                // Scan all doors for the closest one within range
                for (di = 0; di < doorCount; di = di + 1)
                {
                    doorSoundPos = bld.GetDoorSoundPos(di);
                    doorDistSq = vector.DistanceSq(myPos, doorSoundPos);

                    if (doorDistSq <= LFPG_DC_PAIR_DIST_SQ_DOOR && doorDistSq < bestDistSq)
                    {
                        bestDistSq = doorDistSq;
                        bestDoor = obj;
                        bestType = LFPG_DOORTYPE_BUILDING;
                        bestIndex = di;
                    }
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
            string pairMsg = "[LFPG_DoorController] Paired type=";
            pairMsg = pairMsg + bestType.ToString();
            pairMsg = pairMsg + " idx=";
            pairMsg = pairMsg + bestIndex.ToString();
            pairMsg = pairMsg + " dist=";
            pairMsg = pairMsg + bestDist.ToString();
            pairMsg = pairMsg + " id=";
            pairMsg = pairMsg + m_DeviceId;
            LFPG_Util.Info(pairMsg);

            LFPG_ApplyDoorState();
        }
        else
        {
            string noMsg = "[LFPG_DoorController] No door found within range. id=";
            noMsg = noMsg + m_DeviceId;
            LFPG_Util.Debug(noMsg);
        }
        #endif
    }

    // ============================================
    // Door state queries
    // ============================================
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

    // ============================================
    // Door manipulation
    // ============================================
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

                    string openFenceMsg = "[LFPG_DoorController] Opened fence. id=";
                    openFenceMsg = openFenceMsg + m_DeviceId;
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
                // Unlock first if locked (setting-dependent)
                bool lockSetting = LFPG_Settings.Get().DoorControllerLockBuildingDoors;
                if (lockSetting)
                {
                    if (b.IsDoorLocked(m_DoorIndex))
                    {
                        b.UnlockDoor(m_DoorIndex, false);
                    }
                }

                if (!b.IsDoorOpen(m_DoorIndex))
                {
                    b.OpenDoor(m_DoorIndex);

                    string openDoorMsg = "[LFPG_DoorController] Opened building door idx=";
                    openDoorMsg = openDoorMsg + m_DoorIndex.ToString();
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

                    string closeFenceMsg = "[LFPG_DoorController] Closed fence. id=";
                    closeFenceMsg = closeFenceMsg + m_DeviceId;
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

                    string closeDoorMsg = "[LFPG_DoorController] Closed building door idx=";
                    closeDoorMsg = closeDoorMsg + m_DoorIndex.ToString();
                    closeDoorMsg = closeDoorMsg + " id=";
                    closeDoorMsg = closeDoorMsg + m_DeviceId;
                    LFPG_Util.Debug(closeDoorMsg);
                }

                // Lock after close (setting-dependent)
                bool lockSetting = LFPG_Settings.Get().DoorControllerLockBuildingDoors;
                if (lockSetting)
                {
                    if (!b.IsDoorLocked(m_DoorIndex))
                    {
                        b.LockDoor(m_DoorIndex, true);

                        string lockMsg = "[LFPG_DoorController] Locked building door idx=";
                        lockMsg = lockMsg + m_DoorIndex.ToString();
                        lockMsg = lockMsg + " id=";
                        lockMsg = lockMsg + m_DeviceId;
                        LFPG_Util.Debug(lockMsg);
                    }
                }
            }
            return;
        }
        #endif
    }

    // Ensures door is locked when power is off and door is already closed.
    // Called from ApplyDoorState when doorOpen==false and m_PoweredNet==false.
    protected void LFPG_EnsureDoorLocked()
    {
        #ifdef SERVER
        if (!m_PairedDoor)
            return;

        if (m_DoorType != LFPG_DOORTYPE_BUILDING)
            return;

        bool lockSetting = LFPG_Settings.Get().DoorControllerLockBuildingDoors;
        if (!lockSetting)
            return;

        Building b = Building.Cast(m_PairedDoor);
        if (!b)
            return;

        if (!b.IsDoorLocked(m_DoorIndex))
        {
            b.LockDoor(m_DoorIndex, true);

            string lockMsg = "[LFPG_DoorController] EnsureLocked idx=";
            lockMsg = lockMsg + m_DoorIndex.ToString();
            lockMsg = lockMsg + " id=";
            lockMsg = lockMsg + m_DeviceId;
            LFPG_Util.Debug(lockMsg);
        }
        #endif
    }
};
