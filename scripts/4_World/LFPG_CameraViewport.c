// =========================================================
// LF_PowerGrid - CCTV Viewport Manager (v1.2.0)
//
// EXIT STRATEGY: COT DEDICATED SERVER PATTERN
//
//   Referencia: COT JMCameraModule.c (miles de servidores DayZ)
//
//   ENTER (server-side):
//     SelectSpectator(sender, "staticcamera", pos)
//     → engine crea + trackea la cámara internamente
//     → RPC CAMERA_LIST_RESPONSE al cliente
//
//   ENTER (client-side):
//     Camera.GetCurrentCamera() → obtiene cámara engine-managed
//     SetActive(true) + SetPosition + SetOrientation
//
//   EXIT (client-side):
//     Phase 1: m_Active=false, overlay off, RPC EXIT_REQUEST → server
//     Phase 2: WAITING for server CCTV_EXIT_CONFIRM
//     DoExitCleanup: SetActive(false) + null (NO ObjectDeleteOnClient)
//       COT: only ObjectDeleteOnClient for custom JMCinematicCamera
//       staticcamera is engine-managed → engine cleans up via SelectPlayer
//
//   EXIT (server-side):
//     SelectPlayer(sender, player) → engine restaura player cam
//     RPC EXIT_CONFIRM → client → DoExitCleanup
//
//   KEY: CreateObject("staticcamera") = unmanaged → crash 0x68
//        SelectSpectator("staticcamera") = engine-managed → cleanup OK
//
// ENFORCE SCRIPT RULES:
//   - No foreach, no ++/--, no ternario, no +=/-=
//   - No multilinea en params de función
//   - Hoisting de variables antes de if/else
//   - String concat incremental
// =========================================================

static const float LFPG_CCTV_SCANLINE_SPACING = 8.0;
static const float LFPG_CCTV_SCANLINE_ALPHA   = 0.15;
static const float LFPG_CCTV_SCROLL_SPEED     = 20.0;
static const float LFPG_CCTV_VIGNETTE_ALPHA   = 0.60;
static const float LFPG_CCTV_VIGNETTE_W       = 55.0;
static const float LFPG_CCTV_MAX_DURATION_S   = 120.0;

static const int   LFPG_CCTV_EXIT_COOLDOWN    = 3;

// Key codes
static const int   LFPG_KC_ESCAPE = 1;
static const int   LFPG_KC_W      = 17;
static const int   LFPG_KC_Q      = 16;
static const int   LFPG_KC_E      = 18;
static const int   LFPG_KC_A      = 30;
static const int   LFPG_KC_S      = 31;
static const int   LFPG_KC_D      = 32;
static const int   LFPG_KC_SPACE  = 57;

// Camera pan: slow pan speed, limited angles from base orientation
static const float LFPG_CCTV_PAN_SPEED     = 30.0;   // degrees per second
static const float LFPG_CCTV_YAW_LIMIT     = 90.0;   // +/- degrees horizontal
static const float LFPG_CCTV_PITCH_LIMIT   = 45.0;   // +/- degrees vertical

// Overlay layout
static const string LFPG_CCTV_LAYOUT = "LFPowerGrid/gui/layouts/LFPG_CCTVMenu.layout";

class LFPG_CameraViewport
{
    // ---- Singleton ----
    protected static ref LFPG_CameraViewport s_Instance;

    // ---- Camera (engine-managed via SelectSpectator) ----
    protected Object    m_ViewCamObj;
    protected bool      m_Active;
    protected float     m_ScanlineOffset;
    protected float     m_ActiveDuration;

    // ---- Player reference (guardada antes de SelectSpectator) ----
    // GetPlayer() puede retornar null durante spectator mode.
    // Se usa para enviar RPC CCTV_EXIT al salir.
    protected PlayerBase m_PlayerRef;

    // ---- Camera list + cycling ----
    protected ref array<ref LFPG_CameraListEntry> m_CameraList;
    protected int       m_CameraIndex;
    protected int       m_CameraTotal;
    protected string    m_CameraLabel;

    // ---- Colores precalculados ----
    protected int       m_ScanColor;
    protected int       m_VigColor;

    // ---- Overlay widgets ----
    protected Widget         m_OverlayRoot;
    protected ImageWidget    m_wGreyOverlay;
    protected CanvasWidget   m_wScanCanvas;
    protected TextWidget     m_wCamLabel;
    protected TextWidget     m_wRecLabel;
    protected TextWidget     m_wTimestamp;
    protected float          m_BlinkTimer;
    protected bool           m_RecVisible;

    // ---- Camera pan (WASD rotation from base orientation) ----
    protected vector    m_BaseOrientation;  // Original camera orientation
    protected float     m_YawOffset;        // Current yaw offset from base (-90..+90)
    protected float     m_PitchOffset;      // Current pitch offset from base (-45..+45)
    protected bool      m_KeyW;
    protected bool      m_KeyA;
    protected bool      m_KeyS;
    protected bool      m_KeyD;

    // ---- Two-phase exit (COT pattern) ----
    // ---- Two-phase exit + server confirmation (COT pattern) ----
    // 0 = normal
    // 1 = exit requested this frame: hide overlay, send RPC EXIT_REQUEST
    // 2 = waiting for server CCTV_EXIT_CONFIRM (SelectPlayer done)
    // DoExitCleanup() called from RPC → cleanup camera + set phase=0
    protected int       m_ExitPhase;

    // Timeout for waiting state (safety: force cleanup if server doesn't respond)
    protected float     m_ExitWaitTimer;

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
        m_PlayerRef      = null;
        m_CameraList     = null;
        m_CameraIndex    = 0;
        m_CameraTotal    = 0;
        m_OverlayRoot    = null;
        m_wGreyOverlay   = null;
        m_wScanCanvas    = null;
        m_wCamLabel      = null;
        m_wRecLabel      = null;
        m_wTimestamp      = null;
        m_BlinkTimer     = 0.0;
        m_RecVisible     = true;
        m_BaseOrientation = vector.Zero;
        m_YawOffset      = 0.0;
        m_PitchOffset    = 0.0;
        m_KeyW           = false;
        m_KeyA           = false;
        m_KeyS           = false;
        m_KeyD           = false;
        m_ExitPhase      = 0;
        m_ExitWaitTimer  = 0.0;
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
    // InitWidgets — positions set from script (resolution-independent)
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

        // Find all widgets
        string wOvl  = "GreyOverlay";
        string wScan = "ScanlineCanvas";
        string wCam  = "CamLabel";
        string wRec  = "RecLabel";
        string wTs   = "TimestampLabel";
        m_wGreyOverlay = ImageWidget.Cast(m_OverlayRoot.FindAnyWidget(wOvl));
        m_wScanCanvas  = CanvasWidget.Cast(m_OverlayRoot.FindAnyWidget(wScan));
        m_wCamLabel    = TextWidget.Cast(m_OverlayRoot.FindAnyWidget(wCam));
        m_wRecLabel    = TextWidget.Cast(m_OverlayRoot.FindAnyWidget(wRec));
        m_wTimestamp   = TextWidget.Cast(m_OverlayRoot.FindAnyWidget(wTs));

        // Screen dimensions for positioning
        int scrW = 0;
        int scrH = 0;
        GetScreenSize(scrW, scrH);
        float sw = scrW;
        float sh = scrH;
        float scale = sh / 1080.0;

        // Overlay tint disabled — scanlines + vignette give enough camera effect.
        // Widget kept in layout for future tuning. Alpha=0 = invisible.
        if (m_wGreyOverlay)
        {
            m_wGreyOverlay.SetColor(ARGB(0, 0, 0, 0));
            m_wGreyOverlay.Show(false);
        }

        // Colors
        int greenColor = ARGB(200, 40, 220, 40);
        int redColor   = ARGB(220, 220, 30, 30);
        int dimGreen   = ARGB(180, 40, 200, 40);

        // CamLabel: top-left
        float labelH = 28.0 * scale;
        float margin = 16.0 * scale;
        if (m_wCamLabel)
        {
            m_wCamLabel.SetPos(margin, margin);
            m_wCamLabel.SetSize(400.0 * scale, labelH);
            m_wCamLabel.SetColor(greenColor);
            m_wCamLabel.SetText("CAM-000000  [1/1]");
        }

        // RecLabel: top-right
        float recW = 60.0 * scale;
        if (m_wRecLabel)
        {
            float recX = sw - margin - recW;
            m_wRecLabel.SetPos(recX, margin);
            m_wRecLabel.SetSize(recW, labelH);
            m_wRecLabel.SetColor(redColor);
            m_wRecLabel.SetText("REC");
        }

        // TimestampLabel: bottom-left
        if (m_wTimestamp)
        {
            float tsY = sh - margin - labelH;
            m_wTimestamp.SetPos(margin, tsY);
            m_wTimestamp.SetSize(300.0 * scale, labelH);
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
        m_wGreyOverlay = null;
        m_wScanCanvas  = null;
        m_wCamLabel    = null;
        m_wRecLabel    = null;
        m_wTimestamp   = null;
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

        // Guardar referencia al player ANTES de entrar en spectator mode.
        // Después de SelectSpectator, GetPlayer() puede retornar null.
        // Se usa en Phase 2 para enviar RPC CCTV_EXIT.
        m_PlayerRef = p;

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
        m_ExitWaitTimer  = 0.0;

        // COT: disable player input during spectator
        if (m_PlayerRef)
        {
            HumanInputController hic = m_PlayerRef.GetInputController();
            if (hic)
            {
                hic.SetDisabled(true);
            }
        }

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
    // EnterCamera — obtener cámara engine-managed o reusar para cycling.
    //
    // Primera entrada: Camera.GetCurrentCamera() obtiene la cámara que
    // el engine creó via SelectSpectator (server-side).
    // Cycling: reposiciona la cámara existente sin recrearla.
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

        // Reset pan offsets for new camera view
        m_BaseOrientation = camOri;
        m_YawOffset   = 0.0;
        m_PitchOffset = 0.0;
        m_KeyW = false;
        m_KeyA = false;
        m_KeyS = false;
        m_KeyD = false;

        // Reusar objeto existente (cycling intra-sesión)
        if (m_ViewCamObj)
        {
            m_ViewCamObj.SetPosition(camPos);
            m_ViewCamObj.SetOrientation(camOri);
            m_CameraIndex = index;
            Print("[CameraViewport] DIAG: Reused existing camera object");
            return true;
        }

        // Primera entrada: obtener cámara del engine (SelectSpectator server-side).
        // COT Client_Enter pattern: Camera.GetCurrentCamera() devuelve la cámara
        // que el engine creó via SelectSpectator. NO usar CreateObject.
        // CreateObject crea un world object sin tracking → crash 0x68 al salir.
        Print("[CameraViewport] DIAG: Camera.GetCurrentCamera (SelectSpectator)");
        Camera currentCam = Camera.GetCurrentCamera();
        if (!currentCam)
        {
            LFPG_Util.Error("[CameraViewport] Camera.GetCurrentCamera returned null");
            return false;
        }

        currentCam.SetActive(true);
        currentCam.SetPosition(camPos);
        currentCam.SetOrientation(camOri);
        Print("[CameraViewport] DIAG: Engine camera acquired + positioned OK");

        m_ViewCamObj  = currentCam;
        m_CameraIndex = index;
        return true;
    }

    // =========================================================
    // HandleKeyDown — desde MissionGameplay.OnKeyPress.
    // WASD sets held flags. Q/E cycle. SPACE/ESC exit.
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

        // WASD pan — track held state
        if (key == LFPG_KC_W)
        {
            m_KeyW = true;
            return true;
        }
        if (key == LFPG_KC_A)
        {
            m_KeyA = true;
            return true;
        }
        if (key == LFPG_KC_S)
        {
            m_KeyS = true;
            return true;
        }
        if (key == LFPG_KC_D)
        {
            m_KeyD = true;
            return true;
        }

        return false;
    }

    // =========================================================
    // HandleKeyUp — desde MissionGameplay.OnKeyRelease.
    // Clears held state for WASD pan.
    // =========================================================
    void HandleKeyUp(int key)
    {
        if (key == LFPG_KC_W)
        {
            m_KeyW = false;
        }
        if (key == LFPG_KC_A)
        {
            m_KeyA = false;
        }
        if (key == LFPG_KC_S)
        {
            m_KeyS = false;
        }
        if (key == LFPG_KC_D)
        {
            m_KeyD = false;
        }
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
    // ForceCleanup — shutdown/disconnect cleanup.
    // Solo se llama desde Reset().
    // NO envía RPC — durante disconnect el network no es fiable.
    // El servidor limpia identities huérfanas automáticamente.
    // =========================================================
    protected void ForceCleanup()
    {
        Print("[CameraViewport] DIAG: ForceCleanup");
        m_Active    = false;
        m_ExitPhase = 0;
        m_ExitWaitTimer = 0.0;
        m_YawOffset   = 0.0;
        m_PitchOffset = 0.0;
        m_KeyW = false;
        m_KeyA = false;
        m_KeyS = false;
        m_KeyD = false;

        if (m_ViewCamObj)
        {
            Camera forceCleanCam = Camera.Cast(m_ViewCamObj);
            if (forceCleanCam)
            {
                forceCleanCam.SetActive(false);
            }
            // No ObjectDeleteOnClient — engine-managed spectator camera
            m_ViewCamObj = null;
        }

        m_PlayerRef = null;

        // Re-enable input in case we were in spectator mode
        Human cleanupPlayer = Human.Cast(GetGame().GetPlayer());
        if (cleanupPlayer)
        {
            HumanInputController cleanupHic = cleanupPlayer.GetInputController();
            if (cleanupHic)
            {
                cleanupHic.SetDisabled(false);
            }
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
    // DoExitCleanup — called from RPC CCTV_EXIT_CONFIRM.
    // Server has already called SelectPlayer(sender, player)
    // → engine updated internal camera pointer.
    // NOW safe to deactivate and release the spectator camera.
    //
    // COT: does NOT call ObjectDeleteOnClient for engine spectator
    // cameras. Only deletes custom JMCinematicCamera. Engine manages
    // spectator lifecycle via SelectPlayer.
    // We follow the same pattern: SetActive(false) + null. No delete.
    // =========================================================
    void DoExitCleanup()
    {
        Print("[CameraViewport] DIAG: DoExitCleanup — server confirmed");

        if (m_ViewCamObj)
        {
            Camera viewCamTyped = Camera.Cast(m_ViewCamObj);
            if (viewCamTyped)
            {
                viewCamTyped.SetActive(false);
            }
            // NO ObjectDeleteOnClient — staticcamera is engine-managed.
            // COT: IsInherited(JMCinematicCamera) check → only deletes custom.
            // staticcamera ≠ JMCinematicCamera → COT would NOT delete it.
            // Engine cleans up spectator camera when SelectPlayer restores player.
            m_ViewCamObj = null;
        }

        // Re-enable input controller (COT: SetDisabled(false) in Client_Leave)
        Human localPlayer = Human.Cast(GetGame().GetPlayer());
        if (localPlayer)
        {
            HumanInputController hic = localPlayer.GetInputController();
            if (hic)
            {
                hic.SetDisabled(false);
            }
        }

        m_PlayerRef = null;

        UnlockFocus();
        RestoreHUD();

        m_ExitPhase     = 0;
        m_ExitWaitTimer = 0.0;

        LFPG_Util.Info("[CameraViewport] DoExitCleanup complete — camera released");
    }

    // =========================================================
    // Tick — per-frame (runs AFTER super.OnUpdate)
    //
    // EXIT (COT ROUND-TRIP PATTERN):
    //   Phase 1: overlay off + RPC EXIT_REQUEST → server
    //   Phase 2: WAITING for server CCTV_EXIT_CONFIRM
    //            (server does SelectPlayer before confirming)
    //   DoExitCleanup: called from RPC → camera cleanup
    //
    //   This guarantees SelectPlayer propagates to client engine
    //   BEFORE we touch the camera. Zero dangling pointers.
    // =========================================================
    void Tick(float timeslice)
    {
        // ---- Phase 2: waiting for server confirmation ----
        // DON'T touch camera here. Server is processing SelectPlayer.
        // DoExitCleanup() will be called from RPC handler.
        // Timeout safety: if server never responds (5s), force cleanup.
        if (m_ExitPhase == 2)
        {
            m_ExitWaitTimer = m_ExitWaitTimer + timeslice;
            if (m_ExitWaitTimer >= 5.0)
            {
                LFPG_Util.Warn("[CameraViewport] Exit timeout — forcing cleanup");
                DoExitCleanup();
            }
        }

        // ---- Phase 1: send exit request to server ----
        // Camera STAYS ACTIVE — mods safe in next super.OnUpdate.
        if (m_ExitPhase == 1)
        {
            Print("[CameraViewport] DIAG: Phase 1 — m_Active=false + RPC EXIT_REQUEST");

            m_Active       = false;
            m_ExitCooldown = LFPG_CCTV_EXIT_COOLDOWN;

            if (m_OverlayRoot)
                m_OverlayRoot.Show(false);

            m_CameraLabel    = "";
            m_ActiveDuration = 0.0;
            m_ScanlineOffset = 0.0;
            m_YawOffset      = 0.0;
            m_PitchOffset    = 0.0;
            m_KeyW = false;
            m_KeyA = false;
            m_KeyS = false;
            m_KeyD = false;
            m_CameraList     = null;
            m_CameraIndex    = 0;
            m_CameraTotal    = 0;

            // Send EXIT_REQUEST — server will SelectPlayer then send CONFIRM
            if (m_PlayerRef)
            {
                ScriptRPC exitRpc = new ScriptRPC();
                int exitSubId = LFPG_RPC_SubId.CCTV_EXIT_REQUEST;
                exitRpc.Write(exitSubId);
                exitRpc.Send(m_PlayerRef, LFPG_RPC_CHANNEL, true, null);
                Print("[CameraViewport] DIAG: RPC EXIT_REQUEST sent");
            }

            m_ExitWaitTimer = 0.0;
            m_ExitPhase = 2;
            LFPG_Util.Info("[CameraViewport] Phase 1 complete — waiting for server confirm");
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

        // ---- WASD camera pan ----
        bool anyPan = false;
        if (m_KeyA || m_KeyD || m_KeyW || m_KeyS)
        {
            anyPan = true;
        }

        if (anyPan && m_ViewCamObj)
        {
            float panStep = LFPG_CCTV_PAN_SPEED * timeslice;

            // A/D = yaw (horizontal). A=left(+yaw), D=right(-yaw)
            if (m_KeyA)
            {
                m_YawOffset = m_YawOffset + panStep;
            }
            if (m_KeyD)
            {
                m_YawOffset = m_YawOffset - panStep;
            }

            // W/S = pitch (vertical). W=up(-pitch), S=down(+pitch)
            // DayZ: positive pitch = look down, negative = look up
            if (m_KeyW)
            {
                m_PitchOffset = m_PitchOffset - panStep;
            }
            if (m_KeyS)
            {
                m_PitchOffset = m_PitchOffset + panStep;
            }

            // Clamp to limits
            if (m_YawOffset > LFPG_CCTV_YAW_LIMIT)
            {
                m_YawOffset = LFPG_CCTV_YAW_LIMIT;
            }
            if (m_YawOffset < -LFPG_CCTV_YAW_LIMIT)
            {
                m_YawOffset = -LFPG_CCTV_YAW_LIMIT;
            }
            if (m_PitchOffset > LFPG_CCTV_PITCH_LIMIT)
            {
                m_PitchOffset = LFPG_CCTV_PITCH_LIMIT;
            }
            if (m_PitchOffset < -LFPG_CCTV_PITCH_LIMIT)
            {
                m_PitchOffset = -LFPG_CCTV_PITCH_LIMIT;
            }

            // Apply: base orientation + offsets
            // DayZ orientation vector: [yaw, pitch, roll]
            float newYaw   = m_BaseOrientation[0] + m_YawOffset;
            float newPitch = m_BaseOrientation[1] + m_PitchOffset;
            float newRoll  = m_BaseOrientation[2];
            vector panOri  = Vector(newYaw, newPitch, newRoll);
            m_ViewCamObj.SetOrientation(panOri);
        }

        // ---- Scanlines advance ----
        m_ScanlineOffset = m_ScanlineOffset + (LFPG_CCTV_SCROLL_SPEED * timeslice);
        while (m_ScanlineOffset >= LFPG_CCTV_SCANLINE_SPACING)
        {
            m_ScanlineOffset = m_ScanlineOffset - LFPG_CCTV_SCANLINE_SPACING;
        }
    }

    // =========================================================
    // DrawOverlay — uses own ScanlineCanvas (not CableHUD).
    // CableHUD canvas doesn't render over SelectSpectator view.
    // Our canvas is part of the overlay widget tree → renders correctly.
    // =========================================================
    void DrawOverlay(LFPG_CableHUD hud)
    {
        if (!m_Active)
            return;

        if (!m_wScanCanvas)
            return;

        m_wScanCanvas.Clear();

        int scrW = 0;
        int scrH = 0;
        GetScreenSize(scrW, scrH);
        float sw = scrW;
        float sh = scrH;

        if (sw <= 0.0 || sh <= 0.0)
            return;

        // Scanlines: horizontal lines scrolling down
        float lineY = m_ScanlineOffset;
        while (lineY < sh)
        {
            m_wScanCanvas.DrawLine(0.0, lineY, sw, lineY, 1.0, m_ScanColor);
            lineY = lineY + LFPG_CCTV_SCANLINE_SPACING;
        }

        // Vignette: dark edges (thick semi-transparent black lines)
        float vwScale = sh / 1080.0;
        float vw    = LFPG_CCTV_VIGNETTE_W * vwScale;
        float vhalf = vw * 0.5;

        // Left edge
        m_wScanCanvas.DrawLine(vhalf, 0.0, vhalf, sh, vw, m_VigColor);
        // Right edge
        float rX = sw - vhalf;
        m_wScanCanvas.DrawLine(rX, 0.0, rX, sh, vw, m_VigColor);
        // Top edge
        m_wScanCanvas.DrawLine(0.0, vhalf, sw, vhalf, vw, m_VigColor);
        // Bottom edge
        float bY = sh - vhalf;
        m_wScanCanvas.DrawLine(0.0, bY, sw, bY, vw, m_VigColor);
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
