// =========================================================
// LF_PowerGrid - config.cpp
// v0.7.47: Added Solar Panel Kit/T1/T2 + CfgSlots + modded vanilla items
// =========================================================

// ---- v0.7.47: Attachment slots for Solar Panel upgrade materials ----
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
    class LF_PowerGrid
    {
        units[] = { "LF_CableReel", "LF_TestGenerator", "LF_TestLamp", "LF_TestLampHeavy", "LF_Splitter_Kit", "LF_Splitter", "LF_SolarPanel_Kit", "LF_SolarPanel", "LF_SolarPanel_T2" };
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] = { "DZ_Data", "DZ_Scripts", "DZ_Gear_Tools", "DZ_Gear_Camping" };
    };
};

class CfgMods
{
    class LF_PowerGrid
    {
        dir = "LF_PowerGrid";
        picture = "";
        action = "";
        hideName = 0;
        hidePicture = 0;
        name = "LF_PowerGrid";
        prefix = "LF_PowerGrid";
        credits = "";
        author = "LF";
        authorID = "0";
        version = "0.7.47";
        type = "mod";

        dependencies[] = { "Game", "World", "Mission" };

        class defs
        {
            class gameScriptModule
            {
                value = "";
                files[] = { "LF_PowerGrid/scripts/3_Game", "LF_PowerGrid/Scripts/3_Game", "scripts/3_Game", "Scripts/3_Game" };
            };
            class worldScriptModule
            {
                value = "";
                files[] = { "LF_PowerGrid/scripts/4_World", "LF_PowerGrid/Scripts/4_World", "scripts/4_World", "Scripts/4_World" };
            };
            class missionScriptModule
            {
                value = "";
                files[] = { "LF_PowerGrid/scripts/5_Mission", "LF_PowerGrid/Scripts/5_Mission", "scripts/5_Mission", "Scripts/5_Mission" };
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

    // ---- Splitter Kit ----
    class Inventory_Base;
    class LF_Splitter_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "Splitter Kit";
        descriptionShort = "A power splitter. Place to deploy.";
        model = "\LF_PowerGrid\data\splitter\lf_splitter.p3d";
        weight = 3000;
        itemSize[] = {3, 3};
        rotationFlags = 17;
        isDeployable = 1;
        carveNavmesh = 1;
        physLayer = "item_large";
        slopeTolerance = 0.0;
        yawPitchRollLimit[] = {90, 90, 90};
    };

    // ---- Splitter ----
    class LF_Splitter : Inventory_Base
    {
        scope = 2;
        displayName = "Power Splitter";
        descriptionShort = "Splits incoming power into 3 outputs.";
        model = "\LF_PowerGrid\data\splitter\lf_splitter.p3d";
        weight = 5000;
        itemSize[] = {3, 3};
        rotationFlags = 17;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
    };

    // =========================================================
    // v0.7.47: SOLAR PANEL
    // =========================================================

    // ---- Solar Panel Kit (shared box model, holdable) ----
    // Uses isDeployable=1 + hologram projection swap (box → panel).
    // On placement, spawns LF_SolarPanel and deletes the kit.
    class LF_SolarPanel_Kit : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_SOLAR_KIT";
        descriptionShort = "$STR_LFPG_SOLAR_KIT_DESC";
        model = "\LF_PowerGrid\data\kits\lf_kit_box.p3d";
        weight = 2000;
        itemSize[] = {5, 3};
        rotationFlags = 17;
        isDeployable = 1;
        itemBehaviour = 2;
        canBeDigged = 0;
        carveNavmesh = 1;
        physLayer = "item_small";
    };

    // ---- Solar Panel T1 (placed device, SOURCE 20 u/s) ----
    // Has attachment slots for upgrade materials (MetalPlate + Nail).
    // Memory points required in p3d: port_output_1
    class LF_SolarPanel : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_SOLAR_T1";
        descriptionShort = "$STR_LFPG_SOLAR_T1_DESC";
        model = "\LF_PowerGrid\data\solarpanel\lf_solarpanel.p3d";
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
    // No attachment slots — already upgraded.
    // Memory points required in p3d: port_output_1
    class LF_SolarPanel_T2 : Inventory_Base
    {
        scope = 2;
        displayName = "$STR_LFPG_SOLAR_T2";
        descriptionShort = "$STR_LFPG_SOLAR_T2_DESC";
        model = "\LF_PowerGrid\data\solarpanel\lf_solarpanel_t2.p3d";
        weight = 10000;
        itemSize[] = {10, 10};
        rotationFlags = 17;
        carveNavmesh = 1;
        physLayer = "item_large";
        isDeployable = 0;
    };

    // =========================================================
    // v0.7.47: MODDED VANILLA ITEMS (additive, not destructive)
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
