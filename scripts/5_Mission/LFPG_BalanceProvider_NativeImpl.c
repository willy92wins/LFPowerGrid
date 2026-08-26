// =========================================================
// LF_PowerGrid — Native Balance Provider
//
// Standalone player balance system. No external mod dependency.
// Persists balances to $profile:LF_PowerGrid/LF_Balances.json.
// Save triggered on every transaction for data safety.
//
// In-memory: map<string, int> for O(1) lookup by UID.
// On-disk: array of LFPG_BalanceEntry (JSON-serializable).
//
// PR-A: LFPG_BalanceEntry + LFPG_BalanceData live in
// scripts/3_Game/LFPG_Data.c so LFPG_FileUtil can reference them
// for AtomicSaveBalances + typed .tmp recovery.
// =========================================================

// ---- Compound rollback snapshot ----
class LFPG_BalanceCompoundSnapshot
{
    string m_UID;
    bool m_Existed;
    int m_Value;
};

// ---- Native provider implementation ----
class LFPG_BalanceProvider_NativeImpl extends LFPG_BalanceProvider_Native
{
    protected static ref map<string, int> s_Balances;
    protected static bool s_Loaded;
    protected static ref array<ref LFPG_BalanceClaim> s_Claims;
    protected static ref map<string, bool> s_ReconciledDevices;
    protected static ref map<string, bool> s_ReappliedThisBoot;
    protected static ref map<string, bool> s_OrphanObservedThisBoot;
    protected static ref map<string, bool> s_AmbiguousObservedThisBoot;
    protected static ref map<string, int> s_ClaimErrorWindowStartMs;
    protected static ref map<string, int> s_ClaimErrorCounts;
    protected static bool s_FutureVersionReadOnly;

    // PR-A: corrupt-load latch. Set true if LoadFromDisk detects unparseable
    // file; blocks subsequent SaveToDisk so the corrupt file is preserved as
    // .corrupt.<ts>_<rnd> evidence (not silently overwritten with empty map).
    // Process-lifetime only: auto-clears on next server restart.
    protected static bool s_DiskInhibited;

    // PR-A.5: rate-limit the inhibited-save warn so an active server with N
    // players doesn't spam RPT (each AddBalance/RemoveBalance hits SaveToDisk).
    // Log the first attempt, then summarize every 60s with cumulative count.
    protected static int s_InhibitedSaveCount;
    protected static float s_LastInhibitedWarnTime;

    // Save coalescing is scoped to one synchronous compound balance action.
    protected static int s_CompoundActionDepth;
    protected static bool s_CompoundActionDirty;
    protected static bool s_CompoundDirtyBefore;
    protected static ref array<ref LFPG_BalanceCompoundSnapshot> s_CompoundPreState;

    void LFPG_BalanceProvider_NativeImpl()
    {
        m_Name = "Native";
        m_Priority = 0;
    }
    protected static const int LFPG_NATIVE_BALANCE_CAP = 2000000000;
    protected static const int LFPG_CLAIM_PENDING = 0;
    protected static const int LFPG_CLAIM_REFUNDED = 1;
    // One-shot delay leaves hive entities time to populate DeviceRegistry.
    static const int LFPG_BTC_ORPHAN_SWEEP_DELAY_MS = 120000;
    // Bounded purchases: at most eight pending account purchases per device.
    static const int LFPG_BTC_MAX_CLAIMS_PER_DEVICE = 8;
    // Wire-reachable claim errors: first three per UID/device in each minute.
    protected static const int LFPG_BTC_CLAIM_ERROR_WINDOW_MS = 60000;
    protected static const int LFPG_BTC_CLAIM_ERROR_MAX_PER_WINDOW = 3;

    static int GetBalanceCap()
    {
        return LFPG_NATIVE_BALANCE_CAP;
    }

    static bool IsClaimStoreWritable()
    {
        return !s_FutureVersionReadOnly && !s_DiskInhibited;
    }
    protected static void EnsureClaimState()
    {
        if (!s_Claims)
            s_Claims = new array<ref LFPG_BalanceClaim>;
        if (!s_ReconciledDevices)
            s_ReconciledDevices = new map<string, bool>;
        if (!s_ReappliedThisBoot)
            s_ReappliedThisBoot = new map<string, bool>;
        if (!s_OrphanObservedThisBoot)
            s_OrphanObservedThisBoot = new map<string, bool>;
        if (!s_AmbiguousObservedThisBoot)
            s_AmbiguousObservedThisBoot = new map<string, bool>;
        if (!s_ClaimErrorWindowStartMs)
            s_ClaimErrorWindowStartMs = new map<string, int>;
        if (!s_ClaimErrorCounts)
            s_ClaimErrorCounts = new map<string, int>;
    }

    protected static bool AllowClaimErrorLog(string uid, string deviceId)
    {
        EnsureClaimState();
        string key = "uid:" + uid + "|device:" + deviceId;
        if (uid == "")
            key = "device:" + deviceId;
        if (key == "device:")
            key = "unknown";
        if (!g_Game)
            return true;

        int nowMs = g_Game.GetTime();
        int windowStart = 0;
        int count = 0;
        bool resetWindow = true;
        if (s_ClaimErrorWindowStartMs.Contains(key))
        {
            windowStart = s_ClaimErrorWindowStartMs.Get(key);
            resetWindow = (nowMs < windowStart) || ((nowMs - windowStart) >= LFPG_BTC_CLAIM_ERROR_WINDOW_MS);
        }
        if (resetWindow)
        {
            s_ClaimErrorWindowStartMs.Set(key, nowMs);
            s_ClaimErrorCounts.Set(key, 0);
        }
        else if (s_ClaimErrorCounts.Contains(key))
        {
            count = s_ClaimErrorCounts.Get(key);
        }

        if (count >= LFPG_BTC_CLAIM_ERROR_MAX_PER_WINDOW)
            return false;
        s_ClaimErrorCounts.Set(key, count + 1);
        return true;
    }

    protected static string FindDeviceClaimUID(string deviceId)
    {
        EnsureClaimState();
        int i = 0;
        for (i = 0; i < s_Claims.Count(); i = i + 1)
        {
            LFPG_BalanceClaim claim = s_Claims[i];
            if (claim && claim.deviceId == deviceId && claim.debit > 0 && claim.uid != "")
                return claim.uid;
        }
        return "";
    }

    protected static void LogClaimError(string message, string uid, string deviceId)
    {
        if (AllowClaimErrorLog(uid, deviceId))
            LFPG_Util.Error(message);
    }

    protected static bool HasDeviceClaims(string deviceId)
    {
        EnsureClaimState();
        int i = 0;
        for (i = 0; i < s_Claims.Count(); i = i + 1)
        {
            LFPG_BalanceClaim claim = s_Claims[i];
            if (claim && claim.deviceId == deviceId)
                return true;
        }
        return false;
    }

    protected static bool TryGetLastDeviceTarget(string deviceId, out int target)
    {
        target = 0;
        EnsureClaimState();
        bool found = false;
        int i = 0;
        for (i = 0; i < s_Claims.Count(); i = i + 1)
        {
            LFPG_BalanceClaim claim = s_Claims[i];
            if (!claim || claim.deviceId != deviceId || claim.state != LFPG_CLAIM_PENDING)
                continue;
            if (claim.debit < 0)
                return false;
            if (claim.debit > 0 && claim.uid == "")
                return false;
            if (claim.debit == 0 && claim.uid != "")
                return false;
            target = claim.stockTarget;
            found = true;
        }
        return found;
    }

    protected static int FindLastPendingDeviceClaimIndex(string deviceId)
    {
        EnsureClaimState();
        int result = -1;
        int i = 0;
        for (i = 0; i < s_Claims.Count(); i = i + 1)
        {
            LFPG_BalanceClaim claim = s_Claims[i];
            if (claim && claim.deviceId == deviceId && claim.state == LFPG_CLAIM_PENDING)
                result = i;
        }
        return result;
    }
    protected static int CountPendingPurchaseClaims(string deviceId)
    {
        EnsureClaimState();
        int count = 0;
        int i = 0;
        for (i = 0; i < s_Claims.Count(); i = i + 1)
        {
            LFPG_BalanceClaim claim = s_Claims[i];
            if (claim && claim.deviceId == deviceId && claim.debit > 0 && claim.state == LFPG_CLAIM_PENDING)
                count = count + 1;
        }
        return count;
    }

    protected static array<ref LFPG_BalanceClaim> CollectDeviceClaims(string deviceId)
    {
        array<ref LFPG_BalanceClaim> result = new array<ref LFPG_BalanceClaim>;
        EnsureClaimState();
        int i = 0;
        for (i = 0; i < s_Claims.Count(); i = i + 1)
        {
            LFPG_BalanceClaim claim = s_Claims[i];
            if (claim && claim.deviceId == deviceId)
                result.Insert(claim);
        }
        return result;
    }

    protected static array<ref LFPG_BalanceClaim> BuildClaimsWithoutIndex(int excludedIndex)
    {
        array<ref LFPG_BalanceClaim> result = new array<ref LFPG_BalanceClaim>;
        int i = 0;
        for (i = 0; i < s_Claims.Count(); i = i + 1)
        {
            if (i == excludedIndex)
                continue;
            result.Insert(s_Claims[i]);
        }
        return result;
    }

    // Enforce arrays are managed reference objects. Rollback needs a distinct
    // container, so copy every element reference instead of aliasing s_Claims.
    protected static array<ref LFPG_BalanceClaim> CopyClaimReferences()
    {
        array<ref LFPG_BalanceClaim> result = new array<ref LFPG_BalanceClaim>;
        int i = 0;
        for (i = 0; i < s_Claims.Count(); i = i + 1)
            result.Insert(s_Claims[i]);
        return result;
    }

    protected static array<ref LFPG_BalanceClaim> RemoveDeviceClaimPrefix(string deviceId, int removeCount)
    {
        array<ref LFPG_BalanceClaim> removed = new array<ref LFPG_BalanceClaim>;
        array<ref LFPG_BalanceClaim> retained = new array<ref LFPG_BalanceClaim>;
        int i = 0;
        for (i = 0; i < s_Claims.Count(); i = i + 1)
        {
            LFPG_BalanceClaim claim = s_Claims[i];
            if (claim && claim.deviceId == deviceId && removed.Count() < removeCount)
                removed.Insert(claim);
            else
                retained.Insert(claim);
        }
        if (removed.Count() > 0)
        {
            s_Claims = retained;
            s_CompoundActionDirty = true;
        }
        return removed;
    }

    protected static bool PersistRemoveDeviceClaimPrefix(string deviceId, int removeCount)
    {
        if (removeCount <= 0)
            return true;
        array<ref LFPG_BalanceClaim> previousClaims = CopyClaimReferences();
        bool dirtyBefore = s_CompoundActionDirty;
        array<ref LFPG_BalanceClaim> removed = RemoveDeviceClaimPrefix(deviceId, removeCount);
        if (removed.Count() != removeCount)
        {
            s_Claims = previousClaims;
            s_CompoundActionDirty = dirtyBefore;
            LogClaimError("[LFPG_Balance_Native] Durable claim clear rejected: prefix cardinality changed deviceId=" + deviceId, FindDeviceClaimUID(deviceId), deviceId);
            return false;
        }
        if (!SaveToDisk())
        {
            s_Claims = previousClaims;
            s_CompoundActionDirty = dirtyBefore;
            LogClaimError("[LFPG_Balance_Native] Durable claim clear failed; in-memory records restored deviceId=" + deviceId, FindDeviceClaimUID(deviceId), deviceId);
            return false;
        }
        // Claim entry points require depth==0. Preserve a pre-existing dirty
        // bit instead of claiming ownership of an unrelated compound mutation.
        s_CompoundActionDirty = dirtyBefore;
        return true;
    }

    protected static bool PersistRemoveDeviceClaimIndices(string deviceId, array<int> removeIndices)
    {
        if (!removeIndices || removeIndices.Count() == 0)
            return true;

        int validateIndex = 0;
        int previousIndex = -1;
        for (validateIndex = 0; validateIndex < removeIndices.Count(); validateIndex = validateIndex + 1)
        {
            int claimIndex = removeIndices[validateIndex];
            if (claimIndex <= previousIndex || claimIndex < 0 || claimIndex >= s_Claims.Count())
                return false;
            LFPG_BalanceClaim validatedClaim = s_Claims[claimIndex];
            if (!validatedClaim || validatedClaim.deviceId != deviceId)
                return false;
            previousIndex = claimIndex;
        }

        array<ref LFPG_BalanceClaim> previousClaims = CopyClaimReferences();
        bool dirtyBefore = s_CompoundActionDirty;
        int removeIndex = removeIndices.Count() - 1;
        for (removeIndex = removeIndices.Count() - 1; removeIndex >= 0; removeIndex = removeIndex - 1)
            s_Claims.RemoveOrdered(removeIndices[removeIndex]);

        s_CompoundActionDirty = true;
        if (!SaveToDisk())
        {
            s_Claims = previousClaims;
            s_CompoundActionDirty = dirtyBefore;
            LogClaimError("[LFPG_Balance_Native] Durable selective claim clear failed; records restored deviceId=" + deviceId, FindDeviceClaimUID(deviceId), deviceId);
            return false;
        }
        s_CompoundActionDirty = dirtyBefore;
        return true;
    }
    protected static void LogAmbiguousChain(string deviceId, int stock, array<ref LFPG_BalanceClaim> chain, string reason)
    {
        string msg = "[LFPG_Balance_Native] Ambiguous ATM claim chain; stock mutation blocked deviceId=";
        msg = msg + deviceId;
        msg = msg + " stock=";
        msg = msg + stock.ToString();
        msg = msg + " reason=";
        msg = msg + reason;
        int i = 0;
        for (i = 0; i < chain.Count(); i = i + 1)
        {
            LFPG_BalanceClaim claim = chain[i];
            if (!claim)
                continue;
            msg = msg + " [";
            msg = msg + "uid=";
            msg = msg + claim.uid;
            msg = msg + " ";
            msg = msg + claim.stockBefore.ToString();
            msg = msg + "->";
            msg = msg + claim.stockTarget.ToString();
            msg = msg + " debit=";
            msg = msg + claim.debit.ToString();
            msg = msg + " state=";
            msg = msg + claim.state.ToString();
            msg = msg + " sessionLow=";
            msg = msg + claim.sessionLow.ToString();
            msg = msg + " sessionHigh=";
            msg = msg + claim.sessionHigh.ToString();
            msg = msg + " sequence=";
            msg = msg + claim.sequence.ToString();
            msg = msg + " orphanBoots=";
            msg = msg + claim.orphanBoots.ToString();
            msg = msg + " ambigBoots=";
            msg = msg + claim.ambigBoots.ToString();
            msg = msg + " refundBoots=";
            msg = msg + claim.bootsSinceRefund.ToString();
            msg = msg + "]";
        }
        LogClaimError(msg, FindDeviceClaimUID(deviceId), deviceId);
    }

    protected static bool PersistRemoveClaimAt(int claimIndex)
    {
        if (claimIndex < 0 || claimIndex >= s_Claims.Count())
            return false;
        array<ref LFPG_BalanceClaim> previousClaims = CopyClaimReferences();
        bool dirtyBefore = s_CompoundActionDirty;
        s_Claims = BuildClaimsWithoutIndex(claimIndex);
        if (!SaveToDisk())
        {
            s_Claims = previousClaims;
            s_CompoundActionDirty = dirtyBefore;
            return false;
        }
        s_CompoundActionDirty = dirtyBefore;
        return true;
    }

    static bool DebitWithStockClaim(PlayerBase player, string deviceId, int sessionLow, int sessionHigh, int sequence, int debit, int stockBefore, int stockTarget, out int debited)
    {
        debited = 0;
        if (!player || deviceId == "" || debit <= 0)
            return false;
        if (stockTarget <= stockBefore)
            return false;
        int maxStock = LFPG_BTCConfig.GetMaxBtcPerMachine();
        if (stockTarget > maxStock)
            return false;
        if (s_CompoundActionDepth != 0)
            return false;

        EnsureLoaded();
        EnsureClaimState();
        if (!IsClaimStoreWritable())
            return false;

        string uid = GetUID(player);
        if (uid == "")
            return false;

        int pendingPurchaseCount = CountPendingPurchaseClaims(deviceId);
        if (pendingPurchaseCount >= LFPG_BTC_MAX_CLAIMS_PER_DEVICE)
        {
            if (AllowClaimErrorLog(uid, deviceId))
                LFPG_Util.Error("[LFPG_Balance_Native] Account Buy claim denied: per-device pending claim cap reached uid=" + uid + " deviceId=" + deviceId);
            return false;
        }

        if (HasDeviceClaims(deviceId))
        {
            if (!s_ReconciledDevices.Contains(deviceId))
                return false;
            int pendingTipIndex = FindLastPendingDeviceClaimIndex(deviceId);
            if (pendingTipIndex >= 0)
            {
                int previousTarget = 0;
                if (!TryGetLastDeviceTarget(deviceId, previousTarget))
                    return false;
                if (previousTarget != stockBefore)
                    return false;
            }
        }

        int current = 0;
        bool hadBalance = s_Balances.Contains(uid);
        if (hadBalance)
            current = s_Balances.Get(uid);
        if (current < debit)
            return false;

        LFPG_BalanceClaim claim = new LFPG_BalanceClaim();
        claim.uid = uid;
        claim.deviceId = deviceId;
        claim.sessionLow = sessionLow;
        claim.sessionHigh = sessionHigh;
        claim.sequence = sequence;
        claim.debit = debit;
        claim.stockBefore = stockBefore;
        claim.stockTarget = stockTarget;
        claim.state = LFPG_CLAIM_PENDING;
        claim.bootsSinceRefund = 0;
        claim.orphanBoots = 0;
        claim.ambigBoots = 0;

        bool dirtyBefore = s_CompoundActionDirty;
        s_Balances.Set(uid, current - debit);
        s_Claims.Insert(claim);
        if (!SaveToDisk())
        {
            if (hadBalance)
                s_Balances.Set(uid, current);
            else
                s_Balances.Remove(uid);
            int insertedClaimIndex = s_Claims.Find(claim);
            if (insertedClaimIndex >= 0)
                s_Claims.RemoveOrdered(insertedClaimIndex);
            s_CompoundActionDirty = dirtyBefore;
            LogClaimError("[LFPG_Balance_Native] Debit+claim rollback: atomic balance snapshot was not durable", uid, deviceId);
            return false;
        }

        s_CompoundActionDirty = dirtyBefore;
        s_ReconciledDevices.Set(deviceId, true);
        s_ReappliedThisBoot.Remove(deviceId);
        debited = debit;
        return true;
    }
    static void MarkStockClaimApplyFailed(string deviceId)
    {
        EnsureClaimState();
        s_ReconciledDevices.Remove(deviceId);
        LogClaimError("[LFPG_Balance_Native] Durable claim exists but stock apply failed; ATM blocked until boot reconcile deviceId=" + deviceId, FindDeviceClaimUID(deviceId), deviceId);
    }

    static bool CanPrepareStockMutation(string deviceId, int stockBefore, int stockTarget)
    {
        if (stockBefore == stockTarget)
            return true;
        if (deviceId == "")
            return false;
        if (s_CompoundActionDepth != 0)
            return false;

        EnsureLoaded();
        EnsureClaimState();
        if (!IsClaimStoreWritable())
            return false;
        if (!HasDeviceClaims(deviceId))
            return true;
        if (!s_ReconciledDevices.Contains(deviceId))
            return false;
        if (FindLastPendingDeviceClaimIndex(deviceId) < 0)
            return true;

        int previousTarget = 0;
        if (!TryGetLastDeviceTarget(deviceId, previousTarget))
            return false;
        return previousTarget == stockBefore;
    }

    static bool PrepareStockMutation(string deviceId, int stockBefore, int stockTarget)
    {
        if (stockBefore == stockTarget)
            return true;
        if (deviceId == "")
            return false;
        if (s_CompoundActionDepth != 0)
            return false;

        EnsureLoaded();
        EnsureClaimState();
        if (!IsClaimStoreWritable())
            return false;
        if (!HasDeviceClaims(deviceId))
            return true;
        if (!s_ReconciledDevices.Contains(deviceId))
        {
            LogClaimError("[LFPG_Balance_Native] Stock mutation denied while claim chain is unresolved deviceId=" + deviceId, FindDeviceClaimUID(deviceId), deviceId);
            return false;
        }
        if (FindLastPendingDeviceClaimIndex(deviceId) < 0)
            return true;

        int previousTarget = 0;
        if (!TryGetLastDeviceTarget(deviceId, previousTarget))
            return false;
        if (previousTarget != stockBefore)
        {
            LogClaimError("[LFPG_Balance_Native] Stock mutation denied: timeline tip does not match live stock deviceId=" + deviceId, FindDeviceClaimUID(deviceId), deviceId);
            return false;
        }

        int tipIndex = FindLastPendingDeviceClaimIndex(deviceId);
        if (tipIndex < 0)
            return false;
        LFPG_BalanceClaim tip = s_Claims[tipIndex];
        if (!tip)
            return false;

        bool dirtyBefore = s_CompoundActionDirty;
        bool fusedPhysical = (tip.debit == 0);
        int previousTipTarget = 0;
        LFPG_BalanceClaim physical = tip;
        if (fusedPhysical)
        {
            previousTipTarget = tip.stockTarget;
            tip.stockTarget = stockTarget;
        }
        else
        {
            physical = new LFPG_BalanceClaim();
            physical.uid = "";
            physical.deviceId = deviceId;
            physical.debit = 0;
            physical.stockBefore = stockBefore;
            physical.stockTarget = stockTarget;
            physical.state = LFPG_CLAIM_PENDING;
            physical.bootsSinceRefund = 0;
            physical.orphanBoots = 0;
            physical.ambigBoots = 0;
            s_Claims.Insert(physical);
        }

        if (!SaveToDisk())
        {
            if (fusedPhysical)
            {
                tip.stockTarget = previousTipTarget;
            }
            else
            {
                int insertedPhysicalIndex = s_Claims.Find(physical);
                if (insertedPhysicalIndex >= 0)
                    s_Claims.RemoveOrdered(insertedPhysicalIndex);
            }
            s_CompoundActionDirty = dirtyBefore;
            LogClaimError("[LFPG_Balance_Native] Stock timeline segment denied: target was not durable deviceId=" + deviceId, FindDeviceClaimUID(deviceId), deviceId);
            return false;
        }
        s_CompoundActionDirty = dirtyBefore;
        return true;
    }
    protected static bool ResetDeviceOrphanBoots(string deviceId)
    {
        array<ref LFPG_BalanceClaim> changed = new array<ref LFPG_BalanceClaim>;
        array<int> previousOrphanValues = new array<int>;
        array<int> previousAmbigValues = new array<int>;
        bool dirtyBefore = s_CompoundActionDirty;
        int i = 0;
        for (i = 0; i < s_Claims.Count(); i = i + 1)
        {
            LFPG_BalanceClaim claim = s_Claims[i];
            if (claim && claim.deviceId == deviceId && (claim.orphanBoots > 0 || claim.ambigBoots > 0))
            {
                changed.Insert(claim);
                previousOrphanValues.Insert(claim.orphanBoots);
                previousAmbigValues.Insert(claim.ambigBoots);
                claim.orphanBoots = 0;
                claim.ambigBoots = 0;
            }
        }
        if (changed.Count() == 0)
            return true;

        if (!SaveToDisk())
        {
            for (i = 0; i < changed.Count(); i = i + 1)
            {
                changed[i].orphanBoots = previousOrphanValues[i];
                changed[i].ambigBoots = previousAmbigValues[i];
            }
            s_CompoundActionDirty = dirtyBefore;
            LogClaimError("[LFPG_Balance_Native] ATM observation counter reset was not durable; previous values restored deviceId=" + deviceId, FindDeviceClaimUID(deviceId), deviceId);
            return false;
        }
        s_CompoundActionDirty = dirtyBefore;
        return true;
    }

    protected static bool DeviceClaimRecordsValidForTimeline(array<ref LFPG_BalanceClaim> chain)
    {
        int i = 0;
        for (i = 0; i < chain.Count(); i = i + 1)
        {
            LFPG_BalanceClaim claim = chain[i];
            if (!claim)
                return false;
            if (claim.state != LFPG_CLAIM_PENDING && claim.state != LFPG_CLAIM_REFUNDED)
                return false;
            if (claim.debit < 0)
                return false;
            if (claim.state == LFPG_CLAIM_REFUNDED && claim.debit <= 0)
                return false;
            if (claim.debit > 0 && claim.uid == "")
                return false;
            if (claim.debit == 0 && claim.uid != "")
                return false;
        }
        return true;
    }

    protected static array<ref LFPG_BalanceClaim> CollectPendingTimeline(array<ref LFPG_BalanceClaim> chain)
    {
        array<ref LFPG_BalanceClaim> timeline = new array<ref LFPG_BalanceClaim>;
        int i = 0;
        for (i = 0; i < chain.Count(); i = i + 1)
        {
            LFPG_BalanceClaim claim = chain[i];
            if (claim && claim.state == LFPG_CLAIM_PENDING)
                timeline.Insert(claim);
        }
        return timeline;
    }

    protected static array<int> BuildClaimIndices(string deviceId, array<ref LFPG_BalanceClaim> selectedClaims)
    {
        array<int> result = new array<int>;
        if (!selectedClaims)
            return result;
        int i = 0;
        for (i = 0; i < s_Claims.Count(); i = i + 1)
        {
            LFPG_BalanceClaim claim = s_Claims[i];
            if (claim && claim.deviceId == deviceId && selectedClaims.Find(claim) >= 0)
                result.Insert(i);
        }
        return result;
    }

    protected static array<int> FindChainProvenPurchaseIndices(string deviceId)
    {
        array<int> result = new array<int>;
        int i = 0;
        for (i = 0; i < s_Claims.Count(); i = i + 1)
        {
            LFPG_BalanceClaim purchase = s_Claims[i];
            if (!purchase || purchase.deviceId != deviceId || purchase.state != LFPG_CLAIM_PENDING || purchase.debit <= 0)
                continue;

            bool physicalLater = false;
            int laterIndex = i + 1;
            for (laterIndex = i + 1; laterIndex < s_Claims.Count(); laterIndex = laterIndex + 1)
            {
                LFPG_BalanceClaim laterClaim = s_Claims[laterIndex];
                if (laterClaim && laterClaim.deviceId == deviceId && laterClaim.state == LFPG_CLAIM_PENDING && laterClaim.debit == 0 && laterClaim.uid == "")
                {
                    physicalLater = true;
                    break;
                }
            }
            if (physicalLater)
                result.Insert(i);
        }
        return result;
    }

    protected static bool PersistRemoveChainProvenPurchases(string deviceId, out bool removedAny)
    {
        removedAny = false;
        array<int> provenIndices = FindChainProvenPurchaseIndices(deviceId);
        if (provenIndices.Count() == 0)
            return true;
        if (!PersistRemoveDeviceClaimIndices(deviceId, provenIndices))
            return false;
        removedAny = true;
        LFPG_Util.Info("[LFPG_Balance_Native] Chain-proven purchases cleared without refund deviceId=" + deviceId);
        return true;
    }

    protected static bool PersistAmbiguousObservation(string deviceId, array<ref LFPG_BalanceClaim> timeline, array<int> previousValues, bool dirtyBefore)
    {
        if (SaveToDisk())
        {
            s_CompoundActionDirty = dirtyBefore;
            return true;
        }

        int i = 0;
        for (i = 0; i < timeline.Count(); i = i + 1)
            timeline[i].ambigBoots = previousValues[i];
        s_AmbiguousObservedThisBoot.Remove(deviceId);
        s_CompoundActionDirty = dirtyBefore;
        LogClaimError("[LFPG_Balance_Native] Ambiguous present-ATM observation was not durable deviceId=" + deviceId, FindDeviceClaimUID(deviceId), deviceId);
        return false;
    }

    protected static bool RefundAndClearAmbiguousClaims(string deviceId, array<ref LFPG_BalanceClaim> timeline, array<int> previousValues, bool dirtyBefore, map<string, int> refundByUid)
    {
        array<string> refundUids = new array<string>;
        array<int> previousBalances = new array<int>;
        array<int> existedFlags = new array<int>;
        int refundIndex = 0;
        for (refundIndex = 0; refundIndex < refundByUid.Count(); refundIndex = refundIndex + 1)
        {
            string refundUid = refundByUid.GetKey(refundIndex);
            int refundAmount = refundByUid.GetElement(refundIndex);
            int currentBalance = 0;
            bool hadBalance = s_Balances.Contains(refundUid);
            if (hadBalance)
                currentBalance = s_Balances.Get(refundUid);
            if (currentBalance < 0 || refundAmount <= 0 || refundAmount > LFPG_NATIVE_BALANCE_CAP - currentBalance)
            {
                LogClaimError("[LFPG_Balance_Native] Ambiguous multi-claim refund deferred: exact Native room unavailable uid=" + refundUid, refundUid, deviceId);
                PersistAmbiguousObservation(deviceId, timeline, previousValues, dirtyBefore);
                return false;
            }
            refundUids.Insert(refundUid);
            previousBalances.Insert(currentBalance);
            if (hadBalance)
                existedFlags.Insert(1);
            else
                existedFlags.Insert(0);
        }

        array<int> removeIndices = BuildClaimIndices(deviceId, timeline);
        if (removeIndices.Count() != timeline.Count())
        {
            LogClaimError("[LFPG_Balance_Native] Ambiguous multi-claim refund deferred: timeline cardinality changed deviceId=" + deviceId, FindDeviceClaimUID(deviceId), deviceId);
            PersistAmbiguousObservation(deviceId, timeline, previousValues, dirtyBefore);
            return false;
        }

        array<ref LFPG_BalanceClaim> previousClaims = CopyClaimReferences();
        for (refundIndex = 0; refundIndex < refundUids.Count(); refundIndex = refundIndex + 1)
        {
            string creditedUid = refundUids[refundIndex];
            int creditedAmount = refundByUid.Get(creditedUid);
            s_Balances.Set(creditedUid, previousBalances[refundIndex] + creditedAmount);
        }

        int removeIndex = removeIndices.Count() - 1;
        for (removeIndex = removeIndices.Count() - 1; removeIndex >= 0; removeIndex = removeIndex - 1)
            s_Claims.RemoveOrdered(removeIndices[removeIndex]);
        s_CompoundActionDirty = true;

        if (!SaveToDisk())
        {
            for (refundIndex = 0; refundIndex < refundUids.Count(); refundIndex = refundIndex + 1)
            {
                string rollbackUid = refundUids[refundIndex];
                if (existedFlags[refundIndex] == 1)
                    s_Balances.Set(rollbackUid, previousBalances[refundIndex]);
                else
                    s_Balances.Remove(rollbackUid);
            }
            int restoreIndex = 0;
            for (restoreIndex = 0; restoreIndex < timeline.Count(); restoreIndex = restoreIndex + 1)
                timeline[restoreIndex].ambigBoots = previousValues[restoreIndex];
            s_Claims = previousClaims;
            s_AmbiguousObservedThisBoot.Remove(deviceId);
            s_CompoundActionDirty = dirtyBefore;
            LogClaimError("[LFPG_Balance_Native] Ambiguous multi-claim refund+clear save failed; balances and claims restored deviceId=" + deviceId, FindDeviceClaimUID(deviceId), deviceId);
            return false;
        }

        s_CompoundActionDirty = dirtyBefore;
        string uidList = "";
        for (refundIndex = 0; refundIndex < refundUids.Count(); refundIndex = refundIndex + 1)
        {
            if (uidList != "")
                uidList = uidList + ",";
            uidList = uidList + refundUids[refundIndex];
        }
        if (uidList == "")
            uidList = "<none>";
        string refundMsg = "[LFPG_Balance_Native] Irreconcilable present ATM timeline refunded atomically; stock left untouched deviceId=";
        refundMsg = refundMsg + deviceId;
        refundMsg = refundMsg + " uids=";
        refundMsg = refundMsg + uidList;
        LFPG_Util.Warn(refundMsg);
        return true;
    }

    protected static bool ObserveAmbiguousPresentTimeline(string deviceId, array<ref LFPG_BalanceClaim> timeline)
    {
        if (s_AmbiguousObservedThisBoot.Contains(deviceId))
            return false;

        array<int> previousValues = new array<int>;
        bool refundReady = false;
        bool aggregationValid = true;
        map<string, int> refundByUid = new map<string, int>;
        int i = 0;
        for (i = 0; i < timeline.Count(); i = i + 1)
        {
            LFPG_BalanceClaim claim = timeline[i];
            previousValues.Insert(claim.ambigBoots);
            if (claim.ambigBoots < 2)
                claim.ambigBoots = claim.ambigBoots + 1;
            if (claim.ambigBoots >= 2)
                refundReady = true;

            if (claim.debit > 0)
            {
                int accumulated = 0;
                if (refundByUid.Contains(claim.uid))
                    accumulated = refundByUid.Get(claim.uid);
                if (claim.debit > LFPG_NATIVE_BALANCE_CAP - accumulated)
                    aggregationValid = false;
                else
                    refundByUid.Set(claim.uid, accumulated + claim.debit);
            }
        }
        s_AmbiguousObservedThisBoot.Set(deviceId, true);
        bool dirtyBefore = s_CompoundActionDirty;

        if (!refundReady)
        {
            PersistAmbiguousObservation(deviceId, timeline, previousValues, dirtyBefore);
            return false;
        }
        if (!aggregationValid)
        {
            LogClaimError("[LFPG_Balance_Native] Ambiguous multi-claim refund deferred: debit aggregation exceeds Native bounds deviceId=" + deviceId, FindDeviceClaimUID(deviceId), deviceId);
            PersistAmbiguousObservation(deviceId, timeline, previousValues, dirtyBefore);
            return false;
        }
        return RefundAndClearAmbiguousClaims(deviceId, timeline, previousValues, dirtyBefore, refundByUid);
    }

    protected static void HandlePendingNoMatch(string deviceId, int stock, array<ref LFPG_BalanceClaim> chain, string reason)
    {
        LogAmbiguousChain(deviceId, stock, chain, reason);

        bool removedProven = false;
        if (!PersistRemoveChainProvenPurchases(deviceId, removedProven))
            return;

        array<ref LFPG_BalanceClaim> currentChain = CollectDeviceClaims(deviceId);
        array<ref LFPG_BalanceClaim> remainingTimeline = CollectPendingTimeline(currentChain);
        if (remainingTimeline.Count() == 0)
        {
            s_ReconciledDevices.Set(deviceId, true);
            return;
        }

        if (ObserveAmbiguousPresentTimeline(deviceId, remainingTimeline))
            s_ReconciledDevices.Set(deviceId, true);
    }
    protected static void AdvancePresentRefundedClaims(string deviceId)
    {
        int claimIndex = 0;
        while (claimIndex < s_Claims.Count())
        {
            LFPG_BalanceClaim claim = s_Claims[claimIndex];
            if (!claim || claim.deviceId != deviceId || claim.state != LFPG_CLAIM_REFUNDED)
            {
                claimIndex = claimIndex + 1;
                continue;
            }
            bool pruned = AdvanceOrPruneRefundedClaimAt(claimIndex);
            if (pruned)
                continue;
            claimIndex = claimIndex + 1;
        }
    }

    protected static void ReconcilePendingChain(LFPG_BTCAtmBase atm, string deviceId, array<ref LFPG_BalanceClaim> chain)
    {
        int stock = atm.LFPG_GetBtcStock();
        if (!DeviceClaimRecordsValidForTimeline(chain))
        {
            LogAmbiguousChain(deviceId, stock, chain, "invalid claim record or state");
            return;
        }

        array<ref LFPG_BalanceClaim> timeline = CollectPendingTimeline(chain);
        if (timeline.Count() == 0)
        {
            LogAmbiguousChain(deviceId, stock, chain, "pending reconcile has no pending timeline records");
            return;
        }

        array<int> positions = new array<int>;
        bool pendingValidationFailed = false;
        bool discontinuity = false;
        int i = 0;
        for (i = 0; i < timeline.Count(); i = i + 1)
        {
            LFPG_BalanceClaim timelineClaim = timeline[i];
            if (timelineClaim.debit > 0)
            {
                if (timelineClaim.stockBefore < 0 || timelineClaim.stockTarget <= timelineClaim.stockBefore)
                    pendingValidationFailed = true;
            }
            else
            {
                if (timelineClaim.stockBefore < 0 || timelineClaim.stockTarget < 0 || timelineClaim.stockBefore == timelineClaim.stockTarget)
                    pendingValidationFailed = true;
            }

            if (i == 0)
                positions.Insert(timelineClaim.stockBefore);
            else if (timeline[i - 1].stockTarget != timelineClaim.stockBefore)
                discontinuity = true;
            positions.Insert(timelineClaim.stockTarget);
        }

        if (discontinuity)
        {
            string discontinuityMsg = "[LFPG_Balance_Native] Timeline discontinuity retained for legacy compatibility deviceId=";
            discontinuityMsg = discontinuityMsg + deviceId;
            LFPG_Util.Info(discontinuityMsg);
        }

        if (pendingValidationFailed)
        {
            HandlePendingNoMatch(deviceId, stock, chain, "pending timeline transition failed lenient validation");
            if (s_ReconciledDevices.Contains(deviceId))
            {
                ResetDeviceOrphanBoots(deviceId);
                AdvancePresentRefundedClaims(deviceId);
            }
            return;
        }

        int cursor = -1;
        for (i = 0; i < positions.Count(); i = i + 1)
        {
            if (positions[i] != stock)
                continue;
            if (cursor < 0)
            {
                cursor = i;
                continue;
            }

            bool cursorAdvanceProven = true;
            int purchaseIndex = cursor;
            for (purchaseIndex = cursor; purchaseIndex < i; purchaseIndex = purchaseIndex + 1)
            {
                LFPG_BalanceClaim candidatePurchase = timeline[purchaseIndex];
                if (candidatePurchase.debit <= 0)
                    continue;

                bool physicalLaterInCandidatePrefix = false;
                int physicalIndex = purchaseIndex + 1;
                for (physicalIndex = purchaseIndex + 1; physicalIndex < i; physicalIndex = physicalIndex + 1)
                {
                    LFPG_BalanceClaim candidatePhysical = timeline[physicalIndex];
                    if (candidatePhysical.debit == 0 && candidatePhysical.uid == "")
                    {
                        physicalLaterInCandidatePrefix = true;
                        break;
                    }
                }
                if (!physicalLaterInCandidatePrefix)
                {
                    cursorAdvanceProven = false;
                    break;
                }
            }
            if (!cursorAdvanceProven)
                continue;
            cursor = i;
        }
        if (cursor < 0)
        {
            HandlePendingNoMatch(deviceId, stock, chain, "loaded stock matches no timeline position");
            if (s_ReconciledDevices.Contains(deviceId))
            {
                ResetDeviceOrphanBoots(deviceId);
                AdvancePresentRefundedClaims(deviceId);
            }
            return;
        }

        int prefixCount = cursor;
        if (prefixCount > timeline.Count())
        {
            LogAmbiguousChain(deviceId, stock, chain, "timeline cursor exceeded record count");
            return;
        }

        if (prefixCount > 0)
        {
            bool prefixCleared = false;
            if (chain.Count() == timeline.Count())
            {
                prefixCleared = PersistRemoveDeviceClaimPrefix(deviceId, prefixCount);
            }
            else
            {
                array<ref LFPG_BalanceClaim> prefixClaims = new array<ref LFPG_BalanceClaim>;
                for (i = 0; i < prefixCount; i = i + 1)
                    prefixClaims.Insert(timeline[i]);
                array<int> prefixIndices = BuildClaimIndices(deviceId, prefixClaims);
                if (prefixIndices.Count() == prefixCount)
                    prefixCleared = PersistRemoveDeviceClaimIndices(deviceId, prefixIndices);
            }
            if (!prefixCleared)
            {
                LogClaimError("[LFPG_Balance_Native] Timeline cursor prefix was not cleared durably deviceId=" + deviceId, FindDeviceClaimUID(deviceId), deviceId);
                return;
            }
        }

        int running = stock;
        bool hadReapply = false;
        bool purchaseRebaseDirty = false;
        array<ref LFPG_BalanceClaim> physicalTail = new array<ref LFPG_BalanceClaim>;
        array<ref LFPG_BalanceClaim> rebasedPurchases = new array<ref LFPG_BalanceClaim>;
        array<int> previousPurchaseBefore = new array<int>;
        array<int> previousPurchaseTarget = new array<int>;

        for (i = prefixCount; i < timeline.Count(); i = i + 1)
        {
            LFPG_BalanceClaim tailClaim = timeline[i];
            if (tailClaim.debit == 0)
            {
                physicalTail.Insert(tailClaim);
                continue;
            }

            int purchaseDelta = tailClaim.stockTarget - tailClaim.stockBefore;
            if (running < 0 || purchaseDelta <= 0 || purchaseDelta > LFPG_NATIVE_BALANCE_CAP - running)
            {
                int restoreBoundsIndex = 0;
                for (restoreBoundsIndex = 0; restoreBoundsIndex < rebasedPurchases.Count(); restoreBoundsIndex = restoreBoundsIndex + 1)
                {
                    rebasedPurchases[restoreBoundsIndex].stockBefore = previousPurchaseBefore[restoreBoundsIndex];
                    rebasedPurchases[restoreBoundsIndex].stockTarget = previousPurchaseTarget[restoreBoundsIndex];
                }
                LogAmbiguousChain(deviceId, stock, chain, "purchase reapply arithmetic exceeded Native integer bounds");
                return;
            }

            rebasedPurchases.Insert(tailClaim);
            previousPurchaseBefore.Insert(tailClaim.stockBefore);
            previousPurchaseTarget.Insert(tailClaim.stockTarget);
            if (tailClaim.stockBefore != running || tailClaim.stockTarget != running + purchaseDelta)
                purchaseRebaseDirty = true;
            tailClaim.stockBefore = running;
            running = running + purchaseDelta;
            tailClaim.stockTarget = running;
        }

        if (physicalTail.Count() > 0)
        {
            array<int> physicalIndices = BuildClaimIndices(deviceId, physicalTail);
            if (physicalIndices.Count() != physicalTail.Count() || !PersistRemoveDeviceClaimIndices(deviceId, physicalIndices))
            {
                int restoreResyncIndex = 0;
                for (restoreResyncIndex = 0; restoreResyncIndex < rebasedPurchases.Count(); restoreResyncIndex = restoreResyncIndex + 1)
                {
                    rebasedPurchases[restoreResyncIndex].stockBefore = previousPurchaseBefore[restoreResyncIndex];
                    rebasedPurchases[restoreResyncIndex].stockTarget = previousPurchaseTarget[restoreResyncIndex];
                }
                LogClaimError("[LFPG_Balance_Native] Physical-tail resync clear failed; purchases remain pending and device unresolved deviceId=" + deviceId, FindDeviceClaimUID(deviceId), deviceId);
                return;
            }

            int physicalWarnIndex = 0;
            for (physicalWarnIndex = 0; physicalWarnIndex < physicalTail.Count(); physicalWarnIndex = physicalWarnIndex + 1)
            {
                LFPG_BalanceClaim abandonedPhysical = physicalTail[physicalWarnIndex];
                int abandonedDelta = abandonedPhysical.stockTarget - abandonedPhysical.stockBefore;
                string physicalWarn = "[LFPG_Balance_Native] Physical timeline segment resync-cleared; delta abandoned deviceId=";
                physicalWarn = physicalWarn + deviceId;
                physicalWarn = physicalWarn + " delta=";
                physicalWarn = physicalWarn + abandonedDelta.ToString();
                LFPG_Util.Warn(physicalWarn);
            }
        }

        if (physicalTail.Count() == 0 && purchaseRebaseDirty)
        {
            bool rebaseDirtyBefore = s_CompoundActionDirty;
            s_CompoundActionDirty = true;
            if (!SaveToDisk())
            {
                int restoreRebaseIndex = 0;
                for (restoreRebaseIndex = 0; restoreRebaseIndex < rebasedPurchases.Count(); restoreRebaseIndex = restoreRebaseIndex + 1)
                {
                    rebasedPurchases[restoreRebaseIndex].stockBefore = previousPurchaseBefore[restoreRebaseIndex];
                    rebasedPurchases[restoreRebaseIndex].stockTarget = previousPurchaseTarget[restoreRebaseIndex];
                }
                s_CompoundActionDirty = rebaseDirtyBefore;
                LogClaimError("[LFPG_Balance_Native] Rebased purchase timeline was not durable; claims remain pending and device unresolved deviceId=" + deviceId, FindDeviceClaimUID(deviceId), deviceId);
                return;
            }
            s_CompoundActionDirty = rebaseDirtyBefore;
        }

        int applyPurchaseIndex = 0;
        for (applyPurchaseIndex = 0; applyPurchaseIndex < rebasedPurchases.Count(); applyPurchaseIndex = applyPurchaseIndex + 1)
        {
            int durableTarget = rebasedPurchases[applyPurchaseIndex].stockTarget;
            if (!atm.LFPG_ApplyClaimedStockTarget(durableTarget))
            {
                // The rebased JSON is already durable. Keep that coherent
                // timeline so the next boot re-walks from persisted live stock.
                LogAmbiguousChain(deviceId, atm.LFPG_GetBtcStock(), chain, "post-persist purchase reapply failed; durable timeline retained");
                return;
            }
            hadReapply = true;
        }

        ResetDeviceOrphanBoots(deviceId);
        array<ref LFPG_BalanceClaim> survivingChain = CollectDeviceClaims(deviceId);
        array<ref LFPG_BalanceClaim> survivingTimeline = CollectPendingTimeline(survivingChain);
        if (survivingTimeline.Count() > 0)
        {
            int survivingTip = 0;
            if (!TryGetLastDeviceTarget(deviceId, survivingTip) || survivingTip != atm.LFPG_GetBtcStock())
            {
                LogAmbiguousChain(deviceId, atm.LFPG_GetBtcStock(), survivingChain, "post-reconcile timeline tip does not match live stock");
                return;
            }
        }

        s_ReconciledDevices.Set(deviceId, true);
        if (hadReapply)
        {
            s_ReappliedThisBoot.Set(deviceId, true);
            LFPG_Util.Warn("[LFPG_Balance_Native] Reapplied purchase claims; claims remain PENDING until a future boot cursor proves delivery deviceId=" + deviceId);
        }
        else if (prefixCount == timeline.Count())
        {
            LFPG_Util.Info("[LFPG_Balance_Native] Timeline cursor proved and cleared the complete pending chain deviceId=" + deviceId);
        }
        AdvancePresentRefundedClaims(deviceId);
    }
    protected static void ReconcileRefundedChain(LFPG_BTCAtmBase atm, string deviceId, array<ref LFPG_BalanceClaim> chain)
    {
        int stock = atm.LFPG_GetBtcStock();
        int count = chain.Count();
        array<ref LFPG_BalanceClaim> refundedPurchases = new array<ref LFPG_BalanceClaim>;
        int i = 0;
        for (i = 0; i < count; i = i + 1)
        {
            LFPG_BalanceClaim claim = chain[i];
            if (!claim || claim.debit < 0)
            {
                LogAmbiguousChain(deviceId, stock, chain, "invalid refunded claim record");
                return;
            }
            if (claim.debit > 0)
            {
                if (claim.state != LFPG_CLAIM_REFUNDED)
                {
                    LogAmbiguousChain(deviceId, stock, chain, "purchase is not refunded in refunded reconcile");
                    return;
                }
                refundedPurchases.Insert(claim);
            }
        }
        if (refundedPurchases.Count() == 0)
        {
            LogAmbiguousChain(deviceId, stock, chain, "refunded reconcile has no purchase tombstone");
            return;
        }

        LFPG_BalanceClaim first = refundedPurchases[0];
        int restoredStock = stock;

        if (stock != first.stockBefore)
        {
            for (i = refundedPurchases.Count() - 1; i >= 0; i = i - 1)
            {
                LFPG_BalanceClaim refundedClaim = refundedPurchases[i];
                if (restoredStock == refundedClaim.stockTarget)
                    restoredStock = refundedClaim.stockBefore;
                else if (restoredStock != refundedClaim.stockBefore)
                {
                    LogAmbiguousChain(deviceId, stock, chain, "refunded purchase does not match loaded stock");
                    return;
                }
            }
        }

        if (restoredStock != stock)
        {
            if (!atm.LFPG_ApplyClaimedStockTarget(restoredStock))
            {
                LogAmbiguousChain(deviceId, stock, chain, "late refunded-stock revert failed");
                return;
            }
        }

        if (PersistRemoveDeviceClaimPrefix(deviceId, count))
        {
            s_ReconciledDevices.Set(deviceId, true);
            LFPG_Util.Warn("[LFPG_Balance_Native] Late ATM matched refunded tombstone; stock compensation applied and tombstones cleared durably deviceId=" + deviceId);
        }
    }

    static void ReconcileLoadedAtm(LFPG_BTCAtmBase atm)
    {
        if (!g_Game || !g_Game.IsServer())
            return;
        if (!atm)
            return;
        if (s_CompoundActionDepth != 0)
            return;

        EnsureLoaded();
        EnsureClaimState();
        if (s_FutureVersionReadOnly)
            return;

        string deviceId = atm.LFPG_GetDeviceId();
        if (deviceId == "")
            return;
        array<ref LFPG_BalanceClaim> chain = CollectDeviceClaims(deviceId);
        if (chain.Count() == 0)
        {
            s_ReconciledDevices.Set(deviceId, true);
            return;
        }
        s_ReconciledDevices.Remove(deviceId);
        if (s_ReappliedThisBoot.Contains(deviceId))
        {
            s_ReconciledDevices.Set(deviceId, true);
            return;
        }

        if (!DeviceClaimRecordsValidForTimeline(chain))
        {
            LogAmbiguousChain(deviceId, atm.LFPG_GetBtcStock(), chain, "invalid claim state or record");
            return;
        }

        int pendingCount = 0;
        int i = 0;
        for (i = 0; i < chain.Count(); i = i + 1)
        {
            if (chain[i].state == LFPG_CLAIM_PENDING)
                pendingCount = pendingCount + 1;
        }

        if (pendingCount > 0)
        {
            ReconcilePendingChain(atm, deviceId, chain);
            return;
        }
        ReconcileRefundedChain(atm, deviceId, chain);
    }
    protected static bool RefundPendingClaimAt(int claimIndex)
    {
        if (s_CompoundActionDepth != 0)
            return false;
        LFPG_BalanceClaim claim = s_Claims[claimIndex];
        if (!claim || claim.state != LFPG_CLAIM_PENDING || claim.debit <= 0 || claim.uid == "")
        {
            LogClaimError("[LFPG_Balance_Native] Orphan claim refund rejected: invalid pending claim identity or debit", "", "");
            return false;
        }

        int current = 0;
        bool hadBalance = s_Balances.Contains(claim.uid);
        if (hadBalance)
            current = s_Balances.Get(claim.uid);
        if (current < 0 || current > LFPG_NATIVE_BALANCE_CAP)
        {
            LogClaimError("[LFPG_Balance_Native] Orphan claim refund rejected: stored balance outside Native bounds uid=" + claim.uid, claim.uid, claim.deviceId);
            return false;
        }

        int room = LFPG_NATIVE_BALANCE_CAP - current;
        int refunded = claim.debit;
        if (refunded > room)
            refunded = room;
        if (refunded != claim.debit)
        {
            LogClaimError("[LFPG_Balance_Native] Orphan claim refund was not exact; claim remains PENDING uid=" + claim.uid, claim.uid, claim.deviceId);
            return false;
        }

        // A Native debit refunds to the Native ledger. If the live provider is
        // no longer Native (a provider migration between reboots, now reachable
        // once LBmaster_Core compiles), crediting s_Balances would resurrect
        // EUR into a ledger the active UI does not read. Hold the claim PENDING
        // so an admin can migrate it, rather than hide the money.
        // Fail closed: refund to the Native ledger only when the active
        // provider is provably Native. A non-Native provider (migration) OR an
        // indeterminate one (GetActive() is documented nullable) both hold the
        // claim PENDING rather than resurrect EUR into a ledger the live UI may
        // not read. The claim keeps claim.debit; a later boot with Native
        // active, or an admin migration, resolves it.
        LFPG_BalanceProvider activeProvider = LFPG_BalanceRegistry.GetActive();
        if (!activeProvider || activeProvider.GetName() != "Native")
        {
            string heldProvider = "none";
            if (activeProvider)
                heldProvider = activeProvider.GetName();
            LogClaimError("[LFPG_Balance_Native] Orphan claim refund held: active provider is " + heldProvider + ", not Native; claim remains PENDING for admin migration uid=" + claim.uid, claim.uid, claim.deviceId);
            return false;
        }

        bool dirtyBefore = s_CompoundActionDirty;
        int previousState = claim.state;
        int previousBoots = claim.bootsSinceRefund;
        s_Balances.Set(claim.uid, current + refunded);
        claim.state = LFPG_CLAIM_REFUNDED;
        claim.bootsSinceRefund = 0;
        if (!SaveToDisk())
        {
            if (hadBalance)
                s_Balances.Set(claim.uid, current);
            else
                s_Balances.Remove(claim.uid);
            claim.state = previousState;
            claim.bootsSinceRefund = previousBoots;
            s_CompoundActionDirty = dirtyBefore;
            LogClaimError("[LFPG_Balance_Native] Orphan claim refund save failed; claim remains PENDING uid=" + claim.uid, claim.uid, claim.deviceId);
            return false;
        }

        s_CompoundActionDirty = dirtyBefore;
        LFPG_Util.Warn("[LFPG_Balance_Native] Orphan ATM claim refunded and tombstoned uid=" + claim.uid + " deviceId=" + claim.deviceId);
        return true;
    }

    protected static bool AdvanceOrPruneRefundedClaimAt(int claimIndex)
    {
        if (s_CompoundActionDepth != 0)
            return false;
        LFPG_BalanceClaim claim = s_Claims[claimIndex];
        if (!claim || claim.state != LFPG_CLAIM_REFUNDED)
            return false;

        int nextBoot = claim.bootsSinceRefund + 1;
        if (nextBoot >= 3)
        {
            if (!PersistRemoveClaimAt(claimIndex))
            {
                LogClaimError("[LFPG_Balance_Native] Refunded tombstone prune failed deviceId=" + claim.deviceId, claim.uid, claim.deviceId);
                return false;
            }
            return true;
        }

        int previousBoot = claim.bootsSinceRefund;
        bool dirtyBefore = s_CompoundActionDirty;
        claim.bootsSinceRefund = nextBoot;
        if (!SaveToDisk())
        {
            claim.bootsSinceRefund = previousBoot;
            s_CompoundActionDirty = dirtyBefore;
            LogClaimError("[LFPG_Balance_Native] Refunded tombstone boot counter save failed deviceId=" + claim.deviceId, claim.uid, claim.deviceId);
            return false;
        }
        s_CompoundActionDirty = dirtyBefore;
        return false;
    }

    protected static bool ObserveAbsentPendingClaimAt(int claimIndex)
    {
        LFPG_BalanceClaim claim = s_Claims[claimIndex];
        if (!claim || claim.state != LFPG_CLAIM_PENDING)
            return false;
        int previousOrphanBoots = claim.orphanBoots;
        claim.orphanBoots = claim.orphanBoots + 1;
        if (claim.orphanBoots < 2)
        {
            bool dirtyBefore = s_CompoundActionDirty;
            if (!SaveToDisk())
            {
                claim.orphanBoots = previousOrphanBoots;
                s_CompoundActionDirty = dirtyBefore;
                LogClaimError("[LFPG_Balance_Native] First orphan observation was not durable deviceId=" + claim.deviceId, claim.uid, claim.deviceId);
                return false;
            }
            s_CompoundActionDirty = dirtyBefore;
            return false;
        }

        if (claim.debit > 0)
        {
            if (RefundPendingClaimAt(claimIndex))
                return false;
            claim.orphanBoots = previousOrphanBoots;
            return false;
        }
        if (claim.debit == 0)
        {
            if (PersistRemoveClaimAt(claimIndex))
                return true;
            claim.orphanBoots = previousOrphanBoots;
            LogClaimError("[LFPG_Balance_Native] Orphan checkpoint clear failed after two observations deviceId=" + claim.deviceId, FindDeviceClaimUID(claim.deviceId), claim.deviceId);
            return false;
        }
        claim.orphanBoots = previousOrphanBoots;
        return false;
    }

    static void SweepOrphanClaims()
    {
        if (!g_Game || !g_Game.IsServer())
            return;
        if (s_CompoundActionDepth != 0)
            return;

        EnsureLoaded();
        EnsureClaimState();
        if (s_FutureVersionReadOnly)
            return;

        LFPG_Util.Info("[LFPG_Balance_Native] Orphan sweep pass executed");
        int nullIndex = 0;
        while (nullIndex < s_Claims.Count())
        {
            if (s_Claims[nullIndex])
            {
                nullIndex = nullIndex + 1;
                continue;
            }
            if (!PersistRemoveClaimAt(nullIndex))
                nullIndex = nullIndex + 1;
        }

        array<string> deviceIds = new array<string>;
        int collectIndex = 0;
        for (collectIndex = 0; collectIndex < s_Claims.Count(); collectIndex = collectIndex + 1)
        {
            LFPG_BalanceClaim collectedClaim = s_Claims[collectIndex];
            if (collectedClaim && deviceIds.Find(collectedClaim.deviceId) < 0)
                deviceIds.Insert(collectedClaim.deviceId);
        }

        int deviceIndex = 0;
        for (deviceIndex = 0; deviceIndex < deviceIds.Count(); deviceIndex = deviceIndex + 1)
        {
            string deviceId = deviceIds[deviceIndex];
            if (s_OrphanObservedThisBoot.Contains(deviceId))
                continue;

            EntityAI device = null;
            if (deviceId != "")
                device = LFPG_DeviceRegistry.Get().FindById(deviceId);
            if (device)
                continue;

            array<ref LFPG_BalanceClaim> absentChain = CollectDeviceClaims(deviceId);
            if (!DeviceClaimRecordsValidForTimeline(absentChain))
            {
                LogAmbiguousChain(deviceId, 0, absentChain, "invalid absent-device claim chain");
                s_OrphanObservedThisBoot.Set(deviceId, true);
                continue;
            }

            bool removedProven = false;
            if (!PersistRemoveChainProvenPurchases(deviceId, removedProven))
                continue;

            int claimIndex = 0;
            while (claimIndex < s_Claims.Count())
            {
                LFPG_BalanceClaim claim = s_Claims[claimIndex];
                if (!claim || claim.deviceId != deviceId)
                {
                    claimIndex = claimIndex + 1;
                    continue;
                }

                if (claim.state == LFPG_CLAIM_PENDING)
                {
                    bool removedPending = ObserveAbsentPendingClaimAt(claimIndex);
                    if (removedPending)
                        continue;
                    claimIndex = claimIndex + 1;
                    continue;
                }
                if (claim.state == LFPG_CLAIM_REFUNDED)
                {
                    bool pruned = AdvanceOrPruneRefundedClaimAt(claimIndex);
                    if (pruned)
                        continue;
                    claimIndex = claimIndex + 1;
                    continue;
                }

                LogClaimError("[LFPG_Balance_Native] Invalid orphan claim retained fail-closed deviceId=" + deviceId, claim.uid, deviceId);
                claimIndex = claimIndex + 1;
            }
            s_OrphanObservedThisBoot.Set(deviceId, true);
        }
    }
    static void ResetMissionClaimState()
    {
        EnsureClaimState();
        s_ReconciledDevices.Clear();
        s_ReappliedThisBoot.Clear();
        s_OrphanObservedThisBoot.Clear();
        s_AmbiguousObservedThisBoot.Clear();
        s_ClaimErrorWindowStartMs.Clear();
        s_ClaimErrorCounts.Clear();
        s_Claims.Clear();
        if (s_Balances)
            s_Balances.Clear();
        s_Loaded = false;
        s_CompoundActionDirty = false;
    }


    protected static void ClearCompoundPreState()
    {
        if (!s_CompoundPreState)
            s_CompoundPreState = new array<ref LFPG_BalanceCompoundSnapshot>;
        else
            s_CompoundPreState.Clear();
        s_CompoundDirtyBefore = false;
    }

    protected static void CaptureCompoundPreState(string uid, bool existed, int value)
    {
        if (s_CompoundActionDepth <= 0)
            return;

        if (!s_CompoundPreState)
            s_CompoundPreState = new array<ref LFPG_BalanceCompoundSnapshot>;

        int index = 0;
        while (index < s_CompoundPreState.Count())
        {
            LFPG_BalanceCompoundSnapshot existing = s_CompoundPreState[index];
            if (existing && existing.m_UID == uid)
                return;
            index = index + 1;
        }

        LFPG_BalanceCompoundSnapshot snapshot = new LFPG_BalanceCompoundSnapshot();
        snapshot.m_UID = uid;
        snapshot.m_Existed = existed;
        snapshot.m_Value = value;
        s_CompoundPreState.Insert(snapshot);
    }

    static void BeginCompoundBalanceAction()
    {
        if (s_CompoundActionDepth != 0)
        {
            LFPG_Util.Error("[LFPG_Balance_Native] Begin found an orphaned compound; self-healing before the new action");
            s_CompoundActionDepth = 0;
            if (s_CompoundActionDirty)
            {
                bool orphanSaved = SaveToDisk();
                if (orphanSaved)
                    s_CompoundActionDirty = false;
                else
                    s_CompoundActionDirty = true;
            }
        }

        ClearCompoundPreState();
        s_CompoundDirtyBefore = s_CompoundActionDirty;
        s_CompoundActionDepth = s_CompoundActionDepth + 1;
    }

    static bool EndCompoundBalanceAction()
    {
        if (s_CompoundActionDepth <= 0)
            return true;

        s_CompoundActionDepth = s_CompoundActionDepth - 1;
        if (s_CompoundActionDepth > 0)
            return true;
        if (!s_CompoundActionDirty)
        {
            ClearCompoundPreState();
            return true;
        }

        bool saved = SaveToDisk();
        if (!saved)
        {
            s_CompoundActionDirty = true;
            return false;
        }

        s_CompoundActionDirty = false;
        ClearCompoundPreState();
        return true;
    }

    static void RevertCompoundBalanceAction()
    {
        if (s_CompoundPreState)
        {
            int index = 0;
            while (index < s_CompoundPreState.Count())
            {
                LFPG_BalanceCompoundSnapshot snapshot = s_CompoundPreState[index];
                if (snapshot)
                {
                    if (snapshot.m_Existed)
                        s_Balances.Set(snapshot.m_UID, snapshot.m_Value);
                    else
                        s_Balances.Remove(snapshot.m_UID);
                }
                index = index + 1;
            }
        }

        s_CompoundActionDirty = s_CompoundDirtyBefore;
        s_CompoundActionDepth = 0;
        ClearCompoundPreState();
    }

    static bool FlushBalanceOnShutdown()
    {
        s_CompoundActionDepth = 0;
        if (!s_CompoundActionDirty)
            return true;

        bool saved = SaveToDisk();
        if (!saved)
        {
            s_CompoundActionDirty = true;
            return false;
        }

        s_CompoundActionDirty = false;
        return true;
    }

    protected static bool SaveBalanceMutation()
    {
        if (s_CompoundActionDepth > 0)
        {
            s_CompoundActionDirty = true;
            return true;
        }

        bool saved = SaveToDisk();
        if (saved)
            s_CompoundActionDirty = false;
        return saved;
    }
    override int GetBalance(PlayerBase player)
    {
        string uid = GetUID(player);
        if (uid == "")
            return 0;

        EnsureLoaded();

        if (s_FutureVersionReadOnly)
            return 0;

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

        if (s_FutureVersionReadOnly)
            return 0;

        int current = 0;
        bool hadBalance = s_Balances.Contains(uid);
        if (hadBalance)
        {
            current = s_Balances.Get(uid);
        }
        if (current < 0 || current >= LFPG_NATIVE_BALANCE_CAP)
            return 0;

        int room = LFPG_NATIVE_BALANCE_CAP - current;
        int toAdd = amount;
        if (toAdd > room)
        {
            toAdd = room;
        }
        if (toAdd <= 0)
            return 0;

        int newBal = current + toAdd;
        CaptureCompoundPreState(uid, hadBalance, current);
        bool dirtyBefore = s_CompoundActionDirty;
        s_Balances.Set(uid, newBal);

        if (!SaveBalanceMutation())
        {
            if (hadBalance)
                s_Balances.Set(uid, current);
            else
                s_Balances.Remove(uid);
            s_CompoundActionDirty = dirtyBefore;
            LFPG_Util.Error("[LFPG_Balance_Native] Add rollback: balance snapshot was not durable");
            return 0;
        }

        string logMsg = "[LFPG_Balance_Native] Add uid=";
        logMsg = logMsg + uid;
        logMsg = logMsg + " +";
        logMsg = logMsg + toAdd.ToString();
        logMsg = logMsg + " -> ";
        logMsg = logMsg + newBal.ToString();
        LFPG_Util.Info(logMsg);

        return toAdd;
    }

    override int RemoveBalance(PlayerBase player, int amount)
    {
        if (amount <= 0)
            return 0;

        string uid = GetUID(player);
        if (uid == "")
            return 0;

        EnsureLoaded();

        if (s_FutureVersionReadOnly)
            return 0;

        int current = 0;
        bool hadBalance = s_Balances.Contains(uid);
        if (hadBalance)
        {
            current = s_Balances.Get(uid);
        }
        if (current < 0)
            return 0;

        // Cannot remove more than available
        int toRemove = amount;
        if (toRemove > current)
        {
            toRemove = current;
        }

        int newBal = current - toRemove;
        CaptureCompoundPreState(uid, hadBalance, current);
        bool dirtyBefore = s_CompoundActionDirty;
        s_Balances.Set(uid, newBal);

        if (!SaveBalanceMutation())
        {
            if (hadBalance)
                s_Balances.Set(uid, current);
            else
                s_Balances.Remove(uid);
            s_CompoundActionDirty = dirtyBefore;
            LFPG_Util.Error("[LFPG_Balance_Native] Remove rollback: balance snapshot was not durable");
            return 0;
        }

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

    static int ReadPlayerBalance(string uid)
    {
        EnsureLoaded();

        if (s_FutureVersionReadOnly)
            return 0;

        if (s_Balances.Contains(uid))
        {
            int bal = s_Balances.Get(uid);
            return bal;
        }

        return 0;
    }

    static bool WritePlayerBalance(string uid, int balance)
    {
        if (uid == "")
            return false;
        if (balance < 0 || balance > LFPG_NATIVE_BALANCE_CAP)
            return false;

        EnsureLoaded();

        if (s_FutureVersionReadOnly)
            return false;

        int current = 0;
        bool hadBalance = s_Balances.Contains(uid);
        if (hadBalance)
            current = s_Balances.Get(uid);

        CaptureCompoundPreState(uid, hadBalance, current);
        bool dirtyBefore = s_CompoundActionDirty;
        s_Balances.Set(uid, balance);
        if (!SaveBalanceMutation())
        {
            if (hadBalance)
                s_Balances.Set(uid, current);
            else
                s_Balances.Remove(uid);
            s_CompoundActionDirty = dirtyBefore;
            LFPG_Util.Error("[LFPG_Balance_Native] Set rollback: balance snapshot was not durable");
            return false;
        }

        return true;
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
        EnsureClaimState();
        s_Balances.Clear();
        s_Claims.Clear();
        s_ReconciledDevices.Clear();
        s_ReappliedThisBoot.Clear();
        s_OrphanObservedThisBoot.Clear();
        s_AmbiguousObservedThisBoot.Clear();

        string settingsDir = LFPG_BTC_SETTINGS_DIR;
        if (!FileExist(settingsDir))
        {
            MakeDirectory(settingsDir);
        }

        string filePath = LFPG_BALANCE_NATIVE_FILE;

        // A future target must be latched before typed orphan recovery can
        // inspect or promote any sibling file over it.
        int rawVersion = 0;
        if (LFPG_FileUtil.TryReadRawJsonVersion(filePath, rawVersion))
        {
            if (rawVersion > 2)
            {
                s_FutureVersionReadOnly = true;
                LFPG_Util.Error("[LFPG_Balance_Native] Future balance schema ver=" + rawVersion.ToString() + " detected. Native balances are read-only for this process; UI displays zero and the file will not be rewritten.");
                return;
            }
        }

        // PR-A: typed recovery prefers parseable orphan .tmp over .bak.new/.bak.
        // Returns false only if no candidate (target/.tmp/.bak.new/.bak) exists.
        if (!LFPG_FileUtil.EnsureBalancesFileOrRestore(filePath))
        {
            string noFileMsg = "[LFPG_Balance_Native] No balances file (and no .bak/.tmp), starting fresh: ";
            noFileMsg = noFileMsg + filePath;
            LFPG_Util.Info(noFileMsg);
            return;
        }

        // PR-A.6 R21-PR-A5-001 defense-in-depth: if PromoteOrphanTmp aborted
        // and PreserveOrphanTmpEvidence ALSO failed, the .tmp could still be
        // on disk and the next AtomicSave Step 1 would overwrite it. Inhibit
        // save in that case so admin can recover manually. Normal flow:
        // PromoteOrphanTmp success deletes .tmp; PromoteOrphanTmp abort
        // renames .tmp -> .tmp.preserved.<ts>_<rnd>. .tmp surviving = double
        // I/O failure.
        string tmpPath = filePath + ".tmp";
        if (FileExist(tmpPath))
        {
            string defMsg = "[LFPG_Balance_Native] Recovery incomplete: orphan .tmp still on disk after EnsureBalancesFileOrRestore. Loading target as fallback but Save inhibited until next restart to preserve .tmp. Admin: inspect ";
            defMsg = defMsg + tmpPath;
            defMsg = defMsg + " and recover manually if it holds newer state.";
            LFPG_Util.Error(defMsg);
            s_DiskInhibited = true;
        }

        // Recovery can materialize target from .tmp/.bak when it was absent;
        // classify that recovered file before the normal typed load as well.
        rawVersion = 0;
        if (LFPG_FileUtil.TryReadRawJsonVersion(filePath, rawVersion))
        {
            if (rawVersion > 2)
            {
                s_FutureVersionReadOnly = true;
                LFPG_Util.Error("[LFPG_Balance_Native] Future balance schema ver=" + rawVersion.ToString() + " restored during recovery. Native balances are read-only for this process; UI displays zero and the file will not be rewritten.");
                return;
            }
        }

        ref LFPG_BalanceData data = new LFPG_BalanceData();
        string err;
        bool ok = JsonFileLoader<LFPG_BalanceData>.LoadFile(filePath, data, err);
        if (!ok)
        {
            // PR-A P1-7: corruption recovery flow.
            //   1. preserve evidence as .corrupt.<tickTime>_<rnd>
            //   2. latch s_DiskInhibited (process-lifetime; clears on restart)
            //   3. leave s_Balances as-is (caller has empty map → no wipe)
            //   4. RPT escalate so admin sees it
            // PR-A.5 R21-PR-A-004: GetTickTime() resets to 0 on each server
            // boot, so back-to-back boot+corrupt cycles could collide on the
            // same filename. Append Math.RandomInt suffix to drop collision
            // risk to ~1/90000 per same-tick boot.
            float ts = GetGame().GetTickTime();
            int rnd = Math.RandomInt(10000, 99999);
            string corruptPath = filePath + ".corrupt." + ((int)ts).ToString() + "_" + rnd.ToString();
            if (CopyFile(filePath, corruptPath))
            {
                string preservedMsg = "[LFPG_Balance_Native] Load failed (";
                preservedMsg = preservedMsg + err;
                preservedMsg = preservedMsg + "). Preserved as ";
                preservedMsg = preservedMsg + corruptPath;
                preservedMsg = preservedMsg + ". Save inhibited until next restart. Admin: inspect, restore from .bak or .corrupt.<ts>, then restart.";
                LFPG_Util.Error(preservedMsg);
            }
            else
            {
                string failMsg = "[LFPG_Balance_Native] Load failed (";
                failMsg = failMsg + err;
                failMsg = failMsg + ") AND .corrupt preservation copy failed. Save inhibited until next restart. Admin: manual recovery required.";
                LFPG_Util.Error(failMsg);
            }
            s_DiskInhibited = true;
            return;
        }

        if (!data.entries)
            data.entries = new array<ref LFPG_BalanceEntry>;

        int i = 0;
        int count = data.entries.Count();
        for (i = 0; i < count; i = i + 1)
        {
            LFPG_BalanceEntry entry = data.entries[i];
            if (!entry)
                continue;
            if (entry.uid == "")
                continue;

            int loadedBalance = entry.balance;
            if (loadedBalance < 0)
                loadedBalance = 0;
            else if (loadedBalance > LFPG_NATIVE_BALANCE_CAP)
                loadedBalance = LFPG_NATIVE_BALANCE_CAP;

            if (loadedBalance != entry.balance)
            {
                string clampMsg = "[LFPG_Balance_Native] Clamped loaded balance uid=";
                clampMsg = clampMsg + entry.uid;
                clampMsg = clampMsg + " from ";
                clampMsg = clampMsg + entry.balance.ToString();
                clampMsg = clampMsg + " to ";
                clampMsg = clampMsg + loadedBalance.ToString();
                LFPG_Util.Warn(clampMsg);
            }

            s_Balances.Set(entry.uid, loadedBalance);
        }

        if (data.claims)
        {
            int claimIndex = 0;
            for (claimIndex = 0; claimIndex < data.claims.Count(); claimIndex = claimIndex + 1)
            {
                LFPG_BalanceClaim loadedClaim = data.claims[claimIndex];
                if (loadedClaim)
                    s_Claims.Insert(loadedClaim);
            }
        }

        string loadMsg = "[LFPG_Balance_Native] Loaded ";
        loadMsg = loadMsg + count.ToString();
        loadMsg = loadMsg + " player balances from ";
        loadMsg = loadMsg + filePath;
        LFPG_Util.Info(loadMsg);
    }

    protected static bool SaveToDisk()
    {
        if (s_FutureVersionReadOnly)
            return false;
        EnsureClaimState();

        // PR-A P1-7: refuse to overwrite a corrupt file detected at load.
        // PR-A.5 R21-PR-A-003: rate-limit warn (first hit + every 60s with
        // cumulative count) so an active server doesn't spam RPT during
        // incident.
        if (s_DiskInhibited)
        {
            s_InhibitedSaveCount = s_InhibitedSaveCount + 1;
            float now = GetGame().GetTickTime();
            bool firstHit = (s_LastInhibitedWarnTime == 0.0);
            bool periodElapsed = ((now - s_LastInhibitedWarnTime) >= 60.0);
            if (firstHit || periodElapsed)
            {
                string warnMsg = "[LFPG_Balance_Native] Save inhibited (corrupt-load latch active until restart). Cumulative blocked saves: ";
                warnMsg = warnMsg + s_InhibitedSaveCount.ToString();
                warnMsg = warnMsg + ". In-memory mutations will NOT persist.";
                LFPG_Util.Warn(warnMsg);
                s_LastInhibitedWarnTime = now;
            }
            return false;
        }

        string settingsDir = LFPG_BTC_SETTINGS_DIR;
        if (!FileExist(settingsDir))
        {
            MakeDirectory(settingsDir);
        }

        ref LFPG_BalanceData data = new LFPG_BalanceData();
        data.ver = 2;

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

        int claimIndex = 0;
        for (claimIndex = 0; claimIndex < s_Claims.Count(); claimIndex = claimIndex + 1)
        {
            LFPG_BalanceClaim claim = s_Claims[claimIndex];
            if (claim)
                data.claims.Insert(claim);
        }

        string filePath = LFPG_BALANCE_NATIVE_FILE;
        // PR-A P0-1: route through AtomicSave (tmp + bak.new swap + verify).
        bool saved = LFPG_FileUtil.AtomicSaveBalances(filePath, data);
        if (!saved)
        {
            string errMsg = "[LFPG_Balance_Native] AtomicSave failed: ";
            errMsg = errMsg + filePath;
            LFPG_Util.Error(errMsg);
            return false;
        }
        return true;
    }
};
