// ---- v0.8.0: Attachment slots for Solar Panel upgrade materials ----
class CfgSlots
{
    class Slot_LF_SolarPlate
    {
        name = "LF_SolarPlate";
        displayName = "Metal Plate";
        ghostIcon = "missing";
    };
    class Slot_LF_SolarNails
    {
        name = "LF_SolarNails";
        displayName = "Nails";
        ghostIcon = "missing";
    };
};

class CfgPatches
{
    class LFPowerGrid
    {
        units[] = { "LF_CableReel", "LF_TestGenerator", "LF_TestLamp", "LF_TestLampHeavy", "LF_Splitter_Kit", "LF_Splitter", "LF_CeilingLight_Kit", "LF_CeilingLight", "LF_SolarPanel_Kit", "LF_SolarPanel", "LF_SolarPanel_T2", "LF_Combiner_Kit", "LF_Combiner", "LF_Camera_Kit", "LF_Camera", "LF_Monitor_Kit", "LF_Monitor"};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] = { "DZ_Data", "DZ_Scripts", "DZ_Gear_Tools", "DZ_Gear_Camping", "DZ_Gear_Containers" };
    };
};

class CfgMods
{
    class LFPowerGrid
    {
        dir = "LFPowerGrid";
        picture = "";
        action = "";
        hideName = 0;
        hidePicture = 0;
        name = "LFPowerGrid";
        prefix = "LFPowerGrid";
        credits = "";
        author = "LF";
        authorID = "0";
        version = "0.9.2";
        type = "mod";

        dependencies[] = { "Game", "World", "Mission" };

        class defs
        {
            class gameScriptModule
            {
                value = "";
                // Tolerant paths to survive different packers (with/without root folder, case differences)
                files[] = { "LFPowerGrid/scripts/3_Game", "LFPowerGrid/Scripts/3_Game", "scripts/3_Game", "Scripts/3_Game" };
            };
            class worldScriptModule
            {
                value = "";
                files[] = { "LFPowerGrid/scripts/4_World", "LFPowerGrid/Scripts/4_World", "scripts/4_World", "Scripts/4_World" };
            };
            class missionScriptModule
            {
                value = "";
                files[] = { "LFPowerGrid/scripts/5_Mission", "LFPowerGrid/Scripts/5_Mission", "scripts/5_Mission", "Scripts/5_Mission" };
            };
        };
    };
};

class CfgVehicles
{
    class CableReel;
    class PowerGenerator;
    class Spotlight;

    class LF_CableReel : CableReel
    {
        scope = 2;
        isDeployable = 0; // disable vanilla placing/hologram
        displayName = "LF Cable Reel";
        descriptionShort = "Wiring tool for LF_PowerGrid.";
    };

    class LF_TestGenerator : PowerGenerator
    {
        scope = 2;
        displayName = "LF Test Generator";
        descriptionShort = "LF_PowerGrid demo source device.";
    };

    class LF_TestLamp : Spotlight
    {
        scope = 2;
        displayName = "LF Test Lamp";
        descriptionShort = "LF_PowerGrid demo consumer device.";
    };

    class LF_TestLampHeavy : LF_TestLamp
    {
        scope = 2;
        displayName = "LF Test Lamp (Heavy)";
        descriptionShort = "High-consumption test lamp (50 units/s). For load testing.";
    };

    // ---- Splitter Kit (holdable, deployable) ----
    // Uses the splitter model directly so hologram matches final result.
    // On placement, spawns LF_Splitter and deletes the kit.
    class Inventory_Base;
    class DeployableContainer_Base;
    class LF_Splitter_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "Splitter Kit";
        descriptionShort = "A power splitter. Place to deploy.";
        model = "\LFPowerGrid\data\splitter\lf_splitter.p3d";
        weight = 3000;
        itemSize[] = {3, 3};
        rotationFlags = 17;
        isDeployable = 1;
		carveNavmesh    = 1;
		physLayer       = "item_large";
		// slopeTolerance = min dot(surfaceNormal, up). 1.0=flat only, 0.0=any surface.
		// 0.0 allows walls/ceilings. For walls only (no ceiling): ~0.05
		slopeTolerance  = 0.0;
		yawPitchRollLimit[] = {90, 90, 90};
		hiddenSelections[] = {"zbytek"};
		hiddenSelectionsTextures[] = {""};
		hiddenSelectionsMaterials[] = {""};
		// placementOffset removed: causes oscillation loop with vanilla hologram.
		// Wall offset handled by custom hologram (LFPG_SplitterHologram).
    };

    // ---- Splitter (placed device) ----
    // 1 input, 3 outputs. Each output delivers 1/3 of incoming power.
    // Memory points in p3d (LOD Memory):
    //   port_input_1, port_output_1, port_output_2, port_output_3
    class LF_Splitter : Inventory_Base
    {
        scope = 2;
        displayName = "Power Splitter";
        descriptionShort = "Splits incoming power into 3 outputs.";
        model = "\LFPowerGrid\data\splitter\lf_splitter.p3d";
        weight = 5000;
        itemSize[] = {3, 3};
        rotationFlags = 17;
		carveNavmesh    = 1;
		physLayer       = "item_large";
		isDeployable    = 0;
    };

    // ---- CeilingLight Kit (holdable, deployable) ----
    // On placement, spawns LF_CeilingLight and deletes the kit.
    // slopeTolerance=0.0 allows ceiling/wall placement.
    // Pitch=180 for ceiling set by HologramMod, not player input.
    class LF_CeilingLight_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "Ceiling Light Kit";
        descriptionShort = "An overhead light. Place on ceiling, wall, or floor.";
        model = "\LFPowerGrid\data\ceiling_light\lf_ceiling_light.p3d";
        weight = 2000;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh    = 1;
        physLayer       = "item_large";
        slopeTolerance  = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
    };

    // ---- CeilingLight (placed device) ----
    // PASSTHROUGH: 1 input, 1 output. Consumes 10 u/s for light, passes rest downstream.
    // Memory points (LOD Memory): light, port_input_1, port_output_1
    // Named selection: light_emit (hiddenSelections[0] for rvmat swap on power toggle)
    class LF_CeilingLight : Inventory_Base
    {
        scope = 2;
        displayName = "Ceiling Light";
        descriptionShort = "Overhead light. Consumes 10 u/s, passes power downstream.";
        model = "\LFPowerGrid\data\ceiling_light\lf_ceiling_light.p3d";
        weight = 3000;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        carveNavmesh    = 1;
        physLayer       = "item_large";
        isDeployable    = 0;
        hiddenSelections[] = {"light_emit"};
        hiddenSelectionsTextures[] = {""};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\ceiling_light\lf_ceiling_light.rvmat"};
    };

    // =========================================================
    // v0.8.1: SOLAR PANEL (DeployableContainer_Base refactor)
    // =========================================================

    // ---- Solar Panel Kit (DeployableContainer_Base, different-model hologram) ----
    // v0.8.2: Refactored BACK TO Inventory_Base + isDeployable=1
    //   old DeployableContainer_Base IS NOT SHOWING KIT.
    class LF_SolarPanel_Kit : DeployableContainer_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_SOLAR_KIT";
        descriptionShort = "$STR_LFPG_SOLAR_KIT_DESC";
        model = "\LFPowerGrid\data\kits\lf_kit_box.p3d";
        weight = 2000;
        itemSize[] = {5, 3};
        rotationFlags = 2;
        itemBehaviour = 2;
        canBeDigged = 0;
        carveNavmesh = 1;
        physLayer = "item_small";
        // v0.8.1: Vanilla placement actions (sample_container pattern).
        // Script also registers via AddAction() for safety (DayZ deduplicates).
        SingleUseActions[] = {527};
        ContinuousActions[] = {231};
    };

    // ---- Solar Panel T1 (placed device, SOURCE 20 u/s) ----
    // Has attachment slots for upgrade materials (MetalPlate + Nail).
    // Memory points required in p3d: port_output_1
    class LF_SolarPanel : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_SOLAR_T1";
        descriptionShort = "$STR_LFPG_SOLAR_T1_DESC";
        model = "\LFPowerGrid\data\solarpanel\lf_solarpanel.p3d";
        weight = 8000;
        itemSize[] = {10, 10};
        rotationFlags = 17;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        attachments[] = {"LF_SolarPlate", "LF_SolarNails"};
        class GUIInventoryAttachmentsProps
        {
            class UpgradeMaterials
            {
                name = "Upgrade Materials";
                description = "";
                attachmentSlots[] = {"LF_SolarPlate", "LF_SolarNails"};
                icon = "missing";
            };
        };
    };

    // ---- Solar Panel T2 (upgraded device, SOURCE 50 u/s) ----
    // No attachment slots - already upgraded.
    // Memory points required in p3d: port_output_1
    class LF_SolarPanel_T2 : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_SOLAR_T2";
        descriptionShort = "$STR_LFPG_SOLAR_T2_DESC";
        model = "\LFPowerGrid\data\solarpanel\lf_solarpanel_t2.p3d";
        weight = 10000;
        itemSize[] = {10, 10};
        rotationFlags = 17;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
    };


    // =========================================================
    // v0.8.2: COMBINER (same-model deployment, floor + wall)
    // =========================================================

    // ---- Combiner Kit (holdable, deployable) ----
    class LF_Combiner_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_CombinerKit";
        descriptionShort = "$STR_LFPG_CombinerKit_Desc";
        model = "\LFPowerGrid\data\combiner\CombinerLF.p3d";
        weight = 800;
        itemSize[] = {3, 3};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {};
    };

    // ---- Combiner (placed device) ----
    // 2 inputs, 1 output. output = min(input_1 + input_2, 500 u/s)
    // Memory points: port_input_1, port_input_2, port_output_1
    class LF_Combiner : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_Combiner";
        descriptionShort = "$STR_LFPG_Combiner_Desc";
        model = "\LFPowerGrid\data\combiner\CombinerLF.p3d";
        weight = 1200;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        hiddenSelections[] = {};
    };
	
	// =========================================================
    // v0.9.0: CCTV SYSTEM (Camera + Monitor, same-model deploy)
    // =========================================================

    // ---- Camera Kit (holdable, deployable) ----
    class LF_Camera_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_CameraKit";
        descriptionShort = "$STR_LFPG_CameraKit_Desc";
        model = "\LFPowerGrid\data\cctv\CamaraLF.p3d";
        weight = 1500;
        itemSize[] = {3, 3};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {};
    };

    // ---- Camera (placed device) ----
    // Memory points (LOD Memory): port_input_1
    // Named selections (LOD Resolution): cam_led (hiddenSelections[0])
    class LF_Camera : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_Camera";
        descriptionShort = "$STR_LFPG_Camera_Desc";
        model = "\LFPowerGrid\data\cctv\CamaraLF.p3d";
        weight = 2000;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        hiddenSelections[] = { "cam_led" };
		hiddenSelectionsTextures[] = { "\LFPowerGrid\data\camera\lf_camera_led.paa" };
		hiddenSelectionsMaterials[] = { "\LFPowerGrid\data\camera\lf_camera_led_off.rvmat" };
    };

    // ---- Monitor Kit (holdable, deployable) ----
    class LF_Monitor_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_MonitorKit";
        descriptionShort = "$STR_LFPG_MonitorKit_Desc";
        model = "\LFPowerGrid\data\cctv\MonitorLF.p3d";
        weight = 3000;
        itemSize[] = {4, 3};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {};
    };

    // ---- Monitor (placed device) ----
    // Memory points (LOD Memory): port_input_1
    // Named selections (LOD Resolution): screen (hiddenSelections[0])
    class LF_Monitor : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_Monitor";
        descriptionShort = "$STR_LFPG_Monitor_Desc";
        model = "\LFPowerGrid\data\cctv\MonitorLF.p3d";
        weight = 4000;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        hiddenSelections[] = {"screen"};
        hiddenSelectionsTextures[] = {""};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\cctv\lf_monitor_off.rvmat"};
    };
	
    // =========================================================
    // v0.8.0: MODDED VANILLA ITEMS (additive, not destructive)
    // Allow MetalPlate and Nail to go into solar panel slots.
    // =========================================================
    class MetalPlate
    {
        inventorySlot[] += {"LF_SolarPlate"};
    };
    class Nail
    {
        inventorySlot[] += {"LF_SolarNails"};
    };
};
