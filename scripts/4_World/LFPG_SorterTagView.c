// =========================================================
// LF_PowerGrid — Sorter Tag Chip (Dabs MVC prefab, v2.0 FIXED)
//
// Bug 2 fix: stores direct ref to owning controller instead
// of traversing m_ParentScriptedViewBase (which points to
// TagView, not SorterController).
//
// Enforce Script: no ternaries, no ++/--, no foreach.
// =========================================================

class LFPG_SorterTagController extends ViewController
{
    string TagLabel;

    int m_RuleIndex;
    int m_OutputIndex;

    // Bug 2 fix: direct ref instead of parent traversal
    LFPG_SorterController m_OwnerController;

    void BtnRemove()
    {
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

    // Bug 2 fix: ownerCtrl passed directly from Controller.RefreshTagsList
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
            ctrl.NotifyPropertyChanged("TagLabel");
        }

        if (TagBg)
        {
            TagBg.LoadImageFile(0, LFPG_SorterView.PROC_WHITE);
            int bgColor = (color & 0x00FFFFFF) | 0x12000000;
            TagBg.SetColor(bgColor);
        }
        if (TagLabel)
        {
            TagLabel.SetColor(color);
        }
    }
};
