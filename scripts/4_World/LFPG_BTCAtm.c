// =========================================================
// LF_PowerGrid - BTC ATM Devices (Sprint BTC-5)
//
// LFPG_BTCAtmBase:   Abstract base (LFPG_DeviceBase), all BTC logic.
// LFPG_BTCAtm:       CONSUMER 30u/s, 1 IN, deployable by players.
//                    Visual: screen + LED swap on power change.
// LFPG_BTCAtmAdmin:  No power, no ports, placed by admins.
//                    Visual: always green (config.cpp defaults).
//
// Both share:
//   - m_BtcStock:          BTC units stored in this machine (SyncVar)
//   - m_ATMWithdrawOnly:   ATMWithdrawOnly mode (SyncVar)
//   - m_DecimalRemainder:  Accumulated fractional money from rounding (persisted, NOT SyncVar)
//
// Persistence (DeviceBase v3 format):
//   [super: DeviceIdLow/High + devicePersistVer]
//   [m_BtcStock : int]
//   [m_ATMWithdrawOnly : bool]
//   [m_DecimalRemainder : float]
//
// RPC handling lives in LFPG_PlayerRPC.c (Sprint BTC-3).
// UI lives in Sprint BTC-4.
//
// ⚠ SAVE WIPE required (new entity type).
// =========================================================

// ---- BTC ATM rvmat paths (static const, no per-call alloc) ----
// Screen uses the "HideScreen" animation (translation) to reveal/cover the
// baked emission face underneath — NOT a material swap. See model.cfg.
static const string LFPG_BTC_RVMAT_LED_ON     = "\LFPowerGrid\data\btc_atm\data\bitcoin_atm_green.rvmat";
static const string LFPG_BTC_RVMAT_LED_OFF    = "\LFPowerGrid\data\btc_atm\data\bitcoin_atm_red.rvmat";


// =========================================================
// BASE CLASS: shared BTC ATM logic
// =========================================================
class LFPG_BTCAtmBase : LFPG_DeviceBase
{
    // ---- BTC SyncVars ----
    protected int  m_BtcStock          = 0;
    protected bool m_ATMWithdrawOnly   = false;

    // Same 64-entity budget as LFPG_BTCHelper.LFPG_BTC_MAX_ENTITIES_PER_TX.
    // Duplicated here: that protected constant lives in the later Mission module.
    protected static const int LFPG_BTC_MAX_ENTITIES_ON_KILL = 64;
    // One attempt per killed instance, including partial drops and failed commits.
    protected bool m_BtcKillDropHandled = false;

    // ---- Server-only persisted state (NOT SyncVar) ----
    // Accumulated fractional money that couldn't be given as
    // physical bills. Carried over across transactions.
    protected float m_DecimalRemainder = 0.0;

    // ============================================
    // Constructor: register BTC SyncVars
    // (DeviceBase constructor already registers DeviceIdLow/High)
    // ============================================
    void LFPG_BTCAtmBase()
    {
        string varStock     = "m_BtcStock";
        string varWithdraw  = "m_ATMWithdrawOnly";
        RegisterNetSyncVariableInt(varStock, 0, 10000);
        RegisterNetSyncVariableBool(varWithdraw);
    }

    // ============================================
    // Virtual: powered check (overridden by subclasses)
    // ============================================
    bool LFPG_IsATMPowered()
    {
        // Base: always powered (admin default)
        return true;
    }

    // ============================================
    // BTC stock access
    // ============================================
    int LFPG_GetBtcStock()
    {
        return m_BtcStock;
    }

    // Refuse dismantling while the machine still holds BTC. The kit that
    // dismantling spawns carries no stock, and the device is deleted right
    // after, so every unit left inside would be destroyed with no refund
    // and no warning. Empty the ATM first, the same way the action already
    // demands no attachments and no cargo.
    override bool LFPG_BlocksDismantle()
    {
        return m_BtcStock > 0;
    }

    void LFPG_SetBtcStock(int stock)
    {
        #ifdef SERVER
        int maxStock = LFPG_BTCConfig.GetMaxBtcPerMachine();
        if (stock > maxStock)
        {
            stock = maxStock;
        }
        if (stock < 0)
        {
            stock = 0;
        }
        if (stock == m_BtcStock)
            return;
        string deviceId = LFPG_GetDeviceId();
        if (!LFPG_AtmStock.PrepareStockMutation(deviceId, m_BtcStock, stock))
            return;

        m_BtcStock = stock;
        SetSynchDirty();
        #endif
    }

    bool LFPG_CanAddBtcStock(int count)
    {
        #ifdef SERVER
        if (count <= 0)
            return false;

        int maxStock = LFPG_BTCConfig.GetMaxBtcPerMachine();
        if (count > maxStock - m_BtcStock)
            return false;
        int newStock = m_BtcStock + count;
        string deviceId = LFPG_GetDeviceId();
        return LFPG_AtmStock.CanPrepareStockMutation(deviceId, m_BtcStock, newStock);
        #else
        return false;
        #endif
    }

    bool LFPG_AddBtcStock(int amount)
    {
        #ifdef SERVER
        if (amount <= 0)
            return false;

        int maxStock = LFPG_BTCConfig.GetMaxBtcPerMachine();
        // Overflow-safe
        if (amount > maxStock - m_BtcStock)
            return false;
        int newStock = m_BtcStock;
        newStock = newStock + amount;

        string deviceId = LFPG_GetDeviceId();
        if (!LFPG_AtmStock.PrepareStockMutation(deviceId, m_BtcStock, newStock))
            return false;

        m_BtcStock = newStock;
        SetSynchDirty();
        return true;
        #else
        return false;
        #endif
    }

    bool LFPG_RemoveBtcStock(int amount)
    {
        #ifdef SERVER
        if (amount <= 0)
            return false;

        if (amount > m_BtcStock)
            return false;

        int newStock = m_BtcStock - amount;
        string deviceId = LFPG_GetDeviceId();
        if (!LFPG_AtmStock.PrepareStockMutation(deviceId, m_BtcStock, newStock))
            return false;

        m_BtcStock = newStock;
        SetSynchDirty();
        return true;
        #else
        return false;
        #endif
    }

    // Applies a durable claim target without creating a fold for itself.
    bool LFPG_ApplyClaimedStockTarget(int stock)
    {
        if (!g_Game || !g_Game.IsServer())
            return false;
        int maxStock = LFPG_BTCConfig.GetMaxBtcPerMachine();
        if (stock < 0 || stock > maxStock)
            return false;

        m_BtcStock = stock;
        SetSynchDirty();
        return true;
    }

    // ============================================
    // ATMWithdrawOnly access
    // ============================================
    bool LFPG_IsWithdrawOnly()
    {
        // Global floor from config. Server-only: Get() lazy-loads and may write defaults.
        #ifdef SERVER
        if (LFPG_BTCConfig.GetAtmWithdrawOnlyDefault())
            return true;
        #endif
        return m_ATMWithdrawOnly;
    }

    void LFPG_SetWithdrawOnly(bool val)
    {
        #ifdef SERVER
        m_ATMWithdrawOnly = val;
        SetSynchDirty();
        #endif
    }

    // ============================================
    // Decimal remainder (server-only, for Sprint 3 change logic)
    // ============================================
    float LFPG_GetDecimalRemainder()
    {
        return m_DecimalRemainder;
    }

    void LFPG_SetDecimalRemainder(float val)
    {
        #ifdef SERVER
        if (val < 0.0)
        {
            val = 0.0;
        }
        // Logical assertion: this field stores fractions only. Integer EUR
        // belongs to account/cash delivery and must never enter persistence.
        if (val >= 1.0)
        {
            LFPG_Util.Error("[LFPG_BTCAtm] Decimal remainder invariant violated (value >= 1); clamping fail-closed on " + LFPG_GetDeviceId());
            val = 0.999999;
        }
        m_DecimalRemainder = val;
        #endif
    }

    // ============================================
    // DeviceAPI interface (consumer defaults)
    // ============================================
    override bool LFPG_IsSource()
    {
        return false;
    }

    override bool LFPG_GetSourceOn()
    {
        return false;
    }

    override float LFPG_GetConsumption()
    {
        return 0.0;
    }

    override bool LFPG_IsPowered()
    {
        return LFPG_IsATMPowered();
    }

    override void LFPG_SetPowered(bool powered)
    {
        // No-op in base. Consumer variant overrides.
    }

    // ============================================
    // Lifecycle: withdraw-only is not seeded here
    // ============================================
    override void LFPG_OnInit()
    {
        // Deliberately empty. The config value is a global floor applied in
        // LFPG_IsWithdrawOnly under SERVER; it is not written into persisted ATM
        // state, and a per-ATM off cannot override the floor while it is active.
        // The #ifdef SERVER that used to wrap these comments was removed: a guard
        // whose body holds no statements segfaults (pitfalls-advanced.md:66-77).
    }

    // Shared by player and admin ATMs. Do not call Mission helpers from World.
    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        if (m_BtcKillDropHandled || m_BtcStock <= 0)
            return;
        m_BtcKillDropHandled = true;

        int stockBefore = m_BtcStock;
        string deviceId = LFPG_GetDeviceId();
        if (!g_Game)
        {
            LFPG_Util.Error("[LFPG_BTCAtm] kill drop unavailable; stock retained=" + stockBefore.ToString() + " deviceId=" + deviceId);
            return;
        }
        string classname = LFPG_BTCConfig.GetBtcItemClassname();
        if (classname == "")
        {
            LFPG_Util.Error("[LFPG_BTCAtm] kill drop has empty BTC classname; stock retained=" + stockBefore.ToString() + " deviceId=" + deviceId);
            return;
        }

        array<EntityAI> drops = new array<EntityAI>();
        vector basePos = GetPosition();
        int remaining = stockBefore;
        int maxStack = 0;
        int attempt = 0;
        // Failed spawns also consume an attempt, so null cannot cause a retry loop.
        for (attempt = 0; attempt < LFPG_BTC_MAX_ENTITIES_ON_KILL; attempt = attempt + 1)
        {
            if (remaining <= 0)
                break;
            vector pos = basePos;
            pos[0] = pos[0] + Math.RandomFloat(-0.15, 0.15);
            pos[2] = pos[2] + Math.RandomFloat(-0.15, 0.15);
            Object obj = g_Game.CreateObjectEx(classname, pos, ECE_CREATEPHYSICS);
            if (!obj)
            {
                LFPG_Util.Error("[LFPG_BTCAtm] kill drop CreateObjectEx failed cls=" + classname + " remaining=" + remaining.ToString() + " deviceId=" + deviceId);
                continue;
            }
            EntityAI item = EntityAI.Cast(obj);
            if (!item)
            {
                g_Game.ObjectDelete(obj);
                LFPG_Util.Error("[LFPG_BTCAtm] kill drop is not EntityAI cls=" + classname + " deviceId=" + deviceId);
                continue;
            }

            // The first item is also the stack-capacity probe, as in BTC staging.
            if (maxStack == 0)
            {
                maxStack = item.GetQuantityMax();
                if (maxStack < 1 || !item.HasQuantity())
                    maxStack = 1;
            }
            int qty = remaining;
            if (qty > maxStack)
                qty = maxStack;
            item.SetQuantity((float)qty, false, false);
            // SetQuantity returns whether it deleted the item, not success.
            if (!item)
            {
                LFPG_Util.Error("[LFPG_BTCAtm] kill drop item disappeared during SetQuantity cls=" + classname + " deviceId=" + deviceId);
                continue;
            }
            if (item.HasQuantity() && item.GetQuantity() != (float)qty)
            {
                g_Game.ObjectDelete(item);
                LFPG_Util.Error("[LFPG_BTCAtm] kill drop quantity mismatch cls=" + classname + " deviceId=" + deviceId);
                continue;
            }
            drops.Insert(item);
            remaining = remaining - qty;
        }

        if (remaining != stockBefore)
        {
            // Remove preserves the stock timeline and returns success. Unlike
            // LFPG_SetBtcStock it does not clamp residual stock loaded under an
            // older config cap. Never bypass its gate after a failed commit.
            int delivered = stockBefore - remaining;
            if (!LFPG_RemoveBtcStock(delivered))
            {
                int i = 0;
                for (i = 0; i < drops.Count(); i = i + 1)
                {
                    EntityAI staged = drops[i];
                    if (staged)
                        g_Game.ObjectDelete(staged);
                }
                LFPG_Util.Error("[LFPG_BTCAtm] kill drop stock commit denied; drops rolled back, stock retained=" + stockBefore.ToString() + ". Admin: inspect stock timeline before recovering ruined ATM deviceId=" + deviceId);
                return;
            }
            LFPG_Util.Info("[LFPG_BTCAtm] kill drop stockBefore=" + stockBefore.ToString() + " delivered=" + delivered.ToString() + " remaining=" + remaining.ToString() + " cls=" + classname + " deviceId=" + deviceId);
        }
        if (remaining > 0)
        {
            LFPG_Util.Error("[LFPG_BTCAtm] kill drop incomplete after 64-attempt budget; undropped stock retained=" + remaining.ToString() + " cls=" + classname + ". Admin: recover residual from ruined ATM before cleanup deviceId=" + deviceId);
        }
        #endif
    }

    // ============================================
    // Persistence hooks (DeviceBase calls these)
    // ============================================
    override int LFPG_GetDevicePersistVersion()
    {
        return 1;
    }

    override void LFPG_OnStoreSaveExtra(ParamsWriteContext ctx)
    {
        ctx.Write(m_BtcStock);
        ctx.Write(m_ATMWithdrawOnly);
        ctx.Write(m_DecimalRemainder);
    }

    override bool LFPG_OnStoreLoadExtra(ParamsReadContext ctx, int ver)
    {
        if (!ctx.Read(m_BtcStock))
        {
            string errStock = "[LFPG_BTCAtm] OnStoreLoad failed: m_BtcStock on ";
            errStock = errStock + GetType();
            LFPG_Util.Error(errStock);
            return false;
        }

        if (!ctx.Read(m_ATMWithdrawOnly))
        {
            string errWO = "[LFPG_BTCAtm] OnStoreLoad failed: m_ATMWithdrawOnly on ";
            errWO = errWO + GetType();
            LFPG_Util.Error(errWO);
            return false;
        }

        if (!ctx.Read(m_DecimalRemainder))
        {
            string errDR = "[LFPG_BTCAtm] OnStoreLoad failed: m_DecimalRemainder on ";
            errDR = errDR + GetType();
            LFPG_Util.Error(errDR);
            return false;
        }

        string loadMsg = "[LFPG_BTCAtm] Loaded: stock=";
        loadMsg = loadMsg + m_BtcStock.ToString();
        loadMsg = loadMsg + " withdrawOnly=";
        loadMsg = loadMsg + m_ATMWithdrawOnly.ToString();
        loadMsg = loadMsg + " remainder=";
        loadMsg = loadMsg + m_DecimalRemainder.ToString();
        loadMsg = loadMsg + " on ";
        loadMsg = loadMsg + m_DeviceId;
        LFPG_Util.Info(loadMsg);

        return true;
    }

    override void AfterStoreLoad()
    {
        super.AfterStoreLoad();
        // Reconcile is a server persistence concern; the client VM must not
        // feed the pre-mission queue (nothing drains it there).
        #ifdef SERVER
        LFPG_AtmStock.ReconcileLoadedAtm(this);
        #endif
    }

    // ============================================
    // Inventory guards (placed device, not pickable)
    // ============================================
    override bool IsHeavyBehaviour()
    {
        return true;
    }

    override bool CanPutInCargo(EntityAI parent)
    {
        return false;
    }

    override bool CanPutIntoHands(EntityAI parent)
    {
        return false;
    }

    override bool DisableVicinityIcon()
    {
        return true;
    }

    override bool CanDisplayCargo()
    {
        return false;
    }

    // ============================================
    // Actions: open ATM UI
    // ============================================
    override void SetActions()
    {
        super.SetActions();
        AddAction(LFPG_ActionOpenBTCAtm);
    }
};


// =========================================================
// PLAYER ATM: CONSUMER 30u/s, 1 IN, deployable
// =========================================================
class LFPG_BTCAtm : LFPG_BTCAtmBase
{
    // ---- Device-specific SyncVar ----
    protected bool m_PoweredNet = false;

    // ============================================
    // Constructor: port + SyncVar
    // ============================================
    void LFPG_BTCAtm()
    {
        string varPowered = "m_PoweredNet";
        RegisterNetSyncVariableBool(varPowered);

        string portName  = "input_1";
        int portDir      = LFPG_PortDir.IN;
        string portLabel = "Input 1";
        LFPG_AddPort(portName, portDir, portLabel);
    }

    // ============================================
    // Consumer overrides
    // ============================================
    override float LFPG_GetConsumption()
    {
        return LFPG_BTC_ATM_CONSUMPTION;
    }

    override bool LFPG_IsATMPowered()
    {
        return m_PoweredNet;
    }

    override bool LFPG_IsPowered()
    {
        return m_PoweredNet;
    }

    override void LFPG_SetPowered(bool powered)
    {
        #ifdef SERVER
        if (m_PoweredNet == powered)
            return;

        m_PoweredNet = powered;
        SetSynchDirty();
        #endif
    }

    // ============================================
    // Visual sync (client-side)
    // Screen: animation phase hides the "black cover" face to reveal the
    // baked emission underneath. LED: material swap (red/green rvmat).
    // ============================================
    override void LFPG_OnVarSync()
    {
        #ifndef SERVER
        int idxLed = 0;
        string animScreen = "HideScreen";

        if (m_PoweredNet)
        {
            SetAnimationPhase(animScreen, 1.0);
            SetObjectMaterial(idxLed, LFPG_BTC_RVMAT_LED_ON);
        }
        else
        {
            SetAnimationPhase(animScreen, 0.0);
            SetObjectMaterial(idxLed, LFPG_BTC_RVMAT_LED_OFF);
        }
        #endif
    }

    // ============================================
    // Power cleanup on kill / wire disconnect
    // ============================================
    override void LFPG_OnKilled()
    {
        super.LFPG_OnKilled();
        #ifdef SERVER
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        #endif
    }

    override void LFPG_OnWiresCut()
    {
        #ifdef SERVER
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            SetSynchDirty();
        }
        #endif
    }
};


// =========================================================
// ADMIN ATM: No power, no ports, always active
// =========================================================
class LFPG_BTCAtmAdmin : LFPG_BTCAtmBase
{
    // No extra SyncVars needed.
    // Visual: always green via config.cpp hiddenSelectionsMaterials default.
    // No LFPG_OnVarSync needed (state never changes).

    // ============================================
    // Constructor: no ports
    // ============================================
    void LFPG_BTCAtmAdmin()
    {
        // No ports — admin ATM has no electrical connections
    }

    // ============================================
    // Always powered
    // ============================================
    override bool LFPG_IsATMPowered()
    {
        return true;
    }

    override bool LFPG_IsPowered()
    {
        return true;
    }
};
