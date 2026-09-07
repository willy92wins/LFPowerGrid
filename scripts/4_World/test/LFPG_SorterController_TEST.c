#ifndef SERVER
// Client-only compilation boundary
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
//   - m_TagPool: REMOVED in v4.1 (pool reuse broke Dabs re-parenting).
//     Fresh TagViews created each RefreshTagsList call.
//   - ClearCollections: pool cleared on DoClose to break refs.
//
// v2.3 changes (P3 Performance & Polish):
//   S5: Extracted GetStatusColor (eliminates 40-line duplication)
//   S6: Merged save/sort feedback timers into m_FeedbackTimer
//   E1: String literals converted to local variables
//   V2: Tab rule indicators (* on tabs with rules/catch-all)
//   R4: g_Game null-guard in BtnSave/BtnSortHeader
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

class LFPG_SorterController_TEST extends ViewController
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
    ref ObservableCollection<ref LFPG_SorterTagView_TEST> TagsList;
    ref ObservableCollection<ref LFPG_SorterPreviewRow_TEST> PreviewItems;

    // ── (v4.2: m_PreviewPool REMOVED — same Dabs re-parenting issue as tags) ──

    // ── Internal state ──
    protected ref LFPG_SortConfig m_Config;
    protected int m_SelectedOutput;
    protected bool m_ShowRules;
    protected bool m_ResetConfirmActive;
    protected float m_ResetTimer;
    protected float m_FeedbackTimer;

    // D2 (S2 reflow): client-side power state for the read-only guard
    protected bool m_IsPowered;
    protected string m_LastStatus;
    // ClearOut two-click confirm (mirrors the ResetAll pattern)
    protected bool m_ClearConfirmActive;
    protected float m_ClearTimer;
    // D2: cadence for the netsync power re-poll while the panel is open
    protected float m_PowerPollTimer;

    // ── B2 (2026-04-26): in-flight throttling for Save/Sort RPCs ──
    // Prevents user from spamming the button and flooding the server with
    // redundant requests. Reset on the corresponding Ack handler.
    protected bool m_SaveInFlight;
    protected bool m_SortInFlight;
    protected bool m_PreviewInFlight;
    protected float m_PreviewInFlightSince;
    protected bool m_PreviewPending;
    protected float m_PreviewDebounce;

    // ── Pairing state (Bug #5/#6) ──
    protected bool m_IsPaired;
    // Sprint 2 (2026-04-26): builder tab state — 0=CAT 1=PFX 2=CON 3=SLT
    protected int m_ActiveBuilderTab_TEST;
    // ─── IS3 (Sprint 4.5, 2026-04-26): widget caches ─────────────────
    // Resolved once via EnsureV4Cache_TEST(); reused every Refresh.
    // Replaces ~80 FindAnyWidget calls per RefreshAll with 0.
    protected ref array<ImageWidget> m_RailRowBgs_TEST;
    protected ref array<ImageWidget> m_RailRowIndicators_TEST;
    protected ref array<TextWidget>  m_RailRowLabels_TEST;
    protected ref array<TextWidget>  m_RailRowCounts_TEST;
    protected ref array<TextWidget>  m_RailRowContainers_TEST;
    protected ref array<ImageWidget> m_BuilderTabUnderlines_TEST;  // [CAT, PFX, CON, SLT]
    protected ref array<TextWidget>  m_BuilderTabTexts_TEST;       // [CAT, PFX, CON, SLT]
    protected ref array<Widget>      m_SectionRoots_TEST;         // [CAT, PFX, CON, SLT]
    protected TextWidget             m_BuilderCtxLabel_TEST;
    protected TextWidget             m_RulesSublabel_TEST;
    protected bool                   m_V4CacheBuilt_TEST;
    // F4-B: Track first EnsureBindings array resolution
    protected bool m_ArraysResolved;
    protected string m_ContainerDisplayName;
    // F3-B: Last matched item count from preview RPC (-1 = not fetched)
    protected int m_LastMatchedItems;

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

    // Category — arrays prevent Dabs auto-bind corruption (same fix as F2 tabs)
    protected ref array<ImageWidget> m_CatBgs;
    protected ref array<TextWidget> m_CatTexts;
    // Slot — arrays prevent Dabs auto-bind corruption
    protected ref array<ImageWidget> m_SlotBgs;
    protected ref array<TextWidget> m_SlotTexts;
    // F4-D: Reusable sort index array (avoids new array per PopulatePreview)
    protected ref array<int> m_SortIdx;
    // m_LayoutRoot — inherited from ViewController (ScriptedWidgetEventHandler)
    // Catch-all
    ImageWidget BtnCatchAllBg; TextWidget BtnCatchAllText;
    // Preview toggle button
    ImageWidget BtnPreviewBg; TextWidget BtnPreviewText;
    // Footer + header button BGs
    ImageWidget BtnSaveBg;
    ImageWidget BtnResetAllBg; ImageWidget BtnClearOutBg;
    ImageWidget BtnPrefixAddBg; ImageWidget BtnContainsAddBg; ImageWidget BtnSlotAddBg;
    TextWidget BtnResetAllText;
    TextWidget BtnSaveText;
    TextWidget BtnClearOutText;
    // Header sort button
    ImageWidget BtnSortHeaderBg; TextWidget BtnSortHeaderText;
    // Label refs
    TextWidget LblCategory; TextWidget LblPrefix; TextWidget LblContains;
    TextWidget LblSlot; TextWidget LblSlotDash;
    TextWidget LblPreview;
    // Panels
    Widget RulesPanel; Widget PreviewPanel;
    TextWidget TagsEmpty; TextWidget PreviewEmpty;

    // v3: Empty state extras (P-IV)
    TextWidget TagsEmptyIcon;
    TextWidget TagsEmptyHint;
    TextWidget PreviewEmptyIcon;
    TextWidget PreviewEmptyHint;

    // =========================================================
    void LFPG_SorterController_TEST()
    {
        TagsList = new ObservableCollection<ref LFPG_SorterTagView_TEST>(this);
        PreviewItems = new ObservableCollection<ref LFPG_SorterPreviewRow_TEST>(this);
        m_Config = new LFPG_SortConfig();
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
        // F4-D: Reusable sort index array
        m_SortIdx = new array<int>;
        m_SelectedOutput = 0;
        m_ShowRules = true;
        m_ResetConfirmActive = false;
        m_ResetTimer = 0.0;
        m_IsPowered = false;
        m_LastStatus = "";
        m_ClearConfirmActive = false;
        m_ClearTimer = 0.0;
        m_PowerPollTimer = 0.0;
        m_FeedbackTimer = 0.0;
        m_SaveInFlight = false;
        m_SortInFlight = false;
        m_PreviewInFlight = false;
        m_PreviewInFlightSince = 0.0;
        m_PreviewPending = false;
        m_PreviewDebounce = 0.0;
        m_SorterNetLow = 0;
        m_SorterNetHigh = 0;
        m_IsPaired = false;
        m_ContainerDisplayName = "";
        m_ActiveBuilderTab_TEST = 0;  // Sprint 2: default to CATEGORY tab
        // IS3 cache arrays (populated lazily in EnsureV4Cache_TEST)
        m_RailRowBgs_TEST         = new array<ImageWidget>;
        m_RailRowIndicators_TEST  = new array<ImageWidget>;
        m_RailRowLabels_TEST      = new array<TextWidget>;
        m_RailRowCounts_TEST      = new array<TextWidget>;
        m_RailRowContainers_TEST  = new array<TextWidget>;
        m_BuilderTabUnderlines_TEST = new array<ImageWidget>;
        m_BuilderTabTexts_TEST      = new array<TextWidget>;
        m_SectionRoots_TEST = new array<Widget>;
        m_V4CacheBuilt_TEST = false;
        m_LastMatchedItems = -1;

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
        // IS3 (Sprint 4.5, 2026-04-26): warm V4 widget cache once.
        EnsureV4Cache_TEST();
        string bn = "";

        // F4-B: Array loops only on first open — arrays are stable across Dabs rebind.
        // Named fields below MUST re-resolve each open (Dabs may rebind them).
        if (!m_ArraysResolved)
        {

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


            m_ArraysResolved = true;
        }

        // Preview toggle button (re-resolve each open, Dabs may rebind)
        bn = "BtnPreview";
        BtnPreviewBg = FindBtnChildBg(layoutRoot, bn);
        BtnPreviewText = FindBtnChildText(layoutRoot, bn);

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
        bn = "BtnSave";
        BtnSaveBg = FindBtnChildBg(layoutRoot, bn);
        BtnSaveText = FindBtnChildText(layoutRoot, bn);
        bn = "BtnResetAll";
        BtnResetAllBg = FindBtnChildBg(layoutRoot, bn);
        BtnResetAllText = FindBtnChildText(layoutRoot, bn);
        bn = "BtnClearOut";
        BtnClearOutBg = FindBtnChildBg(layoutRoot, bn);
        BtnClearOutText = FindBtnChildText(layoutRoot, bn);

        // ── Header sort button ──
        bn = "BtnSortHeader";
        BtnSortHeaderBg = FindBtnChildBg(layoutRoot, bn);
        BtnSortHeaderText = FindBtnChildText(layoutRoot, bn);

        // ══════════════════════════════════════════════════════
        // Standalone widgets — FindAnyWidget is reliable for
        // these (NOT inside ButtonWidget containers).
        // ══════════════════════════════════════════════════════
        string wn = "";


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
        // B2 (2026-04-26): fresh open clears any in-flight throttle that
        // may have been left set by a previous session that closed mid-RPC.
        m_SaveInFlight = false;
        m_SortInFlight = false;
        m_PreviewInFlight = false;
        m_PreviewInFlightSince = 0.0;
        m_PreviewPending = false;
        m_PreviewDebounce = 0.0;
        // F3-B: Fresh open — no preview data yet
        m_LastMatchedItems = -1;
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

        // D2: client-side power query (panel opens read-only without power).
        // Fail-closed default: stays read-only if the entity cannot be resolved.
        m_IsPowered = false;
        if (g_Game)
        {
            EntityAI sorterEnt = EntityAI.Cast(g_Game.GetObjectByNetworkId(netLow, netHigh));
            LFPG_Sorter sorterDev = LFPG_Sorter.Cast(sorterEnt);
            if (sorterDev)
            {
                m_IsPowered = sorterDev.LFPG_IsPowered();
            }
        }

        // DIAG: Log pairing state and key binding results
        #ifdef LFPG_DEBUG
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
        LFPG_Util.Info(diagBindings);

        if (g_Game)
        {
            EntityAI diagEnt = EntityAI.Cast(g_Game.GetObjectByNetworkId(netLow, netHigh));
            LFPG_Sorter diagSorter = LFPG_Sorter.Cast(diagEnt);
            string diagPower = "[SorterCtrl] InitFromRPC powered=";
            diagPower = diagPower + m_IsPowered.ToString();
            if (diagSorter)
            {
                diagPower = diagPower + " entity=OK";
            }
            else
            {
                diagPower = diagPower + " entity=NULL";
            }
            LFPG_Util.Info(diagPower);
        }
        #endif

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

        if (!m_IsPowered)
        {
            string stNoPower = "NO POWER";
            SetStatus(stNoPower);
        }
        else
        {
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
        ApplyInitialColors();
        ApplyInitialLabels();
        RefreshAll();
    }

    // =========================================================
    // Tint every static button + label on first open
    // =========================================================
    protected void ApplyInitialColors()
    {
        int DIM = LFPG_SorterView_TEST.COL_TEXT_DIM;
        int MID = LFPG_SorterView_TEST.COL_TEXT_MID;
        int GRN = LFPG_SorterView_TEST.COL_GREEN;
        int WHT = LFPG_SorterView_TEST.COL_TEXT;

        // Footer buttons (S2 premixes, layout owns the rest of the chrome)
        TintBg(BtnSaveBg, LFPG_SorterView_TEST.COL_S2_GREENBTN_SEC);
        TintBg(BtnResetAllBg, LFPG_SorterView_TEST.COL_S2_REDBTN_BG);
        // Sprint 4 (2026-04-26): BtnClearOut ghost variant (S2 rulerow premix).
        TintBg(BtnClearOutBg, LFPG_SorterView_TEST.COL_S2_RULEROW_BG);

        // Footer text colors
        SetTxtCol(BtnSaveText, GRN);
        SetTxtCol(BtnResetAllText, WHT);
        // Sprint 4: ghost variant => dim text
        SetTxtCol(BtnClearOutText, DIM);

        // Header sort button (S2 green-header premix)
        TintBg(BtnSortHeaderBg, LFPG_SorterView_TEST.COL_S2_GREENBTN_HDR);
        SetTxtCol(BtnSortHeaderText, GRN);

        // Add buttons (S2 green-panel premix)
        TintBg(BtnPrefixAddBg, LFPG_SorterView_TEST.COL_S2_GREENBTN_PAN);
        TintBg(BtnContainsAddBg, LFPG_SorterView_TEST.COL_S2_GREENBTN_PAN);
        TintBg(BtnSlotAddBg, LFPG_SorterView_TEST.COL_S2_GREENBTN_PAN);

        // Preview toggle button
        TintBg(BtnPreviewBg, LFPG_SorterView_TEST.COL_BG_RULES_PANEL);
        SetTxtCol(BtnPreviewText, MID);

        // Labels
        SetTxtCol(LblCategory, DIM);
        SetTxtCol(LblPrefix, DIM);
        SetTxtCol(LblContains, DIM);
        SetTxtCol(LblSlot, DIM);
        SetTxtCol(LblSlotDash, MID);
        SetTxtCol(LblPreview, DIM);
        SetTxtCol(TagsEmpty, DIM);
        SetTxtCol(PreviewEmpty, DIM);

        // v3: Empty state extras (S2 separator premix)
        SetTxtCol(TagsEmptyIcon, LFPG_SorterView_TEST.COL_S2_SEPARATOR);
        SetTxtCol(TagsEmptyHint, DIM);
        SetTxtCol(PreviewEmptyIcon, LFPG_SorterView_TEST.COL_S2_SEPARATOR);
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
        int dimTxt = LFPG_SorterView_TEST.COL_TEXT_DIM;

        // Footer action buttons
        if (enabled)
        {
            // Sprint 4 (2026-04-26): ghost variant when enabled (was red soft)
            TintBg(BtnClearOutBg, LFPG_SorterView_TEST.COL_S2_RULEROW_BG);
            SetTxtCol(BtnClearOutText, LFPG_SorterView_TEST.COL_TEXT_DIM);
            TintBg(BtnSaveBg, LFPG_SorterView_TEST.COL_S2_GREENBTN_SEC);
            SetTxtCol(BtnSaveText, LFPG_SorterView_TEST.COL_GREEN);
            // Header sort
            TintBg(BtnSortHeaderBg, LFPG_SorterView_TEST.COL_S2_GREENBTN_HDR);
            SetTxtCol(BtnSortHeaderText, LFPG_SorterView_TEST.COL_GREEN);
        }
        else
        {
            TintBg(BtnClearOutBg, dimBg);
            TintBg(BtnSaveBg, dimBg);
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
        LFPG_SorterView_TEST.SetControlsFlag(enabled);
    }

    // =========================================================
    // Status label/dot (reflects pairing + save state)
    // =========================================================
    // S5: Extracted from SetStatus — maps status text to ARGB color
    protected int GetStatusColor(string st)
    {
        string stSaving = "SAVING";
        string stSorting = "SORTING";
        string stError = "ERROR";
        string stNoLink = "NO LINK";
        string stFailed = "FAILED";
        string stNoPower = "NO POWER";
        if (st == stSaving || st == stSorting)
        {
            return LFPG_SorterView_TEST.COL_AMBER;
        }
        if (st == stError)
        {
            return LFPG_SorterView_TEST.COL_RED;
        }
        if (st == stNoLink)
        {
            return LFPG_SorterView_TEST.COL_RED;
        }
        if (st == stFailed)
        {
            return LFPG_SorterView_TEST.COL_RED;
        }
        if (st == stNoPower)
        {
            return LFPG_SorterView_TEST.COL_AMBER;
        }
        return LFPG_SorterView_TEST.COL_GREEN;
    }

    protected void SetStatus(string st)
    {
        m_LastStatus = st;
        int col = GetStatusColor(st);
        if (StatusLabel)
        {
            StatusLabel.SetText(st);
            StatusLabel.SetColor(col);
        }
        if (StatusDot)
        {
            StatusDot.SetColor(col);
        }
    }

    void HandleSaveAck(bool success)
    {
        // B2: clear in-flight throttle on either outcome.
        m_SaveInFlight = false;
        if (success)
        {
            string stSaved = "SAVED";
            SetStatus(stSaved);
        }
        else
        {
            // B1 (2026-04-26): server rejected the save. The most common
            // cause is the linked container being destroyed/unpaired
            // server-side while the panel was open. Tell the user to reopen
            // (which re-runs InitFromRPC with the correct pairing state).
            // TODO: full fix is a server-push RPC on container unlink so the
            // unpaired overlay shows immediately without an action attempt.
            string stErr = "FAILED";
            SetStatus(stErr);
        }
        m_FeedbackTimer = 2.5;
    }

    // v3.2: Server sort result feedback
    void HandleSortAck(bool success, int movedCount)
    {
        // B2: clear in-flight throttle on either outcome.
        m_SortInFlight = false;
        if (success)
        {
            string stSorted = "SORTED: ";
            stSorted = stSorted + movedCount.ToString();
            SetStatus(stSorted);
        }
        else
        {
            // B1 (2026-04-26): see HandleSaveAck note. Most common reason
            // for sort failure is container unlinked server-side.
            string stFail = "FAILED";
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
                if (!m_IsPowered)
                {
                    string stNoPower = "NO POWER";
                    SetStatus(stNoPower);
                }
                else
                {
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
        }

        // Reset confirmation timeout
        if (m_ResetConfirmActive)
        {
            m_ResetTimer = m_ResetTimer - dt;
            if (m_ResetTimer <= 0.0) { CancelResetConfirm(); }
        }

        // ClearOut confirmation timeout (mirrors ResetAll)
        if (m_ClearConfirmActive)
        {
            m_ClearTimer = m_ClearTimer - dt;
            if (m_ClearTimer <= 0.0) { CancelClearConfirm(); }
        }

        // D2 (R21-1): power can change while the panel is open
        m_PowerPollTimer = m_PowerPollTimer - dt;
        if (m_PowerPollTimer <= 0.0)
        {
            m_PowerPollTimer = 0.5;
            RefreshPowerState();
        }

        float previewNow = 0.0;
        if (m_PreviewInFlight && g_Game)
        {
            previewNow = g_Game.GetTickTime();
            if ((previewNow - m_PreviewInFlightSince) > LFPG_SORTER_PREVIEW_INFLIGHT_TIMEOUT_S)
            {
                m_PreviewInFlight = false;
                m_PreviewInFlightSince = 0.0;
            }
        }

        if (m_PreviewPending && !m_PreviewInFlight)
        {
            m_PreviewDebounce = m_PreviewDebounce - dt;
            if (m_PreviewDebounce <= 0.0)
            {
                m_PreviewDebounce = 0.0;
                SendPreviewNow();
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
        // R21-4: cancel BOTH pending confirms (label + color restored)
        CancelResetConfirm();
        CancelClearConfirm();
        // F3-B: Preview is stale for new output
        m_LastMatchedItems = -1;
        RefreshAll();
    }

    // =========================================================
    // View tabs
    // =========================================================
    void TabRules()  { m_ShowRules = true;  RefreshViewTabs(); }
    void TabPreview() { m_ShowRules = false; RefreshViewTabs(); RequestPreview(); }
    // R21-2: re-resolve named button widgets before painting (same pattern as
    // the mutation handlers; Dabs may have rebound them after the last NPC).
    void TogglePreview_TEST() { ReBindButtons(); if (m_ShowRules) { TabPreview(); } else { TabRules(); } }

    // D2: read-only guard (mutating actions need pairing AND power).
    // Navigation (SelectOutput) and preview requests stay allowed.
    protected bool CanEdit() { return m_IsPaired && m_IsPowered; }

    // R21-4: cancel helpers restore label + color so a stale CONFIRM? can
    // never survive an output switch, a rival confirm or a power drop.
    protected void CancelResetConfirm()
    {
        m_ResetConfirmActive = false;
        m_ResetTimer = 0.0;
        string resetLabel = "RESET ALL";
        if (BtnResetAllText) { BtnResetAllText.SetText(resetLabel); }
        TintBg(BtnResetAllBg, LFPG_SorterView_TEST.COL_S2_REDBTN_BG);
    }

    protected void CancelClearConfirm()
    {
        m_ClearConfirmActive = false;
        m_ClearTimer = 0.0;
        string clearLabel = "CLEAR OUT";
        if (BtnClearOutText) { BtnClearOutText.SetText(clearLabel); }
        TintBg(BtnClearOutBg, LFPG_SorterView_TEST.COL_S2_RULEROW_BG);
    }

    // D2 (R21-1): re-poll netsync power so a cut while the panel is open drops
    // to read-only. Fail-closed when the entity cannot be resolved. Keeps the
    // status untouched while a feedback message is showing (TickTimers revert
    // repaints the right idle state afterwards).
    protected void RefreshPowerState()
    {
        bool wasPowered = m_IsPowered;
        m_IsPowered = false;
        if (g_Game)
        {
            EntityAI powEnt = EntityAI.Cast(g_Game.GetObjectByNetworkId(m_SorterNetLow, m_SorterNetHigh));
            LFPG_Sorter powDev = LFPG_Sorter.Cast(powEnt);
            if (powDev)
            {
                m_IsPowered = powDev.LFPG_IsPowered();
            }
        }
        if (m_IsPowered == wasPowered)
            return;
        if (!CanEdit())
        {
            CancelResetConfirm();
            CancelClearConfirm();
        }
        if (m_FeedbackTimer <= 0.0)
        {
            if (!m_IsPowered)
            {
                string stNoPower = "NO POWER";
                SetStatus(stNoPower);
            }
            else
            {
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
        SetControlsEnabled(CanEdit());
    }

    // =========================================================
    // M1: Category toggle by index (replaces CatBtn0..7)
    // =========================================================
    void ToggleCategoryByIdx(int idx)
    {
        if (!CanEdit()) return;
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
        if (!CanEdit()) return;
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
        if (!CanEdit()) return;
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
        LFPG_SorterView_TEST.RefreshHints();
    }

    void BtnContainsAdd()
    {
        if (!CanEdit()) return;
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
        LFPG_SorterView_TEST.RefreshHints();
    }

    void BtnSlotAdd()
    {
        if (!CanEdit()) return;
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
        LFPG_SorterView_TEST.RefreshHints();
    }

    // =========================================================
    // Relay_Commands — catch-all, clear, reset, save, close, sort
    // =========================================================
    void BtnCatchAll()
    {
        if (!CanEdit()) return;
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
        if (!CanEdit()) return;
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg) return;
        // R21-4: arming or executing ClearOut disarms a pending ResetAll
        CancelResetConfirm();
        if (!m_ClearConfirmActive)
        {
            // Two-click confirm (mirrors BtnResetAll)
            m_ClearConfirmActive = true;
            m_ClearTimer = 3.0;
            string confirmLabel = "CONFIRM?";
            if (BtnClearOutText) { BtnClearOutText.SetText(confirmLabel); }
            TintBg(BtnClearOutBg, LFPG_SorterView_TEST.COL_AMBER);
            return;
        }
        m_ClearConfirmActive = false;
        m_ClearTimer = 0.0;
        outCfg.ClearRules();
        string clearLabel = "CLEAR OUT";
        if (BtnClearOutText) { BtnClearOutText.SetText(clearLabel); }
        TintBg(BtnClearOutBg, LFPG_SorterView_TEST.COL_S2_RULEROW_BG);
        // P4: All filter buttons reset + rules cleared
        ReBindButtons();
        RefreshFilterButtons();
        RefreshRulesDisplay();
    }

    void BtnResetAll()
    {
        if (!CanEdit()) return;
        // R21-4: arming or executing ResetAll disarms a pending ClearOut
        CancelClearConfirm();
        if (!m_ResetConfirmActive)
        {
            m_ResetConfirmActive = true;
            m_ResetTimer = 3.0;
            string confirmLabel = "CONFIRM?";
            if (BtnResetAllText) { BtnResetAllText.SetText(confirmLabel); }
            TintBg(BtnResetAllBg, LFPG_SorterView_TEST.COL_AMBER);
            return;
        }
        m_ResetConfirmActive = false;
        m_Config.ResetAll();
        string resetLabel = "RESET ALL";
        if (BtnResetAllText) { BtnResetAllText.SetText(resetLabel); }
        TintBg(BtnResetAllBg, LFPG_SorterView_TEST.COL_S2_REDBTN_BG);
        RefreshAll();
    }

    void BtnSave()
    {
        // S8 fix: guard unpaired — all other action buttons check this
        if (!CanEdit())
            return;
        // B2 (2026-04-26): drop click if a previous Save is still pending.
        // HandleSaveAck (success or failure) clears the flag.
        if (m_SaveInFlight)
            return;
        m_SaveInFlight = true;

        string json = m_Config.ToJSON();
        string saveMsg = "[SorterCtrl] SAVE: ";
        saveMsg = saveMsg + json;
        LFPG_Util.Info(saveMsg);
        string savingLabel = "SAVING";
        SetStatus(savingLabel);
        #ifndef SERVER
        // R4: g_Game guard
        if (!g_Game)
            return;
        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (player)
        {
            ScriptRPC rpc = new ScriptRPC();
            int subId = LFPG_RPC_SubId.SORTER_TEST_CONFIG_SAVE;
            rpc.Write(subId);
            rpc.Write(m_SorterNetLow);
            rpc.Write(m_SorterNetHigh);
            rpc.Write(json);
            rpc.Send(player, LFPG_RPC_CHANNEL, true, null);
        }
        #endif
    }

    // Bug #3: X close button in header
    void BtnCloseX() { LFPG_SorterView_TEST.Close(); }

    // v2.8: Header quick-sort button
    void BtnSortHeader()
    {
        string label = "[SorterCtrl] REQUEST_SORT (header)";
        DoSort(label);
    }

    // N1: Shared sort logic (used by BtnSortHeader)
    protected void DoSort(string logLabel)
    {
        if (!CanEdit()) return;
        // B2 (2026-04-26): drop click if a previous Sort is still pending.
        // HandleSortAck (success or failure) clears the flag.
        if (m_SortInFlight)
            return;
        m_SortInFlight = true;

        LFPG_Util.Info(logLabel);
        string sortingLabel = "SORTING";
        SetStatus(sortingLabel);
        // S6: Timeout timer — SORT_ACK will override with real result
        m_FeedbackTimer = 8.0;
        #ifndef SERVER
        // R4: g_Game guard
        if (!g_Game)
            return;
        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (player)
        {
            ScriptRPC rpc = new ScriptRPC();
            int subId = LFPG_RPC_SubId.SORTER_TEST_REQUEST_SORT;
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
        if (!CanEdit()) return;
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
        bn = "BtnSave";
        BtnSaveBg = FindBtnChildBg(m_LayoutRoot, bn);
        BtnSaveText = FindBtnChildText(m_LayoutRoot, bn);
        bn = "BtnResetAll";
        BtnResetAllBg = FindBtnChildBg(m_LayoutRoot, bn);
        BtnResetAllText = FindBtnChildText(m_LayoutRoot, bn);
        bn = "BtnClearOut";
        BtnClearOutBg = FindBtnChildBg(m_LayoutRoot, bn);
        BtnClearOutText = FindBtnChildText(m_LayoutRoot, bn);
        bn = "BtnSortHeader";
        BtnSortHeaderBg = FindBtnChildBg(m_LayoutRoot, bn);
        BtnSortHeaderText = FindBtnChildText(m_LayoutRoot, bn);
        bn = "BtnPreview";
        BtnPreviewBg = FindBtnChildBg(m_LayoutRoot, bn);
        BtnPreviewText = FindBtnChildText(m_LayoutRoot, bn);
    }

    protected void RefreshAll()
    {
        // Sprint 1 (2026-04-26): keep vertical rail in sync.
        RefreshRail_TEST();
        // Sprint 2 (2026-04-26): keep builder tab + sections in sync.
        RefreshBuilderTab_TEST();
        // Sprint 3 (2026-04-26): keep Active Rules sublabel in sync.
        RefreshRulesHeader_TEST();
        ReBindButtons();
        RefreshViewTabs();
        RefreshFilterButtons();
        RefreshRulesDisplay();
        RefreshHeaderLink();
        // v3: Refresh edit hints
        LFPG_SorterView_TEST.RefreshHints();
        // Apply disabled visual after all refreshes (v2.2)
        SetControlsEnabled(CanEdit());
    }

    // =========================================================
    // P4: Granular refresh helpers — subsets of RefreshAll for
    // handlers that only modify rules (not output/tab context).
    // Order matters: TagsList uses named widgets (safe before NPC),
    // then RuleCount/MatchCount call NotifyPropertyChanged.
    // =========================================================
    protected void RefreshRulesDisplay()
    {
        // F3-B: Rules changed — preview count is stale until server responds
        m_LastMatchedItems = -1;
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

    protected void RefreshViewTabs()
    {
        if (RulesPanel) { RulesPanel.Show(m_ShowRules); }
        if (PreviewPanel) { PreviewPanel.Show(!m_ShowRules); }
        TintBg(BtnPreviewBg, LFPG_SorterView_TEST.COL_BG_RULES_PANEL);
        if (BtnPreviewText)
        {
            if (m_ShowRules)
            {
                string lblPrev = "PREVIEW";
                BtnPreviewText.SetText(lblPrev);
                BtnPreviewText.SetColor(LFPG_SorterView_TEST.COL_TEXT_MID);
            }
            else
            {
                string lblBack = "RULES";
                BtnPreviewText.SetText(lblBack);
                BtnPreviewText.SetColor(LFPG_SorterView_TEST.COL_BLUE);
            }
        }
    }

    protected void RefreshCategoryButtons()
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg) return;
        int ci = 0;
        int gbtn = LFPG_SorterView_TEST.COL_BLUE_BTN;
        int gtxt = LFPG_SorterView_TEST.COL_BLUE;
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
        int bbtn = LFPG_SorterView_TEST.COL_GREEN_BTN;
        int btxt = LFPG_SorterView_TEST.COL_GREEN;
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
        int bgCol = LFPG_SorterView_TEST.COL_BTN;
        int txtCol = LFPG_SorterView_TEST.COL_TEXT_MID;
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
        int bgCol = LFPG_SorterView_TEST.COL_S2_CATCHALL_BG;
        int txtCol = LFPG_SorterView_TEST.COL_AMBER;
        string label = "CATCH-ALL SORTING: OFF";
        if (active)
        {
            bgCol = LFPG_SorterView_TEST.COL_AMBER;
            txtCol = LFPG_SorterView_TEST.COL_BG_DEEP;
            string labelOn = "CATCH-ALL SORTING: ON";
            label = labelOn;
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

        // v4.1: Create fresh TagViews each refresh (no pool reuse).
        // Pool reuse with ObservableCollection causes Dabs MVC to not
        // re-parent recycled ScriptView layout roots to the WrapSpacer
        // after Clear()+Insert(), leaving tags invisible.
        int ri;
        LFPG_SorterTagView_TEST tag = null;
        LFPG_SortFilterRule rule = null;
        string label = "";
        int color = 0;
        int inserted = 0;
        for (ri = 0; ri < ruleCount; ri = ri + 1)
        {
            rule = outCfg.m_Rules[ri];
            if (!rule) continue;
            label = rule.GetDisplayLabel();
            color = GetRuleColor(rule.m_Type);
            tag = new LFPG_SorterTagView_TEST();
            tag.SetData(label, color, GetRuleTypeTag(rule.m_Type), ri, m_SelectedOutput, this);
            TagsList.Insert(tag);
            inserted = inserted + 1;
        }

        // Catch-all tag (always last)
        if (outCfg.m_IsCatchAll)
        {
            string caLabel = "CATCH-ALL";
            string caTag = "*";
            tag = new LFPG_SorterTagView_TEST();
            tag.SetData(caLabel, LFPG_SorterView_TEST.COL_AMBER, caTag, -1, m_SelectedOutput, this);
            TagsList.Insert(tag);
            inserted = inserted + 1;
        }

        // v4.1 DIAG: Log tag creation summary (Debug — fires per click)
        #ifdef LFPG_DEBUG
        string diagTags = "[SorterCtrl] RefreshTagsList output=";
        diagTags = diagTags + m_SelectedOutput.ToString();
        diagTags = diagTags + " rules=";
        diagTags = diagTags + ruleCount.ToString();
        diagTags = diagTags + " inserted=";
        diagTags = diagTags + inserted.ToString();
        LFPG_Util.Debug(diagTags);
        #endif

        bool isEmpty = (ruleCount == 0 && !outCfg.m_IsCatchAll);
        if (TagsEmpty) { TagsEmpty.Show(isEmpty); }
        if (TagsEmptyIcon) { TagsEmptyIcon.Show(isEmpty); }
        if (TagsEmptyHint) { TagsEmptyHint.Show(isEmpty); }
    }

    protected int GetRuleColor(int ruleType)
    {
        if (ruleType == LFPG_SORT_FILTER_CATEGORY) return LFPG_SorterView_TEST.COL_BLUE;
        if (ruleType == LFPG_SORT_FILTER_PREFIX) return LFPG_SorterView_TEST.COL_AMBER;
        if (ruleType == LFPG_SORT_FILTER_CONTAINS) return LFPG_SorterView_TEST.COL_PURPLE;
        if (ruleType == LFPG_SORT_FILTER_SLOT) return LFPG_SorterView_TEST.COL_GREEN;
        return LFPG_SorterView_TEST.COL_TEXT;
    }

    protected string GetRuleTypeTag(int ruleType)
    {
        string tagCat = "CAT";
        string tagPfx = "PFX";
        string tagCon = "CON";
        string tagSlt = "SLT";
        string tagNone = "---";
        if (ruleType == LFPG_SORT_FILTER_CATEGORY) return tagCat;
        if (ruleType == LFPG_SORT_FILTER_PREFIX) return tagPfx;
        if (ruleType == LFPG_SORT_FILTER_CONTAINS) return tagCon;
        if (ruleType == LFPG_SORT_FILTER_SLOT) return tagSlt;
        return tagNone;
    }

    protected void RefreshRuleCount()
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        int count = 0;
        if (outCfg)
        {
            count = outCfg.GetRuleCount();
            // F3-A: Do NOT count catch-all in numerator.
            // Catch-all is a flag, not a user-added rule.
            // With 8 rules + catch-all, display should be "8/8" not "9/8".
        }
        string suffix = "/8";
        RuleCount = count.ToString();
        RuleCount = RuleCount + suffix;
        string propRC = "RuleCount";
        NotifyPropertyChanged(propRC, false);
    }

    protected void RefreshHeaderLink()
    {
        string dest = GetDestName(m_SelectedOutput);
        bool hasDest = (dest != "");
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
    // F3-B: MatchCount shows real matched items from preview.
    // m_LastMatchedItems = -1 means preview not yet fetched.
    // Updated by PopulatePreview when server responds.
    // =========================================================
    protected void RefreshMatchCount()
    {
        if (m_LastMatchedItems < 0)
        {
            string dash = "--";
            MatchCount = dash;
        }
        else
        {
            MatchCount = m_LastMatchedItems.ToString();
            string suffix = " items";
            MatchCount = MatchCount + suffix;
        }
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
        m_PreviewPending = true;
        m_PreviewDebounce = LFPG_SORTER_PREVIEW_DEBOUNCE_S;
        #endif
    }

    protected void SendPreviewNow()
    {
        if (!g_Game)
            return;
        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player)
            return;

        // per-send allocation (not per-frame), mirrors production RequestPreview; reuse of ScriptRPC is not a verified engine API
        ScriptRPC rpc = new ScriptRPC();
        int subId = LFPG_RPC_SubId.SORTER_TEST_PREVIEW_REQUEST;
        rpc.Write(subId);
        rpc.Write(m_SorterNetLow);
        rpc.Write(m_SorterNetHigh);
        rpc.Write(m_SelectedOutput);
        // v4.1: Send current UI config so preview evaluates live rules
        // (not the persisted m_FilterJSON which requires SAVE first)
        string previewJSON = m_Config.ToJSON();
        rpc.Write(previewJSON);

        m_PreviewPending = false;
        m_PreviewInFlight = true;
        m_PreviewInFlightSince = g_Game.GetTickTime();
        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);
    }

    // Called from View.OnPreviewData (static delegate from PlayerRPC)
    // v4.3: slots changed from array<int> to array<string> (formatted "WxH" / "WxH xQ")
    void PopulatePreview(int outputIdx, int totalMatched, array<string> names, array<string> cats, array<string> infos)
    {
        m_PreviewInFlight = false;
        m_PreviewInFlightSince = 0.0;

        // Guard: if user switched output tab while RPC was in flight, ignore
        if (outputIdx != m_SelectedOutput)
            return;

        PreviewItems.Clear();

        int sentCount = names.Count();

        // F4-D: Insertion sort on reusable index array (max 50 items = PREVIEW_CAP).
        // m_SortIdx is member field — avoids new array<int> per call.
        m_SortIdx.Clear();
        int ii = 0;
        for (ii = 0; ii < sentCount; ii = ii + 1)
        {
            m_SortIdx.Insert(ii);
        }
        // Insertion sort: O(n²) worst but fewer swaps than bubble, better cache.
        int iSort = 1;
        int jSort = 0;
        int keyIdx = 0;
        string keyName = "";
        string cmpName = "";
        bool cmpAfter = false;
        while (iSort < sentCount)
        {
            keyIdx = m_SortIdx[iSort];
            keyName = names[keyIdx];
            jSort = iSort - 1;
            cmpAfter = false;
            while (jSort >= 0)
            {
                cmpName = names[m_SortIdx[jSort]];
                cmpAfter = (cmpName > keyName);
                if (!cmpAfter)
                    break;
                int nextJ = jSort + 1;
                m_SortIdx[nextJ] = m_SortIdx[jSort];
                jSort = jSort - 1;
            }
            int insertPos = jSort + 1;
            m_SortIdx[insertPos] = keyIdx;
            iSort = iSort + 1;
        }

        // v4.2: Fresh rows each call (no pool — same fix as tags v4.1).
        // Pool reuse with ObservableCollection causes Dabs MVC to not
        // re-parent recycled ScriptView layout roots to the GridSpacer
        // after Clear()+Insert(), leaving rows invisible.
        int si = 0;
        string itemName = "";
        string itemCat = "";
        string itemInfo = "";
        int sIdx = 0;
        LFPG_SorterPreviewRow_TEST row = null;
        for (si = 0; si < sentCount; si = si + 1)
        {
            sIdx = m_SortIdx[si];
            itemName = names[sIdx];
            itemCat = cats[sIdx];
            itemInfo = infos[sIdx];
            row = new LFPG_SorterPreviewRow_TEST();
            row.SetData(itemName, itemCat, itemInfo);
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

        // F3-B: Store real matched count for MatchCount display
        m_LastMatchedItems = totalMatched;
        RefreshMatchCount();

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
        bg.SetColor(color);
        // Cache in View for hover system (v2.2)
        LFPG_SorterView_TEST.CacheColor(bg, color);
    }

    protected void SetTxtCol(TextWidget txt, int color)
    {
        if (!txt) return;
        txt.SetColor(color);
    }

    // FIX 2: Release tag/preview views on close to break circular refs.
    // Called from View.DoClose. Safe: destructor of TagView already
    // nulls m_OwnerController, so Clear triggers clean teardown.
    // v4.2: m_PreviewPool removed (same Dabs re-parenting fix as tags).
    void ClearCollections()
    {
        m_PreviewInFlight = false;
        m_PreviewInFlightSince = 0.0;
        m_PreviewPending = false;
        m_PreviewDebounce = 0.0;

        if (TagsList)
        {
            TagsList.Clear();
        }
        if (PreviewItems)
        {
            PreviewItems.Clear();
        }
    }

    // ============================================================
    // Sprint 1 (2026-04-26): vertical rail row state refresh.
    // Called from RefreshAll to update each row's active-state visuals
    // and content. Widgets resolved via m_LayoutRoot.FindAnyWidget()
    // — Dabs ViewController exposes m_LayoutRoot (Widget) but not the
    // ScriptView instance directly, so we query widgets by name.
    // ============================================================
    // IS3 (Sprint 4.5, 2026-04-26): rail refresh uses cached widgets.
    // 30 FindAnyWidget calls per refresh -> 0 (cache warmed in
    // EnsureV4Cache_TEST on first EnsureBindings).
    void RefreshRail_TEST()
    {
        EnsureV4Cache_TEST();
        if (!m_V4CacheBuilt_TEST) return;

        int activeIdx = m_SelectedOutput;
        int activeBg = LFPG_SorterView_TEST.COL_BG_ELEVATED;
        int activeIndicator = LFPG_SorterView_TEST.COL_GREEN;
        int activeLabelCol = LFPG_SorterView_TEST.COL_GREEN;
        int dimBg = LFPG_SorterView_TEST.COL_S2_BG_SECTION;
        int dimIndicator = 0x00000000;
        int dimLabelCol = LFPG_SorterView_TEST.COL_TEXT;

        int ri = 0;
        bool isActive = false;
        ImageWidget bg = null;
        ImageWidget indicator = null;
        TextWidget label = null;
        TextWidget count = null;
        TextWidget container = null;
        string ruleSummary;
        string contName;
        int ruleCnt;
        LFPG_SortOutputConfig oc = null;
        string ctxt;
        string emptyTxt;

        for (ri = 0; ri < 6; ri = ri + 1)
        {
            isActive = (ri == activeIdx);
            bg        = m_RailRowBgs_TEST[ri];
            indicator = m_RailRowIndicators_TEST[ri];
            label     = m_RailRowLabels_TEST[ri];
            count     = m_RailRowCounts_TEST[ri];
            container = m_RailRowContainers_TEST[ri];

            if (bg)
            {
                if (isActive)
                {
                    LFPG_SorterView_TEST.CacheColor(bg, activeBg);
                    bg.SetColor(activeBg);
                }
                else
                {
                    LFPG_SorterView_TEST.CacheColor(bg, dimBg);
                    bg.SetColor(dimBg);
                }
            }
            if (indicator)
            {
                if (isActive)
                {
                    LFPG_SorterView_TEST.CacheColor(indicator, activeIndicator);
                    indicator.SetColor(activeIndicator);
                }
                else
                {
                    LFPG_SorterView_TEST.CacheColor(indicator, dimIndicator);
                    indicator.SetColor(dimIndicator);
                }
            }
            if (label)
            {
                if (isActive) { label.SetColor(activeLabelCol); }
                else { label.SetColor(dimLabelCol); }
            }
            if (container)
            {
                contName = "";
                if (ri < m_Dests.Count())
                {
                    contName = m_Dests.Get(ri);
                }
                if (contName != "")
                {
                    ctxt = "-> ";
                    ctxt = ctxt + contName;
                    container.SetText(ctxt);
                }
                else
                {
                    emptyTxt = "-> unlinked";
                    container.SetText(emptyTxt);
                }
                container.SetColor(LFPG_SorterView_TEST.COL_TEXT_DIM);
            }
            if (count)
            {
                ruleCnt = 0;
                if (m_Config)
                {
                    oc = m_Config.GetOutput(ri);
                    if (oc && oc.m_Rules)
                    {
                        ruleCnt = oc.m_Rules.Count();
                    }
                }
                ruleSummary = ruleCnt.ToString();
                ruleSummary = ruleSummary + " rules";
                count.SetText(ruleSummary);
                if (isActive) { count.SetColor(LFPG_SorterView_TEST.COL_TEXT); }
                else { count.SetColor(LFPG_SorterView_TEST.COL_TEXT_DIM); }
            }
        }
    }

    // ============================================================
    // Sprint 2 (2026-04-26): builder tab swap.
    // Hides 3 of 4 filter sections so only the active one is visible.
    // Tints active tab's underline + label per design colour token.
    // ============================================================
    void SelectBuilderTab_TEST(int idx)
    {
        if (idx < 0) idx = 0;
        if (idx > 3) idx = 3;
        m_ActiveBuilderTab_TEST = idx;
        RefreshBuilderTab_TEST();
    }

    // IS1+IS3 (Sprint 4.5, 2026-04-26): Refresh uses cached widget arrays.
    // Per-call cost was ~50 FindAnyWidget + 4 array allocs; now 0 of either.
    void RefreshBuilderTab_TEST()
    {
        EnsureV4Cache_TEST();
        if (!m_V4CacheBuilt_TEST) return;
        int activeIdx = m_ActiveBuilderTab_TEST;

        // Show only the active section root (order: CAT/PFX/CON/SLT)
        int secIdx = 0;
        for (secIdx = 0; secIdx < m_SectionRoots_TEST.Count(); secIdx = secIdx + 1)
        {
            Widget secRoot = m_SectionRoots_TEST.Get(secIdx);
            if (secRoot)
            {
                secRoot.Show(secIdx == activeIdx);
            }
        }

        // Tab visuals — cached underlines + texts (4 each)
        int colCat = LFPG_SorterView_TEST.COL_BLUE;
        int colPfx = LFPG_SorterView_TEST.COL_AMBER;
        int colCon = LFPG_SorterView_TEST.COL_PURPLE;
        int colSlt = LFPG_SorterView_TEST.COL_GREEN;
        int colDim = LFPG_SorterView_TEST.COL_TEXT_DIM;
        int colTransparent = 0x00000000;
        int colActiveLabel = LFPG_SorterView_TEST.COL_TEXT;

        // Hoisted locals
        ImageWidget und = null;
        TextWidget txt = null;
        int ti = 0;
        int colActive;
        bool tabIsActive;

        for (ti = 0; ti < 4; ti = ti + 1)
        {
            tabIsActive = (ti == activeIdx);
            und = m_BuilderTabUnderlines_TEST[ti];
            txt = m_BuilderTabTexts_TEST[ti];

            // pick per-tab color
            if      (ti == 0) { colActive = colCat; }
            else if (ti == 1) { colActive = colPfx; }
            else if (ti == 2) { colActive = colCon; }
            else              { colActive = colSlt; }

            if (und)
            {
                if (tabIsActive) { und.SetColor(colActive); }
                else             { und.SetColor(colTransparent); }
            }
            if (txt)
            {
                if (tabIsActive) { txt.SetColor(colActiveLabel); }
                else             { txt.SetColor(colDim); }
            }
        }
    }

    // ============================================================
    // Sprint 3 (2026-04-26) + IS3 (Sprint 4.5): Active Rules header.
    // Cached sublabel widget; no per-refresh FindAnyWidget.
    // ============================================================
    void RefreshRulesHeader_TEST()
    {
        EnsureV4Cache_TEST();
        if (!m_V4CacheBuilt_TEST) return;

        int activeIdx = m_SelectedOutput;
        string contName = "";
        if (activeIdx >= 0 && activeIdx < m_Dests.Count())
        {
            contName = m_Dests.Get(activeIdx);
        }

        int displayIdx = activeIdx + 1;

        // Sublabel: "ON OUT N - <CONTAINER>" / "ON OUT N - UNLINKED"
        if (m_RulesSublabel_TEST)
        {
            string txt = "ON OUT ";
            txt = txt + displayIdx.ToString();
            if (contName != "")
            {
                string upperCont = contName;
                upperCont.ToUpper();
                txt = txt + " - ";
                txt = txt + upperCont;
            }
            else
            {
                txt = txt + " - UNLINKED";
            }
            m_RulesSublabel_TEST.SetText(txt);
        }

        // S2 ctx label: "(OUT N - <name>)" / "(OUT N - unlinked)"
        if (m_BuilderCtxLabel_TEST)
        {
            string ctx = "(OUT ";
            ctx = ctx + displayIdx.ToString();
            ctx = ctx + " - ";
            if (contName != "")
            {
                ctx = ctx + contName;
            }
            else
            {
                string unlinked = "unlinked";
                ctx = ctx + unlinked;
            }
            ctx = ctx + ")";
            m_BuilderCtxLabel_TEST.SetText(ctx);
        }
    }

    // ============================================================
    // IS3 (Sprint 4.5, 2026-04-26): one-shot V4 widget cache.
    // Resolves all V4-specific widgets via FindAnyWidget once and
    // stores them in m_*_TEST fields. Subsequent refreshes use the
    // cached refs directly. Idempotent: re-entry early-exits via
    // m_V4CacheBuilt_TEST.
    // ============================================================
    void EnsureV4Cache_TEST()
    {
        if (m_V4CacheBuilt_TEST) return;
        if (!m_LayoutRoot) return;

        int i = 0;
        string nm;
        string suffix;

        // ---- 6 rail rows × 5 widgets each ----
        m_RailRowBgs_TEST.Clear();
        m_RailRowIndicators_TEST.Clear();
        m_RailRowLabels_TEST.Clear();
        m_RailRowCounts_TEST.Clear();
        m_RailRowContainers_TEST.Clear();
        for (i = 0; i < 6; i = i + 1)
        {
            suffix = i.ToString();

            nm = "OutputRow"; nm = nm + suffix; nm = nm + "Bg";
            m_RailRowBgs_TEST.Insert(ImageWidget.Cast(m_LayoutRoot.FindAnyWidget(nm)));

            nm = "OutputRow"; nm = nm + suffix; nm = nm + "Indicator";
            m_RailRowIndicators_TEST.Insert(ImageWidget.Cast(m_LayoutRoot.FindAnyWidget(nm)));

            nm = "OutputRow"; nm = nm + suffix; nm = nm + "Label";
            m_RailRowLabels_TEST.Insert(TextWidget.Cast(m_LayoutRoot.FindAnyWidget(nm)));

            nm = "OutputRow"; nm = nm + suffix; nm = nm + "Count";
            m_RailRowCounts_TEST.Insert(TextWidget.Cast(m_LayoutRoot.FindAnyWidget(nm)));

            nm = "OutputRow"; nm = nm + suffix; nm = nm + "Container";
            m_RailRowContainers_TEST.Insert(TextWidget.Cast(m_LayoutRoot.FindAnyWidget(nm)));
        }

        // ---- 4 builder tab underlines + texts ----
        m_BuilderTabUnderlines_TEST.Clear();
        m_BuilderTabTexts_TEST.Clear();
        nm = "BuilderTabCategoryUnderline";
        m_BuilderTabUnderlines_TEST.Insert(ImageWidget.Cast(m_LayoutRoot.FindAnyWidget(nm)));
        nm = "BuilderTabPrefixUnderline";
        m_BuilderTabUnderlines_TEST.Insert(ImageWidget.Cast(m_LayoutRoot.FindAnyWidget(nm)));
        nm = "BuilderTabContainsUnderline";
        m_BuilderTabUnderlines_TEST.Insert(ImageWidget.Cast(m_LayoutRoot.FindAnyWidget(nm)));
        nm = "BuilderTabSlotUnderline";
        m_BuilderTabUnderlines_TEST.Insert(ImageWidget.Cast(m_LayoutRoot.FindAnyWidget(nm)));
        nm = "BuilderTabCategoryText";
        m_BuilderTabTexts_TEST.Insert(TextWidget.Cast(m_LayoutRoot.FindAnyWidget(nm)));
        nm = "BuilderTabPrefixText";
        m_BuilderTabTexts_TEST.Insert(TextWidget.Cast(m_LayoutRoot.FindAnyWidget(nm)));
        nm = "BuilderTabContainsText";
        m_BuilderTabTexts_TEST.Insert(TextWidget.Cast(m_LayoutRoot.FindAnyWidget(nm)));
        nm = "BuilderTabSlotText";
        m_BuilderTabTexts_TEST.Insert(TextWidget.Cast(m_LayoutRoot.FindAnyWidget(nm)));

        // ---- 4 section roots (CAT/PFX/CON/SLT) ----
        m_SectionRoots_TEST.Clear();
        nm = "SectionCategory";
        m_SectionRoots_TEST.Insert(m_LayoutRoot.FindAnyWidget(nm));
        nm = "SectionPrefix";
        m_SectionRoots_TEST.Insert(m_LayoutRoot.FindAnyWidget(nm));
        nm = "SectionContains";
        m_SectionRoots_TEST.Insert(m_LayoutRoot.FindAnyWidget(nm));
        nm = "SectionSlot";
        m_SectionRoots_TEST.Insert(m_LayoutRoot.FindAnyWidget(nm));

        // ---- single widgets ----
        // Enforce rule: assign string to local before passing as param.
        nm = "RulesSublabel";
        m_RulesSublabel_TEST = TextWidget.Cast(m_LayoutRoot.FindAnyWidget(nm));
        nm = "BuilderCtxLabel";
        m_BuilderCtxLabel_TEST = TextWidget.Cast(m_LayoutRoot.FindAnyWidget(nm));

        m_V4CacheBuilt_TEST = true;
    }

    bool McpCanEdit()
    {
        return CanEdit();
    }

    int McpCategoryCount()
    {
        if (!m_CatValues)
        {
            return 0;
        }
        return m_CatValues.Count();
    }

    void McpCollectState(out bool paired, out bool powered, out string status, out bool catchAll, out int ruleCount)
    {
        paired = m_IsPaired;
        powered = m_IsPowered;
        status = m_LastStatus;
        catchAll = false;
        ruleCount = 0;
        if (!m_Config)
        {
            return;
        }
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg)
        {
            return;
        }
        catchAll = outCfg.m_IsCatchAll;
        ruleCount = outCfg.GetRuleCount();
    }

};
#endif
