// =========================================================
// LF_PowerGrid - utility helpers
// =========================================================

class LFPG_Util
{
    protected static void LFPG_LogInternal(int level, string msg)
    {
        if (!LFPG_LOG_ENABLED) return;
        if (level > LFPG_LOG_LEVEL) return;
        Print(LFPG_LOG_PREFIX + msg);
    }

    static void Error(string msg) { LFPG_LogInternal(0, "[ERR] " + msg); }
    static void Warn(string msg)  { LFPG_LogInternal(0, "[WRN] " + msg); }
    static void Info(string msg)  { LFPG_LogInternal(1, msg); }
    static void Debug(string msg) { LFPG_LogInternal(2, msg); }

    // Persistent device id helpers
    static void GenerateDeviceId(out int low, out int high)
    {
        low = Math.RandomInt(1, 2147483647);
        high = Math.RandomInt(1, 2147483647);
    }

    static string MakeDeviceKey(int low, int high)
    {
        if (low == 0 && high == 0)
            return "";
        return low.ToString() + ":" + high.ToString();
    }
};
