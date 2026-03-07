// =========================================================
// LF_PowerGrid - Wash Hands at Water Pump (v1.1.0 Sprint W2)
//
// Parity with vanilla ActionWashHandsWell.
// CCINone + CCTObject. T1: powered. T2: powered OR tank > 0.
// [FIX-11] Full agent cleanup logic matching vanilla.
// =========================================================

class LFPG_ActionWashHandsPumpCB : ActionContinuousBaseCB
{
    override void CreateActionComponent()
    {
        m_ActionData.m_ActionComponent = new CAContinuousRepeat(UATimeSpent.WASH_HANDS);
    }
};

class LFPG_ActionWashHandsPump : ActionContinuousBase
{
    void LFPG_ActionWashHandsPump()
    {
        m_CallbackClass = LFPG_ActionWashHandsPumpCB;
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_WASHHANDSWELL;
        m_FullBody = true;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_CROUCH;
        m_Text = "#STR_LFPG_ACTION_WASH_PUMP";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem = new CCINone;
        m_ConditionTarget = new CCTObject(UAMaxDistances.DEFAULT);
    }

    override typename GetInputType()
    {
        return ContinuousInteractActionInput;
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!player || !target)
            return false;

        if (!player.HasBloodyHands())
            return false;

        // Must have empty hands
        if (player.GetItemInHands())
            return false;

        // Must not have gloves (vanilla parity)
        EntityAI gloves = player.GetItemOnSlot("Gloves");
        if (gloves)
            return false;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return false;

        LF_WaterPump pump1 = LF_WaterPump.Cast(targetObj);
        if (pump1)
        {
            return pump1.LFPG_GetPoweredNet();
        }

        LF_WaterPump_T2 pump2 = LF_WaterPump_T2.Cast(targetObj);
        if (pump2)
        {
            if (pump2.LFPG_GetPoweredNet())
                return true;

            float tankLvl = pump2.LFPG_GetTankLevel();
            if (tankLvl > 0.0)
                return true;
        }

        return false;
    }

    override void OnFinishProgressServer(ActionData action_data)
    {
        if (!action_data || !action_data.m_Target || !action_data.m_Player)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        PlayerBase player = action_data.m_Player;
        EntityAI targetEnt = EntityAI.Cast(targetObj);
        if (!targetEnt)
            return;

        bool powered = false;
        int tankLiquidType = 0;

        LF_WaterPump pump1 = LF_WaterPump.Cast(targetObj);
        LF_WaterPump_T2 pump2 = LF_WaterPump_T2.Cast(targetObj);

        if (pump1)
        {
            powered = pump1.LFPG_GetPoweredNet();
        }
        if (pump2)
        {
            powered = pump2.LFPG_GetPoweredNet();
            tankLiquidType = pump2.LFPG_GetTankLiquidType();
        }

        int liquidType = LFPG_PumpHelper.DetermineLiquidType(targetEnt, powered, tankLiquidType);

        // [FIX-11] Full agent cleanup (vanilla parity)
        PluginLifespan lifespan = PluginLifespan.Cast(GetPlugin(PluginLifespan));
        if (lifespan)
        {
            lifespan.UpdateBloodyHandsVisibility(player, false);
        }

        if (liquidType == LIQUID_CLEANWATER)
        {
            player.ClearBloodyHandsPenaltyChancePerAgent(eAgents.SALMONELLA);
            player.ClearBloodyHandsPenaltyChancePerAgent(eAgents.CHOLERA);
        }
        // River water: salmonella cleared but cholera risk remains (vanilla behavior)

        // T2 unpowered: decrement tank
        if (pump2 && !powered)
        {
            float level = pump2.LFPG_GetTankLevel();
            level = level - LFPG_PUMP_TANK_WASH_COST;
            if (level < 0.0)
            {
                level = 0.0;
            }
            pump2.LFPG_SetTankLevel(level);
        }

        // Play water sound
        SEffectManager.PlaySound(LFPG_PUMP_WATER_SOUNDSET, targetEnt.GetPosition());
    }
};
