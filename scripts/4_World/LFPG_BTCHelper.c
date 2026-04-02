// =========================================================
// LF_PowerGrid - BTC ATM Helper (v5.0 Refactor)
//
// BTC ATM server handlers + utility methods extracted from
// modded PlayerBase into static methods.
//
// Utilities (CountPlayerItems, GreedyChange, etc.) have no
// LBmaster_Core dependency. Server handlers that use
// LB_ATM_Playerbase are wrapped in #ifdef LBmaster_Core.
// =========================================================

class LFPG_BTCHelper
{
    // =========================================================
    // BTC ATM: Utility methods (no LBmaster_Core dependency)
    // =========================================================

    static void SendBTCTxResult(PlayerBase player, int txType, int errCode, int newStock, int newBalance, int btcMoved, float eurAmount)
    {
        int cashOnInv = CountPlayerCash(player);
        string btcCls = LFPG_BTCConfig.GetBtcItemClassname();
        int btcOnInv = CountPlayerItems(player, btcCls);
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
        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);
    }

    static int CountPlayerItems(PlayerBase player, string classname)
    {
        int count = 0;
        int attIdx = 0;
        int attCount = 0;
        int cargoIdx = 0;
        int cargoCount = 0;
        EntityAI att = null;
        EntityAI item = null;
        CargoBase cargo = null;
        string itemType = "";

        // Hands
        HumanInventory hInv = player.GetHumanInventory();
        if (hInv)
        {
            EntityAI hands = hInv.GetEntityInHands();
            if (hands)
            {
                itemType = hands.GetType();
                if (itemType == classname)
                {
                    count = count + 1;
                }
            }
        }

        // Attachments (clothing) + their cargo
        attCount = player.GetInventory().AttachmentCount();
        for (attIdx = 0; attIdx < attCount; attIdx = attIdx + 1)
        {
            att = player.GetInventory().GetAttachmentFromIndex(attIdx);
            if (!att)
                continue;

            itemType = att.GetType();
            if (itemType == classname)
            {
                count = count + 1;
            }

            if (!att.GetInventory())
                continue;

            cargo = att.GetInventory().GetCargo();
            if (!cargo)
                continue;

            cargoCount = cargo.GetItemCount();
            for (cargoIdx = 0; cargoIdx < cargoCount; cargoIdx = cargoIdx + 1)
            {
                item = cargo.GetItem(cargoIdx);
                if (!item)
                    continue;
                itemType = item.GetType();
                if (itemType == classname)
                {
                    count = count + 1;
                }
            }
        }

        return count;
    }

    static int DestroyPlayerItems(PlayerBase player, string classname, int amount)
    {
        if (amount <= 0)
            return 0;

        array<EntityAI> toDelete = new array<EntityAI>();

        int attIdx = 0;
        int attCount = 0;
        int cargoIdx = 0;
        int cargoCount = 0;
        EntityAI att = null;
        EntityAI item = null;
        CargoBase cargo = null;
        string itemType = "";

        // Hands first
        HumanInventory hInvD = player.GetHumanInventory();
        if (hInvD)
        {
            EntityAI hands = hInvD.GetEntityInHands();
            if (hands && toDelete.Count() < amount)
            {
                itemType = hands.GetType();
                if (itemType == classname)
                {
                    toDelete.Insert(hands);
                }
            }
        }

        // Attachments cargo
        attCount = player.GetInventory().AttachmentCount();
        for (attIdx = 0; attIdx < attCount; attIdx = attIdx + 1)
        {
            if (toDelete.Count() >= amount)
                break;

            att = player.GetInventory().GetAttachmentFromIndex(attIdx);
            if (!att)
                continue;

            itemType = att.GetType();
            if (itemType == classname && toDelete.Count() < amount)
            {
                toDelete.Insert(att);
            }

            if (!att.GetInventory())
                continue;

            cargo = att.GetInventory().GetCargo();
            if (!cargo)
                continue;

            cargoCount = cargo.GetItemCount();
            for (cargoIdx = 0; cargoIdx < cargoCount; cargoIdx = cargoIdx + 1)
            {
                if (toDelete.Count() >= amount)
                    break;

                item = cargo.GetItem(cargoIdx);
                if (!item)
                    continue;
                itemType = item.GetType();
                if (itemType == classname)
                {
                    toDelete.Insert(item);
                }
            }
        }

        // Delete collected items
        int destroyed = 0;
        int di = 0;
        int dCount = toDelete.Count();
        for (di = 0; di < dCount; di = di + 1)
        {
            EntityAI delItem = toDelete[di];
            if (delItem)
            {
                GetGame().ObjectDelete(delItem);
                destroyed = destroyed + 1;
            }
        }

        return destroyed;
    }

    static int CreateItemsForPlayer(PlayerBase player, string classname, int amount)
    {
        if (amount <= 0)
            return 0;

        int created = 0;
        int groundDrops = 0;
        int i = 0;
        EntityAI newItem = null;
        Object groundObj = null;
        EntityAI groundItem = null;

        for (i = 0; i < amount; i = i + 1)
        {
            newItem = player.GetInventory().CreateInInventory(classname);
            if (newItem)
            {
                created = created + 1;
            }
            else
            {
                // Ground fallback
                vector pos = player.GetPosition();
                float ox = Math.RandomFloat(-0.5, 0.5);
                float oz = Math.RandomFloat(-0.5, 0.5);
                pos[0] = pos[0] + ox;
                pos[2] = pos[2] + oz;
                int flags = ECE_CREATEPHYSICS;
                groundObj = GetGame().CreateObjectEx(classname, pos, flags);
                groundItem = EntityAI.Cast(groundObj);
                if (groundItem)
                {
                    created = created + 1;
                    groundDrops = groundDrops + 1;
                }
            }
        }

        if (groundDrops > 0)
        {
            string dropMsg = "[BTC] ";
            dropMsg = dropMsg + groundDrops.ToString();
            dropMsg = dropMsg + " items dropped on ground (inventory full)";
            LFPG_Util.Info(dropMsg);
            string clientDropMsg = "Some items were dropped on the ground.";
            PlayerBase.LFPG_SendClientMsg(player, clientDropMsg);
        }

        return created;
    }

    static float GreedyChange(PlayerBase player, float eurAmount)
    {
        if (eurAmount <= 0.0)
            return 0.0;

        int intAmount = (int)eurAmount;
        float fractional = eurAmount - intAmount;

        array<ref LFPG_BTCCurrency> currencies = LFPG_BTCConfig.GetCurrencies();
        if (!currencies)
            return eurAmount;

        int cCount = currencies.Count();
        if (cCount == 0)
            return eurAmount;

        int remaining = intAmount;
        int totalItems = 0;
        int ci = 0;
        int billCount = 0;
        int spaceLeft = 0;
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

            // Safety cap
            spaceLeft = LFPG_BTC_MAX_CHANGE_ITEMS - totalItems;
            if (spaceLeft <= 0)
                break;
            if (billCount > spaceLeft)
            {
                billCount = spaceLeft;
            }

            createdBills = CreateItemsForPlayer(player, cur.classname, billCount);
            eurGiven = createdBills * cur.value;
            remaining = remaining - eurGiven;
            totalItems = totalItems + createdBills;
        }

        // Remainder = fractional cents + any integer overflow from cap
        float totalRemainder = fractional + remaining;

        if (totalItems >= LFPG_BTC_MAX_CHANGE_ITEMS && remaining > 0)
        {
            string capMsg = "[BTC] Change item cap reached. Remaining: ";
            capMsg = capMsg + remaining.ToString();
            capMsg = capMsg + " EUR to machine remainder";
            LFPG_Util.Warn(capMsg);
            string clientCapMsg = "Transaction too large for cash. Some EUR stored as credit.";
            PlayerBase.LFPG_SendClientMsg(player, clientCapMsg);
        }

        return totalRemainder;
    }

    static LFPG_BTCAtmBase ResolveAndValidate(PlayerBase player, ParamsReadContext ctx, string tag)
    {
        int netLow = 0;
        int netHigh = 0;
        if (!ctx.Read(netLow))
            return null;
        if (!ctx.Read(netHigh))
            return null;

        // Resolve entity
        Object rawObj = GetGame().GetObjectByNetworkId(netLow, netHigh);
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

        // Distance check
        float dist = vector.Distance(player.GetPosition(), devEnt.GetPosition());
        if (dist > LFPG_INTERACT_DIST_M)
        {
            string errDist = tag;
            errDist = errDist + " player too far";
            LFPG_Util.Warn(errDist);
            return null;
        }

        // Ruined check
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
        array<ref LFPG_BTCCurrency> currencies = LFPG_BTCConfig.GetCurrencies();
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

    // =========================================================
    // BTC ATM: Server Handlers (require LBmaster_Core)
    // =========================================================
    #ifdef LBmaster_Core

    static void HandleBTCOpenRequest(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
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
        LFPG_BTCAtmBase atm = ResolveAndValidate(player, ctx, tag);
        if (!atm)
            return;

        // Powered check
        if (!atm.LFPG_IsATMPowered())
        {
            string errPower = "ATM has no power.";
            PlayerBase.LFPG_SendClientMsg(player, errPower);
            return;
        }

        // Price: use -1.0 sentinel if unavailable (client detects this)
        float price = -1.0;
        bool priceOk = LFPG_NetworkManager.Get().LFPG_IsBTCPriceAvailable();
        if (priceOk)
        {
            price = LFPG_NetworkManager.Get().LFPG_GetBTCPrice();
        }

        // Read player balance (LBMaster)
        LB_ATM_Playerbase atmPlayer = new LB_ATM_Playerbase(player);
        int balance = atmPlayer.GetATMMoney();

        // ATM state
        int stock = atm.LFPG_GetBtcStock();
        bool withdrawOnly = atm.LFPG_IsWithdrawOnly();

        // Player cash on person
        int cashOnInv = CountPlayerCash(player);

        // A3: Player BTC items on person
        string btcClsOpen = LFPG_BTCConfig.GetBtcItemClassname();
        int btcOnInv = CountPlayerItems(player, btcClsOpen);

        // 24h price change percent
        float priceChange24h = LFPG_NetworkManager.Get().LFPG_GetBTC24hChange();

        // Send response (always — client handles price=-1.0 as N/A)
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
        rpc.Send(player, LFPG_RPC_CHANNEL, true, null);

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

        string tag = "[BTCBuy]";
        LFPG_BTCAtmBase atm = ResolveAndValidate(player, ctx, tag);
        if (!atm)
            return;

        int btcAmount = 0;
        if (!ctx.Read(btcAmount))
            return;

        // B3: optional useAccount (retrocompat default=true = account)
        bool useAccount = true;
        ctx.Read(useAccount);

        if (btcAmount <= 0)
            return;

        // Read balance early for error responses
        LB_ATM_Playerbase atmEarly = new LB_ATM_Playerbase(player);
        int earlyBal = atmEarly.GetATMMoney();

        // Powered
        if (!atm.LFPG_IsATMPowered())
        {
            int errPow = LFPG_BTC_ERR_NOT_POWERED;
            SendBTCTxResult(player, LFPG_BTC_TX_BUY, errPow, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0);
            return;
        }

        // Price
        bool priceOk = LFPG_NetworkManager.Get().LFPG_IsBTCPriceAvailable();
        if (!priceOk)
        {
            int errPrice = LFPG_BTC_ERR_NO_PRICE;
            SendBTCTxResult(player, LFPG_BTC_TX_BUY, errPrice, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0);
            return;
        }
        float price = LFPG_NetworkManager.Get().LFPG_GetBTCPrice();

        // B1: Stock check REMOVED — Buy spawns unlimited BTC

        // Cost calculation (ceiling = player pays more on fractions)
        float costFloat = btcAmount * price;
        int costInt = (int)costFloat;
        float costDiff = costFloat - costInt;
        if (costDiff > 0.001)
        {
            costInt = costInt + 1;
        }

        if (useAccount)
        {
            // ── Account mode: LBMaster −EUR → ATM stock +BTC (virtual, no spawn) ──
            int playerBalance = earlyBal;
            if (playerBalance < costInt)
            {
                int errFunds = LFPG_BTC_ERR_NO_FUNDS;
                SendBTCTxResult(player, LFPG_BTC_TX_BUY, errFunds, atm.LFPG_GetBtcStock(), playerBalance, 0, costFloat);
                return;
            }

            int removed = atmEarly.RemoveATMMoney(costInt);
            if (removed <= 0)
            {
                int errRemove = LFPG_BTC_ERR_NO_FUNDS;
                int curBalR = atmEarly.GetATMMoney();
                SendBTCTxResult(player, LFPG_BTC_TX_BUY, errRemove, atm.LFPG_GetBtcStock(), curBalR, 0, 0.0);
                return;
            }

            // Recalculate affordable BTC if removed < costInt
            int affordableBtc = btcAmount;
            if (removed < costInt)
            {
                float affordFloat = removed / price;
                affordableBtc = (int)affordFloat;
                if (affordableBtc <= 0)
                {
                    atmEarly.AddATMMoney(removed);
                    int errPartial = LFPG_BTC_ERR_NO_FUNDS;
                    int curBalP = atmEarly.GetATMMoney();
                    SendBTCTxResult(player, LFPG_BTC_TX_BUY, errPartial, atm.LFPG_GetBtcStock(), curBalP, 0, 0.0);
                    return;
                }
                if (affordableBtc > btcAmount)
                {
                    affordableBtc = btcAmount;
                }
            }

            // Add BTC to ATM stock (virtual — player uses Withdraw to get physical items)
            atm.LFPG_AddBtcStock(affordableBtc);

            // Calculate actual cost for BTC delivered
            float actualCostFloat = affordableBtc * price;
            int actualCostInt = (int)actualCostFloat;
            float actualCostDiff = actualCostFloat - actualCostInt;
            if (actualCostDiff > 0.001)
            {
                actualCostInt = actualCostInt + 1;
            }
            if (actualCostInt > removed)
            {
                actualCostInt = removed;
            }

            // Refund excess
            int refundAmount = removed - actualCostInt;
            if (refundAmount > 0)
            {
                atmEarly.AddATMMoney(refundAmount);
            }

            int newBalance = atmEarly.GetATMMoney();
            int newStock = atm.LFPG_GetBtcStock();
            int okCode = LFPG_BTC_OK;
            float eurSpent = actualCostInt;
            SendBTCTxResult(player, LFPG_BTC_TX_BUY, okCode, newStock, newBalance, affordableBtc, eurSpent);

            string logBuyA = "[BTCBuy] account→stock: ";
            logBuyA = logBuyA + affordableBtc.ToString();
            logBuyA = logBuyA + " BTC for ";
            logBuyA = logBuyA + actualCostInt.ToString();
            logBuyA = logBuyA + " EUR";
            LFPG_Util.Info(logBuyA);
        }
        else
        {
            // ── Cash mode: charge from physical EUR bills ──
            int destroyed = DestroyPlayerCash(player, costInt);
            if (destroyed <= 0)
            {
                int errNoCash = LFPG_BTC_ERR_NO_CASH;
                SendBTCTxResult(player, LFPG_BTC_TX_BUY, errNoCash, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0);
                return;
            }

            // Guard: if destroyed < costInt, player didn't have enough
            if (destroyed < costInt)
            {
                // Refund what was destroyed
                GreedyChange(player, destroyed);
                int errNoCash2 = LFPG_BTC_ERR_NO_CASH;
                SendBTCTxResult(player, LFPG_BTC_TX_BUY, errNoCash2, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0);
                return;
            }

            // Return change if destroyed > costInt
            int excess = destroyed - costInt;
            if (excess > 0)
            {
                GreedyChange(player, excess);
            }

            // Spawn BTC items
            string btcClsCash = LFPG_BTCConfig.GetBtcItemClassname();
            int createdCash = CreateItemsForPlayer(player, btcClsCash, btcAmount);

            if (createdCash <= 0)
            {
                // Refund NET amount (costInt, not destroyed — excess already returned)
                GreedyChange(player, costInt);
                int errInvC = LFPG_BTC_ERR_INVENTORY_FULL;
                SendBTCTxResult(player, LFPG_BTC_TX_BUY, errInvC, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0);
                return;
            }

            int newStockC = atm.LFPG_GetBtcStock();
            LB_ATM_Playerbase atmFinalC = new LB_ATM_Playerbase(player);
            int newBalC = atmFinalC.GetATMMoney();
            int okCodeC = LFPG_BTC_OK;
            float eurSpentC = costInt;
            SendBTCTxResult(player, LFPG_BTC_TX_BUY, okCodeC, newStockC, newBalC, createdCash, eurSpentC);

            string logBuyC = "[BTCBuy] cash: ";
            logBuyC = logBuyC + createdCash.ToString();
            logBuyC = logBuyC + " BTC for ";
            logBuyC = logBuyC + costInt.ToString();
            logBuyC = logBuyC + " EUR bills";
            LFPG_Util.Info(logBuyC);
        }
    }

    static void HandleBTCSell(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
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

        string tag = "[BTCSell]";
        LFPG_BTCAtmBase atm = ResolveAndValidate(player, ctx, tag);
        if (!atm)
            return;

        int btcAmount = 0;
        if (!ctx.Read(btcAmount))
            return;

        bool toAccount = false;
        if (!ctx.Read(toAccount))
            return;

        if (btcAmount <= 0)
            return;

        // Read balance early for error responses
        LB_ATM_Playerbase atmEarlyS = new LB_ATM_Playerbase(player);
        int earlyBalS = atmEarlyS.GetATMMoney();

        // Powered
        if (!atm.LFPG_IsATMPowered())
        {
            int errPow = LFPG_BTC_ERR_NOT_POWERED;
            SendBTCTxResult(player, LFPG_BTC_TX_SELL, errPow, atm.LFPG_GetBtcStock(), earlyBalS, 0, 0.0);
            return;
        }

        // Price
        bool priceOk = LFPG_NetworkManager.Get().LFPG_IsBTCPriceAvailable();
        if (!priceOk)
        {
            int errPrice = LFPG_BTC_ERR_NO_PRICE;
            SendBTCTxResult(player, LFPG_BTC_TX_SELL, errPrice, atm.LFPG_GetBtcStock(), earlyBalS, 0, 0.0);
            return;
        }
        float price = LFPG_NetworkManager.Get().LFPG_GetBTCPrice();

        // Check player has enough BTC items
        string btcClassname = LFPG_BTCConfig.GetBtcItemClassname();
        int playerBtc = CountPlayerItems(player, btcClassname);
        if (playerBtc < btcAmount)
        {
            int errItems = LFPG_BTC_ERR_NO_ITEMS;
            SendBTCTxResult(player, LFPG_BTC_TX_SELL, errItems, atm.LFPG_GetBtcStock(), earlyBalS, 0, 0.0);
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

        // Execute: destroy BTC items first
        int destroyed = DestroyPlayerItems(player, btcClassname, btcAmount);

        // Guard: if nothing was destroyed, abort
        if (destroyed <= 0)
        {
            int errDestroy = LFPG_BTC_ERR_NO_ITEMS;
            int curStockD = atm.LFPG_GetBtcStock();
            int curBalD = atmEarlyS.GetATMMoney();
            SendBTCTxResult(player, LFPG_BTC_TX_SELL, errDestroy, curStockD, curBalD, 0, 0.0);
            string logNoD = "[BTCSell] failed to destroy any items";
            LFPG_Util.Warn(logNoD);
            return;
        }

        // Calculate EUR revenue based on actual destroyed count
        float eurTotal = destroyed * price;

        // Sold BTC disappear from the game (no stock add)

        // Pay player
        if (toAccount)
        {
            // Integer amount to account
            int eurInt = (int)eurTotal;
            int added = atmEarlyS.AddATMMoney(eurInt);

            // If account was full or partially full, give remainder as cash
            int notAdded = eurInt - added;
            if (notAdded > 0)
            {
                float cashFloat = notAdded;
                float remainCash = GreedyChange(player, cashFloat);
                if (remainCash > 0.001)
                {
                    float curRemC = atm.LFPG_GetDecimalRemainder();
                    float newRemC = curRemC + remainCash;
                    atm.LFPG_SetDecimalRemainder(newRemC);
                }

                string spillMsg = "[BTCSell] account full, spilled ";
                spillMsg = spillMsg + notAdded.ToString();
                spillMsg = spillMsg + " EUR to cash";
                LFPG_Util.Info(spillMsg);
            }
        }
        else
        {
            // Cash: greedy change, remainder to ATM
            float remainder = GreedyChange(player, eurTotal);
            if (remainder > 0.001)
            {
                float curRemainder = atm.LFPG_GetDecimalRemainder();
                float newRemainder = curRemainder + remainder;
                atm.LFPG_SetDecimalRemainder(newRemainder);
            }
        }

        // Read updated state
        int newBalance = atmEarlyS.GetATMMoney();
        int newStock = atm.LFPG_GetBtcStock();

        int okCode = LFPG_BTC_OK;
        SendBTCTxResult(player, LFPG_BTC_TX_SELL, okCode, newStock, newBalance, destroyed, eurTotal);

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

        string tag = "[BTCWithdraw]";
        LFPG_BTCAtmBase atm = ResolveAndValidate(player, ctx, tag);
        if (!atm)
            return;

        int btcAmount = 0;
        if (!ctx.Read(btcAmount))
            return;

        if (btcAmount <= 0)
            return;

        // Read balance early for error responses
        LB_ATM_Playerbase atmEarlyW = new LB_ATM_Playerbase(player);
        int earlyBalW = atmEarlyW.GetATMMoney();

        // Powered
        if (!atm.LFPG_IsATMPowered())
        {
            int errPow = LFPG_BTC_ERR_NOT_POWERED;
            SendBTCTxResult(player, LFPG_BTC_TX_WITHDRAW, errPow, atm.LFPG_GetBtcStock(), earlyBalW, 0, 0.0);
            return;
        }

        // Stock check
        int currentStock = atm.LFPG_GetBtcStock();
        if (btcAmount > currentStock)
        {
            int errStock = LFPG_BTC_ERR_NO_STOCK;
            SendBTCTxResult(player, LFPG_BTC_TX_WITHDRAW, errStock, currentStock, earlyBalW, 0, 0.0);
            return;
        }

        // Execute: create items FIRST, then remove stock only for what was delivered
        string btcClassname = LFPG_BTCConfig.GetBtcItemClassname();
        int created = CreateItemsForPlayer(player, btcClassname, btcAmount);

        // Guard: if nothing was created, don't touch stock
        if (created <= 0)
        {
            int errInvW = LFPG_BTC_ERR_INVENTORY_FULL;
            int curStockW = atm.LFPG_GetBtcStock();
            SendBTCTxResult(player, LFPG_BTC_TX_WITHDRAW, errInvW, curStockW, earlyBalW, 0, 0.0);
            string logNoW = "[BTCWithdraw] inventory full, no items created";
            LFPG_Util.Warn(logNoW);
            return;
        }

        atm.LFPG_RemoveBtcStock(created);

        // Updated state
        int newStock = atm.LFPG_GetBtcStock();
        int newBalance = earlyBalW;

        int okCode = LFPG_BTC_OK;
        SendBTCTxResult(player, LFPG_BTC_TX_WITHDRAW, okCode, newStock, newBalance, created, 0.0);

        string logW = "[BTCWithdraw] player withdrew ";
        logW = logW + created.ToString();
        logW = logW + " BTC from pool";
        LFPG_Util.Info(logW);
    }

    static void HandleBTCDeposit(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
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

        string tag = "[BTCDeposit]";
        LFPG_BTCAtmBase atm = ResolveAndValidate(player, ctx, tag);
        if (!atm)
            return;

        int btcAmount = 0;
        if (!ctx.Read(btcAmount))
            return;

        if (btcAmount <= 0)
            return;

        // Read balance early for error responses
        LB_ATM_Playerbase atmEarlyD = new LB_ATM_Playerbase(player);
        int earlyBalD = atmEarlyD.GetATMMoney();

        // Powered
        if (!atm.LFPG_IsATMPowered())
        {
            int errPow = LFPG_BTC_ERR_NOT_POWERED;
            SendBTCTxResult(player, LFPG_BTC_TX_DEPOSIT, errPow, atm.LFPG_GetBtcStock(), earlyBalD, 0, 0.0);
            return;
        }

        // Player has items?
        string btcClassname = LFPG_BTCConfig.GetBtcItemClassname();
        int playerBtc = CountPlayerItems(player, btcClassname);
        if (playerBtc < btcAmount)
        {
            int errItems = LFPG_BTC_ERR_NO_ITEMS;
            SendBTCTxResult(player, LFPG_BTC_TX_DEPOSIT, errItems, atm.LFPG_GetBtcStock(), earlyBalD, 0, 0.0);
            return;
        }

        // ATM has room?
        int maxStock = LFPG_BTCConfig.GetMaxBtcPerMachine();
        int currentStock = atm.LFPG_GetBtcStock();
        int newStockTest = currentStock + btcAmount;
        if (newStockTest > maxStock)
        {
            int errFull = LFPG_BTC_ERR_STOCK_FULL;
            SendBTCTxResult(player, LFPG_BTC_TX_DEPOSIT, errFull, currentStock, earlyBalD, 0, 0.0);
            return;
        }

        // Execute
        int destroyed = DestroyPlayerItems(player, btcClassname, btcAmount);

        // Guard: if nothing was destroyed, don't touch stock
        if (destroyed <= 0)
        {
            int errDestroyD = LFPG_BTC_ERR_NO_ITEMS;
            int curStockDD = atm.LFPG_GetBtcStock();
            SendBTCTxResult(player, LFPG_BTC_TX_DEPOSIT, errDestroyD, curStockDD, earlyBalD, 0, 0.0);
            string logNoDD = "[BTCDeposit] failed to destroy any items";
            LFPG_Util.Warn(logNoDD);
            return;
        }

        atm.LFPG_AddBtcStock(destroyed);

        // Updated state
        int newStock = atm.LFPG_GetBtcStock();
        int newBalance = earlyBalD;

        int okCode = LFPG_BTC_OK;
        SendBTCTxResult(player, LFPG_BTC_TX_DEPOSIT, okCode, newStock, newBalance, destroyed, 0.0);

        string logD = "[BTCDeposit] player deposited ";
        logD = logD + destroyed.ToString();
        logD = logD + " BTC into pool";
        LFPG_Util.Info(logD);
    }

    static int DestroyPlayerCash(PlayerBase player, int eurAmount)
    {
        int remaining = eurAmount;
        array<ref LFPG_BTCCurrency> currencies = LFPG_BTCConfig.GetCurrencies();
        int ci = 0;
        int cCount = currencies.Count();
        while (ci < cCount)
        {
            LFPG_BTCCurrency cur = currencies[ci];
            string cls = cur.classname;
            int denomination = cur.value;

            int playerHas = CountPlayerItems(player, cls);
            if (playerHas <= 0)
            {
                ci = ci + 1;
                continue;
            }

            int needed = remaining / denomination;
            if (needed <= 0)
            {
                // Check if one bill of this denomination covers what's left
                if (denomination >= remaining)
                {
                    needed = 1;
                }
                else
                {
                    ci = ci + 1;
                    continue;
                }
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

            ci = ci + 1;
        }

        int totalDestroyed = eurAmount - remaining;
        return totalDestroyed;
    }

    static void HandleBTCWithdrawCash(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        #ifdef SERVER
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

        string tag = "[BTCWithdrawCash]";
        LFPG_BTCAtmBase atm = ResolveAndValidate(player, ctx, tag);
        if (!atm)
            return;

        int eurAmount = 0;
        if (!ctx.Read(eurAmount))
            return;

        if (eurAmount <= 0)
            return;

        LB_ATM_Playerbase atmPb = new LB_ATM_Playerbase(player);
        int currentBal = atmPb.GetATMMoney();

        if (!atm.LFPG_IsATMPowered())
        {
            int errPow = LFPG_BTC_ERR_NOT_POWERED;
            SendBTCTxResult(player, LFPG_BTC_TX_WITHDRAW_CASH, errPow, atm.LFPG_GetBtcStock(), currentBal, 0, 0.0);
            return;
        }

        if (currentBal < eurAmount)
        {
            int errFunds = LFPG_BTC_ERR_NO_FUNDS;
            SendBTCTxResult(player, LFPG_BTC_TX_WITHDRAW_CASH, errFunds, atm.LFPG_GetBtcStock(), currentBal, 0, 0.0);
            return;
        }

        int removed = atmPb.RemoveATMMoney(eurAmount);
        if (removed <= 0)
        {
            int errFunds2 = LFPG_BTC_ERR_NO_FUNDS;
            SendBTCTxResult(player, LFPG_BTC_TX_WITHDRAW_CASH, errFunds2, atm.LFPG_GetBtcStock(), currentBal, 0, 0.0);
            return;
        }

        // Spawn EUR bills
        GreedyChange(player, removed);

        int newBal = atmPb.GetATMMoney();
        float eurAmt = removed;
        int okCode = LFPG_BTC_OK;
        SendBTCTxResult(player, LFPG_BTC_TX_WITHDRAW_CASH, okCode, atm.LFPG_GetBtcStock(), newBal, 0, eurAmt);

        string logWC = "[BTCWithdrawCash] ";
        logWC = logWC + removed.ToString();
        logWC = logWC + " EUR withdrawn as bills";
        LFPG_Util.Info(logWC);
        #endif
    }

    static void HandleBTCDepositCash(PlayerBase player, PlayerIdentity sender, ParamsReadContext ctx)
    {
        #ifdef SERVER
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

        string tag = "[BTCDepositCash]";
        LFPG_BTCAtmBase atm = ResolveAndValidate(player, ctx, tag);
        if (!atm)
            return;

        int eurAmount = 0;
        if (!ctx.Read(eurAmount))
            return;

        if (eurAmount <= 0)
            return;

        LB_ATM_Playerbase atmE = new LB_ATM_Playerbase(player);
        int earlyBal = atmE.GetATMMoney();

        if (!atm.LFPG_IsATMPowered())
        {
            int errPow = LFPG_BTC_ERR_NOT_POWERED;
            SendBTCTxResult(player, LFPG_BTC_TX_DEPOSIT_CASH, errPow, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0);
            return;
        }

        // Destroy EUR bills from player
        int destroyed = DestroyPlayerCash(player, eurAmount);
        if (destroyed <= 0)
        {
            int errNoCash = LFPG_BTC_ERR_NO_CASH;
            SendBTCTxResult(player, LFPG_BTC_TX_DEPOSIT_CASH, errNoCash, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0);
            return;
        }

        // Guard: if destroyed < eurAmount, player didn't have enough
        if (destroyed < eurAmount)
        {
            // Refund what was destroyed
            GreedyChange(player, destroyed);
            int errNoCash2 = LFPG_BTC_ERR_NO_CASH;
            SendBTCTxResult(player, LFPG_BTC_TX_DEPOSIT_CASH, errNoCash2, atm.LFPG_GetBtcStock(), earlyBal, 0, 0.0);
            return;
        }

        // Return change if destroyed > eurAmount
        int excess = destroyed - eurAmount;
        if (excess > 0)
        {
            GreedyChange(player, excess);
        }

        // Credit to LBMaster (exactly eurAmount — excess already returned)
        LB_ATM_Playerbase atmPb = new LB_ATM_Playerbase(player);
        atmPb.AddATMMoney(eurAmount);

        int newBal = atmPb.GetATMMoney();
        float eurAmt = eurAmount;
        int okCode = LFPG_BTC_OK;
        SendBTCTxResult(player, LFPG_BTC_TX_DEPOSIT_CASH, okCode, atm.LFPG_GetBtcStock(), newBal, 0, eurAmt);

        string logDC = "[BTCDepositCash] ";
        logDC = logDC + eurAmount.ToString();
        logDC = logDC + " EUR deposited from bills";
        LFPG_Util.Info(logDC);
        #endif
    }
    #endif // LBmaster_Core
};
