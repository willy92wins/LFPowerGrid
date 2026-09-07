# IMPL-T1 — integridad monetaria

Implementación sobre el árbol de trabajo (rama `fix/t1-integridad-monetaria`). **No hay commit.** No se ejecutó AddonBuilder, PBO ni DayZ. No se tocó `LFPowerGrid_dev`, sorter, RPC extra ni `config.cpp`.

**Ficheros tocados**

- `scripts/3_Game/LFPG_FileUtil.c`
- `scripts/3_Game/LFPG_BTCConfig.c`
- `scripts/5_Mission/LFPG_BTCHelper.c`
- `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c`
- `scripts/5_Mission/LFPG_MissionInit.c`

**E15 (contexto, no implementada):** el reorder de Sell no crea claims ni cambia `ReconcilePendingChain`. El tope de 8 compras PENDING por ATM hasta boot sigue igual. Compactar tombstones REFUNDED cuando el stock hive ya coincide no toca esa cuota. El marcador de venta a cuenta es un sibling de `LF_Balances.json`, no un `LFPG_BalanceClaim`.

---

### E04 — CERRADA
- **Qué cambié:** `scripts/5_Mission/LFPG_BTCHelper.c` `HandleBTCSell`. El spill/payout en efectivo se materializa **antes** de tocar los BTC. Antes de `AddBalance` se escribe un marcador sibling `LF_Balances.json.sell.<uid>` (`LFPG_FileUtil.WriteSellDestroyIntent`) con saldo previo, crédito, cantidad y classname. El crédito a cuenta se persiste **con los BTC todavía en inventario**. Solo entonces `DestroyPlayerItems`. Si el crédito no coincide con el preflight, se intenta `RemoveBalance` del parcial, se borra el marcador, se aborta el spill y se sale con BTC intactos. Si la destrucción es parcial o nula: revertir crédito, restituir BTC con `RestoreDestroyedItems` (sin tope de entidades), borrar el marcador y abortar el spill **solo** si la restitución fue exacta (si no, el efectivo queda como compensación). Tras destruir el importe exacto se borra el marcador. `MissionServer.InvokeOnConnect` llama `ReconcilePendingAccountSell`: si el saldo Durable es exactamente `before+credit`, destruye hasta `btcAmount` y limpia el marcador; si el saldo sigue en `before`, limpia sin tocar items; si es cualquier otro valor, no destruye y deja el marcador para admin.
- **Por qué así:** una obligación persistente / journal de venta en `s_Claims` habría tocado el almacén de claims y el cap de 8 PENDING de E15. El sibling copia el patrón `.saving` de SEC09 y no añade API al proveedor de saldo. Invertir crédito/destrucción cierra la pérdida irreversible ante `AtomicSaveBalances` false.
- **Qué NO cubre:** si `DeleteFile` del marcador falla tras una venta ya completada, el siguiente `InvokeOnConnect` con saldo todavía igual a `before+credit` puede volver a destruir BTC (misma clase de residuo que un `.saving` huérfano). Saldo distinto de `before` y de `before+credit` queda para admin. La destrucción en reconcilio no espera a que el hive de inventario confirme el `ObjectDelete`. Si `RemoveBalance` de compensación también falla, puede quedar crédito huérfano con BTC restituidos.
- **Cómo comprobarlo:** Sell a cuenta con inyección de fallo en `AtomicSaveBalances` tras el staging de spill: BTC y EUR de spill conservados, error INVALID, cero crédito, sin marcador. Crash tras `AddBalance` OK y antes de destruir: al reconectar, crédito intacto y BTC destruidos. Crash tras escribir el marcador y antes de `AddBalance`: al reconectar, BTC intactos y marcador limpiado.

### SEC09 — PARCIAL
- **Qué cambié:** `scripts/3_Game/LFPG_FileUtil.c`. La guarda de `.tmp` junto a target vivo (`ef29b73`) **no se rehízo**. (a) Si `SaveFile` falla dejando un `.tmp` parseable, `DiscardAbortedBalancesTmp` lo saca de la ruta de promoción **antes** de escribir el marcador. El marcador `target + ".saving"` se escribe solo después de verificar el tmp y de stagar `.bak.new`. `EnsureBalancesFileOrRestore` (~621-683) promociona `.tmp` solo si: no hay target vivo, hay marcador, y hay backup (`.bak.new` o `.bak`). First-save / abort reportado → fail-closed, no resurrección. Si `CopyFile` de promoción falla: se borra el marcador **antes** de restaurar el target desde bak.
- **Por qué así:** un resultado `committed/aborted/uncertain` en Native habría cambiado el contrato de `AddBalance` y todos sus call-sites. El marcador distingue crash in-flight de abort reportado sin tocar el JSON de balances.
- **Qué NO cubre:** vía (b) residual. Si `CopyFile(tmp,target)` falla, `DeleteFile` del `.saving` **también** falla, la restauración del target desde bak **también** falla, y quedan `.tmp` parseable + marcador + backup: el siguiente boot **sí** puede promover una mutación abortada. El log pide intervención admin (`delete … .saving` antes de reiniciar). Settings/wires no usan este protocolo (fuera de alcance).
- **Cómo comprobarlo:** `SaveFile` deja `.tmp` parseable sin target y sin `.saving` → boot no acredita. Crash en la ventana Delete+Copy con `.saving` + bak y sin target → sí promover. Fallo de `CopyFile` con marker borrado y target restaurado → boot no promociona el `.tmp`.

### E16 — CERRADA
- **Qué cambié:** `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c` `ReconcileRefundedChain`. Si hay que aplicar compensación de stock (`restoredStock != stock`): `LFPG_ApplyClaimedStockTarget`, **no** se llama `PersistRemoveDeviceClaimPrefix`, y se marca `s_ReappliedThisBoot` + `s_ReconciledDevices`. Solo se compactan tombstones cuando el stock hive **ya** coincide con la compensación. `AdvanceOrPruneRefundedClaimAt` ya no llama `PersistRemoveClaimAt`: al llegar a 3 boots incrementa hasta 2 y se detiene. Eso cubre `SweepOrphanClaims` (ATM ausente) y `AdvancePresentRefundedClaims` (ATM presente con cola mixta). `LFPG_BTCAtm.c` no se tocó: `LFPG_OnStoreSaveExtra` ya serializa `m_BtcStock`.
- **Por qué así:** mover stock y claims a un journal común cruzaba hive DayZ + JSON de balances. Conservar tombstones hasta un boot futuro que vea el stock ya compensado es el patrón ya escrito para purchases en el mismo fichero. La poda por recuento de boots no es prueba hive; en un ATM ausente la compensación nunca se aplicó.
- **Qué NO cubre:** cadenas mixtas PENDING+REFUNDED siguen yendo a `ReconcilePendingChain`; la compactación de los REFUNDED espera a un boot con `pendingCount==0` y stock hive ya coincidente. Un ATM destruido para siempre deja tombstones en el JSON (fail-closed, no duplica). No hay prueba de que `SetSynchDirty` + `OnStoreSaveExtra` persistan el stock antes del siguiente boot.
- **Cómo comprobarlo:** ATM refunded reaparece con stock viejo; crash tras apply y **sin** clear JSON; segundo boot con stock hive ya compensado compacta; reembolso+stock no se duplican. ATM ausente ≥3 boots y reaparición: tombstone sigue ahí y `ReconcileRefundedChain` aplica compensación.

### E02 — CERRADA
- **Qué cambié:** `HandleBTCBuy` modo cash (~1282-1396). Catálogo válido → `ReserveRequest` → `PreparePlayerCash` / `MaxAffordableBtc` / `SelectPreparedCashCover` **antes** de crear BTC. Se conserva la política de entrega parcial. Recálculo de coste/cambio si `createdCash < affordableBtc`. `AddCashInput` ignora la misma `EntityAI` dos veces.
- **Por qué así:** exigir el total solicitado habría cambiado producto. El máximo asequible replica la entrega parcial previa sin spawns de prueba.
- **Qué NO cubre:** `ProbeAndCacheStack` / `WarmupCurrencyStackCache` (create+delete) corren **después** de probar fondos, solo en el camino que ya puede pagar. Un jugador sin dinero no genera entidades. Si el presupuesto de entidades se excede tras tener fondos, sí hay probe controlado (E03).
- **Cómo comprobarlo:** jugador sin billetes, request máxima → 0 entidades. Fondos parciales → entrega parcial. Falta de cambio / inventario lleno → `AbortOutputs`, sin consumo de billetes.

### E03 — CERRADA
- **Qué cambié:** tope `LFPG_BTC_MAX_ENTITIES_PER_TX = 64` en `LFPG_BTCHelper`. Estimación con capacidad de pila cacheada tras probe controlado (`GetQuantityMax` en entidad real; no se asume `CfgVehicles`). Preflight antes de `Reserve`/spawn en Sell cash/spill, cash Buy (BTC+cambio), Withdraw BTC y Withdraw cash. `StageItemsForPlayer` y `GreedyChange` rechazan si la estimación supera el tope. `RestoreDestroyedItems` (y el call-site de restitución de `HandleBTCDeposit` que antes usaba `CreateItemsForPlayer`) **no** aplican el tope: restituir no puede rechazarse por presupuesto.
- **Por qué así:** no se partió la transacción en varios frames (exigiría protocolo durable). 64 es un cap de trabajo por frame, no un valor económico. Probe real porque `ConfigGetFloat` no cubre magazines vs vehicles (comentario previo en el mismo helper).
- **Qué NO cubre:** el probe create+delete cuenta como mutación mínima aunque el preflight rechace después. Estimación pesimista (cache miss → pila 1) puede rechazar de más. No se midió tiempo de peor caso in-game. `CreateItemsForPlayer` ahora hereda el tope; el único otro call-site (depósito BTC) se reruteó a restitución sin tope.
- **Cómo comprobarlo:** moneda valor 1 no apilable, amount que exija 65 entidades → `AMOUNT_TOO_LARGE`, cero spawn (salvo probe). Límite 64 justo pasa; 65 falla. BTC no apilable en withdraw/sell spill igual.

### E08 — CERRADA
- **Qué cambié:** `scripts/3_Game/LFPG_BTCConfig.c`. `s_CurrencyCatalogValid` / `IsCurrencyCatalogValid()`. `Load()` siempre corre `ValidateAndClamp` + log + sort (también en defaults). Nulls se quitan (no son valor económico). Vacío, >16 entradas, classname vacío, duplicados, solape con `btcItemClassname`, clase inexistente (`CfgVehicles|CfgMagazines|CfgWeapons` + espacio final, patrón `P:\scripts\3_game\entities\object.c:430`), `value` fuera de `[1, 10000000]` → catálogo inválido **sin** sustituir denominaciones. Cash ops (`CountPlayerCash`, `PreparePlayerCash`, `GreedyChange`, handlers cash) fallan cerradas. Compra/venta a cuenta no dependen del catálogo.
- **Por qué así:** inventar `Paper_Bill_1` como fallback reescribía economía. Fail-closed deja el JSON intacto para que un admin lo corrija.
- **Qué NO cubre:** los defaults `Paper_Bill_*` **no existen en vanilla**. Un server sin mod de billetes y sin JSON propio verá el catálogo inválido y las ops de efectivo fallarán cerradas (account buy/sell siguen). Si `GetGame()` no existiera en `Load`, `ConfigClassExists` devolvería false; en este árbol `Load()` se llama desde `LFPG_NetworkManagerImpl` en init de misión (~534), con juego ya vivo.
- **Cómo comprobarlo:** duplicados, BTC=moneda, classname vacío/inexistente, `[null]` → log de error, cash ops INVALID, sin spawn ni crédito de efectivo.

### E05 — CERRADA
- **Qué cambié:** `HandleBTCDepositCash` (~2570-2640). Catálogo + room Native (`GetBalanceCap`) **antes** de Reserve. `SelectPreparedCashExact(requestedCredit)` **antes** del primer `AddBalance`. Si el crédito persistido no es el pedido, `RemoveBalance` y no se cometen billetes. El mismatch de `CommitPreparedCashValue` se mantiene como last-resort (compensación débito / `GreedyChange`).
- **Por qué así:** Native no expone preflight/commit atómico de “crédito+consumo”. Evitar el save es suficiente para el rechazo previsible (billete 100 / ingreso 60). No se añadió API nueva al provider externo.
- **Qué NO cubre:** proveedores no-Native no tienen `GetBalanceCap`; el room preflight no aplica y `AddBalance` puede devolver parcial (se revierte si ≠ requested). La rama last-resort de commit mismatch sigue pudiendo hacer un segundo save. `SelectPreparedCashExact` es greedy por orden de preparación, no un solver de subset-sum completo: combinaciones exactas no greedy pueden rechazarse.
- **Cómo comprobarlo:** un billete de 100, depósito 60 → 0 saves, rechazo, efectivo intacto. Room Native 30 con exacto representable → un save. Room 30 no representable → 0 saves.

---

## SEGUNDA PASADA (respuesta a REVIEW-T1.md)

### E16 — poda del tombstone ausente
- **Qué cambié:** `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c` `AdvanceOrPruneRefundedClaimAt`. Cuando `bootsSinceRefund + 1 >= 3` ya no llama `PersistRemoveClaimAt`; el tombstone permanece. `SweepOrphanClaims` y `AdvancePresentRefundedClaims` heredan ese comportamiento. Compactar sigue siendo solo `ReconcileRefundedChain` cuando el stock hive ya coincide. No se tocó `CountPendingPurchaseClaims`, `ObserveAbsentPendingClaimAt` ni el recuento de 2 boots PENDING.
- **Qué sigue abierto:** cadenas mixtas PENDING+REFUNDED no compactan los tombstones hasta un boot con `pendingCount==0`. Un ATM que no vuelve a existir deja tombstones en el JSON. La persistencia hive de `m_BtcStock` sigue sin prueba in-game.

### E04 — marcador de crédito-sin-destruir
- **Qué cambié:** `scripts/3_Game/LFPG_FileUtil.c` `WriteSellDestroyIntent` / `TryReadSellDestroyIntent` / `ClearSellDestroyIntent` (sibling de `LF_Balances.json`, no claim). `scripts/5_Mission/LFPG_BTCHelper.c` reconcilia un marcador previo, escribe el nuevo antes de `AddBalance` y lo borra tras destruir o al abortar. `scripts/5_Mission/LFPG_MissionInit.c` `InvokeOnConnect` reconcilia: saldo `before+credit` → destruir; saldo `before` → limpiar; otro valor → no tocar items.
- **Qué sigue abierto:** `DeleteFile` del marcador que falla tras una venta ya hecha puede re-consumir BTC en el próximo connect si el saldo no se ha movido. El reconcilio no espera hive de inventario. `RemoveBalance` de compensación que falla sigue pudiendo dejar crédito huérfano. El marcador usa `GetPlainId` porque el ledger Native ya clavea con eso.

### E08 menor — duplicados case-insensitive
- **Qué cambié:** `scripts/3_Game/LFPG_BTCConfig.c`. Copia + `ToLower` antes de `Find` y antes de comparar con `btcItemClassname`.
- **Qué sigue abierto:** nada de este menor. El JSON de producción (GRAVE E08 del revisor) sigue siendo gate del dueño.

### Helpers muertos
- **Qué cambié:** `CreateItemsForPlayer` eliminado. `DestroyPlayerCash` no se borró (el shell de fsync estaba bloqueado y encoger ese bloque en OneDrive sin `wb+fsync` deja padding NUL); se le añadió `IsCurrencyCatalogValid` para que un call-site futuro no opere con catálogo inválido.
- **Qué sigue abierto:** `DestroyPlayerCash` sigue sin call-sites y ocupa arena. Los logs de perf de `StageItemsForPlayerUnchecked` siguen diciendo `CreateItemsForPlayer`.

### Basura `.tmp.preserved`
- **Qué cambié:** `PreserveOrphanTmpEvidence` usa un sibling fijo `tmp + ".preserved"` (sobrescribe). `EnsureBalancesFileOrRestore` borra ese sibling al cargar y, si `FindFile` responde, hasta 32 restos `*.tmp.preserved.*` bajo `$profile:LF_PowerGrid`.
- **Qué sigue abierto:** `FindFile` con prefijo `$profile:` no está medido in-game; si el patrón no lista, los únicos nombres `*.preserved.<ts>_<rnd>` de la primera pasada pueden quedar hasta borrado admin. Wires/settings no entran en este barrido.

### Acuerdo / desacuerdo con REVIEW-T1.md
- De acuerdo en E16 (vía ausente) y E04 (ventana de dup nueva). Cerré ambas en código sin tocar E15.
- De acuerdo en que SEC09 residual (b) es más estrecho de lo que la primera pasada dijo; no se reabre.
- E08 JSON de producción: no es de esta pasada.

## LO_NO_VERIFICADO

- **Compilación:** el shell de esta sesión bloqueó hooks (`launch-ledger` / `gpu-lease-gate`); no se pudo correr `script_validator.py` ni AddonBuilder (este último además prohibido por el brief). El módulo Mission/Game no está compilado aquí. Revisión offline de convenciones (cero ternarios, sin `++`/`+=`/`foreach` en lo añadido, concatenaciones y args en una línea, `ref` solo en members nuevos). `auto currencies` en `GreedyChange`/`CountPlayerCash`/`DestroyPlayerCash` ya existía en el helper; no se extendió a código nuevo de Sell/Buy.
- **In-game:** ninguna de las pruebas de regresión de las fichas se ejecutó (brief: no lanzar DayZ). Gate in-game pendiente. No se inyectó crash entre `AddBalance` y `DestroyPlayerItems`, ni ausencia de ATM ≥3 boots.
- **APIs leídas en árbol/vanilla (no se inventaron):**
  - `array<T>.Find(T)` — `P:\scripts\1_core\proto\enscript.c:394`. Uso homólogo: `deviceIds.Find(...)` en `LFPG_BalanceProvider_NativeImpl.c:1483`.
  - `OpenFile` / `FPrintln` / `FGets` / `FileHandle == 0` — `P:\scripts\1_core\proto\ensystem.c:400-501`; mismo patrón en `LFPG_FileUtil.c` `WriteBalancesSaveIntent`.
  - `FindFile` / `FindNextFile` / `CloseFindFile` — `ensystem.c:520-522`; patrón `worldsmenu.c:72-84`.
  - `string.ToLower` / `IndexOf` / `Get` — ya usados en `LFPG_BTCConfig.c:264` y `LFPG_FileUtil.c` `TryReadRawJsonVersion`.
  - `GetGame().ConfigIsExisting` — `object.c:430` (`"CfgVehicles " + type + " "`).
  - `EntityAI.GetQuantityMax` / override `ItemBase` — `entityai.c:2256`, `itembase.c:3459`. El helper ya hacía clamp `< 1` → 1.
  - `AddBalance` / `RemoveBalance` / `GetBalanceCap` / `IsClaimStoreWritable` — `LFPG_BalanceProvider_NativeImpl.c:1716,1775,72,77`. `AddBalance` **puede** devolver parcial por room (`toAdd`); Sell ahora revierte ese parcial.
  - `SelectPreparedCashExact` — método nuevo en `LFPG_BTCInventoryPlan` (~91-118), greedy, no subset-sum.
  - `ObjectDelete` / `CreateInInventory` / `CopyFile` / `DeleteFile` — ya usados en los mismos ficheros.
  - `MissionServer.InvokeOnConnect(PlayerBase, PlayerIdentity)` — `P:\scripts\5_mission\mission\missionserver.c:422`. Nombres de parámetros iguales a vanilla.
  - El ledger Native clavea con `PlayerIdentity.GetPlainId()` (`LFPG_BalanceProvider_NativeImpl.c` `GetUID`); el marcador de venta usa la misma clave. Los logs siguen con `LFPG_Util.LogUid`.
- **Contrato CopyFile:** el header de FileUtil exige destino `$profile:` / `$saves:`. El path de balances ya cumple; el marcador `.saving` y el `.sell.<uid>` se escriben con `OpenFile` en el mismo directorio, no con `CopyFile`.
- **E04:** no hay inyección de fallo real. Queda el residuo de marcador huérfano tras `DeleteFile` fallido y el orden hive de inventario vs JSON de balances.
- **SEC09 (b) marker+tmp+bak sin target** no tiene inyección de fallo en este entorno.
- **E16:** no hay prueba de que `SetSynchDirty` + `OnStoreSaveExtra` persistan el stock antes del siguiente boot; por eso se conservan tombstones. Orden real hive vs JSON: no verificado. `FindFile("$profile:...")` para restos únicos `.preserved.*` no medido.
- **E08 defaults `Paper_Bill_*`:** existencia en el PBO del server no verificada. Si no existen, cash queda fail-closed a propósito. El JSON vivo de producción no se validó aquí (gate del dueño).
- **Tope 64:** elegido como presupuesto de spawn por transacción; no hay medición de frame time. No está en JSON (evitar ampliar superficie de config en este tramo).
- **No se relajó G6:** cantidades siguen siendo autoridad de servidor; errores siguen siendo error; no se amplió lo que el cliente puede pedir. Un marcador ilegible o un saldo ambiguo no destruye items.

## TERCERA PASADA
### E04 — marcador re-armado en vez de borrado
- **Qué cambié:** `scripts/5_Mission/LFPG_BTCHelper.c`. Tras destruir, `ClearSellDestroyIntentAfterDestroy` (`:1397-1411`) intenta borrar el sibling `.sell`; si `DeleteFile` falla, reescribe el marcador con `WriteSellDestroyIntent` poniendo `balanceBefore` al saldo post-crédito (la escritura no exige que el borrado previo haya funcionado: `LFPG_FileUtil.c:547-551` borra si puede y abre `FileMode.WRITE` igual). Si la reescritura también falla, log de admin. Call-sites post-destrucción: reconcilio `:1486`, venta normal exacta `:1967-1968`, venta normal con destrucción incompleta (después del intento de revertir crédito) `:1908-1912`. El `Clear` de aborto **antes** de destruir (`:1883`) no se tocó: ahí los BTC siguen intactos. En `ReconcilePendingAccountSell`, `current == balanceBefore` (`:1451-1458`) corre **antes** del gate de cap (`:1461-1466`); sin eso un rebase cerca del tope Native (`creditAmount > cap - current`) nunca alcanza la rama que limpia sin destruir.
- **Qué sigue abierto:** si `Clear` y `Write` fallan los dos, el marcador sigue armado con `before+credit` y el próximo connect puede volver a destruir — solo queda el log de admin. `WriteSellDestroyIntent` no relee el fichero tras escribir; un write truncado deja el marcador ilegible (`TryRead` false → no destruye). El reconcilio sigue sin esperar hive de inventario. `RemoveBalance` de compensación que falla, con rebase OK, puede dejar crédito y BTC restituidos (fail-closed: no se re-destruye).

