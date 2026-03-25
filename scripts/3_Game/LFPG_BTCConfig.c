// =========================================================
// LF_PowerGrid - BTC ATM Config (Sprint BTC-1)
//
// JSON-serializable settings for the Bitcoin ATM system.
// Loaded from $profile:LF_PowerGrid/LF_BTCAtm.json.
// Follows LFPG_Settings.c / LFPG_ServerSettings pattern.
//
// Currency entries define physical money items and their
// denomination values (sorted descending on load for
// greedy change-making algorithm in Sprint 3).
// =========================================================

// ---- Single currency denomination ----
class LFPG_BTCCurrency
{
    string classname;   // e.g. "Paper_Bill_100"
    int value;          // e.g. 100

    void LFPG_BTCCurrency()
    {
        classname = "";
        value = 0;
    }
};

// ---- Main BTC ATM settings (JSON-serializable) ----
class LFPG_BTCSettingsData
{
    int ver;

    // Master toggle: if false, BTC ATM system is completely disabled
    // (no price fetcher, no timer, no RPC handling)
    bool enabled;

    // API configuration
    string apiUrl;              // Base URL for price API
    string apiPath;             // GET path (appended to apiUrl)
    string apiKey;              // Optional API key (empty = free tier)
    string vsCurrency;          // Fiat currency code for API (e.g. "eur", "usd")
    int refreshSeconds;         // Seconds between price fetches

    // BTC item
    string btcItemClassname;    // Classname of the physical BTC item
    int maxBtcPerMachine;       // Max BTC stock per ATM machine

    // ATM behavior
    bool atmWithdrawOnlyDefault;  // Default for new ATMs

    // Currency denominations (physical money items)
    ref array<ref LFPG_BTCCurrency> currencies;

    void LFPG_BTCSettingsData()
    {
        ver = 1;
        enabled = true;
        apiUrl = "https://api.coingecko.com";
        apiPath = "/api/v3/simple/price?ids=bitcoin&vs_currencies=eur";
        apiKey = "";
        vsCurrency = "eur";
        refreshSeconds = 60;
        btcItemClassname = "Ammo_9x19_25Rnd";
        maxBtcPerMachine = 100;
        atmWithdrawOnlyDefault = false;
        currencies = new array<ref LFPG_BTCCurrency>;

        // Default denominations
        ref LFPG_BTCCurrency c100 = new LFPG_BTCCurrency();
        c100.classname = "Paper_Bill_100";
        c100.value = 100;
        currencies.Insert(c100);

        ref LFPG_BTCCurrency c50 = new LFPG_BTCCurrency();
        c50.classname = "Paper_Bill_50";
        c50.value = 50;
        currencies.Insert(c50);

        ref LFPG_BTCCurrency c10 = new LFPG_BTCCurrency();
        c10.classname = "Paper_Bill_10";
        c10.value = 10;
        currencies.Insert(c10);

        ref LFPG_BTCCurrency c1 = new LFPG_BTCCurrency();
        c1.classname = "Paper_Bill_1";
        c1.value = 1;
        currencies.Insert(c1);
    }
};

// ---- Static config accessor (singleton pattern) ----
class LFPG_BTCConfig
{
    protected static ref LFPG_BTCSettingsData s_Data;
    protected static bool s_Loaded = false;

    static LFPG_BTCSettingsData Get()
    {
        if (!s_Data)
        {
            Load();
        }
        return s_Data;
    }

    static void Load()
    {
        if (!s_Data)
        {
            s_Data = new LFPG_BTCSettingsData();
        }

        string settingsDir = LFPG_BTC_SETTINGS_DIR;
        if (!FileExist(settingsDir))
        {
            MakeDirectory(settingsDir);
        }

        string settingsFile = LFPG_BTC_SETTINGS_FILE;

        if (FileExist(settingsFile))
        {
            string err;
            bool loadOk = JsonFileLoader<LFPG_BTCSettingsData>.LoadFile(settingsFile, s_Data, err);
            if (!loadOk)
            {
                string warnMsg = "[LFPG_BTCConfig] Load failed, using defaults. ";
                warnMsg = warnMsg + err;
                LFPG_Util.Warn(warnMsg);
                s_Data = new LFPG_BTCSettingsData();
            }
            else
            {
                ValidateAndClamp();
                LogSettings();
            }
        }
        else
        {
            string createMsg = "[LFPG_BTCConfig] File not found, creating defaults: ";
            createMsg = createMsg + settingsFile;
            LFPG_Util.Info(createMsg);
            Save();
        }

        // Sort currencies descending by value (for greedy change algorithm)
        SortCurrenciesDesc();

        s_Loaded = true;
    }

    static void Save()
    {
        if (!s_Data)
        {
            s_Data = new LFPG_BTCSettingsData();
        }

        string settingsDir = LFPG_BTC_SETTINGS_DIR;
        if (!FileExist(settingsDir))
        {
            MakeDirectory(settingsDir);
        }

        string settingsFile = LFPG_BTC_SETTINGS_FILE;
        string err;
        bool saveOk = JsonFileLoader<LFPG_BTCSettingsData>.SaveFile(settingsFile, s_Data, err);
        if (!saveOk)
        {
            string errMsg = "[LFPG_BTCConfig] Save failed: ";
            errMsg = errMsg + err;
            LFPG_Util.Error(errMsg);
        }
        else
        {
            string okMsg = "[LFPG_BTCConfig] Settings saved: ";
            okMsg = okMsg + settingsFile;
            LFPG_Util.Info(okMsg);
        }
    }

    // ---- Validation ----
    protected static void ValidateAndClamp()
    {
        if (!s_Data)
            return;

        // refreshSeconds: min 10, max 3600
        if (s_Data.refreshSeconds < 10)
        {
            string warnRefresh = "[LFPG_BTCConfig] refreshSeconds too low (";
            warnRefresh = warnRefresh + s_Data.refreshSeconds.ToString();
            warnRefresh = warnRefresh + "), clamping to 10";
            LFPG_Util.Warn(warnRefresh);
            s_Data.refreshSeconds = 10;
        }
        if (s_Data.refreshSeconds > 3600)
        {
            s_Data.refreshSeconds = 3600;
        }

        // maxBtcPerMachine: min 1, max 10000
        if (s_Data.maxBtcPerMachine < 1)
        {
            s_Data.maxBtcPerMachine = 1;
        }
        if (s_Data.maxBtcPerMachine > 10000)
        {
            s_Data.maxBtcPerMachine = 10000;
        }

        // apiUrl: must not be empty
        if (s_Data.apiUrl == "")
        {
            s_Data.apiUrl = "https://api.coingecko.com";
            string warnUrl = "[LFPG_BTCConfig] apiUrl empty, reset to default";
            LFPG_Util.Warn(warnUrl);
        }

        // apiPath: must not be empty
        if (s_Data.apiPath == "")
        {
            s_Data.apiPath = "/api/v3/simple/price?ids=bitcoin&vs_currencies=eur";
            string warnPath = "[LFPG_BTCConfig] apiPath empty, reset to default";
            LFPG_Util.Warn(warnPath);
        }

        // vsCurrency: must not be empty
        if (s_Data.vsCurrency == "")
        {
            s_Data.vsCurrency = "eur";
            string warnVs = "[LFPG_BTCConfig] vsCurrency empty, reset to eur";
            LFPG_Util.Warn(warnVs);
        }

        // btcItemClassname: must not be empty
        if (s_Data.btcItemClassname == "")
        {
            s_Data.btcItemClassname = "Ammo_9x19_25Rnd";
            string warnItem = "[LFPG_BTCConfig] btcItemClassname empty, reset to default";
            LFPG_Util.Warn(warnItem);
        }

        // currencies: must have at least one entry
        if (!s_Data.currencies || s_Data.currencies.Count() == 0)
        {
            string warnCur = "[LFPG_BTCConfig] No currencies defined, creating defaults";
            LFPG_Util.Warn(warnCur);
            s_Data.currencies = new array<ref LFPG_BTCCurrency>;

            ref LFPG_BTCCurrency fallback = new LFPG_BTCCurrency();
            fallback.classname = "Paper_Bill_1";
            fallback.value = 1;
            s_Data.currencies.Insert(fallback);
        }

        // Validate each currency entry
        int ci;
        for (ci = 0; ci < s_Data.currencies.Count(); ci = ci + 1)
        {
            ref LFPG_BTCCurrency cur = s_Data.currencies[ci];
            if (!cur)
                continue;

            if (cur.classname == "")
            {
                string warnCls = "[LFPG_BTCConfig] Currency entry ";
                warnCls = warnCls + ci.ToString();
                warnCls = warnCls + " has empty classname";
                LFPG_Util.Warn(warnCls);
            }

            if (cur.value < 1)
            {
                string warnVal = "[LFPG_BTCConfig] Currency entry ";
                warnVal = warnVal + ci.ToString();
                warnVal = warnVal + " has invalid value (";
                warnVal = warnVal + cur.value.ToString();
                warnVal = warnVal + "), clamping to 1";
                LFPG_Util.Warn(warnVal);
                cur.value = 1;
            }
        }
    }

    // ---- Sort currencies descending by value (bubble sort) ----
    // Required for greedy change-making: largest denomination first.
    protected static void SortCurrenciesDesc()
    {
        if (!s_Data || !s_Data.currencies)
            return;

        int count = s_Data.currencies.Count();
        if (count < 2)
            return;

        // Simple bubble sort (N is tiny, ~4-10 entries max)
        int i;
        int j;
        bool swapped = true;
        while (swapped)
        {
            swapped = false;
            for (i = 0; i < count - 1; i = i + 1)
            {
                j = i + 1;
                ref LFPG_BTCCurrency a = s_Data.currencies[i];
                ref LFPG_BTCCurrency b = s_Data.currencies[j];
                if (a && b && a.value < b.value)
                {
                    // Swap
                    s_Data.currencies.Set(i, b);
                    s_Data.currencies.Set(j, a);
                    swapped = true;
                }
            }
        }

        // Log sorted order
        string sortMsg = "[LFPG_BTCConfig] Currencies sorted (desc): ";
        for (i = 0; i < count; i = i + 1)
        {
            ref LFPG_BTCCurrency entry = s_Data.currencies[i];
            if (!entry)
                continue;
            if (i > 0)
            {
                sortMsg = sortMsg + ", ";
            }
            sortMsg = sortMsg + entry.classname;
            sortMsg = sortMsg + "=";
            sortMsg = sortMsg + entry.value.ToString();
        }
        LFPG_Util.Info(sortMsg);
    }

    // ---- Logging ----
    protected static void LogSettings()
    {
        if (!s_Data)
            return;

        string msg = "[LFPG_BTCConfig] Loaded:";
        msg = msg + " enabled=";
        msg = msg + s_Data.enabled.ToString();
        msg = msg + " apiUrl=";
        msg = msg + s_Data.apiUrl;
        msg = msg + " refreshS=";
        msg = msg + s_Data.refreshSeconds.ToString();
        msg = msg + " btcItem=";
        msg = msg + s_Data.btcItemClassname;
        msg = msg + " maxBtc=";
        msg = msg + s_Data.maxBtcPerMachine.ToString();
        msg = msg + " withdrawOnly=";
        msg = msg + s_Data.atmWithdrawOnlyDefault.ToString();

        int curCount = 0;
        if (s_Data.currencies)
        {
            curCount = s_Data.currencies.Count();
        }
        msg = msg + " currencies=";
        msg = msg + curCount.ToString();

        LFPG_Util.Info(msg);
    }

    // ---- Convenience getters ----

    static bool IsEnabled()
    {
        LFPG_BTCSettingsData d = Get();
        return d.enabled;
    }

    static float GetRefreshMs()
    {
        LFPG_BTCSettingsData d = Get();
        float ms = d.refreshSeconds * 1000.0;
        return ms;
    }

    static string GetBtcItemClassname()
    {
        LFPG_BTCSettingsData d = Get();
        return d.btcItemClassname;
    }

    static int GetMaxBtcPerMachine()
    {
        LFPG_BTCSettingsData d = Get();
        return d.maxBtcPerMachine;
    }

    static bool GetAtmWithdrawOnlyDefault()
    {
        LFPG_BTCSettingsData d = Get();
        return d.atmWithdrawOnlyDefault;
    }

    static string GetApiUrl()
    {
        LFPG_BTCSettingsData d = Get();
        return d.apiUrl;
    }

    static string GetApiPath()
    {
        LFPG_BTCSettingsData d = Get();
        return d.apiPath;
    }

    static string GetApiKey()
    {
        LFPG_BTCSettingsData d = Get();
        return d.apiKey;
    }

    static string GetVsCurrency()
    {
        LFPG_BTCSettingsData d = Get();
        return d.vsCurrency;
    }

    // Returns the currencies array (sorted descending by value).
    static array<ref LFPG_BTCCurrency> GetCurrencies()
    {
        LFPG_BTCSettingsData d = Get();
        return d.currencies;
    }
};
