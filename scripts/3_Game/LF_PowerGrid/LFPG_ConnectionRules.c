// =========================================================
// LF_PowerGrid - Connection Rules (v0.7.13 — Sprint 2.5)
//
// Shared pre-connection validation used by BOTH client and server.
// Returns LFPG_PreConnectStatus + human-readable reason.
// NEVER returns colors, UI elements, or side effects.
//
// Client uses this for:
//   - Preview color semaforo (B2)
//   - Pre-validation before sending RPC (B3)
//
// Server uses this for:
//   - Shared rules parity (B4), PLUS server-only checks
//     (quotas, rate-limit, permissions) that are NOT here.
//
// Design: pure functions, no state, no singletons.
// =========================================================

class LFPG_PreConnectResult
{
    int    m_Status;   // LFPG_PreConnectStatus enum value
    string m_Reason;   // Human-readable reason (for HUD / server log)

    void LFPG_PreConnectResult()
    {
        m_Status = LFPG_PreConnectStatus.OK;
        m_Reason = "";
    }

    bool IsValid()
    {
        return (m_Status < 10);
    }

    bool IsWarning()
    {
        return (m_Status >= 1 && m_Status < 10);
    }
};

// v0.7.13: Parameter struct for CanPreConnect.
// Groups all validation inputs into a single object to avoid
// long parameter lists and potential compiler quirks.
class LFPG_PreConnectParams
{
    EntityAI srcEntity;
    string srcDeviceId;
    string srcPort;
    int srcPortDir;
    EntityAI dstEntity;
    string dstDeviceId;
    string dstPort;
    int dstPortDir;
    array<vector> waypoints;
    vector startPos;
    vector endPos;
};

class LFPG_ConnectionRules
{
    // =========================================================
    // CanPreConnect — main validation entry point
    //
    // Takes LFPG_PreConnectParams (all inputs grouped).
    // Returns: LFPG_PreConnectResult with status + reason.
    //
    // NOTE: Port occupancy is informational, NOT blocking.
    //       Occupied ports trigger REPLACEMENT, not rejection.
    //       The server handles replacement logic in HandleLFPG_FinishWiring.
    //       We only flag it as a warning/info for the semaforo.
    // =========================================================
    static LFPG_PreConnectResult CanPreConnect(LFPG_PreConnectParams p)
    {
        LFPG_PreConnectResult result = new LFPG_PreConnectResult();

        // ------- No target -------
        if (!p.dstEntity)
        {
            result.m_Status = LFPG_PreConnectStatus.NO_TARGET;
            result.m_Reason = "No target device";
            return result;
        }

        // ------- Self-connection -------
        if (p.srcDeviceId == p.dstDeviceId && p.srcDeviceId != "")
        {
            result.m_Status = LFPG_PreConnectStatus.INVALID_SELF_CONNECTION;
            result.m_Reason = "Cannot connect device to itself";
            return result;
        }

        // ------- Direction compatibility -------
        // Both ports must have opposite directions (one IN, one OUT)
        if (p.srcPortDir == p.dstPortDir)
        {
            result.m_Status = LFPG_PreConnectStatus.INVALID_SAME_DIRECTION;
            result.m_Reason = "Ports have same direction";
            return result;
        }

        // ------- Device in cargo/inventory -------
        if (p.dstEntity.GetHierarchyParent())
        {
            result.m_Status = LFPG_PreConnectStatus.INVALID_DEVICE_IN_CARGO;
            result.m_Reason = "Device is in inventory";
            return result;
        }

        // ------- Max waypoints -------
        int wpCount = 0;
        if (p.waypoints)
        {
            wpCount = p.waypoints.Count();
        }
        if (wpCount > LFPG_MAX_WAYPOINTS)
        {
            result.m_Status = LFPG_PreConnectStatus.INVALID_MAX_WAYPOINTS;
            result.m_Reason = "Too many waypoints";
            return result;
        }

        // ------- Geometry: segment lengths + total -------
        // Build point chain: startPos -> waypoints -> endPos
        // Check each segment and total length.
        float totalLen = 0.0;
        float maxSegLen = 0.0;
        vector prev = p.startPos;

        int w;
        for (w = 0; w < wpCount; w = w + 1)
        {
            float segLen = vector.Distance(prev, p.waypoints[w]);
            totalLen = totalLen + segLen;
            if (segLen > maxSegLen)
            {
                maxSegLen = segLen;
            }
            prev = p.waypoints[w];
        }

        // Last segment: last waypoint (or startPos) -> endPos
        float lastSegLen = vector.Distance(prev, p.endPos);
        totalLen = totalLen + lastSegLen;
        if (lastSegLen > maxSegLen)
        {
            maxSegLen = lastSegLen;
        }

        // Check hard limits first (INVALID takes priority over warnings)
        if (maxSegLen > LFPG_MAX_SEGMENT_LEN_M)
        {
            result.m_Status = LFPG_PreConnectStatus.INVALID_SEGMENT_TOO_LONG;
            result.m_Reason = "Segment exceeds maximum length";
            return result;
        }

        if (totalLen > LFPG_MAX_WIRE_LEN_M)
        {
            result.m_Status = LFPG_PreConnectStatus.WARN_TOTAL_EXCEEDED;
            result.m_Reason = "Total wire length exceeded";
            return result;
        }

        // ------- Warnings (still valid, but informational) -------
        // Check near-limit warnings (yellow)
        float segThresh = LFPG_MAX_SEGMENT_LEN_M * LFPG_NEAR_LIMIT_RATIO;
        float totalThresh = LFPG_MAX_WIRE_LEN_M * LFPG_NEAR_LIMIT_RATIO;

        if (maxSegLen > segThresh)
        {
            result.m_Status = LFPG_PreConnectStatus.WARN_SEGMENT_NEAR_LIMIT;
            result.m_Reason = "Segment near maximum length";
            return result;
        }

        if (totalLen > totalThresh)
        {
            result.m_Status = LFPG_PreConnectStatus.WARN_TOTAL_NEAR_LIMIT;
            result.m_Reason = "Total wire near maximum length";
            return result;
        }

        // ------- All checks passed -------
        result.m_Status = LFPG_PreConnectStatus.OK;
        return result;
    }

    // =========================================================
    // StatusToPreviewColor — UI helper to map status to ARGB
    //
    // Lives here for convenience but is only called by CLIENT code.
    // The helper itself returns status, not color. This is the
    // single mapping point from logic -> visual.
    // =========================================================
    static int StatusToPreviewColor(int status)
    {
        if (status == LFPG_PreConnectStatus.OK)
        {
            return LFPG_PREVIEW_COLOR_OK;
        }
        else if (status == LFPG_PreConnectStatus.WARN_SEGMENT_NEAR_LIMIT)
        {
            return LFPG_PREVIEW_COLOR_WARN;
        }
        else if (status == LFPG_PreConnectStatus.WARN_TOTAL_NEAR_LIMIT)
        {
            return LFPG_PREVIEW_COLOR_WARN;
        }
        else if (status == LFPG_PreConnectStatus.WARN_TOTAL_EXCEEDED)
        {
            return LFPG_PREVIEW_COLOR_OVER;
        }
        else if (status == LFPG_PreConnectStatus.NO_TARGET)
        {
            return LFPG_PREVIEW_COLOR_NO_TARGET;
        }

        // All INVALID_* statuses (>= 10)
        return LFPG_PREVIEW_COLOR_INVALID;
    }

    // NOTE: FindBestPort was moved to LFPG_WiringClient (4_World) in v0.7.13
    // because it depends on LFPG_DeviceAPI and LFPG_CableRenderer (both 4_World).
    // 3_Game cannot reference 4_World types (compilation order).
};
