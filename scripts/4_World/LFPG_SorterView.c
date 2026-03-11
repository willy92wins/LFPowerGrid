// =========================================================
// LF_PowerGrid — Sorter View (Dabs MVC, v2.1 Floating Window)
//
// CREATION: LFPG_SorterView.Init() from MissionGameplay.OnInit
//   pre-creates the view HIDDEN (safe context). Avoids
//   RPC-context CreateWidgets corruption.
// OPEN/CLOSE: .Open() shows + pushes data. .Close() hides.
//
// v2.1 changes (Floating Window Sprint):
//   Bug 1: ModalOverlay removed, root ignorepointer, panel captures
//   Bug 2: INPUT_EXCLUDE_ALL removed, cursor-only focus
//   Bug 3: BtnCloseX in header
//   Bug 4: Drag on HeaderFrame with screen clamping
//   Bug 5: PairingBanner shows linked container status
//   Bug 7-9: Color palette bumped for DayZ gamma
//   Double-ESC: first clears EditBox focus, second closes
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
        while (check)
        {
            // If we hit a button first, it's a button click, not drag
            ButtonWidget btnCheck = ButtonWidget.Cast(check);
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

    static void Cleanup()
    {
        if (s_Instance)
        {
            s_Instance.m_IsOpen = false;
            s_Instance.m_FocusLocked = false;
            s_Instance.m_Dragging = false;
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
        root.Show(true);
        CenterPanel();
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

    LFPG_SorterController GetSorterController()
    {
        return LFPG_SorterController.Cast(GetController());
    }
};
