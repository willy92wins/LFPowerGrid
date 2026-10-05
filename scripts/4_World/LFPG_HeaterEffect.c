#ifndef SERVER
// Client-only compilation boundary
// =========================================================
// LF_PowerGrid - Heater point light effect
//
// Client-side only. Created/destroyed by LFPG_Heater in
// LFPG_OnVarSync based on powered + switched-on state.
//
// Tuned for a floor appliance glowing from its coils:
//   - Radius 5m, brightness 3.0: the range of the ceiling and
//     wall lamp effects, so the warm pool reads around the unit
//   - No shadows (performance friendly)
//   - No flare, as the vanilla fireplace and stove lights
//   - Warm orange, matching the emissive coil rvmat
// =========================================================

class LFPG_HeaterEffect : PointLightBase
{
    void LFPG_HeaterEffect()
    {
        SetVisibleDuringDaylight(true);
        SetRadiusTo(5.0);
        SetBrightnessTo(3.0);
        SetCastShadow(false);
        SetFlareVisible(false);
        SetFadeOutTime(0.4);
        SetDiffuseColor(1.0, 0.55, 0.25);
        SetAmbientColor(1.0, 0.55, 0.25);
    }
};
#endif
