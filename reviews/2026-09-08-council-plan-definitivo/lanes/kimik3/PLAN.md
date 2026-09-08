Voy a ejecutar el brief. Empiezo leyendo las fuentes obligatorias en paralelo.Auditoría leída (46 hallazgos). Ahora leo los 5 informes de triaje y el plan vigente.Ahora los dictámenes de Grok (oleada 1) y el plan conjunto vigente.Ahora el dictamen de l7, el HANDOFF (primer bloque) y verificaciones del árbol real.Última verificación: tamaños actuales de los ficheros grandes (la auditoría los midió antes de las 19 fusiones de hoy).Estoy en Ask mode: no puedo escribir `reviews/2026-09-08-council-plan-definitivo/lanes/kimik3/PLAN.md` en disco. Como pide el brief para ese caso, aquí va el contenido COMPLETO del `PLAN.md`, listo para volcar tal cual en esa ruta. No abrí ningún `PLAN.md` de `<RUN>\lanes\` (ceguera preservada).

---

```markdown
# PLAN DEFINITIVO — LFPowerGrid · lane kimik3

Base leída: auditoría 2026-09-07 (46 hallazgos), los 5 TRIAJE (101 fichas), los 8 DICTAMEN-GROK
(oleadas 1 y 2), INFORME-ASTRA de l7 (7 CONFLICTO), PLAN-CONJUNTO 2026-09-06, HANDOFF (primer
bloque), AGENTS.md y el árbol real (`scripts/`, `config.cpp`, `include.lst`, `gui/`). HEAD de
referencia: `main` post-fusiones de hoy (el brief dice `e5b5303`; el HANDOFF, snapshot anterior,
dice `adfd29c`).

---

## A. CRITICA DEL ENCUADRE

**A1. La decisión que el brief cree viva ya murió esta mañana.** El §5 del encargo presenta
«si el fork `_TEST` sale del release» como la decisión que más cambia el tamaño del plan. Eso era
cierto el 2026-09-07, cuando la auditoría (rama `a187898`) convivía con DOS UIs de sorter. Hoy no:
T0b borró las 5 clases de UI V3 y sus 3 layouts (verificado: en `scripts/4_World/` solo queda
`LFPG_Sorter.c`; los layouts V3 no existen; `gui/layouts/test/` tiene los 4 únicos del sorter).
**El `_TEST` ya no es un fork: es la única UI del sorter.** «Sacar el `_TEST` del release» hoy
significa «el sorter se queda sin panel», una regresión funcional con jugadores reales, no un
ahorro. La auditoría entera sobre el fork (S2, el gate `*TEST*` de su §5) está escrita sobre un
mundo pre-T0b que ya no existe. La decisión real que queda es T0c (renombrar), y su efecto
numérico es ±0 fichas (sección C).

**A2. «~170 items» mezcla universos con solape interno.** 46 auditoría + 92 VIVA + 9 MEDIO +
11 MENOR + 7 CONFLICTO = 165, pero: ~17 de los 46 ya están cubiertos por fichas triadas, 2
muertos hoy en el árbol, 4 son elogios/anti-trabajo; y dentro de las 92 VIVA hay fichas que son
la misma unidad de cambio (S05+S06+S24+H4 son el tick del sorter; G12+G13+H1 son el mismo
broadcast; D08+D09+D23 son el mismo mando). El universo real de **unidades de trabajo** es
~60-70, no 170. La intuición del orquestador se confirma, pero por una razón que él no lista:
no es solo que la auditoría solape con el triaje — es que **parte de la auditoría murió hoy**
(S2 por T0b, H9 por S08, §6.2 por l5, S6-migradores por l8/M-03).

**A3. El cuello de botella no es `LFPG_NetworkManagerImpl.c`: son DOS ficheros, y se pueden
congelar.** El orquestador acierta en que el NM (hoy 7.810 líneas, medido por última línea;
la auditoría midió 6.989 antes de las fusiones — creció 821 hoy) concentra trabajo. Pero
`LFPG_RPCServerHandlerImpl.c` (hoy ~3.088) concentra otros 15 items (SEC04-08, SEC15, SEC17,
S03, S11-S14, H8, 2 MEDIO de l4). El plan que rinde no es «repartir con cuidado», es **congelar
ambos en la oleada 1** (el 60% del trabajo vive fuera de ellos) y serializarlos después. Con eso,
6 lanes corren a la vez sin un solo fichero compartido.

**A4. «Disjuntas por fichero» es necesario pero no suficiente.** Hay fichas cuya unidad de cambio
cruza dos ficheros con dueños distintos: S11/S12 (ACK cliente-servidor), G06 (manager-grafo),
D13 (controller-planner), R27 (HUD-dispatch), S06 (semántica L-E, código en NM). El plan nombra
el contrato de interfaz en cada una de esas lanes; sin eso, la partición por fichero solo mueve
el conflicto del merge al diseño.

**A5. El orden propuesto (cruzar auditoría → decidir `_TEST` → repartir) es inocuo pero no
óptimo.** La decisión `_TEST` ya no bloquea nada (T0c es cosmético salvo classname, y el classname
exige un censo de persistencia que solo el dueño puede hacer sobre los ficheros del servidor). Lo
que sí ordena el plan es la propiedad exclusiva de los dos god-files y la dependencia real
L-A→L-R (FileUtil/Settings antes que los handlers que los consumen).

**A6. Hechos del brief que confirmo con lectura propia:** `_TEST` hereda de `LFPG_Sorter` en
config (`config.cpp:1086`) y ambos `_TEST` son `scope = 2` (`config.cpp:1080-1091`); `units[]`
NO lista `Sorter_TEST` pero sí `LF_TestLamp`/`LF_TestLampHeavy` (`config.cpp:187`, clases en
`:256`/`:263`); `include.lst` = `*.c;*.asi;*.anm;*.paa;*.rvmat;*.layout;*.ogg;*.ptc;*.csv` (sin
`.p3d`/`.cpp`/`.cfg`); los 11 SubIds V3 (`LFPG_Defines.c:294-309`) y los 11 espejo TEST
(`:325-335`) siguen declarados; el lease de searchlight de V3-09 YA está en main
(`LFPG_ControlSessionRegistry.c:136`); UIScaler V4 y gates de sonda/MCP YA están en el árbol
(`LFPG_SorterView_TEST.c:83`, `:1584`, `:1652`, `#ifdef DIAG_DEVELOPER` en `:177`/`:1988`).
No re-ejecuté el linter ni git (shell bloqueado por hooks del entorno): esos dos hechos los tomo
del brief §4.

---

## B. SOLAPE AUDITORIA <-> TRABAJO YA TRIADO

Los 46 hallazgos: 34 numerados (S1-S7, H1-H9, C1-C12, U1-U6) + 5 viñetas §5 + 7 propuestas §6.
«Cubierto» cita la ficha/MEDIO/CONFLICTO con su evidencia tal como la leí en el informe de triaje
o dictamen correspondiente; los anclajes que abrí yo mismo en el árbol van marcados con **[yo]**.

| hallazgo | cubierto_por | evidencia (path:line) |
|---|---|---|
| S1 dios NetworkManagerImpl | **NUEVO** (como épica de refactor). Su contenido funcional YA son las fichas G/H/SEC que viven dentro | Fichero hoy de 7.810 ln **[yo]**; fichas internas: G03 `LFPG_NetworkManagerImpl.c:3315`, G05 `:2005`, G12 `:2069`, G23 `:2717` (TRIAJE-grafo) |
| S2 fork TEST divergente | **SUPERADO por T0b** (hecho hoy). Ya no hay duplicación V3↔V4 que unificar; queda T0c | Solo queda `LFPG_Sorter.c` en 4_World **[yo]**; `config.cpp:1080-1091` **[yo]**; DICTAMEN l6 (5 clases UI y 3 layouts borrados); comentario fork vivo y falso en `test/LFPG_SorterView_TEST.c:33-35` **[yo]** |
| S3 reflexión `Call*` | D10 (VIVA) + D19 (VIVA), **parcial**. El borrado total de `Call*` y la migración de las 3 legado es NUEVO y va a F (riesgo persistencia: `LFPG_Generator` es producción, `config.cpp:249` **[yo]**) | D10: `LFPG_IDevice.c:507`/`:840`/`:857`/`:873`; verifiqué que `GetPortWorldPos` (`:507-513`) va directo a `CallVector` sin fast-path **[yo]**; `Call*` en `:433-482` **[yo]**. D19: `LFPG_TestDevices.c:708` → `LFPG_WireHelper.c:404` |
| S4 ecosistema BTC 10.200 ln | Defectos: SEC12/13/14 (VIVA) + err=14, V3-02/03/05, CX-H04/H05 (CONFLICTO). «Fusionar registries» = U2. «Feature-freeze» = decisión del dueño (§6.7), no ficha | SEC12 `LFPG_BTCSessionRegistry.c:375`, SEC13 `:341`, SEC14 `:137`; CONFLICTOs: tabla de `l7-debt-v3/INFORME-ASTRA.md`; 7 `HandleBTC*` confirmados (`LFPG_BTCHelper.c:980`-`:2610`) **[yo]** |
| S5 acciones y chrome ×3 | D18 (VIVA) para acciones. El chrome V3 **murió con T0b** (`LFPG_SorterView.c:1648` ya no existe). El patrón MissionInit ×3 sigue vivo pero ahora es CCTV+sorterTEST+BTC | D18: `LFPG_SwitchV2.c:136` vs `LFPG_SwitchV2Remote.c:72`; `MissionInit.c:170`/`:181`/`:191` **[yo]** |
| S6 capas que no hacen nada | SEC18 (VIVA, migradores huérfanos) + M-03 (ya portado por l8: comentarios e Info inline, verificado en `LFPG_Migrators.c:56`/`:82` **[yo]**) + D05 (hecho, l1). Los «hooks vacíos» son seams documentados: NO tocar (→F) | SEC18: `LFPG_Migrators.c:40` sin llamadores; hooks: `lfpg_devicebase.c:575-587` **[yo]**, `LFPG_WireOwnerBase.c:293-296` **[yo]** |
| S7 Defines.c mezcla todo | **NUEVO** (deuda estructural). Lo accionable hoy: los 11 SubIds V3 muertos tras T0b | `LFPG_Defines.c:294-309` (V3, sin consumidor cliente desde l6) y `:325-335` (TEST vivos) **[yo]** |
| H1 BroadcastOwnerWires doble barrido + JSON | G12 (VIVA) + G13 (VIVA) | G12: `LFPG_NetworkManagerImpl.c:2013`/`:2070`; G13: `:2395`/`:2399` y `:2539`/`:2552` (TRIAJE-grafo) |
| H2 CleanDisappearedVanillaDevice scan por prefijo | **NUEVO** (no es ninguna ficha G; G03 es otra función) | Auditoría: `LFPG_NetworkManagerImpl.c:3380-3477` (líneas pre-merge; localizar por símbolo) |
| H3 CutAllWiresFromDevice fallback O(V·W) | **NUEVO** (G06 es el rebuild global que programa CutAll; H3 es el fallback interno — mecanismos distintos) | Auditoría: `LFPG_NetworkManagerImpl.c:4930-4996` |
| H4 tick sorter: resoluciones + sqrt + dedup | S05 (VIVA) + S07 (VIVA) + S24 (VIVA) | S05: `LFPG_NetworkManagerImpl.c:6077`; S07: `LFPG_SorterLogic.c:576`; S24: `:6066`/`:6174` (TRIAJE-sorter) |
| H5 TickWaterPumps triple barrido | **NUEVO** (la cura es U5, de la propia auditoría; ninguna ficha D/G cubre pumps) | Auditoría: `LFPG_NetworkManagerImpl.c:5438-5720` |
| H6 celdas O(P·C) + doble rebuild | U1 (propuesta auditoría) cubre el doble rebuild; el `map<int,int>` celda→índice es **NUEVO** | Auditoría: `LFPG_NetworkManagerImpl.c:7003`, `:7047-7054`, `:7111-7131` |
| H7 ElecGraph rebuild O(N+E) + sync 3 resoluciones | G20 (VIVA) + G10 (VIVA), parcial; la amortización de `SyncNodeToEntity` es NUEVO menor | G20: `LFPG_ElecGraphImpl.c:237`; G10: `:265` (TRIAJE-grafo) |
| H8 FinishWiring alloc+ToString+sqrt pre-validación | SEC17 (VIVA) + SEC05 (VIVA) | SEC17: `LFPG_RPCServerHandlerImpl.c:465` + `LFPG_ConnectionRules.c:131`; SEC05: `:230` (TRIAJE-red-seguridad) |
| H9 RepackCargoInPlace 9 new + insertion O(n²) | **MUERTO hoy** por S08 (l3): tope 64 ítems, presupuesto de celdas, 4 pasadas. Residual = MEDIO-l3 (repack incompleto >64 → §6.5) | `LFPG_SorterLogic.c:1133`/`:1231`/`:1268` (DICTAMEN l3) |
| C1 sway invalida caché | R10 (VIVA), **parcial**: R10 cubre las claves de proyección y nombra el sway; el fix concreto (gate 25 m + cuantizar `nowMs`) es NUEVO | R10: `LFPG_CableRenderer.c:3113`/`:3292`/`:3632` (TRIAJE-render-ui) |
| C2 tres pasadas O(n)/frame | **NUEVO** (micro-op) | Auditoría: `LFPG_CableRenderer.c:2921-2950` |
| C3 invariantes por subsegmento | **NUEVO** (micro-op) | Auditoría: `LFPG_CableRenderer.c:3391-3395`, `:3448-3461` |
| C4 ClipSegToScreen/EdgeFade solo en miss | **NO-TRABAJO** (elogio: «bien diseñado») | `LFPG_WorldUtil.c:302-392` (auditoría) |
| C5 player-occlusion 2 GetScreenPos/frame | **NUEVO** | Auditoría: `LFPG_CableRenderer.c:2853-2906` |
| C6 selection sort O(n²) gateado | R05 (VIVA) | `LFPG_CableRenderer.c:2719`/`:2729`/`:2734` (TRIAJE-render-ui) |
| C7 oclusión bien presupuestada | **NO-TRABAJO** (elogio: «no tocar») | `LFPG_Defines.c:234-235` (auditoría) |
| C8 CullTick 1 sqrt/wire | **NUEVO** (trivial), vecino de R05/R08 | Auditoría: `LFPG_CableRenderer.c:2561` |
| C9 preview solo con sesión | **NO-TRABAJO** (elogio) | `LFPG_WiringClient.c:615-1062` (auditoría) |
| C10 inspector GetScreenSize+SetPos/frame | R28 (VIVA), **parcial**: cubre lecturas duplicadas; el `SetPos` por frame es NUEVO | R28: `LFPG_DeviceInspector.c:538-563` (TRIAJE); `GetScreenSize:1639-1641` (auditoría) |
| C11 láser sin caché entre frames | R18/R19 (VIVA), **parcial**: cubren CullTick/frustum; la caché de proyección por beam es NUEVO | R18: `LFPG_LaserBeamRenderer.c:101-104`; R19: `:41`/`:68` (TRIAJE) |
| C12 joints ×4 DrawLine | **NUEVO** (micro) | Auditoría: `LFPG_CableHUD.c:349-352` |
| U1 un rebuild de celdas por turno | **NUEVO** como trabajo (cura de H6, ~10 líneas) | Auditoría: `LFPG_NetworkManagerImpl.c:6946-6947`, `:7199-7207`, flag en `:618` |
| U2 sesiones a 1 s + fusionar gemelos | **NUEVO** (refactor). Ojo: la mitad de su motivación (sesión foco sin plazo) ya la curó V3-09 | Lease verificado: `LFPG_ControlSessionRegistry.c:136` + `RenewSearchlightLease:152-161` **[yo]**; gemelos: `LFPG_BTCSessionRegistry.c` (399 ln) vs `LFPG_ControlSessionRegistry.c` (453 ln) |
| U3 one-shots al scheduler (leak LIFE-008) | **NUEVO**. Es un leak real: `CallLater(SYSTEM)` sobrevive a `OnMissionFinish` | Auditoría: `ValidateAllWires` `:504`, `DoGlobalSelfHeal` `:3726`, `DeferredVanillaPrune` `:3973`, `PostBulkRebuild` `:5068`, `RunOrphanSweep` `MissionInit.c:55` |
| U4 cola diferida única con coalescencia | D09 (VIVA), **parcial**: cubre el daño (mando + MemoryCell); la infra de cola única es NUEVO (→F salvo que D09 la exija) | D09: `LFPG_RemoteController.c:539`/`:559`, `LFPG_MemoryCell.c:184`/`:267` (TRIAJE-dispositivos) |
| U5 censo único por turno | **NUEVO** (cura de H5) | Auditoría: `LFPG_NetworkManagerImpl.c:6841-6848` |
| U6 cliente: 1 maintenance tick + nearby cache | **NUEVO** como unificación; los síntomas concretos son R16 (VIVA) y R17 (VIVA) | R16: `LFPG_TankHUD.c:122`/`:169`; R17: `LFPG_SearchlightController.c:351` (TRIAJE) |
| §5.1 techo `.p3d` (gates+lids ≈230 MB) | **NUEVO** — pero es trabajo de ARTE y decisión del dueño, no una lane de script (→F el remodelado; el censo sí entra en L-H1) | Auditoría §5 (tamaños medidos en la rama auditada; NO verificables por mí, ver G) |
| §5.2 junk (kitboxtexture, stereo_backup, Furnace_mono, water.ogg) | **NUEVO**, parcialmente refutado: 0 refs de `kitboxtexture`/`Furnace_mono` en `config.cpp` **[yo]**; los ficheros no son visibles para mí (ver G) — censo previo obligatorio | `config.cpp:90` usa `Furnace` (sin `_mono`) **[yo]** |
| §5.3 duplicados SHA256 (.paa/.rvmat) | **NUEVO** (assets; censo en L-H1) | Auditoría §5 |
| §5.4 compile: 51/122 sin guard, units[], include.lst, reviews/ | **NUEVO**. Verificado: `units[]` con `LF_TestLamp`/`Heavy` **[yo]**; `include.lst` sin `.p3d`/`.cpp`/`.cfg` **[yo]**; `reviews/` existe en el root **[yo]**. Matiz post-T0b: «TEST en server» ya no es queja aplicable — el TEST ES la UI de producción | `config.cpp:187` **[yo]**; `include.lst` (1 línea) **[yo]** |
| §5.5 gates de build propuestos | **NUEVO** (CI). Ojo: el gate «`*TEST*` en staged/units[]» choca con la realidad post-T0b — hay que reescribirlo tras la decisión T0c | `config.cpp:1080-1091` (los `_TEST` son producción) **[yo]** |
| §6.1 MAX_WIRES 128 vs 64 | **NO-TRABAJO** (la propia auditoría dice «no unificar») | `LFPG_Defines.c:220` y `:18` (auditoría) |
| §6.2 S1_PROBE fuera de PERFDIAG + UIScaler ausente en V4 | **MUERTO hoy** (l5: D-02 y gate S1_PROBE, dictamen VERDE) | `LFPG_SorterView_TEST.c:83` (contexto `extends LFPG_UIScaler`), `:1584` (`ComputeScale`), `:1652` (`LFPG_PERFDIAG_ENABLED`), MCP tras `#ifdef DIAG_DEVELOPER` (`:177`, `:1988`) **[yo]** |
| §6.3 OnKeyPress/Release consumen sin `super` | **NUEVO**; verificado vivo | `LFPG_MissionInit.c:170-174` (CCTV), `:186`/`:195` (returns sin super), `:210-225` **[yo]** |
| §6.4 `Widget.SetLV(0)/SetTextLV(0)` globales | R30 (VIVA) | `LFPG_MissionInit.c:140-141` **[yo]**; TRIAJE-render-ui R30 |
| §6.5 repack: avisar n>64 + early-out TryPlaceOnGrid | MEDIO-l3 (residual de S08) | `LFPG_SorterLogic.c:1133` (DICTAMEN l3); `TryPlaceOnGrid:1339` (auditoría) |
| §6.6 documentar split ElecGraph 4_World/5_Mission | **NUEVO** (trivial, doc) | Cabecera de `scripts/4_World/LFPG_ElecGraph.c` |
| §6.7 congelar BTC/CCTV como feature-frozen | **NUEVO** (decisión del dueño; complementa S4) | — |

**Síntesis numérica de B:** de 46 hallazgos — **17** ya cubiertos (total o parcialmente) por
fichas/MEDIO/CONFLICTO triados · **2** muertos hoy en el árbol (S2, §6.2) · **4** son elogios o
anti-trabajo (C4, C7, C9, §6.1) · **23 NUEVOS**. De los 23 nuevos: **7 funcionales reales**
(H2, H3, H5, U1, U3, U5, §6.3), **5 micro-ops de cliente** (C2, C3, C5, C8, C12), **4 refactors
estructurales** (S1, S7, U2, U6 — mi recomendación: casi todos a F), **5 de assets/build**
(§5.1-§5.5), **2 de doc/decisión** (§6.6, §6.7). El trabajo nuevo FUNCIONAL que aporta la
auditoría son 6-7 items, no 46.

---

## C. LA DECISION DE PRODUCTO

**El veredicto: la decisión «¿sale el `_TEST` del release?» ya no existe; fue respondida por T0b
esta mañana.** Con la V3 borrada, el `_TEST` es la UI de producción del sorter. Las tres opciones
reales que quedan:

| opción | qué es | consecuencia numérica |
|---|---|---|
| **C-1. Renombrar todo `_TEST` → canónico (T0c completo)** | 6 `.c` de `test/`, 4 layouts, 11 SubIds (`LFPG_Defines.c:325-335`), stringtable, y los 2 classnames de `config.cpp:1080`/`:1086` | Cierra T0c y habilita el gate §5.5. **Cierra 0 fichas del triaje** (S11/S20/S21/S23 viven en esos ficheros con cualquier nombre). Riesgo: si algún mundo tiene `LFPG_Sorter_TEST` colocado, el rename rompe esa entidad (persistencia, AGENTS.md §4) |
| **C-2. Conservar `_TEST` para siempre** | Solo quitar el `[V4 TEST]` de `displayName` (`config.cpp:1083`, `:1089` — seguro, no toca persistencia) y dejar constancia | 0 riesgo, 0 trabajo de rename. El gate §5.5 hay que reescribirlo con whitelist de estos 2 classnames. Deuda cosmética permanente |
| **C-3. Mixta (MI RECOMENDACIÓN)** | Censo de persistencia del servidor real (buscar `LFPG_Sorter_TEST` en el storage): si es cero → C-1 completo; si hay alguno → rename de todo lo que NO está en config (clases UI, layouts, SubIds: 0 riesgo) y classnames se quedan | El grueso del beneficio (nombre limpio en código y gates) sin el riesgo. El censo lo hace el dueño con los ficheros del servidor; yo no puedo (→G) |

**Lo que la decisión NO cambia:** las 7 fichas/items que viven en ficheros `_TEST` (S10, S11,
S20, S21, S23, l5-MEDIO, l6-MENOR×2) no desaparecen con ninguna opción que mantenga el panel.
S10 se cierra sin código: dos lanes independientes la marcaron NO-LOCALIZADA (dictámenes l5 y
TRIAJE-sorter) — el HANDOFF ya lo anticipa.

**Las dos decisiones que SÍ cambian el tamaño del plan** (el brief pregunta «si crees que la
decisión relevante es otra, dilo»):

1. **¿Partir `LFPG_NetworkManagerImpl.c` (S1) como épica?** Si sí: +1 lane gigante que
   serializa TODAS las demás lanes de NM y exige re-verificación in-game de medio mod. Si no:
   el NM se arregla por dentro en 2 lanes serializadas (L-NM1, L-NM2) y la extracción por dominio
   queda como opción futura («split on touch»). **Recomiendo NO** (→F): el fichero creció 821
   líneas hoy por fusiones verdes; la deuda es real pero el big-bang es el peor momento y el
   mayor riesgo del plan.
2. **¿Feature-freeze de BTC (S4/§6.7)?** Si sí: caen del plan U2 (fusión de registries) y
   cualquier refactor estructural BTC (~2 items grandes). **Los 7 CONFLICTO y SEC12/13/14 NO
   caen**: son integridad del dinero con jugadores reales, no features. Recomiendo SÍ congelar
   features y NO tocar nada estructural de BTC que no sea integridad.

---

## D. EL PLAN: LANES

Reglas de partición: un fichero = un lane por oleada. Los dos god-files
(`LFPG_NetworkManagerImpl.c`, `LFPG_RPCServerHandlerImpl.c`) están **congelados en la oleada 1**
y se serializan después. `config.cpp` lo toca **un solo lane en todo el plan** (L-F). Gates
comunes a todas: delta del linter offline (`script_validator.py`, gatear por `len(errors)` = 0
nuevos, exit 2 = aprobado), `ui_reconcile.py` si tocan layouts o `#STR_`, y UN gate in-game por
oleada (batch, DZ-R5). Ninguna lane propone borrar/renombrar classnames de `CfgVehicles` salvo
L-F bajo censo explícito (C-3).

### Oleada 1 — 6 lanes en paralelo (god-files congelados)

**L-A · dinero-y-persistencia** (la lane data-crítica, R9)
- **Ficheros exclusivos:** `scripts/5_Mission/LFPG_BTCHelper.c`, `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c`, `scripts/5_Mission/LFPG_BTCSessionRegistry.c`, `scripts/4_World/LFPG_BTCAtm.c`, `scripts/4_World/LFPG_AtmStock.c`, `scripts/3_Game/LFPG_FileUtil.c`, `scripts/3_Game/LFPG_Settings.c`, `scripts/4_World/LFPG_DeviceRegistry.c`
- **Items que cierra:** los 7 CONFLICTO (V3-02 cash ledger↔inventario `:2582`/`:2790`; V3-03 fold físico `BalanceProvider_NativeImpl.c:1105`; V3-05 cuarentena de claims `:571`/`:2076`; V3-06 relectura del target `LFPG_FileUtil.c:271`/`:282`; CX-H04 depósito físico `BTCHelper.c:2383`; CX-H05 retirada `:2192`; err=14 saldo cero en 5 operaciones `:1640`/`:2127`/`:2294`/`:2515`/`:2697`) · SEC10 (fallback a backup, `LFPG_FileUtil.c:339`) · SEC11 (Load sin staging, `LFPG_Settings.c:538`) · SEC12 (FIFO roto, `BTCSessionRegistry.c:375`) · SEC13 (salto de secuencia — **ya mitigado por CX-E portado en l7**: verificar y cerrar, `:296`/`:332`) · SEC14 (retención por UID, `:137`) · SEC16 (protocolo duplicado, `LFPG_FileUtil.c:81`/`:169`/`:257`) · SEC19 (lectura completa para `ver`, `:755`/`:761`) · los 2 MEDIO de l7 (`GetAll` oculta duplicados latched `LFPG_DeviceRegistry.c:126`/`:165`; latch congela el sweep `:79` + `BalanceProvider_NativeImpl.c:1504`).
- **Gate:** linter delta 0 errores nuevos · matriz de fallos CON inyección: kill/restart entre cada par de E/S monetaria (buy/sell/withdraw/deposit/cash con cap+1), target ilegible con backup válido → restaura, settings corrupto → defaults publicados, dos ATMs con mismo ID → latch se levanta cuando no quedan vivos y el sweep avanza · **auditor independiente por ángulos (race/persistence/data-loss) antes de declararla release-safe** (R9, no negociable: es el camino del dinero).
- **Depende de:** nadie.

**L-B · grafo eléctrico**
- **Ficheros exclusivos:** `scripts/5_Mission/LFPG_ElecGraphImpl.c`, `scripts/3_Game/LFPG_Data.c`, `scripts/4_World/LFPG_ElecGraph.c` (solo cabecera de documentación §6.6)
- **Items:** G08 (cargador atado al barrido global, `:3175`) · G09 (límite global sobre estado previo, `:443`) · G10 (rebuild elude límites/ciclos, `:265`) · G11 (tiempo de propagación obsoleto, `:2074`) · G14 (doble copia al compactar, `:2749`) · G15 (metadatos en mapas paralelos, `:62`/`:137` + `LFPG_Data.c:264`) · G16-lectura-B (hidratación duplicada, `:1259`/`:3296` — absorber en G20) · G19 (booleano consume visitas, `:3608`) · G20 (nodos desechados + hidratación doble, `:237`) · H7-residual (amortizar `SyncNodeToEntity`) · MEDIO-l2 (la pasada extra de G01 cobra visitas al presupuesto, `:3789`) · §6.6 (doc del split).
- **Gate:** linter · in-game: cargador de baterías con red grande y cola ocupada (G08), rebuild tras importar persistencia con ciclo (G10), telemetría sin acumulación espuria (G11).
- **Depende de:** nadie. **Contrato hacia L-NM2:** la interfaz para preservar estado transitorio de baterías en rebuild (G06) se define por escrito en el brief de esta lane.

**L-C · renderer de cables**
- **Ficheros exclusivos:** `scripts/4_World/LFPG_CableRenderer.c`, `scripts/4_World/LFPG_CableParticle.c`, `scripts/4_World/LFPG_CableHUD.c`, `scripts/4_World/LFPG_WiringClient.c`, `scripts/4_World/LFPG_WorldUtil.c`
- **Items:** R01 (occ samples antes de joints, `:2288`/`:2299`) · R03 (purga sin invalidar cachés, `:2633-2635`) · R05 (selection sort, `:2729`) · R06 (rebuild global por edición local, `:1336`/`:1875`) · R07 (delta sin delta de geometría, `:1873`/`:1876`) · R08 (early-out por origen, `:2413`) · R09 (`HasRenderableWires` cuenta metadatos, `:892`) · R10 (claves sin FOV/roll, `:3113`/`:3292`/`:3632`) · R11 (movimiento frame-a-frame, `:2783-2798`) · R20 (snapshot baja generación, `:1306`/`:1360`) · R21 (sin dedup en vuelo, `:1639`-`:1679`) · R22-parte-renderer (`EstimateSegments:4159`, `cachedWireKey:173`) · R24-parte-renderer · R26 (política de aparición optimista; la parte distancia ya murió con R04) · R27 (frame 0×0 no invalida — **sin cambiar el contrato de `BeginFrame`**; si lo exige, se mueve a oleada 2 con L-D) · C1 (sway, gate por distancia + cuantizar) · C2 · C3 · C5 · C6 (=R05) · C8 (sqrt `:2561`) · C12 (joints ×4, `LFPG_CableHUD.c:349-352`) · MEDIO-l3-R04 (reconciliador mire `segments.Count()==0`, `:4067`).
- **NO entra:** R23 (→F).
- **Gate:** linter + `ui_reconcile` · in-game visual: cable con esquinas tras pared (R01), cable largo junto al destino (R08), zoom/FOV (R10), alt-tab con 0×0 (R27), delta de un cable no reconstruye al vecino (R07).
- **Depende de:** nadie.

**L-D · UI varios (inspector, CCTV, láser, foco, TankHUD, misión cliente)**
- **Ficheros exclusivos:** `scripts/4_World/LFPG_DeviceInspector.c`, `scripts/4_World/LFPG_Camera.c`, `scripts/4_World/LFPG_CameraViewport.c`, `scripts/4_World/LFPG_LaserBeamRenderer.c`, `scripts/4_World/LFPG_SearchlightController.c`, `scripts/4_World/LFPG_Searchlight.c`, `scripts/5_Mission/LFPG_TankHUD.c`, `scripts/5_Mission/LFPG_MissionInit.c`
- **Items:** R12 (inspector sin renovación eléctrica, `:523`/`:1337`) · R13 (offsets incompatibles, `:1286`/`:1418`) · R28 (lecturas duplicadas, `:538-563`) · C10 (`GetScreenSize`/`SetPos` por frame) · R14 (`SafeAbort` sin identidad, `LFPG_Camera.c:100`/`:107`) · R29 (etiquetas CCTV sin resolución, `LFPG_CameraViewport.c:271`/`:323`) · R22-parte-CCTV (`m_ScanlineOffset:1107`) · R17 (predicción foco sin cambio, `LFPG_SearchlightController.c:351` + `LFPG_Searchlight.c:570`) · R16 (sondeo negativo 4 Hz, `LFPG_TankHUD.c:122`/`:169`) · R18/R19 (láser: alta cuadrática + frustum a 250 ms, `LFPG_LaserBeamRenderer.c:101`/`:41`) · C11 (caché de proyección láser) · R30 (`SetLV(0)` global, `LFPG_MissionInit.c:140-141` **[yo]**) · §6.3 (reenviar teclas no consumidas, `:170-200` **[yo]**) · U6 (maintenance tick único + 1 `GetPlayer`/frame) · MEDIO-l3-R15 (**confirmar in-game que `GetCurrentCamera()` es null tras `SelectPlayer` con la spectator activa** — pendiente desde l3; si no lo es, el predicado de `TryCompleteExit:787` es circular y hay que rediseñar la salida).
- **Gate:** linter + `ui_reconcile` · in-game: inspector refleja cambio eléctrico remoto (R12), salida CCTV completa (R15), foco inmóvil sin trabajo por frame (R17), brillo de widgets ajenos no tocado (R30), hotkeys de otros mods no robadas (§6.3).
- **Depende de:** nadie.

**L-E · núcleo del sorter (parser, lógica, entidad)**
- **Ficheros exclusivos:** `scripts/3_Game/LFPG_SorterData.c`, `scripts/5_Mission/LFPG_SorterLogic.c`, `scripts/4_World/LFPG_Sorter.c`
- **Items:** S01 (Reset All no guarda, `LFPG_SorterData.c:594`) · S02 (parser parcial y `ca` sin frontera, `:332`/`:492`) · S07 (SLOT parseado por comparación, `LFPG_SorterLogic.c:576`) · S15 (validez espacial asimétrica del vínculo, `:1012`) · S16 (NetworkID como vínculo persistente, `LFPG_Sorter.c:334`) · S17 (attachment-only sin consumidor, `:373`/`:480`) · S18 (**primero sonda in-game gateada por diag**: si el motor indexa `itemSize 0/1` por ruta, no hay bug y se cierra sin tocar `:950`) · S19 (ghillie → ATTACHMENT, `:748`) · §6.5-parte-lógica (early-out de área en `TryPlaceOnGrid:1339`) · contrato «mejor esfuerzo acotado» del repack (MEDIO-l3-S08, con L-F para el aviso UI).
- **NO entra:** S05/S06/S24 (viven en el NM → L-NM2), S03/S12/S13/S14 (handler → L-R), S09/S11/S20/S21/S23 (UI → L-F).
- **Gate:** linter · in-game: Reset All persiste vacío (S01), JSON malformado no desplaza salidas (S02), ghillie clasifica CLOTHING (S19), contenedor movido fuera de rango deja de rutar (S15), reinicio con orden de carga distinto no reenlaza al contenedor equivocado (S16).
- **Depende de:** nadie. **Contrato hacia L-NM2:** la semántica de reanudación (S06: máscara de cables + generación) se fija por escrito aquí; L-NM2 la implementa en el tick.

**L-G · dispositivos**
- **Ficheros exclusivos:** `scripts/4_World/LFPG_ActionDismantleDevice.c`, `LFPG_ActionUpgradeSolarPanel.c`, `LFPG_ActionUpgradeWaterPump.c`, `LFPG_RemoteController.c`, `LFPG_IDevice.c`, `LFPG_Intercom.c`, `LFPG_DoorController.c`, `LFPG_Furnace.c`, `lfpg_devicebase.c`, `LFPG_MotionSensor.c`, `LFPG_SwitchV2.c`, `LFPG_SwitchV2Remote.c`, `LFPG_SwitchRemote.c`, `LFPG_TestDevices.c`, `LFPG_BatteryAdapter.c`, `LFPG_MemoryCell.c`, `LFPG_KitBase.c`, `LFPG_KitBaseDeployable.c`, `LFPG_ActionPairRemote.c`, `LFPG_ActionPairSensor.c`
- **Items:** D06 (salud dispositivo↔kit, `LFPG_ActionDismantleDevice.c:182`/`:211` + ambas bases de kit) · D07 (sobrantes y filtro sin estado, `LFPG_ActionUpgradeWaterPump.c:148`/`:201`/`:218`) · D08 (sync al cambiar de dueño, `LFPG_RemoteController.c:365`) · D09 (callbacks sin cancelación, `:539`/`:559` + `LFPG_MemoryCell.c:184`/`:267`) · D10 (4 fast-path de puertos, `LFPG_IDevice.c:507`/`:840`/`:857`/`:873` **[yo verifiqué :507]**) · D11 (reescritura visual idéntica, `LFPG_Intercom.c:380`) · D12 (RF con consulta global, `:628` — **solo si una consulta espacial dentro de `LFPG_Intercom.c` es cobertura-equivalente; si no, documentar y deferir**: `LFPG_DeviceRegistry.c` es de L-A) · D13 (parte controller: guarda antes del `GetPlayers`, `LFPG_DoorController.c:385`/`:396`; la parte planner `:6888` es L-NM2) · D14 (estimación ≠ política servidor, `LFPG_Furnace.c:781`/`:795`) · D15 (validación de persistencia: `lfpg_devicebase.c:71`, `LFPG_RemoteController.c:621`/`:642`, `LFPG_MotionSensor.c:343`, `LFPG_Intercom.c:432`) · D17 (emparejamiento por nombre de grupo, `LFPG_ActionPairSensor.c:88` — primero verificar qué ID estable expone LBmaster_Groups) · D18 (duplicación switches/upgrades) · D19 (JSON generador sin caché, `LFPG_TestDevices.c:708`) · D20 (tras decisión del dueño sobre política Debug) · D21 (umbral `searchRadius + 1.0`, `LFPG_IDevice.c:199`) · D22 (EEInit post-super en proyección, `LFPG_Furnace.c:129`/`:147`) · D23 (selección cuadrática en guardado, `LFPG_RemoteController.c:582`) · D24 (getter cliente lee referencia servidor, `LFPG_BatteryAdapter.c:477`) · MENOR-l1 (comentario mentiroso, `LFPG_ActionDismantleDevice.c:27`).
- **NO entra:** borrar `Call*` ni migrar la herencia de las 3 clases legado (→F).
- **Gate:** linter · in-game: ciclo dispositivo→kit→dispositivo conserva salud (D06), upgrade de bomba conserva subtipo de filtro (D07), mando transferido muestra parejas reales (D08), horno whitelist: estimación = consumo (D14), proyección de holograma no crea fuente térmica (D22), adaptador muestra energía en cliente (D24).
- **Depende de:** nadie.

### Oleada 2 — 4 lanes en paralelo

**L-R · handler RPC de servidor**
- **Ficheros exclusivos:** `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`, `scripts/4_World/LFPG_RPCGuard.c`, `scripts/3_Game/LFPG_ConnectionRules.c`, `scripts/3_Game/LFPG_WireHelper.c`, `scripts/5_Mission/LFPG_ControlSessionRegistry.c`
- **Items:** SEC04 (rescate global incondicional, `:1872`) · SEC05 (rechazo con spam, `:230` + manager) · SEC06 (dedup no cruza lotes, `:2213`/`:2238`) · SEC07 (ID sin sanear a `Warn`, `:268`/`:311`) · SEC08-residual (**el plazo ya existe** — lease verificado en `LFPG_ControlSessionRegistry.c:136` **[yo]**; queda la distancia en el tick `:404`) · SEC15 (documentar cobertura real de las 8 políticas; NO aplicar `Authorize` indiscriminado → F parcial) · SEC17 (doble pasada de geometría, `:465`) · S03 (unificar umbral Save/Preview, `:2668` vs `:2899`) · S11-servidor (ACKs terminales en todos los rechazos, `:2654`/`:2733`) · S12 (**diseña el contrato**: entidad + revisión de config en SAVE_ACK/SORT_ACK/PREVIEW_RESPONSE, `:2711`/`:2780`/`:3034`) · S13 (tras decisión del dueño; default: documentar preview como filter-match) · S14 (preview sin política de acceso, `:2956`) · H8 (validar barato primero) · MEDIO-l4 ×2 (reemplazo al techo denegado `:608`/`:811` — reservar descontando las filas autorizadas a salir; barrido fail-closed por owner concreto `:760`/`:777`/`:790`, no abortar el `FinishWiring` del mundo) · MENOR-l4 (Warn en denegaciones de puerto, `:398`) · MENOR-l7 (enter CCTV en vehículo responde al cliente, `:1241`).
- **Gate:** linter · in-game: ráfaga de rechazos con log acotado (SEC05), lote repetido sin re-marcado (SEC06), preview sobre contenedor bloqueado no enumera (S14), Save/Sort rechazados no bloquean el botón (S11, con L-F), reemplazo a capacidad exacta admitido (MEDIO-l4).
- **Depende de:** L-A (FileUtil/Settings ya integrados) y L-E (parser SorterData ya corregido: S02 interactúa con `:2942`). **Contrato S12 pre-acordado con L-F antes de abrir la oleada.**

**L-NM1 · NetworkManager: sync, broadcast, cuotas**
- **Ficheros exclusivos:** `scripts/5_Mission/LFPG_NetworkManagerImpl.c` (único lane con este fichero en la oleada)
- **Items:** G03 (seguimiento de fuentes vanilla solo-OUT, `:3315`) · G05 (mutex global de FullSync, `:2701`) · G07 (cuotas infladas tras poda — recuento existente `:1505`, basta una llamada) · G12+G13+H1 (prefiltro/envío duplicado + serialización antes de receptores, `:2013`/`:2070`/`:2399`) · G16-lectura-A (`ReverseIdxAdd` vs `ReverseOwnersInsert`, `:1376` vs `:1271`) · G21 (consulta legacy con owners obsoletos, `:1472`) · G22 (SOLO rate-limit del log del fallback; la recuperación completa → F) · G23 (`Remove(0)` → `RemoveOrdered`, `:2717`) · H2 (scan por prefijo en `CleanDisappearedVanillaDevice`) · H3 (fallback O(V·W) en `CutAllWiresFromDevice`).
- **Gate:** linter · in-game: JIP con red grande no retrasa deltas de terceros (G05), cuota liberada tras poda (G07), FullSync FIFO con 4 pendientes (G23), fuente vanilla solo-OUT bajo seguimiento (G03).
- **Depende de:** L-B (interfaz del grafo estable).

**L-F · UI del sorter + T0c + config.cpp**
- **Ficheros exclusivos:** los 6 `.c` de `scripts/4_World/test/`, `gui/layouts/test/` (4 layouts), `scripts/4_World/LFPG_RPCClientHandler.c`, `scripts/3_Game/LFPG_Defines.c`, `scripts/4_World/LFPG_Actions.c`, `scripts/4_World/LFPG_ActionSyncSorter.c`, `stringtable.csv`, **`config.cpp` (único lane que lo toca en todo el plan)**
- **Items:** S09-cliente (refrescar ante ACK aunque `movedCount=0`, `LFPG_RPCClientHandler.c:967`) · S11-cliente (liberar Save/Sort sin ACK) · S20 (reconstrucción sin cambio, `LFPG_SorterController_TEST.c:907`/`:1509`) · S21 (rail no actualiza, `:1377`) · S23 (estado de chips sin consumidor, `LFPG_SorterTagView_TEST.c:17-30`) · MEDIO-l5 (generación/invalidación de preview en vuelo, `:1709`/`:1768`) · MENOR-l6 ×2 (comentario de fork falso, `LFPG_SorterView_TEST.c:33-35` **[yo]**; indentación) · MENOR-l5 (guarda de `SorterPanel` antes de `CapturePanel`, `:1384`) · **T0c según C-3**: displayName sin `[V4 TEST]` siempre; rename de clases UI/layouts/SubIds si el dueño aprueba; classnames solo con censo verde · limpieza de los 11 SubIds V3 muertos (`LFPG_Defines.c:294-309` **[yo]**) · `units[]`: sacar `LF_TestLamp`/`LF_TestLampHeavy` de la lista (`config.cpp:187` **[yo]** — quitar de `units[]` NO toca persistencia; las clases se quedan declaradas).
- **Gate:** linter + **`ui_reconcile.py --strict`** (rename de layouts/widgets es su caso exacto) · in-game: el panel abre sobre un `LFPG_Sorter` V3 ya colocado (regresión de T0b), el rail actualiza tras editar reglas (S21), un ACK de otro panel no libera el mío (S12, con L-R), repack con `movedCount=0` refresca (S09).
- **Depende de:** contrato S12 (L-R) y censo de persistencia (dueño, C-3).

**L-H1 · assets, build y higiene** (no toca ningún `.c` de las demás)
- **Ficheros exclusivos:** `data/` (censo y borrado de junk CONFIRMADO), `include.lst`, `scripts/3_Game/LFPG_Migrators.c`, ficheros nuevos de CI/gates fuera de `scripts/`, y el movimiento de `reviews/` fuera del root empaquetable si procede.
- **Items:** censo de `data/` con la máquina del packer (mi Glob no ve los binarios → ver G): §5.2 junk (0 refs de `kitboxtexture`/`Furnace_mono` en `config.cpp` ya verificado **[yo]**; confirmar existencia física antes de borrar) · §5.3 duplicados SHA256 · §5.4-parcial: **resolver la contradicción de `include.lst`** (whitelist sin `.p3d`/`.cpp`/`.cfg` **[yo]**, pero el PBO de 99,6 MB carga modelos — documentar la semántica real del packer) · SEC18 (Migrators: retirar la clase huérfana o dejarla como shim documentado — hoy son 90 líneas con los Info ya inline por l8 **[yo]**) · §5.5 gates de build (tras T0c: el gate `*TEST*` se escribe con la whitelist de C-2/C-3).
- **Gate:** build del PBO con gates activos + diff de contenido del PBO antes/después (mismos `.p3d`, mismo arranque) + linter.
- **Depende de:** decisión T0c (para el gate `*TEST*`).

### Oleada 3 — 2 lanes (serializada sobre NM) + cierre

**L-NM2 · NetworkManager: scheduler, ticks, sorter-tick, baterías**
- **Ficheros exclusivos:** `scripts/5_Mission/LFPG_NetworkManagerImpl.c` (tras L-NM1; mismo fichero, oleadas distintas)
- **Items:** U1 (flag de celdas por turno) · U3 (**leak LIFE-008**: los 5 `CallLater(SYSTEM)` pasan a fases del scheduler con cancelación en `OnMissionFinish`) · U5 (censo único por turno + early-out en `TickWaterPumps` = H5) · H6 (map celda→índice) · H4-en-sitio: S05 (copia completa antes del presupuesto, `:6077`), S06 (reanudación ajena a topología, `:6091` — con la semántica de L-E), S24 (resolución duplicada de salidas, `:6066`/`:6174`) · G06 (CutAll no tumba baterías ajenas, `:5088` — con la interfaz de L-B) · D13-parte-planner (`:6888`).
- **NO entra:** partir el fichero (S1 → F).
- **Gate:** linter · in-game: corte local no apaga la red de baterías del vecino (G06), pumps sin trabajo cuando `t1+t2==0` (H5), sorter con cargo grande sin pico (S05), reanudación respeta una nueva salida prioritaria (S06), cero `CallLater(SYSTEM)` vivos tras `OnMissionFinish` (U3, comprobable por lectura del stop).
- **Depende de:** L-NM1 (mismo fichero), L-B (G06), L-E (semántica S06).

**L-H2 · guards de compilación** (opcional, mecánico, el último)
- **Ficheros exclusivos:** los 51 ficheros de `4_World` sin guard (acciones, `LFPG_ElecGraph.c`, `LFPG_NetworkManager.c`, `LFPG_RPCServerHandler.c`, `LFPG_RPCGuard.c`…), uno a uno.
- **Items:** §5.4-guards.
- **Gate:** linter delta 0 · diff solo de directivas · PBO cliente igual o menor.
- **Depende de:** TODAS las lanes de código (va el último para no pisar a nadie). Si el dueño no valora la superficie de compilación cliente, se va a F sin coste.

---

## E. ORDEN Y PARALELISMO

```
Oleada 0 (dueño, sin código):  censo persistencia _TEST · freeze BTC · S13 · D20 · G16 · S1 (NO recomendado)
        │
Oleada 1 (6 a la vez):         L-A · L-B · L-C · L-D · L-E · L-G        [god-files congelados]
        │                      gate in-game batch 1 (el de L-A es la matriz kill/restart)
Oleada 2 (4 a la vez):         L-R · L-NM1 · L-F · L-H1                 [contrato S12 pre-acordado]
        │                      gate in-game batch 2
Oleada 3 (2):                  L-NM2 · L-H2(opcional) → cierre          [NM serializado]
                               gate in-game final + auditor R9 de L-A si tocó dinero en oleada 2-3
```

- **Máximo de lanes simultáneas sin conflicto: 6** (oleada 1). La partición es por fichero
  exclusivo; las únicas dependencias reales son: L-A→L-R (FileUtil/Settings), L-E→L-R (parser),
  L-B→L-NM1 (interfaz grafo), L-NM1→L-NM2 (mismo fichero), L-E→L-NM2 (semántica S06),
  L-R↔L-F (contrato S12, se resuelve pre-acordando el payload, no serializando),
  T0c→L-H1 (gate `*TEST*`), todas→L-H2.
- **Por qué este orden y no el del brief (auditoría → `_TEST` → repartir):** el cruce de la
  auditoría YA está hecho (sección B) y la decisión `_TEST` ya no bloquea lanes (C-3 la reduce a
  un censo). Lo que ordena el plan es la contención real: los dos god-files. Congelarlos en la
  oleada 1 es lo que permite 6 lanes a la vez; abrirlos antes convertiría cada merge en una
  revisión de 7.800 líneas.
- **El dinero va primero de verdad:** L-A arranca en la oleada 1 (jugadores reales, AGENTS.md
  §4), no espera a nada. Los 7 CONFLICTO son la deuda más cara del mod.
- **Riesgo residual de paralelismo:** L-C y L-D comparten el contrato `BeginFrame`/productores
  (R09/R27) — acotado: L-C no cambia el contrato; si lo necesita, R27 se mueve a oleada 2.
  L-G y L-NM2 comparten D13 — dividida por fichero con la mitad planner documentada en el brief
  de L-G.

---

## F. LO QUE NO HARIA

1. **S1: partir `LFPG_NetworkManagerImpl.c` como épica.** 7.810 líneas, ~20 responsabilidades, y
   creció 821 líneas hoy por fusiones verdes. Un split big-bang serializa todo el plan, exige
   re-verificar medio mod in-game y su beneficio es de mantenibilidad, no de jugador.
   Alternativa: «split on touch» — extraer un dominio solo cuando una ficha futura lo exija.
2. **R23: cambiar la representación de segmentos (una instancia por subsegmento).** AMPLIO,
   toca construcción/bounds/oclusión/dibujo, sin daño medido. P3 hasta que un perfil lo pida.
3. **S3 completo: borrar `Call*` y migrar las 3 clases legado.** `LFPG_Generator` es clase de
   producción (`config.cpp:249`, `scope = 2` **[yo]**): cambiar su herencia es riesgo de
   persistencia con jugadores. La reflexión es el fallback real para vanilla/terceros. Hacer
   solo D10 (4 fast-path) y D19 (caché del generador).
4. **S6-hooks: borrar los hooks vacíos.** Son seams documentados (`lfpg_devicebase.c:575-587`,
   `LFPG_WireOwnerBase.c:293-296` **[yo]**, p. ej. «F6 B1: re-registration seam»). Borrarlos
   rompe overrides existentes a cambio de nada.
5. **S7: partir `LFPG_Defines.c` por dominio.** Cosmético. Lo único accionable (11 SubIds V3
   muertos) ya está en L-F.
6. **U2 (fusionar session registries), U4 (cola diferida única), la parte de NearbyCache de U6.**
   Refactors sin ficha de daño que los exija; D09, SEC08-residual, R16 y R17 cubren los daños
   concretos. Reevaluar solo si esos fixes los requieren.
7. **§5.1: techo de 10 MB por `.p3d` / rehacer gates+lids (~230 MB).** Es trabajo de ARTE con
   impacto visual para los jugadores, no una lane de script. Lo que sí entra (L-H1): censo,
   duplicados y junk confirmado.
8. **Reindentar los ficheros mixtos tabs/espacios (4 de los 11 MENOR).** Infla diffs, beneficio
   cero, y AGENTS.md §3 prohíbe reformatear código ajeno. Constancia y a otra cosa.
9. **SEC15 completo (aplicar `RPCGuard` a las 8 familias).** El propio triaje lo baja a P3 y
   avisa de que `Authorize` indiscriminado sería incorrecto (exige ID LFPG + distancia de
   interacción). Solo documentar la cobertura real (L-R).
10. **S18 sin sonda.** No tocar `GetItemSlotDimensions` hasta que la prueba in-game diga si el
    motor indexa arrays por ruta; si funciona, la ficha muere sola.
11. **G22 completo (recuperación del fallback de grafo).** Condicional sin activación demostrada
    (la misión normal sí obtiene el grafo real, `LFPG_MissionInit.c:75` según TRIAJE). Solo el
    rate-limit del log, dentro de L-NM1.
12. **T6 / censo de clases muertas por huella.** El dueño ya decidió (2026-09-07): manda
    mantenibilidad, no arena. SEC18 queda cubierto por L-H1 sin censo general.
13. **Cualquier rename/borrado de classname de `CfgVehicles` sin censo de persistencia previo**
    (incluye `LFPG_Sorter_TEST`, `LF_TestLamp`). Es la trampa documentada de AGENTS.md §4.

---

## G. LO_NO_VERIFICADO

1. **No ejecuté ni git ni el linter**: los hooks del entorno bloquearon el shell en esta sesión.
   `main = e5b5303`, los 19 commits y el linter en verde los tomo del brief §4; el HANDOFF
   (snapshot anterior) dice `adfd29c` — desfase de snapshot, no contradicción.
2. **`data/` no es visible para mí**: Glob no encuentra `.p3d`, `.ogg` ni `.png` en este entorno
   (subst `P:` + OneDrive). Los hallazgos §5.1-§5.3 de la auditoría (tamaños, junk, duplicados
   SHA256) los doy por plausibles pero **no los verifiqué**; por eso L-H1 empieza por censo y
   solo borra lo confirmado. Sí verifiqué en `config.cpp`: 0 refs de `kitboxtexture`/
   `Furnace_mono`, `units[]` con `LF_TestLamp`/`Heavy` y sin `Sorter_TEST`, `include.lst` sin
   `.p3d`/`.cpp`/`.cfg`.
3. **Las líneas citadas de la auditoría son de `a187898` (pre-fusiones).** Medí por última línea
   el crecimiento actual: NM 6.989→7.810, CableRenderer 3.836→4.350, ElecGraphImpl 3.592→4.037,
   RPCServerHandlerImpl 2.655→~3.088, BTCHelper 2.632→2.850. Toda cita de la auditoría hay que
   localizarla por símbolo, no por número.
4. **No reabrí en el árbol cada `path:line` de las 101 fichas**: las 5 lanes de triaje las
   verificaron sobre `d59cad8` y yo leí sus evidencias completas. Abrí en el árbol los anclajes
   que mueven el plan: `config.cpp:40-110`/`:180-270`/`:1030-1095`, `LFPG_IDevice.c:425-520`,
   `LFPG_ControlSessionRegistry.c:124-200`, `LFPG_MissionInit.c:130-260`,
   `LFPG_Defines.c:269-345`, `LFPG_Migrators.c` (entero), `LFPG_WireOwnerBase.c:285-305`,
   `lfpg_devicebase.c:569-620`, `LFPG_SorterView_TEST.c:28-45` + gates diag, `include.lst`.
5. **SEC08-residual**: el lease de 10 s está en main (`:136`, lo abrí); que el tick siga sin
   comprobar distancia lo tomo del triaje + dictamen l7, no lo re-leí en el cuerpo del tick.
6. **El censo de persistencia de `LFPG_Sorter_TEST`** (¿hay alguno colocado en el servidor
   real?) solo puede hacerlo el dueño con los ficheros del servidor. La opción C-3 depende de él.
7. **La semántica real del packer sobre `include.lst`** (¿whitelist inclusiva?): contradictorio
   con que el PBO de 99,6 MB carga modelos. Queda como tarea explícita de L-H1.
8. **Leí los 8 DICTAMEN-GROK y el INFORME-ASTRA de l7 completos; NO leí los 5 INFORME-ASTRA de
   la oleada 1** (los MEDIO/MENOR salen de los dictámenes, que es lo que el brief manda cruzar).
9. **No abrí ningún `PLAN.md` de `<RUN>\lanes\`** (ceguera respetada); si alguno existe, no lo
   he visto.
10. **Los conteos de B** (17 cubiertos / 2 muertos / 4 no-trabajo / 23 nuevos) son míos, hechos
    hallazgo a hallazgo; una segunda lectura podría mover 2-3 filas entre «cubierto parcial» y
    «nuevo» sin cambiar la síntesis.
```

---

Notas de cierre para quien lo recoja: el fichero está pensado para `reviews/2026-09-08-council-plan-definitivo/lanes/kimik3/PLAN.md`. Si quieres que lo escriba en disco, cambia a Agent mode y lo vuelco sin rehacer el análisis.