// =========================================================
// LF_PowerGrid — Sorter Preview Row (Dabs MVC prefab, v2.0)
//
// ScriptView for a single item in the match preview.
// Used in ObservableCollection<ref LFPG_SorterPreviewRow>
// bound to GridSpacerWidget "PreviewItems".
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
        ctrl.NotifyPropertiesChanged();

        // Style
        string procWhite = LFPG_SorterView.PROC_WHITE;
        if (CatBadge)
        {
            CatBadge.LoadImageFile(0, procWhite);
            CatBadge.SetColor(0x1234D399);  // green ghost
        }
        if (SlotBadgeBg)
        {
            SlotBadgeBg.LoadImageFile(0, procWhite);
            SlotBadgeBg.SetColor(0xE61C2840);  // bg elevated
        }
        if (RowSep)
        {
            RowSep.LoadImageFile(0, procWhite);
            RowSep.SetColor(0x14CBD5E1);  // separator
        }
        if (CatIcon)
        {
            CatIcon.SetColor(0xFF34D399);  // green
        }
        if (ItemName)
        {
            ItemName.SetColor(0xFFF1F5F9);  // text primary
        }
        if (SlotText)
        {
            SlotText.SetColor(0xFF94A3B8);  // text mid
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
