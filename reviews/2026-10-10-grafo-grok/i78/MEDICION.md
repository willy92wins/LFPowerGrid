# MEDICION R-01, R-02, R-04, R-05 (issue #78)

Como activar: hace falta un **build propio** con `LFPG_PERF_PROBE = true` en `scripts/3_Game/LFPG_Defines.c`, empaquetado y desplegado en un **servidor de pruebas**. Cliente y servidor tienen que cargar **el mismo PBO**. No hay RPC ni setting persistido: la constante se resuelve en compile del script. Lineas RPT: `LFPG_PERF` con `clave=valor`.

Contrato O(n) de `map.GetElement` / `map.GetKey`: `P:\scripts\1_core\proto\enscript.c:861` y `:871` ("This operation is O(n) complexity").

`ProcessDirtyQueue` ya mide `g_Game.GetTime()` en `m_LastProcessMs` y `m_EdgesVisitedThisEpoch` (`scripts/5_Mission/LFPG_ElecGraphImpl.c` cerca de `startMs` / `elapsed`). El probe reutiliza esos valores en `OnProcessDirtyQueue`.

---

## R-01 — reconstruccion tras CutAll

**Escenario.** 1 generador, 1 splitter, 8 consumidores. Cables OUT del generador y del splitter, mas 2 cables IN hacia un consumidor (reverse index). Jugador con alicates.

**Accion.** CutAll sobre el splitter (`HandleCutWires`). Repetir sobre el generador. Repetir en un grafo de ~40 dispositivos / ~60 cables.

**Que leer.** `LFPG_PERF event=1 route=R01` (`total_ms`, `phase_index_ms`, `phase_rescue_ms`, `phase_rebuild_ms`, `nested`) y luego `kind=settle settle_ticks=` tras vaciar la cola. No uses `allocs` para decidir: solo cuenta los `new` de deltas en HandleCutWires (2) y los tres arrays de RescueStaleIncomingWires (3), no `PostBulkRebuildAndPropagate` / `RebuildFromWires` / Populate.

**Umbral de refactor.** Si `phase_rebuild_ms` (PostBulkRebuildAndPropagate + Populate + MarkSourcesDirty) crece con el grafo entero y no con los cables cortados, o `settle_ticks` >> 1 con cola llena de nodos fuera de la isla, merece evaluar rebuild por isla. T5-03 permite un rebuild por cascada: un solo evento lento no basta. El umbral **no** se apoya en `allocs`.

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

**Que leer.** `LFPG_PERF event=0 kind=sweep map= map_size= index_calls=` y el `map_index` / `map_sweeps` del evento.

`index_calls` **no es una cuenta medida por iteracion**. Es una **cota estatica** calculada antes del bucle (`Count()` o `Count() * 2`), salvo ValidateConsumerStates, que usa `checked * 2` al terminar el batch. Coste agregado estimado: suma `index_calls * map_size` (cada GetElement/GetKey es O(n) segun `enscript.c:861/871`).

**Umbral de refactor.** Si un tick de servidor gasta decenas de barridos O(n) sobre `m_Nodes` del mismo tamano (rebuild+populate+mark sources+validate) y el tiempo de evento escala quadratico con N, evaluar registro de IDs. La #71 ya lo evita en el tick de cargadores (fuera de esta lane).

**Refactor del issue (NO implementado).** Registros de IDs coherentes en todo el lifecycle.

**Riesgo.** Listas de IDs desincronizadas al borrar nodos (el mismo fallo que el reverse index).

### Inventario estatico (bucles GetElement/GetKey)

Filas con **sonda**: formula de `index_calls` y por que.

| sitio path:line | map | llamadas / formula | tamano | frecuencia |
|---|---|---|---|---|
| `scripts/5_Mission/LFPG_ElecGraphImpl.c:394` prune RebuildFromWires | `m_Nodes` | **sonda** `Count()` = 1 GetKey/iter | N nodos | cada rebuild de wires |
| `scripts/5_Mission/LFPG_ElecGraphImpl.c:977` split watchdog | `m_WdgVisited` | **sonda** `Count()` = 1 GetKey/iter | visitados del componente | al partir componente |
| `scripts/5_Mission/LFPG_ElecGraphImpl.c:1252` RebuildComponents reset | `m_Nodes` | **sonda** `Count()` = 1 GetElement/iter | N | si `m_ComponentsDirty` |
| `scripts/5_Mission/LFPG_ElecGraphImpl.c:1267` RebuildComponents BFS seed | `m_Nodes` | **sonda** `Count()*2` = GetElement + GetKey (`:1277`) por semilla | N | idem |
| `scripts/5_Mission/LFPG_ElecGraphImpl.c:1944` GetOverloadedSourceCount | `m_Nodes` | **sonda** `Count()` = 1 GetElement/iter | N | consulta |
| `scripts/5_Mission/LFPG_ElecGraphImpl.c:2029` PostBulkRebuild snapshot | `m_Nodes` | **sonda** `Count()*2` = GetKey+GetElement/iter | N | CutAll / CutPort changed |
| `scripts/5_Mission/LFPG_ElecGraphImpl.c:2139` MarkComponentDirty | `m_Nodes` | **sonda** `Count()*2` cota: GetElement siempre, GetKey (`:2142`) solo en hit | N | suciedad por componente |
| `scripts/5_Mission/LFPG_ElecGraphImpl.c:2156` MarkSourcesDirty | `m_Nodes` | **sonda** `Count()*2` cota: GetElement siempre, GetKey en SOURCE/PT | N | post rebuild |
| `scripts/5_Mission/LFPG_ElecGraphImpl.c:3418` ValidateConsumerStates | `m_Nodes` | **sonda** `checked*2` al salir: GetKey+GetElement por nodo del batch | N, batch `LFPG_VALIDATE_BATCH_SIZE` | al drenar cola / idle |
| `scripts/5_Mission/LFPG_ElecGraphImpl.c:3639` PopulateAllNodeElecStates | `m_Nodes` | **sonda** `Count()*2` = GetKey+GetElement/iter | N | post bulk rebuild |
| `scripts/5_Mission/LFPG_NetworkManagerImpl.c:567` / `:572` rate limiter | `m_RateByPlayer` | GetElement + GetKey | jugadores recientes | tick limpieza |
| `scripts/5_Mission/LFPG_NetworkManagerImpl.c:688` vanilla by index | `m_VanillaWires` | 1 GetKey | owners vanilla | acceso indexado |
| `scripts/5_Mission/LFPG_NetworkManagerImpl.c:805` / `:809` battery rebuild | `m_BatteryRebuildGeneration` | GetKey+GetElement | baterias | rebuild |
| `scripts/5_Mission/LFPG_NetworkManagerImpl.c:828` CutPendingPowerOff | `m_CutPendingPowerOff` | GetKey | pendientes | corte |
| `scripts/5_Mission/LFPG_NetworkManagerImpl.c:937` / `:938` vanilla rebuild | `m_VanillaWires` | GetKey+GetElement/owner | owners vanilla | rebuild |
| `scripts/5_Mission/LFPG_NetworkManagerImpl.c:1193` vanilla broadcast | `m_VanillaWires` | GetElement | owners | flush |
| `scripts/5_Mission/LFPG_NetworkManagerImpl.c:1555` pending LFPG | `m_PendingBroadcastLFPG` | GetElement | cola | tick |
| `scripts/5_Mission/LFPG_NetworkManagerImpl.c:1565` pending snapshots | `m_PendingOwnerSnapshots` | GetElement | cola | tick |
| `scripts/5_Mission/LFPG_NetworkManagerImpl.c:1573` / `:1574` pending vanilla | `m_PendingBroadcastVanilla` | GetKey+GetElement | cola | tick |
| `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2125` full sync vanilla | `m_VanillaWires` | GetKey | owners | sync |
| `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2230` deferred snapshots | `m_DeferredOwnerSnapshots` | GetElement | owners | sync |
| `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2692` reverse idx | `m_ReverseIdx` | GetKey | claves reverse | rebuild reverse |
| `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2895` validation vanilla ids | `m_VanillaWires` | GetKey | owners | startup |
| `scripts/5_Mission/LFPG_NetworkManagerImpl.c:3426` / `:3427` vanilla walk | `m_VanillaWires` | GetKey+GetElement | owners | rebuild |
| `scripts/5_Mission/LFPG_NetworkManagerImpl.c:3595` / `:3604` vanilla vr | `m_VanillaWires` | GetKey+GetElement | owners | rebuild |
| `scripts/5_Mission/LFPG_NetworkManagerImpl.c:3742` last known pos | `m_LastKnownPos` | GetKey | owners | tracking |
| `scripts/5_Mission/LFPG_NetworkManagerImpl.c:3830` / `:3831` vanilla pos | `m_VanillaWires` | GetKey+GetElement | owners | tracking |
| `scripts/5_Mission/LFPG_NetworkManagerImpl.c:4076` / `:4077` graphIncomingByPort | `graphIncomingByPort` | GetKey+GetElement | puertos | coherencia |
| `scripts/5_Mission/LFPG_NetworkManagerImpl.c:4223` / `:4224` fallback vanilla | `m_VanillaWires` | GetKey+GetElement | owners | fallback |
| `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:891` / `:892` refund | `refundByUid` | GetKey+GetElement | jugadores | refund |
| `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:2163` / `:2164` persist | `s_Balances` | GetKey+GetElement | jugadores | persist |
| `scripts/5_Mission/LFPG_ControlSessionRegistry.c:278` sessions | `m_ByUID` | GetElement | sesiones CCTV | tick |
| `scripts/5_Mission/LFPG_ControlSessionRegistry.c:530` / `:531` sessions keys | `m_ByUID` | GetKey+GetElement | sesiones | listado |
| `scripts/3_Game/LFPG_Util.c:53` / `:55` warn rate limits | `s_WarnRateLimits` | GetElement+GetKey | claves warn | purge |
| `scripts/4_World/LFPG_DeviceRegistry.c:219` ambiguous | `m_AmbiguousIds` | GetKey | ids | GetAll |
| `scripts/4_World/LFPG_DeviceRegistry.c:246` / `:252` by-id | `m_ById` | GetElement+GetKey | dispositivos | GetAll |
| `scripts/4_World/LFPG_DeviceRegistry.c:294` / `:297` null purge | `m_ById` | GetElement+GetKey | dispositivos | GC |
| `scripts/4_World/LFPG_CableRenderer.c:902` owners | `m_ByOwnerId` | GetKey | owners visibles | **frame client** |
| `scripts/4_World/LFPG_CableRenderer.c:930` segments | `m_WireSegments` | GetElement | segmentos | frame |
| `scripts/4_World/LFPG_CableRenderer.c:1028` / `:1031` neg cache | `m_NegCache` | GetElement+GetKey | cache | frame |
| `scripts/4_World/LFPG_CableRenderer.c:1504` pending sync | `m_PendingDeviceSyncLow` | GetKey | pendientes | sync |
| `scripts/4_World/LFPG_CableRenderer.c:1628` / `:1630` retry due | `m_DeviceSyncRetryDue` | GetElement+GetKey | retries | sync |
| `scripts/4_World/LFPG_CableRenderer.c:1662` / `:1664` cooldowns | `s_DeviceSyncCooldowns` | GetElement+GetKey | cooldowns | sync |
| `scripts/4_World/LFPG_CableRenderer.c:1948` cache devices | `st.m_CacheDevices` | GetKey | devices | frame |
| `scripts/4_World/LFPG_CableRenderer.c:2024` last by-owner | `entry.m_ByOwner` | GetElement (Count-1) | 1 | lookup |
| `scripts/4_World/LFPG_CableRenderer.c:2373` owner state | `m_ByOwnerId` | GetElement | owners | frame |
| `scripts/4_World/LFPG_CableRenderer.c:2677` sort segments | `m_WireSegments` | GetElement | segmentos | frame |
| `scripts/4_World/LFPG_CableRenderer.c:2980` candidate | `m_WireSegments` | GetElement | 1 | pick |
| `scripts/4_World/LFPG_CableRenderer.c:3017` draw | `m_WireSegments` | GetElement | segmentos | frame |
| `scripts/4_World/LFPG_CableRenderer.c:3718` priority | `m_WireSegments` | GetElement | segmentos | frame |
| `scripts/4_World/LFPG_CableRenderer.c:3756` wires | `m_WireSegments` | GetElement | segmentos | frame |
| `scripts/4_World/LFPG_CableRenderer.c:3893` retry keys | `m_RetryQueue` | GetKey | retries | retry |
| `scripts/4_World/LFPG_CableRenderer.c:4033` / `:4034` by owner | `m_ByOwnerId` | GetKey+GetElement | owners | frame |
| `scripts/4_World/LFPG_CableRenderer.c:4111` / `:4113` retry prefix | `m_RetryQueue` | GetKey | retries | retry |
| `scripts/4_World/LFPG_CableRenderer.c:4250` / `:4266` resident | `m_WireSegments` | GetElement | segmentos | frame |
| `scripts/4_World/LFPG_CableRenderer.c:4321` / `:4341` segment keys | `m_WireSegments` | GetKey | segmentos | frame |
| `scripts/4_World/LFPG_LaserBeamRenderer.c:253` detectors | `m_Detectors` | GetKey | detectores | cliente |
| `scripts/4_World/LFPG_LaserBeamRenderer.c:275` proj cache | `m_ProjCache` | GetKey | cache | cliente |

Que cuenta `allocs` (campo del evento): solo `AddAlloc` en HandleCutWires (2 arrays de delta), RescueStaleIncomingWires (3 arrays) y MarkUpstreamNodesDirty (array+map = 2). **No** cuenta `new` dentro de `PostBulkRebuildAndPropagate`, `RebuildFromWires`, `RebuildComponents`, `PopulateAllNodeElecStates` ni colas de broadcast.

Instrumentado en esta ronda: solo barridos de `LFPG_ElecGraphImpl.c` (lista blanca). El resto queda en inventario para PRs posteriores.

---

## Como NO cuenta como optimizacion

Mover trabajo a otro modulo o tick, o retirar funciones, no cuenta. La medida es tiempo completo por evento y por fase, visitas, requeues, allocations y ticks hasta cola vacia.
