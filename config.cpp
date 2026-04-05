// ---- v0.8.0: Attachment slots for Solar Panel upgrade materials ----
class CfgSlots
{
    class Slot_LFPG_SolarPlate
    {
        name = "LFPG_SolarPlate";
        displayName = "Metal Plate";
        ghostIcon = "missing";
    };
    class Slot_LFPG_SolarNails
    {
        name = "LFPG_SolarNails";
        displayName = "Nails";
        ghostIcon = "missing";
    };
    // v3.1.0: Intercom radio slot — uses vanilla WalkieTalkie slot.
    // No custom slot needed; PersonalRadio already has inventorySlot="WalkieTalkie".
    // v1.1.0: Water Pump slots
    class Slot_LFPG_PumpPlate
    {
        name = "LFPG_PumpPlate";
        displayName = "Metal Plate";
        ghostIcon = "missing";
    };
    class Slot_LFPG_PumpNails
    {
        name = "LFPG_PumpNails";
        displayName = "Nails";
        ghostIcon = "missing";
    };
    // v1.0.0: Electric Stove 4th DirectCooking slot (vanilla only has A/B/C)
    class Slot_DirectCookingD
    {
        name = "DirectCookingD";
        displayName = "Cooking Slot";
        ghostIcon = "set:dayz_inventory image:directcooking";
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
        samples[] = {{ "\LFPowerGrid\data\pressure_pad\sounds\pressure_pad_press", 1 }};
        volume = 0.5;
        range = 8;
        rangeCurve[] = {{ 0, 1 }, { 4, 0.4 }, { 8, 0 }};
    };
    // v3.0: Intercom RF beep (one-shot, short electronic tone)
    class LFPG_Intercom_RFBeep_Shader
    {
        samples[] = {{ "\LFPowerGrid\data\rf_broadcaster\sounds\rf_beep", 1 }};
        volume = 0.6;
        range = 15;
        rangeCurve[] = {{ 0, 1 }, { 8, 0.4 }, { 15, 0 }};
    };
    // v3.0: Intercom knob click (one-shot, mechanical)
    class LFPG_Intercom_KnobClick_Shader
    {
        samples[] = {{ "\LFPowerGrid\data\rf_broadcaster\sounds\knob_click", 1 }};
        volume = 0.4;
        range = 5;
        rangeCurve[] = {{ 0, 1 }, { 3, 0.3 }, { 5, 0 }};
    };
    // v4.5: Furnace loop (ambient burning hum while m_SourceOn)
    class LFPG_Furnace_Loop_Shader
    {
        samples[] = {{ "\LFPowerGrid\data\furnace\Furnace", 1 }};
        volume = 0.4;
        range = 15;
        loop = 1;
        rangeCurve[] = {{ 0, 1 }, { 8, 0.5 }, { 15, 0 }};
    };
    // v3.0: Intercom static burst (one-shot, radio noise)
    class LFPG_Intercom_Static_Shader
    {
        samples[] = {{ "\LFPowerGrid\data\rf_broadcaster\sounds\static_burst", 1 }};
        volume = 0.5;
        range = 10;
        rangeCurve[] = {{ 0, 1 }, { 5, 0.4 }, { 10, 0 }};
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
    // v3.0: Intercom RF beep — spatial 3D, one-shot on RF toggle
    class LFPG_Intercom_RFBeep_SoundSet
    {
        soundShaders[] = { "LFPG_Intercom_RFBeep_Shader" };
        volumeFactor = 1;
        frequencyFactor = 1;
        spatial = 1;
    };
    // v3.0: Intercom knob click — spatial 3D, one-shot on frequency change
    class LFPG_Intercom_KnobClick_SoundSet
    {
        soundShaders[] = { "LFPG_Intercom_KnobClick_Shader" };
        volumeFactor = 1;
        frequencyFactor = 1;
        spatial = 1;
    };
    // v3.0: Intercom static burst — spatial 3D, one-shot on broadcast enable
    class LFPG_Intercom_Static_SoundSet
    {
        soundShaders[] = { "LFPG_Intercom_Static_Shader" };
        volumeFactor = 1;
        frequencyFactor = 1;
        spatial = 1;
    };
    // v4.5: Furnace loop — spatial 3D, played while m_SourceOn
    class LFPG_Furnace_Loop_SoundSet
    {
        soundShaders[] = { "LFPG_Furnace_Loop_Shader" };
        volumeFactor = 1;
        frequencyFactor = 1;
        spatial = 1;
    };
};

class CfgPatches
{
    class LFPowerGrid
    {
        units[] = { "LFPG_CableReel", "LFPG_Generator", "LF_TestLamp", "LF_TestLampHeavy", "LFPG_Splitter_Kit", "LFPG_Splitter", "LFPG_CeilingLight_Kit", "LFPG_CeilingLight", "LFPG_SolarPanel_Kit", "LFPG_SolarPanel", "LFPG_SolarPanel_T2", "LFPG_Combiner_Kit", "LFPG_Combiner", "LFPG_Camera_Kit", "LFPG_Camera", "LFPG_Monitor_Kit", "LFPG_Monitor", "LFPG_PushButton_Kit", "LFPG_PushButton", "LFPG_SwitchV2_Kit", "LFPG_SwitchV2", "LFPG_WaterPump_Kit", "LFPG_WaterPump", "LFPG_WaterPump_T2", "LFPG_Furnace_Kit", "LFPG_Furnace", "LFPG_Sorter_Kit", "LFPG_Sorter", "LFPG_Searchlight_Kit", "LFPG_Searchlight", "LFPG_MotionSensor_Kit", "LFPG_MotionSensor", "LFPG_AND_Gate_Kit", "LFPG_AND_Gate", "LFPG_OR_Gate_Kit", "LFPG_OR_Gate", "LFPG_XOR_Gate_Kit", "LFPG_XOR_Gate", "LFPG_MemoryCell_Kit", "LFPG_MemoryCell", "LFPG_PressurePad_Kit", "LFPG_PressurePad", "LFPG_LaserDetector_Kit", "LFPG_LaserDetector", "LFPG_ElectronicCounter_Kit", "LFPG_ElectronicCounter", "LFPG_BatteryMedium_Kit", "LFPG_BatteryMedium", "LFPG_BatteryLarge_Kit", "LFPG_BatteryLarge", "LFPG_DoorController_Kit", "LFPG_DoorController", "LFPG_Intercom_Kit", "LFPG_Intercom", "LFPG_GhostRadio", "LFPG_SwitchRemote_Kit", "LFPG_SwitchRemote", "LFPG_SwitchV2Remote_Kit", "LFPG_SwitchV2Remote", "LFPG_Fridge_Kit", "LFPG_Fridge", "LFPG_Sprinkler_Kit", "LFPG_Sprinkler", "LFPG_BatteryAdapter_Kit", "LFPG_BatteryAdapter", "LFPG_ElectricStove_Kit", "LFPG_ElectricStove", "LFPG_BTCAtm_Kit", "LFPG_BTCAtm", "LFPG_BTCAtmAdmin_Kit", "LFPG_BTCAtmAdmin", "LFPG_RemoteController", "LFPG_Speaker_Kit", "LFPG_Speaker", "LFPG_GhostPASReceiver"};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] = { "DZ_Data", "DZ_Scripts", "DZ_Gear_Tools", "DZ_Gear_Camping", "DZ_Gear_Containers", "DZ_Gear_Consumables", "DZ_Radio", "DZ_Gear_Cooking"};
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
    class PASReceiver;

    class LFPG_CableReel : CableReel
    {
        scope = 2;
        isDeployable = 0;
        displayName = "LF Cable Reel";
        descriptionShort = "Wiring tool for LF_PowerGrid.";
    };

    class LFPG_Generator : PowerGenerator
    {
        scope = 2;
        displayName = "Generator";
        descriptionShort = "LF_PowerGrid source device.";
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
    class LFPG_Splitter_Kit : Inventory_Base
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
    class LFPG_Splitter : Inventory_Base
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
    class LFPG_CeilingLight_Kit : Inventory_Base
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
    class LFPG_CeilingLight : Inventory_Base
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
    class LFPG_SolarPanel_Kit : Inventory_Base
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
    class LFPG_SolarPanel : Inventory_Base
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
        attachments[] = {"LFPG_SolarPlate", "LFPG_SolarNails"};
        class GUIInventoryAttachmentsProps
        {
            class UpgradeMaterials
            {
                name = "Upgrade Materials";
                description = "";
                attachmentSlots[] = {"LFPG_SolarPlate", "LFPG_SolarNails"};
                icon = "missing";
            };
        };
    };

    // ---- Solar Panel T2 (upgraded device, SOURCE 50 u/s) ----
    class LFPG_SolarPanel_T2 : Inventory_Base
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
    class LFPG_Combiner_Kit : Inventory_Base
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
    class LFPG_Combiner : Inventory_Base
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
    class LFPG_Camera_Kit : Inventory_Base
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
    class LFPG_Camera : Inventory_Base
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
    class LFPG_Monitor_Kit : Inventory_Base
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
    class LFPG_Monitor : Inventory_Base
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
        model = "\LFPowerGrid\data\switch_v1\switch_v1.p3d";
        weight = 500;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {"camo", "switch"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\data\switch_v1\data\switch_v1_co.paa", "\LFPowerGrid\data\switch_v1\data\switch_v1_co.paa"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\switch_v1\data\switch_v1.rvmat", "\LFPowerGrid\data\switch_v1\data\switch_v1.rvmat"};
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
        model = "\LFPowerGrid\data\switch_v1\switch_v1.p3d";
        weight = 800;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;

        hiddenSelections[] = {"camo", "switch", "light_led"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\data\switch_v1\data\switch_v1_co.paa", "\LFPowerGrid\data\switch_v1\data\switch_v1_co.paa", ""};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\switch_v1\data\switch_v1.rvmat", "\LFPowerGrid\data\switch_v1\data\switch_v1.rvmat", "\LFPowerGrid\data\switch_v1\data\led_off.rvmat"};

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
        model = "\LFPowerGrid\data\switch_v2\switch_v2.p3d";
        weight = 500;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {"camo"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\data\switch_v2\data\switch_v2_co.paa"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\switch_v2\data\switch_v2.rvmat"};
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
        model = "\LFPowerGrid\data\switch_v2\switch_v2.p3d";
        weight = 800;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;

        hiddenSelections[] = {"camo", "light_led"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\data\switch_v2\data\switch_v2_co.paa", ""};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\switch_v2\data\switch_v2.rvmat", "\LFPowerGrid\data\switch_v2\data\led_off.rvmat"};

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

    // ---- SwitchRemote Kit (holdable, same-model deploy, RF-capable) ----
    class LFPG_SwitchRemote_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_SWITCHREMOTE_KIT";
        descriptionShort = "$STR_LFPG_SWITCHREMOTE_KIT_DESC";
        model = "\LFPowerGrid\data\switch_v1_remote\switch_v1_remote.p3d";
        weight = 500;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {"camo", "camoswitch"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\data\switch_v1_remote\data\switch_v1_remote_co.paa", "\LFPowerGrid\data\switch_v1_remote\data\switch_v1_remote_co.paa"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\switch_v1_remote\data\switch_v1_remote.rvmat", "\LFPowerGrid\data\switch_v1_remote\data\switch_v1_remote.rvmat"};
    };

    // ---- SwitchRemote (placed device, PASSTHROUGH 1 IN + 1 OUT, RF-capable) ----
    // Ports: input_1, output_1 (memory points: port_input_0, port_output_0)
    // Toggle: latching ON/OFF (persisted across restart).
    // LED: green (passing power), red (blocking), off (disconnected)
    // RF: toggleable remotely via LFPG_Intercom broadcast
    class LFPG_SwitchRemote : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_SWITCHREMOTE";
        descriptionShort = "$STR_LFPG_SWITCHREMOTE_DESC";
        model = "\LFPowerGrid\data\switch_v1_remote\switch_v1_remote.p3d";
        weight = 800;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;

        hiddenSelections[] = {"camo", "camoswitch", "light_led"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\data\switch_v1_remote\data\switch_v1_remote_co.paa", "\LFPowerGrid\data\switch_v1_remote\data\switch_v1_remote_co.paa", ""};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\switch_v1_remote\data\switch_v1_remote.rvmat", "\LFPowerGrid\data\switch_v1_remote\data\switch_v1_remote.rvmat", "\LFPowerGrid\data\switch_v1_remote\data\led_off.rvmat"};

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
    // v3.1: SWITCH V2 REMOTE (PASSTHROUGH, latching toggle lever, RF-capable)
    // =========================================================

    // ---- SwitchV2Remote Kit (holdable, same-model deploy, RF-capable) ----
    class LFPG_SwitchV2Remote_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_SWITCHV2REMOTE_KIT";
        descriptionShort = "$STR_LFPG_SWITCHV2REMOTE_KIT_DESC";
        model = "\LFPowerGrid\data\switch_v2_remote\switch_v2_remote.p3d";
        weight = 500;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {"camo"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\data\switch_v2_remote\data\switch_v2_remote_co.paa"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\switch_v2_remote\data\switch_v2_remote.rvmat"};
    };

    // ---- SwitchV2Remote (placed device, PASSTHROUGH 1 IN + 1 OUT, RF-capable) ----
    // Ports: input_1, output_1 (memory points: port_input_0, port_output_0)
    // Toggle: latching ON/OFF (persisted across restart).
    // LED: green (passing power), red (blocking), off (disconnected)
    // RF: toggleable remotely via LFPG_Intercom broadcast
    class LFPG_SwitchV2Remote : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_SWITCHV2REMOTE";
        descriptionShort = "$STR_LFPG_SWITCHV2REMOTE_DESC";
        model = "\LFPowerGrid\data\switch_v2_remote\switch_v2_remote.p3d";
        weight = 800;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;

        hiddenSelections[] = {"camo", "light_led"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\data\switch_v2_remote\data\switch_v2_remote_co.paa", ""};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\switch_v2_remote\data\switch_v2_remote.rvmat", "\LFPowerGrid\data\switch_v2\data\led_off.rvmat"};

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
    class MetalPlate : Inventory_Base
    {
        inventorySlot[] += {"LFPG_SolarPlate", "LFPG_PumpPlate"};
    };
    class Nail : Inventory_Base
    {
        inventorySlot[] += {"LFPG_SolarNails", "LFPG_PumpNails"};
    };
	
	
	// =========================================================
    // v1.0.1: Vanilla cookware override — add DirectCookingD slot
    // Allows Pot/FryingPan/Cauldron to attach to the 4th stove burner.
    //
    // IMPORTANT: Explicit parent class required. Without it, the
    // engine may create a NEW class (scope=0) instead of extending
    // the vanilla class, causing the item to disappear from spawn.
    // =========================================================
	class Bottle_Base;
    class Pot : Bottle_Base
    {
        inventorySlot[] += {"DirectCookingD"};
    };

    class FryingPan : Inventory_Base
    {
        inventorySlot[] += {"DirectCookingD"};
    };

    class Cauldron : Bottle_Base
    {
        inventorySlot[] += {"DirectCookingD"};
    };
	
	
	
	
    // v1.1.0: GasMask_Filter — NO override needed.
    // Vanilla defines inventorySlot = "GasMaskFilter" (slot name, no underscore).
    // WaterPump uses the vanilla slot directly via attachments[]={"GasMaskFilter"}.
    // Class name is "GasMask_Filter" (with underscore) — do NOT use in attachments[].

    // v3.1.0: PersonalRadio — NO override needed.
    // Vanilla inventorySlot="WalkieTalkie". Intercom uses attachments[]={"WalkieTalkie"}
    // so PersonalRadio attaches directly. Compatible with Forward Operator Gear etc.

    // =========================================================
    // v1.1.0: WATER PUMP (PASSTHROUGH, filter + tank)
    // =========================================================

    // ---- Water Pump Kit (holdable, different-model deploy like Solar Panel) ----
    class LFPG_WaterPump_Kit : Inventory_Base
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
    class LFPG_WaterPump : Inventory_Base
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
        attachments[] = {"GasMaskFilter", "LFPG_PumpPlate", "LFPG_PumpNails"};
        hiddenSelections[] = {"pump_led"};
        hiddenSelectionsTextures[] = {""};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\waterpump\lf_pump_led_off.rvmat"};
        class GUIInventoryAttachmentsProps
        {
            class WaterFilter
            {
                name = "Water Filter";
                description = "";
                attachmentSlots[] = {"GasMaskFilter"};
                icon = "missing";
            };
            class UpgradeMaterials
            {
                name = "Upgrade Materials";
                description = "";
                attachmentSlots[] = {"LFPG_PumpPlate", "LFPG_PumpNails"};
                icon = "missing";
            };
        };
    };

    // ---- Water Pump T2 (upgraded device, PASSTHROUGH 50 u/s, cap 100 u/s + tank 50L) ----
    class LFPG_WaterPump_T2 : Inventory_Base
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
        attachments[] = {"GasMaskFilter"};
        hiddenSelections[] = {"pump_led"};
        hiddenSelectionsTextures[] = {""};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\waterpump\lf_pump_led_off.rvmat"};
        class GUIInventoryAttachmentsProps
        {
            class WaterFilter
            {
                name = "Water Filter";
                description = "";
                attachmentSlots[] = {"GasMaskFilter"};
                icon = "missing";
            };
        };
    };

    // =========================================================
    // v1.2.0: FURNACE (SOURCE, burns items for power)
    // =========================================================

    // ---- Furnace Kit (holdable, different-model deploy) ----
    class LFPG_Furnace_Kit : Inventory_Base
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
    class LFPG_Furnace : Inventory_Base
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
    class LFPG_Sorter_Kit : Inventory_Base
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
    class LFPG_Sorter : Inventory_Base
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
    class LFPG_Searchlight_Kit : Inventory_Base
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
    class LFPG_Searchlight : Inventory_Base
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
        hiddenSelections[] = { "light" };
        hiddenSelectionsTextures[] = { "" };
        hiddenSelectionsMaterials[] = { "\LFPowerGrid\data\searchlight\lf_searchlight_lens_off.rvmat" };

        class AnimationSources
        {
            class light_main
            {
                source = "user";
                initPhase = 0.5;
                animPeriod = 1;
            };
        };
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

        hiddenSelections[] = {"light_led"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\sensor\motion_sensor_off.rvmat"};

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
        model = "\LFPowerGrid\data\pressure_pad\pressure_pad.p3d";
        weight = 500;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {"port_input_0", "port_output_0"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\data\pressure_pad\data\pressure_pad_grey.paa", "\LFPowerGrid\data\pressure_pad\data\pressure_pad_black.paa"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\pressure_pad\data\pressure_pad.rvmat", "\LFPowerGrid\data\pressure_pad\data\pressure_pad.rvmat"};
    };

    // ---- PressurePad (placed device, PASSTHROUGH 1 IN + 1 OUT) ----
    // Ports: input_1, output_1 (memory points: port_input_0, port_output_0)
    // Gate: opens when player stands on pad, closes when player leaves.
    class LFPG_PressurePad : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_PressurePad";
        descriptionShort = "$STR_LFPG_PressurePad_Desc";
        model = "\LFPowerGrid\data\pressure_pad\pressure_pad.p3d";
        weight = 800;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;

        hiddenSelections[] = {"port_input_0", "port_output_0"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\data\pressure_pad\data\pressure_pad_grey.paa", "\LFPowerGrid\data\pressure_pad\data\pressure_pad_black.paa"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\pressure_pad\data\pressure_pad.rvmat", "\LFPowerGrid\data\pressure_pad\data\pressure_pad.rvmat"};

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
        model = "\LFPowerGrid\data\logic_gate\gate_and.p3d";
        weight = 800;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
    };

    // ---- OR Gate Kit ----
    class LFPG_OR_Gate_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_OR_Gate_Kit";
        descriptionShort = "$STR_LFPG_OR_Gate_Kit_Desc";
        model = "\LFPowerGrid\data\logic_gate\gate_or.p3d";
        weight = 800;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
    };

    // ---- XOR Gate Kit ----
    class LFPG_XOR_Gate_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_XOR_Gate_Kit";
        descriptionShort = "$STR_LFPG_XOR_Gate_Kit_Desc";
        model = "\LFPowerGrid\data\logic_gate\gate_xor.p3d";
        weight = 800;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
    };

    // ---- AND Gate (placed device) ----
    class LFPG_AND_Gate : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_AND_Gate";
        descriptionShort = "$STR_LFPG_AND_Gate_Desc";
        model = "\LFPowerGrid\data\logic_gate\gate_and.p3d";
        weight = 1200;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        hiddenSelections[] = {"light_led_input0", "light_led_input1", "light_led_output0"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\button\materials\led_off.rvmat", "\LFPowerGrid\data\button\materials\led_off.rvmat", "\LFPowerGrid\data\button\materials\led_off.rvmat"};
    };

    // ---- OR Gate (placed device) ----
    class LFPG_OR_Gate : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_OR_Gate";
        descriptionShort = "$STR_LFPG_OR_Gate_Desc";
        model = "\LFPowerGrid\data\logic_gate\gate_or.p3d";
        weight = 1200;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        hiddenSelections[] = {"light_led_input0", "light_led_input1", "light_led_output0"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\button\materials\led_off.rvmat", "\LFPowerGrid\data\button\materials\led_off.rvmat", "\LFPowerGrid\data\button\materials\led_off.rvmat"};
    };

    // ---- XOR Gate (placed device) ----
    class LFPG_XOR_Gate : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_XOR_Gate";
        descriptionShort = "$STR_LFPG_XOR_Gate_Desc";
        model = "\LFPowerGrid\data\logic_gate\gate_xor.p3d";
        weight = 1200;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        hiddenSelections[] = {"light_led_input0", "light_led_input1", "light_led_output0"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\button\materials\led_off.rvmat", "\LFPowerGrid\data\button\materials\led_off.rvmat", "\LFPowerGrid\data\button\materials\led_off.rvmat"};
    };

    // =========================================================
    // v3.1.0: MEMORY CELL (PASSTHROUGH, stateful latch)
    //   4 IN + 2 OUT, capacity 20 u/s, consumption 0 u/s
    //   Latches on/off state; routes power to normal or inverted output.
    // =========================================================

    // ---- Memory Cell Kit ----
    class LFPG_MemoryCell_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_MemoryCell_Kit";
        descriptionShort = "$STR_LFPG_MemoryCell_Kit_Desc";
        model = "\LFPowerGrid\data\logic_gate\memory_cell.p3d";
        weight = 800;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
    };

    // ---- Memory Cell (placed device) ----
    class LFPG_MemoryCell : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_MemoryCell";
        descriptionShort = "$STR_LFPG_MemoryCell_Desc";
        model = "\LFPowerGrid\data\logic_gate\memory_cell.p3d";
        weight = 1200;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        hiddenSelections[] = {"light_led_input0", "light_led_input1", "light_led_output0", "light_led_input2", "light_led_input3"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\button\materials\led_off.rvmat", "\LFPowerGrid\data\button\materials\led_off.rvmat", "\LFPowerGrid\data\button\materials\led_off.rvmat", "\LFPowerGrid\data\button\materials\led_off.rvmat", "\LFPowerGrid\data\button\materials\led_off.rvmat"};
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
        model = "\LFPowerGrid\data\electronic_counter\electronic_counter.p3d";
        weight = 600;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {"camo"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\data\electronic_counter\electronic_counter_grey.paa"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\electronic_counter\electronic_counter.rvmat"};
    };

    // ---- ElectronicCounter (placed device, PASSTHROUGH 2 IN + 1 OUT) ----
    class LFPG_ElectronicCounter : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_ElectronicCounter";
        descriptionShort = "$STR_LFPG_ElectronicCounter_Desc";
        model = "\LFPowerGrid\data\electronic_counter\electronic_counter.p3d";
        weight = 800;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;

        hiddenSelections[] = {"camo", "camo2", "light_led"};
        hiddenSelectionsTextures[] = {"\LFPowerGrid\data\electronic_counter\electronic_counter_grey.paa", "\LFPowerGrid\data\electronic_counter\electronic_counter_black.paa", ""};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\electronic_counter\electronic_counter.rvmat", "\LFPowerGrid\data\electronic_counter\electronic_counter.rvmat", "\LFPowerGrid\data\button\materials\led_off.rvmat"};

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
    class LFPG_BatteryMedium_Kit : Inventory_Base
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
    class LFPG_BatteryMedium : Inventory_Base
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
        varQuantityMax = 10000;
        quantityBar = 1;
        destroyOnEmpty = 0;
        varQuantityDestroyOnMin = 0;
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
            class GlobalHealth
            {
                class Health
                {
                    hitpoints = 200;
                    healthLevels[] = {{1.0,{}},{0.7,{}},{0.5,{}},{0.3,{}}};
                };
            };
            class DamageZones
            {
                class GlobalHealth
                {
                    class Health
                    {
                        hitpoints = 200;
                    };
                    fatalInjuryCoef = -1;
                    componentNames[] = {};
                    transferToZonesNames[] = {};
                    transferToZonesCoefs[] = {};
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
    class LFPG_BatteryLarge_Kit : Inventory_Base
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
    class LFPG_BatteryLarge : Inventory_Base
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
        varQuantityMax = 50000;
        quantityBar = 1;
        destroyOnEmpty = 0;
        varQuantityDestroyOnMin = 0;
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
            class GlobalHealth
            {
                class Health
                {
                    hitpoints = 500;
                    healthLevels[] = {{1.0,{}},{0.7,{}},{0.5,{}},{0.3,{}}};
                };
            };
            class DamageZones
            {
                class GlobalHealth
                {
                    class Health
                    {
                        hitpoints = 500;
                    };
                    fatalInjuryCoef = -1;
                    componentNames[] = {};
                    transferToZonesNames[] = {};
                    transferToZonesCoefs[] = {};
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
    class LFPG_DoorController_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_DoorController_Kit";
        descriptionShort = "$STR_LFPG_DoorController_Kit_Desc";
        model = "\LFPowerGrid\data\door_controller\door_controller.p3d";
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
    class LFPG_DoorController : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_DoorController";
        descriptionShort = "$STR_LFPG_DoorController_Desc";
        model = "\LFPowerGrid\data\door_controller\door_controller.p3d";
        weight = 500;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;

        hiddenSelections[] = {"bolt", "light_led", "screen", "camo"};
        hiddenSelectionsTextures[] =
        {
            "\LFPowerGrid\data\door_controller\data\door_controller_grey.paa",
            "\LFPowerGrid\data\door_controller\data\door_controller_red.paa",
            "\LFPowerGrid\data\door_controller\data\door_controller_grey.paa",
            "\LFPowerGrid\data\door_controller\data\door_controller_grey.paa"
        };
        hiddenSelectionsMaterials[] =
        {
            "\LFPowerGrid\data\door_controller\data\door_controller.rvmat",
            "\LFPowerGrid\data\door_controller\data\door_controller_red.rvmat",
            "\LFPowerGrid\data\door_controller\data\door_controller.rvmat",
            "\LFPowerGrid\data\door_controller\data\door_controller.rvmat"
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
    class LFPG_Intercom_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_INTERCOM_KIT";
        descriptionShort = "$STR_LFPG_INTERCOM_KIT_DESC";
        model = "\LFPowerGrid\data\rf_broadcaster\rf_broadcaster.p3d";
        weight = 2000;
        itemSize[] = {3, 3};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {"camo", "camoscreen", "light_led", "light_led2", "microphone"};
        hiddenSelectionsTextures[] =
        {
            "\LFPowerGrid\data\rf_broadcaster\data\rf_broadcaster_co.paa",
            "\LFPowerGrid\data\rf_broadcaster\data\rf_broadcaster_glass.paa",
            "",
            "",
            ""
        };
        hiddenSelectionsMaterials[] =
        {
            "\LFPowerGrid\data\rf_broadcaster\data\rf_broadcaster.rvmat",
            "\LFPowerGrid\data\rf_broadcaster\data\rf_broadcaster.rvmat",
            "\LFPowerGrid\data\rf_broadcaster\data\led_off.rvmat",
            "\LFPowerGrid\data\rf_broadcaster\data\led_off.rvmat",
            ""
        };
    };

    // ---- Intercom (placed device, CONSUMER 2 IN ports) ----
    // Ports: input_1 (power), input_toggle (RF trigger signal)
    // Memory: port_input_0 -> input_1, port_output_0 -> input_toggle
    // T1: 10 u/s consumption, toggle on/off, LED1 feedback
    // T2: 20 u/s consumption, ghost radio, frequency cycling, LED2
    class LFPG_Intercom : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_INTERCOM";
        descriptionShort = "$STR_LFPG_INTERCOM_DESC";
        model = "\LFPowerGrid\data\rf_broadcaster\rf_broadcaster.p3d";
        weight = 3000;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        attachments[] = {"WalkieTalkie"};

        hiddenSelections[] = {"camo", "camoscreen", "light_led", "light_led2", "microphone"};
        hiddenSelectionsTextures[] =
        {
            "\LFPowerGrid\data\rf_broadcaster\data\rf_broadcaster_co.paa",
            "\LFPowerGrid\data\rf_broadcaster\data\rf_broadcaster_glass.paa",
            "",
            "",
            ""
        };
        hiddenSelectionsMaterials[] =
        {
            "\LFPowerGrid\data\rf_broadcaster\data\rf_broadcaster.rvmat",
            "\LFPowerGrid\data\rf_broadcaster\data\rf_broadcaster.rvmat",
            "\LFPowerGrid\data\rf_broadcaster\data\led_off.rvmat",
            "\LFPowerGrid\data\rf_broadcaster\data\led_off.rvmat",
            ""
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
	class PersonalRadio;
    class LFPG_GhostRadio : PersonalRadio
    {
        scope = 1;
        displayName = "";
        descriptionShort = "";
        model = "\dz\gear\consumables\Stone.p3d";
        weight = 0;
        itemSize[] = {0, 0};
    };

    // =========================================================
    // v4.0: FRIDGE (CONSUMER, powered cooling container)
    //   input_1 = power input (20 u/s)
    //   Cargo: 10x50 = 500 slots, food/drink only
    //   Door animation, LED indicator, cooling when powered + closed
    //   Temperature target: 5 C
    // =========================================================

    // ---- Fridge Kit (holdable box, hologram shows fridge) ----
    class LFPG_Fridge_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_FridgeKit";
        descriptionShort = "$STR_LFPG_FridgeKit_Desc";
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

    // ---- Fridge (placed device, CONSUMER 1 IN, 20 u/s) ----
    class LFPG_Fridge : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_Fridge";
        descriptionShort = "$STR_LFPG_Fridge_Desc";
        model = "\LFPowerGrid\data\fridge\fridge.p3d";
        weight = 15000;
        itemSize[] = {10, 10};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;

        // Cargo: 10 wide x 50 tall = 500 slots
        itemsCargoSize[] = {10, 50};

        // hiddenSelections[0] = "light_led" -> LED material swap
        hiddenSelections[] = {"light_led"};
        hiddenSelectionsTextures[] = {""};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\fridge\led_off.rvmat"};

        class AnimationSources
        {
            class door
            {
                source = "user";
                initPhase = 0;
                animPeriod = 0.5;
            };
        };

        class DamageSystem
        {
            class GlobalHealth
            {
                class Health
                {
                    hitpoints = 1000;
                };
            };
        };
    };

    // =========================================================
    // v5.0: SPRINKLER (CONSUMER, automated crop watering)
    // =========================================================

    // ---- Sprinkler Kit (holdable, deployable, same-model) ----
    class LFPG_Sprinkler_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_SPRINKLER_KIT";
        descriptionShort = "$STR_LFPG_SPRINKLER_KIT_DESC";
        model = "\LFPowerGrid\data\sprinkler\sprinkler.p3d";
        weight = 1500;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {};
    };

    // ---- Sprinkler (placed device, CONSUMER 1 IN) ----
    class LFPG_Sprinkler : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_SPRINKLER";
        descriptionShort = "$STR_LFPG_SPRINKLER_DESC";
        model = "\LFPowerGrid\data\sprinkler\sprinkler.p3d";
        weight = 2000;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        hiddenSelections[] = {};
    };

    // =========================================================
    // v2.1: BATTERY ADAPTER (cradle for vanilla Car/Truck batteries)
    // Same-model kit → spawns adapter. Accepts CarBattery or TruckBattery
    // as attachment. Proxies CompEM via factor ×2.
    // =========================================================

    // ---- BatteryAdapter Kit (holdable, deployable, same-model) ----
    class LFPG_BatteryAdapter_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_BatteryAdapterKit";
        descriptionShort = "$STR_LFPG_BatteryAdapterKit_Desc";
        model = "\LFPowerGrid\data\battery_adapter\battery_adapter.p3d";
        weight = 5000;
        itemSize[] = {4, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {};
    };

    // ---- BatteryAdapter (placed device, PASSTHROUGH 1 IN + 1 OUT) ----
    class LFPG_BatteryAdapter : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_BatteryAdapter";
        descriptionShort = "$STR_LFPG_BatteryAdapter_Desc";
        model = "\LFPowerGrid\data\battery_adapter\battery_adapter.p3d";
        weight = 5000;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        hiddenSelections[] = {"body"};

        // Accepts vanilla CarBattery or TruckBattery as attachment.
        // Slots are defined in vanilla CfgSlots — no custom slot needed.
        attachments[] = {"CarBattery", "TruckBattery"};

        class GUIInventoryAttachmentsProps
        {
            class Battery
            {
                name = "Battery";
                description = "";
                icon = "missing";
                attachmentSlots[] = {"CarBattery", "TruckBattery"};
            };
        };

        class DamageSystem
        {
            class GlobalHealth
            {
                class Health
                {
                    hitpoints = 100;
                    healthLevels[] = {{1.0,{}},{0.7,{}},{0.5,{}},{0.3,{}}};
                };
            };
            class DamageZones
            {
                class GlobalHealth
                {
                    class Health
                    {
                        hitpoints = 100;
                    };
                    fatalInjuryCoef = -1;
                    componentNames[] = {};
                    transferToZonesNames[] = {};
                    transferToZonesCoefs[] = {};
                };
            };
        };
    };

    // =========================================================
    // v1.0.0: ELECTRIC STOVE (CONSUMER, 4 independent burners)
    //   input_0 = power input (0-40 u/s depending on active burners)
    //   4 DirectCooking slots for Pot/FryingPan/Cauldron
    //   PortableGasStove cooking pattern (no FireplaceBase)
    // =========================================================

    // ---- Electric Stove Kit (holdable, deployable, same-model, heavy) ----
    class LFPG_ElectricStove_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_ElectricStoveKit";
        descriptionShort = "$STR_LFPG_ElectricStoveKit_Desc";
        model = "\LFPowerGrid\data\electric_stove\electric_stove.p3d";
        weight = 25000;
        itemSize[] = {6, 4};
        itemBehaviour = 2;
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {};
    };

    // ---- Electric Stove (placed device, CONSUMER 0-40 u/s) ----
    class LFPG_ElectricStove : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_ElectricStove";
        descriptionShort = "$STR_LFPG_ElectricStove_Desc";
        model = "\LFPowerGrid\data\electric_stove\electric_stove.p3d";
        weight = 30000;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;

        // 4 DirectCooking slots for cookware (Pot, FryingPan, Cauldron)
        attachments[] = {"DirectCookingA", "DirectCookingB", "DirectCookingC", "DirectCookingD"};

        class GUIInventoryAttachmentsProps
        {
            class Burners
            {
                name = "Burners";
                description = "";
                icon = "missing";
                attachmentSlots[] = {"DirectCookingA", "DirectCookingB", "DirectCookingC", "DirectCookingD"};
            };
        };

        // hiddenSelections: [0]=stove_1, [1]=stove_2, [2]=stove_3, [3]=stove_4
        // Used for rvmat swap (burner on/off glow)
        hiddenSelections[] = {"stove_1", "stove_2", "stove_3", "stove_4"};
        hiddenSelectionsTextures[] = {"", "", "", ""};
        hiddenSelectionsMaterials[] =
        {
            "\LFPowerGrid\data\electric_stove\electric_stove.rvmat",
            "\LFPowerGrid\data\electric_stove\electric_stove.rvmat",
            "\LFPowerGrid\data\electric_stove\electric_stove.rvmat",
            "\LFPowerGrid\data\electric_stove\electric_stove.rvmat"
        };

        class AnimationSources
        {
            class button_1
            {
                source = "user";
                initPhase = 0;
                animPeriod = 0.3;
            };
            class button_2 : button_1 {};
            class button_3 : button_1 {};
            class button_4 : button_1 {};
        };

        class DamageSystem
        {
            class GlobalHealth
            {
                class Health
                {
                    hitpoints = 200;
                    healthLevels[] = {{1.0,{}},{0.7,{}},{0.5,{}},{0.3,{}}};
                };
            };
            class DamageZones
            {
                class GlobalHealth
                {
                    class Health
                    {
                        hitpoints = 200;
                    };
                    fatalInjuryCoef = -1;
                    componentNames[] = {};
                    transferToZonesNames[] = {};
                    transferToZonesCoefs[] = {};
                };
            };
        };
    };

	// =========================================================
    // v5.0: BTC ATM (Bitcoin ATM with live price)
    // =========================================================

    // ---- BTC ATM Player Kit (deployable, different-model) ----
    class LFPG_BTCAtm_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_BTC_ATM_KIT";
        descriptionShort = "$STR_LFPG_BTC_ATM_KIT_DESC";
        model = "\LFPowerGrid\data\kits\lf_kit_box.p3d";
        weight = 8000;
        itemSize[] = {4, 4};
        rotationFlags = 2;
        itemBehaviour = 2;
        canBeDigged = 0;
        carveNavmesh = 1;
        physLayer = "item_small";
        SingleUseActions[] = {527};
        ContinuousActions[] = {231};
    };

    // ---- BTC ATM Player (placed device, CONSUMER 1 IN) ----
    class LFPG_BTCAtm : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_BTC_ATM";
        descriptionShort = "$STR_LFPG_BTC_ATM_DESC";
        model = "\LFPowerGrid\data\btc_atm\bitcoin_atm.p3d";
        weight = 15000;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        storageCategory = 1;
        hiddenSelections[] = {"screen", "light_led_0"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\btc_atm\data\bitcoin_atm_screen_off.rvmat", "\LFPowerGrid\data\btc_atm\data\bitcoin_atm_red.rvmat"};
    };

    // ---- BTC ATM Admin Kit (deployable, different-model) ----
    class LFPG_BTCAtmAdmin_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_BTC_ATM_ADMIN_KIT";
        descriptionShort = "$STR_LFPG_BTC_ATM_ADMIN_KIT_DESC";
        model = "\LFPowerGrid\data\kits\lf_kit_box.p3d";
        weight = 8000;
        itemSize[] = {4, 4};
        rotationFlags = 2;
        itemBehaviour = 2;
        canBeDigged = 0;
        carveNavmesh = 1;
        physLayer = "item_small";
        SingleUseActions[] = {527};
        ContinuousActions[] = {231};
    };

    // ---- BTC ATM Admin (placed device, NO power) ----
    class LFPG_BTCAtmAdmin : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_BTC_ATM_ADMIN";
        descriptionShort = "$STR_LFPG_BTC_ATM_ADMIN_DESC";
        model = "\LFPowerGrid\data\btc_atm\bitcoin_atm.p3d";
        weight = 15000;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
        storageCategory = 1;
        hiddenSelections[] = {"screen", "light_led_0"};
        hiddenSelectionsMaterials[] = {"\LFPowerGrid\data\btc_atm\data\bitcoin_atm_green.rvmat", "\LFPowerGrid\data\btc_atm\data\bitcoin_atm_green.rvmat"};
    };

    // =========================================================
    // v4.2: REMOTE CONTROLLER (handheld, autonomous, RF pair+toggle)
    // NOT a grid device. Pairs with RF-capable switches.
    // Activate toggles all paired switches within range.
    // =========================================================
    class LFPG_RemoteController : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_REMOTE_CONTROLLER";
        descriptionShort = "$STR_LFPG_REMOTE_CONTROLLER_DESC";
        model = "\LFPowerGrid\data\remote_controller\remote_control.p3d";
        weight = 300;
        itemSize[] = {1, 2};
        rotationFlags = 17;
        animClass = "Pistol";
        itemBehaviour = 0;
        canBeDigged = 0;
        carveNavmesh = 0;
        physLayer = "item_small";

        hiddenSelections[] = {"button_1_led", "button_2_led"};
        hiddenSelectionsTextures[] = {"", ""};
        hiddenSelectionsMaterials[] =
        {
            "\LFPowerGrid\data\remote_controller\data\led_off.rvmat",
            "\LFPowerGrid\data\remote_controller\data\led_off.rvmat"
        };

        class AnimationSources
        {
            class activate_button_1
            {
                source = "user";
                initPhase = 0;
                animPeriod = 0.3;
            };
            class activate_button_2
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
    // v4.5: SPEAKER (PAS megaphone receiver)
    // Same-model kit → floor + wall. CONSUMER 5 u/s.
    // Spawns GhostPASReceiver when powered + on.
    // =========================================================

    // ---- Speaker Kit (holdable, deployable, same-model) ----
    class LFPG_Speaker_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_SPEAKER_KIT";
        descriptionShort = "$STR_LFPG_SPEAKER_KIT_DESC";
        model = "\LFPowerGrid\data\speaker\speaker.p3d";
        weight = 2000;
        itemSize[] = {2, 2};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
        hiddenSelections[] = {"knob", "light_led"};
        hiddenSelectionsTextures[] = {"", ""};
        hiddenSelectionsMaterials[] = {"", ""};
    };

    // ---- Speaker (placed device, CONSUMER 1 IN) ----
    class LFPG_Speaker : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_SPEAKER";
        descriptionShort = "$STR_LFPG_SPEAKER_DESC";
        model = "\LFPowerGrid\data\speaker\speaker.p3d";
        weight = 3000;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;

        hiddenSelections[] = {"knob", "light_led"};
        hiddenSelectionsTextures[] = {"", ""};
        hiddenSelectionsMaterials[] =
        {
            "",
            "\LFPowerGrid\data\speaker\data\speaker_led_off.rvmat"
        };

        class AnimationSources
        {
            class knob
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
                        {0.3, {}}
                    };
                };
            };
            class DamageZones
            {
                class GlobalHealth
                {
                    class Health
                    {
                        hitpoints = 100;
                    };
                    fatalInjuryCoef = -1;
                    componentNames[] = {};
                    transferToZonesNames[] = {};
                    transferToZonesCoefs[] = {};
                };
            };
        };
    };

    // ---- Ghost PAS Receiver (invisible, scope 1) ----
    // Engine routes PAS broadcast audio to entities of this class.
    // Spawned/destroyed by LFPG_Speaker at runtime.
    class LFPG_GhostPASReceiver : PASReceiver
    {
        scope = 1;
        displayName = "";
        descriptionShort = "";
        model = "\dz\gear\tools\stone.p3d";
        weight = 0;
        itemSize[] = {0, 0};
        itemBehaviour = 0;
        carveNavmesh = 0;
    };
};

// =========================================================
// v2.3: Proxy definitions for BatteryAdapter + Electric Stove DirectCookingD
// Class naming: Proxy + model filename (vanilla convention).
// =========================================================
class CfgNonAIVehicles
{
    class ProxyAttachment;
    class Proxyproxy_battery : ProxyAttachment
    {
        scope = 2;
        inventorySlot[] = {"CarBattery", "TruckBattery"};
        model = "\LFPowerGrid\data\battery_adapter\proxy_battery.p3d";
    };

    class Proxy_battery_adapter : ProxyAttachment
    {
        model = "\LFPowerGrid\data\battery_adapter\proxy_battery.p3d";
        simulation = "interiortarget";
        autocenter = 0;
    };

    // v1.0.1: DirectCookingD proxy (vanilla only defines A/B/C)
    class Proxyproxy_direct_cooking_d : ProxyAttachment
    {
        scope = 2;
        inventorySlot[] = {"DirectCookingD"};
        model = "\LFPowerGrid\data\electric_stove\proxy_direct_cooking_d.p3d";
    };
};
