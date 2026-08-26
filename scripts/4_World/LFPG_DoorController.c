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

    // T2: reusable pairing search, cursor budget, and exponential backoff.
    protected static const int LFPG_DC_SEARCH_OBJECT_BUDGET = 128;
    protected static const int LFPG_DC_BACKOFF_MIN_MS = 2000;
    protected static const int LFPG_DC_BACKOFF_MAX_MS = 32000;
    protected ref array<Object> m_SearchObjects;
    protected ref array<Man> m_SearchNearbyPlayers;
    protected int m_SearchCursor = 0;
    protected bool m_SearchInProgress = false;
    protected int m_SearchHintType = -1;
    protected int m_SearchHintIndex = -1;
    protected Object m_SearchBestDoor = null;
    protected float m_SearchBestDistSq = 9999.0;
    protected int m_SearchBestType = LFPG_DOORTYPE_NONE;
    protected int m_SearchBestIndex = -1;
    protected int m_NextSearchMs = 0;
    protected int m_SearchBackoffMs = LFPG_DC_BACKOFF_MIN_MS;
    protected int m_LastSearchAttemptMs = 0;
    protected int m_SearchWakeMs = 0;
    protected int m_SearchAttempts = 0;

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

        if (LFPG_LOG_LEVEL >= 2)
        {
            string msg = "[LFPG_DoorController] SetPowered(";
            msg = msg + powered.ToString();
            msg = msg + ") id=";
            msg = msg + m_DeviceId;
            LFPG_Util.Debug(msg);
        }

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
        if (!m_SearchObjects)
            m_SearchObjects = new array<Object>;
        if (!m_SearchNearbyPlayers)
            m_SearchNearbyPlayers = new array<Man>;
        m_SearchWakeMs = g_Game.GetTime();

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
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_SearchAndPairDoor);
        m_SearchInProgress = false;
        m_SearchObjects = null;
        m_SearchNearbyPlayers = null;
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
        LFPG_NetworkManager nm = LFPG_NetworkManager.GetExisting();
        if (nm) nm.UnregisterDoorController(this);
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(LFPG_SearchAndPairDoor);
        m_SearchInProgress = false;
        m_SearchObjects = null;
        m_SearchNearbyPlayers = null;
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
            if (m_SearchInProgress)
                return;

            int nowMs = g_Game.GetTime();
            if (nowMs < m_NextSearchMs)
            {
                if (m_SearchBackoffMs <= LFPG_DC_BACKOFF_MIN_MS)
                    return;
                if (!m_SearchNearbyPlayers)
                    return;

                // Fence construction requires a physically present player, so a
                // nearby player is a placement proxy; without one no fence can be placed.
                m_SearchNearbyPlayers.Clear();
                g_Game.GetPlayers(m_SearchNearbyPlayers);
                bool hasNearbyPlayer = false;
                int nearbyPlayerIndex;
                Man nearbyPlayer;
                vector controllerSearchPos = GetPosition();
                float playerNearRadiusSq = LFPG_DC_PLAYER_NEAR_RADIUS_M * LFPG_DC_PLAYER_NEAR_RADIUS_M;
                for (nearbyPlayerIndex = 0; nearbyPlayerIndex < m_SearchNearbyPlayers.Count(); nearbyPlayerIndex = nearbyPlayerIndex + 1)
                {
                    nearbyPlayer = m_SearchNearbyPlayers[nearbyPlayerIndex];
                    if (!nearbyPlayer)
                        continue;
                    if (vector.DistanceSq(controllerSearchPos, nearbyPlayer.GetPosition()) <= playerNearRadiusSq)
                    {
                        hasNearbyPlayer = true;
                        break;
                    }
                }
                if (!hasNearbyPlayer)
                    return;
                if (nowMs < m_LastSearchAttemptMs + LFPG_DC_BACKOFF_MIN_MS)
                    return;
            }
            if (!m_SearchObjects)
                return;

            m_SearchObjects.Clear();
            g_Game.GetObjectsAtPosition(GetPosition(), LFPG_DC_SEARCH_RADIUS, m_SearchObjects, null);
            m_SearchCursor = 0;
            m_SearchInProgress = true;
            m_SearchBestDoor = null;
            m_SearchBestDistSq = 9999.0;
            m_SearchBestType = LFPG_DOORTYPE_NONE;
            m_SearchBestIndex = -1;
            LFPG_DoSearchAndPair(m_SearchHintType, m_SearchHintIndex);
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
        if (!m_SearchInProgress)
        {
            LFPG_UnpairDoor();
            if (!m_SearchObjects)
                return;

            m_SearchBackoffMs = LFPG_DC_BACKOFF_MIN_MS;
            m_NextSearchMs = 0;
            m_SearchWakeMs = g_Game.GetTime();
            m_SearchObjects.Clear();
            g_Game.GetObjectsAtPosition(GetPosition(), LFPG_DC_SEARCH_RADIUS, m_SearchObjects, null);
            m_SearchCursor = 0;
            m_SearchInProgress = true;
            m_SearchHintType = -1;
            m_SearchHintIndex = -1;
            m_SearchBestDoor = null;
            m_SearchBestDistSq = 9999.0;
            m_SearchBestType = LFPG_DOORTYPE_NONE;
            m_SearchBestIndex = -1;
        }
        LFPG_DoSearchAndPair(m_SearchHintType, m_SearchHintIndex);
        #endif
    }

    // ============================================
    // Search and pair: with persistence hint
    // ============================================
    protected void LFPG_SearchAndPairDoorWithHint(int hintType, int hintIndex)
    {
        #ifdef SERVER
        LFPG_UnpairDoor();
        if (!m_SearchObjects)
            return;

        m_SearchBackoffMs = LFPG_DC_BACKOFF_MIN_MS;
        m_NextSearchMs = 0;
        m_SearchWakeMs = g_Game.GetTime();
        m_SearchObjects.Clear();
        g_Game.GetObjectsAtPosition(GetPosition(), LFPG_DC_SEARCH_RADIUS, m_SearchObjects, null);
        m_SearchCursor = 0;
        m_SearchInProgress = true;
        m_SearchHintType = hintType;
        m_SearchHintIndex = hintIndex;
        m_SearchBestDoor = null;
        m_SearchBestDistSq = 9999.0;
        m_SearchBestType = LFPG_DOORTYPE_NONE;
        m_SearchBestIndex = -1;
        LFPG_DoSearchAndPair(hintType, hintIndex);
        #endif
    }

    // ============================================
    // Core search logic (shared between normal and hint paths)
    // ============================================
    protected void LFPG_DoSearchAndPair(int hintType, int hintIndex)
    {
        #ifdef SERVER
        int count;
        int endIndex;
        int i;
        Object obj;
        float distSq;
        Fence fence;
        bool isFenceValid;
        Building bld;
        vector myPos;
        vector objPos;
        vector doorSoundPos;
        int doorCount;
        int doorIndex;
        float doorDistSq;
        int nowMs;

        #ifdef BBP
        BBP_BASE bbpBase;
        bool isBBPType;
        #endif

        if (!m_SearchInProgress)
            return;
        if (!m_SearchObjects)
        {
            m_SearchInProgress = false;
            return;
        }

        myPos = GetPosition();
        count = m_SearchObjects.Count();
        endIndex = m_SearchCursor + LFPG_DC_SEARCH_OBJECT_BUDGET;
        if (endIndex > count)
            endIndex = count;

        for (i = m_SearchCursor; i < endIndex; i = i + 1)
        {
            obj = m_SearchObjects[i];
            if (!obj)
                continue;
            if (obj == this)
                continue;

            fence = Fence.Cast(obj);
            if (fence)
            {
                objPos = fence.GetPosition();
                distSq = vector.DistanceSq(myPos, objPos);
                if (distSq > LFPG_DC_PAIR_DIST_SQ_FENCE)
                    continue;

                isFenceValid = false;
                if (fence.HasHinges())
                    isFenceValid = true;

                #ifdef BBP
                if (!isFenceValid)
                {
                    bbpBase = BBP_BASE.Cast(obj);
                    if (bbpBase)
                    {
                        isBBPType = false;
                        if (bbpBase.isBBPDoor())
                            isBBPType = true;
                        if (bbpBase.IsBBPGate())
                            isBBPType = true;
                        if (isBBPType)
                        {
                            if (bbpBase.BBP_HasDoor())
                                isFenceValid = true;
                        }
                    }
                }
                #endif

                if (isFenceValid && distSq < m_SearchBestDistSq)
                {
                    m_SearchBestDistSq = distSq;
                    m_SearchBestDoor = obj;
                    m_SearchBestType = LFPG_DOORTYPE_FENCE;
                    m_SearchBestIndex = -1;
                }
                continue;
            }

            bld = Building.Cast(obj);
            if (bld)
            {
                doorCount = bld.GetDoorCount();
                if (hintType == LFPG_DOORTYPE_BUILDING && hintIndex >= 0 && hintIndex < doorCount)
                {
                    doorSoundPos = bld.GetDoorSoundPos(hintIndex);
                    doorDistSq = vector.DistanceSq(myPos, doorSoundPos);
                    if (doorDistSq <= LFPG_DC_PAIR_DIST_SQ_DOOR && doorDistSq < m_SearchBestDistSq)
                    {
                        m_SearchBestDistSq = doorDistSq;
                        m_SearchBestDoor = obj;
                        m_SearchBestType = LFPG_DOORTYPE_BUILDING;
                        m_SearchBestIndex = hintIndex;
                        continue;
                    }
                }

                for (doorIndex = 0; doorIndex < doorCount; doorIndex = doorIndex + 1)
                {
                    doorSoundPos = bld.GetDoorSoundPos(doorIndex);
                    doorDistSq = vector.DistanceSq(myPos, doorSoundPos);
                    if (doorDistSq <= LFPG_DC_PAIR_DIST_SQ_DOOR && doorDistSq < m_SearchBestDistSq)
                    {
                        m_SearchBestDistSq = doorDistSq;
                        m_SearchBestDoor = obj;
                        m_SearchBestType = LFPG_DOORTYPE_BUILDING;
                        m_SearchBestIndex = doorIndex;
                    }
                }
            }
        }

        m_SearchCursor = endIndex;
        if (m_SearchCursor < count)
        {
            g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_SearchAndPairDoor, 100, false);
            return;
        }

        m_SearchInProgress = false;
        m_SearchAttempts = m_SearchAttempts + 1;
        nowMs = g_Game.GetTime();
        m_LastSearchAttemptMs = nowMs;
        if (m_SearchBestDoor)
        {
            m_PairedDoor = m_SearchBestDoor;
            m_DoorType = m_SearchBestType;
            m_DoorIndex = m_SearchBestIndex;
            m_NextSearchMs = 0;
            m_SearchBackoffMs = LFPG_DC_BACKOFF_MIN_MS;

            float bestDist = Math.Sqrt(m_SearchBestDistSq);
            string pairMsg = "[LFPG_DoorController] Paired type=";
            pairMsg = pairMsg + m_DoorType.ToString();
            pairMsg = pairMsg + " idx=";
            pairMsg = pairMsg + m_DoorIndex.ToString();
            pairMsg = pairMsg + " dist=";
            pairMsg = pairMsg + bestDist.ToString();
            pairMsg = pairMsg + " id=";
            pairMsg = pairMsg + m_DeviceId;
            LFPG_Util.Info(pairMsg);

            #ifndef SERVER
            if (LFPG_PERFDIAG_ENABLED)
            {
                int pairLatencyMs = nowMs - m_SearchWakeMs;
                string pairDiag = "LFPG_PERFDIAG door_pair pair_ms=";
                pairDiag = pairDiag + pairLatencyMs.ToString();
                pairDiag = pairDiag + " attempts=";
                pairDiag = pairDiag + m_SearchAttempts.ToString();
                pairDiag = pairDiag + " objects=";
                pairDiag = pairDiag + count.ToString();
                Print(pairDiag);
            }
            #endif

            LFPG_ApplyDoorState();
        }
        else
        {
            m_NextSearchMs = nowMs + m_SearchBackoffMs;
            #ifndef SERVER
            if (LFPG_PERFDIAG_ENABLED)
            {
                string retryDiag = "LFPG_PERFDIAG door_pair miss attempts=";
                retryDiag = retryDiag + m_SearchAttempts.ToString();
                retryDiag = retryDiag + " backoff_ms=";
                retryDiag = retryDiag + m_SearchBackoffMs.ToString();
                retryDiag = retryDiag + " objects=";
                retryDiag = retryDiag + count.ToString();
                Print(retryDiag);
            }
            #endif
            m_SearchBackoffMs = m_SearchBackoffMs * 2;
            if (m_SearchBackoffMs > LFPG_DC_BACKOFF_MAX_MS)
                m_SearchBackoffMs = LFPG_DC_BACKOFF_MAX_MS;
        }

        m_SearchObjects.Clear();
        m_SearchBestDoor = null;
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
