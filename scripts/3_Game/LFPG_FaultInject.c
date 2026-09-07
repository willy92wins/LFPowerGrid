// =========================================================
// LF_PowerGrid - T5 monetary fault-injection instrument
//
// INSTRUMENT, not a test runner. Armed only when ALL of:
//   1. LFPG_FAULTINJECT_COMPILE_GATE is true (DIAG_DEVELOPER build i.e. DayZDiag; false on
//      retail dedicated — deploying this tree as-is to production
//      does nothing, even if a JSON file is present)
//   2. $profile:LF_PowerGrid/LF_FaultInject.json exists
//   3. armPhrase matches exactly
//   4. enabled is true
//
// This file is NEVER auto-created. BTCConfig.Save() does not touch it.
// remaining is consumed per matching hook so a later save in the same
// boot (e.g. AddBalance rollback) is not poisoned.
// =========================================================

#ifdef DIAG_DEVELOPER
static const bool LFPG_FAULTINJECT_COMPILE_GATE = true;
#else
static const bool LFPG_FAULTINJECT_COMPILE_GATE = false;
#endif

static const string LFPG_FAULTINJECT_ARM_PHRASE = "I_UNDERSTAND_THIS_DESTROYS_MONEY";
static const string LFPG_FAULTINJECT_FILE = "$profile:LF_PowerGrid\\LF_FaultInject.json";
static const string LFPG_FAULTINJECT_FIRED = "$profile:LF_PowerGrid\\LF_FaultInject.fired";

class LFPG_FaultInjectFile
{
    string armPhrase;
    bool enabled;
    string scenario;
    int remaining;

    void LFPG_FaultInjectFile()
    {
        armPhrase = "";
        enabled = false;
        scenario = "";
        remaining = 0;
    }
};

class LFPG_FaultInject
{
    protected static ref LFPG_FaultInjectFile s_File;
    protected static bool s_Loaded;
    protected static bool s_ObserveArmed;
    protected static bool s_FailArmed;
    protected static string s_Scenario;
    protected static int s_Remaining;
    protected static bool s_Announced;

    static void Touch()
    {
        if (!LFPG_FAULTINJECT_COMPILE_GATE)
            return;
        EnsureLoaded();
    }

    static bool ShouldFail(string hookId)
    {
        if (!LFPG_FAULTINJECT_COMPILE_GATE)
            return false;
        EnsureLoaded();
        if (!s_FailArmed)
            return false;
        if (s_Remaining <= 0)
            return false;
        if (!HookMatches(hookId))
            return false;
        s_Remaining = s_Remaining - 1;
        LogFire(hookId, "fail");
        return true;
    }

    static bool ShouldCrash(string hookId)
    {
        if (!LFPG_FAULTINJECT_COMPILE_GATE)
            return false;
        EnsureLoaded();
        if (!s_FailArmed)
            return false;
        if (s_Remaining <= 0)
            return false;
        if (!HookMatches(hookId))
            return false;
        s_Remaining = s_Remaining - 1;
        LogFire(hookId, "crash");
        CrashProcess(hookId);
        return true;
    }

    static void ObserveTx(int txType, int errCode, int newStock, int newBalance, int btcMoved, int cashOnInv, int btcOnInv)
    {
        if (!LFPG_FAULTINJECT_COMPILE_GATE)
            return;
        EnsureLoaded();
        if (!s_ObserveArmed)
            return;
        string line = "LFPG_FAULTINJECT OBSERVE tx=";
        line = line + txType.ToString();
        line = line + " err=";
        line = line + errCode.ToString();
        line = line + " stock=";
        line = line + newStock.ToString();
        line = line + " bal=";
        line = line + newBalance.ToString();
        line = line + " moved=";
        line = line + btcMoved.ToString();
        line = line + " cash=";
        line = line + cashOnInv.ToString();
        line = line + " btc=";
        line = line + btcOnInv.ToString();
        LFPG_Util.Error(line);
    }

    static void ObserveReconcile(int current, int balanceBefore, int creditAmount, int destroyed, int requested)
    {
        if (!LFPG_FAULTINJECT_COMPILE_GATE)
            return;
        EnsureLoaded();
        if (!s_ObserveArmed)
            return;
        string line = "LFPG_FAULTINJECT OBSERVE reconcile current=";
        line = line + current.ToString();
        line = line + " before=";
        line = line + balanceBefore.ToString();
        line = line + " credit=";
        line = line + creditAmount.ToString();
        line = line + " destroyed=";
        line = line + destroyed.ToString();
        line = line + " requested=";
        line = line + requested.ToString();
        LFPG_Util.Error(line);
    }

    protected static void EnsureLoaded()
    {
        if (s_Loaded)
            return;
        s_Loaded = true;
        s_ObserveArmed = false;
        s_FailArmed = false;
        s_Scenario = "";
        s_Remaining = 0;
        s_File = new LFPG_FaultInjectFile();

        if (!FileExist(LFPG_FAULTINJECT_FILE))
        {
            Announce("DISARMED reason=no_file");
            return;
        }

        string err;
        bool loadOk = JsonFileLoader<LFPG_FaultInjectFile>.LoadFile(LFPG_FAULTINJECT_FILE, s_File, err);
        if (!loadOk || !s_File)
        {
            Announce("DISARMED reason=parse");
            return;
        }

        if (!s_File.enabled)
        {
            Announce("DISARMED reason=enabled_false");
            return;
        }
        if (s_File.armPhrase != LFPG_FAULTINJECT_ARM_PHRASE)
        {
            Announce("DISARMED reason=bad_phrase");
            return;
        }
        if (s_File.scenario == "")
        {
            Announce("DISARMED reason=empty_scenario");
            return;
        }

        s_Scenario = s_File.scenario;
        s_Remaining = s_File.remaining;
        s_ObserveArmed = true;
        if (s_Remaining > 0)
            s_FailArmed = true;

        string armLine = "ARMED scenario=";
        armLine = armLine + s_Scenario;
        armLine = armLine + " remaining=";
        armLine = armLine + s_Remaining.ToString();
        Announce(armLine);
    }

    protected static void Announce(string body)
    {
        if (s_Announced)
            return;
        s_Announced = true;
        string line = "LFPG_FAULTINJECT ";
        line = line + body;
        LFPG_Util.Error(line);
    }

    protected static void LogFire(string hookId, string mode)
    {
        string line = "LFPG_FAULTINJECT FIRE hook=";
        line = line + hookId;
        line = line + " mode=";
        line = line + mode;
        line = line + " scenario=";
        line = line + s_Scenario;
        line = line + " remainingAfter=";
        line = line + s_Remaining.ToString();
        LFPG_Util.Error(line);
    }

    protected static bool HookMatches(string hookId)
    {
        if (s_Scenario == hookId)
            return true;

        if (s_Scenario == "E04_save_false_after_spill")
        {
            if (hookId == "AtomicSaveBalances_false")
                return true;
            return false;
        }
        if (s_Scenario == "E04_crash_after_credit")
        {
            if (hookId == "E04_crash_after_credit")
                return true;
            return false;
        }
        if (s_Scenario == "E04_crash_after_marker_before_credit")
        {
            if (hookId == "E04_crash_after_marker_before_credit")
                return true;
            return false;
        }
        if (s_Scenario == "E04_delete_marker_fail")
        {
            if (hookId == "E04_clear_after_destroy")
                return true;
            return false;
        }
        if (s_Scenario == "E04_clear_and_rewrite_fail")
        {
            if (hookId == "E04_clear_after_destroy")
                return true;
            if (hookId == "E04_rewrite_after_destroy")
                return true;
            return false;
        }
        if (s_Scenario == "SEC09_savefile_abort_leave_tmp")
        {
            if (hookId == "SaveFile_abort_leave_tmp")
                return true;
            return false;
        }
        if (s_Scenario == "SEC09_crash_delete_copy_window")
        {
            if (hookId == "balances_after_delete_before_promote")
                return true;
            return false;
        }
        if (s_Scenario == "SEC09_copyfile_promote_fail")
        {
            if (hookId == "CopyFile_promote")
                return true;
            return false;
        }
        if (s_Scenario == "SEC09_residual_b")
        {
            if (hookId == "CopyFile_promote")
                return true;
            if (hookId == "DeleteFile_saving")
                return true;
            if (hookId == "CopyFile_restore")
                return true;
            if (hookId == "DiscardAbortedTmp_false")
                return true;
            return false;
        }
        if (s_Scenario == "E16_crash_after_apply")
        {
            if (hookId == "E16_crash_after_apply")
                return true;
            return false;
        }
        if (s_Scenario == "E05_addbalance_partial")
        {
            if (hookId == "AddBalance_partial")
                return true;
            return false;
        }
        return false;
    }

    protected static void CrashProcess(string hookId)
    {
        LFPG_Util.Error("LFPG_FAULTINJECT CRASH_NOW do not RequestExit; orchestrator SIGKILLs if this process still lives");

        FileHandle firedHandle;
        firedHandle = OpenFile(LFPG_FAULTINJECT_FIRED, FileMode.WRITE);
        if (firedHandle != 0)
        {
            FPrintln(firedHandle, hookId);
            CloseFile(firedHandle);
        }

        // Native crash. RequestExit is forbidden here: it can flush hive
        // and close the window this instrument exists to open.
        Object gone;
        vector crashPos;
        crashPos = gone.GetPosition();
        gone.SetPosition(crashPos);

        int sink = 0;
        while (true)
        {
            sink = sink + 1;
        }
    }
};
