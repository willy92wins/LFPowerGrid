#ifndef SERVER
// Client-only compilation boundary
// LF_PowerGrid - client wiring session + preview rendering
//
// Preview uses CanvasWidget (2D overlay) via LFPG_CableHUD.
// v0.7.10: preview migrated to screen-space projection
// (project once, draw via DrawLineScreen). Eliminates
// redundant GetScreenPos calls that duplicated in DrawSegment.
//
// v0.7.12 (Sprint 2):
//   B1 — Snap preview endpoint to target port anchor
//   B2 — Semáforo: preview color reflects connection rules
//   B3 — Pre-validate Finish via shared CanPreConnect
//
// v0.7.13 (Sprint 2.5):
//   G1 — Preview metrics: span/subseg/culled counters via LFPG_Telemetry
//
// v0.7.14: Fixed behind-camera preview lines (parallel to screen edges).
//   Replaced screen-space extension with 3D near-plane clipping.
//
// v0.7.36 (Audit Lote 1):
//   H1 — Cohen-Sutherland screen clipping for preview (same fix as F1.2)
//   M1 — Reusable PreConnectParams (avoids per-frame allocation)
//   L3 — FrameCounter wrap to prevent overflow
//
// Colour coding (hologram style, v0.7.12 semáforo):
//   GREEN  (0xFF00DD00) - connection valid + within limits
//   YELLOW (0xFFFFDD00) - segment near limit (80%+)
//   AMBER  (0xFFD39B00) - total wire length exceeded
//   RED    (0xFFFF3333) - segment over limit or connection invalid
//   GREY   (0xFF888888) - no valid target under cursor
//   CYAN   (0xFF00FFFF) - waypoint marker cross
class LFPG_PreviewSpanCache
{
    ref array<vector> points;
    float rawDistance;

    void LFPG_PreviewSpanCache()
    {
        points = new array<vector>;
    }
};

class LFPG_WiringClient
{
    protected static ref LFPG_WiringClient s_Instance;

    protected bool   m_Active = false;
    protected string m_SrcDeviceId;
    protected int    m_SrcLow;
    protected int    m_SrcHigh;
    protected string m_SrcPort;
    protected int    m_SrcPortDir;

    protected ref array<vector> m_Waypoints;

    protected int m_FrameCounter = 0;

    protected ref array<vector> m_PreviewPts;

    protected ref array<vector> m_SagPts;

    protected ref array<vector> m_PreviewScreenPts;

    protected static const float SYNC_COOLDOWN_SEC = 5.0;
    protected static float s_LastSyncRequestMs = -99999.0;

    protected int m_LastPreConnectStatus;
    protected string m_LastPreConnectReason;

    protected float m_SessionStartMs;

    protected float m_LastFinishMs;

    protected vector m_ClipA;
    protected vector m_ClipB;

    protected ref LFPG_PreConnectParams m_PreConnectParams;
    protected ref LFPG_PreConnectResult m_PreConnectResult;

    protected ref array<vector> m_PreviewPrefixPts;
    protected ref array<ref LFPG_PreviewSpanCache> m_PrefixSpanCaches;
    protected bool m_PrefixCacheValid;
    protected vector m_PrefixStartPos;
    protected int m_PrefixWaypointCount;
    protected float m_PrefixTotalLen;
    protected float m_PrefixRenderTotalLen;
    protected float m_PrefixMaxSegLen;

    void LFPG_WiringClient()
    {
        m_Waypoints = new array<vector>;
        m_PreviewPts = new array<vector>;
        m_SagPts = new array<vector>;
        m_PreviewScreenPts = new array<vector>;
        m_LastPreConnectStatus = LFPG_PreConnectStatus.NO_TARGET;
        m_LastPreConnectReason = "";
        m_SessionStartMs = 0.0;
        m_LastFinishMs = 0.0;
        m_ClipA = "0 0 0";
        m_ClipB = "0 0 0";
        m_PreConnectParams = new LFPG_PreConnectParams();
        m_PreConnectResult = new LFPG_PreConnectResult();
        m_PreviewPrefixPts = new array<vector>;
        m_PrefixSpanCaches = new array<ref LFPG_PreviewSpanCache>;
        int prefixIndex;
        for (prefixIndex = 0; prefixIndex < LFPG_MAX_WAYPOINTS; prefixIndex = prefixIndex + 1)
        {
            m_PrefixSpanCaches.Insert(new LFPG_PreviewSpanCache());
        }
        m_PrefixCacheValid = false;
        m_PrefixWaypointCount = -1;
    }

    static LFPG_WiringClient Get()
    {
        if (!s_Instance)
            s_Instance = new LFPG_WiringClient();
        return s_Instance;
    }

    // v0.7.5: reset stale session state on reconnect.
    static void Reset()
    {
        if (s_Instance)
        {
            s_Instance.CleanupInstance();
            s_Instance = null;
        }
    }

    protected void CleanupInstance()
    {
        m_Active = false;
        m_SrcDeviceId = "";
        m_SrcLow = 0;
        m_SrcHigh = 0;
        m_SrcPort = "";
        m_SrcPortDir = 0;
        if (m_Waypoints)
            m_Waypoints.Clear();
        if (m_PreviewPts)
            m_PreviewPts.Clear();
        if (m_SagPts)
            m_SagPts.Clear();
        if (m_PreviewScreenPts)
            m_PreviewScreenPts.Clear();
        m_FrameCounter = 0;
        m_LastPreConnectStatus = LFPG_PreConnectStatus.NO_TARGET;
        m_LastPreConnectReason = "";
        m_SessionStartMs = 0.0;
        m_LastFinishMs = 0.0;
        m_ClipA = "0 0 0";
        m_ClipB = "0 0 0";
        m_PreConnectParams = null;
        m_PreConnectResult = null;
        if (m_PreviewPrefixPts)
            m_PreviewPrefixPts.Clear();
        if (m_PrefixSpanCaches)
            m_PrefixSpanCaches.Clear();
        m_PrefixCacheValid = false;
        s_LastSyncRequestMs = -99999.0;
    }

    bool IsActive()
    {
        return m_Active;
    }

    int GetWaypointCount()
    {
        if (!m_Waypoints) return 0;
        return m_Waypoints.Count();
    }

    void AddWaypoint(vector pos)
    {
        if (!m_Active)
        {
            LFPG_Util.Warn("[WiringClient] AddWaypoint ignored - session not active");
            return;
        }

        if (m_Waypoints.Count() >= LFPG_MAX_WAYPOINTS)
        {
            LFPG_Util.Warn("[WiringClient] AddWaypoint ignored - max reached");
            return;
        }

        // v0.7.10: NaN guard — corrupted raycast can produce NaN positions
        if (pos[0] != pos[0] || pos[1] != pos[1] || pos[2] != pos[2])
        {
            LFPG_Util.Warn("[WiringClient] AddWaypoint ignored - NaN position");
            return;
        }

        // v0.7.9: Clamp waypoint to terrain surface at insertion time.
        // Ensures preview and committed cable use identical waypoint positions.
        pos = LFPG_WorldUtil.ClampAboveSurface(pos, LFPG_SURFACE_CLAMP_M);

        // v0.7.10: Reject waypoints nearly identical to the previous one.
        // Prevents degenerate spans, visual noise, and wasted sub-segments.
        if (m_Waypoints.Count() > 0)
        {
            vector prev = m_Waypoints[m_Waypoints.Count() - 1];
            float dx = pos[0] - prev[0];
            float dy = pos[1] - prev[1];
            float dz = pos[2] - prev[2];
            float distSq = dx * dx + dy * dy + dz * dz;
            if (distSq < 0.01)
            {
                LFPG_Util.Debug("[WiringClient] AddWaypoint ignored - too close to previous");
                return;
            }
        }

        m_Waypoints.Insert(pos);
        m_PrefixCacheValid = false;
        LFPG_Util.Info("[WiringClient] WP #" + m_Waypoints.Count().ToString() + " at " + pos.ToString());
    }

    void Start(string srcDeviceId, int srcLow, int srcHigh, string srcPort, int srcPortDir)
    {
        if (m_Active)
        {
            Cancel();
        }

        // v0.7.38 (BugFix): Post-finish cooldown.
        // After Finish() sends RPC and Cancel()s, port action immediately
        // sees !IsActive() and re-triggers Start() — creating an orphan
        // session that times out 2min later with confusing message.
        // Block Start() for 1s after Finish to let the action cycle settle.
        float nowStart = g_Game.GetTime();
        if (m_LastFinishMs > 0.0)
        {
            float sinceFin = nowStart - m_LastFinishMs;
            if (sinceFin < 1000.0)
            {
                return;
            }
        }

        m_Active      = true;
        m_SrcDeviceId = srcDeviceId;
        m_SrcLow      = srcLow;
        m_SrcHigh     = srcHigh;
        m_SrcPort     = srcPort;
        m_SrcPortDir  = srcPortDir;
        m_FrameCounter = 0;
        m_LastPreConnectStatus = LFPG_PreConnectStatus.NO_TARGET;
        m_LastPreConnectReason = "";
        m_SessionStartMs = g_Game.GetTime();

        m_Waypoints.Clear();
        m_PrefixCacheValid = false;

        RequestFullSync();

        LFPG_Diag.ServerEcho("[WiringClient] Session STARTED devId=" + srcDeviceId + " port=" + srcPort + " dir=" + srcPortDir.ToString());
    }

    void Finish(string dstDeviceId, int dstLow, int dstHigh, string dstPort, int dstPortDir)
    {
        if (!m_Active)
        {
            LFPG_Util.Warn("[WiringClient] Finish ignored - session not active");
            return;
        }

        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player)
        {
            LFPG_Util.Error("[WiringClient] Finish - no player");
            return;
        }

        // =========================================================
        // B3 (v0.7.12): Pre-validate via shared CanPreConnect
        // Same rules the server will check. Prevents wasting an RPC.
        // If entities can't be resolved (network bubble), skip pre-validation
        // and let the server handle it — avoids false rejections.
        // =========================================================
        EntityAI srcObj = EntityAI.Cast(g_Game.GetObjectByNetworkId(m_SrcLow, m_SrcHigh));
        EntityAI dstObj = EntityAI.Cast(g_Game.GetObjectByNetworkId(dstLow, dstHigh));

        if (srcObj && dstObj)
        {
            vector startPos = LFPG_DeviceAPI.GetPortWorldPos(srcObj, m_SrcPort);
            vector endPos = LFPG_DeviceAPI.GetPortWorldPos(dstObj, dstPort);

            LFPG_PreConnectParams pcp = new LFPG_PreConnectParams();
            pcp.srcEntity = srcObj;
            pcp.srcDeviceId = m_SrcDeviceId;
            pcp.srcPort = m_SrcPort;
            pcp.srcPortDir = m_SrcPortDir;
            pcp.dstEntity = dstObj;
            pcp.dstDeviceId = dstDeviceId;
            pcp.dstPort = dstPort;
            pcp.dstPortDir = dstPortDir;
            pcp.waypoints = m_Waypoints;
            pcp.startPos = startPos;
            pcp.endPos = endPos;

            LFPG_PreConnectResult preResult = LFPG_ConnectionRules.CanPreConnect(pcp);

            if (!preResult.IsValid())
            {
                // Connection invalid — show message, cancel, no RPC
                string failMsg = preResult.m_Reason;
                if (failMsg == "")
                {
                    failMsg = "Connection not allowed";
                }
                player.MessageStatus("[LFPG] " + failMsg);
                LFPG_Util.Info("[WiringClient] Finish BLOCKED by pre-validation: " + failMsg);
                Cancel();
                return;
            }
        }


        int actualSrcLow = m_SrcLow;
        int actualSrcHigh = m_SrcHigh;
        string actualSrcDeviceId = m_SrcDeviceId;
        string actualSrcPort = m_SrcPort;
        int actualDstLow = dstLow;
        int actualDstHigh = dstHigh;
        string actualDstDeviceId = dstDeviceId;
        string actualDstPort = dstPort;

        ref array<vector> finalWaypoints = new array<vector>;

        if (m_SrcPortDir == LFPG_PortDir.OUT)
        {
            int wi;
            for (wi = 0; wi < m_Waypoints.Count(); wi = wi + 1)
            {
                finalWaypoints.Insert(m_Waypoints[wi]);
            }
        }
        else
        {
            actualSrcLow = dstLow;
            actualSrcHigh = dstHigh;
            actualSrcDeviceId = dstDeviceId;
            actualSrcPort = dstPort;
            actualDstLow = m_SrcLow;
            actualDstHigh = m_SrcHigh;
            actualDstDeviceId = m_SrcDeviceId;
            actualDstPort = m_SrcPort;

            int rw;
            for (rw = m_Waypoints.Count() - 1; rw >= 0; rw = rw - 1)
            {
                finalWaypoints.Insert(m_Waypoints[rw]);
            }

            LFPG_Util.Info("[WiringClient] Reversed: IN->OUT, swapped src/dst");
        }

        LFPG_Util.Info("[WiringClient] Finish src=" + actualSrcDeviceId + ":" + actualSrcPort + " dst=" + actualDstDeviceId + ":" + actualDstPort);

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.FINISH_WIRING);

        rpc.Write(actualSrcLow);
        rpc.Write(actualSrcHigh);
        rpc.Write(actualDstLow);
        rpc.Write(actualDstHigh);

        rpc.Write(actualSrcDeviceId);
        rpc.Write(actualDstDeviceId);
        rpc.Write(actualSrcPort);
        rpc.Write(actualDstPort);
        rpc.Write(finalWaypoints);

        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);

        LFPG_Util.Info("[WiringClient] RPC FINISH_WIRING sent");

        m_LastFinishMs = g_Game.GetTime();

        Cancel();
    }

    void Cancel()
    {
        bool wasActive = m_Active;
        m_Active      = false;
        m_SrcDeviceId = "";
        m_SrcPort     = "";
        m_SrcPortDir  = -1;
        m_Waypoints.Clear();
        m_PrefixCacheValid = false;
        m_LastPreConnectStatus = LFPG_PreConnectStatus.NO_TARGET;
        m_LastPreConnectReason = "";
        m_SessionStartMs = 0.0;

        if (wasActive)
        {
            LFPG_CableHUD hud = LFPG_CableHUD.Get();
            if (hud && hud.IsReady())
            {
                hud.ClearCanvas();
            }

            LFPG_Util.Info("[WiringClient] Session ended");
            LFPG_Diag.ServerEcho("[WiringClient] Session CANCELLED");
        }
    }

    static void TickPreview()
    {
        LFPG_WiringClient wc = Get();
        if (!wc.m_Active)
            return;

        float nowMs = g_Game.GetTime();
        float elapsed = nowMs - wc.m_SessionStartMs;
        if (elapsed > LFPG_WIRING_SESSION_TIMEOUT_MS)
        {
            LFPG_Util.Warn("[WiringClient] Session TIMEOUT after " + elapsed.ToString() + "ms — auto-cancelling");
            PlayerBase timeoutPlayer = PlayerBase.Cast(g_Game.GetPlayer());
            if (timeoutPlayer)
            {
                timeoutPlayer.MessageStatus("[LFPG] Wiring session timed out.");
            }
            wc.Cancel();
            return;
        }

        wc.DrawPreviewFrame();
    }

    static int FindBestPort(EntityAI device, int wantDir, string deviceId, LFPG_CableRenderer renderer)
    {
        if (!device)
            return -1;

        int portCount = LFPG_DeviceAPI.GetPortCount(device);
        if (portCount <= 0)
            return -1;

        int firstEmpty = -1;
        int firstAny = -1;

        int pi;
        for (pi = 0; pi < portCount; pi = pi + 1)
        {
            int dir = LFPG_DeviceAPI.GetPortDir(device, pi);
            if (dir != wantDir)
                continue;

            if (firstAny < 0)
            {
                firstAny = pi;
            }

            if (firstEmpty < 0 && renderer)
            {
                string pName = LFPG_DeviceAPI.GetPortName(device, pi);
                string connType = renderer.GetConnectionType(deviceId, pName, dir);
                if (connType == "")
                {
                    firstEmpty = pi;
                }
            }
            else if (firstEmpty < 0 && !renderer)
            {
                firstEmpty = pi;
            }
        }

        if (firstEmpty >= 0)
            return firstEmpty;
        return firstAny;
    }

    // v0.7.38 (H1): Cohen-Sutherland moved to LFPG_WorldUtil.ClipSegToScreen
    // (shared with CableRenderer).

    protected void BuildPreviewSag(vector spanA, vector spanB, array<vector> output)
    {
        output.Clear();
        output.Insert(spanA);
        float rawDist = vector.Distance(spanA, spanB);
        int subs = LFPG_CableRenderer.GetAdaptiveSubs(rawDist);
        if (subs > 5)
        {
            subs = 5;
        }
        if (subs > 0)
        {
            float sagAmount = LFPG_CableRenderer.GetSagAmount(rawDist);
            int sub;
            for (sub = 1; sub <= subs; sub = sub + 1)
            {
                float t = sub / (subs + 1.0);
                vector lerp = spanA + (spanB - spanA) * t;
                float sag = sagAmount * 4.0 * t * (1.0 - t);
                lerp[1] = lerp[1] - sag;
                lerp = LFPG_WorldUtil.ClampAboveSurface(lerp, LFPG_SURFACE_CLAMP_M);
                output.Insert(lerp);
            }
        }
        output.Insert(spanB);
    }

    protected void EnsurePrefixCache(vector startPos)
    {
        int waypointCount = m_Waypoints.Count();
        bool cacheMatches = m_PrefixCacheValid;
        if (cacheMatches && waypointCount != m_PrefixWaypointCount)
        {
            cacheMatches = false;
        }
        if (cacheMatches && (startPos[0] != m_PrefixStartPos[0] || startPos[1] != m_PrefixStartPos[1] || startPos[2] != m_PrefixStartPos[2]))
        {
            cacheMatches = false;
        }
        if (cacheMatches)
            return;

        m_PreviewPrefixPts.Clear();
        m_PreviewPrefixPts.Insert(LFPG_WorldUtil.ClampAboveSurface(startPos));
        m_PrefixTotalLen = 0.0;
        m_PrefixRenderTotalLen = 0.0;
        m_PrefixMaxSegLen = 0.0;
        vector validationPrev = startPos;

        int w;
        for (w = 0; w < waypointCount; w = w + 1)
        {
            vector waypoint = m_Waypoints[w];
            float validationLen = vector.Distance(validationPrev, waypoint);
            m_PrefixTotalLen = m_PrefixTotalLen + validationLen;
            if (validationLen > m_PrefixMaxSegLen)
            {
                m_PrefixMaxSegLen = validationLen;
            }
            validationPrev = waypoint;
            m_PreviewPrefixPts.Insert(LFPG_WorldUtil.ClampAboveSurface(waypoint, LFPG_SURFACE_CLAMP_M));
        }

        int fixedSpanCount = m_PreviewPrefixPts.Count() - 1;
        for (w = 0; w < fixedSpanCount; w = w + 1)
        {
            LFPG_PreviewSpanCache spanCache = m_PrefixSpanCaches[w];
            vector spanA = m_PreviewPrefixPts[w];
            vector spanB = m_PreviewPrefixPts[w + 1];
            spanCache.rawDistance = vector.Distance(spanA, spanB);
            m_PrefixRenderTotalLen = m_PrefixRenderTotalLen + spanCache.rawDistance;
            BuildPreviewSag(spanA, spanB, spanCache.points);
        }

        m_PrefixStartPos = startPos;
        m_PrefixWaypointCount = waypointCount;
        m_PrefixCacheValid = true;
    }

    protected void DrawPreviewFrame()
    {
        if (g_Game.IsDedicatedServer())
            return;

        m_FrameCounter = m_FrameCounter + 1;
        // v0.7.36 (L3): Wrap to prevent int overflow in very long sessions.
        // 30000 frames ≈ 8.3 minutes at 60fps, safely above the 300-frame log interval.
        if (m_FrameCounter >= 30000)
        {
            m_FrameCounter = 0;
        }

        bool doLog = (LFPG_DIAG_ENABLED && m_FrameCounter % 300 == 1);

        if (doLog)
        {
            string hb = "[WiringClient] HEARTBEAT frame=" + m_FrameCounter.ToString();
            hb = hb + " active=" + m_Active.ToString();
            hb = hb + " wps=" + m_Waypoints.Count().ToString();
            LFPG_Diag.ServerEcho(hb);
        }

        // ---------- Resolve source ----------
        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player)
            return;

        EntityAI srcObj = EntityAI.Cast(g_Game.GetObjectByNetworkId(m_SrcLow, m_SrcHigh));
        if (!srcObj)
        {
            if (doLog)
                LFPG_Diag.ServerEcho("[Preview] SKIP: srcObj NULL");
            return;
        }

        vector startPos = LFPG_DeviceAPI.GetPortWorldPos(srcObj, m_SrcPort);

        // v0.7.10: Guard against invalid port position (port removed, API fail, NaN).
        // Without this, preview could draw from origin or corrupted coords.
        if (startPos[0] != startPos[0] || startPos[1] != startPos[1] || startPos[2] != startPos[2])
        {
            if (doLog)
                LFPG_Diag.ServerEcho("[Preview] SKIP: startPos NaN");
            return;
        }
        EnsurePrefixCache(startPos);

        // ---------- Cursor hit ----------
        vector cursorPos;
        bool hasCursor = LFPG_ActionRaycast.GetCursorWorldPos(player, cursorPos);
        if (!hasCursor)
        {
            cursorPos = g_Game.GetCurrentCameraPosition() + g_Game.GetCurrentCameraDirection() * 5.0;
        }

        // =========================================================
        // B1 (v0.7.12): Snap to target port when cursor is on device
        // B2 (v0.7.12): Evaluate connection rules for semáforo
        // =========================================================
        EntityAI targetDevice = LFPG_ActionRaycast.GetCursorTargetDevice(player);
        string snapDstDeviceId = "";
        string snapDstPort = "";
        int snapDstPortDir = -1;
        bool hasSnap = false;

        if (targetDevice)
        {
            // Determine which direction we want on the target:
            // If session started from OUT, target needs IN. If from IN, target needs OUT.
            int wantDir = LFPG_PortDir.IN;
            if (m_SrcPortDir == LFPG_PortDir.IN)
            {
                wantDir = LFPG_PortDir.OUT;
            }

            snapDstDeviceId = LFPG_DeviceAPI.GetOrCreateDeviceId(targetDevice);

            LFPG_CableRenderer renderer = LFPG_CableRenderer.Get();
            int bestPort = FindBestPort(targetDevice, wantDir, snapDstDeviceId, renderer);

            if (bestPort >= 0)
            {
                snapDstPort = LFPG_DeviceAPI.GetPortName(targetDevice, bestPort);
                snapDstPortDir = LFPG_DeviceAPI.GetPortDir(targetDevice, bestPort);

                // B1: Snap endpoint to port world position
                vector portPos = LFPG_DeviceAPI.GetPortWorldPos(targetDevice, snapDstPort);

                // NaN guard on port position
                if (portPos[0] == portPos[0] && portPos[1] == portPos[1] && portPos[2] == portPos[2])
                {
                    cursorPos = portPos;
                    hasSnap = true;
                }
            }
            else
            {
                // No compatible port on this device: treat as if no valid target.
                // This prevents CanPreConnect from seeing a device with no usable port
                // and incorrectly returning OK.
                targetDevice = null;
                snapDstDeviceId = "";
            }
        }

        // B2: Evaluate connection rules via shared helper
        // v0.7.36 (M1): Reuse class member instead of allocating per-frame.
        m_PreConnectParams.srcEntity = srcObj;
        m_PreConnectParams.srcDeviceId = m_SrcDeviceId;
        m_PreConnectParams.srcPort = m_SrcPort;
        m_PreConnectParams.srcPortDir = m_SrcPortDir;
        m_PreConnectParams.dstEntity = targetDevice;
        m_PreConnectParams.dstDeviceId = snapDstDeviceId;
        m_PreConnectParams.dstPort = snapDstPort;
        m_PreConnectParams.dstPortDir = snapDstPortDir;
        m_PreConnectParams.waypoints = m_Waypoints;
        m_PreConnectParams.startPos = startPos;
        m_PreConnectParams.endPos = cursorPos;

        LFPG_PreConnectResult preResult = LFPG_ConnectionRules.CanPreConnect(m_PreConnectParams, m_PreConnectResult, m_PrefixTotalLen, m_PrefixMaxSegLen);

        m_LastPreConnectStatus = preResult.m_Status;
        m_LastPreConnectReason = preResult.m_Reason;

        // Resolve global color from semáforo status
        int globalColor = LFPG_ConnectionRules.StatusToPreviewColor(preResult.m_Status);
        bool globalOverride = false;
        // INVALID (>= 10) and NO_TARGET (5): override ALL segments to this color
        if (preResult.m_Status >= 10 || preResult.m_Status == LFPG_PreConnectStatus.NO_TARGET)
        {
            globalOverride = true;
        }

        // ---------- Build raw point chain ----------
        // v0.7.9: Terrain clamping applied to all points (matches committed cable).
        m_PreviewPts.Clear();
        int prefixPoint;
        for (prefixPoint = 0; prefixPoint < m_PreviewPrefixPts.Count(); prefixPoint = prefixPoint + 1)
        {
            m_PreviewPts.Insert(m_PreviewPrefixPts[prefixPoint]);
        }

        int wpCount = m_Waypoints.Count();
        m_PreviewPts.Insert(LFPG_WorldUtil.ClampAboveSurface(cursorPos, LFPG_SURFACE_CLAMP_M));

        // ---------- Draw with catenaria sag (v0.7.10: screen-space) ----------
        LFPG_CableHUD hud = LFPG_CableHUD.Get();

        if (!hud || !hud.IsReady())
        {
            if (doLog)
                LFPG_Diag.ServerEcho("[Preview] SKIP: HUD not ready");
            return;
        }

        // v0.7.10: Reuse HUD's cached screen dimensions (set in BeginFrame).
        float swF = hud.GetScreenW();
        float shF = hud.GetScreenH();

        // v0.7.10: Guard against 0x0 resolution (transient during alt-tab/loading).
        if (swF <= 0.0 || shF <= 0.0)
            return;

        // v0.7.14: Camera position and direction for near-plane clipping
        vector camPos = g_Game.GetCurrentCameraPosition();
        vector camDir = g_Game.GetCurrentCameraDirection();

        // Screen margin for off-screen culling (proportional, same as committed cables)
        float margin = shF * LFPG_SCREEN_MARGIN_RATIO;
        if (margin < LFPG_SCREEN_MARGIN_MIN_PX)
        {
            margin = LFPG_SCREEN_MARGIN_MIN_PX;
        }

        // v0.8.x: Degenerate projection limits (same as CableRenderer DrawFrame).
        float degLimX = swF * LFPG_SCREEN_DEGENERATE_MULT;
        float degLimY = shF * LFPG_SCREEN_DEGENERATE_MULT;

        // v0.7.13 (G1): Preview metrics — grab reference once per frame
        LFPG_PreviewMetrics tPrv = LFPG_Telemetry.GetPreview();

        // v0.7.9: compute total wire length for color feedback.
        int rawSegCount = m_PreviewPts.Count() - 1;
        float totalWireLen = m_PrefixRenderTotalLen;
        if (rawSegCount > 0)
        {
            totalWireLen = totalWireLen + vector.Distance(m_PreviewPts[rawSegCount - 1], m_PreviewPts[rawSegCount]);
        }
        bool totalExceedsLimit = (totalWireLen > LFPG_MAX_WIRE_LEN_M);

        // G1: record span count for this frame
        tPrv.m_Spans = tPrv.m_Spans + rawSegCount;

        int s;
        for (s = 0; s < rawSegCount; s = s + 1)
        {
            vector spanA = m_PreviewPts[s];
            vector spanB = m_PreviewPts[s + 1];
            bool fixedPrefixSpan = (s < rawSegCount - 1);
            LFPG_PreviewSpanCache fixedSpanCache = null;
            float rawDist = 0.0;
            if (fixedPrefixSpan)
            {
                fixedSpanCache = m_PrefixSpanCaches[s];
                rawDist = fixedSpanCache.rawDistance;
            }
            else
            {
                rawDist = vector.Distance(spanA, spanB);
            }

            // v0.7.12: Color selection — semáforo global override vs per-segment length
            int color;
            if (globalOverride)
            {
                // INVALID or NO_TARGET: entire preview uses global color
                color = globalColor;
            }
            else
            {
                // Per-segment length-based colors (preserved from v0.7.9)
                // Priority: red (segment) > amber (total) > yellow (near limit) > green (OK)
                if (rawDist > LFPG_MAX_SEGMENT_LEN_M)
                {
                    color = LFPG_PREVIEW_COLOR_INVALID; // red - segment exceeds limit
                }
                else if (totalExceedsLimit)
                {
                    color = LFPG_PREVIEW_COLOR_OVER; // amber - total wire length exceeds limit
                }
                else if (rawDist > LFPG_MAX_SEGMENT_LEN_M * LFPG_NEAR_LIMIT_RATIO)
                {
                    color = LFPG_PREVIEW_COLOR_WARN; // yellow - segment near limit (80%+)
                }
                else
                {
                    // Use global color from semáforo (green if OK, yellow if warning)
                    color = globalColor;
                }
            }

            // Reuse cached world-space catenary for the fixed prefix.
            array<vector> spanSagPts = m_SagPts;
            if (fixedPrefixSpan)
            {
                spanSagPts = fixedSpanCache.points;
            }
            else
            {
                BuildPreviewSag(spanA, spanB, m_SagPts);
            }

            // Projection remains camera-dependent and runs each frame.
            int sagCount = spanSagPts.Count();
            m_PreviewScreenPts.Clear();
            int pp;
            for (pp = 0; pp < sagCount; pp = pp + 1)
            {
                // v0.8.x: Degenerate projection guard (same as CableRenderer Phase 1).
                // Mark extreme projections by zeroing z so the behindA/behindB
                // check downstream skips them naturally.
                vector prvScr = g_Game.GetScreenPos(spanSagPts[pp]);
                if (prvScr[2] > LFPG_BEHIND_CAM_Z)
                {
                    float absPX = prvScr[0];
                    if (absPX < 0.0)
                    {
                        absPX = -absPX;
                    }
                    float absPY = prvScr[1];
                    if (absPY < 0.0)
                    {
                        absPY = -absPY;
                    }
                    if (absPX > degLimX || absPY > degLimY)
                    {
                        prvScr[2] = 0.0;
                    }
                }
                m_PreviewScreenPts.Insert(prvScr);
            }
            // G1: projections for this span
            tPrv.m_Projections = tPrv.m_Projections + sagCount;

            // Draw sub-segments from cached projections
            int subSegCount = sagCount - 1;
            // G1: count sub-segments for this span
            tPrv.m_SubSegments = tPrv.m_SubSegments + subSegCount;

            int ss;
            for (ss = 0; ss < subSegCount; ss = ss + 1)
            {
                vector scrA = m_PreviewScreenPts[ss];
                vector scrB = m_PreviewScreenPts[ss + 1];

                // v0.7.10: NaN guard
                if (scrA[0] != scrA[0] || scrA[1] != scrA[1] || scrA[2] != scrA[2])
                    continue;
                if (scrB[0] != scrB[0] || scrB[1] != scrB[1] || scrB[2] != scrB[2])
                    continue;

                // Behind camera check
                bool behindA = (scrA[2] < LFPG_BEHIND_CAM_Z);
                bool behindB = (scrB[2] < LFPG_BEHIND_CAM_Z);

                // v0.7.38: Skip segments with any endpoint behind camera.
                // Both-behind AND single-behind: skip entirely.
                // ClipBehindCamera produced extreme screen coords causing
                // diagonal artifacts. Losing one sub-segment at the camera
                // transition is imperceptible with catenary subdivision.
                if (behindA || behindB)
                {
                    tPrv.m_CulledBehind = tPrv.m_CulledBehind + 1;
                    continue;
                }

                float sx1 = scrA[0];
                float sy1 = scrA[1];
                float sx2 = scrB[0];
                float sy2 = scrB[1];

                // v0.7.36 (H1): Cohen-Sutherland screen clipping for preview.
                // Replaces offA && offB cull that incorrectly culled segments
                // spanning the viewport (same bug as committed cables F1.2).
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
                if (offA || offB)
                {
                    // FIX: C-S clips to actual viewport, not expanded rect.
                    // Margin only used in fast-path above to decide invocation.
                    bool segVisible = LFPG_WorldUtil.ClipSegToScreen(sx1, sy1, sx2, sy2,
                        0.0, 0.0, swF, shF, m_ClipA, m_ClipB);
                    if (!segVisible)
                    {
                        tPrv.m_CulledOffScreen = tPrv.m_CulledOffScreen + 1;
                        continue;
                    }
                    sx1 = m_ClipA[0];
                    sy1 = m_ClipA[1];
                    sx2 = m_ClipB[0];
                    sy2 = m_ClipB[1];
                }

                // v0.8.x: Edge fade for preview segments near screen edge.
                int segColor = color;
                if (offA || offB)
                {
                    float prvEdgeFade = LFPG_WorldUtil.ComputeEdgeFade(sx1, sy1, sx2, sy2, swF, shF, LFPG_EDGE_FADE_PX);
                    if (prvEdgeFade < 0.01)
                    {
                        tPrv.m_CulledOffScreen = tPrv.m_CulledOffScreen + 1;
                        continue;
                    }
                    if (prvEdgeFade < 0.99)
                    {
                        // Apply fade to alpha channel of color
                        int prvOrigAlpha = (color >> 24) & 0xFF;
                        int prvNewAlpha = (int)(prvOrigAlpha * prvEdgeFade);
                        if (prvNewAlpha < 0)
                        {
                            prvNewAlpha = 0;
                        }
                        if (prvNewAlpha > 255)
                        {
                            prvNewAlpha = 255;
                        }
                        int prvRgb = color & 0x00FFFFFF;
                        segColor = (prvNewAlpha << 24) | prvRgb;
                    }
                }

                hud.DrawLineScreen(sx1, sy1, sx2, sy2, LFPG_PREVIEW_LINE_WIDTH, segColor);
                tPrv.m_Drawn = tPrv.m_Drawn + 1;
            }
        }

        // ---------- Waypoint crosses (cyan, screen-space) ----------
        // v0.7.10: project waypoints once and draw via DrawCrossScreen.
        int m;
        for (m = 0; m < wpCount; m = m + 1)
        {
            vector wpScr = g_Game.GetScreenPos(m_Waypoints[m]);
            tPrv.m_Projections = tPrv.m_Projections + 1;
            if (wpScr[2] > LFPG_BEHIND_CAM_Z)
            {
                hud.DrawCrossScreen(wpScr[0], wpScr[1], 0xFF00FFFF, 12.0);
            }
        }

        // v0.7.12 (B1): Draw snap indicator when endpoint is snapped to a port
        if (hasSnap)
        {
            vector snapScr = g_Game.GetScreenPos(cursorPos);
            tPrv.m_Projections = tPrv.m_Projections + 1;
            if (snapScr[2] > LFPG_BEHIND_CAM_Z && snapScr[0] == snapScr[0] && snapScr[1] == snapScr[1])
            {
                // Diamond marker at snap point using the semáforo color
                hud.DrawCrossScreen(snapScr[0], snapScr[1], globalColor, 10.0);
            }
        }

        // ---------- Periodic log ----------
        if (doLog)
        {
            string dl = "[Preview] segs=" + rawSegCount.ToString();
            dl = dl + " wps=" + wpCount.ToString();
            dl = dl + " totalLen=" + totalWireLen.ToString();
            dl = dl + " srcPos=" + startPos.ToString();
            dl = dl + " cursorPos=" + cursorPos.ToString();
            dl = dl + " hasCursor=" + hasCursor.ToString();
            dl = dl + " snap=" + hasSnap.ToString();
            dl = dl + " preConnect=" + m_LastPreConnectStatus.ToString();

            // G1: inline preview metrics for this frame
            dl = dl + " | drawn=" + tPrv.m_Drawn.ToString();
            dl = dl + " subSegs=" + tPrv.m_SubSegments.ToString();
            dl = dl + " behindCull=" + tPrv.m_CulledBehind.ToString();
            dl = dl + " offCull=" + tPrv.m_CulledOffScreen.ToString();
            dl = dl + " projections=" + tPrv.m_Projections.ToString();
            LFPG_Diag.ServerEcho(dl);

            // v0.7.10: Limit point dump to first 6 to reduce log noise.
            int ptCount = m_PreviewPts.Count();
            int maxPtLog = 6;
            if (ptCount < maxPtLog)
            {
                maxPtLog = ptCount;
            }
            int pi;
            for (pi = 0; pi < maxPtLog; pi = pi + 1)
            {
                LFPG_Diag.ServerEcho("[Preview] pt[" + pi.ToString() + "]=" + m_PreviewPts[pi].ToString());
            }
            if (ptCount > maxPtLog)
            {
                LFPG_Diag.ServerEcho("[Preview] ...+" + (ptCount - maxPtLog).ToString() + " more points");
            }
        }
    }

    // =========================================================
    // Centralized FullSync request (throttled, static)
    // Both MissionInit and WiringClient.Start funnel through here.
    // Skips if a sync was already requested within SYNC_COOLDOWN_SEC.
    // =========================================================
    static bool RequestFullSync()
    {
        if (g_Game.IsDedicatedServer())
            return false;

        float nowMs = g_Game.GetTime();
        float elapsedSec = (nowMs - s_LastSyncRequestMs) * 0.001;

        if (elapsedSec < SYNC_COOLDOWN_SEC)
        {
            LFPG_Util.Debug("[FullSync] THROTTLED (last " + elapsedSec.ToString() + "s ago, cooldown=" + SYNC_COOLDOWN_SEC.ToString() + "s)");
            return false;
        }

        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player) return false;

        s_LastSyncRequestMs = nowMs;

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.REQUEST_FULL_SYNC);
        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);

        LFPG_Util.Info("[FullSync] Requested (elapsed=" + elapsedSec.ToString() + "s)");
        return true;
    }
};
#endif
