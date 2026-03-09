// =========================================================
// LF_PowerGrid - mission hooks (v1.0.0)
//
// v1.0.0: Clean rewrite.
//   - OnKeyPress override: delega SPACE/Q/E al viewport.
//     NO llama super cuando CCTV activo (evita crash de BBP).
//   - super.OnUpdate se llama SIEMPRE — sin skip, sin cooldown.
//     Camera de script (new Camera) no corrompe estado del engine.
//   - ShouldSkipInspector solo cubre viewport activo.
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
    protected bool m_LFPG_WasActive      = false;
    protected bool m_LFPG_SyncRequested   = false;
    protected bool m_LFPG_WidgetsCreated  = false;

    // COT pattern: ResetGUI is called by vanilla OnSelectPlayer.
    // During SelectPlayer(sender, NULL) this crashes the client.
    // Flag m_LFPG_SkipResetGUI is inherited from modded MissionBaseWorld (4_World).
    // LFPG_SetSkipResetGUI() setter also inherited.
    override void ResetGUI()
    {
        if (m_LFPG_SkipResetGUI)
        {
            m_LFPG_SkipResetGUI = false;
            Print("[LF_PowerGrid] ResetGUI skipped (CCTV spectator transition)");
            return;
        }
        super.ResetGUI();
    }

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
        LFPG_SorterUI.Cleanup();

        Print(LFPG_LOG_PREFIX + "Client singletons reset complete");
    }

    // =========================================================
    // OnKeyPress — funciona incluso con Camera activa.
    // Cuando CCTV activo: consume la tecla, NO llama super.
    // =========================================================
    override void OnKeyPress(int key)
    {
        LFPG_CameraViewport vp = LFPG_CameraViewport.Get();
        if (vp && vp.IsActive())
        {
            vp.HandleKeyDown(key);
            return;
        }

        super.OnKeyPress(key);
    }

    // =========================================================
    // OnKeyRelease — clears WASD held state for CCTV pan.
    // =========================================================
    override void OnKeyRelease(int key)
    {
        LFPG_CameraViewport vp = LFPG_CameraViewport.Get();
        if (vp && vp.IsActive())
        {
            vp.HandleKeyUp(key);
        }

        super.OnKeyRelease(key);
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

        // ---- CCTV overlay widgets: crear en contexto OnUpdate (seguro) ----
        // CreateWidgets() cuelga cuando se llama desde contexto RPC.
        // Creamos los widgets aquí (hidden) y solo Show(true/false) desde RPC.
        if (!m_LFPG_WidgetsCreated && m_LFPG_SyncRequested)
        {
            LFPG_CameraViewport vpInit = LFPG_CameraViewport.Get();
            if (vpInit)
            {
                vpInit.InitWidgets();
                m_LFPG_WidgetsCreated = true;
            }
        }

        // ---- SorterUI deferred widget creation (same pattern as CCTV) ----
        // CreateWidgets from RPC context produces corrupt widget trees.
        // DoOpen stores params + sets pending flag; we finish here.
        if (LFPG_SorterUI.NeedsDeferredCreate())
        {
            LFPG_SorterUI.FinishDeferredCreate();
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

        // ---- Render cables + preview + CCTV overlay ----
        bool skipCameraOps = false;
        if (viewport)
        {
            skipCameraOps = viewport.ShouldSkipInspector();
        }

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

        // DeviceInspector — skip durante CCTV activo
        if (!skipCameraOps)
        {
            LFPG_DeviceInspector.Tick();
        }
        else
        {
            LFPG_DeviceInspector.ForceHide();
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
        LFPG_SorterUI.Cleanup();

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
