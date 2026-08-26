#ifndef SERVER
// Client-only compilation boundary
// =========================================================
// LF_PowerGrid - CCTV Viewport Manager (v1.3.2)
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
//     Phase 1: m_Active=false, overlay off, HIC re-enable, RPC EXIT_REQUEST
//              Camera STAYS ACTIVE (engine needs active cam until SelectPlayer)
//     Phase 2: WAITING for server CCTV_EXIT_CONFIRM
//     DoExitCleanup: SetActive(false) + null refs + restore HUD
//
//     v1.3.2 FIX: v1.3.1 moved SetActive(false) to Phase 1 → crash
//     (engine had no active camera between Phase 1 and SelectPlayer).
//     HIC re-enable stays in Phase 1 (prevents 0x54 crash).
//
//   EXIT (server-side):
//     SelectPlayer(sender, player) → engine restaura player cam
//     RPC EXIT_CONFIRM → client → DoExitCleanup
//
//   KEY: CreateObject("staticcamera") = unmanaged → crash 0x68
//        SelectSpectator("staticcamera") = engine-managed → cleanup OK
//
//   SafeAbort: Camera/Monitor EEKilled/EEDelete call SafeAbort()
//     instead of Reset(). SafeAbort only queues exit if active.
//     Reset() destroyed widgets → overlay invisible on next entry.
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

// Forward offset (meters) from model center to clear the lens geometry.
// The spectator camera spawns at GetPosition() = model center; the lens
// protrudes forward from there. This pushes the viewpoint past the lens.
// Tune this value if the lens is still visible or the view is too far out.
static const float LFPG_CCTV_LENS_OFFSET_M    = 0.2;

static const int   LFPG_CCTV_EXIT_COOLDOWN    = 3;

// Key codes
static const int   LFPG_KC_ESCAPE = 1;
static const int   LFPG_KC_W      = 17;
static const int   LFPG_KC_Q      = 16;
static const int   LFPG_KC_E      = 18;
static const int   LFPG_KC_R      = 19;
static const int   LFPG_KC_A      = 30;
static const int   LFPG_KC_S      = 31;
static const int   LFPG_KC_D      = 32;
static const int   LFPG_KC_SPACE  = 57;

// Camera pan: slow pan speed, limited angles from base orientation
static const float LFPG_CCTV_PAN_SPEED     = 30.0;   // degrees per second

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
    protected bool           m_ScanOverlayBuilt;
    protected int            m_ScanOverlayW;
    protected int            m_ScanOverlayH;
    protected int            m_LastTimestampYear;
    protected int            m_LastTimestampMonth;
    protected int            m_LastTimestampDay;
    protected int            m_LastTimestampHour;
    protected int            m_LastTimestampMinute;

    // ---- Camera pan (WASD rotation from base orientation) ----
    protected vector    m_BaseOrientation;  // Original camera orientation
    protected float     m_YawOffset;        // Current yaw offset from base (-90..+90)
    protected float     m_PitchOffset;      // Current pitch offset from base (-45..+45)
    protected bool      m_KeyW;
    protected bool      m_KeyA;
    protected bool      m_KeyS;
    protected bool      m_KeyD;
    protected bool      m_AimDirty;
    protected int       m_AimReapplyFrames;

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
        m_ScanOverlayBuilt = false;
        m_ScanOverlayW = 0;
        m_ScanOverlayH = 0;
        m_LastTimestampYear = -1;
        m_LastTimestampMonth = -1;
        m_LastTimestampDay = -1;
        m_LastTimestampHour = -1;
        m_LastTimestampMinute = -1;
        m_BaseOrientation = vector.Zero;
        m_YawOffset      = 0.0;
        m_PitchOffset    = 0.0;
        m_KeyW           = false;
        m_KeyA           = false;
        m_KeyS           = false;
        m_KeyD           = false;
        m_AimDirty       = false;
        m_AimReapplyFrames = 0;
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
        if (g_Game.IsDedicatedServer())
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
    // SafeAbort — lightweight abort for Camera/Monitor EEKilled/EEDelete.
    //
    // Called when a camera or monitor entity is destroyed.
    // If CCTV is active → triggers exit (Phase 1).
    // If CCTV is NOT active → NO-OP.
    //
    // NEVER destroys widgets or the singleton. Widgets persist
    // across sessions and are reused on the next EnterFromList.
    // Reset() in Camera/Monitor was destroying the singleton
    // on every EEDelete → m_OverlayRoot wiped → overlay invisible
    // on next entry (m_LFPG_WidgetsCreated stays true in MissionInit).
    // =========================================================
    static void SafeAbort()
    {
        if (!s_Instance)
            return;

        if (!s_Instance.m_Active)
            return;

        // Already exiting — don't re-trigger Phase 1
        if (s_Instance.m_ExitPhase > 0)
            return;

        LFPG_Util.Debug("[CameraViewport] DIAG: SafeAbort — device destroyed, queueing exit");
        s_Instance.m_ExitPhase = 1;
    }

    // =========================================================
    // InitWidgets — positions set from script (resolution-independent)
    // Returns true if widgets are ready, false if creation failed.
    // =========================================================
    bool InitWidgets()
    {
        if (m_OverlayRoot)
            return true;

        LFPG_Util.Debug("[CameraViewport] DIAG: InitWidgets — pre-CreateWidgets");
        m_OverlayRoot = g_Game.GetWorkspace().CreateWidgets(LFPG_CCTV_LAYOUT);
        if (!m_OverlayRoot)
        {
            LFPG_Util.Error("[CameraViewport] Failed to create overlay from: " + LFPG_CCTV_LAYOUT);
            return false;
        }
        LFPG_Util.Debug("[CameraViewport] DIAG: InitWidgets — CreateWidgets OK");

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

        LFPG_Util.Debug("[CameraViewport] DIAG: InitWidgets complete");
        LFPG_Util.Info("[CameraViewport] Overlay widgets created (hidden)");
        return true;
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
    void EnterFromList(PlayerBase player, array<ref LFPG_CameraListEntry> entries)
    {
        LFPG_Util.Debug("[CameraViewport] DIAG: EnterFromList entry");

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

        // Dispatch supplies the original RPC target even while g_Game.GetPlayer() is null.
        m_PlayerRef = player;

        if (!entries)
        {
            LFPG_Util.Warn("[CameraViewport] EnterFromList: entries null");
            m_ExitPhase = 1;
            return;
        }

        if (entries.Count() == 0)
        {
            if (m_PlayerRef)
                m_PlayerRef.MessageStatus("[LFPG] No hay camaras activas conectadas.");
            m_ExitPhase = 1;
            return;
        }

        if (!m_PlayerRef || !m_PlayerRef.IsAlive() || m_PlayerRef.IsUnconscious())
        {
            LFPG_Util.Warn("[CameraViewport] EnterFromList: player unavailable or vital state invalid");
            m_ExitPhase = 1;
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

        LFPG_Util.Debug("[CameraViewport] DIAG: pre-EnterCamera(0)");
        bool camOk = EnterCamera(0);
        if (!camOk)
        {
            LFPG_Util.Error("[CameraViewport] EnterCamera failed — queueing coordinated exit");
            m_ExitPhase = 1;
            return;
        }
        LFPG_Util.Debug("[CameraViewport] DIAG: post-EnterCamera OK");

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

        if (m_PlayerRef)
        {
            string totalStr = m_CameraTotal.ToString();
            string enterMsg = "[LFPG] ";
            enterMsg = enterMsg + m_CameraLabel;
            enterMsg = enterMsg + " (1/";
            enterMsg = enterMsg + totalStr;
            enterMsg = enterMsg + ")  SPACE=Salir  Q/E=Ciclar  R=Centrar";
            m_PlayerRef.MessageStatus(enterMsg);
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

        m_BaseOrientation = camOri;
        m_YawOffset = entry.m_YawOffset;
        m_PitchOffset = entry.m_PitchOffset;
        if (m_YawOffset > LFPG_CCTV_YAW_LIMIT)
            m_YawOffset = LFPG_CCTV_YAW_LIMIT;
        if (m_YawOffset < -LFPG_CCTV_YAW_LIMIT)
            m_YawOffset = -LFPG_CCTV_YAW_LIMIT;
        if (m_PitchOffset > LFPG_CCTV_PITCH_LIMIT)
            m_PitchOffset = LFPG_CCTV_PITCH_LIMIT;
        if (m_PitchOffset < -LFPG_CCTV_PITCH_LIMIT)
            m_PitchOffset = -LFPG_CCTV_PITCH_LIMIT;

        float viewYaw = camOri[0] + m_YawOffset;
        float viewPitch = camOri[1] + m_PitchOffset;
        vector viewOri = Vector(viewYaw, viewPitch, camOri[2]);

        // Offset position forward along camera look direction
        // to prevent the lens model from appearing in front of the view.
        // camOri already has the +90 yaw correction (server-side) so
        // yaw=0 means looking North(+Z). Forward = (sin(yaw), 0, cos(yaw)).
        float yawRad = viewYaw * Math.DEG2RAD;
        float fwdX = Math.Sin(yawRad) * LFPG_CCTV_LENS_OFFSET_M;
        float fwdZ = Math.Cos(yawRad) * LFPG_CCTV_LENS_OFFSET_M;
        float oX = camPos[0] + fwdX;
        float oY = camPos[1];
        float oZ = camPos[2] + fwdZ;
        vector viewPos = Vector(oX, oY, oZ);

        m_KeyW = false;
        m_KeyA = false;
        m_KeyS = false;
        m_KeyD = false;
        m_AimDirty = false;

        // Reusar objeto existente (cycling intra-sesión)
        if (m_ViewCamObj)
        {
            m_ViewCamObj.SetPosition(viewPos);
            m_ViewCamObj.SetOrientation(viewOri);
            m_CameraIndex = index;
            // The engine can restore the static camera's spawn orientation
            // after this input/RPC callback. Reapply from normal update once
            // that transition has settled so stored PTZ is visible at once.
            m_AimReapplyFrames = 2;
            LFPG_Util.Debug("[CameraViewport] DIAG: Reused existing camera object");
            return true;
        }

        // Primera entrada: obtener cámara del engine (SelectSpectator server-side).
        // COT Client_Enter pattern: Camera.GetCurrentCamera() devuelve la cámara
        // que el engine creó via SelectSpectator. NO usar CreateObject.
        // CreateObject crea un world object sin tracking → crash 0x68 al salir.
        LFPG_Util.Debug("[CameraViewport] DIAG: Camera.GetCurrentCamera (SelectSpectator)");
        Camera currentCam = Camera.GetCurrentCamera();
        if (!currentCam)
        {
            LFPG_Util.Error("[CameraViewport] Camera.GetCurrentCamera returned null");
            return false;
        }

        currentCam.SetActive(true);
        currentCam.SetPosition(viewPos);
        currentCam.SetOrientation(viewOri);
        LFPG_Util.Debug("[CameraViewport] DIAG: Engine camera acquired + positioned OK");

        m_ViewCamObj  = currentCam;
        m_CameraIndex = index;
        m_AimReapplyFrames = 2;
        return true;
    }

    protected void ApplyCurrentAim()
    {
        if (!m_ViewCamObj)
            return;

        float viewYaw = m_BaseOrientation[0] + m_YawOffset;
        float viewPitch = m_BaseOrientation[1] + m_PitchOffset;
        vector viewOri = Vector(viewYaw, viewPitch, m_BaseOrientation[2]);
        m_ViewCamObj.SetOrientation(viewOri);
    }

    protected void CommitCurrentAim(bool forceCommit)
    {
        if (!m_CameraList || m_CameraIndex < 0 || m_CameraIndex >= m_CameraList.Count())
            return;
        if (!forceCommit && !m_AimDirty)
            return;

        LFPG_CameraListEntry entry = m_CameraList[m_CameraIndex];
        if (!entry || (entry.m_NetLow == 0 && entry.m_NetHigh == 0))
            return;

        entry.m_YawOffset = m_YawOffset;
        entry.m_PitchOffset = m_PitchOffset;

        if (m_PlayerRef)
        {
            ScriptRPC aimRpc = new ScriptRPC();
            aimRpc.Write((int)LFPG_RPC_SubId.CCTV_AIM);
            aimRpc.Write(entry.m_NetLow);
            aimRpc.Write(entry.m_NetHigh);
            aimRpc.Write(m_YawOffset);
            aimRpc.Write(m_PitchOffset);
            aimRpc.Send(m_PlayerRef, LFPG_RPC_CHANNEL, true, null);
        }

        m_AimDirty = false;
    }

    // =========================================================
    // HandleKeyDown — desde MissionGameplay.OnKeyPress.
    // WASD sets held flags. Q/E cycle. R centers. SPACE/ESC exits.
    // =========================================================
    bool HandleKeyDown(int key)
    {
        if (!m_Active)
            return false;

        if (key == LFPG_KC_SPACE || key == LFPG_KC_ESCAPE)
        {
            LFPG_Util.Debug("[CameraViewport] DIAG: EXIT queued via key=" + key.ToString());
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

        if (key == LFPG_KC_R)
        {
            m_YawOffset = 0.0;
            m_PitchOffset = 0.0;
            m_AimDirty = true;
            ApplyCurrentAim();
            CommitCurrentAim(true);
            if (m_PlayerRef)
                m_PlayerRef.MessageStatus("[LFPG] Camera centrada.");
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
        bool releasedPanKey = false;
        if (key == LFPG_KC_W)
        {
            m_KeyW = false;
            releasedPanKey = true;
        }
        if (key == LFPG_KC_A)
        {
            m_KeyA = false;
            releasedPanKey = true;
        }
        if (key == LFPG_KC_S)
        {
            m_KeyS = false;
            releasedPanKey = true;
        }
        if (key == LFPG_KC_D)
        {
            m_KeyD = false;
            releasedPanKey = true;
        }

        if (releasedPanKey)
            CommitCurrentAim(false);
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

        CommitCurrentAim(false);

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

        CommitCurrentAim(false);

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
        PlayerBase p = PlayerBase.Cast(g_Game.GetPlayer());
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
        LFPG_Util.Debug("[CameraViewport] DIAG: ForceCleanup");
        m_Active    = false;
        m_ExitPhase = 0;
        m_ExitWaitTimer = 0.0;
        m_YawOffset   = 0.0;
        m_PitchOffset = 0.0;
        m_KeyW = false;
        m_KeyA = false;
        m_KeyS = false;
        m_KeyD = false;
        m_AimDirty = false;
        m_AimReapplyFrames = 0;

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
        Human cleanupPlayer = g_Game.GetPlayer();
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
    // DoExitCleanup — called from RPC CCTV_EXIT_CONFIRM or timeout.
    // Server has already called SelectPlayer(sender, player)
    // → engine updated internal camera pointer.
    // NOW safe to deactivate and release the spectator camera.
    //
    // v1.3.2: Camera deactivation restored here (was in Phase 1
    // in v1.3.1 → crash). HIC re-enable stays in Phase 1.
    // Kept idempotent — safe to call multiple times.
    // =========================================================
    void DoExitCleanup()
    {
        // A duplicated confirm after the terminal transition is a strict no-op.
        if (!m_Active && m_ExitPhase == 0 && !m_ViewCamObj && !m_PlayerRef)
            return;

        LFPG_Util.Debug("[CameraViewport] DIAG: DoExitCleanup — server confirmed");

        m_Active = false;
        m_ExitCooldown = LFPG_CCTV_EXIT_COOLDOWN;

        if (m_OverlayRoot)
            m_OverlayRoot.Show(false);

        // Deactivate spectator camera only after SelectPlayer restored the player camera.
        if (m_ViewCamObj)
        {
            Camera viewCamTyped = Camera.Cast(m_ViewCamObj);
            if (viewCamTyped)
            {
                viewCamTyped.SetActive(false);
            }
            m_ViewCamObj = null;
        }

        HumanInputController cleanupHic = null;
        if (m_PlayerRef)
        {
            cleanupHic = m_PlayerRef.GetInputController();
        }
        else
        {
            Human localPlayer = g_Game.GetPlayer();
            if (localPlayer)
                cleanupHic = localPlayer.GetInputController();
        }
        if (cleanupHic)
            cleanupHic.SetDisabled(false);

        m_PlayerRef = null;

        UnlockFocus();
        RestoreHUD();

        m_CameraLabel = "";
        m_ActiveDuration = 0.0;
        m_ScanlineOffset = 0.0;
        m_YawOffset = 0.0;
        m_PitchOffset = 0.0;
        m_KeyW = false;
        m_KeyA = false;
        m_KeyS = false;
        m_KeyD = false;
        m_AimDirty = false;
        m_AimReapplyFrames = 0;
        m_CameraList = null;
        m_CameraIndex = 0;
        m_CameraTotal = 0;
        m_ExitPhase = 0;
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
        // Vital-state loss is a terminal transition and joins the existing exit FSM.
        if (m_Active && m_ExitPhase == 0)
        {
            if (!m_PlayerRef || !m_PlayerRef.IsAlive() || m_PlayerRef.IsUnconscious())
            {
                LFPG_Util.Info("[CameraViewport] Auto-exit (vital state)");
                m_ExitPhase = 1;
            }
        }

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
        // Camera STAYS ACTIVE until DoExitCleanup (after server confirm).
        //
        // v1.3.1 moved SetActive(false) here → crash: engine has no
        // active camera between Phase 1 and SelectPlayer. Reverted.
        //
        // HIC re-enable IS done here — prevents 0x54 crash when
        // SelectPlayer activates DayZPlayerCamera3rdPersonErc and
        // UpdateUDAngleUnlocked reads player input state.
        if (m_ExitPhase == 1)
        {
            LFPG_Util.Debug("[CameraViewport] DIAG: Phase 1 — m_Active=false + RPC EXIT_REQUEST");

            // Final reliable commit before the session is ended and the
            // camera list is released. This is a no-op when aim is unchanged.
            CommitCurrentAim(false);

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
            m_AimDirty = false;
            m_AimReapplyFrames = 0;
            m_CameraList     = null;
            m_CameraIndex    = 0;
            m_CameraTotal    = 0;

            // Re-enable HIC BEFORE RPC roundtrip.
            // Prevents 0x54 crash when SelectPlayer re-activates player cam.
            if (m_PlayerRef)
            {
                HumanInputController phase1Hic = m_PlayerRef.GetInputController();
                if (phase1Hic)
                {
                    phase1Hic.SetDisabled(false);
                }
            }

            // Camera STAYS ACTIVE — deactivated in DoExitCleanup after
            // server confirms via CCTV_EXIT_CONFIRM RPC.
            // v1.3.1 deactivated here → crash (no active camera for engine).

            // Send EXIT_REQUEST — server will SelectPlayer then send CONFIRM
            if (m_PlayerRef)
            {
                ScriptRPC exitRpc = new ScriptRPC();
                int exitSubId = LFPG_RPC_SubId.CCTV_EXIT_REQUEST;
                exitRpc.Write(exitSubId);
                exitRpc.Send(m_PlayerRef, LFPG_RPC_CHANNEL, true, null);
                LFPG_Util.Debug("[CameraViewport] DIAG: RPC EXIT_REQUEST sent");
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

        // SelectSpectator/staticcamera may overwrite SetOrientation after
        // EnterCamera returns. A short deferred reapply avoids showing center
        // until the first pan input without adding a permanent per-frame set.
        if (m_AimReapplyFrames > 0)
        {
            ApplyCurrentAim();
            m_AimReapplyFrames = m_AimReapplyFrames - 1;
        }

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
            g_Game.GetWorld().GetDate(year, month, day, hour, minute);

            bool timestampDirty = (year != m_LastTimestampYear || month != m_LastTimestampMonth || day != m_LastTimestampDay || hour != m_LastTimestampHour || minute != m_LastTimestampMinute);
            if (timestampDirty)
            {
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
                m_LastTimestampYear = year;
                m_LastTimestampMonth = month;
                m_LastTimestampDay = day;
                m_LastTimestampHour = hour;
                m_LastTimestampMinute = minute;
            }
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
            float oldYawOffset = m_YawOffset;
            float oldPitchOffset = m_PitchOffset;

            // A/D = yaw (horizontal). A=left(-yaw), D=right(+yaw)
            if (m_KeyA)
            {
                m_YawOffset = m_YawOffset - panStep;
            }
            if (m_KeyD)
            {
                m_YawOffset = m_YawOffset + panStep;
            }

            // W/S = pitch (vertical). W=up(+pitch), S=down(-pitch)
            if (m_KeyW)
            {
                m_PitchOffset = m_PitchOffset + panStep;
            }
            if (m_KeyS)
            {
                m_PitchOffset = m_PitchOffset - panStep;
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

            if (m_YawOffset != oldYawOffset || m_PitchOffset != oldPitchOffset)
            {
                m_AimDirty = true;
                ApplyCurrentAim();
            }
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
    void DrawOverlay()
    {
        if (!m_Active)
            return;

        if (!m_wScanCanvas)
            return;

        int scrW = 0;
        int scrH = 0;
        GetScreenSize(scrW, scrH);
        float sw = scrW;
        float sh = scrH;

        if (sw <= 0.0 || sh <= 0.0)
            return;

        // Static cached Canvas geometry. Rebuild only when resolution changes.
        if (m_ScanOverlayBuilt && scrW == m_ScanOverlayW && scrH == m_ScanOverlayH)
            return;

        m_wScanCanvas.Clear();
        m_ScanOverlayBuilt = true;
        m_ScanOverlayW = scrW;
        m_ScanOverlayH = scrH;

        // Scanlines: horizontal lines cached on the overlay canvas.
        float lineY = 0.0;
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

        Input inp = g_Game.GetInput();
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

        Input inp = g_Game.GetInput();
        if (inp)
        {
            inp.ChangeGameFocus(-1);
        }
        m_FocusLocked = false;
    }

    protected void HideHUD()
    {
        Mission mission = g_Game.GetMission();
        if (mission)
        {
            Hud hud = mission.GetHud();
            if (hud)
            {
                hud.ShowHudPlayer(false);
                hud.ShowQuickbarPlayer(false);
            }
        }

        UIManager uiMgr = g_Game.GetUIManager();
        if (uiMgr)
        {
            uiMgr.ShowUICursor(false);
        }
    }

    protected void RestoreHUD()
    {
        Mission mission = g_Game.GetMission();
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
#endif

#ifdef SERVER
static const float LFPG_CCTV_LENS_OFFSET_M = 0.2;

// Server-side no-op stub: keeps LFPG_CameraViewport type plus the public and
// externally consumed protected surface resolvable after the client boundary
// removed the implementation from the server type surface (SP-075).
class LFPG_CameraViewport
{
    protected Object m_ViewCamObj;
    protected ref array<ref LFPG_CameraListEntry> m_CameraList;
    protected vector m_BaseOrientation;

    void LFPG_CameraViewport()
    {
    }

    static LFPG_CameraViewport Get()
    {
        return null;
    }

    static void Reset()
    {
    }

    static void SafeAbort()
    {
    }

    bool IsActive()
    {
        return false;
    }

    bool ShouldSkipInspector()
    {
        return false;
    }

    bool InitWidgets()
    {
        return false;
    }

    void EnterFromList(array<ref LFPG_CameraListEntry> entries)
    {
    }

    protected bool EnterCamera(int index)
    {
        return false;
    }

    bool HandleKeyDown(int key)
    {
        return false;
    }

    void HandleKeyUp(int key)
    {
    }

    void CycleNext()
    {
    }

    void CyclePrev()
    {
    }

    void DoExitCleanup()
    {
    }

    void Tick(float timeslice)
    {
    }

    void DrawOverlay()
    {
    }
};
#endif
