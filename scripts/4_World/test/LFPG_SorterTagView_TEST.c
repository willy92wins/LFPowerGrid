#ifndef SERVER
// Client-only compilation boundary
// =========================================================
// LF_PowerGrid — Sorter Tag Chip (Dabs MVC prefab, v2.6)
//
// Bug 10 fix: tag bg alpha 0x12→0x26 for visibility in DayZ
// R1 fix: destructor breaks TagController→OwnerController
//         circular reference (refcount GC leak)
//
// Enforce Script: no ternaries, no ++/--, no foreach.
// =========================================================

class LFPG_SorterTagController_TEST extends ViewController
{
    string TagLabel;

    int m_RuleIndex;
    int m_OutputIndex;

    // Direct ref instead of parent traversal
    LFPG_SorterController_TEST m_OwnerController;
};

class LFPG_SorterTagView_TEST extends ScriptView
{
    ImageWidget TagBg;
    TextWidget TagLabel;
    ImageWidget TagLeftBar;
    TextWidget TagTypeLabel;
    protected int m_TagColor;

    override string GetLayoutFile()
    {
        return "LFPowerGrid/gui/layouts/test/LFPG_SorterTag_TEST.layout";
    }

    override typename GetControllerType()
    {
        return LFPG_SorterTagController_TEST;
    }

    override bool UseUpdateLoop()
    {
        return false;
    }

    // R1 fix: break circular ref (Controller → TagsList → TagView → TagController → m_OwnerController → Controller)
    // Without this, refcount GC never frees tags after ObservableCollection.Clear().
    void ~LFPG_SorterTagView_TEST()
    {
        LFPG_SorterTagController_TEST ctrl = LFPG_SorterTagController_TEST.Cast(GetController());
        if (ctrl)
        {
            ctrl.m_OwnerController = null;
        }
    }

    // ownerCtrl passed directly from Controller.RefreshTagsList
    void SetData(string label, int color, string typeTag, int ruleIndex, int outputIndex, LFPG_SorterController_TEST ownerCtrl)
    {
        m_TagColor = color;

        LFPG_SorterTagController_TEST ctrl = LFPG_SorterTagController_TEST.Cast(GetController());
        if (ctrl)
        {
            ctrl.TagLabel = label;
            ctrl.m_RuleIndex = ruleIndex;
            ctrl.m_OutputIndex = outputIndex;
            ctrl.m_OwnerController = ownerCtrl;
            string propTL = "TagLabel";
            ctrl.NotifyPropertyChanged(propTL);
        }

        if (TagLeftBar)
        {
            TagLeftBar.SetColor(color);
        }
        if (TagTypeLabel)
        {
            TagTypeLabel.SetText(typeTag);
            TagTypeLabel.SetColor(color);
        }
        // v4.3: Tag text uses COL_TEXT (light) for readability.
        // Was same color as bg tint → invisible. Color rule-type
        // is already communicated by the bg tint.
        if (TagLabel)
        {
            TagLabel.SetColor(LFPG_SorterView_TEST.COL_TEXT);
        }

        // v4.3: Set BtnRemove X text to grey + encode UID for
        // SorterView.OnClick dispatch (Plan B — Relay_Command
        // never reached TagController because SorterView.OnClick
        // intercepted the event first).
        // F3-C: UID encoding: 600 + outputIdx * 16 + (ruleIdx + 1)
        // Decode: encoded = uid - 600; outIdx = encoded / 16; rIdx = (encoded % 16) - 1
        // Changed from *10 to *16 to prevent collision if MAX_RULES >= 10.
        Widget tagRoot = GetLayoutRoot();
        if (tagRoot)
        {
            string btnName = "BtnRemove";
            ButtonWidget btnRemove = ButtonWidget.Cast(tagRoot.FindAnyWidget(btnName));
            if (btnRemove)
            {
                int encoded = outputIndex * 16;
                int rOffset = ruleIndex + 1;
                encoded = encoded + rOffset;
                int btnUid = 600 + encoded;
                btnRemove.SetUserID(btnUid);
            }
            string btnTxtName = "BtnRemoveText";
            TextWidget btnTxt = TextWidget.Cast(tagRoot.FindAnyWidget(btnTxtName));
            if (btnTxt)
            {
                btnTxt.SetColor(LFPG_SorterView_TEST.COL_TEXT_MID);
            }
        }
    }
};
#endif
