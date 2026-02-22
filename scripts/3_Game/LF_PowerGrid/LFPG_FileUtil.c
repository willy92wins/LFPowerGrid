// =========================================================
// LF_PowerGrid - file utility helpers (v0.7.16, Hotfix)
//
// Atomic save pattern for JSON files:
//   1. Save to .tmp
//   2. Verify .tmp readable (skippable via AtomicVerifyReadback setting)
//   3. Backup existing file to .bak
//   4. Copy .tmp → target
//   5. Delete .tmp
//
// On load: if target missing but .bak exists → restore from backup.
//
// Enforce Script has no RenameFile, so we use CopyFile + DeleteFile.
// The window of inconsistency is minimal (copy is fast for small JSON).
//
// NOTE: Only applies to files WE write (vanilla_wires.json, settings).
// DayZ entity persistence (OnStoreSave) is not under our control.
//
// v0.7.16 H4: Read-back verification is now conditional on
//   LFPG_ServerSettings.AtomicVerifyReadback (default true).
// =========================================================

class LFPG_FileUtil
{
    // Atomic save: writes content via a temp file with backup rotation.
    // Returns true if the final file exists after the operation.
    //
    // Uses JsonFileLoader<T> internally. Caller provides the typed
    // object and the target path. This helper handles the temp/backup dance.
    //
    // Usage:
    //   LFPG_FileUtil.AtomicSaveVanillaWires(path, store);
    //   LFPG_FileUtil.AtomicSaveSettings(path, settings);
    //
    // We need separate typed methods because Enforce Script generics
    // don't support passing Class<T> as a parameter.

    // ---- Vanilla Wires ----
    static bool AtomicSaveVanillaWires(string targetPath, LFPG_VanillaWireStore store)
    {
        string tmpPath = targetPath + ".tmp";
        string bakPath = targetPath + ".bak";

        // Step 1: Save to temp file
        string err;
        if (!JsonFileLoader<LFPG_VanillaWireStore>.SaveFile(tmpPath, store, err))
        {
            LFPG_Util.Error("[FileUtil] AtomicSave: failed to write tmp: " + err);
            return false;
        }

        // Step 2: Verify temp file exists and is readable
        if (!FileExist(tmpPath))
        {
            LFPG_Util.Error("[FileUtil] AtomicSave: tmp file not found after write");
            return false;
        }

        // Step 2b: Read-back verification (v0.7.16 H4: skippable via setting)
        LFPG_ServerSettings st = LFPG_Settings.Get();
        bool doReadback = true;
        if (st)
        {
            doReadback = st.AtomicVerifyReadback;
        }
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

        // Step 3: Backup existing file
        if (FileExist(targetPath))
        {
            if (FileExist(bakPath))
            {
                DeleteFile(bakPath);
            }
            CopyFile(targetPath, bakPath);
        }

        // Step 4: Copy tmp → target
        if (FileExist(targetPath))
        {
            DeleteFile(targetPath);
        }
        CopyFile(tmpPath, targetPath);

        // Step 5: Cleanup tmp
        DeleteFile(tmpPath);

        // Final verify
        if (!FileExist(targetPath))
        {
            LFPG_Util.Error("[FileUtil] AtomicSave: target missing after copy! Restoring backup...");
            if (FileExist(bakPath))
            {
                CopyFile(bakPath, targetPath);
                return FileExist(targetPath);
            }
            return false;
        }

        return true;
    }

    // ---- Server Settings ----
    static bool AtomicSaveSettings(string targetPath, LFPG_ServerSettings settings)
    {
        string tmpPath = targetPath + ".tmp";
        string bakPath = targetPath + ".bak";

        // Step 1: Save to temp
        string err;
        if (!JsonFileLoader<LFPG_ServerSettings>.SaveFile(tmpPath, settings, err))
        {
            LFPG_Util.Error("[FileUtil] AtomicSaveSettings: failed to write tmp: " + err);
            return false;
        }

        // Step 2: Verify
        if (!FileExist(tmpPath))
        {
            LFPG_Util.Error("[FileUtil] AtomicSaveSettings: tmp not found after write");
            return false;
        }

        // Step 2b: Read-back verification (v0.7.16 H4: skippable via setting)
        bool doReadbackS = true;
        if (settings)
        {
            doReadbackS = settings.AtomicVerifyReadback;
        }
        if (doReadbackS)
        {
            LFPG_ServerSettings verifyS = new LFPG_ServerSettings();
            string verifyErr;
            if (!JsonFileLoader<LFPG_ServerSettings>.LoadFile(tmpPath, verifyS, verifyErr))
            {
                LFPG_Util.Error("[FileUtil] AtomicSaveSettings: tmp read-back failed: " + verifyErr);
                DeleteFile(tmpPath);
                return false;
            }
        }

        // Step 3: Backup
        if (FileExist(targetPath))
        {
            if (FileExist(bakPath))
            {
                DeleteFile(bakPath);
            }
            CopyFile(targetPath, bakPath);
        }

        // Step 4: Copy
        if (FileExist(targetPath))
        {
            DeleteFile(targetPath);
        }
        CopyFile(tmpPath, targetPath);

        // Step 5: Cleanup
        DeleteFile(tmpPath);

        if (!FileExist(targetPath))
        {
            LFPG_Util.Error("[FileUtil] AtomicSaveSettings: target missing! Restoring backup...");
            if (FileExist(bakPath))
            {
                CopyFile(bakPath, targetPath);
                return FileExist(targetPath);
            }
            return false;
        }

        return true;
    }

    // ---- Backup restore on load ----
    // Call before loading a file. If the target is missing but a .bak
    // exists, restores from backup. Returns true if file is available.
    static bool EnsureFileOrRestore(string targetPath)
    {
        if (FileExist(targetPath))
            return true;

        string bakPath = targetPath + ".bak";
        if (FileExist(bakPath))
        {
            LFPG_Util.Warn("[FileUtil] Target missing, restoring from backup: " + bakPath);
            CopyFile(bakPath, targetPath);
            return FileExist(targetPath);
        }

        return false;
    }
};
