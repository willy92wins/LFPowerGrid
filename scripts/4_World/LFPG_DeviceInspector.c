class LFPG_InspectWireEntry
{
    static const int SCHEMA_VERSION = 2;
    int m_Direction;           // LFPG_PortDir.IN or OUT
    string m_LocalPort;        // port name on inspected device
    string m_RemoteTypeName;   // entity type name for display
    float m_AllocatedPower;    // Power flowing through this edge (u/s)
    int m_EdgeState;           // v1.0: 0=OK, 2=OVERLOADED (all-off)
    void LFPG_InspectWireEntry()
    {
        m_Direction = -1;
        m_LocalPort = "";
        m_RemoteTypeName = "";
        m_AllocatedPower = 0.0;
        m_EdgeState = 0;
    }
};
#ifndef SERVER
class LFPG_DeviceInspector
{
    protected static ref LFPG_DeviceInspector s_Instance;
    static const int COL_PANEL_BG     = 0xEB090E17;
    static const int COL_HEADER_BG    = 0xF20D131F;
    static const int COL_ACCENT       = 0xD92E8CBF;
    static const int COL_SEP          = 0x99334059;
    static const int COL_TEXT_WHITE   = 0xFFF2F2F2;
    static const int COL_TEXT_LIGHT   = 0xFFB4B4B4;
    static const int COL_GRAY         = 0xFF8C8C8C;
    static const int COL_GRAY_DIM     = 0xFF787878;
    static const int COL_GRAY_MID     = 0xFFA0A0A0;
    static const int COL_GREEN_OK     = 0xFF2E9B59;
    static const int COL_EMERALD      = 0xFF34D399;
    static const int COL_GREEN_WIRE   = 0xFF64B464;
    static const int COL_RED_ERROR    = 0xFFDC3232;
    static const int COL_RED_DARK     = 0xFFC83C3C;
    static const int COL_RED_SOFT     = 0xFFF87171;
    static const int COL_CYAN         = 0xFF32C8DC;
    static const int COL_BLUE         = 0xFF64B4DC;
    static const int COL_BLUE_BRIGHT  = 0xFF3399FF;
    static const int COL_BLUE_WIRE    = 0xFF64A0D2;
    static const int COL_ORANGE       = 0xFFE67E22;
    static const int COL_AMBER_SOURCE = 0xFFE6B432;
    static const int COL_AMBER_FUEL   = 0xFFE6A032;
    static const int COL_AMBER_WARN   = 0xFFE6C832;
    static const int COL_YELLOW       = 0xFFFFC832;
    static const int COL_OLIVE        = 0xFFB4B432;
    static const int COL_PURPLE       = 0xFFA078DC;
    static const int COL_OLIVE_GREEN  = 0xFF88AA44;
    static const int COL_RED_ORANGE   = 0xFFDC5032;
    static const int INSPECT_RPC_MAX_ATTEMPTS = 3;
	static const float INSPECT_SERVER_REFRESH_MS = 2000.0;
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
    protected bool m_Visible;
    protected string m_CurrentDeviceId;
    protected float m_LastRPCSendMs;
    protected bool m_HasServerData;
    protected int m_InspectRequestAttempts;
    protected int m_VisibleWireCount;
    protected float m_LastClientRefreshMs;
    protected float m_TankLineOffset;
    protected float m_FuelLineOffset;
    protected float m_ReserveLineOffset;
    protected TextWidget m_wLinkLine;
    protected float m_LinkLineOffset;
    protected TextWidget m_wBatteryLine;
    protected float m_BatteryLineOffset;
    protected float m_SmoothX;
    protected float m_SmoothY;
    protected bool m_SmoothInit;
    protected bool m_FlippedLeft;
    protected float m_CurrentPanelH;
    protected ref array<ref LFPG_InspectWireEntry> m_RespWires;
    protected bool m_WireDataDirty;
    protected int m_LastTopologyGeneration;
    protected ref map<Widget, string> m_LastWidgetText;
    protected ref map<Widget, int> m_LastWidgetColor;
    protected ref map<Widget, bool> m_LastWidgetVisible;
    protected ref map<Widget, vector> m_LastWidgetPos;
    protected ref map<Widget, vector> m_LastWidgetSize;
    protected bool m_ClientSnapshotValid;
    protected int m_SnapshotDeviceType;
    protected int m_SnapshotIntA;
    protected int m_SnapshotIntB;
    protected int m_SnapshotIntC;
    protected bool m_SnapshotBoolA;
    protected bool m_SnapshotBoolB;
    protected bool m_SnapshotBoolC;
    protected float m_SnapshotFloatA;
    protected float m_SnapshotFloatB;
    protected float m_SnapshotFloatC;
    protected float m_SnapshotFloatD;
    protected float m_SnapshotFloatE;
    protected float m_SnapshotFloatF;
    protected float m_SnapshotFloatG;
    protected EntityAI m_SnapshotEntity;
    static const string LAYOUT_PATH = "LFPowerGrid/gui/layouts/LFPG_DeviceInspector.layout";
    static LFPG_DeviceInspector Get()
    {
        if (!s_Instance)
        {
            s_Instance = new LFPG_DeviceInspector();
        }
        return s_Instance;
    }
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
            s_Instance = null;
        }
    }
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
        m_LastWidgetText = new map<Widget, string>;
        m_LastWidgetColor = new map<Widget, int>;
        m_LastWidgetVisible = new map<Widget, bool>;
        m_LastWidgetPos = new map<Widget, vector>;
        m_LastWidgetSize = new map<Widget, vector>;
        m_Visible = false;
        m_CurrentDeviceId = "";
        m_LastRPCSendMs = 0.0;
        m_HasServerData = false;
        m_InspectRequestAttempts = 0;
        m_VisibleWireCount = 0;
        m_LastClientRefreshMs = 0.0;
        m_SmoothX = 0.0;
        m_SmoothY = 0.0;
        m_SmoothInit = false;
        m_FlippedLeft = false;
        m_CurrentPanelH = 0.0;
        m_WireDataDirty = true;
        m_LastTopologyGeneration = -1;
        m_ClientSnapshotValid = false;
    }
    protected void CreateWidgets()
    {
        if (m_Root)
            return;
        m_Root = g_Game.GetWorkspace().CreateWidgets(LAYOUT_PATH);
        if (!m_Root)
        {
            LFPG_Util.Error("[DeviceInspector] Failed to create widgets from: " + LAYOUT_PATH);
            return;
        }
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
        float maxH = ComputePanelHeight(LFPG_INSPECT_MAX_WIRES);
        m_CurrentPanelH = maxH;
        if (m_Panel)
        {
            m_Panel.SetPos(0, 0);
            m_Panel.SetSize(LFPG_INSPECT_PANEL_W, maxH);
        }
        string procTex = "#(argb,8,8,3)color(1,1,1,1,CO)";
        ImageWidget imgBg = ImageWidget.Cast(m_Root.FindAnyWidget("PanelBg"));
        m_wPanelBg = imgBg;
        if (imgBg)
        {
            imgBg.SetPos(0, 0);
            imgBg.SetSize(LFPG_INSPECT_PANEL_W, maxH);
            imgBg.LoadImageFile(0, procTex);
            imgBg.SetColor(COL_PANEL_BG);
        }
        ImageWidget imgHeader = ImageWidget.Cast(m_Root.FindAnyWidget("HeaderBar"));
        if (imgHeader)
        {
            imgHeader.SetPos(0, 0);
            imgHeader.SetSize(LFPG_INSPECT_PANEL_W, LFPG_INSPECT_HEADER_H);
            imgHeader.LoadImageFile(0, procTex);
            imgHeader.SetColor(COL_HEADER_BG);
        }
        ImageWidget imgAccent = ImageWidget.Cast(m_Root.FindAnyWidget("AccentBar"));
        m_wAccentBar = imgAccent;
        if (imgAccent)
        {
            imgAccent.SetPos(0, 0);
            imgAccent.SetSize(LFPG_INSPECT_ACCENT_W, maxH);
            imgAccent.LoadImageFile(0, procTex);
            imgAccent.SetColor(COL_ACCENT);
        }
        ImageWidget imgSep = ImageWidget.Cast(m_Root.FindAnyWidget("Separator"));
        m_wSeparator = imgSep;
        if (imgSep)
        {
            imgSep.SetPos(12, 93);
            imgSep.SetSize(276, 1);
            imgSep.LoadImageFile(0, procTex);
            imgSep.SetColor(COL_SEP);
        }
        if (m_wDeviceName)
        {
            m_wDeviceName.SetPos(14, 7);
            m_wDeviceName.SetSize(274, 22);
            m_wDeviceName.SetColor(COL_TEXT_WHITE);
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
            m_wCapLine.SetColor(COL_GRAY);
        }
        if (m_wTankLine)
        {
            m_wTankLine.SetPos(14, 94);
            m_wTankLine.SetSize(274, 16);
            m_wTankLine.SetColor(COL_BLUE_BRIGHT);
            m_wTankLine.Show(false);
        }
        m_TankLineOffset = 0.0;
        if (m_wFuelLine)
        {
            m_wFuelLine.SetPos(14, 94);
            m_wFuelLine.SetSize(274, 16);
            m_wFuelLine.SetColor(COL_ORANGE);
            m_wFuelLine.Show(false);
        }
        m_FuelLineOffset = 0.0;
        if (m_wReserveLine)
        {
            m_wReserveLine.SetPos(14, 114);
            m_wReserveLine.SetSize(274, 16);
            m_wReserveLine.SetColor(COL_ORANGE);
            m_wReserveLine.Show(false);
        }
        m_ReserveLineOffset = 0.0;
        if (m_wLinkLine)
        {
            m_wLinkLine.SetPos(14, 94);
            m_wLinkLine.SetSize(274, 16);
            m_wLinkLine.SetColor(COL_EMERALD);
            m_wLinkLine.Show(false);
        }
        m_LinkLineOffset = 0.0;
        if (m_wBatteryLine)
        {
            m_wBatteryLine.SetPos(14, 94);
            m_wBatteryLine.SetSize(360, 18);
            m_wBatteryLine.SetColor(COL_YELLOW);
            m_wBatteryLine.Show(false);
        }
        m_BatteryLineOffset = 0.0;
        if (m_wWiresHeader)
        {
            m_wWiresHeader.SetPos(14, 99);
            m_wWiresHeader.SetSize(274, 16);
            m_wWiresHeader.SetColor(COL_TEXT_LIGHT);
        }
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
        m_LastWidgetText.Clear();
        m_LastWidgetColor.Clear();
        m_LastWidgetVisible.Clear();
        m_LastWidgetPos.Clear();
        m_LastWidgetSize.Clear();
    }
    static void Tick()
    {
        if (g_Game.IsDedicatedServer())
            return;
        LFPG_DeviceInspector inst = Get();
        if (!inst.m_Root)
            return;
        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player)
        {
            inst.HidePanel();
            return;
        }
        if (!IsHoldingCableReel(player))
        {
            inst.HidePanel();
            return;
        }
        LFPG_WiringClient wc = LFPG_WiringClient.Get();
        if (wc && wc.IsActive())
        {
            inst.HidePanel();
            return;
        }
        EntityAI target = LFPG_ActionRaycast.GetCursorTargetDeviceWithProximity(player);
        if (!target)
        {
            inst.HidePanel();
            return;
        }
        string deviceId = LFPG_DeviceAPI.GetDeviceId(target);
        if (deviceId == "")
        {
            inst.HidePanel();
            return;
        }
        float nowMs = g_Game.GetTime();
        int topologyGeneration = -1;
        LFPG_WireOwnerBase inspectedWireOwner = LFPG_WireOwnerBase.Cast(target);
        if (inspectedWireOwner)
        {
            topologyGeneration = inspectedWireOwner.LFPG_GetWireGeneration();
        }
        if (deviceId != inst.m_CurrentDeviceId)
        {
            inst.m_CurrentDeviceId = deviceId;
            inst.m_HasServerData = false;
            inst.m_InspectRequestAttempts = 0;
            inst.m_RespWires.Clear();
            inst.m_SmoothInit = false;
            inst.m_FlippedLeft = false;
            inst.m_WireDataDirty = true;
            inst.m_LastTopologyGeneration = topologyGeneration;
            inst.m_ClientSnapshotValid = false;
            inst.ClientDataChanged(target);
            inst.PopulateClientData(target, deviceId);
            inst.RequestServerData(player, deviceId, target);
            inst.m_LastClientRefreshMs = nowMs;
        }
        else
        {
            bool topologyChanged = (topologyGeneration != inst.m_LastTopologyGeneration);
            if (topologyChanged)
            {
                inst.m_LastTopologyGeneration = topologyGeneration;
                inst.m_HasServerData = false;
                inst.m_InspectRequestAttempts = 0;
                inst.m_WireDataDirty = true;
                inst.ClientDataChanged(target);
                inst.PopulateClientData(target, deviceId);
                inst.m_LastClientRefreshMs = nowMs;
            }
            else
            {
                float sinceLast = nowMs - inst.m_LastClientRefreshMs;
                if (sinceLast >= LFPG_INSPECT_REFRESH_MS)
                {
                    if (inst.ClientDataChanged(target))
                    {
                        inst.PopulateClientData(target, deviceId);
                    }
                    inst.m_LastClientRefreshMs = nowMs;
                }
            }
            if (!inst.m_HasServerData && inst.m_InspectRequestAttempts < INSPECT_RPC_MAX_ATTEMPTS)
            {
                inst.RequestServerData(player, deviceId, target);
            }
			else if (inst.m_HasServerData && nowMs - inst.m_LastRPCSendMs >= INSPECT_SERVER_REFRESH_MS)
			{
				inst.m_InspectRequestAttempts = 0;
				inst.RequestServerData(player, deviceId, target);
			}
        }
        bool posValid = inst.UpdatePanelPosition(target);
        if (posValid)
        {
            inst.ShowPanel();
        }
        else
        {
            if (inst.m_Visible && inst.m_Root)
            {
                inst.m_Root.Show(false);
                inst.m_Visible = false;
            }
        }
    }
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
    protected bool ClientDataChanged(EntityAI device)
    {
        int deviceType = LFPG_DeviceAPI.GetDeviceType(device);
        int intA = 0;
        int intB = 0;
        int intC = 0;
        bool boolA = false;
        bool boolB = false;
        bool boolC = false;
        float floatA = 0.0;
        float floatB = 0.0;
        float floatC = 0.0;
        float floatD = 0.0;
        float floatE = 0.0;
        float floatF = 0.0;
        float floatG = 0.0;
        EntityAI snapshotEntity = null;
        if (deviceType == LFPG_DeviceType.SOURCE)
        {
            boolA = LFPG_DeviceAPI.GetSourceOn(device);
            floatA = LFPG_DeviceAPI.GetLoadRatio(device);
            floatF = LFPG_DeviceAPI.GetCapacity(device);
        }
        else
        {
            boolA = LFPG_DeviceAPI.GetPowered(device);
            if (deviceType == LFPG_DeviceType.PASSTHROUGH)
            {
                floatF = LFPG_DeviceAPI.GetCapacity(device);
            }
            if (deviceType == LFPG_DeviceType.CONSUMER || deviceType == LFPG_DeviceType.CAMERA || deviceType == LFPG_DeviceType.PASSTHROUGH)
            {
                floatG = LFPG_DeviceAPI.GetConsumption(device);
            }
        }
        LFPG_MemoryCell memoryCell = LFPG_MemoryCell.Cast(device);
        if (memoryCell)
        {
            boolB = memoryCell.LFPG_GetCellActive();
        }
        LFPG_WaterPump_T2 t2Pump = LFPG_WaterPump_T2.Cast(device);
        if (t2Pump)
        {
            floatB = t2Pump.LFPG_GetTankLevel();
            intA = t2Pump.LFPG_GetTankLiquidType();
            boolB = t2Pump.LFPG_GetPoweredNet();
        }
        LFPG_Furnace furnace = LFPG_Furnace.Cast(device);
        if (furnace)
        {
            intA = furnace.LFPG_GetFuelCurrent();
            intB = furnace.LFPG_GetCargoItemCount();
            intC = furnace.LFPG_GetCargoFuelEstimate();
            boolB = furnace.LFPG_GetSourceOn();
        }
        LFPG_Sorter sorter = LFPG_Sorter.Cast(device);
        if (sorter)
        {
            snapshotEntity = sorter.LFPG_GetLinkedContainer();
        }
        LFPG_BatteryBase battery = LFPG_BatteryBase.Cast(device);
        if (battery)
        {
            floatC = battery.LFPG_GetStoredEnergy();
            floatD = battery.LFPG_GetMaxStoredEnergy();
            floatE = battery.LFPG_GetChargeRateCurrent();
            boolC = battery.LFPG_IsOutputEnabled();
        }
        bool changed = (!m_ClientSnapshotValid || deviceType != m_SnapshotDeviceType || intA != m_SnapshotIntA || intB != m_SnapshotIntB || intC != m_SnapshotIntC || boolA != m_SnapshotBoolA || boolB != m_SnapshotBoolB || boolC != m_SnapshotBoolC || floatA != m_SnapshotFloatA || floatB != m_SnapshotFloatB || floatC != m_SnapshotFloatC || floatD != m_SnapshotFloatD || floatE != m_SnapshotFloatE || floatF != m_SnapshotFloatF || floatG != m_SnapshotFloatG || snapshotEntity != m_SnapshotEntity);
        m_ClientSnapshotValid = true;
        m_SnapshotDeviceType = deviceType;
        m_SnapshotIntA = intA;
        m_SnapshotIntB = intB;
        m_SnapshotIntC = intC;
        m_SnapshotBoolA = boolA;
        m_SnapshotBoolB = boolB;
        m_SnapshotBoolC = boolC;
        m_SnapshotFloatA = floatA;
        m_SnapshotFloatB = floatB;
        m_SnapshotFloatC = floatC;
        m_SnapshotFloatD = floatD;
        m_SnapshotFloatE = floatE;
        m_SnapshotFloatF = floatF;
        m_SnapshotFloatG = floatG;
        m_SnapshotEntity = snapshotEntity;
        return changed;
    }
    protected void SetTextDirty(TextWidget widget, string value)
    {
        if (!widget)
            return;
        string previous;
        if (m_LastWidgetText.Find(widget, previous) && previous == value)
            return;
        m_LastWidgetText[widget] = value;
        widget.SetText(value);
    }
    protected void SetColorDirty(Widget widget, int value)
    {
        if (!widget)
            return;
        int previous;
        if (m_LastWidgetColor.Find(widget, previous) && previous == value)
            return;
        m_LastWidgetColor[widget] = value;
        widget.SetColor(value);
    }
    protected void ShowDirty(Widget widget, bool value)
    {
        if (!widget)
            return;
        bool previous;
        if (m_LastWidgetVisible.Find(widget, previous) && previous == value)
            return;
        m_LastWidgetVisible[widget] = value;
        widget.Show(value);
    }
    protected void SetPosDirty(Widget widget, float x, float y)
    {
        if (!widget)
            return;
        vector previous;
        if (m_LastWidgetPos.Find(widget, previous) && previous[0] == x && previous[1] == y)
            return;
        m_LastWidgetPos[widget] = Vector(x, y, 0.0);
        widget.SetPos(x, y);
    }
    protected void SetSizeDirty(Widget widget, float width, float height)
    {
        if (!widget)
            return;
        vector previous;
        if (m_LastWidgetSize.Find(widget, previous) && previous[0] == width && previous[1] == height)
            return;
        m_LastWidgetSize[widget] = Vector(width, height, 0.0);
        widget.SetSize(width, height);
    }
    protected void PopulateClientData(EntityAI device, string deviceId)
    {
        if (!m_wDeviceName || !m_wDeviceType || !m_wStatusLine || !m_wCapLine || !m_wWiresHeader)
            return;
		float previousLineOffset = GetExtraLineOffset();
        string typeName = device.GetType();
        SetTextDirty(m_wDeviceName, FormatDeviceName(typeName));
		int devType = m_SnapshotDeviceType;
        string typeStr = Loc("#STR_LFPG_INSPECT_UNKNOWN");
        int typeColor = COL_GRAY;
        if (devType == LFPG_DeviceType.SOURCE)
        {
            typeStr = Loc("#STR_LFPG_INSPECT_SOURCE");
            typeColor = COL_AMBER_SOURCE;
        }
        else if (devType == LFPG_DeviceType.CONSUMER)
        {
            typeStr = Loc("#STR_LFPG_INSPECT_CONSUMER");
            typeColor = COL_BLUE;
        }
        else if (devType == LFPG_DeviceType.PASSTHROUGH)
        {
            typeStr = Loc("#STR_LFPG_INSPECT_PASSTHROUGH");
            typeColor = COL_PURPLE;
        }
        else if (devType == LFPG_DeviceType.CAMERA)
        {
            typeStr = Loc("#STR_LFPG_INSPECT_CAMERA");
            typeColor = COL_BLUE;
        }
        SetTextDirty(m_wDeviceType, typeStr);
        SetColorDirty(m_wDeviceType, typeColor);
        string statusText = "";
        int statusColor = COL_GRAY;
        if (devType == LFPG_DeviceType.SOURCE)
        {
			bool sourceOn = m_SnapshotBoolA;
            if (sourceOn)
            {
				float loadRatio = m_SnapshotFloatA;
                int loadPct = Math.Round(loadRatio * 100.0);
                if (loadRatio >= LFPG_LOAD_CRITICAL_THRESHOLD)
                {
                    statusText = Loc("#STR_LFPG_INSPECT_OVERLOAD");
                    statusText = statusText + "  ";
                    statusColor = COL_RED_ERROR;
                }
                else
                {
                    statusText = Loc("#STR_LFPG_INSPECT_ACTIVE");
                    statusText = statusText + "  ";
                    statusColor = COL_GREEN_OK;
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
                statusColor = COL_GRAY_DIM;
            }
        }
        else if (devType == LFPG_DeviceType.PASSTHROUGH)
        {
			bool ptPowered = m_SnapshotBoolA;
            if (ptPowered)
            {
                statusText = Loc("#STR_LFPG_INSPECT_TRANSMITTING");
                statusColor = COL_GREEN_OK;
            }
            else
            {
                statusText = Loc("#STR_LFPG_INSPECT_NOT_TRANSMITTING");
                statusColor = COL_GRAY_DIM;
            }
            LFPG_MemoryCell mcInspect = LFPG_MemoryCell.Cast(device);
            if (mcInspect)
            {
				bool cellOn = m_SnapshotBoolB;
                if (ptPowered)
                {
                    if (cellOn)
                    {
                        string onTag = " — ON";
                        statusText = statusText + onTag;
                        statusColor = COL_GREEN_OK;
                    }
                    else
                    {
                        string offTag = " — OFF";
                        statusText = statusText + offTag;
                        statusColor = COL_ORANGE;
                    }
                }
            }
        }
        else
        {
			bool powered = m_SnapshotBoolA;
            if (powered)
            {
                statusText = Loc("#STR_LFPG_INSPECT_POWERED");
                statusColor = COL_GREEN_OK;
            }
            else
            {
                statusText = Loc("#STR_LFPG_INSPECT_UNPOWERED");
                statusColor = COL_GRAY_DIM;
            }
        }
        SetTextDirty(m_wStatusLine, statusText);
        SetColorDirty(m_wStatusLine, statusColor);
        string capText = "";
        if (devType == LFPG_DeviceType.SOURCE)
        {
			float cap = m_SnapshotFloatF;
            capText = Loc("#STR_LFPG_INSPECT_CAPACITY");
            capText = capText + FormatFloat1(cap);
            capText = capText + " u/s";
        }
        else if (devType == LFPG_DeviceType.CONSUMER)
        {
			float cons = m_SnapshotFloatG;
            capText = Loc("#STR_LFPG_INSPECT_CONSUMPTION");
            capText = capText + FormatFloat1(cons);
            capText = capText + " u/s";
        }
        else if (devType == LFPG_DeviceType.CAMERA)
        {
			float camCons = m_SnapshotFloatG;
            capText = Loc("#STR_LFPG_INSPECT_CONSUMPTION");
            capText = capText + FormatFloat1(camCons);
            capText = capText + " u/s";
        }
        else if (devType == LFPG_DeviceType.PASSTHROUGH)
        {
			float ptCap = m_SnapshotFloatF;
            capText = Loc("#STR_LFPG_INSPECT_THROUGHPUT");
            capText = capText + FormatFloat1(ptCap);
            capText = capText + " u/s";
			float ptOwnCons = m_SnapshotFloatG;
            float ptDownstream = 0.0;
            if (m_HasServerData && m_RespWires)
            {
                int wdi;
                for (wdi = 0; wdi < m_RespWires.Count(); wdi = wdi + 1)
                {
                    LFPG_InspectWireEntry wde = m_RespWires[wdi];
                    if (!wde)
                        continue;
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
        SetTextDirty(m_wCapLine, capText);
        if (capText != "")
        {
            ShowDirty(m_wCapLine, true);
        }
        else
        {
            ShowDirty(m_wCapLine, false);
        }
        m_TankLineOffset = 0.0;
        if (m_wTankLine)
        {
            LFPG_WaterPump_T2 t2Pump = LFPG_WaterPump_T2.Cast(device);
            if (t2Pump)
            {
				float tankLvl = m_SnapshotFloatB;
				int tankLiq = m_SnapshotIntA;
				bool tankPow = m_SnapshotBoolB;
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
                SetTextDirty(m_wTankLine, tankText);
                if (tankPow && !isFull)
                {
                    SetColorDirty(m_wTankLine, COL_CYAN);
                }
                else if (tankPow && isFull)
                {
                    SetColorDirty(m_wTankLine, COL_GREEN_OK);
                }
                else if (tankLvl < 0.01)
                {
                    SetColorDirty(m_wTankLine, COL_GRAY_DIM);
                }
                else if (tankLiq == LIQUID_CLEANWATER)
                {
                    SetColorDirty(m_wTankLine, COL_BLUE_BRIGHT);
                }
                else
                {
                    SetColorDirty(m_wTankLine, COL_OLIVE_GREEN);
                }
                ShowDirty(m_wTankLine, true);
                m_TankLineOffset = 20.0;
            }
            else
            {
                ShowDirty(m_wTankLine, false);
                m_TankLineOffset = 0.0;
            }
        }
        m_FuelLineOffset = 0.0;
        m_ReserveLineOffset = 0.0;
        if (m_wFuelLine)
        {
            LFPG_Furnace furnaceDevice = LFPG_Furnace.Cast(device);
            if (furnaceDevice)
            {
				int fuelCur = m_SnapshotIntA;
				bool fuelOn = m_SnapshotBoolB;
                int fuelMax = LFPG_FURNACE_MAX_FUEL;
                int totalSec = fuelCur * 30;
                int fuelDays = totalSec / 86400;
                int fuelHours = (totalSec % 86400) / 3600;
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
                string fuelText = "Fuel: ";
                fuelText = fuelText + fuelCur.ToString() + "/" + fuelMax.ToString();
                fuelText = fuelText + " (" + fuelPctW.ToString() + "." + fuelPctT.ToString() + "%)";
                fuelText = fuelText + " | " + fuelDays.ToString() + "D " + fuelHours.ToString() + "H";
				int cargoCount = m_SnapshotIntB;
                int fuelLineColor = COL_GRAY_DIM;
                if (fuelOn)
                {
                    fuelLineColor = COL_CYAN;
                }
                else if (fuelCur > 0)
                {
                    fuelText = fuelText + " [OFF]";
                    fuelLineColor = COL_AMBER_WARN;
                }
                else if (cargoCount > 0)
                {
                    fuelText = fuelText + " [OFF]";
                    fuelLineColor = COL_AMBER_FUEL;
                }
                else
                {
                    fuelText = fuelText + " [EMPTY]";
                }
                SetColorDirty(m_wFuelLine, fuelLineColor);
                SetTextDirty(m_wFuelLine, fuelText);
                ShowDirty(m_wFuelLine, true);
                m_FuelLineOffset = 20.0;
                if (m_wReserveLine)
                {
                    if (cargoCount > 0)
                    {
						int cargoFuel = m_SnapshotIntC;
                        int resSec = cargoFuel * 30;
                        int resDays = resSec / 86400;
                        int resHours = (resSec % 86400) / 3600;
                        string resText = "Reserve: ";
                        resText = resText + cargoCount.ToString();
                        resText = resText + " | " + resDays.ToString() + "D " + resHours.ToString() + "H approx";
                        SetTextDirty(m_wReserveLine, resText);
                        SetColorDirty(m_wReserveLine, fuelLineColor);
                        SetPosDirty(m_wReserveLine, 14, 94 + m_FuelLineOffset);
                        ShowDirty(m_wReserveLine, true);
                        m_ReserveLineOffset = 20.0;
                    }
                    else
                    {
                        ShowDirty(m_wReserveLine, false);
                        m_ReserveLineOffset = 0.0;
                    }
                }
            }
            else
            {
                ShowDirty(m_wFuelLine, false);
                m_FuelLineOffset = 0.0;
                if (m_wReserveLine)
                {
                    ShowDirty(m_wReserveLine, false);
                    m_ReserveLineOffset = 0.0;
                }
            }
        }
        m_LinkLineOffset = 0.0;
        if (m_wLinkLine)
        {
            LFPG_Sorter sorterInspect = LFPG_Sorter.Cast(device);
            if (sorterInspect)
            {
                float linkY = 94.0 + m_TankLineOffset + m_FuelLineOffset + m_ReserveLineOffset;
                SetPosDirty(m_wLinkLine, 14, linkY);
				EntityAI linkedEnt = m_SnapshotEntity;
                if (linkedEnt)
                {
                    string linkText = "Linked: ";
                    linkText = linkText + linkedEnt.GetDisplayName();
                    SetTextDirty(m_wLinkLine, linkText);
                    SetColorDirty(m_wLinkLine, COL_EMERALD);
                }
                else
                {
                    string noLinkText = "Not linked";
                    SetTextDirty(m_wLinkLine, noLinkText);
                    SetColorDirty(m_wLinkLine, COL_RED_SOFT);
                }
                ShowDirty(m_wLinkLine, true);
                m_LinkLineOffset = 20.0;
            }
            else
            {
                ShowDirty(m_wLinkLine, false);
            }
        }
        m_BatteryLineOffset = 0.0;
        if (m_wBatteryLine)
        {
            LFPG_BatteryBase batDevice = LFPG_BatteryBase.Cast(device);
            if (batDevice)
            {
				float batStored = m_SnapshotFloatC;
				float batMax = m_SnapshotFloatD;
				float batRate = m_SnapshotFloatE;
				bool batOutEnabled = m_SnapshotBoolC;
                int batPct = 0;
                if (batMax > 0.0)
                {
                    batPct = (batStored / batMax) * 100.0;
                }
                if (batPct > 100)
                {
                    batPct = 100;
                }
                int batStoredInt = batStored;
                int batMaxInt = batMax;
                string batText = "Charge: ";
                batText = batText + batStoredInt.ToString();
                batText = batText + "/";
                batText = batText + batMaxInt.ToString();
                batText = batText + " (";
                batText = batText + batPct.ToString();
                batText = batText + "%)";
                int batColor = COL_GRAY_DIM;
                if (!batOutEnabled)
                {
                    batText = batText + "  [OFF]";
                    batColor = COL_OLIVE;
                }
                else if (batRate > 0.5)
                {
                    int chgRate = batRate;
                    batText = batText + "  >> CHG +";
                    batText = batText + chgRate.ToString();
                    batText = batText + " u/s";
                    batColor = COL_CYAN;
                }
                else if (batRate < -0.5)
                {
                    float absRate = -batRate;
                    int disRate = absRate;
                    batText = batText + "  << DIS -";
                    batText = batText + disRate.ToString();
                    batText = batText + " u/s";
                    batColor = COL_ORANGE;
                }
                else if (batPct >= 100)
                {
                    batText = batText + "  FULL";
                    batColor = COL_GREEN_OK;
                }
                else if (batPct < 1)
                {
                    batText = batText + "  EMPTY";
                    batColor = COL_RED_DARK;
                }
                else
                {
                    batText = batText + "  IDLE";
                    batColor = COL_GRAY_MID;
                }
                float batY = 94.0 + m_TankLineOffset + m_FuelLineOffset + m_ReserveLineOffset + m_LinkLineOffset;
                SetPosDirty(m_wBatteryLine, 14, batY);
                SetTextDirty(m_wBatteryLine, batText);
                SetColorDirty(m_wBatteryLine, batColor);
                ShowDirty(m_wBatteryLine, true);
                m_BatteryLineOffset = 26.0;
            }
            else
            {
                ShowDirty(m_wBatteryLine, false);
                m_BatteryLineOffset = 0.0;
            }
        }
		float extraLineOffset = GetExtraLineOffset();
		if (extraLineOffset != previousLineOffset)
		{
			m_WireDataDirty = true;
		}
        if (m_wSeparator)
        {
            SetPosDirty(m_wSeparator, 12, 93 + extraLineOffset);
        }
        if (m_wWiresHeader)
        {
            SetPosDirty(m_wWiresHeader, 14, 99 + extraLineOffset);
        }
        if (m_WireDataDirty)
        {
            if (m_HasServerData)
            {
                PopulateWireData();
            }
            else
            {
                ShowDirty(m_wSeparator, true);
                ShowDirty(m_wWiresHeader, true);
                SetTextDirty(m_wWiresHeader, Loc("#STR_LFPG_INSPECT_CONN_LOADING"));
                HideAllWireSlots();
                ResizePanelHeight(0);
            }
            m_WireDataDirty = false;
        }
    }
    static void OnInspectResponse(string deviceId, array<ref LFPG_InspectWireEntry> wires)
    {
        LFPG_DeviceInspector inst = Get();
        if (!inst.m_Root)
            return;
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
        inst.m_InspectRequestAttempts = 0;
        inst.m_WireDataDirty = true;
        inst.m_RespWires.Clear();
        int wi;
        for (wi = 0; wi < wires.Count(); wi = wi + 1)
        {
            inst.m_RespWires.Insert(wires[wi]);
        }
        EntityAI respTarget = LFPG_DeviceRegistry.Get().FindById(deviceId);
        if (respTarget)
        {
			inst.ClientDataChanged(respTarget);
            inst.PopulateClientData(respTarget, deviceId);
        }
        else
        {
            inst.PopulateWireData();
            inst.m_WireDataDirty = false;
        }
    }
	protected float GetExtraLineOffset()
	{
		return m_TankLineOffset + m_FuelLineOffset + m_ReserveLineOffset + m_LinkLineOffset + m_BatteryLineOffset;
	}
    protected void PopulateWireData()
    {
        if (!m_wWiresHeader)
            return;
        int wireCount = m_RespWires.Count();
        EntityAI inspectEnt = null;
        if (m_CurrentDeviceId != "")
        {
            inspectEnt = LFPG_DeviceRegistry.Get().FindById(m_CurrentDeviceId);
        }
        int freeCount = 0;
        int portCount = 0;
        if (inspectEnt)
        {
            portCount = LFPG_DeviceAPI.GetPortCount(inspectEnt);
        }
        int pi;
        for (pi = 0; pi < portCount; pi = pi + 1)
        {
            string declaredName = LFPG_DeviceAPI.GetPortName(inspectEnt, pi);
            if (declaredName == "")
                continue;
            if (IsLocalPortOccupied(declaredName))
                continue;
            freeCount = freeCount + 1;
        }
        if (wireCount == 0 && freeCount == 0)
        {
            ShowDirty(m_wWiresHeader, false);
            if (m_wSeparator)
            {
                ShowDirty(m_wSeparator, false);
            }
            HideAllWireSlots();
            m_VisibleWireCount = 0;
            ResizePanelCompact();
            return;
        }
		float extraLineOffset = GetExtraLineOffset();
        if (m_wSeparator)
        {
            ShowDirty(m_wSeparator, true);
			SetPosDirty(m_wSeparator, 12, 93 + extraLineOffset);
        }
        ShowDirty(m_wWiresHeader, true);
		SetPosDirty(m_wWiresHeader, 14, 99 + extraLineOffset);
        int ri;
        for (ri = 0; ri < m_wWireSlots.Count(); ri = ri + 1)
        {
            TextWidget rSlot = m_wWireSlots[ri];
            if (rSlot)
            {
				float rY = LFPG_INSPECT_PANEL_BASE_H + 2.0 + extraLineOffset + (ri * LFPG_INSPECT_WIRE_ROW_H);
                SetPosDirty(rSlot, 14, rY);
            }
        }
        int totalRows = wireCount + freeCount;
        int maxShow = m_wWireSlots.Count();
        if (totalRows < maxShow)
        {
            maxShow = totalRows;
        }
        string hdrText = Loc("#STR_LFPG_INSPECT_CONNECTIONS");
        hdrText = hdrText + " (";
        hdrText = hdrText + totalRows.ToString();
        if (totalRows > maxShow)
        {
            hdrText = hdrText + " | ";
            hdrText = hdrText + Loc("#STR_LFPG_INSPECT_SHOWING");
            hdrText = hdrText + " ";
            hdrText = hdrText + maxShow.ToString();
        }
        hdrText = hdrText + ")";
        SetTextDirty(m_wWiresHeader, hdrText);
        int wireShow = wireCount;
        if (wireShow > maxShow)
        {
            wireShow = maxShow;
        }
        int si;
        for (si = 0; si < wireShow; si = si + 1)
        {
            LFPG_InspectWireEntry entry = m_RespWires[si];
            TextWidget slot = m_wWireSlots[si];
            if (!slot)
                continue;
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
            line = line + ResolvePortDisplayLabel(inspectEnt, entry.m_LocalPort);
            line = line + "  >  ";
            line = line + FormatDeviceName(entry.m_RemoteTypeName);
            if (entry.m_AllocatedPower > LFPG_PROPAGATION_EPSILON)
            {
                line = line + "  · ";
                line = line + FormatFloat1(entry.m_AllocatedPower);
                line = line + " u/s";
            }
            SetTextDirty(slot, line);
            int wireColor = COL_BLUE_WIRE;
            if (entry.m_EdgeState == 2)
            {
                wireColor = COL_RED_ORANGE;
            }
            else if (entry.m_Direction == LFPG_PortDir.OUT)
            {
                wireColor = COL_GREEN_WIRE;
            }
            SetColorDirty(slot, wireColor);
            ShowDirty(slot, true);
        }
        int slotIdx = wireShow;
        for (pi = 0; pi < portCount; pi = pi + 1)
        {
            if (slotIdx >= maxShow)
                break;
            string freeName = LFPG_DeviceAPI.GetPortName(inspectEnt, pi);
            if (freeName == "")
                continue;
            if (IsLocalPortOccupied(freeName))
                continue;
            TextWidget freeSlot = m_wWireSlots[slotIdx];
            if (!freeSlot)
            {
                slotIdx = slotIdx + 1;
                continue;
            }
            int freeDir = LFPG_DeviceAPI.GetPortDir(inspectEnt, pi);
            string freeArrow = "";
            if (freeDir == LFPG_PortDir.OUT)
            {
                freeArrow = Loc("#STR_LFPG_INSPECT_DIR_OUT");
                freeArrow = freeArrow + " ";
            }
            else
            {
                freeArrow = Loc("#STR_LFPG_INSPECT_DIR_IN");
                freeArrow = freeArrow + "  ";
            }
            string freeLine = freeArrow;
            freeLine = freeLine + ResolvePortDisplayLabel(inspectEnt, freeName);
            freeLine = freeLine + "  >  ";
            freeLine = freeLine + Loc("#STR_LFPG_INSPECT_PORT_EMPTY");
            SetTextDirty(freeSlot, freeLine);
            SetColorDirty(freeSlot, COL_GRAY_DIM);
            ShowDirty(freeSlot, true);
            slotIdx = slotIdx + 1;
        }
        int hi;
        for (hi = maxShow; hi < m_wWireSlots.Count(); hi = hi + 1)
        {
            TextWidget hideSlot = m_wWireSlots[hi];
            if (hideSlot)
            {
                ShowDirty(hideSlot, false);
            }
        }
        m_VisibleWireCount = maxShow;
        ResizePanelHeight(maxShow);
    }
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
		h = h + GetExtraLineOffset();
        ApplyPanelSize(h);
    }
    protected void ResizePanelCompact()
    {
		ApplyPanelSize(LFPG_INSPECT_COMPACT_H + GetExtraLineOffset());
    }
    protected void ApplyPanelSize(float h)
    {
        if (!m_Panel)
            return;
        m_CurrentPanelH = h;
        SetSizeDirty(m_Panel, LFPG_INSPECT_PANEL_W, h);
        if (m_wPanelBg)
        {
            SetSizeDirty(m_wPanelBg, LFPG_INSPECT_PANEL_W, h);
        }
        if (m_wAccentBar)
        {
            SetSizeDirty(m_wAccentBar, LFPG_INSPECT_ACCENT_W, h);
        }
    }
    protected bool UpdatePanelPosition(EntityAI device)
    {
        if (!m_Panel || !device)
            return false;
        vector worldPos = device.GetPosition();
        worldPos[1] = worldPos[1] + LFPG_INSPECT_WORLD_Y_OFFSET;
        vector screenPos = g_Game.GetScreenPos(worldPos);
        if (screenPos[2] < LFPG_BEHIND_CAM_Z)
        {
            return false;
        }
        int screenW = 0;
        int screenH = 0;
        GetScreenSize(screenW, screenH);
        float px = screenPos[0] + LFPG_INSPECT_OFFSET_X;
        float py = screenPos[1] + LFPG_INSPECT_OFFSET_Y;
        float panelW = LFPG_INSPECT_PANEL_W;
        float panelH = m_CurrentPanelH;
        if (panelH < 1.0)
        {
            panelH = ComputePanelHeight(m_VisibleWireCount) + m_TankLineOffset + m_FuelLineOffset + m_ReserveLineOffset + m_BatteryLineOffset;
        }
        float fScreenW = screenW;
        float fScreenH = screenH;
        float rightEdge = px + panelW;
        if (!m_FlippedLeft)
        {
            if (rightEdge > fScreenW - LFPG_INSPECT_SCREEN_MARGIN)
            {
                m_FlippedLeft = true;
            }
        }
        else
        {
            if (rightEdge < fScreenW - LFPG_INSPECT_SCREEN_MARGIN - LFPG_INSPECT_FLIP_HYSTERESIS)
            {
                m_FlippedLeft = false;
            }
        }
        if (m_FlippedLeft)
        {
            px = screenPos[0] - panelW - LFPG_INSPECT_OFFSET_X;
        }
        if (py < LFPG_INSPECT_SCREEN_MARGIN)
        {
            py = LFPG_INSPECT_SCREEN_MARGIN;
        }
        if (py + panelH > fScreenH - LFPG_INSPECT_SCREEN_MARGIN)
        {
            py = fScreenH - panelH - LFPG_INSPECT_SCREEN_MARGIN;
        }
        if (px < LFPG_INSPECT_SCREEN_MARGIN)
        {
            px = LFPG_INSPECT_SCREEN_MARGIN;
        }
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
        SetPosDirty(m_Panel, m_SmoothX, m_SmoothY);
        return true;
    }
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
            m_InspectRequestAttempts = 0;
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
                ShowDirty(tw, false);
            }
        }
        m_VisibleWireCount = 0;
    }
    protected void RequestServerData(PlayerBase player, string deviceId, EntityAI targetEntity)
    {
        if (!player)
            return;
        float nowMs = g_Game.GetTime();
        float elapsed = nowMs - m_LastRPCSendMs;
        if (elapsed < LFPG_INSPECT_RPC_COOLDOWN_MS)
            return;
        m_LastRPCSendMs = nowMs;
        int netLow = 0;
        int netHigh = 0;
        if (targetEntity)
        {
            targetEntity.GetNetworkID(netLow, netHigh);
        }
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(LFPG_RPC_SubId.INSPECT_DEVICE);
        rpc.Write(netLow);
        rpc.Write(netHigh);
        rpc.Write(deviceId);
        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);
        m_InspectRequestAttempts = m_InspectRequestAttempts + 1;
        LFPG_Util.Debug("[DeviceInspector] Sent INSPECT_DEVICE for " + deviceId);
    }
    protected static string FormatDeviceName(string typeName)
    {
        if (typeName == "")
            return "Unknown";
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
        if (result.Length() > 4)
        {
            string testPfx = result.Substring(0, 4);
            if (testPfx == "Test")
            {
                result = result.Substring(4, result.Length() - 4);
            }
        }
        if (result == "")
        {
            result = typeName;
        }
        return result;
    }
    protected static string Loc(string key)
    {
        return Widget.TranslateString(key);
    }
    protected bool IsLocalPortOccupied(string portName)
    {
        string want = portName;
        if (want == "")
            want = "input_main";
        int i;
        int n = m_RespWires.Count();
        for (i = 0; i < n; i = i + 1)
        {
            LFPG_InspectWireEntry entry = m_RespWires[i];
            if (!entry)
                continue;
            string have = entry.m_LocalPort;
            if (have == "")
                have = "input_main";
            if (have == want)
                return true;
        }
        return false;
    }
    protected static string ResolvePortDisplayLabel(EntityAI device, string portName)
    {
        if (device)
        {
            int i;
            int n = LFPG_DeviceAPI.GetPortCount(device);
            for (i = 0; i < n; i = i + 1)
            {
                string declaredName = LFPG_DeviceAPI.GetPortName(device, i);
                if (declaredName != portName)
                    continue;
                string declaredLabel = LFPG_DeviceAPI.GetPortLabel(device, i);
                if (declaredLabel != "")
                    return declaredLabel;
                break;
            }
        }
        return FormatPortName(portName);
    }
    protected static string FormatPortName(string portName)
    {
        if (portName == "")
            return "—";
        if (portName == "input_main")
            return Loc("#STR_LFPG_INSPECT_PORT_MAIN_IN");
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
        if (portLen > 7)
        {
            string pfxOut = portName.Substring(0, 7);
            if (pfxOut == "output_")
            {
                string outSuffix = portName.Substring(7, portLen - 7);
                return Loc("#STR_LFPG_INSPECT_PORT_OUTPUT") + " " + outSuffix;
            }
        }
        return portName;
    }
    protected static string FormatFloat1(float val)
    {
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
#endif
