// =========================================================
// LF_PowerGrid — Native Balance Provider
//
// Standalone player balance system. No external mod dependency.
// Persists balances to $profile:LF_PowerGrid/LF_Balances.json.
// Save triggered on every transaction for data safety.
//
// In-memory: map<string, int> for O(1) lookup by UID.
// On-disk: array of LFPG_BalanceEntry (JSON-serializable).
// =========================================================

// ---- Single player balance entry (JSON-serializable) ----
class LFPG_BalanceEntry
{
    string uid;
    int balance;

    void LFPG_BalanceEntry()
    {
        uid = "";
        balance = 0;
    }
};

// ---- On-disk data container ----
class LFPG_BalanceData
{
    int ver;
    ref array<ref LFPG_BalanceEntry> entries;

    void LFPG_BalanceData()
    {
        ver = 1;
        entries = new array<ref LFPG_BalanceEntry>;
    }
};

// ---- Native provider implementation ----
class LFPG_BalanceProvider_Native extends LFPG_BalanceProvider
{
    protected static ref map<string, int> s_Balances;
    protected static bool s_Loaded;

    void LFPG_BalanceProvider_Native()
    {
        m_Name = "Native";
        m_Priority = 0;
    }

    override int GetBalance(PlayerBase player)
    {
        string uid = GetUID(player);
        if (uid == "")
            return 0;

        EnsureLoaded();

        if (s_Balances.Contains(uid))
        {
            int bal = s_Balances.Get(uid);
            return bal;
        }

        return 0;
    }

    override int AddBalance(PlayerBase player, int amount)
    {
        if (amount <= 0)
            return 0;

        string uid = GetUID(player);
        if (uid == "")
            return 0;

        EnsureLoaded();

        int current = 0;
        if (s_Balances.Contains(uid))
        {
            current = s_Balances.Get(uid);
        }

        int newBal = current + amount;
        s_Balances.Set(uid, newBal);

        SaveToDisk();

        string logMsg = "[LFPG_Balance_Native] Add uid=";
        logMsg = logMsg + uid;
        logMsg = logMsg + " +";
        logMsg = logMsg + amount.ToString();
        logMsg = logMsg + " -> ";
        logMsg = logMsg + newBal.ToString();
        LFPG_Util.Info(logMsg);

        return amount;
    }

    override int RemoveBalance(PlayerBase player, int amount)
    {
        if (amount <= 0)
            return 0;

        string uid = GetUID(player);
        if (uid == "")
            return 0;

        EnsureLoaded();

        int current = 0;
        if (s_Balances.Contains(uid))
        {
            current = s_Balances.Get(uid);
        }

        // Cannot remove more than available
        int toRemove = amount;
        if (toRemove > current)
        {
            toRemove = current;
        }

        int newBal = current - toRemove;
        s_Balances.Set(uid, newBal);

        SaveToDisk();

        string logMsg = "[LFPG_Balance_Native] Remove uid=";
        logMsg = logMsg + uid;
        logMsg = logMsg + " -";
        logMsg = logMsg + toRemove.ToString();
        logMsg = logMsg + " -> ";
        logMsg = logMsg + newBal.ToString();
        LFPG_Util.Info(logMsg);

        return toRemove;
    }

    // ---- Static API for external mods ----

    static int GetPlayerBalance(string uid)
    {
        EnsureLoaded();

        if (s_Balances.Contains(uid))
        {
            int bal = s_Balances.Get(uid);
            return bal;
        }

        return 0;
    }

    static void SetPlayerBalance(string uid, int balance)
    {
        EnsureLoaded();

        s_Balances.Set(uid, balance);
        SaveToDisk();
    }

    // ---- Internal ----

    protected static string GetUID(PlayerBase player)
    {
        if (!player)
            return "";

        PlayerIdentity identity = player.GetIdentity();
        if (!identity)
            return "";

        string uid = identity.GetPlainId();
        return uid;
    }

    protected static void EnsureLoaded()
    {
        if (s_Loaded)
            return;

        if (!s_Balances)
        {
            s_Balances = new map<string, int>;
        }

        LoadFromDisk();
        s_Loaded = true;
    }

    protected static void LoadFromDisk()
    {
        if (!s_Balances)
        {
            s_Balances = new map<string, int>;
        }

        string settingsDir = LFPG_BTC_SETTINGS_DIR;
        if (!FileExist(settingsDir))
        {
            MakeDirectory(settingsDir);
        }

        string filePath = LFPG_BALANCE_NATIVE_FILE;

        if (!FileExist(filePath))
        {
            string noFileMsg = "[LFPG_Balance_Native] No balances file, starting fresh: ";
            noFileMsg = noFileMsg + filePath;
            LFPG_Util.Info(noFileMsg);
            return;
        }

        ref LFPG_BalanceData data = new LFPG_BalanceData();
        string err;
        bool ok = JsonFileLoader<LFPG_BalanceData>.LoadFile(filePath, data, err);
        if (!ok)
        {
            string errMsg = "[LFPG_Balance_Native] Load failed: ";
            errMsg = errMsg + err;
            LFPG_Util.Error(errMsg);
            return;
        }

        if (!data.entries)
            return;

        int i = 0;
        int count = data.entries.Count();
        for (i = 0; i < count; i = i + 1)
        {
            LFPG_BalanceEntry entry = data.entries[i];
            if (!entry)
                continue;
            if (entry.uid == "")
                continue;

            s_Balances.Set(entry.uid, entry.balance);
        }

        string loadMsg = "[LFPG_Balance_Native] Loaded ";
        loadMsg = loadMsg + count.ToString();
        loadMsg = loadMsg + " player balances from ";
        loadMsg = loadMsg + filePath;
        LFPG_Util.Info(loadMsg);
    }

    protected static void SaveToDisk()
    {
        string settingsDir = LFPG_BTC_SETTINGS_DIR;
        if (!FileExist(settingsDir))
        {
            MakeDirectory(settingsDir);
        }

        ref LFPG_BalanceData data = new LFPG_BalanceData();

        // Rebuild entries array from map
        if (s_Balances)
        {
            int i = 0;
            int count = s_Balances.Count();
            for (i = 0; i < count; i = i + 1)
            {
                string uid = s_Balances.GetKey(i);
                int bal = s_Balances.GetElement(i);

                ref LFPG_BalanceEntry entry = new LFPG_BalanceEntry();
                entry.uid = uid;
                entry.balance = bal;
                data.entries.Insert(entry);
            }
        }

        string filePath = LFPG_BALANCE_NATIVE_FILE;
        string err;
        bool ok = JsonFileLoader<LFPG_BalanceData>.SaveFile(filePath, data, err);
        if (!ok)
        {
            string errMsg = "[LFPG_Balance_Native] Save failed: ";
            errMsg = errMsg + err;
            LFPG_Util.Error(errMsg);
        }
    }
};
