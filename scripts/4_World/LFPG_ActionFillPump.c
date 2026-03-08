// =========================================================
// LF_PowerGrid - Fill Container from Water Pump T2 Tank (v1.1.1)
//
// Only appears on T2 when UNPOWERED (vanilla fill covers powered case).
// Uses CAContinuousRepeat + manual liquid transfer per cycle.
// CAContinuousFill does NOT work here because our vanilla overrides
// return LIQUID_NONE when unpowered, blocking the internal transfer.
//
// Each cycle transfers up to LFPG_PUMP_FILL_PER_CYCLE from tank to container.
// Tank decrements proportionally to actual amount transferred.
// =========================================================

class LFPG_ActionFillPumpCB : ActionContinuousBaseCB
{
    override void CreateActionComponent()
    {
        m_ActionData.m_ActionComponent = new CAContinuousRepeat(UATimeSpent.WASH_HANDS);
    }
};

class LFPG_ActionFillPump : ActionContinuousBase
{
    void LFPG_ActionFillPump()
    {
        m_CallbackClass = LFPG_ActionFillPumpCB;
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_FILLBOTTLEWELL;
        m_FullBody = true;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_CROUCH | DayZPlayerConstants.STANCEMASK_ERECT;
        m_Text = "#STR_LFPG_ACTION_FILL_PUMP";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem = new CCINonRuined;
        m_ConditionTarget = new CCTCursor(UAMaxDistances.DEFAULT);
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!player || !target || !item)
            return false;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return false;

        // Only T2 (T1 has no tank)
        LF_WaterPump_T2 pump2 = LF_WaterPump_T2.Cast(targetObj);
        if (!pump2)
            return false;

        // Only when NOT powered (vanilla fill covers powered case)
        EntityAI pumpEnt = EntityAI.Cast(targetObj);
        if (pumpEnt && LFPG_PumpHelper.VerifyPowered(pumpEnt))
            return false;

        // Tank must have water
        float tankLvl = pump2.LFPG_GetTankLevel();
        if (tankLvl <= 0.0)
            return false;

        // Item must be a liquid container with space
        float itemQty = item.GetQuantity();
        float itemMax = item.GetQuantityMax();
        if (itemQty >= itemMax)
            return false;

        // Verify item can accept water (prevents filling non-liquid items)
        int tankLiq = pump2.LFPG_GetTankLiquidType();
        int checkType = LIQUID_RIVERWATER;
        if (tankLiq > 0)
        {
            checkType = tankLiq;
        }
        if (!Liquid.CanFillContainer(item, checkType))
            return false;

        return true;
    }

    override bool ActionConditionContinue(ActionData action_data)
    {
        if (!action_data || !action_data.m_Target || !action_data.m_MainItem)
            return false;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return false;

        LF_WaterPump_T2 pump2 = LF_WaterPump_T2.Cast(targetObj);
        if (!pump2)
            return false;

        // Stop if tank empty
        float tankLvl = pump2.LFPG_GetTankLevel();
        if (tankLvl <= 0.0)
            return false;

        // Stop if container full
        ItemBase mainItem = action_data.m_MainItem;
        float itemQty = mainItem.GetQuantity();
        float itemMax = mainItem.GetQuantityMax();
        if (itemQty >= itemMax)
            return false;

        return true;
    }

    override void OnFinishProgressServer(ActionData action_data)
    {
        if (!action_data || !action_data.m_Target || !action_data.m_Player)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        LF_WaterPump_T2 pump2 = LF_WaterPump_T2.Cast(targetObj);
        if (!pump2)
            return;

        ItemBase fillItem = action_data.m_MainItem;
        if (!fillItem)
            return;

        // How much space the container has
        float itemQty = fillItem.GetQuantity();
        float itemMax = fillItem.GetQuantityMax();
        float spaceLeft = itemMax - itemQty;
        if (spaceLeft <= 0.0)
            return;

        // How much tank has
        float tankLvl = pump2.LFPG_GetTankLevel();
        if (tankLvl <= 0.0)
            return;

        // Transfer amount: min of (space in bottle, tank available, max per cycle)
        // LFPG_PUMP_TANK_FILL_COST = 0.5L = 500ml per cycle
        float transferMl = LFPG_PUMP_TANK_FILL_COST * 1000.0;
        if (transferMl > spaceLeft)
        {
            transferMl = spaceLeft;
        }
        float transferL = transferMl / 1000.0;
        if (transferL > tankLvl)
        {
            transferL = tankLvl;
            transferMl = transferL * 1000.0;
        }

        if (transferMl <= 0.0)
            return;

        // Determine liquid type from tank
        EntityAI targetEnt = EntityAI.Cast(targetObj);
        int tankLiq = pump2.LFPG_GetTankLiquidType();
        int liquidType = LIQUID_RIVERWATER;
        if (targetEnt)
        {
            liquidType = LFPG_PumpHelper.DetermineLiquidType(targetEnt, false, tankLiq);
        }

        // Transfer liquid to container
        Liquid.FillContainerEnviro(fillItem, liquidType, transferMl);

        // Decrement tank proportionally
        float newTankLvl = tankLvl - transferL;
        if (newTankLvl < 0.0)
        {
            newTankLvl = 0.0;
        }
        pump2.LFPG_SetTankLevel(newTankLvl);
    }

    override void OnFinishProgressClient(ActionData action_data)
    {
        if (action_data && action_data.m_Target)
        {
            Object sndObj = action_data.m_Target.GetObject();
            if (sndObj)
            {
                EntityAI sndEnt = EntityAI.Cast(sndObj);
                if (sndEnt)
                {
                    SEffectManager.PlaySound(LFPG_PUMP_WATER_SOUNDSET, sndEnt.GetPosition());
                }
            }
        }
    }
};
