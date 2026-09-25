// =========================================================
// LF_PowerGrid - WallLamp device
//
// LFPG_WallLamp_Kit:  Holdable. Wall + floor placement (mode 1).
// LFPG_WallLamp:      PASSTHROUGH, 1 IN + 1 OUT, 10 u/s self, cap 50.
//                     Light effect on/off via m_PoweredNet.
//
// Same electrical contract as LFPG_CeilingLight, wall-mount placement.
// Lifecycle, ports and SyncVars live in LFPG_LampDeviceBase.
// =========================================================

static const string LFPG_WALLLAMP_RVMAT_OFF = "\LFPowerGrid\data\wall_lamp\lf_wall_lamp.rvmat";
static const string LFPG_WALLLAMP_RVMAT_ON  = "\LFPowerGrid\data\wall_lamp\lf_wall_lamp_on.rvmat";

class LFPG_WallLamp_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_WallLamp";
    }

    override int LFPG_GetPlacementModes()
    {
        // 1 = floor + wall (no ceiling). Wall is the primary mount.
        return 1;
    }
};

// ---------------------------------------------------------
// DEVICE - PASSTHROUGH : LFPG_LampDeviceBase
// ---------------------------------------------------------
class LFPG_WallLamp : LFPG_LampDeviceBase
{
#ifndef SERVER
    // No fallback Y drop (base default 0.0): the light sits at the fixture.
    override typename LFPG_GetLightEffectType() { return LFPG_WallLampEffect; }
    override string LFPG_GetRvmatOn() { return LFPG_WALLLAMP_RVMAT_ON; }
    override string LFPG_GetRvmatOff() { return LFPG_WALLLAMP_RVMAT_OFF; }
#endif
};
