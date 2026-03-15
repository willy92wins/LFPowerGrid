// =========================================================
// LF_PowerGrid - Ghost Radio (v3.0.0)
//
// Invisible radio entity used by LF_Intercom for bidirectional VOIP.
// Inherits from TransmitterBase (script) / PersonalRadio (config).
//
// Config inherits PersonalRadio so the C++ engine creates an
// ItemTransmitter object with native radio methods:
//   EnableBroadcast(), EnableReceive(), SetFrequencyByIndex(),
//   GetTunedFrequencyIndex(), SwitchOn()
//
// Activation flow:
//   1. EEInit: SetEnergy(9999) — pseudo-infinite power
//   2. Intercom calls GetCompEM().SwitchOn() on the ghost
//   3. Engine checks CanWork() → true (has energy)
//   4. Engine calls OnWorkStart() → we enable broadcast+receive
//   5. Intercom calls SetFrequencyByIndex() separately
//
// OnWorkStart overridden to suppress vanilla radio static sound.
// OnWorkStop overridden to suppress vanilla radio sound stop.
//
// NOT persisted — re-created in AfterStoreLoad if conditions met.
// Config: scope=1 (not spawnable), Stone.p3d (tiny invisible model).
// =========================================================

class LF_GhostRadio : TransmitterBase
{
    // Give CompEM pseudo-infinite energy so CanWork() returns true.
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

    // Override OnWorkStart: enable broadcast+receive but NO vanilla sound.
    // Vanilla TransmitterBase.OnWorkStart() calls SoundTurnedOnNoiseStart()
    // which plays static noise — we skip that entirely.
    override void OnWorkStart()
    {
        EnableBroadcast(true);
        EnableReceive(true);
        SwitchOn(true);
        // No SoundTurnedOnNoiseStart() — ghost is silent
    }

    // Override OnWorkStop: disable broadcast+receive, no sound cleanup.
    override void OnWorkStop()
    {
        GetCompEM().SwitchOff();
        EnableBroadcast(false);
        EnableReceive(false);
        SwitchOn(false);
        // No SoundTurnedOnNoiseStop() — ghost has no sound to stop
    }

    // Block all player interaction — no actions available
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

    override bool IsElectricAppliance()
    {
        return false;
    }

    override bool CanReceiveItemIntoCargo(EntityAI item)
    {
        return false;
    }

    override bool CanReceiveAttachment(EntityAI attachment, int slotId)
    {
        return false;
    }
};
