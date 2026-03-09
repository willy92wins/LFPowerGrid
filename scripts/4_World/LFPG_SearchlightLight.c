// =========================================================
// LF_PowerGrid - Searchlight light classes (v1.4.0)
//
// 4 light classes for the LF_Searchlight device:
//   LFPG_SearchlightBeamCore  — primary spot, 80m, narrow 8°, shadows ON
//   LFPG_SearchlightBeamSpill — secondary spot, 50m, wide 25°, shadows OFF
//   LFPG_SearchlightHalo      — lens glow, 3m radius point light
//   LFPG_SearchlightSplash    — ground splash, 5m radius point light
//
// All: client-side only, SetVisibleDuringDaylight(false),
//      SetLifetime(1000000), warm white color.
//
// DancingShadows OFF on BeamCore (Audit H1):
//   ScriptedLightBase does RemoveChild/AddChild/Update per EOnFrame
//   when amplitude > 0. Cost unacceptable with SetOrientation rotating parent.
//
// Flicker order: SetFlickerAmplitude BEFORE SetBrightnessTo
//   (ScriptedLightBase applies flicker as multiplier on brightness).
//
// Enforce Script: no ternaries, no ++/--, no foreach.
// =========================================================

// ---------------------------------------------------------
// BeamCore: primary directional spot
// ---------------------------------------------------------
class LFPG_SearchlightBeamCore : SpotLightBase
{
    void LFPG_SearchlightBeamCore()
    {
        SetVisibleDuringDaylight(false);
        SetRadiusTo(80.0);
        SetSpotLightAngle(14.0);
        SetCastShadow(true);

        // DancingShadows OFF (Audit H1)
        SetDancingShadowsMovementSpeed(0.0);
        SetDancingShadowsAmplitude(0.0);

        // Flicker: subtle realism. Amplitude BEFORE Brightness.
        SetFlickerAmplitude(0.03);
        SetFlickerSpeed(0.4);
        SetBrightnessTo(4.0);

        SetFadeOutTime(0.3);
        SetLifetime(1000000.0);

        // Warm white
        SetDiffuseColor(1.0, 0.97, 0.90);
        SetAmbientColor(1.0, 0.97, 0.90);
    }
};

// ---------------------------------------------------------
// BeamSpill: secondary wide cone (volumetric fill)
// ---------------------------------------------------------
class LFPG_SearchlightBeamSpill : SpotLightBase
{
    void LFPG_SearchlightBeamSpill()
    {
        SetVisibleDuringDaylight(false);
        SetRadiusTo(50.0);
        SetSpotLightAngle(35.0);
        SetCastShadow(false);

        // Minimal flicker
        SetFlickerAmplitude(0.001);
        SetFlickerSpeed(0.2);
        SetBrightnessTo(0.8);

        SetFadeOutTime(0.3);
        SetLifetime(1000000.0);

        SetDiffuseColor(1.0, 0.97, 0.90);
        SetAmbientColor(1.0, 0.97, 0.90);
    }
};

// ---------------------------------------------------------
// Halo: bloom glow around the lens
// ---------------------------------------------------------
class LFPG_SearchlightHalo : PointLightBase
{
    void LFPG_SearchlightHalo()
    {
        SetVisibleDuringDaylight(false);
        SetRadiusTo(3.0);
        SetCastShadow(false);
        SetBrightnessTo(8.0);
        SetFadeOutTime(0.2);
        SetLifetime(1000000.0);

        SetDiffuseColor(1.0, 0.98, 0.92);
        SetAmbientColor(1.0, 0.98, 0.92);
    }
};

// ---------------------------------------------------------
// Splash: ground impact light (repositioned via SyncVars)
// ---------------------------------------------------------
class LFPG_SearchlightSplash : PointLightBase
{
    void LFPG_SearchlightSplash()
    {
        SetVisibleDuringDaylight(false);
        SetRadiusTo(5.0);
        SetCastShadow(false);
        SetBrightnessTo(6.0);
        SetFadeOutTime(0.2);
        SetLifetime(1000000.0);

        SetDiffuseColor(1.0, 0.97, 0.90);
        SetAmbientColor(1.0, 0.97, 0.90);
    }
};
