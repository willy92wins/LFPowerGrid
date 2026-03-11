// =========================================================
// LF_PowerGrid — Sorter Controller (Dabs MVC, v2.2 Polish)
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
    protected float m_SaveFeedbackTimer;

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

    // ── Sort feedback timer (v2.2) ──
    protected float m_SortFeedbackTimer;

    static const string PROC = "#(argb,8,8,3)color(1,1,1,1,CO)";

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
        m_SaveFeedbackTimer = 0.0;
        m_SorterNetLow = 0;
        m_SorterNetHigh = 0;
        m_IsPaired = false;
        m_ContainerDisplayName = "";
        m_SortFeedbackTimer = 0.0;
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
    void InitFromRPC(string configJSON, string containerName, string d0, string d1, string d2, string d3, string d4, string d5, int netLow, int netHigh)
    {
        m_SorterNetLow = netLow;
        m_SorterNetHigh = netHigh;
        m_SelectedOutput = 0;
        m_ShowRules = true;
        m_ResetConfirmActive = false;
        m_SaveFeedbackTimer = 0.0;
        m_SortFeedbackTimer = 0.0;
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

        HeaderTitle = "SORTER";
        NotifyPropertyChanged("HeaderTitle", false);

        if (m_IsPaired)
        {
            SetStatus("ONLINE");
        }
        else
        {
            SetStatus("NO LINK");
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

        SetBtnLabel(SlotPre0Text, "Tiny");
        SetBtnLabel(SlotPre1Text, "Small");
        SetBtnLabel(SlotPre2Text, "Med");
        SetBtnLabel(SlotPre3Text, "Large");
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
            SetTxtCol(BtnSortText, LFPG_SorterView.COL_TEXT);
            SetTxtCol(BtnClearOutText, LFPG_SorterView.COL_TEXT_MID);
        }
        else
        {
            TintBg(BtnSortBg, dimBg);
            TintBg(BtnClearOutBg, dimBg);
            SetTxtCol(BtnSortText, dimTxt);
            SetTxtCol(BtnClearOutText, dimTxt);

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
    protected void SetStatus(string st)
    {
        if (StatusLabel)
        {
            StatusLabel.SetText(st);
            int txtCol = LFPG_SorterView.COL_GREEN;
            if (st == "SAVING..." || st == "SORTING...")
            {
                txtCol = LFPG_SorterView.COL_AMBER;
            }
            else if (st == "ERROR")
            {
                txtCol = LFPG_SorterView.COL_RED;
            }
            else if (st == "NO LINK")
            {
                txtCol = LFPG_SorterView.COL_RED;
            }
            StatusLabel.SetColor(txtCol);
        }

        if (StatusDot)
        {
            StatusDot.LoadImageFile(0, PROC);
            int dotCol = LFPG_SorterView.COL_GREEN;
            if (st == "SAVING..." || st == "SORTING...")
            {
                dotCol = LFPG_SorterView.COL_AMBER;
            }
            else if (st == "ERROR")
            {
                dotCol = LFPG_SorterView.COL_RED;
            }
            else if (st == "NO LINK")
            {
                dotCol = LFPG_SorterView.COL_RED;
            }
            StatusDot.SetColor(dotCol);
        }
    }

    void HandleSaveAck(bool success)
    {
        if (success)
        {
            SetStatus("SAVED");
        }
        else
        {
            SetStatus("ERROR");
        }
        m_SaveFeedbackTimer = 2.5;
    }

    // =========================================================
    // Timer tick (called from View.Update)
    // =========================================================
    void TickTimers(float dt)
    {
        // Save feedback revert
        if (m_SaveFeedbackTimer > 0.0)
        {
            m_SaveFeedbackTimer = m_SaveFeedbackTimer - dt;
            if (m_SaveFeedbackTimer <= 0.0)
            {
                m_SaveFeedbackTimer = 0.0;
                if (m_IsPaired)
                {
                    SetStatus("ONLINE");
                }
                else
                {
                    SetStatus("NO LINK");
                }
            }
        }

        // Sort feedback revert (v2.2)
        if (m_SortFeedbackTimer > 0.0)
        {
            m_SortFeedbackTimer = m_SortFeedbackTimer - dt;
            if (m_SortFeedbackTimer <= 0.0)
            {
                m_SortFeedbackTimer = 0.0;
                if (m_IsPaired)
                {
                    SetStatus("ONLINE");
                }
                else
                {
                    SetStatus("NO LINK");
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
                if (BtnResetAllText) { BtnResetAllText.SetText("Reset All"); }
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
        NotifyPropertyChanged("EditPrefix", false);
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
        NotifyPropertyChanged("EditContains", false);
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
        string slotValue = minVal.ToString() + "-" + maxVal.ToString();
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg) return;
        outCfg.AddRule(LFPG_SORT_FILTER_SLOT, slotValue);
        EditSlotMin = "";
        EditSlotMax = "";
        NotifyPropertyChanged("EditSlotMin", false);
        NotifyPropertyChanged("EditSlotMax", false);
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
        if (!m_ResetConfirmActive)
        {
            m_ResetConfirmActive = true;
            m_ResetTimer = 3.0;
            if (BtnResetAllText) { BtnResetAllText.SetText("Confirm?"); }
            TintBg(BtnResetAllBg, LFPG_SorterView.COL_AMBER);
            LFPG_SorterView.PlayUIClick();
            return;
        }
        m_ResetConfirmActive = false;
        m_Config.ResetAll();
        if (BtnResetAllText) { BtnResetAllText.SetText("Reset All"); }
        TintBg(BtnResetAllBg, LFPG_SorterView.COL_RED_BTN);
        LFPG_SorterView.PlayUIAction();
        RefreshAll();
    }

    void BtnSave()
    {
        string json = m_Config.ToJSON();
        LFPG_Util.Info("[SorterCtrl] SAVE: " + json);
        SetStatus("SAVING...");
        LFPG_SorterView.PlayUIAction();
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

    void BtnSort()
    {
        if (!m_IsPaired) return;
        LFPG_Util.Info("[SorterCtrl] REQUEST_SORT");
        // Sort feedback (v2.2) — immediate client-side status
        SetStatus("SORTING...");
        m_SortFeedbackTimer = 3.0;
        LFPG_SorterView.PlayUIAction();
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

    void BtnClose() { LFPG_SorterView.PlayUIClick(); LFPG_SorterView.Close(); }

    // Bug #3: X close button in header
    void BtnCloseX() { LFPG_SorterView.PlayUIClick(); LFPG_SorterView.Close(); }

    // =========================================================
    // Tag removal (called from tag chip via direct ref)
    // =========================================================
    void OnRemoveTag(int outputIdx, int ruleIdx)
    {
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
        RefreshDestIndicator();
        // Apply disabled visual after all refreshes (v2.2)
        SetControlsEnabled(m_IsPaired);
    }

    protected void RefreshOutputTabs()
    {
        int i;
        for (i = 0; i < 6; i = i + 1)
        {
            bool isSel = (i == m_SelectedOutput);
            int bgCol = LFPG_SorterView.COL_BTN;
            int txtCol = LFPG_SorterView.COL_TEXT_MID;
            if (isSel) { bgCol = LFPG_SorterView.COL_BG_ELEVATED; txtCol = LFPG_SorterView.COL_GREEN; }
            int num = i + 1;
            string label = "0" + num.ToString();
            if (num >= 10) { label = num.ToString(); }
            TintBg(GetTabBg(i), bgCol);
            TextWidget tt = GetTabText(i);
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
            ref LFPG_SortFilterRule rule = outCfg.m_Rules[ri];
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
            caTag.SetData("* CATCH-ALL", LFPG_SorterView.COL_AMBER, -1, m_SelectedOutput, this);
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
        RuleCount = count.ToString() + "/8";
        NotifyPropertyChanged("RuleCount", false);
    }

    protected void RefreshDestIndicator()
    {
        string dest = GetDestName(m_SelectedOutput);
        bool hasDest = (dest != "");
        if (DestIndicator) { DestIndicator.Show(hasDest); }
        if (hasDest)
        {
            DestName = dest;
            NotifyPropertyChanged("DestName", false);
            HeaderTitle = "SORTER  " + dest;
        }
        else
        {
            HeaderTitle = "SORTER";
        }
        NotifyPropertyChanged("HeaderTitle", false);
    }

    protected string GetDestName(int idx)
    {
        if (idx == 0) return m_Dest0; if (idx == 1) return m_Dest1;
        if (idx == 2) return m_Dest2; if (idx == 3) return m_Dest3;
        if (idx == 4) return m_Dest4; if (idx == 5) return m_Dest5;
        return "";
    }

    protected void TintBg(ImageWidget bg, int color)
    {
        if (!bg) return;
        bg.LoadImageFile(0, PROC);
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
