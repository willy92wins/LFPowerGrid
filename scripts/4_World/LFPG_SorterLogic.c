// =========================================================
// LF_PowerGrid - Sorter Logic (v1.2.0 Sprint S3)
//
// Server-side sorting logic used by LFPG_TickSorters
// (NetworkManager) and SORTER_REQUEST_SORT RPC handler.
//
// All methods are static helpers — no state.
//
// Functions:
//   EvaluateItem       — two-pass filter: rules then catch-all
//   MatchesAnyRule     — OR logic across rules in one output
//   MatchRule          — single rule evaluation
//   ResolveCategory    — EntityAI → category string (IsKindOf)
//   GetItemSlotSize    — CfgVehicles itemSize lookup (area)
//   ResolveOutputContainer — wire topology → dest container
//   MoveItemToContainer — server-authoritative inventory move
//   BinPackCargo       — greedy 2D bin-packing for SORT RPC
//
// Enforce Script rules:
//   No foreach, no ++/--, no ternario, no +=/-=
//   Variables hoisted before conditionals
//   Incremental string concat
// =========================================================

class LFPG_SorterLogic
{
    // =========================================================
    // Sprint S3.1 — Container accessibility guards (4-layer)
    // =========================================================
    // Layer 0: VSM (Virtual Storage Module) detection.
    //          #ifdef VSM compile-time conditional.
    //          Blocks interaction with VSM containers that are
    //          closed (items on disk) or processing (batch queue).
    //          Covers ALL VSM containers including doorless ones
    //          (SeaChest, WoodenCrate, etc.) that Layer 3 misses.
    //
    // Layer 1: CanReceiveItemIntoCargo / CanReleaseCargo (vanilla API).
    //          CodeLock hooks TentBase here. Any mod that overrides
    //          these methods is covered automatically.
    //
    // Layer 2: CodeLock direct detection WITHOUT compile-time dep.
    //          FindAttachmentBySlotName("Att_CombinationLock") +
    //          !IsTakeable() = locked.
    //
    // Layer 3: Door animation phase for vanilla furniture.
    //          Barrel_ColorBase, TentBase, Fence.
    //          Unknown types → always accessible (no false positives).
    // =========================================================

    // ---------------------------------------------------------
    // CanTakeFromContainer: checks if the Sorter can READ items
    // from this container (source side).
    // Item param is used for CanReleaseCargo check — can be null
    // for a general "is accessible" query.
    // ---------------------------------------------------------
    static bool CanTakeFromContainer(EntityAI container, EntityAI item)
    {
        if (!container)
            return false;

        // Layer 0: VSM virtual storage detection (compile-time conditional)
        if (IsVSMBlocked(container))
            return false;

        // Layer 1: Vanilla API — mods override this
        if (item)
        {
            if (!container.CanReleaseCargo(item))
                return false;
        }

        // Layer 2: CodeLock direct detection (no compile dependency)
        if (HasLockedCodeLock(container))
            return false;

        // Layer 3: Door animation — if container has a door and it's closed, skip
        if (IsDoorClosed(container))
            return false;

        return true;
    }

    // ---------------------------------------------------------
    // CanPutIntoContainer: checks if the Sorter can WRITE items
    // into this container (destination side).
    // ---------------------------------------------------------
    static bool CanPutIntoContainer(EntityAI container, EntityAI item)
    {
        if (!container)
            return false;

        // Layer 0: VSM virtual storage detection (compile-time conditional)
        if (IsVSMBlocked(container))
            return false;

        // Layer 1: Vanilla API — mods override this
        if (item)
        {
            if (!container.CanReceiveItemIntoCargo(item))
                return false;
        }

        // Layer 2: CodeLock direct detection (no compile dependency)
        if (HasLockedCodeLock(container))
            return false;

        // Layer 3: Door animation — if container has a door and it's closed, skip
        if (IsDoorClosed(container))
            return false;

        return true;
    }

    // ---------------------------------------------------------
    // IsVSMBlocked: detects Virtual Storage Module containers.
    //
    // Uses #ifdef VSM (compile-time conditional).
    // VSM defines "VSM" in Scripts/Common/vsm.c.
    // If VSM is not loaded, this method always returns false
    // and has ZERO overhead (compiled out entirely).
    //
    // When VSM IS loaded, checks:
    //   1. VSM_IsVirtualStorage() — is this a VSM-managed container?
    //   2. VSM_IsOpen() — is the container physically open?
    //   3. VSM_IsProcessing() — is VSM currently restoring/virtualizing?
    //
    // A VSM container is only accessible when:
    //   - It IS a virtual storage AND
    //   - It IS open (items spawned from disk) AND
    //   - It is NOT processing (batch restore/virtualize complete)
    //
    // Closed VSM containers have empty cargo (items on disk).
    // Putting items into a closed VSM container would cause them
    // to be lost when VSM virtualizes on next close cycle, or to
    // coexist with restored items causing cargo overflow.
    // ---------------------------------------------------------
    protected static bool IsVSMBlocked(EntityAI container)
    {
        #ifdef VSM
        ItemBase ib = ItemBase.Cast(container);
        if (ib)
        {
            if (ib.VSM_IsVirtualStorage())
            {
                // VSM container: only accessible when open AND not processing
                if (!ib.VSM_IsOpen())
                    return true;

                if (ib.VSM_IsProcessing())
                    return true;
            }
        }
        #endif

        return false;
    }

    // ---------------------------------------------------------
    // HasLockedCodeLock: detects a locked CodeLock attachment
    // WITHOUT referencing the CodeLock class (zero compile-time
    // dependency on the CodeLock mod).
    //
    // Detection logic:
    //   1. FindAttachmentBySlotName("Att_CombinationLock")
    //      → null if no lock attached (or slot doesn't exist)
    //   2. !lockAtt.IsTakeable()
    //      → CodeLock calls SetTakeable(false) when locking
    //      → SetTakeable(true) + drop when unlocking
    //   3. So: attachment exists AND not takeable = LOCKED
    //
    // Also checks vanilla CombinationLock which uses the same
    // slot name and similar lock semantics.
    // ---------------------------------------------------------
    protected static bool HasLockedCodeLock(EntityAI container)
    {
        if (!container)
            return false;

        if (!container.GetInventory())
            return false;

        // CodeLock and vanilla CombinationLock both use this slot
        string slotName = "Att_CombinationLock";
        EntityAI lockAtt = container.FindAttachmentBySlotName(slotName);
        if (!lockAtt)
            return false;

        // When locked: SetTakeable(false). When unlocked: dropped to ground.
        // If it's still attached and not takeable → locked.
        if (!lockAtt.IsTakeable())
            return true;

        return false;
    }

    // ---------------------------------------------------------
    // IsDoorClosed: checks common door animation phases on
    // vanilla DayZ containers and furniture.
    //
    // Animation sources checked (in order):
    //   "Doors"  — barrels, many modded containers
    //   "Doors1" — double-door containers (left)
    //   "Doors2" — double-door containers (right)
    //   "Door"   — some modded furniture
    //   "lid"    — crates, ammo boxes
    //
    // Phase semantics: 0.0 = fully closed, 1.0 = fully open.
    // We consider < 0.5 as "closed".
    //
    // If NONE of these animation sources exist on the model,
    // the container is treated as always-open (no door to check).
    // This avoids false positives on containers without doors
    // (e.g. open shelves, ground stashes, simple crates).
    // ---------------------------------------------------------
    protected static bool IsDoorClosed(EntityAI container)
    {
        if (!container)
            return false;

        // Check known container types with door animations.
        // Only check animation phase for types we KNOW have doors.
        // Unknown types → treated as always-open (no false positives).
        float phase;

        string kBarrel = "Barrel_ColorBase";
        string kDoors = "Doors";
        string kTent = "TentBase";
        string kEntrance = "EntranceO";
        string kFence = "Fence";
        string kDoors1 = "Doors1";

        // Barrel_ColorBase: "Doors" animation, phase 0=closed, 1=open
        if (container.IsKindOf(kBarrel))
        {
            phase = container.GetAnimationPhase(kDoors);
            if (phase < 0.5)
                return true;
        }

        // TentBase: "EntranceO" animation.
        // CodeLock TentBase.GetDoorAnimState() checks GetAnimationPhase("EntranceO"):
        //   if truthy (non-zero) → entrance closed → return false (not accessible)
        if (container.IsKindOf(kTent))
        {
            phase = container.GetAnimationPhase(kEntrance);
            if (phase > 0.5)
                return true;
        }

        // Fence: gate door. "Doors1" phase 0=closed, 1=open.
        // Fence with CodeLock blocks CanOpenFence() but we check directly.
        if (container.IsKindOf(kFence))
        {
            phase = container.GetAnimationPhase(kDoors1);
            if (phase < 0.5)
                return true;
        }

        // SeaChest, WoodenCrate, etc: no door animations → always accessible.
        // Modded containers with custom doors will be caught by Layer 1
        // (CanReceiveItemIntoCargo) if the mod overrides it.

        return false;
    }

    // ---------------------------------------------------------
    // EvaluateItem: returns output index 0-5, or -1 if no match.
    // Pass 1: outputs with rules (skip catch-all, skip wireless).
    // Pass 2: first catch-all output WITH wire wins.
    // hasWireMask: bitmask where bit N = output N has wire.
    //   Bits: output_1=1, output_2=2, output_3=4, output_4=8, output_5=16, output_6=32
    //   Pass 63 (all bits set) to evaluate without wire filtering.
    // ---------------------------------------------------------
    static int EvaluateItem(EntityAI item, LFPG_SortConfig config, int hasWireMask)
    {
        if (!item)
            return -1;

        if (!config)
            return -1;

        int oi;
        LFPG_SortOutputConfig outCfg;
        int bitCheck;

        // Pass 1: rule-based outputs (skip catch-all, skip wireless)
        bitCheck = 1;
        for (oi = 0; oi < 6; oi = oi + 1)
        {
            if ((hasWireMask & bitCheck) == 0)
            {
                bitCheck = bitCheck * 2;
                continue;
            }

            outCfg = config.GetOutput(oi);
            if (!outCfg)
            {
                bitCheck = bitCheck * 2;
                continue;
            }

            if (outCfg.m_IsCatchAll)
            {
                bitCheck = bitCheck * 2;
                continue;
            }

            if (outCfg.GetRuleCount() == 0)
            {
                bitCheck = bitCheck * 2;
                continue;
            }

            if (MatchesAnyRule(item, outCfg))
                return oi;

            bitCheck = bitCheck * 2;
        }

        // Pass 2: catch-all outputs WITH wire (first one wins)
        bitCheck = 1;
        for (oi = 0; oi < 6; oi = oi + 1)
        {
            if ((hasWireMask & bitCheck) == 0)
            {
                bitCheck = bitCheck * 2;
                continue;
            }

            outCfg = config.GetOutput(oi);
            if (!outCfg)
            {
                bitCheck = bitCheck * 2;
                continue;
            }

            if (outCfg.m_IsCatchAll)
                return oi;

            bitCheck = bitCheck * 2;
        }

        return -1;
    }

    // ---------------------------------------------------------
    // MatchesAnyRule: OR logic — any matching rule returns true.
    // ---------------------------------------------------------
    static bool MatchesAnyRule(EntityAI item, LFPG_SortOutputConfig outCfg)
    {
        if (!outCfg)
            return false;

        if (!outCfg.m_Rules)
            return false;

        int ri;
        LFPG_SortFilterRule rule;

        for (ri = 0; ri < outCfg.m_Rules.Count(); ri = ri + 1)
        {
            rule = outCfg.m_Rules[ri];
            if (!rule)
                continue;

            if (MatchRule(item, rule))
                return true;
        }

        return false;
    }

    // ---------------------------------------------------------
    // MatchRule: evaluates a single filter rule against an item.
    // ---------------------------------------------------------
    static bool MatchRule(EntityAI item, LFPG_SortFilterRule rule)
    {
        if (!item)
            return false;

        if (!rule)
            return false;

        string typeName;
        string cat;
        string ruleLower;
        bool hasHardline;
        int slotSize;
        int dashPos;
        string minStr;
        string maxStr;
        int minVal;
        int maxVal;
        int remainLen;
        string kHardlineCfg = "CfgPatches ExpansionHardline";
        string kDash = "-";

        if (rule.m_Type == LFPG_SORT_FILTER_CATEGORY)
        {
            cat = ResolveCategory(item);
            if (cat == rule.m_Value)
                return true;

            return false;
        }

        if (rule.m_Type == LFPG_SORT_FILTER_PREFIX)
        {
            typeName = item.GetType();
            typeName.ToLower();
            ruleLower = rule.m_Value;
            ruleLower.ToLower();

            if (typeName.IndexOf(ruleLower) == 0)
                return true;

            return false;
        }

        if (rule.m_Type == LFPG_SORT_FILTER_CONTAINS)
        {
            typeName = item.GetType();
            typeName.ToLower();
            ruleLower = rule.m_Value;
            ruleLower.ToLower();

            if (typeName.IndexOf(ruleLower) >= 0)
                return true;

            return false;
        }

        if (rule.m_Type == LFPG_SORT_FILTER_RARITY)
        {
            // S-006: RARITY stub — not exposed in UI (no button creates type 3 rules).
            // Safe as-is. When implemented, requires Expansion-Hardline mod.
            hasHardline = GetGame().ConfigIsExisting(kHardlineCfg);
            if (!hasHardline)
                return false;

            // TODO: Implement rarity check when Hardline API is available
            return false;
        }

        if (rule.m_Type == LFPG_SORT_FILTER_SLOT)
        {
            slotSize = GetItemSlotSize(item);

            // Parse "min-max" from rule.m_Value
            dashPos = rule.m_Value.IndexOf(kDash);
            if (dashPos < 0)
                return false;

            minStr = rule.m_Value.Substring(0, dashPos);
            remainLen = rule.m_Value.Length() - dashPos - 1;
            if (remainLen <= 0)
                return false;

            maxStr = rule.m_Value.Substring(dashPos + 1, remainLen);
            minVal = minStr.ToInt();
            maxVal = maxStr.ToInt();

            if (slotSize >= minVal && slotSize <= maxVal)
                return true;

            return false;
        }

        return false;
    }

    // ---------------------------------------------------------
    // Category cache: typeName → category string.
    // Avoids repeated IsKindOf chains for items of the same type.
    protected static ref map<string, string> s_CategoryCache;

    // ---------------------------------------------------------
    // ResolveCategory: maps EntityAI to category string via
    // IsKindOf checks. Order matters — first match wins.
    // Cached per typeName for O(1) amortized lookups.
    // ---------------------------------------------------------
    static string ResolveCategory(EntityAI item)
    {
        if (!item)
            return LFPG_SORT_CAT_MISC;

        // Init cache on first call
        if (!s_CategoryCache)
        {
            s_CategoryCache = new map<string, string>;
        }

        string typeName = item.GetType();
        if (s_CategoryCache.Contains(typeName))
        {
            return s_CategoryCache.Get(typeName);
        }

        string result = ResolveCategoryUncached(item);
        s_CategoryCache.Set(typeName, result);
        return result;
    }

    // ---------------------------------------------------------
    // ResolveCategoryUncached: actual IsKindOf chain.
    // Called once per typeName, result is cached.
    // ---------------------------------------------------------
    protected static string ResolveCategoryUncached(EntityAI item)
    {
        if (item.IsWeapon())
            return LFPG_SORT_CAT_WEAPON;

        // All class name strings declared as local variables (Enforce Script rule)
        string k;

        k = "Magazine_Base";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_AMMO;

        // Weapon attachments — optics, suppressors, buttstocks, handguards, etc.
        k = "ItemOptics";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_ATTACHMENT;

        k = "ItemSuppressor";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_ATTACHMENT;

        // E11: Expanded attachment coverage — common vanilla weapon parts
        // that inherit directly from Inventory_Base (no shared base class).
        // Buttstocks
        k = "M4_OEBttstck";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_ATTACHMENT;
        k = "AK_WoodBttstck";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_ATTACHMENT;
        k = "AK_FoldingBttstck";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_ATTACHMENT;
        k = "AK_PlasticBttstck";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_ATTACHMENT;
        k = "Fal_OeBttstck";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_ATTACHMENT;
        // Handguards
        k = "M4_RISHndgrd";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_ATTACHMENT;
        k = "M4_PlasticHndgrd";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_ATTACHMENT;
        k = "AK_WoodHndgrd";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_ATTACHMENT;
        k = "AK_RailHndgrd";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_ATTACHMENT;
        k = "AK_PlasticHndgrd";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_ATTACHMENT;
        // Bayonets
        k = "Bayonet_Mosin";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_ATTACHMENT;
        k = "Bayonet_AK";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_ATTACHMENT;
        k = "Bayonet_SKS";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_ATTACHMENT;
        // Wraps/lights
        k = "GhillieSuit_ColorBase";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_ATTACHMENT;
        k = "UniversalLight";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_ATTACHMENT;
        k = "TLRLight";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_ATTACHMENT;

        k = "ItemGrenade";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_WEAPON;

        k = "Clothing_Base";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_CLOTHING;

        k = "Edible_Base";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_FOOD;

        k = "Bottle_Base";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_FOOD;

        k = "Bandage_Base";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_MEDICAL;

        k = "Morphine";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_MEDICAL;

        k = "Epinephrine";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_MEDICAL;

        k = "Saline";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_MEDICAL;

        k = "BloodBagBase";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_MEDICAL;

        k = "CharcoalTablets";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_MEDICAL;

        k = "Tetracycline";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_MEDICAL;

        k = "PainkillerTablets";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_MEDICAL;

        k = "VitaminBottle";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_MEDICAL;

        k = "DisinfectantSpray";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_MEDICAL;

        k = "IodineTincture";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_MEDICAL;

        k = "DisinfectantAlcohol";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_MEDICAL;

        // Tools
        k = "Toolbox";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_TOOL;

        k = "Wrench";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_TOOL;

        k = "Pliers";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_TOOL;

        k = "Hacksaw";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_TOOL;

        k = "Hammer";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_TOOL;

        k = "Shovel";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_TOOL;

        k = "Hatchet";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_TOOL;

        k = "Pickaxe";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_TOOL;

        k = "SewingKit";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_TOOL;

        k = "LeatherSewingKit";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_TOOL;

        k = "WhetStone";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_TOOL;

        k = "Lockpick";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_TOOL;

        k = "KitchenKnife";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_TOOL;

        k = "HuntingKnife";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_TOOL;

        k = "CombatKnife";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_TOOL;

        k = "Duct_Tape";
        if (item.IsKindOf(k))
            return LFPG_SORT_CAT_TOOL;

        return LFPG_SORT_CAT_MISC;
    }

    // ---------------------------------------------------------
    // GetItemSlotSize: returns area (sizeX * sizeY) from
    // CfgVehicles itemSize config. Minimum 1.
    // Delegates to GetItemSlotDimensions to avoid code duplication.
    // ---------------------------------------------------------
    static int GetItemSlotSize(EntityAI item)
    {
        int w;
        int h;
        GetItemSlotDimensions(item, w, h);
        return w * h;
    }

    // ---------------------------------------------------------
    // GetItemSlotDimensions: returns sizeX and sizeY via out params.
    // Used by bin-packing for 2D placement.
    // ---------------------------------------------------------
    static void GetItemSlotDimensions(EntityAI item, out int outW, out int outH)
    {
        outW = 1;
        outH = 1;

        if (!item)
            return;

        string cfgPrefix = "CfgVehicles ";
        string cfgSuffix = " itemSize";
        string cfgPath = cfgPrefix;
        cfgPath = cfgPath + item.GetType();
        cfgPath = cfgPath + cfgSuffix;
        string pathX;
        string pathY;
        string sZero = " 0";
        string sOne = " 1";

        if (GetGame().ConfigIsExisting(cfgPath))
        {
            pathX = cfgPath + sZero;
            pathY = cfgPath + sOne;
            outW = GetGame().ConfigGetInt(pathX);
            outH = GetGame().ConfigGetInt(pathY);
        }

        if (outW <= 0)
            outW = 1;

        if (outH <= 0)
            outH = 1;
    }

    // ---------------------------------------------------------
    // ResolveOutputContainer: given a Sorter and output index,
    // traverses wire topology to find the destination container.
    //   output index → output port → wire → target Sorter → its container
    // Returns null if wire/target missing.
    // ---------------------------------------------------------
    static EntityAI ResolveOutputContainer(LFPG_Sorter sorter, int outputIdx)
    {
        if (!sorter)
            return null;

        // 1. Build port name: output_1 .. output_6
        int portNum = outputIdx + 1;
        string portName = "output_" + portNum.ToString();

        // 2. Find wire from this port in Sorter's wire store
        array<ref LFPG_WireData> wires = sorter.LFPG_GetWires();
        if (!wires)
            return null;

        LFPG_WireData targetWire = null;
        int wi;
        for (wi = 0; wi < wires.Count(); wi = wi + 1)
        {
            if (!wires[wi])
                continue;

            if (wires[wi].m_SourcePort == portName)
            {
                targetWire = wires[wi];
                break;
            }
        }

        if (!targetWire)
            return null;

        // 3. Resolve target device (another LFPG_Sorter) by NetworkID
        EntityAI targetDev = LFPG_DeviceAPI.ResolveByNetworkId(targetWire.m_TargetNetLow, targetWire.m_TargetNetHigh);
        if (!targetDev)
            return null;

        LFPG_Sorter targetSorter = LFPG_Sorter.Cast(targetDev);
        if (!targetSorter)
            return null;

        // 4. Get the target Sorter's linked container
        return targetSorter.LFPG_GetLinkedContainer();
    }

    // ---------------------------------------------------------
    // MoveItemToContainer: server-authoritative inventory move.
    // Returns true if item was successfully moved.
    //
    // Pattern: FindFreeLocationFor → LocationSyncMoveEntity.
    // Explicit src→dst avoids double-lookup from TakeEntityToInventory.
    // ---------------------------------------------------------
    static bool MoveItemToContainer(EntityAI item, EntityAI destContainer)
    {
        if (!item)
            return false;

        if (!destContainer)
            return false;

        // S3.1: Check destination container is accessible
        if (!CanPutIntoContainer(destContainer, item))
            return false;

        GameInventory srcInv = item.GetInventory();
        if (!srcInv)
            return false;

        GameInventory destInv = destContainer.GetInventory();
        if (!destInv)
            return false;

        // Check if item fits in destination cargo
        InventoryLocation il_dst = new InventoryLocation;
        bool fits = destInv.FindFreeLocationFor(item, FindInventoryLocationType.CARGO, il_dst);
        if (!fits)
            return false;

        // Read current location
        InventoryLocation il_src = new InventoryLocation;
        srcInv.GetCurrentInventoryLocation(il_src);

        // Server-authoritative move: explicit src → dst
        bool moved = GameInventory.LocationSyncMoveEntity(il_src, il_dst);
        return moved;
    }

    // ---------------------------------------------------------
    // BinPackCargo: rearranges items in a cargo grid using
    // greedy largest-area-first bin-packing.
    //
    // 1. Collect all items and their sizes
    // 2. Sort by area descending
    // 3. Create a virtual 2D grid
    // 4. For each item, scan for first fit (try both orientations)
    // 5. Move items to their new grid positions
    //
    // Returns number of items successfully repositioned.
    // ---------------------------------------------------------
    static int BinPackCargo(EntityAI container)
    {
        #ifdef SERVER
        if (!container)
            return 0;

        // S3.1: Container must be accessible (not locked/closed/virtualized)
        if (!CanTakeFromContainer(container, null))
            return 0;

        GameInventory inv = container.GetInventory();
        if (!inv)
            return 0;

        CargoBase cargo = inv.GetCargo();
        if (!cargo)
            return 0;

        int gridW = cargo.GetWidth();
        int gridH = cargo.GetHeight();
        if (gridW <= 0 || gridH <= 0)
            return 0;

        // --- Phase 1: Collect all items with dimensions ---
        int totalItems = cargo.GetItemCount();
        if (totalItems <= 0)
            return 0;

        ref array<EntityAI> items = new array<EntityAI>;
        ref array<int> itemWidths = new array<int>;
        ref array<int> itemHeights = new array<int>;
        ref array<int> itemAreas = new array<int>;

        int ci;
        int iw;
        int ih;
        EntityAI cItem;

        for (ci = 0; ci < totalItems; ci = ci + 1)
        {
            cItem = cargo.GetItem(ci);
            if (!cItem)
                continue;

            GetItemSlotDimensions(cItem, iw, ih);
            items.Insert(cItem);
            itemWidths.Insert(iw);
            itemHeights.Insert(ih);
            itemAreas.Insert(iw * ih);
        }

        int n = items.Count();
        if (n <= 0)
            return 0;

        // --- Phase 2: Sort indices by area descending (insertion sort) ---
        ref array<int> sortedIdx = new array<int>;
        int si;
        for (si = 0; si < n; si = si + 1)
        {
            sortedIdx.Insert(si);
        }

        // Insertion sort — stable, simple, no Enforce issues
        int j;
        int keyIdx;
        int keyArea;
        for (si = 1; si < n; si = si + 1)
        {
            keyIdx = sortedIdx[si];
            keyArea = itemAreas[keyIdx];
            j = si - 1;

            while (j >= 0 && itemAreas[sortedIdx[j]] < keyArea)
            {
                sortedIdx[j + 1] = sortedIdx[j];
                j = j - 1;
            }
            sortedIdx[j + 1] = keyIdx;
        }

        // --- Phase 3: Create virtual 2D grid (row-major) ---
        // grid[row * gridW + col] = true if occupied
        int gridSize = gridW * gridH;
        ref array<bool> grid = new array<bool>;
        int gi;
        for (gi = 0; gi < gridSize; gi = gi + 1)
        {
            grid.Insert(false);
        }

        // --- Phase 4: Place items greedily ---
        ref array<int> placedRow = new array<int>;
        ref array<int> placedCol = new array<int>;
        ref array<bool> placedFlip = new array<bool>;
        ref array<bool> placedOk = new array<bool>;

        // Initialize placement arrays
        for (si = 0; si < n; si = si + 1)
        {
            placedRow.Insert(-1);
            placedCol.Insert(-1);
            placedFlip.Insert(false);
            placedOk.Insert(false);
        }

        int idx;
        int pw;
        int ph;
        bool placed;
        int row;
        int col;

        for (si = 0; si < n; si = si + 1)
        {
            idx = sortedIdx[si];
            pw = itemWidths[idx];
            ph = itemHeights[idx];
            placed = false;

            // Try original orientation
            placed = TryPlaceOnGrid(grid, gridW, gridH, pw, ph, placedRow, placedCol, idx);
            if (placed)
            {
                placedFlip[idx] = false;
                placedOk[idx] = true;
                MarkGridOccupied(grid, gridW, placedRow[idx], placedCol[idx], pw, ph);
            }

            // Try rotated orientation if original didn't fit
            if (!placed && pw != ph)
            {
                placed = TryPlaceOnGrid(grid, gridW, gridH, ph, pw, placedRow, placedCol, idx);
                if (placed)
                {
                    placedFlip[idx] = true;
                    placedOk[idx] = true;
                    MarkGridOccupied(grid, gridW, placedRow[idx], placedCol[idx], ph, pw);
                }
            }

            // Item doesn't fit → stays where it is (placedOk[idx] = false)
        }

        // --- Phase 5: Move items to their new grid positions ---
        // Strategy: first move ALL items to ground (clears the cargo grid),
        // then place each item at its computed position.
        // This avoids intra-cargo conflicts where item A's target cell
        // is occupied by item B that hasn't been moved yet.
        int repositioned = 0;
        InventoryLocation il_src;
        InventoryLocation il_ground;
        InventoryLocation il_dst;
        bool dropOk;
        bool placeOk;
        vector groundPos = container.GetPosition();
        // Offset slightly above ground to prevent clipping
        groundPos[1] = groundPos[1] + 0.05;

        // Build identity transform matrix with container position.
        // SetGround requires vector mat[4] (3×3 rotation + position).
        // mat[0]=right, mat[1]=up, mat[2]=forward, mat[3]=position.
        vector groundMat[4];
        Math3D.MatrixIdentity4(groundMat);
        groundMat[3] = groundPos;

        // Phase 5a: Move ALL items to ground (clears cargo grid entirely).
        // We must move ALL items, not just placedOk ones, because unplaced
        // items sitting at their original positions would block Phase 5b
        // SetCargo calls. Items that didn't fit (placedOk=false) will stay
        // on the ground near the container after Phase 5b completes.
        ref array<bool> movedToGround = new array<bool>;
        for (si = 0; si < n; si = si + 1)
        {
            movedToGround.Insert(false);
        }

        for (si = 0; si < n; si = si + 1)
        {
            cItem = items[si];
            if (!cItem)
                continue;

            if (!cItem.GetInventory())
                continue;

            il_src = new InventoryLocation;
            cItem.GetInventory().GetCurrentInventoryLocation(il_src);

            // Already on ground? Skip removal
            if (il_src.GetType() == InventoryLocationType.GROUND)
            {
                movedToGround[si] = true;
                continue;
            }

            il_ground = new InventoryLocation;
            il_ground.SetGround(cItem, groundMat);

            dropOk = GameInventory.LocationSyncMoveEntity(il_src, il_ground);
            if (dropOk)
            {
                movedToGround[si] = true;
            }
        }

        // Phase 5b: Place items back at computed cargo positions
        // Process in sorted order (largest first) to match virtual grid
        for (si = 0; si < n; si = si + 1)
        {
            idx = sortedIdx[si];

            if (!placedOk[idx])
                continue;

            if (!movedToGround[idx])
                continue;

            cItem = items[idx];
            if (!cItem)
                continue;

            if (!cItem.GetInventory())
                continue;

            il_src = new InventoryLocation;
            cItem.GetInventory().GetCurrentInventoryLocation(il_src);

            il_dst = new InventoryLocation;
            il_dst.SetCargo(container, cItem, 0, placedRow[idx], placedCol[idx], placedFlip[idx]);

            placeOk = GameInventory.LocationSyncMoveEntity(il_src, il_dst);
            if (placeOk)
            {
                repositioned = repositioned + 1;
            }
        }

        // Phase 5c: Fallback for items still on ground.
        // Items that didn't fit in virtual grid (!placedOk) or whose
        // SetCargo failed in Phase 5b are still on the ground.
        // Try to re-insert them using FindFreeLocationFor as best-effort.
        // If that also fails, item stays on ground near the container.
        for (si = 0; si < n; si = si + 1)
        {
            if (!movedToGround[si])
                continue;

            cItem = items[si];
            if (!cItem)
                continue;

            if (!cItem.GetInventory())
                continue;

            il_src = new InventoryLocation;
            cItem.GetInventory().GetCurrentInventoryLocation(il_src);

            // Only try fallback if item is still on ground
            if (il_src.GetType() != InventoryLocationType.GROUND)
                continue;

            // Best-effort: let the engine find any free cargo slot
            il_dst = new InventoryLocation;
            placeOk = inv.FindFreeLocationFor(cItem, FindInventoryLocationType.CARGO, il_dst);
            if (placeOk)
            {
                GameInventory.LocationSyncMoveEntity(il_src, il_dst);
            }
        }

        // Phase 5d: Force container network sync so clients refresh
        // their cached cargo grid. LocationSyncMoveEntity syncs
        // individual item positions but the container entity itself
        // needs a dirty signal for DayZ proximity-loot to re-read
        // the cargo layout — especially after the ground round-trip
        // (Phase 5a→5b) that produces many rapid sequential moves.
        container.SetSynchDirty();

        // v4.1 DIAG: Log total inventory operations for desync investigation.
        // 2×N ops (ground + reposition) is the suspected cause of client desync.
        string bpDiag = "[BinPackCargo] items=";
        bpDiag = bpDiag + n.ToString();
        bpDiag = bpDiag + " repositioned=";
        bpDiag = bpDiag + repositioned.ToString();
        bpDiag = bpDiag + " grid=";
        bpDiag = bpDiag + gridW.ToString();
        bpDiag = bpDiag + "x";
        bpDiag = bpDiag + gridH.ToString();
        LFPG_Util.Info(bpDiag);

        return repositioned;
        #else
        return 0;
        #endif
    }

    // ---------------------------------------------------------
    // TryPlaceOnGrid: scans the virtual grid for a free rect
    // of size w×h. Returns true if found, sets placedRow/Col.
    // Scan order: row-major (top-left to bottom-right).
    // ---------------------------------------------------------
    // v4.3: Changed from protected to public for BinPackSpread in NetworkManager
    static bool TryPlaceOnGrid(array<bool> grid, int gridW, int gridH, int itemW, int itemH, array<int> placedRow, array<int> placedCol, int idx)
    {
        if (itemW > gridW || itemH > gridH)
            return false;

        int maxRow = gridH - itemH;
        int maxCol = gridW - itemW;
        int row;
        int col;
        bool fits;
        int dr;
        int dc;
        int cellIdx;

        for (row = 0; row <= maxRow; row = row + 1)
        {
            for (col = 0; col <= maxCol; col = col + 1)
            {
                // Check if entire rect is free
                fits = true;
                for (dr = 0; dr < itemH; dr = dr + 1)
                {
                    for (dc = 0; dc < itemW; dc = dc + 1)
                    {
                        cellIdx = (row + dr) * gridW + (col + dc);
                        if (grid[cellIdx])
                        {
                            fits = false;
                            break;
                        }
                    }

                    if (!fits)
                        break;
                }

                if (fits)
                {
                    placedRow[idx] = row;
                    placedCol[idx] = col;
                    return true;
                }
            }
        }

        return false;
    }

    // ---------------------------------------------------------
    // MarkGridOccupied: marks a rect in the virtual grid.
    // ---------------------------------------------------------
    // v4.3: Changed from protected to public for BinPackSpread in NetworkManager
    static void MarkGridOccupied(array<bool> grid, int gridW, int row, int col, int itemW, int itemH)
    {
        int dr;
        int dc;
        int cellIdx;

        for (dr = 0; dr < itemH; dr = dr + 1)
        {
            for (dc = 0; dc < itemW; dc = dc + 1)
            {
                cellIdx = (row + dr) * gridW + (col + dc);
                grid[cellIdx] = true;
            }
        }
    }
};
