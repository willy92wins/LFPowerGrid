// =========================================================
// LF_PowerGrid - Action: Feed Furnace (v1.2.0)
//
// Destroys the item in hand and converts it to fuel.
// Fuel = inventory squares calculated recursively
// (item + cargo + attachments at any depth).
//
// Base: ActionSingleUseBase (item in hand, CCINonRuined)
// Target: LF_Furnace
// Registered on TARGET (LF_Furnace.SetActions) so that
// ANY non-ruined item in hand triggers the action.
//
// Filtered items (ActionCondition rejects):
//   - LF_CableReel (wiring tool, not fuel)
//   - Any class containing "_Kit" from LFPG (deployment kits)
//   - Items with itemSize 0 in either dimension
//
// IMPORTANTE: Registrar en ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionFeedFurnace).
// =========================================================

class LFPG_ActionFeedFurnace : ActionSingleUseBase
{
    void LFPG_ActionFeedFurnace()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_FEED_FURNACE";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem   = new CCINonRuined;
        m_ConditionTarget = new CCTCursor(LFPG_INTERACT_DIST_M);
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!player)
            return false;

        if (!target)
            return false;

        if (!item)
            return false;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return false;

        // Target must be LF_Furnace
        LF_Furnace furnace = LF_Furnace.Cast(targetObj);
        if (!furnace)
            return false;

        // Distance check
        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), furnace.GetPosition());
        float maxDistSq = LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M;
        if (distSq > maxDistSq)
            return false;

        // Filter: CableReel is a wiring tool, not fuel
        if (item.IsKindOf("LF_CableReel"))
            return false;

        // Filter: LFPG kit items (Splitter_Kit, SolarPanel_Kit, etc.)
        string itemType = item.GetType();
        if (LFPG_IsLFPGKit(itemType))
            return false;

        // Filter: items with zero-size in either dimension
        string cfgPath = "CfgVehicles " + itemType + " itemSize";
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
            // No itemSize config = can't calculate fuel value
            return false;
        }

        // Furnace must not be full
        int fuelCur = furnace.LFPG_GetFuelCurrent();
        if (fuelCur >= LFPG_FURNACE_MAX_FUEL)
            return false;

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

        LF_Furnace furnace = LF_Furnace.Cast(targetObj);
        if (!furnace)
            return;

        ItemBase feedItem = action_data.m_MainItem;
        if (!feedItem)
            return;

        // Calculate fuel recursively (item + all contents)
        int fuelToAdd = furnace.LFPG_CalcFuelRecursive(feedItem);

        if (fuelToAdd <= 0)
        {
            PlayerBase pb = PlayerBase.Cast(action_data.m_Player);
            if (pb)
            {
                pb.MessageStatus("[LFPG] Item has no fuel value.");
            }
            return;
        }

        // Check overflow: reject if would exceed max
        int fuelCur = furnace.LFPG_GetFuelCurrent();
        int fuelAfter = fuelCur + fuelToAdd;
        if (fuelAfter > LFPG_FURNACE_MAX_FUEL)
        {
            PlayerBase pbFull = PlayerBase.Cast(action_data.m_Player);
            if (pbFull)
            {
                pbFull.MessageStatus("[LFPG] Furnace fuel full. Item preserved.");
            }
            return;
        }

        // Add fuel
        furnace.LFPG_AddFuel(fuelToAdd);

        // Destroy item (+ all contents recursively via engine)
        GetGame().ObjectDelete(feedItem);

        // Log
        PlayerBase pbLog = PlayerBase.Cast(action_data.m_Player);
        string playerName = "unknown";
        if (pbLog)
        {
            PlayerIdentity identity = pbLog.GetIdentity();
            if (identity)
            {
                playerName = identity.GetName();
            }
        }
        LFPG_Util.Info("[ActionFeedFurnace] Player=" + playerName + " fed +" + fuelToAdd.ToString() + " fuel. total=" + fuelAfter.ToString());
    }

    // ---- Helper: check if item type is an LFPG kit ----
    protected bool LFPG_IsLFPGKit(string typeName)
    {
        // Check known LFPG kit patterns
        if (typeName == "LF_Splitter_Kit")
            return true;
        if (typeName == "LF_CeilingLight_Kit")
            return true;
        if (typeName == "LF_SolarPanel_Kit")
            return true;
        if (typeName == "LF_Combiner_Kit")
            return true;
        if (typeName == "LF_Camera_Kit")
            return true;
        if (typeName == "LF_Monitor_Kit")
            return true;
        if (typeName == "LF_WaterPump_Kit")
            return true;
        if (typeName == "LF_Furnace_Kit")
            return true;
        if (typeName == "LFPG_PushButton_Kit")
            return true;

        return false;
    }
};
