// =========================================================
// LF_PowerGrid - Sorter variant with retained _TEST classnames.
// Entity behaviour, persistence and the V4 panel action are shared
// with LFPG_Sorter. Keep classnames stable for existing worlds.
// =========================================================

class LFPG_Sorter_TEST_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_Sorter_TEST";
    }
};

class LFPG_Sorter_TEST : LFPG_Sorter
{
};
