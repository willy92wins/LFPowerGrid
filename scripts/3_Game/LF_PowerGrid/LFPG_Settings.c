// =========================================================
// LF_PowerGrid - server settings (profiles JSON)
//
// v0.7.15 (Sprint 3 P2b): Atomic save + backup restore.
// v0.7.16 (Hotfix H4): AtomicVerifyReadback setting.
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
