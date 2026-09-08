// =========================================================
// LF_PowerGrid - ATM stock forwarder (World arena)
//
// The native balance implementation owns the ATM stock timeline and lives in
// the mission arena. These forwarders are the only thing the World arena keeps.
//
// Deliberately NOT routed through LFPG_BalanceRegistry.GetActive(): that answers
// "who holds the player's EUR", which on an LBmaster server is not the native
// provider, while the stock timeline is always the native one.
//
// Both gates deny when the mission seam is unavailable, so a missing mission
// blocks stock movement rather than allowing an unchecked one.
// =========================================================

class LFPG_AtmStock
{
    static bool CanPrepareStockMutation(string deviceId, int stockBefore, int stockTarget)
    {
		if (deviceId == "" || LFPG_DeviceRegistry.Get().IsAmbiguous(deviceId))
			return false;
        MissionBaseWorld mw = MissionBaseWorld.Cast(g_Game.GetMission());
        if (!mw)
            return false;
        return mw.LFPG_AtmCanPrepareStockMutation(deviceId, stockBefore, stockTarget);
    }

    static bool PrepareStockMutation(string deviceId, int stockBefore, int stockTarget)
    {
		if (deviceId == "" || LFPG_DeviceRegistry.Get().IsAmbiguous(deviceId))
			return false;
        MissionBaseWorld mw = MissionBaseWorld.Cast(g_Game.GetMission());
        if (!mw)
            return false;
        return mw.LFPG_AtmPrepareStockMutation(deviceId, stockBefore, stockTarget);
    }

    // ATMs whose AfterStoreLoad landed before the mission was reachable.
    // Non-owning references: a deleted ATM leaves a null the drain skips.
    protected static ref array<LFPG_BTCAtmBase> s_PendingReconcile;
	protected static bool s_ReconcileReady;
	protected static MissionBaseWorld s_ReadyMission;

    static void ReconcileLoadedAtm(LFPG_BTCAtmBase atm)
    {
        if (!atm)
            return;
        MissionBaseWorld mw = MissionBaseWorld.Cast(g_Game.GetMission());
		if (mw && s_ReconcileReady && s_ReadyMission == mw)
        {
			string readyDeviceId = atm.LFPG_GetDeviceId();
			if (readyDeviceId == "" || LFPG_DeviceRegistry.Get().IsAmbiguous(readyDeviceId))
				return;
            mw.LFPG_AtmReconcileLoaded(atm);
            return;
        }
        if (!s_PendingReconcile)
            s_PendingReconcile = new array<LFPG_BTCAtmBase>;
		if (s_PendingReconcile.Find(atm) < 0)
			s_PendingReconcile.Insert(atm);
    }

    // Called by MissionServer right after super.OnInit(). Empty in the normal
    // case; only fills if the engine restores an ATM before the mission is
    // reachable, which is the ordering this net exists to survive.
    static void DrainPendingReconcile(MissionBaseWorld mw)
    {
		if (!mw)
			return;
		s_ReadyMission = mw;
		s_ReconcileReady = true;
		if (!s_PendingReconcile)
            return;
        int pending = s_PendingReconcile.Count();
        if (pending > 0)
            LFPG_Util.Info("[LFPG_Balance] draining " + pending.ToString() + " ATM(s) reconciled late");
        int i = 0;
        for (i = 0; i < pending; i = i + 1)
        {
            LFPG_BTCAtmBase queued = s_PendingReconcile[i];
            if (queued)
			{
				string queuedDeviceId = queued.LFPG_GetDeviceId();
				if (queuedDeviceId != "" && !LFPG_DeviceRegistry.Get().IsAmbiguous(queuedDeviceId))
					mw.LFPG_AtmReconcileLoaded(queued);
			}
        }
        s_PendingReconcile.Clear();
    }
};
