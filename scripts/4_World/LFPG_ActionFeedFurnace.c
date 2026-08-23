// =========================================================
// LF_PowerGrid - Action: Feed Furnace (v1.2.2)
//
// Destroys the item in hand and converts it to fuel.
// Fuel = inventory squares calculated recursively
// (item + cargo + attachments at any depth).
//
// Base: ActionInteractBase (CCINone — no item restriction)
//   DayZ only evaluates ActionSingleUseBase from the ITEM's
//   action list, not the target's. Since Feed is registered
//   on LFPG_Furnace (the target), it MUST be ActionInteractBase
//   so the ActionManager finds it in the target's interact list.
//   Item-in-hand is resolved manually via player.GetItemInHands().
//
// Target: LFPG_Furnace
// v1.2.2: CCTObject→CCTCursor + manual DistSq (fixes interaction
//         reliability on small Geometry LOD models).
//
// Filtered items (ActionCondition rejects):
//   - Empty hands (no item)
//   - LFPG_CableReel (wiring tool, not fuel)
//   - Any LFPG kit class (deployment kits)
//
// Fuel valuation and capacity clamping are authoritative on the server.
// Ruined and modded items without itemSize remain valid incinerator input;
// fuel that exceeds capacity is discarded rather than rejecting the item.
// This also avoids client/server divergence because the full furnace
// whitelist is intentionally not synchronized to clients.
//
// IMPORTANTE: Registrar en ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionFeedFurnace).
// =========================================================

class LFPG_ActionFeedFurnace : ActionInteractBase
{
    void LFPG_ActionFeedFurnace()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_FEED_FURNACE";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem   = new CCINone;
        m_ConditionTarget = new CCTCursor(LFPG_INTERACT_DIST_M);
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!player)
            return false;

        if (!target)
            return false;

        // Resolve item from player hands (ActionInteractBase does not
        // populate the item param reliably for target-registered actions)
        ItemBase handItem = player.GetItemInHands();
        if (!handItem)
            return false;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return false;

        // Target must be LFPG_Furnace
        LFPG_Furnace furnace = LFPG_Furnace.Cast(targetObj);
        if (!furnace)
            return false;

        // Manual proximity check (CCTCursor does not enforce distance by type)
        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), furnace.GetPosition());
        float maxSq = LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M;
        if (distSq > maxSq)
            return false;

        // Filter: CableReel is a wiring tool, not fuel
        if (handItem.IsKindOf("LFPG_CableReel"))
            return false;

        // Filter: LFPG kit items (Splitter_Kit, SolarPanel_Kit, etc.)
        string itemType = handItem.GetType();
        if (LFPG_IsLFPGKit(itemType))
            return false;

        // The server owns whitelist evaluation. Clients receive neither the
        // whitelist nor its per-item burn values, so using local settings here
        // made the action disappear for items the server would accept.
        int fuelCur = furnace.LFPG_GetFuelCurrent();
        int fuelMax = LFPG_FURNACE_MAX_FUEL;
        float feedPctF = 0.0;
        if (fuelMax > 0)
        {
            feedPctF = (fuelCur * 100.0) / fuelMax;
        }
        int feedPctWhole = Math.Floor(feedPctF);
        float feedPctFrac = feedPctF - feedPctWhole;
        int feedPctTenths = Math.Round(feedPctFrac * 10.0);
        if (feedPctTenths >= 10)
        {
            feedPctWhole = feedPctWhole + 1;
            feedPctTenths = 0;
        }
        string feedLabel = Widget.TranslateString("#STR_LFPG_ACTION_FEED_FURNACE");
        string feedPct = " (" + feedPctWhole.ToString() + "." + feedPctTenths.ToString() + "%)";
        m_Text = feedLabel + feedPct;

        return true;
    }

    override void OnExecuteServer(ActionData action_data)
    {
        super.OnExecuteServer(action_data);

        if (!action_data)
            return;

        if (!action_data.m_Target)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        LFPG_Furnace furnace = LFPG_Furnace.Cast(targetObj);
        if (!furnace)
            return;

        // Resolve item from player hands (ActionInteractBase does not
        // set m_MainItem for target-registered actions)
        PlayerBase pb = PlayerBase.Cast(action_data.m_Player);
        if (!pb)
            return;

        ItemBase feedItem = pb.GetItemInHands();
        if (!feedItem)
            return;

        // Re-validate protected items: hands can change between condition and execute.
        if (feedItem.IsKindOf("LFPG_CableReel"))
            return;
        string revalType = feedItem.GetType();
        if (LFPG_IsLFPGKit(revalType))
            return;

        // v4.7: Calculate fuel based on mode
        LFPG_ServerSettings st = LFPG_Settings.Get();
        bool whitelistMode = st.FurnaceFuelWhitelistOnly;
        int fuelToAdd = 0;

        if (whitelistMode)
        {
            fuelToAdd = furnace.LFPG_CalcFuelWhitelist(feedItem);
        }
        else
        {
            fuelToAdd = furnace.LFPG_CalcFuelRecursive(feedItem);
        }

        // Non-whitelist mode: reject items with 0 fuel (preserve item)
        if (!whitelistMode && fuelToAdd <= 0)
        {
            string noFuelMsg = "[LFPG] Item has no fuel value.";
            pb.MessageStatus(noFuelMsg);
            return;
        }

        int fuelAccepted = 0;
        if (fuelToAdd > 0)
        {
            // Saturating addition: an incinerator consumes the item even when
            // only part (or none) of its fuel value fits in the reservoir.
            fuelAccepted = furnace.LFPG_AddFuel(fuelToAdd);
        }

        // Destroy item (+ all contents recursively via engine)
        g_Game.ObjectDelete(feedItem);

        // Log
        string playerName = "unknown";
        PlayerIdentity identity = pb.GetIdentity();
        if (identity)
        {
            playerName = identity.GetName();
        }
        string logMsg = "[ActionFeedFurnace] Player=";
        logMsg = logMsg + playerName;
        logMsg = logMsg + " fed +";
        logMsg = logMsg + fuelAccepted.ToString();
        logMsg = logMsg + "/";
        logMsg = logMsg + fuelToAdd.ToString();
        logMsg = logMsg + " fuel. total=";
        logMsg = logMsg + furnace.LFPG_GetFuelCurrent().ToString();
        LFPG_Util.Info(logMsg);
    }

    // ---- Helper: check if item type is an LFPG kit ----
    protected bool LFPG_IsLFPGKit(string typeName)
    {
        // Check ALL known LFPG kit types (must match config.cpp units[])
        if (typeName == "LFPG_Splitter_Kit")
            return true;
        if (typeName == "LFPG_CeilingLight_Kit")
            return true;
        if (typeName == "LFPG_WallLamp_Kit")
            return true;
        if (typeName == "LFPG_SolarPanel_Kit")
            return true;
        if (typeName == "LFPG_Combiner_Kit")
            return true;
        if (typeName == "LFPG_Camera_Kit")
            return true;
        if (typeName == "LFPG_Monitor_Kit")
            return true;
        if (typeName == "LFPG_WaterPump_Kit")
            return true;
        if (typeName == "LFPG_Furnace_Kit")
            return true;
        if (typeName == "LFPG_PushButton_Kit")
            return true;
        if (typeName == "LFPG_Sorter_Kit")
            return true;
        if (typeName == "LFPG_Searchlight_Kit")
            return true;
        if (typeName == "LFPG_SwitchV2_Kit")
            return true;
        if (typeName == "LFPG_MotionSensor_Kit")
            return true;
        if (typeName == "LFPG_PressurePad_Kit")
            return true;
        if (typeName == "LFPG_AND_Gate_Kit")
            return true;
        if (typeName == "LFPG_OR_Gate_Kit")
            return true;
        if (typeName == "LFPG_XOR_Gate_Kit")
            return true;
        if (typeName == "LFPG_LaserDetector_Kit")
            return true;
        if (typeName == "LFPG_ElectronicCounter_Kit")
            return true;
        if (typeName == "LFPG_BatteryMedium_Kit")
            return true;

        return false;
    }
};
