// =========================================================
// LF_PowerGrid - BTC ATM Config (Sprint BTC-1)
//
// JSON-serializable settings for the Bitcoin ATM system.
// Loaded from $profile:LF_PowerGrid/LF_BTCAtm.json.
// Default API: Binance (no key required).
// Also compatible with CoinGecko, Blockchain.info, etc.
// Follows LFPG_Settings.c / LFPG_ServerSettings pattern.
//
// Currency entries define physical money items and their
// denomination values (sorted descending on load for
// greedy change-making algorithm in Sprint 3).
// =========================================================

// ---- Single currency denomination ----
class LFPG_BTCCurrency
{
    string classname;   // a money item that exists on THIS server (see LogCatalogHelp)
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
    int maxEurPerOperation;     // BTC large-amount cap 2026-05-19: hard cap on eurAmount per WithdrawCash/DepositCash tx

    // ATM behavior
    bool atmWithdrawOnlyDefault;  // Global floor applied in the getter under SERVER; not written to persisted state; a per-ATM off cannot override it while active

    // Balance provider mode: "auto", "native", "lbmaster"
    string balanceMode;

    // Currency denominations (physical money items)
    ref array<ref LFPG_BTCCurrency> currencies;

    void LFPG_BTCSettingsData()
    {
        ver = 1;
        enabled = true;
        apiUrl = "https://api.binance.com";
        apiPath = "/api/v3/ticker/24hr?symbol=BTCEUR";
        apiKey = "";
        vsCurrency = "eur";
        refreshSeconds = 60;
        btcItemClassname = "Ammo_9x19_25Rnd";
        maxBtcPerMachine = 100;
        maxEurPerOperation = 100000;
        atmWithdrawOnlyDefault = false;
        balanceMode = "auto";
        // Currency catalog ships EMPTY on purpose.
        //
        // It used to default to Paper_Bill_100/50/10/1. Those classes do not
        // exist: verified 2026-09-07 by scanning the 124 PBOs of a DayZServer
        // install (0 hits, with Battery9V as positive control at 4 hits), and
        // this mod's own config.cpp does not declare them either. So a fresh
        // install wrote that catalog to disk and then rejected it as invalid,
        // disabling every cash operation on a server whose admin had not
        // touched anything.
        //
        // Any concrete default is a guess about somebody else's economy mod,
        // and that guess already went wrong once. An empty catalog fails
        // closed the same way but says so out loud (see LogCatalogHelp).
        currencies = new array<ref LFPG_BTCCurrency>;
    }
};

// ---- Static config accessor (singleton pattern) ----
class LFPG_BTCConfig
{
    protected static ref LFPG_BTCSettingsData s_Data;
    protected static bool s_Loaded = false;
    protected static bool s_CurrencyCatalogValid = false;

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
        }
        else
        {
            string createMsg = "[LFPG_BTCConfig] File not found, creating defaults: ";
            createMsg = createMsg + settingsFile;
            LFPG_Util.Info(createMsg);
            Save();
        }

        ValidateAndClamp();
        LogSettings();

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

        // maxEurPerOperation: min 100, max 10000000
        if (s_Data.maxEurPerOperation < 100)
        {
            s_Data.maxEurPerOperation = 100;
        }
        if (s_Data.maxEurPerOperation > 10000000)
        {
            s_Data.maxEurPerOperation = 10000000;
        }

        // apiUrl: must not be empty
        if (s_Data.apiUrl == "")
        {
            s_Data.apiUrl = "https://api.binance.com";
            string warnUrl = "[LFPG_BTCConfig] apiUrl empty, reset to default";
            LFPG_Util.Warn(warnUrl);
        }

        // apiPath: must not be empty
        if (s_Data.apiPath == "")
        {
            s_Data.apiPath = "/api/v3/ticker/24hr?symbol=BTCEUR";
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

        // balanceMode: must be auto, native, or lbmaster
        if (s_Data.balanceMode == "")
        {
            s_Data.balanceMode = "auto";
        }
        string bmCheck = s_Data.balanceMode;
        bmCheck.ToLower();
        if (bmCheck != "auto" && bmCheck != "native" && bmCheck != "lbmaster")
        {
            string warnBm = "[LFPG_BTCConfig] Invalid balanceMode '";
            warnBm = warnBm + s_Data.balanceMode;
            warnBm = warnBm + "', reset to auto";
            LFPG_Util.Warn(warnBm);
            s_Data.balanceMode = "auto";
        }
        else
        {
            s_Data.balanceMode = bmCheck;
        }

        // currencies: fail closed on empty, duplicate, BTC overlap, or invalid class.
        // Null entries are removed because they are not economic values. Invalid
        // remaining entries are not rewritten with invented denominations.
        s_CurrencyCatalogValid = true;
        if (!s_Data.currencies)
        {
            s_Data.currencies = new array<ref LFPG_BTCCurrency>;
            s_CurrencyCatalogValid = false;
            LFPG_Util.Error("[LFPG_BTCConfig] Currency catalog missing; cash operations fail closed");
        }

        int ci;
        for (ci = s_Data.currencies.Count() - 1; ci >= 0; ci = ci - 1)
        {
            if (!s_Data.currencies[ci])
            {
                s_Data.currencies.Remove(ci);
                LFPG_Util.Warn("[LFPG_BTCConfig] Removed null currency entry at idx " + ci.ToString());
            }
        }

        if (s_Data.currencies.Count() == 0)
        {
            s_CurrencyCatalogValid = false;
            LFPG_Util.Error("[LFPG_BTCConfig] Currency catalog empty after null removal; cash operations fail closed");
        }
        if (s_Data.currencies.Count() > 16)
        {
            s_CurrencyCatalogValid = false;
            LFPG_Util.Error("[LFPG_BTCConfig] Currency catalog exceeds 16 entries; cash operations fail closed");
        }

        string btcCls = s_Data.btcItemClassname;
        string btcKey = btcCls + "";
        btcKey.ToLower();
        array<string> seenClassnames = new array<string>;
        string classnameKey = "";
        for (ci = 0; ci < s_Data.currencies.Count(); ci = ci + 1)
        {
            LFPG_BTCCurrency cur = s_Data.currencies[ci];
            if (!cur)
            {
                s_CurrencyCatalogValid = false;
                continue;
            }

            classnameKey = cur.classname + "";
            classnameKey.ToLower();

            if (cur.classname == "")
            {
                s_CurrencyCatalogValid = false;
                LFPG_Util.Error("[LFPG_BTCConfig] Currency entry " + ci.ToString() + " has empty classname");
            }
            else if (seenClassnames.Find(classnameKey) >= 0)
            {
                s_CurrencyCatalogValid = false;
                LFPG_Util.Error("[LFPG_BTCConfig] Duplicate currency classname: " + cur.classname);
            }
            else
            {
                seenClassnames.Insert(classnameKey);
            }

            if (btcKey != "" && classnameKey == btcKey)
            {
                s_CurrencyCatalogValid = false;
                LFPG_Util.Error("[LFPG_BTCConfig] Currency classname overlaps btcItemClassname: " + cur.classname);
            }

            if (cur.classname != "" && !ConfigClassExists(cur.classname))
            {
                s_CurrencyCatalogValid = false;
                LFPG_Util.Error("[LFPG_BTCConfig] Currency classname is not a CfgVehicles/CfgMagazines/CfgWeapons class: " + cur.classname);
            }

            if (cur.value < 1 || cur.value > 10000000)
            {
                s_CurrencyCatalogValid = false;
                string warnVal = "[LFPG_BTCConfig] Currency entry ";
                warnVal = warnVal + ci.ToString();
                warnVal = warnVal + " has invalid value (";
                warnVal = warnVal + cur.value.ToString();
                warnVal = warnVal + "); cash operations fail closed";
                LFPG_Util.Error(warnVal);
            }
        }

        if (!s_CurrencyCatalogValid)
        {
            LogCatalogHelp();
        }
    }

    // ---- Verbose operator-facing help when the catalog is refused ----
    //
    // Printed once per boot, right after the per-entry reasons above, because
    // the admin who has to fix this is reading a log file and not this source.
    // One Error() call per line on purpose: level 0 is always written, and
    // building the block without escape sequences keeps it away from the
    // "CParser: quoted string not closed" trap.
    protected static void LogCatalogHelp()
    {
        int total = 0;
        if (s_Data && s_Data.currencies)
        {
            total = s_Data.currencies.Count();
        }

        LFPG_Util.Error("[LFPG_BTCConfig] ============================================================");
        LFPG_Util.Error("[LFPG_BTCConfig] CURRENCY CATALOG REFUSED - ALL CASH OPERATIONS ARE DISABLED");
        LFPG_Util.Error("[LFPG_BTCConfig] ============================================================");

        if (total == 0)
        {
            LFPG_Util.Error("[LFPG_BTCConfig] The catalog is EMPTY. This mod ships no default denominations,");
            LFPG_Util.Error("[LFPG_BTCConfig] because it cannot know which money items your server uses.");
            LFPG_Util.Error("[LFPG_BTCConfig] You are seeing this for one of three reasons:");
            LFPG_Util.Error("[LFPG_BTCConfig]   a) first run - the file was just created for you, fill it in;");
            LFPG_Util.Error("[LFPG_BTCConfig]   b) the file failed to parse, so defaults were used (see the");
            LFPG_Util.Error("[LFPG_BTCConfig]      load error logged above this block);");
            LFPG_Util.Error("[LFPG_BTCConfig]   c) currencies was left as an empty list on purpose.");
        }
        else
        {
            string cntMsg = "[LFPG_BTCConfig] The catalog has ";
            cntMsg = cntMsg + total.ToString();
            cntMsg = cntMsg + " entries and at least one of them was rejected.";
            LFPG_Util.Error(cntMsg);
            LFPG_Util.Error("[LFPG_BTCConfig] The reason for each one is logged immediately above this block.");
            LFPG_Util.Error("[LFPG_BTCConfig] One bad entry refuses the WHOLE catalog - money is not partially trusted.");
        }

        LFPG_Util.Error("[LFPG_BTCConfig] ");
        LFPG_Util.Error("[LFPG_BTCConfig] WHAT IS DISABLED: deposit, withdraw, selling for cash, and the ATM");
        LFPG_Util.Error("[LFPG_BTCConfig] balance read. They refuse cleanly rather than move the wrong amount.");
        LFPG_Util.Error("[LFPG_BTCConfig] Everything else in LF_PowerGrid keeps working normally.");
        LFPG_Util.Error("[LFPG_BTCConfig] ");

        string fileMsg = "[LFPG_BTCConfig] FILE TO EDIT: ";
        fileMsg = fileMsg + LFPG_BTC_SETTINGS_FILE;
        LFPG_Util.Error(fileMsg);
        LFPG_Util.Error("[LFPG_BTCConfig] THIS IS NOT RE-READ WHILE RUNNING. Edit it, then RESTART the server.");
        LFPG_Util.Error("[LFPG_BTCConfig] ");
        LFPG_Util.Error("[LFPG_BTCConfig] EXPECTED SHAPE (classname = a real item, value = what it is worth):");
        LFPG_Util.Error("[LFPG_BTCConfig]     currencies: [");
        LFPG_Util.Error("[LFPG_BTCConfig]         { classname: SomeBanknote_100, value: 100 },");
        LFPG_Util.Error("[LFPG_BTCConfig]         { classname: SomeBanknote_10,  value: 10 }");
        LFPG_Util.Error("[LFPG_BTCConfig]     ]");
        LFPG_Util.Error("[LFPG_BTCConfig] ");
        LFPG_Util.Error("[LFPG_BTCConfig] EVERY ENTRY MUST SATISFY ALL OF:");
        LFPG_Util.Error("[LFPG_BTCConfig]   1. classname is not empty");
        LFPG_Util.Error("[LFPG_BTCConfig]   2. classname exists in CfgVehicles, CfgMagazines or CfgWeapons");
        LFPG_Util.Error("[LFPG_BTCConfig]      -> it must come from vanilla or from a mod this server LOADS");
        LFPG_Util.Error("[LFPG_BTCConfig]   3. classname is not repeated (upper/lower case does not make it different)");

        string btcMsg = "[LFPG_BTCConfig]   4. classname is not the BTC item itself, currently: ";
        btcMsg = btcMsg + s_Data.btcItemClassname;
        LFPG_Util.Error(btcMsg);

        LFPG_Util.Error("[LFPG_BTCConfig]   5. value is a whole number between 1 and 10000000");
        LFPG_Util.Error("[LFPG_BTCConfig]   6. the list holds at least 1 and at most 16 entries");
        LFPG_Util.Error("[LFPG_BTCConfig] ============================================================");
    }

    protected static bool ConfigClassExists(string classname)
    {
        if (classname == "")
            return false;
        if (!GetGame())
            return false;

        string vehiclesPath = "CfgVehicles ";
        vehiclesPath = vehiclesPath + classname;
        vehiclesPath = vehiclesPath + " ";
        if (GetGame().ConfigIsExisting(vehiclesPath))
            return true;

        string magPath = "CfgMagazines ";
        magPath = magPath + classname;
        magPath = magPath + " ";
        if (GetGame().ConfigIsExisting(magPath))
            return true;

        string weapPath = "CfgWeapons ";
        weapPath = weapPath + classname;
        weapPath = weapPath + " ";
        if (GetGame().ConfigIsExisting(weapPath))
            return true;

        return false;
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
                LFPG_BTCCurrency a = s_Data.currencies[i];
                LFPG_BTCCurrency b = s_Data.currencies[j];
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
            LFPG_BTCCurrency entry = s_Data.currencies[i];
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
        msg = msg + " balanceMode=";
        msg = msg + s_Data.balanceMode;

        int curCount = 0;
        if (s_Data.currencies)
        {
            curCount = s_Data.currencies.Count();
        }
        msg = msg + " currencies=";
        msg = msg + curCount.ToString();
        msg = msg + " catalogValid=";
        msg = msg + s_CurrencyCatalogValid.ToString();

        LFPG_Util.Info(msg);
    }

    // ---- Convenience getters ----

    static bool IsEnabled()
    {
        LFPG_BTCSettingsData d = Get();
        return d.enabled;
    }

    static bool IsCurrencyCatalogValid()
    {
        Get();
        return s_CurrencyCatalogValid;
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

    static int GetMaxEurPerOperation()
    {
        LFPG_BTCSettingsData d = Get();
        return d.maxEurPerOperation;
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

    static string GetBalanceMode()
    {
        LFPG_BTCSettingsData d = Get();
        return d.balanceMode;
    }

    // Returns the currencies array (sorted descending by value).
    static array<ref LFPG_BTCCurrency> GetCurrencies()
    {
        LFPG_BTCSettingsData d = Get();
        return d.currencies;
    }
};
