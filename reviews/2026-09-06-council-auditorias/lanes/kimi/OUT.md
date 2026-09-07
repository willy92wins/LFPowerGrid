Leído el BRIEF. Antes de responder, verifico en el repo los hechos de los que depende mi decisión (modo lectura; el entregable será solo texto, como manda la lane).El shell está bloqueado por un hook del entorno (no puedo correr `git`; lo marco como no verificado). Los greps sí confirman varios hechos clave. Completo con tres lecturas dirigidas:He leído el BRIEF y verificado en el repo los hechos de los que depende mi respuesta (el shell quedó bloqueado por un hook del entorno —mismo síntoma que las dos lanes sin git del triage—, así que `git` va a LO_NO_VERIFICADO; todo lo demás lo confirmé con grep/lectura). Entregable de la lane, solo texto:

## 1. CRÍTICA DEL ENCUADRE

**a) La pregunta es un falso binario.** «¿V3 antes o después de los P1 de integridad?» empaqueta 25 P1 heterogéneos como si fueran un bloque. Solo los P1 del sorter (S04, S08) comparten ficheros con la jubilación. Los monetarios viven en ficheros que T0 no toca: E04 en `LFPG_BTCHelper.c:1465`, E16 en `LFPG_BalanceProvider_NativeImpl.c:1230-1243` + `LFPG_BTCAtm.c:165-176`, SEC09 en `LFPG_FileUtil.c`. Los de grafo viven en `LFPG_ElecGraphImpl.c`, fichero intacto desde `d61705e`. La respuesta correcta es por subsistema, no global. El borrador ya opera así de facto (sus tramos separan T0 de T1), pero la pregunta tal como se formula invita a una ordenación única que sería errónea si se toma al pie de la letra.

**b) La huella como objetivo está mal puesta, y falta una cifra que el borrador no cita.** El hueco de arena medido por el proyecto es **885 kB, no 355 kB** (invariante cerrada 2026-08-23, `LFPowerGrid_dev\HANDOFF.md:155-156`; el 355 está retractado en la misma nota). Con H6 (solo la estructura compilada rinde, confirmado en `HANDOFF.md:159-161`) y la ratio ~3 kB/clase, jubilar V3 (7 clases) rinde ~21 kB = **2,4% del hueco**. Como medida de huella es simbólica. La justificación que sobrevive al escrutinio es el impuesto del fork (S10: las UIs ya divergen), no la arena. Y si el dueño quiere hueco de verdad, falta el trabajo que ninguna auditoría hizo porque ninguna conocía el objetivo: **censo de arena por subsistema y borrado de clases muertas** — SEC18 (migrators huérfanos), E13 (compound actions sin uso), R22 (código muerto), D04 (clase PAS compilada, ocupando arena, sin estar en CfgVehicles). Eso probablemente rinde más kB/hora que todo el tramo T0.

**c) El pre-filtro del triage prueba menos de lo que dice en su dirección fuerte.** «Fichero intacto ⇒ ficha viva» solo vale si la cura tendría que tocar ese fichero. Para fichas de tipo «falta un llamador/notificación» (G04: `RefreshSourceState` con un único llamador; G18) la cura natural vive en **otro** fichero (añadir el llamador), y el fichero citado quedaría intacto con la ficha muerta. El «24 VIVO + 1 PARCIAL» puede contener falsos VIVO de este tipo —pocos, pero el método no los distingue—, y las 28 P2/P3 «vivas por construcción» heredan el mismo sesgo.

**d) H7 sostiene la mitad del plan y es el hecho más débil.** Ausencia de `publisherId` en `config.cpp` prueba que no hay Workshop publicado, no que no haya servidor: un servidor privado carga mods por `-mod=` local sin Workshop. El borrador lo trata como supuesto con inversión condicional; el encuadre correcto es convertirlo en **pregunta bloqueante al dueño antes de fijar el orden**, porque invierte T1.

**e) Dependencia cruzada no declarada: T1 no puede cerrarse sin T5.** A03 dice que la cobertura actual valida el checker, no la lógica. Declarar «conservación monetaria» en E04/E16/SEC09 sin harness de inyección de fallos es declarar a fe. El borrador pone T5 «en paralelo desde el día 1» pero no lo hace puerta de cierre de nada.

**f) El borrador omite 2 de las 25 P1 en todos sus tramos: R15 y D16** (recuento: T1=6, T2=3, T3=5, T4=8, T5=1 → 23 de 25). Lo desarrollo en §4.

**g) Menor:** la auditoría A examinó `c4ac943` y HEAD es `421cabb`; sus citas pueden haber derivado, y ella misma marca 6 ficheros TRUNCATED —incluidos los 4 grandes del fork—, así que el inventario de divergencias V3↔V4 está incompleto de origen.

## 2. RESPUESTA A LA PREGUNTA

**Ni «antes» ni «después» en bloque. Si se fuerza una respuesta única: ANTES — pero solo respecto a los P1 del sorter, y solo si el dueño confirma H7.**

Desglose por subsistema:

- **P1 del sorter (S04, S08): DESPUÉS de la consolidación**, para escribir el arreglo una sola vez. Ojo: jubilar V3 **no** arregla S08 — verificado por esta lane: `SORTER_TEST_REQUEST_SORT` y `SORTER_REQUEST_SORT` caen en el mismo `HandleSorterRequestSort` (`LFPG_RPCServerHandlerImpl.c:142-150`); la V4 hereda el pico. La jubilación solo deja un único sitio donde arreglarlo.
- **P1 monetarios (E04, E16, SEC09, E02, E03, E08): INDEPENDIENTES de la jubilación a nivel de fichero.** Su posición la decide H7, no el sorter: si hay jugadores, son lo primero absoluto; si no, pueden ir tras T0b o en paralelo si hay manos.
- **P1 de grafo, dispositivos y render: ortogonales**; cuando haya hueco de calendario.

Por qué «antes» para el sorter: el fork está **vivo y divergiendo** (S10; V4 en desarrollo activo — cada semana añade divergencia que habrá que reconciliar), mientras los P1 están **estables** (24/25 VIVO, latentes bajo H7). Se consolida primero lo que se degrada solo; se arregla después lo que no se mueve. El impuesto real no es «escribir 24 fichas dos veces» (la lógica es compartida) sino doble verificación y decisiones de paridad acumulándose.

## 3. PLAN

**P0 — Día 0, antes de todo (decisiones y medición; sin código):**
- Pregunta bloqueante al dueño: ¿hay servidor con jugadores? (decide T1 vs resto).
- Dos decisiones de producto que T0b necesita cerradas: paridad `powered/linked` de la acción TEST (audit A, P1-1) y dirección de la divergencia de preview S10 (qué comportamiento gana).
- Re-medir arena: H6 es de julio, pre-V4 (+10 clases desde entonces). Sin baseline antes/después, los ~21 kB del borrado quedan sin validar jamás.
- Inventario mecánico de divergencias del fork (diff V3↔V4 de View/Controller/acciones/handlers): audit A tuvo los 4 ficheros grandes TRUNCATED; S10 prueba que hay divergencias no catalogadas. Es el oráculo pre-borrado.
- Localizar `ef29b73` (arreglo SEC09): ¿en qué rama vive? Si está en `debt/v3-data-integrity`, T1 empieza mergeándolo, no reescribiéndolo.
- Señal de fin: 3 decisiones escritas + baseline de arena + lista de divergencias + rama de `ef29b73` identificada + línea de release elegida (main lleva 17 commits de retraso; `release/lfpg-footprint-rc` = `289592b`).

**T0a — Extracción pura de paleta (cero cambio de comportamiento):**
- Entra: mover `LFPG_ColorData` (verificado: `LFPG_SorterView.c:45`) y las 29 `COL_*` a fichero neutro (p. ej. `3_Game/LFPG_UIPalette.c`); repuntar las 76 refs del ATM (verificado: `LFPG_BTCAtmView.c` = 64, `LFPG_BTCAtmController.c` = 12) y las 13 de `LFPG_SorterView_TEST.c`.
- Por qué ahí: desbloquea el borrado de `LFPG_SorterView.c` y es el único paso seguro bajo cualquier valor de H7.
- Bloquea: nada. Señal: compila; diff mecánico; grep `LFPG_SorterView.COL_` fuera de V3 = 0. Sin gate in-game propio.

**T0b — Cierre de V4 y jubilación de V3:**
- Entra: resolver **todas** las divergencias del inventario P0 (incl. powered/linked y S10); emisor cliente de `SORTER_TEST_RESYNC` (65) — el servidor ya lo atiende con handler compartido (verificado: `LFPG_RPCServerHandlerImpl.c:157-160`); lo que falta es la acción TEST, hoy `LFPG_ActionSyncSorter.c:101` emite solo 29; productor de `SORTER_TEST_CARGO_REFRESH` (70) (verificado: solo `LFPG_Defines.c:336` + receptor `LFPG_RPCClientHandler.c:91`; `BroadcastCargoRefreshToNearby` fija 34 en `LFPG_NetworkManagerImpl.c:6316`); `IsOpen()` TEST en `LFPG_Actions.c:528` y `LFPG_ActionSyncSorter.c:69`; repuntar `MissionInit` (9 sitios) y `RPCClientHandler` (4 handlers); borrar las 7 clases V3 y los 3 layouts V3.
- Renombrado `_TEST`→definitivo **partido en dos**: (i) clases de UI/controlador — no están en `config.cpp` (verificado) — renombran libre, con grep de referencias stringly (paths de layout, `FindAnyWidget`); (ii) las entidades `LFPG_Sorter_TEST` / `LFPG_Sorter_TEST_Kit` **sí están en `config.cpp:1080,1086`** (verificado) — renombrarlas toca classnames visibles en persistencia; decisión explícita: conservar classname o migrar. **No** renombrar `LFPG_MCP_SorterCmd.layout` (guarda explícita de audit A §5).
- Alcance duro reformulado: «sin refactor **no relacionado**» (el cableado de 65/70 es código nuevo; la paridad es cambio de comportamiento decidido en P0).
- Bloquea: P0 completo. Señal: grep cero de símbolos V3; compila; **gate in-game batch #1** (panel: abrir/cerrar/ESC/muerte/inconsciencia; sort/save/preview; ATM con paleta neutra; dual-open).

**T1 — Integridad monetaria (E04, E16, SEC09 residual, E02, E03, E08):**
- Posición: H7 falso → inmediatamente después de T0a, antes que T0b (no comparte ficheros con T0). H7 cierto → después de T0b.
- Invariante nombrada (R7): *ningún camino de error monetario deja RAM, disco y hive incoherentes*; grep de todos los call-sites de AddBalance/DepositCash/Sell/Save antes de tocar nada. Ojo E15: la reconciliación de arranque asume el orden commit/abort actual.
- Bloquea: T5-harness (no se declara sin inyección de fallos) + merge previo de `ef29b73`. Señal: matriz de fallos con inyección + reinicio entre cada par de I/O (crash recovery), en verde.

**T2 — Autoridad de servidor (SEC02, SEC20, SEC01):** SEC01 incluye el recuento de entregas por cliente (pendiente de ejecución) → gate batch. Va después de T0b por fricción de merge en `RPCServerHandlerImpl.c`/`RPCClientHandler.c`, no por semántica.

**T3 — Dispositivos (D04, D05, D01, D02, D03, D16):** D04 tiene doble lectura: declarar la clase en CfgVehicles (fix funcional) o borrar el código muerto (fix de arena) — decidir en P0. D16 necesita ejecución (¿`Fence.OpenFence()` respeta candado?) → gate batch.

**T4 — Grafo, render y coste de sorter (G01, G02, G04, G18, R02, R04, R15, S04, S08):** añado **R15**, ausente de todos los tramos del borrador (timeout CCTV de 5 s sin confirmar restauración → verificación in-game). G02 (duración de la incoherencia) se mide en el mismo gate. S04/S08 aquí, sobre código ya único.

**T5 — Instrumento (A03 + baseline §7 + harness de inyección + re-medición arena):** en paralelo desde el día 1, pero **con dientes: puerta de cierre de T1 y T2**, no acompañamiento. Si el checker se va a usar como señal, incluir sus falsos negativos (A01/A02).

**T6 — (Nuevo, condicional) Censo de arena y clases muertas:** si el objetivo real es huella: censo por subsistema + borrado de muertos (SEC18, E13, R22, D04-código). Ninguna auditoría lo hizo porque ninguna conocía el objetivo.

**Gates in-game batchados** (cada test cuesta 3-10 min y vale por todos los cambios pendientes): gate #1 tras T0b; gate #2 tras T1–T4 con la batería pendiente de ejecución (D16, G02, SEC01-recuento, R15) + crash recovery monetario.

## 4. DESACUERDOS CON EL BORRADOR

1. **§F: «T0 y T1 tocan el mismo subsistema; hacerlos por separado significa tocar el ATM dos veces»** — falso a nivel de fichero con las citas del propio brief: T0a toca `LFPG_BTCAtmView.c`/`LFPG_BTCAtmController.c`; T1 toca `LFPG_BTCHelper.c`, `LFPG_BalanceProvider_NativeImpl.c`, `LFPG_FileUtil.c`, `LFPG_BTCAtm.c`. Solape: **cero ficheros**. La justificación real de T0a primero es otra (desbloquea el borrado de `LFPG_SorterView.c` y es seguro bajo cualquier H7), no la doble visita al ATM.
2. **Omite R15 y D16**: 2 de las 25 P1 no aparecen en ningún tramo (23 de 25 programadas). Las programo en T4 y T3 con verificación in-game.
3. **«24 fichas del sorter que habría que escribir dos veces»** — inflado. El fork es UI/acciones/handlers cliente; la lógica es compartida (verificado: ambos subIds de sort caen al mismo handler, `LFPG_RPCServerHandlerImpl.c:142-150`). El impuesto real es doble **verificación** + divergencias de UI (S10, S20–S24), no 24 reescrituras. La conclusión (consolidar primero) se mantiene, pero por la razón estrecha.
4. **«T0 sin refactor» es incumplible tal cual**: cablear 65/70 es código nuevo; la paridad powered/linked es cambio de comportamiento; el renombrado toca strings que el compilador no ve. Lo reformulo como «sin refactor no relacionado» y lo muevo a decisiones P0.
5. **T5 «en paralelo» sin dientes** → yo lo hago puerta de cierre de T1/T2.
6. **Faltan**: la medición arena antes/después, el inventario de divergencias del fork (borrar V3 sin él fosiliza divergencias no detectadas), y el **plan de ramas** (4 vivas, main 17 detrás, `ef29b73` sin localizar).
7. **Coincido en**: T0a como primer paso universal; T1 no primero bajo H7, con inversión si es falso; S04/S08 no se arreglan jubilando; T5 desde el día 1.

## 5. RIESGOS QUE NADIE HA NOMBRADO

1. **Renombrado de entidades config-bound**: `LFPG_Sorter_TEST` y `LFPG_Sorter_TEST_Kit` existen en `config.cpp:1080,1086` (verificado). Borrar/renombrar classnames rompe la persistencia de cualquier mundo de prueba que los tenga colocados (clase desconocida al cargar). El borrador dice «renombrar `_TEST` → nombres definitivos» sin esta distinción, y audit A prohíbe explícitamente tocar el layout MCP.
2. **Borrar V3 elimina el oráculo**: hoy cualquier duda de paridad se resuelve comparando con V3 en vivo. Tras el borrado, las divergencias no catalogadas (audit A: los 4 ficheros grandes del fork llegaron TRUNCATED) se fosilizan. El inventario de divergencias de P0 es precaución irreversible, no opcional.
3. **H6 es de julio, pre-V4**: la decisión de huella se toma con la medición vieja; V4 añadió 10 clases desde entonces. Sin re-medición no se sabe si el hueco sigue siendo 885 kB.
4. **La paleta nueva resta**: `LFPG_UIPalette.c` sería +1 clase (~3 kB por la ratio A2) contra los ~21 kB del borrado → neto ~18 kB, no 21. Menor, pero el borrador da la cifra bruta como neta.
5. **E15 es invariante oculta de T1**: «ocho compras a cuenta bloquean más compras hasta reconciliación de arranque» significa que el arranque **asume** el orden commit/abort actual. Reordenar Sell/Deposit sin tocar la reconciliación puede convertir un bloqueo temporal en pérdida.
6. **Fichas «VIVO (intacto)» potencialmente muertas por vía externa** (§1.c): barato de descartar con una lectura dirigida extra solo para las de tipo «falta llamador» (G04, G18) antes de asignarles tramo.
7. **El coste de no hacer nada de huella no está medido**: si 885 kB de hueco es holgado para la roadmap, el eje «huella» del plan es trabajo opcional y conviene decirlo en voz alta: los drivers reales son mantenibilidad e integridad.

## 6. LO_NO_VERIFICADO

- **H7** (no hay servidor con jugadores): no verificable desde el repo; ausencia de `publisherId` no prueba ausencia de despliegue por `-mod=`. Pregunta al dueño.
- **Ubicación de `ef29b73`** (arreglo SEC09): el shell de mi entorno quedó bloqueado por un hook (mismo síntoma que las dos lanes sin git del triage). No confirmé si está en HEAD o en `debt/v3-data-integrity`.
- **Dirección de la divergencia S10** (qué UI protege preview y cuál no): el detalle de ficha no está en el brief.
- **D16** (candado en `Fence.OpenFence()`), **G02** (duración de la incoherencia), **SEC01** (recuento de entregas), **R15** (restauración CCTV): requieren ejecución in-game.
- **Ratio ~3 kB/clase aplicada a las 7 clases V3**: proyección, no medida sobre este borrado; y desconozco si las `static const int` consumen arena por sí mismas (afecta a si la extracción de paleta es neutra en huella).
- **Las 126 fichas P2/P3 sin triar** (28 «vivas por construcción» con el sesgo de §1.c; 98 desconocidas) y **las 7 P1 que el triage dejó «en curso»** (H3) — asumo que la tabla §4 es el estado final.
- **Herramientas externas al árbol**: verifiqué 0 hits de `LFPG_SorterView`/`LFPG_SorterController` en `.py` del repo, pero `sprint0_clone.py` (mencionado en audit A) no lo localicé; puede vivir fuera del árbol.
- **Deriva de citas de audit A** (`c4ac943` vs HEAD `421cabb`).
- Mi lectura de `LFPG_FileUtil.c:205-224` confirma el flujo tmp/bak pero no reproduje el residuo SEC09 completo (`:280-283`); lo tomo del triage.

**Verificado por esta lane en el repo** (no estaba o estaba incompleto en el brief): hueco de arena 885 kB y H6 (`LFPowerGrid_dev\HANDOFF.md:155-161`); bloqueador (a) — 65 sin emisor y 70 sin productor, con el matiz de que el servidor **sí** atiende 65/70 con handlers compartidos (`LFPG_RPCServerHandlerImpl.c:147-160`), falta solo el lado cliente; `LFPG_ColorData` en `LFPG_SorterView.c:45`; 76 refs `COL_*` en el ATM (64+12, cifra exacta del censo); entidades TEST en `config.cpp:1080,1086`.