// =========================================================
// LF_PowerGrid - client cable renderer (v0.7.38)
//
// v0.7.38 (Audit Phase 1) changes:
//   C1 — Painter's sort: swap-based selection sort replaces O(n³) InsertAt.
//   C2 — All LFPG_Diag.ServerEcho calls guarded with LFPG_DIAG_ENABLED
//        to prevent string concatenation when diagnostics disabled.
//   C3 — CullTick log block gated by LFPG_DIAG_ENABLED. Interval
//        increased from 10s/21% to 30s/~7%.
//   C4 — Bitmask guard changed from < 31 to <= 30 (explicit safe range).
//        Documented MAX_EDGES_PER_NODE = 12 makes overflow academic.
//   L9 — CullTick visibility: removed redundant endpoint distance checks
//        (bounding sphere already covers endpoints by definition).
//
// v0.7.38 (Audit Phase 2) changes:
//   H1 — Cohen-Sutherland + ComputeOutcode moved to LFPG_WorldUtil
//        (shared with WiringClient, ~120 lines removed per file).
//   H6 — Occlusion stagger uses stable occStaggerGroup from wireIndex
//        instead of map index which shifts on add/remove.
//   M1 — Magic numbers replaced with named constants from Defines
//        (LFPG_ALPHA_MIN_THRESHOLD, LFPG_OCC_ALPHA_MIN,
//         LFPG_SCREEN_MARGIN_*, LFPG_SURFACE_CLAMP_M).
//   M11 — BUDGET retry entries get createdMs + TTL expiry (60s).
//   M12 — (CableHUD) Canvas.Clear before early return in BeginFrame.
//
// v0.7.38 (Audit Phase 3) changes:
//   H3 — Ultra-LOD margin: proportional (shF * 0.15) replaces fixed 200px.
//   M3 — cachedMinDist: bounding sphere closest-point estimate
//        (distToCenter - radius) replaces avg(endpointA, endpointB).
//   M6 — (CableHUD) EndFrame diagnostic guarded with LFPG_DIAG_ENABLED.
//   L1 — (WiringClient) Preview line width uses LFPG_PREVIEW_LINE_WIDTH.
//   L2 — Sway hash primes use LFPG_SWAY_HASH_X/Z constants.
//   L4 — Ghost keys use pre-allocated m_GhostKeys instead of per-CullTick new.
//   L6 — Endcap size computed once per wire instead of twice (A + B).
//
// v0.7.38 (Audit Phase 4) changes:
//   M2 — LOD_MID_M alias removed; uses canonical LFPG_LOD_MEDIUM_M.
//   L8 — All z-depth behind-camera thresholds use LFPG_BEHIND_CAM_Z.
//   Remaining M1 — OCC_DIST_SCALE_MAX, OCC_SAMPLE_LIFT_M extracted.
//
// v0.7.38 Player occlusion:
//   Screen-space player model occlusion. Segments behind the player
//   character are alpha-faded so cables don't overdraw the model.
//   Cost: 2 GetScreenPos per frame + 1 line-vs-rect per segment.
//
// Event-driven cable rendering with frozen geometry.
//
// Architecture:
//   1. UpsertOwnerBlob (RPC event) -> stores segment data.
//      Geometry is computed once and never recomputed unless the
//      wire topology changes (new wire, cut, device destroyed).
//   2. CullTick (2s timer) -> distance-based visibility flag.
//   3. DrawFrame (per-frame from MissionGameplay) -> draws visible
//      segments via Canvas 2D with raycast occlusion.
//   4. RetryTick (5s timer) -> builds wires whose target entity
//      was not available at initial build time (network bubble edge).
//
// v0.7.35 (Fase 1) changes:
//   F1.1 — Fixed crash: m_AllWires → m_WireSegments (undefined member)
//   F1.2 — Cohen-Sutherland screen clipping replaces offA&&offB cull.
//          Fixes cables vanishing when segment spans viewport.
//   F1.3 — warningMask integration: per-wire WARNING_LOAD state
//          using LFPG_DeviceAPI.GetWarningMask() bitmask.
//   F1.4 — Sway-aware behind-camera clipping: ClipBehindCamera now
//          receives swayed world coords instead of frozen geometry.
//
// v0.7.35 (Fase 2) changes:
//   F2.1 — Behind-camera dot product early-out in DrawFrame.
//          Skips projection for wires whose bounding sphere is
//          entirely behind the camera.
//   F2.2 — Ultra-LOD for distant wires (lodTier 2, >40m).
//          Projects only endpoints, draws 1 straight line.
//          Skips catenary, sway, endcaps, joints.
//   F2.3 — Partial occlusion alpha: wires with some (not all)
//          occlusion samples blocked fade proportionally instead
//          of the all-or-nothing visibility switch.
//
// v0.7.7 improvements:
//   - Bounding sphere culling (fixes midpoint-near-player bug)
//   - Device proximity bubble (configurable tight cull radius)
//   - LOD visual: 3/2/1 passes by distance (shadow+base+highlight)
//   - Depth-based line width (fake perspective)
//   - Alpha fade at distance (smooth disappearance)
//   - Owner early-out in CullTick (skip all wires if owner far)
//
// Occlusion: raycast from camera to segment midpoint.
//   Budgeted (max N raycasts/frame), staggered by time,
//   with hysteresis to prevent flicker at geometry edges.
//
// Connection cache: rebuilt on UpsertOwnerBlob only.
//   Key: "deviceId|portName|dir"  Value: connected type name
//   GetConnectionType() is O(1) map lookup.
//
// v0.7.13 (Sprint 2.5, G5): Render telemetry counters in DrawFrame.
//   Populates LFPG_RenderMetrics per frame (drawn/culled/occluded/budget/segs).
//   LFPG_Telemetry.Tick() in MissionInit reads+accumulates+resets each frame.
//
// v0.7.14: Fixed behind-camera cable rendering (lines parallel to screen edges).
//   Replaced screen-space extension with 3D near-plane clipping via
//   LFPG_WorldUtil.ClipBehindCamera(). Removed unused extScale variable.
// =========================================================

class LFPG_OwnerWireState
{
    string ownerDeviceId;
    int ownerLow;
    int ownerHigh;

    ref array<ref LFPG_WireData> wires;

    // Last JSON received (for change detection; avoids redundant decode)
    string lastJson;

    // Last known powered state (persists when entity is out of bubble)
    bool lastPowered;

    // v0.7.8: Load ratio from source (0.0-N), synced from server.
    float lastLoadRatio;

    // v0.7.8: Bitmask of overloaded output wires (bit N = wire N overloaded).
    int lastOverloadMask;

    // v0.7.35 (Fase1 F1.3): Bitmask of warning-level output wires.
    // Bit N = wire N is at WARNING_LOAD (80%+ capacity but not overloaded).
    int lastWarningMask;

    // v0.7.9: Pre-computed wire keys ("ownerId|0", "ownerId|1", etc.)
    // Populated in BuildOwnerWires. Eliminates string concat in CullTick.
    ref array<string> cachedWireKeys;

    // v0.7.9: Consecutive CullTick cycles where ownerObj was null.
    // After threshold, wires are destroyed (device likely deleted/despawned).
    int nullOwnerTicks;
};

// Per-wire rendering data: visual sub-segments + wire-level occlusion
class LFPG_WireSegmentInfo
{
    ref array<ref LFPG_CableParticle> segments;
    bool powered;
    bool visible;        // distance-based (CullTick)

    // Cached endpoint positions for distance-based culling.
    vector cachedPosA;
    vector cachedPosB;

    // v0.7.7: Bounding sphere for the ENTIRE wire (all sub-segments).
    // Fixes the known bug where cables with waypoints disappear when
    // the player is near the midpoint but far from both endpoints.
    vector cachedCenter;
    float  cachedRadius;

    // v0.7.7: Minimum distance from player to nearest point on wire.
    // Computed in CullTick, used in DrawFrame for LOD + alpha fade.
    // Avoids redundant distance calculations per frame.
    float  cachedMinDist;

    // v0.7.8: Cable visual state (determines color).
    // Set in CullTick based on powered state + load + overload mask.
    int cableState;

    // v0.7.8: Wire index within the owner (for overload mask bit check).
    int wireIndex;

    // v0.7.9: Pre-computed wire key ("ownerId|wireIdx") to avoid
    // string concatenation in CullTick's inner loop.
    string cachedWireKey;

    // v0.7.8: Waypoint world positions for joint rendering.
    // Stored at build time. Joints are drawn only at LOD close.
    ref array<vector> cachedJoints;

    // ---- Wire-level occlusion (raycast Z-buffer emulation) ----
    // Coarse sample points for raycast (1 or 3 depending on wire length).
    // Built once in BuildWire. Raycasts target these, not individual sub-segs.
    ref array<vector> occSamples;
    bool   occluded;          // current occlusion state (with hysteresis)
    float  occNextCheckMs;    // game time for next recheck
    int    occConsecCount;    // positive=consecutive visible, negative=consecutive occluded

    // v0.7.35 (F2.3): Ratio of occluded samples (0.0 = fully visible, 1.0 = fully blocked).
    // Updated by CheckWireOcclusion. Used in DrawFrame to fade partially
    // occluded wires instead of all-or-nothing. Only meaningful when occluded == false.
    float  occBlockedRatio;

    // v0.7.38 (H6): Stable stagger group for occlusion round-robin.
    // Computed once at BuildWire from wireIndex. Avoids flicker caused by
    // map-index-based stagger shifting when wires are added/removed.
    int    occStaggerGroup;

    void LFPG_WireSegmentInfo()
    {
        segments   = new array<ref LFPG_CableParticle>;
        occSamples = new array<vector>;
        cachedJoints = new array<vector>;
        visible    = true;
        occluded   = false;
        occNextCheckMs  = 0;
        occConsecCount  = 0;
        occBlockedRatio = 0.0;
        occStaggerGroup = 0;
        cachedCenter    = "0 0 0";
        cachedRadius    = 0.0;
        cachedMinDist   = 0.0;
        cableState      = LFPG_CableState.IDLE;
    }

    // Build occlusion sample points from ACTUAL cable geometry.
    // v0.7.9: Uses real sub-segment positions (after sag + waypoints)
    // instead of the straight line A→B. Fixes incorrect occlusion
    // for cables with waypoints or heavy sag.
    // Short wires: 1 sample (midpoint of chain).
    // Long wires:  3 samples (25%, 50%, 75% along chain).
    void BuildOccSamples()
    {
        occSamples.Clear();

        if (!segments || segments.Count() == 0)
        {
            // Fallback: midpoint of endpoints
            occSamples.Insert((cachedPosA + cachedPosB) * 0.5);
            return;
        }

        // Compute total chain length
        float totalLen = 0.0;
        int i;
        LFPG_CableParticle seg;
        for (i = 0; i < segments.Count(); i = i + 1)
        {
            seg = segments[i];
            if (!seg || !seg.IsValid())
                continue;
            totalLen = totalLen + vector.Distance(seg.m_From, seg.m_To);
        }

        if (totalLen < 0.01)
        {
            occSamples.Insert((cachedPosA + cachedPosB) * 0.5);
            return;
        }

        // Determine sample fractions
        // v0.7.9: 3-tier adaptive — short wires 1 sample, medium 3, long 5.
        // More samples for long wires improves detection of partial occlusion
        // behind irregular geometry (rocks, railings, vehicles).
        int sampleCount = 1;
        if (totalLen >= 20.0)
        {
            sampleCount = 5;
        }
        else if (totalLen >= LFPG_OCC_LONG_WIRE_M)
        {
            sampleCount = 3;
        }

        // Walk the chain to find points at desired fractions
        int si;
        LFPG_CableParticle s;
        for (si = 0; si < sampleCount; si = si + 1)
        {
            float frac;
            if (sampleCount == 1)
            {
                frac = 0.5;
            }
            else
            {
                // 0.25, 0.50, 0.75
                frac = (si + 1.0) / (sampleCount + 1.0);
            }

            float targetDist = totalLen * frac;
            float walked = 0.0;
            bool found = false;

            int j;
            for (j = 0; j < segments.Count(); j = j + 1)
            {
                s = segments[j];
                if (!s || !s.IsValid())
                    continue;

                float segLen = vector.Distance(s.m_From, s.m_To);
                if (walked + segLen >= targetDist)
                {
                    // Interpolate within this segment
                    float remain = targetDist - walked;
                    float t = 0.5;
                    if (segLen > 0.01)
                    {
                        t = remain / segLen;
                    }
                    vector pt = s.m_From + (s.m_To - s.m_From) * t;
                    occSamples.Insert(pt);
                    found = true;
                    break;
                }
                walked = walked + segLen;
            }

            if (!found)
            {
                // Edge case: use last segment endpoint
                occSamples.Insert(cachedPosB);
            }
        }
    }

    // v0.7.7: Build bounding sphere from all actual sub-segment points.
    // This properly encloses waypoints and sag, not just endpoints.
    void BuildBoundingSphere()
    {
        if (!segments || segments.Count() == 0)
        {
            cachedCenter = (cachedPosA + cachedPosB) * 0.5;
            cachedRadius = vector.Distance(cachedPosA, cachedPosB) * 0.5;
            return;
        }

        // Accumulate all unique points from sub-segments
        vector sumPos = "0 0 0";
        int pointCount = 0;
        int i;
        LFPG_CableParticle seg;

        // Add first point of first segment (with null guard)
        // v0.7.36 (L1): segments[0] can be null if first Create() failed.
        if (segments[0])
        {
            sumPos = sumPos + segments[0].m_From;
            pointCount = pointCount + 1;
        }

        for (i = 0; i < segments.Count(); i = i + 1)
        {
            seg = segments[i];
            if (!seg || !seg.IsValid())
                continue;

            sumPos = sumPos + seg.m_To;
            pointCount = pointCount + 1;
        }

        if (pointCount == 0)
        {
            cachedCenter = (cachedPosA + cachedPosB) * 0.5;
            cachedRadius = vector.Distance(cachedPosA, cachedPosB) * 0.5;
            return;
        }

        // Center = centroid of all points
        float invCount = 1.0 / pointCount;
        cachedCenter = sumPos * invCount;

        // Radius = max distance from center to any point
        float maxDist = 0.0;
        float d;

        // v0.7.36 (L1): null guard on segments[0] for radius calc
        if (segments[0])
        {
            d = vector.Distance(cachedCenter, segments[0].m_From);
            if (d > maxDist)
            {
                maxDist = d;
            }
        }

        for (i = 0; i < segments.Count(); i = i + 1)
        {
            seg = segments[i];
            if (!seg || !seg.IsValid())
                continue;

            d = vector.Distance(cachedCenter, seg.m_To);
            if (d > maxDist)
            {
                maxDist = d;
            }
        }

        cachedRadius = maxDist;
    }

    // Update hysteresis state after occlusion check.
    // blocked = true if ALL sample points are occluded.
    // v0.7.32 (Audit P2): Accept distance to scale recheck interval.
    // Wires near the culling bubble edge get longer intervals to reduce
    // flickering from rapid visibility changes during player movement.
    // v0.7.38: Asymmetric hysteresis — fast to hide (2 checks), slow to
    // reveal (3 checks). Prevents "ghost cables" behind walls while
    // avoiding pop-in on reveal. Also: high occBlockedRatio (>=0.66)
    // counts as "blocked" for hysteresis to handle edge-of-wall cases
    // where 1 of 3 samples barely passes through.
    void UpdateOcclusion(bool blocked, float nowMs, float wireDist)
    {
        // v0.7.32 (Audit P2): Distance-scaled interval.
        // Close wires: 250ms (base). Far wires (near bubble edge): up to ~750ms.
        float distScale = 1.0;
        if (wireDist > 0.0 && LFPG_CULL_DISTANCE_M > 0.0)
        {
            distScale = 1.0 + (wireDist / LFPG_CULL_DISTANCE_M) * 2.0;
            if (distScale > LFPG_OCC_DIST_SCALE_MAX)
            {
                distScale = LFPG_OCC_DIST_SCALE_MAX;
            }
        }
        occNextCheckMs = nowMs + LFPG_OCC_INTERVAL_MS * distScale;

        // v0.7.38: High blocked ratio counts as blocked for hysteresis.
        // Fixes edge-of-wall case: 2 of 3 samples blocked (ratio>=0.66)
        // but "all blocked" never triggers → counter resets every check.
        if (blocked || occBlockedRatio >= LFPG_OCC_PARTIAL_THRESHOLD)
        {
            if (occConsecCount > 0)
            {
                occConsecCount = 0;
            }
            occConsecCount = occConsecCount - 1;

            // v0.7.38: Asymmetric — HIDE threshold (fast)
            if (occConsecCount <= -LFPG_OCC_HYSTERESIS_HIDE)
            {
                occluded = true;
            }
        }
        else
        {
            if (occConsecCount < 0)
            {
                occConsecCount = 0;
            }
            occConsecCount = occConsecCount + 1;

            // v0.7.38: Asymmetric — SHOW threshold (cautious)
            if (occConsecCount >= LFPG_OCC_HYSTERESIS_SHOW)
            {
                occluded = false;
            }
        }
    }

    void SetVisible(bool vis)
    {
        // v0.7.33 (Fix P6): When wire goes invisible via culling,
        // reset occlusion hysteresis. Without this, a wire re-entering
        // visibility drags old occConsecCount — causing delayed
        // occlusion transitions (e.g., stuck half-faded for 3+ frames).
        // On re-entry, wire starts fresh: assume visible, let raycasts
        // determine actual state from clean baseline.
        if (!vis && visible)
        {
            occConsecCount = 0;
            occBlockedRatio = 0.0;
            occluded = false;
        }
        visible = vis;
    }

    void DestroyAll()
    {
        if (!segments)
            return;

        int i;
        for (i = 0; i < segments.Count(); i = i + 1)
        {
            if (segments[i])
            {
                segments[i].Destroy();
            }
        }
        segments.Clear();
    }

    void ~LFPG_WireSegmentInfo()
    {
        DestroyAll();
    }
};

// Retry entry for wires whose target could not be resolved at build time
class LFPG_RetryEntry
{
    string ownerDeviceId;
    int wireIndex;
    int retryCount;

    // v0.7.10: Reason for retry (TARGET_MISSING vs BUDGET).
    // Budget retries do not count toward retry limit.
    int reason;  // LFPG_RetryReason enum

    // v0.7.38 (M11): Creation timestamp for TTL expiry.
    // BUDGET entries without TTL accumulate indefinitely in long sessions.
    float createdMs;
};

class LFPG_CableRenderer
{
    protected static ref LFPG_CableRenderer s_Instance;

    // v0.7.35 D1: Cooldown map for REQUEST_DEVICE_SYNC RPCs.
    // Maps deviceId -> tick time of last request. Prevents RPC spam
    // when entering a large base with many devices.
    protected static ref map<string, float> s_DeviceSyncCooldowns;

    protected ref map<string, ref LFPG_OwnerWireState> m_ByOwnerId;

    // Wire segment info (key = "ownerId|wireIdx")
    protected ref map<string, ref LFPG_WireSegmentInfo> m_WireSegments;

    // Retry queue: wires that failed to build due to unresolved targets.
    // Keyed by wireKey ("ownerId|wireIdx").
    protected ref map<string, ref LFPG_RetryEntry> m_RetryQueue;

    // Pre-allocated temp arrays (avoids GC pressure in helpers)
    protected ref array<vector> m_TempPoints;
    protected ref array<vector> m_SagPoints;     // catenaria output buffer
    protected ref array<string> m_TempKeys;      // reused in DestroyOwnerLines etc.
    protected ref array<string> m_GhostKeys;     // v0.7.38 (L4): reused for ghost owner cleanup

    // v0.7.9: Per-frame screen projection cache.
    // Stores projected screen coords for all unique points of a wire.
    // Avoids redundant GetScreenPos calls across multi-pass drawing.
    protected ref array<vector> m_ScreenPts;
    protected ref array<vector> m_JointScreenPts;  // v0.7.9: reused for joint projection cache

    // Connection cache: key = "deviceId|portName|dir" -> value = type name
    protected ref map<string, string> m_ConnCache;

    // Negative resolution cache: deviceIds that failed to resolve recently.
    // Avoids re-scanning for unresolvable entities on every RetryTick.
    protected ref map<string, float> m_NegCache;
    protected static const float NEG_CACHE_TTL_MS = 5000.0; // 5 seconds
    // v0.7.45 (P0 fix): Set by ResolveDeviceEntityEx when NegCache blocked
    // the resolution attempt. RetryTick uses this to avoid wasting retries.
    protected bool m_LastResolveWasNegCached;

    // Periodic neg cache purge interval (ms)
    protected static const int NEG_CACHE_PURGE_INTERVAL_MS = 60000; // 60 seconds

    // v0.7.9: Incremental segment budget counter.
    // Updated in BuildWire (+) and DestroyWire (-) to avoid O(N) CountTotalSegments.
    protected int m_TotalSegCount;

    // ---- Occlusion: camera movement detection ----
    // Skip occlusion rechecks when camera is stationary.
    protected vector m_LastCamPos;
    protected vector m_LastCamDir;
    protected bool   m_CamMoved;       // set per frame in DrawFrame

    // ---- Occlusion: stagger round-robin ----
    // Distributes raycast cost across frames.
    protected int    m_OccStaggerIdx;

    // v0.7.7: cached device bubble distance (read once from settings)
    protected float  m_DeviceBubbleM;

    // v0.7.23 (Bug 2): Painter's algorithm sort buffers.
    // Indices into m_WireSegments sorted by cachedMinDist descending (far-to-near).
    protected ref array<int>   m_DrawOrder;
    protected ref array<float> m_DrawDist;

    // v0.7.35 (F1.2): Cohen-Sutherland screen clipping temp output vectors.
    // Reused per-segment to avoid allocation. [0]=x, [1]=y.
    protected vector m_ClipA;
    protected vector m_ClipB;

    void LFPG_CableRenderer()
    {
        m_ByOwnerId      = new map<string, ref LFPG_OwnerWireState>;
        m_WireSegments   = new map<string, ref LFPG_WireSegmentInfo>;
        m_RetryQueue      = new map<string, ref LFPG_RetryEntry>;
        m_TempPoints      = new array<vector>;
        m_SagPoints       = new array<vector>;
        m_TempKeys        = new array<string>;
        m_GhostKeys       = new array<string>;
        m_ScreenPts       = new array<vector>;
        m_JointScreenPts  = new array<vector>;
        m_ConnCache       = new map<string, string>;
        m_NegCache        = new map<string, float>;
        m_LastResolveWasNegCached = false;
        m_DrawOrder       = new array<int>;
        m_DrawDist        = new array<float>;
        m_ClipA           = "0 0 0";
        m_ClipB           = "0 0 0";
        m_OccStaggerIdx   = 0;
        m_CamMoved        = true;
        m_TotalSegCount   = 0;

        // v0.7.7: read bubble setting once
        LFPG_ServerSettings cfg = LFPG_Settings.Get();
        if (cfg)
        {
            m_DeviceBubbleM = cfg.DeviceBubbleM;
        }
        else
        {
            m_DeviceBubbleM = LFPG_DEVICE_BUBBLE_M;
        }

        if (!GetGame().IsDedicatedServer())
        {
            bool bRepeat = true;
            // Lightweight culling tick (replaces the old 0.5s full Refresh)
            GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(CullTick, (int)(LFPG_CULL_TICK_S * 1000.0), bRepeat);

            // Retry tick for unresolved wire targets
            GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(RetryTick, (int)(LFPG_RETRY_TICK_S * 1000.0), bRepeat);

            // Periodic negative cache cleanup
            GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(PurgeNegCache, NEG_CACHE_PURGE_INTERVAL_MS, bRepeat);
        }
    }

    static LFPG_CableRenderer Get()
    {
        if (GetGame().IsDedicatedServer())
            return null;

        if (!s_Instance)
            s_Instance = new LFPG_CableRenderer();
        return s_Instance;
    }

    // v0.7.9: proper cleanup on destruction.
    // Deregisters repeating timers, releases all shape segments, and clears maps.
    // Without this, Reset() during reconnect would leave orphaned CallLater
    // timers pointing to the old instance, causing duplicate ticks and crashes.
    void ~LFPG_CableRenderer()
    {
        if (GetGame())
        {
            ScriptCallQueue cq = GetGame().GetCallQueue(CALL_CATEGORY_GUI);
            if (cq)
            {
                cq.Remove(CullTick);
                cq.Remove(RetryTick);
                cq.Remove(PurgeNegCache);
            }
        }

        DestroyAll();
        m_ByOwnerId.Clear();
        m_ConnCache.Clear();
        m_NegCache.Clear();
    }

    // v0.7.5: called from MissionGameplay.OnInit to ensure a
    // clean slate when reconnecting to a server. Destroys all
    // segments and caches from the previous session.
    static void Reset()
    {
        // v0.7.36 (M3): Clear static cooldown map to prevent stale
        // throttle entries from a previous server session.
        s_DeviceSyncCooldowns = null;

        if (s_Instance)
        {
            delete s_Instance;
            s_Instance = null;
        }
    }

    // v0.7.23 (Bug 4): Force rebuild of all cable geometry.
    // Destroys all segments and rebuilds from the stored wire data.
    // Fixes cables that disappear due to stale geometry, failed retries,
    // or occlusion state corruption. Can be called via admin command or
    // periodic self-heal timer.
    void ForceGlobalRefresh()
    {
        ref array<string> ownerIds = new array<string>;
        int i;
        for (i = 0; i < m_ByOwnerId.Count(); i = i + 1)
        {
            ownerIds.Insert(m_ByOwnerId.GetKey(i));
        }

        int k;
        for (k = 0; k < ownerIds.Count(); k = k + 1)
        {
            string ownerId = ownerIds[k];
            DestroyOwnerLines(ownerId);
            ClearOwnerRetries(ownerId);
            BuildOwnerWires(ownerId);
        }

        string fgrMsg = "[CableRenderer] ForceGlobalRefresh: rebuilt " + ownerIds.Count().ToString() + " owners, totalSegs=" + m_TotalSegCount.ToString();
        LFPG_Util.Info(fgrMsg);
    }

    // ===========================
    // Entity resolution (client-side)
    // ===========================
    protected EntityAI ResolveDeviceEntity(string deviceId)
    {
        if (deviceId == "")
            return null;

        // 1. DeviceRegistry (works for LFPG devices on client)
        EntityAI found = LFPG_DeviceRegistry.Get().FindById(deviceId);
        if (found)
        {
            if (LFPG_DIAG_ENABLED)
            {
                LFPG_Diag.ServerEcho("[Resolve] HIT registry id=" + deviceId + " type=" + found.GetType());
            }
            return found;
        }

        // 2. Check negative cache: skip if recently failed
        float failTime = 0.0;
        float nowMs = GetGame().GetTime();
        if (m_NegCache.Find(deviceId, failTime))
        {
            float age = nowMs - failTime;
            if (age < NEG_CACHE_TTL_MS)
            {
                if (LFPG_DIAG_ENABLED)
                {
                    LFPG_Diag.ServerEcho("[Resolve] NegCache block id=" + deviceId + " age=" + age.ToString());
                }
                return null;
            }
            m_NegCache.Remove(deviceId);
        }

        // 3. Vanilla position-based ID: "vp:TYPE:QX:QY:QZ"
        if (deviceId.IndexOf("vp:") == 0)
        {
            EntityAI vObj = LFPG_DeviceAPI.ResolveVanillaDevice(deviceId);
            if (vObj)
            {
                if (LFPG_DIAG_ENABLED)
                {
                    LFPG_Diag.ServerEcho("[Resolve] HIT vanilla id=" + deviceId + " type=" + vObj.GetType());
                }
                return vObj;
            }
        }

        // 4. Resolution failed
        m_NegCache[deviceId] = nowMs;
        if (LFPG_DIAG_ENABLED)
        {
            LFPG_Diag.ServerEcho("[Resolve] MISS id=" + deviceId + " -> NegCache");
        }
        return null;
    }

    // v0.7.45 (Patch 3C): Extended resolver with NetworkID fallback.
    // Tries DeviceRegistry first, then NetworkID (bypasses NegCache),
    // then vanilla spatial, then NegCache check as last resort.
    // This eliminates invisible cables during SyncVar lag post-kit-placement.
    protected EntityAI ResolveDeviceEntityEx(string deviceId, int netLow, int netHigh)
    {
        // v0.7.45 (P0 fix): Track whether NegCache blocked this attempt.
        // RetryTick checks this flag to avoid wasting retries.
        m_LastResolveWasNegCached = false;

        if (deviceId == "")
            return null;

        // 1. DeviceRegistry (fastest path)
        EntityAI found = LFPG_DeviceRegistry.Get().FindById(deviceId);
        if (found)
        {
            return found;
        }

        // 2. NetworkID fallback: bypasses NegCache entirely.
        // If netLow/netHigh are available, the entity exists even if
        // DeviceRegistry hasn't registered it yet (SyncVar lag window).
        if (netLow != 0 || netHigh != 0)
        {
            EntityAI netObj = EntityAI.Cast(GetGame().GetObjectByNetworkId(netLow, netHigh));
            if (netObj)
            {
                if (LFPG_DIAG_ENABLED)
                {
                    LFPG_Diag.ServerEcho("[ResolveEx] HIT NetworkID low=" + netLow.ToString() + " high=" + netHigh.ToString() + " type=" + netObj.GetType());
                }
                // Clear any stale NegCache entry for this deviceId
                m_NegCache.Remove(deviceId);
                return netObj;
            }
        }

        // 3. Check negative cache (only reached if NetworkID also failed)
        float failTime = 0.0;
        float nowMs = GetGame().GetTime();
        if (m_NegCache.Find(deviceId, failTime))
        {
            float age = nowMs - failTime;
            if (age < NEG_CACHE_TTL_MS)
            {
                // v0.7.45 (P0 fix): Signal that NegCache blocked this attempt
                m_LastResolveWasNegCached = true;
                return null;
            }
            m_NegCache.Remove(deviceId);
        }

        // 4. Vanilla position-based ID fallback
        if (deviceId.IndexOf("vp:") == 0)
        {
            EntityAI vObj = LFPG_DeviceAPI.ResolveVanillaDevice(deviceId);
            if (vObj)
            {
                return vObj;
            }
        }

        // 5. All failed: add to NegCache
        m_NegCache[deviceId] = nowMs;
        if (LFPG_DIAG_ENABLED)
        {
            LFPG_Diag.ServerEcho("[ResolveEx] MISS id=" + deviceId + " net=" + netLow.ToString() + ":" + netHigh.ToString() + " -> NegCache");
        }
        return null;
    }

    // Periodic purge of expired negative cache entries.
    protected void PurgeNegCache()
    {
        if (m_NegCache.Count() == 0)
            return;

        float nowMs = GetGame().GetTime();

        m_TempKeys.Clear();
        int i;
        for (i = 0; i < m_NegCache.Count(); i = i + 1)
        {
            float entryTime = m_NegCache.GetElement(i);
            if ((nowMs - entryTime) >= NEG_CACHE_TTL_MS)
            {
                m_TempKeys.Insert(m_NegCache.GetKey(i));
            }
        }

        int k;
        for (k = 0; k < m_TempKeys.Count(); k = k + 1)
        {
            m_NegCache.Remove(m_TempKeys[k]);
        }

        if (m_TempKeys.Count() > 0)
        {
            string ncMsg = "[CableRenderer] NegCache purged " + m_TempKeys.Count().ToString() + " expired entries";
            LFPG_Util.Debug(ncMsg);
        }
    }

    // ===========================
    // Power state detection (client-side)
    // ===========================
    // v0.7.29 (Audit fix): Separate LFPG path from vanilla CompEM path.
    // Previously, if GetSourceOn returned false (no sparkplug), the
    // fallback to em.IsWorking() could return true when CompEM was
    // activated via vanilla C++ paths. This caused cables to show
    // green (POWERED) even without a valid sparkplug.
    // Now: LFPG-native devices (with device ID) use GetSourceOn
    // exclusively. Only vanilla devices fall back to CompEM.
    protected bool IsOwnerActive(EntityAI ownerObj)
    {
        if (!ownerObj)
            return false;

        // Check if this is an LFPG-native device (has LFPG device ID)
        // GetDeviceId calls LFPG_GetDeviceId via dynamic dispatch.
        // Returns "" for vanilla devices (no such method).
        string devId = LFPG_DeviceAPI.GetDeviceId(ownerObj);
        if (devId != "")
        {
            // LFPG device: use GetSourceOn exclusively.
            // This includes sparkplug validation and health checks.
            return LFPG_DeviceAPI.GetSourceOn(ownerObj);
        }

        // Non-LFPG (vanilla) device: use CompEM as source of truth
        ComponentEnergyManager em = ownerObj.GetCompEM();
        if (em)
        {
            return em.IsWorking();
        }

        return false;
    }

    // ===========================
    // Wire data ingestion (RPC event)
    // ===========================
    void UpsertOwnerBlob(string ownerDeviceId, int low, int high, string json)
    {
        if (ownerDeviceId == "")
            return;

        m_NegCache.Remove(ownerDeviceId);

        string uobMsg = "[CableRenderer] UpsertOwnerBlob owner=" + ownerDeviceId + " net=" + low.ToString() + ":" + high.ToString() + " jsonLen=" + json.Length().ToString();
        LFPG_Util.Info(uobMsg);
        if (LFPG_DIAG_ENABLED)
        {
            LFPG_Diag.ServerEcho("[CableRenderer] UpsertOwnerBlob owner=" + ownerDeviceId + " jsonLen=" + json.Length().ToString());
        }

        ref LFPG_OwnerWireState st;
        if (!m_ByOwnerId.Find(ownerDeviceId, st) || !st)
        {
            st = new LFPG_OwnerWireState();
            st.ownerDeviceId = ownerDeviceId;
            m_ByOwnerId[ownerDeviceId] = st;
        }

        st.ownerLow = low;
        st.ownerHigh = high;

        if (st.lastJson != json)
        {
            // v0.7.10 P1: Try decode FIRST. If it fails, keep old topology intact.
            // This prevents transient JSON errors from wiping visible cables.
            bool decodeOk = DecodeOwner(st, json);
            if (!decodeOk)
            {
                // Parse failed: don't update lastJson (allows retry on next RPC),
                // don't destroy old segments (cables remain visible).
                string uobWarn = "[CableRenderer] UpsertOwnerBlob: decode failed, keeping previous state owner=" + ownerDeviceId;
                LFPG_Util.Warn(uobWarn);
                return;
            }

            // Decode succeeded: now safe to destroy old geometry and rebuild
            st.lastJson = json;

            // v0.7.45 (U6): Clear NegCache for ALL target deviceIds in decoded wires.
            // Without this, when a second blob arrives via RequestDeviceSync,
            // UpsertOwnerBlob only clears NegCache for the OWNER, but targets
            // may still be blocked (age < 5s from first failed BuildOwnerWires).
            // This adds 5-6s of unnecessary latency to cable appearance.
            if (st.wires)
            {
                int nci;
                for (nci = 0; nci < st.wires.Count(); nci = nci + 1)
                {
                    if (st.wires[nci] && st.wires[nci].m_TargetDeviceId != "")
                    {
                        m_NegCache.Remove(st.wires[nci].m_TargetDeviceId);
                    }
                }
            }

            // v0.7.35 D2: Reset visual state masks on topology change.
            // Old mask bits may map to different wires after add/remove.
            // CullTick or NotifyOwnerVisualChanged will repopulate from SyncVars.
            st.lastOverloadMask = 0;
            st.lastWarningMask  = 0;
            st.lastLoadRatio    = 0.0;

            // Topology changed: destroy old segments + clear retries for this owner
            DestroyOwnerLines(ownerDeviceId);
            ClearOwnerRetries(ownerDeviceId);
            RebuildConnCache();

            // Immediately build wire segments (frozen geometry)
            BuildOwnerWires(ownerDeviceId);

            int wireCount = 0;
            if (st.wires)
            {
                wireCount = st.wires.Count();
            }

            // v0.7.3: clean up empty owners to prevent slow memory leak
            if (wireCount == 0)
            {
                m_ByOwnerId.Remove(ownerDeviceId);
                string remMsg = "[CableRenderer] Removed empty owner=" + ownerDeviceId;
                LFPG_Util.Debug(remMsg);
                return;
            }

            string bltMsg = "[CableRenderer] Built owner=" + ownerDeviceId + " wires=" + wireCount.ToString() + " pending=" + m_RetryQueue.Count().ToString();
            LFPG_Util.Info(bltMsg);
        }
        else
        {
            string skipMsg = "[CableRenderer] UpsertOwnerBlob SKIP (json unchanged) owner=" + ownerDeviceId;
            LFPG_Util.Debug(skipMsg);
        }
    }

    // ==========================================================
    // v0.7.35 D1: Check if renderer already has data for an owner
    // ==========================================================
    bool HasOwnerData(string ownerDeviceId)
    {
        if (ownerDeviceId == "") return false;

        ref LFPG_OwnerWireState st;
        if (m_ByOwnerId.Find(ownerDeviceId, st) && st)
        {
            return true;
        }
        return false;
    }

    // ==========================================================
    // v0.7.35 D4: Immediate visual state refresh from SyncVars.
    // Called from OnVariablesSynchronized on owner devices
    // (Generator, Splitter) to eliminate the 0-2s CullTick delay.
    // ==========================================================
    void NotifyOwnerVisualChanged(string ownerDeviceId)
    {
        if (ownerDeviceId == "") return;

        ref LFPG_OwnerWireState st;
        if (!m_ByOwnerId.Find(ownerDeviceId, st) || !st) return;

        EntityAI ownerObj = EntityAI.Cast(GetGame().GetObjectByNetworkId(st.ownerLow, st.ownerHigh));
        if (!ownerObj) return;

        st.lastPowered      = IsOwnerActive(ownerObj);
        st.lastLoadRatio     = LFPG_DeviceAPI.GetLoadRatio(ownerObj);
        st.lastOverloadMask  = LFPG_DeviceAPI.GetOverloadMask(ownerObj);
        st.lastWarningMask   = LFPG_DeviceAPI.GetWarningMask(ownerObj);

        string nvcMsg = "[CableRenderer] NotifyOwnerVisualChanged owner=" + ownerDeviceId + " load=" + st.lastLoadRatio.ToString() + " overload=" + st.lastOverloadMask.ToString() + " warning=" + st.lastWarningMask.ToString();
        LFPG_Util.Debug(nvcMsg);
    }

    // ==========================================================
    // v0.7.35 D1: Client sends REQUEST_DEVICE_SYNC RPC.
    // Cooldown per deviceId prevents spam when entering large bases.
    // Server responds with owner wire blobs relevant to this device.
    // ==========================================================
    // v0.7.45 (H7): Added device parameter for NetworkID-first resolution.
    // All callers are device classes calling from OnVariablesSynchronized
    // where `this` is the entity. NetworkID is written to the RPC so the
    // server can resolve authoritatively even during SyncVar lag.
    void RequestDeviceSync(string deviceId, EntityAI device)
    {
        if (deviceId == "") return;

        if (!s_DeviceSyncCooldowns)
        {
            s_DeviceSyncCooldowns = new map<string, float>;
        }

        float now = GetGame().GetTickTime();

        // Check cooldown: skip if recently requested
        float lastReq;
        if (s_DeviceSyncCooldowns.Find(deviceId, lastReq))
        {
            if ((now - lastReq) < LFPG_DEVICE_SYNC_COOLDOWN_S)
            {
                string thrMsg = "[CableRenderer] RequestDeviceSync THROTTLED deviceId=" + deviceId;
                LFPG_Util.Debug(thrMsg);
                return;
            }
        }

        s_DeviceSyncCooldowns[deviceId] = now;

        // Periodic purge of stale entries to prevent unbounded map growth
        if (s_DeviceSyncCooldowns.Count() > 50)
        {
            PurgeStaleDeviceSyncCooldowns(now);
        }

        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player) return;

        // v0.7.45 (H7): Extract NetworkID from entity for server-side
        // resolution. Consistent with InspectDevice (v0.7.43 fix).
        int netLow = 0;
        int netHigh = 0;
        if (device)
        {
            netLow = device.GetNetworkIDLow();
            netHigh = device.GetNetworkIDHigh();
        }

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.REQUEST_DEVICE_SYNC);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Write(deviceId);
        bool bRpcGuaranteed = true;
        PlayerIdentity noExclude = null;
        rpc.Send(player, LFPG_RPC_CHANNEL, bRpcGuaranteed, noExclude);

        string rdsMsg = "[CableRenderer] RequestDeviceSync sent deviceId=" + deviceId;
        rdsMsg = rdsMsg + " net=" + netLow.ToString() + ":" + netHigh.ToString();
        LFPG_Util.Info(rdsMsg);
    }

    // Helper: remove cooldown entries older than 60s
    protected void PurgeStaleDeviceSyncCooldowns(float now)
    {
        array<string> toRemove = new array<string>;
        int i;
        for (i = 0; i < s_DeviceSyncCooldowns.Count(); i = i + 1)
        {
            if ((now - s_DeviceSyncCooldowns.GetElement(i)) > 60.0)
            {
                toRemove.Insert(s_DeviceSyncCooldowns.GetKey(i));
            }
        }
        for (i = 0; i < toRemove.Count(); i = i + 1)
        {
            s_DeviceSyncCooldowns.Remove(toRemove[i]);
        }
    }

    protected bool DecodeOwner(LFPG_OwnerWireState st, string json)
    {
        // v0.7.10 P1: Parse into temporary structure first.
        // Only overwrite st.wires on SUCCESS. If JSON parse fails,
        // conserve previous topology (fail-soft). Prevents transient
        // network errors from wiping the client's cable geometry.
        // Returns true on success, false on parse failure.

        if (json == "")
        {
            // Empty JSON = explicit "no wires" from server. This is valid.
            st.wires = new array<ref LFPG_WireData>;
            return true;
        }

        LFPG_PersistBlob blob = new LFPG_PersistBlob();
        string err;
        if (!JsonFileLoader<LFPG_PersistBlob>.LoadData(json, blob, err))
        {
            // Parse failed: keep st.wires as-is (may be null or previous data)
            string decMsg = "CableRenderer: decode failed owner=" + st.ownerDeviceId + " err=" + err + " -> KEEPING previous topology";
            LFPG_Util.Warn(decMsg);
            return false;
        }

        if (!blob || !blob.wires)
        {
            // Parsed OK but no wires inside — treat as empty.
            st.wires = new array<ref LFPG_WireData>;
            return true;
        }

        // v0.7.10 P2: Wire count limit per owner on client side.
        // Prevents malicious/bugged JSON from flooding the client.
        int srcCount = blob.wires.Count();
        if (srcCount > LFPG_MAX_WIRES_PER_OWNER_CLIENT)
        {
            string clampMsg = "CableRenderer: owner=" + st.ownerDeviceId + " wire count " + srcCount.ToString() + " exceeds client limit " + LFPG_MAX_WIRES_PER_OWNER_CLIENT.ToString() + " -> CLAMPING";
            LFPG_Util.Warn(clampMsg);
            srcCount = LFPG_MAX_WIRES_PER_OWNER_CLIENT;
        }

        // Build into temporary array; only assign to st.wires at the end.
        ref array<ref LFPG_WireData> parsed = new array<ref LFPG_WireData>;

        if (LFPG_DIAG_ENABLED)
        {
            LFPG_Diag.ServerEcho("[CableRenderer] DecodeOwner " + st.ownerDeviceId + " wires=" + blob.wires.Count().ToString());
        }
        int i;
        for (i = 0; i < srcCount; i = i + 1)
        {
            LFPG_WireData dwd = blob.wires[i];
            if (!dwd)
                continue;

            // Skip garbage entries (empty target = corrupt data)
            if (dwd.m_TargetDeviceId == "" || dwd.m_TargetPort == "")
                continue;

            parsed.Insert(dwd);

            int wpCnt = 0;
            if (dwd.m_Waypoints)
            {
                wpCnt = dwd.m_Waypoints.Count();
            }
            if (LFPG_DIAG_ENABLED)
            {
                LFPG_Diag.ServerEcho("[CableRenderer] wire[" + i.ToString() + "] target=" + dwd.m_TargetDeviceId + " srcPort=" + dwd.m_SourcePort + " dstPort=" + dwd.m_TargetPort + " wps=" + wpCnt.ToString());
            }
        }

        // Success: replace wires with parsed data
        st.wires = parsed;
        return true;
    }

    // ===========================
    // Connection cache (O(1) lookups)
    // ===========================
    protected void RebuildConnCache()
    {
        m_ConnCache.Clear();

        int oi;
        for (oi = 0; oi < m_ByOwnerId.Count(); oi = oi + 1)
        {
            LFPG_OwnerWireState st = m_ByOwnerId.GetElement(oi);
            if (!st || !st.wires) continue;

            string ownerType = "";
            EntityAI ownerObj = EntityAI.Cast(GetGame().GetObjectByNetworkId(st.ownerLow, st.ownerHigh));
            if (ownerObj)
            {
                ownerType = ownerObj.GetType();
            }
            else
            {
                ownerType = st.ownerDeviceId;
            }

            int w;
            for (w = 0; w < st.wires.Count(); w = w + 1)
            {
                LFPG_WireData wd = st.wires[w];
                if (!wd) continue;

                string srcPort = wd.m_SourcePort;
                if (srcPort == "")
                {
                    srcPort = "output_1";
                }

                string targetType = "";
                EntityAI tgtObj = ResolveDeviceEntityEx(wd.m_TargetDeviceId, wd.m_TargetNetLow, wd.m_TargetNetHigh);
                if (tgtObj)
                {
                    targetType = tgtObj.GetType();
                }
                else
                {
                    targetType = wd.m_TargetDeviceId;
                }

                string outKey = st.ownerDeviceId + "|" + srcPort + "|" + LFPG_PortDir.OUT.ToString();
                m_ConnCache[outKey] = targetType;

                string tgtPort = wd.m_TargetPort;
                if (tgtPort == "")
                {
                    tgtPort = "input_main";
                }

                string inKey = wd.m_TargetDeviceId + "|" + tgtPort + "|" + LFPG_PortDir.IN.ToString();
                m_ConnCache[inKey] = ownerType;
            }
        }
    }

    // O(1) lookup - called from ActionCondition per-frame
    string GetConnectionType(string deviceId, string portName, int dir)
    {
        string key = deviceId + "|" + portName + "|" + dir.ToString();
        string val;
        if (m_ConnCache.Find(key, val))
        {
            return val;
        }
        return "";
    }

    // ===========================
    // Geometry build (event-driven, one-shot)
    // ===========================
    // Called once per UpsertOwnerBlob. Builds geometry for all
    // wires of an owner. Wires whose target cannot be resolved go to
    // the retry queue for deferred build.
    protected void BuildOwnerWires(string ownerDeviceId)
    {
        ref LFPG_OwnerWireState st;
        if (!m_ByOwnerId.Find(ownerDeviceId, st) || !st || !st.wires)
            return;

        string bowMsg = "[CableRenderer] BuildOwnerWires owner=" + ownerDeviceId + " net=" + st.ownerLow.ToString() + ":" + st.ownerHigh.ToString() + " wires=" + st.wires.Count().ToString();
        LFPG_Util.Info(bowMsg);

        EntityAI ownerObj = EntityAI.Cast(GetGame().GetObjectByNetworkId(st.ownerLow, st.ownerHigh));
        if (!ownerObj)
        {
            // Owner not yet loaded on client: queue ALL wires for retry
            string bowNullMsg = "[CableRenderer] BuildOwnerWires: ownerObj NULL net=" + st.ownerLow.ToString() + ":" + st.ownerHigh.ToString();
            LFPG_Util.Warn(bowNullMsg);
            if (LFPG_DIAG_ENABLED)
            {
                LFPG_Diag.ServerEcho("[CableRenderer] ownerObj NULL net=" + st.ownerLow.ToString() + ":" + st.ownerHigh.ToString());
            }
            int rw;
            for (rw = 0; rw < st.wires.Count(); rw = rw + 1)
            {
                AddRetry(ownerDeviceId, rw, LFPG_RetryReason.TARGET_MISSING);
            }
            return;
        }

        if (LFPG_DIAG_ENABLED)
        {
            LFPG_Diag.ServerEcho("[CableRenderer] ownerObj OK type=" + ownerObj.GetType() + " pos=" + ownerObj.GetPosition().ToString());
        }

        st.lastPowered = IsOwnerActive(ownerObj);
        st.lastLoadRatio = LFPG_DeviceAPI.GetLoadRatio(ownerObj);
        st.lastOverloadMask = LFPG_DeviceAPI.GetOverloadMask(ownerObj);
        st.lastWarningMask = LFPG_DeviceAPI.GetWarningMask(ownerObj);

        // v0.7.9: Pre-build wire keys for this owner (used by CullTick)
        st.cachedWireKeys = new array<string>;
        int wk;
        for (wk = 0; wk < st.wires.Count(); wk = wk + 1)
        {
            st.cachedWireKeys.Insert(ownerDeviceId + "|" + wk.ToString());
        }

        // Segment budget
        int totalSegs = m_TotalSegCount;

        // G5: get render metrics once outside loop
        LFPG_RenderMetrics bldTelRnd = LFPG_Telemetry.GetRender();

        int w;
        for (w = 0; w < st.wires.Count(); w = w + 1)
        {
            LFPG_WireData wd = st.wires[w];
            if (!wd) continue;

            // v0.7.45 (Patch 3C): Use extended resolver with NetworkID fallback
            EntityAI targetObj = ResolveDeviceEntityEx(wd.m_TargetDeviceId, wd.m_TargetNetLow, wd.m_TargetNetHigh);
            if (!targetObj)
            {
                string tgtNullMsg = "[CableRenderer] BuildOwnerWires: target NULL id=" + wd.m_TargetDeviceId + " -> RETRY";
                LFPG_Util.Warn(tgtNullMsg);
                if (LFPG_DIAG_ENABLED)
                {
                    LFPG_Diag.ServerEcho("[CableRenderer] target NULL id=" + wd.m_TargetDeviceId);
                }
                AddRetry(ownerDeviceId, w, LFPG_RetryReason.TARGET_MISSING);
                // G5: wire waiting for target entity resolution
                bldTelRnd.m_WiresResolving = bldTelRnd.m_WiresResolving + 1;
                continue;
            }

            if (LFPG_DIAG_ENABLED)
            {
                LFPG_Diag.ServerEcho("[CableRenderer] target OK type=" + targetObj.GetType() + " pos=" + targetObj.GetPosition().ToString());
            }

            // Compute endpoint positions
            string srcPort = wd.m_SourcePort;
            if (srcPort == "")
            {
                srcPort = "output_1";
            }
            vector a = LFPG_DeviceAPI.GetPortWorldPos(ownerObj, srcPort);
            vector b = LFPG_DeviceAPI.GetPortWorldPos(targetObj, wd.m_TargetPort);

            a = LFPG_WorldUtil.ClampAboveSurface(a);
            b = LFPG_WorldUtil.ClampAboveSurface(b);

            if (LFPG_DIAG_ENABLED)
            {
                LFPG_Diag.ServerEcho("[CableRenderer] portA=" + a.ToString() + " portB=" + b.ToString());
            }

            // Build raw point chain
            m_TempPoints.Clear();
            m_TempPoints.Insert(a);

            if (wd.m_Waypoints && wd.m_Waypoints.Count() > 0)
            {
                int j;
                for (j = 0; j < wd.m_Waypoints.Count(); j = j + 1)
                {
                    m_TempPoints.Insert(LFPG_WorldUtil.ClampAboveSurface(wd.m_Waypoints[j], LFPG_SURFACE_CLAMP_M));
                }
            }
            else
            {
                // No waypoints: auto midpoint prevents terrain clipping.
                // Catenaria sag is now applied adaptively per-segment by ApplyCatenaria.
                m_TempPoints.Insert(LFPG_WorldUtil.AutoMidpointAboveTerrain(a, b));
            }

            m_TempPoints.Insert(b);

            // v0.7.9: budget estimation uses adaptive subdivision count
            string wireKey = ownerDeviceId + "|" + w.ToString();
            int estSegs = EstimateSegments(m_TempPoints);
            if (totalSegs + estSegs > LFPG_MAX_RENDERED_SEGS)
            {
                string budgMsg = "[CableRenderer] Over segment budget, queue retry " + wireKey;
                LFPG_Util.Debug(budgMsg);
                AddRetry(ownerDeviceId, w, LFPG_RetryReason.BUDGET);
                // G5: wire skipped by segment budget
                bldTelRnd.m_WiresBudget = bldTelRnd.m_WiresBudget + 1;
                continue;
            }
            totalSegs = totalSegs + estSegs;

            // Create frozen wire segments
            BuildWire(wireKey, m_TempPoints, st.lastPowered, a, b, wd.m_Waypoints, w);
        }
    }

    // Build segments for a single wire. Geometry is frozen after creation.
    // v0.7.9: sagSubs removed — ApplyCatenaria is now self-contained.
    // waypoints: user-placed waypoints (for joint rendering at LOD close).
    // wireIdx: index of this wire in the owner's wire array (for overload mask).
    protected void BuildWire(string wireKey, array<vector> pts, bool powered, vector posA, vector posB, array<vector> waypoints, int wireIdx)
    {
        DestroyWire(wireKey);

        if (pts.Count() < 2)
        {
            string ptsMsg = "[CableRenderer] BuildWire: pts < 2 for " + wireKey;
            LFPG_Util.Warn(ptsMsg);
            return;
        }

        // v0.7.9: Compact near-duplicate points (< 5cm apart).
        // Prevents degenerate zero-length segments that cause rendering artifacts
        // (unstable endcap direction, NaN in normalization, zero-width draws).
        // Preserves first and last points (port anchors) unconditionally.
        if (pts.Count() > 2)
        {
            int ci = 1;
            while (ci < pts.Count() - 1)
            {
                float cdist = vector.Distance(pts[ci - 1], pts[ci]);
                if (cdist < 0.05)
                {
                    pts.Remove(ci);
                    // don't increment - check new element at same index
                }
                else
                {
                    ci = ci + 1;
                }
            }
        }

        if (pts.Count() < 2)
        {
            string ptsCMsg = "[CableRenderer] BuildWire: pts < 2 after compact for " + wireKey;
            LFPG_Util.Warn(ptsCMsg);
            return;
        }

        ApplyCatenaria(pts);

        if (m_SagPoints.Count() < 2)
        {
            string sagMsg = "[CableRenderer] BuildWire: sagPoints < 2 for " + wireKey;
            LFPG_Util.Warn(sagMsg);
            return;
        }

        ref LFPG_WireSegmentInfo info = new LFPG_WireSegmentInfo();
        info.powered = powered;
        info.cachedPosA = posA;
        info.cachedPosB = posB;

        int segCount = m_SagPoints.Count() - 1;
        int createdOk = 0;
        int createdFail = 0;
        int si;
        for (si = 0; si < segCount; si = si + 1)
        {
            ref LFPG_CableParticle seg = new LFPG_CableParticle();
            bool created = seg.Create(m_SagPoints[si], m_SagPoints[si + 1]);
            if (created)
            {
                info.segments.Insert(seg);
                createdOk = createdOk + 1;
            }
            else
            {
                createdFail = createdFail + 1;
            }
        }

        // v0.7.9: Abort if no segments were successfully created.
        // Prevents inserting an empty WireSegmentInfo that wastes map entry
        // and causes fallback behavior in DrawFrame/CullTick.
        if (createdOk <= 0)
        {
            string noSegMsg = "[CableRenderer] BuildWire: no valid segments for " + wireKey;
            LFPG_Util.Warn(noSegMsg);
            return;
        }

        // v0.7.7: compute bounding sphere from actual geometry
        info.BuildBoundingSphere();

        // v0.7.9: build occlusion samples from actual geometry
        // (must be after segments are created, since it walks the chain)
        info.BuildOccSamples();

        // v0.7.8: store user waypoints for joint rendering
        // v0.7.9: clamp joints same as segment points — prevents misaligned
        // joint markers if persisted waypoints have stale/bad coordinates.
        info.cachedJoints.Clear();
        if (waypoints)
        {
            int wi;
            for (wi = 0; wi < waypoints.Count(); wi = wi + 1)
            {
                info.cachedJoints.Insert(LFPG_WorldUtil.ClampAboveSurface(waypoints[wi], LFPG_SURFACE_CLAMP_M));
            }
        }

        // v0.7.8: set initial cable state
        if (powered)
        {
            info.cableState = LFPG_CableState.POWERED;
        }
        else
        {
            info.cableState = LFPG_CableState.IDLE;
        }

        // v0.7.8: store wire index for overload mask lookup
        info.wireIndex = wireIdx;
        // v0.7.38 (H6): Stable stagger group from wireIndex.
        info.occStaggerGroup = wireIdx % 3;

        // v0.7.9: pre-compute wire key to avoid string concat in CullTick
        info.cachedWireKey = wireKey;

        m_WireSegments[wireKey] = info;

        // v0.7.9: update incremental segment counter
        m_TotalSegCount = m_TotalSegCount + createdOk;

        if (LFPG_DIAG_ENABLED)
        {
            LFPG_Diag.ServerEcho("[CableRenderer] BuildWire " + wireKey + " segs=" + createdOk.ToString() + "/" + segCount.ToString() + " center=" + info.cachedCenter.ToString() + " radius=" + info.cachedRadius.ToString());
        }
        if (createdFail > 0)
        {
            string failMsg = "[CableRenderer] BuildWire " + wireKey + " FAILED segs=" + createdFail.ToString();
            LFPG_Util.Warn(failMsg);
        }
    }

    // ===========================
    // CullTick - lightweight visibility + powered check
    // ===========================
    // Runs every 2s. Zero geometry computation, zero entity resolution
    // for built wires, zero hash calculation. Only:
    //   1. Distance check against cached positions -> Play/Stop
    //   2. Powered state check (if entity available)
    //   3. Bounding sphere culling (v0.7.7)
    //   4. Device bubble culling (v0.7.7)
    //   5. Owner early-out (v0.7.7)
    //   6. Compute cachedMinDist for LOD/alpha (v0.7.7)
    protected void CullTick()
    {
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player) return;

        vector pp = player.GetPosition();
        float bubbleM = m_DeviceBubbleM;

        // v0.7.9: prepare deferred removal list for ghost owners
        m_TempKeys.Clear();

        // v0.7.38 (C3): Guard entire log block with DIAG check.
        // Prevents 5+ string concatenations per CullTick when diagnostics off.
        int debugTick = GetGame().GetTime();
        bool doCullLog = false;
        if (LFPG_DIAG_ENABLED)
        {
            doCullLog = (debugTick % 30000 < 2000);
            if (doCullLog)
            {
                string rd = "[CableRenderer] CullTick: owners=" + m_ByOwnerId.Count().ToString();
                rd = rd + " wireSegs=" + m_WireSegments.Count().ToString();
                rd = rd + " retries=" + m_RetryQueue.Count().ToString();
                rd = rd + " playerPos=" + pp.ToString();
                rd = rd + " bubble=" + bubbleM.ToString();
                LFPG_Diag.ServerEcho(rd);
            }
        }

        // v0.7.11 (A3): Precompute squared thresholds outside loop.
        // Avoids recomputing per-wire; all distance comparisons use DistSq domain.
        float cullDistSq = LFPG_CULL_DISTANCE_M * LFPG_CULL_DISTANCE_M;
        float earlyOutDist = LFPG_CULL_DISTANCE_M + 25.0;
        float earlyOutDistSq = earlyOutDist * earlyOutDist;
        float bubbleSq = bubbleM * bubbleM;

        int i;
        for (i = 0; i < m_ByOwnerId.Count(); i = i + 1)
        {
            LFPG_OwnerWireState st = m_ByOwnerId.GetElement(i);
            if (!st || !st.wires) continue;

            // Update powered state (only if entity is available)
            EntityAI ownerObj = EntityAI.Cast(GetGame().GetObjectByNetworkId(st.ownerLow, st.ownerHigh));
            if (ownerObj)
            {
                // Owner entity is valid: reset null counter
                st.nullOwnerTicks = 0;

                st.lastPowered = IsOwnerActive(ownerObj);

                // v0.7.8: read synced load ratio from source
                st.lastLoadRatio = LFPG_DeviceAPI.GetLoadRatio(ownerObj);

                // v0.7.8: read overload bitmask from owner
                st.lastOverloadMask = LFPG_DeviceAPI.GetOverloadMask(ownerObj);

                // v0.7.35 (F1.3): read warning bitmask from owner
                st.lastWarningMask = LFPG_DeviceAPI.GetWarningMask(ownerObj);

                // v0.7.7: Owner early-out.
                // If the owner entity itself is farther than cull distance + margin,
                // skip processing all its individual wires (saves iteration).
                // v0.7.11 (A3): Compare in squared domain — eliminates 1 sqrt per owner.
                float ownerDistSq = LFPG_WorldUtil.DistSq(pp, ownerObj.GetPosition());
                if (ownerDistSq > earlyOutDistSq)
                {
                    // Hide all wires for this owner
                    int ew;
                    for (ew = 0; ew < st.wires.Count(); ew = ew + 1)
                    {
                        // v0.7.9: use pre-computed key if available
                        string ewKey;
                        if (st.cachedWireKeys && ew < st.cachedWireKeys.Count())
                        {
                            ewKey = st.cachedWireKeys[ew];
                        }
                        else
                        {
                            ewKey = st.ownerDeviceId + "|" + ew.ToString();
                        }
                        ref LFPG_WireSegmentInfo ewInfo;
                        if (m_WireSegments.Find(ewKey, ewInfo) && ewInfo)
                        {
                            bool bHide = false;
                            ewInfo.SetVisible(bHide);
                        }
                    }
                    continue; // Skip per-wire checks for this owner
                }
            }
            else
            {
                // v0.7.9: Owner entity is null (destroyed or streamed out).
                // Hide all wires immediately to prevent ghost rendering.
                // After 15 consecutive null ticks (~30s) AND no wires near player,
                // queue for full cleanup. Higher threshold + distance check prevents
                // premature destruction during streaming hiccups/net desync.
                st.nullOwnerTicks = st.nullOwnerTicks + 1;

                int hw;
                for (hw = 0; hw < st.wires.Count(); hw = hw + 1)
                {
                    string hwKey;
                    if (st.cachedWireKeys && hw < st.cachedWireKeys.Count())
                    {
                        hwKey = st.cachedWireKeys[hw];
                    }
                    else
                    {
                        hwKey = st.ownerDeviceId + "|" + hw.ToString();
                    }
                    ref LFPG_WireSegmentInfo hwInfo;
                    if (m_WireSegments.Find(hwKey, hwInfo) && hwInfo)
                    {
                        bool bHideHw = false;
                        hwInfo.SetVisible(bHideHw);
                    }
                }

                if (st.nullOwnerTicks >= 15)
                {
                    // Entity gone for 30+ seconds: likely destroyed.
                    // But first check if any wire geometry is near the player.
                    // If so, this could be a streaming/net hiccup — don't destroy yet.
                    bool anyWireNearPlayer = false;
                    int tw;
                    for (tw = 0; tw < st.wires.Count(); tw = tw + 1)
                    {
                        string twKey;
                        if (st.cachedWireKeys && tw < st.cachedWireKeys.Count())
                        {
                            twKey = st.cachedWireKeys[tw];
                        }
                        else
                        {
                            twKey = st.ownerDeviceId + "|" + tw.ToString();
                        }
                        ref LFPG_WireSegmentInfo twInfo;
                        if (m_WireSegments.Find(twKey, twInfo) && twInfo)
                        {
                            if (twInfo.cachedMinDist < LFPG_CULL_DISTANCE_M)
                            {
                                anyWireNearPlayer = true;
                                break;
                            }
                        }
                    }

                    if (!anyWireNearPlayer)
                    {
                        // All wires are far from player AND entity gone 30s → safe to cleanup
                        m_TempKeys.Insert(st.ownerDeviceId);
                    }
                    // else: keep waiting — entity may stream back in
                }

                continue; // Skip per-wire checks — entity is unavailable
            }

            int w;
            for (w = 0; w < st.wires.Count(); w = w + 1)
            {
                // v0.7.9: use pre-computed key if available
                string wireKey;
                if (st.cachedWireKeys && w < st.cachedWireKeys.Count())
                {
                    wireKey = st.cachedWireKeys[w];
                }
                else
                {
                    wireKey = st.ownerDeviceId + "|" + w.ToString();
                }

                ref LFPG_WireSegmentInfo info;
                if (!m_WireSegments.Find(wireKey, info) || !info)
                    continue; // Not built yet (pending retry)

                // v0.7.38 (L9): Bounding sphere culling is sufficient for visibility.
                // Endpoints are inside the sphere by definition, so separate
                // endpoint distance checks are redundant. distASq/distBSq are
                // still needed for the bubble check below.
                float distToCenterSq = LFPG_WorldUtil.DistSq(pp, info.cachedCenter);
                float cullPlusRadius = LFPG_CULL_DISTANCE_M + info.cachedRadius;
                float cullPlusRadiusSq = cullPlusRadius * cullPlusRadius;

                float distASq = LFPG_WorldUtil.DistSq(pp, info.cachedPosA);
                float distBSq = LFPG_WorldUtil.DistSq(pp, info.cachedPosB);

                bool shouldBeVisible = (distToCenterSq <= cullPlusRadiusSq);

                // v0.7.7: Device bubble check (tighter radius).
                // If bubble > 0 and player is beyond bubble from BOTH endpoints,
                // hide the wire even if it's within cull distance.
                // v0.7.11 (A3): Bubble check in squared domain.
                if (shouldBeVisible && bubbleSq > 0.0)
                {
                    if (distASq > bubbleSq && distBSq > bubbleSq)
                    {
                        shouldBeVisible = false;
                    }
                }

                // v0.7.38 (M3): cachedMinDist from bounding sphere.
                // Uses max(distToCenter - radius, 0) as closest-point estimate.
                // More accurate than avg(distA, distB) for LOD, alpha fade, and
                // painter's sort — especially for wires with waypoints where
                // the cable midpoint may be much closer than either endpoint.
                float distToCenter = Math.Sqrt(distToCenterSq);
                float closestEst = distToCenter - info.cachedRadius;
                if (closestEst < 0.0)
                {
                    closestEst = 0.0;
                }
                info.cachedMinDist = closestEst;

                // v0.7.38 (C3): Per-wire log gated by DIAG + doCullLog.
                if (doCullLog)
                {
                    string vl = "[CableRenderer] Cull " + wireKey;
                    vl = vl + " vis=" + shouldBeVisible.ToString();
                    vl = vl + " segs=" + info.segments.Count().ToString();
                    vl = vl + " minD=" + info.cachedMinDist.ToString();
                    vl = vl + " dCenterSq=" + distToCenterSq.ToString();
                    vl = vl + " radius=" + info.cachedRadius.ToString();
                    LFPG_Diag.ServerEcho(vl);
                }

                // Update visibility (SetVisible is a no-op if state unchanged)
                info.SetVisible(shouldBeVisible);

                // Update powered flag
                info.powered = st.lastPowered;

                // v0.7.38 (C4): Overload/warning bitmask check.
                // Enforce int is 32-bit signed: 1 << 31 is undefined behavior.
                // Safe range: bits 0-30 (indices 0-30) = 31 wires per owner.
                // In practice, LFPG_MAX_EDGES_PER_NODE = 12 so wireIndex <= 11,
                // well within the safe range. Guard at 30 is defensive.
                // Full 64-wire support would need dual-mask server-side change.
                bool wireOverloaded = false;
                bool wireWarning = false;
                if (info.wireIndex >= 0 && info.wireIndex <= 30)
                {
                    int wireBit = 1 << info.wireIndex;
                    wireOverloaded = ((st.lastOverloadMask & wireBit) != 0);
                    wireWarning = ((st.lastWarningMask & wireBit) != 0);
                }

                if (wireOverloaded)
                {
                    info.cableState = LFPG_CableState.CRITICAL_LOAD;
                }
                else if (wireWarning)
                {
                    info.cableState = LFPG_CableState.WARNING_LOAD;
                }
                else if (st.lastPowered)
                {
                    info.cableState = LFPG_CableState.POWERED;
                }
                else
                {
                    info.cableState = LFPG_CableState.IDLE;
                }
            }
        }

        // v0.7.38 (L4): Deferred cleanup of owners whose entity has been null for 30+s.
        // Cannot modify m_ByOwnerId during iteration, so keys collected in m_TempKeys.
        // Copy to m_GhostKeys because DestroyOwnerLines() also uses m_TempKeys.
        if (m_TempKeys.Count() > 0)
        {
            m_GhostKeys.Clear();
            int gc;
            for (gc = 0; gc < m_TempKeys.Count(); gc = gc + 1)
            {
                m_GhostKeys.Insert(m_TempKeys[gc]);
            }

            int gk;
            for (gk = 0; gk < m_GhostKeys.Count(); gk = gk + 1)
            {
                string ghostId = m_GhostKeys[gk];
                string ghostMsg = "[CableRenderer] CullTick: removing ghost owner=" + ghostId + " (entity null for 30+s)";
                LFPG_Util.Info(ghostMsg);
                DestroyOwnerLines(ghostId);
                ClearOwnerRetries(ghostId);
                m_ByOwnerId.Remove(ghostId);
            }
        }
    }

    // ===========================
    // ARGB color helpers (v0.7.7)
    // ===========================

    // Apply alpha multiplier to an ARGB color (0.0 = transparent, 1.0 = original)
    protected int ApplyAlpha(int argbColor, float alphaFactor)
    {
        // Extract original alpha (bits 24-31)
        int origAlpha = (argbColor >> 24) & 0xFF;
        int newAlpha = (int)(origAlpha * alphaFactor);

        // Clamp 0-255
        if (newAlpha < 0)
        {
            newAlpha = 0;
        }
        if (newAlpha > 255)
        {
            newAlpha = 255;
        }

        // Replace alpha channel, keep RGB
        int rgb = argbColor & 0x00FFFFFF;
        return (newAlpha << 24) | rgb;
    }

    // Create a highlight color by lightening the RGB and applying alpha
    protected int MakeHighlightColor(int baseColor, int highlightAlpha)
    {
        // Extract RGB components
        int r = (baseColor >> 16) & 0xFF;
        int g = (baseColor >> 8) & 0xFF;
        int b = baseColor & 0xFF;

        // Lighten toward white (add ~40% toward 255)
        r = r + ((255 - r) * 40 / 100);
        g = g + ((255 - g) * 40 / 100);
        b = b + ((255 - b) * 40 / 100);

        return (highlightAlpha << 24) | (r << 16) | (g << 8) | b;
    }

    // v0.7.8: Get ARGB color for a cable state.
    // Maps the LFPG_CableState enum to the palette in LFPG_Defines.
    protected int GetStateColor(int state)
    {
        if (state == LFPG_CableState.POWERED)
            return LFPG_STATE_COLOR_POWERED;

        if (state == LFPG_CableState.RESOLVING)
            return LFPG_STATE_COLOR_RESOLVING;

        if (state == LFPG_CableState.DISCONNECTED)
            return LFPG_STATE_COLOR_DISCONNECTED;

        if (state == LFPG_CableState.WARNING_LOAD)
            return LFPG_STATE_COLOR_WARNING;

        if (state == LFPG_CableState.CRITICAL_LOAD)
            return LFPG_STATE_COLOR_CRITICAL;

        if (state == LFPG_CableState.ERROR_SHORT)
            return LFPG_STATE_COLOR_ERROR_SHORT;

        if (state == LFPG_CableState.ERROR_TOPOLOGY)
            return LFPG_STATE_COLOR_ERROR_TOPO;

        if (state == LFPG_CableState.BLOCKED_LOGIC)
            return LFPG_STATE_COLOR_BLOCKED;

        if (state == LFPG_CableState.SELECTED)
            return LFPG_STATE_COLOR_SELECTED;

        // Default: IDLE
        return LFPG_STATE_COLOR_IDLE;
    }

    // v0.7.38 (H1): Cohen-Sutherland moved to LFPG_WorldUtil.ClipSegToScreen
    // and LFPG_WorldUtil.ComputeOutcode (shared with WiringClient).

    // ===========================
    // DrawFrame - per-frame Canvas 2D rendering
    // ===========================
    // Called every frame from MissionGameplay.OnUpdate.
    // Draws visible cables via CableHUD with wire-level
    // raycast occlusion (budgeted, staggered, hierarchical).
    //
    // v0.7.9 improvements:
    //   - Screen-space caching: GetScreenPos called once per unique point
    //   - Wind sway: subtle oscillation on intermediate sag points
    //   - Depth-scaled joints/endcaps
    //   - Behind-camera and off-screen checks computed once per segment
    void DrawFrame()
    {
        LFPG_CableHUD hud = LFPG_CableHUD.Get();
        if (!hud || !hud.IsReady())
            return;

        if (m_WireSegments.Count() == 0)
            return;

        // v0.7.13 (G5): Render telemetry — grab reference once per frame
        LFPG_RenderMetrics tRnd = LFPG_Telemetry.GetRender();

        vector camPos = GetGame().GetCurrentCameraPosition();
        vector camDir = GetGame().GetCurrentCameraDirection();
        float nowMs = GetGame().GetTime();

        // ---- Camera movement detection ----
        vector camDelta = camPos - m_LastCamPos;
        vector dirDelta = camDir - m_LastCamDir;
        float posDist = camDelta.Length();
        float dirDist = dirDelta.Length();

        m_CamMoved = false;
        if (posDist > LFPG_OCC_CAM_MOVE_THRESH)
        {
            m_CamMoved = true;
        }
        if (dirDist > LFPG_OCC_CAM_DIR_THRESH)
        {
            m_CamMoved = true;
        }
        m_LastCamPos = camPos;
        m_LastCamDir = camDir;

        int rayBudget = LFPG_OCC_MAX_RAYCASTS;
        // v0.7.26 (Audit 4): Adaptive raycast budget.
        // In bases with 50+ wires, raycasts can dominate frame time.
        // Scale down budget when wire count is high:
        //   <25 wires: full budget (20 raycasts)
        //   25-50 wires: half budget (10 raycasts)
        //   >50 wires: quarter budget (5 raycasts)
        // Stagger + forced recheck ensure all wires still get checked over time.
        int visibleWires = m_WireSegments.Count();
        if (visibleWires > 50)
        {
            rayBudget = LFPG_OCC_MAX_RAYCASTS / 4;
            if (rayBudget < 3)
            {
                rayBudget = 3;
            }
        }
        else if (visibleWires > 25)
        {
            rayBudget = LFPG_OCC_MAX_RAYCASTS / 2;
            if (rayBudget < 5)
            {
                rayBudget = 5;
            }
        }
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());

        // v0.7.23 (Bug 9): State colors only show when holding tools.
        // Cache this once per frame to avoid per-wire overhead.
        bool showStateColors = false;
        if (player)
        {
            if (LFPG_WorldUtil.PlayerHasCableReelInHands(player))
            {
                showStateColors = true;
            }
            else if (LFPG_WorldUtil.PlayerHasPliersInHands(player))
            {
                showStateColors = true;
            }
        }

        // v0.7.9: wrap to prevent unbounded growth in long sessions
        m_OccStaggerIdx = (m_OccStaggerIdx + 1) % 3;
        int wireCount = m_WireSegments.Count();

        float fadeRange = LFPG_CULL_DISTANCE_M - LFPG_ALPHA_FADE_START_M;
        bool doAlphaFade = (fadeRange > 0.1);

        // v0.7.9: reuse CableHUD's cached screen dimensions (set in BeginFrame)
        // instead of calling GetScreenSize again per frame.
        float swF = hud.GetScreenW();
        float shF = hud.GetScreenH();

        // v0.7.38 (H3): Proportional ultra-LOD margin.
        // Fixed 200px was too large at 720p and too small at 4K.
        float ulMargin = shF * LFPG_ULTRA_LOD_MARGIN_RATIO;

        // ---- v0.7.38: Player screen-space occlusion ----
        // In 3rd-person view, compute a screen-space bounding rect around
        // the player model. Segments farther from camera than the player
        // that intersect this rect get alpha-faded so cables don't overdraw
        // the character. Cost: 2 GetScreenPos + ~10 float ops per frame.
        bool plOccActive = false;
        float plRectX1 = 0.0;
        float plRectY1 = 0.0;
        float plRectX2 = 0.0;
        float plRectY2 = 0.0;
        float plDepthThreshold = 0.0;
        if (player)
        {
            vector plPos = player.GetPosition();
            float plCamDist = LFPG_WorldUtil.DistSq(camPos, plPos);
            // Only activate in 3rd person: camera must be > 0.5m from player.
            // In 1P the model is hidden so occlusion is unnecessary.
            if (plCamDist > LFPG_PLOCC_3P_MIN_DIST_SQ)
            {
                vector plHead = plPos;
                plHead[1] = plHead[1] + LFPG_PLOCC_HEAD_OFFSET_Y;

                vector plFeetScr = GetGame().GetScreenPos(plPos);
                vector plHeadScr = GetGame().GetScreenPos(plHead);

                // Both points must be in front of camera
                if (plFeetScr[2] > LFPG_BEHIND_CAM_Z && plHeadScr[2] > LFPG_BEHIND_CAM_Z)
                {
                    // Screen-space height of player model
                    float plHPx = plFeetScr[1] - plHeadScr[1];
                    if (plHPx < 0.0)
                    {
                        plHPx = -plHPx;
                    }

                    if (plHPx > 10.0)
                    {
                        // Width from height using body aspect ratio + padding
                        float plWPx = plHPx * LFPG_PLOCC_WIDTH_RATIO;
                        float plCenterX = (plFeetScr[0] + plHeadScr[0]) * 0.5;
                        float plHalfW = plWPx * 0.5;
                        float plPad = plHPx * LFPG_PLOCC_PAD_RATIO;

                        // Build rect (screen Y: 0=top, head < feet)
                        float plTopY = plHeadScr[1];
                        float plBotY = plFeetScr[1];
                        if (plTopY > plBotY)
                        {
                            plTopY = plFeetScr[1];
                            plBotY = plHeadScr[1];
                        }

                        plRectX1 = plCenterX - plHalfW;
                        plRectY1 = plTopY - plPad;
                        plRectX2 = plCenterX + plHalfW;
                        plRectY2 = plBotY + plPad;

                        // Depth threshold: avg player Z + margin.
                        // Segments beyond this are candidates for player occlusion.
                        plDepthThreshold = (plFeetScr[2] + plHeadScr[2]) * 0.5 + LFPG_PLOCC_DEPTH_MARGIN;
                        plOccActive = true;
                    }
                }
            }
        }

        // v0.7.38 (C1): Painter's algorithm — sort wires far-to-near
        // so nearer cables draw ON TOP of farther ones.
        // Phase A: Fill arrays without sorting (O(n)).
        // Phase B: Selection sort in-place (O(n²) compares, O(n) swaps).
        // Replaces old InsertAt approach which was O(n³) due to array shifts.
        m_DrawOrder.Clear();
        m_DrawDist.Clear();
        int si;
        for (si = 0; si < wireCount; si = si + 1)
        {
            ref LFPG_WireSegmentInfo sortWsi = m_WireSegments.GetElement(si);
            if (!sortWsi)
                continue;

            m_DrawOrder.Insert(si);
            m_DrawDist.Insert(sortWsi.cachedMinDist);
        }

        // Selection sort descending (farthest first).
        // Swaps only — no array shifts, no InsertAt.
        int sortCount = m_DrawOrder.Count();
        int si2;
        for (si2 = 0; si2 < sortCount - 1; si2 = si2 + 1)
        {
            int maxIdx = si2;
            float maxDist = m_DrawDist[si2];
            int sj;
            for (sj = si2 + 1; sj < sortCount; sj = sj + 1)
            {
                if (m_DrawDist[sj] > maxDist)
                {
                    maxIdx = sj;
                    maxDist = m_DrawDist[sj];
                }
            }
            if (maxIdx != si2)
            {
                // Swap m_DrawOrder
                int tmpIdx = m_DrawOrder[si2];
                m_DrawOrder[si2] = m_DrawOrder[maxIdx];
                m_DrawOrder[maxIdx] = tmpIdx;
                // Swap m_DrawDist
                float tmpDist = m_DrawDist[si2];
                m_DrawDist[si2] = m_DrawDist[maxIdx];
                m_DrawDist[maxIdx] = tmpDist;
            }
        }

        int di;
        for (di = 0; di < m_DrawOrder.Count(); di = di + 1)
        {
            int i = m_DrawOrder[di];
            ref LFPG_WireSegmentInfo wsi = m_WireSegments.GetElement(i);
            if (!wsi)
                continue;

            // G5: count every wire known to renderer
            tRnd.m_WiresTotal = tRnd.m_WiresTotal + 1;

            if (!wsi.visible)
            {
                tRnd.m_WiresCulled = tRnd.m_WiresCulled + 1;
                continue;
            }

            // v0.7.35 (F2.1): Behind-camera early-out.
            // Dot product of (wireCenter - camPos) with camDir.
            // If the entire bounding sphere is behind the camera, skip
            // projection + drawing entirely. Cost: 3 mul + 3 add + 1 cmp.
            // Saves all GetScreenPos + drawing for wires behind the player.
            // Component-wise to avoid vector allocation on heap per wire.
            {
                float dcx = wsi.cachedCenter[0] - camPos[0];
                float dcy = wsi.cachedCenter[1] - camPos[1];
                float dcz = wsi.cachedCenter[2] - camPos[2];
                float dot = dcx * camDir[0] + dcy * camDir[1] + dcz * camDir[2];
                // dot < 0 means center is behind camera.
                // Add radius to account for sphere extent toward camera.
                if (dot + wsi.cachedRadius < 0.0)
                {
                    tRnd.m_WiresCulled = tRnd.m_WiresCulled + 1;
                    continue;
                }
            }

            // ---- Occlusion recheck ----
            // v0.7.9: When camera moves, recheck at normal interval (200ms staggered).
            // When camera is static, add forced delay to catch moving objects
            // (doors, vehicles) that may occlude/reveal cables.

            bool doOccCheck = false;
            if (rayBudget > 0)
            {
                float occDeadline = wsi.occNextCheckMs;
                if (!m_CamMoved)
                {
                    occDeadline = occDeadline + LFPG_OCC_FORCED_RECHECK_MS;
                }

                if (nowMs >= occDeadline)
                {
                    // v0.7.38 (H6): Use stable stagger group (set at BuildWire)
                    // instead of map index which shifts on add/remove.
                    if (wsi.occStaggerGroup == (m_OccStaggerIdx % 3))
                    {
                        doOccCheck = true;

                        // v0.7.38 (Synergy S2): Skip raycast for wires behind player.
                        // Only checked for wires that WOULD consume budget (passed
                        // deadline + stagger). Avoids wasting GetScreenPos on all
                        // visible wires every frame.
                        // Cost: 1 GetScreenPos per eligible wire (2-10/frame).
                        // Save: 1-3 raycasts freed for wires the player can see.
                        if (plOccActive)
                        {
                            vector scrPlCenter = GetGame().GetScreenPos(wsi.cachedCenter);
                            if (scrPlCenter[2] > 0.0 && scrPlCenter[2] > plDepthThreshold)
                            {
                                if (scrPlCenter[0] > plRectX1 && scrPlCenter[0] < plRectX2 && scrPlCenter[1] > plRectY1 && scrPlCenter[1] < plRectY2)
                                {
                                    doOccCheck = false;
                                }
                            }
                        }
                    }
                }
            }

            if (doOccCheck)
            {
                // v0.7.9: Strict budget check — only perform occlusion if we have
                // enough budget for ALL samples of this wire. Prevents overshooting.
                int samplesNeeded = wsi.occSamples.Count();
                if (samplesNeeded <= rayBudget)
                {
                    bool allBlocked = CheckWireOcclusion(camPos, wsi, player);
                    wsi.UpdateOcclusion(allBlocked, nowMs, wsi.cachedMinDist);
                    rayBudget = rayBudget - samplesNeeded;
                }
            }

            if (wsi.occluded)
            {
                tRnd.m_WiresOccluded = tRnd.m_WiresOccluded + 1;
                continue;
            }

            int segCount = wsi.segments.Count();
            if (segCount == 0)
                continue;

            // ---- LOD tier ----
            float wireDist = wsi.cachedMinDist;
            int lodTier = 2;
            if (wireDist < LFPG_LOD_CLOSE_M)
            {
                lodTier = 0;
            }
            else if (wireDist < LFPG_LOD_MEDIUM_M)
            {
                lodTier = 1;
            }

            // v0.7.38: LOD transition blend for anti-popping.
            // In the last TRANSITION_M metres before tier 0→1 boundary,
            // fade highlight and endcaps/joints to 0 so the switch is seamless.
            // Only computed for tier 0 (close-up detail features).
            // lodTier==0 guarantees wireDist < LOD_CLOSE_M, so lodBlend > 0 always.
            float lodBlend = 1.0;
            if (lodTier == 0 && wireDist > LFPG_LOD_FADE_START_M)
            {
                lodBlend = (LFPG_LOD_CLOSE_M - wireDist) * LFPG_LOD_TRANSITION_INV;
            }

            // ---- Alpha fade ----
            float alphaFactor = 1.0;
            if (doAlphaFade && wireDist > LFPG_ALPHA_FADE_START_M)
            {
                alphaFactor = 1.0 - ((wireDist - LFPG_ALPHA_FADE_START_M) / fadeRange);
                if (alphaFactor < 0.0)
                {
                    alphaFactor = 0.0;
                }
                if (alphaFactor > 1.0)
                {
                    alphaFactor = 1.0;
                }
            }

            if (alphaFactor < LFPG_ALPHA_MIN_THRESHOLD)
            {
                tRnd.m_WiresCulled = tRnd.m_WiresCulled + 1;
                continue;
            }

            // v0.7.35 (F2.3): Partial occlusion alpha.
            // When some (but not all) occlusion samples are blocked, reduce
            // alpha proportionally. This gives a visual "fade through walls"
            // effect instead of the abrupt all-or-nothing occlusion.
            // Only applies when wire has 2+ samples (medium/long wires).
            // Short wires with 1 sample remain all-or-nothing (acceptable).
            if (wsi.occBlockedRatio > 0.01 && !wsi.occluded)
            {
                float occAlpha = 1.0 - (wsi.occBlockedRatio * 0.7);
                if (occAlpha < LFPG_OCC_ALPHA_MIN)
                {
                    occAlpha = LFPG_OCC_ALPHA_MIN;
                }
                alphaFactor = alphaFactor * occAlpha;
            }

            // ---- Colors ----
            int baseColor = GetStateColor(wsi.cableState);
            // v0.7.23 (Bug 9): Without tools, all cables show neutral IDLE color.
            if (!showStateColors)
            {
                baseColor = LFPG_STATE_COLOR_IDLE;
            }
            int drawColor = baseColor;
            if (alphaFactor < 0.99)
            {
                drawColor = ApplyAlpha(baseColor, alphaFactor);
            }

            int shadowColor = 0;
            int highlightColor = 0;

            if (lodTier <= 1)
            {
                shadowColor = LFPG_SHADOW_COLOR;
                if (alphaFactor < 0.99)
                {
                    shadowColor = ApplyAlpha(LFPG_SHADOW_COLOR, alphaFactor);
                }
            }

            if (lodTier == 0)
            {
                highlightColor = MakeHighlightColor(baseColor, LFPG_HIGHLIGHT_ALPHA);
                // v0.7.38: Combine alphaFactor and lodBlend into single ApplyAlpha.
                float hlAlpha = alphaFactor * lodBlend;
                if (hlAlpha < 0.99)
                {
                    highlightColor = ApplyAlpha(highlightColor, hlAlpha);
                }
            }

            // ---- v0.7.38: Player-occluded color variants ----
            // Pre-compute once per wire so per-segment test is a simple select.
            // Derive from drawColor/shadowColor/highlightColor which already
            // incorporate alphaFactor and showStateColors.
            //
            // v0.7.38 (Synergy S1): Skip player clipping when wire is already
            // faded by world occlusion. At alphaFactor<0.5, plOcc segments
            // would be at <4% alpha (invisible). Saves ClipSegToScreen +
            // up to 6 extra DrawLineScreen per segment.
            bool wirePlOcc = false;
            if (plOccActive && alphaFactor >= 0.5)
            {
                wirePlOcc = true;
            }
            int plOccDrawColor = 0;
            int plOccShadowColor = 0;
            int plOccHighlightColor = 0;
            if (wirePlOcc)
            {
                plOccDrawColor = ApplyAlpha(drawColor, LFPG_PLOCC_ALPHA);
                if (lodTier <= 1)
                {
                    plOccShadowColor = ApplyAlpha(shadowColor, LFPG_PLOCC_ALPHA);
                }
                if (lodTier == 0)
                {
                    plOccHighlightColor = ApplyAlpha(highlightColor, LFPG_PLOCC_ALPHA);
                }
            }

            // ================================================
            // v0.7.35 (F2.2): Ultra-LOD for distant wires (lodTier 2).
            // Skip catenary subdivision, sway, endcaps, joints.
            // Project only the two port endpoints and draw a single
            // straight line. Saves (segCount-1) GetScreenPos calls
            // and all multi-pass drawing overhead for far cables.
            // ================================================
            if (lodTier == 2)
            {
                vector ulA = GetGame().GetScreenPos(wsi.cachedPosA);
                vector ulB = GetGame().GetScreenPos(wsi.cachedPosB);

                // At >40m any endpoint behind camera means the visible
                // portion is negligible. Skip entirely (both OR either).
                if (ulA[2] < LFPG_BEHIND_CAM_Z || ulB[2] < LFPG_BEHIND_CAM_Z)
                {
                    tRnd.m_WiresCulled = tRnd.m_WiresCulled + 1;
                    continue;
                }

                float ulx1 = ulA[0];
                float uly1 = ulA[1];
                float ulx2 = ulB[0];
                float uly2 = ulB[1];

                // Screen clipping (fast-path + Cohen-Sutherland)
                bool ulOutA = (ulx1 < -ulMargin || ulx1 > swF + ulMargin || uly1 < -ulMargin || uly1 > shF + ulMargin);
                bool ulOutB = (ulx2 < -ulMargin || ulx2 > swF + ulMargin || uly2 < -ulMargin || uly2 > shF + ulMargin);
                if (ulOutA || ulOutB)
                {
                    float ulMinX = -ulMargin;
                    float ulMinY = -ulMargin;
                    float ulMaxX = swF + ulMargin;
                    float ulMaxY = shF + ulMargin;
                    bool ulVis = LFPG_WorldUtil.ClipSegToScreen(ulx1, uly1, ulx2, uly2, ulMinX, ulMinY, ulMaxX, ulMaxY, m_ClipA, m_ClipB);
                    if (!ulVis)
                    {
                        tRnd.m_WiresCulled = tRnd.m_WiresCulled + 1;
                        continue;
                    }
                    ulx1 = m_ClipA[0];
                    uly1 = m_ClipA[1];
                    ulx2 = m_ClipB[0];
                    uly2 = m_ClipB[1];
                }

                // Draw single line with minimum width.
                // v0.7.38: Negative clipping — split into opaque→faded→opaque
                // at the exact edges of the player rect.
                if (wirePlOcc)
                {
                    float ulAvgZ = (ulA[2] + ulB[2]) * 0.5;
                    if (ulAvgZ > plDepthThreshold)
                    {
                        bool ulHitsPlayer = LFPG_WorldUtil.ClipSegToScreen(ulx1, uly1, ulx2, uly2, plRectX1, plRectY1, plRectX2, plRectY2, m_ClipA, m_ClipB);
                        if (ulHitsPlayer)
                        {
                            float ulPcX1 = m_ClipA[0];
                            float ulPcY1 = m_ClipA[1];
                            float ulPcX2 = m_ClipB[0];
                            float ulPcY2 = m_ClipB[1];

                            // Tramo 1: A → Pin (opaque, skip if degenerate)
                            float uld1 = (ulPcX1 - ulx1) * (ulPcX1 - ulx1) + (ulPcY1 - uly1) * (ulPcY1 - uly1);
                            if (uld1 > 1.0)
                            {
                                hud.DrawLineScreen(ulx1, uly1, ulPcX1, ulPcY1, LFPG_DEPTH_WIDTH_MIN, drawColor);
                            }
                            // Tramo 2: Pin → Pout (faded behind player)
                            hud.DrawLineScreen(ulPcX1, ulPcY1, ulPcX2, ulPcY2, LFPG_DEPTH_WIDTH_MIN, plOccDrawColor);
                            // Tramo 3: Pout → B (opaque, skip if degenerate)
                            float uld3 = (ulx2 - ulPcX2) * (ulx2 - ulPcX2) + (uly2 - ulPcY2) * (uly2 - ulPcY2);
                            if (uld3 > 1.0)
                            {
                                hud.DrawLineScreen(ulPcX2, ulPcY2, ulx2, uly2, LFPG_DEPTH_WIDTH_MIN, drawColor);
                            }

                            tRnd.m_WiresDrawn = tRnd.m_WiresDrawn + 1;
                            tRnd.m_SegmentsDrawn = tRnd.m_SegmentsDrawn + 1;
                            continue;
                        }
                    }
                }
                hud.DrawLineScreen(ulx1, uly1, ulx2, uly2, LFPG_DEPTH_WIDTH_MIN, drawColor);

                tRnd.m_WiresDrawn = tRnd.m_WiresDrawn + 1;
                tRnd.m_SegmentsDrawn = tRnd.m_SegmentsDrawn + 1;
                continue;  // Skip Phase 1/2/3 entirely
            }

            // ================================================
            // Phase 1: Project all unique points ONCE per wire.
            // v0.7.9: Eliminates redundant GetScreenPos across
            // multi-pass drawing (was 6x per seg, now 1x per point).
            //
            // Layout: m_ScreenPts[s] = start of segment s,
            //         m_ScreenPts[s+1] = end of segment s.
            // Total entries = segCount + 1 (one per unique vertex).
            // ================================================
            m_ScreenPts.Clear();

            // Wind sway: unique phase per wire from position hash.
            // v0.7.38: Distance attenuation — at 50m sway is <1px, skip.
            // v0.7.38: Horizontal sway — second Sin with different speed/phase
            // gives organic 2D Lissajous-like motion. Both share swayScale.
            float swayOff = 0.0;
            float swayOffX = 0.0;
            float swayScale = 1.0 - (wireDist * LFPG_SWAY_ATTEN_INV);
            if (swayScale > 0.0)
            {
                float swayPhase = wsi.cachedPosA[0] * LFPG_SWAY_HASH_X + wsi.cachedPosA[2] * LFPG_SWAY_HASH_Z;
                swayOff = Math.Sin(nowMs * LFPG_SWAY_SPEED + swayPhase) * LFPG_SWAY_AMPLITUDE * swayScale;
                swayOffX = Math.Sin(nowMs * LFPG_SWAY_X_SPEED + swayPhase + LFPG_SWAY_X_PHASE_OFS) * LFPG_SWAY_X_AMPLITUDE * swayScale;
            }
            // v0.7.38: Precompute parabolic denominator for sway profile.
            // swayT = (s+1)/(segCount+1), swayW = 4*swayT*(1-swayT).
            // Points at cable center sway most, endpoints don't sway.
            float swayDenom = segCount + 1.0;

            // Guard: verify first segment exists (should always be true)
            LFPG_CableParticle firstSeg = wsi.segments[0];
            if (!firstSeg)
                continue;

            // Project first point (port endpoint: no sway)
            m_ScreenPts.Insert(GetGame().GetScreenPos(firstSeg.m_From));

            int s;
            for (s = 0; s < segCount; s = s + 1)
            {
                LFPG_CableParticle seg = wsi.segments[s];
                if (!seg)
                {
                    // Null segment: project endpoint as fallback to maintain indexing.
                    // Phase 2 detects degenerate segments via behind-camera z check.
                    m_ScreenPts.Insert(m_ScreenPts[m_ScreenPts.Count() - 1]);
                    continue;
                }

                vector wp = seg.m_To;

                // Apply sway to intermediate points only.
                // Port endpoints (first .m_From and last .m_To) do NOT sway —
                // they are anchored to the device. Only sag interpolation points move.
                // v0.7.38: Parabolic profile — cable center sways most,
                // near-anchor points barely move (like a real hanging cable).
                // Both vertical (Y) and horizontal (X) share the same profile.
                // Skip if both sway components are negligible.
                bool isLastPoint = (s == segCount - 1);
                if (!isLastPoint && swayScale > 0.0)
                {
                    float swayT = (s + 1.0) / swayDenom;
                    float swayW = 4.0 * swayT * (1.0 - swayT);
                    wp[1] = wp[1] + swayOff * swayW;
                    wp[0] = wp[0] + swayOffX * swayW;
                }

                m_ScreenPts.Insert(GetGame().GetScreenPos(wp));
            }

            // ================================================
            // Phase 2: Draw sub-segments using cached projections.
            // Behind-camera and off-screen resolved once per seg.
            // ================================================
            for (s = 0; s < segCount; s = s + 1)
            {
                vector sA = m_ScreenPts[s];
                vector sB = m_ScreenPts[s + 1];

                // Behind camera check
                bool behindA = (sA[2] < LFPG_BEHIND_CAM_Z);
                bool behindB = (sB[2] < LFPG_BEHIND_CAM_Z);

                // v0.7.38: Skip segments with any endpoint behind camera.
                // Previous approach (ClipBehindCamera) clipped to near plane
                // and re-projected, producing extreme screen coords that
                // Cohen-Sutherland then clipped to screen edges — creating
                // diagonal artifact lines during camera rotation.
                // Catenary has multiple sub-segments, losing one at the
                // camera transition is imperceptible and artifact-free.
                if (behindA || behindB)
                    continue;

                // Resolve screen coords
                float sx1 = sA[0];
                float sy1 = sA[1];
                float sx2 = sB[0];
                float sy2 = sB[1];

                // v0.7.35 (F1.2): Screen clipping with fast-path.
                // Most segments are fully on-screen → cheap inline check.
                // Only segments with an endpoint outside invoke Cohen-Sutherland
                // to correctly handle segments that SPAN the viewport.
                float margin = shF * LFPG_SCREEN_MARGIN_RATIO;
                if (margin < LFPG_SCREEN_MARGIN_MIN_PX)
                {
                    margin = LFPG_SCREEN_MARGIN_MIN_PX;
                }

                // Fast-path: check if either endpoint is outside screen bounds.
                // If both are inside, skip clipping entirely (zero function calls).
                bool outsideA = (sx1 < -margin || sx1 > swF + margin || sy1 < -margin || sy1 > shF + margin);
                bool outsideB = (sx2 < -margin || sx2 > swF + margin || sy2 < -margin || sy2 > shF + margin);

                if (outsideA || outsideB)
                {
                    // One or both endpoints outside: use Cohen-Sutherland.
                    // This correctly handles segments that cross the viewport
                    // (old code incorrectly culled these with offA && offB).
                    float csMinX = -margin;
                    float csMinY = -margin;
                    float csMaxX = swF + margin;
                    float csMaxY = shF + margin;
                    bool segVisible = LFPG_WorldUtil.ClipSegToScreen(sx1, sy1, sx2, sy2, csMinX, csMinY, csMaxX, csMaxY, m_ClipA, m_ClipB);
                    if (!segVisible)
                        continue;

                    sx1 = m_ClipA[0];
                    sy1 = m_ClipA[1];
                    sx2 = m_ClipB[0];
                    sy2 = m_ClipB[1];
                }

                // Depth-based width (use z of visible point for behind-camera cases)
                float zA = sA[2];
                float zB = sB[2];
                if (behindA)
                {
                    zA = zB;
                }
                if (behindB)
                {
                    zB = zA;
                }
                float avgZ = (zA + zB) * 0.5;
                float depthWidth = LFPG_CABLE_WIDTH;

                if (avgZ > LFPG_BEHIND_CAM_Z)
                {
                    depthWidth = LFPG_CABLE_WIDTH * (LFPG_DEPTH_WIDTH_REF / avgZ);
                    if (depthWidth < LFPG_DEPTH_WIDTH_MIN)
                    {
                        depthWidth = LFPG_DEPTH_WIDTH_MIN;
                    }
                    if (depthWidth > LFPG_DEPTH_WIDTH_MAX)
                    {
                        depthWidth = LFPG_DEPTH_WIDTH_MAX;
                    }
                }

                // ---- v0.7.38: Per-segment player negative clipping ----
                // ClipSegToScreen returns the portion INSIDE the player rect.
                // We split drawing into 3 tramos: opaque → faded → opaque,
                // giving pixel-perfect occlusion at the rect edges.
                bool segPlOcc = false;
                float plClX1 = 0.0;
                float plClY1 = 0.0;
                float plClX2 = 0.0;
                float plClY2 = 0.0;
                if (wirePlOcc)
                {
                    // avgZ is valid here: behindA/behindB segments already
                    // skipped via continue, so zA==sA[2] and zB==sB[2].
                    if (avgZ > plDepthThreshold)
                    {
                        bool plHits = LFPG_WorldUtil.ClipSegToScreen(sx1, sy1, sx2, sy2, plRectX1, plRectY1, plRectX2, plRectY2, m_ClipA, m_ClipB);
                        if (plHits)
                        {
                            segPlOcc = true;
                            plClX1 = m_ClipA[0];
                            plClY1 = m_ClipA[1];
                            plClX2 = m_ClipB[0];
                            plClY2 = m_ClipB[1];
                        }
                    }
                }

                // v0.7.38: Depth-proportional shadow offset (down-right).
                // Scales with depthWidth so shadow hugs cable at all distances.
                float hlWidth = depthWidth - LFPG_HIGHLIGHT_WIDTH_SUB;
                if (hlWidth < 1.0)
                {
                    hlWidth = 1.0;
                }
                float shOfsX = depthWidth * LFPG_SHADOW_OFS_X_RATIO;
                float shOfsY = depthWidth * LFPG_SHADOW_OFS_Y_RATIO;

                // ---- Multi-pass drawing (LOD-dependent) ----
                if (segPlOcc)
                {
                    // Negative clipping: opaque → faded → opaque
                    // Squared pixel distance to detect degenerate tramos (< 1px)
                    float d1sq = (plClX1 - sx1) * (plClX1 - sx1) + (plClY1 - sy1) * (plClY1 - sy1);
                    float d3sq = (sx2 - plClX2) * (sx2 - plClX2) + (sy2 - plClY2) * (sy2 - plClY2);

                    // Tramo 1: A → Pin (opaque)
                    if (d1sq > 1.0)
                    {
                        if (lodTier <= 1)
                        {
                            hud.DrawLineScreen(sx1 + shOfsX, sy1 + shOfsY, plClX1 + shOfsX, plClY1 + shOfsY, depthWidth, shadowColor);
                        }
                        hud.DrawLineScreen(sx1, sy1, plClX1, plClY1, depthWidth, drawColor);
                        if (lodTier == 0)
                        {
                            hud.DrawLineScreen(sx1, sy1, plClX1, plClY1, hlWidth, highlightColor);
                        }
                    }

                    // Tramo 2: Pin → Pout (faded behind player)
                    if (lodTier <= 1)
                    {
                        hud.DrawLineScreen(plClX1 + shOfsX, plClY1 + shOfsY, plClX2 + shOfsX, plClY2 + shOfsY, depthWidth, plOccShadowColor);
                    }
                    hud.DrawLineScreen(plClX1, plClY1, plClX2, plClY2, depthWidth, plOccDrawColor);
                    if (lodTier == 0)
                    {
                        hud.DrawLineScreen(plClX1, plClY1, plClX2, plClY2, hlWidth, plOccHighlightColor);
                    }

                    // Tramo 3: Pout → B (opaque)
                    if (d3sq > 1.0)
                    {
                        if (lodTier <= 1)
                        {
                            hud.DrawLineScreen(plClX2 + shOfsX, plClY2 + shOfsY, sx2 + shOfsX, sy2 + shOfsY, depthWidth, shadowColor);
                        }
                        hud.DrawLineScreen(plClX2, plClY2, sx2, sy2, depthWidth, drawColor);
                        if (lodTier == 0)
                        {
                            hud.DrawLineScreen(plClX2, plClY2, sx2, sy2, hlWidth, highlightColor);
                        }
                    }
                }
                else
                {
                    // Normal draw (no player intersection)
                    if (lodTier <= 1)
                    {
                        hud.DrawLineScreen(sx1 + shOfsX, sy1 + shOfsY, sx2 + shOfsX, sy2 + shOfsY, depthWidth, shadowColor);
                    }
                    hud.DrawLineScreen(sx1, sy1, sx2, sy2, depthWidth, drawColor);
                    if (lodTier == 0)
                    {
                        hud.DrawLineScreen(sx1, sy1, sx2, sy2, hlWidth, highlightColor);
                    }
                }
            }

            // ================================================
            // Phase 3: Endcaps and joints (LOD close only)
            // v0.7.9: Depth-scaled sizes. Uses cached screen coords.
            // ================================================
            if (lodTier == 0)
            {
                // v0.7.38: Fade decorators with lodBlend for smooth LOD transition.
                int decColor = drawColor;
                if (lodBlend < 0.99)
                {
                    decColor = ApplyAlpha(drawColor, lodBlend);
                }

                // Depth scale factor for decorators
                float decZ = wsi.cachedMinDist;
                if (decZ < 1.0)
                {
                    decZ = 1.0;
                }
                float decScale = LFPG_DEPTH_WIDTH_REF / decZ;

                // v0.7.38 (L6): Compute endcap size once (same for both ends).
                float ecSize = LFPG_ENDCAP_SIZE * decScale;
                if (ecSize < LFPG_ENDCAP_SIZE_MIN)
                {
                    ecSize = LFPG_ENDCAP_SIZE_MIN;
                }
                if (ecSize > LFPG_ENDCAP_SIZE_MAX)
                {
                    ecSize = LFPG_ENDCAP_SIZE_MAX;
                }

                // Endcap at port A (first point in m_ScreenPts)
                if (m_ScreenPts.Count() >= 2)
                {
                    vector ecA = m_ScreenPts[0];
                    if (ecA[2] > LFPG_BEHIND_CAM_Z)
                    {
                        vector ecA2 = m_ScreenPts[1];
                        float edx = ecA2[0] - ecA[0];
                        float edy = ecA2[1] - ecA[1];
                        float elen = Math.Sqrt(edx * edx + edy * edy);
                        float epx = 0.0;
                        float epy = 1.0;
                        if (elen > 0.1)
                        {
                            float einv = 1.0 / elen;
                            epx = -edy * einv;
                            epy = edx * einv;
                        }
                        hud.DrawEndcapScreen(ecA[0], ecA[1], epx, epy, ecSize, LFPG_ENDCAP_WIDTH, decColor);
                    }
                }

                // Endcap at port B (last point in m_ScreenPts)
                int lastPtIdx = m_ScreenPts.Count() - 1;
                if (lastPtIdx >= 1)
                {
                    vector ecB = m_ScreenPts[lastPtIdx];
                    if (ecB[2] > LFPG_BEHIND_CAM_Z)
                    {
                        vector ecB2 = m_ScreenPts[lastPtIdx - 1];
                        float edxB = ecB[0] - ecB2[0];
                        float edyB = ecB[1] - ecB2[1];
                        float elenB = Math.Sqrt(edxB * edxB + edyB * edyB);
                        float epxB = 0.0;
                        float epyB = 1.0;
                        if (elenB > 0.1)
                        {
                            float einvB = 1.0 / elenB;
                            epxB = -edyB * einvB;
                            epyB = edxB * einvB;
                        }
                        hud.DrawEndcapScreen(ecB[0], ecB[1], epxB, epyB, ecSize, LFPG_ENDCAP_WIDTH, decColor);
                    }
                }

                // Joints at waypoints (depth-scaled)
                // v0.7.9: Batch-project all joints once, then draw from cache.
                // Avoids per-joint GetScreenPos calls each frame.
                if (wsi.cachedJoints)
                {
                    int jCount = wsi.cachedJoints.Count();
                    if (jCount > 0)
                    {
                        float jSize = LFPG_JOINT_SIZE * decScale;
                        if (jSize < LFPG_JOINT_SIZE_MIN)
                        {
                            jSize = LFPG_JOINT_SIZE_MIN;
                        }
                        if (jSize > LFPG_JOINT_SIZE_MAX)
                        {
                            jSize = LFPG_JOINT_SIZE_MAX;
                        }

                        m_JointScreenPts.Clear();
                        int jp;
                        for (jp = 0; jp < jCount; jp = jp + 1)
                        {
                            m_JointScreenPts.Insert(GetGame().GetScreenPos(wsi.cachedJoints[jp]));
                        }

                        int ji;
                        for (ji = 0; ji < m_JointScreenPts.Count(); ji = ji + 1)
                        {
                            vector jScreen = m_JointScreenPts[ji];
                            if (jScreen[2] > LFPG_BEHIND_CAM_Z)
                            {
                                hud.DrawJointScreen(jScreen[0], jScreen[1], jSize, decColor);
                            }
                        }
                    }
                }
            }

            // G5: this wire was fully drawn
            tRnd.m_WiresDrawn = tRnd.m_WiresDrawn + 1;
            tRnd.m_SegmentsDrawn = tRnd.m_SegmentsDrawn + segCount;
        }
    }

    // -------------------------------------------
    // Wire-level occlusion check.
    // Raycasts to each coarse sample point (1-3).
    // Returns true (blocked) only if ALL samples
    // are occluded. If ANY is visible, wire shows.
    // -------------------------------------------
    protected bool CheckWireOcclusion(vector camPos, LFPG_WireSegmentInfo wsi, Object ignoreObj)
    {
        if (!wsi.occSamples || wsi.occSamples.Count() == 0)
            return false;

        int blockedCount = 0;
        int sampleCount = wsi.occSamples.Count();

        // G5: get render metrics once outside loop
        LFPG_RenderMetrics occRnd = LFPG_Telemetry.GetRender();

        int si;
        for (si = 0; si < sampleCount; si = si + 1)
        {
            // Slight Y offset to avoid ground self-occlusion
            vector target = wsi.occSamples[si];
            target[1] = target[1] + LFPG_OCC_SAMPLE_LIFT_M;

            vector hitPos;
            vector hitNormal;
            int contactComponent;

            // G5: count occlusion raycast
            occRnd.m_OccRaycastsUsed = occRnd.m_OccRaycastsUsed + 1;

            set<Object> rayResults = null;
            Object rayWith = null;
            bool bSorted = false;
            bool bGround = false;
            float rayRadius = 0.0;
            bool hit = DayZPhysics.RaycastRV(camPos, target, hitPos, hitNormal, contactComponent, rayResults, rayWith, ignoreObj, bSorted, bGround, ObjIntersectView, rayRadius);

            if (hit)
            {
                // v0.7.11 (A3): Early-out in squared domain.
                // If hitDistSq >= targetDistSq, hit is at or beyond target → NOT occluded.
                // Only compute sqrt when hit is closer (need precision for 0.3m margin).
                float hitDistSq = LFPG_WorldUtil.DistSq(camPos, hitPos);
                float targetDistSq = LFPG_WorldUtil.DistSq(camPos, target);

                if (hitDistSq < targetDistSq)
                {
                    // Hit closer than target: verify with margin using real distances
                    float hitDist = Math.Sqrt(hitDistSq);
                    float targetDist = Math.Sqrt(targetDistSq);

                    if (hitDist < targetDist - 0.3)
                    {
                        blockedCount = blockedCount + 1;
                    }
                }
            }
            // else: no hit = visible
        }

        // Occluded only if ALL samples are blocked
        // v0.7.35 (F2.3): Store blocked ratio for partial occlusion alpha.
        // Use float intermediates — Enforce implicit int→float is safe;
        // explicit (float) cast is untested in this codebase.
        if (sampleCount > 0)
        {
            float fBlocked = blockedCount;
            float fTotal = sampleCount;
            wsi.occBlockedRatio = fBlocked / fTotal;
        }
        else
        {
            wsi.occBlockedRatio = 0.0;
        }

        if (blockedCount >= sampleCount)
            return true;

        return false;
    }

    // ===========================
    // RetryTick - deferred wire build
    // ===========================
    // Runs every 5s. Only processes wires in the retry queue.
    // v0.7.10 P3: Differentiates TARGET_MISSING vs BUDGET retries.
    // Budget retries do NOT consume retry attempts (they aren't failures).
    // TARGET_MISSING retries only increment when resolution actually fails.
    protected void RetryTick()
    {
        if (m_RetryQueue.Count() == 0)
            return;

        // Collect keys to process (cannot modify map during iteration)
        m_TempKeys.Clear();
        int i;
        for (i = 0; i < m_RetryQueue.Count(); i = i + 1)
        {
            m_TempKeys.Insert(m_RetryQueue.GetKey(i));
        }

        // Segment budget
        int totalSegs = m_TotalSegCount;

        // G5: get render metrics once outside loop
        LFPG_RenderMetrics retTelRnd = LFPG_Telemetry.GetRender();

        int k;
        for (k = 0; k < m_TempKeys.Count(); k = k + 1)
        {
            string wireKey = m_TempKeys[k];
            ref LFPG_RetryEntry entry;
            if (!m_RetryQueue.Find(wireKey, entry) || !entry)
            {
                m_RetryQueue.Remove(wireKey);
                continue;
            }

            // v0.7.38 (M11): TTL expiry for BUDGET retries.
            // BUDGET entries accumulate indefinitely in long sessions when
            // segment budget is always saturated. Expire after 60s — the wire
            // will be rebuilt on next CullTick if still visible.
            if (entry.reason == LFPG_RetryReason.BUDGET)
            {
                float ageS = GetGame().GetTickTime() - entry.createdMs;
                if (ageS > LFPG_RETRY_BUDGET_TTL_S)
                {
                    m_RetryQueue.Remove(wireKey);
                    continue;
                }
            }

            // Find owner state
            ref LFPG_OwnerWireState st;
            if (!m_ByOwnerId.Find(entry.ownerDeviceId, st) || !st || !st.wires)
            {
                m_RetryQueue.Remove(wireKey);
                continue;
            }

            if (entry.wireIndex < 0 || entry.wireIndex >= st.wires.Count())
            {
                m_RetryQueue.Remove(wireKey);
                continue;
            }

            // Resolve owner
            EntityAI ownerObj = EntityAI.Cast(GetGame().GetObjectByNetworkId(st.ownerLow, st.ownerHigh));
            if (!ownerObj)
            {
                // Owner still not loaded.
                // Only count as failure for TARGET_MISSING entries.
                if (entry.reason == LFPG_RetryReason.TARGET_MISSING)
                {
                    entry.retryCount = entry.retryCount + 1;
                    if (entry.retryCount > LFPG_RETRY_MAX)
                    {
                        string rlOwnMsg = "[CableRenderer] Retry limit (owner missing) for " + wireKey + ", giving up";
                        LFPG_Util.Debug(rlOwnMsg);
                        m_RetryQueue.Remove(wireKey);
                    }
                }
                // BUDGET entries: owner was found at insertion time, may just be
                // streaming hiccup. Don't count. Keep in queue.
                continue;
            }

            // Resolve target
            LFPG_WireData wd = st.wires[entry.wireIndex];
            if (!wd)
            {
                m_RetryQueue.Remove(wireKey);
                continue;
            }

            // v0.7.45 (Patch 3C): Use extended resolver with NetworkID fallback
            EntityAI targetObj = ResolveDeviceEntityEx(wd.m_TargetDeviceId, wd.m_TargetNetLow, wd.m_TargetNetHigh);
            if (!targetObj)
            {
                // Target still not loaded.
                // Only count as failure for TARGET_MISSING entries.
                // v0.7.45 (P0 fix): Do NOT count if NegCache blocked the attempt.
                // Without this guard, NegCache-blocked retries consume the retry
                // budget without actually attempting resolution, causing permanent
                // cable loss after ~12s even though the entity may be loadable.
                if (entry.reason == LFPG_RetryReason.TARGET_MISSING && !m_LastResolveWasNegCached)
                {
                    entry.retryCount = entry.retryCount + 1;
                    if (entry.retryCount > LFPG_RETRY_MAX)
                    {
                        string rlTgtMsg = "[CableRenderer] Retry limit (target missing) for " + wireKey + ", giving up";
                        LFPG_Util.Debug(rlTgtMsg);
                        m_RetryQueue.Remove(wireKey);
                    }
                }
                continue;
            }

            // Both resolved: build the wire
            string srcPort = wd.m_SourcePort;
            if (srcPort == "")
            {
                srcPort = "output_1";
            }
            vector a = LFPG_DeviceAPI.GetPortWorldPos(ownerObj, srcPort);
            vector b = LFPG_DeviceAPI.GetPortWorldPos(targetObj, wd.m_TargetPort);

            a = LFPG_WorldUtil.ClampAboveSurface(a);
            b = LFPG_WorldUtil.ClampAboveSurface(b);

            m_TempPoints.Clear();
            m_TempPoints.Insert(a);

            if (wd.m_Waypoints && wd.m_Waypoints.Count() > 0)
            {
                int j;
                for (j = 0; j < wd.m_Waypoints.Count(); j = j + 1)
                {
                    m_TempPoints.Insert(LFPG_WorldUtil.ClampAboveSurface(wd.m_Waypoints[j], LFPG_SURFACE_CLAMP_M));
                }
            }
            else
            {
                m_TempPoints.Insert(LFPG_WorldUtil.AutoMidpointAboveTerrain(a, b));
            }

            m_TempPoints.Insert(b);

            // v0.7.9: adaptive budget estimation
            int estSegs = EstimateSegments(m_TempPoints);
            if (totalSegs + estSegs > LFPG_MAX_RENDERED_SEGS)
            {
                // Budget exceeded. If this was a TARGET_MISSING entry whose target
                // is now found, convert to BUDGET so it stops counting retries.
                if (entry.reason == LFPG_RetryReason.TARGET_MISSING)
                {
                    entry.reason = LFPG_RetryReason.BUDGET;
                }
                // G5: wire skipped by segment budget (retry path)
                retTelRnd.m_WiresBudget = retTelRnd.m_WiresBudget + 1;
                continue; // Over budget, retry next tick (no retryCount increment)
            }

            totalSegs = totalSegs + estSegs;

            st.lastPowered = IsOwnerActive(ownerObj);
            st.lastLoadRatio = LFPG_DeviceAPI.GetLoadRatio(ownerObj);
            st.lastOverloadMask = LFPG_DeviceAPI.GetOverloadMask(ownerObj);
            st.lastWarningMask = LFPG_DeviceAPI.GetWarningMask(ownerObj);
            BuildWire(wireKey, m_TempPoints, st.lastPowered, a, b, wd.m_Waypoints, entry.wireIndex);

            m_RetryQueue.Remove(wireKey);

            string retOkMsg = "[CableRenderer] Retry succeeded: " + wireKey;
            LFPG_Util.Debug(retOkMsg);
        }
    }

    // ===========================
    // Retry queue helpers
    // ===========================
    protected void AddRetry(string ownerDeviceId, int wireIndex, int retryReason)
    {
        string wireKey = ownerDeviceId + "|" + wireIndex.ToString();

        if (m_RetryQueue.Contains(wireKey))
            return;

        ref LFPG_RetryEntry entry = new LFPG_RetryEntry();
        entry.ownerDeviceId = ownerDeviceId;
        entry.wireIndex = wireIndex;
        entry.retryCount = 0;
        entry.reason = retryReason;
        entry.createdMs = GetGame().GetTickTime();

        m_RetryQueue[wireKey] = entry;
    }

    protected void ClearOwnerRetries(string ownerDeviceId)
    {
        m_TempKeys.Clear();
        string prefix = ownerDeviceId + "|";

        int i;
        for (i = 0; i < m_RetryQueue.Count(); i = i + 1)
        {
            if (m_RetryQueue.GetKey(i).IndexOf(prefix) == 0)
            {
                m_TempKeys.Insert(m_RetryQueue.GetKey(i));
            }
        }

        int k;
        for (k = 0; k < m_TempKeys.Count(); k = k + 1)
        {
            m_RetryQueue.Remove(m_TempKeys[k]);
        }
    }

    // ===========================
    // Catenaria (v0.7.9: adaptive subdivisions + quadratic sag)
    // ===========================

    // Determine optimal subdivision count for a segment based on its length.
    // Short cables look taut (0 subs), long cables get more curvature.
    static int GetAdaptiveSubs(float segLen)
    {
        if (segLen < LFPG_SAG_SHORT_THRESH_M)
            return 0;
        if (segLen < 8.0)
            return 1;
        if (segLen < 15.0)
            return 2;
        if (segLen < 25.0)
            return 3;
        return 4;
    }

    // Pre-estimate total segments from a raw point chain (for budget checks).
    // Mirrors GetAdaptiveSubs logic without computing actual geometry.
    protected int EstimateSegments(array<vector> rawPts)
    {
        if (!rawPts || rawPts.Count() < 2)
            return 0;

        int total = 0;
        int seg;
        for (seg = 0; seg < rawPts.Count() - 1; seg = seg + 1)
        {
            float segLen = vector.Distance(rawPts[seg], rawPts[seg + 1]);
            int subs = GetAdaptiveSubs(segLen);
            total = total + subs + 1;
        }
        return total;
    }

    // Compute sag factor for a given segment length.
    // Linear below SAG_QUAD_REF_M, quadratic above (physically correct).
    // Real cable sag: s = wL^2 / (8T). At constant tension, sag ~ L^2.
    static float GetSagAmount(float segLen)
    {
        float sagFactor = LFPG_SAG_FACTOR;
        if (segLen > LFPG_SAG_QUAD_REF_M)
        {
            float ratio = segLen / LFPG_SAG_QUAD_REF_M;
            sagFactor = LFPG_SAG_FACTOR * ratio;
        }
        return segLen * sagFactor;
    }

    // Apply catenaria sag to a raw point chain.
    // v0.7.9: Self-contained. Each segment pair gets adaptive subdivisions
    // and quadratic sag scaling. No external subdivision parameter needed.
    //
    // Input rawPts: [portA, wp1?, wp2?, ..., portB]
    // Output m_SagPoints: interpolated chain with sag sub-points.
    protected void ApplyCatenaria(array<vector> rawPts)
    {
        m_SagPoints.Clear();

        if (!rawPts || rawPts.Count() < 2)
            return;

        int rawCount = rawPts.Count();
        m_SagPoints.Insert(rawPts[0]);

        int seg;
        for (seg = 0; seg < rawCount - 1; seg = seg + 1)
        {
            vector segA = rawPts[seg];
            vector segB = rawPts[seg + 1];
            float segLen = vector.Distance(segA, segB);

            int subs = GetAdaptiveSubs(segLen);

            if (subs > 0)
            {
                float sagAmount = GetSagAmount(segLen);

                int sub;
                for (sub = 1; sub <= subs; sub = sub + 1)
                {
                    float t = sub / (subs + 1.0);
                    vector lerp = segA + (segB - segA) * t;
                    float sag = sagAmount * 4.0 * t * (1.0 - t);
                    lerp[1] = lerp[1] - sag;

                    // Clamp sag point above terrain surface
                    lerp = LFPG_WorldUtil.ClampAboveSurface(lerp, LFPG_SURFACE_CLAMP_M);

                    m_SagPoints.Insert(lerp);
                }
            }

            m_SagPoints.Insert(segB);
        }
    }

    // ===========================
    // Segment budget (v0.7.9: incremental via m_TotalSegCount)
    // ===========================

    // ===========================
    // Wire segment cleanup
    // ===========================
    protected void DestroyWire(string key)
    {
        ref LFPG_WireSegmentInfo info;
        if (m_WireSegments.Find(key, info) && info)
        {
            // v0.7.9: update incremental counter before destroying
            if (info.segments)
            {
                m_TotalSegCount = m_TotalSegCount - info.segments.Count();
                if (m_TotalSegCount < 0)
                {
                    m_TotalSegCount = 0;
                }
            }
            info.DestroyAll();
        }
        m_WireSegments.Remove(key);
    }

    protected void DestroyOwnerLines(string ownerId)
    {
        m_TempKeys.Clear();
        string prefix = ownerId + "|";

        // v0.7.36 (L2): Filter during collection instead of collecting all keys.
        // With 200+ wires across 20+ owners, this avoids copying ~190 irrelevant keys.
        int i;
        for (i = 0; i < m_WireSegments.Count(); i = i + 1)
        {
            string key = m_WireSegments.GetKey(i);
            if (key.IndexOf(prefix) == 0)
            {
                m_TempKeys.Insert(key);
            }
        }

        int k;
        for (k = 0; k < m_TempKeys.Count(); k = k + 1)
        {
            DestroyWire(m_TempKeys[k]);
        }
    }

    // ===========================
    // Cleanup: destroy all (game shutdown / full reset)
    // ===========================
    void DestroyAll()
    {
        m_TempKeys.Clear();
        int i;
        for (i = 0; i < m_WireSegments.Count(); i = i + 1)
        {
            m_TempKeys.Insert(m_WireSegments.GetKey(i));
        }

        int k;
        for (k = 0; k < m_TempKeys.Count(); k = k + 1)
        {
            DestroyWire(m_TempKeys[k]);
        }

        m_RetryQueue.Clear();
    }

};
