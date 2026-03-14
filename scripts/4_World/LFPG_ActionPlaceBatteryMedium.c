// =========================================================
// LF_PowerGrid - Custom placement action for BatteryMedium Kit
//
// v2.0: Same proven pattern as LFPG_ActionPlaceSplitter / Combiner.
//
// Why m_FullBody = true?
//   ActionPlaceObject full-body pipeline moves the item to the
//   hologram position BEFORE OnPlacementComplete fires. Without
//   m_FullBody = true, the modifier pipeline skips this step and
//   the kit stays at the player position -> battery spawns wrong.
//   The CMD_ACTIONFB_PLACING_* commands also give the correct
//   placing animation (kneel + place) instead of generic modifier
//   animations (drinking, etc).
//
// ActionTogglePlaceObject enters placement mode (shows hologram).
// This action confirms the placement (fires animation + callback).
//
// IMPORTANT: Must be registered in ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionPlaceBatteryMedium) or AddAction()
//   will silently fail and the custom action will never be available.
// =========================================================

class LFPG_ActionPlaceBatteryMedium : ActionPlaceObject
{
    void LFPG_ActionPlaceBatteryMedium()
    {
        // Allow placement while standing or crouching
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;

        // Full-body action: required for the engine to move the kit
        // to hologram position before OnPlacementComplete fires.
        m_FullBody = true;
    }

    // Force PLACING animation regardless of item.IsDeployable() result.
    // Without this override, the engine may pick a wrong animation
    // (drinking, eating, etc.) or skip it entirely.
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
            // Battery Medium is a medium-sized item
            m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_PLACING_2HD;
        }
    }

    override protected int GetStanceMask(PlayerBase player)
    {
        return DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
    }
};
