// =========================================================
// LF_PowerGrid - file utility helpers
//
// Atomic save pattern for JSON files:
//   Step 1: write to .tmp
//   Step 2: read-back verify .tmp (skippable via AtomicVerifyReadback)
//   Step 3: stage existing target to .bak.new (NEVER delete .bak yet)
//   Step 4: promote .tmp -> target (DeleteFile + CopyFile)
//   Step 5: rotate .bak (DeleteFile old .bak, CopyFile .bak.new -> .bak,
//           DeleteFile .bak.new)
//   Step 6: cleanup .tmp
//
// Every crash window between steps is recoverable: typed EnsureXOrRestore
// helpers parse any orphan .tmp before promoting, so corrupt mid-save .tmp
// never overwrites a valid target. Promote-fail keeps .tmp + .bak.new on
// disk as evidence for next-boot recovery.
//
// Recovery hierarchy at load time:
//   1. .tmp parses as typed T -> promote staged.
//   2. target parses as typed T -> use it.
//   3. .bak.new parses -> preserve unreadable target, restore staged snapshot.
//   4. .bak parses -> preserve unreadable target, restore older snapshot.
//   5. no target/backups -> return false (caller starts fresh).
//
// Balances are different: Native rolls RAM back when AtomicSaveBalances
// returns false. A leftover parseable .tmp must not be promoted unless an
// in-flight marker (target + ".saving") proves the process died inside the
// DeleteFile/CopyFile window and a backup still exists to reconstruct the
// previous target. An aborted save clears that marker first.
//
// Enforce Script has no RenameFile; we use CopyFile + DeleteFile.
// CopyFile destName must be "$profile:" or "$saves:" (vanilla constraint).
//
// Generic AtomicSave<T> is rejected: Enforce generics cannot pass Class<T>
// as parameter, so typed instantiation for read-back verify and .tmp parse
// requires concrete methods per type; path-only save steps are shared.
// =========================================================

class LFPG_FileUtil
{
	protected static const int RECOVERY_WIRES = 1;
	protected static const int RECOVERY_SETTINGS = 2;
	protected static const int RECOVERY_BALANCES = 3;

	// Only path operations are shared; typed writes and balances aborts stay local.
	protected static bool StageCurrentTarget(string targetPath, string bakNewPath)
	{
		if (!FileExist(targetPath))
			return true;
		if (FileExist(bakNewPath)) DeleteFile(bakNewPath);
		if (CopyFile(targetPath, bakNewPath))
			return true;
		LFPG_Util.Error("[FileUtil] AtomicSave: stage bak.new failed for " + targetPath);
		return false;
	}

	protected static void RotateStagedBackup(string bakPath, string bakNewPath)
	{
		if (FileExist(bakPath)) DeleteFile(bakPath);
		if (FileExist(bakNewPath))
		{
			if (CopyFile(bakNewPath, bakPath))
				DeleteFile(bakNewPath);
			else
				LFPG_Util.Warn("[FileUtil] AtomicSave: bak rotation failed; leaving bak.new for recovery");
		}
	}

    // =========================================================
    // ATOMIC SAVE - one method per concrete type
    // =========================================================

    // ---- Vanilla Wires ----
    static bool AtomicSaveVanillaWires(string targetPath, LFPG_VanillaWireStore store)
    {
        string tmpPath    = targetPath + ".tmp";
        string bakPath    = targetPath + ".bak";
        string bakNewPath = targetPath + ".bak.new";

        // Step 1: write to .tmp
        string err;
        if (!JsonFileLoader<LFPG_VanillaWireStore>.SaveFile(tmpPath, store, err))
        {
            LFPG_Util.Error("[FileUtil] AtomicSave: failed to write tmp: " + err);
            return false;
        }
        if (!FileExist(tmpPath))
        {
            LFPG_Util.Error("[FileUtil] AtomicSave: tmp file not found after write");
            return false;
        }

        // Step 2: read-back verify (skippable via setting)
        LFPG_ServerSettings st = LFPG_Settings.Get();
        bool doReadback = true;
        if (st) doReadback = st.AtomicVerifyReadback;
        if (doReadback)
        {
            LFPG_VanillaWireStore verify = new LFPG_VanillaWireStore();
            string verifyErr;
            if (!JsonFileLoader<LFPG_VanillaWireStore>.LoadFile(tmpPath, verify, verifyErr))
            {
                LFPG_Util.Error("[FileUtil] AtomicSave: tmp read-back failed: " + verifyErr);
                DeleteFile(tmpPath);
                return false;
            }
        }

        // Step 3: stage current target as .bak.new (do NOT delete .bak yet)
		if (!StageCurrentTarget(targetPath, bakNewPath))
		{
			DeleteFile(tmpPath);
			return false;
		}

        // Step 4: promote .tmp -> target.
        // Window between DeleteFile(target) and CopyFile(tmp,target) is bounded:
        //   .tmp still on disk (newest) + .bak.new still has previous target.
        // Next boot's typed recovery promotes .tmp.
        if (FileExist(targetPath)) DeleteFile(targetPath);
        if (!CopyFile(tmpPath, targetPath))
        {
            LFPG_Util.Error("[FileUtil] AtomicSave: promote tmp->target failed");
            // Restore previous target; .bak.new is newer than .bak.
            if (FileExist(bakNewPath))
            {
                CopyFile(bakNewPath, targetPath);
            }
            else if (FileExist(bakPath))
            {
                CopyFile(bakPath, targetPath);
            }
            // Keep .tmp + .bak.new for next-boot recovery/evidence.
            return false;
        }

        // Step 5: rotate .bak (old .bak discarded, .bak.new -> .bak).
		RotateStagedBackup(bakPath, bakNewPath);

        // Step 6: cleanup .tmp after target + bak are stable.
        DeleteFile(tmpPath);

        return true;
    }

    // ---- Server Settings ----
    static bool AtomicSaveSettings(string targetPath, LFPG_ServerSettings settings)
    {
        string tmpPath    = targetPath + ".tmp";
        string bakPath    = targetPath + ".bak";
        string bakNewPath = targetPath + ".bak.new";

        string err;
        if (!JsonFileLoader<LFPG_ServerSettings>.SaveFile(tmpPath, settings, err))
        {
            LFPG_Util.Error("[FileUtil] AtomicSaveSettings: failed to write tmp: " + err);
            return false;
        }
        if (!FileExist(tmpPath))
        {
            LFPG_Util.Error("[FileUtil] AtomicSaveSettings: tmp not found after write");
            return false;
        }

        // Settings IS the file being saved; read AtomicVerifyReadback from it
        // directly (avoids recursion if Settings.Get() lazy-loads during first save).
        bool doReadback = true;
        if (settings) doReadback = settings.AtomicVerifyReadback;
        if (doReadback)
        {
            LFPG_ServerSettings verify = new LFPG_ServerSettings();
            string verifyErr;
            if (!JsonFileLoader<LFPG_ServerSettings>.LoadFile(tmpPath, verify, verifyErr))
            {
                LFPG_Util.Error("[FileUtil] AtomicSaveSettings: tmp read-back failed: " + verifyErr);
                DeleteFile(tmpPath);
                return false;
            }
        }

		if (!StageCurrentTarget(targetPath, bakNewPath))
		{
			DeleteFile(tmpPath);
			return false;
		}

        if (FileExist(targetPath)) DeleteFile(targetPath);
        if (!CopyFile(tmpPath, targetPath))
        {
            LFPG_Util.Error("[FileUtil] AtomicSaveSettings: promote tmp->target failed");
            if (FileExist(bakNewPath))
            {
                CopyFile(bakNewPath, targetPath);
            }
            else if (FileExist(bakPath))
            {
                CopyFile(bakPath, targetPath);
            }
            return false;
        }

		RotateStagedBackup(bakPath, bakNewPath);
        DeleteFile(tmpPath);

        return true;
    }

    // ---- Player Balances ----
    static bool AtomicSaveBalances(string targetPath, LFPG_BalanceData data)
    {
        LFPG_FaultInject.Touch();
        if (LFPG_FaultInject.ShouldFail("AtomicSaveBalances_false"))
            return false;

        string tmpPath    = targetPath + ".tmp";
        string bakPath    = targetPath + ".bak";
        string bakNewPath = targetPath + ".bak.new";

        string err;
        if (!JsonFileLoader<LFPG_BalanceData>.SaveFile(tmpPath, data, err))
        {
            LFPG_Util.Error("[FileUtil] AtomicSaveBalances: failed to write tmp: " + err);
            // SaveFile can return false and still leave a parseable .tmp.
            // No in-flight marker has been written yet, so recovery must not
            // treat that leftover as a committed mutation.
            if (!LFPG_FaultInject.ShouldFail("DiscardAbortedTmp_false"))
                DiscardAbortedBalancesTmp(tmpPath);
            return false;
        }
        if (LFPG_FaultInject.ShouldFail("SaveFile_abort_leave_tmp"))
            return false;
        if (!FileExist(tmpPath))
        {
            LFPG_Util.Error("[FileUtil] AtomicSaveBalances: tmp not found after write");
            return false;
        }

        LFPG_ServerSettings st = LFPG_Settings.Get();
        bool doReadback = true;
        if (st) doReadback = st.AtomicVerifyReadback;
        if (doReadback)
        {
            LFPG_BalanceData verify = new LFPG_BalanceData();
            string verifyErr;
            if (!JsonFileLoader<LFPG_BalanceData>.LoadFile(tmpPath, verify, verifyErr))
            {
                LFPG_Util.Error("[FileUtil] AtomicSaveBalances: tmp read-back failed: " + verifyErr);
                DeleteFile(tmpPath);
                return false;
            }
        }

		if (!StageCurrentTarget(targetPath, bakNewPath))
		{
			DiscardAbortedBalancesTmp(tmpPath);
			return false;
		}

        // Marker is written only after the candidate verifies and the previous
        // target is staged. A crash past this line, with target absent and a
        // backup present, is the sole case recovery may promote the .tmp.
        if (!WriteBalancesSaveIntent(targetPath))
        {
            DiscardAbortedBalancesTmp(tmpPath);
            return false;
        }

        if (FileExist(targetPath)) DeleteFile(targetPath);
        if (LFPG_FaultInject.ShouldCrash("balances_after_delete_before_promote"))
            return false;
        bool promoteOk = false;
        if (!LFPG_FaultInject.ShouldFail("CopyFile_promote"))
            promoteOk = CopyFile(tmpPath, targetPath);
        if (!promoteOk)
        {
            LFPG_Util.Error("[FileUtil] AtomicSaveBalances: promote tmp->target failed");
            // Clear the in-flight marker before touching leftovers so the next
            // boot cannot promote a mutation the caller is about to roll back.
            if (!ClearBalancesSaveIntent(targetPath))
            {
                LFPG_Util.Error("[FileUtil] AtomicSaveBalances: in-flight marker survived a reported abort. Admin: delete it before restarting: " + targetPath + ".saving");
            }
            if (FileExist(bakNewPath))
            {
                if (!LFPG_FaultInject.ShouldFail("CopyFile_restore"))
                    CopyFile(bakNewPath, targetPath);
            }
            else if (FileExist(bakPath))
            {
                if (!LFPG_FaultInject.ShouldFail("CopyFile_restore"))
                    CopyFile(bakPath, targetPath);
            }
            if (!FileExist(targetPath))
            {
                DiscardAbortedBalancesTmp(tmpPath);
            }
            return false;
        }

		RotateStagedBackup(bakPath, bakNewPath);
        DeleteFile(tmpPath);
        ClearBalancesSaveIntent(targetPath);

        return true;
    }

    // =========================================================
    // RECOVERY ON LOAD - typed helpers + raw fallback
    // =========================================================

    // ---- Raw fallback: target / .bak.new / .bak (NO .tmp handling) ----
	// Compatibility API only; typed loaders use EnsureTypedFileOrRestore.
    static bool EnsureFileOrRestore(string targetPath)
    {
        string bakPath    = targetPath + ".bak";
        string bakNewPath = targetPath + ".bak.new";

        if (FileExist(targetPath))
            return true;

        // .bak.new is more recent than .bak (it holds the previous target
        // staged between save steps; appears when promote failed mid-save).
        if (FileExist(bakNewPath))
        {
            LFPG_Util.Warn("[FileUtil] Target missing, restoring from staged .bak.new: " + bakNewPath);
            if (CopyFile(bakNewPath, targetPath))
                return FileExist(targetPath);
        }

        if (FileExist(bakPath))
        {
            LFPG_Util.Warn("[FileUtil] Target missing, restoring from .bak: " + bakPath);
            if (CopyFile(bakPath, targetPath))
                return FileExist(targetPath);
        }

        return false;
    }

	// A future balances file must reach Native's existing read-only barrier,
	// even if the current typed deserializer cannot understand its payload.
	protected static bool IsFutureBalancesFile(string path)
	{
		int version = 0;
		if (!TryReadRawJsonVersion(path, version))
			return false;
		return version > 2;
	}

	protected static bool IsRecoveryFileReadable(string path, int fileType)
	{
		if (!FileExist(path))
			return false;
		string err;
		if (fileType == RECOVERY_WIRES)
		{
			LFPG_VanillaWireStore wires = new LFPG_VanillaWireStore();
			return JsonFileLoader<LFPG_VanillaWireStore>.LoadFile(path, wires, err);
		}
		if (fileType == RECOVERY_SETTINGS)
		{
			LFPG_ServerSettings settings = new LFPG_ServerSettings();
			return JsonFileLoader<LFPG_ServerSettings>.LoadFile(path, settings, err);
		}
		if (fileType == RECOVERY_BALANCES)
		{
			if (IsFutureBalancesFile(path))
				return true;
			LFPG_BalanceData balances = new LFPG_BalanceData();
			return JsonFileLoader<LFPG_BalanceData>.LoadFile(path, balances, err);
		}
		return false;
	}

	// Recovery evidence must be complete before deleting an unreadable target.
	protected static bool RecoveryFilesEqual(string sourcePath, string copyPath)
	{
		FileHandle sourceHandle = OpenFile(sourcePath, FileMode.READ);
		if (sourceHandle == 0)
			return false;
		FileHandle copyHandle = OpenFile(copyPath, FileMode.READ);
		if (copyHandle == 0)
		{
			CloseFile(sourceHandle);
			return false;
		}
		string sourceBlock;
		string copyBlock;
		int sourceCount;
		int copyCount;
		bool equal = true;
		while (equal)
		{
			sourceBlock = "";
			copyBlock = "";
			sourceCount = ReadFile(sourceHandle, sourceBlock, 4096);
			copyCount = ReadFile(copyHandle, copyBlock, 4096);
			if (sourceCount < 0 || copyCount < 0 || sourceCount != copyCount || sourceBlock != copyBlock)
				equal = false;
			if (sourceCount <= 0 || copyCount <= 0)
				break;
		}
		CloseFile(sourceHandle);
		CloseFile(copyHandle);
		return equal;
	}

	protected static bool RestoreTypedBackup(string targetPath, string candidatePath, int fileType)
	{
		if (!IsRecoveryFileReadable(candidatePath, fileType))
			return false;
		string evidencePath = "";
		bool hasEvidence = false;
		if (FileExist(targetPath))
		{
			evidencePath = targetPath + ".corrupt.recovery";
			int evidenceIndex = 0;
			while (FileExist(evidencePath))
			{
				evidenceIndex = evidenceIndex + 1;
				evidencePath = targetPath + ".corrupt.recovery." + evidenceIndex.ToString();
			}
			if (!CopyFile(targetPath, evidencePath) || !RecoveryFilesEqual(targetPath, evidencePath))
			{
				LFPG_Util.Error("[FileUtil] Recovery: cannot preserve unreadable target; leaving it untouched: " + targetPath);
				return false;
			}
			hasEvidence = true;
			if (!DeleteFile(targetPath))
				return false;
		}
		bool copied = CopyFile(candidatePath, targetPath);
		if (!copied || !RecoveryFilesEqual(candidatePath, targetPath) || !IsRecoveryFileReadable(targetPath, fileType))
		{
			// A short copy can still be parseable. Do not expose it as accepted.
			if (FileExist(targetPath) && !DeleteFile(targetPath))
			{
				// The rejected copy stays on disk and EnsureTypedFileOrRestore would report
				// "artifacts exist". A short but parseable file would then be applied and a
				// later save would persist it. Put the original bytes back so the consumer
				// fails on the real damage instead.
				if (hasEvidence && CopyFile(evidencePath, targetPath) && RecoveryFilesEqual(evidencePath, targetPath))
					LFPG_Util.Error("[FileUtil] Recovery: rejected copy could not be removed; original bytes restored from evidence: " + targetPath);
				else
					LFPG_Util.Error("[FileUtil] Recovery: rejected copy cannot be removed; manual recovery required: " + targetPath);
			}
			return false;
		}
		LFPG_Util.Warn("[FileUtil] Recovery: restored validated backup: " + candidatePath);
		return true;
	}

	// Keep the public bool contract: false means no target/backups at all.
	// Unrecoverable artifacts still return true so Native attempts LoadFile and
	// latches s_DiskInhibited instead of treating damaged balances as fresh.
	protected static bool EnsureTypedFileOrRestore(string targetPath, int fileType)
	{
		if (IsRecoveryFileReadable(targetPath, fileType))
			return true;
		string bakNewPath = targetPath + ".bak.new";
		string bakPath = targetPath + ".bak";
		if (RestoreTypedBackup(targetPath, bakNewPath, fileType))
			return true;
		// A future staged backup blocks fallback to an older schema even if
		// materialization failed. Native will reject the target or missing file.
		if (fileType == RECOVERY_BALANCES && (IsFutureBalancesFile(targetPath) || IsFutureBalancesFile(bakNewPath)))
			return true;
		if (RestoreTypedBackup(targetPath, bakPath, fileType))
			return true;
		return FileExist(targetPath) || FileExist(bakNewPath) || FileExist(bakPath);
	}

    // ---- Shared promote logic (post-parse) ----
    // .tmp has been confirmed parseable. Stage current target to .bak.new
    // (if exists) and promote .tmp -> target, then rotate .bak.new -> .bak
    // (mirrors AtomicSave Step 5). Fail-closed under CopyFile failure:
    //   - stage CopyFile(target -> .bak.new) FAILS: do NOT delete target.
    //     Rename .tmp to .tmp.preserved.<ts>_<rnd> via PreserveOrphanTmpEvidence
    //     so the next AtomicSave Step 1 cannot overwrite the parseable orphan.
    //     Return true with target intact; caller loads previous state.
    //   - promote CopyFile(tmp -> target) FAILS: try restore from .bak.new
    //     (just-staged previous) or .bak (older); preserve .tmp as evidence
    //     (same reason) and keep .bak.new for next-boot.
    protected static bool PromoteOrphanTmp(string targetPath, string tmpPath, string bakPath, string bakNewPath)
    {
        // Stage current target to .bak.new only if target exists.
        if (FileExist(targetPath))
        {
            if (FileExist(bakNewPath)) DeleteFile(bakNewPath);
            if (!CopyFile(targetPath, bakNewPath))
            {
                // PR-A.6 R21-PR-A5-001: fail-closed. Preserve .tmp as evidence
                // so the next AtomicSave Step 1 cannot overwrite it.
                LFPG_Util.Error("[FileUtil] PromoteOrphanTmp: stage target -> .bak.new failed; aborting promote, target preserved.");
                PreserveOrphanTmpEvidence(tmpPath);
                return true;
            }
            DeleteFile(targetPath);
        }

        if (CopyFile(tmpPath, targetPath))
        {
            // Promote OK; mirror AtomicSave Step 5 bak rotation so post-promote
            // state matches a successful save: target current, .bak previous,
            // .tmp/.bak.new absent.
            DeleteFile(tmpPath);
            if (FileExist(bakPath)) DeleteFile(bakPath);
            if (FileExist(bakNewPath))
            {
                if (CopyFile(bakNewPath, bakPath))
                {
                    DeleteFile(bakNewPath);
                }
                else
                {
                    LFPG_Util.Warn("[FileUtil] PromoteOrphanTmp: bak rotation failed; leaving bak.new for recovery");
                }
            }
            return true;
        }

        // PR-A.6 R21-PR-A5-001: promote failed AFTER stage succeeded.
        // Restore target from .bak.new (just-staged previous), else .bak.
        // Preserve .tmp as evidence so next AtomicSave Step 1 cannot overwrite.
        LFPG_Util.Error("[FileUtil] PromoteOrphanTmp: CopyFile(tmp,target) failed; restoring previous");
        if (FileExist(bakNewPath))
        {
            CopyFile(bakNewPath, targetPath);
        }
        else if (FileExist(bakPath))
        {
            CopyFile(bakPath, targetPath);
        }
        PreserveOrphanTmpEvidence(tmpPath);
        return FileExist(targetPath);
    }

    // ---- Preserve a failed .tmp beside target as .tmp.preserved (PR-A.6) ----
    // Called from PromoteOrphanTmp abort paths. Prevents the next AtomicSave
    // Step 1 (which writes targetPath + ".tmp") from silently overwriting a
    // parseable orphan that recovery couldn't promote safely. EnsureBalances
    // deletes leftover .tmp.preserved.* siblings on the next load so they
    // cannot grow without bound. Copy anything still needed before restart.
    protected static void PreserveOrphanTmpEvidence(string tmpPath)
    {
        if (!FileExist(tmpPath))
            return;
        string preservedPath = tmpPath + ".preserved";
        if (FileExist(preservedPath))
            DeleteFile(preservedPath);
        if (CopyFile(tmpPath, preservedPath))
        {
            DeleteFile(tmpPath);
            string okMsg = "[FileUtil] PromoteOrphanTmp: .tmp preserved as ";
            okMsg = okMsg + preservedPath;
            okMsg = okMsg + " - admin can recover manually if needed.";
            LFPG_Util.Error(okMsg);
        }
        else
        {
            string failMsg = "[FileUtil] PromoteOrphanTmp: failed to preserve .tmp as ";
            failMsg = failMsg + preservedPath;
            failMsg = failMsg + " - .tmp will be overwritten by next save. Manual intervention required NOW.";
            LFPG_Util.Error(failMsg);
        }
    }

    protected static string BalancesSaveIntentPath(string targetPath)
    {
        return targetPath + ".saving";
    }

    protected static bool WriteBalancesSaveIntent(string targetPath)
    {
        string savingPath = BalancesSaveIntentPath(targetPath);
        if (FileExist(savingPath))
            DeleteFile(savingPath);

        FileHandle handle;
        handle = OpenFile(savingPath, FileMode.WRITE);
        if (handle == 0)
        {
            LFPG_Util.Error("[FileUtil] AtomicSaveBalances: failed to write in-flight marker");
            return false;
        }
        FPrintln(handle, "1");
        CloseFile(handle);
        if (!FileExist(savingPath))
        {
            LFPG_Util.Error("[FileUtil] AtomicSaveBalances: in-flight marker missing after write");
            return false;
        }
        return true;
    }

    protected static bool ClearBalancesSaveIntent(string targetPath)
    {
        string savingPath = BalancesSaveIntentPath(targetPath);
        if (!FileExist(savingPath))
            return true;
        if (LFPG_FaultInject.ShouldFail("DeleteFile_saving"))
            return false;
        DeleteFile(savingPath);
        return !FileExist(savingPath);
    }

    protected static void DiscardAbortedBalancesTmp(string tmpPath)
    {
        if (!FileExist(tmpPath))
            return;
        if (LFPG_FaultInject.ShouldFail("DiscardAbortedTmp_false"))
            return;
        PreserveOrphanTmpEvidence(tmpPath);
        if (FileExist(tmpPath))
        {
            if (!DeleteFile(tmpPath))
            {
                LFPG_Util.Error("[FileUtil] AtomicSaveBalances: aborted .tmp remains after preserve and delete failed: " + tmpPath);
            }
        }
    }

    protected static bool IsSellIntentUidSafe(string uid)
    {
        if (uid == "")
            return false;
        string allowed = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-";
        int i = 0;
        int n = uid.Length();
        string ch = "";
        for (i = 0; i < n; i = i + 1)
        {
            ch = uid.Get(i);
            if (allowed.IndexOf(ch) < 0)
                return false;
        }
        return true;
    }

    protected static string SellDestroyIntentPath(string uid)
    {
        return LFPG_BALANCE_NATIVE_FILE + ".sell." + uid;
    }

    protected static bool ParseNonNegativeIntText(string text, out int value)
    {
        value = 0;
        if (text == "")
            return false;
        int i = 0;
        int n = text.Length();
        string ch = "";
        bool isDigit = false;
        for (i = 0; i < n; i = i + 1)
        {
            ch = text.Get(i);
            isDigit = false;
            if (ch == "0" || ch == "1" || ch == "2" || ch == "3" || ch == "4")
                isDigit = true;
            if (ch == "5" || ch == "6" || ch == "7" || ch == "8" || ch == "9")
                isDigit = true;
            if (!isDigit)
                return false;
        }
        value = text.ToInt();
        if (value < 0)
            return false;
        return true;
    }

    static bool WriteSellDestroyIntent(string uid, int balanceBefore, int creditAmount, int btcAmount, string classname)
    {
        if (!IsSellIntentUidSafe(uid) || classname == "" || balanceBefore < 0 || creditAmount <= 0 || btcAmount <= 0)
            return false;

        string intentPath = SellDestroyIntentPath(uid);
        if (FileExist(intentPath))
            DeleteFile(intentPath);

        FileHandle handle;
        handle = OpenFile(intentPath, FileMode.WRITE);
        if (handle == 0)
        {
            LFPG_Util.Error("[FileUtil] WriteSellDestroyIntent: failed to open marker");
            return false;
        }
        FPrintln(handle, uid);
        FPrintln(handle, balanceBefore.ToString());
        FPrintln(handle, creditAmount.ToString());
        FPrintln(handle, btcAmount.ToString());
        FPrintln(handle, classname);
        CloseFile(handle);
        if (!FileExist(intentPath))
        {
            LFPG_Util.Error("[FileUtil] WriteSellDestroyIntent: marker missing after write");
            return false;
        }
        return true;
    }

    static bool ClearSellDestroyIntent(string uid)
    {
        if (!IsSellIntentUidSafe(uid))
            return false;
        string intentPath = SellDestroyIntentPath(uid);
        if (!FileExist(intentPath))
            return true;
        DeleteFile(intentPath);
        return !FileExist(intentPath);
    }

    static bool TryReadSellDestroyIntent(string uid, out int balanceBefore, out int creditAmount, out int btcAmount, out string classname)
    {
        balanceBefore = 0;
        creditAmount = 0;
        btcAmount = 0;
        classname = "";
        if (!IsSellIntentUidSafe(uid))
            return false;

        string intentPath = SellDestroyIntentPath(uid);
        if (!FileExist(intentPath))
            return false;

        FileHandle handle = OpenFile(intentPath, FileMode.READ);
        if (handle == 0)
            return false;

        string lineUid = "";
        string lineBalance = "";
        string lineCredit = "";
        string lineBtc = "";
        string lineClass = "";
        bool readOk = true;
        if (FGets(handle, lineUid) <= 0)
            readOk = false;
        if (readOk && FGets(handle, lineBalance) <= 0)
            readOk = false;
        if (readOk && FGets(handle, lineCredit) <= 0)
            readOk = false;
        if (readOk && FGets(handle, lineBtc) <= 0)
            readOk = false;
        if (readOk && FGets(handle, lineClass) <= 0)
            readOk = false;
        CloseFile(handle);
        if (!readOk)
            return false;

        // Trim line endings and edge whitespace without changing field interiors.
        lineUid = lineUid.Trim();
        lineBalance = lineBalance.Trim();
        lineCredit = lineCredit.Trim();
        lineBtc = lineBtc.Trim();
        lineClass = lineClass.Trim();

        if (lineUid != uid || lineClass == "")
            return false;
        if (!ParseNonNegativeIntText(lineBalance, balanceBefore))
            return false;
        if (!ParseNonNegativeIntText(lineCredit, creditAmount))
            return false;
        if (!ParseNonNegativeIntText(lineBtc, btcAmount))
            return false;
        if (creditAmount <= 0 || btcAmount <= 0)
            return false;
        classname = lineClass;
        return true;
    }

    protected static void SweepPreservedBalanceTmpEvidence(string targetPath)
    {
        string preservedPath = targetPath + ".tmp.preserved";
        if (FileExist(preservedPath))
        {
            if (DeleteFile(preservedPath))
                LFPG_Util.Info("[FileUtil] Removed leftover preserved balances tmp: " + preservedPath);
        }

        string fileName;
        FileAttr fileAttr;
        string pattern = targetPath + ".tmp.preserved.*";
        FindFileHandle handle = FindFile(pattern, fileName, fileAttr, FindFileFlags.ALL);
        if (!handle)
            return;

        int removed = 0;
        bool found = true;
        string leftoverPath = "";
        while (found)
        {
            if (fileName != "" && fileAttr != FileAttr.DIRECTORY && removed < 32)
            {
                leftoverPath = LFPG_BTC_SETTINGS_DIR + "/" + fileName;
                if (FileExist(leftoverPath))
                {
                    if (DeleteFile(leftoverPath))
                    {
                        removed = removed + 1;
                        LFPG_Util.Info("[FileUtil] Removed leftover unique preserved balances tmp: " + leftoverPath);
                    }
                }
            }
            found = FindNextFile(handle, fileName, fileAttr);
        }
        CloseFindFile(handle);
    }

    // ---- Typed: Vanilla Wires ----
    static bool EnsureVanillaWiresFileOrRestore(string targetPath)
    {
        string tmpPath    = targetPath + ".tmp";
        string bakPath    = targetPath + ".bak";
        string bakNewPath = targetPath + ".bak.new";

        if (FileExist(tmpPath))
        {
            LFPG_VanillaWireStore probe = new LFPG_VanillaWireStore();
            string parseErr;
            if (JsonFileLoader<LFPG_VanillaWireStore>.LoadFile(tmpPath, probe, parseErr))
            {
                LFPG_Util.Warn("[FileUtil] Orphan .tmp parses as LFPG_VanillaWireStore, promoting: " + tmpPath);
				PromoteOrphanTmp(targetPath, tmpPath, bakPath, bakNewPath);
				return EnsureTypedFileOrRestore(targetPath, RECOVERY_WIRES);
            }
            else
            {
                LFPG_Util.Error("[FileUtil] Orphan .tmp unparseable (LFPG_VanillaWireStore): " + parseErr + " - discarding: " + tmpPath);
                DeleteFile(tmpPath);
            }
        }

		return EnsureTypedFileOrRestore(targetPath, RECOVERY_WIRES);
    }

    // ---- Typed: Server Settings ----
    static bool EnsureSettingsFileOrRestore(string targetPath)
    {
        string tmpPath    = targetPath + ".tmp";
        string bakPath    = targetPath + ".bak";
        string bakNewPath = targetPath + ".bak.new";

        if (FileExist(tmpPath))
        {
            LFPG_ServerSettings probe = new LFPG_ServerSettings();
            string parseErr;
            if (JsonFileLoader<LFPG_ServerSettings>.LoadFile(tmpPath, probe, parseErr))
            {
                LFPG_Util.Warn("[FileUtil] Orphan .tmp parses as LFPG_ServerSettings, promoting: " + tmpPath);
				PromoteOrphanTmp(targetPath, tmpPath, bakPath, bakNewPath);
				return EnsureTypedFileOrRestore(targetPath, RECOVERY_SETTINGS);
            }
            else
            {
                LFPG_Util.Error("[FileUtil] Orphan .tmp unparseable (LFPG_ServerSettings): " + parseErr + " - discarding: " + tmpPath);
                DeleteFile(tmpPath);
            }
        }

		return EnsureTypedFileOrRestore(targetPath, RECOVERY_SETTINGS);
    }

	// Header probe, not a JSON validator. Stops at the first top-level integer
	// ver; typed LoadFile remains responsible for the complete document.
	// ReadFile bounds each block even for minified files with a huge single line.
	static bool TryReadRawJsonVersion(string targetPath, out int version)
	{
		version = 0;
		FileHandle handle = OpenFile(targetPath, FileMode.READ);
		if (handle == 0)
			return false;
		bool found = ScanRawJsonVersion(handle, version);
		CloseFile(handle);
		return found;
	}

	protected static bool ScanRawJsonVersion(FileHandle handle, out int version)
	{
		version = 0;
		string block;
		string ch;
		string key = "";
		string digits = "0123456789";
		int count;
		int i;
		int depth = 0;
		int bomRemaining = 0;
		bool firstByte = true;
		// 0 root, 1 key, 2 colon, 3 value, 4 skip value, 5 integer, 6 delimiter.
		int state = 0;
		int digit;
		int number = 0;
		bool inString = false;
		bool escaped = false;
		bool captureKey = false;
		bool longKey = false;
		bool versionKey = false;
		bool negative = false;
		bool hasDigit = false;
		bool leadingZero = false;
		bool whitespace;
		while (true)
		{
			block = "";
			count = ReadFile(handle, block, 4096);
			if (count <= 0)
				break;
			for (i = 0; i < block.Length(); i = i + 1)
			{
				ch = block.Get(i);
				// Accept the UTF-8 BOM used by some administrator editors.
				if (firstByte)
				{
					firstByte = false;
					if (ch.ToAscii() == 239)
					{
						bomRemaining = 2;
						continue;
					}
				}
				if (bomRemaining == 2)
				{
					if (ch.ToAscii() != 187)
						return false;
					bomRemaining = 1;
					continue;
				}
				if (bomRemaining == 1)
				{
					if (ch.ToAscii() != 191)
						return false;
					bomRemaining = 0;
					continue;
				}
				whitespace = ch == " " || ch == "\t" || ch == "\r" || ch == "\n";
				if (inString)
				{
					if (!escaped && ch == "\"")
					{
						inString = false;
						if (captureKey)
						{
							// The only escaped codepoints that can form the key ver.
							key.Replace("\\u0076", "v");
							key.Replace("\\u0065", "e");
							key.Replace("\\u0072", "r");
							versionKey = !longKey && key == "ver";
							state = 2;
						}
						else if (depth == 1)
							state = 4;
						continue;
					}
					if (captureKey)
					{
						if (key.Length() < 18)
							key = key + ch;
						else
							longKey = true;
					}
					if (escaped)
						escaped = false;
					else if (ch == "\\")
						escaped = true;
					continue;
				}
				if (depth > 1)
				{
					if (ch == "\"")
					{
						inString = true;
						captureKey = false;
					}
					else if (ch == "{" || ch == "[")
						depth = depth + 1;
					else if (ch == "}" || ch == "]")
						depth = depth - 1;
					if (depth == 1)
						state = 4;
					continue;
				}
				if (state == 0)
				{
					if (whitespace)
						continue;
					if (ch != "{")
						return false;
					depth = 1;
					state = 1;
					continue;
				}
				if (state == 1)
				{
					if (whitespace)
						continue;
					if (ch != "\"")
						return false;
					inString = true;
					captureKey = true;
					key = "";
					longKey = false;
					continue;
				}
				if (state == 2)
				{
					if (whitespace)
						continue;
					if (ch != ":")
						return false;
					state = 3;
					continue;
				}
				if (state == 3)
				{
					if (whitespace)
						continue;
					if (versionKey)
					{
						state = 5;
						if (ch == "-")
						{
							negative = true;
							continue;
						}
					}
					else
					{
						state = 4;
						if (ch == "\"")
						{
							inString = true;
							captureKey = false;
							continue;
						}
						if (ch == "{" || ch == "[")
						{
							depth = 2;
							continue;
						}
					}
				}
				if (state == 4)
				{
					if (ch == ",")
						state = 1;
					else if (ch == "}")
						return false;
					continue;
				}
				if (state == 5)
				{
					digit = digits.IndexOf(ch);
					if (digit >= 0)
					{
						if (hasDigit && leadingZero)
							return false;
						if (!hasDigit)
							leadingZero = digit == 0;
						hasDigit = true;
						// Saturate overflow: a huge positive version stays future.
						if (number > 214748364 || (number == 214748364 && digit > 7))
							number = 2147483647;
						else
							number = number * 10 + digit;
						continue;
					}
					if (!hasDigit)
						return false;
					state = 6;
				}
				if (state == 6)
				{
					if (whitespace)
						continue;
					if (ch != "," && ch != "}")
						return false;
					version = number;
					if (negative)
						version = 0 - number;
					return true;
				}
			}
		}
		// A truncated payload with a complete future integer still needs the
		// read-only barrier. This result never asserts full JSON validity.
		if (count == 0 && hasDigit && (state == 5 || state == 6))
		{
			version = number;
			if (negative)
				version = 0 - number;
			return true;
		}
		return false;
	}

    // ---- Typed: Player Balances ----
    static bool EnsureBalancesFileOrRestore(string targetPath)
    {
		// Preserve a future target and every sibling before any recovery mutation.
		if (IsFutureBalancesFile(targetPath))
			return true;
        LFPG_FaultInject.Touch();
        SweepPreservedBalanceTmpEvidence(targetPath);
        string tmpPath    = targetPath + ".tmp";
        string bakPath    = targetPath + ".bak";
        string bakNewPath = targetPath + ".bak.new";
        string savingPath = BalancesSaveIntentPath(targetPath);
        bool inFlight = FileExist(savingPath);

        if (FileExist(tmpPath))
        {
            // A live target means Native already had a committed snapshot. Any
            // sibling .tmp is a mutation the runtime rolled back or a crash
            // before the replace window. Never promote beside a live target.
            if (FileExist(targetPath))
            {
                LFPG_Util.Error("[FileUtil] Orphan .tmp beside a live balances target: NOT promoting, the caller rolled this mutation back. Preserving as evidence: " + tmpPath);
                PreserveOrphanTmpEvidence(tmpPath);
                ClearBalancesSaveIntent(targetPath);
				return EnsureTypedFileOrRestore(targetPath, RECOVERY_BALANCES);
            }

            bool hasBackup = false;
            if (FileExist(bakNewPath))
                hasBackup = true;
            else if (FileExist(bakPath))
                hasBackup = true;

            // Promote only an in-flight crash inside the replace window: no
            // target, marker still present, and a backup that can rebuild the
            // previous snapshot. First-save crashes and reported aborts fail
            // closed (empty/fresh or restore-from-backup) instead of resurrecting
            // a rolled-back credit.
            if (!inFlight || !hasBackup)
            {
                LFPG_Util.Error("[FileUtil] Orphan balances .tmp is not an in-flight replace. NOT promoting: " + tmpPath);
                PreserveOrphanTmpEvidence(tmpPath);
                ClearBalancesSaveIntent(targetPath);
				return EnsureTypedFileOrRestore(targetPath, RECOVERY_BALANCES);
            }

			// Materialize future in-flight data for Native to reject read-only.
			if (IsFutureBalancesFile(tmpPath))
			{
				CopyFile(tmpPath, targetPath);
				return true;
			}

            LFPG_BalanceData probe = new LFPG_BalanceData();
            string parseErr;
            if (JsonFileLoader<LFPG_BalanceData>.LoadFile(tmpPath, probe, parseErr))
            {
                LFPG_Util.Warn("[FileUtil] In-flight balances .tmp parses as LFPG_BalanceData, promoting: " + tmpPath);
				PromoteOrphanTmp(targetPath, tmpPath, bakPath, bakNewPath);
                ClearBalancesSaveIntent(targetPath);
				return EnsureTypedFileOrRestore(targetPath, RECOVERY_BALANCES);
            }
            else
            {
                LFPG_Util.Error("[FileUtil] Orphan .tmp unparseable (LFPG_BalanceData): " + parseErr + " - discarding: " + tmpPath);
                DeleteFile(tmpPath);
                ClearBalancesSaveIntent(targetPath);
            }
        }
        else if (inFlight)
        {
            ClearBalancesSaveIntent(targetPath);
        }

		return EnsureTypedFileOrRestore(targetPath, RECOVERY_BALANCES);
    }
};
