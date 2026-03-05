// =========================================================
// LF_PowerGrid - mission hooks (v0.9.0 - Etapa 3)
//
// Cambios respecto a v0.8.x:
//   - LFPG_CameraViewport.Reset() en OnInit (clean slate reconexion)
//   - LFPG_CameraViewport.Get().Tick(timeslice) en OnUpdate
//   - viewport.DrawOverlay(hud) dentro de la ventana BeginFrame/EndFrame
//   - LFPG_CameraViewport.Reset() en OnMissionFinish (limpieza)
//
// INTEGRACION: Este archivo es REEMPLAZO COMPLETO del existente.
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

        // Etapa 3: reset del viewport CCTV al conectar/reconectar.
        // Asegura que no quede un staticcamera local de una sesion anterior
        // si el jugador se desconecta mientras miraba una camara.
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

        // ---- Etapa 3: tick del viewport CCTV ----
        // Procesa input de salida (SPACE), animacion de scanlines y auto-timeout.
        // Debe ejecutarse ANTES del bloque BeginFrame/EndFrame para que
        // m_Active y m_ScanlineOffset esten actualizados antes del draw.
        // Get() devuelve null en servidor → sin coste en servidor.
        LFPG_CameraViewport viewport = LFPG_CameraViewport.Get();
        if (viewport)
        {
            viewport.Tick(timeslice);
        }

        // ---- Every frame: render committed cables + preview + CCTV overlay ----
        LFPG_CableHUD hud = LFPG_CableHUD.Get();
        hud.BeginFrame();

        // Committed cables (Canvas 2D + raycast occlusion).
        // SKIP cuando el viewport CCTV esta activo: el POV se ha movido al
        // staticcamera y GetCurrentCameraPosition() devolveria la posicion de la
        // camara de seguridad. DrawFrame proyectaria cables desde ese POV,
        // produciendo cables flotantes en la pantalla del feed CCTV.
        bool viewportActive = (viewport && viewport.IsActive());
        LFPG_CableRenderer renderer = LFPG_CableRenderer.Get();
        if (renderer && !viewportActive)
        {
            renderer.DrawFrame();
        }

        // Preview lines (solo durante sesion de cableado activa Y sin viewport CCTV).
        // TickPreview usa GetCurrentCameraPosition() internamente: con viewport activo
        // esa posicion es la del staticcamera, por lo que las lineas de preview
        // se proyectarian desde el POV de la camara de seguridad y aparecerian
        // en el feed CCTV, igual que el problema resuelto para DrawFrame arriba.
        if (isActive && !viewportActive)
        {
            LFPG_WiringClient.TickPreview();
        }

        // Etapa 3: overlay CCTV — scanlines + vignette + indicador REC.
        // DrawOverlay es no-op si viewport.IsActive() == false, sin coste.
        if (viewport)
        {
            viewport.DrawOverlay(hud);
        }

        hud.EndFrame();

        // Telemetry tick
        LFPG_Telemetry.Tick(GetGame().GetTime());

        // Device inspector tick.
        // Saltarlo cuando el viewport CCTV esta activo: el inspector hace raycasts
        // desde GetCurrentCameraPosition(), que apuntaria a la camara de seguridad,
        // pudiendo mostrar el panel flotante dentro del feed CCTV.
        if (!viewportActive)
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

        // Etapa 3: salir del viewport CCTV y liberar el staticcamera local
        // antes de que el motor destruya la escena.
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
