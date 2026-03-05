// =========================================================
// LF_PowerGrid - Action: Unlink camera from a Monitor (v0.9.0 - Etapa 2)
//
// Aparece al mirar un LF_Monitor que TENGA una camara enlazada.
// No requiere item en mano.
//
// Registrar en LFPG_ActionRegistration.RegisterActions():
//   actions.Insert(LFPG_ActionUnlinkCamera);
// =========================================================

class LFPG_ActionUnlinkCamera : ActionSingleUseBase
{
    void LFPG_ActionUnlinkCamera()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_UNLINK_CAMERA";
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

        if (!targetObj.IsKindOf("LF_Monitor"))
            return false;

        // DistSq antes del Cast: falla rapido si el jugador esta lejos.
        // Patron consistente con ToggleSource, CutWires y resto de actions.
        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), targetObj.GetPosition());
        if (distSq > LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M)
            return false;

        // Solo aparece cuando el monitor tiene una camara enlazada.
        LF_Monitor mon = LF_Monitor.Cast(targetObj);
        if (!mon)
            return false;

        if (mon.LFPG_GetLinkedCameraId() == "")
            return false;

        return true;
    }

    override void OnExecuteClient(ActionData action_data)
    {
        super.OnExecuteClient(action_data);

        if (!action_data.m_Target)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        if (!targetObj.IsKindOf("LF_Monitor"))
            return;

        int netLow  = 0;
        int netHigh = 0;
        targetObj.GetNetworkID(netLow, netHigh);

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.CAMERA_UNLINK);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Send(action_data.m_Player, LFPG_RPC_CHANNEL, true, null);
    }

    override void OnExecuteServer(ActionData action_data) {}
};
