// =========================================================
// LF_PowerGrid - server-side registry deviceId -> EntityAI
// NOTE: kept minimal; avoids global ticks.
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

    EntityAI FindById(string deviceId)
    {
        EntityAI obj;
        if (m_ById.Find(deviceId, obj))
            return obj;
        return null;
    }

    // NOTE: Avoid using `ref` or `out` here to prevent the Enforce warning:
    // "FIX-ME: Method argument can't be strong reference".
    // Passing an object parameter without `ref` gives weak reference semantics,
    // while still allowing us to mutate the same array instance.
    void GetAll(array<EntityAI> outArr)
    {
        if (!outArr)
        {
            // Caller should pass a valid array instance.
            LFPG_Util.Warn("DeviceRegistry.GetAll called with null outArr");
            return;
        }

        outArr.Clear();

        int i;
        for (i = 0; i < m_ById.Count(); i = i + 1)
        {
            outArr.Insert(m_ById.GetElement(i));
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
};
