// =========================================================
// LF_PowerGrid - GardenBase Extension (v5.2 Sprinkler Watering)
//
// Adds LFPG_WaterFromSprinkler() to vanilla GardenBase.
// m_Slots is protected → only accessible via modded class.
//
// Called by LFPG_Sprinkler.LFPG_TickWatering() during NM tick.
// Replicates vanilla CheckRainTick watering logic:
//   1. WaterAllSlots() → sets watered bitmask (visual)
//   2. slot.GiveWater() → adds actual water for plant growth
//   3. SetSynchDirty() → syncs state to clients
// =========================================================

modded class GardenBase
{
    void LFPG_WaterFromSprinkler(float amount)
    {
        #ifdef SERVER
        // Phase 1: Set visual watered state (bitmask)
        WaterAllSlots();

        // Phase 2: Add actual water to each slot (plant growth)
        int slotsCount = GetGardenSlotsCount();
        int i;
        Slot slot;
        float waterAmt;

        for (i = 0; i < slotsCount; i = i + 1)
        {
            if (!m_Slots)
                break;

            if (i >= m_Slots.Count())
                break;

            slot = m_Slots.Get(i);
            if (!slot)
                continue;

            waterAmt = amount * Math.RandomFloat01();
            slot.GiveWater(waterAmt);
        }

        // Phase 3: Sync to clients
        SetSynchDirty();
        #endif
    }
};
