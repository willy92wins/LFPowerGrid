// =========================================================
// LF_PowerGrid - action registration + tool attachments
// IMPORTANT:
//  - Actions MUST be registered in ActionConstructor::RegisterActions()
//    otherwise AddAction(...) on items will not create an instance.
// =========================================================

modded class ActionConstructor
{
    override void RegisterActions(TTypenameArray actions)
    {
        super.RegisterActions(actions);

        // --- LF_PowerGrid per-port actions ---
        actions.Insert(ActionLFPG_Port0);
        actions.Insert(ActionLFPG_Port1);
        actions.Insert(ActionLFPG_Port2);
        actions.Insert(ActionLFPG_Port3);
        actions.Insert(ActionLFPG_Port4);
        actions.Insert(ActionLFPG_Port5);

        // --- LF_PowerGrid session + utility ---
        actions.Insert(ActionLFPG_PlaceWaypoint);
        actions.Insert(ActionLFPG_CancelWiring);
        actions.Insert(ActionLFPG_CutWires);
        actions.Insert(ActionLFPG_CutPort0);
        actions.Insert(ActionLFPG_CutPort1);
        actions.Insert(ActionLFPG_CutPort2);
        actions.Insert(ActionLFPG_CutPort3);
        actions.Insert(ActionLFPG_CutPort4);
        actions.Insert(ActionLFPG_CutPort5);
        actions.Insert(ActionLFPG_ToggleSource);
        actions.Insert(ActionLFPG_DebugStatus);

        LFPG_Util.Debug("[Actions] LFPG actions registered (17)");
    }
};

modded class Pliers
{
    override void SetActions()
    {
        super.SetActions();
        AddAction(ActionLFPG_CutWires);
        AddAction(ActionLFPG_CutPort0);
        AddAction(ActionLFPG_CutPort1);
        AddAction(ActionLFPG_CutPort2);
        AddAction(ActionLFPG_CutPort3);
        AddAction(ActionLFPG_CutPort4);
        AddAction(ActionLFPG_CutPort5);
    }
};
