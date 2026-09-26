// Server action implementation supplied by both gameplay mission types.
class LFPG_ServerActionsImpl : LFPG_ServerActions
{
    // ---- Heater ----
    // The World side is a shell by the 2026-09-21 owner decision; the work
    // lives here. Heat rides the same UniversalTemperatureSource path as the
    // furnace, gated by powered AND switched on.
    //
    // Device state arrives through the `dev` parameter and this class does not
    // extend LFPG_DeviceBase, so only public members are reachable. Anything
    // the base keeps protected goes through its accessor.

    override void Heater_EEInit(LFPG_Heater dev)
    {
        #ifdef SERVER
        LFPG_ServerSettings st = LFPG_Settings.Get();
        if (st.HeaterHeatEnabled)
        {
            dev.m_UTSSettings = new UniversalTemperatureSourceSettings();
            dev.m_UTSSettings.m_Updateable = true;
            dev.m_UTSSettings.m_ManualUpdate = false;
            float utsInterval = 3.0;
            dev.m_UTSSettings.m_UpdateInterval = utsInterval;
            dev.m_UTSSettings.m_RangeFull = st.HeaterHeatFullWarmthRadiusM;
            dev.m_UTSSettings.m_RangeMax = st.HeaterHeatFadeOutRadiusM;
            float heatCap = LFPG_CAMPFIRE_HEAT_CAP * st.HeaterHeatStrengthMultiplier;
            dev.m_UTSSettings.m_TemperatureCap = heatCap;
            dev.m_UTSSettings.m_TemperatureItemCap = GameConstants.ITEM_TEMPERATURE_NEUTRAL_ZONE_MIDDLE;

            UniversalTemperatureSourceLambdaConstant utsLambda = new UniversalTemperatureSourceLambdaConstant();
            dev.m_UTSource = new UniversalTemperatureSource(dev, dev.m_UTSSettings, utsLambda);
        }
        // No resume call here, unlike the furnace: EEInit runs before
        // OnStoreLoad, so m_HeaterOn still holds its default. A heater
        // restored ON resumes when grid propagation calls LFPG_SetPowered.
        // The source is created inactive (universaltemperaturesource.c:78).
        #endif
    }

    // Heat follows the conjunction: the switch alone does nothing on a dead
    // grid, and power alone does nothing with the switch off.
    protected void Heater_RefreshHeat(LFPG_Heater dev)
    {
        #ifdef SERVER
        bool active = false;
        if (dev.m_PoweredNet)
        {
            if (dev.m_HeaterOn)
            {
                active = true;
            }
        }
        Heater_LFPG_SetHeatActive(dev, active);
        #endif
    }

    override void Heater_LFPG_SetHeatActive(LFPG_Heater dev, bool active)
    {
        #ifdef SERVER
        if (dev.m_UTSource)
        {
            dev.m_UTSource.SetActive(active);
        }
        #endif
    }

    override void Heater_LFPG_SetPowered(LFPG_Heater dev, bool powered)
    {
        #ifdef SERVER
        if (dev.m_PoweredNet == powered)
            return;

        dev.m_PoweredNet = powered;
        dev.SetSynchDirty();
        Heater_RefreshHeat(dev);

        if (LFPG_LOG_LEVEL >= 2)
        {
            string msg = "[LFPG_Heater] SetPowered(";
            msg = msg + powered.ToString();
            msg = msg + ") id=";
            msg = msg + dev.LFPG_GetDeviceId();
            LFPG_Util.Debug(msg);
        }
        #endif
    }

    override void Heater_LFPG_OnWiresCut(LFPG_Heater dev)
    {
        #ifdef SERVER
        if (dev.m_PoweredNet)
        {
            dev.m_PoweredNet = false;
            dev.SetSynchDirty();
        }
        Heater_LFPG_SetHeatActive(dev, false);
        #endif
    }

    override void Heater_LFPG_OnKilled(LFPG_Heater dev)
    {
        #ifdef SERVER
        if (dev.m_PoweredNet)
        {
            dev.m_PoweredNet = false;
            dev.SetSynchDirty();
        }
        Heater_LFPG_SetHeatActive(dev, false);
        #endif
    }

    // An active source runs a repeating timer (universaltemperaturesource.c:107),
    // so it has to be stopped before the parent goes away.
    override void Heater_LFPG_OnDeleted(LFPG_Heater dev)
    {
        #ifdef SERVER
        Heater_LFPG_SetHeatActive(dev, false);
        #endif
    }

    // The switch is physical: it flips with the grid dead. Only the heat
    // waits for power.
    override void Heater_LFPG_ToggleHeater(LFPG_Heater dev)
    {
        #ifdef SERVER
        if (dev.m_HeaterOn)
        {
            dev.m_HeaterOn = false;
        }
        else
        {
            dev.m_HeaterOn = true;
        }
        dev.SetSynchDirty();
        Heater_RefreshHeat(dev);

        string toggleMsg = "[LFPG_Heater] Toggle: on=";
        toggleMsg = toggleMsg + dev.m_HeaterOn.ToString();
        toggleMsg = toggleMsg + " id=";
        toggleMsg = toggleMsg + dev.LFPG_GetDeviceId();
        LFPG_Util.Debug(toggleMsg);
        #endif
    }

    override void ToggleHeater_OnExecuteServer(ActionData action_data)
    {
        if (!action_data || !action_data.m_Target)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        LFPG_Heater heater = LFPG_Heater.Cast(targetObj);
        if (!heater)
            return;

        heater.LFPG_ToggleHeater();
    }

};
