# REPORT — T5 harness (instrumento, no resultados)

Lane `feat/t5-harness`. Sin commit. Sin DayZ, sin AddonBuilder, sin PBO.
Ningun caso se ha ejecutado. La matriz no lleva PASA/FALLA.

Copia maquina de la matriz: `harness/cases.json`.
Uso: `harness/README.md`. Driver: `harness/t5_fault_driver.py`.

---

## MATRIZ DE CASOS (VACIA — sin resultados)

| # | ficha | punto de inyeccion | preparacion | accion | reinicio? | invariante esperado | como se lee en el log |
|---|---|---|---|---|---|---|---|
| 1 | E04 | `AtomicSaveBalances_false` tras spill y marcador `.sell` | Native, sell a cuenta, spill stagged si hay overflow de cap | `HandleBTCSell` toAccount | no | BTC y EUR de spill intactos; INVALID; cero credito; sin marcador | `FIRE hook=AtomicSaveBalances_false`; `account credit differed from preflight`; `Add rollback: balance snapshot was not durable`. Prohibido: `[BTCSell] player sold ` |
| 2 | E04 | crash tras `AddBalance` OK, antes de `DestroyPlayerItems` | Anotar saldo B, credito C, BTC N. SIGKILL si el proceso vive. Desarmar JSON antes del boot 2 | Sell a cuenta; morir; reconectar | si | Credito B+C durable; reconcilio destruye N BTC | Boot1: `FIRE hook=E04_crash_after_credit` + `CRASH_NOW`. Boot2: `OBSERVE reconcile current=` |
| 3 | E04 | crash tras `WriteSellDestroyIntent`, antes de `AddBalance` | Igual. Desarmar JSON en boot 2 | Sell a cuenta; morir; reconectar | si | BTC intactos; marcador limpiado; cero credito | `FIRE hook=E04_crash_after_marker_before_credit` + `CRASH_NOW`; `sell-intent marker had no durable credit; marker cleared` |
| 4 | E04 | `ClearSellDestroyIntentAfterDestroy` omite el delete; rebase permitido (2a pasada / 3a pasada mitigacion) | Sell a cuenta hasta destruir. Desarmar JSON en boot 2 | Completar venta; reconectar | si | Marcador reescrito con `balanceBefore` = saldo post-credito. Boot 2 no re-destruye | `FIRE hook=E04_clear_after_destroy`; `rebased balanceBefore to current` |
| 5 | E04 | AfterDestroy: Clear y Write fallan (3a pasada, residuo) | remaining=2. Desarmar JSON en boot 2 | Completar venta; reconectar | si | Log de admin. Boot 2 PUEDE re-destruir BTC si el saldo sigue en before+credit. Residuo vivo: no cerrar la ficha desde esta lane | `FIRE` clear + rewrite; `marker survived after destroy and could not be rebased` |
| 6 | SEC09 | `SaveFile` deja `.tmp` parseable, sin `.saving`, sin Discard | Preferir first-save (sin target). Desarmar JSON en boot 2 | Un save Native; reboot | si | Boot 2 no promociona el `.tmp` | `FIRE hook=SaveFile_abort_leave_tmp`; `Orphan balances .tmp is not an in-flight replace. NOT promoting`. Prohibido: `In-flight balances .tmp parses as LFPG_BalanceData, promoting` |
| 7 | SEC09 | crash tras `DeleteFile(target)`, antes de `CopyFile(tmp,target)` | Target+bak existentes (no first-save). SIGKILL. Desarmar JSON | Un save; morir; boot 2 | si | Unica ventana donde el boot PUEDE promocionar | `FIRE hook=balances_after_delete_before_promote` + `CRASH_NOW`; `In-flight balances .tmp parses as LFPG_BalanceData, promoting` |
| 8 | SEC09 | `CopyFile(tmp,target)` no se ejecuta; marcador se limpia; restore desde bak | Target+bak. remaining=1. Desarmar JSON | Un save; reboot | si | Abort reportado. Boot 2 no promociona | `FIRE hook=CopyFile_promote`; `promote tmp->target failed`. Prohibido: promoting in-flight |
| 9 | SEC09 | residual (b) REVIEW-T1 MENOR: promote + delete `.saving` + restore + discard | remaining=8. Desarmar JSON | Un save; reboot | si | Si quedan `.tmp`+`.saving`+bak sin target, boot 2 PUEDE promocionar una mutacion abortada. Residuo abierto | `FIRE hook=CopyFile_promote`; `in-flight marker survived a reported abort` |
| 10 | E16 | crash tras apply de stock compensado (tombstones no se compactan en este path) | ATM REFUNDED con `restoredStock !=` hive. SIGKILL. Desarmar JSON | Boot 1 reconcilia y muere; boot 2 recarga el ATM | si | Tombstones siguen en JSON. Boot 2 no duplica reembolso+stock | `FIRE hook=E16_crash_after_apply` + `CRASH_NOW`; `Late ATM matched refunded tombstone; stock compensation applied, tombstones kept` |
| 11 | E16 | ninguno (operacional) | ATM refunded, ausente del registry >=3 boots, luego reaparece con stock hive viejo | Tres sweeps + reaparicion | si | Tras la 2a pasada T1, `AdvanceOrPruneRefundedClaimAt` ya no poda a 3 boots. Tombstone permanece; compensacion una vez | `Orphan sweep pass executed`. No hay FIRE. Comprobar a mano que el JSON de claims no perdio el tombstone |
| 12 | E16 | ninguno (operacional) | Tras un boot que compenso y conservo tombstones, hive ya coincide | Siguiente boot con ATM presente y `pendingCount==0` | si | Compacta | `hive stock already matched refunded compensation; tombstones cleared durably` |
| 13 | E02 | ninguno (input). JSON remaining=0 para OBSERVE | Jugador sin billetes. Catalogo valido. Request maxima cash buy | `HandleBTCBuy` cash | no | Cero entidades BTC. err 10 NO_CASH. Probe no se alcanza | `OBSERVE tx=1 err=10`. Prohibido: `[BTCBuy] cash: ` |
| 14 | E02 | ninguno (input) | Billetes cubren K < pedido | Cash buy | no | Entrega parcial | `[BTCBuy] cash: ` + OBSERVE |
| 15 | E02 | ninguno (input) | Puede pagar; inventario no acepta BTC/cambio | Cash buy | no | AbortOutputs, sin consumo de billetes | `OBSERVE tx=1 err=6` |
| 16 | E03 | ninguno (input). Moneda valor 1, pila 1, 65 entidades | Catalogo y fondos. Estimate*Entities=65 | Buy/sell/withdraw que exceda | no | AMOUNT_TOO_LARGE, cero spawn salvo probe | `entity budget` |
| 17 | E03 | ninguno (input). 64 entidades justo | Mismo catalogo, estimate=64 | La op | no | No rechaza por presupuesto | OBSERVE presente. Prohibido: `entity budget exceeded` / `entity budget rejected` |
| 18 | E03 | ninguno (input). BTC no apilable, withdraw 65 | btcItemClassname pila 1. Stock>=65 | `HandleBTCWithdraw` 65 | no | Rechazo antes de spawn | `[BTCWithdraw] rejected: entity budget exceeded before spawn` |
| 19 | E08 | `LF_BTCAtm.json` (no gancho de codigo). `Load()` una vez | Dos entries mismo classname. Reinicio | Boot + op de efectivo | no | Catalogo entero INVALID | `Duplicate currency classname`; `CURRENCY CATALOG REFUSED` |
| 20 | E08 | JSON, duplicado case-insensitive (2a pasada ToLower) | `Paper_Bill_100` y `PAPER_BILL_100` | Boot + cash | no | Igual que duplicados | mismas cadenas |
| 21 | E08 | JSON, currency == btcItemClassname | Overlap. Reinicio | Boot + cash | no | Catalogo rechazado | `Currency classname overlaps btcItemClassname` |
| 22 | E08 | JSON, classname vacio o inexistente en Cfg* | Una entry mala. Reinicio | Boot + cash | no | Catalogo rechazado | `CURRENCY CATALOG REFUSED` |
| 23 | E08 | JSON `[null]` o catalogo vacio | Defaults vacios o nulls. Reinicio | Boot + cash | no | Fail-closed. Account buy/sell siguen | `CURRENCY CATALOG REFUSED` |
| 24 | E08 | ninguno — gate de deploy REVIEW-T1 GRAVE | Copiar el `LF_BTCAtm.json` VIVO al box DIAG | Boot | no | Si el JSON vivo rompe una de las 7 reglas, el efectivo muere hasta editar+reiniciar | leer el bloque LogCatalogHelp; no hay veredicto de codigo |
| 25 | E05 | ninguno (input). Billete 100, deposito 60 | Native con room. Catalogo valido | `HandleBTCDepositCash` 60 | no | Cero saves. Efectivo intacto | `amount is not representable with prepared bills; no account mutation`. Prohibido: `EUR deposited from bills` y `Add uid=` |
| 26 | E05 | ninguno (input). Room Native 30, exacto representable | Saldo a 30 del cap. Billetes exactos a 30. Deposito 100 | DepositCash | no | Un save. Credito 30 | `[BTCDepositCash] 30 EUR deposited from bills` |
| 27 | E05 | ninguno (input). Room 30, [50,50] | Exact(30) imposible | DepositCash | no | Cero saves | misma cadena representable + prohibido `Add uid=` |
| 28 | E05 | `AddBalance_partial` (toAdd forzado a 1) | Deposito representable (50/50). remaining=1 para que el revert pueda guardar | DepositCash | no | credited != requested; no commit de billetes | `FIRE hook=AddBalance_partial`. Prohibido: `EUR deposited from bills` |

Ids estables (JSON + driver): columna implicita = `harness/cases.json` campo `id`. Escenarios copiables: `harness/scenarios/<id>.json`. Boot 2: copiar `harness/scenarios/DISARMED.json` o borrar `LF_FaultInject.json`.

---

## MECANISMO DE INYECCION

### Como se arma y se desarma, y por que no puede activarse por accidente

Tres pestillos en serie. Hace falta romperlos todos.

1. **Gate de compilacion** `LFPG_FAULTINJECT_COMPILE_GATE` en `scripts/3_Game/LFPG_FaultInject.c`.
   `#ifdef DIAG` → `true`. Retail `DayZServer_x64` (sin `DIAG`) → `false`.
   AddonBuilder no compila scripts: el exe que carga el PBO decide el macro.
   Con gate `false`, `ShouldFail` / `ShouldCrash` / `ObserveTx` / `Touch` vuelven
   en la primera linea. **No se abre el JSON. No se escribe una linea.**
2. **Fichero hermano que el mod nunca crea:** `$profile:LF_PowerGrid/LF_FaultInject.json`.
   No es `LF_BTCAtm.json`. `BTCConfig.Save()` no lo toca. Un `{}` o un parse
   fallido deja `enabled=false` por constructor.
3. **Frase exacta** `I_UNDERSTAND_THIS_DESTROYS_MONEY` + `enabled: true` +
   `scenario` no vacio.

`remaining` se decrementa **por gancho que dispara**, no por boot. Un
`AddBalance` que falla (consume 1) deja el `RemoveBalance` de rollback limpio.

Desarme: `enabled: false`, `remaining: 0`, frase distinta, o borrar el fichero.
Entre casos: reinicio (Load del instrumento es lazy, una vez por proceso, igual
que `LFPG_BTCConfig.Load()`).

No se usa `RequestExit` para la familia crash: `OnMissionFinish` llama
`FlushBalanceOnShutdown` y cerraria la ventana. El crash nativo va despues de
escribir `LFPG_FAULTINJECT CRASH_NOW` (y un sibling `.fired`). Si el proceso
sigue vivo, el orquestador hace SIGKILL.

### Que pasa si esto se despliega tal cual en produccion

**Nada**, en el sentido que pide el brief, sobre un dedicado retail:
`DIAG` no esta definido, el gate es `false`, los ganchos son no-ops, el JSON
aunque exista no se lee, no hay lineas `LFPG_FAULTINJECT` en el log, y el
dinero sigue el codigo T1 identico.

Si alguien corre **DayZDiag** como si fuera produccion, el gate pasa a `true`.
Sigue haciendo falta el JSON con la frase. Eso no es produccion; es el box de
prueba. Si el orquestador usa dedicado retail, tiene que poner `true` solo en
la rama `else` del gate, y no enviar ese artefacto a jugadores.

Leccion S1_PROBE: el armado **no** vive en `LF_BTCAtm.json`. Un campo ahi
habria sobrevivido al `Save()` y viajado al profile de produccion.

### Fallo devuelto vs muerte del proceso

- **Fallo devuelto:** `ShouldFail(hookId)` → el call-site toma la rama `false`
  de `CopyFile` / `AtomicSaveBalances` / `Clear` / `AddBalance` parcial, sin
  llamar al IO real cuando el fallo es "no copies / no borres".
- **Muerte del proceso:** `ShouldCrash(hookId)` registra FIRE + CRASH_NOW,
  escribe `LF_FaultInject.fired`, llama `GetPosition`/`SetPosition` sobre un
  `Object` nulo, y si eso no mata, un `while (true)` para que no siga la
  transaccion ni un shutdown limpio.

---

## GANCHOS

`LFPG_MissionInit.c` y `LFPG_BTCConfig.c` no se tocaron. El reconcilio E04 ya
corre desde `InvokeOnConnect` → `ReconcilePendingAccountSell`. E08 se arma
editando `LF_BTCAtm.json`, no el loader.

| ficha | fichero:linea | que intercepta | por que ahi |
|---|---|---|---|
| (boot) | `LFPG_FileUtil.c:215` / `:819` | `Touch()` al entrar en save y en `EnsureBalancesFileOrRestore` | Primera lectura del JSON; ARMED/DISARMED en el log antes de la tx o en el boot de recuperacion |
| E04 #1 | `LFPG_FileUtil.c:216` | `AtomicSaveBalances` return false inmediato | El spill y el marcador `.sell` ya los hizo el caller; este es el save del credito |
| E04 #2 | `LFPG_BTCHelper.c:1908` | crash tras credito OK, antes de `DestroyPlayerItems` | Ventana literal del informe y del GRAVE E04 |
| E04 #3 | `LFPG_BTCHelper.c:1880` | crash tras `WriteSellDestroyIntent`, antes de `AddBalance` | Tercer experimento E04 |
| E04 #4/#5 | `LFPG_BTCHelper.c:1401` / `:1409` | omitir Clear / omitir Write rebase en AfterDestroy | No se engancha `WriteSellDestroyIntent` por dentro: el write inicial de la venta no debe consumir `remaining` |
| E04 reconcilio | `LFPG_BTCHelper.c:1461` / `:1494` | `ObserveReconcile` | Canal de vuelta en boot 2 (sin mutar) |
| SEC09 #6 | `LFPG_FileUtil.c:234` | tras `SaveFile` OK, return false sin Discard | `.tmp` parseable, sin `.saving` |
| SEC09 #7 | `LFPG_FileUtil.c:278` | crash tras borrar target, antes de promote | Ventana Delete+Copy con `.saving`+bak |
| SEC09 #8 | `LFPG_FileUtil.c:281` | no llamar `CopyFile(tmp,target)` | Abort reportado; restore real sigue (remaining=1) |
| SEC09 #9 | `LFPG_FileUtil.c:230,281,294,299,489,499` | promote + `.saving` + restore + discard | Residual (b) en un solo save |
| E16 #10 | `LFPG_BalanceProvider_NativeImpl.c:1243` | crash tras apply+log, antes del `return` | Tombstones no se compactan en este path; el crash prueba hive vs JSON |
| E05 #28 | `LFPG_BalanceProvider_NativeImpl.c:1745` | `toAdd = 1` antes del save | Credito parcial reportado; remaining=1 deja vivo el revert |
| E02/E03/E05 oraculo | `LFPG_BTCHelper.c:274` | `ObserveTx` en `SendBTCTxResult` | E02 NO_CASH no tenia `Error()` propio; el log es el unico canal |

Fichero nuevo (modulo Game, alfabetico antes de `LFPG_FileUtil.c`): `scripts/3_Game/LFPG_FaultInject.c`. No hace falta declararlo en `config.cpp`.

---

## PRUEBA DE NO-REGRESION

Con gate `false` (dedicado retail) o JSON ausente/frase mala, cada gancho es
un `if` cuyo predicado es false. Recorrido:

| gancho | por que la ruta normal es la de antes |
|---|---|
| `Touch` / `ObserveTx` / `ObserveReconcile` / `ShouldCrash` como statement | return inmediato si gate false; cero IO, cero log |
| `AtomicSaveBalances` early `ShouldFail` | no match → no return; sigue SaveFile como siempre |
| `DiscardAbortedTmp_false` en el `if (!SaveFile)` | `!false` → se llama `DiscardAbortedBalancesTmp` igual que antes |
| `SaveFile_abort_leave_tmp` | no match → no return |
| `ShouldCrash` post-DeleteFile | no match → no return; `CopyFile` se llama |
| `CopyFile_promote` | `!false` → `CopyFile(tmp,target)` real |
| `CopyFile_restore` | `!false` → `CopyFile` de bak igual |
| `DeleteFile_saving` | no match → `DeleteFile` real |
| `DiscardAbortedTmp_false` en Discard | FileExist primero; no match → Preserve+Delete iguales |
| AfterDestroy Clear/Write | `!ShouldFail` true → mismas dos llamadas, mismo orden |
| crash marker / crash credit | `if (false) return` no sale; AddBalance / Destroy iguales |
| E16 `ShouldCrash` antes del return | valor ignorado; `return` identico al original |
| `AddBalance_partial` | bloque no entra; `toAdd` no se toca |
| `ObserveTx` entre CompleteRequest y Send payload | no escribe; orden RPC inalterado |

No se reordeno logica T1. No se toco `config.cpp` ni los ficheros vetados
(RPCServer, NetworkManager, SorterView, BTCAtm*).

---

## DRIVER

```
python harness/t5_fault_driver.py --list
python harness/t5_fault_driver.py --case E04_save_false_after_spill --log boot1.rpt
python harness/t5_fault_driver.py --case E04_crash_after_credit --log boot1.rpt --log boot2.rpt
```

Lee `harness/cases.json`. Concatena los logs. Codigos: 0 PASS, 1 FAIL,
2 INCONCLUSIVE, 3 uso/id desconocido.

INCONCLUSIVE (no FAIL) si falta `ARMED`, si hay `DISARMED`, si un caso
inyectable no tiene FIRE, o si un crash no tiene `CRASH_NOW`. Un FAIL
inventado por un log vacio no debe salir: sin `--log` existente, 2.

No se ha corrido contra un RPT real en esta lane.

---

## COMO LO USA EL ORQUESTADOR

1. Confirmar que el exe de prueba define `DIAG` (DayZDiag). Si es dedicado
   retail, flipar solo el `else` del gate en el artefacto de prueba.
2. Parar el servidor.
3. Copiar `harness/scenarios/<id>.json` a `$profile:LF_PowerGrid/LF_FaultInject.json`.
4. Preparar mundo / JSON de ATM / ledger segun la fila.
5. Arrancar. Exigir `LFPG_FAULTINJECT ARMED scenario=<id>` en el script log.
   Si no sale: INCONCLUSIVE, no se declara la ficha.
6. Disparar la accion (RPC ATM, o dejar que el boot reconcilie).
7. Si crash: `CRASH_NOW` y SIGKILL si el proceso vive. No shutdown limpio.
8. Si la fila pide reinicio: poner `DISARMED.json` o borrar el JSON. Arrancar.
   Recoger boot2.
9. `python harness/t5_fault_driver.py --case <id> --log ...`
10. Rellenar la matriz. Esta lane no lo hace.

Un caso por boot. `Load()` del instrumento es una vez por proceso.

---

## QUE NO CUBRE

- **E16 ausente 3 boots (#11) y compactacion hive (#12):** no hay gancho que
  simule "ATM fuera de burbuja" ni `OnStoreSaveExtra`. Operacional.
- **Persistencia hive de `m_BtcStock`:** el crash E16 prueba el codigo de
  claims, no que DayZ haya materializado el store extra. El informe T1 ya
  lo marca no verificado.
- **Wires/settings `PromoteOrphanTmp` sin marcador:** REVIEW-T1 MENOR, FileUtil
  91–105 / 174–186. Fuera de T1; no se engancho `AtomicSaveVanillaWires` /
  `AtomicSaveSettings`.
- **E15** (8 PENDING por ATM): el informe T1 dice no implementada. Sin casos.
- **Cadenas mixtas PENDING+REFUNDED:** compactacion de tombstones espera
  `pendingCount==0`. Sin gancho.
- **`FindFile("$profile:")` de `.tmp.preserved.*`:** no medido, no inyectado.
- **Fusion de `CreateInInventory` con pilas existentes:** preexistente, no T1.
- **E08 JSON de produccion (#24):** gate de deploy, no de codigo.
- **Subset-sum greedy E05:** residuo admitido; no hay gancho que pruebe
  [100,60,60]→120. El caso #25 cubre el escenario de la ficha (100 vs 60).
- **Probe create+delete E03:** el informe lo admite; el oraculo "cero spawn
  salvo probe" no cuenta entidades de probe (no hay log de probe).
- **Listen server vs dedicado:** el gate mira `DIAG`, no `IsDedicatedServer()`.
- **`SendBTCReplayResult` / nonce reject:** no pasan por `ObserveTx`.
- **Compilacion in-game y AddonBuilder:** prohibidos. El `.c` nuevo se asume
  recogido por el modulo Game como el resto de `scripts/3_Game/*.c`.

---

## LO QUE NO PUDE VERIFICAR

- **Cero casos ejecutados.** Ni un sell, ni un crash, ni un boot de
  recuperacion. El driver no ha visto un RPT. Cualquier tabla PASA/FALLA
  aqui seria fabricada.
- **Compilacion Enforce.** AddonBuilder/PBO/DayZ prohibidos. El shell de
  este host mata python/bash con hooks de PowerShell (`launch-ledger`,
  `prime-agent-skills-gate`, `gpu-lease-gate`) antes de arrancar el comando:
  `python enfcheck.py` **no se ejecuto**. Equivalente con grep sobre
  `LFPG_FaultInject.c`: 0 ternarios, 0 `foreach`, 0 `++`/`--`/`+=`/`-=`,
  `ref` solo en el miembro `s_File`, una `Error()` por linea en el
  instrumento, literales sin dos escapes adyacentes (los `\\` de
  `$profile:...\\LF_FaultInject.json` son el mismo patron que
  `LFPG_BTC_SETTINGS_FILE` en `LFPG_BTCDefines.c:36`). Llaves `{` y `}`
  aparecen en 31 lineas cada una; no es el recuento de tokens de enfcheck.
  El orquestador corre el linter de verdad al recibir.
- **Que `DIAG` este definido en el exe que usara el puente MCP.** Si no lo
  esta, todos los casos inyectables salen INCONCLUSIVE hasta flipar el else.
- **Que un deref nulo de `Object.GetPosition` mate el proceso** y no solo
  el VM de script. Por eso CRASH_NOW + SIGKILL + bucle infinito.
- **Orden real hive vs JSON** en el crash E04/E16.
- **`GetQuantityMax` de las clases de produccion** (E03).
- **Que `JsonFileLoader<LFPG_FaultInjectFile>` acepte el JSON de escenarios**
  (bool `enabled`, etc.) en esta version del engine.

---

## LA PREMISA DE ESTE ENCARGO

La inyeccion en codigo es la forma correcta **solo** para las fichas cuyo
fallo es una mentira de IO o una muerte entre dos lineas: E04 (save false,
dos crashes, marcador huerfano), SEC09 (SaveFile/CopyFile/DeleteFile y la
ventana Delete+Copy), E16 crash post-apply, y el AddBalance parcial de E05.
Ahi no hay prueba mas barata que deje el disco en el estado que el
reconcilio va a ver.

Para E02, E03, E05 (100/60 y room) y E08, el `Como comprobarlo` del informe
T1 **no pide inyeccion**: pide un jugador, un catalogo o un amount. Un RPC
contra un server DIAG con el JSON de ATM ya da la misma certeza, y es mas
barato. El harness las incluye para que el orquestador no tenga que
re-leer el informe, y les anade `ObserveTx` porque E02 NO_CASH **no escribe
un `Error()`** — sin ese gancho el log no veria el codigo 10.

Hay fichas cuyo `Como comprobarlo` es insuficiente para lo que el encabezado
afirma:

- **E16 "CERRADA"** en el informe de implementacion, con un `Como comprobarlo`
  que mezcla reaparicion, crash y compactacion. El GRAVE de REVIEW-T1 sobre
  poda a 3 boots **ya no esta en este arbol** (2a pasada: `AdvanceOrPruneRefundedClaimAt`
  en `LFPG_BalanceProvider_NativeImpl.c:1390-1396` hace `return false` y no
  llama `PersistRemoveClaimAt`). Lo que sigue sin demostrarse es hive
  `OnStoreSaveExtra`, y eso **no lo cierra un gancho de script**. El
  experimento #11 es operacional; si alguien lo da por cerrado porque el
  codigo "ya no poda", esta leyendo construccion, no evidencia — exactamente
  el hueco que este tramo T5 existe para no repetir.
- **E04 crash post-credito:** el `Como comprobarlo` dice "al reconectar,
  credito intacto y BTC destruidos". Eso demuestra el reconcilio del
  marcador, no que no haya habido una ventana de dup visible al jugador
  *antes* de reconectar, ni el orden hive de inventario vs JSON. Es el
  experimento correcto para la mitigacion que T1 escribio; no demuestra
  ausencia de dup si el reconcilio falla en silencio (saldo ambiguo → no
  destruye, marcador queda, GRAVE convertido en ticket de admin).
- **E03 "cero spawn (salvo probe)":** el probe no deja traza de nivel 0.
  Un oraculo que solo busca `entity budget` no cuenta entidades. Si el
  preflight se saltara y el spawn fallara despues, el log podria parecerse.
- **SEC09 residual (b):** cinco fallos de IO a la vez. El `Como comprobarlo`
  del informe no pide esa pila; la pidio el revisor. Reproducirla no dice
  casi nada sobre produccion. El caso #9 existe para no dejar el residuo
  innombrado, no porque sea el experimento barato.

La forma incorrecta de usar este instrumento: armar el gate, no ver FIRE, y
leer la ausencia de `player sold` como PASA. El driver trata eso como
INCONCLUSIVE a proposito.

---

## VISTO-DE-PASO

- En este worktree, E16 ya no poda tombstones ausentes a 3 boots. El GRAVE
  de `REVIEW-T1.md` describe el parche de primera pasada. Cite
  `NativeImpl.c:1386-1396` abierto ahora, no las lineas del review.
- `LFPG_PERFDIAG_ENABLED` (`LFPG_Defines.c:405`) es `false` y los `if` siguen
  en el binario. El instrumento copia ese pestillo, no el fallo S1_PROBE
  (`LFPG_SorterView_TEST.c` `S1_PROBE = true` sin gate).
- No toque `LFPG_BTCConfig.c`: un campo de armado ahi habria salido en el
  `Save()` al profile. E08 se prueba mutando el catalogo, que es el sujeto.
- No toque `LFPG_MissionInit.c`: `InvokeOnConnect` ya llama al reconcilio.
- El shell no corre aqui. Lo que pide el §11 del brief (enfcheck) queda para
  el orquestador; no invento una salida.

---

## Ficheros de esta lane (sin commitear)

- `scripts/3_Game/LFPG_FaultInject.c` (nuevo)
- `scripts/3_Game/LFPG_FileUtil.c` (solo ganchos)
- `scripts/5_Mission/LFPG_BTCHelper.c` (solo ganchos)
- `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c` (solo ganchos)
- `harness/README.md`
- `harness/cases.json`
- `harness/t5_fault_driver.py`
- `harness/scenarios/*.json`
- `REPORT.md` (este fichero)
