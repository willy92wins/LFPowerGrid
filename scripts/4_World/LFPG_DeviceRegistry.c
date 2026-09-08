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

    protected ref TStringManagedMap m_ById;
	protected ref map<string, bool> m_AmbiguousIds;
	protected ref array<EntityAI> m_AllRegistered;

    void LFPG_DeviceRegistry()
    {
        m_ById = new TStringManagedMap;
		m_AmbiguousIds = new map<string, bool>;
		m_AllRegistered = new array<EntityAI>;
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

		if (m_AllRegistered.Find(obj) < 0)
			m_AllRegistered.Insert(obj);

		if (m_AmbiguousIds.Contains(deviceId))
			return;

		Managed currentRaw;
		if (m_ById.Find(deviceId, currentRaw))
		{
			EntityAI current = EntityAI.Cast(currentRaw);
			if (!current)
			{
				m_ById.Remove(deviceId);
				m_ById[deviceId] = obj;
				return;
			}
			if (current == obj)
				return;

			if (m_AllRegistered.Find(current) < 0)
				m_AllRegistered.Insert(current);
			m_ById.Remove(deviceId);
			m_AmbiguousIds.Set(deviceId, true);
			LFPG_Util.Error("[DeviceRegistry] Ambiguous live deviceId latched until restart: " + deviceId);
			return;
		}

        m_ById[deviceId] = obj;
    }

    void Unregister(string deviceId, EntityAI objExpected = null)
    {
        if (deviceId == "")
			return;

		if (objExpected)
		{
			int trackedIndex = m_AllRegistered.Find(objExpected);
			if (trackedIndex >= 0)
				m_AllRegistered.RemoveOrdered(trackedIndex);
		}

		if (m_AmbiguousIds.Contains(deviceId))
            return;

        Managed currentRaw;
        if (m_ById.Find(deviceId, currentRaw))
        {
            EntityAI current = EntityAI.Cast(currentRaw);
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
		if (m_AmbiguousIds.Contains(deviceId))
			return null;

        Managed objRaw;
        if (m_ById.Find(deviceId, objRaw))
        {
            EntityAI obj = EntityAI.Cast(objRaw);
            if (!obj)
            {
                m_ById.Remove(deviceId);
                return null;
            }
            return obj;
        }
        return null;
    }

	bool IsAmbiguous(string deviceId)
	{
		if (deviceId == "")
			return false;
		return m_AmbiguousIds.Contains(deviceId);
	}

    // v0.7.44 (Level 4, hallazgo 1a): Filter null refs in GetAll.
    // v0.9.3: Deduplicate by entity pointer — same entity can be registered
    // under multiple keys if TryRegister misses cleanup of old key.
    // Without dedup, RebuildFromWires iterates wires twice → double edges.
    void GetAll(array<EntityAI> outArr)
    {
        if (!outArr)
        {
            LFPG_Util.Warn("DeviceRegistry.GetAll called with null outArr");
            return;
        }

        outArr.Clear();

        // Dedup by entity pointer in O(n): a hashed seen-map replaces the former
        // O(n^2) outArr.Find() scan. Object-keyed maps are a vanilla pattern
        // (scripts/4_world/classes/useractionscomponent/actiontargets.c:4
        // map<Object,Object>). Output order and contents are unchanged.
        ref map<Object, Object> seen = new map<Object, Object>;

        int i;
        for (i = 0; i < m_ById.Count(); i = i + 1)
        {
            EntityAI ent = EntityAI.Cast(m_ById.GetElement(i));
            if (!ent) continue;

            if (seen.Contains(ent))
            {
                // Log the duplicate key for debugging
                string dupKey = m_ById.GetKey(i);
                string dedupMsg = "[DeviceRegistry] DEDUP: dupKey=";
                dedupMsg = dedupMsg + dupKey;
                dedupMsg = dedupMsg + " type=";
                dedupMsg = dedupMsg + ent.GetType();
                LFPG_Util.Warn(dedupMsg);
                continue;
            }

            seen.Set(ent, ent);
            outArr.Insert(ent);
        }
    }

	void GetAllRegisteredForSafety(array<EntityAI> outArr)
	{
		if (!outArr)
		{
			LFPG_Util.Warn("DeviceRegistry.GetAllRegisteredForSafety called with null outArr");
			return;
		}

		outArr.Clear();
		int safetyIndex = 0;
		for (safetyIndex = 0; safetyIndex < m_AllRegistered.Count(); safetyIndex = safetyIndex + 1)
		{
			EntityAI safetyEntity = m_AllRegistered[safetyIndex];
			if (safetyEntity)
				outArr.Insert(safetyEntity);
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
            EntityAI ent = EntityAI.Cast(m_ById.GetElement(i));
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

		int safetyPruned = 0;
		int safetyIndex = m_AllRegistered.Count() - 1;
		for (safetyIndex = m_AllRegistered.Count() - 1; safetyIndex >= 0; safetyIndex = safetyIndex - 1)
        {
			if (!m_AllRegistered[safetyIndex])
			{
				m_AllRegistered.RemoveOrdered(safetyIndex);
				safetyPruned = safetyPruned + 1;
			}
		}

		if (nullKeys.Count() > 0 || safetyPruned > 0)
		{
			LFPG_Util.Info("[DeviceRegistry] Pruned " + nullKeys.Count().ToString() + " null canonical entries and " + safetyPruned.ToString() + " safety refs");
        }

        return nullKeys.Count();
    }

    // v0.7.44: Diagnostic — count entries (for debug logging).
    int Count()
    {
        return m_ById.Count();
    }
};
