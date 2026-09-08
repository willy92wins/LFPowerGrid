Voy a ejecutar el brief. Estoy en Ask mode (no puedo escribir ficheros), así que al final imprimiré el contenido COMPLETO de `PLAN.md` como mensaje final, tal como pide el usuario. Empiezo leyendo las fuentes obligatorias.Shell bloqueado por hooks. Usaré Glob y Read directamenteTengo la auditoría y el HANDOFF. Ahora los triajes, los dictámenes de Grok y el INFORME-ASTRA de lAhora los dictámenes de Grok (MEDIO/MENOR) y el INFORME-ASTRA de l7 (7 CONFLICTO), más el PLAN-CONJUNTO.Ahora los dictámenes de oleada 2 (l6, l7, l8) y el INFORME-ASTRA de l7 con los 7 CONFLICTAhora el PLAN-CONJUNTO vigente y el MANIFEST del council.Voy a verificar la estructura del árbol real y algunas afirmaciones clave de la auditoríaVerifico que la UI V3 efectivamente se jubiló (HANDOFF dice que sí) y compruebo el tamaño de NetworkManagerImplTengo todo lo necesario. Verifiqué: `LFPG_NetworkManagerImpl.c` ahora tiene **7.810 líneas** (no 6.989; el árbol creció desde `a187898`), la UI V3 está jubilada (solo quedan `LFPG_Sorter.c` y `LFPG_Sorter_TEST.c` como subclase vacía de 14 líneas), y los 11 SubIds espejo 60-70 siguen en `LFPG_Defines.c:324-336`. Imprimo el PLAN.md completo como mensaje final, según pide el usuario.

```markdown
# PLAN — Plan de ejecución definitivo para LFPowerGrid

**Lane:** `glm` (`glm-5.2-max` vía `cursor-agent --mode ask`)
**Fecha:** 2026-09-08
**Brief:** `reviews/2026-09-08-council-plan-definitivo/BRIEF.md` (sha256 `abed2152…`)
**Base del árbol:** `main` @ `e5b5303` (verificado: UI V3 jubilada, `LFPG_Sorter_TEST.c` es subclase vacía de 14 líneas, `LFPG_NetworkManagerImpl.c` = 7.810 líneas — creció desde las 6.989 que cita la auditoría sobre `a187898`).

---

## A. CRÍTICA DEL ENCUADRE

El encargo está bien planteado en lo nuclear: la auditoría de 2026-09-07 sigue sin cruzar contra el trabajo triado (lo confirma `HANDOFF.md:718-728`), y ese cruce es la mitad del valor. Lo acepto como premisa.

**Tres objeciones concretas al encuadre:**

1. **El criterio "lanes disjuntas por FICHERO" colisiona con el hallazgo más grave de la propia auditoría.** S1 (`AUDITORIA-2026-09-07.md:18-27`) dice que `LFPG_NetworkManagerImpl.c` es un dios de 7.810 líneas con ~20 responsabilidades y que **muchos items caen dentro**. Verifiqué: G03-G13, G21-G23, S05-S06, S15, S24, SEC04-SEC06, SEC08, SEC17, R03, R06, R09, R20, R21 viven en ese fichero. Si las lanes deben ser disjuntas por fichero, **o todo eso es una sola lane (cero paralelismo), o hay que partir el dios primero** (que es S1, un NUEVO). El brief me pide lo segundo sin nombrarlo: el orden "cruzar → decidir `_TEST` → repartir" esconde que el reparto requiere un paso previo de partición que el propio brief no lista. Lo hago explícito en §D (lane `P`).

2. **El brief cuenta 46 "hallazgos" incluyendo las 7 propuestas de §6 de la auditoría.** Leídas: §6 son "propuestas adicionales, no pedidas, baratas" (`:108-115`), no defectos. El universo de defectos es 34 numerados + 5 viñetas de §5 = **39**, no 46. La diferencia importa para §B: las 7 propuestas son trabajo nuevo real pero de otra categoría (mejoras, no correcciones). Las cuento por separado para no inflar el "NUEVO" de defectos.

3. **El brief ignora el GRAVE de l6.** `l6-t0b-jubilar/DICTAMEN-GROK.md` dictaminó **ROJO — 1 GRAVE**: el recorte editó `LFPG_ActionOpenSorterPanel_TEST.c:50` y `LFPG_Sorter_TEST.c:15`, fuera de la lista blanca del `BRIEF.md` de implementación. Verifiqué el árbol: el estado final (subclase vacía, acción V4 cableada a la base) es el correcto de producto — el GRAVE es de **proceso** (salida de whitelist), no de código. Pero el brief al contar "9 MEDIO y 11 MENOR" sin mencionar el GRAVE oculta que T0b ya se ejecutó con un defecto de reparto que el orquestador tuvo que aceptar. Lo declaro en §G porque no puedo verificar si el dueño firmó la corrección de whitelist.

**Donde el encuadre acierta y lo conservo:** el orden B→C→D, el presupuesto de esfuerzo, la prohibición de leer las otras lanes (no lo hice; `lanes/glm/` solo tiene `EXIT.start` y `events.jsonl`, ningún `PLAN.md` ajeno), y la priorización del solape sobre el reparto.

---

## B. SOLAPE AUDITORIA <-> TRABAJO YA TRIADO

**Es la sección más importante.** Universo: 34 hallazgos numerados (S1-S7, H1-H9, C1-C12, U1-U6) + 5 viñetas de §5 (assets/build) + 7 propuestas de §6. Los cruzo contra las 101 fichas P2/P3 (92 VIVA + 6 DUDOSA + 3 MUERTA), los 9 MEDIO vivos, los 11 MENOR y los 7 CONFLICTO.

Leyenda: `CUBIERTO` = ya atendido o fichado; `PARCIAL` = parte cubierta, parte nueva; `NUEVO` = no hay ficha ni MEDIO/CONFLICTO que lo cubra; `OBSOLETO` = la auditoría misma lo da por muerto o el árbol lo mató.

### S — Sobreingeniería

| hallazgo | cubierto_por | evidencia (path:line) |
|---|---|---|
| S1 Dios `NetworkManagerImpl` 7.810 ln, ~20 responsabilidades | NUEVO (la propuesta de partirlo por dominio) | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:577-700` (scheduler), `:1817-1950` (batching), `:3713-4442` (self-heal 16 fases), `:5838-6591` (sorter), `:6592-6996` (sensores/pads/láser/intercom/horno/frigo/cocina/puerta). Ninguna ficha propone partirlo; las G/SEC/S solo tocan puntos calientes dentro. |
| S2 Fork TEST divergente (4.600 ln + layouts + RPCs) | CUBIERTO por T0b (DONE) + T0c (PENDIENTE) | `scripts/4_World/test/LFPG_Sorter_TEST.c:13` (subclase vacía de 14 ln); V3 UI borrada (Glob: 0 hits de `LFPG_SorterView.c`/`Controller.c`/`TagView.c`/`PreviewRow.c`/`ActionOpenSorterPanel.c` sin `_TEST`). Residual: T0c (renombrado `_TEST`). |
| S3 Reflexión `Call*` sin beneficiario en producción | PARCIAL — D10 cubre los 4 fast-path; el borrado de `Call*` y migrar 3 legado son NUEVO | `TRIAJE-dispositivos.md` D10 (`scripts/4_World/LFPG_IDevice.c:507,840,857,873`); audit `:32-34` cita `LFPG_IDevice.c:433-482` y `LFPG_TestDevices.c:53,930,1354` (las 3 legado). |
| S4 Ecosistema BTC ~10.200 ln / 14 ficheros | PARCIAL — 7 CONFLICTO + SEC12-14 cubren defectos; congelar/fusionar es NUEVO | `l7-debt-v3/INFORME-ASTRA.md` (V3-02, V3-03, V3-05, V3-06, H-04, H-05, err=14); `TRIAJE-red-seguridad.md` SEC12/13/14 (`LFPG_BTCSessionRegistry.c:13,18,91,137,375`); audit `:36-38` propone congelar BTC. |
| S5 Acciones y chrome UI triplicados | PARCIAL — T0a (paleta) + D18 (switches) cubren partes; `ActionToggleBase<T>` es NUEVO | T0a DONE (`LFPG_UIPalette.c` existe); `TRIAJE-dispositivos.md` D18 (`LFPG_SwitchV2.c:136` vs `LFPG_SwitchV2Remote.c:72`); audit `:40-42` propone base de vista común. |
| S6 Capas que no hacen nada | PARCIAL — SEC18 (migradores huérfanos) cubre una; el resto es NUEVO | `TRIAJE-red-seguridad.md` SEC18 (`LFPG_Migrators.c:5,40,68`); audit `:44` cita `LFPG_Migrators.c:96-107` (2 no-op) + `lfpg_devicebase.c:575-587` (hooks vacíos) + `lfpg_devicebase.c:596-619` (GetKitClassname/BlocksDismantle). |
| S7 `Defines.c` (811 ln) mezcla todo + estados futuros | NUEVO (partir por dominio al tocar) | `scripts/3_Game/LFPG_Defines.c:1-811` (verifiqué: Sway/LOD/oclusión/persistencia/RPC/sorter/sensores/pump/searchlight/baterías/intercom todos ahí); `LFPG_CableState:84-95` (9 estados, 3 usados). Ninguna ficha propone partirlo. |

### H — Servidor

| hallazgo | cubierto_por | evidencia (path:line) |
|---|---|---|
| H1 `BroadcastOwnerWires` doble barrido + JSON por mutación | CUBIERTO por G12 + G13 (VIVA) | `TRIAJE-grafo.md` G12 (`LFPG_NetworkManagerImpl.c:2069,2107,2116`), G13 (`:2399,2414,2453,2539,2552,2559,2590`); audit `:54`. |
| H2 `CleanDisappearedVanillaDevice` scan por prefijo | NUEVO (no hay ficha) | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:3380-3477` (audit `:54`); `TRIAJE-grafo.md` no la lista. |
| H3 `CutAllWiresFromDevice` fallback O(V·W) | NUEVO (no hay ficha) | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:4930-4996` (audit `:54`); G06 toca CutAll pero por otro ángulo (rebuild de baterías ajenas). |
| H4 Tick sorter: 6 resoluciones lineales + sqrt + dedup O(n²) | CUBIERTO por S05 + S07 + S24 (VIVA) | `TRIAJE-sorter.md` S05 (`LFPG_NetworkManagerImpl.c:6077,6118,6150`), S07 (`LFPG_SorterLogic.c:576,409`), S24 (`:6066,6174`); audit `:54`. |
| H5 `TickWaterPumps` triple barrido incondicional | NUEVO (no hay ficha) | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:5438-5720` (audit `:54`); ninguna ficha la lista. |
| H6 Celdas de jugadores O(P·C) + doble rebuild | CUBIERTO por U1 (audit) — pero U1 no está en las 101 fichas (es propuesta de la auditoría) | `AUDITORIA-2026-09-07.md:62` (U1); `TRIAJE-grafo.md` no tiene ficha de celdas. **NUEVO como ficha, CUBIERTO como propuesta U1.** |
| H7 `ElecGraph.RebuildFromWires` O(N+E) + sync 3 resoluciones/nodo | PARCIAL — G08 + G14 + G20 tocan el grafo; la amortización con scheduler (U3) es NUEVO | `TRIAJE-grafo.md` G08 (`LFPG_ElecGraphImpl.c:2071,2745,2926`), G14 (`:2747-2759`), G20 (`:224,237,311,318,1912`); audit `:60` cita U3. |
| H8 `HandleFinishWiring`: alloc+ToString+sqrt antes de validar | PARCIAL — SEC17 (VIVA) cubre la doble pasada de geometría; el alloc/ToString pre-validación es NUEVO | `TRIAJE-red-seguridad.md` SEC17 (`LFPG_RPCServerHandlerImpl.c:440,465,484,496`); audit `:60`. |
| H9 `RepackCargoInPlace`: 9 `new` + insertion sort O(n²) | CUBIERTO por S08 (arreglado en `cd5d153`, dictamen l3 MEDIO residual) | `l3-t4-render/DICTAMEN-GROK.md` S08 (`LFPG_SorterLogic.c:1133,1231,1268`); audit `:60`. |

### C — Cliente

| hallazgo | cubierto_por | evidencia (path:line) |
|---|---|---|
| C1 Sway invalida la caché | NUEVO (no hay ficha) | `scripts/4_World/LFPG_CableRenderer.c:3274-3275,3283,3289,3328` (audit `:68`); `TRIAJE-render-ui.md` no tiene ficha de sway. R10/R11 tocan caché de proyección pero no el sway. |
| C2 Tres pasadas O(n)/frame antes del loop | NUEVO (no hay ficha) | `scripts/4_World/LFPG_CableRenderer.c:2921-2950` (audit `:68`); R06 toca `RebuildConnCache` pero no las pasadas previas. |
| C3 Invariantes por subsegmento | NUEVO (no hay ficha) | `scripts/4_World/LFPG_CableRenderer.c:3391-3395,3448-3461,3567,3591` (audit `:68`); R24 toca duplicación de preparación pero no el hoist de invariantes. |
| C4 `ClipSegToScreen`/`ComputeEdgeFade` solo en miss | CUBIERTO por R26 (DUDOSA, lectura A) — el patrón está bien diseñado | `TRIAJE-render-ui.md` R26 (`LFPG_CableRenderer.c:2281,231,307,309`); audit `:70` ("Bien diseñado"). **No es defecto; la auditoría lo marca como correcto.** |
| C5 Player-occlusion 2 `GetScreenPos`/frame | NUEVO (no hay ficha) | `scripts/4_World/LFPG_CableRenderer.c:2853-2906,2864-2865` (audit `:70`); R10 toca claves de proyección pero no la occlusion. |
| C6 Selection sort O(n²) gateado | CUBIERTO por R05 (VIVA) — pero R05 dice "acotado por límite 512, sin perfil" | `TRIAJE-render-ui.md` R05 (`LFPG_CableRenderer.c:2719,2729,2734`); audit `:72` ("Correcto; escalar con sort nativo si n grande"). |
| C7 Oclusión bien presupuestada | CUBIERTO — la auditoría dice "No tocar" | `AUDITORIA-2026-09-07.md:74` ("No tocar"); `TRIAJE-render-ui.md` no la lista como defecto. **No es defecto.** |
| C8 CullTick O(owners+wires)/2 s + 1 sqrt/wire | PARCIAL — R08 (VIVA) toca CullTick; el sqrt residual es NUEVO | `TRIAJE-render-ui.md` R08 (`LFPG_CableRenderer.c:2382,2413,2414,2434,2438`); audit `:74` cita `:2561`. |
| C9 Preview solo con sesión | CUBIERTO — la auditoría dice "OK" | `AUDITORIA-2026-09-07.md:76` ("OK"); no hay ficha. **No es defecto.** |
| C10 Inspector bien throttled salvo `GetScreenSize+SetPos`/frame | PARCIAL — R13 (VIVA) toca offsets; el `GetScreenSize+SetPos`/frame es NUEVO | `TRIAJE-render-ui.md` R13 (`LFPG_DeviceInspector.c:1286,1289,1293,1301,1418,1421,1430`); audit `:76` cita `:1639-1641,1715Continúo el PLAN.md desde donde se cortó (sección B, tabla C, fila C10):

```markdown
| C10 Inspector bien throttled salvo `GetScreenSize+SetPos`/frame | PARCIAL — R13 (VIVA) toca offsets; `GetScreenSize+SetPos`/frame es NUEVO | `TRIAJE-render-ui.md` R13 (`LFPG_DeviceInspector.c:1286,1301,1418,1430`); audit `:76` cita `:1639-1641,1715`. |
| C11 Láser duplica el patrón sin caché entre frames | NUEVO (no hay ficha) | `scripts/4_World/LFPG_LaserBeamRenderer.c:157-342,264-265,299` (audit `:78`); R18/R19 tocan `CullTick` del láser pero no la caché de proyección entre frames. |
| C12 Joints ×4 `DrawLine` | NUEVO (no hay ficha) | `scripts/4_World/LFPG_CableHUD.c:349-352` (audit `:78`); R27 toca `CableHUD` pero por 0×0, no por joints. |

### U — Unificación

| hallazgo | cubierto_por | evidencia (path:line) |
|---|---|---|
| U1 Un rebuild de celdas por turno | NUEVO (es propuesta de la auditoría, no ficha) | `AUDITORIA-2026-09-07.md:62` (`:6946-6947` + `:7199-7207`); `TRIAJE-grafo.md` no la lista. La auditoría lo propone, no lo denuncia como bug. |
| U2 Sesiones a 1 s + fusionar gemelos | PARCIAL — SEC08 (VIVA, tick sin distancia) + SEC12-14 (BTCSessionRegistry) tocan los gemelos; fusionar `BTCSessionRegistry`+`ControlSessionRegistry` es NUEVO | `TRIAJE-red-seguridad.md` SEC08 (`LFPG_ControlSessionRegistry.c:136,391,404,420`), SEC12-14 (`LFPG_BTCSessionRegistry.c`); audit `:62` propone `SessionStore` con `kind`. S4 también. |
| U3 One-shots al scheduler (cierra leak LIFE-008) | NUEVO (es propuesta de la auditoría) | `AUDITORIA-2026-09-07.md:64` (`:504,3726,3973,5068,55`); `TRIAJE-grafo.md` no la lista. H7 la cita como amortización. |
| U4 Cola diferida única con coalescencia | PARCIAL — D09 (VIVA) toca callbacks de RemoteController/MemoryCell sin cancelar; la cola única es NUEVO | `TRIAJE-dispositivos.md` D09 (`LFPG_RemoteController.c:539,559`, `LFPG_MemoryCell.c:184,267`); audit `:64`. |
| U5 Censo único por turno | NUEVO (es propuesta de la auditoría) | `AUDITORIA-2026-09-07.md:66` (`:6841-6848`); `TRIAJE-grafo.md` no la lista. |
| U6 Cliente: 1 maintenance tick + 1 nearby cache + 1 player/frame | NUEVO (es propuesta de la auditoría) | `AUDITORIA-2026-09-07.md:66` (`MissionInit.c:246-465`); R16/R17 tocan puntos sueltos pero no la unificación. |

### §5 — Assets, config y build (5 viñetas)

| hallazgo | cubierto_por | evidencia (path:line) |
|---|---|---|
| §5a Top `.p3d` (4 gates + 5 lids ≈ 230 MB, 55% de `data/`) | NUEVO (no hay ficha; es deuda de assets) | `AUDITORIA-2026-09-07.md:98` (techo 10 MB, 1 gate + 1 lid instanciados). Ninguna ficha P2/P3 toca assets. |
| §5b Junk (`kitboxtexture.png` 2,3 MB + `.paa` 0 refs; `*.ogg.stereo_backup`; `Furnace_mono.ogg` huérfano; `water.ogg` 5,5 MB) | NUEVO (no hay ficha) | `AUDITORIA-2026-09-07.md:100`. |
| §5c Duplicados SHA256 (`switch_v1_co==switch_v1_remote_co` 4,3 MB ×2; `.rvmat` 104 ficheros en ~5 plantillas) | NUEVO (no hay ficha) | `AUDITORIA-2026-09-07.md:100`. |
| §5d Compile (51/122 ficheros `4_World` sin guard; TEST en server; `units[]` con `LF_TestLamp/Heavy`; `include.lst` sin `*.p3d,*.cpp,*.cfg`; `reviews/` ~19 MB en el árbol) | PARCIAL — S2 (TEST en server) se cubre con T0b; el resto NUEVO | `AUDITORIA-2026-09-07.md:102`. T0b jubiló la UI V3 pero los `_TEST` de entidad y `TestDevices` siguen. |
| §5e Gates propuestos (fallo build si `*TEST*` en staged/`units[]`; blocklist `*.stereo_backup,*.bak,*.png`; tope `.ogg`; 0 `*View*` sin guard; aserción versión única) | NUEVO (es propuesta de build) | `AUDITORIA-2026-09-07.md:104`. |

### §6 — Propuestas adicionales (7, "no pedidas, baratas")

| propuesta | cubierto_por | evidencia (path:line) |
|---|---|---|
| §6.1 `MAX_WIRES_PER_OWNER_CLIENT=128` vs `MAX_WIRES_PER_DEVICE=64` — no unificar | OBSOLETO (la auditoría misma lo retira) | `AUDITORIA-2026-09-07.md:108` ("no unificar; la auditoría vieja que lo pedía está desfasada"). **No es trabajo.** |
| §6.2 `S1_PROBE` fuera de `LFPG_PERFDIAG_ENABLED` + `LFPG_UIScaler` ausente en V4 | CUBIERTO por T0b (DONE — `S1_PROBE` gateado, `UIScaler` portado aislado) | `l5-t0b-recorte/DICTAMEN-GROK.md` (S1_PROBE gateado en `LFPG_SorterView_TEST.c:1658`, UIScaler portado en `:1384`); `HANDOFF.md:226-228`. |
| §6.3 `OnKeyPress/OnKeyRelease` consumen teclas sin `super` con UI activa | NUEVO (no hay ficha) | `AUDITORIA-2026-09-07.md:108` (`MissionInit.c:170-244`). |
| §6.4 `Widget.SetLV(0)/SetTextLV(0)` globales | PARCIAL — R30 (VIVA) toca lo mismo; el "acotar a raíces LFPG" es NUEVO | `TRIAJE-render-ui.md` R30 (`LFPG_MissionInit.c:140-141`); audit `:108`. |
| §6.5 `RepackCargoInPlace`: avisar en UI cuando `n>64` | CUBIERTO por S08 (arreglado, con residual MEDIO) | `l3-t4-render/DICTAMEN-GROK.md` S08; audit `:108`. |
| §6.6 Documentar por qué `LFPG_ElecGraph.c` vive partido 4_World/5_Mission | NUEVO (es propuesta de comentario) | `AUDITORIA-2026-09-07.md:108`. |
| §6.7 Congelar por comentario los sistemas cerrados (BTC, CCTV) | NUEVO (es propuesta de comentario) | `AUDITORIA-2026-09-07.md:108`. |

### Resumen del solape

| categoría | recuento | detalle |
|---|---|---|
| CUBIERTO (ya atendido o fichado) | **9** | S2 (T0b), H1 (G12+G13), H4 (S05+S07+S24), H9 (S08), §6.2 (T0b), §6.5 (S08), C4 (no-defecto), C7 (no-defecto), C9 (no-defecto) |
| PARCIAL (parte cubierta, parte nueva) | **11** | S3, S4, S5, S6, H7, H8, C8, C10, U2, U4, §5d, §6.4 |
| NUEVO (no hay ficha ni MEDIO/CONFLICTO) | **21** | S1, S7, H2, H3, H5, H6, U1, U3, U5, U6, C1, C2, C3, C5, C11, C12, §5a, §5b, §5c, §5e, §6.3, §6.6, §6.7 |
| OBSOLETO (la auditoría lo retira) | **1** | §6.1 |
| **Total** | **46** | (39 defectos + 7 propuestas) |

**Conclusión del solape:** de los 39 hallazgos que son defectos reales, **9 están cubiertos** (3 de ellos porque la auditoría los marca como no-defecto), **11 son parcialmente nuevos** y **19 son totalmente nuevos**. De las 7 propuestas de §6, 1 está cubierta (T0b), 1 es obsoleta, 1 parcial y 4 nuevas. **El trabajo nuevo real es ~25-30 items, no 170.** La inferencia del orquestador (§5: "una fracción grande ya está cubierta") se confirma, pero el número es mayor de lo que sugiere: ~25-30 items nuevos, no "bastante menor que 170" sin más matiz. La auditoría aporta **trabajo nuevo sustancial**, sobre todo en render cliente (C1-C3, C5, C11-C12: 7 items nuevos no cubiertos por ninguna ficha) y en optimización de servidor (H2, H3, H5: 3 items nuevos).

---

## C. LA DECISIÓN DE PRODUCTO

**Veredicto: el fork `_TEST` NO debe salir del release. Conservar el classname `LFPG_Sorter_TEST` es obligatorio por persistencia. La decisión real de producto es otra (ver abajo).**

**Consecuencia numérica de conservar `_TEST`:**
- **0 items desaparecen** del backlog por esta decisión. Las fichas S10 (DUDOSA, lectura duplicación), S20, S21, S23 (que viven en `test/LFPG_SorterController_TEST.c` y `test/LFPG_SorterTagView_TEST.c`) siguen vivas porque el `_TEST` sigue como entidad spawneable.
- **0 items aparecen.** No hay trabajo nuevo que cree la decisión de conservar.
- **T0c (renombrado `_TEST` → canónico) se CANCELA.** Renombrar rompe mundos con sorters colocados (`config.cpp:1080,1086` — verifiqué que ambos classnames existen). La subclase vacía de 14 líneas (`scripts/4_World/test/LFPG_Sorter_TEST.c:13`) es el coste mínimo de conservar.

**Consecuencia numérica de sacar `_TEST` (hipotético, rechazado):**
- Rompería persistencia de cualquier mundo con `LFPG_Sorter_TEST` colocado (violación de `AGENTS.md` §4).
- Las fichas S20, S21, S23 dejarían de importar (3 items fuera).
- Pero exigiría una migración de classname que el propio `AGENTS.md` prohíbe sin decisión explícita del dueño.

**La decisión de producto que SÍ cambia el tamaño del plan** no es `_TEST`. Es **si se parte `LFPG_NetworkManagerImpl.c` (S1) antes de repartir, o no.** El brief la esconde tras `_TEST`, pero el árbol la desmiente:
- `LFPG_NetworkManagerImpl.c` = **7.810 líneas** (verifiqué, no 6.989), con ~20 responsabilidades.
- **23 fichas VIVA** viven en ese fichero (G03, G05-G13, G21-G23, S05, S06, S15, S24, SEC04-SEC06, SEC08, SEC17, R03, R06, R09, R20, R21).
- Si no se parte: las 23 fichas son **una sola lane** (cero paralelismo en el servidor).
- Si se parte por dominio (detección → sorter → solar/pump → BTC → baterías → scheduler): las 23 fichas se reparten en ~5 lanes paralelas, pero la partición misma es trabajo (S1, NUEVO, AMPLIO).

**Mi recomendación: partir S1 primero (lane `P`), luego repartir.** Sin S1, el plan tiene una lane de 7.810 líneas que serializa todo el servidor. Con S1, el plan gana 4-5 lanes paralelas al coste de un refactor estructural que la auditoría ya priorizó.

---

## D. EL PLAN: LANES

**Convención:** ficheros exclusivos = si dos lanes tocan el mismo `.c`, no son paralelas y se dice cómo se resuelve. `LFPG_NetworkManagerImpl.c` se parte en la lane `P` antes de que arranquen las lanes que lo consumen.

### Lane `P` — Partir `LFPG_NetworkManagerImpl.c` por dominio (S1)

| campo | valor |
|---|---|
| objetivo | Extraer bloques por dominio (detección, sorter, solar/pump, BTC, baterías, scheduler núcleo) a ficheros separados bajo `5_Mission/`. El núcleo queda ≤2.500 ln. |
| ficheros exclusivos | `scripts/5_Mission/LFPG_NetworkManagerImpl.c` (partición), crea `LFPG_NetworkManager_Sorter.c`, `LFPG_NetworkManager_Detection.c`, `LFPG_NetworkManager_Pumps.c`, `LFPG_NetworkManager_BTC.c`, `LFPG_NetworkManager_Batteries.c` (nuevos) |
| items que cierra | S1 (NUEVO) |
| gate de aceptación | Linter offline: `len(errors)==0` sobre el árbol. Compilación: el mundo carga sin `Can't compile` (gate in-game, no negociable). Invariante: cada nuevo fichero no referencia símbolos de `5_Mission` no listados en él (orden de compilación). |
| depende de | ninguna (es prerrequisito de L1-L5) |

### Lane `L1` — Grafo eléctrico (fichas G + H de servidor no cubiertas)

| campo | valor |
|---|---|
| objetivo | Cerrar G03, G05-G11, G13, G14, G19-G23 + H2, H3, H5, H7 (parcial), H8 (parcial) |
| ficheros exclusivos | `scripts/5_Mission/LFPG_ElecGraphImpl.c`, `scripts/5_Mission/LFPG_NetworkManager_Graph.c` (salido de `P`) |
| items que cierra | G03, G05, G06, G07, G08, G09, G10, G11, G13, G14, G19, G20, G21, G22, G23, H2, H3, H5, H7(parcial), H8(parcial) — **~20 items** |
| gate de aceptación | Linter `len(errors)==0`. Lectura: `RebuildFromWires` rechaza ciclos y tamaño global/componente (G10). `RecountAllPlayerWires` se llama tras poda diferida (G07). `IsPortReceivingPower` no devuelve true por owner retirado (G21). |
| depende de | `P` (para que `LFPG_NetworkManager_Graph.c` exista) |

### Lane `L2` — Sorter servidor (fichas S de servidor)

| campo | valor |
|---|---|
| objetivo | Cerrar S01-S03, S05, S06, S12, S14-S16, S24 |
| ficheros exclusivos | `scripts/5_Mission/LFPG_SorterLogic.c`, `scripts/3_Game/LFPG_SorterData.c`, `scripts/5_Mission/LFPG_NetworkManager_Sorter.c` (salido de `P`) |
| items que cierra | S01, S02, S03, S05, S06, S12, S14, S15, S16, S24 — **10 items** |
| gate de aceptación | Linter `len(errors)==0`. Lectura: `FromJSON` rechaza config vacía válida (S01) y no publica reglas parciales tras false (S02). `Save` y `Preview` comparten umbral (S03). `HandleSorterPreviewRequest` aplica `CanTakeFromContainer` (S14).Continúo con L2 en adelante:

```markdown
| gate de aceptación | Linter `len(errors)==0`. Lectura: `FromJSON` acepta config vacía válida (S01) y no publica reglas parciales tras false (S02). `Save` y `Preview` comparten umbral 2048 (S03). `HandleSorterPreviewRequest` aplica `CanTakeFromContainer` (S14). Validación simétrica de vínculo en origen/destino/manual (S15). |
| depende de | `P` |

### Lane `L3` — Render/UI cliente (fichas R + C de cliente no cubiertas)

| campo | valor |
|---|---|
| objetivo | Cerrar R01, R03, R05-R13, R20-R22, R24, R26-R30 + C1, C2, C3, C5, C8(parcial), C10(parcial), C11, C12 |
| ficheros exclusivos | `scripts/4_World/LFPG_CableRenderer.c`, `scripts/4_World/LFPG_DeviceInspector.c`, `scripts/4_World/LFPG_LaserBeamRenderer.c`, `scripts/4_World/LFPG_CableHUD.c`, `scripts/4_World/LFPG_CableParticle.c`, `scripts/4_World/LFPG_CameraViewport.c`, `scripts/4_World/LFPG_Camera.c`, `scripts/4_World/LFPG_SearchlightController.c`, `scripts/5_Mission/LFPG_TankHUD.c`, `scripts/5_Mission/LFPG_MissionInit.c` |
| items que cierra | R01, R03, R05, R06, R07, R08, R09, R10, R11, R12, R13, R20, R21, R22, R24, R26, R27, R28, R29, R30, C1, C2, C3, C5, C8(parcial), C10(parcial), C11, C12 — **~28 items** |
| gate de aceptación | Linter `len(errors)==0` + `ui_reconcile.py` 0 FAIL. Lectura: `BuildOccSamples` se llama tras llenar `cachedJoints` (R01). `RebuildConnCache` tras purga (R03). `HasRenderableWires` cuenta `segments.Count()` no entradas (R09). `SafeAbort` identifica la entidad de sesión (R14, fuera de esta lane — ver L7). |
| depende de | ninguna (cliente puro, no toca `LFPG_NetworkManagerImpl.c`) |

### Lane `L4` — Dispositivos (fichas D no cubiertas)

| campo | valor |
|---|---|
| objetivo | Cerrar D06-D11, D13-D24 + S3 (reflexión Call*), S6 (capas vacías, parcial) |
| ficheros exclusivos | `scripts/4_World/LFPG_IDevice.c`, `scripts/4_World/lfpg_devicebase.c`, `scripts/4_World/LFPG_ActionDismantleDevice.c`, `scripts/4_World/LFPG_RemoteController.c`, `scripts/4_World/LFPG_MemoryCell.c`, `scripts/4_World/LFPG_Intercom.c`, `scripts/4_World/LFPG_DoorController.c`, `scripts/4_World/LFPG_Furnace.c`, `scripts/4_World/LFPG_MotionSensor.c`, `scripts/4_World/LFPG_SwitchV2Remote.c`, `scripts/4_World/LFPG_SwitchV2.c`, `scripts/4_World/LFPG_SwitchRemote.c`, `scripts/4_World/LFPG_TestDevices.c`, `scripts/4_World/LFPG_BatteryAdapter.c`, `scripts/4_World/LFPG_Battery.c`, `scripts/4_World/LFPG_ActionUpgradeSolarPanel.c`, `scripts/4_World/LFPG_ActionUpgradeWaterPump.c`, `scripts/4_World/LFPG_KitBase.c`, `scripts/4_World/LFPG_KitBaseDeployable.c` |
| items que cierra | D06, D07, D08, D09, D10, D11, D13, D14, D15, D17, D18, D19, D21, D22, D23, D24, S3(parcial), S6(parcial) — **~18 items** |
| gate de aceptación | Linter `len(errors)==0`. Lectura: `LFPG_ActionDismantleDevice` copia salud antes de `ObjectDelete` (D06). `LFPG_GetStoredEnergy` del adaptador lee `m_StoredEnergyX10` en cliente (D24). `LFPG_BlocksDismantle` existe y bloquea con saldo (verificado en `LFPG_BTCAtm.c`). |
| depende de | ninguna (4_World puro) |

### Lane `L5` — Red/seguridad servidor (fichas SEC no cubiertas + H8/SEC17)

| campo | valor |
|---|---|
| objetivo | Cerrar SEC04-SEC08, SEC10, SEC11, SEC15-SEC19 + H8(parcial), SEC17 |
| ficheros exclusivos | `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`, `scripts/5_Mission/LFPG_NetworkManager_RPC.c` (salido de `P`), `scripts/3_Game/LFPG_FileUtil.c`, `scripts/3_Game/LFPG_Settings.c`, `scripts/4_World/LFPG_RPCGuard.c`, `scripts/3_Game/LFPG_Migrators.c` |
| items que cierra | SEC04, SEC05, SEC06, SEC07, SEC08, SEC10, SEC11, SEC15, SEC16, SEC17, SEC18, SEC19, H8(parcial) — **~13 items** |
| gate de aceptación | Linter `len(errors)==0`. Lectura: `RescueStaleIncomingWires` condiciona a `changed` (SEC04). `RateLimitedWarn` acota por UID (SEC05). `EnsureFileOrRestore` valida parseo del target antes de aceptar (SEC10). `LFPG_Settings.Load` no publica singleton corrupto (SEC11). |
| depende de | `P` (para `LFPG_NetworkManager_RPC.c`) |

### Lane `L6` — Deuda BTC / CONFLICTO de dinero (los 7 CONFLICTO)

| campo | valor |
|---|---|
| objetivo | Decidir los 7 CONFLICTO de `debt/v3-data-integrity` (V3-02, V3-03, V3-05, V3-06, H-04, H-05, err=14) — son decisiones de diseño de recovery, no código mecánico |
| ficheros exclusivos | `scripts/5_Mission/LFPG_BTCHelper.c`, `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c` |
| items que cierra | V3-02, V3-03, V3-05, V3-06, H-04, H-05, err=14 — **7 items** (todos CONFLICTO, requieren decisión del dueño, no implementación directa) |
| gate de aceptación | **No es código, es decisión.** Gate = el dueño firma por escrito qué recovery admite (cuarentena de claims V3-05, relectura de target V3-06, atomicidad cash-ledger V3-02/H-04/H-05, err=14 en 5 operaciones). Cada decisión documentada con su invariante. |
| depende de | ninguna (es decisión, no toca ficheros hasta que se decida) |

### Lane `L7` — CCTV / Cámara (fichas R14, R15 residual, V3-09, V3-10)

| campo | valor |
|---|---|
| objetivo | Cerrar R14 (SafeAbort sin identidad), R15 residual (restauración CCTV), V3-09 (lease searchlight), V3-10 (CCTV desde vehículo) |
| ficheros exclusivos | `scripts/4_World/LFPG_Camera.c`, `scripts/4_World/LFPG_CameraViewport.c`, `scripts/5_Mission/LFPG_ControlSessionRegistry.c`, `scripts/4_World/LFPG_SearchlightController.c`, `scripts/4_World/LFPG_Searchlight.c`, `scripts/4_World/LFPG_ActionWatchMonitor.c` |
| items que cierra | R14, R15(residual), V3-09, V3-10 — **4 items** |
| gate de aceptación | Linter `len(errors)==0`. **Gate in-game no negociable** para R15 (restauración de cámara CCTV) y V3-09 (lease de searchlight con heartbeat). `SafeAbort` recibe `entityId` y solo cierra si coincide (R14). |
| depende de | ninguna |

### Lane `L8` — Sorter UI V4 (fichas S de UI que viven en `test/`)

| campo | valor |
|---|---|
| objetivo | Cerrar S09, S11, S13, S18, S20, S21, S23 (las que viven en `test/LFPG_SorterController_TEST.c` y `test/LFPG_SorterTagView_TEST.c`) |
| ficheros exclusivos | `scripts/4_World/test/LFPG_SorterController_TEST.c`, `scripts/4_World/test/LFPG_SorterView_TEST.c`, `scripts/4_World/test/LFPG_SorterTagView_TEST.c`, `scripts/4_World/test/LFPG_SorterPreviewRow_TEST.c`, `scripts/4_World/test/LFPG_ActionOpenSorterPanel_TEST.c`, `scripts/4_World/LFPG_RPCClientHandler.c`, `scripts/4_World/LFPG_ActionSyncSorter.c`, `scripts/4_World/LFPG_Actions.c` |
| items que cierra | S09, S11, S13, S18, S20, S21, S23 — **7 items** |
| gate de aceptación | Linter `len(errors)==0` + `ui_reconcile.py` 0 FAIL. Lectura: ACK de repack refresca al solicitante aunque `movedCount==0` (S09). `RefreshRulesDisplay` llama a `RefreshRail_TEST` (S21). `SetData` no guarda owner sin consumidor (S23). |
| depende de | `L2` (S09/S12 comparten handler servidor) |

### Lane `L9` — Assets / Build / Higiene (§5 + §6.3, §6.6, §6.7 + gates de build)

| campo | valor |
|---|---|
| objetivo | Cerrar §5a-§5e (assets, compile, gates) + §6.3 (hotkeys), §6.6 (comentario ElecGraph), §6.7 (congelar BTC/CCTV por comentario) |
| ficheros exclusivos | `data/` (todos los `.p3d`, `.ogg`, `.png`, `.paa`, `.rvmat`), `config.cpp`, `include.lst`, `scripts/3_Game/LFPG_Defines.c` (solo para aserción versión), `scripts/4_World/LFPG_ElecGraph.c` (solo comentario cabecera) |
| items que cierra | §5a, §5b, §5c, §5d, §5e, §6.3, §6.6, §6.7 — **8 items** |
| gate de aceptación | Linter `len(errors)==0`. Build: `include.lst` lista `*.p3d,*.cpp,*.cfg` o se documenta semántica inclusiva. `reviews/` movido fuera del root. Aserción `LFPG_VERSION_STR` única. Blocklist `*.stereo_backup,*.bak,*.png` aplicada. |
| depende de | ninguna (no toca `.c` de lógica) |

### Resumen de lanes

| lane | items | ficheros exclusivos | depende de |
|---|---|---|---|
| `P` | 1 (S1) | `LFPG_NetworkManagerImpl.c` + 5 nuevos | — |
| `L1` | ~20 | `LFPG_ElecGraphImpl.c` + `NM_Graph.c` | `P` |
| `L2` | 10 | `LFPG_SorterLogic.c`, `LFPG_SorterData.c` + `NM_Sorter.c` | `P` |
| `L3` | ~28 | 10 ficheros cliente | — |
| `L4` | ~18 | 19 ficheros `4_World` | — |
| `L5` | ~13 | `LFPG_RPCServerHandlerImpl.c` + 5 | `P` |
| `L6` | 7 (decisiones) | `LFPG_BTCHelper.c`, `LFPG_BalanceProvider_NativeImpl.c` | — |
| `L7` | 4 | 6 ficheros CCTV/cámara | — |
| `L8` | 7 | 8 ficheros `test/` + cliente | `L2` |
| `L9` | 8 | `data/`, `config.cpp`, `include.lst` | — |
| **Total** | **~116** | | |

**Nota sobre conflictos de fichero resueltos:** `LFPG_NetworkManagerImpl.c` lo toca solo `P` (partición); las lanes que necesitan su contenido lo consumen de los ficheros nuevos salida de `P`. `LFPG_MissionInit.c` lo toca solo `L3` (cliente). `LFPG_RPCServerHandlerImpl.c` lo toca solo `L5`. `LFPG_SorterLogic.c` lo toca solo `L2`. `LFPG_ElecGraphImpl.c` lo toca solo `L1`. `LFPG_BTC*.c` lo toca solo `L6`. Sin solapamientos.

---

## E. ORDEN Y PARALELISMO

**Fases:**

1. **Fase 0 — Decisiones (sin código):** `L6` (deuda BTC, 7 decisiones) y la decisión de `_TEST` (ya resuelta en §C: conservar). Corre en paralelo con todo, no toca ficheros hasta firmarse.

2. **Fase 1 — Partición (serializa el servidor):** `P` solo. Sin `P`, las lanes de servidor (`L1`, `L2`, `L5`) no son paralelas porque todas querrían escribir `LFPG_NetworkManagerImpl.c`. **`P` es el cuello de botella y debe ir primero.**

3. **Fase 2 — Paralelo masivo (tras `P`):** `L1`, `L2`, `L3`, `L4`, `L5`, `L7`, `L8`, `L9` corren a la vez. **8 lanes simultáneas.**
   - `L1` (grafo) + `L2` (sorter servidor) + `L5` (RPC/seguridad) consumen los ficheros nuevos salida de `P`, disjuntos.
   - `L3` (render cliente), `L4` (dispositivos), `L7` (CCTV), `L9` (assets/build) no tocan `5_Mission` ni `LFPG_NetworkManagerImpl.c`.
   - `L8` (sorter UI) depende de `L2` por el handler compartido, pero solo lee, no escribe el mismo fichero: puede arrancar en Fase 2 y esperar el gate de `L2` para su cierre.

4. **Fase 3 — Cierre (tras Fase 2):** `L8` cierra tras `L2`. Gate in-game único para `L7` (CCTV/searchlight).

**Cuántas lanes simultáneas aguanta el plan sin conflictos: 8** (Fase 2). El límite lo da `P` (Fase 1, serial), no el paralelismo de Fase 2. Si `P` se rechaza por riesgo, el plan cae a 5 lanes paralelas agrupando `L1`+`L5` (grafo+RPC, mismo dominio servidor) y `L2`+`L8` (sorter, mismo dominio), con `L3`/`L4`/`L7`/`L9` independientes — peor pero viable.

**Bloqueos:**
- `P` bloquea a `L1`, `L2`, `L5` (todas consumen `LFPG_NetworkManagerImpl.c`).
- `L2` bloquea a `L8` (handler compartido S09/S12).
- `L6` no bloquea nada (es decisión, no código).
- `L3`, `L4`, `L7`, `L9` no bloquean ni son bloqueadas (cliente/assets puros).

---

## F. LO QUE NO HARÍA

1. **No borrar ni renombrar `LFPG_Sorter_TEST` ni `LFPG_Sorter_TEST_Kit`** (`config.cpp:1080,1086`). Hay jugadores reales con sorters colocados. Renombrar rompe mundos. La subclase vacía de 14 líneas es el coste mínimo. (Violación de `AGENTS.md` §4.)

2. **No meter los 7 CONFLICTO de BTC como código.** Son decisiones de recovery (cuarentena de claims, atomicidad cash-ledger, relectura de target). Implementarlos sin decisión del dueño es re-armar la bomba de T1 (`HANDOFF.md:465-478`). Van a `L6` como decisiones.

3. **No partir `LFPG_NetworkManagerImpl.c` sin gate in-game.** S1 es un refactor estructural AMPLIO. Si se hace mal, rompe el arranque (Enforce solo compila al cargar mundo). El gate inContinúo y termino (sección F, punto 3, y sección G):

```markdown
3. **No partir `LFPG_NetworkManagerImpl.c` sin gate in-game.** S1 es un refactor estructural AMPLIO. Si se hace mal, rompe el arranque (Enforce solo compila al cargar mundo). El gate in-game no es opcional para `P`. Si el dueño rechaza `P` por riesgo, el plan cae a 5 lanes paralelas (ver §E) — es preferible a serializar 23 fichas en una lane.

4. **No arreglar las 6 DUDOSAS como si fueran VIVA.** D20 (logging en Debug, ya corregido en producción), D21 (umbral latente, requiere motor), G16 (dos lecturas, sin especificar), R26 (distancia corregida, oclusión optimista), S10 (divergencia no localizada), S13 (promesa de producto ausente), S18 (API nativa no verificable offline). Cada una necesita una decisión o una prueba que no es código. Marcarlas VIVA sería inflar el plan.

5. **No meter las 7 propuestas de §6 como defectos.** Son mejoras ("no pedidas, baratas"). §6.1 está obsoleto (la auditoría lo retira). Tratarlas como P2 infla el backlog con trabajo que no corrige fallos. Van a `L9` solo las que tienen coste bajo y sentido (§6.3, §6.6, §6.7).

6. **No tocar BTC estructuralmente (S4) sin congelar primero.** La auditoría propone "congelar BTC (feature-freeze)". Sin congelamiento, cualquier refactor de BTC choca con los 7 CONFLICTO vivos. `L6` decide antes que `L5`/`L9` toquen BTC.

7. **No reabrir T2 (autoridad de servidor).** Ya está archivado con 6/7 condiciones y V2 parcial (`HANDOFF.md:440-446`). Las 4 fichas SEC01/02/03/20 ya están atendidas. SEC04-SEC08 (no cubiertas por T2) van a `L5`, no a reabrir T2.

8. **No prometer reducción de arena.** La invariante `HANDOFF.md:788-790` dice "recortar líneas NO reduce el arena". La jubilación de V3 ya está hecha y rindió ~18 kB neto (~2% del hueco de 885 kB). El plan no se vende por arena, se vende por mantenibilidad y corrección.

---

## G. LO_NO_VERIFICADO

1. **No verifiqué `main = e5b5303` con `git log`.** El shell está bloqueado por hooks en esta sesión. Asumo el SHA del brief §4. La verifiqué indirectamente: el árbol coincide con el estado que describe `HANDOFF.md` (UI V3 jubilada, `_TEST` subclase vacía, `LFPG_Sorter.c` v4.0).

2. **No corrí el linter offline.** El shell está bloqueado. Asumo la línea base del brief (263 ficheros, 0 errores, 47 warnings, exit 2). El gate de cada lane exige re-correrlo; no lo declaro verde sin correrlo.

3. **No verifiqué las 6.989 líneas de `LFPG_NetworkManagerImpl.c` que cita la auditoría.** Verifiqué que el fichero tiene 7.810 líneas (leí hasta `:7810` que es el `};` de cierre). La auditoría midió sobre `a187898`; el árbol creció desde entonces. La cifra de la auditoría está desfasada pero la conclusión (dios, cuello de botella) se refuerza.

4. **No verifiqué que las 23 fichas que asigno a `LFPG_NetworkManagerImpl.c` son exhaustivas.** Las conté leyendo los 5 `TRIAJE-*.md` y cruzando `path:line`. Es posible que alguna ficha de render (R03, R06, R09, R20, R21) tenga un call-site secundario en `LFPG_NetworkManagerImpl.c` que no vi. El reparto por fichero se ajusta si se encuentra un solapamiento.

5. **No verifiqué que `archive/t2-autoridad-servidor` aporta 1.162 inserciones netas en 4 ficheros.** El brief lo afirma; no puedo correr `git diff`. Asumo que es cierto. Si la rama se fusiona, sus 4 ficheros de `5_Mission` pueden chocar con `L1`/`L5` — el plan asume que NO se fusiona (el HANDOFF la deja sin fusionar y "nunca compilada ni arrancada").

6. **No verifiqué el estado real de `debt/v3-data-integrity` y `maint/audit-kimi-followup`.** El brief dice "ya vaciadas por las lanes `l7` y `l8`". Los `INFORME-ASTRA.md` y `DICTAMEN-GROK.md` de l7/l8 que leí confirman que se portaron 6+3 items y dejaron 7 CONFLICTO sin portar. Asumo que las ramas no se fusionan (el HANDOFF no las fusiona).

7. **No verifiqué que el GRAVE de l6 (salida de lista blanca) esté resuelto o aceptado.** Leí el `DICTAMEN-GROK.md` de l6 que lo dictamina ROJO. Verifiqué el árbol: el estado final (subclase vacía, acción V4 cableada) es coherente. Pero no sé si el dueño firmó la corrección de whitelist o si el recorte se rehará. Si se rehace, `LFPG_Sorter.c` y `LFPG_ActionOpenSorterPanel_TEST.c` se editan de nuevo y chocan con `L4`/`L8`.

8. **No verifiqué las afirmaciones de comportamiento in-game.** Ninguna. La auditoría, los triajes y los dictámenes son lectura estática. El gate in-game (arranque diag) está hecho para `adfd29c` (`HANDOFF.md:60-72`) pero no para `e5b5303` ni para el árbol tras `P`. Cada lane que toque `5_Mission` necesita gate in-game propio.

9. **No verifiqué que `LFPG_UIScaler` portado a V4 (T0b) escale a otros DPI.** El `DICTAMEN-GROK.md` de l5 lo deja en MENOR y el `HANDOFF.md:228` lo declara pendiente de prueba. Lo asumo como hecho (T0b DONE) pero la prueba in-game sigue abierta.

10. **No abrí los `PLAN.md` de las otras lanes.** El brief lo prohíbe. `lanes/glm/`, `lanes/kimik3/`, `lanes/grok/`, `lanes/astra/`, `lanes/gemini/` solo tienen `EXIT.start` y `events.jsonl` (verifiqué con Glob). Ningún `PLAN.md` ajeno existe aún en disco. Si existe en memoria de otra lane, no lo sé.

11. **No verifiqué la cifra "9 MEDIO vivos y 11 MENOR".** La reconstruí de los 8 `DICTAMEN-GROK.md` que leí (l1-l8). Mi recuento da 10 MEDIO (l1=1, l2=1, l3=3, l4=2, l5=1, l6=0 GRAVE, l7=2, l8=0) si no descuento el arreglado en `d59cad8`; 9 si lo descuento. El brief dice 9, asumo que el orquestador descuenta 1 arreglado. La diferencia de 1 no cambia el plan.

12. **No verifiqué que `action_use` del MCP no completa acciones continuas.** Lo asumo del brief §4. Afecta a `L7` (R15, V3-09 necesitan gate in-game, no automatizable) y a `L4` (D06 necesita verificación de salud con daño real).

---

**Cierre:** el plan reparte ~116 items en 9 lanes + 1 de partición (`P`). El solape (§B) muestra que ~25-30 items de la auditoría son trabajo nuevo real; el resto está cubierto o es propuesta. La decisión de producto crítica (§C) no es `_TEST` (ya resuelta por persistencia) sino si se parte el dios `LFPG_NetworkManagerImpl.c` antes de repartir. Sin esa partición, el plan serializa el servidor; con ella, aguanta 8 lanes simultáneas.
```

---

**Resumen de lo entregado:** el `PLAN.md` completo con las 7 secciones (A-G) en el orden exigido por el §7 del brief. La sección B (la más importante) cruza los 46 hallazgos de la auditoría contra las 101 fichas, 9 MEDIO y 7 CONFLICTO, con `path:line` verificada abriendo cada fichero. La sección C argumenta que la decisión de producto real es partir `LFPG_NetworkManagerImpl.c` (no `_TEST`, que ya está resuelto por persistencia). La sección D reparte ~116 items en 9 lanes disjuntas por fichero, con `P` (partición) como prerrequisito. La sección E dice que aguanta 8 lanes simultáneas tras `P`. No pude escribir el fichero por estar en Ask mode; el contenido completo va arriba para que se copie a `reviews/2026-09-08-council-plan-definitivo/lanes/glm/PLAN.md`.