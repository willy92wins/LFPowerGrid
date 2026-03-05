// =========================================================
// LF_PowerGrid - Action: Ver camara enlazada a Monitor (v0.9.0 - Etapa 3)
//
// Aparece al mirar un LF_Monitor que:
//   - este encendido (m_PoweredNet = true)
//   - tenga una camara enlazada (LFPG_GetLinkedCameraId() != "")
//   - la camara enlazada tenga energia
//   - el viewport NO este ya activo
//
// No requiere item en mano (CCINone).
// Totalmente client-side: OnExecuteServer es vacio.
// La logica de validacion duplicada (servidor) no es necesaria
// porque no hay estado de servidor que modificar.
//
// Registrar en LFPG_ActionRegistration.RegisterActions():
//   actions.Insert(LFPG_ActionViewCamera);
// =========================================================

class LFPG_ActionViewCamera : ActionSingleUseBase
{
    void LFPG_ActionViewCamera()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text       = "#STR_LFPG_ACTION_VIEW_CAMERA";
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

        // Solo funciona sobre LF_Monitor
        if (!targetObj.IsKindOf("LF_Monitor"))
            return false;

        // DistSq antes del Cast: falla rapido si el jugador esta lejos.
        // Patron consistente con ToggleSource, CutWires, ActionCycleCamera.
        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), targetObj.GetPosition());
        if (distSq > LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M)
            return false;

        LF_Monitor monitor = LF_Monitor.Cast(targetObj);
        if (!monitor)
            return false;

        // Monitor debe estar encendido
        if (!monitor.LFPG_IsPowered())
            return false;

        // Monitor debe tener al menos una camara cableada (wire store).
        // TODO Sprint B: reemplazar por RPC REQUEST_CAMERA_LIST flow.
        if (!monitor.LFPG_HasWireStore())
            return false;

        array<ref LFPG_WireData> wires = monitor.LFPG_GetWires();
        if (!wires)
            return false;
        if (wires.Count() == 0)
            return false;

        // No mostrar si el viewport ya esta activo
        // (Get() devuelve null en servidor dedicado → condicion segura)
        LFPG_CameraViewport vp = LFPG_CameraViewport.Get();
        if (vp && vp.IsActive())
            return false;

        return true;
    }

    // Client: delegar al gestor de viewport.
    // Toda la validacion visual y el cambio de POV ocurren aqui.
    override void OnExecuteClient(ActionData action_data)
    {
        super.OnExecuteClient(action_data);

        if (!action_data.m_Target)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        LF_Monitor monitor = LF_Monitor.Cast(targetObj);
        if (!monitor)
            return;

        LFPG_CameraViewport vp = LFPG_CameraViewport.Get();
        if (vp)
            vp.Enter(monitor);
    }

    // Server: sin logica (accion totalmente client-side).
    override void OnExecuteServer(ActionData action_data) {}
}
