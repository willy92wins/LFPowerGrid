// =========================================================
// LF_PowerGrid — Sorter View (Dabs MVC, v2.0 FIXED)
//
// CREATION: LFPG_SorterView.Init() from MissionGameplay.OnInit
//   pre-creates the view HIDDEN (safe context). Avoids
//   RPC-context CreateWidgets corruption.
// OPEN/CLOSE: .Open() shows + pushes data. .Close() hides.
//
// Enforce Script: no ternaries, no ++/--, no foreach.
// =========================================================

class LFPG_SorterView extends ScriptView
{
    protected static ref LFPG_SorterView s_Instance;
    protected bool m_IsOpen;
    protected bool m_FocusLocked;

    // Widget refs for ApplyColors ONLY (no dupes with Controller)
    ImageWidget ModalOverlay;
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

    static const string PROC_WHITE = "#(argb,8,8,3)color(1,1,1,1,CO)";

    // LFPG Palette (ARGB)
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

    override string GetLayoutFile()
    {
        return "LFPowerGrid/gui/layouts/LF_Sorter.layout";
    }

    override typename GetControllerType()
    {
        return LFPG_SorterController;
    }

    // Bug 6 fix: forward tick to controller for save/reset timers
    override void Update(float dt)
    {
        if (!m_IsOpen)
            return;

        if (GetGame().GetInput())
        {
            if (GetGame().GetInput().LocalPress("UAUIBack", false))
            {
                DoClose();
                return;
            }
        }

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
    }

    override void OnWidgetScriptInit(Widget w)
    {
        super.OnWidgetScriptInit(w);
        ApplyColors();
    }

    protected void ApplyColors()
    {
        Tint(ModalOverlay, COL_MODAL);
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
    }

    protected void Tint(ImageWidget img, int color)
    {
        if (!img)
            return;
        img.LoadImageFile(0, PROC_WHITE);
        img.SetColor(color);
    }

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
    // Bug 1 fix: pre-create from MissionGameplay.OnInit (safe)
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
        root.Show(true);
        ShowCursor();
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

    LFPG_SorterController GetSorterController()
    {
        return LFPG_SorterController.Cast(GetController());
    }
};
