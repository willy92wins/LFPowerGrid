// =========================================================
// LF_PowerGrid - Tank HUD for Water Pump T2 (v1.1.0 Sprint W3)
//
// Client-side HUD showing tank level when looking at T2 pump.
// Uses LFPG_ActionRaycast for cursor target (shared with DeviceInspector).
// =========================================================

class LFPG_TankHUD
{
    // Singleton
    protected static ref LFPG_TankHUD s_Instance;

    // Widgets
    protected Widget m_Root;
    protected ImageWidget m_BarBg;
    protected ImageWidget m_BarFill;
    protected TextWidget m_TankText;
    protected TextWidget m_StatusText;

    // State (delta check to avoid SetText spam)
    protected float m_LastLevel = -1.0;
    protected int m_LastLiquidType = -1;
    protected bool m_LastPowered = false;
    protected bool m_Visible = false;

    static void Init()
    {
        if (!s_Instance)
        {
            s_Instance = new LFPG_TankHUD;
        }
        s_Instance.CreateWidgets();
    }

    static LFPG_TankHUD Get()
    {
        return s_Instance;
    }

    static void Cleanup()
    {
        if (s_Instance)
        {
            s_Instance.DestroyWidgets();
        }
        s_Instance = null;
    }

    protected void CreateWidgets()
    {
        #ifndef SERVER
        if (m_Root)
            return;

        m_Root = GetGame().GetWorkspace().CreateWidgets("LFPowerGrid/gui/layouts/LFPG_TankHUD.layout");
        if (!m_Root)
        {
            LFPG_Util.Error("[TankHUD] Failed to create layout");
            return;
        }

        m_BarBg = ImageWidget.Cast(m_Root.FindAnyWidget("BarBg"));
        m_BarFill = ImageWidget.Cast(m_Root.FindAnyWidget("BarFill"));
        m_TankText = TextWidget.Cast(m_Root.FindAnyWidget("TankText"));
        m_StatusText = TextWidget.Cast(m_Root.FindAnyWidget("StatusText"));

        m_Root.Show(false);
        m_Visible = false;
        #endif
    }

    protected void DestroyWidgets()
    {
        #ifndef SERVER
        if (m_Root)
        {
            m_Root.Unlink();
            m_Root = null;
        }
        #endif
    }

    void Tick()
    {
        #ifndef SERVER
        if (!m_Root)
            return;

        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player)
        {
            Hide();
            return;
        }

        // Reuse ActionRaycast cursor target (shared with DeviceInspector)
        EntityAI target = LFPG_ActionRaycast.GetCursorTargetDevice(player);
        if (!target)
        {
            Hide();
            return;
        }

        // Only show for T2
        LF_WaterPump_T2 t2 = LF_WaterPump_T2.Cast(target);
        if (!t2)
        {
            Hide();
            return;
        }

        // Read SyncVars (client-side, no RPC cost)
        float level = t2.LFPG_GetTankLevel();
        int liquidType = t2.LFPG_GetTankLiquidType();
        bool powered = t2.LFPG_GetPoweredNet();

        UpdateBar(level, liquidType, powered);
        Show();
        #endif
    }

    protected void UpdateBar(float level, int liquidType, bool powered)
    {
        #ifndef SERVER
        // Delta check — skip if nothing changed
        float levelDelta = level - m_LastLevel;
        if (levelDelta < 0.0)
        {
            levelDelta = -levelDelta;
        }

        bool changed = false;
        if (levelDelta > 0.05)
        {
            changed = true;
        }
        if (liquidType != m_LastLiquidType)
        {
            changed = true;
        }
        if (powered != m_LastPowered)
        {
            changed = true;
        }
        if (!m_Visible)
        {
            changed = true;
        }

        if (!changed)
            return;

        m_LastLevel = level;
        m_LastLiquidType = liquidType;
        m_LastPowered = powered;

        // Bar fill width (proportional to tank level)
        float fillPct = 0.0;
        if (LFPG_PUMP_TANK_MAX > 0.0)
        {
            fillPct = level / LFPG_PUMP_TANK_MAX;
        }
        if (fillPct < 0.0)
        {
            fillPct = 0.0;
        }
        if (fillPct > 1.0)
        {
            fillPct = 1.0;
        }

        // BarFill uses proportional sizing: sizeX = 0.90 * fillPct
        float barMaxW = 0.90;
        float fillW = barMaxW * fillPct;

        if (m_BarFill)
        {
            m_BarFill.SetSize(fillW, 0.30);

            // Color based on liquid type
            int r = 85;
            int g = 85;
            int b = 85;
            int a = 220;

            if (level < 0.01)
            {
                r = 85; g = 85; b = 85; a = 180; // grey - empty
            }
            else if (liquidType == LIQUID_CLEANWATER)
            {
                r = 51; g = 153; b = 255; a = 220; // blue - clean
            }
            else
            {
                r = 136; g = 170; b = 68; a = 220; // green-brown - river
            }

            m_BarFill.SetColor(ARGB(a, r, g, b));
        }

        // Tank text
        if (m_TankText)
        {
            int levelInt = level;
            int pctInt = fillPct * 100.0;
            string tankMax = LFPG_PUMP_TANK_MAX.ToString();
            string levelStr = levelInt.ToString();
            string pctStr = pctInt.ToString();
            string txt = levelStr + "L / " + tankMax + "L  (" + pctStr + "%)";
            m_TankText.SetText(txt);
        }

        // Status text
        if (m_StatusText)
        {
            if (powered)
            {
                m_StatusText.SetText("POWERED");
                m_StatusText.SetColor(ARGB(220, 50, 220, 50));
            }
            else if (level > 0.01)
            {
                m_StatusText.SetText("OFFLINE - Tank");
                m_StatusText.SetColor(ARGB(220, 220, 200, 50));
            }
            else
            {
                m_StatusText.SetText("EMPTY");
                m_StatusText.SetColor(ARGB(220, 220, 50, 50));
            }
        }
        #endif
    }

    protected void Show()
    {
        #ifndef SERVER
        if (!m_Visible && m_Root)
        {
            m_Root.Show(true);
            m_Visible = true;
        }
        #endif
    }

    protected void Hide()
    {
        #ifndef SERVER
        if (m_Visible && m_Root)
        {
            m_Root.Show(false);
            m_Visible = false;
            m_LastLevel = -1.0;
            m_LastLiquidType = -1;
        }
        #endif
    }
};
