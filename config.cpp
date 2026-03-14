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
    // v1.1.0: Water Pump slots
    class Slot_LF_PumpFilter
    {
        name = "LF_PumpFilter";
        displayName = "Water Filter";
        ghostIcon = "missing";
    };
    class Slot_LF_PumpPlate
    {
        name = "LF_PumpPlate";
        displayName = "Metal Plate";
        ghostIcon = "missing";
    };
    class Slot_LF_PumpNails
    {
        name = "LF_PumpNails";
        displayName = "Nails";
        ghostIcon = "missing";
    };
};

// =========================================================
// v1.1.0: Water Pump sounds
// =========================================================

class CfgSoundShaders
{
    // Pump loop: low ambient motor hum while powered
    class LFPG_WaterPump_Loop_Shader
    {
        samples[] = {{ "\LFPowerGrid\data\waterpump\pump", 1 }};
        volume = 0.3;
        range = 20;
        loop = 1;
        rangeCurve[] = {{ 0, 1 }, { 10, 0.5 }, { 20, 0 }};
    };
    // Water dispense: one-shot when drinking/filling/washing
    class LFPG_WaterPump_Water_Shader
    {
        samples[] = {{ "\LFPowerGrid\data\waterpump\water", 1 }};
        volume = 0.6;
        range = 10;
        rangeCurve[] = {{ 0, 1 }, { 5, 0.4 }, { 10, 0 }};
    };
    // v1.8.0: Pressure Pad press click (one-shot)
    class LFPG_PressurePad_Press_Shader
    {
        samples[] = {{ "\LFPowerGrid\pressure_pad\sounds\pressure_pad_press", 1 }};
        volume = 0.5;
        range = 8;
        rangeCurve[] = {{ 0, 1 }, { 4, 0.4 }, { 8, 0 }};
    };
};

class CfgSoundSets
{
    // Pump loop — spatial 3D, played by SEffectManager in OnVariablesSynchronized
    class LFPG_WaterPump_Loop_SoundSet
    {
        soundShaders[] = { "LFPG_WaterPump_Loop_Shader" };
        volumeFactor = 1;
        frequencyFactor = 1;
        spatial = 1;
    };
    // Water dispense — spatial 3D, played during drink/fill/wash actions (Sprint W2)
    class LFPG_WaterPump_Water_SoundSet
    {
        soundShaders[] = { "LFPG_WaterPump_Water_Shader" };
        volumeFactor = 1;
        frequencyFactor = 1;
        spatial = 1;
    };
    // v1.8.0: Pressure Pad press — spatial 3D, one-shot on gate open
    class LFPG_PressurePad_Press_SoundSet
    {
        soundShaders[] = { "LFPG_PressurePad_Press_Shader" };
        volumeFactor = 1;
        frequencyFactor = 1;
        spatial = 1;
    };
};

class CfgPatches
{
    class LFPowerGrid
    {
        units[] = { "LF_CableReel", "LF_TestGenerator", "LF_TestLamp", "LF_TestLampHeavy", "LF_Splitter_Kit", "LF_Splitter", "LF_CeilingLight_Kit", "LF_CeilingLight", "LF_SolarPanel_Kit", "LF_SolarPanel", "LF_SolarPanel_T2", "LF_Combiner_Kit", "LF_Combiner", "LF_Camera_Kit", "LF_Camera", "LF_Monitor_Kit", "LF_Monitor", "LFPG_PushButton_Kit", "LFPG_PushButton", "LFPG_SwitchV2_Kit", "LFPG_SwitchV2", "LF_WaterPump_Kit", "LF_WaterPump", "LF_WaterPump_T2", "LF_Furnace_Kit", "LF_Furnace", "LF_Sorter_Kit", "LF_Sorter", "LF_Searchlight_Kit", "LF_Searchlight", "LFPG_MotionSensor_Kit", "LFPG_MotionSensor", "LFPG_AND_Gate_Kit", "LFPG_AND_Gate", "LFPG_OR_Gate_Kit", "LFPG_OR_Gate", "LFPG_XOR_Gate_Kit", "LFPG_XOR_Gate", "LFPG_PressurePad_Kit", "LFPG_PressurePad", "LFPG_LaserDetector_Kit", "LFPG_LaserDetector", "LFPG_ElectronicCounter_Kit", "LFPG_ElectronicCounter", "LF_BatteryMedium_Kit", "LF_BatteryMedium", "LF_BatteryLarge_Kit", "LF_BatteryLarge", "LF_DoorController_Kit", "LF_DoorController"};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] = { "DZ_Data", "DZ_Scripts", "DZ_Gear_Tools", "DZ_Gear_Camping", "DZ_Gear_Containers", "DZ_Gear_Consumables" };
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
        credits = "Extanator";
        author = "Return";
        authorID = "0";
        version = "1.2.0";
        type = "mod";

        dependencies[] = { "Game", "World", "Mission" };

        class defs
        {
            class gameScriptModule
            {
                value = "";
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
        isDeployable = 0;
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
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {"zbytek"};
        hiddenSelectionsTextures[] = {""};
        hiddenSelectionsMaterials[] = {""};
    };

    // ---- Splitter (placed device) ----
    class LF_Splitter : Inventory_Base
    {
        scope = 2;
        displayName = "Power Splitter";
        descriptionShort = "Splits incoming power into 3 outputs.";
        model = "\LFPowerGrid\data\splitter\lf_splitter.p3d";
        weight = 5000;
        itemSize[] = {3, 3};
        rotationFlags = 17;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
    };

    // ---- CeilingLight Kit (holdable, deployable) ----
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
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
    };

    // ---- CeilingLight (placed device) ----
    class LF_CeilingLight : Inventory_Base
    {
        scope = 2;
        displayName = "Ceiling Light";
        descriptionShort = "Overhead light. Consumes 10 u/s, passes power downstream.";
        model = "\LFPowerGrid\data\ceiling_light\lf_ceiling_light.p3d";
        weight = 3000;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        hiddenSelections[] = {"light_emit"};
        hiddenSelectionsTextures[] = {""};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\ceiling_light\lf_ceiling_light.rvmat"};
    };

    // =========================================================
    // v0.8.1: SOLAR PANEL
    // =========================================================

    // ---- Solar Panel Kit ----
    class LF_SolarPanel_Kit : Inventory_Base
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
        SingleUseActions[] = {527};
        ContinuousActions[] = {231};
    };

    // ---- Solar Panel T1 (placed device, SOURCE 20 u/s) ----
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
        hiddenSelectionsTextures[] = { "\LFPowerGrid\data\cctv\lf_camera_led.paa" };
        hiddenSelectionsMaterials[] = { "\LFPowerGrid\data\cctv\lf_camera_led_off.rvmat" };
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
    // v0.10.0: PUSH BUTTON (PASSTHROUGH, momentary toggle)
    // =========================================================

    // ---- PushButton Kit (holdable, deployable, same-model) ----
    class LFPG_PushButton_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "Push Button Kit";
        descriptionShort = "A toggle switch. Place to deploy.";
        model = "\LFPowerGrid\switch_v1\switch_v1.p3d";
        weight = 500;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {"camo", "switch"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\switch_v1\data\switch_v1_co.paa", "\LFPowerGrid\switch_v1\data\switch_v1_co.paa"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\switch_v1\data\switch_v1.rvmat", "\LFPowerGrid\switch_v1\data\switch_v1.rvmat"};
    };

    // ---- PushButton (placed device, PASSTHROUGH 1 IN + 1 OUT) ----
    // Ports: input_1, output_1 (memory points: port_input_0, port_output_0)
    // Toggle: momentary ON for 2s, then auto-OFF.
    // LED: green (passing power), red (blocking), off (disconnected)
    class LFPG_PushButton : Inventory_Base
    {
        scope = 2;
        displayName = "Electrical Switch";
        descriptionShort = "A toggle switch with LED status indicator.";
        model = "\LFPowerGrid\switch_v1\switch_v1.p3d";
        weight = 800;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;

        hiddenSelections[] = {"camo", "switch", "light_led"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\switch_v1\data\switch_v1_co.paa", "\LFPowerGrid\switch_v1\data\switch_v1_co.paa", ""};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\switch_v1\data\switch_v1.rvmat", "\LFPowerGrid\switch_v1\data\switch_v1.rvmat", "\LFPowerGrid\switch_v1\data\led_off.rvmat"};

        class AnimationSources
        {
            class switch_state
            {
                source = "user";
                initPhase = 0;
                animPeriod = 0.3;
            };
        };

        class DamageSystem
        {
            class DamageZones
            {
                class Zone_Housing
                {
                    class Health
                    {
                        hitpoints = 100;
                        healthLevels[] =
                        {
                            {1.0, {}},
                            {0.7, {}},
                            {0.5, {}},
                            {0.3, {}},
                            {0.0, {}}
                        };
                    };
                    componentNames[] = {"Component01"};
                    fatalInjuryCoef = -1;
                };
            };
        };
    };

    // =========================================================
    // v1.6.0: SWITCH V2 (PASSTHROUGH, latching toggle lever)
    // =========================================================

    // ---- SwitchV2 Kit (holdable, same-model deploy) ----
    class LFPG_SwitchV2_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_SWITCHV2_KIT";
        descriptionShort = "$STR_LFPG_SWITCHV2_KIT_DESC";
        model = "\LFPowerGrid\switch_v2\switch_v2.p3d";
        weight = 500;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {"camo"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\switch_v2\data\switch_v2_co.paa"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\switch_v2\data\switch_v2.rvmat"};
    };

    // ---- SwitchV2 (placed device, PASSTHROUGH 1 IN + 1 OUT) ----
    // Ports: input_1, output_1 (memory points: port_input_0, port_output_0)
    // Toggle: latching ON/OFF (persisted across restart).
    // LED: green (passing power), red (blocking), off (disconnected)
    class LFPG_SwitchV2 : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_SWITCHV2";
        descriptionShort = "$STR_LFPG_SWITCHV2_DESC";
        model = "\LFPowerGrid\switch_v2\switch_v2.p3d";
        weight = 800;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;

        hiddenSelections[] = {"camo", "light_led"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\switch_v2\data\switch_v2_co.paa", ""};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\switch_v2\data\switch_v2.rvmat", "\LFPowerGrid\switch_v2\data\led_off.rvmat"};

        class AnimationSources
        {
            class switch
            {
                source = "user";
                initPhase = 0;
                animPeriod = 0.3;
            };
        };

        class DamageSystem
        {
            class GlobalHealth
            {
                class Health
                {
                    hitpoints = 100;
                    healthLevels[] =
                    {
                        {1.0, {}},
                        {0.7, {}},
                        {0.5, {}},
                        {0.3, {}},
                        {0.0, {}}
                    };
                };
                componentNames[] = {"Component01"};
                fatalInjuryCoef = -1;
            };
        };
    };

    // =========================================================
    // v0.8.0: MODDED VANILLA ITEMS (additive, not destructive)
    // =========================================================
    class MetalPlate
    {
        inventorySlot[] += {"LF_SolarPlate", "LF_PumpPlate"};
    };
    class Nail
    {
        inventorySlot[] += {"LF_SolarNails", "LF_PumpNails"};
    };
    // v1.1.0: GasMask_Filter (vanilla NBC filter) can go in pump filter slot
    class GasMask_Filter
    {
        inventorySlot[] += {"LF_PumpFilter"};
    };

    // =========================================================
    // v1.1.0: WATER PUMP (PASSTHROUGH, filter + tank)
    // =========================================================

    // ---- Water Pump Kit (holdable, different-model deploy like Solar Panel) ----
    class LF_WaterPump_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_PUMP_KIT";
        descriptionShort = "$STR_LFPG_PUMP_KIT_DESC";
        model = "\LFPowerGrid\data\kits\lf_kit_box.p3d";
        weight = 5000;
        itemSize[] = {5, 3};
        rotationFlags = 2;
        itemBehaviour = 2;
        canBeDigged = 0;
        carveNavmesh = 1;
        physLayer = "item_small";
        SingleUseActions[] = {527};
        ContinuousActions[] = {231};
    };

    // ---- Water Pump T1 (placed device, PASSTHROUGH 50 u/s, cap 100 u/s) ----
    class LF_WaterPump : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_PUMP_T1";
        descriptionShort = "$STR_LFPG_PUMP_T1_DESC";
        model = "\LFPowerGrid\data\waterpump\lf_waterpump.p3d";
        weight = 12000;
        itemSize[] = {10, 10};
        rotationFlags = 17;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        attachments[] = {"LF_PumpFilter", "LF_PumpPlate", "LF_PumpNails"};
        hiddenSelections[] = {"pump_led"};
        hiddenSelectionsTextures[] = {""};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\waterpump\lf_pump_led_off.rvmat"};
        class GUIInventoryAttachmentsProps
        {
            class WaterFilter
            {
                name = "Water Filter";
                description = "";
                attachmentSlots[] = {"LF_PumpFilter"};
                icon = "missing";
            };
            class UpgradeMaterials
            {
                name = "Upgrade Materials";
                description = "";
                attachmentSlots[] = {"LF_PumpPlate", "LF_PumpNails"};
                icon = "missing";
            };
        };
    };

    // ---- Water Pump T2 (upgraded device, PASSTHROUGH 50 u/s, cap 100 u/s + tank 50L) ----
    class LF_WaterPump_T2 : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_PUMP_T2";
        descriptionShort = "$STR_LFPG_PUMP_T2_DESC";
        model = "\LFPowerGrid\data\waterpump\lf_waterpump_t2.p3d";
        weight = 18000;
        itemSize[] = {10, 10};
        rotationFlags = 17;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        attachments[] = {"LF_PumpFilter"};
        hiddenSelections[] = {"pump_led"};
        hiddenSelectionsTextures[] = {""};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\waterpump\lf_pump_led_off.rvmat"};
        class GUIInventoryAttachmentsProps
        {
            class WaterFilter
            {
                name = "Water Filter";
                description = "";
                attachmentSlots[] = {"LF_PumpFilter"};
                icon = "missing";
            };
        };
    };

    // =========================================================
    // v1.2.0: FURNACE (SOURCE, burns items for power)
    // =========================================================

    // ---- Furnace Kit (holdable, different-model deploy) ----
    class LF_Furnace_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_FURNACE_KIT";
        descriptionShort = "$STR_LFPG_FURNACE_KIT_DESC";
        model = "\LFPowerGrid\data\kits\lf_kit_box.p3d";
        weight = 8000;
        itemSize[] = {5, 3};
        rotationFlags = 2;
        itemBehaviour = 2;
        canBeDigged = 0;
        carveNavmesh = 1;
        physLayer = "item_small";
        SingleUseActions[] = {527};
        ContinuousActions[] = {231};
    };

    // ---- Furnace (placed device, SOURCE 50 u/s) ----
    class LF_Furnace : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_FURNACE";
        descriptionShort = "$STR_LFPG_FURNACE_DESC";
        model = "\LFPowerGrid\data\furnace\Furnace.p3d";
        weight = 25000;
        itemSize[] = {10, 10};
        itemsCargoSize[] = {10, 10};
        rotationFlags = 17;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        hiddenSelections[] = {"body"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\data\furnace\Furnace.paa"};
    };

    // ---- Sorter Kit (holdable, deployable) ----
    class LF_Sorter_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_SorterKit";
        descriptionShort = "$STR_LFPG_SorterKit_Desc";
        model = "\LFPowerGrid\data\sorter\lf_sorter.p3d";
        weight = 3000;
        itemSize[] = {3, 3};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {"zbytek"};
        hiddenSelectionsTextures[] = {""};
        hiddenSelectionsMaterials[] = {""};
    };

    // ---- Sorter (placed device, PASSTHROUGH 5 u/s) ----
    class LF_Sorter : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_Sorter";
        descriptionShort = "$STR_LFPG_Sorter_Desc";
        model = "\LFPowerGrid\data\sorter\lf_sorter.p3d";
        weight = 5000;
        itemSize[] = {3, 3};
        rotationFlags = 17;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        hiddenSelections[] = { "sorter_led" };
        hiddenSelectionsTextures[] = { "\LFPowerGrid\data\sorter\textures\led_co.paa" };
        hiddenSelectionsMaterials[] = { "\LFPowerGrid\data\sorter\materials\lf_sorter_led_off.rvmat" };
    };

    // =========================================================
    // v1.4.0: SEARCHLIGHT (CONSUMER, directional light)
    // =========================================================

    // ---- Searchlight Kit (holdable, deployable, same-model) ----
    class LF_Searchlight_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_SearchlightKit";
        descriptionShort = "$STR_LFPG_SearchlightKit_Desc";
        model = "\LFPowerGrid\data\searchlight\lf_searchlight.p3d";
        weight = 4000;
        itemSize[] = {4, 3};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {};
    };

    // ---- Searchlight (placed device) ----
    class LF_Searchlight : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_Searchlight";
        descriptionShort = "$STR_LFPG_Searchlight_Desc";
        model = "\LFPowerGrid\data\searchlight\lf_searchlight.p3d";
        weight = 5000;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        hiddenSelections[] = { "lens_glow" };
        hiddenSelectionsTextures[] = { "" };
        hiddenSelectionsMaterials[] = { "\LFPowerGrid\data\searchlight\lf_searchlight_lens_off.rvmat" };
    };
	
    // =========================================================
    // v1.5.0: MOTION SENSOR (PASSTHROUGH, gated by detection)
    // =========================================================

    // ---- MotionSensor Kit (holdable, deployable, same-model) ----
    class LFPG_MotionSensor_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_MotionSensorKit";
        descriptionShort = "$STR_LFPG_MotionSensorKit_Desc";
        model = "\LFPowerGrid\data\sensor\lfpg_motion_sensor.p3d";
        weight = 600;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {};
    };

    // ---- MotionSensor (placed device, PASSTHROUGH 1 IN + 1 OUT) ----
    class LFPG_MotionSensor : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_MotionSensor";
        descriptionShort = "$STR_LFPG_MotionSensor_Desc";
        model = "\LFPowerGrid\data\sensor\lfpg_motion_sensor.p3d";
        weight = 800;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;

        hiddenSelections[] = {"led_indicator"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\sensor\materials\sensor_led_off.rvmat"};

        class DamageSystem
        {
            class DamageZones
            {
                class GlobalHealth
                {
                    class Health
                    {
                        hitpoints = 100;
                    };
                };
            };
        };
    };

    // =========================================================
    // v1.8.0: PRESSURE PAD (PASSTHROUGH, gated by player presence)
    //   1 IN + 1 OUT, capacity 20 u/s, consumption 5 u/s
    //   Animated pad depression when player stands on it
    // =========================================================

    // ---- PressurePad Kit (holdable, deployable, same-model) ----
    class LFPG_PressurePad_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_PressurePadKit";
        descriptionShort = "$STR_LFPG_PressurePadKit_Desc";
        model = "\LFPowerGrid\pressure_pad\pressure_pad.p3d";
        weight = 500;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {"model_0", "pressure_pad", "port_input_0", "port_output_0"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\pressure_pad\data\pressure_pad_co.paa", "\LFPowerGrid\pressure_pad\data\pressure_pad_co.paa", "\LFPowerGrid\pressure_pad\data\pressure_pad_grey.paa", "\LFPowerGrid\pressure_pad\data\pressure_pad_black.paa"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\pressure_pad\data\pressure_pad.rvmat", "\LFPowerGrid\pressure_pad\data\pressure_pad.rvmat", "\LFPowerGrid\pressure_pad\data\pressure_pad.rvmat", "\LFPowerGrid\pressure_pad\data\pressure_pad.rvmat"};
    };

    // ---- PressurePad (placed device, PASSTHROUGH 1 IN + 1 OUT) ----
    // Ports: input_1, output_1 (memory points: port_input_0, port_output_0)
    // Gate: opens when player stands on pad, closes when player leaves.
    class LFPG_PressurePad : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_PressurePad";
        descriptionShort = "$STR_LFPG_PressurePad_Desc";
        model = "\LFPowerGrid\pressure_pad\pressure_pad.p3d";
        weight = 800;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;

        hiddenSelections[] = {"model_0", "pressure_pad", "port_input_0", "port_output_0"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\pressure_pad\data\pressure_pad_co.paa", "\LFPowerGrid\pressure_pad\data\pressure_pad_co.paa", "\LFPowerGrid\pressure_pad\data\pressure_pad_grey.paa", "\LFPowerGrid\pressure_pad\data\pressure_pad_black.paa"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\pressure_pad\data\pressure_pad.rvmat", "\LFPowerGrid\pressure_pad\data\pressure_pad.rvmat", "\LFPowerGrid\pressure_pad\data\pressure_pad.rvmat", "\LFPowerGrid\pressure_pad\data\pressure_pad.rvmat"};

        class AnimationSources
        {
            class pad_press
            {
                source = "user";
                initPhase = 0;
                animPeriod = 0.3;
            };
        };

        class DamageSystem
        {
            class DamageZones
            {
                class GlobalHealth
                {
                    class Health
                    {
                        hitpoints = 100;
                    };
                };
            };
        };
    };

    // =========================================================
    // v1.7.0: LOGIC GATES (AND / OR / XOR)
    //   PASSTHROUGH, 2 IN + 1 OUT, capacity 20 u/s
    //   Shared model, different symbol texture via hiddenSelections
    // =========================================================

    // ---- AND Gate Kit (holdable, deployable, same-model) ----
    class LFPG_AND_Gate_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_AND_Gate_Kit";
        descriptionShort = "$STR_LFPG_AND_Gate_Kit_Desc";
        model = "\LFPowerGrid\data\logic_gate\AND_OR_XOR_Memory_cell.p3d";
        weight = 800;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {"camo"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\data\logic_gate\data\memory_cell_symbol_and.paa"};
    };

    // ---- OR Gate Kit ----
    class LFPG_OR_Gate_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_OR_Gate_Kit";
        descriptionShort = "$STR_LFPG_OR_Gate_Kit_Desc";
        model = "\LFPowerGrid\data\logic_gate\AND_OR_XOR_Memory_cell.p3d";
        weight = 800;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {"camo"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\data\logic_gate\data\memory_cell_symbol_or.paa"};
    };

    // ---- XOR Gate Kit ----
    class LFPG_XOR_Gate_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_XOR_Gate_Kit";
        descriptionShort = "$STR_LFPG_XOR_Gate_Kit_Desc";
        model = "\LFPowerGrid\data\logic_gate\AND_OR_XOR_Memory_cell.p3d";
        weight = 800;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {"camo"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\data\logic_gate\data\memory_cell_symbol_xor.paa"};
    };

    // ---- AND Gate (placed device) ----
    class LFPG_AND_Gate : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_AND_Gate";
        descriptionShort = "$STR_LFPG_AND_Gate_Desc";
        model = "\LFPowerGrid\data\logic_gate\AND_OR_XOR_Memory_cell.p3d";
        weight = 1200;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        hiddenSelections[] = {"camo", "light_led_input0", "light_led_input1", "light_led_output0"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\data\logic_gate\data\memory_cell_symbol_and.paa", "", "", ""};
        hiddenSelectionsMaterials[] = {"", "\LFPowerGrid\data\button\materials\led_off.rvmat", "\LFPowerGrid\data\button\materials\led_off.rvmat", "\LFPowerGrid\data\button\materials\led_off.rvmat"};
    };

    // ---- OR Gate (placed device) ----
    class LFPG_OR_Gate : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_OR_Gate";
        descriptionShort = "$STR_LFPG_OR_Gate_Desc";
        model = "\LFPowerGrid\data\logic_gate\AND_OR_XOR_Memory_cell.p3d";
        weight = 1200;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        hiddenSelections[] = {"camo", "light_led_input0", "light_led_input1", "light_led_output0"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\data\logic_gate\data\memory_cell_symbol_or.paa", "", "", ""};
        hiddenSelectionsMaterials[] = {"", "\LFPowerGrid\data\button\materials\led_off.rvmat", "\LFPowerGrid\data\button\materials\led_off.rvmat", "\LFPowerGrid\data\button\materials\led_off.rvmat"};
    };

    // ---- XOR Gate (placed device) ----
    class LFPG_XOR_Gate : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_XOR_Gate";
        descriptionShort = "$STR_LFPG_XOR_Gate_Desc";
        model = "\LFPowerGrid\data\logic_gate\AND_OR_XOR_Memory_cell.p3d";
        weight = 1200;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        hiddenSelections[] = {"camo", "light_led_input0", "light_led_input1", "light_led_output0"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\data\logic_gate\data\memory_cell_symbol_xor.paa", "", "", ""};
        hiddenSelectionsMaterials[] = {"", "\LFPowerGrid\data\button\materials\led_off.rvmat", "\LFPowerGrid\data\button\materials\led_off.rvmat", "\LFPowerGrid\data\button\materials\led_off.rvmat"};
    };

    // =========================================================
    // v1.9.0: LASER DETECTOR (PASSTHROUGH, gated by beam crossing)
    //   1 IN + 1 OUT, capacity 20 u/s, consumption 5 u/s
    //   Emits red laser beam, detects when player crosses it
    // =========================================================

    // ---- LaserDetector Kit (holdable, deployable, same-model) ----
    class LFPG_LaserDetector_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_LaserDetectorKit";
        descriptionShort = "$STR_LFPG_LaserDetectorKit_Desc";
        model = "\LFPowerGrid\data\laser_detector\laser_detector.p3d";
        weight = 600;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {"light_led"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\button\materials\led_off.rvmat"};
    };

    // ---- LaserDetector (placed device, PASSTHROUGH 1 IN + 1 OUT) ----
    // Ports: input_1, output_1 (memory points: port_input_0, port_output_0)
    // Gate: opens when player crosses laser beam, closes when player leaves.
    class LFPG_LaserDetector : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_LaserDetector";
        descriptionShort = "$STR_LFPG_LaserDetector_Desc";
        model = "\LFPowerGrid\data\laser_detector\laser_detector.p3d";
        weight = 800;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;

        hiddenSelections[] = {"light_led"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\button\materials\led_off.rvmat"};

        class DamageSystem
        {
            class DamageZones
            {
                class GlobalHealth
                {
                    class Health
                    {
                        hitpoints = 100;
                    };
                };
            };
        };
    };

    // =========================================================
    // v2.0.0: ELECTRONIC COUNTER (PASSTHROUGH, 2 IN + 1 OUT)
    //   input_0 = power, input_1 = toggle (edge-detected)
    //   output_0 = momentary pulse on 9→wrap
    //   Capacity 20 u/s, consumption 5 u/s
    //   7-segment display: show_0..show_9 animations
    // =========================================================

    // ---- ElectronicCounter Kit (holdable, deployable) ----
    class LFPG_ElectronicCounter_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_ElectronicCounter_Kit";
        descriptionShort = "$STR_LFPG_ElectronicCounter_Kit_Desc";
        model = "\LFPowerGrid\electronic_counter\electronic_counter.p3d";
        weight = 600;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {"camo"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\electronic_counter\data\electronic_counter_grey.paa"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\electronic_counter\data\electronic_counter.rvmat"};
    };

    // ---- ElectronicCounter (placed device, PASSTHROUGH 2 IN + 1 OUT) ----
    class LFPG_ElectronicCounter : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_ElectronicCounter";
        descriptionShort = "$STR_LFPG_ElectronicCounter_Desc";
        model = "\LFPowerGrid\electronic_counter\electronic_counter.p3d";
        weight = 800;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;

        hiddenSelections[] = {"camo", "camo2", "light_led"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\electronic_counter\data\electronic_counter_grey.paa", "\LFPowerGrid\electronic_counter\data\electronic_counter_black.paa", ""};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\electronic_counter\data\electronic_counter.rvmat", "\LFPowerGrid\electronic_counter\data\electronic_counter.rvmat", "\LFPowerGrid\data\button\materials\led_off.rvmat"};

        class AnimationSources
        {
            class show_0
            {
                source = "user";
                initPhase = 0;
                animPeriod = 0.1;
            };
            class show_1 : show_0 {};
            class show_2 : show_0 {};
            class show_3 : show_0 {};
            class show_4 : show_0 {};
            class show_5 : show_0 {};
            class show_6 : show_0 {};
            class show_7 : show_0 {};
            class show_8 : show_0 {};
            class show_9 : show_0 {};
        };

        class DamageSystem
        {
            class DamageZones
            {
                class GlobalHealth
                {
                    class Health
                    {
                        hitpoints = 100;
                    };
                };
            };
        };
    };

    // =========================================================
    // v2.0: BATTERY MEDIUM (PASSTHROUGH + energy storage, UPS model)
    // =========================================================

    // ---- BatteryMedium Kit (holdable, deployable, same-model) ----
    class LF_BatteryMedium_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_BatteryMediumKit";
        descriptionShort = "$STR_LFPG_BatteryMediumKit_Desc";
        model = "\LFPowerGrid\data\battery_medium\ups.p3d";
        weight = 8000;
        itemSize[] = {3, 3};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {};
    };

    // ---- BatteryMedium (placed device, PASSTHROUGH 1 IN + 1 OUT) ----
    class LF_BatteryMedium : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_BatteryMedium";
        descriptionShort = "$STR_LFPG_BatteryMedium_Desc";
        model = "\LFPowerGrid\data\battery_medium\ups.p3d";
        weight = 12000;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;

        hiddenSelections[] = {"light_led_0", "light_led_1", "light_led_2", "light_led_3", "light_led_4", "light_led_5", "light_led_6"};
        hiddenSelectionsTextures[] = {"", "", "", "", "", "", ""};
        hiddenSelectionsMaterials[] = {
            "\LFPowerGrid\data\battery_medium\ups_led_off.rvmat",
            "\LFPowerGrid\data\battery_medium\ups_led_off.rvmat",
            "\LFPowerGrid\data\battery_medium\ups_led_off.rvmat",
            "\LFPowerGrid\data\battery_medium\ups_led_off.rvmat",
            "\LFPowerGrid\data\battery_medium\ups_led_off.rvmat",
            "\LFPowerGrid\data\battery_medium\ups_led_off.rvmat",
            "\LFPowerGrid\data\battery_medium\ups_led_off.rvmat"
        };

        class AnimationSources
        {
            class switch
            {
                source = "user";
                initPhase = 0;
                animPeriod = 0.3;
            };
        };

        class DamageSystem
        {
            class DamageZones
            {
                class GlobalHealth
                {
                    class Health
                    {
                        hitpoints = 200;
                    };
                };
            };
        };

        // EnergyManager: drives vanilla inventory white charge bar.
        // All actual energy logic is handled by LFPG timer + SyncCompEM().
        // energyStorageMax MUST match LFPG_BATTERY_MEDIUM_CAPACITY (10000).
        class EnergyManager
        {
            energyStorageMax = 10000;
            energyAtSpawn = 0;
            convertEnergyToQuantity = 1;
            updateInterval = 0;
            isPassiveDevice = 1;
            canReceiveAttachment = 0;
            canWork = 0;
        };
    };

    // =========================================================
    // v2.0: BATTERY LARGE (PASSTHROUGH + energy storage, transformer model)
    // Different-model kit: box kit → hologram of transformer → spawn
    // =========================================================

    // ---- BatteryLarge Kit (box, different-model hologram) ----
    class LF_BatteryLarge_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_BatteryLargeKit";
        descriptionShort = "$STR_LFPG_BatteryLargeKit_Desc";
        model = "\LFPowerGrid\data\kits\lf_kit_box.p3d";
        weight = 15000;
        itemSize[] = {5, 3};
        rotationFlags = 2;
        itemBehaviour = 2;
        canBeDigged = 0;
        carveNavmesh = 1;
        physLayer = "item_small";
        SingleUseActions[] = {527};
        ContinuousActions[] = {231};
    };

    // ---- BatteryLarge (placed device, PASSTHROUGH 1 IN + 1 OUT) ----
    class LF_BatteryLarge : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_BatteryLarge";
        descriptionShort = "$STR_LFPG_BatteryLarge_Desc";
        model = "\LFPowerGrid\data\battery_large\substation_transformer.p3d";
        weight = 25000;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;

        hiddenSelections[] = {"light_led_0", "light_led_1", "light_led_2", "light_led_3", "light_led_4", "light_led_5", "light_led_6"};
        hiddenSelectionsTextures[] = {"", "", "", "", "", "", ""};
        hiddenSelectionsMaterials[] = {
            "\LFPowerGrid\data\battery_large\substation_transformer_led_off.rvmat",
            "\LFPowerGrid\data\battery_large\substation_transformer_led_off.rvmat",
            "\LFPowerGrid\data\battery_large\substation_transformer_led_off.rvmat",
            "\LFPowerGrid\data\battery_large\substation_transformer_led_off.rvmat",
            "\LFPowerGrid\data\battery_large\substation_transformer_led_off.rvmat",
            "\LFPowerGrid\data\battery_large\substation_transformer_led_off.rvmat",
            "\LFPowerGrid\data\battery_large\substation_transformer_led_off.rvmat"
        };

        class AnimationSources
        {
            class switch
            {
                source = "user";
                initPhase = 0;
                animPeriod = 0.3;
            };
        };

        class DamageSystem
        {
            class DamageZones
            {
                class GlobalHealth
                {
                    class Health
                    {
                        hitpoints = 500;
                    };
                };
            };
        };

        // EnergyManager: drives vanilla inventory white charge bar.
        // All actual energy logic is handled by LFPG timer + SyncCompEM().
        // energyStorageMax MUST match LFPG_BATTERY_LARGE_CAPACITY (50000).
        class EnergyManager
        {
            energyStorageMax = 50000;
            energyAtSpawn = 0;
            convertEnergyToQuantity = 1;
            updateInterval = 0;
            isPassiveDevice = 1;
            canReceiveAttachment = 0;
            canWork = 0;
        };
    };

    // =========================================================
    // v3.0: DOOR CONTROLLER (CONSUMER, auto open/close doors)
    //   input_0 = power input (5 u/s)
    //   Pairs to nearest door within 1m (Fence/BBP/Building)
    //   Powered = open, Unpowered = close
    // =========================================================

    // ---- DoorController Kit (holdable, deployable, same-model) ----
    class LF_DoorController_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_DoorController_Kit";
        descriptionShort = "$STR_LFPG_DoorController_Kit_Desc";
        model = "\LFPowerGrid\door_controller\door_controller.p3d";
        weight = 500;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {};
    };

    // ---- DoorController (placed device, CONSUMER 1 IN) ----
    class LF_DoorController : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_DoorController";
        descriptionShort = "$STR_LFPG_DoorController_Desc";
        model = "\LFPowerGrid\door_controller\door_controller.p3d";
        weight = 500;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;

        hiddenSelections[] = {"bolt", "light_led", "screen", "camo"};
        hiddenSelectionsTextures[] =
        {
            "\LFPowerGrid\door_controller\data\door_controller_grey.paa",
            "\LFPowerGrid\door_controller\data\door_controller_red.paa",
            "\LFPowerGrid\door_controller\data\door_controller_grey.paa",
            "\LFPowerGrid\door_controller\data\door_controller_grey.paa"
        };
        hiddenSelectionsMaterials[] =
        {
            "\LFPowerGrid\door_controller\data\door_controller.rvmat",
            "\LFPowerGrid\door_controller\data\door_controller_red.rvmat",
            "\LFPowerGrid\door_controller\data\door_controller.rvmat",
            "\LFPowerGrid\door_controller\data\door_controller.rvmat"
        };

        class AnimationSources
        {
            class bolt
            {
                source = "user";
                initPhase = 0;
                animPeriod = 0.3;
            };
        };

        class DamageSystem
        {
            class DamageZones
            {
                class GlobalHealth
                {
                    class Health
                    {
                        hitpoints = 200;
                    };
                };
            };
        };
    };

    // =========================================================
    // v3.0: INTERCOM / RF BROADCASTER (CONSUMER, 2 IN ports)
    // =========================================================

    // ---- Intercom Kit (holdable, same-model deploy) ----
    class LF_Intercom_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_INTERCOM_KIT";
        descriptionShort = "$STR_LFPG_INTERCOM_KIT_DESC";
        model = "\LFPowerGrid\rf_broadcaster\rf_broadcaster.p3d";
        weight = 2000;
        itemSize[] = {3, 3};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {"camo", "camoscreen", "light_led", "light_led2"};
        hiddenSelectionsTextures[] =
        {
            "\LFPowerGrid\rf_broadcaster\data\rf_broadcaster_co.paa",
            "\LFPowerGrid\rf_broadcaster\data\rf_broadcaster_glass.paa",
            "",
            ""
        };
        hiddenSelectionsMaterials[] =
        {
            "\LFPowerGrid\rf_broadcaster\data\rf_broadcaster.rvmat",
            "\LFPowerGrid\rf_broadcaster\data\rf_broadcaster.rvmat",
            "\LFPowerGrid\rf_broadcaster\data\led_off.rvmat",
            "\LFPowerGrid\rf_broadcaster\data\led_off.rvmat"
        };
    };

    // ---- Intercom (placed device, CONSUMER 2 IN ports) ----
    // Ports: input_1 (power), input_toggle (RF trigger signal)
    // Memory: port_input_0 -> input_1, port_output_0 -> input_toggle
    // T1: 10 u/s consumption, toggle on/off, LED1 feedback
    // T2: 20 u/s consumption, ghost radio, frequency cycling, LED2
    class LF_Intercom : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_INTERCOM";
        descriptionShort = "$STR_LFPG_INTERCOM_DESC";
        model = "\LFPowerGrid\rf_broadcaster\rf_broadcaster.p3d";
        weight = 3000;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;

        hiddenSelections[] = {"camo", "camoscreen", "light_led", "light_led2", "microphone"};
        hiddenSelectionsTextures[] =
        {
            "\LFPowerGrid\rf_broadcaster\data\rf_broadcaster_co.paa",
            "\LFPowerGrid\rf_broadcaster\data\rf_broadcaster_glass.paa",
            "",
            "",
            ""
        };
        hiddenSelectionsMaterials[] =
        {
            "\LFPowerGrid\rf_broadcaster\data\rf_broadcaster.rvmat",
            "\LFPowerGrid\rf_broadcaster\data\rf_broadcaster.rvmat",
            "\LFPowerGrid\rf_broadcaster\data\led_off.rvmat",
            "\LFPowerGrid\rf_broadcaster\data\led_off.rvmat",
            "\LFPowerGrid\rf_broadcaster\data\rf_broadcaster.rvmat"
        };

        class AnimationSources
        {
            class knob_freq
            {
                source = "user";
                initPhase = 0;
                animPeriod = 0.3;
            };
            class knob_input
            {
                source = "user";
                initPhase = 0;
                animPeriod = 0.3;
            };
            class knob_vol
            {
                source = "user";
                initPhase = 0;
                animPeriod = 0.3;
            };
        };

        class DamageSystem
        {
            class DamageZones
            {
                class Zone_Housing
                {
                    class Health
                    {
                        hitpoints = 200;
                        healthLevels[] =
                        {
                            {1.0, {}},
                            {0.7, {}},
                            {0.5, {}},
                            {0.3, {}},
                            {0.0, {}}
                        };
                    };
                    componentNames[] = {"Component01"};
                    fatalInjuryCoef = -1;
                };
            };
        };
    };

    // =========================================================
    // v3.0: GHOST RADIO (invisible transceiver for Intercom T2)
    // NOT spawnable by players (scope=1). No attachments, no actions.
    // CompEM disabled in script. Tiny invisible model.
    // =========================================================
    class LF_GhostRadio : PersonalRadio
    {
        scope = 1;
        displayName = "";
        descriptionShort = "";
        model = "\dz\gear\consumables\Stone.p3d";
        weight = 0;
        itemSize[] = {0, 0};
    };
};
