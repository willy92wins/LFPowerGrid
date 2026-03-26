// =========================================================
// LF_PowerGrid - Ghost PAS Broadcaster (v1.0.0)
//
// Invisible PAS broadcaster entity used by LFPG_Intercom T2
// for megaphone audio. Inherits Land_Radio_PanelPAS in config
// so the C++ engine creates a PASBroadcaster object with
// native proto methods: SwitchOn(bool), IsOn().
//
// When active, the engine captures voice from players near
// this entity and routes it to ALL PASReceiver entities in
// the world (vanilla amp poles + LFPG_Speaker ghosts).
//
// Activation flow (driven by Intercom):
//   1. EEInit: SetEnergy(9999) — pseudo-infinite power
//   2. Intercom calls GetCompEM().SwitchOn() on the ghost
//   3. Engine checks CanWork() → true (has energy)
//   4. Engine calls OnWorkStart() → SwitchOn(true) → PAS active
//
// All sounds suppressed. All player interaction blocked.
// NOT persisted — re-created by Intercom in AfterStoreLoad.
// Config: scope=1 (not spawnable), Stone.p3d (invisible).
// =========================================================

class LFPG_GhostPASBroadcaster : Land_Radio_PanelPAS
{
    override void EEInit()
    {
        super.EEInit();

        #ifdef SERVER
        ComponentEnergyManager cem = GetCompEM();
        if (cem)
        {
            cem.SetEnergy(9999);
        }
        #endif
    }

    // Suppress vanilla PAS panel sounds on switch on
    override void OnSwitchOn()
    {
        if (!GetCompEM().CanWork())
        {
            GetCompEM().SwitchOff();
        }
        // No SoundTurnOn() — ghost is silent
    }

    // Suppress vanilla PAS panel sounds on switch off
    override void OnSwitchOff()
    {
        // No SoundTurnOff() — ghost is silent
    }

    // Start PAS broadcast, no sound
    override void OnWorkStart()
    {
        SwitchOn(true);
        // No super — skip vanilla sound + redundant SwitchOn
        // No SoundTurnedOnNoiseStart() — ghost is silent
    }

    // Stop PAS broadcast, no sound
    override void OnWorkStop()
    {
        SwitchOn(false);
        // No super — skip vanilla CompEM.SwitchOff + sound
        // No SoundTurnedOnNoiseStop() — ghost is silent
    }

    // Block all player interaction
    override void SetActions()
    {
    }

    override bool CanPutInCargo(EntityAI parent)
    {
        return false;
    }

    override bool CanPutIntoHands(EntityAI parent)
    {
        return false;
    }

    override bool IsInventoryVisible()
    {
        return false;
    }

    override bool DisableVicinityIcon()
    {
        return true;
    }

    override bool CanReceiveItemIntoCargo(EntityAI item)
    {
        return false;
    }

    override bool CanReceiveAttachment(EntityAI attachment, int slotId)
    {
        return false;
    }

    override bool IsElectricAppliance()
    {
        return false;
    }
};
