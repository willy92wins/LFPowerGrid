// =========================================================
// LF_PowerGrid - bootstrap breadcrumbs
// Small, safe prints so you can verify scripts are actually loaded.
// =========================================================

class LFPG_Bootstrap
{
    static bool s_Logged;

    static void LogOnce(string msg)
    {
        if (s_Logged) return;
        s_Logged = true;
        Print(LFPG_LOG_PREFIX + msg);
    }
};
