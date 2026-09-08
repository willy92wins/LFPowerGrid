# LFPowerGrid — Auditoría completa: sobreingeniería, optimización y armonización de procesos

- Fecha: 2026-09-07. Rama auditada: `fix/t6-dismantle-guard` (@ `a187898`), árbol `LFPowerGrid/` (fuente compilable).
- Método: 4 lanes delegadas en paralelo (servidor / cliente / sorter+BTC+dispositivos / assets+compile) + verificación propia de cada ancla citada (grep/lectura directa, sin modificar nada). Sin pruebas in-game.
- Nota de líneas: los números son de filesystem (`Get-Content`/`Select-String`); el visor de lectura reporta totales algo mayores en los 3 ficheros grandes, pero los números de línea de las citas coinciden en ambas herramientas. Si tu checkout difiere en ±20 líneas, busca por nombre de función.
- Leyenda: **[V]** verificado por mí · **[L]** aportado por lane y ancla re-verificada · **[P]** plausible, falta lectura directa o dato in-game.

## 0. Métricas base [V]

- 152 `.c`, 72.249 líneas. `3_Game` 18 fich/5.350 ln · `4_World` 122/45.927 · `5_Mission` 12/20.972.
- Top por líneas: `5_Mission/LFPG_NetworkManagerImpl.c` 6.989 · `4_World/LFPG_CableRenderer.c` 3.836 · `5_Mission/LFPG_ElecGraphImpl.c` 3.592 · `5_Mission/LFPG_RPCServerHandlerImpl.c` 2.655 · `5_Mission/LFPG_BTCHelper.c` 2.632 · `4_World/test/LFPG_SorterController_TEST.c` 2.094 · `test/LFPG_SorterView_TEST.c` 2.006 · `5_Mission/LFPG_BalanceProvider_NativeImpl.c` 1.945 · `4_World/LFPG_SorterController.c` 1.878 · `4_World/LFPG_DeviceInspector.c` 1.823 · `4_World/LFPG_SorterView.c` 1.589 · `4_World/LFPG_Actions.c` 1.366 · `5_Mission/LFPG_SorterLogic.c` 1.202 · `4_World/LFPG_BTCAtmView.c` 1.195 · `4_World/LFPG_TestDevices.c` 1.176 · `4_World/LFPG_CameraViewport.c` 1.128.
- Versión coherente: `config.cpp:208` y `3_Game/LFPG_Defines.c:527` ambas `1.2.4` (F1 de agosto, cerrado).
- `data/`: 295 ficheros, ~419 MB. `.p3d` 49 fich/≈355 MB. Fuente empaquetable total ≈422 MB → PBO ≈99 MB.
- 117 `RegisterNetSyncVariable` en 33 ficheros [V]. ~50 SubIds RPC en `3_Game/LFPG_Defines.c:276-341` [V]. 1.651 líneas de guards `#ifndef/#ifdef SERVER` [V].
- `stringtable.csv` 263 líneas · `model.cfg` 1.783 líneas · `gui/` 12 layouts.

---

## 1. Sobreingeniería

### S1. Dios `NetworkManagerImpl` — 6.989 ln, ~20 responsabilidades [L+V]
`5_Mission/LFPG_NetworkManagerImpl.c`: ciclo de vida+scheduler `:577-700`, rate-limit `:710-828`, wires vanilla `:829-954`, fachada grafo `:956-1187`, índice inverso `:1188-1485`, contador por jugador `:1486-1727`, cuotas+geometría `:1728-1816`, batching `:1817-1950`, sync LFPG `:1990-2505`, sync vanilla+full-sync `:2506-2870`, device-sync `:2864-3056`, propagación `:3057-3194`, polling movimiento `:3195-3710`, self-heal 16 fases `:3713-4442` (16× `LFPG_Validation*` + dispatcher con 15 `if` en `:4055+`, máquina de estados manual donde bastaría `switch`/tabla), persistencia diferida `:4443-4703`, CutAll `:4704-5126`, solar+BTC+agua `:5127-5736`, registros `:5737-5837`, sorter `:5838-6591`, sensores/pads/láser/intercom/horno/frigo/cocina/puerta `:6592-6996`, detección `:6997-7432`, baterías `:7433-fin`.
Propuesta: extraer por dominio (detección → sorter → solar/pump → BTC), núcleo ≤2.500 ln, un subsistema por PR.

### S2. Fork TEST divergente, hoy peor que en agosto [V+L]
Declarado en `4_World/test/LFPG_SorterView_TEST.c:34` ("fork of LFPG_SorterView.c… propagate to V3 first then re-clone"). Medido por intersección: View 786 comunes (80% de V3 duplicado), Controller 881 (72%). El TEST ya es MÁS GRANDE que prod (2.006 vs 1.589; 2.094 vs 1.878). Clases TEST spawneables en `config.cpp:1080-1091`; layouts `gui/layouts/test/LFPG_Sorter_TEST.layout` 58.240 B vs `gui/layouts/LFPG_Sorter.layout` 51.143 B; 11 SubIds espejo en `3_Game/LFPG_Defines.c:324-336` (`SORTER_TEST_*=60-70` replican 19-23,29-34). Duplicación función a función (mismo cuerpo + sufijo `_TEST`): `EnsureViewBindings` (`LFPG_SorterView.c:336` vs `test/…:446`), `AssignButtonIDs` (`:482` vs `:639`), `ApplyColors` (`:587` vs `:732`), `Tint/Cache/OnClick` (`:740-1669` vs `:816-1851`), controller `EnsureBindings/FindBtnChildBg/SetStatus/ToggleCategory/Save/Sort/RefreshAll/RequestPreview` (`LFPG_SorterController.c:261-1990` vs `test/…:267-1877`). Novedad real ≈450 ln (rail, builder tabs, hook MCP `RunS1Probe/PollMcpSorterCmd` en `test/LFPG_SorterView_TEST.c:1573-1993`).
Propuesta: base común (`SorterViewBase/ControllerBase/TagBase/PreviewRowBase` + chrome compartido) y TEST reducido a ~500 ln de delta; colapsar los 11 RPCs con parámetro `viewKind`. Alternativa de producto: TEST fuera del PBO release.

### S3. Reflexión `Call*` sin beneficiario en producción [V+L]
`4_World/LFPG_IDevice.c:433-482` (`CallInt/Bool/String/Vector/Void/Float` vía `CallFunctionParams`, 42 hits) + ~25 wrappers con doble vía (`GetDeviceId:487-495`, `HasPort:497-505`, `IsSource:515-523`…). Los 34 dispositivos de producción heredan `DeviceBase/WireOwnerBase`; solo 3 clases legado usan el fallback (`LFPG_TestDevices.c:53,930,1354`). 4 wrappers ni tienen fast-path (`GetPortWorldPos:507-513`, `GetPortName:840-853`, `GetPortDir:857-870`, `GetPortLabel:874-886`).
Propuesta: borrar `Call*`, añadir los 4 fast-path, migrar las 3 legado a la jerarquía.

### S4. Ecosistema BTC ≈10.200 ln / 14 ficheros para una feature lateral [L+V]
`5_Mission/LFPG_BTCHelper.c` 2.632 (7 `HandleBTC*`: `Open:979`, `Buy:1064`, `Sell:1536`, `Withdraw:2032`, `Deposit:2199`, cash `:2488,:2670`), `5_Mission/LFPG_BalanceProvider_NativeImpl.c` 1.945 (8 mapas estáticos claims/reconcile/orphan), `4_World/LFPG_BTCAtmView.c` 1.195 + `LFPG_BTCAtmController.c` 925 (tercer MVC que repite el chrome del sorter), `3_Game/LFPG_BTCConfig.c` 631, `5_Mission/LFPG_BTCPriceFetcher.c` 536, `4_World/LFPG_BTCAtm.c` 509, `5_Mission/LFPG_BTCSessionRegistry.c` 399 gemelo de `LFPG_ControlSessionRegistry.c` 453 sin base común, `3_Game/LFPG_BTCDefines.c` 315, providers 241+71+40+53. El provider promete futuro (`4_World/LFPG_BalanceProvider.c:14-16`) pero solo existen Native+LBmaster (este tras `#ifdef`); `4_World/LFPG_AtmStock.c:7-12` admite que el stock no usa el registry.
Propuesta: congelar BTC (feature-freeze), partir el helper (cash vs RPC vs sesión), fusionar los 2 session registries, 1 sola API de precio.

### S5. Acciones y chrome UI triplicados [V+L]
Switch ×3 (97+100+98 ln, solo cambia el `Cast`), Speaker On/Off (83+82, difieren en 1 negación), ~30 acciones con mismo `CreateConditionComponents+DistSq+Cast`. Cliente: `5_Mission/LFPG_MissionInit.c:182-208` OnKeyPress ×3, `:227-244` ESC ×3, `:256-329` force-close ×3; `LightenARGB` en `4_World/LFPG_BTCAtmView.c:1043` + `4_World/LFPG_SorterView.c:1648`, `IsEscCooldown` ×3.
Propuesta: `ActionToggleBase<T>` genérica + base de vista común.

### S6. Capas que no hacen nada [V]
`3_Game/LFPG_Migrators.c:96-107` dos migraciones no-op ("No field changes — just bumps version"); hooks vacíos en `4_World/lfpg_devicebase.c:575-587` + `4_World/LFPG_WireOwnerBase.c:293-296` (4 niveles de indirección para un `EEInit`); `LFPG_GetKitClassname/BlocksDismantle` (`lfpg_devicebase.c:596-619`) para 3-4 overrides.
Propuesta: borrar o implementar; no mantener esqueletos.

### S7. `Defines.c` (811 ln) mezcla todo + estados futuros [V]
Sway, LOD, oclusión, persistencia, RPC, sorter, sensores, pump, searchlight, baterías, intercom en `3_Game/LFPG_Defines.c:1-811`. `LFPG_CableState:84-95` con 9 estados y 3 usados (el propio comentario lo admite); presupuestos warmup/dinámicos `:449-458`; `DeviceType.CAMERA` futuro.
Propuesta: no añadir más reservas; partir por dominio solo cuando se toque (sway/LOD/oclusión a un `LFPG_RenderDefines`, economía a `BTCDefines`).

---

## 2. Mala optimización — servidor

- **H1. `BroadcastOwnerWires` doble barrido + JSON por mutación [L].** `5_Mission/LFPG_NetworkManagerImpl.c:1993-2112`: `GetPlayers` en `:2012` y `:2069`, scans en `:2021-2033` y `:2097-2112`, `GetWiresJSON` en `:2062`. O(P+W+P+W) por broadcast, ×N owners en cascadas. Fix: un solo `GetPlayers`, un barrido, `DistSq` antes de serializar (~30 líneas).
- **H2. `CleanDisappearedVanillaDevice` scan por prefijo [L].** `:3380-3477`: allocs `:3391,:3450`, scan `m_ReverseIdx` con `IndexOf(prefix)` en `:3452-3458`, encadenable desde `CheckDeviceMovement:3650-3653`. Fix: índice por owner en vez de scan por prefijo.
- **H3. `CutAllWiresFromDevice` fallback O(V·W) [L].** `:4930-4996` (`GetAll` + `new` por fuente `:4959` + segundo scan `:4993-4996`), gateado por `reverseIndexConsistent:4937` pero pico peor del servidor. Fix: mantener el índice consistente para no pisar el fallback + reusar buffers.
- **H4. Tick sorter: 6 resoluciones lineales + sqrt + dedup O(n²) [L+V parcial].** `:5897-6279`: `Distance` con sqrt `:5996`, 6× `ResolveOutputContainer` (`5_Mission/LFPG_SorterLogic.c:971-1003`, con `"output_"+ToString:978`), rebuild cache O(items), `.Find` dedup `:6164-6216`. Presupuestos en `3_Game/LFPG_Defines.c:351-355` (8 sorters/tick, 3 ítems, 20 evals, budget 512). Fix: `DistSq`, tabla de nombres de puerto, cache `(sorterId,puerto,generación)`, `map` temporal en vez de `.Find`.
- **H5. `TickWaterPumps` triple barrido incondicional [L].** `:5438-5720`: reset de todos los sprinklers `:5467-5483`, degradación T1+T2, `GetWires+FindById+Cast` por wire. Fix: early-out si `t1+t2==0` o ningún pump con power.
- **H6. Celdas de jugadores O(P·C) + doble rebuild [V+L].** `LFPG_RebuildPlayerCells:7003` con búsqueda lineal `:7047-7054`; llamada desde simple `:6946-6947` y detección `:7199-7207` (coinciden cada ~3 s). `Collect` escanea todas las celdas por consulta `:7111-7131`. Fix: §4-U1 (flag por turno) + `map<int,int>` celda→índice.
- **H7. `ElecGraph.RebuildFromWires` O(N+E) + sync con 3 resoluciones/nodo [L].** `5_Mission/LFPG_ElecGraphImpl.c:190-328`, `ProcessDirtyQueue` con budgets 64/256 (`3_Game/LFPG_Defines.c:437,443`), `SyncNodeToEntity:2782-2810` (`FindById`→`ResolveVanillaDevice`→`GetObjectByNetworkId`). Correcto pero caro en bulk; amortizar con los diferidos del scheduler (§4-U3).
- **H8. `HandleFinishWiring`: alloc+`ToString`+sqrt antes de validar [L].** `5_Mission/LFPG_RPCServerHandlerImpl.c:222,261,272-275,296,348-349`. Fix: validar barato primero, `DistSq`, construir strings dentro del guard de nivel.
- **H9. `RepackCargoInPlace`: 9 `new` + insertion sort O(n²) [V+L].** `5_Mission/LFPG_SorterLogic.c:1132-1197` allocs, `:1159-1182` insertion sort (verificado), grid W·H `:1186`. Fix: buffers reutilizados + tope `n≤64` con fallback sin repack.

---

## 3. Mala optimización — cliente

Flujo por frame en `5_Mission/LFPG_MissionInit.c:246-465` [V]: `GetPlayer+Cast` 6× (`:258,:287,:311,:334,:451,:490`), cadena inventario reel/pliers 3-4× (`4_World/LFPG_CableRenderer.c:2798-2805` vía `4_World/LFPG_WorldUtil.c:11-36`, `4_World/LFPG_DeviceInspector.c:488-614`, `MissionInit.c:459-460`), `Viewport.Tick:378-382`, searchlight `:385-390`, gates `HasRenderableWires/HasActiveBeams` O(1) (`CableRenderer.c:890-893`, `LaserBeamRenderer.c:124-127`), `DrawFrame`, `DrawOverlay`, `Telemetry.Tick:431` (O(1), 5 s), `TankHUD.Tick:434-435`, `Inspector.Tick/ForceHide:438-445`.

- **C1. Sway invalida la caché [V].** `4_World/LFPG_CableRenderer.c:3274-3275` 2× `Math.Sin`/wire/frame; la comparación de caché `:3283` incluye el sway → casi nunca `reuse` → `(segs+1)×GetScreenPos` (`:3289,:3328`). Fix: gate por distancia (>25 m ⇒ sway 0) + cuantizar `nowMs` a 50-100 ms.
- **C2. Tres pasadas O(n)/frame antes del loop [L].** Reset decoradores `:2921-2926` + reserva near-to-far `:2930-2950` + loop `:2952-3656`. Baratas pero evitables con dirty real.
- **C3. Invariantes por subsegmento [L].** `margin:3391-3395`, `depthWidth` con div+clamps `:3448-3461`, `Sqrt` `:3567,:3591`. Fix: hoist a nivel frame/wire + cuantizar a 0,5 px.
- **C4. `ClipSegToScreen`/`ComputeEdgeFade` solo en miss [V+L].** Bien diseñado (`:3399-3402` gatea; impl `WorldUtil.c:302-392`, fade `:136-196`); hotspot solo con movimiento.
- **C5. Player-occlusion 2 `GetScreenPos`/frame + test por segmento [L].** `:2853-2906`, proyecciones `:2864-2865`, bien gateado por 3P (`Defines.c:584`) y alpha (`:3082`). Fix: actualizar rect a 10-15 Hz.
- **C6. Selection sort O(n²) gateado [V].** `:2717-2742`, solo con `m_DrawOrderDirty` (`:2912-2916`, CullTick `:2631`). Correcto; escalar con sort nativo si n grande.
- **C7. Oclusión bien presupuestada [L].** Tick 50 ms/budget 4 (`Defines.c:234-235`, ~80 rayos/s), 1 muestra/wire (`:3661-3835`). No tocar.
- **C8. CullTick O(owners+wires)/2 s + 1 sqrt/wire [L].** `:2354-2632` (`CULL_TICK_S:Defines.c:551`); `DistSq` salvo `:2561`. Pasar ese `Sqrt` a squared.
- **C9. Preview solo con sesión, `GetScreenPos` por punto de sag [L].** `4_World/LFPG_WiringClient.c:615-1062`, spans cacheados `:661-759` (bien), sag limitado a 5 subs. OK.
- **C10. Inspector bien throttled salvo `GetScreenSize+SetPos`/frame [V+L].** Datos 500 ms + RPC 1 s (`Defines.c:675-676`), `new ScriptRPC` solo al enviar, populate O(P·W) acotado a 8 slots; pero `GetScreenSize:1639-1641` + `SetPos:1715` por frame. Fix: reusar `hud.GetScreenW/H` (`CableHUD.c:221-222`, como ya hacen renderer `:2829-2830` y láser `:226-227`) + `SetPosDirty`.
- **C11. Láser duplica el patrón sin caché entre frames [L].** `LaserBeamRenderer.c:157-342` (cull 250 ms, 2 `GetScreenPos`/beam/frame `:264-265`, `Sqrt:299`). Aplicar la misma caché de proyección que cables.
- **C12. Joints ×4 `DrawLine` [L].** `CableHUD.c:349-352` con budget 64 (`Defines.c:628`, reserva `:2929-2950`). Si `lodBlend<0,05`, saltar proyección de decoradores.

---

## 4. Procesos no armonizados → propuestas de unificación

Base: el scheduler de 100 ms (`NetworkManagerImpl.c:577-700`) ya unifica 10 lanes [V]; `TickSimpleDevices:6806-6995` ya escalona intercom/puertas/hornos/baterías/frigos/stoves/sprinklers por `%`/cursores [V]. Lo siguiente cuelga fuera:

- **U1. Un rebuild de celdas por turno (único doble trabajo real) [V].** Simple `:6946-6947` (si sprinkler activo) y detección `:7199-7207` (si láser/pad/sensor) reconstruyen el mismo índice y coinciden cada ~3 s. Flag `m_PlayerCellsBuiltThisTurn` reseteado en `:618`, consultado en ambos. ~10 líneas, comportamiento idéntico.
- **U2. Sesiones a 1 s + fusionar gemelos [V].** `ControlSessions.Tick()` corre cada 100 ms (`:621-622`) iterando todo (`ControlSessionRegistry.c:371-427`: `IsAlive/IsUnconscious/IsPowered/HasOperator` por sesión) para un timeout de 125 s (`:15`). Acumulador propio a 1 s. Fusionar `BTCSessionRegistry` (399 ln) + `ControlSessionRegistry` (453 ln) en una `SessionStore` con `kind`.
- **U3. One-shots al scheduler (cierra leak LIFE-008) [V].** `ValidateAllWires` 5 s (`:504`), `DoGlobalSelfHeal` 500 ms (`:3726`), `DeferredVanillaPrune` 30 s (`:3973`), `PostBulkRebuild` 1 ms (`:5068`), `RunOrphanSweep` 120 s (`MissionInit.c:55`) son `CallLater(SYSTEM)` que sobreviven a `OnMissionFinish` (el stop `:606-616` solo para el `Timer`). Fases diferidas del scheduler = cancelación gratis + un punto de auditoría.
- **U4. Cola diferida única con coalescencia [V].** `MemoryCell` encola por cambio sin coalescer (`MemoryCell.c:184,267`), `RemoteController` 4 resets + sync (`:539-560,:708`), pulse-offs (`ElectronicCounter.c:363`, `PushButton.c:139`, `SwitchRemote.c:148`), deletes (`KitBase.c:208`, `KitBaseDeployable.c:153`), pair (`DoorController.c:652`). Una `(dueMs,fn,arg)` drenada por el scheduler, coalescencia por `(obj,fn)`.
- **U5. Censo único por turno [V].** 7 counts en `:6841-6848` + re-counts en detection/sorter/pumps/batteries → `m_TurnCensus` calculado una vez por turno; early-outs en 0 en todos los ticks (falta en `TickWaterPumps`).
- **U6. Cliente: 1 maintenance tick + 1 nearby cache + 1 player/frame [V].** 5 cadenas `CallLater` GUI (`CableRenderer.c:865-876`, `LaserBeamRenderer.c:68`) → acumulador en `OnUpdate` (250 ms: laser/tankhud/inspector-TTL; 2 s: cull/retry; 60 s: purge/reconcile). Inspector-fallback/TTL-100 ms + TankHUD-250 ms + Actions-proximity → `NearbyCache` 250 ms compartido. 1× `player` + 1× `hasTool` por frame para los 6 `GetPlayer` y 3-4 cadenas de inventario.

---

## 5. Assets, config y build

- **Top `.p3d` (rutas+bytes medidos [L]):** `logic_gate/memory_cell.p3d` 43.602.321 · `gate_xor` 39.729.763 · `gate_and` 39.729.763 · `gate_or` 39.729.750 · `electric_stove.p3d` 22.333.458 · `proxy/lid_xor|lid_mem|lid_and` 15.487.990 ×3 · `lid_or` 15.487.982 · `lid` 15.486.238 · `rf_broadcaster` 15.377.641 · `searchlight` 11.317.925 · `substation_transformer` 9.730.213 · `sprinkler` 9.534.199 · `sensor` 8.839.857. 4 gates+5 lids ≈230 MB (55% de `data/`). Propuesta: techo ningún `.p3d` >10 MB, `data/` ≤150 MB; 1 gate + 1 lid instanciados (≈200 MB menos).
- **Junk [V+L]:** `data/kits/kitboxtexture.png` 2.340.485 B (+`.paa` 1.219.223 B, 0 refs en config — sacar ambos o referenciar uno) [V]; 3× `*.ogg.stereo_backup` 0,54 MB; `Furnace_mono.ogg` huérfano (`config.cpp:90` usa `Furnace`); `water.ogg` 5.527.214 B (`config.cpp:58`, recomprimir, loops a mono, ningún `.ogg` >500 KB sin justificar).
- **Duplicados SHA256 [L]:** `switch_v1_co==switch_v1_remote_co` (4,3 MB ×2), `lf_solarpanel==t2`, antennas, swatches 14 KB; `.rvmat` 104 ficheros en ~5 plantillas. Propuesta: 0 duplicados por hash, 0 `.png`, materiales compartidos (≥12 MB).
- **Compile [V+L]:** 51/122 ficheros `4_World` sin guard (acciones, `ElecGraph.c` 257 ln, `NetworkManager.c` fachada 645 ln, `RPCServerHandler.c` 22 ln, `RPCGuard` 283 ln); TEST en server (`test/` 178 KB/4.419 ln + `TestDevices.c` 44 KB con 8 syncvars + 4 layouts ~63 KB); `units[]` (`config.cpp:187`) sin `Sorter_TEST` (bien) pero con `LF_TestLamp/Heavy` (`:256,:263`); `include.lst`=`*.c;*.asi;*.anm;*.paa;*.rvmat;*.layout;*.ogg;*.ptc;*.csv` — whitelist que NO lista `*.p3d,*.cpp,*.cfg` (verificar semántica del packer; si es inclusiva el PBO saldría sin modelos); `reviews/` (~19 MB jsonl) y `.github/.claude` solo se salvan por esa lista — mover `reviews/` fuera del root.
- **Gates propuestos:** fallo de build si `*TEST*` en staged/`units[]`; blocklist `*.stereo_backup,*.bak,*.png`; tope por `.ogg`; 0 `*View*` sin guard; staged con 0 `.jsonl/.err/.md`; aserción versión única (ya existe el bump 1.2.4, falta el assert).

## 6. Propuestas adicionales (no pedidas, baratas)

1. `MAX_WIRES_PER_OWNER_CLIENT=128` (`Defines.c:220`) vs `MAX_WIRES_PER_DEVICE=64` (`:18`) es intencional (techo vanilla, comentario `:212-220`) — no unificar; la auditoría vieja que lo pedía está desfasada.
2. `S1_PROBE` fuera de `LFPG_PERFDIAG_ENABLED` (~16 líneas/RPT por apertura) y `LFPG_UIScaler` ausente en V4 (panel desproporcionado fuera de 1080p) — pendientes conocidos, incluir en la base común de vistas.
3. `OnKeyPress/OnKeyRelease` consumen teclas sin `super` con UI activa (`MissionInit.c:170-244`) — puede robar hotkeys a otros mods/admin tools; reenviar lo no consumido.
4. `Widget.SetLV(0)/SetTextLV(0)` globales (`:140-141`) — acotar a raíces LFPG o restaurar al cerrar.
5. `RepackCargoInPlace`: avisar en UI cuando `n>64` en vez de fallar silencioso; `TryPlaceOnGrid:1339` con early-out de área.
6. Documentar en cabecera de `LFPG_ElecGraph.c` POR QUÉ vive partido el split 4_World/5_Mission (arena de compile, incidente julio) para que nadie lo "simplifique".
7. Congelar por comentario los sistemas cerrados (BTC, CCTV) como `feature-frozen` para excluirlos de refactors.

---

## 7. Puntuación por partes (sobre 10)

| Parte | Nota | Bien | Mal / mejorar |
|---|---|---|---|
| Scheduler y armonización server | 8 | 10 lanes en 100 ms, fases escalonadas, early-outs | U1-U5: doble celdas, sesiones a 10 Hz, one-shots con leak, censo repetido |
| Arquitectura servidor (NM) | 5 | Registros/timers centralizados, presupuestos en Defines | Dios de 7.000 ln y 20 responsabilidades; partir por dominio |
| ElecGraph | 7 | Budgets 64/256, warmup, validación zombie, falla ruidoso | Complejidad accidental (epochs, fallbacks); amortizar sync en bulk |
| RPC server/cliente | 6 | Rate-limit 2 capas, cooldowns, debounce preview | alloc+strings+sqrt pre-validación; 11 SubIds espejo TEST; sin idempotencia global |
| Renderer cables/láser | 7 | LOD, oclusión presupuestada, budgets decoradores, ultra-LOD cacheado | sway invalida caché; sorts O(n²); invariantes por subsegmento; láser sin caché |
| Integración cliente (OnUpdate) | 6 | Gates por productor, throttles parciales | 6 GetPlayer + 3-4 inventario/frame; 5 timers GUI; SetPos/GetScreenSize por frame |
| Sorter V3 | 6 | Presupuestos tick, preview con cap/debounce, BinPack acotado | Repack con 9 allocs + insertion O(n²); resoluciones lineales; dedup con Find |
| Fork TEST | 3 | Patrón entidad (`Sorter_TEST:30-43` hereda, bien) | Fork de 4.600 ln + layouts + RPCs; divergente y spawneable; sacar o basar |
| BTC/economía | 5 | Sesiones/nonce, reconciliación, catálogo validado | God-helper 2,6k ln, 8 mapas de claims, registries gemelos, 10% del mod |
| Dispositivos/persistencia | 7 | Base centralizada, `deviceVer` con rango, super-primero | Reflexión muerta, migradores no-op, hooks vacíos, 4 wrappers sin fast-path |
| Acciones | 5 | Conditions con `DistSq`, cooldowns | Triplicación switch/speaker/~30 moldes; validación server desigual |
| Assets | 3 | `.rvmat/.ptc` sanos, `$PBOPREFIX$` ok | `.p3d` de 40 MB, 230 MB en gates+lids, dup `.paa`, junk, `water.ogg` 5,5 MB |
| Config/build | 5 | Versión 1.2.4 coherente, `units[]` sin TEST | `include.lst` sin `p3d/cpp/cfg`, `reviews/` en el árbol, sin gates de TEST/junk/versión |
| Higiene (TODOs/guards/logs) | 7 | 4 TODOs en 72k ln, 1.651 guards, diag flagueado | `ToString` pre-log en caliente (816 hits), 421 Prints, 117 syncvars sin auditar una a una |

**Nota global ponderada: ≈5,5/10.** El código es honesto y está endurecido por capas de fix reales (scheduler, presupuestos, validación, sesiones); su deuda es estructural (duplicación TEST, dios NM, assets) no de corrección. El orden que más rinde: TEST fuera del release → techo assets → U1-U6 → partir NM → quitar reflexión.
