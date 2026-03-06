// =========================================================
// LF_PowerGrid - CCTV Viewport Manager (v1.2.0)
//
// EXIT STRATEGY: KEEP-ALIVE (no ObjectDelete durante exit normal)
//
//   El crash 0x68 ocurre porque ObjectDelete libera la memoria
//   del objeto staticcamera mientras el engine aún mantiene un
//   puntero interno a ella. Otros mods (BBP, Expansion, AdminTools)
//   llaman GetCurrentCameraPosition() en super.OnUpdate y leen
//   memoria liberada → access violation at 0x68.
//
//   NINGUN número de frames de margen entre ObjectDelete y el
//   siguiente super.OnUpdate es suficiente — el engine cachea el
//   puntero y no lo actualiza hasta que completa un ciclo interno
//   que no podemos controlar desde script.
//
//   SOLUCIÓN: NO destruir la cámara durante exit normal.
//     1. SetActive(false) → el engine actualiza su puntero interno
//        para apuntar a la player camera (fallback nativo).
//     2. Mantener el objeto vivo en m_ViewCamObj.
//     3. Reusar el objeto en la siguiente sesión CCTV.
//     4. Solo ObjectDelete en ForceCleanup (shutdown/disconnect).
//
//   Esto elimina el dangling pointer COMPLETAMENTE — el objeto
//   nunca se libera mientras hay posibilidad de que otro mod
//   lea el puntero en super.OnUpdate.
//
//   Two-phase exit con margen de 1 frame para SetActive(false):
//
//     Frame N:   OnKeyPress → m_ExitPhase=1
//                super.OnUpdate: cámara ACTIVA, mods OK
//                Tick Phase 1: m_Active=false, overlay off.
//                              Cámara SIGUE ACTIVA.
//
//     Frame N+1: super.OnUpdate: cámara SIGUE ACTIVA, mods OK
//                Tick Phase 2: SetActive(false) + unlock + restore.
//                              Objeto SIGUE VIVO. Cero dangling ptrs.
//
//     Frame N+2+: super.OnUpdate: engine usa player cam. Objeto
//                 staticcamera sigue en memoria pero inactivo.
//
// ENFORCE SCRIPT RULES:
//   - No foreach, no ++/--, no ternario, no +=/-=
//   - No multilinea en params de función
//   - Hoisting de variables antes de if/else
//   - String concat incremental
// =========================================================

static const float LFPG_CCTV_SCANLINE_SPACING = 5.0;
static const float LFPG_CCTV_SCANLINE_ALPHA   = 0.32;
static const float LFPG_CCTV_SCROLL_SPEED     = 20.0;
static const float LFPG_CCTV_VIGNETTE_ALPHA   = 0.60;
static const float LFPG_CCTV_VIGNETTE_W       = 55.0;
static const float LFPG_CCTV_MAX_DURATION_S   = 120.0;

static const int   LFPG_CCTV_EXIT_COOLDOWN    = 3;

// Key codes
static const int   LFPG_KC_ESCAPE = 1;
static const int   LFPG_KC_Q      = 16;
static const int   LFPG_KC_E      = 18;
static const int   LFPG_KC_SPACE  = 57;

// Overlay layout
static const string LFPG_CCTV_LAYOUT = "LFPowerGrid/gui/layouts/LFPG_CCTVMenu.layout";

class LFPG_CameraViewport
{
    // ---- Singleton ----
    protected static ref LFPG_CameraViewport s_Instance;

    // ---- Camera (world object — kept alive between sessions) ----
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

    // ---- Overlay widgets ----
    protected Widget       m_OverlayRoot;
    protected TextWidget   m_wCamLabel;
    protected TextWidget   m_wRecLabel;
    protected TextWidget   m_wTimestamp;
    protected float        m_BlinkTimer;
    protected bool         m_RecVisible;

    // ---- Two-phase exit ----
    // 0 = normal, 1 = exit requested, 2 = SetActive(false) + unlock
    protected int       m_ExitPhase;

    // ---- Inspector cooldown (post-exit) ----
    protected int       m_ExitCooldown;

    // ---- Focus lock tracking ----
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
        m_ExitPhase      = 0;
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
        if (m_ExitPhase > 0)
            return true;
        if (m_ExitCooldown > 0)
            return true;
        return false;
    }

    // =========================================================
    // InitWidgets
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

        m_OverlayRoot.SetSort(10002);

        string wCam = "CamLabel";
        string wRec = "RecLabel";
        string wTs  = "TimestampLabel";
        m_wCamLabel  = TextWidget.Cast(m_OverlayRoot.FindAnyWidget(wCam));
        m_wRecLabel  = TextWidget.Cast(m_OverlayRoot.FindAnyWidget(wRec));
        m_wTimestamp = TextWidget.Cast(m_OverlayRoot.FindAnyWidget(wTs));

        // Colores y texto desde script — NO en layout.
        // Layout con color "A R G B" (comillas) cuelga el parser nativo.
        // DeviceInspector tampoco pone color en layout.
        int greenColor = ARGB(200, 40, 220, 40);
        int redColor   = ARGB(220, 220, 30, 30);
        int dimGreen   = ARGB(180, 40, 200, 40);

        if (m_wCamLabel)
        {
            m_wCamLabel.SetColor(greenColor);
            m_wCamLabel.SetText("CAM-000000  [1/1]");
        }
        if (m_wRecLabel)
        {
            m_wRecLabel.SetColor(redColor);
            m_wRecLabel.SetText("REC");
        }
        if (m_wTimestamp)
        {
            m_wTimestamp.SetColor(dimGreen);
            m_wTimestamp.SetText("0000-00-00  00:00");
        }

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
    // EnterFromList
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

        if (m_ExitPhase > 0)
        {
            LFPG_Util.Warn("[CameraViewport] EnterFromList: exit in progress, ignoring");
            return;
        }

        m_CameraList  = entries;
        m_CameraTotal = entries.Count();
        m_CameraIndex = 0;

        // Widgets: NO crear aquí — EnterFromList corre en contexto RPC
        // y CreateWidgets cuelga el engine desde RPC handlers.
        // Verificado: COT, VPP y CableHUD crean widgets desde OnUpdate.
        // InitWidgets() se llama desde MissionGameplay.OnUpdate.
        // Aquí solo Show(true) si ya existen.

        LockFocus();
        HideHUD();

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
        m_ExitPhase      = 0;

        // Solo Show — widgets ya creados arriba (o en sesión anterior)
        if (m_OverlayRoot)
        {
            m_OverlayRoot.Show(true);
            m_BlinkTimer = 0.0;
            m_RecVisible = true;
            if (m_wRecLabel)
                m_wRecLabel.Show(true);
        }

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
    // EnterCamera — crear o reutilizar staticcamera.
    //
    // Si m_ViewCamObj existe (reutilización entre sesiones o
    // cycling intra-sesión), solo reposiciona y re-activa.
    // Si no, crea nuevo world object.
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

        // Reusar objeto existente (keep-alive de sesión anterior o cycling)
        if (m_ViewCamObj)
        {
            m_ViewCamObj.SetPosition(camPos);
            m_ViewCamObj.SetOrientation(camOri);

            // Re-activar SOLO si estaba desactivado (keep-alive).
            // Durante cycling m_Active==true → cámara ya activa, skip.
            // En re-entry desde keep-alive m_Active==false → necesita SetActive.
            if (!m_Active)
            {
                Camera existingCam = Camera.Cast(m_ViewCamObj);
                if (existingCam)
                {
                    existingCam.SetActive(true);
                }
            }

            m_CameraIndex = index;
            Print("[CameraViewport] DIAG: Reused existing camera object");
            return true;
        }

        // Primera entrada: crear world object
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
    // Solo pone flags. No toca la cámara.
    // =========================================================
    bool HandleKeyDown(int key)
    {
        if (!m_Active)
            return false;

        if (key == LFPG_KC_SPACE || key == LFPG_KC_ESCAPE)
        {
            Print("[CameraViewport] DIAG: EXIT queued via key=" + key.ToString());
            m_ExitPhase = 1;
            return true;
        }

        if (key == LFPG_KC_E)
        {
            CycleNext();
            return true;
        }

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
    // ForceCleanup — ÚNICO lugar donde se hace ObjectDelete.
    // Solo se llama desde Reset() (shutdown/disconnect).
    // =========================================================
    protected void ForceCleanup()
    {
        Print("[CameraViewport] DIAG: ForceCleanup");
        m_Active    = false;
        m_ExitPhase = 0;

        if (m_ViewCamObj)
        {
            // Shutdown — no hay siguiente frame, ObjectDelete es seguro
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
        m_ExitCooldown   = 0;
    }

    // =========================================================
    // Tick — per-frame (runs AFTER super.OnUpdate)
    //
    // TWO-PHASE KEEP-ALIVE EXIT:
    //
    //   Phase 1 (Frame N Tick):
    //     m_Active=false, overlay off.
    //     Cámara SIGUE ACTIVA (mods safe en Frame N+1 super.OnUpdate).
    //
    //   Phase 2 (Frame N+1 Tick):
    //     SetActive(false) → engine fallback a player cam.
    //     UnlockFocus + RestoreHUD.
    //     m_ViewCamObj SIGUE VIVO — NO ObjectDelete.
    //     Cero dangling pointers.
    //
    //   Frame N+2+ super.OnUpdate:
    //     Engine usa player cam. Objeto staticcamera en memoria
    //     pero inactivo. Será reusado en la siguiente sesión CCTV
    //     o destruido en ForceCleanup (shutdown).
    // =========================================================
    void Tick(float timeslice)
    {
        // ---- Phase 2: SetActive(false) + unlock (NO ObjectDelete) ----
        if (m_ExitPhase == 2)
        {
            Print("[CameraViewport] DIAG: Phase 2 — SetActive(false) + unlock (keep-alive)");

            if (m_ViewCamObj)
            {
                Camera viewCamTyped = Camera.Cast(m_ViewCamObj);
                if (viewCamTyped)
                {
                    viewCamTyped.SetActive(false);
                }
                // m_ViewCamObj NO se nullifica — objeto sigue vivo para reuso
            }

            UnlockFocus();
            RestoreHUD();

            m_ExitPhase = 0;
            LFPG_Util.Info("[CameraViewport] Phase 2 complete — camera deactivated (kept alive)");
        }

        // ---- Phase 1: desactivación suave ----
        if (m_ExitPhase == 1)
        {
            Print("[CameraViewport] DIAG: Phase 1 — m_Active=false, camera stays active");

            m_Active       = false;
            m_ExitCooldown = LFPG_CCTV_EXIT_COOLDOWN;

            if (m_OverlayRoot)
                m_OverlayRoot.Show(false);

            m_CameraLabel    = "";
            m_ActiveDuration = 0.0;
            m_ScanlineOffset = 0.0;
            m_CameraList     = null;
            m_CameraIndex    = 0;
            m_CameraTotal    = 0;

            m_ExitPhase = 2;
            LFPG_Util.Info("[CameraViewport] Phase 1 complete — camera stays active");
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
            m_ExitPhase = 1;
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
    // DrawOverlay
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
