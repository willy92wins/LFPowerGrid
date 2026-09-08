// =========================================================
// LF_PowerGrid - persistence schema migrators (v0.7.15, Sprint 3 P1)
//
// Legacy version helpers: apply the v1 -> v2 bump without changing wire fields.
// Newer schema versions are returned unchanged; validation belongs to loaders.
//
// Migration chain:
//   v1 (original) → v2 (Sprint 3: infrastructure + sanitization)
//   v2 → v3 (Sprint 4: reserved for graph model fields)
//
// =========================================================
// COMPATIBILITY STRATEGY for Sprint 4 (grafo):
//
// The electrical graph is an IN-MEMORY structure reconstructed from wires
// on server startup. Wires remain the persisted source of truth.
//
// Sprint 4 may add optional fields to LFPG_WireData:
//   - m_Priority (int): wire priority for load allocation (default: 0)
//   - m_Flags (int): bitfield for future states (default: 0)
//
// Migrator v2→v3 will:
//   - Set m_Priority = 0 for all existing wires (preserves behavior)
//   - Set m_Flags = 0 for all existing wires
//
// Rollback policy:
//   - v3 data loaded by v2 code: unknown fields ignored by JsonFileLoader
//   - v2 data loaded by v3 code: migrator fills defaults
//   - v1 data loaded by any version: migrator chain applies (v1→v2→v3)
//
// What is persisted vs reconstructed:
//   PERSISTED: wire endpoints, ports, waypoints, creator, priority, flags
//   RECONSTRUCTED: graph topology, power states, load ratios, overload masks
// =========================================================

class LFPG_Migrators
{
	// Apply the legacy version bump without validating or rewriting wire fields.
	// Returns the version; newer schemas are left unchanged.
    static int MigrateBlob(LFPG_PersistBlob blob)
    {
        if (!blob)
            return LFPG_PERSIST_VER;

        int ver = blob.ver;

        // Unknown future version: load tolerantly, don't migrate down
        if (ver > LFPG_PERSIST_VER)
        {
            LFPG_Util.Warn("[Migrators] Blob ver " + ver.ToString() + " > current " + LFPG_PERSIST_VER.ToString() + ". Loading tolerantly (no downgrade).");
            return ver;
        }

		// v1 -> v2 changes only the version; loaders validate wire data.
        if (ver < 2)
        {
			LFPG_Util.Info("[Migrators] Migrating PersistBlob v1 → v2");
            ver = 2;
        }

        // Future: if (ver < 3) { MigrateV2ToV3(blob); ver = 3; }

        blob.ver = ver;
        return ver;
    }

    // Apply all necessary migrations to a VanillaWireStore.
    static int MigrateVanillaStore(LFPG_VanillaWireStore store)
    {
        if (!store)
            return LFPG_VANILLA_PERSIST_VER;

        int ver = store.ver;

        if (ver > LFPG_VANILLA_PERSIST_VER)
        {
            LFPG_Util.Warn("[Migrators] VanillaStore ver " + ver.ToString() + " > current " + LFPG_VANILLA_PERSIST_VER.ToString() + ". Loading tolerantly.");
            return ver;
        }

        if (ver < 2)
        {
			LFPG_Util.Info("[Migrators] Migrating VanillaWireStore v1 → v2");
            ver = 2;
        }

        store.ver = ver;
        return ver;
    }
};
