// =========================================================
// LF_PowerGrid — Sorter View (Dabs MVC, v2.2 Polish Sprint)
//
// CREATION: LFPG_SorterView.Init() from MissionGameplay.OnInit
//   pre-creates the view HIDDEN (safe context). Avoids
//   RPC-context CreateWidgets corruption.
// OPEN/CLOSE: .Open() shows + pushes data. .Close() hides.
//
// v2.2 changes (Polish Sprint):
//   - Button hover feedback via color cache + OnMouseEnter/Leave
//   - Fade-in animation on open (0.2s alpha lerp)
//   - Enter-to-submit on EditBoxes via OnKeyDown
//   - UI click sounds (SEffectManager)
//   - Visual disabled state when unpaired (IGNOREPOINTER + dim)
//   - Sort feedback in StatusLabel (client-only)
//
// v2.1 changes (Floating Window Sprint):
//   Bug 1-9, drag, double-ESC, pairing banner, DayZ palette
//
// Enforce Script: no ternaries, no ++/--, no foreach.
// =========================================================

class LFPG_SorterView extends ScriptView
{
    protected static ref LFPG_SorterView s_Instance;
    protected bool m_IsOpen;
    protected bool m_FocusLocked;

    // ── Drag state ──
    protected bool m_Dragging;
    protected float m_DragOffX;
    protected float m_DragOffY;

    // ── Hover color cache (v2.2) ──
    // Parallel arrays: widget ref + its base color (no GetColor / no map<Widget> in DayZ)
    protected ref array<Widget> m_CacheWidgets;
    protected ref array<int> m_CacheColors;
    // Currently hovered bg (null if none)
    protected ImageWidget m_HoveredBg;

    // ── Fade-in state (v2.2) ──
    protected float m_FadeAlpha;
    protected bool m_FadingIn;

    // Widget refs for ApplyColors ONLY (no dupes with Controller)
    // ModalOverlay REMOVED (Bug #1)
    Widget SorterPanel;
    Widget HeaderFrame;
    Widget PairingBanner;
    ImageWidget PairingBannerBg;
    ImageWidget PairingDot;
    TextWidget PairingText;
    ImageWidget PanelBg;
    ImageWidget AccentLine;
    ImageWidget HeaderBg;
    ImageWidget TabBarBg;
    ImageWidget TabSep;
    ImageWidget ColumnSep;
    ImageWidget RulesPanelBg;
    ImageWidget PreviewPanelBg;
    ImageWidget FooterBg;
    ImageWidget FooterSep;
    ImageWidget SepCat;
    ImageWidget SepSlot;
    ImageWidget SepCatchAll;
    ImageWidget EditPrefixBg;
    ImageWidget EditContainsBg;
    ImageWidget EditSlotMinBg;
    ImageWidget EditSlotMaxBg;
    ImageWidget DestIndicatorBg;
    ImageWidget MatchFooterBg;
    ImageWidget BtnCloseXBg;
    TextWidget BtnCloseXText;

    static const string PROC_WHITE = "#(argb,8,8,3)color(1,1,1,1,CO)";

    // ── LFPG Palette (ARGB) — DayZ-adjusted (Bug #7/#8/#9) ──
    static const int COL_BG_DEEP      = 0xFF0E1520;
    static const int COL_BG_PANEL     = 0xF50D1528;
    static const int COL_BG_SECTION   = 0xEB101828;
    static const int COL_BG_ELEVATED  = 0xE6162030;
    static const int COL_BG_INPUT     = 0xFF253550;
    static const int COL_GREEN        = 0xFF34D399;
    static const int COL_GREEN_DIM    = 0x0F34D399;
    static const int COL_GREEN_BORDER = 0x2634D399;
    static const int COL_BLUE         = 0xFF60A5FA;
    static const int COL_AMBER        = 0xFFFBBF24;
    static const int COL_RED          = 0xFFF87171;
    static const int COL_BTN          = 0xFF2A3A55;
    static const int COL_TEXT         = 0xFFF1F5F9;
    static const int COL_TEXT_DIM     = 0xFF7A8A9B;
    static const int COL_TEXT_MID     = 0xFFB0BEC5;
    static const int COL_SEPARATOR    = 0x30CBD5E1;
    static const int COL_HEADER       = 0xF50B1120;
    static const int COL_BLUE_BTN     = 0xFF1E3A5F;
    static const int COL_GREEN_BTN    = 0xFF065F46;
    static const int COL_RED_BTN      = 0xFF991B1B;
    static const int COL_PAIRING_OK   = 0x1A34D399;
    static const int COL_PAIRING_ERR  = 0x1AF87171;

    override string GetLayoutFile()
    {
        return "LFPowerGrid/gui/layouts/LF_Sorter.layout";
    }

    override typename GetControllerType()
    {
        return LFPG_SorterController;
    }

    // =========================================================
    // Update: drag, double-ESC, controller timers
    // =========================================================
    override void Update(float dt)
    {
        if (!m_IsOpen)
            return;

        // ── Fade-in animation (v2.2) ──
        if (m_FadingIn)
        {
            m_FadeAlpha = m_FadeAlpha + dt * 5.0;
            if (m_FadeAlpha >= 1.0)
            {
                m_FadeAlpha = 1.0;
                m_FadingIn = false;
            }
            if (SorterPanel)
            {
                SorterPanel.SetAlpha(m_FadeAlpha);
            }
        }

        // ── Drag logic ──
        if (m_Dragging)
        {
            int mx = 0;
            int my = 0;
            GetMousePos(mx, my);
            float newX = mx - m_DragOffX;
            float newY = my - m_DragOffY;
            // Clamp to screen bounds
            int scrW = 0;
            int scrH = 0;
            GetScreenSize(scrW, scrH);
            float panW = 0.0;
            float panH = 0.0;
            if (SorterPanel)
            {
                SorterPanel.GetSize(panW, panH);
            }
            if (newX < 0.0)
            {
                newX = 0.0;
            }
            if (newY < 0.0)
            {
                newY = 0.0;
            }
            float maxX = scrW - panW;
            float maxY = scrH - panH;
            if (newX > maxX)
            {
                newX = maxX;
            }
            if (newY > maxY)
            {
                newY = maxY;
            }
            if (SorterPanel)
            {
                SorterPanel.SetPos(newX, newY);
            }
        }

        // ── Double-ESC pattern ──
        if (GetGame().GetInput())
        {
            if (GetGame().GetInput().LocalPress("UAUIBack", false))
            {
                // First ESC: if an EditBox has focus, just clear focus
                Widget focused = GetFocus();
                if (focused)
                {
                    EditBoxWidget editCheck = EditBoxWidget.Cast(focused);
                    if (editCheck)
                    {
                        SetFocus(null);
                        return;
                    }
                }
                // Second ESC (or no EditBox focused): close
                DoClose();
                return;
            }
        }

        // ── Controller timers ──
        LFPG_SorterController ctrl = LFPG_SorterController.Cast(GetController());
        if (ctrl)
        {
            ctrl.TickTimers(dt);
        }
    }

    void LFPG_SorterView()
    {
        m_IsOpen = false;
        m_FocusLocked = false;
        m_Dragging = false;
        m_DragOffX = 0.0;
        m_DragOffY = 0.0;
        m_CacheWidgets = new array<Widget>();
        m_CacheColors = new array<int>();
        m_HoveredBg = null;
        m_FadeAlpha = 1.0;
        m_FadingIn = false;
    }

    // S1 fix: destructor releases input lock if destroyed while open
    // Prevents permanent stuck-input (CRASH-08) on game shutdown,
    // mission end, or unexpected deletion.
    void ~LFPG_SorterView()
    {
        if (GetGame())
        {
            if (m_FocusLocked)
            {
                Input inp = GetGame().GetInput();
                if (inp)
                {
                    inp.ChangeGameFocus(-1);
                }
                UIManager uiMgr = GetGame().GetUIManager();
                if (uiMgr)
                {
                    uiMgr.ShowUICursor(false);
                }
                m_FocusLocked = false;
            }
        }
    }

    override void OnWidgetScriptInit(Widget w)
    {
        super.OnWidgetScriptInit(w);
        ApplyColors();
    }

    protected void ApplyColors()
    {
        // Bug #1: ModalOverlay removed
        Tint(PanelBg, COL_BG_PANEL);
        Tint(AccentLine, COL_GREEN);
        Tint(HeaderBg, COL_HEADER);
        Tint(TabBarBg, COL_BG_DEEP);
        Tint(TabSep, COL_SEPARATOR);
        Tint(ColumnSep, COL_SEPARATOR);
        Tint(RulesPanelBg, COL_BG_DEEP);
        Tint(PreviewPanelBg, COL_BG_DEEP);
        Tint(FooterBg, COL_BG_PANEL);
        Tint(FooterSep, COL_SEPARATOR);
        Tint(SepCat, COL_SEPARATOR);
        Tint(SepSlot, COL_SEPARATOR);
        Tint(SepCatchAll, COL_SEPARATOR);
        Tint(EditPrefixBg, COL_BG_INPUT);
        Tint(EditContainsBg, COL_BG_INPUT);
        Tint(EditSlotMinBg, COL_BG_INPUT);
        Tint(EditSlotMaxBg, COL_BG_INPUT);
        Tint(DestIndicatorBg, COL_GREEN_DIM);
        Tint(MatchFooterBg, COL_SEPARATOR);
        // BtnCloseX default color
        Tint(BtnCloseXBg, COL_BTN);
        if (BtnCloseXText)
        {
            BtnCloseXText.SetColor(COL_TEXT_DIM);
        }
        // PairingBanner default (unpaired)
        Tint(PairingBannerBg, COL_PAIRING_ERR);
        Tint(PairingDot, COL_RED);
        if (PairingText)
        {
            PairingText.SetColor(COL_RED);
            PairingText.SetText("No container linked");
        }
    }

    protected void Tint(ImageWidget img, int color)
    {
        if (!img)
            return;
        img.LoadImageFile(0, PROC_WHITE);
        img.SetColor(color);
        // Cache for hover system (v2.2)
        CacheColorLocal(img, color);
    }

    // Store/update color for a widget in parallel arrays
    protected void CacheColorLocal(Widget w, int color)
    {
        if (!w)
            return;
        if (!m_CacheWidgets)
            return;
        int i = 0;
        int count = m_CacheWidgets.Count();
        for (i = 0; i < count; i = i + 1)
        {
            if (m_CacheWidgets[i] == w)
            {
                m_CacheColors[i] = color;
                return;
            }
        }
        m_CacheWidgets.Insert(w);
        m_CacheColors.Insert(color);
    }

    // Find cached color for a widget, returns -1 if not found
    protected int FindCachedColor(Widget w)
    {
        if (!w)
            return -1;
        if (!m_CacheWidgets)
            return -1;
        int i = 0;
        int count = m_CacheWidgets.Count();
        for (i = 0; i < count; i = i + 1)
        {
            if (m_CacheWidgets[i] == w)
            {
                return m_CacheColors[i];
            }
        }
        return -1;
    }

    // =========================================================
    // Static color cache accessor (called from Controller.TintBg)
    // =========================================================
    static void CacheColor(Widget w, int color)
    {
        if (!s_Instance)
            return;
        if (!w)
            return;
        s_Instance.CacheColorLocal(w, color);
    }

    // =========================================================
    // Drag: MouseDown on header starts drag (Bug #4)
    // =========================================================
    override bool OnMouseButtonDown(Widget w, int x, int y, int button)
    {
        if (!m_IsOpen)
            return false;
        if (button != 0)
            return false;

        // Check if the click is on the header (drag handle)
        if (IsHeaderWidget(w))
        {
            m_Dragging = true;
            float px = 0.0;
            float py = 0.0;
            if (SorterPanel)
            {
                SorterPanel.GetPos(px, py);
            }
            m_DragOffX = x - px;
            m_DragOffY = y - py;
            return true;
        }

        return super.OnMouseButtonDown(w, x, y, button);
    }

    override bool OnMouseButtonUp(Widget w, int x, int y, int button)
    {
        if (button == 0)
        {
            m_Dragging = false;
        }
        return super.OnMouseButtonUp(w, x, y, button);
    }

    // Walk the parent chain to see if widget is in the HeaderFrame
    // Stops if we hit a ButtonWidget (don't drag on buttons inside header)
    protected bool IsHeaderWidget(Widget w)
    {
        if (!w)
            return false;
        if (!HeaderFrame)
            return false;

        Widget check = w;
        ButtonWidget btnCheck = null;
        while (check)
        {
            // If we hit a button first, it's a button click, not drag
            btnCheck = ButtonWidget.Cast(check);
            if (btnCheck)
            {
                return false;
            }
            if (check == HeaderFrame)
            {
                return true;
            }
            check = check.GetParent();
        }
        return false;
    }

    // =========================================================
    // Center panel on screen (called on open)
    // =========================================================
    protected void CenterPanel()
    {
        if (!SorterPanel)
            return;
        int scrW = 0;
        int scrH = 0;
        GetScreenSize(scrW, scrH);
        float panW = 0.0;
        float panH = 0.0;
        SorterPanel.GetSize(panW, panH);
        float cx = (scrW - panW) * 0.5;
        float cy = (scrH - panH) * 0.5;
        SorterPanel.SetPos(cx, cy);
    }

    // =========================================================
    // Pairing state visual update (Bug #5)
    // =========================================================
    void UpdatePairingState(string containerDisplayName)
    {
        bool paired = false;
        if (containerDisplayName != "")
        {
            paired = true;
        }

        if (paired)
        {
            Tint(PairingBannerBg, COL_PAIRING_OK);
            Tint(PairingDot, COL_GREEN);
            if (PairingText)
            {
                string linkedMsg = "Linked: " + containerDisplayName;
                PairingText.SetText(linkedMsg);
                PairingText.SetColor(COL_GREEN);
            }
        }
        else
        {
            Tint(PairingBannerBg, COL_PAIRING_ERR);
            Tint(PairingDot, COL_RED);
            if (PairingText)
            {
                PairingText.SetText("No container linked");
                PairingText.SetColor(COL_RED);
            }
        }
    }

    // =========================================================
    // Singleton lifecycle
    // =========================================================
    static void Init()
    {
        #ifndef SERVER
        if (s_Instance)
            return;
        s_Instance = new LFPG_SorterView();
        Widget root = s_Instance.GetLayoutRoot();
        if (root)
        {
            root.Show(false);
            root.SetSort(10003);
        }
        LFPG_Util.Info("[SorterView] Pre-created (hidden)");
        #endif
    }

    // Called from RPC — widgets already exist, just show + data
    static void Open(string configJSON, string containerName, string d0, string d1, string d2, string d3, string d4, string d5, int netLow, int netHigh)
    {
        #ifndef SERVER
        if (!s_Instance)
        {
            LFPG_Util.Warn("[SorterView] Open before Init — cannot open");
            return;
        }
        s_Instance.DoOpen(configJSON, containerName, d0, d1, d2, d3, d4, d5, netLow, netHigh);
        #endif
    }

    static void Close()
    {
        if (s_Instance)
        {
            s_Instance.DoClose();
        }
    }

    static bool IsOpen()
    {
        if (!s_Instance)
            return false;
        return s_Instance.m_IsOpen;
    }

    // S2 fix: Cleanup deletes instance properly (prevents leak).
    // Input release is handled by the destructor (S1) which has
    // GetGame() null guard for safe shutdown. Do NOT call HideCursor()
    // here — it lacks GetGame() check and would crash during late shutdown.
    static void Cleanup()
    {
        if (s_Instance)
        {
            s_Instance.m_IsOpen = false;
            s_Instance.m_Dragging = false;
            s_Instance.m_FadingIn = false;
            // delete triggers ~LFPG_SorterView (input release) then
            // ~ScriptView (Unlink + removes from All)
            delete s_Instance;
        }
        s_Instance = null;
    }

    static void OnSaveAck(bool success)
    {
        if (!s_Instance)
            return;
        if (!s_Instance.m_IsOpen)
            return;
        LFPG_SorterController ctrl = LFPG_SorterController.Cast(s_Instance.GetController());
        if (ctrl)
        {
            ctrl.HandleSaveAck(success);
        }
    }

    protected void DoOpen(string configJSON, string containerName, string d0, string d1, string d2, string d3, string d4, string d5, int netLow, int netHigh)
    {
        if (m_IsOpen)
            return;
        Widget root = GetLayoutRoot();
        if (!root)
        {
            LFPG_Util.Error("[SorterView] No layout root");
            return;
        }
        m_IsOpen = true;
        m_Dragging = false;
        m_HoveredBg = null;
        root.Show(true);
        CenterPanel();

        // Fade-in (v2.2)
        m_FadeAlpha = 0.0;
        m_FadingIn = true;
        if (SorterPanel)
        {
            SorterPanel.SetAlpha(0.0);
        }

        ShowCursor();

        // Update pairing banner (Bug #5)
        UpdatePairingState(containerName);

        LFPG_SorterController ctrl = LFPG_SorterController.Cast(GetController());
        if (ctrl)
        {
            ctrl.InitFromRPC(configJSON, containerName, d0, d1, d2, d3, d4, d5, netLow, netHigh);
        }
        LFPG_Util.Info("[SorterView] Opened for: " + containerName);
    }

    protected void DoClose()
    {
        if (!m_IsOpen)
            return;
        m_IsOpen = false;
        m_Dragging = false;
        m_FadingIn = false;
        m_HoveredBg = null;
        Widget root = GetLayoutRoot();
        if (root)
        {
            root.Show(false);
        }
        HideCursor();
        LFPG_Util.Info("[SorterView] Closed");
    }

    // =========================================================
    // Bug #2 fix: NO INPUT_EXCLUDE_ALL — cursor-only focus
    // =========================================================
    protected void ShowCursor()
    {
        #ifndef SERVER
        UIManager uiMgr = GetGame().GetUIManager();
        if (uiMgr)
        {
            uiMgr.ShowUICursor(true);
        }
        if (!m_FocusLocked)
        {
            Input inp = GetGame().GetInput();
            if (inp)
            {
                inp.ChangeGameFocus(1);
            }
            m_FocusLocked = true;
        }
        #endif
    }

    protected void HideCursor()
    {
        #ifndef SERVER
        UIManager uiMgr = GetGame().GetUIManager();
        if (uiMgr)
        {
            uiMgr.ShowUICursor(false);
        }
        if (m_FocusLocked)
        {
            Input inp = GetGame().GetInput();
            if (inp)
            {
                inp.ChangeGameFocus(-1);
            }
            m_FocusLocked = false;
        }
        #endif
    }

    // =========================================================
    // Hover feedback (v2.2) — lighten button bg on mouse enter
    // =========================================================
    override bool OnMouseEnter(Widget w, int x, int y)
    {
        if (!m_IsOpen)
            return false;

        ImageWidget bg = FindButtonBg(w);
        int baseColor = 0;
        int hoverColor = 0;
        if (bg)
        {
            // Restore previous hover first (guards against Enter-before-Leave race)
            if (m_HoveredBg && m_HoveredBg != bg)
            {
                baseColor = FindCachedColor(m_HoveredBg);
                if (baseColor >= 0)
                {
                    m_HoveredBg.SetColor(baseColor);
                }
                m_HoveredBg = null;
                baseColor = 0;
            }

            baseColor = FindCachedColor(bg);
            if (baseColor >= 0)
            {
                m_HoveredBg = bg;
                hoverColor = LightenARGB(baseColor, 20);
                bg.SetColor(hoverColor);
            }
        }
        return false;
    }

    override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
    {
        int baseColor = 0;
        if (m_HoveredBg)
        {
            baseColor = FindCachedColor(m_HoveredBg);
            if (baseColor >= 0)
            {
                m_HoveredBg.SetColor(baseColor);
            }
            m_HoveredBg = null;
        }
        return false;
    }

    // Walk up from w to find enclosing ButtonWidget, then return first ImageWidget child
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
            {
                break;
            }
            check = check.GetParent();
        }

        if (!btn)
            return null;

        // First child is the Bg ImageWidget
        Widget child = btn.GetChildren();
        if (!child)
            return null;

        ImageWidget bg = ImageWidget.Cast(child);
        return bg;
    }

    // Lighten an ARGB color by adding 'amount' to RGB channels (clamped to 255)
    static int LightenARGB(int color, int amount)
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

    // =========================================================
    // Enter-to-submit on EditBoxes (v2.2)
    // =========================================================
    override bool OnKeyDown(Widget w, int x, int y, int key)
    {
        if (!m_IsOpen)
            return false;

        Widget focused = null;
        LFPG_SorterController ctrl = null;
        string wName = "";

        // KC_RETURN = 28
        if (key == KeyCode.KC_RETURN)
        {
            focused = GetFocus();
            if (!focused)
                return false;

            ctrl = LFPG_SorterController.Cast(GetController());
            if (!ctrl)
                return false;

            // Match focused widget to the known EditBoxes
            wName = focused.GetName();
            if (wName == "EditPrefix")
            {
                ctrl.BtnPrefixAdd();
                return true;
            }
            if (wName == "EditContains")
            {
                ctrl.BtnContainsAdd();
                return true;
            }
            if (wName == "EditSlotMin" || wName == "EditSlotMax")
            {
                ctrl.BtnSlotAdd();
                return true;
            }
        }

        return false;
    }

    // =========================================================
    // UI Sound helper (v2.2)
    // =========================================================
    static void PlayUIClick()
    {
        #ifndef SERVER
        if (!GetGame())
            return;

        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player)
            return;

        SEffectManager.PlaySoundOnObject("pickUpItem_SoundSet", player);
        #endif
    }

    static void PlayUIAction()
    {
        #ifndef SERVER
        if (!GetGame())
            return;

        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player)
            return;

        SEffectManager.PlaySoundOnObject("attachmentAdded_SoundSet", player);
        #endif
    }

    LFPG_SorterController GetSorterController()
    {
        return LFPG_SorterController.Cast(GetController());
    }
};
