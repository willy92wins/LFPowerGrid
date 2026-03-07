// =========================================================
// LF_PowerGrid - Fill Container from Water Pump T2 Tank (v1.1.0 Sprint W3)
//
// Only appears on T2 when UNPOWERED (vanilla fill covers powered case).
// [FIX-18] Registered on TARGET (T2), not on item.
// [FIX-7] liquidType determined ONCE in CB, not mid-fill.
// =========================================================

class LFPG_ActionFillPumpCB : ActionContinuousBaseCB
{
    override void CreateActionComponent()
    {
        // Determine liquid type once at start (FIX-7)
        int liquidType = LIQUID_RIVERWATER;

        if (m_ActionData && m_ActionData.m_Target)
        {
            Object obj = m_ActionData.m_Target.GetObject();
            if (obj)
            {
                LF_WaterPump_T2 pump2 = LF_WaterPump_T2.Cast(obj);
                if (pump2)
                {
                    EntityAI ent = EntityAI.Cast(obj);
                    if (ent)
                    {
                        int tankLiq = pump2.LFPG_GetTankLiquidType();
                        liquidType = LFPG_PumpHelper.DetermineLiquidType(ent, false, tankLiq);
                    }
                }
            }
        }

        m_ActionData.m_ActionComponent = new CAContinuousFill(UAQuantityConsumed.FILL_LIQUID, liquidType);
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
        if (pump2.LFPG_GetPoweredNet())
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

        // Determine liquid type for container check
        EntityAI ent = EntityAI.Cast(targetObj);
        if (!ent)
            return false;

        int tankLiq = pump2.LFPG_GetTankLiquidType();
        int liquidType = LFPG_PumpHelper.DetermineLiquidType(ent, false, tankLiq);

        bool canFill = Liquid.CanFillContainer(item, liquidType);
        return canFill;
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
        float itemQty = action_data.m_MainItem.GetQuantity();
        float itemMax = action_data.m_MainItem.GetQuantityMax();
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

        // Decrement tank per fill cycle [FIX-2]
        float level = pump2.LFPG_GetTankLevel();
        level = level - LFPG_PUMP_TANK_FILL_COST;
        if (level < 0.0)
        {
            level = 0.0;
        }
        pump2.LFPG_SetTankLevel(level);
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
