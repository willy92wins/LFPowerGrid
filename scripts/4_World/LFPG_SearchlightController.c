// =========================================================
// LF_PowerGrid - Searchlight Controller (v1.4.0)
//
// Client-side singleton for spectator control of LF_Searchlight.
// Pattern: COT (same as LFPG_CameraViewport).
//
// ENTER (server-side):
//   SelectPlayer(sender, NULL) + SelectSpectator(sender, "staticcamera", pos)
//   → RPC SEARCHLIGHT_ENTER_CONFIRM(yaw, pitch) → client
//
// ENTER (client-side):
//   Camera.GetCurrentCamera() → SetActive(true) + position behind beam
//   Show HUD overlay
//
// TICK (client-side):
//   Accumulate WASD → yaw/pitch (30 deg/s)
//   Clamp: yaw [-120,120], pitch [-10,45]
//   Update spectator camera LOCAL (responsive)
//   Throttle RPC SEARCHLIGHT_AIM every 100ms
//
// EXIT (COT round-trip):
//   Phase 1: RPC EXIT_REQUEST → server
//   Phase 2: Wait for CONFIRM (5s timeout)
//   DoExitCleanup: re-enable input, hide HUD, release camera
//
// Enforce Script: no ternaries, no ++/--, no foreach, no +=/-=.
// =========================================================

static const string LFPG_SL_HUD_LAYOUT = "LFPowerGrid/gui/layouts/LFPG_SearchlightHUD.layout";

class LFPG_SearchlightController
{
    // ---- Singleton ----
    protected static ref LFPG_SearchlightController s_Instance;

    // ---- Camera (engine-managed via SelectSpectator) ----
    protected Object    m_ViewCamObj;
    protected bool      m_Active;
    protected float     m_ActiveDuration;

    // ---- Player reference (saved before SelectSpectator) ----
    protected PlayerBase m_PlayerRef;

    // ---- Searchlight NetworkID (for RPCs) ----
    protected int       m_TargetNetLow;
    protected int       m_TargetNetHigh;

    // ---- Aim state ----
    protected float     m_AimYaw;
    protected float     m_AimPitch;

    // ---- Key tracking ----
    protected bool      m_KeyW;
    protected bool      m_KeyA;
    protected bool      m_KeyS;
    protected bool      m_KeyD;

    // ---- RPC throttle ----
    protected float     m_RpcAccum;
    protected bool      m_AimDirty;

    // ---- Cached searchlight world pos (avoid NetworkID resolve per frame) ----
    protected vector    m_CachedSlPos;

    // ---- HUD change tracking (avoid string alloc every frame) ----
    protected int       m_PrevHudYaw;
    protected int       m_PrevHudPitch;

    // ---- Two-phase exit (COT pattern) ----
    protected int       m_ExitPhase;
    protected float     m_ExitWaitTimer;

    // ---- HUD ----
    protected Widget      m_HudRoot;
    protected TextWidget  m_wAimText;
    protected TextWidget  m_wHintText;
    protected TextWidget  m_wCrosshair;

    // ---- Focus lock ----
    protected bool      m_FocusLocked;

    // ============================================
    // Constructor
    // ============================================
    void LFPG_SearchlightController()
    {
        m_ViewCamObj    = null;
        m_Active        = false;
        m_ActiveDuration = 0.0;
        m_PlayerRef     = null;
        m_TargetNetLow  = 0;
        m_TargetNetHigh = 0;
        m_AimYaw        = 0.0;
        m_AimPitch      = 0.0;
        m_KeyW          = false;
        m_KeyA          = false;
        m_KeyS          = false;
        m_KeyD          = false;
        m_RpcAccum      = 0.0;
        m_AimDirty      = false;
        m_CachedSlPos   = "0 0 0";
        m_PrevHudYaw    = -9999;
        m_PrevHudPitch  = -9999;
        m_ExitPhase     = 0;
        m_ExitWaitTimer = 0.0;
        m_HudRoot       = null;
        m_wAimText      = null;
        m_wHintText     = null;
        m_wCrosshair    = null;
        m_FocusLocked   = false;
    }

    // ============================================
    // Singleton
    // ============================================
    static LFPG_SearchlightController Get()
    {
        if (GetGame().IsDedicatedServer())
            return null;

        if (!s_Instance)
            s_Instance = new LFPG_SearchlightController();
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

    bool ShouldBlockInput()
    {
        if (m_Active)
            return true;
        if (m_ExitPhase > 0)
            return true;
        return false;
    }

    // ============================================
    // InitWidgets — called from MissionGameplay.OnUpdate (safe context)
    // ============================================
    void InitWidgets()
    {
        if (m_HudRoot)
            return;

        m_HudRoot = GetGame().GetWorkspace().CreateWidgets(LFPG_SL_HUD_LAYOUT);
        if (!m_HudRoot)
        {
            LFPG_Util.Error("[SearchlightCtrl] Failed to create HUD layout");
            return;
        }

        m_HudRoot.SetSort(10003);

        string wAim   = "AimText";
        string wHint  = "HintText";
        string wCross = "Crosshair";
        m_wAimText   = TextWidget.Cast(m_HudRoot.FindAnyWidget(wAim));
        m_wHintText  = TextWidget.Cast(m_HudRoot.FindAnyWidget(wHint));
        m_wCrosshair = TextWidget.Cast(m_HudRoot.FindAnyWidget(wCross));

        // Position widgets based on screen size
        int scrW = 0;
        int scrH = 0;
        GetScreenSize(scrW, scrH);
        float sw = scrW;
        float sh = scrH;
        float scale = sh / 1080.0;

        int amberColor = ARGB(220, 255, 180, 40);
        int whiteColor = ARGB(200, 220, 220, 200);

        float labelH = 24.0 * scale;
        float margin = 16.0 * scale;

        // AimText: bottom-left
        if (m_wAimText)
        {
            float aimY = sh - margin - labelH;
            m_wAimText.SetPos(margin, aimY);
            m_wAimText.SetSize(300.0 * scale, labelH);
            m_wAimText.SetColor(amberColor);
            m_wAimText.SetText("YAW: 0   PITCH: 0");
        }

        // HintText: bottom-center
        if (m_wHintText)
        {
            float hintW = 350.0 * scale;
            float hintX = (sw - hintW) * 0.5;
            float hintY = sh - margin - labelH;
            m_wHintText.SetPos(hintX, hintY);
            m_wHintText.SetSize(hintW, labelH);
            m_wHintText.SetColor(whiteColor);
            m_wHintText.SetText("WASD: Aim  |  ESC: Exit");
        }

        // Crosshair: center of screen
        if (m_wCrosshair)
        {
            float crossW = 40.0 * scale;
            float crossH = 30.0 * scale;
            float crossX = (sw - crossW) * 0.5;
            float crossY = (sh - crossH) * 0.5;
            m_wCrosshair.SetPos(crossX, crossY);
            m_wCrosshair.SetSize(crossW, crossH);
            m_wCrosshair.SetColor(amberColor);
            m_wCrosshair.SetText("+");
        }

        m_HudRoot.Show(false);
        LFPG_Util.Info("[SearchlightCtrl] HUD widgets created (hidden)");
    }

    protected void DestroyWidgets()
    {
        if (m_HudRoot)
        {
            m_HudRoot.Unlink();
            m_HudRoot = null;
        }
        m_wAimText   = null;
        m_wHintText  = null;
        m_wCrosshair = null;
    }

    // ============================================
    // Enter — called from RPC SEARCHLIGHT_ENTER_CONFIRM
    // Server has already called SelectSpectator.
    // ============================================
    void Enter(int netLow, int netHigh, float yaw, float pitch)
    {
        if (m_Active)
        {
            LFPG_Util.Warn("[SearchlightCtrl] Enter: already active, ignoring");
            return;
        }

        if (m_ExitPhase > 0)
        {
            LFPG_Util.Warn("[SearchlightCtrl] Enter: exit in progress, ignoring");
            return;
        }

        PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
        m_PlayerRef = p;

        m_TargetNetLow  = netLow;
        m_TargetNetHigh = netHigh;
        m_AimYaw        = yaw;
        m_AimPitch      = pitch;
        m_KeyW  = false;
        m_KeyA  = false;
        m_KeyS  = false;
        m_KeyD  = false;
        m_RpcAccum      = 0.0;
        m_AimDirty      = false;
        m_ActiveDuration = 0.0;
        m_ExitPhase     = 0;
        m_ExitWaitTimer = 0.0;
        m_PrevHudYaw    = -9999;
        m_PrevHudPitch  = -9999;

        // Cache searchlight world position (avoid NetworkID resolve per frame)
        Object slObj = GetGame().GetObjectByNetworkId(netLow, netHigh);
        if (slObj)
        {
            m_CachedSlPos = slObj.GetPosition();
        }

        // Get engine-managed camera from SelectSpectator
        Camera currentCam = Camera.GetCurrentCamera();
        if (!currentCam)
        {
            LFPG_Util.Error("[SearchlightCtrl] Camera.GetCurrentCamera returned null");
            m_PlayerRef = null;
            return;
        }

        currentCam.SetActive(true);
        m_ViewCamObj = currentCam;

        // Position camera behind the beam
        PositionCamera();

        // Lock input
        LockFocus();
        if (m_PlayerRef)
        {
            HumanInputController hic = m_PlayerRef.GetInputController();
            if (hic)
            {
                hic.SetDisabled(true);
            }
        }

        // Show HUD
        if (m_HudRoot)
        {
            m_HudRoot.Show(true);
        }

        m_Active = true;

        if (p)
        {
            p.MessageStatus("[LFPG] Searchlight: WASD=Aim  ESC=Exit");
        }

        LFPG_Util.Info("[SearchlightCtrl] Entered spectator mode");
    }

    // ============================================
    // PositionCamera — place camera behind beam direction
    // ============================================
    protected void PositionCamera()
    {
        if (!m_ViewCamObj)
            return;

        // Use cached position (resolved once on Enter)
        vector slPos = m_CachedSlPos;

        // Beam direction from yaw/pitch angles
        float yawRad = m_AimYaw * Math.DEG2RAD;
        float pitchRad = m_AimPitch * Math.DEG2RAD;
        float cosPitch = Math.Cos(pitchRad);

        float dirX = Math.Sin(yawRad) * cosPitch;
        float dirY = Math.Sin(pitchRad);
        float dirZ = Math.Cos(yawRad) * cosPitch;

        // Camera = searchlight pos - (beamDir * BEHIND) + (0, UP, 0)
        float camX = slPos[0] - dirX * LFPG_SEARCHLIGHT_CAM_BEHIND_M;
        float camY = slPos[1] + 0.83 - dirY * LFPG_SEARCHLIGHT_CAM_BEHIND_M + LFPG_SEARCHLIGHT_CAM_UP_M;
        float camZ = slPos[2] - dirZ * LFPG_SEARCHLIGHT_CAM_BEHIND_M;
        vector camPos = Vector(camX, camY, camZ);

        // Camera orientation: look along beam
        vector camOri = Vector(m_AimYaw, m_AimPitch, 0.0);

        m_ViewCamObj.SetPosition(camPos);
        m_ViewCamObj.SetOrientation(camOri);
    }

    // ============================================
    // HandleKeyDown
    // ============================================
    bool HandleKeyDown(int key)
    {
        if (!m_Active)
            return false;

        if (key == LFPG_KC_ESCAPE)
        {
            m_ExitPhase = 1;
            return true;
        }

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

    // ============================================
    // HandleKeyUp
    // ============================================
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

    // ============================================
    // Tick — per-frame from MissionGameplay.OnUpdate
    // ============================================
    void Tick(float timeslice)
    {
        // ---- Phase 2: waiting for server confirmation ----
        if (m_ExitPhase == 2)
        {
            m_ExitWaitTimer = m_ExitWaitTimer + timeslice;
            if (m_ExitWaitTimer >= LFPG_SEARCHLIGHT_EXIT_TIMEOUT_S)
            {
                LFPG_Util.Warn("[SearchlightCtrl] Exit timeout — forcing cleanup");
                DoExitCleanup();
            }
            return;
        }

        // ---- Phase 1: send exit request to server ----
        if (m_ExitPhase == 1)
        {
            m_Active = false;

            if (m_HudRoot)
                m_HudRoot.Show(false);

            m_KeyW = false;
            m_KeyA = false;
            m_KeyS = false;
            m_KeyD = false;

            if (m_PlayerRef)
            {
                ScriptRPC exitRpc = new ScriptRPC();
                int exitSubId = LFPG_RPC_SubId.SEARCHLIGHT_EXIT_REQUEST;
                exitRpc.Write(exitSubId);
                exitRpc.Send(m_PlayerRef, LFPG_RPC_CHANNEL, true, null);
            }

            m_ExitWaitTimer = 0.0;
            m_ExitPhase = 2;
            LFPG_Util.Info("[SearchlightCtrl] Phase 1 — exit request sent");
            return;
        }

        if (!m_Active)
            return;

        // ---- Timeout ----
        m_ActiveDuration = m_ActiveDuration + timeslice;
        if (m_ActiveDuration >= LFPG_CCTV_MAX_DURATION_S)
        {
            LFPG_Util.Info("[SearchlightCtrl] Auto-exit (timeout)");
            m_ExitPhase = 1;
            return;
        }

        // ---- WASD aim ----
        bool anyInput = false;
        if (m_KeyA || m_KeyD || m_KeyW || m_KeyS)
        {
            anyInput = true;
        }

        if (anyInput)
        {
            float panStep = LFPG_SEARCHLIGHT_PAN_SPEED * timeslice;

            // A = left (-yaw), D = right (+yaw)
            if (m_KeyA)
            {
                m_AimYaw = m_AimYaw - panStep;
            }
            if (m_KeyD)
            {
                m_AimYaw = m_AimYaw + panStep;
            }

            // W = up (+pitch), S = down (-pitch)
            if (m_KeyW)
            {
                m_AimPitch = m_AimPitch + panStep;
            }
            if (m_KeyS)
            {
                m_AimPitch = m_AimPitch - panStep;
            }

            // Clamp
            if (m_AimYaw < LFPG_SEARCHLIGHT_YAW_MIN)
                m_AimYaw = LFPG_SEARCHLIGHT_YAW_MIN;
            if (m_AimYaw > LFPG_SEARCHLIGHT_YAW_MAX)
                m_AimYaw = LFPG_SEARCHLIGHT_YAW_MAX;
            if (m_AimPitch < LFPG_SEARCHLIGHT_PITCH_MIN)
                m_AimPitch = LFPG_SEARCHLIGHT_PITCH_MIN;
            if (m_AimPitch > LFPG_SEARCHLIGHT_PITCH_MAX)
                m_AimPitch = LFPG_SEARCHLIGHT_PITCH_MAX;

            // Update camera LOCAL (responsive)
            PositionCamera();
            m_AimDirty = true;
        }

        // ---- RPC throttle: only send when aim changed ----
        m_RpcAccum = m_RpcAccum + timeslice * 1000.0;
        if (m_AimDirty && m_RpcAccum >= LFPG_SEARCHLIGHT_RPC_THROTTLE_MS)
        {
            m_RpcAccum = 0.0;
            m_AimDirty = false;
            SendAimRPC();
        }

        // ---- Update HUD: only when values change ----
        int curYawInt = m_AimYaw;
        int curPitchInt = m_AimPitch;
        if (curYawInt != m_PrevHudYaw || curPitchInt != m_PrevHudPitch)
        {
            m_PrevHudYaw   = curYawInt;
            m_PrevHudPitch = curPitchInt;
            UpdateHUD();
        }
    }

    // ============================================
    // SendAimRPC — throttled to server
    // ============================================
    protected void SendAimRPC()
    {
        if (!m_PlayerRef)
            return;

        ScriptRPC rpc = new ScriptRPC();
        int subId = LFPG_RPC_SubId.SEARCHLIGHT_AIM;
        rpc.Write(subId);
        rpc.Write(m_TargetNetLow);
        rpc.Write(m_TargetNetHigh);
        rpc.Write(m_AimYaw);
        rpc.Write(m_AimPitch);
        rpc.Send(m_PlayerRef, LFPG_RPC_CHANNEL, true, null);
    }

    // ============================================
    // UpdateHUD
    // ============================================
    protected void UpdateHUD()
    {
        if (!m_wAimText)
            return;

        int yawInt = m_AimYaw;
        int pitchInt = m_AimPitch;
        string aimStr = "YAW: ";
        aimStr = aimStr + yawInt.ToString();
        aimStr = aimStr + "   PITCH: ";
        aimStr = aimStr + pitchInt.ToString();
        m_wAimText.SetText(aimStr);
    }

    // ============================================
    // DoExitCleanup — called from RPC SEARCHLIGHT_EXIT_CONFIRM
    // ============================================
    void DoExitCleanup()
    {
        if (m_ViewCamObj)
        {
            Camera viewCamTyped = Camera.Cast(m_ViewCamObj);
            if (viewCamTyped)
            {
                viewCamTyped.SetActive(false);
            }
            m_ViewCamObj = null;
        }

        // Re-enable input
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

        if (m_HudRoot)
            m_HudRoot.Show(false);

        m_ExitPhase     = 0;
        m_ExitWaitTimer = 0.0;
        m_Active        = false;

        LFPG_Util.Info("[SearchlightCtrl] DoExitCleanup complete");
    }

    // ============================================
    // ForceCleanup — shutdown/disconnect
    // ============================================
    protected void ForceCleanup()
    {
        m_Active    = false;
        m_ExitPhase = 0;
        m_ExitWaitTimer = 0.0;
        m_KeyW = false;
        m_KeyA = false;
        m_KeyS = false;
        m_KeyD = false;

        if (m_ViewCamObj)
        {
            Camera forceCam = Camera.Cast(m_ViewCamObj);
            if (forceCam)
            {
                forceCam.SetActive(false);
            }
            m_ViewCamObj = null;
        }

        m_PlayerRef = null;

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
        DestroyWidgets();

        m_ActiveDuration = 0.0;
        m_RpcAccum       = 0.0;
        m_AimDirty       = false;
        m_CachedSlPos    = "0 0 0";
        m_PrevHudYaw     = -9999;
        m_PrevHudPitch   = -9999;
    }

    // ============================================
    // Focus lock helpers
    // ============================================
    protected void LockFocus()
    {
        if (m_FocusLocked)
            return;
        m_FocusLocked = true;
        GetGame().GetInput().ChangeGameFocus(1);
        GetGame().GetUIManager().ShowUICursor(false);
    }

    protected void UnlockFocus()
    {
        if (!m_FocusLocked)
            return;
        m_FocusLocked = false;
        GetGame().GetInput().ChangeGameFocus(-1);
    }
};
