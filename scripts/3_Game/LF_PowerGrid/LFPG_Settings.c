// =========================================================
// LF_PowerGrid - server settings (profiles JSON)
//
// v0.7.15 (Sprint 3 P2b): Atomic save + backup restore.
// v0.7.16 (Hotfix H4): AtomicVerifyReadback setting.
// v0.7.35 (Gemini 3b): Range validation + clamping on load.
// =========================================================

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
    protected static void ValidateAndClamp()
    {
        if (!s_Settings)
            return;

        int clamped = 0;
        int intVal;
        float floatVal;

        // --- ver: must be >= 1 ---
        if (s_Settings.ver < 1)
        {
            LFPG_Util.Warn("Settings: ver=" + s_Settings.ver.ToString() + " invalid, reset to 1");
            s_Settings.ver = 1;
            clamped = clamped + 1;
        }

        // --- MaxWiresPerPlayer ---
        intVal = ClampInt(s_Settings.MaxWiresPerPlayer,
                          LFPG_SETTINGS_MIN_WIRES_PLAYER,
                          LFPG_SETTINGS_MAX_WIRES_PLAYER);
        if (intVal != s_Settings.MaxWiresPerPlayer)
        {
            LFPG_Util.Warn("Settings: MaxWiresPerPlayer=" + s_Settings.MaxWiresPerPlayer.ToString()
                + " clamped to " + intVal.ToString()
                + " [" + LFPG_SETTINGS_MIN_WIRES_PLAYER.ToString()
                + ".." + LFPG_SETTINGS_MAX_WIRES_PLAYER.ToString() + "]");
            s_Settings.MaxWiresPerPlayer = intVal;
            clamped = clamped + 1;
        }

        // --- MaxWiresPerDevice ---
        intVal = ClampInt(s_Settings.MaxWiresPerDevice,
                          LFPG_SETTINGS_MIN_WIRES_DEVICE,
                          LFPG_SETTINGS_MAX_WIRES_DEVICE);
        if (intVal != s_Settings.MaxWiresPerDevice)
        {
            LFPG_Util.Warn("Settings: MaxWiresPerDevice=" + s_Settings.MaxWiresPerDevice.ToString()
                + " clamped to " + intVal.ToString()
                + " [" + LFPG_SETTINGS_MIN_WIRES_DEVICE.ToString()
                + ".." + LFPG_SETTINGS_MAX_WIRES_DEVICE.ToString() + "]");
            s_Settings.MaxWiresPerDevice = intVal;
            clamped = clamped + 1;
        }

        // --- RpcCooldownSeconds ---
        floatVal = ClampFloat(s_Settings.RpcCooldownSeconds,
                              LFPG_SETTINGS_MIN_RPC_COOLDOWN,
                              LFPG_SETTINGS_MAX_RPC_COOLDOWN);
        if (floatVal != s_Settings.RpcCooldownSeconds)
        {
            LFPG_Util.Warn("Settings: RpcCooldownSeconds=" + s_Settings.RpcCooldownSeconds.ToString()
                + " clamped to " + floatVal.ToString()
                + " [" + LFPG_SETTINGS_MIN_RPC_COOLDOWN.ToString()
                + ".." + LFPG_SETTINGS_MAX_RPC_COOLDOWN.ToString() + "]");
            s_Settings.RpcCooldownSeconds = floatVal;
            clamped = clamped + 1;
        }

        // --- DeviceBubbleM ---
        floatVal = ClampFloat(s_Settings.DeviceBubbleM,
                              LFPG_SETTINGS_MIN_BUBBLE_M,
                              LFPG_SETTINGS_MAX_BUBBLE_M);
        if (floatVal != s_Settings.DeviceBubbleM)
        {
            LFPG_Util.Warn("Settings: DeviceBubbleM=" + s_Settings.DeviceBubbleM.ToString()
                + " clamped to " + floatVal.ToString()
                + " [" + LFPG_SETTINGS_MIN_BUBBLE_M.ToString()
                + ".." + LFPG_SETTINGS_MAX_BUBBLE_M.ToString() + "]");
            s_Settings.DeviceBubbleM = floatVal;
            clamped = clamped + 1;
        }

        if (clamped > 0)
        {
            LFPG_Util.Warn("Settings: " + clamped.ToString() + " value(s) clamped. Review LF_PowerGrid.json");
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
                LFPG_Util.Info(msg);
            }
        }
        else
        {
            LFPG_Util.Info("Settings file not found; creating defaults: " + SETTINGS_FILE);
            Save(); // create defaults
        }
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
};
