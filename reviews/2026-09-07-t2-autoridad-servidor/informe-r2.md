# REPORT — T2 ronda 2: el reemplazo de cable como transaccion

Arbol: worktree `fix/t2-r2-transaccion`. Cambios sin commit.

Ficheros tocados:
- `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`
- `scripts/5_Mission/LFPG_NetworkManagerImpl.c`
- `scripts/5_Mission/LFPG_FinishWiringTxn.c` (nuevo; la carpeta ya esta en `config.cpp`, no hace falta tocarlo)

---

### F-01 — CERRADO
- **Que cambie:**
  - El reemplazo deja de mutar a trozos en el handler. `HandleFinishWiring` monta un `LFPG_FinishWiringTxnRequest` y llama `TryCommitFinishWiring` (`LFPG_RPCServerHandlerImpl.c:675-686` → `LFPG_NetworkManagerImpl.c:955-1009`).
  - Un solo conjunto de conflictos (puerto de origen **o** incoming del destino, sin duplicar la misma fila) se calcula **antes** de quitar ninguno (`FinishTxnCollectConflicts` `:1093`, `FinishTxnCollectFromArray` `:1071`).
  - La mutacion (`FinishTxnDetachConflicts` `:1220`) quita grafo + indice inverso + recuento de jugador **por cable** (el credito de `NotifyGraphWireRemoved` es de un solo uso) y despues las filas del almacen.
  - El cable nuevo entra en el almacen (`FinishTxnInsertNew` `:1325`) y solo entonces se pide la arista (`NotifyGraphWireAdded` en `:997`).
  - Si el almacen o el grafo fallan: se saca el cable nuevo si llego a insertarse (`FinishTxnUninsertNew` `:1384`), se reponen las filas, indices, recuentos y aristas viejas (`FinishTxnRestoreConflicts` `:1286`), se cierra el batch de grafo y **no** se publica ni se marca persistencia vanilla. El log `SUCCESS` (`LFPG_RPCServerHandlerImpl.c:720`) solo corre tras `OK`.
  - Ya no se llama `PostBulkRebuildAndPropagate` para “reconciliar” un almacen que contiene el cable rechazado. Eso era exactamente el fantasma de SEC20(b): el rebuild y el full sync (`SendVanillaWiresTo`) leen el almacen.
- **Por que asi:** el revisor pidio una operacion cuyo exito exige almacen **y** grafo, publicada despues. Replica parcial de reglas en el handler (ronda 1) no puede deshacer escrituras que viven en el manager. Alternativa descartada: dry-run del tope de 12 aristas en el handler — seria otra replica, la misma clase de defecto que F-02. Alternativa descartada: rebuild-desde-almacen tras el rechazo — deja el decimotercer cable persistible y lo reinyecta al siguiente sync.
- **Que NO cubre:**
  - `NotifyGraphWireAdded` sigue poniendo `m_GraphFullRebuildRequired` si la arista nueva falla (`LFPG_NetworkManagerImpl.c:997` via `:991` del metodo original). Tras el rollback el almacen ya no tiene el fantasma; un rebuild **posterior** reconstruiria desde el almacen restaurado, no desde la fila rechazada. Puede haber un rebuild extra, no un cable fantasma.
  - Si `NotifyGraphWireAdded` de la restauracion fallara (no lo he visto en el camino de tope de aristas: `AddEdgeInternal` rechaza **antes** de insertar, `LFPG_ElecGraphImpl.c:1346-1350`), store y grafo podrian quedar desfasados hasta el siguiente rebuild. No es el escenario F-01.
  - Datos legacy con puertos inventados (`legacy_0` …) siguen en disco. No los borro ni cambio el formato persistente. El contrato nuevo evita **ampliar** esa divergencia; no repara el historial.
  - El SyncVar nativo de un `LFPG_WireOwnerBase` no se toca hasta `LFPG_CommitWireMutation` en `FinishTxnPublish` (`:1458-1462`). Un fallo no hace commit. Owners LFPG que no castean a `LFPG_WireOwnerBase` caen al fallback `SetSynchDirty` + `BroadcastOwnerWires` solo en el exito.
- **Como comprobarlo:** fixture del revisor. PowerGenerator vanilla G con 12 cables `legacy_0`…`legacy_11` hacia 12 consumidores (estado que el loader conserva). H alimenta D. `MaxWiresPerDevice=64`. A pide `G.output_1 -> D.input_main`. Esperado: mensaje `"Connection rejected."`, el almacen de G sigue con 12, H->D sigue, un jugador que se acerque a G **no** recibe una 13ª fila en el blob vanilla. Control: quitar uno de los 12 de G y repetir — el cable nuevo entra, H->D desaparece, el blob de G tiene 12 otra vez incluyendo el nuevo.

### F-02 — CERRADO
- **Que cambie:**
  - Eliminado `FinishWiringCanAdmitAfterReplace` (el helper que solo restaba el mismo puerto de origen).
  - `FinishTxnCanAdmit` (`LFPG_NetworkManagerImpl.c:1148`) resta del recuento del **origen** cada fila del conjunto de conflictos cuyo `m_OwnerId` es ese origen. Ese conjunto incluye incoming al puerto de destino aunque salga de **otro** puerto del mismo aparato (`FinishTxnCollectFromArray` `:1071-1088`: dest match **o** source-port match).
  - El mismo conjunto alimenta la mutacion. No hay una regla de admision y otra de borrado.
  - Tras despegar, `FinishTxnInsertNew` (`:1325`) vuelve a aplicar los topes reales sobre el array ya reducido (LFPG: setting y hardcap 64; vanilla: setting o 64, sin el hardcap encima del setting).
- **Por que asi:** el falso “device is full” nacia de replicar mal la fase de reemplazo. Subir `MaxWiresPerDevice` o exigir un corte previo ocultaria la regresion. Alternativa descartada: pasar el destino al helper viejo y seguir restando en el handler — seguiria habiendo dos sitios que pueden desincronizarse.
- **Que NO cubre:**
  - Incoming de **otro** origen no libera hueco en este origen (correcto). Con `MaxWiresPerDevice=1` y un cable `S.output_2 -> otra_lampara`, mover `S.output_1 -> D` a `S.output_2 -> D` sigue rechazandose: el destino distinto no esta en el conjunto y el total final seria 2.
  - Duplicate exacto que sobreviva al conjunto (misma tripleta en una fila que no se quita) se niega en admit y otra vez en insert.
  - El lock sigue siendo del puerto de **destino**, no del almacen de origen. Dos `FinishWiring` al mismo generador por destinos distintos no estan serializados (ya lo dijo la lane A). El insert post-detach es la ultima puerta de cap.
- **Como comprobarlo:** `MaxWiresPerDevice=1`, splitter S (`output_1..3`), spotlight D. Cable `S.output_1 -> D.input_main`. Moverlo a `S.output_2 -> D.input_main`. Esperado: acepta, un solo cable al final. Control negativo: el mismo S con `output_1 -> D` e intento `output_2 -> otra_cosa` — rechazo “full”. Control: sustituir desde el **mismo** `output_1` hacia D — acepta.

### F-03 — CERRADO
- **Que cambie:**
  - `BroadcastVanillaWires` une a las posiciones de los destinos **actuales** las guardadas en `m_VanillaInvalidationPositions` (`LFPG_NetworkManagerImpl.c:109`, `:929`, consumo en `:3217-3222`). El `rpc.Send` sigue yendo a `pid` (`:3271`), no a `null`.
  - Esas posiciones se registran **antes** de publicar: en `FinishTxnPublish` para cada vanilla quitado (`:1431`) y el destino nuevo del origen vanilla (`:1473`); en `RemoveWiresTargeting` al borrar vanilla (`:2313`); en `HandleCutWires` al vaciar un origen vanilla (`LFPG_RPCServerHandlerImpl.c:991-992` y `:1011-1012`).
  - La cola de FullSync **no** se amplio con vectores. El defer de `BroadcastVanillaWires` (`LFPG_NetworkManagerImpl.c:3139-3151`) retorna **sin** consumir el mapa. `FlushDeferredBroadcasts` vuelve a llamar `BroadcastVanillaWires`, que entonces une y borra las extras. Contrato: la cola sigue siendo (id, obj); las invalidaciones viven hasta el envio real, igual que los destinos de un delta nativo (`:3063-3071`).
- **Por que asi:** el camino nativo ya conservaba destinos de operaciones eliminadas; el vanilla no. Volver a `recipient = null` reabre SEC01. Alternativa descartada: cambiar la firma de `BroadcastVanillaWires` en el facade `4_World` — no es de esta ronda y hay otra entrega sin fusionar. Alternativa descartada: meter vectores en `m_DeferredBroadcastVanillaIds` — cambia el shape de la cola y obliga a tocar el flush en mas sitios.
- **Que NO cubre:**
  - Un observador fuera de `LFPG_CULL_DISTANCE_M + 20` **y** de todos los destinos actuales **y** de los destinos invalidados. Ese era el leak de SEC01; no lo reabro.
  - `SendVanillaWiresTo` (full sync / unicast a un jugador concreto) no usa el mapa de extras: manda el almacen actual a **ese** jugador. F-01 evita que el almacen lleve el fantasma; F-03 cubre a quien ya tenia el cable y deja de estar en el interes **actual**.
  - Destino irresoluble (`FindById` null): no hay posicion que recordar. Igual que el delta nativo.
  - `BroadcastOwnerWires` (snapshot LFPG completo, no delta) no gana extras. El reemplazo LFPG normal usa `BroadcastOwnerWireDelta`, que ya unionaba `deltaWires`. El fallback sin `LFPG_WireOwnerBase` usa snapshot completo; es el mismo hueco preexistente del snapshot, no el unicast vanilla del hallazgo.
  - S-01 (CCTV / `GetPlayers`) sigue siendo sospecha de motor, no cerrado.
- **Como comprobarlo:** terreno llano. G=0, destino viejo T=60, destino nuevo U=-10, actor A=0, observador B=72, carrete, B mira a T. Cable `G -> T` de 60 m (tramos < 50). B lo recibio (12 m de T). A lo sustituye por `G -> U`. Esperado: B a 72 de G y 82 de U **no** esta en el interes nuevo, pero **si** en el de T (12 m < 70, y 72 < 75 del early-out del renderer). Debe recibir el blob vanilla sin el cable viejo. Control: cortar el unico cable vanilla de G con alicates — B junto a T deja de verlo. Control SEC01: un tercer jugador a >70 m de G, U y T no debe recibir el RPC.

---

## LO QUE NO ROMPI

- **SEC02.** Sigue abortando entero **antes** de la transaccion si `AllowCutOthersWires` esta off y hay cable ajeno en origen o incoming de destino (`LFPG_RPCServerHandlerImpl.c:647-665`, mensaje `"Cannot replace another player's wire."`). El criterio sigue siendo `LFPG_WireHelper.CanCreatorCutWire` (`FinishWiringSourcePortHasForeign` `:778`, `FinishWiringDestHasForeignIncoming` `:818`). La transaccion vuelve a mirar el conjunto (`TryCommitFinishWiring` `:971-979`) y, si colara uno, devuelve `DENIED_FOREIGN` sin mutar. No reabro la variante permisiva. Con la flag en true, `m_AllowOthers` viaja en el request y el conjunto se quita entero, como antes.
- **SEC20(a).** La validacion vanilla por `GetPortCount` / `GetPortName` / `GetPortDir` no se ha tocado (`LFPG_RPCServerHandlerImpl.c:464-538`, helper `:723`). Sigue normalizando `""` a `output_1` / `input_main` (o el primer puerto de esa direccion) y rechazando cualquier otra cadena.
- **SEC01.** Cero `Send(..., null)` en los dos ficheros. `.Send(` en `LFPG_NetworkManagerImpl.c` sigue en **9** sitios, todos con `pid` / `sender` concreto (`:2780 :2972 :3119 :3271 :3318 :3443 :3647 :3692 :7026`). F-03 amplia el conjunto de **posiciones de interes**, no el destinatario nulo.

## SALIDA DEL LINTER Y DE LOS GATES

### python enfcheck.py — no arranco (antes ni despues)

Mismo bloqueo que la ronda 1. El hook de PowerShell lo evalua bash y muere antes de lanzar Python:

```
Hook blocked with message: --: eval: line 1: syntax error near unexpected token `&'
--: eval: line 1: `$OutputEncoding = [System.Text.Encoding]::UTF8; Get-Content -LiteralPath '...' -Raw | & { $input | powershell ... launch-ledger.ps1 -Mode Pre }'
```

No invento una salida de `enfcheck.py`. Equivalencia con la herramienta de busqueda del arnes (Grep), mismos predicados que `enfcheck.py` y los gates del brief.

### Gates del brief (despues)

| Gate | Resultado |
|---|---|
| `Send(.*, null)` en los dos ficheros | **0** coincidencias |
| `.Send(` en `LFPG_NetworkManagerImpl.c` | **9** (lista arriba). No anadi envios; F-03 reutiliza el Send vanilla existente |

### Equivalencia enfcheck (antes = ronda 1 ya aplicada; despues = este parche)

Handler `LFPG_RPCServerHandlerImpl.c`:

| Predicado | Antes (ronda 1 / lane A) | Despues |
|---|---|---|
| ternario `[^?]\?[^?:]{1,80}:` | 0 | 0 |
| `++` / `--` | solo comentario searchlight `--` (~linea 1581 entonces; ahora el `--` del comentario de splash sigue en el mismo estilo) | sin `++`/`--` nuevos; el comentario preexistente permanece |
| `+=` / `-=` | 0 | 0 |
| `foreach` | 0 | 0 |
| lineas con `{` / `}` | 359 / 359 (lane A, Grep de lineas) | **332 / 332** (Grep de lineas; baje al sacar el reemplazo del handler) |
| `ref` en locales nuevos | no | no (`finishImpl`, `txnReq`, `txnResult` sin `ref`) |

Manager `LFPG_NetworkManagerImpl.c`:

| Predicado | Antes (lane B) | Despues |
|---|---|---|
| ternario | 0 | 0 |
| `++` / `--` | solo reglas `---` de comentarios | igual; el parche no anade `++`/`--` |
| `+=` / `-=` | 10 del scheduler `:624-689` | **10** (siguen siendo solo el scheduler) |
| `foreach` | 0 | 0 |
| `.Send(` | 9 | **9** |
| `noExclude` / `Send(.*, null)` | 0 | 0 |
| lineas con `{` / `}` | 795 / 795 | **876 / 876** (Grep de lineas; subi al meter la transaccion) |

`LFPG_FinishWiringTxn.c`: ternarios / `++` / `+=` / `foreach` / escapes adyacentes = 0. `{`/`}` lineas = 3/3. `ref` solo en miembros `m_Wire`.

Escapes adyacentes: el parche no anade literales con `\\`. El `$profile:LF_PowerGrid\\vanilla_wires.json` preexistente del manager no se toco.

**Nota:** Grep cuenta **lineas** que contienen el signo, no caracteres. `enfcheck.py` cuenta `src.count("{")`. El brief avisa que el handler ya tenia `delta=-2` de parentesis en comentarios en la linea base. No tengo esa cifra de caracteres porque el linter no corrio.

## LO QUE NO PUDE VERIFICAR

- Compilacion Enforce. AddonBuilder/juego prohibidos.
- `python enfcheck.py` (hook).
- Los tres experimentos in-game (dos clientes, fixture de 12 aristas legacy, `MaxWiresPerDevice=1`).
- Semantica nativa de `GetPlayers()` / `GetIdentity()` durante CCTV (S-01).
- Que `map.Find` rellene un local **sin** `ref` para `m_ReverseOwners` y `m_VanillaInvalidationPositions`. El resto del fichero usa `ref array` locales en esos Find; yo no anadi `ref` en locales nuevos (convencion). Si Find no rellenara el local, el collect de incoming **ajeno** por indice inverso quedaria vacio — el origen sigue recorriendo **su** array (cubre F-02). F-03 recordaria extras vacias si Find del mapa de vectores fallara: el observer del destino viejo no recibiria. Hay que mirar el primer boot.
- Coste del scan: el conjunto de conflictos usa el reverse index igual que `RemoveWiresTargeting`, no el barrido total de `FinishWiringDestHasForeignIncoming` (ese barrido sigue solo en SEC02).

## LA PREMISA DE ESTE ENCARGO

- **Si, “transaccion almacen+grafo” es el contrato correcto.** Guardar no es completar la conexion. El full sync vanilla serializa el almacen; no difundir *ese* add no basta. El revisor no se equivoco de frontera. La ronda 1 fallo porque el veto por fichero impedia exactamente esta operacion.
- **Revertir no es peor que el estado a medias en los caminos de F-01/F-02.** Dejar el cable rechazado persistible y romper H->D es perdida de conexion real. Reponer las filas (los mismos objetos `LFPG_WireData`, no clones) deja el mundo como antes del intento. El unico caso donde revertir “falla peor” seria que la restauracion del grafo no reinsertara una arista y un rebuild no corriera nunca: entonces habria store con el cable viejo y grafo sin el, hasta el proximo heal. Eso ya era posible antes; ahora el almacen **no** lleva el rechazado.
- **No me has vuelto a vetar el manager.** El facade `scripts/4_World/LFPG_NetworkManager.c` **no** esta en la lista de dueño. No lo toque: metodos nuevos en `LFPG_NetworkManagerImpl` y `LFPG_NetworkManagerImpl.Cast(Get())`, el mismo idioma que `Sessions()`. Si el revisor exige la firma en el facade, eso **si** es un veto residual: sin el, un fallback inerte de menu no tiene `TryCommitFinishWiring` y el handler aborta con error (fail-closed, `:668-673`). Para un servidor con mission factory no cambia nada.
- No cambie el formato en disco ni purgue filas `legacy_*`. Eso iba a `REPORT.md` como propuesta: una pasada de validacion/reparacion de puertos vanilla contra `GetPortName` en load, **opt-in**, no silenciosa.

## VISTO-DE-PASO

- `m_GraphFullRebuildRequired` se enciende en un add de grafo fallido aunque hayamos restaurado el almacen. Rebuild extra posible; no lo “apague” a ciegas.
- `RemoveWiresTargeting` compara `m_TargetPort` literal; SEC02 usa `IncomingPortIndexKey`. Sigue el hueco de incoming legacy con puerto vacio vs `input_main`. El conjunto de la transaccion usa la misma literalidad que la mutacion vieja, a proposito. SEC02 sigue abortando por la clave normalizada (mas restrictivo).
- `ref` en locales de Find/GetVanillaWires preexistentes. No los “limpie”.
- Scheduler `+=` preexistente (10). `Print` de perfdiag preexistente.
- S-01 CCTV: no medido, no cerrado.
- `HandleFinishWiring` ya no hace `MarkVanillaDirty`/`FlushVanillaIfDirty` en el fallo de store: el rollback deja la memoria como estaba; persistir el borrado era el defecto.
