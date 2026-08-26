// =========================================================
// LF_PowerGrid - RPC Policy Guard
//
// Central admission and authorization policy for guarded RPCs.
// Unknown policies are denied by default.
// =========================================================

class LFPG_RPCGuard
{
    static const int POLICY_INSPECT_READ = 1;
    static const int POLICY_WIRING_MUTATION = 2;
    static const int POLICY_SYNC_READ = 3;
    static const int POLICY_DIAGNOSTIC = 4;
    static const int POLICY_CCTV_SESSION = 5;
    static const int POLICY_SORTER_DOSSIER_FROZEN = 6;
    static const int POLICY_SEARCHLIGHT_DEVICE_OR_SESSION = 7;
    static const int POLICY_BTC_READ_OR_MUTATION = 8;

    // Inbound subId -> one of the eight policies. 0 = not classified (deny).
    // Outbound enum values are not inbound and are not listed.
    static int PolicyForSubId(int subId)
    {
        if (subId == LFPG_RPC_SubId.FINISH_WIRING)
            return POLICY_WIRING_MUTATION;
        if (subId == LFPG_RPC_SubId.CUT_WIRES)
            return POLICY_WIRING_MUTATION;
        if (subId == LFPG_RPC_SubId.CUT_PORT)
            return POLICY_WIRING_MUTATION;

        if (subId == LFPG_RPC_SubId.REQUEST_FULL_SYNC)
            return POLICY_SYNC_READ;
        if (subId == LFPG_RPC_SubId.REQUEST_DEVICE_SYNC)
            return POLICY_SYNC_READ;
        if (subId == LFPG_RPC_SubId.REQUEST_DEVICE_SYNC_BATCH)
            return POLICY_SYNC_READ;

        if (subId == LFPG_RPC_SubId.DIAG_CLIENT_LOG)
            return POLICY_DIAGNOSTIC;

        if (subId == LFPG_RPC_SubId.INSPECT_DEVICE)
            return POLICY_INSPECT_READ;

        if (subId == LFPG_RPC_SubId.CCTV_EXIT_REQUEST)
            return POLICY_CCTV_SESSION;
        if (subId == LFPG_RPC_SubId.CAMERA_CYCLE)
            return POLICY_CCTV_SESSION;
        if (subId == LFPG_RPC_SubId.CAMERA_UNLINK)
            return POLICY_CCTV_SESSION;
        if (subId == LFPG_RPC_SubId.REQUEST_CAMERA_LIST)
            return POLICY_CCTV_SESSION;
        if (subId == LFPG_RPC_SubId.CCTV_AIM)
            return POLICY_CCTV_SESSION;

        if (subId == LFPG_RPC_SubId.SORTER_CONFIG_REQUEST)
            return POLICY_SORTER_DOSSIER_FROZEN;
        if (subId == LFPG_RPC_SubId.SORTER_TEST_CONFIG_REQUEST)
            return POLICY_SORTER_DOSSIER_FROZEN;
        if (subId == LFPG_RPC_SubId.SORTER_CONFIG_SAVE)
            return POLICY_SORTER_DOSSIER_FROZEN;
        if (subId == LFPG_RPC_SubId.SORTER_TEST_CONFIG_SAVE)
            return POLICY_SORTER_DOSSIER_FROZEN;
        if (subId == LFPG_RPC_SubId.SORTER_REQUEST_SORT)
            return POLICY_SORTER_DOSSIER_FROZEN;
        if (subId == LFPG_RPC_SubId.SORTER_TEST_REQUEST_SORT)
            return POLICY_SORTER_DOSSIER_FROZEN;
        if (subId == LFPG_RPC_SubId.SORTER_RESYNC)
            return POLICY_SORTER_DOSSIER_FROZEN;
        if (subId == LFPG_RPC_SubId.SORTER_TEST_RESYNC)
            return POLICY_SORTER_DOSSIER_FROZEN;
        if (subId == LFPG_RPC_SubId.SORTER_PREVIEW_REQUEST)
            return POLICY_SORTER_DOSSIER_FROZEN;
        if (subId == LFPG_RPC_SubId.SORTER_TEST_PREVIEW_REQUEST)
            return POLICY_SORTER_DOSSIER_FROZEN;

        if (subId == LFPG_RPC_SubId.SEARCHLIGHT_AIM)
            return POLICY_SEARCHLIGHT_DEVICE_OR_SESSION;
        if (subId == LFPG_RPC_SubId.SEARCHLIGHT_ENTER)
            return POLICY_SEARCHLIGHT_DEVICE_OR_SESSION;
        if (subId == LFPG_RPC_SubId.SEARCHLIGHT_EXIT_REQUEST)
            return POLICY_SEARCHLIGHT_DEVICE_OR_SESSION;
        if (subId == LFPG_RPC_SubId.SEARCHLIGHT_EXIT_V2)
            return POLICY_SEARCHLIGHT_DEVICE_OR_SESSION;

        if (subId == LFPG_RPC_SubId.BTC_OPEN_REQUEST)
            return POLICY_BTC_READ_OR_MUTATION;
        if (subId == LFPG_RPC_SubId.BTC_BUY)
            return POLICY_BTC_READ_OR_MUTATION;
        if (subId == LFPG_RPC_SubId.BTC_SELL)
            return POLICY_BTC_READ_OR_MUTATION;
        if (subId == LFPG_RPC_SubId.BTC_WITHDRAW)
            return POLICY_BTC_READ_OR_MUTATION;
        if (subId == LFPG_RPC_SubId.BTC_DEPOSIT)
            return POLICY_BTC_READ_OR_MUTATION;
        if (subId == LFPG_RPC_SubId.BTC_WITHDRAW_CASH)
            return POLICY_BTC_READ_OR_MUTATION;
        if (subId == LFPG_RPC_SubId.BTC_DEPOSIT_CASH)
            return POLICY_BTC_READ_OR_MUTATION;

        return 0;
    }

    static bool Admit(int policyId, PlayerBase player, PlayerIdentity sender)
    {
        if (!RoutePolicy(policyId))
        {
            LFPG_Util.RateLimitedWarn(sender, "rpc_guard_admit_unknown_policy", "[LFPG_RPCGuard] Admission denied: unknown policy=" + policyId.ToString());
            return false;
        }

        if (!CheckIdentity(policyId, player, sender, "admit"))
            return false;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            string rateKey = RateLimitKey("admit", "rate_limit", policyId);
            string rateMsg = "[LFPG_RPCGuard] ";
            rateMsg = rateMsg + PolicyName(policyId);
            rateMsg = rateMsg + " admission denied: rate limit";
            LFPG_Util.RateLimitedWarn(sender, rateKey, rateMsg);
            return false;
        }

        return true;
    }

    static bool Authorize(int policyId, PlayerBase player, PlayerIdentity sender, int netLow, int netHigh, out EntityAI target, out string canonicalDeviceId)
    {
        target = null;
        canonicalDeviceId = "";

        if (!RoutePolicy(policyId))
        {
            LFPG_Util.RateLimitedWarn(sender, "rpc_guard_auth_unknown_policy", "[LFPG_RPCGuard] Authorization denied: unknown policy=" + policyId.ToString());
            return false;
        }

        if (!CheckIdentity(policyId, player, sender, "auth"))
            return false;

        if (netLow == 0 && netHigh == 0)
        {
            string zeroKey = RateLimitKey("auth", "zero_network_id", policyId);
            string zeroMsg = "[LFPG_RPCGuard] ";
            zeroMsg = zeroMsg + PolicyName(policyId);
            zeroMsg = zeroMsg + " denied: NetworkID is 0/0";
            LFPG_Util.RateLimitedWarn(sender, zeroKey, zeroMsg);
            return false;
        }

        Object rawTarget = g_Game.GetObjectByNetworkId(netLow, netHigh);
        if (!rawTarget)
        {
            string unresolvedKey = RateLimitKey("auth", "unresolved_network_id", policyId);
            string unresolvedMsg = "[LFPG_RPCGuard] ";
            unresolvedMsg = unresolvedMsg + PolicyName(policyId);
            unresolvedMsg = unresolvedMsg + " denied: NetworkID did not resolve";
            LFPG_Util.RateLimitedWarn(sender, unresolvedKey, unresolvedMsg);
            return false;
        }

        EntityAI resolvedTarget = EntityAI.Cast(rawTarget);
        if (!resolvedTarget)
        {
            string nonEntityKey = RateLimitKey("auth", "non_entity_target", policyId);
            string nonEntityMsg = "[LFPG_RPCGuard] ";
            nonEntityMsg = nonEntityMsg + PolicyName(policyId);
            nonEntityMsg = nonEntityMsg + " denied: target is not EntityAI";
            LFPG_Util.RateLimitedWarn(sender, nonEntityKey, nonEntityMsg);
            return false;
        }

        if (resolvedTarget.IsRuined())
        {
            string ruinedKey = RateLimitKey("auth", "ruined_target", policyId);
            string ruinedMsg = "[LFPG_RPCGuard] ";
            ruinedMsg = ruinedMsg + PolicyName(policyId);
            ruinedMsg = ruinedMsg + " denied: target is ruined";
            LFPG_Util.RateLimitedWarn(sender, ruinedKey, ruinedMsg);
            return false;
        }

        string resolvedDeviceId = LFPG_DeviceAPI.GetDeviceId(resolvedTarget);
        if (resolvedDeviceId == "")
        {
            string emptyIdKey = RateLimitKey("auth", "empty_device_id", policyId);
            string emptyIdMsg = "[LFPG_RPCGuard] ";
            emptyIdMsg = emptyIdMsg + PolicyName(policyId);
            emptyIdMsg = emptyIdMsg + " denied: target has no canonical deviceId";
            LFPG_Util.RateLimitedWarn(sender, emptyIdKey, emptyIdMsg);
            return false;
        }

        float distanceSq = LFPG_WorldUtil.DistSq(player.GetPosition(), resolvedTarget.GetPosition());
        float interactRadiusSq = LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M;
        if (distanceSq > interactRadiusSq)
        {
            string farKey = RateLimitKey("auth", "target_too_far", policyId);
            string farMsg = "[LFPG_RPCGuard] ";
            farMsg = farMsg + PolicyName(policyId);
            farMsg = farMsg + " denied: target is beyond interaction distance";
            LFPG_Util.RateLimitedWarn(sender, farKey, farMsg);
            return false;
        }

        target = resolvedTarget;
        canonicalDeviceId = resolvedDeviceId;
        return true;
    }

    protected static bool RoutePolicy(int policyId)
    {
        if (policyId == POLICY_INSPECT_READ)
            return true;
        if (policyId == POLICY_WIRING_MUTATION)
            return true;
        if (policyId == POLICY_SYNC_READ)
            return true;
        if (policyId == POLICY_DIAGNOSTIC)
            return true;
        if (policyId == POLICY_CCTV_SESSION)
            return true;
        if (policyId == POLICY_SORTER_DOSSIER_FROZEN)
            return true;
        if (policyId == POLICY_SEARCHLIGHT_DEVICE_OR_SESSION)
            return true;
        if (policyId == POLICY_BTC_READ_OR_MUTATION)
            return true;

        return false;
    }

    protected static string PolicyName(int policyId)
    {
        if (policyId == POLICY_INSPECT_READ)
            return "INSPECT_READ";
        if (policyId == POLICY_WIRING_MUTATION)
            return "WIRING_MUTATION";
        if (policyId == POLICY_SYNC_READ)
            return "SYNC_READ";
        if (policyId == POLICY_DIAGNOSTIC)
            return "DIAGNOSTIC";
        if (policyId == POLICY_CCTV_SESSION)
            return "CCTV_SESSION";
        if (policyId == POLICY_SORTER_DOSSIER_FROZEN)
            return "SORTER_DOSSIER_FROZEN";
        if (policyId == POLICY_SEARCHLIGHT_DEVICE_OR_SESSION)
            return "SEARCHLIGHT_DEVICE_OR_SESSION";
        if (policyId == POLICY_BTC_READ_OR_MUTATION)
            return "BTC_READ_OR_MUTATION";

        string name = "policy=";
        name = name + policyId.ToString();
        return name;
    }

    // Session-memory rate-limit key: rpc_guard_<flow>_<slug>_p<policyId>.
    protected static string RateLimitKey(string flow, string slug, int policyId)
    {
        string key = "rpc_guard_";
        key = key + flow;
        key = key + "_";
        key = key + slug;
        key = key + "_p";
        key = key + policyId.ToString();
        return key;
    }

    // Shared sender/player/mismatch checks. flow is "admit" or "auth".
    protected static bool CheckIdentity(int policyId, PlayerBase player, PlayerIdentity sender, string flow)
    {
        string policyLabel = PolicyName(policyId);
        string denyVerb;
        if (flow == "admit")
            denyVerb = "admission denied";
        else
            denyVerb = "denied";

        string key;
        string msg;

        if (!sender)
        {
            key = RateLimitKey(flow, "sender_missing", policyId);
            msg = "[LFPG_RPCGuard] ";
            msg = msg + policyLabel;
            msg = msg + " ";
            msg = msg + denyVerb;
            msg = msg + ": sender missing";
            LFPG_Util.RateLimitedWarn(sender, key, msg);
            return false;
        }

        if (!player)
        {
            key = RateLimitKey(flow, "player_missing", policyId);
            msg = "[LFPG_RPCGuard] ";
            msg = msg + policyLabel;
            msg = msg + " ";
            msg = msg + denyVerb;
            msg = msg + ": player missing";
            LFPG_Util.RateLimitedWarn(sender, key, msg);
            return false;
        }

        PlayerBase senderPlayer = PlayerBase.Cast(sender.GetPlayer());
        if (!senderPlayer || senderPlayer != player)
        {
            key = RateLimitKey(flow, "identity_mismatch", policyId);
            msg = "[LFPG_RPCGuard] ";
            msg = msg + policyLabel;
            msg = msg + " ";
            msg = msg + denyVerb;
            msg = msg + ": sender/player mismatch";
            LFPG_Util.RateLimitedWarn(sender, key, msg);
            return false;
        }

        return true;
    }
};
