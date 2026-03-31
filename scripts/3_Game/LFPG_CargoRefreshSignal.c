// =========================================================
// LF_PowerGrid - Cargo Refresh Signal (v5.0)
//
// Cross-module event bridge for inventory UI refresh.
// 4_World (PlayerRPC) calls Request() after sort operations.
// 5_Mission (modded InventoryMenu) subscribes via GetInvoker().
//
// Pattern: static ScriptInvoker — zero polling, event-driven.
// Layer: 3_Game (visible from both 4_World and 5_Mission).
// =========================================================

class LFPG_CargoRefreshSignal
{
    protected static ref ScriptInvoker s_OnRefresh;

    static ScriptInvoker GetInvoker()
    {
        if (!s_OnRefresh)
        {
            s_OnRefresh = new ScriptInvoker;
        }
        return s_OnRefresh;
    }

    static void Request()
    {
        if (s_OnRefresh)
        {
            s_OnRefresh.Invoke();
        }
    }
};
