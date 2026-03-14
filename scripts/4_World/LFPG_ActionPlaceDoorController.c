// =========================================================
// LF_PowerGrid - Custom placement action for DoorController Kit (v1.0.0)
//
// Patron identico a LFPG_ActionPlaceCamera.
//
// m_FullBody = true: obligatorio para que el pipeline mueva el kit
// a la posicion del hologram ANTES de llamar OnPlacementComplete.
// Sin esto, el controller spawna en la pos del player.
//
// IMPORTANTE: registrar en ActionConstructor.RegisterActions()
// via actions.Insert(LFPG_ActionPlaceDoorController).
// =========================================================

class LFPG_ActionPlaceDoorController : ActionPlaceObject
{
    void LFPG_ActionPlaceDoorController()
    {
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
        m_FullBody   = true;
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
