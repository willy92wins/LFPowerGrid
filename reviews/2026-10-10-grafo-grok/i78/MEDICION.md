# MEDICION R-01, R-02, R-04, R-05 (issue #78)

Como activar: en `scripts/3_Game/LFPG_Defines.c` poner `LFPG_PERF_PROBE = true` en un servidor de debug. No hay RPC ni setting persistido. Lineas RPT: `LFPG_PERF` con `clave=valor`.

Contrato O(n) de `map.GetElement` / `map.GetKey`: `P:\scripts\1_core\proto\enscript.c:861` y `:871` ("This operation is O(n) complexity").

`ProcessDirtyQueue` ya mide `g_Game.GetTime()` en `m_LastProcessMs` y `m_EdgesVisitedThisEpoch` (`scripts/5_Mission/LFPG_ElecGraphImpl.c` cerca de `startMs` / `elapsed`). El probe reutiliza esos valores en `OnProcessDirtyQueue`.

---

## R-01 — reconstruccion tras CutAll

**Escenario.** 1 generador, 1 splitter, 8 consumidores. Cables OUT del generador y del splitter, mas 2 cables IN hacia un consumidor (reverse index). Jugador con alicates.

**Accion.** CutAll sobre el splitter (`HandleCutWires`). Repetir sobre el generador. Repetir en un grafo de ~40 dispositivos / ~60 cables.

**Que leer.** `LFPG_PERF event=1 route=R01` (`total_ms`, `phase_index_ms`, `phase_rescue_ms`, `phase_rebuild_ms`, `allocs`) y luego `kind=settle settle_ticks=` tras vaciar la cola.

**Umbral de refactor.** Si `phase_rebuild_ms` (PostBulkRebuildAndPropagate + Populate + MarkSourcesDirty) crece con el grafo entero y no con los cables cortados, o `settle_ticks` >> 1 con cola llena de nodos fuera de la isla, merece evaluar rebuild por isla. T5-03 permite un rebuild por cascada: un solo evento lento no basta.

**Refactor que plantea el issue (NO implementado).** Reparacion por isla y agrupacion de eventos, conservando recuperacion completa.

**Riesgo.** Perder wires/dispositivos fuera de la isla o dejar SyncVars stale en huerfanos (`PostBulkRebuild` existe precisamente por eso).

---

## R-02 — upstream repetido

**Escenario.** Cadena SOURCE → PT → PT → N consumidores. Cambiar consumo de 4 consumidores en el mismo tick (o en ticks consecutivos antes de settlement).

**Accion.** Forzar `RefreshSourceState` / cambio de consumo en varios nodos que llaman `MarkUpstreamNodesDirty`.

**Que leer.** `route=R02` `phase_walk_ms`, `nodes`, `requeues`, `allocs` (2 por llamada: array+map). Varios `event=1 route=R02` antes de un `kind=settle`.

**Umbral de refactor.** Si K cambios solapados producen K BFS con `nodes` casi iguales y `allocs=2*K`, la union de raices se justifica. Si `settle_ticks` sube porque se retrasan apagados, no agrupar.

**Refactor del issue (NO implementado).** Union de raices e invalidacion por version, sin retrasar apagados.

**Riesgo.** Apagados tardios o demand stale en PASSTHROUGH.

---

## R-04 — scan global al cortar un puerto IN

**Escenario.** Indice reverse sano. Un consumidor con 1 cable IN indexado. Segundo caso: indice parcial (owner no listado).

**Accion.** CutPort IN. Comparar `removed` vs `rescued` en logs existentes mas `route=R04` fases `index` / `rescue` / `rebuild`.

**Umbral de refactor.** Si con indice sano `phase_rescue_ms` + rebuild completo dominan y `rescued=0` de forma estable, evaluar omitir scan. No omitir solo por un hit: hace falta completitud de owners y el negativo de indice parcial.

**Refactor del issue (NO implementado).** No escanear en caliente si el indice esta transaccionalmente sano.

**Riesgo.** Cables IN huerfanos si el indice miente; el comentario en `HandleCutPort` lo deja explicito.

---

## R-05 — enumeracion indexada de maps

**Escenario.** Grafo de 20 / 50 / 100 nodos. Un CutAll (dispara rebuild + populate + MarkSourcesDirty) y un periodo idle (ValidateConsumerStates).

**Que leer.** `LFPG_PERF event=0 kind=sweep map= map_size= index_calls=` y el `map_index` / `map_sweeps` del evento. Coste agregado ~ suma `index_calls * map_size` (cada GetElement/GetKey es O(n)).

**Umbral de refactor.** Si un tick de servidor gasta decenas de barridos O(n) sobre `m_Nodes` del mismo tamano (rebuild+populate+mark sources+validate) y el tiempo de evento escala quadratico con N, evaluar registro de IDs. La #71 ya lo evita en el tick de cargadores (fuera de esta lane).

**Refactor del issue (NO implementado).** Registros de IDs coherentes en todo el lifecycle.

**Riesgo.** Listas de IDs desincronizadas al borrar nodos (el mismo fallo que el reverse index).

### Inventario estatico (bucles GetElement/GetKey)

| sitio | map | llamadas por pasada | tamano esperado | frecuencia |
|---|---|---|---|---|
| `LFPG_ElecGraphImpl.c` prune RebuildFromWires (`GetKey` en bucle `m_Nodes`) | `m_Nodes` | N GetKey | nodos del grafo | cada rebuild de wires |
| `LFPG_ElecGraphImpl.c` split watchdog (`m_WdgVisited.GetKey`) | `m_WdgVisited` | |visitados| GetKey | componente | al quitar arista que parte componente |
| `LFPG_ElecGraphImpl.c` RebuildComponents reset | `m_Nodes` | N GetElement | N | si `m_ComponentsDirty` |
| `LFPG_ElecGraphImpl.c` RebuildComponents BFS seed | `m_Nodes` | N GetElement + 1 GetKey/componente | N | idem |
| `LFPG_ElecGraphImpl.c` GetOverloadedSourceCount | `m_Nodes` | N GetElement | N | consulta |
| `LFPG_ElecGraphImpl.c` PostBulkRebuild snapshot | `m_Nodes` | N GetKey + N GetElement | N | CutAll / CutPort changed |
| `LFPG_ElecGraphImpl.c` MarkComponentDirty | `m_Nodes` | N GetElement + GetKey en hits | N | suciedad por componente |
| `LFPG_ElecGraphImpl.c` MarkSourcesDirty | `m_Nodes` | N GetElement + GetKey en sources/PT | N | post rebuild |
| `LFPG_ElecGraphImpl.c` ValidateConsumerStates | `m_Nodes` | 2 * batch (GetKey+GetElement) | N, batch `LFPG_VALIDATE_BATCH_SIZE` | al drenar cola / idle |
| `LFPG_ElecGraphImpl.c` PopulateAllNodeElecStates | `m_Nodes` | 2N | N | post bulk rebuild |
| `LFPG_NetworkManagerImpl.c` rate limiter stale | `m_RateByPlayer` | 2 * count | jugadores recientes | tick de limpieza |
| `LFPG_NetworkManagerImpl.c` GetVanillaWiresByIndex | `m_VanillaWires` | 1 GetKey | owners vanilla | acceso indexado |
| `LFPG_NetworkManagerImpl.c` battery rebuild gen | `m_BatteryRebuildGeneration` | GetKey+GetElement | baterias | rebuild |
| `LFPG_NetworkManagerImpl.c` CutPendingPowerOff | `m_CutPendingPowerOff` | GetKey | pendientes | corte |
| `LFPG_NetworkManagerImpl.c` vanilla rebuild/broadcast | `m_VanillaWires` | GetKey+GetElement por owner | owners vanilla | rebuild / flush |
| `LFPG_NetworkManagerImpl.c` pending broadcasts | `m_PendingBroadcastLFPG`, `m_PendingOwnerSnapshots`, `m_PendingBroadcastVanilla` | GetElement / GetKey | cola de broadcast | tick |
| `LFPG_NetworkManagerImpl.c` full/deferred sync | `m_VanillaWires`, `m_DeferredOwnerSnapshots` | indexado | owners | sync |
| `LFPG_NetworkManagerImpl.c` reverse idx walk | `m_ReverseIdx` | GetKey | claves reverse | rebuild reverse |
| `LFPG_NetworkManagerImpl.c` validation vanilla ids | `m_VanillaWires` | GetKey | owners | validacion startup |
| `LFPG_NetworkManagerImpl.c` last known pos | `m_LastKnownPos` | GetKey | owners | tracking |
| `LFPG_NetworkManagerImpl.c` graphIncomingByPort | `graphIncomingByPort` | GetKey+GetElement | puertos | coherencia |
| `LFPG_BalanceProvider_NativeImpl.c` refund / persist | `refundByUid`, `s_Balances` | GetKey+GetElement | jugadores | persist / refund |
| `LFPG_ControlSessionRegistry.c` sessions | `m_ByUID` | GetElement / GetKey | sesiones CCTV | tick / listado |
| `LFPG_Util.c` PurgeStaleWarnRateLimits | `s_WarnRateLimits` | GetElement+GetKey | claves warn | purge |
| `LFPG_DeviceRegistry.c` GetAll / purge | `m_ById`, `m_AmbiguousIds` | GetElement/GetKey | dispositivos | GetAll, GC |
| `LFPG_CableRenderer.c` (cliente, varios) | `m_ByOwnerId`, `m_WireSegments`, `m_NegCache`, retry maps | GetElement/GetKey por frame o por owner | segmentos visibles | **frame client** — fuera del grafo server; no instrumentado aqui |
| `LFPG_LaserBeamRenderer.c` | `m_Detectors`, `m_ProjCache` | GetKey | detectores | cliente |

Instrumentado en esta ronda: solo barridos de `LFPG_ElecGraphImpl.c` (lista blanca). El resto queda en inventario para PRs posteriores.

---

## Como NO cuenta como optimizacion

Mover trabajo a otro modulo o tick, o retirar funciones, no cuenta. La medida es tiempo completo por evento y por fase, visitas, requeues, allocations y ticks hasta cola vacia.
