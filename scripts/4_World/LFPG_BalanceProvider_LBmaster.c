// =========================================================
// LF_PowerGrid — LBmaster Balance Provider
//
// Wraps LB_ATM_Playerbase from the LBmaster ATM mod.
// Entire file compiled only when LBmaster_Core is present.
// =========================================================

#ifdef LBmaster_Core

class LFPG_BalanceProvider_LBmaster extends LFPG_BalanceProvider
{
    void LFPG_BalanceProvider_LBmaster()
    {
        m_Name = "LBmaster";
        m_Priority = 10;
    }

    override int GetBalance(PlayerBase player)
    {
        if (!player)
            return 0;

        LB_ATM_Playerbase atmPlayer = new LB_ATM_Playerbase(player);
        int balance = atmPlayer.GetATMMoney();
        return balance;
    }

    override int AddBalance(PlayerBase player, int amount)
    {
        if (!player)
            return 0;
        if (amount <= 0)
            return 0;

        LB_ATM_Playerbase atmPlayer = new LB_ATM_Playerbase(player);
        int added = atmPlayer.AddATMMoney(amount);
        return added;
    }

    override int RemoveBalance(PlayerBase player, int amount)
    {
        if (!player)
            return 0;
        if (amount <= 0)
            return 0;

        LB_ATM_Playerbase atmPlayer = new LB_ATM_Playerbase(player);
        int removed = atmPlayer.RemoveATMMoney(amount);
        return removed;
    }
};

#endif
