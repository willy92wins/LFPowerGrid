// =========================================================
// LF_PowerGrid — Balance Provider System
//
// Abstract base class for player balance providers + registry
// with priority-based auto-selection.
//
// Providers register at startup with a name and priority.
// The registry resolves the active provider based on the
// balanceMode setting in LF_BTCAtm.json:
//   "auto"     -> highest priority supported provider, no fallback after exclusion
//   "native"   → force LFPG native (always available)
//   "lbmaster" -> LBmaster, unavailable if its banking API is missing
//
// Future providers (Expansion, Trader+, etc.) just need a
// new class extending LFPG_BalanceProvider and calling
// LFPG_BalanceRegistry.Register() with their priority.
// Optional integrations can override the read-only IsSupported check.
// =========================================================

class LFPG_BalanceProvider
{
    protected string m_Name;
    protected int m_Priority;

    void LFPG_BalanceProvider()
    {
        m_Name = "Unknown";
        m_Priority = 0;
    }

    string GetName()
    {
        return m_Name;
    }

    int GetPriority()
    {
        return m_Priority;
    }

    // --- API: override in subclasses ---

    // Preserve existing providers; optional APIs override this without account access.
    bool IsSupported()
    {
        return true;
    }

    int GetBalance(PlayerBase player)
    {
        return 0;
    }

    // Returns amount actually added
    int AddBalance(PlayerBase player, int amount)
    {
        return 0;
    }

    // Returns amount actually removed
    int RemoveBalance(PlayerBase player, int amount)
    {
        return 0;
    }
};

// =========================================================
// Registry — singleton, holds all registered providers
// =========================================================

class LFPG_BalanceRegistry
{
    protected static ref TManagedRefArray s_Providers;
    protected static ref LFPG_BalanceProvider s_Active;
    protected static bool s_Initialized;

    static void Init(string balanceMode)
    {
        if (!s_Providers)
        {
            s_Providers = new TManagedRefArray;
        }

        // Log all registered providers
        int i = 0;
        int count = s_Providers.Count();
        string regMsg = "[LFPG_Balance] Registered: ";
        for (i = 0; i < count; i = i + 1)
        {
            LFPG_BalanceProvider prov = LFPG_BalanceProvider.Cast(s_Providers[i]);
            if (!prov)
                continue;

            if (i > 0)
            {
                regMsg = regMsg + ", ";
            }
            string provName = prov.GetName();
            int provPrio = prov.GetPriority();
            regMsg = regMsg + provName;
            regMsg = regMsg + "(";
            regMsg = regMsg + provPrio.ToString();
            regMsg = regMsg + ")";
        }
        LFPG_Util.Info(regMsg);

        // Resolve active provider
        s_Active = null;

        string modeLower = balanceMode;
        modeLower.ToLower();

        if (modeLower == "native")
        {
            s_Active = FindByName("Native");
            if (!s_Active)
            {
                string errNative = "[LFPG_Balance] Mode=native but Native provider not registered!";
                LFPG_Util.Error(errNative);
            }
        }
        else if (modeLower == "lbmaster")
        {
            s_Active = FindByName("LBmaster");
            if (!s_Active)
            {
                string errLB = "[LFPG_Balance] Mode=lbmaster requested but LBmaster banking API is unavailable (missing provider or Core without banking). Requested configuration CANNOT be served. Account operations disabled; wallet switching intentionally disabled. If this server previously used LBmaster banking, restore that API; changing balanceMode does not recover or migrate balances, and using another wallet would leave the existing funds in the original bank. If this server never used LBmaster banking and Core was installed only as a dependency, set balanceMode to 'native' to make the ATM operational with the native wallet. The administrator must choose based on the server's history; startup cannot distinguish these cases and therefore does not switch wallets automatically.";
                LFPG_Util.Error(errLB);
            }
        }
        else
        {
            // "auto" or any unrecognized → highest priority
            s_Active = FindHighestPriority();
        }

        // Log result
        if (s_Active)
        {
            string activeMsg = "[LFPG_Balance] Mode=";
            activeMsg = activeMsg + balanceMode;
            string activeName = s_Active.GetName();
            int activePrio = s_Active.GetPriority();
            activeMsg = activeMsg + " -> Active: ";
            activeMsg = activeMsg + activeName;
            activeMsg = activeMsg + " (priority ";
            activeMsg = activeMsg + activePrio.ToString();
            activeMsg = activeMsg + ")";
            LFPG_Util.Info(activeMsg);
        }
        else
        {
            string noMsg = "[LFPG_Balance] Mode=";
            noMsg = noMsg + balanceMode;
            noMsg = noMsg + " -> No active provider. Account operations disabled; wallet unchanged intentionally. Physical BTC stock operations remain available. If this server previously used LBmaster banking, restore that API; changing balanceMode does not recover or migrate balances, and using another wallet would leave the existing funds in the original bank. If this server never used LBmaster banking and Core was installed only as a dependency, set balanceMode to 'native' to make the ATM operational with the native wallet. The administrator must choose based on the server's history; startup cannot distinguish these cases and therefore does not switch wallets automatically.";
            LFPG_Util.Error(noMsg);
        }

        s_Initialized = true;
    }

    static void Register(LFPG_BalanceProvider provider)
    {
        if (!provider)
            return;

        if (!s_Providers)
        {
            s_Providers = new TManagedRefArray;
        }

        s_Providers.Insert(provider);

        string msg = "[LFPG_Balance] Provider registered: ";
        string pName = provider.GetName();
        int pPrio = provider.GetPriority();
        msg = msg + pName;
        msg = msg + " (priority ";
        msg = msg + pPrio.ToString();
        msg = msg + ")";
        LFPG_Util.Info(msg);
    }

    // Returns the active provider (may be null if none resolved)
    static LFPG_BalanceProvider GetActive()
    {
        return s_Active;
    }

    // The active provider passed the installation capability check at Init.
    static bool IsAvailable()
    {
        if (s_Active)
            return true;

        return false;
    }

    // --- Internal helpers ---

    protected static LFPG_BalanceProvider FindByName(string name)
    {
        if (!s_Providers)
            return null;

        int i = 0;
        int count = s_Providers.Count();
        for (i = 0; i < count; i = i + 1)
        {
            LFPG_BalanceProvider prov = LFPG_BalanceProvider.Cast(s_Providers[i]);
            if (!prov)
                continue;

            string provName = prov.GetName();
            if (provName == name && prov.IsSupported())
                return prov;
        }
        return null;
    }

    protected static LFPG_BalanceProvider FindHighestPriority()
    {
        if (!s_Providers)
            return null;

        int count = s_Providers.Count();
        if (count == 0)
            return null;

        LFPG_BalanceProvider best = null;
        bool skippedUnsupported = false;
        int bestPrio = -1;
        int i = 0;
        for (i = 0; i < count; i = i + 1)
        {
            LFPG_BalanceProvider prov = LFPG_BalanceProvider.Cast(s_Providers[i]);
            if (!prov)
                continue;

            if (!prov.IsSupported())
            {
                skippedUnsupported = true;
                LFPG_Util.Warn("[LFPG_Balance] Mode=auto: excluding " + prov.GetName() + "; banking API unavailable. Core alone does not provide LBmaster banking.");
                continue;
            }

            int prio = prov.GetPriority();
            if (prio > bestPrio)
            {
                bestPrio = prio;
                best = prov;
            }
        }
        if (skippedUnsupported)
        {
            // A supported fallback may hold unrelated or previously migrated balances.
            LFPG_Util.Warn("[LFPG_Balance] Mode=auto: no eligible banking provider remains after exclusion; fallback to another wallet intentionally disabled. Account operations disabled. If this server previously used LBmaster banking, restore that API; changing balanceMode does not recover or migrate balances, and using another wallet would leave the existing funds in the original bank. If this server never used LBmaster banking and Core was installed only as a dependency, set balanceMode to 'native' to make the ATM operational with the native wallet. The administrator must choose based on the server's history; startup cannot distinguish these cases and therefore does not switch wallets automatically.");
            return null;
        }
        return best;
    }
};
