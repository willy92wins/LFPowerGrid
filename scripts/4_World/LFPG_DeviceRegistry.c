// =========================================================
// LF_PowerGrid - server-side registry deviceId -> EntityAI
// NOTE: kept minimal; avoids global ticks.
//
// v0.7.43 (Fix 1): Stale ref auto-prune in FindById.
// v0.7.44 (Level 4): Null filter in GetAll (hallazgo 1a),
//                     Count() diagnostic helper.
// =========================================================

class LFPG_DeviceRegistry
{
    protected static ref LFPG_DeviceRegistry s_Instance;

    protected ref map<string, EntityAI> m_ById;

    void LFPG_DeviceRegistry()
    {
        m_ById = new map<string, EntityAI>;
    }

    static LFPG_DeviceRegistry Get()
    {
        if (!s_Instance)
            s_Instance = new LFPG_DeviceRegistry();
        return s_Instance;
    }

    void Register(EntityAI obj, string deviceId)
    {
        if (!obj || deviceId == "")
            return;

        m_ById[deviceId] = obj;
    }

    void Unregister(string deviceId, EntityAI objExpected = null)
    {
        if (deviceId == "")
            return;

        EntityAI current;
        if (m_ById.Find(deviceId, current))
        {
            if (!objExpected || objExpected == current)
                m_ById.Remove(deviceId);
        }
    }

    // v0.7.43 (Fix 1): Validate entity ref is still alive.
    // DayZ can invalidate EntityAI refs when the C++ backing
    // is destroyed (streaming, forced deletion without EEDelete).
    // Stale refs evaluate as non-null in map but null in usage.
    // One null-check per lookup — zero overhead for valid refs.
    EntityAI FindById(string deviceId)
    {
        EntityAI obj;
        if (m_ById.Find(deviceId, obj))
        {
            if (!obj)
            {
                m_ById.Remove(deviceId);
                return null;
            }
            return obj;
        }
        return null;
    }

    // v0.7.44 (Level 4, hallazgo 1a): Filter null refs in GetAll.
    // Stale EntityAI refs can linger after engine invalidation (streaming,
    // LOD unload). Without filtering, callers receive zombie pointers.
    // All current callers have null guards, but this is defense-in-depth.
    // NOTE: Avoid using `ref` or `out` here to prevent the Enforce warning:
    // "FIX-ME: Method argument can't be strong reference".
    // Passing an object parameter without `ref` gives weak reference semantics,
    // while still allowing us to mutate the same array instance.
    void GetAll(array<EntityAI> outArr)
    {
        if (!outArr)
        {
            LFPG_Util.Warn("DeviceRegistry.GetAll called with null outArr");
            return;
        }

        outArr.Clear();

        int i;
        for (i = 0; i < m_ById.Count(); i = i + 1)
        {
            EntityAI ent = m_ById.GetElement(i);
            if (!ent) continue;
            outArr.Insert(ent);
        }
    }

    // v0.7.4: remove entries where the entity reference has been
    // invalidated by the engine (despawn, streaming, forced deletion
    // without EEDelete). Called during self-heal.
    int PruneNullEntries()
    {
        ref array<string> nullKeys = new array<string>;

        int i;
        for (i = 0; i < m_ById.Count(); i = i + 1)
        {
            EntityAI ent = m_ById.GetElement(i);
            if (!ent)
            {
                nullKeys.Insert(m_ById.GetKey(i));
            }
        }

        int k;
        for (k = 0; k < nullKeys.Count(); k = k + 1)
        {
            m_ById.Remove(nullKeys[k]);
        }

        if (nullKeys.Count() > 0)
        {
            LFPG_Util.Info("[DeviceRegistry] Pruned " + nullKeys.Count().ToString() + " null entries");
        }

        return nullKeys.Count();
    }

    // v0.7.44: Diagnostic — count entries (for debug logging).
    int Count()
    {
        return m_ById.Count();
    }
};
