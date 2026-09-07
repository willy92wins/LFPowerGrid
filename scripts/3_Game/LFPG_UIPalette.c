#ifndef SERVER
// Client-only compilation boundary
// =========================================================
// LF_PowerGrid — UI palette (neutral)
//
// Shared ARGB colors + LFPG_ColorData for SetUserData hover cache.
// Lives in 3_Game so World consumers (ATM, sorter V3) can load it.
// Canonical values: do not "normalize" duplicates; two names, two constants.
//
// Enforce Script: no ternaries, no increment ops, no foreach, no compound assign.
// =========================================================

// Per-widget color data for O(1) hover lookup via SetUserData.
// SetUserData does NOT hold a strong ref — callers keep array<ref LFPG_ColorData>.
class LFPG_ColorData extends Managed
{
    int m_BaseColor;

    void LFPG_ColorData(int color)
    {
        m_BaseColor = color;
    }
};

class LFPG_UIPalette
{
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
};
#endif
