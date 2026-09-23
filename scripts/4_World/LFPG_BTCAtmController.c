#ifndef SERVER
// Client-only compilation boundary
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

    // ── Palette (dedup F2-B) ──
    // Shared colors: use LFPG_UIPalette.COL_*
    // ATM-specific only:
    static const int COL_STATUS_OK  = 0x1734D399;
    static const int COL_STATUS_ERR = 0x17F87171;
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
                    m_PriceChangeText.SetColor(LFPG_UIPalette.COL_RED);
                }
                else
                {
                    m_PriceChangeText.SetColor(LFPG_UIPalette.COL_GREEN);
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

        // Mirror server cost idiom (LFPG_BTCHelper.c HandleBTCBuy): truncate +
        // bump by 1 when fractional, so the displayed EUR matches exactly what
        // the server will charge. Math.Floor here would short the user by 1.
        float eurFloat = btcVal * price;
        int eurInt = (int)eurFloat;
        float eurDiff = eurFloat - eurInt;
        if (eurDiff > 0.001)
        {
            eurInt = eurInt + 1;
        }
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
        DimButton(m_BtnSellBtcBg, m_BtnSellBtcText, m_BtnSellBtcHint, sellDimmed, LFPG_UIPalette.COL_RED_BTN);

        // Deposit EUR: dimmed if wo (always, regardless of tab)
        DimButton(m_BtnDepositEurBg, m_BtnDepositEurText, m_BtnDepositEurHint, wo, LFPG_UIPalette.COL_BTN);

        // Deposit BTC: dimmed if wo (always, regardless of tab)
        DimButton(m_BtnDepositBtcBg, m_BtnDepositBtcText, m_BtnDepositBtcHint, wo, LFPG_UIPalette.COL_BTN);
    }

    protected void DimButton(ImageWidget bg, TextWidget txt, TextWidget hint, bool dimmed, int normalBgColor)
    {
        if (dimmed)
        {
            // Use m_View.Tint to update hover cache alongside color
            if (m_View && bg)
            {
                m_View.Tint(bg, COL_DIM_BG);
            }
            if (txt) { txt.SetColor(LFPG_UIPalette.COL_TEXT_DIM); }
            if (hint) { hint.SetColor(LFPG_UIPalette.COL_TEXT_DIM); }
        }
        else
        {
            if (m_View && bg)
            {
                m_View.Tint(bg, normalBgColor);
            }
            if (txt) { txt.SetColor(LFPG_UIPalette.COL_TEXT); }
            if (hint) { hint.SetColor(LFPG_UIPalette.COL_TEXT_MID); }
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
            ShowStatus(Widget.TranslateString("#STR_LFPG_BTC_ERR_PRICE_NA"), true);
            return;
        }

        int btcAmount = GetBtcInput();
        if (btcAmount <= 0)
        {
            ShowStatus(Widget.TranslateString("#STR_LFPG_BTC_ERR_EMPTY"), true);
            return;
        }

        bool useAccount = m_AccountMode;

        int netLow = LFPG_BTCAtmClientData.s_NetLow;
        int netHigh = LFPG_BTCAtmClientData.s_NetHigh;

        SendBTCBuyRpc(LFPG_RPC_SubId.BTC_BUY, netLow, netHigh, btcAmount, useAccount);
    }

    void OnSellClick()
    {
        bool priceNA = LFPG_BTCAtmClientData.s_PriceUnavailable;
        if (priceNA)
        {
            ShowStatus(Widget.TranslateString("#STR_LFPG_BTC_ERR_PRICE_NA"), true);
            return;
        }

        int btcAmount = GetBtcInput();
        if (btcAmount <= 0)
        {
            ShowStatus(Widget.TranslateString("#STR_LFPG_BTC_ERR_EMPTY"), true);
            return;
        }

        bool useAccount = m_AccountMode;

        // WithdrawOnly: block account sell
        bool wo = LFPG_BTCAtmClientData.s_WithdrawOnly;
        if (wo && useAccount)
        {
            ShowStatus(Widget.TranslateString("#STR_LFPG_BTC_WITHDRAW_ONLY"), true);
            return;
        }

        int netLow = LFPG_BTCAtmClientData.s_NetLow;
        int netHigh = LFPG_BTCAtmClientData.s_NetHigh;

        SendBTCSellRpc(LFPG_RPC_SubId.BTC_SELL, netLow, netHigh, btcAmount, useAccount);
    }

    void OnWithdrawEurClick()
    {
        int eurAmount = GetEurInput();
        if (eurAmount <= 0)
        {
            ShowStatus(Widget.TranslateString("#STR_LFPG_BTC_ERR_EMPTY_EUR"), true);
            return;
        }
        int netLow = LFPG_BTCAtmClientData.s_NetLow;
        int netHigh = LFPG_BTCAtmClientData.s_NetHigh;
        SendBTCCashRpc(LFPG_RPC_SubId.BTC_WITHDRAW_CASH, netLow, netHigh, eurAmount);
    }

    void OnDepositEurClick()
    {
        // WithdrawOnly guard
        bool wo = LFPG_BTCAtmClientData.s_WithdrawOnly;
        if (wo)
        {
            ShowStatus(Widget.TranslateString("#STR_LFPG_BTC_WITHDRAW_ONLY"), true);
            return;
        }
        int eurAmount = GetEurInput();
        if (eurAmount <= 0)
        {
            ShowStatus(Widget.TranslateString("#STR_LFPG_BTC_ERR_EMPTY_EUR"), true);
            return;
        }
        int netLow = LFPG_BTCAtmClientData.s_NetLow;
        int netHigh = LFPG_BTCAtmClientData.s_NetHigh;
        SendBTCCashRpc(LFPG_RPC_SubId.BTC_DEPOSIT_CASH, netLow, netHigh, eurAmount);
    }

    void OnWithdrawBtcClick()
    {
        int btcAmount = GetBtcInput();
        if (btcAmount <= 0)
        {
            ShowStatus(Widget.TranslateString("#STR_LFPG_BTC_ERR_EMPTY"), true);
            return;
        }
        int netLow = LFPG_BTCAtmClientData.s_NetLow;
        int netHigh = LFPG_BTCAtmClientData.s_NetHigh;
        SendBTCRpc(LFPG_RPC_SubId.BTC_WITHDRAW, netLow, netHigh, btcAmount);
    }

    void OnDepositBtcClick()
    {
        // WithdrawOnly guard
        bool wo = LFPG_BTCAtmClientData.s_WithdrawOnly;
        if (wo)
        {
            ShowStatus(Widget.TranslateString("#STR_LFPG_BTC_WITHDRAW_ONLY"), true);
            return;
        }
        int btcAmount = GetBtcInput();
        if (btcAmount <= 0)
        {
            ShowStatus(Widget.TranslateString("#STR_LFPG_BTC_ERR_EMPTY"), true);
            return;
        }
        int netLow = LFPG_BTCAtmClientData.s_NetLow;
        int netHigh = LFPG_BTCAtmClientData.s_NetHigh;
        SendBTCRpc(LFPG_RPC_SubId.BTC_DEPOSIT, netLow, netHigh, btcAmount);
    }

    // =========================================================
    // RPC senders (client to server) - session-bound mutations
    // =========================================================
    protected void SendBTCRpc(int subId, int netLow, int netHigh, int btcAmount)
    {
        if (!g_Game)
            return;

        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player)
            return;

        int serverSessionLow = 0;
        int serverSessionHigh = 0;
        int sequence = 0;
        if (!LFPG_BTCAtmClientData.BeginMutation(subId, serverSessionLow, serverSessionHigh, sequence))
        {
            if (LFPG_BTCAtmClientData.TakeSessionRefreshRequest())
                RequestBTCSession(player, netLow, netHigh);
            return;
        }

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(subId);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Write(btcAmount);
        rpc.Write(serverSessionLow);
        rpc.Write(serverSessionHigh);
        rpc.Write(sequence);
        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);
    }

    protected void SendBTCSellRpc(int subId, int netLow, int netHigh, int btcAmount, bool useAccount)
    {
        if (!g_Game)
            return;

        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player)
            return;

        int serverSessionLow = 0;
        int serverSessionHigh = 0;
        int sequence = 0;
        if (!LFPG_BTCAtmClientData.BeginMutation(subId, serverSessionLow, serverSessionHigh, sequence))
        {
            if (LFPG_BTCAtmClientData.TakeSessionRefreshRequest())
                RequestBTCSession(player, netLow, netHigh);
            return;
        }

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(subId);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Write(btcAmount);
        rpc.Write(useAccount);
        rpc.Write(serverSessionLow);
        rpc.Write(serverSessionHigh);
        rpc.Write(sequence);
        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);
    }

    protected void SendBTCBuyRpc(int subId, int netLow, int netHigh, int btcAmount, bool useAccount)
    {
        if (!g_Game)
            return;

        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player)
            return;

        int serverSessionLow = 0;
        int serverSessionHigh = 0;
        int sequence = 0;
        if (!LFPG_BTCAtmClientData.BeginMutation(subId, serverSessionLow, serverSessionHigh, sequence))
        {
            if (LFPG_BTCAtmClientData.TakeSessionRefreshRequest())
                RequestBTCSession(player, netLow, netHigh);
            return;
        }

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(subId);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Write(btcAmount);
        rpc.Write(useAccount);
        rpc.Write(serverSessionLow);
        rpc.Write(serverSessionHigh);
        rpc.Write(sequence);
        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);
    }

    protected void SendBTCCashRpc(int subId, int netLow, int netHigh, int eurAmount)
    {
        if (!g_Game)
            return;

        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player)
            return;

        int serverSessionLow = 0;
        int serverSessionHigh = 0;
        int sequence = 0;
        if (!LFPG_BTCAtmClientData.BeginMutation(subId, serverSessionLow, serverSessionHigh, sequence))
        {
            if (LFPG_BTCAtmClientData.TakeSessionRefreshRequest())
                RequestBTCSession(player, netLow, netHigh);
            return;
        }

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(subId);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Write(eurAmount);
        rpc.Write(serverSessionLow);
        rpc.Write(serverSessionHigh);
        rpc.Write(sequence);
        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);
    }

    protected void RequestBTCSession(PlayerBase player, int netLow, int netHigh)
    {
        if (!player)
            return;

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write((int)LFPG_RPC_SubId.BTC_OPEN_REQUEST);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);
    }

    // =========================================================
    // Input reading
    // =========================================================
    protected int ReadEditBoxInput(EditBoxWidget w)
    {
        if (!w)
            return 0;

        string text = w.GetText();
        if (text == "")
            return 0;

        int val = text.ToInt();
        if (val < 0)
        {
            val = 0;
        }
        return val;
    }

    protected int GetBtcInput()
    {
        return ReadEditBoxInput(m_EditBtc);
    }

    protected int GetEurInput()
    {
        return ReadEditBoxInput(m_EditEur);
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
                m_StatusText.SetColor(LFPG_UIPalette.COL_RED);
            }
            else
            {
                m_StatusText.SetColor(LFPG_UIPalette.COL_GREEN);
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
                return Widget.TranslateString("#STR_LFPG_BTC_TX_BUY_OK") + moved.ToString() + " BTC";
            }
            if (txType == LFPG_BTC_TX_SELL)
            {
                return Widget.TranslateString("#STR_LFPG_BTC_TX_SELL_OK") + moved.ToString() + " BTC";
            }
            if (txType == LFPG_BTC_TX_WITHDRAW)
            {
                return Widget.TranslateString("#STR_LFPG_BTC_TX_WITHDRAW_OK") + moved.ToString() + " BTC";
            }
            if (txType == LFPG_BTC_TX_DEPOSIT)
            {
                return Widget.TranslateString("#STR_LFPG_BTC_TX_DEPOSIT_OK") + moved.ToString() + " BTC";
            }
            if (txType == LFPG_BTC_TX_WITHDRAW_CASH)
            {
                float wdcEurRound = Math.Round(eur);
                int wdcEurInt = wdcEurRound;
                return Widget.TranslateString("#STR_LFPG_BTC_TX_WITHDRAW_CASH_OK") + wdcEurInt.ToString() + " E";
            }
            if (txType == LFPG_BTC_TX_DEPOSIT_CASH)
            {
                float dpcEurRound = Math.Round(eur);
                int dpcEurInt = dpcEurRound;
                return Widget.TranslateString("#STR_LFPG_BTC_TX_DEPOSIT_CASH_OK") + dpcEurInt.ToString() + " E";
            }
            return Widget.TranslateString("#STR_LFPG_BTC_TX_OK_GENERIC");
        }

        // Errors
        if (errCode == LFPG_BTC_ERR_NO_PRICE)
        {
            return Widget.TranslateString("#STR_LFPG_BTC_ERR_PRICE_NA");
        }
        if (errCode == LFPG_BTC_ERR_NO_FUNDS)
        {
            return Widget.TranslateString("#STR_LFPG_BTC_ERR_NO_FUNDS");
        }
        if (errCode == LFPG_BTC_ERR_NO_STOCK)
        {
            return Widget.TranslateString("#STR_LFPG_BTC_ERR_NO_STOCK");
        }
        if (errCode == LFPG_BTC_ERR_STOCK_FULL)
        {
            return Widget.TranslateString("#STR_LFPG_BTC_ERR_STOCK_FULL");
        }
        if (errCode == LFPG_BTC_ERR_NO_ITEMS)
        {
            return Widget.TranslateString("#STR_LFPG_BTC_ERR_NO_ITEMS");
        }
        if (errCode == LFPG_BTC_ERR_INVENTORY_FULL)
        {
            return Widget.TranslateString("#STR_LFPG_BTC_ERR_INV_FULL");
        }
        if (errCode == LFPG_BTC_ERR_NOT_POWERED)
        {
            return Widget.TranslateString("#STR_LFPG_BTC_ERR_NO_POWER");
        }
        if (errCode == LFPG_BTC_ERR_TOO_FAR)
        {
            return Widget.TranslateString("#STR_LFPG_BTC_ERR_TOO_FAR");
        }
        if (errCode == LFPG_BTC_ERR_NO_CASH)
        {
            return Widget.TranslateString("#STR_LFPG_BTC_ERR_NO_CASH");
        }
        if (errCode == LFPG_BTC_ERR_NO_BALANCE_PROVIDER)
        {
            return Widget.TranslateString("#STR_LFPG_BTC_ERR_NO_BALANCE");
        }
        if (errCode == LFPG_BTC_ERR_REFUNDED)
        {
            return Widget.TranslateString("#STR_LFPG_BTC_ERR_REFUNDED");
        }
        if (errCode == LFPG_BTC_ERR_REFUND_PARTIAL)
        {
            return Widget.TranslateString("#STR_LFPG_BTC_ERR_REFUND_PARTIAL");
        }
        if (errCode == LFPG_BTC_ERR_AMOUNT_TOO_LARGE)
        {
            return Widget.TranslateString("#STR_LFPG_BTC_ERR_AMOUNT_TOO_LARGE");
        }
        return Widget.TranslateString("#STR_LFPG_BTC_ERR_UNKNOWN");
    }

    // =========================================================
    // Format helpers
    // =========================================================
    protected string FormatEur(float val)
    {
        // Ceil-idiom matches server cost rounding (HandleBTCBuy). Using
        // Math.Round here would print a price 1 EUR below what 1 BTC costs
        // when the upstream price has decimals (e.g. 67931.50 → "67931 E"
        // while server charges 67932), confusing players who match amounts.
        int roundedInt = (int)val;
        float roundedDiff = val - roundedInt;
        if (roundedDiff > 0.001)
        {
            roundedInt = roundedInt + 1;
        }
        string result = roundedInt.ToString();
        result = result + " E";
        return result;
    }

    protected string FormatEurInt(int val)
    {
        string result = val.ToString();
        result = result + " E";
        return result;
    }
};
#endif
