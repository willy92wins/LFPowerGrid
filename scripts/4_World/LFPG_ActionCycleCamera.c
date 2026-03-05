// =========================================================
// LF_PowerGrid - Action: Cycle camera source for a Monitor (v0.9.0 - Etapa 2)
//
// Aparece al mirar un LF_Monitor. Sin item requerido en mano.
// Cicla entre todas las LF_Camera del servidor ordenadas por
// distancia al monitor (mas cercana primero).
//
// Flujo:
//   Client: GetNetworkID(monitor) → RPC CAMERA_CYCLE → server
//   Server: recopila cameras, avanza indice, llama LFPG_SetLinkedCamera
//   Monitor: SyncVars replican → OnVariablesSynchronized actualiza estado
//
// Registrar en LFPG_ActionRegistration.RegisterActions():
//   actions.Insert(LFPG_ActionCycleCamera);
// =========================================================

class LFPG_ActionCycleCamera : ActionSingleUseBase
{
    void LFPG_ActionCycleCamera()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_CYCLE_CAMERA";
    }

    override void CreateConditionComponents()
    {
        // CCINone: no item requerido en mano.
        // CCTCursor: el jugador debe mirar el objeto objetivo.
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

        // DistSq: evita sqrt en per-frame condition check.
        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), targetObj.GetPosition());
        if (distSq > LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M)
            return false;

        // CAM-04: solo mostrar si el monitor tiene alimentacion.
        LF_Monitor mon = LF_Monitor.Cast(targetObj);
        if (!mon)
            return false;

        if (!mon.LFPG_IsPowered())
            return false;

        // CAM-07: no mostrar mientras el jugador este viendo el feed CCTV.
        // Get() devuelve null en servidor dedicado → condicion segura.
        LFPG_CameraViewport vp = LFPG_CameraViewport.Get();
        if (vp && vp.IsActive())
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
        rpc.Write((int)LFPG_RPC_SubId.CAMERA_CYCLE);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Send(action_data.m_Player, LFPG_RPC_CHANNEL, true, null);
    }

    // Toda la logica es server-authoritative via RPC.
    override void OnExecuteServer(ActionData action_data) {}
};
