// =========================================================
// LF_PowerGrid - Placement action for Logic Gate Kits
//
// v4.0 (Fase 3): Inherits from LFPG_ActionPlaceGeneric.
// Only overrides ActionCondition because vanilla placement
// validation fails for small items (logic gate kits).
//
// Constructor, SetupAnimation, GetStanceMask all inherited.
// =========================================================

class LFPG_ActionPlaceLogicGate : LFPG_ActionPlaceGeneric
{
    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        // Direct check instead of super.ActionCondition which
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
};
