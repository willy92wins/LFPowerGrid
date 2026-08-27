// =========================================================
// LF_PowerGrid - Upgrade Water Pump T1 → T2
//
// v1.1.0: Sprint W1 — ActionContinuousBase with crafting bar.
//   Pattern: LFPG_ActionUpgradeSolarPanel.
//
// Requirements:
//   - Player holds Hammer (item in hands)
//   - Target: LFPG_WaterPump (T1, NOT T2)
//   - T1 has MetalPlate in slot LFPG_PumpPlate with qty >= LFPG_PUMP_UPGRADE_PLATES
//   - T1 has Nail in slot LFPG_PumpNails with qty >= LFPG_PUMP_UPGRADE_NAILS
//
// On completion:
//   1. Capture pos/ori and filter state
//   2. Create and validate T2 above T1, then align its base to T1's base
//   3. Consume materials (excess dropped to ground)
//   4. DeviceLifecycle.OnDeviceKilled (cuts wires, cleans graph)
//   5. Delete T1
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
    // T2 must exist before T1 materials are consumed so the upgrade remains
    // rollback-safe. Stage it above the old pump, then move it into the
    // vacated position after T1's physics body has been removed.
    static const float PUMP_UPGRADE_STAGING_HEIGHT_M = 2.0;
    static const int PUMP_UPGRADE_FINALIZE_DELAY_MS = 50;

    // The T2 P3D extends farther below its object origin than the T1 P3D.
    // Visual LOD minima are -0.929 m (T2) and -0.587 m (T1), so copying the
    // T1 origin directly buries T2 by their 0.342 m difference.
    static const float PUMP_T2_BASE_ALIGNMENT_Y_M = 0.342;

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
        LFPG_WaterPump pump = LFPG_WaterPump.Cast(targetObj);
        if (!pump)
            return false;

        // Must NOT be T2 already
        if (pump.IsKindOf("LFPG_WaterPump_T2"))
            return false;

        // Check materials in attachment slots
        EntityAI plate = pump.FindAttachmentBySlotName("LFPG_PumpPlate");
        if (!plate)
            return false;

        int plateQty = plate.GetQuantity();
        if (plateQty < LFPG_PUMP_UPGRADE_PLATES)
            return false;

        EntityAI nails = pump.FindAttachmentBySlotName("LFPG_PumpNails");
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
    
        LFPG_WaterPump pump = LFPG_WaterPump.Cast(targetObj);
        if (!pump)
            return;
    
        // Revalidate the target and material references before claiming the operation.
        if (pump.IsKindOf("LFPG_WaterPump_T2"))
        {
            LFPG_Util.Warn("[UpgradePump] Target is already T2, aborting.");
            return;
        }
    
        EntityAI plate = pump.FindAttachmentBySlotName("LFPG_PumpPlate");
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
    
        EntityAI nails = pump.FindAttachmentBySlotName("LFPG_PumpNails");
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
    
        vector pos = pump.GetPosition();
        vector ori = pump.GetOrientation();
        string deviceId = pump.LFPG_GetDeviceId();
    
        int filterQty = 0;
        string filterSlot = "GasMaskFilter";
        EntityAI filterItem = pump.FindAttachmentBySlotName(filterSlot);
        if (filterItem)
            filterQty = filterItem.GetQuantity();
    
        if (!pump.LFPG_TryBeginExclusiveOp())
        {
            LFPG_Util.Warn("[UpgradePump] Another destructive operation already owns the target.");
            return;
        }

        vector stagingPos = pos;
        stagingPos[1] = stagingPos[1] + PUMP_UPGRADE_STAGING_HEIGHT_M;
        EntityAI t2 = EntityAI.Cast(g_Game.CreateObjectEx("LFPG_WaterPump_T2", stagingPos, ECE_CREATEPHYSICS));
        if (!t2)
        {
            pump.LFPG_EndExclusiveOp();
            LFPG_Util.Error("[UpgradePump] Failed to create LFPG_WaterPump_T2 - aborting upgrade, T1 + materials preserved");
            return;
        }
    
        t2.SetPosition(stagingPos);
        t2.SetOrientation(ori);
        t2.Update();
    
        array<EntityAI> stagedOutputs = new array<EntityAI>();
        stagedOutputs.Insert(t2);
    
        // Stage the replacement filter before any T1 attachment can be deleted.
        if (filterQty > 0)
        {
            EntityAI newFilter = t2.GetInventory().CreateAttachment("GasMask_Filter");
            if (!newFilter)
            {
                g_Game.ObjectDelete(t2);
                pump.LFPG_EndExclusiveOp();
                LFPG_Util.Error("[UpgradePump] Failed to create T2 filter - T1 + original filter preserved");
                return;
            }
    
            ItemBase newFilterItem = ItemBase.Cast(newFilter);
            if (!newFilterItem)
            {
                g_Game.ObjectDelete(t2);
                pump.LFPG_EndExclusiveOp();
                LFPG_Util.Error("[UpgradePump] Invalid T2 filter type - T1 + original filter preserved");
                return;
            }
            newFilterItem.SetQuantity(filterQty);
        }
    
        if (!StageMaterialSurplus(plate, plateQty, LFPG_PUMP_UPGRADE_PLATES, pos, stagedOutputs))
        {
            AbortStagedUpgradeOutputs(stagedOutputs);
            pump.LFPG_EndExclusiveOp();
            LFPG_Util.Error("[UpgradePump] Failed to stage MetalPlate surplus - T1 + materials preserved");
            return;
        }
        if (!StageMaterialSurplus(nails, nailsQty, LFPG_PUMP_UPGRADE_NAILS, pos, stagedOutputs))
        {
            AbortStagedUpgradeOutputs(stagedOutputs);
            pump.LFPG_EndExclusiveOp();
            LFPG_Util.Error("[UpgradePump] Failed to stage Nail surplus - T1 + materials preserved");
            return;
        }
        // Commit sources only after T2, its filter, and every surplus output exist.
        g_Game.ObjectDelete(plate);
        g_Game.ObjectDelete(nails);
        LFPG_DeviceLifecycle.OnDeviceKilled(pump, deviceId);
        g_Game.ObjectDelete(pump);

        // ObjectDelete is deferred. Moving T2 next tick guarantees that its
        // physics body never overlaps the old T1 body at the target position.
        // Raise the final origin by the models' base-height difference so the
        // replacement preserves T1's ground contact instead of sinking.
        vector finalPos = pos;
        finalPos[1] = finalPos[1] + PUMP_T2_BASE_ALIGNMENT_Y_M;
        bool finalizeOnce = false;
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(FinalizeT2Placement, PUMP_UPGRADE_FINALIZE_DELAY_MS, finalizeOnce, t2, finalPos, ori);
    }

    protected static void FinalizeT2Placement(EntityAI t2, vector finalPos, vector finalOri)
    {
        if (!t2)
        {
            LFPG_Util.Error("[UpgradePump] T2 vanished before final placement.");
            return;
        }

        t2.SetPosition(finalPos);
        t2.SetOrientation(finalOri);
        t2.Update();
        LFPG_Util.Info("[UpgradePump] T2 finalized at " + finalPos.ToString() + " ori=" + finalOri.ToString());
    }

    // Helpers stage every excess output before source materials are consumed.
    protected bool StageMaterialSurplus(EntityAI item, int currentQty, int required, vector dropPos, array<EntityAI> stagedOutputs)
    {
        if (!item || !stagedOutputs)
            return false;
        if (currentQty <= required)
            return true;
    
        int excessQty = currentQty - required;
        string itemType = item.GetType();
        vector spawnPos = dropPos;
        spawnPos[1] = spawnPos[1] + 0.1;
    
        EntityAI excess = EntityAI.Cast(g_Game.CreateObject(itemType, spawnPos, false));
        if (!excess)
            return false;
    
        stagedOutputs.Insert(excess);
        ItemBase excessItem = ItemBase.Cast(excess);
        if (!excessItem)
            return false;
    
        excessItem.SetQuantity(excessQty);
        return true;
    }
    
    protected void AbortStagedUpgradeOutputs(array<EntityAI> stagedOutputs)
    {
        if (!stagedOutputs)
            return;
    
        int i = 0;
        for (i = 0; i < stagedOutputs.Count(); i = i + 1)
        {
            EntityAI output = stagedOutputs[i];
            if (output)
                g_Game.ObjectDelete(output);
        }
        stagedOutputs.Clear();
    }
};
