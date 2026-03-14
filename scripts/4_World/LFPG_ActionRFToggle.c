// =========================================================
// LF_PowerGrid - Action: RF Toggle (v3.0.0 - Sprint 2)
//
// Sends an RF signal to toggle all RF-capable devices within 50m.
// No item required (CCINone) — player walks up and interacts.
//
// Conditions:
//   - Target must be LF_Intercom
//   - Must be powered (m_PoweredNet == true)
//   - Must be switched ON (m_SwitchOn == true)
//   - 2s cooldown between activations
//
// IMPORTANTE: Registrar en ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionRFToggle).
// =========================================================

class LFPG_ActionRFToggle : ActionInteractBase
{
    void LFPG_ActionRFToggle()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_RF_TOGGLE";
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

        LF_Intercom ic = LF_Intercom.Cast(targetObj);
        if (!ic)
            return false;

        // Must be powered AND switched on to send RF
        if (!ic.LFPG_IsPowered())
            return false;

        if (!ic.LFPG_GetSwitchOn())
            return false;

        // Distance check
        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), ic.GetPosition());
        float maxDistSq = LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M;
        if (distSq > maxDistSq)
            return false;

        // Cooldown check (client-side hint — server re-checks in ExecuteRFToggle)
        int now = GetGame().GetTime();
        int lastToggle = ic.LFPG_GetLastRFToggleTime();
        int elapsed = now - lastToggle;
        if (elapsed < LFPG_INTERCOM_RF_COOLDOWN_MS)
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

        LF_Intercom ic = LF_Intercom.Cast(targetObj);
        if (!ic)
            return;

        // Server-side power and state validation
        if (!ic.LFPG_IsPowered())
            return;

        if (!ic.LFPG_GetSwitchOn())
            return;

        ic.LFPG_ExecuteRFToggle();

        PlayerBase pb = PlayerBase.Cast(action_data.m_Player);
        if (pb)
        {
            string msg = "[LFPG] RF signal sent";
            pb.MessageStatus(msg);
        }
    }
};
