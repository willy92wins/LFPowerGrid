# REVIEW-T1 — revisión adversarial del parche de integridad monetaria

Revisor independiente. Solo lectura: sin git, sin compilación, sin in-game. Todo lo
afirmado abajo está contrastado contra el árbol de trabajo (rama
`fix/t1-integridad-monetaria`, cambios sin commitear) y contra `P:\scripts` vanilla.
Lo que el informe del implementador afirma y he podido confirmar se indica; lo que no
cuadra, también.

Resumen de método (para que conste que se buscó donde manda el brief):

- **(a) Compilación estática:** los 4 ficheros leídos completos (BTCHelper 2693 líneas,
  FileUtil 683, BTCConfig 558, NativeImpl 2150). Firmas verificadas con grep una a una
  (lista en «Lo verificado»). Cero ternarios / `++` / `--` / `+=` / `-=` / `foreach` en
  lo añadido (grep sobre los 4 ficheros: solo comentarios). `#ifdef SERVER` balanceados
  (BTCHelper 2273/2450, 2455/2691). Sin literales con dos escapes en lo añadido.
- **(b) Fichas:** camino de ejecución nuevo trazado para E04, SEC09, E16, E02, E03,
  E08, E05 con entradas concretas.
- **(c) Vías nuevas de pérdida/dup:** todos los `return` de error nuevos inspeccionados;
  reversas (`RemoveBalance`, `RestoreDestroyedItems`, `AbortOutputs`) seguidas hasta su
  fallo.
- **(d) Frontera de confianza:** sin cambios; ver sección específica.
- **(e) Regresiones en servidor vivo:** ver E08 y E03 abajo.

---

## HALLAZGOS

### [GRAVE] E16 está etiquetada CERRADA pero la vía del ATM ausente ≥3 boots sigue abierta (dup reembolso+stock)
- **Dónde:** `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1386-1397` y `:1532-1537`; contraste con `:1230-1250`.
- **Qué veo:** el fix nuevo conserva los tombstones REFUNDED cuando el ATM **presente**
  recibe la compensación (`s_ReappliedThisBoot` + `s_ReconciledDevices`, `:1240-1241`) y
  solo compacta cuando el stock hive ya coincide (`:1246-1250`). Pero
  `AdvanceOrPruneRefundedClaimAt` (`:1380`) sigue podando el tombstone cuando
  `bootsSinceRefund + 1 >= 3`, y `SweepOrphanClaims` (`:1532-1537`) ejecuta esa poda
  precisamente para ATMs **ausentes** (no en `LFPG_DeviceRegistry`).
- **Por qué falla:** es el escenario literal de la ficha («ATM ausente que fue refunded
  y reaparece»). Camino concreto: (1) compra a cuenta aplica stock 10→15 y persiste en
  hive; (2) el ATM queda ausente (empaquetado / fuera de burbuja); (3) dos sweeps lo
  observan ausente → `RefundPendingClaimAt` devuelve el débito al jugador y tombstonea
  (boot N); (4) boots N+1, N+2 incrementan `bootsSinceRefund`; boot N+3 poda el
  tombstone (`PersistRemoveClaimAt`); (5) el ATM reaparece en boot N+4 con hive
  `m_BtcStock=15`; `ReconcileLoadedAtm` encuentra cadena vacía (`:1271-1274`) → reconciliado
  sin compensación. Resultado: jugador con el reembolso **y** ATM con 5 BTC retirables.
  Duplicación. El propio informe lo admite en «Qué NO cubre», lo cual contradice el
  encabezado «E16 — CERRADA»: la ficha pedía idempotencia + compactación solo tras
  prueba durable, y eso solo se cumple para el ATM presente.
- **Confianza:** alta en el camino lógico (leído línea a línea); media en la
  explotabilidad real — exige ausencia ≥3 boots y reaparición con stock viejo. Me falta
  saber si un ATM empaquetado conserva `m_BtcStock` en hive (no verificado in-game).

### [GRAVE] E04: el reorder convierte pérdida en duplicación por crash — exactamente lo que la ficha avisó
- **Dónde:** `scripts/5_Mission/LFPG_BTCHelper.c:1719-1747`.
- **Qué veo:** spill de efectivo stagged (`:1709`), `AddBalance` persistido (`:1725`),
  y **solo después** `DestroyPlayerItems` (`:1747`).
- **Por qué falla:** crash del proceso después de que `AtomicSaveBalances` complete
  (crédito durable) y antes de la destrucción: al reiniciar el jugador tiene el crédito
  (+ el spill stagged si el hive lo persistió) **y** los BTC intactos. La ficha E04 decía
  textualmente: «Cambiar simplemente el orden crédito/destrucción desplaza el riesgo a
  duplicación tras crash». El implementador hizo exactamente ese reorder y lo admite en
  «Qué NO cubre». La ficha permitía la «mitigación síncrona con rollback explícito», así
  que el cierre del escenario reportado (save devuelve false → BTC intactos, cero
  crédito) es legítimo y está bien ejecutado; pero la ventana de duplicación queda
  **abierta y es nueva** respecto al comportamiento anterior (que perdía, no duplicaba).
  En una economía viva, dup > pérdida como riesgo sistémico. Es decisión de producto
  firmarla, no detalle menor. Nota a favor: la ventana es estrecha (mismo frame, crash
  entre dos líneas) y el camino de fallo reportado queda cerrado por dos reversas
  verificadas (`:1728-1743` crédito parcial revertido + spill abortado con BTC intactos;
  `:1748-1783` destrucción parcial → reversa de crédito + restitución).
- **Confianza:** alta (el implementador la admite; el código la confirma). Falta
  inyección de fallo real para cuantificarla.

### [GRAVE] E08: riesgo de despliegue — el catálogo JSON del servidor vivo no está verificado contra las reglas nuevas
- **Dónde:** `scripts/3_Game/LFPG_BTCConfig.c:280-358` (validación), `:343-347`
  (`ConfigClassExists`), `:484-488` (flag); consumidores: `LFPG_BTCHelper.c:766`,
  `:883-885`, `:920-921`, `:1283`, `:1670`, `:2388`, `:2576`.
- **Qué veo:** una sola entrada mala (duplicado, classname vacío, solape con
  `btcItemClassname`, clase inexistente en CfgVehicles/CfgMagazines/CfgWeapons, `value`
  fuera de [1, 10000000], >16 entradas, vacío tras quitar nulls) invalida el catálogo
  **entero** y todas las operaciones de efectivo fallan cerradas: Buy cash, Sell cash y
  spill, WithdrawCash, DepositCash, y `CountPlayerCash` devuelve 0.
- **Por qué falla:** no es un defecto del código — el fail-closed es la decisión
  correcta y está bien implementado — sino del **paquete de despliegue**: el informe no
  verifica el `LF_BTCAtm.json` de producción. Si el JSON vivo tiene cualquier entrada
  que hoy «funciona» (p. ej. un classname de un mod de billetes retirado, un duplicado
  que hoy infla conteos silenciosamente, 17 denominaciones), este parche mata toda la
  economía de efectivo en el boot siguiente hasta que un admin corrija el JSON **y se
  reinicie** (`Load()` corre una sola vez desde `LFPG_NetworkManagerImpl.c:534`; no hay
  hot-reload). Añadido: los defaults `Paper_Bill_*` no existen en vanilla, así que un
  servidor fresco sin mod de billetes arranca directamente en fail-closed (admitido por
  el implementador). Gate previo al deploy: validar el JSON vivo contra las 7 reglas.
- **Confianza:** alta en el mecanismo; baja en si el JSON vivo lo supera — no lo tengo.

### [MENOR] SEC09: la vía residual (b) necesita más fallos de los que el informe cuenta; wires/settings quedan sin protocolo
- **Dónde:** `scripts/3_Game/LFPG_FileUtil.c:271-292` (promote-fail), `:633-639`
  (guarda de target vivo), `:677-679` (limpieza de marcador huérfano); wires/settings
  `:47-210` y `:493-542`.
- **Qué veo:** el escenario exacto de la ficha (abort reportado + reinicio → promoción
  del `.tmp`) queda cerrado por **dos** vías independientes: si el target se restaura,
  la guarda de target vivo (`:633-639`) se niega a promover y preserva el `.tmp` como
  evidencia; si no se restaura, `DiscardAbortedBalancesTmp` (`:288-291`) saca el `.tmp`
  de la ruta de promoción. La vía residual (b) que admite el informe (CopyFile de
  promoción falla + DeleteFile del `.saving` falla + restauración desde bak falla) en
  realidad requiere **además** que `DiscardAbortedBalancesTmp` falle dos veces
  (`PreserveOrphanTmpEvidence` + `DeleteFile`, `:480-489`): ~5 fallos de E/S
  simultáneos. El informe subestima su propio margen. Verificado también: crash
  post-`CopyFile` pre-cleanup → target vivo gana, `.tmp` preservado, marcador limpiado;
  crash post-marcador pre-`DeleteFile(target)` → target vivo, no promoción, mutación
  nunca reconocida se pierde (consistente: nadie la vio); first-save abort → fail-closed
  a fresco, sin resurrección. El marcador se escribe solo tras verificar el tmp y stagarear
  bak.new (`:263-268`), tal como dice el informe.
- **Por qué falla (lo que queda):** `AtomicSaveVanillaWires` / `AtomicSaveSettings` y
  sus `Ensure*` siguen promoviendo cualquier `.tmp` parseable sin marcador
  (`:503-505`, `:530-532` vía `PromoteOrphanTmp`). El contrato contradictorio de la
  ficha persiste para esos dos tipos. Admitido como «fuera de alcance»; lo dejo
  registrado porque la ficha los citaba (`FileUtil 91–105 y 174–186`). Además, los
  `.tmp.preserved.<ts>_<rnd>` no los limpia nadie (`:417-439`): basura acumulativa en
  `$profile:` tras cada abort.
- **Confianza:** alta. Falta inyección de fallo real en DayZ (imposible aquí).

### [MENOR] E08: detección de duplicados sensible a mayúsculas
- **Dónde:** `scripts/3_Game/LFPG_BTCConfig.c:325-334`.
- **Qué veo:** `seenClassnames.Find(cur.classname)` compara cadenas exactas.
- **Por qué falla:** el engine resuelve clases de config sin distinguir
  mayúsculas/minúsculas; `Paper_Bill_100` y `PAPER_BILL_100` son la misma clase para
  `ConfigIsExisting` pero pasan el filtro como «distintas» → el doble conteo de E08
  sobrevive en ese caso concreto. Requiere que un admin escriba el mismo classname con
  distinta capitalización; remoto pero trivial de cerrar (normalizar con `ToLower`
  antes del `Find`, patrón ya usado en `:264`).
- **Confianza:** media — asumo la insensibilidad a mayúsculas del config lookup del
  engine (no verificable sin lanzar DayZ).

### [MENOR] E03: el cap de 64 rechaza operaciones legítimas grandes con configs extremas; probes create+delete en rechazos
- **Dónde:** `scripts/5_Mission/LFPG_BTCHelper.c:240`, `:582-601`, `:777-788`,
  preflights `:1327-1337`, `:1663-1686`, `:1930-1939`, `:2396-2404`.
- **Qué veo:** cobertura completa de las vías de spawn (Sell cash/spill, Buy
  BTC+cambio, Withdraw BTC, Withdraw cash) con estimación previa a `Reserve`/spawn, y
  `RestoreDestroyedItems`/`StageItemsForPlayerUnchecked` sin tope para restitución
  (`:604-609`) — correcto: restituir no puede rechazarse. `CreateItemsForPlayer`
  hereda el tope y ya no tiene call-sites vivos (grep: solo la definición `:458-461`).
- **Por qué falla (bordes):** (1) con el default `Ammo_9x19_25Rnd` (pila 25), un
  withdraw de >1600 BTC se rechaza aunque `maxBtcPerMachine` permita 10000 — cambio de
  comportamiento para operaciones legítimas grandes; con el default de stock (100) no
  se nota, pero configs altas sí. Es el remedio pedido por la ficha, pero conviene
  documentarlo como cambio de producto. (2) `ProbeAndCacheStack`/`WarmupCurrencyStackCache`
  crean+destruyen una entidad por classname no cacheado **antes** del rechazo por
  presupuesto (`:1325-1326`, `:1677`): un jugador con fondos que exceda el cap genera
  probes; admitido por el implementador y acotado a N classnames una vez por boot
  (cache). (3) Estimación pesimista en cache miss (pila=1) puede rechazar de más la
  primera transacción de una classname; se autocorrige tras el probe.
- **Confianza:** alta en (1) y (2); media en (3) — depende de `GetQuantityMax` real
  por clase (verificado `entityai.c:2256`, devuelve `int`; el cast es redundante pero
  inocuo).

### [MENOR] E05: greedy-exact no es subset-sum; no se intentan importes menores representables
- **Dónde:** `scripts/5_Mission/LFPG_BTCHelper.c:91-118` (`SelectPreparedCashExact`),
  `:2585-2631` (clamp a room + exact).
- **Qué veo:** el escenario de la ficha (billete de 100, ingreso de 60) queda cerrado
  con **cero** saves: `SelectPreparedCashExact(60)` falla antes del primer
  `AddBalance` (`:2625-2631`). El clamp a room Native (`:2587-2603`) evita el
  AddBalance-parcial previsible.
- **Por qué falla (bordes):** (1) greedy por orden de preparación (descendente por
  valor) rechaza combinaciones exactas no greedy: billetes [100, 60, 60], ingreso 120
  → coge 100, resto 20 no representable → rechazo pese a existir 60+60. Admitido.
  (2) Con room < pedido (saldo a 30 del cap, ingreso 100, billetes [50,50]):
  `requestedCredit=30`, exact(30) falla → rechazo total cuando un depósito de 50 era
  viable. El comportamiento anterior también rechazaba (tras Add+Remove), así que no es
  regresión; es mejora de UX no tomada. (3) La rama last-resort de commit-mismatch
  (`:2649-2679`) mantiene el segundo save y la compensación débil (admitido);
  `CommitPreparedCashValue` valida todo antes de consumir (`:170-190`), así que esa
  rama es prácticamente inalcanzable en operación síncrona.
- **Confianza:** alta.

### [MENOR] Código muerto y fricción de UI
- **Dónde:** `LFPG_BTCHelper.c:2197-2268` (`DestroyPlayerCash`, sin call-sites tras el
  rewrite de Buy cash — grep en todo `scripts/`), `:458-461` (`CreateItemsForPlayer`,
  idem), `:883-885` (`CountPlayerCash` → 0 con catálogo inválido).
- **Qué veo:** dos helpers monetarios muertos y un «0» cosmético.
- **Por qué falla:** el muerto no rompe nada pero invita a reuso futuro sin las guardas
  nuevas (`DestroyPlayerCash` no consulta `IsCurrencyCatalogValid`). El «0» de
  `CountPlayerCash` con catálogo inválido pinta al jugador sin efectivo en UI
  (`HandleBTCOpenRequest:1001`, payloads de resultado) mientras lleva billetes encima —
  confusión de soporte, no pérdida.
- **Confianza:** alta.

---

## LO VERIFICADO (muestras de contraste, no exhaustivo)

- **Firmas contra el árbol:** `ReserveRequest` 9 args (`LFPG_BTCSessionRegistry.c:311`),
  `CompleteRequest` 13 (`:345`), `OpenSession` 4 (`:141`), `AllowReplayResponse`
  (`:380`); `GetName` (`LFPG_BalanceProvider.c:30`), `GetActive` nullable pero
  `IsAvailable()` ⇔ `s_Active` no nulo (`:178-190`) → `atmEarlyS.GetName()` en
  `BTCHelper:1581` es seguro tras el gate de `:1460`; `GetBalanceCap`/`IsClaimStoreWritable`
  (`NativeImpl:72,77`); métodos ATM (`LFPG_BTCAtm.c:63,72,100,117,142,165,181,202,207`);
  `LFPG_OnStoreSaveExtra` serializa `m_BtcStock` (`LFPG_BTCAtm.c:273-278`) — la premisa
  del fix E16 es cierta. Constantes `LFPG_BTC_ERR_*`/`LFPG_BTC_TX_*` existen
  (`LFPG_BTCDefines.c:40-64`). `LFPG_BalanceClaim`/`LFPG_BalanceData` en
  `3_Game/LFPG_Data.c:122-171`.
- **Firmas contra vanilla:** `DeleteFile`→bool, `CopyFile`→bool
  (`1_core/proto/ensystem.c:528,531`); `OpenFile`/`FPrintln`/`CloseFile`/`FileExist`
  (`ensystem.c:397,417,443,481`); `array.Find` (`1_core/proto/enscript.c:394`);
  `GetGame().ConfigIsExisting` (`3_game/global/game.c:611`); `GetQuantityMax`
  (`3_game/entities/entityai.c:2256`); `CreateInInventory(string)`
  (`3_game/systems/inventory/inventory.c:876`).
- **Regla de escapes:** lo añadido no contiene literales con dos escapes. Los dos hits
  del fichero (`FileUtil.c:563` `"\"ver\""`, `:584` `"\t"…"\r"…"\n"`) están en
  `TryReadRawJsonVersion`, código del latch de versión futura que el informe cita como
  patrón preexistente (`:551-552`) y que alimenta `s_FutureVersionReadOnly` (usado por
  `IsClaimStoreWritable`, ya citado en la ficha E04 como «comprobación anterior») →
  inferencia razonable de preexistencia. `:584` son literales de un solo escape, sanos
  por la regla documentada (`LFPG_SorterView_TEST.c:2073-2081`).
- **Nonce/sesión:** los rechazos nuevos pre-`ReserveRequest` en Sell (`:1585-1685`) son
  libres de mutación y usan `SendBTCTxResult`→`CompleteRequest`, que devuelve false sin
  slot IN_FLIGHT propio y no toca slots ajenos (`LFPG_BTCSessionRegistry.c:345-378`) —
  consistente con el patrón preexistente (powered/price, `:1513,1522`). Buy cash
  reserva **antes** de tocar inventario (`:1291`), Sell antes de la primera mutación
  (`:1689`).
- **E02:** jugador sin billetes → `PreparePlayerCash`→`MaxAffordableBtc`=0 → NO_CASH
  con **cero** entidades (`:1297-1313`); entrega parcial conservada vía
  `MaxAffordableBtc` + recálculo `actualCostIntC` (`:1347-1359`); aborts con
  `AbortOutputs` sin consumo de billetes (`:1340-1344`, `:1352-1357`, `:1364-1370`,
  `:1374-1380`).
- **E15 no tocada (afirmación del informe):** Sell no crea claims (no llama
  `DebitWithStockClaim`); la compactación de tombstones REFUNDED no altera
  `CountPendingPurchaseClaims` (solo cuenta PENDING con debit>0, `NativeImpl:204-216`).
  Confirmado.
- **Frontera de confianza (d):** sin hallazgos. Ninguna cantidad/saldo/identidad nueva
  viene del cliente; los rechazos nuevos envían error explícito; ningún error se
  convirtió en éxito silencioso. `requestedToAccount` (fingerprint con el bool del
  cliente) es preexistente y la política se revalida server-side (`:1585-1592`).
- **Aritmética Sell:** guarda de carry-overflow (`:1621-1626`) antes de
  `expectedIntegerPayout`; room nunca negativo (`:1630-1633`); `expectedCashPayout`
  acotado por construcción; `expectedCashStage ≤ 2e9+1 < INT_MAX`. Sin desbordamientos
  nuevos.

## VEREDICTO

**No desplegar tal cual.** No hay BLOQUEANTE de compilación detectable en estático, pero
nadie lo ha compilado y tres puntos exigen gate previo: (1) compilar/empaquetar una vez
antes de tocar el servidor vivo; (2) validar el `LF_BTCAtm.json` de producción contra
las reglas E08 o el efectivo muere en el primer boot; (3) firmar como producto la
ventana de duplicación E04 (crash post-crédito) y la poda de tombstones E16 con ATM
ausente ≥3 boots — ambas son vías de duplicación abiertas y admitidas, no olvidadas.

## LO QUE NO PUDE VERIFICAR

- **Compilación real:** prohibido compilar; el shell de esta sesión además bloquea
  hooks. Todo el apartado (a) es revisión estática. Un solo error de tipado que se me
  haya escapado tumba el módulo Mission entero al arrancar: el gate de build sigue
  pendiente y es innegociable.
- **El diff real contra el último build desplegado:** prohibido git; el fetch al repo
  remoto fue rechazado. Atribuyo «nuevo vs preexistente» por inferencia del informe y
  las fichas. Si `TryReadRawJsonVersion` (`FileUtil.c:546-617`) no ha pasado nunca por
  un build, el literal `"\"ver\""` (`:563`) choca con la regla CParser documentada y es
  BLOQUEANTE; mi inferencia dice que es preexistente, pero no puedo cerrarlo sin
  historial.
- **El `LF_BTCAtm.json` de producción** (¿pasa las 7 reglas E08?) y la existencia real
  de las clases de billetes en el PBO del server (`ConfigIsExisting` depende del
  config cargado en ese proceso).
- **Persistencia hive real:** que `SetSynchDirty` + `OnStoreSaveExtra` materialicen el
  stock compensado antes del siguiente boot (premisa de E16; el informe la admite sin
  verificar). Orden real hive vs JSON de balances en crash.
- **Comportamiento de `CreateInInventory` con stacks** (¿fusiona con pilas existentes
  del jugador?): asumido no-fusión por uso preexistente en `StageItemsForPlayerUnchecked`;
  si fusionara, `AbortOutputs`/`ObjectDelete` sobre salidas stagged podría tocar pilas
  propias del jugador. Preexistente, no introducido aquí, pero nunca verificado.
- **In-game:** ninguna prueba de regresión de las fichas ejecutada (brief lo prohíbe).
  Los caminos de crash (E04 dup, SEC09 in-flight, E16 reapply) están razonados, no
  medidos.
- **`GetQuantityMax` por clase concreta** de la economía de producción (magazines vs
  vehicles), del que depende la estimación de E03.
