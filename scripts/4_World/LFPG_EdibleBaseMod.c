// =========================================================
// LF_PowerGrid - Edible_Base mod: pause decay inside powered, closed fridge
//
// Vanilla DayZ only pauses food decay when GetIsFrozen() is true
// (edible_base.c : CanProcessDecay). Setting the temperature low
// does not stop spoilage. This override checks whether the food's
// hierarchy root is a powered, closed LFPG_Fridge; if so decay is
// paused. The fridge still handles temperature via its cool tick.
//
// Compatibility:
//   - Calls super.CanProcessDecay() first, so any other mod that
//     returns false (e.g., tightens decay rules) keeps precedence.
//   - Only forces false when BOTH fridge.LFPG_IsPowered() and
//     !fridge.LFPG_IsOpen(). Any other state defers to super.
//   - Does not add member fields (Enforce restriction on modded class).
// =========================================================

modded class Edible_Base
{
    override bool CanProcessDecay()
    {
        if (!super.CanProcessDecay())
            return false;

        EntityAI root = GetHierarchyRoot();
        if (!root)
            return true;

        LFPG_Fridge fridge = LFPG_Fridge.Cast(root);
        if (!fridge)
            return true;

        if (!fridge.LFPG_IsPowered())
            return true;

        if (fridge.LFPG_IsOpen())
            return true;

        return false;
    }
};
