// =========================================================
// LF_PowerGrid - Custom placement action for Splitter Kit
//
// v0.7.26: Based on ActionPlaceObjectGemaLF (proven pattern).
// v0.7.38: Registered in ActionConstructor (was missing).
// v0.7.39: Restored m_FullBody = true and SetupAnimation.
//   The v0.7.38 removal was incorrect — the real bug was only
//   the missing ActionConstructor registration. Without m_FullBody
//   the modifier pipeline runs a wrong animation (drinking) and
//   does NOT move the kit to hologram position before the callback.
//
// Why m_FullBody = true?
//   ActionPlaceObject's full-body pipeline moves the item to the
//   hologram position BEFORE OnPlacementComplete fires. Without
//   m_FullBody = true, the modifier pipeline skips this step and
//   the kit stays at the player position -> splitter spawns wrong.
//   The CMD_ACTIONFB_PLACING_* commands also give the correct
//   placing animation (kneel + place) instead of generic modifier
//   animations (drinking, etc).
//
// Why not ActionDeployObject?
//   ActionDeployObject passes position/orientation as "0 0 0"
//   to OnPlacementComplete, and does NOT move the item to the
//   hologram position before the callback. This caused the
//   splitter to spawn at the player instead of the hologram.
//
// ActionTogglePlaceObject enters placement mode (shows hologram).
// This action confirms the placement (fires animation + callback).
//
// IMPORTANT: Must be registered in ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionPlaceSplitter) or AddAction() will
//   silently fail and the custom action will never be available.
// =========================================================

class LFPG_ActionPlaceSplitter : ActionPlaceObject
{
    void LFPG_ActionPlaceSplitter()
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
            // LF_Splitter_Kit is a medium-sized item
            m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_PLACING_2HD;
        }
    }

    override protected int GetStanceMask(PlayerBase player)
    {
        return DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
    }
};
