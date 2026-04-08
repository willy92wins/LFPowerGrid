// =========================================================
// LF_PowerGrid - Actions: Toggle Electric Stove Burners (v1.0.1)
//
// Base class + 4 subclasses (one per burner index 0..3).
// CCTCursor + manual DistSq (same pattern as Furnace toggle).
// Dynamic text: "Turn On Top-Left" / "Turn Off Top-Left" etc.
//
// No item required (CCINone). Target: LFPG_ElectricStove.
// =========================================================

class LFPG_ActionToggleBurnerBase : ActionInteractBase
{
    protected int m_BurnerIndex;

    void LFPG_ActionToggleBurnerBase()
    {
        m_BurnerIndex = 0;
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_TOGGLE_BURNER";
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

        LFPG_ElectricStove stove = LFPG_ElectricStove.Cast(targetObj);
        if (!stove)
            return false;

        // Manual proximity check
        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), stove.GetPosition());
        float maxSq = LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M;
        if (distSq > maxSq)
            return false;

        // Dynamic text based on burner state
        bool isOn = stove.LFPG_IsBurnerOn(m_BurnerIndex);
        string posLabel = LFPG_GetPositionLabel();

        if (isOn)
        {
            string offText = "Turn Off ";
            offText = offText + posLabel;
            m_Text = offText;
        }
        else
        {
            string onText = "Turn On ";
            onText = onText + posLabel;
            m_Text = onText;
        }

        return true;
    }

    override void OnExecuteServer(ActionData action_data)
    {
        super.OnExecuteServer(action_data);

        if (!action_data || !action_data.m_Target)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        LFPG_ElectricStove stove = LFPG_ElectricStove.Cast(targetObj);
        if (!stove)
            return;

        stove.LFPG_ToggleBurner(m_BurnerIndex);
    }

    protected string LFPG_GetPositionLabel()
    {
        // v1.0.1: Labels corrected to match physical button positions
        // in electric_stove.p3d (verified via py3d Memory LOD coords).
        // button_1: X-neg Z-neg = Left-Front  = Bottom-Left
        // button_2: X-neg Z-pos = Left-Back   = Top-Left
        // button_3: X-pos Z-pos = Right-Back  = Top-Right
        // button_4: X-pos Z-neg = Right-Front = Bottom-Right
        if (m_BurnerIndex == 0)
            return "Bottom-Left";
        if (m_BurnerIndex == 1)
            return "Top-Left";
        if (m_BurnerIndex == 2)
            return "Top-Right";
        if (m_BurnerIndex == 3)
            return "Bottom-Right";
        return "Burner";
    }
};

// ---------------------------------------------------------
// Subclasses (one per burner)
// ---------------------------------------------------------
class LFPG_ActionToggleBurner0 : LFPG_ActionToggleBurnerBase
{
    void LFPG_ActionToggleBurner0()
    {
        m_BurnerIndex = 0;
    }
};

class LFPG_ActionToggleBurner1 : LFPG_ActionToggleBurnerBase
{
    void LFPG_ActionToggleBurner1()
    {
        m_BurnerIndex = 1;
    }
};

class LFPG_ActionToggleBurner2 : LFPG_ActionToggleBurnerBase
{
    void LFPG_ActionToggleBurner2()
    {
        m_BurnerIndex = 2;
    }
};

class LFPG_ActionToggleBurner3 : LFPG_ActionToggleBurnerBase
{
    void LFPG_ActionToggleBurner3()
    {
        m_BurnerIndex = 3;
    }
};
