// =========================================================
// LF_PowerGrid — Sorter Preview Row (Dabs MVC prefab, v2.6)
//
// v2.6: m_Scaled guard for pool-safe reuse (future).
// Bug 9 fix: separator alpha 0x14→0x30 (via shared constant)
// Bug 14 fix: hardcoded colors replaced with shared constants
//
// Enforce Script: no ternaries, no ++/--, no foreach.
// =========================================================

class LFPG_SorterPreviewRowController extends ViewController
{
    // Bound to TextWidget "ItemName"
    string ItemName;

    // Bound to TextWidget "CatIcon"
    string CatIcon;

    // Bound to TextWidget "SlotText"
    string SlotText;
};

class LFPG_SorterPreviewRow extends ScriptView
{
    // Auto-assigned widgets
    ImageWidget CatBadge;
    ImageWidget SlotBadgeBg;
    ImageWidget RowSep;
    TextWidget CatIcon;
    TextWidget ItemName;
    TextWidget SlotText;
    protected bool m_Scaled;

    override string GetLayoutFile()
    {
        return "LFPowerGrid/gui/layouts/LF_SorterPreviewRow.layout";
    }

    override typename GetControllerType()
    {
        return LFPG_SorterPreviewRowController;
    }

    override bool UseUpdateLoop()
    {
        return false;
    }

    void SetData(string itemName, string catKey, int slotCount)
    {
        LFPG_SorterPreviewRowController ctrl = LFPG_SorterPreviewRowController.Cast(GetController());
        if (!ctrl)
            return;

        ctrl.ItemName = itemName;
        ctrl.CatIcon = GetCatIcon(catKey);
        string slotSuffix = "s";
        string slotVal = slotCount.ToString();
        slotVal = slotVal + slotSuffix;
        ctrl.SlotText = slotVal;
        string propIN = "ItemName";
        ctrl.NotifyPropertyChanged(propIN);
        string propCI = "CatIcon";
        ctrl.NotifyPropertyChanged(propCI);
        string propST = "SlotText";
        ctrl.NotifyPropertyChanged(propST);

        // Style — shared constants (Bug #14)
        string procWhite = LFPG_SorterView.PROC_WHITE;
        if (CatBadge)
        {
            CatBadge.LoadImageFile(0, procWhite);
            CatBadge.SetColor(LFPG_SorterView.COL_GREEN_BORDER);
        }
        if (SlotBadgeBg)
        {
            SlotBadgeBg.LoadImageFile(0, procWhite);
            SlotBadgeBg.SetColor(LFPG_SorterView.COL_BTN);
        }
        if (RowSep)
        {
            RowSep.LoadImageFile(0, procWhite);
            RowSep.SetColor(LFPG_SorterView.COL_SEPARATOR);
        }
        if (CatIcon)
        {
            CatIcon.SetColor(LFPG_SorterView.COL_GREEN);
        }
        if (ItemName)
        {
            ItemName.SetColor(LFPG_SorterView.COL_TEXT);
        }
        if (SlotText)
        {
            SlotText.SetColor(LFPG_SorterView.COL_TEXT_MID);
        }

        // v2.6: Scale only on first use (pool-safe for future reuse).
        if (!m_Scaled)
        {
            Widget rowRoot = GetLayoutRoot();
            float rowScale = LFPG_UIScaler.ComputeScale();
            LFPG_UIScaler.ScaleWidget(rowRoot, rowScale);
            m_Scaled = true;
        }
    }

    protected string GetCatIcon(string catKey)
    {
        if (catKey == "WEAPON")     return "W";
        if (catKey == "ATTACHMENT") return "A";
        if (catKey == "AMMO")       return "R";
        if (catKey == "CLOTHING")   return "C";
        if (catKey == "FOOD")       return "F";
        if (catKey == "MEDICAL")    return "M";
        if (catKey == "TOOL")       return "T";
        return "X";
    }
};
