// =========================================================
// LF_PowerGrid - Remote Controller (v1.0.0)
//
// LFPG_RemoteController: Handheld item (Inventory_Base).
//   NOT a grid device — no ports, no wires, no consumption.
//   Autonomous: always functional, no power required.
//
// Pairing: Player holds remote + looks at RF-capable switch
//   → Action toggles pair/unpair.
//   Paired entries stored as DeviceId + position.
//   Auto-unpair if device moved beyond tolerance.
//
// Activate: Player holds remote, action (no target) toggles
//   all paired switches within configurable range.
//
// Persistence: OnStoreSave/OnStoreLoad with version header.
//   Format: [version][count][{deviceId,posX,posY,posZ}...]
//
// Client sync: After each pair/unpair and after OnStoreLoad,
//   server sends paired ID list via ScriptRPC to owning client.
//   Client stores IDs in m_ClientPairedIds for ActionCondition
//   dynamic text (Pair/Unpair).
//
// Visual feedback:
//   button_1 + button_1_led = pair/unpair button (red LED)
//   button_2 + button_2_led = activate button (green LED)
//   LED off = led_off.rvmat (default)
// =========================================================

// --- Constants ---
static const float LFPG_REMOTE_RANGE_M         = 200.0;
static const float LFPG_REMOTE_RANGE_SQ         = 40000.0;
static const float LFPG_REMOTE_POS_TOLERANCE     = 1.0;
static const float LFPG_REMOTE_POS_TOLERANCE_SQ  = 1.0;
static const int   LFPG_REMOTE_COOLDOWN_MS       = 1000;
static const int   LFPG_REMOTE_LED_DURATION_MS   = 2000;
static const int   LFPG_REMOTE_BTN_DURATION_MS   = 300;
static const int   LFPG_REMOTE_PERSIST_VER       = 1;
static const int   LFPG_REMOTE_SYNC_DELAY_MS     = 2000;

// hiddenSelections indices
static const int   LFPG_RC_HS_LED1 = 0;
static const int   LFPG_RC_HS_LED2 = 1;

// rvmat paths
static const string LFPG_RC_RVMAT_OFF   = "\\LFPowerGrid\\data\\remote_controller\\data\\led_off.rvmat";
static const string LFPG_RC_RVMAT_RED   = "\\LFPowerGrid\\data\\remote_controller\\data\\remote_control_red.rvmat";
static const string LFPG_RC_RVMAT_GREEN = "\\LFPowerGrid\\data\\remote_controller\\data\\remote_control_green.rvmat";

// RPC type for paired list sync (entity-level, not PlayerRPC)
static const int LFPG_RPC_REMOTE_PAIR_SYNC = 0x4C465250;

// --- Paired entry data class ---
class LFPG_PairedEntry : Managed
{
    string m_DeviceId;
    float  m_PosX;
    float  m_PosY;
    float  m_PosZ;

    void LFPG_PairedEntry()
    {
        m_DeviceId = "";
        m_PosX = 0.0;
        m_PosY = 0.0;
        m_PosZ = 0.0;
    }
};

// --- Remote Controller item ---
class LFPG_RemoteController : Inventory_Base
{
    // Paired devices (server-authoritative, full data)
    protected ref array<ref LFPG_PairedEntry> m_PairedEntries;

    // Client mirror (IDs only, synced via RPC for ActionCondition text)
    protected ref array<string> m_ClientPairedIds;

    // Cooldown (server only)
    protected int m_LastActivateTime;

    // ============================================
    // Constructor
    // ============================================
    void LFPG_RemoteController()
    {
        m_PairedEntries = new array<ref LFPG_PairedEntry>;
        m_ClientPairedIds = new array<string>;
        m_LastActivateTime = 0;
    }

    // ============================================
    // Actions
    // ============================================
    override void SetActions()
    {
        super.SetActions();
        AddAction(LFPG_ActionPairRemote);
        AddAction(LFPG_ActionActivateRemote);
    }

    // ============================================
    // IsPaired — works on BOTH server and client
    // ============================================
    bool LFPG_IsPaired(string deviceId)
    {
        #ifdef SERVER
        int i;
        int count = m_PairedEntries.Count();
        for (i = 0; i < count; i = i + 1)
        {
            LFPG_PairedEntry entry = m_PairedEntries[i];
            if (entry && entry.m_DeviceId == deviceId)
            {
                return true;
            }
        }
        return false;
        #else
        int ci;
        int ccount = m_ClientPairedIds.Count();
        for (ci = 0; ci < ccount; ci = ci + 1)
        {
            string cid = m_ClientPairedIds[ci];
            if (cid == deviceId)
            {
                return true;
            }
        }
        return false;
        #endif
    }

    // ============================================
    // Pairing API (server only)
    // ============================================
    void LFPG_PairDevice(string deviceId, vector pos)
    {
        #ifdef SERVER
        if (LFPG_IsPaired(deviceId))
            return;

        LFPG_PairedEntry entry = new LFPG_PairedEntry();
        entry.m_DeviceId = deviceId;
        entry.m_PosX = pos[0];
        entry.m_PosY = pos[1];
        entry.m_PosZ = pos[2];
        m_PairedEntries.Insert(entry);

        string pairMsg = "[LFPG_RemoteController] Paired deviceId=";
        pairMsg = pairMsg + deviceId;
        pairMsg = pairMsg + " total=";
        pairMsg = pairMsg + m_PairedEntries.Count().ToString();
        LFPG_Util.Info(pairMsg);
        #endif
    }

    void LFPG_UnpairDevice(string deviceId)
    {
        #ifdef SERVER
        int i;
        int count = m_PairedEntries.Count();
        for (i = 0; i < count; i = i + 1)
        {
            LFPG_PairedEntry entry = m_PairedEntries[i];
            if (entry && entry.m_DeviceId == deviceId)
            {
                m_PairedEntries.Remove(i);

                string unpairMsg = "[LFPG_RemoteController] Unpaired deviceId=";
                unpairMsg = unpairMsg + deviceId;
                unpairMsg = unpairMsg + " remaining=";
                unpairMsg = unpairMsg + m_PairedEntries.Count().ToString();
                LFPG_Util.Info(unpairMsg);
                return;
            }
        }
        #endif
    }

    int LFPG_GetPairedCount()
    {
        #ifdef SERVER
        return m_PairedEntries.Count();
        #else
        return m_ClientPairedIds.Count();
        #endif
    }

    // ============================================
    // RPC: Sync paired IDs to owning client
    // ============================================
    void LFPG_SyncPairedListToClient(PlayerIdentity recipient)
    {
        #ifdef SERVER
        if (!recipient)
            return;

        ScriptRPC rpc = new ScriptRPC();

        int count = m_PairedEntries.Count();
        rpc.Write(count);

        int i;
        for (i = 0; i < count; i = i + 1)
        {
            LFPG_PairedEntry entry = m_PairedEntries[i];
            if (entry)
            {
                rpc.Write(entry.m_DeviceId);
            }
            else
            {
                string emptyId = "";
                rpc.Write(emptyId);
            }
        }

        rpc.Send(this, LFPG_RPC_REMOTE_PAIR_SYNC, true, recipient);

        string syncMsg = "[LFPG_RemoteController] SyncPairedList -> client, count=";
        syncMsg = syncMsg + count.ToString();
        LFPG_Util.Debug(syncMsg);
        #endif
    }

    // Send to whoever currently holds this item
    void LFPG_SyncToOwner()
    {
        #ifdef SERVER
        PlayerBase owner = PlayerBase.Cast(GetHierarchyRootPlayer());
        if (!owner)
            return;

        PlayerIdentity identity = owner.GetIdentity();
        if (!identity)
            return;

        LFPG_SyncPairedListToClient(identity);
        #endif
    }

    // ============================================
    // OnRPC — receive paired list on client
    // ============================================
    override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
    {
        super.OnRPC(sender, rpc_type, ctx);

        if (rpc_type != LFPG_RPC_REMOTE_PAIR_SYNC)
            return;

        #ifndef SERVER
        int count;
        if (!ctx.Read(count))
            return;

        m_ClientPairedIds.Clear();

        int i;
        for (i = 0; i < count; i = i + 1)
        {
            string devId;
            if (!ctx.Read(devId))
                break;

            if (devId != "")
            {
                m_ClientPairedIds.Insert(devId);
            }
        }

        string rcvMsg = "[LFPG_RemoteController] Client received paired list: ";
        rcvMsg = rcvMsg + m_ClientPairedIds.Count().ToString();
        rcvMsg = rcvMsg + " entries";
        LFPG_Util.Debug(rcvMsg);
        #endif
    }

    // ============================================
    // Activate: toggle all paired in range
    // ============================================
    int LFPG_ActivateToggle(vector playerPos)
    {
        #ifdef SERVER
        int now = GetGame().GetTime();
        int elapsed = now - m_LastActivateTime;
        if (elapsed < LFPG_REMOTE_COOLDOWN_MS)
            return -1;

        m_LastActivateTime = now;

        int toggled = 0;
        int removed = 0;
        bool listChanged = false;

        // Iterate backwards for safe removal
        int i = m_PairedEntries.Count() - 1;
        while (i >= 0)
        {
            LFPG_PairedEntry entry = m_PairedEntries[i];
            if (!entry)
            {
                m_PairedEntries.Remove(i);
                listChanged = true;
                i = i - 1;
                continue;
            }

            // Resolve device from registry
            string devId = entry.m_DeviceId;
            EntityAI device = LFPG_DeviceRegistry.Get().Find(devId);

            if (!device)
            {
                // Device no longer exists — auto-unpair
                m_PairedEntries.Remove(i);
                removed = removed + 1;
                listChanged = true;
                i = i - 1;
                continue;
            }

            // Position check — auto-unpair if moved
            vector devPos = device.GetPosition();
            vector storedPos;
            storedPos[0] = entry.m_PosX;
            storedPos[1] = entry.m_PosY;
            storedPos[2] = entry.m_PosZ;

            float posDist = LFPG_WorldUtil.DistSq(devPos, storedPos);
            if (posDist > LFPG_REMOTE_POS_TOLERANCE_SQ)
            {
                m_PairedEntries.Remove(i);
                removed = removed + 1;
                listChanged = true;

                string movedMsg = "[LFPG_RemoteController] Auto-unpair (moved) deviceId=";
                movedMsg = movedMsg + devId;
                LFPG_Util.Info(movedMsg);

                i = i - 1;
                continue;
            }

            // Range check from player
            float rangeDist = LFPG_WorldUtil.DistSq(playerPos, devPos);
            if (rangeDist > LFPG_REMOTE_RANGE_SQ)
            {
                i = i - 1;
                continue;
            }

            // RF capability check (safety)
            if (!LFPG_DeviceAPI.IsRFCapable(device))
            {
                i = i - 1;
                continue;
            }

            // Toggle!
            bool result = LFPG_DeviceAPI.RemoteToggle(device);
            if (result)
            {
                toggled = toggled + 1;
            }

            i = i - 1;
        }

        string actMsg = "[LFPG_RemoteController] Activate: toggled=";
        actMsg = actMsg + toggled.ToString();
        actMsg = actMsg + " removed=";
        actMsg = actMsg + removed.ToString();
        actMsg = actMsg + " remaining=";
        actMsg = actMsg + m_PairedEntries.Count().ToString();
        LFPG_Util.Info(actMsg);

        // If auto-unpair removed entries, sync updated list to client
        if (listChanged)
        {
            LFPG_SyncToOwner();
        }

        return toggled;
        #else
        return 0;
        #endif
    }

    // ============================================
    // Visual feedback (button press + LED flash)
    // ============================================
    void LFPG_FlashLED1Red()
    {
        SetObjectMaterial(LFPG_RC_HS_LED1, LFPG_RC_RVMAT_RED);
        string animBtn1 = "activate_button_1";
        SetAnimationPhase(animBtn1, 1.0);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_ResetLED1, LFPG_REMOTE_LED_DURATION_MS, false);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_ResetBtn1, LFPG_REMOTE_BTN_DURATION_MS, false);
    }

    void LFPG_ResetLED1()
    {
        SetObjectMaterial(LFPG_RC_HS_LED1, LFPG_RC_RVMAT_OFF);
    }

    void LFPG_ResetBtn1()
    {
        string animBtn1 = "activate_button_1";
        SetAnimationPhase(animBtn1, 0.0);
    }

    void LFPG_FlashLED2Green()
    {
        SetObjectMaterial(LFPG_RC_HS_LED2, LFPG_RC_RVMAT_GREEN);
        string animBtn2 = "activate_button_2";
        SetAnimationPhase(animBtn2, 1.0);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_ResetLED2, LFPG_REMOTE_LED_DURATION_MS, false);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_ResetBtn2, LFPG_REMOTE_BTN_DURATION_MS, false);
    }

    void LFPG_ResetLED2()
    {
        SetObjectMaterial(LFPG_RC_HS_LED2, LFPG_RC_RVMAT_OFF);
    }

    void LFPG_ResetBtn2()
    {
        string animBtn2 = "activate_button_2";
        SetAnimationPhase(animBtn2, 0.0);
    }

    // ============================================
    // Persistence
    // ============================================
    override void OnStoreSave(ParamsWriteContext ctx)
    {
        super.OnStoreSave(ctx);

        ctx.Write(LFPG_REMOTE_PERSIST_VER);

        int count = m_PairedEntries.Count();
        ctx.Write(count);

        int i;
        for (i = 0; i < count; i = i + 1)
        {
            LFPG_PairedEntry entry = m_PairedEntries[i];
            if (entry)
            {
                ctx.Write(entry.m_DeviceId);
                ctx.Write(entry.m_PosX);
                ctx.Write(entry.m_PosY);
                ctx.Write(entry.m_PosZ);
            }
            else
            {
                string emptyId = "";
                ctx.Write(emptyId);
                ctx.Write(0.0);
                ctx.Write(0.0);
                ctx.Write(0.0);
            }
        }

        string saveMsg = "[LFPG_RemoteController] OnStoreSave: ";
        saveMsg = saveMsg + count.ToString();
        saveMsg = saveMsg + " entries";
        LFPG_Util.Debug(saveMsg);
    }

    override bool OnStoreLoad(ParamsReadContext ctx, int version)
    {
        if (!super.OnStoreLoad(ctx, version))
            return false;

        int persistVer;
        if (!ctx.Read(persistVer))
        {
            string errVer = "[LFPG_RemoteController] OnStoreLoad failed: persistVer";
            LFPG_Util.Error(errVer);
            return false;
        }

        int count;
        if (!ctx.Read(count))
        {
            string errCount = "[LFPG_RemoteController] OnStoreLoad failed: count";
            LFPG_Util.Error(errCount);
            return false;
        }

        m_PairedEntries.Clear();

        int i;
        for (i = 0; i < count; i = i + 1)
        {
            string devId;
            float px;
            float py;
            float pz;

            if (!ctx.Read(devId))
            {
                string errId = "[LFPG_RemoteController] OnStoreLoad failed: deviceId at index ";
                errId = errId + i.ToString();
                LFPG_Util.Error(errId);
                return false;
            }

            if (!ctx.Read(px))
                return false;
            if (!ctx.Read(py))
                return false;
            if (!ctx.Read(pz))
                return false;

            if (devId != "")
            {
                LFPG_PairedEntry entry = new LFPG_PairedEntry();
                entry.m_DeviceId = devId;
                entry.m_PosX = px;
                entry.m_PosY = py;
                entry.m_PosZ = pz;
                m_PairedEntries.Insert(entry);
            }
        }

        string loadMsg = "[LFPG_RemoteController] OnStoreLoad: ";
        loadMsg = loadMsg + m_PairedEntries.Count().ToString();
        loadMsg = loadMsg + " entries loaded";
        LFPG_Util.Debug(loadMsg);

        // Delayed sync to owner (player might not have identity yet during load)
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(LFPG_SyncToOwner, LFPG_REMOTE_SYNC_DELAY_MS, false);

        return true;
    }
};
