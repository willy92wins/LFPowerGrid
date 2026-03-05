// =========================================================
// LF_PowerGrid - Action: Watch Monitor (v0.9.2 - Sprint B)
//
// Replaces LFPG_ActionViewCamera (v0.9.0).
// Server-authoritative camera list via RPC.
//
// Aparece al mirar un LF_Monitor que:
//   - este encendido (m_PoweredNet = true)
//   - tenga al menos un wire (OUT ports)
//   - el viewport NO este ya activo
//
// No requiere item en mano (CCINone).
//
// Flujo:
//   Client: envía REQUEST_CAMERA_LIST(monitorNetLow, monitorNetHigh)
//   Server: resuelve monitor → filtra cameras powered → responde
//   Client: recibe CAMERA_LIST_RESPONSE → CameraViewport.EnterFromList
//
// Registrar en LF_Monitor.SetActions() y LFPG_ActionRegistration.
// =========================================================

class LFPG_ActionWatchMonitor : ActionSingleUseBase
{
    void LFPG_ActionWatchMonitor()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text       = "#STR_LFPG_ACTION_VIEW_CAMERA";
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
        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), targetObj.GetPosition());
        if (distSq > LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M)
            return false;

        LF_Monitor monitor = LF_Monitor.Cast(targetObj);
        if (!monitor)
            return false;

        // Monitor debe estar encendido
        if (!monitor.LFPG_IsPowered())
            return false;

        // Monitor debe tener wire store con al menos un cable
        if (!monitor.LFPG_HasWireStore())
            return false;

        array<ref LFPG_WireData> wires = monitor.LFPG_GetWires();
        if (!wires)
            return false;
        if (wires.Count() == 0)
            return false;

        // No mostrar si el viewport ya esta activo
        LFPG_CameraViewport vp = LFPG_CameraViewport.Get();
        if (vp && vp.IsActive())
            return false;

        return true;
    }

    // Client: enviar RPC pidiendo la lista de camaras al servidor.
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
        rpc.Write((int)LFPG_RPC_SubId.REQUEST_CAMERA_LIST);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Send(action_data.m_Player, LFPG_RPC_CHANNEL, true, null);
    }

    // Server: toda la logica esta en PlayerRPC handler.
    override void OnExecuteServer(ActionData action_data) {}
};
