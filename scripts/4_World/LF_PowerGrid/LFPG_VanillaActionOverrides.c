// =========================================================
// LF_PowerGrid - Vanilla Action Overrides (v0.7.37)
//
// Problem: RemoveAction(ActionTurnOnPowerGenerator) in
// LF_TestGenerator.SetActions() does not reliably prevent
// vanilla actions from appearing on LF_TestGenerator.
// DayZ's action system can re-inject inherited actions via
// C++ paths, CompEM state changes, or class hierarchy
// resolution that bypasses script-level RemoveAction().
//
// Solution: Use `modded class` to globally override the
// vanilla ActionCondition. When the target is an LFPG
// generator (LF_TestGenerator), return false so the vanilla
// action never appears. For any other PowerGenerator, fall
// through to vanilla logic via super.
//
// This is non-destructive:
//   - Other mods / vanilla generators are unaffected.
//   - LF_TestGenerator keeps its own ActionLFPG_ToggleSource.
//   - No changes needed to SetActions() (RemoveAction calls
//     remain as defense-in-depth).
//
// Files affected: This file only. No changes to existing
// LFPG files required.
// =========================================================

// ---------------------------------------------------------
// Block vanilla "Turn On" on LFPG generators
// ---------------------------------------------------------
modded class ActionTurnOnPowerGenerator
{
    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!player || !target)
            return false;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return false;

        // If target is an LFPG generator, block vanilla action entirely.
        // ActionLFPG_ToggleSource handles on/off with sparkplug validation.
        LF_TestGenerator lfpgGen = LF_TestGenerator.Cast(targetObj);
        if (lfpgGen)
            return false;

        // All other PowerGenerators: vanilla behavior unchanged
        return super.ActionCondition(player, target, item);
    }
};

// ---------------------------------------------------------
// Block vanilla "Turn Off" on LFPG generators
// ---------------------------------------------------------
modded class ActionTurnOffPowerGenerator
{
    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!player || !target)
            return false;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return false;

        // If target is an LFPG generator, block vanilla action entirely.
        // ActionLFPG_ToggleSource handles on/off with sparkplug validation.
        LF_TestGenerator lfpgGen = LF_TestGenerator.Cast(targetObj);
        if (lfpgGen)
            return false;

        // All other PowerGenerators: vanilla behavior unchanged
        return super.ActionCondition(player, target, item);
    }
};
