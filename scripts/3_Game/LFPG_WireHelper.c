// =========================================================
// LF_PowerGrid - Wire ownership helper (v0.7.16, Hotfix)
//
// Centralized wire array manipulation. Used by any device
// that owns wires (LFPG_Generator, LFPG_Splitter, future).
//
// All methods are static and operate on an external array.
// The calling device handles SetSynchDirty() after mutation.
//
// v0.7.15 (Sprint 3):
//   - ValidateWireData: exhaustive per-wire sanitization
//   - DeserializeJSON: exhaustive per-wire validation
//   - ValidateWaypoints: NaN + range check per waypoint
//
// v0.7.16 (Hotfix):
//   - H1: Fixed migration log (was always false)
//   - H2: Waypoints out-of-range → discard wire (was clamp)
//   - H2: Inter-waypoint distance check (> MAX_WIRE_LEN → discard)
//   - H3: Map-based O(N) dedup in DeserializeJSON
//   - H7: Port string constants
// =========================================================

class LFPG_WireHelper
{
    // ===========================
    // Wire validation (v0.7.15, Sprint 3 P2)
    // ===========================

    // Validate a single wire's data integrity.
    // Returns true if wire is valid and should be kept.
    // Returns false if wire is corrupt and should be discarded.
    // Applies fixups where possible (defaults for missing fields).
    static bool ValidateWireData(LFPG_WireData wd, string debugLabel)
    {
        if (!wd)
            return false;

        // Required fields
        if (wd.m_TargetDeviceId == "")
        {
            LFPG_Util.Warn("[" + debugLabel + "] Wire discarded: empty m_TargetDeviceId");
            return false;
        }

        if (wd.m_TargetPort == "")
        {
            LFPG_Util.Warn("[" + debugLabel + "] Wire discarded: empty m_TargetPort (target=" + wd.m_TargetDeviceId + ")");
            return false;
        }

        // Forward-compat default for source port
        if (wd.m_SourcePort == "")
        {
            wd.m_SourcePort = LFPG_PORT_OUTPUT_1;
            LFPG_Util.Info("[" + debugLabel + "] Wire fixup: empty m_SourcePort → output_1 (target=" + wd.m_TargetDeviceId + ")");
        }

        // Ensure waypoints array exists
        if (!wd.m_Waypoints)
        {
            wd.m_Waypoints = new array<vector>;
        }

        // Cap waypoints
        if (wd.m_Waypoints.Count() > LFPG_MAX_WAYPOINTS)
        {
            LFPG_Util.Warn("[" + debugLabel + "] Wire fixup: truncating " + wd.m_Waypoints.Count().ToString() + " waypoints to " + LFPG_MAX_WAYPOINTS.ToString());
            while (wd.m_Waypoints.Count() > LFPG_MAX_WAYPOINTS)
            {
                wd.m_Waypoints.RemoveOrdered(wd.m_Waypoints.Count() - 1);
            }
        }

        // Validate each waypoint (NaN + range)
        if (!ValidateWaypoints(wd.m_Waypoints, debugLabel, wd.m_TargetDeviceId))
        {
            // Wire has corrupt waypoints → discard entire wire
            return false;
        }

        return true;
    }

    // Validate all waypoints in an array.
    // Returns false if any waypoint is irreparably corrupt.
    //
    // v0.7.16 (H2): Changed from clamp to discard.
    // Clamping out-of-range waypoints to map edges created "spaghetti" cables
    // stretching across the entire map. Now we discard the wire entirely if:
    //   - Any coordinate is NaN (irrecoverable)
    //   - Any coordinate is outside world bounds (corrupt data)
    //   - Any two consecutive waypoints are farther than MAX_WIRE_LEN_M (corrupt topology)
    static bool ValidateWaypoints(array<vector> wps, string debugLabel, string wireId)
    {
        if (!wps)
            return true;

        int i;
        for (i = 0; i < wps.Count(); i = i + 1)
        {
            vector wp = wps[i];
            float px = wp[0];
            float py = wp[1];
            float pz = wp[2];

            // NaN check — irrecoverable, discard wire
            if (px != px || py != py || pz != pz)
            {
                LFPG_Util.Warn("[" + debugLabel + "] Wire discarded: NaN waypoint[" + i.ToString() + "] (target=" + wireId + ")");
                return false;
            }

            bool outX = (px < LFPG_COORD_MIN || px > LFPG_COORD_MAX);
			bool outZ = (pz < LFPG_COORD_MIN || pz > LFPG_COORD_MAX);
			bool outY = (py < -500.0 || py > 2000.0);

			if (outX || outZ || outY)
			{
				LFPG_Util.Warn("[" + debugLabel + "] Wire discarded: waypoint[" + i.ToString() + "] out of range (target=" + wireId + ")");
				return false;
			}

            // Inter-waypoint distance check (v0.7.16 H2)
            // If consecutive waypoints are farther than MAX_WIRE_LEN_M,
            // the data is corrupt (legitimate cables can't exceed this).
            if (i > 0)
            {
                vector prev = wps[i - 1];
                float dx = px - prev[0];
                float dy = py - prev[1];
                float dz = pz - prev[2];
                float distSq = dx * dx + dy * dy + dz * dz;
                float maxSq = LFPG_MAX_WIRE_LEN_M * LFPG_MAX_WIRE_LEN_M;
                if (distSq > maxSq)
                {
                    LFPG_Util.Warn("[" + debugLabel + "] Wire discarded: waypoint[" + i.ToString() + "] too far from previous (target=" + wireId + ")");
                    return false;
                }
            }
        }

        return true;
    }

    // ===========================
    // Wire array manipulation (unchanged from v0.7.11)
    // ===========================

    // Add a wire to the array. Returns true if inserted.
    // Handles: default source port, settings cap, hardcap, dedup.
    static bool AddWire(array<ref LFPG_WireData> wires, LFPG_WireData wd)
    {
        if (!wires || !wd)
            return false;

        // Forward-compat default
        if (wd.m_SourcePort == "")
            wd.m_SourcePort = LFPG_PORT_OUTPUT_1;

        // Settings cap
        LFPG_ServerSettings st = LFPG_Settings.Get();
        if (st && st.MaxWiresPerDevice > 0 && wires.Count() >= st.MaxWiresPerDevice)
        {
            LFPG_Util.Info("[WireHelper] MaxWiresPerDevice reached");
            return false;
        }

        // Hard cap
        if (wires.Count() >= LFPG_MAX_WIRES_PER_DEVICE)
            return false;

        // Deduplicate exact same link
        int i;
        for (i = 0; i < wires.Count(); i = i + 1)
        {
            LFPG_WireData e = wires[i];
            if (!e)
                continue;
            if (e.m_TargetDeviceId == wd.m_TargetDeviceId && e.m_TargetPort == wd.m_TargetPort && e.m_SourcePort == wd.m_SourcePort)
                return false;
        }

        wires.Insert(wd);
        return true;
    }

    // Clear all wires. Returns true if anything was removed.
    static bool ClearAll(array<ref LFPG_WireData> wires)
    {
        if (!wires)
            return false;
        if (wires.Count() == 0)
            return false;

        wires.Clear();
        return true;
    }

    // Clear wires created by a given player, plus unclaimed wires (empty CreatorId).
    // Returns true if changed.
    static bool ClearForCreator(array<ref LFPG_WireData> wires, string creatorId)
    {
        if (!wires || creatorId == "")
            return false;

        bool changed = false;
        int i;
        for (i = wires.Count() - 1; i >= 0; i = i - 1)
        {
            LFPG_WireData wd = wires[i];
            if (!wd)
            {
                wires.Remove(i);
                changed = true;
                continue;
            }
            if (wd.m_CreatorId == "" || wd.m_CreatorId == creatorId)
            {
                wires.Remove(i);
                changed = true;
            }
        }

        return changed;
    }

    // Remove wires whose targets are not in the provided set of valid IDs.
    // Caller (4_World) builds validIds from DeviceRegistry, keeping
    // WireHelper independent of 4_World classes.
    static bool PruneMissingTargets(array<ref LFPG_WireData> wires, map<string, bool> validIds)
    {
        if (!wires || wires.Count() == 0)
            return false;

        bool changed = false;
        int i;
        for (i = wires.Count() - 1; i >= 0; i = i - 1)
        {
            LFPG_WireData wd = wires[i];
            if (!wd)
            {
                wires.Remove(i);
                changed = true;
                continue;
            }

            bool exists = false;
            if (validIds)
            {
                validIds.Find(wd.m_TargetDeviceId, exists);
            }

            if (!exists)
            {
                wires.Remove(i);
                changed = true;
            }
        }

        return changed;
    }

    // Serialize wire array to JSON string.
    static void SerializeJSON(array<ref LFPG_WireData> wires, out string jsonOut)
    {
        jsonOut = "";

        LFPG_PersistBlob blob = new LFPG_PersistBlob();
        blob.ver = LFPG_PERSIST_VER;

        if (wires)
        {
            int i;
            for (i = 0; i < wires.Count(); i = i + 1)
            {
                blob.wires.Insert(wires[i]);
            }
        }

        string err;
        if (!JsonFileLoader<LFPG_PersistBlob>.MakeData(blob, jsonOut, err, false))
        {
            jsonOut = "";
        }
    }

    // Deserialize JSON into wire array (clears existing contents).
    // v0.7.15: Exhaustive per-wire validation.
    // v0.7.16: H1 fix migration log, H3 map-based O(N) dedup.
    // v0.7.34: Removed migrator chain (no production saves exist pre-v3).
    static void DeserializeJSON(array<ref LFPG_WireData> wires, string jsonIn, string debugLabel)
    {
        if (!wires)
            return;

        wires.Clear();

        if (jsonIn == "")
            return;

        LFPG_PersistBlob blob = new LFPG_PersistBlob();
        string err;
        if (!JsonFileLoader<LFPG_PersistBlob>.LoadData(jsonIn, blob, err))
        {
            LFPG_Util.Info("[" + debugLabel + "] Deserialize wires failed: " + err);
            return;
        }

        if (!blob.wires)
            return;

        LFPG_ServerSettings st = LFPG_Settings.Get();
        int maxWires = LFPG_MAX_WIRES_PER_DEVICE;
        if (st && st.MaxWiresPerDevice > 0)
        {
            maxWires = Math.Min(maxWires, st.MaxWiresPerDevice);
        }

        // v0.7.16 H3: Map-based O(N) dedup (replaced legacy IsDuplicate loop)
        ref map<string, bool> dedupMap = new map<string, bool>;

        int discarded = 0;
        int duplicates = 0;
        int count = blob.wires.Count();
        int i;
        for (i = 0; i < count; i = i + 1)
        {
            // Cap check
            if (wires.Count() >= maxWires)
            {
                LFPG_Util.Info("[" + debugLabel + "] Trimmed at MaxWiresPerDevice=" + maxWires.ToString() + " (had " + count.ToString() + ")");
                break;
            }

            LFPG_WireData wd = blob.wires[i];

            // v0.7.15: Exhaustive per-wire validation
            if (!ValidateWireData(wd, debugLabel))
            {
                discarded = discarded + 1;
                continue;
            }

            // v0.7.16 H3: O(1) dedup via map
            string dedupKey = wd.m_TargetDeviceId + "|" + wd.m_TargetPort + "|" + wd.m_SourcePort;
            bool exists = false;
            dedupMap.Find(dedupKey, exists);
            if (exists)
            {
                duplicates = duplicates + 1;
                LFPG_Util.Warn("[" + debugLabel + "] Wire discarded: duplicate target=" + wd.m_TargetDeviceId + " port=" + wd.m_TargetPort);
                continue;
            }
            dedupMap.Set(dedupKey, true);

            wires.Insert(wd);
        }

        if (discarded > 0)
        {
            LFPG_Util.Warn("[" + debugLabel + "] Discarded " + discarded.ToString() + " corrupt wires during load");
        }
        if (duplicates > 0)
        {
            LFPG_Util.Warn("[" + debugLabel + "] Removed " + duplicates.ToString() + " duplicate wires during load");
        }
    }

    // Convenience: serialize and return JSON string.
    static string GetJSON(array<ref LFPG_WireData> wires)
    {
        string json;
        SerializeJSON(wires, json);
        return json;
    }
};
