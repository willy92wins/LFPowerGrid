#ifndef SERVER
// Client-only compilation boundary
// =========================================================
// LF_PowerGrid — BTC ATM View (Sprint BTC-5: 6 Buttons)
//
// ScriptView floating window. Sorter pattern simplified.
// Singleton: Init() pre-creates hidden, Open()/Close() toggle.
// Drag header, fade-in, hover feedback, cursor lock.
// All button dispatch via OnClick → Controller methods.
//
// BTC-5 changes:
//   - Checkbox replaced by 2 tabs (Cash/Account)
//   - 4 buttons → 6 (BuyBtc, SellBtc, WithdrawEur, DepositEur, WithdrawBtc, DepositBtc)
//   - Separator between Buy/Sell and EUR/BTC rows
//   - Tab hover feedback (same pattern as buttons)
//   - Consistent naming: layout names = view field names
// =========================================================

class LFPG_BTCAtmView extends LFPG_FloatingViewBase
{
    protected static ref LFPG_BTCAtmView s_Instance;
    protected bool m_FocusLocked;
    protected bool m_ControlsLocked;

    // ── Drag state ──

    // ── Fade-in ──
    protected float m_FadeAlpha;
    protected bool m_FadingIn;

    // ── Hover ──
    protected ref array<ref LFPG_ColorData> m_ColorDataRefs;

    // ── Widget refs (manual binding) ──
    Widget BTCAtmPanel;
    Widget HeaderFrame;
    ImageWidget PanelBg;
    ImageWidget HeaderBg;
    ImageWidget AccentLine;
    TextWidget HeaderIcon;
    TextWidget HeaderTitle;
    TextWidget DragHandle;
    ImageWidget BtnCloseXBg;
    TextWidget BtnCloseXText;

    // Info cards
    ImageWidget CardPriceBg;
    TextWidget CardPriceLabel;
    TextWidget PriceText;
    TextWidget PriceChangeText;
    ImageWidget CardStockBg;
    TextWidget CardStockLabel;
    TextWidget StockText;
    ImageWidget CardBalanceBg;
    TextWidget CardBalanceLabel;
    TextWidget BalanceText;
    ImageWidget CardCashBg;
    TextWidget CardCashLabel;
    TextWidget CashEurText;
    TextWidget CashBtcText;

    // Amount section
    ImageWidget AmountBg;
    TextWidget AmountLabel;
    TextWidget BtcLabel;
    ImageWidget EditBtcBorder;
    ImageWidget EditBtcBg;
    EditBoxWidget EditBtcAmount;
    TextWidget ArrowLabel;
    TextWidget EurLabel;
    ImageWidget EditEurBorder;
    ImageWidget EditEurBg;
    EditBoxWidget EditEurAmount;

    // Mode tabs (BTC-5: replaces checkbox)
    ImageWidget TabCashBg;
    TextWidget TabCashText;
    ImageWidget TabAccountBg;
    TextWidget TabAccountText;

    // Separator
    ImageWidget SepBuySell;

    // Row 1: Buy/Sell BTC (bg refs for coloring + hover)
    ImageWidget BtnBuyBtcBg;
    TextWidget BtnBuyBtcText;
    TextWidget BtnBuyBtcHint;
    ImageWidget BtnSellBtcBg;
    TextWidget BtnSellBtcText;
    TextWidget BtnSellBtcHint;

    // Row 2: Withdraw/Deposit EUR
    ImageWidget BtnWithdrawEurBg;
    TextWidget BtnWithdrawEurText;
    TextWidget BtnWithdrawEurHint;
    ImageWidget BtnDepositEurBg;
    TextWidget BtnDepositEurText;
    TextWidget BtnDepositEurHint;

    // Row 3: Withdraw/Deposit BTC
    ImageWidget BtnWithdrawBtcBg;
    TextWidget BtnWithdrawBtcText;
    TextWidget BtnWithdrawBtcHint;
    ImageWidget BtnDepositBtcBg;
    TextWidget BtnDepositBtcText;
    TextWidget BtnDepositBtcHint;

    // Status
    ImageWidget StatusBg;
    TextWidget StatusText;

    // Footer
    ImageWidget FooterSep;
    ImageWidget FooterBg;
    TextWidget FooterEscHint;
    TextWidget FooterBrand;

    // ── Palette ──
    // Shared colors: use LFPG_UIPalette.COL_* (dedup F2-B)
    // ATM-specific only:
    static const string PROC_WHITE = "#(argb,8,8,3)color(1,1,1,1,CO)";
    static const int COL_AMBER_BTN    = 0xFFB8880F;
    static const int COL_STATUS_OK_BG = 0x1734D399;

    // ── Button UserID constants (BTC-5: consistent naming) ──
    static const int UID_TAB_CASH      = 120;
    static const int UID_TAB_ACCOUNT   = 121;
    static const int UID_BUY_BTC       = 100;
    static const int UID_SELL_BTC      = 101;
    static const int UID_WITHDRAW_EUR  = 102;
    static const int UID_DEPOSIT_EUR   = 103;
    static const int UID_WITHDRAW_BTC  = 104;
    static const int UID_DEPOSIT_BTC   = 105;
    static const int UID_CLOSE_X       = 110;

    protected bool m_ColorsInitialized;
    protected bool m_ButtonIDsAssigned;

    // A7: ESC timestamp guard
    protected static float s_EscCloseTime = 0.0;

    override string GetLayoutFile()
    {
        return "LFPowerGrid/gui/layouts/LFPG_BTCAtm.layout";
    }

    override typename GetControllerType()
    {
        return LFPG_BTCAtmController;
    }

    void LFPG_BTCAtmView()
    {
        m_IsOpen = false;
        m_FocusLocked = false;
        m_ControlsLocked = false;
        m_Dragging = false;
        m_DragOffX = 0.0;
        m_DragOffY = 0.0;
        m_FadeAlpha = 1.0;
        m_FadingIn = false;
        m_HoveredBg = null;
        m_ColorDataRefs = new array<ref LFPG_ColorData>();
    }

    void ~LFPG_BTCAtmView()
    {
        #ifndef SERVER
        if (g_Game)
        {
            if (m_ControlsLocked && g_Game.GetMission())
            {
                g_Game.GetMission().PlayerControlEnable(false);
                m_ControlsLocked = false;
            }
            if (m_FocusLocked)
            {
                Input inp = g_Game.GetInput();
                if (inp)
                    inp.ChangeGameFocus(-1);
                UIManager uiMgr = g_Game.GetUIManager();
                if (uiMgr)
                    uiMgr.ShowUICursor(false);
                m_FocusLocked = false;
            }
        }
        #endif
    }

    // =========================================================
    // Update: fade + drag
    // =========================================================
    override void Update(float dt)
    {
        if (!m_IsOpen)
            return;

        if (m_FadingIn)
        {
            m_FadeAlpha = m_FadeAlpha + dt * 5.0;
            if (m_FadeAlpha >= 1.0)
            {
                m_FadeAlpha = 1.0;
                m_FadingIn = false;
            }
            if (BTCAtmPanel)
                BTCAtmPanel.SetAlpha(m_FadeAlpha);
        }

        if (m_Dragging)
        {
            int mx = 0;
            int my = 0;
            GetMousePos(mx, my);
            float newX = mx - m_DragOffX;
            float newY = my - m_DragOffY;
            float clampedX = 0.0;
            float clampedY = 0.0;
            float dragMinY = 5.0;
            ClampPanelPos(newX, newY, dragMinY, clampedX, clampedY);
            if (BTCAtmPanel)
                BTCAtmPanel.SetPos(clampedX, clampedY);
        }

        // Controller timers (status feedback)
        LFPG_BTCAtmController ctrl = LFPG_BTCAtmController.Cast(GetController());
        if (ctrl)
            ctrl.TickTimers(dt);
    }

    // =========================================================
    // Manual widget binding (Dabs auto-bind misses ButtonWidget children)
    // =========================================================
    protected void EnsureViewBindings()
    {
        Widget root = GetLayoutRoot();
        if (!root)
            return;

        if (!BTCAtmPanel) { BTCAtmPanel = root.FindAnyWidget("BTCAtmPanel"); }
        if (!HeaderFrame) { HeaderFrame = root.FindAnyWidget("HeaderFrame"); }
        if (!PanelBg) { PanelBg = ImageWidget.Cast(root.FindAnyWidget("PanelBg")); }
        if (!HeaderBg) { HeaderBg = ImageWidget.Cast(root.FindAnyWidget("HeaderBg")); }
        if (!AccentLine) { AccentLine = ImageWidget.Cast(root.FindAnyWidget("AccentLine")); }
        if (!HeaderIcon) { HeaderIcon = TextWidget.Cast(root.FindAnyWidget("HeaderIcon")); }
        if (!HeaderTitle) { HeaderTitle = TextWidget.Cast(root.FindAnyWidget("HeaderTitle")); }
        if (!DragHandle) { DragHandle = TextWidget.Cast(root.FindAnyWidget("DragHandle")); }

        // CloseX — child-walk (ButtonWidget child issue)
        Widget closeXBtn = root.FindAnyWidget("BtnCloseX");
        if (closeXBtn)
        {
            Widget closeXChild = closeXBtn.GetChildren();
            ImageWidget closeXImg = null;
            TextWidget closeXTxt = null;
            while (closeXChild)
            {
                if (!closeXImg)
                    closeXImg = ImageWidget.Cast(closeXChild);
                if (!closeXTxt)
                    closeXTxt = TextWidget.Cast(closeXChild);
                closeXChild = closeXChild.GetSibling();
            }
            BtnCloseXBg = closeXImg;
            BtnCloseXText = closeXTxt;
        }

        // Info cards
        if (!CardPriceBg) { CardPriceBg = ImageWidget.Cast(root.FindAnyWidget("CardPriceBg")); }
        if (!CardPriceLabel) { CardPriceLabel = TextWidget.Cast(root.FindAnyWidget("CardPriceLabel")); }
        if (!PriceText) { PriceText = TextWidget.Cast(root.FindAnyWidget("PriceText")); }
        if (!PriceChangeText) { PriceChangeText = TextWidget.Cast(root.FindAnyWidget("PriceChangeText")); }
        if (!CardStockBg) { CardStockBg = ImageWidget.Cast(root.FindAnyWidget("CardStockBg")); }
        if (!CardStockLabel) { CardStockLabel = TextWidget.Cast(root.FindAnyWidget("CardStockLabel")); }
        if (!StockText) { StockText = TextWidget.Cast(root.FindAnyWidget("StockText")); }
        if (!CardBalanceBg) { CardBalanceBg = ImageWidget.Cast(root.FindAnyWidget("CardBalanceBg")); }
        if (!CardBalanceLabel) { CardBalanceLabel = TextWidget.Cast(root.FindAnyWidget("CardBalanceLabel")); }
        if (!BalanceText) { BalanceText = TextWidget.Cast(root.FindAnyWidget("BalanceText")); }
        if (!CardCashBg) { CardCashBg = ImageWidget.Cast(root.FindAnyWidget("CardCashBg")); }
        if (!CardCashLabel) { CardCashLabel = TextWidget.Cast(root.FindAnyWidget("CardCashLabel")); }
        if (!CashEurText) { CashEurText = TextWidget.Cast(root.FindAnyWidget("CashEurText")); }
        if (!CashBtcText) { CashBtcText = TextWidget.Cast(root.FindAnyWidget("CashBtcText")); }

        // Amount section
        if (!AmountBg) { AmountBg = ImageWidget.Cast(root.FindAnyWidget("AmountBg")); }
        if (!AmountLabel) { AmountLabel = TextWidget.Cast(root.FindAnyWidget("AmountLabel")); }
        if (!BtcLabel) { BtcLabel = TextWidget.Cast(root.FindAnyWidget("BtcLabel")); }
        if (!EditBtcBorder) { EditBtcBorder = ImageWidget.Cast(root.FindAnyWidget("EditBtcBorder")); }
        if (!EditBtcBg) { EditBtcBg = ImageWidget.Cast(root.FindAnyWidget("EditBtcBg")); }
        if (!EditBtcAmount) { EditBtcAmount = EditBoxWidget.Cast(root.FindAnyWidget("EditBtcAmount")); }
        if (!ArrowLabel) { ArrowLabel = TextWidget.Cast(root.FindAnyWidget("ArrowLabel")); }
        if (!EurLabel) { EurLabel = TextWidget.Cast(root.FindAnyWidget("EurLabel")); }
        if (!EditEurBorder) { EditEurBorder = ImageWidget.Cast(root.FindAnyWidget("EditEurBorder")); }
        if (!EditEurBg) { EditEurBg = ImageWidget.Cast(root.FindAnyWidget("EditEurBg")); }
        if (!EditEurAmount) { EditEurAmount = EditBoxWidget.Cast(root.FindAnyWidget("EditEurAmount")); }

        // Separator
        if (!SepBuySell) { SepBuySell = ImageWidget.Cast(root.FindAnyWidget("SepBuySell")); }

        // Tabs — child-walk (ButtonWidget children)
        BindTabChildren(root, "TabCash");
        BindTabChildren(root, "TabAccount");

        // Row 1: Buy/Sell BTC — child-walk
        BindButtonChildren3(root, "BtnBuyBtc");
        BindButtonChildren3(root, "BtnSellBtc");

        // Row 2: Withdraw/Deposit EUR — child-walk
        BindButtonChildren3(root, "BtnWithdrawEur");
        BindButtonChildren3(root, "BtnDepositEur");

        // Row 3: Withdraw/Deposit BTC — child-walk
        BindButtonChildren3(root, "BtnWithdrawBtc");
        BindButtonChildren3(root, "BtnDepositBtc");

        // Status
        if (!StatusBg) { StatusBg = ImageWidget.Cast(root.FindAnyWidget("StatusBg")); }
        if (!StatusText) { StatusText = TextWidget.Cast(root.FindAnyWidget("StatusText")); }

        // Footer
        if (!FooterSep) { FooterSep = ImageWidget.Cast(root.FindAnyWidget("FooterSep")); }
        if (!FooterBg) { FooterBg = ImageWidget.Cast(root.FindAnyWidget("FooterBg")); }
        if (!FooterEscHint) { FooterEscHint = TextWidget.Cast(root.FindAnyWidget("FooterEscHint")); }
        if (!FooterBrand) { FooterBrand = TextWidget.Cast(root.FindAnyWidget("FooterBrand")); }
    }

    // ── Tab child-walk helper (Bg + Text) ──
    protected void BindTabChildren(Widget root, string tabName)
    {
        Widget tabW = root.FindAnyWidget(tabName);
        if (!tabW)
            return;
        Widget child = tabW.GetChildren();
        ImageWidget foundBg = null;
        TextWidget foundTxt = null;
        while (child)
        {
            if (!foundBg)
                foundBg = ImageWidget.Cast(child);
            if (!foundTxt)
                foundTxt = TextWidget.Cast(child);
            child = child.GetSibling();
        }

        if (tabName == "TabCash")
        {
            TabCashBg = foundBg;
            TabCashText = foundTxt;
        }
        if (tabName == "TabAccount")
        {
            TabAccountBg = foundBg;
            TabAccountText = foundTxt;
        }
    }

    // ── Button child-walk helper (Bg + Text + Hint) ──
    protected void BindButtonChildren3(Widget root, string btnName)
    {
        Widget btnW = root.FindAnyWidget(btnName);
        if (!btnW)
            return;
        Widget child = btnW.GetChildren();
        ImageWidget foundBg = null;
        TextWidget foundTxt = null;
        TextWidget foundHint = null;
        bool txtFound = false;
        while (child)
        {
            if (!foundBg)
            {
                ImageWidget imgC = ImageWidget.Cast(child);
                if (imgC) { foundBg = imgC; }
            }
            if (!txtFound)
            {
                TextWidget txtC = TextWidget.Cast(child);
                if (txtC) { foundTxt = txtC; txtFound = true; child = child.GetSibling(); continue; }
            }
            else if (!foundHint)
            {
                TextWidget hintC = TextWidget.Cast(child);
                if (hintC) { foundHint = hintC; }
            }
            child = child.GetSibling();
        }

        // Assign to the correct field by name
        if (btnName == "BtnBuyBtc")
        {
            BtnBuyBtcBg = foundBg;
            BtnBuyBtcText = foundTxt;
            BtnBuyBtcHint = foundHint;
        }
        if (btnName == "BtnSellBtc")
        {
            BtnSellBtcBg = foundBg;
            BtnSellBtcText = foundTxt;
            BtnSellBtcHint = foundHint;
        }
        if (btnName == "BtnWithdrawEur")
        {
            BtnWithdrawEurBg = foundBg;
            BtnWithdrawEurText = foundTxt;
            BtnWithdrawEurHint = foundHint;
        }
        if (btnName == "BtnDepositEur")
        {
            BtnDepositEurBg = foundBg;
            BtnDepositEurText = foundTxt;
            BtnDepositEurHint = foundHint;
        }
        if (btnName == "BtnWithdrawBtc")
        {
            BtnWithdrawBtcBg = foundBg;
            BtnWithdrawBtcText = foundTxt;
            BtnWithdrawBtcHint = foundHint;
        }
        if (btnName == "BtnDepositBtc")
        {
            BtnDepositBtcBg = foundBg;
            BtnDepositBtcText = foundTxt;
            BtnDepositBtcHint = foundHint;
        }
    }

    // =========================================================
    // Assign button UserIDs
    // =========================================================
    protected void AssignButtonIDs()
    {
        if (m_ButtonIDsAssigned)
            return;
        Widget root = GetLayoutRoot();
        if (!root)
            return;
        Widget btn = null;

        btn = root.FindAnyWidget("TabCash");
        if (btn) { btn.SetUserID(UID_TAB_CASH); }

        btn = root.FindAnyWidget("TabAccount");
        if (btn) { btn.SetUserID(UID_TAB_ACCOUNT); }

        btn = root.FindAnyWidget("BtnBuyBtc");
        if (btn) { btn.SetUserID(UID_BUY_BTC); }

        btn = root.FindAnyWidget("BtnSellBtc");
        if (btn) { btn.SetUserID(UID_SELL_BTC); }

        btn = root.FindAnyWidget("BtnWithdrawEur");
        if (btn) { btn.SetUserID(UID_WITHDRAW_EUR); }

        btn = root.FindAnyWidget("BtnDepositEur");
        if (btn) { btn.SetUserID(UID_DEPOSIT_EUR); }

        btn = root.FindAnyWidget("BtnWithdrawBtc");
        if (btn) { btn.SetUserID(UID_WITHDRAW_BTC); }

        btn = root.FindAnyWidget("BtnDepositBtc");
        if (btn) { btn.SetUserID(UID_DEPOSIT_BTC); }

        btn = root.FindAnyWidget("BtnCloseX");
        if (btn) { btn.SetUserID(UID_CLOSE_X); }

        m_ButtonIDsAssigned = true;
    }

    // =========================================================
    // Colors
    // =========================================================
    protected void ApplyColors()
    {
        Tint(PanelBg, LFPG_UIPalette.COL_BG_PANEL);
        Tint(HeaderBg, LFPG_UIPalette.COL_HEADER);
        Tint(AccentLine, LFPG_UIPalette.COL_GREEN);
        Tint(BtnCloseXBg, LFPG_UIPalette.COL_BTN);
        if (BtnCloseXText) { BtnCloseXText.SetColor(LFPG_UIPalette.COL_TEXT_DIM); }
        if (HeaderIcon) { HeaderIcon.SetColor(LFPG_UIPalette.COL_AMBER); }
        if (HeaderTitle) { HeaderTitle.SetColor(LFPG_UIPalette.COL_TEXT); }
        if (DragHandle) { DragHandle.SetColor(LFPG_UIPalette.COL_TEXT_DIM); }

        // Info cards
        Tint(CardPriceBg, LFPG_UIPalette.COL_BG_ELEVATED);
        Tint(CardStockBg, LFPG_UIPalette.COL_BG_ELEVATED);
        Tint(CardBalanceBg, LFPG_UIPalette.COL_BG_ELEVATED);
        Tint(CardCashBg, LFPG_UIPalette.COL_BG_ELEVATED);
        if (CardPriceLabel) { CardPriceLabel.SetColor(LFPG_UIPalette.COL_TEXT_DIM); }
        if (CardStockLabel) { CardStockLabel.SetColor(LFPG_UIPalette.COL_TEXT_DIM); }
        if (CardBalanceLabel) { CardBalanceLabel.SetColor(LFPG_UIPalette.COL_TEXT_DIM); }
        if (CardCashLabel) { CardCashLabel.SetColor(LFPG_UIPalette.COL_TEXT_DIM); }
        if (PriceText) { PriceText.SetColor(LFPG_UIPalette.COL_AMBER); }
        if (StockText) { StockText.SetColor(LFPG_UIPalette.COL_BLUE); }
        if (BalanceText) { BalanceText.SetColor(LFPG_UIPalette.COL_GREEN); }
        if (CashEurText) { CashEurText.SetColor(LFPG_UIPalette.COL_GREEN); }
        if (CashBtcText) { CashBtcText.SetColor(LFPG_UIPalette.COL_AMBER); }

        // Amount section
        Tint(AmountBg, LFPG_UIPalette.COL_BG_ELEVATED);
        if (AmountLabel) { AmountLabel.SetColor(LFPG_UIPalette.COL_TEXT_DIM); }
        if (BtcLabel) { BtcLabel.SetColor(LFPG_UIPalette.COL_AMBER); }
        Tint(EditBtcBorder, LFPG_UIPalette.COL_INPUT_BORDER);
        Tint(EditBtcBg, LFPG_UIPalette.COL_BG_INPUT);
        if (EditBtcAmount) { EditBtcAmount.SetColor(LFPG_UIPalette.COL_TEXT); }
        if (ArrowLabel) { ArrowLabel.SetColor(LFPG_UIPalette.COL_TEXT_DIM); }
        if (EurLabel) { EurLabel.SetColor(LFPG_UIPalette.COL_GREEN); }
        Tint(EditEurBorder, LFPG_UIPalette.COL_INPUT_BORDER);
        Tint(EditEurBg, LFPG_UIPalette.COL_BG_INPUT);
        if (EditEurAmount) { EditEurAmount.SetColor(LFPG_UIPalette.COL_TEXT); }

        // Tabs — default: Cash active (green), Account dimmed
        SetTabColors(false);

        // Separator
        Tint(SepBuySell, LFPG_UIPalette.COL_SEPARATOR);

        // Row 1: Buy/Sell BTC
        Tint(BtnBuyBtcBg, LFPG_UIPalette.COL_GREEN_BTN);
        Tint(BtnSellBtcBg, LFPG_UIPalette.COL_RED_BTN);
        if (BtnBuyBtcText) { BtnBuyBtcText.SetColor(LFPG_UIPalette.COL_TEXT); }
        if (BtnSellBtcText) { BtnSellBtcText.SetColor(LFPG_UIPalette.COL_TEXT); }
        if (BtnBuyBtcHint) { BtnBuyBtcHint.SetColor(LFPG_UIPalette.COL_TEXT_MID); }
        if (BtnSellBtcHint) { BtnSellBtcHint.SetColor(LFPG_UIPalette.COL_TEXT_MID); }

        // Row 2: Withdraw/Deposit EUR
        Tint(BtnWithdrawEurBg, LFPG_UIPalette.COL_BLUE_BTN);
        Tint(BtnDepositEurBg, LFPG_UIPalette.COL_BTN);
        if (BtnWithdrawEurText) { BtnWithdrawEurText.SetColor(LFPG_UIPalette.COL_TEXT); }
        if (BtnDepositEurText) { BtnDepositEurText.SetColor(LFPG_UIPalette.COL_TEXT); }
        if (BtnWithdrawEurHint) { BtnWithdrawEurHint.SetColor(LFPG_UIPalette.COL_TEXT_MID); }
        if (BtnDepositEurHint) { BtnDepositEurHint.SetColor(LFPG_UIPalette.COL_TEXT_MID); }

        // Row 3: Withdraw/Deposit BTC
        Tint(BtnWithdrawBtcBg, LFPG_UIPalette.COL_BLUE_BTN);
        Tint(BtnDepositBtcBg, LFPG_UIPalette.COL_BTN);
        if (BtnWithdrawBtcText) { BtnWithdrawBtcText.SetColor(LFPG_UIPalette.COL_TEXT); }
        if (BtnDepositBtcText) { BtnDepositBtcText.SetColor(LFPG_UIPalette.COL_TEXT); }
        if (BtnWithdrawBtcHint) { BtnWithdrawBtcHint.SetColor(LFPG_UIPalette.COL_TEXT_MID); }
        if (BtnDepositBtcHint) { BtnDepositBtcHint.SetColor(LFPG_UIPalette.COL_TEXT_MID); }

        // Status (initially hidden)
        Tint(StatusBg, COL_STATUS_OK_BG);
        if (StatusText) { StatusText.SetColor(LFPG_UIPalette.COL_GREEN); }

        // Footer
        Tint(FooterSep, LFPG_UIPalette.COL_SEPARATOR);
        Tint(FooterBg, LFPG_UIPalette.COL_HEADER);
        if (FooterEscHint) { FooterEscHint.SetColor(LFPG_UIPalette.COL_TEXT_DIM); }
        if (FooterBrand) { FooterBrand.SetColor(LFPG_UIPalette.COL_TEXT_DIM); }

        // A4: Hide "drag" label
        if (DragHandle) { DragHandle.Show(false); }
        // A5: Hide "PowerGrid" brand
        if (FooterBrand) { FooterBrand.Show(false); }

        m_ColorsInitialized = true;
    }

    // ── Tab colors (called from Controller via m_View ref) ──
    void SetTabColors(bool accountMode)
    {
        if (accountMode)
        {
            Tint(TabAccountBg, COL_AMBER_BTN);
            if (TabAccountText) { TabAccountText.SetColor(LFPG_UIPalette.COL_TEXT); }
            Tint(TabCashBg, LFPG_UIPalette.COL_BTN);
            if (TabCashText) { TabCashText.SetColor(LFPG_UIPalette.COL_TEXT_DIM); }
        }
        else
        {
            Tint(TabCashBg, LFPG_UIPalette.COL_GREEN_BTN);
            if (TabCashText) { TabCashText.SetColor(LFPG_UIPalette.COL_TEXT); }
            Tint(TabAccountBg, LFPG_UIPalette.COL_BTN);
            if (TabAccountText) { TabAccountText.SetColor(LFPG_UIPalette.COL_TEXT_DIM); }
        }
    }

    void Tint(ImageWidget img, int color)
    {
        if (!img)
            return;
        if (!m_ColorsInitialized)
            img.LoadImageFile(0, PROC_WHITE);
        img.SetColor(color);
        CacheColorLocal(img, color);
    }

    // =========================================================
    // Hover color cache (O(1) per-widget via SetUserData)
    //
    // WARNING (2026-04-26): do NOT add m_ColorDataRefs.Clear() in
    // ApplyColors or anywhere else without first nulling SetUserData on
    // every widget that received it. The engine stores SetUserData as a
    // raw pointer (not a Managed soft link); clearing the strong refs
    // while widgets still point at the data leaves dangling pointers,
    // and the next GetUserData/Cast(rawData) below will read freed
    // memory → heap corruption (SEH 0xc0000374). See LFPG_SorterView.c
    // F1-B for the tracked-widgets fix pattern. This view is currently
    // safe ONLY because ColorData persists across Opens (no Clear).
    // =========================================================
    protected void CacheColorLocal(Widget w, int color)
    {
        if (!w)
            return;
        Class rawData = null;
        w.GetUserData(rawData);
        LFPG_ColorData existing = LFPG_ColorData.Cast(rawData);
        if (existing)
        {
            existing.m_BaseColor = color;
            return;
        }
        LFPG_ColorData data = new LFPG_ColorData(color);
        w.SetUserData(data);
        m_ColorDataRefs.Insert(data);
    }

    override protected int FindCachedColor(Widget w)
    {
        if (!w)
            return 0;
        Class rawData = null;
        w.GetUserData(rawData);
        LFPG_ColorData colorData = LFPG_ColorData.Cast(rawData);
        if (!colorData)
            return 0;
        return colorData.m_BaseColor;
    }

    // =========================================================
    // OnClick dispatch (manual, no Relay_Command)
    // =========================================================
    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (!m_IsOpen)
            return super.OnClick(w, x, y, button);
        if (!w)
            return super.OnClick(w, x, y, button);
        if (button != 0)
            return super.OnClick(w, x, y, button);

        // Find enclosing ButtonWidget
        Widget check = w;
        ButtonWidget btn = null;
        while (check)
        {
            btn = ButtonWidget.Cast(check);
            if (btn)
                break;
            check = check.GetParent();
        }
        if (!btn)
            return super.OnClick(w, x, y, button);

        LFPG_BTCAtmController ctrl = LFPG_BTCAtmController.Cast(GetController());
        if (!ctrl)
            return super.OnClick(w, x, y, button);

        int uid = btn.GetUserID();

        if (uid == UID_TAB_CASH)      { ctrl.OnTabClick(false);         return true; }
        if (uid == UID_TAB_ACCOUNT)   { ctrl.OnTabClick(true);          return true; }
        if (uid == UID_BUY_BTC)       { ctrl.OnBuyClick();              return true; }
        if (uid == UID_SELL_BTC)      { ctrl.OnSellClick();             return true; }
        if (uid == UID_WITHDRAW_EUR)  { ctrl.OnWithdrawEurClick();      return true; }
        if (uid == UID_DEPOSIT_EUR)   { ctrl.OnDepositEurClick();       return true; }
        if (uid == UID_WITHDRAW_BTC)  { ctrl.OnWithdrawBtcClick();      return true; }
        if (uid == UID_DEPOSIT_BTC)   { ctrl.OnDepositBtcClick();       return true; }
        if (uid == UID_CLOSE_X)       { DoClose();                      return true; }

        return super.OnClick(w, x, y, button);
    }

    // =========================================================
    // OnChange: EditBox auto-conversion (no more checkbox)
    // =========================================================
    override bool OnChange(Widget w, int x, int y, bool finished)
    {
        if (!m_IsOpen)
            return false;

        LFPG_BTCAtmController ctrl = LFPG_BTCAtmController.Cast(GetController());
        if (!ctrl)
            return false;

        if (w == EditBtcAmount)
        {
            ctrl.OnBtcAmountChanged();
            return false;
        }
        if (w == EditEurAmount)
        {
            ctrl.OnEurAmountChanged();
            return false;
        }
        return false;
    }

    // =========================================================
    // Drag: MouseDown on header
    // =========================================================

    // =========================================================
    // Hover feedback (buttons + tabs)
    // =========================================================

    // =========================================================
    // Helpers
    // =========================================================

    static int LightenARGB(int color, int amount)
    {
        return LFPG_FloatingViewBase.LFPG_SharedLightenARGB(color, amount);
    }

    // =========================================================
    // Input lock
    // =========================================================
    protected void ShowCursor()
    {
        #ifndef SERVER
        if (!g_Game)
            return;
        UIManager uiMgr = g_Game.GetUIManager();
        if (uiMgr)
            uiMgr.ShowUICursor(true);
        if (!m_FocusLocked)
        {
            Input inp = g_Game.GetInput();
            if (inp)
                inp.ChangeGameFocus(1);
            m_FocusLocked = true;
        }
        #endif
    }

    protected void HideCursor()
    {
        #ifndef SERVER
        if (!g_Game)
            return;
        UIManager uiMgr = g_Game.GetUIManager();
        if (uiMgr)
            uiMgr.ShowUICursor(false);
        if (m_FocusLocked)
        {
            Input inp = g_Game.GetInput();
            if (inp)
                inp.ChangeGameFocus(-1);
            m_FocusLocked = false;
        }
        #endif
    }

    // =========================================================
    // Singleton lifecycle
    // =========================================================
    static void Init()
    {
        #ifndef SERVER
        if (s_Instance)
            return;
        s_Instance = new LFPG_BTCAtmView();
        Widget root = s_Instance.GetLayoutRoot();
        if (root)
        {
            root.Show(false);
            root.SetSort(50001);
        }
        string initMsg = "[BTCAtmView] Pre-created (hidden)";
        LFPG_Util.Info(initMsg);
        #endif
    }

    static void Open()
    {
        #ifndef SERVER
        if (!s_Instance)
        {
            string warnMsg = "[BTCAtmView] Open before Init";
            LFPG_Util.Warn(warnMsg);
            return;
        }
        s_Instance.DoOpen();
        #endif
    }

    static void Close()
    {
        if (s_Instance)
            s_Instance.DoClose();
    }

    static bool IsOpen()
    {
        if (!s_Instance)
            return false;
        return s_Instance.m_IsOpen;
    }

    static bool HandleEscKey()
    {
        if (!s_Instance)
            return false;
        if (!s_Instance.m_IsOpen)
            return false;

        Widget focused = GetFocus();
        if (focused)
        {
            EditBoxWidget editCheck = EditBoxWidget.Cast(focused);
            if (editCheck)
            {
                SetFocus(null);
                return true;
            }
        }
        if (g_Game)
            s_EscCloseTime = g_Game.GetTickTime();
        s_Instance.DoClose();
        return true;
    }

    static bool IsEscCooldown()
    {
        return LFPG_FloatingViewBase.LFPG_SharedEscCooldown(s_EscCloseTime);
    }

    static void Cleanup()
    {
        if (s_Instance)
        {
            s_Instance.m_IsOpen = false;
            s_Instance.m_Dragging = false;
            s_Instance.m_FadingIn = false;
        }
        s_Instance = null;
    }

    static void OnTxResult()
    {
        if (!s_Instance)
            return;
        if (!s_Instance.m_IsOpen)
            return;
        LFPG_BTCAtmController ctrl = LFPG_BTCAtmController.Cast(s_Instance.GetController());
        if (ctrl)
            ctrl.RefreshFromClientData();
    }

    static void OnPriceUnavailable()
    {
        OnTxResult();
    }

    // =========================================================
    // DoOpen / DoClose
    // =========================================================
    protected void DoOpen()
    {
        if (m_IsOpen)
            return;
        Widget root = GetLayoutRoot();
        if (!root)
        {
            LFPG_Util.Error("[BTCAtmView] No layout root");
            return;
        }
        m_IsOpen = true;
        m_Dragging = false;
        m_HoveredBg = null;
        root.Show(true);

        CenterPanel();

        m_FadeAlpha = 0.0;
        m_FadingIn = true;
        if (BTCAtmPanel)
            BTCAtmPanel.SetAlpha(0.0);

        ShowCursor();

        #ifndef SERVER
        if (g_Game && g_Game.GetMission())
        {
            g_Game.GetMission().PlayerControlDisable(INPUT_EXCLUDE_ALL);
            m_ControlsLocked = true;
        }
        #endif

        EnsureViewBindings();
        AssignButtonIDs();
        ApplyColors();

        LFPG_BTCAtmController ctrl = LFPG_BTCAtmController.Cast(GetController());
        if (ctrl)
        {
            ctrl.InitWidgetRefs(this);
            ctrl.ResetTxState();
            ctrl.RefreshFromClientData();
        }

        // Clear EditBoxes on open
        if (EditBtcAmount)
        {
            EditBtcAmount.SetText("");
        }
        if (EditEurAmount)
        {
            EditEurAmount.SetText("");
        }

        LFPG_Util.Info("[BTCAtmView] Opened");
    }

    protected void DoClose()
    {
        if (!m_IsOpen)
            return;
        m_IsOpen = false;
        m_Dragging = false;
        m_FadingIn = false;
        m_HoveredBg = null;

        Widget root = GetLayoutRoot();
        if (root)
            root.Show(false);

        #ifndef SERVER
        if (m_ControlsLocked && g_Game && g_Game.GetMission())
        {
            g_Game.GetMission().PlayerControlEnable(false);
            m_ControlsLocked = false;
        }
        #endif

        HideCursor();
        LFPG_Util.Info("[BTCAtmView] Closed");
    }
    override protected Widget LFPG_ViewPanel()
    {
        return BTCAtmPanel;
    }
    override protected Widget LFPG_ViewHeader()
    {
        return HeaderFrame;
    }
};
#endif
