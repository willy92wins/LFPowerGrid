// =========================================================
// LF_PowerGrid - cable segment data (v0.7.11)
//
// Pure geometry class: stores endpoints (from/to) for one
// visual sub-segment of a cable wire (after catenaria sag).
//
// Rendering: CableRenderer.DrawFrame() via CableHUD Canvas 2D.
// Occlusion: handled at wire level in LFPG_WireSegmentInfo,
//   NOT per sub-segment (audit: "occlusion samples = few").
// =========================================================

class LFPG_CableParticle
{
    // Segment geometry (set once at build time)
    vector m_From;
    vector m_To;
    protected bool m_Valid;

    void LFPG_CableParticle()
    {
        m_Valid = false;
    }

    // -------------------------------------------
    // Store segment endpoints. Returns false for
    // degenerate (zero-length) segments.
    // -------------------------------------------
    bool Create(vector from, vector to)
    {
        Destroy();

        m_From = from;
        m_To   = to;

        float dist = vector.Distance(m_From, m_To);
        if (dist < 0.01)
        {
            return false;
        }

        m_Valid = true;
        return true;
    }

    bool IsValid()
    {
        return m_Valid;
    }

    void Destroy()
    {
        m_Valid = false;
    }

    void ~LFPG_CableParticle()
    {
        Destroy();
    }
};
