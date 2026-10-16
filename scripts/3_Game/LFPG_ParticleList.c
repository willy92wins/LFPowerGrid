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
    static const int LFPG_SPRINKLER_SPRAY = LFPG_RegisterKeepCase(
        "LFPowerGrid/data/particles/", "lfpg_sprinkler_spray");

    static const int LFPG_FURNACE_SMOKE = LFPG_RegisterKeepCase(
        "LFPowerGrid/data/particles/", "lfpg_furnace_smoke");

    // DIAG 1.30 (E3): RegisterParticle lowercases the path since 1.30; store
    // it as written, like 1.29 did, to see whether the case is what hangs.
    static int LFPG_RegisterKeepCase(string rootPath, string fileName)
    {
        int id = RegisterParticle(rootPath, fileName);
        m_ParticlePaths.Set(id, rootPath + fileName);
        return id;
    }
};
#endif
