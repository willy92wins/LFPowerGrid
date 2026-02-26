// =========================================================
// LF_PowerGrid - Cable HUD (2D line overlay) (v0.7.38)
//
// v0.7.38 (Audit) changes:
//   H2 — Removed DrawSegment dead code (113 lines, zero callers).
//   M6 — EndFrame diagnostic guarded with LFPG_DIAG_ENABLED.
//   M12 — Canvas.Clear before early return in BeginFrame (alt-tab fix).
//
// Uses CanvasWidget to draw cable lines on screen.
// Works on RETAIL client (Shape.CreateLines only works in Diag EXE).
//
// Initializes via layout file (gui/layouts/cable_hud.layout).
//
// v0.6.3: Cached GetScreenSize per frame to avoid redundant calls.
// =========================================================

class LFPG_CableHUD
{
    protected static ref LFPG_CableHUD s_Instance;

    protected CanvasWidget m_Canvas;
    protected bool m_Ready = false;
    protected bool m_InitAttempted = false;

    protected int m_FrameNum = 0;

    // Track canvas dimensions for resize detection (int for SetSize)
    protected int m_CanvasW = 0;
    protected int m_CanvasH = 0;

    // Per-frame cached screen size as floats (used by CableRenderer/WiringClient)
    protected float m_ScreenWF = 0.0;
    protected float m_ScreenHF = 0.0;

    // Stats for diagnostics
    protected int m_SegmentsDrawn = 0;
    protected int m_SegmentsCulled = 0;


    static LFPG_CableHUD Get()
    {
        if (!s_Instance)
        {
            s_Instance = new LFPG_CableHUD();
        }
        return s_Instance;
    }

    void LFPG_CableHUD()
    {
    }

    // v0.7.5: proper cleanup on destruction.
    // Prevents widget leak when player disconnects/reconnects
    // or exits to main menu. Without this, the CanvasWidget
    // lingers in the workspace and m_InitAttempted stays true
    // with a stale pointer in the next session.
    void ~LFPG_CableHUD()
    {
        if (m_Canvas)
        {
            m_Canvas.Unlink();
            m_Canvas = null;
        }
        m_Ready = false;
        m_InitAttempted = false;
    }

    // v0.7.5: called from MissionGameplay.OnInit to ensure a
    // clean slate when reconnecting to a server. Destroys the
    // previous singleton so the next Get() creates a fresh one
    // with m_InitAttempted = false and no stale canvas reference.
    static void Reset()
    {
        if (s_Instance)
        {
            // destructor handles canvas cleanup
            delete s_Instance;
            s_Instance = null;
        }
    }

    bool IsReady()
    {
        return m_Ready;
    }

    // v0.7.9: expose cached screen dimensions for CableRenderer.DrawFrame
    // to reuse (avoids redundant GetScreenSize calls per frame).
    float GetScreenW()
    {
        return m_ScreenWF;
    }

    float GetScreenH()
    {
        return m_ScreenHF;
    }

    // ---- Initialization via layout file ----
    // v0.7.9: m_InitAttempted set only after definitive outcome.
    // Allows one retry if CreateWidgets fails transiently during layout loading.
    protected void TryInit()
    {
        if (m_InitAttempted)
            return;

        if (GetGame().IsDedicatedServer())
        {
            m_InitAttempted = true;
            return;
        }

        // Defer until player exists (UI guaranteed ready)
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player)
            return;

        WorkspaceWidget ws = GetGame().GetWorkspace();
        if (!ws)
            return;

        // Load CanvasWidget from layout
        Widget root = ws.CreateWidgets("LFPowerGrid/gui/layouts/cable_hud.layout");
        if (root)
        {
            m_Canvas = CanvasWidget.Cast(root);
            if (!m_Canvas)
            {
                m_Canvas = CanvasWidget.Cast(root.FindAnyWidget("CableCanvas"));
            }
        }

        if (m_Canvas)
        {
            // Success — mark as attempted
            m_InitAttempted = true;

            // Force pixel-based position and size.
            int canvasW = 0;
            int canvasH = 0;
            GetScreenSize(canvasW, canvasH);
            m_Canvas.SetPos(0, 0);
            m_Canvas.SetSize(canvasW, canvasH);
            m_CanvasW = canvasW;
            m_CanvasH = canvasH;
            m_ScreenWF = canvasW;
            m_ScreenHF = canvasH;

            // Force visible + on top of everything
            m_Canvas.Show(true);
            m_Canvas.SetSort(10000);
            m_Ready = true;
            LFPG_Util.Info("[CableHUD] Canvas OK size=" + canvasW.ToString() + "x" + canvasH.ToString() + " sort=10000");
            LFPG_Diag.ServerEcho("[CableHUD] Canvas OK " + canvasW.ToString() + "x" + canvasH.ToString());
        }
        else
        {
            // Definitive failure — mark as attempted to prevent infinite retries
            m_InitAttempted = true;

            string rootInfo = "null";
            if (root)
            {
                rootInfo = "exists";
            }
            LFPG_Util.Warn("[CableHUD] No canvas - lines disabled");
            LFPG_Diag.ServerEcho("[CableHUD] No canvas root=" + rootInfo);
        }
    }

    // ---- Per-frame API ----

    void BeginFrame()
    {
        if (!m_Ready)
        {
            TryInit();
        }

        if (!m_Ready || !m_Canvas)
            return;

        // Cache screen size once per frame (avoids redundant calls).
        int curW = 0;
        int curH = 0;
        GetScreenSize(curW, curH);

        // v0.7.38 (M12): Guard against transient 0x0 resolution (alt-tab, loading,
        // resolution change). Clear canvas first to avoid frozen last-frame artifacts,
        // then return without updating dimensions (keeps last valid size).
        if (curW <= 0 || curH <= 0)
        {
            m_Canvas.Clear();
            return;
        }

        m_ScreenWF = curW;
        m_ScreenHF = curH;

        // Detect resolution change and resize canvas
        if (curW != m_CanvasW || curH != m_CanvasH)
        {
            m_CanvasW = curW;
            m_CanvasH = curH;
            m_Canvas.SetSize(curW, curH);
        }

        m_Canvas.Clear();
        m_FrameNum = m_FrameNum + 1;
        m_SegmentsDrawn = 0;
        m_SegmentsCulled = 0;
    }

    // v0.7.38 (H2): DrawSegment removed — dead code since v0.7.9.
    // Superseded by DrawLineScreen which accepts pre-projected screen coords.
    // Active path: CableRenderer.DrawFrame → Phase 1 projection → DrawLineScreen.

    void EndFrame()
    {
        // v0.7.38 (M6): Guard diagnostic log. Counters are redundant with
        // LFPG_Telemetry.m_SegmentsDrawn but kept for HUD-level debugging.
        if (LFPG_DIAG_ENABLED && m_FrameNum % 300 == 0)
        {
            string msg = "[CableHUD] frame=" + m_FrameNum.ToString();
            msg = msg + " drawn=" + m_SegmentsDrawn.ToString();
            msg = msg + " culled=" + m_SegmentsCulled.ToString();
            msg = msg + " ready=" + m_Ready.ToString();
            LFPG_Diag.ServerEcho(msg);
        }
    }

    // v0.7.6: Clear all drawn lines from the canvas.
    // Called when the wiring session ends to prevent leftover
    // preview lines from persisting on screen indefinitely.
    // Without this, the last frame's lines remain because
    // BeginFrame (which calls m_Canvas.Clear) is only called
    // while the session is active.
    void ClearCanvas()
    {
        if (!m_Ready || !m_Canvas)
            return;

        m_Canvas.Clear();
    }

    // v0.7.8: Draw a small diamond at a world position (joint marker).
    // v0.7.9: DrawJoint (world-space) and DrawEndcap (world-space) removed.
    // Both had been superseded by DrawJointScreen and DrawEndcapScreen which
    // accept pre-projected screen coords and avoid redundant GetScreenPos calls.
    // The old DrawEndcap also had a dirWorld normalization issue (audit point 2).

    // v0.7.10: Draw a cross marker in pre-projected screen coordinates.
    // Used by WiringClient preview for waypoint markers (avoids GetScreenPos per marker).
    void DrawCrossScreen(float x, float y, int color, float size)
    {
        if (!m_Ready || !m_Canvas)
            return;

        // NaN guard
        if (x != x || y != y)
            return;

        float swF = m_ScreenWF;
        float shF = m_ScreenHF;

        if (x < -50.0)
            return;
        if (x > swF + 50.0)
            return;
        if (y < -50.0)
            return;
        if (y > shF + 50.0)
            return;

        m_Canvas.DrawLine(x - size, y, x + size, y, 2.0, color);
        m_Canvas.DrawLine(x, y - size, x, y + size, 2.0, color);
    }

    // v0.7.9: Draw a line directly in screen coordinates.
    // Bypasses GetScreenPos projection (caller pre-projects).
    // Used by CableRenderer.DrawFrame for multi-pass efficiency.
    void DrawLineScreen(float x1, float y1, float x2, float y2, float width, int color)
    {
        if (!m_Ready || !m_Canvas)
            return;

        // v0.7.9: NaN guard — corrupted projection can produce NaN coords
        if (x1 != x1 || y1 != y1 || x2 != x2 || y2 != y2)
            return;

        m_Canvas.DrawLine(x1, y1, x2, y2, width, color);
        m_SegmentsDrawn = m_SegmentsDrawn + 1;
    }

    // v0.7.9: Draw a diamond joint in screen coordinates.
    void DrawJointScreen(float x, float y, float halfSize, int color)
    {
        if (!m_Ready || !m_Canvas)
            return;

        // v0.7.10: NaN guard (matches DrawLineScreen/DrawCrossScreen)
        if (x != x || y != y)
            return;

        m_Canvas.DrawLine(x, y - halfSize, x + halfSize, y, 2.0, color);
        m_Canvas.DrawLine(x + halfSize, y, x, y + halfSize, 2.0, color);
        m_Canvas.DrawLine(x, y + halfSize, x - halfSize, y, 2.0, color);
        m_Canvas.DrawLine(x - halfSize, y, x, y - halfSize, 2.0, color);
        m_SegmentsDrawn = m_SegmentsDrawn + 4;
    }

    // v0.7.9: Draw a perpendicular endcap tick in screen coordinates.
    // perpX/perpY: pre-computed perpendicular unit vector in screen space.
    void DrawEndcapScreen(float x, float y, float perpX, float perpY, float halfLen, float width, int color)
    {
        if (!m_Ready || !m_Canvas)
            return;

        // v0.7.10: NaN guard (coords + perpendicular vector)
        if (x != x || y != y || perpX != perpX || perpY != perpY)
            return;

        float x1 = x + perpX * halfLen;
        float y1 = y + perpY * halfLen;
        float x2 = x - perpX * halfLen;
        float y2 = y - perpY * halfLen;

        m_Canvas.DrawLine(x1, y1, x2, y2, width, color);
        m_SegmentsDrawn = m_SegmentsDrawn + 1;
    }

};
