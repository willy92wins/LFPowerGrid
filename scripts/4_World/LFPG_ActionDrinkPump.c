// =========================================================
// LF_PowerGrid - Drink from Water Pump (v1.1.0 Sprint W2)
//
// Continuous action with CAContinuousRepeat (parity with ActionDrinkWellContinuous).
// Registered on T1 and T2 SetActions. CCINone = empty hands.
// T1: requires powered. T2: powered OR tank > 0.
// =========================================================

class LFPG_ActionDrinkPumpCB : ActionContinuousBaseCB
{
    override void CreateActionComponent()
    {
        m_ActionData.m_ActionComponent = new CAContinuousRepeat(UATimeSpent.DRINK_WELL);
    }
};

class LFPG_ActionDrinkPump : ActionContinuousBase
{
    void LFPG_ActionDrinkPump()
    {
        m_CallbackClass = LFPG_ActionDrinkPumpCB;
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_DRINKWELL;
        m_FullBody = true;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_CROUCH;
        m_Text = "#STR_LFPG_ACTION_DRINK_PUMP";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem = new CCINone;
        m_ConditionTarget = new CCTCursor(UAMaxDistances.DEFAULT);
    }

    override typename GetInputType()
    {
        return ContinuousInteractActionInput;
    }

    override bool IsDrink()
    {
        return true;
    }

    override bool IsLockTargetOnUse()
    {
        return false;
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!player || !target)
            return false;

        if (!player.CanEatAndDrink())
            return false;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return false;

        // T1 check
        LF_WaterPump pump1 = LF_WaterPump.Cast(targetObj);
        if (pump1)
        {
            return pump1.LFPG_GetPoweredNet();
        }

        // T2 check: powered OR tank > 0
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

    override void OnStart(ActionData action_data)
    {
        super.OnStart(action_data);
        if (action_data.m_Player)
        {
            action_data.m_Player.TryHideItemInHands(true);
        }

        // Play water.ogg on client (SEffectManager is client-side only)
        #ifndef SERVER
        Object sndTarget = action_data.m_Target.GetObject();
        if (sndTarget)
        {
            EntityAI sndEnt = EntityAI.Cast(sndTarget);
            if (sndEnt)
            {
                SEffectManager.PlaySound(LFPG_PUMP_WATER_SOUNDSET, sndEnt.GetPosition());
            }
        }
        #endif
    }

    override void OnEnd(ActionData action_data)
    {
        if (action_data.m_Player)
        {
            action_data.m_Player.TryHideItemInHands(false);
        }
        super.OnEnd(action_data);
    }

    override void OnFinishProgressServer(ActionData action_data)
    {
        if (!action_data || !action_data.m_Target || !action_data.m_Player)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        PlayerBase player = action_data.m_Player;

        // Determine liquid type and powered state
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

        EntityAI targetEnt = EntityAI.Cast(targetObj);
        if (!targetEnt)
            return;

        int liquidType = LFPG_PumpHelper.DetermineLiquidType(targetEnt, powered, tankLiquidType);

        // Determine consume type (parity with vanilla well)
        int consumeType = EConsumeType.ENVIRO_POND;
        if (liquidType == LIQUID_CLEANWATER)
        {
            consumeType = EConsumeType.ENVIRO_WELL;
        }

        // Consume
        PlayerConsumeData consumeData = new PlayerConsumeData;
        consumeData.m_Type = consumeType;
        consumeData.m_Amount = UAQuantityConsumed.DRINK;
        consumeData.m_Source = null;
        consumeData.m_Agents = player.GetBloodyHandsPenaltyAgents();
        consumeData.m_LiquidType = liquidType;
        player.Consume(consumeData);

        // T2 unpowered: decrement tank
        if (pump2 && !powered)
        {
            float level = pump2.LFPG_GetTankLevel();
            level = level - LFPG_PUMP_TANK_DRINK_COST;
            if (level < 0.0)
            {
                level = 0.0;
            }
            pump2.LFPG_SetTankLevel(level);
        }
    }
};
