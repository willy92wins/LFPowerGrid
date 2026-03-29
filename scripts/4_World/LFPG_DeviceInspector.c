// =========================================================
// LF_PowerGrid - Device Inspector (v0.8.0, Sprint 5 S3)
//
// Client-side floating panel that shows device info when the
// player holds a cable reel and looks at an electrical device
// WITHOUT an active wiring session.
//
// Architecture:
//   - Per-frame Tick() detects held reel + cursor on device
//   - Immediate client-side data (SyncVars via DeviceAPI)
//   - RPC enrichment for wire topology (server has wire arrays)
//   - .layout widget tree for proper text rendering
//   - Screen-space floating position (projected from device)
//
// Integration:
//   Call LFPG_DeviceInspector.Init() once during MissionGameplay init.
//   Call LFPG_DeviceInspector.Tick() every frame (client-side).
//   Call LFPG_DeviceInspector.Cleanup() on mission finish.
//   Call LFPG_DeviceInspector.OnInspectResponse() from PlayerRPC
//   when INSPECT_RESPONSE arrives.
//
// Layer: 4_World (needs LFPG_DeviceAPI, LFPG_WiringClient,
//        LFPG_ActionRaycast, LFPG_DeviceRegistry).
// =========================================================

class LFPG_InspectWireEntry
{
    int m_Direction;           // LFPG_PortDir.IN or OUT
    string m_LocalPort;        // port name on inspected device
    string m_RemoteDeviceId;   // target/source device ID
    string m_RemotePort;       // port name on remote device
    string m_RemoteTypeName;   // entity type name for display

    // v0.7.47: Per-wire power data for inspector display
    float m_AllocatedPower;    // Power flowing through this edge (u/s)
    int m_EdgeState;           // v1.0: 0=OK, 2=OVERLOADED (all-off)

    void LFPG_InspectWireEntry()
    {
        m_Direction = -1;
        m_LocalPort = "";
        m_RemoteDeviceId = "";
        m_RemotePort = "";
        m_RemoteTypeName = "";
        m_AllocatedPower = 0.0;
        m_EdgeState = 0;
    }
};

class LFPG_DeviceInspector
{
    // ---- Singleton ----
    protected static ref LFPG_DeviceInspector s_Instance;

    // ---- Widget references ----
    protected Widget m_Root;
    protected Widget m_Panel;
    protected ImageWidget m_wPanelBg;
    protected ImageWidget m_wAccentBar;
    protected ImageWidget m_wSeparator;
    protected TextWidget m_wDeviceName;
    protected TextWidget m_wDeviceType;
    protected TextWidget m_wStatusLine;
    protected TextWidget m_wCapLine;
    protected TextWidget m_wTankLine;
    protected TextWidget m_wFuelLine;
    protected TextWidget m_wReserveLine;
    protected TextWidget m_wWiresHeader;
    protected ref array<TextWidget> m_wWireSlots;

    // ---- State ----
    protected bool m_Visible;
    protected string m_CurrentDeviceId;
    protected float m_LastRPCSendMs;
    protected bool m_HasServerData;
    protected int m_VisibleWireCount;
    // H1 fix: periodic client-side SyncVar refresh
    protected float m_LastClientRefreshMs;
    // v1.1.0: Tank line offset for T2 pumps
    protected float m_TankLineOffset;
    // v1.2.0: Fuel line offset for Furnace
    protected float m_FuelLineOffset;
    // v1.2.2: Reserve line offset for Furnace cargo
    protected float m_ReserveLineOffset;
    // v2.4: Link line offset for Sorter
    protected TextWidget m_wLinkLine;
    protected float m_LinkLineOffset;
    // v2.1: Battery line for charge display
    protected TextWidget m_wBatteryLine;
    protected float m_BatteryLineOffset;

    // ---- Position smoothing (P1-A anti-jitter) ----
    protected float m_SmoothX;
    protected float m_SmoothY;
    protected bool m_SmoothInit;
    // ---- Flip hysteresis (P2-B anti-oscillation) ----
    protected bool m_FlippedLeft;

    // ---- Current panel height (for accurate screen clamping) ----
    protected float m_CurrentPanelH;

    // ---- Server response cache ----
    protected ref array<ref LFPG_InspectWireEntry> m_RespWires;

    // ---- Layout path (adjust to match your PBO structure) ----
    static const string LAYOUT_PATH = "LFPowerGrid/gui/layouts/LFPG_DeviceInspector.layout";

    // =========================================================
    // Singleton access
    // =========================================================
    static LFPG_DeviceInspector Get()
    {
        if (!s_Instance)
        {
            s_Instance = new LFPG_DeviceInspector();
        }
        return s_Instance;
    }

    // =========================================================
    // Lifecycle
    // =========================================================
    static void Init()
    {
        LFPG_DeviceInspector inst = Get();
        inst.CreateWidgets();
        LFPG_Util.Info("[DeviceInspector] Initialized");
    }

    static void Cleanup()
    {
        if (s_Instance)
        {
            s_Instance.DestroyWidgets();
            delete s_Instance;
            s_Instance = null;
        }
    }

    // v1.3.1: Force-hide panel when CCTV viewport is active.
    // MissionInit skips Tick() during CCTV, but if the panel was
    // already visible when the viewport activated, stopping Tick()
    // just freezes it on screen. This actively hides it.
    static void ForceHide()
    {
        if (!s_Instance)
            return;

        if (!s_Instance.m_Visible)
            return;

        s_Instance.HidePanel();
    }

    void LFPG_DeviceInspector()
    {
        m_wWireSlots = new array<TextWidget>;
        m_RespWires = new array<ref LFPG_InspectWireEntry>;
        m_Visible = false;
        m_CurrentDeviceId = "";
        m_LastRPCSendMs = 0.0;
        m_HasServerData = false;
        m_VisibleWireCount = 0;
        m_LastClientRefreshMs = 0.0;
        m_SmoothX = 0.0;
        m_SmoothY = 0.0;
        m_SmoothInit = false;
        m_FlippedLeft = false;
        m_CurrentPanelH = 0.0;
    }

    // =========================================================
    // Widget creation (called once at init)
    // =========================================================
    protected void CreateWidgets()
    {
        if (m_Root)
            return;

        m_Root = GetGame().GetWorkspace().CreateWidgets(LAYOUT_PATH);
        if (!m_Root)
        {
            LFPG_Util.Error("[DeviceInspector] Failed to create widgets from: " + LAYOUT_PATH);
            return;
        }

        // v0.7.42 (BugFix): Inspector must render above CableHUD canvas
        // (which uses SetSort(10000)). Without this, cable lines are
        // drawn on top of the inspector panel.
        m_Root.SetSort(10001);

        m_Panel = m_Root.FindAnyWidget("InspectorPanel");
        m_wDeviceName = TextWidget.Cast(m_Root.FindAnyWidget("DeviceName"));
        m_wDeviceType = TextWidget.Cast(m_Root.FindAnyWidget("DeviceType"));
        m_wStatusLine = TextWidget.Cast(m_Root.FindAnyWidget("StatusLine"));
        m_wCapLine = TextWidget.Cast(m_Root.FindAnyWidget("CapLine"));
        m_wTankLine = TextWidget.Cast(m_Root.FindAnyWidget("TankLine"));
        m_wFuelLine = TextWidget.Cast(m_Root.FindAnyWidget("FuelLine"));
        m_wReserveLine = TextWidget.Cast(m_Root.FindAnyWidget("ReserveLine"));
        m_wLinkLine = TextWidget.Cast(m_Root.FindAnyWidget("LinkLine"));
        m_wBatteryLine = TextWidget.Cast(m_Root.FindAnyWidget("BatteryLine"));
        m_wWiresHeader = TextWidget.Cast(m_Root.FindAnyWidget("WiresHeader"));

        // ---- Force geometry from code (layout pos/size unreliable in FrameWidgetClass) ----
        // Compute max panel height for initial sizing (will be adjusted by ResizePanelHeight)
        float maxH = ComputePanelHeight(LFPG_INSPECT_MAX_WIRES);
        m_CurrentPanelH = maxH;

        // Panel container
        if (m_Panel)
        {
            m_Panel.SetPos(0, 0);
            m_Panel.SetSize(LFPG_INSPECT_PANEL_W, maxH);
        }

        // Background images: position + size + texture + color
        string procTex = "#(argb,8,8,3)color(1,1,1,1,CO)";

        ImageWidget imgBg = ImageWidget.Cast(m_Root.FindAnyWidget("PanelBg"));
        m_wPanelBg = imgBg;
        if (imgBg)
        {
            imgBg.SetPos(0, 0);
            imgBg.SetSize(LFPG_INSPECT_PANEL_W, maxH);
            imgBg.LoadImageFile(0, procTex);
            imgBg.SetColor(ARGB(235, 9, 14, 23));
        }

        ImageWidget imgHeader = ImageWidget.Cast(m_Root.FindAnyWidget("HeaderBar"));
        if (imgHeader)
        {
            imgHeader.SetPos(0, 0);
            imgHeader.SetSize(LFPG_INSPECT_PANEL_W, LFPG_INSPECT_HEADER_H);
            imgHeader.LoadImageFile(0, procTex);
            imgHeader.SetColor(ARGB(242, 13, 19, 31));
        }

        ImageWidget imgAccent = ImageWidget.Cast(m_Root.FindAnyWidget("AccentBar"));
        m_wAccentBar = imgAccent;
        if (imgAccent)
        {
            imgAccent.SetPos(0, 0);
            imgAccent.SetSize(LFPG_INSPECT_ACCENT_W, maxH);
            imgAccent.LoadImageFile(0, procTex);
            imgAccent.SetColor(ARGB(217, 46, 140, 191));
        }

        ImageWidget imgSep = ImageWidget.Cast(m_Root.FindAnyWidget("Separator"));
        m_wSeparator = imgSep;
        if (imgSep)
        {
            imgSep.SetPos(12, 93);
            imgSep.SetSize(276, 1);
            imgSep.LoadImageFile(0, procTex);
            imgSep.SetColor(ARGB(153, 51, 64, 89));
        }

        // Text widgets: position + size + color
        if (m_wDeviceName)
        {
            m_wDeviceName.SetPos(14, 7);
            m_wDeviceName.SetSize(274, 22);
            m_wDeviceName.SetColor(ARGB(255, 242, 242, 242));
        }
        if (m_wDeviceType)
        {
            m_wDeviceType.SetPos(14, 30);
            m_wDeviceType.SetSize(274, 16);
        }
        if (m_wStatusLine)
        {
            m_wStatusLine.SetPos(14, 54);
            m_wStatusLine.SetSize(274, 16);
        }
        if (m_wCapLine)
        {
            m_wCapLine.SetPos(14, 74);
            m_wCapLine.SetSize(274, 16);
            m_wCapLine.SetColor(ARGB(255, 140, 140, 140));
        }
        if (m_wTankLine)
        {
            m_wTankLine.SetPos(14, 94);
            m_wTankLine.SetSize(274, 16);
            m_wTankLine.SetColor(ARGB(255, 51, 153, 255));
            m_wTankLine.Show(false);
        }
        m_TankLineOffset = 0.0;
        // v1.2.0: FuelLine (same Y as TankLine — mutually exclusive)
        if (m_wFuelLine)
        {
            m_wFuelLine.SetPos(14, 94);
            m_wFuelLine.SetSize(274, 16);
            m_wFuelLine.SetColor(ARGB(255, 230, 126, 34));
            m_wFuelLine.Show(false);
        }
        m_FuelLineOffset = 0.0;
        // v1.2.2: ReserveLine (below FuelLine, furnace cargo reserve)
        if (m_wReserveLine)
        {
            m_wReserveLine.SetPos(14, 114);
            m_wReserveLine.SetSize(274, 16);
            m_wReserveLine.SetColor(ARGB(255, 230, 126, 34));
            m_wReserveLine.Show(false);
        }
        m_ReserveLineOffset = 0.0;
        // v2.4: LinkLine (Sorter container link status)
        if (m_wLinkLine)
        {
            m_wLinkLine.SetPos(14, 94);
            m_wLinkLine.SetSize(274, 16);
            m_wLinkLine.SetColor(ARGB(255, 52, 211, 153));
            m_wLinkLine.Show(false);
        }
        m_LinkLineOffset = 0.0;
        // v2.1: BatteryLine (charge level + rate for batteries)
        if (m_wBatteryLine)
        {
            m_wBatteryLine.SetPos(14, 94);
            m_wBatteryLine.SetSize(360, 18);
            m_wBatteryLine.SetColor(ARGB(255, 255, 200, 50));
            m_wBatteryLine.Show(false);
        }
        m_BatteryLineOffset = 0.0;
        if (m_wWiresHeader)
        {
            m_wWiresHeader.SetPos(14, 99);
            m_wWiresHeader.SetSize(274, 16);
            m_wWiresHeader.SetColor(ARGB(255, 180, 180, 180));
        }

        // Wire slot widgets: position + size
        m_wWireSlots.Clear();
        int wi;
        for (wi = 0; wi < LFPG_INSPECT_MAX_WIRES; wi = wi + 1)
        {
            string slotName = "Wire";
            slotName = slotName + wi.ToString();
            TextWidget tw = TextWidget.Cast(m_Root.FindAnyWidget(slotName));
            if (tw)
            {
                float wireY = LFPG_INSPECT_PANEL_BASE_H + 2.0 + (wi * LFPG_INSPECT_WIRE_ROW_H);
                tw.SetPos(14, wireY);
                tw.SetSize(274, 14);
                m_wWireSlots.Insert(tw);
            }
            else
            {
                LFPG_Util.Warn("[DeviceInspector] Missing widget: " + slotName);
            }
        }

        // Start hidden
        m_Root.Show(false);
        m_Visible = false;

        LFPG_Util.Info("[DeviceInspector] Widgets created, wireSlots=" + m_wWireSlots.Count().ToString());
    }

    protected void DestroyWidgets()
    {
        if (m_Root)
        {
            m_Root.Unlink();
            m_Root = null;
        }
        m_Panel = null;
        m_wPanelBg = null;
        m_wAccentBar = null;
        m_wSeparator = null;
        m_wDeviceName = null;
        m_wDeviceType = null;
        m_wStatusLine = null;
        m_wCapLine = null;
        m_wTankLine = null;
        m_wFuelLine = null;
        m_wReserveLine = null;
        m_wLinkLine = null;
        m_wBatteryLine = null;
        m_wWiresHeader = null;
        m_wWireSlots.Clear();
        m_RespWires.Clear();
    }

    // =========================================================
    // Per-frame tick (client only)
    // =========================================================
    static void Tick()
    {
        if (GetGame().IsDedicatedServer())
            return;

        LFPG_DeviceInspector inst = Get();
        if (!inst.m_Root)
            return;

        // ---- Condition 1: Player exists ----
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player)
        {
            inst.HidePanel();
            return;
        }

        // ---- Condition 2: Holding cable reel ----
        if (!IsHoldingCableReel(player))
        {
            inst.HidePanel();
            return;
        }

        // ---- Condition 3: No active wiring session ----
        LFPG_WiringClient wc = LFPG_WiringClient.Get();
        if (wc && wc.IsActive())
        {
            inst.HidePanel();
            return;
        }

        // ---- Condition 4: Raycast to device under cursor (with proximity fallback) ----
        EntityAI target = LFPG_ActionRaycast.GetCursorTargetDeviceWithProximity(player);
        if (!target)
        {
            inst.HidePanel();
            return;
        }

        // ---- Condition 5: Device has valid ID ----
        string deviceId = LFPG_DeviceAPI.GetDeviceId(target);
        if (deviceId == "")
        {
            inst.HidePanel();
            return;
        }

        float nowMs = GetGame().GetTime();

        // ---- New device? Full populate + request RPC ----
        if (deviceId != inst.m_CurrentDeviceId)
        {
            inst.m_CurrentDeviceId = deviceId;
            inst.m_HasServerData = false;
            inst.m_RespWires.Clear();
            inst.m_SmoothInit = false;
            inst.m_FlippedLeft = false;
            inst.PopulateClientData(target, deviceId);
            inst.RequestServerData(player, deviceId, target);
            inst.m_LastClientRefreshMs = nowMs;
        }
        else
        {
            // H1 fix: Periodic SyncVar refresh while looking at same device.
            // Catches state changes (gen on/off, load ratio updates) without
            // requiring the player to look away and back.
            // 500ms = 2Hz refresh, negligible cost (DeviceAPI calls are cached SyncVars).
            float sinceLast = nowMs - inst.m_LastClientRefreshMs;
            if (sinceLast >= LFPG_INSPECT_REFRESH_MS)
            {
                inst.PopulateClientData(target, deviceId);
                inst.m_LastClientRefreshMs = nowMs;
            }

            // H2 fix: Retry RPC if cooldown blocked the initial request.
            // Without this, rapidly switching devices leaves panel stuck on
            // "Connections ..." because the cooldown guard returns early.
            if (!inst.m_HasServerData)
            {
                inst.RequestServerData(player, deviceId, target);
            }
        }

        // ---- Update floating position every frame ----
        bool posValid = inst.UpdatePanelPosition(target);
        if (posValid)
        {
            inst.ShowPanel();
        }
        else
        {
            // Behind camera or invalid — hide widget but KEEP state.
            // When device returns to view, same deviceId is recognized
            // and no fresh RPC is needed.
            if (inst.m_Visible && inst.m_Root)
            {
                inst.m_Root.Show(false);
                inst.m_Visible = false;
            }
        }
    }

    // =========================================================
    // Cable reel detection
    // =========================================================
    protected static bool IsHoldingCableReel(PlayerBase player)
    {
        if (!player)
            return false;

        HumanInventory hinv = player.GetHumanInventory();
        if (!hinv)
            return false;

        EntityAI item = hinv.GetEntityInHands();
        if (!item)
            return false;

        return item.IsKindOf(LFPG_CABLE_REEL_TYPE);
    }

    // =========================================================
    // Populate panel with client-side data (instant, no RPC)
    // =========================================================
    protected void PopulateClientData(EntityAI device, string deviceId)
    {
        if (!m_wDeviceName || !m_wDeviceType || !m_wStatusLine || !m_wCapLine || !m_wWiresHeader)
            return;

        // ---- Device name (entity type, cleaned up) ----
        string typeName = device.GetType();
        m_wDeviceName.SetText(FormatDeviceName(typeName));

        // ---- Device type badge ----
        int devType = LFPG_DeviceAPI.GetDeviceType(device);
        string typeStr = Loc("#STR_LFPG_INSPECT_UNKNOWN");
        int typeColor = ARGB(255, 140, 140, 140);

        if (devType == LFPG_DeviceType.SOURCE)
        {
            typeStr = Loc("#STR_LFPG_INSPECT_SOURCE");
            typeColor = ARGB(255, 230, 180, 50);
        }
        else if (devType == LFPG_DeviceType.CONSUMER)
        {
            typeStr = Loc("#STR_LFPG_INSPECT_CONSUMER");
            typeColor = ARGB(255, 100, 180, 220);
        }
        else if (devType == LFPG_DeviceType.PASSTHROUGH)
        {
            typeStr = Loc("#STR_LFPG_INSPECT_PASSTHROUGH");
            typeColor = ARGB(255, 160, 120, 220);
        }
        else if (devType == LFPG_DeviceType.CAMERA)
        {
            typeStr = Loc("#STR_LFPG_INSPECT_CAMERA");
            typeColor = ARGB(255, 100, 180, 220);
        }

        m_wDeviceType.SetText(typeStr);
        m_wDeviceType.SetColor(typeColor);

        // ---- Power status ----
        string statusText = "";
        int statusColor = ARGB(255, 140, 140, 140);

        if (devType == LFPG_DeviceType.SOURCE)
        {
            bool sourceOn = LFPG_DeviceAPI.GetSourceOn(device);
            if (sourceOn)
            {
                float loadRatio = LFPG_DeviceAPI.GetLoadRatio(device);
                int loadPct = Math.Round(loadRatio * 100.0);

                if (loadRatio >= LFPG_LOAD_CRITICAL_THRESHOLD)
                {
                    statusText = Loc("#STR_LFPG_INSPECT_OVERLOAD");
                    statusText = statusText + "  ";
                    statusColor = ARGB(255, 220, 50, 50);
                }
                else
                {
                    statusText = Loc("#STR_LFPG_INSPECT_ACTIVE");
                    statusText = statusText + "  ";
                    statusColor = ARGB(255, 46, 155, 89);
                }

                string barStr = BuildLoadBar(loadRatio);
                statusText = statusText + barStr;
                statusText = statusText + " ";
                statusText = statusText + loadPct.ToString();
                statusText = statusText + "%";
            }
            else
            {
                statusText = Loc("#STR_LFPG_INSPECT_INACTIVE");
                statusColor = ARGB(255, 120, 120, 120);
            }
        }
        else if (devType == LFPG_DeviceType.PASSTHROUGH)
        {
            // v0.7.47: PASSTHROUGH shows Transmitting / Not Transmitting
            bool ptPowered = LFPG_DeviceAPI.GetPowered(device);
            if (ptPowered)
            {
                statusText = Loc("#STR_LFPG_INSPECT_TRANSMITTING");
                statusColor = ARGB(255, 46, 155, 89);
            }
            else
            {
                statusText = Loc("#STR_LFPG_INSPECT_NOT_TRANSMITTING");
                statusColor = ARGB(255, 120, 120, 120);
            }
        }
        else
        {
            bool powered = LFPG_DeviceAPI.GetPowered(device);
            if (powered)
            {
                statusText = Loc("#STR_LFPG_INSPECT_POWERED");
                statusColor = ARGB(255, 46, 155, 89);
            }
            else
            {
                statusText = Loc("#STR_LFPG_INSPECT_UNPOWERED");
                statusColor = ARGB(255, 120, 120, 120);
            }
        }

        m_wStatusLine.SetText(statusText);
        m_wStatusLine.SetColor(statusColor);

        // ---- Capacity / consumption line ----
        string capText = "";
        if (devType == LFPG_DeviceType.SOURCE)
        {
            float cap = LFPG_DeviceAPI.GetCapacity(device);
            capText = Loc("#STR_LFPG_INSPECT_CAPACITY");
            capText = capText + FormatFloat1(cap);
            capText = capText + " u/s";
        }
        else if (devType == LFPG_DeviceType.CONSUMER)
        {
            float cons = LFPG_DeviceAPI.GetConsumption(device);
            capText = Loc("#STR_LFPG_INSPECT_CONSUMPTION");
            capText = capText + FormatFloat1(cons);
            capText = capText + " u/s";
        }
        else if (devType == LFPG_DeviceType.CAMERA)
        {
            float camCons = LFPG_DeviceAPI.GetConsumption(device);
            capText = Loc("#STR_LFPG_INSPECT_CONSUMPTION");
            capText = capText + FormatFloat1(camCons);
            capText = capText + " u/s";
        }
        else if (devType == LFPG_DeviceType.PASSTHROUGH)
        {
            float ptCap = LFPG_DeviceAPI.GetCapacity(device);
            capText = Loc("#STR_LFPG_INSPECT_THROUGHPUT");
            capText = capText + FormatFloat1(ptCap);
            capText = capText + " u/s";

            // v0.8.1: Show "Total Load" = own consumption + sum of outgoing wire demands.
            // Own consumption is 0 for splitter, but future passthroughs (motorized switch)
            // may have self-draw. Outgoing wire data comes from RPC (m_RespWires).
            float ptOwnCons = LFPG_DeviceAPI.GetConsumption(device);
            float ptDownstream = 0.0;
            if (m_HasServerData && m_RespWires)
            {
                int wdi;
                for (wdi = 0; wdi < m_RespWires.Count(); wdi = wdi + 1)
                {
                    LFPG_InspectWireEntry wde = m_RespWires[wdi];
                    if (!wde)
                        continue;
                    // Only sum outgoing (OUT) wires — those are the downstream demands
                    if (wde.m_Direction == LFPG_PortDir.OUT)
                    {
                        ptDownstream = ptDownstream + wde.m_AllocatedPower;
                    }
                }
            }
            float ptTotalLoad = ptOwnCons + ptDownstream;

            capText = capText + "  |  ";
            capText = capText + Loc("#STR_LFPG_INSPECT_TOTAL_LOAD");
            capText = capText + FormatFloat1(ptTotalLoad);
            capText = capText + " u/s";
        }
        m_wCapLine.SetText(capText);
        if (capText != "")
        {
            m_wCapLine.Show(true);
        }
        else
        {
            m_wCapLine.Show(false);
        }

        // ---- v1.1.0: Tank line (T2 Water Pump only) ----
        m_TankLineOffset = 0.0;
        if (m_wTankLine)
        {
            LFPG_WaterPump_T2 t2Pump = LFPG_WaterPump_T2.Cast(device);
            if (t2Pump)
            {
                float tankLvl = t2Pump.LFPG_GetTankLevel();
                int tankLiq = t2Pump.LFPG_GetTankLiquidType();
                bool tankPow = t2Pump.LFPG_GetPoweredNet();

                int tankPct = 0;
                if (LFPG_PUMP_TANK_MAX > 0.0)
                {
                    tankPct = (tankLvl / LFPG_PUMP_TANK_MAX) * 100.0;
                }

                int tankLvlInt = tankLvl;
                int tankMaxInt = LFPG_PUMP_TANK_MAX;

                string tankText = "Tank: ";
                tankText = tankText + tankLvlInt.ToString() + "L / " + tankMaxInt.ToString() + "L";
                tankText = tankText + "  (" + tankPct.ToString() + "%)";

                // Status indicator
                bool isFull = false;
                if (tankLvl >= LFPG_PUMP_TANK_MAX - 0.1)
                {
                    isFull = true;
                }

                if (tankPow && !isFull)
                {
                    tankText = tankText + "  >> FILLING";
                }
                else if (tankPow && isFull)
                {
                    tankText = tankText + "  -- FULL";
                }
                else if (!tankPow && tankLvl > 0.01)
                {
                    tankText = tankText + "  [OFFLINE]";
                }
                else if (!tankPow && tankLvl < 0.01)
                {
                    tankText = tankText + "  [EMPTY]";
                }

                m_wTankLine.SetText(tankText);

                // Color: filling=cyan, full=green, offline=yellow, empty=grey
                if (tankPow && !isFull)
                {
                    m_wTankLine.SetColor(ARGB(255, 50, 200, 220));
                }
                else if (tankPow && isFull)
                {
                    m_wTankLine.SetColor(ARGB(255, 46, 155, 89));
                }
                else if (tankLvl < 0.01)
                {
                    m_wTankLine.SetColor(ARGB(255, 120, 120, 120));
                }
                else if (tankLiq == LIQUID_CLEANWATER)
                {
                    m_wTankLine.SetColor(ARGB(255, 51, 153, 255));
                }
                else
                {
                    m_wTankLine.SetColor(ARGB(255, 136, 170, 68));
                }

                m_wTankLine.Show(true);
                m_TankLineOffset = 20.0;
            }
            else
            {
                m_wTankLine.Show(false);
                m_TankLineOffset = 0.0;
            }
        }

        // ---- v1.2.0: Fuel line (LFPG_Furnace only) ----
        m_FuelLineOffset = 0.0;
        m_ReserveLineOffset = 0.0;
        if (m_wFuelLine)
        {
            LFPG_Furnace furnaceDevice = LFPG_Furnace.Cast(device);
            if (furnaceDevice)
            {
                int fuelCur = furnaceDevice.LFPG_GetFuelCurrent();
                bool fuelOn = furnaceDevice.LFPG_GetSourceOn();
                int fuelMax = LFPG_FURNACE_MAX_FUEL;

                // Time remaining from current fuel (Days + Hours)
                int totalSec = fuelCur * 30;
                int fuelDays = totalSec / 86400;
                int fuelHours = (totalSec % 86400) / 3600;

                // Fuel percentage
                float fuelPctF = 0.0;
                if (fuelMax > 0)
                {
                    fuelPctF = (fuelCur * 100.0) / fuelMax;
                }
                int fuelPctW = Math.Floor(fuelPctF);
                float fuelPctFrac = fuelPctF - fuelPctW;
                int fuelPctT = Math.Round(fuelPctFrac * 10.0);
                if (fuelPctT >= 10)
                {
                    fuelPctW = fuelPctW + 1;
                    fuelPctT = 0;
                }

                // Fuel line: "Fuel: 500/2880 (17.3%) | 0D 12H"
                string fuelText = "Fuel: ";
                fuelText = fuelText + fuelCur.ToString() + "/" + fuelMax.ToString();
                fuelText = fuelText + " (" + fuelPctW.ToString() + "." + fuelPctT.ToString() + "%)";
                fuelText = fuelText + " | " + fuelDays.ToString() + "D " + fuelHours.ToString() + "H";

                // Cargo reserve data (used for both fuel line status + reserve line)
                int cargoCount = furnaceDevice.LFPG_GetCargoItemCount();

                // Status color (shared with reserve line below)
                int fuelLineColor = ARGB(255, 120, 120, 120);
                if (fuelOn)
                {
                    fuelLineColor = ARGB(255, 50, 200, 220);
                }
                else if (fuelCur > 0)
                {
                    fuelText = fuelText + " [OFF]";
                    fuelLineColor = ARGB(255, 230, 200, 50);
                }
                else if (cargoCount > 0)
                {
                    fuelText = fuelText + " [OFF]";
                    fuelLineColor = ARGB(255, 230, 160, 50);
                }
                else
                {
                    fuelText = fuelText + " [EMPTY]";
                }
                m_wFuelLine.SetColor(fuelLineColor);

                m_wFuelLine.SetText(fuelText);
                m_wFuelLine.Show(true);
                m_FuelLineOffset = 20.0;

                // ---- v1.2.2: Reserve line (separate widget below fuel) ----
                if (m_wReserveLine)
                {
                    if (cargoCount > 0)
                    {
                        int cargoFuel = furnaceDevice.LFPG_GetCargoFuelEstimate();
                        int resSec = cargoFuel * 30;
                        int resDays = resSec / 86400;
                        int resHours = (resSec % 86400) / 3600;

                        string resText = "Reserve: ";
                        resText = resText + cargoCount.ToString();
                        resText = resText + " | " + resDays.ToString() + "D " + resHours.ToString() + "H approx";

                        m_wReserveLine.SetText(resText);
                        m_wReserveLine.SetColor(fuelLineColor);
                        m_wReserveLine.SetPos(14, 94 + m_FuelLineOffset);
                        m_wReserveLine.Show(true);
                        m_ReserveLineOffset = 20.0;
                    }
                    else
                    {
                        m_wReserveLine.Show(false);
                        m_ReserveLineOffset = 0.0;
                    }
                }
            }
            else
            {
                m_wFuelLine.Show(false);
                m_FuelLineOffset = 0.0;
                if (m_wReserveLine)
                {
                    m_wReserveLine.Show(false);
                    m_ReserveLineOffset = 0.0;
                }
            }
        }

        // v2.4: Link line (LFPG_Sorter only)
        m_LinkLineOffset = 0.0;
        if (m_wLinkLine)
        {
            LFPG_Sorter sorterInspect = LFPG_Sorter.Cast(device);
            if (sorterInspect)
            {
                float linkY = 94.0 + m_TankLineOffset + m_FuelLineOffset + m_ReserveLineOffset;
                m_wLinkLine.SetPos(14, linkY);

                EntityAI linkedEnt = sorterInspect.LFPG_GetLinkedContainer();
                if (linkedEnt)
                {
                    string linkText = "Linked: ";
                    linkText = linkText + linkedEnt.GetDisplayName();
                    m_wLinkLine.SetText(linkText);
                    m_wLinkLine.SetColor(ARGB(255, 52, 211, 153));
                }
                else
                {
                    string noLinkText = "Not linked";
                    m_wLinkLine.SetText(noLinkText);
                    m_wLinkLine.SetColor(ARGB(255, 248, 113, 113));
                }
                m_wLinkLine.Show(true);
                m_LinkLineOffset = 20.0;
            }
            else
            {
                m_wLinkLine.Show(false);
            }
        }

        // ---- v2.1: Battery line (LFPG_BatteryBase tiers) ----
        m_BatteryLineOffset = 0.0;
        if (m_wBatteryLine)
        {
            LFPG_BatteryBase batDevice = LFPG_BatteryBase.Cast(device);
            if (batDevice)
            {
                float batStored = batDevice.LFPG_GetStoredEnergy();
                float batMax = batDevice.LFPG_GetMaxStoredEnergy();
                float batRate = batDevice.LFPG_GetChargeRateCurrent();
                bool batOutEnabled = batDevice.LFPG_IsOutputEnabled();

                // Percentage
                int batPct = 0;
                if (batMax > 0.0)
                {
                    batPct = (batStored / batMax) * 100.0;
                }
                if (batPct > 100)
                {
                    batPct = 100;
                }

                // Integer display values
                int batStoredInt = batStored;
                int batMaxInt = batMax;

                // Base text: "Charge: 5000/10000 (50%)"
                string batText = "Charge: ";
                batText = batText + batStoredInt.ToString();
                batText = batText + "/";
                batText = batText + batMaxInt.ToString();
                batText = batText + " (";
                batText = batText + batPct.ToString();
                batText = batText + "%)";

                // Status suffix + color
                int batColor = ARGB(255, 120, 120, 120);

                if (!batOutEnabled)
                {
                    // Output disabled (switch off)
                    batText = batText + "  [OFF]";
                    batColor = ARGB(255, 180, 180, 50);
                }
                else if (batRate > 0.5)
                {
                    // Charging
                    int chgRate = batRate;
                    batText = batText + "  >> CHG +";
                    batText = batText + chgRate.ToString();
                    batText = batText + " u/s";
                    batColor = ARGB(255, 50, 200, 220);
                }
                else if (batRate < -0.5)
                {
                    // Discharging
                    float absRate = -batRate;
                    int disRate = absRate;
                    batText = batText + "  << DIS -";
                    batText = batText + disRate.ToString();
                    batText = batText + " u/s";
                    batColor = ARGB(255, 230, 126, 34);
                }
                else if (batPct >= 100)
                {
                    // Full and idle
                    batText = batText + "  FULL";
                    batColor = ARGB(255, 46, 155, 89);
                }
                else if (batPct < 1)
                {
                    // Empty
                    batText = batText + "  EMPTY";
                    batColor = ARGB(255, 200, 60, 60);
                }
                else
                {
                    // Idle (connected but not charging/discharging)
                    batText = batText + "  IDLE";
                    batColor = ARGB(255, 160, 160, 160);
                }

                float batY = 94.0 + m_TankLineOffset + m_FuelLineOffset + m_ReserveLineOffset + m_LinkLineOffset;
                m_wBatteryLine.SetPos(14, batY);
                m_wBatteryLine.SetText(batText);
                m_wBatteryLine.SetColor(batColor);
                m_wBatteryLine.Show(true);
                m_BatteryLineOffset = 26.0;
            }
            else
            {
                m_wBatteryLine.Show(false);
                m_BatteryLineOffset = 0.0;
            }
        }

        // Reposition separator + wires header with extra line offsets
        // (TankLine vs FuelLine+ReserveLine are mutually exclusive sets)
        float extraLineOffset = m_TankLineOffset + m_FuelLineOffset + m_ReserveLineOffset + m_LinkLineOffset + m_BatteryLineOffset;
        if (m_wSeparator)
        {
            m_wSeparator.SetPos(12, 93 + extraLineOffset);
        }
        if (m_wWiresHeader)
        {
            m_wWiresHeader.SetPos(14, 99 + extraLineOffset);
        }

        // ---- Wire section: re-display cached data or show loading ----
        if (m_HasServerData)
        {
            PopulateWireData();
        }
        else
        {
            // Ensure separator + header visible (P2-A may have collapsed them)
            if (m_wSeparator)
            {
                m_wSeparator.Show(true);
            }
            m_wWiresHeader.Show(true);
            m_wWiresHeader.SetText(Loc("#STR_LFPG_INSPECT_CONN_LOADING"));
            HideAllWireSlots();
            ResizePanelHeight(0);
        }
    }

    // =========================================================
    // Apply server RPC response (wire topology)
    // =========================================================
    static void OnInspectResponse(string deviceId, ref array<ref LFPG_InspectWireEntry> wires)
    {
        LFPG_DeviceInspector inst = Get();
        if (!inst.m_Root)
            return;

        // Stale response — player already looking at different device
        if (deviceId != inst.m_CurrentDeviceId)
        {
            string dbgMsg = "[DeviceInspector] Stale response for ";
            dbgMsg = dbgMsg + deviceId;
            dbgMsg = dbgMsg + " (current=";
            dbgMsg = dbgMsg + inst.m_CurrentDeviceId;
            dbgMsg = dbgMsg + ")";
            LFPG_Util.Debug(dbgMsg);
            return;
        }

        inst.m_HasServerData = true;
        inst.m_RespWires.Clear();

        int wi;
        for (wi = 0; wi < wires.Count(); wi = wi + 1)
        {
            inst.m_RespWires.Insert(wires[wi]);
        }

        // v0.8.1: Resolve entity and run full PopulateClientData so capLine
        // "Total Load" updates in the same pass as wire data.
        // PopulateClientData calls PopulateWireData internally (m_HasServerData
        // is now true), so everything updates in one shot — no double call,
        // no frame delay, no m_LastClientRefreshMs hack needed.
        EntityAI respTarget = LFPG_DeviceRegistry.Get().FindById(deviceId);
        if (respTarget)
        {
            inst.PopulateClientData(respTarget, deviceId);
        }
        else
        {
            // Entity gone (destroyed/despawned) — just update wires section.
            // Next Tick() will detect missing entity and hide panel.
            inst.PopulateWireData();
        }
    }

    protected void PopulateWireData()
    {
        if (!m_wWiresHeader)
            return;

        int wireCount = m_RespWires.Count();

        if (wireCount == 0)
        {
            // P2-A: Collapse wire section entirely — cleaner look
            m_wWiresHeader.Show(false);
            if (m_wSeparator)
            {
                m_wSeparator.Show(false);
            }
            HideAllWireSlots();
            m_VisibleWireCount = 0;
            ResizePanelCompact();
            return;
        }

        // Ensure separator + header are visible (may have been hidden by 0-wire collapse)
        if (m_wSeparator)
        {
            m_wSeparator.Show(true);
            m_wSeparator.SetPos(12, 93 + m_TankLineOffset + m_FuelLineOffset + m_ReserveLineOffset);
        }
        m_wWiresHeader.Show(true);
        m_wWiresHeader.SetPos(14, 99 + m_TankLineOffset + m_FuelLineOffset + m_ReserveLineOffset);

        // Reposition wire slots with tank/fuel offset
        int ri;
        for (ri = 0; ri < m_wWireSlots.Count(); ri = ri + 1)
        {
            TextWidget rSlot = m_wWireSlots[ri];
            if (rSlot)
            {
                float rY = LFPG_INSPECT_PANEL_BASE_H + 2.0 + m_TankLineOffset + m_FuelLineOffset + m_ReserveLineOffset + (ri * LFPG_INSPECT_WIRE_ROW_H);
                rSlot.SetPos(14, rY);
            }
        }

        int maxShow = m_wWireSlots.Count();
        if (wireCount < maxShow)
        {
            maxShow = wireCount;
        }

        // Header text with overflow indicator
        string hdrText = Loc("#STR_LFPG_INSPECT_CONNECTIONS");
        hdrText = hdrText + " (";
        hdrText = hdrText + wireCount.ToString();
        if (wireCount > maxShow)
        {
            hdrText = hdrText + " | ";
            hdrText = hdrText + Loc("#STR_LFPG_INSPECT_SHOWING");
            hdrText = hdrText + " ";
            hdrText = hdrText + maxShow.ToString();
        }
        hdrText = hdrText + ")";
        m_wWiresHeader.SetText(hdrText);

        int si;
        for (si = 0; si < maxShow; si = si + 1)
        {
            LFPG_InspectWireEntry entry = m_RespWires[si];
            TextWidget slot = m_wWireSlots[si];
            if (!slot)
                continue;

            // Build display text: direction arrow + local port + remote name
            string arrow = "";
            if (entry.m_Direction == LFPG_PortDir.OUT)
            {
                arrow = Loc("#STR_LFPG_INSPECT_DIR_OUT");
                arrow = arrow + " ";
            }
            else
            {
                arrow = Loc("#STR_LFPG_INSPECT_DIR_IN");
                arrow = arrow + "  ";
            }

            string line = arrow;
            line = line + FormatPortName(entry.m_LocalPort);
            line = line + "  >  ";
            line = line + FormatDeviceName(entry.m_RemoteTypeName);

            // v0.7.47: Append allocated power to the right
            if (entry.m_AllocatedPower > LFPG_PROPAGATION_EPSILON)
            {
                line = line + "  · ";
                line = line + FormatFloat1(entry.m_AllocatedPower);
                line = line + " u/s";
            }

            slot.SetText(line);

            // v0.7.47: Color based on edge state (overrides direction color)
            int wireColor = ARGB(255, 100, 160, 210);
            if (entry.m_EdgeState == 2)
            {
                // BROWNOUT: orange-red
                wireColor = ARGB(255, 220, 80, 50);
            }
            else if (entry.m_Direction == LFPG_PortDir.OUT)
            {
                wireColor = ARGB(255, 100, 180, 100);
            }
            slot.SetColor(wireColor);
            slot.Show(true);
        }

        // Hide unused slots
        int hi;
        for (hi = maxShow; hi < m_wWireSlots.Count(); hi = hi + 1)
        {
            TextWidget hideSlot = m_wWireSlots[hi];
            if (hideSlot)
            {
                hideSlot.Show(false);
            }
        }

        m_VisibleWireCount = maxShow;
        ResizePanelHeight(maxShow);
    }

    // =========================================================
    // Panel sizing and positioning
    // =========================================================
    protected static float ComputePanelHeight(int wireCount)
    {
        float h = LFPG_INSPECT_PANEL_BASE_H;
        h = h + (wireCount * LFPG_INSPECT_WIRE_ROW_H);
        h = h + LFPG_INSPECT_PANEL_PAD;
        return h;
    }

    protected void ResizePanelHeight(int wireCount)
    {
        float h = ComputePanelHeight(wireCount);
        // v4.3 (Audit fix F3): Include ALL dynamic line offsets.
        // Previously missing m_LinkLineOffset + m_BatteryLineOffset
        // caused panel clipping when Sorter LinkLine or Battery
        // charge line were visible alongside wire rows.
        h = h + m_TankLineOffset + m_FuelLineOffset + m_ReserveLineOffset + m_LinkLineOffset + m_BatteryLineOffset;
        ApplyPanelSize(h);
    }

    // P2-A: Compact panel height when wire section is collapsed (0 confirmed wires).
    // Separator and WiresHeader are hidden, so panel stops after CapLine + padding.
    protected void ResizePanelCompact()
    {
        // v4.3 (Audit fix F3): Include ALL dynamic line offsets (same as ResizePanelHeight).
        ApplyPanelSize(LFPG_INSPECT_COMPACT_H + m_TankLineOffset + m_FuelLineOffset + m_ReserveLineOffset + m_LinkLineOffset + m_BatteryLineOffset);
    }

    // Shared resize implementation: sets panel, background, and accent bar heights.
    protected void ApplyPanelSize(float h)
    {
        if (!m_Panel)
            return;

        m_CurrentPanelH = h;

        m_Panel.SetSize(LFPG_INSPECT_PANEL_W, h);
        if (m_wPanelBg)
        {
            m_wPanelBg.SetSize(LFPG_INSPECT_PANEL_W, h);
        }
        if (m_wAccentBar)
        {
            m_wAccentBar.SetSize(LFPG_INSPECT_ACCENT_W, h);
        }
    }

    protected bool UpdatePanelPosition(EntityAI device)
    {
        if (!m_Panel || !device)
            return false;

        // Project device world position to screen
        vector worldPos = device.GetPosition();
        worldPos[1] = worldPos[1] + LFPG_INSPECT_WORLD_Y_OFFSET;

        vector screenPos = GetGame().GetScreenPos(worldPos);

        // Behind camera check — return false, caller hides without clearing state
        if (screenPos[2] < LFPG_BEHIND_CAM_Z)
        {
            return false;
        }

        // Screen dimensions
        int screenW = 0;
        int screenH = 0;
        GetScreenSize(screenW, screenH);

        float px = screenPos[0] + LFPG_INSPECT_OFFSET_X;
        float py = screenPos[1] + LFPG_INSPECT_OFFSET_Y;

        // Clamp to screen bounds
        float panelW = LFPG_INSPECT_PANEL_W;
        float panelH = m_CurrentPanelH;
        if (panelH < 1.0)
        {
            panelH = ComputePanelHeight(m_VisibleWireCount) + m_TankLineOffset + m_FuelLineOffset + m_ReserveLineOffset + m_BatteryLineOffset;
        }

        float fScreenW = screenW;
        float fScreenH = screenH;

        // Flip to left side if too close to right edge (P2-B: with hysteresis)
        // rightEdge = where the panel's right side WOULD be if placed on the right.
        float rightEdge = px + panelW;
        if (!m_FlippedLeft)
        {
            // Not flipped yet: flip when panel overflows right margin
            if (rightEdge > fScreenW - LFPG_INSPECT_SCREEN_MARGIN)
            {
                m_FlippedLeft = true;
            }
        }
        else
        {
            // Currently flipped: only un-flip when panel clears right margin + hysteresis
            if (rightEdge < fScreenW - LFPG_INSPECT_SCREEN_MARGIN - LFPG_INSPECT_FLIP_HYSTERESIS)
            {
                m_FlippedLeft = false;
            }
        }

        if (m_FlippedLeft)
        {
            px = screenPos[0] - panelW - LFPG_INSPECT_OFFSET_X;
        }

        // Clamp vertical
        if (py < LFPG_INSPECT_SCREEN_MARGIN)
        {
            py = LFPG_INSPECT_SCREEN_MARGIN;
        }
        if (py + panelH > fScreenH - LFPG_INSPECT_SCREEN_MARGIN)
        {
            py = fScreenH - panelH - LFPG_INSPECT_SCREEN_MARGIN;
        }

        // Clamp horizontal minimum
        if (px < LFPG_INSPECT_SCREEN_MARGIN)
        {
            px = LFPG_INSPECT_SCREEN_MARGIN;
        }

        // ---- Position smoothing (P1-A anti-jitter) ----
        // First frame after device switch: snap directly (no lag).
        // Subsequent frames: lerp towards target to absorb camera jitter.
        if (!m_SmoothInit)
        {
            m_SmoothX = px;
            m_SmoothY = py;
            m_SmoothInit = true;
        }
        else
        {
            float dx = px - m_SmoothX;
            float dy = py - m_SmoothY;
            m_SmoothX = m_SmoothX + (dx * LFPG_INSPECT_POS_LERP);
            m_SmoothY = m_SmoothY + (dy * LFPG_INSPECT_POS_LERP);
        }

        m_Panel.SetPos(m_SmoothX, m_SmoothY);
        return true;
    }

    // =========================================================
    // Show / Hide
    // =========================================================
    protected void ShowPanel()
    {
        if (!m_Visible && m_Root)
        {
            m_Root.Show(true);
            m_Visible = true;
        }
    }

    protected void HidePanel()
    {
        if (m_Visible && m_Root)
        {
            m_Root.Show(false);
            m_Visible = false;
        }

        if (m_CurrentDeviceId != "")
        {
            m_CurrentDeviceId = "";
            m_HasServerData = false;
            m_RespWires.Clear();
        }
    }

    protected void HideAllWireSlots()
    {
        int i;
        for (i = 0; i < m_wWireSlots.Count(); i = i + 1)
        {
            TextWidget tw = m_wWireSlots[i];
            if (tw)
            {
                tw.Show(false);
            }
        }
        m_VisibleWireCount = 0;
    }

    // =========================================================
    // RPC request (client → server)
    // v0.7.43 (Fix 2): Send NetworkID for authoritative resolution.
    // The client's deviceId may not match the server's (SyncVar race
    // during kit placement). Server resolves via NetworkID (engine
    // identity, always matches). Client deviceId echoed as correlation.
    // Same proven pattern as FinishWiring.
    // =========================================================
    protected void RequestServerData(PlayerBase player, string deviceId, EntityAI targetEntity)
    {
        if (!player)
            return;

        // Cooldown
        float nowMs = GetGame().GetTime();
        float elapsed = nowMs - m_LastRPCSendMs;
        if (elapsed < LFPG_INSPECT_RPC_COOLDOWN_MS)
            return;

        m_LastRPCSendMs = nowMs;

        // Get NetworkID directly from the raycast entity (authoritative)
        int netLow = 0;
        int netHigh = 0;
        if (targetEntity)
        {
            targetEntity.GetNetworkID(netLow, netHigh);
        }

        // Build and send RPC
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(LFPG_RPC_SubId.INSPECT_DEVICE);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Write(deviceId);
        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);

        LFPG_Util.Debug("[DeviceInspector] Sent INSPECT_DEVICE for " + deviceId);
    }

    // =========================================================
    // Formatting helpers
    // =========================================================

    // Clean up entity type name for display.
    // "LF_TestLamp" → "Lamp"
    // "LF_TestGenerator" → "Generator"
    // "LFPG_Splitter" → "Splitter"
    protected static string FormatDeviceName(string typeName)
    {
        if (typeName == "")
            return "Unknown";

        // Strip "LFPG_" prefix (production devices), then "LF_" (test devices)
        string result = typeName;
        if (result.Length() > 5)
        {
            string pfxLong = result.Substring(0, 5);
            if (pfxLong == "LFPG_")
            {
                result = result.Substring(5, result.Length() - 5);
            }
        }
        if (result == typeName && result.Length() > 3)
        {
            string pfxShort = result.Substring(0, 3);
            if (pfxShort == "LF_")
            {
                result = result.Substring(3, result.Length() - 3);
            }
        }

        // Strip "Test" prefix for cleaner display
        if (result.Length() > 4)
        {
            string testPfx = result.Substring(0, 4);
            if (testPfx == "Test")
            {
                result = result.Substring(4, result.Length() - 4);
            }
        }

        // If we stripped everything, use original
        if (result == "")
        {
            result = typeName;
        }

        return result;
    }

    // Resolve a stringtable key (e.g. "#STR_LFPG_INSPECT_SOURCE") to the
    // player's current language. Thin wrapper so there is a single place
    // to change if the engine API ever differs.
    protected static string Loc(string key)
    {
        return Widget.TranslateString(key);
    }

    // Clean up port internal name for display.
    // "input_main" → "Main Input"
    // "input_1"    → "Input 1"
    // "output_1"   → "Output 1"
    // "output_2"   → "Output 2"
    // Unknown patterns → returned as-is
    protected static string FormatPortName(string portName)
    {
        if (portName == "")
            return "—";

        // Known special names
        if (portName == "input_main")
            return Loc("#STR_LFPG_INSPECT_PORT_MAIN_IN");

        // Check "input_N" pattern
        int portLen = portName.Length();
        if (portLen > 6)
        {
            string pfxIn = portName.Substring(0, 6);
            if (pfxIn == "input_")
            {
                string inSuffix = portName.Substring(6, portLen - 6);
                return Loc("#STR_LFPG_INSPECT_PORT_INPUT") + " " + inSuffix;
            }
        }

        // Check "output_N" pattern
        if (portLen > 7)
        {
            string pfxOut = portName.Substring(0, 7);
            if (pfxOut == "output_")
            {
                string outSuffix = portName.Substring(7, portLen - 7);
                return Loc("#STR_LFPG_INSPECT_PORT_OUTPUT") + " " + outSuffix;
            }
        }

        // Fallback: return as-is
        return portName;
    }

    // Format float to 1 decimal place.
    // Enforce Script has no printf, so we do it manually.
    protected static string FormatFloat1(float val)
    {
        // Handle negative: format absolute value then prepend "-"
        string sign = "";
        float absVal = val;
        if (val < 0.0)
        {
            sign = "-";
            absVal = -val;
        }

        int whole = Math.Floor(absVal);
        float frac = absVal - whole;
        int tenths = Math.Round(frac * 10.0);
        if (tenths >= 10)
        {
            whole = whole + 1;
            tenths = 0;
        }
        string result = sign;
        result = result + whole.ToString();
        result = result + ".";
        result = result + tenths.ToString();
        return result;
    }

    // Build ASCII load bar: [========--] style
    protected static string BuildLoadBar(float ratio)
    {
        if (ratio < 0.0)
        {
            ratio = 0.0;
        }
        if (ratio > 1.5)
        {
            ratio = 1.5;
        }

        int totalSlots = 10;
        int filled = Math.Round(ratio * totalSlots);
        if (filled > totalSlots)
        {
            filled = totalSlots;
        }

        string bar = "[";
        int bi;
        for (bi = 0; bi < totalSlots; bi = bi + 1)
        {
            if (bi < filled)
            {
                bar = bar + "|";
            }
            else
            {
                bar = bar + ".";
            }
        }
        bar = bar + "]";
        return bar;
    }
};
