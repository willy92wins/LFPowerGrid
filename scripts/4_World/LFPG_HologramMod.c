// =========================================================
// LF_PowerGrid - Hologram override for wall-aware placement
//
// v0.7.26: Wall placement support for LF_Splitter_Kit.
// v0.7.36: Fix Enforce compile errors (literal in func params),
//          force green holo in wall mode, use Math.AbsFloat.
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
        PlayerBase player = m_Player;
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
        int contactComponent;  // required out param for RaycastRV, unused

        // v0.7.38: Exclude projection entity (not player) from raycast.
        // The hologram sits between camera and wall — excluding the player
        // let the ray hit the hologram itself instead of the wall behind it,
        // causing the hologram to snap to itself, jitter, and prevent placement.
        // Also: ObjIntersectFire matches vanilla placement raycasts and has
        // better coverage of building walls than ObjIntersectGeom.
        float rayRadius = 0.0;
        set<Object> rayResults = null;
        Object rayWith = null;
        bool bSorted = false;
        bool bGroundOnly = false;

        bool hit = DayZPhysics.RaycastRV(camPos, rayEnd, hitPos, hitNormal, contactComponent, rayResults, rayWith, projection, bSorted, bGroundOnly, ObjIntersectFire, rayRadius);

        if (!hit)
        {
            // No surface found - vanilla floor logic
            super.UpdateHologram(timeslice);
            return;
        }

        // Classify surface by vertical component of normal
        // v0.7.36: Use Math.AbsFloat instead of manual negation
        float normalY = Math.AbsFloat(hitNormal[1]);

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

        // v0.7.36: Force green hologram — since we skip super.UpdateHologram()
        // vanilla never calls EvaluateCollision(), leaving the holo color stale.
        // SetIsColliding(false) forces the green (valid) material.
        bool bNoCollide = false;
        SetIsColliding(bNoCollide);
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

    // ---- Server-side collision evaluation: skip all checks in wall mode ----
    // v0.7.38: ActionPlaceObject/ActionDeployObject may call EvaluateCollision
    // on the server hologram. Vanilla checks (IsBaseViable, HeightPlacement,
    // IsInTerrain) all fail for wall placement. Skip entirely when wall mode
    // was detected by UpdateHologram's raycast.
    override void EvaluateCollision(ItemBase action_item)
    {
        if (m_LFPG_IsSplitterKit && m_LFPG_IsWallMode)
        {
            bool bNoCollide = false;
            SetIsColliding(bNoCollide);
            return;
        }
        super.EvaluateCollision(action_item);
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
