// Server action contract for World callers. Mission supplies the implementation.
// The base callbacks are inert when the mission factory is unavailable.
class LFPG_ServerActions
{
    protected static ref LFPG_ServerActions s_Instance;
    // Inert stand-in handed out while no mission can build the real implementation.
    // Deliberately NOT stored in s_Instance: caching it there would pin every
    // later Get() to the dead facade for the rest of the session, including
    // after a real mission appears. MissionMainMenu reaches MissionBaseWorld
    // without the factory override, so that path is reachable from the menu.
    protected static ref LFPG_ServerActions s_Fallback;

    static LFPG_ServerActions Get()
    {
        if (s_Instance)
            return s_Instance;
        if (g_Game)
        {
            MissionBaseWorld mw = MissionBaseWorld.Cast(g_Game.GetMission());
            if (mw)
                s_Instance = mw.LFPG_CreateServerActions();
        }
        if (s_Instance)
            return s_Instance;
        if (!s_Fallback)
        {
            // Log once while later calls continue to retry the mission factory.
            LFPG_Util.Error("[LFPG_ServerActions] Mission factory unavailable - inert actions until mission factory is available");
            s_Fallback = new LFPG_ServerActions();
        }
        return s_Fallback;
    }

    // Solo los metodos del calefactor. El resto de la fachada llega con el
    // carril de recorte de huella, que todavia no esta en main.
    void Heater_EEInit(LFPG_Heater dev) { }
    void Heater_LFPG_SetPowered(LFPG_Heater dev, bool powered) { }
    void Heater_LFPG_SetHeatActive(LFPG_Heater dev, bool active) { }
    void Heater_LFPG_OnWiresCut(LFPG_Heater dev) { }
    void Heater_LFPG_OnKilled(LFPG_Heater dev) { }
    void Heater_LFPG_OnDeleted(LFPG_Heater dev) { }
    void Heater_LFPG_ToggleHeater(LFPG_Heater dev) { }
    void ToggleHeater_OnExecuteServer(ActionData action_data) { }
};
