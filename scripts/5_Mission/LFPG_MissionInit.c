// =========================================================
// LF_PowerGrid - mission hooks (v0.7.13)
//
// ALL wiring input handled via scroll actions (LFPG_Actions.c).
// MissionGameplay handles:
//  - FullSync request on first tick with valid player (throttled)
//  - Committed cable rendering via Canvas 2D every frame
//  - Wiring preview HUD (only during active wiring session)
//  - Auto-cancel when cable reel is removed from hands
//
// v0.7.7: Canvas 2D for both preview AND committed cables.
//   Shape.LINE (debug API) does not render on retail client.
//   BeginFrame/DrawFrame/EndFrame runs every frame for cables.
//   Preview lines drawn on top within the same canvas pass.
//   LOD visual (3/2/1 passes), depth width, alpha fade.
// v0.7.8: Cable state system (IDLE/POWERED colors).
//   Joints at waypoints, endcaps at ports (LOD close only).
// v0.7.13: Telemetry tick after render for G1/G5 metrics collection.
// =========================================================

modded class MissionServer
{
    override void OnInit()
    {
        super.OnInit();
        Print(LFPG_LOG_PREFIX + "MissionServer OnInit (v" + LFPG_VERSION_STR + ")");
    }

    override void OnMissionFinish()
    {
        LFPG_NetworkManager.Get().FlushVanillaOnShutdown();
        super.OnMissionFinish();
    }
};

modded class MissionGameplay
{
    protected bool m_LFPG_WasActive = false;
    protected bool m_LFPG_SyncRequested = false;

    override void OnInit()
    {
        super.OnInit();
        Print(LFPG_LOG_PREFIX + "MissionGameplay OnInit (v" + LFPG_VERSION_STR + ")");

        Print(LFPG_LOG_PREFIX + "Resetting client singletons...");
        LFPG_CableHUD.Reset();
        LFPG_CableRenderer.Reset();
        LFPG_WiringClient.Reset();
        LFPG_DeviceInspector.Init();
        Print(LFPG_LOG_PREFIX + "Client singletons reset complete");
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);

        if (GetGame().IsDedicatedServer())
            return;

        // ---- FullSync: once when player position is valid ----
        if (!m_LFPG_SyncRequested)
        {
            PlayerBase syncPlayer = PlayerBase.Cast(GetGame().GetPlayer());
            if (syncPlayer)
            {
                vector syncPos = syncPlayer.GetPosition();
                if (syncPos[0] != 0.0 || syncPos[1] != 0.0 || syncPos[2] != 0.0)
                {
                    m_LFPG_SyncRequested = true;
                    LFPG_WiringClient.RequestFullSync();
                }
            }
        }

        // ---- Wiring session lifecycle ----
        LFPG_WiringClient wc = LFPG_WiringClient.Get();
        bool isActive = wc.IsActive();

        if (isActive && !m_LFPG_WasActive)
        {
            m_LFPG_WasActive = true;
            LFPG_ShowMsg("Wiring: scroll to place waypoint / finish / cancel.");
        }

        if (!isActive && m_LFPG_WasActive)
        {
            m_LFPG_WasActive = false;
        }

        // ---- Every frame: render committed cables + preview ----
        LFPG_CableHUD hud = LFPG_CableHUD.Get();
        hud.BeginFrame();

        // Committed cables (Canvas 2D + raycast occlusion)
        LFPG_CableRenderer renderer = LFPG_CableRenderer.Get();
        if (renderer)
        {
            renderer.DrawFrame();
        }

        // Preview lines (only during active wiring session)
        if (isActive)
        {
            LFPG_WiringClient.TickPreview();
        }

        hud.EndFrame();

        // v0.7.13 (Sprint 2.5): Telemetry tick — reads G1 (preview) + G5 (render)
        // counters filled by DrawFrame and TickPreview above, accumulates into
        // interval window, dumps summary to RPT every LFPG_TELEM_INTERVAL_MS.
        LFPG_Telemetry.Tick(GetGame().GetTime());

        // Sprint 5: Device inspector tick (runs when NOT wiring — has own guards)
        LFPG_DeviceInspector.Tick();

        // ---- Auto-cancel: cable reel removed from hands ----
        if (!isActive)
            return;

        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player)
            return;

        HumanInventory hinv = player.GetHumanInventory();
        if (!hinv)
            return;

        EntityAI inHands = hinv.GetEntityInHands();
        if (!inHands || !inHands.IsKindOf(LFPG_CABLE_REEL_TYPE))
        {
            LFPG_ShowMsg("Wiring cancelled.");
            wc.Cancel();
        }
    }

    override void OnMissionFinish()
    {
        LFPG_DeviceInspector.Cleanup();
        super.OnMissionFinish();
    }

    protected void LFPG_ShowMsg(string text)
    {
        PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
        if (!player)
            return;

        player.MessageStatus("[LFPG] " + text);
    }
};
