// =========================================================
// LF_PowerGrid - wiring tool item
// =========================================================

class LFPG_CableReel : CableReel
{
    protected static bool s_SetActionsLogged;

    // CableReel vanilla:
    //  - IsElectricAppliance() == true
    //  - Adds plug + placement actions in CableReel.SetActions()
    //  - Implements OnPlacementStarted() and drives hologram selections
    //
    // LF_PowerGrid: this item is ONLY a wiring tool for our system.
    // It must never start vanilla plug/placement/hologram logic.
    override bool IsDeployable()
    {
        return false;
    }

    // Ensure vanilla EM workflow can't treat it like a cable reel
    override bool IsElectricAppliance()
    {
        return false;
    }

    // Belt-and-suspenders: even if some external mod tries to start placement,
    // do nothing (avoid hologram code paths).
    override void OnPlacementStarted(Man player)
    {
        // Intentionally empty.
    }

    override void SetActions()
    {
        // DO NOT call super.SetActions() (CableReel adds plug + placement actions).

        // v0.7.38: Restore vanilla take actions so player can pick up the reel.
        AddAction(ActionTakeItem);
        AddAction(ActionTakeItemToHands);

        // Per-port actions (max 7 ports per device)
        AddAction(ActionLFPG_Port0);
        AddAction(ActionLFPG_Port1);
        AddAction(ActionLFPG_Port2);
        AddAction(ActionLFPG_Port3);
        AddAction(ActionLFPG_Port4);
        AddAction(ActionLFPG_Port5);
        AddAction(ActionLFPG_Port6);

        // Session actions (terrain)
        AddAction(ActionLFPG_PlaceWaypoint);
        AddAction(ActionLFPG_CancelWiring);

        // Utility
        AddAction(ActionLFPG_DebugStatus);

        if (!s_SetActionsLogged)
        {
            s_SetActionsLogged = true;
            LFPG_Util.Info("[Items] LFPG_CableReel.SetActions v" + LFPG_VERSION_STR + " (12 actions)");
        }
    }

    override void EEInit()
    {
        super.EEInit();
        LFPG_Bootstrap.LogOnce("LFPG_CableReel EEInit reached -> script class active");
    }
};
