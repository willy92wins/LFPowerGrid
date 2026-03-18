// =========================================================
// LF_PowerGrid - Hologram override v2.0
//
// COMPLETE REWRITE: Camera-based placement for ALL LFPG kits.
//
// v2.0 Changes:
//   - ALL LFPG kits now use camera raycast for positioning
//     (including floor mode). Vanilla heading-based distance
//     caused hologram to appear far from where the player looks.
//   - Position smoothing (lerp) eliminates jitter.
//   - Orientation smoothing prevents 90-degree yaw jumps when
//     raycast hits edge geometry between wall components.
//   - Hysteresis on floor/wall threshold prevents mode flicker
//     at the transition boundary.
//   - Scroll-wheel rotation (m_Rotation) supported in ALL modes.
//   - Different-model kits (Solar, WaterPump) also use camera
//     raycast for consistent placement feel.
//   - Ceiling mode preserved for CeilingLight_Kit.
//
// Surface classification (with hysteresis):
//   FLOOR:   normalY > +threshold  (normal points UP)
//   CEILING: normalY < -threshold  (normal points DOWN)
//   WALL:    everything between
//
// Non-LFPG-kit items always use vanilla (super) behavior.
// =========================================================

modded class Hologram
{
    // ---- Tuning constants ----
    static const float LFPG_HOLO_MAX_RANGE         = 4.0;   // max ray distance (m)
    static const float LFPG_HOLO_SURFACE_OFFSET     = 0.03;  // offset from wall/ceiling surface (m)
    static const float LFPG_HOLO_FLOOR_GROUND_SNAP  = 0.05;  // Y offset above ground for floor mode (m)

    // Hysteresis thresholds for surface classification.
    // ENTER = threshold to ENTER wall mode from floor/ceiling.
    // EXIT  = threshold to EXIT wall mode back to floor/ceiling.
    // EXIT > ENTER prevents flickering at the boundary.
    static const float LFPG_HOLO_ENTER_WALL_THRESHOLD = 0.45;
    static const float LFPG_HOLO_EXIT_WALL_THRESHOLD  = 0.55;

    // Smoothing factors (higher = snappier, lower = smoother).
    // Applied as: lerp(previous, target, FACTOR * timeslice)
    static const float LFPG_HOLO_POS_SMOOTH  = 20.0;
    static const float LFPG_HOLO_ORI_SMOOTH  = 12.0;

    // Vertical ray range for ground-snap
    static const float LFPG_HOLO_GROUND_RAY_UP   = 2.0;
    static const float LFPG_HOLO_GROUND_RAY_DOWN  = 4.0;

    // Minimum horizontal length of hit normal to trust its direction.
    // Below this, the normal is nearly vertical (edge geometry hit)
    // and Atan2 becomes unstable => fall back to camera direction.
    static const float LFPG_HOLO_MIN_HORIZ_NORMAL = 0.2;

    // ---- State ----
    protected bool m_LFPG_IsWallMode;
    protected bool m_LFPG_IsLFPGKit;

    // Smoothing state
    protected vector m_LFPG_SmoothedPos;
    protected vector m_LFPG_SmoothedOri;
    protected bool   m_LFPG_HasPreviousState;

    // Hysteresis: tracks whether we were in wall mode last frame
    protected bool m_LFPG_WasWallMode;

    // ============================================
    // Different-model kit hologram overrides
    // (Solar Panel, Water Pump: box kit -> deployed model)
    // ============================================

    override string ProjectionBasedOnParent()
    {
        if (m_Parent)
        {
            LF_SolarPanel_Kit solarKit = LF_SolarPanel_Kit.Cast(m_Parent);
            if (solarKit)
            {
                return solarKit.GetDeployedClassname();
            }

            LF_WaterPump_Kit pumpKit = LF_WaterPump_Kit.Cast(m_Parent);
            if (pumpKit)
            {
                return pumpKit.GetDeployedClassname();
            }

            LF_Furnace_Kit furnaceKit = LF_Furnace_Kit.Cast(m_Parent);
            if (furnaceKit)
            {
                return furnaceKit.GetDeployedClassname();
            }
        }

        return super.ProjectionBasedOnParent();
    }

    override string GetProjectionName(ItemBase item)
    {
        if (m_Parent)
        {
            LF_SolarPanel_Kit solarKit = LF_SolarPanel_Kit.Cast(m_Parent);
            if (solarKit)
            {
                return solarKit.GetDeployedClassname();
            }

            LF_WaterPump_Kit pumpKit = LF_WaterPump_Kit.Cast(m_Parent);
            if (pumpKit)
            {
                return pumpKit.GetDeployedClassname();
            }

            LF_Furnace_Kit furnaceKit = LF_Furnace_Kit.Cast(m_Parent);
            if (furnaceKit)
            {
                return furnaceKit.GetDeployedClassname();
            }
        }

        return super.GetProjectionName(item);
    }

    // CRITICAL: Prevents ghost entity creation for different-model kits.
    // Without this, ProjectionBasedOnParent causes engine to spawn a
    // REAL entity (runs EEInit, registers, replicates).
    override EntityAI PlaceEntity(EntityAI entity_for_placing)
    {
        if (m_Parent)
        {
            LF_SolarPanel_Kit solarKit = LF_SolarPanel_Kit.Cast(m_Parent);
            if (solarKit)
            {
                return entity_for_placing;
            }

            LF_WaterPump_Kit pumpKit = LF_WaterPump_Kit.Cast(m_Parent);
            if (pumpKit)
            {
                return entity_for_placing;
            }

            LF_Furnace_Kit furnaceKit = LF_Furnace_Kit.Cast(m_Parent);
            if (furnaceKit)
            {
                return entity_for_placing;
            }
        }

        return super.PlaceEntity(entity_for_placing);
    }

    // Position offset for different-model kits.
    // NOTE: In v2.0, floor positioning is done by our camera raycast
    // so this only applies when vanilla code paths call SetProjectionPosition
    // (e.g., during hologram creation before first UpdateHologram tick).
    override void SetProjectionPosition(vector position)
    {
        if (m_Parent)
        {
            LF_SolarPanel_Kit solarKit = LF_SolarPanel_Kit.Cast(m_Parent);
            if (solarKit)
            {
                vector solarOffset = solarKit.GetDeployPositionOffset();
                vector solarFinal = position + solarOffset;

                if (m_Projection)
                {
                    m_Projection.SetPosition(solarFinal);
                }
                return;
            }

            LF_WaterPump_Kit pumpKit = LF_WaterPump_Kit.Cast(m_Parent);
            if (pumpKit)
            {
                vector pumpOffset = pumpKit.GetDeployPositionOffset();
                vector pumpFinal = position + pumpOffset;

                if (m_Projection)
                {
                    m_Projection.SetPosition(pumpFinal);
                }
                return;
            }

            LF_Furnace_Kit furnaceKit = LF_Furnace_Kit.Cast(m_Parent);
            if (furnaceKit)
            {
                vector furnaceOffset = furnaceKit.GetDeployPositionOffset();
                vector furnaceFinal = position + furnaceOffset;

                if (m_Projection)
                {
                    m_Projection.SetPosition(furnaceFinal);
                }
                return;
            }
        }

        super.SetProjectionPosition(position);
    }

    // Orientation offset for different-model kits
    override vector GetDefaultOrientation()
    {
        if (m_Parent)
        {
            LF_SolarPanel_Kit solarKit = LF_SolarPanel_Kit.Cast(m_Parent);
            if (solarKit)
            {
                vector solarBase = super.GetDefaultOrientation();
                vector solarOriOff = solarKit.GetDeployOrientationOffset();
                vector solarResult = solarBase + solarOriOff;
                return solarResult;
            }

            LF_WaterPump_Kit pumpKit = LF_WaterPump_Kit.Cast(m_Parent);
            if (pumpKit)
            {
                vector pumpBase = super.GetDefaultOrientation();
                vector pumpOriOff = pumpKit.GetDeployOrientationOffset();
                vector pumpResult = pumpBase + pumpOriOff;
                return pumpResult;
            }

            LF_Furnace_Kit furnaceKit = LF_Furnace_Kit.Cast(m_Parent);
            if (furnaceKit)
            {
                vector furnaceBase = super.GetDefaultOrientation();
                vector furnaceOriOff = furnaceKit.GetDeployOrientationOffset();
                vector furnaceResult = furnaceBase + furnaceOriOff;
                return furnaceResult;
            }
        }

        return super.GetDefaultOrientation();
    }

    // ============================================
    // Kit projection detection helper
    // ============================================

    protected bool LFPG_IsLFPGKitProjection()
    {
        EntityAI proj = GetProjectionEntity();
        if (!proj)
            return false;

        // Same-model kits: projection IS the kit type
        if (proj.IsKindOf("LF_Splitter_Kit"))
            return true;
        if (proj.IsKindOf("LF_CeilingLight_Kit"))
            return true;
        if (proj.IsKindOf("LF_Combiner_Kit"))
            return true;
        if (proj.IsKindOf("LF_Camera_Kit"))
            return true;
        if (proj.IsKindOf("LF_Monitor_Kit"))
            return true;
        if (proj.IsKindOf("LFPG_PushButton_Kit"))
            return true;
        if (proj.IsKindOf("LF_Searchlight_Kit"))
            return true;
        if (proj.IsKindOf("LFPG_SwitchV2_Kit"))
            return true;
        if (proj.IsKindOf("LF_SwitchRemote_Kit"))
            return true;
        if (proj.IsKindOf("LFPG_MotionSensor_Kit"))
            return true;
        if (proj.IsKindOf("LFPG_PressurePad_Kit"))
            return true;
        if (proj.IsKindOf("LFPG_LaserDetector_Kit"))
            return true;
        // Logic gate kits (AND, OR, XOR, MemoryCell all inherit LFPG_LogicGate_Kit)
        if (proj.IsKindOf("LFPG_LogicGate_Kit"))
            return true;
        if (proj.IsKindOf("LFPG_ElectronicCounter_Kit"))
            return true;
        if (proj.IsKindOf("LF_Sorter_Kit"))
            return true;
        if (proj.IsKindOf("LF_BatteryMedium_Kit"))
            return true;
        if (proj.IsKindOf("LF_DoorController_Kit"))
            return true;
        if (proj.IsKindOf("LF_Intercom_Kit"))
            return true;
        if (m_Parent && m_Parent.IsKindOf("LF_SolarPanel_Kit"))
            return true;
        if (m_Parent && m_Parent.IsKindOf("LF_WaterPump_Kit"))
            return true;
        if (m_Parent && m_Parent.IsKindOf("LF_Furnace_Kit"))
            return true;

        return false;
    }

    // ---- Helper: Check which placement modes a kit supports ----
    // Returns: 0 = floor only, 1 = floor + wall, 2 = floor + wall + ceiling
    protected int LFPG_GetKitPlacementModes(EntityAI projection)
    {
        if (!projection)
            return 0;

        // CeilingLight supports all three modes
        if (projection.IsKindOf("LF_CeilingLight_Kit"))
            return 2;

        // Wall-capable same-model kits
        if (projection.IsKindOf("LF_Splitter_Kit"))
            return 1;
        if (projection.IsKindOf("LF_Combiner_Kit"))
            return 1;
        if (projection.IsKindOf("LF_Camera_Kit"))
            return 1;
        if (projection.IsKindOf("LF_Monitor_Kit"))
            return 1;
        if (projection.IsKindOf("LFPG_PushButton_Kit"))
            return 1;
        if (projection.IsKindOf("LFPG_SwitchV2_Kit"))
            return 1;
        if (projection.IsKindOf("LF_SwitchRemote_Kit"))
            return 1;
        if (projection.IsKindOf("LFPG_MotionSensor_Kit"))
            return 2;
        if (projection.IsKindOf("LFPG_LaserDetector_Kit"))
            return 1;
        if (projection.IsKindOf("LFPG_ElectronicCounter_Kit"))
            return 1;
        if (projection.IsKindOf("LF_DoorController_Kit"))
            return 1;
        if (projection.IsKindOf("LF_Intercom_Kit"))
            return 1;
        // Logic gates (AND, OR, XOR, MemoryCell): floor + wall
        if (projection.IsKindOf("LFPG_LogicGate_Kit"))
            return 1;

        // Different-model kits and everything else: floor only
        return 0;
    }

    // ---- Helper: detect if parent is a different-model kit ----
    protected bool LFPG_IsDifferentModelKit()
    {
        if (!m_Parent)
            return false;
        if (m_Parent.IsKindOf("LF_SolarPanel_Kit"))
            return true;
        if (m_Parent.IsKindOf("LF_WaterPump_Kit"))
            return true;
        if (m_Parent.IsKindOf("LF_Furnace_Kit"))
            return true;
        return false;
    }

    // ---- Helper: per-kit pitch offset (degrees) ----
    // LaserDetector deploys rotated 90° in pitch (ori[1]).
    protected float LFPG_GetKitPitchOffset(EntityAI projection)
    {
        if (!projection)
            return 0.0;
        if (projection.IsKindOf("LFPG_LaserDetector_Kit"))
            return 90.0;
        return 0.0;
    }

    // ---- Helper: per-kit WALL pitch offset (degrees) ----
    // Applied only in wall mode. MotionSensor is ceiling-oriented
    // (sensor dome at -Y), so on a wall we pitch -90° to rotate
    // dome outward (toward room) and housing top against wall.
    // DayZ pitch convention: +pitch rotates +Y toward +Z (outward),
    // which puts dome(-Y) INTO wall. -90° is the correct sign:
    // +Y→-Z (top against wall), -Y→+Z (dome outward).
    protected float LFPG_GetKitWallPitchOffset(EntityAI projection)
    {
        if (!projection)
            return 0.0;
        if (projection.IsKindOf("LFPG_MotionSensor_Kit"))
            return -90.0;
        // Logic gates: lid/symbol face is +Y (top). +90° pitches
        // +Y outward from wall so symbol faces the player.
        if (projection.IsKindOf("LFPG_LogicGate_Kit"))
            return 90.0;
        return 0.0;
    }

    // ---- Helper: per-kit WALL surface offset (metres) ----
    // Distance from wall surface to model origin along the normal.
    // Default LFPG_HOLO_SURFACE_OFFSET (0.03m) works for small/flat kits.
    // Larger models need more offset to avoid embedding in the wall.
    // Value = backHalf depth of model in local -Z + small margin.
    protected float LFPG_GetKitWallSurfaceOffset(EntityAI projection)
    {
        if (!projection)
            return LFPG_HOLO_SURFACE_OFFSET;
        // Camera model: Z extends -0.057 to +0.057, backHalf=0.057
        // Offset 0.08 places back face 2.3cm from wall (clean gap).
        if (projection.IsKindOf("LF_Camera_Kit"))
            return 0.08;
        // Monitor model: Z extends -0.307, very deep
        if (projection.IsKindOf("LF_Monitor_Kit"))
            return 0.32;
        // Logic gates: origin at base (Y=0), model extends 0.17m.
        // 0.05m offset keeps back face off the wall surface.
        if (projection.IsKindOf("LFPG_LogicGate_Kit"))
            return 0.05;
        return LFPG_HOLO_SURFACE_OFFSET;
    }

    // ============================================
    // Main placement logic (UpdateHologram)
    // ============================================

    override void UpdateHologram(float timeslice)
    {
        EntityAI projection = GetProjectionEntity();
        m_LFPG_IsLFPGKit = false;
        m_LFPG_IsWallMode = false;

        if (!projection)
        {
            super.UpdateHologram(timeslice);
            return;
        }

        // Detect if this is ANY LFPG kit
        bool isLFPGKit = LFPG_IsLFPGKitProjection();
        if (!isLFPGKit)
        {
            super.UpdateHologram(timeslice);
            return;
        }

        // --- Vanilla safety checks (replicated from base UpdateHologram) ---
        // Without these, placement mode can get stuck or crash.
        if (!m_Parent)
        {
            m_Player.TogglePlacingLocal();
            return;
        }

        if (IsRestrictedFromAdvancedPlacing())
        {
            m_Player.TogglePlacingLocal();
            return;
        }

        if (!GetUpdatePosition())
            return;

        m_LFPG_IsLFPGKit = true;

        // Get placement capability for this kit
        int placementModes = LFPG_GetKitPlacementModes(projection);

        // --- Camera raycast ---
        PlayerBase player = m_Player;
        if (!player)
        {
            super.UpdateHologram(timeslice);
            return;
        }

        vector camPos = GetGame().GetCurrentCameraPosition();
        vector camDir = GetGame().GetCurrentCameraDirection();

        // FIX: Looking-at-sky guard. Vanilla constant LOOKING_TO_SKY = 0.75.
        // When looking nearly straight up, camera ray goes into sky and misses
        // everything, causing hologram to jump to max range. Use ground-snap
        // at player position instead.
        float lookUpThreshold = 0.75;
        if (camDir[1] > lookUpThreshold)
        {
            vector skyFallbackPos = LFPG_GroundSnap(player.GetPosition());
            vector skyFallbackOri = LFPG_CalcFloorOrientation();
            LFPG_ApplySmoothed(skyFallbackPos, skyFallbackOri, timeslice, projection);
            return;
        }
        vector rayEnd = camPos + (camDir * LFPG_HOLO_MAX_RANGE);

        vector hitPos;
        vector hitNormal;
        int contactComponent;

        // Raycast: exclude projection entity to avoid self-hit.
        float rayRadius = 0.0;
        set<Object> rayResults = null;
        Object rayWith = null;
        bool bSorted = false;
        bool bGroundOnly = false;

        bool hit = DayZPhysics.RaycastRV(camPos, rayEnd, hitPos, hitNormal, contactComponent, rayResults, rayWith, projection, bSorted, bGroundOnly, ObjIntersectFire, rayRadius);

        if (!hit)
        {
            // No surface hit - project to max range and ground-snap
            vector noHitPoint = camPos + (camDir * LFPG_HOLO_MAX_RANGE);
            vector noHitGroundPos = LFPG_GroundSnap(noHitPoint);
            vector noHitOri = LFPG_CalcFloorOrientation();

            LFPG_ApplySmoothed(noHitGroundPos, noHitOri, timeslice, projection);
            return;
        }

        // --- Surface classification with hysteresis ---
        float rawNormalY = hitNormal[1];

        // Choose threshold based on previous state (hysteresis)
        float floorThreshold = LFPG_HOLO_ENTER_WALL_THRESHOLD;
        float ceilThreshold = LFPG_HOLO_ENTER_WALL_THRESHOLD;
        if (m_LFPG_WasWallMode)
        {
            floorThreshold = LFPG_HOLO_EXIT_WALL_THRESHOLD;
            ceilThreshold = LFPG_HOLO_EXIT_WALL_THRESHOLD;
        }

        // ================================================================
        // FLOOR MODE: normalY > threshold (normal points UP)
        // ================================================================
        if (rawNormalY >= floorThreshold)
        {
            m_LFPG_WasWallMode = false;

            // Camera-based floor position.
            // When normal is nearly vertical (normalY > 0.9), the camera
            // ray hit a flat floor directly — hitPos Y is already accurate.
            // Skip the expensive ground-snap raycast in that case.
            vector floorPos;
            float flatFloorThreshold = 0.9;
            if (rawNormalY > flatFloorThreshold)
            {
                // Pure flat floor: use hitPos directly with small Y offset
                floorPos = Vector(hitPos[0], hitPos[1] + LFPG_HOLO_FLOOR_GROUND_SNAP, hitPos[2]);
            }
            else
            {
                // Angled floor (ramp/slope): ground-snap for precision
                floorPos = LFPG_GroundSnap(hitPos);
            }
            vector floorOri = LFPG_CalcFloorOrientation();

            // For different-model kits, apply position offset
            if (LFPG_IsDifferentModelKit())
            {
                floorPos = LFPG_ApplyDiffModelPosOffset(floorPos);
            }

            LFPG_ApplySmoothed(floorPos, floorOri, timeslice, projection);
            return;
        }

        // ================================================================
        // CEILING MODE: normalY < -threshold (normal points DOWN)
        // ================================================================
        float negCeilThreshold = -1.0 * ceilThreshold;
        if (rawNormalY <= negCeilThreshold)
        {
            // Only CeilingLight supports ceiling (placementModes == 2)
            if (placementModes < 2)
            {
                // Kit doesn't support ceiling: ground-snap below hit point
                m_LFPG_WasWallMode = false;
                vector ceilFallPos = LFPG_GroundSnap(hitPos);
                vector ceilFallOri = LFPG_CalcFloorOrientation();
                LFPG_ApplySmoothed(ceilFallPos, ceilFallOri, timeslice, projection);
                return;
            }

            m_LFPG_IsWallMode = true;
            m_LFPG_WasWallMode = true;

            // Position: surface + small offset along normal
            vector ceilPos = hitPos + (hitNormal * LFPG_HOLO_SURFACE_OFFSET);

            // Orientation: yaw from camera horizontal direction + scroll, pitch=180
            float scrollCeil = GetProjectionRotation()[0];
            float ceilYaw = Math.Atan2(camDir[0], camDir[2]) * Math.RAD2DEG;
            ceilYaw = ceilYaw + scrollCeil;
            float ceilPitch = 180.0;
            vector ceilOri = Vector(ceilYaw, ceilPitch, 0);

            LFPG_ApplySmoothed(ceilPos, ceilOri, timeslice, projection);

            // Double-set: forces physics engine to accept pitch=180
            projection.SetOrientation(projection.GetOrientation());
            return;
        }

        // ================================================================
        // WALL MODE: |normalY| < threshold (normal is ~horizontal)
        // ================================================================

        // If this kit doesn't support wall placement, ground-snap
        if (placementModes < 1)
        {
            m_LFPG_WasWallMode = false;
            vector wallFallPos = LFPG_GroundSnap(hitPos);
            vector wallFallOri = LFPG_CalcFloorOrientation();

            if (LFPG_IsDifferentModelKit())
            {
                wallFallPos = LFPG_ApplyDiffModelPosOffset(wallFallPos);
            }

            LFPG_ApplySmoothed(wallFallPos, wallFallOri, timeslice, projection);
            return;
        }

        m_LFPG_IsWallMode = true;
        m_LFPG_WasWallMode = true;

        // Position: hit point + per-kit offset along surface normal (away from wall)
        float wallSurfOff = LFPG_GetKitWallSurfaceOffset(projection);
        vector wallPos = hitPos + (hitNormal * wallSurfOff);

        // Orientation: model faces outward from wall.
        // Use HORIZONTAL component of the normal for stable yaw.
        // When normal is nearly vertical (edge hit), Atan2 is unstable
        // => fall back to camera direction.
        float normalHorizLenSq = hitNormal[0] * hitNormal[0] + hitNormal[2] * hitNormal[2];
        float normalHorizLen = Math.Sqrt(normalHorizLenSq);

        float wallYaw;
        if (normalHorizLen < LFPG_HOLO_MIN_HORIZ_NORMAL)
        {
            // Normal is nearly vertical (edge geometry) - use camera direction
            wallYaw = Math.Atan2(camDir[0], camDir[2]) * Math.RAD2DEG;
            // Flip 180: camera points INTO wall, model should face OUT
            wallYaw = wallYaw + 180.0;
        }
        else
        {
            wallYaw = Math.Atan2(hitNormal[0], hitNormal[2]) * Math.RAD2DEG;
        }

        // Apply scroll-wheel rotation
        float scrollWall = GetProjectionRotation()[0];
        wallYaw = wallYaw + scrollWall;

        // Per-kit wall pitch (e.g. MotionSensor -90° so dome faces out)
        float wallPitchOff = LFPG_GetKitWallPitchOffset(projection);
        vector wallOri = Vector(wallYaw, wallPitchOff, 0);

        LFPG_ApplySmoothed(wallPos, wallOri, timeslice, projection);
    }

    // ============================================
    // Helper: Calculate floor orientation
    // Uses DefaultOrientation + scroll-wheel rotation
    // ============================================
    protected vector LFPG_CalcFloorOrientation()
    {
        vector defOri = GetDefaultOrientation();
        vector scrollRot = GetProjectionRotation();
        vector result = defOri + scrollRot;

        // Normalize yaw to [-180, 180]
        if (result[0] > 180.0)
        {
            result[0] = result[0] - 360.0;
        }
        if (result[0] < -180.0)
        {
            result[0] = result[0] + 360.0;
        }

        return result;
    }

    // ============================================
    // Helper: Ground-snap a position
    // Casts a vertical ray down from given point.
    // ============================================
    protected vector LFPG_GroundSnap(vector pos)
    {
        vector rayFrom = Vector(pos[0], pos[1] + LFPG_HOLO_GROUND_RAY_UP, pos[2]);
        vector rayTo   = Vector(pos[0], pos[1] - LFPG_HOLO_GROUND_RAY_DOWN, pos[2]);

        vector groundHitPos;
        vector groundHitNormal;
        int groundComponent;
        set<Object> groundResults = null;
        Object groundWith = null;

        EntityAI proj = GetProjectionEntity();
        bool gSorted = false;
        bool gGroundOnly = false;
        float gRadius = 0.0;

        bool groundHit = DayZPhysics.RaycastRV(rayFrom, rayTo, groundHitPos, groundHitNormal, groundComponent, groundResults, groundWith, proj, gSorted, gGroundOnly, ObjIntersectFire, gRadius);

        if (groundHit)
        {
            vector snapped = Vector(groundHitPos[0], groundHitPos[1] + LFPG_HOLO_FLOOR_GROUND_SNAP, groundHitPos[2]);
            return snapped;
        }

        // Fallback: use engine surface Y
        float surfaceY = GetGame().SurfaceY(pos[0], pos[2]);
        vector fallback = Vector(pos[0], surfaceY + LFPG_HOLO_FLOOR_GROUND_SNAP, pos[2]);
        return fallback;
    }

    // ============================================
    // Helper: Apply position offset for different-model kits
    // ============================================
    protected vector LFPG_ApplyDiffModelPosOffset(vector pos)
    {
        if (!m_Parent)
            return pos;

        LF_SolarPanel_Kit solarKit = LF_SolarPanel_Kit.Cast(m_Parent);
        if (solarKit)
        {
            vector solarOff = solarKit.GetDeployPositionOffset();
            vector solarOut = pos + solarOff;
            return solarOut;
        }

        LF_WaterPump_Kit pumpKit = LF_WaterPump_Kit.Cast(m_Parent);
        if (pumpKit)
        {
            vector pumpOff = pumpKit.GetDeployPositionOffset();
            vector pumpOut = pos + pumpOff;
            return pumpOut;
        }

        LF_Furnace_Kit furnaceKit = LF_Furnace_Kit.Cast(m_Parent);
        if (furnaceKit)
        {
            vector furnaceOff = furnaceKit.GetDeployPositionOffset();
            vector furnaceOut = pos + furnaceOff;
            return furnaceOut;
        }

        return pos;
    }

    // ============================================
    // Helper: Apply position and orientation with smoothing
    // ============================================
    protected void LFPG_ApplySmoothed(vector targetPos, vector targetOri, float timeslice, EntityAI projection)
    {
        // Per-kit pitch offset (e.g. LaserDetector 90°) — applied BEFORE smoothing
        float kitPitch = LFPG_GetKitPitchOffset(projection);
        if (kitPitch != 0.0)
        {
            targetOri[1] = targetOri[1] + kitPitch;
        }

        vector finalPos;
        vector finalOri;

        if (m_LFPG_HasPreviousState && timeslice > 0.0)
        {
            // --- Smooth position ---
            float posAlpha = LFPG_HOLO_POS_SMOOTH * timeslice;
            if (posAlpha > 1.0)
            {
                posAlpha = 1.0;
            }
            float px = Math.Lerp(m_LFPG_SmoothedPos[0], targetPos[0], posAlpha);
            float py = Math.Lerp(m_LFPG_SmoothedPos[1], targetPos[1], posAlpha);
            float pz = Math.Lerp(m_LFPG_SmoothedPos[2], targetPos[2], posAlpha);
            finalPos = Vector(px, py, pz);

            // --- Smooth orientation with yaw wrap-around ---
            float prevYaw = m_LFPG_SmoothedOri[0];
            float tgtYaw  = targetOri[0];

            // Handle wrap-around: if difference > 180, adjust previous
            float yawDiff = tgtYaw - prevYaw;
            if (yawDiff > 180.0)
            {
                prevYaw = prevYaw + 360.0;
            }
            if (yawDiff < -180.0)
            {
                prevYaw = prevYaw - 360.0;
            }

            float oriAlpha = LFPG_HOLO_ORI_SMOOTH * timeslice;
            if (oriAlpha > 1.0)
            {
                oriAlpha = 1.0;
            }
            float oy  = Math.Lerp(prevYaw, tgtYaw, oriAlpha);
            float op  = Math.Lerp(m_LFPG_SmoothedOri[1], targetOri[1], oriAlpha);
            float orr = Math.Lerp(m_LFPG_SmoothedOri[2], targetOri[2], oriAlpha);
            finalOri = Vector(oy, op, orr);
        }
        else
        {
            // First frame: snap directly to target (no smoothing)
            finalPos = targetPos;
            finalOri = targetOri;
            m_LFPG_HasPreviousState = true;
        }

        // Store smoothed values for next frame
        m_LFPG_SmoothedPos = finalPos;
        m_LFPG_SmoothedOri = finalOri;

        // Apply to projection entity
        projection.SetPosition(finalPos);
        projection.SetOrientation(finalOri);

        // Force green hologram: since we skip super.UpdateHologram(),
        // vanilla never calls EvaluateCollision(), leaving color stale.
        // SetIsColliding(false) forces the green (valid) material.
        bool bNoCollide = false;
        SetIsColliding(bNoCollide);

        // FIX: Vanilla calls these every frame in UpdateHologram.
        // Without RefreshTrigger, the ProjectionTrigger stays at its
        // initial position and never follows the hologram.
        // Without RefreshVisual, hologram material (green/red rvmat) is
        // never applied after the initial constructor call.
        // Without OnHologramBeingPlaced, projection entities that override
        // this callback would miss their per-frame logic.
        RefreshTrigger();
        RefreshVisual();
        projection.OnHologramBeingPlaced(m_Player);
    }

    // ============================================
    // Collision/validation overrides
    // All bypass collision for LFPG kits since
    // our camera raycast handles placement visually.
    // Works on both client and server (entity-type check).
    // ============================================

    override bool IsColliding()
    {
        if (LFPG_IsLFPGKitProjection())
        {
            return false;
        }
        return super.IsColliding();
    }

    override void EvaluateCollision(ItemBase action_item)
    {
        if (LFPG_IsLFPGKitProjection())
        {
            bool bNoCollide = false;
            SetIsColliding(bNoCollide);
            return;
        }
        super.EvaluateCollision(action_item);
    }

    override bool IsCollidingAngle()
    {
        if (LFPG_IsLFPGKitProjection())
        {
            return false;
        }
        return super.IsCollidingAngle();
    }

    override bool IsFloating()
    {
        if (LFPG_IsLFPGKitProjection())
        {
            return false;
        }
        return super.IsFloating();
    }

    // ---- Getter for external use ----
    bool LFPG_IsWallMode()
    {
        return m_LFPG_IsWallMode;
    }
};
