// =========================================================
// LF_PowerGrid - Action: Watch Monitor (v0.9.4 - Wire check fix)
//
// Replaces LFPG_ActionViewCamera (v0.9.0).
// Server-authoritative camera list via RPC.
//
// Aparece al mirar un LF_Monitor que:
//   - este encendido (m_PoweredNet = true)
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
//
// v0.9.3 (Bug 2 fix): Changed parent from ActionSingleUseBase to ActionInteractBase.
// v0.9.4 (Bug fix): Removed client-side HasWireStore/GetWires/Count check.
//   m_Wires is populated server-side only (persistence + wiring RPCs).
//   BroadcastOwnerWires sends data to CableRenderer, NOT to entity m_Wires.
//   On the client, monitor.LFPG_GetWires().Count() is ALWAYS 0 → action
//   never appeared. Server already validates wires + powered cameras in
//   HandleLFPG_RequestCameraList — client check was redundant and broken.
// =========================================================

class LFPG_ActionWatchMonitor : ActionInteractBase
{
    void LFPG_ActionWatchMonitor()
    {
        // No command animation — camera view is instant.
        // CMD_ACTIONMOD_INTERACTONCE causes native crash on LF_Monitor
        // (Inventory_Base in world, not BaseBuildingBase with anim support).
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

        string monitorType = "LF_Monitor";
        if (!targetObj.IsKindOf(monitorType))
            return false;

        // DistSq antes del Cast: falla rapido si el jugador esta lejos.
        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), targetObj.GetPosition());
        if (distSq > LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M)
            return false;

        LF_Monitor monitor = LF_Monitor.Cast(targetObj);
        if (!monitor)
            return false;

        // Monitor debe estar encendido (m_PoweredNet se replica via SyncVar)
        if (!monitor.LFPG_IsPowered())
            return false;

        // v0.9.4: Wire/camera validation is server-authoritative.
        // HandleLFPG_RequestCameraList checks wires, resolves cameras,
        // filters powered ones, and sends appropriate error messages
        // if no cameras are connected or active.

        // No mostrar si el viewport ya esta activo
        LFPG_CameraViewport vp = LFPG_CameraViewport.Get();
        if (vp && vp.IsActive())
            return false;

        return true;
    }

    // Client: enviar RPC pidiendo la lista de camaras al servidor.
    // v0.9.5: Moved from OnExecuteClient to OnStartClient.
    // OnExecuteClient fires AFTER the CMD_ACTIONMOD_INTERACTONCE animation.
    // The animation command was causing a native crash on LF_Monitor
    // (Inventory_Base placed in world — not a BaseBuildingBase with
    // proper animation support). OnStartClient fires BEFORE the command.
    override void OnStartClient(ActionData action_data)
    {
        super.OnStartClient(action_data);

        if (!action_data.m_Target)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        string monitorCheck = "LF_Monitor";
        if (!targetObj.IsKindOf(monitorCheck))
            return;

        int netLow  = 0;
        int netHigh = 0;
        targetObj.GetNetworkID(netLow, netHigh);

        ScriptRPC rpc = new ScriptRPC();
        int subId = LFPG_RPC_SubId.REQUEST_CAMERA_LIST;
        rpc.Write(subId);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Send(action_data.m_Player, LFPG_RPC_CHANNEL, true, null);
    }

    // Server: toda la logica esta en PlayerRPC handler.
    override void OnExecuteServer(ActionData action_data) {}
};
