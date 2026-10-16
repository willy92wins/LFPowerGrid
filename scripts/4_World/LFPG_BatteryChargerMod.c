// Close the vanilla charger clock on LargeBattery attach/detach so a
// same-object reinsert between two TickVanillaChargers visits cannot
// inherit the time the battery spent off the slot (G03-b). super first;
// SERVER only; no extra members (modded class restriction).
modded class BatteryCharger
{
	override void EEItemAttached(EntityAI item, string slot_name)
	{
		super.EEItemAttached(item, slot_name);
		#ifdef SERVER
		if (slot_name != "LargeBattery")
			return;
		LFPG_NetworkManager nm = LFPG_NetworkManager.GetExisting();
		if (!nm)
			return;
		LFPG_ElecGraph graph = nm.GetGraph();
		if (graph)
			graph.NotifyVanillaChargerAttachment(this, false);
		#endif
	}

	override void EEItemDetached(EntityAI item, string slot_name)
	{
		super.EEItemDetached(item, slot_name);
		#ifdef SERVER
		if (slot_name != "LargeBattery")
			return;
		LFPG_NetworkManager nm = LFPG_NetworkManager.GetExisting();
		if (!nm)
			return;
		LFPG_ElecGraph graph = nm.GetGraph();
		if (graph)
			graph.NotifyVanillaChargerAttachment(this, true);
		#endif
	}
}
