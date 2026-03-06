// =========================================================
// LF_PowerGrid - CCTV Viewport Manager (v0.9.9)
//
// Evidencia de tests v0.9.7 / v0.9.8 / v0.9.9-a:
//
//   1. staticcamera bloquea Input API de script (LocalPress,
//      LocalValue, UAInput) a nivel C++ engine.
//   2. CreateWidgets colgaba/crasheaba en TODAS las versiones.
//      Causa real: layout malformado — hijos sin bloque { }
//      interno. El parser nativo entra en loop infinito.
//      Corregido en LFPG_CCTVMenu.layout v0.9.9.
//   3. ShowScriptedMenu congela el juego (loop modal interno).
//   4. MissionGameplay.OnKeyPress SI RECIBE TECLAS incluso con
//      staticcamera activa (confirmado via crash log de BBP).
//
// Solucion v0.9.9 (3 pilares):
//
//   PILAR 1 — Layout corregido con bloques { } anidados.
//     Widgets creados lazy en EnterFromList (primer uso).
//     Se muestran/ocultan con Show(true/false) por sesion.
//
//   PILAR 2 — Input via MissionGameplay.OnKeyPress override.
//     CameraViewport.HandleKeyDown(key) procesa Q/E/SPACE/ESC.
//
//   PILAR 3 — ChangeGameFocus(1) suprime movimiento del jugador.
//     Tracking con m_FocusLocked para prevenir desbalance.
//
//   Two-phase delete:
//     Frame N:   SetActive(false), m_PendingDelete = obj
//     Frame N+1: ObjectDelete + ChangeGameFocus(-1) + restore HUD
//     Frame N+2+: Inspector cooldown expira
//
// ENFORCE SCRIPT NOTES:
//   - No foreach, no ++/--, no ternario, no multilinea en params
//   - cast implicito float→int via asignacion directa
//   - Hoisting de variables antes de if/else
// =========================================================

static const float LFPG_CCTV_SCANLINE_SPACING = 5.0;
static const float LFPG_CCTV_SCANLINE_ALPHA   = 0.32;
static const float LFPG_CCTV_SCROLL_SPEED     = 20.0;
static const float LFPG_CCTV_VIGNETTE_ALPHA   = 0.60;
static const float LFPG_CCTV_VIGNETTE_W       = 55.0;
static const float LFPG_CCTV_MAX_DURATION_S   = 120.0;

static const int   LFPG_CCTV_EXIT_COOLDOWN    = 3;

// Key codes (raw ints — Enforce no expone KeyboardKey como enum)
static const int   LFPG_KC_ESCAPE = 1;
static const int   LFPG_KC_Q      = 16;
static const int   LFPG_KC_E      = 18;
static const int   LFPG_KC_SPACE  = 57;

// Overlay layout — ya empaquetado en PBO (gui/layouts/)
static const string LFPG_CCTV_LAYOUT = "LFPowerGrid/gui/layouts/LFPG_CCTVMenu.layout";

class LFPG_CameraViewport
{
    // ---- Singleton ----
    protected static ref LFPG_CameraViewport s_Instance;

    // ---- Estado ----
    protected Object    m_ViewCamObj;
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

    // ---- Overlay widgets (pre-creados en InitWidgets, ocultos por defecto) ----
    protected Widget       m_OverlayRoot;
    protected TextWidget   m_wCamLabel;
    protected TextWidget   m_wRecLabel;
    protected TextWidget   m_wTimestamp;
    protected float        m_BlinkTimer;
    protected bool         m_RecVisible;

    // ---- Two-phase delete ----
    protected Object    m_PendingDelete;

    // ---- Inspector cooldown ----
    protected int       m_ExitCooldown;

    // ---- Focus lock tracking ----
    // true si ChangeGameFocus(1) fue llamado y aun no se libero.
    // Previene double-increment / double-decrement.
    protected bool      m_FocusLocked;

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
        m_PendingDelete  = null;
        m_ExitCooldown   = 0;
        m_FocusLocked    = false;

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
            s_Instance.ForceCleanup();
            delete s_Instance;
            s_Instance = null;
        }
    }

    bool IsActive()
    {
        return m_Active;
    }

    bool ShouldSkipInspector()
    {
        if (m_Active)
            return true;
        if (m_ExitCooldown > 0)
            return true;
        return false;
    }

    // =========================================================
    // InitWidgets — crear overlay widgets (hidden).
    // Llamado LAZY desde EnterFromList, ANTES de activar
    // staticcamera (workspace estable, sin crash).
    //
    // NO llamar desde MissionGameplay.OnInit — cuelga el engine
    // (FrameWidgetClass fullscreen en init causa hang).
    //
    // Los widgets se crean una sola vez y se reusan entre sesiones.
    // Show(true) al entrar, Show(false) al salir.
    // =========================================================
    void InitWidgets()
    {
        if (m_OverlayRoot)
            return;

        Print("[CameraViewport] DIAG: InitWidgets — pre-CreateWidgets");
        m_OverlayRoot = GetGame().GetWorkspace().CreateWidgets(LFPG_CCTV_LAYOUT);
        if (!m_OverlayRoot)
        {
            LFPG_Util.Error("[CameraViewport] Failed to create overlay from: " + LFPG_CCTV_LAYOUT);
            return;
        }
        Print("[CameraViewport] DIAG: InitWidgets — CreateWidgets OK");

        // Sort por encima de CableHUD (10000) y DeviceInspector (10001)
        m_OverlayRoot.SetSort(10002);

        string wCam = "CamLabel";
        string wRec = "RecLabel";
        string wTs  = "TimestampLabel";
        m_wCamLabel  = TextWidget.Cast(m_OverlayRoot.FindAnyWidget(wCam));
        m_wRecLabel  = TextWidget.Cast(m_OverlayRoot.FindAnyWidget(wRec));
        m_wTimestamp = TextWidget.Cast(m_OverlayRoot.FindAnyWidget(wTs));

        // Ocultar hasta que se active una sesion CCTV
        m_OverlayRoot.Show(false);

        Print("[CameraViewport] DIAG: InitWidgets complete");
        LFPG_Util.Info("[CameraViewport] Overlay widgets created (hidden)");
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
    //
    // Orden:
    //   1. Mostrar overlay (widgets ya existen, solo Show(true))
    //   2. ChangeGameFocus(1) — suprimir movimiento del jugador
    //   3. Ocultar HUD vanilla
    //   4. CreateObject staticcamera + SetActive(true)
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

        // Limpiar pending delete si quedaba
        if (m_PendingDelete)
        {
            GetGame().ObjectDelete(m_PendingDelete);
            m_PendingDelete = null;
        }

        m_CameraList  = entries;
        m_CameraTotal = entries.Count();
        m_CameraIndex = 0;

        // PASO 1: Crear overlay widgets si no existen (lazy init).
        // DEBE ejecutarse ANTES de EnterCamera — sin staticcamera activa,
        // workspace estable, CreateWidgets no crashea.
        if (!m_OverlayRoot)
        {
            Print("[CameraViewport] DIAG: lazy InitWidgets");
            InitWidgets();
        }

        // PASO 2: Mostrar overlay widgets
        if (m_OverlayRoot)
        {
            m_OverlayRoot.Show(true);
            m_BlinkTimer = 0.0;
            m_RecVisible = true;
            if (m_wRecLabel)
                m_wRecLabel.Show(true);
        }

        // PASO 2: Suprimir input del jugador
        LockFocus();

        // PASO 3: Ocultar HUD vanilla
        HideHUD();

        // PASO 4: Activar staticcamera
        Print("[CameraViewport] DIAG: pre-EnterCamera(0)");
        bool camOk = EnterCamera(0);
        if (!camOk)
        {
            LFPG_Util.Error("[CameraViewport] EnterCamera failed — aborting");
            if (m_OverlayRoot)
                m_OverlayRoot.Show(false);
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
        m_ExitCooldown   = 0;

        // PASO 5: Actualizar overlay label
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
    // EnterCamera
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

        Print("[CameraViewport] DIAG: pre-SetActive(true)");
        viewCamTyped.SetActive(true);
        Print("[CameraViewport] DIAG: post-SetActive(true) OK");

        m_ViewCamObj  = viewCam;
        m_CameraIndex = index;
        return true;
    }

    // =========================================================
    // HandleKeyDown — llamado desde MissionGameplay.OnKeyPress.
    // Retorna true si la tecla fue consumida.
    // =========================================================
    bool HandleKeyDown(int key)
    {
        if (!m_Active)
            return false;

        Print("[CameraViewport] DIAG: HandleKeyDown key=" + key.ToString());

        // SPACE o ESC → salir
        if (key == LFPG_KC_SPACE || key == LFPG_KC_ESCAPE)
        {
            Print("[CameraViewport] DIAG: EXIT via key=" + key.ToString());
            RequestExit();
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
    // RequestExit — two-phase delete.
    //
    // Frame N (aqui):
    //   1. m_Active = false
    //   2. Camera.SetActive(false)
    //   3. m_PendingDelete = obj (NO delete aun)
    //   4. Ocultar overlay
    //   5. m_ExitCooldown = 3
    //
    // Frame N+1 (Tick):
    //   1. ObjectDelete(m_PendingDelete)
    //   2. UnlockFocus + RestoreHUD
    //
    // El focus se mantiene locked durante el frame de transicion
    // para que el jugador no se mueva mientras el engine restaura
    // la camara. Se libera en phase 2 junto con el HUD.
    // =========================================================
    void RequestExit()
    {
        if (!m_Active)
            return;

        Print("[CameraViewport] DIAG: RequestExit()");

        m_Active       = false;
        m_ExitCooldown = LFPG_CCTV_EXIT_COOLDOWN;

        // Phase 1: desactivar, no destruir
        if (m_ViewCamObj)
        {
            Camera viewCamTyped = Camera.Cast(m_ViewCamObj);
            if (viewCamTyped)
            {
                viewCamTyped.SetActive(false);
            }
            m_PendingDelete = m_ViewCamObj;
            m_ViewCamObj    = null;
        }

        // Ocultar overlay inmediatamente
        if (m_OverlayRoot)
        {
            m_OverlayRoot.Show(false);
        }

        // Limpiar estado
        m_CameraLabel    = "";
        m_ActiveDuration = 0.0;
        m_ScanlineOffset = 0.0;
        m_CameraList     = null;
        m_CameraIndex    = 0;
        m_CameraTotal    = 0;

        LFPG_Util.Info("[CameraViewport] Exit phase 1 (pending delete)");
    }

    // =========================================================
    // Exit — forzado (Reset / OnMissionFinish). Todo en un frame.
    // =========================================================
    void Exit()
    {
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

        if (m_PendingDelete)
        {
            GetGame().ObjectDelete(m_PendingDelete);
            m_PendingDelete = null;
        }

        if (m_OverlayRoot)
        {
            m_OverlayRoot.Show(false);
        }

        UnlockFocus();
        RestoreHUD();

        m_CameraLabel    = "";
        m_ActiveDuration = 0.0;
        m_ScanlineOffset = 0.0;
        m_CameraList     = null;
        m_CameraIndex    = 0;
        m_CameraTotal    = 0;
        m_ExitCooldown   = 0;
    }

    protected void ForceCleanup()
    {
        Exit();
        DestroyWidgets();
    }

    // =========================================================
    // Tick — per-frame
    //   1. Two-phase delete (phase 2)
    //   2. Inspector cooldown
    //   3. Timeout
    //   4. Overlay text update (REC blink, timestamp)
    //   5. Scanline advance
    // =========================================================
    void Tick(float timeslice)
    {
        // ---- Phase 2 del two-phase delete ----
        if (m_PendingDelete)
        {
            Print("[CameraViewport] DIAG: Phase 2 — ObjectDelete + restore");
            GetGame().ObjectDelete(m_PendingDelete);
            m_PendingDelete = null;

            // Restaurar focus + HUD ahora que staticcamera ya no existe
            UnlockFocus();
            RestoreHUD();
        }

        // ---- Cooldown de inspector ----
        if (m_ExitCooldown > 0)
        {
            m_ExitCooldown = m_ExitCooldown - 1;
        }

        if (!m_Active)
            return;

        // ---- Timeout ----
        m_ActiveDuration = m_ActiveDuration + timeslice;
        if (m_ActiveDuration >= LFPG_CCTV_MAX_DURATION_S)
        {
            LFPG_Util.Info("[CameraViewport] Auto-exit (timeout)");
            RequestExit();
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
