// =========================================================
// LF_PowerGrid - WallLamp point light effect
//
// Client-side only. Created/destroyed by LFPG_WallLamp
// in LFPG_OnVarSyncDevice based on m_PoweredNet state.
//
// Tuned for wall-mount placement:
//   - Radius 7m (slightly narrower than CeilingLight 8m)
//   - Brightness 2.8 (ceiling 3.0) — wall sconces usually lower intensity
//   - No shadows (performance friendly)
//   - Warm white matching emmisive rvmat {50,48,42}
// =========================================================

class LFPG_WallLampEffect : PointLightBase
{
    void LFPG_WallLampEffect()
    {
        SetVisibleDuringDaylight(true);
        SetRadiusTo(7.0);
        SetBrightnessTo(2.8);
        SetCastShadow(false);
        SetFadeOutTime(0.25);
        SetDiffuseColor(1.0, 0.95, 0.75);
        SetAmbientColor(1.0, 0.95, 0.75);
    }
};
