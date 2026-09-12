#ifndef SERVER
// Client-only compilation boundary
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
// v0.7.48 (Bug 3): Cables visible through building floors.
//   - Occlusion hit margin reduced 0.3→0.10m (LFPG_OCC_HIT_MARGIN_M).
//   - Algebraic optimisation: eliminates one Math.Sqrt per sample
//     via (hitDist + M)^2 < targetDistSq instead of hitDist < targetDist - M.
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
//   F1.3 — v1.0: binary overload via LFPG_DeviceAPI.GetOverloaded().
//          All wires show CRITICAL_LOAD when owner is overloaded.
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
// Connection cache: updated per owner on snapshots, deltas and local purge.
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

class LFPG_ConnectionContributors
{
	ref map<string, string> m_ByOwner = new map<string, string>;
};

class LFPG_OwnerWireState
{
    string ownerDeviceId;
    int ownerLow;
    int ownerHigh;

    ref array<ref LFPG_WireData> wires;

    // Last JSON received (for change detection; avoids redundant decode)
    string lastJson;

    // v1.2.3: last authoritative owner mutation generation applied locally.
    int wireGeneration = -1;

    // Last known powered state (persists when entity is out of bubble)
    bool lastPowered;

    // v0.7.8: Load ratio from source (0.0-N), synced from server.
    float lastLoadRatio;

    // v1.0: Overloaded state (all-off policy). If true, ALL wires show CRITICAL_LOAD.
    bool lastOverloaded;

    // v0.7.9: Pre-computed wire keys ("ownerId|0", "ownerId|1", etc.)
    // Populated in BuildOwnerWires. Eliminates string concat in CullTick.
    ref array<string> cachedWireKeys;

    // v0.7.9: Consecutive CullTick cycles where ownerObj was null.
    // After threshold, wires are destroyed (device likely deleted/despawned).
    int nullOwnerTicks;

	ref array<string> m_ConnectionKeys = new array<string>;
	ref map<string, bool> m_CacheDevices = new map<string, bool>;
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

    // v0.7.8: Waypoint world positions for joint rendering.
    // Stored at build time. Joints are drawn only at LOD close.
    ref array<vector> cachedJoints;

    // ---- Wire-level occlusion (raycast Z-buffer emulation) ----
    // Coarse sample points for raycast (1 or 3 depending on wire length).
    // Built once in BuildWire. Raycasts target these, not individual sub-segs.
    ref array<vector> occSamples;
    ref array<bool> occSampleBlocked;
    int    occSampleCursor;
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

    // Per-wire projection caches. Valid only for identical camera and sway inputs.
    ref array<vector> cachedScreenPts;
    ref array<vector> cachedJointScreenPts;
    vector screenCacheCamPos;
    vector screenCacheCamDir;
    float screenCacheSwayY;
    float screenCacheSwayX;
    float screenCacheViewportW;
    float screenCacheViewportH;
    bool screenCacheValid;
    vector jointCacheCamPos;
    vector jointCacheCamDir;
    float jointCacheViewportW;
    float jointCacheViewportH;
    bool jointCacheValid;
    vector ultraScreenA;
    vector ultraScreenB;
    vector ultraCacheCamPos;
    vector ultraCacheCamDir;
    float ultraCacheViewportW;
    float ultraCacheViewportH;
    bool ultraCacheValid;
	int m_ScreenProjectionRevision;
	int m_JointProjectionRevision;
	int m_UltraProjectionRevision;
    int decoratorAllowance;

    void LFPG_WireSegmentInfo()
    {
        segments   = new array<ref LFPG_CableParticle>;
        occSamples = new array<vector>;
        occSampleBlocked = new array<bool>;
        cachedJoints = new array<vector>;
        cachedScreenPts = new array<vector>;
        cachedJointScreenPts = new array<vector>;
        visible    = true;
        occluded   = false;
        occNextCheckMs  = 0;
        occConsecCount  = 0;
        occBlockedRatio = 0.0;
        occStaggerGroup = 0;
        occSampleCursor = 0;
        screenCacheValid = false;
        jointCacheValid = false;
        ultraCacheValid = false;
        decoratorAllowance = 0;
        cachedCenter    = "0 0 0";
        cachedRadius    = 0.0;
        cachedMinDist   = 0.0;
        cableState      = LFPG_CableState.IDLE;
    }

    // Build occlusion sample points from ACTUAL cable geometry.
    //
    // v0.8.2: Endpoint-anchored sampling with per-span distribution
    // for waypoint cables. Fixes long cable visibility behind walls.
    //
    // ---- ROOT CAUSE OF BUG ----
    //   A cable [DeviceA]--10% visible--[WALL]--90% hidden--[DeviceB]
    //   with uniform samples at 16/33/50/66/83% has NO sample in the
    //   visible 10% section. All samples blocked → cable hides entirely.
    //
    // ---- FIX ----
    //   Always insert cachedPosA and cachedPosB as samples first.
    //   For waypoint cables (Branch A): sample the MIDPOINT of each span
    //   (A→j[0], j[0]→j[1], ...) instead of uniform length fractions.
    //   Each span is independently testable. If the span near the player
    //   is visible, its midpoint passes → cable shows.
    //
    // ---- BUDGET CONSTRAINT (CRITICAL — DO NOT EXCEED 5 SAMPLES) ----
    //   DrawFrame has a STRICT budget check:
    //     if (samplesNeeded <= rayBudget) → check; else → SKIP entirely
    //   Worst-case adaptive budget = max(3, OCC_MAX_RAYCASTS/4) = 5
    //   for bases with >50 wires. A wire with 6+ samples PERMANENTLY
    //   skips its recheck → freezes occluded=true → invisible forever.
    //
    // ---- PARTIAL THRESHOLD INTERACTION (CRITICAL — DO NOT IGNORE) ----
    //   UpdateOcclusion treats a wire as blocked (→ hides it) when:
    //     occBlockedRatio >= LFPG_OCC_PARTIAL_THRESHOLD (≈0.66)
    //   This means "cable shows" requires AT LEAST 2 samples to pass.
    //   With N total samples, blockedCount ≤ N-2 → ratio ≤ (N-2)/N:
    //     N=3: ratio ≤ 0.33 < 0.66 ✓   N=4: ratio ≤ 0.50 < 0.66 ✓
    //     N=5: ratio ≤ 0.60 < 0.66 ✓
    //   Branch A achieves this: posA + span0_midpoint are BOTH visible
    //   whenever the near-player span is unobstructed (the common case).
    //   Branch B: only posA passes for cables going straight into a wall.
    //     Short (2 samples): 1/2=0.50 → shows ✓
    //     Medium (3 samples): 2/3=0.667 ≥ 0.66 → hides (same as before)
    //     Long (5 samples): 4/5=0.80 → hides (same as before)
    //   This is intentional: straight cables that immediately enter a wall
    //   are correctly hidden. Branch A fixes the actual reported use case
    //   ("cable with turns behind a wall").
    //
    // ---- SAMPLE ALLOCATION (all totals ≤ 5) ----
    //   Branch A, 1-3 spans: 1 midpoint/span + 2 ep → 3-5 total
    //   Branch A, 4+ spans:  first+middle+last + 2 ep → 5 total
    //   Branch B, short (<LONG_WIRE_M): 0 interior + 2 ep → 2 total
    //   Branch B, medium (>=LONG_WIRE_M): 1 interior + 2 ep → 3 total
    //   Branch B, long (>=20m): 3 interior + 2 ep → 5 total
    void BuildOccSamples()
    {
        occSamples.Clear();
        occSampleBlocked.Clear();
        occSampleCursor = 0;

        // Always sample both device endpoint positions (2 of 5 budget).
        // Ensures the cable shows whenever either device is directly visible
        // from the camera. Also provides the second "passing sample" that
        // keeps occBlockedRatio below LFPG_OCC_PARTIAL_THRESHOLD in
        // Branch A when posA + span0_mid are both unobstructed.
        occSamples.Insert(cachedPosA);
        occSampleBlocked.Insert(false);
        occSamples.Insert(cachedPosB);
        occSampleBlocked.Insert(false);

        if (!segments || segments.Count() == 0)
            return;

        // Compute total chain length (needed for Branch B tier selection)
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
            return;  // endpoints already inserted, sufficient

        // ------------------------------------------------------------------
        // BRANCH A: Cable with player-placed waypoints (corner joints).
        //
        // Sample the midpoint of each span:
        //   A → j[0], j[0] → j[1], ..., j[N-1] → B
        //
        // This is the primary fix for the reported bug. The visible span
        // (e.g. A→j[0] before the wall) produces a midpoint that passes
        // the raycast. Combined with posA (also passing), 2 of N samples
        // pass → occBlockedRatio < LFPG_OCC_PARTIAL_THRESHOLD → shows.
        //
        // Interior budget = 3 (total ≤ 5).
        //   1-3 spans (jCount 1-2): 1 midpoint/span (3-5 total)
        //   4+ spans  (jCount >=3): first+middle+last span (5 total)
        //
        // Index safety proof for 4+ spans path:
        //   jCount = spanCount-1 >= 3
        //   midSpan = spanCount/2 (floor) → midSpan in [2, spanCount/2]
        //   cachedJoints[midSpan-1]: max index = spanCount/2-1 < jCount ✓
        //   midNext = midSpan+1: max = spanCount/2+1 ≤ spanCount-1 = jCount
        //   so midNext > jCount is unreachable (proof in comment below).
        // ------------------------------------------------------------------
        if (cachedJoints && cachedJoints.Count() > 0)
        {
            int jCount = cachedJoints.Count();
            int spanCount = jCount + 1;

            // Hoisted before branching: Enforce Script forbids same-name
            // variable declarations in sibling if/else blocks.
            vector spnA;
            vector spnB;
            vector spnMid;
            // nextIdx hoisted here — declared inside a loop body can
            // cause compiler issues in some Enforce Script versions.
            int nextIdx;

            if (spanCount <= 3)
            {
                // 1-3 spans: one midpoint per span (budget allows it)
                int ni;
                for (ni = 0; ni < spanCount; ni = ni + 1)
                {
                    if (ni == 0)
                        spnA = cachedPosA;
                    else
                        spnA = cachedJoints[ni - 1];

                    nextIdx = ni + 1;
                    if (nextIdx > jCount)
                        spnB = cachedPosB;
                    else
                        spnB = cachedJoints[nextIdx - 1];

                    spnMid[0] = (spnA[0] + spnB[0]) * 0.5;
                    spnMid[1] = (spnA[1] + spnB[1]) * 0.5;
                    spnMid[2] = (spnA[2] + spnB[2]) * 0.5;
                    occSamples.Insert(spnMid);
                    occSampleBlocked.Insert(false);
                }
            }
            else
            {
                // 4+ spans: first + last + middle (3 interior samples).
                // First and last cover the exits from each device.
                // Middle covers cables traversing multiple rooms.

                // Span 0: cachedPosA → j[0]
                spnA = cachedPosA;
                spnB = cachedJoints[0];
                spnMid[0] = (spnA[0] + spnB[0]) * 0.5;
                spnMid[1] = (spnA[1] + spnB[1]) * 0.5;
                spnMid[2] = (spnA[2] + spnB[2]) * 0.5;
                occSamples.Insert(spnMid);
                occSampleBlocked.Insert(false);

                // Last span: j[jCount-1] → cachedPosB
                spnA = cachedJoints[jCount - 1];
                spnB = cachedPosB;
                spnMid[0] = (spnA[0] + spnB[0]) * 0.5;
                spnMid[1] = (spnA[1] + spnB[1]) * 0.5;
                spnMid[2] = (spnA[2] + spnB[2]) * 0.5;
                occSamples.Insert(spnMid);
                occSampleBlocked.Insert(false);

                // Middle span (floor of spanCount/2).
                // midNext > jCount proof: midSpan+1 > jCount = spanCount-1
                //   → spanCount/2 > spanCount-2 → only true when spanCount<4.
                //   We are in spanCount>=4 branch, so this never triggers.
                int midSpan = spanCount / 2;
                int midNext = midSpan + 1;

                spnA = cachedJoints[midSpan - 1];
                if (midNext > jCount)
                    spnB = cachedPosB;
                else
                    spnB = cachedJoints[midNext - 1];

                spnMid[0] = (spnA[0] + spnB[0]) * 0.5;
                spnMid[1] = (spnA[1] + spnB[1]) * 0.5;
                spnMid[2] = (spnA[2] + spnB[2]) * 0.5;
                occSamples.Insert(spnMid);
                occSampleBlocked.Insert(false);
            }

            return;
        }

        // ------------------------------------------------------------------
        // BRANCH B: Straight cable (no waypoints). Uniform interior samples.
        // Endpoints already inserted. Interior budget = 3.
        //
        //   < LONG_WIRE_M:  0 interior → 2 total
        //   >= LONG_WIRE_M: 1 interior → 3 total (midpoint)
        //   >= 20m:         3 interior → 5 total (25%, 50%, 75%)
        //
        // For straight cables entering a wall immediately after posA:
        //   Short (2 samples): ratio=0.50 → shows with partial fade ✓
        //   Medium (3 samples): ratio=0.667 ≥ threshold → hides (same as
        //   original behavior — acceptable for cables going straight into
        //   walls with no waypoints to mark the visible section).
        //
        // The interior samples serve a different purpose here: detecting
        // cables whose MIDDLE is occluded (both endpoints visible but cable
        // dips through geometry). occBlockedRatio then produces a partial
        // alpha fade (DrawFrame line ~2326) proportional to blockage.
        // ------------------------------------------------------------------
        int uniformCount = 0;
        if (totalLen >= 20.0)
        {
            uniformCount = 3;
        }
        else if (totalLen >= LFPG_OCC_LONG_WIRE_M)
        {
            uniformCount = 1;
        }

        if (uniformCount == 0)
            return;  // short straight cable — endpoints are sufficient

        int si;
        LFPG_CableParticle s;
        for (si = 0; si < uniformCount; si = si + 1)
        {
            float frac;
            if (uniformCount == 1)
            {
                frac = 0.5;
            }
            else
            {
                // uniformCount == 3: fractions 0.25, 0.50, 0.75
                frac = (si + 1.0) / (uniformCount + 1.0);
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
                float nextWalked = walked + segLen;
                if (nextWalked >= targetDist)
                {
                    float remain = targetDist - walked;
                    float t = 0.5;
                    if (segLen > 0.01)
                        t = remain / segLen;

                    vector pt = s.m_From + (s.m_To - s.m_From) * t;
                    occSamples.Insert(pt);
                    occSampleBlocked.Insert(false);
                    found = true;
                    break;
                }
                walked = nextWalked;
            }

            if (!found)
            {
                occSamples.Insert(cachedPosB);
                occSampleBlocked.Insert(false);
            }
        }
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
            occSampleCursor = 0;
            int si;
            for (si = 0; si < occSampleBlocked.Count(); si = si + 1)
            {
                occSampleBlocked[si] = false;
            }
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

// v0.9.2 (M1): LFPG_OCC_HIT_MARGIN_M moved to LFPG_Defines.c.

class LFPG_CableRenderer
{
    protected static ref LFPG_CableRenderer s_Instance;

    // v0.7.35 D1: Cooldown map for REQUEST_DEVICE_SYNC RPCs.
    // Maps deviceId -> tick time of the last valid received blob. Prevents RPC spam
    // while allowing one response-timeout retry for a lost or rejected batch.
    protected static ref map<string, float> s_DeviceSyncCooldowns;

    protected ref map<string, int> m_PendingDeviceSyncLow;
    protected ref map<string, int> m_PendingDeviceSyncHigh;
    protected ref map<string, bool> m_PendingDeviceSyncForced;
    protected ref map<string, bool> m_PendingDeviceSyncRetry;
    protected ref map<string, int> m_DeviceSyncRetryLow;
    protected ref map<string, int> m_DeviceSyncRetryHigh;
    protected ref map<string, float> m_DeviceSyncRetryDue;
	protected ref map<string, int> m_DeviceSyncInFlightGeneration;
	protected ref map<string, bool> m_DeviceSyncRetryAttempted;
    protected ref map<string, int> m_AnnouncedWireGenerations;
	protected ref map<string, int> m_KnownCableDevices;
    protected ref array<string> m_DeviceSyncBatchIds;
    protected ref array<string> m_DeviceSyncDropIds;
    protected ref array<string> m_DeviceSyncRetryIds;
    protected bool m_DeviceSyncFlushQueued;
    protected int m_PerfDiagDeviceSyncBatchCount;

    // v4.5: Server setting synced via RPC on JIP.
    // When true, cables are hidden unless player holds CableReel or Pliers.
    protected static bool s_ServerHideCablesNoReel = false;

    static void SetServerHideCablesNoReel(bool val)
    {
        s_ServerHideCablesNoReel = val;
    }

    protected ref map<string, ref LFPG_OwnerWireState> m_ByOwnerId;

    // Wire segment info (key = "ownerId|wireIdx")
    protected ref map<string, ref LFPG_WireSegmentInfo> m_WireSegments;

    // Retry queue: wires that failed to build due to unresolved targets.
    // Keyed by wireKey ("ownerId|wireIdx").
    protected ref map<string, ref LFPG_RetryEntry> m_RetryQueue;

    // Pre-allocated temp arrays (avoids GC pressure in helpers)
    protected ref array<vector> m_TempPoints;
    protected ref array<vector> m_SagPoints;     // catenaria output buffer
	protected ref array<vector> m_BoundsPoints;
    protected ref array<string> m_TempKeys;      // reused in DestroyOwnerLines etc.
    protected ref array<string> m_GhostKeys;     // v0.7.38 (L4): reused for ghost owner cleanup

	// Connection cache: key = "deviceId|portName|dir" -> owner contributions.
	protected ref map<string, ref LFPG_ConnectionContributors> m_ConnCache;

    // Negative resolution cache: deviceIds that failed to resolve recently.
    // Avoids re-scanning for unresolvable entities on every RetryTick.
    protected ref map<string, float> m_NegCache;
    protected static const float NEG_CACHE_TTL_MS = 5000.0; // 5 seconds
    // v0.7.45 (P0 fix): Set by ResolveDeviceEntityEx when NegCache blocked
    // the resolution attempt. RetryTick uses this to avoid wasting retries.
    protected bool m_LastResolveWasNegCached;

    // Periodic neg cache purge interval (ms)
    protected static const int NEG_CACHE_PURGE_INTERVAL_MS = 60000; // 60 seconds

    // Sway sample cadence so screenCacheSwayY/X can match across still-camera frames.
    // Applied only while the camera is still; moving camera keeps continuous nowMs
    // (projection already misses, so forcing 20 Hz sway for all frames is useless).
    protected static const float LFPG_SWAY_CACHE_CADENCE_MS = 50.0;

    // v0.7.9: Incremental segment budget counter.
    // Updated in BuildWire (+) and DestroyWire (-) to avoid O(N) CountTotalSegments.
    protected int m_TotalSegCount;

    // U6: acumuladores del tick de mantenimiento, en segundos. Sustituyen a las
    // cuatro cadenas CallLater repetidas que se registraban en el constructor.
    protected float m_CullAccS;
    protected float m_RetryAccS;
    protected float m_PurgeAccS;
    protected float m_ReconcileAccS;

    // ---- Occlusion: camera movement detection ----
    // Skip occlusion rechecks when camera is stationary.
    protected vector m_LastCamPos;
    protected vector m_LastCamDir;
    protected bool   m_CamMoved;       // set per frame in DrawFrame
	protected vector m_ProjectionProbeX;
	protected vector m_ProjectionProbeY;
	protected vector m_ProjectionProbeZ;
	protected int m_ProjectionRevision;

    // C4: reuse 3P player screen rect while cam projection + player root stay still.
    protected vector m_PlOccLastPlPos;
    protected int    m_PlOccProjectionRevision;
    protected bool   m_PlOccCacheValid;
    protected bool   m_PlOccCachedActive;
    protected float  m_PlOccRectX1;
    protected float  m_PlOccRectY1;
    protected float  m_PlOccRectX2;
    protected float  m_PlOccRectY2;
    protected float  m_PlOccDepthThreshold;

    // ---- Occlusion: stagger round-robin ----
    // Distributes raycast cost across frames.
    protected int    m_OccStaggerIdx;
    protected float  m_NextOccRaycastTickMs;
    protected ref array<int> m_OccCursorByGroup;

    // v0.7.7: cached device bubble distance (read once from settings)
    protected float  m_DeviceBubbleM;

    // v0.7.23 (Bug 2): Painter's algorithm sort buffers.
    // Indices into m_WireSegments sorted by cachedMinDist descending (far-to-near).
    protected ref array<int>   m_DrawOrder;
    protected ref array<float> m_DrawDist;
	protected ref array<int> m_FrameDrawOrder;
    // vX (perf): draw order is cached and only rebuilt when its inputs change
    // (CullTick distance update, or wire add/remove) — not every frame.
    protected bool m_DrawOrderDirty;

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
		m_BoundsPoints = new array<vector>;
        m_TempKeys        = new array<string>;
        m_GhostKeys       = new array<string>;
		m_ConnCache = new map<string, ref LFPG_ConnectionContributors>;
        m_NegCache        = new map<string, float>;
        m_PendingDeviceSyncLow = new map<string, int>;
        m_PendingDeviceSyncHigh = new map<string, int>;
        m_PendingDeviceSyncForced = new map<string, bool>;
        m_PendingDeviceSyncRetry = new map<string, bool>;
        m_DeviceSyncRetryLow = new map<string, int>;
        m_DeviceSyncRetryHigh = new map<string, int>;
        m_DeviceSyncRetryDue = new map<string, float>;
		m_DeviceSyncInFlightGeneration = new map<string, int>;
		m_DeviceSyncRetryAttempted = new map<string, bool>;
        m_AnnouncedWireGenerations = new map<string, int>;
		m_KnownCableDevices = new map<string, int>;
        m_DeviceSyncBatchIds = new array<string>;
        m_DeviceSyncDropIds = new array<string>;
        m_DeviceSyncRetryIds = new array<string>;
        m_DeviceSyncFlushQueued = false;
        m_LastResolveWasNegCached = false;
        m_DrawOrder       = new array<int>;
        m_DrawDist        = new array<float>;
		m_FrameDrawOrder = new array<int>;
        m_DrawOrderDirty  = true;
        m_ClipA           = "0 0 0";
        m_ClipB           = "0 0 0";
        m_OccStaggerIdx   = 0;
        m_NextOccRaycastTickMs = 0.0;
        m_OccCursorByGroup = new array<int>;
        m_OccCursorByGroup.Insert(0);
        m_OccCursorByGroup.Insert(0);
        m_OccCursorByGroup.Insert(0);
        m_CamMoved        = true;
        m_TotalSegCount   = 0;
        m_PlOccCacheValid = false;
        m_PlOccCachedActive = false;
        m_PlOccProjectionRevision = -1;
        m_PlOccLastPlPos = "0 0 0";
        m_PlOccRectX1 = 0.0;
        m_PlOccRectY1 = 0.0;
        m_PlOccRectX2 = 0.0;
        m_PlOccRectY2 = 0.0;
        m_PlOccDepthThreshold = 0.0;

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

        m_CullAccS      = 0.0;
        m_RetryAccS     = 0.0;
        m_PurgeAccS     = 0.0;
        m_ReconcileAccS = 0.0;
    }

    static LFPG_CableRenderer Get()
    {
        if (g_Game.IsDedicatedServer())
            return null;

        if (!s_Instance)
            s_Instance = new LFPG_CableRenderer();
        return s_Instance;
    }

    bool HasRenderableWires()
    {
		return m_TotalSegCount > 0;
    }

    // U6: un solo tick de mantenimiento, gobernado por el frame.
    // Antes eran cuatro cadenas CallLater repetidas en CALL_CATEGORY_GUI,
    // registradas en el constructor y desregistradas a mano en CleanupInstance;
    // una instancia que sobreviviera a Reset() dejaba temporizadores huerfanos
    // apuntando al objeto viejo. El hub de frame resuelve el singleton por Get()
    // en cada llamada, asi que ese modo de fallo desaparece.
    //
    // Los periodos son los mismos y arrancan a la vez, igual que los CallLater
    // registrados en el mismo instante, asi que la coincidencia de ticks no cambia.
    // El acumulador se pone a cero al disparar en vez de restar el periodo: tras un
    // tiron largo se dispara una vez, no en rafaga.
    void MaintenanceTick(float timeslice)
    {
        m_CullAccS      = m_CullAccS + timeslice;
        m_RetryAccS     = m_RetryAccS + timeslice;
        m_PurgeAccS     = m_PurgeAccS + timeslice;
        m_ReconcileAccS = m_ReconcileAccS + timeslice;

        if (m_CullAccS >= LFPG_CULL_TICK_S)
        {
            m_CullAccS = 0.0;
            CullTick();
        }
        if (m_RetryAccS >= LFPG_RETRY_TICK_S)
        {
            m_RetryAccS = 0.0;
            RetryTick();
        }

        float purgePeriodS = NEG_CACHE_PURGE_INTERVAL_MS / 1000.0;
        if (m_PurgeAccS >= purgePeriodS)
        {
            m_PurgeAccS = 0.0;
            PurgeNegCache();
        }

        float reconcilePeriodS = LFPG_RECONCILE_TICK_MS / 1000.0;
        if (m_ReconcileAccS >= reconcilePeriodS)
        {
            m_ReconcileAccS = 0.0;
            ReconcileTick();
        }
    }

    // v0.7.9: proper cleanup on destruction.
    // Deregisters the pending one-shot timers, releases all shape segments and
    // clears maps. Without this, Reset() during reconnect would leave orphaned
    // CallLater entries pointing to the old instance.
    // U6: los cuatro ticks repetidos que antes vivian aqui ya no se registran;
    // los gobierna MaintenanceTick desde el frame, asi que no pueden quedar
    // huerfanos. Solo quedan los one-shot del batch de sincronizacion.
    void ~LFPG_CableRenderer()
    {
        CleanupInstance();
    }

    protected void CleanupInstance()
    {
        if (g_Game)
        {
            ScriptCallQueue cq = g_Game.GetCallQueue(CALL_CATEGORY_GUI);
            if (cq)
            {
                cq.Remove(FlushDeviceSyncBatch);
                cq.Remove(CheckDeviceSyncBatchResponse);
            }
        }

        if (m_TempKeys && m_WireSegments && m_RetryQueue)
            DestroyAll();
        if (m_ByOwnerId)
            m_ByOwnerId.Clear();
        if (m_ConnCache)
            m_ConnCache.Clear();
        if (m_NegCache)
            m_NegCache.Clear();
        if (m_PendingDeviceSyncLow)
            m_PendingDeviceSyncLow.Clear();
        if (m_PendingDeviceSyncHigh)
            m_PendingDeviceSyncHigh.Clear();
        if (m_PendingDeviceSyncForced)
            m_PendingDeviceSyncForced.Clear();
        if (m_PendingDeviceSyncRetry)
            m_PendingDeviceSyncRetry.Clear();
        if (m_DeviceSyncRetryLow)
            m_DeviceSyncRetryLow.Clear();
        if (m_DeviceSyncRetryHigh)
            m_DeviceSyncRetryHigh.Clear();
        if (m_DeviceSyncRetryDue)
            m_DeviceSyncRetryDue.Clear();
		if (m_DeviceSyncInFlightGeneration)
			m_DeviceSyncInFlightGeneration.Clear();
		if (m_DeviceSyncRetryAttempted)
			m_DeviceSyncRetryAttempted.Clear();
        if (m_AnnouncedWireGenerations)
            m_AnnouncedWireGenerations.Clear();
        if (m_KnownCableDevices)
            m_KnownCableDevices.Clear();
        m_DeviceSyncFlushQueued = false;
    }

    // v0.7.5: called from MissionGameplay.OnInit to ensure a
    // clean slate when reconnecting to a server. Destroys all
    // segments and caches from the previous session.
    static void Reset()
    {
        // v0.7.36 (M3): Clear static cooldown map to prevent stale
        // throttle entries from a previous server session.
        s_DeviceSyncCooldowns = null;

        // v4.5: Reset server settings flag to default (visible).
        // Prevents carry-over from previous server session.
        s_ServerHideCablesNoReel = false;

        if (s_Instance)
        {
            s_Instance.CleanupInstance();
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
            EntityAI netObj = EntityAI.Cast(g_Game.GetObjectByNetworkId(netLow, netHigh));
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
        float nowMs = g_Game.GetTime();
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

        float nowMs = g_Game.GetTime();

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
            if (LFPG_LOG_LEVEL >= 2)
            {
                string ncMsg = "[CableRenderer] NegCache purged " + m_TempKeys.Count().ToString() + " expired entries";
                LFPG_Util.Debug(ncMsg);
            }
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
    protected int ResolveSnapshotGeneration(string ownerDeviceId)
    {
        int result = -1;
        m_AnnouncedWireGenerations.Find(ownerDeviceId, result);
        return result;
    }

	protected void AnnounceWireGeneration(string ownerDeviceId, int generation)
	{
		if (generation > ResolveSnapshotGeneration(ownerDeviceId))
			m_AnnouncedWireGenerations[ownerDeviceId] = generation;
	}

    void UpsertOwnerBlob(string ownerDeviceId, int low, int high, string json)
    {
		// Legacy blobs carry no generation; never label them with a newer announcement.
		UpsertOwnerBlobInternal(ownerDeviceId, low, high, json, -1);
    }

    void UpsertOwnerBlobV2(string ownerDeviceId, int low, int high, string json, int generation)
    {
        if (ownerDeviceId == "")
            return;
        UpsertOwnerBlobInternal(ownerDeviceId, low, high, json, generation);
    }

    protected void MarkDeviceSyncReceivedForId(string deviceId, float receivedAt)
    {
        if (deviceId == "")
            return;
        if (!s_DeviceSyncCooldowns)
        {
            s_DeviceSyncCooldowns = new map<string, float>;
        }
		if (NeedsDeviceSync(deviceId))
			return;
        s_DeviceSyncCooldowns[deviceId] = receivedAt;
        m_DeviceSyncRetryLow.Remove(deviceId);
        m_DeviceSyncRetryHigh.Remove(deviceId);
        m_DeviceSyncRetryDue.Remove(deviceId);
		m_DeviceSyncInFlightGeneration.Remove(deviceId);
		m_DeviceSyncRetryAttempted.Remove(deviceId);
		m_PendingDeviceSyncLow.Remove(deviceId);
		m_PendingDeviceSyncHigh.Remove(deviceId);
		m_PendingDeviceSyncForced.Remove(deviceId);
		m_PendingDeviceSyncRetry.Remove(deviceId);
    }

    protected void MarkDeviceSyncBlobReceived(string ownerDeviceId, LFPG_OwnerWireState st)
    {
        if (!g_Game)
            return;

        float receivedAt = g_Game.GetTickTime();
        MarkDeviceSyncReceivedForId(ownerDeviceId, receivedAt);
        if (!st || !st.wires)
            return;

        int receivedWireIndex;
        for (receivedWireIndex = 0; receivedWireIndex < st.wires.Count(); receivedWireIndex = receivedWireIndex + 1)
        {
            LFPG_WireData receivedWire = st.wires[receivedWireIndex];
            if (receivedWire && receivedWire.m_TargetDeviceId != "")
            {
                MarkDeviceSyncReceivedForId(receivedWire.m_TargetDeviceId, receivedAt);
            }
        }
    }

    protected void UpsertOwnerBlobInternal(string ownerDeviceId, int low, int high, string json, int snapshotGeneration)
    {
        if (ownerDeviceId == "")
            return;

		LFPG_OwnerWireState currentState;
		m_ByOwnerId.Find(ownerDeviceId, currentState);
		if (currentState && currentState.wireGeneration >= 0)
		{
			if (snapshotGeneration < currentState.wireGeneration)
				return;
		}
		if (snapshotGeneration >= 0 && snapshotGeneration < ResolveSnapshotGeneration(ownerDeviceId))
			return;

        m_NegCache.Remove(ownerDeviceId);

        if (LFPG_LOG_LEVEL >= 2)
        {
            string uobMsg = "[CableRenderer] UpsertOwnerBlob owner=" + ownerDeviceId + " net=" + low.ToString() + ":" + high.ToString() + " jsonLen=" + json.Length().ToString();
            LFPG_Util.Debug(uobMsg);
        }
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

        if (!st.wires || st.lastJson != json)
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
            if (snapshotGeneration >= 0)
            {
				st.wireGeneration = snapshotGeneration;
				AnnounceWireGeneration(ownerDeviceId, snapshotGeneration);
            }

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
            st.lastOverloaded = false;
            st.lastLoadRatio    = 0.0;

            // Topology changed: destroy old segments + clear retries for this owner
            DestroyOwnerLines(ownerDeviceId);
            ClearOwnerRetries(ownerDeviceId);
			UpdateOwnerConnCache(st);
			MarkDeviceSyncBlobReceived(ownerDeviceId, st);

            // Immediately build wire segments (frozen geometry)
            BuildOwnerWires(ownerDeviceId);

            int wireCount = 0;
            if (st.wires)
            {
                wireCount = st.wires.Count();
            }

            // Retain an authoritative empty owner state so generation checks
            // can prove that a valid no-wire snapshot is up to date.

            if (LFPG_LOG_LEVEL >= 2)
            {
                string bltMsg = "[CableRenderer] Built owner=" + ownerDeviceId + " wires=" + wireCount.ToString() + " pending=" + m_RetryQueue.Count().ToString();
                LFPG_Util.Debug(bltMsg);
            }
        }
        else
        {
            if (snapshotGeneration >= 0)
            {
				st.wireGeneration = snapshotGeneration;
				AnnounceWireGeneration(ownerDeviceId, snapshotGeneration);
            }
            MarkDeviceSyncBlobReceived(ownerDeviceId, st);
            if (LFPG_LOG_LEVEL >= 2)
            {
                string skipMsg = "[CableRenderer] UpsertOwnerBlob SKIP (json unchanged) owner=" + ownerDeviceId;
                LFPG_Util.Debug(skipMsg);
            }
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

    // HasOwnerData is map membership only. The owner entry is created
    // before DecodeOwner runs, so membership does not imply decoded
    // topology. True only when that owner's wires array is non-null.
    bool HasDecodedOwnerData(string ownerDeviceId)
    {
        if (ownerDeviceId == "") return false;

        ref LFPG_OwnerWireState st;
        if (m_ByOwnerId.Find(ownerDeviceId, st) && st && st.wires)
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
    // v0.9.1 (H5): Enriched to also update per-wire cableState immediately.
    // Previously only st.lastPowered/masks were updated, but individual
    // LFPG_WireSegmentInfo.cableState was only set in CullTick (2s delay).
    // DrawFrame reads info.cableState for color, causing 0-2s of stale
    // cable colors after JIP or power state changes.
    void NotifyOwnerVisualChanged(string ownerDeviceId)
    {
        if (ownerDeviceId == "") return;

        ref LFPG_OwnerWireState st;
        if (!m_ByOwnerId.Find(ownerDeviceId, st) || !st) return;

        EntityAI ownerObj = EntityAI.Cast(g_Game.GetObjectByNetworkId(st.ownerLow, st.ownerHigh));
        if (!ownerObj) return;

        st.lastPowered      = IsOwnerActive(ownerObj);
        st.lastLoadRatio     = LFPG_DeviceAPI.GetLoadRatio(ownerObj);
        st.lastOverloaded = LFPG_DeviceAPI.GetOverloaded(ownerObj);

        // v0.9.1 (H5): Update per-wire cableState immediately.
        // Eliminates 0-2s color delay on JIP and power state transitions.
        if (st.cachedWireKeys)
        {
            int nvcWi;
            for (nvcWi = 0; nvcWi < st.cachedWireKeys.Count(); nvcWi = nvcWi + 1)
            {
                string nvcKey = st.cachedWireKeys[nvcWi];
                LFPG_WireSegmentInfo nvcInfo;
                if (m_WireSegments.Find(nvcKey, nvcInfo) && nvcInfo)
                {
                    // v1.0: Binary overload — all wires same state.
                    if (st.lastOverloaded)
                    {
                        nvcInfo.cableState = LFPG_CableState.CRITICAL_LOAD;
                    }
                    else if (st.lastPowered)
                    {
                        nvcInfo.cableState = LFPG_CableState.POWERED;
                    }
                    else
                    {
                        nvcInfo.cableState = LFPG_CableState.IDLE;
                    }

                    nvcInfo.powered = st.lastPowered;
                }
            }
        }

        if (LFPG_LOG_LEVEL >= 2)
        {
            string nvcMsg = "[CableRenderer] NotifyOwnerVisualChanged owner=" + ownerDeviceId;
            nvcMsg = nvcMsg + " powered=" + st.lastPowered.ToString();
            nvcMsg = nvcMsg + " load=" + st.lastLoadRatio.ToString();
            nvcMsg = nvcMsg + " overloaded=" + st.lastOverloaded.ToString();
            LFPG_Util.Debug(nvcMsg);
        }
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
        if (deviceId == "" || !g_Game)
            return;

        LFPG_WireOwnerBase wireOwner = LFPG_WireOwnerBase.Cast(device);
        if (wireOwner)
        {
			AnnounceWireGeneration(deviceId, wireOwner.LFPG_GetWireGeneration());
        }

        if (!NeedsDeviceSync(deviceId))
            return;

        int netLow = 0;
        int netHigh = 0;
        if (device)
        {
            device.GetNetworkID(netLow, netHigh);
        }
        QueueDeviceSync(deviceId, netLow, netHigh, false, false);
    }

    protected bool NeedsDeviceSync(string deviceId)
    {
        int announcedGeneration = -1;
        if (m_AnnouncedWireGenerations.Find(deviceId, announcedGeneration))
        {
            ref LFPG_OwnerWireState ownerState;
            if (!m_ByOwnerId.Find(deviceId, ownerState) || !ownerState)
                return true;
            if (ownerState.wireGeneration < announcedGeneration)
                return true;
            return false;
        }

        if (m_KnownCableDevices.Contains(deviceId))
            return false;
        return true;
    }

    protected void QueueDeviceSync(string deviceId, int netLow, int netHigh, bool forced, bool retryRequest)
    {
        if (deviceId == "" || !g_Game)
            return;

		int inFlightGeneration = -1;
		if (m_DeviceSyncRetryDue.Contains(deviceId) && m_DeviceSyncInFlightGeneration.Find(deviceId, inFlightGeneration))
		{
			if (ResolveSnapshotGeneration(deviceId) <= inFlightGeneration)
				return;
		}

        if (!s_DeviceSyncCooldowns)
        {
            s_DeviceSyncCooldowns = new map<string, float>;
        }

        float now = g_Game.GetTickTime();
        float lastReq = 0.0;
        if (!forced && s_DeviceSyncCooldowns.Find(deviceId, lastReq))
        {
            if ((now - lastReq) < LFPG_DEVICE_SYNC_COOLDOWN_S)
                return;
        }

        m_PendingDeviceSyncLow[deviceId] = netLow;
        m_PendingDeviceSyncHigh[deviceId] = netHigh;
        if (forced)
        {
            m_PendingDeviceSyncForced[deviceId] = true;
        }
        else if (!m_PendingDeviceSyncForced.Contains(deviceId))
        {
            m_PendingDeviceSyncForced[deviceId] = false;
        }
        if (retryRequest)
        {
            m_PendingDeviceSyncRetry[deviceId] = true;
        }
        else if (!m_PendingDeviceSyncRetry.Contains(deviceId))
        {
            m_PendingDeviceSyncRetry[deviceId] = false;
        }

        if (!m_DeviceSyncFlushQueued)
        {
            m_DeviceSyncFlushQueued = true;
            g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(FlushDeviceSyncBatch, LFPG_DEVICE_SYNC_BATCH_DEBOUNCE_MS, false);
        }
    }

    protected void RequestOwnerSnapshot(string ownerDeviceId, int low, int high, int generation)
    {
        if (ownerDeviceId == "")
            return;
		AnnounceWireGeneration(ownerDeviceId, generation);
        QueueDeviceSync(ownerDeviceId, low, high, true, false);
    }

    protected void FlushDeviceSyncBatch()
    {
        m_DeviceSyncFlushQueued = false;
        if (m_PendingDeviceSyncLow.Count() == 0)
            return;

        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player)
            return;

        float now = g_Game.GetTickTime();
        m_DeviceSyncBatchIds.Clear();
        m_DeviceSyncDropIds.Clear();

        int i;
        for (i = 0; i < m_PendingDeviceSyncLow.Count(); i = i + 1)
        {
            string deviceId = m_PendingDeviceSyncLow.GetKey(i);
            bool forced = false;
            m_PendingDeviceSyncForced.Find(deviceId, forced);
            bool retryRequest = false;
            m_PendingDeviceSyncRetry.Find(deviceId, retryRequest);
            if ((!forced || retryRequest) && !NeedsDeviceSync(deviceId))
            {
                m_DeviceSyncDropIds.Insert(deviceId);
                continue;
            }

            float lastReq = 0.0;
            if (!forced && s_DeviceSyncCooldowns.Find(deviceId, lastReq))
            {
                if ((now - lastReq) < LFPG_DEVICE_SYNC_COOLDOWN_S)
                {
                    m_DeviceSyncDropIds.Insert(deviceId);
                    continue;
                }
            }

            if (m_DeviceSyncBatchIds.Count() < LFPG_DEVICE_SYNC_BATCH_MAX)
            {
                m_DeviceSyncBatchIds.Insert(deviceId);
            }
        }

        for (i = 0; i < m_DeviceSyncDropIds.Count(); i = i + 1)
        {
            string dropId = m_DeviceSyncDropIds[i];
            m_PendingDeviceSyncLow.Remove(dropId);
            m_PendingDeviceSyncHigh.Remove(dropId);
            m_PendingDeviceSyncForced.Remove(dropId);
            m_PendingDeviceSyncRetry.Remove(dropId);
        }

        int batchCount = m_DeviceSyncBatchIds.Count();
        if (batchCount > 0)
        {
            ScriptRPC rpc = new ScriptRPC();
            rpc.Write((int)LFPG_RPC_SubId.REQUEST_DEVICE_SYNC_BATCH);
            rpc.Write(batchCount);

            for (i = 0; i < batchCount; i = i + 1)
            {
                string sendId = m_DeviceSyncBatchIds[i];
                int sendLow = 0;
                int sendHigh = 0;
                m_PendingDeviceSyncLow.Find(sendId, sendLow);
                m_PendingDeviceSyncHigh.Find(sendId, sendHigh);
                rpc.Write(sendLow);
                rpc.Write(sendHigh);
                rpc.Write(sendId);
            }

            rpc.Send(player, LFPG_RPC_CHANNEL, true, null);

            if (LFPG_PERFDIAG_ENABLED)
            {
                m_PerfDiagDeviceSyncBatchCount = m_PerfDiagDeviceSyncBatchCount + 1;
                for (i = 0; i < batchCount; i = i + 1)
                {
                    string perfDeviceId = m_DeviceSyncBatchIds[i];
                    string perfSync = "LFPG_PERFDIAG t=";
                    perfSync = perfSync + now.ToString();
                    perfSync = perfSync + " deviceId=";
                    perfSync = perfSync + perfDeviceId;
                    perfSync = perfSync + " resync_batch=";
                    perfSync = perfSync + m_PerfDiagDeviceSyncBatchCount.ToString();
                    perfSync = perfSync + " batch_size=";
                    perfSync = perfSync + batchCount.ToString();
                    Print(perfSync);
                }
            }

            bool scheduleResponseCheck = false;
            for (i = 0; i < batchCount; i = i + 1)
            {
                string sentId = m_DeviceSyncBatchIds[i];
                bool sentRetry = false;
                m_PendingDeviceSyncRetry.Find(sentId, sentRetry);
				int retryLow = 0;
				int retryHigh = 0;
				m_PendingDeviceSyncLow.Find(sentId, retryLow);
				m_PendingDeviceSyncHigh.Find(sentId, retryHigh);
				m_DeviceSyncRetryLow[sentId] = retryLow;
				m_DeviceSyncRetryHigh[sentId] = retryHigh;
				m_DeviceSyncRetryDue[sentId] = now + 3.0;
				m_DeviceSyncInFlightGeneration[sentId] = ResolveSnapshotGeneration(sentId);
				m_DeviceSyncRetryAttempted[sentId] = sentRetry;
				scheduleResponseCheck = true;
                m_PendingDeviceSyncLow.Remove(sentId);
                m_PendingDeviceSyncHigh.Remove(sentId);
                m_PendingDeviceSyncForced.Remove(sentId);
                m_PendingDeviceSyncRetry.Remove(sentId);
            }
            if (scheduleResponseCheck)
            {
                g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(CheckDeviceSyncBatchResponse, 3000, false);
            }
        }

        if (s_DeviceSyncCooldowns.Count() > 50)
        {
            PurgeStaleDeviceSyncCooldowns(now);
        }

        if (m_PendingDeviceSyncLow.Count() > 0 && !m_DeviceSyncFlushQueued)
        {
            m_DeviceSyncFlushQueued = true;
            g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(FlushDeviceSyncBatch, LFPG_DEVICE_SYNC_CHUNK_SPACING_MS, false);
        }
    }

    protected void CheckDeviceSyncBatchResponse()
    {
        if (!g_Game)
            return;

        float retryNow = g_Game.GetTickTime();
        m_DeviceSyncRetryIds.Clear();
        int retryIndex;
        for (retryIndex = 0; retryIndex < m_DeviceSyncRetryDue.Count(); retryIndex = retryIndex + 1)
        {
            if (m_DeviceSyncRetryDue.GetElement(retryIndex) <= retryNow)
            {
                m_DeviceSyncRetryIds.Insert(m_DeviceSyncRetryDue.GetKey(retryIndex));
            }
        }

        for (retryIndex = 0; retryIndex < m_DeviceSyncRetryIds.Count(); retryIndex = retryIndex + 1)
        {
            string retryDeviceId = m_DeviceSyncRetryIds[retryIndex];
            int retryNetLow = 0;
            int retryNetHigh = 0;
            m_DeviceSyncRetryLow.Find(retryDeviceId, retryNetLow);
            m_DeviceSyncRetryHigh.Find(retryDeviceId, retryNetHigh);
			bool retryAttempted = false;
			m_DeviceSyncRetryAttempted.Find(retryDeviceId, retryAttempted);
			m_DeviceSyncRetryAttempted.Remove(retryDeviceId);
			m_DeviceSyncInFlightGeneration.Remove(retryDeviceId);
            m_DeviceSyncRetryLow.Remove(retryDeviceId);
            m_DeviceSyncRetryHigh.Remove(retryDeviceId);
            m_DeviceSyncRetryDue.Remove(retryDeviceId);
			if (!retryAttempted && NeedsDeviceSync(retryDeviceId) && !m_PendingDeviceSyncLow.Contains(retryDeviceId))
            {
                QueueDeviceSync(retryDeviceId, retryNetLow, retryNetHigh, true, true);
            }
        }
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

    protected LFPG_WireData DecodeDeltaWire(string json)
    {
        if (json == "")
            return null;

        LFPG_PersistBlob blob = new LFPG_PersistBlob();
        string err = "";
        if (!JsonFileLoader<LFPG_PersistBlob>.LoadData(json, blob, err))
            return null;
        if (!blob || !blob.wires || blob.wires.Count() != 1)
            return null;

        LFPG_WireData wire = blob.wires[0];
        if (!wire)
            return null;
        if (wire.m_TargetDeviceId == "" || wire.m_TargetPort == "")
            return null;
        return wire;
    }

    protected int FindWireIndex(array<ref LFPG_WireData> wires, LFPG_WireData needle)
    {
        if (!wires || !needle)
            return -1;

        int i;
        for (i = 0; i < wires.Count(); i = i + 1)
        {
            LFPG_WireData current = wires[i];
            if (!current)
                continue;
            if (current.m_SourcePort != needle.m_SourcePort)
                continue;
            if (current.m_TargetDeviceId != needle.m_TargetDeviceId)
                continue;
            if (current.m_TargetPort != needle.m_TargetPort)
                continue;
            return i;
        }
        return -1;
    }

	protected void RemoveOwnerWire(LFPG_OwnerWireState st, int wireIndex)
	{
		string key = st.ownerDeviceId + "|" + wireIndex.ToString();
		DestroyWire(key);
		m_RetryQueue.Remove(key);
		int lastIndex = st.wires.Count() - 1;
		if (wireIndex != lastIndex)
		{
			string lastKey = st.ownerDeviceId + "|" + lastIndex.ToString();
			LFPG_WireSegmentInfo movedInfo;
			if (m_WireSegments.Find(lastKey, movedInfo) && movedInfo)
			{
				movedInfo.wireIndex = wireIndex;
				m_WireSegments[key] = movedInfo;
			}
			m_WireSegments.Remove(lastKey);
			LFPG_RetryEntry movedRetry;
			if (m_RetryQueue.Find(lastKey, movedRetry) && movedRetry)
			{
				movedRetry.wireIndex = wireIndex;
				m_RetryQueue[key] = movedRetry;
			}
			m_RetryQueue.Remove(lastKey);
		}
		st.wires.Remove(wireIndex);
		m_DrawOrderDirty = true;
	}

    bool ApplyOwnerDelta(string ownerDeviceId, int low, int high, int generation, array<int> operations, array<string> wireJsons)
    {
        if (ownerDeviceId == "" || !operations || !wireJsons)
            return false;

        int entryCount = operations.Count();
        if (entryCount <= 0 || entryCount > LFPG_WIRE_DELTA_MAX_ENTRIES)
            return false;
        if (wireJsons.Count() != entryCount)
            return false;

        ref LFPG_OwnerWireState st;
        if (!m_ByOwnerId.Find(ownerDeviceId, st) || !st || !st.wires)
        {
            RequestOwnerSnapshot(ownerDeviceId, low, high, generation);
            return false;
        }

        if (generation <= st.wireGeneration)
            return true;
        if (st.wireGeneration < 0 || generation != (st.wireGeneration + 1))
        {
            RequestOwnerSnapshot(ownerDeviceId, low, high, generation);
            return false;
        }

        ref array<ref LFPG_WireData> decoded = new array<ref LFPG_WireData>;
        int i;
        for (i = 0; i < entryCount; i = i + 1)
        {
            int op = operations[i];
            if (op != LFPG_WireDeltaOp.ADD && op != LFPG_WireDeltaOp.REMOVE && op != LFPG_WireDeltaOp.UPDATE)
            {
                RequestOwnerSnapshot(ownerDeviceId, low, high, generation);
                return false;
            }

            LFPG_WireData decodedWire = DecodeDeltaWire(wireJsons[i]);
            if (!decodedWire)
            {
                RequestOwnerSnapshot(ownerDeviceId, low, high, generation);
                return false;
            }
            decoded.Insert(decodedWire);
        }

        for (i = 0; i < entryCount; i = i + 1)
        {
            int applyOp = operations[i];
            LFPG_WireData applyWire = decoded[i];
            int existingIndex = FindWireIndex(st.wires, applyWire);

            if (applyOp == LFPG_WireDeltaOp.REMOVE)
            {
                if (existingIndex >= 0)
                {
					RemoveOwnerWire(st, existingIndex);
                }
            }
            else if (existingIndex >= 0)
            {
				string changedKey = ownerDeviceId + "|" + existingIndex.ToString();
				DestroyWire(changedKey);
				m_RetryQueue.Remove(changedKey);
                st.wires[existingIndex] = applyWire;
            }
            else
            {
                st.wires.Insert(applyWire);
            }

            if (applyOp != LFPG_WireDeltaOp.REMOVE && applyWire.m_TargetDeviceId != "")
            {
                m_NegCache.Remove(applyWire.m_TargetDeviceId);
            }
        }

        st.ownerLow = low;
        st.ownerHigh = high;
        st.wireGeneration = generation;
        st.lastJson = "__LFPG_DELTA__";
		AnnounceWireGeneration(ownerDeviceId, generation);
        m_NegCache.Remove(ownerDeviceId);

		UpdateOwnerConnCache(st);
		MarkDeviceSyncBlobReceived(ownerDeviceId, st);
        BuildOwnerWires(ownerDeviceId);

        if (LFPG_PERFDIAG_ENABLED)
        {
            string perfDelta = "LFPG_PERFDIAG t=";
            perfDelta = perfDelta + g_Game.GetTickTime().ToString();
            perfDelta = perfDelta + " deviceId=";
            perfDelta = perfDelta + ownerDeviceId;
            perfDelta = perfDelta + " delta_receive entries=";
            perfDelta = perfDelta + entryCount.ToString();
            perfDelta = perfDelta + " generation=";
            perfDelta = perfDelta + generation.ToString();
            perfDelta = perfDelta + " wires=";
            perfDelta = perfDelta + st.wires.Count().ToString();
            Print(perfDelta);
        }
        return true;
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
	protected void RemoveOwnerConnCache(LFPG_OwnerWireState st)
	{
		if (!st)
			return;
		int i;
		for (i = 0; i < st.m_ConnectionKeys.Count(); i = i + 1)
		{
			string key = st.m_ConnectionKeys[i];
			LFPG_ConnectionContributors entry;
			if (m_ConnCache.Find(key, entry) && entry)
			{
				entry.m_ByOwner.Remove(st.ownerDeviceId);
				if (entry.m_ByOwner.Count() == 0)
					m_ConnCache.Remove(key);
			}
		}
		st.m_ConnectionKeys.Clear();
		for (i = 0; i < st.m_CacheDevices.Count(); i = i + 1)
		{
			string deviceId = st.m_CacheDevices.GetKey(i);
			int count = 0;
			if (m_KnownCableDevices.Find(deviceId, count))
			{
				if (count <= 1)
					m_KnownCableDevices.Remove(deviceId);
				else
					m_KnownCableDevices[deviceId] = count - 1;
			}
		}
		st.m_CacheDevices.Clear();
	}

	protected void AddOwnerCacheDevice(LFPG_OwnerWireState st, string deviceId)
	{
		if (deviceId == "" || st.m_CacheDevices.Contains(deviceId))
			return;
		int count = 0;
		m_KnownCableDevices.Find(deviceId, count);
		m_KnownCableDevices[deviceId] = count + 1;
		st.m_CacheDevices[deviceId] = true;
	}

	protected void AddOwnerConnection(LFPG_OwnerWireState st, string key, string typeName)
	{
		LFPG_ConnectionContributors entry;
		if (!m_ConnCache.Find(key, entry) || !entry)
		{
			entry = new LFPG_ConnectionContributors();
			m_ConnCache[key] = entry;
		}
		if (!entry.m_ByOwner.Contains(st.ownerDeviceId))
			st.m_ConnectionKeys.Insert(key);
		entry.m_ByOwner[st.ownerDeviceId] = typeName;
	}

	protected void UpdateOwnerConnCache(LFPG_OwnerWireState st)
	{
		RemoveOwnerConnCache(st);
		if (!st || !st.wires)
			return;
		AddOwnerCacheDevice(st, st.ownerDeviceId);
		string ownerType = st.ownerDeviceId;
		EntityAI ownerObj = EntityAI.Cast(g_Game.GetObjectByNetworkId(st.ownerLow, st.ownerHigh));
		if (ownerObj)
			ownerType = ownerObj.GetType();
		int w;
		for (w = 0; w < st.wires.Count(); w = w + 1)
		{
			LFPG_WireData wd = st.wires[w];
			if (!wd)
				continue;
			AddOwnerCacheDevice(st, wd.m_TargetDeviceId);
			string srcPort = wd.m_SourcePort;
			if (srcPort == "")
				srcPort = "output_1";
			string targetType = wd.m_TargetDeviceId;
			EntityAI targetObj = ResolveDeviceEntityEx(wd.m_TargetDeviceId, wd.m_TargetNetLow, wd.m_TargetNetHigh);
			if (targetObj)
				targetType = targetObj.GetType();
			string outKey = st.ownerDeviceId + "|" + srcPort + "|" + LFPG_PortDir.OUT.ToString();
			AddOwnerConnection(st, outKey, targetType);
			string tgtPort = wd.m_TargetPort;
			if (tgtPort == "")
				tgtPort = "input_main";
			string inKey = wd.m_TargetDeviceId + "|" + tgtPort + "|" + LFPG_PortDir.IN.ToString();
			AddOwnerConnection(st, inKey, ownerType);
		}
	}

	// O(1), including ports referenced by more than one cached owner.
	string GetConnectionType(string deviceId, string portName, int dir)
	{
		string key = deviceId + "|" + portName + "|" + dir.ToString();
		LFPG_ConnectionContributors entry;
		if (m_ConnCache.Find(key, entry) && entry && entry.m_ByOwner.Count() > 0)
			return entry.m_ByOwner.GetElement(entry.m_ByOwner.Count() - 1);
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

		// Keys follow the authoritative array even when the owner is streamed out.
		st.cachedWireKeys = new array<string>;
		int wk;
		for (wk = 0; wk < st.wires.Count(); wk = wk + 1)
		{
			st.cachedWireKeys.Insert(ownerDeviceId + "|" + wk.ToString());
		}

        if (LFPG_LOG_LEVEL >= 2)
        {
            string bowMsg = "[CableRenderer] BuildOwnerWires owner=" + ownerDeviceId + " net=" + st.ownerLow.ToString() + ":" + st.ownerHigh.ToString() + " wires=" + st.wires.Count().ToString();
            LFPG_Util.Debug(bowMsg);
        }

        EntityAI ownerObj = EntityAI.Cast(g_Game.GetObjectByNetworkId(st.ownerLow, st.ownerHigh));
        if (!ownerObj)
        {
			// Owner not yet loaded: queue missing geometry without resetting resident state.
            string bowNullMsg = "[CableRenderer] BuildOwnerWires: ownerObj NULL net=" + st.ownerLow.ToString() + ":" + st.ownerHigh.ToString();
            LFPG_Util.Warn(bowNullMsg);
            if (LFPG_DIAG_ENABLED)
            {
                LFPG_Diag.ServerEcho("[CableRenderer] ownerObj NULL net=" + st.ownerLow.ToString() + ":" + st.ownerHigh.ToString());
            }
            int rw;
            for (rw = 0; rw < st.wires.Count(); rw = rw + 1)
            {
				if (!m_WireSegments.Contains(st.cachedWireKeys[rw]))
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
        st.lastOverloaded = LFPG_DeviceAPI.GetOverloaded(ownerObj);

        // G5: get render metrics once outside loop
        LFPG_RenderMetrics bldTelRnd = LFPG_Telemetry.GetRender();

        int w;
        for (w = 0; w < st.wires.Count(); w = w + 1)
        {
			string wireKey = st.cachedWireKeys[w];
			if (m_WireSegments.Contains(wireKey) || m_RetryQueue.Contains(wireKey))
				continue;

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

			vector a;
			vector b;
			PrepareWirePoints(wd, ownerObj, targetObj, a, b);

			if (!BuildWire(wireKey, m_TempPoints, st.lastPowered, a, b, wd.m_Waypoints, w))
			{
				AddRetry(ownerDeviceId, w, LFPG_RetryReason.BUDGET);
				bldTelRnd.m_WiresBudget = bldTelRnd.m_WiresBudget + 1;
			}
        }
    }

	protected void PrepareWirePoints(LFPG_WireData wd, EntityAI ownerObj, EntityAI targetObj, out vector a, out vector b)
	{
		string srcPort = wd.m_SourcePort;
		if (srcPort == "")
			srcPort = "output_1";
		a = LFPG_WorldUtil.ClampAboveSurface(LFPG_DeviceAPI.GetPortWorldPos(ownerObj, srcPort));
		b = LFPG_WorldUtil.ClampAboveSurface(LFPG_DeviceAPI.GetPortWorldPos(targetObj, wd.m_TargetPort));
		m_TempPoints.Clear();
		m_TempPoints.Insert(a);
		if (wd.m_Waypoints && wd.m_Waypoints.Count() > 0)
		{
			int j;
			for (j = 0; j < wd.m_Waypoints.Count(); j = j + 1)
				m_TempPoints.Insert(LFPG_WorldUtil.ClampAboveSurface(wd.m_Waypoints[j], LFPG_SURFACE_CLAMP_M));
		}
		else
		{
			m_TempPoints.Insert(LFPG_WorldUtil.AutoMidpointAboveTerrain(a, b));
		}
		m_TempPoints.Insert(b);
	}

	protected void ComputePointBounds(array<vector> points, out vector center, out float radius)
	{
		center = "0 0 0";
		radius = 0.0;
		if (!points || points.Count() == 0)
			return;
		int i;
		for (i = 0; i < points.Count(); i = i + 1)
			center = center + points[i];
		float invCount = 1.0 / points.Count();
		center = center * invCount;
		for (i = 0; i < points.Count(); i = i + 1)
		{
			float pointDist = vector.Distance(center, points[i]);
			if (pointDist > radius)
				radius = pointDist;
		}
	}

    // Build segments for a single wire. Geometry is frozen after creation.
    // v0.7.9: sagSubs removed — ApplyCatenaria is now self-contained.
    // waypoints: user-placed waypoints (for joint rendering at LOD close).
    // wireIdx: index of this wire in the owner's wire array (for overload mask).
	protected bool BuildWire(string wireKey, array<vector> pts, bool powered, vector posA, vector posB, array<vector> waypoints, int wireIdx)
	{

        if (pts.Count() < 2)
        {
            string ptsMsg = "[CableRenderer] BuildWire: pts < 2 for " + wireKey;
            LFPG_Util.Warn(ptsMsg);
			return false;
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
			return false;
        }

        ApplyCatenaria(pts);

        if (m_SagPoints.Count() < 2)
        {
            string sagMsg = "[CableRenderer] BuildWire: sagPoints < 2 for " + wireKey;
            LFPG_Util.Warn(sagMsg);
			return false;
        }

		// Admission uses the exact post-compaction, post-sag segment count.
		if (!ReserveWireSegments(wireKey, m_SagPoints, posA, posB))
			return false;
		DestroyWire(wireKey);

		LFPG_WireSegmentInfo info = new LFPG_WireSegmentInfo();
        info.powered = powered;
        info.cachedPosA = posA;
        info.cachedPosB = posB;

		m_BoundsPoints.Clear();
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
				if (createdOk == 0)
					m_BoundsPoints.Insert(seg.m_From);
				m_BoundsPoints.Insert(seg.m_To);
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
			return false;
        }

		// Admission uses planned points; final bounds use only successfully created segments.
		vector builtCenter;
		float builtRadius;
		ComputePointBounds(m_BoundsPoints, builtCenter, builtRadius);
		info.cachedCenter = builtCenter;
		info.cachedRadius = builtRadius;
		PlayerBase buildPlayer = PlayerBase.Cast(g_Game.GetPlayer());
		if (buildPlayer)
		{
			info.cachedMinDist = vector.Distance(buildPlayer.GetPosition(), info.cachedCenter) - info.cachedRadius;
			if (info.cachedMinDist < 0.0)
				info.cachedMinDist = 0.0;
		}

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

		// Samples need both the built segments and the clamped waypoint joints.
		info.BuildOccSamples();

        // v0.7.8: set initial cable state
        if (powered)
        {
            info.cableState = LFPG_CableState.POWERED;
        }
        else
        {
            info.cableState = LFPG_CableState.IDLE;
        }

        // Wire index: used for occlusion stagger group and wire key
        info.wireIndex = wireIdx;
        // v0.7.38 (H6): Stable stagger group from wireIndex.
        info.occStaggerGroup = wireIdx % 3;

        m_WireSegments[wireKey] = info;
        m_DrawOrderDirty = true;

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
		return true;
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
    //   6. Compute cachedMinDist for LOD/alpha (v0.7.7)
    protected void CullTick()
    {
        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player) return;

        vector pp = player.GetPosition();
        float bubbleM = m_DeviceBubbleM;

        // v0.7.9: prepare deferred removal list for ghost owners
        m_TempKeys.Clear();

        // v0.7.38 (C3): Guard entire log block with DIAG check.
        // Prevents 5+ string concatenations per CullTick when diagnostics off.
        int debugTick = g_Game.GetTime();
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
        float bubbleSq = bubbleM * bubbleM;

        int i;
        for (i = 0; i < m_ByOwnerId.Count(); i = i + 1)
        {
            LFPG_OwnerWireState st = m_ByOwnerId.GetElement(i);
            if (!st || !st.wires) continue;

            // Update powered state (only if entity is available)
            EntityAI ownerObj = EntityAI.Cast(g_Game.GetObjectByNetworkId(st.ownerLow, st.ownerHigh));
            if (ownerObj)
            {
                // Owner entity is valid: reset null counter
                st.nullOwnerTicks = 0;

                st.lastPowered = IsOwnerActive(ownerObj);

                // v0.7.8: read synced load ratio from source
                st.lastLoadRatio = LFPG_DeviceAPI.GetLoadRatio(ownerObj);

                // v0.7.8: read overload bitmask from owner
                st.lastOverloaded = LFPG_DeviceAPI.GetOverloaded(ownerObj);

                // v0.7.35 (F1.3): read warning bitmask from owner

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
						ReleaseWireSegments(hwInfo);
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
							float missingOwnerDist = vector.Distance(pp, twInfo.cachedCenter) - twInfo.cachedRadius;
							if (missingOwnerDist < 0.0)
								missingOwnerDist = 0.0;
							twInfo.cachedMinDist = missingOwnerDist;
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
				{
					AddRetry(st.ownerDeviceId, w, LFPG_RetryReason.BUDGET);
					continue;
				}

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
				if (!shouldBeVisible)
					ReleaseWireSegments(info);
				else if (info.segments.Count() == 0)
					AddRetry(st.ownerDeviceId, w, LFPG_RetryReason.BUDGET);

                // Update powered flag
                info.powered = st.lastPowered;

                // v1.0: Binary overload — all wires same state.
                if (st.lastOverloaded)
                {
                    info.cableState = LFPG_CableState.CRITICAL_LOAD;
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
                if (LFPG_LOG_LEVEL >= 2)
                {
                    string ghostMsg = "[CableRenderer] CullTick: removing ghost owner=" + ghostId + " (entity null for 30+s)";
                    LFPG_Util.Debug(ghostMsg);
                }
                DestroyOwnerLines(ghostId);
                ClearOwnerRetries(ghostId);
				LFPG_OwnerWireState ghostState;
				if (m_ByOwnerId.Find(ghostId, ghostState))
					RemoveOwnerConnCache(ghostState);
                m_ByOwnerId.Remove(ghostId);
            }
        }

        // Distances were just recomputed → draw order must be re-sorted.
        m_DrawOrderDirty = true;
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
        // cableState is only ever assigned IDLE / POWERED / CRITICAL_LOAD (verified against
        // every `cableState =` writer in this file). The other LFPG_CableState values never
        // reach here, so their branches were dead and are omitted. If a new state is ever
        // assigned to cableState, add its branch back.
        if (state == LFPG_CableState.POWERED)
            return LFPG_STATE_COLOR_POWERED;

        if (state == LFPG_CableState.CRITICAL_LOAD)
            return LFPG_STATE_COLOR_CRITICAL;

        // Default: IDLE
        return LFPG_STATE_COLOR_IDLE;
    }

    // v0.7.38 (H1): Cohen-Sutherland moved to LFPG_WorldUtil.ClipSegToScreen
    // and LFPG_WorldUtil.ComputeOutcode (shared with WiringClient).

    // Rebuild the far-to-near painter's-sort order (indices into
    // m_WireSegments sorted by cachedMinDist descending). Inputs change only
    // at CullTick (2s) or on wire add/remove, so DrawFrame invokes this via
    // the m_DrawOrderDirty gate instead of re-sorting every frame.
    protected void RebuildDrawOrder()
    {
        m_DrawOrder.Clear();
        m_DrawDist.Clear();
        int wc = m_WireSegments.Count();
        int si;
        for (si = 0; si < wc; si = si + 1)
        {
            ref LFPG_WireSegmentInfo sortWsi = m_WireSegments.GetElement(si);
			if (!sortWsi || !sortWsi.visible || sortWsi.segments.Count() == 0)
				continue;

            m_DrawOrder.Insert(si);
            m_DrawDist.Insert(sortWsi.cachedMinDist);
        }

		// Min-heap extraction leaves descending distances in place: O(n log n).
		int count = m_DrawOrder.Count();
		int root;
		for (root = count / 2 - 1; root >= 0; root = root - 1)
			SiftDrawHeap(root, count);
		int end;
		for (end = count - 1; end > 0; end = end - 1)
		{
			SwapDrawEntries(0, end);
			SiftDrawHeap(0, end);
		}
    }

	protected void SwapDrawEntries(int a, int b)
	{
		int index = m_DrawOrder[a];
		m_DrawOrder[a] = m_DrawOrder[b];
		m_DrawOrder[b] = index;
		float distance = m_DrawDist[a];
		m_DrawDist[a] = m_DrawDist[b];
		m_DrawDist[b] = distance;
	}

	protected void SiftDrawHeap(int root, int count)
	{
		int child = root * 2 + 1;
		while (child < count)
		{
			if (child + 1 < count && m_DrawDist[child + 1] < m_DrawDist[child])
				child = child + 1;
			if (m_DrawDist[root] <= m_DrawDist[child])
				return;
			SwapDrawEntries(root, child);
			root = child;
			child = root * 2 + 1;
		}
	}

	// CGame exposes active position/direction, but no complete active-camera FOV/roll getter.
	// Sample its actual projection instead, including scripted cameras and optical zoom.
	// Three world-axis offsets ensure that at least two probes are off the viewing axis.
	protected void UpdateProjectionRevision(vector camPos, vector camDir)
	{
		vector probeBase = camPos + camDir * 10.0;
		vector probeX = g_Game.GetScreenPos(probeBase + "1 0 0");
		vector probeY = g_Game.GetScreenPos(probeBase + "0 1 0");
		vector probeZ = g_Game.GetScreenPos(probeBase + "0 0 1");
		bool changed = LFPG_WorldUtil.DistSq(probeX, m_ProjectionProbeX) > 0.0;
		changed = changed || LFPG_WorldUtil.DistSq(probeY, m_ProjectionProbeY) > 0.0;
		changed = changed || LFPG_WorldUtil.DistSq(probeZ, m_ProjectionProbeZ) > 0.0;
		if (changed)
		{
			m_ProjectionRevision = m_ProjectionRevision + 1;
			m_ProjectionProbeX = probeX;
			m_ProjectionProbeY = probeY;
			m_ProjectionProbeZ = probeZ;
		}
	}

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

		if (!HasRenderableWires())
			return;

        // v0.7.13 (G5): Render telemetry — grab reference once per frame
        LFPG_RenderMetrics tRnd = LFPG_Telemetry.GetRender();

        vector camPos = g_Game.GetCurrentCameraPosition();
        vector camDir = g_Game.GetCurrentCameraDirection();
        float nowMs = g_Game.GetTime();

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

        // Quantize sway inputs only when camera is still so Phase-1 cache can hit;
        // while moving, keep continuous nowMs (do not force 20 Hz for all frames).
        float swayNowMs = nowMs;
        if (!m_CamMoved)
        {
            swayNowMs = Math.Floor(nowMs / LFPG_SWAY_CACHE_CADENCE_MS) * LFPG_SWAY_CACHE_CADENCE_MS;
        }

        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());

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

        // v4.5: Server option to hide cables entirely without tools.
        // s_ServerHideCablesNoReel is set via RPC on JIP (SYNC_SERVER_SETTINGS).
        // Check once per frame. Early return skips all drawing but
		// Camera displacement continues accumulating until an occlusion tick consumes it.
        if (!showStateColors && s_ServerHideCablesNoReel)
        {
            return;
        }

        // v4.5: Width multiplier for no-reel mode (pre-computed once per frame).
        float noReelMult = 1.0;
        if (!showStateColors)
        {
            noReelMult = LFPG_NO_REEL_WIDTH_MULT;
        }

        float fadeRange = LFPG_CULL_DISTANCE_M - LFPG_ALPHA_FADE_START_M;
        bool doAlphaFade = (fadeRange > 0.1);

        // v0.7.9: reuse CableHUD's cached screen dimensions (set in BeginFrame)
        // instead of calling GetScreenSize again per frame.
        float swF = hud.GetScreenW();
        float shF = hud.GetScreenH();
		UpdateProjectionRevision(camPos, camDir);
		tRnd.m_Projections = tRnd.m_Projections + 3;

        // v0.7.38 (H3): Proportional ultra-LOD margin.
        // Fixed 200px was too large at 720p and too small at 4K.
        float ulMargin = shF * LFPG_ULTRA_LOD_MARGIN_RATIO;

        // v0.8.x: Degenerate projection limits (precomputed once per frame).
        // Used in Phase 1 to mark extreme GetScreenPos results as behind-camera,
        // so Phase 2's behindA/behindB check skips them naturally.
        float degLimX = swF * LFPG_SCREEN_DEGENERATE_MULT;
        float degLimY = shF * LFPG_SCREEN_DEGENERATE_MULT;

        // ---- v0.7.38: Player screen-space occlusion ----
        // In 3rd-person view, compute a screen-space bounding rect around
        // the player model. Segments farther from camera than the player
        // that intersect this rect get alpha-faded so cables don't overdraw
        // the character. C4: skip 2x GetScreenPos when cam+player are steady.
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
                bool reusePlOcc = false;
                if (m_PlOccCacheValid && m_PlOccProjectionRevision == m_ProjectionRevision)
                {
                    if (LFPG_WorldUtil.DistSq(plPos, m_PlOccLastPlPos) <= 0.0)
                    {
                        reusePlOcc = true;
                    }
                }

                if (reusePlOcc)
                {
                    plOccActive = m_PlOccCachedActive;
                    plRectX1 = m_PlOccRectX1;
                    plRectY1 = m_PlOccRectY1;
                    plRectX2 = m_PlOccRectX2;
                    plRectY2 = m_PlOccRectY2;
                    plDepthThreshold = m_PlOccDepthThreshold;
                }
                else
                {
                    vector plHead = plPos;
                    plHead[1] = plHead[1] + LFPG_PLOCC_HEAD_OFFSET_Y;

                    vector plFeetScr = g_Game.GetScreenPos(plPos);
                    vector plHeadScr = g_Game.GetScreenPos(plHead);

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

                    m_PlOccLastPlPos = plPos;
                    m_PlOccProjectionRevision = m_ProjectionRevision;
                    m_PlOccCacheValid = true;
                    m_PlOccCachedActive = plOccActive;
                    m_PlOccRectX1 = plRectX1;
                    m_PlOccRectY1 = plRectY1;
                    m_PlOccRectX2 = plRectX2;
                    m_PlOccRectY2 = plRectY2;
                    m_PlOccDepthThreshold = plDepthThreshold;
                }
            }
            else
            {
                m_PlOccCacheValid = false;
            }
        }
        else
        {
            m_PlOccCacheValid = false;
        }

        // v0.7.38 (C1) / vX (perf): far-to-near painter's order. The sort
        // inputs (cachedMinDist, wire membership) change only at CullTick (2s)
        // or on wire add/remove, so the cached order is rebuilt only when
        // m_DrawOrderDirty instead of every frame.
        if (m_DrawOrderDirty)
        {
            RebuildDrawOrder();
            m_DrawOrderDirty = false;
        }

        UpdateOcclusionBudget(camPos, camDir, player, nowMs);

		// C2: one near-to-far preparation pass, then far-to-near drawing.
		// Occluded/behind wires stay in the recheck population, never in this frame's draws.
		m_FrameDrawOrder.Clear();
		int decoratorBudgetRemaining = LFPG_CABLE_DECORATOR_BUDGET;
		int di;
		for (di = m_DrawOrder.Count() - 1; di >= 0; di = di - 1)
		{
			int candidateIndex = m_DrawOrder[di];
			LFPG_WireSegmentInfo candidate = m_WireSegments.GetElement(candidateIndex);
			if (!candidate)
				continue;
			candidate.decoratorAllowance = 0;
			tRnd.m_WiresTotal = tRnd.m_WiresTotal + 1;
			float dcx = candidate.cachedCenter[0] - camPos[0];
			float dcy = candidate.cachedCenter[1] - camPos[1];
			float dcz = candidate.cachedCenter[2] - camPos[2];
			float dot = dcx * camDir[0] + dcy * camDir[1] + dcz * camDir[2];
			if (!candidate.visible || dot + candidate.cachedRadius < 0.0)
			{
				tRnd.m_WiresCulled = tRnd.m_WiresCulled + 1;
				continue;
			}
			if (candidate.occluded)
			{
				tRnd.m_WiresOccluded = tRnd.m_WiresOccluded + 1;
				continue;
			}
			if (candidate.segments.Count() == 0)
				continue;
			m_FrameDrawOrder.Insert(candidateIndex);
			if (candidate.cachedMinDist < LFPG_LOD_CLOSE_M && decoratorBudgetRemaining > 0)
			{
				int requestedDecorators = 2;
				if (candidate.cachedJoints)
					requestedDecorators = requestedDecorators + candidate.cachedJoints.Count();
				if (requestedDecorators > decoratorBudgetRemaining)
					requestedDecorators = decoratorBudgetRemaining;
				candidate.decoratorAllowance = requestedDecorators;
				decoratorBudgetRemaining = decoratorBudgetRemaining - requestedDecorators;
			}
		}

		for (di = m_FrameDrawOrder.Count() - 1; di >= 0; di = di - 1)
		{
			int i = m_FrameDrawOrder[di];
			LFPG_WireSegmentInfo wsi = m_WireSegments.GetElement(i);
			if (!wsi)
				continue;
			int decoratorBudget = wsi.decoratorAllowance;
			int segCount = wsi.segments.Count();

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

            // ---- v0.7.38: Player-occluded color variants ----
            // Pre-compute once per wire so per-segment test is a simple select.
            // Derive from drawColor, which already
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
            if (wirePlOcc)
            {
                plOccDrawColor = ApplyAlpha(drawColor, LFPG_PLOCC_ALPHA);
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
                // v4.5: Pre-compute ultra-LOD width with no-reel multiplier.
                float ulWidthBase = LFPG_DEPTH_WIDTH_MIN * noReelMult;

				bool reuseUltraProjection = (wsi.ultraCacheValid && wsi.m_UltraProjectionRevision == m_ProjectionRevision && wsi.ultraCacheCamPos[0] == camPos[0] && wsi.ultraCacheCamPos[1] == camPos[1] && wsi.ultraCacheCamPos[2] == camPos[2] && wsi.ultraCacheCamDir[0] == camDir[0] && wsi.ultraCacheCamDir[1] == camDir[1] && wsi.ultraCacheCamDir[2] == camDir[2] && wsi.ultraCacheViewportW == swF && wsi.ultraCacheViewportH == shF);
                if (!reuseUltraProjection)
                {
                    wsi.ultraScreenA = g_Game.GetScreenPos(wsi.cachedPosA);
                    wsi.ultraScreenB = g_Game.GetScreenPos(wsi.cachedPosB);
                    wsi.ultraCacheCamPos = camPos;
                    wsi.ultraCacheCamDir = camDir;
                    wsi.ultraCacheViewportW = swF;
                    wsi.ultraCacheViewportH = shF;
					wsi.m_UltraProjectionRevision = m_ProjectionRevision;
                    wsi.ultraCacheValid = true;
                }
                vector ulA = wsi.ultraScreenA;
                vector ulB = wsi.ultraScreenB;

                bool ulBehindA = (ulA[2] < LFPG_BEHIND_CAM_Z);
                bool ulBehindB = (ulB[2] < LFPG_BEHIND_CAM_Z);

                // Both behind camera: nothing visible, skip.
                if (ulBehindA && ulBehindB)
                {
                    tRnd.m_WiresCulled = tRnd.m_WiresCulled + 1;
                    continue;
                }

                // v0.8.x: One endpoint behind camera — 3D near-plane clip.
                // Instead of skipping the entire wire (which causes brief
                // vanishing during rotation), clip to the camera plane and
                // project the clipped point. ClipBehindCamera uses the
                // parametric intersection with the near plane:
                //   t = (nearClip - dot(behind-cam, camDir)) / dot(vis-behind, camDir)
                // Safety: CableHUD degenerate guard catches extreme results.
                if (ulBehindA)
                {
                    ulA = LFPG_WorldUtil.ClipBehindCamera(wsi.cachedPosA, wsi.cachedPosB, camPos, camDir);
                }
                if (ulBehindB)
                {
                    ulB = LFPG_WorldUtil.ClipBehindCamera(wsi.cachedPosB, wsi.cachedPosA, camPos, camDir);
                }

                // v0.8.x: Degenerate projection guard for ultra-LOD.
                // ClipBehindCamera or original projection may produce extreme
                // screen coords near the frustum edge. Reject if either
                // endpoint exceeds 3× screen dimensions.
                float absUlAx = ulA[0];
                if (absUlAx < 0.0)
                {
                    absUlAx = -absUlAx;
                }
                float absUlAy = ulA[1];
                if (absUlAy < 0.0)
                {
                    absUlAy = -absUlAy;
                }
                float absUlBx = ulB[0];
                if (absUlBx < 0.0)
                {
                    absUlBx = -absUlBx;
                }
                float absUlBy = ulB[1];
                if (absUlBy < 0.0)
                {
                    absUlBy = -absUlBy;
                }
                if (absUlAx > degLimX || absUlAy > degLimY || absUlBx > degLimX || absUlBy > degLimY)
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
                    // FIX: C-S clips to actual viewport, not expanded rect.
                    // ulMargin only used in fast-path above.
                    float ulMinX = 0.0;
                    float ulMinY = 0.0;
                    float ulMaxX = swF;
                    float ulMaxY = shF;
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

                // v0.8.x: Edge fade for ultra-LOD.
                float ulEdgeFade = 1.0;
                if (ulOutA || ulOutB)
                {
                    ulEdgeFade = LFPG_WorldUtil.ComputeEdgeFade(ulx1, uly1, ulx2, uly2, swF, shF, LFPG_EDGE_FADE_PX);
                    if (ulEdgeFade < 0.01)
                    {
                        tRnd.m_WiresCulled = tRnd.m_WiresCulled + 1;
                        continue;
                    }
                }
                int ulDraw = drawColor;
                int ulPlDraw = plOccDrawColor;
                if (ulEdgeFade < 0.99)
                {
                    ulDraw = ApplyAlpha(drawColor, ulEdgeFade);
                    ulPlDraw = ApplyAlpha(plOccDrawColor, ulEdgeFade);
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
                                hud.DrawLineScreen(ulx1, uly1, ulPcX1, ulPcY1, ulWidthBase, ulDraw);
                            }
                            // Tramo 2: Pin → Pout (faded behind player)
                            hud.DrawLineScreen(ulPcX1, ulPcY1, ulPcX2, ulPcY2, ulWidthBase, ulPlDraw);
                            // Tramo 3: Pout → B (opaque, skip if degenerate)
                            float uld3 = (ulx2 - ulPcX2) * (ulx2 - ulPcX2) + (uly2 - ulPcY2) * (uly2 - ulPcY2);
                            if (uld3 > 1.0)
                            {
                                hud.DrawLineScreen(ulPcX2, ulPcY2, ulx2, uly2, ulWidthBase, ulDraw);
                            }

                            tRnd.m_WiresDrawn = tRnd.m_WiresDrawn + 1;
                            tRnd.m_SegmentsDrawn = tRnd.m_SegmentsDrawn + 1;
                            continue;
                        }
                    }
                }
                hud.DrawLineScreen(ulx1, uly1, ulx2, uly2, ulWidthBase, ulDraw);

                tRnd.m_WiresDrawn = tRnd.m_WiresDrawn + 1;
                tRnd.m_SegmentsDrawn = tRnd.m_SegmentsDrawn + 1;
                continue;  // Skip Phase 1/2/3 entirely
            }

            // ================================================
            // Phase 1: reuse projected points while camera and sway are unchanged.
            // Still camera: sway quantized on LFPG_SWAY_CACHE_CADENCE_MS so inputs can match.
            // ================================================
            float swayOff = 0.0;
            float swayOffX = 0.0;
            float swayScale = 1.0 - (wireDist * LFPG_SWAY_ATTEN_INV);
            if (swayScale > 0.0)
            {
                float swayPhase = wsi.cachedPosA[0] * LFPG_SWAY_HASH_X + wsi.cachedPosA[2] * LFPG_SWAY_HASH_Z;
                swayOff = Math.Sin(swayNowMs * LFPG_SWAY_SPEED + swayPhase) * LFPG_SWAY_AMPLITUDE * swayScale;
                swayOffX = Math.Sin(swayNowMs * LFPG_SWAY_X_SPEED + swayPhase + LFPG_SWAY_X_PHASE_OFS) * LFPG_SWAY_X_AMPLITUDE * swayScale;
            }
            float swayDenom = segCount + 1.0;

            LFPG_CableParticle firstSeg = wsi.segments[0];
            if (!firstSeg)
                continue;

			bool reuseScreenProjection = (wsi.screenCacheValid && wsi.m_ScreenProjectionRevision == m_ProjectionRevision && wsi.cachedScreenPts.Count() == segCount + 1 && wsi.screenCacheCamPos[0] == camPos[0] && wsi.screenCacheCamPos[1] == camPos[1] && wsi.screenCacheCamPos[2] == camPos[2] && wsi.screenCacheCamDir[0] == camDir[0] && wsi.screenCacheCamDir[1] == camDir[1] && wsi.screenCacheCamDir[2] == camDir[2] && wsi.screenCacheSwayY == swayOff && wsi.screenCacheSwayX == swayOffX && wsi.screenCacheViewportW == swF && wsi.screenCacheViewportH == shF);
            int s;
            if (!reuseScreenProjection)
            {
                wsi.cachedScreenPts.Clear();

                vector firstScr = g_Game.GetScreenPos(firstSeg.m_From);
                if (firstScr[2] > LFPG_BEHIND_CAM_Z)
                {
                    float absFX = firstScr[0];
                    if (absFX < 0.0)
                    {
                        absFX = -absFX;
                    }
                    float absFY = firstScr[1];
                    if (absFY < 0.0)
                    {
                        absFY = -absFY;
                    }
                    if (absFX > degLimX || absFY > degLimY)
                    {
                        firstScr[2] = 0.0;
                    }
                }
                wsi.cachedScreenPts.Insert(firstScr);

                for (s = 0; s < segCount; s = s + 1)
                {
                    LFPG_CableParticle seg = wsi.segments[s];
                    if (!seg)
                    {
                        wsi.cachedScreenPts.Insert(wsi.cachedScreenPts[wsi.cachedScreenPts.Count() - 1]);
                        continue;
                    }

                    vector wp = seg.m_To;
                    bool isLastPoint = (s == segCount - 1);
                    if (!isLastPoint && swayScale > 0.0)
                    {
                        float swayT = (s + 1.0) / swayDenom;
                        float swayW = 4.0 * swayT * (1.0 - swayT);
                        wp[1] = wp[1] + swayOff * swayW;
                        wp[0] = wp[0] + swayOffX * swayW;
                    }

                    vector segScr = g_Game.GetScreenPos(wp);
                    if (segScr[2] > LFPG_BEHIND_CAM_Z)
                    {
                        float absWX = segScr[0];
                        if (absWX < 0.0)
                        {
                            absWX = -absWX;
                        }
                        float absWY = segScr[1];
                        if (absWY < 0.0)
                        {
                            absWY = -absWY;
                        }
                        if (absWX > degLimX || absWY > degLimY)
                        {
                            segScr[2] = 0.0;
                        }
                    }
                    wsi.cachedScreenPts.Insert(segScr);
                }

                wsi.screenCacheCamPos = camPos;
                wsi.screenCacheCamDir = camDir;
                wsi.screenCacheSwayY = swayOff;
                wsi.screenCacheSwayX = swayOffX;
                wsi.screenCacheViewportW = swF;
                wsi.screenCacheViewportH = shF;
				wsi.m_ScreenProjectionRevision = m_ProjectionRevision;
                wsi.screenCacheValid = true;
            }

            // ================================================
            // Phase 2: Draw sub-segments using cached projections.
            // Behind-camera and off-screen resolved once per seg.
            // ================================================
            for (s = 0; s < segCount; s = s + 1)
            {
                vector sA = wsi.cachedScreenPts[s];
                vector sB = wsi.cachedScreenPts[s + 1];

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
                    // FIX: C-S clips to ACTUAL viewport (0,0,swF,shF).
                    // Margin is only used in the fast-path above to decide
                    // whether to invoke C-S. Output coords are always in
                    // [0,swF]×[0,shF] — no negative values for Canvas.
                    float csMinX = 0.0;
                    float csMinY = 0.0;
                    float csMaxX = swF;
                    float csMaxY = shF;
                    bool segVisible = LFPG_WorldUtil.ClipSegToScreen(sx1, sy1, sx2, sy2, csMinX, csMinY, csMaxX, csMaxY, m_ClipA, m_ClipB);
                    if (!segVisible)
                        continue;

                    sx1 = m_ClipA[0];
                    sy1 = m_ClipA[1];
                    sx2 = m_ClipB[0];
                    sy2 = m_ClipB[1];
                }

                // v0.8.x: Edge fade — smooth alpha falloff near screen edges.
                // Only computed for clipped segments (outsideA || outsideB).
                // Segments fully on-screen skip this entirely (edgeFade stays 1.0).
                float edgeFade = 1.0;
                if (outsideA || outsideB)
                {
                    edgeFade = LFPG_WorldUtil.ComputeEdgeFade(sx1, sy1, sx2, sy2, swF, shF, LFPG_EDGE_FADE_PX);
                    if (edgeFade < 0.01)
                        continue;
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

                // v4.5: Thinner cables when not holding tools.
                depthWidth = depthWidth * noReelMult;

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

                // v0.8.x: Per-segment edge-faded base colors.
                int segDraw = drawColor;
                int segPlDraw = plOccDrawColor;
                if (edgeFade < 0.99)
                {
                    segDraw = ApplyAlpha(drawColor, edgeFade);
                    if (wirePlOcc)
                    {
                        segPlDraw = ApplyAlpha(plOccDrawColor, edgeFade);
                    }
                }

                // ---- Single-pass base drawing ----
                if (segPlOcc)
                {
                    float d1sq = (plClX1 - sx1) * (plClX1 - sx1) + (plClY1 - sy1) * (plClY1 - sy1);
                    float d3sq = (sx2 - plClX2) * (sx2 - plClX2) + (sy2 - plClY2) * (sy2 - plClY2);
                    if (d1sq > 1.0)
                    {
                        hud.DrawLineScreen(sx1, sy1, plClX1, plClY1, depthWidth, segDraw);
                    }
                    hud.DrawLineScreen(plClX1, plClY1, plClX2, plClY2, depthWidth, segPlDraw);
                    if (d3sq > 1.0)
                    {
                        hud.DrawLineScreen(plClX2, plClY2, sx2, sy2, depthWidth, segDraw);
                    }
                }
                else
                {
                    hud.DrawLineScreen(sx1, sy1, sx2, sy2, depthWidth, segDraw);
                }
            }

            // ================================================
            // Phase 3: Endcaps and joints (LOD close only)
            // v0.7.9: Depth-scaled sizes. Uses cached screen coords.
            // ================================================
            if (lodTier == 0 && decoratorBudget > 0)
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

                // Endcap at port A (first cached screen point)
                if (decoratorBudget > 0 && wsi.cachedScreenPts.Count() >= 2)
                {
                    vector ecA = wsi.cachedScreenPts[0];
                    if (ecA[2] > LFPG_BEHIND_CAM_Z)
                    {
                        vector ecA2 = wsi.cachedScreenPts[1];
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
                        decoratorBudget = decoratorBudget - 1;
                    }
                }

                // Endcap at port B (last cached screen point)
                int lastPtIdx = wsi.cachedScreenPts.Count() - 1;
                if (decoratorBudget > 0 && lastPtIdx >= 1)
                {
                    vector ecB = wsi.cachedScreenPts[lastPtIdx];
                    if (ecB[2] > LFPG_BEHIND_CAM_Z)
                    {
                        vector ecB2 = wsi.cachedScreenPts[lastPtIdx - 1];
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
                        decoratorBudget = decoratorBudget - 1;
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

						bool reuseJointProjection = (wsi.jointCacheValid && wsi.m_JointProjectionRevision == m_ProjectionRevision && wsi.cachedJointScreenPts.Count() == jCount && wsi.jointCacheCamPos[0] == camPos[0] && wsi.jointCacheCamPos[1] == camPos[1] && wsi.jointCacheCamPos[2] == camPos[2] && wsi.jointCacheCamDir[0] == camDir[0] && wsi.jointCacheCamDir[1] == camDir[1] && wsi.jointCacheCamDir[2] == camDir[2] && wsi.jointCacheViewportW == swF && wsi.jointCacheViewportH == shF);
                        if (!reuseJointProjection)
                        {
                            wsi.cachedJointScreenPts.Clear();
                            int jp;
                            for (jp = 0; jp < jCount; jp = jp + 1)
                            {
                                wsi.cachedJointScreenPts.Insert(g_Game.GetScreenPos(wsi.cachedJoints[jp]));
                            }
                            wsi.jointCacheCamPos = camPos;
                            wsi.jointCacheCamDir = camDir;
                            wsi.jointCacheViewportW = swF;
                            wsi.jointCacheViewportH = shF;
							wsi.m_JointProjectionRevision = m_ProjectionRevision;
                            wsi.jointCacheValid = true;
                        }

                        int ji;
                        for (ji = 0; ji < wsi.cachedJointScreenPts.Count() && decoratorBudget > 0; ji = ji + 1)
                        {
                            vector jScreen = wsi.cachedJointScreenPts[ji];
                            if (jScreen[2] > LFPG_BEHIND_CAM_Z)
                            {
                                hud.DrawJointScreen(jScreen[0], jScreen[1], jSize, decColor);
                                decoratorBudget = decoratorBudget - 1;
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

    // Fixed-cadence occlusion tick. Draw order is far-to-near, so walking it
    // backwards prioritizes nearby front-facing wires without changing paint order.
    protected void UpdateOcclusionBudget(vector camPos, vector camDir, Object ignoreObj, float nowMs)
    {
        if (nowMs < m_NextOccRaycastTickMs)
            return;

		if (m_CamMoved)
		{
			m_LastCamPos = camPos;
			m_LastCamDir = camDir;
		}

        m_NextOccRaycastTickMs = nowMs + LFPG_OCC_RAYCAST_TICK_MS;
        m_OccStaggerIdx = (m_OccStaggerIdx + 1) % 3;
        int rayBudget = LFPG_OCC_RAYCASTS_PER_TICK;
        int orderCount = m_DrawOrder.Count();
        if (rayBudget <= 0 || orderCount == 0)
            return;

        // Preserve near/in-cone priority while reserving one slot for fairness.
        int priorityBudget = rayBudget - 1;
        int di;
        for (di = orderCount - 1; di >= 0 && priorityBudget > 0; di = di - 1)
        {
            int priorityWireIndex = m_DrawOrder[di];
            LFPG_WireSegmentInfo priorityWsi = m_WireSegments.GetElement(priorityWireIndex);
            if (!priorityWsi || !priorityWsi.visible || priorityWsi.occStaggerGroup != m_OccStaggerIdx)
                continue;

            float priorityDcx = priorityWsi.cachedCenter[0] - camPos[0];
            float priorityDcy = priorityWsi.cachedCenter[1] - camPos[1];
            float priorityDcz = priorityWsi.cachedCenter[2] - camPos[2];
            float priorityDot = priorityDcx * camDir[0] + priorityDcy * camDir[1] + priorityDcz * camDir[2];
            if (priorityDot + priorityWsi.cachedRadius < 0.0)
                continue;

            float priorityDistSq = priorityDcx * priorityDcx + priorityDcy * priorityDcy + priorityDcz * priorityDcz;
            bool priorityInViewCone = (priorityDot > 0.0 && priorityDot * priorityDot >= priorityDistSq * 0.25);
            if (!priorityInViewCone)
                continue;

            float priorityDeadline = priorityWsi.occNextCheckMs;
            if (!m_CamMoved)
                priorityDeadline = priorityDeadline + LFPG_OCC_FORCED_RECHECK_MS;
            if (nowMs < priorityDeadline)
                continue;

            bool priorityBlocked = CheckWireOcclusionSample(camPos, priorityWsi, ignoreObj);
            priorityWsi.UpdateOcclusion(priorityBlocked, nowMs, priorityWsi.cachedMinDist);
            rayBudget = rayBudget - 1;
            priorityBudget = priorityBudget - 1;
        }

        int startCursor = m_OccCursorByGroup[m_OccStaggerIdx];
        if (startCursor < 0 || startCursor >= orderCount)
            startCursor = 0;
        int nextCursor = (startCursor + 1) % orderCount;
        bool cursorAdvanced = false;
        int walkOffset;
        for (walkOffset = 0; walkOffset < orderCount && rayBudget > 0; walkOffset = walkOffset + 1)
        {
            int walkPos = (startCursor + walkOffset) % orderCount;
            int wireIndex = m_DrawOrder[orderCount - 1 - walkPos];
            LFPG_WireSegmentInfo wsi = m_WireSegments.GetElement(wireIndex);
            if (!wsi || !wsi.visible || wsi.occStaggerGroup != m_OccStaggerIdx)
                continue;

            float dcx = wsi.cachedCenter[0] - camPos[0];
            float dcy = wsi.cachedCenter[1] - camPos[1];
            float dcz = wsi.cachedCenter[2] - camPos[2];
            float dot = dcx * camDir[0] + dcy * camDir[1] + dcz * camDir[2];
            if (dot + wsi.cachedRadius < 0.0)
                continue;

            float occDeadline = wsi.occNextCheckMs;
            if (!m_CamMoved)
                occDeadline = occDeadline + LFPG_OCC_FORCED_RECHECK_MS;
            if (nowMs < occDeadline)
                continue;

            bool allBlocked = CheckWireOcclusionSample(camPos, wsi, ignoreObj);
            wsi.UpdateOcclusion(allBlocked, nowMs, wsi.cachedMinDist);
            rayBudget = rayBudget - 1;
            nextCursor = (walkPos + 1) % orderCount;
            cursorAdvanced = true;
        }

        if (cursorAdvanced)
            m_OccCursorByGroup[m_OccStaggerIdx] = nextCursor;
        else
            m_OccCursorByGroup[m_OccStaggerIdx] = (startCursor + 1) % orderCount;
    }

    // One coarse sample is refreshed per eligible wire. Other sample results are
    // reused, preserving the existing blocked-ratio hysteresis at a bounded cost.
    protected bool CheckWireOcclusionSample(vector camPos, LFPG_WireSegmentInfo wsi, Object ignoreObj)
    {
        if (!wsi.occSamples || !wsi.occSampleBlocked || wsi.occSamples.Count() == 0 || wsi.occSampleBlocked.Count() != wsi.occSamples.Count())
            return false;

        int sampleCount = wsi.occSamples.Count();
        int sampleIndex = wsi.occSampleCursor;
        if (sampleIndex < 0 || sampleIndex >= sampleCount)
        {
            sampleIndex = 0;
        }

        vector target = wsi.occSamples[sampleIndex];
        target[1] = target[1] + LFPG_OCC_SAMPLE_LIFT_M;

        float pbDx = target[0] - camPos[0];
        float pbDy = target[1] - camPos[1];
        float pbDz = target[2] - camPos[2];
        float pbLenSq = pbDx * pbDx + pbDy * pbDy + pbDz * pbDz;
        if (pbLenSq > 0.01)
        {
            float pbLen = Math.Sqrt(pbLenSq);
            float pbInv = LFPG_OCC_WALL_PULLBACK_M / pbLen;
            target[0] = target[0] - pbDx * pbInv;
            target[1] = target[1] - pbDy * pbInv;
            target[2] = target[2] - pbDz * pbInv;
        }

        vector hitPos;
        vector hitNormal;
        int contactComponent;
        LFPG_RenderMetrics occRnd = LFPG_Telemetry.GetRender();
        occRnd.m_OccRaycastsUsed = occRnd.m_OccRaycastsUsed + 1;

        set<Object> rayResults = null;
        Object rayWith = null;
        bool bSorted = false;
        bool bGround = false;
        float rayRadius = 0.0;
        bool hit = DayZPhysics.RaycastRV(camPos, target, hitPos, hitNormal, contactComponent, rayResults, rayWith, ignoreObj, bSorted, bGround, ObjIntersectView, rayRadius);
        bool blocked = false;

        if (hit)
        {
            float hitDistSq = LFPG_WorldUtil.DistSq(camPos, hitPos);
            float targetDistSq = LFPG_WorldUtil.DistSq(camPos, target);
            if (hitDistSq < targetDistSq)
            {
                float effectiveMargin = LFPG_OCC_HIT_MARGIN_M;
                if (wsi.cachedRadius < LFPG_OCC_SHORT_WIRE_RADIUS_M)
                {
                    effectiveMargin = LFPG_OCC_HIT_MARGIN_WALL_M;
                }

                float hitDist = Math.Sqrt(hitDistSq);
                float offsetDist = hitDist + effectiveMargin;
                if (offsetDist * offsetDist < targetDistSq)
                {
                    float devRadSq = LFPG_OCC_DEVICE_RADIUS_M * LFPG_OCC_DEVICE_RADIUS_M;
                    float hitToASq = LFPG_WorldUtil.DistSq(hitPos, wsi.cachedPosA);
                    float hitToBSq = LFPG_WorldUtil.DistSq(hitPos, wsi.cachedPosB);
                    if (hitToASq >= devRadSq && hitToBSq >= devRadSq)
                    {
                        blocked = true;
                    }
                }
            }
        }

        wsi.occSampleBlocked[sampleIndex] = blocked;
        wsi.occSampleCursor = (sampleIndex + 1) % sampleCount;

        int blockedCount = 0;
        int si;
        for (si = 0; si < sampleCount; si = si + 1)
        {
            if (wsi.occSampleBlocked[si])
            {
                blockedCount = blockedCount + 1;
            }
        }

        float fBlocked = blockedCount;
        float fTotal = sampleCount;
        wsi.occBlockedRatio = fBlocked / fTotal;
        return blockedCount >= sampleCount;
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
                float ageS = g_Game.GetTickTime() - entry.createdMs;
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
            EntityAI ownerObj = EntityAI.Cast(g_Game.GetObjectByNetworkId(st.ownerLow, st.ownerHigh));
            if (!ownerObj)
            {
                // Owner still not loaded.
                // Only count as failure for TARGET_MISSING entries.
                if (entry.reason == LFPG_RetryReason.TARGET_MISSING)
                {
                    entry.retryCount = entry.retryCount + 1;
                    if (entry.retryCount > LFPG_RETRY_MAX)
                    {
                        if (LFPG_LOG_LEVEL >= 2)
                        {
                            string rlOwnMsg = "[CableRenderer] Retry limit (owner missing) for " + wireKey + ", giving up";
                            LFPG_Util.Debug(rlOwnMsg);
                        }
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
                        if (LFPG_LOG_LEVEL >= 2)
                        {
                            string rlTgtMsg = "[CableRenderer] Retry limit (target missing) for " + wireKey + ", giving up";
                            LFPG_Util.Debug(rlTgtMsg);
                        }
                        m_RetryQueue.Remove(wireKey);
                    }
                }
                continue;
            }

			vector a;
			vector b;
			PrepareWirePoints(wd, ownerObj, targetObj, a, b);

			st.lastPowered = IsOwnerActive(ownerObj);
			st.lastLoadRatio = LFPG_DeviceAPI.GetLoadRatio(ownerObj);
			st.lastOverloaded = LFPG_DeviceAPI.GetOverloaded(ownerObj);
			if (!BuildWire(wireKey, m_TempPoints, st.lastPowered, a, b, wd.m_Waypoints, entry.wireIndex))
			{
				entry.reason = LFPG_RetryReason.BUDGET;
				retTelRnd.m_WiresBudget = retTelRnd.m_WiresBudget + 1;
				continue;
			}

            m_RetryQueue.Remove(wireKey);

            if (LFPG_LOG_LEVEL >= 2)
            {
                string retOkMsg = "[CableRenderer] Retry succeeded: " + wireKey;
                LFPG_Util.Debug(retOkMsg);
            }
        }
    }

    // ===========================
    // ReconcileTick — periodic cable self-heal (v0.7.38, Audit #1)
    // ===========================
    // Runs every 60s (LFPG_RECONCILE_TICK_MS). Client-side only.
    //
    // Problem: A wire enters retry as TARGET_MISSING, retryCount hits
    // LFPG_RETRY_MAX (5), the entry is removed. If the target entity
    // loads AFTER that (late streaming, heavy server), the cable stays
    // invisible until the player reconnects or an admin forces refresh.
    //
    // Solution: Scan all owners' wire data. For each wire that has:
    //   - valid data in m_ByOwnerId (wire exists in topology)
    //   - NO built segments in m_WireSegments
    //   - NO active entry in m_RetryQueue
    // → re-insert into retry queue with fresh retryCount=0.
    //
    // Cost: O(total_wires) string lookups + map.Contains checks.
    // No entity resolution, no raycasts, no geometry. Safe at 60s interval.
    //
    // Note: NegCache entries for failed deviceIds expire after 5s
    // (NEG_CACHE_TTL_MS), so by the time ReconcileTick runs (60s),
    // stale NegCache entries are already purged. The re-inserted retry
    // will get a fresh resolution attempt in the next RetryTick cycle.
    protected void ReconcileTick()
    {
        int reconciled = 0;
        int totalWiresScanned = 0;

        int ownerIdx;
        for (ownerIdx = 0; ownerIdx < m_ByOwnerId.Count(); ownerIdx = ownerIdx + 1)
        {
            string ownerId = m_ByOwnerId.GetKey(ownerIdx);
            ref LFPG_OwnerWireState st = m_ByOwnerId.GetElement(ownerIdx);

            if (!st || !st.wires)
                continue;

            int wireCount = st.wires.Count();
            int w;
            for (w = 0; w < wireCount; w = w + 1)
            {
                totalWiresScanned = totalWiresScanned + 1;

                LFPG_WireData wd = st.wires[w];
                if (!wd)
                    continue;

                // Build the wireKey for this wire
                string wireKey = ownerId + "|" + w.ToString();

                // Check: does this wire have built segments?
                bool hasSegments = m_WireSegments.Contains(wireKey);

                // Check: is this wire already in the retry queue?
                bool inRetry = m_RetryQueue.Contains(wireKey);

                // If wire has data but no segments and no pending retry → reconcile
                if (!hasSegments && !inRetry)
                {
                    // Re-insert with TARGET_MISSING reason and fresh retryCount.
                    // AddRetry already checks for duplicates (no-op if key exists).
                    AddRetry(ownerId, w, LFPG_RetryReason.TARGET_MISSING);
                    reconciled = reconciled + 1;
                }
            }
        }

        if (reconciled > 0)
        {
            if (LFPG_LOG_LEVEL >= 1)
            {
                string logMsg = "[CableRenderer] ReconcileTick: re-queued ";
                logMsg = logMsg + reconciled.ToString();
                logMsg = logMsg + " orphaned wire(s) out of ";
                logMsg = logMsg + totalWiresScanned.ToString();
                logMsg = logMsg + " scanned";
                LFPG_Util.Info(logMsg);
            }
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
        entry.createdMs = g_Game.GetTickTime();

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

	// Keep bounds when releasing capacity; CullTick can rebuild on re-entry.
	protected void ReleaseWireSegments(LFPG_WireSegmentInfo info)
	{
		if (!info || !info.segments || info.segments.Count() == 0)
			return;
		m_TotalSegCount = m_TotalSegCount - info.segments.Count();
		info.DestroyAll();
		m_DrawOrderDirty = true;
	}

	// Under pressure, release invisible geometry first, then farther wires.
	// Empty metadata entries reserve no live segments.
	protected bool ReserveWireSegments(string wireKey, array<vector> points, vector posA, vector posB)
	{
		PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
		if (!player || !points || points.Count() < 2)
			return false;
		int required = points.Count() - 1;
		if (required > LFPG_MAX_RENDERED_SEGS)
			return false;
		vector playerPos = player.GetPosition();
		vector center = "0 0 0";
		float radius = 0.0;
		float candidateDist;
		float residentDist;
		int i;
		LFPG_WireSegmentInfo resident;
		LFPG_WireSegmentInfo victim;
		LFPG_WireSegmentInfo previous;
		int previousCount = 0;
		int reclaimable = 0;
		float farthestDist;
		ComputePointBounds(points, center, radius);
		candidateDist = vector.Distance(playerPos, center) - radius;
		if (candidateDist < 0.0)
			candidateDist = 0.0;
		if (candidateDist > LFPG_CULL_DISTANCE_M)
			return false;
		if (m_DeviceBubbleM > 0.0 && vector.Distance(playerPos, posA) > m_DeviceBubbleM && vector.Distance(playerPos, posB) > m_DeviceBubbleM)
			return false;
		if (m_WireSegments.Find(wireKey, previous) && previous && previous.segments)
			previousCount = previous.segments.Count();
		if (m_TotalSegCount - previousCount + required <= LFPG_MAX_RENDERED_SEGS)
			return true;

		// Preflight avoids eviction when nearer residents still leave no room.
		for (i = 0; i < m_WireSegments.Count(); i = i + 1)
		{
			resident = m_WireSegments.GetElement(i);
			if (!resident || resident == previous || resident.segments.Count() == 0)
				continue;
			residentDist = vector.Distance(playerPos, resident.cachedCenter) - resident.cachedRadius;
			if (!resident.visible || residentDist > candidateDist)
				reclaimable = reclaimable + resident.segments.Count();
		}
		if (m_TotalSegCount - previousCount + required - reclaimable > LFPG_MAX_RENDERED_SEGS)
			return false;

		while (m_TotalSegCount - previousCount + required > LFPG_MAX_RENDERED_SEGS)
		{
			victim = null;
			farthestDist = candidateDist;
			for (i = 0; i < m_WireSegments.Count(); i = i + 1)
			{
				resident = m_WireSegments.GetElement(i);
				if (!resident || resident == previous || resident.segments.Count() == 0)
					continue;
				if (!resident.visible)
				{
					victim = resident;
					break;
				}
				residentDist = vector.Distance(playerPos, resident.cachedCenter) - resident.cachedRadius;
				if (residentDist > farthestDist)
				{
					farthestDist = residentDist;
					victim = resident;
				}
			}
			if (!victim)
				return false;
			ReleaseWireSegments(victim);
		}
		return true;
	}

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
        m_DrawOrderDirty = true;
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
#endif
