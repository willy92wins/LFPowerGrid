// =========================================================
// LF_PowerGrid - Runtime control-session registry
// Mission-owned by LFPG_NetworkManager; never persisted.
// =========================================================

static const int LFPG_CONTROL_KIND_CCTV = 1;
static const int LFPG_CONTROL_KIND_SEARCHLIGHT = 2;

static const int LFPG_CONTROL_STATE_ENTERING = 1;
static const int LFPG_CONTROL_STATE_ACTIVE = 2;
static const int LFPG_CONTROL_STATE_EXITING = 3;

// CCTV keeps its 120 s window plus 5 s exit-confirm grace.
// Searchlights arm this deadline only after an exit becomes terminal-pending.
static const int LFPG_CONTROL_SESSION_TIMEOUT_MS = 125000;

class LFPG_ControlSessionRecord
{
    string m_UID;
    int m_Kind;
    int m_State;

    PlayerIdentity m_Identity;
    PlayerBase m_Player;

    LFPG_Monitor m_Monitor;
    LFPG_Searchlight m_Searchlight;

    int m_DeviceNetLow;
    int m_DeviceNetHigh;
    int m_PlayerNetLow;
    int m_PlayerNetHigh;
    int m_DeadlineMs;

    int m_CameraCount;
    ref array<vector> m_CameraPositions;
    ref array<vector> m_CameraOrientations;
    ref array<string> m_CameraLabels;
    ref array<int> m_CameraNetLows;
    ref array<int> m_CameraNetHighs;
    ref array<string> m_CameraDeviceIds;
    ref array<float> m_CameraYaws;
    ref array<float> m_CameraPitches;

    float m_SearchlightYaw;
    float m_SearchlightPitch;

    // Per-session AIM limiter. Allocated once in BeginSearchlight; never on the AIM path.
    ref LFPG_RateLimiter m_AimLimiter;
    // Per-session CCTV replay limiter. Allocated once in BeginCCTV; never on the replay path.
    ref LFPG_RateLimiter m_ReplayLimiter;
};

class LFPG_ControlSessionRegistry
{
    protected ref map<string, ref LFPG_ControlSessionRecord> m_ByUID;
    protected ref array<string> m_TerminalUIDs;

    void LFPG_ControlSessionRegistry()
    {
        m_ByUID = new map<string, ref LFPG_ControlSessionRecord>;
        m_TerminalUIDs = new array<string>;
    }

    LFPG_ControlSessionRecord Get(PlayerIdentity identity)
    {
        if (!identity)
            return null;

        string uid = identity.GetPlainId();
        if (uid == "")
            return null;

        return m_ByUID.Get(uid);
    }

    bool Matches(LFPG_ControlSessionRecord record, int kind, int deviceNetLow, int deviceNetHigh)
    {
        if (!record)
            return false;
        if (record.m_Kind != kind)
            return false;
        if (record.m_DeviceNetLow != deviceNetLow)
            return false;
        if (record.m_DeviceNetHigh != deviceNetHigh)
            return false;
        if (record.m_State != LFPG_CONTROL_STATE_ENTERING && record.m_State != LFPG_CONTROL_STATE_ACTIVE)
            return false;
        return true;
    }

    LFPG_ControlSessionRecord BeginCCTV(PlayerIdentity identity, PlayerBase player, LFPG_Monitor monitor, int deviceNetLow, int deviceNetHigh, int cameraCount, array<vector> cameraPositions, array<vector> cameraOrientations, array<string> cameraLabels, array<int> cameraNetLows, array<int> cameraNetHighs, array<string> cameraDeviceIds, array<float> cameraYaws, array<float> cameraPitches)
    {
        if (!identity || !player || !monitor)
            return null;

        string uid = identity.GetPlainId();
        if (uid == "" || m_ByUID.Contains(uid))
            return null;

        LFPG_ControlSessionRecord record = new LFPG_ControlSessionRecord();
        record.m_UID = uid;
        record.m_Kind = LFPG_CONTROL_KIND_CCTV;
        record.m_State = LFPG_CONTROL_STATE_ENTERING;
        record.m_Identity = identity;
        record.m_Player = player;
        record.m_Monitor = monitor;
        record.m_DeviceNetLow = deviceNetLow;
        record.m_DeviceNetHigh = deviceNetHigh;
        record.m_DeadlineMs = g_Game.GetTime() + LFPG_CONTROL_SESSION_TIMEOUT_MS;
        record.m_CameraCount = cameraCount;
        record.m_CameraPositions = cameraPositions;
        record.m_CameraOrientations = cameraOrientations;
        record.m_CameraLabels = cameraLabels;
        record.m_CameraNetLows = cameraNetLows;
        record.m_CameraNetHighs = cameraNetHighs;
        record.m_CameraDeviceIds = cameraDeviceIds;
        record.m_CameraYaws = cameraYaws;
        record.m_CameraPitches = cameraPitches;
        record.m_AimLimiter = new LFPG_RateLimiter();
        record.m_ReplayLimiter = new LFPG_RateLimiter();
        m_ByUID.Set(uid, record);
        return record;
    }
    LFPG_ControlSessionRecord BeginSearchlight(PlayerIdentity identity, PlayerBase player, LFPG_Searchlight searchlight, int deviceNetLow, int deviceNetHigh, int playerNetLow, int playerNetHigh, float yaw, float pitch)
    {
        if (!identity || !player || !searchlight)
            return null;
        if (playerNetLow == 0 && playerNetHigh == 0)
            return null;

        string uid = identity.GetPlainId();
        if (uid == "" || m_ByUID.Contains(uid))
            return null;

        LFPG_ControlSessionRecord record = new LFPG_ControlSessionRecord();
        record.m_UID = uid;
        record.m_Kind = LFPG_CONTROL_KIND_SEARCHLIGHT;
        record.m_State = LFPG_CONTROL_STATE_ENTERING;
        record.m_Identity = identity;
        record.m_Player = player;
        record.m_Searchlight = searchlight;
        record.m_DeviceNetLow = deviceNetLow;
        record.m_DeviceNetHigh = deviceNetHigh;
        record.m_PlayerNetLow = playerNetLow;
        record.m_PlayerNetHigh = playerNetHigh;
        record.m_DeadlineMs = 0;
        record.m_SearchlightYaw = yaw;
        record.m_SearchlightPitch = pitch;
        record.m_AimLimiter = new LFPG_RateLimiter();
        m_ByUID.Set(uid, record);
        return record;
    }

    bool AllowSearchlightAim(LFPG_ControlSessionRecord record, float nowSeconds)
    {
        if (!record || !record.m_AimLimiter)
            return false;

        return record.m_AimLimiter.Allow(nowSeconds, LFPG_SEARCHLIGHT_AIM_COOLDOWN_S);
    }

    bool AllowCCTVReplay(LFPG_ControlSessionRecord record, float nowSeconds)
    {
        if (!record || !record.m_ReplayLimiter)
            return false;

        return record.m_ReplayLimiter.Allow(nowSeconds, LFPG_CCTV_REPLAY_COOLDOWN_S);
    }

    bool AllowCCTVAim(LFPG_ControlSessionRecord record, float nowSeconds)
    {
        if (!record || !record.m_AimLimiter)
            return false;

        return record.m_AimLimiter.Allow(nowSeconds, LFPG_CCTV_AIM_COOLDOWN_S);
    }

    int FindCCTVCameraIndex(LFPG_ControlSessionRecord record, int cameraNetLow, int cameraNetHigh)
    {
        if (!record || record.m_Kind != LFPG_CONTROL_KIND_CCTV)
            return -1;
        if (record.m_State != LFPG_CONTROL_STATE_ENTERING && record.m_State != LFPG_CONTROL_STATE_ACTIVE)
            return -1;
        if (!record.m_CameraNetLows || !record.m_CameraNetHighs)
            return -1;

        int cameraIndex = 0;
        while (cameraIndex < record.m_CameraCount)
        {
            if (cameraIndex >= record.m_CameraNetLows.Count() || cameraIndex >= record.m_CameraNetHighs.Count())
                return -1;
            if (record.m_CameraNetLows[cameraIndex] == cameraNetLow && record.m_CameraNetHighs[cameraIndex] == cameraNetHigh)
                return cameraIndex;
            cameraIndex = cameraIndex + 1;
        }
        return -1;
    }

    string GetCCTVCameraDeviceId(LFPG_ControlSessionRecord record, int cameraIndex)
    {
        if (!record || !record.m_CameraDeviceIds)
            return "";
        if (cameraIndex < 0 || cameraIndex >= record.m_CameraDeviceIds.Count())
            return "";
        return record.m_CameraDeviceIds[cameraIndex];
    }

    void UpdateCCTVAimCache(LFPG_ControlSessionRecord record, int cameraIndex, float yaw, float pitch)
    {
        if (!record || !record.m_CameraYaws || !record.m_CameraPitches)
            return;
        if (cameraIndex < 0 || cameraIndex >= record.m_CameraYaws.Count() || cameraIndex >= record.m_CameraPitches.Count())
            return;

        record.m_CameraYaws[cameraIndex] = yaw;
        record.m_CameraPitches[cameraIndex] = pitch;
    }

    void UpdateAllCCTVAimCaches(int cameraNetLow, int cameraNetHigh, float yaw, float pitch)
    {
        int sessionIndex = 0;
        while (sessionIndex < m_ByUID.Count())
        {
            LFPG_ControlSessionRecord record = m_ByUID.GetElement(sessionIndex);
            int cameraIndex = FindCCTVCameraIndex(record, cameraNetLow, cameraNetHigh);
            if (cameraIndex >= 0)
                UpdateCCTVAimCache(record, cameraIndex, yaw, pitch);
            sessionIndex = sessionIndex + 1;
        }
    }

    void MarkActive(LFPG_ControlSessionRecord record)
    {
        if (!record)
            return;
        if (record.m_State != LFPG_CONTROL_STATE_ENTERING)
            return;
        record.m_State = LFPG_CONTROL_STATE_ACTIVE;
    }

    bool ArmSearchlightExitDeadline(PlayerIdentity identity, int deviceNetLow, int deviceNetHigh)
    {
        LFPG_ControlSessionRecord record = Get(identity);
        if (!Matches(record, LFPG_CONTROL_KIND_SEARCHLIGHT, deviceNetLow, deviceNetHigh))
            return false;

        if (record.m_DeadlineMs <= 0)
            record.m_DeadlineMs = g_Game.GetTime() + LFPG_CONTROL_SESSION_TIMEOUT_MS;
        return true;
    }

    bool SendCCTVEnterResponse(LFPG_ControlSessionRecord record)
    {
        if (!record || record.m_Kind != LFPG_CONTROL_KIND_CCTV)
            return false;
        if (!record.m_Identity || !record.m_Player)
            return false;
        if (!record.m_CameraPositions || !record.m_CameraOrientations || !record.m_CameraLabels)
            return false;
        if (!record.m_CameraNetLows || !record.m_CameraNetHighs || !record.m_CameraDeviceIds || !record.m_CameraYaws || !record.m_CameraPitches)
            return false;
        if (record.m_CameraCount <= 0)
            return false;
        if (record.m_CameraPositions.Count() < record.m_CameraCount)
            return false;
        if (record.m_CameraOrientations.Count() < record.m_CameraCount)
            return false;
        if (record.m_CameraLabels.Count() < record.m_CameraCount)
            return false;
        if (record.m_CameraNetLows.Count() < record.m_CameraCount)
            return false;
        if (record.m_CameraNetHighs.Count() < record.m_CameraCount)
            return false;
        if (record.m_CameraDeviceIds.Count() < record.m_CameraCount)
            return false;
        if (record.m_CameraYaws.Count() < record.m_CameraCount)
            return false;
        if (record.m_CameraPitches.Count() < record.m_CameraCount)
            return false;

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.CAMERA_LIST_RESPONSE);
        rpc.Write(record.m_CameraCount);

        int cameraIndex = 0;
        vector writePos = "0 0 0";
        vector writeOri = "0 0 0";
        float writeFloat = 0.0;
        while (cameraIndex < record.m_CameraCount)
        {
            writePos = record.m_CameraPositions[cameraIndex];
            writeFloat = writePos[0];
            rpc.Write(writeFloat);
            writeFloat = writePos[1];
            rpc.Write(writeFloat);
            writeFloat = writePos[2];
            rpc.Write(writeFloat);

            writeOri = record.m_CameraOrientations[cameraIndex];
            writeFloat = writeOri[0];
            rpc.Write(writeFloat);
            writeFloat = writeOri[1];
            rpc.Write(writeFloat);
            writeFloat = writeOri[2];
            rpc.Write(writeFloat);
            rpc.Write(record.m_CameraLabels[cameraIndex]);
            rpc.Write(record.m_CameraNetLows[cameraIndex]);
            rpc.Write(record.m_CameraNetHighs[cameraIndex]);
            rpc.Write(record.m_CameraYaws[cameraIndex]);
            rpc.Write(record.m_CameraPitches[cameraIndex]);
            cameraIndex = cameraIndex + 1;
        }

        rpc.Send(record.m_Player, LFPG_RPC_CHANNEL, true, record.m_Identity);
        return true;
    }

    bool SendSearchlightEnterConfirm(LFPG_ControlSessionRecord record)
    {
        if (!record || record.m_Kind != LFPG_CONTROL_KIND_SEARCHLIGHT)
            return false;
        if (!record.m_Identity || !record.m_Player)
            return false;

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.SEARCHLIGHT_ENTER_CONFIRM);
        rpc.Write(record.m_DeviceNetLow);
        rpc.Write(record.m_DeviceNetHigh);
        rpc.Write(record.m_SearchlightYaw);
        rpc.Write(record.m_SearchlightPitch);
        rpc.Send(record.m_Player, LFPG_RPC_CHANNEL, true, record.m_Identity);
        return true;
    }
    protected bool RemoveCurrent(LFPG_ControlSessionRecord record)
    {
        if (!record || record.m_UID == "")
            return false;

        LFPG_ControlSessionRecord current = m_ByUID.Get(record.m_UID);
        if (current != record)
            return false;

        m_ByUID.Remove(record.m_UID);
        return true;
    }

    protected void SendCCTVExitConfirm(LFPG_ControlSessionRecord record)
    {
        if (!record || !record.m_Identity || !record.m_Player)
            return;

        ScriptRPC confirmRpc = new ScriptRPC();
        confirmRpc.Write((int)LFPG_RPC_SubId.CCTV_EXIT_CONFIRM);
        confirmRpc.Send(record.m_Player, LFPG_RPC_CHANNEL, true, record.m_Identity);
    }

    protected void SendSearchlightExitConfirm(PlayerIdentity identity, PlayerBase player)
    {
        if (!identity || !player)
            return;

        ScriptRPC confirmRpc = new ScriptRPC();
        confirmRpc.Write((int)LFPG_RPC_SubId.SEARCHLIGHT_EXIT_CONFIRM);
        confirmRpc.Send(player, LFPG_RPC_CHANNEL, true, identity);
    }

    protected bool HasRegisteredSearchlightOperator(LFPG_ControlSessionRecord record)
    {
        if (!record || !record.m_Searchlight)
            return false;
        if (record.m_PlayerNetLow == 0 && record.m_PlayerNetHigh == 0)
            return false;
        return record.m_Searchlight.LFPG_IsOperator(record.m_PlayerNetLow, record.m_PlayerNetHigh);
    }

    protected bool FinishCCTV(LFPG_ControlSessionRecord record, bool sendConfirm)
    {
        if (!record || record.m_Kind != LFPG_CONTROL_KIND_CCTV)
            return false;
        if (record.m_State == LFPG_CONTROL_STATE_EXITING)
            return false;

        record.m_State = LFPG_CONTROL_STATE_EXITING;

        // A disconnected identity is represented by a null engine reference.
        // In that case the record is dropped without an invalid SelectPlayer call.
        if (record.m_Identity && record.m_Player)
        {
            g_Game.SelectPlayer(record.m_Identity, record.m_Player);
            if (sendConfirm)
                SendCCTVExitConfirm(record);
        }

        return RemoveCurrent(record);
    }

    protected bool FinishSearchlight(LFPG_ControlSessionRecord record, bool sendConfirm)
    {
        if (!record || record.m_Kind != LFPG_CONTROL_KIND_SEARCHLIGHT)
            return false;
        if (record.m_State == LFPG_CONTROL_STATE_EXITING)
            return false;

        record.m_State = LFPG_CONTROL_STATE_EXITING;

        if (HasRegisteredSearchlightOperator(record))
            record.m_Searchlight.LFPG_ClearOperator();

        if (sendConfirm)
            SendSearchlightExitConfirm(record.m_Identity, record.m_Player);

        return RemoveCurrent(record);
    }

    bool EndCCTV(PlayerIdentity identity, bool sendConfirm)
    {
        LFPG_ControlSessionRecord record = Get(identity);
        if (!record || record.m_Kind != LFPG_CONTROL_KIND_CCTV)
            return false;
        return FinishCCTV(record, sendConfirm);
    }

    bool EndSearchlight(PlayerIdentity identity, int deviceNetLow, int deviceNetHigh, bool sendConfirm)
    {
        LFPG_ControlSessionRecord record = Get(identity);
        if (Matches(record, LFPG_CONTROL_KIND_SEARCHLIGHT, deviceNetLow, deviceNetHigh))
            return FinishSearchlight(record, sendConfirm);

        if (!identity)
            return false;

        Object searchlightObject = g_Game.GetObjectByNetworkId(deviceNetLow, deviceNetHigh);
        LFPG_Searchlight searchlight = LFPG_Searchlight.Cast(searchlightObject);
        if (!searchlight)
            return false;

        PlayerBase operatorPlayer = PlayerBase.Cast(identity.GetPlayer());
        if (!operatorPlayer)
            return false;

        int operatorNetLow = 0;
        int operatorNetHigh = 0;
        operatorPlayer.GetNetworkID(operatorNetLow, operatorNetHigh);
        if (operatorNetLow == 0 && operatorNetHigh == 0)
            return false;
        if (!searchlight.LFPG_IsOperator(operatorNetLow, operatorNetHigh))
            return false;

        searchlight.LFPG_ClearOperator();
        if (sendConfirm)
            SendSearchlightExitConfirm(identity, operatorPlayer);
        return true;
    }
    int Count()
    {
        return m_ByUID.Count();
    }

    void Tick()
    {
        if (!g_Game)
            return;

        m_TerminalUIDs.Clear();
        int nowMs = g_Game.GetTime();
        int sessionIndex = 0;
        while (sessionIndex < m_ByUID.Count())
        {
            string uid = m_ByUID.GetKey(sessionIndex);
            LFPG_ControlSessionRecord record = m_ByUID.GetElement(sessionIndex);
            bool terminal = false;

            if (!record)
            {
                terminal = true;
            }
            else if (record.m_State == LFPG_CONTROL_STATE_ENTERING || record.m_State == LFPG_CONTROL_STATE_ACTIVE)
            {
                if (!record.m_Identity || !record.m_Player)
                {
                    terminal = true;
                }
                else if (!record.m_Player.IsAlive() || record.m_Player.IsUnconscious())
                {
                    terminal = true;
                }
                else if (record.m_Kind == LFPG_CONTROL_KIND_CCTV)
                {
                    if (!record.m_Monitor || !record.m_Monitor.LFPG_IsPowered())
                        terminal = true;
                }
                else if (record.m_Kind == LFPG_CONTROL_KIND_SEARCHLIGHT)
                {
                    if (!record.m_Searchlight || !record.m_Searchlight.LFPG_IsPowered())
                    {
                        terminal = true;
                    }
                    else if (!HasRegisteredSearchlightOperator(record))
                    {
                        terminal = true;
                    }
                }
                else
                {
                    terminal = true;
                }

                if (!terminal && record.m_DeadlineMs > 0 && nowMs >= record.m_DeadlineMs)
                    terminal = true;
            }

            if (terminal)
                m_TerminalUIDs.Insert(uid);
            sessionIndex = sessionIndex + 1;
        }

        int terminalIndex = 0;
        while (terminalIndex < m_TerminalUIDs.Count())
        {
            string terminalUID = m_TerminalUIDs[terminalIndex];
            LFPG_ControlSessionRecord terminalRecord = m_ByUID.Get(terminalUID);
            if (!terminalRecord)
            {
                m_ByUID.Remove(terminalUID);
            }
            else if (terminalRecord.m_Kind == LFPG_CONTROL_KIND_CCTV)
            {
                FinishCCTV(terminalRecord, true);
            }
            else if (terminalRecord.m_Kind == LFPG_CONTROL_KIND_SEARCHLIGHT)
            {
                FinishSearchlight(terminalRecord, true);
            }
            else
            {
                RemoveCurrent(terminalRecord);
            }
            terminalIndex = terminalIndex + 1;
        }
    }
};
