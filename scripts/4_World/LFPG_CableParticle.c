#ifndef SERVER
// Client-only compilation boundary
// =========================================================
// LF_PowerGrid - cable segment data (v0.7.11)
//
// Pure geometry class: stores endpoints (from/to) for one
// visual sub-segment of a cable wire (after catenaria sag).
//
// Rendering: CableRenderer.DrawFrame() via CableHUD Canvas 2D,
//   or one client-local 3D object per sub-segment (LFPG_Cable3D.c).
// Occlusion: handled at wire level in LFPG_WireSegmentInfo,
//   NOT per sub-segment (audit: "occlusion samples = few").
// =========================================================

class LFPG_CableParticle
{
    // Segment geometry (set once at build time)
    vector m_From;
    vector m_To;
    protected bool m_Valid;

    // 3D backend (LFPG_Cable3D): client-local object of this sub-segment,
    // its applied and wanted look (LFPG_C3D_LOOK_*, -1 = none), the failed
    // creation attempts for the current wanted look, and whether the work
    // queue holds an entry for it (at most one; cleared when it is dropped).
    Object m_Cable3DObj;
    int m_Cable3DLook = -1;
    int m_Cable3DWant = -1;
    int m_Cable3DTries;
    bool m_Cable3DQueued;

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
        // Next LFPG_Cable3D.Tick recomputes the wanted looks (3D mode only).
        LFPG_Cable3D.MarkLookDirty();
        return true;
    }

    bool IsValid()
    {
        return m_Valid;
    }

    void Destroy()
    {
        m_Valid = false;
        if (m_Cable3DObj)
        {
            LFPG_Cable3D.DeleteObject(m_Cable3DObj);
            m_Cable3DObj = null;
        }
        m_Cable3DLook = -1;
        m_Cable3DWant = -1;
    }

    void ~LFPG_CableParticle()
    {
        Destroy();
    }
};
#endif
