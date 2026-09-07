# DICTAMEN T1+T6 — ronda 1

## VEREDICTO
**VERDE.** Cero GRAVE vivo en el camino del dinero: el crédito a cuenta no se confirma sin diario, el efectivo no se mueve sin catálogo válido y capacidad previa, y el destornillador ya no borra un ATM con saldo.

## HALLAZGOS
### [MEDIO] El ATM con saldo sigue perdiendo el stock si lo matan por daño, no por destornillador
- **Dónde:** `scripts/4_World/LFPG_BTCAtm.c:82-85`, `scripts/4_World/LFPG_BTCAtm.c:456-465`, `scripts/4_World/lfpg_devicebase.c:273-283`, `scripts/4_World/LFPG_ActionDismantleDevice.c:116-117`
- **Qué pasa:** T6 cierra solo `LFPG_ActionDismantleDevice`. `LFPG_BlocksDismantle()` es `m_BtcStock > 0`, y esa comprobación vive únicamente en la acción del destornillador. `EEKilled` llama `LFPG_OnKilled()`; en el ATM de jugador ese override solo apaga el LED de potencia. No hay reembolso, no hay drop, no hay bloqueo. Ambos ATM (jugador y admin) tienen `hitpoints = 200` en `config.cpp`.
- **Escenario de fallo:** (1) ATM con `m_BtcStock = 40`. (2) Un jugador lo dispara hasta 0 HP. (3) `EEKilled` → `ObjectDelete` posterior. Resultado: 40 BTC de máquina desaparecen. No hace falta crash ni ventana de milisegundos; 200 HP es alcance de raid normal. El kit **no** borra un ATM ya colocado: al colocar, borra el kit y spawnea un aparato nuevo.
- **Corrección propuesta:** cabe en una pasada de producto, no de “un if más”: o el ATM no es destructible / sube mucho la vida, o `LFPG_OnKilled`/`EEDelete` liquidan el stock a items en el suelo con el mismo tope de 64. Si el dueño quiere que un raid queme el dinero, fírmalo como riesgo aceptado; no lo trates como el bug del destornillador.
- **Confianza:** CONFIRMADO

### [MEDIO] `LFPG_BlocksDismantle()` por defecto deja pasar batería y horno con valor de script
- **Dónde:** `scripts/4_World/lfpg_devicebase.c:616-619`
- **Qué pasa:** el default devuelve `false`. El comentario de encima habla de fail-closed; el cuerpo es fail-open. Cargo y attachments ya los cubre la acción. Lo que no cubre es estado solo en campos: `LFPG_Battery.m_StoredEnergyX10`, `LFPG_Furnace.m_FuelCurrent`. Esos aparatos usan el kit por convención (`GetType()+"_Kit"`); el adaptador de batería sí anula el kit (`LFPG_GetKitClassname` vacío) y no se desmonta.
- **Escenario de fallo:** (1) Batería cargada o horno con combustible. (2) Destornillador 5 s. (3) Sale el kit vacío; la energía/combustible no viaja. No es BTC ni saldo Native.
- **Corrección propuesta:** `override` en batería (`stored > 0`) y horno (`m_FuelCurrent > 0`), el mismo patrón que el ATM. No es el camino del dinero; no bloquea el verde.
- **Confianza:** CONFIRMADO

### [MEDIO] Diario de venta: un tercer saldo, o un marcador ilegible, deja crédito y objetos
- **Dónde:** `scripts/5_Mission/LFPG_BTCHelper.c:1460-1461`, `scripts/5_Mission/LFPG_BTCHelper.c:1499-1504`, `scripts/5_Mission/LFPG_BTCHelper.c:1521-1533`
- **Qué pasa:** la máquina es `before` → limpiar sin tocar items; `before+credit` → destruir una vez; cualquier otro valor → no destruir y dejar el fichero; `TryRead` falso → return inmediato. Eso evita re-destruir el inventario equivocado. También deja de cobrar la obligación: si el crédito **sí** se grabó y el saldo luego se movió, o el marcador no parsea, los BTC pueden quedar.
- **Escenario de fallo:** hace falta alinear varias cosas, por eso no es GRAVE. (1) `WriteSellDestroyIntent` y `AddBalance` OK. (2) Crash o corte **antes** de `DestroyPlayerItems`. (3) Antes del `InvokeOnConnect` del jugador, otra vía Native cambia el saldo (reembolso huérfano a los 120 s, depósito, admin) **o** `FGets` deja el marcador ilegible. (4) Reconexión: rama ambigua / read falso → items intactos + crédito intacto. El mapa `s_SellDestroyedThisBoot` no aplica: la destrucción de esa venta nunca llegó a correr.
- **Corrección propuesta:** no cabe bien en una pasada. Un bit durable “crédito cobrado / items aún no destruidos” es el mismo write que ya puede fallar. Lo accionable: no borrar el marcador en reconcilio si `destroyed != btcAmount`; y `Trim()` de cada línea leída. Lo segundo sí cabe.
- **Confianza:** CONFIRMADO el código de las tres ramas; PLAUSIBLE que el tercer saldo ocurra en un boot real (hace falta otra mutación Native en esa ventana)

### [MEDIO] Diario de venta: clear+rebase fallidos + reinicio pueden volver a destruir
- **Dónde:** `scripts/5_Mission/LFPG_BTCHelper.c:1428-1441`, `scripts/5_Mission/LFPG_BTCHelper.c:2014-2020`, `scripts/5_Mission/LFPG_BTCHelper.c:1512-1516`
- **Qué pasa:** tras una venta ya destruida, si `DeleteFile` y el rewrite de rebase fallan, el marcador sigue armado con el `before` original. `s_SellDestroyedThisBoot` cubre reconexiones **en el mismo proceso**. No se persiste (el comentario lo dice). Tras restart el flag se pierde.
- **Escenario de fallo:** (1) Venta a cuenta completa: crédito durable, items destruidos. (2) `ClearSellDestroyIntent` falso y `WriteSellDestroyIntent` de rebase falso — dos I/O al mismo sibling, no una ventana de milisegundos. (3) El Error de admin se ignora. (4) Reinicio. (5) Saldo Native sigue siendo `before+credit` (el jugador no tocó la cuenta; loot / compra en efectivo / munición de la misma clase sí puede rellenar inventario). (6) `InvokeOnConnect` destruye otra vez `btcAmount` de esa clase. El default `btcItemClassname` es `Ammo_9x19_25Rnd`: puede ser munición, no solo “fichas”.
- **Corrección propuesta:** no hay write durable que sobreviva al disco que acaba de rechazar clear y rebase. Operación: el Error ya nombra el fichero; no reiniciar sin borrarlo. Código: no desarmar el marcador en reconcilio si `destroyed == 0` **y** el flag de boot no existe, a costa de dejar ventas ya cobradas atascadas — peor UX, no cierra el agujero de disco lleno.
- **Confianza:** CONFIRMADO el camino; la explotabilidad exige fallo persistente de FS ya logueado

### [MEDIO] E16: la poda a 3 boots ya no existe; los tombstones de un ATM muerto crecen sin compactar
- **Dónde:** `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1388-1395`, `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1230-1249`
- **Qué pasa:** `AdvanceOrPruneRefundedClaimAt` al llegar a 3 boots **no** llama `PersistRemoveClaimAt`; solo incrementa hasta 2 y se detiene. `PersistRemoveClaimAt` queda para claims PENDING físicos (`debit == 0`) nulos, no para REFUNDED. Compactar REFUNDED solo ocurre en `ReconcileRefundedChain` cuando el stock hive **ya** coincide. Un ATM borrado para siempre nunca entra ahí.
- **Escenario de fallo:** no duplica dinero. Cada ATM destruido con cadena REFUNDED deja objetos en `LF_Balances.json` hasta intervención. No hay tope por aparato ausente.
- **Corrección propuesta:** no tocarlo en esta ronda (es el fail-closed que cierra el dup). Si duele el tamaño del JSON, compactar solo con prueba hive o comando admin, no por recuento de boots.
- **Confianza:** CONFIRMADO

### [MEDIO] SEC09 sigue abierta solo si fallan juntas promoción, borrado del `.saving`, restore y descarte del `.tmp`
- **Dónde:** `scripts/3_Game/LFPG_FileUtil.c:264-292`, `scripts/3_Game/LFPG_FileUtil.c:808-837`
- **Qué pasa:** un `.tmp` parseable **no** se promociona si hay target vivo, o si falta marcador `.saving`, o si falta backup. El marcador se escribe **después** de verificar y stagar. Un `CopyFile` de promote fallido intenta borrar el marcador **antes** de restaurar. Native, si recibe `false`, revierte RAM. La vía que aún promociona una mutación abortada es: promote falla, `ClearBalancesSaveIntent` falla, restore desde bak falla, `DiscardAbortedBalancesTmp` falla, y en el boot hay `.tmp` + `.saving` + bak sin target.
- **Escenario de fallo:** cuatro I/O fallidas encadenadas más un reinicio. No es operación normal. El log ya pide borrar el `.saving` antes de rearrancar.
- **Corrección propuesta:** no reabrir el contrato de `AddBalance`. El residual está acotado y logueado. Un `committed/aborted/uncertain` no cabe en una ronda.
- **Confianza:** CONFIRMADO

### [MENOR] `SelectPreparedCashExact` es greedy, no subset-sum
- **Dónde:** `scripts/5_Mission/LFPG_BTCHelper.c:91-118`, call-site `scripts/5_Mission/LFPG_BTCHelper.c:2842-2847`
- **Qué pasa:** E05 sí evita el `AddBalance` si no hay cobertura exacta greedy. Combinaciones exactas no greedy (p. ej. dos de 3 frente a greedy 5+…) se rechazan sin mutar. No hay pérdida; hay depósitos que el jugador podría hacer a mano y el ATM niega.
- **Escenario de fallo:** catálogo con denominaciones no canónicas + combinación exacta que el greedy no arma → `INVALID`, billetes intactos, 0 saves.
- **Corrección propuesta:** no en esta ronda. Un solver de subset-sum en Enforce no es un parche mínimo.
- **Confianza:** CONFIRMADO

### [MENOR] `ref` en locales (convención del repo)
- **Dónde:** `scripts/3_Game/LFPG_BTCConfig.c:485-486`, `scripts/3_Game/LFPG_BTCConfig.c:501`, `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1987`, `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:2108`
- **Qué pasa:** el brief marca `ref` solo en miembros. Compila; no cambia el dinero.
- **Escenario de fallo:** ninguno monetario.
- **Corrección propuesta:** quitar `ref` de esos locales si se toca el fichero por otra razón.
- **Confianza:** CONFIRMADO

### [MENOR] `DestroyPlayerCash` no tiene call-sites
- **Dónde:** `scripts/5_Mission/LFPG_BTCHelper.c:2412`
- **Qué pasa:** grep de `DestroyPlayerCash(` en el helper: solo la definición. El catálogo inválido ahora la deja en 0 si alguien la llama. Sigue ocupando arena.
- **Escenario de fallo:** ninguno hoy.
- **Corrección propuesta:** borrar en una pasada de higiene, no en la de GRAVE.
- **Confianza:** CONFIRMADO

## LO QUE HE VERIFICADO
Ficheros leídos enteros o por tramo de dinero: `LFPG_BTCConfig.c`, `LFPG_FileUtil.c`, `LFPG_BalanceProvider_NativeImpl.c`, `LFPG_MissionInit.c`, `LFPG_ActionDismantleDevice.c`, `LFPG_BTCAtm.c`, `lfpg_devicebase.c`; de `LFPG_BTCHelper.c` el plan de inventario, probes, GreedyChange, Buy cash/cuenta, Sell (marcador + destroy), Withdraw/Deposit BTC, Withdraw/Deposit cash, `ReconcilePendingAccountSell`.

Caminos recorridos de punta a punta:

- **E04 venta a cuenta:** spill `GreedyChange` → `WriteSellDestroyIntent` (`FileUtil.c:541`) **antes** de `AddBalance` → destroy → `LFPG_MarkSellDestroyedThisBoot` → clear/rebase. Aborto de crédito: `RemoveBalance` + clear **sin** destroy. `MissionInit.c:44-47` `InvokeOnConnect` → reconcilio. Tres ramas de saldo + ilegible + flag de boot.
- **E16:** `AdvanceOrPruneRefundedClaimAt` no poda; compactación solo en `ReconcileRefundedChain` con stock ya coincidente. `PersistRemoveClaimAt` solo en PENDING físico nulo / slot null.
- **SEC09:** barrera `.saving` + no promocionar junto a target vivo. Residual de cuatro fallos leído línea a línea.
- **E02:** cash buy `PreparePlayerCash` / `SelectPreparedCashCover` **antes** de `StageItemsForPlayer`; `AbortOutputs` si cambio, commit o spawn fallan; `CommitPreparedCashValue` es validate-then-mutate.
- **E03:** `LFPG_BTC_MAX_ENTITIES_PER_TX = 64`; estimación antes de spawn en buy cash, sell cash/spill, withdraw BTC/cash; `RestoreDestroyedItems` sin tope.
- **E05:** `SelectPreparedCashExact` antes de `AddBalance`; mismatch de crédito se revierte sin `Commit`.
- **E08:** catálogo vacío a propósito; `s_CurrencyCatalogValid`; cash ops gated; account buy/sell no dependen del catálogo.
- **T6:** `LFPG_ValidateDismantle` en condition y en `OnFinishProgressServer`; override ATM; destornillador en `LFPG_ActionRegistration.c:157`. Grep: ningún otro `LFPG_BlocksDismantle`. Kit al colocar hace `ObjectDelete(this)` del kit, no del ATM.
- Vanilla abierto, no de memoria: `CreateInInventory` en `P:\scripts\3_game\systems\inventory\inventory.c:876-894` crea entidad **nueva** en slot libre (el probe+`ObjectDelete` no fusiona pilas ajenas). `InvokeOnConnect` en `missionserver.c:316-347` (ClientNew / ClientReady); **ClientReconnectEvent no lo llama** (`367-380`). `FGets` en `ensystem.c:483-501` (no documenta `\r`). `config.cpp` ATM jugador y admin: `hitpoints = 200`.
- Convenciones: grep de `++`/`+=`/`foreach`/ternarios en los 8 ficheros: no hay en código (solo comentarios).

Afirmaciones del implementador que **sostengo**: E02, E03, E08, E16 (la vía ausente ≥3 boots ya no poda), T6 destornillador, E04 mismo boot, SEC09 fail-closed en aborto reportado con I/O sana.

## LO QUE NO HE PODIDO VERIFICAR
- In-game: nada. No hay servidor en esta lane.
- Si `FGets` tras `FPrintln` deja `\r` en Windows. Si lo deja, `lineUid != uid` y `ParseNonNegativeIntText` fallan siempre → el reconcilio **nunca** destruye (fail-closed hacia dup si hubo crédito). No lo ejecuté; no lo marco GRAVE.
- Si en `ClientReadyEvent` el inventario hive ya está colgado del `PlayerBase` cuando corre `InvokeOnConnect`. Lo razono porque el engine pasa el player ya construido (`missionserver.c:338` + `SelectPlayer`); no tengo el loader C++.
- Orden real hive `OnStoreSaveExtra` (`m_BtcStock`) vs `LF_Balances.json` entre dos boots. E16 asume que puede no coincidir; por eso conserva tombstones.
- `FindFile("$profile:...")` para restos `.tmp.preserved.*`.
- El `LF_BTCAtm.json` de producción (gate del dueño; catálogo vacío firmado).
- Que 200 HP se pueda vaciar con las armas del server (el `DamageSystem` está; no disparé).
- `CopyFile`/`DeleteFile` fallando de verdad en `$profile:`.

**Supuestos etiquetados:** Enforce es single-thread por frame en estos handlers. `GetPlainId()` es la misma clave que Native. `CreateInInventory` del probe no reusa una pila existente (visto en vanilla). Un escenario que exige disco roto **y** ignorar el Error **y** reiniciar no lo escalo a GRAVE.