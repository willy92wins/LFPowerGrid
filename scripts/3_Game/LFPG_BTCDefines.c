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
