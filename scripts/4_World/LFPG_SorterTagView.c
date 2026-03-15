// =========================================================
// LF_PowerGrid — Sorter Tag Chip (Dabs MVC prefab, v2.5)
//
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

    void BtnRemove()
    {
        LFPG_SorterView.PlayUIClick();
        if (m_OwnerController)
        {
            m_OwnerController.OnRemoveTag(m_OutputIndex, m_RuleIndex);
        }
    }
};

class LFPG_SorterTagView extends ScriptView
{
    ImageWidget TagBg;
    TextWidget TagLabel;
    protected int m_TagColor;

    override string GetLayoutFile()
    {
        return "LFPowerGrid/gui/layouts/LF_SorterTag.layout";
    }

    override typename GetControllerType()
    {
        return LFPG_SorterTagController;
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
        if (TagLabel)
        {
            TagLabel.SetColor(color);
        }

        // v2.5 B3: Scale dynamic tag to match current resolution.
        // ScaleWidget is a no-op at scale ~1.0 (1080p).
        Widget tagRoot = GetLayoutRoot();
        float tagScale = LFPG_UIScaler.ComputeScale();
        LFPG_UIScaler.ScaleWidget(tagRoot, tagScale);
    }
};
