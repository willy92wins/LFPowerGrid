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
//   2. target exists -> use it.
//   3. .bak.new exists -> restore (previous target between save steps).
//   4. .bak exists -> restore (older snapshot).
//   5. nothing -> return false (caller starts fresh).
//
// Enforce Script has no RenameFile; we use CopyFile + DeleteFile.
// CopyFile destName must be "$profile:" or "$saves:" (vanilla constraint).
//
// Generic AtomicSave<T> is rejected: Enforce generics cannot pass Class<T>
// as parameter, so typed instantiation for read-back verify and .tmp parse
// requires concrete methods per type. ~120 duplicated lines accepted.
// =========================================================

class LFPG_FileUtil
{
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
        if (FileExist(targetPath))
        {
            if (FileExist(bakNewPath)) DeleteFile(bakNewPath);
            if (!CopyFile(targetPath, bakNewPath))
            {
                LFPG_Util.Error("[FileUtil] AtomicSave: stage bak.new failed for " + targetPath);
                DeleteFile(tmpPath);
                return false;
            }
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
        if (FileExist(bakPath)) DeleteFile(bakPath);
        if (FileExist(bakNewPath))
        {
            if (CopyFile(bakNewPath, bakPath))
            {
                DeleteFile(bakNewPath);
            }
            else
            {
                LFPG_Util.Warn("[FileUtil] AtomicSave: bak rotation failed; leaving bak.new for recovery");
            }
        }

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

        if (FileExist(targetPath))
        {
            if (FileExist(bakNewPath)) DeleteFile(bakNewPath);
            if (!CopyFile(targetPath, bakNewPath))
            {
                LFPG_Util.Error("[FileUtil] AtomicSaveSettings: stage bak.new failed");
                DeleteFile(tmpPath);
                return false;
            }
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

        if (FileExist(bakPath)) DeleteFile(bakPath);
        if (FileExist(bakNewPath))
        {
            if (CopyFile(bakNewPath, bakPath))
            {
                DeleteFile(bakNewPath);
            }
            else
            {
                LFPG_Util.Warn("[FileUtil] AtomicSaveSettings: bak rotation failed; leaving bak.new for recovery");
            }
        }
        DeleteFile(tmpPath);

        return true;
    }

    // ---- Player Balances ----
    static bool AtomicSaveBalances(string targetPath, LFPG_BalanceData data)
    {
        string tmpPath    = targetPath + ".tmp";
        string bakPath    = targetPath + ".bak";
        string bakNewPath = targetPath + ".bak.new";

        string err;
        if (!JsonFileLoader<LFPG_BalanceData>.SaveFile(tmpPath, data, err))
        {
            LFPG_Util.Error("[FileUtil] AtomicSaveBalances: failed to write tmp: " + err);
            return false;
        }
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

        if (FileExist(targetPath))
        {
            if (FileExist(bakNewPath)) DeleteFile(bakNewPath);
            if (!CopyFile(targetPath, bakNewPath))
            {
                LFPG_Util.Error("[FileUtil] AtomicSaveBalances: stage bak.new failed");
                DeleteFile(tmpPath);
                return false;
            }
        }

        if (FileExist(targetPath)) DeleteFile(targetPath);
        if (!CopyFile(tmpPath, targetPath))
        {
            LFPG_Util.Error("[FileUtil] AtomicSaveBalances: promote tmp->target failed");
            if (FileExist(bakNewPath))
            {
                CopyFile(bakNewPath, targetPath);
            }
            else if (FileExist(bakPath))
            {
                CopyFile(bakPath, targetPath);
            }
            if (!FileExist(targetPath))
            {
                // Nothing was restored: the first save on a fresh install, or both
                // backups gone. The recovery guard in EnsureBalancesFileOrRestore only
                // fires while a target is ALIVE, so a lone .tmp would be promoted on
                // the next boot even though the caller is about to roll this mutation
                // back. Reaching this line means the failure WAS reported upstream, so
                // the snapshot must lose the auto-promotable name. A crash inside the
                // delete/copy window never gets here, which is why that case still
                // promotes: nothing was reported to anyone.
                PreserveOrphanTmpEvidence(tmpPath);
                if (FileExist(tmpPath))
                {
                    // Last resort. If this also fails there is nothing else the code
                    // can do about the file, so make the state loud: this is the only
                    // remaining path where a rolled-back balance can come back after
                    // a restart, and it needs three consecutive I/O failures to reach.
                    if (!DeleteFile(tmpPath))
                    {
                        LFPG_Util.Error("[FileUtil] AtomicSaveBalances: rejected .tmp could neither be preserved NOR deleted. Next boot WILL promote a balance the runtime rolled back. Admin: delete it by hand before restarting: " + tmpPath);
                    }
                }
            }
            return false;
        }

        if (FileExist(bakPath)) DeleteFile(bakPath);
        if (FileExist(bakNewPath))
        {
            if (CopyFile(bakNewPath, bakPath))
            {
                DeleteFile(bakNewPath);
            }
            else
            {
                LFPG_Util.Warn("[FileUtil] AtomicSaveBalances: bak rotation failed; leaving bak.new for recovery");
            }
        }
        DeleteFile(tmpPath);

        return true;
    }

    // =========================================================
    // RECOVERY ON LOAD - typed helpers + raw fallback
    // =========================================================

    // ---- Raw fallback: target / .bak.new / .bak (NO .tmp handling) ----
    // The 3 typed helpers consume .tmp first (parseable -> promote, else discard).
    // This helper covers the post-.tmp state.
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

    // ---- Rename .tmp orphan to .tmp.preserved.<ts>_<rnd> (PR-A.6) ----
    // Called from PromoteOrphanTmp abort paths. Prevents the next AtomicSave
    // Step 1 (which writes targetPath + ".tmp") from silently overwriting a
    // parseable orphan that recovery couldn't promote safely. The preserved
    // file is admin-recoverable; not auto-consumed by any future Ensure*.
    protected static void PreserveOrphanTmpEvidence(string tmpPath)
    {
        if (!FileExist(tmpPath))
            return;
        float ts = GetGame().GetTickTime();
        int rnd = Math.RandomInt(10000, 99999);
        string preservedPath = tmpPath + ".preserved." + ((int)ts).ToString() + "_" + rnd.ToString();
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
                return PromoteOrphanTmp(targetPath, tmpPath, bakPath, bakNewPath);
            }
            else
            {
                LFPG_Util.Error("[FileUtil] Orphan .tmp unparseable (LFPG_VanillaWireStore): " + parseErr + " - discarding: " + tmpPath);
                DeleteFile(tmpPath);
            }
        }

        return EnsureFileOrRestore(targetPath);
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
                return PromoteOrphanTmp(targetPath, tmpPath, bakPath, bakNewPath);
            }
            else
            {
                LFPG_Util.Error("[FileUtil] Orphan .tmp unparseable (LFPG_ServerSettings): " + parseErr + " - discarding: " + tmpPath);
                DeleteFile(tmpPath);
            }
        }

        return EnsureFileOrRestore(targetPath);
    }

    // Reads the first numeric top-level version marker without typed JSON load.
    static bool TryReadRawJsonVersion(string targetPath, out int version)
    {
        version = 0;
        if (!FileExist(targetPath))
            return false;

        FileHandle handle = OpenFile(targetPath, FileMode.READ);
        if (handle == 0)
            return false;

        string raw = "";
        string line = "";
        while (FGets(handle, line) > 0)
        {
            raw = raw + line;
        }
        CloseFile(handle);

        string key = "\"ver\"";
        int keyPos = raw.IndexOf(key);
        if (keyPos < 0)
            return false;

        int afterKeyStart = keyPos + key.Length();
        int afterKeyLength = raw.Length() - afterKeyStart;
        if (afterKeyLength <= 0)
            return false;

        string afterKey = raw.Substring(afterKeyStart, afterKeyLength);
        int colonPos = afterKey.IndexOf(":");
        if (colonPos < 0)
            return false;

        int scan = colonPos + 1;
        int textLength = afterKey.Length();
        string ch = "";
        while (scan < textLength)
        {
            ch = afterKey.Get(scan);
            if (ch == " " || ch == "\t" || ch == "\r" || ch == "\n")
                scan = scan + 1;
            else
                break;
        }
        if (scan >= textLength)
            return false;

        int numberStart = scan;
        ch = afterKey.Get(scan);
        if (ch == "-")
            scan = scan + 1;

        bool hasDigit = false;
        while (scan < textLength)
        {
            ch = afterKey.Get(scan);
            bool isDigit = false;
            if (ch == "0" || ch == "1" || ch == "2" || ch == "3" || ch == "4")
                isDigit = true;
            if (ch == "5" || ch == "6" || ch == "7" || ch == "8" || ch == "9")
                isDigit = true;
            if (!isDigit)
                break;
            hasDigit = true;
            scan = scan + 1;
        }
        if (!hasDigit)
            return false;

        int numberLength = scan - numberStart;
        string numberText = afterKey.Substring(numberStart, numberLength);
        version = numberText.ToInt();
        return true;
    }

    // ---- Typed: Player Balances ----
    static bool EnsureBalancesFileOrRestore(string targetPath)
    {
        string tmpPath    = targetPath + ".tmp";
        string bakPath    = targetPath + ".bak";
        string bakNewPath = targetPath + ".bak.new";

        if (FileExist(tmpPath))
        {
            // Balances are the only typed store whose caller rolls the mutation back
            // when the save reports failure (LFPG_BalanceProvider_Native.c AddBalance
            // logs "balance snapshot was not durable" and returns 0). A .tmp sitting
            // next to a LIVE target is therefore a transaction the runtime already
            // told the player was rejected, and promoting it would resurrect it.
            // Promotion stays correct when the target is ABSENT: that is a crash
            // inside the DeleteFile/CopyFile window, where the save was never
            // reported to anyone. Vanilla wires and settings keep promoting in both
            // cases on purpose - their callers only log and keep the new state.
            if (FileExist(targetPath))
            {
                LFPG_Util.Error("[FileUtil] Orphan .tmp beside a live balances target: NOT promoting, the caller rolled this mutation back. Preserving as evidence: " + tmpPath);
                PreserveOrphanTmpEvidence(tmpPath);
                return true;
            }

            LFPG_BalanceData probe = new LFPG_BalanceData();
            string parseErr;
            if (JsonFileLoader<LFPG_BalanceData>.LoadFile(tmpPath, probe, parseErr))
            {
                LFPG_Util.Warn("[FileUtil] Orphan .tmp parses as LFPG_BalanceData, promoting: " + tmpPath);
                return PromoteOrphanTmp(targetPath, tmpPath, bakPath, bakNewPath);
            }
            else
            {
                LFPG_Util.Error("[FileUtil] Orphan .tmp unparseable (LFPG_BalanceData): " + parseErr + " - discarding: " + tmpPath);
                DeleteFile(tmpPath);
            }
        }

        return EnsureFileOrRestore(targetPath);
    }
};
