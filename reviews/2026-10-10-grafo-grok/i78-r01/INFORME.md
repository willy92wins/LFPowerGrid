# INFORME i78-r01 — CutWires reconstruye el indice solo si rescued > 0

## Tabla

| punto | veredicto | ficheros:lineas | prueba |
|---|---|---|---|
| D1 | OK (con hallazgo adyacente empty pid) | `LFPG_RPCServerHandlerImpl.c:1003-1018`, `LFPG_WireHelper.c:190-227`, `LFPG_IDevice.c:635-652` | T3 + lectura |
| D2 | OK | `ReverseIdxRemove` `LFPG_NetworkManagerImpl.c:1087-1133`, credito `:760`, batch `:4098-4259` | lectura |
| D3 | OK | lectores `m_ReverseOwners`/`m_ReverseIdx` listados abajo | lectura |
| D4 | OK | `PostBulkRebuildAndPropagate` `:853-860`, `PostBulkRebuild` `LFPG_ElecGraphImpl.c:2009-2072`, `OnWireRemoved` `:897-1000` | lectura + T4 |
| D5 | OK | vanilla `HandleCutWires` `:1027-1077` | lectura |
| cambio | OK | `LFPG_RPCServerHandlerImpl.c:1121-1123` | diff |
| T1 | OK | `.github/tools/test_cutwires_rebuild_gate.py` | 6 casos; positivos fallan con la condicion de `4e897f2` (revision) |
| T2 | OK | mismo fichero | 1 RebuildReverseIdx; Rescue y sondas fijados a `4e897f2` (revision) |
| T3 | OK | mismo fichero | combinaciones D1 salvo pid vacio restringido |
| T4 | OK | QueueModel importado de `test_graph_multifeed_split.py` | montajes de i76 (`GRAPHS`), aparatos sin entradas: corte + reposicion (revision) |

### D1 — espejo del pre-scan

El pre-scan LFPG (`LFPG_RPCServerHandlerImpl.c:994-1010`) salta un cable si `st && !st.AllowCutOthersWires && pwd.m_CreatorId != "" && pwd.m_CreatorId != cutPid` (`:1003`). Luego `ClearDeviceWiresForCreator` (`:1012-1014`) o `ClearDeviceWires` (`:1018`).

`ClearDeviceWires` (`LFPG_IDevice.c:635-642`) llama `LFPG_ClearWires` → `LFPG_WireHelper.ClearAll` (`LFPG_WireHelper.c:190-198`), que vacia el array entero.

`ClearDeviceWiresForCreator` (`LFPG_IDevice.c:645-652`) llama `ClearForCreator` (`LFPG_WireHelper.c:203-227`), que borra `m_CreatorId == "" || m_CreatorId == creatorId`.

Combinaciones: sin settings o `AllowCutOthersWires` true → no skip, `ClearAll`. Restringido: propio, vacio y ajeno coinciden con ClearForCreator. T3 lo ejecuta.

Vanilla (`:1033-1067`): misma predicado propio+vacio vs todos, y `ReverseIdxRemove` sobre el mismo conjunto que se `Remove`/`Clear`.

Hallazgo adyacente (no empeora el cambio): `ClearForCreator` con `creatorId == ""` sale false (`LFPG_WireHelper.c:205-206`); el pre-scan con `cutPid` vacio y modo restringido sigue indexando cables sin creador. `HandleCutWires` toma `cutPid` de `sender.GetPlainId()` (`:977`). Entradas `pwd` nulas: pre-scan `continue` (`:1000`), `ClearForCreator` las borra (`:213-217`).

### D2 — el grafo se entera cable a cable

Cada cable del pre-scan llama `ReverseIdxRemove` (`:1007` LFPG, `:1044`/`:1062` vanilla).

`ReverseIdxRemove` (`LFPG_NetworkManagerImpl.c:1087-1133`):
- `m_Graph` nulo: no llama `OnWireRemoved`; `PostBulkRebuildAndPropagate` (`:841-851`) tampoco reconstruye el grafo. No hay arista viva.
- `ownerDeviceId`: el `deviceId` de `GetOrCreateDeviceId` (`HandleCutWires:971-972`), no vacio.
- `m_ExplicitGraphRemovalCredit`: lo escribe `NotifyGraphWireRemoved` (`:760`) como `sourceId|targetId|creditTargetPort`. Lo consume `ReverseIdxRemove` (`:1096-1100`) y **siempre lo vacia**, coincida o no. `HandleCutWires` no escribe el credito. Un credito viejo solo podria silenciar `OnWireRemoved` en el **primer** cable del pre-scan si la tripleta coincide; el resto ya ve credito vacio. Camino de corte de puerto IN usa el mismo gancho. Si `reverseMatchCount > 1` (`:1131-1132`) se pone `m_GraphFullRebuildRequired`.
- `m_CutAllGraphBatchActive`: true solo entre `:4098` y `:4259` dentro de `CutAllWiresFromDevice`. `HandleCutWires` no lo pone. RPC de alicates y corte por movimiento no se anidan. Durante CutWires vale false, asi que el gancho de grafo corre.

### D3 — duenos fantasma

`ReverseIdxRemove` solo borra la lista de duenos cuando `prev <= 1` (`:1140-1148`). Si quedan mas cables a la misma clave, el id del dueno cortado puede quedar en `m_ReverseOwners` (camino Splitter hoy).

Lectores:
- `CountWiresTargeting` (`:976-990`): usa `m_ReverseIdx`, no la lista. Contador decrementado una vez por cable.
- `IsPortTargetedByPoweredSource` (`:991-1050`): recorre duenos y exige cable vivo (`stillTargetsPort`); el fantasma hace `continue`.
- `RemoveWiresTargeting` (`:1205-1354`): recorre copia; sin cable no borra. Lista vacia retorna 0 (`:1219-1221`).
- `DeviceHasAnyWires` (`:2564-2600`): salidas por almacen; entradas por `CountWiresTargeting`.
- `CleanDisappearedVanillaDevice` (`:2686-2706`): claves de `m_ReverseIdx` con prefijo del vanilla; `RemoveWiresTargeting` tolera fantasma.
- `CutAllWiresFromDevice` (`:3995-4097`): `m_ReverseOwners` para ver si el origen del grafo esta indexado; un id extra no oculta al real.
- `LFPG_Actions.c:902`: `CountWiresTargeting`.
- `RebuildReverseIdx` (`:906-907`) limpia ambos mapas.
- `ReverseOwnersInsert` / `ReverseIdxAdd` (`:960-1084`): insercion, no lectura de fantasma como verdad.

Contadores: cada `ReverseIdxRemove` resta 1 a la clave `target|port` (`:1147`) o elimina la clave si era el ultimo (`:1142-1143`). El pre-scan llama una vez por cable que luego borra el store.

### D4 — reconstruccion completa vs incremental

`PostBulkRebuildAndPropagate` rama completa (`LFPG_NetworkManagerImpl.c:853-860`): `CaptureBatteryGraphState`, `m_Graph.PostBulkRebuild`, `RestoreBatteryGraphState`, `m_GraphFullRebuildRequired = false`, `m_GraphRebuildGeneration + 1`. Luego `ProcessDirtyQueue`.

`PostBulkRebuild` (`LFPG_ElecGraphImpl.c:2009-2072`): snapshot de nodos, `RebuildFromWires`, `PopulateAllNodeElecStates`, `MarkSourcesDirty`, `ResetOrphanSyncVars` en ids que ya no estan.

Incremental (mismo que Splitter con IN): `OnWireRemoved` (`:897-1000`) quita arista, `SetPowered(false)` en destino (`:937-941`), `CleanupOrphanNode` (`:1626-1675`) llama `ResetOrphanSyncVars` (`:1658`). `MarkNodeDirty` TOPOLOGY en origen y destino (`:997-998`). `PostBulkRebuildAndPropagate` sin flag completo solo drena la cola (`:861-864`). Baterias: Capture/Restore solo en rebuild completo; el incremental conserva `m_VirtualGeneration` igual que el Splitter. `m_GraphRebuildGeneration` no sube en corte limpio; el Splitter tampoco. T4 compara (criterio de la ronda 5 de i76, mas powered) tras quitar todas las salidas de un aparato sin entradas y tras reponerlas una a una.

### D5 — fuente vanilla

Sin `HasWireStore` (`:1027-1077`): mismo filtro de creador, `ReverseIdxRemove` + `PlayerWireCountAdd` por cable, luego `Remove` o `Clear`. El bloque de rescate (`:1116`) es el mismo. Un generador vanilla sin puertos IN entra al scan; con este cambio no llama `RebuildReverseIdx` si `rescued == 0`.

### Cambio

`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1121-1123`: comentario nuevo y `if (rescued > 0)` (antes `rescued > 0 || (changed && !anyInRemovedByIndex)`). Scan `:1116` y aviso `:1125` intactos. `HandleCutPort` IN (`:2097-2100`) no cambia.

`enforce_scalar_slice` no admite el literal del `LFPG_Util.Warn`; T1 sustituye esa llamada por `WarnStub()` y `""` del rescate por `EMPTY`.

## PROTOCOLO EN PARTIDA

PENDIENTE-INGAME. Lo corre otro hilo con DayZ MCP.

1. Empaquetar esta rama con `LFPG_PERF_PROBE = true` solo en el servidor de pruebas.
2. Montar `fixture_grid` ~51 nodos y ~2031 nodos (misma familia que la medicion 13-oct, build `0843f22`).
3. Alicates reales:
   - generador cuya isla cortada tenga ~10 nodos, red total ~2031;
   - Splitter con cable de entrada;
   - Splitter sin cable de entrada.
4. Esperado en generador y Splitter sin entrada, red 2031: `phase_rebuild_ms` <= ~10 (hoy 96), `settle_ticks` <= 2 (hoy 49), **cero** lineas `[CutWires-Fallback] Reverse index was stale`, `phase_rescue_ms` anotado (el scan sigue). Splitter con entrada: mismo orden que hoy (~4 ms rebuild, 1 tick).
5. Correccion: isla cortada sin corriente, generador 0 % en inspector; red aparte igual; al reconectar, estado previo; tras reiniciar el servidor, el mismo estado.
6. Indice parcial (IN miss, rescued=0) no se monta en partida; queda en T1.

## GATES

Linter offline `script_validator.py .` (gate por `len(errors)`):
- base: errors=0, warnings=48, status WARN, files_scanned=267
- final: errors=0, warnings=48, status WARN, files_scanned=267
- delta errors 0; delta warnings 0

Tests (todos exit 0):
- `test_enforce_checks.py`: 21 OK
- `test_graph_capacity_refresh.py`: 6 OK
- `enforce_checks.py --root .`: FAIL=0 WARN=0
- `test_graph_charger_energy.py`: 19 OK
- `test_finish_wiring_quota.py`: 7 OK
- `test_graph_perf_probe.py`: 14 OK
- `test_graph_reference.py`: 20 OK
- `test_graph_multifeed_split.py`: 39 OK
- `test_broadcast_contract.py`: 3 OK
- `test_cutwires_rebuild_gate.py`: 9 OK

T1 contra la base: la base es el bloque actual con la condicion de `4e897f2` repuesta (`old_gate_block`); es igual al bloque de `4e897f2` salvo comentarios y da la misma tabla de verdad. Con ella los dos positivos dan rebuilds=1; con el cambio, 0. Negativos: rescued=1 → 1 llamada; changed=false rescued=0 → 0; IN acertado (no entra al bloque) → 0. Ambos arboles.

## HALLAZGOS-ADYACENTES

- `ClearForCreator` rechaza `creatorId == ""`; el pre-scan restringido con `cutPid` vacio no. Alcance: RPC con PlainId vacio. El cambio de gate no lo empeora.
- Cables nulos en el array: pre-scan los ignora, `ClearForCreator` los borra. El indice no se toca para nulos.
- Credito `m_ExplicitGraphRemovalCredit` residual podria saltar `OnWireRemoved` en el primer `ReverseIdxRemove` de un RPC posterior. No lo toca este brief.
- `ReverseIdxRemove` no recorta un dueno concreto de `m_ReverseOwners` cuando el contador queda > 0 (fantasmas). Ya existia en el Splitter.
- (revisor) En el modelo, quitar `esp` (lleva 0) del montaje F deja `e20`=20 y la sobrecarga en `c`; la reconstruccion sin `esp` da `e20`=0 y la sobrecarga en `s20`. Al reponer el cable converge. Es el camino incremental de siempre (cortar un cable, o CutAll de un Splitter con entrada, que nunca reconstruian): R-01 no lo toca. La propiedad de la ronda 5 de i76 no compara ese estado intermedio.

## LO QUE NO PUDE VERIFICAR

- Compilacion Enforce al cargar el mundo (no hay compilador aqui).
- Tiempos `phase_rebuild_ms` / `settle_ticks` in-game (PENDIENTE-INGAME).
- Persistencia tras restart de servidor.
- Que `sender.GetPlainId()` sea siempre no vacio en partida.
- Que un credito de `NotifyGraphWireRemoved` sobreviva hasta `HandleCutWires` (no hay traza de RPC solapados).

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

La premisa asume que, con indice sano, el pre-scan mas `OnWireRemoved` cubre lo que hacia `PostBulkRebuild`. Eso es cierto para aristas unicas (`reverseMatchCount == 1`). Dos cables del mismo origen al mismo puerto destino marcan `m_GraphFullRebuildRequired` (`:1131-1132`) y el rebuild completo sigue ocurriendo **sin** pasar por `rescued > 0`. Si ese caso es frecuente, el ahorro de R-01 no llega a esas redes; no es un fallo del gate, es el gancho ya existente.

Si el indice inverso esta podrido de un corte anterior (contadores != cables vivos) y el scan de rescate no encuentra huerfanos porque el aparato cortado no es el destino de esos huerfanos, este cambio **deja** de reconstruir el indice. La premisa dice que eso solo importaba por duenos fantasma, y D3 argumenta que los lectores los toleran. Si hay un lector no listado (mod externo, UI futura), el indice no se cura hasta el siguiente `rescued > 0` o un `RebuildReverseIdx` de otra ruta (`:3104`, `:3714`).

## REVISION DEL REVISOR (ronda 1)

- El test leia la base con `git show HEAD:`; tras el commit (y en CI) HEAD ya trae el cambio y los dos positivos de T1 fallaban. T1 usa ahora la condicion vieja repuesta en el bloque actual, y T2 fija como literales el hash de `RescueStaleIncomingWires` y la lista de sondas de `4e897f2`.
- T4 usa los montajes de i76 (`GRAPHS`, `_recon`, `_assert_match`), repone los cables uno a uno sembrando solo sus extremos y se limita a aparatos sin entradas, que es el caso que cambia R-01. Un Splitter que conserva su entrada no tiene un CutAll real que quite solo las salidas, y ese camino ya era incremental antes del cambio.
- T3 explica en un comentario por que salta el caso de pid vacio en modo restringido.
