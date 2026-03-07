// =========================================================
// LF_PowerGrid - Custom placement action for PushButton Kit (v0.10.0)
//
// Patron identico a LFPG_ActionPlaceSplitter / LFPG_ActionPlaceCombiner.
//
// IMPORTANTE: Registrar en ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionPlacePushButton).
// =========================================================

class LFPG_ActionPlacePushButton : ActionPlaceObject
{
    void LFPG_ActionPlacePushButton()
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
            m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_PLACING_1HD;
        }
    }

    override protected int GetStanceMask(PlayerBase player)
    {
        return DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
    }
};
