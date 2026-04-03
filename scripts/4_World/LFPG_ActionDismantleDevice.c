// =========================================================
// LF_PowerGrid - Action: Dismantle Device (v4.5)
//
// Continuous action (5s) to dismantle a placed LFPG device
// and recover its deployment kit.
//
// Requirements:
//   - Player holds Screwdriver in hands
//   - Target: any LFPG_DeviceBase with LFPG_GetKitClassname() != ""
//   - Device must be completely empty:
//     * No attachments (upgrade materials, radio, batteries, etc.)
//     * No cargo items (Fridge contents, etc.)
//
// On completion (server):
//   1. Re-validate all conditions (anti-exploit)
//   2. Spawn kit at player feet via CreateObjectEx
//   3. Damage screwdriver 10% of max health
//   4. ObjectDelete(device) → EEDelete handles full lifecycle:
//      wire cutting, graph cleanup, NM unregister, registry removal
//
// Excluded devices (LFPG_GetKitClassname returns ""):
//   - LFPG_SolarPanel_T2    (upgraded, non-reversible)
//   - LFPG_WaterPump_T2     (upgraded, non-reversible)
//   - LFPG_BatteryAdapter   (can be picked up directly)
//
// Devices not extending LFPG_DeviceBase are excluded by Cast:
//   - LFPG_Generator (PowerGenerator), LF_TestLamp (Spotlight)
//
// IMPORTANTE: Registrar en ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionDismantleDevice).
//   Also AddAction in modded class Screwdriver.
// =========================================================

// ---- Callback: 5 second progress bar ----
class LFPG_ActionDismantleDeviceCB : ActionContinuousBaseCB
{
    override void CreateActionComponent()
    {
        m_ActionData.m_ActionComponent = new CAContinuousTime(5.0);
    }
};

// ---- Main action ----
class LFPG_ActionDismantleDevice : ActionContinuousBase
{
    void LFPG_ActionDismantleDevice()
    {
        m_CallbackClass = LFPG_ActionDismantleDeviceCB;
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_CRAFTING;
        m_FullBody = true;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
        m_Text = "#STR_LFPG_ACTION_DISMANTLE";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem   = new CCINonRuined;
        m_ConditionTarget = new CCTCursor(LFPG_INTERACT_DIST_M);
    }

    // ============================================
    // Shared validation (used by ActionCondition + OnFinishProgressServer)
    // Returns the device if valid, null otherwise.
    // ============================================
    protected LFPG_DeviceBase LFPG_ValidateDismantle(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!player)
            return null;

        if (!target)
            return null;

        if (!item)
            return null;

        // Item in hands must be a Screwdriver
        string kindScrew = "Screwdriver";
        if (!item.IsKindOf(kindScrew))
            return null;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return null;

        // Target must be LFPG_DeviceBase
        LFPG_DeviceBase device = LFPG_DeviceBase.Cast(targetObj);
        if (!device)
            return null;

        // Device must have a kit classname (non-empty = dismantlable)
        string kitClass = device.LFPG_GetKitClassname();
        if (kitClass == "")
            return null;

        // Device must have no attachments
        int attCount = device.GetInventory().AttachmentCount();
        if (attCount > 0)
            return null;

        // Device must have no cargo items
        CargoBase cargo = device.GetInventory().GetCargo();
        if (cargo)
        {
            int cargoCount = cargo.GetItemCount();
            if (cargoCount > 0)
                return null;
        }

        // Manual proximity check (belt-and-suspenders with CCTCursor)
        float distSq = LFPG_WorldUtil.DistSq(player.GetPosition(), device.GetPosition());
        float maxDistSq = LFPG_INTERACT_DIST_M * LFPG_INTERACT_DIST_M;
        if (distSq > maxDistSq)
            return null;

        return device;
    }

    // ============================================
    // ActionCondition — evaluated every frame on client + server
    // ============================================
    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        LFPG_DeviceBase device = LFPG_ValidateDismantle(player, target, item);
        if (!device)
            return false;

        return true;
    }

    // ============================================
    // OnFinishProgressServer — called once after 5s progress bar
    // ============================================
    override void OnFinishProgressServer(ActionData action_data)
    {
        super.OnFinishProgressServer(action_data);

        if (!action_data)
            return;

        if (!action_data.m_Target)
            return;

        PlayerBase player = PlayerBase.Cast(action_data.m_Player);
        if (!player)
            return;

        ItemBase screwdriver = ItemBase.Cast(action_data.m_MainItem);
        if (!screwdriver)
            return;

        // Re-validate everything (anti-exploit: conditions may have changed during 5s)
        LFPG_DeviceBase device = LFPG_ValidateDismantle(player, action_data.m_Target, screwdriver);
        if (!device)
        {
            string abortMsg = "[LFPG] Dismantle aborted: conditions changed.";
            player.MessageStatus(abortMsg);
            return;
        }

        string kitClass = device.LFPG_GetKitClassname();
        string deviceType = device.GetType();
        string deviceId = device.LFPG_GetDeviceId();
        vector playerPos = player.GetPosition();

        // Spawn kit at player feet
        EntityAI kit = GetGame().CreateObjectEx(kitClass, playerPos, ECE_CREATEPHYSICS);
        if (!kit)
        {
            string failMsg = "[LFPG] Dismantle failed: could not create ";
            failMsg = failMsg + kitClass;
            player.MessageStatus(failMsg);

            string failLog = "[Dismantle] CreateObjectEx failed for ";
            failLog = failLog + kitClass;
            failLog = failLog + " at ";
            failLog = failLog + playerPos.ToString();
            LFPG_Util.Error(failLog);
            return;
        }

        kit.SetPosition(playerPos);
        kit.PlaceOnSurface();

        // Damage screwdriver 10% of max health
        string dmgZone = "";
        string dmgType = "";
        float maxHP = screwdriver.GetMaxHealth(dmgZone, dmgType);
        float dmg = maxHP * 0.1;
        screwdriver.DecreaseHealth(dmgZone, dmgType, dmg);

        // Delete device — EEDelete handles full lifecycle:
        // LFPG_OnDeleted() → device-specific cleanup (NM unregister, sounds, etc.)
        // LFPG_DeviceLifecycle.OnDeviceDeleted() → wire cut + graph + registry
        GetGame().ObjectDelete(device);

        // Log
        string okLog = "[Dismantle] ";
        okLog = okLog + deviceType;
        okLog = okLog + " (";
        okLog = okLog + deviceId;
        okLog = okLog + ") -> ";
        okLog = okLog + kitClass;
        okLog = okLog + " by ";
        okLog = okLog + player.GetIdentity().GetName();
        LFPG_Util.Info(okLog);

        // Player feedback
        string okMsg = "[LFPG] Dismantled ";
        okMsg = okMsg + deviceType;
        player.MessageStatus(okMsg);
    }
};
