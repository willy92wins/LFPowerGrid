// =========================================================
// LF_PowerGrid - mission hooks (v0.9.9)
//
// v0.9.9:
//   - OnKeyPress override: delega Q/E/SPACE/ESC al viewport
//     cuando CCTV esta activo. MissionGameplay.OnKeyPress es
//     llamado por el engine INCLUSO con staticcamera activa
//     (confirmado via crash log de BBP en v0.9.8).
//     Cuando CCTV esta activo, NO llama super para evitar que
//     otros mods procesen teclas (BBP crashea sin player null-check).
//   - ShouldSkipInspector() con cooldown de 3 frames post-exit.
//   - Overlay widgets NO se crean en OnInit (cuelga el engine).
//     Se crean lazy en CameraViewport.EnterFromList.
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

    // =========================================================
    // OnKeyPress — llamado por el engine para TODA tecla.
    //
    // Funciona incluso con staticcamera activa (a diferencia de
    // Input.LocalPress/LocalValue que estan bloqueados por el
    // engine C++ cuando staticcamera captura el input pipeline).
    //
    // Cuando el viewport CCTV esta activo:
    //   - Delegamos la tecla a HandleKeyDown
    //   - NO llamamos super → evita que otros mods procesen la
    //     tecla (BBP crashea con null player en este callback)
    //
    // Cuando el viewport NO esta activo:
    //   - Llamamos super normalmente → chain de mods intacto
    // =========================================================
    override void OnKeyPress(int key)
    {
        LFPG_CameraViewport vp = LFPG_CameraViewport.Get();
        if (vp && vp.IsActive())
        {
            vp.HandleKeyDown(key);
            // NO super — suprimir procesamiento de otros mods durante CCTV
            return;
        }

        super.OnKeyPress(key);
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

        // v0.9.9: ShouldSkipInspector cubre viewport activo + cooldown post-exit.
        bool skipCameraOps = false;
        if (viewport)
        {
            skipCameraOps = viewport.ShouldSkipInspector();
        }

        // ---- Every frame: render committed cables + preview + CCTV overlay ----
        LFPG_CableHUD hud = LFPG_CableHUD.Get();
        hud.BeginFrame();

        LFPG_CableRenderer renderer = LFPG_CableRenderer.Get();
        if (renderer && !skipCameraOps)
        {
            renderer.DrawFrame();
        }

        if (isActive && !skipCameraOps)
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

        // Skip DeviceInspector durante viewport + cooldown post-exit
        if (!skipCameraOps)
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
