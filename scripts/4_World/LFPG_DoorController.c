// =========================================================
// LF_PowerGrid - Door Controller device (v1.0.0)
//
// LF_DoorController_Kit:  Holdable, deployable (same-model pattern = Camera).
// LF_DoorController:      CONSUMER, 1 IN (input_1), 5 u/s, no OUT, no wire store.
//
// Memory points (LOD Memory in p3d):
//   port_input_0  — upstream cable anchor
//
// Named selections / hiddenSelections (from model.cfg sections[]):
//   [0] bolt       — bolt geometry
//   [1] light_led  — LED indicator (rvmat swap on/off)
//   [2] screen     — screen face
//   [3] camo       — camo texture slot
//
// rvmats:
//   door_controller.rvmat       — body (Super shader)
//   door_controller_green.rvmat — LED ON/powered (Normal/Basic, emmisive green)
//   door_controller_red.rvmat   — LED OFF/unpowered (Normal/Basic, emmisive red)
//
// Function:
//   Auto-pairs to nearest door within 1m (Fence/BBP/Building).
//   Powered → opens paired door (bypasses locks from script side).
//   Unpowered → closes paired door.
//   Locks (CombinationLock, CodeLock) remain attached — they only
//   block player actions, not server-side OpenFence()/CloseFence().
//   Polling interval: 5s (per-device Timer, server only).
//
// Supported door types:
//   - Vanilla Fence gates (constructed with hinges)
//   - BBP doors/gates (#ifdef BBP, inherits Fence)
//   - Vanilla Building doors (proto native OpenDoor/CloseDoor)
//
// Enforce Script: no ternaries, no ++/--, no foreach, no +=/-=.
// =========================================================

static const string LFPG_DC_RVMAT_OFF = "\\LFPowerGrid\\data\\door_controller\\data\\door_controller_red.rvmat";
static const string LFPG_DC_RVMAT_ON  = "\\LFPowerGrid\\data\\door_controller\\data\\door_controller_green.rvmat";

// Door type constants (Enforce Script has no enums in user scripts)
static const int LFPG_DOORTYPE_NONE     = 0;
static const int LFPG_DOORTYPE_FENCE    = 1;
static const int LFPG_DOORTYPE_BUILDING = 2;

// Pairing radius (meters)
static const float LFPG_DC_PAIR_RADIUS     = 1.0;
// DistanceSq threshold = radius^2
static const float LFPG_DC_PAIR_RADIUS_SQ  = 1.0;

// v4.0: Poll interval moved to LFPG_Defines.c (LFPG_DC_TICK_MS = 2000ms)
// Centralized in NetworkManager — no per-device Timer.

// Max door index to scan for vanilla buildings
static const int LFPG_DC_MAX_DOOR_INDEX    = 5;

// ---------------------------------------------------------
// KIT — patron identico a LF_Camera_Kit
// ---------------------------------------------------------
class LF_DoorController_Kit : Inventory_Base
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
    // interrumpe cleanup del action callback antes de detener sonido.
    override string GetLoopDeploySoundset()
    {
        return "";
    }

    override void SetActions()
    {
        super.SetActions();
        AddAction(ActionTogglePlaceObject);
        AddAction(LFPG_ActionPlaceDoorController);
    }

    // Usar parametro position/orientation, NUNCA GetPosition().
    // GetPosition() devuelve la pos del kit (cerca del player), no el hologram.
    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[DoorController_Kit] OnPlacementComplete: param=" + position.ToString();
        tLog = tLog + " kitPos=" + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        string spawnType = "LF_DoorController";
        EntityAI dc = GetGame().CreateObjectEx(spawnType, finalPos, ECE_CREATEPHYSICS);
        if (dc)
        {
            dc.SetPosition(finalPos);
            dc.SetOrientation(finalOri);
            dc.Update();

            string deployMsg = "[DoorController_Kit] Deployed LF_DoorController at " + finalPos.ToString();
            deployMsg = deployMsg + " ori=" + finalOri.ToString();
            LFPG_Util.Info(deployMsg);

            GetGame().ObjectDelete(this);
        }
        else
        {
            string kitErr = "[DoorController_Kit] Failed to create LF_DoorController! Kit preserved.";
            LFPG_Util.Error(kitErr);
            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                string errMsg = "[LFPG] DoorController placement failed. Kit preserved.";
                pb.MessageStatus(errMsg);
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// DEVICE — CONSUMER, 1 IN (input_1), 5 u/s
// ---------------------------------------------------------
class LF_DoorController : Inventory_Base
{
    // ---- SyncVars ----
    protected int  m_DeviceIdLow  = 0;
    protected int  m_DeviceIdHigh = 0;
    protected bool m_PoweredNet   = false;

    // ---- Estado local ----
    protected string m_DeviceId      = "";
    protected bool   m_LFPG_Deleting = false;

    // ---- Door pairing (server only, NOT persisted) ----
    protected Object m_PairedDoor   = null;
    protected int    m_DoorType     = 0;   // LFPG_DOORTYPE_*
    protected int    m_DoorIndex    = -1;  // Only for BUILDING type

    // ============================================
    // Constructor — registro de SyncVars
    // MUST be constructor, NOT EEInit.
    // ============================================
    void LF_DoorController()
    {
        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
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
    // Lifecycle: EEInit
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

        // CONSUMER: no BroadcastOwnerWires (no OUT port, no wire store)

        #ifdef SERVER
        // v4.0: Centralized poll via NetworkManager (replaces per-device Timer)
        LFPG_NetworkManager.Get().RegisterDoorController(this);

        // Attempt immediate pairing on spawn/load
        LFPG_SearchAndPairDoor();
        #endif
    }

    // ============================================
    // Lifecycle: EEKilled
    // ============================================
    override void EEKilled(Object killer)
    {
        LFPG_DeviceLifecycle.OnDeviceKilled(this, m_DeviceId);

        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterDoorController(this);
        LFPG_UnpairDoor();

        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        #endif

        super.EEKilled(killer);
    }

    // ============================================
    // Lifecycle: EEDelete (with anti-ghost guard)
    // ============================================
    override void EEDelete(EntityAI parent)
    {
        m_LFPG_Deleting = true;

        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterDoorController(this);
        LFPG_UnpairDoor();
        #endif

        LFPG_DeviceLifecycle.OnDeviceDeleted(this, m_DeviceId);
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
            LFPG_UnpairDoor();

            if (m_PoweredNet)
            {
                m_PoweredNet = false;
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
        // LED rvmat swap based on power state.
        // light_led = hiddenSelections[1] in config.cpp.
        if (m_PoweredNet)
        {
            SetObjectMaterial(1, LFPG_DC_RVMAT_ON);
        }
        else
        {
            SetObjectMaterial(1, LFPG_DC_RVMAT_OFF);
        }

        // Request wire data from server so cables
        // towards this controller render on JIP.
        if (m_DeviceId != "")
        {
            LFPG_CableRenderer r = LFPG_CableRenderer.Get();
            if (r)
            {
                r.RequestDeviceSync(m_DeviceId, this);
            }
        }
        #endif
    }

    // ============================================
    // Persistence — CONSUMER: ids only.
    // m_PoweredNet: NOT persisted (derived state).
    // m_PairedDoor: NOT persisted (re-discovered on load).
    // ============================================
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);
        ctx.Write(m_DeviceIdLow);
        ctx.Write(m_DeviceIdHigh);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version))
            return false;

        if (!ctx.Read(m_DeviceIdLow))
            return false;
        if (!ctx.Read(m_DeviceIdHigh))
            return false;

        return true;
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

    override void SetActions()
    {
        super.SetActions();
        RemoveAction(ActionTakeItem);
        RemoveAction(ActionTakeItemToHands);
    }

    // Block vanilla CompEM entirely.
    override bool IsElectricAppliance()
    {
        return false;
    }

    // ============================================
    // LFPG_IDevice interface
    // ============================================
    string LFPG_GetDeviceId()
    {
        return m_DeviceId;
    }

    int LFPG_GetDeviceIdLow()
    {
        return m_DeviceIdLow;
    }

    int LFPG_GetDeviceIdHigh()
    {
        return m_DeviceIdHigh;
    }

    // ---- Port definition: 1x IN ----
    int LFPG_GetPortCount()
    {
        return 1;
    }

    string LFPG_GetPortName(int idx)
    {
        if (idx == 0)
            return "input_1";
        return "";
    }

    int LFPG_GetPortDir(int idx)
    {
        if (idx == 0)
            return LFPG_PortDir.IN;
        return -1;
    }

    string LFPG_GetPortLabel(int idx)
    {
        if (idx == 0)
            return "IN";
        return "";
    }

    bool LFPG_HasPort(string name, int dir)
    {
        string inName = "input_1";
        if (name == inName && dir == LFPG_PortDir.IN)
            return true;
        return false;
    }

    vector LFPG_GetPortWorldPos(string portName)
    {
        // p3d skeleton defines "port_input_0" (not "port_input_1")
        string memPoint = "port_input_0";

        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

        // Fallback: slightly below device position
        string warnMsg = "[LF_DoorController] Missing memory point for port: " + portName;
        LFPG_Util.Warn(warnMsg);
        vector p = GetPosition();
        p[1] = p[1] - 0.1;
        return p;
    }

    // ---- Device type ----
    int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.CONSUMER;
    }

    bool LFPG_IsSource()
    {
        return false;
    }

    float LFPG_GetConsumption()
    {
        return 5.0;
    }

    bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;
        SetSynchDirty();

        string msg = "[LF_DoorController] SetPowered(" + powered.ToString() + ") id=" + m_DeviceId;
        LFPG_Util.Debug(msg);

        // v2.1: Immediate door reaction — no waiting for poll timer.
        // Eliminates 0-5s delay from async timer stacking.
        LFPG_ApplyDoorState();
        #endif
    }

    // CONSUMER — no output port, cannot initiate connections.
    bool LFPG_CanConnectTo(Object other, string myPort, string otherPort)
    {
        return false;
    }

    // No wire store (IN-only).
    bool LFPG_HasWireStore()
    {
        return false;
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

    // ============================================
    // POLL CALLBACK (server, every 5s)
    // ============================================
    void LFPG_OnDoorPoll()
    {
        #ifdef SERVER
        if (!GetGame().IsServer())
            return;

        // 1. Validate existing pairing
        if (m_PairedDoor)
        {
            // Check door still alive
            EntityAI pairedEntity = EntityAI.Cast(m_PairedDoor);
            bool isAlive = false;
            if (pairedEntity)
            {
                isAlive = pairedEntity.IsAlive();
            }
            else
            {
                // Object cast — check if still exists (non-EntityAI building)
                isAlive = (m_PairedDoor != null);
            }

            if (!isAlive)
            {
                LFPG_UnpairDoor();
                return;
            }

            // Check distance (DistanceSq avoids sqrt)
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

        // 2. If not paired, search for a door
        if (!m_PairedDoor)
        {
            LFPG_SearchAndPairDoor();
            if (!m_PairedDoor)
                return;
        }

        // 3. Apply door state based on power
        LFPG_ApplyDoorState();
        #endif
    }

    // ============================================
    // APPLY DOOR STATE — opens/closes based on m_PoweredNet.
    // Called from LFPG_SetPowered (immediate) and LFPG_OnDoorPoll (periodic).
    // Safe to call anytime: guards on m_PairedDoor null.
    // ============================================
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

    // ============================================
    // DOOR SEARCH — finds nearest valid door within 1m
    // Priority: Fence/BBP > Building
    // ============================================
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

        // Hoisted variables for loop body (Enforce: no declarations inside loops)
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

        // BBP-specific hoisted variables (only compiled when BBP defined)
        #ifdef BBP
        BBP_BASE bbpBase;
        bool isBBPType;
        #endif

        for (i = 0; i < count; i = i + 1)
        {
            obj = objects[i];
            if (!obj)
                continue;

            // Skip self
            if (obj == this)
                continue;

            objPos = obj.GetPosition();
            dx = myPos[0] - objPos[0];
            dy = myPos[1] - objPos[1];
            dz = myPos[2] - objPos[2];
            distSq = (dx * dx) + (dy * dy) + (dz * dz);

            // --- Check Fence (vanilla gate + BBP) ---
            fence = Fence.Cast(obj);
            if (fence)
            {
                isFenceValid = false;

                // Vanilla: must have hinges (gate constructed)
                if (fence.HasHinges())
                {
                    isFenceValid = true;
                }

                // BBP: door/gate might not have vanilla hinges.
                // BBP_HasDoor() is on BBP_BASE, not modded ItemBase.
                // isBBPDoor() and IsBBPGate() are on modded ItemBase
                // but accessible via BBP_BASE (inherits from Fence).
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

            // --- Check Building (vanilla houses) ---
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

        // Apply best match
        if (bestDoor)
        {
            m_PairedDoor = bestDoor;
            m_DoorType = bestType;
            m_DoorIndex = bestIndex;

            float bestDist = Math.Sqrt(bestDistSq);
            string pairMsg = "[LF_DoorController] Paired to door type=" + bestType.ToString();
            pairMsg = pairMsg + " idx=" + bestIndex.ToString();
            pairMsg = pairMsg + " dist=" + bestDist.ToString();
            pairMsg = pairMsg + " id=" + m_DeviceId;
            LFPG_Util.Info(pairMsg);
        }
        #endif
    }

    // ============================================
    // BUILDING DOOR INDEX — find first valid door (0-5)
    // A door "exists" if it's currently open or can be opened.
    // ============================================
    protected int LFPG_FindFirstValidDoorIndex(Building bld)
    {
        if (!bld)
            return -1;

        int i = 0;
        bool canOpen = false;

        for (i = 0; i <= LFPG_DC_MAX_DOOR_INDEX; i = i + 1)
        {
            // Door is valid if it's open...
            if (bld.IsDoorOpen(i))
                return i;

            // ...or if it can be opened (false = ignore lock check)
            canOpen = bld.CanDoorBeOpened(i, false);
            if (canOpen)
                return i;
        }

        return -1;
    }

    // ============================================
    // QUERY: Is paired door currently open?
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
    // FORCE OPEN — bypasses lock checks
    // OpenFence() / OpenDoor() are server methods that
    // do NOT check IsLocked(). Locks only gate player actions.
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
                    openDoorMsg = openDoorMsg + " id=" + m_DeviceId;
                    LFPG_Util.Debug(openDoorMsg);
                }
            }
            return;
        }
        #endif
    }

    // ============================================
    // FORCE CLOSE — bypasses lock checks
    // CloseFence() / CloseDoor() are server methods that
    // do NOT check IsLocked().
    // ============================================
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
                    closeDoorMsg = closeDoorMsg + " id=" + m_DeviceId;
                    LFPG_Util.Debug(closeDoorMsg);
                }
            }
            return;
        }
        #endif
    }
};
