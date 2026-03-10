// =========================================================
// LF_PowerGrid — Sorter View (Dabs MVC, v2.0)
//
// ScriptView that loads LF_Sorter.layout and creates the
// LFPG_SorterController. Handles input lock, cursor, and
// the per-frame Update loop (no external MissionInit hook).
//
// Open/Close via static API:
//   LFPG_SorterView.Open(json, name, d0..d5, netL, netH)
//   LFPG_SorterView.Close()
//
// Enforce Script: no ternaries, no ++/--, no foreach.
// =========================================================

class LFPG_SorterView extends ScriptView
{
    // ---- Singleton ----
    protected static ref LFPG_SorterView s_Instance;

    // ---- State ----
    protected bool m_IsOpen;
    protected bool m_FocusLocked;

    // ---- Auto-assigned widget refs (by name) ----
    ImageWidget ModalOverlay;
    ImageWidget PanelBg;
    ImageWidget AccentLine;
    ImageWidget HeaderBg;
    ImageWidget StatusDot;
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
    Widget RulesPanel;
    Widget PreviewPanel;
    Widget DestIndicator;
    ImageWidget DestIndicatorBg;
    TextWidget TagsEmpty;
    TextWidget PreviewEmpty;
    ImageWidget MatchFooterBg;

    // ---- Procedural white texture for tinting ----
    static const string PROC_WHITE = "#(argb,8,8,3)color(1,1,1,1,CO)";

    // ---- LFPG Palette (ARGB ints) ----
    static const int COL_BG_DEEP      = 0xF5060B12;
    static const int COL_BG_PANEL     = 0xF20B1120;
    static const int COL_BG_SECTION   = 0xEB101828;
    static const int COL_BG_ELEVATED  = 0xE6162030;
    static const int COL_BG_INPUT     = 0xFF1C2840;
    static const int COL_GREEN        = 0xFF34D399;
    static const int COL_GREEN_DIM    = 0x0F34D399;
    static const int COL_GREEN_BORDER = 0x2634D399;
    static const int COL_BLUE         = 0xFF60A5FA;
    static const int COL_AMBER        = 0xFFFBBF24;
    static const int COL_RED          = 0xFFF87171;
    static const int COL_BTN          = 0xE61C2840;
    static const int COL_TEXT         = 0xFFF1F5F9;
    static const int COL_TEXT_DIM     = 0xFF475569;
    static const int COL_TEXT_MID     = 0xFF94A3B8;
    static const int COL_SEPARATOR    = 0x14CBD5E1;
    static const int COL_MODAL        = 0xA6000000;
    static const int COL_HEADER       = 0xF50B1120;
    static const int COL_BLUE_BTN     = 0xFF1E3A5F;
    static const int COL_GREEN_BTN    = 0xFF065F46;
    static const int COL_RED_BTN      = 0xFF991B1B;

    // =========================================================
    // ScriptView overrides
    // =========================================================
    override string GetLayoutFile()
    {
        return "LFPowerGrid/gui/layouts/LF_Sorter.layout";
    }

    override typename GetControllerType()
    {
        return LFPG_SorterController;
    }

    // Per-frame update (built-in, no external hook needed)
    override void Update(float dt)
    {
        if (!m_IsOpen)
            return;

        // ESC to close
        if (GetGame().GetInput())
        {
            if (GetGame().GetInput().LocalPress("UAUIBack", false))
            {
                DoClose();
                return;
            }
        }
    }

    // =========================================================
    // Constructor / Setup
    // =========================================================
    void LFPG_SorterView()
    {
        m_IsOpen = false;
        m_FocusLocked = false;
    }

    override void OnWidgetScriptInit(Widget w)
    {
        super.OnWidgetScriptInit(w);
        ApplyColors();
    }

    // =========================================================
    // Color application (one-time, replaces PositionAllWidgets)
    // =========================================================
    protected void ApplyColors()
    {
        LoadAndTint(ModalOverlay, COL_MODAL);
        LoadAndTint(PanelBg, COL_BG_PANEL);
        LoadAndTint(AccentLine, COL_GREEN);
        LoadAndTint(HeaderBg, COL_HEADER);
        LoadAndTint(StatusDot, COL_GREEN);
        LoadAndTint(TabBarBg, COL_BG_DEEP);
        LoadAndTint(TabSep, COL_SEPARATOR);
        LoadAndTint(ColumnSep, COL_SEPARATOR);
        LoadAndTint(RulesPanelBg, COL_BG_DEEP);
        LoadAndTint(PreviewPanelBg, COL_BG_DEEP);
        LoadAndTint(FooterBg, COL_BG_PANEL);
        LoadAndTint(FooterSep, COL_SEPARATOR);
        LoadAndTint(SepCat, COL_SEPARATOR);
        LoadAndTint(SepSlot, COL_SEPARATOR);
        LoadAndTint(SepCatchAll, COL_SEPARATOR);
        LoadAndTint(EditPrefixBg, COL_BG_INPUT);
        LoadAndTint(EditContainsBg, COL_BG_INPUT);
        LoadAndTint(EditSlotMinBg, COL_BG_INPUT);
        LoadAndTint(EditSlotMaxBg, COL_BG_INPUT);
        LoadAndTint(DestIndicatorBg, COL_GREEN_DIM);
        LoadAndTint(MatchFooterBg, COL_SEPARATOR);

        // Labels
        SetTextColor(TagsEmpty, COL_TEXT_DIM);
        SetTextColor(PreviewEmpty, COL_TEXT_DIM);
    }

    protected void LoadAndTint(ImageWidget img, int color)
    {
        if (!img)
            return;
        img.LoadImageFile(0, PROC_WHITE);
        img.SetColor(color);
    }

    protected void SetTextColor(TextWidget txt, int color)
    {
        if (!txt)
            return;
        txt.SetColor(color);
    }

    // =========================================================
    // Modal overlay click — close on click outside panel
    // =========================================================
    override bool OnMouseButtonDown(Widget w, int x, int y, int button)
    {
        if (!m_IsOpen)
            return false;
        if (button != 0)
            return false;
        if (w == ModalOverlay)
        {
            DoClose();
            return true;
        }
        return super.OnMouseButtonDown(w, x, y, button);
    }

    // =========================================================
    // Open / Close (public static API)
    // =========================================================
    static void Open(string configJSON, string containerName, string d0, string d1, string d2, string d3, string d4, string d5, int netLow, int netHigh)
    {
        #ifndef SERVER
        if (!s_Instance)
        {
            s_Instance = new LFPG_SorterView();
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

    // =========================================================
    // Internal open/close
    // =========================================================
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
        root.Show(true);
        root.SetSort(10003);
        ShowCursor();

        // Push data to controller
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
        Widget root = GetLayoutRoot();
        if (root)
        {
            root.Show(false);
        }
        HideCursor();
        LFPG_Util.Info("[SorterView] Closed");
    }

    // =========================================================
    // Cursor / Input lock
    // =========================================================
    protected void ShowCursor()
    {
        #ifndef SERVER
        Mission mission = GetGame().GetMission();
        if (mission)
        {
            mission.PlayerControlDisable(INPUT_EXCLUDE_ALL);
        }

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

        Mission mission = GetGame().GetMission();
        if (mission)
        {
            mission.PlayerControlEnable(true);
        }
        #endif
    }

    // =========================================================
    // Helper: access typed controller
    // =========================================================
    LFPG_SorterController GetSorterController()
    {
        return LFPG_SorterController.Cast(GetController());
    }
};
