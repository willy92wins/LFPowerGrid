// =========================================================
// LF_PowerGrid - Action: Operate Searchlight (v1.5.0)
//
// Toggle action on LF_Searchlight (CCINone, CCTCursor).
//   - If NOT grabbing: "Operate Searchlight" -> sends ENTER RPC
//   - If already grabbing THIS searchlight: "Release Searchlight" -> local exit
//
// Appears when player looks at a powered searchlight within
// interact distance. No item required (bare hands).
//
// Enforce Script: no ternaries, no ++/--, no foreach.
// =========================================================

class LFPG_ActionOperateSearchlight : ActionInteractBase
{
    void LFPG_ActionOperateSearchlight()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text       = "#STR_LFPG_ACTION_OPERATE_SEARCHLIGHT";
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

        string slType = "LF_Searchlight";
        if (!targetObj.IsKindOf(slType))
            return false;

        LF_Searchlight sl = LF_Searchlight.Cast(targetObj);
        if (!sl)
            return false;

        if (!sl.LFPG_IsPowered())
            return false;

        // Distance check
        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), targetObj.GetPosition());
        float maxDist = LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M;
        if (distSq > maxDist)
            return false;

        // Block if CCTV viewport is active
        LFPG_CameraViewport vp = LFPG_CameraViewport.Get();
        if (vp && vp.IsActive())
            return false;

        // Toggle text based on current state
        LFPG_SearchlightController ctrl = LFPG_SearchlightController.Get();
        if (ctrl && ctrl.IsOperatingEntity(sl))
        {
            m_Text = "#STR_LFPG_ACTION_RELEASE_SEARCHLIGHT";
        }
        else
        {
            // Block if already operating a DIFFERENT searchlight
            if (ctrl && ctrl.IsActive())
                return false;

            m_Text = "#STR_LFPG_ACTION_OPERATE_SEARCHLIGHT";
        }

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

        LF_Searchlight sl = LF_Searchlight.Cast(targetObj);
        if (!sl)
            return;

        // Toggle: if already operating, release; otherwise request enter
        LFPG_SearchlightController ctrl = LFPG_SearchlightController.Get();
        if (ctrl && ctrl.IsOperatingEntity(sl))
        {
            // Release
            ctrl.RequestExit();
            return;
        }

        // Request enter via RPC
        int netLow  = 0;
        int netHigh = 0;
        targetObj.GetNetworkID(netLow, netHigh);

        ScriptRPC rpc = new ScriptRPC();
        int subId = LFPG_RPC_SubId.SEARCHLIGHT_ENTER;
        rpc.Write(subId);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Send(action_data.m_Player, LFPG_RPC_CHANNEL, true, null);
    }

    override void OnExecuteServer(ActionData action_data) {}
};
