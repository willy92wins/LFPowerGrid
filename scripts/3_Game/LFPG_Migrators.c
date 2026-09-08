// =========================================================
// LF_PowerGrid - persistence compatibility note
//
// Runtime migrators were retired: LFPG_WireHelper.DeserializeJSON loads
// LFPG_PersistBlob directly and validates individual wires. The unused
// MigrateBlob / MigrateVanillaStore version-only helpers are removed.
// This file stays because LFPG_Defines.c points here for compatibility.
//
// No persisted fields, version constants or CfgVehicles classes change.
// There is no active migration chain here and no automatic v1 -> v2 rewrite.
// Future-version policy belongs to each loader; this note does not promise
// that an older deserializer can safely read a newer schema.
// =========================================================
