#ifndef SERVER
// Client-only compilation boundary
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
    protected Widget m_BarBg;
    protected Widget m_BarFill;
    protected TextWidget m_TankText;
    protected TextWidget m_StatusText;

    // State (delta check to avoid SetText spam)
    protected float m_LastLevel = -1.0;
    protected int m_LastLiquidType = -1;
    protected bool m_LastPowered = false;
    protected bool m_Visible = false;

    // Cheap eligibility gate: a throttled proximity query with reusable buffers.
    protected ref array<Object> m_NearbyObjects;
    protected ref array<CargoBase> m_NearbyCargo;
    protected float m_LastNearPumpCheckMs = -1.0;
    protected bool m_NearT2Pump = false;
    static const float NEAR_PUMP_CHECK_MS = 250.0;
	static const float NEGATIVE_PUMP_CHECK_MS = 1000.0;
	protected vector m_LastNearPumpPosition;
	protected PlayerBase m_LastNearPumpPlayer;

    void LFPG_TankHUD()
    {
        m_NearbyObjects = new array<Object>;
        m_NearbyCargo = new array<CargoBase>;
    }

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

        if (!g_Game)
            return;

        WorkspaceWidget workspace = g_Game.GetWorkspace();
        if (!workspace)
        {
            LFPG_Util.Error("[TankHUD] Workspace unavailable");
            return;
        }

        m_Root = workspace.CreateWidgets("LFPowerGrid/gui/layouts/LFPG_TankHUD.layout");
        if (!m_Root)
        {
            LFPG_Util.Error("[TankHUD] Failed to create layout");
            return;
        }

        m_BarBg = m_Root.FindAnyWidget("BarBg");
        m_BarFill = m_Root.FindAnyWidget("BarFill");
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

        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player)
        {
            Hide();
            return;
        }

        // Do not wake the shared cursor ray unless a T2 pump is nearby.
        if (!CanQueryTank(player))
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
        LFPG_WaterPump_T2 t2 = LFPG_WaterPump_T2.Cast(target);
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

    protected bool CanQueryTank(PlayerBase player)
    {
        if (!player || !m_NearbyObjects || !m_NearbyCargo)
            return false;

        float nowMs = g_Game.GetTime();
        float elapsed = nowMs - m_LastNearPumpCheckMs;
		vector playerPos = player.GetPosition();
		if (player == m_LastNearPumpPlayer && m_LastNearPumpCheckMs >= 0.0 && elapsed >= 0.0)
		{
			if (elapsed < NEAR_PUMP_CHECK_MS)
				return m_NearT2Pump;
			// The query has 1.5m padding: recheck after moving 1m, or at 1s
			// while stationary so newly streamed/placed pumps are still discovered.
			vector moved = playerPos - m_LastNearPumpPosition;
			float movedSq = moved[0] * moved[0] + moved[1] * moved[1] + moved[2] * moved[2];
			if (!m_NearT2Pump && elapsed < NEGATIVE_PUMP_CHECK_MS && movedSq < 1.0)
				return false;
		}

		m_LastNearPumpPosition = playerPos;
		m_LastNearPumpPlayer = player;
        m_LastNearPumpCheckMs = nowMs;
        m_NearT2Pump = false;
        m_NearbyObjects.Clear();
        m_NearbyCargo.Clear();
        float queryRadius = LFPG_INTERACT_DIST_M + 1.5;
		g_Game.GetObjectsAtPosition3D(playerPos, queryRadius, m_NearbyObjects, m_NearbyCargo);

        int i;
        for (i = 0; i < m_NearbyObjects.Count(); i = i + 1)
        {
            LFPG_WaterPump_T2 nearbyPump = LFPG_WaterPump_T2.Cast(m_NearbyObjects[i]);
            if (nearbyPump)
            {
                m_NearT2Pump = true;
                break;
            }
        }
        return m_NearT2Pump;
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
#endif
