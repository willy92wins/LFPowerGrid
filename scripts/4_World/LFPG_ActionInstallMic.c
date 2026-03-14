// =========================================================
// LF_PowerGrid - Action: Install Microphone (v3.0.0 - Sprint 3)
//
// Continuous action (5s) to upgrade Intercom from T1 to T2.
// Consumes a PersonalRadio from player inventory. Irreversible.
//
// Requirements:
//   - Player holds Screwdriver in hands
//   - Target: LF_Intercom with m_RadioInstalled == false
//   - Player has PersonalRadio anywhere in inventory
//
// On completion (server):
//   1. Find and delete PersonalRadio from player inventory
//   2. Set m_RadioInstalled = true
//   3. Set m_FrequencyIndex = 0 (default 87.8 MHz)
//   4. Show microphone (SetObjectTexture on hiddenSelection 4)
//   5. If powered+switchOn → spawn ghost radio
//   6. SetSynchDirty()
//
// IMPORTANTE: Registrar en ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionInstallMic).
// =========================================================

// ---- Callback: 5 second progress bar ----
class LFPG_ActionInstallMicCB : ActionContinuousBaseCB
{
    override void CreateActionComponent()
    {
        m_ActionData.m_ActionComponent = new CAContinuousTime(5.0);
    }
};

// ---- Main action ----
class LFPG_ActionInstallMic : ActionContinuousBase
{
    void LFPG_ActionInstallMic()
    {
        m_CallbackClass = LFPG_ActionInstallMicCB;
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_CRAFTING;
        m_FullBody = true;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
        m_Text = "#STR_LFPG_ACTION_INSTALL_MIC";
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

        // Item in hands must be a Screwdriver
        if (!item.IsKindOf("Screwdriver"))
            return false;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return false;

        LF_Intercom ic = LF_Intercom.Cast(targetObj);
        if (!ic)
            return false;

        // Must NOT already have radio installed
        if (ic.LFPG_GetRadioInstalled())
            return false;

        // Player must have a PersonalRadio in inventory
        if (!LFPG_FindPlayerRadio(player))
            return false;

        return true;
    }

    override void OnFinishProgressServer(ActionData action_data)
    {
        super.OnFinishProgressServer(action_data);

        if (!action_data)
            return;

        if (!action_data.m_Target)
            return;

        Object targetObj = action_data.m_Target.GetObject();
        if (!targetObj)
            return;

        LF_Intercom ic = LF_Intercom.Cast(targetObj);
        if (!ic)
            return;

        // Re-validate: not already installed (anti-exploit)
        if (ic.LFPG_GetRadioInstalled())
        {
            LFPG_Util.Warn("[InstallMic] Radio already installed, aborting.");
            return;
        }

        // Find and consume PersonalRadio from player inventory
        PlayerBase pb = PlayerBase.Cast(action_data.m_Player);
        if (!pb)
            return;

        EntityAI radio = LFPG_FindPlayerRadio(pb);
        if (!radio)
        {
            LFPG_Util.Warn("[InstallMic] No PersonalRadio found in player inventory.");
            return;
        }

        // Delete the radio (consumed permanently)
        GetGame().ObjectDelete(radio);

        // Upgrade intercom to T2
        ic.LFPG_InstallRadio();

        string installMsg = "[LFPG] Microphone installed";
        pb.MessageStatus(installMsg);

        LFPG_Util.Info("[InstallMic] Radio consumed, T2 upgrade complete for id=" + ic.LFPG_GetDeviceId());
    }

    // Helper: find a PersonalRadio in player's inventory (excluding LF_GhostRadio)
    static EntityAI LFPG_FindPlayerRadio(PlayerBase player)
    {
        if (!player)
            return null;

        array<EntityAI> items = new array<EntityAI>;
        player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, items);

        int i;
        int count = items.Count();
        EntityAI candidate;
        string typeName;

        for (i = 0; i < count; i = i + 1)
        {
            candidate = items[i];
            if (!candidate)
                continue;

            typeName = candidate.GetType();

            // Must be a PersonalRadio (or subclass)
            if (!candidate.IsKindOf("PersonalRadio"))
                continue;

            // Exclude ghost radios (should never be in player inventory, but safety)
            if (typeName == "LF_GhostRadio")
                continue;

            return candidate;
        }

        return null;
    }
};
