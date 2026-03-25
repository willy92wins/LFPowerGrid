// =========================================================
// LF_PowerGrid - action registration + tool attachments
//
// v4.0 (Fase 3): Consolidated 20 individual LFPG_ActionPlaceX into
//   LFPG_ActionPlaceGeneric + LFPG_ActionPlaceLogicGate (override).
// v1.2.0:  Sprint S0 (Sorter prereq) — Added ActionLFPG_Port6 + CutPort6
//          to support 7-port devices (1 IN + 6 OUT).
//
// IMPORTANT:
//  - Actions MUST be registered in ActionConstructor::RegisterActions()
//    otherwise AddAction(...) on items will not create an instance.
// =========================================================

modded class ActionConstructor
{
    override void RegisterActions(TTypenameArray actions)
    {
        super.RegisterActions(actions);

        // --- LF_PowerGrid per-port actions ---
        actions.Insert(ActionLFPG_Port0);
        actions.Insert(ActionLFPG_Port1);
        actions.Insert(ActionLFPG_Port2);
        actions.Insert(ActionLFPG_Port3);
        actions.Insert(ActionLFPG_Port4);
        actions.Insert(ActionLFPG_Port5);
        actions.Insert(ActionLFPG_Port6);

        // --- LF_PowerGrid session + utility ---
        actions.Insert(ActionLFPG_PlaceWaypoint);
        actions.Insert(ActionLFPG_CancelWiring);
        actions.Insert(ActionLFPG_CutWires);
        actions.Insert(ActionLFPG_CutPort0);
        actions.Insert(ActionLFPG_CutPort1);
        actions.Insert(ActionLFPG_CutPort2);
        actions.Insert(ActionLFPG_CutPort3);
        actions.Insert(ActionLFPG_CutPort4);
        actions.Insert(ActionLFPG_CutPort5);
        actions.Insert(ActionLFPG_CutPort6);
        actions.Insert(ActionLFPG_ToggleSource);
        actions.Insert(ActionLFPG_DebugStatus);

        // --- Solar Panel / Water Pump upgrades ---
        actions.Insert(LFPG_ActionUpgradeSolarPanel);
        actions.Insert(LFPG_ActionUpgradeWaterPump);

        // --- v4.0 (Fase 3): Generic placement for ALL kits ---
        actions.Insert(LFPG_ActionPlaceGeneric);
        actions.Insert(LFPG_ActionPlaceLogicGate);

        // --- CCTV ---
        actions.Insert(LFPG_ActionWatchMonitor);

        // --- Switches / Buttons ---
        actions.Insert(LFPG_ActionTogglePushButton);
        actions.Insert(LFPG_ActionToggleSwitchV2);
        actions.Insert(LFPG_ActionToggleSwitchRemote);
        actions.Insert(LFPG_ActionToggleSwitchV2Remote);

        // --- Water Pump ---
        actions.Insert(LFPG_ActionDrinkPump);
        actions.Insert(LFPG_ActionWashHandsPump);
        actions.Insert(LFPG_ActionFillPump);

        // --- Sorter ---
        actions.Insert(LFPG_ActionOpenSorterPanel);
        actions.Insert(LFPG_ActionSyncSorter);

        // --- Furnace ---
        actions.Insert(LFPG_ActionToggleFurnace);
        actions.Insert(LFPG_ActionFeedFurnace);

        // --- Searchlight ---
        actions.Insert(LFPG_ActionOperateSearchlight);

        // --- Motion Sensor ---
        actions.Insert(LFPG_ActionCycleDetectMode);
        actions.Insert(LFPG_ActionPairSensor);

        // --- Battery ---
        actions.Insert(LFPG_ActionToggleBatteryOutput);

        // --- Intercom / RF Broadcaster ---
        actions.Insert(LFPG_ActionToggleIntercom);
        actions.Insert(LFPG_ActionRFToggle);
        actions.Insert(LFPG_ActionInstallMic);
        actions.Insert(LFPG_ActionToggleBroadcast);
        actions.Insert(LFPG_ActionCycleFrequency);

        // --- Sprinkler ---
        actions.Insert(LFPG_ActionCheckSprinkler);

        // --- Fridge ---
        actions.Insert(LFPG_ActionToggleFridgeDoor);

        // --- Electric Stove ---
        actions.Insert(LFPG_ActionToggleBurner0);
        actions.Insert(LFPG_ActionToggleBurner1);
        actions.Insert(LFPG_ActionToggleBurner2);
        actions.Insert(LFPG_ActionToggleBurner3);

        LFPG_Util.Debug("[Actions] LFPG actions registered (50)");
    }
};

modded class Pliers
{
    override void SetActions()
    {
        super.SetActions();
        AddAction(ActionLFPG_CutWires);
        AddAction(ActionLFPG_CutPort0);
        AddAction(ActionLFPG_CutPort1);
        AddAction(ActionLFPG_CutPort2);
        AddAction(ActionLFPG_CutPort3);
        AddAction(ActionLFPG_CutPort4);
        AddAction(ActionLFPG_CutPort5);
        AddAction(ActionLFPG_CutPort6);
    }
};

// v0.7.47: Hammer can upgrade solar panels
modded class Hammer
{
    override void SetActions()
    {
        super.SetActions();
        AddAction(LFPG_ActionUpgradeSolarPanel);
        AddAction(LFPG_ActionUpgradeWaterPump);
    }
};

// v3.0: Screwdriver can install microphone on Intercom
modded class Screwdriver
{
    override void SetActions()
    {
        super.SetActions();
        AddAction(LFPG_ActionInstallMic);
    }
};
