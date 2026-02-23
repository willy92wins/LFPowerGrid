// =========================================================
// LF_PowerGrid - client cable renderer (v0.7.26)
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

    void LFPG_WireSegmentInfo()
    {
        segments   = new array<ref LFPG_CableParticle>;
        occSamples = new array<vector>;
        cachedJoints = new array<vector>;
        visible    = true;
        occluded   = false;
        occNextCheckMs  = 0;
        occConsecCount  = 0;
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

        // Add first point of first segment
        sumPos = sumPos + segments[0].m_From;
        pointCount = pointCount + 1;

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

        d = vector.Distance(cachedCenter, segments[0].m_From);
        if (d > maxDist)
        {
            maxDist = d;
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
    void UpdateOcclusion(bool blocked, float nowMs)
    {
        occNextCheckMs = nowMs + LFPG_OCC_INTERVAL_MS;

        if (blocked)
        {
            if (occConsecCount > 0)
            {
                occConsecCount = 0;
            }
            occConsecCount = occConsecCount - 1;

            if (occConsecCount <= -LFPG_OCC_HYSTERESIS)
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

            if (occConsecCount >= LFPG_OCC_HYSTERESIS)
            {
                occluded = false;
            }
        }
    }

    void SetVisible(bool vis)
    {
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
};

class LFPG_CableRenderer
{
    protected static ref LFPG_CableRenderer s_Instance;

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

    void LFPG_CableRenderer()
    {
        m_ByOwnerId      = new map<string, ref LFPG_OwnerWireState>;
        m_WireSegments   = new map<string, ref LFPG_WireSegmentInfo>;
        m_RetryQueue      = new map<string, ref LFPG_RetryEntry>;
        m_TempPoints      = new array<vector>;
        m_SagPoints       = new array<vector>;
        m_TempKeys        = new array<string>;
        m_ScreenPts       = new array<vector>;
        m_JointScreenPts  = new array<vector>;
        m_ConnCache       = new map<string, string>;
        m_NegCache        = new map<string, float>;
        m_DrawOrder       = new array<int>;
        m_DrawDist        = new array<float>;
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
            // Lightweight culling tick (replaces the old 0.5s full Refresh)
            GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(CullTick, (int)(LFPG_CULL_TICK_S * 1000.0), true);

            // Retry tick for unresolved wire targets
            GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(RetryTick, (int)(LFPG_RETRY_TICK_S * 1000.0), true);

            // Periodic negative cache cleanup
            GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(PurgeNegCache, NEG_CACHE_PURGE_INTERVAL_MS, true);
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

        LFPG_Util.Info("[CableRenderer] ForceGlobalRefresh: rebuilt " + ownerIds.Count().ToString() + " owners, totalSegs=" + m_TotalSegCount.ToString());
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
            LFPG_Diag.ServerEcho("[Resolve] HIT registry id=" + deviceId + " type=" + found.GetType());
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
                LFPG_Diag.ServerEcho("[Resolve] NegCache block id=" + deviceId + " age=" + age.ToString());
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
                LFPG_Diag.ServerEcho("[Resolve] HIT vanilla id=" + deviceId + " type=" + vObj.GetType());
                return vObj;
            }
        }

        // 4. Resolution failed
        m_NegCache[deviceId] = nowMs;
        LFPG_Diag.ServerEcho("[Resolve] MISS id=" + deviceId + " -> NegCache");
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
            LFPG_Util.Debug("[CableRenderer] NegCache purged " + m_TempKeys.Count().ToString() + " expired entries");
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

        LFPG_Util.Info("[CableRenderer] UpsertOwnerBlob owner=" + ownerDeviceId + " net=" + low.ToString() + ":" + high.ToString() + " jsonLen=" + json.Length().ToString());
        LFPG_Diag.ServerEcho("[CableRenderer] UpsertOwnerBlob owner=" + ownerDeviceId + " jsonLen=" + json.Length().ToString());

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
                LFPG_Util.Warn("[CableRenderer] UpsertOwnerBlob: decode failed, keeping previous state owner=" + ownerDeviceId);
                return;
            }

            // Decode succeeded: now safe to destroy old geometry and rebuild
            st.lastJson = json;

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
                LFPG_Util.Debug("[CableRenderer] Removed empty owner=" + ownerDeviceId);
                return;
            }

            LFPG_Util.Info("[CableRenderer] Built owner=" + ownerDeviceId + " wires=" + wireCount.ToString() + " pending=" + m_RetryQueue.Count().ToString());
        }
        else
        {
            LFPG_Util.Debug("[CableRenderer] UpsertOwnerBlob SKIP (json unchanged) owner=" + ownerDeviceId);
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
            LFPG_Util.Warn("CableRenderer: decode failed owner=" + st.ownerDeviceId + " err=" + err + " -> KEEPING previous topology");
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
            LFPG_Util.Warn("CableRenderer: owner=" + st.ownerDeviceId + " wire count " + srcCount.ToString() + " exceeds client limit " + LFPG_MAX_WIRES_PER_OWNER_CLIENT.ToString() + " -> CLAMPING");
            srcCount = LFPG_MAX_WIRES_PER_OWNER_CLIENT;
        }

        // Build into temporary array; only assign to st.wires at the end.
        ref array<ref LFPG_WireData> parsed = new array<ref LFPG_WireData>;

        LFPG_Diag.ServerEcho("[CableRenderer] DecodeOwner " + st.ownerDeviceId + " wires=" + blob.wires.Count().ToString());
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
            LFPG_Diag.ServerEcho("[CableRenderer] wire[" + i.ToString() + "] target=" + dwd.m_TargetDeviceId + " srcPort=" + dwd.m_SourcePort + " dstPort=" + dwd.m_TargetPort + " wps=" + wpCnt.ToString());
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
                EntityAI tgtObj = ResolveDeviceEntity(wd.m_TargetDeviceId);
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

        LFPG_Util.Info("[CableRenderer] BuildOwnerWires owner=" + ownerDeviceId + " net=" + st.ownerLow.ToString() + ":" + st.ownerHigh.ToString() + " wires=" + st.wires.Count().ToString());

        EntityAI ownerObj = EntityAI.Cast(GetGame().GetObjectByNetworkId(st.ownerLow, st.ownerHigh));
        if (!ownerObj)
        {
            // Owner not yet loaded on client: queue ALL wires for retry
            LFPG_Util.Warn("[CableRenderer] BuildOwnerWires: ownerObj NULL net=" + st.ownerLow.ToString() + ":" + st.ownerHigh.ToString());
            LFPG_Diag.ServerEcho("[CableRenderer] ownerObj NULL net=" + st.ownerLow.ToString() + ":" + st.ownerHigh.ToString());
            int rw;
            for (rw = 0; rw < st.wires.Count(); rw = rw + 1)
            {
                AddRetry(ownerDeviceId, rw, LFPG_RetryReason.TARGET_MISSING);
            }
            return;
        }

        LFPG_Diag.ServerEcho("[CableRenderer] ownerObj OK type=" + ownerObj.GetType() + " pos=" + ownerObj.GetPosition().ToString());

        st.lastPowered = IsOwnerActive(ownerObj);
        st.lastLoadRatio = LFPG_DeviceAPI.GetLoadRatio(ownerObj);
        st.lastOverloadMask = LFPG_DeviceAPI.GetOverloadMask(ownerObj);

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

            EntityAI targetObj = ResolveDeviceEntity(wd.m_TargetDeviceId);
            if (!targetObj)
            {
                LFPG_Util.Warn("[CableRenderer] BuildOwnerWires: target NULL id=" + wd.m_TargetDeviceId + " -> RETRY");
                LFPG_Diag.ServerEcho("[CableRenderer] target NULL id=" + wd.m_TargetDeviceId);
                AddRetry(ownerDeviceId, w, LFPG_RetryReason.TARGET_MISSING);
                // G5: wire waiting for target entity resolution
                bldTelRnd.m_WiresResolving = bldTelRnd.m_WiresResolving + 1;
                continue;
            }

            LFPG_Diag.ServerEcho("[CableRenderer] target OK type=" + targetObj.GetType() + " pos=" + targetObj.GetPosition().ToString());

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

            LFPG_Diag.ServerEcho("[CableRenderer] portA=" + a.ToString() + " portB=" + b.ToString());

            // Build raw point chain
            m_TempPoints.Clear();
            m_TempPoints.Insert(a);

            if (wd.m_Waypoints && wd.m_Waypoints.Count() > 0)
            {
                int j;
                for (j = 0; j < wd.m_Waypoints.Count(); j = j + 1)
                {
                    m_TempPoints.Insert(LFPG_WorldUtil.ClampAboveSurface(wd.m_Waypoints[j], 0.02));
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
                LFPG_Util.Debug("[CableRenderer] Over segment budget, queue retry " + wireKey);
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
            LFPG_Util.Warn("[CableRenderer] BuildWire: pts < 2 for " + wireKey);
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
            LFPG_Util.Warn("[CableRenderer] BuildWire: pts < 2 after compact for " + wireKey);
            return;
        }

        ApplyCatenaria(pts);

        if (m_SagPoints.Count() < 2)
        {
            LFPG_Util.Warn("[CableRenderer] BuildWire: sagPoints < 2 for " + wireKey);
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
            LFPG_Util.Warn("[CableRenderer] BuildWire: no valid segments for " + wireKey);
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
                info.cachedJoints.Insert(LFPG_WorldUtil.ClampAboveSurface(waypoints[wi], 0.02));
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

        // v0.7.9: pre-compute wire key to avoid string concat in CullTick
        info.cachedWireKey = wireKey;

        m_WireSegments[wireKey] = info;

        // v0.7.9: update incremental segment counter
        m_TotalSegCount = m_TotalSegCount + createdOk;

        LFPG_Diag.ServerEcho("[CableRenderer] BuildWire " + wireKey + " segs=" + createdOk.ToString() + "/" + segCount.ToString() + " center=" + info.cachedCenter.ToString() + " radius=" + info.cachedRadius.ToString());
        if (createdFail > 0)
        {
            LFPG_Util.Warn("[CableRenderer] BuildWire " + wireKey + " FAILED segs=" + createdFail.ToString());
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

        // Periodic diagnostic (every ~10s)
        int debugTick = GetGame().GetTime();
        bool doCullLog = (debugTick % 10000 < 2100);
        if (doCullLog)
        {
            string rd = "[CableRenderer] CullTick: owners=" + m_ByOwnerId.Count().ToString();
            rd = rd + " wireSegs=" + m_WireSegments.Count().ToString();
            rd = rd + " retries=" + m_RetryQueue.Count().ToString();
            rd = rd + " playerPos=" + pp.ToString();
            rd = rd + " bubble=" + bubbleM.ToString();
            LFPG_Diag.ServerEcho(rd);
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
                            ewInfo.SetVisible(false);
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
                        hwInfo.SetVisible(false);
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

                // v0.7.7: Bounding sphere culling.
                // Uses center + radius instead of just endpoints.
                // Fixes: cables with waypoints disappearing when player is near midpoint.
                // v0.7.11 (A3): All comparisons in squared domain.
                // Bounding sphere check reformulated:
                //   distToCenter - radius <= CULL_DIST
                //   → distToCenter <= CULL_DIST + radius
                //   → distToCenterSq <= (CULL_DIST + radius)²
                float distToCenterSq = LFPG_WorldUtil.DistSq(pp, info.cachedCenter);
                float cullPlusRadius = LFPG_CULL_DISTANCE_M + info.cachedRadius;
                float cullPlusRadiusSq = cullPlusRadius * cullPlusRadius;

                // Also check endpoint distances for backward compatibility
                float distASq = LFPG_WorldUtil.DistSq(pp, info.cachedPosA);
                float distBSq = LFPG_WorldUtil.DistSq(pp, info.cachedPosB);

                bool shouldBeVisible = false;

                // v0.7.11 (A3): Visibility checks entirely in squared domain.
                // Bounding sphere: visible if player within (cullDist + radius) of center
                if (distToCenterSq <= cullPlusRadiusSq)
                {
                    shouldBeVisible = true;
                }
                // Endpoint A within cull distance
                if (distASq <= cullDistSq)
                {
                    shouldBeVisible = true;
                }
                // Endpoint B within cull distance
                if (distBSq <= cullDistSq)
                {
                    shouldBeVisible = true;
                }

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

                // v0.7.23 (Bug 1): cachedMinDist for LOD + alpha fade + depth sort.
                // Changed from min(distA, distB) to avg(distA, distB).
                // Using the average ensures the painter's algorithm sorts wires
                // by their centroid depth, not their nearest endpoint. This fixes
                // wires with one near and one far endpoint drawing on top of
                // mid-distance wires.
                float avgDistSq = (distASq + distBSq) * 0.5;
                info.cachedMinDist = Math.Sqrt(avgDistSq);

                // v0.7.6: log visibility state for beam debugging
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

                // v0.7.8: update cable visual state based on powered + load + overload
                // Check if THIS specific wire is overloaded (bit in mask)
                // v0.7.9: Guard against shift overflow — Enforce int is 32-bit signed,
                // so 1 << 31 = negative (undefined). Limit bitmask to indices 0-30.
                bool wireOverloaded = false;
                if (info.wireIndex >= 0 && info.wireIndex < 31)
                {
                    int wireBit = 1 << info.wireIndex;
                    wireOverloaded = ((st.lastOverloadMask & wireBit) != 0);
                }

                // v0.7.23 (Bug 3): Cable state per-wire using overloadMask only.
                // Previously, WARNING_LOAD was set for ALL wires from a source
                // when the source's global loadRatio >= 0.80. This caused ALL
                // cables to show orange even when only specific wires are
                // overloaded. Now: only the specific wires in the overloadMask
                // show CRITICAL_LOAD. All other powered wires show POWERED.
                // Per-wire WARNING_LOAD requires a warningMask (future work).
                if (wireOverloaded)
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

        // v0.7.9: Deferred cleanup of owners whose entity has been null for 10+ seconds.
        // Cannot modify m_ByOwnerId during iteration, so we collected keys in m_TempKeys.
        // IMPORTANT: Copy to local array first because DestroyOwnerLines() clears m_TempKeys.
        if (m_TempKeys.Count() > 0)
        {
            ref array<string> ghostKeys = new array<string>;
            int gc;
            for (gc = 0; gc < m_TempKeys.Count(); gc = gc + 1)
            {
                ghostKeys.Insert(m_TempKeys[gc]);
            }

            int gk;
            for (gk = 0; gk < ghostKeys.Count(); gk = gk + 1)
            {
                string ghostId = ghostKeys[gk];
                LFPG_Util.Info("[CableRenderer] CullTick: removing ghost owner=" + ghostId + " (entity null for 10+s)");
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
        int visibleWires = m_AllWires.Count();
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

        // v0.7.23 (Bug 2): Painter's algorithm — sort wires far-to-near
        // so nearer cables draw ON TOP of farther ones.
        // Insertion sort is O(n²) but n is typically <100 visible wires.
        m_DrawOrder.Clear();
        m_DrawDist.Clear();
        int si;
        for (si = 0; si < wireCount; si = si + 1)
        {
            ref LFPG_WireSegmentInfo sortWsi = m_WireSegments.GetElement(si);
            if (!sortWsi)
                continue;

            float dist = sortWsi.cachedMinDist;
            // Insertion sort: find position (descending = farthest first)
            int insertAt = m_DrawOrder.Count(); // default: end
            int sj;
            for (sj = 0; sj < m_DrawDist.Count(); sj = sj + 1)
            {
                if (dist > m_DrawDist[sj])
                {
                    insertAt = sj;
                    break;
                }
            }
            m_DrawOrder.InsertAt(insertAt, si);
            m_DrawDist.InsertAt(insertAt, dist);
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

            // ---- Occlusion recheck ----
            // v0.7.9: When camera moves, recheck at normal interval (200ms staggered).
            // When camera is static, add forced delay to catch moving objects
            // (doors, vehicles) that may occlude/reveal cables.
            // Per-wire rate: ~600ms when moving, ~3s when static.
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
                    if ((i % 3) == (m_OccStaggerIdx % 3))
                    {
                        doOccCheck = true;
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
                    wsi.UpdateOcclusion(allBlocked, nowMs);
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
            else if (wireDist < LFPG_LOD_MID_M)
            {
                lodTier = 1;
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

            if (alphaFactor < 0.02)
            {
                tRnd.m_WiresCulled = tRnd.m_WiresCulled + 1;
                continue;
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
                if (alphaFactor < 0.99)
                {
                    highlightColor = ApplyAlpha(highlightColor, alphaFactor);
                }
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

            // Wind sway: unique phase per wire from position hash
            float swayPhase = wsi.cachedPosA[0] * 17.3 + wsi.cachedPosA[2] * 31.7;
            float swayOff = Math.Sin(nowMs * LFPG_SWAY_SPEED + swayPhase) * LFPG_SWAY_AMPLITUDE;

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
                bool isLastPoint = (s == segCount - 1);
                if (!isLastPoint)
                {
                    wp[1] = wp[1] + swayOff;
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
                bool behindA = (sA[2] < 0.1);
                bool behindB = (sB[2] < 0.1);
                if (behindA && behindB)
                    continue;

                // Resolve screen coords (behind-camera near-plane clipping)
                float sx1 = sA[0];
                float sy1 = sA[1];
                float sx2 = sB[0];
                float sy2 = sB[1];

                // v0.7.14: When one point is behind the camera, GetScreenPos
                // returns garbage. Clip the segment against the camera near plane
                // in 3D world space and re-project for a correct screen position.
                if (behindA || behindB)
                {
                    LFPG_CableParticle segW = wsi.segments[s];
                    if (segW)
                    {
                        if (behindA)
                        {
                            vector clipA = LFPG_WorldUtil.ClipBehindCamera(segW.m_From, segW.m_To, camPos, camDir);
                            sx1 = clipA[0];
                            sy1 = clipA[1];
                        }
                        else
                        {
                            vector clipB = LFPG_WorldUtil.ClipBehindCamera(segW.m_To, segW.m_From, camPos, camDir);
                            sx2 = clipB[0];
                            sy2 = clipB[1];
                        }
                    }
                    else
                    {
                        // Null segment: skip drawing
                        continue;
                    }
                }

                // Off-screen check (v0.7.9: proportional to resolution, unified with HUD)
                // v0.7.23 (Bug 1): Clipped points (near-plane projection) can produce
                // extreme screen coords that pass the normal generous margin.
                // Use a tighter margin (50px) when either point was clipped to
                // prevent "sticky lines at screen edges" during camera rotation.
                float margin;
                bool wasClipped = (behindA || behindB);
                if (wasClipped)
                {
                    margin = 50.0;
                }
                else
                {
                    margin = shF * 0.25;
                    if (margin < 200.0)
                    {
                        margin = 200.0;
                    }
                }
                bool offA = false;
                if (sx1 < -margin || sx1 > swF + margin || sy1 < -margin || sy1 > shF + margin)
                {
                    offA = true;
                }
                bool offB = false;
                if (sx2 < -margin || sx2 > swF + margin || sy2 < -margin || sy2 > shF + margin)
                {
                    offB = true;
                }
                if (offA && offB)
                    continue;

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

                if (avgZ > 0.1)
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

                // ---- Multi-pass drawing (LOD-dependent) ----
                if (lodTier <= 1)
                {
                    float shadowWidth = depthWidth + LFPG_SHADOW_WIDTH_ADD;
                    hud.DrawLineScreen(sx1, sy1, sx2, sy2, shadowWidth, shadowColor);
                }

                hud.DrawLineScreen(sx1, sy1, sx2, sy2, depthWidth, drawColor);

                if (lodTier == 0)
                {
                    float hlWidth = depthWidth - LFPG_HIGHLIGHT_WIDTH_SUB;
                    if (hlWidth < 1.0)
                    {
                        hlWidth = 1.0;
                    }
                    hud.DrawLineScreen(sx1, sy1, sx2, sy2, hlWidth, highlightColor);
                }
            }

            // ================================================
            // Phase 3: Endcaps and joints (LOD close only)
            // v0.7.9: Depth-scaled sizes. Uses cached screen coords.
            // ================================================
            if (lodTier == 0)
            {
                // Depth scale factor for decorators
                float decZ = wsi.cachedMinDist;
                if (decZ < 1.0)
                {
                    decZ = 1.0;
                }
                float decScale = LFPG_DEPTH_WIDTH_REF / decZ;

                // Endcap at port A (first point in m_ScreenPts)
                if (m_ScreenPts.Count() >= 2)
                {
                    vector ecA = m_ScreenPts[0];
                    if (ecA[2] > 0.1)
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
                        float ecSize = LFPG_ENDCAP_SIZE * decScale;
                        if (ecSize < LFPG_ENDCAP_SIZE_MIN)
                        {
                            ecSize = LFPG_ENDCAP_SIZE_MIN;
                        }
                        if (ecSize > LFPG_ENDCAP_SIZE_MAX)
                        {
                            ecSize = LFPG_ENDCAP_SIZE_MAX;
                        }
                        hud.DrawEndcapScreen(ecA[0], ecA[1], epx, epy, ecSize, LFPG_ENDCAP_WIDTH, drawColor);
                    }
                }

                // Endcap at port B (last point in m_ScreenPts)
                int lastPtIdx = m_ScreenPts.Count() - 1;
                if (lastPtIdx >= 1)
                {
                    vector ecB = m_ScreenPts[lastPtIdx];
                    if (ecB[2] > 0.1)
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
                        float ecSizeB = LFPG_ENDCAP_SIZE * decScale;
                        if (ecSizeB < LFPG_ENDCAP_SIZE_MIN)
                        {
                            ecSizeB = LFPG_ENDCAP_SIZE_MIN;
                        }
                        if (ecSizeB > LFPG_ENDCAP_SIZE_MAX)
                        {
                            ecSizeB = LFPG_ENDCAP_SIZE_MAX;
                        }
                        hud.DrawEndcapScreen(ecB[0], ecB[1], epxB, epyB, ecSizeB, LFPG_ENDCAP_WIDTH, drawColor);
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
                            if (jScreen[2] > 0.1)
                            {
                                hud.DrawJointScreen(jScreen[0], jScreen[1], jSize, drawColor);
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
            target[1] = target[1] + 0.08;

            vector hitPos;
            vector hitNormal;
            int contactComponent;

            // G5: count occlusion raycast
            occRnd.m_OccRaycastsUsed = occRnd.m_OccRaycastsUsed + 1;

            bool hit = DayZPhysics.RaycastRV(camPos, target, hitPos, hitNormal, contactComponent, null, null, ignoreObj, false, false, ObjIntersectView, 0.0);

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
                        LFPG_Util.Debug("[CableRenderer] Retry limit (owner missing) for " + wireKey + ", giving up");
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

            EntityAI targetObj = ResolveDeviceEntity(wd.m_TargetDeviceId);
            if (!targetObj)
            {
                // Target still not loaded.
                // Only count as failure for TARGET_MISSING entries.
                if (entry.reason == LFPG_RetryReason.TARGET_MISSING)
                {
                    entry.retryCount = entry.retryCount + 1;
                    if (entry.retryCount > LFPG_RETRY_MAX)
                    {
                        LFPG_Util.Debug("[CableRenderer] Retry limit (target missing) for " + wireKey + ", giving up");
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
                    m_TempPoints.Insert(LFPG_WorldUtil.ClampAboveSurface(wd.m_Waypoints[j], 0.02));
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
            BuildWire(wireKey, m_TempPoints, st.lastPowered, a, b, wd.m_Waypoints, entry.wireIndex);

            m_RetryQueue.Remove(wireKey);

            LFPG_Util.Debug("[CableRenderer] Retry succeeded: " + wireKey);
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
                    lerp = LFPG_WorldUtil.ClampAboveSurface(lerp, 0.02);

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

        int i;
        for (i = 0; i < m_WireSegments.Count(); i = i + 1)
        {
            m_TempKeys.Insert(m_WireSegments.GetKey(i));
        }

        int k;
        for (k = 0; k < m_TempKeys.Count(); k = k + 1)
        {
            if (m_TempKeys[k].IndexOf(prefix) == 0)
            {
                DestroyWire(m_TempKeys[k]);
            }
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
