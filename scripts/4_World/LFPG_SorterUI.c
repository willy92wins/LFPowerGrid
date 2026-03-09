// =========================================================
// LF_PowerGrid - Sorter UI Panel (v1.2.0 Sprint S1)
//
// Client-side interactive panel for configuring LF_Sorter
// filter rules. ScriptedWidgetEventHandler for click/change
// events on buttons, editboxes, and tabs.
//
// Architecture:
//   - Modal overlay: blocks game input, shows cursor
//   - Left column (55%): output tabs + filter controls
//   - Right column (45%): FILTROS (active rules) / CONTENEDOR (grid)
//   - Data model: LFPG_SortConfig (6 outputs × rules)
//   - RPC integration in Sprint S4 (mock data for S1)
//
// Opening:
//   LFPG_SorterUI.Open(mockConfig)   — Sprint S1 (testing)
//   LFPG_SorterUI.Open(jsonConfig)   — Sprint S4 (real data)
//
// Enforce Script rules:
//   No foreach, no ++/--, no ternario, no +=/-=
//   Variables hoisted before conditionals
//   Incremental string concat
//   No multiline expressions in function params
//
// Layer: 4_World (needs LFPG_SorterData from 3_Game).
// =========================================================

// ---------------------------------------------------------
// Color palette (ARGB)
// ---------------------------------------------------------
static const int LFPG_SORT_COL_PANEL_BG     = 0xEB09101B;  // 235, 9,16,27
static const int LFPG_SORT_COL_HEADER_BG    = 0xF20D1321;  // 242,13,19,33
static const int LFPG_SORT_COL_ACCENT       = 0xD92E8C3B;  // 217,46,140,59
static const int LFPG_SORT_COL_GREEN        = 0xFF2E8C5B;  // 255,46,140,91
static const int LFPG_SORT_COL_GREEN_DIM    = 0x662E8C5B;  // 102,46,140,91
static const int LFPG_SORT_COL_BTN_NORMAL   = 0xFF232D3C;  // 255,35,45,60
static const int LFPG_SORT_COL_BTN_ACTIVE   = 0xFF2E8C5B;  // 255,46,140,91
static const int LFPG_SORT_COL_ORANGE       = 0xFFBF8C2E;  // 255,191,140,46
static const int LFPG_SORT_COL_BLUE         = 0xFF2E64BF;  // 255,46,100,191
static const int LFPG_SORT_COL_RED          = 0xFFBF2E2E;  // 255,191,46,46
static const int LFPG_SORT_COL_TEXT         = 0xFFC8D2C8;  // 255,200,210,200
static const int LFPG_SORT_COL_TEXT_DIM     = 0xFF50645A;  // 255,80,100,90
static const int LFPG_SORT_COL_SEPARATOR    = 0x99334059;  // 153,51,64,89
static const int LFPG_SORT_COL_MODAL        = 0x99000000;  // 153,0,0,0
static const int LFPG_SORT_COL_TAGS_BG      = 0xCC0A0F18;  // 204,10,15,24
static const int LFPG_SORT_COL_WHITE        = 0xFFF2F2F2;  // 255,242,242,242
static const int LFPG_SORT_COL_BTN_HOVER    = 0xFF2E3D50;  // 255,46,61,80
static const int LFPG_SORT_COL_EDIT_BG      = 0xFF1A2233;  // 255,26,34,51

// ---------------------------------------------------------
// Layout dimensions (pixels, based on 1080p reference)
// ---------------------------------------------------------
static const float LFPG_SORT_PANEL_W   = 680.0;
static const float LFPG_SORT_PANEL_H   = 600.0;
static const float LFPG_SORT_HEADER_H  = 48.0;
static const float LFPG_SORT_TAB_H     = 30.0;
static const float LFPG_SORT_FOOTER_H  = 44.0;
static const float LFPG_SORT_LEFT_W    = 374.0;  // 55%
static const float LFPG_SORT_RIGHT_W   = 306.0;  // 45%
static const float LFPG_SORT_PAD       = 12.0;

static const string LFPG_SORT_LAYOUT = "LFPowerGrid/gui/layouts/LF_Sorter.layout";

// Procedural 1x1 white texture (tinted via SetColor)
static const string LFPG_SORT_PROC_TEX = "#(argb,8,8,3)color(1,1,1,1,CO)";

// Category labels (8 total, indexed 0-7)
static const string LFPG_SORT_CAT_LABELS_0 = "WEAPON";
static const string LFPG_SORT_CAT_LABELS_1 = "ATTACH";
static const string LFPG_SORT_CAT_LABELS_2 = "AMMO";
static const string LFPG_SORT_CAT_LABELS_3 = "CLOTHING";
static const string LFPG_SORT_CAT_LABELS_4 = "FOOD";
static const string LFPG_SORT_CAT_LABELS_5 = "MEDICAL";
static const string LFPG_SORT_CAT_LABELS_6 = "TOOL";
static const string LFPG_SORT_CAT_LABELS_7 = "MISC";

// Category values for filter rules (match ResolveCategory)
static const string LFPG_SORT_CAT_VALUES_0 = "WEAPON";
static const string LFPG_SORT_CAT_VALUES_1 = "ATTACHMENT";
static const string LFPG_SORT_CAT_VALUES_2 = "AMMO";
static const string LFPG_SORT_CAT_VALUES_3 = "CLOTHING";
static const string LFPG_SORT_CAT_VALUES_4 = "FOOD";
static const string LFPG_SORT_CAT_VALUES_5 = "MEDICAL";
static const string LFPG_SORT_CAT_VALUES_6 = "TOOL";
static const string LFPG_SORT_CAT_VALUES_7 = "MISC";

// Slot preset labels and values
static const string LFPG_SORT_SLOT_LABELS_0 = "TINY";
static const string LFPG_SORT_SLOT_LABELS_1 = "SMALL";
static const string LFPG_SORT_SLOT_LABELS_2 = "MED";
static const string LFPG_SORT_SLOT_LABELS_3 = "LARGE";

// =========================================================
// Main UI handler class
// =========================================================
class LFPG_SorterUI : ScriptedWidgetEventHandler
{
    // ---- Singleton ----
    protected static ref LFPG_SorterUI s_Instance;

    // ---- Root widgets ----
    protected Widget m_Root;
    protected Widget m_Panel;
    protected ImageWidget m_wModalOverlay;
    protected ImageWidget m_wPanelBg;
    protected ImageWidget m_wPanelGlow;
    protected ImageWidget m_wHeaderBg;
    protected ImageWidget m_wScanlines;

    // ---- Header ----
    protected TextWidget m_wTitle;
    protected TextWidget m_wStatus;
    protected ButtonWidget m_wBtnSort;
    protected TextWidget m_wBtnSortText;

    // ---- Output tabs (6) ----
    protected ref array<ButtonWidget> m_wTabOut;
    protected ref array<TextWidget> m_wTabOutText;

    // ---- View tabs ----
    protected ButtonWidget m_wTabFilters;
    protected TextWidget m_wTabFiltersText;
    protected ButtonWidget m_wTabContView;
    protected TextWidget m_wTabContViewText;

    // ---- Views ----
    protected Widget m_wFiltersView;
    protected Widget m_wContainerView;

    // ---- Categories (8 buttons) ----
    protected TextWidget m_wLblCategories;
    protected ref array<ButtonWidget> m_wCatBtn;
    protected ref array<TextWidget> m_wCatBtnText;

    // ---- Separators ----
    protected ImageWidget m_wSepCat;
    protected ImageWidget m_wSepSlot;
    protected ImageWidget m_wSepCatchAll;

    // ---- Prefix ----
    protected TextWidget m_wLblPrefix;
    protected EditBoxWidget m_wEditPrefix;
    protected TextWidget m_wPlhPrefix;
    protected ButtonWidget m_wBtnPrefixAdd;
    protected TextWidget m_wBtnPrefixAddText;

    // ---- Contains ----
    protected TextWidget m_wLblContains;
    protected EditBoxWidget m_wEditContains;
    protected TextWidget m_wPlhContains;
    protected ButtonWidget m_wBtnContainsAdd;
    protected TextWidget m_wBtnContainsAddText;

    // ---- Slot ----
    protected TextWidget m_wLblSlot;
    protected ref array<ButtonWidget> m_wSlotPre;
    protected ref array<TextWidget> m_wSlotPreText;
    protected EditBoxWidget m_wEditSlotMin;
    protected TextWidget m_wLblSlotDash;
    protected EditBoxWidget m_wEditSlotMax;
    protected ButtonWidget m_wBtnSlotAdd;
    protected TextWidget m_wBtnSlotAddText;

    // ---- Catch-All ----
    protected ButtonWidget m_wBtnCatchAll;
    protected TextWidget m_wBtnCatchAllText;

    // ---- Tags panel (right column, FILTROS view) ----
    protected Widget m_wTagsPanel;
    protected ImageWidget m_wTagsBg;
    protected TextWidget m_wLblTags;
    protected TextWidget m_wTagsEmpty;
    protected WrapSpacerWidget m_wTagsWrap;
    protected ref array<Widget> m_TagWidgets;  // dynamic tag children

    // ---- Container view (right column) ----
    protected ImageWidget m_wGridBg;
    protected TextWidget m_wGridTitle;
    protected TextWidget m_wGridInfo;
    protected Widget m_wGridArea;

    // ---- Footer ----
    protected ImageWidget m_wFooterSep;
    protected ButtonWidget m_wBtnClearOut;
    protected TextWidget m_wBtnClearOutText;
    protected ButtonWidget m_wBtnResetAll;
    protected TextWidget m_wBtnResetAllText;
    protected ButtonWidget m_wBtnSave;
    protected TextWidget m_wBtnSaveText;
    protected ButtonWidget m_wBtnClose;
    protected TextWidget m_wBtnCloseText;

    // ---- Accent bar (M1) ----
    protected ImageWidget m_wAccentBar;

    // ---- EditBox backgrounds (E1) ----
    protected ImageWidget m_wEditPrefixBg;
    protected ImageWidget m_wEditContainsBg;
    protected ImageWidget m_wEditSlotMinBg;
    protected ImageWidget m_wEditSlotMaxBg;

    // ---- State ----
    protected bool m_IsOpen;
    protected int m_SelectedOutput;     // 0-5
    protected bool m_ShowFilters;       // true=FILTROS, false=CONTENEDOR
    protected bool m_FocusLocked;
    protected float m_GlowPhase;        // glow pulse animation
    protected float m_SaveFeedbackTimer; // H4: save feedback countdown (seconds)

    // ---- Reset confirmation (M5) ----
    protected bool m_ResetConfirmActive;
    protected float m_ResetConfirmTimer;

    // ---- Tags flash (U1) ----
    protected float m_TagsFlashTimer;

    // ---- Hover tracking (E3: cursor polling) ----
    protected ButtonWidget m_HoveredBtn;
    protected float m_PanelScreenX;
    protected float m_PanelScreenY;

    // ---- Deferred creation (avoid CreateWidgets from RPC context) ----
    protected bool m_PendingOpen;

    // ---- Data model ----
    protected ref LFPG_SortConfig m_Config;

    // ---- Linked entity (set when opened via RPC, null for mock) ----
    protected string m_ContainerName;

    // ---- S4: Sorter identity for RPCs ----
    protected int m_SorterNetLow;
    protected int m_SorterNetHigh;

    // ---- S4: Dest container names per output (6 slots) ----
    protected string m_DestName0;
    protected string m_DestName1;
    protected string m_DestName2;
    protected string m_DestName3;
    protected string m_DestName4;
    protected string m_DestName5;

    // =========================================================
    // Constructor
    // =========================================================
    void LFPG_SorterUI()
    {
        m_wTabOut = new array<ButtonWidget>;
        m_wTabOutText = new array<TextWidget>;
        m_wCatBtn = new array<ButtonWidget>;
        m_wCatBtnText = new array<TextWidget>;
        m_wSlotPre = new array<ButtonWidget>;
        m_wSlotPreText = new array<TextWidget>;
        m_TagWidgets = new array<Widget>;
        m_Config = new LFPG_SortConfig();
        m_IsOpen = false;
        m_SelectedOutput = 0;
        m_ShowFilters = true;
        m_FocusLocked = false;
        m_GlowPhase = 0.0;
        m_SaveFeedbackTimer = 0.0;
        m_PendingOpen = false;
        m_ResetConfirmActive = false;
        m_ResetConfirmTimer = 0.0;
        m_TagsFlashTimer = 0.0;
        m_HoveredBtn = null;
        m_PanelScreenX = 0.0;
        m_PanelScreenY = 0.0;
        m_ContainerName = "Container";
        m_SorterNetLow = 0;
        m_SorterNetHigh = 0;
        m_DestName0 = "";
        m_DestName1 = "";
        m_DestName2 = "";
        m_DestName3 = "";
        m_DestName4 = "";
        m_DestName5 = "";
    }

    // =========================================================
    // Singleton
    // =========================================================
    static LFPG_SorterUI Get()
    {
        if (!s_Instance)
        {
            s_Instance = new LFPG_SorterUI();
        }
        return s_Instance;
    }

    static void Cleanup()
    {
        if (s_Instance)
        {
            // Lightweight reset: do NOT call DoClose/HideCursor.
            // OnInit: input system is fresh (no prior Disable/Focus to undo).
            // OnMissionFinish: input system is about to be destroyed.
            // Normal close (ESC) uses DoClose which handles input properly.
            s_Instance.m_IsOpen = false;
            s_Instance.m_FocusLocked = false;
            s_Instance.m_PendingOpen = false;
            s_Instance.DestroyWidgets();
        }
        s_Instance = null;
    }

    static bool IsOpen()
    {
        if (!s_Instance)
            return false;
        return s_Instance.m_IsOpen;
    }

    // =========================================================
    // Deferred widget creation (called from MissionGameplay.OnUpdate)
    //
    // CreateWidgets MUST run in OnUpdate context, not RPC context.
    // Same pattern as LFPG_CameraViewport.InitWidgets.
    // =========================================================
    static bool NeedsDeferredCreate()
    {
        if (!s_Instance)
            return false;
        return s_Instance.m_PendingOpen;
    }

    static void FinishDeferredCreate()
    {
        if (!s_Instance)
            return;
        if (!s_Instance.m_PendingOpen)
            return;

        s_Instance.m_PendingOpen = false;

        // Create widgets in safe OnUpdate context
        s_Instance.CreateWidgets();

        if (!s_Instance.m_Root)
        {
            LFPG_Util.Error("[SorterUI] Deferred CreateWidgets failed");
            return;
        }

        // Complete the open sequence that was deferred
        s_Instance.CompleteOpen();
    }

    // =========================================================
    // Open / Close (public API)
    // =========================================================

    // Sprint S1: Open with mock data for testing.
    // Sprint S4: Open with real config JSON from RPC response.
    static void Open(string configJSON, string containerName, string d0, string d1, string d2, string d3, string d4, string d5, int netLow, int netHigh)
    {
        LFPG_SorterUI ui = Get();
        ui.DoOpen(configJSON, containerName, d0, d1, d2, d3, d4, d5, netLow, netHigh);
    }

    // Sprint S1: Open with mock data (no params).
    static void OpenMock()
    {
        LFPG_SorterUI ui = Get();
        // Hardcoded mock JSON: output 0 = WEAPON+AMMO, output 1 = prefix M4, output 5 = catch-all
        string mockJSON = "{\"o\":[";
        mockJSON = mockJSON + "{\"r\":[{\"t\":0,\"v\":\"WEAPON\"},{\"t\":0,\"v\":\"AMMO\"}],\"ca\":false},";
        mockJSON = mockJSON + "{\"r\":[{\"t\":1,\"v\":\"M4\"}],\"ca\":false},";
        mockJSON = mockJSON + "{\"r\":[],\"ca\":false},";
        mockJSON = mockJSON + "{\"r\":[],\"ca\":false},";
        mockJSON = mockJSON + "{\"r\":[],\"ca\":false},";
        mockJSON = mockJSON + "{\"r\":[],\"ca\":true}";
        mockJSON = mockJSON + "]}";
        ui.DoOpen(mockJSON, "Wooden Crate", "", "", "", "", "", "", 0, 0);
    }

    static void Close()
    {
        if (s_Instance)
        {
            s_Instance.DoClose();
        }
    }

    // =========================================================
    // Internal open/close
    // =========================================================
    protected void DoOpen(string configJSON, string containerName, string d0, string d1, string d2, string d3, string d4, string d5, int netLow, int netHigh)
    {
        if (m_IsOpen)
            return;

        m_ContainerName = containerName;
        m_SorterNetLow = netLow;
        m_SorterNetHigh = netHigh;
        m_SelectedOutput = 0;
        m_ShowFilters = true;

        // Store dest names
        m_DestName0 = d0;
        m_DestName1 = d1;
        m_DestName2 = d2;
        m_DestName3 = d3;
        m_DestName4 = d4;
        m_DestName5 = d5;

        // Parse config
        if (configJSON != "")
        {
            m_Config.FromJSON(configJSON);
        }
        else
        {
            m_Config.ResetAll();
        }

        // Widgets already created (second+ open): complete immediately
        if (m_Root)
        {
            CompleteOpen();
            return;
        }

        // First open: defer CreateWidgets to MissionGameplay.OnUpdate
        // context. CreateWidgets from RPC context produces corrupt
        // widget trees (partial render: black bg + green square).
        m_PendingOpen = true;
        LFPG_Util.Info("[SorterUI] Deferred open — waiting for OnUpdate context");
    }

    // Called from FinishDeferredCreate (OnUpdate context) or
    // directly from DoOpen when widgets already exist.
    protected void CompleteOpen()
    {
        if (!m_Root)
        {
            LFPG_Util.Error("[SorterUI] CompleteOpen called without m_Root");
            return;
        }

        m_IsOpen = true;
        m_Root.Show(true);

        // Reset transient state
        m_ResetConfirmActive = false;
        m_ResetConfirmTimer = 0.0;
        m_TagsFlashTimer = 0.0;

        // M5: Ensure RESET ALL button shows correct text on reopen
        if (m_wBtnResetAllText)
        {
            m_wBtnResetAllText.SetText("RESET ALL");
        }
        SetBtnBgColor(m_wBtnResetAll, LFPG_SORT_COL_RED);
        m_HoveredBtn = null;

        // Show cursor + lock game focus
        ShowCursor();

        // Refresh UI
        RefreshOutputTabs();
        RefreshViewTabs();
        RefreshFiltersView();
        RefreshTagsPanel();
        RefreshStatusText(true);  // server validated powered before sending response

        // S4: Update title with linked container name
        if (m_wTitle)
        {
            string titleText = "SORTER";
            if (m_ContainerName != "")
            {
                titleText = titleText + " — " + m_ContainerName;
            }
            m_wTitle.SetText(titleText);
        }

        LFPG_Util.Info("[SorterUI] Opened for container: " + m_ContainerName);
    }

    protected void DoClose()
    {
        if (!m_IsOpen)
            return;

        m_IsOpen = false;

        if (m_Root)
        {
            m_Root.Show(false);
        }

        HideCursor();
        LFPG_Util.Info("[SorterUI] Closed");
    }

    // =========================================================
    // Cursor / Input management
    // =========================================================
    protected void ShowCursor()
    {
        #ifndef SERVER
        // Block all player actions while panel is open
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

        // Re-enable player actions
        Mission mission = GetGame().GetMission();
        if (mission)
        {
            mission.PlayerControlEnable(true);
        }
        #endif
    }

    // =========================================================
    // Widget creation
    // =========================================================
    protected void CreateWidgets()
    {
        #ifndef SERVER
        if (m_Root)
            return;

        m_Root = GetGame().GetWorkspace().CreateWidgets(LFPG_SORT_LAYOUT);
        if (!m_Root)
        {
            LFPG_Util.Error("[SorterUI] Failed to load layout: " + LFPG_SORT_LAYOUT);
            return;
        }

        m_Root.SetSort(10003);
        m_Root.SetHandler(this);

        // Find all widgets by name
        FindWidgets();

        // Position + color everything from code
        PositionAllWidgets();

        m_Root.Show(false);
        LFPG_Util.Info("[SorterUI] Widgets created");
        #endif
    }

    protected void FindWidgets()
    {
        m_wModalOverlay = ImageWidget.Cast(m_Root.FindAnyWidget("ModalOverlay"));
        m_Panel = m_Root.FindAnyWidget("SorterPanel");
        m_wPanelBg = ImageWidget.Cast(m_Root.FindAnyWidget("PanelBg"));
        m_wPanelGlow = ImageWidget.Cast(m_Root.FindAnyWidget("PanelGlow"));
        m_wHeaderBg = ImageWidget.Cast(m_Root.FindAnyWidget("HeaderBg"));
        m_wScanlines = ImageWidget.Cast(m_Root.FindAnyWidget("ScanlineOverlay"));

        m_wTitle = TextWidget.Cast(m_Root.FindAnyWidget("HeaderTitle"));
        m_wStatus = TextWidget.Cast(m_Root.FindAnyWidget("StatusText"));
        m_wBtnSort = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnSort"));
        m_wBtnSortText = TextWidget.Cast(m_Root.FindAnyWidget("BtnSortText"));

        // Output tabs
        int ti;
        for (ti = 0; ti < 6; ti = ti + 1)
        {
            string tabName = "TabOut" + ti.ToString();
            string tabTextName = tabName + "Text";
            ButtonWidget tabBtn = ButtonWidget.Cast(m_Root.FindAnyWidget(tabName));
            TextWidget tabTxt = TextWidget.Cast(m_Root.FindAnyWidget(tabTextName));
            m_wTabOut.Insert(tabBtn);
            m_wTabOutText.Insert(tabTxt);
        }

        // View tabs
        m_wTabFilters = ButtonWidget.Cast(m_Root.FindAnyWidget("TabFilters"));
        m_wTabFiltersText = TextWidget.Cast(m_Root.FindAnyWidget("TabFiltersText"));
        m_wTabContView = ButtonWidget.Cast(m_Root.FindAnyWidget("TabContView"));
        m_wTabContViewText = TextWidget.Cast(m_Root.FindAnyWidget("TabContViewText"));

        // Views
        m_wFiltersView = m_Root.FindAnyWidget("FiltersView");
        m_wContainerView = m_Root.FindAnyWidget("ContainerView");

        // Categories
        m_wLblCategories = TextWidget.Cast(m_Root.FindAnyWidget("LblCategories"));
        int ci;
        for (ci = 0; ci < 8; ci = ci + 1)
        {
            string catName = "CatBtn" + ci.ToString();
            string catTextName = catName + "Text";
            ButtonWidget catBtn = ButtonWidget.Cast(m_Root.FindAnyWidget(catName));
            TextWidget catTxt = TextWidget.Cast(m_Root.FindAnyWidget(catTextName));
            m_wCatBtn.Insert(catBtn);
            m_wCatBtnText.Insert(catTxt);
        }

        // Separators
        m_wSepCat = ImageWidget.Cast(m_Root.FindAnyWidget("SepCat"));
        m_wSepSlot = ImageWidget.Cast(m_Root.FindAnyWidget("SepSlot"));
        m_wSepCatchAll = ImageWidget.Cast(m_Root.FindAnyWidget("SepCatchAll"));

        // Prefix
        m_wLblPrefix = TextWidget.Cast(m_Root.FindAnyWidget("LblPrefix"));
        m_wEditPrefix = EditBoxWidget.Cast(m_Root.FindAnyWidget("EditPrefix"));
        m_wPlhPrefix = TextWidget.Cast(m_Root.FindAnyWidget("PlhPrefix"));
        m_wBtnPrefixAdd = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnPrefixAdd"));
        m_wBtnPrefixAddText = TextWidget.Cast(m_Root.FindAnyWidget("BtnPrefixAddText"));

        // Contains
        m_wLblContains = TextWidget.Cast(m_Root.FindAnyWidget("LblContains"));
        m_wEditContains = EditBoxWidget.Cast(m_Root.FindAnyWidget("EditContains"));
        m_wPlhContains = TextWidget.Cast(m_Root.FindAnyWidget("PlhContains"));
        m_wBtnContainsAdd = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnContainsAdd"));
        m_wBtnContainsAddText = TextWidget.Cast(m_Root.FindAnyWidget("BtnContainsAddText"));

        // Slot
        m_wLblSlot = TextWidget.Cast(m_Root.FindAnyWidget("LblSlot"));
        int si;
        for (si = 0; si < 4; si = si + 1)
        {
            string slotName = "SlotPre" + si.ToString();
            string slotTextName = slotName + "Text";
            ButtonWidget slotBtn = ButtonWidget.Cast(m_Root.FindAnyWidget(slotName));
            TextWidget slotTxt = TextWidget.Cast(m_Root.FindAnyWidget(slotTextName));
            m_wSlotPre.Insert(slotBtn);
            m_wSlotPreText.Insert(slotTxt);
        }
        m_wEditSlotMin = EditBoxWidget.Cast(m_Root.FindAnyWidget("EditSlotMin"));
        m_wLblSlotDash = TextWidget.Cast(m_Root.FindAnyWidget("LblSlotDash"));
        m_wEditSlotMax = EditBoxWidget.Cast(m_Root.FindAnyWidget("EditSlotMax"));
        m_wBtnSlotAdd = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnSlotAdd"));
        m_wBtnSlotAddText = TextWidget.Cast(m_Root.FindAnyWidget("BtnSlotAddText"));

        // Catch-All
        m_wBtnCatchAll = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnCatchAll"));
        m_wBtnCatchAllText = TextWidget.Cast(m_Root.FindAnyWidget("BtnCatchAllText"));

        // Tags panel
        m_wTagsPanel = m_Root.FindAnyWidget("TagsPanel");
        m_wTagsBg = ImageWidget.Cast(m_Root.FindAnyWidget("TagsBg"));
        m_wLblTags = TextWidget.Cast(m_Root.FindAnyWidget("LblTags"));
        m_wTagsEmpty = TextWidget.Cast(m_Root.FindAnyWidget("TagsEmpty"));
        m_wTagsWrap = WrapSpacerWidget.Cast(m_Root.FindAnyWidget("TagsWrap"));

        // Container view
        m_wGridBg = ImageWidget.Cast(m_Root.FindAnyWidget("GridBg"));
        m_wGridTitle = TextWidget.Cast(m_Root.FindAnyWidget("GridTitle"));
        m_wGridInfo = TextWidget.Cast(m_Root.FindAnyWidget("GridInfo"));
        m_wGridArea = m_Root.FindAnyWidget("GridArea");

        // Footer
        m_wFooterSep = ImageWidget.Cast(m_Root.FindAnyWidget("FooterSep"));
        m_wBtnClearOut = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnClearOut"));
        m_wBtnClearOutText = TextWidget.Cast(m_Root.FindAnyWidget("BtnClearOutText"));
        m_wBtnResetAll = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnResetAll"));
        m_wBtnResetAllText = TextWidget.Cast(m_Root.FindAnyWidget("BtnResetAllText"));
        m_wBtnSave = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnSave"));
        m_wBtnSaveText = TextWidget.Cast(m_Root.FindAnyWidget("BtnSaveText"));
        m_wBtnClose = ButtonWidget.Cast(m_Root.FindAnyWidget("BtnClose"));
        m_wBtnCloseText = TextWidget.Cast(m_Root.FindAnyWidget("BtnCloseText"));

        // Accent bar (M1)
        m_wAccentBar = ImageWidget.Cast(m_Root.FindAnyWidget("AccentBar"));

        // EditBox backgrounds (E1)
        m_wEditPrefixBg = ImageWidget.Cast(m_Root.FindAnyWidget("EditPrefixBg"));
        m_wEditContainsBg = ImageWidget.Cast(m_Root.FindAnyWidget("EditContainsBg"));
        m_wEditSlotMinBg = ImageWidget.Cast(m_Root.FindAnyWidget("EditSlotMinBg"));
        m_wEditSlotMaxBg = ImageWidget.Cast(m_Root.FindAnyWidget("EditSlotMaxBg"));
    }

    // =========================================================
    // Position + style all widgets from code
    // =========================================================
    protected void PositionAllWidgets()
    {
        // Screen dimensions for centering
        int scrW = 0;
        int scrH = 0;
        GetScreenSize(scrW, scrH);
        float px = (scrW - LFPG_SORT_PANEL_W) * 0.5;
        float py = (scrH - LFPG_SORT_PANEL_H) * 0.5;
        m_PanelScreenX = px;
        m_PanelScreenY = py;

        // Modal overlay: fullscreen dark
        // Layout defines size 1 1 (proportional, 100% of parent root).
        // Do NOT call SetPos/SetSize — pixel values in proportional mode
        // create an oversized widget. Only load texture + tint.
        if (m_wModalOverlay)
        {
            m_wModalOverlay.LoadImageFile(0, LFPG_SORT_PROC_TEX);
            m_wModalOverlay.SetColor(LFPG_SORT_COL_MODAL);
        }

        // Panel container
        if (m_Panel)
        {
            m_Panel.SetPos(px, py);
            m_Panel.SetSize(LFPG_SORT_PANEL_W, LFPG_SORT_PANEL_H);
        }

        // Panel glow (slightly larger, behind)
        SetupImage(m_wPanelGlow, -6, -6, LFPG_SORT_PANEL_W + 12, LFPG_SORT_PANEL_H + 12, LFPG_SORT_COL_GREEN_DIM);

        // Panel background
        SetupImage(m_wPanelBg, 0, 0, LFPG_SORT_PANEL_W, LFPG_SORT_PANEL_H, LFPG_SORT_COL_PANEL_BG);

        // Accent bar (M1): green left edge
        SetupImage(m_wAccentBar, 0, 0, 3, LFPG_SORT_PANEL_H, LFPG_SORT_COL_GREEN);

        // Header background
        SetupImage(m_wHeaderBg, 0, 0, LFPG_SORT_PANEL_W, LFPG_SORT_HEADER_H, LFPG_SORT_COL_HEADER_BG);

        // Header title
        SetupText(m_wTitle, 14, 12, 200, 24, LFPG_SORT_COL_GREEN, "SORTER");

        // Status text
        SetupText(m_wStatus, 220, 16, 120, 20, LFPG_SORT_COL_GREEN, "ONLINE");

        // ORDENAR button
        SetupButton(m_wBtnSort, m_wBtnSortText, 568, 8, 100, 32, LFPG_SORT_COL_BLUE, LFPG_SORT_COL_WHITE, "ORDENAR");

        // ---- Output tabs (6 across left column) ----
        float tabY = LFPG_SORT_HEADER_H;
        float tabW = LFPG_SORT_LEFT_W / 6.0;
        int oti;
        for (oti = 0; oti < 6; oti = oti + 1)
        {
            float tabX = tabW * oti;
            int outNum = oti + 1;
            string outLabel = "OUT " + outNum.ToString();
            SetupButton(m_wTabOut[oti], m_wTabOutText[oti], tabX, tabY, tabW, LFPG_SORT_TAB_H, LFPG_SORT_COL_BTN_NORMAL, LFPG_SORT_COL_TEXT, outLabel);
        }

        // ---- View tabs (right column header) ----
        float viewTabY = LFPG_SORT_HEADER_H;
        float viewTabW = LFPG_SORT_RIGHT_W * 0.5;
        SetupButton(m_wTabFilters, m_wTabFiltersText, LFPG_SORT_LEFT_W, viewTabY, viewTabW, LFPG_SORT_TAB_H, LFPG_SORT_COL_BTN_NORMAL, LFPG_SORT_COL_TEXT, "FILTROS");
        float contTabX = LFPG_SORT_LEFT_W + viewTabW;
        SetupButton(m_wTabContView, m_wTabContViewText, contTabX, viewTabY, viewTabW, LFPG_SORT_TAB_H, LFPG_SORT_COL_BTN_NORMAL, LFPG_SORT_COL_TEXT, "CONTENEDOR");

        // ---- Content area starts ----
        float contentY = LFPG_SORT_HEADER_H + LFPG_SORT_TAB_H;
        float contentH = LFPG_SORT_PANEL_H - contentY - LFPG_SORT_FOOTER_H;

        // Filters view frame (left column content area)
        if (m_wFiltersView)
        {
            m_wFiltersView.SetPos(0, contentY);
            m_wFiltersView.SetSize(LFPG_SORT_LEFT_W, contentH);
        }

        // ---- CATEGORIES section ----
        float secY = 4.0;  // relative to FiltersView
        SetupText(m_wLblCategories, LFPG_SORT_PAD, secY, 150, 18, LFPG_SORT_COL_TEXT_DIM, "CATEGORIES");
        secY = secY + 22.0;

        // 8 buttons in 4x2 grid
        float catBtnW = 84.0;
        float catBtnH = 26.0;
        float catGap = 4.0;
        int catIdx;
        for (catIdx = 0; catIdx < 8; catIdx = catIdx + 1)
        {
            int catRow = 0;
            int catCol = catIdx;
            if (catIdx >= 4)
            {
                catRow = 1;
                catCol = catIdx - 4;
            }
            float catX = LFPG_SORT_PAD + (catBtnW + catGap) * catCol;
            float catY = secY + (catBtnH + catGap) * catRow;
            string catLabel = GetCategoryLabel(catIdx);
            SetupButton(m_wCatBtn[catIdx], m_wCatBtnText[catIdx], catX, catY, catBtnW, catBtnH, LFPG_SORT_COL_BTN_NORMAL, LFPG_SORT_COL_TEXT, catLabel);
        }

        secY = secY + (catBtnH + catGap) * 2 + 6.0;

        // Separator
        SetupSeparator(m_wSepCat, LFPG_SORT_PAD, secY, LFPG_SORT_LEFT_W - LFPG_SORT_PAD * 2);
        secY = secY + 8.0;

        // ---- PREFIX section ----
        SetupText(m_wLblPrefix, LFPG_SORT_PAD, secY, 80, 18, LFPG_SORT_COL_TEXT_DIM, "PREFIX");
        secY = secY + 20.0;

        float editW = 240.0;
        float editH = 26.0;
        float addBtnW = 36.0;
        SetupImage(m_wEditPrefixBg, LFPG_SORT_PAD, secY, editW, editH, LFPG_SORT_COL_EDIT_BG);
        SetupEditBox(m_wEditPrefix, LFPG_SORT_PAD, secY, editW, editH);
        SetupText(m_wPlhPrefix, LFPG_SORT_PAD + 6, secY + 3, editW - 12, 20, LFPG_SORT_COL_TEXT_DIM, "Type prefix...");
        float addX = LFPG_SORT_PAD + editW + 4;
        SetupButton(m_wBtnPrefixAdd, m_wBtnPrefixAddText, addX, secY, addBtnW, editH, LFPG_SORT_COL_GREEN, LFPG_SORT_COL_WHITE, "+");
        secY = secY + editH + 8.0;

        // ---- CONTAINS section ----
        SetupText(m_wLblContains, LFPG_SORT_PAD, secY, 80, 18, LFPG_SORT_COL_TEXT_DIM, "CONTAINS");
        secY = secY + 20.0;

        SetupImage(m_wEditContainsBg, LFPG_SORT_PAD, secY, editW, editH, LFPG_SORT_COL_EDIT_BG);
        SetupEditBox(m_wEditContains, LFPG_SORT_PAD, secY, editW, editH);
        SetupText(m_wPlhContains, LFPG_SORT_PAD + 6, secY + 3, editW - 12, 20, LFPG_SORT_COL_TEXT_DIM, "Type substring...");
        SetupButton(m_wBtnContainsAdd, m_wBtnContainsAddText, addX, secY, addBtnW, editH, LFPG_SORT_COL_GREEN, LFPG_SORT_COL_WHITE, "+");
        secY = secY + editH + 8.0;

        // Separator
        SetupSeparator(m_wSepSlot, LFPG_SORT_PAD, secY, LFPG_SORT_LEFT_W - LFPG_SORT_PAD * 2);
        secY = secY + 8.0;

        // ---- SLOT SIZE section ----
        SetupText(m_wLblSlot, LFPG_SORT_PAD, secY, 100, 18, LFPG_SORT_COL_TEXT_DIM, "SLOT SIZE");
        secY = secY + 22.0;

        // 4 preset buttons
        float slotBtnW = 64.0;
        float slotBtnH = 26.0;
        float slotGap = 4.0;
        int sli;
        for (sli = 0; sli < 4; sli = sli + 1)
        {
            float slotX = LFPG_SORT_PAD + (slotBtnW + slotGap) * sli;
            string slotLabel = GetSlotPresetLabel(sli);
            SetupButton(m_wSlotPre[sli], m_wSlotPreText[sli], slotX, secY, slotBtnW, slotBtnH, LFPG_SORT_COL_BTN_NORMAL, LFPG_SORT_COL_TEXT, slotLabel);
        }
        secY = secY + slotBtnH + 6.0;

        // Custom min-max
        float slotEditW = 50.0;
        SetupImage(m_wEditSlotMinBg, LFPG_SORT_PAD, secY, slotEditW, editH, LFPG_SORT_COL_EDIT_BG);
        SetupEditBox(m_wEditSlotMin, LFPG_SORT_PAD, secY, slotEditW, editH);
        float dashX = LFPG_SORT_PAD + slotEditW + 4;
        SetupText(m_wLblSlotDash, dashX, secY + 3, 14, 20, LFPG_SORT_COL_TEXT, "-");
        float maxEditX = dashX + 16;
        SetupImage(m_wEditSlotMaxBg, maxEditX, secY, slotEditW, editH, LFPG_SORT_COL_EDIT_BG);
        SetupEditBox(m_wEditSlotMax, maxEditX, secY, slotEditW, editH);
        float slotAddX = maxEditX + slotEditW + 4;
        SetupButton(m_wBtnSlotAdd, m_wBtnSlotAddText, slotAddX, secY, addBtnW, editH, LFPG_SORT_COL_GREEN, LFPG_SORT_COL_WHITE, "+");
        secY = secY + editH + 8.0;

        // Separator
        SetupSeparator(m_wSepCatchAll, LFPG_SORT_PAD, secY, LFPG_SORT_LEFT_W - LFPG_SORT_PAD * 2);
        secY = secY + 8.0;

        // ---- CATCH-ALL ----
        SetupButton(m_wBtnCatchAll, m_wBtnCatchAllText, LFPG_SORT_PAD, secY, 140, 28, LFPG_SORT_COL_BTN_NORMAL, LFPG_SORT_COL_TEXT, "CATCH-ALL");

        // ---- Tags panel (right column) ----
        float rightX = LFPG_SORT_LEFT_W;
        if (m_wTagsPanel)
        {
            m_wTagsPanel.SetPos(rightX, contentY);
            m_wTagsPanel.SetSize(LFPG_SORT_RIGHT_W, contentH);
        }
        SetupImage(m_wTagsBg, 0, 0, LFPG_SORT_RIGHT_W, contentH, LFPG_SORT_COL_TAGS_BG);
        SetupText(m_wLblTags, 10, 4, 200, 18, LFPG_SORT_COL_TEXT_DIM, "ACTIVE RULES");
        SetupText(m_wTagsEmpty, 10, 30, 280, 40, LFPG_SORT_COL_TEXT_DIM, "No rules configured for this output");

        if (m_wTagsWrap)
        {
            m_wTagsWrap.SetPos(8, 26);
            m_wTagsWrap.SetSize(LFPG_SORT_RIGHT_W - 16, contentH - 34);
        }

        // ---- Container view (right column, initially hidden) ----
        if (m_wContainerView)
        {
            m_wContainerView.SetPos(rightX, contentY);
            m_wContainerView.SetSize(LFPG_SORT_RIGHT_W, contentH);
        }
        SetupImage(m_wGridBg, 0, 0, LFPG_SORT_RIGHT_W, contentH, LFPG_SORT_COL_TAGS_BG);
        SetupText(m_wGridTitle, 10, 4, 200, 18, LFPG_SORT_COL_TEXT_DIM, "CONTAINER");
        SetupText(m_wGridInfo, 10, 30, 280, 60, LFPG_SORT_COL_TEXT_DIM, "Container grid visualization\n(Sprint S4)");
        if (m_wGridArea)
        {
            m_wGridArea.SetPos(8, 52);
            m_wGridArea.SetSize(LFPG_SORT_RIGHT_W - 16, contentH - 60);
        }

        // ---- Scanlines overlay (full panel, on top, no pointer) ----
        SetupImage(m_wScanlines, 0, 0, LFPG_SORT_PANEL_W, LFPG_SORT_PANEL_H, LFPG_SORT_COL_GREEN_DIM);
        if (m_wScanlines)
        {
            m_wScanlines.SetAlpha(0.06);
        }

        // ---- Footer ----
        float footY = LFPG_SORT_PANEL_H - LFPG_SORT_FOOTER_H;
        SetupSeparator(m_wFooterSep, LFPG_SORT_PAD, footY, LFPG_SORT_PANEL_W - LFPG_SORT_PAD * 2);
        footY = footY + 6.0;

        float footBtnW = 130.0;
        float footBtnH = 30.0;
        float footGap = 8.0;
        float footBtnX = LFPG_SORT_PAD;

        SetupButton(m_wBtnClearOut, m_wBtnClearOutText, footBtnX, footY, footBtnW, footBtnH, LFPG_SORT_COL_BTN_NORMAL, LFPG_SORT_COL_TEXT, "CLEAR OUTPUT");
        footBtnX = footBtnX + footBtnW + footGap;
        SetupButton(m_wBtnResetAll, m_wBtnResetAllText, footBtnX, footY, footBtnW, footBtnH, LFPG_SORT_COL_RED, LFPG_SORT_COL_WHITE, "RESET ALL");
        footBtnX = footBtnX + footBtnW + footGap;

        // Save + Close on the right side
        float closeX = LFPG_SORT_PANEL_W - LFPG_SORT_PAD - 80;
        float saveX = closeX - 100 - footGap;
        SetupButton(m_wBtnSave, m_wBtnSaveText, saveX, footY, 100, footBtnH, LFPG_SORT_COL_GREEN, LFPG_SORT_COL_WHITE, "SAVE");
        SetupButton(m_wBtnClose, m_wBtnCloseText, closeX, footY, 80, footBtnH, LFPG_SORT_COL_BTN_NORMAL, LFPG_SORT_COL_TEXT, "CLOSE");
    }

    // =========================================================
    // Widget setup helpers
    // =========================================================
    protected void SetupImage(ImageWidget img, float x, float y, float w, float h, int color)
    {
        if (!img)
            return;
        img.SetPos(x, y);
        img.SetSize(w, h);
        img.LoadImageFile(0, LFPG_SORT_PROC_TEX);
        img.SetColor(color);
    }

    protected void SetupText(TextWidget txt, float x, float y, float w, float h, int color, string text)
    {
        if (!txt)
            return;
        txt.SetPos(x, y);
        txt.SetSize(w, h);
        txt.SetColor(color);
        txt.SetText(text);
    }

    protected void SetupButton(ButtonWidget btn, TextWidget btnText, float x, float y, float w, float h, int bgColor, int textColor, string label)
    {
        if (!btn)
            return;
        btn.SetPos(x, y);
        btn.SetSize(w, h);

        // Find BG ImageWidget child (first child, inserted by layout)
        // and load procedural texture + tint. This gives buttons a
        // visible solid-color background — ButtonWidget.SetColor alone
        // does NOT render a visible fill without a style or image.
        Widget child = btn.GetChildren();
        while (child)
        {
            ImageWidget bgImg = ImageWidget.Cast(child);
            if (bgImg)
            {
                bgImg.LoadImageFile(0, LFPG_SORT_PROC_TEX);
                bgImg.SetColor(bgColor);
                break;
            }
            child = child.GetSibling();
        }

        if (btnText)
        {
            btnText.SetColor(textColor);
            btnText.SetText(label);
        }
    }

    // Helper: re-color a button's BG ImageWidget child.
    // Used by Refresh methods for dynamic state changes
    // (selected tab, active filter, etc.)
    protected void SetBtnBgColor(ButtonWidget btn, int color)
    {
        if (!btn)
            return;
        Widget child = btn.GetChildren();
        while (child)
        {
            ImageWidget bgImg = ImageWidget.Cast(child);
            if (bgImg)
            {
                bgImg.SetColor(color);
                return;
            }
            child = child.GetSibling();
        }
    }

    protected void SetupEditBox(EditBoxWidget edit, float x, float y, float w, float h)
    {
        if (!edit)
            return;
        edit.SetPos(x, y);
        edit.SetSize(w, h);
        edit.SetColor(LFPG_SORT_COL_BTN_NORMAL);
    }

    protected void SetupSeparator(ImageWidget sep, float x, float y, float w)
    {
        if (!sep)
            return;
        sep.SetPos(x, y);
        sep.SetSize(w, 1);
        sep.LoadImageFile(0, LFPG_SORT_PROC_TEX);
        sep.SetColor(LFPG_SORT_COL_SEPARATOR);
    }

    protected void DestroyWidgets()
    {
        ClearTagWidgets();

        if (m_Root)
        {
            m_Root.Unlink();
            m_Root = null;
        }
    }

    // =========================================================
    // Event: OnMouseButtonDown
    // ImageWidget does NOT fire OnClick — only ButtonWidget does.
    // OnMouseButtonDown fires for any widget without ignorepointer.
    // Used for the modal overlay (close on click outside panel).
    // =========================================================
    override bool OnMouseButtonDown(Widget w, int x, int y, int button)
    {
        if (!m_IsOpen)
            return false;

        if (button != 0)
            return false;

        // Modal overlay: clicking outside the panel closes the UI
        if (w == m_wModalOverlay)
        {
            DoClose();
            return true;
        }

        return false;
    }

    // =========================================================
    // Event: OnClick
    // Only fires for ButtonWidget descendants.
    // =========================================================
    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (!m_IsOpen)
            return false;

        if (button != 0)
            return false;

        // Close button
        if (w == m_wBtnClose)
        {
            DoClose();
            return true;
        }

        // SAVE button
        if (w == m_wBtnSave)
        {
            OnSave();
            return true;
        }

        // ORDENAR button
        if (w == m_wBtnSort)
        {
            OnRequestSort();
            return true;
        }

        // CLEAR OUTPUT
        if (w == m_wBtnClearOut)
        {
            OnClearOutput();
            return true;
        }

        // RESET ALL
        if (w == m_wBtnResetAll)
        {
            OnResetAll();
            return true;
        }

        // CATCH-ALL toggle
        if (w == m_wBtnCatchAll)
        {
            OnToggleCatchAll();
            return true;
        }

        // Output tabs
        int oi;
        for (oi = 0; oi < 6; oi = oi + 1)
        {
            if (w == m_wTabOut[oi])
            {
                SelectOutput(oi);
                return true;
            }
        }

        // View tabs
        if (w == m_wTabFilters)
        {
            m_ShowFilters = true;
            RefreshViewTabs();
            return true;
        }
        if (w == m_wTabContView)
        {
            m_ShowFilters = false;
            RefreshViewTabs();
            return true;
        }

        // Category buttons
        int catI;
        for (catI = 0; catI < 8; catI = catI + 1)
        {
            if (w == m_wCatBtn[catI])
            {
                OnToggleCategory(catI);
                return true;
            }
        }

        // Slot preset buttons
        int slI;
        for (slI = 0; slI < 4; slI = slI + 1)
        {
            if (w == m_wSlotPre[slI])
            {
                OnToggleSlotPreset(slI);
                return true;
            }
        }

        // PREFIX add
        if (w == m_wBtnPrefixAdd)
        {
            OnAddPrefix();
            return true;
        }

        // CONTAINS add
        if (w == m_wBtnContainsAdd)
        {
            OnAddContains();
            return true;
        }

        // SLOT custom add
        if (w == m_wBtnSlotAdd)
        {
            OnAddSlotCustom();
            return true;
        }

        return false;
    }

    // =========================================================
    // Event: OnChange (EditBox)
    // =========================================================
    override bool OnChange(Widget w, int x, int y, bool finished)
    {
        // Placeholder hide/show for PREFIX
        if (w == m_wEditPrefix && m_wPlhPrefix)
        {
            string prefixText = m_wEditPrefix.GetText();
            bool prefixEmpty = (prefixText == "");
            m_wPlhPrefix.Show(prefixEmpty);
        }

        // Placeholder hide/show for CONTAINS
        if (w == m_wEditContains && m_wPlhContains)
        {
            string containsText = m_wEditContains.GetText();
            bool containsEmpty = (containsText == "");
            m_wPlhContains.Show(containsEmpty);
        }

        return false;
    }


    // =========================================================
    // Event: OnUpdate (per-frame animation)
    // =========================================================
    override bool OnUpdate(Widget w, float timeslice)
    {
        if (!m_IsOpen)
            return false;

        // Glow pulse: alpha = 0.65 + 0.35 * sin(phase)
        m_GlowPhase = m_GlowPhase + timeslice * 2.5;
        if (m_GlowPhase > 6.283)
        {
            m_GlowPhase = m_GlowPhase - 6.283;
        }

        if (m_wPanelGlow)
        {
            float glowAlpha = 0.15 + 0.10 * Math.Sin(m_GlowPhase);
            m_wPanelGlow.SetAlpha(glowAlpha);
        }

        // Scanline subtle flicker (E6: increased visibility)
        if (m_wScanlines)
        {
            float flickChance = Math.RandomFloat01();
            if (flickChance < 0.03)
            {
                m_wScanlines.SetAlpha(0.12);
            }
            else
            {
                m_wScanlines.SetAlpha(0.06);
            }
        }

        // E3: Cursor-based hover polling
        PollHover();

        // ESC key check
        if (GetGame().GetInput())
        {
            if (GetGame().GetInput().LocalPress("UAUIBack", false))
            {
                DoClose();
                return true;
            }

            // M7: Enter key submits active EditBox
            if (GetGame().GetInput().LocalPress("UAUISelect", false))
            {
                TrySubmitActiveEdit();
            }
        }

        // H4: Save feedback timer — revert status to ONLINE after countdown
        if (m_SaveFeedbackTimer > 0.0)
        {
            m_SaveFeedbackTimer = m_SaveFeedbackTimer - timeslice;
            if (m_SaveFeedbackTimer <= 0.0)
            {
                m_SaveFeedbackTimer = 0.0;
                RefreshStatusText(true);
            }
        }

        // M5: Reset confirmation timeout
        if (m_ResetConfirmActive)
        {
            m_ResetConfirmTimer = m_ResetConfirmTimer - timeslice;
            if (m_ResetConfirmTimer <= 0.0)
            {
                m_ResetConfirmActive = false;
                m_ResetConfirmTimer = 0.0;
                if (m_wBtnResetAllText)
                {
                    m_wBtnResetAllText.SetText("RESET ALL");
                }
                SetBtnBgColor(m_wBtnResetAll, LFPG_SORT_COL_RED);
            }
        }

        // U1: Tags flash on rule add
        if (m_TagsFlashTimer > 0.0)
        {
            m_TagsFlashTimer = m_TagsFlashTimer - timeslice;
            if (m_TagsFlashTimer <= 0.0)
            {
                m_TagsFlashTimer = 0.0;
                if (m_wTagsBg)
                {
                    m_wTagsBg.SetColor(LFPG_SORT_COL_TAGS_BG);
                }
            }
        }

        return false;
    }

    // =========================================================

    // =========================================================
    // E3: Hover via cursor polling (OnMouseEnter not available)
    // Called from OnUpdate. Checks cursor against button bounds.
    // Only applies hover to BTN_NORMAL buttons (not active ones).
    // =========================================================
    protected void PollHover()
    {
        int mx = 0;
        int my = 0;
        GetMousePos(mx, my);

        // Relative to panel
        float relX = mx - m_PanelScreenX;
        float relY = my - m_PanelScreenY;

        // Early-out: cursor outside panel
        if (relX < 0.0)
        {
            ClearHover();
            return;
        }
        if (relY < 0.0)
        {
            ClearHover();
            return;
        }
        if (relX > LFPG_SORT_PANEL_W)
        {
            ClearHover();
            return;
        }
        if (relY > LFPG_SORT_PANEL_H)
        {
            ClearHover();
            return;
        }

        // FiltersView offset for nested buttons
        float fvOffX = 0.0;
        float fvOffY = 0.0;
        if (m_wFiltersView)
        {
            m_wFiltersView.GetPos(fvOffX, fvOffY);
        }

        // Check all buttons. First match wins (buttons don't overlap).
        ButtonWidget hitBtn = null;
        int oi;
        int ci;
        int si;

        // Header: BtnSort (direct child of panel)
        if (IsCursorInBtn(m_wBtnSort, relX, relY, 0.0, 0.0))
        {
            hitBtn = m_wBtnSort;
        }

        // Output tabs (direct children of panel)
        if (!hitBtn)
        {
            for (oi = 0; oi < 6; oi = oi + 1)
            {
                if (IsCursorInBtn(m_wTabOut[oi], relX, relY, 0.0, 0.0))
                {
                    hitBtn = m_wTabOut[oi];
                    break;
                }
            }
        }

        // View tabs (direct children of panel)
        if (!hitBtn)
        {
            if (IsCursorInBtn(m_wTabFilters, relX, relY, 0.0, 0.0))
            {
                hitBtn = m_wTabFilters;
            }
        }
        if (!hitBtn)
        {
            if (IsCursorInBtn(m_wTabContView, relX, relY, 0.0, 0.0))
            {
                hitBtn = m_wTabContView;
            }
        }

        // Category buttons (children of FiltersView)
        if (!hitBtn && m_ShowFilters)
        {
            for (ci = 0; ci < 8; ci = ci + 1)
            {
                if (IsCursorInBtn(m_wCatBtn[ci], relX, relY, fvOffX, fvOffY))
                {
                    hitBtn = m_wCatBtn[ci];
                    break;
                }
            }
        }

        // Slot presets (children of FiltersView)
        if (!hitBtn && m_ShowFilters)
        {
            for (si = 0; si < 4; si = si + 1)
            {
                if (IsCursorInBtn(m_wSlotPre[si], relX, relY, fvOffX, fvOffY))
                {
                    hitBtn = m_wSlotPre[si];
                    break;
                }
            }
        }

        // Add buttons (children of FiltersView)
        if (!hitBtn && m_ShowFilters)
        {
            if (IsCursorInBtn(m_wBtnPrefixAdd, relX, relY, fvOffX, fvOffY))
            {
                hitBtn = m_wBtnPrefixAdd;
            }
        }
        if (!hitBtn && m_ShowFilters)
        {
            if (IsCursorInBtn(m_wBtnContainsAdd, relX, relY, fvOffX, fvOffY))
            {
                hitBtn = m_wBtnContainsAdd;
            }
        }
        if (!hitBtn && m_ShowFilters)
        {
            if (IsCursorInBtn(m_wBtnSlotAdd, relX, relY, fvOffX, fvOffY))
            {
                hitBtn = m_wBtnSlotAdd;
            }
        }
        if (!hitBtn && m_ShowFilters)
        {
            if (IsCursorInBtn(m_wBtnCatchAll, relX, relY, fvOffX, fvOffY))
            {
                hitBtn = m_wBtnCatchAll;
            }
        }

        // Footer buttons (direct children of panel)
        if (!hitBtn)
        {
            if (IsCursorInBtn(m_wBtnClearOut, relX, relY, 0.0, 0.0))
            {
                hitBtn = m_wBtnClearOut;
            }
        }
        if (!hitBtn)
        {
            if (IsCursorInBtn(m_wBtnResetAll, relX, relY, 0.0, 0.0))
            {
                hitBtn = m_wBtnResetAll;
            }
        }
        if (!hitBtn)
        {
            if (IsCursorInBtn(m_wBtnSave, relX, relY, 0.0, 0.0))
            {
                hitBtn = m_wBtnSave;
            }
        }
        if (!hitBtn)
        {
            if (IsCursorInBtn(m_wBtnClose, relX, relY, 0.0, 0.0))
            {
                hitBtn = m_wBtnClose;
            }
        }

        // Apply hover state change
        ApplyHover(hitBtn);
    }

    protected bool IsCursorInBtn(ButtonWidget btn, float curX, float curY, float parentOffX, float parentOffY)
    {
        if (!btn)
            return false;

        float bx = 0.0;
        float by = 0.0;
        float bw = 0.0;
        float bh = 0.0;
        btn.GetPos(bx, by);
        btn.GetSize(bw, bh);
        bx = bx + parentOffX;
        by = by + parentOffY;

        if (curX < bx)
            return false;
        if (curY < by)
            return false;
        if (curX > bx + bw)
            return false;
        if (curY > by + bh)
            return false;
        return true;
    }

    protected void ApplyHover(ButtonWidget newBtn)
    {
        // No change
        if (newBtn == m_HoveredBtn)
            return;

        // Restore previous hovered button
        if (m_HoveredBtn)
        {
            int restoreCol = GetBtnBaseColor(m_HoveredBtn);
            SetBtnBgColor(m_HoveredBtn, restoreCol);
        }

        m_HoveredBtn = newBtn;

        // Apply hover to new button (only if base color is BTN_NORMAL)
        if (m_HoveredBtn)
        {
            int baseCol = GetBtnBaseColor(m_HoveredBtn);
            if (baseCol == LFPG_SORT_COL_BTN_NORMAL)
            {
                SetBtnBgColor(m_HoveredBtn, LFPG_SORT_COL_BTN_HOVER);
            }
        }
    }

    protected void ClearHover()
    {
        if (m_HoveredBtn)
        {
            int restoreCol = GetBtnBaseColor(m_HoveredBtn);
            SetBtnBgColor(m_HoveredBtn, restoreCol);
            m_HoveredBtn = null;
        }
    }

    // Returns the non-hovered color a button should have based on state
    protected int GetBtnBaseColor(ButtonWidget btn)
    {
        if (!btn)
            return LFPG_SORT_COL_BTN_NORMAL;

        int oi;
        int ci;
        int si;
        string catVal = "";
        string slotVal = "";
        LFPG_SortOutputConfig outCfg = null;

        // Output tabs
        for (oi = 0; oi < 6; oi = oi + 1)
        {
            if (btn == m_wTabOut[oi])
            {
                if (oi == m_SelectedOutput)
                    return LFPG_SORT_COL_GREEN;
                return LFPG_SORT_COL_BTN_NORMAL;
            }
        }

        // View tabs
        if (btn == m_wTabFilters)
        {
            if (m_ShowFilters)
                return LFPG_SORT_COL_GREEN;
            return LFPG_SORT_COL_BTN_NORMAL;
        }
        if (btn == m_wTabContView)
        {
            if (!m_ShowFilters)
                return LFPG_SORT_COL_GREEN;
            return LFPG_SORT_COL_BTN_NORMAL;
        }

        // Category buttons
        outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (outCfg)
        {
            for (ci = 0; ci < 8; ci = ci + 1)
            {
                if (btn == m_wCatBtn[ci])
                {
                    catVal = GetCategoryValue(ci);
                    if (outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, catVal))
                        return LFPG_SORT_COL_GREEN;
                    return LFPG_SORT_COL_BTN_NORMAL;
                }
            }

            for (si = 0; si < 4; si = si + 1)
            {
                if (btn == m_wSlotPre[si])
                {
                    slotVal = GetSlotPresetValue(si);
                    if (outCfg.HasRule(LFPG_SORT_FILTER_SLOT, slotVal))
                        return LFPG_SORT_COL_BLUE;
                    return LFPG_SORT_COL_BTN_NORMAL;
                }
            }

            if (btn == m_wBtnCatchAll)
            {
                if (outCfg.m_IsCatchAll)
                    return LFPG_SORT_COL_ORANGE;
                return LFPG_SORT_COL_BTN_NORMAL;
            }
        }

        // Special buttons
        if (btn == m_wBtnSort)
            return LFPG_SORT_COL_BLUE;
        if (btn == m_wBtnSave)
            return LFPG_SORT_COL_GREEN;
        if (btn == m_wBtnResetAll)
        {
            if (m_ResetConfirmActive)
                return LFPG_SORT_COL_ORANGE;
            return LFPG_SORT_COL_RED;
        }
        if (btn == m_wBtnPrefixAdd)
            return LFPG_SORT_COL_GREEN;
        if (btn == m_wBtnContainsAdd)
            return LFPG_SORT_COL_GREEN;
        if (btn == m_wBtnSlotAdd)
            return LFPG_SORT_COL_GREEN;

        return LFPG_SORT_COL_BTN_NORMAL;
    }

    // Action handlers
    // =========================================================
    protected void SelectOutput(int idx)
    {
        if (idx < 0 || idx >= 6)
            return;
        m_SelectedOutput = idx;
        RefreshOutputTabs();
        RefreshFiltersView();
        RefreshTagsPanel();
    }

    protected void OnToggleCategory(int catIdx)
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg)
            return;

        string catValue = GetCategoryValue(catIdx);
        bool hasIt = outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, catValue);

        if (hasIt)
        {
            // Remove it
            int ri;
            for (ri = 0; ri < outCfg.m_Rules.Count(); ri = ri + 1)
            {
                if (outCfg.m_Rules[ri].Equals(LFPG_SORT_FILTER_CATEGORY, catValue))
                {
                    outCfg.RemoveRuleAt(ri);
                    break;
                }
            }
        }
        else
        {
            outCfg.AddRule(LFPG_SORT_FILTER_CATEGORY, catValue);
        }

        RefreshFiltersView();
        RefreshTagsPanel();
        FlashTagsPanel();
    }

    protected void OnToggleSlotPreset(int presetIdx)
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg)
            return;

        string slotValue = GetSlotPresetValue(presetIdx);
        bool hasIt = outCfg.HasRule(LFPG_SORT_FILTER_SLOT, slotValue);

        if (hasIt)
        {
            int ri;
            for (ri = 0; ri < outCfg.m_Rules.Count(); ri = ri + 1)
            {
                if (outCfg.m_Rules[ri].Equals(LFPG_SORT_FILTER_SLOT, slotValue))
                {
                    outCfg.RemoveRuleAt(ri);
                    break;
                }
            }
        }
        else
        {
            outCfg.AddRule(LFPG_SORT_FILTER_SLOT, slotValue);
        }

        RefreshFiltersView();
        RefreshTagsPanel();
        FlashTagsPanel();
    }

    protected void OnAddPrefix()
    {
        if (!m_wEditPrefix)
            return;

        string val = m_wEditPrefix.GetText();
        if (val == "")
            return;

        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg)
            return;

        outCfg.AddRule(LFPG_SORT_FILTER_PREFIX, val);
        m_wEditPrefix.SetText("");
        if (m_wPlhPrefix)
        {
            m_wPlhPrefix.Show(true);
        }

        RefreshTagsPanel();
        FlashTagsPanel();
    }

    protected void OnAddContains()
    {
        if (!m_wEditContains)
            return;

        string val = m_wEditContains.GetText();
        if (val == "")
            return;

        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg)
            return;

        outCfg.AddRule(LFPG_SORT_FILTER_CONTAINS, val);
        m_wEditContains.SetText("");
        if (m_wPlhContains)
        {
            m_wPlhContains.Show(true);
        }

        RefreshTagsPanel();
        FlashTagsPanel();
    }

    protected void OnAddSlotCustom()
    {
        if (!m_wEditSlotMin || !m_wEditSlotMax)
            return;

        string minStr = m_wEditSlotMin.GetText();
        string maxStr = m_wEditSlotMax.GetText();
        if (minStr == "" || maxStr == "")
            return;

        int minVal = minStr.ToInt();
        int maxVal = maxStr.ToInt();
        if (minVal < 1)
        {
            minVal = 1;
        }
        if (maxVal < minVal)
        {
            maxVal = minVal;
        }

        string slotValue = minVal.ToString() + "-" + maxVal.ToString();

        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg)
            return;

        outCfg.AddRule(LFPG_SORT_FILTER_SLOT, slotValue);
        m_wEditSlotMin.SetText("");
        m_wEditSlotMax.SetText("");

        RefreshFiltersView();
        RefreshTagsPanel();
        FlashTagsPanel();
    }

    protected void OnToggleCatchAll()
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg)
            return;

        if (outCfg.m_IsCatchAll)
        {
            outCfg.m_IsCatchAll = false;
        }
        else
        {
            outCfg.m_IsCatchAll = true;
        }

        RefreshFiltersView();
        RefreshTagsPanel();
        FlashTagsPanel();
    }

    protected void OnClearOutput()
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg)
            return;

        outCfg.ClearRules();
        RefreshFiltersView();
        RefreshTagsPanel();
    }

    protected void OnResetAll()
    {
        // M5: Two-click confirmation
        if (!m_ResetConfirmActive)
        {
            m_ResetConfirmActive = true;
            m_ResetConfirmTimer = 3.0;
            if (m_wBtnResetAllText)
            {
                m_wBtnResetAllText.SetText("CONFIRM?");
            }
            SetBtnBgColor(m_wBtnResetAll, LFPG_SORT_COL_ORANGE);
            return;
        }

        // Second click: actually reset
        m_ResetConfirmActive = false;
        m_ResetConfirmTimer = 0.0;
        m_Config.ResetAll();
        if (m_wBtnResetAllText)
        {
            m_wBtnResetAllText.SetText("RESET ALL");
        }
        SetBtnBgColor(m_wBtnResetAll, LFPG_SORT_COL_RED);
        RefreshFiltersView();
        RefreshTagsPanel();
    }

    protected void OnSave()
    {
        string json = m_Config.ToJSON();
        LFPG_Util.Info("[SorterUI] SAVE config: " + json);

        // H4: Immediate feedback
        if (m_wStatus)
        {
            m_wStatus.SetText("SAVING...");
            m_wStatus.SetColor(LFPG_SORT_COL_ORANGE);
        }

        // S4: Send CONFIG_SAVE RPC to server
        #ifndef SERVER
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (player)
        {
            ScriptRPC rpc = new ScriptRPC();
            int subId = LFPG_RPC_SubId.SORTER_CONFIG_SAVE;
            rpc.Write(subId);
            rpc.Write(m_SorterNetLow);
            rpc.Write(m_SorterNetHigh);
            rpc.Write(json);
            rpc.Send(player, LFPG_RPC_CHANNEL, true, null);
        }
        #endif
    }

    protected void OnRequestSort()
    {
        LFPG_Util.Info("[SorterUI] REQUEST_SORT triggered");

        // S4: Send REQUEST_SORT RPC to server
        // Server handler: PlayerRPC dispatches to NetworkManager.HandleSorterRequestSort
        #ifndef SERVER
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (player)
        {
            ScriptRPC rpc = new ScriptRPC();
            int subId = LFPG_RPC_SubId.SORTER_REQUEST_SORT;
            rpc.Write(subId);
            rpc.Write(m_SorterNetLow);
            rpc.Write(m_SorterNetHigh);
            rpc.Send(player, LFPG_RPC_CHANNEL, true, null);
        }
        #endif
    }

    protected void OnRemoveRule(int ruleIdx)
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg)
            return;

        outCfg.RemoveRuleAt(ruleIdx);
        RefreshFiltersView();
        RefreshTagsPanel();
    }

    // M7: Submit active EditBox on Enter key
    protected void TrySubmitActiveEdit()
    {
        string pVal = "";
        string cVal = "";
        string sMin = "";
        string sMax = "";

        // Check PREFIX
        if (m_wEditPrefix)
        {
            pVal = m_wEditPrefix.GetText();
            if (pVal != "")
            {
                OnAddPrefix();
                return;
            }
        }

        // Check CONTAINS
        if (m_wEditContains)
        {
            cVal = m_wEditContains.GetText();
            if (cVal != "")
            {
                OnAddContains();
                return;
            }
        }

        // Check SLOT custom
        if (m_wEditSlotMin && m_wEditSlotMax)
        {
            sMin = m_wEditSlotMin.GetText();
            sMax = m_wEditSlotMax.GetText();
            if (sMin != "")
            {
                OnAddSlotCustom();
                return;
            }
            if (sMax != "")
            {
                OnAddSlotCustom();
                return;
            }
        }
    }

    // U1: Visual flash on the tags panel
    protected void FlashTagsPanel()
    {
        m_TagsFlashTimer = 0.4;
        if (m_wTagsBg)
        {
            m_wTagsBg.SetColor(LFPG_SORT_COL_GREEN_DIM);
        }
    }

    // =========================================================
    // UI refresh methods
    // =========================================================
    protected void RefreshOutputTabs()
    {
        int i;
        int outNum = 0;
        string tabLabel = "";
        string destN = "";
        LFPG_SortOutputConfig tabCfg = null;
        int tabRules = 0;
        bool tabCatchAll = false;
        bool hasContent = false;
        for (i = 0; i < 6; i = i + 1)
        {
            ButtonWidget tabBtn = m_wTabOut[i];
            if (!tabBtn)
                continue;

            int bgCol = LFPG_SORT_COL_BTN_NORMAL;
            int txtCol = LFPG_SORT_COL_TEXT;
            if (i == m_SelectedOutput)
            {
                bgCol = LFPG_SORT_COL_GREEN;
                txtCol = LFPG_SORT_COL_WHITE;
            }

            SetBtnBgColor(tabBtn, bgCol);
            TextWidget tabTxt = m_wTabOutText[i];
            if (tabTxt)
            {
                tabTxt.SetColor(txtCol);

                // E4: Show indicators for rules and linked dest
                outNum = i + 1;
                destN = GetDestName(i);
                tabCfg = m_Config.GetOutput(i);
                tabRules = 0;
                tabCatchAll = false;
                if (tabCfg)
                {
                    tabRules = tabCfg.GetRuleCount();
                    tabCatchAll = tabCfg.m_IsCatchAll;
                }

                hasContent = false;
                if (tabRules > 0 || tabCatchAll)
                {
                    hasContent = true;
                }

                if (destN != "")
                {
                    tabLabel = "O" + outNum.ToString() + " *";
                }
                else if (hasContent)
                {
                    tabLabel = "O" + outNum.ToString() + " +";
                }
                else
                {
                    tabLabel = "OUT " + outNum.ToString();
                }
                tabTxt.SetText(tabLabel);
            }
        }
    }

    protected void RefreshViewTabs()
    {
        // FILTROS tab
        if (m_wTabFilters)
        {
            int fBg = LFPG_SORT_COL_BTN_NORMAL;
            int fTxt = LFPG_SORT_COL_TEXT;
            if (m_ShowFilters)
            {
                fBg = LFPG_SORT_COL_GREEN;
                fTxt = LFPG_SORT_COL_WHITE;
            }
            SetBtnBgColor(m_wTabFilters, fBg);
            if (m_wTabFiltersText)
            {
                m_wTabFiltersText.SetColor(fTxt);
            }
        }

        // CONTENEDOR tab
        if (m_wTabContView)
        {
            int cBg = LFPG_SORT_COL_BTN_NORMAL;
            int cTxt = LFPG_SORT_COL_TEXT;
            if (!m_ShowFilters)
            {
                cBg = LFPG_SORT_COL_GREEN;
                cTxt = LFPG_SORT_COL_WHITE;
            }
            SetBtnBgColor(m_wTabContView, cBg);
            if (m_wTabContViewText)
            {
                m_wTabContViewText.SetColor(cTxt);
            }
        }

        // Show/hide views
        if (m_wTagsPanel)
        {
            m_wTagsPanel.Show(m_ShowFilters);
        }
        if (m_wContainerView)
        {
            m_wContainerView.Show(!m_ShowFilters);
        }
    }

    protected void RefreshFiltersView()
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg)
            return;

        // Category buttons: highlight active
        int ci;
        for (ci = 0; ci < 8; ci = ci + 1)
        {
            string catVal = GetCategoryValue(ci);
            bool active = outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, catVal);
            int catBgCol = LFPG_SORT_COL_BTN_NORMAL;
            int catTxtCol = LFPG_SORT_COL_TEXT;
            if (active)
            {
                catBgCol = LFPG_SORT_COL_GREEN;
                catTxtCol = LFPG_SORT_COL_WHITE;
            }
            if (m_wCatBtn[ci])
            {
                SetBtnBgColor(m_wCatBtn[ci], catBgCol);
            }
            if (m_wCatBtnText[ci])
            {
                m_wCatBtnText[ci].SetColor(catTxtCol);
            }
        }

        // Slot presets: highlight active
        int si;
        for (si = 0; si < 4; si = si + 1)
        {
            string slotVal = GetSlotPresetValue(si);
            bool slotActive = outCfg.HasRule(LFPG_SORT_FILTER_SLOT, slotVal);
            int slotBgCol = LFPG_SORT_COL_BTN_NORMAL;
            int slotTxtCol = LFPG_SORT_COL_TEXT;
            if (slotActive)
            {
                slotBgCol = LFPG_SORT_COL_BLUE;
                slotTxtCol = LFPG_SORT_COL_WHITE;
            }
            if (m_wSlotPre[si])
            {
                SetBtnBgColor(m_wSlotPre[si], slotBgCol);
            }
            if (m_wSlotPreText[si])
            {
                m_wSlotPreText[si].SetColor(slotTxtCol);
            }
        }

        // Catch-all button
        if (m_wBtnCatchAll)
        {
            int caCol = LFPG_SORT_COL_BTN_NORMAL;
            int caTxtCol = LFPG_SORT_COL_TEXT;
            if (outCfg.m_IsCatchAll)
            {
                caCol = LFPG_SORT_COL_ORANGE;
                caTxtCol = LFPG_SORT_COL_WHITE;
            }
            SetBtnBgColor(m_wBtnCatchAll, caCol);
            if (m_wBtnCatchAllText)
            {
                m_wBtnCatchAllText.SetColor(caTxtCol);
            }
        }
    }

    protected void RefreshTagsPanel()
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg)
            return;

        int ruleCount = outCfg.GetRuleCount();
        bool hasCatchAll = outCfg.m_IsCatchAll;

        // S4: Prepend dest container name if available
        string destInfo = GetDestName(m_SelectedOutput);
        string summary = "";
        if (destInfo != "")
        {
            summary = "  DEST: " + destInfo;
        }

        // Build text summary of all active rules
        int ri;
        for (ri = 0; ri < ruleCount; ri = ri + 1)
        {
            ref LFPG_SortFilterRule rule = outCfg.m_Rules[ri];
            if (!rule)
                continue;
            if (summary != "")
            {
                summary = summary + "\n";
            }
            string ruleLabel = rule.GetDisplayLabel();
            summary = summary + "  " + ruleLabel;
        }

        if (hasCatchAll)
        {
            if (summary != "")
            {
                summary = summary + "\n";
            }
            summary = summary + "  CATCH-ALL";
        }

        // Show "no rules" or summary
        bool hasDestInfo = (destInfo != "");
        bool showEmpty = (ruleCount == 0 && !hasCatchAll && !hasDestInfo);
        if (m_wTagsEmpty)
        {
            if (showEmpty)
            {
                m_wTagsEmpty.SetText("No rules configured for this output");
                m_wTagsEmpty.SetColor(LFPG_SORT_COL_TEXT_DIM);
                m_wTagsEmpty.Show(true);
            }
            else
            {
                m_wTagsEmpty.SetText(summary);
                m_wTagsEmpty.SetColor(LFPG_SORT_COL_TEXT);
                m_wTagsEmpty.Show(true);
                // Resize for multiline content
                float lineCount = ruleCount;
                if (hasCatchAll)
                {
                    lineCount = lineCount + 1;
                }
                if (hasDestInfo)
                {
                    lineCount = lineCount + 1;
                }
                float textH = lineCount * 18.0;
                if (textH < 40.0)
                {
                    textH = 40.0;
                }
                m_wTagsEmpty.SetSize(280, textH);
            }
        }
    }

    // Dynamic chip creation reserved for polish pass (WrapSpacer + CreateWidgets).
    // Sprint S1 uses text-based display via m_wTagsEmpty (above).

    protected void ClearTagWidgets()
    {
        // Reserved for dynamic widget cleanup in future polish pass.
        m_TagWidgets.Clear();
    }

    protected void RefreshStatusText(bool powered)
    {
        if (!m_wStatus)
            return;

        if (powered)
        {
            m_wStatus.SetText("ONLINE");
            m_wStatus.SetColor(LFPG_SORT_COL_GREEN);
        }
        else
        {
            m_wStatus.SetText("OFFLINE");
            m_wStatus.SetColor(LFPG_SORT_COL_RED);
        }
    }

    // H4: Server ACK for config save — show feedback for 2.5 seconds
    static void OnSaveAck(bool success)
    {
        if (!s_Instance)
            return;
        if (!s_Instance.m_IsOpen)
            return;
        if (!s_Instance.m_wStatus)
            return;

        if (success)
        {
            s_Instance.m_wStatus.SetText("SAVED");
            s_Instance.m_wStatus.SetColor(LFPG_SORT_COL_GREEN);
        }
        else
        {
            s_Instance.m_wStatus.SetText("ERROR");
            s_Instance.m_wStatus.SetColor(LFPG_SORT_COL_RED);
        }
        s_Instance.m_SaveFeedbackTimer = 2.5;
    }

    // =========================================================
    // Lookup helpers (avoid arrays of string — Enforce limitation)
    // =========================================================

    // S4: Dest container name per output index
    protected string GetDestName(int idx)
    {
        if (idx == 0) return m_DestName0;
        if (idx == 1) return m_DestName1;
        if (idx == 2) return m_DestName2;
        if (idx == 3) return m_DestName3;
        if (idx == 4) return m_DestName4;
        if (idx == 5) return m_DestName5;
        return "";
    }

    protected string GetCategoryLabel(int idx)
    {
        if (idx == 0) return LFPG_SORT_CAT_LABELS_0;
        if (idx == 1) return LFPG_SORT_CAT_LABELS_1;
        if (idx == 2) return LFPG_SORT_CAT_LABELS_2;
        if (idx == 3) return LFPG_SORT_CAT_LABELS_3;
        if (idx == 4) return LFPG_SORT_CAT_LABELS_4;
        if (idx == 5) return LFPG_SORT_CAT_LABELS_5;
        if (idx == 6) return LFPG_SORT_CAT_LABELS_6;
        if (idx == 7) return LFPG_SORT_CAT_LABELS_7;
        return "?";
    }

    protected string GetCategoryValue(int idx)
    {
        if (idx == 0) return LFPG_SORT_CAT_VALUES_0;
        if (idx == 1) return LFPG_SORT_CAT_VALUES_1;
        if (idx == 2) return LFPG_SORT_CAT_VALUES_2;
        if (idx == 3) return LFPG_SORT_CAT_VALUES_3;
        if (idx == 4) return LFPG_SORT_CAT_VALUES_4;
        if (idx == 5) return LFPG_SORT_CAT_VALUES_5;
        if (idx == 6) return LFPG_SORT_CAT_VALUES_6;
        if (idx == 7) return LFPG_SORT_CAT_VALUES_7;
        return "MISC";
    }

    protected string GetSlotPresetLabel(int idx)
    {
        if (idx == 0) return LFPG_SORT_SLOT_LABELS_0;
        if (idx == 1) return LFPG_SORT_SLOT_LABELS_1;
        if (idx == 2) return LFPG_SORT_SLOT_LABELS_2;
        if (idx == 3) return LFPG_SORT_SLOT_LABELS_3;
        return "?";
    }

    protected string GetSlotPresetValue(int idx)
    {
        if (idx == 0) return LFPG_SORT_SLOT_TINY;
        if (idx == 1) return LFPG_SORT_SLOT_SMALL;
        if (idx == 2) return LFPG_SORT_SLOT_MEDIUM;
        if (idx == 3) return LFPG_SORT_SLOT_LARGE;
        return "1-1";
    }
};
