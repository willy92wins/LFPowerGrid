// =========================================================
// LF_PowerGrid - world-level utility helpers (PlayerBase etc.)
//
// v0.7.14: Added ClipBehindCamera() for proper near-plane clipping
//          of behind-camera cable/preview segments.
// =========================================================

class LFPG_WorldUtil
{
    // Server-authoritative check for actions that require the wiring tool.
    static bool PlayerHasCableReelInHands(PlayerBase pb)
    {
        if (!pb) return false;

        HumanInventory inv = pb.GetHumanInventory();
        if (!inv) return false;

        EntityAI inHands = inv.GetEntityInHands();
        if (!inHands) return false;

        return inHands.IsKindOf("LF_CableReel");
    }

    // Server-authoritative check for cut action (requires Pliers or similar).
    static bool PlayerHasPliersInHands(PlayerBase pb)
    {
        if (!pb) return false;

        HumanInventory inv = pb.GetHumanInventory();
        if (!inv) return false;

        EntityAI inHands = inv.GetEntityInHands();
        if (!inHands) return false;

        return inHands.IsKindOf("Pliers");
    }

    // --------------------------
    // Cable rendering helpers
    // --------------------------
    static vector ClampAboveSurface(vector p, float minOffset = 0.05)
    {
        // v0.7.9: Guard against NaN/infinite coordinates from corrupted persistence.
        // NaN comparisons always return false, so without this check a NaN position
        // would pass through unchanged and corrupt downstream geometry.
        //
        // Strategy: clamp each NaN component to zero individually rather than
        // returning a fixed "0 10 0" that could create cables spanning the map.
        // For out-of-bounds coords, clamp to the boundary instead of replacing entirely.
        float px = p[0];
        float py = p[1];
        float pz = p[2];

        bool wasInvalid = false;

        // Fix individual NaN components
        if (px != px) { px = 0.0; wasInvalid = true; }
        if (py != py) { py = 0.0; wasInvalid = true; }
        if (pz != pz) { pz = 0.0; wasInvalid = true; }

        // Clamp to reasonable world bounds (generous for custom maps up to 20km)
        float minB = -500.0;
        float maxB = 20500.0;

        if (px < minB) { px = minB; wasInvalid = true; }
        if (px > maxB) { px = maxB; wasInvalid = true; }
        if (pz < minB) { pz = minB; wasInvalid = true; }
        if (pz > maxB) { pz = maxB; wasInvalid = true; }

        if (wasInvalid)
        {
            LFPG_Util.Warn("[WorldUtil] ClampAboveSurface: corrected invalid coord " + p.ToString());
            p[0] = px;
            p[1] = py;
            p[2] = pz;
        }

        float sy = GetGame().SurfaceY(px, pz);
        if (py < sy + minOffset)
            p[1] = sy + minOffset;
        return p;
    }

    // For straight wires (no waypoints): generate a midpoint that clears terrain.
    // This avoids "wire under the ground" when endpoints are near/below the surface or terrain is between them.
    // v0.7.38 (M8): Increased default samples from 6 to 10 for better
    // hill detection. Replaced (float) cast with explicit float division.
    // Only called at build time (not per-frame), so extra samples are free.
    static vector AutoMidpointAboveTerrain(vector a, vector b, float lift = 0.25, int samples = 10)
    {
        float maxY = -10000.0;
        float fSamples = samples;

        int i;
        for (i = 0; i <= samples; i = i + 1)
        {
            float t = i / fSamples;
            vector p = a + (b - a) * t;
            float sy = GetGame().SurfaceY(p[0], p[2]);
            if (sy > maxY) maxY = sy;
        }

        vector mid = (a + b) * 0.5;
        float want = maxY + lift;

        if (mid[1] < want)
            mid[1] = want;

        return mid;
    }

    // v0.7.10: Squared distance between two points.
    // Use for all threshold comparisons (distSq < threshSq) to avoid
    // the sqrt inside vector.Distance. Compare against threshold*threshold.
    // Only call vector.Distance when the actual distance value is needed
    // (e.g. for LOD interpolation, alpha fade, length accumulation).
    static float DistSq(vector a, vector b)
    {
        float dx = a[0] - b[0];
        float dy = a[1] - b[1];
        float dz = a[2] - b[2];
        return dx * dx + dy * dy + dz * dz;
    }

    // v0.7.14: Clip a behind-camera point toward a visible point in world space,
    // then project the clipped point to get valid screen coordinates.
    //
    // When GetScreenPos() is called on a point behind the camera, it returns
    // mirrored/garbage screen coords. The old approach tried to compute an
    // extension direction from those garbage coords, producing lines parallel
    // to screen edges.
    //
    // This helper clips the 3D segment against the camera's near plane,
    // producing a geometrically correct screen position that extends
    // the line toward the correct screen edge.
    //
    // behindWorld = the endpoint that is behind the camera (world space)
    // visibleWorld = the endpoint that is in front of the camera (world space)
    // camPos = camera world position
    // camDir = camera forward direction (normalized)
    //
    // Returns: screen-space vector from GetScreenPos of the clipped point.
    static vector ClipBehindCamera(vector behindWorld, vector visibleWorld, vector camPos, vector camDir)
    {
        // Near plane offset: slightly in front of camera to avoid edge artifacts.
        float nearClip = 0.3;

        // Signed distance from camera plane for each point.
        // d = dot(point - camPos, camDir)
        // Positive = in front of camera, negative = behind.
        vector diffA = behindWorld - camPos;
        float dA = diffA[0] * camDir[0] + diffA[1] * camDir[1] + diffA[2] * camDir[2];

        vector diffB = visibleWorld - camPos;
        float dB = diffB[0] * camDir[0] + diffB[1] * camDir[1] + diffB[2] * camDir[2];

        // Compute t parameter where segment crosses the near plane.
        // P(t) = behind + t * (visible - behind)
        // dot(P(t) - camPos, camDir) = nearClip
        // t = (nearClip - dA) / (dB - dA)
        float denom = dB - dA;
        if (denom < 0.001 && denom > -0.001)
        {
            // Degenerate: both points at same depth. Return visible projection.
            return GetGame().GetScreenPos(visibleWorld);
        }

        float t = (nearClip - dA) / denom;

        // v0.7.35 (F1.4): NaN guard — floating point edge cases in Enforce VM
        // can produce NaN that passes through clamp (NaN comparisons are false).
        if (t != t)
        {
            return GetGame().GetScreenPos(visibleWorld);
        }

        // Clamp t to [0,1] for safety
        if (t < 0.0)
        {
            t = 0.0;
        }
        if (t > 1.0)
        {
            t = 1.0;
        }

        // Interpolate to get the clipped world point
        vector clipped = behindWorld + (visibleWorld - behindWorld) * t;

        return GetGame().GetScreenPos(clipped);
    }

    // =========================================================
    // Cohen-Sutherland line clipping (v0.7.38 H1)
    // =========================================================
    // Shared implementation used by both CableRenderer and WiringClient.
    // Clips a 2D line segment to a rectangular region.
    // Returns true if any portion is visible.
    // Clipped coordinates written to clipA[0..1] and clipB[0..1].

    // Outcode bits: 1=LEFT, 2=RIGHT, 4=TOP, 8=BOTTOM
    static int ComputeOutcode(float x, float y, float minX, float minY, float maxX, float maxY)
    {
        int code = 0;
        if (x < minX)
        {
            code = code | 1;
        }
        if (x > maxX)
        {
            code = code | 2;
        }
        if (y < minY)
        {
            code = code | 4;
        }
        if (y > maxY)
        {
            code = code | 8;
        }
        return code;
    }

    // Cohen-Sutherland line clipping against screen rectangle.
    // Returns true if any portion is visible (clipped result in clipA, clipB).
    // Returns false if the segment is entirely outside the rectangle.
    // Max 8 iterations to guarantee termination.
    static bool ClipSegToScreen(float x1, float y1, float x2, float y2,
                                float minX, float minY, float maxX, float maxY,
                                out vector clipA, out vector clipB)
    {
        int codeA = ComputeOutcode(x1, y1, minX, minY, maxX, maxY);
        int codeB = ComputeOutcode(x2, y2, minX, minY, maxX, maxY);

        int iter = 0;
        while (iter < 8)
        {
            iter = iter + 1;

            // Both inside: accept
            if ((codeA | codeB) == 0)
            {
                clipA[0] = x1;
                clipA[1] = y1;
                clipB[0] = x2;
                clipB[1] = y2;
                return true;
            }

            // Both on same outside side: reject
            if ((codeA & codeB) != 0)
            {
                return false;
            }

            // Pick the endpoint that is outside
            int codeOut = codeA;
            if (codeOut == 0)
            {
                codeOut = codeB;
            }

            float dx = x2 - x1;
            float dy = y2 - y1;
            float cx = 0.0;
            float cy = 0.0;

            // Clip against the boundary indicated by codeOut
            if ((codeOut & 8) != 0)
            {
                if (dy > -0.001 && dy < 0.001)
                    return false;
                cx = x1 + dx * (maxY - y1) / dy;
                cy = maxY;
            }
            else if ((codeOut & 4) != 0)
            {
                if (dy > -0.001 && dy < 0.001)
                    return false;
                cx = x1 + dx * (minY - y1) / dy;
                cy = minY;
            }
            else if ((codeOut & 2) != 0)
            {
                if (dx > -0.001 && dx < 0.001)
                    return false;
                cy = y1 + dy * (maxX - x1) / dx;
                cx = maxX;
            }
            else if ((codeOut & 1) != 0)
            {
                if (dx > -0.001 && dx < 0.001)
                    return false;
                cy = y1 + dy * (minX - x1) / dx;
                cx = minX;
            }

            // Replace the outside endpoint with the clipped point
            if (codeOut == codeA)
            {
                x1 = cx;
                y1 = cy;
                codeA = ComputeOutcode(x1, y1, minX, minY, maxX, maxY);
            }
            else
            {
                x2 = cx;
                y2 = cy;
                codeB = ComputeOutcode(x2, y2, minX, minY, maxX, maxY);
            }
        }

        // Max iterations reached — accept with current coords (safety fallback)
        clipA[0] = x1;
        clipA[1] = y1;
        clipB[0] = x2;
        clipB[1] = y2;
        return true;
    }
};
