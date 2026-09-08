#ifndef SERVER
// Client-only compilation boundary
// =========================================================
// LF_PowerGrid — Sorter Preview Row (Dabs MVC prefab, v2.6)
//
// Bug 9 fix: separator alpha 0x14→0x30 (via shared constant)
// Bug 14 fix: hardcoded colors replaced with shared constants
//
// Enforce Script: no ternaries, no ++/--, no foreach.
// =========================================================

class LFPG_SorterPreviewRowController_TEST extends ViewController
{
    // Bound to TextWidget "ItemName"
    string ItemName;

    // Bound to TextWidget "CatIcon"
    string CatIcon;

    // Bound to TextWidget "SlotText"
    string SlotText;
};

class LFPG_SorterPreviewRow_TEST extends ScriptView
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
        return "LFPowerGrid/gui/layouts/test/LFPG_SorterPreviewRow_TEST.layout";
    }

    override typename GetControllerType()
    {
        return LFPG_SorterPreviewRowController_TEST;
    }

    override bool UseUpdateLoop()
    {
        return false;
    }

    // v4.3: slotCount replaced by infoStr (pre-formatted "WxH" or "WxH xQ")
    void SetData(string itemName, string catKey, string infoStr)
    {
        LFPG_SorterPreviewRowController_TEST ctrl = LFPG_SorterPreviewRowController_TEST.Cast(GetController());
        if (!ctrl)
            return;

        ctrl.ItemName = itemName;
        ctrl.CatIcon = GetCatIcon(catKey);
        ctrl.SlotText = infoStr;
        string propIN = "ItemName";
        ctrl.NotifyPropertyChanged(propIN);
        string propCI = "CatIcon";
        ctrl.NotifyPropertyChanged(propCI);
        string propST = "SlotText";
        ctrl.NotifyPropertyChanged(propST);

        if (CatIcon)
        {
            CatIcon.SetColor(LFPG_SorterView_TEST.COL_GREEN);
        }
        if (ItemName)
        {
            ItemName.SetColor(LFPG_SorterView_TEST.COL_TEXT);
        }
        if (SlotText)
        {
            SlotText.SetColor(LFPG_SorterView_TEST.COL_TEXT_MID);
        }

		// Scale each dynamic row once, as in V3.
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
#endif
