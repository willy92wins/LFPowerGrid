// =========================================================
// LF_PowerGrid - BTC ATM Defines (Sprint BTC-1)
//
// Constants, RPC SubIds, and timer intervals for the
// Bitcoin ATM system. Follows LFPG_Defines.c conventions.
//
// RPC SubIds: 40-49 reserved for BTC ATM.
// =========================================================

// ---- BTC ATM: RPC SubIds (40-49 range) ----
// Added to LFPG_RPC_SubId enum — listed here as reference.
// INTEGRATION: Add these to the existing LFPG_RPC_SubId enum
// in LFPG_Defines.c (after SORTER_SORT_ACK = 33).
//
//   BTC_OPEN_REQUEST       = 40,  // Client→Server: player opened ATM UI
//   BTC_OPEN_RESPONSE      = 41,  // Server→Client: price + stock + balance
//   BTC_BUY                = 42,  // Client→Server: buy BTC with ATM money
//   BTC_SELL               = 43,  // Client→Server: sell BTC for money
//   BTC_WITHDRAW           = 44,  // Client→Server: withdraw BTC items from machine
//   BTC_DEPOSIT            = 45,  // Client→Server: deposit BTC items into machine
//   BTC_TX_RESULT          = 46,  // Server→Client: transaction result (success/fail + updated state)
//   BTC_PRICE_UNAVAILABLE  = 47,  // Server→Client: price not available (API error)

// ---- BTC ATM: Timer ----
static const int LFPG_BTC_PRICE_CHECK_MS = 60000;  // 60s between price fetches

// ---- BTC ATM: Price sentinel ----
// When m_CachedPrice is this value, no price has been fetched yet.
static const float LFPG_BTC_PRICE_UNAVAILABLE = -1.0;

// ---- BTC ATM: Device consumption ----
static const float LFPG_BTC_ATM_CONSUMPTION = 30.0;  // u/s for player-deployed ATM

// ---- BTC ATM: Config paths ----
static const string LFPG_BTC_SETTINGS_DIR  = "$profile:LF_PowerGrid";
static const string LFPG_BTC_SETTINGS_FILE = "$profile:LF_PowerGrid\\LF_BTCAtm.json";
static const string LFPG_BALANCE_NATIVE_FILE = "$profile:LF_PowerGrid\\LF_Balances.json";

// ---- BTC ATM: Transaction types (for TX_RESULT RPC) ----
static const int LFPG_BTC_TX_BUY      = 1;
static const int LFPG_BTC_TX_SELL     = 2;
static const int LFPG_BTC_TX_WITHDRAW      = 3;
static const int LFPG_BTC_TX_DEPOSIT       = 4;
static const int LFPG_BTC_TX_WITHDRAW_CASH = 5;
static const int LFPG_BTC_TX_DEPOSIT_CASH  = 6;

// ---- BTC ATM: Error codes (for TX_RESULT RPC) ----
static const int LFPG_BTC_OK                  = 0;
static const int LFPG_BTC_ERR_NO_PRICE        = 1;   // price not available
static const int LFPG_BTC_ERR_NO_FUNDS        = 2;   // not enough ATM money
static const int LFPG_BTC_ERR_NO_STOCK        = 3;   // machine has no BTC
static const int LFPG_BTC_ERR_STOCK_FULL      = 4;   // machine BTC at max
static const int LFPG_BTC_ERR_NO_ITEMS        = 5;   // player has no BTC items
static const int LFPG_BTC_ERR_INVENTORY_FULL  = 6;   // player inventory full
static const int LFPG_BTC_ERR_NOT_POWERED     = 7;   // device not powered (consumer variant)
static const int LFPG_BTC_ERR_TOO_FAR         = 8;   // player too far from ATM
static const int LFPG_BTC_ERR_INVALID         = 9;   // generic validation failure
static const int LFPG_BTC_ERR_NO_CASH         = 10;  // player has no EUR bills
static const int LFPG_BTC_ERR_NO_BALANCE_PROVIDER = 11;  // no balance provider active
// A3 patch 2026-05-17: race / refund visibility codes
static const int LFPG_BTC_ERR_REFUNDED        = 12;  // race after charge, full refund applied
static const int LFPG_BTC_ERR_REFUND_PARTIAL  = 13;  // race after charge, refund INCOMPLETE - admin required
// BTC large-amount cap 2026-05-19: input rejection by server cap
static const int LFPG_BTC_ERR_AMOUNT_TOO_LARGE = 14;  // amount exceeds per-tx server cap

// ---- BTC ATM: Protocol ----
static const int LFPG_BTC_PROTOCOL_VERSION = 2;

class LFPG_BTCTxTypeMapper
{
    static int GetTxTypeForSubId(int subId)
    {
        if (subId == LFPG_RPC_SubId.BTC_BUY)
            return LFPG_BTC_TX_BUY;
        if (subId == LFPG_RPC_SubId.BTC_SELL)
            return LFPG_BTC_TX_SELL;
        if (subId == LFPG_RPC_SubId.BTC_WITHDRAW)
            return LFPG_BTC_TX_WITHDRAW;
        if (subId == LFPG_RPC_SubId.BTC_DEPOSIT)
            return LFPG_BTC_TX_DEPOSIT;
        if (subId == LFPG_RPC_SubId.BTC_WITHDRAW_CASH)
            return LFPG_BTC_TX_WITHDRAW_CASH;
        if (subId == LFPG_RPC_SubId.BTC_DEPOSIT_CASH)
            return LFPG_BTC_TX_DEPOSIT_CASH;
        return 0;
    }
};

// ---- BTC ATM: Client-side data holder (Sprint BTC-3) ----
// Populated by client RPC handlers, read by UI (Sprint BTC-4).
#ifndef SERVER
class LFPG_BTCClientPending
{
    int m_Sequence;
    int m_TxType;
};

class LFPG_BTCAtmClientData
{
    protected static const int LFPG_BTC_CLIENT_MAX_SEQUENCE = 2000000000;
    protected static const int LFPG_BTC_CLIENT_PENDING_LIMIT = 64;

    // From BTC_OPEN_RESPONSE
    static float  s_Price          = -1.0;
    static int    s_Stock          = 0;
    static int    s_Balance        = 0;
    static int    s_CashOnInventory = 0;
    static int    s_BtcOnInventory  = 0;
    static bool   s_WithdrawOnly   = false;
    static bool   s_PriceUnavailable = false;
    static float  s_PriceChange24h   = 0.0;

    // ATM network ID (set by action before RPC, used by controller)
    static int    s_NetLow         = 0;
    static int    s_NetHigh        = 0;

    // Server-session nonce state. Pending entries are runtime-only.
    static bool   s_MutationsEnabled = false;
    static int    s_ServerSessionLow = 0;
    static int    s_ServerSessionHigh = 0;
    static int    s_NextSequence = 1;
    protected static bool s_SequenceRefreshNeeded = false;
    protected static ref array<ref LFPG_BTCClientPending> s_Pending;

    // From BTC_TX_RESULT
    static int    s_LastTxType     = 0;
    static int    s_LastErrCode    = 0;
    static int    s_LastNewStock   = 0;
    static int    s_LastNewBalance = 0;
    static int    s_LastBtcMoved   = 0;
    static float  s_LastEurAmount  = 0.0;

    protected static void EnsurePending()
    {
        if (!s_Pending)
            s_Pending = new array<ref LFPG_BTCClientPending>;
    }

    static bool BeginMutation(int subId, out int serverSessionLow, out int serverSessionHigh, out int sequence)
    {
        serverSessionLow = 0;
        serverSessionHigh = 0;
        sequence = 0;

        if (!s_MutationsEnabled)
            return false;
        if (s_NextSequence <= 0 || s_NextSequence > LFPG_BTC_CLIENT_MAX_SEQUENCE)
        {
            s_MutationsEnabled = false;
            s_SequenceRefreshNeeded = true;
            return false;
        }

        int txType = LFPG_BTCTxTypeMapper.GetTxTypeForSubId(subId);
        if (txType <= 0)
            return false;

        EnsurePending();
        while (s_Pending.Count() >= LFPG_BTC_CLIENT_PENDING_LIMIT)
        {
            s_Pending.Remove(0);
        }

        LFPG_BTCClientPending pending = new LFPG_BTCClientPending();
        pending.m_Sequence = s_NextSequence;
        pending.m_TxType = txType;
        s_Pending.Insert(pending);

        serverSessionLow = s_ServerSessionLow;
        serverSessionHigh = s_ServerSessionHigh;
        sequence = s_NextSequence;
        s_NextSequence = s_NextSequence + 1;
        return true;
    }

    static bool TakeSessionRefreshRequest()
    {
        if (!s_SequenceRefreshNeeded)
            return false;
        s_SequenceRefreshNeeded = false;
        return true;
    }

    static void OnOpenResponse(float price, int stock, int balance, int cashOnInv, bool wo, int btcOnInv, float priceChange24h, int protocolVersion, int serverSessionLow, int serverSessionHigh, int highWatermark)
    {
        s_Stock = stock;
        s_Balance = balance;
        s_CashOnInventory = cashOnInv;
        s_BtcOnInventory = btcOnInv;
        s_WithdrawOnly = wo;
        s_PriceChange24h = priceChange24h;

        // Sentinel: price <= 0 means server has no price
        if (price <= 0.0)
        {
            s_Price = -1.0;
            s_PriceUnavailable = true;
        }
        else
        {
            s_Price = price;
            s_PriceUnavailable = false;
        }

        EnsurePending();
        bool sessionPairChanged = s_ServerSessionLow != serverSessionLow || s_ServerSessionHigh != serverSessionHigh;
        if (sessionPairChanged)
            s_Pending.Clear();

        bool protocolReady = (protocolVersion == LFPG_BTC_PROTOCOL_VERSION);
        if (serverSessionLow == 0 && serverSessionHigh == 0)
            protocolReady = false;
        if (highWatermark < 0 || highWatermark > LFPG_BTC_CLIENT_MAX_SEQUENCE)
            protocolReady = false;

        if (protocolReady)
        {
            s_ServerSessionLow = serverSessionLow;
            s_ServerSessionHigh = serverSessionHigh;
            int nextFromWatermark = highWatermark + 1;
            if (nextFromWatermark > s_NextSequence)
                s_NextSequence = nextFromWatermark;
            s_MutationsEnabled = s_NextSequence > 0 && s_NextSequence <= LFPG_BTC_CLIENT_MAX_SEQUENCE;
        }
        else
        {
            s_ServerSessionLow = 0;
            s_ServerSessionHigh = 0;
            s_NextSequence = 1;
            s_MutationsEnabled = false;
        }
        s_SequenceRefreshNeeded = false;
    }

    static bool OnTxResult(int txType, int errCode, int newStock, int newBalance, int btcMoved, float eurAmount, int cashOnInv, int btcOnInv, int serverSessionLow, int serverSessionHigh, int sequence)
    {
        if (!s_MutationsEnabled)
            return false;
        if (serverSessionLow != s_ServerSessionLow || serverSessionHigh != s_ServerSessionHigh)
            return false;
        if (sequence <= 0)
            return false;

        EnsurePending();
        int pendingIndex = 0;
        while (pendingIndex < s_Pending.Count())
        {
            LFPG_BTCClientPending pending = s_Pending[pendingIndex];
            if (pending && pending.m_Sequence == sequence)
            {
                if (pending.m_TxType != txType)
                    return false;

                s_Pending.Remove(pendingIndex);
                s_LastTxType = txType;
                s_LastErrCode = errCode;
                bool preserveDisplay = errCode == LFPG_BTC_ERR_INVALID && newStock == 0 && newBalance == 0;
                // err=11 before ATM/balance resolve: 0/0 is unavailable, not a real zero.
                bool noProviderZeros = errCode == LFPG_BTC_ERR_NO_BALANCE_PROVIDER && newStock == 0 && newBalance == 0;
                if (noProviderZeros)
                {
                    preserveDisplay = true;
                }
                if (!preserveDisplay)
                {
                    s_LastNewStock = newStock;
                    s_LastNewBalance = newBalance;
                    s_Stock = newStock;
                    s_Balance = newBalance;
                    s_CashOnInventory = cashOnInv;
                    s_BtcOnInventory = btcOnInv;
                }
                s_LastBtcMoved = btcMoved;
                s_LastEurAmount = eurAmount;
                return true;
            }
            pendingIndex = pendingIndex + 1;
        }
        return false;
    }

    static void OnPriceUnavailable()
    {
        s_PriceUnavailable = true;
        s_Price = -1.0;
    }

    static void Reset()
    {
        s_Price = -1.0;
        s_Stock = 0;
        s_Balance = 0;
        s_CashOnInventory = 0;
        s_BtcOnInventory = 0;
        s_WithdrawOnly = false;
        s_PriceUnavailable = false;
        s_PriceChange24h = 0.0;
        s_NetLow = 0;
        s_NetHigh = 0;
        s_MutationsEnabled = false;
        s_ServerSessionLow = 0;
        s_ServerSessionHigh = 0;
        s_NextSequence = 1;
        s_SequenceRefreshNeeded = false;
        EnsurePending();
        s_Pending.Clear();
        s_LastTxType = 0;
        s_LastErrCode = 0;
        s_LastNewStock = 0;
        s_LastNewBalance = 0;
        s_LastBtcMoved = 0;
        s_LastEurAmount = 0.0;
    }
};
#endif
