// =========================================================
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
// =========================================================

class LFPG_WiringClient
{
    protected static ref LFPG_WiringClient s_Instance;

    // Session state
    protected bool   m_Active = false;
    protected string m_SrcDeviceId;
    protected int    m_SrcLow;
    protected int    m_SrcHigh;
    protected string m_SrcPort;
    protected int    m_SrcPortDir;

    protected ref array<vector> m_Waypoints;

    // Frame counter for throttled logging
    protected int m_FrameCounter = 0;

    // Reusable temp array for preview drawing (avoids GC pressure)
    protected ref array<vector> m_PreviewPts;

    // v0.7.9: Reusable sag output buffer for preview catenaria
    protected ref array<vector> m_SagPts;

    // v0.7.10: Screen projection cache for preview (avoids redundant GetScreenPos).
    // Matches CableRenderer.DrawFrame approach: project once, draw from cache.
    protected ref array<vector> m_PreviewScreenPts;

    // FullSync throttle: minimum seconds between sync requests (client-side)
    protected static const float SYNC_COOLDOWN_SEC = 5.0;
    protected static float s_LastSyncRequestMs = -99999.0;

    // v0.7.12 (B2): Cached semáforo status for Finish pre-validation (B3)
    protected int m_LastPreConnectStatus;
    protected string m_LastPreConnectReason;

    // v0.7.33 (Fix #14): Session start time for timeout detection.
    // Prevents stuck sessions from disconnect, alt-tab, or unresponsive server.
    protected float m_SessionStartMs;

    // v0.7.36 (H1): Cohen-Sutherland clip output buffers.
    // Reused per-segment to avoid allocation (mirrors CableRenderer approach).
    protected vector m_ClipA;
    protected vector m_ClipB;

    // v0.7.36 (M1): Reusable PreConnectParams to avoid per-frame allocation.
    // Fields are overwritten each frame in DrawPreviewFrame.
    protected ref LFPG_PreConnectParams m_PreConnectParams;

    void LFPG_WiringClient()
    {
        m_Waypoints = new array<vector>;
        m_PreviewPts = new array<vector>;
        m_SagPts = new array<vector>;
        m_PreviewScreenPts = new array<vector>;
        m_LastPreConnectStatus = LFPG_PreConnectStatus.NO_TARGET;
        m_LastPreConnectReason = "";
        m_SessionStartMs = 0.0;
        m_ClipA = "0 0 0";
        m_ClipB = "0 0 0";
        m_PreConnectParams = new LFPG_PreConnectParams();
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
            delete s_Instance;
            s_Instance = null;
        }
    }

    // ---- Public API ----

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
        LFPG_Util.Info("[WiringClient] WP #" + m_Waypoints.Count().ToString() + " at " + pos.ToString());
    }

    void Start(string srcDeviceId, int srcLow, int srcHigh, string srcPort, int srcPortDir)
    {
        if (m_Active)
        {
            Cancel();
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
        m_SessionStartMs = GetGame().GetTime();

        m_Waypoints.Clear();

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

        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
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
        EntityAI srcObj = EntityAI.Cast(GetGame().GetObjectByNetworkId(m_SrcLow, m_SrcHigh));
        EntityAI dstObj = EntityAI.Cast(GetGame().GetObjectByNetworkId(dstLow, dstHigh));

        if (srcObj && dstObj)
        {
            // Resolve positions for geometry validation
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

        // =========================================================
        // Direction swap logic (preserved from v0.7.11)
        // =========================================================

        // Determine actual source (OUT) and target (IN).
        // If user started from IN and is finishing on OUT, swap.
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
            // Normal order: first=OUT, second=IN. Waypoints stay as-is.
            int wi;
            for (wi = 0; wi < m_Waypoints.Count(); wi = wi + 1)
            {
                finalWaypoints.Insert(m_Waypoints[wi]);
            }
        }
        else
        {
            // Reversed: first=IN, second=OUT. Swap and reverse waypoints.
            actualSrcLow = dstLow;
            actualSrcHigh = dstHigh;
            actualSrcDeviceId = dstDeviceId;
            actualSrcPort = dstPort;
            actualDstLow = m_SrcLow;
            actualDstHigh = m_SrcHigh;
            actualDstDeviceId = m_SrcDeviceId;
            actualDstPort = m_SrcPort;

            // Reverse waypoint order so geometry matches source→target direction
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
        m_LastPreConnectStatus = LFPG_PreConnectStatus.NO_TARGET;
        m_LastPreConnectReason = "";
        m_SessionStartMs = 0.0;

        // v0.7.10: Explicitly clear canvas to prevent ghost preview lines
        // persisting for one frame if Cancel() fires outside the normal draw cycle.
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

    // =========================================================
    // Per-frame preview rendering
    // =========================================================
    static void TickPreview()
    {
        LFPG_WiringClient wc = Get();
        if (!wc.m_Active)
            return;

        // v0.7.33 (Fix #14): Session timeout — auto-cancel stale sessions.
        // Prevents stuck state from disconnect, alt-tab, or unresponsive server.
        float nowMs = GetGame().GetTime();
        float elapsed = nowMs - wc.m_SessionStartMs;
        if (elapsed > LFPG_WIRING_SESSION_TIMEOUT_MS)
        {
            LFPG_Util.Warn("[WiringClient] Session TIMEOUT after " + elapsed.ToString() + "ms — auto-cancelling");
            PlayerBase timeoutPlayer = PlayerBase.Cast(GetGame().GetPlayer());
            if (timeoutPlayer)
            {
                timeoutPlayer.MessageStatus("[LFPG] Wiring session timed out.");
            }
            wc.Cancel();
            return;
        }

        wc.DrawPreviewFrame();
    }

    // =========================================================
    // FindBestPort — find the best compatible port on a device
    //
    // Moved from LFPG_ConnectionRules (3_Game) in v0.7.13 because
    // it depends on LFPG_DeviceAPI + LFPG_CableRenderer (both 4_World).
    // 3_Game layer cannot reference 4_World types.
    //
    // Returns port index, or -1 if no compatible port exists.
    // Priority: empty port > occupied port (for replacement).
    // =========================================================
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

            // Check occupancy via renderer cache (client-side best effort)
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

    protected void DrawPreviewFrame()
    {
        if (GetGame().IsDedicatedServer())
            return;

        m_FrameCounter = m_FrameCounter + 1;
        // v0.7.36 (L3): Wrap to prevent int overflow in very long sessions.
        // 30000 frames ≈ 8.3 minutes at 60fps, safely above the 300-frame log interval.
        if (m_FrameCounter >= 30000)
        {
            m_FrameCounter = 0;
        }

        bool doLog = (m_FrameCounter % 300 == 1);

        if (doLog)
        {
            string hb = "[WiringClient] HEARTBEAT frame=" + m_FrameCounter.ToString();
            hb = hb + " active=" + m_Active.ToString();
            hb = hb + " wps=" + m_Waypoints.Count().ToString();
            LFPG_Diag.ServerEcho(hb);
        }

        // ---------- Resolve source ----------
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player)
            return;

        EntityAI srcObj = EntityAI.Cast(GetGame().GetObjectByNetworkId(m_SrcLow, m_SrcHigh));
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

        // ---------- Cursor hit ----------
        vector cursorPos;
        bool hasCursor = LFPG_ActionRaycast.GetCursorWorldPos(player, cursorPos);
        if (!hasCursor)
        {
            cursorPos = GetGame().GetCurrentCameraPosition() + GetGame().GetCurrentCameraDirection() * 5.0;
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

        LFPG_PreConnectResult preResult = LFPG_ConnectionRules.CanPreConnect(m_PreConnectParams);

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
        m_PreviewPts.Insert(LFPG_WorldUtil.ClampAboveSurface(startPos));

        int wpCount = m_Waypoints.Count();
        int w;
        for (w = 0; w < wpCount; w = w + 1)
        {
            m_PreviewPts.Insert(LFPG_WorldUtil.ClampAboveSurface(m_Waypoints[w], LFPG_SURFACE_CLAMP_M));
        }
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
        vector camPos = GetGame().GetCurrentCameraPosition();
        vector camDir = GetGame().GetCurrentCameraDirection();

        // Screen margin for off-screen culling (proportional, same as committed cables)
        float margin = shF * LFPG_SCREEN_MARGIN_RATIO;
        if (margin < LFPG_SCREEN_MARGIN_MIN_PX)
        {
            margin = LFPG_SCREEN_MARGIN_MIN_PX;
        }

        // v0.7.13 (G1): Preview metrics — grab reference once per frame
        LFPG_PreviewMetrics tPrv = LFPG_Telemetry.GetPreview();

        // v0.7.9: compute total wire length for color feedback.
        int rawSegCount = m_PreviewPts.Count() - 1;
        float totalWireLen = 0.0;
        int ts;
        for (ts = 0; ts < rawSegCount; ts = ts + 1)
        {
            totalWireLen = totalWireLen + vector.Distance(m_PreviewPts[ts], m_PreviewPts[ts + 1]);
        }
        bool totalExceedsLimit = (totalWireLen > LFPG_MAX_WIRE_LEN_M);

        // G1: record span count for this frame
        tPrv.m_Spans = tPrv.m_Spans + rawSegCount;

        int s;
        for (s = 0; s < rawSegCount; s = s + 1)
        {
            vector spanA = m_PreviewPts[s];
            vector spanB = m_PreviewPts[s + 1];
            float rawDist = vector.Distance(spanA, spanB);

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

            // Compute adaptive sag for this span
            int subs = LFPG_CableRenderer.GetAdaptiveSubs(rawDist);

            // v0.7.10: Cap preview subdivisions to limit per-frame cost.
            if (subs > 5)
            {
                subs = 5;
            }

            // Build point list for this span (either with sag or direct)
            m_SagPts.Clear();
            m_SagPts.Insert(spanA);

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
                    m_SagPts.Insert(lerp);
                }
            }

            m_SagPts.Insert(spanB);

            // Project all points for this span ONCE
            int sagCount = m_SagPts.Count();
            m_PreviewScreenPts.Clear();
            int pp;
            for (pp = 0; pp < sagCount; pp = pp + 1)
            {
                m_PreviewScreenPts.Insert(GetGame().GetScreenPos(m_SagPts[pp]));
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
                    bool segVisible = LFPG_WorldUtil.ClipSegToScreen(sx1, sy1, sx2, sy2,
                        -margin, -margin, swF + margin, shF + margin, m_ClipA, m_ClipB);
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

                hud.DrawLineScreen(sx1, sy1, sx2, sy2, LFPG_PREVIEW_LINE_WIDTH, color);
                tPrv.m_Drawn = tPrv.m_Drawn + 1;
            }
        }

        // ---------- Waypoint crosses (cyan, screen-space) ----------
        // v0.7.10: project waypoints once and draw via DrawCrossScreen.
        int m;
        for (m = 0; m < wpCount; m = m + 1)
        {
            vector wpScr = GetGame().GetScreenPos(m_Waypoints[m]);
            tPrv.m_Projections = tPrv.m_Projections + 1;
            if (wpScr[2] > LFPG_BEHIND_CAM_Z)
            {
                hud.DrawCrossScreen(wpScr[0], wpScr[1], 0xFF00FFFF, 12.0);
            }
        }

        // v0.7.12 (B1): Draw snap indicator when endpoint is snapped to a port
        if (hasSnap)
        {
            vector snapScr = GetGame().GetScreenPos(cursorPos);
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
        if (GetGame().IsDedicatedServer())
            return false;

        float nowMs = GetGame().GetTime();
        float elapsedSec = (nowMs - s_LastSyncRequestMs) * 0.001;

        if (elapsedSec < SYNC_COOLDOWN_SEC)
        {
            LFPG_Util.Debug("[FullSync] THROTTLED (last " + elapsedSec.ToString() + "s ago, cooldown=" + SYNC_COOLDOWN_SEC.ToString() + "s)");
            return false;
        }

        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player) return false;

        s_LastSyncRequestMs = nowMs;

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.REQUEST_FULL_SYNC);
        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);

        LFPG_Util.Info("[FullSync] Requested (elapsed=" + elapsedSec.ToString() + "s)");
        return true;
    }
};
