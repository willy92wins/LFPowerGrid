// =========================================================
// LF_PowerGrid - Sprinkler device (v5.4 Watering Fix)
//
// LFPG_Sprinkler_Kit:  Holdable, deployable (same-model pattern).
// LFPG_Sprinkler:      CONSUMER, 1 IN (input_0), 5 u/s, no wire store.
//
// v4.0: Migrated from Inventory_Base to LFPG_DeviceBase.
// v4.1: RegisterSprinkler/UnregisterSprinkler in NM.
// v5.2: Watering logic (GardenBase + SetWet).
// v5.3: Phase C — player wetting (rain parity). Wets clothing
//        attachments + cargo inside each attachment + item in hands.
//        Vanilla Environment detects wet clothing and updates
//        GetStatWet() / hypothermia on its own tick — no manual
//        stat manipulation needed.
// v5.4: LFPG_TickWatering integrated into NM TickSimpleDevices
//        (counter==8, ~10s). Radius 3m. GardenBaseMod removed
//        deprecated WaterAllSlots → GiveWater only (vanilla parity).
//
// Watering: LFPG_TickWatering() called by NM TickSimpleDevices ~10s.
//   - Phase A: GardenBase within radius → LFPG_WaterFromSprinkler()
//   - Phase B: ItemBase within radius → SetWet() increment
//   - Phase C: PlayerBase within radius → wet attachments + cargo
//   - Uses pre-allocated m_ arrays (no alloc in tick)
//
// Particle: PENDING — needs custom sprinkler_spray.ptc.
//   No vanilla looping water particle exists.
// =========================================================

// ---------------------------------------------------------
// KIT (unchanged)
// ---------------------------------------------------------

class LFPG_Sprinkler_Kit : LFPG_KitBase
{
    override string LFPG_GetSpawnClassname()
    {
        return "LFPG_Sprinkler";
    }
};

// ---------------------------------------------------------
// DEVICE - CONSUMER : LFPG_DeviceBase
// ---------------------------------------------------------
class LFPG_Sprinkler : LFPG_DeviceBase
{
    // ---- Device-specific SyncVars ----
    protected bool m_PoweredNet      = false;
    protected bool m_SprinklerActive = false;

    // ---- Server-only: upstream tracking (set by NetworkManager tick) ----
    protected bool   m_HasWaterSource = false;
    protected string m_WaterSourceId  = "";
    protected string m_SourcePort     = "";

    // ---- Server-only: reusable arrays for spatial query (no alloc in tick) ----
    protected ref array<Object>    m_WaterNearby;
    protected ref array<CargoBase> m_WaterCargos;

    // ---- Client: sound ----
    protected EffectSound m_LoopSound;

    void LFPG_Sprinkler()
    {
        string pIn = "input_0";
        string lIn = "Power Input";
        LFPG_AddPort(pIn, LFPG_PortDir.IN, lIn);

        string varPowered = "m_PoweredNet";
        RegisterNetSyncVariableBool(varPowered);
        string varActive = "m_SprinklerActive";
        RegisterNetSyncVariableBool(varActive);

        // Pre-allocate arrays for server-side spatial queries (server only)
        #ifdef SERVER
        m_WaterNearby = new array<Object>;
        m_WaterCargos = new array<CargoBase>;
        #endif
    }

    // ---- Actions (add sprinkler-specific) ----
    override void SetActions()
    {
        super.SetActions();
        AddAction(LFPG_ActionCheckSprinkler);
    }

    // ---- Virtual interface ----
    override int LFPG_GetDeviceType()
    {
        return LFPG_DeviceType.CONSUMER;
    }

    override float LFPG_GetConsumption()
    {
        return LFPG_SPRINKLER_CONSUMPTION;
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

        string msg = "[LFPG_Sprinkler] SetPowered(";
        msg = msg + powered.ToString();
        msg = msg + ") id=";
        msg = msg + m_DeviceId;
        LFPG_Util.Debug(msg);
        #endif
    }

    // ---- Sprinkler-specific state accessors (NM tick) ----
    bool LFPG_GetSprinklerActive()
    {
        return m_SprinklerActive;
    }

    void LFPG_SetSprinklerActive(bool active)
    {
        #ifdef SERVER
        if (m_SprinklerActive == active)
            return;

        m_SprinklerActive = active;
        SetSynchDirty();

        string msg = "[LFPG_Sprinkler] SetSprinklerActive(";
        msg = msg + active.ToString();
        msg = msg + ") id=";
        msg = msg + m_DeviceId;
        LFPG_Util.Debug(msg);
        #endif
    }

    bool LFPG_GetHasWaterSource()
    {
        return m_HasWaterSource;
    }

    void LFPG_SetHasWaterSource(bool has)
    {
        #ifdef SERVER
        m_HasWaterSource = has;
        #endif
    }

    string LFPG_GetWaterSourceId()
    {
        return m_WaterSourceId;
    }

    void LFPG_SetWaterSourceId(string id)
    {
        #ifdef SERVER
        m_WaterSourceId = id;
        #endif
    }

    string LFPG_GetSourcePort()
    {
        return m_SourcePort;
    }

    void LFPG_SetSourcePort(string port)
    {
        #ifdef SERVER
        m_SourcePort = port;
        #endif
    }

    bool LFPG_GetPoweredNet()
    {
        return m_PoweredNet;
    }

    // ---- Lifecycle hooks ----
    override void LFPG_OnInit()
    {
        LFPG_NetworkManager.Get().RegisterSprinkler(this);
    }

    override void LFPG_OnKilled()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterSprinkler(this);
        bool dirty = false;
        if (m_PoweredNet)
        {
            m_PoweredNet = false;
            dirty = true;
        }
        if (m_SprinklerActive)
        {
            m_SprinklerActive = false;
            dirty = true;
        }
        if (dirty)
        {
            SetSynchDirty();
        }
        if (m_WaterSourceId != "")
        {
            LFPG_NetworkManager.Get().LFPG_RefreshPumpSprinklerLink(m_WaterSourceId, m_DeviceId);
        }
        #endif

        #ifndef SERVER
        LFPG_CleanupClientFX();
        #endif
    }

    override void LFPG_OnDeleted()
    {
        #ifdef SERVER
        LFPG_NetworkManager.Get().UnregisterSprinkler(this);
        if (m_WaterSourceId != "")
        {
            LFPG_NetworkManager.Get().LFPG_RefreshPumpSprinklerLink(m_WaterSourceId, m_DeviceId);
        }
        #endif

        #ifndef SERVER
        LFPG_CleanupClientFX();
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

    // =========================================================
    // VarSync: sound toggle (CLIENT)
    // =========================================================
    override void LFPG_OnVarSync()
    {
        #ifndef SERVER
        // ---- Sound toggle ----
        if (m_SprinklerActive && !m_LoopSound)
        {
            string soundSet = LFPG_SPRINKLER_LOOP_SOUNDSET;
            m_LoopSound = SEffectManager.PlaySound(soundSet, GetPosition());
            if (m_LoopSound)
            {
                m_LoopSound.SetAutodestroy(false);
            }
        }
        if (!m_SprinklerActive && m_LoopSound)
        {
            m_LoopSound.SoundStop();
            m_LoopSound = null;
        }
        // TODO: custom sprinkler_spray.ptc particle toggle here
        #endif
    }

    // =========================================================
    // LFPG_TickWatering — SERVER: water gardens + wet items + wet players
    //
    // Called by NM TickSimpleDevices (counter==8, ~10s effective).
    // Only called when m_SprinklerActive == true (NM pre-filters).
    //
    // Phase A: Find GardenBase within radius → LFPG_WaterFromSprinkler()
    // Phase B: Find ItemBase within radius → SetWet() increment
    // Phase C: Find PlayerBase within radius → wet attachments + cargo
    //          (rain parity — vanilla Environment handles GetStatWet)
    // =========================================================
    void LFPG_TickWatering()
    {
        #ifdef SERVER
        vector sprPos = GetPosition();

        // Clear reusable arrays
        m_WaterNearby.Clear();
        m_WaterCargos.Clear();

        // Spatial query: all objects within radius
        float radius = LFPG_SPRINKLER_RADIUS;
        GetGame().GetObjectsAtPosition3D(sprPos, radius, m_WaterNearby, m_WaterCargos);

        int objCount = m_WaterNearby.Count();
        int i;
        Object obj;
        EntityAI ent;
        float distSq;
        float radiusSq = radius * radius;

        // Hoisted variables (Enforce: no declarations inside loop body)
        GardenBase garden;
        ItemBase item;
        float waterAmt;
        float curWet;
        float newWet;
        int gardensWatered = 0;
        int itemsWetted = 0;

        // Phase C hoisted variables (player wetting)
        PlayerBase player;
        GameInventory playerInv;
        int attCount;
        int attIdx;
        EntityAI att;
        ItemBase attItem;
        GameInventory attInv;
        CargoBase attCargo;
        int cargoCount;
        int cargoIdx;
        EntityAI cargoEnt;
        ItemBase cargoItem;
        ItemBase handsItem;
        int playersWetted = 0;

        for (i = 0; i < objCount; i = i + 1)
        {
            obj = m_WaterNearby[i];
            if (!obj)
                continue;

            // Skip self
            if (obj == this)
                continue;

            ent = EntityAI.Cast(obj);
            if (!ent)
                continue;

            // Precise 3D distance check (engine query can overshoot)
            distSq = LFPG_WorldUtil.DistSq(sprPos, ent.GetPosition());
            if (distSq > radiusSq)
                continue;

            // Phase A: GardenBase watering
            garden = GardenBase.Cast(ent);
            if (garden)
            {
                // Skip ruined gardens
                if (garden.IsRuined())
                    continue;

                waterAmt = LFPG_SPRINKLER_WATER_AMOUNT;
                garden.LFPG_WaterFromSprinkler(waterAmt);
                gardensWatered = gardensWatered + 1;
                continue;
            }

            // Phase B: ItemBase wetting (clothes, items on ground)
            item = ItemBase.Cast(ent);
            if (item)
            {
                curWet = item.GetWet();

                // Skip items already fully wet
                if (curWet >= 1.0)
                    continue;

                newWet = curWet + LFPG_SPRINKLER_WET_AMOUNT;
                if (newWet > 1.0)
                {
                    newWet = 1.0;
                }
                item.SetWet(newWet);
                itemsWetted = itemsWetted + 1;
                continue;
            }

            // Phase C: PlayerBase wetting (rain parity)
            // PlayerBase inherits ManBase → DayZCreature → EntityAI (NOT ItemBase).
            // Vanilla Environment.ProcessWetnessByRain iterates clothing
            // attachments and applies soaking. We replicate the same effect:
            //   1) Wet each clothing attachment
            //   2) Wet cargo items inside each attachment
            //   3) Wet item in hands
            // Vanilla Environment will detect wet clothing on its next tick
            // and update GetStatWet() / hypothermia automatically.
            player = PlayerBase.Cast(ent);
            if (player)
            {
                // Skip dead players (corpses still return as PlayerBase)
                if (!player.IsAlive())
                    continue;

                playerInv = player.GetInventory();
                if (!playerInv)
                    continue;

                // ---- Wet clothing attachments + their cargo ----
                attCount = playerInv.AttachmentCount();
                for (attIdx = 0; attIdx < attCount; attIdx = attIdx + 1)
                {
                    att = playerInv.GetAttachmentFromIndex(attIdx);
                    if (!att)
                        continue;

                    attItem = ItemBase.Cast(att);
                    if (!attItem)
                        continue;

                    // Wet the clothing piece itself
                    curWet = attItem.GetWet();
                    if (curWet < 1.0)
                    {
                        newWet = curWet + LFPG_SPRINKLER_WET_AMOUNT;
                        if (newWet > 1.0)
                        {
                            newWet = 1.0;
                        }
                        attItem.SetWet(newWet);
                    }

                    // Wet cargo items inside this attachment (rain parity)
                    attInv = att.GetInventory();
                    if (!attInv)
                        continue;

                    attCargo = attInv.GetCargo();
                    if (!attCargo)
                        continue;

                    cargoCount = attCargo.GetItemCount();
                    for (cargoIdx = 0; cargoIdx < cargoCount; cargoIdx = cargoIdx + 1)
                    {
                        cargoEnt = attCargo.GetItem(cargoIdx);
                        if (!cargoEnt)
                            continue;

                        cargoItem = ItemBase.Cast(cargoEnt);
                        if (!cargoItem)
                            continue;

                        curWet = cargoItem.GetWet();
                        if (curWet >= 1.0)
                            continue;

                        newWet = curWet + LFPG_SPRINKLER_WET_AMOUNT;
                        if (newWet > 1.0)
                        {
                            newWet = 1.0;
                        }
                        cargoItem.SetWet(newWet);
                    }
                }

                // ---- Wet item in hands ----
                handsItem = player.GetItemInHands();
                if (handsItem)
                {
                    curWet = handsItem.GetWet();
                    if (curWet < 1.0)
                    {
                        newWet = curWet + LFPG_SPRINKLER_WET_AMOUNT;
                        if (newWet > 1.0)
                        {
                            newWet = 1.0;
                        }
                        handsItem.SetWet(newWet);
                    }
                }

                playersWetted = playersWetted + 1;
                continue;
            }
        }

        // Debug log OUTSIDE loop (1 string per tick, not per object)
        if (gardensWatered > 0 || itemsWetted > 0 || playersWetted > 0)
        {
            string tickLog = "[Sprinkler] Watered gardens=";
            tickLog = tickLog + gardensWatered.ToString();
            tickLog = tickLog + " items=";
            tickLog = tickLog + itemsWetted.ToString();
            tickLog = tickLog + " players=";
            tickLog = tickLog + playersWetted.ToString();
            LFPG_Util.Debug(tickLog);
        }
        #endif
    }

    // ---- Client FX cleanup ----
    protected void LFPG_CleanupClientFX()
    {
        if (m_LoopSound)
        {
            m_LoopSound.SoundStop();
            m_LoopSound = null;
        }
    }

    // ---- No extra persistence (CONSUMER: ids + deviceVer only) ----
};
