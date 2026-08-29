#ifndef SERVER
// Client-only compilation boundary
// =========================================================
// LF_PowerGrid — Sorter View V4 TEST (Dabs MVC)
//
// V4 redesign progress (2026-04-26):
//   Sprint 0  ✓ Scaffolding — V4 entity + view + controller cloned
//                from V3, separate RPC SubIds (60-70), independent
//                singleton. V3 production untouched.
//   Sprint 1  ✓ Vertical rail (column 1: PICK OUTPUT)
//                — 6 OutputRow buttons + catch-all anchored bottom.
//                V3 horizontal TabBar hidden at runtime (Show false).
//   Sprint 2  ✓ Builder tabs (column 2: BUILD A RULE)
//                — CATEGORY/PREFIX/CONTAINS/SLOT tabs swap which of
//                the 4 filter sections is visible.
//   Sprint 3  ✓ Active Rules header (column 3)
//                — Step ③ circle + title + sublabel "ON OUT N - ...".
//                Legacy header label removed with the old layout.
//   Sprint 4  ✓ Polish — BtnClearOut ghost variant; sublabel uppercase.
//
// Sprint 5+ TODO (deferred until in-game testing feedback):
//   - Panel resize 720x590 -> 820x600 (design spec)
//   - Step circles 1/2/3 as proper circular sprites (currently boxes)
//   - Reposition filter sections so only the active one occupies the
//     visible area (right now hidden sections leave empty space)
//   - Header redesign: SORT NOW button + POWERED dot + linked badge
//   - Tag chip restyle: per-rule-type colored prefix (CAT blue, PFX
//     amber, CON purple, SLT green) + color border-left
//   - Legacy header label + horizontal tab bar widgets are gone
//     from the new S2 layout entirely
//   - Hoist new array<string> allocations in RefreshBuilderTab_TEST
//     to member fields (avoids 4 allocs per refresh)
//
// V3 -> V4 mapping note: this file is a fork of LFPG_SorterView.c.
// When fixing bugs that apply to BOTH versions, propagate to V3 first
// then re-clone via sprint0_clone.py to keep them in sync.
//
// CREATION: The view is initialized lazily by the first Open() call.
//   Widget creation therefore occurs only when the test sorter UI is requested.
// OPEN/CLOSE: .Open() creates or shows the view and pushes data. .Close() hides it.
//
// v3.3 changes (Sprint 3 — Performance):
//   M4: Hover O(1) — parallel arrays replaced by per-widget
//       SetUserData/GetUserData (LFPG_ColorData_TEST). No more O(n) scan.
//   M5: ClampPanelPos helper — DPI-safe clamp shared between
//       CenterPanel and drag, eliminates 45 lines of duplication.
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

// M4: Per-widget color data for O(1) hover lookup via SetUserData.
// Replaces parallel arrays (m_CacheWidgets/m_CacheColors) with O(n) scan.
class LFPG_ColorData_TEST extends Managed
{
    int m_BaseColor;

    void LFPG_ColorData_TEST(int color)
    {
        m_BaseColor = color;
    }
};

class LFPG_SorterView_TEST extends ScriptView
{
    protected static ref LFPG_SorterView_TEST s_Instance;
    protected static int s_PerfDiagConstructionCount;
    // A7: ESC timestamp guard (prevents engine pause menu on release)
    protected static float s_EscCloseTime = 0.0;
    protected bool m_IsOpen;
    protected bool m_FocusLocked;

    // ── Drag state ──
    protected bool m_Dragging;
    protected float m_DragOffX;
    protected float m_DragOffY;

    // ── Hover color cache (v2.2, M4: per-widget via SetUserData) ──
    // Strong refs to prevent GC — SetUserData does NOT hold strong ref!
    protected ref array<ref LFPG_ColorData_TEST> m_ColorDataRefs;
    // F1-B (heap corruption fix 2026-04-26): tracks widgets that received
    // SetUserData(LFPG_ColorData_TEST). Before clearing m_ColorDataRefs we MUST
    // call SetUserData(null) on each — the engine stores SetUserData as a
    // raw pointer (not a Managed soft link), so freeing the data while the
    // widget still points to it leaves a dangling read for the next
    // GetUserData/Cast → heap corruption (0xc0000374). array<Widget> is a
    // weak ref by design (no `ref`); destroyed widgets nullify on their own.
    protected ref array<Widget> m_TintedWidgets;
    // Currently hovered bg (null if none)
    protected ImageWidget m_HoveredBg;
    // N3: Tracks whether controls are enabled (unpaired = false).
    // Set from Controller via static setter; read by OnMouseEnter.
    protected bool m_ControlsEnabled;
    // M2: Track first AssignButtonIDs pass (UserIDs don't change)
    protected bool m_ButtonIDsAssigned;

    // ── Fade-in state (v2.2) ──
    protected float m_FadeAlpha;
    protected bool m_FadingIn;
    // MCP TEST command hook. Separate layout so ui_reload_layout
    // preview cannot hijack the name. Polled from Update because
    // DispatchUiSetText calls SetText and never runs OnClick.
    protected EditBoxWidget m_McpCmd;
    protected bool m_McpCmdOwned;
    protected bool m_McpCmdCreateFailed;

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
    ImageWidget ColumnSep;
    ImageWidget RulesPanelBg;
    ImageWidget PreviewPanelBg;
    ImageWidget EditPrefixBg;
    ImageWidget EditContainsBg;
    ImageWidget EditSlotMinBg;
    ImageWidget EditSlotMaxBg;
    ImageWidget EditPrefixBorder;
    ImageWidget EditContainsBorder;
    ImageWidget EditSlotMinBorder;
    ImageWidget EditSlotMaxBorder;
    ImageWidget MatchFooterBg;
    ImageWidget BtnCloseXBg;
    TextWidget BtnCloseXText;

    // === Sprint 1 (2026-04-26): Vertical rail bindings ===
    ImageWidget OutputRailBg;
    ImageWidget OutputRailBorder;
    ImageWidget Step1Circle;
    TextWidget  Step1Number;
    TextWidget  Step1Title;
    // 6 output rows (button + bg + indicator + label + count + container)
    Widget      OutputRow0;
    ImageWidget OutputRow0Bg;
    ImageWidget OutputRow0Indicator;
    TextWidget  OutputRow0Label;
    TextWidget  OutputRow0Count;
    TextWidget  OutputRow0Container;
    Widget      OutputRow1;
    ImageWidget OutputRow1Bg;
    ImageWidget OutputRow1Indicator;
    TextWidget  OutputRow1Label;
    TextWidget  OutputRow1Count;
    TextWidget  OutputRow1Container;
    Widget      OutputRow2;
    ImageWidget OutputRow2Bg;
    ImageWidget OutputRow2Indicator;
    TextWidget  OutputRow2Label;
    TextWidget  OutputRow2Count;
    TextWidget  OutputRow2Container;
    Widget      OutputRow3;
    ImageWidget OutputRow3Bg;
    ImageWidget OutputRow3Indicator;
    TextWidget  OutputRow3Label;
    TextWidget  OutputRow3Count;
    TextWidget  OutputRow3Container;
    Widget      OutputRow4;
    ImageWidget OutputRow4Bg;
    ImageWidget OutputRow4Indicator;
    TextWidget  OutputRow4Label;
    TextWidget  OutputRow4Count;
    TextWidget  OutputRow4Container;
    Widget      OutputRow5;
    ImageWidget OutputRow5Bg;
    ImageWidget OutputRow5Indicator;
    TextWidget  OutputRow5Label;
    TextWidget  OutputRow5Count;
    TextWidget  OutputRow5Container;
    Widget      CatchAllRow;
    ImageWidget CatchAllRowBg;
    ImageWidget CatchAllRowIndicator;
    TextWidget  CatchAllRowLabel;
    TextWidget  CatchAllRowSublabel;

    // === Sprint 2 (2026-04-26): Builder tab bar bindings ===
    ImageWidget BuilderTabBg;
    ImageWidget BuilderTabBorder;
    Widget      BuilderTabCategory;
    ImageWidget BuilderTabCategoryBg;
    ImageWidget BuilderTabCategoryUnderline;
    TextWidget  BuilderTabCategoryText;
    Widget      BuilderTabPrefix;
    ImageWidget BuilderTabPrefixBg;
    ImageWidget BuilderTabPrefixUnderline;
    TextWidget  BuilderTabPrefixText;
    Widget      BuilderTabContains;
    ImageWidget BuilderTabContainsBg;
    ImageWidget BuilderTabContainsUnderline;
    TextWidget  BuilderTabContainsText;
    Widget      BuilderTabSlot;
    ImageWidget BuilderTabSlotBg;
    ImageWidget BuilderTabSlotUnderline;
    TextWidget  BuilderTabSlotText;

    // === Sprint 3 (2026-04-26): Active Rules header bindings ===
    ImageWidget Step3Circle;
    TextWidget  Step3Number;
    TextWidget  Step3Title;
    TextWidget  RulesSublabel;

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
    // v3: Edit hints (E)
    TextWidget EditPrefixHint;
    TextWidget EditContainsHint;
    TextWidget EditSlotMinHint;
    TextWidget EditSlotMaxHint;

    static const bool S1_PROBE = true;

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
    // v3.2: Was 0x08FFFFFF (invisible), then 0x40FFFFFF (still too faint).
    // Now matches COL_BG_ELEVATED — opaque dark blue, clearly visible.
    static const int COL_BG_SECTION_CARD = 0xE61E2B41;
    static const int COL_BG_RULES_PANEL  = 0xFF1E2B41;
    static const int COL_RED_BTN_SOFT    = 0x26F87171;
    static const int COL_RED_BTN_BORDER  = 0x40F87171;
    // U1 (2026-04-26): alpha 0x10 (16/255) is below DayZ visibility
    // threshold (~0x30). Bumped to 0x26 to match COL_RED_BTN_SOFT/
    // COL_PAIRING_OK alpha range — catch-all card now actually visible.
    static const int COL_CATCHALL_BG     = 0x26FBBF24;
    static const int COL_PURPLE          = 0xFFA78BFA;

    // S2 premixes (spec): opaque static-chrome colors, painted by the layout
    static const int COL_S2_BG_SECTION   = 0xFF162036;
    static const int COL_S2_HEADER       = 0xFF0F172B;
    static const int COL_S2_SEPARATOR    = 0xFF424C62;
    static const int COL_S2_CATCHALL_BG  = 0xFF242A35;
    static const int COL_S2_RULEROW_BG   = 0xFF1D273C;
    static const int COL_S2_GREENBTN_HDR = 0xFF14313A;
    static const int COL_S2_GREENBTN_SEC = 0xFF1A3944;
    static const int COL_S2_GREENBTN_PAN = 0xFF173644;
    static const int COL_S2_REDBTN_BG    = 0xFF2D283C;

    // ── M2: Button UserID ranges (int dispatch replaces string comparison) ──
    // 111: preview toggle, 200+i: categories,
    // 300+i: slots, 400-402: adds, 500+: actions
    static const int UID_TAB_PREVIEW   = 111;
    static const int UID_CAT_BASE      = 200;
    static const int UID_SLOT_BASE     = 300;
    static const int UID_PREFIX_ADD    = 400;
    static const int UID_CONTAINS_ADD  = 401;
    static const int UID_SLOT_ADD      = 402;
    static const int UID_CATCH_ALL     = 500;
    static const int UID_CLEAR_OUT     = 501;
    static const int UID_RESET_ALL     = 502;
    static const int UID_SAVE          = 503;
    static const int UID_CLOSE_X       = 506;
    static const int UID_SORT_HEADER   = 507;
    // v4.3: Tag BtnRemove — UID = 600 + outIdx*10 + (ruleIdx+1)
    static const int UID_TAG_REMOVE_BASE = 600;
    // Sprint 1 (2026-04-26): rail row UIDs
    static const int UID_RAIL_ROW_BASE = 700;   // 700..705 = OutputRow0..5
    static const int UID_RAIL_CATCHALL = 710;
    // Sprint 2 (2026-04-26): builder tab UIDs
    static const int UID_BUILDER_TAB_BASE = 720;  // 720..723 = CAT/PFX/CON/SLT

    override string GetLayoutFile()
    {
        return "LFPowerGrid/gui/layouts/test/LFPG_Sorter_TEST.layout";
    }

    override typename GetControllerType()
    {
        return LFPG_SorterController_TEST;
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
            // M5: Clamp via shared helper (minY=5 keeps header visible)
            float clampedX = 0.0;
            float clampedY = 0.0;
            float dragMinY = 5.0;
            ClampPanelPos(newX, newY, dragMinY, clampedX, clampedY);
            if (SorterPanel)
            {
                SorterPanel.SetPos(clampedX, clampedY);
            }
        }

        // v2.4 Bug A: ESC handled via MissionGameplay.OnKeyPress → HandleEscKey()
        // UAUIBack block removed — ChangeGameFocus(1) blocks LocalPress("UAUIBack").

        // ── Controller timers ──
        LFPG_SorterController_TEST ctrl = LFPG_SorterController_TEST.Cast(GetController());
        if (ctrl)
        {
            ctrl.TickTimers(dt);
        }

        // Read LFPG_MCP_SorterCmd here. ui_set_text writes the widget
        // directly; routing this through OnClick would hit the five guards.
        if (m_McpCmd)
        {
            PollMcpSorterCmd();
        }
    }

    void LFPG_SorterView_TEST()
    {
        m_IsOpen = false;
        m_FocusLocked = false;
        m_Dragging = false;
        m_DragOffX = 0.0;
        m_DragOffY = 0.0;
        m_HoveredBg = null;
        m_FadeAlpha = 1.0;
        m_FadingIn = false;
        m_McpCmd = null;
        m_McpCmdOwned = false;
        m_McpCmdCreateFailed = false;
        m_ColorDataRefs = new array<ref LFPG_ColorData_TEST>();
        m_TintedWidgets = new array<Widget>();
    }

    // S1 fix: destructor releases input lock if destroyed while open
    void ~LFPG_SorterView_TEST()
    {
        DestroyMcpCmdWidget();
        if (g_Game)
        {
            // v2.4 Bug D: Restore player actions on destruction
            // BUG 7 fix: unified cast to PlayerBase (matches DoOpen/DoClose)
            PlayerBase dtorPlayer = PlayerBase.Cast(g_Game.GetPlayer());
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
                Input inp = g_Game.GetInput();
                if (inp)
                {
                    inp.ChangeGameFocus(-1);
                }
                UIManager uiMgr = g_Game.GetUIManager();
                if (uiMgr)
                {
                    uiMgr.ShowUICursor(false);
                }
                m_FocusLocked = false;
            }
        }
    }

    // S3: ApplyColors lives in DoOpen(), NOT OnWidgetScriptInit — calling
    // there caused flash because colors were painted before data existed.

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
        wn = "ColumnSep";
        if (!ColumnSep) { ColumnSep = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "RulesPanelBg";
        if (!RulesPanelBg) { RulesPanelBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "PreviewPanelBg";
        if (!PreviewPanelBg) { PreviewPanelBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
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
        wn = "MatchFooterBg";
        if (!MatchFooterBg) { MatchFooterBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }

        // === Sprint 1 (2026-04-26): rail widget bindings ===
        wn = "OutputRailBg";
        if (!OutputRailBg) { OutputRailBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRailBorder";
        if (!OutputRailBorder) { OutputRailBorder = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "Step1Circle";
        if (!Step1Circle) { Step1Circle = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "Step1Number";
        if (!Step1Number) { Step1Number = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "Step1Title";
        if (!Step1Title) { Step1Title = TextWidget.Cast(root.FindAnyWidget(wn)); }
        // 6 rows — bind each by name (one block per row to keep it explicit)
        wn = "OutputRow0"; if (!OutputRow0) { OutputRow0 = root.FindAnyWidget(wn); }
        wn = "OutputRow0Bg"; if (!OutputRow0Bg) { OutputRow0Bg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow0Indicator"; if (!OutputRow0Indicator) { OutputRow0Indicator = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow0Label"; if (!OutputRow0Label) { OutputRow0Label = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow0Count"; if (!OutputRow0Count) { OutputRow0Count = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow0Container"; if (!OutputRow0Container) { OutputRow0Container = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow1"; if (!OutputRow1) { OutputRow1 = root.FindAnyWidget(wn); }
        wn = "OutputRow1Bg"; if (!OutputRow1Bg) { OutputRow1Bg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow1Indicator"; if (!OutputRow1Indicator) { OutputRow1Indicator = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow1Label"; if (!OutputRow1Label) { OutputRow1Label = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow1Count"; if (!OutputRow1Count) { OutputRow1Count = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow1Container"; if (!OutputRow1Container) { OutputRow1Container = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow2"; if (!OutputRow2) { OutputRow2 = root.FindAnyWidget(wn); }
        wn = "OutputRow2Bg"; if (!OutputRow2Bg) { OutputRow2Bg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow2Indicator"; if (!OutputRow2Indicator) { OutputRow2Indicator = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow2Label"; if (!OutputRow2Label) { OutputRow2Label = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow2Count"; if (!OutputRow2Count) { OutputRow2Count = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow2Container"; if (!OutputRow2Container) { OutputRow2Container = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow3"; if (!OutputRow3) { OutputRow3 = root.FindAnyWidget(wn); }
        wn = "OutputRow3Bg"; if (!OutputRow3Bg) { OutputRow3Bg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow3Indicator"; if (!OutputRow3Indicator) { OutputRow3Indicator = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow3Label"; if (!OutputRow3Label) { OutputRow3Label = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow3Count"; if (!OutputRow3Count) { OutputRow3Count = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow3Container"; if (!OutputRow3Container) { OutputRow3Container = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow4"; if (!OutputRow4) { OutputRow4 = root.FindAnyWidget(wn); }
        wn = "OutputRow4Bg"; if (!OutputRow4Bg) { OutputRow4Bg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow4Indicator"; if (!OutputRow4Indicator) { OutputRow4Indicator = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow4Label"; if (!OutputRow4Label) { OutputRow4Label = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow4Count"; if (!OutputRow4Count) { OutputRow4Count = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow4Container"; if (!OutputRow4Container) { OutputRow4Container = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow5"; if (!OutputRow5) { OutputRow5 = root.FindAnyWidget(wn); }
        wn = "OutputRow5Bg"; if (!OutputRow5Bg) { OutputRow5Bg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow5Indicator"; if (!OutputRow5Indicator) { OutputRow5Indicator = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow5Label"; if (!OutputRow5Label) { OutputRow5Label = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow5Count"; if (!OutputRow5Count) { OutputRow5Count = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "OutputRow5Container"; if (!OutputRow5Container) { OutputRow5Container = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "CatchAllRow"; if (!CatchAllRow) { CatchAllRow = root.FindAnyWidget(wn); }
        wn = "CatchAllRowBg"; if (!CatchAllRowBg) { CatchAllRowBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "CatchAllRowIndicator"; if (!CatchAllRowIndicator) { CatchAllRowIndicator = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "CatchAllRowLabel"; if (!CatchAllRowLabel) { CatchAllRowLabel = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "CatchAllRowSublabel"; if (!CatchAllRowSublabel) { CatchAllRowSublabel = TextWidget.Cast(root.FindAnyWidget(wn)); }

        // === Sprint 2 (2026-04-26): builder tab bindings ===
        wn = "BuilderTabBg"; if (!BuilderTabBg) { BuilderTabBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "BuilderTabBorder"; if (!BuilderTabBorder) { BuilderTabBorder = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "BuilderTabCategory"; if (!BuilderTabCategory) { BuilderTabCategory = root.FindAnyWidget(wn); }
        wn = "BuilderTabCategoryBg"; if (!BuilderTabCategoryBg) { BuilderTabCategoryBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "BuilderTabCategoryUnderline"; if (!BuilderTabCategoryUnderline) { BuilderTabCategoryUnderline = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "BuilderTabCategoryText"; if (!BuilderTabCategoryText) { BuilderTabCategoryText = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "BuilderTabPrefix"; if (!BuilderTabPrefix) { BuilderTabPrefix = root.FindAnyWidget(wn); }
        wn = "BuilderTabPrefixBg"; if (!BuilderTabPrefixBg) { BuilderTabPrefixBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "BuilderTabPrefixUnderline"; if (!BuilderTabPrefixUnderline) { BuilderTabPrefixUnderline = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "BuilderTabPrefixText"; if (!BuilderTabPrefixText) { BuilderTabPrefixText = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "BuilderTabContains"; if (!BuilderTabContains) { BuilderTabContains = root.FindAnyWidget(wn); }
        wn = "BuilderTabContainsBg"; if (!BuilderTabContainsBg) { BuilderTabContainsBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "BuilderTabContainsUnderline"; if (!BuilderTabContainsUnderline) { BuilderTabContainsUnderline = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "BuilderTabContainsText"; if (!BuilderTabContainsText) { BuilderTabContainsText = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "BuilderTabSlot"; if (!BuilderTabSlot) { BuilderTabSlot = root.FindAnyWidget(wn); }
        wn = "BuilderTabSlotBg"; if (!BuilderTabSlotBg) { BuilderTabSlotBg = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "BuilderTabSlotUnderline"; if (!BuilderTabSlotUnderline) { BuilderTabSlotUnderline = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "BuilderTabSlotText"; if (!BuilderTabSlotText) { BuilderTabSlotText = TextWidget.Cast(root.FindAnyWidget(wn)); }

        // === Sprint 3 (2026-04-26): Active Rules header bindings ===
        wn = "Step3Circle"; if (!Step3Circle) { Step3Circle = ImageWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "Step3Number"; if (!Step3Number) { Step3Number = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "Step3Title"; if (!Step3Title) { Step3Title = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "RulesSublabel"; if (!RulesSublabel) { RulesSublabel = TextWidget.Cast(root.FindAnyWidget(wn)); }


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
            // U3 (2026-04-26): silent-failure diagnostic. If BtnCloseX has no
            // ImageWidget child the X button stays invisible (Tint() null-checks
            // and skips). Without this log we'd debug it cold.
            if (!BtnCloseXBg)
            {
                string wWarn = "[SorterView] BtnCloseX child-walk: no ImageWidget child found — X button will be invisible";
                LFPG_Util.Warn(wWarn);
            }
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
        wn = "EditPrefixHint";
        if (!EditPrefixHint) { EditPrefixHint = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EditContainsHint";
        if (!EditContainsHint) { EditContainsHint = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EditSlotMinHint";
        if (!EditSlotMinHint) { EditSlotMinHint = TextWidget.Cast(root.FindAnyWidget(wn)); }
        wn = "EditSlotMaxHint";
        if (!EditSlotMaxHint) { EditSlotMaxHint = TextWidget.Cast(root.FindAnyWidget(wn)); }
    }

    // =========================================================
    // M2: Assign UserIDs to buttons for int-based dispatch.
    // Called once from DoOpen after EnsureViewBindings.
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



        // Category buttons (200+i)
        int ci = 0;
        string catPrefix = "CatBtn";
        for (ci = 0; ci < 8; ci = ci + 1)
        {
            wn = catPrefix;
            wn = wn + ci.ToString();
            btn = root.FindAnyWidget(wn);
            if (btn)
            {
                int catId = UID_CAT_BASE;
                catId = catId + ci;
                btn.SetUserID(catId);
            }
        }

        // Slot presets (300+i)
        int si = 0;
        string slotPrefix = "SlotPre";
        for (si = 0; si < 4; si = si + 1)
        {
            wn = slotPrefix;
            wn = wn + si.ToString();
            btn = root.FindAnyWidget(wn);
            if (btn)
            {
                int slotId = UID_SLOT_BASE;
                slotId = slotId + si;
                btn.SetUserID(slotId);
            }
        }

        // Add buttons
        wn = "BtnPrefixAdd";
        btn = root.FindAnyWidget(wn);
        if (btn) { btn.SetUserID(UID_PREFIX_ADD); }
        wn = "BtnContainsAdd";
        btn = root.FindAnyWidget(wn);
        if (btn) { btn.SetUserID(UID_CONTAINS_ADD); }
        wn = "BtnSlotAdd";
        btn = root.FindAnyWidget(wn);
        if (btn) { btn.SetUserID(UID_SLOT_ADD); }

        // Action buttons
        wn = "BtnCatchAll";
        btn = root.FindAnyWidget(wn);
        if (btn) { btn.SetUserID(UID_CATCH_ALL); }
        wn = "BtnClearOut";
        btn = root.FindAnyWidget(wn);
        if (btn) { btn.SetUserID(UID_CLEAR_OUT); }
        wn = "BtnResetAll";
        btn = root.FindAnyWidget(wn);
        if (btn) { btn.SetUserID(UID_RESET_ALL); }
        wn = "BtnSave";
        btn = root.FindAnyWidget(wn);
        if (btn) { btn.SetUserID(UID_SAVE); }
        wn = "BtnPreview";
        btn = root.FindAnyWidget(wn);
        if (btn) { btn.SetUserID(UID_TAB_PREVIEW); }
        wn = "BtnCloseX";
        btn = root.FindAnyWidget(wn);
        if (btn) { btn.SetUserID(UID_CLOSE_X); }
        wn = "BtnSortHeader";
        btn = root.FindAnyWidget(wn);
        if (btn) { btn.SetUserID(UID_SORT_HEADER); }
        // Sprint 1 (2026-04-26): rail row UIDs
        if (OutputRow0) { OutputRow0.SetUserID(UID_RAIL_ROW_BASE + 0); }
        if (OutputRow1) { OutputRow1.SetUserID(UID_RAIL_ROW_BASE + 1); }
        if (OutputRow2) { OutputRow2.SetUserID(UID_RAIL_ROW_BASE + 2); }
        if (OutputRow3) { OutputRow3.SetUserID(UID_RAIL_ROW_BASE + 3); }
        if (OutputRow4) { OutputRow4.SetUserID(UID_RAIL_ROW_BASE + 4); }
        if (OutputRow5) { OutputRow5.SetUserID(UID_RAIL_ROW_BASE + 5); }
        if (CatchAllRow) { CatchAllRow.SetUserID(UID_RAIL_CATCHALL); }
        // Sprint 2 (2026-04-26): builder tab UIDs
        if (BuilderTabCategory) { BuilderTabCategory.SetUserID(UID_BUILDER_TAB_BASE + 0); }
        if (BuilderTabPrefix)   { BuilderTabPrefix.SetUserID(UID_BUILDER_TAB_BASE + 1); }
        if (BuilderTabContains) { BuilderTabContains.SetUserID(UID_BUILDER_TAB_BASE + 2); }
        if (BuilderTabSlot)     { BuilderTabSlot.SetUserID(UID_BUILDER_TAB_BASE + 3); }
        m_ButtonIDsAssigned = true;
    }

    protected void ApplyColors()
    {
        // F1-B (2026-04-26): null SetUserData on previously tinted widgets
        // BEFORE freeing the LFPG_ColorData_TEST they point at. SetUserData stores
        // a raw pointer; without this step, the next CacheColorLocal would
        // GetUserData() a dangling pointer and Cast() it → heap corruption
        // (verified crash 2026-04-26: SEH 0xc0000374 at CacheColorLocal:723).
        int twn = m_TintedWidgets.Count();
        int twi = 0;
        while (twi < twn)
        {
            Widget tw = m_TintedWidgets[twi];
            if (tw)
            {
                tw.SetUserData(null);
            }
            twi = twi + 1;
        }
        m_TintedWidgets.Clear();

        // F1-A: Clear stale strong refs from previous Open cycle.
        // SetUserData does NOT hold strong ref — m_ColorDataRefs keeps
        // objects alive. Without Clear(), each Open accumulates ~130 objects.
        m_ColorDataRefs.Clear();

        // BtnCloseX default color (static chrome is painted by the layout)
        Tint(BtnCloseXBg, COL_S2_HEADER);
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

        // v2.4 Bug C: Unpaired overlay labels (overlay bg painted by the layout)
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
    }

    protected void Tint(ImageWidget img, int color)
    {
        if (!img)
            return;
        img.SetColor(color);
        // Cache for hover system (v2.2)
        CacheColorLocal(img, color);
    }

    // M4: Store color on the widget itself via SetUserData (O(1) lookup).
    // Reuses existing LFPG_ColorData_TEST if present (avoids alloc+GC churn on refresh).
    // F1-B (2026-04-26): track widget in m_TintedWidgets so ApplyColors can
    // null SetUserData before freeing data — prevents dangling-pointer crash.
    protected void CacheColorLocal(Widget w, int color)
    {
        if (!w)
            return;
        Class rawData = null;
        w.GetUserData(rawData);
        LFPG_ColorData_TEST existing = LFPG_ColorData_TEST.Cast(rawData);
        if (existing)
        {
            existing.m_BaseColor = color;
            return;
        }
        LFPG_ColorData_TEST data = new LFPG_ColorData_TEST(color);
        w.SetUserData(data);
        m_ColorDataRefs.Insert(data);
        m_TintedWidgets.Insert(w);
    }

    // M4: O(1) color retrieval via GetUserData — replaces O(n) array scan
    protected int FindCachedColor(Widget w)
    {
        if (!w)
            return 0;
        Class rawData = null;
        w.GetUserData(rawData);
        LFPG_ColorData_TEST colorData = LFPG_ColorData_TEST.Cast(rawData);
        if (!colorData)
            return 0;
        return colorData.m_BaseColor;
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
                    if (restoreCol != 0)
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
            bool baseNoBtn = super.OnClick(w, x, y, button);
            return baseNoBtn;
        }

        LFPG_SorterController_TEST ctrl = LFPG_SorterController_TEST.Cast(GetController());
        if (!ctrl)
        {
            bool baseNoCtrl = super.OnClick(w, x, y, button);
            return baseNoCtrl;
        }

        // M2: Dispatch by UserID (int) — no string comparisons
        int uid = btn.GetUserID();

        // Sprint 1 (2026-04-26): vertical rail rows -> same SelectOutput
        if (uid >= UID_RAIL_ROW_BASE && uid < UID_RAIL_ROW_BASE + 6)
        {
            int railIdx = uid - UID_RAIL_ROW_BASE;
            ctrl.SelectOutput(railIdx);
            return true;
        }
        // Sprint 1: rail catch-all routes to existing BtnCatchAll
        // (toggles m_IsCatchAll on the currently-selected output).
        if (uid == UID_RAIL_CATCHALL)
        {
            ctrl.BtnCatchAll();
            return true;
        }
        // Sprint 2 (2026-04-26): builder tab dispatch
        if (uid >= UID_BUILDER_TAB_BASE && uid < UID_BUILDER_TAB_BASE + 4)
        {
            int builderIdx = uid - UID_BUILDER_TAB_BASE;
            ctrl.SelectBuilderTab_TEST(builderIdx);
            return true;
        }
        // Preview toggle
        if (uid == UID_TAB_PREVIEW) { ctrl.TogglePreview_TEST(); return true; }
        // Category buttons: 200..207
        if (uid >= UID_CAT_BASE && uid < UID_CAT_BASE + 8)
        {
            int catIdx = uid - UID_CAT_BASE;
            ctrl.ToggleCategoryByIdx(catIdx);
            return true;
        }
        // Slot presets: 300..303
        if (uid >= UID_SLOT_BASE && uid < UID_SLOT_BASE + 4)
        {
            int slotIdx = uid - UID_SLOT_BASE;
            ctrl.ToggleSlotByIdx(slotIdx);
            return true;
        }
        // Add buttons
        if (uid == UID_PREFIX_ADD)   { ctrl.BtnPrefixAdd();   return true; }
        if (uid == UID_CONTAINS_ADD) { ctrl.BtnContainsAdd(); return true; }
        if (uid == UID_SLOT_ADD)     { ctrl.BtnSlotAdd();     return true; }
        // Action buttons
        if (uid == UID_CATCH_ALL)    { ctrl.BtnCatchAll();    return true; }
        if (uid == UID_CLEAR_OUT)    { ctrl.BtnClearOut();    return true; }
        if (uid == UID_RESET_ALL)    { ctrl.BtnResetAll();    return true; }
        if (uid == UID_SAVE)         { ctrl.BtnSave();        return true; }
        if (uid == UID_CLOSE_X)      { ctrl.BtnCloseX();      return true; }
        if (uid == UID_SORT_HEADER)  { ctrl.BtnSortHeader();  return true; }

        // v4.3: Tag BtnRemove dispatch (Plan B — Relay_Command never
        // reached TagController because SorterView.OnClick intercepted).
        // F3-C: UID encoding: 600 + outputIdx*16 + (ruleIdx+1)
        if (uid >= UID_TAG_REMOVE_BASE && uid < UID_TAG_REMOVE_BASE + 96)
        {
            int encoded = uid - UID_TAG_REMOVE_BASE;
            int outIdx = encoded / 16;
            int remainder = encoded - (outIdx * 16);
            int rIdx = remainder - 1;
            ctrl.OnRemoveTag(outIdx, rIdx);
            return true;
        }

        // Unrecognized button — delegate to ScriptView base class
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
    // M5: DPI-safe position clamp — shared between CenterPanel and drag.
    // minY allows drag to keep header visible (5.0) while center uses 0.0.
    // At DPI > 100%, physical screen is larger than logical viewport.
    // Caps ensure panel stays visible regardless of DPI.
    // =========================================================
    protected void ClampPanelPos(float inX, float inY, float minY, out float outX, out float outY)
    {
        int scrW = 0;
        int scrH = 0;
        GetScreenSize(scrW, scrH);
        float panW = 0.0;
        float panH = 0.0;
        if (SorterPanel)
        {
            SorterPanel.GetSize(panW, panH);
        }

        // DPI-safe max — at DPI > 100%, physical > logical viewport
        float maxX = scrW - panW;
        float maxY = scrH - panH;
        float dpiCapX = panW;
        float dpiCapY = panH * 0.5;
        if (maxX > dpiCapX)
        {
            maxX = dpiCapX;
        }
        if (maxY > dpiCapY)
        {
            maxY = dpiCapY;
        }
        // At very low resolutions, max could be < min
        if (maxX < 0.0)
        {
            maxX = 0.0;
        }
        if (maxY < minY)
        {
            maxY = minY;
        }

        outX = inX;
        outY = inY;
        if (outX < 0.0) { outX = 0.0; }
        if (outY < minY) { outY = minY; }
        if (outX > maxX) { outX = maxX; }
        if (outY > maxY) { outY = maxY; }
    }

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

        // Ideal centered position
        float cx = (scrW - panW) * 0.5;
        float cy = (scrH - panH) * 0.5;

        // M5: Clamp via shared helper (minY=0 for centering)
        float clampedX = 0.0;
        float clampedY = 0.0;
        float minY = 0.0;
        ClampPanelPos(cx, cy, minY, clampedX, clampedY);
        SorterPanel.SetPos(clampedX, clampedY);
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
            // v4.3: Badge uses header blue instead of green for visual consistency
            Tint(PairingBadgeBg, COL_HEADER);
            if (PairingBadgeText)
            {
                // v3.1: Show container name instead of generic "PAIRED"
                string badgePrefix = ">> ";
                string pairedLabel = badgePrefix;
                pairedLabel = pairedLabel + containerDisplayName;
                PairingBadgeText.SetText(pairedLabel);
                PairingBadgeText.SetColor(COL_TEXT);
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
        LFPG_SorterController_TEST ctrl = LFPG_SorterController_TEST.Cast(GetController());
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
        s_Instance = new LFPG_SorterView_TEST();
        Widget root = s_Instance.GetLayoutRoot();
        if (root)
        {
            root.Show(false);
            root.SetSort(50000);
        }
        #endif
    }

    // Called from RPC — widgets already exist, just show + data
    static void Open(string configJSON, string containerName, string d0, string d1, string d2, string d3, string d4, string d5, int netLow, int netHigh)
    {
        #ifndef SERVER
        if (LFPG_SorterView.IsOpen())
        {
            string dualOpenMsg = "[LFPG_Sorter_TEST] Open blocked: production sorter is already open";
            Print(dualOpenMsg);
            return;
        }
        bool constructedNow = false;
        if (!s_Instance)
        {
            Init();
            constructedNow = true;
        }
        if (!s_Instance)
        {
            string warnMsg = "[SorterView_TEST] Lazy Init failed";
            LFPG_Util.Warn(warnMsg);
            return;
        }

        if (constructedNow && LFPG_PERFDIAG_ENABLED)
        {
            s_PerfDiagConstructionCount = s_PerfDiagConstructionCount + 1;
            string sorterId = "";
            EntityAI sorterEntity = EntityAI.Cast(g_Game.GetObjectByNetworkId(netLow, netHigh));
            if (sorterEntity)
            {
                sorterId = LFPG_DeviceAPI.GetDeviceId(sorterEntity);
            }
            string perfView = "[LFPG_Sorter_TEST] LFPG_PERFDIAG t=";
            perfView = perfView + g_Game.GetTickTime().ToString();
            perfView = perfView + " deviceId=";
            perfView = perfView + sorterId;
            perfView = perfView + " test_view_construction=";
            perfView = perfView + s_PerfDiagConstructionCount.ToString();
            Print(perfView);
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
        // A7: Record timestamp before close
        if (g_Game)
        {
            s_EscCloseTime = g_Game.GetTickTime();
        }
        s_Instance.DoClose();
        return true;
    }

    // A7: ESC cooldown check — true if UI was closed < 200ms ago
    static bool IsEscCooldown()
    {
        if (s_EscCloseTime <= 0.0)
            return false;
        if (!g_Game)
            return false;
        float now = g_Game.GetTickTime();
        float elapsed = now - s_EscCloseTime;
        if (elapsed < 0.2)
            return true;
        return false;
    }

    // S2 fix: Cleanup deletes instance properly (prevents leak).
    // Input release is handled by the destructor (S1) which has
    // g_Game null guard for safe shutdown.
    static void Cleanup()
    {
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
        LFPG_SorterController_TEST ctrl = LFPG_SorterController_TEST.Cast(s_Instance.GetController());
        if (ctrl)
        {
            ctrl.HandleSaveAck(success);
        }
    }

    // v3.2: Server sort result → delegate to Controller
    static void OnSortAck(bool success, int movedCount)
    {
        if (!s_Instance)
            return;
        if (!s_Instance.m_IsOpen)
            return;
        LFPG_SorterController_TEST ctrl = LFPG_SorterController_TEST.Cast(s_Instance.GetController());
        if (ctrl)
        {
            ctrl.HandleSortAck(success, movedCount);
        }
    }

    // v2.6: Server preview response → delegate to Controller
    // v4.3: slots changed from array<int> to array<string> (formatted info)
    static void OnPreviewData(int outputIdx, int totalMatched, array<string> names, array<string> cats, array<string> infos)
    {
        if (!s_Instance)
            return;
        if (!s_Instance.m_IsOpen)
            return;
        LFPG_SorterController_TEST ctrl = LFPG_SorterController_TEST.Cast(s_Instance.GetController());
        if (ctrl)
        {
            ctrl.PopulatePreview(outputIdx, totalMatched, names, cats, infos);
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
        if (g_Game)
        {
            PlayerBase openPlayer = PlayerBase.Cast(g_Game.GetPlayer());
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
        // M2: Assign int IDs to buttons (only first open)
        AssignButtonIDs();
        m_McpCmdCreateFailed = false;
        EnsureMcpCmdWidget();
        LFPG_SorterController_TEST ctrl = LFPG_SorterController_TEST.Cast(GetController());
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

        if (S1_PROBE)
        {
            RunS1Probe();
        }

        string openMsg = "[SorterView] Opened for: ";
        openMsg = openMsg + containerName;
        LFPG_Util.Info(openMsg);
    }

    // S1 instrumented probe: layout metrics, SetSize units, SetColor without texture.
    protected static void RunS1Probe()
    {
        if (!s_Instance)
            return;

        string line;
        float szW = 0.0;
        float szH = 0.0;
        float posX = 0.0;
        float posY = 0.0;
        float scrW = 0.0;
        float scrH = 0.0;
        float scrX = 0.0;
        float scrY = 0.0;
        float origW = 0.0;
        float origH = 0.0;
        float afterW = 0.0;
        float afterH = 0.0;
        float restW = 0.0;
        float restH = 0.0;

        Widget panel = s_Instance.SorterPanel;
        if (panel)
        {
            panel.GetSize(szW, szH);
            panel.GetPos(posX, posY);
            panel.GetScreenSize(scrW, scrH);
            panel.GetScreenPos(scrX, scrY);
            line = string.Format("[LFPG_Sorter_TEST][S1Probe] sorterpanel_size_w=%1", szW);
            Print(line);
            line = string.Format("[LFPG_Sorter_TEST][S1Probe] sorterpanel_size_h=%1", szH);
            Print(line);
            line = string.Format("[LFPG_Sorter_TEST][S1Probe] sorterpanel_pos_x=%1", posX);
            Print(line);
            line = string.Format("[LFPG_Sorter_TEST][S1Probe] sorterpanel_pos_y=%1", posY);
            Print(line);
            line = string.Format("[LFPG_Sorter_TEST][S1Probe] sorterpanel_screensize_w=%1", scrW);
            Print(line);
            line = string.Format("[LFPG_Sorter_TEST][S1Probe] sorterpanel_screensize_h=%1", scrH);
            Print(line);
            line = string.Format("[LFPG_Sorter_TEST][S1Probe] sorterpanel_screenpos_x=%1", scrX);
            Print(line);
            line = string.Format("[LFPG_Sorter_TEST][S1Probe] sorterpanel_screenpos_y=%1", scrY);
            Print(line);
        }

        Widget col = s_Instance.OutputRailBg;
        if (col)
        {
            col.GetSize(szW, szH);
            col.GetPos(posX, posY);
            col.GetScreenSize(scrW, scrH);
            col.GetScreenPos(scrX, scrY);
            line = string.Format("[LFPG_Sorter_TEST][S1Probe] outputrailbg_size_w=%1", szW);
            Print(line);
            line = string.Format("[LFPG_Sorter_TEST][S1Probe] outputrailbg_size_h=%1", szH);
            Print(line);
            line = string.Format("[LFPG_Sorter_TEST][S1Probe] outputrailbg_pos_x=%1", posX);
            Print(line);
            line = string.Format("[LFPG_Sorter_TEST][S1Probe] outputrailbg_pos_y=%1", posY);
            Print(line);
            line = string.Format("[LFPG_Sorter_TEST][S1Probe] outputrailbg_screensize_w=%1", scrW);
            Print(line);
            line = string.Format("[LFPG_Sorter_TEST][S1Probe] outputrailbg_screensize_h=%1", scrH);
            Print(line);
            line = string.Format("[LFPG_Sorter_TEST][S1Probe] outputrailbg_screenpos_x=%1", scrX);
            Print(line);
            line = string.Format("[LFPG_Sorter_TEST][S1Probe] outputrailbg_screenpos_y=%1", scrY);
            Print(line);
        }

    }

    protected void DoClose()
    {
        if (!m_IsOpen)
            return;
        m_IsOpen = false;
        m_Dragging = false;
        m_FadingIn = false;
        m_HoveredBg = null;
        if (m_McpCmd)
        {
            m_McpCmd.Show(false);
        }

        // FIX 2: Release tag/preview views now (breaks circular refs).
        // Without this, views survive until next Open or full Cleanup.
        LFPG_SorterController_TEST ctrl = LFPG_SorterController_TEST.Cast(GetController());
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
        if (g_Game)
        {
            PlayerBase closePlayer = PlayerBase.Cast(g_Game.GetPlayer());
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
        if (!g_Game)
            return;
        UIManager uiMgr = g_Game.GetUIManager();
        if (uiMgr)
        {
            uiMgr.ShowUICursor(true);
        }
        if (!m_FocusLocked)
        {
            Input inp = g_Game.GetInput();
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
        if (!g_Game)
            return;
        UIManager uiMgr = g_Game.GetUIManager();
        if (uiMgr)
        {
            uiMgr.ShowUICursor(false);
        }
        if (m_FocusLocked)
        {
            Input inp = g_Game.GetInput();
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
        // FIX 5: Guard symmetric with OnMouseEnter
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
        LFPG_SorterController_TEST ctrl = null;
        string wName = "";

        // KC_RETURN = 28
        if (key == KeyCode.KC_RETURN)
        {
            focused = GetFocus();
            if (!focused)
                return false;

            ctrl = LFPG_SorterController_TEST.Cast(GetController());
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


    // MCP TEST command hook. Lives in LFPG_MCP_SorterCmd.layout, not the
    // panel layout. Polled from Update because ui_set_text calls SetText
    // directly (MCPClientBridge.c DispatchUiSetText) and never OnClick.
    protected void EnsureMcpCmdWidget()
    {
        if (m_McpCmd)
        {
            m_McpCmd.Show(true);
            return;
        }
        if (m_McpCmdCreateFailed)
        {
            return;
        }
        string mcpFail = "[SorterView] MCP cmd hook create failed";
        if (!g_Game)
        {
            m_McpCmdCreateFailed = true;
            LFPG_Util.Warn(mcpFail);
            return;
        }
        WorkspaceWidget ws = g_Game.GetWorkspace();
        if (!ws)
        {
            m_McpCmdCreateFailed = true;
            LFPG_Util.Warn(mcpFail);
            return;
        }
        string hookName = "LFPG_MCP_SorterCmd";
        Widget existing = ws.FindAnyWidget(hookName);
        if (existing)
        {
            m_McpCmd = EditBoxWidget.Cast(existing);
            if (m_McpCmd)
            {
                m_McpCmdOwned = false;
                m_McpCmd.Show(true);
                return;
            }
        }
        string layoutPath = "LFPowerGrid/gui/layouts/test/LFPG_MCP_SorterCmd.layout";
        Widget created = ws.CreateWidgets(layoutPath);
        m_McpCmd = EditBoxWidget.Cast(created);
        if (!m_McpCmd)
        {
            if (created)
            {
                m_McpCmd = EditBoxWidget.Cast(created.FindAnyWidget(hookName));
            }
        }
        if (m_McpCmd)
        {
            m_McpCmdOwned = true;
            m_McpCmd.Show(true);
            string emptyText = "";
            m_McpCmd.SetText(emptyText);
        }
        else
        {
            m_McpCmdCreateFailed = true;
            LFPG_Util.Warn(mcpFail);
        }
    }

    protected void DestroyMcpCmdWidget()
    {
        if (!m_McpCmd)
        {
            return;
        }
        if (m_McpCmdOwned)
        {
            if (g_Game)
            {
                m_McpCmd.Unlink();
            }
        }
        m_McpCmd = null;
        m_McpCmdOwned = false;
    }

    protected void PollMcpSorterCmd()
    {
        if (!m_McpCmd)
        {
            return;
        }
        string cmd = m_McpCmd.GetText();
        cmd.TrimInPlace();
        if (cmd == "")
        {
            return;
        }
        string emptyText = "";
        m_McpCmd.SetText(emptyText);
        bool ok = DispatchMcpCommand(cmd);
        WriteMcpDump(cmd, ok);
    }

    protected bool DispatchMcpCommand(string cmd)
    {
        LFPG_SorterController_TEST ctrl = LFPG_SorterController_TEST.Cast(GetController());
        if (cmd == "dump")
        {
            return true;
        }
        if (cmd == "close")
        {
            if (!ctrl)
            {
                return false;
            }
            ctrl.BtnCloseX();
            return true;
        }
        if (cmd == "catch_all")
        {
            if (!ctrl)
            {
                return false;
            }
            if (!ctrl.McpCanEdit())
            {
                return false;
            }
            ctrl.BtnCatchAll();
            return true;
        }
        if (cmd == "tab_preview")
        {
            if (!ctrl)
            {
                return false;
            }
            ctrl.TabPreview();
            return true;
        }
        if (cmd.IndexOf("cat:") == 0)
        {
            int cmdLen = cmd.Length();
            if (cmdLen <= 4)
            {
                return false;
            }
            string rest = cmd.Substring(4, cmdLen - 4);
            int idx = rest.ToInt();
            string idxText = idx.ToString();
            if (idxText != rest)
            {
                return false;
            }
            if (!ctrl)
            {
                return false;
            }
            if (!ctrl.McpCanEdit())
            {
                return false;
            }
            int catCount = ctrl.McpCategoryCount();
            if (idx < 0 || idx >= catCount)
            {
                return false;
            }
            ctrl.ToggleCategoryByIdx(idx);
            return true;
        }
        return false;
    }

    protected string McpJsonBool(bool v)
    {
        if (v)
        {
            return "true";
        }
        return "false";
    }

    // Escapes are built by CONCATENATING single-escape literals, never written as
    // one literal that holds two escape sequences. A lone escaped backslash and a
    // lone escaped quote each compile (vanilla ships both); a literal carrying two
    // of them does not. The client died on this with CParser: quoted string not
    // closed, and took the whole World module with it. Vanilla's own JSON writer
    // uses this same concatenation (scripts/3_game/tools/jsonobject.c:54-55).
    // Deliberately phrased without the offending character sequences: a comment is
    // supposed to be lexed before string literals, and that assumption is exactly
    // what cost a boot here, so it is not worth re-testing in production code.
    protected string McpJsonEscape(string s)
    {
        string bs = "\\";
        string quote = "\"";
        string outStr = s;
        outStr.Replace(bs, bs + bs);
        outStr.Replace(quote, bs + quote);
        outStr.Replace("\n", bs + "n");
        outStr.Replace("\r", bs + "r");
        outStr.Replace("\t", bs + "t");
        return outStr;
    }

    protected void WriteMcpDump(string cmd, bool ok)
    {
        bool openFlag = m_IsOpen;
        bool paired = false;
        bool powered = false;
        string status = "";
        bool catchAll = false;
        int ruleCount = 0;
        int closeUid = 0;

        Widget root = GetLayoutRoot();
        if (root)
        {
            string closeName = "BtnCloseX";
            Widget closeW = root.FindAnyWidget(closeName);
            if (closeW)
            {
                closeUid = closeW.GetUserID();
            }
        }

        LFPG_SorterController_TEST ctrl = LFPG_SorterController_TEST.Cast(GetController());
        if (ctrl)
        {
            ctrl.McpCollectState(paired, powered, status, catchAll, ruleCount);
        }

        string json = "{";
        json = json + "\"cmd\":\"";
        json = json + McpJsonEscape(cmd);
        json = json + "\",\"ok\":";
        json = json + McpJsonBool(ok);
        json = json + ",\"open\":";
        json = json + McpJsonBool(openFlag);
        json = json + ",\"powered\":";
        json = json + McpJsonBool(powered);
        json = json + ",\"paired\":";
        json = json + McpJsonBool(paired);
        json = json + ",\"status\":\"";
        json = json + McpJsonEscape(status);
        json = json + "\",\"close_x_uid\":";
        json = json + closeUid.ToString();
        json = json + ",\"catch_all\":";
        json = json + McpJsonBool(catchAll);
        json = json + ",\"rule_count\":";
        json = json + ruleCount.ToString();
        json = json + "}";

        string dumpPath = "$profile:lfpg_sorter_mcp.json";
        FileHandle file = OpenFile(dumpPath, FileMode.WRITE);
        if (file == 0)
        {
            string dumpFail = "[SorterView] MCP dump OpenFile failed";
            LFPG_Util.Warn(dumpFail);
            return;
        }
        FPrint(file, json);
        CloseFile(file);
    }

    LFPG_SorterController_TEST GetSorterController()
    {
        return LFPG_SorterController_TEST.Cast(GetController());
    }

    // [IS2 cleanup] Sprint 1 accessors deleted — controller now caches
    // widgets directly in m_*_TEST fields, no view-side accessors needed.
};
#endif
