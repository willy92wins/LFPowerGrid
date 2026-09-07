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

    // Per-sender rate-limited warn (anti RPT spam from malicious clients).
    // PR-C 2026-05-26: gate for RPC target-mismatch warnings; cooldown is
    // per (sender plain id + key) so different bug-categories do not mask
    // each other. Default 1 s.
    protected static ref map<string, ref LFPG_RateLimiter> s_WarnRateLimits;

    static void RateLimitedWarn(PlayerIdentity sender, string key, string msg, float cooldownSeconds = 1.0)
    {
        if (!sender) { Warn(msg); return; }
        if (!s_WarnRateLimits) s_WarnRateLimits = new map<string, ref LFPG_RateLimiter>();
        string mapKey = sender.GetPlainId() + ":" + key;
        LFPG_RateLimiter limiter = s_WarnRateLimits.Get(mapKey);
        if (!limiter)
        {
            limiter = new LFPG_RateLimiter();
            s_WarnRateLimits.Set(mapKey, limiter);
        }
        float nowSec = GetGame().GetTickTime();
        if (limiter.Allow(nowSec, cooldownSeconds))
            Warn(msg);
    }

    // Log-safe rendering of a player id. Vanilla marks the plaintext id as
    // unusable in logs (3_game/gameplay.c:369-370), but the balances ledger
    // keys on it durably, so the stored value cannot change without a data
    // migration; only its printed form is masked here. The last 4 characters
    // survive so an admin can still correlate lines with a player list.
    static string LogUid(string uid)
    {
        int uidLen = uid.Length();
        if (uidLen <= 4)
            return "***";
        return "***" + uid.Substring(uidLen - 4, 4);
    }

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

    // Merged from LFPG_Bootstrap.c (Phase B refactor)
    static bool s_BootstrapLogged;

    static void LogOnce(string msg)
    {
        if (s_BootstrapLogged) return;
        s_BootstrapLogged = true;
        string fullMsg = LFPG_LOG_PREFIX + msg;
        Print(fullMsg);
    }
};
