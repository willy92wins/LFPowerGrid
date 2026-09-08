# PLAN — lane `fable` · council plan definitivo LFPowerGrid · 2026-09-08

Lane ciega. Fuentes abiertas por mi: `BRIEF.md`, `MANIFEST.md`, `fuentes/AUDITORIA-2026-09-07.md`
(sha256 `84d1392bf4150a3d…` recalculado, 136 lineas), `fuentes/HANDOFF.md:1-105`, los cinco
`TRIAJE-*.md`, los ocho `DICTAMEN-GROK.md`, `l7-debt-v3/INFORME-ASTRA.md`, `PLAN-CONJUNTO.md`,
`AGENTS.md`, y el arbol `main @ e5b5303` (arbol limpio salvo `reviews/2026-09-08-council-plan-definitivo/`
sin trackear). No abri ningun `PLAN.md` de `lanes/`: solo liste los nombres de directorio
(`astra fable gemini glm grok kimik3`). No toque codigo, no corri el linter (no cambie nada), no
arranque el juego.

Abreviaturas de ruta usadas en todo el fichero (relativas a `P:\LFPowerGrid\`):
`NM` = `scripts/5_Mission/LFPG_NetworkManagerImpl.c` · `RPCS` = `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c` ·
`EG` = `scripts/5_Mission/LFPG_ElecGraphImpl.c` · `CR` = `scripts/4_World/LFPG_CableRenderer.c` ·
`DEF` = `scripts/3_Game/LFPG_Defines.c` · `MI` = `scripts/5_Mission/LFPG_MissionInit.c` ·
`SL` = `scripts/5_Mission/LFPG_SorterLogic.c` · `TRI-{dis,gra,red,ren,sor}` = los cinco
`reviews/2026-09-08-triaje-fichas-p2-p3/TRIAJE-*.md` · `DIC-lN` = `DICTAMEN-GROK.md` de la lane N ·
`AUD` = la auditoria. Las lineas de `AUD` son de `a187898` y en `NM` van desfasadas hasta +821: toda
cita mia sobre `NM` la relocalice por nombre de funcion en `e5b5303`.

---

## A. CRITICA DEL ENCUADRE

**Conclusion: el encargo esta bien planteado en su pregunta (cuanto de la auditoria es nuevo) y mal
en su decision estrella (el fork `_TEST` ya no existe como fork). Cuatro premisas caen, dos se
sostienen.**

1. **La decision "¿sale `_TEST` del release?" es inaplicable: no hay fork.** La auditoria (fecha
   2026-09-07, rama `fix/t6-dismantle-guard @ a187898`, `AUD:3`) es anterior a T0b (`9fdafa1`,
   "jubilar la INTERFAZ V3"). Hoy en `scripts/4_World/` solo quedan `LFPG_Sorter.c` y
   `LFPG_ActionSyncSorter.c` del sorter (ls); las cinco clases de UI y sus tres layouts V3 no estan
   (`gui/layouts/` = 5 layouts sin sorter; `gui/layouts/test/` = 4). La entidad de produccion
   engancha la accion V4: `scripts/4_World/LFPG_Sorter.c:111` `AddAction(LFPG_ActionOpenSorterPanel_TEST)`.
   `scripts/4_World/test/LFPG_Sorter_TEST.c:15-17` es una subclase vacia con cabecera "Keep classnames
   stable for existing worlds" (`:2-4`). El 3/10 de `AUD:128` ("fork de 4.600 ln… divergente") describe
   un estado que ya no existe. Si `_TEST` "saliera", el sorter se quedaria sin panel. Lo que queda de
   S2 es residuo, no decision (seccion C).

2. **Las cifras de la auditoria estan viejas para el fichero que mas importa.** `NM` mide **7.810**
   lineas (`wc -l`), no 6.989 (`AUD:11`); el arbol tiene 149 `.c` / 78.131 lineas (`find … | wc`), no
   152 / 72.249 (`AUD:10`). El brief ya marcaba la cifra `[SIN CENSO PROPIO]` (`MANIFEST.md`, tabla
   universo). Consecuencia practica: ninguna `:linea` de `AUD` sobre `NM` vale sin relocalizar
   (T2 metio +1.162 en `5_Mission`, brief §4).

3. **El cruce no es auditoria ↔ fichas; es auditoria ↔ fichas ↔ lo fusionado DESPUES del triaje.**
   Las cinco lanes de triaje trabajaron sobre `d59cad8` (`TRI-dis:26`, `TRI-gra:26`, `TRI-red:22`,
   `TRI-ren:33`, `TRI-sor:30`). Despues se fusionaron `lane/l7-debt-v3` (`64063c2`) y
   `lane/l8-kimi-followup` (`c8546b0`). Con eso, tres fichas VIVAS encogieron o murieron a medias y
   nadie lo ha anotado:
   - **SEC05, lado manager: MUERTA.** `NM:741-743` ya usa `LFPG_Util.RateLimitedWarn(ident,
     "global_sliding_window", …)` (V3-13, `INFORME-ASTRA l7:148`). Sobrevive solo el lado handler
     (`RPCS:223` "Too fast!" y `:1712` segun `TRI-red:50`).
   - **SEC14, mitad `LFPG_Util`: MUERTA.** `scripts/3_Game/LFPG_Util.c:44` `PurgeStaleWarnRateLimits`
     existe y lo llama `NM:811`. Queda la retencion `m_ByUID` del registro BTC, que el propio triaje
     dice que es anti-replay y no se borra por simetria (`TRI-red:217`).
   - **SEC13: REDUCIDA.** `scripts/5_Mission/LFPG_BTCSessionRegistry.c:297` y `:333` acotan el salto
     con `LFPG_BTC_MAX_FORWARD_GAP` (CX-E H-05, `DIC-l7:88`). Ya no es "un salto agota el UID";
     es "2.000 reservas de 1e6 lo agotan".
   Y de la auditoria, **§6.2 esta HECHO** (l5, `f247f5a`: `test/LFPG_SorterView_TEST.c:1652-1654`
   gatea `RunS1Probe` por `LFPG_PERFDIAG_ENABLED`; `:83-110` es el scaler aislado; MCP bajo
   `#ifdef DIAG_DEVELOPER` en `:177,:438,:458,:470,:1624,:1746,:1988`) y **S6-migradores esta a
   medias** (l8 M-03 inlineo los dos no-op: `scripts/3_Game/LFPG_Migrators.c` tiene 89 lineas,
   `f3fbf83` −26 en ese fichero). El "92 VIVA" es un techo medido en `d59cad8`, no el numero de hoy.

4. **"Disjuntas por FICHERO" es la regla correcta, pero aplicada a `NM` deja ~25 items en una sola
   lane y mata el paralelismo que el brief pide.** El propio orquestador ya la rompio con exito en la
   oleada 1: `lane/l3-t4-render` toco `NM` (13 lineas, hunk 6193-6200, `DIC-l3:5`) y
   `lane/l4-t2-autoridad` toco `NM` (48 lineas, hunks ~2147-3047, `DIC-l4:50`). Lo medi:
   `git merge-tree --write-tree lane/l3-t4-render lane/l4-t2-autoridad` produce el arbol `e5c364e0`
   sin ninguna linea de conflicto (rc=0). Git fusiona por hunks, no por ficheros. Mantengo la regla
   del brief como default y declaro UNA excepcion medida: `NM` partido en zonas separadas por mas de
   1.000 lineas, con `git merge-tree --write-tree main lane/X` en seco como gate antes de cada merge.
   Si el dueño no acepta la excepcion, el plan sigue valiendo: las dos zonas son una sola lane (E).

5. **"~170 items" cuenta el mismo mecanismo varias veces.** H1 = G12+G13; C6 = R05; §6.4 = R30;
   U4 ⊂ D09; H4 = S05+S24+S07; H9 = S08 (hecho); §6.5 = el MEDIO de l3 sobre S08; U1 = H6. La tabla B
   lo desglosa: de 46 hallazgos, 19 ya estan hechos o cubiertos, 5 no piden accion, 7 son codigo nuevo
   barato y 15 son optimizaciones sin perfil o decisiones del dueño. Del lado de los dictamenes, 6 de
   los 11 MENOR son tabs/comentarios. El backlog real de codigo que yo programaria son ~60 items, no
   170; el resto es decision o perfil.

6. **El precedente de `LFPG_Sorter.c` se repite dos veces dentro de la propia auditoria.** S3 propone
   borrar la reflexion `Call*` (`scripts/4_World/LFPG_IDevice.c:433-479`), pero `LFPG_Generator :
   PowerGenerator` (`scripts/4_World/LFPG_TestDevices.c:53`), `LF_TestLamp : Spotlight` (`:930`) y
   `LF_TestLampHeavy` (`:1354`) viven de ese fallback (`LFPG_IDevice.c:646` llama
   `CallFunctionParams(obj, "LFPG_GetWires", …)`; `TRI-dis:126` lo confirma para el generador) y los
   tres estan en `units[]` (`config.cpp:187`): son clases que un mundo puede tener colocadas. Y S6
   propone borrar `LFPG_GetKitClassname` (`scripts/4_World/lfpg_devicebase.c:596`), que D05 acaba de
   usar para hacer recuperable el `LFPG_BatteryAdapter` (`DIC-l1:41`, `LFPG_BatteryAdapter.c:85-87`).
   Dos "sobreingenierias" cuyo borrado rompe funciones enviadas.

7. **La decision que de verdad cambia el tamaño del plan no es `_TEST`: es el journal del dinero.**
   De los 7 CONFLICTO, cuatro piden el mismo diseño (commit atomico ledger+inventario+hive: V3-02,
   H-04, H-05, V3-03 — `INFORME-ASTRA l7:46-60,152-166`), uno es politica de recovery (V3-05, `:70-78`),
   uno es contenido en un fichero (V3-06: verificar target tras `CopyFile` en
   `scripts/3_Game/LFPG_FileUtil.c:282`, `:80-88`) y uno es trivial (err=14: cinco operaciones envian
   saldo 0, `LFPG_BTCHelper.c:1640,2127,2294,2515,2697`, `:184-190`). "Journal si o no" es una
   decision del dueño que mete o saca una lane serial entera con harness (seccion C, al final).

Lo que se sostiene del encuadre: el orden "cruzar → decidir → repartir" es correcto (y lo seguido
aqui), y el linter como gate por `len(errors)` con delta contra 263/0/47 (`AGENTS.md:20-35`) es el
unico gate estatico que existe; lo uso en todas las lanes.

---

## B. SOLAPE AUDITORIA <-> TRABAJO YA TRIADO

Etiquetas en `cubierto_por`: **CUBIERTO** (una ficha, MEDIO o CONFLICTO ya describe el mecanismo),
**HECHO** (ya arreglado en `e5b5303`), **NUEVO-codigo** (barato, entra en una lane de D),
**NUEVO-perfil** (optimizacion sin medida: F), **NUEVO-decision** (del dueño o de proceso: F),
**NUEVO-higiene** (assets/repo), **NO-ACCION** (la auditoria misma lo da por bien) y **REFUTADO**.
"Parcial" = una parte cubierta, el resto con su etiqueta.

| hallazgo | cubierto_por | evidencia (path:line) |
|---|---|---|
| S1 dios `NM` | NUEVO-decision (arquitectura; ninguna ficha pide partirlo). Cifra real 7.810 ln. | `NM` wc=7810; scheduler `NM:618-684`; validacion en 16 fases `NM:3787-4156`; sorter `NM:5918`; simples `NM:6830`; celdas `NM:7027` |
| S2 fork TEST | HECHO en el fondo (T0b `9fdafa1`). Residuo NUEVO-codigo: 5 despachos V3 huerfanos en servidor + `allowUnpowered` + T0c (cola del HANDOFF). | `scripts/4_World/test/` = 6 ficheros (ls); `scripts/4_World/LFPG_Sorter.c:111`; `test/LFPG_Sorter_TEST.c:15-17`; despacho V3 vivo `RPCS:113,124,134,144,154` frente a TEST `:118,129,139,149,159`; `scripts/4_World/LFPG_RPCGuard.c:48-67`; `DEF:294-309,326-336`; `RPCS:2523` (`allowUnpowered`); `HANDOFF.md:94-95` |
| S3 reflexion `Call*` | CUBIERTO por D10 (los 4 wrappers sin fast-path) y D19 (el generador legado NECESITA el fallback). Borrar `Call*` = NO. | `scripts/4_World/LFPG_IDevice.c:433-479` (Call*), `:507,:840,:857,:873` (los 4 de D10), `:646` (`LFPG_GetWires` por reflexion); `LFPG_TestDevices.c:53,930,1354`; `config.cpp:187` (`LFPG_Generator`, `LF_TestLamp`, `LF_TestLampHeavy` en `units[]`); `TRI-dis:60-65` (D10), `:122-128` (D19) |
| S4 ecosistema BTC | CUBIERTO: 7 CONFLICTO + SEC12/13/14 + SEC16. Fusionar registries = U2 (NUEVO-decision); freeze = §6.7. | `INFORME-ASTRA l7:5-24` (tabla), `:30`; `LFPG_BTCHelper.c` wc=2850, `LFPG_BalanceProvider_NativeImpl.c` wc=2167, `LFPG_BTCSessionRegistry.c` wc=407, `LFPG_ControlSessionRegistry.c` wc=466; `TRI-red:108-136,148-156` |
| S5 acciones/chrome ×3 | CUBIERTO-parcial por D18 (switches). El chrome UI paso de ×3 a ×2 con T0b. Resto NUEVO-decision (refactor sin fallo). | `scripts/4_World/LFPG_ActionToggleSwitchRemote.c`, `…SwitchV2.c`, `…SwitchV2Remote.c`, `LFPG_ActionSpeakerOn.c`/`Off.c` (ls); `MI:168-201,206-230`; `LightenARGB` en `scripts/4_World/LFPG_BTCAtmView.c:1043` y `test/LFPG_SorterView_TEST.c:1924`; `IsEscCooldown` en `LFPG_BTCAtmView.c:1181` y `SorterView_TEST.c:1478`; `TRI-dis:115-120` (D18) |
| S6 capas vacias | HECHO-parcial (l8 M-03 inlineo los 2 no-op) + CUBIERTO por SEC18 (clase sin llamadores). Hooks vacios: NUEVO-decision; `LFPG_GetKitClassname` NO se borra (D05 lo usa). | `scripts/3_Game/LFPG_Migrators.c:39,:67` (89 ln); `git show --stat f3fbf83` (−26 en Migrators); `TRI-red:168-176` (SEC18); `scripts/4_World/lfpg_devicebase.c:18,573,596,615`; `scripts/4_World/LFPG_WireOwnerBase.c:291-295`; `DIC-l1:37-45,81` (D05 usa el kit classname) |
| S7 `Defines.c` mezcla todo | NUEVO-decision (no añadir; no partir). | `DEF` wc=813; `enum LFPG_CableState` `DEF:84`; `CAMERA = 4` `DEF:415`; `DEF:18/:220` |
| H1 `BroadcastOwnerWires` doble barrido | CUBIERTO por G12 + G13 (mismo mecanismo, misma funcion). | `TRI-gra:110-116` (G12: `NM:2013,2030,2038,2070,2092,2107,2116`), `:118-124` (G13: `NM:2395,2399,2414,2453,2539,2552,2559,2590`); relocalizado: `NM:1994` `BroadcastOwnerWires`, `:2352` `…Delta`, `:2517` `BroadcastVanillaWires` |
| H2 `CleanDisappearedVanillaDevice` scan por prefijo | NUEVO-perfil (ninguna ficha). Nota: usa `ref` en locales, fuera de la convencion de `AGENTS.md:71`. | `NM:3401` (funcion), `:3476` `rKey.IndexOf(keyPrefix) == 0`, `:3412,:3471` `ref array<string>` locales, llamador `:3674` |
| H3 `CutAllWiresFromDevice` fallback O(V·W) | NUEVO-perfil; gateado por `reverseIndexConsistent`; vecino de G06/G07/G21/V3-07 (indice). | `NM:4733` (funcion), `:4748,:4958,:5050` (`reverseIndexConsistent`); G06 en `NM:5086-5089` (`TRI-gra:52`) |
| H4 tick sorter (6 resoluciones, sqrt, dedup) | CUBIERTO por S24 (6× `ResolveOutputContainer`), S05 (copia completa), S07 (por item). El `sqrt` es NUEVO-codigo trivial. | `TRI-sor:68-74` (S05: `NM:6077,6118,6150`), `:214-220` (S24: `NM:6066,6174`), `:84-90` (S07: `SL:576,409`); `NM:6017` `vector.Distance(sorter.GetPosition(), …)`; `SL:978` `"output_" + portNum.ToString()` |
| H5 `TickWaterPumps` triple barrido | NUEVO-codigo (early-out). U5 lo repite. | `NM:5459` (funcion), fases `:5484,:5547,:5615`; ningun `return` temprano entre 5459-5730 (awk sobre `return;`) |
| H6 celdas O(P·C) + doble rebuild | = U1 → NUEVO-codigo (flag por turno); la parte O(P·C) es NUEVO-perfil. D13 es otro registro (jugadores por controlador), no este. | `NM:7027` `LFPG_RebuildPlayerCells`; llamadas `NM:6971` (sprinklers) y `NM:7231` (deteccion); `grep m_PlayerCellsBuiltThisTurn` = 0 |
| H7 `RebuildFromWires` + sync 3 resoluciones/nodo | CUBIERTO-parcial por G20 (nodos de todo el registro + hidratacion doble). `SyncNodeToEntity` = NUEVO-perfil. | `TRI-gra:161-169` (G20: `EG:224,237,311,318,1259,1912-1913`); `EG:193` `RebuildFromWires`, `:2785` `SyncNodeToEntity`, llamadas `:2708,:3071,:3090`, `:1884` `PostBulkRebuild` |
| H8 `HandleFinishWiring` alloc+ToString+sqrt antes de validar | CUBIERTO por SEC07 (limites tras leer) y SEC17 (doble geometria). Lo que queda es NUEVO-codigo trivial: las strings `:264-267` se construyen sin guard de nivel. El rate-limit ya va antes de leer. | `RPCS:214` (funcion), `:223` rate-limit antes de `ctx.Read`, `:253` `new array<vector>` antes de leer, `:264-267` tres `ToString` + `LFPG_Util.Debug` sin `if (LFPG_LOG_LEVEL >= 2)`; `TRI-red:68-76` (SEC07: `RPCS:268,278,289,304,311`), `:158-166` (SEC17: `RPCS:440,465,484,496`) |
| H9 `RepackCargoInPlace` 9 allocs + O(n²) | HECHO (S08, `cd5d153`). Residuo = MEDIO l3 (tope 64 silencioso) = §6.5. | `SL:1133` `if (totalItems > 64 \|\| gridW > 1024 \|\| gridH > 1024)` → return; `:1230,:1242` `TryPlaceOnGridBudgeted`, `:1351`; `DIC-l3:9-12` |
| C1 sway invalida cache | CUBIERTO-parcial por R10 (la clave incluye el sway y reproyecta). La atenuacion por distancia YA existe (0 a 50 m). Cuantizar `nowMs` = NUEVO-codigo trivial. | `CR:3223` `swayScale = 1.0 - (wireDist * LFPG_SWAY_ATTEN_INV)`, `:3227-3228` `Math.Sin`, `:3236` compara `screenCacheSwayY/X`; `DEF:38` `LFPG_SWAY_ATTEN_INV = 0.02` (1/50 m); `TRI-ren:112-118` (R10) |
| C2 tres pasadas O(n)/frame | NUEVO-perfil (vecinas R05/R09). | `CR:2710` `DrawFrame`; `TRI-ren:72-78` (R05), `:104-110` (R09) |
| C3 invariantes por subsegmento | CUBIERTO-parcial por R24 (matematica duplicada); el hoist es NUEVO-perfil. | `CR:3520,:3544` `Math.Sqrt` por subsegmento; `TRI-ren:223-229` (R24: `CR:2159,3981,4212,4221`) |
| C4 clip/fade solo en miss | NO-ACCION (la auditoria lo da por bien). No relocalice `:3399-3402` (G). | `AUD:72` |
| C5 oclusion por jugador 2 `GetScreenPos`/frame | NUEVO-perfil; el gate 3P existe. | `CR:2812` `if (plCamDist > LFPG_PLOCC_3P_MIN_DIST_SQ)` |
| C6 selection sort O(n²) gateado | CUBIERTO por R05 (identico). | `CR:2654` `RebuildDrawOrder`, `:2865` `if (m_DrawOrderDirty)`; `TRI-ren:72-78` |
| C7 oclusion bien presupuestada | NO-ACCION. | `DEF:229-239` (`LFPG_OCC_*`) |
| C8 `CullTick` 1 sqrt/wire | NUEVO-codigo trivial (vecina R08, misma funcion). | `CR:2294` `CullTick`, `:2510` `float distToCenter = Math.Sqrt(distToCenterSq)`; `TRI-ren:96-102` (R08: `CR:2382,2413-2438`) |
| C9 preview solo con sesion | NO-ACCION. No releí `LFPG_WiringClient.c` (G). | `AUD:77` |
| C10 inspector `GetScreenSize`+`SetPos`/frame | NUEVO-codigo trivial (vecinas R13/R28, mismo fichero). | `scripts/4_World/LFPG_DeviceInspector.c:1641` `GetScreenSize(screenW, screenH)`, `:752` `widget.SetPos(x, y)`, `:470` `Tick`; `TRI-ren:138-144` (R13), `:249-255` (R28) |
| C11 laser sin cache entre frames | CUBIERTO-parcial por R18/R19 (mismo `CullTick`); la cache de proyeccion es NUEVO-perfil. | `scripts/4_World/LFPG_LaserBeamRenderer.c:157` `CullTick`, `:188-189` y `:264-265` `GetScreenPos`, `:299` `Math.Sqrt`; `TRI-ren:170-184` |
| C12 joints ×4 `DrawLine` | NUEVO-perfil (saltar decoradores con `lodBlend` bajo). | `scripts/4_World/LFPG_CableHUD.c:336` `DrawJointScreen`, `:349-352` cuatro `m_Canvas.DrawLine` |
| U1 un rebuild de celdas por turno | = H6 → NUEVO-codigo (~10 lineas). | `NM:6971`, `NM:7231` (ver H6) |
| U2 sesiones a 1 s + fusionar gemelos | CUBIERTO-parcial por SEC08 (el tick sin distancia); el acumulador a 1 s y la fusion son NUEVO-decision. | `NM:622` `m_ControlSessions.Tick()` dentro del scheduler de 100 ms (`NM:618`); `scripts/5_Mission/LFPG_ControlSessionRegistry.c:15` (125 s), `:385` `Tick`, `:409-420`; `TRI-red:78-86` (SEC08: `:136,:391,:404,:420`) |
| U3 one-shots al scheduler (leak LIFE-008) | NUEVO-codigo (4 `Remove` en `StopServerScheduler`). Solo el Timer se para; el 5.º one-shot (`RunOrphanSweep`) ya se retira en `MI:70`. | `NM:606-616` (`StopServerScheduler` solo `m_ServerScheduler.Stop()`); one-shots `NM:504` (5 s), `:3747` (500 ms), `:3994` (30 s), `:5089` (1 ms); `NM:1092` `Remove(PostBulkRebuildAndPropagate)` parcial; `MI:55` y `:70`; llamador `MI:63` |
| U4 cola diferida unica con coalescencia | CUBIERTO-parcial por D09 (RemoteController + MemoryCell). Los otros 6 sitios son NUEVO; la cola unica es NUEVO-decision (AMPLIO). | `scripts/4_World/LFPG_MemoryCell.c:184,267`; `LFPG_RemoteController.c:539-540,559-560,708`; `LFPG_ElectronicCounter.c:363`; `LFPG_PushButton.c:139`; `LFPG_SwitchRemote.c:148`; `LFPG_KitBase.c:208`; `LFPG_KitBaseDeployable.c:153`; `LFPG_DoorController.c:652` (grep `CallLater` en 4_World); `TRI-dis:52-58` (D09) |
| U5 censo unico por turno | NUEVO-perfil; solapa H5 (early-out en pumps). No relocalice los 7 conteos `:6841-6848` (G). | `NM:6830` `LFPG_TickSimpleDevices` |
| U6 cliente: 1 maintenance tick + nearby cache + 1 player/frame | NUEVO-refactor de oleada 2 (cruza `CR` + laser + `MI`; no cabe en una lane disjunta hoy). R16/R17 vecinas. Son 5 `GetPlayer`, no 6. | `CR:865,:868,:871,:876` (4 `CallLater` GUI), `LFPG_LaserBeamRenderer.c:68`; `MI:232` `OnUpdate`, `GetPlayer()` en `MI:244,267,290,407,445`; `TRI-ren:154-168` |
| §5.1 techo `.p3d` (gates+lids 230 MB) | NUEVO-decision (trabajo de Blender/py3d, no de Codex). Cifras re-medidas y exactas. | `find data -name "*.p3d" -size +9M`: `memory_cell.p3d` 43.602.321; `gate_xor`/`gate_and` 39.729.763; `gate_or` 39.729.750; `electric_stove` 22.333.458; `lid_xor/lid_mem/lid_and` 15.487.990; `lid_or` 15.487.982; `lid` 15.486.238; `rf_broadcaster` 15.377.641; `lf_searchlight` 11.317.925; `substation_transformer` 9.730.213; `sprinkler` 9.534.199; `du -sh data` = 420M |
| §5.2 junk (png/paa, stereo_backup, Furnace_mono, water.ogg) | REFUTADO-parcial: el `.paa` SI tiene consumidor (lo referencia el `.p3d` del kit); el `.png` y los 3 `.stereo_backup` no se empaquetan (no estan en `include.lst`) → NUEVO-higiene de repo, no de PBO. `water.ogg` si se empaqueta (`*.ogg`). | `grep -rl kitboxtexture config.cpp model.cfg data` → solo `data/kits/lf_kit_box.p3d`; `data/kits/kitboxtexture.png` 2.340.485 B, `.paa` 1.219.223 B; `find data -name "*.stereo_backup"` = 3; `data/waterpump/water.ogg` 5.527.214 B; `data/furnace/Furnace_mono.ogg` 3.650 B (existe; referencia no buscada, G); `include.lst` = `*.c;*.asi;*.anm;*.paa;*.rvmat;*.layout;*.ogg;*.ptc;*.csv` |
| §5.3 duplicados SHA256 | NUEVO-higiene (repuntar `.rvmat`; verificacion visual). Re-hasheado 1 par. | sha256 `bc816460888382cf…` = `data/switch_v1/data/switch_v1_co.paa` = `data/switch_v1_remote/data/switch_v1_remote_co.paa`; `find data -name "*.rvmat" \| wc -l` = 104 |
| §5.4 compile (`include.lst`, TEST en server, `units[]`) | REFUTADO en lo grave: `include.lst` no es una whitelist inclusiva, porque el PBO desplegado carga con `config.cpp` dentro. TEST en server y `LF_TestLamp/Heavy` en `units[]` = NUEVO-decision (persistencia). | `include.lst` (57 B, sin `*.cpp`/`*.p3d`); `reviews/2026-09-08-arranque-verificacion/server-script.log:9` (`Module: Mission; loaded 249x files; 590x classes` con define `LFPowerGrid`), `:30` `MissionServer OnInit (v1.2.4)`; `HANDOFF.md:58-68`; `config.cpp:187` (`LF_TestLamp`, `LF_TestLampHeavy` en `units[]`), `:256,:263` |
| §5.5 gates de build | NUEVO-decision (no hay script de build en el repo). | raiz = `$PBOPREFIX$ AGENTS.md CLAUDE.md config.cpp data gui harness include.lst model.cfg reviews scripts stringtable.csv` (ls); `du -sh reviews` = 25M |
| §6.1 128 vs 64 intencional | NO-ACCION (la auditoria lo cierra). | `DEF:18` `LFPG_MAX_WIRES_PER_DEVICE = 64`, `DEF:215-220` comentario y `LFPG_MAX_WIRES_PER_OWNER_CLIENT = 128` |
| §6.2 `S1_PROBE` gateado + `UIScaler` en V4 | HECHO (l5, `f247f5a`). | `test/LFPG_SorterView_TEST.c:1652-1654` (`if (LFPG_PERFDIAG_ENABLED) RunS1Probe()`), `:1663`; `:83` `LFPG_SorterScaleContext_TEST extends LFPG_UIScaler`, `:93-110`, `:1384` `CapturePanel`, `:1584` `ComputeScale`; `DIC-l5:37-41` (tabla) |
| §6.3 `OnKeyPress` sin `super` con UI activa | NUEVO-decision (UX/compat con otros mods; el comentario lo declara a proposito). | `MI:168-201`: `return` en `:174` (CCTV), `:187` (sorter), `:197` (ATM) antes de `super.OnKeyPress` `:200`; comentario `:177-180`; `MI:206-230` |
| §6.4 `SetLV(0)`/`SetTextLV(0)` globales | CUBIERTO por R30 (identico). | `MI:140-141`; `TRI-ren:265-271` |
| §6.5 avisar en UI cuando `n>64`; early-out de area | CUBIERTO por el MEDIO de l3 sobre S08. El aviso en UI exige contrato de ACK (F). El presupuesto de celdas ya existe. | `SL:1133` (`return` silencioso), `:1351` `TryPlaceOnGridBudgeted(…, inout int checksRemaining)`; `DIC-l3:9-12` |
| §6.6 documentar el split World/Mission de `ElecGraph` | NUEVO-codigo (solo comentario, ~10 lineas). La cabecera actual no lo menciona. | `scripts/4_World/LFPG_ElecGraph.c:1-30` (cabecera: modelo, allocation, subsistemas; nada del split ni del incidente de julio) |
| §6.7 comentarios `feature-frozen` | NUEVO-decision del dueño (sin consumidor ejecutable). | — |

**Recuento (46 filas):** HECHO 3 (S2 fondo, H9, §6.2) · CUBIERTO 16 (S3, S4, S5, S6, H1, H4, H7, H8,
C1, C3, C6, C11, U2, U4, §6.4, §6.5; varios "parcial") · NUEVO-codigo 7 (H5, H6, U1, U3, C8, C10,
§6.6; mas cuatro añadidos triviales dentro de H4/H8/C1/S2) · NUEVO-perfil 7 (H2, H3, C2, C5, C12,
U5, U6) · NUEVO-higiene 2 (§5.2, §5.3) · NUEVO-decision 6 (S1, S7, §5.1, §5.5, §6.3, §6.7) ·
NO-ACCION/REFUTADO 5 (C4, C7, C9, §6.1, §5.4). **24 de 46 no necesitan lane nueva; de los 22
restantes solo 7 (+4 triviales) son codigo que programaria hoy.** La interpretacion del orquestador
("una fraccion grande ya cubierta") se confirma: 41 % cubierto o hecho, y el trabajo nuevo real de la
auditoria cabe en las lanes que ya existen por fichero, sin abrir ninguna lane "de auditoria".

---

## C. LA DECISION DE PRODUCTO

**Veredicto: `_TEST` no puede "salir del release" porque es la unica UI del sorter desde T0b; la
decision real son dos, y ninguna mueve fichas: (1) conservar los dos classnames `_TEST` de `config.cpp`
(persistencia, coste 0) y (2) NO renombrar ahora (T0c al final o nunca). Recomiendo conservar sin
renombrar y cerrar el unico residuo funcional (5 despachos V3 huerfanos + `allowUnpowered`) en la
lane serial de cierre.**

Universo `_TEST` medido: fichas cuyo fichero principal esta en `scripts/4_World/test/`: S10 (DUDOSA,
`TRI-sor:12`), S11, S20, S21, S23 (VIVA, `:13,:22,:23,:25`), S22 (MUERTA, `:24`) → **4 VIVA + 1
DUDOSA**. Dictamenes en `test/`: MEDIO l5 (`SorterController_TEST.c:1709/:1768`, `DIC-l5:11-19`),
MENOR l5 (`SorterView_TEST.c:1384`, `:21-29`), MENOR l6 (`SorterView_TEST.c:34`, `DIC-l6:21-25`) →
**3**. Auditoria: S2 (HECHO en el fondo) y §6.2 (HECHO). Total: **8 items vivos** viven en `_TEST`.

| opcion | fichas que desaparecen/aparecen | coste | veredicto |
|---|---|---|---|
| (a) sacar `_TEST` del PBO | −0 fichas; −1 UI (el sorter se queda sin panel: `LFPG_Sorter.c:111` engancha `LFPG_ActionOpenSorterPanel_TEST`, y no hay otra) | inviable | NO |
| (b) conservar y renombrar ya (T0c primero) | 0 fichas cambian; las 8 citas `_TEST` quedan desfasadas; bloquea 3 lanes (L6 sorter-UI, L4 handler por SubIds, L12 cliente por `MI`) hasta fusionar; toca ≥12 ficheros (6 `test/*.c`, 4 layouts, `MI`, `LFPG_Actions.c`, `LFPG_ActionSyncSorter.c`, `LFPG_ActionRegistration.c`, `LFPG_RPCClientHandler.c`, `RPCS`, `LFPG_RPCGuard.c`, `DEF`) | alto, valor cosmetico | NO ahora |
| (c) conservar sin renombrar; retirar solo el residuo | 0 fichas; +1 item (~40 lineas) en la lane de cierre: retirar despachos `RPCS:113,124,134,144,154` y politicas `LFPG_RPCGuard.c:48,52,56,60,64`, reservar los numeros 19-23/29-33 en `DEF:294-308` como hizo V3-12 con 13/14 (`DEF:288-289`), y quitar `allowUnpowered` (`RPCS:2523`: hoy la config V4 se sirve sin energia mientras la accion exige `powered+linked`, `DIC-l5:49`) | ~40 lineas, 0 riesgo de persistencia | **SI** |
| (b') T0c despues de todo, solo clases de UI | 0 fichas; no bloquea nada; los dos classnames de entidad no se tocan | 1 lane serial | opcional del dueño |

Riesgo de persistencia que hay que decir en voz alta: `LFPG_Sorter_TEST_Kit` y `LFPG_Sorter_TEST`
estan en `config.cpp:1080-1090` con `scope = 2` y `displayName "[V4 TEST] …"`: son spawneables y
pueden estar colocados en el servidor privado (numero desconocido, G). Ni (b) ni (b') ni (c) los
renombran ni borran. Cualquier plan que los toque rompe esos mundos (`AGENTS.md:83-90`).

**Si la decision relevante es otra: lo es, y es el journal del dinero.** Con "no journal, se acepta
la ventana de crash entre efecto y save", 5 de los 7 CONFLICTO (V3-02, V3-03, V3-05, H-04, H-05)
salen del plan a F como riesgo aceptado con cita, y quedan err=14 y V3-06 en lanes (D). Con "journal",
entra una lane serial nueva sobre `LFPG_BTCHelper.c` + `LFPG_BalanceProvider_NativeImpl.c` +
`LFPG_FileUtil.c` con el harness de inyeccion (T5) como gate, y es la pieza mas grande de todo el
backlog. Tamaño del plan: **−5 items o +1 lane serial de varios dias**. Esa es la decision que
cambia el plan; `_TEST` no cambia nada.

---

## D. EL PLAN: LANES

Reglas comunes a todas las lanes (son el contrato, no adorno):
- **Un fichero = una lane.** Ficheros compartidos VETADOS en la oleada 1: `DEF`, `scripts/3_Game/LFPG_Util.c`,
  `config.cpp`, `stringtable.csv`, `scripts/3_Game/LFPG_WireHelper.c`, `scripts/4_World/LFPG_NetworkManager.c`,
  `scripts/4_World/LFPG_ElecGraph.c` (salvo la cabecera de L1). Una constante nueva se declara local
  en el fichero de la lane; la lane de cierre L15 la mueve a `DEF`.
- **Gate estatico de toda lane:** `python C:/Users/guill/DayZ-Modding-Knowledge-Pack/tools/dayz-script-validator/scripts/script_validator.py .`
  con `len(errors) == 0` y **delta** contra la base 263 ficheros / 0 errores / 47 warnings
  (`AGENTS.md:34-35`); si toca layouts o `FindAnyWidget`, ademas `ui_reconcile.py . --strict` = 0 FAIL
  (`AGENTS.md:37-46`). Convenciones de `AGENTS.md:59-77` en cada linea añadida.
- **Gate de merge:** `git merge-tree --write-tree main lane/<id>` en seco sin conflictos, y el linter
  sobre el arbol fusionado antes del siguiente merge.
- **Gate in-game por lote**, no por lane: un arranque diag por oleada (como `HANDOFF.md:58-77`); lo
  que exija fixture (sorter alimentado+enlazado), dos clientes o accion continua se declara
  PENDIENTE, no verificado (`AGENTS.md:108-109`). `action_use` no completa acciones continuas (brief §4).
- **No commitear** (`AGENTS.md:96`); revisor de otra familia por lane (regla G7 del dueño).

| id | objetivo | ficheros exclusivos | items que cierra | gate de aceptacion | depende de |
|---|---|---|---|---|---|
| **L1 grafo** | corregir contrato de limites y contadores del solver | `EG` (+ solo la cabecera de comentario de `scripts/4_World/LFPG_ElecGraph.c` para §6.6) | G19 (TRIVIAL), G09, G11, MEDIO l2 (G01 cobra presupuesto, `EG:3789`), §6.6 | linter delta 0; lectura: `AllocateOutput` corta el bucle al poner `ptHasDown=true` (`EG:3608-3616`); precheck global cuenta extremos nuevos (`EG:443,1687-1698`: 2.047+2 nuevos → rechaza; 2.048 entre existentes → admite); `m_LastProcessMs` se actualiza tambien en la rama de cola vacia (`EG:2074,2766`); la pasada de comparacion de G01 no incrementa `m_EdgesVisitedThisEpoch`; arranque diag sin `Can't compile` | nada |
| **L2 nm-sync** | integridad de cuota, seguimiento y FIFO; leak de one-shots | `NM` **zona A** (lineas 606-616, 1327-1485, 1994-2900, 3315-3372, 4267-4450) | G07 (TRIVIAL), G23 (TRIVIAL), G03, G12+G13 (=H1), G21, U3 | linter; lectura: `RecountAllPlayerWires` (`NM:1505` segun `TRI-gra:66`) llamado al final del callback diferido tras las podas; `RemoveOrdered` en `LFPG_StartNextFullSync` (`NM:2716-2717` segun `TRI-gra:193`); `DeviceHasAnyWires` consulta `m_VanillaWires` (`NM:3315-3324`, `TRI-gra:32`); un solo `GetPlayers` por broadcast y serializacion tras filtrar receptores en las tres funciones (`NM:1994,2352,2517`); `StopServerScheduler` (`NM:606`) hace `Remove` de los 4 one-shots (`NM:504,3747,3994,5089`). In-game PENDIENTE: cuota decrementa tras poda (2 clientes) | nada |
| **L3 nm-tick** | sorter tick/manual, bombas y celdas | `NM` **zona B** (lineas 5459-5730, 5918-6600, 6830-7300). *Por la regla del brief, L2+L3 son UNA lane; dos solo bajo la excepcion de zonas (A.4)* | H5 (early-out), S05, S06, S15-lado manual (`NM:6431`), U1/H6 (flag), H4-sqrt (`NM:6017`) | linter; lectura: `LFPG_TickWaterPumps` retorna si no hay sprinklers ni bomba alimentada; `m_PlayerCellsBuiltThisTurn` se resetea por turno y se consulta en `NM:6971` y `:7231`; el cursor de reanudacion guarda la generacion de cables del sorter (`WireOwnerBase.c:161` segun `TRI-sor:82`) y se invalida si cambia; la copia de cargo respeta el presupuesto antes de enumerar (`NM:6077→6118`); el origen manual comprueba `LFPG_SORTER_LINK_RADIUS` como el tick (`NM:6016`) | nada; **merge despues de L2** (mismo fichero) |
| **L4 rpc-handler** | cerrar lo funcional del handler sin cambiar contratos | `RPCS` | S03 (TRIVIAL), S14, V3-11 (`RPCS:2908`, `INFORME-ASTRA l7:132`), S02-consumidor (`RPCS:2942`), SEC06, SEC07-parte (sanear `dstDeviceId` antes de `ValidateWaypoints` `:311`; limitar antes de leer donde la API lo permita), H8-trivial (strings `:264-267` tras guard de nivel), MEDIO l4 #2 (fail-closed por owner, no global, `:777/:790`), MENOR l4 (`:398` log), MENOR l7 (`:1241` respuesta vacia) | linter; lectura por item: Save y Preview usan el mismo tope `LFPG_SORT_MAX_JSON_BYTES` (`RPCS:2668` vs `:2899`, `SorterData.c:45`); `HandleSorterPreviewRequest` llama `CanTakeFromContainer` (`SL:56`) antes de enumerar (`RPCS:2956-2976`); `canProceed` exige energia y no-ruined (`:2908`); un registro sucio deniega solo ese owner (`:777,:790`). In-game PENDIENTE: RPC 67 sobre sorter sin energia responde vacio; preview sobre contenedor con CodeLock devuelve vacio | nada |
| **L5 cable-renderer** | defectos funcionales del renderer + dos triviales de la auditoria | `CR` | R01 (TRIVIAL), R03 (TRIVIAL), R08, R09, R20 (guard en `CR:1220/1306/1360`, sin tocar `RPCClientHandler`), R21, R22-renderer (`EstimateSegments` `CR:4159`, `cachedWireKey` `CR:173/2319`), MEDIO l3 R04 (`CR:4067` mira `segments.Count()==0`), C1-cuantizar `nowMs` (`CR:3227-3228`), C8 (`CR:2510`) | linter; lectura: `BuildOccSamples` se llama tras rellenar `cachedJoints` (`CR:2288→2299`, `TRI-ren:56`); la purga de owner (`CR:2633-2635`) reconstruye o invalida las dos caches; `CullTick` prueba extremos antes del early-out por owner (`CR:2413-2438`); `HasRenderableWires` cuenta geometria real (`CR:892`); snapshot con generacion menor se rechaza como el delta (`CR:1810`). In-game PENDIENTE: cable con esquinas tras pared visible (R01); cable visible junto al destino con owner a 80 m (R08) | nada |
| **L6 sorter-ui-v4** | recuperar la UI de rechazos y refrescos | `scripts/4_World/test/LFPG_SorterController_TEST.c`, `test/LFPG_SorterView_TEST.c`, `test/LFPG_SorterTagView_TEST.c`, `test/LFPG_SorterPreviewRow_TEST.c`, `scripts/4_World/LFPG_RPCClientHandler.c` | S21 (TRIVIAL), S09 (TRIVIAL: guard `success` en `RPCClientHandler.c:967`), S11-cliente (timeout in-flight de Save/Sort como el de Preview, `SorterController_TEST.c:881`, `DEF:358`), S20, MEDIO l5 (generacion en `RefreshCargoPreview` `:1709/:1768`), MENOR l5 (`SorterView_TEST.c:1384` guard `SorterPanel`), MENOR l6 (`:34` comentario) | linter + `ui_reconcile.py --strict` 0 FAIL; lectura: `RefreshRulesDisplay` (`:1377`) llama `RefreshRail_TEST`; Save/Sort in-flight expiran por temporizador y liberan el boton; respuesta de preview con generacion vieja se descarta. In-game PENDIENTE (necesita fixture alimentada+enlazada, `HANDOFF.md:80-81`): rail actualiza al borrar regla; Save rechazado por distancia libera el boton | nada |
| **L7 sorter-server** | parser y vinculo del sorter, lado compartido | `scripts/3_Game/LFPG_SorterData.c`, `SL`, `scripts/4_World/LFPG_Sorter.c` | S01, S02 (parser: fronteras de objeto y publicacion atomica), S19 (TRIVIAL), S17, S15-lado destino (`SL:1007-1012` distancia en `ResolveOutputContainer`), H4-micro (`SL:978` tabla de nombres de puerto, opcional) | linter; lectura: `FromJSON` acepta 0 reglas como valido (`SorterData.c:594`) y no deja estado parcial al fallar (`:332,:466,:492,:579`); `GhillieSuit_ColorBase` → CLOTHING (`SL:748-762`); una sola busqueda de contenedor y exige cargo (`Sorter.c:373/480`, `:409/:515`). In-game PENDIENTE: Reset All + Save persiste vacio y reabre vacio | nada |
| **L8 file-persistence** | que un target ilegible no bloquee el backup | `scripts/3_Game/LFPG_FileUtil.c`, `scripts/3_Game/LFPG_Settings.c` | SEC10 (contenido en los wrappers `FileUtil.c:707/745/888`: si el target no parsea y el backup si, restaurar; SIN tocar consumidores), SEC11, SEC19, V3-06 (verificar target tras `CopyFile` `FileUtil.c:282`, conservando `.saving` `:271`, abortos y hooks `:859`) | linter; **harness `LFPG_FaultInject` (T5, `d1ebf87`; citado en `DIC-l5:55`)**: matriz target-corrupto/backup-valido → carga backup; copia truncada que reporta exito → `false` y el credito no reaparece al siguiente boot; `Settings.Load` publica solo estado aceptado (`Settings.c:520-545`). Sin harness no se declara (PLAN-CONJUNTO T5) | nada; **revisor cruzado obligatorio (dinero)** |
| **L9 btc** | lo contenido del dinero | `scripts/5_Mission/LFPG_BTCHelper.c`, `scripts/5_Mission/LFPG_BTCSessionRegistry.c`, `scripts/4_World/LFPG_DeviceRegistry.c`, `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c` (solo `:1504`) | err=14 (cinco `Write` de saldo: `BTCHelper.c:1640,2127,2294,2515,2697` como `:1160`), SEC12 (`RemoveOrdered` en `BTCSessionRegistry.c:250,383`), SEC13-residual (bajar `LFPG_BTC_MAX_FORWARD_GAP` si el dueño quiere: 1 constante), MEDIO l7 #2 (levantar el latch cuando `m_AllRegistered` no tiene vivos con ese ID: `DeviceRegistry.c:79`, `NativeImpl.c:1504`) | linter; **harness**: stock 42/saldo 137, cap+1 en las seis operaciones → las seis informan 42/137 sin tocar ledger (`INFORME-ASTRA l7:190`); dos ATM con ID igual borrados en la misma sesion → el sweep vuelve a avanzar (`DIC-l7:15-23`); 64 terminales + 2 nuevos → se expulsa el mas antiguo | nada; **revisor cruzado obligatorio** |
| **L10 devices-kits** | conservar estado al sustituir entidades | `scripts/4_World/LFPG_ActionDismantleDevice.c`, `LFPG_KitBase.c`, `LFPG_KitBaseDeployable.c`, `LFPG_ActionUpgradeSolarPanel.c`, `LFPG_ActionUpgradeWaterPump.c` | D06 (AMPLIO, 3 ficheros todos aqui), D07 (2 ficheros aqui), MENOR l1 (comentario `ActionDismantleDevice.c:27`) | linter; lectura: salud copiada dispositivo→kit (`:182-211`) y kit→dispositivo (`KitBase.c:193`, `KitBaseDeployable.c:138`); sobrantes y filtro conservan salud/subtipo (`UpgradeWaterPump.c:201-218,258-267`) manteniendo el staging atomico actual. In-game PENDIENTE (desmontar es accion continua: manual): dispositivo al 40 % → kit → desplegado al 40 % | nada |
| **L11 devices-classes** | contratos cliente/servidor de dispositivos | `scripts/4_World/LFPG_RemoteController.c`, `LFPG_Furnace.c`, `LFPG_BatteryAdapter.c`, `LFPG_IDevice.c`, `LFPG_Intercom.c`, `LFPG_MotionSensor.c` | D08, D24, D21 (TRIVIAL), D22, D10 (4 fast-paths, conservando el fallback), D14 (bool de politica sincronizado en el horno; sin tocar `Settings.c`), D09-parcial (`Remove` antes de cada `CallLater` en `RemoteController.c:539-540,559-560`), D15-parcial (`count` acotado en `RemoteController.c:642`; modo validado en `MotionSensor.c:343`), D11-Intercom | linter; lectura por item: `LFPG_GetStoredEnergy` del adaptador lee la SyncVar (`BatteryAdapter.c:477`); `bestDist` arranca en `searchRadius` (`IDevice.c:199,218`); `EEInit` del horno respeta `m_LFPG_IsHologramProjection` tras `super` (`Furnace.c:129-147`); sync de parejas al cambiar de dueño (`RemoteController.c:365`). In-game PENDIENTE: inspector muestra capacidad del adaptador (D24); mando cedido muestra etiqueta correcta (2 clientes) | nada |
| **L12 client-hud** | inspector y mision, correccion visual | `scripts/4_World/LFPG_DeviceInspector.c`, `scripts/5_Mission/LFPG_TankHUD.c`, `MI`, `scripts/4_World/LFPG_CableHUD.c` | R12, R13, R27, R30/§6.4 (retirar o acotar `SetLV/SetTextLV`, `MI:140-141`), C10 (trivial) | linter + `ui_reconcile.py --strict`; lectura: cabecera y filas usan el mismo conjunto de offsets (`DeviceInspector.c:1286-1293` vs `:1418-1430`); refresco de datos electricos con cadencia (`:523-572`); `BeginFrame` 0×0 invalida `IsReady` (`CableHUD.c:215-218,:95`). In-game PENDIENTE: filas del inspector no se solapan con sorter/bateria | nada |
| **L13 cctv-laser-searchlight** | no expulsar al espectador; salida CCTV verificable | `scripts/4_World/LFPG_CameraViewport.c`, `LFPG_Camera.c`, `LFPG_LaserBeamRenderer.c`, `LFPG_SearchlightController.c`, `LFPG_Searchlight.c` | R14 (2 ficheros, ambos aqui), R29, R22-CCTV (`CameraViewport.c:1107` scanline muerto), MEDIO l3 R15 (fallback si `GetCurrentCamera()` nunca es null, `CameraViewport.c:787,809,897`), R17 | linter; lectura: `SafeAbort(entity)` solo aborta si la entidad es la de la sesion (`Camera.c:100,107` → `CameraViewport.c:249-262`); etiquetas recolocadas al cambiar resolucion (`:271-344`, `:1129-1140`); `ApplyOrientation` solo con cambio de angulos (`Searchlight.c:570-624`). **In-game OBLIGATORIO para R15** (el MEDIO dice que el predicado puede ser circular, `DIC-l3:19-22`): entrar y salir de CCTV dos veces seguidas con MCP `key_press`/`ui_*`; si no se puede automatizar, manual y PENDIENTE | nada |
| **L14 assets-hygiene** | quitar del repo lo que no se empaqueta y esta huerfano | `data/kits/kitboxtexture.png`, `data/furnace/Furnace.ogg.stereo_backup`, `data/pressure_pad/sounds/pressure_pad_press.ogg.stereo_backup`, `data/waterpump/pump.ogg.stereo_backup` (+ `data/furnace/Furnace_mono.ogg` solo si `grep` = 0 refs) | §5.2-parte, §5.3 NO (repuntar rvmat exige verificacion visual) | `grep -rl <nombre> config.cpp model.cfg data` = 0 refs por fichero borrado; linter (`.rvmat`/`config.cpp`) 0 errores; NO se toca `kitboxtexture.paa` (referenciado por `data/kits/lf_kit_box.p3d`), ningun `.p3d` ni `water.ogg`. Backup = git (destructiva, `G5` del dueño) | aprobacion del dueño (borra ficheros) |
| **L15 cierre (serial)** | residuo de S2, SEC18, constantes locales → `DEF`, y T0c si el dueño lo pide | `DEF`, `RPCS` (solo despacho `:113-161` y `:2523`), `scripts/4_World/LFPG_RPCGuard.c` (`:48-67`), `scripts/3_Game/LFPG_Migrators.c` (borrar la clase: 0 llamadores segun `TRI-red:170` y `DIC-l8:44`), y solo si T0c: `test/*.c`, `gui/layouts/test/*`, `MI`, `LFPG_Actions.c`, `LFPG_ActionSyncSorter.c`, `LFPG_ActionRegistration.c`, `LFPG_RPCClientHandler.c`, `stringtable.csv` | S2-residuo (5 despachos + 5 politicas + `allowUnpowered`), SEC18, consolidacion de constantes, T0c (clases de UI; **nunca** `LFPG_Sorter_TEST`/`_Kit` de `config.cpp:1080/1086`) | linter 0 errores es el gate que caza referencias colgando tras borrar (`AGENTS.md:31-32`); `ui_reconcile.py --strict` 0 FAIL si hay rename; `git merge-tree` limpio contra `main` ya fusionado; **arranque diag**: mundo carga, panel V4 abre sobre `LFPG_Sorter` y sobre `LFPG_Sorter_TEST`, spam del SubId 19 → deny sin `ctx.Read` (`RPCS:41`) | **L4, L6, L12, L13 fusionadas** (comparte ficheros con ellas) y, si T0c, todas las de sorter |

Oleada 2 (tras fusionar la 1; cruzan ficheros y no caben hoy): G06 (`NM`+`EG`, top-1 de `TRI-gra:201`),
G20 mitad manager, G05, MEDIO l7 #1 (que iteradores usan `GetAllRegisteredForSafety`: `NM:1205`,
`MI:28`, `DeviceRegistry.c:126/165`), U6 (acumulador cliente: `CR`+laser+`MI`), SEC05-lado handler
(`RPCS:223/:1712` con `RateLimitedWarn`), SEC16 (tras SEC10), S12 (contrato de identidad de peticion),
y la lane de journal si el dueño la aprueba (C).

---

## E. ORDEN Y PARALELISMO

- **Simultaneas sin conflicto:** con la regla estricta del brief, **13** (L1, L2+L3 como una, L4,
  L5, L6, L7, L8, L9, L10, L11, L12, L13, L14); con la excepcion medida de zonas en `NM`, **14**.
  Ninguna de ellas comparte un fichero con otra: lo comprobe lane a lane sobre la lista de ficheros
  de D (el unico cruce potencial, `LFPG_RPCClientHandler.c` entre L5 y L6, se resuelve dejando R20 en
  `CR:1220` y el fichero entero a L6).
- **Techo practico:** no es git, es la revision: cada lane exige un dictamen de otra familia (G7 del
  dueño) y un arranque diag por lote. Recomiendo **dos lotes de 7**: lote 1 = las de impacto directo
  en jugadores (L2, L4, L5, L6, L7, L8, L10); lote 2 = L1, L3, L9, L11, L12, L13, L14. Las dos de
  dinero (L8, L9) se fusionan las ultimas de su lote para que el harness corra sobre el arbol final.
- **Bloqueos:** L3 fusiona despues de L2 (mismo fichero, zonas distintas). L15 espera a L4, L6, L12 y
  L13 (comparte `RPCS`, `test/*`, `MI`). Nada mas bloquea a nada. Las decisiones de F no bloquean
  ninguna lane: todas las lanes de D estan definidas sin depender de ellas.
- **Secuencia:** lote 1 (7 en paralelo) → merges en seco uno a uno con linter → arranque diag #1 →
  lote 2 (7 en paralelo) → merges → L15 serial → arranque diag #2 con los PENDIENTE in-game
  acumulados (fixture de sorter alimentado+enlazado, R15, 2 clientes donde haga falta) → oleada 2.
- **Que hacer si Cursor/Codex caen a medias:** N baja y se dice; las lanes son independientes, asi
  que un hueco no invalida los merges ya hechos.

---

## F. LO QUE NO HARIA

1. **S1, partir `NM` (7.810 ln) ahora.** Cero valor para el jugador; conflicto con L2, L3 y la oleada
   2 entera. Cuando el backlog de `NM` este vacio, partir por los dos cortes naturales que ya son
   zonas: sorter (`NM:5918-6600`) y deteccion (`NM:6997+`).
2. **S3, borrar la reflexion `Call*`.** Rompe `LFPG_Generator`, `LF_TestLamp`, `LF_TestLampHeavy`
   (`LFPG_TestDevices.c:53,930,1354`), que estan en `units[]` (`config.cpp:187`) y por tanto pueden
   estar colocadas. Migrarlas a la jerarquia es una decision de producto (¿se siguen enviando las
   lamparas de test?) con riesgo de persistencia, no una limpieza.
3. **S4/U2, congelar o partir BTC y fusionar los dos registries.** Refactor sin consumidor sobre el
   camino del dinero con jugadores reales; el triaje avisa que la retencion BTC es anti-replay
   (`TRI-red:217`). Solo entra lo contenido (L9) y lo que decida el journal (C).
4. **Los 5 CONFLICTO de journal (V3-02, V3-03, V3-05, H-04, H-05).** No son codigo: son "esto no es
   una decision del implementador". Si el dueño acepta la ventana de crash, se cierran como riesgo
   aceptado con cita (`INFORME-ASTRA l7:46-78,152-166`), como se hizo con D16 (PLAN-CONJUNTO:94-103).
5. **S5, `ActionToggleBase<T>` y base de vista comun.** Doce ficheros de acciones que los jugadores
   usan a diario, sin ningun fallo funcional detras; el chrome duplicado ya bajo de ×3 a ×2 con T0b.
6. **S6-hooks y `LFPG_GetKitClassname`.** Los hooks vacios son el patron declarado de la casa
   (`lfpg_devicebase.c:18`, `WireOwnerBase.c:15`); el kit classname lo acaba de usar D05.
7. **S7, partir `DEF`.** `3_Game` lo ve todo (`AGENTS.md:52-55`); mover constantes choca con todas las
   lanes a la vez. Regla en su lugar: no añadir reservas.
8. **Todo lo de coste sin perfil**: H2, H3, H7-sync, C2, C3, C5, C12, U5, U6-parcial y las fichas de
   coste D10-parte, D12, D13, D18, D19, D23, G14, G15, R05, R06, R07, R10, R11, R16, R17-parte, R18,
   R19, R23, R24, R28, S07, S24, SEC04, SEC17. El propio triaje lo dice: "la prioridad de costes no
   esta sustentada por perfil" (`TRI-ren:312`, `TRI-sor:261`, `TRI-gra:222`). Primero un perfil con
   `LFPG_PERFDIAG_ENABLED` (`DEF:405`) y `LFPG_Telemetry` (que l8 dejo silencioso salvo PERFDIAG,
   `DIC-l8:28-34`) en el servidor privado; luego elegir tres.
9. **Las DUDOSAS como lanes:** D20 (politica de diagnostico), G16 (sin enumeracion), R26-B (visual),
   S10 (P0.1 cerrado: "no se reproduce", `HANDOFF.md:97-100`, `DIC-l5:65-66`), S13 (promesa de
   producto). S18 no es lane: es UNA sonda in-game (`ConfigGetInt "itemSize 0"` vs
   `GetInventoryItemSize`, `TRI-sor:172-180`) que se hace en el arranque diag #2.
10. **Fichas de contrato**: S12 (identidad de peticion), S16 (cambia formato guardado), S23, D15-resto,
    D17 (depende de LBmaster), SEC15, SEC14-BTC, G05/G08/G10/G15/G22 (AMPLIO de grafo; G06 va a la
    oleada 2), R23. Cada una necesita una decision de formato o de producto antes de una linea.
11. **MEDIO l4 #1 (reemplazo al tope se deniega).** Arreglarlo exige reservar capacidad en
    `LFPG_WireHelper.AddWire` y en el grafo; con jugadores reales prefiero el fallo conservador actual
    (el cable viejo se queda, `DIC-l4:5-13`) a una reserva nueva sin harness.
12. **§5.1 (`.p3d` de 40 MB), §5.3 (repuntar `.rvmat`), `water.ogg`.** Es trabajo de Blender/py3d,
    TexView y ffmpeg con verificacion visual, no de un subagente de Codex. Y PLAN-CONJUNTO:86 dejo
    dicho que la huella no manda. Decision del dueño: ¿importa el PBO de 99 MB?
13. **§5.4-TEST en server, `LF_TestLamp` en `units[]`, mover `reviews/`, gates de build (§5.5).** No
    hay script de build en el repo; `reviews/` no se empaqueta (no esta en `include.lst`) y
    `AGENTS.md:115` lo declara como sitio oficial de evidencia. Proceso del dueño.
14. **§6.3 y §6.7.** Reenviar teclas con UI abierta deshace un diseño explicito (`MI:177-180`); los
    comentarios `feature-frozen` no tienen consumidor ejecutable.
15. **T0c antes que nada, y en cualquier caso los dos classnames de `config.cpp:1080/1086`.** Ver C.
16. **Reindentar ficheros para cerrar los 6 MENOR de tabs.** `AGENTS.md:76-77` lo prohibe; se cierran
    por convencion ("tabs en lo nuevo") y ya.

---

## G. LO_NO_VERIFICADO

- **Nada in-game.** Ninguna afirmacion de comportamiento de este plan se ejecuto. El unico arranque
  es el de `HANDOFF.md:58-77` y no cubre panel abierto, SEC01 ni R15 (`HANDOFF.md:79-83`).
- **Los veredictos de las 92 fichas son del triaje sobre `d59cad8`.** Reabri y re-anclé en `e5b5303`
  solo: SEC05 (manager muerto), SEC12 (siguen los dos `Remove(0)`), SEC13 (gap acotado), SEC14
  (mitad Util muerta), U1/H6, U3, H2, H5, H8, C1, C8, C10, §6.2, §6.3, §6.4, el estado de `_TEST` y
  los despachos V3. Las otras ~75 fichas las cito por el `path:line` del triaje sin haberlo reabierto;
  si l7/l8 movieron alguna de esas lineas, la cita esta desfasada aunque el mecanismo siga.
- **De los MEDIO/MENOR reabri solo** `SL:1133` (S08) y `RPCS:2523`. Las lineas de los MEDIO de l4
  (`RPCS:608,811,777,790`), l5 (`:1709,:1768`) y l7 (`DeviceRegistry.c:79,126,165`, `NativeImpl.c:1504`)
  van por el dictamen.
- **Citas de la auditoria que no relocalice:** C4 (`CR:3399-3402`), C9 (`LFPG_WiringClient.c:615-1062`),
  U5 (los 7 conteos `:6841-6848`), las rutas de sonido `config.cpp:58` y `:90` (mi `grep "\.ogg"
  config.cpp` no devuelve nada: probablemente `samples[]` sin extension), `lf_solarpanel==t2` y
  antenas (solo re-hashee el par `switch_v1`), las "5 plantillas" de `.rvmat`, y si `Furnace_mono.ogg`
  tiene referencia (existe: 3.650 B; no busque quien lo usa).
- **`include.lst`:** refute que sea una whitelist inclusiva porque `config.cpp` esta en el PBO
  desplegado (`server-script.log:9,:30`), pero no lei el script/packer que construyo ese PBO (no
  esta en el repo; `reviews/2026-09-08-arranque-verificacion/` solo tiene 4 logs). La presencia de
  los `.p3d` en el PBO no la prueban esos logs: no hay `Create entity type 'LFPG_…'` en los RPT y los
  dos unicos `cannot load` son materiales vanilla (`client.RPT`); el spawn de `LFPG_Sorter` por MCP
  lo afirma `HANDOFF.md:73-74`, no un log que yo haya leido.
- **La excepcion de zonas en `NM`** esta medida sobre UN par (l3: 13 lineas; l4: 48 lineas). Hunks
  futuros mas cercanos pueden chocar; lo que la hace segura es el `merge-tree` en seco por lane, no
  el precedente. Tampoco inspeccione el contenido del arbol `e5c364e0` que produjo.
- **Persistencia de `LFPG_Sorter_TEST`/`_Kit`:** no se cuantos hay colocados en el servidor privado
  ni si `scope` influye en la carga de entidades ya colocadas; por eso el plan no los toca.
- **Harness `LFPG_FaultInject`:** lo doy por existente por `d1ebf87` (merge T5) y `DIC-l5:55`; no abri
  el fichero ni se que casos cubre. Si no cubre "copia truncada que reporta exito", L8 no tiene gate
  y hay que escribirlo antes (o abandonar V3-06).
- **`RecountAllPlayerWires` (`NM:1505`), `RemoveOrdered`, `CanTakeFromContainer` (`SL:56`),
  `LFPG_SORTER_LINK_RADIUS`, `WireOwnerBase.c:161`:** nombres tomados del triaje y del dictamen l7,
  no releidos por mi en `e5b5303`. Antes de que una lane los use, `grep` de la firma real.
- **Cifras de §0 de la auditoria:** medi 149 `.c` / 78.131 lineas frente a 152 / 72.249; no reconcilie
  la aritmetica (T2 +1.162, T0b −7.739 segun `HANDOFF.md:10`) y no se donde estan los 3 ficheros de
  diferencia.
- **Recuento 9 MEDIO / 11 MENOR:** mi tally por los ocho dictamenes (oleada 1: 8 MEDIO con 1 arreglado
  en `d59cad8` + 7 MENOR; l6: 0 + 2 (+1 GRAVE de proceso, resuelto por el receptor); l7: 2 + 2; l8: 0)
  coincide con el brief; no coincide necesariamente con como el orquestador etiqueto los "MENOR" de
  l1 (yo cuento tabs y comentario como 2, el dictamen lista 2 MENOR).
- **No corri el linter ni `ui_reconcile.py`** (no cambie nada); las lineas base que cito son las de
  `AGENTS.md:34,46` y `val_main.json` (`status WARN, 0 errores, 56 warnings` en la base pre-oleada-2).
- **Otras lanes:** no lei ningun `PLAN.md` ajeno. Vi seis directorios en `lanes/`; el mio estaba vacio
  al empezar.
