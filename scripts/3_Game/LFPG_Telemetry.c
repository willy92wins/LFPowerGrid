// =========================================================
// LF_PowerGrid - Client Telemetry (v0.7.13 — Sprint 2.5)
//
// Lightweight frame-level counters for preview (G1) and
// committed cable rendering (G5). No allocations per-frame.
//
// Usage:
//   - Each subsystem increments counters during its frame work.
//   - LFPG_Telemetry.Tick() is called once per frame from
//     the render loop. It accumulates per-frame data into
//     the running window, and dumps a summary log on interval.
//   - Counters are reset each frame by the subsystem that owns them.
//
// Data flow:
//   Preview (WiringClient) → s_Preview  (G1)
//   Render  (CableRenderer) → s_Render  (G5)
//   Tick() reads both, logs, resets interval accumulators.
//
// Design: static class, no instance, no allocations.
// =========================================================

// G1: Preview metrics — filled by LFPG_WiringClient.DrawPreviewFrame
class LFPG_PreviewMetrics
{
    int m_Spans;           // raw spans (waypoint segments) drawn this frame
    int m_SubSegments;     // total sub-segments (after sag subdivision)
    int m_CulledBehind;    // sub-segments skipped (behind camera)
    int m_CulledOffScreen; // sub-segments skipped (off-screen)
    int m_Drawn;           // sub-segments actually drawn
    int m_Projections;     // GetScreenPos calls this frame

    void Reset()
    {
        m_Spans = 0;
        m_SubSegments = 0;
        m_CulledBehind = 0;
        m_CulledOffScreen = 0;
        m_Drawn = 0;
        m_Projections = 0;
    }
};

// G5: Committed cable render metrics — filled by LFPG_CableRenderer.DrawFrame
class LFPG_RenderMetrics
{
    int m_WiresTotal;       // total wires known to renderer
    int m_WiresDrawn;       // wires that produced visible geometry
    int m_WiresCulled;      // wires skipped by distance culling
    int m_WiresOccluded;    // wires hidden by occlusion
    int m_WiresBudget;      // wires skipped by segment budget
    int m_WiresResolving;   // wires waiting for target entity
    int m_SegmentsDrawn;    // total segments rendered this frame
    int m_SegmentBudgetMax; // LFPG_MAX_RENDERED_SEGS (for % calc)
    int m_OccRaycastsUsed;  // occlusion raycasts fired this frame

    void Reset()
    {
        m_WiresTotal = 0;
        m_WiresDrawn = 0;
        m_WiresCulled = 0;
        m_WiresOccluded = 0;
        m_WiresBudget = 0;
        m_WiresResolving = 0;
        m_SegmentsDrawn = 0;
        m_SegmentBudgetMax = 512;  // mirrors LFPG_MAX_RENDERED_SEGS
        m_OccRaycastsUsed = 0;
    }
};

// Central telemetry collector — static, no instance
class LFPG_Telemetry
{
    // ---- Current frame data (written by subsystems, read by Tick) ----
    protected static ref LFPG_PreviewMetrics s_Preview;
    protected static ref LFPG_RenderMetrics  s_Render;

    // ---- Interval accumulators ----
    protected static float s_LastDumpMs = -99999.0;
    protected static int   s_FrameCount = 0;

    // Preview accumulators (sum over interval)
    protected static int s_SumPrvSpans = 0;
    protected static int s_SumPrvSubSegs = 0;
    protected static int s_SumPrvCulledBehind = 0;
    protected static int s_SumPrvCulledOff = 0;
    protected static int s_SumPrvDrawn = 0;
    protected static int s_SumPrvProjections = 0;

    // Render accumulators (sum over interval)
    protected static int s_SumRndTotal = 0;
    protected static int s_SumRndDrawn = 0;
    protected static int s_SumRndCulled = 0;
    protected static int s_SumRndOccluded = 0;
    protected static int s_SumRndBudget = 0;
    protected static int s_SumRndSegs = 0;
    protected static int s_SumRndOccRays = 0;
    protected static int s_PeakRndSegs = 0;

    // ---- Ensure metrics objects exist ----
    static LFPG_PreviewMetrics GetPreview()
    {
        if (!s_Preview)
        {
            s_Preview = new LFPG_PreviewMetrics();
        }
        return s_Preview;
    }

    static LFPG_RenderMetrics GetRender()
    {
        if (!s_Render)
        {
            s_Render = new LFPG_RenderMetrics();
        }
        return s_Render;
    }

    // ---- Called once per frame from render loop ----
    // Accumulates current frame metrics, dumps on interval, resets.
    // NOTE: caller (MissionInit, 5_Mission) must NOT call this on dedicated server.
    // nowMs passed in to avoid g_Game dependency in 3_Game layer.
    static void Tick(float nowMs)
    {
        // Ensure metrics exist
        LFPG_PreviewMetrics prv = GetPreview();
        LFPG_RenderMetrics rnd = GetRender();

        // Accumulate this frame
        s_FrameCount = s_FrameCount + 1;

        s_SumPrvSpans = s_SumPrvSpans + prv.m_Spans;
        s_SumPrvSubSegs = s_SumPrvSubSegs + prv.m_SubSegments;
        s_SumPrvCulledBehind = s_SumPrvCulledBehind + prv.m_CulledBehind;
        s_SumPrvCulledOff = s_SumPrvCulledOff + prv.m_CulledOffScreen;
        s_SumPrvDrawn = s_SumPrvDrawn + prv.m_Drawn;
        s_SumPrvProjections = s_SumPrvProjections + prv.m_Projections;

        s_SumRndTotal = s_SumRndTotal + rnd.m_WiresTotal;
        s_SumRndDrawn = s_SumRndDrawn + rnd.m_WiresDrawn;
        s_SumRndCulled = s_SumRndCulled + rnd.m_WiresCulled;
        s_SumRndOccluded = s_SumRndOccluded + rnd.m_WiresOccluded;
        s_SumRndBudget = s_SumRndBudget + rnd.m_WiresBudget;
        s_SumRndSegs = s_SumRndSegs + rnd.m_SegmentsDrawn;
        s_SumRndOccRays = s_SumRndOccRays + rnd.m_OccRaycastsUsed;

        if (rnd.m_SegmentsDrawn > s_PeakRndSegs)
        {
            s_PeakRndSegs = rnd.m_SegmentsDrawn;
        }

        // Reset per-frame counters for next frame
        prv.Reset();
        rnd.Reset();

        // Check if dump interval elapsed
        float elapsed = nowMs - s_LastDumpMs;
        if (s_LastDumpMs < 0.0)
        {
            // First tick: just record time, no dump
            s_LastDumpMs = nowMs;
            return;
        }

        if (elapsed < 5000.0)  // LFPG_TELEM_INTERVAL_MS
            return;

        // ---- Dump summary ----
        if (s_FrameCount <= 0)
        {
            s_LastDumpMs = nowMs;
            return;
        }

        // Preview averages (only meaningful if wiring was active)
        if (s_SumPrvSpans > 0)
        {
            int avgSpans = s_SumPrvSpans / s_FrameCount;
            int avgSubSegs = s_SumPrvSubSegs / s_FrameCount;
            int avgDrawn = s_SumPrvDrawn / s_FrameCount;
            int totalCulled = s_SumPrvCulledBehind + s_SumPrvCulledOff;
            int avgProjections = s_SumPrvProjections / s_FrameCount;

            string pLog = "[Telemetry-Preview] frames=" + s_FrameCount.ToString();
            pLog = pLog + " avgSpans=" + avgSpans.ToString();
            pLog = pLog + " avgSubSegs=" + avgSubSegs.ToString();
            pLog = pLog + " avgDrawn=" + avgDrawn.ToString();
            pLog = pLog + " totalCulled=" + totalCulled.ToString();
            pLog = pLog + " (behind=" + s_SumPrvCulledBehind.ToString();
            pLog = pLog + " off=" + s_SumPrvCulledOff.ToString() + ")";
            pLog = pLog + " avgProjections=" + avgProjections.ToString();
            Print("[LF_PowerGrid] " + pLog);
        }

        // Render averages (only meaningful if CableRenderer populated data)
        if (s_SumRndTotal > 0)
        {
            int avgWires = s_SumRndTotal / s_FrameCount;
            int avgDrawnW = s_SumRndDrawn / s_FrameCount;
            int avgCulledW = s_SumRndCulled / s_FrameCount;
            int avgOccW = s_SumRndOccluded / s_FrameCount;
            int avgSegs = s_SumRndSegs / s_FrameCount;
            int budgetPct = 0;
            int budgetMax = 512;  // mirrors LFPG_MAX_RENDERED_SEGS
            if (budgetMax > 0)
            {
                budgetPct = (s_PeakRndSegs * 100) / budgetMax;
            }

            string rLog = "[Telemetry-Render] frames=" + s_FrameCount.ToString();
            rLog = rLog + " avgWires=" + avgWires.ToString();
            rLog = rLog + " avgDrawn=" + avgDrawnW.ToString();
            rLog = rLog + " avgCulled=" + avgCulledW.ToString();
            rLog = rLog + " avgOccluded=" + avgOccW.ToString();
            rLog = rLog + " avgSegs=" + avgSegs.ToString();
            rLog = rLog + " peakSegs=" + s_PeakRndSegs.ToString();
            rLog = rLog + " budgetPct=" + budgetPct.ToString() + "%";
            rLog = rLog + " occRays=" + s_SumRndOccRays.ToString();
            Print("[LF_PowerGrid] " + rLog);
        }

        // Reset accumulators
        s_FrameCount = 0;
        s_SumPrvSpans = 0;
        s_SumPrvSubSegs = 0;
        s_SumPrvCulledBehind = 0;
        s_SumPrvCulledOff = 0;
        s_SumPrvDrawn = 0;
        s_SumPrvProjections = 0;
        s_SumRndTotal = 0;
        s_SumRndDrawn = 0;
        s_SumRndCulled = 0;
        s_SumRndOccluded = 0;
        s_SumRndBudget = 0;
        s_SumRndSegs = 0;
        s_SumRndOccRays = 0;
        s_PeakRndSegs = 0;
        s_LastDumpMs = nowMs;
    }
};
