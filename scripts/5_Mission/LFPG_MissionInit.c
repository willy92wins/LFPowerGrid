// =========================================================
// LF_PowerGrid - mission hooks (v0.9.7)
//
// v0.9.7:
//   - DeviceInspector skip en el frame donde viewport hace Exit().
//     Tras Camera.SetActive(false) + ObjectDelete, GetCurrentCameraPosition()
//     puede devolver basura o crashear en el mismo frame.
//     DidExitThisFrame() evita raycasts con camara en transicion.
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
    protected bool m_LFPG_WasActive    = false;
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

        LFPG_CameraViewport.Reset();

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

        // ---- Tick del viewport CCTV ----
        LFPG_CameraViewport viewport = LFPG_CameraViewport.Get();
        if (viewport)
        {
            viewport.Tick(timeslice);
        }

        // v0.9.7: Detectar si Exit() ocurrio en este frame.
        // Tras Camera.SetActive(false) + ObjectDelete, el engine necesita
        // un frame para restaurar la camara del jugador.
        // GetCurrentCameraPosition() devuelve basura -> crash en DeviceInspector.
        bool viewportExited = false;
        if (viewport)
        {
            viewportExited = viewport.DidExitThisFrame();
        }

        // ---- Every frame: render committed cables + preview + CCTV overlay ----
        LFPG_CableHUD hud = LFPG_CableHUD.Get();
        hud.BeginFrame();

        bool viewportActive = false;
        if (viewport)
        {
            viewportActive = viewport.IsActive();
        }

        LFPG_CableRenderer renderer = LFPG_CableRenderer.Get();
        if (renderer && !viewportActive)
        {
            renderer.DrawFrame();
        }

        if (isActive && !viewportActive)
        {
            LFPG_WiringClient.TickPreview();
        }

        if (viewport)
        {
            viewport.DrawOverlay(hud);
        }

        hud.EndFrame();

        // Telemetry tick
        LFPG_Telemetry.Tick(GetGame().GetTime());

        // v0.9.7: Skip DeviceInspector si:
        //   - viewport CCTV esta activo (raycasts apuntarian a staticcamera)
        //   - viewport acaba de hacer Exit() este frame (camara en transicion)
        if (!viewportActive && !viewportExited)
        {
            LFPG_DeviceInspector.Tick();
        }

        // ---- Auto-cancel wiring: cable reel removed from hands ----
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

        LFPG_CameraViewport.Reset();

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
