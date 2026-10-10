// =========================================================
// LF_PowerGrid - server perf probe for R-01/R-02/R-04/R-05
// Default OFF via LFPG_PERF_PROBE. No persistence, no RPC.
// =========================================================

#ifdef SERVER
class LFPG_PerfProbe
{
	protected static bool s_Active;
	protected static string s_Route;
	protected static int s_T0;
	protected static int s_PhaseT0;
	protected static int s_PhaseIndexMs;
	protected static int s_PhaseRescueMs;
	protected static int s_PhaseRebuildMs;
	protected static int s_PhaseWalkMs;
	protected static int s_Allocs;
	protected static int s_Nodes;
	protected static int s_Edges;
	protected static int s_Requeues;
	protected static int s_MapIndexCalls;
	protected static int s_MapSweepCount;
	protected static int s_LastMapSize;
	protected static int s_PdqTicks;
	protected static int s_SettleTicks;
	protected static bool s_WaitingSettle;
	protected static int s_SummaryT0;
	protected static int s_Events;
	protected static int s_SumMs;
	protected static int s_SumAllocs;
	protected static int s_SumMapIndex;

	static void Begin(string route)
	{
		if (!LFPG_PERF_PROBE)
			return;
		s_Route = route;
		s_T0 = g_Game.GetTime();
		s_PhaseT0 = s_T0;
		s_PhaseIndexMs = 0;
		s_PhaseRescueMs = 0;
		s_PhaseRebuildMs = 0;
		s_PhaseWalkMs = 0;
		s_Allocs = 0;
		s_Nodes = 0;
		s_Edges = 0;
		s_Requeues = 0;
		s_MapIndexCalls = 0;
		s_MapSweepCount = 0;
		s_LastMapSize = 0;
		s_PdqTicks = 0;
		s_SettleTicks = -1;
		s_WaitingSettle = false;
		s_Active = true;
	}

	static void Phase(string name)
	{
		if (!LFPG_PERF_PROBE)
			return;
		if (!s_Active)
			return;
		int now = g_Game.GetTime();
		int elapsed = now - s_PhaseT0;
		s_PhaseT0 = now;
		if (name == "index")
			s_PhaseIndexMs = elapsed;
		else if (name == "rescue")
			s_PhaseRescueMs = elapsed;
		else if (name == "rebuild")
			s_PhaseRebuildMs = elapsed;
		else if (name == "walk")
			s_PhaseWalkMs = elapsed;
	}

	static void AddAlloc(int n)
	{
		if (!LFPG_PERF_PROBE)
			return;
		if (!s_Active)
			return;
		s_Allocs = s_Allocs + n;
	}

	static void AddVisit(int nodes, int edges)
	{
		if (!LFPG_PERF_PROBE)
			return;
		if (!s_Active)
			return;
		s_Nodes = s_Nodes + nodes;
		s_Edges = s_Edges + edges;
	}

	static void AddRequeue(int n)
	{
		if (!LFPG_PERF_PROBE)
			return;
		if (!s_Active)
			return;
		s_Requeues = s_Requeues + n;
	}

	static void CountMapSweep(string mapName, int mapSize, int indexCalls)
	{
		if (!LFPG_PERF_PROBE)
			return;
		s_MapSweepCount = s_MapSweepCount + 1;
		s_LastMapSize = mapSize;
		s_MapIndexCalls = s_MapIndexCalls + indexCalls;
		LFPG_Util.Info("LFPG_PERF event=0 kind=sweep map=" + mapName + " map_size=" + mapSize.ToString() + " index_calls=" + indexCalls.ToString());
	}

	static void End()
	{
		if (!LFPG_PERF_PROBE)
			return;
		if (!s_Active)
			return;
		int totalMs = g_Game.GetTime() - s_T0;
		string line = "LFPG_PERF event=1 route=" + s_Route;
		line = line + " total_ms=" + totalMs.ToString();
		line = line + " phase_index_ms=" + s_PhaseIndexMs.ToString();
		line = line + " phase_rescue_ms=" + s_PhaseRescueMs.ToString();
		line = line + " phase_rebuild_ms=" + s_PhaseRebuildMs.ToString();
		line = line + " phase_walk_ms=" + s_PhaseWalkMs.ToString();
		line = line + " nodes=" + s_Nodes.ToString();
		line = line + " edges=" + s_Edges.ToString();
		line = line + " requeues=" + s_Requeues.ToString();
		line = line + " allocs=" + s_Allocs.ToString();
		line = line + " map_index=" + s_MapIndexCalls.ToString();
		line = line + " map_size=" + s_LastMapSize.ToString();
		line = line + " map_sweeps=" + s_MapSweepCount.ToString();
		LFPG_Util.Info(line);
		s_Events = s_Events + 1;
		s_SumMs = s_SumMs + totalMs;
		s_SumAllocs = s_SumAllocs + s_Allocs;
		s_SumMapIndex = s_SumMapIndex + s_MapIndexCalls;
		s_WaitingSettle = true;
		s_SettleTicks = 0;
		s_Active = false;
		MaybeSummary();
	}

	static void OnProcessDirtyQueue(int remaining, int processed, int edges, int ms)
	{
		if (!LFPG_PERF_PROBE)
			return;
		if (s_WaitingSettle)
		{
			s_PdqTicks = s_PdqTicks + 1;
			s_Edges = s_Edges + edges;
			s_Nodes = s_Nodes + processed;
			if (remaining <= 0)
			{
				s_SettleTicks = s_PdqTicks;
				s_WaitingSettle = false;
				string settle = "LFPG_PERF event=1 route=" + s_Route;
				settle = settle + " kind=settle settle_ticks=" + s_SettleTicks.ToString();
				settle = settle + " pdq_ticks=" + s_PdqTicks.ToString();
				settle = settle + " last_process_ms=" + ms.ToString();
				settle = settle + " nodes=" + s_Nodes.ToString();
				settle = settle + " edges=" + s_Edges.ToString();
				LFPG_Util.Info(settle);
			}
		}
		MaybeSummary();
	}

	protected static void MaybeSummary()
	{
		if (!LFPG_PERF_PROBE)
			return;
		int now = g_Game.GetTime();
		if (s_SummaryT0 == 0)
		{
			s_SummaryT0 = now;
			return;
		}
		if ((now - s_SummaryT0) < LFPG_PERF_PROBE_SUMMARY_MS)
			return;
		string sum = "LFPG_PERF event=0 kind=summary";
		sum = sum + " events=" + s_Events.ToString();
		sum = sum + " sum_ms=" + s_SumMs.ToString();
		sum = sum + " sum_allocs=" + s_SumAllocs.ToString();
		sum = sum + " sum_map_index=" + s_SumMapIndex.ToString();
		sum = sum + " interval_ms=" + LFPG_PERF_PROBE_SUMMARY_MS.ToString();
		LFPG_Util.Info(sum);
		s_Events = 0;
		s_SumMs = 0;
		s_SumAllocs = 0;
		s_SumMapIndex = 0;
		s_SummaryT0 = now;
	}
}
#endif
