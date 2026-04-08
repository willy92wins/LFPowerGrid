// =========================================================
// LF_PowerGrid - Laser Beam Renderer (v1.9.0)
//
// Client-side singleton that draws laser beams for all
// registered LFPG_LaserDetector devices each frame.
//
// Uses LFPG_CableHUD.DrawLineScreen (same Canvas2D pipeline
// as cable rendering) for consistency and zero extra overhead.
//
// Beam is drawn as a red line slightly thicker than cables
// (3px base vs 2px for cables), with depth-based scaling.
//
// Culled by distance (same LFPG_CULL_DISTANCE_M as cables).
// Culled when behind camera.
//
// Lifecycle:
//   Init: MissionGameplay.OnInit → Reset()
//   Tick: MissionGameplay.OnUpdate → DrawFrame() (inside HUD begin/end)
//   Cleanup: singleton destruction on mission end
//
// No allocations per frame. Pre-allocated screen coord arrays.
// =========================================================

class LFPG_LaserBeamRenderer
{
    protected static ref LFPG_LaserBeamRenderer s_Instance;

    // Registered laser detectors (populated by device EEInit/EEDelete)
    protected ref array<LFPG_LaserDetector> m_Detectors;

    // ---- Beam visual constants ----
    static const int   LFPG_LASER_BEAM_COLOR     = 0xC0FF0000;  // red, semi-transparent
    static const float LFPG_LASER_BEAM_WIDTH     = 3.0;         // base width (px), slightly thicker than cables
    static const float LFPG_LASER_BEAM_WIDTH_MIN = 1.5;         // min depth-scaled width
    static const float LFPG_LASER_BEAM_WIDTH_MAX = 5.0;         // max depth-scaled width

    static LFPG_LaserBeamRenderer Get()
    {
        if (!s_Instance)
        {
            s_Instance = new LFPG_LaserBeamRenderer();
        }
        return s_Instance;
    }

    static void Reset()
    {
        if (s_Instance)
        {
            delete s_Instance;
            s_Instance = null;
        }
    }

    void LFPG_LaserBeamRenderer()
    {
        m_Detectors = new array<LFPG_LaserDetector>;
    }

    void ~LFPG_LaserBeamRenderer()
    {
        if (m_Detectors)
        {
            m_Detectors.Clear();
        }
    }

    // ---- Registration (called by LFPG_LaserDetector) ----
    void RegisterDetector(LFPG_LaserDetector detector)
    {
        if (!detector)
            return;
        if (m_Detectors.Find(detector) < 0)
        {
            m_Detectors.Insert(detector);
        }
    }

    void UnregisterDetector(LFPG_LaserDetector detector)
    {
        if (!detector)
            return;
        int idx = m_Detectors.Find(detector);
        if (idx >= 0)
        {
            m_Detectors.Remove(idx);
        }
    }

    // ---- DrawFrame: called every frame from MissionGameplay.OnUpdate ----
    // Must be called between CableHUD.BeginFrame() and CableHUD.EndFrame().
    void DrawFrame()
    {
        int count = m_Detectors.Count();
        if (count == 0)
            return;

        LFPG_CableHUD hud = LFPG_CableHUD.Get();
        if (!hud || !hud.IsReady())
            return;

        vector camPos = g_Game.GetCurrentCameraPosition();
        vector camDir = g_Game.GetCurrentCameraDirection();
        float cullDistSq = LFPG_CULL_DISTANCE_M * LFPG_CULL_DISTANCE_M;

        // v0.8.x: Precompute screen dims once (not per-laser).
        float lasSwF = hud.GetScreenW();
        float lasShF = hud.GetScreenH();

        int i;
        LFPG_LaserDetector det;
        float beamLen;
        vector beamStart;
        vector beamEnd;
        float midX;
        float midY;
        float midZ;
        float dCamX;
        float dCamY;
        float dCamZ;
        float distSq;
        float dotMid;
        vector scrA;
        vector scrB;
        float dist;
        float depthWidth;

        for (i = 0; i < count; i = i + 1)
        {
            det = m_Detectors[i];
            if (!det)
                continue;

            // Skip if not powered or beam is negligible
            if (!det.LFPG_IsPowered())
                continue;

            beamLen = det.LFPG_GetBeamLength();
            if (beamLen < 0.05)
                continue;

            // Beam world coordinates
            beamStart = det.LFPG_GetBeamStart();
            beamEnd = det.LFPG_GetBeamEnd();

            // Distance cull (use beam midpoint)
            midX = (beamStart[0] + beamEnd[0]) * 0.5;
            midY = (beamStart[1] + beamEnd[1]) * 0.5;
            midZ = (beamStart[2] + beamEnd[2]) * 0.5;
            dCamX = midX - camPos[0];
            dCamY = midY - camPos[1];
            dCamZ = midZ - camPos[2];
            distSq = dCamX * dCamX + dCamY * dCamY + dCamZ * dCamZ;
            if (distSq > cullDistSq)
                continue;

            // Behind-camera check (dot product of beam midpoint direction with camera forward)
            dotMid = dCamX * camDir[0] + dCamY * camDir[1] + dCamZ * camDir[2];
            if (dotMid < -1.0)
                continue;

            // Project to screen — GetScreenPos returns vector(screenX, screenY, depth)
            scrA = g_Game.GetScreenPos(beamStart);
            scrB = g_Game.GetScreenPos(beamEnd);

            // Z check (behind camera plane)
            bool lasBehindA = (scrA[2] < LFPG_BEHIND_CAM_Z);
            bool lasBehindB = (scrB[2] < LFPG_BEHIND_CAM_Z);

            if (lasBehindA && lasBehindB)
                continue;

            // v0.8.x: Single-behind — 3D near-plane clip.
            // Laser beams are single segments (no catenary), so clipping
            // to the near plane is essential to avoid vanishing or artifacts.
            if (lasBehindA)
            {
                scrA = LFPG_WorldUtil.ClipBehindCamera(beamStart, beamEnd, camPos, camDir);
            }
            if (lasBehindB)
            {
                scrB = LFPG_WorldUtil.ClipBehindCamera(beamEnd, beamStart, camPos, camDir);
            }

            // Depth-based width scaling (same formula as cables)
            dist = Math.Sqrt(distSq);
            if (dist < 0.1)
            {
                dist = 0.1;
            }
            depthWidth = LFPG_LASER_BEAM_WIDTH * (LFPG_DEPTH_WIDTH_REF / dist);
            if (depthWidth < LFPG_LASER_BEAM_WIDTH_MIN)
            {
                depthWidth = LFPG_LASER_BEAM_WIDTH_MIN;
            }
            if (depthWidth > LFPG_LASER_BEAM_WIDTH_MAX)
            {
                depthWidth = LFPG_LASER_BEAM_WIDTH_MAX;
            }

            // Draw the beam line
            // v0.8.x: Edge fade — smooth alpha falloff near screen edges.
            int lasDraw = LFPG_LASER_BEAM_COLOR;
            if (lasSwF > 0.0 && lasShF > 0.0)
            {
                float lasEdgeFade = LFPG_WorldUtil.ComputeEdgeFade(scrA[0], scrA[1], scrB[0], scrB[1], lasSwF, lasShF, LFPG_EDGE_FADE_PX);
                if (lasEdgeFade < 0.01)
                    continue;
                if (lasEdgeFade < 0.99)
                {
                    int lasOrigAlpha = (LFPG_LASER_BEAM_COLOR >> 24) & 0xFF;
                    int lasNewAlpha = (int)(lasOrigAlpha * lasEdgeFade);
                    if (lasNewAlpha < 0)
                    {
                        lasNewAlpha = 0;
                    }
                    if (lasNewAlpha > 255)
                    {
                        lasNewAlpha = 255;
                    }
                    int lasRgb = LFPG_LASER_BEAM_COLOR & 0x00FFFFFF;
                    lasDraw = (lasNewAlpha << 24) | lasRgb;
                }
            }
            hud.DrawLineScreen(scrA[0], scrA[1], scrB[0], scrB[1], depthWidth, lasDraw);
        }
    }
};
