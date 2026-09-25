// =========================================================
// LF_PowerGrid - CeilingLight device (v4.0 Refactor)
//
// LFPG_CeilingLight_Kit:  Holdable (ceiling mount via HologramMod).
// LFPG_CeilingLight:      PASSTHROUGH, 1 IN + 1 OUT, 10 u/s self, cap 50.
//                       Light effect on/off via m_PoweredNet.
//
// v4.0: Migrated from Inventory_Base to LFPG_WireOwnerBase.
// Lifecycle, ports and SyncVars live in LFPG_LampDeviceBase.
// =========================================================

static const string LFPG_CEILING_RVMAT_OFF = "\LFPowerGrid\data\ceiling_light\lf_ceiling_light.rvmat";
static const string LFPG_CEILING_RVMAT_ON  = "\LFPowerGrid\data\ceiling_light\lf_ceiling_light_on.rvmat";

class LFPG_CeilingLight_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_CeilingLight";
    }

    override int LFPG_GetPlacementModes()
    {
        return 2;
    }
};

// ---------------------------------------------------------
// DEVICE - PASSTHROUGH : LFPG_LampDeviceBase
// ---------------------------------------------------------
class LFPG_CeilingLight : LFPG_LampDeviceBase
{
#ifndef SERVER
    override typename LFPG_GetLightEffectType() { return LFPG_CeilingLightEffect; }
    // Ceiling mount: without a "light" memory point, drop the light below the fixture.
    override float LFPG_GetLightFallbackDropY() { return 0.15; }
    override string LFPG_GetRvmatOn() { return LFPG_CEILING_RVMAT_ON; }
    override string LFPG_GetRvmatOff() { return LFPG_CEILING_RVMAT_OFF; }
#endif
};
