// =========================================================
// LF_PowerGrid - Custom placement action for Splitter Kit
//
// v0.7.26: Based on ActionPlaceObjectGemaLF (proven pattern).
//
// Why not ActionDeployObject?
//   ActionDeployObject passes position/orientation as "0 0 0"
//   to OnPlacementComplete, and does NOT move the item to the
//   hologram position before the callback. This caused the
//   splitter to spawn at the player instead of the hologram.
//
// ActionPlaceObject (this parent) moves the item to the hologram
// position BEFORE calling OnPlacementComplete, so GetPosition()
// and GetOrientation() return the correct hologram transform.
//
// ActionTogglePlaceObject enters placement mode (shows hologram).
// This action confirms the placement (fires animation + callback).
// =========================================================

class LFPG_ActionPlaceSplitter : ActionPlaceObject
{
    void LFPG_ActionPlaceSplitter()
    {
        // Allow placement while standing or crouching
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
        m_FullBody = true;
    }

    // Force PLACING animation regardless of item.IsDeployable() result.
    // Without this override, the engine may pick wrong animation or skip it.
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
            m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_PLACING_2HD; // splitter is a medium item
        }
    }

    override protected int GetStanceMask(PlayerBase player)
    {
        return DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
    }
};
