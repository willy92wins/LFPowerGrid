#ifndef SERVER
// Client-only compilation boundary
// =========================================================
// LF_PowerGrid — Custom Particle Registration
//
// Registers custom .ptc files so they get an int ID usable
// with Particle.PlayOnObject / ParticleManager.PlayOnObject.
//
// Each RegisterParticle call returns a unique int constant.
// Path = PBO-relative folder, name = filename without .ptc.
// =========================================================

modded class ParticleList
{
    // DIAG 1.30 (E4): register on first use, not at script load, so the
    // preload in DayZGame.OnInitialize never opens the two LFPG .ptc files.
    static int m_LFPG_SprinklerSpray;
    static int m_LFPG_FurnaceSmoke;

    static int LFPG_SprinklerSpray()
    {
        if (m_LFPG_SprinklerSpray == NONE)
        {
            m_LFPG_SprinklerSpray = RegisterParticle(
                "LFPowerGrid/data/particles/", "lfpg_sprinkler_spray");
        }
        return m_LFPG_SprinklerSpray;
    }

    static int LFPG_FurnaceSmoke()
    {
        if (m_LFPG_FurnaceSmoke == NONE)
        {
            m_LFPG_FurnaceSmoke = RegisterParticle(
                "LFPowerGrid/data/particles/", "lfpg_furnace_smoke");
        }
        return m_LFPG_FurnaceSmoke;
    }
};
#endif
