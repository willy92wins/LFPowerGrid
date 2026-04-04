// =========================================================
// LF_PowerGrid - server settings (profiles JSON)
//
// v0.7.15 (Sprint 3 P2b): Atomic save + backup restore.
// v0.7.16 (Hotfix H4): AtomicVerifyReadback setting.
// v0.7.35 (Gemini 3b): Range validation + clamping on load.
// v0.7.36 (Hotfix): Pre-build Warn strings for Enforce compat.
// v4.6: Vanilla compat — external mod consumer support.
// v4.7: Furnace fuel whitelist + heat emission (UTS).
// =========================================================

// ---- Furnace fuel whitelist entry (v4.7) ----
// JSON-serializable. Matched by exact classname (GetType()).
// burnTimeSec: how many seconds ONE unit of this item fuels the furnace.
// Internally converted to fuel units: fuelUnits = burnTimeSec / 30.
// Example: { "classname": "Firewood", "burnTimeSec": 300 } = 5 minutes
class LFPG_FurnaceFuelEntry
{
    string classname;
    int burnTimeSec;

    void LFPG_FurnaceFuelEntry()
    {
        classname = "";
        burnTimeSec = 0;
    }
};

// ---- Vanilla consumption override entry (v4.6) ----
// JSON-serializable. Matches by IsKindOf (inheritance-aware).
// Example: { "classname": "Spotlight", "consumption": 10.0 }
class LFPG_VanillaConsumptionEntry
{
    string classname;
    float consumption;

    void LFPG_VanillaConsumptionEntry()
    {
        classname = "";
        consumption = 0.0;
    }
};

// ---- Vanilla blacklist entry (v4.6) ----
// JSON-serializable. Supports prefix ("BBP_*") or exact ("Fireplace").
class LFPG_VanillaBlacklistEntry
{
    string pattern;

    void LFPG_VanillaBlacklistEntry()
    {
        pattern = "";
    }
};

class LFPG_ServerSettings
{
    int ver = 1;

    bool KickOnInvalidWire = false;

    // Hardening knobs
    int MaxWiresPerPlayer = LFPG_MAX_WIRES_PER_PLAYER;
    int MaxWiresPerDevice = LFPG_MAX_WIRES_PER_DEVICE;

    // Permissions / anti-grief
    bool AllowCutOthersWires = false;

    // Optional: if you want extra player-spam control later
    float RpcCooldownSeconds = LFPG_RPC_COOLDOWN_S;

    // v0.7.7: Device proximity bubble (meters).
    // If > 0, wires are hidden when BOTH endpoints are farther
    // than this distance from the player. Set to 0 to disable
    // (falls back to LFPG_CULL_DISTANCE_M = 50m only).
    // Recommended: 10-25m for performance, 0 for full range.
    float DeviceBubbleM = LFPG_DEVICE_BUBBLE_M;

    // v0.7.16 H4: Atomic save read-back verification.
    // When true (default), saved .tmp files are re-read to verify integrity.
    // Doubles I/O on save but catches rare write corruption.
    // Set to false if you notice save lag with very large wire files (>5MB).
    bool AtomicVerifyReadback = true;

    // v4.5: When true, cables are completely hidden unless the player
    // holds a CableReel or Pliers in hand. Sent to clients on JIP.
    // Default false: cables always visible (grey when no tools).
    bool HideCablesWithoutReel = false;

    // ---- v4.6: Vanilla Compat (external mod consumers) ----
    // When true, any non-LFPG entity with CompEM.GetEnergyUsage() > 0
    // is accepted as a consumer (subject to blacklist).
    // When false, only Spotlight is accepted (legacy behavior).
    bool VanillaCompatEnabled = true;

    // Default consumption (u/s) for vanilla-compat devices that have
    // no entry in VanillaCustomConsumption and whose CompEM usage is 0.
    float VanillaDefaultConsumption = 20.0;

    // Per-class consumption overrides. Matched via IsKindOf (inheritance).
    // Example: Spotlight=10, BatteryCharger=15
    ref array<ref LFPG_VanillaConsumptionEntry> VanillaCustomConsumption;

    // Blacklist patterns. Prefix: "BBP_*" blocks any type starting
    // with "BBP_". Exact: "Fireplace" blocks via IsKindOf (inheritance).
    ref array<ref LFPG_VanillaBlacklistEntry> VanillaBlacklist;

    // ---- v4.7: Furnace Fuel Whitelist ----
    // When true, ONLY items listed in FurnaceFuelWhitelist generate fuel.
    // Items NOT in the list are still burned (destroyed) but add 0 fuel.
    // When false (default), any item generates fuel based on its inventory size (w*h*qty).
    bool FurnaceFuelWhitelistOnly = false;

    // List of accepted fuel items with burn time in seconds per unit.
    // Only used when FurnaceFuelWhitelistOnly = true.
    // Each entry: classname (exact match) + burnTimeSec (seconds of fuel per item).
    // Stack items (canBeSplit) multiply by quantity.
    ref array<ref LFPG_FurnaceFuelEntry> FurnaceFuelWhitelist;

    // ---- v4.7: Furnace Heat Emission ----
    // When true, the furnace warms nearby players/items while burning,
    // similar to a vanilla campfire. Uses DayZ UniversalTemperatureSource.
    bool FurnaceHeatEnabled = false;

    // Radius (meters) where players receive full warmth. Campfire = 2m.
    float FurnaceHeatFullWarmthRadiusM = 3.0;

    // Radius (meters) where warmth fades to zero. Must be > FullWarmthRadiusM. Campfire = 4m.
    float FurnaceHeatFadeOutRadiusM = 5.0;

    // Heat intensity as a multiplier of a vanilla campfire.
    // 1.0 = same as campfire, 1.5 = 50% warmer, 2.0 = double. Campfire = 1.0.
    float FurnaceHeatStrengthMultiplier = 1.25;

    void LFPG_ServerSettings()
    {
        VanillaCustomConsumption = new array<ref LFPG_VanillaConsumptionEntry>;
        VanillaBlacklist = new array<ref LFPG_VanillaBlacklistEntry>;

        // Default custom consumption: Spotlight keeps legacy 10 u/s
        ref LFPG_VanillaConsumptionEntry spotEntry = new LFPG_VanillaConsumptionEntry();
        string spotClass = "Spotlight";
        spotEntry.classname = spotClass;
        spotEntry.consumption = 10.0;
        VanillaCustomConsumption.Insert(spotEntry);

        // Default blacklist: handheld/unsuitable vanilla items
        ref LFPG_VanillaBlacklistEntry e0 = new LFPG_VanillaBlacklistEntry();
        e0.pattern = "Flashlight_*";
        VanillaBlacklist.Insert(e0);

        ref LFPG_VanillaBlacklistEntry e1 = new LFPG_VanillaBlacklistEntry();
        e1.pattern = "HeadTorch_*";
        VanillaBlacklist.Insert(e1);

        ref LFPG_VanillaBlacklistEntry e2 = new LFPG_VanillaBlacklistEntry();
        e2.pattern = "PersonalRadio";
        VanillaBlacklist.Insert(e2);

        ref LFPG_VanillaBlacklistEntry e3 = new LFPG_VanillaBlacklistEntry();
        e3.pattern = "Megaphone";
        VanillaBlacklist.Insert(e3);

        ref LFPG_VanillaBlacklistEntry e4 = new LFPG_VanillaBlacklistEntry();
        e4.pattern = "Defibrillator";
        VanillaBlacklist.Insert(e4);

        ref LFPG_VanillaBlacklistEntry e5 = new LFPG_VanillaBlacklistEntry();
        e5.pattern = "CattleProd";
        VanillaBlacklist.Insert(e5);

        ref LFPG_VanillaBlacklistEntry e6 = new LFPG_VanillaBlacklistEntry();
        e6.pattern = "StunBaton";
        VanillaBlacklist.Insert(e6);

        ref LFPG_VanillaBlacklistEntry e7 = new LFPG_VanillaBlacklistEntry();
        e7.pattern = "RadioTransmitterCivil";
        VanillaBlacklist.Insert(e7);

        // ---- Furnace fuel whitelist defaults (v4.7) ----
        FurnaceFuelWhitelist = new array<ref LFPG_FurnaceFuelEntry>;

        ref LFPG_FurnaceFuelEntry fuelFirewood = new LFPG_FurnaceFuelEntry();
        string fwClass = "Firewood";
        fuelFirewood.classname = fwClass;
        fuelFirewood.burnTimeSec = 300;
        FurnaceFuelWhitelist.Insert(fuelFirewood);

        ref LFPG_FurnaceFuelEntry fuelLog = new LFPG_FurnaceFuelEntry();
        string logClass = "WoodenLog";
        fuelLog.classname = logClass;
        fuelLog.burnTimeSec = 600;
        FurnaceFuelWhitelist.Insert(fuelLog);
    }
};

// ---- Validation bounds (v0.7.35, Gemini 3b) ----
// Hard limits for settings loaded from JSON. Values outside these
// ranges are clamped and logged as warnings. Prevents misconfigured
// servers from causing OOM, spam, or invisible-wire issues.
static const int   LFPG_SETTINGS_MIN_WIRES_PLAYER = 1;
static const int   LFPG_SETTINGS_MAX_WIRES_PLAYER = 1024;
static const int   LFPG_SETTINGS_MIN_WIRES_DEVICE = 1;
static const int   LFPG_SETTINGS_MAX_WIRES_DEVICE = 128;
static const float LFPG_SETTINGS_MIN_RPC_COOLDOWN = 0.1;
static const float LFPG_SETTINGS_MAX_RPC_COOLDOWN = 30.0;
static const float LFPG_SETTINGS_MIN_BUBBLE_M     = 0.0;
static const float LFPG_SETTINGS_MAX_BUBBLE_M     = 500.0;

// v4.6: Vanilla compat bounds
static const float LFPG_SETTINGS_MIN_VANILLA_CONSUMPTION = 0.1;
static const float LFPG_SETTINGS_MAX_VANILLA_CONSUMPTION = 500.0;

// v4.7: Furnace fuel whitelist bounds
static const int   LFPG_SETTINGS_MIN_FUEL_BURN_SEC  = 30;
static const int   LFPG_SETTINGS_MAX_FUEL_BURN_SEC  = 86400;

// v4.7: Furnace heat emission bounds
static const float LFPG_SETTINGS_MIN_HEAT_RADIUS    = 0.5;
static const float LFPG_SETTINGS_MAX_HEAT_FULL_R    = 10.0;
static const float LFPG_SETTINGS_MAX_HEAT_FADE_R    = 20.0;
static const float LFPG_SETTINGS_MIN_HEAT_MULT      = 0.1;
static const float LFPG_SETTINGS_MAX_HEAT_MULT      = 5.0;

// Vanilla campfire heat cap (PARAM_MAX_TRANSFERED_TEMPERATURE).
// Used internally: effective temp = this * FurnaceHeatStrengthMultiplier.
static const float LFPG_CAMPFIRE_HEAT_CAP = 20.0;

class LFPG_Settings
{
    protected static ref LFPG_ServerSettings s_Settings;
    protected static bool s_LoggedBanner = false;
    protected static const string SETTINGS_DIR = "$profile:LF_PowerGrid";
    protected static const string SETTINGS_FILE = "$profile:LF_PowerGrid\\LF_PowerGrid.json";

    static LFPG_ServerSettings Get()
    {
        if (!s_Settings)
            Load();
        return s_Settings;
    }

    // ---- v0.7.35 (Gemini 3b): Range validation helpers ----
    // Returns clamped value. Enforce has no Math.Clamp for int/float,
    // so we do manual min/max comparisons.

    protected static int ClampInt(int val, int lo, int hi)
    {
        if (val < lo)
            return lo;
        if (val > hi)
            return hi;
        return val;
    }

    protected static float ClampFloat(float val, float lo, float hi)
    {
        if (val < lo)
            return lo;
        if (val > hi)
            return hi;
        return val;
    }

    // v0.7.35 (Gemini 3b): Validate and clamp all numeric settings.
    // Called after successful JSON deserialization. Logs a warning
    // for every field that required clamping so admins know their
    // settings.json has out-of-range values.
    // v0.7.36: All Warn strings pre-built to avoid Enforce
    //          "Invalid statement ')'" on long inline concatenation.
    protected static void ValidateAndClamp()
    {
        if (!s_Settings)
            return;

        int clamped = 0;
        int intVal;
        float floatVal;
        string msg;

        // --- ver: must be >= 1 ---
        if (s_Settings.ver < 1)
        {
            msg = "Settings: ver=" + s_Settings.ver.ToString() + " invalid, reset to 1";
            LFPG_Util.Warn(msg);
            s_Settings.ver = 1;
            clamped = clamped + 1;
        }

        // --- MaxWiresPerPlayer ---
        intVal = ClampInt(s_Settings.MaxWiresPerPlayer,
                          LFPG_SETTINGS_MIN_WIRES_PLAYER,
                          LFPG_SETTINGS_MAX_WIRES_PLAYER);
        if (intVal != s_Settings.MaxWiresPerPlayer)
        {
            msg = "Settings: MaxWiresPerPlayer=" + s_Settings.MaxWiresPerPlayer.ToString();
            msg = msg + " clamped to " + intVal.ToString();
            msg = msg + " [" + LFPG_SETTINGS_MIN_WIRES_PLAYER.ToString();
            msg = msg + ".." + LFPG_SETTINGS_MAX_WIRES_PLAYER.ToString() + "]";
            LFPG_Util.Warn(msg);
            s_Settings.MaxWiresPerPlayer = intVal;
            clamped = clamped + 1;
        }

        // --- MaxWiresPerDevice ---
        intVal = ClampInt(s_Settings.MaxWiresPerDevice,
                          LFPG_SETTINGS_MIN_WIRES_DEVICE,
                          LFPG_SETTINGS_MAX_WIRES_DEVICE);
        if (intVal != s_Settings.MaxWiresPerDevice)
        {
            msg = "Settings: MaxWiresPerDevice=" + s_Settings.MaxWiresPerDevice.ToString();
            msg = msg + " clamped to " + intVal.ToString();
            msg = msg + " [" + LFPG_SETTINGS_MIN_WIRES_DEVICE.ToString();
            msg = msg + ".." + LFPG_SETTINGS_MAX_WIRES_DEVICE.ToString() + "]";
            LFPG_Util.Warn(msg);
            s_Settings.MaxWiresPerDevice = intVal;
            clamped = clamped + 1;
        }

        // --- RpcCooldownSeconds ---
        floatVal = ClampFloat(s_Settings.RpcCooldownSeconds,
                              LFPG_SETTINGS_MIN_RPC_COOLDOWN,
                              LFPG_SETTINGS_MAX_RPC_COOLDOWN);
        if (floatVal != s_Settings.RpcCooldownSeconds)
        {
            msg = "Settings: RpcCooldownSeconds=" + s_Settings.RpcCooldownSeconds.ToString();
            msg = msg + " clamped to " + floatVal.ToString();
            msg = msg + " [" + LFPG_SETTINGS_MIN_RPC_COOLDOWN.ToString();
            msg = msg + ".." + LFPG_SETTINGS_MAX_RPC_COOLDOWN.ToString() + "]";
            LFPG_Util.Warn(msg);
            s_Settings.RpcCooldownSeconds = floatVal;
            clamped = clamped + 1;
        }

        // --- DeviceBubbleM ---
        floatVal = ClampFloat(s_Settings.DeviceBubbleM,
                              LFPG_SETTINGS_MIN_BUBBLE_M,
                              LFPG_SETTINGS_MAX_BUBBLE_M);
        if (floatVal != s_Settings.DeviceBubbleM)
        {
            msg = "Settings: DeviceBubbleM=" + s_Settings.DeviceBubbleM.ToString();
            msg = msg + " clamped to " + floatVal.ToString();
            msg = msg + " [" + LFPG_SETTINGS_MIN_BUBBLE_M.ToString();
            msg = msg + ".." + LFPG_SETTINGS_MAX_BUBBLE_M.ToString() + "]";
            LFPG_Util.Warn(msg);
            s_Settings.DeviceBubbleM = floatVal;
            clamped = clamped + 1;
        }

        if (clamped > 0)
        {
            msg = "Settings: " + clamped.ToString() + " value(s) clamped. Review LF_PowerGrid.json";
            LFPG_Util.Warn(msg);
        }

        // ---- v4.6: Vanilla compat validation ----
        floatVal = ClampFloat(s_Settings.VanillaDefaultConsumption,
                              LFPG_SETTINGS_MIN_VANILLA_CONSUMPTION,
                              LFPG_SETTINGS_MAX_VANILLA_CONSUMPTION);
        if (floatVal != s_Settings.VanillaDefaultConsumption)
        {
            msg = "Settings: VanillaDefaultConsumption=";
            msg = msg + s_Settings.VanillaDefaultConsumption.ToString();
            msg = msg + " clamped to ";
            msg = msg + floatVal.ToString();
            LFPG_Util.Warn(msg);
            s_Settings.VanillaDefaultConsumption = floatVal;
        }

        // Ensure arrays exist (could be null from malformed JSON)
        if (!s_Settings.VanillaCustomConsumption)
        {
            s_Settings.VanillaCustomConsumption = new array<ref LFPG_VanillaConsumptionEntry>;
        }
        if (!s_Settings.VanillaBlacklist)
        {
            s_Settings.VanillaBlacklist = new array<ref LFPG_VanillaBlacklistEntry>;
        }

        // Validate custom consumption entries
        int vci;
        for (vci = 0; vci < s_Settings.VanillaCustomConsumption.Count(); vci = vci + 1)
        {
            LFPG_VanillaConsumptionEntry vce = s_Settings.VanillaCustomConsumption[vci];
            if (!vce)
                continue;
            if (vce.classname == "")
            {
                msg = "Settings: VanillaCustomConsumption[";
                msg = msg + vci.ToString();
                msg = msg + "] has empty classname";
                LFPG_Util.Warn(msg);
            }
            floatVal = ClampFloat(vce.consumption,
                                  LFPG_SETTINGS_MIN_VANILLA_CONSUMPTION,
                                  LFPG_SETTINGS_MAX_VANILLA_CONSUMPTION);
            if (floatVal != vce.consumption)
            {
                msg = "Settings: VanillaCustomConsumption[";
                msg = msg + vci.ToString();
                msg = msg + "] consumption=";
                msg = msg + vce.consumption.ToString();
                msg = msg + " clamped to ";
                msg = msg + floatVal.ToString();
                LFPG_Util.Warn(msg);
                vce.consumption = floatVal;
            }
        }

        // ---- v4.7: Furnace fuel whitelist validation ----
        if (!s_Settings.FurnaceFuelWhitelist)
        {
            s_Settings.FurnaceFuelWhitelist = new array<ref LFPG_FurnaceFuelEntry>;
        }

        int fwi;
        for (fwi = 0; fwi < s_Settings.FurnaceFuelWhitelist.Count(); fwi = fwi + 1)
        {
            LFPG_FurnaceFuelEntry fwe = s_Settings.FurnaceFuelWhitelist[fwi];
            if (!fwe)
                continue;
            if (fwe.classname == "")
            {
                msg = "Settings: FurnaceFuelWhitelist[";
                msg = msg + fwi.ToString();
                msg = msg + "] has empty classname";
                LFPG_Util.Warn(msg);
            }
            intVal = ClampInt(fwe.burnTimeSec,
                              LFPG_SETTINGS_MIN_FUEL_BURN_SEC,
                              LFPG_SETTINGS_MAX_FUEL_BURN_SEC);
            if (intVal != fwe.burnTimeSec)
            {
                msg = "Settings: FurnaceFuelWhitelist[";
                msg = msg + fwi.ToString();
                msg = msg + "] burnTimeSec=";
                msg = msg + fwe.burnTimeSec.ToString();
                msg = msg + " clamped to ";
                msg = msg + intVal.ToString();
                LFPG_Util.Warn(msg);
                fwe.burnTimeSec = intVal;
            }
        }

        // Warn: whitelist enabled but empty
        if (s_Settings.FurnaceFuelWhitelistOnly)
        {
            if (s_Settings.FurnaceFuelWhitelist.Count() <= 0)
            {
                msg = "Settings: FurnaceFuelWhitelistOnly=true but whitelist is empty.";
                msg = msg + " Furnace will burn items without fuel benefit.";
                LFPG_Util.Warn(msg);
            }
        }

        // ---- v4.7: Furnace heat emission validation ----
        floatVal = ClampFloat(s_Settings.FurnaceHeatFullWarmthRadiusM,
                              LFPG_SETTINGS_MIN_HEAT_RADIUS,
                              LFPG_SETTINGS_MAX_HEAT_FULL_R);
        if (floatVal != s_Settings.FurnaceHeatFullWarmthRadiusM)
        {
            msg = "Settings: FurnaceHeatFullWarmthRadiusM=";
            msg = msg + s_Settings.FurnaceHeatFullWarmthRadiusM.ToString();
            msg = msg + " clamped to ";
            msg = msg + floatVal.ToString();
            LFPG_Util.Warn(msg);
            s_Settings.FurnaceHeatFullWarmthRadiusM = floatVal;
        }

        floatVal = ClampFloat(s_Settings.FurnaceHeatFadeOutRadiusM,
                              LFPG_SETTINGS_MIN_HEAT_RADIUS,
                              LFPG_SETTINGS_MAX_HEAT_FADE_R);
        if (floatVal != s_Settings.FurnaceHeatFadeOutRadiusM)
        {
            msg = "Settings: FurnaceHeatFadeOutRadiusM=";
            msg = msg + s_Settings.FurnaceHeatFadeOutRadiusM.ToString();
            msg = msg + " clamped to ";
            msg = msg + floatVal.ToString();
            LFPG_Util.Warn(msg);
            s_Settings.FurnaceHeatFadeOutRadiusM = floatVal;
        }

        // Ensure FullWarmth < FadeOut
        if (s_Settings.FurnaceHeatFullWarmthRadiusM >= s_Settings.FurnaceHeatFadeOutRadiusM)
        {
            msg = "Settings: FurnaceHeatFullWarmthRadiusM (";
            msg = msg + s_Settings.FurnaceHeatFullWarmthRadiusM.ToString();
            msg = msg + ") must be < FadeOutRadiusM (";
            msg = msg + s_Settings.FurnaceHeatFadeOutRadiusM.ToString();
            msg = msg + "). Adjusting FadeOut to Full+2.";
            LFPG_Util.Warn(msg);
            s_Settings.FurnaceHeatFadeOutRadiusM = s_Settings.FurnaceHeatFullWarmthRadiusM + 2.0;
        }

        floatVal = ClampFloat(s_Settings.FurnaceHeatStrengthMultiplier,
                              LFPG_SETTINGS_MIN_HEAT_MULT,
                              LFPG_SETTINGS_MAX_HEAT_MULT);
        if (floatVal != s_Settings.FurnaceHeatStrengthMultiplier)
        {
            msg = "Settings: FurnaceHeatStrengthMultiplier=";
            msg = msg + s_Settings.FurnaceHeatStrengthMultiplier.ToString();
            msg = msg + " clamped to ";
            msg = msg + floatVal.ToString();
            LFPG_Util.Warn(msg);
            s_Settings.FurnaceHeatStrengthMultiplier = floatVal;
        }
    }

    static void Load()
    {
        if (!s_Settings)
            s_Settings = new LFPG_ServerSettings();

        if (!s_LoggedBanner)
        {
            s_LoggedBanner = true;
            LFPG_Util.Info("Loaded (v=" + LFPG_VERSION_STR + ")");
        }

        string err;
        if (!FileExist(SETTINGS_DIR))
            MakeDirectory(SETTINGS_DIR);

        // v0.7.15 (Sprint 3 P2b): Backup restore if main file missing
        LFPG_FileUtil.EnsureFileOrRestore(SETTINGS_FILE);

        if (FileExist(SETTINGS_FILE))
        {
            if (!JsonFileLoader<LFPG_ServerSettings>.LoadFile(SETTINGS_FILE, s_Settings, err))
            {
                LFPG_Util.Warn("Settings load failed, using defaults. " + err);
            }
            else
            {
                // v0.7.35 (Gemini 3b): Validate ranges before logging
                ValidateAndClamp();

                string msg = "Settings loaded:";
                msg = msg + " MaxWiresPerPlayer=" + s_Settings.MaxWiresPerPlayer.ToString();
                msg = msg + " MaxWiresPerDevice=" + s_Settings.MaxWiresPerDevice.ToString();
                msg = msg + " AllowCutOthersWires=" + s_Settings.AllowCutOthersWires.ToString();
                msg = msg + " RpcCooldownSeconds=" + s_Settings.RpcCooldownSeconds.ToString();
                msg = msg + " DeviceBubbleM=" + s_Settings.DeviceBubbleM.ToString();
                msg = msg + " AtomicVerifyReadback=" + s_Settings.AtomicVerifyReadback.ToString();
                msg = msg + " HideCablesWithoutReel=" + s_Settings.HideCablesWithoutReel.ToString();
                msg = msg + " VanillaCompat=" + s_Settings.VanillaCompatEnabled.ToString();
                msg = msg + " VanillaDefConsumption=" + s_Settings.VanillaDefaultConsumption.ToString();
                int vcCount = 0;
                if (s_Settings.VanillaCustomConsumption)
                {
                    vcCount = s_Settings.VanillaCustomConsumption.Count();
                }
                msg = msg + " CustomEntries=" + vcCount.ToString();
                int blCount = 0;
                if (s_Settings.VanillaBlacklist)
                {
                    blCount = s_Settings.VanillaBlacklist.Count();
                }
                msg = msg + " Blacklist=" + blCount.ToString();
                // v4.7: Furnace settings
                msg = msg + " FuelWhitelistOnly=" + s_Settings.FurnaceFuelWhitelistOnly.ToString();
                int fwCount = 0;
                if (s_Settings.FurnaceFuelWhitelist)
                {
                    fwCount = s_Settings.FurnaceFuelWhitelist.Count();
                }
                msg = msg + " FuelEntries=" + fwCount.ToString();
                msg = msg + " FurnaceHeat=" + s_Settings.FurnaceHeatEnabled.ToString();
                if (s_Settings.FurnaceHeatEnabled)
                {
                    float heatAbs = LFPG_CAMPFIRE_HEAT_CAP * s_Settings.FurnaceHeatStrengthMultiplier;
                    msg = msg + " FullRadius=" + s_Settings.FurnaceHeatFullWarmthRadiusM.ToString() + "m";
                    msg = msg + " FadeOut=" + s_Settings.FurnaceHeatFadeOutRadiusM.ToString() + "m";
                    msg = msg + " Strength=x" + s_Settings.FurnaceHeatStrengthMultiplier.ToString();
                    msg = msg + " (=" + heatAbs.ToString() + ", campfire=20)";
                }
                LFPG_Util.Info(msg);
            }
        }
        else
        {
            LFPG_Util.Info("Settings file not found; creating defaults: " + SETTINGS_FILE);
            Save(); // create defaults
        }

        // v4.6: Parse blacklist into prefix/exact arrays for fast lookup
        BuildBlacklistIndex();
    }

    static void Save()
    {
        if (!s_Settings)
            s_Settings = new LFPG_ServerSettings();

        if (!FileExist(SETTINGS_DIR))
            MakeDirectory(SETTINGS_DIR);

        // v0.7.15 (Sprint 3 P2b): Atomic save with backup rotation
        if (LFPG_FileUtil.AtomicSaveSettings(SETTINGS_FILE, s_Settings))
        {
            LFPG_Util.Info("Settings saved (atomic): " + SETTINGS_FILE);
        }
        else
        {
            LFPG_Util.Error("Settings atomic save failed: " + SETTINGS_FILE);
        }
    }

    // =========================================================
    // v4.6: Vanilla Compat runtime cache + helpers
    //
    // Lazy caches: populated on first query per typename, cleared
    // on Load(). Avoids repeated IsKindOf/array iteration during
    // graph propagation. O(1) after first hit per typename.
    // =========================================================

    // Cache: typename -> true (blacklisted) / false (allowed)
    protected static ref map<string, bool> s_BlacklistCache;

    // Cache: typename -> resolved consumption (custom or default)
    protected static ref map<string, float> s_ConsumptionCache;

    // Parsed blacklist: prefix patterns (without trailing *)
    protected static ref array<string> s_BLPrefixes;

    // Parsed blacklist: exact patterns (for IsKindOf)
    protected static ref array<string> s_BLExact;

    // v4.7: Fuel whitelist cache: classname -> burnTimeSec
    protected static ref map<string, int> s_FuelWhitelistCache;

    // Called after Load to parse blacklist into prefix/exact arrays.
    protected static void BuildBlacklistIndex()
    {
        s_BlacklistCache = new map<string, bool>;
        s_ConsumptionCache = new map<string, float>;
        s_BLPrefixes = new array<string>;
        s_BLExact = new array<string>;
        s_FuelWhitelistCache = new map<string, int>;

        if (!s_Settings || !s_Settings.VanillaBlacklist)
            return;

        int i;
        for (i = 0; i < s_Settings.VanillaBlacklist.Count(); i = i + 1)
        {
            LFPG_VanillaBlacklistEntry entry = s_Settings.VanillaBlacklist[i];
            if (!entry)
                continue;

            string pattern = entry.pattern;
            if (pattern == "")
                continue;

            int lastIdx = pattern.Length() - 1;
            string lastChar = pattern.Substring(lastIdx, 1);
            if (lastChar == "*")
            {
                // Prefix pattern: strip trailing *
                string prefix = pattern.Substring(0, lastIdx);
                if (prefix != "")
                {
                    s_BLPrefixes.Insert(prefix);
                }
            }
            else
            {
                // Exact class (matched via IsKindOf for inheritance)
                s_BLExact.Insert(pattern);
            }
        }

        string blMsg = "VanillaCompat: blacklist parsed prefixes=";
        blMsg = blMsg + s_BLPrefixes.Count().ToString();
        blMsg = blMsg + " exact=";
        blMsg = blMsg + s_BLExact.Count().ToString();
        LFPG_Util.Info(blMsg);

        // v4.7: Build fuel whitelist cache
        if (s_Settings && s_Settings.FurnaceFuelWhitelist)
        {
            int fwi;
            for (fwi = 0; fwi < s_Settings.FurnaceFuelWhitelist.Count(); fwi = fwi + 1)
            {
                LFPG_FurnaceFuelEntry fwe = s_Settings.FurnaceFuelWhitelist[fwi];
                if (!fwe)
                    continue;
                if (fwe.classname == "")
                    continue;
                if (fwe.burnTimeSec <= 0)
                    continue;
                s_FuelWhitelistCache.Set(fwe.classname, fwe.burnTimeSec);
            }

            string fwMsg = "FurnaceFuel: whitelist cached entries=";
            fwMsg = fwMsg + s_FuelWhitelistCache.Count().ToString();
            LFPG_Util.Info(fwMsg);
        }
    }

    // =========================================================
    // v4.7: Furnace fuel whitelist helper
    //
    // Returns burnTimeSec for a given classname, or -1 if not found.
    // O(1) via cached map. Only meaningful when FurnaceFuelWhitelistOnly=true.
    // =========================================================
    static int GetWhitelistFuelSec(string typeName)
    {
        if (!s_FuelWhitelistCache)
            return -1;

        int burnSec = 0;
        bool found = s_FuelWhitelistCache.Find(typeName, burnSec);
        if (found)
            return burnSec;

        return -1;
    }

    // Check if entity is blacklisted. Uses prefix match on GetType()
    // and IsKindOf for exact entries (catches inheritance).
    // Result cached per typename for O(1) subsequent lookups.
    static bool IsVanillaBlacklistedEntity(EntityAI e)
    {
        if (!e)
            return true;

        string typeName = e.GetType();

        // Fast path: check typename cache first
        if (s_BlacklistCache)
        {
            bool cached = false;
            bool found = s_BlacklistCache.Find(typeName, cached);
            if (found)
                return cached;
        }

        // Ensure cache exists
        if (!s_BlacklistCache)
        {
            s_BlacklistCache = new map<string, bool>;
        }

        bool blocked = false;
        int typeLen = typeName.Length();

        // Check prefix patterns on typename
        if (s_BLPrefixes)
        {
            int pi;
            for (pi = 0; pi < s_BLPrefixes.Count(); pi = pi + 1)
            {
                string prefix = s_BLPrefixes[pi];
                int prefixLen = prefix.Length();
                if (typeLen >= prefixLen)
                {
                    string sub = typeName.Substring(0, prefixLen);
                    if (sub == prefix)
                    {
                        blocked = true;
                        break;
                    }
                }
            }
        }

        // Check exact patterns via IsKindOf (inheritance-aware)
        if (!blocked && s_BLExact)
        {
            int ei;
            for (ei = 0; ei < s_BLExact.Count(); ei = ei + 1)
            {
                string exact = s_BLExact[ei];
                if (e.IsKindOf(exact))
                {
                    blocked = true;
                    break;
                }
            }
        }

        s_BlacklistCache.Set(typeName, blocked);
        return blocked;
    }

    // Get consumption for a vanilla-compat device. Priority:
    // 1. Custom entry match (IsKindOf) → use configured value
    // 2. CompEM.GetEnergyUsage() > 0 → use engine value
    // 3. VanillaDefaultConsumption fallback
    // Result cached per typename.
    static float GetVanillaConsumption(EntityAI e)
    {
        if (!e)
            return 0.0;

        string typeName = e.GetType();

        // Cache hit?
        if (s_ConsumptionCache)
        {
            float cached = 0.0;
            bool found = s_ConsumptionCache.Find(typeName, cached);
            if (found)
                return cached;
        }

        // Ensure cache exists
        if (!s_ConsumptionCache)
        {
            s_ConsumptionCache = new map<string, float>;
        }

        LFPG_ServerSettings st = Get();
        float result = st.VanillaDefaultConsumption;
        bool hadCustomMatch = false;

        // Step 1: Check custom entries (IsKindOf for inheritance)
        if (st.VanillaCustomConsumption)
        {
            int ci;
            for (ci = 0; ci < st.VanillaCustomConsumption.Count(); ci = ci + 1)
            {
                LFPG_VanillaConsumptionEntry entry = st.VanillaCustomConsumption[ci];
                if (!entry)
                    continue;
                if (entry.classname == "")
                    continue;
                if (e.IsKindOf(entry.classname))
                {
                    result = entry.consumption;
                    hadCustomMatch = true;
                    break;
                }
            }
        }

        // Step 2: No custom match → try CompEM native usage
        if (!hadCustomMatch)
        {
            ComponentEnergyManager em = e.GetCompEM();
            if (em)
            {
                float emUsage = em.GetEnergyUsage();
                if (emUsage > 0.0)
                {
                    result = emUsage;
                }
            }
        }

        // Step 3: result is already VanillaDefaultConsumption if neither matched

        s_ConsumptionCache.Set(typeName, result);
        return result;
    }
};
