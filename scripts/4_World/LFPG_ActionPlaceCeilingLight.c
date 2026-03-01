// =========================================================
// LF_PowerGrid - Custom placement action for CeilingLight Kit
//
// v0.7.47: Clone of LFPG_ActionPlaceSplitter (proven pattern).
//
// ActionTogglePlaceObject enters placement mode (shows hologram).
// This action confirms the placement (fires animation + callback).
//
// ActionPlaceObject (parent) moves the item to the hologram
// position BEFORE calling OnPlacementComplete, so the kit's
// GetPosition()/GetOrientation() return the hologram transform.
//
// CeilingLight Kit uses OnPlacementComplete PARAMETERS instead
// of GetPosition() (Hallazgo 2) — but the parent still needs
// m_FullBody=true and correct animation setup to work.
// =========================================================

class LFPG_ActionPlaceCeilingLight : ActionPlaceObject
{
    void LFPG_ActionPlaceCeilingLight()
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
