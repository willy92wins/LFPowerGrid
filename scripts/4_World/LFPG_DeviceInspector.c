// =========================================================
// LF_PowerGrid - Device Inspector (v0.8.0, Sprint 5 S1)
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

    void LFPG_InspectWireEntry()
    {
        m_Direction = -1;
        m_LocalPort = "";
        m_RemoteDeviceId = "";
        m_RemotePort = "";
        m_RemoteTypeName = "";
    }
};

class LFPG_DeviceInspector
{
    // ---- Singleton ----
    protected static ref LFPG_DeviceInspector s_Instance;

    // ---- Widget references ----
    protected Widget m_Root;
    protected Widget m_Panel;
    protected TextWidget m_wDeviceName;
    protected TextWidget m_wDeviceType;
    protected TextWidget m_wStatusLine;
    protected TextWidget m_wCapLine;
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

        m_Panel = m_Root.FindAnyWidget("InspectorPanel");
        m_wDeviceName = TextWidget.Cast(m_Root.FindAnyWidget("DeviceName"));
        m_wDeviceType = TextWidget.Cast(m_Root.FindAnyWidget("DeviceType"));
        m_wStatusLine = TextWidget.Cast(m_Root.FindAnyWidget("StatusLine"));
        m_wCapLine = TextWidget.Cast(m_Root.FindAnyWidget("CapLine"));
        m_wWiresHeader = TextWidget.Cast(m_Root.FindAnyWidget("WiresHeader"));

        // Set static text colors (layout has no color attrs — uses engine default white).
        // Dynamic colors (DeviceType, StatusLine, Wire slots) are set in Populate methods.
        if (m_wDeviceName)
        {
            m_wDeviceName.SetColor(ARGB(255, 242, 242, 242));
        }
        if (m_wWiresHeader)
        {
            m_wWiresHeader.SetColor(ARGB(255, 180, 180, 180));
        }
        if (m_wCapLine)
        {
            m_wCapLine.SetColor(ARGB(255, 140, 140, 140));
        }

        // Load procedural textures on background ImageWidgets.
        // Layout may or may not support src="#(argb...)", so we force-load
        // from code as a safety net. White 1x1 texture tinted by SetColor().
        string procTex = "#(argb,1,1,3)color(1,1,1,1,ca)";
        ImageWidget imgBg = ImageWidget.Cast(m_Root.FindAnyWidget("PanelBg"));
        if (imgBg)
        {
            imgBg.LoadImageFile(0, procTex);
            imgBg.SetColor(ARGB(235, 9, 14, 23));
        }
        ImageWidget imgHeader = ImageWidget.Cast(m_Root.FindAnyWidget("HeaderBar"));
        if (imgHeader)
        {
            imgHeader.LoadImageFile(0, procTex);
            imgHeader.SetColor(ARGB(242, 13, 19, 31));
        }
        ImageWidget imgAccent = ImageWidget.Cast(m_Root.FindAnyWidget("AccentBar"));
        if (imgAccent)
        {
            imgAccent.LoadImageFile(0, procTex);
            imgAccent.SetColor(ARGB(217, 46, 140, 191));
        }
        ImageWidget imgSep = ImageWidget.Cast(m_Root.FindAnyWidget("Separator"));
        if (imgSep)
        {
            imgSep.LoadImageFile(0, procTex);
            imgSep.SetColor(ARGB(153, 51, 64, 89));
        }

        // Collect wire slot widgets
        m_wWireSlots.Clear();
        int wi;
        for (wi = 0; wi < LFPG_INSPECT_MAX_WIRES; wi = wi + 1)
        {
            string slotName = "Wire" + wi.ToString();
            TextWidget tw = TextWidget.Cast(m_Root.FindAnyWidget(slotName));
            if (tw)
            {
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
        m_wDeviceName = null;
        m_wDeviceType = null;
        m_wStatusLine = null;
        m_wCapLine = null;
        m_wWiresHeader = null;
        m_wWireSlots.Clear();
    }

    // =========================================================
    // Per-frame tick (client only)
    // =========================================================
    static void Tick()
    {
        LFPG_DeviceInspector inst = Get();
        if (!inst.m_Root)
            return;

        if (GetGame().IsDedicatedServer())
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

        // ---- Condition 4: Raycast to device under cursor ----
        EntityAI target = LFPG_ActionRaycast.GetCursorTargetDevice(player);
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
            inst.PopulateClientData(target, deviceId);
            inst.RequestServerData(player, deviceId);
            inst.m_LastClientRefreshMs = nowMs;
        }
        else
        {
            // H1 fix: Periodic SyncVar refresh while looking at same device.
            // Catches state changes (gen on/off, load ratio updates) without
            // requiring the player to look away and back.
            // 500ms = 2Hz refresh, negligible cost (DeviceAPI calls are cached SyncVars).
            float sinceLast = nowMs - inst.m_LastClientRefreshMs;
            if (sinceLast >= 500.0)
            {
                inst.PopulateClientData(target, deviceId);
                inst.m_LastClientRefreshMs = nowMs;
            }

            // H2 fix: Retry RPC if cooldown blocked the initial request.
            // Without this, rapidly switching devices leaves panel stuck on
            // "Connections ..." because the cooldown guard returns early.
            if (!inst.m_HasServerData)
            {
                inst.RequestServerData(player, deviceId);
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
        if (!m_wDeviceName)
            return;

        // ---- Device name (entity type, cleaned up) ----
        string typeName = device.GetType();
        m_wDeviceName.SetText(FormatDeviceName(typeName));

        // ---- Device type badge ----
        int devType = LFPG_DeviceAPI.GetDeviceType(device);
        string typeStr = "UNKNOWN";
        int typeColor = ARGB(255, 140, 140, 140);

        if (devType == LFPG_DeviceType.SOURCE)
        {
            typeStr = "SOURCE";
            typeColor = ARGB(255, 230, 180, 50);
        }
        else if (devType == LFPG_DeviceType.CONSUMER)
        {
            typeStr = "CONSUMER";
            typeColor = ARGB(255, 100, 180, 220);
        }
        else if (devType == LFPG_DeviceType.PASSTHROUGH)
        {
            typeStr = "PASSTHROUGH";
            typeColor = ARGB(255, 160, 120, 220);
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

                statusText = "ACTIVE  ";
                string barStr = BuildLoadBar(loadRatio);
                statusText = statusText + barStr;
                statusText = statusText + " ";
                statusText = statusText + loadPct.ToString();
                statusText = statusText + "%";

                if (loadRatio >= LFPG_LOAD_CRITICAL_THRESHOLD)
                {
                    statusColor = ARGB(255, 230, 126, 34);
                }
                else if (loadRatio >= LFPG_LOAD_WARNING_THRESHOLD)
                {
                    statusColor = ARGB(255, 211, 155, 0);
                }
                else
                {
                    statusColor = ARGB(255, 46, 155, 89);
                }
            }
            else
            {
                statusText = "INACTIVE";
                statusColor = ARGB(255, 120, 120, 120);
            }
        }
        else
        {
            bool powered = LFPG_DeviceAPI.GetPowered(device);
            if (powered)
            {
                statusText = "POWERED";
                statusColor = ARGB(255, 46, 155, 89);
            }
            else
            {
                statusText = "UNPOWERED";
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
            capText = "Capacity: ";
            capText = capText + FormatFloat1(cap);
            capText = capText + " u/s";
        }
        else if (devType == LFPG_DeviceType.CONSUMER)
        {
            float cons = LFPG_DeviceAPI.GetConsumption(device);
            capText = "Consumption: ";
            capText = capText + FormatFloat1(cons);
            capText = capText + " u/s";
        }
        else if (devType == LFPG_DeviceType.PASSTHROUGH)
        {
            float ptCap = LFPG_DeviceAPI.GetCapacity(device);
            capText = "Throughput: ";
            capText = capText + FormatFloat1(ptCap);
            capText = capText + " u/s";
        }
        m_wCapLine.SetText(capText);
        // ---- Wire section: re-display cached data or show loading ----
        if (m_HasServerData)
        {
            PopulateWireData();
        }
        else
        {
            m_wWiresHeader.SetText("Connections ...");
            HideAllWireSlots();
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

        inst.PopulateWireData();
    }

    protected void PopulateWireData()
    {
        int wireCount = m_RespWires.Count();

        if (wireCount == 0)
        {
            m_wWiresHeader.SetText("No connections");
            HideAllWireSlots();
            m_VisibleWireCount = 0;
            ResizePanelHeight(0);
            return;
        }

        string hdrText = "Connections (";
        hdrText = hdrText + wireCount.ToString();
        hdrText = hdrText + ")";
        m_wWiresHeader.SetText(hdrText);

        int maxShow = m_wWireSlots.Count();
        if (wireCount < maxShow)
        {
            maxShow = wireCount;
        }

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
                arrow = "OUT ";
            }
            else
            {
                arrow = "IN  ";
            }

            string line = arrow;
            line = line + entry.m_LocalPort;
            line = line + "  >  ";
            line = line + FormatDeviceName(entry.m_RemoteTypeName);

            slot.SetText(line);

            // Color based on direction
            int wireColor = ARGB(255, 150, 150, 150);
            if (entry.m_Direction == LFPG_PortDir.OUT)
            {
                wireColor = ARGB(255, 100, 180, 100);
            }
            else
            {
                wireColor = ARGB(255, 100, 160, 210);
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
    protected void ResizePanelHeight(int wireCount)
    {
        if (!m_Panel)
            return;

        float h = LFPG_INSPECT_PANEL_BASE_H;
        h = h + (wireCount * LFPG_INSPECT_WIRE_ROW_H);
        h = h + LFPG_INSPECT_PANEL_PAD;

        m_Panel.SetSize(LFPG_INSPECT_PANEL_W, h);
    }

    protected bool UpdatePanelPosition(EntityAI device)
    {
        if (!m_Panel || !device)
            return false;

        // Project device world position to screen
        vector worldPos = device.GetPosition();
        worldPos[1] = worldPos[1] + 1.0;   // offset up from feet

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
        float panelH = LFPG_INSPECT_PANEL_BASE_H;
        panelH = panelH + (m_VisibleWireCount * LFPG_INSPECT_WIRE_ROW_H);
        panelH = panelH + LFPG_INSPECT_PANEL_PAD;

        float fScreenW = screenW;
        float fScreenH = screenH;

        // Flip to left side if too close to right edge
        if (px + panelW > fScreenW - 10.0)
        {
            px = screenPos[0] - panelW - LFPG_INSPECT_OFFSET_X;
        }

        // Clamp vertical
        if (py < 10.0)
        {
            py = 10.0;
        }
        if (py + panelH > fScreenH - 10.0)
        {
            py = fScreenH - panelH - 10.0;
        }

        // Clamp horizontal minimum
        if (px < 10.0)
        {
            px = 10.0;
        }

        m_Panel.SetPos(px, py);
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
    // =========================================================
    protected void RequestServerData(PlayerBase player, string deviceId)
    {
        if (!player)
            return;

        // Cooldown
        float nowMs = GetGame().GetTime();
        float elapsed = nowMs - m_LastRPCSendMs;
        if (elapsed < LFPG_INSPECT_RPC_COOLDOWN_MS)
            return;

        m_LastRPCSendMs = nowMs;

        // Build and send RPC
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(LFPG_RPC_SubId.INSPECT_DEVICE);
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
    // "LF_Splitter" → "Splitter"
    protected static string FormatDeviceName(string typeName)
    {
        if (typeName == "")
            return "Unknown";

        // Strip "LF_" prefix
        string result = typeName;
        if (result.Length() > 3)
        {
            string prefix = result.Substring(0, 3);
            if (prefix == "LF_")
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
