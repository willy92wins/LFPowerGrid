// =========================================================
// LF_PowerGrid - Action: Toggle Switch V2 (v1.6.0)
//
// Latching toggle: flips switch ON→OFF or OFF→ON.
// No item required (CCINone) — player walks up and interacts.
//
// Conditions:
//   - Target must be LFPG_SwitchV2
//   - Within interact distance
//   - No debounce: action always available (bidirectional toggle)
//
// IMPORTANTE: Registrar en ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionToggleSwitchV2).
// =========================================================

class LFPG_ActionToggleSwitchV2 : ActionInteractBase
{
    void LFPG_ActionToggleSwitchV2()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_TOGGLE_SWITCH";
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

        LFPG_SwitchV2 sw = LFPG_SwitchV2.Cast(targetObj);
        if (!sw)
            return false;

        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), sw.GetPosition());
        if (distSq > LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M)
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

        LFPG_SwitchV2 sw = LFPG_SwitchV2.Cast(targetObj);
        if (!sw)
            return;

        sw.LFPG_ToggleSwitch();

        PlayerBase pb = PlayerBase.Cast(action_data.m_Player);
        if (pb)
        {
            if (sw.LFPG_GetSwitchOn())
            {
                pb.MessageStatus("[LFPG] Switch ON");
            }
            else
            {
                pb.MessageStatus("[LFPG] Switch OFF");
            }
        }
    }
};
