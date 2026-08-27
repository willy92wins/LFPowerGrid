// =========================================================
// LF_PowerGrid - BTC ATM Helper (v5.0 Refactor)
//
// BTC ATM server handlers + utility methods extracted from
// modded PlayerBase into static methods.
//
// Utilities (CountPlayerItems, GreedyChange, etc.) have no
// external dependency. Server handlers use
// Balance operations use LFPG_BalanceRegistry (provider pattern).
// =========================================================

class LFPG_BTCInventoryInput
{
    EntityAI m_Entity;
    int m_UnitValue;
    int m_AvailableUnits;
    int m_SelectedUnits;

    void LFPG_BTCInventoryInput()
    {
        m_Entity = null;
        m_UnitValue = 0;
        m_AvailableUnits = 0;
        m_SelectedUnits = 0;
    }
};

// Runtime-only plan. BTC handlers finish synchronously in one frame; the
// nonce registry already owns the per-player IN_FLIGHT slot during mutation.
class LFPG_BTCInventoryPlan
{
    protected ref array<ref LFPG_BTCInventoryInput> m_Inputs;
    protected ref array<EntityAI> m_Outputs;

    void LFPG_BTCInventoryPlan()
    {
        m_Inputs = new array<ref LFPG_BTCInventoryInput>;
        m_Outputs = new array<EntityAI>;
    }

    void ResetCashInputs()
    {
        m_Inputs.Clear();
    }

    void AddCashInput(EntityAI ent, int unitValue, int availableUnits)
    {
        if (!ent || unitValue <= 0 || availableUnits <= 0)
            return;

        LFPG_BTCInventoryInput input = new LFPG_BTCInventoryInput();
        input.m_Entity = ent;
        input.m_UnitValue = unitValue;
        input.m_AvailableUnits = availableUnits;
        m_Inputs.Insert(input);
    }

    int GetPreparedCashValue()
    {
        int total = 0;
        int i = 0;
        for (i = 0; i < m_Inputs.Count(); i = i + 1)
        {
            LFPG_BTCInventoryInput input = m_Inputs[i];
            if (!input)
                continue;
            total = total + (input.m_UnitValue * input.m_AvailableUnits);
        }
        return total;
    }

    protected void ClearCashSelection()
    {
        int i = 0;
        for (i = 0; i < m_Inputs.Count(); i = i + 1)
        {
            LFPG_BTCInventoryInput input = m_Inputs[i];
            if (input)
                input.m_SelectedUnits = 0;
        }
    }

    bool SelectPreparedCashExact(int amount)
    {
        ClearCashSelection();
        if (amount <= 0)
            return false;

        int remaining = amount;
        int i = 0;
        for (i = 0; i < m_Inputs.Count(); i = i + 1)
        {
            LFPG_BTCInventoryInput input = m_Inputs[i];
            if (!input || input.m_UnitValue <= 0)
                continue;

            int units = remaining / input.m_UnitValue;
            if (units > input.m_AvailableUnits)
                units = input.m_AvailableUnits;
            if (units <= 0)
                continue;

            input.m_SelectedUnits = units;
            remaining = remaining - (units * input.m_UnitValue);
            if (remaining == 0)
                return true;
        }

        ClearCashSelection();
        return false;
    }

    bool SelectPreparedCashCover(int amount, out int selectedValue)
    {
        selectedValue = 0;
        ClearCashSelection();
        if (amount <= 0)
            return false;

        int remaining = amount;
        int i = 0;
        for (i = 0; i < m_Inputs.Count(); i = i + 1)
        {
            LFPG_BTCInventoryInput input = m_Inputs[i];
            if (!input || input.m_UnitValue <= 0)
                continue;

            int units = remaining / input.m_UnitValue;
            if (units > input.m_AvailableUnits)
                units = input.m_AvailableUnits;
            if (units <= 0)
                continue;

            input.m_SelectedUnits = units;
            int value = units * input.m_UnitValue;
            selectedValue = selectedValue + value;
            remaining = remaining - value;
            if (remaining == 0)
                return true;
        }

        for (i = m_Inputs.Count() - 1; i >= 0; i = i - 1)
        {
            LFPG_BTCInventoryInput cover = m_Inputs[i];
            if (!cover || cover.m_UnitValue < remaining)
                continue;
            if (cover.m_SelectedUnits >= cover.m_AvailableUnits)
                continue;

            cover.m_SelectedUnits = cover.m_SelectedUnits + 1;
            selectedValue = selectedValue + cover.m_UnitValue;
            return true;
        }

        ClearCashSelection();
        selectedValue = 0;
        return false;
    }

    int CommitPreparedCashValue(int expectedValue)
    {
        int selectedValue = 0;
        int i = 0;
        for (i = 0; i < m_Inputs.Count(); i = i + 1)
        {
            LFPG_BTCInventoryInput input = m_Inputs[i];
            if (!input || input.m_SelectedUnits <= 0)
                continue;
            if (!input.m_Entity)
                return 0;

            int currentQty = (int)input.m_Entity.GetQuantity();
            if (currentQty < 1)
                currentQty = 1;
            if (currentQty < input.m_SelectedUnits)
                return 0;

            selectedValue = selectedValue + (input.m_SelectedUnits * input.m_UnitValue);
        }

        if (selectedValue != expectedValue)
            return 0;

        int committedValue = 0;
        for (i = 0; i < m_Inputs.Count(); i = i + 1)
        {
            LFPG_BTCInventoryInput commitInput = m_Inputs[i];
            if (!commitInput || commitInput.m_SelectedUnits <= 0)
                continue;

            int available = (int)commitInput.m_Entity.GetQuantity();
            if (available < 1)
                available = 1;
            if (commitInput.m_SelectedUnits >= available)
                g_Game.ObjectDelete(commitInput.m_Entity);
            else
                commitInput.m_Entity.SetQuantity(available - commitInput.m_SelectedUnits, false, false);
            committedValue = committedValue + (commitInput.m_SelectedUnits * commitInput.m_UnitValue);
        }
        return committedValue;
    }

    void TrackOutput(EntityAI ent)
    {
        if (ent)
            m_Outputs.Insert(ent);
    }

    void AbortOutputs()
    {
        int i = 0;
        for (i = 0; i < m_Outputs.Count(); i = i + 1)
        {
            EntityAI output = m_Outputs[i];
            if (output)
                g_Game.ObjectDelete(output);
        }
        m_Outputs.Clear();
    }
};

class LFPG_BTCHelper
{
    // ===== QA hook (permanente, default false) =====
    // Cambia a true + rebuild para forzar el branch race (REFUNDED /
    // REFUND_PARTIAL) en escenarios 6-8 del plan A3+B2
    // (plans/2026-05-17-btc-atomic-tx.md). Default false en produccion.
    // NO requiere revert antes de commit; es hook oficial debug-only.
    static bool LFPG_DebugForceAddStockFail = false;

    // =========================================================
    // BTC ATM: Utility methods (no external dependency)
    // =========================================================

    static void SendBTCTxResultPayload(PlayerBase player, PlayerIdentity sender, int txType, int errCode, int newStock, int newBalance, int btcMoved, float eurAmount, int cashOnInv, int btcOnInv, int serverSessionLow, int serverSessionHigh, int sequence)
    {
        ScriptRPC rpc = new ScriptRPC();
        int subId = LFPG_RPC_SubId.BTC_TX_RESULT;
        rpc.Write(subId);
        rpc.Write(txType);
        rpc.Write(errCode);
        rpc.Write(newStock);
        rpc.Write(newBalance);
        rpc.Write(btcMoved);
        rpc.Write(eurAmount);
        rpc.Write(cashOnInv);
        rpc.Write(btcOnInv);
        rpc.Write(serverSessionLow);
        rpc.Write(serverSessionHigh);
        rpc.Write(sequence);
        // Targeting the player keeps the response private to the requester.
        rpc.Send(player, LFPG_RPC_CHANNEL, true, sender);
    }

    static void SendBTCTxResult(PlayerBase player, PlayerIdentity sender, int txType, int errCode, int newStock, int newBalance, int btcMoved, float eurAmount, int serverSessionLow, int serverSessionHigh, int sequence)
    {
        int cashOnInv = CountPlayerCash(player);
        string btcCls = LFPG_BTCConfig.GetBtcItemClassname();
        int btcOnInv = CountPlayerItems(player, btcCls);

        LFPG_BTCSessionRegistry.Get().CompleteRequest(sender, serverSessionLow, serverSessionHigh, sequence, txType, errCode, newStock, newBalance, btcMoved, eurAmount, cashOnInv, btcOnInv);
        SendBTCTxResultPayload(player, sender, txType, errCode, newStock, newBalance, btcMoved, eurAmount, cashOnInv, btcOnInv, serverSessionLow, serverSessionHigh, sequence);
    }

    static void SendBTCReplayResult(PlayerBase player, PlayerIdentity sender, LFPG_BTCSessionResponse response)
    {
        if (!response)
            return;

        SendBTCTxResultPayload(player, sender, response.m_TxType, response.m_ErrCode, response.m_NewStock, response.m_NewBalance, response.m_BtcMoved, response.m_EurAmount, response.m_CashOnInventory, response.m_BtcOnInventory, response.m_ServerSessionLow, response.m_ServerSessionHigh, response.m_Sequence);
    }

    static void SendBTCNonceRejection(PlayerBase player, PlayerIdentity sender, int txType, int serverSessionLow, int serverSessionHigh, int sequence)
    {
        LFPG_BTCSessionRegistry registry = LFPG_BTCSessionRegistry.Get();
        if (!registry.AllowReplayResponse(sender))
            return;

        // A pre-reservation rejection must not complete an unrelated request
        // that currently owns the per-UID IN_FLIGHT slot.
        int cashOnInv = CountPlayerCash(player);
        string btcCls = LFPG_BTCConfig.GetBtcItemClassname();
        int btcOnInv = CountPlayerItems(player, btcCls);
        int errInvalid = LFPG_BTC_ERR_INVALID;
        SendBTCTxResultPayload(player, sender, txType, errInvalid, 0, 0, 0, 0.0, cashOnInv, btcOnInv, serverSessionLow, serverSessionHigh, sequence);
    }
    static int LFPG_GetEffectiveQty(EntityAI ent)
    {
        if (!ent)
            return 0;

        float fQty = ent.GetQuantity();
        int qty = fQty;
        if (qty < 1)
        {
            qty = 1;
        }
        return qty;
    }

    // Consumes 'toConsume' units from an entity stack.
    // If consuming all or more → ObjectDelete.
    // If partial → SetQuantity to remainder.
    // Returns actual amount consumed.
    static int LFPG_ConsumeFromStack(EntityAI ent, int toConsume)
    {
        if (!ent || toConsume <= 0)
            return 0;

        int available = LFPG_GetEffectiveQty(ent);
        int actual = toConsume;
        if (actual > available)
        {
            actual = available;
        }

        if (actual >= available)
        {
            // Consume entire entity
            g_Game.ObjectDelete(ent);
            return actual;
        }

        // Partial consume — set new quantity directly on EntityAI
        float newQty = available - actual;
        bool bFalse = false;
        ent.SetQuantity(newQty, bFalse, bFalse);
        return actual;
    }

    // Recursively collect entities matching `classname` from `root`'s full
    // inventory tree — walks attachments + cargo, recursing into nested
    // containers (e.g. a case inside a backpack).
    static void LFPG_CollectMatching(EntityAI root, string classname, array<EntityAI> result)
    {
        if (!root || !result)
            return;

        if (root.GetType() == classname)
        {
            result.Insert(root);
        }

        GameInventory inv = root.GetInventory();
        if (!inv)
            return;

        int i = 0;
        int n = inv.AttachmentCount();
        for (i = 0; i < n; i = i + 1)
        {
            EntityAI att = inv.GetAttachmentFromIndex(i);
            LFPG_CollectMatching(att, classname, result);
        }

        CargoBase cargo = inv.GetCargo();
        if (cargo)
        {
            n = cargo.GetItemCount();
            for (i = 0; i < n; i = i + 1)
            {
                EntityAI item = cargo.GetItem(i);
                LFPG_CollectMatching(item, classname, result);
            }
        }
    }

    // Collects all matching items from hands + player's entire inventory tree.
    static void LFPG_CollectPlayerItems(PlayerBase player, string classname, array<EntityAI> result)
    {
        if (!player || !result)
            return;

        HumanInventory hInv = player.GetHumanInventory();
        if (hInv)
        {
            EntityAI hands = hInv.GetEntityInHands();
            LFPG_CollectMatching(hands, classname, result);
        }

        LFPG_CollectMatching(player, classname, result);
    }

    static int CountPlayerItems(PlayerBase player, string classname)
    {
        array<EntityAI> matches = new array<EntityAI>();
        LFPG_CollectPlayerItems(player, classname, matches);

        int count = 0;
        int i = 0;
        int n = matches.Count();
        for (i = 0; i < n; i = i + 1)
        {
            EntityAI ent = matches[i];
            if (!ent)
                continue;
            count = count + LFPG_GetEffectiveQty(ent);
        }
        return count;
    }

    static int DestroyPlayerItems(PlayerBase player, string classname, int amount)
    {
        if (amount <= 0)
            return 0;

        array<EntityAI> candidates = new array<EntityAI>();
        LFPG_CollectPlayerItems(player, classname, candidates);

        int remaining = amount;
        int destroyed = 0;
        int di = 0;
        int dCount = candidates.Count();
        int consumed = 0;
        for (di = 0; di < dCount; di = di + 1)
        {
            if (remaining <= 0)
                break;

            EntityAI ent = candidates[di];
            if (!ent)
                continue;

            consumed = LFPG_ConsumeFromStack(ent, remaining);
            destroyed = destroyed + consumed;
            remaining = remaining - consumed;
        }

        return destroyed;
    }

    // Spawn entity on ground near player with random scatter.
    // Returns null on failure.
    // Scatter stays inside the recipient's own footprint: a ground drop carries
    // no ownership, so any nearby player can take it first.
    static EntityAI SpawnOnGroundNear(string classname, vector basePos)
    {
        basePos[0] = basePos[0] + Math.RandomFloat(-0.15, 0.15);
        basePos[2] = basePos[2] + Math.RandomFloat(-0.15, 0.15);
        Object obj = g_Game.CreateObjectEx(classname, basePos, ECE_CREATEPHYSICS);
        return EntityAI.Cast(obj);
    }

    // Creates `amount` units of `classname` for `player`, stacked to max.
    // Returns total UNITS dispensed (not entity count).
    static int CreateItemsForPlayer(PlayerBase player, string classname, int amount)
    {
        return StageItemsForPlayer(player, classname, amount, null);
    }

    static int StageItemsForPlayer(PlayerBase player, string classname, int amount, LFPG_BTCInventoryPlan outputPlan)
    {
        if (amount <= 0)
            return 0;

        int created = 0;
        int groundDrops = 0;
        vector playerPos = player.GetPosition();

        // BTC perf 2026-05-19: log entry for observability
        int existingCount = CountPlayerItems(player, classname);
        string logEntry = "[BTC perf] CreateItemsForPlayer cls=";
        logEntry = logEntry + classname;
        logEntry = logEntry + " amount=";
        logEntry = logEntry + amount.ToString();
        logEntry = logEntry + " inv=";
        logEntry = logEntry + existingCount.ToString();
        LFPG_Util.Info(logEntry);
        float tStart = g_Game.GetTime();

        // First entity doubles as probe to read GetQuantityMax().
        // ConfigGetFloat is unreliable here: classname may live in
        // CfgMagazines or CfgVehicles depending on user config.
        EntityAI probeItem = player.GetInventory().CreateInInventory(classname);
        bool probeOnGround = false;

        if (!probeItem)
        {
            probeItem = SpawnOnGroundNear(classname, playerPos);
            if (!probeItem)
                return 0;
            probeOnGround = true;
        }

        if (outputPlan)
            outputPlan.TrackOutput(probeItem);

        int maxStack = (int)probeItem.GetQuantityMax();
        if (maxStack < 1)
            maxStack = 1;

        // BTC perf 2026-05-19: stackability gate - warn if amplifier present
        if (maxStack == 1 && amount > 10)
        {
            string warnStack = "[BTC perf] STACKABILITY GATE: maxStack=1 for cls=";
            warnStack = warnStack + classname;
            warnStack = warnStack + " amount=";
            warnStack = warnStack + amount.ToString();
            warnStack = warnStack + " - O(N) entity spawn risk. Consider reconfiguring btcItemClassname to a stackable item or adding varStackMax to its config.";
            LFPG_Util.Warn(warnStack);
        }

        // Assign quantity to probe (first stack)
        int firstQty = amount;
        if (firstQty > maxStack)
            firstQty = maxStack;
        probeItem.SetQuantity((float)firstQty, false, false);
        created = created + firstQty;
        if (probeOnGround)
            groundDrops = groundDrops + 1;

        // Create remaining stacks
        int remaining = amount - firstQty;
        while (remaining > 0)
        {
            int stackQty = remaining;
            if (stackQty > maxStack)
                stackQty = maxStack;

            EntityAI newItem = player.GetInventory().CreateInInventory(classname);
            if (newItem)
            {
                if (outputPlan)
                    outputPlan.TrackOutput(newItem);
                newItem.SetQuantity((float)stackQty, false, false);
                created = created + stackQty;
            }
            else
            {
                EntityAI groundItem = SpawnOnGroundNear(classname, playerPos);
                if (groundItem)
                {
                    if (outputPlan)
                        outputPlan.TrackOutput(groundItem);
                    groundItem.SetQuantity((float)stackQty, false, false);
                    created = created + stackQty;
                    groundDrops = groundDrops + 1;
                }
                else
                {
                    break;
                }
            }

            remaining = remaining - stackQty;
        }

        if (groundDrops > 0)
        {
            // Ground drops are unowned and takeable by anyone nearby. Warn level
            // with id and position so a theft claim can be traced offline; the
            // client message names the count so the recipient reacts at once.
            // GetId (hashed) is the log-safe identifier: gameplay.c:369-370
            // states GetPlainId cannot be used in logs.
            // Staging-time report: a caller that aborts afterwards deletes these
            // objects through LFPG_BTCInventoryPlan.AbortOutputs, so the line is
            // labelled staged and is not by itself proof of a delivered drop.
            string dropUid = "unknown";
            PlayerIdentity dropIdentity = player.GetIdentity();
            if (dropIdentity)
                dropUid = dropIdentity.GetId();
            string dropMsg = "[BTC] ";
            dropMsg = dropMsg + groundDrops.ToString();
            dropMsg = dropMsg + " stacks staged on ground (inventory full; may be rolled back by the caller) cls=";
            dropMsg = dropMsg + classname;
            dropMsg = dropMsg + " uid=";
            dropMsg = dropMsg + dropUid;
            dropMsg = dropMsg + " pos=";
            dropMsg = dropMsg + playerPos.ToString(false);
            LFPG_Util.Warn(dropMsg);
            string dropClientMsg = "Inventory full: ";
            dropClientMsg = dropClientMsg + groundDrops.ToString();
            dropClientMsg = dropClientMsg + " stack(s) dropped at your feet. Pick them up now, anyone nearby can take them.";
            PlayerBase.LFPG_SendClientMsg(player, dropClientMsg);
        }

        float tEnd = g_Game.GetTime();
        float duration = tEnd - tStart;
        string logExit = "[BTC perf] CreateItemsForPlayer cls=";
        logExit = logExit + classname;
        logExit = logExit + " duration_ms=";
        logExit = logExit + duration.ToString();
        logExit = logExit + " created=";
        logExit = logExit + created.ToString();
        logExit = logExit + " maxStack=";
        logExit = logExit + maxStack.ToString();
        LFPG_Util.Info(logExit);
        if (duration > 200.0)
        {
            string warnSlow = "[BTC perf] SLOW: CreateItemsForPlayer took ";
            warnSlow = warnSlow + duration.ToString();
            warnSlow = warnSlow + "ms - cls=";
            warnSlow = warnSlow + classname;
            warnSlow = warnSlow + " amount=";
            warnSlow = warnSlow + amount.ToString();
            LFPG_Util.Warn(warnSlow);
        }

        return created;
    }

    static float GreedyChange(PlayerBase player, float eurAmount, LFPG_BTCInventoryPlan outputPlan)
    {
        if (eurAmount <= 0.0)
            return 0.0;

        float tStartG = g_Game.GetTime();
        string logEntryG = "[BTC perf] GreedyChange eurAmount=";
        logEntryG = logEntryG + eurAmount.ToString();
        LFPG_Util.Info(logEntryG);

        int intAmount = (int)eurAmount;
        float fractional = eurAmount - intAmount;

        auto currencies = LFPG_BTCConfig.GetCurrencies();
        if (!currencies)
            return eurAmount;

        int cCount = currencies.Count();
        if (cCount == 0)
            return eurAmount;

        int remaining = intAmount;
        int ci = 0;
        int billCount = 0;
        int createdBills = 0;
        int eurGiven = 0;

        for (ci = 0; ci < cCount; ci = ci + 1)
        {
            if (remaining <= 0)
                break;

            LFPG_BTCCurrency cur = currencies[ci];
            if (!cur)
                continue;
            if (cur.value <= 0)
                continue;

            billCount = remaining / cur.value;
            if (billCount <= 0)
                continue;

            createdBills = StageItemsForPlayer(player, cur.classname, billCount, outputPlan);
            eurGiven = createdBills * cur.value;
            remaining = remaining - eurGiven;
        }

        // Remainder = fractional cents + any sub-denomination leftover
        float totalRemainder = fractional + remaining;
        float tEndG = g_Game.GetTime();
        float durationG = tEndG - tStartG;
        if (durationG > 200.0)
        {
            string warnSlowG = "[BTC perf] SLOW: GreedyChange took ";
            warnSlowG = warnSlowG + durationG.ToString();
            warnSlowG = warnSlowG + "ms eurAmount=";
            warnSlowG = warnSlowG + eurAmount.ToString();
            LFPG_Util.Warn(warnSlowG);
        }
        return totalRemainder;
    }

    static LFPG_BTCAtmBase ResolveAndValidate(PlayerBase player, int netLow, int netHigh, string tag)
    {
        Object rawObj = g_Game.GetObjectByNetworkId(netLow, netHigh);
        EntityAI devEnt = EntityAI.Cast(rawObj);
        if (!devEnt)
        {
            string errEnt = tag;
            errEnt = errEnt + " entity not found";
            LFPG_Util.Warn(errEnt);
            return null;
        }

        LFPG_BTCAtmBase atm = LFPG_BTCAtmBase.Cast(devEnt);
        if (!atm)
        {
            string errCast = tag;
            errCast = errCast + " entity is not BTCAtm";
            LFPG_Util.Warn(errCast);
            return null;
        }

        float dist = vector.Distance(player.GetPosition(), devEnt.GetPosition());
        if (dist > LFPG_INTERACT_DIST_M)
        {
            string errDist = tag;
            errDist = errDist + " player too far";
            LFPG_Util.Warn(errDist);
            return null;
        }

        if (atm.IsRuined())
        {
            string errRuined = tag;
            errRuined = errRuined + " ATM is ruined";
            LFPG_Util.Warn(errRuined);
            return null;
        }

        return atm;
    }
    static int CountPlayerCash(PlayerBase player)
    {
        int total = 0;
        auto currencies = LFPG_BTCConfig.GetCurrencies();
        if (!currencies)
            return 0;

        int curIdx = 0;
        int curCount = currencies.Count();
        for (curIdx = 0; curIdx < curCount; curIdx = curIdx + 1)
        {
            LFPG_BTCCurrency cur = currencies[curIdx];
            if (!cur)
                continue;

            string cn = cur.classname;
            int itemCount = CountPlayerItems(player, cn);
            int curValue = cur.value;
            int subtotal = itemCount * curValue;
            total = total + subtotal;
        }

        return total;
    }

    static bool PreparePlayerCash(PlayerBase player, LFPG_BTCInventoryPlan plan)
    {
        if (!player || !plan)
            return false;

        plan.ResetCashInputs();
        array<ref LFPG_BTCCurrency> currencies = LFPG_BTCConfig.GetCurrencies();
        if (!currencies)
            return false;

        int ci = 0;
        for (ci = 0; ci < currencies.Count(); ci = ci + 1)
        {
            LFPG_BTCCurrency cur = currencies[ci];
            if (!cur || cur.value <= 0 || cur.classname == "")
                continue;

            array<EntityAI> matches = new array<EntityAI>();
            LFPG_CollectPlayerItems(player, cur.classname, matches);
            int mi = 0;
            for (mi = 0; mi < matches.Count(); mi = mi + 1)
            {
                EntityAI ent = matches[mi];
                if (!ent)
                    continue;
                plan.AddCashInput(ent, cur.value, LFPG_GetEffectiveQty(ent));
            }
        }
        return true;
    }

    // =========================================================
    // BTC ATM: Server Handlers (use BalanceRegistry)
    // =========================================================

    static void HandleBTCOpenRequest(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        int netLow = 0;
        int netHigh = 0;
        if (!ctx.Read(netLow))
            return;
        if (!ctx.Read(netHigh))
            return;

        if (!sender)
            return;

        if (!LFPG_BTCConfig.IsEnabled())
            return;

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            string rlMsg = "Too fast! Wait a moment.";
            PlayerBase.LFPG_SendClientMsg(player, rlMsg);
            return;
        }

        string tag = "[BTCOpenRequest]";
        LFPG_BTCAtmBase atm = ResolveAndValidate(player, netLow, netHigh, tag);
        if (!atm)
            return;

        if (!atm.LFPG_IsATMPowered())
        {
            string errPower = "ATM has no power.";
            PlayerBase.LFPG_SendClientMsg(player, errPower);
            return;
        }

        float price = -1.0;
        bool priceOk = LFPG_NetworkManager.Get().LFPG_IsBTCPriceAvailable();
        if (priceOk)
        {
            price = LFPG_NetworkManager.Get().LFPG_GetBTCPrice();
        }

        int balance = 0;
        LFPG_BalanceProvider atmPlayer = LFPG_BalanceRegistry.GetActive();
        if (atmPlayer)
        {
            balance = atmPlayer.GetBalance(player);
        }

        int stock = atm.LFPG_GetBtcStock();
        bool withdrawOnly = atm.LFPG_IsWithdrawOnly();
        int cashOnInv = CountPlayerCash(player);
        string btcClsOpen = LFPG_BTCConfig.GetBtcItemClassname();
        int btcOnInv = CountPlayerItems(player, btcClsOpen);
        float priceChange24h = LFPG_NetworkManager.Get().LFPG_GetBTC24hChange();

        int serverSessionLow = 0;
        int serverSessionHigh = 0;
        int highWatermark = 0;
        if (!LFPG_BTCSessionRegistry.Get().OpenSession(sender, serverSessionLow, serverSessionHigh, highWatermark))
            return;

        ScriptRPC rpc = new ScriptRPC();
        int subResp = LFPG_RPC_SubId.BTC_OPEN_RESPONSE;
        rpc.Write(subResp);
        rpc.Write(price);
        rpc.Write(stock);
        rpc.Write(balance);
        rpc.Write(cashOnInv);
        rpc.Write(withdrawOnly);
        rpc.Write(btcOnInv);
        rpc.Write(priceChange24h);
        rpc.Write(LFPG_BTC_PROTOCOL_VERSION);
        rpc.Write(serverSessionLow);
        rpc.Write(serverSessionHigh);
        rpc.Write(highWatermark);
        rpc.Send(player, LFPG_RPC_CHANNEL, true, sender);

        string logOpen = "[BTCOpenRequest] price=";
        logOpen = logOpen + price.ToString();
        logOpen = logOpen + " stock=";
        logOpen = logOpen + stock.ToString();
        logOpen = logOpen + " bal=";
        logOpen = logOpen + balance.ToString();
        LFPG_Util.Info(logOpen);
    }
    static void HandleBTCBuy(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        int netLow = 0;
        int netHigh = 0;
        int btcAmount = 0;
        bool useAccount = true;
        int serverSessionLow = 0;
        int serverSessionHigh = 0;
        int sequence = 0;
        bool payloadOk = true;
        if (!ctx.Read(netLow))
            payloadOk = false;
        if (!ctx.Read(netHigh))
            payloadOk = false;
        if (!ctx.Read(btcAmount))
            payloadOk = false;
        if (!ctx.Read(useAccount))
            payloadOk = false;
        if (!ctx.Read(serverSessionLow))
            payloadOk = false;
        if (!ctx.Read(serverSessionHigh))
            payloadOk = false;
        if (!ctx.Read(sequence))
            payloadOk = false;

        if (!sender)
            return;
        if (!payloadOk)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_BUY, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        int requestSubId = (int)LFPG_RPC_SubId.BTC_BUY;
        LFPG_BTCSessionRegistry btcSessions = LFPG_BTCSessionRegistry.Get();
        LFPG_BTCSessionResponse cachedResponse = null;
        int nonceState = btcSessions.CheckRequest(sender, serverSessionLow, serverSessionHigh, sequence, requestSubId, netLow, netHigh, btcAmount, useAccount, cachedResponse);
        if (nonceState == LFPG_BTC_NONCE_REPLAY)
        {
            if (btcSessions.AllowReplayResponse(sender))
                SendBTCReplayResult(player, sender, cachedResponse);
            return;
        }
        if (nonceState == LFPG_BTC_NONCE_IN_FLIGHT)
            return;
        if (nonceState != LFPG_BTC_NONCE_NEW)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_BUY, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!LFPG_BTCConfig.IsEnabled())
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_BUY, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!LFPG_BalanceRegistry.IsAvailable())
        {
            int errNoBp = LFPG_BTC_ERR_NO_BALANCE_PROVIDER;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_BUY, errNoBp, 0, 0, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            string rlMsg = "Too fast! Wait a moment.";
            PlayerBase.LFPG_SendClientMsg(player, rlMsg);
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_BUY, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        string tag = "[BTCBuy]";
        LFPG_BTCAtmBase atm = ResolveAndValidate(player, netLow, netHigh, tag);
        if (!atm)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_BUY, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (btcAmount <= 0)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_BUY, serverSessionLow, serverSessionHigh, sequence);
            return;
        }
        int maxBtcOp = LFPG_BTCConfig.GetMaxBtcPerMachine();
        if (btcAmount > maxBtcOp)
        {
            int errLargeB = LFPG_BTC_ERR_AMOUNT_TOO_LARGE;
            LFPG_BalanceProvider capBp = LFPG_BalanceRegistry.GetActive();
            int capBal = 0;
            if (capBp)
            {
                capBal = capBp.GetBalance(player);
            }
            SendBTCTxResult(player, sender, LFPG_BTC_TX_BUY, errLargeB, atm.LFPG_GetBtcStock(), capBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            string warnLargeB = "[BTCBuy] amount rejected (> cap): requested=";
            warnLargeB = warnLargeB + btcAmount.ToString();
            warnLargeB = warnLargeB + " cap=";
            warnLargeB = warnLargeB + maxBtcOp.ToString();
            LFPG_Util.Warn(warnLargeB);
            return;
        }

        // Read balance early for error responses
        LFPG_BalanceProvider atmEarly = LFPG_BalanceRegistry.GetActive();
        int earlyBal = 0;
        if (atmEarly)
        {
            earlyBal = atmEarly.GetBalance(player);
        }

        // Powered
        if (!atm.LFPG_IsATMPowered())
        {
            int errPow = LFPG_BTC_ERR_NOT_POWERED;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_BUY,errPow, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        // Price
        bool priceOk = LFPG_NetworkManager.Get().LFPG_IsBTCPriceAvailable();
        if (!priceOk)
        {
            int errPrice = LFPG_BTC_ERR_NO_PRICE;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_BUY,errPrice, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }
        float price = LFPG_NetworkManager.Get().LFPG_GetBTCPrice();

        // B1: Stock check REMOVED — Buy spawns unlimited BTC

        // Cost calculation (ceiling = player pays more on fractions)
        float costFloat = btcAmount * price;
        // Overflow guard: (int)costFloat is UB if costFloat exceeds INT_MAX
        // (~2.147e9). Defense-in-depth on top of the btcAmount cap above.
        if (costFloat > 2000000000.0)
        {
            int errOverflowB = LFPG_BTC_ERR_AMOUNT_TOO_LARGE;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_BUY, errOverflowB, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            string warnOverflowB = "[BTCBuy] cost overflow rejected: btcAmount=";
            warnOverflowB = warnOverflowB + btcAmount.ToString();
            warnOverflowB = warnOverflowB + " price=";
            warnOverflowB = warnOverflowB + price.ToString();
            warnOverflowB = warnOverflowB + " costFloat=";
            warnOverflowB = warnOverflowB + costFloat.ToString();
            LFPG_Util.Error(warnOverflowB);
            return;
        }
        int costInt = (int)costFloat;
        float costDiff = costFloat - costInt;
        if (costDiff > 0.001)
        {
            costInt = costInt + 1;
        }

        if (useAccount)
        {
            // WithdrawOnly: account BUY adds BTC to ATM stock; cash BUY does not.
            if (atm.LFPG_IsWithdrawOnly())
            {
                int errWo = LFPG_BTC_ERR_INVALID;
                SendBTCTxResult(player, sender, LFPG_BTC_TX_BUY,errWo, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
                return;
            }

            // Claims are implemented only by Native; piggyback account buys fail closed.
            if (!atmEarly || atmEarly.GetName() != "Native")
            {
                if (!btcSessions.ReserveRequest(sender, serverSessionLow, serverSessionHigh, sequence, requestSubId, netLow, netHigh, btcAmount, useAccount))
                {
                    SendBTCNonceRejection(player, sender, LFPG_BTC_TX_BUY, serverSessionLow, serverSessionHigh, sequence);
                    return;
                }
                int errProviderMode = LFPG_BTC_ERR_NO_BALANCE_PROVIDER;
                SendBTCTxResult(player, sender, LFPG_BTC_TX_BUY, errProviderMode, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
                return;
            }
        
            int playerBalance = earlyBal;
            if (playerBalance < costInt)
            {
                int errFunds = LFPG_BTC_ERR_NO_FUNDS;
                SendBTCTxResult(player, sender, LFPG_BTC_TX_BUY, errFunds, atm.LFPG_GetBtcStock(), playerBalance, 0, costFloat, serverSessionLow, serverSessionHigh, sequence);
                return;
            }
        
            int currentStock = atm.LFPG_GetBtcStock();
            int maxStock = LFPG_BTCConfig.GetMaxBtcPerMachine();
            if (btcAmount > maxStock - currentStock)
            {
                int errFullA = LFPG_BTC_ERR_STOCK_FULL;
                SendBTCTxResult(player, sender, LFPG_BTC_TX_BUY, errFullA, currentStock, playerBalance, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
                string warnBuyFull = "[BTCBuy] account rejected: ATM stock full ";
                warnBuyFull = warnBuyFull + currentStock.ToString();
                warnBuyFull = warnBuyFull + "/";
                warnBuyFull = warnBuyFull + maxStock.ToString();
                LFPG_Util.Warn(warnBuyFull);
                return;
            }
        
            if (!btcSessions.ReserveRequest(sender, serverSessionLow, serverSessionHigh, sequence, requestSubId, netLow, netHigh, btcAmount, useAccount))
            {
                SendBTCNonceRejection(player, sender, LFPG_BTC_TX_BUY, serverSessionLow, serverSessionHigh, sequence);
                return;
            }
        
            int stockTarget = currentStock + btcAmount;
            int claimedDebit = 0;
            string deviceId = atm.LFPG_GetDeviceId();
            bool claimSaved = LFPG_BalanceProvider_NativeImpl.DebitWithStockClaim(player, deviceId, serverSessionLow, serverSessionHigh, sequence, costInt, currentStock, stockTarget, claimedDebit);
            if (!claimSaved || claimedDebit != costInt)
            {
                int claimBalance = atmEarly.GetBalance(player);
                int errClaim = LFPG_BTC_ERR_INVALID;
                SendBTCTxResult(player, sender, LFPG_BTC_TX_BUY, errClaim, atm.LFPG_GetBtcStock(), claimBalance, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
                return;
            }
            bool stockApplied = atm.LFPG_ApplyClaimedStockTarget(stockTarget);
            // Permanent QA hook: a forced failure leaves the durable claim recoverable.
            if (LFPG_DebugForceAddStockFail)
                stockApplied = false;
            if (!stockApplied)
            {
                LFPG_BalanceProvider_NativeImpl.MarkStockClaimApplyFailed(deviceId);
                int applyBalance = atmEarly.GetBalance(player);
                int errApply = LFPG_BTC_ERR_INVALID;
                SendBTCTxResult(player, sender, LFPG_BTC_TX_BUY, errApply, atm.LFPG_GetBtcStock(), applyBalance, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
                return;
            }
        
            int newBalance = atmEarly.GetBalance(player);
            int newStock = atm.LFPG_GetBtcStock();
            int okCode = LFPG_BTC_OK;
            float eurSpent = claimedDebit;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_BUY, okCode, newStock, newBalance, btcAmount, eurSpent, serverSessionLow, serverSessionHigh, sequence);
        
            string logBuyA = "[BTCBuy] account->stock: ";
            logBuyA = logBuyA + btcAmount.ToString();
            logBuyA = logBuyA + " BTC for ";
            logBuyA = logBuyA + claimedDebit.ToString();
            logBuyA = logBuyA + " EUR (durable claim pending boot proof)";
            LFPG_Util.Info(logBuyA);
        }
        else
        {
            // ── Cash mode: stage delivery and exact net debit before commit ──
            if (!btcSessions.ReserveRequest(sender, serverSessionLow, serverSessionHigh, sequence, requestSubId, netLow, netHigh, btcAmount, useAccount))
            {
                SendBTCNonceRejection(player, sender, LFPG_BTC_TX_BUY, serverSessionLow, serverSessionHigh, sequence);
                return;
            }

            LFPG_BTCInventoryPlan buyCashPlan = new LFPG_BTCInventoryPlan();
            string btcClsCash = LFPG_BTCConfig.GetBtcItemClassname();
            int createdCash = StageItemsForPlayer(player, btcClsCash, btcAmount, buyCashPlan);
            if (createdCash <= 0)
            {
                buyCashPlan.AbortOutputs();
                int errInvC = LFPG_BTC_ERR_INVENTORY_FULL;
                SendBTCTxResult(player, sender, LFPG_BTC_TX_BUY, errInvC, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
                return;
            }

            float actualCostFloatC = createdCash * price;
            int actualCostIntC = (int)actualCostFloatC;
            float actualCostDiffC = actualCostFloatC - actualCostIntC;
            if (actualCostDiffC > 0.001)
                actualCostIntC = actualCostIntC + 1;

            if (!PreparePlayerCash(player, buyCashPlan))
            {
                buyCashPlan.AbortOutputs();
                int errNoCash = LFPG_BTC_ERR_NO_CASH;
                SendBTCTxResult(player, sender, LFPG_BTC_TX_BUY, errNoCash, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
                return;
            }

            int selectedCashValue = 0;
            if (!buyCashPlan.SelectPreparedCashCover(actualCostIntC, selectedCashValue))
            {
                buyCashPlan.AbortOutputs();
                int errNoCash2 = LFPG_BTC_ERR_NO_CASH;
                SendBTCTxResult(player, sender, LFPG_BTC_TX_BUY, errNoCash2, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
                return;
            }

            int cashChange = selectedCashValue - actualCostIntC;
            if (cashChange > 0)
            {
                float changeRemainder = GreedyChange(player, cashChange, buyCashPlan);
                if (changeRemainder > 0.001)
                {
                    buyCashPlan.AbortOutputs();
                    int errChange = LFPG_BTC_ERR_INVENTORY_FULL;
                    SendBTCTxResult(player, sender, LFPG_BTC_TX_BUY, errChange, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
                    return;
                }
            }

            int committedCashValue = buyCashPlan.CommitPreparedCashValue(selectedCashValue);
            if (committedCashValue != selectedCashValue)
            {
                buyCashPlan.AbortOutputs();
                int errCommitC = LFPG_BTC_ERR_INVALID;
                SendBTCTxResult(player, sender, LFPG_BTC_TX_BUY, errCommitC, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
                return;
            }

            int newStockC = atm.LFPG_GetBtcStock();
            int newBalC = 0;
            LFPG_BalanceProvider atmFinalC = LFPG_BalanceRegistry.GetActive();
            if (atmFinalC)
                newBalC = atmFinalC.GetBalance(player);
            int okCodeC = LFPG_BTC_OK;
            float eurSpentC = actualCostIntC;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_BUY, okCodeC, newStockC, newBalC, createdCash, eurSpentC, serverSessionLow, serverSessionHigh, sequence);

            string logBuyC = "[BTCBuy] cash: ";
            logBuyC = logBuyC + createdCash.ToString();
            logBuyC = logBuyC + " BTC for ";
            logBuyC = logBuyC + actualCostIntC.ToString();
            logBuyC = logBuyC + " EUR bills";
            LFPG_Util.Info(logBuyC);
        }
    }

    static void HandleBTCSell(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        int netLow = 0;
        int netHigh = 0;
        int btcAmount = 0;
        bool toAccount = false;
        int serverSessionLow = 0;
        int serverSessionHigh = 0;
        int sequence = 0;
        bool payloadOk = true;
        if (!ctx.Read(netLow))
            payloadOk = false;
        if (!ctx.Read(netHigh))
            payloadOk = false;
        if (!ctx.Read(btcAmount))
            payloadOk = false;
        if (!ctx.Read(toAccount))
            payloadOk = false;
        if (!ctx.Read(serverSessionLow))
            payloadOk = false;
        if (!ctx.Read(serverSessionHigh))
            payloadOk = false;
        if (!ctx.Read(sequence))
            payloadOk = false;

        if (!sender)
            return;
        if (!payloadOk)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_SELL, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        // Fingerprints use the decoded request argument, before policy can
        // force the execution mode to cash.
        bool requestedToAccount = toAccount;
        int requestSubId = (int)LFPG_RPC_SubId.BTC_SELL;
        LFPG_BTCSessionRegistry btcSessions = LFPG_BTCSessionRegistry.Get();
        LFPG_BTCSessionResponse cachedResponse = null;
        int nonceState = btcSessions.CheckRequest(sender, serverSessionLow, serverSessionHigh, sequence, requestSubId, netLow, netHigh, btcAmount, requestedToAccount, cachedResponse);
        if (nonceState == LFPG_BTC_NONCE_REPLAY)
        {
            if (btcSessions.AllowReplayResponse(sender))
                SendBTCReplayResult(player, sender, cachedResponse);
            return;
        }
        if (nonceState == LFPG_BTC_NONCE_IN_FLIGHT)
            return;
        if (nonceState != LFPG_BTC_NONCE_NEW)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_SELL, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!LFPG_BTCConfig.IsEnabled())
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_SELL, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!LFPG_BalanceRegistry.IsAvailable())
        {
            int errNoBp = LFPG_BTC_ERR_NO_BALANCE_PROVIDER;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_SELL, errNoBp, 0, 0, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            string rlMsg = "Too fast! Wait a moment.";
            PlayerBase.LFPG_SendClientMsg(player, rlMsg);
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_SELL, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        string tag = "[BTCSell]";
        LFPG_BTCAtmBase atm = ResolveAndValidate(player, netLow, netHigh, tag);
        if (!atm)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_SELL, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (btcAmount <= 0)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_SELL, serverSessionLow, serverSessionHigh, sequence);
            return;
        }
        int maxBtcOp = LFPG_BTCConfig.GetMaxBtcPerMachine();
        if (btcAmount > maxBtcOp)
        {
            int errLargeS = LFPG_BTC_ERR_AMOUNT_TOO_LARGE;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_SELL, errLargeS, atm.LFPG_GetBtcStock(), 0, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            string warnLargeS = "[BTCSell] amount rejected (> cap): requested=";
            warnLargeS = warnLargeS + btcAmount.ToString();
            warnLargeS = warnLargeS + " cap=";
            warnLargeS = warnLargeS + maxBtcOp.ToString();
            LFPG_Util.Warn(warnLargeS);
            return;
        }

        // Read balance early for error responses
        LFPG_BalanceProvider atmEarlyS = LFPG_BalanceRegistry.GetActive();
        int earlyBalS = 0;
        if (atmEarlyS)
        {
            earlyBalS = atmEarlyS.GetBalance(player);
        }

        // Powered
        if (!atm.LFPG_IsATMPowered())
        {
            int errPow = LFPG_BTC_ERR_NOT_POWERED;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_SELL,errPow, atm.LFPG_GetBtcStock(), earlyBalS, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        // Price
        bool priceOk = LFPG_NetworkManager.Get().LFPG_IsBTCPriceAvailable();
        if (!priceOk)
        {
            int errPrice = LFPG_BTC_ERR_NO_PRICE;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_SELL,errPrice, atm.LFPG_GetBtcStock(), earlyBalS, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }
        float price = LFPG_NetworkManager.Get().LFPG_GetBTCPrice();

        // Overflow guard BEFORE destruction: subsequent eurTotal = destroyed*price
        // cast to int would be UB if it exceeds INT_MAX (~2.147e9). Rejecting
        // here avoids losing player items to a transaction that cannot complete.
        float eurTotalCheck = btcAmount * price;
        if (eurTotalCheck > 2000000000.0)
        {
            int errOverflowS = LFPG_BTC_ERR_AMOUNT_TOO_LARGE;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_SELL, errOverflowS, atm.LFPG_GetBtcStock(), earlyBalS, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            string warnOverflowS = "[BTCSell] revenue overflow rejected: btcAmount=";
            warnOverflowS = warnOverflowS + btcAmount.ToString();
            warnOverflowS = warnOverflowS + " price=";
            warnOverflowS = warnOverflowS + price.ToString();
            warnOverflowS = warnOverflowS + " eurTotal=";
            warnOverflowS = warnOverflowS + eurTotalCheck.ToString();
            LFPG_Util.Error(warnOverflowS);
            return;
        }

        // Check player has enough BTC items
        string btcClassname = LFPG_BTCConfig.GetBtcItemClassname();
        int playerBtc = CountPlayerItems(player, btcClassname);
        if (playerBtc < btcAmount)
        {
            int errItems = LFPG_BTC_ERR_NO_ITEMS;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_SELL,errItems, atm.LFPG_GetBtcStock(), earlyBalS, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        // Sell: BTC items are destroyed, no stock room check needed
        // (sold BTC disappear from the game)

        // Enforce WithdrawOnly: if true, always cash
        bool withdrawOnly = atm.LFPG_IsWithdrawOnly();
        if (withdrawOnly)
        {
            toAccount = false;
        }

        // Account Sell preflight. Integer value must be representable either
        // by the account room or by an exact staged cash payout. Only the
        // fractional residue remains on the ATM.
        int eurIntExpected = (int)eurTotalCheck;
        int expectedCarry = 0;
        int expectedIntegerPayout = 0;
        int accountRoom = 0;
        int expectedAccountPayout = 0;
        int expectedCashPayout = 0;
        float storedRemainderExpected = 0.0;
        float nextDecimalRemainder = 0.0;
        LFPG_BTCInventoryPlan sellAccountPlan = new LFPG_BTCInventoryPlan();
        LFPG_BTCInventoryPlan sellCashPlan = new LFPG_BTCInventoryPlan();
        float nextCashDecimalRemainder = 0.0;
        bool nativeAccountRoomKnown = (atmEarlyS.GetName() == "Native");
        // Mirror of the account-buy gate: account credit is implemented only
        // by Native. A sell requested "to account" with an external provider
        // fails closed before any reservation or destruction instead of
        // silently degrading to a cash payout.
        if (toAccount && !nativeAccountRoomKnown)
        {
            int errProviderSell = LFPG_BTC_ERR_NO_BALANCE_PROVIDER;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_SELL, errProviderSell, atm.LFPG_GetBtcStock(), earlyBalS, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            LFPG_Util.Error("[BTCSell] non-Native provider; account Sell rejected before reservation/destruction");
            return;
        }
        if (toAccount && nativeAccountRoomKnown && !LFPG_BalanceProvider_NativeImpl.IsClaimStoreWritable())
        {
            int errNativeStore = LFPG_BTC_ERR_INVALID;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_SELL, errNativeStore, atm.LFPG_GetBtcStock(), earlyBalS, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            LFPG_Util.Error("[BTCSell] Native store not writable; account Sell rejected before reservation/destruction");
            return;
        }
        if (toAccount)
        {
            float fractionalExpected = eurTotalCheck - eurIntExpected;
            storedRemainderExpected = atm.LFPG_GetDecimalRemainder();
            if (storedRemainderExpected < 0.0 || storedRemainderExpected >= 1.0)
            {
                int errStoredRemainder = LFPG_BTC_ERR_INVALID;
                SendBTCTxResult(player, sender, LFPG_BTC_TX_SELL, errStoredRemainder, atm.LFPG_GetBtcStock(), earlyBalS, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
                LFPG_Util.Error("[BTCSell] invalid persisted decimal remainder; account Sell rejected before reservation/destruction");
                return;
            }
            if (earlyBalS < 0)
            {
                int errInvalidBalance = LFPG_BTC_ERR_INVALID;
                SendBTCTxResult(player, sender, LFPG_BTC_TX_SELL, errInvalidBalance, atm.LFPG_GetBtcStock(), earlyBalS, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
                LFPG_Util.Error("[BTCSell] negative provider balance; account Sell rejected before capacity arithmetic");
                return;
            }
            float combinedExpectedRemainder = storedRemainderExpected + fractionalExpected;
            expectedCarry = (int)combinedExpectedRemainder;
            nextDecimalRemainder = combinedExpectedRemainder - expectedCarry;
            if (eurIntExpected > LFPG_BalanceProvider_NativeImpl.GetBalanceCap() - expectedCarry)
            {
                int errCarryOverflow = LFPG_BTC_ERR_AMOUNT_TOO_LARGE;
                SendBTCTxResult(player, sender, LFPG_BTC_TX_SELL, errCarryOverflow, atm.LFPG_GetBtcStock(), earlyBalS, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
                return;
            }
            expectedIntegerPayout = eurIntExpected + expectedCarry;
            if (nativeAccountRoomKnown)
            {
                accountRoom = LFPG_BalanceProvider_NativeImpl.GetBalanceCap() - earlyBalS;
                if (accountRoom < 0)
                    accountRoom = 0;
                if (accountRoom > expectedIntegerPayout)
                    accountRoom = expectedIntegerPayout;
            }
            else
            {
                // External providers expose no capacity contract. Zero is the
                // only proven room; stage the complete integer value as cash.
                accountRoom = 0;
            }
            expectedAccountPayout = accountRoom;
            expectedCashPayout = expectedIntegerPayout - expectedAccountPayout;
        }

        // Reserve before the first inventory mutation.
        if (!btcSessions.ReserveRequest(sender, serverSessionLow, serverSessionHigh, sequence, requestSubId, netLow, netHigh, btcAmount, requestedToAccount))
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_SELL, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!toAccount)
        {
            float cashFractionalExpected = eurTotalCheck - eurIntExpected;
            float storedCashRemainder = atm.LFPG_GetDecimalRemainder();
            if (storedCashRemainder < 0.0 || storedCashRemainder >= 1.0)
            {
                int errStoredCashRemainder = LFPG_BTC_ERR_INVALID;
                SendBTCTxResult(player, sender, LFPG_BTC_TX_SELL, errStoredCashRemainder, atm.LFPG_GetBtcStock(), earlyBalS, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
                LFPG_Util.Error("[BTCSell] invalid persisted decimal remainder; cash Sell rejected before destruction");
                return;
            }
            float combinedCashExpected = storedCashRemainder + cashFractionalExpected;
            int expectedCashCarry = (int)combinedCashExpected;
            int expectedCashStage = eurIntExpected + expectedCashCarry;
            nextCashDecimalRemainder = combinedCashExpected - expectedCashCarry;
            float stagedCashSellRemainder = GreedyChange(player, expectedCashStage, sellCashPlan);
            if (stagedCashSellRemainder > 0.001)
            {
                sellCashPlan.AbortOutputs();
                int errCashStage = LFPG_BTC_ERR_INVENTORY_FULL;
                SendBTCTxResult(player, sender, LFPG_BTC_TX_SELL, errCashStage, atm.LFPG_GetBtcStock(), earlyBalS, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
                return;
            }
        }

        if (toAccount && expectedCashPayout > 0)
        {
            float stagedCashRemainder = GreedyChange(player, expectedCashPayout, sellAccountPlan);
            if (stagedCashRemainder > 0.001)
            {
                sellAccountPlan.AbortOutputs();
                int errCashCapacity = LFPG_BTC_ERR_INVENTORY_FULL;
                SendBTCTxResult(player, sender, LFPG_BTC_TX_SELL, errCashCapacity, atm.LFPG_GetBtcStock(), earlyBalS, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
                return;
            }
        }

        // Commit BTC input destruction only after every output is staged.
        int destroyed = DestroyPlayerItems(player, btcClassname, btcAmount);

        // Guard: if nothing was destroyed, abort
        if (destroyed <= 0)
        {
            if (sellAccountPlan)
                sellAccountPlan.AbortOutputs();
            if (sellCashPlan)
                sellCashPlan.AbortOutputs();
            int errDestroy = LFPG_BTC_ERR_NO_ITEMS;
            int curStockD = atm.LFPG_GetBtcStock();
            int curBalD = atmEarlyS.GetBalance(player);
            SendBTCTxResult(player, sender, LFPG_BTC_TX_SELL,errDestroy, curStockD, curBalD, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            string logNoD = "[BTCSell] failed to destroy any items";
            LFPG_Util.Warn(logNoD);
            return;
        }
        if (destroyed != btcAmount)
        {
            if (sellAccountPlan)
                sellAccountPlan.AbortOutputs();
            if (sellCashPlan)
                sellCashPlan.AbortOutputs();
            int errPartialDestroy = LFPG_BTC_ERR_INVALID;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_SELL, errPartialDestroy, atm.LFPG_GetBtcStock(), earlyBalS, destroyed, 0.0, serverSessionLow, serverSessionHigh, sequence);
            LFPG_Util.Error("[BTCSell] post-destruction cardinality mismatch; terminal error emitted instead of reporting undelivered value as OK");
            return;
        }

        // Calculate EUR revenue based on actual destroyed count
        float eurTotal = destroyed * price;

        // Sold BTC disappear from the game (no stock add)

        // Pay player
        int accountAdded = -1;
        if (toAccount)
        {
            accountAdded = 0;
            if (expectedAccountPayout > 0)
                accountAdded = atmEarlyS.AddBalance(player, expectedAccountPayout);
            if (accountAdded != expectedAccountPayout)
            {
                int failedAccountBalance = atmEarlyS.GetBalance(player);
                int errAccountCredit = LFPG_BTC_ERR_INVALID;
                SendBTCTxResult(player, sender, LFPG_BTC_TX_SELL, errAccountCredit, atm.LFPG_GetBtcStock(), failedAccountBalance, destroyed, accountAdded + expectedCashPayout, serverSessionLow, serverSessionHigh, sequence);
                LFPG_Util.Error("[BTCSell] post-destruction account credit differed from preflight; terminal error emitted, integer value was never placed in decimal remainder");
                return;
            }
            atm.LFPG_SetDecimalRemainder(nextDecimalRemainder);

            if (expectedCashPayout > 0)
            {
                string spillMsg = "[BTCSell] account cap spill staged exactly as cash EUR=";
                spillMsg = spillMsg + expectedCashPayout.ToString();
                LFPG_Util.Info(spillMsg);
            }
        }
        else
        {
            // Cash outputs were staged exactly before BTC destruction.
            atm.LFPG_SetDecimalRemainder(nextCashDecimalRemainder);
        }


        // Read updated state
        int newBalance = atmEarlyS.GetBalance(player);
        int newStock = atm.LFPG_GetBtcStock();

        int okCode = LFPG_BTC_OK;
        SendBTCTxResult(player, sender, LFPG_BTC_TX_SELL,okCode, newStock, newBalance, destroyed, eurTotal, serverSessionLow, serverSessionHigh, sequence);

        string logSell = "[BTCSell] player sold ";
        logSell = logSell + destroyed.ToString();
        logSell = logSell + " BTC for ";
        logSell = logSell + eurTotal.ToString();
        logSell = logSell + " EUR toAccount=";
        logSell = logSell + toAccount.ToString();
        LFPG_Util.Info(logSell);
    }

    static void HandleBTCWithdraw(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        int netLow = 0;
        int netHigh = 0;
        int btcAmount = 0;
        int serverSessionLow = 0;
        int serverSessionHigh = 0;
        int sequence = 0;
        bool payloadOk = true;
        if (!ctx.Read(netLow))
            payloadOk = false;
        if (!ctx.Read(netHigh))
            payloadOk = false;
        if (!ctx.Read(btcAmount))
            payloadOk = false;
        if (!ctx.Read(serverSessionLow))
            payloadOk = false;
        if (!ctx.Read(serverSessionHigh))
            payloadOk = false;
        if (!ctx.Read(sequence))
            payloadOk = false;

        if (!sender)
            return;
        if (!payloadOk)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_WITHDRAW, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        int requestSubId = (int)LFPG_RPC_SubId.BTC_WITHDRAW;
        LFPG_BTCSessionRegistry btcSessions = LFPG_BTCSessionRegistry.Get();
        LFPG_BTCSessionResponse cachedResponse = null;
        int nonceState = btcSessions.CheckRequest(sender, serverSessionLow, serverSessionHigh, sequence, requestSubId, netLow, netHigh, btcAmount, false, cachedResponse);
        if (nonceState == LFPG_BTC_NONCE_REPLAY)
        {
            if (btcSessions.AllowReplayResponse(sender))
                SendBTCReplayResult(player, sender, cachedResponse);
            return;
        }
        if (nonceState == LFPG_BTC_NONCE_IN_FLIGHT)
            return;
        if (nonceState != LFPG_BTC_NONCE_NEW)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_WITHDRAW, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!LFPG_BTCConfig.IsEnabled())
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_WITHDRAW, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            string rlMsg = "Too fast! Wait a moment.";
            PlayerBase.LFPG_SendClientMsg(player, rlMsg);
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_WITHDRAW, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        string tag = "[BTCWithdraw]";
        LFPG_BTCAtmBase atm = ResolveAndValidate(player, netLow, netHigh, tag);
        if (!atm)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_WITHDRAW, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (btcAmount <= 0)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_WITHDRAW, serverSessionLow, serverSessionHigh, sequence);
            return;
        }
        int maxBtcOp = LFPG_BTCConfig.GetMaxBtcPerMachine();
        if (btcAmount > maxBtcOp)
        {
            int errLargeW = LFPG_BTC_ERR_AMOUNT_TOO_LARGE;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_WITHDRAW, errLargeW, atm.LFPG_GetBtcStock(), 0, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            string warnLargeW = "[BTCWithdraw] amount rejected (> cap): requested=";
            warnLargeW = warnLargeW + btcAmount.ToString();
            warnLargeW = warnLargeW + " cap=";
            warnLargeW = warnLargeW + maxBtcOp.ToString();
            LFPG_Util.Warn(warnLargeW);
            return;
        }

        // Read balance early for error responses
        int earlyBalW = 0;
        LFPG_BalanceProvider atmEarlyW = LFPG_BalanceRegistry.GetActive();
        if (atmEarlyW)
        {
            earlyBalW = atmEarlyW.GetBalance(player);
        }

        // Powered
        if (!atm.LFPG_IsATMPowered())
        {
            int errPow = LFPG_BTC_ERR_NOT_POWERED;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_WITHDRAW,errPow, atm.LFPG_GetBtcStock(), earlyBalW, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        // Stock check
        int currentStock = atm.LFPG_GetBtcStock();
        if (btcAmount > currentStock)
        {
            int errStock = LFPG_BTC_ERR_NO_STOCK;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_WITHDRAW,errStock, currentStock, earlyBalW, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!btcSessions.ReserveRequest(sender, serverSessionLow, serverSessionHigh, sequence, requestSubId, netLow, netHigh, btcAmount, false))
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_WITHDRAW, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        // Stage physical delivery before the claim fold and stock commit.
        LFPG_BTCInventoryPlan btcWithdrawPlan = new LFPG_BTCInventoryPlan();
        string btcClassname = LFPG_BTCConfig.GetBtcItemClassname();
        int created = StageItemsForPlayer(player, btcClassname, btcAmount, btcWithdrawPlan);

        if (created <= 0)
        {
            btcWithdrawPlan.AbortOutputs();
            int errInvW = LFPG_BTC_ERR_INVENTORY_FULL;
            int curStockW = atm.LFPG_GetBtcStock();
            SendBTCTxResult(player, sender, LFPG_BTC_TX_WITHDRAW, errInvW, curStockW, earlyBalW, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            LFPG_Util.Warn("[BTCWithdraw] inventory full, no items created");
            return;
        }

        bool stockRemoved = atm.LFPG_RemoveBtcStock(created);
        if (!stockRemoved)
        {
            btcWithdrawPlan.AbortOutputs();
            int errClaimFold = LFPG_BTC_ERR_INVALID;
            int blockedStock = atm.LFPG_GetBtcStock();
            SendBTCTxResult(player, sender, LFPG_BTC_TX_WITHDRAW, errClaimFold, blockedStock, earlyBalW, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        // Updated state
        int newStock = atm.LFPG_GetBtcStock();
        int newBalance = earlyBalW;

        int okCode = LFPG_BTC_OK;
        SendBTCTxResult(player, sender, LFPG_BTC_TX_WITHDRAW,okCode, newStock, newBalance, created, 0.0, serverSessionLow, serverSessionHigh, sequence);

        string logW = "[BTCWithdraw] player withdrew ";
        logW = logW + created.ToString();
        logW = logW + " BTC from pool";
        LFPG_Util.Info(logW);
    }

    static void HandleBTCDeposit(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        int netLow = 0;
        int netHigh = 0;
        int btcAmount = 0;
        int serverSessionLow = 0;
        int serverSessionHigh = 0;
        int sequence = 0;
        bool payloadOk = true;
        if (!ctx.Read(netLow))
            payloadOk = false;
        if (!ctx.Read(netHigh))
            payloadOk = false;
        if (!ctx.Read(btcAmount))
            payloadOk = false;
        if (!ctx.Read(serverSessionLow))
            payloadOk = false;
        if (!ctx.Read(serverSessionHigh))
            payloadOk = false;
        if (!ctx.Read(sequence))
            payloadOk = false;

        if (!sender)
            return;
        if (!payloadOk)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_DEPOSIT, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        int requestSubId = (int)LFPG_RPC_SubId.BTC_DEPOSIT;
        LFPG_BTCSessionRegistry btcSessions = LFPG_BTCSessionRegistry.Get();
        LFPG_BTCSessionResponse cachedResponse = null;
        int nonceState = btcSessions.CheckRequest(sender, serverSessionLow, serverSessionHigh, sequence, requestSubId, netLow, netHigh, btcAmount, false, cachedResponse);
        if (nonceState == LFPG_BTC_NONCE_REPLAY)
        {
            if (btcSessions.AllowReplayResponse(sender))
                SendBTCReplayResult(player, sender, cachedResponse);
            return;
        }
        if (nonceState == LFPG_BTC_NONCE_IN_FLIGHT)
            return;
        if (nonceState != LFPG_BTC_NONCE_NEW)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_DEPOSIT, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!LFPG_BTCConfig.IsEnabled())
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_DEPOSIT, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            string rlMsg = "Too fast! Wait a moment.";
            PlayerBase.LFPG_SendClientMsg(player, rlMsg);
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_DEPOSIT, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        string tag = "[BTCDeposit]";
        LFPG_BTCAtmBase atm = ResolveAndValidate(player, netLow, netHigh, tag);
        if (!atm)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_DEPOSIT, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (btcAmount <= 0)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_DEPOSIT, serverSessionLow, serverSessionHigh, sequence);
            return;
        }
        int maxBtcOp = LFPG_BTCConfig.GetMaxBtcPerMachine();
        if (btcAmount > maxBtcOp)
        {
            int errLargeD = LFPG_BTC_ERR_AMOUNT_TOO_LARGE;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_DEPOSIT, errLargeD, atm.LFPG_GetBtcStock(), 0, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            string warnLargeD = "[BTCDeposit] amount rejected (> cap): requested=";
            warnLargeD = warnLargeD + btcAmount.ToString();
            warnLargeD = warnLargeD + " cap=";
            warnLargeD = warnLargeD + maxBtcOp.ToString();
            LFPG_Util.Warn(warnLargeD);
            return;
        }

        // Read balance early for error responses
        int earlyBalD = 0;
        LFPG_BalanceProvider atmEarlyD = LFPG_BalanceRegistry.GetActive();
        if (atmEarlyD)
        {
            earlyBalD = atmEarlyD.GetBalance(player);
        }

        // Powered
        if (!atm.LFPG_IsATMPowered())
        {
            int errPow = LFPG_BTC_ERR_NOT_POWERED;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_DEPOSIT,errPow, atm.LFPG_GetBtcStock(), earlyBalD, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        // WithdrawOnly: nothing enters the account or the ATM.
        if (atm.LFPG_IsWithdrawOnly())
        {
            int errWo = LFPG_BTC_ERR_INVALID;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_DEPOSIT,errWo, atm.LFPG_GetBtcStock(), earlyBalD, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        // Player has items?
        string btcClassname = LFPG_BTCConfig.GetBtcItemClassname();
        int playerBtc = CountPlayerItems(player, btcClassname);
        if (playerBtc < btcAmount)
        {
            int errItems = LFPG_BTC_ERR_NO_ITEMS;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_DEPOSIT,errItems, atm.LFPG_GetBtcStock(), earlyBalD, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        // ATM has room?
        int maxStock = LFPG_BTCConfig.GetMaxBtcPerMachine();
        int currentStock = atm.LFPG_GetBtcStock();
        // Overflow-safe
        if (btcAmount > maxStock - currentStock)
        {
            int errFull = LFPG_BTC_ERR_STOCK_FULL;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_DEPOSIT,errFull, currentStock, earlyBalD, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!atm.LFPG_CanAddBtcStock(btcAmount))
        {
            int errMutationNotAdmissible = LFPG_BTC_ERR_INVALID;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_DEPOSIT, errMutationNotAdmissible, currentStock, earlyBalD, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            LFPG_Util.Error("[BTCDeposit] stock mutation not admissible; deposit rejected before destruction");
            return;
        }

        if (!btcSessions.ReserveRequest(sender, serverSessionLow, serverSessionHigh, sequence, requestSubId, netLow, netHigh, btcAmount, false))
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_DEPOSIT, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        // Execute
        int destroyed = DestroyPlayerItems(player, btcClassname, btcAmount);

        // Guard: if nothing was destroyed, don't touch stock
        if (destroyed <= 0)
        {
            int errDestroyD = LFPG_BTC_ERR_NO_ITEMS;
            int curStockDD = atm.LFPG_GetBtcStock();
            SendBTCTxResult(player, sender, LFPG_BTC_TX_DEPOSIT,errDestroyD, curStockDD, earlyBalD, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            string logNoDD = "[BTCDeposit] failed to destroy any items";
            LFPG_Util.Warn(logNoDD);
            return;
        }

        // Permanent QA hook: the forced failure must SKIP the real mutation.
        // Forcing the flag after LFPG_AddBtcStock ran would leave the stock
        // incremented while the refund branch restores the items (BTC dupe).
        bool stockAddedDep;
        if (LFPG_DebugForceAddStockFail)
            stockAddedDep = false;
        else
            stockAddedDep = atm.LFPG_AddBtcStock(destroyed);
        if (!stockAddedDep)
        {
            // Race after pre-check. Restore destroyed items best-effort.
            int restoredDep = CreateItemsForPlayer(player, btcClassname, destroyed);
            int newStockDepR = atm.LFPG_GetBtcStock();
            int errCodeDep = LFPG_BTC_ERR_REFUNDED;
            if (restoredDep < destroyed)
            {
                errCodeDep = LFPG_BTC_ERR_REFUND_PARTIAL;
                string critDep = "[BTCDeposit] CRITICAL race + partial restore - player=";
                critDep = critDep + sender.GetId();
                critDep = critDep + " destroyed=";
                critDep = critDep + destroyed.ToString();
                critDep = critDep + " restored=";
                critDep = critDep + restoredDep.ToString();
                critDep = critDep + " lost=";
                critDep = critDep + (destroyed - restoredDep).ToString();
                critDep = critDep + " items";
                LFPG_Util.Error(critDep);
            }
            else
            {
                string warnDepRace = "[BTCDeposit] race - restored ";
                warnDepRace = warnDepRace + restoredDep.ToString();
                warnDepRace = warnDepRace + " items";
                LFPG_Util.Warn(warnDepRace);
            }
            SendBTCTxResult(player, sender, LFPG_BTC_TX_DEPOSIT,errCodeDep, newStockDepR, earlyBalD, restoredDep, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        // Updated state
        int newStock = atm.LFPG_GetBtcStock();
        int newBalance = earlyBalD;

        int okCode = LFPG_BTC_OK;
        SendBTCTxResult(player, sender, LFPG_BTC_TX_DEPOSIT,okCode, newStock, newBalance, destroyed, 0.0, serverSessionLow, serverSessionHigh, sequence);

        string logD = "[BTCDeposit] player deposited ";
        logD = logD + destroyed.ToString();
        logD = logD + " BTC into pool";
        LFPG_Util.Info(logD);
    }

    static int DestroyPlayerCash(PlayerBase player, int eurAmount)
    {
        int remaining = eurAmount;
        auto currencies = LFPG_BTCConfig.GetCurrencies();
        int cCount = currencies.Count();
        int ci;
        for (ci = 0; ci < cCount; ci = ci + 1)
        {
            LFPG_BTCCurrency cur = currencies[ci];
            if (!cur)
                continue;
            string cls = cur.classname;
            int denomination = cur.value;

            int playerHas = CountPlayerItems(player, cls);
            if (playerHas <= 0)
                continue;

            int needed = remaining / denomination;
            if (needed <= 0)
            {
                // Check if one bill of this denomination covers what's left
                if (denomination >= remaining)
                {
                    needed = 1;
                }
                else
                    continue;
            }

            if (needed > playerHas)
            {
                needed = playerHas;
            }

            int destroyed = DestroyPlayerItems(player, cls, needed);
            int valueDestroyed = destroyed * denomination;
            remaining = remaining - valueDestroyed;

            if (remaining <= 0)
            {
                break;
            }
        }

        // Ceiling pass: greedy alone cannot cover prices that don't divide
        // evenly into the denominations the player carries (e.g. price 67931
        // with only $100 bills leaves remaining=31 after destroying 679 bills).
        // Find the smallest denomination on hand that covers the residue and
        // destroy one extra bill. Callers refund the excess via GreedyChange.
        if (remaining > 0)
        {
            int sj = cCount - 1;
            while (sj >= 0)
            {
                LFPG_BTCCurrency curS = currencies[sj];
                if (curS && curS.value >= remaining)
                {
                    int hasS = CountPlayerItems(player, curS.classname);
                    if (hasS > 0)
                    {
                        int destS = DestroyPlayerItems(player, curS.classname, 1);
                        remaining = remaining - (destS * curS.value);
                        break;
                    }
                }
                sj = sj - 1;
            }
        }

        int totalDestroyed = eurAmount - remaining;
        return totalDestroyed;
    }

    static void HandleBTCWithdrawCash(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        #ifdef SERVER
        int netLow = 0;
        int netHigh = 0;
        int eurAmount = 0;
        int serverSessionLow = 0;
        int serverSessionHigh = 0;
        int sequence = 0;
        bool payloadOk = true;
        if (!ctx.Read(netLow))
            payloadOk = false;
        if (!ctx.Read(netHigh))
            payloadOk = false;
        if (!ctx.Read(eurAmount))
            payloadOk = false;
        if (!ctx.Read(serverSessionLow))
            payloadOk = false;
        if (!ctx.Read(serverSessionHigh))
            payloadOk = false;
        if (!ctx.Read(sequence))
            payloadOk = false;

        if (!sender)
            return;
        if (!payloadOk)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_WITHDRAW_CASH, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        int requestSubId = (int)LFPG_RPC_SubId.BTC_WITHDRAW_CASH;
        LFPG_BTCSessionRegistry btcSessions = LFPG_BTCSessionRegistry.Get();
        LFPG_BTCSessionResponse cachedResponse = null;
        int nonceState = btcSessions.CheckRequest(sender, serverSessionLow, serverSessionHigh, sequence, requestSubId, netLow, netHigh, eurAmount, false, cachedResponse);
        if (nonceState == LFPG_BTC_NONCE_REPLAY)
        {
            if (btcSessions.AllowReplayResponse(sender))
                SendBTCReplayResult(player, sender, cachedResponse);
            return;
        }
        if (nonceState == LFPG_BTC_NONCE_IN_FLIGHT)
            return;
        if (nonceState != LFPG_BTC_NONCE_NEW)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_WITHDRAW_CASH, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!LFPG_BTCConfig.IsEnabled())
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_WITHDRAW_CASH, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!LFPG_BalanceRegistry.IsAvailable())
        {
            int errNoBp = LFPG_BTC_ERR_NO_BALANCE_PROVIDER;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_WITHDRAW_CASH, errNoBp, 0, 0, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            string rlMsg = "Too fast! Wait a moment.";
            PlayerBase.LFPG_SendClientMsg(player, rlMsg);
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_WITHDRAW_CASH, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        string tag = "[BTCWithdrawCash]";
        LFPG_BTCAtmBase atm = ResolveAndValidate(player, netLow, netHigh, tag);
        if (!atm)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_WITHDRAW_CASH, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (eurAmount <= 0)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_WITHDRAW_CASH, serverSessionLow, serverSessionHigh, sequence);
            return;
        }
        int maxEurOpW = LFPG_BTCConfig.GetMaxEurPerOperation();
        if (eurAmount > maxEurOpW)
        {
            int errLargeWC = LFPG_BTC_ERR_AMOUNT_TOO_LARGE;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_WITHDRAW_CASH, errLargeWC, atm.LFPG_GetBtcStock(), 0, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            string warnLargeWC = "[BTCWithdrawCash] amount rejected (> cap): requested=";
            warnLargeWC = warnLargeWC + eurAmount.ToString();
            warnLargeWC = warnLargeWC + " cap=";
            warnLargeWC = warnLargeWC + maxEurOpW.ToString();
            LFPG_Util.Warn(warnLargeWC);
            return;
        }

        LFPG_BalanceProvider atmPb = LFPG_BalanceRegistry.GetActive();
        int currentBal = 0;
        if (atmPb)
        {
            currentBal = atmPb.GetBalance(player);
        }

        if (!atm.LFPG_IsATMPowered())
        {
            int errPow = LFPG_BTC_ERR_NOT_POWERED;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_WITHDRAW_CASH,errPow, atm.LFPG_GetBtcStock(), currentBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (currentBal < eurAmount)
        {
            int errFunds = LFPG_BTC_ERR_NO_FUNDS;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_WITHDRAW_CASH,errFunds, atm.LFPG_GetBtcStock(), currentBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!btcSessions.ReserveRequest(sender, serverSessionLow, serverSessionHigh, sequence, requestSubId, netLow, netHigh, eurAmount, false))
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_WITHDRAW_CASH, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        // BTC handlers are synchronous in one frame and the nonce registry owns
        // the per-player IN_FLIGHT slot, so the staged references cannot overlap
        // another accepted mutation before this commit or abort completes.
        LFPG_BTCInventoryPlan withdrawPlan = new LFPG_BTCInventoryPlan();
        float stagedRemainder = GreedyChange(player, eurAmount, withdrawPlan);
        if (stagedRemainder > 0.001)
        {
            withdrawPlan.AbortOutputs();
            int errStage = LFPG_BTC_ERR_INVENTORY_FULL;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_WITHDRAW_CASH, errStage, atm.LFPG_GetBtcStock(), currentBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        int removed = atmPb.RemoveBalance(player, eurAmount);
        if (removed != eurAmount)
        {
            withdrawPlan.AbortOutputs();
            if (removed > 0)
            {
                int refundedDebit = atmPb.AddBalance(player, removed);
                if (refundedDebit != removed)
                    LFPG_Util.Error("[BTCWithdrawCash] partial debit refund was not durable/exact");
            }
            int failedBal = atmPb.GetBalance(player);
            int errDurability = LFPG_BTC_ERR_INVALID;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_WITHDRAW_CASH, errDurability, atm.LFPG_GetBtcStock(), failedBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        int newBal = atmPb.GetBalance(player);
        float eurAmt = removed;
        int okCode = LFPG_BTC_OK;
        SendBTCTxResult(player, sender, LFPG_BTC_TX_WITHDRAW_CASH, okCode, atm.LFPG_GetBtcStock(), newBal, 0, eurAmt, serverSessionLow, serverSessionHigh, sequence);

        string logWC = "[BTCWithdrawCash] ";
        logWC = logWC + removed.ToString();
        logWC = logWC + " EUR withdrawn as bills";
        LFPG_Util.Info(logWC);
        #endif
    }

    static void HandleBTCDepositCash(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        #ifdef SERVER
        int netLow = 0;
        int netHigh = 0;
        int eurAmount = 0;
        int serverSessionLow = 0;
        int serverSessionHigh = 0;
        int sequence = 0;
        bool payloadOk = true;
        if (!ctx.Read(netLow))
            payloadOk = false;
        if (!ctx.Read(netHigh))
            payloadOk = false;
        if (!ctx.Read(eurAmount))
            payloadOk = false;
        if (!ctx.Read(serverSessionLow))
            payloadOk = false;
        if (!ctx.Read(serverSessionHigh))
            payloadOk = false;
        if (!ctx.Read(sequence))
            payloadOk = false;

        if (!sender)
            return;
        if (!payloadOk)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_DEPOSIT_CASH, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        int requestSubId = (int)LFPG_RPC_SubId.BTC_DEPOSIT_CASH;
        LFPG_BTCSessionRegistry btcSessions = LFPG_BTCSessionRegistry.Get();
        LFPG_BTCSessionResponse cachedResponse = null;
        int nonceState = btcSessions.CheckRequest(sender, serverSessionLow, serverSessionHigh, sequence, requestSubId, netLow, netHigh, eurAmount, false, cachedResponse);
        if (nonceState == LFPG_BTC_NONCE_REPLAY)
        {
            if (btcSessions.AllowReplayResponse(sender))
                SendBTCReplayResult(player, sender, cachedResponse);
            return;
        }
        if (nonceState == LFPG_BTC_NONCE_IN_FLIGHT)
            return;
        if (nonceState != LFPG_BTC_NONCE_NEW)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_DEPOSIT_CASH, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!LFPG_BTCConfig.IsEnabled())
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_DEPOSIT_CASH, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!LFPG_BalanceRegistry.IsAvailable())
        {
            int errNoBp = LFPG_BTC_ERR_NO_BALANCE_PROVIDER;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_DEPOSIT_CASH, errNoBp, 0, 0, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!LFPG_NetworkManager.Get().AllowPlayerAction(sender))
        {
            string rlMsg = "Too fast! Wait a moment.";
            PlayerBase.LFPG_SendClientMsg(player, rlMsg);
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_DEPOSIT_CASH, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        string tag = "[BTCDepositCash]";
        LFPG_BTCAtmBase atm = ResolveAndValidate(player, netLow, netHigh, tag);
        if (!atm)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_DEPOSIT_CASH, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (eurAmount <= 0)
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_DEPOSIT_CASH, serverSessionLow, serverSessionHigh, sequence);
            return;
        }
        int maxEurOpD = LFPG_BTCConfig.GetMaxEurPerOperation();
        if (eurAmount > maxEurOpD)
        {
            int errLargeDC = LFPG_BTC_ERR_AMOUNT_TOO_LARGE;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_DEPOSIT_CASH, errLargeDC, atm.LFPG_GetBtcStock(), 0, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            string warnLargeDC = "[BTCDepositCash] amount rejected (> cap): requested=";
            warnLargeDC = warnLargeDC + eurAmount.ToString();
            warnLargeDC = warnLargeDC + " cap=";
            warnLargeDC = warnLargeDC + maxEurOpD.ToString();
            LFPG_Util.Warn(warnLargeDC);
            return;
        }

        // Capture one provider before any physical side effect. The same
        // instance performs credit and any compensating debit.
        LFPG_BalanceProvider atmPb = LFPG_BalanceRegistry.GetActive();
        int earlyBal = 0;
        if (!atmPb)
        {
            int errProvider = LFPG_BTC_ERR_NO_BALANCE_PROVIDER;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_DEPOSIT_CASH, errProvider, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }
        earlyBal = atmPb.GetBalance(player);

        if (!atm.LFPG_IsATMPowered())
        {
            int errPow = LFPG_BTC_ERR_NOT_POWERED;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_DEPOSIT_CASH, errPow, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        // WithdrawOnly: nothing enters the account or the ATM.
        if (atm.LFPG_IsWithdrawOnly())
        {
            int errWo = LFPG_BTC_ERR_INVALID;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_DEPOSIT_CASH, errWo, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!btcSessions.ReserveRequest(sender, serverSessionLow, serverSessionHigh, sequence, requestSubId, netLow, netHigh, eurAmount, false))
        {
            SendBTCNonceRejection(player, sender, LFPG_BTC_TX_DEPOSIT_CASH, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        LFPG_BTCInventoryPlan depositCashPlan = new LFPG_BTCInventoryPlan();
        if (!PreparePlayerCash(player, depositCashPlan))
        {
            int errNoCash = LFPG_BTC_ERR_NO_CASH;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_DEPOSIT_CASH, errNoCash, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }
        if (depositCashPlan.GetPreparedCashValue() < eurAmount)
        {
            int errNoCash2 = LFPG_BTC_ERR_NO_CASH;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_DEPOSIT_CASH, errNoCash2, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        int creditedCash = atmPb.AddBalance(player, eurAmount);
        if (creditedCash <= 0)
        {
            int failedBal = atmPb.GetBalance(player);
            int errDurability = LFPG_BTC_ERR_INVALID;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_DEPOSIT_CASH, errDurability, atm.LFPG_GetBtcStock(), failedBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        if (!depositCashPlan.SelectPreparedCashExact(creditedCash))
        {
            int revertedCredit = atmPb.RemoveBalance(player, creditedCash);
            if (revertedCredit != creditedCash)
                LFPG_Util.Error("[BTCDepositCash] partial credit could not be represented and exact account revert failed");
            int revertedBal = atmPb.GetBalance(player);
            int errUnrepresentable = LFPG_BTC_ERR_INVALID;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_DEPOSIT_CASH, errUnrepresentable, atm.LFPG_GetBtcStock(), revertedBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        int committedCash = depositCashPlan.CommitPreparedCashValue(creditedCash);
        if (committedCash != creditedCash)
        {
            if (committedCash < creditedCash)
            {
                int unmatchedCredit = creditedCash - committedCash;
                int revertedUnmatched = atmPb.RemoveBalance(player, unmatchedCredit);
                if (revertedUnmatched != unmatchedCredit)
                    LFPG_Util.Error("[BTCDepositCash] staged cash commit failed and unmatched account credit could not be reverted exactly");
            }
            else
            {
                // Last-resort restitution: prevalidation makes over-commit
                // unreachable in normal synchronous operation.
                int restitutionAmount = committedCash - creditedCash;
                LFPG_BTCInventoryPlan restitutionPlan = new LFPG_BTCInventoryPlan();
                float restitutionResidual = GreedyChange(player, restitutionAmount, restitutionPlan);
                if (restitutionResidual > 0.0)
                {
                    string restitutionError = "[BTCDepositCash] restitution incomplete uid=";
                    restitutionError = restitutionError + sender.GetId();
                    restitutionError = restitutionError + " requestedRestitution=";
                    restitutionError = restitutionError + restitutionAmount.ToString();
                    restitutionError = restitutionError + " residual=";
                    restitutionError = restitutionError + restitutionResidual.ToString();
                    LFPG_Util.Error(restitutionError);
                }
            }
            int commitBal = atmPb.GetBalance(player);
            int errCommit = LFPG_BTC_ERR_INVALID;
            SendBTCTxResult(player, sender, LFPG_BTC_TX_DEPOSIT_CASH, errCommit, atm.LFPG_GetBtcStock(), commitBal, 0, 0.0, serverSessionLow, serverSessionHigh, sequence);
            return;
        }

        int newBal = atmPb.GetBalance(player);
        float eurAmt = creditedCash;
        int okCode = LFPG_BTC_OK;
        SendBTCTxResult(player, sender, LFPG_BTC_TX_DEPOSIT_CASH, okCode, atm.LFPG_GetBtcStock(), newBal, 0, eurAmt, serverSessionLow, serverSessionHigh, sequence);

        string logDC = "[BTCDepositCash] ";
        logDC = logDC + creditedCash.ToString();
        logDC = logDC + " EUR deposited from bills";
        LFPG_Util.Info(logDC);
        #endif
    }
};
