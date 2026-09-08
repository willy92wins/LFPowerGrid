Voy a leer el brief y las skills que manda, y luego ejecutar el encargo de principio a fin.El brief pide un plan de ejecución ciego: primero cruzo la auditoría con el trabajo ya triado y luego armo las lanes. Empiezo por las fuentes obligatorias.Tengo la auditoría y el plan vigente. Siguiente: triajes, dictámenes y los 7 CONFLICTO, para cruzar cada hallazgo con lo ya cubierto.La auditoría es del 7-sep, con V3 aún viva. Voy a comprobar el árbol actual y el resto de fichas para no cruzar deuda ya jubilada.La UI V3 ya no está; el fork `_TEST` es el panel de producción. Verifico en el árbol actual las citas que cargan el solape y las lanes.Hay un hueco importante en el encuadre: tras jubilar la UI V3, `_TEST` ya no es un fork experimental, es el panel de producción. El resto del plan sale de ese cruce. Como estoy en Ask mode, no puedo escribir el fichero; aquí va el `PLAN.md` completo.

---

# PLAN.md

Lane: grok. Árbol leído: `P:\LFPowerGrid` (alias del repo compilable). No abrí ningún `PLAN.md` de `<RUN>\lanes\`.

## A. CRITICA DEL ENCUADRE

El encargo acierta en una cosa y se equivoca en el palanca que más cambia el tamaño.

**Acierto.** Cruzar la auditoría del 7-sep contra el trabajo ya triado *antes* de repartir es el paso correcto. Sumar 46 + 92 + 9 MEDIO + 7 CONFLICTO ≈ 170 es un inventario con doble conteo, no un plan. El precedente de negarse a borrar `LFPG_Sorter.c` sigue aplicando: una extracción o un rename «obvio» puede romper World y las bases.

**La palanca `_TEST` fuera del PBO ya no existe como opción de producto.** La auditoría S2 (`AUDITORIA-2026-09-07.md:26-28`) y la interpretación del orquestador asumen un mundo con **dos** UIs: V3 de producción y V4 `_TEST`. Ese mundo murió el 8-sep. En el árbol actual:

- No queda `LFPG_SorterView.c` ni `LFPG_SorterController.c`. El panel vivo es `scripts/4_World/test/LFPG_SorterView_TEST.c` y su controller.
- `LFPG_Sorter.SetActions` registra solo `LFPG_ActionOpenSorterPanel_TEST` (`scripts/4_World/LFPG_Sorter.c:111`). La acción hace `LFPG_Sorter.Cast` (`scripts/4_World/test/LFPG_ActionOpenSorterPanel_TEST.c:50-52`), así que una entidad `LFPG_Sorter` ya colocada abre el panel V4.
- `LFPG_Sorter_TEST` es una subclase vacía (`scripts/4_World/test/LFPG_Sorter_TEST.c:15-17`) conservada por persistencia (`config.cpp:1080-1086`).

Sacar `_TEST` del PBO ahora dejaría **cero panel de sorter** en el release. No «muchas fichas `*_TEST.c` dejan de importar»: dejarían de importar 4 VIVA de UI (`S11 S20 S21 S23`) a costa de romper el producto. Las 14 VIVA compartidas (`S01`–`S09` menos las ya cerradas, `S12`–`S19`, `S24`) viven en `Sorter.c` / `SorterData.c` / `SorterLogic.c` / `NetworkManagerImpl.c` / `RPCServerHandlerImpl.c` y **sobreviven** a cualquier destino del classname `_TEST`.

**La decisión que sí cambia el tamaño es el journal de dinero**, no el fork. Los 7 CONFLICTO están todos en el camino del saldo (`l7-debt-v3/INFORME-ASTRA.md:6-22`) y chocan con T1. Aceptarlos como riesgo (como E04) saca 7 ítems gordos del plan de código. Pedir cierre atómico con hive **añade** un tramo largo, no lo reduce, y exige el inyector T5 que nunca se ha armado (`fuentes/HANDOFF.md:164-165` del bloque vivo).

**Partir `LFPG_NetworkManagerImpl.c` no es el primer movimiento, es el que impide el primero.** El cuello de paralelismo es real (el fichero sigue siendo el dios: `BroadcastOwnerWires` en `:1994`, `LFPG_TickSorters` en `:5918`, `CutAllWiresFromDevice` en `:4733`). Extraerlo por dominio (S1) es un refactor de ~7.000 líneas que no cierra G06 ni S15 y concentra el riesgo sobre jugadores reales. Las fichas de ese fichero se parchean **en serie, in situ**. El split, si alguna vez, es *después* de que esas fichas estén verdes, no la condición para empezar.

**No fusionar `archive/t2-autoridad-servidor`.** El brief la cita como 1.162 inserciones nunca compiladas. `main` ya tiene T2 por la lane `l4` (SEC01/02/03/20 en `RPCServerHandlerImpl.c`, dictamen VERDE). Un merge de aquella rama sobre este `main` no es «terminar T2»: es reintroducir un paralelo histórico. Lo que queda de T2 es in-game (V1–V7, SEC01 recuento) y dos MEDIO residuales del propio `l4`, no aquellas 1.162 líneas.

**`PLAN-CONJUNTO.md` no se re-ejecuta.** Su T0b (borrar 7 clases V3, cablear RPC 65/70, UIScaler, gatear `S1_PROBE`) está hecho en sustancia: UIScaler vive en V4 (`LFPG_SorterView_TEST.c:83-110`), `RunS1Probe` está detrás de `LFPG_PERFDIAG_ENABLED` (`:1652-1654`; cero hits de `S1_PROBE` suelto), la acción V4 alimenta `LFPG_Sorter`. Las secciones bajas del HANDOFF que aún pintan T0b como pendiente son sedimentación; manda el primer bloque hasta «Cola que queda» (`fuentes/HANDOFF.md:93-100`).

**Orden que sí rinde:** (1) cruzar auditoría —esto— (2) dueño: journal de dinero sí/no, y *no* borrar classnames `_TEST` (3) cuatro lanes de daño a jugador en ficheros disjuntos (4) el resto. El linter offline y el arranque verde no autorizan a tratar el backlog como «ya compilado, luego es seguro refactorizar el dios».

## B. SOLAPE AUDITORIA <-> TRABAJO YA TRIADO

Universo de la auditoría: 34 IDs numerados + 5 viñetas §5 + 7 propuestas §6 = 46. Citas de código re-leídas en este árbol, no copiadas de la auditoría (rama `a187898`; el HEAD del triaje era `d59cad8` y el HANDOFF vivo cita `adfd29c` — ver G).

Leyenda de `cubierto_por`: ID de ficha triada; `MEDIO-*` residual de oleada; `CONFLICTO-*`; `YA-CERRADO` (trabajo 8-sep posterior a la auditoría); `NUEVO`; `NO-HACER`; `STALE` (describe el árbol del 7-sep, no el de hoy).

| hallazgo | cubierto_por | evidencia (path:line) |
|---|---|---|
| S1 Dios NM ~20 responsabilidades | NUEVO como extracción; **no entra**. Bugs internos ya triados (G03–G13, G21–G23, S05/S06/S24, SEC05, H1–H6, U1/U3/U5) | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:1994` `BroadcastOwnerWires`; `:4733` `CutAllWiresFromDevice`; `:5918` `LFPG_TickSorters`; `:5459` `LFPG_TickWaterPumps`; `:7027` `LFPG_RebuildPlayerCells` |
| S2 Fork TEST divergente | **STALE en la mitad V3.** T0b jubiló la UI V3. No hay segundo View/Controller de producción. Resta: classnames `CfgVehicles` + sufijo `_TEST` en UI + 11 SubIds espejo. Cubierto como T0c/`S11 S20 S21 S23`, no como «sacar del PBO» | `scripts/4_World/test/LFPG_Sorter_TEST.c:15-17` subclase vacía; `config.cpp:1080-1086`; `scripts/4_World/LFPG_Sorter.c:111`; glob: no existe `LFPG_SorterView.c` |
| S3 Reflexión `Call*` | **D10** (los 4 wrappers sin fast-path). El resto de `Call*` con fast-path Dual es el mismo fichero | `scripts/4_World/LFPG_IDevice.c:433-482` `CallInt`/`CallFunctionParams`; `:507` `GetPortWorldPos`; `:840` `GetPortName`; D10 en `TRIAJE-dispositivos.md:9` |
| S4 Ecosistema BTC ~10 k ln | NUEVO como feature-freeze/split del helper. **No entra** (F). Solapa SEC12–14 y los 7 CONFLICTO, que sí entran o se aceptan | `reviews/2026-09-08-backlog-paralelo-oleada2/l7-debt-v3/INFORME-ASTRA.md:6-22`; `TRIAJE-red-seguridad.md:12-14` SEC12–14 |
| S5 Acciones/chrome ×3 | **PARCIAL D18** (switches). `ActionToggleBase` NUEVO — no entra. Chrome: `LightenARGB`/`IsEscCooldown` siguen duplicados ATM vs sorter | `scripts/4_World/LFPG_BTCAtmView.c:1043` y `:1181`; `scripts/4_World/test/LFPG_SorterView_TEST.c:1924` y `:1478`; D18 `TRIAJE-dispositivos.md:16` |
| S6 Capas no-op | **SEC18** migrators (hoy **cero llamadores**). Los hooks vacíos de `DeviceBase` son el contrato de extensión, no esqueletos a borrar | `scripts/3_Game/LFPG_Migrators.c:39-64`; grep `LFPG_Migrators`/`MigrateBlob` solo ese fichero + comentario `LFPG_Defines.c:253`; `scripts/4_World/lfpg_devicebase.c:575-587` |
| S7 `Defines.c` mezcla + estados futuros | NUEVO «no añadir reservas». CableState 9 vs 3 usado: comentario vivo. No hay ficha. **No partir el fichero** | `scripts/3_Game/LFPG_Defines.c:81-95` |
| H1 `BroadcastOwnerWires` doble barrido + JSON | **G12** (segundo `GetPlayers`); **G13** es el pariente delta/vanilla | `:2014` y `:2071` ambos `g_Game.GetPlayers`; `:2063` `GetWiresJSON` entre medias; `TRIAJE-grafo.md:12` G12 `:2069` |
| H2 `CleanDisappearedVanillaDevice` scan por prefijo | **NUEVO** (mecanismo distinto de G03, que es seguimiento de fuentes vanilla OUT) | `:3401` función; `:3470-3476` `keyPrefix` + `IndexOf(keyPrefix)` sobre `m_ReverseIdx` |
| H3 `CutAll` fallback O(V·W) | **PARCIAL.** Sigue el fallback, gateado por índice inconsistente. No es G06 (rebuild de baterías ajenas) | `:4958-4962` `if (!reverseIndexConsistent)` + `GetAll`; G06 es `:5088` `CallLater(PostBulkRebuildAndPropagate` — `TRIAJE-grafo.md:7` |
| H4 Tick sorter: 6 resoluciones + sqrt + Find | **S05 S06 S07 S24.** `vector.Distance` (sqrt) **sigue** en el tick. S08/H9 ya acotado | `:6017` `linkDistance = vector.Distance(...)`; S05 `:6077` en `TRIAJE-sorter.md:8`; S08/H9 `LFPG_SorterLogic.c:1132-1136` tope 64 |
| H5 `TickWaterPumps` triple barrido | **NUEVO residual.** Ya no es `GetAll+Cast` (usa registries). Falta early-out si t1+t2==0 / ningún pump con power. U5 | `:5459-5504` reset de **todos** los sprinklers antes de degradar T1/T2; comentario `:5485` «Replaces GetAll+Cast» |
| H6 Celdas jugadores O(P·C) + doble rebuild | **U1** (mismo doble llamador). No es D12 (Intercom `GetAll`) | `:6971` desde sprinklers; `:7231` desde detección; no hay `m_PlayerCellsBuiltThisTurn` |
| H7 `RebuildFromWires` + sync 3 resoluciones/nodo | **G08 G10 G14 G20** | `TRIAJE-grafo.md:9-11,15,19`; `LFPG_ElecGraphImpl.c` citas del triaje |
| H8 `HandleFinishWiring` alloc+ToString+sqrt pre-validar | **PARCIAL SEC07 + SEC17.** Rate-limit/startup sí van antes (`:220-235`). Strings de payload en Debug `:264-267`. `vector.Distance` **después** de resolver objetos `:340-341` | `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:214-341`; SEC07 `:311`; SEC17 `:465` |
| H9 `RepackCargoInPlace` 9 `new` + insertion | **YA-CERRADO / MEDIO-l3-S08.** Pico acotado; empaquetado completo no garantizado. §6.5 avisar UI si n>64 | `scripts/5_Mission/LFPG_SorterLogic.c:1132-1136`; `l3-t4-render/DICTAMEN-GROK.md:9-12` |
| C1 Sway invalida caché | **NUEVO** (R10 nota la clave con sway pero el defecto es FOV/roll, no el sway). `screenCacheSwayY/X` se guardan y el Sin sigue por wire/frame | `scripts/4_World/LFPG_CableRenderer.c:3227-3228` `Math.Sin`; `:3304-3305` cache incluye sway |
| C2 Tres pasadas O(n)/frame | NUEVO / solapa R05 `m_DrawOrderDirty`. **No entra** hasta perfil | `:2865` gate de sort; auditoría `:2921-2952` (líneas de `a187898`, orden actual distinto) |
| C3 Invariantes por subsegmento | **NUEVO.** `margin` se recalcula dentro del loop de segs | `LFPG_CableRenderer.c:3344-3347` `margin = shF * LFPG_SCREEN_MARGIN_RATIO` por segmento |
| C4 `ClipSegToScreen` solo en miss | **NO-HACER** (la propia auditoría: bien diseñado) | auditoría `:73`; confirmado comentario `:3340-3343` |
| C5 Player-occlusion 2 `GetScreenPos`/frame | **NUEVO.** Sigue el coste declarado | `LFPG_CableRenderer.c:2817-2818` `plFeetScr`/`plHeadScr` |
| C6 Selection sort O(n²) gateado | **R05** | `TRIAJE-render-ui.md:7`; sort `:2719+` según triaje |
| C7 Oclusión presupuestada | **NO-HACER** | auditoría `:76` |
| C8 CullTick 1 sqrt/wire | **PARCIAL R04/R08.** DistSq en movimiento de devices del manager (`NetworkManagerImpl.c:3223`). CullTick renderer: R08 early-out por origen | `TRIAJE-render-ui.md:10` R08 `:2382`; R04 MEDIO reentrada `DICTAMEN l3:14-17` |
| C9 Preview sesión / sag | **NO-HACER** | auditoría `:78` |
| C10 Inspector `GetScreenSize`+`SetPos`/frame | **PARCIAL R12/R13/R28** (datos/offsets). El `GetScreenSize` por frame **sigue** — NUEVO residual de C10 | `scripts/4_World/LFPG_DeviceInspector.c:1641` `GetScreenSize`; R12 `TRIAJE-render-ui.md:12` |
| C11 Láser sin caché de proyección | **NUEVO + R18 R19** (altas/frustum). `GetScreenPos` por beam/frame intacto | `scripts/4_World/LFPG_LaserBeamRenderer.c:264-265`; R18/R19 `TRIAJE-render-ui.md:18-19` |
| C12 Joints ×4 `DrawLine` | **NUEVO.** Sigue el diamante de 4 líneas | `scripts/4_World/LFPG_CableHUD.c:349-352` |
| U1 Un rebuild de celdas por turno | **= H6.** NUEVO de unificación (~10 líneas). Ninguna ficha lo nombra con este ID | `:6971` y `:7231` |
| U2 Sesiones a 1 s + fusionar gemelos | **PARCIAL SEC08** (tick 100 ms, foco sin plazo de distancia). Fusionar BTC+Control registries: NUEVO — **no entra** (SEC14 replay ≠ ControlSession) | `NetworkManagerImpl.c:621-622` `m_ControlSessions.Tick()` cada 100 ms; `TRIAJE-red-seguridad.md:10` SEC08 `:404`; timeout 125 s citado en dictamen l7 `:78` |
| U3 One-shots al scheduler (leak `CallLater`) | **NUEVO.** Los five `CallLater(SYSTEM)` siguen. `StopServerScheduler` solo para el `Timer` | `:504` Validate 5 s; `:3747` self-heal 500 ms; `:3994` prune 30 s; `:5089` PostBulk 1 ms; `LFPG_MissionInit.c:55` orphan sweep. Stop `:606-616` |
| U4 Cola diferida + coalescencia | **D09** | `TRIAJE-dispositivos.md:8`; `LFPG_RemoteController.c:539-560,:708`; `LFPG_MemoryCell.c:184,:267` |
| U5 Censo único por turno | **NUEVO.** Hermano de H5. No hay `m_TurnCensus` | early-out sorter sí (`:5965-5968` si `total==0`); pumps no |
| U6 Cliente: 1 tick + nearby + 1 player/frame | **PARCIAL R16 R30 + MissionInit.** `GetPlayer` sigue por rama de UI. NUEVO como unificación | `LFPG_MissionInit.c:242-244` Cast player si sorter abierto; `:140` `SetLV`; R16 TankHUD; R30 `TRIAJE-render-ui.md:32` |
| §5 techo `.p3d` / `data/` ≤150 MB | **NUEVO** de arte. **No entra** en lanes de script (F). No pude censar `.p3d` en este checkout (G) | auditoría `:99`; glob `**/*.p3d` = 0 hits aquí |
| §5 junk png/ogg/stereo_backup | **NUEVO** si sigue en disco. glob `data/**/*.png` = 0. **INVESTIGAR**, no borrar a ciegas | auditoría `:100`; este checkout no lista esos binarios |
| §5 duplicados SHA256 / `.rvmat` | **NUEVO** de assets. **No entra** | auditoría `:101` |
| §5 compile: `include.lst`, TEST en server, `units[]` TestLamp, `reviews/` | **`include.lst` NUEVO — NO «arreglar» sin medir PBO** (el mundo carga con modelos, `:59-61` HANDOFF). TestLamp en `units[]` NUEVO. `reviews/` en el repo no entra al PBO si el packer usa la whitelist | `include.lst:1` `*.c;*.asi;…` sin `p3d/cpp/cfg`; `config.cpp:187` `units[]` incluye `LF_TestLamp`; `:256-263` las clases |
| §5 gates TEST/junk/versión | **NUEVO** de tooling (`LFPowerGrid_dev` / CI). No es Enforce. Fuera de este repo salvo un chequeo de `units[]` | auditoría `:103` |
| §6.1 `MAX_WIRES_PER_OWNER_CLIENT=128` vs 64 | **NO-HACER** (intencional) | auditoría `:107` |
| §6.2 `S1_PROBE` + `LFPG_UIScaler` ausente en V4 | **YA-CERRADO.** Probe gateado; scaler portado (S22 MUERTA; D-02 de T0b ARREGLADA). Resta el MEDIO-l5 de preview flicker, no el scaler | `LFPG_SorterView_TEST.c:1652-1654` `if (LFPG_PERFDIAG_ENABLED) RunS1Probe()`; `:83-110` scaler; `TRIAJE-sorter.md:24` S22; `l5 DICTAMEN:37-40` |
| §6.3 `OnKeyPress` sin `super` con UI | **PARCIAL, y es el diseño.** Con CCTV/sorter/ATM abierto traga teclas (`return` sin super). Sin UI sí llama `super` | `LFPG_MissionInit.c:168-200` |
| §6.4 `Widget.SetLV(0)` global | **R30** | `LFPG_MissionInit.c:140`; `TRIAJE-render-ui.md:32` |
| §6.5 Aviso UI si repack n>64 | **MEDIO-l3-S08** (producto: mejor-esfuerzo). NUEVO el aviso en UI | `LFPG_SorterLogic.c:1133`; dictamen l3 `:9-12` |
| §6.6 Documentar split 4_World/5_Mission en `LFPG_ElecGraph.c` | **NUEVO.** La cabecera explica el grafo, **no** el incidente de compile | `scripts/4_World/LFPG_ElecGraph.c:1-20`; `PLAN-CONJUNTO.md:48-50` y AGENTS.md §2 |
| §6.7 Comentarios `feature-frozen` BTC/CCTV | **NUEVO** ceremonia. **No entra** | auditoría `:113` |

**Conteo (silencioso, resultado):** de 46 hallazgos, ~28 están cubiertos por ficha/MEDIO/CONFLICTO/cierre 8-sep; ~6 son NO-HACER o STALE; ~12 son NUEVO de verdad. De esos 12, el plan **ejecuta** H2, H5, U1, U3, U5, C1, C3, C5, C10 residual, C11, C12, §6.6 (comentario). Deja fuera S1 split, S4 freeze, S5 genéricos, S7 split Defines, assets pesados, gates de packer, §6.7.

La interpretación del orquestador («fracción grande ya cubierta, <<170») **se sostiene**. El número útil de trabajo nuevo de la auditoría es del orden de **una docena**, no 46.

## C. LA DECISION DE PRODUCTO

**Veredicto: no sacar `_TEST` del release. No borrar ni renombrar `LFPG_Sorter_TEST` / `LFPG_Sorter_TEST_Kit` en `CfgVehicles`.** Conservar classnames (`config.cpp:1080-1086`, `LFPG_Sorter_TEST.c:1-4`). El panel de producción *es* el código `_TEST`. T0c como rename masivo de clases UI + 11 SubIds + layouts **no se hace en esta oleada**: riesgo de `FindAnyWidget` / stringtable / RPC sin ganancia de jugador (AGENTS.md §4).

T0c-lite, si el dueño quiere dejar de ver «[V4 TEST]» en el inventario: cambiar solo `displayName`/`descriptionShort` en `config.cpp:1083-1090`. Eso **no** rompe persistencia.

### Consecuencia numérica (fichas P2/P3 sorter = 22; 18 VIVA + 3 DUDOSA + 1 MUERTA)

| decisión | VIVA sorter que desaparecen | VIVA sorter que quedan | efecto de producto |
|---|---|---|---|
| A. Sacar UI `_TEST` del PBO (opción S2 de la auditoría) | 4 (`S11 S20 S21 S23`) + 1 DUDOSA `S10` | 14 compartidas | **Sin panel.** V3 UI ya no existe. Inaceptable |
| B. Borrar/renombrar classnames `CfgVehicles` `_TEST` | **0** | 18 VIVA | Rompe mundos con `LFPG_Sorter_TEST` colocado |
| C. T0c rename de clases UI/RPC/layouts, classnames intactos | **0** | 18 | ~8 ficheros extra en serie; bloquea L-SORTUI; cero bugs cerrados |
| D. **Elegida:** classnames y nombres de clase UI se quedan; `displayName` opcional | **0** | 18; se **arreglan** las 4 de UI + las compartidas que el plan priorice | Panel sigue; persistencia intacta |

`S10` / P0.1: dos lanes independientes no localizaron la divergencia de preview (`fuentes/HANDOFF.md:97-100`; `TRIAJE-sorter.md:12`; `l5 DICTAMEN:43`). **No es una decisión de código.** Se archiva.

### La decisión que sí mueve el plan: los 7 CONFLICTO de dinero

| si el dueño… | ítems de código | tamaño |
|---|---|---|
| Acepta la ventana crash (mismo criterio que E04: fallar a duplicado peor que a pérdida, o riesgo archivado) | **−7** CONFLICTO del plan de implementación; quedan como deuda citada | El plan de script se encoge más que con cualquier destino `_TEST` |
| Exige cierre atómico RAM/disco/hive | **+7** en una sola lane L-MONEY (`BTCHelper.c` + `BalanceProvider_NativeImpl.c` + `FileUtil.c`) | Semanas; gate = T5 armado + kill entre cada par de I/O. Hoy T5 **nunca se ha ejecutado** |

Recomiendo **no abrir L-MONEY hasta que el dueño firme una de las dos**. Sí abrir **SEC10** (backup ilegible, `FileUtil.c:339`) en L-FILE *antes* de ese journal: es el mismo fichero, pero el arreglo de lectura no es el protocolo de escritura V3-06.

### Qué no es la palanca, aunque lo parezca

- Extraer NM (S1): 0 fichas de jugador cerradas el día del split; mata el paralelismo durante el split.
- Base común View/Controller (S2 original): con una sola UI, el extract es vanity.
- Fusionar `archive/t2`: 0 fichas nuevas; riesgo de regresionar el T2 ya en `main`.

## D. EL PLAN: LANES

Dueño primero (`L-OWNER`). Después, lanes de Codex **disjuntas por fichero**. Si dos ítems comparten fichero, van en la misma lane o en oleadas sucesivas de esa lane — nunca en paralelo.

### L-OWNER — no es código

| campo | valor |
|---|---|
| id | L-OWNER |
| objetivo | Firmar: (1) classnames `_TEST` se quedan (2) T0c rename de UI **no** (3) 7 CONFLICTO: aceptar o journal (4) no merge `archive/t2` (5) `displayName` TEST→producción sí/no |
| ficheros exclusivos | ninguno |
| items que cierra | P0.1/S10 archivada por no-localizada; T0c reducido a opcional displayName |
| gate | Respuestas por escrito en el HANDOFF vivo. Sin esto, L-MONEY no arranca; el resto sí |
| depende de | — |

### Oleada 1 — daño a jugador, 8 lanes simultáneas

| id | objetivo | ficheros exclusivos | items que cierra | gate de aceptación | depende de |
|---|---|---|---|---|---|
| **L-NM** | Integridad de grafo/cuotas/seguimiento en el dios, *sin extraerlo* | `scripts/5_Mission/LFPG_NetworkManagerImpl.c` **solo** | Oleada 1a: **G06 G07 G03**. Luego 1b (misma lane, después): **G05 G23**. Luego 1c: **G12/H1 G13**. Luego 1d: **S05 S06 H4-sqrt S24**. Luego 1e: **H2 H5 U1 U3 U5 SEC05** | Lectura: `DeviceHasAnyWires` contempla salidas vanilla (`GetVanillaWires`); `PostBulkRebuild` no pone `m_VirtualGeneration=0` en baterías de otros componentes; poda diferida llama recuento de creador; `RemoveOrdered` en FullSync; un solo `GetPlayers` por broadcast; tick sorter usa `DistSq`; flag `m_PlayerCellsBuiltThisTurn` en `:618` y los dos llamadores; `CallLater` de validate/self-heal/prune/orphan viven como fases del scheduler o se `Remove` en `StopServerScheduler`. Linter: `len(errors)==0` delta vs base. In-game G06: corte local no apaga batería ajena — **pendiente** (MCP no completa acciones continuas, brief §4) | L-OWNER (1)(4) no bloquean el código; no espera journal |
| **L-GRAPH** | Carga de baterías y rebuild honesto | `scripts/5_Mission/LFPG_ElecGraphImpl.c`; comentario en `scripts/4_World/LFPG_ElecGraph.c` (§6.6) | **G08** primero; **G09 G10** si el arreglo de rebuild toca admisión; **G11 G14 G19** de paso si el mismo hunk los pisa. **No** G15/G16/G20 como refactor de mapas | Lectura: carga de batería no depende de vaciar la cola global ajena (`TRIAJE-grafo.md:9` `:3175`). Cabecera de `LFPG_ElecGraph.c` explica *por qué* vive el split 4_World/5_Mission (arena de compile). `len(errors)==0` | — |
| **L-RPC** | Preview no lee cargo protegido; logs no concatenan IDs crudos | `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c` **solo** | **S14 S03**; **SEC07** (saneo del `wireId` en Warn). **S12** (ACK con entidad/revisión) en el mismo fichero, segunda pasada. **No** en esta pasada: SEC04 rescate global (AMPLIO), MEDIO techo-de-almacén, MEDIO barrido autoritativo — quedan anotados para pasada 2 del mismo fichero | Lectura: `HandleSorterPreviewRequest` aplica los mismos guards de acceso que sort (`TRIAJE-sorter.md:16` `:2956`). Save y Preview mismo tope de bytes (`:2668` vs `:2899`). `ValidateWaypoints` no imprime el string del cliente crudo. `len(errors)==0` | S13 es decisión de producto (promesa de preview); si el dueño no la firma, no se «arregla» el simulador que no existe |
| **L-FILE** | Target ilegible no deja sin cables/saldos si hay backup | `scripts/3_Game/LFPG_FileUtil.c` **solo** | **SEC10**. **No** V3-06 ni SEC16/19 en esta pasada | Lectura: si el target existe y el parseo falla, se intenta el backup (`TRIAJE-red-seguridad.md:11` `:339`). Control de lectura (no hace falta juego): fixture de target truncado + backup válido. `len(errors)==0` | No espera L-OWNER-journal. **Bloquea** V3-06 sobre el mismo fichero |
| **L-SORTCORE** | Vínculo y parser del sorter (entidad V3/V4 compartida) | `scripts/5_Mission/LFPG_SorterLogic.c`; `scripts/3_Game/LFPG_SorterData.c`; `scripts/4_World/LFPG_Sorter.c` | **S15** primero; **S01 S02**; **S16 S17** en la misma entidad. **S19** si el hunk de clasificación está abierto. **No** S07/S18/S24 (coste / API nativa incierta) | Lectura: destino y origen manual usan el mismo chequeo de distancia que el origen del tick (`SorterLogic.c:1012` vs tick `NetworkManagerImpl.c:6017` — el tick es L-NM 1d; **S15 destino/manual es este fichero**). `FromJSON` no publica reglas parciales; vacío válido se puede guardar (`SorterData.c:594`). `len(errors)==0` | No espera L-NM para S15 destino; el DistSq del *tick* sí es L-NM |
| **L-KIT** | Salud dispositivo↔kit | `scripts/4_World/LFPG_ActionDismantleDevice.c`; `scripts/4_World/LFPG_KitBase.c`; `scripts/4_World/LFPG_KitBaseDeployable.c` | **D06**. Comentario mentiroso MENOR-l1 `:27` de la acción, de paso | Lectura: desmontaje copia salud (y el inverso al desplegar) en las dos bases. In-game **pendiente**: `action_use` no completa barras (brief §4) | — |
| **L-RENDER** | Cables que desaparecen / oclusión de esquinas / cachés sucias | `scripts/4_World/LFPG_CableRenderer.c`; `scripts/4_World/LFPG_CableParticle.c` (solo si R01 obliga); `scripts/4_World/LFPG_CableHUD.c` (R09/R27 señal de canvas) | **R08 R01 R03** primero. Luego en el mismo fichero: **C1 C3 C5**, **R09**, MEDIO-l3-R04 reentrada del reconciliador. **No** R06/R07/R23 rewrite de representación | Lectura: CullTick no oculta por distancia al *owner* sin probar el extremo cercano (`TRIAJE-render-ui.md:10`). `BuildOccSamples` corre **después** de llenar joints (`:2288` vs `:2299` en el triaje). Purga invalida las dos cachés derivadas. Sway no entra en la clave de reuse *o* se cuantiza a 50–100 ms (C1). `len(errors)==0` | — |
| **L-CCTV** | Abortar cámara ajena no echa al operador | `scripts/4_World/LFPG_Camera.c`; `scripts/4_World/LFPG_CameraViewport.c` | **R14**. **No** R15 in-game ni el MEDIO-l3 de predicado circular en esta pasada (mismo fichero: **segunda** pasada tras R14, no paralela) | Lectura: `SafeAbort` identifica la entidad de la sesión (`Camera.c:100-107` vs `CameraViewport.c:249`). Una cámara distinta no pone fase 1. `len(errors)==0`. In-game R15 sigue abierto (HANDOFF `:81-83`) | — |

### Oleada 2 — mismo criterio, otros ficheros (pueden arrancar el día 1 *si* hay más agentes; no pisan oleada 1)

| id | objetivo | ficheros exclusivos | items | gate | depende de |
|---|---|---|---|---|---|
| **L-SESS** | FIFO de respuestas BTC | `scripts/5_Mission/LFPG_BTCSessionRegistry.c` | **SEC12**; **SEC13** si el hunk de watermark está abierto. **No** SEC14 (purga vs replay: decisión de seguridad) | Lectura: cierre no usa `Remove(0)` (`:375`). `len(errors)==0` | — |
| **L-CTRL** | Foco no se reserva al infinito al alejarse | `scripts/5_Mission/LFPG_ControlSessionRegistry.c` | **SEC08** (distancia en el tick que ya corre). No fusionar con BTCSession (U2-merge) | Lectura: tick comprueba distancia o deadline del searchlight (`:404` + lease V3-09 ya portado). `len(errors)==0` | — |
| **L-SORTUI** | Save/Sort no se quedan en vuelo; rail/chips | `scripts/4_World/test/LFPG_SorterController_TEST.c`; `scripts/4_World/test/LFPG_SorterView_TEST.c`; `scripts/4_World/test/LFPG_SorterTagView_TEST.c`; `scripts/4_World/test/LFPG_SorterPreviewRow_TEST.c` | **S11**; MEDIO-l5 `RefreshCargoPreview`; **S20 S21**; MENOR-l6 comentario de clone V3 (`View_TEST.c:34`). **No** rename de clases | Lectura: retorno sin ACK libera el bloqueo (`:1188`). Generación de preview descarta in-flight tras Clear (`:1709/:1768`). `ui_reconcile.py .` 0 FAIL/0 WARN (layouts). `len(errors)==0` | S12 (identidad ACK) es L-RPC pasada 2; L-SORTUI puede poner generación cliente **sin** esperar, pero el cierre entero de S11+S12 espera esa pasada |
| **L-IDEV** | Fast-path de puertos | `scripts/4_World/LFPG_IDevice.c` | **D10** (4 wrappers); **D21** solo si el mismo hunk toca selección por radio | Lectura: `GetPortWorldPos`/`GetPortName`/`GetPortDir`/`GetPortLabel` hacen Cast a `DeviceBase` antes de `Call*` (`:507+`). `len(errors)==0` | — |
| **L-LASER** | Proyección láser con caché | `scripts/4_World/LFPG_LaserBeamRenderer.c` | **C11**; **R18 R19** si el cull se toca | Lectura: no hay 2 `GetScreenPos`/beam/frame en miss de caché (`:264-265`). `len(errors)==0` | — |
| **L-INSPECT** | Inspector no miente ni pisa offsets | `scripts/4_World/LFPG_DeviceInspector.c` | **R12 R13**; C10 residual `GetScreenSize`; **no** R28 rewrite de setters | Lectura: filas de wires usan los mismos offsets que la cabecera (`:1286` vs `:1418`). Datos eléctricos caducan. Reusa tamaño de HUD si está listo. `len(errors)==0` | — |
| **L-REMOTE** | Mando: sync al nuevo dueño + coalescer CallLater | `scripts/4_World/LFPG_RemoteController.c`; `scripts/4_World/LFPG_MemoryCell.c` | **D08 D09** (parte mando + MemoryCell); **D23** si el hunk de min repetido está abierto. **No** D20 | Lectura: cambio de dueño llama al sync existente. Resets LED cancelan el `CallLater` previo. `len(errors)==0` | — |
| **L-INTER** | Intercom RF no barre el registro global *y* no reescribe visuals iguales | `scripts/4_World/LFPG_Intercom.c` | **D11 D12** | Lectura: RF no hace `GetAll` del registro entero para un alcance local (`:628`). Visuals con estado idéntico no reescriben materiales (`:380`). `len(errors)==0` | — |
| **L-REG** | Latch de ID ambiguo no oculta a los dos vivos en el barrido que T2 necesita | `scripts/4_World/LFPG_DeviceRegistry.c` | **MEDIO-l7** GetAll oculta vivos; **MEDIO-l7** latch sobrevive a los dos objetos | Lectura: el enumerado de safety de T2 sigue viendo ambos (`DICTAMEN-l7:5-22`). Quitar un duplicado no congela el sweep de huérfanos para siempre. `len(errors)==0` | No reabre V3-07 |

### Oleada 3 — solo tras L-OWNER si journal=sí; o higiene acotada

| id | objetivo | ficheros exclusivos | items | gate | depende de |
|---|---|---|---|---|---|
| **L-MONEY** | Integridad monetaria residual | `scripts/5_Mission/LFPG_BTCHelper.c`; `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c`; `scripts/3_Game/LFPG_FileUtil.c` (después de L-FILE) | 7 CONFLICTO: V3-02, V3-03, V3-05, V3-06, H-04, H-05, err=14 | **No se declara cerrado sin T5 armado** y kill entre cada par de I/O (A03 de PLAN-CONJUNTO). `len(errors)==0` es necesario y **no basta** | L-OWNER journal=sí; L-FILE terminada |
| **L-RPC-2** | Residuos T2 en el handler | `LFPG_RPCServerHandlerImpl.c` (misma exclusividad: **después** de L-RPC) | MEDIO techo de almacén; MEDIO barrido que tumba todo FinishWiring; SEC04 si hay señal de índice completo | Lectura: reemplazo al cap neta; fallo de un owner sucio no aborta el mundo. `len(errors)==0` | L-RPC oleada 1 |
| **L-NM-2** | Pasadas 1b–1e si no cupieron | el mismo `NetworkManagerImpl.c` | ver L-NM | ver L-NM | L-NM oleada 1a verde |
| **L-CCTV-2** | R15 predicado + R29 etiquetas | `CameraViewport.c` (+ `Camera.c` si el contrato de abort lo exige) | MEDIO-l3-R15; **R29**. In-game obligatorio para R15 | Tras `SelectPlayer`, o el predicado de `GetCurrentCamera()` es el documentado o se cambia. Escalado de etiquetas si cambia resolución | L-CCTV |
| **L-DMISC** | Dispositivos sueltos, un fichero cada uno | `LFPG_DoorController.c` D13; `LFPG_Furnace.c` D14/D22; `LFPG_MotionSensor.c` D17; `LFPG_BatteryAdapter.c` D24; `LFPG_ActionUpgradeWaterPump.c` + `LFPG_ActionUpgradeSolarPanel.c` D07 | D07 D13 D14 D17 D22 D24 | Lectura por ficha en `TRIAJE-dispositivos.md`. D17 no se «arregla» inventando IDs de LBmaster | — (D07 no comparte fichero con L-KIT) |
| **L-SET** | Settings fail-open | `scripts/3_Game/LFPG_Settings.c` | **SEC11** | Carga fallida restaura defaults o no muta `s_Settings` (`:538`). `len(errors)==0` | — |
| **L-TANK** | Sondeo negativo 4 Hz | `scripts/5_Mission/LFPG_TankHUD.c` | **R16** | Cadencia de miss más baja o registro de bombas. `len(errors)==0` | — |
| **L-MISSION** | SetLV + 1 player/frame | `scripts/5_Mission/LFPG_MissionInit.c` | **R30**; U6 *parcial* (cachear player/hasTool en `OnUpdate`) | `SetLV` acotado a raíces LFPG o retirado. Un `GetPlayer` por frame en el camino caliente. `len(errors)==0` | No en paralelo con un T0c rename (que no hacemos) |
| **L-UP** | (incluido en L-DMISC) | — | — | — | — |

Conflictos de fichero **declarados y resueltos:**

- `NetworkManagerImpl.c` → solo L-NM (oleadas internas).
- `RPCServerHandlerImpl.c` → L-RPC luego L-RPC-2.
- `FileUtil.c` → L-FILE luego L-MONEY (nunca a la vez).
- `CameraViewport.c` → L-CCTV luego L-CCTV-2.
- `SorterLogic.c` está en L-SORTCORE, **no** en L-NM (H9/S08 ya está; el DistSq del tick es L-NM).

## E. ORDEN Y PARALELISMO

**Simultáneas sin conflicto de fichero: 8 en oleada 1** (L-NM, L-GRAPH, L-RPC, L-FILE, L-SORTCORE, L-KIT, L-RENDER, L-CCTV). Eso es el techo útil: más agentes en oleada 2 (hasta **8 más**: L-SESS, L-CTRL, L-SORTUI, L-IDEV, L-LASER, L-INSPECT, L-REMOTE, L-INTER) si hay quota. L-REG y L-SET/L-TANK/L-MISSION/L-DMISC caben cuando quede sitio.

**Lo que bloquea a qué**

- L-OWNER (journal) → L-MONEY. No bloquea oleada 1.
- L-FILE → V3-06 dentro de L-MONEY.
- L-RPC pasada 1 → L-RPC-2 y el cierre conjunto S11+S12.
- L-NM 1a (G06/G07/G03) → L-NM 1b–1e. Nadie más espera a L-NM salvo S05/S06/S24 que *son* L-NM.
- L-CCTV → L-CCTV-2.
- In-game global: el puente no completa acciones de barra → **no** programar gates de desmontar/desplegar/cablear como si fueran automáticos. El arranque 12:01 ya demostró que el mundo carga; no se re-pide para cada lane (DZ-R5). Un solo ciclo in-game al cerrar oleada 1: panel V4 con sorter alimentado+enlazado (HANDOFF `:79-82`), más lo que se pueda sin barras.

**Qué no corre en paralelo nunca:** dos agentes sobre `NetworkManagerImpl.c`, `RPCServerHandlerImpl.c`, `FileUtil.c` o `CameraViewport.c`.

**Capacidad recomendada para un único revisor humano:** 4 simultáneas (L-NM + L-SORTCORE + L-FILE + L-KIT). Las otras 4 de oleada 1 solo si hay segundo revisor.

## F. LO QUE NO HARIA

- **Partir el dios NM (S1).** Coste/beneficio: semanas, merge hell, cero G06 cerrado el día 1. El paralelismo se obtiene *no tocando* ese fichero desde otras lanes.
- **Sacar `_TEST` del PBO o borrar classnames `CfgVehicles`.** Riesgo de persistencia + deja el sorter sin UI. AGENTS.md §4.
- **T0c rename de clases UI/RPC/layouts.** Cosmética; `FindAnyWidget` no lo ve el compilador; `ui_reconcile` ayuda pero el valor de jugador es 0.
- **Fusionar `archive/t2-autoridad-servidor`.** Main ya tiene T2 vía l4; aquella rama nunca compiló contra este árbol.
- **Feature-freeze + partir BTCHelper (S4) y fusionar session registries (U2-merge).** 10 k ln laterales; SEC12/13 se arreglan en el registry sin casar tipos. Freeze por comentario (§6.7) no cambia runtime.
- **`ActionToggleBase` genérico (S5) y D18 «familias de switches».** Mantenimiento; no es el daño de S15/G06/SEC10.
- **Techo de `.p3d` 10 MB / borrar gates de 40 MB.** Es arte, no Codex; hay jugadores con esos props colocados. Sin censo de `data/` en este checkout (G).
- **«Arreglar» `include.lst` añadiendo `*.p3d`.** El mundo carga con modelos. Si la lista fuera un whitelist que excluye p3d, el PBO de 99 MB no existiría. Investigar el packer *antes* de tocar; un cambio ciego puede *sacar* modelos.
- **Borrar `LFPG_Migrators` aunque no tenga llamadores (SEC18).** Persistencia: si algún save viejo o mod hermano llama por nombre, es un mundo roto. Dejar el bump no-op o cablear el deserializador; no `rm`.
- **G15 G16 G20 G21 G22, R23 partículas, C2/C6/R05 sort, S07/S24 coste de sorter, SEC15 RPCGuard, SEC14 purga BTC, D18, D20, MENOR de indentación (11).** Deuda o condicional; inflarían el plan a inventario.
- **Reabrir T0b / borrar `LFPG_Sorter.c`.** Ya se negó una vez, con razón.
- **Lanes que exijan desmontar/desplegar por MCP** como gate verde. `action_use` no completa continuas (brief §4). El gate de L-KIT es lectura; in-game es del dueño.
- **Armar T5 «de paso»** para declarar L-MONEY. Es un instrumento con JSON y `armPhrase`; no se ha ejecutado nunca. Si journal=sí, T5 es prerequisito explícito, no un extra.

## G. LO_NO_VERIFICADO

- **SHA de `main`.** El brief dice `e5b5303`; el HANDOFF vivo `adfd29c`; los triajes `d59cad8`. No corrí `git log`/`rev-parse` en esta sesión. Las citas `path:line` son del árbol que leí, no de un commit pineado.
- **Líneas de la auditoría vs este árbol.** Confirmé las anclas grandes de NM/IDevice/Migrators/Renderer/RPC/sorter; no re-derivé las 6.989 líneas ni los 152 `.c` / 72.249 ln (censo de `a187898`).
- **`data/` binarios.** Glob de `*.p3d`/`*.png`/`*.ogg` en este workspace devolvió 0. No afirmo que el junk del §5 se haya limpiado ni que siga: puede ser LFS, sparse checkout o exclusión del índice. Por eso assets no son lane de borrado.
- **Semántica real de `include.lst` para AddonBuilder.** Solo leí el fichero (`include.lst:1`). El PBO desplegado contiene modelos según el arranque; el mecanismo exacto no lo verifiqué abriendo un PBO.
- **G06 en runtime.** Leí `PostBulkRebuildAndPropagate` `:1087-1124` y el `CallLater` `:5088`. No ejecuté un corte con dos redes de batería.
- **SEC12 `array.Remove(0)`.** Confío en el triaje + la cita vanilla `enscript.c` que ellos abrieron; no reabrí el proto vanilla en esta sesión.
- **V3-07 MEDIO-l7 en el árbol *post* `adfd29c`.** El dictamen ancla `d59cad8`. Asumo que el port sigue; no re-leí `DeviceRegistry.c:59` hoy.
- **Que `LFPG_ActionOpenSorterPanel` (sin `_TEST`) esté muerta.** Grep en `ActionRegistration.c` solo encontró la `_TEST` (`:68`). No busqué restos de layout V3 en `gui/` uno a uno.
- **Cuota/estado de `archive/t2` vs `main` (diffstat 1.162).** Hecho verificado del brief; no re-medí `git diff --stat`.
- **In-game pendiente de siempre:** panel V4 abre con fixture alimentada; UIScaler a otra resolución/DPI; SEC01 recuento; R15 restore; C5 kill-ATM; T5 armado. El arranque 12:01 no los cubre (`fuentes/HANDOFF.md:79-83`).
- **No leí** `reviews/2026-09-07-t2-autoridad-servidor/README.md` (deuda R-T2-AGUA / R3-01). Si siguen vivos, no están en las lanes de arriba.
- **Ceguera:** listé `<RUN>\lanes\*\` lo bastante para ver `events.jsonl` / `EXIT.start` / logs; **no abrí ningún `PLAN.md` de otra lane** (y el glob de la corrida no mostraba `PLAN.md` ajenos, solo esos control files).

---

Ese es el entregable. En Agent mode se puede volcar tal cual a `reviews/2026-09-08-council-plan-definitivo/lanes/grok/PLAN.md`.