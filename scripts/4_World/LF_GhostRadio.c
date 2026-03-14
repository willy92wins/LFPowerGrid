// =========================================================
// LF_PowerGrid - Ghost Radio (v3.0.0 - Sprint 3)
//
// Invisible radio entity used by LF_Intercom for bidirectional VOIP.
// Inherits from TransmitterBase (PersonalRadio script class).
//
// Behavior:
//   - CompEM disabled (no battery drain)
//   - No player actions (cannot be picked up, interacted with)
//   - Controlled entirely by LF_Intercom lifecycle:
//     spawn → SwitchOn + EnableBroadcast + EnableReceive + SetFrequency
//     destroy → ObjectDelete when conditions lost
//   - NOT persisted — re-created in AfterStoreLoad if conditions met
//
// Config: scope=1 (not spawnable), Stone.p3d (tiny invisible model)
// =========================================================

class LF_GhostRadio : TransmitterBase
{
    // Give CompEM pseudo-infinite energy so the radio can transmit
    // without a battery. We do NOT SwitchOff — that would prevent
    // TurnOnTransmitter from working. Instead we set energy very high.
    // The ghost is deleted by the intercom when power is lost, so
    // energy drain is irrelevant (ghost lifetime = seconds to hours).
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

    // Block all player interaction
    override void SetActions()
    {
        // Empty — no actions available
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

    // Prevent vanilla inventory operations
    override bool CanReceiveItemIntoCargo(EntityAI item)
    {
        return false;
    }

    override bool CanReceiveAttachment(EntityAI attachment, int slotId)
    {
        return false;
    }
};
