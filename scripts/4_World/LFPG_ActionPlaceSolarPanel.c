// =========================================================
// LF_PowerGrid - Placement action for Solar Panel Kit
//
// v0.7.47: Follows proven LFPG_ActionPlaceSplitter pattern.
//
// ActionPlaceObject moves the kit to hologram position BEFORE
// OnPlacementComplete fires. GetPosition()/GetOrientation()
// on the kit return correct hologram transform.
//
// ActionTogglePlaceObject enters placement mode (shows hologram).
// This action confirms placement (fires animation + callback).
// =========================================================

class LFPG_ActionPlaceSolarPanel : ActionPlaceObject
{
    void LFPG_ActionPlaceSolarPanel()
    {
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
        m_FullBody = true;
    }

    // Force PLACING animation regardless of item.IsDeployable() result.
    override void SetupAnimation(ItemBase item)
    {
        if (!item)
            return;

        if (item.IsHeavyBehaviour())
        {
            m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_PLACING_HEAVY;
        }
        else if (item.IsOneHandedBehaviour())
        {
            m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_PLACING_1HD;
        }
        else if (item.IsTwoHandedBehaviour())
        {
            m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_PLACING_2HD;
        }
        else
        {
            m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_PLACING_2HD;
        }
    }

    override protected int GetStanceMask(PlayerBase player)
    {
        return DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
    }
};
