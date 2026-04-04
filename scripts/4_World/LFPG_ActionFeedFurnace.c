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
//   - Ruined items
//   - LFPG_CableReel (wiring tool, not fuel)
//   - Any LFPG kit class (deployment kits)
//   - Items with itemSize 0 in either dimension
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

        // Must not be ruined (manual check — replaces CCINonRuined)
        if (handItem.IsRuined())
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

        // Filter: items with zero-size in either dimension (system items)
        string cfgPath = "CfgVehicles ";
        cfgPath = cfgPath + itemType;
        cfgPath = cfgPath + " itemSize";
        if (GetGame().ConfigIsExisting(cfgPath))
        {
            TIntArray sizeArr = new TIntArray;
            GetGame().ConfigGetIntArray(cfgPath, sizeArr);
            int w = 0;
            int h = 0;
            if (sizeArr.Count() >= 2)
            {
                w = sizeArr[0];
                h = sizeArr[1];
            }
            if (w <= 0 || h <= 0)
                return false;
        }
        else
        {
            // No itemSize config = not a valid item
            return false;
        }

        // v4.7: Check whitelist mode for dynamic text
        LFPG_ServerSettings st = LFPG_Settings.Get();
        bool whitelistMode = st.FurnaceFuelWhitelistOnly;
        bool isWhitelisted = false;

        if (whitelistMode)
        {
            int burnSec = LFPG_Settings.GetWhitelistFuelSec(itemType);
            if (burnSec > 0)
            {
                isWhitelisted = true;
            }
        }

        int fuelCur = furnace.LFPG_GetFuelCurrent();
        int fuelMax = LFPG_FURNACE_MAX_FUEL;

        // Non-whitelisted items in whitelist mode: always allowed (burn for nothing)
        // Whitelisted items and normal mode: check fuel full
        if (whitelistMode && !isWhitelisted)
        {
            // Show "Burn Item (no fuel)" text
            string burnLabel = Widget.TranslateString("#STR_LFPG_ACTION_BURN_ITEM_NO_FUEL");
            m_Text = burnLabel;
        }
        else
        {
            // Furnace must not be full for items that give fuel
            if (fuelCur >= fuelMax)
                return false;

            // Dynamic text: show fuel percentage
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
        }

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

        // Re-validate: item could have changed between condition check and execute
        if (feedItem.IsRuined())
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

        // If item gives fuel, check overflow
        if (fuelToAdd > 0)
        {
            int fuelCur = furnace.LFPG_GetFuelCurrent();
            int fuelAfter = fuelCur + fuelToAdd;
            if (fuelAfter > LFPG_FURNACE_MAX_FUEL)
            {
                string fullMsg = "[LFPG] Furnace fuel full. Item preserved.";
                pb.MessageStatus(fullMsg);
                return;
            }

            furnace.LFPG_AddFuel(fuelToAdd);
        }

        // Destroy item (+ all contents recursively via engine)
        GetGame().ObjectDelete(feedItem);

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
