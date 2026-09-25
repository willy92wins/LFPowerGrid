#ifndef SERVER
// Client-only compilation boundary
// =========================================================
// LF_PowerGrid - Heater point light effect
//
// Client-side only. Created/destroyed by LFPG_Heater in
// LFPG_OnVarSync based on powered + switched-on state.
//
// Tuned for a floor appliance glowing from its coils:
//   - Radius 3m (a heater lights its corner, not the room)
//   - Brightness 1.4 (well under WallLamp 2.8)
//   - No shadows (performance friendly)
//   - Warm orange, matching the emissive coil rvmat
// =========================================================

class LFPG_HeaterEffect : PointLightBase
{
    void LFPG_HeaterEffect()
    {
        SetVisibleDuringDaylight(true);
        SetRadiusTo(3.0);
        SetBrightnessTo(1.4);
        SetCastShadow(false);
        SetFadeOutTime(0.4);
        SetDiffuseColor(1.0, 0.55, 0.25);
        SetAmbientColor(1.0, 0.55, 0.25);
    }
};
#endif
