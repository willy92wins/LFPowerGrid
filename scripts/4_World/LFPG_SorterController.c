// =========================================================
// LF_PowerGrid — Sorter Controller (Dabs MVC, v2.0)
//
// ViewController for the Sorter UI. Handles:
//   - Output tab selection (6 outputs)
//   - Category / Slot / Prefix / Contains filter toggles
//   - Tag chip ObservableCollection (removable chips)
//   - Preview ObservableCollection (matched items)
//   - View switching (RULES / PREVIEW)
//   - Save / Sort / Reset / Close RPCs
//   - Status feedback with timer
//
// Bound properties auto-sync to layout widgets by name.
// Button Relay_Commands auto-route to methods by name.
//
// Enforce Script: no ternaries, no ++/--, no foreach.
// =========================================================

class LFPG_SorterController extends ViewController
{
    // ── Bound text properties ──
    string HeaderTitle;
    string StatusText;
    string RuleCount;
    string MatchCount;
    string PreviewCount;
    string DestName;

    // ── Bound EditBox properties (two-way via ViewBinding) ──
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
    protected bool m_ShowRules;         // true=RULES, false=PREVIEW
    protected bool m_ResetConfirmActive;
    protected float m_ResetTimer;
    protected float m_SaveFeedbackTimer;

    // ── RPC identity ──
    protected int m_SorterNetLow;
    protected int m_SorterNetHigh;

    // ── Dest names (6 slots, no string arrays in Enforce) ──
    protected string m_Dest0;
    protected string m_Dest1;
    protected string m_Dest2;
    protected string m_Dest3;
    protected string m_Dest4;
    protected string m_Dest5;

    // ── Category labels / values (same as SorterData) ──
    protected string m_CatLabel0; protected string m_CatValue0;
    protected string m_CatLabel1; protected string m_CatValue1;
    protected string m_CatLabel2; protected string m_CatValue2;
    protected string m_CatLabel3; protected string m_CatValue3;
    protected string m_CatLabel4; protected string m_CatValue4;
    protected string m_CatLabel5; protected string m_CatValue5;
    protected string m_CatLabel6; protected string m_CatValue6;
    protected string m_CatLabel7; protected string m_CatValue7;

    // ── Widget refs for manual styling (auto-assigned by name) ──
    // Output tab backgrounds
    ImageWidget TabOut0Bg; ImageWidget TabOut1Bg; ImageWidget TabOut2Bg;
    ImageWidget TabOut3Bg; ImageWidget TabOut4Bg; ImageWidget TabOut5Bg;
    TextWidget TabOut0Text; TextWidget TabOut1Text; TextWidget TabOut2Text;
    TextWidget TabOut3Text; TextWidget TabOut4Text; TextWidget TabOut5Text;
    // View tab backgrounds
    ImageWidget TabRulesBg; ImageWidget TabPreviewBg;
    TextWidget TabRulesText; TextWidget TabPreviewText;
    // Category button backgrounds
    ImageWidget CatBtn0Bg; ImageWidget CatBtn1Bg; ImageWidget CatBtn2Bg; ImageWidget CatBtn3Bg;
    ImageWidget CatBtn4Bg; ImageWidget CatBtn5Bg; ImageWidget CatBtn6Bg; ImageWidget CatBtn7Bg;
    TextWidget CatBtn0Text; TextWidget CatBtn1Text; TextWidget CatBtn2Text; TextWidget CatBtn3Text;
    TextWidget CatBtn4Text; TextWidget CatBtn5Text; TextWidget CatBtn6Text; TextWidget CatBtn7Text;
    // Slot preset backgrounds
    ImageWidget SlotPre0Bg; ImageWidget SlotPre1Bg; ImageWidget SlotPre2Bg; ImageWidget SlotPre3Bg;
    TextWidget SlotPre0Text; TextWidget SlotPre1Text; TextWidget SlotPre2Text; TextWidget SlotPre3Text;
    // Catch-all
    ImageWidget BtnCatchAllBg; TextWidget BtnCatchAllText;
    // Footer
    ImageWidget BtnSortBg; ImageWidget BtnSaveBg; ImageWidget BtnResetAllBg;
    ImageWidget BtnClearOutBg; ImageWidget BtnCloseBg;
    TextWidget BtnResetAllText;
    // Panels
    Widget RulesPanel; Widget PreviewPanel;
    Widget DestIndicator;
    TextWidget TagsEmpty; TextWidget PreviewEmpty;
    ImageWidget StatusDot;

    // ── Procedural texture ──
    static const string PROC = "#(argb,8,8,3)color(1,1,1,1,CO)";

    // =========================================================
    // Constructor
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

        // Category labels + values
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
    // Init from RPC data (called by View.DoOpen)
    // =========================================================
    void InitFromRPC(string configJSON, string containerName, string d0, string d1, string d2, string d3, string d4, string d5, int netLow, int netHigh)
    {
        m_SorterNetLow = netLow;
        m_SorterNetHigh = netHigh;
        m_SelectedOutput = 0;
        m_ShowRules = true;
        m_ResetConfirmActive = false;
        m_SaveFeedbackTimer = 0.0;

        m_Dest0 = d0; m_Dest1 = d1; m_Dest2 = d2;
        m_Dest3 = d3; m_Dest4 = d4; m_Dest5 = d5;

        if (configJSON != "")
        {
            m_Config.FromJSON(configJSON);
        }
        else
        {
            m_Config.ResetAll();
        }

        // Set header
        HeaderTitle = "SORTER";
        NotifyPropertyChanged("HeaderTitle", false);

        SetStatus("online");
        ApplyInitialButtonLabels();
        RefreshAll();
    }

    // =========================================================
    // Status
    // =========================================================
    protected void SetStatus(string st)
    {
        StatusText = st;
        NotifyPropertyChanged("StatusText", false);

        if (StatusDot)
        {
            StatusDot.LoadImageFile(0, PROC);
            int dotColor = LFPG_SorterView.COL_GREEN;
            if (st == "saved")
            {
                dotColor = LFPG_SorterView.COL_GREEN;
            }
            else if (st == "saving...")
            {
                dotColor = LFPG_SorterView.COL_AMBER;
            }
            else if (st == "error")
            {
                dotColor = LFPG_SorterView.COL_RED;
            }
            StatusDot.SetColor(dotColor);
        }

        if (StatusText)
        {
            // Intentional: StatusText is the TextWidget auto-assigned by name
            // Color matches the status dot
        }
    }

    void HandleSaveAck(bool success)
    {
        if (success)
        {
            SetStatus("saved");
        }
        else
        {
            SetStatus("error");
        }
        m_SaveFeedbackTimer = 2.5;
    }

    // =========================================================
    // Initial button labels (category, slot, tab texts)
    // =========================================================
    protected void ApplyInitialButtonLabels()
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
        if (txt)
        {
            txt.SetText(label);
        }
    }

    // =========================================================
    // Relay_Command handlers — output tabs
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
        RefreshAll();
    }

    // =========================================================
    // Relay_Command handlers — view tabs
    // =========================================================
    void TabRules()
    {
        m_ShowRules = true;
        RefreshViewTabs();
    }

    void TabPreview()
    {
        m_ShowRules = false;
        RefreshViewTabs();
    }

    // =========================================================
    // Relay_Command handlers — category toggles
    // =========================================================
    void CatBtn0() { ToggleCategory(m_CatValue0); }
    void CatBtn1() { ToggleCategory(m_CatValue1); }
    void CatBtn2() { ToggleCategory(m_CatValue2); }
    void CatBtn3() { ToggleCategory(m_CatValue3); }
    void CatBtn4() { ToggleCategory(m_CatValue4); }
    void CatBtn5() { ToggleCategory(m_CatValue5); }
    void CatBtn6() { ToggleCategory(m_CatValue6); }
    void CatBtn7() { ToggleCategory(m_CatValue7); }

    protected void ToggleCategory(string catValue)
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg)
            return;

        bool hasIt = outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, catValue);
        if (hasIt)
        {
            RemoveRuleByValue(outCfg, LFPG_SORT_FILTER_CATEGORY, catValue);
        }
        else
        {
            outCfg.AddRule(LFPG_SORT_FILTER_CATEGORY, catValue);
        }
        RefreshAll();
    }

    // =========================================================
    // Relay_Command handlers — slot preset toggles
    // =========================================================
    void SlotPre0() { ToggleSlot(LFPG_SORT_SLOT_TINY); }
    void SlotPre1() { ToggleSlot(LFPG_SORT_SLOT_SMALL); }
    void SlotPre2() { ToggleSlot(LFPG_SORT_SLOT_MEDIUM); }
    void SlotPre3() { ToggleSlot(LFPG_SORT_SLOT_LARGE); }

    protected void ToggleSlot(string slotValue)
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg)
            return;

        bool hasIt = outCfg.HasRule(LFPG_SORT_FILTER_SLOT, slotValue);
        if (hasIt)
        {
            RemoveRuleByValue(outCfg, LFPG_SORT_FILTER_SLOT, slotValue);
        }
        else
        {
            outCfg.AddRule(LFPG_SORT_FILTER_SLOT, slotValue);
        }
        RefreshAll();
    }

    // =========================================================
    // Relay_Command handlers — add buttons
    // =========================================================
    void BtnPrefixAdd()
    {
        if (EditPrefix == "")
            return;
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg)
            return;
        outCfg.AddRule(LFPG_SORT_FILTER_PREFIX, EditPrefix);
        EditPrefix = "";
        NotifyPropertyChanged("EditPrefix", false);
        RefreshAll();
    }

    void BtnContainsAdd()
    {
        if (EditContains == "")
            return;
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg)
            return;
        outCfg.AddRule(LFPG_SORT_FILTER_CONTAINS, EditContains);
        EditContains = "";
        NotifyPropertyChanged("EditContains", false);
        RefreshAll();
    }

    void BtnSlotAdd()
    {
        if (EditSlotMin == "" || EditSlotMax == "")
            return;
        int minVal = EditSlotMin.ToInt();
        int maxVal = EditSlotMax.ToInt();
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
        EditSlotMin = "";
        EditSlotMax = "";
        NotifyPropertyChanged("EditSlotMin", false);
        NotifyPropertyChanged("EditSlotMax", false);
        RefreshAll();
    }

    // =========================================================
    // Relay_Command handlers — catch-all, clear, reset, save, close, sort
    // =========================================================
    void BtnCatchAll()
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
        RefreshAll();
    }

    void BtnClearOut()
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg)
            return;
        outCfg.ClearRules();
        RefreshAll();
    }

    void BtnResetAll()
    {
        if (!m_ResetConfirmActive)
        {
            m_ResetConfirmActive = true;
            m_ResetTimer = 3.0;
            if (BtnResetAllText)
            {
                BtnResetAllText.SetText("Confirm?");
            }
            TintBtnBg(BtnResetAllBg, LFPG_SorterView.COL_AMBER);
            return;
        }

        // Second click — actually reset
        m_ResetConfirmActive = false;
        m_Config.ResetAll();
        if (BtnResetAllText)
        {
            BtnResetAllText.SetText("Reset All");
        }
        TintBtnBg(BtnResetAllBg, LFPG_SorterView.COL_RED_BTN);
        RefreshAll();
    }

    void BtnSave()
    {
        string json = m_Config.ToJSON();
        LFPG_Util.Info("[SorterController] SAVE: " + json);
        SetStatus("saving...");

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
        LFPG_Util.Info("[SorterController] REQUEST_SORT");

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

    void BtnClose()
    {
        LFPG_SorterView.Close();
    }

    // =========================================================
    // Tag removal (called from LFPG_SorterTagView.BtnRemove)
    // =========================================================
    void OnRemoveTag(int outputIdx, int ruleIdx)
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(outputIdx);
        if (!outCfg)
            return;

        // Check if it is a catch-all removal (ruleIdx == -1 convention)
        if (ruleIdx < 0)
        {
            outCfg.m_IsCatchAll = false;
        }
        else
        {
            outCfg.RemoveRuleAt(ruleIdx);
        }
        RefreshAll();
    }

    // =========================================================
    // Helper: remove first rule matching type+value
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
    // Full UI refresh
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
    }

    // =========================================================
    // Output tabs
    // =========================================================
    protected void RefreshOutputTabs()
    {
        int i;
        for (i = 0; i < 6; i = i + 1)
        {
            bool isSelected = (i == m_SelectedOutput);
            LFPG_SortOutputConfig cfg = m_Config.GetOutput(i);
            bool hasContent = false;
            if (cfg)
            {
                int rc = cfg.GetRuleCount();
                if (rc > 0 || cfg.m_IsCatchAll)
                {
                    hasContent = true;
                }
            }

            int bgCol = LFPG_SorterView.COL_BTN;
            int txtCol = LFPG_SorterView.COL_TEXT_MID;
            if (isSelected)
            {
                bgCol = LFPG_SorterView.COL_BG_ELEVATED;
                txtCol = LFPG_SorterView.COL_GREEN;
            }

            string label = (i + 1).ToString();
            if (i < 10)
            {
                label = "0" + label;
            }

            ImageWidget tabBg = GetTabBg(i);
            TextWidget tabTxt = GetTabText(i);
            TintBtnBg(tabBg, bgCol);
            if (tabTxt)
            {
                tabTxt.SetColor(txtCol);
                tabTxt.SetText(label);
            }
        }
    }

    protected ImageWidget GetTabBg(int idx)
    {
        if (idx == 0) return TabOut0Bg;
        if (idx == 1) return TabOut1Bg;
        if (idx == 2) return TabOut2Bg;
        if (idx == 3) return TabOut3Bg;
        if (idx == 4) return TabOut4Bg;
        if (idx == 5) return TabOut5Bg;
        return null;
    }

    protected TextWidget GetTabText(int idx)
    {
        if (idx == 0) return TabOut0Text;
        if (idx == 1) return TabOut1Text;
        if (idx == 2) return TabOut2Text;
        if (idx == 3) return TabOut3Text;
        if (idx == 4) return TabOut4Text;
        if (idx == 5) return TabOut5Text;
        return null;
    }

    // =========================================================
    // View tabs + panel visibility
    // =========================================================
    protected void RefreshViewTabs()
    {
        int rulesBg = LFPG_SorterView.COL_BTN;
        int rulesTxt = LFPG_SorterView.COL_TEXT_DIM;
        int prevBg = LFPG_SorterView.COL_BTN;
        int prevTxt = LFPG_SorterView.COL_TEXT_DIM;

        if (m_ShowRules)
        {
            rulesBg = LFPG_SorterView.COL_BG_ELEVATED;
            rulesTxt = LFPG_SorterView.COL_GREEN;
        }
        else
        {
            prevBg = LFPG_SorterView.COL_BG_ELEVATED;
            prevTxt = LFPG_SorterView.COL_GREEN;
        }

        TintBtnBg(TabRulesBg, rulesBg);
        TintBtnBg(TabPreviewBg, prevBg);
        if (TabRulesText)  { TabRulesText.SetColor(rulesTxt); }
        if (TabPreviewText) { TabPreviewText.SetColor(prevTxt); }

        if (RulesPanel)   { RulesPanel.Show(m_ShowRules); }
        if (PreviewPanel) { PreviewPanel.Show(!m_ShowRules); }
    }

    // =========================================================
    // Category buttons
    // =========================================================
    protected void RefreshCategoryButtons()
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg)
            return;

        RefreshCatBtn(CatBtn0Bg, CatBtn0Text, outCfg, m_CatValue0);
        RefreshCatBtn(CatBtn1Bg, CatBtn1Text, outCfg, m_CatValue1);
        RefreshCatBtn(CatBtn2Bg, CatBtn2Text, outCfg, m_CatValue2);
        RefreshCatBtn(CatBtn3Bg, CatBtn3Text, outCfg, m_CatValue3);
        RefreshCatBtn(CatBtn4Bg, CatBtn4Text, outCfg, m_CatValue4);
        RefreshCatBtn(CatBtn5Bg, CatBtn5Text, outCfg, m_CatValue5);
        RefreshCatBtn(CatBtn6Bg, CatBtn6Text, outCfg, m_CatValue6);
        RefreshCatBtn(CatBtn7Bg, CatBtn7Text, outCfg, m_CatValue7);
    }

    protected void RefreshCatBtn(ImageWidget bg, TextWidget txt, LFPG_SortOutputConfig outCfg, string catValue)
    {
        bool active = outCfg.HasRule(LFPG_SORT_FILTER_CATEGORY, catValue);
        int bgCol = LFPG_SorterView.COL_BTN;
        int txtCol = LFPG_SorterView.COL_TEXT_MID;
        if (active)
        {
            bgCol = LFPG_SorterView.COL_GREEN_BTN;
            txtCol = LFPG_SorterView.COL_GREEN;
        }
        TintBtnBg(bg, bgCol);
        if (txt) { txt.SetColor(txtCol); }
    }

    // =========================================================
    // Slot buttons
    // =========================================================
    protected void RefreshSlotButtons()
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg)
            return;

        RefreshSlotBtn(SlotPre0Bg, SlotPre0Text, outCfg, LFPG_SORT_SLOT_TINY);
        RefreshSlotBtn(SlotPre1Bg, SlotPre1Text, outCfg, LFPG_SORT_SLOT_SMALL);
        RefreshSlotBtn(SlotPre2Bg, SlotPre2Text, outCfg, LFPG_SORT_SLOT_MEDIUM);
        RefreshSlotBtn(SlotPre3Bg, SlotPre3Text, outCfg, LFPG_SORT_SLOT_LARGE);
    }

    protected void RefreshSlotBtn(ImageWidget bg, TextWidget txt, LFPG_SortOutputConfig outCfg, string slotValue)
    {
        bool active = outCfg.HasRule(LFPG_SORT_FILTER_SLOT, slotValue);
        int bgCol = LFPG_SorterView.COL_BTN;
        int txtCol = LFPG_SorterView.COL_TEXT_MID;
        if (active)
        {
            bgCol = LFPG_SorterView.COL_BLUE_BTN;
            txtCol = LFPG_SorterView.COL_BLUE;
        }
        TintBtnBg(bg, bgCol);
        if (txt) { txt.SetColor(txtCol); }
    }

    // =========================================================
    // Catch-all button
    // =========================================================
    protected void RefreshCatchAllButton()
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg)
            return;

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
        TintBtnBg(BtnCatchAllBg, bgCol);
        if (BtnCatchAllText) { BtnCatchAllText.SetColor(txtCol); BtnCatchAllText.SetText(label); }
    }

    // =========================================================
    // Tags list (ObservableCollection rebuild)
    // =========================================================
    protected void RefreshTagsList()
    {
        TagsList.Clear();

        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        if (!outCfg)
            return;

        int ruleCount = outCfg.GetRuleCount();
        int ri;
        for (ri = 0; ri < ruleCount; ri = ri + 1)
        {
            ref LFPG_SortFilterRule rule = outCfg.m_Rules[ri];
            if (!rule)
                continue;

            string label = rule.GetDisplayLabel();
            int color = GetRuleColor(rule.m_Type);

            LFPG_SorterTagView tag = new LFPG_SorterTagView();
            tag.SetData(label, color, ri, m_SelectedOutput);
            TagsList.Insert(tag);
        }

        // Catch-all as a special tag
        if (outCfg.m_IsCatchAll)
        {
            LFPG_SorterTagView caTag = new LFPG_SorterTagView();
            caTag.SetData("* CATCH-ALL", LFPG_SorterView.COL_AMBER, -1, m_SelectedOutput);
            TagsList.Insert(caTag);
        }

        // Empty state
        bool isEmpty = (ruleCount == 0 && !outCfg.m_IsCatchAll);
        if (TagsEmpty)
        {
            TagsEmpty.Show(isEmpty);
        }
    }

    protected int GetRuleColor(int ruleType)
    {
        if (ruleType == LFPG_SORT_FILTER_CATEGORY)
            return LFPG_SorterView.COL_GREEN;
        if (ruleType == LFPG_SORT_FILTER_PREFIX)
            return LFPG_SorterView.COL_BLUE;
        if (ruleType == LFPG_SORT_FILTER_CONTAINS)
            return LFPG_SorterView.COL_AMBER;
        if (ruleType == LFPG_SORT_FILTER_SLOT)
            return 0xFFA78BFA;  // purple
        return LFPG_SorterView.COL_TEXT;
    }

    // =========================================================
    // Rule count + dest indicator
    // =========================================================
    protected void RefreshRuleCount()
    {
        LFPG_SortOutputConfig outCfg = m_Config.GetOutput(m_SelectedOutput);
        int count = 0;
        if (outCfg)
        {
            count = outCfg.GetRuleCount();
        }
        RuleCount = count.ToString() + "/8";
        NotifyPropertyChanged("RuleCount", false);

        MatchCount = "0 items match";
        NotifyPropertyChanged("MatchCount", false);
    }

    protected void RefreshDestIndicator()
    {
        string dest = GetDestName(m_SelectedOutput);
        bool hasDest = (dest != "");

        if (DestIndicator)
        {
            DestIndicator.Show(hasDest);
        }

        if (hasDest)
        {
            DestName = dest;
            NotifyPropertyChanged("DestName", false);
        }

        // Update header subtitle
        if (hasDest)
        {
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
        if (idx == 0) return m_Dest0;
        if (idx == 1) return m_Dest1;
        if (idx == 2) return m_Dest2;
        if (idx == 3) return m_Dest3;
        if (idx == 4) return m_Dest4;
        if (idx == 5) return m_Dest5;
        return "";
    }

    // =========================================================
    // Helper: tint an ImageWidget as button background
    // =========================================================
    protected void TintBtnBg(ImageWidget bg, int color)
    {
        if (!bg)
            return;
        bg.LoadImageFile(0, PROC);
        bg.SetColor(color);
    }
};
