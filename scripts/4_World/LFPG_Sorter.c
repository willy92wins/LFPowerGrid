// =========================================================
// LF_PowerGrid - Sorter device (v4.0 Refactor)
//
// LFPG_Sorter_Kit: Holdable item (same-model deployment).
// LFPG_Sorter:     PASSTHROUGH, 1 IN + 6 OUT, 5 u/s self-consumption.
//                Sorts items from linked container to downstream Sorters.
//                OUT ports restricted to LFPG_Sorter only (CanConnectTo).
//
// v4.0: Migrated from Inventory_Base to LFPG_WireOwnerBase.
//   Wire store, wire API, persistence wireJSON, base CanConnectTo — all in base.
//   Sorter overrides CanConnectTo (Sorter-only downstream).
//   SyncVar order: DeviceBase(DeviceId) → WireOwner(WireGen)
//     → Sorter(LinkedContainerLow/High, PoweredNet, Overloaded).
//
// Memory points: port_input_1, port_output_1..6 (match base pattern, no override).
// =========================================================

static const float LFPG_SORTER_LINK_RADIUS = 3.0;

static const string LFPG_SORTER_RVMAT_OFF = "\LFPowerGrid\data\sorter\materials\lf_sorter_led_off.rvmat";
static const string LFPG_SORTER_RVMAT_ON  = "\LFPowerGrid\data\sorter\materials\lf_sorter_led_on.rvmat";

class LFPG_Sorter_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_Sorter";
    }
};

// ---------------------------------------------------------
// DEVICE — PASSTHROUGH : LFPG_WireOwnerBase
// 1 IN (input_1) + 6 OUT (output_1..6), 5 u/s
// ---------------------------------------------------------
class LFPG_Sorter : LFPG_WireOwnerBase
{
    // ---- Device-specific SyncVars ----
    protected int  m_LinkedContainerLow  = 0;
    protected int  m_LinkedContainerHigh = 0;
    protected bool m_PoweredNet          = false;
    protected bool m_Overloaded          = false;

    // ---- Filter config (persisted as JSON, NOT SyncVar) ----
    protected string m_FilterJSON = "";
    protected ref LFPG_SortConfig m_FilterConfig;

    // ---- Container uniqueness (static, server-side) ----
    protected static ref map<string, EntityAI> s_ContainerMap;

    // ============================================
    // Constructor — ports + SyncVars
    // ============================================
    void LFPG_Sorter()
    {
        m_FilterConfig = new LFPG_SortConfig();

        string pIn = "input_1";
        string lIn = "Input";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, lIn);

        string pO1 = "output_1";
        string lO1 = "Output 1";
        LFPG_AddPort(pO1, LFPG_PortDir.OUT, lO1);
        string pO2 = "output_2";
        string lO2 = "Output 2";
        LFPG_AddPort(pO2, LFPG_PortDir.OUT, lO2);
        string pO3 = "output_3";
        string lO3 = "Output 3";
        LFPG_AddPort(pO3, LFPG_PortDir.OUT, lO3);
        string pO4 = "output_4";
        string lO4 = "Output 4";
        LFPG_AddPort(pO4, LFPG_PortDir.OUT, lO4);
        string pO5 = "output_5";
        string lO5 = "Output 5";
        LFPG_AddPort(pO5, LFPG_PortDir.OUT, lO5);
        string pO6 = "output_6";
        string lO6 = "Output 6";
        LFPG_AddPort(pO6, LFPG_PortDir.OUT, lO6);

        string varLinkLow  = "m_LinkedContainerLow";
        string varLinkHigh = "m_LinkedContainerHigh";
        string varPowered  = "m_PoweredNet";
        string varOverload = "m_Overloaded";

        RegisterNetSyncVariableInt(varLinkLow);
        RegisterNetSyncVariableInt(varLinkHigh);
        RegisterNetSyncVariableBool(varPowered);
        RegisterNetSyncVariableBool(varOverload);

        if (!s_ContainerMap)
        {
            s_ContainerMap = new map<string, EntityAI>;
        }
    }

    // ============================================
    // SetActions
    // ============================================
    override void SetActions()
    {
        super.SetActions();
        AddAction(LFPG_ActionOpenSorterPanel);
        AddAction(LFPG_ActionSyncSorter);
    }

    // ============================================
    // CanConnectTo override — Sorter-only downstream
    // ============================================
    override bool LFPG_CanConnectTo(Object other, string myPort, string otherPort)
    {
        if (!other)
            return false;

        if (!LFPG_HasPort(myPort, LFPG_PortDir.OUT))
            return false;

        string kSorter = "LFPG_Sorter";
        if (!other.IsKindOf(kSorter))
            return false;

        EntityAI otherEntity = EntityAI.Cast(other);
        if (!otherEntity)
            return false;

        return LFPG_DeviceAPI.HasPort(other, otherPort, LFPG_PortDir.IN);
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
        return 5.0;
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

        string msg = "[LFPG_Sorter] SetPowered(";
        msg = msg + powered.ToString();
        msg = msg + ") id=";
        msg = msg + m_DeviceId;
        LFPG_Util.Debug(msg);
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
        LFPG_NetworkManager.Get().RegisterSorter(this);

        // Post-restart re-link: if container link persisted,
        // verify it still resolves. If stale, re-scan by proximity.
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
                m_LinkedContainerLow = 0;
                m_LinkedContainerHigh = 0;
                LFPG_LinkNearestContainer(GetPosition());
                relinkMsg = "[LFPG_Sorter] Post-restart re-link attempted at ";
                relinkMsg = relinkMsg + GetPosition().ToString();
                LFPG_Util.Info(relinkMsg);
            }
            else
            {
                rLow = 0;
                rHigh = 0;
                existCheck.GetNetworkID(rLow, rHigh);
                rKey = rLow.ToString();
                rKey = rKey + ":";
                rKey = rKey + rHigh.ToString();
                s_ContainerMap.Set(rKey, this);

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

    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterSorter(this);
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        UnregisterContainer();
        #endif
    }

    override void LFPG_OnDeleted()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterSorter(this);
        UnregisterContainer();
        #endif
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
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
        if (m_PoweredNet)
        {
            SetObjectMaterial(0, LFPG_SORTER_RVMAT_ON);
        }
        else
        {
            SetObjectMaterial(0, LFPG_SORTER_RVMAT_OFF);
        }
        #endif
    }

    // ============================================
    // Persistence: LinkedContainer + FilterJSON
    // (after wireJSON from WireOwnerBase)
    // ============================================
    override void LFPG_OnStoreSaveDevice(ParamsWriteContext ctx)
    {
        ctx.Write(m_LinkedContainerLow);
        ctx.Write(m_LinkedContainerHigh);
        ctx.Write(m_FilterJSON);
    }

    override bool LFPG_OnStoreLoadDevice(ParamsReadContext ctx, int deviceVer)
    {
        if (!ctx.Read(m_LinkedContainerLow))
        {
            string errLow = "[LFPG_Sorter] OnStoreLoad failed: m_LinkedContainerLow";
            LFPG_Util.Error(errLow);
            return false;
        }

        if (!ctx.Read(m_LinkedContainerHigh))
        {
            string errHigh = "[LFPG_Sorter] OnStoreLoad failed: m_LinkedContainerHigh";
            LFPG_Util.Error(errHigh);
            return false;
        }

        if (!ctx.Read(m_FilterJSON))
        {
            string errFilter = "[LFPG_Sorter] OnStoreLoad failed: m_FilterJSON";
            LFPG_Util.Error(errFilter);
            return false;
        }

        if (m_FilterJSON != "")
        {
            m_FilterConfig.FromJSON(m_FilterJSON);
        }

        return true;
    }

    // ============================================
    // Container linking
    // ============================================
    void LFPG_LinkNearestContainer(vector searchPos)
    {
        #ifdef SERVER
        float bestDistSq = LFPG_SORTER_LINK_RADIUS * LFPG_SORTER_LINK_RADIUS;
        EntityAI bestContainer = null;

        ref array<Object> nearObjects = new array<Object>;
        GetGame().GetObjectsAtPosition(searchPos, LFPG_SORTER_LINK_RADIUS, nearObjects, null);

        int i;
        for (i = 0; i < nearObjects.Count(); i = i + 1)
        {
            Object obj = nearObjects[i];
            if (!obj)
                continue;

            if (obj == this)
                continue;

            EntityAI candidate = EntityAI.Cast(obj);
            if (!candidate)
                continue;

            Man manCheck = Man.Cast(candidate);
            if (manCheck)
                continue;

            if (LFPG_DeviceAPI.IsElectricDevice(candidate))
                continue;

            if (!candidate.GetInventory())
                continue;

            CargoBase candidateCargo = candidate.GetInventory().GetCargo();
            int attachCount = candidate.GetInventory().AttachmentCount();
            if (!candidateCargo && attachCount == 0)
                continue;

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

            string key = linkLow.ToString();
            key = key + ":";
            key = key + linkHigh.ToString();
            s_ContainerMap.Set(key, this);

            SetSynchDirty();

            string linkLog = "[LFPG_Sorter] Linked container: ";
            linkLog = linkLog + bestContainer.GetType();
            linkLog = linkLog + " netId=";
            linkLog = linkLog + linkLow.ToString();
            linkLog = linkLog + ":";
            linkLog = linkLog + linkHigh.ToString();
            LFPG_Util.Info(linkLog);
        }
        else
        {
            string noFoundMsg = "[LFPG_Sorter] No container found within ";
            noFoundMsg = noFoundMsg + LFPG_SORTER_LINK_RADIUS.ToString();
            noFoundMsg = noFoundMsg + "m";
            LFPG_Util.Warn(noFoundMsg);
        }
        #endif
    }

    EntityAI LFPG_GetLinkedContainer()
    {
        if (m_LinkedContainerLow == 0 && m_LinkedContainerHigh == 0)
            return null;

        EntityAI resolved = LFPG_DeviceAPI.ResolveByNetworkId(m_LinkedContainerLow, m_LinkedContainerHigh);
        if (!resolved)
        {
            #ifdef SERVER
            UnregisterContainer();
            m_LinkedContainerLow = 0;
            m_LinkedContainerHigh = 0;
            SetSynchDirty();
            string warnMsg = "[LFPG_Sorter] Linked container no longer exists — cleared stale reference";
            LFPG_Util.Warn(warnMsg);
            #endif
            return null;
        }
        return resolved;
    }

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

    int LFPG_GetLinkedContainerLow()
    {
        return m_LinkedContainerLow;
    }

    int LFPG_GetLinkedContainerHigh()
    {
        return m_LinkedContainerHigh;
    }

    bool LFPG_IsLinked()
    {
        if (m_LinkedContainerLow == 0 && m_LinkedContainerHigh == 0)
            return false;
        return true;
    }

    void LFPG_UnlinkContainer()
    {
        #ifdef SERVER
        UnregisterContainer();
        m_LinkedContainerLow = 0;
        m_LinkedContainerHigh = 0;
        SetSynchDirty();
        #endif
    }

    // ============================================
    // Filter config access
    // ============================================
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
        ref LFPG_SortConfig testConfig = new LFPG_SortConfig();
        bool parseOk = testConfig.FromJSON(json);
        if (!parseOk)
        {
            string rejectMsg = "[LFPG_Sorter] SetFilterJSON rejected: malformed JSON";
            LFPG_Util.Warn(rejectMsg);
            return false;
        }

        m_FilterJSON = json;
        m_FilterConfig = testConfig;
        return true;
        #else
        return false;
        #endif
    }
};
