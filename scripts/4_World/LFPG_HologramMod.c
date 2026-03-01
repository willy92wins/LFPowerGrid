// =========================================================
// LF_PowerGrid - Hologram override for wall/ceiling-aware placement
//
// v0.7.26: Wall placement support for LF_Splitter_Kit.
// v0.7.36: Fix Enforce compile errors (literal in func params),
//          force green holo in wall mode, use Math.AbsFloat.
// v0.7.38 (Fix): All collision/validation overrides now check entity
//   type directly via LFPG_IsLFPGKitProjection() instead of
//   relying on m_LFPG_IsWallMode state. Server-side EvaluateCollision
//   never had wall mode set (UpdateHologram only runs on client),
//   causing it to fall through to vanilla checks that reject wall
//   placement. Type-based check works on both client and server.
// v0.7.47: 3-way surface classification (floor/wall/ceiling).
//   - Generalized LFPG_IsLFPGKitProjection() to cover all LFPG kits.
//   - Raw normalY (no abs) enables distinguishing floor from ceiling.
//   - Ceiling mode: pitch=180 inverts model, yaw from camera direction.
//   - Only CeilingLight_Kit supports ceiling; Splitter uses vanilla reject.
//
// Strategy:
//   1. Do a forward raycast from the camera
//   2. If hit surface is a FLOOR (normalY > threshold) -> call super (100% vanilla)
//   3. If hit surface is a CEILING (normalY < -threshold) -> pitch=180 placement
//   4. If hit surface is a WALL -> custom position/orient, skip vanilla
//
// This means floor placement is IDENTICAL to vanilla behavior.
// Wall/ceiling placement uses our forward raycast and bypasses vanilla
// collision checks that would reject it (angle, floating, etc).
//
// Non-LFPG-kit items always use vanilla (super) behavior.
// =========================================================

modded class Hologram
{
    // ---- Tuning constants ----
    static const float LFPG_HOLO_MAX_RANGE      = 3.0;   // max ray distance (m)
    static const float LFPG_HOLO_WALL_THRESHOLD  = 0.5;   // |normalY| < this = wall
    static const float LFPG_HOLO_SURFACE_OFFSET  = 0.03;  // offset from wall/ceiling (m)

    // ---- State ----
    protected bool m_LFPG_IsWallMode;
    protected bool m_LFPG_IsLFPGKit;

    // ---- Helper: check if projection entity is ANY LFPG deployable kit ----
    // v0.7.47: Generalized from Splitter-only to include all LFPG kits.
    // Does NOT depend on state flags from UpdateHologram.
    // Works on server where UpdateHologram may never have run.
    // Used by all collision/validation overrides.
    protected bool LFPG_IsLFPGKitProjection()
    {
        EntityAI proj = GetProjectionEntity();
        if (!proj)
            return false;

        if (proj.IsKindOf("LF_Splitter_Kit"))
            return true;

        if (proj.IsKindOf("LF_CeilingLight_Kit"))
            return true;

        return false;
    }

    // ---- Main override ----
    override void UpdateHologram(float timeslice)
    {
        // Identify if this is an LFPG deployable kit
        EntityAI projection = GetProjectionEntity();
        m_LFPG_IsLFPGKit = false;
        m_LFPG_IsWallMode = false;

        if (!projection)
        {
            super.UpdateHologram(timeslice);
            return;
        }

        bool isSplitterKit = projection.IsKindOf("LF_Splitter_Kit");
        bool isCeilingKit  = projection.IsKindOf("LF_CeilingLight_Kit");

        if (!isSplitterKit && !isCeilingKit)
        {
            super.UpdateHologram(timeslice);
            return;
        }

        m_LFPG_IsLFPGKit = true;

        // --- Forward raycast to detect wall vs floor vs ceiling ---
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
        int contactComponent;

        // v0.7.38: Exclude projection entity (not player) from raycast.
        // The hologram sits between camera and wall — excluding the player
        // let the ray hit the hologram itself instead of the wall behind it,
        // causing the hologram to snap to itself, jitter, and prevent placement.
        // ObjIntersectFire matches vanilla placement raycasts and has
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

        // v0.7.47: 3-way classification using RAW normalY (no abs).
        // Floor:   normalY > +threshold  (normal points UP)
        // Ceiling: normalY < -threshold  (normal points DOWN)
        // Wall:    everything in between  (normal is ~horizontal)
        float rawNormalY = hitNormal[1];

        if (rawNormalY >= LFPG_HOLO_WALL_THRESHOLD)
        {
            // ================================================================
            // ---- FLOOR ---- (normal points UP)
            // Let vanilla handle everything: positioning, collision, validation
            // ================================================================
            super.UpdateHologram(timeslice);
            return;
        }

        if (rawNormalY <= -LFPG_HOLO_WALL_THRESHOLD)
        {
            // ================================================================
            // ---- CEILING ---- (normal points DOWN)
            // Only CeilingLight_Kit supports ceiling placement.
            // Splitter falls through to vanilla (which will reject — correct).
            // ================================================================
            if (!isCeilingKit)
            {
                super.UpdateHologram(timeslice);
                return;
            }

            m_LFPG_IsWallMode = true;

            // Position: surface + small offset along normal (pushes down from ceiling)
            vector ceilPos = hitPos + (hitNormal * LFPG_HOLO_SURFACE_OFFSET);

            // Orientation: yaw from camera horizontal direction, pitch=180 to invert.
            // Ceiling normal is ~(0,-1,0) so yaw from normal would always be 0.
            // Camera direction gives the player-controlled facing instead.
            float ceilYaw = Math.Atan2(camDir[0], camDir[2]) * Math.RAD2DEG;
            float ceilPitch = 180.0;
            vector ceilOri = Vector(ceilYaw, ceilPitch, 0);

            projection.SetPosition(ceilPos);
            projection.SetOrientation(ceilOri);
            // Double-set: forces physics engine to accept pitch=180
            projection.SetOrientation(projection.GetOrientation());

            bool bNoCeilCollide = false;
            SetIsColliding(bNoCeilCollide);
            return;
        }

        // ================================================================
        // ---- WALL MODE ---- (|normalY| < threshold, normal is ~horizontal)
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
    // v0.7.38 (Fix): Uses entity type check instead of state flags.
    // On the server, m_LFPG_IsWallMode is never set (UpdateHologram only
    // runs on client), causing super.IsColliding() to return the stale
    // value from vanilla EvaluateCollision which rejects wall placement.
    // By checking entity type directly, this works on both client and server.
    // Trade-off: floor collision for LFPG kits is also bypassed, but
    // the client hologram still provides visual feedback, and CanBePlaced
    // returns true unconditionally for the kits.
    // v0.7.47: Generalized to all LFPG kits (Splitter + CeilingLight).
    override bool IsColliding()
    {
        if (LFPG_IsLFPGKitProjection())
        {
            return false;
        }
        return super.IsColliding();
    }

    // ---- Server-side collision evaluation: skip all checks for LFPG kits ----
    // v0.7.38 (Fix): Checks entity type directly instead of m_LFPG_IsWallMode.
    // Server hologram never runs UpdateHologram, so state flags are unset.
    // For LFPG kits, always allow — client hologram already validated visually.
    // v0.7.47: Generalized to all LFPG kits.
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

    // ---- Angle check: always pass for LFPG kits ----
    // v0.7.47: Generalized to all LFPG kits.
    override bool IsCollidingAngle()
    {
        if (LFPG_IsLFPGKitProjection())
        {
            return false;
        }
        return super.IsCollidingAngle();
    }

    // ---- Floating check: LFPG kits can be wall/ceiling-mounted ----
    // v0.7.47: Generalized to all LFPG kits.
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
