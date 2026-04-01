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
    static const int LFPG_SPRINKLER_SPRAY = RegisterParticle(
        "LFPowerGrid/data/particles/", "lfpg_sprinkler_spray");

    static const int LFPG_FURNACE_SMOKE = RegisterParticle(
        "LFPowerGrid/data/particles/", "lfpg_furnace_smoke");
};
