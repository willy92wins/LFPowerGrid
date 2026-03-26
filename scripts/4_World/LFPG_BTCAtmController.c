// =========================================================
// LF_PowerGrid — BTC ATM Controller (Sprint BTC-5: 6 Buttons)
//
// Handles: info card display, dual EditBox auto-conversion,
// 6 button RPCs, tab toggle, WithdrawOnly dimming, status feedback.
// No Relay_Command — View dispatches OnClick by UserID.
//
// BTC-5 changes:
//   - Checkbox replaced by m_AccountMode bool + OnTabClick
//   - 4 click handlers → 6 (Buy, Sell, WithdrawEur, DepositEur, WithdrawBtc, DepositBtc)
//   - UpdateButtonHints → UpdateBuySellHints (only 2 hints dynamic)
//   - DimButton with normalBgColor restore parameter
//   - m_View back-ref for SetTabColors
// =========================================================

class LFPG_BTCAtmController extends ViewController
{
    // ── Widget refs (set from View) ──
    protected TextWidget m_PriceText;
    protected TextWidget m_PriceChangeText;
    protected TextWidget m_StockText;
    protected TextWidget m_BalanceText;
    protected TextWidget m_CashEurText;
    protected TextWidget m_CashBtcText;
    protected EditBoxWidget m_EditBtc;
    protected EditBoxWidget m_EditEur;
    protected ImageWidget m_StatusBg;
    protected TextWidget m_StatusText;

    // Buy/Sell hint refs (dynamic, changes with tab)
    protected TextWidget m_BtnBuyBtcHint;
    protected TextWidget m_BtnSellBtcHint;

    // WithdrawOnly dimming refs
    protected ImageWidget m_BtnSellBtcBg;
    protected TextWidget m_BtnSellBtcText;
    protected TextWidget m_BtnSellBtcHintDim;
    protected ImageWidget m_BtnDepositEurBg;
    protected TextWidget m_BtnDepositEurText;
    protected TextWidget m_BtnDepositEurHint;
    protected ImageWidget m_BtnDepositBtcBg;
    protected TextWidget m_BtnDepositBtcText;
    protected TextWidget m_BtnDepositBtcHint;

    // ── View back-ref (for SetTabColors) ──
    protected LFPG_BTCAtmView m_View;

    // ── Tab state ──
    protected bool m_AccountMode;

    // ── Conversion re-entry guard ──
    protected bool m_UpdatingBtc;
    protected bool m_UpdatingEur;

    // ── Status feedback timer ──
    protected float m_StatusTimer;

    // ── Palette refs (for status + dimming) ──
    static const int COL_GREEN      = 0xFF34D399;
    static const int COL_RED        = 0xFFF87171;
    static const int COL_AMBER      = 0xFFFBBF24;
    static const int COL_TEXT       = 0xFFF1F5F9;
    static const int COL_TEXT_MID   = 0xFFB0BEC5;
    static const int COL_TEXT_DIM   = 0xFF7A8A9B;
    static const int COL_STATUS_OK  = 0x1734D399;
    static const int COL_STATUS_ERR = 0x17F87171;
    static const int COL_BTN        = 0xFF374B6F;
    static const int COL_RED_BTN    = 0xFFC72323;
    static const int COL_DIM_BG     = 0x40374B6F;

    void LFPG_BTCAtmController()
    {
        m_AccountMode = false;
        m_UpdatingBtc = false;
        m_UpdatingEur = false;
        m_StatusTimer = 0.0;
        m_View = null;
    }

    // Called from View.DoOpen after EnsureViewBindings
    void InitWidgetRefs(LFPG_BTCAtmView view)
    {
        if (!view)
            return;

        m_View = view;

        m_PriceText = view.PriceText;
        m_PriceChangeText = view.PriceChangeText;
        m_StockText = view.StockText;
        m_BalanceText = view.BalanceText;
        m_CashEurText = view.CashEurText;
        m_CashBtcText = view.CashBtcText;
        m_EditBtc = view.EditBtcAmount;
        m_EditEur = view.EditEurAmount;
        m_StatusBg = view.StatusBg;
        m_StatusText = view.StatusText;

        // Buy/Sell hints (dynamic)
        m_BtnBuyBtcHint = view.BtnBuyBtcHint;
        m_BtnSellBtcHint = view.BtnSellBtcHint;

        // WithdrawOnly dimming refs
        m_BtnSellBtcBg = view.BtnSellBtcBg;
        m_BtnSellBtcText = view.BtnSellBtcText;
        m_BtnSellBtcHintDim = view.BtnSellBtcHint;
        m_BtnDepositEurBg = view.BtnDepositEurBg;
        m_BtnDepositEurText = view.BtnDepositEurText;
        m_BtnDepositEurHint = view.BtnDepositEurHint;
        m_BtnDepositBtcBg = view.BtnDepositBtcBg;
        m_BtnDepositBtcText = view.BtnDepositBtcText;
        m_BtnDepositBtcHint = view.BtnDepositBtcHint;

        // Reset tab to default
        m_AccountMode = false;
    }

    // =========================================================
    // Refresh all display from LFPG_BTCAtmClientData
    // =========================================================
    void RefreshFromClientData()
    {
        float price = LFPG_BTCAtmClientData.s_Price;
        int stock = LFPG_BTCAtmClientData.s_Stock;
        int balance = LFPG_BTCAtmClientData.s_Balance;
        int cash = LFPG_BTCAtmClientData.s_CashOnInventory;
        bool priceNA = LFPG_BTCAtmClientData.s_PriceUnavailable;

        // Price card
        if (m_PriceText)
        {
            if (priceNA)
            {
                string naStr = "N/A";
                m_PriceText.SetText(naStr);
            }
            else
            {
                string priceStr = FormatEur(price);
                m_PriceText.SetText(priceStr);
            }
        }

        // 24h price change indicator
        if (m_PriceChangeText)
        {
            float change24h = LFPG_BTCAtmClientData.s_PriceChange24h;
            if (priceNA)
            {
                string emptyChange = "";
                m_PriceChangeText.SetText(emptyChange);
            }
            else
            {
                bool isNeg = false;
                float absChange = change24h;
                if (change24h < 0.0)
                {
                    isNeg = true;
                    absChange = -change24h;
                }

                int intPart = (int)absChange;
                float decFloat = absChange - intPart;
                int decPart = (int)(decFloat * 100.0);
                if (decPart < 0)
                {
                    decPart = -decPart;
                }

                string changeStr = "(";
                if (isNeg)
                {
                    changeStr = changeStr + "-";
                }
                else
                {
                    changeStr = changeStr + "+";
                }
                changeStr = changeStr + intPart.ToString();
                changeStr = changeStr + ".";
                if (decPart < 10)
                {
                    changeStr = changeStr + "0";
                }
                changeStr = changeStr + decPart.ToString();
                changeStr = changeStr + "% 24h)";
                m_PriceChangeText.SetText(changeStr);

                if (isNeg)
                {
                    m_PriceChangeText.SetColor(COL_RED);
                }
                else
                {
                    m_PriceChangeText.SetColor(COL_GREEN);
                }
            }
        }

        // Stock card
        if (m_StockText)
        {
            string stockStr = stock.ToString();
            stockStr = stockStr + " B";
            m_StockText.SetText(stockStr);
        }

        // Balance card
        if (m_BalanceText)
        {
            string balStr = FormatEurInt(balance);
            m_BalanceText.SetText(balStr);
        }

        // Cash card — split EUR/BTC
        if (m_CashEurText)
        {
            string cashStr = FormatEurInt(cash);
            m_CashEurText.SetText(cashStr);
        }
        if (m_CashBtcText)
        {
            int btcInv = LFPG_BTCAtmClientData.s_BtcOnInventory;
            string btcStr = "/ ";
            btcStr = btcStr + btcInv.ToString();
            btcStr = btcStr + " B";
            m_CashBtcText.SetText(btcStr);
        }

        // Update Buy/Sell hints for current tab
        UpdateBuySellHints(m_AccountMode);

        // Update tab visuals
        if (m_View)
        {
            m_View.SetTabColors(m_AccountMode);
        }

        // WithdrawOnly dimming
        UpdateWithdrawOnly(m_AccountMode);

        // Show tx result if we have one
        int errCode = LFPG_BTCAtmClientData.s_LastErrCode;
        int txType = LFPG_BTCAtmClientData.s_LastTxType;
        if (txType > 0)
        {
            ShowTxFeedback(txType, errCode);
        }
    }

    // F3: Reset stale tx data on open
    void ResetTxState()
    {
        LFPG_BTCAtmClientData.s_LastTxType = 0;
        LFPG_BTCAtmClientData.s_LastErrCode = 0;
    }

    // =========================================================
    // EditBox auto-conversion (bidirectional)
    // =========================================================
    void OnBtcAmountChanged()
    {
        if (m_UpdatingBtc)
            return;

        float price = LFPG_BTCAtmClientData.s_Price;
        if (price <= 0.0)
            return;

        if (!m_EditBtc)
            return;

        string btcStr = m_EditBtc.GetText();
        if (btcStr == "")
        {
            m_UpdatingEur = true;
            if (m_EditEur)
            {
                string emptyStr = "";
                m_EditEur.SetText(emptyStr);
            }
            m_UpdatingEur = false;
            return;
        }

        int btcVal = btcStr.ToInt();
        if (btcVal < 0)
        {
            btcVal = 0;
        }

        float eurFloat = btcVal * price;
        float eurFloor = Math.Floor(eurFloat);
        int eurInt = eurFloor;
        string eurStr = eurInt.ToString();

        m_UpdatingEur = true;
        if (m_EditEur)
        {
            m_EditEur.SetText(eurStr);
        }
        m_UpdatingEur = false;
    }

    void OnEurAmountChanged()
    {
        if (m_UpdatingEur)
            return;

        float price = LFPG_BTCAtmClientData.s_Price;
        if (price <= 0.0)
            return;

        if (!m_EditEur)
            return;

        string eurStr = m_EditEur.GetText();
        if (eurStr == "")
        {
            m_UpdatingBtc = true;
            if (m_EditBtc)
            {
                string emptyStr = "";
                m_EditBtc.SetText(emptyStr);
            }
            m_UpdatingBtc = false;
            return;
        }

        int eurVal = eurStr.ToInt();
        if (eurVal < 0)
        {
            eurVal = 0;
        }

        float btcFloat = eurVal / price;
        float btcFloor = Math.Floor(btcFloat);
        int btcInt = btcFloor;
        string btcStr = btcInt.ToString();

        m_UpdatingBtc = true;
        if (m_EditBtc)
        {
            m_EditBtc.SetText(btcStr);
        }
        m_UpdatingBtc = false;
    }

    // =========================================================
    // Tab toggle
    // =========================================================
    void OnTabClick(bool accountMode)
    {
        m_AccountMode = accountMode;

        UpdateBuySellHints(accountMode);

        if (m_View)
        {
            m_View.SetTabColors(accountMode);
        }

        UpdateWithdrawOnly(accountMode);
    }

    // =========================================================
    // Buy/Sell hint update (only these 2 are dynamic)
    // =========================================================
    protected void UpdateBuySellHints(bool accountMode)
    {
        if (accountMode)
        {
            if (m_BtnBuyBtcHint)
            {
                string hBuy = "#STR_LFPG_BTC_HINT_BUY";
                string hBuyT = Widget.TranslateString(hBuy);
                m_BtnBuyBtcHint.SetText(hBuyT);
            }
            if (m_BtnSellBtcHint)
            {
                string hSell = "#STR_LFPG_BTC_HINT_SELL";
                string hSellT = Widget.TranslateString(hSell);
                m_BtnSellBtcHint.SetText(hSellT);
            }
        }
        else
        {
            if (m_BtnBuyBtcHint)
            {
                string hBuyC = "#STR_LFPG_BTC_HINT_BUY_CASH";
                string hBuyCT = Widget.TranslateString(hBuyC);
                m_BtnBuyBtcHint.SetText(hBuyCT);
            }
            if (m_BtnSellBtcHint)
            {
                string hSellC = "#STR_LFPG_BTC_HINT_SELL_CASH";
                string hSellCT = Widget.TranslateString(hSellC);
                m_BtnSellBtcHint.SetText(hSellCT);
            }
        }
    }

    // =========================================================
    // WithdrawOnly dimming
    // =========================================================
    protected void UpdateWithdrawOnly(bool accountMode)
    {
        bool wo = LFPG_BTCAtmClientData.s_WithdrawOnly;

        // Sell BTC: dimmed only if wo AND accountMode
        bool sellDimmed = false;
        if (wo && accountMode)
        {
            sellDimmed = true;
        }
        DimButton(m_BtnSellBtcBg, m_BtnSellBtcText, m_BtnSellBtcHintDim, sellDimmed, COL_RED_BTN);

        // Deposit EUR: dimmed if wo (always, regardless of tab)
        DimButton(m_BtnDepositEurBg, m_BtnDepositEurText, m_BtnDepositEurHint, wo, COL_BTN);

        // Deposit BTC: dimmed if wo (always, regardless of tab)
        DimButton(m_BtnDepositBtcBg, m_BtnDepositBtcText, m_BtnDepositBtcHint, wo, COL_BTN);
    }

    protected void DimButton(ImageWidget bg, TextWidget txt, TextWidget hint, bool dimmed, int normalBgColor)
    {
        if (dimmed)
        {
            if (bg) { bg.SetColor(COL_DIM_BG); }
            if (txt) { txt.SetColor(COL_TEXT_DIM); }
            if (hint) { hint.SetColor(COL_TEXT_DIM); }
        }
        else
        {
            if (bg) { bg.SetColor(normalBgColor); }
            if (txt) { txt.SetColor(COL_TEXT); }
            if (hint) { hint.SetColor(COL_TEXT_MID); }
        }
    }

    // =========================================================
    // Button handlers: 6 explicit operations
    // =========================================================
    void OnBuyClick()
    {
        // F5: client-side price guard
        bool priceNA = LFPG_BTCAtmClientData.s_PriceUnavailable;
        if (priceNA)
        {
            string errNAKey = "#STR_LFPG_BTC_ERR_PRICE_NA";
            string errNA = Widget.TranslateString(errNAKey);
            ShowStatus(errNA, true);
            return;
        }

        int btcAmount = GetBtcInput();
        if (btcAmount <= 0)
        {
            string errEmptyKey = "#STR_LFPG_BTC_ERR_EMPTY";
            string errEmpty = Widget.TranslateString(errEmptyKey);
            ShowStatus(errEmpty, true);
            return;
        }

        bool useAccount = m_AccountMode;

        int netLow = LFPG_BTCAtmClientData.s_NetLow;
        int netHigh = LFPG_BTCAtmClientData.s_NetHigh;
        int subId = LFPG_RPC_SubId.BTC_BUY;

        SendBTCBuyRpc(subId, netLow, netHigh, btcAmount, useAccount);
    }

    void OnSellClick()
    {
        bool priceNA = LFPG_BTCAtmClientData.s_PriceUnavailable;
        if (priceNA)
        {
            string errNAKey = "#STR_LFPG_BTC_ERR_PRICE_NA";
            string errNA = Widget.TranslateString(errNAKey);
            ShowStatus(errNA, true);
            return;
        }

        int btcAmount = GetBtcInput();
        if (btcAmount <= 0)
        {
            string errEmptyKey = "#STR_LFPG_BTC_ERR_EMPTY";
            string errEmpty = Widget.TranslateString(errEmptyKey);
            ShowStatus(errEmpty, true);
            return;
        }

        bool useAccount = m_AccountMode;

        // WithdrawOnly: block account sell
        bool wo = LFPG_BTCAtmClientData.s_WithdrawOnly;
        if (wo && useAccount)
        {
            string errWoKey = "#STR_LFPG_BTC_WITHDRAW_ONLY";
            string errWo = Widget.TranslateString(errWoKey);
            ShowStatus(errWo, true);
            return;
        }

        int subId = LFPG_RPC_SubId.BTC_SELL;
        int netLow = LFPG_BTCAtmClientData.s_NetLow;
        int netHigh = LFPG_BTCAtmClientData.s_NetHigh;

        SendBTCSellRpc(subId, netLow, netHigh, btcAmount, useAccount);
    }

    void OnWithdrawEurClick()
    {
        int eurAmount = GetEurInput();
        if (eurAmount <= 0)
        {
            string errEmptyKey = "#STR_LFPG_BTC_ERR_EMPTY";
            string errEmpty = Widget.TranslateString(errEmptyKey);
            ShowStatus(errEmpty, true);
            return;
        }
        int subId = LFPG_RPC_SubId.BTC_WITHDRAW_CASH;
        int netLow = LFPG_BTCAtmClientData.s_NetLow;
        int netHigh = LFPG_BTCAtmClientData.s_NetHigh;
        SendBTCCashRpc(subId, netLow, netHigh, eurAmount);
    }

    void OnDepositEurClick()
    {
        // WithdrawOnly guard
        bool wo = LFPG_BTCAtmClientData.s_WithdrawOnly;
        if (wo)
        {
            string errWoKey = "#STR_LFPG_BTC_WITHDRAW_ONLY";
            string errWo = Widget.TranslateString(errWoKey);
            ShowStatus(errWo, true);
            return;
        }
        int eurAmount = GetEurInput();
        if (eurAmount <= 0)
        {
            string errEmptyKey = "#STR_LFPG_BTC_ERR_EMPTY";
            string errEmpty = Widget.TranslateString(errEmptyKey);
            ShowStatus(errEmpty, true);
            return;
        }
        int subId = LFPG_RPC_SubId.BTC_DEPOSIT_CASH;
        int netLow = LFPG_BTCAtmClientData.s_NetLow;
        int netHigh = LFPG_BTCAtmClientData.s_NetHigh;
        SendBTCCashRpc(subId, netLow, netHigh, eurAmount);
    }

    void OnWithdrawBtcClick()
    {
        int btcAmount = GetBtcInput();
        if (btcAmount <= 0)
        {
            string errEmptyKey = "#STR_LFPG_BTC_ERR_EMPTY";
            string errEmpty = Widget.TranslateString(errEmptyKey);
            ShowStatus(errEmpty, true);
            return;
        }
        int subId = LFPG_RPC_SubId.BTC_WITHDRAW;
        int netLow = LFPG_BTCAtmClientData.s_NetLow;
        int netHigh = LFPG_BTCAtmClientData.s_NetHigh;
        SendBTCRpc(subId, netLow, netHigh, btcAmount);
    }

    void OnDepositBtcClick()
    {
        // WithdrawOnly guard
        bool wo = LFPG_BTCAtmClientData.s_WithdrawOnly;
        if (wo)
        {
            string errWoKey = "#STR_LFPG_BTC_WITHDRAW_ONLY";
            string errWo = Widget.TranslateString(errWoKey);
            ShowStatus(errWo, true);
            return;
        }
        int btcAmount = GetBtcInput();
        if (btcAmount <= 0)
        {
            string errEmptyKey = "#STR_LFPG_BTC_ERR_EMPTY";
            string errEmpty = Widget.TranslateString(errEmptyKey);
            ShowStatus(errEmpty, true);
            return;
        }
        int subId = LFPG_RPC_SubId.BTC_DEPOSIT;
        int netLow = LFPG_BTCAtmClientData.s_NetLow;
        int netHigh = LFPG_BTCAtmClientData.s_NetHigh;
        SendBTCRpc(subId, netLow, netHigh, btcAmount);
    }

    // =========================================================
    // RPC senders (client → server) — UNCHANGED
    // =========================================================
    protected void SendBTCRpc(int subId, int netLow, int netHigh, int btcAmount)
    {
        #ifndef SERVER
        if (!GetGame())
            return;

        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player)
            return;

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(subId);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Write(btcAmount);
        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);
        #endif
    }

    protected void SendBTCSellRpc(int subId, int netLow, int netHigh, int btcAmount, bool useAccount)
    {
        #ifndef SERVER
        if (!GetGame())
            return;

        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player)
            return;

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(subId);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Write(btcAmount);
        rpc.Write(useAccount);
        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);
        #endif
    }

    protected void SendBTCBuyRpc(int subId, int netLow, int netHigh, int btcAmount, bool useAccount)
    {
        #ifndef SERVER
        if (!GetGame())
            return;

        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player)
            return;

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(subId);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Write(btcAmount);
        rpc.Write(useAccount);
        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);
        #endif
    }

    protected void SendBTCCashRpc(int subId, int netLow, int netHigh, int eurAmount)
    {
        #ifndef SERVER
        if (!GetGame())
            return;

        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player)
            return;

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(subId);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Write(eurAmount);
        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);
        #endif
    }

    // =========================================================
    // Input reading
    // =========================================================
    protected int GetBtcInput()
    {
        if (!m_EditBtc)
            return 0;

        string text = m_EditBtc.GetText();
        if (text == "")
            return 0;

        int val = text.ToInt();
        if (val < 0)
        {
            val = 0;
        }
        return val;
    }

    protected int GetEurInput()
    {
        if (!m_EditEur)
            return 0;

        string text = m_EditEur.GetText();
        if (text == "")
            return 0;

        int val = text.ToInt();
        if (val < 0)
        {
            val = 0;
        }
        return val;
    }

    // =========================================================
    // Status feedback
    // =========================================================
    protected void ShowTxFeedback(int txType, int errCode)
    {
        bool isErr = (errCode != LFPG_BTC_OK);
        string msg = BuildTxMessage(txType, errCode);
        ShowStatus(msg, isErr);
    }

    void ShowStatus(string msg, bool isError)
    {
        if (m_StatusText)
        {
            m_StatusText.SetText(msg);
            if (isError)
            {
                m_StatusText.SetColor(COL_RED);
            }
            else
            {
                m_StatusText.SetColor(COL_GREEN);
            }
        }

        if (m_StatusBg)
        {
            if (isError)
            {
                m_StatusBg.SetColor(COL_STATUS_ERR);
            }
            else
            {
                m_StatusBg.SetColor(COL_STATUS_OK);
            }
            m_StatusBg.Show(true);
        }

        m_StatusTimer = 5.0;
    }

    void TickTimers(float dt)
    {
        if (m_StatusTimer > 0.0)
        {
            m_StatusTimer = m_StatusTimer - dt;
            if (m_StatusTimer <= 0.0)
            {
                if (m_StatusBg)
                {
                    m_StatusBg.Show(false);
                }
                if (m_StatusText)
                {
                    string emptyStatus = "";
                    m_StatusText.SetText(emptyStatus);
                }
            }
        }
    }

    // =========================================================
    // Message builder
    // =========================================================
    protected string BuildTxMessage(int txType, int errCode)
    {
        if (errCode == LFPG_BTC_OK)
        {
            int moved = LFPG_BTCAtmClientData.s_LastBtcMoved;
            float eur = LFPG_BTCAtmClientData.s_LastEurAmount;

            if (txType == LFPG_BTC_TX_BUY)
            {
                string buyOkKey = "#STR_LFPG_BTC_TX_BUY_OK";
                string buyOk = Widget.TranslateString(buyOkKey);
                buyOk = buyOk + moved.ToString();
                buyOk = buyOk + " BTC";
                return buyOk;
            }
            if (txType == LFPG_BTC_TX_SELL)
            {
                string sellOkKey = "#STR_LFPG_BTC_TX_SELL_OK";
                string sellOk = Widget.TranslateString(sellOkKey);
                sellOk = sellOk + moved.ToString();
                sellOk = sellOk + " BTC";
                return sellOk;
            }
            if (txType == LFPG_BTC_TX_WITHDRAW)
            {
                string wdOkKey = "#STR_LFPG_BTC_TX_WITHDRAW_OK";
                string wdOk = Widget.TranslateString(wdOkKey);
                wdOk = wdOk + moved.ToString();
                wdOk = wdOk + " BTC";
                return wdOk;
            }
            if (txType == LFPG_BTC_TX_DEPOSIT)
            {
                string depOkKey = "#STR_LFPG_BTC_TX_DEPOSIT_OK";
                string depOk = Widget.TranslateString(depOkKey);
                depOk = depOk + moved.ToString();
                depOk = depOk + " BTC";
                return depOk;
            }
            if (txType == LFPG_BTC_TX_WITHDRAW_CASH)
            {
                string wdcOkKey = "#STR_LFPG_BTC_TX_WITHDRAW_CASH_OK";
                string wdcOk = Widget.TranslateString(wdcOkKey);
                float wdcEurRound = Math.Round(eur);
                int wdcEurInt = wdcEurRound;
                wdcOk = wdcOk + wdcEurInt.ToString();
                wdcOk = wdcOk + " E";
                return wdcOk;
            }
            if (txType == LFPG_BTC_TX_DEPOSIT_CASH)
            {
                string dpcOkKey = "#STR_LFPG_BTC_TX_DEPOSIT_CASH_OK";
                string dpcOk = Widget.TranslateString(dpcOkKey);
                float dpcEurRound = Math.Round(eur);
                int dpcEurInt = dpcEurRound;
                dpcOk = dpcOk + dpcEurInt.ToString();
                dpcOk = dpcOk + " E";
                return dpcOk;
            }
            string genericOkKey = "#STR_LFPG_BTC_TX_OK_GENERIC";
            string genericOk = Widget.TranslateString(genericOkKey);
            return genericOk;
        }

        // Errors
        if (errCode == LFPG_BTC_ERR_NO_PRICE)
        {
            string e1Key = "#STR_LFPG_BTC_ERR_PRICE_NA";
            string e1 = Widget.TranslateString(e1Key);
            return e1;
        }
        if (errCode == LFPG_BTC_ERR_NO_FUNDS)
        {
            string e2Key = "#STR_LFPG_BTC_ERR_NO_FUNDS";
            string e2 = Widget.TranslateString(e2Key);
            return e2;
        }
        if (errCode == LFPG_BTC_ERR_NO_STOCK)
        {
            string e3Key = "#STR_LFPG_BTC_ERR_NO_STOCK";
            string e3 = Widget.TranslateString(e3Key);
            return e3;
        }
        if (errCode == LFPG_BTC_ERR_STOCK_FULL)
        {
            string e4Key = "#STR_LFPG_BTC_ERR_STOCK_FULL";
            string e4 = Widget.TranslateString(e4Key);
            return e4;
        }
        if (errCode == LFPG_BTC_ERR_NO_ITEMS)
        {
            string e5Key = "#STR_LFPG_BTC_ERR_NO_ITEMS";
            string e5 = Widget.TranslateString(e5Key);
            return e5;
        }
        if (errCode == LFPG_BTC_ERR_INVENTORY_FULL)
        {
            string e6Key = "#STR_LFPG_BTC_ERR_INV_FULL";
            string e6 = Widget.TranslateString(e6Key);
            return e6;
        }
        if (errCode == LFPG_BTC_ERR_NOT_POWERED)
        {
            string e7Key = "#STR_LFPG_BTC_ERR_NO_POWER";
            string e7 = Widget.TranslateString(e7Key);
            return e7;
        }
        if (errCode == LFPG_BTC_ERR_TOO_FAR)
        {
            string e8Key = "#STR_LFPG_BTC_ERR_TOO_FAR";
            string e8 = Widget.TranslateString(e8Key);
            return e8;
        }
        if (errCode == LFPG_BTC_ERR_NO_CASH)
        {
            string e10Key = "#STR_LFPG_BTC_ERR_NO_CASH";
            string e10 = Widget.TranslateString(e10Key);
            return e10;
        }
        string e9Key = "#STR_LFPG_BTC_ERR_UNKNOWN";
        string e9 = Widget.TranslateString(e9Key);
        return e9;
    }

    // =========================================================
    // Format helpers
    // =========================================================
    protected string FormatEur(float val)
    {
        float rounded = Math.Round(val);
        int roundedInt = rounded;
        string result = roundedInt.ToString();
        string suffix = " E";
        result = result + suffix;
        return result;
    }

    protected string FormatEurInt(int val)
    {
        string result = val.ToString();
        string suffix = " E";
        result = result + suffix;
        return result;
    }
};
