// Concrete leaves retain typed selection and toggle dispatch.
class LFPG_ActionToggleSwitchBase : ActionInteractBase
{
    protected string m_LFPG_MessageOn;
    protected string m_LFPG_MessageOff;

    void LFPG_ActionToggleSwitchBase()
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
    protected LFPG_SwitchDeviceBase LFPG_SelectSwitch(Object targetObj)
    {
        return null;
    }
    protected void LFPG_ToggleTarget(LFPG_SwitchDeviceBase device)
    {
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

        LFPG_SwitchDeviceBase sw = LFPG_SelectSwitch(targetObj);
        if (!sw)
            return false;

        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), sw.GetPosition());
        float maxDistSq = LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M;
        if (distSq > maxDistSq)
            return false;

        if (sw.LFPG_GetSwitchOn())
        {
            m_Text = "#STR_LFPG_ACTION_SWITCH_OFF";
        }
        else
        {
            m_Text = "#STR_LFPG_ACTION_SWITCH_ON";
        }

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

        LFPG_SwitchDeviceBase sw = LFPG_SelectSwitch(targetObj);
        if (!sw)
            return;

        LFPG_ToggleTarget(sw);

        PlayerBase pb = PlayerBase.Cast(action_data.m_Player);
        if (pb)
        {
            if (sw.LFPG_GetSwitchOn())
            {
                pb.MessageStatus(m_LFPG_MessageOn);
            }
            else
            {
                pb.MessageStatus(m_LFPG_MessageOff);
            }
        }
    }
};
