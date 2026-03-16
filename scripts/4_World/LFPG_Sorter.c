// =========================================================
// LF_PowerGrid - Sorter device (v1.2.0 Sprint S2)
//
// LF_Sorter_Kit: Holdable item (same-model deployment).
//                Player places it via hologram -> spawns LF_Sorter.
//                On placement, server scans for nearest container
//                within 3m radius and links it.
//
// LF_Sorter:     1 input  (input_1)
//                6 outputs (output_1..output_6)
//                PASSTHROUGH device that sorts items from its
//                linked container to downstream Sorter containers.
//                Consumes 5.0 u/s (unlike Splitter which is 0.0).
//                OUT ports restricted to LF_Sorter only (CanConnectTo).
//                Owns wires on output side (same pattern as Splitter).
//
// Memory points (LOD Memory, Sprint S5):
//   port_input_1    - upstream cable
//   port_output_1   - downstream cable 1
//   port_output_2   - downstream cable 2
//   port_output_3   - downstream cable 3
//   port_output_4   - downstream cable 4
//   port_output_5   - downstream cable 5
//   port_output_6   - downstream cable 6
//
// Container association:
//   - OnPlacementComplete scans for nearest entity with GetInventory()
//   - Stored as NetworkID (m_LinkedContainerLow/High, SyncVars)
//   - Resolved at runtime by NetworkID (never caches reference)
//   - Uniqueness: 1 Sorter per container (static map s_ContainerMap)
//   - If container destroyed -> Sorter goes orphan, stops sorting
//
// Filter config:
//   - Stored as compact JSON in m_FilterJSON (persisted)
//   - Data model: LFPG_SortConfig (3_Game/LFPG_SorterData.c)
//   - Loaded on EEInit, modified via RPC (Sprint S4)
//
// Enforce Script rules:
//   No foreach, no ++/--, no ternario, no +=/-=
//   Variables hoisted before conditionals
//   Incremental string concat
//
// Wire manipulation delegated to LFPG_WireHelper (3_Game).
// =========================================================

// Proximity scan radius for container association
static const float LFPG_SORTER_LINK_RADIUS = 3.0;

// LED rvmat paths (hiddenSelections[0] = "sorter_led")
static const string LFPG_SORTER_RVMAT_OFF = "\\LFPowerGrid\\data\\sorter\\materials\\lf_sorter_led_off.rvmat";
static const string LFPG_SORTER_RVMAT_ON  = "\\LFPowerGrid\\data\\sorter\\materials\\lf_sorter_led_on.rvmat";

// ---------------------------------------------------------
// KIT: deployable item that spawns the actual Sorter
// Uses splitter model as placeholder (Sprint S5 = real model).
// ---------------------------------------------------------
class LF_Sorter_Kit : Inventory_Base
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
        AddAction(LFPG_ActionPlaceSorter);
    }

    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[Sorter_Kit] OnPlacementComplete: param=";
        tLog = tLog + position.ToString();
        tLog = tLog + " kitPos=";
        tLog = tLog + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        string kSorterType = "LF_Sorter";
        EntityAI sorter = GetGame().CreateObjectEx(kSorterType, finalPos, ECE_CREATEPHYSICS);
        if (sorter)
        {
            sorter.SetPosition(finalPos);
            sorter.SetOrientation(finalOri);
            sorter.Update();

            // Attempt container association
            LF_Sorter sorterObj = LF_Sorter.Cast(sorter);
            if (sorterObj)
            {
                sorterObj.LFPG_LinkNearestContainer(finalPos);
            }

            string deployMsg = "[Sorter_Kit] Deployed LF_Sorter at ";
            deployMsg = deployMsg + finalPos.ToString();
            LFPG_Util.Info(deployMsg);
            GetGame().ObjectDelete(this);
        }
        else
        {
            string errCreate = "[Sorter_Kit] Failed to create LF_Sorter! Kit preserved.";
            LFPG_Util.Error(errCreate);
            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                string failMsg = "[LFPG] Sorter placement failed. Kit preserved.";
                pb.MessageStatus(failMsg);
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// SORTER: pass-through device (consumer + source)
// 1 IN + 6 OUT, consumes 5.0 u/s, owns wire store
// ---------------------------------------------------------
class LF_Sorter : Inventory_Base
{
    // ---- Device identity ----
    protected int m_DeviceIdLow = 0;
    protected int m_DeviceIdHigh = 0;
    protected string m_DeviceId;

    // ---- Wires owned (output side) ----
    protected ref array<ref LFPG_WireData> m_Wires;

    // ---- Power state ----
    protected bool m_PoweredNet = false;
    protected bool m_Overloaded = false;

    // ---- Deletion guard ----
    protected bool m_LFPG_Deleting = false;

    // ---- Linked container (NetworkID) ----
    protected int m_LinkedContainerLow = 0;
    protected int m_LinkedContainerHigh = 0;

    // ---- Filter config (persisted as JSON) ----
    protected string m_FilterJSON = "";
    protected ref LFPG_SortConfig m_FilterConfig;

    // ---- Container uniqueness (static, server-side) ----
    // Key: "low:high" of container NetworkID -> Sorter entity
    // Prevents multiple Sorters linking the same container.
    protected static ref map<string, EntityAI> s_ContainerMap;

    // =========================================================
    // Constructor
    // =========================================================
    void LF_Sorter()
    {
        m_Wires = new array<ref LFPG_WireData>;
        m_FilterConfig = new LFPG_SortConfig();

        RegisterNetSyncVariableInt("m_DeviceIdLow");
        RegisterNetSyncVariableInt("m_DeviceIdHigh");
        RegisterNetSyncVariableInt("m_LinkedContainerLow");
        RegisterNetSyncVariableInt("m_LinkedContainerHigh");
        RegisterNetSyncVariableBool("m_PoweredNet");
        RegisterNetSyncVariableBool("m_Overloaded");

        if (!s_ContainerMap)
        {
            s_ContainerMap = new map<string, EntityAI>;
        }
    }

    // =========================================================
    // Actions + Guards
    // =========================================================
    override void SetActions()
    {
        super.SetActions();
        RemoveAction(ActionTakeItem);
        RemoveAction(ActionTakeItemToHands);
        AddAction(LFPG_ActionOpenSorterPanel);
        AddAction(LFPG_ActionSyncSorter);
    }

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

    // =========================================================
    // Lifecycle
    // =========================================================
    override void EEInit()
    {
        super.EEInit();

        #ifdef SERVER
        if (m_DeviceIdLow == 0 && m_DeviceIdHigh == 0)
        {
            LFPG_Util.GenerateDeviceId(m_DeviceIdLow, m_DeviceIdHigh);
        }
        // v0.9.3 (Audit Fix #2): Unconditional SetSynchDirty for persistence load.
        SetSynchDirty();
        #endif

        LFPG_UpdateDeviceIdString();
        LFPG_TryRegister();

        #ifdef SERVER
        LFPG_NetworkManager.Get().RegisterSorter(this);
        LFPG_NetworkManager.Get().BroadcastOwnerWires(this);

        // Post-restart re-link: if we had a container link but the
        // NetworkIDs are stale (entity no longer resolves), re-scan
        // by proximity to restore the association.
        EntityAI existCheck;
        int rLow;
        int rHigh;
        string rKey;
        string relinkMsg;
        bool isValidContainer;
        Man manGuard;
        CargoBase cargoGuard;
        if (m_LinkedContainerLow != 0 || m_LinkedContainerHigh != 0)
        {
            existCheck = LFPG_DeviceAPI.ResolveByNetworkId(m_LinkedContainerLow, m_LinkedContainerHigh);

            // Validate: after restart, recycled IDs could resolve to a
            // completely different entity (player, generator, etc.).
            // Apply same filters as LinkNearestContainer.
            isValidContainer = false;
            if (existCheck)
            {
                manGuard = Man.Cast(existCheck);
                if (!manGuard && !LFPG_DeviceAPI.IsElectricDevice(existCheck))
                {
                    if (existCheck.GetInventory())
                    {
                        cargoGuard = existCheck.GetInventory().GetCargo();
                        if (cargoGuard)
                        {
                            isValidContainer = true;
                        }
                    }
                }
            }

            if (!isValidContainer)
            {
                // IDs are stale or resolved to non-container — re-link by proximity
                m_LinkedContainerLow = 0;
                m_LinkedContainerHigh = 0;
                LFPG_LinkNearestContainer(GetPosition());
                relinkMsg = "[LF_Sorter] Post-restart re-link attempted at ";
                relinkMsg = relinkMsg + GetPosition().ToString();
                LFPG_Util.Info(relinkMsg);
            }
            else
            {
                // Container still exists — register in uniqueness map
                // Use CURRENT NetworkID (may differ from persisted after restart)
                rLow = 0;
                rHigh = 0;
                existCheck.GetNetworkID(rLow, rHigh);
                rKey = rLow.ToString();
                rKey = rKey + ":";
                rKey = rKey + rHigh.ToString();
                s_ContainerMap.Set(rKey, this);

                // Update stored IDs to current
                if (rLow != m_LinkedContainerLow || rHigh != m_LinkedContainerHigh)
                {
                    m_LinkedContainerLow = rLow;
                    m_LinkedContainerHigh = rHigh;
                    SetSynchDirty();
                }
            }
        }
        #endif
    }

    override void EEKilled(Object killer)
    {
        LFPG_DeviceLifecycle.OnDeviceKilled(this, m_DeviceId);

        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterSorter(this);
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        UnregisterContainer();
        #endif

        super.EEKilled(killer);
    }

    override void EEDelete(EntityAI parent)
    {
        m_LFPG_Deleting = true;

        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterSorter(this);
        UnregisterContainer();
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
            if (m_PoweredNet)
            {
                m_PoweredNet = false;
                SetSynchDirty();
            }
        }
        #endif
    }

    override void OnVariablesSynchronized()
    {
        super.OnVariablesSynchronized();
        LFPG_TryRegister();

        #ifndef SERVER
        // LED swap: sorter_led = hiddenSelections[0] in config.cpp
        if (m_PoweredNet)
        {
            SetObjectMaterial(0, LFPG_SORTER_RVMAT_ON);
        }
        else
        {
            SetObjectMaterial(0, LFPG_SORTER_RVMAT_OFF);
        }

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

    // =========================================================
    // Device ID management
    // =========================================================
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

    // =========================================================
    // Container association
    // =========================================================

    // Called by Kit on placement — scans for nearest container
    void LFPG_LinkNearestContainer(vector searchPos)
    {
        #ifdef SERVER
        float bestDistSq = LFPG_SORTER_LINK_RADIUS * LFPG_SORTER_LINK_RADIUS;
        EntityAI bestContainer = null;

        // Get all nearby entities
        ref array<Object> nearObjects = new array<Object>;
        GetGame().GetObjectsAtPosition(searchPos, LFPG_SORTER_LINK_RADIUS, nearObjects, null);

        int i;
        for (i = 0; i < nearObjects.Count(); i = i + 1)
        {
            Object obj = nearObjects[i];
            if (!obj)
                continue;

            // Skip self
            if (obj == this)
                continue;

            EntityAI candidate = EntityAI.Cast(obj);
            if (!candidate)
                continue;

            // Skip players/zombies — they have inventory but aren't containers
            Man manCheck = Man.Cast(candidate);
            if (manCheck)
            {
                string manReject = "[LF_Sorter] SCAN reject: ";
                manReject = manReject + candidate.GetType();
                manReject = manReject + " reason=IS_MAN";
                LFPG_Util.Debug(manReject);
                continue;
            }

            // Skip ALL electrical devices (Sorters, Splitters, Generators, etc.)
            if (LFPG_DeviceAPI.IsElectricDevice(candidate))
            {
                string elecReject = "[LF_Sorter] SCAN reject: ";
                elecReject = elecReject + candidate.GetType();
                elecReject = elecReject + " reason=IS_ELECTRIC";
                LFPG_Util.Debug(elecReject);
                continue;
            }

            // Must have inventory system
            if (!candidate.GetInventory())
            {
                string invReject = "[LF_Sorter] SCAN reject: ";
                invReject = invReject + candidate.GetType();
                invReject = invReject + " reason=NO_INVENTORY";
                LFPG_Util.Debug(invReject);
                continue;
            }

            // v2.4 Bug B: Accept containers with cargo OR attachments
            // Some vanilla containers (WoodenCrate) use proxy slots instead of cargo.
            CargoBase candidateCargo = candidate.GetInventory().GetCargo();
            int attachCount = candidate.GetInventory().AttachmentCount();
            if (!candidateCargo && attachCount == 0)
            {
                string cargoReject = "[LF_Sorter] SCAN reject: ";
                cargoReject = cargoReject + candidate.GetType();
                cargoReject = cargoReject + " reason=NO_CARGO_NO_ATTACH";
                LFPG_Util.Debug(cargoReject);
                continue;
            }

            // Check uniqueness: container not already claimed
            int candLow = 0;
            int candHigh = 0;
            candidate.GetNetworkID(candLow, candHigh);
            string candKey = candLow.ToString();
            candKey = candKey + ":";
            candKey = candKey + candHigh.ToString();

            if (s_ContainerMap.Contains(candKey))
            {
                EntityAI claimant = s_ContainerMap.Get(candKey);
                if (claimant && claimant != this)
                    continue;
            }

            // Distance check
            float distSq = LFPG_WorldUtil.DistSq(searchPos, candidate.GetPosition());
            if (distSq < bestDistSq)
            {
                bestDistSq = distSq;
                bestContainer = candidate;
            }
        }

        if (bestContainer)
        {
            int linkLow = 0;
            int linkHigh = 0;
            bestContainer.GetNetworkID(linkLow, linkHigh);
            m_LinkedContainerLow = linkLow;
            m_LinkedContainerHigh = linkHigh;

            // Register in uniqueness map
            string key = linkLow.ToString();
            key = key + ":";
            key = key + linkHigh.ToString();
            s_ContainerMap.Set(key, this);

            SetSynchDirty();

            string linkLog = "[LF_Sorter] Linked container: ";
            linkLog = linkLog + bestContainer.GetType();
            linkLog = linkLog + " netId=";
            linkLog = linkLog + linkLow.ToString();
            linkLog = linkLog + ":";
            linkLog = linkLog + linkHigh.ToString();
            LFPG_Util.Info(linkLog);
        }
        else
        {
            string noFoundMsg = "[LF_Sorter] No container found within ";
            noFoundMsg = noFoundMsg + LFPG_SORTER_LINK_RADIUS.ToString();
            noFoundMsg = noFoundMsg + "m";
            LFPG_Util.Warn(noFoundMsg);
        }
        #endif
    }

    // Resolve linked container by NetworkID at runtime
    EntityAI LFPG_GetLinkedContainer()
    {
        if (m_LinkedContainerLow == 0 && m_LinkedContainerHigh == 0)
            return null;

        EntityAI resolved = LFPG_DeviceAPI.ResolveByNetworkId(m_LinkedContainerLow, m_LinkedContainerHigh);
        if (!resolved)
        {
            // Container destroyed or despawned — auto-cleanup
            #ifdef SERVER
            UnregisterContainer();
            m_LinkedContainerLow = 0;
            m_LinkedContainerHigh = 0;
            SetSynchDirty();
            string warnMsg = "[LF_Sorter] Linked container no longer exists — cleared stale reference";
            LFPG_Util.Warn(warnMsg);
            #endif
            return null;
        }
        return resolved;
    }

    // Unregister from uniqueness map
    protected void UnregisterContainer()
    {
        if (m_LinkedContainerLow == 0 && m_LinkedContainerHigh == 0)
            return;

        string key = m_LinkedContainerLow.ToString();
        key = key + ":";
        key = key + m_LinkedContainerHigh.ToString();
        if (s_ContainerMap && s_ContainerMap.Contains(key))
        {
            EntityAI owner = s_ContainerMap.Get(key);
            if (owner == this)
            {
                s_ContainerMap.Remove(key);
            }
        }
    }

    // Expose container NetworkID for tick system (Sprint S3)
    int LFPG_GetLinkedContainerLow()
    {
        return m_LinkedContainerLow;
    }

    int LFPG_GetLinkedContainerHigh()
    {
        return m_LinkedContainerHigh;
    }

    // v2.4 Bug B: Quick linked check (no entity resolve — safe for ActionCondition)
    bool LFPG_IsLinked()
    {
        if (m_LinkedContainerLow == 0 && m_LinkedContainerHigh == 0)
            return false;
        return true;
    }

    // v2.4 Bug B: Public unlink (called from TickSorters auto-unlink + Resync)
    void LFPG_UnlinkContainer()
    {
        #ifdef SERVER
        UnregisterContainer();
        m_LinkedContainerLow = 0;
        m_LinkedContainerHigh = 0;
        SetSynchDirty();
        #endif
    }

    // =========================================================
    // Filter config access
    // =========================================================
    LFPG_SortConfig LFPG_GetFilterConfig()
    {
        return m_FilterConfig;
    }

    string LFPG_GetFilterJSON()
    {
        return m_FilterJSON;
    }

    bool LFPG_SetFilterJSON(string json)
    {
        #ifdef SERVER
        // M3: Validate JSON before persisting — reject malformed input
        ref LFPG_SortConfig testConfig = new LFPG_SortConfig();
        bool parseOk = testConfig.FromJSON(json);
        if (!parseOk)
        {
            string rejectMsg = "[LF_Sorter] SetFilterJSON rejected: malformed JSON";
            LFPG_Util.Warn(rejectMsg);
            return false;
        }

        m_FilterJSON = json;
        m_FilterConfig = testConfig;
        // Note: m_FilterJSON is NOT a SyncVar — too large (up to 2048 bytes).
        // Client receives config via RPC (Sprint S4 CONFIG_REQUEST).
        // Persistence handled by OnStoreSave writing m_FilterJSON directly.
        return true;
        #else
        return false;
        #endif
    }

    // =========================================================
    // LFPG_IDevice interface — 7 ports
    // =========================================================
    string LFPG_GetDeviceId()
    {
        return m_DeviceId;
    }

    int LFPG_GetPortCount()
    {
        return 7;
    }

    string LFPG_GetPortName(int idx)
    {
        if (idx == 0) return "input_1";
        if (idx == 1) return "output_1";
        if (idx == 2) return "output_2";
        if (idx == 3) return "output_3";
        if (idx == 4) return "output_4";
        if (idx == 5) return "output_5";
        if (idx == 6) return "output_6";
        return "";
    }

    int LFPG_GetPortDir(int idx)
    {
        if (idx == 0) return LFPG_PortDir.IN;
        if (idx >= 1 && idx <= 6) return LFPG_PortDir.OUT;
        return -1;
    }

    string LFPG_GetPortLabel(int idx)
    {
        if (idx == 0) return "Input";
        if (idx == 1) return "Output 1";
        if (idx == 2) return "Output 2";
        if (idx == 3) return "Output 3";
        if (idx == 4) return "Output 4";
        if (idx == 5) return "Output 5";
        if (idx == 6) return "Output 6";
        return "";
    }

    bool LFPG_HasPort(string portName, int dir)
    {
        if (dir == LFPG_PortDir.IN && portName == "input_1")
            return true;

        if (dir == LFPG_PortDir.OUT)
        {
            if (portName == "output_1") return true;
            if (portName == "output_2") return true;
            if (portName == "output_3") return true;
            if (portName == "output_4") return true;
            if (portName == "output_5") return true;
            if (portName == "output_6") return true;
        }
        return false;
    }

    vector LFPG_GetPortWorldPos(string portName)
    {
        string memPoint = "port_";
        memPoint = memPoint + portName;
        if (MemoryPointExists(memPoint))
        {
            return ModelToWorld(GetMemoryPointPos(memPoint));
        }

        // Fallback: compact naming (port_output1 vs port_output_1)
        int len = portName.Length();
        if (len >= 3)
        {
            string lastChar = portName.Substring(len - 1, 1);
            string beforeLast = portName.Substring(len - 2, 1);
            if (beforeLast == "_")
            {
                string compact = "port_";
                compact = compact + portName.Substring(0, len - 2);
                compact = compact + lastChar;
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

        string missMsg = "[LF_Sorter] Missing memory point for port: ";
        missMsg = missMsg + portName;
        LFPG_Util.Warn(missMsg);
        vector p = GetPosition();
        p[1] = p[1] + 0.5;
        return p;
    }

    // =========================================================
    // Source / PASSTHROUGH behavior
    // =========================================================
    bool LFPG_IsSource()
    {
        return true;
    }

    bool LFPG_GetSourceOn()
    {
        return m_PoweredNet;
    }

    bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.PASSTHROUGH;
    }

    // Sorter consumes 5.0 u/s (unlike Splitter which is 0.0)
    float LFPG_GetConsumption()
    {
        return 5.0;
    }

    float LFPG_GetCapacity()
    {
        return LFPG_DEFAULT_PASSTHROUGH_CAPACITY;
    }

    void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;
        SetSynchDirty();

        string dbg = "[LF_Sorter] SetPowered(";
        dbg = dbg + powered.ToString();
        dbg = dbg + ") id=";
        dbg = dbg + m_DeviceId;
        LFPG_Util.Debug(dbg);
        #endif
    }

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

    // =========================================================
    // Connection validation — OUT ports only accept LF_Sorter
    // =========================================================
    bool LFPG_CanConnectTo(Object other, string myPort, string otherPort)
    {
        if (!other)
            return false;

        if (!LFPG_HasPort(myPort, LFPG_PortDir.OUT))
            return false;

        // Restriction: only LF_Sorter as downstream target
        string kSorter = "LF_Sorter";
        if (!other.IsKindOf(kSorter))
            return false;

        EntityAI otherEntity = EntityAI.Cast(other);
        if (!otherEntity)
            return false;

        return LFPG_DeviceAPI.HasPort(other, otherPort, LFPG_PortDir.IN);
    }

    // =========================================================
    // Wire ownership API (delegates to WireHelper)
    // Pattern identical to Splitter.
    // =========================================================
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
        if (!wd)
            return false;

        if (wd.m_SourcePort == "")
            wd.m_SourcePort = "output_1";

        if (!LFPG_HasPort(wd.m_SourcePort, LFPG_PortDir.OUT))
        {
            string wireReject = "[LF_Sorter] AddWire rejected: not an output port: ";
            wireReject = wireReject + wd.m_SourcePort;
            LFPG_Util.Warn(wireReject);
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

    // =========================================================
    // Persistence
    // Order: DeviceIdLow, DeviceIdHigh, LinkedContainerLow,
    //        LinkedContainerHigh, WiresJSON, FilterJSON
    // NO m_PoweredNet (derived state).
    // =========================================================
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);

        ctx.Write(m_DeviceIdLow);
        ctx.Write(m_DeviceIdHigh);
        ctx.Write(m_LinkedContainerLow);
        ctx.Write(m_LinkedContainerHigh);

        string wiresJson;
        LFPG_WireHelper.SerializeJSON(m_Wires, wiresJson);
        ctx.Write(wiresJson);

        ctx.Write(m_FilterJSON);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version))
            return false;

        string loadErr = "";

        if (!ctx.Read(m_DeviceIdLow))
        {
            loadErr = "[LF_Sorter] OnStoreLoad: failed to read m_DeviceIdLow";
            LFPG_Util.Error(loadErr);
            return false;
        }

        if (!ctx.Read(m_DeviceIdHigh))
        {
            loadErr = "[LF_Sorter] OnStoreLoad: failed to read m_DeviceIdHigh";
            LFPG_Util.Error(loadErr);
            return false;
        }

        LFPG_UpdateDeviceIdString();

        if (!ctx.Read(m_LinkedContainerLow))
        {
            loadErr = "[LF_Sorter] OnStoreLoad: failed to read m_LinkedContainerLow";
            LFPG_Util.Error(loadErr);
            return false;
        }

        if (!ctx.Read(m_LinkedContainerHigh))
        {
            loadErr = "[LF_Sorter] OnStoreLoad: failed to read m_LinkedContainerHigh";
            LFPG_Util.Error(loadErr);
            return false;
        }

        // NOTE: Do NOT register in s_ContainerMap here.
        // After server restart, NetworkIDs are regenerated — the persisted
        // IDs are stale and would contaminate the uniqueness map.
        // LFPG_GetLinkedContainer auto-cleans stale IDs on first access.

        string wiresJson;
        if (!ctx.Read(wiresJson))
        {
            loadErr = "[LF_Sorter] OnStoreLoad: failed to read wires json";
            LFPG_Util.Error(loadErr);
            return false;
        }
        string wireOwner = "LF_Sorter";
        LFPG_WireHelper.DeserializeJSON(m_Wires, wiresJson, wireOwner);

        if (!ctx.Read(m_FilterJSON))
        {
            loadErr = "[LF_Sorter] OnStoreLoad: failed to read filter json";
            LFPG_Util.Error(loadErr);
            return false;
        }

        if (m_FilterJSON != "")
        {
            m_FilterConfig.FromJSON(m_FilterJSON);
        }

        return true;
    }
};
