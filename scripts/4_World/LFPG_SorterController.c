// =========================================================
// LF_PowerGrid — Sorter Controller (Dabs MVC, v2.6 Pool Sprint)
//
// v2.6 changes (Tag Pool — Fase 5):
//   - m_TagPool: reuse TagViews via pool (eliminates new/delete
//     per RefreshTagsList call). Max 9 = 8 rules + 1 catch-all.
//   - ClearCollections: pool cleared on DoClose to break refs.
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

    // ── Tag pool (v2.6 — reuse TagViews across RefreshTagsList calls) ──
    ref array<ref LFPG_SorterTagView> m_TagPool;

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

    // ── Widget refs (resolved by child-walk in EnsureBindings) ──
    TextWidget StatusLabel;
    ImageWidget StatusDot;

    // Output tabs (m_ prefix prevents Dabs auto-bind from overwriting EnsureBindings refs)
    ImageWidget m_TabOut0Bg; ImageWidget m_TabOut1Bg; ImageWidget m_TabOut2Bg;
    ImageWidget m_TabOut3Bg; ImageWidget m_TabOut4Bg; ImageWidget m_TabOut5Bg;
    TextWidget m_TabOut0Text; TextWidget m_TabOut1Text; TextWidget m_TabOut2Text;
    TextWidget m_TabOut3Text; TextWidget m_TabOut4Text; TextWidget m_TabOut5Text;
    // View tabs
    ImageWidget TabRulesBg; ImageWidget TabPreviewBg;
    TextWidget TabRulesText; TextWidget TabPreviewText;
    ImageWidget TabIndicator;
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
    // Header sort button
    ImageWidget BtnSortHeaderBg; TextWidget BtnSortHeaderText;
    // Label refs
    TextWidget LblCategory; TextWidget LblPrefix; TextWidget LblContains;
    TextWidget LblSlot; TextWidget LblSlotDash;
    TextWidget LblActiveRules; TextWidget DestLabel;
    TextWidget LblPreview;
    // Panels
    Widget RulesPanel; Widget PreviewPanel;
    Widget DestIndicator;
    TextWidget TagsEmpty; TextWidget PreviewEmpty;

    // v3: Tab dots (P-III)
    ImageWidget TabDot0; ImageWidget TabDot1; ImageWidget TabDot2;
    ImageWidget TabDot3; ImageWidget TabDot4; ImageWidget TabDot5;
    // v3: View tab indicator (C)
    ImageWidget ViewTabIndicator;
    // v3: Empty state extras (P-IV)
    TextWidget TagsEmptyIcon;
    TextWidget TagsEmptyHint;
    TextWidget PreviewEmptyIcon;
    TextWidget PreviewEmptyHint;

    // E3: PROC constant removed — use LFPG_SorterView.PROC_WHITE instead

    // =========================================================
    void LFPG_SorterController()
    {
        TagsList = new ObservableCollection<ref LFPG_SorterTagView>(this);
        PreviewItems = new ObservableCollection<ref LFPG_SorterPreviewRow>(this);
        m_TagPool = new array<ref LFPG_SorterTagView>;
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
    // v2.8: Manual binding — ALL button children resolved via
    // child-walk. Dabs MVC auto-bind AND FindAnyWidget both
    // return INCORRECT (non-null) refs for ImageWidget/TextWidget
    // inside ButtonWidget. This causes null-guard fallbacks to
    // never trigger, leaving refs pointing to wrong widgets.
    // Child-walk is 100% reliable: find the ButtonWidget by name
    // (direct child of container), walk its children to find the
    // first ImageWidget (Bg) and first TextWidget (Text).
    // =========================================================
    void EnsureBindings(Widget layoutRoot)
    {
        if (!layoutRoot)
            return;

        string bn = "";

        // ── Output tabs (child-walk, overwrite any auto-bind) ──
        bn = "TabOut0";
        m_TabOut0Bg = FindBtnChildBg(layoutRoot, bn);
        m_TabOut0Text = FindBtnChildText(layoutRoot, bn);
        bn = "TabOut1";
        m_TabOut1Bg = FindBtnChildBg(layoutRoot, bn);
        m_TabOut1Text = FindBtnChildText(layoutRoot, bn);
        bn = "TabOut2";
        m_TabOut2Bg = FindBtnChildBg(layoutRoot, bn);
        m_TabOut2Text = FindBtnChildText(layoutRoot, bn);
        bn = "TabOut3";
        m_TabOut3Bg = FindBtnChildBg(layoutRoot, bn);
        m_TabOut3Text = FindBtnChildText(layoutRoot, bn);
        bn = "TabOut4";
        m_TabOut4Bg = FindBtnChildBg(layoutRoot, bn);
        m_TabOut4Text = FindBtnChildText(layoutRoot, bn);
        bn = "TabOut5";
        m_TabOut5Bg = FindBtnChildBg(layoutRoot, bn);
        m_TabOut5Text = FindBtnChildText(layoutRoot, bn);

        // ── View tabs ──
        bn = "TabRules";
        TabRulesBg = FindBtnChildBg(layoutRoot, bn);
        TabRulesText = FindBtnChildText(layoutRoot, bn);
        bn = "TabPreview";
        TabPreviewBg = FindBtnChildBg(layoutRoot, bn);
        TabPreviewText = FindBtnChildText(layoutRoot, bn);

        // ── Category buttons ──
        bn = "CatBtn0";
        CatBtn0Bg = FindBtnChildBg(layoutRoot, bn);
        CatBtn0Text = FindBtnChildText(layoutRoot, bn);
        bn = "CatBtn1";
        CatBtn1Bg = FindBtnChildBg(layoutRoot, bn);
        CatBtn1Text = FindBtnChildText(layoutRoot, bn);
        bn = "CatBtn2";
        CatBtn2Bg = FindBtnChildBg(layoutRoot, bn);
        CatBtn2Text = FindBtnChildText(layoutRoot, bn);
        bn = "CatBtn3";
        CatBtn3Bg = FindBtnChildBg(layoutRoot, bn);
        CatBtn3Text = FindBtnChildText(layoutRoot, bn);
        bn = "CatBtn4";
        CatBtn4Bg = FindBtnChildBg(layoutRoot, bn);
        CatBtn4Text = FindBtnChildText(layoutRoot, bn);
        bn = "CatBtn5";
        CatBtn5Bg = FindBtnChildBg(layoutRoot, bn);
        CatBtn5Text = FindBtnChildText(layoutRoot, bn);
        bn = "CatBtn6";
        CatBtn6Bg = FindBtnChildBg(layoutRoot, bn);
        CatBtn6Text = FindBtnChildText(layoutRoot, bn);
        bn = "CatBtn7";
        CatBtn7Bg = FindBtnChildBg(layoutRoot, bn);
        CatBtn7Text = FindBtnChildText(layoutRoot, bn);

        // ── Slot presets ──
        bn = "SlotPre0";
        SlotPre0Bg = FindBtnChildBg(layoutRoot, bn);
        SlotPre0Text = FindBtnChildText(layoutRoot, bn);
        bn = "SlotPre1";
        SlotPre1Bg = FindBtnChildBg(layoutRoot, bn);
        SlotPre1Text = FindBtnChildText(layoutRoot, bn);
        bn = "SlotPre2";
        SlotPre2Bg = FindBtnChildBg(layoutRoot, bn);
        SlotPre2Text = FindBtnChildText(layoutRoot, bn);
        bn = "SlotPre3";
        SlotPre3Bg = FindBtnChildBg(layoutRoot, bn);
        SlotPre3Text = FindBtnChildText(layoutRoot, bn);

        // ── Catch-all ──
        bn = "BtnCatchAll";
        BtnCatchAllBg = FindBtnChildBg(layoutRoot, bn);
        BtnCatchAllText = FindBtnChildText(layoutRoot, bn);

        // ── Add buttons ──
        bn = "BtnPrefixAdd";
        BtnPrefixAddBg = FindBtnChildBg(layoutRoot, bn);
        bn = "BtnContainsAdd";
        BtnContainsAddBg = FindBtnChildBg(layoutRoot, bn);
        bn = "BtnSlotAdd";
        BtnSlotAddBg = FindBtnChildBg(layoutRoot, bn);

        // ── Footer buttons ──
        bn = "BtnSort";
        BtnSortBg = FindBtnChildBg(layoutRoot, bn);
        BtnSortText = FindBtnChildText(layoutRoot, bn);
        bn = "BtnSave";
        BtnSaveBg = FindBtnChildBg(layoutRoot, bn);
        BtnSaveText = FindBtnChildText(layoutRoot, bn);
        bn = "BtnResetAll";
        BtnResetAllBg = FindBtnChildBg(layoutRoot, bn);
        BtnResetAllText = FindBtnChildText(layoutRoot, bn);
        bn = "BtnClearOut";
        BtnClearOutBg = FindBtnChildBg(layoutRoot, bn);
        BtnClearOutText = FindBtnChildText(layoutRoot, bn);
        bn = "BtnClose";
        BtnCloseBg = FindBtnChildBg(layoutRoot, bn);
        BtnCloseText = FindBtnChildText(layoutRoot, bn);

        // ── Header sort button ──
        bn = "BtnSortHeader";
        BtnSortHeaderBg = FindBtnChildBg(layoutRoot, bn);
        BtnSortHeaderText = FindBtnChildText(layoutRoot, bn);

        // ══════════════════════════════════════════════════════
        // Standalone widgets — FindAnyWidget is reliable for
        // these (NOT inside ButtonWidget containers).
        // ══════════════════════════════════════════════════════
        string wn = "";

        wn = "TabIndicator";
        if (!TabIndicator) { TabIndicator = ImageWidget.Cast(layoutRoot.FindAnyWidget(wn)); }

        wn = "StatusLabel";
        if (!StatusLabel) { StatusLabel = TextWidget.Cast(layoutRoot.FindAnyWidget(wn)); }
        wn = "StatusDot";
        if (!StatusDot) { StatusDot = ImageWidget.Cast(layoutRoot.FindAnyWidget(wn)); }

        wn = "LblCategory";
        if (!LblCategory) { LblCategory = TextWidget.Cast(layoutRoot.FindAnyWidget(wn)); }
        wn = "LblPrefix";
        if (!LblPrefix) { LblPrefix = TextWidget.Cast(layoutRoot.FindAnyWidget(wn)); }
        wn = "LblContains";
        if (!LblContains) { LblContains = TextWidget.Cast(layoutRoot.FindAnyWidget(wn)); }
        wn = "LblSlot";
        if (!LblSlot) { LblSlot = TextWidget.Cast(layoutRoot.FindAnyWidget(wn)); }
        wn = "LblSlotDash";
        if (!LblSlotDash) { LblSlotDash = TextWidget.Cast(layoutRoot.FindAnyWidget(wn)); }
        wn = "LblActiveRules";
        if (!LblActiveRules) { LblActiveRules = TextWidget.Cast(layoutRoot.FindAnyWidget(wn)); }
        wn = "DestLabel";
        if (!DestLabel) { DestLabel = TextWidget.Cast(layoutRoot.FindAnyWidget(wn)); }
        wn = "LblPreview";
        if (!LblPreview) { LblPreview = TextWidget.Cast(layoutRoot.FindAnyWidget(wn)); }
        wn = "TagsEmpty";
        if (!TagsEmpty) { TagsEmpty = TextWidget.Cast(layoutRoot.FindAnyWidget(wn)); }
        wn = "PreviewEmpty";
        if (!PreviewEmpty) { PreviewEmpty = TextWidget.Cast(layoutRoot.FindAnyWidget(wn)); }

        wn = "RulesPanel";
        if (!RulesPanel) { RulesPanel = layoutRoot.FindAnyWidget(wn); }
        wn = "PreviewPanel";
        if (!PreviewPanel) { PreviewPanel = layoutRoot.FindAnyWidget(wn); }
        wn = "DestIndicator";
        if (!DestIndicator) { DestIndicator = layoutRoot.FindAnyWidget(wn); }

        // v3: Tab dots
        wn = "TabDot0";
        if (!TabDot0) { TabDot0 = ImageWidget.Cast(layoutRoot.FindAnyWidget(wn)); }
        wn = "TabDot1";
        if (!TabDot1) { TabDot1 = ImageWidget.Cast(layoutRoot.FindAnyWidget(wn)); }
        wn = "TabDot2";
        if (!TabDot2) { TabDot2 = ImageWidget.Cast(layoutRoot.FindAnyWidget(wn)); }
        wn = "TabDot3";
        if (!TabDot3) { TabDot3 = ImageWidget.Cast(layoutRoot.FindAnyWidget(wn)); }
        wn = "TabDot4";
        if (!TabDot4) { TabDot4 = ImageWidget.Cast(layoutRoot.FindAnyWidget(wn)); }
        wn = "TabDot5";
        if (!TabDot5) { TabDot5 = ImageWidget.Cast(layoutRoot.FindAnyWidget(wn)); }
        // v3: View tab indicator
        wn = "ViewTabIndicator";
        if (!ViewTabIndicator) { ViewTabIndicator = ImageWidget.Cast(layoutRoot.FindAnyWidget(wn)); }
        // v3: Empty state extras
        wn = "TagsEmptyIcon";
        if (!TagsEmptyIcon) { TagsEmptyIcon = TextWidget.Cast(layoutRoot.FindAnyWidget(wn)); }
        wn = "TagsEmptyHint";
        if (!TagsEmptyHint) { TagsEmptyHint = TextWidget.Cast(layoutRoot.FindAnyWidget(wn)); }
        wn = "PreviewEmptyIcon";
        if (!PreviewEmptyIcon) { PreviewEmptyIcon = TextWidget.Cast(layoutRoot.FindAnyWidget(wn)); }
        wn = "PreviewEmptyHint";
        if (!PreviewEmptyHint) { PreviewEmptyHint = TextWidget.Cast(layoutRoot.FindAnyWidget(wn)); }
    }

    // v2.7: Fallback helpers — find a ButtonWidget by name,
    // then walk its immediate children to find the ImageWidget
    // (Bg) or TextWidget (Text). Handles cases where
    // FindAnyWidget fails for widgets nested inside ButtonWidget.
    // v3.1: Added diagnostic logging + fallback via direct name lookup.
    protected ImageWidget FindBtnChildBg(Widget layoutRoot, string btnName)
    {
        if (!layoutRoot)
            return null;

        Widget btnW = layoutRoot.FindAnyWidget(btnName);
        if (!btnW)
        {
            string warnBtn = "[EnsureBindings] ButtonWidget NOT FOUND: ";
            warnBtn = warnBtn + btnName;
            LFPG_Util.Warn(warnBtn);
            return null;
        }

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

        // v3.1 Fallback: try direct FindAnyWidget for "<btnName>Bg"
        string bgName = btnName;
        bgName = bgName + "Bg";
        ImageWidget fallback = ImageWidget.Cast(layoutRoot.FindAnyWidget(bgName));
        if (fallback)
        {
            string fbMsg = "[EnsureBindings] child-walk null, fallback OK: ";
            fbMsg = fbMsg + bgName;
            LFPG_Util.Info(fbMsg);
            return fallback;
        }

        string warnNull = "[EnsureBindings] Bg NULL for: ";
        warnNull = warnNull + btnName;
        LFPG_Util.Warn(warnNull);
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

        // v3.1 Fallback: try direct FindAnyWidget for "<btnName>Text"
        string txtName = btnName;
        txtName = txtName + "Text";
        TextWidget fallbackTxt = TextWidget.Cast(layoutRoot.FindAnyWidget(txtName));
        if (fallbackTxt)
        {
            string fbMsg2 = "[EnsureBindings] child-walk null, fallback OK: ";
            fbMsg2 = fbMsg2 + txtName;
            LFPG_Util.Info(fbMsg2);
            return fallbackTxt;
        }

        string warnNullTxt = "[EnsureBindings] Text NULL for: ";
        warnNullTxt = warnNullTxt + btnName;
        LFPG_Util.Warn(warnNullTxt);
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

        // DIAG: Log pairing state and key binding results
        string diagInit = "[SorterCtrl] InitFromRPC paired=";
        diagInit = diagInit + m_IsPaired.ToString();
        diagInit = diagInit + " container=";
        diagInit = diagInit + containerName;
        LFPG_Util.Info(diagInit);
        string diagBindings = "[SorterCtrl] Bindings CatBtn0Bg=";
        if (CatBtn0Bg) { diagBindings = diagBindings + "OK"; }
        else { diagBindings = diagBindings + "NULL"; }
        diagBindings = diagBindings + " CatBtn0Text=";
        if (CatBtn0Text) { diagBindings = diagBindings + "OK"; }
        else { diagBindings = diagBindings + "NULL"; }
        diagBindings = diagBindings + " TabOut1Text=";
        if (m_TabOut1Text) { diagBindings = diagBindings + "OK"; }
        else { diagBindings = diagBindings + "NULL"; }
        diagBindings = diagBindings + " TabOut3Text=";
        if (m_TabOut3Text) { diagBindings = diagBindings + "OK"; }
        else { diagBindings = diagBindings + "NULL"; }
        diagBindings = diagBindings + " TabOut5Text=";
        if (m_TabOut5Text) { diagBindings = diagBindings + "OK"; }
        else { diagBindings = diagBindings + "NULL"; }
        LFPG_Util.Info(diagBindings);

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
        // v3: BtnClearOut destructive tint (was neutral grey)
        int RS = LFPG_SorterView.COL_RED_BTN_SOFT;
        TintBg(BtnClearOutBg, RS);
        TintBg(BtnCloseBg, V);

        // Footer text colors
        SetTxtCol(BtnSortText, WHT);
        SetTxtCol(BtnSaveText, GRN);
        SetTxtCol(BtnResetAllText, WHT);
        SetTxtCol(BtnClearOutText, LFPG_SorterView.COL_RED);
        SetTxtCol(BtnCloseText, DIM);

        // Header sort button (blue, like footer BtnSort)
        TintBg(BtnSortHeaderBg, B);
        SetTxtCol(BtnSortHeaderText, WHT);

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

        // v3: Empty state extras
        SetTxtCol(TagsEmptyIcon, LFPG_SorterView.COL_SEPARATOR);
        SetTxtCol(TagsEmptyHint, DIM);
        SetTxtCol(PreviewEmptyIcon, LFPG_SorterView.COL_SEPARATOR);
        SetTxtCol(PreviewEmptyHint, DIM);
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
        int dimBg = 0xFF1C293E;
        int dimTxt = LFPG_SorterView.COL_TEXT_DIM;

        // Footer action buttons
        if (enabled)
        {
            TintBg(BtnSortBg, LFPG_SorterView.COL_BLUE_BTN);
            // v3: BtnClearOut destructive tint when enabled
            TintBg(BtnClearOutBg, LFPG_SorterView.COL_RED_BTN_SOFT);
            SetTxtCol(BtnClearOutText, LFPG_SorterView.COL_RED);
            TintBg(BtnSaveBg, LFPG_SorterView.COL_GREEN_BTN);
            SetTxtCol(BtnSortText, LFPG_SorterView.COL_TEXT);
            SetTxtCol(BtnSaveText, LFPG_SorterView.COL_GREEN);
            // Header sort
            TintBg(BtnSortHeaderBg, LFPG_SorterView.COL_BLUE_BTN);
            SetTxtCol(BtnSortHeaderText, LFPG_SorterView.COL_TEXT);
        }
        else
        {
            TintBg(BtnSortBg, dimBg);
            TintBg(BtnClearOutBg, dimBg);
            TintBg(BtnSaveBg, dimBg);
            SetTxtCol(BtnSortText, dimTxt);
            SetTxtCol(BtnClearOutText, dimTxt);
            SetTxtCol(BtnSaveText, dimTxt);
            // Header sort
            TintBg(BtnSortHeaderBg, dimBg);
            SetTxtCol(BtnSortHeaderText, dimTxt);

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
        // N3: Sync flag to View so OnMouseEnter skips hover on dimmed buttons
        LFPG_SorterView.SetControlsFlag(enabled);
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
        // N2: No output tab switching when unpaired
        if (!m_IsPaired)
            return;
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
    void TabPreview() { m_ShowRules = false; LFPG_SorterView.PlayUIClick(); RefreshViewTabs(); RequestPreview(); }

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
        string label = "[SorterCtrl] REQUEST_SORT";
        DoSort(label);
    }

    void BtnClose() { LFPG_SorterView.PlayUIClick(); LFPG_SorterView.Close(); }

    // Bug #3: X close button in header
    void BtnCloseX() { LFPG_SorterView.PlayUIClick(); LFPG_SorterView.Close(); }

    // v2.8: Header quick-sort button
    void BtnSortHeader()
    {
        string label = "[SorterCtrl] REQUEST_SORT (header)";
        DoSort(label);
    }

    // N1: Shared sort logic (was duplicated in BtnSort + BtnSortHeader)
    protected void DoSort(string logLabel)
    {
        if (!m_IsPaired) return;
        LFPG_Util.Info(logLabel);
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
        // v2.6: Request server preview if on preview tab, else just count
        if (!m_ShowRules)
        {
            RequestPreview();
        }
        else
        {
            RefreshPreviewCount();
        }
        RefreshDestIndicator();
        // v3: Refresh edit hints
        LFPG_SorterView.RefreshHints();
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

            // V2: Check tabs with rules/catch-all
            tabCfg = m_Config.GetOutput(i);
            tabHasContent = false;
            if (tabCfg)
            {
                tabRules = tabCfg.GetRuleCount();
                if (tabRules > 0) { tabHasContent = true; }
                if (tabCfg.m_IsCatchAll) { tabHasContent = true; }
            }

            TintBg(GetTabBg(i), bgCol);
            tt = GetTabText(i);
            if (tt)
            {
                tt.SetColor(txtCol);
                tt.SetText(label);
            }
            else
            {
                // DIAG: Log if TextWidget is null for this tab index
                string diagTab = "[SorterCtrl] Tab ";
                diagTab = diagTab + i.ToString();
                diagTab = diagTab + " TextWidget NULL, label=";
                diagTab = diagTab + label;
                LFPG_Util.Warn(diagTab);
            }

            // v3: Tab dot indicator (replaces " *" suffix)
            ImageWidget dot = GetTabDot(i);
            if (dot)
            {
                dot.LoadImageFile(0, LFPG_SorterView.PROC_WHITE);
                if (tabHasContent && !isSel)
                {
                    dot.Show(true);
                    int dotCol = LFPG_SorterView.COL_GREEN;
                    bool onlyCatchAll = false;
                    if (tabCfg)
                    {
                        if (tabRules == 0 && tabCfg.m_IsCatchAll)
                        {
                            onlyCatchAll = true;
                        }
                    }
                    if (onlyCatchAll)
                    {
                        dotCol = LFPG_SorterView.COL_AMBER;
                    }
                    dot.SetColor(dotCol);
                }
                else
                {
                    dot.Show(false);
                }
            }
        }

        // Position active-tab indicator under selected tab.
        // v2.8: Read actual tab button position (UIScaler-compatible).
        if (TabIndicator)
        {
            float indCurX = 0.0;
            float indCurY = 0.0;
            TabIndicator.GetPos(indCurX, indCurY);
            ImageWidget selBg = GetTabBg(m_SelectedOutput);
            if (selBg)
            {
                Widget selBtn = selBg.GetParent();
                if (selBtn)
                {
                    float btnPosX = 0.0;
                    float btnPosY = 0.0;
                    selBtn.GetPos(btnPosX, btnPosY);
                    TabIndicator.SetPos(btnPosX, indCurY);
                }
            }
        }
    }

    protected ImageWidget GetTabBg(int idx)
    {
        if (idx == 0) return m_TabOut0Bg; if (idx == 1) return m_TabOut1Bg;
        if (idx == 2) return m_TabOut2Bg; if (idx == 3) return m_TabOut3Bg;
        if (idx == 4) return m_TabOut4Bg; if (idx == 5) return m_TabOut5Bg;
        return null;
    }
    protected TextWidget GetTabText(int idx)
    {
        if (idx == 0) return m_TabOut0Text; if (idx == 1) return m_TabOut1Text;
        if (idx == 2) return m_TabOut2Text; if (idx == 3) return m_TabOut3Text;
        if (idx == 4) return m_TabOut4Text; if (idx == 5) return m_TabOut5Text;
        return null;
    }

    // v3: Tab dot accessor
    protected ImageWidget GetTabDot(int idx)
    {
        if (idx == 0) return TabDot0;
        if (idx == 1) return TabDot1;
        if (idx == 2) return TabDot2;
        if (idx == 3) return TabDot3;
        if (idx == 4) return TabDot4;
        if (idx == 5) return TabDot5;
        return null;
    }

    protected void RefreshViewTabs()
    {
        int rBg = LFPG_SorterView.COL_BTN; int rTxt = LFPG_SorterView.COL_TEXT_DIM;
        int pBg = LFPG_SorterView.COL_BTN; int pTxt = LFPG_SorterView.COL_TEXT_DIM;
        if (m_ShowRules) { rBg = LFPG_SorterView.COL_BG_ELEVATED; rTxt = LFPG_SorterView.COL_BLUE; }
        else { pBg = LFPG_SorterView.COL_BG_ELEVATED; pTxt = LFPG_SorterView.COL_BLUE; }
        TintBg(TabRulesBg, rBg); TintBg(TabPreviewBg, pBg);
        if (TabRulesText) { TabRulesText.SetColor(rTxt); }
        if (TabPreviewText) { TabPreviewText.SetColor(pTxt); }
        if (RulesPanel) { RulesPanel.Show(m_ShowRules); }
        if (PreviewPanel) { PreviewPanel.Show(!m_ShowRules); }

        // v3: Position ViewTabIndicator under active view tab
        if (ViewTabIndicator)
        {
            ViewTabIndicator.LoadImageFile(0, LFPG_SorterView.PROC_WHITE);
            ViewTabIndicator.SetColor(LFPG_SorterView.COL_BLUE);
            ImageWidget activeViewBg = TabRulesBg;
            if (!m_ShowRules)
            {
                activeViewBg = TabPreviewBg;
            }
            if (activeViewBg)
            {
                Widget activeViewBtn = activeViewBg.GetParent();
                if (activeViewBtn)
                {
                    float vx = 0.0;
                    float vy = 0.0;
                    activeViewBtn.GetPos(vx, vy);
                    float ix = 0.0;
                    float iy = 0.0;
                    ViewTabIndicator.GetPos(ix, iy);
                    ViewTabIndicator.SetPos(vx, iy);
                }
            }
        }
    }

    protected void RefreshCategoryButtons()
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg) return;
        RefreshToggleBtn(CatBtn0Bg, CatBtn0Text, outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, m_CatValue0), LFPG_SorterView.COL_GREEN_BTN, LFPG_SorterView.COL_GREEN, m_CatLabel0);
        RefreshToggleBtn(CatBtn1Bg, CatBtn1Text, outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, m_CatValue1), LFPG_SorterView.COL_GREEN_BTN, LFPG_SorterView.COL_GREEN, m_CatLabel1);
        RefreshToggleBtn(CatBtn2Bg, CatBtn2Text, outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, m_CatValue2), LFPG_SorterView.COL_GREEN_BTN, LFPG_SorterView.COL_GREEN, m_CatLabel2);
        RefreshToggleBtn(CatBtn3Bg, CatBtn3Text, outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, m_CatValue3), LFPG_SorterView.COL_GREEN_BTN, LFPG_SorterView.COL_GREEN, m_CatLabel3);
        RefreshToggleBtn(CatBtn4Bg, CatBtn4Text, outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, m_CatValue4), LFPG_SorterView.COL_GREEN_BTN, LFPG_SorterView.COL_GREEN, m_CatLabel4);
        RefreshToggleBtn(CatBtn5Bg, CatBtn5Text, outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, m_CatValue5), LFPG_SorterView.COL_GREEN_BTN, LFPG_SorterView.COL_GREEN, m_CatLabel5);
        RefreshToggleBtn(CatBtn6Bg, CatBtn6Text, outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, m_CatValue6), LFPG_SorterView.COL_GREEN_BTN, LFPG_SorterView.COL_GREEN, m_CatLabel6);
        RefreshToggleBtn(CatBtn7Bg, CatBtn7Text, outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, m_CatValue7), LFPG_SorterView.COL_GREEN_BTN, LFPG_SorterView.COL_GREEN, m_CatLabel7);
    }

    protected void RefreshSlotButtons()
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg) return;
        string lblTiny = "Tiny";
        string lblSmall = "Small";
        string lblMed = "Med";
        string lblLarge = "Large";
        RefreshToggleBtn(SlotPre0Bg, SlotPre0Text, outCfg.HasRule(LFPG_SORT_FILTER_SLOT, LFPG_SORT_SLOT_TINY), LFPG_SorterView.COL_BLUE_BTN, LFPG_SorterView.COL_BLUE, lblTiny);
        RefreshToggleBtn(SlotPre1Bg, SlotPre1Text, outCfg.HasRule(LFPG_SORT_FILTER_SLOT, LFPG_SORT_SLOT_SMALL), LFPG_SorterView.COL_BLUE_BTN, LFPG_SorterView.COL_BLUE, lblSmall);
        RefreshToggleBtn(SlotPre2Bg, SlotPre2Text, outCfg.HasRule(LFPG_SORT_FILTER_SLOT, LFPG_SORT_SLOT_MEDIUM), LFPG_SorterView.COL_BLUE_BTN, LFPG_SorterView.COL_BLUE, lblMed);
        RefreshToggleBtn(SlotPre3Bg, SlotPre3Text, outCfg.HasRule(LFPG_SORT_FILTER_SLOT, LFPG_SORT_SLOT_LARGE), LFPG_SorterView.COL_BLUE_BTN, LFPG_SorterView.COL_BLUE, lblLarge);
    }

    // Unified toggle button refresh: active/inactive with custom active colors
    // v3: Added baseLabel param for active state indicator
    protected void RefreshToggleBtn(ImageWidget bg, TextWidget txt, bool active, int activeBg, int activeTxt, string baseLabel)
    {
        int bgCol = LFPG_SorterView.COL_BTN;
        int txtCol = LFPG_SorterView.COL_TEXT_MID;
        string displayLabel = baseLabel;
        if (active)
        {
            bgCol = activeBg;
            txtCol = activeTxt;
            string suffix = " *";
            displayLabel = baseLabel + suffix;
        }
        TintBg(bg, bgCol);
        if (txt)
        {
            txt.SetColor(txtCol);
            txt.SetText(displayLabel);
        }
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
    // Tags list rebuild — v2.6 pool pattern.
    // Clear() detaches widgets from WrapSpacer but pool refs
    // keep TagViews alive. SetData reuses existing instances.
    // Only creates new TagViews when pool is too small.
    // Max 9 tags per output (8 rules + 1 catch-all).
    // =========================================================
    protected void RefreshTagsList()
    {
        TagsList.Clear();
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg)
        {
            bool emptyAll = true;
            if (TagsEmpty) { TagsEmpty.Show(emptyAll); }
            if (TagsEmptyIcon) { TagsEmptyIcon.Show(emptyAll); }
            if (TagsEmptyHint) { TagsEmptyHint.Show(emptyAll); }
            return;
        }

        int ruleCount = outCfg.GetRuleCount();
        int needed = ruleCount;
        if (outCfg.m_IsCatchAll)
        {
            needed = needed + 1;
        }

        // Grow pool if needed (tags persist across tab switches)
        int poolSize = m_TagPool.Count();
        int pi;
        for (pi = poolSize; pi < needed; pi = pi + 1)
        {
            LFPG_SorterTagView newTag = new LFPG_SorterTagView();
            m_TagPool.Insert(newTag);
        }

        // Populate from rules
        int ri;
        int tagIdx = 0;
        LFPG_SorterTagView tag = null;
        LFPG_SortFilterRule rule = null;
        string label = "";
        int color = 0;
        for (ri = 0; ri < ruleCount; ri = ri + 1)
        {
            rule = outCfg.m_Rules[ri];
            if (!rule) continue;
            label = rule.GetDisplayLabel();
            color = GetRuleColor(rule.m_Type);
            tag = m_TagPool[tagIdx];
            tag.SetData(label, color, ri, m_SelectedOutput, this);
            TagsList.Insert(tag);
            tagIdx = tagIdx + 1;
        }

        // Catch-all tag (always last)
        if (outCfg.m_IsCatchAll)
        {
            string caLabel = "* CATCH-ALL";
            tag = m_TagPool[tagIdx];
            tag.SetData(caLabel, LFPG_SorterView.COL_AMBER, -1, m_SelectedOutput, this);
            TagsList.Insert(tag);
        }

        bool isEmpty = (ruleCount == 0 && !outCfg.m_IsCatchAll);
        if (TagsEmpty) { TagsEmpty.Show(isEmpty); }
        if (TagsEmptyIcon) { TagsEmptyIcon.Show(isEmpty); }
        if (TagsEmptyHint) { TagsEmptyHint.Show(isEmpty); }
    }

    protected int GetRuleColor(int ruleType)
    {
        if (ruleType == LFPG_SORT_FILTER_CATEGORY) return LFPG_SorterView.COL_GREEN;
        if (ruleType == LFPG_SORT_FILTER_PREFIX) return LFPG_SorterView.COL_BLUE;
        if (ruleType == LFPG_SORT_FILTER_CONTAINS) return LFPG_SorterView.COL_AMBER;
        if (ruleType == LFPG_SORT_FILTER_SLOT) return LFPG_SorterView.COL_PURPLE;
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
        bool showEmpty = (count == 0);
        if (PreviewEmpty)
        {
            if (showEmpty)
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
        if (PreviewEmptyIcon) { PreviewEmptyIcon.Show(showEmpty); }
        if (PreviewEmptyHint) { PreviewEmptyHint.Show(showEmpty); }
    }

    // =========================================================
    // v2.6: Preview Items — server-authoritative
    // =========================================================
    protected void RequestPreview()
    {
        if (!m_IsPaired)
        {
            // Show "not linked" empty state immediately
            PreviewItems.Clear();
            string noLink = "No container linked";
            if (PreviewEmpty) { PreviewEmpty.SetText(noLink); PreviewEmpty.Show(true); }
            if (PreviewEmptyIcon) { PreviewEmptyIcon.Show(true); }
            if (PreviewEmptyHint) { PreviewEmptyHint.Show(false); }
            string zeroPrev = "0 items";
            PreviewCount = zeroPrev;
            string propPC = "PreviewCount";
            NotifyPropertyChanged(propPC, false);
            return;
        }
        #ifndef SERVER
        if (!GetGame())
            return;
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (player)
        {
            ScriptRPC rpc = new ScriptRPC();
            int subId = LFPG_RPC_SubId.SORTER_PREVIEW_REQUEST;
            rpc.Write(subId);
            rpc.Write(m_SorterNetLow);
            rpc.Write(m_SorterNetHigh);
            rpc.Write(m_SelectedOutput);
            rpc.Send(player, LFPG_RPC_CHANNEL, true, null);
        }
        #endif
    }

    // Called from View.OnPreviewData (static delegate from PlayerRPC)
    void PopulatePreview(int outputIdx, int totalMatched, array<string> names, array<string> cats, array<int> slots)
    {
        // Guard: if user switched output tab while RPC was in flight, ignore
        if (outputIdx != m_SelectedOutput)
            return;

        PreviewItems.Clear();

        int sentCount = names.Count();
        int si;
        string itemName = "";
        string itemCat = "";
        int itemSlot = 0;
        for (si = 0; si < sentCount; si = si + 1)
        {
            itemName = names[si];
            itemCat = cats[si];
            itemSlot = slots[si];
            LFPG_SorterPreviewRow row = new LFPG_SorterPreviewRow();
            row.SetData(itemName, itemCat, itemSlot);
            PreviewItems.Insert(row);
        }

        // Update count display
        bool showEmpty = (sentCount == 0);
        string countStr = "";
        if (totalMatched > LFPG_SORTER_PREVIEW_CAP)
        {
            string capStr = LFPG_SORTER_PREVIEW_CAP.ToString();
            countStr = capStr;
            countStr = countStr + "+ items";
        }
        else
        {
            countStr = totalMatched.ToString();
            string suffItems = " items";
            countStr = countStr + suffItems;
        }
        PreviewCount = countStr;
        string propPC = "PreviewCount";
        NotifyPropertyChanged(propPC, false);

        // Empty states
        if (PreviewEmpty)
        {
            if (showEmpty)
            {
                string emptyMsg = "No matching items";
                PreviewEmpty.SetText(emptyMsg);
                PreviewEmpty.Show(true);
            }
            else
            {
                PreviewEmpty.Show(false);
            }
        }
        if (PreviewEmptyIcon) { PreviewEmptyIcon.Show(showEmpty); }
        if (PreviewEmptyHint)
        {
            if (showEmpty)
            {
                string hintMsg = "Add rules or enable catch-all";
                PreviewEmptyHint.SetText(hintMsg);
                PreviewEmptyHint.Show(true);
            }
            else
            {
                PreviewEmptyHint.Show(false);
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

    // FIX 2: Release tag/preview views on close to break circular refs.
    // Called from View.DoClose. Safe: destructor of TagView already
    // nulls m_OwnerController, so Clear triggers clean teardown.
    // v2.6: Pool must be cleared AFTER TagsList.Clear() so destructors
    // fire when refcount drops to 0 (pool held the last strong ref).
    void ClearCollections()
    {
        if (TagsList)
        {
            TagsList.Clear();
        }
        if (m_TagPool)
        {
            m_TagPool.Clear();
        }
        if (PreviewItems)
        {
            PreviewItems.Clear();
        }
    }
};
