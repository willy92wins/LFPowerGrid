// =========================================================
// LF_PowerGrid - CCTV Viewport Manager (v1.0.0)
//
// Rewrite completo. Usa Camera de script (new Camera()) en vez
// de CreateObject("staticcamera").
//
// HISTORIA DE BUGS CON staticcamera:
//   - Input API bloqueado a nivel C++ (LocalPress/LocalValue)
//   - ObjectDelete deja puntero dangling → crash at 0x68
//   - SetActive(false) corrompe estado interno del engine
//   - Otros mods (AdminTools, Expansion, BBP) crashean en
//     super.OnUpdate al leer el puntero corrupto
//   - Requeria skip de super.OnUpdate + cooldown de 10+ frames
//
// SOLUCION: Camera de script (new Camera())
//   - Objeto gestionado por script, no por el world object system
//   - SetActive(true/false) limpio, sin corrupcion de punteros
//   - No necesita ObjectDelete — el GC limpia al nullificar ref
//   - No necesita skip de super.OnUpdate
//   - No necesita three-phase exit ni cooldown
//   - Exit simple: SetActive(false) + null ref + unlock focus
//
// INPUT: MissionGameplay.OnKeyPress (funciona con cualquier camara)
//   - SPACE: salir (deferred a Tick via m_WantsExit)
//   - Q/E: ciclar camaras
//   - ESC excluido (el engine lo procesa para menu de pausa)
//
// OVERLAY: Widgets lazy-init en primer uso. Layout corregido
//   con bloques { } anidados (LFPG_CCTVMenu.layout).
//   DESHABILITADO temporalmente hasta validar camara+input.
//
// ENFORCE SCRIPT NOTES:
//   - No foreach, no ++/--, no ternario, no multilinea en params
//   - Variables hoisted antes de if/else
// =========================================================

static const float LFPG_CCTV_SCANLINE_SPACING = 5.0;
static const float LFPG_CCTV_SCANLINE_ALPHA   = 0.32;
static const float LFPG_CCTV_SCROLL_SPEED     = 20.0;
static const float LFPG_CCTV_VIGNETTE_ALPHA   = 0.60;
static const float LFPG_CCTV_VIGNETTE_W       = 55.0;
static const float LFPG_CCTV_MAX_DURATION_S   = 120.0;

// Key codes (raw ints)
static const int   LFPG_KC_Q      = 16;
static const int   LFPG_KC_E      = 18;
static const int   LFPG_KC_SPACE  = 57;

// Overlay layout
static const string LFPG_CCTV_LAYOUT = "LFPowerGrid/gui/layouts/LFPG_CCTVMenu.layout";

class LFPG_CameraViewport
{
    // ---- Singleton ----
    protected static ref LFPG_CameraViewport s_Instance;

    // ---- Camera (world object via CreateObject) ----
    // new Camera() no funciona en DayZ Enforce (~Object private).
    // CreateObject("staticcamera") crea un objeto del world system.
    //
    // EXIT: ObjectDelete SIN SetActive(false).
    // SetActive(false) corrompe un puntero interno del engine (0x68).
    // ObjectDelete directo fuerza al engine a detectar que la camara
    // activa fue destruida y hacer fallback al player camera por un
    // code path diferente que SI limpia el estado correctamente.
    protected Object m_ViewCamObj;
    protected bool      m_Active;
    protected float     m_ScanlineOffset;
    protected float     m_ActiveDuration;

    // ---- Camera list + cycling ----
    protected ref array<ref LFPG_CameraListEntry> m_CameraList;
    protected int       m_CameraIndex;
    protected int       m_CameraTotal;
    protected string    m_CameraLabel;

    // ---- Colores precalculados ----
    protected int       m_ScanColor;
    protected int       m_VigColor;

    // ---- Overlay widgets ----
    protected Widget       m_OverlayRoot;
    protected TextWidget   m_wCamLabel;
    protected TextWidget   m_wRecLabel;
    protected TextWidget   m_wTimestamp;
    protected float        m_BlinkTimer;
    protected bool         m_RecVisible;

    // ---- Focus lock tracking ----
    protected bool      m_FocusLocked;

    // ---- Deferred exit ----
    // OnKeyPress corre ANTES de OnUpdate. Ponemos flag aqui,
    // Tick() (DESPUES de super.OnUpdate) procesa el exit.
    protected bool      m_WantsExit;

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
        m_OverlayRoot    = null;
        m_wCamLabel      = null;
        m_wRecLabel      = null;
        m_wTimestamp      = null;
        m_BlinkTimer     = 0.0;
        m_RecVisible     = true;
        m_FocusLocked    = false;
        m_WantsExit      = false;

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
            s_Instance.Cleanup();
            delete s_Instance;
            s_Instance = null;
        }
    }

    bool IsActive()
    {
        return m_Active;
    }

    // =========================================================
    // ShouldSkipInspector — skip raycasts durante viewport activo.
    // DeviceInspector usa GetCurrentCameraPosition() que apuntaria
    // al POV de la camara CCTV en vez del jugador.
    // =========================================================
    bool ShouldSkipInspector()
    {
        return m_Active;
    }

    // =========================================================
    // InitWidgets — lazy, primer uso.
    // =========================================================
    void InitWidgets()
    {
        if (m_OverlayRoot)
            return;

        Print("[CameraViewport] DIAG: InitWidgets — pre-CreateWidgets");
        m_OverlayRoot = GetGame().GetWorkspace().CreateWidgets(LFPG_CCTV_LAYOUT);
        if (!m_OverlayRoot)
        {
            LFPG_Util.Error("[CameraViewport] Failed to create overlay: " + LFPG_CCTV_LAYOUT);
            return;
        }
        Print("[CameraViewport] DIAG: InitWidgets — CreateWidgets OK");

        m_OverlayRoot.SetSort(10002);

        string wCam = "CamLabel";
        string wRec = "RecLabel";
        string wTs  = "TimestampLabel";
        m_wCamLabel  = TextWidget.Cast(m_OverlayRoot.FindAnyWidget(wCam));
        m_wRecLabel  = TextWidget.Cast(m_OverlayRoot.FindAnyWidget(wRec));
        m_wTimestamp = TextWidget.Cast(m_OverlayRoot.FindAnyWidget(wTs));

        m_OverlayRoot.Show(false);
        Print("[CameraViewport] DIAG: InitWidgets complete");
    }

    protected void DestroyWidgets()
    {
        if (m_OverlayRoot)
        {
            m_OverlayRoot.Unlink();
            m_OverlayRoot = null;
        }
        m_wCamLabel  = null;
        m_wRecLabel  = null;
        m_wTimestamp = null;
    }

    // =========================================================
    // EnterFromList — punto de entrada desde RPC response.
    // =========================================================
    void EnterFromList(array<ref LFPG_CameraListEntry> entries)
    {
        Print("[CameraViewport] DIAG: EnterFromList entry");

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
        {
            LFPG_Util.Warn("[CameraViewport] EnterFromList: already active, ignoring");
            return;
        }

        m_CameraList  = entries;
        m_CameraTotal = entries.Count();
        m_CameraIndex = 0;
        m_WantsExit   = false;

        // Overlay widgets (deshabilitado temporalmente para test)
        // if (!m_OverlayRoot)
        //     InitWidgets();
        // if (m_OverlayRoot)
        // {
        //     m_OverlayRoot.Show(true);
        //     m_BlinkTimer = 0.0;
        //     m_RecVisible = true;
        // }

        // Suprimir input del jugador
        LockFocus();

        // Ocultar HUD vanilla
        HideHUD();

        // Activar camara
        Print("[CameraViewport] DIAG: pre-EnterCamera(0)");
        bool camOk = EnterCamera(0);
        if (!camOk)
        {
            LFPG_Util.Error("[CameraViewport] EnterCamera failed — aborting");
            UnlockFocus();
            RestoreHUD();
            m_CameraList  = null;
            m_CameraTotal = 0;
            return;
        }
        Print("[CameraViewport] DIAG: post-EnterCamera OK");

        m_Active         = true;
        m_ScanlineOffset = 0.0;
        m_ActiveDuration = 0.0;

        UpdateOverlayLabel();

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

    // =========================================================
    // EnterCamera — crea o reposiciona Camera de script.
    //
    // new Camera() crea un objeto gestionado por script, NO
    // por el world object system. No sufre los bugs de
    // CreateObject("staticcamera"):
    //   - Sin puntero dangling al destruir
    //   - SetActive(false) no corrompe estado del engine
    //   - No necesita ObjectDelete — ref counting + GC
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

        // Reusar camara existente (cycling)
        if (m_ViewCamObj)
        {
            m_ViewCamObj.SetPosition(camPos);
            m_ViewCamObj.SetOrientation(camOri);
            m_CameraIndex = index;
            return true;
        }

        // Primera entrada: crear staticcamera LOCAL
        Print("[CameraViewport] DIAG: CreateObject staticcamera");
        Object viewCam = GetGame().CreateObject("staticcamera", camPos, true, false, false);
        if (!viewCam)
        {
            LFPG_Util.Error("[CameraViewport] CreateObject staticcamera failed");
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

        Print("[CameraViewport] DIAG: pre-SetActive(true)");
        viewCamTyped.SetActive(true);
        Print("[CameraViewport] DIAG: post-SetActive(true) OK");

        m_ViewCamObj  = viewCam;
        m_CameraIndex = index;
        return true;
    }

    // =========================================================
    // HandleKeyDown — desde MissionGameplay.OnKeyPress.
    // Solo pone flags. No toca la camara.
    // =========================================================
    bool HandleKeyDown(int key)
    {
        if (!m_Active)
            return false;

        // SPACE → marcar para salir en Tick
        if (key == LFPG_KC_SPACE)
        {
            Print("[CameraViewport] DIAG: EXIT queued via SPACE");
            m_WantsExit = true;
            return true;
        }

        // E → siguiente camara
        if (key == LFPG_KC_E)
        {
            CycleNext();
            return true;
        }

        // Q → camara anterior
        if (key == LFPG_KC_Q)
        {
            CyclePrev();
            return true;
        }

        return false;
    }

    // =========================================================
    // Cycling
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
            UpdateOverlayLabel();
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
            UpdateOverlayLabel();
            ShowCycleMessage();
        }
    }

    protected void ShowCycleMessage()
    {
        PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
        if (!p)
            return;

        int displayIdx = m_CameraIndex + 1;
        string msg = "[LFPG] ";
        msg = msg + m_CameraLabel;
        msg = msg + " (";
        msg = msg + displayIdx.ToString();
        msg = msg + "/";
        msg = msg + m_CameraTotal.ToString();
        msg = msg + ")";
        p.MessageStatus(msg);
    }

    // =========================================================
    // DoExit — sale del viewport. Llamado desde Tick.
    //
    // Camera de script: SetActive(false) restaura la camara del
    // jugador limpiamente. Nullificar m_Camera libera el ref count
    // y el GC destruye el objeto — sin puntero dangling.
    // =========================================================
    protected void DoExit()
    {
        Print("[CameraViewport] DIAG: DoExit");

        m_Active    = false;
        m_WantsExit = false;

        // Destruir camara SIN SetActive(false).
        // SetActive(false) corrompe puntero interno del engine (0x68).
        // ObjectDelete directo fuerza al engine a detectar la destruccion
        // de la camara activa y hacer fallback al player camera por un
        // code path interno que SI limpia el estado correctamente.
        if (m_ViewCamObj)
        {
            Print("[CameraViewport] DIAG: ObjectDelete (sin SetActive false)");
            GetGame().ObjectDelete(m_ViewCamObj);
            m_ViewCamObj = null;
        }

        // Ocultar overlay
        if (m_OverlayRoot)
        {
            m_OverlayRoot.Show(false);
        }

        // Restaurar input + HUD
        UnlockFocus();
        RestoreHUD();

        // Limpiar estado
        m_CameraLabel    = "";
        m_ActiveDuration = 0.0;
        m_ScanlineOffset = 0.0;
        m_CameraList     = null;
        m_CameraIndex    = 0;
        m_CameraTotal    = 0;

        LFPG_Util.Info("[CameraViewport] Viewport cerrado.");
    }

    // =========================================================
    // Cleanup — forzado (Reset / OnMissionFinish).
    // =========================================================
    protected void Cleanup()
    {
        m_Active    = false;
        m_WantsExit = false;

        if (m_ViewCamObj)
        {
            GetGame().ObjectDelete(m_ViewCamObj);
            m_ViewCamObj = null;
        }

        UnlockFocus();
        RestoreHUD();
        DestroyWidgets();

        m_CameraLabel    = "";
        m_ActiveDuration = 0.0;
        m_ScanlineOffset = 0.0;
        m_CameraList     = null;
        m_CameraIndex    = 0;
        m_CameraTotal    = 0;
    }

    // =========================================================
    // Tick — per-frame (runs AFTER super.OnUpdate)
    //
    //   1. Deferred exit (m_WantsExit from OnKeyPress)
    //   2. Timeout
    //   3. Overlay text update
    //   4. Scanline advance
    // =========================================================
    void Tick(float timeslice)
    {
        if (!m_Active)
            return;

        // ---- Deferred exit ----
        if (m_WantsExit)
        {
            DoExit();
            return;
        }

        // ---- Timeout ----
        m_ActiveDuration = m_ActiveDuration + timeslice;
        if (m_ActiveDuration >= LFPG_CCTV_MAX_DURATION_S)
        {
            LFPG_Util.Info("[CameraViewport] Auto-exit (timeout)");
            DoExit();
            return;
        }

        // ---- REC blink (0.7s toggle) ----
        m_BlinkTimer = m_BlinkTimer + timeslice;
        if (m_BlinkTimer >= 0.7)
        {
            m_BlinkTimer = 0.0;
            if (m_RecVisible)
            {
                m_RecVisible = false;
            }
            else
            {
                m_RecVisible = true;
            }
            if (m_wRecLabel)
            {
                m_wRecLabel.Show(m_RecVisible);
            }
        }

        // ---- Timestamp ----
        if (m_wTimestamp)
        {
            int year   = 0;
            int month  = 0;
            int day    = 0;
            int hour   = 0;
            int minute = 0;
            GetGame().GetWorld().GetDate(year, month, day, hour, minute);

            string ts = year.ToStringLen(4);
            ts = ts + "-";
            ts = ts + month.ToStringLen(2);
            ts = ts + "-";
            ts = ts + day.ToStringLen(2);
            ts = ts + "  ";
            ts = ts + hour.ToStringLen(2);
            ts = ts + ":";
            ts = ts + minute.ToStringLen(2);
            m_wTimestamp.SetText(ts);
        }

        // ---- Scanlines advance ----
        m_ScanlineOffset = m_ScanlineOffset + (LFPG_CCTV_SCROLL_SPEED * timeslice);
        while (m_ScanlineOffset >= LFPG_CCTV_SCANLINE_SPACING)
        {
            m_ScanlineOffset = m_ScanlineOffset - LFPG_CCTV_SCANLINE_SPACING;
        }
    }

    // =========================================================
    // DrawOverlay — scanlines + vignette via canvas
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

    // =========================================================
    // Overlay label update
    // =========================================================
    protected void UpdateOverlayLabel()
    {
        if (!m_wCamLabel)
            return;

        int displayIdx = m_CameraIndex + 1;
        string labelText = m_CameraLabel;
        labelText = labelText + "  [";
        labelText = labelText + displayIdx.ToString();
        labelText = labelText + "/";
        labelText = labelText + m_CameraTotal.ToString();
        labelText = labelText + "]";
        m_wCamLabel.SetText(labelText);
    }

    // =========================================================
    // Focus + HUD helpers
    // =========================================================
    protected void LockFocus()
    {
        if (m_FocusLocked)
            return;

        Input inp = GetGame().GetInput();
        if (inp)
        {
            inp.ChangeGameFocus(1);
        }
        m_FocusLocked = true;
    }

    protected void UnlockFocus()
    {
        if (!m_FocusLocked)
            return;

        Input inp = GetGame().GetInput();
        if (inp)
        {
            inp.ChangeGameFocus(-1);
        }
        m_FocusLocked = false;
    }

    protected void HideHUD()
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

    protected void RestoreHUD()
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
}
