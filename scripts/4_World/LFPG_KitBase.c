// =========================================================
// LF_PowerGrid - Kit Base Class (Fase 4A+4B)
//
// Base class for ~20 same-model kits (Inventory_Base).
// Centralizes: IsDeployable, CanDisplayCargo, CanBePlaced,
// DoPlacingHeightCheck, GetDeploySoundset, GetLoopDeploySoundset,
// SetActions, and OnPlacementComplete with generic spawn logic.
//
// Subclass contract:
//   - MUST override LFPG_GetSpawnClassname() → entity class to spawn
//   - MAY override LFPG_AddPlaceAction() for custom placement action
//     (default: LFPG_ActionPlaceGeneric)
//   - MAY override LFPG_GetPlacementModes() and other placement
//     virtuals for wall/ceiling support (default: floor-only)
//
// Spawn uses CreateObjectEx(ECE_CREATEPHYSICS) + ObjectDelete(kit).
// =========================================================

class LFPG_KitBase : Inventory_Base
{
    // ============================================
    // Virtual: subclass MUST override
    // ============================================
    string LFPG_GetSpawnClassname()
    {
        return "";
    }

    // ============================================
    // Virtual: override for custom placement action
    // (e.g. LogicGate uses LFPG_ActionPlaceLogicGate)
    // ============================================
    void LFPG_AddPlaceAction()
    {
        AddAction(LFPG_ActionPlaceGeneric);
    }

    // ============================================
    // Placement virtuals (Fase 4B)
    // HologramMod calls these via virtual dispatch.
    // Override in subclass for non-default values.
    //
    // Modes: 0=floor, 1=floor+wall, 2=floor+wall+ceiling
    // ============================================
    int LFPG_GetPlacementModes()
    {
        return 0;
    }

    // Wall: offset from surface along normal (metres)
    float LFPG_GetWallSurfaceOffset()
    {
        return 0.03;
    }

    // Wall: pitch offset (degrees)
    float LFPG_GetWallPitchOffset()
    {
        return 0.0;
    }

    // Wall: yaw offset (degrees)
    float LFPG_GetWallYawOffset()
    {
        return 0.0;
    }

    // Wall: roll offset (degrees)
    float LFPG_GetWallRollOffset()
    {
        return 0.0;
    }

    // Global: pitch offset applied in all modes (degrees)
    float LFPG_GetPitchOffset()
    {
        return 0.0;
    }

    // Global: roll offset applied in all modes (degrees)
    float LFPG_GetRollOffset()
    {
        return 0.0;
    }

    // Floor: Y offset above ground (metres)
    float LFPG_GetFloorYOffset()
    {
        return 0.05;
    }

    // Floor: yaw offset (degrees) — rotation around vertical axis
    float LFPG_GetFloorYawOffset()
    {
        return 0.0;
    }

    // Floor: pitch offset (degrees) — tilt forward/backward
    float LFPG_GetFloorPitchOffset()
    {
        return 0.0;
    }

    // Floor: adapt orientation to terrain slope?
    bool LFPG_AdaptsToTerrain()
    {
        return false;
    }

    // ============================================
    // Standard kit overrides (identical across all kits)
    // ============================================
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
        LFPG_AddPlaceAction();
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
            string errEmpty = "[LFPG_KitBase] Empty spawn classname on ";
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
