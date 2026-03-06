// =========================================================
// LF_PowerGrid - CCTV Viewport Manager (v0.9.7)
//
// v0.9.7:
//   - CreateWidgets ELIMINADO: crash nativo con staticcamera.
//   - ChangeGameFocus ELIMINADO: bloquea LocalPress sin suprimir controles.
//   - ResetGameFocus() llamado tras SetActive(true) para limpiar
//     cualquier focus lock que el engine imponga al activar staticcamera.
//   - Input: LocalValue edge detect como metodo principal.
//   - Timeout 30s como mecanismo de salida garantizado.
//   - m_ExitedThisFrame flag para que MissionInit sepa si debe
//     saltar DeviceInspector en el frame del exit.
// =========================================================

static const float LFPG_CCTV_SCANLINE_SPACING = 5.0;
static const float LFPG_CCTV_SCANLINE_ALPHA   = 0.32;
static const float LFPG_CCTV_SCROLL_SPEED     = 20.0;
static const float LFPG_CCTV_VIGNETTE_ALPHA   = 0.60;
static const float LFPG_CCTV_VIGNETTE_W       = 55.0;
static const float LFPG_CCTV_MAX_DURATION_S   = 30.0;

class LFPG_CameraViewport
{
    protected static ref LFPG_CameraViewport s_Instance;

    protected Object    m_ViewCamObj;
    protected bool      m_Active;
    protected float     m_ScanlineOffset;
    protected float     m_ActiveDuration;

    protected ref array<ref LFPG_CameraListEntry> m_CameraList;
    protected int       m_CameraIndex;
    protected int       m_CameraTotal;
    protected string    m_CameraLabel;

    protected int       m_ScanColor;
    protected int       m_VigColor;

    // Edge detect state
    protected bool      m_PrevSpace;
    protected bool      m_PrevE;
    protected bool      m_PrevQ;

    // Flag para MissionInit: skip DeviceInspector en frame de exit
    protected bool      m_ExitedThisFrame;

    void LFPG_CameraViewport()
    {
        m_ViewCamObj      = null;
        m_Active          = false;
        m_ScanlineOffset  = 0.0;
        m_CameraLabel     = "";
        m_ActiveDuration  = 0.0;
        m_CameraList      = null;
        m_CameraIndex     = 0;
        m_CameraTotal     = 0;
        m_PrevSpace       = false;
        m_PrevE           = false;
        m_PrevQ           = false;
        m_ExitedThisFrame = false;

        int scanAlphaI = LFPG_CCTV_SCANLINE_ALPHA * 255.0;
        int vigAlphaI  = LFPG_CCTV_VIGNETTE_ALPHA * 255.0;
        m_ScanColor    = ARGB(scanAlphaI, 0, 15, 0);
        m_VigColor     = ARGB(vigAlphaI,  0, 0, 0);
    }

    static LFPG_CameraViewport Get()
    {
        if (GetGame().IsDedicatedServer())
            return null;

        if (!s_Instance)
            s_Instance = new LFPG_CameraViewport();
        return s_Instance;
    }

    static void Reset()
    {
        if (s_Instance)
        {
            s_Instance.Exit();
            delete s_Instance;
            s_Instance = null;
        }
    }

    bool IsActive()
    {
        return m_Active;
    }

    bool DidExitThisFrame()
    {
        return m_ExitedThisFrame;
    }

    protected void HideAllUI()
    {
        Mission mission = GetGame().GetMission();
        if (mission)
        {
            Hud hud = mission.GetHud();
            if (hud)
            {
                hud.ShowHudPlayer(false);
                hud.ShowQuickbarPlayer(false);
            }
        }

        UIManager uiMgr = GetGame().GetUIManager();
        if (uiMgr)
        {
            uiMgr.ShowUICursor(false);
        }
    }

    protected void RestoreAllUI()
    {
        Mission mission = GetGame().GetMission();
        if (mission)
        {
            Hud hud = mission.GetHud();
            if (hud)
            {
                hud.ShowHudPlayer(true);
                hud.ShowQuickbarPlayer(true);
            }
        }
    }

    void EnterFromList(array<ref LFPG_CameraListEntry> entries)
    {
        PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());

        if (!entries)
        {
            LFPG_Util.Warn("[CameraViewport] EnterFromList: entries null");
            return;
        }

        if (entries.Count() == 0)
        {
            if (p)
                p.MessageStatus("[LFPG] No hay camaras activas conectadas.");
            return;
        }

        if (m_Active)
            Exit();

        m_CameraList      = entries;
        m_CameraTotal     = entries.Count();
        m_CameraIndex     = 0;
        m_ExitedThisFrame = false;

        bool ok = EnterCamera(0);
        if (!ok)
        {
            m_CameraList  = null;
            m_CameraTotal = 0;
            return;
        }

        m_Active         = true;
        m_ScanlineOffset = 0.0;
        m_ActiveDuration = 0.0;
        m_PrevSpace      = false;
        m_PrevE          = false;
        m_PrevQ          = false;

        HideAllUI();

        // v0.9.7: Limpiar cualquier game focus que el engine haya puesto
        // al activar staticcamera. Sin esto, el Input API no detecta teclas.
        Input inp = GetGame().GetInput();
        if (inp)
        {
            inp.ResetGameFocus();
        }

        if (p)
        {
            string totalStr = m_CameraTotal.ToString();
            string enterMsg = "[LFPG] ";
            enterMsg = enterMsg + m_CameraLabel;
            enterMsg = enterMsg + " (1/";
            enterMsg = enterMsg + totalStr;
            enterMsg = enterMsg + ")  SPACE=Salir  Q/E=Ciclar";
            p.MessageStatus(enterMsg);
        }

        string logEntry = "[CameraViewport] EnterFromList: ";
        logEntry = logEntry + m_CameraTotal.ToString();
        logEntry = logEntry + " cameras, active: ";
        logEntry = logEntry + m_CameraLabel;
        LFPG_Util.Info(logEntry);
    }

    protected bool EnterCamera(int index)
    {
        if (!m_CameraList)
            return false;
        if (index < 0 || index >= m_CameraList.Count())
            return false;

        LFPG_CameraListEntry entry = m_CameraList[index];
        if (!entry)
            return false;

        vector camPos = entry.m_Pos;
        vector camOri = entry.m_Ori;
        m_CameraLabel = entry.m_Label;

        if (m_ViewCamObj)
        {
            m_ViewCamObj.SetPosition(camPos);
            m_ViewCamObj.SetOrientation(camOri);
            m_CameraIndex = index;
            return true;
        }

        Object viewCam = GetGame().CreateObject("staticcamera", camPos, true, false, false);
        if (!viewCam)
        {
            LFPG_Util.Error("[CameraViewport] Fallo CreateObject staticcamera");
            return false;
        }

        viewCam.SetOrientation(camOri);

        Camera viewCamTyped = Camera.Cast(viewCam);
        if (!viewCamTyped)
        {
            LFPG_Util.Error("[CameraViewport] staticcamera no casteable a Camera");
            GetGame().ObjectDelete(viewCam);
            return false;
        }

        viewCamTyped.SetActive(true);

        m_ViewCamObj  = viewCam;
        m_CameraIndex = index;
        return true;
    }

    void CycleNext()
    {
        if (!m_Active)
            return;
        if (m_CameraTotal <= 1)
            return;

        int nextIdx = m_CameraIndex + 1;
        if (nextIdx >= m_CameraTotal)
            nextIdx = 0;

        bool ok = EnterCamera(nextIdx);
        if (ok)
        {
            ShowCycleMessage();
        }
    }

    void CyclePrev()
    {
        if (!m_Active)
            return;
        if (m_CameraTotal <= 1)
            return;

        int prevIdx = m_CameraIndex - 1;
        if (prevIdx < 0)
            prevIdx = m_CameraTotal - 1;

        bool ok = EnterCamera(prevIdx);
        if (ok)
        {
            ShowCycleMessage();
        }
    }

    protected void ShowCycleMessage()
    {
        PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
        if (!p)
            return;

        int displayIdx = m_CameraIndex + 1;
        string idxStr = displayIdx.ToString();
        string totalStr = m_CameraTotal.ToString();
        string msg = "[LFPG] ";
        msg = msg + m_CameraLabel;
        msg = msg + " (";
        msg = msg + idxStr;
        msg = msg + "/";
        msg = msg + totalStr;
        msg = msg + ")";
        p.MessageStatus(msg);
    }

    void Exit()
    {
        if (!m_Active)
            return;

        RestoreAllUI();

        m_Active          = false;
        m_ExitedThisFrame = true;

        if (m_ViewCamObj)
        {
            Camera viewCamTyped = Camera.Cast(m_ViewCamObj);
            if (viewCamTyped)
            {
                viewCamTyped.SetActive(false);
            }
            GetGame().ObjectDelete(m_ViewCamObj);
            m_ViewCamObj = null;
        }

        m_CameraLabel    = "";
        m_ActiveDuration = 0.0;
        m_ScanlineOffset = 0.0;
        m_CameraList     = null;
        m_CameraIndex    = 0;
        m_CameraTotal    = 0;
        m_PrevSpace      = false;
        m_PrevE          = false;
        m_PrevQ          = false;

        LFPG_Util.Info("[CameraViewport] Viewport cerrado.");
    }

    void Tick(float timeslice)
    {
        m_ExitedThisFrame = false;

        if (!m_Active)
            return;

        m_ActiveDuration = m_ActiveDuration + timeslice;
        if (m_ActiveDuration >= LFPG_CCTV_MAX_DURATION_S)
        {
            LFPG_Util.Info("[CameraViewport] Auto-exit (timeout 30s)");
            Exit();
            return;
        }

        Input inp = GetGame().GetInput();
        if (!inp)
            return;

        // SPACE = salir (LocalValue edge detect)
        float spaceVal = inp.LocalValue("UAJump");
        bool spaceCur = false;
        if (spaceVal > 0.5)
        {
            spaceCur = true;
        }
        if (spaceCur && !m_PrevSpace)
        {
            LFPG_Util.Info("[CameraViewport] EXIT via SPACE");
            Exit();
            m_PrevSpace = false;
            return;
        }
        m_PrevSpace = spaceCur;

        // E = siguiente camara
        float eVal = inp.LocalValue("UAPeekRight");
        bool eCur = false;
        if (eVal > 0.5)
        {
            eCur = true;
        }
        if (eCur && !m_PrevE)
        {
            CycleNext();
        }
        m_PrevE = eCur;

        // Q = camara anterior
        float qVal = inp.LocalValue("UAPeekLeft");
        bool qCur = false;
        if (qVal > 0.5)
        {
            qCur = true;
        }
        if (qCur && !m_PrevQ)
        {
            CyclePrev();
        }
        m_PrevQ = qCur;

        // Avanzar scanlines
        m_ScanlineOffset = m_ScanlineOffset + (LFPG_CCTV_SCROLL_SPEED * timeslice);
        while (m_ScanlineOffset >= LFPG_CCTV_SCANLINE_SPACING)
        {
            m_ScanlineOffset = m_ScanlineOffset - LFPG_CCTV_SCANLINE_SPACING;
        }
    }

    void DrawOverlay(LFPG_CableHUD hud)
    {
        if (!m_Active)
            return;

        if (!hud || !hud.IsReady())
            return;

        float sw = hud.GetScreenW();
        float sh = hud.GetScreenH();

        if (sw <= 0.0 || sh <= 0.0)
            return;

        float lineY = m_ScanlineOffset;
        while (lineY < sh)
        {
            hud.DrawLineScreen(0.0, lineY, sw, lineY, 1.0, m_ScanColor);
            lineY = lineY + LFPG_CCTV_SCANLINE_SPACING;
        }

        float vwScale = sh / 1080.0;
        float vw    = LFPG_CCTV_VIGNETTE_W * vwScale;
        float vhalf = vw * 0.5;

        hud.DrawLineScreen(vhalf,      0.0,  vhalf,      sh,   vw, m_VigColor);
        hud.DrawLineScreen(sw - vhalf, 0.0,  sw - vhalf, sh,   vw, m_VigColor);
        hud.DrawLineScreen(0.0,        vhalf, sw,         vhalf, vw, m_VigColor);
        hud.DrawLineScreen(0.0,        sh - vhalf, sw, sh - vhalf, vw, m_VigColor);
    }
}
