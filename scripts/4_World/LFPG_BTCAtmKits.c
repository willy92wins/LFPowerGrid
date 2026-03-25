// =========================================================
// LF_PowerGrid - BTC ATM Kits (Sprint BTC-2)
//
// Both kits use the different-model pattern (KitBaseDeployable):
//   Box model (kit) → deploys ATM model (device).
//
// LF_BTCAtm_Kit:      Player kit → spawns LF_BTCAtm (CONSUMER)
// LF_BTCAtmAdmin_Kit: Admin kit  → spawns LF_BTCAtmAdmin (no power)
//
// Model paths are placeholders — Sprint 5 provides real assets.
// =========================================================

// ---- Player ATM Kit ----
class LF_BTCAtm_Kit : LFPG_KitBaseDeployable
{
    override string LFPG_GetSpawnClassname()
    {
        return "LF_BTCAtm";
    }
};

// ---- Admin ATM Kit ----
class LF_BTCAtmAdmin_Kit : LFPG_KitBaseDeployable
{
    override string LFPG_GetSpawnClassname()
    {
        return "LF_BTCAtmAdmin";
    }
};
