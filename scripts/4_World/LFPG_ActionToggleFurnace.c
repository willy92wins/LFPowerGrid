// =========================================================
// LF_PowerGrid - Action: Toggle Furnace On/Off (v1.2.0)
//
// Turns the furnace on or off. No item required (CCINone).
// Dynamic text: "Turn On Furnace" / "Turn Off Furnace"
//
// Conditions to turn ON:  m_FuelCurrent > 0
// Conditions to turn OFF: always available if furnace is on
//
// Base: ActionInteractBase (CCINone, no item in hand)
// Target: LF_Furnace
//
// IMPORTANTE: Registrar en ActionConstructor.RegisterActions()
//   via actions.Insert(LFPG_ActionToggleFurnace).
// =========================================================

class LFPG_ActionToggleFurnace : ActionInteractBase
{
    void LFPG_ActionToggleFurnace()
    {
        m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
        m_StanceMask = DayZPlayerConstants.STANCEMASK_ALL;
        m_Text = "#STR_LFPG_ACTION_TOGGLE_FURNACE";
    }

    override void CreateConditionComponents()
    {
        m_ConditionItem   = new CCINone;
        m_ConditionTarget = new CCTObject(LFPG_INTERACT_DIST_M);
    }

    override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
    {
        if (!player)
            return false;

        if (!target)
            return false;

        Object targetObj = target.GetObject();
        if (!targetObj)
            return false;

        LF_Furnace furnace = LF_Furnace.Cast(targetObj);
        if (!furnace)
            return false;

        bool isOn = furnace.LFPG_GetSourceOn();
        int fuelCur = furnace.LFPG_GetFuelCurrent();

        // If furnace is OFF, can only turn ON if there's fuel OR cargo items
        if (!isOn && fuelCur <= 0)
        {
            bool hasCargo = furnace.LFPG_HasCargoItems();
            if (!hasCargo)
                return false;
        }

        // Dynamic text update with fuel percentage
        int fuelMax = LFPG_FURNACE_MAX_FUEL;
        float fuelPctF = 0.0;
        if (fuelMax > 0)
        {
            fuelPctF = (fuelCur * 100.0) / fuelMax;
        }
        // Format to 0.1% precision
        int fuelPctWhole = Math.Floor(fuelPctF);
        float fuelPctFrac = fuelPctF - fuelPctWhole;
        int fuelPctTenths = Math.Round(fuelPctFrac * 10.0);
        if (fuelPctTenths >= 10)
        {
            fuelPctWhole = fuelPctWhole + 1;
            fuelPctTenths = 0;
        }
        string pctStr = " (" + fuelPctWhole.ToString() + "." + fuelPctTenths.ToString() + "%)";

        string actionLabel = "";
        if (isOn)
        {
            actionLabel = Widget.TranslateString("#STR_LFPG_ACTION_FURNACE_OFF");
        }
        else
        {
            actionLabel = Widget.TranslateString("#STR_LFPG_ACTION_FURNACE_ON");
        }
        m_Text = actionLabel + pctStr;

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

        furnace.LFPG_ToggleFurnace();

        PlayerBase pb = PlayerBase.Cast(action_data.m_Player);
        if (pb)
        {
            bool nowOn = furnace.LFPG_GetSourceOn();
            if (nowOn)
            {
                pb.MessageStatus("[LFPG] Furnace ON");
            }
            else
            {
                pb.MessageStatus("[LFPG] Furnace OFF");
            }
        }
    }
};
