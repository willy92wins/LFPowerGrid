// =========================================================
// LF_PowerGrid - Ghost PAS Receiver (v1.0.0)
//
// Invisible PAS receiver entity used by LFPG_Speaker.
// Inherits PASReceiver in config so the C++ engine routes
// PAS broadcast audio to this entity's world position.
//
// No CompEM needed — PASReceiver is passive. The engine
// detects entities of this class and plays PAS audio at
// their position automatically.
//
// Lifecycle driven by LFPG_Speaker:
//   - Speaker powered + on → spawn ghost → audio plays here
//   - Speaker off or unpowered → destroy ghost → silence
//
// All player interaction blocked.
// NOT persisted — re-created by Speaker in AfterStoreLoad.
// Config: scope=1 (not spawnable), Stone.p3d (invisible).
// =========================================================

class LFPG_GhostPASReceiver : PASReceiver
{
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
