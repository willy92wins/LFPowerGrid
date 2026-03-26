// =========================================================
// LF_PowerGrid - BTC ATM Kits (Sprint BTC-2)
//
// Both kits use the different-model pattern (KitBaseDeployable):
//   Box model (kit) → deploys ATM model (device).
//
// LFPG_BTCAtm_Kit:      Player kit → spawns LFPG_BTCAtm (CONSUMER)
// LFPG_BTCAtmAdmin_Kit: Admin kit  → spawns LFPG_BTCAtmAdmin (no power)
//
// Model paths are placeholders — Sprint 5 provides real assets.
// =========================================================

// ---- Player ATM Kit ----
class LFPG_BTCAtm_Kit : LFPG_KitBaseDeployable
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_BTCAtm";
    }
};

// ---- Admin ATM Kit ----
class LFPG_BTCAtmAdmin_Kit : LFPG_KitBaseDeployable
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_BTCAtmAdmin";
    }
};
