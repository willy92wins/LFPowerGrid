// =========================================================
// LF_PowerGrid - Action: Pair/Unpair Remote Controller (v1.0.0)
//
// Toggle pair: hold Remote Controller in hand + look at RF device.
//   If not paired → pair (store DeviceId + position).
//   If already paired → unpair (remove from list).
//
// Visual: button_1 press animation + red LED flash (~2s).
// Sound: reuse RF beep from intercom.
//
// Conditions:
//   - Item in hands must be LFPG_RemoteController
//   - Target must be RF-capable (LFPG_IsRFCapable)
//   - Within interact distance
//
// Register in ActionConstructor.RegisterActions().
// =========================================================

class LFPG_ActionPairRemote : ActionSingleUseBase
{
    void LFPG_ActionPairRemote()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_PAIR_REMOTE";
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

        // Must have Remote Controller in hands
        if (!item)
            return false;

        LFPG_RemoteController rc = LFPG_RemoteController.Cast(item);
        if (!rc)
            return false;

        // Must target an object
        if (!target)
            return false;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return false;

        EntityAI targetEnt = EntityAI.Cast(targetObj);
        if (!targetEnt)
            return false;

        // Target must be RF-capable
        if (!LFPG_DeviceAPI.IsRFCapable(targetEnt))
            return false;

        // Distance check
        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), targetEnt.GetPosition());
        float maxDistSq = LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M;
        if (distSq > maxDistSq)
            return false;

        // Dynamic text: Pair / Unpair based on current state
        string devId = LFPG_DeviceAPI.GetOrCreateDeviceId(targetEnt);
        if (rc.LFPG_IsPaired(devId))
        {
            m_Text = "#STR_LFPG_ACTION_UNPAIR_REMOTE";
        }
        else
        {
            m_Text = "#STR_LFPG_ACTION_PAIR_REMOTE";
        }

        return true;
    }

    override bool HasTarget()
    {
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

        if (!action_data.m_Target)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        EntityAI targetEnt = EntityAI.Cast(targetObj);
        if (!targetEnt)
            return;

        string devId = LFPG_DeviceAPI.GetOrCreateDeviceId(targetEnt);
        if (devId == "")
            return;

        PlayerBase pb = PlayerBase.Cast(action_data.m_Player);

        if (rc.LFPG_IsPaired(devId))
        {
            // Unpair
            rc.LFPG_UnpairDevice(devId);

            if (pb)
            {
                string typeName = targetEnt.GetType();
                string offMsg = "[LFPG] Unpaired: ";
                offMsg = offMsg + typeName;
                offMsg = offMsg + " (";
                offMsg = offMsg + rc.LFPG_GetPairedCount().ToString();
                offMsg = offMsg + " remaining)";
                pb.MessageStatus(offMsg);
            }
        }
        else
        {
            // Pair
            vector devPos = targetEnt.GetPosition();
            rc.LFPG_PairDevice(devId, devPos);

            if (pb)
            {
                string typeName2 = targetEnt.GetType();
                string onMsg = "[LFPG] Paired: ";
                onMsg = onMsg + typeName2;
                onMsg = onMsg + " (";
                onMsg = onMsg + rc.LFPG_GetPairedCount().ToString();
                onMsg = onMsg + " total)";
                pb.MessageStatus(onMsg);
            }
        }

        // Visual feedback: button 1 press + red LED flash
        rc.LFPG_FlashLED1Red();

        // Sound feedback
        vector rcPos = rc.GetPosition();
        SEffectManager.PlaySound(LFPG_INTERCOM_SND_KNOB_CLICK, rcPos);

        // Sync updated paired list to client for dynamic action text
        if (pb)
        {
            PlayerIdentity identity = pb.GetIdentity();
            if (identity)
            {
                rc.LFPG_SyncPairedListToClient(identity);
            }
        }
    }
};
