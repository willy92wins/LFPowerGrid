// =========================================================
// LF_PowerGrid - Hologram override for device kit placement
//
// v0.7.26: Wall placement support for LF_Splitter_Kit.
// v0.7.47: Solar Panel Kit projection swap (box → panel model).
//
// Strategy:
//   Splitter Kit:
//     1. Forward raycast from camera
//     2. If WALL → custom position/orient, skip vanilla
//     3. If FLOOR or no hit → call super (100% vanilla)
//
//   Solar Panel Kit:
//     1. ProjectionBasedOnParent returns "LF_SolarPanel" (model swap)
//     2. UpdateHologram uses vanilla floor placement (super)
//     3. Collision bypassed (model mismatch would cause false rejects)
//
// Non-kit items always use vanilla (super) behavior.
//
// ALL hologram overrides for ALL kits go in this ONE file.
// Enforce Script cannot have two separate modded Hologram classes.
// =========================================================

modded class Hologram
{
    // ---- Tuning constants ----
    static const float LFPG_HOLO_MAX_RANGE      = 3.0;
    static const float LFPG_HOLO_WALL_THRESHOLD  = 0.5;
    static const float LFPG_HOLO_SURFACE_OFFSET  = 0.03;

    // ---- State ----
    protected bool m_LFPG_IsWallMode;
    protected bool m_LFPG_IsSplitterKit;
    protected bool m_LFPG_IsSolarKit;        // v0.7.47: solar panel kit flag

    // ============================================
    // v0.7.47: Projection model swap for different-model kits
    // ============================================
    // When placing a solar panel kit (box model), the hologram
    // should show the deployed solar panel model instead.
    // This override is called during Hologram construction to
    // determine which class to instantiate as the projection entity.
    override string ProjectionBasedOnParent()
    {
        if (m_Parent)
        {
            LF_SolarPanel_Kit solarKit = LF_SolarPanel_Kit.Cast(m_Parent);
            if (solarKit)
            {
                return solarKit.GetDeployedClassname();
            }
        }

        return super.ProjectionBasedOnParent();
    }

    // ---- Main hologram update ----
    override void UpdateHologram(float timeslice)
    {
        EntityAI projection = GetProjectionEntity();
        m_LFPG_IsSplitterKit = false;
        m_LFPG_IsWallMode = false;
        m_LFPG_IsSolarKit = false;

        if (!projection)
        {
            super.UpdateHologram(timeslice);
            return;
        }

        // ---- v0.7.47: Solar Panel Kit detection ----
        // Set flag for collision bypass, then use vanilla floor placement.
        if (m_Parent && m_Parent.IsKindOf("LF_SolarPanel_Kit"))
        {
            m_LFPG_IsSolarKit = true;
            super.UpdateHologram(timeslice);
            return;
        }

        // ---- Splitter Kit detection ----
        if (!m_Parent || !m_Parent.IsKindOf("LF_Splitter_Kit"))
        {
            super.UpdateHologram(timeslice);
            return;
        }

        m_LFPG_IsSplitterKit = true;

        // --- Forward raycast to detect wall vs floor ---
        PlayerBase player = GetParentPlayer();
        if (!player)
        {
            super.UpdateHologram(timeslice);
            return;
        }

        vector camPos = GetGame().GetCurrentCameraPosition();
        vector camDir = GetGame().GetCurrentCameraDirection();
        vector rayEnd = camPos + (camDir * LFPG_HOLO_MAX_RANGE);

        vector hitPos;
        vector hitNormal;
        int contactComponent;

        bool hit = DayZPhysics.RaycastRV(
            camPos,
            rayEnd,
            hitPos,
            hitNormal,
            contactComponent,
            null,
            null,
            player,
            false,
            false,
            ObjIntersectGeom,
            0.0
        );

        if (!hit)
        {
            super.UpdateHologram(timeslice);
            return;
        }

        // Classify surface by vertical component of normal
        float normalY = hitNormal[1];
        if (normalY < 0) normalY = normalY * -1.0;

        if (normalY >= LFPG_HOLO_WALL_THRESHOLD)
        {
            // FLOOR / CEILING — vanilla handles everything
            super.UpdateHologram(timeslice);
            return;
        }

        // ================================================================
        // WALL MODE — do NOT call super, vanilla would reject this.
        // ================================================================
        m_LFPG_IsWallMode = true;

        vector finalPos = hitPos + (hitNormal * LFPG_HOLO_SURFACE_OFFSET);

        float yaw = Math.Atan2(hitNormal[0], hitNormal[2]) * Math.RAD2DEG;
        vector finalOri = Vector(yaw, 0, 0);

        projection.SetPosition(finalPos);
        projection.SetOrientation(finalOri);
    }

    // ---- Collision gate ----
    override bool IsColliding()
    {
        if (m_LFPG_IsSplitterKit && m_LFPG_IsWallMode)
        {
            return false;
        }
        // v0.7.47: Solar kit bypass — projection model differs from kit model,
        // vanilla collision checks may produce false positives.
        if (m_LFPG_IsSolarKit)
        {
            return false;
        }
        return super.IsColliding();
    }

    // ---- Angle check ----
    override bool IsCollidingAngle()
    {
        if (m_LFPG_IsSplitterKit && m_LFPG_IsWallMode)
        {
            return false;
        }
        if (m_LFPG_IsSolarKit)
        {
            return false;
        }
        return super.IsCollidingAngle();
    }

    // ---- Floating check ----
    override bool IsFloating()
    {
        if (m_LFPG_IsSplitterKit && m_LFPG_IsWallMode)
        {
            return false;
        }
        if (m_LFPG_IsSolarKit)
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
