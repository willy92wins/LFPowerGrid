// =========================================================
// LF_PowerGrid — BTC ATM Controller (Sprint BTC-4, Dabs MVC)
//
// Handles: info card display, dual EditBox auto-conversion,
// 4 button RPCs, status feedback timer.
// No Relay_Command — View dispatches OnClick by UserID.
//
// Audit fixes (v2):
//   F1: String literals assigned to local before ShowStatus
//   F2: BindButtonChildren removed → inline in View.EnsureViewBindings
//   F3: s_LastTxType reset on open → no stale feedback
//   F4: WithdrawOnly restores label when false
//   F5: Client-side price guard on Buy/Sell
//   F6: Removed unused 'unused' param from SendBTCRpc
//   F7: Explicit int cast on Math.Floor/Math.Round
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
    protected CheckBoxWidget m_ChkToAccount;
    protected TextWidget m_ChkToAccountLabel;
    protected ImageWidget m_StatusBg;
    protected TextWidget m_StatusText;
    // B3: Hint refs for dynamic update
    protected TextWidget m_BtnBuyHint;
    protected TextWidget m_BtnSellHint;
    protected TextWidget m_BtnWithdrawHint;
    protected TextWidget m_BtnDepositHint;

    // ── Conversion re-entry guard ──
    protected bool m_UpdatingBtc;
    protected bool m_UpdatingEur;

    // ── Status feedback timer ──
    protected float m_StatusTimer;

    // ── Palette refs (for status) ──
    static const int COL_GREEN      = 0xFF34D399;
    static const int COL_RED        = 0xFFF87171;
    static const int COL_AMBER      = 0xFFFBBF24;
    static const int COL_TEXT_MID   = 0xFFB0BEC5;
    static const int COL_STATUS_OK  = 0x1734D399;
    static const int COL_STATUS_ERR = 0x17F87171;

    void LFPG_BTCAtmController()
    {
        m_UpdatingBtc = false;
        m_UpdatingEur = false;
        m_StatusTimer = 0.0;
    }

    // Called from View.DoOpen after EnsureViewBindings
    void InitWidgetRefs(LFPG_BTCAtmView view)
    {
        if (!view)
            return;

        m_PriceText = view.PriceText;
        m_PriceChangeText = view.PriceChangeText;
        m_StockText = view.StockText;
        m_BalanceText = view.BalanceText;
        m_CashEurText = view.CashEurText;
        m_CashBtcText = view.CashBtcText;
        m_EditBtc = view.EditBtcAmount;
        m_EditEur = view.EditEurAmount;
        m_ChkToAccount = view.ChkToAccount;
        m_ChkToAccountLabel = view.ChkToAccountLabel;
        m_StatusBg = view.StatusBg;
        m_StatusText = view.StatusText;
        // B3: Hint refs
        m_BtnBuyHint = view.BtnBuyHint;
        m_BtnSellHint = view.BtnSellHint;
        m_BtnWithdrawHint = view.BtnWithdrawHint;
        m_BtnDepositHint = view.BtnDepositHint;
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
        bool wo = LFPG_BTCAtmClientData.s_WithdrawOnly;

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

                // Format: "(+2.34%▲)" or "(-1.50%▼)"
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
                changeStr = changeStr + "%";
                if (isNeg)
                {
                    changeStr = changeStr + " 24h)";
                }
                else
                {
                    changeStr = changeStr + " 24h)";
                }
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

        // Cash card — A3: split EUR/BTC
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

        // F4: WithdrawOnly — always set label + checkbox state (both branches)
        if (wo)
        {
            if (m_ChkToAccount)
            {
                m_ChkToAccount.SetChecked(false);
            }
            if (m_ChkToAccountLabel)
            {
                string woKey = "#STR_LFPG_BTC_WITHDRAW_ONLY";
                string woLabel = Widget.TranslateString(woKey);
                m_ChkToAccountLabel.SetText(woLabel);
                m_ChkToAccountLabel.SetColor(COL_RED);
            }
        }
        else
        {
            if (m_ChkToAccountLabel)
            {
                string normalKey = "#STR_LFPG_BTC_CHK_TO_ACCOUNT";
                string normalLabel = Widget.TranslateString(normalKey);
                m_ChkToAccountLabel.SetText(normalLabel);
                m_ChkToAccountLabel.SetColor(COL_TEXT_MID);
            }
        }

        // B3: Update button hints based on checkbox state
        bool accountMode = false;
        if (m_ChkToAccount)
        {
            accountMode = m_ChkToAccount.IsChecked();
        }
        UpdateButtonHints(accountMode);

        // Show tx result if we have one
        int errCode = LFPG_BTCAtmClientData.s_LastErrCode;
        int txType = LFPG_BTCAtmClientData.s_LastTxType;
        if (txType > 0)
        {
            ShowTxFeedback(txType, errCode);
        }
    }

    // F3: Reset stale tx data on open — prevents showing old result
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
        // F7: explicit float → int via intermediate
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
        // F7: explicit cast
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
    // Button handlers: send RPCs (B3: toggle Cuenta/Efectivo)
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

        bool useAccount = false;
        if (m_ChkToAccount)
        {
            useAccount = m_ChkToAccount.IsChecked();
        }

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

        bool useAccount = false;
        if (m_ChkToAccount)
        {
            useAccount = m_ChkToAccount.IsChecked();
        }

        // WithdrawOnly forces cash (useAccount = false)
        bool wo = LFPG_BTCAtmClientData.s_WithdrawOnly;
        if (wo)
        {
            useAccount = false;
        }

        int subId = LFPG_RPC_SubId.BTC_SELL;
        int netLow = LFPG_BTCAtmClientData.s_NetLow;
        int netHigh = LFPG_BTCAtmClientData.s_NetHigh;

        SendBTCSellRpc(subId, netLow, netHigh, btcAmount, useAccount);
    }

    void OnWithdrawClick()
    {
        bool useAccount = false;
        if (m_ChkToAccount)
        {
            useAccount = m_ChkToAccount.IsChecked();
        }

        int netLow = LFPG_BTCAtmClientData.s_NetLow;
        int netHigh = LFPG_BTCAtmClientData.s_NetHigh;

        if (useAccount)
        {
            // Account mode: withdraw BTC from ATM stock (original)
            int btcAmount = GetBtcInput();
            if (btcAmount <= 0)
            {
                string errEmptyKey = "#STR_LFPG_BTC_ERR_EMPTY";
                string errEmpty = Widget.TranslateString(errEmptyKey);
                ShowStatus(errEmpty, true);
                return;
            }
            int subId = LFPG_RPC_SubId.BTC_WITHDRAW;
            SendBTCRpc(subId, netLow, netHigh, btcAmount);
        }
        else
        {
            // Cash mode: withdraw EUR from LBMaster → bills
            int eurAmount = GetEurInput();
            if (eurAmount <= 0)
            {
                string errEmptyKey2 = "#STR_LFPG_BTC_ERR_EMPTY";
                string errEmpty2 = Widget.TranslateString(errEmptyKey2);
                ShowStatus(errEmpty2, true);
                return;
            }
            int subIdCash = LFPG_RPC_SubId.BTC_WITHDRAW_CASH;
            SendBTCCashRpc(subIdCash, netLow, netHigh, eurAmount);
        }
    }

    void OnDepositClick()
    {
        bool useAccount = false;
        if (m_ChkToAccount)
        {
            useAccount = m_ChkToAccount.IsChecked();
        }

        int netLow = LFPG_BTCAtmClientData.s_NetLow;
        int netHigh = LFPG_BTCAtmClientData.s_NetHigh;

        if (useAccount)
        {
            // Account mode: deposit BTC items into ATM stock (original)
            int btcAmount = GetBtcInput();
            if (btcAmount <= 0)
            {
                string errEmptyKey = "#STR_LFPG_BTC_ERR_EMPTY";
                string errEmpty = Widget.TranslateString(errEmptyKey);
                ShowStatus(errEmpty, true);
                return;
            }
            int subId = LFPG_RPC_SubId.BTC_DEPOSIT;
            SendBTCRpc(subId, netLow, netHigh, btcAmount);
        }
        else
        {
            // Cash mode: deposit EUR bills → LBMaster
            int eurAmount = GetEurInput();
            if (eurAmount <= 0)
            {
                string errEmptyKey2 = "#STR_LFPG_BTC_ERR_EMPTY";
                string errEmpty2 = Widget.TranslateString(errEmptyKey2);
                ShowStatus(errEmpty2, true);
                return;
            }
            int subIdCash = LFPG_RPC_SubId.BTC_DEPOSIT_CASH;
            SendBTCCashRpc(subIdCash, netLow, netHigh, eurAmount);
        }
    }

    // =========================================================
    // RPC senders (client → server)
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

    // Sell has extra bool (useAccount)
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

    // B3: Buy has extra bool (useAccount)
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

    // B3: Cash-mode RPCs (Withdraw/Deposit EUR)
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

    // B3: Read EUR amount from EditBox
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
    // B3: Toggle callback + dynamic button hints
    // =========================================================
    void OnAccountModeChanged(bool accountMode)
    {
        UpdateButtonHints(accountMode);
    }

    protected void UpdateButtonHints(bool accountMode)
    {
        if (accountMode)
        {
            if (m_BtnBuyHint)
            {
                string hBuyAcc = "#STR_LFPG_BTC_HINT_BUY";
                string hBuyAccT = Widget.TranslateString(hBuyAcc);
                m_BtnBuyHint.SetText(hBuyAccT);
            }
            if (m_BtnSellHint)
            {
                string hSellAcc = "#STR_LFPG_BTC_HINT_SELL";
                string hSellAccT = Widget.TranslateString(hSellAcc);
                m_BtnSellHint.SetText(hSellAccT);
            }
            if (m_BtnWithdrawHint)
            {
                string hWdAcc = "#STR_LFPG_BTC_HINT_WITHDRAW";
                string hWdAccT = Widget.TranslateString(hWdAcc);
                m_BtnWithdrawHint.SetText(hWdAccT);
            }
            if (m_BtnDepositHint)
            {
                string hDepAcc = "#STR_LFPG_BTC_HINT_DEPOSIT";
                string hDepAccT = Widget.TranslateString(hDepAcc);
                m_BtnDepositHint.SetText(hDepAccT);
            }
        }
        else
        {
            if (m_BtnBuyHint)
            {
                string hBuyCash = "#STR_LFPG_BTC_HINT_BUY_CASH";
                string hBuyCashT = Widget.TranslateString(hBuyCash);
                m_BtnBuyHint.SetText(hBuyCashT);
            }
            if (m_BtnSellHint)
            {
                string hSellCash = "#STR_LFPG_BTC_HINT_SELL_CASH";
                string hSellCashT = Widget.TranslateString(hSellCash);
                m_BtnSellHint.SetText(hSellCashT);
            }
            if (m_BtnWithdrawHint)
            {
                string hWdCash = "#STR_LFPG_BTC_HINT_WITHDRAW_CASH";
                string hWdCashT = Widget.TranslateString(hWdCash);
                m_BtnWithdrawHint.SetText(hWdCashT);
            }
            if (m_BtnDepositHint)
            {
                string hDepCash = "#STR_LFPG_BTC_HINT_DEPOSIT_CASH";
                string hDepCashT = Widget.TranslateString(hDepCash);
                m_BtnDepositHint.SetText(hDepCashT);
            }
        }
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
            // B3: Cash-mode TX types (EUR amounts)
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
        // B3: No cash on player
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
        // F7: explicit float → int via intermediate
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
