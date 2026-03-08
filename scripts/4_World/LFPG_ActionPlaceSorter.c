// =========================================================
// LF_PowerGrid - Custom placement action for Sorter Kit
//
// v1.2.0 Sprint S2: Same proven pattern as LFPG_ActionPlaceSplitter.
//
// Why m_FullBody = true?
//   ActionPlaceObject full-body pipeline moves the item to the
//   hologram position BEFORE OnPlacementComplete fires. Without
//   m_FullBody = true, the modifier pipeline skips this step and
//   the kit stays at the player position -> sorter spawns wrong.
//
// ActionTogglePlaceObject enters placement mode (shows hologram).
// This action confirms the placement (fires animation + callback).
//
// IMPORTANT: Must be registered in ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionPlaceSorter) or AddAction() will
//   silently fail and the custom action will never be available.
// =========================================================

class LFPG_ActionPlaceSorter : ActionPlaceObject
{
    void LFPG_ActionPlaceSorter()
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
            m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_PLACING_2HD;
        }
    }

    override protected int GetStanceMask(PlayerBase player)
    {
        return DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
    }
};
