# PLAN-CONSOLIDADO v3 — señal lógica separada del flujo (puertas y Laser -> Set detrás de un Splitter, 1.3.1)

Estado: v3, tras la R22 de la v2 (`REVIEW-grok-plan-v2.md`, APTO CON CAMBIOS, y
`REVIEW-codex-plan-v2.md`, NO APTO por un pulso falso que no pasa por 00). Cambios frente a la v2:
§0, §2.1, §3.1, §4, §5.7-5.8, §6.3-6.6, §7, §8, §9, §10 y §11.2. La v2 queda como
`PLAN-CONSOLIDADO-v2.md` y la v1 como `PLAN-CONSOLIDADO-v1.md`. Base `main` 592ebcc, rama
`claude/project-thread-0iolln`, PR #98 en borrador. Si algo choca con `AGENTS.md`, manda `AGENTS.md`
y se dice en el informe. Las líneas citadas son de 592ebcc (los commits posteriores solo añaden
documentos).

## 0. La decisión en una frase

Las entradas de señal de las puertas (AND/OR/XOR `input_0` e `input_1`) y de la Memory Cell
(Toggle `input_1`, Reset `input_2`, Set `input_3`) dejan de leer el flujo asignado y pasan a leer un
booleano por arista, `m_SignalHigh`, que el grafo propaga junto al reparto **sin cambiar el reparto**.
Power (`input_0`) de la Memory Cell sigue leyendo flujo. Las puertas publican en su salida la lógica de
las entradas que leen **en el momento de publicar** (`LFPG_EvaluateGateLogic` sobre la señal de
`input_0` e `input_1`), no su latch.

Qué se toma de cada plan:

- **Codex:** la dirección (canal de señal), el contrato de señal, la causa de A4 y la mitad de D5
  que explica por qué no se recupera, la tabla de mutantes, los límites de convergencia y, tras las
  R22, quitar la tolerancia de un apagón de las puertas y publicar la lógica actual de la puerta.
- **Grok:** la causa de A3 (coincide con Codex), el porqué del control sin Splitter (SOURCE con 50
  fijos frente a PASSTHROUGH con el input de la época), la mitad estática de D5 (oferta 5 - 5 = 0 y
  exclusión del vector), la fidelidad del arnés (el `QueueModel` actual no aplica la sonda de
  compuerta cerrada) y el control negativo sin Splitter.
- **Descartado de Grok:** el reparto parcial en PASSTHROUGH (su parche 1) y el +1 u/s del Laser
  abierto (su parche 2). Motivos en §5.

## 1. Causas confirmadas

### 1.1 Mecanismo común

Las entradas lógicas leen flujo: `LFPG_LogicGate.c:141-142` y `LFPG_MemoryCell.c:205-208` llaman a
`IsPortReceivingPower`, que devuelve `edge.m_AllocatedPower > LFPG_PROPAGATION_EPSILON`
(`LFPG_ElecGraphImpl.c:5013-5048`, `:5044`). El flujo depende de la demanda, y la demanda depende del
latch de la propia puerta: sonda `max(1, autoconsumo)` con la compuerta cerrada (`:2658-2671`),
demanda real aguas abajo con la compuerta abierta (`:2549-2631`). Cualquier cero de flujo, pasajero o
estructural, vuelve a la lógica. Pasa de tres maneras:

### 1.2 A1-A3 con Splitter: all-off pasajero durante la subida (las dos lanes)

- Al abrir la puerta su demanda salta de la sonda (1) a la carga (10) (`:2575`, publicada en `:2789`).
- Cada PASSTHROUGH entre el generador y la puerta (interruptor, Splitter) compara esa demanda con su
  `availableOutput`, que es el input **de esta época** (`:2519`, pasado en `:2525`), y si no le llega
  pone todas sus salidas a 0 (`:4734-4735`, bucle `:4749-4798`, `if (!overloaded)` en `:4763`).
- Con Splitter salen **dos** lecturas oscuras seguidas (la pasada del interruptor y, tras el all-off
  del Splitter, el `inputChanged` del interruptor). La tolerancia de `LFPG_LogicGate.c:164-189` solo
  cubre una: la puerta cierra, vuelve la sonda, reabre y el ciclo sigue. Paso a paso en Codex §4.2 y
  Grok «Circuito A3»; casa con el log de `GRAFO-SPLITTER-MEDICIONES.md` (`spl[p1 i11 o2]`).
- Sin Splitter el proveedor de los interruptores es un SOURCE con 50 fijos (`:2366-2372`): una sola
  lectura oscura, que la tolerancia absorbe. Por eso el control pasa.
- El water-fill **no** es la causa de A3: con B cerrado, B queda fuera del vector y A conserva los 10.

### 1.3 A4 (puerta sin carga): demanda cero con la compuerta abierta (Codex; Grok no lo analiza)

Abierta, sin cable de salida y con autoconsumo 0, la puerta publica 0 (`:2614-2629`). El interruptor
le asigna 0, la entrada lee 0 dos veces, cierra, vuelve la sonda, abre. Por lectura **no depende del
Splitter**. No está medido sin Splitter: entra en la fase 3 como diagnóstico.

### 1.4 D5 (Laser -> Set): el flujo de Set depende de cómo se reparte la demanda de la celda

- Mitad estática (Grok): los demás proveedores dejan fuera al Laser porque su suministro es
  `5 - 5 = 0` (`PassthroughSupplyAt` `:4022-4034`, `IncludeInMergerVector` `:4009-4019`), y Power
  cubre toda la demanda de la celda.
- Mitad dinámica (Codex §5.2-5.3): en su propia pasada el Laser sí entra (`fromSelf`), pero en la
  primera pasada abierta su base de oferta usa el `m_GateClosed` cacheado, que aún vale cerrado
  (`ProviderOfferBase` `:4225` -> `ComputeOfferBasePassthrough` `:4074-4092`; `m_GateClosed` se
  actualiza después de `AllocateOutput`, `:2640-2652`). Pide 0 hacia Set. La transición marca aguas
  arriba (`:2688-2706`), el generador reasigna sin cambios y nada garantiza otra pasada del Laser.
- Ninguna de las dos mitades hace falta arreglarla: con la señal, Set lee la compuerta viva del Laser.
  Las dos van al bug-ledger (§9).

### 1.5 Erratas del brief

- `~4015` es `IncludeInMergerVector` (`:4009-4019`); `ComputeOfferBasePassthrough` está en `:4074`.
- TA3-01 no son dos umbrales: los dos usan 0.001; cambian los predicados (nivel en `:5044`, delta en
  `:4788-4794`). No se toca.
- El filtro de 400 ms es solo de Power y Toggle (`LFPG_MemoryCell.c:213-226`); Set y Reset son de
  nivel (`:235-242`).

## 2. Diseño

### 2.1 Contrato de la señal

`m_SignalHigh` dice «esta línea está viva». Lo escribe el proveedor en `AllocateOutput`, en cada
arista de salida habilitada, sin mirar demanda, asignación ni sobrecarga.

| Proveedor (`node.m_DeviceType`) | `signalOut` |
|---|---|
| SOURCE | `newOutput > EPS` (encendido y con capacidad; `:2366-2372`) |
| PASSTHROUGH sin autoconsumo | (alguna arista de entrada habilitada con señal, o `m_VirtualGeneration > EPS`) y compuerta abierta |
| PASSTHROUGH con autoconsumo | lo anterior, y `newPowered` (su entrada real cubre su autoconsumo, `:2379-2399`) |
| Compuerta cerrada | baja |
| Puerta lógica: la entidad que resuelve el pre-gate es `LFPG_LogicGateBase` | `LFPG_EvaluateGateLogic(señal de input_0, señal de input_1)`, leídas en ese momento con `IsPortReceivingSignal`; no usa el latch ni `signalIn`. Si la entidad no se resuelve, la fila de PASSTHROUGH con el latch cacheado |

- La compuerta es el `gateIsClosed` **leído en vivo** en el pre-gate (`:2483-2509`), no
  `node.m_GateClosed`. Si la entidad no se resuelve, el pre-gate ya copia el valor cacheado.
- **Nuevo en v3 (Codex R22-V2-01):** el latch de una puerta (`m_GateOpen`, que devuelve
  `LFPG_IsGateOpen`, `LFPG_LogicGate.c:125-128`) es el de su lectura anterior, porque el callback que
  lo actualiza corre en Step 5, después de `AllocateOutput` (`:2525`). Con `signalIn` y ese latch, una
  puerta cuyas entradas pasan de 11 a 10 (AND) o de 10 a 11 (XOR) publica un alto durante una pasada,
  y una Memory Cell en Set lo captura. Por eso la puerta publica la lógica de lo que lee en ese
  momento. El reparto sigue usando el latch vivo, como hoy. En Step 5 el callback lee las mismas
  aristas sin que se haya procesado otro nodo entre medias, así que el latch queda igual a lo
  publicado.
- «SOURCE» es `LFPG_DeviceType.SOURCE`, nunca `LFPG_IsSource()` (puertas e interruptores devuelven
  true y son PASSTHROUGH).
- La señal de entrada se lee del bit de la arista, **no** a través de `GetEdgeAllocatedPower`, que
  devuelve 0 con el proveedor en sobrecarga (`:4942-4944`).
- Aristas deshabilitadas: el proveedor no las escribe y los lectores las saltan, igual que con la
  asignación (`:5083-5089`).
- **Nuevo en v2:** cuando el flag de habilitación de una arista cambia, en cualquier sentido,
  `SetOutputPortEnabled` pone su `m_SignalHigh` a false (`:5075-5098`). La asignación sigue sin
  tocarse, como manda la nota v3.1 de `:5084-5091`. Una salida rehabilitada no puede enseñar el bit de
  antes de deshabilitarse; vuelve a alto solo cuando su proveedor la escribe (Codex R22-01, Grok
  R22-04).
- Arista nueva: `false` en el constructor. Se reconstruye por propagación. Ni se guarda ni se envía.
- Baterías: su `m_VirtualGeneration` sale de la carga y del permiso de descarga, no de la demanda
  (`LFPG_NetworkManagerImpl.c:6313-6322`), así que una batería con carga da señal sin oscilar.

Consecuencias, declaradas (criterio de producto, también en la fase 3):

- Señal alta no certifica energía. Una puerta puede estar abierta con asignación 0 (A4), un Laser
  puede dar Set sin financiar la celda (D5) y una lámpara sigue necesitando sus 10 reales.
- **Con un generador sobrecargado (all-off, B5) la lógica sigue viva**: las señales siguen altas, los
  LEDs de las puertas muestran los interruptores, una puerta puede abrir y cerrar y una Memory Cell
  puede ponerse o quitarse por Set y Reset. Toggle solo si a Power le llega flujo, es decir, desde una
  fuente que no esté en sobrecarga. Los cables siguen en CRITICAL y aguas abajo no llega energía. Se queda así a propósito: atar la señal a
  la sobrecarga (Grok R22-02) vuelve a acoplar lógica y reparto, que es lo que este arreglo separa;
  hoy, con una carga detrás de una puerta, la sobrecarga cierra y reabre la puerta en bucle.
- Invariante que se prueba: en estado estable, `m_AllocatedPower > EPS` implica `m_SignalHigh` en toda
  arista habilitada. Señal sin flujo vale; flujo sin señal es un fallo.

### 2.2 Quién lee qué

| Lector | Puerto | Lee |
|---|---|---|
| AND/OR/XOR | `input_0`, `input_1` | señal |
| Memory Cell | Toggle `input_1`, Reset `input_2`, Set `input_3` | señal |
| Memory Cell | Power `input_0` | flujo, sin cambios |
| Electronic Counter, consumidores, `VerifyPassthroughPowered` | todos | sin cambios |

## 3. Cambios, cada uno trazado a §1 o a §11

### 3.1 Producción, cinco ficheros

| Fichero:ancla (592ebcc) | Cambio | Causa |
|---|---|---|
| `3_Game/LFPG_Data.c:325-356` (`LFPG_ElecEdge`) | `bool m_SignalHigh;` y `m_SignalHigh = false;` en el constructor, con el comentario «runtime only» como `m_OfferedResidual` (`:340-343`) | llevar la señal sin tocar asignación ni oferta |
| `4_World/LFPG_ElecGraph.c:305-311` | `bool IsPortReceivingSignal(string deviceId, string portName)` en la base, con el mismo fallback de error y `return false` | que lo vean las puertas y la celda (orden de módulos) |
| `5_Mission/LFPG_ElecGraphImpl.c:81`, `:179`, `:2298` | `protected bool m_SignalChanged;`, a false en el constructor y al empezar cada nodo junto a `m_AllocChanged = false;` | ver cambios de señal aunque todos los floats sigan iguales (A4, cadenas sin flujo) |
| `:2308-2349` (Step 1) | `bool signalIn = false;` junto a `inputSum`, y tras el filtro `LFPG_EDGE_ENABLED`: `if (inEdge.m_SignalHigh) signalIn = true;`, leyendo el bit directamente | propagar la línea viva por tramos sin flujo |
| helper nuevo junto a `:4009-4034` | `protected bool ComputeSignalOut(bool isSource, float newOutput, bool signalIn, float virt, float cons, bool powered, bool gateClosed)` con la tabla de §2.1; solo `if/else`, bool y float, sin bucles ni enums, para que `enforce_scalar_slice` lo ejecute | el contrato en un sitio y probado offline |
| `:2516-2525` (Step 2b) | calcular `signalOut` con `ComputeSignalOut` (`gateIsClosed` vivo, `newPowered`, `signalIn`). Después, si el pre-gate resolvió la entidad (`gateEntityResolved`, `resolvedEnt` de `:2487`) y `LFPG_LogicGateBase.Cast(resolvedEnt)` no es null: `signalOut = LFPG_EvaluateGateLogic(IsPortReceivingSignal(nodeId, "input_0"), IsPortReceivingSignal(nodeId, "input_1"))`, con los nombres de puerto en locales `string` como en `LFPG_LogicGate.c:139-140`. Ni `gateIsClosed` ni `allocAvail` cambian. Luego `AllocateOutput(nodeId, allocAvail, signalOut)` (única llamada, verificado por las dos R22) | D5: publicar con la compuerta viva; Codex R22-V2-01: la puerta publica su lógica actual |
| `:4562` y bucle Pass 2 `:4749-4798` | parámetro `bool signalOut`; tras el filtro `LFPG_EDGE_ENABLED` y **fuera** de `if (!overloaded)`: si `allocEdge.m_SignalHigh != signalOut`, `m_SignalChanged = true`; luego `allocEdge.m_SignalHigh = signalOut;` | que la señal sobreviva al all-off (A3) |
| `:2786` y `:2815` | añadir `\|\| m_SignalChanged` a la condición de Step 3 y a la del reproceso en la misma época | que el lector se entere, acotado por `m_RequeueCount` |
| junto a `:5013-5048` | `override bool IsPortReceivingSignal(...)`: copia de `IsPortReceivingPower` (contabilidad de visitas, `LFPG_EDGE_ENABLED`, puerto) que mira `edge.m_SignalHigh` | la lectura |
| `:5075-5098` (`SetOutputPortEnabled`) | en las dos ramas que cambian el flag (`:5076-5080` y `:5081-5092`), `edge.m_SignalHigh = false;`. Nada más: ni la asignación ni la marca de topología cambian | Codex R22-01 / Grok R22-04: bit viejo al rehabilitar |
| `4_World/LFPG_LogicGate.c:136-143` | `LFPG_ElecGraph graph = nm.GetGraph(); if (graph) { newIn0 = graph.IsPortReceivingSignal(...); newIn1 = ...; }`, el patrón de `LFPG_MemoryCell.c:411-422` | A1-A4, B1-B2 |
| `LFPG_LogicGate.c:80-84`, `:148-189` | **quitar la tolerancia de un apagón:** fuera `m_BrownoutHeld` y su comentario (`:80-84`), fuera `totalBrownout`/`holdLatch` (`:166-184`), y `:186` queda `if (m_GateOpen != newGate)`. El comentario de `:148-163` se cambia por uno corto: las entradas se leen por señal, que no se apaga durante una subida del reparto, así que el latch sigue a la lógica en cada lectura | Grok R22-01 (puerta abierta con 00 sin segunda visita) y Codex R22-02 (pulso falso al salir de 00) |
| `4_World/LFPG_MemoryCell.c:197-208` | Power sigue con `nm.IsPortReceivingPower`; Toggle, Reset y Set con `graph.IsPortReceivingSignal` | D5; C1-C7 conservan sus filtros |

### 3.2 Tests y CI

- Nuevo `.github/tools/test_graph_logic_signals.py` (§6), que además comprueba que el workflow lo
  corre (patrón de `test_graph_perf_probe.py:198-199`).
- `.github/workflows/checks.yml`: un paso más que lo ejecuta.
- `.github/tools/test_memorycell_toggle.py`: el `NM` falso (`:117-127`) gana `GetGraph()`, que
  devuelve un objeto cuyo `IsPortReceivingSignal` lee el mismo diccionario `ports`. **Ninguna
  expectativa cambia.** El mutante `test_swapped_power_toggle_ports_break_f1` (`:507-514`) sigue
  aplicando porque la línea de Power no cambia.

## 4. Qué no se toca

- Energía: fórmulas de Pass 1, 2 y 3 de `AllocateOutput`, all-off (`:4734-4735`),
  `ApplyMergerWaterFill`, `IncludeInMergerVector`, `PassthroughSupplyAt`, `ComputeOfferBase*`,
  `PublishEdgeOffers`, `GetEdgeAllocatedPower`, `m_LastStableOutput`, `m_DemandKnown`, cold-start,
  sonda de compuerta cerrada (`:2658-2671`), generación virtual, demanda soft.
- `SetOutputPortEnabled` salvo la línea de §3.1: la asignación de una arista deshabilitada se sigue
  conservando (`:5084-5091`).
- El orden de actualización de `m_GateClosed` (`:2640-2652`) y `RefreshSourceState` (`:3724-3769`).
- `IsPortReceivingPower`, `LFPG_PROPAGATION_EPSILON`, el criterio de `m_AllocChanged` (TA3-01).
- `RequestPropagate`, SyncVars y su orden. `m_Input0Powered` y `m_Input1Powered` conservan nombre y
  orden; pasan a llevar la señal (LEDs). `m_GateOpen` sigue sin sincronizarse ni guardarse.
- `LFPG_EvaluateGateLogic` y sus overrides de AND, OR y XOR: ya es público en la base
  (`LFPG_LogicGate.c:119-122`) y el grafo solo lo llama.
- Memory Cell: 400 ms, rearme de 1000 ms, gracia de 30 s, prioridad Set/Reset, consumo de 1 u/s y
  routing.
- Laser, PressurePad, PushButton, Electronic Counter, Door Controller.
- `config.cpp`, classnames, persistencia, RPC, layouts, stringtable.

## 5. Decisiones sobre lo que los planes no comparten

1. **A3: canal de señal, no reparto parcial.**
   - En orden estable, el reparto parcial deja sin nada a una entrada de AND. AND (1,1) detrás de
     Splitter: la demanda 10 se parte 5/5, el Splitter tiene input 2 (las dos sondas) y da 2 a la
     primera arista y 0 a la segunda, la puerta lee (1,0) y cierra.
   - El prorrateo lo evita, pero cambia el reparto de todos los PASSTHROUGH en cada subida, que es lo
     que el brief protege (fixtures y B5 cerca del límite).
   - No arregla A4: ahí la demanda es 0 y no hay sobrecarga.
   - Su criterio de «subida» frente a «sobrecarga real» depende de ofertas que también pueden estar
     atrasadas.
2. **D5:** la causa son las dos mitades de §1.4. El arreglo no las necesita; van al bug-ledger.
3. **A4:** es otro mecanismo y el mismo canal lo arregla.
4. **Flujo por Set:** no se fuerza. El +1 u/s de Grok cuesta energía, sube la demanda cerca del
   límite y con la señal sobra.
5. **Tolerancia de un apagón: se quita (cambia respecto a la v1).** La v1 decía que el latch retenido
   en (0,0) no se veía. Las dos R22 lo tumban:
   - Grok R22-01: con señal ya no hay all-off que vuelva a visitar la puerta. XOR en 10, se apaga A:
     una sola lectura (0,0), `holdLatch` deja el latch abierto, `RequestPropagate` solo salta con
     `gateChanged` (`:217`) y la puerta se queda abierta con 00. B1 termina así, y AND 11 -> 00 en la
     misma época también.
   - Codex R22-02: AND en 11, luego 00 retenido, luego 10. En esa pasada el pre-gate aún ve la
     compuerta abierta y `signalIn` es verdadero, así que publica señal alta antes de que el callback
     cierre; una Memory Cell en Set la captura y se queda activada. Testigo de Codex con callbacks
     reales: AND acaba false y la celda true; sin la tolerancia, la celda queda false.
   - Sin la tolerancia, un latch abierto implica que en la lectura anterior había una entrada alta. Si
     las dos bajan, esa misma pasada publica señal baja (`signalIn` falso) y el callback cierra, así
     que no queda ningún estado «abierto con 00» del que pueda salir un alto falso. Desde la v3 la
     puerta además publica su lógica actual y no el latch (§5.7), que cierra los altos falsos que no
     pasan por 00.
   - Lo que la tolerancia absorbía y deja de absorber: una puerta procesada antes que sus proveedores
     justo tras reconstruir aristas (nacen sin señal). Ahora cierra y reabre al llegar la señal, en la
     misma época o la siguiente. Fixture de arranque en §6.4.
6. **B5 (Grok R22-02): se queda como está y se declara**, por lo dicho en §2.1.
7. **Puertas: lógica actual, no latch (Codex R22-V2-01, BLOQUEA en la v2).**
   - Escenario: G 50 -> Splitter -> interruptores A y B -> AND en 11, abierto y sin cable de salida; M
     desactivada, con Power aparte de 1. En el mismo tick se apaga B y se conecta AND -> M.Set (la
     arista nace con señal baja); cola [B, AND, M]. B publica baja. AND lee 10, pero su pre-gate ve el
     latch de 11 y, con `signalIn` verdadero, publica alto en la arista nueva; su callback cierra
     después. M lee Set alto y se queda activada. Testigo de Codex con callbacks reales: AND false y
     M true; publicando AND(1,0), M false.
   - Pasa también sin tocar cables: AND -> interruptor apagado -> M.Set, apagando B y encendiendo el
     interruptor en el mismo tick; o un XOR que va de 10 a 11.
   - Grok no lo vio porque solo miró los caminos que pasan por 00, y para esos el cierre de la v2 es
     correcto.
   - Descartado: reevaluar el latch en el pre-gate, porque cambia cuándo cierra la compuerta para el
     reparto y la energía no se toca. Descartado también un método nuevo en la entidad: sobra, porque
     `LFPG_EvaluateGateLogic` ya es público y el pre-gate ya resuelve la entidad.
   - Coste: dos lecturas de puerto más por pasada de puerta, las mismas que hace su callback.
8. **Salida rehabilitada: baja hasta la pasada de su proveedor (Codex, pregunta 2 de la v2).**
   - Limpiar el bit al rehabilitar (§2.1) alarga una bajada corta de esa salida hasta la siguiente
     pasada del proveedor: un tick con carga normal, más si los presupuestos la retrasan. Si la bajada
     pasa de los 400 ms del filtro, un Toggle que lea esa salida puede voltear donde hoy no voltea
     (hoy la asignación vieja se ve al instante, GL-05).
   - Se acepta y se declara. Copiar al rehabilitar la última señal del proveedor devuelve el alto falso
     de Codex R22-01 cuando el proveedor ha perdido la alimentación y aún no se ha reprocesado. Un alto
     falso deja una celda activada; un alto tardío solo llega tarde.
   - Fixture en §6.4.

## 6. Tests (R26)

### 6.1 Fichero, comandos y códigos

```
python -B .github/tools/test_graph_logic_signals.py
python -B -m unittest discover -s .github/tools -p "test_graph_*.py"
python -B .github/tools/test_memorycell_toggle.py
python -B .github/tools/test_pulse_survives_blackout.py
```

0 = todo pasa; 1 = fallo o excepción (el informe distingue los dos). Cero tests descubiertos o un
fallo de extracción es instrumentación inválida, no una reproducción.

### 6.2 Qué ejecuta del fuente real

- `LFPG_SetPowered` de `LFPG_LogicGateBase` y `LFPG_EvaluateGateLogic` de AND, OR y XOR, con la
  técnica de `test_memorycell_toggle.py:178-205`. El callback llama a lo que diga el fuente (hoy
  `nm.IsPortReceivingPower`; con el arreglo `graph.IsPortReceivingSignal`). El `nm` y el `graph`
  falsos **no deciden**: leen las aristas del modelo (asignación > EPS, o `m_SignalHigh`, con
  `LFPG_EDGE_ENABLED` y el puerto).
- `LFPG_SetPowered` de la Memory Cell y sus temporizadores, como en `test_memorycell_toggle.py`, con
  `LFPG_ApplyRouting` sustituido por lo que hace en `LFPG_MemoryCell.c:405-422` (Output habilitada si
  `m_CellActive`, Inverted si no) y que pasa por el `SetOutputPortEnabled` del modelo, que imita
  `:5075-5098` incluida la línea nueva.
- Los helpers escalares que ya extrae `test_graph_multifeed_split.py:28-44`, y `ComputeSignalOut` con
  el arreglo. En 592ebcc no existe: el modelo deja todas las señales en false, y los callbacks de la
  base tampoco las leen.

### 6.3 Modelo de PDQ: fidelidad exigida

Parte de `QueueModel` (`test_graph_multifeed_split.py:101-472`) y añade, citando la línea que imita:

- PASSTHROUGH con autoconsumo y `newPowered` (`:2379-2399`) y el tope de `m_MaxOutput`.
- `allocAvail = 0` con la compuerta cerrada (`:2519-2522`) y la demanda de compuerta cerrada
  `max(sonda, autoconsumo) + soft` (`:2658-2671`). Hoy falta (lo vio Grok).
- Demanda 0 con la compuerta abierta y sin carga (`:2614-2629`).
- La condición de Step 3 con sus cinco términos (`:2786`), más `m_SignalChanged` con el arreglo; la
  marca `DIRTY_INPUT` aguas abajo (`:2791-2826`) con el reproceso en la misma época (`:2815`); el
  reencolado aguas arriba de PASSTHROUGH (`:2840-2866`); el tope de 5 reencolados
  (`LFPG_Defines.c:494`) con paso a la época siguiente. El modelo actual propaga siempre aguas abajo:
  eso puede tapar el fallo.
- La transición de compuerta marca aguas arriba y pone `m_AllocChanged` (`:2688-2706`).
- Step 5: `SetPowered` de puertas y celdas salvo cuando `NodeEntitySyncUnchanged` lo salta
  (`:2966-3010`, llamado en `:3133`: sin `DIRTY_INPUT` ni topología y sin cambio de estado). Si el
  latch cambia, `RequestPropagate` del propio nodo.
- **Presupuestos (Codex R22-03 y R22-V2-02):** 64 nodos y 256 visitas de arista por llamada
  (`LFPG_Defines.c:465`, `:471`), el doble con más de 128 en cola y 128/512 en el warmup
  (`LFPG_NetworkManagerImpl.c:2446-2461`). Los dos se comprueban **antes de tomar cada nodo**
  (`LFPG_ElecGraphImpl.c:2250-2253`): un nodo empezado se termina aunque el contador pase de 256, así
  que una llamada puede acabar con más de 256 visitas. Un nodo que se salta por estar ya procesado en
  la época no cuenta como procesado (`:2266-2275`); uno que pasa el tope de reencolados sí cuenta
  (`:2278-2293`). Lo que no cabe se queda en la cola, en su orden. Las visitas se cuentan donde las
  cuenta el fuente.
- **Reloj por eventos, en ms (Grok R22v2-02; Codex, pregunta 3 de la v2):** el PDQ corre cada 100 ms
  (`LFPG_Defines.c:534`) y además en las llamadas directas, sin avance de reloj, del warmup y la
  reconstrucción (`LFPG_NetworkManagerImpl.c:865`, `:3158`, `:3722`). Cada `CallLater` dispara en su
  instante, también entre dos PDQ: el routing diferido de la Memory Cell a 50 ms
  (`LFPG_MemoryCell.c:262-269`), sus filtros de 400 ms y el rearme de 1000 ms. `g_Game.GetTime()`
  devuelve ese reloj. Si un temporizador y un PDQ caen en el mismo ms, se prueban los dos órdenes.
- **Barrido en vacío:** `ValidateConsumerStates` (`:3417`, llamado en `:2231` y `:2921` con la cola
  vacía, cada `LFPG_CONSUMER_VALIDATE_TICK_INTERVAL` = 10 ticks y en lotes de
  `LFPG_VALIDATE_BATCH_SIZE` = 32; `LFPG_Defines.c:514`, `:518`) corrige `m_Powered` por flujo, también
  en PASSTHROUGH, y llama `SyncNodeToEntity`, que dispara el callback de una puerta o de una celda. El
  modelo lo imita, porque puede tapar un mutante o provocar un cambio.
- La señal: `signalIn` en Step 1, `ComputeSignalOut` antes de `AllocateOutput`, la rama de puerta con
  el `LFPG_EvaluateGateLogic` real sobre las lecturas de `input_0` e `input_1`, escritura sin mirar la
  sobrecarga, `m_SignalChanged`, y la limpieza en `SetOutputPortEnabled`.
- Instrumentación: por cada pasada de nodo, el modelo anota qué disparadores de Step 3 se
  encendieron (`outputDelta`, `forceDownstream`, `m_AllocChanged`, `inputChanged`,
  `demandBecameKnown`, `m_SignalChanged`), para que un test pueda afirmar cuál movió la propagación.

Órdenes: (a) guionado, la tabla de Codex §4.2 para A3 y §5.2 para D5; (b) FIFO; (c) LIFO; (d) 20
permutaciones con semilla fija de la cola sucia. La base tiene que dar rojo al menos en (a); el
arreglo, verde en todos.

**Recorrido de la tabla (medida del PC en partida):** un XOR recién montado con solo A encendido
funciona; recorriendo la tabla, no. El rojo depende de la historia, así que A1-A3 se recorren en
secuencia sobre un mismo circuito, sin reconstruir entre pasos: 00 -> 10 -> 11 -> 01 -> 00 y
00 -> 01 -> 11 -> 10 -> 00. Un circuito nuevo por combinación queda solo como control.

Fin de cada transición: cola y diferidos vacíos, como mucho 100 ticks (agotarlos es FAIL); se cuentan
los cambios de latch. Los criterios de corte no dicen «en la misma época»: dicen «cuando la
propagación acotada ha llegado al lector», y se miden por las lecturas que hacen los callbacks. Las
aserciones leen el latch **y** los bits de las aristas después de vaciar la cola y los diferidos, no
un solo `SetPowered`.

### 6.4 Fixtures y resultado esperado

| Fixture | Hoy (592ebcc) | Con el arreglo | Muerde |
|---|---|---|---|
| A1-A3 con Splitter: AND, OR, XOR recorriendo la tabla en secuencia sobre un mismo circuito (§6.3); lámpara = CeilingLight PT, consumo 10, capacidad 50 | rojo en (a): XOR con 10 cierra tras dos visitas oscuras | latch = tabla, entrada de la lámpara >= 10 solo si la tabla da 1, como mucho 1 cambio de latch por combinación, en todos los órdenes | lectura por flujo |
| **Transiciones** con Splitter: 10 -> 00, 11 -> 00 (los dos en la misma época) y 11 -> 00 -> 10, con una Memory Cell en Set a la salida del AND (Power aparte de 1, sin carga) | se anota | tras cada paso el latch = tabla; la celda nunca se activa en 11 -> 00 -> 10 salvo por el 11 inicial, que se resetea antes; ningún alto transitorio en la salida del AND en 00 -> 10 | dejar la tolerancia |
| **Latch viejo (Codex R22-V2-01):** el escenario de §5.7, con la cola [B, AND, M] guionada y en los órdenes (b)-(d) | se anota | AND cerrado, M desactivada y la arista AND -> M.Set baja en cada lectura de M | publicar con `signalIn` y el latch |
| Latch viejo sin tocar cables: AND -> interruptor apagado -> M.Set, B off y el interruptor on en el mismo tick; y XOR -> interruptor -> M.Set con 10 -> 11 | se anota | M desactivada; ninguna lectura alta en Set | publicar con `signalIn` y el latch |
| A1-A3 sin Splitter (control) | verde | verde, y asignaciones finales iguales a las de la base | regresión de energía |
| A4: XOR sin carga, A on, B off, con y sin Splitter, 600 ticks | **rojo obligatorio** (oscila); si el modelo no lo reproduce, el modelo está incompleto y se arregla antes de implementar | como mucho 1 cambio; puerta abierta con AX asignación 0 y señal alta | lectura por flujo |
| B1: XOR -> Door Controller (CONSUMER) detrás de Splitter; A on, B on, A off, B off | rojo | después de cada paso, puerta = XOR(A,B) y Door Controller alimentado solo si da 1; tras B off, latch cerrado | lectura por flujo; dejar la tolerancia |
| D5: Power por Splitter; Power directo; Laser -> interruptor encendido -> Set | rojo: Set nunca alto | el Laser abre: Set alto, latch true, Output habilitada, lámpara >= 10, Power alto; el Laser cierra: latch se queda; pulso de Reset (2000 ms): latch false y lámpara 0. Se admite XM = 0 | lectura por flujo de Set; compuerta cacheada |
| D5 sin Power (solo Laser) | verde (medido así) | verde | control |
| Laser con entrada < 5 y compuerta abierta | n/a | señal baja en Set | quitar la condición de autoconsumo |
| **Salida rehabilitada (Codex R22-01):** celda A con Output deshabilitada y bit alto antiguo; A pierde alimentación; un Set la activa y rehabilita Output; la celda B (Set desde Output de A, Power aparte) va en la cola antes del reproceso de A | se anota | B no se activa; mientras A no se reprocesa, el lector ve baja | no limpiar el bit al cambiar el flag |
| **Q -> Toggle (Grok R22v2-02):** Output de A -> Toggle de B, Power de B aparte; rearme de 1000 ms ya cumplido; A pasa de desactivada a activada, y luego de activada a desactivada; reloj por eventos (§6.3) | se anota | A off -> on: B voltea exactamente una vez; A on -> off: B no voltea | reloj de 100 ms que junta el routing de 50 ms con el PDQ |
| **Salida rehabilitada con retraso (§5.8):** como la anterior, con Set de A sostenido y un pulso de Reset de 100 ms (Output deshabilitada unos 100 ms); (i) carga normal; (ii) con la cola llena de nodos pendientes para que la pasada de A llegue más de 1000 ms tarde | se anota | (i) B no voltea; (ii) B voltea como mucho una vez, que es la limitación declarada en §5.8 | - |
| C con Splitter: pulsador en Toggle (pulso 2000 ms, presses cada 3 s), en Set y en Reset | verde | verde: un flip por pulsación; Set y Reset ponen y quitan | regresión C1-C7 |
| Sobrecarga: G 50 -> Splitter; swA on -> XOR.in0, swB off -> in1, XOR -> lámpara 10; lámpara de 60 en la tercera salida | se anota | todas las asignaciones 0 (B5 intacto), latch estable (como mucho 1 cambio en 600 ticks), AX con señal alta | acoplar señal a sobrecarga |
| Corte real: G apagado; cable swA -> XOR cortado; Output de la celda deshabilitada | n/a | cuando la propagación llega, los lectores leen baja; las aristas habilitadas quedan con asignación 0 y señal baja; en las deshabilitadas **no** se exige asignación 0 (el código la conserva), sí señal baja; al volver G, las tablas como antes | retener señal; ignorar `LFPG_EDGE_ENABLED` |
| **Cadena larga (Codex R22-03):** G -> 64 Splitters en cadena -> interruptor -> XOR (sin carga) y -> pulsador -> Toggle de una celda | n/a | al apagar G el último tramo se procesa en ticks posteriores (presupuesto); cuando llega, lecturas bajas; ningún flip de la celda ni cambio de latch de más; al volver G, la celda no se voltea sola. No prueba por sí sola los presupuestos: para eso, las dos filas siguientes | no respetar presupuestos |
| **Presupuesto de nodos (Codex R22-V2-02):** 65 CONSUMER pendientes, cada uno con una entrada y sin lecturas en su callback | verde: es del modelo y no depende del arreglo | una llamada procesa 64, cuenta 64 visitas y deja 1 en la cola | modelo sin el límite de nodos |
| **Presupuesto de aristas:** nodos pendientes con varias aristas, de forma que el contador cruce 256 dentro de un nodo | verde: es del modelo | ese nodo se termina, el contador acaba por encima de 256, no se toma ningún nodo más y los que quedan siguen en su orden | modelo sin el límite de aristas; modelo que corta dentro de un nodo |
| Arranque y reconstrucción: todas las aristas nacen con señal false, con la puerta procesada **antes** que sus proveedores y también **entre** ellos (A, puerta, B), con PDQ directo sin avance de reloj | n/a | A3 y D5 llegan a su estado estable con como mucho un cierre y una reapertura de la puerta, y sin activar ninguna celda aguas abajo | señal inicial true; dejar la tolerancia |
| Invariante en todos los estados estables de todos los fixtures | n/a | asignación > EPS implica señal alta | - |

### 6.5 Estructurales sobre el fuente Enforce (el modelo no ve el cableado de PDQ)

- S1: `AllocateOutput` recibe `signalOut` y escribe `m_SignalHigh` tras el filtro
  `LFPG_EDGE_ENABLED` y fuera de `if (!overloaded)` (comprobado con llaves, no con un grep plano).
- S2: `m_SignalChanged` está en la condición de Step 3 y en la del reproceso en la misma época.
- S3: `m_SignalChanged = false;` va junto al `m_AllocChanged = false;` por nodo.
- S4: Step 1 lee `inEdge.m_SignalHigh` después del filtro `LFPG_EDGE_ENABLED` y sin pasar por
  `GetEdgeAllocatedPower`.
- S5: `ComputeSignalOut` recibe `gateIsClosed`, no `node.m_GateClosed`, y va antes de `AllocateOutput`.
- S6: `IsPortReceivingSignal` filtra `LFPG_EDGE_ENABLED` y el puerto, y mira `m_SignalHigh`, no
  `m_AllocatedPower`.
- S7: las puertas leen `input_0` e `input_1` por señal; la celda lee Power por flujo y Toggle, Reset
  y Set por señal.
- S8: `LFPG_ElecEdge` declara `m_SignalHigh` y lo pone a false; la base `LFPG_ElecGraph` declara
  `IsPortReceivingSignal` y devuelve false.
- S9: `checks.yml` corre el fichero nuevo.
- S10: `SetOutputPortEnabled` pone `m_SignalHigh = false` en las dos ramas que cambian el flag y sigue
  sin escribir `m_AllocatedPower`.
- S11: `LFPG_LogicGate.c` ya no declara ni usa `m_BrownoutHeld`.
- S12: en Step 2b, entre `ComputeSignalOut` y `AllocateOutput`, la rama de puerta hace
  `LFPG_LogicGateBase.Cast` de la entidad del pre-gate, lee `input_0` e `input_1` con
  `IsPortReceivingSignal`, asigna `signalOut` con `LFPG_EvaluateGateLogic` y no escribe ni
  `gateIsClosed` ni `allocAvail`.

### 6.6 Mutantes (sobre el texto del fuente, como `test_memorycell_toggle.py:507-514`)

Cada uno tiene que hacer fallar una aserción de comportamiento (F) o estructural (S). Un error de
extracción, un método ausente o una excepción del arnés no cuentan.

| Mutante | Tiene que morder |
|---|---|
| Puertas de vuelta a `IsPortReceivingPower` | F: A3 con Splitter, A4, B1 |
| Set de la celda de vuelta a flujo | F: D5 |
| Power de la celda a señal | F: señal presente y Power con asignación 0, Toggle no tiene permiso |
| Escribir la señal solo dentro de `if (!overloaded)` | S1, y F en sobrecarga |
| Quitar `m_SignalChanged` de Step 3 | S2, y F en dos casos donde la instrumentación de §6.3 afirma, en la pasada del interruptor, que los otros cinco disparadores están apagados y `m_SignalChanged` encendido: (i) XOR sin carga con `input_1` sin cable, detrás de Splitter -> swA: al apagar G, la pasada de swA (entrada 0 -> 0, demanda 0 -> 0, AX 0 -> 0) es el único camino al XOR, que con el arreglo acaba cerrado y con el mutante acaba abierto **en todos los órdenes** (Grok R22v2-01; Codex, pregunta 4 de la v2: con swB conectado, su sonda cambia 1 -> 0 y despierta al XOR según el orden); (ii) Laser -> interruptor -> Set al abrir el Laser, en la pasada del interruptor. No vale D5 con el Laser directo en Set: su transición de compuerta ya pone `m_AllocChanged` (`:2688-2690`) |
| Quitar `m_SignalChanged` del reproceso en la misma época | S2, y F de convergencia en (a) |
| `ComputeSignalOut` con `node.m_GateClosed` | S5, y F: primera pasada abierta de D5 |
| `ComputeSignalOut` sin la condición de autoconsumo | F: Laser con entrada < 5 |
| Señal atada a `m_Overloaded` | F: sobrecarga |
| Step 1 sin filtro de habilitada para la señal | S4, y F: Output de la celda deshabilitada |
| `IsPortReceivingSignal` sin filtro de puerto | S6, y F: XOR (1,0) leído como (1,1) |
| Constructor con `m_SignalHigh = true` | S8, y F: arranque |
| No limpiar el bit en `SetOutputPortEnabled` | S10, y F: salida rehabilitada |
| Volver a poner la tolerancia de un apagón (el bloque de 592ebcc) | S11, y F: transiciones 10 -> 00 (puerta abierta con 00) y 11 -> 00 -> 10 (celda activada), y B1 tras B off |
| Quitar la rama de puerta (la puerta publica con `signalIn` y el latch) | S12, y F: las dos filas de latch viejo |
| Modelo sin el límite de nodos (solo arnés) | F: presupuesto de nodos |
| Modelo sin el límite de aristas, o que corta dentro de un nodo (solo arnés) | F: presupuesto de aristas |

### 6.7 Lo que tienen que dar las suites que ya existen

- Todos los `test_graph_*.py`, `test_memorycell_toggle.py` y `test_pulse_survives_blackout.py` en
  verde sin cambiar expectativas; solo el `NM` falso de `test_memorycell_toggle.py` gana `GetGraph()`.
  Cualquier otro cambio se rechaza salvo justificación uno por uno.
- `test_graph_perf_probe.py` comprueba la instrumentación, no mide coste (Codex R22-03). El coste se
  mide en partida (§7).
- Oráculo `graph_reference`: sin cambios. En los fixtures nuevos solo comprueba que el reparto final
  es factible (Codex §8.5); no certifica la lógica.
- Linter de `AGENTS.md` §1 sobre la base y sobre el árbol: delta de `errors` = 0, gateado por
  `len(errors)`. Más el barrido manual de §1 por cada local tipada del diff (`graph.IsPortReceivingSignal`
  declarado en `LFPG_ElecGraph`; `allocEdge.m_SignalHigh`, `inEdge.m_SignalHigh` y `edge.m_SignalHigh`
  en `LFPG_ElecEdge`).

## 7. Riesgos del brief, uno a uno

- **B5 y sondas cerca del límite.** No se añade demanda, el all-off no cambia y la señal se escribe
  fuera de `!overloaded`. La lógica viva durante la sobrecarga es criterio de producto (§2.1).
  Fixture de sobrecarga y prueba en partida.
- **Regresión en lo que no es lógica.** Las fórmulas de energía no cambian. El único cambio de
  planificación es `m_SignalChanged`, que solo salta cuando un bit cambia. En redes sin lógica un bit
  cambia a la vez que `outputDelta`, `m_AllocChanged` o `inputChanged` (encender, apagar, compuerta,
  topología), así que esa propagación ya ocurría. Suites existentes sin cambiar expectativas.
- **Convergencia.** El bit es función monótona de los bits de arriba y de las compuertas; la
  propagación está acotada por `m_RequeueCount` (5), los presupuestos y los diferidos. Un bucle que
  monte el jugador (salida de una puerta a su propia entrada) oscila como hoy. Tests: como mucho 100
  ticks por transición y colas vacías, con presupuestos.
- **Coste por tick.** Un bool por arista; lecturas y escrituras dentro de bucles que ya existen; un
  lector nuevo que llaman los callbacks de puertas y celdas y, dos veces por pasada, la rama de puerta
  de Step 2b (unas cuatro visitas de arista por pasada de puerta, contadas en el presupuesto). Ni bucles, ni mapas, ni
  temporizadores, ni búsquedas de entidad nuevas por tick. Offline no se mide. En partida, con un PBO
  con `LFPG_PERF_PROBE` (ticks hasta asentar, nodos y visitas por evento, `LFPG_PerfProbe.c:162-180`,
  llamada en `LFPG_ElecGraphImpl.c:2941`): en eventos de circuitos sin lógica (generador con Splitter y
  lámparas encendido y apagado; el banco grande si se puede) nodos y visitas iguales a BASE con un
  margen del 10 %; en los circuitos lógicos, el arreglo asienta en 10 ticks como mucho. Y ningún
  «Requeue limit reached» en estado estable.
- **Persistencia y sincronización.** `m_SignalHigh` es solo de runtime. No cambian `OnStoreSave` ni
  `OnStoreLoad`, ni RPC, ni el registro de SyncVars.
- **TA3-01.** Sin tocar; el canal nuevo compara booleanos.

## 8. Partida (fase 3)

La matriz del brief, más:

- PBO del arreglo frente a BASE = `main` 592ebcc, con los hashes anotados.
- A1-A4 y B1-B2 con Splitter, con A1-A3 recorriendo la tabla en secuencia sobre el mismo circuito
  (§6.3); **A4 también sin Splitter** (en BASE es diagnóstico).
- **Q -> Toggle:** Output de una Memory Cell al Toggle de otra; activar la primera voltea la segunda
  una vez y desactivarla no la voltea.
- **Transiciones:** XOR 10 -> 00 y AND 11 -> 00 -> 10 con una Memory Cell en Set a la salida del AND:
  la puerta cierra en 00 y la celda no se activa.
- C1-C7 y D5 con Splitter, más D5 con Power directo y Laser -> interruptor -> Set.
- Regresión sin Splitter: A, B, C, D1-D4. G1-G2.
- **Sobrecarga con puerta (criterio de producto de §2.1):** generador sobrecargado con un XOR abierto:
  estable (como mucho 1 cambio en 60 s), cables en CRITICAL, lámpara apagada, LEDs de entrada siguiendo
  a los interruptores.
- **Coste:** las medidas de §7 con el PBO de `LFPG_PERF_PROBE`, BASE y arreglo.
- Criterios: los del brief, y además ningún «Requeue limit reached» en estado estable.
- El banco grande si se puede. Al acabar, el banco como estaba.

## 9. Para el bug-ledger (no se arregla aquí)

- **GL-01.** Electronic Counter lee `in0` e `in1` por flujo (`LFPG_ElectronicCounter.c:166-167`).
  Detrás de un Splitter, la misma subida podría contar pulsos de más. Además, con el canal nuevo su
  salida usa la regla general (`signalIn` y su compuerta de la pasada anterior), sin rama propia como
  las puertas. Sin medir.
- **GL-02.** Energía: un PASSTHROUGH hace all-off mientras su input va por detrás de la demanda
  (`:4734-4735` con `:2519`). Un consumidor detrás de un Splitter puede quedarse a oscuras una pasada
  cuando se abre una carga con compuerta. El reparto parcial de Grok es candidato para otra ronda.
- **GL-03.** Energía: la primera pasada abierta de un PASSTHROUGH con compuerta calcula su base de
  oferta con el `m_GateClosed` cacheado (`:2640-2652`, después de `:2525`), y nada garantiza una
  segunda pasada (Codex §5.2). Con la señal, D5 ya no depende de esto.
- **GL-04.** Una fuente de señal con autoconsumo (Laser, PressurePad) detrás de un distribuidor que
  va por detrás pierde sus 5 u/s una pasada, cierra su compuerta y resetea su hold
  (`LFPG_LaserDetector.c:229-239`). Un AND con un Laser detrás de un Splitter podría parpadear. Fuera
  de la matriz; sin medir.
- **GL-05.** La asignación de una arista deshabilitada se conserva (`:5084-5091`) y un lector de flujo
  podría ver la vieja al rehabilitarla, antes del reproceso del proveedor. Es el mismo hueco que Codex
  R22-01 cerró para la señal; para el flujo ya existía y no se toca.
- **TA3-01** sigue abierto.

## 10. R22 de la v3: qué tiene que mirar cada revisor

Solo el delta frente a la v2 y el cierre de sus propios hallazgos (§11.2), sin ver la revisión del
otro. Veredicto APTO / APTO CON CAMBIOS / NO APTO. Por hallazgo: id, gravedad, `path:line`, escenario
de fallo concreto y arreglo propuesto. En particular:

1. Con la rama de puerta, ¿queda algún camino en el que una puerta publique un valor distinto de la
   lógica de sus entradas actuales, o en el que una Memory Cell capture un alto falso? Mirad también
   la entidad que no se resuelve y el barrido en vacío.
2. La puerta publica su lógica actual mientras el reparto usa el latch vivo: ¿hay algún estado estable
   en el que no se cumpla el invariante «asignación > EPS implica señal alta»?
3. El reloj por eventos, el barrido en vacío y los dos presupuestos de §6.3: ¿bastan para que las
   filas nuevas de §6.4 den un rojo o un verde verdaderos?
4. La limitación declarada de §5.8: ¿algún criterio del brief o de C1-C7 que la contradiga?
5. El nuevo anclaje (i) del mutante de `m_SignalChanged`: ¿hay otro camino que despierte al XOR?

## 11. Cierre de las R22

### 11.1 R22 de la v1

| Hallazgo | Decisión | Dónde |
|---|---|---|
| Grok R22-01 (BLOQUEA): puerta abierta con 00 sin segunda visita | aceptado: se quita la tolerancia | §3.1, §5.5, §6.4 transiciones y B1, §6.6 |
| Codex R22-02 (BLOQUEA): pulso falso al salir de 00 | aceptado: se quita la tolerancia | §3.1, §5.5, §6.4 transiciones, §8 |
| Codex R22-01 (BLOQUEA): bit viejo al rehabilitar una salida | aceptado: bit a false al cambiar el flag, en los dos sentidos | §2.1, §3.1, §6.4, §6.6 |
| Grok R22-04 (IMPORTANTE): bit no limpio al deshabilitar | aceptado con el mismo cambio | §3.1 |
| Grok R22-02 (IMPORTANTE): B5 con lógica viva | no se cambia el contrato; se declara como criterio de producto y se prueba en partida | §2.1, §5.6, §8 |
| Grok R22-03 (IMPORTANTE): A4 con salida de escape | aceptado: rojo obligatorio | §6.4 |
| Grok R22-05 (IMPORTANTE): mutante de Step 3 mal anclado | aceptado con otro anclaje: D5 con el Laser directo tampoco muerde, porque la transición de compuerta pone `m_AllocChanged` (`:2688-2690`); se anclan las pasadas de un interruptor intermedio, con la instrumentación | §6.3, §6.6 |
| Codex R22-03 (IMPORTANTE): presupuestos y reloj | aceptado: presupuestos de producción, reloj compartido, corte por lecturas tras propagación acotada, cadena larga; sin exigir asignación 0 en aristas deshabilitadas | §6.3, §6.4, §6.6 |
| Grok R22-06 (MENOR): comentarios de `:80-84` y `:148-158` | resuelto al quitar la tolerancia | §3.1 |
| Grok R22-07 (MENOR): `test_pulse_survives_blackout.py` | añadido | §6.1, §6.7 |
| Grok §10.5: `NodeEntitySyncUnchanged` y `GetEdgeAllocatedPower` | añadidos a la fidelidad y al contrato | §2.1, §6.3, §6.5 S4 |

### 11.2 R22 de la v2

| Hallazgo | Decisión | Dónde |
|---|---|---|
| Codex R22-V2-01 (BLOQUEA): la puerta publica con el latch de su lectura anterior; alto falso 11 -> 10 con una arista nueva | aceptado: la puerta publica `LFPG_EvaluateGateLogic` de sus entradas actuales; el reparto no cambia | §0, §2.1, §3.1, §5.7, §6.4, §6.5 S12, §6.6 |
| Codex R22-V2-02 (IMPORTANTE): la cadena larga no prueba el límite de nodos; el corte es entre nodos | aceptado: una fila y un mutante por presupuesto, y corte antes de tomar cada nodo | §6.3, §6.4, §6.6 |
| Codex, pregunta 2: bajada corta alargada por un proveedor retrasado | aceptado como limitación declarada, con fixture | §5.8, §6.4 |
| Codex, pregunta 3: llamadas directas al PDQ y empates de temporizadores | aceptado: reloj por eventos sin avance en las llamadas directas; los dos órdenes en los empates | §6.3 |
| Codex, pregunta 4, y Grok R22v2-01 (IMPORTANTE): el anclaje (i) depende del orden y de en qué pasada se afirme | aceptado: XOR con `input_1` sin cable, swA como único camino, afirmado en la pasada de swA y en todos los órdenes | §6.6 |
| Codex, pregunta 5: Toggle durante la sobrecarga | aceptado: Toggle funciona si a Power le llega flujo de otra fuente | §2.1 |
| Grok R22v2-02 (IMPORTANTE): el reloj de 100 ms junta el routing de 50 ms con el PDQ; falta Q -> Toggle | aceptado: reloj por eventos y fila Q -> Toggle con su criterio | §6.3, §6.4, §8 |
| Grok R22v2-03 (MENOR): `SetOutputPortEnabled` no marca `m_SignalChanged` | sin cambio: las dos ramas que cambian el flag marcan `DIRTY_TOPOLOGY` (`:5096-5098`) y Step 3 propaga con `forceDownstream` (`:2769-2774`); Grok no encontró ningún lector huérfano | - |
| Grok, premisa de la v2: el cierre depende del reencolado, del reproceso en la misma época y de que Step 5 no salte el callback | aceptado: las aserciones leen latch y bits tras vaciar cola y diferidos | §6.3 |
| Medida del PC en partida: el rojo depende de recorrer la tabla | aceptado: A1-A3 en secuencia sobre un mismo circuito | §6.3, §6.4, §8 |
