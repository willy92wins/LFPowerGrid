// =========================================================
// LF_PowerGrid - action registration + tool attachments
//
// v0.7.47: Added LFPG_ActionUpgradeSolarPanel + LFPG_ActionPlaceSolarPanel
//          + modded Hammer for upgrade action.
// v0.8.1:  Removed LFPG_ActionPlaceSolarPanel — solar kit now uses
//          DeployableContainer_Base pattern with vanilla ActionPlaceObject.
//          LFPG_ActionPlaceSolarPanel.c can be deleted from the project.
// v0.8.2:  Added LFPG_ActionPlaceCombiner for Combiner Kit deployment.
// v0.9.2:  Sprint B — Replaced ActionViewCamera/CycleCamera/UnlinkCamera
//          with single ActionWatchMonitor (RPC-based camera list + in-viewport cycling).
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

        // --- v0.7.47: Solar Panel upgrade (placement now uses vanilla ActionPlaceObject) ---
        actions.Insert(LFPG_ActionUpgradeSolarPanel);

        // --- v0.7.48: Placement actions (parity registration) ---
        actions.Insert(LFPG_ActionPlaceSplitter);
        actions.Insert(LFPG_ActionPlaceCeilingLight);

        // --- v0.8.2: Combiner Kit placement ---
        actions.Insert(LFPG_ActionPlaceCombiner);
		
		// --- v0.9.0: CCTV Kit placement ---
        actions.Insert(LFPG_ActionPlaceCamera);
        actions.Insert(LFPG_ActionPlaceMonitor);

        // v0.9.2 Sprint B: ActionWatchMonitor replaces ViewCamera/CycleCamera/UnlinkCamera
        actions.Insert(LFPG_ActionWatchMonitor);
		
		actions.Insert(LFPG_ActionPlacePushButton);
		actions.Insert(LFPG_ActionTogglePushButton);

        // --- v1.6.0: Switch V2 (latching toggle lever) ---
        actions.Insert(LFPG_ActionPlaceSwitchV2);
        actions.Insert(LFPG_ActionToggleSwitchV2);

        // --- v1.1.0: Water Pump upgrade + water actions ---
        actions.Insert(LFPG_ActionUpgradeWaterPump);
        actions.Insert(LFPG_ActionDrinkPump);
        actions.Insert(LFPG_ActionWashHandsPump);
        actions.Insert(LFPG_ActionFillPump);

        // --- v1.2.0: Sorter Kit placement ---
        actions.Insert(LFPG_ActionPlaceSorter);

        // --- v1.2.0 S4: Sorter panel (interact, no item in hand) ---
        actions.Insert(LFPG_ActionOpenSorterPanel);

        // --- v2.4: Sorter container sync ---
        actions.Insert(LFPG_ActionSyncSorter);

        // --- v1.2.0: Furnace actions ---
        actions.Insert(LFPG_ActionToggleFurnace);
        actions.Insert(LFPG_ActionFeedFurnace);

        // --- v1.4.0: Searchlight Kit placement ---
        actions.Insert(LFPG_ActionPlaceSearchlight);

        // --- v1.4.0: Searchlight operate (spectator control) ---
        actions.Insert(LFPG_ActionOperateSearchlight);

        // --- v1.5.0: Motion Sensor actions ---
        actions.Insert(LFPG_ActionPlaceMotionSensor);
        actions.Insert(LFPG_ActionCycleDetectMode);
        actions.Insert(LFPG_ActionPairSensor);

        // --- v1.7.0: Logic Gate Kit placement ---
        actions.Insert(LFPG_ActionPlaceLogicGate);

        // --- v1.8.0: Pressure Pad Kit placement ---
        actions.Insert(LFPG_ActionPlacePressurePad);

        // --- v1.9.0: Laser Detector Kit placement ---
        actions.Insert(LFPG_ActionPlaceLaserDetector);

        // --- v2.0.0: Electronic Counter Kit placement ---
        actions.Insert(LFPG_ActionPlaceElectronicCounter);

        // --- v2.0: Battery Medium Kit placement ---
        actions.Insert(LFPG_ActionPlaceBatteryMedium);

        // --- v2.0: Battery output toggle (Medium switch) ---
        actions.Insert(LFPG_ActionToggleBatteryOutput);

        // --- v3.0: Door Controller Kit placement ---
        actions.Insert(LFPG_ActionPlaceDoorController);

        // --- v3.0: Intercom / RF Broadcaster ---
        actions.Insert(LFPG_ActionPlaceIntercom);
        actions.Insert(LFPG_ActionToggleIntercom);
        actions.Insert(LFPG_ActionRFToggle);
        actions.Insert(LFPG_ActionInstallMic);
        actions.Insert(LFPG_ActionToggleBroadcast);
        actions.Insert(LFPG_ActionCycleFrequency);

        LFPG_Util.Debug("[Actions] LFPG actions registered (56)");
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
