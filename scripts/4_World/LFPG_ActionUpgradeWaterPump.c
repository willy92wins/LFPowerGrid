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
//   2. Drop T1 physics, then create T2 with physics at the same pos/ori
//   3. Stage material surplus with vanilla properties preserved
//   4. Move the original filter to T2 after all output creation succeeds
//   5. Consume originals, cut wires and delete T1
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
    
		string filterSlot = "GasMaskFilter";
		EntityAI filterItem = pump.FindAttachmentBySlotName(filterSlot);
    
        // Resolve the T2 surface position while T1 is still in the physics world.
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
            surfY = g_Game.SurfaceY(pos[0], pos[2]);
            spawnPos[1] = surfY;
        }
    
        if (!pump.LFPG_TryBeginExclusiveOp())
        {
            LFPG_Util.Warn("[UpgradePump] Another destructive operation already owns the target.");
            return;
        }

        // T2 must not carry a physics body inside T1's volume. Strip T1 first,
        // then spawn T2 with ECE_CREATEPHYSICS at the same origin used today.
        if (dBodyIsSet(pump))
            dBodyDestroy(pump);

        EntityAI t2 = EntityAI.Cast(g_Game.CreateObjectEx("LFPG_WaterPump_T2", spawnPos, ECE_CREATEPHYSICS));
        if (!t2)
        {
            RestorePumpStaticPhysics(pump);
            pump.LFPG_EndExclusiveOp();
            LFPG_Util.Error("[UpgradePump] Failed to create LFPG_WaterPump_T2 - aborting upgrade, T1 + materials preserved");
            return;
        }
    
        t2.SetPosition(spawnPos);
        t2.SetOrientation(ori);
        t2.Update();
    
        array<EntityAI> stagedOutputs = new array<EntityAI>();
        stagedOutputs.Insert(t2);
    
        if (!StageMaterialSurplus(plate, plateQty, LFPG_PUMP_UPGRADE_PLATES, pos, stagedOutputs))
        {
            AbortStagedUpgradeOutputs(stagedOutputs);
            RestorePumpStaticPhysics(pump);
            pump.LFPG_EndExclusiveOp();
            LFPG_Util.Error("[UpgradePump] Failed to stage MetalPlate surplus - T1 + materials preserved");
            return;
        }
        if (!StageMaterialSurplus(nails, nailsQty, LFPG_PUMP_UPGRADE_NAILS, pos, stagedOutputs))
        {
            AbortStagedUpgradeOutputs(stagedOutputs);
            RestorePumpStaticPhysics(pump);
            pump.LFPG_EndExclusiveOp();
            LFPG_Util.Error("[UpgradePump] Failed to stage Nail surplus - T1 + materials preserved");
            return;
        }
		// Move the original filter only after every fallible output creation succeeds.
		// Its identity preserves subtype, zero quantity, health and custom script state.
		if (filterItem)
		{
			InventoryLocation filterSource = new InventoryLocation();
			InventoryLocation filterTarget = new InventoryLocation();
			if (!filterItem.GetInventory().GetCurrentInventoryLocation(filterSource))
			{
				AbortStagedUpgradeOutputs(stagedOutputs);
				RestorePumpStaticPhysics(pump);
				pump.LFPG_EndExclusiveOp();
				LFPG_Util.Error("[UpgradePump] Cannot locate original filter - T1 preserved");
				return;
			}
			filterTarget.SetAttachment(t2, filterItem, filterSource.GetSlot());
			if (!GameInventory.LocationSyncMoveEntity(filterSource, filterTarget))
			{
				AbortStagedUpgradeOutputs(stagedOutputs);
				RestorePumpStaticPhysics(pump);
				pump.LFPG_EndExclusiveOp();
				LFPG_Util.Error("[UpgradePump] Cannot transfer original filter - T1 preserved");
				return;
			}
		}
		// No fallible creation or transfer remains after moving the original filter.
        g_Game.ObjectDelete(plate);
        g_Game.ObjectDelete(nails);
        LFPG_DeviceLifecycle.OnDeviceKilled(pump, deviceId);
        g_Game.ObjectDelete(pump);
    
        // The exclusive flag intentionally remains set until EEDelete completes.
        LFPG_Util.Info("[UpgradePump] T2 created at " + spawnPos.ToString() + " ori=" + ori.ToString() + " (T1 was " + pos.ToString() + ")");
    }

    // Rebuild a static body from the VObject if an abort happens after T1
    // physics was stripped and T2 did not commit. Layer mask 0xffffffff
    // keeps the geometry layers from the P3D unmodified.
    protected void RestorePumpStaticPhysics(EntityAI device)
    {
        if (!device)
            return;
        if (dBodyIsSet(device))
            return;

        Physics.CreateStatic(device, 0xffffffff);
        if (!dBodyIsSet(device))
            LFPG_Util.Error("[UpgradePump] Failed to restore pump physics after aborted upgrade");
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
    
		// Keep vanilla health (including zones), variables and agents; quantity is the surplus.
		MiscGameplayFunctions.TransferItemProperties(item, excessItem, true, true, true, true);
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
            {
                if (dBodyIsSet(output))
                    dBodyDestroy(output);
                g_Game.ObjectDelete(output);
            }
        }
        stagedOutputs.Clear();
    }
};
