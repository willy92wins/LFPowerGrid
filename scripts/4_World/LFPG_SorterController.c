// =========================================================
// LF_PowerGrid — Sorter Controller (Dabs MVC, v3.3)
//
// v3.3 changes (Sprint 3 — Performance):
//   P4: Granular refresh — handlers call targeted subsets
//       instead of RefreshAll(). ~60% fewer ops per click.
//       New helpers: RefreshRulesDisplay, RefreshFilterButtons.
//       OnRemoveTag reads rule type before delete for precision.
//
// v2.6 changes (Tag Pool — Phase 5):
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
    // ── Preview pool (reuse PreviewRows across PopulatePreview calls) ──
    ref array<ref LFPG_SorterPreviewRow> m_PreviewPool;

    // ── Internal state ──
    protected ref LFPG_SortConfig m_Config;
    protected int m_SelectedOutput;
    protected bool m_ShowRules;
    protected bool m_ResetConfirmActive;
    protected float m_ResetTimer;
    protected float m_FeedbackTimer;

    // ── Pairing state (Bug #5/#6) ──
    protected bool m_IsPaired;
    // P3: Track first TintBg pass (LoadImageFile only needed once)
    protected bool m_BgInitialized;
    protected string m_ContainerDisplayName;

    // ── RPC identity ──
    protected int m_SorterNetLow;
    protected int m_SorterNetHigh;

    // ── Dest names (array replaces m_Dest0..5) ──
    protected ref array<string> m_Dests;

    // ── Category (arrays replace m_CatLabel0..7 / m_CatValue0..7) ──
    protected ref array<string> m_CatLabels;
    protected ref array<string> m_CatValues;
    // ── Slot preset values (for index-based dispatch) ──
    protected ref array<string> m_SlotValues;
    protected ref array<string> m_SlotLabels;

    // ── Widget refs (resolved by child-walk in EnsureBindings) ──
    TextWidget StatusLabel;
    ImageWidget StatusDot;

    // Output tabs — stored in arrays to prevent Dabs auto-bind from
    // overwriting refs during NotifyPropertyChanged. Dabs only matches
    // named Widget/ImageWidget/TextWidget fields, NOT array elements.
    protected ref array<ImageWidget> m_TabBgs;
    protected ref array<TextWidget> m_TabTexts;
    // View tabs
    ImageWidget TabRulesBg; ImageWidget TabPreviewBg;
    TextWidget TabRulesText; TextWidget TabPreviewText;
    ImageWidget TabIndicator;
    // Category — arrays prevent Dabs auto-bind corruption (same fix as F2 tabs)
    protected ref array<ImageWidget> m_CatBgs;
    protected ref array<TextWidget> m_CatTexts;
    // Slot — arrays prevent Dabs auto-bind corruption
    protected ref array<ImageWidget> m_SlotBgs;
    protected ref array<TextWidget> m_SlotTexts;
    // m_LayoutRoot — inherited from ViewController (ScriptedWidgetEventHandler)
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

    // v3: Tab dots (array replaces TabDot0..5)
    protected ref array<ImageWidget> m_TabDots;
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
        m_PreviewPool = new array<ref LFPG_SorterPreviewRow>;
        m_Config = new LFPG_SortConfig();
        // v3.2: Tab widget arrays (6 outputs)
        m_TabBgs = new array<ImageWidget>;
        m_TabTexts = new array<TextWidget>;
        int ti = 0;
        for (ti = 0; ti < 6; ti = ti + 1)
        {
            m_TabBgs.Insert(null);
            m_TabTexts.Insert(null);
        }
        // v3.2: Category button arrays (8 categories) — Dabs-proof
        m_CatBgs = new array<ImageWidget>;
        m_CatTexts = new array<TextWidget>;
        int ci = 0;
        for (ci = 0; ci < 8; ci = ci + 1)
        {
            m_CatBgs.Insert(null);
            m_CatTexts.Insert(null);
        }
        // v3.2: Slot preset arrays (4 slots) — Dabs-proof
        m_SlotBgs = new array<ImageWidget>;
        m_SlotTexts = new array<TextWidget>;
        int si = 0;
        for (si = 0; si < 4; si = si + 1)
        {
            m_SlotBgs.Insert(null);
            m_SlotTexts.Insert(null);
        }
        m_SelectedOutput = 0;
        m_ShowRules = true;
        m_ResetConfirmActive = false;
        m_ResetTimer = 0.0;
        m_FeedbackTimer = 0.0;
        m_SorterNetLow = 0;
        m_SorterNetHigh = 0;
        m_IsPaired = false;
        m_ContainerDisplayName = "";

        // M1: Category labels + values (data-driven)
        m_CatLabels = new array<string>;
        m_CatValues = new array<string>;
        string lW = "Weapons";  string vW = "WEAPON";     m_CatLabels.Insert(lW); m_CatValues.Insert(vW);
        string lAt = "Attach";  string vAt = "ATTACHMENT"; m_CatLabels.Insert(lAt); m_CatValues.Insert(vAt);
        string lAm = "Ammo";   string vAm = "AMMO";      m_CatLabels.Insert(lAm); m_CatValues.Insert(vAm);
        string lCl = "Clothing"; string vCl = "CLOTHING"; m_CatLabels.Insert(lCl); m_CatValues.Insert(vCl);
        string lFo = "Food";   string vFo = "FOOD";      m_CatLabels.Insert(lFo); m_CatValues.Insert(vFo);
        string lMe = "Medical"; string vMe = "MEDICAL";   m_CatLabels.Insert(lMe); m_CatValues.Insert(vMe);
        string lTo = "Tools";  string vTo = "TOOL";       m_CatLabels.Insert(lTo); m_CatValues.Insert(vTo);
        string lMi = "Misc";   string vMi = "MISC";       m_CatLabels.Insert(lMi); m_CatValues.Insert(vMi);

        // M1: Slot preset values (index-based dispatch)
        m_SlotValues = new array<string>;
        m_SlotValues.Insert(LFPG_SORT_SLOT_TINY);
        m_SlotValues.Insert(LFPG_SORT_SLOT_SMALL);
        m_SlotValues.Insert(LFPG_SORT_SLOT_MEDIUM);
        m_SlotValues.Insert(LFPG_SORT_SLOT_LARGE);
        m_SlotLabels = new array<string>;
        string slT = "Tiny"; string slS = "Small"; string slM = "Med"; string slL = "Large";
        m_SlotLabels.Insert(slT); m_SlotLabels.Insert(slS); m_SlotLabels.Insert(slM); m_SlotLabels.Insert(slL);

        // M1: Dest names array
        m_Dests = new array<string>;
        int di = 0;
        for (di = 0; di < 6; di = di + 1)
        {
            string emptyDest = "";
            m_Dests.Insert(emptyDest);
        }

        // M1: TabDot array
        m_TabDots = new array<ImageWidget>;
        int dti = 0;
        for (dti = 0; dti < 6; dti = dti + 1)
        {
            m_TabDots.Insert(null);
        }
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

        m_LayoutRoot = layoutRoot;
        string bn = "";

        // ── Output tabs (child-walk → store in arrays, safe from Dabs rebind) ──
        string tabName = "";
        int tabIdx = 0;
        for (tabIdx = 0; tabIdx < 6; tabIdx = tabIdx + 1)
        {
            tabName = "TabOut";
            tabName = tabName + tabIdx.ToString();
            m_TabBgs.Set(tabIdx, FindBtnChildBg(layoutRoot, tabName));
            m_TabTexts.Set(tabIdx, FindBtnChildText(layoutRoot, tabName));
        }

        // ── View tabs ──
        bn = "TabRules";
        TabRulesBg = FindBtnChildBg(layoutRoot, bn);
        TabRulesText = FindBtnChildText(layoutRoot, bn);
        bn = "TabPreview";
        TabPreviewBg = FindBtnChildBg(layoutRoot, bn);
        TabPreviewText = FindBtnChildText(layoutRoot, bn);

        // ── Category buttons (arrays — safe from Dabs rebind) ──
        string catName = "";
        int catIdx = 0;
        for (catIdx = 0; catIdx < 8; catIdx = catIdx + 1)
        {
            catName = "CatBtn";
            catName = catName + catIdx.ToString();
            m_CatBgs.Set(catIdx, FindBtnChildBg(layoutRoot, catName));
            m_CatTexts.Set(catIdx, FindBtnChildText(layoutRoot, catName));
        }

        // ── Slot presets (arrays — safe from Dabs rebind) ──
        string slotName = "";
        int slotIdx = 0;
        for (slotIdx = 0; slotIdx < 4; slotIdx = slotIdx + 1)
        {
            slotName = "SlotPre";
            slotName = slotName + slotIdx.ToString();
            m_SlotBgs.Set(slotIdx, FindBtnChildBg(layoutRoot, slotName));
            m_SlotTexts.Set(slotIdx, FindBtnChildText(layoutRoot, slotName));
        }

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

        // v3: Tab dots (M1: array-based loop)
        int dotIdx = 0;
        string dotPrefix = "TabDot";
        for (dotIdx = 0; dotIdx < 6; dotIdx = dotIdx + 1)
        {
            if (!m_TabDots.Get(dotIdx))
            {
                wn = dotPrefix;
                wn = wn + dotIdx.ToString();
                m_TabDots.Set(dotIdx, ImageWidget.Cast(layoutRoot.FindAnyWidget(wn)));
            }
        }
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
        m_Dests.Set(0, d0); m_Dests.Set(1, d1); m_Dests.Set(2, d2);
        m_Dests.Set(3, d3); m_Dests.Set(4, d4); m_Dests.Set(5, d5);

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
        string diagBindings = "[SorterCtrl] Bindings CatBg0=";
        if (m_CatBgs.Get(0)) { diagBindings = diagBindings + "OK"; }
        else { diagBindings = diagBindings + "NULL"; }
        diagBindings = diagBindings + " CatTxt0=";
        if (m_CatTexts.Get(0)) { diagBindings = diagBindings + "OK"; }
        else { diagBindings = diagBindings + "NULL"; }
        diagBindings = diagBindings + " TabOut1Text=";
        if (m_TabTexts.Get(1)) { diagBindings = diagBindings + "OK"; }
        else { diagBindings = diagBindings + "NULL"; }
        diagBindings = diagBindings + " TabOut3Text=";
        if (m_TabTexts.Get(3)) { diagBindings = diagBindings + "OK"; }
        else { diagBindings = diagBindings + "NULL"; }
        diagBindings = diagBindings + " TabOut5Text=";
        if (m_TabTexts.Get(5)) { diagBindings = diagBindings + "OK"; }
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

        // v3.2: Dabs re-bind corrupts named widget fields after NotifyPropertyChanged.
        // Re-resolve before ApplyInitialColors/Labels use them.
        ReBindButtons();

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
        m_BgInitialized = true;
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
        // M1: Data-driven category labels
        int ci = 0;
        for (ci = 0; ci < 8; ci = ci + 1)
        {
            SetBtnLabel(m_CatTexts.Get(ci), m_CatLabels.Get(ci));
        }
        // M1: Data-driven slot labels
        int si = 0;
        for (si = 0; si < 4; si = si + 1)
        {
            SetBtnLabel(m_SlotTexts.Get(si), m_SlotLabels.Get(si));
        }
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
            int dci = 0;
            for (dci = 0; dci < 8; dci = dci + 1)
            {
                TintBg(m_CatBgs.Get(dci), dimBg);
                SetTxtCol(m_CatTexts.Get(dci), dimTxt);
            }

            // Dim all slot preset buttons
            int dsi = 0;
            for (dsi = 0; dsi < 4; dsi = dsi + 1)
            {
                TintBg(m_SlotBgs.Get(dsi), dimBg);
                SetTxtCol(m_SlotTexts.Get(dsi), dimTxt);
            }

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
        string stSortFail = "SORT FAILED";
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
        if (st == stSortFail)
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

    // v3.2: Server sort result feedback
    void HandleSortAck(bool success, int movedCount)
    {
        if (success)
        {
            string stSorted = "SORTED: ";
            stSorted = stSorted + movedCount.ToString();
            SetStatus(stSorted);
        }
        else
        {
            string stFail = "SORT FAILED";
            SetStatus(stFail);
        }
        m_FeedbackTimer = 3.0;
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
    // M2: Index-based handlers (called from View OnClick dispatch)
    // =========================================================
    void SelectOutput(int idx)
    {
        // N2: No output tab switching when unpaired
        if (!m_IsPaired)
            return;
        if (idx < 0 || idx >= LFPG_SORT_MAX_OUTPUTS)
            return;
        m_SelectedOutput = idx;
        m_ResetConfirmActive = false;
        RefreshAll();
    }

    // =========================================================
    // View tabs
    // =========================================================
    void TabRules()  { m_ShowRules = true;  RefreshViewTabs(); }
    void TabPreview() { m_ShowRules = false; RefreshViewTabs(); RequestPreview(); }

    // =========================================================
    // M1: Category toggle by index (replaces CatBtn0..7)
    // =========================================================
    void ToggleCategoryByIdx(int idx)
    {
        if (!m_IsPaired) return;
        if (idx < 0 || idx >= m_CatValues.Count()) return;
        string catValue = m_CatValues.Get(idx);
        ToggleCategory(catValue);
    }

    protected void ToggleCategory(string catValue)
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg) return;
        bool hasIt = outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, catValue);
        if (hasIt) { RemoveRuleByValue(outCfg, LFPG_SORT_FILTER_CATEGORY, catValue); }
        else { outCfg.AddRule(LFPG_SORT_FILTER_CATEGORY, catValue); }
        // P4: Only cat buttons + rules changed
        ReBindButtons();
        RefreshCategoryButtons();
        RefreshRulesDisplay();
    }

    // =========================================================
    // M1: Slot toggle by index (replaces SlotPre0..3)
    // =========================================================
    void ToggleSlotByIdx(int idx)
    {
        if (!m_IsPaired) return;
        if (idx < 0 || idx >= m_SlotValues.Count()) return;
        string slotValue = m_SlotValues.Get(idx);
        ToggleSlot(slotValue);
    }

    protected void ToggleSlot(string slotValue)
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg) return;
        bool hasIt = outCfg.HasRule(LFPG_SORT_FILTER_SLOT, slotValue);
        if (hasIt) { RemoveRuleByValue(outCfg, LFPG_SORT_FILTER_SLOT, slotValue); }
        else { outCfg.AddRule(LFPG_SORT_FILTER_SLOT, slotValue); }
        // P4: Only slot buttons + rules changed
        ReBindButtons();
        RefreshSlotButtons();
        RefreshRulesDisplay();
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
        // P4: Only rules changed + hints (edit field cleared)
        ReBindButtons();
        RefreshRulesDisplay();
        LFPG_SorterView.RefreshHints();
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
        // P4: Only rules changed + hints (edit field cleared)
        ReBindButtons();
        RefreshRulesDisplay();
        LFPG_SorterView.RefreshHints();
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
        // P4: Only rules changed + hints (edit fields cleared)
        ReBindButtons();
        RefreshRulesDisplay();
        LFPG_SorterView.RefreshHints();
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
        // P4: Only catch-all button + rules changed
        ReBindButtons();
        RefreshCatchAllButton();
        RefreshRulesDisplay();
    }

    void BtnClearOut()
    {
        if (!m_IsPaired) return;
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg) return;
        outCfg.ClearRules();
        // P4: All filter buttons reset + rules cleared
        ReBindButtons();
        RefreshFilterButtons();
        RefreshRulesDisplay();
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
            return;
        }
        m_ResetConfirmActive = false;
        m_Config.ResetAll();
        string resetLabel = "Reset All";
        if (BtnResetAllText) { BtnResetAllText.SetText(resetLabel); }
        TintBg(BtnResetAllBg, LFPG_SorterView.COL_RED_BTN);
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

    void BtnClose() { LFPG_SorterView.Close(); }

    // Bug #3: X close button in header
    void BtnCloseX() { LFPG_SorterView.Close(); }

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
        // S6: Timeout timer — SORT_ACK will override with real result
        m_FeedbackTimer = 8.0;
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

        // P4: Read rule type BEFORE removing so we know which buttons to refresh
        int removedType = -1;
        if (ruleIdx >= 0 && ruleIdx < outCfg.m_Rules.Count())
        {
            LFPG_SortFilterRule rule = outCfg.m_Rules[ruleIdx];
            if (rule)
            {
                removedType = rule.m_Type;
            }
        }

        if (ruleIdx < 0) { outCfg.m_IsCatchAll = false; }
        else { outCfg.RemoveRuleAt(ruleIdx); }

        // P4: Targeted button refresh based on removed rule type
        ReBindButtons();
        if (ruleIdx < 0)
        {
            RefreshCatchAllButton();
        }
        else if (removedType == LFPG_SORT_FILTER_CATEGORY)
        {
            RefreshCategoryButtons();
        }
        else if (removedType == LFPG_SORT_FILTER_SLOT)
        {
            RefreshSlotButtons();
        }
        // PREFIX and CONTAINS have no toggle buttons — no button refresh needed
        RefreshRulesDisplay();
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
    // v3.2: Re-resolve non-array button widgets before each refresh.
    // Dabs auto-bind corrupts named ImageWidget/TextWidget fields
    // after any NotifyPropertyChanged call. Arrays are safe (Dabs
    // cannot match array elements), but named fields must be
    // re-resolved each time from m_LayoutRoot.
    protected void ReBindButtons()
    {
        if (!m_LayoutRoot)
            return;
        string bn = "";
        bn = "BtnCatchAll";
        BtnCatchAllBg = FindBtnChildBg(m_LayoutRoot, bn);
        BtnCatchAllText = FindBtnChildText(m_LayoutRoot, bn);
        bn = "BtnPrefixAdd";
        BtnPrefixAddBg = FindBtnChildBg(m_LayoutRoot, bn);
        bn = "BtnContainsAdd";
        BtnContainsAddBg = FindBtnChildBg(m_LayoutRoot, bn);
        bn = "BtnSlotAdd";
        BtnSlotAddBg = FindBtnChildBg(m_LayoutRoot, bn);
        bn = "BtnSort";
        BtnSortBg = FindBtnChildBg(m_LayoutRoot, bn);
        BtnSortText = FindBtnChildText(m_LayoutRoot, bn);
        bn = "BtnSave";
        BtnSaveBg = FindBtnChildBg(m_LayoutRoot, bn);
        BtnSaveText = FindBtnChildText(m_LayoutRoot, bn);
        bn = "BtnResetAll";
        BtnResetAllBg = FindBtnChildBg(m_LayoutRoot, bn);
        BtnResetAllText = FindBtnChildText(m_LayoutRoot, bn);
        bn = "BtnClearOut";
        BtnClearOutBg = FindBtnChildBg(m_LayoutRoot, bn);
        BtnClearOutText = FindBtnChildText(m_LayoutRoot, bn);
        bn = "BtnClose";
        BtnCloseBg = FindBtnChildBg(m_LayoutRoot, bn);
        BtnCloseText = FindBtnChildText(m_LayoutRoot, bn);
        bn = "BtnSortHeader";
        BtnSortHeaderBg = FindBtnChildBg(m_LayoutRoot, bn);
        BtnSortHeaderText = FindBtnChildText(m_LayoutRoot, bn);
        bn = "TabRules";
        TabRulesBg = FindBtnChildBg(m_LayoutRoot, bn);
        TabRulesText = FindBtnChildText(m_LayoutRoot, bn);
        bn = "TabPreview";
        TabPreviewBg = FindBtnChildBg(m_LayoutRoot, bn);
        TabPreviewText = FindBtnChildText(m_LayoutRoot, bn);
    }

    protected void RefreshAll()
    {
        ReBindButtons();
        RefreshOutputTabs();
        RefreshViewTabs();
        RefreshFilterButtons();
        RefreshRulesDisplay();
        RefreshDestIndicator();
        // v3: Refresh edit hints
        LFPG_SorterView.RefreshHints();
        // Apply disabled visual after all refreshes (v2.2)
        SetControlsEnabled(m_IsPaired);
    }

    // =========================================================
    // P4: Granular refresh helpers — subsets of RefreshAll for
    // handlers that only modify rules (not output/tab context).
    // Order matters: TagsList uses named widgets (safe before NPC),
    // then RuleCount/MatchCount call NotifyPropertyChanged.
    // =========================================================
    protected void RefreshRulesDisplay()
    {
        RefreshTagsList();
        RefreshRuleCount();
        RefreshMatchCount();
        if (!m_ShowRules)
        {
            RequestPreview();
        }
        else
        {
            RefreshPreviewCount();
        }
    }

    protected void RefreshFilterButtons()
    {
        RefreshCategoryButtons();
        RefreshSlotButtons();
        RefreshCatchAllButton();
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
        if (idx < 0) return null;
        if (idx > 5) return null;
        if (!m_TabBgs) return null;
        return m_TabBgs.Get(idx);
    }
    protected TextWidget GetTabText(int idx)
    {
        if (idx < 0) return null;
        if (idx > 5) return null;
        if (!m_TabTexts) return null;
        return m_TabTexts.Get(idx);
    }

    // v3: Tab dot accessor
    protected ImageWidget GetTabDot(int idx)
    {
        if (idx < 0 || idx >= m_TabDots.Count())
            return null;
        return m_TabDots.Get(idx);
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
        int ci = 0;
        int gbtn = LFPG_SorterView.COL_GREEN_BTN;
        int gtxt = LFPG_SorterView.COL_GREEN;
        bool hasRule = false;
        string catVal = "";
        string catLbl = "";
        for (ci = 0; ci < 8; ci = ci + 1)
        {
            catVal = m_CatValues.Get(ci);
            catLbl = m_CatLabels.Get(ci);
            hasRule = outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, catVal);
            RefreshToggleBtn(m_CatBgs.Get(ci), m_CatTexts.Get(ci), hasRule, gbtn, gtxt, catLbl);
        }
    }

    protected void RefreshSlotButtons()
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg) return;
        int bbtn = LFPG_SorterView.COL_BLUE_BTN;
        int btxt = LFPG_SorterView.COL_BLUE;
        bool hasRule = false;
        string slotVal = "";
        int si = 0;
        for (si = 0; si < 4; si = si + 1)
        {
            slotVal = m_SlotValues.Get(si);
            hasRule = outCfg.HasRule(LFPG_SORT_FILTER_SLOT, slotVal);
            RefreshToggleBtn(m_SlotBgs.Get(si), m_SlotTexts.Get(si), hasRule, bbtn, btxt, m_SlotLabels.Get(si));
        }
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
        if (idx < 0 || idx >= m_Dests.Count())
            return "";
        return m_Dests.Get(idx);
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

        // P2: Grow pool if needed (rows persist across preview refreshes)
        int poolSize = m_PreviewPool.Count();
        int pi = 0;
        for (pi = poolSize; pi < sentCount; pi = pi + 1)
        {
            LFPG_SorterPreviewRow newRow = new LFPG_SorterPreviewRow();
            m_PreviewPool.Insert(newRow);
        }

        int si = 0;
        string itemName = "";
        string itemCat = "";
        int itemSlot = 0;
        LFPG_SorterPreviewRow row = null;
        for (si = 0; si < sentCount; si = si + 1)
        {
            itemName = names[si];
            itemCat = cats[si];
            itemSlot = slots[si];
            row = m_PreviewPool[si];
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
        // P3: LoadImageFile only on first pass — subsequent calls just SetColor
        if (!m_BgInitialized)
        {
            bg.LoadImageFile(0, LFPG_SorterView.PROC_WHITE);
        }
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
        if (m_PreviewPool)
        {
            m_PreviewPool.Clear();
        }
    }
};
