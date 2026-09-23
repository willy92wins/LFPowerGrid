class LFPG_SorterLogic
{
    static bool CanTakeFromContainer(EntityAI container, EntityAI item)
    {
        if (!container)
            return false;

        if (IsVSMBlocked(container))
            return false;

        if (item)
        {
            if (!container.CanReleaseCargo(item))
                return false;
        }

        if (HasLockedCodeLock(container))
            return false;

        if (IsDoorClosed(container))
            return false;

        return true;
    }

    static bool CanPutIntoContainer(EntityAI container, EntityAI item)
    {
        if (!container)
            return false;

        if (IsVSMBlocked(container))
            return false;

        if (item)
        {
            if (!container.CanReceiveItemIntoCargo(item))
                return false;
        }

        if (HasLockedCodeLock(container))
            return false;

        if (IsDoorClosed(container))
            return false;

        return true;
    }

    protected static bool IsVSMBlocked(EntityAI container)
    {
        #ifdef VSM
        ItemBase ib = ItemBase.Cast(container);
        if (ib)
        {
            if (ib.VSM_IsVirtualStorage())
            {
                if (!ib.VSM_IsOpen())
                    return true;

                if (ib.VSM_IsProcessing())
                    return true;
            }
        }
        #endif

        return false;
    }

    protected static bool HasLockedCodeLock(EntityAI container)
    {
        if (!container)
            return false;

        if (!container.GetInventory())
            return false;

        string slotName = "Att_CombinationLock";
        EntityAI lockAtt = container.FindAttachmentBySlotName(slotName);
        if (!lockAtt)
            return false;

        if (!lockAtt.IsTakeable())
            return true;

        return false;
    }

    protected static bool IsDoorClosed(EntityAI container)
    {
        if (!container)
            return false;

        float phase;

        string kBarrel = "Barrel_ColorBase";
        string kDoors = "Doors";
        string kTent = "TentBase";
        string kEntrance = "EntranceO";
        string kFence = "Fence";
        string kDoors1 = "Doors1";

        if (container.IsKindOf(kBarrel))
        {
            Barrel_ColorBase barrel = Barrel_ColorBase.Cast(container);
            if (!barrel)
                return true;
            if (!barrel.IsOpen())
                return true;
        }

        if (container.IsKindOf(kTent))
        {
            phase = container.GetAnimationPhase(kEntrance);
            if (phase > 0.5)
                return true;
        }

        if (container.IsKindOf(kFence))
        {
            phase = container.GetAnimationPhase(kDoors1);
            if (phase < 0.5)
                return true;
        }

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

    // Budgeted variant for the periodic scheduler. The cursor identifies the
    // next rule that has not yet been checked, so exhaustion defers work.
    static int EvaluateItemBudgeted(EntityAI item, LFPG_SortConfig config, int hasWireMask, int startOutput, int startRule, int budgetRemaining, out int nextOutput, out int nextRule, out int ruleChecks, out int configMisses, out bool deferred)
    {
        int oi;
        int ri;
        int ruleCount;
        int bitCheck;
        int ruleCost;
        int configCallCost;
        int totalSpent;
        LFPG_SortOutputConfig outputConfig;
        LFPG_SortFilterRule filterRule;
        bool dimensionCached;

        nextOutput = 0;
        nextRule = 0;
        ruleChecks = 0;
        configMisses = 0;
        deferred = false;

        if (!item)
            return -1;
        if (!config)
            return -1;
        if (budgetRemaining <= 0)
        {
            nextOutput = startOutput;
            nextRule = startRule;
            deferred = true;
            return -1;
        }
        if (startOutput < 0)
            startOutput = 0;
        if (startOutput > 5)
            startOutput = 5;
        if (startRule < 0)
            startRule = 0;

        for (oi = startOutput; oi < 6; oi = oi + 1)
        {
            bitCheck = 1 << oi;
            if ((hasWireMask & bitCheck) == 0)
                continue;

            outputConfig = config.GetOutput(oi);
            if (!outputConfig)
                continue;
            if (outputConfig.m_IsCatchAll)
                continue;
            if (!outputConfig.m_Rules)
                continue;

            ruleCount = outputConfig.m_Rules.Count();
            ri = 0;
            if (oi == startOutput)
                ri = startRule;
            if (ri > ruleCount)
                ri = ruleCount;

            while (ri < ruleCount)
            {
                filterRule = outputConfig.m_Rules[ri];
                if (!filterRule)
                {
                    ri = ri + 1;
                    continue;
                }

                ruleCost = 1;
                configCallCost = 0;
                dimensionCached = true;
                if (filterRule.m_Type == LFPG_SORT_FILTER_SLOT)
                {
                    dimensionCached = IsItemDimensionsCached(item);
                    if (!dimensionCached)
                        configCallCost = 3;
                }
                if (filterRule.m_Type == LFPG_SORT_FILTER_RARITY)
                    configCallCost = 1;
                ruleCost = ruleCost + configCallCost;

                totalSpent = ruleChecks + configMisses + ruleCost;
                if (totalSpent > budgetRemaining)
                {
                    nextOutput = oi;
                    nextRule = ri;
                    deferred = true;
                    return -1;
                }

                ruleChecks = ruleChecks + 1;
                configMisses = configMisses + configCallCost;

                if (MatchRule(item, filterRule))
                    return oi;
                ri = ri + 1;
            }
        }

        bitCheck = 1;
        for (oi = 0; oi < 6; oi = oi + 1)
        {
            if ((hasWireMask & bitCheck) != 0)
            {
                outputConfig = config.GetOutput(oi);
                if (outputConfig)
                {
                    if (outputConfig.m_IsCatchAll)
                        return oi;
                }
            }
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
		int minVal;
		int maxVal;
        string kHardlineCfg = "CfgPatches ExpansionHardline";

        if (rule.m_Type == LFPG_SORT_FILTER_CATEGORY)
        {
            cat = ResolveCategory(item);
            if (cat == rule.m_Value)
                return true;

            return false;
        }

        if (rule.m_Type == LFPG_SORT_FILTER_PREFIX)
        {
            typeName = GetLowerTypeName(item);
            ruleLower = rule.m_NormalizedValue;
            if (ruleLower == "")
            {
                ruleLower = rule.m_Value + "";
                ruleLower.ToLower();
            }

            if (typeName.IndexOf(ruleLower) == 0)
                return true;

            return false;
        }

        if (rule.m_Type == LFPG_SORT_FILTER_CONTAINS)
        {
            typeName = GetLowerTypeName(item);
            ruleLower = rule.m_NormalizedValue;
            if (ruleLower == "")
            {
                ruleLower = rule.m_Value + "";
                ruleLower.ToLower();
            }

            if (typeName.IndexOf(ruleLower) >= 0)
                return true;

            return false;
        }

        if (rule.m_Type == LFPG_SORT_FILTER_RARITY)
        {
            // S-006: RARITY stub — not exposed in UI (no button creates type 3 rules).
            // Safe as-is. When implemented, requires Expansion-Hardline mod.
            hasHardline = g_Game.ConfigIsExisting(kHardlineCfg);
            if (!hasHardline)
                return false;

            // TODO: Implement rarity check when Hardline API is available
            return false;
        }

        if (rule.m_Type == LFPG_SORT_FILTER_SLOT)
        {
            slotSize = GetItemSlotSize(item);

			if (!GetSlotRuleRange(rule.m_Value, minVal, maxVal))
				return false;

            if (slotSize >= minVal && slotSize <= maxVal)
                return true;

            return false;
        }

        return false;
    }

    // ---------------------------------------------------------
    // Immutable per-type caches. InitCaches is called by NetworkManager
    // construction so periodic sorter paths never allocate containers.
    protected static ref map<string, string> s_CategoryCache;
    protected static ref map<string, string> s_LowerTypeCache;
    protected static ref map<string, int> s_SlotWidthCache;
    protected static ref map<string, int> s_SlotHeightCache;
	protected static const int s_SlotRuleCacheLimit = 256;
	protected static ref map<string, int> s_SlotRuleMinCache;
	protected static ref map<string, int> s_SlotRuleMaxCache;

    static void InitCaches()
    {
        if (!s_CategoryCache)
            s_CategoryCache = new map<string, string>;
        if (!s_LowerTypeCache)
            s_LowerTypeCache = new map<string, string>;
        if (!s_SlotWidthCache)
            s_SlotWidthCache = new map<string, int>;
        if (!s_SlotHeightCache)
            s_SlotHeightCache = new map<string, int>;
		if (!s_SlotRuleMinCache)
			s_SlotRuleMinCache = new map<string, int>;
		if (!s_SlotRuleMaxCache)
			s_SlotRuleMaxCache = new map<string, int>;
    }

	// Key by value, not rule identity: edited/reloaded rules cannot reuse stale bounds.
	// No permissions, cargo capacity or topology are cached here.
	protected static bool GetSlotRuleRange(string value, out int minVal, out int maxVal)
	{
		bool canCache = s_SlotRuleMinCache && s_SlotRuleMaxCache && value.Length() <= 64;
		if (canCache && s_SlotRuleMinCache.Contains(value))
		{
			minVal = s_SlotRuleMinCache.Get(value);
			maxVal = s_SlotRuleMaxCache.Get(value);
			return minVal <= maxVal;
		}

		// An inverted interval caches malformed input as a non-match.
		minVal = 1;
		maxVal = 0;
		int dashPos = value.IndexOf("-");
		int remainLen = value.Length() - dashPos - 1;
		if (dashPos >= 0 && remainLen > 0)
		{
			string minStr = value.Substring(0, dashPos);
			string maxStr = value.Substring(dashPos + 1, remainLen);
			minVal = minStr.ToInt();
			maxVal = maxStr.ToInt();
		}

		if (canCache)
		{
			if (s_SlotRuleMinCache.Count() >= s_SlotRuleCacheLimit)
			{
				s_SlotRuleMinCache.Clear();
				s_SlotRuleMaxCache.Clear();
			}
			s_SlotRuleMinCache.Set(value, minVal);
			s_SlotRuleMaxCache.Set(value, maxVal);
		}
		return minVal <= maxVal;
	}

    protected static string GetLowerTypeName(EntityAI item)
    {
        if (!item)
            return "";

        string typeName = item.GetType();
        if (s_LowerTypeCache)
        {
            if (s_LowerTypeCache.Contains(typeName))
                return s_LowerTypeCache.Get(typeName);
        }

        string lowerType = typeName + "";
        lowerType.ToLower();
        if (s_LowerTypeCache)
            s_LowerTypeCache.Set(typeName, lowerType);
        return lowerType;
    }

    static bool IsItemDimensionsCached(EntityAI item)
    {
        if (!item)
            return true;
        if (!s_SlotWidthCache)
            return false;
        if (!s_SlotHeightCache)
            return false;

        string typeName = item.GetType();
        if (!s_SlotWidthCache.Contains(typeName))
            return false;
        if (!s_SlotHeightCache.Contains(typeName))
            return false;
        return true;
    }

    // ---------------------------------------------------------
    // ResolveCategory: maps EntityAI to category string via
    // IsKindOf checks. Order matters — first match wins.
    // Cached per typeName for O(1) amortized lookups.
    // ---------------------------------------------------------
    static string ResolveCategory(EntityAI item)
    {
        if (!item)
            return LFPG_SORT_CAT_MISC;

        string typeName = item.GetType();
        if (s_CategoryCache)
        {
            if (s_CategoryCache.Contains(typeName))
                return s_CategoryCache.Get(typeName);
        }

        string result = ResolveCategoryUncached(item);
        if (s_CategoryCache)
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
		// The ghillie suit is clothing; weapon lights remain attachments.
        k = "GhillieSuit_ColorBase";
        if (item.IsKindOf(k))
			return LFPG_SORT_CAT_CLOTHING;
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
        string typeName;
        string cfgPrefix;
        string cfgSuffix;
        string cfgPath;
        string pathX;
        string pathY;
        string sZero;
        string sOne;

        outW = 1;
        outH = 1;

        if (!item)
            return;

        typeName = item.GetType();
        if (s_SlotWidthCache)
        {
            if (s_SlotHeightCache)
            {
                if (s_SlotWidthCache.Contains(typeName) && s_SlotHeightCache.Contains(typeName))
                {
                    outW = s_SlotWidthCache.Get(typeName);
                    outH = s_SlotHeightCache.Get(typeName);
                    return;
                }
            }
        }

        cfgPrefix = "CfgVehicles ";
        cfgSuffix = " itemSize";
        cfgPath = cfgPrefix;
        cfgPath = cfgPath + typeName;
        cfgPath = cfgPath + cfgSuffix;
        sZero = " 0";
        sOne = " 1";

        if (g_Game.ConfigIsExisting(cfgPath))
        {
            pathX = cfgPath + sZero;
            pathY = cfgPath + sOne;
            outW = g_Game.ConfigGetInt(pathX);
            outH = g_Game.ConfigGetInt(pathY);
        }

        if (outW <= 0)
            outW = 1;
        if (outH <= 0)
            outH = 1;

        if (s_SlotWidthCache)
            s_SlotWidthCache.Set(typeName, outW);
        if (s_SlotHeightCache)
            s_SlotHeightCache.Set(typeName, outH);
    }

    // ---------------------------------------------------------
    // ResolveOutputContainer: given a Sorter and output index,
    // traverses wire topology to find the destination container.
    //   output index → output port → wire → target Sorter → its container
    // Returns null if wire/target missing.
    // ---------------------------------------------------------
	// Resolve the live link at use time; power and cargo permissions are separate.
	static EntityAI ResolveLinkedContainer(LFPG_Sorter sorter)
	{
		if (!sorter)
			return null;
		EntityAI container = sorter.LFPG_GetLinkedContainer();
		if (!container)
			return null;
		float distanceSq = LFPG_WorldUtil.DistSq(sorter.GetPosition(), container.GetPosition());
		float radiusSq = LFPG_SORTER_LINK_RADIUS * LFPG_SORTER_LINK_RADIUS;
		if (distanceSq <= radiusSq)
			return container;
		return null;
	}

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

		// S15: a resolved NetworkID alone does not keep a moved link in range.
		return ResolveLinkedContainer(targetSorter);
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

    // Periodic scheduler variant: caller owns both reusable locations.
    static bool MoveItemToContainerReusable(EntityAI item, EntityAI destContainer, InventoryLocation sourceLocation, InventoryLocation destinationLocation)
    {
        if (!item)
            return false;
        if (!destContainer)
            return false;
        if (!sourceLocation)
            return false;
        if (!destinationLocation)
            return false;
        if (!CanPutIntoContainer(destContainer, item))
            return false;

        GameInventory sourceInventory = item.GetInventory();
        if (!sourceInventory)
            return false;
        GameInventory destinationInventory = destContainer.GetInventory();
        if (!destinationInventory)
            return false;

        bool fits = destinationInventory.FindFreeLocationFor(item, FindInventoryLocationType.CARGO, destinationLocation);
        if (!fits)
            return false;
        sourceInventory.GetCurrentInventoryLocation(sourceLocation);
        return GameInventory.LocationSyncMoveEntity(sourceLocation, destinationLocation);
    }

    // ---------------------------------------------------------
    // RepackCargoInPlace: rearranges items in a cargo grid using
    // greedy largest-area-first bin-packing WITHOUT moving items
    // to the ground. Replaces BinPackCargo (v4.3) and
    // BinPackSpread (v4.3) which caused desync via ground
    // round-trip (items temporarily on ground = stealable,
    // race conditions with TickSorters, client flicker).
    //
    // Algorithm:
    //   1. Collect all items and their sizes
    //   2. Sort by area descending
    //   3. Compute optimal grid positions (virtual 2D bin-pack)
    //   4. Multi-pass in-place moves: each pass tries to move
    //      items whose target cell is free. Successful moves
	//      free cells for the next pass, within the fixed work limits.
    //   5. Items that cannot reach optimal position (deadlock)
    //      stay in their current cargo slot — no items on ground.
    //
    // Returns number of items successfully repositioned.
    // ---------------------------------------------------------
    static int RepackCargoInPlace(EntityAI container)
    {
        #ifdef SERVER
        if (!container)
            return 0;

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

        int totalItems = cargo.GetItemCount();
        if (totalItems <= 0)
            return 0;

		// Bound synchronous preparation before allocating or moving anything.
		// Large/modded cargo keeps its existing layout.
		if (totalItems > 64 || gridW > 1024 || gridH > 1024)
			return 0;
		if (gridW * gridH > 1024)
			return 0;
		int cellChecksRemaining = 16384;
		int moveChecksRemaining = 200;

		// --- Phase 1: Collect all items with dimensions ---
		array<EntityAI> items = new array<EntityAI>;
		array<int> itemWidths = new array<int>;
		array<int> itemHeights = new array<int>;
		array<int> itemAreas = new array<int>;

        int ci = 0;
        int iw = 0;
        int ih = 0;
        EntityAI cItem = null;

        for (ci = 0; ci < totalItems; ci = ci + 1)
        {
            cItem = cargo.GetItem(ci);
            if (!cItem)
                continue;

			GetItemSlotDimensions(cItem, iw, ih);
			if (iw <= 0 || ih <= 0 || iw > 1024 || ih > 1024)
				return 0;
            items.Insert(cItem);
            itemWidths.Insert(iw);
            itemHeights.Insert(ih);
            itemAreas.Insert(iw * ih);
        }

        int n = items.Count();
        if (n <= 0)
            return 0;

        // --- Phase 2: Sort indices by area descending (insertion sort) ---
		array<int> sortedIdx = new array<int>;
        int si = 0;
        for (si = 0; si < n; si = si + 1)
        {
            sortedIdx.Insert(si);
        }

        int j = 0;
        int keyIdx = 0;
        int keyArea = 0;
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
        int gridSize = gridW * gridH;
		array<bool> grid = new array<bool>;
        int gi = 0;
        for (gi = 0; gi < gridSize; gi = gi + 1)
        {
            grid.Insert(false);
        }

        // --- Phase 4: Compute optimal placements greedily ---
		array<int> placedRow = new array<int>;
		array<int> placedCol = new array<int>;
		array<bool> placedFlip = new array<bool>;
		array<bool> placedOk = new array<bool>;

        for (si = 0; si < n; si = si + 1)
        {
            placedRow.Insert(-1);
            placedCol.Insert(-1);
            placedFlip.Insert(false);
            placedOk.Insert(false);
        }

        int idx = 0;
        int pw = 0;
        int ph = 0;
        bool placed = false;

        for (si = 0; si < n; si = si + 1)
        {
            idx = sortedIdx[si];
            pw = itemWidths[idx];
            ph = itemHeights[idx];
            placed = false;

			placed = TryPlaceOnGridBudgeted(grid, gridW, gridH, pw, ph, placedRow, placedCol, idx, cellChecksRemaining);
			if (cellChecksRemaining <= 0)
				return 0;
            if (placed)
            {
                placedFlip[idx] = false;
                placedOk[idx] = true;
                MarkGridOccupied(grid, gridW, placedRow[idx], placedCol[idx], pw, ph);
            }

            if (!placed && pw != ph)
            {
				placed = TryPlaceOnGridBudgeted(grid, gridW, gridH, ph, pw, placedRow, placedCol, idx, cellChecksRemaining);
				if (cellChecksRemaining <= 0)
					return 0;
                if (placed)
                {
                    placedFlip[idx] = true;
                    placedOk[idx] = true;
                    MarkGridOccupied(grid, gridW, placedRow[idx], placedCol[idx], ph, pw);
                }
            }
        }

        // --- Phase 5: Multi-pass in-place moves ---
        // Each pass tries to move items to their optimal position.
        // If target cells are occupied by another item, the move
        // fails (LocationSyncMoveEntity returns false) and we retry
        // next pass. Successful moves free cells for subsequent items.
		// At most four passes and 200 item visits across all passes.
		// Unresolved items stay in place; completed moves remain valid.
		array<bool> done = new array<bool>;
        for (si = 0; si < n; si = si + 1)
        {
            done.Insert(false);
        }

        int repositioned = 0;
		int maxPasses = n;
		if (maxPasses > 4)
			maxPasses = 4;
        int pass = 0;
        bool progress = false;
		InventoryLocation ilSrc = new InventoryLocation;
		InventoryLocation ilDst = new InventoryLocation;
        bool moveOk = false;

		for (pass = 0; pass < maxPasses && moveChecksRemaining > 0; pass = pass + 1)
        {
            progress = false;

			for (si = 0; si < n && moveChecksRemaining > 0; si = si + 1)
			{
				moveChecksRemaining = moveChecksRemaining - 1;
				idx = sortedIdx[si];

                if (done[idx])
                    continue;

                if (!placedOk[idx])
                {
                    done[idx] = true;
                    continue;
                }

                cItem = items[idx];
                if (!cItem)
                {
                    done[idx] = true;
                    continue;
                }

                if (!cItem.GetInventory())
                {
                    done[idx] = true;
                    continue;
                }

				if (!cItem.GetInventory().GetCurrentInventoryLocation(ilSrc))
					continue;

				ilDst.SetCargo(container, cItem, 0, placedRow[idx], placedCol[idx], placedFlip[idx]);

				moveOk = GameInventory.LocationSyncMoveEntity(ilSrc, ilDst);
                if (moveOk)
                {
                    done[idx] = true;
                    repositioned = repositioned + 1;
                    progress = true;
                }
            }

            if (!progress)
                break;
        }

        // --- Phase 6: Force container network sync ---
        if (repositioned > 0)
        {
            container.SetSynchDirty();

            string rpLog = "[RepackCargoInPlace] items=";
            rpLog = rpLog + n.ToString();
            rpLog = rpLog + " repositioned=";
            rpLog = rpLog + repositioned.ToString();
            rpLog = rpLog + " passes=";
            rpLog = rpLog + pass.ToString();
            rpLog = rpLog + " grid=";
            rpLog = rpLog + gridW.ToString();
            rpLog = rpLog + "x";
            rpLog = rpLog + gridH.ToString();
            LFPG_Util.Info(rpLog);
        }

        return repositioned;
        #else
        return 0;
        #endif
    }

	// A shared cell-probe allowance covers both orientations and all items.
	protected static bool TryPlaceOnGridBudgeted(array<bool> grid, int gridW, int gridH, int itemW, int itemH, array<int> placedRow, array<int> placedCol, int idx, inout int checksRemaining)
	{
		if (itemW <= 0 || itemH <= 0 || itemW > gridW || itemH > gridH)
			return false;
		int row;
		int col;
		int dr;
		int dc;
		int cellIdx;
		bool fits;
		for (row = 0; row <= gridH - itemH; row = row + 1)
		{
			for (col = 0; col <= gridW - itemW; col = col + 1)
			{
				fits = true;
				for (dr = 0; dr < itemH; dr = dr + 1)
				{
					for (dc = 0; dc < itemW; dc = dc + 1)
					{
						if (checksRemaining <= 0)
							return false;
						checksRemaining = checksRemaining - 1;
						cellIdx = (row + dr) * gridW + col + dc;
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
