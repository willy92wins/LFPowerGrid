// =========================================================
// LF_PowerGrid - tiny client light for demo lamp
// =========================================================

class LFPG_LampLight : PointLightBase
{
    void LFPG_LampLight()
    {
        SetVisibleDuringDaylight(true);
        SetRadiusTo(6);
        SetBrightnessTo(2.5);
        SetCastShadow(false);
        SetFadeOutTime(0.2);
        SetDiffuseColor(1.0, 0.95, 0.85);
        SetAmbientColor(1.0, 0.95, 0.85);
    }
};
