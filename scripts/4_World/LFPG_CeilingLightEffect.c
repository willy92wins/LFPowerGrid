// =========================================================
// LF_PowerGrid - CeilingLight point light effect (v0.7.47)
//
// Client-side only. Created/destroyed by LFPG_CeilingLight
// in OnVariablesSynchronized based on m_PoweredNet state.
//
// Compared to LFPG_LampLight (radius=6, brightness=2.5):
//   - Wider radius (8m) — overhead light covers more area
//   - Brighter (3.0) — ceiling mount = less obstruction
//   - No shadows — performance friendly for multiple lights
//   - Warm white color matching emmisive rvmat {40,38,30}
// =========================================================

class LFPG_CeilingLightEffect : PointLightBase
{
    void LFPG_CeilingLightEffect()
    {
        SetVisibleDuringDaylight(true);
        SetRadiusTo(8.0);
        SetBrightnessTo(3.0);
        SetCastShadow(false);
        SetFadeOutTime(0.25);
        SetDiffuseColor(1.0, 0.95, 0.75);
        SetAmbientColor(1.0, 0.95, 0.75);
    }
};
