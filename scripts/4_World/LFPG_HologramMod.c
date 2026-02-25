// =========================================================
// LF_PowerGrid - Hologram override for wall-aware placement
//
// v0.7.26: Wall placement support for LF_Splitter_Kit.
//
// Strategy:
//   1. Do a forward raycast from the camera
//   2. If hit surface is a WALL -> custom position/orient, skip vanilla
//   3. If hit surface is FLOOR or no hit -> call super (100% vanilla)
//
// This means floor placement is IDENTICAL to vanilla behavior.
// Wall placement uses our forward raycast and bypasses vanilla
// collision checks that would reject it (angle, floating, etc).
//
// Non-splitter items always use vanilla (super) behavior.
// =========================================================

modded class Hologram
{
    // ---- Tuning constants ----
    static const float LFPG_HOLO_MAX_RANGE      = 3.0;   // max ray distance (m)
    static const float LFPG_HOLO_WALL_THRESHOLD  = 0.5;   // |normalY| < this = wall
    static const float LFPG_HOLO_SURFACE_OFFSET  = 0.03;  // offset from wall (m)

    // ---- State ----
    protected bool m_LFPG_IsWallMode;
    protected bool m_LFPG_IsSplitterKit;

    // ---- Main override ----
    override void UpdateHologram(float timeslice)
    {
        // Identify if this is a splitter kit
        EntityAI projection = GetProjectionEntity();
        m_LFPG_IsSplitterKit = false;
        m_LFPG_IsWallMode = false;

        if (!projection)
        {
            super.UpdateHologram(timeslice);
            return;
        }

        if (!projection.IsKindOf("LF_Splitter_Kit"))
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
            false,              // ground_only = false (critical for walls)
            ObjIntersectGeom,
            0.0
        );

        if (!hit)
        {
            // No surface found - vanilla floor logic
            super.UpdateHologram(timeslice);
            return;
        }

        // Classify surface by vertical component of normal
        float normalY = hitNormal[1];
        if (normalY < 0) normalY = normalY * -1.0; // abs

        if (normalY >= LFPG_HOLO_WALL_THRESHOLD)
        {
            // ---- FLOOR / CEILING ----
            // Let vanilla handle everything: positioning, collision, validation
            super.UpdateHologram(timeslice);
            return;
        }

        // ================================================================
        // ---- WALL MODE ----
        // From here we do NOT call super - vanilla would reject this.
        // ================================================================
        m_LFPG_IsWallMode = true;

        // Position: hit point + small offset along surface normal (away from wall)
        vector finalPos = hitPos + (hitNormal * LFPG_HOLO_SURFACE_OFFSET);

        // Orientation: model faces outward from wall
        // Yaw from the horizontal component of surface normal
        float yaw = Math.Atan2(hitNormal[0], hitNormal[2]) * Math.RAD2DEG;
        vector finalOri = Vector(yaw, 0, 0);

        // Apply to projection entity
        projection.SetPosition(finalPos);
        projection.SetOrientation(finalOri);

        // Force collision state: we bypass EvaluateCollision() entirely because
        // vanilla checks (angle, floating, surface) would reject wall placement.
        // Validation is handled by IsColliding/IsCollidingAngle/IsFloating overrides below.
        // Do NOT call EvaluateCollision() or SetIsColliding() here.
    }

    // ---- Main collision gate: action system calls this to allow/block placement ----
    override bool IsColliding()
    {
        if (m_LFPG_IsSplitterKit && m_LFPG_IsWallMode)
        {
            return false; // Wall raycast already validated the surface
        }
        return super.IsColliding();
    }

    // ---- Angle check: always pass for splitter kit on wall ----
    override bool IsCollidingAngle()
    {
        if (m_LFPG_IsSplitterKit && m_LFPG_IsWallMode)
        {
            return false;
        }
        return super.IsCollidingAngle();
    }

    // ---- Floating check: wall placement is technically "floating" ----
    override bool IsFloating()
    {
        if (m_LFPG_IsSplitterKit && m_LFPG_IsWallMode)
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
