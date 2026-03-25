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
    protected TextWidget m_StockText;
    protected TextWidget m_BalanceText;
    protected TextWidget m_CashText;
    protected EditBoxWidget m_EditBtc;
    protected EditBoxWidget m_EditEur;
    protected CheckBoxWidget m_ChkToAccount;
    protected TextWidget m_ChkToAccountLabel;
    protected ImageWidget m_StatusBg;
    protected TextWidget m_StatusText;

    // ── Conversion re-entry guard ──
    protected bool m_UpdatingBtc;
    protected bool m_UpdatingEur;

    // ── Status feedback timer ──
    protected float m_StatusTimer;

    // ── Palette refs (for status) ──
    static const int COL_GREEN      = 0xFF34D399;
    static const int COL_RED        = 0xFFF87171;
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
        m_StockText = view.StockText;
        m_BalanceText = view.BalanceText;
        m_CashText = view.CashText;
        m_EditBtc = view.EditBtcAmount;
        m_EditEur = view.EditEurAmount;
        m_ChkToAccount = view.ChkToAccount;
        m_ChkToAccountLabel = view.ChkToAccountLabel;
        m_StatusBg = view.StatusBg;
        m_StatusText = view.StatusText;
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

        // Cash card
        if (m_CashText)
        {
            string cashStr = FormatEurInt(cash);
            m_CashText.SetText(cashStr);
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
                string woLabel = "Solo retiro (admin)";
                m_ChkToAccountLabel.SetText(woLabel);
                m_ChkToAccountLabel.SetColor(COL_RED);
            }
        }
        else
        {
            if (m_ChkToAccountLabel)
            {
                string normalLabel = "Ingresar a cuenta (sin efectivo)";
                m_ChkToAccountLabel.SetText(normalLabel);
                m_ChkToAccountLabel.SetColor(COL_TEXT_MID);
            }
        }

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
    // Button handlers: send RPCs
    // =========================================================
    void OnBuyClick()
    {
        // F5: client-side price guard
        bool priceNA = LFPG_BTCAtmClientData.s_PriceUnavailable;
        if (priceNA)
        {
            string errNA = "Precio no disponible";
            ShowStatus(errNA, true);
            return;
        }

        int btcAmount = GetBtcInput();
        if (btcAmount <= 0)
        {
            // F1: string literal to local
            string errEmpty = "Introduce cantidad de BTC";
            ShowStatus(errEmpty, true);
            return;
        }

        int subId = LFPG_RPC_SubId.BTC_BUY;
        int netLow = LFPG_BTCAtmClientData.s_NetLow;
        int netHigh = LFPG_BTCAtmClientData.s_NetHigh;

        SendBTCRpc(subId, netLow, netHigh, btcAmount);
    }

    void OnSellClick()
    {
        // F5: client-side price guard
        bool priceNA = LFPG_BTCAtmClientData.s_PriceUnavailable;
        if (priceNA)
        {
            string errNA = "Precio no disponible";
            ShowStatus(errNA, true);
            return;
        }

        int btcAmount = GetBtcInput();
        if (btcAmount <= 0)
        {
            string errEmpty = "Introduce cantidad de BTC";
            ShowStatus(errEmpty, true);
            return;
        }

        bool toAccount = false;
        if (m_ChkToAccount)
        {
            toAccount = m_ChkToAccount.IsChecked();
        }

        // WithdrawOnly forces cash (toAccount = false)
        bool wo = LFPG_BTCAtmClientData.s_WithdrawOnly;
        if (wo)
        {
            toAccount = false;
        }

        int subId = LFPG_RPC_SubId.BTC_SELL;
        int netLow = LFPG_BTCAtmClientData.s_NetLow;
        int netHigh = LFPG_BTCAtmClientData.s_NetHigh;

        SendBTCSellRpc(subId, netLow, netHigh, btcAmount, toAccount);
    }

    void OnWithdrawClick()
    {
        int btcAmount = GetBtcInput();
        if (btcAmount <= 0)
        {
            string errEmpty = "Introduce cantidad de BTC";
            ShowStatus(errEmpty, true);
            return;
        }

        int subId = LFPG_RPC_SubId.BTC_WITHDRAW;
        int netLow = LFPG_BTCAtmClientData.s_NetLow;
        int netHigh = LFPG_BTCAtmClientData.s_NetHigh;

        SendBTCRpc(subId, netLow, netHigh, btcAmount);
    }

    void OnDepositClick()
    {
        int btcAmount = GetBtcInput();
        if (btcAmount <= 0)
        {
            string errEmpty = "Introduce cantidad de BTC";
            ShowStatus(errEmpty, true);
            return;
        }

        int subId = LFPG_RPC_SubId.BTC_DEPOSIT;
        int netLow = LFPG_BTCAtmClientData.s_NetLow;
        int netHigh = LFPG_BTCAtmClientData.s_NetHigh;

        SendBTCRpc(subId, netLow, netHigh, btcAmount);
    }

    // =========================================================
    // RPC senders (client → server)
    // F6: removed unused param
    // =========================================================
    protected void SendBTCRpc(int subId, int netLow, int netHigh, int btcAmount)
    {
        #ifndef SERVER
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

    // Sell has extra bool (toAccount)
    protected void SendBTCSellRpc(int subId, int netLow, int netHigh, int btcAmount, bool toAccount)
    {
        #ifndef SERVER
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player)
            return;

        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(subId);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Write(btcAmount);
        rpc.Write(toAccount);
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
                string buyOk = "Compra OK: ";
                buyOk = buyOk + moved.ToString();
                buyOk = buyOk + " BTC";
                return buyOk;
            }
            if (txType == LFPG_BTC_TX_SELL)
            {
                string sellOk = "Venta OK: ";
                sellOk = sellOk + moved.ToString();
                sellOk = sellOk + " BTC";
                return sellOk;
            }
            if (txType == LFPG_BTC_TX_WITHDRAW)
            {
                string wdOk = "Retirado: ";
                wdOk = wdOk + moved.ToString();
                wdOk = wdOk + " BTC";
                return wdOk;
            }
            if (txType == LFPG_BTC_TX_DEPOSIT)
            {
                string depOk = "Ingresado: ";
                depOk = depOk + moved.ToString();
                depOk = depOk + " BTC";
                return depOk;
            }
            string genericOk = "Operacion completada";
            return genericOk;
        }

        // Errors
        if (errCode == LFPG_BTC_ERR_NO_PRICE)
        {
            string e1 = "Precio no disponible";
            return e1;
        }
        if (errCode == LFPG_BTC_ERR_NO_FUNDS)
        {
            string e2 = "Saldo insuficiente";
            return e2;
        }
        if (errCode == LFPG_BTC_ERR_NO_STOCK)
        {
            string e3 = "Sin stock en ATM";
            return e3;
        }
        if (errCode == LFPG_BTC_ERR_STOCK_FULL)
        {
            string e4 = "ATM lleno";
            return e4;
        }
        if (errCode == LFPG_BTC_ERR_NO_ITEMS)
        {
            string e5 = "No tienes BTC encima";
            return e5;
        }
        if (errCode == LFPG_BTC_ERR_INVENTORY_FULL)
        {
            string e6 = "Inventario lleno (items en suelo)";
            return e6;
        }
        if (errCode == LFPG_BTC_ERR_NOT_POWERED)
        {
            string e7 = "ATM sin electricidad";
            return e7;
        }
        if (errCode == LFPG_BTC_ERR_TOO_FAR)
        {
            string e8 = "Demasiado lejos del ATM";
            return e8;
        }
        string e9 = "Error desconocido";
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
        string prefix = "E ";
        result = prefix + result;
        return result;
    }

    protected string FormatEurInt(int val)
    {
        string result = val.ToString();
        string prefix = "E ";
        result = prefix + result;
        return result;
    }
};
