// =========================================================
// LF_PowerGrid - Splitter device (v4.0 Refactor)
//
// LF_Splitter_Kit:  Holdable (same-model deployment).
// LF_Splitter:      PASSTHROUGH, 1 IN + 3 OUT, 0 u/s self-consumption.
//
// v4.0: Migrated from Inventory_Base to LFPG_WireOwnerBase.
//   Wire store, wire API, persistence wireJSON, CanConnectTo — all in base.
// =========================================================

// ---------------------------------------------------------
// KIT (unchanged)
// ---------------------------------------------------------
class LF_Splitter_Kit : Inventory_Base
{
    override bool IsDeployable() { return true; }
    override bool CanDisplayCargo() { return false; }
    override bool CanBePlaced(Man player, vector position) { return true; }
    override bool DoPlacingHeightCheck() { return false; }
    override string GetDeploySoundset() { return "placeBarbedWire_SoundSet"; }
    override string GetLoopDeploySoundset() { return ""; }

    override void SetActions()
    {
        super.SetActions();
        AddAction(ActionTogglePlaceObject);
        AddAction(LFPG_ActionPlaceGeneric);
    }

    override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
    {
        super.OnPlacementComplete(player, position, orientation);

        #ifdef SERVER
        vector finalPos = position;
        vector finalOri = orientation;

        string tLog = "[Splitter_Kit] OnPlacementComplete: param=" + position.ToString();
        tLog = tLog + " kitPos=";
        tLog = tLog + GetPosition().ToString();
        LFPG_Util.Info(tLog);

        EntityAI splitter = GetGame().CreateObjectEx("LF_Splitter", finalPos, ECE_CREATEPHYSICS);
        if (splitter)
        {
            splitter.SetPosition(finalPos);
            splitter.SetOrientation(finalOri);
            splitter.Update();
            string depMsg = "[Splitter_Kit] Deployed at ";
            depMsg = depMsg + finalPos.ToString();
            LFPG_Util.Info(depMsg);
            GetGame().ObjectDelete(this);
        }
        else
        {
            string errKit = "[Splitter_Kit] Failed to create LF_Splitter! Kit preserved.";
            LFPG_Util.Error(errKit);
            PlayerBase pb = PlayerBase.Cast(player);
            if (pb)
            {
                pb.MessageStatus("[LFPG] Splitter placement failed. Kit preserved.");
            }
        }
        #endif
    }
};

// ---------------------------------------------------------
// DEVICE - PASSTHROUGH : LFPG_WireOwnerBase
// ---------------------------------------------------------
class LF_Splitter : LFPG_WireOwnerBase
{
    protected bool m_PoweredNet = false;
    protected bool m_Overloaded = false;

    void LF_Splitter()
    {
        string pIn = "input_1";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, "Input");
        string pO1 = "output_1";
        LFPG_AddPort(pO1, LFPG_PortDir.OUT, "Output 1");
        string pO2 = "output_2";
        LFPG_AddPort(pO2, LFPG_PortDir.OUT, "Output 2");
        string pO3 = "output_3";
        LFPG_AddPort(pO3, LFPG_PortDir.OUT, "Output 3");

        string varP = "m_PoweredNet";
        RegisterNetSyncVariableBool(varP);
        string varO = "m_Overloaded";
        RegisterNetSyncVariableBool(varO);
    }

    override int LFPG_GetDeviceType() { return LFPG_DeviceType.PASSTHROUGH; }
    override float LFPG_GetConsumption() { return 0.0; }
    override bool LFPG_IsSource() { return true; }
    override bool LFPG_GetSourceOn() { return m_PoweredNet; }
    override bool LFPG_IsPowered() { return m_PoweredNet; }

    override void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;
        m_PoweredNet = powered;
        SetSynchDirty();
        string msg = "[LF_Splitter] SetPowered(";
        msg = msg + powered.ToString();
        msg = msg + ") id=";
        msg = msg + m_DeviceId;
        LFPG_Util.Debug(msg);
        #endif
    }

    override bool LFPG_GetOverloaded() { return m_Overloaded; }

    override void LFPG_SetOverloaded(bool val)
    {
        #ifdef SERVER
        if (m_Overloaded != val)
        {
            m_Overloaded = val;
            SetSynchDirty();
        }
        #endif
    }

    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        #endif
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        #endif
    }

    // No extra persistence (PASSTHROUGH: ids + deviceVer + wireJSON from base)
};
