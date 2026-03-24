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
    // Client-side sound ref (static OK — only 1 local player)
    protected static EffectSound m_DrinkSound;

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

        // T1 check: requires verified power, blocked if sprinkler connected
        LFPG_WaterPump pump1 = LFPG_WaterPump.Cast(targetObj);
        if (pump1)
        {
            // v5.1: T1 water goes entirely to sprinkler — no drinking
            if (pump1.LFPG_GetHasSprinklerOutput())
                return false;

            EntityAI ent1 = EntityAI.Cast(targetObj);
            return LFPG_PumpHelper.VerifyPowered(ent1);
        }

        // T2 check: verified power OR tank > 0
        LFPG_WaterPump_T2 pump2 = LFPG_WaterPump_T2.Cast(targetObj);
        if (pump2)
        {
            EntityAI ent2 = EntityAI.Cast(targetObj);
            bool t2Powered = LFPG_PumpHelper.VerifyPowered(ent2);

            if (t2Powered)
            {
                // v5.1: 3+ sprinklers with empty tank → net drain, no water left
                int sprCnt = pump2.LFPG_GetConnectedSprinklerCount();
                if (sprCnt >= 3)
                {
                    float tankCheck = pump2.LFPG_GetTankLevel();
                    if (tankCheck <= 0.0)
                        return false;
                }
                return true;
            }

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

        #ifndef SERVER
        // Stop any previous sound
        if (m_DrinkSound)
        {
            m_DrinkSound.SoundStop();
            m_DrinkSound = null;
        }
        Object sndTarget = action_data.m_Target.GetObject();
        if (sndTarget)
        {
            EntityAI sndEnt = EntityAI.Cast(sndTarget);
            if (sndEnt)
            {
                m_DrinkSound = SEffectManager.PlaySound(LFPG_PUMP_WATER_SOUNDSET, sndEnt.GetPosition());
                if (m_DrinkSound)
                {
                    m_DrinkSound.SetAutodestroy(false);
                }
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

        #ifndef SERVER
        if (m_DrinkSound)
        {
            m_DrinkSound.SoundStop();
            m_DrinkSound = null;
        }
        #endif

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

        EntityAI targetEnt = EntityAI.Cast(targetObj);
        if (!targetEnt)
            return;

        LFPG_WaterPump pump1 = LFPG_WaterPump.Cast(targetObj);
        LFPG_WaterPump_T2 pump2 = LFPG_WaterPump_T2.Cast(targetObj);

        if (pump1)
        {
            powered = LFPG_PumpHelper.VerifyPowered(targetEnt);
        }
        if (pump2)
        {
            powered = LFPG_PumpHelper.VerifyPowered(targetEnt);
            tankLiquidType = pump2.LFPG_GetTankLiquidType();
        }

        // Server-side authority: abort if no valid water source
        // T1: requires power (no tank). T2: requires power OR tank > 0.
        if (pump1 && !powered)
            return;

        if (pump2 && !powered)
        {
            float srvTankLvl = pump2.LFPG_GetTankLevel();
            if (srvTankLvl <= 0.0)
                return;
        }

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
