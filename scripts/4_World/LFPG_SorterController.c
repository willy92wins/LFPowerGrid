// =========================================================
// LF_PowerGrid — Sorter Controller (Dabs MVC, v2.3 P3 Sprint)
//
// v2.3 changes (P3 Performance & Polish):
//   S5: Extracted GetStatusColor (eliminates 40-line duplication)
//   S6: Merged save/sort feedback timers into m_FeedbackTimer
//   E1: String literals converted to local variables
//   V2: Tab rule indicators (* on tabs with rules/catch-all)
//   R4: GetGame() null-guard in BtnSave/BtnSort
//
// v2.2 changes (Polish Sprint):
//   - Visual disabled state when unpaired (IGNOREPOINTER + dim)
//   - Sort feedback in StatusLabel (client-only, 3s timer)
//   - UI click/action sounds on all interactions
//   - Color cache integration for View hover system
//
// v2.1 changes (Floating Window Sprint):
//   Bug 3: BtnCloseX relay
//   Bug 5: Pairing state (m_IsPaired) controls banner + status
//   Bug 6: Guards all filter handlers when unpaired
//   Bug 7-9: Color palette bumped
//
// Enforce Script: no ternaries, no ++/--, no foreach.
// =========================================================

class LFPG_SorterController extends ViewController
{
    // ── Bound text properties (name = layout widget with ViewBinding) ──
    string HeaderTitle;
    string RuleCount;
    string MatchCount;
    string PreviewCount;
    string DestName;

    // ── Bound EditBox properties (two-way) ──
    string EditPrefix;
    string EditContains;
    string EditSlotMin;
    string EditSlotMax;

    // ── ObservableCollections ──
    ref ObservableCollection<ref LFPG_SorterTagView> TagsList;
    ref ObservableCollection<ref LFPG_SorterPreviewRow> PreviewItems;

    // ── Internal state ──
    protected ref LFPG_SortConfig m_Config;
    protected int m_SelectedOutput;
    protected bool m_ShowRules;
    protected bool m_ResetConfirmActive;
    protected float m_ResetTimer;
    protected float m_FeedbackTimer;

    // ── Pairing state (Bug #5/#6) ──
    protected bool m_IsPaired;
    protected string m_ContainerDisplayName;

    // ── RPC identity ──
    protected int m_SorterNetLow;
    protected int m_SorterNetHigh;

    // ── Dest names ──
    protected string m_Dest0;
    protected string m_Dest1;
    protected string m_Dest2;
    protected string m_Dest3;
    protected string m_Dest4;
    protected string m_Dest5;

    // ── Category ──
    protected string m_CatLabel0; protected string m_CatValue0;
    protected string m_CatLabel1; protected string m_CatValue1;
    protected string m_CatLabel2; protected string m_CatValue2;
    protected string m_CatLabel3; protected string m_CatValue3;
    protected string m_CatLabel4; protected string m_CatValue4;
    protected string m_CatLabel5; protected string m_CatValue5;
    protected string m_CatLabel6; protected string m_CatValue6;
    protected string m_CatLabel7; protected string m_CatValue7;

    // ── Widget refs (auto-assigned by name match) ──
    TextWidget StatusLabel;
    ImageWidget StatusDot;

    // Output tabs
    ImageWidget TabOut0Bg; ImageWidget TabOut1Bg; ImageWidget TabOut2Bg;
    ImageWidget TabOut3Bg; ImageWidget TabOut4Bg; ImageWidget TabOut5Bg;
    TextWidget TabOut0Text; TextWidget TabOut1Text; TextWidget TabOut2Text;
    TextWidget TabOut3Text; TextWidget TabOut4Text; TextWidget TabOut5Text;
    // View tabs
    ImageWidget TabRulesBg; ImageWidget TabPreviewBg;
    TextWidget TabRulesText; TextWidget TabPreviewText;
    // Category
    ImageWidget CatBtn0Bg; ImageWidget CatBtn1Bg; ImageWidget CatBtn2Bg; ImageWidget CatBtn3Bg;
    ImageWidget CatBtn4Bg; ImageWidget CatBtn5Bg; ImageWidget CatBtn6Bg; ImageWidget CatBtn7Bg;
    TextWidget CatBtn0Text; TextWidget CatBtn1Text; TextWidget CatBtn2Text; TextWidget CatBtn3Text;
    TextWidget CatBtn4Text; TextWidget CatBtn5Text; TextWidget CatBtn6Text; TextWidget CatBtn7Text;
    // Slot
    ImageWidget SlotPre0Bg; ImageWidget SlotPre1Bg; ImageWidget SlotPre2Bg; ImageWidget SlotPre3Bg;
    TextWidget SlotPre0Text; TextWidget SlotPre1Text; TextWidget SlotPre2Text; TextWidget SlotPre3Text;
    // Catch-all
    ImageWidget BtnCatchAllBg; TextWidget BtnCatchAllText;
    // Footer + header button BGs
    ImageWidget BtnSortBg; ImageWidget BtnSaveBg;
    ImageWidget BtnResetAllBg; ImageWidget BtnClearOutBg; ImageWidget BtnCloseBg;
    ImageWidget BtnPrefixAddBg; ImageWidget BtnContainsAddBg; ImageWidget BtnSlotAddBg;
    TextWidget BtnResetAllText;
    TextWidget BtnSortText; TextWidget BtnSaveText;
    TextWidget BtnClearOutText; TextWidget BtnCloseText;
    // Label refs
    TextWidget LblCategory; TextWidget LblPrefix; TextWidget LblContains;
    TextWidget LblSlot; TextWidget LblSlotDash;
    TextWidget LblActiveRules; TextWidget DestLabel;
    TextWidget LblPreview;
    // Panels
    Widget RulesPanel; Widget PreviewPanel;
    Widget DestIndicator;
    TextWidget TagsEmpty; TextWidget PreviewEmpty;

    // E3: PROC constant removed — use LFPG_SorterView.PROC_WHITE instead

    // =========================================================
    void LFPG_SorterController()
    {
        TagsList = new ObservableCollection<ref LFPG_SorterTagView>(this);
        PreviewItems = new ObservableCollection<ref LFPG_SorterPreviewRow>(this);
        m_Config = new LFPG_SortConfig();
        m_SelectedOutput = 0;
        m_ShowRules = true;
        m_ResetConfirmActive = false;
        m_ResetTimer = 0.0;
        m_FeedbackTimer = 0.0;
        m_SorterNetLow = 0;
        m_SorterNetHigh = 0;
        m_IsPaired = false;
        m_ContainerDisplayName = "";
        m_CatLabel0 = "Weapons";    m_CatValue0 = "WEAPON";
        m_CatLabel1 = "Attach";     m_CatValue1 = "ATTACHMENT";
        m_CatLabel2 = "Ammo";       m_CatValue2 = "AMMO";
        m_CatLabel3 = "Clothing";   m_CatValue3 = "CLOTHING";
        m_CatLabel4 = "Food";       m_CatValue4 = "FOOD";
        m_CatLabel5 = "Medical";    m_CatValue5 = "MEDICAL";
        m_CatLabel6 = "Tools";      m_CatValue6 = "TOOL";
        m_CatLabel7 = "Misc";       m_CatValue7 = "MISC";

        m_Dest0 = ""; m_Dest1 = ""; m_Dest2 = "";
        m_Dest3 = ""; m_Dest4 = ""; m_Dest5 = "";
    }

    // =========================================================
    // v2.6: Manual binding fallback for widgets that Dabs MVC
    // auto-binding sometimes fails to resolve (observed on
    // odd-indexed tab ImageWidgets). Called from View.DoOpen.
    // =========================================================
    void EnsureBindings(Widget layoutRoot)
    {
        if (!layoutRoot)
            return;

        string wName = "";
        string btnN = "";

        // v2.7: Force all tab lookups (no null guards).
        // Dabs MVC auto-bind can set non-null but WRONG refs
        // for widgets inside ButtonWidget (observed on odd-indexed
        // tabs: 1,3,5). Forcing the lookup overwrites any stale ref.
        // Fallback: find the ButtonWidget by name, walk children.

        // Output tab backgrounds — force lookup
        wName = "TabOut0Bg";
        TabOut0Bg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName));
        btnN = "TabOut0";
        if (!TabOut0Bg) { TabOut0Bg = FindBtnChildBg(layoutRoot, btnN); }
        wName = "TabOut1Bg";
        TabOut1Bg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName));
        btnN = "TabOut1";
        if (!TabOut1Bg) { TabOut1Bg = FindBtnChildBg(layoutRoot, btnN); }
        wName = "TabOut2Bg";
        TabOut2Bg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName));
        btnN = "TabOut2";
        if (!TabOut2Bg) { TabOut2Bg = FindBtnChildBg(layoutRoot, btnN); }
        wName = "TabOut3Bg";
        TabOut3Bg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName));
        btnN = "TabOut3";
        if (!TabOut3Bg) { TabOut3Bg = FindBtnChildBg(layoutRoot, btnN); }
        wName = "TabOut4Bg";
        TabOut4Bg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName));
        btnN = "TabOut4";
        if (!TabOut4Bg) { TabOut4Bg = FindBtnChildBg(layoutRoot, btnN); }
        wName = "TabOut5Bg";
        TabOut5Bg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName));
        btnN = "TabOut5";
        if (!TabOut5Bg) { TabOut5Bg = FindBtnChildBg(layoutRoot, btnN); }

        // Output tab text — force lookup
        wName = "TabOut0Text";
        TabOut0Text = TextWidget.Cast(layoutRoot.FindAnyWidget(wName));
        btnN = "TabOut0";
        if (!TabOut0Text) { TabOut0Text = FindBtnChildText(layoutRoot, btnN); }
        wName = "TabOut1Text";
        TabOut1Text = TextWidget.Cast(layoutRoot.FindAnyWidget(wName));
        btnN = "TabOut1";
        if (!TabOut1Text) { TabOut1Text = FindBtnChildText(layoutRoot, btnN); }
        wName = "TabOut2Text";
        TabOut2Text = TextWidget.Cast(layoutRoot.FindAnyWidget(wName));
        btnN = "TabOut2";
        if (!TabOut2Text) { TabOut2Text = FindBtnChildText(layoutRoot, btnN); }
        wName = "TabOut3Text";
        TabOut3Text = TextWidget.Cast(layoutRoot.FindAnyWidget(wName));
        btnN = "TabOut3";
        if (!TabOut3Text) { TabOut3Text = FindBtnChildText(layoutRoot, btnN); }
        wName = "TabOut4Text";
        TabOut4Text = TextWidget.Cast(layoutRoot.FindAnyWidget(wName));
        btnN = "TabOut4";
        if (!TabOut4Text) { TabOut4Text = FindBtnChildText(layoutRoot, btnN); }
        wName = "TabOut5Text";
        TabOut5Text = TextWidget.Cast(layoutRoot.FindAnyWidget(wName));
        btnN = "TabOut5";
        if (!TabOut5Text) { TabOut5Text = FindBtnChildText(layoutRoot, btnN); }

        // View tabs — force lookup
        wName = "TabRulesBg";
        TabRulesBg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName));
        btnN = "TabRules";
        if (!TabRulesBg) { TabRulesBg = FindBtnChildBg(layoutRoot, btnN); }
        wName = "TabPreviewBg";
        TabPreviewBg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName));
        btnN = "TabPreview";
        if (!TabPreviewBg) { TabPreviewBg = FindBtnChildBg(layoutRoot, btnN); }
        wName = "TabRulesText";
        TabRulesText = TextWidget.Cast(layoutRoot.FindAnyWidget(wName));
        btnN = "TabRules";
        if (!TabRulesText) { TabRulesText = FindBtnChildText(layoutRoot, btnN); }
        wName = "TabPreviewText";
        TabPreviewText = TextWidget.Cast(layoutRoot.FindAnyWidget(wName));
        btnN = "TabPreview";
        if (!TabPreviewText) { TabPreviewText = FindBtnChildText(layoutRoot, btnN); }

        // Category button bgs
        wName = "CatBtn0Bg";
        if (!CatBtn0Bg) { CatBtn0Bg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "CatBtn1Bg";
        if (!CatBtn1Bg) { CatBtn1Bg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "CatBtn2Bg";
        if (!CatBtn2Bg) { CatBtn2Bg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "CatBtn3Bg";
        if (!CatBtn3Bg) { CatBtn3Bg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "CatBtn4Bg";
        if (!CatBtn4Bg) { CatBtn4Bg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "CatBtn5Bg";
        if (!CatBtn5Bg) { CatBtn5Bg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "CatBtn6Bg";
        if (!CatBtn6Bg) { CatBtn6Bg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "CatBtn7Bg";
        if (!CatBtn7Bg) { CatBtn7Bg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }

        // Category button text
        wName = "CatBtn0Text";
        if (!CatBtn0Text) { CatBtn0Text = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "CatBtn1Text";
        if (!CatBtn1Text) { CatBtn1Text = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "CatBtn2Text";
        if (!CatBtn2Text) { CatBtn2Text = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "CatBtn3Text";
        if (!CatBtn3Text) { CatBtn3Text = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "CatBtn4Text";
        if (!CatBtn4Text) { CatBtn4Text = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "CatBtn5Text";
        if (!CatBtn5Text) { CatBtn5Text = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "CatBtn6Text";
        if (!CatBtn6Text) { CatBtn6Text = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "CatBtn7Text";
        if (!CatBtn7Text) { CatBtn7Text = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }

        // Slot preset bgs
        wName = "SlotPre0Bg";
        if (!SlotPre0Bg) { SlotPre0Bg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "SlotPre1Bg";
        if (!SlotPre1Bg) { SlotPre1Bg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "SlotPre2Bg";
        if (!SlotPre2Bg) { SlotPre2Bg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "SlotPre3Bg";
        if (!SlotPre3Bg) { SlotPre3Bg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }

        // Slot preset text
        wName = "SlotPre0Text";
        if (!SlotPre0Text) { SlotPre0Text = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "SlotPre1Text";
        if (!SlotPre1Text) { SlotPre1Text = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "SlotPre2Text";
        if (!SlotPre2Text) { SlotPre2Text = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "SlotPre3Text";
        if (!SlotPre3Text) { SlotPre3Text = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }

        // Catch-all
        wName = "BtnCatchAllBg";
        if (!BtnCatchAllBg) { BtnCatchAllBg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "BtnCatchAllText";
        if (!BtnCatchAllText) { BtnCatchAllText = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }

        // Footer buttons
        wName = "BtnSortBg";
        if (!BtnSortBg) { BtnSortBg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "BtnSaveBg";
        if (!BtnSaveBg) { BtnSaveBg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "BtnResetAllBg";
        if (!BtnResetAllBg) { BtnResetAllBg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "BtnClearOutBg";
        if (!BtnClearOutBg) { BtnClearOutBg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "BtnCloseBg";
        if (!BtnCloseBg) { BtnCloseBg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "BtnPrefixAddBg";
        if (!BtnPrefixAddBg) { BtnPrefixAddBg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "BtnContainsAddBg";
        if (!BtnContainsAddBg) { BtnContainsAddBg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "BtnSlotAddBg";
        if (!BtnSlotAddBg) { BtnSlotAddBg = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }

        // Footer text
        wName = "BtnSortText";
        if (!BtnSortText) { BtnSortText = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "BtnSaveText";
        if (!BtnSaveText) { BtnSaveText = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "BtnResetAllText";
        if (!BtnResetAllText) { BtnResetAllText = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "BtnClearOutText";
        if (!BtnClearOutText) { BtnClearOutText = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "BtnCloseText";
        if (!BtnCloseText) { BtnCloseText = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }

        // Status
        wName = "StatusLabel";
        if (!StatusLabel) { StatusLabel = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "StatusDot";
        if (!StatusDot) { StatusDot = ImageWidget.Cast(layoutRoot.FindAnyWidget(wName)); }

        // Labels
        wName = "LblCategory";
        if (!LblCategory) { LblCategory = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "LblPrefix";
        if (!LblPrefix) { LblPrefix = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "LblContains";
        if (!LblContains) { LblContains = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "LblSlot";
        if (!LblSlot) { LblSlot = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "LblSlotDash";
        if (!LblSlotDash) { LblSlotDash = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "LblActiveRules";
        if (!LblActiveRules) { LblActiveRules = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "DestLabel";
        if (!DestLabel) { DestLabel = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "LblPreview";
        if (!LblPreview) { LblPreview = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "TagsEmpty";
        if (!TagsEmpty) { TagsEmpty = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }
        wName = "PreviewEmpty";
        if (!PreviewEmpty) { PreviewEmpty = TextWidget.Cast(layoutRoot.FindAnyWidget(wName)); }

        // Panels
        wName = "RulesPanel";
        if (!RulesPanel) { RulesPanel = layoutRoot.FindAnyWidget(wName); }
        wName = "PreviewPanel";
        if (!PreviewPanel) { PreviewPanel = layoutRoot.FindAnyWidget(wName); }
        wName = "DestIndicator";
        if (!DestIndicator) { DestIndicator = layoutRoot.FindAnyWidget(wName); }
    }

    // v2.7: Fallback helpers — find a ButtonWidget by name,
    // then walk its immediate children to find the ImageWidget
    // (Bg) or TextWidget (Text). Handles cases where
    // FindAnyWidget fails for widgets nested inside ButtonWidget.
    protected ImageWidget FindBtnChildBg(Widget layoutRoot, string btnName)
    {
        if (!layoutRoot)
            return null;

        Widget btnW = layoutRoot.FindAnyWidget(btnName);
        if (!btnW)
            return null;

        Widget child = btnW.GetChildren();
        ImageWidget imgChild = null;
        while (child)
        {
            imgChild = ImageWidget.Cast(child);
            if (imgChild)
            {
                return imgChild;
            }
            child = child.GetSibling();
        }
        return null;
    }

    protected TextWidget FindBtnChildText(Widget layoutRoot, string btnName)
    {
        if (!layoutRoot)
            return null;

        Widget btnW = layoutRoot.FindAnyWidget(btnName);
        if (!btnW)
            return null;

        Widget child = btnW.GetChildren();
        TextWidget txtChild = null;
        while (child)
        {
            txtChild = TextWidget.Cast(child);
            if (txtChild)
            {
                return txtChild;
            }
            child = child.GetSibling();
        }
        return null;
    }

    // =========================================================
    void InitFromRPC(string configJSON, string containerName, string d0, string d1, string d2, string d3, string d4, string d5, int netLow, int netHigh)
    {
        m_SorterNetLow = netLow;
        m_SorterNetHigh = netHigh;
        m_SelectedOutput = 0;
        m_ShowRules = true;
        m_ResetConfirmActive = false;
        m_FeedbackTimer = 0.0;
        m_Dest0 = d0; m_Dest1 = d1; m_Dest2 = d2;
        m_Dest3 = d3; m_Dest4 = d4; m_Dest5 = d5;

        // Pairing state (Bug #5/#6)
        m_ContainerDisplayName = containerName;
        if (containerName != "")
        {
            m_IsPaired = true;
        }
        else
        {
            m_IsPaired = false;
        }

        if (configJSON != "")
        {
            m_Config.FromJSON(configJSON);
        }
        else
        {
            m_Config.ResetAll();
        }

        string sorterTitle = "SORTER";
        HeaderTitle = sorterTitle;
        string propHeader = "HeaderTitle";
        NotifyPropertyChanged(propHeader, false);

        if (m_IsPaired)
        {
            string stOnline = "ONLINE";
            SetStatus(stOnline);
        }
        else
        {
            string stNoLink = "NO LINK";
            SetStatus(stNoLink);
        }
        ApplyInitialColors();
        ApplyInitialLabels();
        RefreshAll();
    }

    // =========================================================
    // Tint every static button + label on first open
    // =========================================================
    protected void ApplyInitialColors()
    {
        int V = LFPG_SorterView.COL_BTN;
        int G = LFPG_SorterView.COL_GREEN_BTN;
        int B = LFPG_SorterView.COL_BLUE_BTN;
        int R = LFPG_SorterView.COL_RED_BTN;
        int DIM = LFPG_SorterView.COL_TEXT_DIM;
        int MID = LFPG_SorterView.COL_TEXT_MID;
        int GRN = LFPG_SorterView.COL_GREEN;
        int WHT = LFPG_SorterView.COL_TEXT;

        // Footer buttons
        TintBg(BtnSortBg, B);
        TintBg(BtnSaveBg, G);
        TintBg(BtnResetAllBg, R);
        TintBg(BtnClearOutBg, V);
        TintBg(BtnCloseBg, V);

        // Footer text colors
        SetTxtCol(BtnSortText, WHT);
        SetTxtCol(BtnSaveText, GRN);
        SetTxtCol(BtnResetAllText, WHT);
        SetTxtCol(BtnClearOutText, MID);
        SetTxtCol(BtnCloseText, DIM);

        // Add buttons (green)
        TintBg(BtnPrefixAddBg, G);
        TintBg(BtnContainsAddBg, G);
        TintBg(BtnSlotAddBg, G);

        // Labels
        SetTxtCol(LblCategory, DIM);
        SetTxtCol(LblPrefix, DIM);
        SetTxtCol(LblContains, DIM);
        SetTxtCol(LblSlot, DIM);
        SetTxtCol(LblSlotDash, MID);
        SetTxtCol(LblActiveRules, DIM);
        SetTxtCol(DestLabel, DIM);
        SetTxtCol(LblPreview, DIM);
        SetTxtCol(TagsEmpty, DIM);
        SetTxtCol(PreviewEmpty, DIM);
    }

    protected void ApplyInitialLabels()
    {
        SetBtnLabel(CatBtn0Text, m_CatLabel0);
        SetBtnLabel(CatBtn1Text, m_CatLabel1);
        SetBtnLabel(CatBtn2Text, m_CatLabel2);
        SetBtnLabel(CatBtn3Text, m_CatLabel3);
        SetBtnLabel(CatBtn4Text, m_CatLabel4);
        SetBtnLabel(CatBtn5Text, m_CatLabel5);
        SetBtnLabel(CatBtn6Text, m_CatLabel6);
        SetBtnLabel(CatBtn7Text, m_CatLabel7);

        string lblTiny = "Tiny";
        string lblSmall = "Small";
        string lblMed = "Med";
        string lblLarge = "Large";
        SetBtnLabel(SlotPre0Text, lblTiny);
        SetBtnLabel(SlotPre1Text, lblSmall);
        SetBtnLabel(SlotPre2Text, lblMed);
        SetBtnLabel(SlotPre3Text, lblLarge);
    }

    protected void SetBtnLabel(TextWidget txt, string label)
    {
        if (txt) { txt.SetText(label); }
    }

    // =========================================================
    // Visual disabled state (v2.2) — dim controls when unpaired
    // IGNOREPOINTER does NOT propagate to children in DayZ,
    // so we rely on m_IsPaired guards in each handler +
    // visual dimming of every interactive element.
    // =========================================================
    protected void SetControlsEnabled(bool enabled)
    {
        int dimBg = 0xFF151E2E;
        int dimTxt = LFPG_SorterView.COL_TEXT_DIM;

        // Footer action buttons
        if (enabled)
        {
            TintBg(BtnSortBg, LFPG_SorterView.COL_BLUE_BTN);
            TintBg(BtnClearOutBg, LFPG_SorterView.COL_BTN);
            TintBg(BtnSaveBg, LFPG_SorterView.COL_GREEN_BTN);
            SetTxtCol(BtnSortText, LFPG_SorterView.COL_TEXT);
            SetTxtCol(BtnClearOutText, LFPG_SorterView.COL_TEXT_MID);
            SetTxtCol(BtnSaveText, LFPG_SorterView.COL_GREEN);
        }
        else
        {
            TintBg(BtnSortBg, dimBg);
            TintBg(BtnClearOutBg, dimBg);
            TintBg(BtnSaveBg, dimBg);
            SetTxtCol(BtnSortText, dimTxt);
            SetTxtCol(BtnClearOutText, dimTxt);
            SetTxtCol(BtnSaveText, dimTxt);

            // Dim all category buttons
            TintBg(CatBtn0Bg, dimBg); SetTxtCol(CatBtn0Text, dimTxt);
            TintBg(CatBtn1Bg, dimBg); SetTxtCol(CatBtn1Text, dimTxt);
            TintBg(CatBtn2Bg, dimBg); SetTxtCol(CatBtn2Text, dimTxt);
            TintBg(CatBtn3Bg, dimBg); SetTxtCol(CatBtn3Text, dimTxt);
            TintBg(CatBtn4Bg, dimBg); SetTxtCol(CatBtn4Text, dimTxt);
            TintBg(CatBtn5Bg, dimBg); SetTxtCol(CatBtn5Text, dimTxt);
            TintBg(CatBtn6Bg, dimBg); SetTxtCol(CatBtn6Text, dimTxt);
            TintBg(CatBtn7Bg, dimBg); SetTxtCol(CatBtn7Text, dimTxt);

            // Dim all slot preset buttons
            TintBg(SlotPre0Bg, dimBg); SetTxtCol(SlotPre0Text, dimTxt);
            TintBg(SlotPre1Bg, dimBg); SetTxtCol(SlotPre1Text, dimTxt);
            TintBg(SlotPre2Bg, dimBg); SetTxtCol(SlotPre2Text, dimTxt);
            TintBg(SlotPre3Bg, dimBg); SetTxtCol(SlotPre3Text, dimTxt);

            // Dim add buttons
            TintBg(BtnPrefixAddBg, dimBg);
            TintBg(BtnContainsAddBg, dimBg);
            TintBg(BtnSlotAddBg, dimBg);

            // Dim catch-all
            TintBg(BtnCatchAllBg, dimBg);
            SetTxtCol(BtnCatchAllText, dimTxt);
        }
    }

    // =========================================================
    // Status label/dot (reflects pairing + save state)
    // =========================================================
    // S5: Extracted from SetStatus — maps status text to ARGB color
    protected int GetStatusColor(string st)
    {
        string stSaving = "SAVING...";
        string stSorting = "SORTING...";
        string stError = "ERROR";
        string stNoLink = "NO LINK";
        if (st == stSaving || st == stSorting)
        {
            return LFPG_SorterView.COL_AMBER;
        }
        if (st == stError)
        {
            return LFPG_SorterView.COL_RED;
        }
        if (st == stNoLink)
        {
            return LFPG_SorterView.COL_RED;
        }
        return LFPG_SorterView.COL_GREEN;
    }

    protected void SetStatus(string st)
    {
        int col = GetStatusColor(st);
        if (StatusLabel)
        {
            StatusLabel.SetText(st);
            StatusLabel.SetColor(col);
        }
        if (StatusDot)
        {
            StatusDot.LoadImageFile(0, LFPG_SorterView.PROC_WHITE);
            StatusDot.SetColor(col);
        }
    }

    void HandleSaveAck(bool success)
    {
        if (success)
        {
            string stSaved = "SAVED";
            SetStatus(stSaved);
        }
        else
        {
            string stErr = "ERROR";
            SetStatus(stErr);
        }
        m_FeedbackTimer = 2.5;
    }

    // =========================================================
    // Timer tick (called from View.Update)
    // S6: Single m_FeedbackTimer (last-write-wins)
    // =========================================================
    void TickTimers(float dt)
    {
        // Feedback revert (save or sort)
        if (m_FeedbackTimer > 0.0)
        {
            m_FeedbackTimer = m_FeedbackTimer - dt;
            if (m_FeedbackTimer <= 0.0)
            {
                m_FeedbackTimer = 0.0;
                if (m_IsPaired)
                {
                    string stOnline = "ONLINE";
                    SetStatus(stOnline);
                }
                else
                {
                    string stNoLink = "NO LINK";
                    SetStatus(stNoLink);
                }
            }
        }

        // Reset confirmation timeout
        if (m_ResetConfirmActive)
        {
            m_ResetTimer = m_ResetTimer - dt;
            if (m_ResetTimer <= 0.0)
            {
                m_ResetConfirmActive = false;
                m_ResetTimer = 0.0;
                string resetLabel = "Reset All";
                if (BtnResetAllText) { BtnResetAllText.SetText(resetLabel); }
                TintBg(BtnResetAllBg, LFPG_SorterView.COL_RED_BTN);
            }
        }
    }

    // =========================================================
    // Relay_Commands — output tabs
    // =========================================================
    void TabOut0() { SelectOutput(0); }
    void TabOut1() { SelectOutput(1); }
    void TabOut2() { SelectOutput(2); }
    void TabOut3() { SelectOutput(3); }
    void TabOut4() { SelectOutput(4); }
    void TabOut5() { SelectOutput(5); }

    protected void SelectOutput(int idx)
    {
        if (idx < 0 || idx >= LFPG_SORT_MAX_OUTPUTS)
            return;
        m_SelectedOutput = idx;
        m_ResetConfirmActive = false;
        LFPG_SorterView.PlayUIClick();
        RefreshAll();
    }

    // =========================================================
    // Relay_Commands — view tabs
    // =========================================================
    void TabRules()  { m_ShowRules = true;  LFPG_SorterView.PlayUIClick(); RefreshViewTabs(); }
    void TabPreview() { m_ShowRules = false; LFPG_SorterView.PlayUIClick(); RefreshViewTabs(); }

    // =========================================================
    // Relay_Commands — category toggles (Bug #6: guard unpaired)
    // =========================================================
    void CatBtn0() { if (!m_IsPaired) return; ToggleCategory(m_CatValue0); }
    void CatBtn1() { if (!m_IsPaired) return; ToggleCategory(m_CatValue1); }
    void CatBtn2() { if (!m_IsPaired) return; ToggleCategory(m_CatValue2); }
    void CatBtn3() { if (!m_IsPaired) return; ToggleCategory(m_CatValue3); }
    void CatBtn4() { if (!m_IsPaired) return; ToggleCategory(m_CatValue4); }
    void CatBtn5() { if (!m_IsPaired) return; ToggleCategory(m_CatValue5); }
    void CatBtn6() { if (!m_IsPaired) return; ToggleCategory(m_CatValue6); }
    void CatBtn7() { if (!m_IsPaired) return; ToggleCategory(m_CatValue7); }

    protected void ToggleCategory(string catValue)
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg) return;
        bool hasIt = outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, catValue);
        if (hasIt) { RemoveRuleByValue(outCfg, LFPG_SORT_FILTER_CATEGORY, catValue); }
        else { outCfg.AddRule(LFPG_SORT_FILTER_CATEGORY, catValue); }
        LFPG_SorterView.PlayUIClick();
        RefreshAll();
    }

    // =========================================================
    // Relay_Commands — slot toggles (Bug #6: guard unpaired)
    // =========================================================
    void SlotPre0() { if (!m_IsPaired) return; ToggleSlot(LFPG_SORT_SLOT_TINY); }
    void SlotPre1() { if (!m_IsPaired) return; ToggleSlot(LFPG_SORT_SLOT_SMALL); }
    void SlotPre2() { if (!m_IsPaired) return; ToggleSlot(LFPG_SORT_SLOT_MEDIUM); }
    void SlotPre3() { if (!m_IsPaired) return; ToggleSlot(LFPG_SORT_SLOT_LARGE); }

    protected void ToggleSlot(string slotValue)
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg) return;
        bool hasIt = outCfg.HasRule(LFPG_SORT_FILTER_SLOT, slotValue);
        if (hasIt) { RemoveRuleByValue(outCfg, LFPG_SORT_FILTER_SLOT, slotValue); }
        else { outCfg.AddRule(LFPG_SORT_FILTER_SLOT, slotValue); }
        LFPG_SorterView.PlayUIClick();
        RefreshAll();
    }

    // =========================================================
    // Relay_Commands — add buttons (Bug #6: guard unpaired)
    // =========================================================
    void BtnPrefixAdd()
    {
        if (!m_IsPaired) return;
        if (EditPrefix == "") return;
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg) return;
        outCfg.AddRule(LFPG_SORT_FILTER_PREFIX, EditPrefix);
        EditPrefix = "";
        string propEP = "EditPrefix";
        NotifyPropertyChanged(propEP, false);
        LFPG_SorterView.PlayUIAction();
        RefreshAll();
    }

    void BtnContainsAdd()
    {
        if (!m_IsPaired) return;
        if (EditContains == "") return;
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg) return;
        outCfg.AddRule(LFPG_SORT_FILTER_CONTAINS, EditContains);
        EditContains = "";
        string propEC = "EditContains";
        NotifyPropertyChanged(propEC, false);
        LFPG_SorterView.PlayUIAction();
        RefreshAll();
    }

    void BtnSlotAdd()
    {
        if (!m_IsPaired) return;
        if (EditSlotMin == "" || EditSlotMax == "") return;
        int minVal = EditSlotMin.ToInt();
        int maxVal = EditSlotMax.ToInt();
        if (minVal < 1) { minVal = 1; }
        if (maxVal < minVal) { maxVal = minVal; }
        string dash = "-";
        string slotValue = minVal.ToString();
        slotValue = slotValue + dash;
        slotValue = slotValue + maxVal.ToString();
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg) return;
        outCfg.AddRule(LFPG_SORT_FILTER_SLOT, slotValue);
        EditSlotMin = "";
        EditSlotMax = "";
        string propMin = "EditSlotMin";
        string propMax = "EditSlotMax";
        NotifyPropertyChanged(propMin, false);
        NotifyPropertyChanged(propMax, false);
        LFPG_SorterView.PlayUIAction();
        RefreshAll();
    }

    // =========================================================
    // Relay_Commands — catch-all, clear, reset, save, close, sort
    // =========================================================
    void BtnCatchAll()
    {
        if (!m_IsPaired) return;
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg) return;
        if (outCfg.m_IsCatchAll) { outCfg.m_IsCatchAll = false; }
        else { outCfg.m_IsCatchAll = true; }
        LFPG_SorterView.PlayUIClick();
        RefreshAll();
    }

    void BtnClearOut()
    {
        if (!m_IsPaired) return;
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg) return;
        outCfg.ClearRules();
        LFPG_SorterView.PlayUIClick();
        RefreshAll();
    }

    void BtnResetAll()
    {
        if (!m_IsPaired) return;
        if (!m_ResetConfirmActive)
        {
            m_ResetConfirmActive = true;
            m_ResetTimer = 3.0;
            string confirmLabel = "Confirm?";
            if (BtnResetAllText) { BtnResetAllText.SetText(confirmLabel); }
            TintBg(BtnResetAllBg, LFPG_SorterView.COL_AMBER);
            LFPG_SorterView.PlayUIClick();
            return;
        }
        m_ResetConfirmActive = false;
        m_Config.ResetAll();
        string resetLabel = "Reset All";
        if (BtnResetAllText) { BtnResetAllText.SetText(resetLabel); }
        TintBg(BtnResetAllBg, LFPG_SorterView.COL_RED_BTN);
        LFPG_SorterView.PlayUIAction();
        RefreshAll();
    }

    void BtnSave()
    {
        // S8 fix: guard unpaired — all other action buttons check this
        if (!m_IsPaired)
            return;

        string json = m_Config.ToJSON();
        string saveMsg = "[SorterCtrl] SAVE: ";
        saveMsg = saveMsg + json;
        LFPG_Util.Info(saveMsg);
        string stSaving = "SAVING...";
        SetStatus(stSaving);
        LFPG_SorterView.PlayUIAction();
        #ifndef SERVER
        // R4: GetGame guard
        if (!GetGame())
            return;
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

    void BtnSort()
    {
        if (!m_IsPaired) return;
        string sortMsg = "[SorterCtrl] REQUEST_SORT";
        LFPG_Util.Info(sortMsg);
        // Sort feedback — immediate client-side status
        string stSorting = "SORTING...";
        SetStatus(stSorting);
        // S6: Single feedback timer (last-write-wins over save)
        m_FeedbackTimer = 3.0;
        LFPG_SorterView.PlayUIAction();
        #ifndef SERVER
        // R4: GetGame guard
        if (!GetGame())
            return;
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

    void BtnClose() { LFPG_SorterView.PlayUIClick(); LFPG_SorterView.Close(); }

    // Bug #3: X close button in header
    void BtnCloseX() { LFPG_SorterView.PlayUIClick(); LFPG_SorterView.Close(); }

    // =========================================================
    // Tag removal (called from tag chip via direct ref)
    // =========================================================
    void OnRemoveTag(int outputIdx, int ruleIdx)
    {
        if (!m_IsPaired) return;
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(outputIdx);
        if (!outCfg) return;
        if (ruleIdx < 0) { outCfg.m_IsCatchAll = false; }
        else { outCfg.RemoveRuleAt(ruleIdx); }
        RefreshAll();
    }

    // =========================================================
    protected void RemoveRuleByValue(LFPG_SortOutputConfig outCfg, int ruleType, string ruleValue)
    {
        int ri;
        for (ri = 0; ri < outCfg.m_Rules.Count(); ri = ri + 1)
        {
            if (outCfg.m_Rules[ri].Equals(ruleType, ruleValue))
            {
                outCfg.RemoveRuleAt(ri);
                return;
            }
        }
    }

    // =========================================================
    // Full refresh
    // =========================================================
    protected void RefreshAll()
    {
        RefreshOutputTabs();
        RefreshViewTabs();
        RefreshCategoryButtons();
        RefreshSlotButtons();
        RefreshCatchAllButton();
        RefreshTagsList();
        RefreshRuleCount();
        RefreshMatchCount();
        RefreshPreviewCount();
        RefreshDestIndicator();
        // Apply disabled visual after all refreshes (v2.2)
        SetControlsEnabled(m_IsPaired);
    }

    protected void RefreshOutputTabs()
    {
        int i;
        LFPG_SortOutputConfig tabCfg = null;
        int tabRules = 0;
        bool tabHasContent = false;
        bool isSel = false;
        int bgCol = 0;
        int txtCol = 0;
        int num = 0;
        string numStr = "";
        string prefix = "0";
        string label = "";
        string indicator = " *";
        string destName = "";
        int destLen = 0;
        int maxDestChars = 4;
        TextWidget tt = null;
        for (i = 0; i < 6; i = i + 1)
        {
            isSel = (i == m_SelectedOutput);
            bgCol = LFPG_SorterView.COL_BTN;
            txtCol = LFPG_SorterView.COL_TEXT_MID;
            if (isSel) { bgCol = LFPG_SorterView.COL_BG_ELEVATED; txtCol = LFPG_SorterView.COL_GREEN; }
            num = i + 1;
            numStr = num.ToString();
            label = prefix;
            label = label + numStr;
            if (num >= 10) { label = numStr; }

            // D5: Append truncated dest name when available
            destName = GetDestName(i);
            if (destName != "")
            {
                destLen = destName.Length();
                if (destLen > maxDestChars)
                {
                    destLen = maxDestChars;
                }
                label = label + ":";
                label = label + destName.Substring(0, destLen);
            }

            // V2: Indicate tabs with rules/catch-all (non-selected only)
            tabCfg = m_Config.GetOutput(i);
            tabHasContent = false;
            if (tabCfg)
            {
                tabRules = tabCfg.GetRuleCount();
                if (tabRules > 0) { tabHasContent = true; }
                if (tabCfg.m_IsCatchAll) { tabHasContent = true; }
            }
            if (tabHasContent && !isSel)
            {
                label = label + indicator;
                txtCol = LFPG_SorterView.COL_TEXT;
            }

            TintBg(GetTabBg(i), bgCol);
            tt = GetTabText(i);
            if (tt) { tt.SetColor(txtCol); tt.SetText(label); }
        }
    }

    protected ImageWidget GetTabBg(int idx)
    {
        if (idx == 0) return TabOut0Bg; if (idx == 1) return TabOut1Bg;
        if (idx == 2) return TabOut2Bg; if (idx == 3) return TabOut3Bg;
        if (idx == 4) return TabOut4Bg; if (idx == 5) return TabOut5Bg;
        return null;
    }
    protected TextWidget GetTabText(int idx)
    {
        if (idx == 0) return TabOut0Text; if (idx == 1) return TabOut1Text;
        if (idx == 2) return TabOut2Text; if (idx == 3) return TabOut3Text;
        if (idx == 4) return TabOut4Text; if (idx == 5) return TabOut5Text;
        return null;
    }

    protected void RefreshViewTabs()
    {
        int rBg = LFPG_SorterView.COL_BTN; int rTxt = LFPG_SorterView.COL_TEXT_DIM;
        int pBg = LFPG_SorterView.COL_BTN; int pTxt = LFPG_SorterView.COL_TEXT_DIM;
        if (m_ShowRules) { rBg = LFPG_SorterView.COL_BG_ELEVATED; rTxt = LFPG_SorterView.COL_GREEN; }
        else { pBg = LFPG_SorterView.COL_BG_ELEVATED; pTxt = LFPG_SorterView.COL_GREEN; }
        TintBg(TabRulesBg, rBg); TintBg(TabPreviewBg, pBg);
        if (TabRulesText) { TabRulesText.SetColor(rTxt); }
        if (TabPreviewText) { TabPreviewText.SetColor(pTxt); }
        if (RulesPanel) { RulesPanel.Show(m_ShowRules); }
        if (PreviewPanel) { PreviewPanel.Show(!m_ShowRules); }
    }

    protected void RefreshCategoryButtons()
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg) return;
        RefreshToggleBtn(CatBtn0Bg, CatBtn0Text, outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, m_CatValue0), LFPG_SorterView.COL_GREEN_BTN, LFPG_SorterView.COL_GREEN);
        RefreshToggleBtn(CatBtn1Bg, CatBtn1Text, outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, m_CatValue1), LFPG_SorterView.COL_GREEN_BTN, LFPG_SorterView.COL_GREEN);
        RefreshToggleBtn(CatBtn2Bg, CatBtn2Text, outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, m_CatValue2), LFPG_SorterView.COL_GREEN_BTN, LFPG_SorterView.COL_GREEN);
        RefreshToggleBtn(CatBtn3Bg, CatBtn3Text, outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, m_CatValue3), LFPG_SorterView.COL_GREEN_BTN, LFPG_SorterView.COL_GREEN);
        RefreshToggleBtn(CatBtn4Bg, CatBtn4Text, outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, m_CatValue4), LFPG_SorterView.COL_GREEN_BTN, LFPG_SorterView.COL_GREEN);
        RefreshToggleBtn(CatBtn5Bg, CatBtn5Text, outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, m_CatValue5), LFPG_SorterView.COL_GREEN_BTN, LFPG_SorterView.COL_GREEN);
        RefreshToggleBtn(CatBtn6Bg, CatBtn6Text, outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, m_CatValue6), LFPG_SorterView.COL_GREEN_BTN, LFPG_SorterView.COL_GREEN);
        RefreshToggleBtn(CatBtn7Bg, CatBtn7Text, outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, m_CatValue7), LFPG_SorterView.COL_GREEN_BTN, LFPG_SorterView.COL_GREEN);
    }

    protected void RefreshSlotButtons()
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg) return;
        RefreshToggleBtn(SlotPre0Bg, SlotPre0Text, outCfg.HasRule(LFPG_SORT_FILTER_SLOT, LFPG_SORT_SLOT_TINY), LFPG_SorterView.COL_BLUE_BTN, LFPG_SorterView.COL_BLUE);
        RefreshToggleBtn(SlotPre1Bg, SlotPre1Text, outCfg.HasRule(LFPG_SORT_FILTER_SLOT, LFPG_SORT_SLOT_SMALL), LFPG_SorterView.COL_BLUE_BTN, LFPG_SorterView.COL_BLUE);
        RefreshToggleBtn(SlotPre2Bg, SlotPre2Text, outCfg.HasRule(LFPG_SORT_FILTER_SLOT, LFPG_SORT_SLOT_MEDIUM), LFPG_SorterView.COL_BLUE_BTN, LFPG_SorterView.COL_BLUE);
        RefreshToggleBtn(SlotPre3Bg, SlotPre3Text, outCfg.HasRule(LFPG_SORT_FILTER_SLOT, LFPG_SORT_SLOT_LARGE), LFPG_SorterView.COL_BLUE_BTN, LFPG_SorterView.COL_BLUE);
    }

    // Unified toggle button refresh: active/inactive with custom active colors
    protected void RefreshToggleBtn(ImageWidget bg, TextWidget txt, bool active, int activeBg, int activeTxt)
    {
        int bgCol = LFPG_SorterView.COL_BTN;
        int txtCol = LFPG_SorterView.COL_TEXT_MID;
        if (active) { bgCol = activeBg; txtCol = activeTxt; }
        TintBg(bg, bgCol);
        if (txt) { txt.SetColor(txtCol); }
    }

    protected void RefreshCatchAllButton()
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg) return;
        bool active = outCfg.m_IsCatchAll;
        int bgCol = LFPG_SorterView.COL_BTN;
        int txtCol = LFPG_SorterView.COL_TEXT_DIM;
        string label = "CATCH-ALL";
        if (active)
        {
            bgCol = LFPG_SorterView.COL_AMBER;
            txtCol = LFPG_SorterView.COL_BG_DEEP;
            label = "* CATCH-ALL";
        }
        TintBg(BtnCatchAllBg, bgCol);
        if (BtnCatchAllText) { BtnCatchAllText.SetColor(txtCol); BtnCatchAllText.SetText(label); }
    }

    // =========================================================
    // Tags list rebuild (passes 'this' for direct parent ref)
    // =========================================================
    protected void RefreshTagsList()
    {
        TagsList.Clear();
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg) return;

        int ruleCount = outCfg.GetRuleCount();
        int ri;
        for (ri = 0; ri < ruleCount; ri = ri + 1)
        {
            LFPG_SortFilterRule rule = outCfg.m_Rules[ri];
            if (!rule) continue;
            string label = rule.GetDisplayLabel();
            int color = GetRuleColor(rule.m_Type);
            LFPG_SorterTagView tag = new LFPG_SorterTagView();
            tag.SetData(label, color, ri, m_SelectedOutput, this);
            TagsList.Insert(tag);
        }

        if (outCfg.m_IsCatchAll)
        {
            LFPG_SorterTagView caTag = new LFPG_SorterTagView();
            string caLabel = "* CATCH-ALL";
            caTag.SetData(caLabel, LFPG_SorterView.COL_AMBER, -1, m_SelectedOutput, this);
            TagsList.Insert(caTag);
        }

        bool isEmpty = (ruleCount == 0 && !outCfg.m_IsCatchAll);
        if (TagsEmpty) { TagsEmpty.Show(isEmpty); }
    }

    protected int GetRuleColor(int ruleType)
    {
        if (ruleType == LFPG_SORT_FILTER_CATEGORY) return LFPG_SorterView.COL_GREEN;
        if (ruleType == LFPG_SORT_FILTER_PREFIX) return LFPG_SorterView.COL_BLUE;
        if (ruleType == LFPG_SORT_FILTER_CONTAINS) return LFPG_SorterView.COL_AMBER;
        if (ruleType == LFPG_SORT_FILTER_SLOT) return 0xFFA78BFA;
        return LFPG_SorterView.COL_TEXT;
    }

    protected void RefreshRuleCount()
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        int count = 0;
        if (outCfg) { count = outCfg.GetRuleCount(); }
        string suffix = "/8";
        RuleCount = count.ToString();
        RuleCount = RuleCount + suffix;
        string propRC = "RuleCount";
        NotifyPropertyChanged(propRC, false);
    }

    protected void RefreshDestIndicator()
    {
        string dest = GetDestName(m_SelectedOutput);
        bool hasDest = (dest != "");
        if (DestIndicator) { DestIndicator.Show(hasDest); }
        string propDN = "DestName";
        string propHT = "HeaderTitle";
        if (hasDest)
        {
            DestName = dest;
            NotifyPropertyChanged(propDN, false);
            string sorterPrefix = "SORTER  ";
            HeaderTitle = sorterPrefix;
            HeaderTitle = HeaderTitle + dest;
        }
        else
        {
            string sorterTitle = "SORTER";
            HeaderTitle = sorterTitle;
        }
        NotifyPropertyChanged(propHT, false);
    }

    protected string GetDestName(int idx)
    {
        if (idx == 0) return m_Dest0; if (idx == 1) return m_Dest1;
        if (idx == 2) return m_Dest2; if (idx == 3) return m_Dest3;
        if (idx == 4) return m_Dest4; if (idx == 5) return m_Dest5;
        return "";
    }

    // =========================================================
    // BUG #1 fix: MatchCount was never updated — show rule total
    // for current output so the RulesPanel footer is not blank.
    // =========================================================
    protected void RefreshMatchCount()
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        int total = 0;
        if (outCfg)
        {
            total = outCfg.GetRuleCount();
            if (outCfg.m_IsCatchAll)
            {
                total = total + 1;
            }
        }
        string suffix = " rules";
        MatchCount = total.ToString();
        MatchCount = MatchCount + suffix;
        string propMC = "MatchCount";
        NotifyPropertyChanged(propMC, false);
    }

    // =========================================================
    // BUG #2 fix: PreviewCount was never updated — show count
    // of items in the PreviewItems collection.
    // D6: Also manages PreviewEmpty state and placeholder text.
    // =========================================================
    protected void RefreshPreviewCount()
    {
        int count = 0;
        if (PreviewItems)
        {
            count = PreviewItems.Count();
        }
        string suffix = " items";
        PreviewCount = count.ToString();
        PreviewCount = PreviewCount + suffix;
        string propPC = "PreviewCount";
        NotifyPropertyChanged(propPC, false);

        // D6: Show placeholder when no preview data
        if (PreviewEmpty)
        {
            if (count == 0)
            {
                string emptyMsg = "Sort preview not yet available";
                PreviewEmpty.SetText(emptyMsg);
                PreviewEmpty.Show(true);
            }
            else
            {
                PreviewEmpty.Show(false);
            }
        }
    }

    protected void TintBg(ImageWidget bg, int color)
    {
        if (!bg) return;
        bg.LoadImageFile(0, LFPG_SorterView.PROC_WHITE);
        bg.SetColor(color);
        // Cache in View for hover system (v2.2)
        LFPG_SorterView.CacheColor(bg, color);
    }

    protected void SetTxtCol(TextWidget txt, int color)
    {
        if (!txt) return;
        txt.SetColor(color);
    }
};
