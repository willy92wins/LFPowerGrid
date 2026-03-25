// =========================================================
// LF_PowerGrid — BTC ATM View (Sprint BTC-4, Dabs MVC)
//
// ScriptView floating window. Sorter pattern simplified.
// Singleton: Init() pre-creates hidden, Open()/Close() toggle.
// Drag header, fade-in, hover feedback, cursor lock.
// All button dispatch via OnClick → Controller methods.
// =========================================================

class LFPG_BTCAtmView extends ScriptView
{
    protected static ref LFPG_BTCAtmView s_Instance;
    protected bool m_IsOpen;
    protected bool m_FocusLocked;

    // ── Drag state ──
    protected bool m_Dragging;
    protected float m_DragOffX;
    protected float m_DragOffY;

    // ── Fade-in ──
    protected float m_FadeAlpha;
    protected bool m_FadingIn;

    // ── Hover ──
    protected ref array<ref LFPG_ColorData> m_ColorDataRefs;
    protected ImageWidget m_HoveredBg;

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
    ImageWidget CardStockBg;
    TextWidget CardStockLabel;
    TextWidget StockText;
    ImageWidget CardBalanceBg;
    TextWidget CardBalanceLabel;
    TextWidget BalanceText;
    ImageWidget CardCashBg;
    TextWidget CardCashLabel;
    TextWidget CashText;

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

    // Checkbox
    CheckBoxWidget ChkToAccount;
    TextWidget ChkToAccountLabel;

    // Buttons (bg refs for coloring)
    ImageWidget BtnBuyBg;
    TextWidget BtnBuyText;
    TextWidget BtnBuyHint;
    ImageWidget BtnSellBg;
    TextWidget BtnSellText;
    TextWidget BtnSellHint;
    ImageWidget BtnWithdrawBg;
    TextWidget BtnWithdrawText;
    TextWidget BtnWithdrawHint;
    ImageWidget BtnDepositBg;
    TextWidget BtnDepositText;
    TextWidget BtnDepositHint;

    // Status
    ImageWidget StatusBg;
    TextWidget StatusText;

    // Footer
    ImageWidget FooterSep;
    ImageWidget FooterBg;
    TextWidget FooterEscHint;
    TextWidget FooterBrand;

    // ── Palette (shared with Sorter) ──
    static const string PROC_WHITE = "#(argb,8,8,3)color(1,1,1,1,CO)";
    static const int COL_BG_PANEL     = 0xF5121C36;
    static const int COL_BG_ELEVATED  = 0xE61E2B41;
    static const int COL_BG_INPUT     = 0xFF202E4C;
    static const int COL_INPUT_BORDER = 0x4CCBD5E1;
    static const int COL_HEADER       = 0xF50F172B;
    static const int COL_GREEN        = 0xFF34D399;
    static const int COL_BLUE         = 0xFF60A5FA;
    static const int COL_AMBER        = 0xFFFBBF24;
    static const int COL_RED          = 0xFFF87171;
    static const int COL_TEXT         = 0xFFF1F5F9;
    static const int COL_TEXT_DIM     = 0xFF7A8A9B;
    static const int COL_TEXT_MID     = 0xFFB0BEC5;
    static const int COL_SEPARATOR    = 0x43CBD5E1;
    static const int COL_BTN          = 0xFF374B6F;
    static const int COL_GREEN_BTN    = 0xFF087C5B;
    static const int COL_RED_BTN      = 0xFFC72323;
    static const int COL_BLUE_BTN     = 0xFF274B7C;
    static const int COL_STATUS_OK_BG = 0x1734D399;
    static const int COL_STATUS_ERR_BG = 0x17F87171;

    // ── Button UserID constants ──
    static const int UID_BUY      = 100;
    static const int UID_SELL     = 101;
    static const int UID_WITHDRAW = 102;
    static const int UID_DEPOSIT  = 103;
    static const int UID_CLOSE_X  = 110;

    protected bool m_ColorsInitialized;
    protected bool m_ButtonIDsAssigned;

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
        if (GetGame())
        {
            PlayerBase dtorPlayer = PlayerBase.Cast(GetGame().GetPlayer());
            if (dtorPlayer)
            {
                HumanInputController hicDtor = dtorPlayer.GetInputController();
                if (hicDtor)
                {
                    hicDtor.SetDisabled(false);
                }
            }
            if (m_FocusLocked)
            {
                Input inp = GetGame().GetInput();
                if (inp)
                {
                    inp.ChangeGameFocus(-1);
                }
                UIManager uiMgr = GetGame().GetUIManager();
                if (uiMgr)
                {
                    uiMgr.ShowUICursor(false);
                }
                m_FocusLocked = false;
            }
        }
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
            {
                BTCAtmPanel.SetAlpha(m_FadeAlpha);
            }
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
            {
                BTCAtmPanel.SetPos(clampedX, clampedY);
            }
        }

        // Controller timers (status feedback)
        LFPG_BTCAtmController ctrl = LFPG_BTCAtmController.Cast(GetController());
        if (ctrl)
        {
            ctrl.TickTimers(dt);
        }
    }

    // =========================================================
    // Manual widget binding (Dabs auto-bind misses ButtonWidget children)
    // =========================================================
    protected void EnsureViewBindings()
    {
        Widget root = GetLayoutRoot();
        if (!root)
            return;

        string wn = "";

        wn = "BTCAtmPanel";
        if (!BTCAtmPanel) { BTCAtmPanel = root.FindAnyWidget(wn); }
        wn = "HeaderFrame";
        if (!HeaderFrame) { HeaderFrame = root.FindAnyWidget(wn); }
        wn = "PanelBg";
        if (!PanelBg) { PanelBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "HeaderBg";
        if (!HeaderBg) { HeaderBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "AccentLine";
        if (!AccentLine) { AccentLine = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "HeaderIcon";
        if (!HeaderIcon) { HeaderIcon = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "HeaderTitle";
        if (!HeaderTitle) { HeaderTitle = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "DragHandle";
        if (!DragHandle) { DragHandle = TextWidget.Cast(root.FindAnyWidget(wn)); }

        // CloseX — child-walk (ButtonWidget child issue)
        wn = "BtnCloseX";
        Widget closeXBtn = root.FindAnyWidget(wn);
        if (closeXBtn)
        {
            Widget closeXChild = closeXBtn.GetChildren();
            ImageWidget closeXImg = null;
            TextWidget closeXTxt = null;
            while (closeXChild)
            {
                if (!closeXImg)
                {
                    closeXImg = ImageWidget.Cast(closeXChild);
                }
                if (!closeXTxt)
                {
                    closeXTxt = TextWidget.Cast(closeXChild);
                }
                closeXChild = closeXChild.GetSibling();
            }
            BtnCloseXBg = closeXImg;
            BtnCloseXText = closeXTxt;
        }

        // Info cards
        wn = "CardPriceBg";
        if (!CardPriceBg) { CardPriceBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "CardPriceLabel";
        if (!CardPriceLabel) { CardPriceLabel = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "PriceText";
        if (!PriceText) { PriceText = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "CardStockBg";
        if (!CardStockBg) { CardStockBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "CardStockLabel";
        if (!CardStockLabel) { CardStockLabel = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "StockText";
        if (!StockText) { StockText = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "CardBalanceBg";
        if (!CardBalanceBg) { CardBalanceBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "CardBalanceLabel";
        if (!CardBalanceLabel) { CardBalanceLabel = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "BalanceText";
        if (!BalanceText) { BalanceText = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "CardCashBg";
        if (!CardCashBg) { CardCashBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "CardCashLabel";
        if (!CardCashLabel) { CardCashLabel = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "CashText";
        if (!CashText) { CashText = TextWidget.Cast(root.FindAnyWidget(wn)); }

        // Amount section
        wn = "AmountBg";
        if (!AmountBg) { AmountBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "AmountLabel";
        if (!AmountLabel) { AmountLabel = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "BtcLabel";
        if (!BtcLabel) { BtcLabel = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EditBtcBorder";
        if (!EditBtcBorder) { EditBtcBorder = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EditBtcBg";
        if (!EditBtcBg) { EditBtcBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EditBtcAmount";
        if (!EditBtcAmount) { EditBtcAmount = EditBoxWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "ArrowLabel";
        if (!ArrowLabel) { ArrowLabel = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EurLabel";
        if (!EurLabel) { EurLabel = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EditEurBorder";
        if (!EditEurBorder) { EditEurBorder = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EditEurBg";
        if (!EditEurBg) { EditEurBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EditEurAmount";
        if (!EditEurAmount) { EditEurAmount = EditBoxWidget.Cast(root.FindAnyWidget(wn)); }

        // Checkbox
        wn = "ChkToAccount";
        if (!ChkToAccount) { ChkToAccount = CheckBoxWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "ChkToAccountLabel";
        if (!ChkToAccountLabel) { ChkToAccountLabel = TextWidget.Cast(root.FindAnyWidget(wn)); }

        // Buttons — inline child-walk (F2: out params risky in Enforce Script)
        // BtnBuy
        wn = "BtnBuy";
        Widget btnBuy = root.FindAnyWidget(wn);
        if (btnBuy)
        {
            Widget buyChild = btnBuy.GetChildren();
            bool buyTxtFound = false;
            while (buyChild)
            {
                if (!BtnBuyBg)
                {
                    ImageWidget buyImg = ImageWidget.Cast(buyChild);
                    if (buyImg) { BtnBuyBg = buyImg; }
                }
                if (!buyTxtFound)
                {
                    TextWidget buyTxt = TextWidget.Cast(buyChild);
                    if (buyTxt) { BtnBuyText = buyTxt; buyTxtFound = true; buyChild = buyChild.GetSibling(); continue; }
                }
                else if (!BtnBuyHint)
                {
                    TextWidget buyHint = TextWidget.Cast(buyChild);
                    if (buyHint) { BtnBuyHint = buyHint; }
                }
                buyChild = buyChild.GetSibling();
            }
        }

        // BtnSell
        wn = "BtnSell";
        Widget btnSell = root.FindAnyWidget(wn);
        if (btnSell)
        {
            Widget sellChild = btnSell.GetChildren();
            bool sellTxtFound = false;
            while (sellChild)
            {
                if (!BtnSellBg)
                {
                    ImageWidget sellImg = ImageWidget.Cast(sellChild);
                    if (sellImg) { BtnSellBg = sellImg; }
                }
                if (!sellTxtFound)
                {
                    TextWidget sellTxt = TextWidget.Cast(sellChild);
                    if (sellTxt) { BtnSellText = sellTxt; sellTxtFound = true; sellChild = sellChild.GetSibling(); continue; }
                }
                else if (!BtnSellHint)
                {
                    TextWidget sellHintW = TextWidget.Cast(sellChild);
                    if (sellHintW) { BtnSellHint = sellHintW; }
                }
                sellChild = sellChild.GetSibling();
            }
        }

        // BtnWithdraw
        wn = "BtnWithdraw";
        Widget btnWd = root.FindAnyWidget(wn);
        if (btnWd)
        {
            Widget wdChild = btnWd.GetChildren();
            bool wdTxtFound = false;
            while (wdChild)
            {
                if (!BtnWithdrawBg)
                {
                    ImageWidget wdImg = ImageWidget.Cast(wdChild);
                    if (wdImg) { BtnWithdrawBg = wdImg; }
                }
                if (!wdTxtFound)
                {
                    TextWidget wdTxt = TextWidget.Cast(wdChild);
                    if (wdTxt) { BtnWithdrawText = wdTxt; wdTxtFound = true; wdChild = wdChild.GetSibling(); continue; }
                }
                else if (!BtnWithdrawHint)
                {
                    TextWidget wdHintW = TextWidget.Cast(wdChild);
                    if (wdHintW) { BtnWithdrawHint = wdHintW; }
                }
                wdChild = wdChild.GetSibling();
            }
        }

        // BtnDeposit
        wn = "BtnDeposit";
        Widget btnDep = root.FindAnyWidget(wn);
        if (btnDep)
        {
            Widget depChild = btnDep.GetChildren();
            bool depTxtFound = false;
            while (depChild)
            {
                if (!BtnDepositBg)
                {
                    ImageWidget depImg = ImageWidget.Cast(depChild);
                    if (depImg) { BtnDepositBg = depImg; }
                }
                if (!depTxtFound)
                {
                    TextWidget depTxt = TextWidget.Cast(depChild);
                    if (depTxt) { BtnDepositText = depTxt; depTxtFound = true; depChild = depChild.GetSibling(); continue; }
                }
                else if (!BtnDepositHint)
                {
                    TextWidget depHintW = TextWidget.Cast(depChild);
                    if (depHintW) { BtnDepositHint = depHintW; }
                }
                depChild = depChild.GetSibling();
            }
        }

        // Status
        wn = "StatusBg";
        if (!StatusBg) { StatusBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "StatusText";
        if (!StatusText) { StatusText = TextWidget.Cast(root.FindAnyWidget(wn)); }

        // Footer
        wn = "FooterSep";
        if (!FooterSep) { FooterSep = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "FooterBg";
        if (!FooterBg) { FooterBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "FooterEscHint";
        if (!FooterEscHint) { FooterEscHint = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "FooterBrand";
        if (!FooterBrand) { FooterBrand = TextWidget.Cast(root.FindAnyWidget(wn)); }
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
        string wn = "";
        Widget btn = null;

        wn = "BtnBuy";
        btn = root.FindAnyWidget(wn);
        if (btn) { btn.SetUserID(UID_BUY); }

        wn = "BtnSell";
        btn = root.FindAnyWidget(wn);
        if (btn) { btn.SetUserID(UID_SELL); }

        wn = "BtnWithdraw";
        btn = root.FindAnyWidget(wn);
        if (btn) { btn.SetUserID(UID_WITHDRAW); }

        wn = "BtnDeposit";
        btn = root.FindAnyWidget(wn);
        if (btn) { btn.SetUserID(UID_DEPOSIT); }

        wn = "BtnCloseX";
        btn = root.FindAnyWidget(wn);
        if (btn) { btn.SetUserID(UID_CLOSE_X); }

        m_ButtonIDsAssigned = true;
    }

    // =========================================================
    // Colors
    // =========================================================
    protected void ApplyColors()
    {
        Tint(PanelBg, COL_BG_PANEL);
        Tint(HeaderBg, COL_HEADER);
        Tint(AccentLine, COL_GREEN);
        Tint(BtnCloseXBg, COL_BTN);
        if (BtnCloseXText) { BtnCloseXText.SetColor(COL_TEXT_DIM); }
        if (HeaderIcon) { HeaderIcon.SetColor(COL_AMBER); }
        if (HeaderTitle) { HeaderTitle.SetColor(COL_TEXT); }
        if (DragHandle) { DragHandle.SetColor(COL_TEXT_DIM); }

        // Info cards
        Tint(CardPriceBg, COL_BG_ELEVATED);
        Tint(CardStockBg, COL_BG_ELEVATED);
        Tint(CardBalanceBg, COL_BG_ELEVATED);
        Tint(CardCashBg, COL_BG_ELEVATED);
        if (CardPriceLabel) { CardPriceLabel.SetColor(COL_TEXT_DIM); }
        if (CardStockLabel) { CardStockLabel.SetColor(COL_TEXT_DIM); }
        if (CardBalanceLabel) { CardBalanceLabel.SetColor(COL_TEXT_DIM); }
        if (CardCashLabel) { CardCashLabel.SetColor(COL_TEXT_DIM); }
        if (PriceText) { PriceText.SetColor(COL_GREEN); }
        if (StockText) { StockText.SetColor(COL_BLUE); }
        if (BalanceText) { BalanceText.SetColor(COL_TEXT); }
        if (CashText) { CashText.SetColor(COL_AMBER); }

        // Amount section
        Tint(AmountBg, COL_BG_ELEVATED);
        if (AmountLabel) { AmountLabel.SetColor(COL_TEXT_DIM); }
        if (BtcLabel) { BtcLabel.SetColor(COL_AMBER); }
        Tint(EditBtcBorder, COL_INPUT_BORDER);
        Tint(EditBtcBg, COL_BG_INPUT);
        if (EditBtcAmount) { EditBtcAmount.SetColor(COL_TEXT); }
        if (ArrowLabel) { ArrowLabel.SetColor(COL_TEXT_DIM); }
        if (EurLabel) { EurLabel.SetColor(COL_GREEN); }
        Tint(EditEurBorder, COL_INPUT_BORDER);
        Tint(EditEurBg, COL_BG_INPUT);
        if (EditEurAmount) { EditEurAmount.SetColor(COL_TEXT); }

        // Checkbox
        if (ChkToAccountLabel) { ChkToAccountLabel.SetColor(COL_TEXT_MID); }

        // Buttons
        Tint(BtnBuyBg, COL_GREEN_BTN);
        Tint(BtnSellBg, COL_RED_BTN);
        Tint(BtnWithdrawBg, COL_BLUE_BTN);
        Tint(BtnDepositBg, COL_BTN);
        if (BtnBuyText) { BtnBuyText.SetColor(COL_TEXT); }
        if (BtnSellText) { BtnSellText.SetColor(COL_TEXT); }
        if (BtnWithdrawText) { BtnWithdrawText.SetColor(COL_TEXT); }
        if (BtnDepositText) { BtnDepositText.SetColor(COL_TEXT); }
        if (BtnBuyHint) { BtnBuyHint.SetColor(COL_TEXT_MID); }
        if (BtnSellHint) { BtnSellHint.SetColor(COL_TEXT_MID); }
        if (BtnWithdrawHint) { BtnWithdrawHint.SetColor(COL_TEXT_MID); }
        if (BtnDepositHint) { BtnDepositHint.SetColor(COL_TEXT_MID); }

        // Status (initially hidden)
        Tint(StatusBg, COL_STATUS_OK_BG);
        if (StatusText) { StatusText.SetColor(COL_GREEN); }

        // Footer
        Tint(FooterSep, COL_SEPARATOR);
        Tint(FooterBg, COL_HEADER);
        if (FooterEscHint) { FooterEscHint.SetColor(COL_TEXT_DIM); }
        if (FooterBrand) { FooterBrand.SetColor(COL_TEXT_DIM); }

        m_ColorsInitialized = true;
    }

    protected void Tint(ImageWidget img, int color)
    {
        if (!img)
            return;
        if (!m_ColorsInitialized)
        {
            img.LoadImageFile(0, PROC_WHITE);
        }
        img.SetColor(color);
        CacheColorLocal(img, color);
    }

    // =========================================================
    // Hover color cache (O(1) per-widget via SetUserData)
    // Reuses LFPG_ColorData from SorterView (same class).
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

    protected int FindCachedColor(Widget w)
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
        {
            return super.OnClick(w, x, y, button);
        }
        if (!w)
        {
            return super.OnClick(w, x, y, button);
        }
        if (button != 0)
        {
            return super.OnClick(w, x, y, button);
        }

        // Find enclosing ButtonWidget
        Widget check = w;
        ButtonWidget btn = null;
        while (check)
        {
            btn = ButtonWidget.Cast(check);
            if (btn)
            {
                break;
            }
            check = check.GetParent();
        }
        if (!btn)
        {
            return super.OnClick(w, x, y, button);
        }

        LFPG_BTCAtmController ctrl = LFPG_BTCAtmController.Cast(GetController());
        if (!ctrl)
        {
            return super.OnClick(w, x, y, button);
        }

        int uid = btn.GetUserID();

        if (uid == UID_BUY)      { ctrl.OnBuyClick();      return true; }
        if (uid == UID_SELL)     { ctrl.OnSellClick();     return true; }
        if (uid == UID_WITHDRAW) { ctrl.OnWithdrawClick(); return true; }
        if (uid == UID_DEPOSIT)  { ctrl.OnDepositClick();  return true; }
        if (uid == UID_CLOSE_X)  { DoClose();              return true; }

        return super.OnClick(w, x, y, button);
    }

    // =========================================================
    // OnChange: EditBox auto-conversion
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
    override bool OnMouseButtonDown(Widget w, int x, int y, int button)
    {
        if (!m_IsOpen)
            return false;

        if (button == 0)
        {
            if (IsHeaderWidget(w))
            {
                m_Dragging = true;
                if (m_HoveredBg)
                {
                    int restoreCol = FindCachedColor(m_HoveredBg);
                    if (restoreCol != 0)
                    {
                        m_HoveredBg.SetColor(restoreCol);
                    }
                    m_HoveredBg = null;
                }
                float px = 0.0;
                float py = 0.0;
                if (BTCAtmPanel)
                {
                    BTCAtmPanel.GetPos(px, py);
                }
                m_DragOffX = x - px;
                m_DragOffY = y - py;
            }
        }

        if (IsInteractiveWidget(w))
        {
            return false;
        }
        return true;
    }

    override bool OnMouseButtonUp(Widget w, int x, int y, int button)
    {
        if (button == 0)
        {
            m_Dragging = false;
        }
        if (!m_IsOpen)
            return false;

        if (IsInteractiveWidget(w))
        {
            return false;
        }
        return true;
    }

    // =========================================================
    // Hover feedback
    // =========================================================
    override bool OnMouseEnter(Widget w, int x, int y)
    {
        if (!m_IsOpen)
            return false;

        ImageWidget bg = FindButtonBg(w);
        int baseColor = 0;
        int hoverColor = 0;
        if (bg)
        {
            if (m_HoveredBg && m_HoveredBg != bg)
            {
                baseColor = FindCachedColor(m_HoveredBg);
                if (baseColor != 0)
                {
                    m_HoveredBg.SetColor(baseColor);
                }
                m_HoveredBg = null;
                baseColor = 0;
            }
            baseColor = FindCachedColor(bg);
            if (baseColor != 0)
            {
                m_HoveredBg = bg;
                hoverColor = LightenARGB(baseColor, 20);
                bg.SetColor(hoverColor);
            }
        }
        return false;
    }

    override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
    {
        if (!m_IsOpen)
            return false;

        int baseColor = 0;
        if (m_HoveredBg)
        {
            baseColor = FindCachedColor(m_HoveredBg);
            if (baseColor != 0)
            {
                m_HoveredBg.SetColor(baseColor);
            }
            m_HoveredBg = null;
        }
        return false;
    }

    // =========================================================
    // Helpers
    // =========================================================
    protected bool IsHeaderWidget(Widget w)
    {
        if (!w)
            return false;
        if (!HeaderFrame)
            return false;

        Widget check = w;
        ButtonWidget btnCheck = null;
        while (check)
        {
            btnCheck = ButtonWidget.Cast(check);
            if (btnCheck)
            {
                return false;
            }
            if (check == HeaderFrame)
            {
                return true;
            }
            check = check.GetParent();
        }
        return false;
    }

    protected bool IsInteractiveWidget(Widget w)
    {
        if (!w)
            return false;

        Widget check = w;
        ButtonWidget btnCast = null;
        EditBoxWidget editCast = null;
        CheckBoxWidget chkCast = null;
        while (check)
        {
            btnCast = ButtonWidget.Cast(check);
            if (btnCast)
            {
                return true;
            }
            editCast = EditBoxWidget.Cast(check);
            if (editCast)
            {
                return true;
            }
            chkCast = CheckBoxWidget.Cast(check);
            if (chkCast)
            {
                return true;
            }
            check = check.GetParent();
        }
        return false;
    }

    protected ImageWidget FindButtonBg(Widget w)
    {
        if (!w)
            return null;

        Widget check = w;
        ButtonWidget btn = null;
        while (check)
        {
            btn = ButtonWidget.Cast(check);
            if (btn)
            {
                break;
            }
            check = check.GetParent();
        }
        if (!btn)
            return null;

        Widget child = btn.GetChildren();
        if (!child)
            return null;

        ImageWidget bg = ImageWidget.Cast(child);
        return bg;
    }

    protected void ClampPanelPos(float inX, float inY, float minY, out float outX, out float outY)
    {
        int scrW = 0;
        int scrH = 0;
        GetScreenSize(scrW, scrH);
        float panW = 0.0;
        float panH = 0.0;
        if (BTCAtmPanel)
        {
            BTCAtmPanel.GetSize(panW, panH);
        }

        float maxX = scrW - panW;
        float maxY = scrH - panH;
        float dpiCapX = panW;
        float dpiCapY = panH * 0.5;
        if (maxX > dpiCapX) { maxX = dpiCapX; }
        if (maxY > dpiCapY) { maxY = dpiCapY; }
        if (maxX < 0.0) { maxX = 0.0; }
        if (maxY < minY) { maxY = minY; }

        outX = inX;
        outY = inY;
        if (outX < 0.0) { outX = 0.0; }
        if (outY < minY) { outY = minY; }
        if (outX > maxX) { outX = maxX; }
        if (outY > maxY) { outY = maxY; }
    }

    protected void CenterPanel()
    {
        if (!BTCAtmPanel)
            return;
        int scrW = 0;
        int scrH = 0;
        GetScreenSize(scrW, scrH);
        float panW = 0.0;
        float panH = 0.0;
        BTCAtmPanel.GetSize(panW, panH);

        float cx = (scrW - panW) * 0.5;
        float cy = (scrH - panH) * 0.5;
        float clampedX = 0.0;
        float clampedY = 0.0;
        float minY = 0.0;
        ClampPanelPos(cx, cy, minY, clampedX, clampedY);
        BTCAtmPanel.SetPos(clampedX, clampedY);
    }

    static int LightenARGB(int color, int amount)
    {
        int a = (color >> 24) & 0xFF;
        int r = (color >> 16) & 0xFF;
        int g = (color >> 8) & 0xFF;
        int b = color & 0xFF;

        r = r + amount;
        g = g + amount;
        b = b + amount;

        if (r > 255) { r = 255; }
        if (g > 255) { g = 255; }
        if (b > 255) { b = 255; }

        return (a << 24) | (r << 16) | (g << 8) | b;
    }

    // =========================================================
    // Input lock
    // =========================================================
    protected void ShowCursor()
    {
        #ifndef SERVER
        if (!GetGame())
            return;
        UIManager uiMgr = GetGame().GetUIManager();
        if (uiMgr)
        {
            uiMgr.ShowUICursor(true);
        }
        if (!m_FocusLocked)
        {
            Input inp = GetGame().GetInput();
            if (inp)
            {
                inp.ChangeGameFocus(1);
            }
            m_FocusLocked = true;
        }
        #endif
    }

    protected void HideCursor()
    {
        #ifndef SERVER
        if (!GetGame())
            return;
        UIManager uiMgr = GetGame().GetUIManager();
        if (uiMgr)
        {
            uiMgr.ShowUICursor(false);
        }
        if (m_FocusLocked)
        {
            Input inp = GetGame().GetInput();
            if (inp)
            {
                inp.ChangeGameFocus(-1);
            }
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
        {
            s_Instance.DoClose();
        }
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

        // If EditBox focused, unfocus first (double-ESC pattern)
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
        s_Instance.DoClose();
        return true;
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

    // Called from PlayerRPC client handlers
    static void OnTxResult()
    {
        if (!s_Instance)
            return;
        if (!s_Instance.m_IsOpen)
            return;
        LFPG_BTCAtmController ctrl = LFPG_BTCAtmController.Cast(s_Instance.GetController());
        if (ctrl)
        {
            ctrl.RefreshFromClientData();
        }
    }

    static void OnPriceUnavailable()
    {
        if (!s_Instance)
            return;
        if (!s_Instance.m_IsOpen)
            return;
        LFPG_BTCAtmController ctrl = LFPG_BTCAtmController.Cast(s_Instance.GetController());
        if (ctrl)
        {
            ctrl.RefreshFromClientData();
        }
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
            string errMsg = "[BTCAtmView] No layout root";
            LFPG_Util.Error(errMsg);
            return;
        }
        m_IsOpen = true;
        m_Dragging = false;
        m_HoveredBg = null;
        root.Show(true);

        CenterPanel();

        // Fade-in
        m_FadeAlpha = 0.0;
        m_FadingIn = true;
        if (BTCAtmPanel)
        {
            BTCAtmPanel.SetAlpha(0.0);
        }

        ShowCursor();

        #ifndef SERVER
        if (GetGame())
        {
            PlayerBase openPlayer = PlayerBase.Cast(GetGame().GetPlayer());
            if (openPlayer)
            {
                HumanInputController hicOpen = openPlayer.GetInputController();
                if (hicOpen)
                {
                    hicOpen.SetDisabled(true);
                }
            }
        }
        #endif

        EnsureViewBindings();
        AssignButtonIDs();
        ApplyColors();

        // Populate from LFPG_BTCAtmClientData
        LFPG_BTCAtmController ctrl = LFPG_BTCAtmController.Cast(GetController());
        if (ctrl)
        {
            ctrl.InitWidgetRefs(this);
            // F3: Reset stale tx so old result doesn't flash
            ctrl.ResetTxState();
            ctrl.RefreshFromClientData();
        }

        // Clear EditBoxes on open
        if (EditBtcAmount)
        {
            string clearBtc = "";
            EditBtcAmount.SetText(clearBtc);
        }
        if (EditEurAmount)
        {
            string clearEur = "";
            EditEurAmount.SetText(clearEur);
        }

        string openMsg = "[BTCAtmView] Opened";
        LFPG_Util.Info(openMsg);
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
        {
            root.Show(false);
        }

        #ifndef SERVER
        if (GetGame())
        {
            PlayerBase closePlayer = PlayerBase.Cast(GetGame().GetPlayer());
            if (closePlayer)
            {
                HumanInputController hicClose = closePlayer.GetInputController();
                if (hicClose)
                {
                    hicClose.SetDisabled(false);
                }
            }
        }
        #endif

        HideCursor();
        string closeMsg = "[BTCAtmView] Closed";
        LFPG_Util.Info(closeMsg);
    }
};
