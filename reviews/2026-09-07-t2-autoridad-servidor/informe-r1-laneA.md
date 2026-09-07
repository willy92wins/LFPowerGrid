# REPORT — T2 lane A: HandleFinishWiring (SEC02 / SEC03 / SEC20)

Arbol: worktree `fix/t2-finishwiring`. Solo se edito `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`. Sin commit.

## DECISION-DE-PRODUCTO

Al rechazar un reemplazo que cortaria el cable de otro jugador, **se aborta toda la operacion**. No se borra el cable de origen, no se toca el puerto de destino, no se guarda el cable nuevo. El brief no dejaba tomar la variante permisiva (saltar solo ese borrado y seguir); G6 pide fail-closed. El camino de corte (`HandleCutWires`) ya lee `AllowCutOthersWires`; el de acabado de cable no puede ser una puerta trasera.

---

### SEC02 — CERRADA
- **Que cambie:**
  - `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:630-636` — se lee `AllowCutOthersWires` y el `GetPlainId()` del sender, igual que `HandleCutWires:1142-1146`.
  - `:647-674` — preflight **despues** del lock del puerto de destino y **antes** de `BeginGraphMutation`. Si la politica esta off y el puerto de origen o el de destino tienen un cable ajeno (`m_CreatorId` no vacio y distinto del sender), se hace `UnlockPort` y return. Mensaje al jugador: `"Cannot replace another player's wire."`
  - `:770` — `RemoveWiresTargeting(dstRealId, dstPort, finishCutPid, finishAllowOthers)` (antes eran dos argumentos; el default `allowOthers=true` saltaba la politica). Firma leida, no editada, en `LFPG_NetworkManagerImpl.c:1552`.
  - Helpers `:994-1031` (`FinishWiringSourcePortHasForeign`) y `:1034-1098` (`FinishWiringDestHasForeignIncoming`). El scan de destino recorre los stores (registry LFPG + `GetVanillaWireOwnerCount`/`GetVanillaWires`), no el grafo: los cables son la fuente de verdad. Criterio: `LFPG_WireHelper.CanCreatorCutWire` (`LFPG_WireHelper.c:230-237`) — propio o sin reclamar, como el corte.
- **Por que asi:** abortar entero, no “saltar el borrado y conectar igual”. Conectar sin borrar el incoming ajeno deja dos cables en el mismo IN; borrar solo los propios y seguir es la variante permisiva que el brief prohibe tomar por nuestra cuenta. Descarte: llamar `RemoveWiresTargeting` filtrado sin preflight (mutaria los propios y dejaria el ajeno; o, con `allowOthers=true`, lo borraria).
- **Que NO cubre:**
  - Cables ajenos que el reverse index cuenta (`CountWiresTargeting`) pero cuyo `m_TargetPort` literal no coincide con el destino normalizado. `RemoveWiresTargeting` compara `wd.m_TargetPort == targetPort` exacto (`LFPG_NetworkManagerImpl.c:1603`), mientras el indice trata `""` como `input_main`. El preflight usa `IncomingPortIndexKey` (`:2140-2147`) y aborta si ve un ajeno bajo esa clave; si el ajeno **no** esta en ningun store visible al scan, no lo vemos.
  - El lock cubre el puerto de destino, no el store del origen. Dos `FinishWiring` concurrentes sobre el mismo generador no estan serializados.
  - `GetPlainId()` vacio: `CanCreatorCutWire` falla cerrado y abortamos todo reemplazo. No lo he visto ocurrir; no lo probe.
- **Como comprobarlo:** servidor con `AllowCutOthersWires=false`. Jugador A deja un cable en el IN de un spotlight vanilla. Jugador B, con carrete, intenta conectar su generador a ese mismo IN. Esperado: B recibe el mensaje de rechazo, el cable de A sigue, el grafo y el renderer de A no cambian. Repetir con `AllowCutOthersWires=true`: el cable de A desaparece y entra el de B. Control positivo del corte: con la flag en false, B con alicates sobre el spotlight de A no puede cortar (camino ya existente); el acabado de cable tiene que comportarse igual.

### SEC03 — CERRADA
- **Que cambie:**
  - `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:647-657` — `FinishWiringCanAdmitAfterReplace` corre bajo el lock, antes de mutar. Si el store del origen, **despues** de quitar los cables del mismo puerto de salida, seguiria en o por encima del tope, se deniega con el mismo mensaje de “full/duplicate” y sin `BeginGraphMutation`.
  - Helper `:936-992`. Replica los topes reales: LFPG = `MaxWiresPerDevice` (si >0) **y** `LFPG_MAX_WIRES_PER_DEVICE` (`LFPG_WireHelper.c:164-172`); vanilla = `MaxWiresPerDevice` si >0 si no el hardcap (`AddVanillaWire` en `LFPG_NetworkManagerImpl.c:884-892`), **sin** aplicar el hardcap por encima del setting — esa asimetria ya estaba y no la “corrijo” aqui. El recuento usa `wires.Count()` menos las filas cuyo puerto de origen coincidira con el reemplazo (en vanilla, `""` cuenta como `output_1`, igual que el bucle `:735-740`).
- **Por que asi:** el brief pide que un fallo de admision no destruya conexiones. Duplicate exacto despues de reemplazar el mismo `srcPort` no es un fallo de admision: se quita y se vuelve a poner. El fallo real es “este origen ya esta lleno y este puerto de salida no libera hueco”. Eso se decide leyendo el array, sin API nueva en `LFPG_NetworkManager`. Descarte: guardar primero y borrar despues (rompe el invariante 1-wire-por-IN si el add cuela). Descarte: deshacer los borrados a mano (no hay snapshot de los cables quitados que no pase por mutar `NetworkManagerImpl.c`).
- **Que NO cubre:**
  - Si `AddDeviceWire`/`AddVanillaWire` fallan por una causa **no** decidible con el recuento (el `CallBool` de un owner que no es `LFPG_WireOwnerBase`, un `HasPort` que se pone falso entre el check LFPG y el add), el bloque `:794-830` sigue siendo destructivo: persiste el borrado a proposito y fuerza rebuild. Con el preflight, cap y duplicado del mismo puerto no deberian llegar ahi; una carrera sobre el mismo origen si.
  - No preflighteo el tope de **grafo** (`LFPG_MAX_EDGES_PER_NODE=12` en `LFPG_Defines.c:520`). Eso no es “dispositivo lleno” del store; mezclarlo con `MaxWiresPerDevice=128` es el falso positivo que el brief prohibe. El fallo de insercion de arista es SEC20b, no SEC03.
  - Vanilla de un solo puerto: si el origen solo tiene cables en `output_1`, el reemplazo libera el hueco y el preflight deja pasar. El escenario 128 no aplica a ese dispositivo.
- **Como comprobarlo:** LFPG splitter (varios OUT) con `MaxWiresPerDevice` (o el hardcap 64) ya saturado en puertos distintos de `output_2`. Un segundo jugador (o el mismo, da igual para el cap) intenta un cable nuevo desde `output_2` hacia un IN que ya tiene cable. Esperado: rechazo “Wire already exists or device is full.”, el incoming previo intacto, sin rebuild RC-06 en el log. Control: con un hueco libre en el origen, el mismo IN se reemplaza y el cable nuevo queda.

### SEC20 — CERRADA
- **Que cambie:**
  - **(a) nombre de puerto vanilla.** `:464-538` — si `GetDeviceId` es `""` (vanilla; `GetOrCreateDeviceId` no escribe id en la entidad, `LFPG_IDevice.c:96-107`), el puerto tiene que existir en `GetPortCount`/`GetPortName`/`GetPortDir`, que para vanilla son 1 puerto: `output_1` OUT o `input_main` IN (`LFPG_IDevice.c:820-868`). `""` se acepta y se normaliza a esas constantes (o al primer puerto de esa direccion si `output_1`/`input_main` no existieran). Cualquier otra cadena de ≤32 caracteres se deniega antes de mutar. Helpers `:881-933`.
  - **(b) no difundir antes del grafo.** `:837-878` — `NotifyGraphWireAdded` corre, se cierra la mutacion y se suelta el lock; `BroadcastOwnerWireDelta` / `BroadcastVanillaWires` solo si `edgeAdded`. Si no, rebuild como antes y **return sin broadcast del cable nuevo**.
- **Por que asi:** (a) reutiliza la enumeracion de puertos que `HandleCutWires` ya usa para vanilla, en vez de hardcodear un if suelto que se desvie. (b) el orden viejo difundia y luego insertaba; el fallo de insercion no deshacia el RPC. No invente API en `NetworkManager`. Descarte: rollback del store si el grafo rechaza — exigiria borrar el cable recien escrito y reponer los reemplazados, otra vez chocando con la otra lane.
- **Que NO cubre:**
  - LFPG nativo sigue por `HasPort` (`:468-476`, `:503-511`). Esta ficha era el agujero vanilla.
  - `LFPG_AddWire` hace `SetSynchDirty` al guardar (`LFPG_WireOwnerBase.c:195-201`). En un owner LFPG, el SyncVar puede ensear el cable nuevo a los clientes aunque el grafo rechace y no haya `BroadcastOwnerWireDelta`. Vanilla no tiene ese SyncVar; ahi (b) cierra el agujero del RPC.
  - Insercion de grafo fallida: el cable **sigue en el store** y se fuerza rebuild (`:850-861`). Store y grafo pueden diverger hasta que el rebuild reconstruya (o siga rechazando por cap de nodos/aristas). No difundimos el fantasma; tampoco deshacemos el store.
  - No toca `CanConnectTo` en vanilla (el camino LFPG-only se mantiene). Un vanilla source sigue sin esa comprobacion de tipo de destino mas alla de `IsEnergyConsumer`.
- **Como comprobarlo:**
  - (a) cliente modificado manda `FINISH_WIRING` a un `PowerGenerator` vanilla con `srcPort="bogus_port"`. Esperado: warn `denied (vanilla src port ...)`, nada en el store vanilla, nada en el reverse index. Control: `srcPort="output_1"` (o vacio) hacia un spotlight con `dstPort="input_main"` funciona.
  - (b) saturar `LFPG_MAX_EDGES_PER_NODE` en un origen LFPG de muchos OUT y cablear uno mas. Esperado: warn de arista no insertada, **sin** delta/vanilla broadcast de ese add; el renderer del cliente no pinta un cable que el grafo acabo de rechazar. El rebuild posterior es el mismo de siempre.

## SALIDA DEL LINTER

**No pude ejecutar `python enfcheck.py`.** El shell de esta sesion esta bloqueado por un hook de Cursor que inyecta PowerShell (`$OutputEncoding ... | & { ... powershell ... launch-ledger.ps1 }`) en bash; bash revienta en `&` y el comando no llega a arrancar. El mismo bloqueo ocurrio en el baseline (antes del primer edit) y en un subagente. `enfcheck.py` no produjo stdout.

Lo que si pude mirar a ojo sobre el diff, con las reglas 6.1-6.4:

| Comprobacion | Resultado en el codigo nuevo |
|---|---|
| ternarios `?:` | ninguno |
| `++` / `--` | ninguno (hay un `--` preexistente en un comentario de searchlight, ~1581; el regex del linter lo contaria igual antes y despues) |
| `+=` / `-=` | ninguno |
| `foreach` | ninguno |
| `ref` en locales nuevos | no anadi ninguno (los `ref array` de `srcDeltaOps` en `:685-686` son preexistentes) |
| escapes adyacentes | no anadi literales con `\\` |
| llamadas con lista de argumentos partida | no anadi ninguna (siguen las preexistentes de `NotifyGraphWireRemoved` en el bucle de reemplazo) |

Grep de lineas (no de ocurrencias; **no** sustituye a `enfcheck.py`): `{` 359 lineas / `}` 359 lineas en el fichero entero.

## LO QUE NO PUDE VERIFICAR

- Compilacion Enforce (AddonBuilder/juego). Nadie en este worktree puede.
- `enfcheck.py` sobre el estado inicial ni el final (shell muerto).
- Comportamiento in-game de las tres fichas: no hay servidor, no hay PBO, el brief lo prohibe.
- Que `GetPortCount`/`GetPortName` en un vanilla no-PowerGenerator/no-Spotlight (si el whitelist de `IsVanillaSource`/`IsVanillaConsumer` se amplia) siga siendo 1 puerto. Me apoyo en `LFPG_IDevice.c:820-851` tal como esta hoy.
- Que `GetPlainId()` del sender coincida siempre con `m_CreatorId` guardado al crear el cable (`:581`). El corte ya usa ese par; si hay mismatch de identidad, SEC02 y el corte fallan igual.
- Que el scan de `FinishWiringDestHasForeignIncoming` sea barato en un mapa con miles de devices LFPG. Es el mismo recorrido que `RescueStaleIncomingWires`; el RPC ya va rate-limited. No lo medi.

## LA PREMISA DE ESTE ENCARGO

- El diagnostico de las tres fichas **casa con este arbol**. `RemoveWiresTargeting` en el reemplazo iba de dos argumentos; `GetDeviceId` vanilla sigue `""` despues de `GetOrCreateDeviceId`; `HasPort` solo corria si esas banderas eran true; el broadcast iba antes de `NotifyGraphWireAdded`.
- El arreglo correcto de SEC02/SEC03 **podia** haber vivido dentro de `RemoveWiresTargeting` / `AddVanillaWire` (un `wouldAdmit` o devolver “skipped foreign” distinto de “removed 0”). El veto a `LFPG_NetworkManagerImpl.c` obliga a decidir en el handler con un scan. Es peor en coste, no en autoridad: el handler es el que tenia la puerta abierta.
- SEC20(a) **no** se cierra validando longitud. Hacia falta un nombre que exista. Use la enumeracion vanilla de `LFPG_DeviceAPI`, no un literal suelto, para no inventar un tercer vocabulario de puertos.
- SEC20(b) **no** cierra la divergencia store/grafo cuando la arista no entra: el brief pide no difundir, no pide rollback del store. El rebuild sigue siendo el plan de recuperacion que ya habia. Si el orquestador queria atomicidad store+grafo, hacia falta o tocar el manager o un snapshot de cables; las dos cosas estaban vetadas o eran producto.
- El linter estructural era el unico gate local y **no corrio**. Un informe que pegue una salida inventada de `enfcheck.py` seria peor que decir que el proceso no arranco.

## VISTO-DE-PASO

- `ref` en locales (`srcDeltaOps`, `srcDeltaWires`, y los equivalentes de corte). La casa lo prohibe; DayZDiag lo mata en parametros (SP-047). No lo toque: refactor ajeno a las fichas.
- `NotifyGraphWireRemoved(...)` partido en varias lineas en el reemplazo de origen (`:707-709`, `:745-747`). El compilador de Mission ha muerto por listas de argumentos multilinea. Preexistente; no lo “alinee” para no mezclar diffs.
- `RemoveWiresTargeting` no normaliza `m_TargetPort` vacio al comparar, pero el reverse index si. Hueco real para incoming legacy con puerto vacio; el arreglo fiel vive en el manager (otra lane).
- El reemplazo de origen LFPG muta `srcWires` a mano y solo hace `LFPG_CommitWireMutation` si el store falla (`:813-816`). El exito depende de `AddDeviceWire` → `SetSynchDirty`. Coherente con lo que habia; raro, no lo cambie.
- `HandleCutWires` sigue siendo la referencia de politica; no lo toque.
