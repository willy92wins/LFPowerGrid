// =========================================================
// LF_PowerGrid - GardenBase Extension (v5.4 Sprinkler Watering)
//
// Adds LFPG_WaterFromSprinkler() to vanilla GardenBase.
// m_Slots is protected → only accessible via modded class.
//
// Called by LFPG_Sprinkler.LFPG_TickWatering() during NM tick.
// Mirrors vanilla CheckRainTick pattern exactly:
//   slot.GiveWater() handles everything internally:
//     - updates water quantity via SetWaterQuantity bitmap
//     - transitions DRY→WET via SetWateredState+SlotWaterStateUpdate
//     - triggers CreatePlant if seed present + water sufficient
//
// v5.4: Removed deprecated WaterAllSlots() (vanilla [Obsolete]).
//        GiveWater alone is the correct future-proof path.
//        Works for GardenBase and all subclasses (GardenPlot, etc).
// =========================================================

modded class GardenBase
{
    void LFPG_WaterFromSprinkler(float amount)
    {
        #ifdef SERVER
        if (!m_Slots)
            return;

        int slotsCount = GetGardenSlotsCount();
        int i;
        Slot slot;
        float waterAmt;

        for (i = 0; i < slotsCount; i = i + 1)
        {
            if (i >= m_Slots.Count())
                break;

            slot = m_Slots.Get(i);
            if (!slot)
                continue;

            waterAmt = amount * Math.RandomFloat01();
            slot.GiveWater(waterAmt);
        }

        // Sync bitmap changes to clients
        SetSynchDirty();
        #endif
    }
};
