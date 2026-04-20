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
        m_BtcStock = stock;
        SetSynchDirty();
        #endif
    }

    bool LFPG_AddBtcStock(int amount)
    {
        #ifdef SERVER
        if (amount <= 0)
            return false;

        int maxStock = LFPG_BTCConfig.GetMaxBtcPerMachine();
        int newStock = m_BtcStock + amount;
        if (newStock > maxStock)
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

        m_BtcStock = m_BtcStock - amount;
        SetSynchDirty();
        return true;
        #else
        return false;
        #endif
    }

    // ============================================
    // ATMWithdrawOnly access
    // ============================================
    bool LFPG_IsWithdrawOnly()
    {
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
    // Lifecycle: init defaults from config
    // ============================================
    override void LFPG_OnInit()
    {
        #ifdef SERVER
        // On first creation (stock=0 and no persist load),
        // set withdrawOnly from config default.
        // (Persist load will overwrite if it runs after.)
        // This is safe because OnStoreLoad runs before EEInit.
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
