// =========================================================
// LF_PowerGrid — Sorter View (Dabs MVC, v2.5 Sprint)
//
// CREATION: LFPG_SorterView.Init() from MissionGameplay.OnInit
//   pre-creates the view HIDDEN (safe context). Avoids
//   RPC-context CreateWidgets corruption.
// OPEN/CLOSE: .Open() shows + pushes data. .Close() hides.
//
// v2.5 changes:
//   B1-B3: UIScaler — resolution-proportional scaling via
//          Capture(design values) + Apply(scale) on every Open.
//          Dynamic items (tags, preview rows) scaled in SetData.
//
// v2.4 changes:
//   Bug A: ESC via MissionGameplay.OnKeyPress (LocalPress blocked by ChangeGameFocus)
//   Bug C: UnpairedOverlay when no container linked
//   Bug D: SetDisabled(true) blocks player actions; OnKeyPress consumes all keys
//   E8: CenterPanel clamp for small resolutions
//   E9: Hover color cache cleared per ApplyColors
//
// v2.2 changes (Polish Sprint):
//   - Button hover feedback via color cache + OnMouseEnter/Leave
//   - Fade-in animation on open (0.2s alpha lerp)
//   - Enter-to-submit on EditBoxes via OnKeyDown
//   - UI click sounds (SEffectManager)
//   - Visual disabled state when unpaired (IGNOREPOINTER + dim)
//   - Sort feedback in StatusLabel (client-only)
//
// v2.1 changes (Floating Window Sprint):
//   Bug 1-9, drag, double-ESC, pairing banner, DayZ palette
//
// Enforce Script: no ternaries, no ++/--, no foreach.
// =========================================================

class LFPG_SorterView extends ScriptView
{
    protected static ref LFPG_SorterView s_Instance;
    protected bool m_IsOpen;
    protected bool m_FocusLocked;

    // ── Drag state ──
    protected bool m_Dragging;
    protected float m_DragOffX;
    protected float m_DragOffY;

    // ── Hover color cache (v2.2) ──
    // Parallel arrays: widget ref + its base color (no GetColor / no map<Widget> in DayZ)
    protected ref array<Widget> m_CacheWidgets;
    protected ref array<int> m_CacheColors;
    // Currently hovered bg (null if none)
    protected ImageWidget m_HoveredBg;
    // N3: Tracks whether controls are enabled (unpaired = false).
    // Set from Controller via static setter; read by OnMouseEnter.
    protected bool m_ControlsEnabled;

    // ── Fade-in state (v2.2) ──
    protected float m_FadeAlpha;
    protected bool m_FadingIn;

    // Widget refs for ApplyColors ONLY (no dupes with Controller)
    // ModalOverlay REMOVED (Bug #1)
    Widget SorterPanel;
    Widget HeaderFrame;
    // Pairing badge (v3 — replaces PairingBanner)
    ImageWidget PairingBadgeBg;
    TextWidget PairingBadgeText;
    ImageWidget PanelBg;
    ImageWidget AccentLine;
    ImageWidget HeaderBg;
    ImageWidget TabBarBg;
    ImageWidget TabSep;
    ImageWidget TabIndicator;
    ImageWidget ColumnSep;
    ImageWidget RulesPanelBg;
    ImageWidget PreviewPanelBg;
    ImageWidget FooterBg;
    ImageWidget FooterSep;
    ImageWidget FooterMidSep;
    ImageWidget EditPrefixBg;
    ImageWidget EditContainsBg;
    ImageWidget EditSlotMinBg;
    ImageWidget EditSlotMaxBg;
    ImageWidget EditPrefixBorder;
    ImageWidget EditContainsBorder;
    ImageWidget EditSlotMinBorder;
    ImageWidget EditSlotMaxBorder;
    ImageWidget DestIndicatorBg;
    ImageWidget MatchFooterBg;
    ImageWidget BtnCloseXBg;
    TextWidget BtnCloseXText;

    // v2.4 Bug C: Overlay when no container linked
    Widget UnpairedOverlay;
    ImageWidget UnpairedOverlayBg;
    TextWidget UnpairedLabel;
    TextWidget UnpairedHint;

    // v3: Panel frame (P-I)
    ImageWidget PanelBorderLeft;
    ImageWidget PanelBorderRight;
    // v3: Bottom accent (P-VI)
    ImageWidget AccentLineBottom;
    // v3: Drag handle (P-II)
    TextWidget DragHandle;
    // v3: Section cards (A)
    ImageWidget CatSectionBg;
    ImageWidget CatSectionAccent;
    ImageWidget PrefixSectionBg;
    ImageWidget PrefixSectionAccent;
    ImageWidget ContainsSectionBg;
    ImageWidget ContainsSectionAccent;
    ImageWidget SlotSectionBg;
    ImageWidget SlotSectionAccent;
    ImageWidget CatchAllCardBg;
    // v3: Edit hints (E)
    TextWidget EditPrefixHint;
    TextWidget EditContainsHint;
    TextWidget EditSlotMinHint;
    TextWidget EditSlotMaxHint;
    // v3: Footer ESC (P-V)
    TextWidget FooterEscHint;

    static const string PROC_WHITE = "#(argb,8,8,3)color(1,1,1,1,CO)";

    // ── LFPG Palette v2 (ARGB) — DayZ-adjusted (RGB×1.35 bg, ×1.30 btn, alpha×1.40) ──
    static const int COL_BG_DEEP      = 0xFF131C2B;
    static const int COL_BG_PANEL     = 0xF5121C36;
    static const int COL_BG_SECTION   = 0xEB162036;
    static const int COL_BG_ELEVATED  = 0xE61E2B41;
    static const int COL_BG_INPUT     = 0xFF202E4C;
    static const int COL_INPUT_BORDER = 0x4CCBD5E1;
    static const int COL_GREEN        = 0xFF34D399;
    static const int COL_GREEN_DIM    = 0x1734D399;
    static const int COL_GREEN_BORDER = 0x3334D399;
    static const int COL_BLUE         = 0xFF60A5FA;
    static const int COL_AMBER        = 0xFFFBBF24;
    static const int COL_RED          = 0xFFF87171;
    static const int COL_BTN          = 0xFF374B6F;
    static const int COL_TEXT         = 0xFFF1F5F9;
    static const int COL_TEXT_DIM     = 0xFF7A8A9B;
    static const int COL_TEXT_MID     = 0xFFB0BEC5;
    static const int COL_SEPARATOR    = 0x43CBD5E1;
    static const int COL_HEADER       = 0xF50F172B;
    static const int COL_BLUE_BTN     = 0xFF274B7C;
    static const int COL_GREEN_BTN    = 0xFF087C5B;
    static const int COL_RED_BTN      = 0xFFC72323;
    static const int COL_PAIRING_OK   = 0x5034D399;
    static const int COL_PAIRING_ERR  = 0x50F87171;
    // v3: New constants
    static const int COL_BG_SECTION_CARD = 0x08FFFFFF;
    static const int COL_BG_RULES_PANEL  = 0xFF1E2B41;
    static const int COL_RED_BTN_SOFT    = 0x26F87171;
    static const int COL_RED_BTN_BORDER  = 0x40F87171;
    static const int COL_CATCHALL_BG     = 0x10FBBF24;
    static const int COL_PURPLE          = 0xFFA78BFA;

    override string GetLayoutFile()
    {
        return "LFPowerGrid/gui/layouts/LF_Sorter.layout";
    }

    override typename GetControllerType()
    {
        return LFPG_SorterController;
    }

    // =========================================================
    // Update: drag, double-ESC, controller timers
    // =========================================================
    override void Update(float dt)
    {
        if (!m_IsOpen)
            return;

        // ── Fade-in animation (v2.2) ──
        if (m_FadingIn)
        {
            m_FadeAlpha = m_FadeAlpha + dt * 5.0;
            if (m_FadeAlpha >= 1.0)
            {
                m_FadeAlpha = 1.0;
                m_FadingIn = false;
            }
            if (SorterPanel)
            {
                SorterPanel.SetAlpha(m_FadeAlpha);
            }
        }

        // ── Drag logic ──
        if (m_Dragging)
        {
            int mx = 0;
            int my = 0;
            GetMousePos(mx, my);
            float newX = mx - m_DragOffX;
            float newY = my - m_DragOffY;
            // Clamp to screen bounds
            int scrW = 0;
            int scrH = 0;
            GetScreenSize(scrW, scrH);
            float panW = 0.0;
            float panH = 0.0;
            if (SorterPanel)
            {
                SorterPanel.GetSize(panW, panH);
            }
            if (newX < 0.0)
            {
                newX = 0.0;
            }
            // v2.6: Keep header visible — small safety margin at top.
            // Combined with z-sort 50000 this prevents header hiding
            // behind other mod/admin HUD bars.
            if (newY < 5.0)
            {
                newY = 5.0;
            }
            float maxX = scrW - panW;
            float maxY = scrH - panH;
            // v2.6: At very low resolutions, max could be < min.
            // Clamp to min values so the panel stays on screen.
            if (maxX < 0.0)
            {
                maxX = 0.0;
            }
            if (maxY < 5.0)
            {
                maxY = 5.0;
            }
            if (newX > maxX)
            {
                newX = maxX;
            }
            if (newY > maxY)
            {
                newY = maxY;
            }
            if (SorterPanel)
            {
                SorterPanel.SetPos(newX, newY);
            }
        }

        // v2.4 Bug A: ESC handled via MissionGameplay.OnKeyPress → HandleEscKey()
        // UAUIBack block removed — ChangeGameFocus(1) blocks LocalPress("UAUIBack").

        // ── Controller timers ──
        LFPG_SorterController ctrl = LFPG_SorterController.Cast(GetController());
        if (ctrl)
        {
            ctrl.TickTimers(dt);
        }
    }

    void LFPG_SorterView()
    {
        m_IsOpen = false;
        m_FocusLocked = false;
        m_Dragging = false;
        m_DragOffX = 0.0;
        m_DragOffY = 0.0;
        m_CacheWidgets = new array<Widget>();
        m_CacheColors = new array<int>();
        m_HoveredBg = null;
        m_FadeAlpha = 1.0;
        m_FadingIn = false;
    }

    // S1 fix: destructor releases input lock if destroyed while open
    void ~LFPG_SorterView()
    {
        if (GetGame())
        {
            // v2.4 Bug D: Restore player actions on destruction
            // BUG 7 fix: unified cast to PlayerBase (matches DoOpen/DoClose)
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

    // S3: ApplyColors moved to DoOpen() — calling here caused flash
    // because colors were painted before data existed, then overwritten by InitFromRPC.
    // R2: Override kept intentionally to document this design decision.
    override void OnWidgetScriptInit(Widget w)
    {
        super.OnWidgetScriptInit(w);
    }

    // =========================================================
    // v2.6: Manual binding fallback for View-level widget refs.
    // Dabs MVC auto-bind can miss widgets inside ButtonWidget
    // children or deeply nested structures.
    // =========================================================
    protected void EnsureViewBindings()
    {
        Widget root = GetLayoutRoot();
        if (!root)
            return;

        string wn = "";

        wn = "SorterPanel";
        if (!SorterPanel) { SorterPanel = root.FindAnyWidget(wn); }
        wn = "HeaderFrame";
        if (!HeaderFrame) { HeaderFrame = root.FindAnyWidget(wn); }
        wn = "PanelBg";
        if (!PanelBg) { PanelBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "AccentLine";
        if (!AccentLine) { AccentLine = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "HeaderBg";
        if (!HeaderBg) { HeaderBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "TabBarBg";
        if (!TabBarBg) { TabBarBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "TabSep";
        if (!TabSep) { TabSep = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "TabIndicator";
        if (!TabIndicator) { TabIndicator = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "ColumnSep";
        if (!ColumnSep) { ColumnSep = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "RulesPanelBg";
        if (!RulesPanelBg) { RulesPanelBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "PreviewPanelBg";
        if (!PreviewPanelBg) { PreviewPanelBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "FooterBg";
        if (!FooterBg) { FooterBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "FooterSep";
        if (!FooterSep) { FooterSep = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "FooterMidSep";
        if (!FooterMidSep) { FooterMidSep = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EditPrefixBg";
        if (!EditPrefixBg) { EditPrefixBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EditContainsBg";
        if (!EditContainsBg) { EditContainsBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EditSlotMinBg";
        if (!EditSlotMinBg) { EditSlotMinBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EditSlotMaxBg";
        if (!EditSlotMaxBg) { EditSlotMaxBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EditPrefixBorder";
        if (!EditPrefixBorder) { EditPrefixBorder = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EditContainsBorder";
        if (!EditContainsBorder) { EditContainsBorder = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EditSlotMinBorder";
        if (!EditSlotMinBorder) { EditSlotMinBorder = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EditSlotMaxBorder";
        if (!EditSlotMaxBorder) { EditSlotMaxBorder = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "DestIndicatorBg";
        if (!DestIndicatorBg) { DestIndicatorBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "MatchFooterBg";
        if (!MatchFooterBg) { MatchFooterBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }

        // v2.8: BtnCloseX — use child-walk (same fix as Controller).
        // FindAnyWidget returns incorrect refs for widgets inside ButtonWidget.
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

        wn = "PairingBadgeBg";
        if (!PairingBadgeBg) { PairingBadgeBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "PairingBadgeText";
        if (!PairingBadgeText) { PairingBadgeText = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "UnpairedOverlay";
        if (!UnpairedOverlay) { UnpairedOverlay = root.FindAnyWidget(wn); }
        wn = "UnpairedOverlayBg";
        if (!UnpairedOverlayBg) { UnpairedOverlayBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "UnpairedLabel";
        if (!UnpairedLabel) { UnpairedLabel = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "UnpairedHint";
        if (!UnpairedHint) { UnpairedHint = TextWidget.Cast(root.FindAnyWidget(wn)); }

        // v3: New widget bindings
        wn = "PanelBorderLeft";
        if (!PanelBorderLeft) { PanelBorderLeft = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "PanelBorderRight";
        if (!PanelBorderRight) { PanelBorderRight = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "AccentLineBottom";
        if (!AccentLineBottom) { AccentLineBottom = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "DragHandle";
        if (!DragHandle) { DragHandle = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "CatSectionBg";
        if (!CatSectionBg) { CatSectionBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "CatSectionAccent";
        if (!CatSectionAccent) { CatSectionAccent = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "PrefixSectionBg";
        if (!PrefixSectionBg) { PrefixSectionBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "PrefixSectionAccent";
        if (!PrefixSectionAccent) { PrefixSectionAccent = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "ContainsSectionBg";
        if (!ContainsSectionBg) { ContainsSectionBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "ContainsSectionAccent";
        if (!ContainsSectionAccent) { ContainsSectionAccent = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "SlotSectionBg";
        if (!SlotSectionBg) { SlotSectionBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "SlotSectionAccent";
        if (!SlotSectionAccent) { SlotSectionAccent = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "CatchAllCardBg";
        if (!CatchAllCardBg) { CatchAllCardBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EditPrefixHint";
        if (!EditPrefixHint) { EditPrefixHint = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EditContainsHint";
        if (!EditContainsHint) { EditContainsHint = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EditSlotMinHint";
        if (!EditSlotMinHint) { EditSlotMinHint = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EditSlotMaxHint";
        if (!EditSlotMaxHint) { EditSlotMaxHint = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "FooterEscHint";
        if (!FooterEscHint) { FooterEscHint = TextWidget.Cast(root.FindAnyWidget(wn)); }
    }

    protected void ApplyColors()
    {
        // E9 fix: Clear stale hover color cache before re-populating
        if (m_CacheWidgets)
        {
            m_CacheWidgets.Clear();
        }
        if (m_CacheColors)
        {
            m_CacheColors.Clear();
        }

        // Bug #1: ModalOverlay removed
        Tint(PanelBg, COL_BG_PANEL);
        Tint(AccentLine, COL_GREEN);
        Tint(HeaderBg, COL_HEADER);
        Tint(TabBarBg, COL_BG_DEEP);
        Tint(TabSep, COL_SEPARATOR);
        Tint(TabIndicator, COL_GREEN);
        Tint(ColumnSep, COL_SEPARATOR);
        Tint(RulesPanelBg, COL_BG_RULES_PANEL);
        Tint(PreviewPanelBg, COL_BG_RULES_PANEL);
        Tint(FooterBg, COL_BG_PANEL);
        Tint(FooterSep, COL_SEPARATOR);
        Tint(FooterMidSep, COL_SEPARATOR);
        Tint(EditPrefixBg, COL_BG_INPUT);
        Tint(EditContainsBg, COL_BG_INPUT);
        Tint(EditSlotMinBg, COL_BG_INPUT);
        Tint(EditSlotMaxBg, COL_BG_INPUT);
        Tint(EditPrefixBorder, COL_INPUT_BORDER);
        Tint(EditContainsBorder, COL_INPUT_BORDER);
        Tint(EditSlotMinBorder, COL_INPUT_BORDER);
        Tint(EditSlotMaxBorder, COL_INPUT_BORDER);
        Tint(DestIndicatorBg, COL_GREEN_DIM);
        Tint(MatchFooterBg, COL_SEPARATOR);
        // BtnCloseX default color
        Tint(BtnCloseXBg, COL_BTN);
        if (BtnCloseXText)
        {
            BtnCloseXText.SetColor(COL_TEXT_DIM);
        }
        // Pairing badge default (unpaired)
        Tint(PairingBadgeBg, COL_PAIRING_ERR);
        if (PairingBadgeText)
        {
            string defaultBadge = "UNLINKED";
            PairingBadgeText.SetText(defaultBadge);
            PairingBadgeText.SetColor(COL_RED);
        }

        // v2.4 Bug C: Unpaired overlay (v3: updated hex)
        if (UnpairedOverlayBg)
        {
            Tint(UnpairedOverlayBg, 0xCC0E1423);
        }
        if (UnpairedLabel)
        {
            UnpairedLabel.SetColor(COL_RED);
        }
        if (UnpairedHint)
        {
            UnpairedHint.SetColor(COL_TEXT_DIM);
        }

        // FIX L1: EditBox text color — engine default is white puro,
        // inconsistent with COL_TEXT scheme. Resolved via FindAnyWidget
        // (one-shot per Open, acceptable here).
        Widget root = GetLayoutRoot();
        if (root)
        {
            string wnEP = "EditPrefix";
            EditBoxWidget ebPfx = EditBoxWidget.Cast(root.FindAnyWidget(wnEP));
            if (ebPfx)
            {
                ebPfx.SetColor(COL_TEXT);
            }
            string wnEC = "EditContains";
            EditBoxWidget ebCon = EditBoxWidget.Cast(root.FindAnyWidget(wnEC));
            if (ebCon)
            {
                ebCon.SetColor(COL_TEXT);
            }
            string wnSM = "EditSlotMin";
            EditBoxWidget ebMin = EditBoxWidget.Cast(root.FindAnyWidget(wnSM));
            if (ebMin)
            {
                ebMin.SetColor(COL_TEXT);
            }
            string wnSX = "EditSlotMax";
            EditBoxWidget ebMax = EditBoxWidget.Cast(root.FindAnyWidget(wnSX));
            if (ebMax)
            {
                ebMax.SetColor(COL_TEXT);
            }
        }

        // v3: Panel frame
        Tint(PanelBorderLeft, COL_SEPARATOR);
        Tint(PanelBorderRight, COL_SEPARATOR);
        Tint(AccentLineBottom, COL_GREEN);
        // v3: Drag handle
        if (DragHandle)
        {
            DragHandle.SetColor(COL_TEXT_DIM);
        }
        // v3: Section cards
        Tint(CatSectionBg, COL_BG_SECTION_CARD);
        Tint(CatSectionAccent, COL_GREEN);
        Tint(PrefixSectionBg, COL_BG_SECTION_CARD);
        Tint(PrefixSectionAccent, COL_BLUE);
        Tint(ContainsSectionBg, COL_BG_SECTION_CARD);
        Tint(ContainsSectionAccent, COL_AMBER);
        Tint(SlotSectionBg, COL_BG_SECTION_CARD);
        Tint(SlotSectionAccent, COL_PURPLE);
        Tint(CatchAllCardBg, COL_CATCHALL_BG);
        // v3: Edit hints
        if (EditPrefixHint)
        {
            EditPrefixHint.SetColor(COL_TEXT_DIM);
        }
        if (EditContainsHint)
        {
            EditContainsHint.SetColor(COL_TEXT_DIM);
        }
        if (EditSlotMinHint)
        {
            EditSlotMinHint.SetColor(COL_TEXT_DIM);
        }
        if (EditSlotMaxHint)
        {
            EditSlotMaxHint.SetColor(COL_TEXT_DIM);
        }
        // v3: Footer ESC hint
        if (FooterEscHint)
        {
            FooterEscHint.SetColor(COL_TEXT_DIM);
        }
    }

    protected void Tint(ImageWidget img, int color)
    {
        if (!img)
            return;
        img.LoadImageFile(0, PROC_WHITE);
        img.SetColor(color);
        // Cache for hover system (v2.2)
        CacheColorLocal(img, color);
    }

    // Store/update color for a widget in parallel arrays
    protected void CacheColorLocal(Widget w, int color)
    {
        if (!w)
            return;
        if (!m_CacheWidgets)
            return;
        int i = 0;
        int count = m_CacheWidgets.Count();
        for (i = 0; i < count; i = i + 1)
        {
            if (m_CacheWidgets[i] == w)
            {
                m_CacheColors[i] = color;
                return;
            }
        }
        m_CacheWidgets.Insert(w);
        m_CacheColors.Insert(color);
    }

    // Find cached color for a widget, returns -1 if not found
    protected int FindCachedColor(Widget w)
    {
        if (!w)
            return -1;
        if (!m_CacheWidgets)
            return -1;
        int i = 0;
        int count = m_CacheWidgets.Count();
        for (i = 0; i < count; i = i + 1)
        {
            if (m_CacheWidgets[i] == w)
            {
                return m_CacheColors[i];
            }
        }
        return -1;
    }

    // =========================================================
    // Static color cache accessor (called from Controller.TintBg)
    // =========================================================
    static void CacheColor(Widget w, int color)
    {
        if (!s_Instance)
            return;
        if (!w)
            return;
        s_Instance.CacheColorLocal(w, color);
    }

    // N3: Static setter — called from Controller.SetControlsEnabled
    // so OnMouseEnter can skip hover on disabled buttons.
    static void SetControlsFlag(bool enabled)
    {
        if (!s_Instance)
            return;
        s_Instance.m_ControlsEnabled = enabled;
    }

    // =========================================================
    // Drag: MouseDown on header starts drag (Bug #4)
    // =========================================================
    override bool OnMouseButtonDown(Widget w, int x, int y, int button)
    {
        if (!m_IsOpen)
            return false;

        // Header drag (LMB only)
        if (button == 0)
        {
            if (IsHeaderWidget(w))
            {
                m_Dragging = true;
                // FIX 4: Restore hover color before drag moves panel
                if (m_HoveredBg)
                {
                    int restoreCol = FindCachedColor(m_HoveredBg);
                    if (restoreCol >= 0)
                    {
                        m_HoveredBg.SetColor(restoreCol);
                    }
                    m_HoveredBg = null;
                }
                float px = 0.0;
                float py = 0.0;
                if (SorterPanel)
                {
                    SorterPanel.GetPos(px, py);
                }
                m_DragOffX = x - px;
                m_DragOffY = y - py;
            }
        }

        // v2.6 fix: If the click landed on an interactive widget
        // (ButtonWidget or EditBoxWidget), return false so the widget's
        // internal handler can process it → OnClick → Relay_Command.
        // Returning true was consuming the event BEFORE ButtonWidget
        // could generate OnClick, breaking ALL button clicks.
        // ChangeGameFocus(1) + SetDisabled(true) already prevent
        // game-side click-through (movement, attacks, interactions).
        if (IsInteractiveWidget(w))
        {
            return false;
        }

        // DIAG: Log consumed non-interactive clicks — if category buttons
        // land here, OnClick will NEVER fire for them.
        string diagDown = "[SorterView] MouseDown CONSUMED w=";
        diagDown = diagDown + w.GetName();
        Widget diagDownP = w.GetParent();
        if (diagDownP)
        {
            diagDown = diagDown + " parent=";
            diagDown = diagDown + diagDownP.GetName();
        }
        LFPG_Util.Info(diagDown);

        // Consume non-interactive clicks (panel bg, headers, labels)
        // so mouse events don't leak to game layer.
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

        // v2.6 fix: Let interactive widgets receive mouse-up too
        // (ButtonWidget needs both down+up to fire OnClick).
        if (IsInteractiveWidget(w))
        {
            return false;
        }
        return true;
    }

    // =========================================================
    // v2.7: Manual OnClick dispatch — robust fallback.
    // Dabs MVC Relay_Command is processed by ScriptView.OnClick.
    // This override intercepts FIRST, dispatches known buttons
    // directly to the controller, and returns true to prevent
    // the base class from ALSO dispatching (which would cause
    // double-fire → toggles cancel out).
    //
    // For unrecognized buttons: delegates to super.OnClick so
    // Relay_Command still works (future-proofing).
    // =========================================================
    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (!m_IsOpen)
        {
            bool baseNotOpen = super.OnClick(w, x, y, button);
            return baseNotOpen;
        }
        if (!w)
        {
            bool baseNoW = super.OnClick(w, x, y, button);
            return baseNoW;
        }
        if (button != 0)
        {
            bool baseNotLMB = super.OnClick(w, x, y, button);
            return baseNotLMB;
        }

        // Find the enclosing ButtonWidget (click may land on a child)
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
            // DIAG: Log what widget received the click when no ButtonWidget found
            string diagNoBtn = "[SorterView] OnClick NO ButtonWidget w=";
            diagNoBtn = diagNoBtn + w.GetName();
            Widget diagParent = w.GetParent();
            if (diagParent)
            {
                diagNoBtn = diagNoBtn + " parent=";
                diagNoBtn = diagNoBtn + diagParent.GetName();
            }
            LFPG_Util.Info(diagNoBtn);
            bool baseNoBtn = super.OnClick(w, x, y, button);
            return baseNoBtn;
        }

        string bName = btn.GetName();

        // DIAG: Log every button click for debugging
        string diagClick = "[SorterView] OnClick btn=";
        diagClick = diagClick + bName;
        LFPG_Util.Info(diagClick);

        LFPG_SorterController ctrl = LFPG_SorterController.Cast(GetController());
        if (!ctrl)
        {
            bool baseNoCtrl = super.OnClick(w, x, y, button);
            return baseNoCtrl;
        }

        // Output tabs
        string nTabOut0 = "TabOut0";
        string nTabOut1 = "TabOut1";
        string nTabOut2 = "TabOut2";
        string nTabOut3 = "TabOut3";
        string nTabOut4 = "TabOut4";
        string nTabOut5 = "TabOut5";
        if (bName == nTabOut0) { ctrl.TabOut0(); return true; }
        if (bName == nTabOut1) { ctrl.TabOut1(); return true; }
        if (bName == nTabOut2) { ctrl.TabOut2(); return true; }
        if (bName == nTabOut3) { ctrl.TabOut3(); return true; }
        if (bName == nTabOut4) { ctrl.TabOut4(); return true; }
        if (bName == nTabOut5) { ctrl.TabOut5(); return true; }

        // View tabs
        string nTabRules = "TabRules";
        string nTabPreview = "TabPreview";
        if (bName == nTabRules) { ctrl.TabRules(); return true; }
        if (bName == nTabPreview) { ctrl.TabPreview(); return true; }

        // Category buttons
        string nCat0 = "CatBtn0";
        string nCat1 = "CatBtn1";
        string nCat2 = "CatBtn2";
        string nCat3 = "CatBtn3";
        string nCat4 = "CatBtn4";
        string nCat5 = "CatBtn5";
        string nCat6 = "CatBtn6";
        string nCat7 = "CatBtn7";
        if (bName == nCat0) { ctrl.CatBtn0(); return true; }
        if (bName == nCat1) { ctrl.CatBtn1(); return true; }
        if (bName == nCat2) { ctrl.CatBtn2(); return true; }
        if (bName == nCat3) { ctrl.CatBtn3(); return true; }
        if (bName == nCat4) { ctrl.CatBtn4(); return true; }
        if (bName == nCat5) { ctrl.CatBtn5(); return true; }
        if (bName == nCat6) { ctrl.CatBtn6(); return true; }
        if (bName == nCat7) { ctrl.CatBtn7(); return true; }

        // Slot presets
        string nSlot0 = "SlotPre0";
        string nSlot1 = "SlotPre1";
        string nSlot2 = "SlotPre2";
        string nSlot3 = "SlotPre3";
        if (bName == nSlot0) { ctrl.SlotPre0(); return true; }
        if (bName == nSlot1) { ctrl.SlotPre1(); return true; }
        if (bName == nSlot2) { ctrl.SlotPre2(); return true; }
        if (bName == nSlot3) { ctrl.SlotPre3(); return true; }

        // Add buttons
        string nPrefixAdd = "BtnPrefixAdd";
        string nContainsAdd = "BtnContainsAdd";
        string nSlotAdd = "BtnSlotAdd";
        if (bName == nPrefixAdd) { ctrl.BtnPrefixAdd(); return true; }
        if (bName == nContainsAdd) { ctrl.BtnContainsAdd(); return true; }
        if (bName == nSlotAdd) { ctrl.BtnSlotAdd(); return true; }

        // Action buttons
        string nCatchAll = "BtnCatchAll";
        string nClearOut = "BtnClearOut";
        string nResetAll = "BtnResetAll";
        string nSave = "BtnSave";
        string nSort = "BtnSort";
        string nClose = "BtnClose";
        string nCloseX = "BtnCloseX";
        string nSortHeader = "BtnSortHeader";
        if (bName == nCatchAll) { ctrl.BtnCatchAll(); return true; }
        if (bName == nClearOut) { ctrl.BtnClearOut(); return true; }
        if (bName == nResetAll) { ctrl.BtnResetAll(); return true; }
        if (bName == nSave) { ctrl.BtnSave(); return true; }
        if (bName == nSort) { ctrl.BtnSort(); return true; }
        if (bName == nClose) { ctrl.BtnClose(); return true; }
        if (bName == nCloseX) { ctrl.BtnCloseX(); return true; }
        if (bName == nSortHeader) { ctrl.BtnSortHeader(); return true; }

        // Unrecognized button — delegate to ScriptView base class
        // so Relay_Command still works for any future buttons
        bool baseFallback = super.OnClick(w, x, y, button);
        return baseFallback;
    }

    // Walk the parent chain to see if widget is in the HeaderFrame
    // Stops if we hit a ButtonWidget (don't drag on buttons inside header)
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
            // If we hit a button first, it's a button click, not drag
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

    // v2.6: Walk the parent chain to check if w is (or is inside)
    // a ButtonWidget, EditBoxWidget, or ScrollWidget.
    // Used by OnMouseButtonDown/Up to decide whether to consume
    // the event (return true) or let the widget handle it
    // (return false → Relay_Command / scroll / text input fires).
    protected bool IsInteractiveWidget(Widget w)
    {
        if (!w)
            return false;

        Widget check = w;
        ButtonWidget btnCast = null;
        EditBoxWidget editCast = null;
        ScrollWidget scrollCast = null;
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
            scrollCast = ScrollWidget.Cast(check);
            if (scrollCast)
            {
                return true;
            }
            check = check.GetParent();
        }
        return false;
    }

    // =========================================================
    // Center panel on screen (called on open)
    // Panel is 720×590 fixed pixels (designed for 1080p).
    // At 4K it appears smaller (18.75%) but fully functional.
    // Full proportional scaling requires layout redesign (future sprint).
    // =========================================================
    protected void CenterPanel()
    {
        if (!SorterPanel)
            return;
        int scrW = 0;
        int scrH = 0;
        GetScreenSize(scrW, scrH);
        float panW = 0.0;
        float panH = 0.0;
        SorterPanel.GetSize(panW, panH);
        float cx = (scrW - panW) * 0.5;
        float cy = (scrH - panH) * 0.5;
        // E8: Clamp for resolutions smaller than panel
        if (cx < 0.0)
        {
            cx = 0.0;
        }
        if (cy < 0.0)
        {
            cy = 0.0;
        }
        SorterPanel.SetPos(cx, cy);
    }

    // =========================================================
    // Pairing state visual update (Bug #5)
    // =========================================================
    void UpdatePairingState(string containerDisplayName)
    {
        bool paired = false;
        if (containerDisplayName != "")
        {
            paired = true;
        }

        if (paired)
        {
            Tint(PairingBadgeBg, COL_PAIRING_OK);
            if (PairingBadgeText)
            {
                // v3.1: Show container name instead of generic "PAIRED"
                string badgePrefix = ">> ";
                string pairedLabel = badgePrefix;
                pairedLabel = pairedLabel + containerDisplayName;
                PairingBadgeText.SetText(pairedLabel);
                PairingBadgeText.SetColor(COL_GREEN);
            }
        }
        else
        {
            Tint(PairingBadgeBg, COL_PAIRING_ERR);
            if (PairingBadgeText)
            {
                string unlinkedLabel = "UNLINKED";
                PairingBadgeText.SetText(unlinkedLabel);
                PairingBadgeText.SetColor(COL_RED);
            }
        }

        // v2.4 Bug C: Show/hide unpaired overlay
        if (UnpairedOverlay)
        {
            if (paired)
            {
                UnpairedOverlay.Show(false);
            }
            else
            {
                UnpairedOverlay.Show(true);
            }
        }
    }

    // =========================================================
    // v3: Edit hint show/hide (E) — event-driven via OnChange
    // =========================================================
    protected void RefreshEditHints()
    {
        LFPG_SorterController ctrl = LFPG_SorterController.Cast(GetController());
        if (!ctrl)
            return;
        bool pfxEmpty = (ctrl.EditPrefix == "");
        bool conEmpty = (ctrl.EditContains == "");
        bool minEmpty = (ctrl.EditSlotMin == "");
        bool maxEmpty = (ctrl.EditSlotMax == "");
        if (EditPrefixHint)
        {
            EditPrefixHint.Show(pfxEmpty);
        }
        if (EditContainsHint)
        {
            EditContainsHint.Show(conEmpty);
        }
        if (EditSlotMinHint)
        {
            EditSlotMinHint.Show(minEmpty);
        }
        if (EditSlotMaxHint)
        {
            EditSlotMaxHint.Show(maxEmpty);
        }
    }

    static void RefreshHints()
    {
        if (!s_Instance)
            return;
        s_Instance.RefreshEditHints();
    }

    // v3: OnChange fires on every EditBox keystroke
    override bool OnChange(Widget w, int x, int y, bool finished)
    {
        if (!m_IsOpen)
            return false;
        RefreshEditHints();
        return false;
    }

    // =========================================================
    // Singleton lifecycle
    // =========================================================
    static void Init()
    {
        #ifndef SERVER
        if (s_Instance)
            return;
        s_Instance = new LFPG_SorterView();
        Widget root = s_Instance.GetLayoutRoot();
        if (root)
        {
            root.Show(false);
            root.SetSort(50000);
        }
        string initMsg = "[SorterView] Pre-created (hidden)";
        LFPG_Util.Info(initMsg);

        // v2.5 B1: Capture design-time widget values for resolution scaling.
        // Must happen AFTER ScriptView creates widgets (constructor) and
        // BEFORE any Apply call. SorterPanel and all children are captured.
        if (s_Instance.SorterPanel)
        {
            LFPG_UIScaler.Capture(s_Instance.SorterPanel);
        }
        #endif
    }

    // Called from RPC — widgets already exist, just show + data
    static void Open(string configJSON, string containerName, string d0, string d1, string d2, string d3, string d4, string d5, int netLow, int netHigh)
    {
        #ifndef SERVER
        if (!s_Instance)
        {
            string warnMsg = "[SorterView] Open before Init — cannot open";
            LFPG_Util.Warn(warnMsg);
            return;
        }
        s_Instance.DoOpen(configJSON, containerName, d0, d1, d2, d3, d4, d5, netLow, netHigh);
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

    // v2.4 Bug A: Called from MissionGameplay.OnKeyPress(key==1).
    // Double-ESC: first clears EditBox focus, second closes panel.
    // Returns true if event consumed (panel was open).
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
        s_Instance.DoClose();
        return true;
    }

    // S2 fix: Cleanup deletes instance properly (prevents leak).
    // Input release is handled by the destructor (S1) which has
    // GetGame() null guard for safe shutdown.
    static void Cleanup()
    {
        // v2.5 B3: Release scaler arrays before destroying widgets
        LFPG_UIScaler.Reset();

        if (s_Instance)
        {
            s_Instance.m_IsOpen = false;
            s_Instance.m_Dragging = false;
            s_Instance.m_FadingIn = false;
        }
        // FIX 3: null decrements refcount → GC runs destructor safely.
        // Explicit delete risked segfault if callback still held ref.
        s_Instance = null;
    }

    static void OnSaveAck(bool success)
    {
        if (!s_Instance)
            return;
        if (!s_Instance.m_IsOpen)
            return;
        LFPG_SorterController ctrl = LFPG_SorterController.Cast(s_Instance.GetController());
        if (ctrl)
        {
            ctrl.HandleSaveAck(success);
        }
    }

    // v2.6: Server preview response → delegate to Controller
    static void OnPreviewData(int outputIdx, int totalMatched, array<string> names, array<string> cats, array<int> slots)
    {
        if (!s_Instance)
            return;
        if (!s_Instance.m_IsOpen)
            return;
        LFPG_SorterController ctrl = LFPG_SorterController.Cast(s_Instance.GetController());
        if (ctrl)
        {
            ctrl.PopulatePreview(outputIdx, totalMatched, names, cats, slots);
        }
    }

    protected void DoOpen(string configJSON, string containerName, string d0, string d1, string d2, string d3, string d4, string d5, int netLow, int netHigh)
    {
        if (m_IsOpen)
            return;
        Widget root = GetLayoutRoot();
        if (!root)
        {
            string errMsg = "[SorterView] No layout root";
            LFPG_Util.Error(errMsg);
            return;
        }
        m_IsOpen = true;
        m_Dragging = false;
        m_HoveredBg = null;
        m_ControlsEnabled = true;
        root.Show(true);

        // v2.5 B3: Apply resolution scaling BEFORE centering.
        // Apply reads from captured design values (never accumulates error).
        // CenterPanel then reads the scaled SorterPanel size to center correctly.
        float uiScale = LFPG_UIScaler.ComputeScale();
        LFPG_UIScaler.Apply(uiScale);
        CenterPanel();

        // Fade-in (v2.2)
        m_FadeAlpha = 0.0;
        m_FadingIn = true;
        if (SorterPanel)
        {
            SorterPanel.SetAlpha(0.0);
        }

        ShowCursor();

        // v2.4 Bug D: Block all player actions while UI open
        // (CCTV pattern: SetDisabled blocks movement/interaction/inventory
        //  but widget event system still works for EditBox typing + buttons)
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

        // v2.6: Manual binding fallback — fixes white tabs (Dabs auto-bind miss).
        // Must run BEFORE ApplyColors/InitFromRPC so all widget refs are available.
        EnsureViewBindings();
        LFPG_SorterController ctrl = LFPG_SorterController.Cast(GetController());
        if (ctrl)
        {
            ctrl.EnsureBindings(root);
        }

        // S3: ApplyColors here (not in OnWidgetScriptInit) — avoids flash
        ApplyColors();

        // Update pairing banner (Bug #5)
        UpdatePairingState(containerName);

        if (ctrl)
        {
            ctrl.InitFromRPC(configJSON, containerName, d0, d1, d2, d3, d4, d5, netLow, netHigh);
        }

        // v3: Initial hint visibility
        RefreshEditHints();

        string openMsg = "[SorterView] Opened for: ";
        openMsg = openMsg + containerName;
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

        // FIX 2: Release tag/preview views now (breaks circular refs).
        // Without this, views survive until next Open or full Cleanup.
        LFPG_SorterController ctrl = LFPG_SorterController.Cast(GetController());
        if (ctrl)
        {
            ctrl.ClearCollections();
        }

        Widget root = GetLayoutRoot();
        if (root)
        {
            root.Show(false);
        }
        // v2.4 Bug D: Restore player actions before releasing cursor
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
        string closeMsg = "[SorterView] Closed";
        LFPG_Util.Info(closeMsg);
    }

    // =========================================================
    // Input lock: ChangeGameFocus(1) suppresses continuous input (WASD/look).
    // OnMouseButtonDown returning true suppresses click-through to game.
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
    // Hover feedback (v2.2) — lighten button bg on mouse enter
    // =========================================================
    override bool OnMouseEnter(Widget w, int x, int y)
    {
        if (!m_IsOpen)
            return false;
        // N3: No hover flash on disabled/dimmed buttons
        if (!m_ControlsEnabled)
            return false;

        ImageWidget bg = FindButtonBg(w);
        int baseColor = 0;
        int hoverColor = 0;
        if (bg)
        {
            // Restore previous hover first (guards against Enter-before-Leave race)
            if (m_HoveredBg && m_HoveredBg != bg)
            {
                baseColor = FindCachedColor(m_HoveredBg);
                if (baseColor >= 0)
                {
                    m_HoveredBg.SetColor(baseColor);
                }
                m_HoveredBg = null;
                baseColor = 0;
            }

            baseColor = FindCachedColor(bg);
            if (baseColor >= 0)
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
        // FIX 5: Guard symmetric with OnMouseEnter
        if (!m_IsOpen)
            return false;

        int baseColor = 0;
        if (m_HoveredBg)
        {
            baseColor = FindCachedColor(m_HoveredBg);
            if (baseColor >= 0)
            {
                m_HoveredBg.SetColor(baseColor);
            }
            m_HoveredBg = null;
        }
        return false;
    }

    // Walk up from w to find enclosing ButtonWidget, then return first ImageWidget child
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

        // First child is the Bg ImageWidget
        Widget child = btn.GetChildren();
        if (!child)
            return null;

        ImageWidget bg = ImageWidget.Cast(child);
        return bg;
    }

    // Lighten an ARGB color by adding 'amount' to RGB channels (clamped to 255)
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
    // Enter-to-submit on EditBoxes (v2.2)
    // =========================================================
    override bool OnKeyDown(Widget w, int x, int y, int key)
    {
        if (!m_IsOpen)
            return false;

        Widget focused = null;
        LFPG_SorterController ctrl = null;
        string wName = "";

        // KC_RETURN = 28
        if (key == KeyCode.KC_RETURN)
        {
            focused = GetFocus();
            if (!focused)
                return false;

            ctrl = LFPG_SorterController.Cast(GetController());
            if (!ctrl)
                return false;

            // Match focused widget to the known EditBoxes
            wName = focused.GetName();
            if (wName == "EditPrefix")
            {
                ctrl.BtnPrefixAdd();
                return true;
            }
            if (wName == "EditContains")
            {
                ctrl.BtnContainsAdd();
                return true;
            }
            if (wName == "EditSlotMin" || wName == "EditSlotMax")
            {
                ctrl.BtnSlotAdd();
                return true;
            }
        }

        return false;
    }

    // =========================================================
    // UI Sound helper (v2.2)
    // =========================================================
    static void PlayUIClick()
    {
        #ifndef SERVER
        if (!GetGame())
            return;

        // v3.1: "pickUpItem_SoundSet" is likely invalid in vanilla DayZ
        // (same convention as attachmentAdded_SoundSet which is confirmed invalid).
        // TODO: Define custom CfgSoundSets or find validated vanilla set.
        #endif
    }

    static void PlayUIAction()
    {
        #ifndef SERVER
        if (!GetGame())
            return;

        // v3.1: "attachmentAdded_SoundSet" is invalid in vanilla DayZ.
        // TODO: Define custom CfgSoundSets in config.cpp with a real .ogg
        // for reliable UI feedback. For now, no-op to prevent log spam.
        #endif
    }

    LFPG_SorterController GetSorterController()
    {
        return LFPG_SorterController.Cast(GetController());
    }
};
