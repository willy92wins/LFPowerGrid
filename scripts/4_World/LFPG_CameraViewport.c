// =========================================================
// LF_PowerGrid - CCTV Viewport Manager (v0.9.2 - Sprint B)
//
// Singleton CLIENT-ONLY. Gestiona la transicion al POV de una
// camara de seguridad vinculada a un LF_Monitor.
//
// Sprint B: Server-authoritative camera list + in-viewport cycling.
//   - EnterFromList(array<LFPG_CameraListEntry>) reemplaza Enter(LF_Monitor)
//   - Q/E ciclan entre camaras (sin RPC adicional)
//   - SPACE sale del viewport
//
// Flujo de entrada (HandleLFPG_CameraListResponse → EnterFromList):
//   1. Server resolvio cameras powered → envio lista al cliente
//   2. EnterFromList almacena lista → EnterCamera(0)
//   3. EnterCamera: CreateObject staticcamera + SetActive(true)
//   4. Activa overlay: scanlines + vignette + indicador REC
//
// Flujo de ciclado (Q/E en Tick):
//   1. CycleNext/CyclePrev avanza indice circular
//   2. Reposiciona staticcamera existente (sin destroy/create)
//   3. Muestra MessageStatus con label "CAM X/Y"
//
// Flujo de salida (SPACE, timeout o Exit() directo):
//   1. Camera.SetActive(false) → motor restaura POV del jugador
//   2. ObjectDelete del staticcamera local
//   3. Limpia lista y estado
//
// Overlay (DrawOverlay, llamado por MissionGameplay.OnUpdate):
//   - Lineas horizontales semi-transparentes (efecto CRT)
//   - Desplazamiento vertical lento para animacion de scanlines
//   - Vignette oscura en los 4 bordes (marco CRT)
//   - Indicador REC parpadeante en esquina superior derecha
//
// ENFORCE SCRIPT NOTES:
//   - No foreach, no ++/--, no ternario, no multilinea en params
//   - cast implicito float→int via asignacion directa
//   - Hoisting de variables antes de if/else
//   - create_local=true en CreateObject (objeto solo en este cliente)
// =========================================================

static const float LFPG_CCTV_SCANLINE_SPACING = 5.0;   // px entre lineas
static const float LFPG_CCTV_SCANLINE_ALPHA   = 0.32;  // 0-1 opacidad lineas (verde oscuro CCTV)
static const float LFPG_CCTV_SCROLL_SPEED     = 20.0;  // px/s desplazamiento
static const float LFPG_CCTV_VIGNETTE_ALPHA   = 0.60;  // 0-1 opacidad vignette
static const float LFPG_CCTV_VIGNETTE_W       = 55.0;  // px grosor borde
static const float LFPG_CCTV_MAX_DURATION_S   = 120.0; // auto-exit tras 2 min

class LFPG_CameraViewport
{
    // ---- Singleton ----
    protected static ref LFPG_CameraViewport s_Instance;

    // ---- Estado ----
    protected Object    m_ViewCamObj;      // staticcamera local creado en EnterCamera
    protected bool      m_Active;
    protected float     m_ScanlineOffset; // px: posicion actual del scroll
    protected float     m_ActiveDuration; // segundos desde Enter

    // ---- Sprint B: camera list + cycling ----
    protected ref array<ref LFPG_CameraListEntry> m_CameraList;
    protected int       m_CameraIndex;   // indice actual en m_CameraList
    protected int       m_CameraTotal;   // cache de m_CameraList.Count()
    protected string    m_CameraLabel;   // etiqueta actual (e.g. "CAM-XXXXXX")

    // ---- Input caches (lazy init en primer Tick) ----
    protected UAInput   m_ExitInput;     // SPACE (UAJump)
    protected UAInput   m_NextInput;     // E (UAPeekRight)
    protected UAInput   m_PrevInput;     // Q (UAPeekLeft)

    // ---- Colores precalculados ----
    protected int       m_ScanColor;
    protected int       m_VigColor;

    // ---- Overlay widgets (layout-based) ----
    protected Widget       m_OverlayRoot;
    protected TextWidget   m_LabelWidget;
    protected TextWidget   m_RecWidget;
    protected TextWidget   m_TimestampWidget;

    void LFPG_CameraViewport()
    {
        m_ViewCamObj     = null;
        m_Active         = false;
        m_ScanlineOffset = 0.0;
        m_CameraLabel    = "";
        m_ActiveDuration = 0.0;
        m_CameraList     = null;
        m_CameraIndex    = 0;
        m_CameraTotal    = 0;
        m_ExitInput      = null;
        m_NextInput      = null;
        m_PrevInput      = null;

        m_OverlayRoot     = null;
        m_LabelWidget     = null;
        m_RecWidget       = null;
        m_TimestampWidget = null;

        // Precomputar colores — cast float→int implicito (Enforce Script).
        int scanAlphaI = LFPG_CCTV_SCANLINE_ALPHA * 255.0;
        int vigAlphaI  = LFPG_CCTV_VIGNETTE_ALPHA * 255.0;
        m_ScanColor    = ARGB(scanAlphaI, 0, 15, 0);
        m_VigColor     = ARGB(vigAlphaI,  0, 0, 0);
    }

    // =========================================================
    // Singleton
    // =========================================================
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

    // =========================================================
    // HideAllUI — suprime input del jugador + oculta HUD vanilla
    // ChangeGameFocus(1) incrementa contador interno del motor.
    // ShowHudPlayer(false) usa capa "player hide" (no toca m_HudState).
    // =========================================================
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

        GetGame().GetInput().ChangeGameFocus(1);
        GetGame().GetUIManager().ShowUICursor(false);
    }

    // =========================================================
    // RestoreAllUI — restaura input + HUD vanilla
    // ChangeGameFocus(-1) decrementa contador.
    // ShowHudPlayer(true) es idempotente — no fuerza HUD si
    // el jugador lo habia ocultado con ~ antes de entrar.
    // =========================================================
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

        GetGame().GetInput().ChangeGameFocus(-1);
    }

    // =========================================================
    // UpdateOverlayText — actualiza label, REC parpadeo, timestamp
    // Llamado cada frame desde Tick + una vez al entrar.
    // =========================================================
    protected void UpdateOverlayText()
    {
        if (m_LabelWidget)
        {
            int displayIdx = m_CameraIndex + 1;
            string idxStr = displayIdx.ToString();
            string totalStr = m_CameraTotal.ToString();
            string labelText = m_CameraLabel;
            labelText = labelText + "  [";
            labelText = labelText + idxStr;
            labelText = labelText + "/";
            labelText = labelText + totalStr;
            labelText = labelText + "]";
            m_LabelWidget.SetText(labelText);
        }

        if (m_RecWidget)
        {
            int recSec = m_ActiveDuration;
            bool recVisible = ((recSec % 2) == 0);
            m_RecWidget.Show(recVisible);
        }

        if (m_TimestampWidget)
        {
            int year = 0;
            int month = 0;
            int day = 0;
            int hour = 0;
            int minute = 0;
            int second = 0;
            GetGame().GetWorld().GetDate(year, month, day, hour, minute, second);

            string moStr = month.ToString();
            string dStr = day.ToString();
            string hStr = hour.ToString();
            string miStr = minute.ToString();
            string sStr = second.ToString();

            if (month < 10)
                moStr = "0" + moStr;
            if (day < 10)
                dStr = "0" + dStr;
            if (hour < 10)
                hStr = "0" + hStr;
            if (minute < 10)
                miStr = "0" + miStr;
            if (second < 10)
                sStr = "0" + sStr;

            string ts = year.ToString();
            ts = ts + "-";
            ts = ts + moStr;
            ts = ts + "-";
            ts = ts + dStr;
            ts = ts + "  ";
            ts = ts + hStr;
            ts = ts + ":";
            ts = ts + miStr;
            ts = ts + ":";
            ts = ts + sStr;
            m_TimestampWidget.SetText(ts);
        }
    }

    // =========================================================
    // EnterFromList — Sprint B: llamado desde HandleLFPG_CameraListResponse
    // Recibe la lista de camaras ya validadas por el servidor.
    // =========================================================
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

        // Limpiar viewport previo si lo habia
        if (m_Active)
            Exit();

        // Almacenar lista de camaras
        m_CameraList  = entries;
        m_CameraTotal = entries.Count();
        m_CameraIndex = 0;

        // Activar viewport con la primera camara
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

        // Suprimir input del jugador + ocultar HUD vanilla
        HideAllUI();

        // Crear overlay layout (label, REC, timestamp)
        string overlayPath = "LFPowerGrid/gui/layouts/LFPG_CameraOverlay.layout";
        m_OverlayRoot = GetGame().GetWorkspace().CreateWidgets(overlayPath);
        if (m_OverlayRoot)
        {
            string wCamLabel = "CamLabel";
            string wRecLabel = "RecLabel";
            string wTimestamp = "TimestampLabel";
            m_LabelWidget     = TextWidget.Cast(m_OverlayRoot.FindAnyWidget(wCamLabel));
            m_RecWidget       = TextWidget.Cast(m_OverlayRoot.FindAnyWidget(wRecLabel));
            m_TimestampWidget = TextWidget.Cast(m_OverlayRoot.FindAnyWidget(wTimestamp));
            int overlaySort = 11000;
            m_OverlayRoot.Show(true);
            m_OverlayRoot.SetSort(overlaySort);
            UpdateOverlayText();
        }

        // Feedback al jugador
        if (p)
        {
            string totalStr = m_CameraTotal.ToString();
            string enterMsg = "[LFPG] ";
            enterMsg = enterMsg + m_CameraLabel;
            enterMsg = enterMsg + " (1/";
            enterMsg = enterMsg + totalStr;
            enterMsg = enterMsg + ")  Q/E=Ciclar  SPACE=Salir";
            p.MessageStatus(enterMsg);
        }

        string logEntry = "[CameraViewport] EnterFromList: ";
        logEntry = logEntry + m_CameraTotal.ToString();
        logEntry = logEntry + " cameras, active: ";
        logEntry = logEntry + m_CameraLabel;
        LFPG_Util.Info(logEntry);
    }

    // =========================================================
    // EnterCamera — crea o reposiciona el staticcamera en la camara[index]
    // Retorna true si exitoso.
    // =========================================================
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

        // Si ya existe un staticcamera, reposicionar (cycling, sin destroy/create)
        if (m_ViewCamObj)
        {
            m_ViewCamObj.SetPosition(camPos);
            m_ViewCamObj.SetOrientation(camOri);
            m_CameraIndex = index;
            return true;
        }

        // Primera entrada: crear staticcamera LOCAL
        // create_local=true → solo visible en ESTE cliente, no replicado.
        Object viewCam = GetGame().CreateObject("staticcamera", camPos, true, false, false);
        if (!viewCam)
        {
            LFPG_Util.Error("[CameraViewport] Fallo CreateObject staticcamera");
            PlayerBase pErr = PlayerBase.Cast(GetGame().GetPlayer());
            if (pErr)
                pErr.MessageStatus("[LFPG] Error al activar camara. Intenta de nuevo.");
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

    // =========================================================
    // CycleNext / CyclePrev — avanza/retrocede indice circular
    // =========================================================
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

        // Display index is 1-based
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

    // =========================================================
    // Exit
    // =========================================================
    void Exit()
    {
        if (!m_Active)
            return;

        // Restaurar input + HUD vanilla (ANTES de limpiar estado)
        RestoreAllUI();

        // Destruir overlay layout
        if (m_OverlayRoot)
        {
            m_OverlayRoot.Unlink();
            m_OverlayRoot     = null;
            m_LabelWidget     = null;
            m_RecWidget       = null;
            m_TimestampWidget = null;
        }

        m_Active = false;

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

        LFPG_Util.Info("[CameraViewport] Viewport cerrado.");
    }

    // =========================================================
    // Tick — llamado cada frame desde MissionGameplay.OnUpdate
    // Detecta: SPACE=exit, Q=prev, E=next, timeout
    // =========================================================
    void Tick(float timeslice)
    {
        if (!m_Active)
            return;

        // Auto-exit por tiempo maximo
        m_ActiveDuration = m_ActiveDuration + timeslice;
        if (m_ActiveDuration >= LFPG_CCTV_MAX_DURATION_S)
        {
            LFPG_Util.Info("[CameraViewport] Auto-exit (timeout)");
            Exit();
            return;
        }

        // Lazy init de inputs (evita GetInputByName cada frame)
        if (!m_ExitInput)
            m_ExitInput = GetUApi().GetInputByName("UAJump");
        if (!m_NextInput)
            m_NextInput = GetUApi().GetInputByName("UAPeekRight");
        if (!m_PrevInput)
            m_PrevInput = GetUApi().GetInputByName("UAPeekLeft");

        // SPACE → salir
        if (m_ExitInput && m_ExitInput.LocalPress())
        {
            Exit();
            return;
        }

        // E → siguiente camara
        if (m_NextInput && m_NextInput.LocalPress())
        {
            CycleNext();
        }

        // Q → camara anterior
        if (m_PrevInput && m_PrevInput.LocalPress())
        {
            CyclePrev();
        }

        // Actualizar texto del overlay (label, REC parpadeo, timestamp)
        UpdateOverlayText();

        // Avanzar offset de scanlines
        m_ScanlineOffset = m_ScanlineOffset + (LFPG_CCTV_SCROLL_SPEED * timeslice);
        while (m_ScanlineOffset >= LFPG_CCTV_SCANLINE_SPACING)
        {
            m_ScanlineOffset = m_ScanlineOffset - LFPG_CCTV_SCANLINE_SPACING;
        }
    }

    // =========================================================
    // DrawOverlay — scanlines verdes + vignette
    // v0.9.3: REC indicador migrado a TextWidget (UpdateOverlayText).
    //         Scanlines ahora verde oscuro (CCTV look).
    // =========================================================
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

        // Scanlines
        float lineY = m_ScanlineOffset;
        while (lineY < sh)
        {
            hud.DrawLineScreen(0.0, lineY, sw, lineY, 1.0, m_ScanColor);
            lineY = lineY + LFPG_CCTV_SCANLINE_SPACING;
        }

        // Vignette (4 bordes oscuros)
        float vwScale = sh / 1080.0;
        float vw    = LFPG_CCTV_VIGNETTE_W * vwScale;
        float vhalf = vw * 0.5;

        hud.DrawLineScreen(vhalf,      0.0,  vhalf,      sh,   vw, m_VigColor);
        hud.DrawLineScreen(sw - vhalf, 0.0,  sw - vhalf, sh,   vw, m_VigColor);
        hud.DrawLineScreen(0.0,        vhalf, sw,         vhalf, vw, m_VigColor);
        hud.DrawLineScreen(0.0,        sh - vhalf, sw, sh - vhalf, vw, m_VigColor);
    }
}
