// =========================================================
// LF_PowerGrid - Action: Toggle Push Button (v0.10.0)
//
// Momentary press: activates the button for 2 seconds.
// No item required (CCINone) — player walks up and interacts.
//
// Conditions:
//   - Target must be LFPG_PushButton
//   - Within interact distance
//   - Button must NOT already be in SwitchOn state (debounce)
//
// IMPORTANTE: Registrar en ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionTogglePushButton).
// =========================================================

class LFPG_ActionTogglePushButton : ActionInteractBase
{
    void LFPG_ActionTogglePushButton()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_PRESS_BUTTON";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem   = new CCINone;
        m_ConditionTarget = new CCTCursor(LFPG_INTERACT_DIST_M);
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!player)
            return false;

        if (!target)
            return false;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return false;

        LFPG_PushButton btn = LFPG_PushButton.Cast(targetObj);
        if (!btn)
            return false;

        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), btn.GetPosition());
        if (distSq > LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M)
            return false;

        // Hide action while button is already pressed (prevents spam)
        if (btn.LFPG_GetSwitchOn())
            return false;

        return true;
    }

    override void OnExecuteServer(ActionData action_data)
    {
        super.OnExecuteServer(action_data);

        if (!action_data)
            return;

        if (!action_data.m_Target)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        LFPG_PushButton btn = LFPG_PushButton.Cast(targetObj);
        if (!btn)
            return;

        btn.LFPG_ToggleButton();

        PlayerBase pb = PlayerBase.Cast(action_data.m_Player);
        if (pb)
        {
            pb.MessageStatus("[LFPG] Button pressed");
        }
    }
};
