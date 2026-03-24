// =========================================================
// LF_PowerGrid - Action: Cycle Frequency (v3.0.0 - Sprint 3)
//
// Cycles through 7 vanilla radio frequencies on the Intercom.
// Updates knob_freq animation and ghost radio frequency.
//
// Frequencies: 87.8, 89.5, 91.3, 94.6, 96.6, 99.7, 102.5 MHz
//
// Conditions:
//   - Target must be LFPG_Intercom
//   - m_RadioInstalled == true (T2 upgrade completed)
//
// IMPORTANTE: Registrar en ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionCycleFrequency).
// =========================================================

class LFPG_ActionCycleFrequency : ActionInteractBase
{
    void LFPG_ActionCycleFrequency()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_CYCLE_FREQ";
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

        LFPG_Intercom ic = LFPG_Intercom.Cast(targetObj);
        if (!ic)
            return false;

        // T2 only — radio must be installed
        if (!ic.LFPG_GetRadioInstalled())
            return false;

        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), ic.GetPosition());
        float maxDistSq = LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M;
        if (distSq > maxDistSq)
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

        LFPG_Intercom ic = LFPG_Intercom.Cast(targetObj);
        if (!ic)
            return;

        ic.LFPG_CycleFrequency();

        PlayerBase pb = PlayerBase.Cast(action_data.m_Player);
        if (pb)
        {
            int freqIdx = ic.LFPG_GetFrequencyIndex();
            string freqMsg = "[LFPG] Frequency: ";
            freqMsg = freqMsg + LFPG_GetFrequencyName(freqIdx);
            freqMsg = freqMsg + " MHz";
            pb.MessageStatus(freqMsg);
        }
    }

    // Helper: get frequency display name by index
    static string LFPG_GetFrequencyName(int idx)
    {
        if (idx == 0) return "87.8";
        if (idx == 1) return "89.5";
        if (idx == 2) return "91.3";
        if (idx == 3) return "94.6";
        if (idx == 4) return "96.6";
        if (idx == 5) return "99.7";
        if (idx == 6) return "102.5";
        return "87.8";
    }
};
