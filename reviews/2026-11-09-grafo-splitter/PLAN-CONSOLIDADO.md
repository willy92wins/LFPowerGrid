# PLAN-CONSOLIDADO v2 — señal lógica separada del flujo (puertas y Laser -> Set detrás de un Splitter, 1.3.1)

Estado: v2, tras la R22 de la v1 (`REVIEW-grok-plan-consolidado.md` y
`REVIEW-codex-plan-consolidado.md`, las dos NO APTO; ninguna discute el canal de señal). Cambios
frente a la v1: §0, §2.1, §3.1, §4, §5.5, §6.3-6.7, §7, §8, y la tabla de cierre de §11. La v1 queda
como `PLAN-CONSOLIDADO-v1.md`. Base `main` 592ebcc, rama `claude/project-thread-0iolln`, PR #98 en
borrador. Si algo choca con `AGENTS.md`, manda `AGENTS.md` y se dice en el informe. Las líneas citadas
son de 592ebcc (los commits posteriores solo añaden documentos).

## 0. La decisión en una frase

Las entradas de señal de las puertas (AND/OR/XOR `input_0` e `input_1`) y de la Memory Cell
(Toggle `input_1`, Reset `input_2`, Set `input_3`) dejan de leer el flujo asignado y pasan a leer un
booleano por arista, `m_SignalHigh`, que el grafo propaga junto al reparto **sin cambiar el reparto**.
Power (`input_0`) de la Memory Cell sigue leyendo flujo.

Qué se toma de cada plan:

- **Codex:** la dirección (canal de señal), el contrato de señal, la causa de A4 y la mitad de D5
  que explica por qué no se recupera, la tabla de mutantes, los límites de convergencia y, tras la
  R22, quitar la tolerancia de un apagón de las puertas.
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

- La compuerta es el `gateIsClosed` **leído en vivo** en el pre-gate (`:2483-2509`), no
  `node.m_GateClosed`. Si la entidad no se resuelve, el pre-gate ya copia el valor cacheado.
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
  puede ponerse o quitarse por Set y Reset. Toggle no, porque necesita Power, que lee flujo. Los
  cables siguen en CRITICAL y aguas abajo no llega energía. Se queda así a propósito: atar la señal a
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
| `:2516-2525` (Step 2b) | calcular `signalOut` con `gateIsClosed` (vivo), `newPowered` y `signalIn`, y llamar `AllocateOutput(nodeId, allocAvail, signalOut)` (única llamada, verificado por las dos R22) | D5: publicar con la compuerta viva |
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
     que no queda ningún estado «abierto con 00» del que pueda salir un alto falso.
   - Lo que la tolerancia absorbía y deja de absorber: una puerta procesada antes que sus proveedores
     justo tras reconstruir aristas (nacen sin señal). Ahora cierra y reabre al llegar la señal, en la
     misma época o la siguiente. Fixture de arranque en §6.4.
6. **B5 (Grok R22-02): se queda como está y se declara**, por lo dicho en §2.1.

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
- **Presupuestos (Codex R22-03):** como mucho 64 nodos y 256 visitas de arista por llamada
  (`LFPG_Defines.c:465`, `:471`; cortes en `LFPG_ElecGraphImpl.c:2250-2253`), el doble con más de 128
  en cola y 128/512 en el warmup (`LFPG_NetworkManagerImpl.c:2446-2461`). Lo que no cabe se queda en
  la cola para la llamada siguiente, en su orden. Las visitas se cuentan donde las cuenta el fuente.
- **Reloj compartido:** cada llamada al PDQ es un tick de 100 ms (`LFPG_Defines.c:534`); el
  `g_Game.GetTime()` y la cola de llamadas de la Memory Cell avanzan con ese mismo reloj, así que los
  filtros de 400 ms ven los retrasos de los presupuestos.
- La señal: `signalIn` en Step 1, `ComputeSignalOut` antes de `AllocateOutput`, escritura sin mirar
  la sobrecarga, `m_SignalChanged`, y la limpieza en `SetOutputPortEnabled`.
- Instrumentación: por cada pasada de nodo, el modelo anota qué disparadores de Step 3 se
  encendieron (`outputDelta`, `forceDownstream`, `m_AllocChanged`, `inputChanged`,
  `demandBecameKnown`, `m_SignalChanged`), para que un test pueda afirmar cuál movió la propagación.

Órdenes: (a) guionado, la tabla de Codex §4.2 para A3 y §5.2 para D5; (b) FIFO; (c) LIFO; (d) 20
permutaciones con semilla fija de la cola sucia. La base tiene que dar rojo al menos en (a); el
arreglo, verde en todos.

Fin de cada transición: cola y diferidos vacíos, como mucho 100 ticks (agotarlos es FAIL); se cuentan
los cambios de latch. Los criterios de corte no dicen «en la misma época»: dicen «cuando la
propagación acotada ha llegado al lector», y se miden por las lecturas que hacen los callbacks.

### 6.4 Fixtures y resultado esperado

| Fixture | Hoy (592ebcc) | Con el arreglo | Muerde |
|---|---|---|---|
| A1-A3 con Splitter: AND, OR, XOR por 00/01/10/11; lámpara = CeilingLight PT, consumo 10, capacidad 50 | rojo en (a): XOR con 10 cierra tras dos visitas oscuras | latch = tabla, entrada de la lámpara >= 10 solo si la tabla da 1, como mucho 1 cambio de latch por combinación, en todos los órdenes | lectura por flujo |
| **Transiciones** con Splitter: 10 -> 00, 11 -> 00 (los dos en la misma época) y 11 -> 00 -> 10, con una Memory Cell en Set a la salida del AND (Power aparte de 1, sin carga) | se anota | tras cada paso el latch = tabla; la celda nunca se activa en 11 -> 00 -> 10 salvo por el 11 inicial, que se resetea antes; ningún alto transitorio en la salida del AND en 00 -> 10 | dejar la tolerancia |
| A1-A3 sin Splitter (control) | verde | verde, y asignaciones finales iguales a las de la base | regresión de energía |
| A4: XOR sin carga, A on, B off, con y sin Splitter, 600 ticks | **rojo obligatorio** (oscila); si el modelo no lo reproduce, el modelo está incompleto y se arregla antes de implementar | como mucho 1 cambio; puerta abierta con AX asignación 0 y señal alta | lectura por flujo |
| B1: XOR -> Door Controller (CONSUMER) detrás de Splitter; A on, B on, A off, B off | rojo | después de cada paso, puerta = XOR(A,B) y Door Controller alimentado solo si da 1; tras B off, latch cerrado | lectura por flujo; dejar la tolerancia |
| D5: Power por Splitter; Power directo; Laser -> interruptor encendido -> Set | rojo: Set nunca alto | el Laser abre: Set alto, latch true, Output habilitada, lámpara >= 10, Power alto; el Laser cierra: latch se queda; pulso de Reset (2000 ms): latch false y lámpara 0. Se admite XM = 0 | lectura por flujo de Set; compuerta cacheada |
| D5 sin Power (solo Laser) | verde (medido así) | verde | control |
| Laser con entrada < 5 y compuerta abierta | n/a | señal baja en Set | quitar la condición de autoconsumo |
| **Salida rehabilitada (Codex R22-01):** celda A con Output deshabilitada y bit alto antiguo; A pierde alimentación; un Set la activa y rehabilita Output; la celda B (Set desde Output de A, Power aparte) va en la cola antes del reproceso de A | se anota | B no se activa; mientras A no se reprocesa, el lector ve baja | no limpiar el bit al cambiar el flag |
| C con Splitter: pulsador en Toggle (pulso 2000 ms, presses cada 3 s), en Set y en Reset | verde | verde: un flip por pulsación; Set y Reset ponen y quitan | regresión C1-C7 |
| Sobrecarga: G 50 -> Splitter; swA on -> XOR.in0, swB off -> in1, XOR -> lámpara 10; lámpara de 60 en la tercera salida | se anota | todas las asignaciones 0 (B5 intacto), latch estable (como mucho 1 cambio en 600 ticks), AX con señal alta | acoplar señal a sobrecarga |
| Corte real: G apagado; cable swA -> XOR cortado; Output de la celda deshabilitada | n/a | cuando la propagación llega, los lectores leen baja; las aristas habilitadas quedan con asignación 0 y señal baja; en las deshabilitadas **no** se exige asignación 0 (el código la conserva), sí señal baja; al volver G, las tablas como antes | retener señal; ignorar `LFPG_EDGE_ENABLED` |
| **Cadena larga (Codex R22-03):** G -> 64 Splitters en cadena -> interruptor -> XOR (sin carga) y -> pulsador -> Toggle de una celda | n/a | al apagar G el último tramo se procesa en ticks posteriores (presupuesto); cuando llega, lecturas bajas; ningún flip de la celda ni cambio de latch de más; al volver G, la celda no se voltea sola | no respetar presupuestos |
| Arranque y reconstrucción: todas las aristas nacen con señal false, con la puerta procesada **antes** que sus proveedores | n/a | A3 y D5 llegan a su estado estable con como mucho un cierre y una reapertura de la puerta, y sin activar ninguna celda aguas abajo | señal inicial true; dejar la tolerancia |
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

### 6.6 Mutantes (sobre el texto del fuente, como `test_memorycell_toggle.py:507-514`)

Cada uno tiene que hacer fallar una aserción de comportamiento (F) o estructural (S). Un error de
extracción, un método ausente o una excepción del arnés no cuentan.

| Mutante | Tiene que morder |
|---|---|
| Puertas de vuelta a `IsPortReceivingPower` | F: A3 con Splitter, A4, B1 |
| Set de la celda de vuelta a flujo | F: D5 |
| Power de la celda a señal | F: señal presente y Power con asignación 0, Toggle no tiene permiso |
| Escribir la señal solo dentro de `if (!overloaded)` | S1, y F en sobrecarga |
| Quitar `m_SignalChanged` de Step 3 | S2, y F en dos pasadas donde los otros cinco disparadores están apagados y el test lo afirma con la instrumentación de §6.3: (i) A4 con Splitter al apagar G, en la pasada del interruptor A (entrada 0 -> 0, demanda 0 -> 0, AX 0 -> 0); (ii) Laser -> interruptor -> Set al abrir el Laser, en la pasada del interruptor. No vale D5 con el Laser directo en Set: su transición de compuerta ya pone `m_AllocChanged` (`:2688-2690`) |
| Quitar `m_SignalChanged` del reproceso en la misma época | S2, y F de convergencia en (a) |
| `ComputeSignalOut` con `node.m_GateClosed` | S5, y F: primera pasada abierta de D5 |
| `ComputeSignalOut` sin la condición de autoconsumo | F: Laser con entrada < 5 |
| Señal atada a `m_Overloaded` | F: sobrecarga |
| Step 1 sin filtro de habilitada para la señal | S4, y F: Output de la celda deshabilitada |
| `IsPortReceivingSignal` sin filtro de puerto | S6, y F: XOR (1,0) leído como (1,1) |
| Constructor con `m_SignalHigh = true` | S8, y F: arranque |
| No limpiar el bit en `SetOutputPortEnabled` | S10, y F: salida rehabilitada |
| Volver a poner la tolerancia de un apagón (el bloque de 592ebcc) | S11, y F: transiciones 10 -> 00 (puerta abierta con 00) y 11 -> 00 -> 10 (celda activada), y B1 tras B off |
| Modelo sin presupuestos (solo arnés) | F: cadena larga, que tiene que fallar si el modelo lo drena todo en un tick |

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
  lector nuevo que solo llaman los callbacks de puertas y celdas. Ni bucles, ni mapas, ni
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
- A1-A4 y B1-B2 con Splitter; **A4 también sin Splitter** (en BASE es diagnóstico).
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
  Detrás de un Splitter, la misma subida podría contar pulsos de más. Sin medir.
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

## 10. R22 de la v2: qué tiene que mirar cada revisor

Solo el delta frente a la v1 y el cierre de sus propios hallazgos (§11), sin ver la revisión del otro.
Veredicto APTO / APTO CON CAMBIOS / NO APTO. Por hallazgo: id, gravedad, `path:line`, escenario de
fallo concreto y arreglo propuesto. En particular:

1. Sin la tolerancia, ¿queda algún camino que publique un alto falso en la salida de una puerta, o que
   deje el latch distinto de la tabla tras la última lectura?
2. La limpieza en `SetOutputPortEnabled`: ¿algún caso en el que un bajo forzado ahí se vea como un
   flanco (Toggle) o borre una señal buena?
3. Los presupuestos y el reloj compartido de §6.3: ¿bastan para que la cadena larga y los 400 ms den
   un rojo o un verde verdaderos?
4. El mutante de `m_SignalChanged` con los dos anclajes de §6.6: ¿muerde de verdad?
5. B5 declarado: ¿algún criterio del brief que lo contradiga?

## 11. Cierre de la R22 de la v1

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
