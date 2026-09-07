# BRIEF — Council LFPowerGrid: plan conjunto de acción sobre dos auditorías

## 0. Tu papel y la única pregunta

Eres una lane de un council de tres. Las tres recibís **este mismo fichero, byte a byte**. No veis
lo que responden las demás. El orquestador arbitra después.

**La pregunta:**

> Dadas las dos auditorías de abajo y el triage que ya se ha hecho contra el código actual,
> ¿cuál es el plan de acción correcto, y en qué orden? Concretamente: **¿se cierra y jubila la V3
> del sorter ANTES o DESPUÉS de arreglar los P1 de integridad?**

## 1. Antes de valorar el menú: ataca el encuadre

**Primero responde a esto, y solo después opines sobre el orden:** ¿qué puede estar mal en el
encuadre del problema o en las opciones propuestas? Puedes aceptar un hecho y refutar la
inferencia que el orquestador saca de él. Un council que solo vota las opciones que hereda no
sirve para nada; el valor está en poder rechazar la premisa.

Sitios donde sospechar, sin limitarte a ellos:
- ¿Es correcto tratar la huella como el objetivo, dado el hecho H6?
- ¿El triage prueba lo que dice que prueba, o el pre-filtro tiene un falso negativo?
- ¿Los tramos están bien cortados, o hay dependencias cruzadas que los rompen?
- ¿Falta algún trabajo que ninguna de las dos auditorías vio porque ninguna conocía el objetivo?

## 2. Reglas de esta lane

- **Propón, no dispongas.** No escribes código ni editas nada. Tu entregable es texto.
- **No inventes APIs.** Todo lo que afirmes del código sale de las citas `path:line` que este
  brief te da. Si necesitas un hecho que no está aquí, va a `LO_NO_VERIFICADO`, no a una
  suposición presentada como hecho.
- **Cifras con universo.** Si das un número, di de dónde sale. Los números de este brief traen el
  suyo.
- Castellano.

## 3. Contexto del proyecto (lo que ninguna de las dos auditorías sabía)

LFPowerGrid es un mod de DayZ (Enforce Script), ~152 ficheros `.c`, 2.818.739 B de scripts,
286 clases. El objetivo declarado por el dueño del proyecto es **jubilar la V3 del sorter para
dejar más hueco en el mod**, y lleva semanas de trabajo en una V4 (sufijo `_TEST`) con ese fin.

Restricción del motor: DayZ tiene un límite de **arena** de script. «Hueco en el mod» significa
arena, no bytes en disco.

Ramas vivas (todas salen del tronco `289592b`; `main` sigue en `d61705e` y lleva 17 commits de
retraso): `sorter/v4-finish` (+5, es HEAD `421cabb`), `debt/v3-data-integrity` (+1),
`maint/audit-kimi-followup` (+1), `release/lfpg-footprint-rc` (=`289592b`).


## 4. Triage ya hecho contra el código actual

# Triage de las 25 fichas P1 contra HEAD `421cabb` (rama `sorter/v4-finish`)

Método: (1) pre-filtro mecánico — intersección de los ficheros que cita cada ficha contra
`git diff --name-only --ignore-all-space d61705e..HEAD`; (2) lectura dirigida por símbolo
(nunca por número de línea) de las 19 fichas cuyos ficheros sí habían cambiado.

**Resultado: 24 VIVO + 1 PARCIAL. Cero corregidas.**

| ID | Veredicto | Dónde está hoy | Nota |
|---|---|---|---|
| G01 | VIVO (intacto) | `5_Mission/LFPG_ElecGraphImpl.c:3673+` | fichero sin un solo cambio desde `d61705e` |
| G02 | VIVO | `5_Mission/LFPG_ElecGraphImpl.c:3010`, `:3052` | solver suma `m_VirtualGeneration`, validador solo mira `incomingPower` |
| G04 | VIVO | `4_World/LFPG_VanillaActionOverrides.c:100`,`:172` | `RefreshSourceState` tiene un único llamador y ninguno parte de generador vanilla |
| G18 | VIVO (intacto) | `5_Mission/LFPG_ElecGraphImpl.c` | — |
| SEC01 | VIVO | `5_Mission/LFPG_NetworkManagerImpl.c:2150` y 7 sitios más | los 9 `.Send(` migraron idénticos; el FullSync valida identidad y luego envía `null` |
| SEC02 | VIVO | `5_Mission/LFPG_RPCServerHandlerImpl.c:602`,`:680` | `RemoveWiresTargeting` con `allowOthers=true` por defecto; el camino de corte SÍ aplica política — asimetría confirmada |
| SEC09 | **PARCIAL** | `3_Game/LFPG_FileUtil.c:580-585` (guarda nueva), `:214-218` (residuo) | arreglado en `ef29b73`; quedan dos vías: `.tmp` parseable sin target, y el último recurso documentado en `:280-283` |
| SEC20 | VIVO | `5_Mission/LFPG_RPCServerHandlerImpl.c:283`,`:465-496`,`:750-790` | **no hay validación del nombre de puerto en servidor antes de mutar**; solo longitud ≤32. `GetDeviceId()==""` en vanilla salta `HasPort`/`CanConnectTo`. Difunde antes de insertar en el grafo y no revierte |
| R02 | VIVO | `4_World/LFPG_CableRenderer.c:2444-2510`,`:2556-2567` | la rama owner-null hace `continue` antes de recalcular `cachedMinDist`; decide con distancia congelada |
| R04 | VIVO | `4_World/LFPG_CableRenderer.c:2182`,`:3992`; `3_Game/LFPG_Defines.c:223` | presupuesto global 512 sin prioridad espacial; `CullTick` nunca decrementa |
| R15 | VIVO | `4_World/LFPG_CameraViewport.c:846-855`,`:757-779` | timeout de 5 s llama al mismo cleanup sin confirmar restauración; sin token de sesión |
| S04 | VIVO | `5_Mission/LFPG_NetworkManagerImpl.c:6193-6197`,`:6272-6276` | al agotar presupuesto el cursor global se queda en el mismo sorter; hay reanudación local pero no rotación |
| S08 | VIVO | `5_Mission/LFPG_NetworkManagerImpl.c:6566`; `LFPG_SorterLogic.c:1105-1327` | `maxEval=200` no envuelve `RepackCargoInPlace`, que es O(N²) + hasta N pasadas. **V4 TEST hereda el mismo pico** (`RPCServerHandlerImpl.c:147-150`) |
| E02 | VIVO (intacto) | `5_Mission/LFPG_BTCHelper.c` | — |
| E03 | VIVO (intacto) | `5_Mission/LFPG_BTCHelper.c` | — |
| E04 | VIVO | `5_Mission/LFPG_BTCHelper.c:1465`,`:1505-1513` | bloque **byte a byte idéntico** al del commit auditado. BTC destruidos, crédito falla, se emite error y se sale sin restituir |
| E08 | VIVO (intacto) | — | — |
| E16 | VIVO | `5_Mission/LFPG_BalanceProvider_NativeImpl.c:1230-1243`; `4_World/LFPG_BTCAtm.c:165-176` | stock en RAM + tombstones borrados de forma durable, en distinto mecanismo y momento |
| D01 | VIVO | `4_World/LFPG_Furnace.c:453-470`,`:540-547` | deadline absoluto reescrito en cada encendido; `m_BurnNextMs` no se persiste |
| D02 | VIVO | `4_World/LFPG_Furnace.c:121-145`,`:289-292` | `super.EEInit()` activa el calor antes de que exista `m_UTSource`; el guard lo traga |
| D03 | VIVO | `4_World/LFPG_Battery.c:341-352`; `NetworkManagerImpl.c:7644`,`:7722` | round-trip float→int×10→float por tick; el adaptador no lo sufre → asimetría de precisión |
| D04 | VIVO | `4_World/LFPG_Intercom.c:822-831`; ausente en `config.cpp` | `LFPG_GhostPASBroadcaster` **no está declarada en CfgVehicles**; `config.cpp` es el único config del árbol y no tiene includes. El hermano `LFPG_GhostPASReceiver` sí está (`:2562`). Función muerta |
| D05 | VIVO | `4_World/LFPG_BatteryAdapter.c:84-89`; `lfpg_devicebase.c:430-455` | el comentario promete recogida con F, pero no reoverridea los tres guards heredados; y sin kit no hay desmontaje. Irrecuperable |
| D16 | VIVO (potencial) | `4_World/LFPG_DoorController.c:574-612`,`:754-804` | cero coincidencias de propiedad/permiso/candado en la ruta Fence |
| A03 | VIVO (intacto) | — | la cobertura actual valida el checker, no la lógica |

## Lo que el triage NO cubre
- **Las 126 fichas P2/P3 siguen sin triar una a una.** El pre-filtro mecánico dice que 28 de ellas
  viven en ficheros intactos (vivas por construcción) y 98 en ficheros tocados (desconocidas).
- Tres fichas necesitan ejecución para cerrarse: D16 (¿`Fence.OpenFence()` respeta el candado?),
  G02 (cuánto persiste la incoherencia), SEC01 (recuento de entregas por cliente).
- Dos lanes no pudieron correr `git` por el shell de su entorno; sus veredictos salen de lectura
  del árbol actual, que el encargo aceptaba como criterio decisorio.


---

## 5. Censo de acoplamientos de la V3 (verificado por grep del orquestador)

## PARTE B — Censo V3 y bloqueadores

Los tres bloqueadores conocidos **siguen en HEAD**:

**(a) Confirmado.** `SORTER_TEST_RESYNC = 65` tiene receptor (`LFPG_RPCServerHandlerImpl.c:157-160`), guard (`LFPG_RPCGuard.c:66`) y ACK cliente (`LFPG_RPCClientHandler.c:79-81`). Ningún emisor: el único `Write` de resync es V3 `LFPG_ActionSyncSorter.c:101` → `SORTER_RESYNC` (29). `SORTER_TEST_CARGO_REFRESH = 70` tiene receptor (`RPCClientHandler.c:91-93`, `:976`) y **ningún productor**: `BroadcastCargoRefreshToNearby` fija `refreshSubId = LFPG_RPC_SubId.SORTER_CARGO_REFRESH` (34) en `LFPG_NetworkManagerImpl.c:6316`. Esto no impide borrar los `.c` V3; sí impide que V4 sustituya del todo el cableado RPC al jubilar V3.

**(b) Confirmado, y más amplio.** No solo `LFPG_BTCAtmController.c`: también `LFPG_BTCAtmView.c` (64 usos de `COL_*` + la clase `LFPG_ColorData` definida en `LFPG_SorterView.c`).

**(c) Confirmado.** `LFPG_Actions.c:528` y `LFPG_ActionSyncSorter.c:69` miran solo `LFPG_SorterView.IsOpen()`. `LFPG_ActionOpenSorterPanel.c:73-75` y `LFPG_ActionOpenSorterPanel_TEST.c:65-67` ya miran las dos.

`config.cpp` **no nombra** `LFPG_SorterView` / `Controller` / `TagView` / `PreviewRow` / `ColorData`. Los `.layout` V3 son artefactos propios de esa UI.

### Tabla de acoplamientos (símbolos V3 desde ficheros que no son los cuatro de producción)

| símbolo V3 | fichero:línea que lo usa | qué necesita de la V3 | ¿bloquea el borrado? | sustituto V4 si existe |
|---|---|---|---|---|
| `LFPG_SorterView.Init` | `scripts/5_Mission/LFPG_MissionInit.c:148` | precrea el panel V3 | sí | `LFPG_SorterView_TEST.Init` (hoy lazy en `Open`, `:1310`) |
| `LFPG_SorterView.IsOpen` / `HandleEscKey` | `LFPG_MissionInit.c:176,180` | traga teclas con el panel V3 abierto | sí | `_TEST.IsOpen` / `HandleEscKey` ya en `:185-189` |
| `LFPG_SorterView.IsOpen` / `IsEscCooldown` | `LFPG_MissionInit.c:227,229` | no abrir menú pausa al soltar ESC | sí | `_TEST` ya en `:231-233` |
| `LFPG_SorterView.IsOpen` / `Close` | `LFPG_MissionInit.c:250,268` | cierra V3 si muere/inconsciente | sí | bloque `_TEST` en `:272+` |
| `LFPG_SorterView.IsOpen` | `LFPG_MissionInit.c:275` | dual-open: TEST cede si V3 está abierta | sí (hay que quitar el yield) | no hace falta tras borrar V3 |
| `LFPG_SorterView.Cleanup` | `LFPG_MissionInit.c:473` | teardown de misión | sí | `_TEST.Cleanup` ya en `:475` |
| `LFPG_SorterView.Open` | `LFPG_RPCClientHandler.c:568` | abre UI V3 al `CONFIG_RESPONSE` | sí | `_TEST.Open` en `:937` |
| `LFPG_SorterView.OnSaveAck` | `LFPG_RPCClientHandler.c:580` | ACK de guardado V3 | sí | `_TEST.OnSaveAck` `:949` |
| `LFPG_SorterView.OnSortAck` | `LFPG_RPCClientHandler.c:592` | ACK de sort V3 | sí | `_TEST.OnSortAck` `:961` |
| `LFPG_SorterView.OnPreviewData` | `LFPG_RPCClientHandler.c:729` | preview V3 | sí | `_TEST.OnPreviewData` `:1098` |
| `LFPG_SorterView.IsOpen` | `LFPG_Actions.c:528` | bloquea cableado con panel V3 abierto; **no** consulta TEST | sí | `LFPG_SorterView_TEST.IsOpen` (hay que añadirlo; hoy no está) |
| `LFPG_SorterView.IsOpen` | `LFPG_ActionSyncSorter.c:69` | no resync V3 con UI abierta; **no** consulta TEST | sí | idem; la acción emite solo `SORTER_RESYNC` (29), no 65 |
| `LFPG_SorterView.IsOpen` | `LFPG_ActionOpenSorterPanel.c:73` | no abrir V3 si ya hay panel | sí mientras viva la entidad `LFPG_Sorter` | ya combina con `_TEST.IsOpen` `:75` |
| `LFPG_SorterView.IsOpen` | `LFPG_ActionOpenSorterPanel_TEST.c:67` | dual-open desde la acción TEST | sí hasta borrar el guard | `_TEST.IsOpen` `:65` |
| `LFPG_SorterView.IsOpen` | `LFPG_SorterView_TEST.c:1329` | `Open` TEST aborta si V3 está abierta | sí hasta borrar el guard | ninguno (es el mutex anti dual-open) |
| `LFPG_SorterView.COL_RED` | `LFPG_BTCAtmController.c:186,803` | paleta ATM | sí | `LFPG_SorterView_TEST.COL_RED` (`:257`); mejor extraer paleta compartida, no acoplar el ATM a TEST |
| `LFPG_SorterView.COL_GREEN` | `LFPG_BTCAtmController.c:190,807` | paleta ATM | sí | `_TEST.COL_GREEN` `:252` |
| `LFPG_SorterView.COL_RED_BTN` | `LFPG_BTCAtmController.c:418` | paleta ATM | sí | `_TEST.COL_RED_BTN` `:266` |
| `LFPG_SorterView.COL_BTN` | `LFPG_BTCAtmController.c:421,424` | paleta ATM | sí | `_TEST.COL_BTN` `:258` |
| `LFPG_SorterView.COL_TEXT_DIM` | `LFPG_BTCAtmController.c:436,437` | paleta ATM | sí | `_TEST.COL_TEXT_DIM` `:260` |
| `LFPG_SorterView.COL_TEXT` | `LFPG_BTCAtmController.c:445` | paleta ATM | sí | `_TEST.COL_TEXT` `:259` |
| `LFPG_SorterView.COL_TEXT_MID` | `LFPG_BTCAtmController.c:446` | paleta ATM | sí | `_TEST.COL_TEXT_MID` `:261` |
| `LFPG_SorterView.COL_BG_PANEL` | `LFPG_BTCAtmView.c:567` | paleta ATM | sí | `_TEST.COL_BG_PANEL` `:247` |
| `LFPG_SorterView.COL_HEADER` | `LFPG_BTCAtmView.c:568,640` | paleta ATM | sí | `_TEST.COL_HEADER` `:262` |
| `LFPG_SorterView.COL_GREEN` | `LFPG_BTCAtmView.c:569,587,588,599,636` | paleta ATM | sí | `_TEST.COL_GREEN` |
| `LFPG_SorterView.COL_BTN` | `LFPG_BTCAtmView.c:570,620,628,659,666` | paleta ATM | sí | `_TEST.COL_BTN` |
| `LFPG_SorterView.COL_TEXT_DIM` | `LFPG_BTCAtmView.c:571,574,581,582,583,584,593,598,641,642,660,667` | paleta ATM | sí | `_TEST.COL_TEXT_DIM` |
| `LFPG_SorterView.COL_AMBER` | `LFPG_BTCAtmView.c:572,585,589,594` | paleta ATM | sí | `_TEST.COL_AMBER` `:256` |
| `LFPG_SorterView.COL_TEXT` | `LFPG_BTCAtmView.c:573,597,602,613,614,621,622,629,630,658,665` | paleta ATM | sí | `_TEST.COL_TEXT` |
| `LFPG_SorterView.COL_BG_ELEVATED` | `LFPG_BTCAtmView.c:577-580,592` | paleta ATM | sí | `_TEST.COL_BG_ELEVATED` `:249` |
| `LFPG_SorterView.COL_BLUE` | `LFPG_BTCAtmView.c:586` | paleta ATM | sí | `_TEST.COL_BLUE` `:255` |
| `LFPG_SorterView.COL_INPUT_BORDER` | `LFPG_BTCAtmView.c:595,600` | paleta ATM | sí | `_TEST.COL_INPUT_BORDER` `:250` |
| `LFPG_SorterView.COL_BG_INPUT` | `LFPG_BTCAtmView.c:596,601` | paleta ATM | sí | `_TEST.COL_BG_INPUT` `:250` |
| `LFPG_SorterView.COL_SEPARATOR` | `LFPG_BTCAtmView.c:608,639` | paleta ATM | sí | `_TEST.COL_SEPARATOR` `:261` |
| `LFPG_SorterView.COL_GREEN_BTN` | `LFPG_BTCAtmView.c:611,664` | paleta ATM | sí | `_TEST.COL_GREEN_BTN` `:264` |
| `LFPG_SorterView.COL_RED_BTN` | `LFPG_BTCAtmView.c:612` | paleta ATM | sí | `_TEST.COL_RED_BTN` |
| `LFPG_SorterView.COL_TEXT_MID` | `LFPG_BTCAtmView.c:615,616,623,624,631,632` | paleta ATM | sí | `_TEST.COL_TEXT_MID` |
| `LFPG_SorterView.COL_BLUE_BTN` | `LFPG_BTCAtmView.c:619,627` | paleta ATM | sí | `_TEST.COL_BLUE_BTN` `:263` |
| `LFPG_ColorData` (clase en `LFPG_SorterView.c:45`) | `LFPG_BTCAtmView.c:36,169,702,708,719` | cache de color por `SetUserData` | **sí: borrar `LFPG_SorterView.c` tumba el ATM** | `LFPG_ColorData_TEST` en `LFPG_SorterView_TEST.c:71` (no usarlo desde ATM; extraer la clase) |
| `LFPG_SorterController` | ningún fichero fuera de V3 | — | no (si se borran los 4 `.c` juntos) | `LFPG_SorterController_TEST` |
| `LFPG_SorterTagView` | ningún fichero de código fuera de V3; comentario en `gui/layouts/LFPG_SorterTag.layout:3` | prefab del tag | no (retirar el layout con la UI) | `LFPG_SorterTagView_TEST` + `gui/layouts/test/LFPG_SorterTag_TEST.layout` |
| `LFPG_SorterPreviewRow` | ningún fichero de código fuera de V3; comentario en `gui/layouts/LFPG_SorterPreviewRow.layout:3` | prefab de preview | no (retirar el layout con la UI) | `LFPG_SorterPreviewRow_TEST` + `gui/layouts/test/LFPG_SorterPreviewRow_TEST.layout` |
| `LFPG_SorterView` (comentario de layout) | `gui/layouts/LFPG_Sorter.layout:9` | layout raíz V3 (`GetLayoutFile` → ese path en `LFPG_SorterView.c:219`) | no (es el artefacto V3) | layout TEST bajo `gui/layouts/test/` |

**Coste real de jubilar V3 (lectura de HEAD):** no es borrar 4 ficheros. `LFPG_SorterController` / `TagView` / `PreviewRow` no tienen clientes externos. Lo que obliga trabajo es: (1) extraer paleta + `LFPG_ColorData` del ATM (~75 usos de `COL_*` entre view y controller); (2) retocar `MissionInit` y los handlers V3 de `RPCClientHandler`; (3) completar `IsOpen()` TEST en cableado y resync; (4) decidir qué hace `LFPG_ActionOpenSorterPanel` / entidad `LFPG_Sorter` si la UI V3 desaparece; (5) aparte, los agujeros RPC V4 del bloqueador (a), que no son acoplamientos V3 pero sí impiden que TEST sea un reemplazo completo.



**Contraverificación del orquestador (grep propio, HEAD `421cabb`):** `LFPG_ColorData` se define en
`LFPG_SorterView.c:45`; hay 29 `COL_*` declaradas allí; `LFPG_BTCAtmView.c` = 64 refs `COL_*` + 5
`LFPG_ColorData`; `LFPG_BTCAtmController.c` = 12 refs; `LFPG_MissionInit.c` = 9; `LFPG_RPCClientHandler.c` = 4;
`LFPG_SorterView_TEST.c` = 13. **Total fuera de la V3: 106 referencias.**


---

## 6. Borrador del orquestador — ESTO ES LO QUE DEBES ATACAR

# Plan conjunto de acción — BORRADOR DEL ORQUESTADOR (para ser atacado)

Este borrador NO es la conclusión. Es la posición del orquestador, puesta por escrito para que
cada lane pueda refutarla. Si el encuadre está mal, decirlo es más valioso que ordenar bien un
menú equivocado.

## A. Hechos verificados en esta sesión (con su medición)

| # | Hecho | Cómo se midió |
|---|---|---|
| H1 | La auditoría grande apunta a `d61705e`, que **es el merge-base exacto de las cuatro ramas vivas**. No está desfasada: `main` sigue en `d61705e` y lleva 17 commits de retraso respecto al tronco real `289592b` | `git merge-base`, `git rev-list --count` |
| H2 | De las 151 fichas, **34 viven en ficheros que ningún commit ha tocado** desde `d61705e`; 117 en ficheros que sí cambiaron | pre-filtro: intersección de ficheros citados por ficha contra `git diff --name-only --ignore-all-space d61705e..HEAD` |
| H3 | Triage de las 25 fichas P1: **0 CORREGIDAS**. 11 VIVO + 1 PARCIAL verificadas leyendo código; 6 vivas por H2; 7 en curso | lectura dirigida con `path:line` del árbol actual |
| H4 | Los 22 commits desde `d61705e` fueron sorter V4, split fachada/impl, privacidad de logs y deuda v3. **Ninguno atacó integridad** — la auditoría se entregó el 05-sep, después de casi todos | `git log d61705e..HEAD` |
| H5 | Huella del sorter: V3 = 143.010 B / **7 clases**; V4 TEST = 178.279 B / **10 clases**. V4 es 35 kB MÁS GRANDE que V3. Árbol entero: 286 clases, 2.818.739 B | `grep -c 'class'`, `stat` |
| H6 | **Recortar líneas NO libera arena.** Medición propia del proyecto: 6.466 líneas fuera = 1 kB; 1.071.376 B de fuente fuera = −2/0 kB. Solo la estructura compilada rindió: 15 clases = 44/45 kB | `assumptions.md` 2026-07-25/28, citado en `HANDOFF.md:159-161` |
| H7 | Sin rastro de despliegue público: no hay Workshop ID ni `publisherId` en `config.cpp`, ni mención de servidor en producción en el handoff | grep sobre `config.cpp` y `HANDOFF.md` |

**Advertencia de instrumento (H2):** «fichero intacto» es evidencia fuerte de que la ficha sigue
viva; «fichero tocado» **no** es evidencia de que se arreglara. El pre-filtro solo ahorra lectura
en la primera dirección. Los 217 clases que cuento hoy en `4_World` no casan con el 199→185 que
registró la palanca A2: mi regex y su método de conteo pueden no ser el mismo, así que la ratio
kB/clase se usa como orden de magnitud, no como cifra exacta.

## B. La consecuencia incómoda de H5 + H6

El objetivo declarado del proyecto es **jubilar la V3 para dejar más hueco en el mod**. Por la
medición propia del proyecto, eso rinde:

- borrar 143.010 B de fuente V3 → **~0 kB de arena** (H6);
- borrar **7 clases** → ~21 kB de arena por la ratio A2 (3,0 kB/clase).

Es decir: **la jubilación de la V3 vale ~21 kB de arena, no 143 kB.** Y hoy el mod carga las dos
implementaciones a la vez, así que el estado actual es el peor de los tres posibles.

## C. La decisión de producto que este council debe cambiar

Las dos auditorías dan órdenes **incompatibles** y ninguna de las dos conoce el objetivo del
proyecto:

- La auditoría grande (§8) ordena 12 PRs con **integridad primero** (PR1 destinatarios RPC, PR2
  permisos/puertos, PR3 commit/abort monetario) y pone **«Consolidación UI TEST/normal» en la
  PR 11 de 12**.
- La auditoría muse y el trabajo en vuelo ponen la **consolidación primero**.
- Chocan de verdad: las PR6/PR7 de la grande arreglan contrato y scheduler del sorter — código
  que la jubilación de la V3 reescribe o borra. Arreglar bugs en código que vas a borrar es
  trabajo tirado; borrar antes de arreglar puede llevarse por delante correcciones que la V4 no
  tiene (la ficha **S10** ya dice que las dos UIs **divergen** en protección de preview).

**La pregunta:** ¿se cierra y jubila la V3 ANTES o DESPUÉS de los P1 de integridad?

## D. Posición del orquestador (atacadla)

**Orden propuesto: T0 → T1 → T2 → T3 → T4, con T5 empezando en paralelo desde el día 1.**

| Tramo | Contenido | Por qué ahí |
|---|---|---|
| **T0** | Cerrar V4 y jubilar V3: decidir la paridad `powered/linked` de la acción TEST, cerrar los bloqueadores del censo, borrar las 7 clases V3, renombrar `_TEST` → nombres definitivos | Es lo único en vuelo; quita el impuesto de escribir cada arreglo del sorter dos veces; S10 dice que el fork YA diverge. **Alcance duro: sin refactor.** |
| **T1** | Integridad monetaria: E04, E16, SEC09 residual, E02, E03, E08 | Es lo único que destruye valor del jugador de forma irreversible |
| **T2** | Autoridad de servidor: SEC02, SEC20, SEC01 | Saltos de política y destinatarios de red equivocados |
| **T3** | Dispositivos rotos: D04 (clase PAS que no existe en config), D05 (adaptador irrecuperable), D01/D02/D03 | Funciones muertas o que mienten al jugador |
| **T4** | Grafo y coste: G01, G02, G04, G18, R02, R04, S04, S08 | Ninguno pierde datos; son trabajo evitable y coherencia |
| **T5** | Instrumento: A03 + línea base del §7 de la auditoría | A03 dice que la cobertura actual valida el checker, **no** la lógica. Sin esto, ningún tramo se puede declarar sin regresión |

**Razonamiento de por qué T0 va primero, pese a B:** no porque la huella lo merezca —no lo
merece, son 21 kB— sino porque el fork es un **impuesto recurrente** sobre todo lo que venga
después, y T1..T4 incluyen 24 fichas del sorter (S01–S24) que habría que escribir dos veces
mientras vivan las dos UIs.

**Razonamiento de por qué T1 no va primero:** H7. Sin despliegue público, la pérdida de dinero es
**latente**, no daño activo. Si ese hecho es falso —si hay servidor con jugadores— **el orden se
invierte y T1 pasa a ser lo primero**.

## E. Lo que el orquestador NO ha verificado

- Las 117 fichas en ficheros tocados, salvo las P1: **P2 y P3 están sin triar**. El plan las trata
  como clases, no como fichas individuales.
- Si `Fence.OpenFence()` respeta la política de candado del motor (decide si D16 es exploit real).
- Si existe servidor privado con jugadores que H7 no detecta.
- La ratio kB/clase de A2 aplicada a la V3 es una proyección, no una medida sobre este borrado.

## F. Lo que el censo V3 cambió del propio borrador (añadido tras medirlo)

El borrador daba T0 por «pequeño y en vuelo». **Es falso.** El censo de acoplamientos, verificado
por grep propio, dice que jubilar la V3 no es borrar cuatro ficheros:

- `LFPG_ColorData` se define en `LFPG_SorterView.c:45`, y allí viven **29 constantes `COL_*`**.
- El **cajero BTC depende de esa paleta con 76 referencias**: `LFPG_BTCAtmView.c` (64 `COL_*` + 5
  `LFPG_ColorData`) y `LFPG_BTCAtmController.c` (12 `COL_*`). *Borrar `LFPG_SorterView.c` tumba el
  ATM.*
- `LFPG_MissionInit.c` engancha la V3 en nueve sitios (precreación, ESC, muerte/inconsciencia,
  cleanup, y el mutex anti dual-open) y `LFPG_RPCClientHandler.c` en cuatro (Open, SaveAck,
  SortAck, PreviewData).
- El propio `LFPG_SorterView_TEST.c` referencia la V3 **13 veces** (entre ellas el mutex de
  `Open`, `:1329`).
- `LFPG_SorterController`, `LFPG_SorterTagView` y `LFPG_SorterPreviewRow` **no tienen clientes
  externos**: esos tres sí se borran limpio.

**Consecuencia que el borrador no vio:** T0 y T1 tocan **el mismo subsistema**. Extraer la paleta
obliga a reescribir 76 líneas del cajero, y el cajero es justo donde viven E04/E16/SEC09. Hacerlos
por separado significa tocar el ATM dos veces, con la segunda vez encima de una refactorización
recién hecha y sin cobertura (A03).

**Revisión de la posición del orquestador:**

| Tramo | Cambio respecto a D |
|---|---|
| **T0a (nuevo, primero)** | **Extracción pura de la paleta**: sacar `LFPG_ColorData` y las 29 `COL_*` de `LFPG_SorterView.c` a un fichero neutro (p. ej. `3_Game/LFPG_UIPalette.c`) y repuntar los 76 usos del ATM + los 13 de V4. **Cero cambio de comportamiento**, commit aislado, revisable por diff mecánico. Desbloquea T0 y desacopla el ATM del sorter para siempre |
| **T0b** | Lo que era T0: paridad `powered/linked`, `MissionInit` + `RPCClientHandler`, `IsOpen()` de TEST en `LFPG_Actions.c:528` y `LFPG_ActionSyncSorter.c:69`, emisor de `SORTER_TEST_RESYNC (65)` y productor de `SORTER_TEST_CARGO_REFRESH (70)`, y borrado de las 7 clases V3 |
| **T1** | Sin cambio, pero ahora **se beneficia** de que T0a ya desacopló el ATM |

**Y una advertencia que el borrador no daba:** la ficha **S08** dice que el repack síncrono sin
presupuesto **lo hereda la V4** (`RPCServerHandlerImpl.c:147-150` enruta `SORTER_TEST_REQUEST_SORT`
al mismo handler). Jubilar la V3 **no** arregla S04/S08: los arrastra.


---

# 7. AUDITORÍA A — «muse-spark», 12 KB, sobre `c4ac943` (`sorter/v4-finish`)

# Auditoría LANE Consejo — LFPowerGrid `sorter/v4-finish` (`c4ac943`)

## 1. Resumen ejecutivo
> Commit `S2-B`: hook `TEST` para manejar sorter sin ratón vía `LFPG_MCP_SorterCmd.layout` + `Update()` polling.
> Buena postura: `RPCGuard` central + rebind `A1` + `McpCanEdit()` en mutaciones + `McpJsonEscape`.
> Coste principal: fork prod vs `TEST` (~80KB+75KB vistas, ~85KB+76KB controladores) con deriva visible.
> Hallazgo de paridad: `ActionOpenSorterPanel_TEST` no exige `powered/linked` como prod — ampliar superficie TEST.
> `Update()` polling + `WriteMcpDump` con `OpenFile($profile:)` en cada comando: I/O sin throttle explícito.
> No editar repo; solo propuestas. Incertidumbre alta por **TRUNCATED** en 6 ficheros grandes — ver marcas abajo.

## 2. Tabla de scores

| Eje | Score/10 | Luz | Rationale una línea |
|---|---|---|---|
| 1. Optimizaciones posibles | 6/10 | 🟡 | Cache `IS3` + throttling `Save/Sort/Preview` bien; lastra `Update()` polling + allocs/refresh admitidos en TODO |
| 2. Boilerplate / ruido | 3/10 | 🔴 | Fork V3→V4 + cabeceras históricas + `S1_PROBE=true` + scaffolding TEST |
| 3. Patrones subóptimos | 5/10 | 🟡 | Polling `Update`, magic numbers `600/16`, stringly `cat:`/`FindAnyWidget` por nombre |
| 4. Redundancia | 3/10 | 🔴 | Vista+Controlador prod vs `TEST` duplicados; dispatch RPC V3/V4 paralelo |
| 5. Vulnerabilidades seguridad | 6/10 | 🟡 | `RPCGuard`+`A1 rebind` sólidos; riesgo contenido en canal `ui_set_text` + `$profile` write |
| 6. Findings / bugs / footguns | 5/10 | 🟡 | Falta paridad `powered/linked` en acción TEST; trampas `World` compile + vida `SetUserData` |
| 7. Simplificar sin regresión | 6/10 | 🟡 | Hay refactors seguros pequeños (consts, helpers, hoist allocs) sin tocar set cerrado |

> Nota TRUNCATED: `LFPG_SorterView_TEST.c` (80964B), `LFPG_SorterController_TEST.c` (85494B), `LFPG_RPCServerHandlerImpl.c` (120629B), `LFPG_RPCClientHandler.c` (35383B), `LFPG_Sorter.c` (20710B), `LFPG_SorterController.c` (75992B), `LFPG_SorterView.c` (58433B) — solo extractos visibles. No inferir APIs fuera de CORPUS.

## 3. Hallazgos por eje

### 1. Optimizaciones posibles
- **med** `LFPG_SorterView_TEST.c:Update polling m_McpCmd` — `DispatchUiSetText → SetText; Update` sondea cada tick. Comentario propio lo admite. Coste CPU/UI tick + `FindAnyWidget(close)` + `McpCollectState` + `OpenFile/WRITE+FPrint+CloseFile` por comando en `WriteMcpDump`. Alternativa eventos no disponible → documentar cadencia/gate `m_IsOpen`.
- **low** `LFPG_SorterView_TEST.c:Sprint5 TODO` — admite `4 allocs per refresh` en `RefreshBuilderTab_TEST` pendientes de hoist a miembros. Evidencia directa de churn.
- **low** `LFPG_SorterController_TEST.c:m_SaveInFlight/m_SortInFlight/m_PreviewInFlight+m_PreviewDebounce` — bien: throttling anti-spam RPC. No tocar; extender patrón a hook MCP si se usa en bucle. *Incertidumbre TRUNCATED: lógica `Update()` completa no visible.*
- **low** `LFPG_SorterController_TEST.c:EnsureV4Cache_TEST` — bien: `FindAnyWidget` once + `m_V4CacheBuilt_TEST` early-exit. Ahorra ~80 lookups/refresh según comentario.

### 2. Boilerplate / ruido
- **med** Duplicación V3→V4: `LFPG_SorterView.c` vs `LFPG_SorterView_TEST.c` (`LFPG_ColorData` vs `LFPG_ColorData_TEST`), `LFPG_SorterController.c` vs `..._TEST.c`. `sprint0_clone.py` mencionado como sync manual — frágil.
- **low** `LFPG_SorterView_TEST.c:S1_PROBE=true` — `static const bool S1_PROBE` sin uso visible en extracto; ruido TEST.
- **low** Cabeceras Sprint 0-4 + `v2.1/v2.2/v2.4/v3.3` (~80 líneas) duplicadas en ambos forks. No funcional, dificulta diff.
- **low** `LFPG_MCP_SorterCmd.layout:543B` — `EditBoxWidgetClass 1x1 ignorepointer` correcto y mínimo; ejemplo anti-boilerplate. Mantener.

### 3. Patrones subóptimos
- **med** `LFPG_SorterView_TEST.c:DispatchMcpCommand/Update` — polling en `Update()` por limitación `DispatchUiSetText nunca corre OnClick`. Aceptable pero subóptimo; sin debounce/rate-limit visible.
- **med** Magic numbers: `LFPG_SorterTagView.c:SetData:600+output*16+(rule+1)` + stringly `cat:<idx>` parse con `ToInt()` + round-trip `idx.ToString()!=rest` en `DispatchMcpCommand`. Funciona pero frágil; centralizar en consts/helpers compartidos.
- **low** Stringly widgets: `EnsureV4Cache_TEST` + `WriteMcpDump:root.FindAnyWidget("BtnCloseX")` — nombres literales dispersos; ya mitigado con cache pero sin consts.
- **low** Enforce hygiene OK: `no ternaries, no ++/--, no foreach` respetado en muestras.

### 4. Redundancia
- **high** Paralelo prod/TEST: dispatch RPC en `LFPG_RPCServerHandlerImpl.c:HandleSorterConfigRequest(...,srvCfgRespIdT)` parametrizado bien, pero `LFPG_RPCClientHandler.c:HandleSorterTest*` duplica 6 ramas (`CONFIG_RESPONSE/SAVE_ACK/RESYNC_ACK/PREVIEW_RESPONSE/SORT_ACK/CARGO_REFRESH`). Oportunidad helper común.
- **med** `LFPG_SorterController_TEST.c:McpCanEdit()->CanEdit()` + `McpCategoryCount/McpCollectState` — wrappers finos solo para MCP; OK pero síntoma de controlador paralelo.
- **med** `LFPG_RPCGuard.c:PolicyForSubId` — 10× `if(subId==SORTER_*) return POLICY_SORTER_DOSSIER_FROZEN` + `RoutePolicy/PolicyName` con 8 ramas repetidas. Tabla/map reduciría líneas sin cambio conductual — pero **no proponer rewrite** sin harness; solo P3.
- **low** `LFPG_Sorter_TEST.c` + `LFPG_ActionOpenSorterPanel_TEST.c` vs `LFPG_ActionOpenSorterPanel.c`/`LFPG_ActionSyncSorter.c` — `SetActions()+super` + guards casi idénticos.

### 5. Seguridad
- **high (positivo, guardar)** `LFPG_RPCGuard.c:Admit/Authorize/CheckIdentity` + `LFPG_RPCServerHandlerImpl.c:Dispatch A1 rebind via sender.GetPlayer()` + `PolicyForSubId==0 deny` — niega desconocidos, valida `sender/player`, `0/0 netId`, `ruined`, `deviceId`, `distSq`. No debilitar.
- **med** Superficie MCP `ui_set_text`: `LFPG_SorterView_TEST.c:DispatchMcpCommand` set cerrado (`dump/close/catch_all/tab_preview/cat:<n>`) + `McpCanEdit()` en mutaciones (`catch_all`, `cat:`) + validación rango `idx<catCount` + `ToInt` round-trip. Bien. Riesgo si se amplía sin `McpCanEdit` + validación.
- **med** `WriteMcpDump:$profile:lfpg_sorter_mcp.json` con `McpJsonEscape(cmd/status)` + `McpJsonBool` — escape correcto; `OpenFile WRITE` por comando sin cooldown visible → abuso local/spam comandos = churn I/O. Sin exfiltración remota (solo `$profile` local).
- **low** `LFPG_RateLimiter.c:Allow` + `LFPG_NetworkManager.Get().AllowPlayerAction` en `Admit` — rate-limit admisión OK. *Incertidumbre TRUNCATED: cooldowns Sorter SAVE/SORT/PREVIEW en `Impl` no visibles; no afirmar límites.*
- **low** `LFPG_ActionOpenSorterPanel_TEST.c:OnExecuteClient` envía `SORTER_TEST_CONFIG_REQUEST(netLow,netHigh)` sin checks cliente extra — autorización es servidor (`Authorize` + distancia). Correcto; no añadir trust cliente.

### 6. Potenciales bugs / footguns
- **high** Paridad acción TEST: `LFPG_ActionOpenSorterPanel_TEST.c:ActionCondition` **no** chequea `LFPG_IsPowered()` ni `LFPG_IsLinked()`, mientras `LFPG_ActionOpenSorterPanel.c` exige ambos + `GetType()!=` exacto vs `IsKindOf` en TEST. TEST además hereda `LFPG_ActionSyncSorter` vía `super.SetActions()` (IsKindOf → visible en V4). Resultado: panel TEST abrible sin power/link; `McpCollectState(paired/powered)` lo reflejará pero flujo diverge de prod. ¿Intencional para test sin ratón? Documentar o alinear.
- **med** `McpJsonEscape` single-escape rule: comentario advierte `World` muere con `CParser: quoted string not closed` si se junta `\\`+`\"` en un literal. Footgun compilación crítico — ver §5.
- **med** Vida `SetUserData`: `LFPG_SorterView_TEST.c:m_ColorDataRefs/m_TintedWidgets + SetUserData(null)` antes de clear — fix `F1-B heap 0xc0000374` correcto. Riesgo de regresión si se refactoriza hover sin respetar raw-pointer.
- **low** `WriteMcpDump:file==0 → Warn+return` + `closeUid` default 0 si `FindAnyWidget` falla — manejo nulo correcto.
- **low** Raza open-panel: ambas acciones chequean `IsOpen()` de ambas vistas — bien. *TRUNCATED: `Open()/Close()/Update()` cuerpo no visible; no validar doble-ESC/focus.*

### 7. Simplificar sin regresión
- Propuestas seguras (solo proponer):
  1. Hoist `array<string>` en `RefreshBuilderTab_TEST` a miembros (TODO propio).
  2. Consts para `600/16`, `cat:` prefix, nombres widgets MCP (`BtnCloseX`), path `$profile:lfpg_sorter_mcp.json`.
  3. Unificar `McpJsonBool/McpJsonEscape` en helper compartido TEST/prod si existe duplicado (verificar fuera de CORPUS antes).
  4. Colapsar ramas `PolicyForSubId` SORTER a helper `IsSorterSubId()` sin cambiar políticas.
  5. Eliminar `S1_PROBE` si muerto + podar cabeceras históricas a `reviews/` (no en código).

## 4. Lista priorizada (P0–P3)

- **P0-1 Set cerrado MCP — no ampliar sin guard.** *Por qué:* `DispatchMcpCommand` hoy solo muta tras `McpCanEdit()` + rango. *Riesgo si se ignora:* CSRF local vía `ui_set_text` → mutación sorter. *Primer paso seguro:* test manual `cat:-1/cat:999/cat:abc/dump/close` + assert `ok=false` y sin RPC; documentar set en comentario layout.
- **P0-2 Congelar `McpJsonEscape` single-escape.** *Por qué:* rompe compilación `World` entero. *Riesgo:* boot muerto. *Paso:* lint `grep` de literales con doble escape en `4_World/`; prohibir en revisión.
- **P1-1 Aclarar paridad `powered/linked` TEST.** *Por qué:* diverge de prod; MCP automatiza flujo irreal. *Riesgo:* tests verdes que en prod no abren. *Paso:* decidir y comentar en `ActionOpenSorterPanel_TEST`: o añadir `IsPowered/IsLinked` o marcar `// TEST-intencional: sin power gate`.
- **P1-2 Throttle `WriteMcpDump` / polling.** *Por qué:* `OpenFile WRITE` por comando + `Update` tick. *Riesgo:* spam I/O, lecturas externas a medio escribir. *Paso:* solo escribir si `cmd` cambió (`m_LastMcpCmd`) + early-return si `!m_IsOpen` salvo `dump`; medir.
- **P2-1 Hoist allocs `RefreshBuilderTab_TEST`.** *Por qué:* TODO propio, 4 allocs/refresh. *Riesgo:* GC churn UI. *Paso:* campos miembros reutilizables, `Clear()` no `new`.
- **P2-2 Deduplicar `HandleSorterTest*` cliente vía parámetro `isTest`.** *Por qué:* 6 handlers paralelos. *Riesgo:* deriva V3/V4. *Paso:* wrapper que mapea `SubId→view TEST/prod`, sin tocar lógica parse (ya siguen patrón `Sprint0 parametrized` del servidor).
- **P3-1 Consts + helpers `600/16/cat:/BtnCloseX`.** Sin cambio conductual, facilita auditoría.
- **P3-2 Podar `S1_PROBE`/cabeceras.** Solo ruido; hacer tras P0–P2.

## 5. Qué NO cambiar (guardas regresión)
- **Set cerrado MCP:** `dump/close/catch_all/tab_preview/cat:N` + `McpCanEdit()` en `catch_all` y `cat:`. No añadir `save/sort/prefix/contains/slot` sin `CanEdit+throttle+validación` y revisión consejo.
- **`McpJsonEscape` single-escape:** construir escapes por concatenación (`bs+bs`, `bs+quote`, `bs+"n/r/t"`). No juntar dos secuencias en un literal, no tocar comentarios que evitan secuencias ofensivas, no “simplificar” con librería JSON sin probar boot `World`.
- **`RPCGuard` deny-by-default + `A1 rebind` + `Authorize` (distancia/ruina/deviceId).** No fail-open nuevo.
- **Vida `SetUserData`:** siempre `SetUserData(null)` antes de liberar `m_ColorDataRefs`; `m_TintedWidgets` weak.
- **`GetType()!=LFPG_Sorter` en acción prod** (evita doble acción en V4) y chequeo dual `IsOpen()` en ambas acciones.
- **Layout `LFPG_MCP_SorterCmd` separado 1×1 `ignorepointer`:** no renombrar a widget del sorter ni fusionar con `LFPG_Sorter_TEST.layout` (anti-hijack `ui_reload_layout`).

## 6. Top 5 acciones
1. **P0 Congelar set MCP + `McpCanEdit`** — verificar `dump/close/catch_all/tab_preview/cat:N` no mutan sin pairing/power.
2. **P0 No tocar `McpJsonEscape`** — riesgo compilación `World`.
3. **P1 Resolver paridad `powered/linked` TEST vs prod** — documentar o alinear.
4. **P1 Gatear `WriteMcpDump`** — solo en cambio de `cmd` y panel abierto.
5. **P2 Hoist allocs + dedup cliente TEST** — menos churn y deriva.


---

# 8. AUDITORÍA B — 408 KB, 151 fichas, sobre `d61705e` (merge-base de todas las ramas)


> Se te dan el resumen, las notas, las prioridades, el registro completo de las 151 fichas y la secuencia de 12 PRs que propone. El detalle por ficha (2.880 líneas) se omite; las 25 fichas P1 ya están triadas en la sección 4.


## B.1 Resumen y alcance

# LFPowerGrid — Auditoría de optimización, simplificación y seguridad

**Fecha de entrega:** 5 de septiembre de 2026. **Repositorio:** [willy92wins/LFPowerGrid](https://github.com/willy92wins/LFPowerGrid). **Rama examinada:** `main`. **Commit fijado al iniciar la revisión:** [`d61705eb862a9781393a94335fab7315a4fd375a`](https://github.com/willy92wins/LFPowerGrid/commit/d61705eb862a9781393a94335fab7315a4fd375a), del 16 de agosto de 2026. Las referencias de código de este informe apuntan a ese commit, no a una rama que pueda cambiar.

La revisión se centra en trabajo evitable, comportamiento subóptimo, redundancias y formas de simplificar sin introducir regresiones. Incluye problemas de seguridad, integridad, persistencia y funcionamiento encontrados durante la inspección. Se mantiene separado lo demostrado en el código de lo que necesita comprobación en DayZ.

**Resultado:** 151 hallazgos y oportunidades únicos: 125 patrones o defectos confirmados estáticamente y 26 findings potenciales. Los confirmados incluyen oportunidades de mantenimiento: **no equivalen a 125 bugs ni a 125 vulnerabilidades**. G17 y D10 describen el mismo fast path; se conserva la referencia cruzada y se cuenta una sola vez. Los condicionantes adicionales dentro de una ficha se mantienen explícitos.

**Navegación del informe:**

- [1. Alcance y evidencias de la revisión](#1-alcance-y-evidencias-de-la-revisión)
- [2. Valoración general y puntuaciones](#2-valoración-general-y-puntuaciones)
- [3. Qué priorizaría](#3-qué-priorizaría)
- [4. Registro completo](#4-registro-completo)
- [5. Auditoría detallada por módulo](#5-auditoría-detallada-por-módulo)
- [6. Propuestas concretas para acortar código con garantías](#6-propuestas-concretas-para-acortar-código-con-garantías)
- [7. Plan para medir mejoras y evitar regresiones](#7-plan-para-medir-mejoras-y-evitar-regresiones)
- [8. Secuencia de implementación propuesta](#8-secuencia-de-implementación-propuesta)
- [9. Límites, hipótesis y descartes relevantes](#9-límites-hipótesis-y-descartes-relevantes)
- [10. Fuentes y trazabilidad](#10-fuentes-y-trazabilidad)

## 1. Alcance y evidencias de la revisión

| Dato | Resultado |
| --- | --- |
| Scripts Enforce inventariados | 148 archivos `.c` |
| Líneas físicas de scripts | 78.710, incluidos comentarios y líneas vacías |
| Líneas con código tras quitar comentarios/vacías | 58.234; recuento léxico, incluye delimitadores y preprocesador |
| Scripts de variante `test/` | 6 archivos, 4.824 líneas físicas |
| `NetworkManager` | 7.751 líneas |
| `CableRenderer` | 4.280 líneas |
| Implementación del grafo | 4.002 líneas |
| `RPCServerHandler` | 2.957 líneas |
| `config.cpp` / `model.cfg` | 2.603 / 1.879 líneas |
| Tests existentes ejecutados | 12/12 correctos: validan el checker offline |
| Barrido original ejecutado sobre el checkout de código | 162 textos, 148 Enforce; `FAIL=0`, `WARN=0` |
| Nuevos casos negativos del checker | 4 casos inválidos aceptados: A01–A02 |
| Archivos del repositorio modificados | Ninguno |

Se inspeccionaron los subsistemas en paralelo, se siguieron llamadas y guardas relevantes y se contrastaron los problemas prioritarios entre revisores. Se comprobaron todas las referencias de las fichas contra la copia fijada. La cobertura detallada por módulo distingue lectura completa de lectura dirigida; **no se afirma que cada línea de cada recurso binario o integración externa haya sido auditada**.

Los recursos `.p3d`, `.paa`, audio y demás assets no se decodificaron. El checkout de análisis incluye código, configuración raíz, layouts y revisiones existentes; los 104 `.rvmat` del árbol no se materializaron. Por ello el barrido local no se presenta como ejecución idéntica del job completo de CI. Se usó el árbol Git completo para inventario/tamaños y se inspeccionaron referencias relevantes de configuración.

No había un servidor DayZ/diag disponible para compilar ni ejecutar estos scripts. No se midieron FPS, latencias reales, memoria de la VM ni comportamiento de terceros como CodeLock, VSM o LBmaster. Los cálculos de complejidad, tamaños de payload y contraejemplos aritméticos son evidencias offline, **no benchmarks del motor**. Una corrección propuesta todavía necesita las regresiones indicadas antes de considerarse segura para producción.

### Cómo leer los estados y las prioridads

- **CONFIRMADO:** existe una ruta, inconsistencia, duplicación o patrón verificable en esta revisión. Cuando hay semántica externa relevante se aporta fuente primaria. El efecto práctico puede depender de carga, configuración o fallos de I/O expresamente descritos.
- **POTENCIAL:** falta confirmar semántica de motor, política de producto, integración externa o secuencia de persistencia. Incluye prueba concreta para confirmarlo o descartarlo.
- **P1:** corregir o confirmar primero; riesgo de integridad, autorización, trabajo desproporcionado o invariantes centrales. No significa automáticamente severidad crítica ni explotación observada.
- **P2:** mejora relevante o defecto acotado. **P3:** limpieza, coste secundario o herramientas de desarrollo. La prioridad concreta del despliegue depende del uso real del servidor.


## B.2 Valoración y puntuaciones

## 2. Valoración general y puntuaciones

**Mi valoración general es 5,5/10.** Hay una base técnica seria: adyacencias compartidas, presupuestos de trabajo, índices y cachés, autoridad del servidor, ciclo de vida común de dispositivos y mecanismos de staging/claims. Conservaría esas piezas y corregiría sus contratos antes de añadir más capas.

La deuda principal está en la coherencia entre esas piezas. El solver detecta cambios intermedios como si fueran finales; algunos receptores de red están mal seleccionados; la persistencia puede interpretar como confirmada una operación que devolvió error; una parte de las optimizaciones de UI conserva datos demasiado tiempo. El código también ha crecido mediante variantes, arrays paralelos y caminos parecidos que ya no reciben las mismas correcciones.

| Sección | Nota /10 | Qué cambiaría primero |
| --- | ---: | --- |
| Núcleo eléctrico y grafo | 5,5 | Detectar cambios finales, reconciliar generación virtual y hacer mutaciones locales. |
| Red, RPC y permisos | 4,5 | Destinatarios correctos, admisión antes de mutar y autorización común de reemplazo/corte. |
| Renderizado y UI general | 6,2 | Liberar/priorizar geometría residente, invalidar bien y evitar rebuilds globales. |
| Sorter | 6,0 | Equidad del scheduler, presupuesto del repack, parser/contrato único y UI compartida. |
| Economía y BTC | 5,4 | Conservar valor ante errores, presupuestar entidades y recorrer inventario una sola vez. |
| Dispositivos y ciclo de vida | 6,0 | Horno, precisión de batería, creación PAS, adapter y actualizaciones visuales por cambio. |
| Arquitectura y mantenibilidad global | 5,5 | Separar responsabilidades de NetworkManager y consolidar duplicación concreta. |
| Pruebas y garantías de no regresión | 3,0 | Escenarios de DayZ/diag, pruebas de fallo y línea base de rendimiento. |

Estas notas son juicio técnico consolidado, no una medición física ni el promedio del número de findings. Las subsecciones detalladas puntúan ámbitos más estrechos y explican sus fortalezas y límites. No se penaliza la existencia de guards, compatibilidad vanilla o temporales de Enforce por el mero hecho de ocupar líneas.

### Lo que conviene preservar

- El doble índice de aristas y las colas con presupuesto; corregir su convergencia e invalidación.
- La fachada `LFPG_DeviceAPI`: debe seguir atendiendo DeviceBase, el generador/lámpara con otra herencia y dispositivos vanilla o de terceros.
- Los checks autoritativos de servidor, límites de entrada y replay; compartir código sin suprimir fronteras de confianza.
- Staging, rollback, claims y errores explícitos en economía; completar sus invariantes duraderas.
- Culling, LOD, buffers reutilizados, debounce y agrupación de dirty; corregir su cobertura y frescura.
- Las mejoras ya presentes de dormancy en dispositivos y ciclos de vida; evitar volver a scans indiscriminados.


## B.3 Qué priorizaría

## 3. Qué priorizaría

### 3.1. Cambios con mayor interés para optimización

| Orden | Referencias | Cambio | Beneficio razonado y límite |
| --- | --- | --- | --- |
| 1 | SEC01 | Corregir destinatario en sincronizaciones y centralizar envío dirigido. | Elimina broadcasts involuntarios y duplicación por jugador. El tráfico real se debe medir. |
| 2 | G01 | Comparar asignaciones eléctricas finales una sola vez. | Evita reencolar por un estado intermedio en redes estables con demanda flexible. |
| 3 | S04, S05, S08 | Turnos justos, escaneo acotado y repack con presupuesto. | Reduce trabajo concentrado y latencia de otros sorters; no basta con subir el presupuesto. |
| 4 | R02, R04, R05 | Separar topología conocida de geometría residente y ordenar candidatos útiles. | Recupera presupuesto retenido y evita ordenar entradas invisibles sin cambios. |
| 5 | R06, R07, G06, SEC04 | Actualizaciones por owner/arista/componente. | Evita recorrer o reconstruir estructuras globales ante una edición local. |
| 6 | E01, E02, E03 | Snapshot de inventario por operación y preflight con límite de entidades. | Evita recorridos por denominación y creación destinada a abortar. |
| 7 | E06, E07 | Reducir serialización total e indexar claims por ATM. | El coste deja de depender innecesariamente de todos los saldos/claims; preservar durabilidad. |
| 8 | D10, D11, D12, D13 | Fast paths, visuales solo al cambiar e índices/candidatos locales. | Mejoras más pequeñas y medibles sobre caminos frecuentes. |
| 9 | G11, A05, D20, R25 | Corregir telemetría y controlar logging. | Hace fiables las mediciones y evita trabajo de diagnóstico ajeno al modo elegido. |

### 3.2. Integridad y seguridad que resolvería antes de una limpieza amplia

- **SEC02/SEC03/SEC20:** permisos y validación prospectiva de cableado deben ejecutarse antes de borrar o almacenar. Un error de admisión no puede dejar topología/almacenamiento distintos.
- **SEC09/E04/E05/E16:** el resultado de una operación monetaria debe coincidir con lo recuperado al reiniciar. Un retorno de error no repara por sí mismo BTC ya destruidos ni un `.tmp` que después será promovido.
- **G02/D03/D01:** alimentación, energía almacenada y tiempo de combustible necesitan una fuente de verdad consistente.
- **D04/D05:** completar la definición del objeto PAS y las vías de recuperación del adaptador.
- **S01/S02/S03/S11/S12:** contrato del sorter válido de extremo a extremo, incluso vacíos, errores y respuestas tardías.
- **S14/S15/S16/D16/D17:** confirmar con los mods y políticas reales los riesgos potenciales de inventario, enlaces, puertas y grupos. No convertir hipótesis en incidencias explotadas.

### Tres hallazgos graves contrastados por segunda revisión

1. **G01:** con `oldFinal=20`, demanda 20 y ratio soft=1, la implementación pasa por `20→0→20` y marca cambio aunque el resultado sea idéntico. Una fuente→splitter→batería puede volver a encolar vecinos entre épocas; una fuente directa a una batería hoja no prueba por sí sola un bucle. Los límites 64/256 nodos/aristas por callback, warmup y requeue por época acotan cada ejecución, no garantizan convergencia global. El fix debe comparar el total final una vez.
2. **SEC09:** el problema no exige que falte el archivo principal ni un crash. Tras fallar la promoción de un guardado, puede restaurarse bien el archivo anterior y aun así conservarse el `.tmp` nuevo. AddBalance revierte RAM; DepositCash conserva billetes; una carga posterior prioriza ese `.tmp`. Requiere que sobreviva sin sobrescritura y que la recuperación tenga éxito. Un reinicio limpio también puede bastar. Se ha contrastado el flujo, no se ha inyectado el fallo dentro de DayZ.
3. **A06/SEC20:** configurar 128 no hace que la UI normal genere 65 cables vanilla. Hay un solo puerto vanilla y límites de 12 aristas por dirección en el grafo. La inconsistencia de constantes existe; SEC20 identifica una vía distinta, mediante nombres de puerto inventados por cliente modificado, que necesita validación antes de mutación. Se separan los dos escenarios para evitar un falso positivo.


## B.4 Registro completo de las 151 fichas

## 4. Registro completo

Las fichas enlazan al detalle y, dentro de él, al código del commit. «Confirmado» engloba defectos y oportunidades estáticas; los costes de motor siguen pendientes de perfilado. G17 se referencia como revisión adicional de D10 y no aumenta el total.

| ID | Área | Prioridad | Estado | Hallazgo |
| --- | --- | --- | --- | --- |
| G01 | Grafo | P1 | Confirmado | La asignación flexible señala cambios aunque la asignación final sea idéntica |
| G02 | Grafo | P1 | Confirmado | El validador apaga lógicamente baterías que sí generan desde almacenamiento |
| G03 | Grafo | P2 | Confirmado | Reconstruir el seguimiento excluye fuentes vanilla con cables de salida |
| G04 | Grafo | P1 | Potencial | cambios de funcionamiento de una fuente vanilla no notifican al solver |
| G05 | Grafo | P2 | Confirmado | El FullSync de un jugador retiene cambios de cables para todos |
| G06 | Grafo | P2 | Confirmado | Un CutAll local reconstruye todo el grafo y pierde estado transitorio de baterías ajenas |
| G07 | Grafo | P2 | Confirmado | La poda diferida deja cuotas por jugador con cables que ya no existen |
| G08 | Grafo | P2 | Confirmado | La carga de BatteryCharger depende del tamaño y actividad de todo el grafo |
| G09 | Grafo | P2 | Confirmado | El límite global de nodos compara el estado actual en vez del resultado |
| G10 | Grafo | P2 | Potencial | rebuild desde persistencia no aplica límites de tamaño ni detección de ciclos |
| G11 | Grafo | P2 | Confirmado | La telemetría repite el último coste activo durante todos los ticks inactivos |
| G12 | Grafo | P3 | Confirmado | El broadcast completo calcula destinatarios y posiciones dos veces |
| G13 | Grafo | P3 | Confirmado | Delta y vanilla serializan antes de saber si hay receptores |
| G14 | Grafo | P3 | Confirmado | La compactación de la cola hace dos copias y una asignación evitable |
| G15 | Grafo | P3 | Confirmado | Metadatos 1:1 del nodo viven en mapas paralelos innecesarios |
| G16 | Grafo | P3 | Confirmado | Duplicaciones concretas que pueden convertirse en helpers pequeños |
| G18 | Grafo | P1 | Potencial | se cuenta como proveedor activo un PASSTHROUGH cuya salida sólo representa demanda |
| G19 | Grafo | P3 | Confirmado | El test booleano de descendientes sigue visitando aristas después de encontrar uno |
| G20 | Grafo | P3 | Confirmado | Rebuild crea nodos desconectados para borrarlos y repite hidratación inmediatamente |
| G21 | Grafo | P3 | Potencial | la API legacy de puerto energizado puede devolver propietarios obsoletos |
| G22 | Grafo | P2 | Potencial | el fallback de fábrica de grafo no se recupera y puede inundar logs |
| G23 | Grafo | P2 | Confirmado | La cola de FullSync no es FIFO por usar Remove(0) |
| SEC01 | Red/seguridad | P1 | Confirmado | Las sincronizaciones «unicast» y con filtrado de proximidad usan broadcast |
| SEC02 | Red/seguridad | P1 | Confirmado | Reemplazar un cable evita la política AllowCutOthersWires |
| SEC03 | Red/seguridad | P2 | Confirmado | El reemplazo elimina conexiones aunque la nueva no pueda almacenarse |
| SEC04 | Red/seguridad | P2 | Confirmado | Cortar un IN siempre escanea todos los dispositivos y cables, incluso sin cambios |
| SEC05 | Red/seguridad | P2 | Confirmado | El rechazo de spam genera trabajo de red y logs proporcional al spam |
| SEC06 | Red/seguridad | P2 | Confirmado | La resincronización de lote puede forzar SyncVars repetidamente sin necesidad real |
| SEC07 | Red/seguridad | P3 | Confirmado; aspectos potenciales | Strings de cliente sin normalizar llegan al RPT; ciertos límites se aplican tras deserializar |
| SEC08 | Red/seguridad | P2 | Confirmado | Un operador puede abandonar el foco y mantenerlo reservado indefinidamente |
| SEC09 | Red/seguridad | P1 | Confirmado | Un guardado que devuelve fallo puede confirmarse en el siguiente arranque |
| SEC10 | Red/seguridad | P2 | Confirmado | El fallback de archivo acepta un target corrupto y no intenta el backup válido |
| SEC11 | Red/seguridad | P2 | Potencial | Settings.Load deserializa directamente sobre el singleton y no restaura defaults al fallar |
| SEC12 | Red/seguridad | P2 | Confirmado | El historial BTC no conserva las últimas 64 respuestas: Remove(0) no es FIFO en Enforce |
| SEC13 | Red/seguridad | P3 | Confirmado | Una secuencia saltada al máximo inutiliza las futuras transacciones de ese UID |
| SEC14 | Red/seguridad | P3 | Confirmado | Cachés por UID y avisos conservan datos de toda la vida del proceso |
| SEC15 | Red/seguridad | P2 | Confirmado | RPCGuard clasifica ocho políticas pero solo aplica Admit/Authorize al inspector |
| SEC16 | Red/seguridad | P3 | Confirmado | Los tres guardados duplican el mismo protocolo de archivos pese a que la parte duplicada no necesita genéricos |
| SEC17 | Red/seguridad | P3 | Confirmado | FINISH_WIRING calcula y valida dos veces la geometría y parte de sus condiciones |
| SEC18 | Red/seguridad | P3 | Confirmado | Migrators es infraestructura huérfana y describe una cadena que ya no se ejecuta |
| SEC19 | Red/seguridad | P3 | Confirmado; aspectos potenciales | TryReadRawJsonVersion lee y copia todo el archivo para localizar un encabezado |
| SEC20 | Red/seguridad | P1 | Confirmado | Los puertos inventados de dispositivos vanilla evitan el límite físico y provocan divergencia store/grafo |
| R01 | Render/UI | P2 | Confirmado | La rama de oclusión específica de cables con esquinas nunca recibe los waypoints |
| R02 | Render/UI | P1 | Confirmado | La limpieza de owners desaparecidos decide con una distancia congelada |
| R03 | Render/UI | P2 | Confirmado | Eliminar un owner deja conexiones y dispositivos conocidos en cachés derivadas |
| R04 | Render/UI | P1 | Confirmado | El presupuesto global reserva segmentos invisibles y no prioriza cables próximos |
| R05 | Render/UI | P2 | Confirmado | Selection sort cuadrático cada dos segundos, incluyendo cables invisibles |
| R06 | Render/UI | P2 | Confirmado | Cada cambio local reconstruye el índice global de conexiones |
| R07 | Render/UI | P2 | Confirmado | El delta ahorra red pero reconstruye toda la geometría del owner |
| R08 | Render/UI | P2 | Confirmado | El early-out del owner oculta cables largos aunque el jugador esté junto al destino |
| R09 | Render/UI | P2 | Confirmado | El canvas sigue trabajando cada frame aunque no haya cables que puedan dibujarse |
| R10 | Render/UI | P2 | Potencial | Las claves de proyección omiten FOV y orientación completa de cámara |
| R11 | Render/UI | P2 | Confirmado | La detección de movimiento para oclusión depende del FPS y pierde desplazamientos acumulados |
| R12 | Render/UI | P2 | Confirmado | El inspector conserva indefinidamente datos de conexiones que cambian sin variar la topología del owner |
| R13 | Render/UI | P2 | Confirmado | Dos cálculos distintos del offset del inspector solapan filas de batería/sorter |
| R14 | Render/UI | P2 | Confirmado | La destrucción de cualquier cámara puede cerrar la sesión CCTV de otra |
| R15 | Render/UI | P1 | Potencial | El timeout de salida CCTV desactiva la cámara sin confirmar la restauración del jugador |
| R16 | Render/UI | P3 | Confirmado | TankHUD consulta objetos del escenario cuatro veces por segundo aunque nunca haya una bomba |
| R17 | Render/UI | P2 | Confirmado | El foco recalcula toda la predicción visual mientras permanece inmóvil |
| R18 | Render/UI | P2 | Confirmado | Registrar N láseres recorre repetidamente todos los anteriores |
| R19 | Render/UI | P2 | Confirmado | El filtro de frustum de láser se actualiza a 4Hz y produce pop-in al girar |
| R20 | Render/UI | P2 | Potencial | Los snapshots pueden retroceder las generaciones locales |
| R21 | Render/UI | P2 | Confirmado | La cola de sync no deduplica solicitudes que ya están en vuelo |
| R22 | Render/UI | P3 | Confirmado | Código y estado muertos sobreviven a refactors y se siguen manteniendo |
| R23 | Render/UI | P2 | Confirmado | La geometría usa un objeto gestionado por cada segmento para datos que caben en una polilínea |
| R24 | Render/UI | P3 | Confirmado | La preparación de geometría y varios helpers matemáticos están duplicados |
| R25 | Render/UI | P2 | Confirmado | El renderer crea y emite muchos logs operativos aun en uso normal |
| R26 | Render/UI | P2 | Confirmado | Los cables recién construidos se dibujan inicialmente como cercanos y visibles |
| R27 | Render/UI | P3 | Potencial | BeginFrame rechaza resolución 0×0 pero los productores pueden dibujar con dimensiones antiguas |
| R28 | Render/UI | P3 | Confirmado | Los helpers de inspector leen/formatean el mismo estado dos veces y su caché es más genérica de lo necesario |
| R29 | Render/UI | P3 | Confirmado | Los widgets CCTV se recolocan sólo al crearse, mientras el canvas sí reacciona a resolución |
| R30 | Render/UI | P3 | Potencial | MissionGameplay modifica el brillo global de widgets sin guardar/restaurar estado |
| S01 | Sorter | P2 | Confirmado | Reset All no se puede guardar; una configuración vacía se trata como inválida |
| S02 | Sorter | P2 | Confirmado | Parser no transaccional y búsqueda de delimitadores fuera de su objeto |
| S03 | Sorter | P2 | Confirmado | Save acepta configuraciones que Preview rechaza por tamaño |
| S04 | Sorter | P1 | Confirmado | El presupuesto global rompe el round-robin y puede privar de turno a otros sorters |
| S05 | Sorter | P2 | Confirmado | Se copia todo el cargo antes de aplicar los límites de trabajo |
| S06 | Sorter | P2 | Confirmado | La reanudación de una evaluación no invalida al cambiar los cables |
| S07 | Sorter | P2 | Confirmado | Compilar filtros y cachear rutas por tipo evita trabajo repetido en el camino caliente |
| S08 | Sorter | P1 | Confirmado | Repack manual síncrono no está acotado por el presupuesto del scheduler |
| S09 | Sorter | P2 | Confirmado | Repack sin transferencias no actualiza el inventario del solicitante |
| S10 | Sorter | P2 | Confirmado | Dos implementaciones de UI publicadas casi idénticas ya divergen en protección de preview |
| S11 | Sorter | P2 | Confirmado | Save/Sort pueden quedar bloqueados hasta cerrar el panel por rechazos normales |
| S12 | Sorter | P2 | Confirmado | Las respuestas de preview/ACK no identifican instancia ni revisión de reglas |
| S13 | Sorter | P3 | Potencial | Preview muestra coincidencias de una regla, no el resultado real del routing |
| S14 | Sorter | P2 | Potencial | Preview de contenedor bloqueado: posible divulgación de inventario |
| S15 | Sorter | P2 | Potencial | Validez de enlace no se comprueba simétricamente en origen, destino y operación manual |
| S16 | Sorter | P2 | Potencial | Restaurar NetworkID como identidad persistente puede enlazar el contenedor equivocado |
| S17 | Sorter | P2 | Confirmado | Búsqueda de contenedores duplicada y contrato incompatible para attachment-only |
| S18 | Sorter | P2 | Potencial | Lectura de dimensiones por rutas «itemSize 0/1» necesita validar API |
| S19 | Sorter | P3 | Confirmado | GhillieSuit se clasifica como accesorio de arma antes que ropa |
| S20 | Sorter | P2 | Confirmado | UI reconstruye vistas y re-resuelve botones aunque no haya cambios útiles |
| S21 | Sorter | P3 | Confirmado | El rail de reglas de la UI TEST no se refresca tras ediciones granulares |
| S22 | Sorter | P2 | Confirmado | El scaler global es sobrescrito al abrir por primera vez la UI TEST |
| S23 | Sorter | P3 | Confirmado | Referencias/campos muertos y scaffolding histórico evitables en los chips |
| S24 | Sorter | P3 | Confirmado | Guards de acceso repetidos, resolución de salidas repetida y estado paralelo excesivo |
| E01 | Economía | P2 | Confirmado | El inventario completo se recorre una vez por denominación y se vuelve a recorrer para construir cada respuesta |
| E02 | Economía | P1 | Confirmado | La compra en efectivo crea entidades antes de comprobar que el jugador puede pagarlas |
| E03 | Economía | P1 | Confirmado | Los límites monetarios no limitan el número de entidades generadas por una transacción |
| E04 | Economía | P1 | Confirmado | Sell a cuenta pierde BTC si falla el guardado del crédito después de destruir los objetos |
| E05 | Economía | P2 | Confirmado | DepositCash persiste un crédito antes de comprobar que ese importe es representable por los billetes disponibles |
| E06 | Economía | P2 | Confirmado | Cada transacción y cada contador del barrido de claims reserializa todos los saldos y claims |
| E07 | Economía | P2 | Confirmado | El índice de claims por ATM falta y los barridos vuelven a filtrar la lista global |
| E08 | Economía | P1 | Confirmado | La configuración de monedas admite classnames duplicados, solapados con BTC o completamente inválidos |
| E09 | Economía | P2 | Confirmado | El greedy no encuentra siempre una selección exacta y puede consumir más billetes de los necesarios |
| E10 | Economía | P2 | Confirmado | refreshSeconds se valida y anuncia, pero el planificador usa siempre sesenta segundos |
| E11 | Economía | P2 | Confirmado | El último precio no caduca y el contador de errores no implementa el backoff que anuncia |
| E12 | Economía | P2 | Confirmado | La UI muestra una cotización que no se vincula al precio finalmente cobrado |
| E13 | Economía | P3 | Confirmado; aspectos potenciales | Quedan helpers sin consumidores y un subsistema de compound actions sin uso en el addon |
| E14 | Economía | P2 | Confirmado | Cuatro serializers RPC y seis bloques de validación reproducen la misma lógica |
| E15 | Economía | P2 | Confirmado | Ocho compras a cuenta bloquean más compras en el mismo ATM hasta una futura reconciliación de arranque |
| E16 | Economía | P1 | Potencial | La compensación de claims REFUNDED elimina el tombstone antes de demostrar que el stock compensado quedó persistido en hive |
| E17 | Economía | P2 | Confirmado | El parser de precios acepta números truncados y confunde claves de distintos niveles/monedas |
| E18 | Economía | P2 | Confirmado; aspectos potenciales | Las protecciones numéricas están incompletas fuera de precio×BTC |
| E19 | Economía | P2 | Confirmado | El contrato del proveedor de saldo no expresa capacidades y fuerza tratamientos especiales por nombre |
| E20 | Economía | P3 | Confirmado; aspectos potenciales | El binding/estilo de la vista BTC conserva repetición y una posible dependencia del autobinding en la primera apertura |
| E21 | Economía | P2 | Potencial | Los singletons del proveedor y del fetcher no tienen un reset efectivo consumido al recrear misión |
| D01 | Dispositivos | P1 | Confirmado | El horno puede reiniciar indefinidamente el plazo de consumo al apagar y encender |
| D02 | Dispositivos | P1 | Confirmado | La restauración de calor del horno ocurre antes de crear su fuente térmica |
| D03 | Dispositivos | P1 | Confirmado | La cuantización de batería altera la contabilidad energética autoritativa |
| D04 | Dispositivos | P1 | Confirmado | El intercom intenta crear una clase PAS que no está declarada en config |
| D05 | Dispositivos | P1 | Confirmado | El BatteryAdapter desplegado no tiene ninguna vía de recuperación coherente |
| D06 | Dispositivos | P2 | Potencial | Desmontaje y despliegue convierten salud del dispositivo en un objeto nuevo |
| D07 | Dispositivos | P2 | Confirmado | Las mejoras reconstruyen los sobrantes y el filtro perdiendo estado del objeto |
| D08 | Dispositivos | P2 | Confirmado | La lista remota de emparejamientos no se sincroniza al cambiar de propietario |
| D09 | Dispositivos | P2 | Potencial | Callbacks diferidos de mando y MemoryCell no se cancelan ni se coalescen |
| D10 | Dispositivos | P2 | Confirmado | El fast path de DeviceAPI no se utiliza para varios accesos frecuentes a puertos |
| D11 | Dispositivos | P2 | Confirmado | Se reescriben LEDs y animaciones aunque no cambie su estado visual |
| D12 | Dispositivos | P2 | Confirmado | Intercom RF copia y recorre todo el registro para una búsqueda local |
| D13 | Dispositivos | P2 | Confirmado | El backoff de puertas aún consulta todos los jugadores por cada controlador sin pareja |
| D14 | Dispositivos | P2 | Confirmado | La estimación de combustible ignora el modo whitelist y repite lecturas de config |
| D15 | Dispositivos | P2 | Potencial | La carga de persistencia acepta versiones y valores sin comprobar el contrato concreto |
| D16 | Dispositivos | P1 | Potencial | El controlador de puerta puede operar cierres ajenos sin verificar permiso de apertura |
| D17 | Dispositivos | P2 | Potencial | La identidad de grupo del sensor se basa en un nombre mutable |
| D18 | Dispositivos | P3 | Confirmado | Las familias de dispositivos duplican comportamiento compartible sin necesitar una jerarquía universal |
| D19 | Dispositivos | P3 | Confirmado | El generador de producción legacy vuelve a serializar wires en cada petición |
| D20 | Dispositivos | P2 | Confirmado | MemoryCell emite log de cada propagación aunque no cambie el estado |
| D21 | Dispositivos | P2 | Potencial | ResolveVanillaDevice usa un radio de aceptación diferente del declarado |
| D22 | Dispositivos | P3 | Potencial | El guard de holograma de la base no detiene inicialización añadida después de super |
| D23 | Dispositivos | P3 | Confirmado | La ordenación del mando realiza selección cuadrática en cada guardado |
| D24 | Dispositivos | P2 | Confirmado | El adaptador publica SyncVars de energía pero su API cliente no usa ese estado |
| A01 | Transversal | P2 | Confirmado | El detector de clases duplicadas confunde ramas diferentes con ramas mutuamente excluyentes |
| A02 | Transversal | P3 | Confirmado | El control BALANCE solo cuenta delimitadores y admite un orden inválido |
| A03 | Transversal | P1 | Confirmado | La cobertura automatizada actual valida el checker, no la lógica que se va a optimizar |
| A04 | Transversal | P3 | Confirmado | La CI descarga recursos binarios que el barrido no utiliza |
| A05 | Transversal | P2 | Confirmado | La telemetría evita el control de logging y duplica sus constantes |
| A06 | Transversal | P2 | Potencial | El límite configurable por dispositivo puede superar el contrato de 64 cables del cliente |
| A07 | Transversal | P3 | Confirmado | Las acciones de interacción repiten estructura que puede compartirse sin eliminar validaciones de servidor |
| A08 | Transversal | P3 | Confirmado | Los kits de config.cpp duplican propiedades comunes; una base oculta simplificaría el catálogo |
| A09 | Transversal | P3 | Confirmado | La configuración puede acortarse con helpers tipados de clamp/log y constructores de entradas |
| A10 | Transversal | P2 | Confirmado | Faltan contratos operativos y una línea base de rendimiento versionada |


## B.8 Secuencia de implementación que propone

## 8. Secuencia de implementación propuesta

| Entrega | Contenido | Tamaño relativo | Evidencia necesaria para avanzar |
| --- | --- | --- | --- |
| PR 1 | Destinatarios RPC y helper explícito; contador de mensajes por receptor. | Pequeña/media | Dos clientes, JIP y edición: mensaje correcto sin difusión involuntaria. |
| PR 2 | Permisos/puertos/capacidad prospectiva y commit de reemplazo de cable. | Media | Reemplazo autorizado, no autorizado y alta fallida sin borrar anterior. |
| PR 3 | Contrato de commit/abort/indeterminado en saldos y orden de Sell/Deposit. | Grande y delicada | Matriz de fallos y reinicio; conservación monetaria. |
| PR 4 | G01 y coherencia de alimentación/generación virtual. | Media | Red estable drena, batería autónoma sigue coherente. |
| PR 5 | Tiempo de horno, precisión de batería, PAS y recuperación del adapter. | Media, divisible por dispositivo | Pruebas de tiempo/energía/lifecycle independientes. |
| PR 6 | Parser/contrato/RPC del sorter y respuestas correlacionadas. | Media/grande | Round-trip, vacíos, límites, late responses y acceso. |
| PR 7 | Scheduler justo, snapshot de ventana y repack presupuestado. | Grande | Coste máximo acotado y turnos de todos bajo carga. |
| PR 8 | Limpieza/culling/presupuesto de geometría y oclusión. | Media | Viaje entre bases, cables largos, paredes y regreso. |
| PR 9 | Índices/deltas incrementales y mutación local de grafo. | Grande | Comparación contra reconstrucción completa como oráculo de estado. |
| PR 10 | Inventario/claims indexados y límites de entidades BTC. | Media/grande | Igual valor y selección compatible, menos trabajo/tx. |
| PR 11 | Consolidación UI TEST/normal, helpers/acciones/kits y código sin consumidores. | Varias pequeñas | Matriz de UI y contratos externos; compilar ambos lados. |
| PR 12 | CI selectiva, checker mejorado, documentación y benchmark mantenible. | Pequeña/media | Mismos textos inspeccionados, negativos nuevos detectados y receta reproducible. |

La instrumentación y las pruebas de PR 12 deben empezar al principio; la última entrega formaliza el conjunto. El orden separa optimizaciones equivalentes de cambios de integridad con más riesgo. No se ha implementado ninguna de estas PR ni cambiado el repositorio.

## 9. Límites, hipótesis y descartes relevantes

- El repositorio tiene presupuestos y límites reales. Los recuentos O(N²) se acompañan de frecuencia/cardinalidad; no prueban por sí solos que esa función domine un frame.
- «Cada frame/tick» se refiere al caller mostrado en la ficha, con sus guards; no se presupone que todos los dispositivos estén activos a la vez.
- La pausa de broadcasts de FullSync no equivale a parar toda la simulación eléctrica.
- La demanda flexible puede causar reencolado repetido, pero los presupuestos limitan cada callback. No se ha afirmado un bucle nativo infinito sin salida.
- Los campos/caminos sin consumidores dentro del repo pueden ser API para otros mods. Se exige búsqueda externa o de integración antes de retirarlos.
- Las acciones del motor pueden revalidar parte de los estados; las ausencias cuya explotabilidad depende de ello se mantienen como potenciales, sin demostrar ataques ejecutados.
- No se interpretó `test/` automáticamente como excluido: la variante está declarada/registrada. Tampoco se asumió que una clase denominada TestDevices sea necesariamente código muerto.
- Se descartó como reproducción ordinaria la idea de que poner MaxWiresPerDevice=128 baste para superar el contrato de inspección; SEC20 describe un camino distinto y acotado a cliente modificado.
- Las mejoras propuestas de ordenación deben usar semántica correcta de arrays. En Enforce `Remove` no es intercambiable con una extracción FIFO; se contrastó en la fuente oficial.
- No se auditó historial Git completo, binarios empaquetados, servidores en funcionamiento, credenciales desplegadas ni todos los mods dependientes. No haber reportado otra vulnerabilidad no demuestra su inexistencia.


---

# 9. Contrato de salida

Responde en castellano con **exactamente** estas secciones, en este orden:

## 1. CRÍTICA DEL ENCUADRE
Lo primero. ¿Qué está mal planteado? Cita el hecho (H1..H7) o la sección que refutas. Si crees que
el encuadre es correcto, dilo en una línea y sigue — pero solo después de haberlo buscado.

## 2. RESPUESTA A LA PREGUNTA
¿V3 antes o después de integridad? Una recomendación clara en las dos primeras líneas, y luego el
porqué. Si tu respuesta es «ninguna de las dos», dilo.

## 3. PLAN
La secuencia que propones, en tramos. Por cada tramo: qué entra, por qué va ahí, qué lo bloquea y
cuál es la señal observable de que está terminado. Sé concreto con ficheros y fichas: quien lo
implemente necesita saber qué tocar.

## 4. DESACUERDOS CON EL BORRADOR
Punto por punto, en qué te apartas de la sección 6 y por qué. Si coincides del todo, dilo.

## 5. RIESGOS QUE NADIE HA NOMBRADO
Lo que ni las auditorías ni el borrador vieron.

## 6. LO_NO_VERIFICADO
Todo lo que has necesitado suponer. Sin esta sección la respuesta no vale.
