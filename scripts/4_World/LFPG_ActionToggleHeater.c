// =========================================================
// LF_PowerGrid - Action: Toggle Heater
//
// Flips the heater switch. No item required (CCINone).
// Dynamic text: "Turn On Heater" / "Turn Off Heater"
//
// The switch is physical: it can be flipped with the grid dead.
// Heat and glow only follow when the device is actually powered.
//
// Base: ActionInteractBase (CCINone, no item in hand)
// Target: LFPG_Heater
// =========================================================

class LFPG_ActionToggleHeater : ActionInteractBase
{
    void LFPG_ActionToggleHeater()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_HEATER_ON";
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

        LFPG_Heater heater = LFPG_Heater.Cast(targetObj);
        if (!heater)
            return false;

        // Manual proximity check
        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), heater.GetPosition());
        float maxSq = LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M;
        if (distSq > maxSq)
            return false;

        // Dynamic text based on switch state
        bool isOn = heater.LFPG_IsHeaterOn();
        if (isOn)
        {
            m_Text = "#STR_LFPG_ACTION_HEATER_OFF";
        }
        else
        {
            m_Text = "#STR_LFPG_ACTION_HEATER_ON";
        }

        return true;
    }

    override void OnExecuteServer(ActionData action_data)
    {
        super.OnExecuteServer(action_data);
        LFPG_ServerActions.Get().ToggleHeater_OnExecuteServer(action_data);
    }
};
