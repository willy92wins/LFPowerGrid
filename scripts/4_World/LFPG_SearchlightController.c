// =========================================================
// LF_PowerGrid - Searchlight Grab Controller (v1.5.0)
//
// Lightweight client-side singleton. Player walks up to a
// powered searchlight, presses F to "grab" it, then:
//   YAW:   computed from player position relative to foco
//           (beam points toward player = drag-the-barrel)
//   PITCH: scroll wheel (slow increments, +-120 deg)
//   EXIT:  F again (action toggle) or distance > 2m
//
// NO spectator, NO COT, NO camera, NO HUD overlay.
// The player remains in normal 3rd/1st person the whole time.
//
// Tick is called from MissionGameplay.OnUpdate.
// Scroll is captured via UANextAction / UAPrevAction.
//
// Enforce Script: no ternaries, no ++/--, no foreach, no +=/-=.
// =========================================================

class LFPG_SearchlightController
{
    // ---- Singleton ----
    protected static ref LFPG_SearchlightController s_Instance;

    // ---- Target searchlight ----
    protected int    m_TargetNetLow;
    protected int    m_TargetNetHigh;
    protected bool   m_Active;

    // ---- Cached searchlight ref (resolved once on Enter) ----
    protected LF_Searchlight m_TargetSl;

    // ---- Aim state ----
    protected float  m_AimYaw;
    protected float  m_AimPitch;

    // ---- RPC throttle ----
    protected float  m_RpcAccum;
    protected bool   m_AimDirty;

    // ============================================
    // Constructor
    // ============================================
    void LFPG_SearchlightController()
    {
        m_TargetNetLow  = 0;
        m_TargetNetHigh = 0;
        m_Active        = false;
        m_TargetSl      = null;
        m_AimYaw        = 0.0;
        m_AimPitch      = 0.0;
        m_RpcAccum      = 0.0;
        m_AimDirty      = false;
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

    // Returns true if this controller is operating the given searchlight
    bool IsOperating(int netLow, int netHigh)
    {
        if (!m_Active)
            return false;
        if (m_TargetNetLow == netLow && m_TargetNetHigh == netHigh)
            return true;
        return false;
    }

    // Returns true if this controller is operating the given entity
    bool IsOperatingEntity(LF_Searchlight sl)
    {
        if (!m_Active)
            return false;
        if (m_TargetSl == sl)
            return true;
        return false;
    }

    // ============================================
    // Enter -- called from RPC SEARCHLIGHT_ENTER_CONFIRM
    // ============================================
    void Enter(int netLow, int netHigh, float yaw, float pitch)
    {
        if (m_Active)
        {
            LFPG_Util.Warn("[SearchlightCtrl] Enter: already active, ignoring");
            return;
        }

        m_TargetNetLow  = netLow;
        m_TargetNetHigh = netHigh;
        m_AimYaw        = yaw;
        m_AimPitch      = pitch;
        m_RpcAccum      = 0.0;
        m_AimDirty      = false;

        // Resolve searchlight entity
        Object slObj = GetGame().GetObjectByNetworkId(netLow, netHigh);
        if (!slObj)
        {
            LFPG_Util.Error("[SearchlightCtrl] Cannot resolve NetworkID on Enter");
            return;
        }

        m_TargetSl = LF_Searchlight.Cast(slObj);
        if (!m_TargetSl)
        {
            LFPG_Util.Error("[SearchlightCtrl] Object is not LF_Searchlight on Enter");
            return;
        }

        m_Active = true;

        PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
        if (p)
        {
            p.MessageStatus("[LFPG] Searchlight grabbed. Scroll=Pitch  Walk=Aim  F=Release");
        }

        string logMsg = "[SearchlightCtrl] Grab started netId=";
        logMsg = logMsg + netLow.ToString();
        logMsg = logMsg + ":" + netHigh.ToString();
        LFPG_Util.Info(logMsg);
    }

    // ============================================
    // Exit -- send RPC to server + cleanup locally
    // ============================================
    void RequestExit()
    {
        if (!m_Active)
            return;

        PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
        if (p)
        {
            ScriptRPC rpc = new ScriptRPC();
            int subId = LFPG_RPC_SubId.SEARCHLIGHT_EXIT_REQUEST;
            rpc.Write(subId);
            rpc.Write(m_TargetNetLow);
            rpc.Write(m_TargetNetHigh);
            rpc.Send(p, LFPG_RPC_CHANNEL, true, null);
        }

        DoCleanup();
    }

    // ============================================
    // DoCleanup -- server-initiated or local exit
    // ============================================
    void DoCleanup()
    {
        m_Active        = false;
        m_TargetSl      = null;
        m_TargetNetLow  = 0;
        m_TargetNetHigh = 0;
        m_RpcAccum      = 0.0;
        m_AimDirty      = false;

        LFPG_Util.Info("[SearchlightCtrl] Grab released");
    }

    // ============================================
    // Tick -- called every frame from MissionGameplay.OnUpdate
    // ============================================
    void Tick(float timeslice)
    {
        if (!m_Active)
            return;

        // ---- Validate target still exists ----
        if (!m_TargetSl)
        {
            DoCleanup();
            return;
        }

        // ---- Get player ----
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player)
        {
            DoCleanup();
            return;
        }

        // ---- Distance check: auto-exit if > grab radius ----
        vector playerPos = player.GetPosition();
        vector slPos     = m_TargetSl.GetPosition();
        float dx = playerPos[0] - slPos[0];
        float dz = playerPos[2] - slPos[2];
        float distSq = dx * dx + dz * dz;
        float maxDistSq = LFPG_SEARCHLIGHT_GRAB_RADIUS_M * LFPG_SEARCHLIGHT_GRAB_RADIUS_M;

        if (distSq > maxDistSq)
        {
            LFPG_Util.Info("[SearchlightCtrl] Auto-exit: player too far");
            RequestExit();
            return;
        }

        // ---- F-key exit (bypasses action system) ----
        // The action LFPG_ActionOperateSearchlight uses CCTCursor which
        // requires crosshair over the searchlight. While operating, the
        // player walks around aiming the beam — crosshair is NOT on the
        // searchlight, so the action never resolves and F does nothing.
        // Detect UAAction (F key) directly here for reliable exit.
        Input exitInp = GetGame().GetInput();
        if (exitInp)
        {
            string uaAction = "UAAction";
            if (exitInp.LocalPress(uaAction, false))
            {
                RequestExit();
                return;
            }
        }

        // ---- Compute yaw from player position ----
        // Beam points OPPOSITE to player = AWAY from player.
        // Vector FROM player TO searchlight, extended through = beam direction.
        // Atan2(dx, dz) gives world angle from +Z toward +X.
        // Subtract searchlight BASE orientation (deployment yaw, frozen)
        // to get local yaw. NOT GetOrientation which changes every frame.
        float awayX = slPos[0] - playerPos[0];
        float awayZ = slPos[2] - playerPos[2];
        float worldRad = Math.Atan2(awayX, awayZ);
        float worldDeg = worldRad * Math.RAD2DEG;
        float slBaseYaw = m_TargetSl.LFPG_GetBaseYaw();
        float localYaw = worldDeg - slBaseYaw;

        // Normalize to [-180, 180]
        while (localYaw > 180.0)
        {
            localYaw = localYaw - 360.0;
        }
        while (localYaw < -180.0)
        {
            localYaw = localYaw + 360.0;
        }

        // No yaw clamp — full 360 degree rotation.
        // Always update (no deadzone — smooth tracking)
        m_AimYaw = localYaw;
        m_AimDirty = true;

        // ---- Read scroll wheel for pitch ----
        // LocalPress gives exactly one event per scroll notch (no cooldown needed).
        // UANextAction/UAPrevAction = vanilla scroll-wheel action-menu bindings.
        // With only one action visible ("Release Searchlight"), the action menu
        // scroll is inert. If conflict arises with multi-action objects nearby,
        // this can be switched to a different input binding.
        Input inp = GetGame().GetInput();
        if (inp)
        {
            bool scrolled = false;

            if (inp.LocalPress("UANextAction", false))
            {
                m_AimPitch = m_AimPitch + LFPG_SEARCHLIGHT_SCROLL_STEP;
                scrolled = true;
            }
            if (inp.LocalPress("UAPrevAction", false))
            {
                m_AimPitch = m_AimPitch - LFPG_SEARCHLIGHT_SCROLL_STEP;
                scrolled = true;
            }

            if (scrolled)
            {
                // Clamp pitch
                if (m_AimPitch < LFPG_SEARCHLIGHT_PITCH_MIN)
                    m_AimPitch = LFPG_SEARCHLIGHT_PITCH_MIN;
                if (m_AimPitch > LFPG_SEARCHLIGHT_PITCH_MAX)
                    m_AimPitch = LFPG_SEARCHLIGHT_PITCH_MAX;

                m_AimDirty = true;
            }
        }

        // ---- Local prediction: immediate visual feedback ----
        m_TargetSl.LFPG_ApplyAimLocal(m_AimYaw, m_AimPitch);

        // ---- RPC throttle ----
        m_RpcAccum = m_RpcAccum + timeslice * 1000.0;
        if (m_AimDirty && m_RpcAccum >= LFPG_SEARCHLIGHT_RPC_THROTTLE_MS)
        {
            m_RpcAccum = 0.0;
            m_AimDirty = false;
            SendAimRPC(player);
        }
    }

    // ============================================
    // SendAimRPC -- throttled to server
    // ============================================
    protected void SendAimRPC(PlayerBase player)
    {
        if (!player)
            return;

        ScriptRPC rpc = new ScriptRPC();
        int subId = LFPG_RPC_SubId.SEARCHLIGHT_AIM;
        rpc.Write(subId);
        rpc.Write(m_TargetNetLow);
        rpc.Write(m_TargetNetHigh);
        rpc.Write(m_AimYaw);
        rpc.Write(m_AimPitch);
        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);
    }

    // ============================================
    // ForceCleanup -- shutdown / disconnect
    // ============================================
    protected void ForceCleanup()
    {
        m_Active        = false;
        m_TargetSl      = null;
        m_TargetNetLow  = 0;
        m_TargetNetHigh = 0;
        m_RpcAccum      = 0.0;
        m_AimDirty      = false;
    }
};
