// =========================================================
// LF_PowerGrid - Upgrade Water Pump T1 → T2
//
// v1.1.0: Sprint W1 — ActionContinuousBase with crafting bar.
//   Pattern: LFPG_ActionUpgradeSolarPanel.
//
// Requirements:
//   - Player holds Hammer (item in hands)
//   - Target: LF_WaterPump (T1, NOT T2)
//   - T1 has MetalPlate in slot LF_PumpPlate with qty >= LFPG_PUMP_UPGRADE_PLATES
//   - T1 has Nail in slot LF_PumpNails with qty >= LFPG_PUMP_UPGRADE_NAILS
//
// On completion:
//   1. Capture pos/ori and filter state
//   2. Consume materials (excess dropped to ground)
//   3. DeviceLifecycle.OnDeviceKilled (cuts wires, cleans graph)
//   4. Delete T1
//   5. Create T2 at same pos/ori
//   6. Transfer NBC filter (GasMask_Filter, if any) to T2
//
// ENFORCE SCRIPT NOTES:
//   - No ternary operators
//   - No ++ / --
//   - Explicit typing
//   - No foreach
// =========================================================

class LFPG_ActionUpgradeWaterPumpCB : ActionContinuousBaseCB
{
    override void CreateActionComponent()
    {
        m_ActionData.m_ActionComponent = new CAContinuousTime(8.0);
    }
};

class LFPG_ActionUpgradeWaterPump : ActionContinuousBase
{
    void LFPG_ActionUpgradeWaterPump()
    {
        m_CallbackClass = LFPG_ActionUpgradeWaterPumpCB;
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_CRAFTING;
        m_FullBody = true;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
        m_Text = "#STR_LFPG_ACTION_UPGRADE_PUMP";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem = new CCINonRuined;
        m_ConditionTarget = new CCTCursor(LFPG_INTERACT_DIST_M);
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!player || !target || !item)
            return false;

        if (!item.IsKindOf("Hammer"))
            return false;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return false;

        // Target must be T1 water pump
        LF_WaterPump pump = LF_WaterPump.Cast(targetObj);
        if (!pump)
            return false;

        // Must NOT be T2 already
        if (pump.IsKindOf("LF_WaterPump_T2"))
            return false;

        // Check materials in attachment slots
        EntityAI plate = pump.FindAttachmentBySlotName("LF_PumpPlate");
        if (!plate)
            return false;

        int plateQty = plate.GetQuantity();
        if (plateQty < LFPG_PUMP_UPGRADE_PLATES)
            return false;

        EntityAI nails = pump.FindAttachmentBySlotName("LF_PumpNails");
        if (!nails)
            return false;

        int nailsQty = nails.GetQuantity();
        if (nailsQty < LFPG_PUMP_UPGRADE_NAILS)
            return false;

        return true;
    }

    override void OnFinishProgressServer(ActionData action_data)
    {
        super.OnFinishProgressServer(action_data);

        if (!action_data || !action_data.m_Target)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        LF_WaterPump pump = LF_WaterPump.Cast(targetObj);
        if (!pump)
            return;

        // Re-validate T2 check (anti-exploit)
        if (pump.IsKindOf("LF_WaterPump_T2"))
        {
            LFPG_Util.Warn("[UpgradePump] Target is already T2, aborting.");
            return;
        }

        // ---- Re-validate materials (server authority) ----
        EntityAI plate = pump.FindAttachmentBySlotName("LF_PumpPlate");
        if (!plate)
        {
            LFPG_Util.Warn("[UpgradePump] No MetalPlate found, aborting.");
            return;
        }

        int plateQty = plate.GetQuantity();
        if (plateQty < LFPG_PUMP_UPGRADE_PLATES)
        {
            LFPG_Util.Warn("[UpgradePump] Insufficient MetalPlate qty=" + plateQty.ToString());
            return;
        }

        EntityAI nails = pump.FindAttachmentBySlotName("LF_PumpNails");
        if (!nails)
        {
            LFPG_Util.Warn("[UpgradePump] No Nails found, aborting.");
            return;
        }

        int nailsQty = nails.GetQuantity();
        if (nailsQty < LFPG_PUMP_UPGRADE_NAILS)
        {
            LFPG_Util.Warn("[UpgradePump] Insufficient Nails qty=" + nailsQty.ToString());
            return;
        }

        // ---- Capture transform ----
        vector pos = pump.GetPosition();
        vector ori = pump.GetOrientation();

        // ---- Capture filter state before deletion ----
        int filterQty = 0;
        EntityAI filterItem = pump.FindAttachmentBySlotName("LF_PumpFilter");
        if (filterItem)
        {
            filterQty = filterItem.GetQuantity();
        }

        // ---- Consume materials BEFORE pump deletion ----
        ConsumeMaterial(pump, plate, plateQty, LFPG_PUMP_UPGRADE_PLATES, pos);
        ConsumeMaterial(pump, nails, nailsQty, LFPG_PUMP_UPGRADE_NAILS, pos);

        // ---- Kill T1 device (cuts wires, cleans graph, unregisters) ----
        string deviceId = pump.LFPG_GetDeviceId();
        LFPG_DeviceLifecycle.OnDeviceKilled(pump, deviceId);

        // ---- Surface detection BEFORE T1 deletion ----
        // v1.2.1: Downward raycast to find the actual surface (terrain or
        // building floor) at T1's XZ position. This avoids T2 sinking below
        // ground when its P3D model has a different origin height than T1's.
        // Must run BEFORE ObjectDelete because ObjectDelete is deferred —
        // T1 still exists in physics world during this frame.
        // Pass pump as ignore object so the ray skips the T1 model.
        vector rayFrom = pos;
        rayFrom[1] = rayFrom[1] + 0.5;
        vector rayTo = pos;
        rayTo[1] = rayTo[1] - 1.0;

        vector spawnPos = pos;
        float groundY = 0.0;
        float surfY = 0.0;
        RaycastRVParams surfRay = new RaycastRVParams(rayFrom, rayTo, pump, 0);
        surfRay.sorted = true;
        array<ref RaycastRVResult> surfResults = new array<ref RaycastRVResult>;
        DayZPhysics.RaycastRVProxy(surfRay, surfResults);

        if (surfResults.Count() > 0)
        {
            groundY = surfResults[0].pos[1];
            spawnPos[1] = groundY;
        }
        else
        {
            // Fallback: use engine surface Y (terrain only, no floors)
            surfY = GetGame().SurfaceY(pos[0], pos[2]);
            spawnPos[1] = surfY;
        }

        // ---- Delete T1 (deferred — object persists in physics until end of frame) ----
        GetGame().ObjectDelete(pump);

        EntityAI t2 = GetGame().CreateObjectEx("LF_WaterPump_T2", spawnPos, ECE_CREATEPHYSICS);
        if (t2)
        {
            t2.SetPosition(spawnPos);
            t2.SetOrientation(ori);
            t2.Update();

            // Transfer NBC filter to T2
            if (filterQty > 0)
            {
                EntityAI newFilter = t2.GetInventory().CreateAttachment("GasMask_Filter");
                if (newFilter)
                {
                    ItemBase newFilterItem = ItemBase.Cast(newFilter);
                    if (newFilterItem)
                    {
                        newFilterItem.SetQuantity(filterQty);
                    }
                }
            }

            LFPG_Util.Info("[UpgradePump] T2 created at " + spawnPos.ToString() + " ori=" + ori.ToString() + " (T1 was " + pos.ToString() + ")");
        }
        else
        {
            LFPG_Util.Error("[UpgradePump] Failed to create LF_WaterPump_T2!");
        }
    }

    // Helper: consume exact amount, drop excess to ground
    protected void ConsumeMaterial(EntityAI parent, EntityAI item, int currentQty, int required, vector dropPos)
    {
        if (!item)
            return;

        if (currentQty <= required)
        {
            GetGame().ObjectDelete(item);
        }
        else
        {
            int excessQty = currentQty - required;
            string itemType = item.GetType();

            vector spawnPos = dropPos;
            spawnPos[1] = spawnPos[1] + 0.1;

            EntityAI excess = GetGame().CreateObject(itemType, spawnPos, false);
            if (excess)
            {
                ItemBase excessItem = ItemBase.Cast(excess);
                if (excessItem)
                {
                    excessItem.SetQuantity(excessQty);
                }
            }

            GetGame().ObjectDelete(item);
        }
    }
};
