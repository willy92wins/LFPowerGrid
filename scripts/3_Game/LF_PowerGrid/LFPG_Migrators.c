// =========================================================
// LF_PowerGrid - persistence schema migrators (v0.7.15, Sprint 3 P1)
//
// Chained migration: each migrator transforms data from version N to N+1.
// DeserializeJSON calls MigrateBlob() which applies all needed steps.
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
    // Apply all necessary migrations to a PersistBlob.
    // Called from DeserializeJSON after initial parse.
    // Returns the final version number.
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

        // Apply chained migrations
        if (ver < 2)
        {
            MigrateV1ToV2(blob);
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
            MigrateVanillaV1ToV2(store);
            ver = 2;
        }

        store.ver = ver;
        return ver;
    }

    // ---- v1 → v2: Infrastructure migration ----
    // No schema changes. Establishes the migration chain.
    // Applies sanitization that v1 data may need (NaN, range, dedup).
    // Sanitization is done in ValidateWireData (called by DeserializeJSON),
    // so this migrator just bumps the version.
    protected static void MigrateV1ToV2(LFPG_PersistBlob blob)
    {
        LFPG_Util.Info("[Migrators] Migrating PersistBlob v1 → v2");
        // No field changes — sanitization handled by ValidateWireData
        // Version bump signals that data has passed through v2 sanitization
    }

    protected static void MigrateVanillaV1ToV2(LFPG_VanillaWireStore store)
    {
        LFPG_Util.Info("[Migrators] Migrating VanillaWireStore v1 → v2");
        // No field changes — sanitization handled by ValidateVanillaEntry
    }
};
