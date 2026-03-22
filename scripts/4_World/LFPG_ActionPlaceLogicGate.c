// =========================================================
// LF_PowerGrid - Placement action for Logic Gate Kits
//
// v1.7.0: Shared action for AND/OR/XOR gate kit placement.
// Same proven pattern as LFPG_ActionPlaceCombiner.
//
// ActionTogglePlaceObject enters placement mode (shows hologram).
// This action confirms the placement (fires animation + callback).
//
// IMPORTANT: Must be registered in ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionPlaceLogicGate) or AddAction() will
//   silently fail.
// =========================================================

class LFPG_ActionPlaceLogicGate : ActionPlaceObject
{
    void LFPG_ActionPlaceLogicGate()
    {
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
        m_FullBody = true;
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        // v2.1: Direct check instead of super.ActionCondition which
        // returns false for logic gate kits (vanilla placement validation
        // issue with small items). Matches proven pattern from other mods.
        if (!player)
            return false;

        if (!item)
            return false;

        if (!player.IsPlacingLocal())
            return false;

        Hologram holo = player.GetHologramLocal();
        if (!holo)
            return false;

        if (holo.IsColliding())
            return false;

        return true;
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
            // Logic gate kits are medium-sized
            m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_PLACING_2HD;
        }
    }

    override protected int GetStanceMask(PlayerBase player)
    {
        return DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
    }
};
