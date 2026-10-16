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
    // DIAG 1.30 (V2): vanilla particle names, no LFPG .ptc registered.
    static const int LFPG_SPRINKLER_SPRAY = RegisterParticle("smoke_small_generator_01");

    static const int LFPG_FURNACE_SMOKE = RegisterParticle("smoke_small_generator_01");
};
#endif
