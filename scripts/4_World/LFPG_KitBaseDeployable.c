// =========================================================
// LF_PowerGrid - Kit Base Deployable Class (Fase 4A)
//
// Base class for ~5 different-model kits (DeployableContainer_Base).
// These kits show a hologram of a different model during placement.
//
// Centralizes: IsBasebuildingKit, IsDeployable, CanDisplayCargo,
// CanBePlaced, DoPlacingHeightCheck, GetDeploySoundset,
// GetLoopDeploySoundset, SetActions, GetDeployedClassname,
// GetDeployPositionOffset, GetDeployOrientationOffset,
// and OnPlacementComplete with generic spawn logic.
//
// Subclass contract:
//   - MUST override LFPG_GetSpawnClassname() → entity class to spawn
//   - MAY override GetDeployOrientationOffset() for custom orientation
//     (default: "0 0 0"; Solar uses "0 -90 0")
//
// Spawn standardized to CreateObjectEx(ECE_CREATEPHYSICS) + ObjectDelete.
// (Previously Solar/BatteryLarge used CreateObject+DeleteSafe — legacy.)
//
// NOTE: LF_WaterPump_Kit is NOT migrated (uses DeployableContainer_Base
// super for placement via a different mechanism; works fine as-is).
// =========================================================

class LFPG_KitBaseDeployable : DeployableContainer_Base
{
    // ============================================
    // Virtual: subclass MUST override
    // ============================================
    string LFPG_GetSpawnClassname()
    {
        return "";
    }

    // ============================================
    // DeployableContainer_Base interface
    // (called by hologram system for projection model)
    // ============================================
    string GetDeployedClassname()
    {
        return LFPG_GetSpawnClassname();
    }

    vector GetDeployPositionOffset()
    {
        return "0 0 0";
    }

    vector GetDeployOrientationOffset()
    {
        return "0 0 0";
    }

    // ============================================
    // Standard kit overrides
    // ============================================
    override bool IsBasebuildingKit()
    {
        return true;
    }

    override bool IsDeployable()
    {
        return true;
    }

    override bool CanDisplayCargo()
    {
        return false;
    }

    override bool CanBePlaced(Man player, vector position)
    {
        return true;
    }

    override bool DoPlacingHeightCheck()
    {
        return false;
    }

    override string GetDeploySoundset()
    {
        string snd = "placeBarbedWire_SoundSet";
        return snd;
    }

    override string GetLoopDeploySoundset()
    {
        string empty = "";
        return empty;
    }

    override void SetActions()
    {
        super.SetActions();
        AddAction(ActionTogglePlaceObject);
        AddAction(LFPG_ActionPlaceGeneric);
    }

    // ============================================
    // Generic OnPlacementComplete
    // ============================================
    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        string spawnClass = LFPG_GetSpawnClassname();
        if (spawnClass == "")
        {
            string errEmpty = "[LFPG_KitBaseDeployable] Empty spawn classname on ";
            errEmpty = errEmpty + GetType();
            LFPG_Util.Error(errEmpty);
            return;
        }

        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[";
        tLog = tLog + GetType();
        tLog = tLog + "] OnPlacementComplete: pos=";
        tLog = tLog + finalPos.ToString();
        LFPG_Util.Info(tLog);

        EntityAI device = GetGame().CreateObjectEx(spawnClass, finalPos, ECE_CREATEPHYSICS);
        if (device)
        {
            device.SetPosition(finalPos);
            device.SetOrientation(finalOri);
            device.Update();

            string okLog = "[";
            okLog = okLog + GetType();
            okLog = okLog + "] Deployed ";
            okLog = okLog + spawnClass;
            okLog = okLog + " at ";
            okLog = okLog + finalPos.ToString();
            LFPG_Util.Info(okLog);

            GetGame().ObjectDelete(this);
        }
        else
        {
            string failLog = "[";
            failLog = failLog + GetType();
            failLog = failLog + "] Failed to create ";
            failLog = failLog + spawnClass;
            failLog = failLog + "! Kit preserved.";
            LFPG_Util.Error(failLog);

            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                string errMsg = "[LFPG] Placement failed. Kit preserved.";
                pb.MessageStatus(errMsg);
            }
        }
        #endif
    }
};
