#ifndef SERVER
// Client-only compilation boundary
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
//   Tick: MissionGameplay.OnUpdate → MaintenanceTick() + DrawFrame()
//         (DrawFrame inside HUD begin/end)
//   Cleanup: singleton destruction on mission end
//
// No allocations per frame. Pre-allocated screen coord arrays.
// =========================================================

class LFPG_LaserBeamRenderer
{
    protected static ref LFPG_LaserBeamRenderer s_Instance;

    // Registered laser detectors (populated by device EEInit/EEDelete)
	protected ref map<LFPG_LaserDetector, bool> m_Detectors;
    protected ref array<LFPG_LaserDetector> m_ActiveDetectors;
    protected vector m_CullClipA;
    protected vector m_CullClipB;
    // U6: acumulador del tick de mantenimiento, en segundos.
    protected float  m_CullAccS;

    // ---- Beam visual constants ----
    static const int   LFPG_LASER_BEAM_COLOR     = 0xC0FF0000;  // red, semi-transparent
    static const float LFPG_LASER_BEAM_WIDTH     = 3.0;         // base width (px), slightly thicker than cables
    static const float LFPG_LASER_BEAM_WIDTH_MIN = 1.5;         // min depth-scaled width
    static const float LFPG_LASER_BEAM_WIDTH_MAX = 5.0;         // max depth-scaled width
    static const int LFPG_LASER_CULL_TICK_MS = 250;

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
            s_Instance.CleanupInstance();
            s_Instance = null;
        }
    }

    void LFPG_LaserBeamRenderer()
    {
		m_Detectors = new map<LFPG_LaserDetector, bool>;
        m_ActiveDetectors = new array<LFPG_LaserDetector>;
        m_CullClipA = "0 0 0";
        m_CullClipB = "0 0 0";
        m_CullAccS  = 0.0;
    }

    void ~LFPG_LaserBeamRenderer()
    {
        CleanupInstance();
    }

    protected void CleanupInstance()
    {
        if (m_Detectors)
        {
            m_Detectors.Clear();
        }
        if (m_ActiveDetectors)
        {
            m_ActiveDetectors.Clear();
        }
    }

    // ---- Registration (called by LFPG_LaserDetector) ----
	void RegisterDetector(LFPG_LaserDetector detector)
	{
		if (!detector || m_Detectors.Contains(detector))
			return;

		m_Detectors.Set(detector, true);
		// Admit only the newcomer immediately; DrawFrame validates its live state.
		m_ActiveDetectors.Insert(detector);
	}

	void UnregisterDetector(LFPG_LaserDetector detector)
	{
		if (!detector)
			return;
		m_Detectors.Remove(detector);
		int activeIdx = m_ActiveDetectors.Find(detector);
		if (activeIdx >= 0)
		{
			m_ActiveDetectors.Remove(activeIdx);
		}
	}

    bool HasActiveBeams()
    {
        return (m_ActiveDetectors && m_ActiveDetectors.Count() > 0);
    }

    protected float DistanceSqToSegment(vector point, vector start, vector end)
    {
        float segX = end[0] - start[0];
        float segY = end[1] - start[1];
        float segZ = end[2] - start[2];
        float lenSq = segX * segX + segY * segY + segZ * segZ;
        float t = 0.0;
        if (lenSq > 0.0001)
        {
            float pointX = point[0] - start[0];
            float pointY = point[1] - start[1];
            float pointZ = point[2] - start[2];
            t = (pointX * segX + pointY * segY + pointZ * segZ) / lenSq;
            if (t < 0.0)
                t = 0.0;
            if (t > 1.0)
                t = 1.0;
        }

        float closestX = start[0] + segX * t;
        float closestY = start[1] + segY * t;
        float closestZ = start[2] + segZ * t;
        float dx = point[0] - closestX;
        float dy = point[1] - closestY;
        float dz = point[2] - closestZ;
        return dx * dx + dy * dy + dz * dz;
    }

    // U6: un solo tick de mantenimiento, gobernado por el frame.
    // Antes esto era una cadena CallLater repetida en CALL_CATEGORY_GUI que se
    // registraba en el constructor y habia que desregistrar a mano; una instancia
    // vieja que sobreviviera al Reset() dejaba el temporizador huerfano. El hub de
    // frame resuelve el singleton por Get() en cada llamada, asi que ese modo de
    // fallo desaparece. El acumulador se pone a cero al disparar en vez de restar
    // el periodo: tras un tiron largo se dispara una vez, no en rafaga.
    void MaintenanceTick(float timeslice)
    {
        m_CullAccS = m_CullAccS + timeslice;
        float cullPeriodS = LFPG_LASER_CULL_TICK_MS / 1000.0;
        if (m_CullAccS >= cullPeriodS)
        {
            m_CullAccS = 0.0;
            CullTick();
        }
    }

    protected void CullTick()
    {
        if (!m_Detectors || !m_ActiveDetectors)
            return;

		m_ActiveDetectors.Clear();
        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player)
            return;

        vector camPos = g_Game.GetCurrentCameraPosition();
        float cullDistSq = LFPG_CULL_DISTANCE_M * LFPG_CULL_DISTANCE_M;

        int i;
        for (i = 0; i < m_Detectors.Count(); i = i + 1)
        {
			LFPG_LaserDetector detector = m_Detectors.GetKey(i);
            if (!detector || !detector.LFPG_IsPowered() || detector.LFPG_GetBeamLength() < 0.05)
                continue;

            vector start = detector.LFPG_GetBeamStart();
            vector end = detector.LFPG_GetBeamEnd();
            float distSq = DistanceSqToSegment(camPos, start, end);
            if (distSq > cullDistSq)
                continue;

			// Distance/power candidates survive camera turns. Frustum clipping belongs
			// to DrawFrame so a newly visible beam is reconsidered on that frame.
            m_ActiveDetectors.Insert(detector);
        }
    }

    // ---- DrawFrame: called every frame from MissionGameplay.OnUpdate ----
    // Must be called between CableHUD.BeginFrame() and CableHUD.EndFrame().
    void DrawFrame()
    {
        int count = m_ActiveDetectors.Count();
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
        float distSq;
        vector scrA;
        vector scrB;
        float dist;
        float depthWidth;

        for (i = 0; i < count; i = i + 1)
        {
            det = m_ActiveDetectors[i];
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

            // Distance cull against the nearest point on the beam segment.
            distSq = DistanceSqToSegment(camPos, beamStart, beamEnd);
            if (distSq > cullDistSq)
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

            bool lasEndpointInsideA = (lasSwF > 0.0 && lasShF > 0.0 && scrA[0] >= 0.0 && scrA[0] <= lasSwF && scrA[1] >= 0.0 && scrA[1] <= lasShF);
            bool lasEndpointInsideB = (lasSwF > 0.0 && lasShF > 0.0 && scrB[0] >= 0.0 && scrB[0] <= lasSwF && scrB[1] >= 0.0 && scrB[1] <= lasShF);
            bool lasCrossOnly = (!lasEndpointInsideA && !lasEndpointInsideB);
            if (lasSwF > 0.0 && lasShF > 0.0)
            {
                bool lasSegmentVisible = LFPG_WorldUtil.ClipSegToScreen(scrA[0], scrA[1], scrB[0], scrB[1], 0.0, 0.0, lasSwF, lasShF, m_CullClipA, m_CullClipB);
                if (!lasSegmentVisible)
                    continue;
                scrA = m_CullClipA;
                scrB = m_CullClipB;
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
                float lasEdgeFade = 1.0;
                if (!lasCrossOnly)
                    lasEdgeFade = LFPG_WorldUtil.ComputeEdgeFade(scrA[0], scrA[1], scrB[0], scrB[1], lasSwF, lasShF, LFPG_EDGE_FADE_PX);
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
#endif
