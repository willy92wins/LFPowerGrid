// =========================================================
// LF_PowerGrid - InventoryMenu Mod (v5.0)
//
// Subscribes to LFPG_CargoRefreshSignal when inventory is open.
// On signal: calls VicinityItemManager.RefreshVicinityItems()
// so nearby container cargo grids update after sort operations.
//
// Lifecycle:
//   OnShow → subscribe (Remove first for safety, then Insert)
//   OnHide → unsubscribe (Remove)
//   Zero overhead when inventory is closed.
//   No stale references after mission restart.
// =========================================================

modded class InventoryMenu
{
    override void OnShow()
    {
        super.OnShow();

        ScriptInvoker invoker = LFPG_CargoRefreshSignal.GetInvoker();
        invoker.Remove(LFPG_OnCargoRefresh);
        invoker.Insert(LFPG_OnCargoRefresh);
    }

    override void OnHide()
    {
        ScriptInvoker invoker = LFPG_CargoRefreshSignal.GetInvoker();
        invoker.Remove(LFPG_OnCargoRefresh);

        super.OnHide();
    }

    void LFPG_OnCargoRefresh()
    {
        VicinityItemManager vicMgr = VicinityItemManager.GetInstance();
        if (vicMgr)
        {
            vicMgr.RefreshVicinityItems();
        }
    }
};
