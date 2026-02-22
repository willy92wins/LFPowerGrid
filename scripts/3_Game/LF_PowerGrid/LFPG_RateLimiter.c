// =========================================================
// LF_PowerGrid - small per-player rate limiter (anti spam)
// =========================================================

class LFPG_RateLimiter
{
    protected float m_NextAllowed = 0.0;

    bool Allow(float nowSeconds, float cooldownSeconds)
    {
        if (nowSeconds < m_NextAllowed)
            return false;

        m_NextAllowed = nowSeconds + cooldownSeconds;
        return true;
    }

    // Used by periodic cleanup to detect stale entries
    float GetNextAllowed()
    {
        return m_NextAllowed;
    }
};
