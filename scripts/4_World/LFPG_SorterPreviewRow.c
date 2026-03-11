// =========================================================
// LF_PowerGrid — Sorter Preview Row (Dabs MVC prefab, v2.1)
//
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

    override string GetLayoutFile()
    {
        return "LFPowerGrid/gui/layouts/LF_SorterPreviewRow.layout";
    }

    override typename GetControllerType()
    {
        return LFPG_SorterPreviewRowController;
    }

    void SetData(string itemName, string catKey, int slotCount)
    {
        LFPG_SorterPreviewRowController ctrl = LFPG_SorterPreviewRowController.Cast(GetController());
        if (!ctrl)
            return;

        ctrl.ItemName = itemName;
        ctrl.CatIcon = GetCatIcon(catKey);
        ctrl.SlotText = slotCount.ToString() + "s";
        ctrl.NotifyPropertyChanged("ItemName");
        ctrl.NotifyPropertyChanged("CatIcon");
        ctrl.NotifyPropertyChanged("SlotText");

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
