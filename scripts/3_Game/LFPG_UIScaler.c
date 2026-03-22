// =========================================================
// LF_PowerGrid — UI Scaler (v3.2)
//
// Script-based proportional scaling for fixed-pixel UIs.
// Captures design-time (1080p) positions/sizes from the widget
// tree, then reapplies them multiplied by a resolution factor.
//
// Usage:
//   Init  → LFPG_UIScaler.Capture(panelRoot)
//   Open  → LFPG_UIScaler.DetectLogicalSize(fullscreenRoot)
//           float s = LFPG_UIScaler.ComputeScale();
//           LFPG_UIScaler.Apply(s);
//   Close → (no action needed)
//   Quit  → LFPG_UIScaler.Reset();
//
// v3.2: DPI-aware. DetectLogicalSize reads the rendered size
// of a fullscreen root widget to get logical (widget-space)
// dimensions. Fixes overflow at 4K with Windows DPI > 100%.
//
// The Capture pass stores original values so Apply can be
// called repeatedly (e.g. on every open) without accumulating
// rounding error — it always multiplies from the original.
//
// Heuristic: widgets with BOTH w <= 1.5 AND h <= 1.5 are
// treated as proportional (0.0-1.0 coords) and skipped.
//
// Layer: 3_Game (no 4_World dependencies).
// Enforce Script: no ternaries, no ++/--, no foreach, no +=/-=.
// =========================================================

class LFPG_UIScaler
{
    // Design resolution (base values captured at this res)
    static const float DESIGN_W = 1920.0;
    static const float DESIGN_H = 1080.0;

    // Scale clamps
    static const float SCALE_MIN = 0.65;
    static const float SCALE_MAX = 2.0;

    // Threshold: if BOTH size components <= this, treat as proportional
    static const float PROP_THRESHOLD = 1.5;

    // Parallel arrays — captured design values
    protected static ref array<Widget> s_Widgets;
    protected static ref array<float>  s_DesignX;
    protected static ref array<float>  s_DesignY;
    protected static ref array<float>  s_DesignW;
    protected static ref array<float>  s_DesignH;
    protected static bool s_Captured;
    protected static bool s_LoggedOnce;

    // v3.2: Detected logical screen size (widget coordinate space).
    // At DPI > 100%, global GetScreenSize returns physical pixels
    // but SetPos/SetSize operate in logical pixels. This mismatch
    // causes panels to overflow. DetectLogicalSize reads the
    // rendered size of a fullscreen (size 1 1) root widget to get
    // the actual available space in widget coordinates.
    protected static float s_LogicalW;
    protected static float s_LogicalH;
    protected static bool s_LogicalDetected;

    // ─────────────────────────────────────────────
    // Capture: walk the widget tree, store design values
    // Call ONCE after layout is created (in Init).
    // ─────────────────────────────────────────────
    static void Capture(Widget root)
    {
        if (!root)
            return;

        // Create arrays on first call (persist until Reset)
        if (!s_Widgets)
        {
            s_Widgets = new array<Widget>();
            s_DesignX = new array<float>();
            s_DesignY = new array<float>();
            s_DesignW = new array<float>();
            s_DesignH = new array<float>();
        }

        // Clear any previous capture
        s_Widgets.Clear();
        s_DesignX.Clear();
        s_DesignY.Clear();
        s_DesignW.Clear();
        s_DesignH.Clear();

        // Walk the tree starting from root (inclusive).
        // SorterPanel itself is pixel-based (720x590) and will be captured.
        // After Apply(), CenterPanel() reads the scaled size to re-center.
        CaptureRecursive(root);
        s_Captured = true;

        if (!s_LoggedOnce)
        {
            int capturedCount = s_Widgets.Count();
            string msg = "[UIScaler] Captured ";
            msg = msg + capturedCount.ToString();
            msg = msg + " pixel-based widgets";
            Print(msg);
        }
    }

    protected static void CaptureRecursive(Widget w)
    {
        if (!w)
            return;

        float posX = 0.0;
        float posY = 0.0;
        float sizeW = 0.0;
        float sizeH = 0.0;

        w.GetPos(posX, posY);
        w.GetSize(sizeW, sizeH);

        // Heuristic: if BOTH dimensions <= threshold → proportional → skip
        bool isProportional = false;
        if (sizeW <= PROP_THRESHOLD && sizeH <= PROP_THRESHOLD)
        {
            isProportional = true;
        }

        if (!isProportional)
        {
            s_Widgets.Insert(w);
            s_DesignX.Insert(posX);
            s_DesignY.Insert(posY);
            s_DesignW.Insert(sizeW);
            s_DesignH.Insert(sizeH);
        }

        // Recurse: children first, then siblings
        Widget child = w.GetChildren();
        if (child)
        {
            CaptureRecursive(child);
        }

        Widget sibling = w.GetSibling();
        if (sibling)
        {
            CaptureRecursive(sibling);
        }
    }

    // ─────────────────────────────────────────────
    // DetectLogicalSize: read the rendered size of a fullscreen
    // root widget (size 1 1, proportional) to get the available
    // space in widget coordinates. Must be called BEFORE ComputeScale.
    //
    // Why: GetScreenSize() (global) returns physical pixels (e.g. 3840x2160).
    // But SetPos/SetSize operate in logical pixels. At Windows DPI 150%,
    // logical = physical / 1.5 (e.g. 2560x1440). Without this correction,
    // scale=2.0 creates a panel that overflows the logical viewport.
    //
    // The root widget (SorterRoot, size 1 1) fills the entire viewport
    // in widget-space. Its GetScreenSize returns the actual usable area.
    // ─────────────────────────────────────────────
    static void DetectLogicalSize(Widget fullscreenRoot)
    {
        s_LogicalDetected = false;
        s_LogicalW = 0.0;
        s_LogicalH = 0.0;

        if (!fullscreenRoot)
            return;

        float rootW = 0.0;
        float rootH = 0.0;
        fullscreenRoot.GetScreenSize(rootW, rootH);

        // Widget.GetScreenSize on a proportional (size 1 1) root returns
        // the viewport in logical coordinates. Validate: must be > 100
        // (rules out fractional 0.0-1.0 values AND zero if engine hasn't
        // completed a layout pass yet after Show(true)) and < 20000 (sanity).
        // If validation fails, GetLogicalW/H fallback to global GetScreenSize.
        bool validW = false;
        bool validH = false;
        if (rootW > 100.0 && rootW < 20000.0)
        {
            validW = true;
        }
        if (rootH > 100.0 && rootH < 20000.0)
        {
            validH = true;
        }

        if (validW && validH)
        {
            s_LogicalW = rootW;
            s_LogicalH = rootH;
            s_LogicalDetected = true;
        }

        // Diagnostic log (once)
        if (!s_LoggedOnce)
        {
            int physW = 0;
            int physH = 0;
            GetScreenSize(physW, physH);
            string detectMsg = "[UIScaler] physical=";
            detectMsg = detectMsg + physW.ToString();
            detectMsg = detectMsg + "x";
            detectMsg = detectMsg + physH.ToString();
            detectMsg = detectMsg + " logical=";
            detectMsg = detectMsg + rootW.ToString();
            detectMsg = detectMsg + "x";
            detectMsg = detectMsg + rootH.ToString();
            detectMsg = detectMsg + " detected=";
            detectMsg = detectMsg + s_LogicalDetected.ToString();
            Print(detectMsg);
        }
    }

    // Getters for CenterPanel (SorterView)
    static float GetLogicalW()
    {
        if (s_LogicalDetected)
        {
            return s_LogicalW;
        }
        // Fallback: global GetScreenSize (works correctly at DPI 100%)
        int fw = 0;
        int fh = 0;
        GetScreenSize(fw, fh);
        return fw;
    }

    static float GetLogicalH()
    {
        if (s_LogicalDetected)
        {
            return s_LogicalH;
        }
        int fw = 0;
        int fh = 0;
        GetScreenSize(fw, fh);
        return fh;
    }

    // ─────────────────────────────────────────────
    // ComputeScale: derive factor from logical screen dimensions.
    // v3.2: Uses DetectLogicalSize result when available.
    // Falls back to global GetScreenSize if detection failed.
    // ─────────────────────────────────────────────
    static float ComputeScale()
    {
        float availW = GetLogicalW();
        float availH = GetLogicalH();

        float scaleW = availW / DESIGN_W;
        float scaleH = availH / DESIGN_H;

        // Use the smaller axis to guarantee the panel fits on screen
        float scale = scaleW;
        if (scaleH < scaleW)
        {
            scale = scaleH;
        }

        // Panel-fit clamp — ensure 720x590 * scale fits with margin
        float panelH = 590.0 * scale;
        float panelW = 720.0 * scale;
        float maxH = availH * 0.92;
        float maxW = availW * 0.92;
        if (panelH > maxH)
        {
            scale = maxH / 590.0;
        }
        if (panelW > maxW)
        {
            float altScale = maxW / 720.0;
            if (altScale < scale)
            {
                scale = altScale;
            }
        }

        // Clamp
        if (scale < SCALE_MIN)
        {
            scale = SCALE_MIN;
        }
        if (scale > SCALE_MAX)
        {
            scale = SCALE_MAX;
        }

        if (!s_LoggedOnce)
        {
            string scaleMsg = "[UIScaler] screen=";
            scaleMsg = scaleMsg + availW.ToString();
            scaleMsg = scaleMsg + "x";
            scaleMsg = scaleMsg + availH.ToString();
            scaleMsg = scaleMsg + " scale=";
            scaleMsg = scaleMsg + scale.ToString();
            Print(scaleMsg);
            s_LoggedOnce = true;
        }

        return scale;
    }

    // ─────────────────────────────────────────────
    // Apply: set all captured widgets to design × scale
    // Safe to call multiple times (reads from stored originals).
    // Per-axis: only scales dimensions whose DESIGN value > threshold.
    // This protects mixed widgets like TagsList (size 295 1) where
    // width is pixel (295) but height is proportional (1.0).
    // ─────────────────────────────────────────────
    static void Apply(float scale)
    {
        if (!s_Captured)
            return;
        if (!s_Widgets)
            return;

        int count = s_Widgets.Count();
        int i = 0;
        Widget w = null;
        float newX = 0.0;
        float newY = 0.0;
        float newW = 0.0;
        float newH = 0.0;

        for (i = 0; i < count; i = i + 1)
        {
            w = s_Widgets[i];
            if (!w)
                continue;

            // Per-axis: only scale the pixel-based component
            newX = s_DesignX[i];
            if (newX > PROP_THRESHOLD)
            {
                newX = newX * scale;
            }

            newY = s_DesignY[i];
            if (newY > PROP_THRESHOLD)
            {
                newY = newY * scale;
            }

            newW = s_DesignW[i];
            if (newW > PROP_THRESHOLD)
            {
                newW = newW * scale;
            }

            newH = s_DesignH[i];
            if (newH > PROP_THRESHOLD)
            {
                newH = newH * scale;
            }

            // Guard: sub-pixel design values (> 0 and < 1.0) should scale
            // but never go below 1px (invisible). Does NOT catch 1.0 to
            // avoid false positives on proportional values in mixed widgets.
            if (s_DesignW[i] > 0.0 && s_DesignW[i] < 1.0)
            {
                newW = s_DesignW[i] * scale;
                if (newW < 1.0)
                {
                    newW = 1.0;
                }
            }
            if (s_DesignH[i] > 0.0 && s_DesignH[i] < 1.0)
            {
                newH = s_DesignH[i] * scale;
                if (newH < 1.0)
                {
                    newH = 1.0;
                }
            }

            w.SetPos(newX, newY);
            w.SetSize(newW, newH);
        }
    }

    // ─────────────────────────────────────────────
    // Reset: free arrays (call on mission finish / cleanup)
    // ─────────────────────────────────────────────
    static void Reset()
    {
        if (s_Widgets)
        {
            s_Widgets.Clear();
        }
        if (s_DesignX)
        {
            s_DesignX.Clear();
        }
        if (s_DesignY)
        {
            s_DesignY.Clear();
        }
        if (s_DesignW)
        {
            s_DesignW.Clear();
        }
        if (s_DesignH)
        {
            s_DesignH.Clear();
        }
        s_Captured = false;
        s_LoggedOnce = false;
        s_LogicalDetected = false;
        s_LogicalW = 0.0;
        s_LogicalH = 0.0;

        s_Widgets = null;
        s_DesignX = null;
        s_DesignY = null;
        s_DesignW = null;
        s_DesignH = null;
    }

    // ─────────────────────────────────────────────
    // ScaleWidget: scale a single widget tree (for dynamic items
    // created after Capture, e.g. tag chips, preview rows).
    // Per-axis: only scales dimensions > threshold (pixel-based).
    // Reads current values and multiplies — caller must ensure
    // this is called only ONCE per widget instance.
    // ─────────────────────────────────────────────
    static void ScaleWidget(Widget w, float scale)
    {
        if (!w)
            return;

        // At scale 1.0, skip entirely (no-op)
        if (scale > 0.999 && scale < 1.001)
            return;

        float posX = 0.0;
        float posY = 0.0;
        float sizeW = 0.0;
        float sizeH = 0.0;
        float newX = 0.0;
        float newY = 0.0;
        float newW = 0.0;
        float newH = 0.0;
        bool hasPixel = false;

        w.GetPos(posX, posY);
        w.GetSize(sizeW, sizeH);

        // Check if at least one size dimension is pixel-based
        if (sizeW > PROP_THRESHOLD || sizeH > PROP_THRESHOLD)
        {
            hasPixel = true;
        }

        if (hasPixel)
        {
            // Per-axis: only scale dimensions > threshold
            newX = posX;
            if (posX > PROP_THRESHOLD)
            {
                newX = posX * scale;
            }

            newY = posY;
            if (posY > PROP_THRESHOLD)
            {
                newY = posY * scale;
            }

            newW = sizeW;
            if (sizeW > PROP_THRESHOLD)
            {
                newW = sizeW * scale;
            }

            newH = sizeH;
            if (sizeH > PROP_THRESHOLD)
            {
                newH = sizeH * scale;
            }

            // Guard: sub-pixel design values (> 0 and < 1.0)
            if (sizeW > 0.0 && sizeW < 1.0)
            {
                newW = sizeW * scale;
                if (newW < 1.0)
                {
                    newW = 1.0;
                }
            }
            if (sizeH > 0.0 && sizeH < 1.0)
            {
                newH = sizeH * scale;
                if (newH < 1.0)
                {
                    newH = 1.0;
                }
            }

            w.SetPos(newX, newY);
            w.SetSize(newW, newH);
        }

        // Recurse into children
        Widget child = w.GetChildren();
        while (child)
        {
            ScaleWidget(child, scale);
            child = child.GetSibling();
        }
    }
};
