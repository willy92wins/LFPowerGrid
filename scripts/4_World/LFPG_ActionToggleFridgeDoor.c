// =========================================================
// LF_PowerGrid - Action: Toggle Fridge Door (v1.0.0)
//
// Opens or closes the fridge door. No item required (CCINone).
// Dynamic text: "Open Fridge" / "Close Fridge"
//
// Base: ActionInteractBase (CCINone, no item in hand)
// Target: LFPG_Fridge
// =========================================================

class LFPG_ActionToggleFridgeDoor : ActionInteractBase
{
    void LFPG_ActionToggleFridgeDoor()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_TOGGLE_FRIDGE";
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

        LFPG_Fridge fridge = LFPG_Fridge.Cast(targetObj);
        if (!fridge)
            return false;

        // Manual proximity check
        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), fridge.GetPosition());
        float maxSq = LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M;
        if (distSq > maxSq)
            return false;

        // Dynamic text based on door state
        bool isOpen = fridge.LFPG_IsOpen();
        if (isOpen)
        {
            m_Text = "#STR_LFPG_ACTION_CLOSE_FRIDGE";
        }
        else
        {
            m_Text = "#STR_LFPG_ACTION_OPEN_FRIDGE";
        }

        return true;
    }

    override void OnExecuteServer(ActionData action_data)
    {
        super.OnExecuteServer(action_data);

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        LFPG_Fridge fridge = LFPG_Fridge.Cast(targetObj);
        if (!fridge)
            return;

        fridge.LFPG_ToggleDoor();
    }
};
