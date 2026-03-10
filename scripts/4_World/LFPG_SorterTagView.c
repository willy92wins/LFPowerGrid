// =========================================================
// LF_PowerGrid — Sorter Tag Chip (Dabs MVC prefab, v2.0)
//
// ScriptView for a single rule tag chip. Used as items in
// ObservableCollection<ref LFPG_SorterTagView> bound to
// WrapSpacerWidget "TagsList".
//
// The BtnRemove Relay_Command propagates up to the parent
// controller via ScriptedViewBase event hierarchy.
//
// Enforce Script: no ternaries, no ++/--, no foreach.
// =========================================================

class LFPG_SorterTagController extends ViewController
{
    // Bound to TextWidget "TagLabel" in LF_SorterTag.layout
    string TagLabel;

    // Data stored for removal lookup (not bound to UI)
    int m_RuleIndex;
    int m_OutputIndex;

    // Relay_Command: BtnRemove click
    void BtnRemove()
    {
        // Propagate removal to parent controller
        // Parent is the LFPG_SorterController via view hierarchy
        LFPG_SorterController parent = LFPG_SorterController.Cast(m_ParentScriptedViewBase);
        if (!parent)
        {
            // Try grandparent (view → controller)
            ScriptedViewBase parentView = ScriptedViewBase.Cast(m_ParentScriptedViewBase);
            if (parentView)
            {
                parent = LFPG_SorterController.Cast(parentView);
            }
        }
        if (parent)
        {
            parent.OnRemoveTag(m_OutputIndex, m_RuleIndex);
        }
    }
};

class LFPG_SorterTagView extends ScriptView
{
    // Auto-assigned widgets
    ImageWidget TagBg;
    TextWidget TagLabel;

    // Color for this tag type
    protected int m_TagColor;

    override string GetLayoutFile()
    {
        return "LFPowerGrid/gui/layouts/LF_SorterTag.layout";
    }

    override typename GetControllerType()
    {
        return LFPG_SorterTagController;
    }

    void SetData(string label, int color, int ruleIndex, int outputIndex)
    {
        m_TagColor = color;

        LFPG_SorterTagController ctrl = LFPG_SorterTagController.Cast(GetController());
        if (ctrl)
        {
            ctrl.TagLabel = label;
            ctrl.m_RuleIndex = ruleIndex;
            ctrl.m_OutputIndex = outputIndex;
            ctrl.NotifyPropertyChanged("TagLabel");
        }

        // Style the chip
        if (TagBg)
        {
            TagBg.LoadImageFile(0, LFPG_SorterView.PROC_WHITE);
            // Tinted background at ~7% opacity of the tag color
            int bgColor = (color & 0x00FFFFFF) | 0x12000000;
            TagBg.SetColor(bgColor);
        }
        if (TagLabel)
        {
            TagLabel.SetColor(color);
        }
    }
};
