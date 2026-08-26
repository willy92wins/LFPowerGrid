// =========================================================
// LF_PowerGrid - BTC runtime session and replay registry
// Process-lifetime state only. Never persisted.
// =========================================================

static const int LFPG_BTC_NONCE_NEW = 1;
static const int LFPG_BTC_NONCE_REPLAY = 2;
static const int LFPG_BTC_NONCE_IN_FLIGHT = 3;
static const int LFPG_BTC_NONCE_INVALID = 4;
static const int LFPG_BTC_NONCE_STALE = 5;
static const int LFPG_BTC_NONCE_CONFLICT = 6;

static const int LFPG_BTC_TERMINAL_CACHE_SIZE = 64;
static const int LFPG_BTC_REPLAY_WINDOW_MS = 1000;
static const int LFPG_BTC_REPLAY_MAX_PER_WINDOW = 4;
static const int LFPG_BTC_WARN_WINDOW_MS = 60000;
static const int LFPG_BTC_WARN_MAX_PER_WINDOW = 3;
static const int LFPG_BTC_SEQUENCE_CAP = 2000000000;
static const int LFPG_BTC_SEQUENCE_EXHAUSTION_GUARD = 1000;

class LFPG_BTCSessionResponse
{
    int m_TxType;
    int m_ErrCode;
    int m_NewStock;
    int m_NewBalance;
    int m_BtcMoved;
    float m_EurAmount;
    int m_CashOnInventory;
    int m_BtcOnInventory;
    int m_ServerSessionLow;
    int m_ServerSessionHigh;
    int m_Sequence;
};

class LFPG_BTCNonceRecord
{
    int m_ServerSessionLow;
    int m_ServerSessionHigh;
    int m_Sequence;

    int m_SubId;
    int m_AtmNetLow;
    int m_AtmNetHigh;
    int m_Amount;
    bool m_Mode;

    ref LFPG_BTCSessionResponse m_Response;

    bool MatchesFingerprint(int subId, int atmNetLow, int atmNetHigh, int amount, bool mode)
    {
        if (m_SubId != subId)
            return false;
        if (m_AtmNetLow != atmNetLow)
            return false;
        if (m_AtmNetHigh != atmNetHigh)
            return false;
        if (m_Amount != amount)
            return false;
        if (m_Mode != mode)
            return false;
        return true;
    }
};

class LFPG_BTCPlayerSession
{
    string m_UID;
    bool m_Active;
    int m_HighestAcceptedSequence;

    ref LFPG_BTCNonceRecord m_InFlight;
    ref array<ref LFPG_BTCNonceRecord> m_Terminals;

    bool m_ReplayWindowActive;
    int m_ReplayWindowStartMs;
    int m_ReplayResponsesInWindow;

    bool m_WarnWindowActive;
    int m_WarnWindowStartMs;
    int m_WarningsInWindow;

    void LFPG_BTCPlayerSession()
    {
        m_Terminals = new array<ref LFPG_BTCNonceRecord>;
    }
};

class LFPG_BTCSessionRegistry
{
    protected static ref LFPG_BTCSessionRegistry s_Instance;

    protected int m_ServerSessionLow;
    protected int m_ServerSessionHigh;
    protected ref map<string, ref LFPG_BTCPlayerSession> m_ByUID;

    void LFPG_BTCSessionRegistry()
    {
        m_ByUID = new map<string, ref LFPG_BTCPlayerSession>;
        LFPG_Util.GenerateDeviceId(m_ServerSessionLow, m_ServerSessionHigh);
    }

    static LFPG_BTCSessionRegistry Get()
    {
        if (!s_Instance)
            s_Instance = new LFPG_BTCSessionRegistry();
        return s_Instance;
    }
    protected LFPG_BTCPlayerSession GetSession(PlayerIdentity identity)
    {
        if (!identity)
            return null;

        string uid = identity.GetPlainId();
        if (uid == "")
            return null;
        return m_ByUID.Get(uid);
    }

    protected LFPG_BTCPlayerSession GetOrCreateSession(PlayerIdentity identity)
    {
        if (!identity)
            return null;

        string uid = identity.GetPlainId();
        if (uid == "")
            return null;

        LFPG_BTCPlayerSession session = m_ByUID.Get(uid);
        if (session)
            return session;

        session = new LFPG_BTCPlayerSession();
        session.m_UID = uid;
        m_ByUID.Set(uid, session);
        return session;
    }

    bool OpenSession(PlayerIdentity identity, out int serverSessionLow, out int serverSessionHigh, out int highWatermark)
    {
        serverSessionLow = 0;
        serverSessionHigh = 0;
        highWatermark = 0;

        LFPG_BTCPlayerSession session = GetOrCreateSession(identity);
        if (!session)
            return false;

        // Reopen exposes the same process-lifetime pair and preserves replay
        // state. The watermark lets the client continue without nonce reuse.
        session.m_Active = true;

        serverSessionLow = m_ServerSessionLow;
        serverSessionHigh = m_ServerSessionHigh;
        highWatermark = session.m_HighestAcceptedSequence;
        return true;
    }

    protected LFPG_BTCNonceRecord FindTerminal(LFPG_BTCPlayerSession session, int serverSessionLow, int serverSessionHigh, int sequence)
    {
        if (!session || !session.m_Terminals)
            return null;

        int index = session.m_Terminals.Count() - 1;
        while (index >= 0)
        {
            LFPG_BTCNonceRecord terminal = session.m_Terminals[index];
            if (terminal && terminal.m_ServerSessionLow == serverSessionLow && terminal.m_ServerSessionHigh == serverSessionHigh && terminal.m_Sequence == sequence)
                return terminal;
            index = index - 1;
        }
        return null;
    }

    protected void WarnFingerprint(LFPG_BTCPlayerSession session, int sequence, int subId)
    {
        if (!session || !g_Game)
            return;

        int nowMs = g_Game.GetTime();
        if (!session.m_WarnWindowActive || (nowMs - session.m_WarnWindowStartMs) >= LFPG_BTC_WARN_WINDOW_MS)
        {
            session.m_WarnWindowActive = true;
            session.m_WarnWindowStartMs = nowMs;
            session.m_WarningsInWindow = 0;
        }
        if (session.m_WarningsInWindow >= LFPG_BTC_WARN_MAX_PER_WINDOW)
            return;

        session.m_WarningsInWindow = session.m_WarningsInWindow + 1;
        string msg = "[BTCSession] nonce fingerprint conflict uid=";
        msg = msg + session.m_UID;
        msg = msg + " sequence=";
        msg = msg + sequence.ToString();
        msg = msg + " subId=";
        msg = msg + subId.ToString();
        LFPG_Util.Warn(msg);
    }

    protected void WarnSequenceExhaustion(LFPG_BTCPlayerSession session)
    {
        if (!session || !g_Game)
            return;

        int nowMs = g_Game.GetTime();
        if (!session.m_WarnWindowActive || (nowMs - session.m_WarnWindowStartMs) >= LFPG_BTC_WARN_WINDOW_MS)
        {
            session.m_WarnWindowActive = true;
            session.m_WarnWindowStartMs = nowMs;
            session.m_WarningsInWindow = 0;
        }
        if (session.m_WarningsInWindow >= LFPG_BTC_WARN_MAX_PER_WINDOW)
            return;

        session.m_WarningsInWindow = session.m_WarningsInWindow + 1;
        string msg = "[BTCSession] sequence domain nearly exhausted uid=";
        msg = msg + session.m_UID;
        msg = msg + " watermark=";
        msg = msg + session.m_HighestAcceptedSequence.ToString();
        LFPG_Util.Warn(msg);
    }

    protected void SealOrphanedInFlight(LFPG_BTCPlayerSession session)
    {
        if (!session || !session.m_InFlight)
            return;

        LFPG_BTCNonceRecord record = session.m_InFlight;
        LFPG_BTCSessionResponse response = new LFPG_BTCSessionResponse();
        response.m_TxType = LFPG_BTCTxTypeMapper.GetTxTypeForSubId(record.m_SubId);
        response.m_ErrCode = LFPG_BTC_ERR_INVALID;
        response.m_NewStock = 0;
        response.m_NewBalance = 0;
        response.m_BtcMoved = 0;
        response.m_EurAmount = 0.0;
        response.m_CashOnInventory = 0;
        response.m_BtcOnInventory = 0;
        response.m_ServerSessionLow = record.m_ServerSessionLow;
        response.m_ServerSessionHigh = record.m_ServerSessionHigh;
        response.m_Sequence = record.m_Sequence;

        record.m_Response = response;
        session.m_InFlight = null;
        session.m_Terminals.Insert(record);
        while (session.m_Terminals.Count() > LFPG_BTC_TERMINAL_CACHE_SIZE)
        {
            session.m_Terminals.Remove(0);
        }
    }

    int CheckRequest(PlayerIdentity identity, int serverSessionLow, int serverSessionHigh, int sequence, int subId, int atmNetLow, int atmNetHigh, int amount, bool mode, out LFPG_BTCSessionResponse cachedResponse)
    {
        cachedResponse = null;

        LFPG_BTCPlayerSession session = GetSession(identity);
        if (!session || !session.m_Active)
            return LFPG_BTC_NONCE_INVALID;
        if (serverSessionLow != m_ServerSessionLow || serverSessionHigh != m_ServerSessionHigh)
            return LFPG_BTC_NONCE_INVALID;
        if (sequence <= 0)
            return LFPG_BTC_NONCE_INVALID;
        if (sequence > LFPG_BTC_SEQUENCE_CAP)
        {
            WarnSequenceExhaustion(session);
            return LFPG_BTC_NONCE_INVALID;
        }

        LFPG_BTCNonceRecord terminal = FindTerminal(session, serverSessionLow, serverSessionHigh, sequence);
        if (terminal)
        {
            if (!terminal.MatchesFingerprint(subId, atmNetLow, atmNetHigh, amount, mode))
            {
                WarnFingerprint(session, sequence, subId);
                return LFPG_BTC_NONCE_CONFLICT;
            }
            cachedResponse = terminal.m_Response;
            return LFPG_BTC_NONCE_REPLAY;
        }

        LFPG_BTCNonceRecord inFlight = session.m_InFlight;
        if (inFlight && inFlight.m_ServerSessionLow == serverSessionLow && inFlight.m_ServerSessionHigh == serverSessionHigh && inFlight.m_Sequence == sequence)
        {
            if (!inFlight.MatchesFingerprint(subId, atmNetLow, atmNetHigh, amount, mode))
            {
                WarnFingerprint(session, sequence, subId);
                return LFPG_BTC_NONCE_CONFLICT;
            }
            return LFPG_BTC_NONCE_IN_FLIGHT;
        }

        // BTC handlers execute synchronously in one frame. A surviving
        // in-flight record when a later sequence arrives is therefore orphaned.
        // If any handler starts deferring work with CallLater, this rule is no
        // longer valid and must be replaced with an explicit lifecycle.
        if (inFlight && sequence > inFlight.m_Sequence)
            SealOrphanedInFlight(session);

        if (sequence <= session.m_HighestAcceptedSequence)
            return LFPG_BTC_NONCE_STALE;
        // Reaching this guard requires approximately two billion mutations for
        // one UID in one server lifetime, which the action rate limit prevents.
        if (session.m_HighestAcceptedSequence > LFPG_BTC_SEQUENCE_CAP - LFPG_BTC_SEQUENCE_EXHAUSTION_GUARD)
        {
            WarnSequenceExhaustion(session);
            return LFPG_BTC_NONCE_INVALID;
        }
        return LFPG_BTC_NONCE_NEW;
    }
    bool ReserveRequest(PlayerIdentity identity, int serverSessionLow, int serverSessionHigh, int sequence, int subId, int atmNetLow, int atmNetHigh, int amount, bool mode)
    {
        LFPG_BTCPlayerSession session = GetSession(identity);
        if (!session || !session.m_Active)
            return false;
        if (serverSessionLow != m_ServerSessionLow || serverSessionHigh != m_ServerSessionHigh)
            return false;
        if (sequence <= 0 || sequence > LFPG_BTC_SEQUENCE_CAP)
            return false;
        if (session.m_HighestAcceptedSequence > LFPG_BTC_SEQUENCE_CAP - LFPG_BTC_SEQUENCE_EXHAUSTION_GUARD)
        {
            WarnSequenceExhaustion(session);
            return false;
        }
        if (sequence <= session.m_HighestAcceptedSequence)
            return false;
        if (session.m_InFlight)
            return false;

        LFPG_BTCNonceRecord record = new LFPG_BTCNonceRecord();
        record.m_ServerSessionLow = serverSessionLow;
        record.m_ServerSessionHigh = serverSessionHigh;
        record.m_Sequence = sequence;
        record.m_SubId = subId;
        record.m_AtmNetLow = atmNetLow;
        record.m_AtmNetHigh = atmNetHigh;
        record.m_Amount = amount;
        record.m_Mode = mode;

        session.m_InFlight = record;
        session.m_HighestAcceptedSequence = sequence;
        return true;
    }

    bool CompleteRequest(PlayerIdentity identity, int serverSessionLow, int serverSessionHigh, int sequence, int txType, int errCode, int newStock, int newBalance, int btcMoved, float eurAmount, int cashOnInv, int btcOnInv)
    {
        LFPG_BTCPlayerSession session = GetSession(identity);
        if (!session)
            return false;

        LFPG_BTCNonceRecord record = session.m_InFlight;
        if (!record)
            return false;
        if (record.m_ServerSessionLow != serverSessionLow || record.m_ServerSessionHigh != serverSessionHigh || record.m_Sequence != sequence)
            return false;

        LFPG_BTCSessionResponse response = new LFPG_BTCSessionResponse();
        response.m_TxType = txType;
        response.m_ErrCode = errCode;
        response.m_NewStock = newStock;
        response.m_NewBalance = newBalance;
        response.m_BtcMoved = btcMoved;
        response.m_EurAmount = eurAmount;
        response.m_CashOnInventory = cashOnInv;
        response.m_BtcOnInventory = btcOnInv;
        response.m_ServerSessionLow = serverSessionLow;
        response.m_ServerSessionHigh = serverSessionHigh;
        response.m_Sequence = sequence;

        record.m_Response = response;
        session.m_InFlight = null;
        session.m_Terminals.Insert(record);
        while (session.m_Terminals.Count() > LFPG_BTC_TERMINAL_CACHE_SIZE)
        {
            session.m_Terminals.Remove(0);
        }
        return true;
    }

    bool AllowReplayResponse(PlayerIdentity identity)
    {
        LFPG_BTCPlayerSession session = GetSession(identity);
        if (!session || !g_Game)
            return false;

        int nowMs = g_Game.GetTime();
        if (!session.m_ReplayWindowActive || (nowMs - session.m_ReplayWindowStartMs) >= LFPG_BTC_REPLAY_WINDOW_MS)
        {
            session.m_ReplayWindowActive = true;
            session.m_ReplayWindowStartMs = nowMs;
            session.m_ReplayResponsesInWindow = 0;
        }
        if (session.m_ReplayResponsesInWindow >= LFPG_BTC_REPLAY_MAX_PER_WINDOW)
            return false;

        session.m_ReplayResponsesInWindow = session.m_ReplayResponsesInWindow + 1;
        return true;
    }
};
