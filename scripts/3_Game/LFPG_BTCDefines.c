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

// ---- BTC ATM: Transaction types (for TX_RESULT RPC) ----
static const int LFPG_BTC_TX_BUY      = 1;
static const int LFPG_BTC_TX_SELL     = 2;
static const int LFPG_BTC_TX_WITHDRAW = 3;
static const int LFPG_BTC_TX_DEPOSIT  = 4;

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

// ---- BTC ATM: Safety cap for greedy change ----
static const int LFPG_BTC_MAX_CHANGE_ITEMS = 500;

// ---- BTC ATM: Client-side data holder (Sprint BTC-3) ----
// Populated by client RPC handlers, read by UI (Sprint BTC-4).
class LFPG_BTCAtmClientData
{
    // From BTC_OPEN_RESPONSE
    static float  s_Price          = -1.0;
    static int    s_Stock          = 0;
    static int    s_Balance        = 0;
    static int    s_CashOnInventory = 0;
    static bool   s_WithdrawOnly   = false;
    static bool   s_PriceUnavailable = false;

    // ATM network ID (set by action before RPC, used by controller)
    static int    s_NetLow         = 0;
    static int    s_NetHigh        = 0;

    // From BTC_TX_RESULT
    static int    s_LastTxType     = 0;
    static int    s_LastErrCode    = 0;
    static int    s_LastNewStock   = 0;
    static int    s_LastNewBalance = 0;
    static int    s_LastBtcMoved   = 0;
    static float  s_LastEurAmount  = 0.0;

    static void OnOpenResponse(float price, int stock, int balance, int cashOnInv, bool wo)
    {
        s_Stock = stock;
        s_Balance = balance;
        s_CashOnInventory = cashOnInv;
        s_WithdrawOnly = wo;

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
    }

    static void OnTxResult(int txType, int errCode, int newStock, int newBalance, int btcMoved, float eurAmount, int cashOnInv)
    {
        s_LastTxType = txType;
        s_LastErrCode = errCode;
        s_LastNewStock = newStock;
        s_LastNewBalance = newBalance;
        s_LastBtcMoved = btcMoved;
        s_LastEurAmount = eurAmount;
        // Also update live stock/balance/cash
        s_Stock = newStock;
        s_Balance = newBalance;
        s_CashOnInventory = cashOnInv;
    }

    static void OnPriceUnavailable()
    {
        s_PriceUnavailable = true;
        s_Price = -1.0;
    }
};
