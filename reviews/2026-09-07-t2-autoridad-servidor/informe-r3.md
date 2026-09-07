# REPORT — T2 ronda 3: cerrar el bucle

Arbol: worktree `fix/t2-r2-transaccion`. Cambios sin commit.

Ficheros tocados:
- `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`
- `scripts/5_Mission/LFPG_NetworkManagerImpl.c`
- `scripts/5_Mission/LFPG_FinishWiringTxn.c`
- `scripts/5_Mission/LFPG_ElecGraphImpl.c` (ampliacion del brief para N-01)

Facade `scripts/4_World/LFPG_NetworkManager.c`: no tocado.

---

| # | condicion | ALCANZADO / NO / PARCIAL | evidencia (path:line) |
|---|---|---|---|
| V1 | N-01 cerrado | ALCANZADO | `LFPG_ElecGraphImpl.c:612-616` (cap de arista antes de `EnsureNode`); `:1379-1384` / `:1532-1546` (si el nodo se creo, `DiscardUnusedNodes` lo anota; `EndGraphMutation` poda si sigue sin aristas) |
| V2 | N-02 cerrado | ALCANZADO | `LFPG_NetworkManagerImpl.c:1340-1353` quita filas **antes** de `:1359-1369` `NotifyGraphWireRemoved` → `LFPG_RefreshPumpSprinklerLink` `:6042` lee `LFPG_GetWires()` `:6084` / `:6089` y reactiva solo lo que sigue en el array `:6104-6128` |
| V3 | N-03 cerrado | ALCANZADO | Envio: `:3323`. Cancelacion: `ClearVanillaInvalidation` `:954-960` en cola nula `:2702-2705`, defer muerto `:3611-3613`, owner no publicable `:1616-1618` / `:2416-2418`, lifecycle `:4206`, prune vacio `:4987` |
| V4 | Residuo F-01 cerrado | ALCANZADO | Snapshot de la secuencia completa `:1262-1287`, restore `Clear`+replay `:1290-1310` / `:1373-1381`. No `Insert` al final |
| V5 | Residuo F-03 cerrado | ALCANZADO | `HandleCutPort` OUT vanilla `:1874-1875`. Fallback incoming vanilla `:2017-2018` |
| V6 | Cero GRAVE nuevo | ALCANZADO | La transaccion sigue publicando solo en `OK` (`:1019-1021`). No hay rebuild sobre la fila rechazada. Las notificaciones de retirada ya no leen el store pre-detach |
| V7 | No se rompe lo que ya funciona | ALCANZADO | F-02 `:1187-1189`. SEC02 handler `:649-664` + txn `:981-989`. SEC20(a) `:723`. SEC01: 9 `.Send(` con pid, 0 `Send(..., null)` |

---

## N-01

- **Que cambie:**
  - `OnWireAdded` consulta el tope de aristas por nodo **antes** de `EnsureNode` (`LFPG_ElecGraphImpl.c:612-616`, helper `:1321-1340`). El escenario del revisor (G con 12 salidas, D sin nodo, 2.047 nodos globales) muere ahi: D no entra en `m_Nodes`.
  - Si `AddEdgeInternal` aun crea un nodo y luego rechaza (rebuild, duplicado, destino sin registro tras haber creado el origen), llama `DiscardUnusedNodes` (`:1370`, `:1383`, `:1395`, `:1412`). En mutacion lo mete en `m_DeferredOrphanCleanup` (`:1535-1540`); `EndGraphMutation` ya recorre esa cola. `CleanupOrphanNode` solo borra si no hay aristas, asi G con 12 salidas se queda y D huerfano no.
  - Tras `DENIED_GRAPH`, `TryCommitFinishWiring` sigue haciendo `FinishTxnUninsertNew` + restore + `EndGraphMutation` (`LFPG_NetworkManagerImpl.c:1011-1014`). La poda del huerfano corre **despues** de reponer las aristas viejas.

- **Por que asi:** la hipotesis del orquestador (rebuild tras rollback, ahora que la fila rechazada ya no esta) es **cierta** como mecanismo, y el flag `m_GraphFullRebuildRequired` en el fallo de arista (`:1708-1709`) se conserva como red de seguridad. No la uso como arreglo primario: un `PostBulkRebuildAndPropagate` en cada denegacion reescribe SyncVars de toda la red y es justo el camino que ronda 2 quito para no republicar un fantasma. Impedir el `EnsureNode` y revertir los nodos creados cubre el control del revisor (2.047 → 2.047, D sin nodo, un cableado independiente sigue pasando) sin reconstruir.

- **Que NO cubre:**
  - `RebuildFromWires` sigue haciendo `EnsureNode` de todos los extremos del store **antes** de insertar aristas. Una 13ª fila legacy sigue sin arista; ahora `DiscardUnusedNodes` puede borrar su nodo si no gano ninguna. Eso alinea grafo y admision; no reescribe el loader ni el formato persistente.
  - El tope global 2.048 **antes** de `EnsureNode` ya existia (`OnWireAdded` PASO 1). N-01 cruzaba el umbral **dentro** del intento; ese PASO 1 no lo cazaba y no lo toco.
  - No hay API publica de recuento de nodos en el facade. El control in-game es operacional (un segundo cableado admisible), no un numero en HUD.

- **Como comprobarlo:** servidor con `MaxWiresPerDevice` alto. Un generador G con 12 cables en 12 OUT distintos (tope de aristas). Un consumidor D **nuevo**, registrado, cerca, sin nodo. Pedir `G.output_1 → D.input_main` (G no tiene fila en `output_1`). Esperado: rechazo, G sigue con 12, D no alimentado. Inmediatamente cablear dos dispositivos **distintos** ya en el grafo, con hueco de aristas: debe aceptar. Control negativo del revisor: no hace falta entrar ya en 2.048; el fallo es crear D y quedarse a 2.048.

## N-02

- **Que cambie:** `FinishTxnDetachConflicts` ahora (1) snapshot, (2) **borra las filas del store**, (3) **despues** `NotifyGraphWireRemoved` + indice + cuota (`LFPG_NetworkManagerImpl.c:1340-1369`). `LFPG_RefreshPumpSprinklerLink` (`:6042`) lee `LFPG_GetWires()` (`:6084` / `:6089`) y en el pass 1 (`:6104-6128`) reactiva solo destinos que siguen en ese array. Con el lote B-luego-A, al notificar A el store ya no contiene B, asi que no lo reactiva.

- **Por que asi:** el revisor pidio no leer un store transitorio como definitivo. Alternativa descartada: diferir el refresh de bombas a un flag en `NotifyGraphWireRemoved` — el facade del grafo no esta en alcance para anadir ese parametro, y el arreglo correcto es el orden store→notificacion, no silenciar el refresh. Alternativa descartada: notificar y borrar por fila como ronda 1 — reintroduce el acoplamiento OUT-antes-que-IN que la transaccion unifico.

- **Que NO cubre:**
  - El tick de 60 s (`:6136`) sigue apagando todos los aspersores y reactivando desde el store. Es red de seguridad, no el camino del reemplazo.
  - Otros consumidores que rescanean el store desde `NotifyGraphWireRemoved` (si los hay fuera de bomba/aspersor) quedan cubiertos por el mismo orden; no audite cada `Cast` del manager.
  - En el **exito**, el aspersor del cable **nuevo** se activa en `NotifyGraphWireAdded` (`:1716-1718`) sobre remaining+nuevo. Eso es el estado comprometido, no un residuo de B.

- **Como comprobarlo:** bomba T2 P alimentada, aspersores A y B, mismo creador, `AllowCutOthersWires=false`, `MaxWiresPerDevice>=2`. Crear `P.output_2 → B.input_0`, luego `P.output_1 → A.input_0`. Pedir `P.output_2 → A.input_0`. Esperado: A queda activo (nuevo cable), **B inactivo y sin fuente**. Control: el mismo reemplazo en ronda 2 dejaba B activo; ahora no. No esperar al tick de 60 s.

## N-03

- **Que cambie:**
  - `ClearVanillaInvalidation` (`LFPG_NetworkManagerImpl.c:954-960`) borra la entrada del mapa.
  - **Envio:** `BroadcastVanillaWires` sigue consumiendo en `:3323` (incluso sin destinatario elegible; eso no es otro bug, el revisor lo dijo).
  - **Diferimiento vivo:** el defer `:3148+` sigue **sin** consumir; `FlushDeferredBroadcasts` vuelve a `BroadcastVanillaWires` (`:3607-3609`).
  - **Cancelacion:** owner nulo en `FlushBroadcasts` (`:2702-2705`); owner nulo/id vacio en flush diferido (`:3611-3613`); vanilla ajeno no resoluble en publish (`:1616-1618`); `RemoveWiresTargeting` sin `vObj` (`:2416-2418`); device lifecycle (`:4206`); owners vacios del prune (`:4987`); fallback CutAll sin owner (`:5792-5794`); fallback incoming del handler sin owner (`LFPG_RPCServerHandlerImpl.c:2034-2036`).

- **Por que asi:** el mapa es auxiliar de un broadcast. Si no hay objeto que publicar, no hay envio posible (hace falta NetworkID). Cancelar es el unico cierre honesto. Alternativa descartada: enviar un blob huerfano sin owner — reabre SEC01 o inventa un destinatario.

- **Que NO cubre:**
  - `SendVanillaWiresTo` (unicast / full sync a un jugador) sigue sin extras: manda el store actual a **ese** jugador. Un observador de un owner **destruido** no recibe invalidacion; el mapa ya no retiene la clave, pero el ghost del cliente vive hasta que salga de rango o reconecte. No hay destinatario publicable.
  - `RecordVanillaInvalidationTarget` sigue sin guardar nada si `FindById(target)` falla (`:939-941`). Igual que el delta nativo.
  - No medí bytes ni tasa de owners destruidos durante FullSync. El camino sin liberacion del modelo (1.000 claves) queda cerrado en codigo; no es prueba de heap.

- **Como comprobarlo:** no hay un contador in-game del mapa. Proxy: cortar un vanilla durante FullSync de otro jugador, **borrar el generador** antes de que acabe el flush, y repetir con IDs distintos muchas veces en la misma mision. Esperado: no hay crecimiento de trabajo por broadcast a owners muertos (el flush ya no retiene la clave). Control: el mismo corte con el generador vivo debe seguir llegando al observador junto al extremo antiguo (F-03).

## F-01 residual

- **Que cambie:**
  - `LFPG_FinishWiringStoreSnapshot` (`LFPG_FinishWiringTxn.c:37-42`) guarda la secuencia completa de cada store afectado, no solo `m_Index`.
  - Captura **antes** de borrar filas (`LFPG_NetworkManagerImpl.c:1340`, copia `:1279-1285`).
  - Restore: `live.Clear()` y reinserta esa secuencia (`:1303-1310`), luego indice/cuota/aristas (`:1394-1399`). Ya no hay `live.Insert(wd)` al final.

- **Por que asi:** devolver una fila a `m_Index` no basta si `Remove` compacta o intercambia con la ultima. El revisor reprodujo el fallo con los dos semanticas. Reponer el array entero es independiente de `Remove`. Alternativa descartada: `InsertAt` + ordenar por `m_Index` — sigue dependiendo de como quedaron las filas que no se tocaron. `m_Index` se sigue rellenando (`FinishTxnAppendConflict`); el restore no lo usa.

- **Que NO cubre:**
  - El snapshot guarda **referencias** a los mismos `LFPG_WireData`, no un deep copy. Nadie mas debe mutar esas filas durante la transaccion (un solo RPC, un hilo).
  - Si el owner LFPG desaparece y `GetDeviceWires` devuelve null, no hay array que reponer. Se restauran indice/grafo igual que antes; el store no. No es el fixture del revisor.
  - Datos legacy `legacy_0..11` siguen en disco. El restore conserva su orden; no los borra ni cambia el formato persistente.
  - Un rebuild **posterior** (flag de arista fallida, corte, self-heal) admite en el orden restaurado. No fuerzo un rebuild en el rollback.

- **Como comprobarlo:** el fixture residual del revisor. G vanilla con 13 filas: `output_1 → T.input_main` **primera**, luego 12 `legacy_0..legacy_11` a otros consumidores. D con 12 incoming `legacy_in_*` (ninguno `input_main`). `MaxWiresPerDevice=64`. Pedir `G.output_1 → D.input_main`. Esperado: rechazo; G sigue con las 13 filas **en ese orden**; `G→T` sigue alimentando. Disparar un rebuild (cortar y recablear **otro** aparato, o reiniciar mision con el mismo JSON). Esperado: `G→T` sigue admitido, no la 13ª legacy. Control original de F-01: 12 filas en G y H→D — el nuevo no aparece en el blob, H→D vuelve.

## F-03 residual

- **Que cambie:**
  - `HandleCutPort` OUT vanilla registra el destino **antes** de borrar (`LFPG_RPCServerHandlerImpl.c:1874-1875`) y sigue haciendo `BroadcastVanillaWires` (`:1888`).
  - `RescueStaleIncomingWires` vanilla registra (`:2017-2018`); si el owner no existe, cancela (`:2034-2036`).
  - De paso, los productores CutAll vanilla (`LFPG_NetworkManagerImpl.c:5660`) y fallback CutAll (`:5773` / cancel `:5792-5794`) quedan en el mismo contrato envio-o-cancelacion. No eran V5; el principio de la §4 los incluye.

- **Por que asi:** el corte completo y el reemplazo ya registraban. Faltaban estos dos del dictamen. Alternativa descartada: `Send(..., null)` — SEC01.

- **Que NO cubre:**
  - Corte OUT **LFPG** sigue yendo por `BroadcastOwnerWireDelta` con las filas quitadas; el nativo ya unia destinos eliminados. V5 es el vanilla del escenario G→T.
  - Destino irresoluble: no hay posicion. Observador a >70 m de owner **y** de todos los extras: no recibe (techo SEC01).
  - `SendVanillaWiresTo` sin extras, igual que ronda 2.

- **Como comprobarlo:** G=0, T=60, actor=0, observador B=72, carrete, B mira a T, cable G→T ya recibido, tramos <50 m. Cortar `output_1` OUT con alicates en G. Esperado: B (12 m de T, 72 m de G) recibe el blob vanilla **sin** el cable. Control: corte completo del mismo G (`HandleCutWires`) — B tambien lo pierde. Control SEC01: un tercero a >70 m de G y de T no recibe el RPC.

---

## LO QUE NO ROMPI

- **F-02.** `FinishTxnCanAdmit` sigue restando del origen cada conflicto cuyo `m_OwnerId` es ese origen (`LFPG_NetworkManagerImpl.c:1187-1189`). El conjunto unico no se ha partido. `FinishTxnInsertNew` vuelve a aplicar los topes sobre el array ya reducido. No toque la recogida OUT-o-destino ni el dedup por puntero (`FinishTxnAlreadyCollected`).

- **SEC01.** `Send(.*, null)` = **0** en los dos ficheros del gate (y en ElecGraph / FinishWiringTxn). `.Send(` en `LFPG_NetworkManagerImpl.c` = **9**, todos con `pid` / identity: `:2872 :3064 :3211 :3363 :3410 :3535 :3743 :3788 :7132`. F-03 sigue ampliando posiciones, no el destinatario.

- **SEC02.** Aborto restrictivo **antes** de la transaccion (`LFPG_RPCServerHandlerImpl.c:649-664`, mensaje `"Cannot replace another player's wire."`). La transaccion relee el conjunto (`LFPG_NetworkManagerImpl.c:981-989`) y puede devolver `DENIED_FOREIGN` sin haber llamado `BeginGraphMutation`. No reabro la variante permisiva. Con `AllowCutOthersWires=true`, `m_AllowOthers` viaja en el request igual que en ronda 2.

- **SEC20(a).** `FinishWiringVanillaPortAllowed` / canonico no se han tocado (`LFPG_RPCServerHandlerImpl.c:723`, usos `:479 :514`). Normalizacion de `""` y rechazo de nombres que no salen de `GetPortCount`/`GetPortName`/`GetPortDir` siguen iguales.

## SALIDA DEL LINTER Y DE LOS GATES

### python enfcheck.py — no arranco

Mismo bloqueo de ronda 2. El hook de PowerShell lo evalua bash y muere antes de lanzar Python:

```
Hook blocked with message: --: eval: line 1: syntax error near unexpected token '&'
```

No invento una salida de `enfcheck.py`. Equivalencia con Grep del arnes.

### Gates del brief (despues de editar)

| Gate | Resultado |
|---|---|
| `Send(.*, null)` en H y N (tambien txn + ElecGraph) | **0** coincidencias |
| `.Send(` en `LFPG_NetworkManagerImpl.c` | **9** |
| `foreach` en los cuatro ficheros tocados | **0** (codigo) |
| ternarios `? :` en codigo nuevo | **0** (el `"?` de diagnostico en ElecGraph es un string preexistente) |
| `++` / `--` / `+=` / `-=` en codigo nuevo | **0**. Los `+=` del scheduler (`:629-694`, diez) y los `--` de comentarios `---` son los de siempre |

Escapes adyacentes: el unico `\\` del manager en esos ficheros sigue siendo el path preexistente `$profile:LF_PowerGrid\\vanilla_wires.json`. Las cadenas nuevas no llevan barras.

## LO QUE NO PUDE VERIFICAR

- Compilacion Enforce y runtime DayZ. No lance el juego, AddonBuilder ni builds.
- El recuento real de `m_NodeCount` tras el rechazo N-01. El control in-game de V1 es el segundo cableado admisible, no un print del tope.
- Semantica exacta de `array.Remove` (compacta vs swap-con-ultimo). Por eso el restore no depende de ella.
- S-01 / CCTV: no lo toque. Sigue aparte.
- Que `ref` en locales de `EdgeCapacityReached` (`existOut` / `existIn`) compile igual que el resto de `Find` del mismo fichero. Es el tipo del mapa, no un `ref` nuevo en la transaccion. Si el diag STRICT lo rechazara, el arreglo es quitar el `ref` exterior del local; no lo he podido comprobar.

## LA PREMISA DE ESTE ENCARGO

- **El VERDE de la §1 es el listado correcto.** No me falta una octava condicion para cerrar T2. S-01 sigue siendo incertidumbre de motor y no debe entrar en este liston. Un recuento instrumentado de `m_VanillaInvalidationPositions` habria hecho V3 mas barato de auditar; no es necesario para el contrato.
- **Ningun punto del VERDE exige el facade ni cambiar el formato persistente.** `RecordVanillaInvalidationTarget` / `ClearVanillaInvalidation` viven en la impl; el handler ya casteaba para F-03. El grafo se arreglo **dentro** de `LFPG_ElecGraphImpl.c`, que esta ronda autoriza. Si el revisor exigiera `GetNodeCount()` en el facade para V1, eso seria instrumentacion, no el cierre.
- **Por que dos rondas cambiaron defectos por otros:** la frontera «almacen + grafo, publicar solo si ambos admiten» era correcta y se queda. La implementacion de ronda 2 cubria filas y aristas y dejo fuera tres canales que **leen** ese intento: el orden del array en el proximo rebuild, el rescan de bombas sobre el store, y el mapa de invalidacion. Cada ronda cerro el hallazgo nombrado moviendo un helper; los consumidores del estado transitorio no estaban en el contrato escrito. Eso no pide un rediseno. Un commit funcional (una foto nueva de stores+grafo aplicada una vez, sin notificar a media mutacion) seria mas robusto ante un **cuarto** canal, y seria una respuesta valida si aparece. Con los tres canales del dictamen metidos en la transaccion, esta pasada es el cierre de T2, no una cuarta ronda.

## VISTO-DE-PASO

- `m_Index` en `LFPG_FinishWiringRemovedWire` queda escrito y sin lector en el restore. No lo borro.
- Dos implementaciones del tope de store (admit vs insert) siguen. El revisor lo dejo como riesgo de mantenimiento, no como F-02. No las unifique.
- `RebuildFromWires` + `DiscardUnusedNodes` puede podar nodos de filas no admitidas en un rebuild de datos legacy. Alineacion store/grafo; no es un cambio de persistencia.
- `ClearVanillaInvalidation` es publico en la impl, no en el facade, igual que `RecordVanillaInvalidationTarget`.
