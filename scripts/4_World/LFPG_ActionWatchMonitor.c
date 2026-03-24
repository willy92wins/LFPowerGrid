// =========================================================
// LF_PowerGrid - Action: Watch Monitor (v0.9.6 - Crash fix)
//
// Replaces LFPG_ActionViewCamera (v0.9.0).
// Server-authoritative camera list via RPC.
//
// Aparece al mirar un LFPG_Monitor que:
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
// Registrar en LFPG_Monitor.SetActions() y LFPG_ActionRegistration.
//
// v0.9.3 (Bug 2 fix): Changed parent from ActionSingleUseBase to ActionInteractBase.
// v0.9.4 (Bug fix): Removed client-side HasWireStore/GetWires/Count check.
//   m_Wires is populated server-side only (persistence + wiring RPCs).
//   BroadcastOwnerWires sends data to CableRenderer, NOT to entity m_Wires.
//   On the client, monitor.LFPG_GetWires().Count() is ALWAYS 0 → action
//   never appeared. Server already validates wires + powered cameras in
//   HandleLFPG_RequestCameraList — client check was redundant and broken.
// v0.9.6 (Crash fix): Reverted RPC send from OnStartClient back to OnExecuteClient.
//   OnStartClient fires BEFORE the animation command completes. The RPC response
//   (CAMERA_LIST_RESPONSE) could arrive while the action is still in-flight,
//   causing Camera.SetActive(true) to execute during ActionManager processing
//   → native C++ crash. OnExecuteClient fires AFTER the animation completes,
//   ensuring the action pipeline is fully resolved before the viewport activates.
//   Restored m_CommandUID = CMD_ACTIONMOD_INTERACTONCE (required for OnExecuteClient).
// =========================================================

class LFPG_ActionWatchMonitor : ActionInteractBase
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

        string monitorType = "LFPG_Monitor";
        if (!targetObj.IsKindOf(monitorType))
            return false;

        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), targetObj.GetPosition());
        if (distSq > LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M)
            return false;

        LFPG_Monitor monitor = LFPG_Monitor.Cast(targetObj);
        if (!monitor)
            return false;

        if (!monitor.LFPG_IsPowered())
            return false;

        LFPG_CameraViewport vp = LFPG_CameraViewport.Get();
        if (vp && vp.IsActive())
            return false;

        // v1.4.0: block if searchlight spectator is active
        LFPG_SearchlightController slCtrl = LFPG_SearchlightController.Get();
        if (slCtrl && slCtrl.IsActive())
            return false;

        return true;
    }

    // v0.9.6: RPC en OnExecuteClient — se dispara DESPUES de que la animacion
    // de interact termina. La respuesta CAMERA_LIST_RESPONSE llega cuando la
    // action ya no esta activa, evitando el crash nativo al llamar
    // Camera.SetActive(true) durante el procesamiento del ActionManager.
    override void OnExecuteClient(ActionData action_data)
    {
        super.OnExecuteClient(action_data);

        if (!action_data.m_Target)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        string monitorCheck = "LFPG_Monitor";
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

    override void OnExecuteServer(ActionData action_data) {}
};
