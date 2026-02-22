// =========================================================
// LF_PowerGrid - Cable HUD (2D line overlay) (v0.7.11)
//
// Uses CanvasWidget to draw cable lines on screen.
// Works on RETAIL client (Shape.CreateLines only works in Diag EXE).
//
// Initializes via layout file (gui/layouts/cable_hud.layout).
//
// v0.6.3: Cached GetScreenSize per frame to avoid redundant
//         calls in DrawSegment/DrawCross.
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

    // Per-frame cached screen size as floats (used in DrawSegment/DrawCross)
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
        Widget root = ws.CreateWidgets("LF_PowerGrid/gui/layouts/cable_hud.layout");
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

        // Cache screen size once per frame (avoids redundant calls in
        // DrawSegment and DrawCross which previously called GetScreenSize
        // individually — up to ~50 calls/frame with many cables).
        int curW = 0;
        int curH = 0;
        GetScreenSize(curW, curH);

        // v0.7.10: Guard against transient 0x0 resolution (alt-tab, loading,
        // resolution change). Keep last valid dimensions instead of corrupting
        // cache, which would cause division issues in margin/extScale calcs.
        if (curW <= 0 || curH <= 0)
            return;

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

    // Draw a 3D line segment projected onto the 2D screen.
    // color: ARGB int (e.g. 0xFFFF0000 = red)
    // width: line width in pixels (2-4 recommended)
    void DrawSegment(vector worldA, vector worldB, int color, float width)
    {
        if (!m_Ready || !m_Canvas)
            return;

        // Project 3D to screen coords
        vector sa = GetGame().GetScreenPos(worldA);
        vector sb = GetGame().GetScreenPos(worldB);

        // Behind camera check: z < 0.1 means at or behind camera plane
        bool behindA = (sa[2] < 0.1);
        bool behindB = (sb[2] < 0.1);

        // Diagnostic: log first few segments per frame for debugging
        if (m_SegmentsDrawn < 2 && m_FrameNum % 300 == 1)
        {
            string dbg = "[HUD-Draw] worldA=" + worldA.ToString() + " sA=" + sa.ToString();
            dbg = dbg + " worldB=" + worldB.ToString() + " sB=" + sb.ToString();
            dbg = dbg + " behindA=" + behindA.ToString() + " behindB=" + behindB.ToString();
            LFPG_Diag.ServerEcho(dbg);
        }

        // Both behind camera: completely invisible
        if (behindA && behindB)
        {
            m_SegmentsCulled = m_SegmentsCulled + 1;
            return;
        }

        // Use per-frame cached screen size (set in BeginFrame)
        float swF = m_ScreenWF;
        float shF = m_ScreenHF;

        // v0.7.6: Fixed behind-camera clamp.
        // When GetScreenPos projects a behind-camera point, the returned x/y
        // are "inverted" through the vanishing point. The old code extended
        // toward screen center which produced wild diagonal lines.
        // Fix: extend from the visible point AWAY from the inverted position
        // of the behind-camera point — i.e. in direction (visible - inverted)
        // — which follows the actual cable geometry on screen.
        if (behindA)
        {
            float dxBA = sb[0] - sa[0];
            float dyBA = sb[1] - sa[1];
            float lenBA = Math.Sqrt(dxBA * dxBA + dyBA * dyBA);
            if (lenBA < 1.0)
            {
                sa[0] = sb[0];
                sa[1] = sb[1] - swF;
            }
            else
            {
                float extA = (swF + shF) / lenBA;
                sa[0] = sb[0] + dxBA * extA;
                sa[1] = sb[1] + dyBA * extA;
            }
        }
        else if (behindB)
        {
            float dxAB = sa[0] - sb[0];
            float dyAB = sa[1] - sb[1];
            float lenAB = Math.Sqrt(dxAB * dxAB + dyAB * dyAB);
            if (lenAB < 1.0)
            {
                sb[0] = sa[0];
                sb[1] = sa[1] - swF;
            }
            else
            {
                float extB = (swF + shF) / lenAB;
                sb[0] = sa[0] + dxAB * extB;
                sb[1] = sa[1] + dyAB * extB;
            }
        }

        // Screen bounds check with resolution-proportional margin.
        // v0.7.9: Fixed 400px was excessive at 720p and tight at 4K.
        // Using 25% of screen height (min 200px) scales naturally.
        float margin = shF * 0.25;
        if (margin < 200.0)
        {
            margin = 200.0;
        }

        bool offA = false;
        if (sa[0] < -margin)
            offA = true;
        if (sa[0] > swF + margin)
            offA = true;
        if (sa[1] < -margin)
            offA = true;
        if (sa[1] > shF + margin)
            offA = true;

        bool offB = false;
        if (sb[0] < -margin)
            offB = true;
        if (sb[0] > swF + margin)
            offB = true;
        if (sb[1] < -margin)
            offB = true;
        if (sb[1] > shF + margin)
            offB = true;

        if (offA && offB)
        {
            m_SegmentsCulled = m_SegmentsCulled + 1;
            return;
        }

        m_Canvas.DrawLine(sa[0], sa[1], sb[0], sb[1], width, color);
        m_SegmentsDrawn = m_SegmentsDrawn + 1;
    }

    void EndFrame()
    {
        // Periodic diagnostic
        if (m_FrameNum % 300 == 0)
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


    // Draw a small cross marker on screen at a world position
    void DrawCross(vector worldPos, int color, float size)
    {
        if (!m_Ready || !m_Canvas)
            return;

        vector sp = GetGame().GetScreenPos(worldPos);

        // Behind camera or at camera plane
        if (sp[2] < 0.1)
            return;

        float x = sp[0];
        float y = sp[1];

        // Use per-frame cached screen size
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
