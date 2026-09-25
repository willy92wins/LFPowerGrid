#ifndef SERVER
// Shared event behavior; widget bindings, color caches and close timestamps stay in each view.
class LFPG_FloatingViewBase extends ScriptView
{
    protected bool m_IsOpen;
    protected bool m_Dragging;
    protected float m_DragOffX;
    protected float m_DragOffY;
    protected ImageWidget m_HoveredBg;

    protected Widget LFPG_ViewPanel()
    {
        return null;
    }
    protected Widget LFPG_ViewHeader()
    {
        return null;
    }
    protected bool LFPG_ViewIncludesScroll()
    {
        return false;
    }
    protected bool LFPG_ViewAllowsHover()
    {
        return true;
    }
    protected int FindCachedColor(Widget w)
    {
        return 0;
    }

    override bool OnMouseButtonDown(Widget w, int x, int y, int button)
    {
        Widget panel = LFPG_ViewPanel();
        if (!m_IsOpen)
            return false;

        if (button == 0)
        {
            if (IsHeaderWidget(w))
            {
                m_Dragging = true;
                if (m_HoveredBg)
                {
                    int restoreCol = FindCachedColor(m_HoveredBg);
                    if (restoreCol != 0)
                        m_HoveredBg.SetColor(restoreCol);
                    m_HoveredBg = null;
                }
                float px = 0.0;
                float py = 0.0;
                if (panel)
                    panel.GetPos(px, py);
                m_DragOffX = x - px;
                m_DragOffY = y - py;
            }
        }

        if (IsInteractiveWidget(w))
            return false;
        return true;
    }

    override bool OnMouseButtonUp(Widget w, int x, int y, int button)
    {
        if (button == 0)
            m_Dragging = false;
        if (!m_IsOpen)
            return false;

        if (IsInteractiveWidget(w))
            return false;
        return true;
    }

    override bool OnMouseEnter(Widget w, int x, int y)
    {
        if (!m_IsOpen)
            return false;

        if (!LFPG_ViewAllowsHover())
            return false;
        ImageWidget bg = FindButtonBg(w);
        int baseColor = 0;
        int hoverColor = 0;
        if (bg)
        {
            if (m_HoveredBg && m_HoveredBg != bg)
            {
                baseColor = FindCachedColor(m_HoveredBg);
                if (baseColor != 0)
                    m_HoveredBg.SetColor(baseColor);
                m_HoveredBg = null;
                baseColor = 0;
            }
            baseColor = FindCachedColor(bg);
            if (baseColor != 0)
            {
                m_HoveredBg = bg;
                hoverColor = LFPG_SharedLightenARGB(baseColor, 20);
                bg.SetColor(hoverColor);
            }
        }
        return false;
    }

    override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
    {
        if (!m_IsOpen)
            return false;

        int baseColor = 0;
        if (m_HoveredBg)
        {
            baseColor = FindCachedColor(m_HoveredBg);
            if (baseColor != 0)
                m_HoveredBg.SetColor(baseColor);
            m_HoveredBg = null;
        }
        return false;
    }

    protected bool IsHeaderWidget(Widget w)
    {
        Widget header = LFPG_ViewHeader();
        if (!w)
            return false;
        if (!header)
            return false;

        Widget check = w;
        ButtonWidget btnCheck = null;
        while (check)
        {
            btnCheck = ButtonWidget.Cast(check);
            if (btnCheck)
                return false;
            if (check == header)
                return true;
            check = check.GetParent();
        }
        return false;
    }

    protected bool IsInteractiveWidget(Widget w)
    {
        if (!w)
            return false;

        Widget check = w;
        ButtonWidget btnCast = null;
        EditBoxWidget editCast = null;
        ScrollWidget scrollCast = null;
        bool includeScroll = LFPG_ViewIncludesScroll();
        while (check)
        {
            btnCast = ButtonWidget.Cast(check);
            if (btnCast)
                return true;
            editCast = EditBoxWidget.Cast(check);
            if (editCast)
                return true;
            if (includeScroll)
            {
                scrollCast = ScrollWidget.Cast(check);
                if (scrollCast)
                    return true;
            }
            check = check.GetParent();
        }
        return false;
    }

    protected ImageWidget FindButtonBg(Widget w)
    {
        if (!w)
            return null;

        Widget check = w;
        ButtonWidget btn = null;
        while (check)
        {
            btn = ButtonWidget.Cast(check);
            if (btn)
                break;
            check = check.GetParent();
        }
        if (!btn)
            return null;

        Widget child = btn.GetChildren();
        if (!child)
            return null;

        return ImageWidget.Cast(child);
    }

    protected void ClampPanelPos(float inX, float inY, float minY, out float outX, out float outY)
    {
        Widget panel = LFPG_ViewPanel();
        int scrW = 0;
        int scrH = 0;
        GetScreenSize(scrW, scrH);
        float panW = 0.0;
        float panH = 0.0;
        if (panel)
            panel.GetSize(panW, panH);

        float maxX = scrW - panW;
        float maxY = scrH - panH;
        float dpiCapX = panW;
        float dpiCapY = panH * 0.5;
        if (maxX > dpiCapX) { maxX = dpiCapX; }
        if (maxY > dpiCapY) { maxY = dpiCapY; }
        if (maxX < 0.0) { maxX = 0.0; }
        if (maxY < minY) { maxY = minY; }

        outX = inX;
        outY = inY;
        if (outX < 0.0) { outX = 0.0; }
        if (outY < minY) { outY = minY; }
        if (outX > maxX) { outX = maxX; }
        if (outY > maxY) { outY = maxY; }
    }

    protected void CenterPanel()
    {
        Widget panel = LFPG_ViewPanel();
        if (!panel)
            return;
        int scrW = 0;
        int scrH = 0;
        GetScreenSize(scrW, scrH);
        float panW = 0.0;
        float panH = 0.0;
        panel.GetSize(panW, panH);

        float cx = (scrW - panW) * 0.5;
        float cy = (scrH - panH) * 0.5;
        float clampedX = 0.0;
        float clampedY = 0.0;
        float minY = 0.0;
        ClampPanelPos(cx, cy, minY, clampedX, clampedY);
        panel.SetPos(clampedX, clampedY);
    }

    static int LFPG_SharedLightenARGB(int color, int amount)
    {
        int a = (color >> 24) & 0xFF;
        int r = (color >> 16) & 0xFF;
        int g = (color >> 8) & 0xFF;
        int b = color & 0xFF;

        r = r + amount;
        g = g + amount;
        b = b + amount;

        if (r > 255) { r = 255; }
        if (g > 255) { g = 255; }
        if (b > 255) { b = 255; }

        return (a << 24) | (r << 16) | (g << 8) | b;
    }

    static bool LFPG_SharedEscCooldown(float lastCloseTime)
    {
        if (lastCloseTime <= 0.0)
            return false;
        if (!g_Game)
            return false;
        float now = g_Game.GetTickTime();
        float elapsed = now - lastCloseTime;
        if (elapsed < 0.2)
            return true;
        return false;
    }
}
#endif
