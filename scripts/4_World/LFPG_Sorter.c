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
// Device schema 3 appends the linked container's persistent id.
static const int LFPG_SORTER_PERSIST_VERSION = 3;

static const string LFPG_SORTER_RVMAT_OFF = "LFPowerGrid\\data\\sorter\\materials\\lf_sorter_led_off.rvmat";
static const string LFPG_SORTER_RVMAT_ON  = "LFPowerGrid\\data\\sorter\\materials\\lf_sorter_led_on.rvmat";

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
    // F6 B1: idempotent re-registration point for the OnInit sweep
    // (devices restored during super.OnInit() registered against the
    // inert fallback). RegisterX dedups; this replicates only the
    // registration condition, never init side effects.
    override void LFPG_RegisterWithNetworkManager(LFPG_NetworkManager nm)
    {
        if (nm) nm.RegisterSorter(this);
    }

    // ---- Device-specific SyncVars ----
    protected int  m_LinkedContainerLow  = 0;
    protected int  m_LinkedContainerHigh = 0;
    protected bool m_PoweredNet          = false;
    protected bool m_Overloaded          = false;

    // ---- Filter config (persisted as JSON, NOT SyncVar) ----
    protected string m_FilterJSON = "";
    protected ref LFPG_SortConfig m_FilterConfig;

    // ---- Container uniqueness (static, server-side) ----
    protected static ref TStringManagedMap s_ContainerMap;

    // ---- Saved container link, resolved once in EEOnAfterLoad (server-side) ----
    protected int m_PendingLinkPid1 = 0;
    protected int m_PendingLinkPid2 = 0;
    protected int m_PendingLinkPid3 = 0;
    protected int m_PendingLinkPid4 = 0;

    // ---- Persistent id of the linked container, captured when it is linked (server-side) ----
    // The shutdown save can no longer resolve the session NetworkID, so the id is never resolved there.
    protected int m_LinkPid1 = 0;
    protected int m_LinkPid2 = 0;
    protected int m_LinkPid3 = 0;
    protected int m_LinkPid4 = 0;

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
            s_ContainerMap = new TStringManagedMap;
        }
    }

    // ============================================
    // SetActions
    // ============================================
    override void SetActions()
    {
        super.SetActions();
		AddAction(LFPG_ActionOpenSorterPanel_TEST);
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

        if (LFPG_LOG_LEVEL >= 2)
        {
            string msg = "[LFPG_Sorter] SetPowered(";
            msg = msg + powered.ToString();
            msg = msg + ") id=";
            msg = msg + m_DeviceId;
            LFPG_Util.Debug(msg);
        }
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
        if (nm) nm.RegisterSorter(this);

		// Restored session IDs are discarded on load; re-link is explicit.
        #endif
    }

    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        LFPG_NetworkManager nm = LFPG_NetworkManager.Get();
        if (nm) nm.UnregisterSorter(this);
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        // A ruined sorter drops its link entirely, so no restart can restore it.
        LFPG_UnlinkContainer();
        #endif
    }

    override void LFPG_OnDeleted()
    {
        #ifdef SERVER
        LFPG_NetworkManager nm = LFPG_NetworkManager.GetExisting();
        if (nm) nm.UnregisterSorter(this);
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
    // Persistence: legacy link slots + FilterJSON
    // + container persistent id (schema 3)
    // (after wireJSON from WireOwnerBase)
    // ============================================
    override int LFPG_GetDevicePersistVersion()
    {
        return LFPG_SORTER_PERSIST_VERSION;
    }

    override void LFPG_OnStoreSaveDevice(ParamsWriteContext ctx)
    {
		// Preserve the legacy int/int/string layout, including for old readers.
		// Session NetworkIDs are never persisted: the link travels as a persistent id.
		int noSessionLink = 0;
		ctx.Write(noSessionLink);
		ctx.Write(noSessionLink);
        ctx.Write(m_FilterJSON);

		int pid1 = 0;
		int pid2 = 0;
		int pid3 = 0;
		int pid4 = 0;
		LFPG_GetSavedLinkId(pid1, pid2, pid3, pid4);
		ctx.Write(pid1);
		ctx.Write(pid2);
		ctx.Write(pid3);
		ctx.Write(pid4);
    }

    override bool LFPG_OnStoreLoadDevice(ParamsReadContext ctx, int deviceVer)
    {
		// Consume legacy IDs without publishing them as a live container link.
		int legacyLow = 0;
		int legacyHigh = 0;
		string loadedFilterJSON;
		if (!ctx.Read(legacyLow))
        {
            string errLow = "[LFPG_Sorter] OnStoreLoad failed: m_LinkedContainerLow";
            LFPG_Util.Error(errLow);
            return false;
        }

		if (!ctx.Read(legacyHigh))
        {
            string errHigh = "[LFPG_Sorter] OnStoreLoad failed: m_LinkedContainerHigh";
            LFPG_Util.Error(errHigh);
            return false;
        }

		if (!ctx.Read(loadedFilterJSON))
        {
            string errFilter = "[LFPG_Sorter] OnStoreLoad failed: m_FilterJSON";
            LFPG_Util.Error(errFilter);
            return false;
        }

		int pid1 = 0;
		int pid2 = 0;
		int pid3 = 0;
		int pid4 = 0;
		if (deviceVer >= LFPG_SORTER_PERSIST_VERSION)
		{
			// The id is the last field: a short read only loses the saved link, never the rules or wires.
			if (!LFPG_ReadSavedLinkId(ctx, pid1, pid2, pid3, pid4))
			{
				pid1 = 0;
				pid2 = 0;
				pid3 = 0;
				pid4 = 0;
				string warnPid = "[LFPG_Sorter] OnStoreLoad: linked container persistent id unreadable; explicit container resync required";
				LFPG_Util.Warn(warnPid);
			}
		}

		// All fields have been read. Never resolve or proximity-replace a saved session ID.
		LFPG_UnlinkContainer();
		if (legacyLow != 0 || legacyHigh != 0)
		{
			LFPG_Util.Warn("[LFPG_Sorter] Discarded saved session link; explicit container resync required");
		}
		// The persistent id resolves in EEOnAfterLoad, once every stored entity exists.
		m_PendingLinkPid1 = pid1;
		m_PendingLinkPid2 = pid2;
		m_PendingLinkPid3 = pid3;
		m_PendingLinkPid4 = pid4;
		m_FilterJSON = loadedFilterJSON;

        if (m_FilterJSON != "")
        {
            m_FilterConfig.FromJSON(m_FilterJSON);
        }

        return true;
    }

    // ============================================
    // Restart: restore the saved container link
    // GetEntityByPersitentID is only available in this event
    // (see EntityAI.EEOnAfterLoad).
    // ============================================
    override void EEOnAfterLoad()
    {
        super.EEOnAfterLoad();

        #ifdef SERVER
        LFPG_RestoreSavedLink();
        #endif
    }

    protected void LFPG_RestoreSavedLink()
    {
        #ifdef SERVER
        int pid1 = m_PendingLinkPid1;
        int pid2 = m_PendingLinkPid2;
        int pid3 = m_PendingLinkPid3;
        int pid4 = m_PendingLinkPid4;
        LFPG_ClearPendingLink();

        bool hasSavedLink = (pid1 != 0 || pid2 != 0);
        if (!hasSavedLink)
            hasSavedLink = (pid3 != 0 || pid4 != 0);
        if (!hasSavedLink)
            return;

        if (IsRuined())
            return;

        // Same container rule as an explicit resync, plus the tick's link radius.
        EntityAI container = g_Game.GetEntityByPersitentID(pid1, pid2, pid3, pid4);
        bool restorable = LFPG_IsLinkCandidate(container);
        if (restorable)
        {
            float linkDistSq = LFPG_WorldUtil.DistSq(GetPosition(), container.GetPosition());
            float linkRadiusSq = LFPG_SORTER_LINK_RADIUS * LFPG_SORTER_LINK_RADIUS;
            restorable = (linkDistSq <= linkRadiusSq);
        }

        if (restorable)
        {
            string restoreMsg = "[LFPG_Sorter] Restored saved container link: ";
            restoreMsg = restoreMsg + container.GetType();
            restoreMsg = restoreMsg + " id=";
            restoreMsg = restoreMsg + m_DeviceId;
            LFPG_Util.Info(restoreMsg);
            LFPG_LinkContainer(container);
            return;
        }

        string lostMsg = "[LFPG_Sorter] Saved container not restored (missing, out of range or claimed); explicit container resync required id=";
        lostMsg = lostMsg + m_DeviceId;
        LFPG_Util.Warn(lostMsg);
        #endif
    }

    // Persistent id to save for the link: the one captured when the container was
    // linked, otherwise a restore that has not run yet. Zeros mean no link.
    protected void LFPG_GetSavedLinkId(out int pid1, out int pid2, out int pid3, out int pid4)
    {
        pid1 = m_PendingLinkPid1;
        pid2 = m_PendingLinkPid2;
        pid3 = m_PendingLinkPid3;
        pid4 = m_PendingLinkPid4;

        #ifdef SERVER
        if (m_LinkedContainerLow == 0 && m_LinkedContainerHigh == 0)
            return;

        pid1 = m_LinkPid1;
        pid2 = m_LinkPid2;
        pid3 = m_LinkPid3;
        pid4 = m_LinkPid4;
        #endif
    }

    // Reads the four persistent id fields of schema 3; false on a short read.
    protected bool LFPG_ReadSavedLinkId(ParamsReadContext ctx, out int pid1, out int pid2, out int pid3, out int pid4)
    {
        if (!ctx.Read(pid1))
            return false;
        if (!ctx.Read(pid2))
            return false;
        if (!ctx.Read(pid3))
            return false;
        if (!ctx.Read(pid4))
            return false;
        return true;
    }

    protected void LFPG_ClearPendingLink()
    {
        m_PendingLinkPid1 = 0;
        m_PendingLinkPid2 = 0;
        m_PendingLinkPid3 = 0;
        m_PendingLinkPid4 = 0;
    }

    // ============================================
    // Container linking
    // ============================================
	EntityAI LFPG_FindNearestContainerCandidate(float maxDist)
	{
		return LFPG_FindContainerCandidateAt(GetPosition(), maxDist);
	}

	protected EntityAI LFPG_FindContainerCandidateAt(vector searchPos, float maxDist)
	{
        #ifdef SERVER
        float bestDistSq = maxDist * maxDist;
        EntityAI bestContainer = null;

        array<Object> nearObjects = new array<Object>;
        g_Game.GetObjectsAtPosition(searchPos, maxDist, nearObjects, null);

        int i;
        for (i = 0; i < nearObjects.Count(); i = i + 1)
        {
            EntityAI candidate = EntityAI.Cast(nearObjects[i]);
            if (!LFPG_IsLinkCandidate(candidate))
                continue;

            float distSq = LFPG_WorldUtil.DistSq(searchPos, candidate.GetPosition());
            if (distSq < bestDistSq)
            {
                bestDistSq = distSq;
                bestContainer = candidate;
            }
        }

        return bestContainer;
        #else
        return null;
        #endif
    }

	#ifdef SERVER
	// Container rule shared by the explicit link search and the restart restore.
	// Clears a stale claim left by a ruined or deleted sorter.
	protected bool LFPG_IsLinkCandidate(EntityAI candidate)
	{
        if (!candidate)
            return false;

        if (candidate == this)
            return false;

        // Only a container standing in the world, never one carried, attached or stored in another entity.
        if (candidate.GetHierarchyParent())
            return false;

        Man manCheck = Man.Cast(candidate);
        if (manCheck)
            return false;

        if (LFPG_DeviceAPI.IsElectricDevice(candidate))
            return false;

        if (!candidate.GetInventory())
            return false;

        CargoBase candidateCargo = candidate.GetInventory().GetCargo();
		// Tick, manual sort and preview consume cargo only.
		if (!candidateCargo)
            return false;

        int candLow = 0;
        int candHigh = 0;
        candidate.GetNetworkID(candLow, candHigh);
        string candKey = candLow.ToString();
        candKey = candKey + ":";
        candKey = candKey + candHigh.ToString();

        if (s_ContainerMap.Contains(candKey))
        {
            EntityAI claimant = EntityAI.Cast(s_ContainerMap.Get(candKey));
            bool claimantValid = false;
            if (claimant)
            {
                LFPG_Sorter claimSorter = LFPG_Sorter.Cast(claimant);
                if (claimSorter && !claimSorter.IsRuined())
                {
                    claimantValid = true;
                }
            }
            if (claimantValid && claimant != this)
            {
                return false;
            }
			// Keep the stale-claim cleanup shared with the explicit link search.
			if (!claimantValid)
			{
				s_ContainerMap.Remove(candKey);
			}
        }

        return true;
	}
	#endif

    void LFPG_LinkContainer(EntityAI container)
    {
        #ifdef SERVER
        if (!container)
            return;

        // An explicit link supersedes a saved link that has not been restored.
        LFPG_ClearPendingLink();
        container.GetPersistentID(m_LinkPid1, m_LinkPid2, m_LinkPid3, m_LinkPid4);

        int linkLow = 0;
        int linkHigh = 0;
        container.GetNetworkID(linkLow, linkHigh);
        m_LinkedContainerLow = linkLow;
        m_LinkedContainerHigh = linkHigh;

        string key = linkLow.ToString();
        key = key + ":";
        key = key + linkHigh.ToString();
        s_ContainerMap.Set(key, this);

        SetSynchDirty();

        string linkLog = "[LFPG_Sorter] Linked container: ";
        linkLog = linkLog + container.GetType();
        linkLog = linkLog + " netId=";
        linkLog = linkLog + linkLow.ToString();
        linkLog = linkLog + ":";
        linkLog = linkLog + linkHigh.ToString();
        LFPG_Util.Info(linkLog);
        #endif
    }

    void LFPG_LinkNearestContainer(vector searchPos)
    {
        #ifdef SERVER
		EntityAI bestContainer = LFPG_FindContainerCandidateAt(searchPos, LFPG_SORTER_LINK_RADIUS);

        if (bestContainer)
        {
            LFPG_LinkContainer(bestContainer);
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

        #ifdef SERVER
        // A container picked up, attached or stored inside another entity stops being a sorter link.
        if (resolved.GetHierarchyParent())
        {
            LFPG_UnlinkContainer();
            string nestedMsg = "[LFPG_Sorter] Linked container is now inside another entity; link cleared id=";
            nestedMsg = nestedMsg + m_DeviceId;
            LFPG_Util.Warn(nestedMsg);
            return null;
        }
        #endif
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
            EntityAI owner = EntityAI.Cast(s_ContainerMap.Get(key));
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
        LFPG_ClearPendingLink();
        m_LinkPid1 = 0;
        m_LinkPid2 = 0;
        m_LinkPid3 = 0;
        m_LinkPid4 = 0;
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
        SetSynchDirty();
        return true;
        #else
        return false;
        #endif
    }
};
