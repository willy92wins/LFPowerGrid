// =========================================================
// LF_PowerGrid - Upgrade Solar Panel T1 → T2
//
// v0.7.47: ActionContinuousBase with crafting progress bar.
//
// Requirements:
//   - Player holds Hammer (item in hands)
//   - Target: LF_SolarPanel (T1, NOT T2)
//   - T1 has MetalPlate in slot LF_SolarPlate with qty >= 5
//   - T1 has Nail in slot LF_SolarNails with qty >= 20
//
// On completion:
//   1. Consume materials (excess returned to ground)
//   2. DeviceLifecycle.OnDeviceKilled (cuts wires, cleans graph)
//   3. Delete T1
//   4. Create T2 at same position/orientation
//
// ENFORCE SCRIPT NOTES:
//   - No ternary operators
//   - No ++ / -- operators
//   - Explicit typing on all variables
//   - No foreach loops
// =========================================================

// ---- Callback: controls progress bar duration ----
class LFPG_ActionUpgradeSolarCB : ActionContinuousBaseCB
{
    override void CreateActionComponent()
    {
        m_ActionData.m_ActionComponent = new CAContinuousTime(8.0);
    }
};

// ---- Main action ----
class LFPG_ActionUpgradeSolarPanel : ActionContinuousBase
{
    // Material requirements
    static const int LFPG_SOLAR_PLATE_REQUIRED = 5;
    static const int LFPG_SOLAR_NAILS_REQUIRED = 20;

    void LFPG_ActionUpgradeSolarPanel()
    {
        m_CallbackClass = LFPG_ActionUpgradeSolarCB;
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_CRAFTING;
        m_FullBody = true;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
        m_Text = "#STR_LFPG_ACTION_UPGRADE_SOLAR";
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

        // Item in hands must be a Hammer
        if (!item.IsKindOf("Hammer"))
            return false;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return false;

        // Target must be T1 solar panel
        LF_SolarPanel panel = LF_SolarPanel.Cast(targetObj);
        if (!panel)
            return false;

        // Must NOT be T2 already
        if (panel.IsKindOf("LF_SolarPanel_T2"))
            return false;

        // Check materials in attachment slots
        EntityAI plate = panel.FindAttachmentBySlotName("LF_SolarPlate");
        if (!plate)
            return false;

        int plateQty = plate.GetQuantity();
        if (plateQty < LFPG_SOLAR_PLATE_REQUIRED)
            return false;

        EntityAI nails = panel.FindAttachmentBySlotName("LF_SolarNails");
        if (!nails)
            return false;

        int nailsQty = nails.GetQuantity();
        if (nailsQty < LFPG_SOLAR_NAILS_REQUIRED)
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

        LF_SolarPanel panel = LF_SolarPanel.Cast(targetObj);
        if (!panel)
            return;

        // Re-validate T2 check (anti-exploit: action could be started on T1 then swapped)
        if (panel.IsKindOf("LF_SolarPanel_T2"))
        {
            LFPG_Util.Warn("[UpgradeSolar] Target is already T2, aborting.");
            return;
        }

        // ---- Re-validate materials (server authority) ----
        EntityAI plate = panel.FindAttachmentBySlotName("LF_SolarPlate");
        if (!plate)
        {
            LFPG_Util.Warn("[UpgradeSolar] No MetalPlate found, aborting.");
            return;
        }

        int plateQty = plate.GetQuantity();
        if (plateQty < LFPG_SOLAR_PLATE_REQUIRED)
        {
            LFPG_Util.Warn("[UpgradeSolar] Insufficient MetalPlate qty=" + plateQty.ToString());
            return;
        }

        EntityAI nails = panel.FindAttachmentBySlotName("LF_SolarNails");
        if (!nails)
        {
            LFPG_Util.Warn("[UpgradeSolar] No Nails found, aborting.");
            return;
        }

        int nailsQty = nails.GetQuantity();
        if (nailsQty < LFPG_SOLAR_NAILS_REQUIRED)
        {
            LFPG_Util.Warn("[UpgradeSolar] Insufficient Nails qty=" + nailsQty.ToString());
            return;
        }

        // ---- Capture transform before deletion ----
        vector pos = panel.GetPosition();
        vector ori = panel.GetOrientation();

        // ---- Consume / detach materials BEFORE panel deletion ----
        // ObjectDelete(panel) cascade-deletes all attachments.
        // Excess materials must be detached to ground first.
        ConsumeMaterial(panel, plate, plateQty, LFPG_SOLAR_PLATE_REQUIRED, pos);
        ConsumeMaterial(panel, nails, nailsQty, LFPG_SOLAR_NAILS_REQUIRED, pos);

        // ---- Kill T1 device (cuts wires, cleans graph, unregisters) ----
        string deviceId = panel.LFPG_GetDeviceId();
        LFPG_DeviceLifecycle.OnDeviceKilled(panel, deviceId);

        // ---- Delete T1 ----
        GetGame().ObjectDelete(panel);

        // ---- Spawn T2 at same position ----
        EntityAI t2 = GetGame().CreateObjectEx("LF_SolarPanel_T2", pos, ECE_CREATEPHYSICS);
        if (t2)
        {
            t2.SetPosition(pos);
            t2.SetOrientation(ori);
            t2.Update();
            LFPG_Util.Info("[UpgradeSolar] T2 created at " + pos.ToString() + " ori=" + ori.ToString());
        }
        else
        {
            LFPG_Util.Error("[UpgradeSolar] Failed to create LF_SolarPanel_T2!");
        }
    }

    // ---- Helper: consume exact amount, drop excess to ground ----
    // If qty <= required: delete item entirely (consumed in full).
    // If qty > required: spawn NEW item with excess on ground, then delete original.
    //   The original attachment is cascade-deleted with the panel.
    //   Spawning a new item avoids inventory slot complications
    //   (PlaceOnSurface/DropEntity on attached items is unreliable).
    protected void ConsumeMaterial(EntityAI parent, EntityAI item, int currentQty, int required, vector dropPos)
    {
        if (!item)
            return;

        if (currentQty <= required)
        {
            // Consume all — delete the item
            GetGame().ObjectDelete(item);
        }
        else
        {
            // Spawn excess as new item on ground
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

            // Delete original (consumed). Also handles cascade from panel delete.
            GetGame().ObjectDelete(item);
        }
    }
};
