// =========================================================
// LF_PowerGrid — Sorter Tag Chip (Dabs MVC prefab, v2.6)
//
// v2.6: Pool-safe reuse. m_Scaled guards ScaleWidget so it
//       only runs once per instance (ScaleWidget multiplies
//       current values — calling twice corrupts geometry).
// Bug 10 fix: tag bg alpha 0x12→0x26 for visibility in DayZ
// R1 fix: destructor breaks TagController→OwnerController
//         circular reference (refcount GC leak)
//
// Enforce Script: no ternaries, no ++/--, no foreach.
// =========================================================

class LFPG_SorterTagController extends ViewController
{
    string TagLabel;

    int m_RuleIndex;
    int m_OutputIndex;

    // Direct ref instead of parent traversal
    LFPG_SorterController m_OwnerController;
};

class LFPG_SorterTagView extends ScriptView
{
    ImageWidget TagBg;
    TextWidget TagLabel;
    protected int m_TagColor;
    protected bool m_Scaled;

    override string GetLayoutFile()
    {
        return "LFPowerGrid/gui/layouts/LFPG_SorterTag.layout";
    }

    override typename GetControllerType()
    {
        return LFPG_SorterTagController;
    }

    override bool UseUpdateLoop()
    {
        return false;
    }

    // R1 fix: break circular ref (Controller → TagsList → TagView → TagController → m_OwnerController → Controller)
    // Without this, refcount GC never frees tags after ObservableCollection.Clear().
    void ~LFPG_SorterTagView()
    {
        LFPG_SorterTagController ctrl = LFPG_SorterTagController.Cast(GetController());
        if (ctrl)
        {
            ctrl.m_OwnerController = null;
        }
    }

    // ownerCtrl passed directly from Controller.RefreshTagsList
    void SetData(string label, int color, int ruleIndex, int outputIndex, LFPG_SorterController ownerCtrl)
    {
        m_TagColor = color;

        LFPG_SorterTagController ctrl = LFPG_SorterTagController.Cast(GetController());
        if (ctrl)
        {
            ctrl.TagLabel = label;
            ctrl.m_RuleIndex = ruleIndex;
            ctrl.m_OutputIndex = outputIndex;
            ctrl.m_OwnerController = ownerCtrl;
            string propTL = "TagLabel";
            ctrl.NotifyPropertyChanged(propTL);
        }

        if (TagBg)
        {
            TagBg.LoadImageFile(0, LFPG_SorterView.PROC_WHITE);
            // Bug #10 fix: alpha 0x12→0x26 for visibility
            int bgColor = (color & 0x00FFFFFF) | 0x26000000;
            TagBg.SetColor(bgColor);
        }
        // v4.3: Tag text uses COL_TEXT (light) for readability.
        // Was same color as bg tint → invisible. Color rule-type
        // is already communicated by the bg tint.
        if (TagLabel)
        {
            TagLabel.SetColor(LFPG_SorterView.COL_TEXT);
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
                btnTxt.SetColor(LFPG_SorterView.COL_TEXT_MID);
            }
        }

        // v2.6: Scale only on first use. Pool reuse calls SetData again
        // but ScaleWidget multiplies current values — double-call corrupts.
        if (!m_Scaled)
        {
            float tagScale = LFPG_UIScaler.ComputeScale();
            if (tagRoot)
            {
                LFPG_UIScaler.ScaleWidget(tagRoot, tagScale);
            }
            m_Scaled = true;
        }
    }
};
