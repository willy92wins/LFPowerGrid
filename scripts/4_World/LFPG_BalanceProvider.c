// =========================================================
// LF_PowerGrid — Balance Provider System
//
// Abstract base class for player balance providers + registry
// with priority-based auto-selection.
//
// Providers register at startup with a name and priority.
// The registry resolves the active provider based on the
// balanceMode setting in LF_BTCAtm.json:
//   "auto"     → highest priority registered provider
//   "native"   → force LFPG native (always available)
//   "lbmaster" → force LBmaster (disabled if mod absent)
//
// Future providers (Expansion, Trader+, etc.) just need a
// new class extending LFPG_BalanceProvider and calling
// LFPG_BalanceRegistry.Register() with their priority.
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
    protected static ref array<ref LFPG_BalanceProvider> s_Providers;
    protected static ref LFPG_BalanceProvider s_Active;
    protected static bool s_Initialized;

    static void Init(string balanceMode)
    {
        if (!s_Providers)
        {
            s_Providers = new array<ref LFPG_BalanceProvider>;
        }

        // Log all registered providers
        int i = 0;
        int count = s_Providers.Count();
        string regMsg = "[LFPG_Balance] Registered: ";
        for (i = 0; i < count; i = i + 1)
        {
            LFPG_BalanceProvider prov = s_Providers[i];
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
                string errLB = "[LFPG_Balance] Mode=lbmaster but LBmaster provider not registered! ATM will be disabled.";
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
            noMsg = noMsg + " -> No active provider! BTC ATM balance operations will fail.";
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
            s_Providers = new array<ref LFPG_BalanceProvider>;
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

    // Convenience: is there a working provider?
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
            LFPG_BalanceProvider prov = s_Providers[i];
            if (!prov)
                continue;

            string provName = prov.GetName();
            if (provName == name)
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
        int bestPrio = -1;
        int i = 0;
        for (i = 0; i < count; i = i + 1)
        {
            LFPG_BalanceProvider prov = s_Providers[i];
            if (!prov)
                continue;

            int prio = prov.GetPriority();
            if (prio > bestPrio)
            {
                bestPrio = prio;
                best = prov;
            }
        }
        return best;
    }
};
