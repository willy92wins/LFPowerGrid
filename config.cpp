class CfgPatches
{
    class LFPowerGrid
    {
        units[] = { "LF_CableReel", "LF_TestGenerator", "LF_TestLamp", "LF_TestLampHeavy", "LF_Splitter_Kit", "LF_Splitter", "LF_CeilingLight_Kit", "LF_CeilingLight" };
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] = { "DZ_Data", "DZ_Scripts", "DZ_Gear_Tools", "DZ_Gear_Camping" };
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
        version = "0.8.0";
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
};
