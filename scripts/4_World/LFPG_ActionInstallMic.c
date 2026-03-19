// =========================================================
// LF_PowerGrid - Action: Install Microphone (v3.1.0)
//
// Continuous action (5s) to upgrade Intercom from T1 to T2.
// Consumes a PersonalRadio from the Intercom's attachment slot.
// Irreversible: radio is locked after install (CanReleaseAttachment).
//
// Requirements:
//   - Player holds Screwdriver in hands
//   - Target: LF_Intercom with m_RadioInstalled == false
//   - LF_Intercom has a PersonalRadio in its LF_IntercomRadio slot
//
// On completion (server):
//   1. Set m_RadioInstalled = true (locks slot via CanReleaseAttachment)
//   2. Delete PersonalRadio from intercom attachment slot (consumed)
//   3. Set m_FrequencyIndex = 0 (default 87.8 MHz)
//   4. Show microphone (SetObjectTexture on hiddenSelection 4)
//   5. If powered+switchOn -> spawn ghost radio
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
        string kindScrew = "Screwdriver";
        if (!item.IsKindOf(kindScrew))
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

        // Intercom must have a PersonalRadio in its attachment slot
        EntityAI slotRadio = LFPG_FindIntercomRadio(ic);
        if (!slotRadio)
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

        // Find PersonalRadio in intercom attachment slot
        EntityAI radio = LFPG_FindIntercomRadio(ic);
        if (!radio)
        {
            LFPG_Util.Warn("[InstallMic] No PersonalRadio in intercom slot.");
            return;
        }

        // Upgrade intercom to T2 FIRST (sets m_RadioInstalled = true,
        // which makes CanReleaseAttachment return false — locking the radio)
        ic.LFPG_InstallRadio();

        // Delete the radio from slot (consumed permanently)
        GetGame().ObjectDelete(radio);

        PlayerBase pb = PlayerBase.Cast(action_data.m_Player);
        if (pb)
        {
            string installMsg = "[LFPG] Microphone installed";
            pb.MessageStatus(installMsg);
        }

        string logMsg = "[InstallMic] Radio consumed, T2 upgrade complete for id=";
        logMsg = logMsg + ic.LFPG_GetDeviceId();
        LFPG_Util.Info(logMsg);
    }

    // Helper: find a PersonalRadio in the intercom's attachment slot
    static EntityAI LFPG_FindIntercomRadio(LF_Intercom ic)
    {
        if (!ic)
            return null;

        string slotName = "LF_IntercomRadio";
        int slotId = InventorySlots.GetSlotIdFromString(slotName);
        if (slotId == InventorySlots.INVALID)
            return null;

        EntityAI att = ic.GetInventory().FindAttachment(slotId);
        if (!att)
            return null;

        // Must be a PersonalRadio (safety)
        string kindRadio = "PersonalRadio";
        if (!att.IsKindOf(kindRadio))
            return null;

        // Exclude ghost radios (should never be here, but safety)
        string typeName = att.GetType();
        if (typeName == "LF_GhostRadio")
            return null;

        return att;
    }
};
