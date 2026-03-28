// =========================================================
// LF_PowerGrid - Action: Activate Remote Controller (v1.0.0)
//
// Hold Remote Controller in hand → trigger toggle on ALL
// paired RF switches within configured range.
//
// No external target needed (CCTNone).
// Auto-unpairs devices that moved or no longer exist.
// Cooldown: LFPG_REMOTE_COOLDOWN_MS between activations.
//
// Visual: button_2 press animation + green LED flash (~2s).
//
// Conditions:
//   - Item in hands must be LFPG_RemoteController
//   - At least 1 paired device
//
// Register in ActionConstructor.RegisterActions().
// =========================================================

class LFPG_ActionActivateRemote : ActionInteractBase
{
    void LFPG_ActionActivateRemote()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_ACTIVATE_REMOTE";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem   = new CCINone;
        m_ConditionTarget = new CCTNone;
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!player)
            return false;

        // Must have Remote Controller in hands
        if (!item)
            return false;

        LFPG_RemoteController rc = LFPG_RemoteController.Cast(item);
        if (!rc)
            return false;

        // Must have at least one paired device
        if (rc.LFPG_GetPairedCount() < 1)
            return false;

        return true;
    }

    override void OnExecuteServer(ActionData action_data)
    {
        super.OnExecuteServer(action_data);

        if (!action_data)
            return;

        ItemBase handItem = action_data.m_MainItem;
        if (!handItem)
            return;

        LFPG_RemoteController rc = LFPG_RemoteController.Cast(handItem);
        if (!rc)
            return;

        PlayerBase pb = PlayerBase.Cast(action_data.m_Player);
        if (!pb)
            return;

        vector playerPos = pb.GetPosition();
        int toggled = rc.LFPG_ActivateToggle(playerPos);

        if (toggled < 0)
        {
            // Cooldown active
            string cdMsg = "[LFPG] Remote: cooldown active";
            pb.MessageStatus(cdMsg);
        }
        else if (toggled == 0)
        {
            string noneMsg = "[LFPG] Remote: no switches in range";
            pb.MessageStatus(noneMsg);
        }
        else
        {
            string okMsg = "[LFPG] Remote: ";
            okMsg = okMsg + toggled.ToString();
            okMsg = okMsg + " switch(es) toggled";
            pb.MessageStatus(okMsg);
        }

        // Visual feedback: button 2 press + green LED flash
        rc.LFPG_FlashLED2Green();

        // Sound feedback
        vector rcPos = rc.GetPosition();
        SEffectManager.PlaySound(LFPG_INTERCOM_SND_RF_BEEP, rcPos);
    }
};
