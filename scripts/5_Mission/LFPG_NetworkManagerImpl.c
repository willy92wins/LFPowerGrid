class LFPG_SorterResumeState
{
	EntityAI m_Item;
	EntityAI m_InputContainer;
	int m_ItemIndex;
	int m_Output;
	int m_Rule;
	int m_WireMask;
	int m_WireGeneration;
	ref LFPG_SortConfig m_Config;
	ref array<EntityAI> m_Destinations = new array<EntityAI>;
	void Clear()
	{
		m_Item = null;
		m_InputContainer = null;
		m_ItemIndex = 0;
		m_Output = 0;
		m_Rule = 0;
		m_Config = null;
		m_WireMask = 0;
		m_WireGeneration = -1;
		m_Destinations.Clear();
	}
	void Store(EntityAI item, int itemIndex, int outputIndex, int ruleIndex, LFPG_SortConfig config, int wireMask, int wireGeneration, EntityAI inputContainer, array<EntityAI> destinations)
	{
		m_Item = item;
		m_ItemIndex = itemIndex;
		m_Output = outputIndex;
		m_Rule = ruleIndex;
		m_Config = config;
		m_WireMask = wireMask;
		m_WireGeneration = wireGeneration;
		m_InputContainer = inputContainer;
		m_Destinations.Clear();
		for (int routeIndex = 0; routeIndex < destinations.Count(); routeIndex = routeIndex + 1)
			m_Destinations.Insert(destinations[routeIndex]);
	}
	bool MatchesRoutes(int wireMask, int wireGeneration, array<EntityAI> destinations)
	{
		if (m_WireMask != wireMask || m_WireGeneration != wireGeneration || m_Destinations.Count() != destinations.Count())
			return false;
		for (int routeIndex = 0; routeIndex < destinations.Count(); routeIndex = routeIndex + 1)
		{
			if (m_Destinations[routeIndex] != destinations[routeIndex])
				return false;
		}
		return true;
	}
};
class LFPG_NetworkManagerImpl : LFPG_NetworkManager
{
    static LFPG_ControlSessionRegistry Sessions()
    {
        LFPG_NetworkManagerImpl impl = LFPG_NetworkManagerImpl.Cast(LFPG_NetworkManager.Get());
        if (!impl)
            return null;
        return impl.GetControlSessionRegistry();
    }
    protected ref LFPG_ControlSessionRegistry m_ControlSessions;
    protected ref TStringManagedRefMap m_RateByPlayer;
    protected ref map<string, ref array<ref LFPG_WireData>> m_VanillaWires;
	protected ref array<ref LFPG_WireData> m_WireQueryStore;
    protected ref map<string, int> m_ReverseIdx;
    protected ref map<string, ref array<string>> m_ReverseOwners;
    protected ref map<string, int> m_WiresByPlayer;
    protected bool m_SelfHealQueued = false;
    protected bool m_CutGraphRebuildQueued = false;
    protected bool m_IndexHealAfterCut = false;
    protected bool m_ReverseIndexTrusted = false;
    protected bool m_ValidationOnlyHealPending = false;
    protected bool m_ValidationSkipGraphRebuild = false;
    protected bool m_ValidationAfterCutRequested = false;
    protected int m_GraphRebuildGeneration = 0;
    protected int m_ValidationGraphGeneration = 0;
    protected bool m_GraphFullRebuildRequired = true;
    protected string m_ExplicitGraphRemovalCredit = "";
    protected bool m_CutAllGraphBatchActive = false;
    protected bool m_CutAllHasPreviousOwnerPosition = false;
    protected vector m_CutAllPreviousOwnerPosition;
    protected ref map<string, bool> m_CutPendingPowerOff;
    protected ref map<string, bool> m_PortLocks;
    protected bool m_StartupValidationDone = false;
    protected bool m_ValidationActive = false;
    protected bool m_ValidationRerunRequested = false;
    protected int m_ValidationPhase;
    protected int m_ValidationCursor;
    protected int m_ValidationStartMs;
    protected int m_ValidationOwnersPruned;
    protected ref array<EntityAI> m_ValidationDevices;
    protected ref map<string, bool> m_ValidationValidIds;
    protected ref array<string> m_ValidationVanillaIds;
    protected bool m_FullSyncInProgress = false;
    protected PlayerBase m_FullSyncPlayer;
	protected bool m_FullSyncReplayActive = false;
    protected ref array<EntityAI> m_FullSyncPendingPlayers;
    protected ref array<EntityAI> m_FullSyncOwners;
    protected ref array<string> m_FullSyncVanillaIds;
    protected int m_FullSyncOwnerCursor;
    protected int m_FullSyncVanillaCursor;
    protected vector m_FullSyncPlayerPos;
    protected float m_FullSyncMaxDistSq;
    protected static const int LFPG_OWNER_SNAPSHOT_MAX_INTEREST_POSITIONS = 128;
    protected ref TStringManagedRefMap m_DeferredOwnerSnapshots;
    protected ref array<string>   m_DeferredBroadcastVanillaIds;
    protected ref array<EntityAI> m_DeferredBroadcastVanillaObjs;
    protected bool m_VanillaDirty = false;
    protected int m_LastVanillaSaveFailureWarnMs = 0;
    protected int m_VanillaSaveFailureCount = 0;
    protected static const int LFPG_VANILLA_SAVE_WARN_INTERVAL_MS = 60000;
    protected int m_VanillaLoadedVer = 0;
    protected bool m_VanillaReadOnly = false;
    protected bool m_DeferredPruneScheduled = false;
    protected bool m_SolarHasSun = false;
    protected float m_TankFillLastMs = -1.0;
    protected int m_SorterCursor = 0;
    protected ref array<EntityAI> m_RegisteredSorters;
    protected ref array<EntityAI> m_SorterItemCache;
    protected ref array<EntityAI> m_TickAffectedContainers;
    protected ref array<EntityAI> m_TickDirtyDestinations;
    protected ref array<EntityAI> m_TickDirtySources;
    protected ref InventoryLocation m_SorterMoveSourceLocation;
    protected ref InventoryLocation m_SorterMoveDestinationLocation;
    #ifndef SERVER
    protected int m_PerfDiagSorterDestDirtyCount;
    #endif
    #ifndef SERVER
    protected int m_PerfDiagSorterSourceDirtyCount;
    #endif
	protected ref array<ref LFPG_SorterResumeState> m_SorterResumes;
	protected ref array<EntityAI> m_SorterOutputContainers;
    #ifndef SERVER
    protected int m_PerfDiagSorterRuleChecks;
    #endif
    #ifndef SERVER
    protected int m_PerfDiagSorterConfigMisses;
    #endif
    #ifndef SERVER
    protected int m_PerfDiagSorterDeferrals;
    #endif
    protected ref array<EntityAI> m_RegisteredSensors;
    protected ref array<EntityAI> m_RegisteredPads;
    protected ref array<EntityAI> m_RegisteredLasers;
    protected ref array<EntityAI> m_RegisteredBatteries;
    protected float m_BatteryLastTickMs;
	protected ref map<string, float> m_BatteryRebuildGeneration;
	protected ref map<string, float> m_BatteryRebuildDemand;
    protected ref array<EntityAI> m_RegisteredIntercoms;
    protected ref array<EntityAI> m_RegisteredFurnaces;
    protected ref array<EntityAI> m_RegisteredFridges;
    protected ref array<int> m_RegisteredFridgePhases;
    protected int m_NextFridgePhase = 0;
    protected ref array<EntityAI> m_RegisteredStoves;
    protected ref array<int> m_RegisteredStovePhases;
    protected int m_NextStovePhase = 0;
    protected ref array<EntityAI> m_RegisteredDoorControllers;
    protected ref array<EntityAI> m_RegisteredSolars;
    protected ref array<EntityAI> m_RegisteredT1Pumps;
    protected ref array<EntityAI> m_RegisteredT2Pumps;
    protected ref array<EntityAI> m_RegisteredSprinklers;
    protected ref array<int> m_RegisteredSprinklerPhases;
    protected int m_NextSprinklerPhase = 0;
    protected int m_PlayerDetectCounter;
    protected static const float LFPG_PLAYER_CELL_SIZE_M = 10.0;
	protected bool m_PlayerCellsBuiltThisTurn = false;
	protected ref map<string, int> m_PlayerCellIndex;
    protected ref array<Man> m_PlayerCellPlayers;
    protected ref array<int> m_PlayerCellMembership;
    protected ref array<int> m_PlayerCellX;
    protected ref array<int> m_PlayerCellZ;
    protected ref array<int> m_PlayerCellStart;
    protected ref array<int> m_PlayerCellCount;
    protected ref array<int> m_PlayerCellWrite;
    protected ref array<Man> m_PlayerCellOrdered;
    protected ref array<Man> m_PlayerCandidates;
    protected int m_LaserDetectCursor;
    protected int m_PadDetectCursor;
    protected int m_SensorDetectCursor;
    protected int m_LaserRaycastCursor;
    #ifndef SERVER
    protected int m_PerfDiagLaserEvaluations;
    #endif
    #ifndef SERVER
    protected int m_PerfDiagPadEvaluations;
    #endif
    #ifndef SERVER
    protected int m_PerfDiagSensorEvaluations;
    #endif
    #ifndef SERVER
    protected int m_PerfDiagLaserDormant;
    #endif
    #ifndef SERVER
    protected int m_PerfDiagPadDormant;
    #endif
    #ifndef SERVER
    protected int m_PerfDiagSensorDormant;
    #endif
    #ifndef SERVER
    protected int m_PerfDiagLaserChanges;
    #endif
    #ifndef SERVER
    protected int m_PerfDiagPadChanges;
    #endif
    #ifndef SERVER
    protected int m_PerfDiagSensorChanges;
    #endif
    protected int m_SimpleTickCounter;
    protected int m_FridgePhaseCursor;
    protected int m_StovePhaseCursor;
    protected int m_SprinklerPhaseCursor;
    protected ref array<Man> m_SprinklerWetPlayers;
    #ifndef SERVER
    protected int m_PerfDiagWetApplied;
    #endif
    #ifndef SERVER
    protected int m_PerfDiagWetCoalesced;
    #endif
    #ifndef SERVER
    protected int m_PerfDiagWetPreGateSkips;
    #endif
    protected ref map<string, bool> m_CachedValidIds;
    protected ref LFPG_BTCPriceFetcher m_BTCPriceFetcher;
    protected ref array<Man>      m_ReusablePlayers;
    protected ref array<string>   m_ReusableMovedIds;
    protected ref array<EntityAI> m_ReusableMovedDevs;
    protected ref array<vector>   m_ReusableMovedOldPositions;
    protected ref array<string>   m_ReusableDisappearedIds;
    static const int LFPG_SERVER_SCHEDULER_TICK_MS = 100;
    protected static const int LFPG_VALIDATE_RESOLVE_VANILLA = 1;
    protected static const int LFPG_VALIDATE_SNAPSHOT_PRE = 2;
    protected static const int LFPG_VALIDATE_RESOLVE_LFPG = 3;
    protected static const int LFPG_VALIDATE_SNAPSHOT_FINAL = 4;
    protected static const int LFPG_VALIDATE_BUILD_VALID = 5;
    protected static const int LFPG_VALIDATE_REFRESH_LFPG = 6;
    protected static const int LFPG_VALIDATE_REFRESH_VANILLA = 7;
    protected static const int LFPG_VALIDATE_PRUNE_LFPG = 8;
    protected static const int LFPG_VALIDATE_INDEX_REBUILD = 9;
    protected static const int LFPG_VALIDATE_SCHEDULE_PRUNE = 12;
    protected static const int LFPG_VALIDATE_GRAPH_REBUILD = 13;
    protected static const int LFPG_VALIDATE_GRAPH_POPULATE = 14;
    protected static const int LFPG_VALIDATE_GRAPH_MARK = 15;
    protected static const int LFPG_VALIDATE_PRUNE_POSITIONS = 16;
    protected static const int LFPG_VALIDATE_REBUILD_TRACKED = 17;
    protected static const int LFPG_VALIDATE_FINALIZE = 18;
    protected ref Timer m_ServerScheduler;
    protected ref array<string> m_StaleRateLimiterKeys;
    protected int m_SchedPurgeMs;
    protected int m_SchedFlushMs;
    protected int m_SchedPropagationMs;
    protected int m_SchedMovementMs;
    protected int m_SchedSolarMs;
    protected int m_SchedPumpMs;
    protected int m_SchedSorterMs;
    protected int m_SchedPlayerDetectionMs;
    protected int m_SchedSimpleMs;
    protected int m_SchedBtcMs;
    protected int m_SchedBtcIntervalMs;
    protected ref array<Man>      m_ReusableBroadcastPlayers;
    protected ref array<vector>   m_ReusableBroadcastPositions;
    protected ref array<string>   m_ReusableReversePorts;
    protected ref array<EntityAI> m_ReusableCutAllDevices;
    protected ref array<ref LFPG_WireData> m_ReusableCutAllFallbackWires;
    #ifndef SERVER
    protected int m_PerfDiagOwnerSnapshotUnicastCount;
    #endif
    #ifndef SERVER
    protected int m_PerfDiagOwnerDeltaSendCount;
    #endif
    protected static const string VANILLA_WIRES_DIR  = "$profile:LF_PowerGrid";
    protected static const string VANILLA_WIRES_FILE = "$profile:LF_PowerGrid\\vanilla_wires.json";
    protected static const float RATE_LIMITER_STALE_SEC = 600.0;
    protected static const int LFPG_RPC_MAX_OPS_PER_SEC = 5;
    protected ref map<string, float> m_RateWindowStart;
    protected ref map<string, int>   m_RateOpsInWindow;
    protected ref TStringManagedMap m_PendingBroadcastLFPG;
    protected ref TStringManagedRefMap m_PendingOwnerSnapshots;
    protected ref TStringManagedMap m_PendingBroadcastVanilla;
    protected ref LFPG_ElecGraph m_Graph;
    protected bool m_WarmupActive;
    protected int m_TelemTickCount;
    protected int m_TelemTotalProcessMs;
    protected int m_TelemPeakProcessMs;
    protected int m_TelemTotalEdgesVisited;
    protected float m_TelemLastDumpMs;
    protected ref map<string, vector> m_LastKnownPos;
    protected ref array<string>       m_TrackedDeviceIds;
    protected ref map<string, int>    m_TrackedDeviceIndex;
    protected int                     m_TrackCursor;
    void LFPG_NetworkManagerImpl()
    {
        m_RateByPlayer = new TStringManagedRefMap;
        m_RateWindowStart = new map<string, float>;
        m_RateOpsInWindow = new map<string, int>;
        m_VanillaWires = new map<string, ref array<ref LFPG_WireData>>;
        m_ReverseIdx = new map<string, int>;
        m_ReverseOwners = new map<string, ref array<string>>;
        m_WiresByPlayer = new map<string, int>;
        m_PendingBroadcastLFPG = new TStringManagedMap;
        m_PendingOwnerSnapshots = new TStringManagedRefMap;
        m_PendingBroadcastVanilla = new TStringManagedMap;
        m_LastKnownPos = new map<string, vector>;
        m_PortLocks = new map<string, bool>;
        m_FullSyncPendingPlayers = new array<EntityAI>;
        m_FullSyncOwners = new array<EntityAI>;
        m_FullSyncVanillaIds = new array<string>;
        m_DeferredOwnerSnapshots = new TStringManagedRefMap;
        m_DeferredBroadcastVanillaIds = new array<string>;
        m_DeferredBroadcastVanillaObjs = new array<EntityAI>;
        m_RegisteredSorters = new array<EntityAI>;
        m_SorterItemCache = new array<EntityAI>;
        m_TickAffectedContainers = new array<EntityAI>;
        m_TickDirtyDestinations = new array<EntityAI>;
        m_TickDirtySources = new array<EntityAI>;
		m_SorterResumes = new array<ref LFPG_SorterResumeState>;
		m_SorterOutputContainers = new array<EntityAI>;
        m_RegisteredSensors = new array<EntityAI>;
        m_RegisteredPads = new array<EntityAI>;
        m_RegisteredLasers = new array<EntityAI>;
        m_RegisteredBatteries = new array<EntityAI>;
        m_RegisteredIntercoms = new array<EntityAI>;
        m_RegisteredFurnaces = new array<EntityAI>;
        m_RegisteredFridges = new array<EntityAI>;
        m_RegisteredFridgePhases = new array<int>;
        m_RegisteredStoves = new array<EntityAI>;
        m_RegisteredStovePhases = new array<int>;
        m_RegisteredDoorControllers = new array<EntityAI>;
        m_RegisteredSolars = new array<EntityAI>;
        m_RegisteredT1Pumps = new array<EntityAI>;
        m_RegisteredT2Pumps = new array<EntityAI>;
        m_RegisteredSprinklers = new array<EntityAI>;
        m_RegisteredSprinklerPhases = new array<int>;
        m_ReusablePlayers = new array<Man>;
        m_ReusableMovedIds = new array<string>;
        m_ReusableMovedDevs = new array<EntityAI>;
        m_ReusableMovedOldPositions = new array<vector>;
        m_ReusableDisappearedIds = new array<string>;
        m_ReusableBroadcastPlayers = new array<Man>;
        m_ReusableBroadcastPositions = new array<vector>;
        m_ReusableReversePorts = new array<string>;
        m_ReusableCutAllDevices = new array<EntityAI>;
        m_ReusableCutAllFallbackWires = new array<ref LFPG_WireData>;
        m_StaleRateLimiterKeys = new array<string>;
        m_SchedPurgeMs = 0;
        m_SchedFlushMs = 0;
        m_SchedPropagationMs = 0;
        m_SchedMovementMs = 0;
        m_SchedSolarMs = 0;
        m_SchedPumpMs = 0;
        m_SchedSorterMs = 0;
        m_SchedPlayerDetectionMs = 0;
        m_SchedSimpleMs = 0;
        m_SchedBtcMs = 0;
        m_SchedBtcIntervalMs = 0;
        #ifdef SERVER
        m_ControlSessions = new LFPG_ControlSessionRegistry();
        m_TrackedDeviceIds = new array<string>;
        m_ValidationDevices = new array<EntityAI>;
        m_ValidationValidIds = new map<string, bool>;
        m_ValidationVanillaIds = new array<string>;
        m_CutPendingPowerOff = new map<string, bool>;
        m_TrackedDeviceIndex = new map<string, int>;
        m_TrackCursor = 0;
        m_SorterMoveSourceLocation = new InventoryLocation;
        m_SorterMoveDestinationLocation = new InventoryLocation;
		m_BatteryRebuildGeneration = new map<string, float>;
		m_BatteryRebuildDemand = new map<string, float>;
		m_PlayerCellIndex = new map<string, int>;
        m_PlayerCellPlayers = new array<Man>;
        m_PlayerCellMembership = new array<int>;
        m_PlayerCellX = new array<int>;
        m_PlayerCellZ = new array<int>;
        m_PlayerCellStart = new array<int>;
        m_PlayerCellCount = new array<int>;
        m_PlayerCellWrite = new array<int>;
        m_PlayerCellOrdered = new array<Man>;
        m_PlayerCandidates = new array<Man>;
        m_SprinklerWetPlayers = new array<Man>;
        LFPG_SorterLogic.InitCaches();
        MissionBaseWorld mw = MissionBaseWorld.Cast(g_Game.GetMission());
        if (mw) m_Graph = mw.LFPG_CreateElecGraph();
        if (!m_Graph)
        {
            LFPG_Util.Error("[LFPG_NetworkManager] Mission factory unavailable - fallback base graph (sim degraded)");
            m_Graph = new LFPG_ElecGraph();
        }
        m_WarmupActive = false;
        m_TelemTickCount = 0;
        m_TelemTotalProcessMs = 0;
        m_TelemPeakProcessMs = 0;
        m_TelemTotalEdgesVisited = 0;
        m_TelemLastDumpMs = -99999.0;
        string initMsg = "NetworkManager init (server).";
        LFPG_Util.Info(initMsg);
        LoadVanillaWires();
        bool bFalse = false;
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ValidateAllWiresAndPropagate, 5000, bFalse);
        LFPG_ComputeSunState();
        LFPG_InitTankFillTime();
        m_PlayerDetectCounter = 0;
        m_SimpleTickCounter = 0;
        m_BatteryLastTickMs = g_Game.GetTime();
        LFPG_BTCConfig.Load();
        LFPG_BalanceProvider_NativeImpl nativeProv = new LFPG_BalanceProvider_NativeImpl();
        LFPG_BalanceRegistry.Register(nativeProv);
        #ifdef LBmaster_Core
        LFPG_BalanceProvider_LBmaster lbProv = new LFPG_BalanceProvider_LBmaster();
        LFPG_BalanceRegistry.Register(lbProv);
        #endif
        string balMode = LFPG_BTCConfig.GetBalanceMode();
        LFPG_BalanceRegistry.Init(balMode);
        if (LFPG_BTCConfig.IsEnabled())
        {
            LFPG_BTCPriceFetcher.Create();
            m_BTCPriceFetcher = LFPG_BTCPriceFetcher.Get();
            if (m_BTCPriceFetcher)
            {
                m_BTCPriceFetcher.Init();
                m_SchedBtcIntervalMs = LFPG_BTC_PRICE_CHECK_MS;
                string btcInitMsg = "[NM] BTC Price fetcher initialized, tick every ";
                btcInitMsg = btcInitMsg + m_SchedBtcIntervalMs.ToString();
                btcInitMsg = btcInitMsg + "ms";
                LFPG_Util.Info(btcInitMsg);
            }
        }
        else
        {
            string btcOffMsg = "[NM] BTC ATM system DISABLED by config";
            LFPG_Util.Info(btcOffMsg);
        }
        StartServerScheduler();
        #endif
    }
    LFPG_ControlSessionRegistry GetControlSessionRegistry()
    {
        return m_ControlSessions;
    }
    override void StartServerScheduler()
    {
        #ifdef SERVER
        if (!g_Game || !g_Game.IsServer())
            return;
        if (m_ServerScheduler)
            return;
        m_ControlSessions = new LFPG_ControlSessionRegistry();
        m_SchedPurgeMs = 0;
        m_SchedFlushMs = 0;
        m_SchedPropagationMs = 0;
        m_SchedMovementMs = 0;
        m_SchedSolarMs = 0;
        m_SchedPumpMs = 0;
        m_SchedSorterMs = 0;
        m_SchedPlayerDetectionMs = 0;
        m_SchedSimpleMs = 0;
        m_SchedBtcMs = 0;
        m_ServerScheduler = new Timer(CALL_CATEGORY_SYSTEM);
        float tickSeconds = LFPG_SERVER_SCHEDULER_TICK_MS / 1000.0;
        m_ServerScheduler.Run(tickSeconds, this, "LFPG_ServerSchedulerTick", NULL, true);
        LFPG_Util.Info("[Scheduler] start tick_ms=100");
        #endif
    }
    override void StopServerScheduler()
    {
        #ifdef SERVER
        if (m_ServerScheduler)
        {
            m_ServerScheduler.Stop();
            m_ServerScheduler = null;
            LFPG_Util.Info("[Scheduler] stop");
        }
        #endif
    }
    protected void LFPG_ServerSchedulerTick()
    {
        #ifdef SERVER
		m_PlayerCellsBuiltThisTurn = false;
        if (m_ControlSessions)
            m_ControlSessions.Tick();
        m_SchedPurgeMs += LFPG_SERVER_SCHEDULER_TICK_MS;
        if (m_SchedPurgeMs >= 300000)
        {
            m_SchedPurgeMs = 0;
            PurgeStaleRateLimiters();
        }
        m_SchedFlushMs += LFPG_SERVER_SCHEDULER_TICK_MS;
        if (m_SchedFlushMs >= LFPG_VANILLA_FLUSH_S * 1000)
        {
            m_SchedFlushMs = 0;
            FlushVanillaIfDirty();
        }
        m_SchedPropagationMs += LFPG_SERVER_SCHEDULER_TICK_MS;
        if (m_SchedPropagationMs >= LFPG_PROPAGATE_TICK_MS)
        {
            m_SchedPropagationMs = 0;
            TickPropagation();
        }
        m_SchedMovementMs += LFPG_SERVER_SCHEDULER_TICK_MS;
        if (m_SchedMovementMs >= LFPG_MOVE_DETECT_TICK_MS)
        {
            m_SchedMovementMs = 0;
            CheckDeviceMovement();
        }
        m_SchedSolarMs += LFPG_SERVER_SCHEDULER_TICK_MS;
        if (m_SchedSolarMs >= LFPG_SOLAR_CHECK_MS)
        {
            m_SchedSolarMs = 0;
            LFPG_TickSolarPanels();
        }
        m_SchedPumpMs += LFPG_SERVER_SCHEDULER_TICK_MS;
        if (m_SchedPumpMs >= LFPG_PUMP_CHECK_MS)
        {
            m_SchedPumpMs = 0;
            LFPG_TickWaterPumps();
        }
        m_SchedSorterMs += LFPG_SERVER_SCHEDULER_TICK_MS;
        if (m_SchedSorterMs >= LFPG_SORTER_TICK_MS)
        {
            m_SchedSorterMs = 0;
            LFPG_TickSorters();
        }
        m_SchedPlayerDetectionMs += LFPG_SERVER_SCHEDULER_TICK_MS;
        if (m_SchedPlayerDetectionMs >= 300)
        {
            m_SchedPlayerDetectionMs = 0;
            LFPG_TickPlayerDetection();
        }
        m_SchedSimpleMs += LFPG_SERVER_SCHEDULER_TICK_MS;
        if (m_SchedSimpleMs >= 1000)
        {
            m_SchedSimpleMs = 0;
            LFPG_TickSimpleDevices();
        }
        if (m_SchedBtcIntervalMs > 0)
        {
            m_SchedBtcMs += LFPG_SERVER_SCHEDULER_TICK_MS;
        }
        if (m_SchedBtcIntervalMs > 0 && m_SchedBtcMs >= m_SchedBtcIntervalMs)
        {
            m_SchedBtcMs = 0;
            LFPG_TickBTCPrice();
        }
        LFPG_ProcessStartupValidationSlice();
        LFPG_ProcessFullSyncSpread();
        #endif
    }
    override bool AllowPlayerAction(PlayerIdentity ident)
    {
        if (!ident) return false;
        string pid = ident.GetId();
        LFPG_ServerSettings st = LFPG_Settings.Get();
        float now = g_Game.GetTime() * 0.001;
        float windowStart = 0.0;
        int opsInWindow = 0;
        if (m_RateWindowStart.Find(pid, windowStart))
        {
            float elapsed = now - windowStart;
            if (elapsed >= 1.0)
            {
                m_RateWindowStart[pid] = now;
                m_RateOpsInWindow[pid] = 0;
                opsInWindow = 0;
            }
            else
            {
                if (!m_RateOpsInWindow.Find(pid, opsInWindow))
                    opsInWindow = 0;
                if (opsInWindow >= LFPG_RPC_MAX_OPS_PER_SEC)
                {
					string swLog = "[RateLimiter] Sliding window exceeded for " + LFPG_Util.LogUid(pid);
                    swLog = swLog + " ops=" + opsInWindow.ToString();
					LFPG_Util.RateLimitedWarn(ident, "global_sliding_window", swLog);
                    return false;
                }
            }
        }
        else
        {
            m_RateWindowStart[pid] = now;
            m_RateOpsInWindow[pid] = 0;
            opsInWindow = 0;
        }
        m_RateOpsInWindow[pid] = opsInWindow + 1;
        ref LFPG_RateLimiter rl;
        Managed rateLimiterRaw;
        if (!m_RateByPlayer.Find(pid, rateLimiterRaw) || !Class.CastTo(rl, rateLimiterRaw) || !rl)
        {
            rl = new LFPG_RateLimiter();
            m_RateByPlayer[pid] = rl;
        }
        return rl.Allow(now, st.RpcCooldownSeconds);
    }
    protected void PurgeStaleRateLimiters()
    {
        #ifdef SERVER
        float now = g_Game.GetTime() * 0.001;
        m_StaleRateLimiterKeys.Clear();
        int i;
        for (i = 0; i < m_RateByPlayer.Count(); i = i + 1)
        {
            LFPG_RateLimiter rl = LFPG_RateLimiter.Cast(m_RateByPlayer.GetElement(i));
            if (!rl) continue;
            float idleSec = now - rl.GetNextAllowed();
            if (idleSec > RATE_LIMITER_STALE_SEC)
            {
                m_StaleRateLimiterKeys.Insert(m_RateByPlayer.GetKey(i));
            }
        }
        int removed = m_StaleRateLimiterKeys.Count();
        int k;
        string staleKey;
        for (k = 0; k < removed; k = k + 1)
        {
            staleKey = m_StaleRateLimiterKeys[k];
            m_RateByPlayer.Remove(staleKey);
            m_RateWindowStart.Remove(staleKey);
            m_RateOpsInWindow.Remove(staleKey);
        }
		int warnRemoved = LFPG_Util.PurgeStaleWarnRateLimits(GetGame().GetTickTime(), RATE_LIMITER_STALE_SEC);
		if (removed > 0 || warnRemoved > 0)
        {
			string purgeMsg = "[RateLimiter] Purged cooldown=" + removed.ToString() + " warn=" + warnRemoved.ToString() + " stale entries";
            LFPG_Util.Info(purgeMsg);
        }
        int pruned = LFPG_DeviceRegistry.Get().PruneNullEntries();
        if (pruned > 0)
        {
            string pruneMsg = "[PurgeStale] DeviceRegistry pruned " + pruned.ToString() + " null entries";
            LFPG_Util.Info(pruneMsg);
        }
        #endif
    }
    override bool IsPortLocked(string lockKey)
    {
        bool locked = false;
        if (m_PortLocks.Find(lockKey, locked))
        {
            return locked;
        }
        return false;
    }
    override void LockPort(string lockKey)
    {
        m_PortLocks.Set(lockKey, true);
    }
    override void UnlockPort(string lockKey)
    {
        m_PortLocks.Remove(lockKey);
    }
    override bool IsStartupValidationDone()
    {
        return m_StartupValidationDone;
    }
    override bool IsValidationActive()
    {
        return m_ValidationActive;
    }
    override bool AddVanillaWire(string ownerDeviceId, LFPG_WireData wd)
    {
        if (ownerDeviceId == "" || !wd)
            return false;
        if (wd.m_SourcePort == "")
            wd.m_SourcePort = "output_1";
        ref array<ref LFPG_WireData> wires;
        if (!m_VanillaWires.Find(ownerDeviceId, wires) || !wires)
        {
            wires = new array<ref LFPG_WireData>;
            m_VanillaWires[ownerDeviceId] = wires;
        }
        LFPG_ServerSettings st = LFPG_Settings.Get();
        int maxWires = LFPG_MAX_WIRES_PER_DEVICE;
        if (st && st.MaxWiresPerDevice > 0)
        {
            maxWires = st.MaxWiresPerDevice;
        }
        if (wires.Count() >= maxWires)
            return false;
        int i;
        for (i = 0; i < wires.Count(); i = i + 1)
        {
            LFPG_WireData e = wires[i];
            if (!e) continue;
            if (e.m_TargetDeviceId == wd.m_TargetDeviceId && e.m_TargetPort == wd.m_TargetPort && e.m_SourcePort == wd.m_SourcePort)
                return false;
        }
        wires.Insert(wd);
        ReverseIdxAdd(wd.m_TargetDeviceId, wd.m_TargetPort, ownerDeviceId);
        PlayerWireCountAdd(wd.m_CreatorId, 1);
        MarkVanillaDirty();
        return true;
    }
    override array<ref LFPG_WireData> GetVanillaWires(string ownerDeviceId)
    {
        ref array<ref LFPG_WireData> wires;
        if (m_VanillaWires.Find(ownerDeviceId, wires))
            return wires;
        return null;
    }
    override array<ref LFPG_WireData> GetWiresForDevice(string deviceId)
    {
        EntityAI obj = LFPG_DeviceRegistry.Get().FindById(deviceId);
        if (obj)
        {
            if (LFPG_DeviceAPI.HasWireStore(obj))
            {
                return LFPG_DeviceAPI.GetDeviceWires(obj);
            }
        }
        return GetVanillaWires(deviceId);
    }
    override int GetVanillaWireOwnerCount()
    {
        return m_VanillaWires.Count();
    }
    override string GetVanillaWireOwnerKey(int idx)
    {
        if (idx < 0 || idx >= m_VanillaWires.Count())
            return "";
        return m_VanillaWires.GetKey(idx);
    }
    override bool CheckCycleBeforeWire(string sourceId, string targetId)
    {
        #ifdef SERVER
        if (!m_Graph)
            return false;
        return m_Graph.DetectCycleIfAdded(sourceId, targetId);
        #else
        return false;
        #endif
    }
    override bool CheckComponentSizeBeforeWire(string sourceId, string targetId)
    {
        #ifdef SERVER
        if (!m_Graph)
            return false;
        return m_Graph.WouldExceedComponentLimit(sourceId, targetId);
        #else
        return false;
        #endif
    }
    override bool NotifyGraphWireAdded(string sourceId, string targetId, string sourcePort, string targetPort, LFPG_WireData wireRef)
    {
        #ifdef SERVER
        if (!m_Graph)
            return false;
        bool inserted = m_Graph.OnWireAdded(sourceId, targetId, sourcePort, targetPort, wireRef);
        if (!inserted)
            m_GraphFullRebuildRequired = true;
        if (inserted)
        {
            TrackDeviceForPolling(sourceId);
            TrackDeviceForPolling(targetId);
        }
        string noRemoved = "";
        LFPG_RefreshPumpSprinklerLink(sourceId, noRemoved);
        return inserted;
        #else
        return false;
        #endif
    }
    override void NotifyGraphWireRemoved(string sourceId, string targetId, string sourcePort, string targetPort)
    {
        #ifdef SERVER
        if (!m_Graph)
            return;
        string creditTargetPort = targetPort;
        if (creditTargetPort == "")
            creditTargetPort = "input_main";
        int explicitMatchCount = 0;
        ref array<ref LFPG_ElecEdge> explicitOutEdges = m_Graph.GetOutgoing(sourceId);
        if (explicitOutEdges)
        {
            int explicitEdgeIndex;
            for (explicitEdgeIndex = 0; explicitEdgeIndex < explicitOutEdges.Count(); explicitEdgeIndex = explicitEdgeIndex + 1)
            {
                LFPG_ElecEdge explicitEdge = explicitOutEdges[explicitEdgeIndex];
                if (!explicitEdge || explicitEdge.m_TargetNodeId != targetId || explicitEdge.m_SourcePort != sourcePort)
                    continue;
                string explicitTargetPort = explicitEdge.m_TargetPort;
                if (explicitTargetPort == "")
                    explicitTargetPort = "input_main";
                if (explicitTargetPort == creditTargetPort)
                    explicitMatchCount = explicitMatchCount + 1;
            }
        }
        if (explicitMatchCount != 1)
        {
            m_GraphFullRebuildRequired = true;
            RequestGlobalSelfHeal();
        }
        m_ExplicitGraphRemovalCredit = sourceId + "|" + targetId + "|" + creditTargetPort;
        m_Graph.OnWireRemoved(sourceId, targetId, sourcePort, targetPort);
        LFPG_RefreshPumpSprinklerLink(sourceId, targetId);
        #endif
    }
    override void BeginGraphMutation()
    {
        #ifdef SERVER
        if (!m_Graph)
            return;
        m_Graph.BeginGraphMutation();
        #endif
    }
    override void EndGraphMutation()
    {
        #ifdef SERVER
        if (!m_Graph)
            return;
        m_Graph.EndGraphMutation();
        #endif
    }
	protected void CaptureBatteryGraphState()
	{
		#ifdef SERVER
		m_BatteryRebuildGeneration.Clear();
		m_BatteryRebuildDemand.Clear();
		for (int batteryIndex = 0; batteryIndex < m_RegisteredBatteries.Count(); batteryIndex = batteryIndex + 1)
		{
			EntityAI battery = m_RegisteredBatteries[batteryIndex];
			if (!battery)
				continue;
			string batteryId = LFPG_DeviceAPI.GetDeviceId(battery);
			LFPG_ElecNode oldNode = m_Graph.GetNode(batteryId);
			if (!oldNode)
				continue;
			m_BatteryRebuildGeneration.Set(batteryId, oldNode.m_VirtualGeneration);
			m_BatteryRebuildDemand.Set(batteryId, oldNode.m_SoftDemand);
		}
		#endif
	}
	protected void RestoreBatteryGraphState()
	{
		#ifdef SERVER
		for (int stateIndex = 0; stateIndex < m_BatteryRebuildGeneration.Count(); stateIndex = stateIndex + 1)
		{
			string batteryId = m_BatteryRebuildGeneration.GetKey(stateIndex);
			LFPG_ElecNode newNode = m_Graph.GetNode(batteryId);
			if (!newNode)
				continue;
			newNode.m_VirtualGeneration = m_BatteryRebuildGeneration.GetElement(stateIndex);
			newNode.m_SoftDemand = m_BatteryRebuildDemand.Get(batteryId);
			m_Graph.MarkNodeDirty(batteryId, LFPG_DIRTY_INPUT);
		}
		m_BatteryRebuildGeneration.Clear();
		m_BatteryRebuildDemand.Clear();
		#endif
	}
    override void PostBulkRebuildAndPropagate()
    {
        #ifdef SERVER
        bool cutGraphRebuild = m_CutGraphRebuildQueued;
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(PostBulkRebuildAndPropagate);
        m_CutGraphRebuildQueued = false;
        if (cutGraphRebuild)
        {
            int pendingPowerIndex;
            for (pendingPowerIndex = 0; pendingPowerIndex < m_CutPendingPowerOff.Count(); pendingPowerIndex = pendingPowerIndex + 1)
            {
                string pendingPowerId = m_CutPendingPowerOff.GetKey(pendingPowerIndex);
                EntityAI pendingPowerDevice = LFPG_DeviceRegistry.Get().FindById(pendingPowerId);
                if (!pendingPowerDevice)
                    pendingPowerDevice = LFPG_DeviceAPI.ResolveVanillaDevice(pendingPowerId);
                if (pendingPowerDevice)
                    LFPG_DeviceAPI.SetPowered(pendingPowerDevice, false);
            }
            m_CutPendingPowerOff.Clear();
        }
        if (!m_Graph)
        {
            if (cutGraphRebuild)
                FlushBroadcasts();
            m_ValidationAfterCutRequested = false;
            if (m_IndexHealAfterCut)
            {
                m_IndexHealAfterCut = false;
                RequestGlobalSelfHeal();
            }
            return;
        }
        if (cutGraphRebuild || m_GraphFullRebuildRequired)
        {
			CaptureBatteryGraphState();
            m_Graph.PostBulkRebuild(this);
			RestoreBatteryGraphState();
            m_GraphFullRebuildRequired = false;
            m_GraphRebuildGeneration = m_GraphRebuildGeneration + 1;
        }
        m_WarmupActive = true;
        int flushBudget = LFPG_PROPAGATE_WARMUP_BUDGET;
        int flushEdge = LFPG_PROPAGATE_EDGE_WARMUP_BUDGET;
        m_Graph.ProcessDirtyQueue(flushBudget, flushEdge);
        if (cutGraphRebuild)
            FlushBroadcasts();
        if (cutGraphRebuild && m_ValidationAfterCutRequested)
        {
            m_ValidationAfterCutRequested = false;
            m_ValidationOnlyHealPending = true;
            RequestGlobalSelfHeal();
        }
        if (m_IndexHealAfterCut)
        {
            m_IndexHealAfterCut = false;
            m_ValidationOnlyHealPending = true;
            RequestGlobalSelfHeal();
        }
        #endif
    }
    override void NotifyGraphDeviceRemoved(string deviceId)
    {
        #ifdef SERVER
        if (!m_Graph)
            return;
        m_Graph.OnDeviceRemoved(deviceId);
        m_LastKnownPos.Remove(deviceId);
        UntrackDeviceFromPolling(deviceId);
        #endif
    }
    override LFPG_ElecGraph GetGraph()
    {
        return m_Graph;
    }
    override bool IsPortReceivingPower(string deviceId, string portName)
    {
        if (!m_Graph)
            return false;
        return m_Graph.IsPortReceivingPower(deviceId, portName);
    }
    override void RebuildReverseIdx()
    {
        #ifdef SERVER
        m_GraphFullRebuildRequired = true;
        m_ReverseIndexTrusted = false;
        m_ReverseIdx.Clear();
        m_ReverseOwners.Clear();
        array<EntityAI> all = new array<EntityAI>;
        LFPG_DeviceRegistry.Get().GetAll(all);
        int i;
        for (i = 0; i < all.Count(); i = i + 1)
        {
            if (!LFPG_DeviceAPI.HasWireStore(all[i])) continue;
            string ownerId = LFPG_DeviceAPI.GetDeviceId(all[i]);
            ref array<ref LFPG_WireData> gWires = LFPG_DeviceAPI.GetDeviceWires(all[i]);
            if (!gWires) continue;
            int gw;
            for (gw = 0; gw < gWires.Count(); gw = gw + 1)
            {
                LFPG_WireData wd = gWires[gw];
                if (!wd) continue;
                string tPort = wd.m_TargetPort;
                if (tPort == "")
                {
                    tPort = "input_main";
                }
                string rKey = wd.m_TargetDeviceId + "|" + tPort;
                int prev = 0;
                m_ReverseIdx.Find(rKey, prev);
                m_ReverseIdx[rKey] = prev + 1;
                ReverseOwnersInsert(rKey, ownerId);
            }
        }
        int vk;
        for (vk = 0; vk < m_VanillaWires.Count(); vk = vk + 1)
        {
            string vOwnerId = m_VanillaWires.GetKey(vk);
            ref array<ref LFPG_WireData> vWires = m_VanillaWires.GetElement(vk);
            if (!vWires) continue;
            int vw;
            for (vw = 0; vw < vWires.Count(); vw = vw + 1)
            {
                LFPG_WireData vwd = vWires[vw];
                if (!vwd) continue;
                string vtPort = vwd.m_TargetPort;
                if (vtPort == "")
                {
                    vtPort = "input_main";
                }
                string vrKey = vwd.m_TargetDeviceId + "|" + vtPort;
                int vprev = 0;
                m_ReverseIdx.Find(vrKey, vprev);
                m_ReverseIdx[vrKey] = vprev + 1;
                ReverseOwnersInsert(vrKey, vOwnerId);
            }
        }
        m_ReverseIndexTrusted = true;
        #endif
    }
    protected void ReverseOwnersInsert(string rKey, string ownerDeviceId)
    {
        ref array<string> owners;
        if (!m_ReverseOwners.Find(rKey, owners) || !owners)
        {
            owners = new array<string>;
            m_ReverseOwners[rKey] = owners;
        }
        int oi;
        for (oi = 0; oi < owners.Count(); oi = oi + 1)
        {
            if (owners[oi] == ownerDeviceId)
                return;
        }
        owners.Insert(ownerDeviceId);
    }
    override int CountWiresTargeting(string targetDeviceId, string targetPort)
    {
        #ifdef SERVER
        if (targetPort == "")
        {
            targetPort = "input_main";
        }
        string rKey = targetDeviceId + "|" + targetPort;
        int count = 0;
        m_ReverseIdx.Find(rKey, count);
        return count;
        #else
        return 0;
        #endif
    }
    override bool IsPortTargetedByPoweredSource(string targetDeviceId, string targetPort)
    {
        #ifdef SERVER
        if (targetDeviceId == "" || targetPort == "")
            return false;
        string rKey = targetDeviceId + "|" + targetPort;
        int count = 0;
        m_ReverseIdx.Find(rKey, count);
        if (count <= 0)
            return false;
        ref array<string> owners;
        if (!m_ReverseOwners.Find(rKey, owners))
            return false;
        if (!owners)
            return false;
        int i;
        for (i = 0; i < owners.Count(); i = i + 1)
        {
            string ownerId = owners[i];
            if (ownerId == "")
                continue;
            EntityAI srcEntity = LFPG_DeviceRegistry.Get().FindById(ownerId);
            if (!srcEntity)
            {
                srcEntity = LFPG_DeviceAPI.ResolveVanillaDevice(ownerId);
            }
            if (!srcEntity)
                continue;
			if (LFPG_DeviceAPI.HasWireStore(srcEntity))
				m_WireQueryStore = LFPG_DeviceAPI.GetDeviceWires(srcEntity);
			else
				m_WireQueryStore = GetVanillaWires(ownerId);
			bool stillTargetsPort = false;
			if (m_WireQueryStore)
			{
				for (int wireIndex = 0; wireIndex < m_WireQueryStore.Count(); wireIndex = wireIndex + 1)
				{
					LFPG_WireData currentWire = m_WireQueryStore[wireIndex];
					if (!currentWire || currentWire.m_TargetDeviceId != targetDeviceId)
						continue;
					string currentPort = currentWire.m_TargetPort;
					if (currentPort == "")
						currentPort = "input_main";
					if (currentPort == targetPort)
					{
						stillTargetsPort = true;
						break;
					}
				}
			}
			m_WireQueryStore = null;
			if (!stillTargetsPort)
				continue;
            bool srcOn = LFPG_DeviceAPI.GetSourceOn(srcEntity);
            if (srcOn)
                return true;
        }
        #endif
        return false;
    }
    override void ReverseIdxAdd(string targetDeviceId, string targetPort, string ownerDeviceId = "")
    {
        #ifdef SERVER
        if (targetPort == "")
        {
            targetPort = "input_main";
        }
        string rKey = targetDeviceId + "|" + targetPort;
        int prev = 0;
        m_ReverseIdx.Find(rKey, prev);
        m_ReverseIdx[rKey] = prev + 1;
        if (ownerDeviceId != "")
        {
            ref array<string> owners;
            if (!m_ReverseOwners.Find(rKey, owners) || !owners)
            {
                owners = new array<string>;
                m_ReverseOwners[rKey] = owners;
            }
            bool found = false;
            int oi;
            for (oi = 0; oi < owners.Count(); oi = oi + 1)
            {
                if (owners[oi] == ownerDeviceId)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                owners.Insert(ownerDeviceId);
            }
        }
        #endif
    }
    override void ReverseIdxRemove(string targetDeviceId, string targetPort, string ownerDeviceId = "")
    {
        #ifdef SERVER
        if (targetPort == "")
        {
            targetPort = "input_main";
        }
        string explicitRemovalKey = ownerDeviceId + "|" + targetDeviceId + "|" + targetPort;
        bool graphAlreadyNotified = false;
        if (m_ExplicitGraphRemovalCredit != "")
        {
            if (m_ExplicitGraphRemovalCredit == explicitRemovalKey)
                graphAlreadyNotified = true;
            m_ExplicitGraphRemovalCredit = "";
        }
        if (m_Graph && ownerDeviceId != "" && !graphAlreadyNotified && !m_CutAllGraphBatchActive)
        {
            ref array<ref LFPG_ElecEdge> reverseOutEdges = m_Graph.GetOutgoing(ownerDeviceId);
            int reverseMatchCount = 0;
            string reverseSourcePort = "";
            string reverseGraphTargetPort = "";
            if (reverseOutEdges)
            {
                int reverseEdgeIndex;
                for (reverseEdgeIndex = 0; reverseEdgeIndex < reverseOutEdges.Count(); reverseEdgeIndex = reverseEdgeIndex + 1)
                {
                    LFPG_ElecEdge reverseEdge = reverseOutEdges[reverseEdgeIndex];
                    if (!reverseEdge || reverseEdge.m_TargetNodeId != targetDeviceId)
                        continue;
                    string reverseTargetPort = reverseEdge.m_TargetPort;
                    if (reverseTargetPort == "")
                        reverseTargetPort = "input_main";
                    if (reverseTargetPort != targetPort)
                        continue;
                    reverseMatchCount = reverseMatchCount + 1;
                    reverseSourcePort = reverseEdge.m_SourcePort;
                    reverseGraphTargetPort = reverseEdge.m_TargetPort;
                }
            }
            if (reverseMatchCount == 1)
            {
                m_Graph.OnWireRemoved(ownerDeviceId, targetDeviceId, reverseSourcePort, reverseGraphTargetPort);
                LFPG_RefreshPumpSprinklerLink(ownerDeviceId, targetDeviceId);
            }
            else if (reverseMatchCount > 1)
                m_GraphFullRebuildRequired = true;
        }
        string rKey = targetDeviceId + "|" + targetPort;
        int prev = 0;
        if (m_ReverseIdx.Find(rKey, prev))
        {
            if (prev <= 0)
                m_ReverseIndexTrusted = false;
            if (prev <= 1)
            {
                m_ReverseIdx.Remove(rKey);
                m_ReverseOwners.Remove(rKey);
            }
            else
            {
                m_ReverseIdx[rKey] = prev - 1;
            }
        }
        else
        {
            m_ReverseIndexTrusted = false;
        }
        #endif
    }
    override void PlayerWireCountAdd(string creatorId, int delta)
    {
        #ifdef SERVER
        if (creatorId == "") return;
        int prev = 0;
        m_WiresByPlayer.Find(creatorId, prev);
        int next = prev + delta;
        if (next < 0)
        {
            next = 0;
        }
        m_WiresByPlayer[creatorId] = next;
        #endif
    }
    protected void RecountAllPlayerWires()
    {
        #ifdef SERVER
        m_WiresByPlayer.Clear();
        array<EntityAI> all = new array<EntityAI>;
        LFPG_DeviceRegistry.Get().GetAll(all);
        int i;
        for (i = 0; i < all.Count(); i = i + 1)
        {
            if (!LFPG_DeviceAPI.HasWireStore(all[i])) continue;
            ref array<ref LFPG_WireData> wires = LFPG_DeviceAPI.GetDeviceWires(all[i]);
            if (!wires) continue;
            int w;
            for (w = 0; w < wires.Count(); w = w + 1)
            {
                LFPG_WireData wd = wires[w];
                if (!wd || wd.m_CreatorId == "") continue;
                PlayerWireCountAdd(wd.m_CreatorId, 1);
            }
        }
        int vk;
        for (vk = 0; vk < m_VanillaWires.Count(); vk = vk + 1)
        {
            ref array<ref LFPG_WireData> vWires = m_VanillaWires.GetElement(vk);
            if (!vWires) continue;
            int vw;
            for (vw = 0; vw < vWires.Count(); vw = vw + 1)
            {
                LFPG_WireData vwd = vWires[vw];
                if (!vwd || vwd.m_CreatorId == "") continue;
                PlayerWireCountAdd(vwd.m_CreatorId, 1);
            }
        }
        #endif
    }
    override int RemoveWiresTargeting(string targetDeviceId, string targetPort, string creatorId = "", bool allowOthers = true)
    {
        #ifdef SERVER
        int removed = 0;
        bool filterByCreator = !allowOthers;
        if (filterByCreator && creatorId == "")
            return 0;
        string normPort = targetPort;
        if (normPort == "")
        {
            normPort = "input_main";
        }
        string rKey = targetDeviceId + "|" + normPort;
        ref array<string> owners;
        if (!m_ReverseOwners.Find(rKey, owners) || !owners || owners.Count() == 0)
        {
            return 0;
        }
        ref array<string> ownersCopy = new array<string>;
        int oc;
        for (oc = 0; oc < owners.Count(); oc = oc + 1)
        {
            ownersCopy.Insert(owners[oc]);
        }
        int oi;
        for (oi = 0; oi < ownersCopy.Count(); oi = oi + 1)
        {
            string ownerId = ownersCopy[oi];
            EntityAI ownerObj = LFPG_DeviceRegistry.Get().FindById(ownerId);
            if (ownerObj && LFPG_DeviceAPI.HasWireStore(ownerObj))
            {
                ref array<ref LFPG_WireData> gWires = LFPG_DeviceAPI.GetDeviceWires(ownerObj);
                if (gWires)
                {
                    bool ownerChanged = false;
                    ref array<int> ownerDeltaOps = new array<int>;
                    ref array<ref LFPG_WireData> ownerDeltaWires = new array<ref LFPG_WireData>;
                    int gw = gWires.Count() - 1;
                    while (gw >= 0)
                    {
                        LFPG_WireData wd = gWires[gw];
                        if (wd && wd.m_TargetDeviceId == targetDeviceId && wd.m_TargetPort == targetPort && (!filterByCreator || LFPG_WireHelper.CanCreatorCutWire(wd, creatorId, allowOthers)))
                        {
                            if (filterByCreator)
                            {
                                NotifyGraphWireRemoved(ownerId, targetDeviceId, wd.m_SourcePort, targetPort);
                                ReverseIdxRemove(targetDeviceId, targetPort, ownerId);
                            }
                            else if (m_Graph && !m_CutAllGraphBatchActive)
                            {
                                m_Graph.OnWireRemoved(ownerId, targetDeviceId, wd.m_SourcePort, targetPort);
                            }
                            PlayerWireCountAdd(wd.m_CreatorId, -1);
                            ownerDeltaOps.Insert(LFPG_WireDeltaOp.REMOVE);
                            ownerDeltaWires.Insert(wd);
                            gWires.Remove(gw);
                            removed = removed + 1;
                            ownerChanged = true;
                            if (LFPG_LOG_LEVEL >= 1)
                            {
                                string prMsg = "[PortReplace] Removed wire from " + ownerId + " -> " + targetDeviceId + ":" + normPort;
                                LFPG_Util.Info(prMsg);
                            }
                        }
                        gw = gw - 1;
                    }
                    if (ownerChanged)
                    {
                        LFPG_WireOwnerBase wireOwner = LFPG_WireOwnerBase.Cast(ownerObj);
                        if (wireOwner)
                        {
                            wireOwner.LFPG_CommitWireMutation();
                            if (m_CutAllGraphBatchActive)
                                QueueBroadcastOwnerSnapshotFromWires(ownerObj, ownerDeltaWires);
                            else
                                BroadcastOwnerWireDelta(ownerObj, ownerDeltaOps, ownerDeltaWires);
                        }
                        else
                        {
                            ownerObj.SetSynchDirty();
                            QueueBroadcastOwnerSnapshotFromWires(ownerObj, ownerDeltaWires);
                        }
                        RequestPropagate(ownerId);
                    }
                }
                continue;
            }
            ref array<ref LFPG_WireData> vWires;
            if (m_VanillaWires.Find(ownerId, vWires) && vWires)
            {
                bool vChanged = false;
                int vw = vWires.Count() - 1;
                while (vw >= 0)
                {
                    LFPG_WireData vwd = vWires[vw];
                    if (vwd && vwd.m_TargetDeviceId == targetDeviceId && vwd.m_TargetPort == targetPort && (!filterByCreator || LFPG_WireHelper.CanCreatorCutWire(vwd, creatorId, allowOthers)))
                    {
                        string vSrcP = vwd.m_SourcePort;
                        if (vSrcP == "")
                        {
                            vSrcP = "output_1";
                        }
                        if (filterByCreator)
                        {
                            NotifyGraphWireRemoved(ownerId, targetDeviceId, vSrcP, targetPort);
                            ReverseIdxRemove(targetDeviceId, targetPort, ownerId);
                        }
                        else if (m_Graph && !m_CutAllGraphBatchActive)
                        {
                            m_Graph.OnWireRemoved(ownerId, targetDeviceId, vSrcP, targetPort);
                        }
                        PlayerWireCountAdd(vwd.m_CreatorId, -1);
                        vWires.Remove(vw);
                        removed = removed + 1;
                        vChanged = true;
                        if (LFPG_LOG_LEVEL >= 1)
                        {
                            string prVMsg = "[PortReplace] Removed vanilla wire from " + ownerId + " -> " + targetDeviceId + ":" + normPort;
                            LFPG_Util.Info(prVMsg);
                        }
                    }
                    vw = vw - 1;
                }
                if (vChanged)
                {
                    EntityAI vObj = LFPG_DeviceRegistry.Get().FindById(ownerId);
                    if (vObj)
                    {
                        QueueBroadcastVanilla(ownerId, vObj);
                    }
                    RequestPropagate(ownerId);
                }
            }
        }
        if (removed > 0)
        {
            if (!filterByCreator)
            {
                m_ReverseIdx.Remove(rKey);
                m_ReverseOwners.Remove(rKey);
            }
            MarkVanillaDirty();
            if (!m_CutAllGraphBatchActive)
                FlushBroadcasts();
        }
        return removed;
        #else
        return 0;
        #endif
    }
    override bool CanPlayerCreateAnotherWire(PlayerIdentity ident, out string reason)
    {
        reason = "";
        #ifdef SERVER
        if (!ident)
        {
            reason = "no identity";
            return false;
        }
        LFPG_ServerSettings st = LFPG_Settings.Get();
        int limit = LFPG_MAX_WIRES_PER_PLAYER;
        if (st && st.MaxWiresPerPlayer > 0)
        {
            limit = st.MaxWiresPerPlayer;
        }
        if (limit <= 0)
            return true;
        string pid = ident.GetPlainId();
        int count = 0;
        m_WiresByPlayer.Find(pid, count);
        if (count >= limit)
        {
            reason = "MaxWiresPerPlayer reached (" + count.ToString() + "/" + limit.ToString() + ")";
            return false;
        }
        #endif
        return true;
    }
    override bool ValidateWire(vector startPos, vector endPos, array<vector> waypoints, out string reason)
    {
        reason = "";
        int wpCount = 0;
        if (waypoints)
        {
            wpCount = waypoints.Count();
        }
        if (wpCount > LFPG_MAX_WAYPOINTS)
        {
            reason = "Too many waypoints";
            return false;
        }
        vector prev = startPos;
        float total = 0.0;
        int i;
        for (i = 0; i < wpCount; i = i + 1)
        {
            float seg = vector.Distance(prev, waypoints[i]);
            if (seg > LFPG_MAX_SEGMENT_LEN_M)
            {
                reason = "Segment too long";
                return false;
            }
            total = total + seg;
            prev = waypoints[i];
        }
        float lastSeg = vector.Distance(prev, endPos);
        if (lastSeg > LFPG_MAX_SEGMENT_LEN_M)
        {
            reason = "Last segment too long";
            return false;
        }
        total = total + lastSeg;
        if (total > LFPG_MAX_WIRE_LEN_M)
        {
            reason = "Wire too long total";
            return false;
        }
        return true;
    }
    override void QueueBroadcastOwner(EntityAI owner)
    {
        if (!owner) return;
        string devId = LFPG_DeviceAPI.GetDeviceId(owner);
        if (devId == "") return;
        if (CoalescePendingOwnerSnapshot(owner, null))
            return;
        m_PendingOwnerSnapshots.Remove(devId);
        m_PendingBroadcastLFPG[devId] = owner;
    }
    protected bool AppendOwnerSnapshotPosition(array<vector> positions, vector position)
    {
        if (!positions)
            return false;
        if (positions.Find(position) >= 0)
            return true;
        if (positions.Count() >= LFPG_OWNER_SNAPSHOT_MAX_INTEREST_POSITIONS)
            return false;
        positions.Insert(position);
        return true;
    }
    protected void StorePendingOwnerSnapshot(LFPG_OwnerBroadcastSnapshot snapshot)
    {
        if (!snapshot || snapshot.m_OwnerDeviceId == "")
            return;
        array<vector> combinedPositions = new array<vector>;
        LFPG_OwnerBroadcastSnapshot previousSnapshot;
        LFPG_OwnerBroadcastSnapshot mergedSnapshot;
        int snapshotIndex;
        int previousIndex;
        bool broadcastAll = snapshot.m_BroadcastAll;
        if (m_CutAllGraphBatchActive && m_CutAllHasPreviousOwnerPosition)
        {
            if (!AppendOwnerSnapshotPosition(combinedPositions, m_CutAllPreviousOwnerPosition))
                broadcastAll = true;
        }
        for (snapshotIndex = 0; snapshotIndex < snapshot.m_TargetPositions.Count() && !broadcastAll; snapshotIndex = snapshotIndex + 1)
        {
            if (!AppendOwnerSnapshotPosition(combinedPositions, snapshot.m_TargetPositions[snapshotIndex]))
                broadcastAll = true;
        }
        Managed previousPendingSnapshotRaw;
        if (m_PendingOwnerSnapshots.Find(snapshot.m_OwnerDeviceId, previousPendingSnapshotRaw) && Class.CastTo(previousSnapshot, previousPendingSnapshotRaw) && previousSnapshot)
        {
            if (previousSnapshot.m_BroadcastAll)
                broadcastAll = true;
            if (!broadcastAll && !AppendOwnerSnapshotPosition(combinedPositions, previousSnapshot.m_OwnerPosition))
                broadcastAll = true;
            for (previousIndex = 0; previousIndex < previousSnapshot.m_TargetPositions.Count() && !broadcastAll; previousIndex = previousIndex + 1)
            {
                if (!AppendOwnerSnapshotPosition(combinedPositions, previousSnapshot.m_TargetPositions[previousIndex]))
                    broadcastAll = true;
            }
        }
        if (broadcastAll)
            combinedPositions.Clear();
        mergedSnapshot = new LFPG_OwnerBroadcastSnapshot(snapshot.m_OwnerDeviceId, snapshot.m_OwnerLow, snapshot.m_OwnerHigh, snapshot.m_JSON, snapshot.m_Generation, snapshot.m_OwnerPosition, combinedPositions);
        mergedSnapshot.m_BroadcastAll = broadcastAll;
        m_PendingBroadcastLFPG.Remove(snapshot.m_OwnerDeviceId);
        m_PendingOwnerSnapshots[snapshot.m_OwnerDeviceId] = mergedSnapshot;
    }
    override void QueueBroadcastOwnerSnapshot(EntityAI owner, array<vector> targetPositions, bool broadcastAll)
    {
        if (!owner)
            return;
        string ownerId = LFPG_DeviceAPI.GetDeviceId(owner);
        if (ownerId == "")
            return;
        int ownerLow = 0;
        int ownerHigh = 0;
        string json = LFPG_DeviceAPI.GetWiresJSON(owner);
        int generation = -1;
        vector ownerPosition = owner.GetPosition();
        LFPG_WireOwnerBase wireOwner = LFPG_WireOwnerBase.Cast(owner);
        LFPG_OwnerBroadcastSnapshot snapshot;
        owner.GetNetworkID(ownerLow, ownerHigh);
        if (wireOwner)
            generation = wireOwner.LFPG_GetWireGeneration();
        snapshot = new LFPG_OwnerBroadcastSnapshot(ownerId, ownerLow, ownerHigh, json, generation, ownerPosition, targetPositions);
        snapshot.m_BroadcastAll = broadcastAll;
        StorePendingOwnerSnapshot(snapshot);
    }
    protected void QueueBroadcastOwnerSnapshotFromWires(EntityAI owner, array<ref LFPG_WireData> interestWires)
    {
        LFPG_OwnerBroadcastSnapshot snapshot;
        snapshot = CaptureOwnerBroadcastSnapshot(owner, interestWires);
        if (snapshot)
            StorePendingOwnerSnapshot(snapshot);
    }
    protected bool CoalescePendingOwnerSnapshot(EntityAI owner, array<ref LFPG_WireData> extraInterestWires)
    {
        if (!owner)
            return false;
        string ownerId = LFPG_DeviceAPI.GetDeviceId(owner);
        if (ownerId == "")
            return false;
        if (!m_PendingOwnerSnapshots.Contains(ownerId))
            return false;
        LFPG_OwnerBroadcastSnapshot snapshot;
        snapshot = CaptureOwnerBroadcastSnapshot(owner, extraInterestWires);
        if (snapshot)
            StorePendingOwnerSnapshot(snapshot);
        return true;
    }
    override void QueueBroadcastVanilla(string ownerDeviceId, EntityAI ownerObj)
    {
        if (ownerDeviceId == "" || !ownerObj) return;
        m_PendingBroadcastVanilla[ownerDeviceId] = ownerObj;
    }
    override void FlushBroadcasts()
    {
        int i;
        for (i = 0; i < m_PendingBroadcastLFPG.Count(); i = i + 1)
        {
            EntityAI owner = EntityAI.Cast(m_PendingBroadcastLFPG.GetElement(i));
            if (owner)
            {
                BroadcastOwnerWires(owner);
            }
        }
        m_PendingBroadcastLFPG.Clear();
        int snapshotIndex;
        for (snapshotIndex = 0; snapshotIndex < m_PendingOwnerSnapshots.Count(); snapshotIndex = snapshotIndex + 1)
        {
            LFPG_OwnerBroadcastSnapshot snapshot = LFPG_OwnerBroadcastSnapshot.Cast(m_PendingOwnerSnapshots.GetElement(snapshotIndex));
            if (snapshot)
                BroadcastOwnerSnapshot(snapshot);
        }
        m_PendingOwnerSnapshots.Clear();
        int v;
        for (v = 0; v < m_PendingBroadcastVanilla.Count(); v = v + 1)
        {
            string vId = m_PendingBroadcastVanilla.GetKey(v);
            EntityAI vObj = EntityAI.Cast(m_PendingBroadcastVanilla.GetElement(v));
            if (vObj)
            {
                BroadcastVanillaWires(vId, vObj);
            }
        }
        m_PendingBroadcastVanilla.Clear();
    }
	protected bool CanReceiveWireBroadcast(PlayerBase player)
	{
		if (!player || !player.GetIdentity())
			return false;
		if (m_FullSyncReplayActive)
			return player == m_FullSyncPlayer;
		if (m_FullSyncInProgress && player == m_FullSyncPlayer)
			return false;
		if (m_FullSyncPendingPlayers && m_FullSyncPendingPlayers.Find(player) >= 0)
			return false;
		return true;
	}
	protected bool SelectWireBroadcastRecipients(vector ownerPosition)
	{
		m_ReusableBroadcastPlayers.Clear();
		g_Game.GetPlayers(m_ReusableBroadcastPlayers);
		float maxDistance = LFPG_CULL_DISTANCE_M + 20.0;
		float maxDistanceSq = maxDistance * maxDistance;
		for (int playerIndex = m_ReusableBroadcastPlayers.Count() - 1; playerIndex >= 0; playerIndex = playerIndex - 1)
		{
			PlayerBase player = PlayerBase.Cast(m_ReusableBroadcastPlayers[playerIndex]);
			bool inRange = false;
			if (CanReceiveWireBroadcast(player))
			{
				vector playerPosition = player.GetPosition();
				inRange = LFPG_WorldUtil.DistSq(playerPosition, ownerPosition) <= maxDistanceSq;
				for (int targetIndex = 0; targetIndex < m_ReusableBroadcastPositions.Count() && !inRange; targetIndex = targetIndex + 1)
					inRange = LFPG_WorldUtil.DistSq(playerPosition, m_ReusableBroadcastPositions[targetIndex]) <= maxDistanceSq;
			}
			if (!inRange)
				m_ReusableBroadcastPlayers.Remove(playerIndex);
		}
		return m_ReusableBroadcastPlayers.Count() > 0;
	}
    override void BroadcastOwnerWires(EntityAI owner)
    {
        if (!owner) return;
        string ownerId = LFPG_DeviceAPI.GetDeviceId(owner);
        if (ownerId == "") return;
        if (CoalescePendingOwnerSnapshot(owner, null))
            return;
        m_PendingOwnerSnapshots.Remove(ownerId);
		if (m_FullSyncInProgress && m_FullSyncPlayer)
			DeferOwnerSnapshot(owner, null);
        m_ReusableBroadcastPositions.Clear();
        ref array<ref LFPG_WireData> preOwnerWires = LFPG_DeviceAPI.GetDeviceWires(owner);
        if (preOwnerWires)
        {
            LFPG_DeviceRegistry preReg = LFPG_DeviceRegistry.Get();
            int preTw;
            for (preTw = 0; preTw < preOwnerWires.Count(); preTw = preTw + 1)
            {
                if (!preOwnerWires[preTw]) continue;
                if (preOwnerWires[preTw].m_TargetDeviceId == "") continue;
                EntityAI preTarget = preReg.FindById(preOwnerWires[preTw].m_TargetDeviceId);
                if (preTarget)
                    m_ReusableBroadcastPositions.Insert(preTarget.GetPosition());
            }
        }
		if (!SelectWireBroadcastRecipients(owner.GetPosition()))
			return;
        string json = LFPG_DeviceAPI.GetWiresJSON(owner);
        LFPG_WireOwnerBase snapshotWireOwner = LFPG_WireOwnerBase.Cast(owner);
        int snapshotGeneration = -1;
        if (snapshotWireOwner)
        {
            snapshotGeneration = snapshotWireOwner.LFPG_GetWireGeneration();
        }
        int low = 0;
        int high = 0;
        owner.GetNetworkID(low, high);
        if (LFPG_LOG_LEVEL >= 2)
        {
            string bcastMsg = "[BroadcastOwnerWires] owner=" + ownerId + " net=" + low.ToString() + ":" + high.ToString() + " type=" + owner.GetType() + " jsonLen=" + json.Length().ToString();
            LFPG_Util.Debug(bcastMsg);
        }
        if (json.Length() > 12000)
        {
            string bcastWarn = "[BroadcastOwnerWires] LARGE BLOB owner=" + ownerId + " jsonLen=" + json.Length().ToString() + " — approaching RPC limit";
            LFPG_Util.Warn(bcastWarn);
        }
        int i;
        for (i = 0; i < m_ReusableBroadcastPlayers.Count(); i = i + 1)
        {
            PlayerBase pb = PlayerBase.Cast(m_ReusableBroadcastPlayers[i]);
            if (!pb) continue;
            ScriptRPC rpc = new ScriptRPC();
            rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES_V2);
            rpc.Write(ownerId);
            rpc.Write(low);
            rpc.Write(high);
            rpc.Write(json);
            rpc.Write(snapshotGeneration);
            bool bRpcGuaranteed = true;
			PlayerIdentity recipient = pb.GetIdentity();
			if (!recipient)
				continue;
			rpc.Send(pb, LFPG_RPC_CHANNEL, bRpcGuaranteed, recipient);
        }
    }
    protected EntityAI ResolveOwnerSnapshotTarget(LFPG_WireData wire)
    {
        if (!wire)
            return null;
        EntityAI target = null;
        if (wire.m_TargetDeviceId != "")
            target = LFPG_DeviceRegistry.Get().FindById(wire.m_TargetDeviceId);
        if (!target)
            target = LFPG_DeviceAPI.ResolveByNetworkId(wire.m_TargetNetLow, wire.m_TargetNetHigh);
        return target;
    }
    protected LFPG_OwnerBroadcastSnapshot CaptureOwnerBroadcastSnapshot(EntityAI owner, array<ref LFPG_WireData> extraInterestWires)
    {
        if (!owner)
            return null;
        string ownerId = LFPG_DeviceAPI.GetDeviceId(owner);
        if (ownerId == "")
            return null;
        array<vector> targetPositions = new array<vector>;
        array<ref LFPG_WireData> currentWires = LFPG_DeviceAPI.GetDeviceWires(owner);
        LFPG_WireData interestWire;
        EntityAI interestTarget;
        int currentIndex;
        int extraIndex;
        bool broadcastAll = false;
        if (currentWires)
        {
            for (currentIndex = 0; currentIndex < currentWires.Count(); currentIndex = currentIndex + 1)
            {
                interestWire = currentWires[currentIndex];
                if (!interestWire)
                    continue;
                interestTarget = ResolveOwnerSnapshotTarget(interestWire);
                if (interestTarget)
                    targetPositions.Insert(interestTarget.GetPosition());
                else
                    broadcastAll = true;
            }
        }
        if (extraInterestWires)
        {
            for (extraIndex = 0; extraIndex < extraInterestWires.Count(); extraIndex = extraIndex + 1)
            {
                interestWire = extraInterestWires[extraIndex];
                if (!interestWire)
                    continue;
                interestTarget = ResolveOwnerSnapshotTarget(interestWire);
                if (interestTarget)
                    targetPositions.Insert(interestTarget.GetPosition());
                else
                    broadcastAll = true;
            }
        }
        int ownerLow = 0;
        int ownerHigh = 0;
        string json = LFPG_DeviceAPI.GetWiresJSON(owner);
        int generation = -1;
        vector ownerPosition = owner.GetPosition();
        LFPG_WireOwnerBase wireOwner = LFPG_WireOwnerBase.Cast(owner);
        owner.GetNetworkID(ownerLow, ownerHigh);
        if (wireOwner)
            generation = wireOwner.LFPG_GetWireGeneration();
        LFPG_OwnerBroadcastSnapshot snapshot = new LFPG_OwnerBroadcastSnapshot(ownerId, ownerLow, ownerHigh, json, generation, ownerPosition, targetPositions);
        snapshot.m_BroadcastAll = broadcastAll;
        return snapshot;
    }
    protected void StoreDeferredOwnerSnapshot(LFPG_OwnerBroadcastSnapshot snapshot)
    {
        if (!snapshot || snapshot.m_OwnerDeviceId == "")
            return;
        array<vector> combinedPositions = new array<vector>;
        LFPG_OwnerBroadcastSnapshot previousSnapshot;
        LFPG_OwnerBroadcastSnapshot mergedSnapshot;
        int snapshotIndex;
        int previousIndex;
        bool broadcastAll = snapshot.m_BroadcastAll;
        for (snapshotIndex = 0; snapshotIndex < snapshot.m_TargetPositions.Count() && !broadcastAll; snapshotIndex = snapshotIndex + 1)
        {
            if (!AppendOwnerSnapshotPosition(combinedPositions, snapshot.m_TargetPositions[snapshotIndex]))
                broadcastAll = true;
        }
        Managed previousDeferredSnapshotRaw;
        if (m_DeferredOwnerSnapshots.Find(snapshot.m_OwnerDeviceId, previousDeferredSnapshotRaw) && Class.CastTo(previousSnapshot, previousDeferredSnapshotRaw) && previousSnapshot)
        {
            if (previousSnapshot.m_BroadcastAll)
                broadcastAll = true;
            if (!broadcastAll && !AppendOwnerSnapshotPosition(combinedPositions, previousSnapshot.m_OwnerPosition))
                broadcastAll = true;
            for (previousIndex = 0; previousIndex < previousSnapshot.m_TargetPositions.Count() && !broadcastAll; previousIndex = previousIndex + 1)
            {
                if (!AppendOwnerSnapshotPosition(combinedPositions, previousSnapshot.m_TargetPositions[previousIndex]))
                    broadcastAll = true;
            }
        }
        if (broadcastAll)
            combinedPositions.Clear();
        mergedSnapshot = new LFPG_OwnerBroadcastSnapshot(snapshot.m_OwnerDeviceId, snapshot.m_OwnerLow, snapshot.m_OwnerHigh, snapshot.m_JSON, snapshot.m_Generation, snapshot.m_OwnerPosition, combinedPositions);
        mergedSnapshot.m_BroadcastAll = broadcastAll;
        m_DeferredOwnerSnapshots[snapshot.m_OwnerDeviceId] = mergedSnapshot;
    }
    protected void DeferOwnerSnapshot(EntityAI owner, array<ref LFPG_WireData> extraInterestWires)
    {
        LFPG_OwnerBroadcastSnapshot snapshot;
        snapshot = CaptureOwnerBroadcastSnapshot(owner, extraInterestWires);
        if (snapshot)
            StoreDeferredOwnerSnapshot(snapshot);
    }
    protected void BroadcastOwnerSnapshot(LFPG_OwnerBroadcastSnapshot snapshot)
    {
        if (!snapshot || snapshot.m_OwnerDeviceId == "")
            return;
		if (m_FullSyncInProgress && m_FullSyncPlayer)
			StoreDeferredOwnerSnapshot(snapshot);
        m_ReusableBroadcastPlayers.Clear();
        g_Game.GetPlayers(m_ReusableBroadcastPlayers);
        if (LFPG_LOG_LEVEL >= 2)
        {
            string snapshotMsg = "[BroadcastOwnerSnapshot] owner=" + snapshot.m_OwnerDeviceId + " generation=" + snapshot.m_Generation.ToString() + " jsonLen=" + snapshot.m_JSON.Length().ToString();
            LFPG_Util.Debug(snapshotMsg);
        }
        float syncMaxDist = LFPG_CULL_DISTANCE_M + 20.0;
        float syncMaxDistSq = syncMaxDist * syncMaxDist;
        int playerIndex;
        int targetIndex;
        PlayerBase player;
        vector playerPosition;
        bool inRange;
        ScriptRPC rpc;
        bool guaranteed = true;
        for (playerIndex = 0; playerIndex < m_ReusableBroadcastPlayers.Count(); playerIndex = playerIndex + 1)
        {
            player = PlayerBase.Cast(m_ReusableBroadcastPlayers[playerIndex]);
			if (!CanReceiveWireBroadcast(player))
                continue;
            playerPosition = player.GetPosition();
            inRange = snapshot.m_BroadcastAll;
            if (!inRange)
                inRange = LFPG_WorldUtil.DistSq(playerPosition, snapshot.m_OwnerPosition) <= syncMaxDistSq;
            if (!inRange)
            {
                for (targetIndex = 0; targetIndex < snapshot.m_TargetPositions.Count(); targetIndex = targetIndex + 1)
                {
                    if (LFPG_WorldUtil.DistSq(playerPosition, snapshot.m_TargetPositions[targetIndex]) <= syncMaxDistSq)
                    {
                        inRange = true;
                        break;
                    }
                }
            }
            if (!inRange)
                continue;
            rpc = new ScriptRPC();
            rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES_V2);
            rpc.Write(snapshot.m_OwnerDeviceId);
            rpc.Write(snapshot.m_OwnerLow);
            rpc.Write(snapshot.m_OwnerHigh);
            rpc.Write(snapshot.m_JSON);
            rpc.Write(snapshot.m_Generation);
			PlayerIdentity recipient = player.GetIdentity();
			if (!recipient)
				continue;
			rpc.Send(player, LFPG_RPC_CHANNEL, guaranteed, recipient);
        }
    }
    override void BroadcastOwnerWireDelta(EntityAI owner, array<int> operations, array<ref LFPG_WireData> deltaWires)
    {
        if (!owner || !operations || !deltaWires)
            return;
        int entryCount = operations.Count();
        if (entryCount <= 0 || entryCount > LFPG_WIRE_DELTA_MAX_ENTRIES)
            return;
        if (deltaWires.Count() != entryCount)
            return;
        string ownerId = LFPG_DeviceAPI.GetDeviceId(owner);
        if (ownerId == "")
            return;
        if (CoalescePendingOwnerSnapshot(owner, deltaWires))
            return;
        m_PendingOwnerSnapshots.Remove(ownerId);
		if (m_FullSyncInProgress && m_FullSyncPlayer)
			DeferOwnerSnapshot(owner, deltaWires);
        LFPG_WireOwnerBase wireOwner = LFPG_WireOwnerBase.Cast(owner);
        if (!wireOwner)
        {
            BroadcastOwnerWires(owner);
            return;
        }
		int e;
        m_ReusableBroadcastPositions.Clear();
        LFPG_DeviceRegistry reg = LFPG_DeviceRegistry.Get();
        ref array<ref LFPG_WireData> currentWires = LFPG_DeviceAPI.GetDeviceWires(owner);
        if (currentWires)
        {
            int cw;
            for (cw = 0; cw < currentWires.Count(); cw = cw + 1)
            {
                LFPG_WireData currentWire = currentWires[cw];
                if (!currentWire || currentWire.m_TargetDeviceId == "")
                    continue;
                EntityAI currentTarget = reg.FindById(currentWire.m_TargetDeviceId);
                if (currentTarget)
                {
                    m_ReusableBroadcastPositions.Insert(currentTarget.GetPosition());
                }
            }
        }
        for (e = 0; e < entryCount; e = e + 1)
        {
            LFPG_WireData interestWire = deltaWires[e];
            if (!interestWire || interestWire.m_TargetDeviceId == "")
                continue;
            EntityAI interestTarget = reg.FindById(interestWire.m_TargetDeviceId);
            if (interestTarget)
            {
                m_ReusableBroadcastPositions.Insert(interestTarget.GetPosition());
            }
        }
		if (!SelectWireBroadcastRecipients(owner.GetPosition()))
			return;
		array<string> entryJsons = new array<string>;
		int payloadChars = 0;
		for (e = 0; e < entryCount; e = e + 1)
		{
			int operation = operations[e];
			if (operation != LFPG_WireDeltaOp.ADD && operation != LFPG_WireDeltaOp.REMOVE && operation != LFPG_WireDeltaOp.UPDATE)
				return;
			LFPG_WireData deltaWire = deltaWires[e];
			if (!deltaWire)
				return;
			LFPG_PersistBlob entryBlob = new LFPG_PersistBlob();
			entryBlob.wires.Insert(deltaWire);
			string entryJson = "";
			string entryErr = "";
			if (!JsonFileLoader<LFPG_PersistBlob>.MakeData(entryBlob, entryJson, entryErr, false))
			{
				BroadcastOwnerWires(owner);
				return;
			}
			entryJsons.Insert(entryJson);
			payloadChars = payloadChars + entryJson.Length();
		}
		int low = 0;
		int high = 0;
		owner.GetNetworkID(low, high);
		int generation = wireOwner.LFPG_GetWireGeneration();
        int i;
        for (i = 0; i < m_ReusableBroadcastPlayers.Count(); i = i + 1)
        {
            PlayerBase pb = PlayerBase.Cast(m_ReusableBroadcastPlayers[i]);
            if (!pb)
                continue;
            ScriptRPC rpc = new ScriptRPC();
            rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES_DELTA);
            rpc.Write(ownerId);
            rpc.Write(low);
            rpc.Write(high);
            rpc.Write(generation);
            rpc.Write(entryCount);
            for (e = 0; e < entryCount; e = e + 1)
            {
                rpc.Write(operations[e]);
                rpc.Write(entryJsons[e]);
            }
			PlayerIdentity recipient = pb.GetIdentity();
			if (!recipient)
				continue;
			rpc.Send(pb, LFPG_RPC_CHANNEL, true, recipient);
            #ifndef SERVER
            if (LFPG_PERFDIAG_ENABLED)
            {
                m_PerfDiagOwnerDeltaSendCount = m_PerfDiagOwnerDeltaSendCount + 1;
                string perfDelta = "LFPG_PERFDIAG delta_send count=";
                perfDelta = perfDelta + m_PerfDiagOwnerDeltaSendCount.ToString();
                perfDelta = perfDelta + " deviceId=";
                perfDelta = perfDelta + ownerId;
                perfDelta = perfDelta + " entries=";
                perfDelta = perfDelta + entryCount.ToString();
                perfDelta = perfDelta + " generation=";
                perfDelta = perfDelta + generation.ToString();
                perfDelta = perfDelta + " payload_chars=";
                perfDelta = perfDelta + payloadChars.ToString();
                Print(perfDelta);
            }
            #endif
        }
    }
    override void BroadcastVanillaWires(string ownerDeviceId, EntityAI ownerObj)
    {
        if (ownerDeviceId == "" || !ownerObj) return;
		if (m_FullSyncInProgress && m_FullSyncPlayer)
        {
            int vanillaDeferredIndex = m_DeferredBroadcastVanillaIds.Find(ownerDeviceId);
            if (vanillaDeferredIndex < 0)
            {
                m_DeferredBroadcastVanillaIds.Insert(ownerDeviceId);
                m_DeferredBroadcastVanillaObjs.Insert(ownerObj);
            }
            else
            {
                m_DeferredBroadcastVanillaObjs[vanillaDeferredIndex] = ownerObj;
            }
        }
        ref array<ref LFPG_WireData> wires = GetVanillaWires(ownerDeviceId);
        m_ReusableBroadcastPositions.Clear();
        if (wires)
        {
            LFPG_DeviceRegistry reg = LFPG_DeviceRegistry.Get();
            int tw;
            for (tw = 0; tw < wires.Count(); tw = tw + 1)
            {
                if (!wires[tw]) continue;
                if (wires[tw].m_TargetDeviceId == "") continue;
                EntityAI targetObj = reg.FindById(wires[tw].m_TargetDeviceId);
                if (targetObj)
                {
                    m_ReusableBroadcastPositions.Insert(targetObj.GetPosition());
                }
            }
        }
		if (!SelectWireBroadcastRecipients(ownerObj.GetPosition()))
			return;
		LFPG_PersistBlob blob = new LFPG_PersistBlob();
		blob.ver = LFPG_PERSIST_VER;
		if (wires)
		{
			int w;
			for (w = 0; w < wires.Count(); w = w + 1)
			{
				blob.wires.Insert(wires[w]);
			}
		}
		string json;
		string err;
		if (!JsonFileLoader<LFPG_PersistBlob>.MakeData(blob, json, err, false))
		{
			json = "";
		}
		int vanillaSnapshotGeneration = -1;
		int low = 0;
		int high = 0;
		ownerObj.GetNetworkID(low, high);
        int i;
        for (i = 0; i < m_ReusableBroadcastPlayers.Count(); i = i + 1)
        {
            PlayerBase pb = PlayerBase.Cast(m_ReusableBroadcastPlayers[i]);
            if (!pb) continue;
            ScriptRPC rpc = new ScriptRPC();
            rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES_V2);
            rpc.Write(ownerDeviceId);
            rpc.Write(low);
            rpc.Write(high);
            rpc.Write(json);
            rpc.Write(vanillaSnapshotGeneration);
            bool bRpcGuaranteed = true;
			PlayerIdentity recipient = pb.GetIdentity();
			if (!recipient)
				continue;
			rpc.Send(pb, LFPG_RPC_CHANNEL, bRpcGuaranteed, recipient);
        }
    }
    override void SendVanillaWiresTo(PlayerBase player, string ownerDeviceId, EntityAI ownerObj)
    {
        if (!player || ownerDeviceId == "" || !ownerObj) return;
        ref array<ref LFPG_WireData> wires = GetVanillaWires(ownerDeviceId);
        LFPG_PersistBlob blob = new LFPG_PersistBlob();
        blob.ver = LFPG_PERSIST_VER;
        if (wires)
        {
            int w;
            for (w = 0; w < wires.Count(); w = w + 1)
            {
                blob.wires.Insert(wires[w]);
            }
        }
        string json;
        string err;
        if (!JsonFileLoader<LFPG_PersistBlob>.MakeData(blob, json, err, false))
        {
            json = "";
        }
        int vanillaUnicastGeneration = -1;
        int low = 0;
        int high = 0;
        ownerObj.GetNetworkID(low, high);
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES_V2);
        rpc.Write(ownerDeviceId);
        rpc.Write(low);
        rpc.Write(high);
        rpc.Write(json);
        rpc.Write(vanillaUnicastGeneration);
        bool bRpcGuaranteed = true;
		PlayerIdentity recipient = player.GetIdentity();
		if (!recipient)
			return;
		rpc.Send(player, LFPG_RPC_CHANNEL, bRpcGuaranteed, recipient);
        #ifndef SERVER
        if (LFPG_PERFDIAG_ENABLED)
        {
            m_PerfDiagOwnerSnapshotUnicastCount = m_PerfDiagOwnerSnapshotUnicastCount + 1;
            string perfSnapshot = "LFPG_PERFDIAG snapshot_unicast count=";
            perfSnapshot = perfSnapshot + m_PerfDiagOwnerSnapshotUnicastCount.ToString();
            perfSnapshot = perfSnapshot + " deviceId=";
            perfSnapshot = perfSnapshot + ownerDeviceId;
            perfSnapshot = perfSnapshot + " jsonLen=";
            perfSnapshot = perfSnapshot + json.Length().ToString();
            Print(perfSnapshot);
        }
        #endif
    }
    override void SendFullSyncTo(PlayerBase player)
    {
        if (!player) return;
        if (m_FullSyncPlayer == player) return;
        if (m_FullSyncPendingPlayers.Find(player) >= 0) return;
        bool wasIdle = !m_FullSyncInProgress;
        m_FullSyncPendingPlayers.Insert(player);
        m_FullSyncInProgress = true;
        if (!m_StartupValidationDone || m_ValidationActive)
            return;
        if (wasIdle && m_StartupValidationDone && !m_ValidationActive)
            LFPG_StartNextFullSync();
    }
    protected void LFPG_StartNextFullSync()
    {
		m_FullSyncInProgress = false;
		m_FullSyncReplayActive = true;
        FlushDeferredBroadcasts();
		m_FullSyncReplayActive = false;
		m_FullSyncPlayer = null;
        while (m_FullSyncPendingPlayers.Count() > 0 && !m_FullSyncPlayer)
        {
            PlayerBase candidate = PlayerBase.Cast(m_FullSyncPendingPlayers[0]);
			m_FullSyncPendingPlayers.RemoveOrdered(0);
            if (candidate && candidate.GetIdentity())
                m_FullSyncPlayer = candidate;
        }
        if (!m_FullSyncPlayer)
        {
            return;
        }
        m_FullSyncInProgress = true;
        m_FullSyncOwners.Clear();
        LFPG_DeviceRegistry.Get().GetAll(m_FullSyncOwners);
        m_FullSyncVanillaIds.Clear();
        int vanillaSnapshotIndex;
        for (vanillaSnapshotIndex = 0; vanillaSnapshotIndex < m_VanillaWires.Count(); vanillaSnapshotIndex = vanillaSnapshotIndex + 1)
            m_FullSyncVanillaIds.Insert(m_VanillaWires.GetKey(vanillaSnapshotIndex));
        m_FullSyncOwnerCursor = 0;
        m_FullSyncVanillaCursor = 0;
        m_FullSyncPlayerPos = m_FullSyncPlayer.GetPosition();
        float maxDist = LFPG_CULL_DISTANCE_M + 20.0;
        m_FullSyncMaxDistSq = maxDist * maxDist;
        #ifndef SERVER
        if (LFPG_PERFDIAG_ENABLED)
        {
            string startMsg = "[FullSync] queued devices=";
            startMsg = startMsg + m_FullSyncOwners.Count().ToString();
            startMsg = startMsg + " playerPos=";
            startMsg = startMsg + m_FullSyncPlayerPos.ToString();
            LFPG_Util.Info(startMsg);
        }
        #endif
    }
    protected void LFPG_SendFullSyncOwner(EntityAI owner)
    {
        if (!owner) return;
        if (!LFPG_DeviceAPI.HasWireStore(owner)) return;
        if (LFPG_WorldUtil.DistSq(m_FullSyncPlayerPos, owner.GetPosition()) > m_FullSyncMaxDistSq) return;
        string devId = LFPG_DeviceAPI.GetDeviceId(owner);
        string json = LFPG_DeviceAPI.GetWiresJSON(owner);
        LFPG_WireOwnerBase wireOwner = LFPG_WireOwnerBase.Cast(owner);
        int generation = -1;
        if (wireOwner)
            generation = wireOwner.LFPG_GetWireGeneration();
        int low = 0;
        int high = 0;
        owner.GetNetworkID(low, high);
        #ifndef SERVER
        if (LFPG_PERFDIAG_ENABLED)
        {
            string devMsg = "[FullSync] LFPG dev=";
            devMsg = devMsg + devId;
            devMsg = devMsg + " net=" + low.ToString() + ":" + high.ToString();
            devMsg = devMsg + " type=" + owner.GetType();
            devMsg = devMsg + " jsonLen=" + json.Length().ToString();
            LFPG_Util.Info(devMsg);
        }
        #endif
        if (json.Length() > 12000)
        {
            string fsWarn = "[FullSync] LARGE BLOB dev=" + devId + " jsonLen=" + json.Length().ToString() + " — approaching RPC limit";
            LFPG_Util.Warn(fsWarn);
        }
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES_V2);
        rpc.Write(devId);
        rpc.Write(low);
        rpc.Write(high);
        rpc.Write(json);
        rpc.Write(generation);
        bool guaranteed = true;
		if (!m_FullSyncPlayer)
			return;
		PlayerIdentity recipient = m_FullSyncPlayer.GetIdentity();
		if (!recipient)
			return;
		rpc.Send(m_FullSyncPlayer, LFPG_RPC_CHANNEL, guaranteed, recipient);
    }
    protected void LFPG_ProcessFullSyncSpread()
    {
        if (!m_StartupValidationDone || m_ValidationActive)
            return;
        if (!m_FullSyncInProgress)
        {
            if (m_StartupValidationDone && !m_ValidationActive && m_FullSyncPendingPlayers.Count() > 0)
                LFPG_StartNextFullSync();
            return;
        }
        if (!m_FullSyncPlayer || !m_FullSyncPlayer.GetIdentity())
        {
            LFPG_StartNextFullSync();
            return;
        }
        int examined = 0;
        while (examined < LFPG_FULLSYNC_SENDS_PER_TICK && m_FullSyncOwnerCursor < m_FullSyncOwners.Count())
        {
            EntityAI owner = m_FullSyncOwners[m_FullSyncOwnerCursor];
            m_FullSyncOwnerCursor = m_FullSyncOwnerCursor + 1;
            examined = examined + 1;
            LFPG_SendFullSyncOwner(owner);
        }
        while (examined < LFPG_FULLSYNC_SENDS_PER_TICK && m_FullSyncOwnerCursor >= m_FullSyncOwners.Count() && m_FullSyncVanillaCursor < m_FullSyncVanillaIds.Count())
        {
            string vanillaId = m_FullSyncVanillaIds[m_FullSyncVanillaCursor];
            m_FullSyncVanillaCursor = m_FullSyncVanillaCursor + 1;
            examined = examined + 1;
            EntityAI vanillaObj = LFPG_DeviceRegistry.Get().FindById(vanillaId);
            if (!vanillaObj) continue;
            if (LFPG_WorldUtil.DistSq(m_FullSyncPlayerPos, vanillaObj.GetPosition()) > m_FullSyncMaxDistSq) continue;
            SendVanillaWiresTo(m_FullSyncPlayer, vanillaId, vanillaObj);
        }
        if (m_FullSyncOwnerCursor >= m_FullSyncOwners.Count() && m_FullSyncVanillaCursor >= m_FullSyncVanillaIds.Count())
            LFPG_StartNextFullSync();
    }
    protected void FlushDeferredBroadcasts()
    {
        int snapshotIndex;
        LFPG_OwnerBroadcastSnapshot snapshot;
        bool pendingOwner;
        for (snapshotIndex = 0; snapshotIndex < m_DeferredOwnerSnapshots.Count(); snapshotIndex = snapshotIndex + 1)
        {
            snapshot = LFPG_OwnerBroadcastSnapshot.Cast(m_DeferredOwnerSnapshots.GetElement(snapshotIndex));
            if (snapshot)
            {
                pendingOwner = m_PendingOwnerSnapshots.Contains(snapshot.m_OwnerDeviceId);
                if (!pendingOwner)
                    pendingOwner = m_PendingBroadcastLFPG.Contains(snapshot.m_OwnerDeviceId);
                if (!pendingOwner)
                    BroadcastOwnerSnapshot(snapshot);
            }
        }
        m_DeferredOwnerSnapshots.Clear();
        int vi;
        for (vi = 0; vi < m_DeferredBroadcastVanillaIds.Count(); vi = vi + 1)
        {
            string vId = m_DeferredBroadcastVanillaIds[vi];
            EntityAI vObj = m_DeferredBroadcastVanillaObjs[vi];
            if (vId != "" && vObj)
            {
                BroadcastVanillaWires(vId, vObj);
            }
        }
        m_DeferredBroadcastVanillaIds.Clear();
        m_DeferredBroadcastVanillaObjs.Clear();
    }
    override void SendDeviceSyncTo(PlayerBase player, string deviceId)
    {
        if (!player || deviceId == "")
            return;
        map<string, bool> sentOwners = new map<string, bool>;
        SendDeviceSyncToBatched(player, deviceId, sentOwners);
    }
    override void SendDeviceSyncToBatched(PlayerBase player, string deviceId, map<string, bool> sentOwners)
    {
        if (!player || deviceId == "" || !sentOwners)
            return;
        LFPG_DeviceRegistry reg = LFPG_DeviceRegistry.Get();
        if (!reg)
            return;
        bool hasRelevantState = false;
        EntityAI deviceObj = reg.FindById(deviceId);
        if (deviceObj && LFPG_DeviceAPI.HasWireStore(deviceObj))
        {
            hasRelevantState = true;
            if (!sentOwners.Contains(deviceId))
            {
                SendOwnerBlobTo(player, deviceObj, deviceId);
                sentOwners[deviceId] = true;
            }
        }
        else if (deviceObj && m_VanillaWires.Contains(deviceId))
        {
            hasRelevantState = true;
            if (!sentOwners.Contains(deviceId))
            {
                SendVanillaWiresTo(player, deviceId, deviceObj);
                sentOwners[deviceId] = true;
            }
        }
        if (m_Graph)
        {
            ref array<ref LFPG_ElecEdge> incomingEdges = m_Graph.GetIncoming(deviceId);
            if (incomingEdges)
            {
                int i;
                for (i = 0; i < incomingEdges.Count(); i = i + 1)
                {
                    LFPG_ElecEdge edge = incomingEdges[i];
                    if (!edge || edge.m_SourceNodeId == "")
                        continue;
                    string ownerId = edge.m_SourceNodeId;
                    if (sentOwners.Contains(ownerId))
                    {
                        hasRelevantState = true;
                        continue;
                    }
                    EntityAI ownerObj = reg.FindById(ownerId);
                    if (!ownerObj)
                        continue;
                    if (LFPG_DeviceAPI.HasWireStore(ownerObj))
                    {
                        hasRelevantState = true;
                        SendOwnerBlobTo(player, ownerObj, ownerId);
                        sentOwners[ownerId] = true;
                    }
                    else if (m_VanillaWires.Contains(ownerId))
                    {
                        hasRelevantState = true;
                        SendVanillaWiresTo(player, ownerId, ownerObj);
                        sentOwners[ownerId] = true;
                    }
                }
            }
        }
        if (deviceObj && !hasRelevantState)
        {
            SendEmptyDeviceCableStateTo(player, deviceObj, deviceId);
        }
        if (LFPG_LOG_LEVEL >= 2)
        {
            string sdMsg = "[SendDeviceSyncTo] Completed for deviceId=" + deviceId;
            LFPG_Util.Debug(sdMsg);
        }
    }
    override void SendEmptyDeviceCableStateTo(PlayerBase player, EntityAI deviceObj, string deviceId)
    {
        if (!player || !deviceObj || deviceId == "")
            return;
        int low = 0;
        int high = 0;
        deviceObj.GetNetworkID(low, high);
        string json = LFPG_WireHelper.GetJSON(null);
        int emptyGeneration = -1;
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES_V2);
        rpc.Write(deviceId);
        rpc.Write(low);
        rpc.Write(high);
        rpc.Write(json);
        rpc.Write(emptyGeneration);
		PlayerIdentity recipient = player.GetIdentity();
		if (!recipient)
			return;
		rpc.Send(player, LFPG_RPC_CHANNEL, true, recipient);
        #ifndef SERVER
        if (LFPG_PERFDIAG_ENABLED)
        {
            m_PerfDiagOwnerSnapshotUnicastCount = m_PerfDiagOwnerSnapshotUnicastCount + 1;
            string perfSnapshot = "LFPG_PERFDIAG snapshot_unicast count=";
            perfSnapshot = perfSnapshot + m_PerfDiagOwnerSnapshotUnicastCount.ToString();
            perfSnapshot = perfSnapshot + " deviceId=";
            perfSnapshot = perfSnapshot + deviceId;
            perfSnapshot = perfSnapshot + " jsonLen=";
            perfSnapshot = perfSnapshot + json.Length().ToString();
            Print(perfSnapshot);
        }
        #endif
    }
    override void SendOwnerBlobTo(PlayerBase player, EntityAI ownerObj, string ownerId)
    {
        if (!player || !ownerObj || ownerId == "") return;
        string json = LFPG_DeviceAPI.GetWiresJSON(ownerObj);
        LFPG_WireOwnerBase ownerWireState = LFPG_WireOwnerBase.Cast(ownerObj);
        int ownerGeneration = -1;
        if (ownerWireState)
        {
            ownerGeneration = ownerWireState.LFPG_GetWireGeneration();
        }
        int low = 0;
        int high = 0;
        ownerObj.GetNetworkID(low, high);
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.SYNC_OWNER_WIRES_V2);
        rpc.Write(ownerId);
        rpc.Write(low);
        rpc.Write(high);
        rpc.Write(json);
        rpc.Write(ownerGeneration);
        bool bRpcGuaranteed = true;
		PlayerIdentity recipient = player.GetIdentity();
		if (!recipient)
			return;
		rpc.Send(player, LFPG_RPC_CHANNEL, bRpcGuaranteed, recipient);
        #ifndef SERVER
        if (LFPG_PERFDIAG_ENABLED)
        {
            m_PerfDiagOwnerSnapshotUnicastCount = m_PerfDiagOwnerSnapshotUnicastCount + 1;
            string perfSnapshot = "LFPG_PERFDIAG snapshot_unicast count=";
            perfSnapshot = perfSnapshot + m_PerfDiagOwnerSnapshotUnicastCount.ToString();
            perfSnapshot = perfSnapshot + " deviceId=";
            perfSnapshot = perfSnapshot + ownerId;
            perfSnapshot = perfSnapshot + " jsonLen=";
            perfSnapshot = perfSnapshot + json.Length().ToString();
            Print(perfSnapshot);
        }
        #endif
        if (LFPG_LOG_LEVEL >= 2)
        {
            string sobMsg = "[SendOwnerBlobTo] owner=" + ownerId + " net=" + low.ToString() + ":" + high.ToString() + " jsonLen=" + json.Length().ToString();
            LFPG_Util.Debug(sobMsg);
        }
        if (json.Length() > 12000)
        {
            string sobWarn = "[SendOwnerBlobTo] LARGE BLOB owner=" + ownerId + " jsonLen=" + json.Length().ToString() + " — approaching RPC limit";
            LFPG_Util.Warn(sobWarn);
        }
    }
    override void RequestPropagate(string sourceDeviceId)
    {
        #ifdef SERVER
        if (sourceDeviceId == "") return;
        if (!m_Graph)
        {
            string gNullMsg = "[Propagate] Graph null, cannot propagate " + sourceDeviceId;
            LFPG_Util.Warn(gNullMsg);
            return;
        }
        m_Graph.RefreshSourceState(sourceDeviceId);
        m_Graph.MarkNodeDirty(sourceDeviceId, LFPG_DIRTY_INTERNAL);
        if (LFPG_LOG_LEVEL >= 2)
        {
            string qdMsg = "[Propagate] Queued dirty: " + sourceDeviceId;
            LFPG_Util.Debug(qdMsg);
        }
        #endif
    }
   protected void TickPropagation()
    {
        #ifdef SERVER
        if (!m_Graph)
            return;
        int nodeBudget = LFPG_PROPAGATE_NODE_BUDGET;
        int edgeBudget = LFPG_PROPAGATE_EDGE_BUDGET;
        if (m_WarmupActive)
        {
            nodeBudget = LFPG_PROPAGATE_WARMUP_BUDGET;
            edgeBudget = LFPG_PROPAGATE_EDGE_WARMUP_BUDGET;
        }
        else
        {
            int queueSize = m_Graph.GetDirtyQueueSize();
            if (queueSize > LFPG_DYNAMIC_BUDGET_QUEUE_THRESHOLD)
            {
                edgeBudget = edgeBudget * 2;
                nodeBudget = nodeBudget * 2;
            }
        }
        int remaining = m_Graph.ProcessDirtyQueue(nodeBudget, edgeBudget);
        if (remaining <= 0 && m_WarmupActive)
        {
            m_WarmupActive = false;
            string wdMsg = "[Propagate] Warmup drain complete";
            LFPG_Util.Info(wdMsg);
        }
        if (remaining > 0 || m_Graph.GetLastEdgesVisited() > 0)
        {
            if (LFPG_LOG_LEVEL >= 2)
            {
                string tickMsg = "[Propagate] Tick: " + remaining.ToString() + " remaining" + " nodeBudget=" + nodeBudget.ToString() + " edgeBudget=" + edgeBudget.ToString() + " edgesUsed=" + m_Graph.GetLastEdgesVisited().ToString();
                LFPG_Util.Debug(tickMsg);
            }
        }
        int processMs = m_Graph.GetLastProcessMs();
        int edgesUsed = m_Graph.GetLastEdgesVisited();
        m_TelemTickCount = m_TelemTickCount + 1;
        m_TelemTotalProcessMs = m_TelemTotalProcessMs + processMs;
        m_TelemTotalEdgesVisited = m_TelemTotalEdgesVisited + edgesUsed;
        if (processMs > m_TelemPeakProcessMs)
        {
            m_TelemPeakProcessMs = processMs;
        }
        float nowMs = g_Game.GetTime();
        float elapsed = nowMs - m_TelemLastDumpMs;
        if (m_TelemLastDumpMs < 0.0)
        {
            m_TelemLastDumpMs = nowMs;
        }
        else if (elapsed >= LFPG_TELEM_INTERVAL_MS && m_TelemTickCount > 0)
        {
            int avgMs = 0;
            int avgEdges = 0;
            if (m_TelemTickCount > 0)
            {
                avgMs = m_TelemTotalProcessMs / m_TelemTickCount;
                avgEdges = m_TelemTotalEdgesVisited / m_TelemTickCount;
            }
            int overloadCount = m_Graph.GetOverloadedSourceCount();
            if (LFPG_LOG_LEVEL >= 2)
            {
                string tLog = "[Telemetry-Propagation]";
                tLog = tLog + " ticks=" + m_TelemTickCount.ToString();
                tLog = tLog + " avgMs=" + avgMs.ToString();
                tLog = tLog + " peakMs=" + m_TelemPeakProcessMs.ToString();
                tLog = tLog + " avgEdges=" + avgEdges.ToString();
                tLog = tLog + " nodes=" + m_Graph.GetNodeCount().ToString();
                tLog = tLog + " edges=" + m_Graph.GetEdgeCount().ToString();
                tLog = tLog + " queueRemain=" + remaining.ToString();
                tLog = tLog + " overloadedSources=" + overloadCount.ToString();
                tLog = tLog + " epoch=" + m_Graph.GetCurrentEpoch().ToString();
                LFPG_Util.Debug(tLog);
            }
            m_TelemTickCount = 0;
            m_TelemTotalProcessMs = 0;
            m_TelemPeakProcessMs = 0;
            m_TelemTotalEdgesVisited = 0;
            m_TelemLastDumpMs = nowMs;
        }
        #endif
    }
    override void TrackDeviceForPolling(string deviceId)
    {
        #ifdef SERVER
        if (!g_Game.IsServer())
            return;
        if (deviceId == "")
            return;
        int existingIdx;
        if (m_TrackedDeviceIndex.Find(deviceId, existingIdx))
            return;  // already tracked
        int idx = m_TrackedDeviceIds.Count();
        m_TrackedDeviceIds.Insert(deviceId);
        m_TrackedDeviceIndex.Set(deviceId, idx);
        EntityAI dev = LFPG_DeviceRegistry.Get().FindById(deviceId);
        if (dev)
        {
            m_LastKnownPos.Set(deviceId, dev.GetPosition());
        }
        #endif
    }
    override void UntrackDeviceFromPolling(string deviceId)
    {
        #ifdef SERVER
        if (!g_Game.IsServer())
            return;
        if (deviceId == "")
            return;
        int idx;
        if (!m_TrackedDeviceIndex.Find(deviceId, idx))
            return;  // not tracked
        int lastIdx = m_TrackedDeviceIds.Count() - 1;
        if (idx < lastIdx)
        {
            string lastId = m_TrackedDeviceIds[lastIdx];
            m_TrackedDeviceIds[idx] = lastId;
            m_TrackedDeviceIndex.Set(lastId, idx);
        }
        m_TrackedDeviceIds.Remove(lastIdx);
        m_TrackedDeviceIndex.Remove(deviceId);
        m_LastKnownPos.Remove(deviceId);
        if (m_TrackCursor >= m_TrackedDeviceIds.Count())
        {
            m_TrackCursor = 0;
        }
        #endif
    }
    protected bool DeviceHasAnyWires(EntityAI device, string deviceId)
    {
        #ifdef SERVER
        if (!g_Game.IsServer())
            return false;
        if (!device || deviceId == "")
            return false;
        if (LFPG_DeviceAPI.HasWireStore(device))
        {
            ref array<ref LFPG_WireData> ownedWires = LFPG_DeviceAPI.GetDeviceWires(device);
            if (ownedWires && ownedWires.Count() > 0)
            {
                return true;
            }
        }
		m_WireQueryStore = GetVanillaWires(deviceId);
		bool hasVanillaOutputs = m_WireQueryStore && m_WireQueryStore.Count() > 0;
		m_WireQueryStore = null;
		if (hasVanillaOutputs)
			return true;
        int portCount = LFPG_DeviceAPI.GetPortCount(device);
        int pci;
        for (pci = 0; pci < portCount; pci = pci + 1)
        {
            int pdChk = LFPG_DeviceAPI.GetPortDir(device, pci);
            if (pdChk == LFPG_PortDir.IN)
            {
                string pnChk = LFPG_DeviceAPI.GetPortName(device, pci);
                if (CountWiresTargeting(deviceId, pnChk) > 0)
                {
                    return true;
                }
            }
        }
        #endif
        return false;
    }
    protected void RebuildTrackedDevices()
    {
        #ifdef SERVER
        if (!g_Game.IsServer())
            return;
        m_TrackedDeviceIds.Clear();
        m_TrackedDeviceIndex.Clear();
        m_TrackCursor = 0;
        ref array<EntityAI> allDevs = new array<EntityAI>;
        LFPG_DeviceRegistry.Get().GetAll(allDevs);
        int i;
        for (i = 0; i < allDevs.Count(); i = i + 1)
        {
            EntityAI dev = allDevs[i];
            if (!dev)
                continue;
            string devId = LFPG_DeviceAPI.GetOrCreateDeviceId(dev);
            if (devId == "")
                continue;
            if (DeviceHasAnyWires(dev, devId))
            {
                int insertIdx = m_TrackedDeviceIds.Count();
                m_TrackedDeviceIds.Insert(devId);
                m_TrackedDeviceIndex.Set(devId, insertIdx);
                m_LastKnownPos.Set(devId, dev.GetPosition());
            }
        }
        string rtdMsg = "[Movement] RebuildTrackedDevices: tracking " + m_TrackedDeviceIds.Count().ToString() + " wired devices";
        LFPG_Util.Info(rtdMsg);
        #endif
    }
    protected void CleanDisappearedVanillaDevice(string deviceId)
    {
        #ifdef SERVER
        if (deviceId == "" || deviceId.IndexOf("vp:") != 0)
            return;
        bool anyChanged = false;
        ref array<string> neighborIds = new array<string>;
        if (m_Graph)
        {
            ref array<ref LFPG_ElecEdge> outEdges = m_Graph.GetOutgoing(deviceId);
            if (outEdges)
            {
                int oe;
                for (oe = 0; oe < outEdges.Count(); oe = oe + 1)
                {
                    ref LFPG_ElecEdge oEdge = outEdges[oe];
                    if (oEdge && oEdge.m_TargetNodeId != "")
                    {
                        neighborIds.Insert(oEdge.m_TargetNodeId);
                    }
                }
            }
            ref array<ref LFPG_ElecEdge> inEdges = m_Graph.GetIncoming(deviceId);
            if (inEdges)
            {
                int ie;
                for (ie = 0; ie < inEdges.Count(); ie = ie + 1)
                {
                    ref LFPG_ElecEdge iEdge = inEdges[ie];
                    if (iEdge && iEdge.m_SourceNodeId != "")
                    {
                        neighborIds.Insert(iEdge.m_SourceNodeId);
                    }
                }
            }
        }
        ref array<ref LFPG_WireData> vWires;
        if (m_VanillaWires.Find(deviceId, vWires) && vWires)
        {
            int vw = vWires.Count() - 1;
            while (vw >= 0)
            {
                LFPG_WireData vwd = vWires[vw];
                if (vwd)
                {
                    ReverseIdxRemove(vwd.m_TargetDeviceId, vwd.m_TargetPort, deviceId);
                    PlayerWireCountAdd(vwd.m_CreatorId, -1);
                }
                vw = vw - 1;
            }
            vWires.Clear();
            anyChanged = true;
        }
        m_VanillaWires.Remove(deviceId);
		string keyPrefix = deviceId + "|";
		int prefixLength = keyPrefix.Length();
		m_ReusableReversePorts.Clear();
		int reverseIndex;
		for (reverseIndex = 0; reverseIndex < m_ReverseIdx.Count(); reverseIndex = reverseIndex + 1)
		{
			string reverseKey = m_ReverseIdx.GetKey(reverseIndex);
			if (reverseKey.IndexOf(keyPrefix) != 0)
				continue;
			string targetPort = "input_main";
			int portLength = reverseKey.Length() - prefixLength;
			if (portLength > 0)
				targetPort = reverseKey.Substring(prefixLength, portLength);
			m_ReusableReversePorts.Insert(targetPort);
		}
		int portIndex;
		for (portIndex = 0; portIndex < m_ReusableReversePorts.Count(); portIndex = portIndex + 1)
		{
			if (RemoveWiresTargeting(deviceId, m_ReusableReversePorts[portIndex]) > 0)
				anyChanged = true;
		}
        int ni;
        for (ni = 0; ni < neighborIds.Count(); ni = ni + 1)
        {
            EntityAI neighborDev = LFPG_DeviceRegistry.Get().FindById(neighborIds[ni]);
            if (!neighborDev)
            {
                neighborDev = LFPG_DeviceAPI.ResolveVanillaDevice(neighborIds[ni]);
            }
            if (neighborDev)
            {
                LFPG_DeviceAPI.SetPowered(neighborDev, false);
            }
        }
        if (anyChanged)
        {
            MarkVanillaDirty();
            PostBulkRebuildAndPropagate();
            FlushBroadcasts();
            if (m_VanillaDirty)
            {
                FlushVanillaIfDirty();
            }
            LFPG_Util.Warn("[VanillaGone] Cleaned wires for disappeared device " + deviceId);
        }
        m_LastKnownPos.Remove(deviceId);
        RequestGlobalSelfHeal();
        int nui;
        for (nui = 0; nui < neighborIds.Count(); nui = nui + 1)
        {
            string nId = neighborIds[nui];
            EntityAI nDev = LFPG_DeviceRegistry.Get().FindById(nId);
            if (nDev)
            {
                if (!DeviceHasAnyWires(nDev, nId))
                {
                    UntrackDeviceFromPolling(nId);
                }
            }
            else
            {
                UntrackDeviceFromPolling(nId);
            }
        }
        #endif
    }
    protected void CheckDeviceMovement()
    {
        #ifdef SERVER
        if (!g_Game.IsServer())
            return;
        int totalTracked = m_TrackedDeviceIds.Count();
        if (totalTracked == 0)
            return;
        if (m_TrackCursor >= totalTracked)
        {
            m_TrackCursor = 0;
        }
        int batchEnd = m_TrackCursor + LFPG_MOVE_DETECT_BATCH_SIZE;
        if (batchEnd > totalTracked)
        {
            batchEnd = totalTracked;
        }
        m_ReusableMovedIds.Clear();
        m_ReusableMovedDevs.Clear();
        m_ReusableMovedOldPositions.Clear();
        m_ReusableDisappearedIds.Clear();
        int i;
        for (i = m_TrackCursor; i < batchEnd; i = i + 1)
        {
            string devId = m_TrackedDeviceIds[i];
            if (devId == "")
                continue;
            EntityAI dev = LFPG_DeviceRegistry.Get().FindById(devId);
            if (!dev)
            {
                m_ReusableDisappearedIds.Insert(devId);
                continue;
            }
            vector currentPos = dev.GetPosition();
            vector lastPos;
            bool hadPos = m_LastKnownPos.Find(devId, lastPos);
            if (hadPos)
            {
                float distSq = LFPG_WorldUtil.DistSq(currentPos, lastPos);
                if (distSq > LFPG_MOVE_DETECT_THRESHOLD_SQ)
                {
                    m_ReusableMovedIds.Insert(devId);
                    m_ReusableMovedDevs.Insert(dev);
                    m_ReusableMovedOldPositions.Insert(lastPos);
                }
            }
            else
            {
                m_LastKnownPos.Set(devId, currentPos);
            }
        }
        int newCursor = batchEnd;
        int di;
        for (di = 0; di < m_ReusableDisappearedIds.Count(); di = di + 1)
        {
            string goneId = m_ReusableDisappearedIds[di];
            if (goneId.IndexOf("vp:") == 0)
            {
                LFPG_Util.Warn("[Movement] Vanilla device disappeared id=" + goneId + " — cleaning orphan wires");
                CleanDisappearedVanillaDevice(goneId);
            }
            UntrackDeviceFromPolling(goneId);
        }
        int mi;
        for (mi = 0; mi < m_ReusableMovedIds.Count(); mi = mi + 1)
        {
            EntityAI movedDev = m_ReusableMovedDevs[mi];
            string movedId = m_ReusableMovedIds[mi];
            vector movedOldPosition = m_ReusableMovedOldPositions[mi];
            string mvMsg = "[Movement] Device " + movedId + " type=" + movedDev.GetType() + " moved — disconnecting wires";
            LFPG_Util.Warn(mvMsg);
            CutAllWiresFromMovedDevice(movedDev, movedOldPosition, movedId);
            LFPG_Generator gen = LFPG_Generator.Cast(movedDev);
            if (gen && gen.LFPG_GetSwitchState())
            {
                gen.LFPG_ToggleSource();
            }
        }
        if (m_ReusableMovedIds.Count() > 0)
        {
            string mvCntMsg = "[Movement] " + m_ReusableMovedIds.Count().ToString() + " devices moved, requesting self-heal";
            LFPG_Util.Info(mvCntMsg);
            RequestGlobalSelfHeal(true);
        }
        int trackedCount = m_TrackedDeviceIds.Count();
        if (trackedCount == 0)
        {
            m_TrackCursor = 0;
        }
        else if (newCursor >= trackedCount)
        {
            m_TrackCursor = 0;
        }
        else
        {
            m_TrackCursor = newCursor;
        }
        #endif
    }
    override void RequestGlobalSelfHeal(bool validationOnlyAfterCut = false)
    {
        #ifdef SERVER
        if (validationOnlyAfterCut && m_CutGraphRebuildQueued)
            m_ValidationAfterCutRequested = true;
        else if (validationOnlyAfterCut && !m_IndexHealAfterCut)
            return;
        if (m_SelfHealQueued) return;
        m_SelfHealQueued = true;
        bool bOnce = false;
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DoGlobalSelfHeal, 500, bOnce);
        #endif
    }
    protected void DoGlobalSelfHeal()
    {
        #ifdef SERVER
        m_SelfHealQueued = false;
        ValidateAllWiresAndPropagate();
        #endif
    }
    override void ValidateAllWiresAndPropagate()
    {
        #ifdef SERVER
        if (m_ValidationActive)
        {
            m_ValidationRerunRequested = true;
            return;
        }
        m_ValidationActive = true;
        m_ValidationRerunRequested = false;
        m_ValidationPhase = LFPG_VALIDATE_RESOLVE_VANILLA;
        m_ValidationCursor = 0;
        m_ValidationStartMs = g_Game.GetTime();
        m_ValidationOwnersPruned = 0;
        m_ValidationSkipGraphRebuild = m_ValidationOnlyHealPending;
        m_ValidationOnlyHealPending = false;
        m_ValidationGraphGeneration = m_GraphRebuildGeneration;
        m_ValidationDevices.Clear();
        m_ValidationValidIds.Clear();
        m_ValidationVanillaIds.Clear();
        int vanillaSnapshotIndex;
        for (vanillaSnapshotIndex = 0; vanillaSnapshotIndex < m_VanillaWires.Count(); vanillaSnapshotIndex = vanillaSnapshotIndex + 1)
            m_ValidationVanillaIds.Insert(m_VanillaWires.GetKey(vanillaSnapshotIndex));
        m_CachedValidIds = null;
        LFPG_Util.Info("[SelfHeal] Phased validation scheduled");
        #endif
    }
    protected void LFPG_ValidationResolveVanillaOwners()
    {
        int end = Math.Min(m_ValidationCursor + LFPG_STARTUP_VALIDATE_OWNERS_PER_TICK, m_ValidationVanillaIds.Count());
        int vi;
        for (vi = m_ValidationCursor; vi < end; vi = vi + 1)
        {
            string ownerId = m_ValidationVanillaIds[vi];
            array<ref LFPG_WireData> wires;
            if (!m_VanillaWires.Find(ownerId, wires) || !wires)
                continue;
            if (!LFPG_DeviceRegistry.Get().FindById(ownerId))
                LFPG_DeviceAPI.ResolveVanillaDevice(ownerId);
            int wi;
            for (wi = 0; wi < wires.Count(); wi = wi + 1)
            {
                LFPG_WireData wd = wires[wi];
                if (!wd || wd.m_TargetDeviceId == "") continue;
                if (!LFPG_DeviceRegistry.Get().FindById(wd.m_TargetDeviceId))
                    LFPG_DeviceAPI.ResolveVanillaDevice(wd.m_TargetDeviceId);
            }
        }
        m_ValidationCursor = end;
        if (m_ValidationCursor >= m_ValidationVanillaIds.Count())
        {
            m_ValidationCursor = 0;
            m_ValidationPhase = LFPG_VALIDATE_SNAPSHOT_PRE;
        }
    }
    protected void LFPG_ValidationSnapshotPre()
    {
        LFPG_DeviceRegistry.Get().GetAll(m_ValidationDevices);
        m_ValidationCursor = 0;
        m_ValidationPhase = LFPG_VALIDATE_RESOLVE_LFPG;
    }
    protected void LFPG_ValidationResolveLFPGTargets()
    {
        int end = Math.Min(m_ValidationCursor + LFPG_STARTUP_VALIDATE_OWNERS_PER_TICK, m_ValidationDevices.Count());
        int di;
        for (di = m_ValidationCursor; di < end; di = di + 1)
        {
            ref array<ref LFPG_WireData> wires = LFPG_DeviceAPI.GetDeviceWires(m_ValidationDevices[di]);
            if (!wires) continue;
            int wi;
            for (wi = 0; wi < wires.Count(); wi = wi + 1)
            {
                LFPG_WireData wd = wires[wi];
                if (!wd) continue;
                if (wd.m_TargetDeviceId.IndexOf("vp:") != 0) continue;
                if (!LFPG_DeviceRegistry.Get().FindById(wd.m_TargetDeviceId))
                    LFPG_DeviceAPI.ResolveVanillaDevice(wd.m_TargetDeviceId);
            }
        }
        m_ValidationCursor = end;
        if (m_ValidationCursor >= m_ValidationDevices.Count())
        {
            m_ValidationCursor = 0;
            m_ValidationPhase = LFPG_VALIDATE_SNAPSHOT_FINAL;
        }
    }
    protected void LFPG_ValidationSnapshotFinal()
    {
        LFPG_DeviceRegistry.Get().PruneNullEntries();
        LFPG_DeviceRegistry.Get().GetAll(m_ValidationDevices);
        m_ValidationValidIds.Clear();
        m_CachedValidIds = m_ValidationValidIds;
        m_ValidationCursor = 0;
        m_ValidationPhase = LFPG_VALIDATE_BUILD_VALID;
    }
    protected void LFPG_ValidationBuildValidIds()
    {
        int end = Math.Min(m_ValidationCursor + LFPG_STARTUP_VALIDATE_OWNERS_PER_TICK, m_ValidationDevices.Count());
        int vi;
        for (vi = m_ValidationCursor; vi < end; vi = vi + 1)
        {
            string deviceId = LFPG_DeviceAPI.GetOrCreateDeviceId(m_ValidationDevices[vi]);
            if (deviceId != "")
                m_ValidationValidIds[deviceId] = true;
        }
        m_ValidationCursor = end;
        if (m_ValidationCursor >= m_ValidationDevices.Count())
        {
            m_ValidationCursor = 0;
            m_ValidationPhase = LFPG_VALIDATE_REFRESH_LFPG;
        }
    }
    protected void LFPG_ValidationRefreshLFPGNetworkIds()
    {
        int end = Math.Min(m_ValidationCursor + LFPG_STARTUP_VALIDATE_OWNERS_PER_TICK, m_ValidationDevices.Count());
        int di;
        for (di = m_ValidationCursor; di < end; di = di + 1)
        {
            EntityAI owner = m_ValidationDevices[di];
            ref array<ref LFPG_WireData> wires = LFPG_DeviceAPI.GetDeviceWires(owner);
            if (!wires) continue;
            int wi;
            for (wi = 0; wi < wires.Count(); wi = wi + 1)
            {
                LFPG_WireData wd = wires[wi];
                if (!wd) continue;
                EntityAI target = LFPG_DeviceRegistry.Get().FindById(wd.m_TargetDeviceId);
                if (target)
                {
                    int low = 0;
                    int high = 0;
                    target.GetNetworkID(low, high);
                    wd.m_TargetNetLow = low;
                    wd.m_TargetNetHigh = high;
                }
                else
                {
                    wd.m_TargetNetLow = 0;
                    wd.m_TargetNetHigh = 0;
                }
            }
            LFPG_WireOwnerBase cacheOwner = LFPG_WireOwnerBase.Cast(owner);
            if (cacheOwner)
                cacheOwner.LFPG_InvalidateWireJSONCache();
        }
        m_ValidationCursor = end;
        if (m_ValidationCursor >= m_ValidationDevices.Count())
        {
            m_ValidationCursor = 0;
            m_ValidationPhase = LFPG_VALIDATE_REFRESH_VANILLA;
        }
    }
    protected void LFPG_ValidationRefreshVanillaNetworkIds()
    {
        int end = Math.Min(m_ValidationCursor + LFPG_STARTUP_VALIDATE_OWNERS_PER_TICK, m_ValidationVanillaIds.Count());
        int vi;
        for (vi = m_ValidationCursor; vi < end; vi = vi + 1)
        {
            string ownerId = m_ValidationVanillaIds[vi];
            array<ref LFPG_WireData> wires;
            if (!m_VanillaWires.Find(ownerId, wires) || !wires)
                continue;
            int wi;
            for (wi = 0; wi < wires.Count(); wi = wi + 1)
            {
                LFPG_WireData wd = wires[wi];
                if (!wd) continue;
                EntityAI target = LFPG_DeviceRegistry.Get().FindById(wd.m_TargetDeviceId);
                if (target)
                {
                    int low = 0;
                    int high = 0;
                    target.GetNetworkID(low, high);
                    wd.m_TargetNetLow = low;
                    wd.m_TargetNetHigh = high;
                }
                else
                {
                    wd.m_TargetNetLow = 0;
                    wd.m_TargetNetHigh = 0;
                }
            }
        }
        m_ValidationCursor = end;
        if (m_ValidationCursor >= m_ValidationVanillaIds.Count())
        {
            m_ValidationCursor = 0;
            m_ValidationPhase = LFPG_VALIDATE_PRUNE_LFPG;
        }
    }
    protected void LFPG_ValidationPruneLFPGOwners()
    {
        int end = Math.Min(m_ValidationCursor + LFPG_STARTUP_VALIDATE_OWNERS_PER_TICK, m_ValidationDevices.Count());
        int di;
        for (di = m_ValidationCursor; di < end; di = di + 1)
        {
            EntityAI owner = m_ValidationDevices[di];
            if (!LFPG_DeviceAPI.HasWireStore(owner)) continue;
            bool changed = LFPG_DeviceAPI.PruneDeviceMissingTargets(owner);
            if (changed)
            {
                m_ValidationOwnersPruned = m_ValidationOwnersPruned + 1;
                BroadcastOwnerWires(owner);
            }
        }
        m_ValidationCursor = end;
        if (m_ValidationCursor >= m_ValidationDevices.Count())
        {
            m_CachedValidIds = null;
            m_ValidationCursor = 0;
            m_ValidationPhase = LFPG_VALIDATE_INDEX_REBUILD;
        }
    }
    protected void LFPG_ValidationRebuildIndexes()
    {
        bool fullRequiredBeforeIndex = m_GraphFullRebuildRequired;
        RebuildReverseIdx();
        RecountAllPlayerWires();
        if (!fullRequiredBeforeIndex && (m_ValidationSkipGraphRebuild || m_GraphRebuildGeneration != m_ValidationGraphGeneration) && m_ValidationOwnersPruned <= 0)
            m_GraphFullRebuildRequired = false;
        m_ValidationPhase = LFPG_VALIDATE_SCHEDULE_PRUNE;
    }
    protected void LFPG_ValidationScheduleDeferredPrune()
    {
        if (!m_DeferredPruneScheduled)
        {
            m_DeferredPruneScheduled = true;
            bool noRepeat = false;
            g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DeferredVanillaPruneAndRebuild, 30000, noRepeat);
            LFPG_Util.Info("[SelfHeal] Vanilla wire pruning deferred until after phased validation");
        }
        m_ValidationPhase = LFPG_VALIDATE_GRAPH_REBUILD;
    }
    protected void LFPG_ValidationRebuildGraph()
    {
        bool graphRebuiltAfterValidationStart = m_GraphRebuildGeneration != m_ValidationGraphGeneration;
        if (m_Graph && ((!m_ValidationSkipGraphRebuild && !graphRebuiltAfterValidationStart) || m_ValidationOwnersPruned > 0 || m_GraphFullRebuildRequired))
        {
			CaptureBatteryGraphState();
            m_Graph.RebuildFromWires(this);
			RestoreBatteryGraphState();
            m_GraphFullRebuildRequired = false;
            m_GraphRebuildGeneration = m_GraphRebuildGeneration + 1;
        }
        else if (m_Graph)
            LFPG_Util.Info("[SelfHeal] Graph rebuild skipped; preceding CutAll rebuild remains authoritative");
        m_ValidationSkipGraphRebuild = false;
        m_ValidationPhase = LFPG_VALIDATE_GRAPH_POPULATE;
    }
    protected void LFPG_ValidationPopulateGraph()
    {
        if (m_Graph)
            m_Graph.PopulateAllNodeElecStates();
        m_ValidationPhase = LFPG_VALIDATE_GRAPH_MARK;
    }
    protected void LFPG_ValidationMarkGraph()
    {
        if (m_Graph)
        {
            m_Graph.MarkSourcesDirty();
            m_WarmupActive = true;
            int flushBudget = LFPG_PROPAGATE_WARMUP_BUDGET;
            int flushEdge = LFPG_PROPAGATE_EDGE_WARMUP_BUDGET;
            m_Graph.ProcessDirtyQueue(flushBudget, flushEdge);
            LFPG_Util.Info("[SelfHeal] Graph warmup phase complete");
        }
        m_ValidationPhase = LFPG_VALIDATE_PRUNE_POSITIONS;
    }
    protected void LFPG_ValidationPrunePositions()
    {
        PruneStaleLastKnownPositions();
        m_ValidationPhase = LFPG_VALIDATE_REBUILD_TRACKED;
    }
    protected void LFPG_ValidationRebuildTracked()
    {
        RebuildTrackedDevices();
        m_ValidationPhase = LFPG_VALIDATE_FINALIZE;
    }
    protected void LFPG_ValidationFinalize()
    {
        int durationMs = g_Game.GetTime() - m_ValidationStartMs;
        int deviceCount = m_ValidationDevices.Count();
        m_ValidationActive = false;
        if (m_ValidationRerunRequested)
        {
            m_ValidationRerunRequested = false;
            LFPG_Util.Info("[SelfHeal] Changes arrived during validation; scheduling one more phased pass");
            ValidateAllWiresAndPropagate();
            return;
        }
        bool wasStartupPending = !m_StartupValidationDone;
        m_StartupValidationDone = true;
        if (wasStartupPending)
            LFPG_Util.Info("[SelfHeal] Startup validation done — RPCs enabled");
        string summary = "[SelfHeal] phased summary duration_ms=";
        summary = summary + durationMs.ToString();
        summary = summary + " devices=" + deviceCount.ToString();
        summary = summary + " owners_pruned=" + m_ValidationOwnersPruned.ToString();
        LFPG_Util.Info(summary);
        if (m_FullSyncPendingPlayers.Count() > 0)
            LFPG_StartNextFullSync();
    }
    protected void LFPG_ProcessStartupValidationSlice()
    {
        if (!m_ValidationActive)
            return;
        if (m_ValidationPhase == LFPG_VALIDATE_RESOLVE_VANILLA)
        {
            LFPG_ValidationResolveVanillaOwners();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_SNAPSHOT_PRE)
        {
            LFPG_ValidationSnapshotPre();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_RESOLVE_LFPG)
        {
            LFPG_ValidationResolveLFPGTargets();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_SNAPSHOT_FINAL)
        {
            LFPG_ValidationSnapshotFinal();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_BUILD_VALID)
        {
            LFPG_ValidationBuildValidIds();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_REFRESH_LFPG)
        {
            LFPG_ValidationRefreshLFPGNetworkIds();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_REFRESH_VANILLA)
        {
            LFPG_ValidationRefreshVanillaNetworkIds();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_PRUNE_LFPG)
        {
            LFPG_ValidationPruneLFPGOwners();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_INDEX_REBUILD)
        {
            LFPG_ValidationRebuildIndexes();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_SCHEDULE_PRUNE)
        {
            LFPG_ValidationScheduleDeferredPrune();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_GRAPH_REBUILD)
        {
            LFPG_ValidationRebuildGraph();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_GRAPH_POPULATE)
        {
            LFPG_ValidationPopulateGraph();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_GRAPH_MARK)
        {
            LFPG_ValidationMarkGraph();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_PRUNE_POSITIONS)
        {
            LFPG_ValidationPrunePositions();
            return;
        }
        if (m_ValidationPhase == LFPG_VALIDATE_REBUILD_TRACKED)
        {
            LFPG_ValidationRebuildTracked();
            return;
        }
        LFPG_ValidationFinalize();
    }
    override map<string, bool> GetCachedValidIds()
    {
        return m_CachedValidIds;
    }
    protected int PruneUnresolvableVanillaWires()
    {
        #ifdef SERVER
        int totalPruned = 0;
        ref array<string> emptyOwners = new array<string>;
        int vk;
        for (vk = 0; vk < m_VanillaWires.Count(); vk = vk + 1)
        {
            string ownerId = m_VanillaWires.GetKey(vk);
            EntityAI ownerObj = LFPG_DeviceRegistry.Get().FindById(ownerId);
            if (!ownerObj)
            {
                ownerObj = LFPG_DeviceAPI.ResolveVanillaDevice(ownerId);
            }
            if (!ownerObj)
            {
                ref array<ref LFPG_WireData> ownerWires = m_VanillaWires.GetElement(vk);
                int ownerCount = 0;
                if (ownerWires)
                {
                    ownerCount = ownerWires.Count();
                }
                totalPruned = totalPruned + ownerCount;
                emptyOwners.Insert(ownerId);
                continue;
            }
            ref array<ref LFPG_WireData> wires = m_VanillaWires.GetElement(vk);
            if (!wires)
                continue;
            int w = wires.Count() - 1;
            while (w >= 0)
            {
                LFPG_WireData wd = wires[w];
                if (!wd || wd.m_TargetDeviceId == "")
                {
                    wires.Remove(w);
                    totalPruned = totalPruned + 1;
                    w = w - 1;
                    continue;
                }
                EntityAI tObj = LFPG_DeviceRegistry.Get().FindById(wd.m_TargetDeviceId);
                if (!tObj)
                {
                    tObj = LFPG_DeviceAPI.ResolveVanillaDevice(wd.m_TargetDeviceId);
                }
                if (!tObj)
                {
                    string vpMsg = "[VanillaPrune] Removed wire " + ownerId + " -> " + wd.m_TargetDeviceId + " (target unresolvable)";
                    LFPG_Util.Debug(vpMsg);
                    wires.Remove(w);
                    totalPruned = totalPruned + 1;
                }
                w = w - 1;
            }
            if (wires.Count() == 0)
            {
                emptyOwners.Insert(ownerId);
            }
        }
        int eo;
        for (eo = 0; eo < emptyOwners.Count(); eo = eo + 1)
        {
            m_VanillaWires.Remove(emptyOwners[eo]);
        }
        if (totalPruned > 0)
        {
            string shPruneMsg = "[SelfHeal] Pruned " + totalPruned.ToString() + " unresolvable vanilla wire(s), " + emptyOwners.Count().ToString() + " empty owner(s)";
            LFPG_Util.Info(shPruneMsg);
            MarkVanillaDirty();
        }
        return totalPruned;
        #else
        return 0;
        #endif
    }
    protected void DeferredVanillaPruneAndRebuild()
    {
        #ifdef SERVER
        int deferredStartMs = g_Game.GetTime();
        int newlyResolved = 0;
        LFPG_Util.Info("[DeferredPrune] Starting deferred vanilla wire validation...");
        int vr;
        for (vr = 0; vr < m_VanillaWires.Count(); vr = vr + 1)
        {
            string vrOwnerId = m_VanillaWires.GetKey(vr);
            if (!LFPG_DeviceRegistry.Get().FindById(vrOwnerId))
            {
                EntityAI deferredOwnerResolved = LFPG_DeviceAPI.ResolveVanillaDevice(vrOwnerId);
                if (deferredOwnerResolved)
                {
                    newlyResolved = newlyResolved + 1;
                }
            }
            ref array<ref LFPG_WireData> vrWires = m_VanillaWires.GetElement(vr);
            if (vrWires)
            {
                int vrw;
                for (vrw = 0; vrw < vrWires.Count(); vrw = vrw + 1)
                {
                    LFPG_WireData vrWd = vrWires[vrw];
                    if (vrWd && vrWd.m_TargetDeviceId != "")
                    {
                        if (!LFPG_DeviceRegistry.Get().FindById(vrWd.m_TargetDeviceId))
                        {
                            EntityAI deferredTargetResolved = LFPG_DeviceAPI.ResolveVanillaDevice(vrWd.m_TargetDeviceId);
                            if (deferredTargetResolved)
                            {
                                newlyResolved = newlyResolved + 1;
                            }
                        }
                    }
                }
            }
        }
        array<EntityAI> allDev = new array<EntityAI>;
        LFPG_DeviceRegistry.Get().GetAll(allDev);
        int pa;
        for (pa = 0; pa < allDev.Count(); pa = pa + 1)
        {
            ref array<ref LFPG_WireData> lfWires = LFPG_DeviceAPI.GetDeviceWires(allDev[pa]);
            if (!lfWires) continue;
            int lw;
            for (lw = 0; lw < lfWires.Count(); lw = lw + 1)
            {
                LFPG_WireData lwd = lfWires[lw];
                if (!lwd) continue;
                string tid = lwd.m_TargetDeviceId;
                if (tid.IndexOf("vp:") == 0)
                {
                    if (!LFPG_DeviceRegistry.Get().FindById(tid))
                    {
                        EntityAI deferredLFPGTargetResolved = LFPG_DeviceAPI.ResolveVanillaDevice(tid);
                        if (deferredLFPGTargetResolved)
                        {
                            newlyResolved = newlyResolved + 1;
                        }
                    }
                }
            }
        }
        int vanillaPruneCount = PruneUnresolvableVanillaWires();
        LFPG_DeviceRegistry.Get().PruneNullEntries();
        array<EntityAI> allAfterPrune = new array<EntityAI>;
        LFPG_DeviceRegistry.Get().GetAll(allAfterPrune);
        m_CachedValidIds = new map<string, bool>;
        int vi;
        for (vi = 0; vi < allAfterPrune.Count(); vi = vi + 1)
        {
            string did = LFPG_DeviceAPI.GetOrCreateDeviceId(allAfterPrune[vi]);
            if (did != "")
            {
                m_CachedValidIds[did] = true;
            }
        }
        int pruneCount = 0;
        int pi;
        for (pi = 0; pi < allAfterPrune.Count(); pi = pi + 1)
        {
            if (!LFPG_DeviceAPI.HasWireStore(allAfterPrune[pi])) continue;
            bool changed = LFPG_DeviceAPI.PruneDeviceMissingTargets(allAfterPrune[pi]);
            if (changed)
            {
                pruneCount = pruneCount + 1;
                BroadcastOwnerWires(allAfterPrune[pi]);
            }
        }
        m_CachedValidIds = null;
        bool vanillaDirtyBeforeFlush = m_VanillaDirty;
        bool needsDeferredRebuild = false;
        if (newlyResolved > 0 || pruneCount > 0 || vanillaPruneCount > 0 || vanillaDirtyBeforeFlush)
        {
            needsDeferredRebuild = true;
        }
        if (m_VanillaDirty)
        {
            FlushVanillaIfDirty();
        }
        string deferredRebuildResult = "skipped";
        if (needsDeferredRebuild)
        {
            deferredRebuildResult = "executed";
            if (m_Graph)
            {
				CaptureBatteryGraphState();
                m_Graph.RebuildFromWires(this);
				RestoreBatteryGraphState();
                m_GraphRebuildGeneration = m_GraphRebuildGeneration + 1;
                m_Graph.PopulateAllNodeElecStates();
                m_Graph.MarkSourcesDirty();
                int flushBudget = LFPG_PROPAGATE_WARMUP_BUDGET;
                int flushEdge = LFPG_PROPAGATE_EDGE_WARMUP_BUDGET;
                m_Graph.ProcessDirtyQueue(flushBudget, flushEdge);
            }
            RebuildReverseIdx();
			RecountAllPlayerWires();
            m_GraphFullRebuildRequired = false;
            RebuildTrackedDevices();
        }
        else
        {
            LFPG_Util.Info("[DeferredPrune] rebuild=skipped newly_resolved=0 lfpg_pruned=0 vanilla_pruned=0 vanilla_dirty=0");
        }
        int deferredDurationMs = g_Game.GetTime() - deferredStartMs;
        int vanillaDirtySummary = 0;
        if (vanillaDirtyBeforeFlush)
        {
            vanillaDirtySummary = 1;
        }
        string deferredSummary = "[DeferredPrune] summary duration_ms=" + deferredDurationMs.ToString() + " newly_resolved=" + newlyResolved.ToString() + " lfpg_pruned=" + pruneCount.ToString() + " vanilla_pruned=" + vanillaPruneCount.ToString() + " vanilla_dirty_before_flush=" + vanillaDirtySummary.ToString() + " rebuild=" + deferredRebuildResult;
        LFPG_Util.Info(deferredSummary);
        #endif
    }
    protected void PruneStaleLastKnownPositions()
    {
        #ifdef SERVER
        if (m_LastKnownPos.Count() == 0)
            return;
        ref array<string> staleIds = new array<string>;
        int pk;
        for (pk = 0; pk < m_LastKnownPos.Count(); pk = pk + 1)
        {
            string posKey = m_LastKnownPos.GetKey(pk);
            EntityAI posObj = LFPG_DeviceRegistry.Get().FindById(posKey);
            if (!posObj)
            {
                posObj = LFPG_DeviceAPI.ResolveVanillaDevice(posKey);
            }
            if (!posObj)
            {
                staleIds.Insert(posKey);
            }
        }
        int sk;
        for (sk = 0; sk < staleIds.Count(); sk = sk + 1)
        {
            m_LastKnownPos.Remove(staleIds[sk]);
        }
        if (staleIds.Count() > 0)
        {
            string shStaleMsg = "[SelfHeal] Pruned " + staleIds.Count().ToString() + " stale position tracking entries";
            LFPG_Util.Debug(shStaleMsg);
        }
        #endif
    }
    override void MarkVanillaDirty()
    {
        #ifdef SERVER
        m_VanillaDirty = true;
        #endif
    }
    override void FlushVanillaIfDirty()
    {
        #ifdef SERVER
        if (!m_VanillaDirty)
            return;
        bool saveOk = SaveVanillaWires();
        if (saveOk)
        {
            m_VanillaDirty = false;
            m_VanillaSaveFailureCount = 0;
            return;
        }
        m_VanillaDirty = true;
        m_VanillaSaveFailureCount = m_VanillaSaveFailureCount + 1;
        int now = g_Game.GetTime();
        if (m_VanillaSaveFailureCount == 1 || now - m_LastVanillaSaveFailureWarnMs >= LFPG_VANILLA_SAVE_WARN_INTERVAL_MS)
        {
            string retryWarn = "[VanillaWires] Save failed; dirty state retained for retry. Cumulative failed flushes: ";
            retryWarn = retryWarn + m_VanillaSaveFailureCount.ToString();
            LFPG_Util.Warn(retryWarn);
            m_LastVanillaSaveFailureWarnMs = now;
        }
        #endif
    }
    override void FlushVanillaOnShutdown()
    {
        #ifdef SERVER
        if (m_VanillaDirty)
        {
            string vFlushMsg = "[VanillaWires] Flushing on shutdown...";
            LFPG_Util.Info(vFlushMsg);
            bool saveOk = SaveVanillaWires();
            if (saveOk)
            {
                m_VanillaDirty = false;
            }
            else
            {
                m_VanillaDirty = true;
                LFPG_Util.Warn("[VanillaWires] Shutdown flush failed; dirty state remains in memory.");
            }
        }
        #endif
    }
    protected bool SaveVanillaWires()
    {
        #ifdef SERVER
        if (m_VanillaReadOnly)
        {
            string saveBlockMsg = "[VanillaWires] SAVE BLOCKED: loaded from schema v" + m_VanillaLoadedVer.ToString() + " > current v" + LFPG_VANILLA_PERSIST_VER.ToString() + ". Upgrade the mod to save changes.";
            LFPG_Util.Warn(saveBlockMsg);
            return true;
        }
        if (!FileExist(VANILLA_WIRES_DIR))
            MakeDirectory(VANILLA_WIRES_DIR);
        LFPG_VanillaWireStore store = new LFPG_VanillaWireStore();
        int vk;
        for (vk = 0; vk < m_VanillaWires.Count(); vk = vk + 1)
        {
            string ownerId = m_VanillaWires.GetKey(vk);
            ref array<ref LFPG_WireData> wires = m_VanillaWires.GetElement(vk);
            if (!wires) continue;
            int w;
            for (w = 0; w < wires.Count(); w = w + 1)
            {
                LFPG_WireData wd = wires[w];
                if (!wd) continue;
                LFPG_VanillaWireEntry entry = new LFPG_VanillaWireEntry();
                entry.m_OwnerDeviceId = ownerId;
                entry.m_TargetDeviceId = wd.m_TargetDeviceId;
                entry.m_TargetPort = wd.m_TargetPort;
                entry.m_SourcePort = wd.m_SourcePort;
                entry.m_CreatorId = wd.m_CreatorId;
                if (wd.m_Waypoints && wd.m_Waypoints.Count() > 0)
                {
                    int wp;
                    for (wp = 0; wp < wd.m_Waypoints.Count(); wp = wp + 1)
                    {
                        entry.m_Waypoints.Insert(wd.m_Waypoints[wp]);
                    }
                }
                store.entries.Insert(entry);
            }
        }
        bool saveOk = LFPG_FileUtil.AtomicSaveVanillaWires(VANILLA_WIRES_FILE, store);
        if (saveOk)
        {
            string vSaveMsg = "[VanillaWires] Saved " + store.entries.Count().ToString() + " entries (atomic)";
            LFPG_Util.Info(vSaveMsg);
        }
        else
        {
            string vSaveErr = "[VanillaWires] Atomic save failed!";
            LFPG_Util.Error(vSaveErr);
        }
        return saveOk;
        #endif
        return true;
    }
    protected void LoadVanillaWires()
    {
        #ifdef SERVER
        if (!LFPG_FileUtil.EnsureVanillaWiresFileOrRestore(VANILLA_WIRES_FILE))
        {
            string vFreshMsg = "[VanillaWires] No saved file found, starting fresh.";
            LFPG_Util.Info(vFreshMsg);
            return;
        }
        LFPG_VanillaWireStore store = new LFPG_VanillaWireStore();
        string err;
        if (!JsonFileLoader<LFPG_VanillaWireStore>.LoadFile(VANILLA_WIRES_FILE, store, err))
        {
            string vLoadErr = "[VanillaWires] Load failed: " + err;
            LFPG_Util.Warn(vLoadErr);
            return;
        }
        if (!store.entries)
        {
            string vEmptyMsg = "[VanillaWires] Loaded empty store.";
            LFPG_Util.Info(vEmptyMsg);
            return;
        }
        m_VanillaLoadedVer = store.ver;
        if (m_VanillaLoadedVer > LFPG_VANILLA_PERSIST_VER)
        {
            m_VanillaReadOnly = true;
            string vSchemaMsg = "[VanillaWires] Schema v" + m_VanillaLoadedVer.ToString() + " > current v" + LFPG_VANILLA_PERSIST_VER.ToString() + ". Entering READ-ONLY mode to protect data. Upgrade the mod.";
            LFPG_Util.Warn(vSchemaMsg);
        }
        int loaded = 0;
        int discarded = 0;
        int duplicates = 0;
        ref map<string, ref map<string, bool>> dedupByOwner = new map<string, ref map<string, bool>>;
        int i;
        for (i = 0; i < store.entries.Count(); i = i + 1)
        {
            LFPG_VanillaWireEntry entry = store.entries[i];
            if (!entry) continue;
            if (entry.m_OwnerDeviceId == "" || entry.m_TargetDeviceId == "")
            {
                discarded = discarded + 1;
                continue;
            }
            LFPG_WireData wd = new LFPG_WireData();
            wd.m_TargetDeviceId = entry.m_TargetDeviceId;
            wd.m_TargetPort = entry.m_TargetPort;
            wd.m_SourcePort = entry.m_SourcePort;
            wd.m_CreatorId = entry.m_CreatorId;
            if (entry.m_Waypoints && entry.m_Waypoints.Count() > 0)
            {
                int wp;
                for (wp = 0; wp < entry.m_Waypoints.Count(); wp = wp + 1)
                {
                    wd.m_Waypoints.Insert(entry.m_Waypoints[wp]);
                }
            }
            if (!LFPG_WireHelper.ValidateWireData(wd, "VanillaWires"))
            {
                discarded = discarded + 1;
                continue;
            }
            ref map<string, bool> ownerDedup;
            if (!dedupByOwner.Find(entry.m_OwnerDeviceId, ownerDedup) || !ownerDedup)
            {
                ownerDedup = new map<string, bool>;
                dedupByOwner.Set(entry.m_OwnerDeviceId, ownerDedup);
            }
            string dedupKey = wd.m_TargetDeviceId + "|" + wd.m_TargetPort + "|" + wd.m_SourcePort;
            bool isDup = false;
            ownerDedup.Find(dedupKey, isDup);
            if (isDup)
            {
                duplicates = duplicates + 1;
                continue;
            }
            bool bDedup = true;
            ownerDedup.Set(dedupKey, bDedup);
            ref array<ref LFPG_WireData> wires;
            if (!m_VanillaWires.Find(entry.m_OwnerDeviceId, wires) || !wires)
            {
                wires = new array<ref LFPG_WireData>;
                m_VanillaWires[entry.m_OwnerDeviceId] = wires;
            }
            wires.Insert(wd);
            loaded = loaded + 1;
        }
        string loadMsg = "[VanillaWires] Loaded " + loaded.ToString() + " entries from " + store.entries.Count().ToString();
        if (discarded > 0)
        {
            loadMsg = loadMsg + " (discarded " + discarded.ToString() + " corrupt)";
        }
        if (duplicates > 0)
        {
            loadMsg = loadMsg + " (removed " + duplicates.ToString() + " duplicates)";
        }
        LFPG_Util.Info(loadMsg);
        #endif
    }
    override void CutAllWiresFromMovedDevice(EntityAI device, vector previousOwnerPosition, string knownDeviceId = "")
    {
        m_CutAllHasPreviousOwnerPosition = true;
        m_CutAllPreviousOwnerPosition = previousOwnerPosition;
        CutAllWiresFromDevice(device, knownDeviceId);
        m_CutAllHasPreviousOwnerPosition = false;
    }
	override void CutAllWiresFromDevice(EntityAI device, string knownDeviceId = "")
    {
        #ifdef SERVER
        if (!device)
            return;
        string deviceId = knownDeviceId;
        if (deviceId == "")
            deviceId = LFPG_DeviceAPI.GetDeviceId(device);
        if (deviceId == "")
            return;
        bool anyChanged = false;
        bool reverseIndexConsistent = false;
        ref map<string, int> graphIncomingByPort = new map<string, int>;
        int portCount = LFPG_DeviceAPI.GetPortCount(device);
        ref map<string, bool> declaredInputPorts = new map<string, bool>;
        int declaredPortIndex;
        for (declaredPortIndex = 0; declaredPortIndex < portCount; declaredPortIndex = declaredPortIndex + 1)
        {
            if (LFPG_DeviceAPI.GetPortDir(device, declaredPortIndex) != LFPG_PortDir.IN)
                continue;
            string declaredPortName = LFPG_DeviceAPI.GetPortName(device, declaredPortIndex);
            if (declaredPortName == "")
                declaredPortName = "input_main";
            declaredInputPorts.Set(deviceId + "|" + declaredPortName, true);
        }
        ref array<string> neighborIds = new array<string>;
        if (m_Graph)
        {
            reverseIndexConsistent = m_ReverseIndexTrusted;
            ref array<ref LFPG_ElecEdge> preOutEdges = m_Graph.GetOutgoing(deviceId);
            if (preOutEdges)
            {
                int poi;
                for (poi = 0; poi < preOutEdges.Count(); poi = poi + 1)
                {
                    ref LFPG_ElecEdge poEdge = preOutEdges[poi];
                    if (poEdge && poEdge.m_TargetNodeId != "")
                    {
                        neighborIds.Insert(poEdge.m_TargetNodeId);
                    }
                }
            }
            ref array<ref LFPG_ElecEdge> preInEdges = m_Graph.GetIncoming(deviceId);
            if (preInEdges)
            {
                int pii;
                for (pii = 0; pii < preInEdges.Count(); pii = pii + 1)
                {
                    ref LFPG_ElecEdge piEdge = preInEdges[pii];
                    if (piEdge && piEdge.m_SourceNodeId != "")
                    {
                        neighborIds.Insert(piEdge.m_SourceNodeId);
                        string incomingPort = piEdge.m_TargetPort;
                        if (incomingPort == "")
                        {
                            incomingPort = "input_main";
                            reverseIndexConsistent = false;
                        }
                        string incomingKey = deviceId + "|" + incomingPort;
                        bool declaredInputPort = false;
                        if (!declaredInputPorts.Find(incomingKey, declaredInputPort) || !declaredInputPort)
                            reverseIndexConsistent = false;
                        int graphPortCount = 0;
                        graphIncomingByPort.Find(incomingKey, graphPortCount);
                        graphIncomingByPort.Set(incomingKey, graphPortCount + 1);
                        ref array<string> indexedOwners;
                        if (!m_ReverseOwners.Find(incomingKey, indexedOwners) || !indexedOwners)
                        {
                            reverseIndexConsistent = false;
                        }
                        else
                        {
                            bool ownerFound = false;
                            int ownerIndex;
                            for (ownerIndex = 0; ownerIndex < indexedOwners.Count(); ownerIndex = ownerIndex + 1)
                            {
                                if (indexedOwners[ownerIndex] == piEdge.m_SourceNodeId)
                                {
                                    ownerFound = true;
                                    break;
                                }
                            }
                            if (!ownerFound)
                                reverseIndexConsistent = false;
                        }
                    }
                }
            }
        }
        int graphPortIndex;
        for (graphPortIndex = 0; graphPortIndex < graphIncomingByPort.Count(); graphPortIndex = graphPortIndex + 1)
        {
            string graphPortKey = graphIncomingByPort.GetKey(graphPortIndex);
            int graphPortTotal = graphIncomingByPort.GetElement(graphPortIndex);
            int indexedPortTotal = 0;
            m_ReverseIdx.Find(graphPortKey, indexedPortTotal);
            if (indexedPortTotal != graphPortTotal)
                reverseIndexConsistent = false;
        }
        int healthPortIndex;
        for (healthPortIndex = 0; healthPortIndex < portCount; healthPortIndex = healthPortIndex + 1)
        {
            if (LFPG_DeviceAPI.GetPortDir(device, healthPortIndex) != LFPG_PortDir.IN)
                continue;
            string healthPortName = LFPG_DeviceAPI.GetPortName(device, healthPortIndex);
            if (healthPortName == "")
                healthPortName = "input_main";
            string healthPortKey = deviceId + "|" + healthPortName;
            int graphHealthCount = 0;
            graphIncomingByPort.Find(healthPortKey, graphHealthCount);
            int indexedHealthCount = CountWiresTargeting(deviceId, healthPortName);
            if (indexedHealthCount != graphHealthCount)
                reverseIndexConsistent = false;
        }
        m_CutAllGraphBatchActive = true;
        if (LFPG_DeviceAPI.HasWireStore(device))
        {
            ref array<ref LFPG_WireData> ownedWires = LFPG_DeviceAPI.GetDeviceWires(device);
            if (ownedWires && ownedWires.Count() > 0)
            {
                array<vector> ownedTargetPositions = new array<vector>;
                EntityAI ownedTarget;
                bool ownedBroadcastAll = false;
                int ow = ownedWires.Count() - 1;
                while (ow >= 0)
                {
                    LFPG_WireData wd = ownedWires[ow];
                    if (wd)
                    {
                        ownedTarget = ResolveOwnerSnapshotTarget(wd);
                        if (ownedTarget)
                            ownedTargetPositions.Insert(ownedTarget.GetPosition());
                        else
                            ownedBroadcastAll = true;
                        ReverseIdxRemove(wd.m_TargetDeviceId, wd.m_TargetPort, deviceId);
                        PlayerWireCountAdd(wd.m_CreatorId, -1);
                    }
                    ow = ow - 1;
                }
                LFPG_DeviceAPI.ClearDeviceWires(device);
                QueueBroadcastOwnerSnapshot(device, ownedTargetPositions, ownedBroadcastAll);
                anyChanged = true;
            }
        }
        if (deviceId.IndexOf("vp:") == 0)
        {
            ref array<ref LFPG_WireData> vWires;
            if (m_VanillaWires.Find(deviceId, vWires) && vWires && vWires.Count() > 0)
            {
                int vw = vWires.Count() - 1;
                while (vw >= 0)
                {
                    LFPG_WireData vwd = vWires[vw];
                    if (vwd)
                    {
                        ReverseIdxRemove(vwd.m_TargetDeviceId, vwd.m_TargetPort, deviceId);
                        PlayerWireCountAdd(vwd.m_CreatorId, -1);
                    }
                    vw = vw - 1;
                }
                vWires.Clear();
                MarkVanillaDirty();
                QueueBroadcastVanilla(deviceId, device);
                anyChanged = true;
            }
        }
        int pi;
        for (pi = 0; pi < portCount; pi = pi + 1)
        {
            int portDir = LFPG_DeviceAPI.GetPortDir(device, pi);
            if (portDir == LFPG_PortDir.IN)
            {
                string portName = LFPG_DeviceAPI.GetPortName(device, pi);
                int removed = RemoveWiresTargeting(deviceId, portName);
                if (removed > 0)
                {
                    anyChanged = true;
                    if (LFPG_LOG_LEVEL >= 1)
                    {
                        string cutMsg = "[CutAll] Removed " + removed.ToString() + " incoming wire(s) on " + deviceId + ":" + portName;
                        LFPG_Util.Info(cutMsg);
                    }
                }
            }
        }
        if (!m_ReverseIndexTrusted)
            reverseIndexConsistent = false;
        if (!reverseIndexConsistent)
        {
            LFPG_Util.Warn("[CutAll] Reverse index mismatch detected; running global fallback scan");
            m_ReusableCutAllDevices.Clear();
            LFPG_DeviceRegistry.Get().GetAll(m_ReusableCutAllDevices);
            int di;
            for (di = 0; di < m_ReusableCutAllDevices.Count(); di = di + 1)
            {
                EntityAI srcDev = m_ReusableCutAllDevices[di];
                if (!srcDev)
                    continue;
                if (srcDev == device)
                    continue;
                if (!LFPG_DeviceAPI.HasWireStore(srcDev))
                    continue;
                string srcId = LFPG_DeviceAPI.GetDeviceId(srcDev);
                ref array<ref LFPG_WireData> srcWires = LFPG_DeviceAPI.GetDeviceWires(srcDev);
                if (!srcWires)
                    continue;
                bool srcChanged = false;
                m_ReusableCutAllFallbackWires.Clear();
                int sw = srcWires.Count() - 1;
                while (sw >= 0)
                {
                    LFPG_WireData swd = srcWires[sw];
                    if (swd && swd.m_TargetDeviceId == deviceId)
                    {
                        string cutFbMsg = "[CutAll-Fallback] Found stale wire: " + srcId + " -> " + deviceId;
                        LFPG_Util.Warn(cutFbMsg);
                        PlayerWireCountAdd(swd.m_CreatorId, -1);
                        m_ReusableCutAllFallbackWires.Insert(swd);
                        srcWires.Remove(sw);
                        srcChanged = true;
                        anyChanged = true;
                    }
                    sw = sw - 1;
                }
                if (srcChanged)
                {
                    LFPG_WireOwnerBase srcWireOwner = LFPG_WireOwnerBase.Cast(srcDev);
                    if (srcWireOwner)
                    {
                        srcWireOwner.LFPG_CommitWireMutation();
                    }
                    srcDev.SetSynchDirty();
                    QueueBroadcastOwnerSnapshotFromWires(srcDev, m_ReusableCutAllFallbackWires);
                    m_ReusableCutAllFallbackWires.Clear();
                }
            }
            int fallbackVanillaOwnerIndex;
            for (fallbackVanillaOwnerIndex = 0; fallbackVanillaOwnerIndex < m_VanillaWires.Count(); fallbackVanillaOwnerIndex = fallbackVanillaOwnerIndex + 1)
            {
                string fallbackVanillaOwnerId = m_VanillaWires.GetKey(fallbackVanillaOwnerIndex);
                ref array<ref LFPG_WireData> fallbackVanillaWires = m_VanillaWires.GetElement(fallbackVanillaOwnerIndex);
                if (!fallbackVanillaWires)
                    continue;
                bool fallbackVanillaChanged = false;
                int fallbackVanillaWireIndex = fallbackVanillaWires.Count() - 1;
                while (fallbackVanillaWireIndex >= 0)
                {
                    LFPG_WireData fallbackVanillaWire = fallbackVanillaWires[fallbackVanillaWireIndex];
                    if (fallbackVanillaWire && fallbackVanillaWire.m_TargetDeviceId == deviceId)
                    {
                        string fallbackVanillaMsg = "[CutAll-Fallback] Found stale vanilla wire: " + fallbackVanillaOwnerId + " -> " + deviceId;
                        LFPG_Util.Warn(fallbackVanillaMsg);
                        PlayerWireCountAdd(fallbackVanillaWire.m_CreatorId, -1);
                        fallbackVanillaWires.Remove(fallbackVanillaWireIndex);
                        fallbackVanillaChanged = true;
                        anyChanged = true;
                    }
                    fallbackVanillaWireIndex = fallbackVanillaWireIndex - 1;
                }
                if (fallbackVanillaChanged)
                {
                    MarkVanillaDirty();
                    EntityAI fallbackVanillaOwner = LFPG_DeviceRegistry.Get().FindById(fallbackVanillaOwnerId);
                    if (!fallbackVanillaOwner)
                        fallbackVanillaOwner = LFPG_DeviceAPI.ResolveVanillaDevice(fallbackVanillaOwnerId);
                    if (fallbackVanillaOwner)
                        QueueBroadcastVanilla(fallbackVanillaOwnerId, fallbackVanillaOwner);
                }
            }
        }
        if (!reverseIndexConsistent)
        {
            m_ReverseIndexTrusted = false;
            m_IndexHealAfterCut = true;
        }
        m_CutAllGraphBatchActive = false;
        m_LastKnownPos.Remove(deviceId);
        EntityAI neighborDev;
        if (anyChanged)
        {
            m_CutPendingPowerOff.Set(deviceId, true);
            int nbi;
            for (nbi = 0; nbi < neighborIds.Count(); nbi = nbi + 1)
                m_CutPendingPowerOff.Set(neighborIds[nbi], true);
        }
        if (anyChanged)
        {
            if (m_Graph)
            {
                m_Graph.OnDeviceRemoved(deviceId);
            }
            if (!m_CutGraphRebuildQueued)
            {
                m_CutGraphRebuildQueued = true;
                bool cutRebuildOnce = false;
                g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(PostBulkRebuildAndPropagate, 1, cutRebuildOnce);
            }
            if (LFPG_LOG_LEVEL >= 1)
            {
                string cutAllMsg = "[CutAll] All wires removed for device " + deviceId + " type=" + device.GetType();
                LFPG_Util.Info(cutAllMsg);
            }
            if (m_VanillaDirty)
            {
                FlushVanillaIfDirty();
            }
        }
        else if (m_IndexHealAfterCut && !m_CutGraphRebuildQueued)
        {
            m_IndexHealAfterCut = false;
            RequestGlobalSelfHeal();
        }
        UntrackDeviceFromPolling(deviceId);
        int nui;
        for (nui = 0; nui < neighborIds.Count(); nui = nui + 1)
        {
            string neighborId = neighborIds[nui];
            neighborDev = LFPG_DeviceRegistry.Get().FindById(neighborId);
            if (neighborDev)
            {
                if (!DeviceHasAnyWires(neighborDev, neighborId))
                {
                    UntrackDeviceFromPolling(neighborId);
                }
            }
            else
            {
                UntrackDeviceFromPolling(neighborId);
            }
        }
        #endif
    }
    override bool LFPG_GetCachedSunState()
    {
        return m_SolarHasSun;
    }
    override float LFPG_GetBTCPrice()
    {
        if (m_BTCPriceFetcher)
        {
            return m_BTCPriceFetcher.GetCachedPrice();
        }
        return LFPG_BTC_PRICE_UNAVAILABLE;
    }
    override bool LFPG_IsBTCPriceAvailable()
    {
        if (m_BTCPriceFetcher)
        {
            return m_BTCPriceFetcher.IsPriceAvailable();
        }
        return false;
    }
    override float LFPG_GetBTC24hChange()
    {
        if (m_BTCPriceFetcher)
        {
            return m_BTCPriceFetcher.Get24hChangePercent();
        }
        return 0.0;
    }
    protected void LFPG_ComputeSunState()
    {
        #ifdef SERVER
        if (!g_Game)
            return;
        World world = g_Game.GetWorld();
        if (!world)
            return;
        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        world.GetDate(year, month, day, hour, minute);
        bool hasSun = false;
        if (hour >= LFPG_SOLAR_DAWN_HOUR && hour < LFPG_SOLAR_DUSK_HOUR)
        {
            hasSun = true;
        }
        m_SolarHasSun = hasSun;
        #endif
    }
    protected void LFPG_TickBTCPrice()
    {
        #ifdef SERVER
        if (m_BTCPriceFetcher)
        {
            m_BTCPriceFetcher.Tick();
        }
        #endif
    }
    protected void LFPG_TickSolarPanels()
    {
        #ifdef SERVER
        bool prevSun = m_SolarHasSun;
        LFPG_ComputeSunState();
        if (m_SolarHasSun == prevSun)
            return;
        int total = m_RegisteredSolars.Count();
        if (total == 0)
            return;
        int i;
        int updated = 0;
        LFPG_SolarPanel panel;
        for (i = 0; i < total; i = i + 1)
        {
            if (i >= m_RegisteredSolars.Count())
                break;
            panel = LFPG_SolarPanel.Cast(m_RegisteredSolars[i]);
            if (!panel)
                continue;
            panel.LFPG_UpdateSunState(m_SolarHasSun);
            updated = updated + 1;
        }
        string msg = "[Solar] Sun changed to ";
        msg = msg + m_SolarHasSun.ToString();
        msg = msg + ", updated ";
        msg = msg + updated.ToString();
        msg = msg + " panels";
        LFPG_Util.Info(msg);
        #endif
    }
    override void LFPG_RefreshPumpSprinklerLink(string sourceId, string removedTargetId)
    {
        #ifdef SERVER
        LFPG_DeviceRegistry reg = LFPG_DeviceRegistry.Get();
        EntityAI srcEnt = reg.FindById(sourceId);
        if (!srcEnt)
            return;
        LFPG_WaterPump rp1 = LFPG_WaterPump.Cast(srcEnt);
        LFPG_WaterPump_T2 rp2 = LFPG_WaterPump_T2.Cast(srcEnt);
        if (!rp1 && !rp2)
            return;
        if (removedTargetId != "")
        {
            EntityAI removedEnt = reg.FindById(removedTargetId);
            if (removedEnt)
            {
                LFPG_Sprinkler removedSpr = LFPG_Sprinkler.Cast(removedEnt);
                if (removedSpr)
                {
                    string curSource = removedSpr.LFPG_GetWaterSourceId();
                    if (curSource == sourceId)
                    {
                        removedSpr.LFPG_SetHasWaterSource(false);
                        removedSpr.LFPG_SetSprinklerActive(false);
                        string emptyId = "";
                        removedSpr.LFPG_SetWaterSourceId(emptyId);
                    }
                }
            }
        }
        array<ref LFPG_WireData> rpWires;
        bool rpPowered;
        float rpTank = 0.0;
        if (rp1)
        {
            rpWires = rp1.LFPG_GetWires();
            rpPowered = rp1.LFPG_GetPoweredNet();
        }
        else
        {
            rpWires = rp2.LFPG_GetWires();
            rpPowered = rp2.LFPG_GetPoweredNet();
            rpTank = rp2.LFPG_GetTankLevel();
        }
        int rpSprCount = 0;
        int rwi;
        int rpWireCount = rpWires.Count();
        LFPG_WireData rpWd;
        string rpTid;
        EntityAI rpTEnt;
        LFPG_Sprinkler rpTSpr;
        bool rpSprActive;
        for (rwi = 0; rwi < rpWireCount; rwi = rwi + 1)
        {
            rpWd = rpWires[rwi];
            if (!rpWd)
                continue;
            rpTid = rpWd.m_TargetDeviceId;
            if (rpTid == "")
                continue;
            if (rpTid == removedTargetId)
                continue;
            rpTEnt = reg.FindById(rpTid);
            if (!rpTEnt)
                continue;
            rpTSpr = LFPG_Sprinkler.Cast(rpTEnt);
            if (!rpTSpr)
                continue;
            rpSprCount = rpSprCount + 1;
            rpTSpr.LFPG_SetHasWaterSource(true);
            rpTSpr.LFPG_SetWaterSourceId(sourceId);
            if (rp1)
            {
                rpTSpr.LFPG_SetSprinklerActive(rpPowered);
            }
        }
        bool rpHasSpr = false;
        if (rp1)
        {
            if (rpSprCount > 0)
            {
                rpHasSpr = true;
            }
            rp1.LFPG_SetHasSprinklerOutput(rpHasSpr);
        }
        else
        {
            rp2.LFPG_SetConnectedSprinklerCount(rpSprCount);
            rpSprActive = false;
            if (rpPowered)
            {
                if (rpSprCount <= 2)
                {
                    rpSprActive = true;
                }
                else if (rpTank > 0.0)
                {
                    rpSprActive = true;
                }
            }
            for (rwi = 0; rwi < rpWireCount; rwi = rwi + 1)
            {
                rpWd = rpWires[rwi];
                if (!rpWd)
                    continue;
                rpTid = rpWd.m_TargetDeviceId;
                if (rpTid == "")
                    continue;
                if (rpTid == removedTargetId)
                    continue;
                rpTEnt = reg.FindById(rpTid);
                if (!rpTEnt)
                    continue;
                rpTSpr = LFPG_Sprinkler.Cast(rpTEnt);
                if (!rpTSpr)
                    continue;
                rpTSpr.LFPG_SetSprinklerActive(rpSprActive);
            }
        }
        #endif
    }
    protected void LFPG_InitTankFillTime()
    {
        #ifdef SERVER
        m_TankFillLastMs = g_Game.GetTime();
        #endif
    }
    protected void LFPG_TickWaterPumps()
    {
        #ifdef SERVER
        float nowMs = g_Game.GetTime();
        float thresholdMs = LFPG_PUMP_FILTER_INTERVAL_MS;
        float fillAmount = 0.0;
        bool doTankFill = false;
        if (m_TankFillLastMs >= 0.0)
        {
            float elapsedFillMs = nowMs - m_TankFillLastMs;
            if (elapsedFillMs > 0.0)
            {
                fillAmount = (elapsedFillMs / 3600000.0) * LFPG_PUMP_TANK_FILL_PER_HOUR;
                m_TankFillLastMs = nowMs;
                if (fillAmount > 0.001)
                {
                    doTankFill = true;
                }
            }
        }
        if (m_RegisteredSprinklers.Count() == 0 && m_RegisteredT1Pumps.Count() == 0 && m_RegisteredT2Pumps.Count() == 0)
        {
            return;
        }
        int i;
        int sprTotal = m_RegisteredSprinklers.Count();
        LFPG_Sprinkler castSpr;
        float elapsed;
        for (i = 0; i < sprTotal; i = i + 1)
        {
            if (i >= m_RegisteredSprinklers.Count())
                break;
            castSpr = LFPG_Sprinkler.Cast(m_RegisteredSprinklers[i]);
            if (!castSpr)
                continue;
            castSpr.LFPG_SetHasWaterSource(false);
            castSpr.LFPG_SetSprinklerActive(false);
        }
        int t1Total = m_RegisteredT1Pumps.Count();
        LFPG_WaterPump castT1;
        for (i = 0; i < t1Total; i = i + 1)
        {
            if (i >= m_RegisteredT1Pumps.Count())
                break;
            castT1 = LFPG_WaterPump.Cast(m_RegisteredT1Pumps[i]);
            if (!castT1)
                continue;
            elapsed = nowMs - castT1.LFPG_GetFilterLastMs();
            if (elapsed >= thresholdMs)
            {
                castT1.LFPG_DegradeFilter();
                castT1.LFPG_SetFilterLastMs(nowMs);
            }
        }
        int t2Total = m_RegisteredT2Pumps.Count();
        LFPG_WaterPump_T2 castT2;
        for (i = 0; i < t2Total; i = i + 1)
        {
            if (i >= m_RegisteredT2Pumps.Count())
                break;
            castT2 = LFPG_WaterPump_T2.Cast(m_RegisteredT2Pumps[i]);
            if (!castT2)
                continue;
            elapsed = nowMs - castT2.LFPG_GetFilterLastMs();
            if (elapsed >= thresholdMs)
            {
                castT2.LFPG_DegradeFilter();
                castT2.LFPG_SetFilterLastMs(nowMs);
            }
        }
        LFPG_DeviceRegistry reg = LFPG_DeviceRegistry.Get();
        int t1Count = m_RegisteredT1Pumps.Count();
        int pi;
        int wi;
        int wireCount;
        int sprCount;
        LFPG_WaterPump curT1;
        array<ref LFPG_WireData> wires;
        LFPG_WireData wd;
        string targetId;
        EntityAI targetEnt;
        LFPG_Sprinkler targetSpr;
        bool pumpPowered;
        string pumpId;
        bool hasSprOut;
        for (pi = 0; pi < t1Count; pi = pi + 1)
        {
            if (pi >= m_RegisteredT1Pumps.Count())
                break;
            curT1 = LFPG_WaterPump.Cast(m_RegisteredT1Pumps[pi]);
            if (!curT1)
                continue;
            wires = curT1.LFPG_GetWires();
            wireCount = wires.Count();
            sprCount = 0;
            pumpPowered = curT1.LFPG_GetPoweredNet();
            pumpId = curT1.LFPG_GetDeviceId();
            for (wi = 0; wi < wireCount; wi = wi + 1)
            {
                wd = wires[wi];
                if (!wd)
                    continue;
                targetId = wd.m_TargetDeviceId;
                if (targetId == "")
                    continue;
                targetEnt = reg.FindById(targetId);
                if (!targetEnt)
                    continue;
                targetSpr = LFPG_Sprinkler.Cast(targetEnt);
                if (!targetSpr)
                    continue;
                sprCount = sprCount + 1;
                targetSpr.LFPG_SetHasWaterSource(true);
                targetSpr.LFPG_SetWaterSourceId(pumpId);
                targetSpr.LFPG_SetSprinklerActive(pumpPowered);
            }
            hasSprOut = false;
            if (sprCount > 0)
            {
                hasSprOut = true;
            }
            curT1.LFPG_SetHasSprinklerOutput(hasSprOut);
        }
        int t2Count = m_RegisteredT2Pumps.Count();
        LFPG_WaterPump_T2 curT2B;
        bool sprActive;
        float curTank;
        float level;
        float sprDrainFactor;
        float netFactor;
        float netFill;
        int incomingType;
        int currentType;
        for (pi = 0; pi < t2Count; pi = pi + 1)
        {
            if (pi >= m_RegisteredT2Pumps.Count())
                break;
            curT2B = LFPG_WaterPump_T2.Cast(m_RegisteredT2Pumps[pi]);
            if (!curT2B)
                continue;
            wires = curT2B.LFPG_GetWires();
            wireCount = wires.Count();
            sprCount = 0;
            pumpPowered = curT2B.LFPG_GetPoweredNet();
            pumpId = curT2B.LFPG_GetDeviceId();
            curTank = curT2B.LFPG_GetTankLevel();
            for (wi = 0; wi < wireCount; wi = wi + 1)
            {
                wd = wires[wi];
                if (!wd)
                    continue;
                targetId = wd.m_TargetDeviceId;
                if (targetId == "")
                    continue;
                targetEnt = reg.FindById(targetId);
                if (!targetEnt)
                    continue;
                targetSpr = LFPG_Sprinkler.Cast(targetEnt);
                if (!targetSpr)
                    continue;
                sprCount = sprCount + 1;
                targetSpr.LFPG_SetHasWaterSource(true);
                targetSpr.LFPG_SetWaterSourceId(pumpId);
            }
            curT2B.LFPG_SetConnectedSprinklerCount(sprCount);
            sprActive = false;
            if (pumpPowered)
            {
                if (sprCount <= 2)
                {
                    sprActive = true;
                }
                else if (curTank > 0.0)
                {
                    sprActive = true;
                }
            }
            for (wi = 0; wi < wireCount; wi = wi + 1)
            {
                wd = wires[wi];
                if (!wd)
                    continue;
                targetId = wd.m_TargetDeviceId;
                if (targetId == "")
                    continue;
                targetEnt = reg.FindById(targetId);
                if (!targetEnt)
                    continue;
                targetSpr = LFPG_Sprinkler.Cast(targetEnt);
                if (!targetSpr)
                    continue;
                targetSpr.LFPG_SetSprinklerActive(sprActive);
            }
            if (doTankFill && pumpPowered)
            {
                level = curT2B.LFPG_GetTankLevel();
                sprDrainFactor = sprCount * 0.5;
                netFactor = 1.0 - sprDrainFactor;
                netFill = fillAmount * netFactor;
                level = level + netFill;
                if (level < 0.0)
                {
                    level = 0.0;
                }
                if (level > LFPG_PUMP_TANK_MAX)
                {
                    level = LFPG_PUMP_TANK_MAX;
                }
                if (netFill > 0.0)
                {
                    incomingType = LIQUID_RIVERWATER;
                    if (LFPG_PumpHelper.HasActiveFilter(curT2B))
                    {
                        incomingType = LIQUID_CLEANWATER;
                    }
                    currentType = curT2B.LFPG_GetTankLiquidType();
                    if (level < 0.01)
                    {
                        curT2B.LFPG_SetTankLiquidType(incomingType);
                    }
                    else if (incomingType != currentType)
                    {
                        curT2B.LFPG_SetTankLiquidType(LIQUID_RIVERWATER);
                    }
                }
                curT2B.LFPG_SetTankLevel(level);
            }
        }
        #endif
    }
    override void RegisterSolar(LFPG_SolarPanel panel)
    {
        if (!panel)
            return;
        if (m_RegisteredSolars.Find(panel) < 0)
        {
            m_RegisteredSolars.Insert(panel);
        }
    }
    override void UnregisterSolar(LFPG_SolarPanel panel)
    {
        if (!panel)
            return;
        int idx = m_RegisteredSolars.Find(panel);
        if (idx >= 0)
        {
            m_RegisteredSolars.Remove(idx);
        }
    }
    override void RegisterT1Pump(LFPG_WaterPump pump)
    {
        if (!pump)
            return;
        if (m_RegisteredT1Pumps.Find(pump) < 0)
        {
            m_RegisteredT1Pumps.Insert(pump);
        }
    }
    override void UnregisterT1Pump(LFPG_WaterPump pump)
    {
        if (!pump)
            return;
        int idx = m_RegisteredT1Pumps.Find(pump);
        if (idx >= 0)
        {
            m_RegisteredT1Pumps.Remove(idx);
        }
    }
    override void RegisterT2Pump(LFPG_WaterPump_T2 pump)
    {
        if (!pump)
            return;
        if (m_RegisteredT2Pumps.Find(pump) < 0)
        {
            m_RegisteredT2Pumps.Insert(pump);
        }
    }
    override void UnregisterT2Pump(LFPG_WaterPump_T2 pump)
    {
        if (!pump)
            return;
        int idx = m_RegisteredT2Pumps.Find(pump);
        if (idx >= 0)
        {
            m_RegisteredT2Pumps.Remove(idx);
        }
    }
    override void RegisterSprinkler(LFPG_Sprinkler spr)
    {
        if (!spr)
            return;
        if (m_RegisteredSprinklers.Find(spr) < 0)
        {
            m_RegisteredSprinklers.Insert(spr);
            m_RegisteredSprinklerPhases.Insert(m_NextSprinklerPhase);
            m_NextSprinklerPhase = m_NextSprinklerPhase + 1;
            if (m_NextSprinklerPhase >= 10)
                m_NextSprinklerPhase = 0;
        }
    }
    override void UnregisterSprinkler(LFPG_Sprinkler spr)
    {
        if (!spr)
            return;
        int idx = m_RegisteredSprinklers.Find(spr);
        if (idx >= 0)
        {
            m_RegisteredSprinklers.Remove(idx);
            m_RegisteredSprinklerPhases.Remove(idx);
        }
    }
    override void RegisterSorter(LFPG_Sorter sorter)
    {
        if (!sorter)
            return;
        if (m_RegisteredSorters.Find(sorter) < 0)
        {
            m_RegisteredSorters.Insert(sorter);
			LFPG_SorterResumeState resumeState = new LFPG_SorterResumeState;
			m_SorterResumes.Insert(resumeState);
        }
    }
    override void UnregisterSorter(LFPG_Sorter sorter)
    {
        if (!sorter)
            return;
        int idx = m_RegisteredSorters.Find(sorter);
        if (idx >= 0)
        {
            m_RegisteredSorters.Remove(idx);
			m_SorterResumes.Remove(idx);
            if (m_SorterCursor > idx)
                m_SorterCursor = m_SorterCursor - 1;
        }
    }
	protected void LFPG_ClearSorterResume(int sorterIndex)
	{
		if (sorterIndex < 0 || sorterIndex >= m_SorterResumes.Count())
			return;
		m_SorterResumes[sorterIndex].Clear();
	}
	protected int CollectSorterOutputs(LFPG_Sorter sorter, array<EntityAI> destinations)
	{
		destinations.Clear();
		int wireMask = 0;
		int portBit = 1;
		for (int portIndex = 0; portIndex < 6; portIndex = portIndex + 1)
		{
			EntityAI destination = LFPG_SorterLogic.ResolveOutputContainer(sorter, portIndex);
			destinations.Insert(destination);
			if (destination)
				wireMask = wireMask | portBit;
			portBit = portBit * 2;
		}
		return wireMask;
	}
    protected void LFPG_TickSorters()
    {
        #ifdef SERVER
        int total;
        int batchEnd;
        int sorterIndex;
        int itemIndex;
        int outputIndex;
        int hasWireMask;
        int itemCount;
        int moved;
        int evaluated;
        int resumeItemIndex;
        int resumeOutput;
        int resumeRule;
        int nextOutput;
        int nextRule;
        int itemRuleChecks;
        int itemConfigMisses;
        int budgetRemaining;
        int ruleChecksTick;
        int configMissesTick;
        int deferralsTick;
        int dirtyDestCommitCount;
        int dirtySourceCommitCount;
        int dirtyDestIndex;
        int dirtySourceIndex;
        bool moveResult;
        bool itemDeferred;
        bool budgetExhausted;
        bool storedResumeValid;
		int cacheEnd;
		int wireGeneration;
		LFPG_SorterResumeState resumeState;
        LFPG_Sorter sorter;
        LFPG_SortConfig filterConfig;
        LFPG_SortConfig resumeConfig;
        EntityAI inputContainer;
        CargoBase inputCargo;
        EntityAI destinationContainer;
        EntityAI sortItem;
        EntityAI resumeItem;
        EntityAI dirtyDestination;
        EntityAI dirtySource;
        float linkDistanceSq;
        float linkRadiusSq;
        total = m_RegisteredSorters.Count();
        if (total == 0)
        {
            m_SorterCursor = 0;
            return;
        }
        if (m_SorterCursor >= total)
            m_SorterCursor = 0;
        batchEnd = m_SorterCursor + LFPG_SORTER_BATCH_SIZE;
        if (batchEnd > total)
            batchEnd = total;
        m_TickAffectedContainers.Clear();
        m_TickDirtyDestinations.Clear();
        m_TickDirtySources.Clear();
        linkRadiusSq = LFPG_SORTER_LINK_RADIUS * LFPG_SORTER_LINK_RADIUS;
        ruleChecksTick = 0;
        configMissesTick = 0;
        deferralsTick = 0;
        budgetExhausted = false;
        for (sorterIndex = m_SorterCursor; sorterIndex < batchEnd; sorterIndex = sorterIndex + 1)
        {
            if (sorterIndex >= m_RegisteredSorters.Count())
                break;
            sorter = LFPG_Sorter.Cast(m_RegisteredSorters[sorterIndex]);
            if (!sorter)
            {
                LFPG_ClearSorterResume(sorterIndex);
                continue;
            }
            if (!sorter.LFPG_IsPowered())
            {
                LFPG_ClearSorterResume(sorterIndex);
                continue;
            }
            if (sorter.IsRuined())
            {
                LFPG_ClearSorterResume(sorterIndex);
                continue;
            }
            filterConfig = sorter.LFPG_GetFilterConfig();
            if (!filterConfig)
            {
                LFPG_ClearSorterResume(sorterIndex);
                continue;
            }
            inputContainer = sorter.LFPG_GetLinkedContainer();
            if (inputContainer)
            {
                linkDistanceSq = LFPG_WorldUtil.DistSq(sorter.GetPosition(), inputContainer.GetPosition());
                if (linkDistanceSq > linkRadiusSq)
                {
                    sorter.LFPG_UnlinkContainer();
                    LFPG_ClearSorterResume(sorterIndex);
                    if (LFPG_LOG_LEVEL >= 1)
                    {
                        string unlinkMsg = "[Sorter] Auto-unlink: container beyond ";
                        unlinkMsg = unlinkMsg + LFPG_SORTER_LINK_RADIUS.ToString();
                        unlinkMsg = unlinkMsg + "m (was ";
                        unlinkMsg = unlinkMsg + Math.Sqrt(linkDistanceSq).ToString();
                        unlinkMsg = unlinkMsg + "m)";
                        LFPG_Util.Info(unlinkMsg);
                    }
                    continue;
                }
            }
            if (!inputContainer)
            {
                LFPG_ClearSorterResume(sorterIndex);
                continue;
            }
            if (!LFPG_SorterLogic.CanTakeFromContainer(inputContainer, null))
            {
                LFPG_ClearSorterResume(sorterIndex);
                continue;
            }
            if (!inputContainer.GetInventory())
            {
                LFPG_ClearSorterResume(sorterIndex);
                continue;
            }
            inputCargo = inputContainer.GetInventory().GetCargo();
            if (!inputCargo)
            {
                LFPG_ClearSorterResume(sorterIndex);
                continue;
            }
            itemCount = inputCargo.GetItemCount();
            if (itemCount <= 0)
            {
                LFPG_ClearSorterResume(sorterIndex);
                continue;
            }
			hasWireMask = CollectSorterOutputs(sorter, m_SorterOutputContainers);
            if (hasWireMask == 0)
            {
                LFPG_ClearSorterResume(sorterIndex);
                continue;
            }
			resumeState = m_SorterResumes[sorterIndex];
			resumeItem = resumeState.m_Item;
			resumeItemIndex = resumeState.m_ItemIndex;
			resumeOutput = resumeState.m_Output;
			resumeRule = resumeState.m_Rule;
			resumeConfig = resumeState.m_Config;
			wireGeneration = sorter.LFPG_GetWireGeneration();
			storedResumeValid = false;
			if (resumeItem && resumeConfig == filterConfig && resumeState.m_InputContainer == inputContainer)
			{
				if (resumeItemIndex >= 0 && resumeItemIndex < itemCount)
					storedResumeValid = inputCargo.GetItem(resumeItemIndex) == resumeItem;
				if (!storedResumeValid)
				{
					resumeItemIndex = inputCargo.FindEntityInCargo(resumeItem);
					storedResumeValid = resumeItemIndex >= 0;
				}
			}
			if (!storedResumeValid)
			{
				LFPG_ClearSorterResume(sorterIndex);
				resumeItem = null;
				resumeItemIndex = 0;
				resumeOutput = 0;
				resumeRule = 0;
			}
			else if (!resumeState.MatchesRoutes(hasWireMask, wireGeneration, m_SorterOutputContainers))
			{
				resumeOutput = 0;
				resumeRule = 0;
			}
			m_SorterItemCache.Clear();
			cacheEnd = resumeItemIndex + LFPG_SORTER_MAX_EVAL + 1;
			if (cacheEnd > itemCount)
				cacheEnd = itemCount;
			for (itemIndex = resumeItemIndex; itemIndex < cacheEnd; itemIndex = itemIndex + 1)
				m_SorterItemCache.Insert(inputCargo.GetItem(itemIndex));
            moved = 0;
            evaluated = 0;
			for (itemIndex = 0; itemIndex < m_SorterItemCache.Count(); itemIndex = itemIndex + 1)
            {
                if (moved >= LFPG_SORTER_ITEMS_PER_TICK || evaluated >= LFPG_SORTER_MAX_EVAL)
                {
                    sortItem = m_SorterItemCache[itemIndex];
					resumeState.Store(sortItem, resumeItemIndex + itemIndex, 0, 0, filterConfig, hasWireMask, wireGeneration, inputContainer, m_SorterOutputContainers);
                    break;
                }
                sortItem = m_SorterItemCache[itemIndex];
				evaluated = evaluated + 1;
                if (!sortItem)
                    continue;
                if (!inputContainer.CanReleaseCargo(sortItem))
                {
                    if (sortItem == resumeItem)
                        LFPG_ClearSorterResume(sorterIndex);
                    continue;
                }
                nextOutput = 0;
                nextRule = 0;
                itemRuleChecks = 0;
                itemConfigMisses = 0;
                itemDeferred = false;
                if (sortItem != resumeItem)
                {
                    resumeOutput = 0;
                    resumeRule = 0;
                }
                budgetRemaining = LFPG_SORTER_RULECHECK_BUDGET - ruleChecksTick - configMissesTick;
                outputIndex = LFPG_SorterLogic.EvaluateItemBudgeted(sortItem, filterConfig, hasWireMask, resumeOutput, resumeRule, budgetRemaining, nextOutput, nextRule, itemRuleChecks, itemConfigMisses, itemDeferred);
                ruleChecksTick = ruleChecksTick + itemRuleChecks;
                configMissesTick = configMissesTick + itemConfigMisses;
                if (itemDeferred)
                {
					resumeState.Store(sortItem, resumeItemIndex + itemIndex, nextOutput, nextRule, filterConfig, hasWireMask, wireGeneration, inputContainer, m_SorterOutputContainers);
                    deferralsTick = deferralsTick + 1;
                    budgetExhausted = true;
                    break;
                }
                LFPG_ClearSorterResume(sorterIndex);
                resumeItem = null;
                resumeOutput = 0;
                resumeRule = 0;
                if (outputIndex < 0)
                    continue;
				destinationContainer = m_SorterOutputContainers[outputIndex];
                if (!destinationContainer)
                    continue;
                if (destinationContainer == inputContainer)
                    continue;
                moveResult = LFPG_SorterLogic.MoveItemToContainerReusable(sortItem, destinationContainer, m_SorterMoveSourceLocation, m_SorterMoveDestinationLocation);
                if (moveResult)
                {
                    moved = moved + 1;
                    if (m_TickDirtyDestinations.Find(destinationContainer) < 0)
                        m_TickDirtyDestinations.Insert(destinationContainer);
                    if (m_TickAffectedContainers.Find(destinationContainer) < 0)
                        m_TickAffectedContainers.Insert(destinationContainer);
                }
            }
            if (itemIndex >= m_SorterItemCache.Count() && !budgetExhausted)
                LFPG_ClearSorterResume(sorterIndex);
            if (moved > 0)
            {
                if (m_TickDirtySources.Find(inputContainer) < 0)
                    m_TickDirtySources.Insert(inputContainer);
                if (m_TickAffectedContainers.Find(inputContainer) < 0)
                    m_TickAffectedContainers.Insert(inputContainer);
                if (LFPG_LOG_LEVEL >= 2)
                {
                    string tickDiag = "[TickSorters] moved=";
                    tickDiag = tickDiag + moved.ToString();
                    tickDiag = tickDiag + " evaluated=";
                    tickDiag = tickDiag + evaluated.ToString();
                    tickDiag = tickDiag + " src=";
                    tickDiag = tickDiag + inputContainer.GetType();
                    LFPG_Util.Debug(tickDiag);
                }
            }
			if (budgetExhausted)
			{
				m_SorterCursor = sorterIndex + 1;
				if (m_SorterCursor >= total)
					m_SorterCursor = 0;
				break;
			}
        }
        dirtyDestCommitCount = 0;
        dirtySourceCommitCount = 0;
        for (dirtyDestIndex = 0; dirtyDestIndex < m_TickDirtyDestinations.Count(); dirtyDestIndex = dirtyDestIndex + 1)
        {
            dirtyDestination = m_TickDirtyDestinations[dirtyDestIndex];
            if (dirtyDestination)
            {
                dirtyDestination.SetSynchDirty();
                dirtyDestCommitCount = dirtyDestCommitCount + 1;
            }
        }
        for (dirtySourceIndex = 0; dirtySourceIndex < m_TickDirtySources.Count(); dirtySourceIndex = dirtySourceIndex + 1)
        {
            dirtySource = m_TickDirtySources[dirtySourceIndex];
            if (!dirtySource)
                continue;
            if (m_TickDirtyDestinations.Find(dirtySource) >= 0)
                continue;
            dirtySource.SetSynchDirty();
            dirtySourceCommitCount = dirtySourceCommitCount + 1;
        }
        #ifndef SERVER
        if (LFPG_PERFDIAG_ENABLED)
        {
            m_PerfDiagSorterDestDirtyCount = m_PerfDiagSorterDestDirtyCount + dirtyDestCommitCount;
            m_PerfDiagSorterSourceDirtyCount = m_PerfDiagSorterSourceDirtyCount + dirtySourceCommitCount;
            string perfSorter = "LFPG_PERFDIAG sorter_dirty dest_tick=";
            perfSorter = perfSorter + dirtyDestCommitCount.ToString();
            perfSorter = perfSorter + " source_tick=";
            perfSorter = perfSorter + dirtySourceCommitCount.ToString();
            perfSorter = perfSorter + " dest_total=";
            perfSorter = perfSorter + m_PerfDiagSorterDestDirtyCount.ToString();
            perfSorter = perfSorter + " source_total=";
            perfSorter = perfSorter + m_PerfDiagSorterSourceDirtyCount.ToString();
            Print(perfSorter);
        }
        #endif
        #ifndef SERVER
        if (LFPG_PERFDIAG_ENABLED)
        {
            m_PerfDiagSorterRuleChecks = m_PerfDiagSorterRuleChecks + ruleChecksTick;
            m_PerfDiagSorterConfigMisses = m_PerfDiagSorterConfigMisses + configMissesTick;
            m_PerfDiagSorterDeferrals = m_PerfDiagSorterDeferrals + deferralsTick;
            int budgetUsed = ruleChecksTick + configMissesTick;
            string perfBudget = "LFPG_PERFDIAG sorter_budget rule_checks=";
            perfBudget = perfBudget + ruleChecksTick.ToString();
            perfBudget = perfBudget + " config_misses=";
            perfBudget = perfBudget + configMissesTick.ToString();
            perfBudget = perfBudget + " used=";
            perfBudget = perfBudget + budgetUsed.ToString();
            perfBudget = perfBudget + " budget=";
            perfBudget = perfBudget + LFPG_SORTER_RULECHECK_BUDGET.ToString();
            perfBudget = perfBudget + " deferrals=";
            perfBudget = perfBudget + deferralsTick.ToString();
            perfBudget = perfBudget + " checks_total=";
            perfBudget = perfBudget + m_PerfDiagSorterRuleChecks.ToString();
            perfBudget = perfBudget + " misses_total=";
            perfBudget = perfBudget + m_PerfDiagSorterConfigMisses.ToString();
            perfBudget = perfBudget + " deferrals_total=";
            perfBudget = perfBudget + m_PerfDiagSorterDeferrals.ToString();
            Print(perfBudget);
        }
        #endif
        if (m_TickAffectedContainers.Count() > 0)
        {
            string emptyExclude = "";
            BroadcastCargoRefreshToNearby(m_TickAffectedContainers, emptyExclude);
        }
        if (!budgetExhausted)
        {
            m_SorterCursor = batchEnd;
            if (m_SorterCursor >= total)
                m_SorterCursor = 0;
        }
        #endif
    }
    override void BroadcastCargoRefreshToNearby(array<EntityAI> containers, string excludePlayerId)
    {
        #ifdef SERVER
        if (!containers)
            return;
        int containerCount = containers.Count();
        if (containerCount <= 0)
            return;
        m_ReusablePlayers.Clear();
        g_Game.GetPlayers(m_ReusablePlayers);
        int playerCount = m_ReusablePlayers.Count();
        if (playerCount <= 0)
            return;
        float maxDistSq = 225.0;
        int pi = 0;
        int ci = 0;
        Man man = null;
        PlayerBase pb = null;
        PlayerIdentity pid = null;
        string pidStr = "";
        float distSq = 0.0;
        vector playerPos = vector.Zero;
        vector containerPos = vector.Zero;
        bool isNear = false;
        int notified = 0;
        int refreshSubId = LFPG_RPC_SubId.SORTER_CARGO_REFRESH;
        for (pi = 0; pi < playerCount; pi = pi + 1)
        {
            man = m_ReusablePlayers[pi];
            if (!man)
                continue;
            pb = PlayerBase.Cast(man);
            if (!pb)
                continue;
            pid = pb.GetIdentity();
            if (!pid)
                continue;
            pidStr = pid.GetId();
            if (excludePlayerId != "" && pidStr == excludePlayerId)
                continue;
            playerPos = pb.GetPosition();
            isNear = false;
            for (ci = 0; ci < containerCount; ci = ci + 1)
            {
                if (!containers[ci])
                    continue;
                containerPos = containers[ci].GetPosition();
                distSq = LFPG_WorldUtil.DistSq(playerPos, containerPos);
                if (distSq <= maxDistSq)
                {
                    isNear = true;
                    break;
                }
            }
            if (!isNear)
                continue;
            ScriptRPC refreshRpc = new ScriptRPC();
            refreshRpc.Write(refreshSubId);
            refreshRpc.Send(pb, LFPG_RPC_CHANNEL, true, pid);
            notified = notified + 1;
        }
        if (notified > 0)
        {
            if (LFPG_LOG_LEVEL >= 2)
            {
                string bcastLog = "[Sorter] CargoRefresh broadcast: notified=";
                bcastLog = bcastLog + notified.ToString();
                bcastLog = bcastLog + " containers=";
                bcastLog = bcastLog + containerCount.ToString();
                LFPG_Util.Debug(bcastLog);
            }
        }
        #endif
    }
    override int HandleSorterRequestSort(LFPG_Sorter sorter, string excludePlayerId)
    {
        #ifdef SERVER
        if (!sorter)
        {
            string w0 = "[Sorter] REQUEST_SORT: null sorter";
            LFPG_Util.Warn(w0);
            return -1;
        }
        if (!sorter.LFPG_IsPowered())
        {
            string d0 = "[Sorter] REQUEST_SORT: sorter not powered";
            LFPG_Util.Debug(d0);
            return -1;
        }
        EntityAI container = sorter.LFPG_GetLinkedContainer();
        if (!container)
        {
            string w1 = "[Sorter] REQUEST_SORT: no linked container";
            LFPG_Util.Warn(w1);
            return -1;
        }
        if (!LFPG_SorterLogic.CanTakeFromContainer(container, null))
        {
            string w2 = "[Sorter] REQUEST_SORT: container not accessible";
            LFPG_Util.Warn(w2);
            return -1;
        }
        LFPG_SortConfig filterConfig = sorter.LFPG_GetFilterConfig();
        if (!filterConfig)
        {
            string w3 = "[Sorter] REQUEST_SORT: no filter config";
            LFPG_Util.Warn(w3);
            return -1;
        }
        if (!container.GetInventory())
        {
            string w4 = "[Sorter] REQUEST_SORT: no inventory";
            LFPG_Util.Warn(w4);
            return -1;
        }
        CargoBase srcCargo = container.GetInventory().GetCargo();
        if (!srcCargo)
        {
            string w5 = "[Sorter] REQUEST_SORT: no cargo";
            LFPG_Util.Warn(w5);
            return -1;
        }
		int maxEval = 200;
        int itemCount = srcCargo.GetItemCount();
        if (itemCount <= 0)
        {
            string d1 = "[Sorter] REQUEST_SORT: cargo empty";
            LFPG_Util.Debug(d1);
            return 0;
        }
		array<EntityAI> outputContainers = new array<EntityAI>;
		int hasWireMask = CollectSorterOutputs(sorter, outputContainers);
        if (hasWireMask == 0)
        {
            string d2 = "[Sorter] REQUEST_SORT: no wired outputs, bin-pack only";
            LFPG_Util.Debug(d2);
            LFPG_SorterLogic.RepackCargoInPlace(container);
            array<EntityAI> repackOnly = new array<EntityAI>;
            repackOnly.Insert(container);
            BroadcastCargoRefreshToNearby(repackOnly, excludePlayerId);
            return 0;
        }
        array<EntityAI> sortCache = new array<EntityAI>;
        int ci = 0;
        EntityAI sortItem = null;
		for (ci = 0; ci < itemCount && ci < maxEval; ci = ci + 1)
        {
            sortItem = srcCargo.GetItem(ci);
            if (sortItem)
            {
                sortCache.Insert(sortItem);
            }
        }
        int moved = 0;
        int evaluated = 0;
        int outputIdx = 0;
        EntityAI destContainer = null;
        bool moveResult = false;
        array<EntityAI> dirtiedDests = new array<EntityAI>;
        for (ci = 0; ci < sortCache.Count(); ci = ci + 1)
        {
            if (evaluated >= maxEval)
                break;
            sortItem = sortCache[ci];
            if (!sortItem)
                continue;
            evaluated = evaluated + 1;
            if (!container.CanReleaseCargo(sortItem))
                continue;
            outputIdx = LFPG_SorterLogic.EvaluateItem(sortItem, filterConfig, hasWireMask);
            if (outputIdx < 0)
                continue;
			destContainer = outputContainers[outputIdx];
            if (!destContainer)
                continue;
            if (destContainer == container)
                continue;
            moveResult = LFPG_SorterLogic.MoveItemToContainer(sortItem, destContainer);
            if (moveResult)
            {
                moved = moved + 1;
                if (dirtiedDests.Find(destContainer) < 0)
                {
                    dirtiedDests.Insert(destContainer);
                }
            }
        }
        int di = 0;
        for (di = 0; di < dirtiedDests.Count(); di = di + 1)
        {
            dirtiedDests[di].SetSynchDirty();
        }
        if (moved > 0)
        {
            container.SetSynchDirty();
        }
        LFPG_SorterLogic.RepackCargoInPlace(container);
        array<EntityAI> affectedContainers = new array<EntityAI>;
        affectedContainers.Insert(container);
        for (di = 0; di < dirtiedDests.Count(); di = di + 1)
        {
            if (affectedContainers.Find(dirtiedDests[di]) < 0)
            {
                affectedContainers.Insert(dirtiedDests[di]);
            }
        }
        BroadcastCargoRefreshToNearby(affectedContainers, excludePlayerId);
        string sortLog = "[Sorter] REQUEST_SORT: evaluated=";
        sortLog = sortLog + evaluated.ToString();
        sortLog = sortLog + " moved=";
        sortLog = sortLog + moved.ToString();
        LFPG_Util.Info(sortLog);
        return moved;
        #endif
        return -1;
    }
    override void RegisterMotionSensor(LFPG_MotionSensor sensor)
    {
        if (!sensor)
            return;
        if (m_RegisteredSensors.Find(sensor) < 0)
        {
            m_RegisteredSensors.Insert(sensor);
        }
    }
    override void UnregisterMotionSensor(LFPG_MotionSensor sensor)
    {
        if (!sensor)
            return;
        int idx = m_RegisteredSensors.Find(sensor);
        if (idx >= 0)
        {
            m_RegisteredSensors.Remove(idx);
        }
    }
    override void RegisterPressurePad(LFPG_PressurePad pad)
    {
        if (!pad)
            return;
        if (m_RegisteredPads.Find(pad) < 0)
        {
            m_RegisteredPads.Insert(pad);
        }
    }
    override void UnregisterPressurePad(LFPG_PressurePad pad)
    {
        if (!pad)
            return;
        int idx = m_RegisteredPads.Find(pad);
        if (idx >= 0)
        {
            m_RegisteredPads.Remove(idx);
        }
    }
    override void RegisterLaserDetector(LFPG_LaserDetector laser)
    {
        if (!laser)
            return;
        if (m_RegisteredLasers.Find(laser) < 0)
        {
            m_RegisteredLasers.Insert(laser);
            laser.LFPG_UpdateBeamRaycast();
        }
    }
    override void UnregisterLaserDetector(LFPG_LaserDetector laser)
    {
        if (!laser)
            return;
        int idx = m_RegisteredLasers.Find(laser);
        if (idx >= 0)
        {
            m_RegisteredLasers.Remove(idx);
        }
    }
    override void RegisterIntercom(LFPG_Intercom ic)
    {
        if (!ic)
            return;
        if (m_RegisteredIntercoms.Find(ic) < 0)
        {
            m_RegisteredIntercoms.Insert(ic);
        }
    }
    override void UnregisterIntercom(LFPG_Intercom ic)
    {
        if (!ic)
            return;
        int idx = m_RegisteredIntercoms.Find(ic);
        if (idx >= 0)
        {
            m_RegisteredIntercoms.Remove(idx);
        }
    }
    override void RegisterFurnace(LFPG_Furnace furnace)
    {
        if (!furnace)
            return;
        if (m_RegisteredFurnaces.Find(furnace) < 0)
        {
            m_RegisteredFurnaces.Insert(furnace);
        }
    }
    override void UnregisterFurnace(LFPG_Furnace furnace)
    {
        if (!furnace)
            return;
        int idx = m_RegisteredFurnaces.Find(furnace);
        if (idx >= 0)
        {
            m_RegisteredFurnaces.Remove(idx);
        }
    }
    override void RegisterFridge(LFPG_Fridge fridge)
    {
        if (!fridge)
            return;
        if (m_RegisteredFridges.Find(fridge) < 0)
        {
            m_RegisteredFridges.Insert(fridge);
            m_RegisteredFridgePhases.Insert(m_NextFridgePhase);
            m_NextFridgePhase = m_NextFridgePhase + 1;
            if (m_NextFridgePhase >= 10)
                m_NextFridgePhase = 0;
        }
    }
    override void UnregisterFridge(LFPG_Fridge fridge)
    {
        if (!fridge)
            return;
        int idx = m_RegisteredFridges.Find(fridge);
        if (idx >= 0)
        {
            m_RegisteredFridges.Remove(idx);
            m_RegisteredFridgePhases.Remove(idx);
        }
    }
    override void RegisterStove(LFPG_ElectricStove stove)
    {
        if (!stove)
            return;
        if (m_RegisteredStoves.Find(stove) < 0)
        {
            m_RegisteredStoves.Insert(stove);
            m_RegisteredStovePhases.Insert(m_NextStovePhase);
            m_NextStovePhase = m_NextStovePhase + 1;
            if (m_NextStovePhase >= 3)
                m_NextStovePhase = 0;
        }
    }
    override void UnregisterStove(LFPG_ElectricStove stove)
    {
        if (!stove)
            return;
        int idx = m_RegisteredStoves.Find(stove);
        if (idx >= 0)
        {
            m_RegisteredStoves.Remove(idx);
            m_RegisteredStovePhases.Remove(idx);
        }
    }
    override void RegisterDoorController(LFPG_DoorController dc)
    {
        if (!dc)
            return;
        if (m_RegisteredDoorControllers.Find(dc) < 0)
        {
            m_RegisteredDoorControllers.Insert(dc);
        }
    }
    override void UnregisterDoorController(LFPG_DoorController dc)
    {
        if (!dc)
            return;
        int idx = m_RegisteredDoorControllers.Find(dc);
        if (idx >= 0)
        {
            m_RegisteredDoorControllers.Remove(idx);
        }
    }
    protected void LFPG_TickSimpleDevices()
    {
        #ifdef SERVER
        int totalIc;
        int totalDc;
        int totalFur;
        int totalBat;
        int totalFri;
        int totalStv;
        int totalSpr;
        int totalSimple;
        int intercomIndex;
        int doorIndex;
        int furnaceIndex;
        int fridgeIndex;
        int stoveIndex;
        int sprinklerIndex;
        int doorMod;
        int furnaceMod;
        int batteryMod;
        int wetAppliedTick;
        int wetCoalescedTick;
        int wetPreGateTick;
        int wetAppliedOne;
        int wetCoalescedOne;
        float stoveDelta;
        bool hasPlayerCell;
        bool sprinklerPhaseActive;
        LFPG_Intercom intercomDevice;
        LFPG_DoorController doorController;
        LFPG_Furnace furnaceDevice;
        LFPG_Fridge fridgeDevice;
        LFPG_ElectricStove stoveDevice;
        LFPG_Sprinkler sprinklerDevice;
        totalIc = m_RegisteredIntercoms.Count();
        totalDc = m_RegisteredDoorControllers.Count();
        totalFur = m_RegisteredFurnaces.Count();
        totalBat = m_RegisteredBatteries.Count();
        totalFri = m_RegisteredFridges.Count();
        totalStv = m_RegisteredStoves.Count();
        totalSpr = m_RegisteredSprinklers.Count();
        totalSimple = totalIc + totalDc + totalFur + totalBat + totalFri + totalStv + totalSpr;
        if (totalSimple == 0)
            return;
        m_SimpleTickCounter = m_SimpleTickCounter + 1;
        if (m_SimpleTickCounter >= 10)
            m_SimpleTickCounter = 0;
        for (intercomIndex = 0; intercomIndex < totalIc; intercomIndex = intercomIndex + 1)
        {
            if (intercomIndex >= m_RegisteredIntercoms.Count())
                break;
            intercomDevice = LFPG_Intercom.Cast(m_RegisteredIntercoms[intercomIndex]);
            if (intercomDevice)
                intercomDevice.LFPG_EvaluateToggleInput();
        }
        doorMod = m_SimpleTickCounter % 2;
        if (doorMod == 1)
        {
            for (doorIndex = 0; doorIndex < totalDc; doorIndex = doorIndex + 1)
            {
                if (doorIndex >= m_RegisteredDoorControllers.Count())
                    break;
                doorController = LFPG_DoorController.Cast(m_RegisteredDoorControllers[doorIndex]);
                if (doorController)
                    doorController.LFPG_OnDoorPoll();
            }
        }
        furnaceMod = m_SimpleTickCounter % 5;
        if (furnaceMod == 2)
        {
            for (furnaceIndex = 0; furnaceIndex < totalFur; furnaceIndex = furnaceIndex + 1)
            {
                if (furnaceIndex >= m_RegisteredFurnaces.Count())
                    break;
                furnaceDevice = LFPG_Furnace.Cast(m_RegisteredFurnaces[furnaceIndex]);
                if (furnaceDevice)
                    furnaceDevice.LFPG_BurnTick();
            }
        }
        batteryMod = m_SimpleTickCounter % 5;
        if (batteryMod == 4 && totalBat > 0)
            LFPG_TickBatteriesInternal();
        for (fridgeIndex = 0; fridgeIndex < totalFri; fridgeIndex = fridgeIndex + 1)
        {
            if (fridgeIndex >= m_RegisteredFridges.Count())
                break;
            if (m_RegisteredFridgePhases[fridgeIndex] != m_FridgePhaseCursor)
                continue;
            fridgeDevice = LFPG_Fridge.Cast(m_RegisteredFridges[fridgeIndex]);
            if (fridgeDevice)
                fridgeDevice.LFPG_OnCoolTick();
        }
        m_FridgePhaseCursor = m_FridgePhaseCursor + 1;
        if (m_FridgePhaseCursor >= 10)
            m_FridgePhaseCursor = 0;
        stoveDelta = 3.0;
        for (stoveIndex = 0; stoveIndex < totalStv; stoveIndex = stoveIndex + 1)
        {
            if (stoveIndex >= m_RegisteredStoves.Count())
                break;
            if (m_RegisteredStovePhases[stoveIndex] != m_StovePhaseCursor)
                continue;
            stoveDevice = LFPG_ElectricStove.Cast(m_RegisteredStoves[stoveIndex]);
            if (stoveDevice)
                stoveDevice.LFPG_TickCooking(stoveDelta);
        }
        m_StovePhaseCursor = m_StovePhaseCursor + 1;
        if (m_StovePhaseCursor >= 3)
            m_StovePhaseCursor = 0;
        wetAppliedTick = 0;
        wetCoalescedTick = 0;
        wetPreGateTick = 0;
        if (m_SprinklerPhaseCursor == 0)
            m_SprinklerWetPlayers.Clear();
        sprinklerPhaseActive = false;
        for (sprinklerIndex = 0; sprinklerIndex < totalSpr; sprinklerIndex = sprinklerIndex + 1)
        {
            if (sprinklerIndex >= m_RegisteredSprinklers.Count())
                break;
            if (m_RegisteredSprinklerPhases[sprinklerIndex] != m_SprinklerPhaseCursor)
                continue;
            sprinklerDevice = LFPG_Sprinkler.Cast(m_RegisteredSprinklers[sprinklerIndex]);
            if (sprinklerDevice)
            {
                if (sprinklerDevice.LFPG_GetSprinklerActive())
                {
                    sprinklerPhaseActive = true;
                    break;
                }
            }
        }
        if (sprinklerPhaseActive)
            LFPG_RebuildPlayerCells();
        for (sprinklerIndex = 0; sprinklerIndex < totalSpr; sprinklerIndex = sprinklerIndex + 1)
        {
            if (sprinklerIndex >= m_RegisteredSprinklers.Count())
                break;
            if (m_RegisteredSprinklerPhases[sprinklerIndex] != m_SprinklerPhaseCursor)
                continue;
            sprinklerDevice = LFPG_Sprinkler.Cast(m_RegisteredSprinklers[sprinklerIndex]);
            if (!sprinklerDevice)
                continue;
            if (!sprinklerDevice.LFPG_GetSprinklerActive())
                continue;
            hasPlayerCell = LFPG_HasPlayerCellNear(sprinklerDevice.GetPosition(), LFPG_SPRINKLER_RADIUS);
            if (!hasPlayerCell)
                wetPreGateTick = wetPreGateTick + 1;
            wetAppliedOne = 0;
            wetCoalescedOne = 0;
            sprinklerDevice.LFPG_TickWatering(hasPlayerCell, m_SprinklerWetPlayers, wetAppliedOne, wetCoalescedOne);
            wetAppliedTick = wetAppliedTick + wetAppliedOne;
            wetCoalescedTick = wetCoalescedTick + wetCoalescedOne;
        }
        m_SprinklerPhaseCursor = m_SprinklerPhaseCursor + 1;
        if (m_SprinklerPhaseCursor >= 10)
            m_SprinklerPhaseCursor = 0;
        #ifndef SERVER
        if (LFPG_PERFDIAG_ENABLED)
        {
            m_PerfDiagWetApplied = m_PerfDiagWetApplied + wetAppliedTick;
            m_PerfDiagWetCoalesced = m_PerfDiagWetCoalesced + wetCoalescedTick;
            m_PerfDiagWetPreGateSkips = m_PerfDiagWetPreGateSkips + wetPreGateTick;
            string wetDiag = "LFPG_PERFDIAG sprinkler_wet applied=";
            wetDiag = wetDiag + wetAppliedTick.ToString();
            wetDiag = wetDiag + " coalesced=";
            wetDiag = wetDiag + wetCoalescedTick.ToString();
            wetDiag = wetDiag + " pregate_skips=";
            wetDiag = wetDiag + wetPreGateTick.ToString();
            wetDiag = wetDiag + " applied_total=";
            wetDiag = wetDiag + m_PerfDiagWetApplied.ToString();
            wetDiag = wetDiag + " coalesced_total=";
            wetDiag = wetDiag + m_PerfDiagWetCoalesced.ToString();
            wetDiag = wetDiag + " pregate_total=";
            wetDiag = wetDiag + m_PerfDiagWetPreGateSkips.ToString();
            Print(wetDiag);
        }
        #endif
        #endif
    }
    protected void LFPG_RebuildPlayerCells()
    {
		if (m_PlayerCellsBuiltThisTurn)
			return;
		m_PlayerCellsBuiltThisTurn = true;
		m_PlayerCellIndex.Clear();
        int playerIndex;
        int playerTotal;
        int cellIndex;
        int cellTotal;
        int memberIndex;
        int writeIndex;
        int prefix;
        Man playerMan;
        PlayerBase playerBase;
        vector playerPos;
        int cellX;
        int cellZ;
        m_ReusablePlayers.Clear();
        g_Game.GetPlayers(m_ReusablePlayers);
        m_PlayerCellPlayers.Clear();
        m_PlayerCellMembership.Clear();
        m_PlayerCellX.Clear();
        m_PlayerCellZ.Clear();
        m_PlayerCellStart.Clear();
        m_PlayerCellCount.Clear();
        m_PlayerCellWrite.Clear();
        m_PlayerCellOrdered.Clear();
        playerTotal = m_ReusablePlayers.Count();
        for (playerIndex = 0; playerIndex < playerTotal; playerIndex = playerIndex + 1)
        {
            playerMan = m_ReusablePlayers[playerIndex];
            if (!playerMan)
                continue;
            if (!playerMan.IsAlive())
                continue;
            playerBase = PlayerBase.Cast(playerMan);
            if (!playerBase)
                continue;
            playerPos = playerBase.GetPosition();
            cellX = Math.Floor(playerPos[0] / LFPG_PLAYER_CELL_SIZE_M);
            cellZ = Math.Floor(playerPos[2] / LFPG_PLAYER_CELL_SIZE_M);
            cellIndex = -1;
			string cellKey = cellX.ToString() + "|" + cellZ.ToString();
			if (!m_PlayerCellIndex.Find(cellKey, cellIndex))
			{
                cellIndex = m_PlayerCellX.Count();
				m_PlayerCellIndex.Set(cellKey, cellIndex);
                m_PlayerCellX.Insert(cellX);
                m_PlayerCellZ.Insert(cellZ);
                m_PlayerCellStart.Insert(0);
                m_PlayerCellCount.Insert(0);
                m_PlayerCellWrite.Insert(0);
            }
            m_PlayerCellPlayers.Insert(playerMan);
            m_PlayerCellMembership.Insert(cellIndex);
            m_PlayerCellCount[cellIndex] = m_PlayerCellCount[cellIndex] + 1;
        }
        prefix = 0;
        cellTotal = m_PlayerCellX.Count();
        for (cellIndex = 0; cellIndex < cellTotal; cellIndex = cellIndex + 1)
        {
            m_PlayerCellStart[cellIndex] = prefix;
            m_PlayerCellWrite[cellIndex] = prefix;
            prefix = prefix + m_PlayerCellCount[cellIndex];
        }
        for (memberIndex = 0; memberIndex < prefix; memberIndex = memberIndex + 1)
        {
            m_PlayerCellOrdered.Insert(null);
        }
        for (memberIndex = 0; memberIndex < m_PlayerCellPlayers.Count(); memberIndex = memberIndex + 1)
        {
            cellIndex = m_PlayerCellMembership[memberIndex];
            writeIndex = m_PlayerCellWrite[cellIndex];
            m_PlayerCellOrdered[writeIndex] = m_PlayerCellPlayers[memberIndex];
            m_PlayerCellWrite[cellIndex] = writeIndex + 1;
        }
    }
    protected int LFPG_CollectPlayerCandidates(vector center, float radius)
    {
        int minCellX;
        int maxCellX;
        int minCellZ;
        int maxCellZ;
        int cellIndex;
        int cellTotal;
        int memberIndex;
        int memberEnd;
        Man candidate;
        m_PlayerCandidates.Clear();
        minCellX = Math.Floor((center[0] - radius) / LFPG_PLAYER_CELL_SIZE_M);
        maxCellX = Math.Floor((center[0] + radius) / LFPG_PLAYER_CELL_SIZE_M);
        minCellZ = Math.Floor((center[2] - radius) / LFPG_PLAYER_CELL_SIZE_M);
        maxCellZ = Math.Floor((center[2] + radius) / LFPG_PLAYER_CELL_SIZE_M);
        cellTotal = m_PlayerCellX.Count();
        for (cellIndex = 0; cellIndex < cellTotal; cellIndex = cellIndex + 1)
        {
            if (m_PlayerCellX[cellIndex] < minCellX)
                continue;
            if (m_PlayerCellX[cellIndex] > maxCellX)
                continue;
            if (m_PlayerCellZ[cellIndex] < minCellZ)
                continue;
            if (m_PlayerCellZ[cellIndex] > maxCellZ)
                continue;
            memberIndex = m_PlayerCellStart[cellIndex];
            memberEnd = memberIndex + m_PlayerCellCount[cellIndex];
            while (memberIndex < memberEnd)
            {
                candidate = m_PlayerCellOrdered[memberIndex];
                if (candidate)
                    m_PlayerCandidates.Insert(candidate);
                memberIndex = memberIndex + 1;
            }
        }
        return m_PlayerCandidates.Count();
    }
    protected bool LFPG_HasPlayerCellNear(vector center, float radius)
    {
        if (LFPG_CollectPlayerCandidates(center, radius) > 0)
            return true;
        return false;
    }
    protected void LFPG_TickPlayerDetection()
    {
        #ifdef SERVER
        int totalLasers;
        int totalPads;
        int totalSensors;
        int totalDetect;
        int padMod;
        bool padDue;
        bool sensorDue;
        bool needPlayers;
        int nowMs;
        int scanIndex;
        int processCount;
        int processIndex;
        int registryIndex;
        int candidateCount;
        int laserEvaluated;
        int padEvaluated;
        int sensorEvaluated;
        int laserDormant;
        int padDormant;
        int sensorDormant;
        int laserChanged;
        int padChanged;
        int sensorChanged;
        int raycasts;
        int rayChecked;
        int maintenanceBudget;
        int maintenanceRaycasts;
        LFPG_LaserDetector laser;
        LFPG_PressurePad pad;
        LFPG_MotionSensor sensor;
        bool stateChanged;
        string deviceId;
        totalLasers = m_RegisteredLasers.Count();
        totalPads = m_RegisteredPads.Count();
        totalSensors = m_RegisteredSensors.Count();
        totalDetect = totalLasers + totalPads + totalSensors;
        if (totalDetect == 0)
            return;
        m_PlayerDetectCounter = m_PlayerDetectCounter + 1;
        padMod = m_PlayerDetectCounter % 2;
        padDue = false;
        sensorDue = false;
        if (padMod == 0 && totalPads > 0)
            padDue = true;
        if (m_PlayerDetectCounter >= 10)
        {
            m_PlayerDetectCounter = 0;
            if (totalSensors > 0)
                sensorDue = true;
        }
        needPlayers = false;
        if (totalLasers > 0)
            needPlayers = true;
        if (padDue)
            needPlayers = true;
        if (sensorDue)
            needPlayers = true;
        if (needPlayers)
            LFPG_RebuildPlayerCells();
        nowMs = g_Game.GetTime();
        raycasts = 0;
        for (scanIndex = 0; scanIndex < totalLasers; scanIndex = scanIndex + 1)
        {
            if (scanIndex >= m_RegisteredLasers.Count())
                break;
            laser = LFPG_LaserDetector.Cast(m_RegisteredLasers[scanIndex]);
            if (!laser)
                continue;
            if (laser.LFPG_HasBeamTransformChanged())
            {
                laser.LFPG_UpdateBeamRaycast();
                raycasts = raycasts + 1;
            }
        }
        if (m_LaserRaycastCursor >= totalLasers)
            m_LaserRaycastCursor = 0;
        maintenanceBudget = (totalLasers + 22) / 23;
        if (maintenanceBudget < 4)
            maintenanceBudget = 4;
        maintenanceRaycasts = 0;
        rayChecked = 0;
        while (rayChecked < totalLasers && maintenanceRaycasts < maintenanceBudget)
        {
            registryIndex = m_LaserRaycastCursor;
            m_LaserRaycastCursor = m_LaserRaycastCursor + 1;
            if (m_LaserRaycastCursor >= totalLasers)
                m_LaserRaycastCursor = 0;
            rayChecked = rayChecked + 1;
            if (registryIndex >= m_RegisteredLasers.Count())
                continue;
            laser = LFPG_LaserDetector.Cast(m_RegisteredLasers[registryIndex]);
            if (!laser)
                continue;
            if (laser.LFPG_IsBeamMaintenanceDue(nowMs))
            {
                laser.LFPG_UpdateBeamRaycast();
                raycasts = raycasts + 1;
                maintenanceRaycasts = maintenanceRaycasts + 1;
            }
        }
        laserEvaluated = 0;
        laserDormant = 0;
        laserChanged = 0;
        if (totalLasers > 0)
        {
            if (m_LaserDetectCursor >= totalLasers)
                m_LaserDetectCursor = 0;
            processCount = totalLasers;
            for (processIndex = 0; processIndex < processCount; processIndex = processIndex + 1)
            {
                registryIndex = m_LaserDetectCursor;
                m_LaserDetectCursor = m_LaserDetectCursor + 1;
                if (m_LaserDetectCursor >= totalLasers)
                    m_LaserDetectCursor = 0;
                if (registryIndex >= m_RegisteredLasers.Count())
                    continue;
                laser = LFPG_LaserDetector.Cast(m_RegisteredLasers[registryIndex]);
                if (!laser)
                    continue;
                candidateCount = LFPG_CollectPlayerCandidates(laser.GetPosition(), LFPG_LASER_BEAM_RANGE_M + 1.0);
                laserEvaluated = laserEvaluated + 1;
                if (candidateCount == 0)
                    laserDormant = laserDormant + 1;
                stateChanged = laser.LFPG_EvaluateCrossing(m_PlayerCandidates);
                if (stateChanged)
                {
                    deviceId = laser.LFPG_GetDeviceId();
                    if (deviceId != "")
                        RequestPropagate(deviceId);
                    laserChanged = laserChanged + 1;
                }
            }
        }
        if (laserChanged > 0)
        {
            if (LFPG_LOG_LEVEL >= 2)
            {
                string laserMsg = "[PlayerDetect] Lasers: ";
                laserMsg = laserMsg + laserChanged.ToString();
                laserMsg = laserMsg + " changed state";
                LFPG_Util.Debug(laserMsg);
            }
        }
        padEvaluated = 0;
        padDormant = 0;
        padChanged = 0;
        if (padDue)
        {
            if (m_PadDetectCursor >= totalPads)
                m_PadDetectCursor = 0;
            processCount = totalPads;
            for (processIndex = 0; processIndex < processCount; processIndex = processIndex + 1)
            {
                registryIndex = m_PadDetectCursor;
                m_PadDetectCursor = m_PadDetectCursor + 1;
                if (m_PadDetectCursor >= totalPads)
                    m_PadDetectCursor = 0;
                if (registryIndex >= m_RegisteredPads.Count())
                    continue;
                pad = LFPG_PressurePad.Cast(m_RegisteredPads[registryIndex]);
                if (!pad)
                    continue;
                candidateCount = LFPG_CollectPlayerCandidates(pad.GetPosition(), 1.0);
                padEvaluated = padEvaluated + 1;
                if (candidateCount == 0)
                    padDormant = padDormant + 1;
                stateChanged = pad.LFPG_EvaluatePresence(m_PlayerCandidates);
                if (stateChanged)
                {
                    deviceId = pad.LFPG_GetDeviceId();
                    if (deviceId != "")
                        RequestPropagate(deviceId);
                    padChanged = padChanged + 1;
                }
            }
        }
        if (padChanged > 0)
        {
            if (LFPG_LOG_LEVEL >= 2)
            {
                string padMsg = "[PlayerDetect] Pads: ";
                padMsg = padMsg + padChanged.ToString();
                padMsg = padMsg + " changed state";
                LFPG_Util.Debug(padMsg);
            }
        }
        sensorEvaluated = 0;
        sensorDormant = 0;
        sensorChanged = 0;
        if (sensorDue)
        {
            if (m_SensorDetectCursor >= totalSensors)
                m_SensorDetectCursor = 0;
            processCount = totalSensors;
            for (processIndex = 0; processIndex < processCount; processIndex = processIndex + 1)
            {
                registryIndex = m_SensorDetectCursor;
                m_SensorDetectCursor = m_SensorDetectCursor + 1;
                if (m_SensorDetectCursor >= totalSensors)
                    m_SensorDetectCursor = 0;
                if (registryIndex >= m_RegisteredSensors.Count())
                    continue;
                sensor = LFPG_MotionSensor.Cast(m_RegisteredSensors[registryIndex]);
                if (!sensor)
                    continue;
                candidateCount = LFPG_CollectPlayerCandidates(sensor.GetPosition(), LFPG_SENSOR_RANGE_M);
                sensorEvaluated = sensorEvaluated + 1;
                if (candidateCount == 0)
                    sensorDormant = sensorDormant + 1;
                stateChanged = sensor.LFPG_EvaluateDetection(m_PlayerCandidates);
                if (stateChanged)
                {
                    deviceId = sensor.LFPG_GetDeviceId();
                    if (deviceId != "")
                        RequestPropagate(deviceId);
                    sensorChanged = sensorChanged + 1;
                }
            }
        }
        if (sensorChanged > 0)
        {
            if (LFPG_LOG_LEVEL >= 2)
            {
                string sensorMsg = "[PlayerDetect] Sensors: ";
                sensorMsg = sensorMsg + sensorChanged.ToString();
                sensorMsg = sensorMsg + " changed state";
                LFPG_Util.Debug(sensorMsg);
            }
        }
        #ifndef SERVER
        if (LFPG_PERFDIAG_ENABLED)
        {
            m_PerfDiagLaserEvaluations = m_PerfDiagLaserEvaluations + laserEvaluated;
            m_PerfDiagPadEvaluations = m_PerfDiagPadEvaluations + padEvaluated;
            m_PerfDiagSensorEvaluations = m_PerfDiagSensorEvaluations + sensorEvaluated;
            m_PerfDiagLaserDormant = m_PerfDiagLaserDormant + laserDormant;
            m_PerfDiagPadDormant = m_PerfDiagPadDormant + padDormant;
            m_PerfDiagSensorDormant = m_PerfDiagSensorDormant + sensorDormant;
            m_PerfDiagLaserChanges = m_PerfDiagLaserChanges + laserChanged;
            m_PerfDiagPadChanges = m_PerfDiagPadChanges + padChanged;
            m_PerfDiagSensorChanges = m_PerfDiagSensorChanges + sensorChanged;
            string detectDiag = "LFPG_PERFDIAG detection laser_eval=";
            detectDiag = detectDiag + laserEvaluated.ToString();
            detectDiag = detectDiag + " pad_eval=";
            detectDiag = detectDiag + padEvaluated.ToString();
            detectDiag = detectDiag + " sensor_eval=";
            detectDiag = detectDiag + sensorEvaluated.ToString();
            detectDiag = detectDiag + " laser_dormant=";
            detectDiag = detectDiag + laserDormant.ToString();
            detectDiag = detectDiag + " pad_dormant=";
            detectDiag = detectDiag + padDormant.ToString();
            detectDiag = detectDiag + " sensor_dormant=";
            detectDiag = detectDiag + sensorDormant.ToString();
            detectDiag = detectDiag + " laser_changed=";
            detectDiag = detectDiag + laserChanged.ToString();
            detectDiag = detectDiag + " pad_changed=";
            detectDiag = detectDiag + padChanged.ToString();
            detectDiag = detectDiag + " sensor_changed=";
            detectDiag = detectDiag + sensorChanged.ToString();
            detectDiag = detectDiag + " raycasts=";
            detectDiag = detectDiag + raycasts.ToString();
            detectDiag = detectDiag + " laser_eval_total=";
            detectDiag = detectDiag + m_PerfDiagLaserEvaluations.ToString();
            detectDiag = detectDiag + " pad_eval_total=";
            detectDiag = detectDiag + m_PerfDiagPadEvaluations.ToString();
            detectDiag = detectDiag + " sensor_eval_total=";
            detectDiag = detectDiag + m_PerfDiagSensorEvaluations.ToString();
            Print(detectDiag);
        }
        #endif
        #endif
    }
    override void RegisterBattery(EntityAI battery)
    {
        if (!battery)
            return;
        if (m_RegisteredBatteries.Find(battery) < 0)
        {
            m_RegisteredBatteries.Insert(battery);
        }
    }
    override void UnregisterBattery(EntityAI battery)
    {
        if (!battery)
            return;
        int idx = m_RegisteredBatteries.Find(battery);
        if (idx >= 0)
        {
            m_RegisteredBatteries.Remove(idx);
        }
    }
    protected void LFPG_TickBatteriesInternal()
    {
        #ifdef SERVER
        int batCount = m_RegisteredBatteries.Count();
        if (batCount <= 0)
            return;
        if (!m_Graph)
            return;
        float nowMs = g_Game.GetTime();
        float deltaMs = nowMs - m_BatteryLastTickMs;
        m_BatteryLastTickMs = nowMs;
        if (deltaMs < 100.0)
            return;
        if (deltaMs > 30000.0)
        {
            deltaMs = 30000.0;
        }
        float deltaSec = deltaMs / 1000.0;
        string hpZone = "";
        string hpPart = "";
        int bi;
        int dirtyCount = 0;
        for (bi = 0; bi < batCount; bi = bi + 1)
        {
            EntityAI batEnt = m_RegisteredBatteries[bi];
            if (!batEnt)
                continue;
            string batId = LFPG_DeviceAPI.GetDeviceId(batEnt);
            if (batId == "")
                continue;
            ref LFPG_ElecNode node = m_Graph.GetNode(batId);
            if (!node)
                continue;
            float storedEnergy = 0.0;
            float maxStored = 0.0;
            float maxCharge = 0.0;
            float maxDischarge = 0.0;
            float efficiency = 1.0;
            float selfDischargeRate = 0.0;
            bool dischargeEnabled = true;
            bool outputEnabled = true;
            LFPG_BatteryBase batBase = LFPG_BatteryBase.Cast(batEnt);
            LFPG_BatteryAdapter batAdapt = null;
            if (batBase)
            {
                storedEnergy = batBase.LFPG_GetStoredEnergy();
                maxStored = batBase.LFPG_GetMaxStoredEnergy();
                maxCharge = batBase.LFPG_GetMaxChargeRate();
                maxDischarge = batBase.LFPG_GetMaxDischargeRate();
                efficiency = batBase.LFPG_GetEfficiency();
                selfDischargeRate = batBase.LFPG_GetSelfDischargeRate();
                dischargeEnabled = batBase.LFPG_IsDischargeEnabled();
                outputEnabled = batBase.LFPG_IsOutputEnabled();
            }
            else
            {
                batAdapt = LFPG_BatteryAdapter.Cast(batEnt);
                if (batAdapt)
                {
                    storedEnergy = batAdapt.LFPG_GetStoredEnergy();
                    maxStored = batAdapt.LFPG_GetMaxStoredEnergy();
                    maxCharge = batAdapt.LFPG_GetMaxChargeRate();
                    maxDischarge = batAdapt.LFPG_GetMaxDischargeRate();
                    efficiency = batAdapt.LFPG_GetEfficiency();
                    selfDischargeRate = batAdapt.LFPG_GetSelfDischargeRate();
                    dischargeEnabled = batAdapt.LFPG_IsDischargeEnabled();
                    outputEnabled = batAdapt.LFPG_IsOutputEnabled();
                }
                else
                {
                    continue;
                }
            }
            if (maxStored < LFPG_PROPAGATION_EPSILON)
                continue;
            float healthRatio = 1.0;
            float maxHP = batEnt.GetMaxHealth(hpZone, hpPart);
            if (maxHP > 0.1)
            {
                float curHP = batEnt.GetHealth(hpZone, hpPart);
                healthRatio = curHP / maxHP;
                if (healthRatio < 0.0)
                {
                    healthRatio = 0.0;
                }
                if (healthRatio > 1.0)
                {
                    healthRatio = 1.0;
                }
            }
            float effectiveMax = maxStored * healthRatio;
            float inputReceived = node.m_InputPower;
            float outputDelivered = m_Graph.SumOutgoingAllocations(batId);
            float selfCons = node.m_Consumption;
            float netFlow = inputReceived - outputDelivered - selfCons;
            if (netFlow > maxCharge)
            {
                netFlow = maxCharge;
            }
            float negMaxDischarge = -maxDischarge;
            if (netFlow < negMaxDischarge)
            {
                netFlow = negMaxDischarge;
            }
            float energyDelta = 0.0;
            if (netFlow > LFPG_PROPAGATION_EPSILON)
            {
                float chargeWatts = netFlow;
                if (chargeWatts > maxCharge)
                {
                    chargeWatts = maxCharge;
                }
                energyDelta = chargeWatts * efficiency * deltaSec;
            }
            else if (netFlow < -LFPG_PROPAGATION_EPSILON)
            {
                float dischargeWatts = -netFlow;
                if (dischargeWatts > maxDischarge)
                {
                    dischargeWatts = maxDischarge;
                }
                energyDelta = -dischargeWatts * deltaSec;
            }
            float selfDrain = storedEnergy * selfDischargeRate * deltaSec / 3600.0;
            energyDelta = energyDelta - selfDrain;
            float newStored = storedEnergy + energyDelta;
            if (newStored < 0.0)
            {
                newStored = 0.0;
            }
            if (newStored > effectiveMax)
            {
                newStored = effectiveMax;
            }
            float offThreshold = effectiveMax * LFPG_BATTERY_DISCHARGE_OFF_PCT;
            float onThreshold = effectiveMax * LFPG_BATTERY_DISCHARGE_ON_PCT;
            bool newDischargeEnabled = dischargeEnabled;
            if (dischargeEnabled && newStored < offThreshold)
            {
                newDischargeEnabled = false;
            }
            else if (!dischargeEnabled && newStored > onThreshold)
            {
                newDischargeEnabled = true;
            }
            float newVirtualGen = 0.0;
            if (outputEnabled && newDischargeEnabled && newStored > LFPG_PROPAGATION_EPSILON)
            {
                float energyBudgetW = newStored / deltaSec;
                newVirtualGen = maxDischarge;
                if (newVirtualGen > energyBudgetW)
                {
                    newVirtualGen = energyBudgetW;
                }
            }
            float newSoftDemand = 0.0;
            float freeSpace = effectiveMax - newStored;
            if (freeSpace > LFPG_PROPAGATION_EPSILON)
            {
                float spaceBudgetW = freeSpace / deltaSec;
                newSoftDemand = maxCharge;
                if (newSoftDemand > spaceBudgetW)
                {
                    newSoftDemand = spaceBudgetW;
                }
            }
            float chargeRateDisplay = (newStored - storedEnergy) / deltaSec;
            if (chargeRateDisplay > maxCharge)
            {
                chargeRateDisplay = maxCharge;
            }
            float negMaxDischDisplay = -maxDischarge;
            if (chargeRateDisplay < negMaxDischDisplay)
            {
                chargeRateDisplay = negMaxDischDisplay;
            }
            if (batBase)
            {
                batBase.LFPG_SetStoredEnergy(newStored);
                if (newDischargeEnabled != dischargeEnabled)
                {
                    batBase.LFPG_SetDischargeEnabled(newDischargeEnabled);
                }
                batBase.LFPG_SetChargeRateCurrent(chargeRateDisplay);
            }
            else if (batAdapt)
            {
                batAdapt.LFPG_SetStoredEnergy(newStored);
                if (newDischargeEnabled != dischargeEnabled)
                {
                    batAdapt.LFPG_SetDischargeEnabled(newDischargeEnabled);
                }
                batAdapt.LFPG_SetChargeRateCurrent(chargeRateDisplay);
            }
            float vgDelta = newVirtualGen - node.m_VirtualGeneration;
            if (vgDelta < 0.0)
            {
                vgDelta = -vgDelta;
            }
            float sdDelta = newSoftDemand - node.m_SoftDemand;
            if (sdDelta < 0.0)
            {
                sdDelta = -sdDelta;
            }
            bool needsDirty = false;
            if (vgDelta > LFPG_PROPAGATION_EPSILON)
            {
                needsDirty = true;
            }
            if (sdDelta > LFPG_PROPAGATION_EPSILON)
            {
                needsDirty = true;
            }
            node.m_VirtualGeneration = newVirtualGen;
            node.m_SoftDemand = newSoftDemand;
            if (needsDirty)
            {
                m_Graph.MarkNodeDirty(batId, LFPG_DIRTY_INPUT);
                dirtyCount = dirtyCount + 1;
            }
        }
        if (dirtyCount > 0)
        {
            if (LFPG_LOG_LEVEL >= 2)
            {
                string batMsg = "[SimpleDevices] Batteries: ";
                batMsg = batMsg + dirtyCount.ToString();
                batMsg = batMsg + "/";
                batMsg = batMsg + batCount.ToString();
                batMsg = batMsg + " triggered propagation";
                LFPG_Util.Debug(batMsg);
            }
        }
        #endif
    }
};
