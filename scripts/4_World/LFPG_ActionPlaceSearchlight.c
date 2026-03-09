// =========================================================
// LF_PowerGrid - Custom placement action for Searchlight Kit
//
// v1.4.0: Based on LFPG_ActionPlaceSplitter (proven pattern).
//
// ActionTogglePlaceObject enters placement mode (shows hologram).
// This action confirms the placement (fires animation + callback).
//
// IMPORTANT: Must be registered in ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionPlaceSearchlight) or AddAction() will
//   silently fail and the custom action will never be available.
// =========================================================

class LFPG_ActionPlaceSearchlight : ActionPlaceObject
{
    void LFPG_ActionPlaceSearchlight()
    {
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
        m_FullBody = true;
    }

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
            // LF_Searchlight_Kit is a medium-large item (tripod)
            m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_PLACING_2HD;
        }
    }

    override protected int GetStanceMask(PlayerBase player)
    {
        return DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
    }
};
