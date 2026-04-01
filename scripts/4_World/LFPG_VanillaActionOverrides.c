// =========================================================
// LF_PowerGrid - Vanilla Action Overrides (v0.7.37)
//
// Uses `modded class` to intercept vanilla PowerGenerator
// actions. When target is an LFPG device, vanilla actions
// are blocked (ActionCondition returns false) or redirected
// (OnExecuteServer routes through LFPG logic).
//
// Defense-in-depth: works alongside RemoveAction() calls
// in LFPG_Generator.SetActions() and sparkplug validation
// in ActionLFPG_ToggleSource.ActionCondition/OnExecuteServer.
//
// IMPORTANT: Do NOT override constructors in modded classes.
// In Enforce Script, a modded constructor REPLACES the vanilla
// constructor entirely. The vanilla constructor sets m_CommandUID,
// m_StanceMask, m_Text, etc. Replacing it breaks the action
// for ALL generators on the server, not just LFPG ones.
// =========================================================

// ---------------------------------------------------------
// Block vanilla "Turn On" on LFPG generators
// ---------------------------------------------------------
modded class ActionTurnOnPowerGenerator
{
    // Diagnostic: one-time log to confirm modded class loaded.
    // Check server/client .RPT for "[LFPG] modded ActionTurnOnPowerGenerator active".
    // Remove both lines once confirmed working.
    protected static bool s_LFPG_TurnOnLogDone = false;

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        // One-time diagnostic print (does not repeat every frame)
        if (!s_LFPG_TurnOnLogDone)
        {
            s_LFPG_TurnOnLogDone = true;
            Print("[LFPG] modded ActionTurnOnPowerGenerator active");
        }

        if (!player || !target)
            return false;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return false;

        // v0.7.37: Block vanilla TurnOn for LFPG generators.
        // ActionLFPG_ToggleSource handles on/off with sparkplug validation.
        // Use both Cast and IsKindOf for maximum compatibility across
        // DayZ versions where class resolution order may differ.
        LFPG_Generator lfpgGen = LFPG_Generator.Cast(targetObj);
        if (lfpgGen)
            return false;

        if (targetObj.IsKindOf("LFPG_Generator"))
            return false;

        // All other PowerGenerators: vanilla behavior unchanged
        return super.ActionCondition(player, target, item);
    }

    // v0.7.37: Safety net for TurnOn.
    // If ActionCondition fails to block (DayZ re-evaluates actions
    // dynamically in some versions), redirect execution through
    // LFPG logic with sparkplug validation.
    override void OnExecuteServer(ActionData action_data)
    {
        if (!action_data || !action_data.m_Target)
        {
            super.OnExecuteServer(action_data);
            return;
        }

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
        {
            super.OnExecuteServer(action_data);
            return;
        }

        LFPG_Generator gen = LFPG_Generator.Cast(targetObj);
        if (gen)
        {
            Print("[LFPG] Vanilla TurnOn intercepted -- redirecting to LFPG_ToggleSource");

            // Block if no valid sparkplug (same gate as ActionLFPG_ToggleSource)
            if (!gen.LFPG_GetSwitchState() && !LFPG_DeviceLifecycle.IsSparkPlugValid(gen))
            {
                PlayerBase blockPlayer = PlayerBase.Cast(action_data.m_Player);
                if (blockPlayer)
                {
                    blockPlayer.MessageStatus("[LFPG] Cannot start: needs Spark Plug");
                }
                return;
            }

            gen.LFPG_ToggleSource();

            PlayerBase execPlayer = PlayerBase.Cast(action_data.m_Player);
            if (execPlayer)
            {
                bool nowOn = gen.LFPG_GetSwitchState();
                if (nowOn)
                {
                    execPlayer.MessageStatus("[LFPG] Generator ON - producing power");
                }
                else
                {
                    execPlayer.MessageStatus("[LFPG] Generator OFF");
                }
            }
            return;
        }

        // Not an LFPG device -- vanilla behavior
        super.OnExecuteServer(action_data);
    }
};

// ---------------------------------------------------------
// Block vanilla "Turn Off" on LFPG generators
// ---------------------------------------------------------
modded class ActionTurnOffPowerGenerator
{
    // Diagnostic: one-time log to confirm modded class loaded.
    // Check server/client .RPT for "[LFPG] modded ActionTurnOffPowerGenerator active".
    // Remove both lines once confirmed working.
    protected static bool s_LFPG_TurnOffLogDone = false;

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        // One-time diagnostic print (does not repeat every frame)
        if (!s_LFPG_TurnOffLogDone)
        {
            s_LFPG_TurnOffLogDone = true;
            Print("[LFPG] modded ActionTurnOffPowerGenerator active");
        }

        if (!player || !target)
            return false;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return false;

        // v0.7.37: Block vanilla TurnOff for LFPG generators.
        LFPG_Generator lfpgGen = LFPG_Generator.Cast(targetObj);
        if (lfpgGen)
            return false;

        if (targetObj.IsKindOf("LFPG_Generator"))
            return false;

        // All other PowerGenerators: vanilla behavior unchanged
        return super.ActionCondition(player, target, item);
    }

    // v0.7.37: Safety net for TurnOff.
    // If ActionCondition fails to block, redirect execution
    // through LFPG logic so m_SourceOn stays in sync and
    // the player gets proper feedback.
    override void OnExecuteServer(ActionData action_data)
    {
        if (!action_data || !action_data.m_Target)
        {
            super.OnExecuteServer(action_data);
            return;
        }

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
        {
            super.OnExecuteServer(action_data);
            return;
        }

        LFPG_Generator gen = LFPG_Generator.Cast(targetObj);
        if (gen)
        {
            Print("[LFPG] Vanilla TurnOff intercepted -- redirecting to LFPG_ToggleSource");
            gen.LFPG_ToggleSource();

            PlayerBase execPlayer = PlayerBase.Cast(action_data.m_Player);
            if (execPlayer)
            {
                bool nowOn = gen.LFPG_GetSwitchState();
                if (!nowOn)
                {
                    execPlayer.MessageStatus("[LFPG] Generator OFF");
                }
                else
                {
                    execPlayer.MessageStatus("[LFPG] Generator ON - producing power");
                }
            }
            return;
        }

        // Not an LFPG device -- vanilla behavior
        super.OnExecuteServer(action_data);
    }
};
