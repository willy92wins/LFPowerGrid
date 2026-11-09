# PLAN-CONSOLIDADO v6 — señal lógica separada del flujo (puertas y Laser -> Set detrás de un Splitter, 1.3.1)

Estado: v6, tras la R22 de la v5 (`REVIEW-grok-plan-v5.md`, NO APTO: el reintento marca el nodo con
una máscara que puede saltarse el callback de la puerta, y reintenta en cada llamada en vacío para
siempre; `REVIEW-codex-plan-v5.md`, APTO CON CAMBIOS: el PDQ dice que la cola está vacía justo después
de que el reintento la llene, y el vecino ruidoso cuelga 65 consumidores de un Splitter de tres
salidas). Cambios frente a la v5: §0, §2.1, §3.1, §4, §5.10, §5.11, §6.3-6.7, §7, §8, §9, §10 y
§11.5. Los cambios son los arreglos que propusieron los propios revisores (Grok R22v5-03 se rechaza
con motivo), así que no hay R22 de la v6: la R21 del código los comprueba uno a uno (§10). La v5 queda
como `PLAN-CONSOLIDADO-v5.md`, la v4 como `PLAN-CONSOLIDADO-v4.md`, la v3 como
`PLAN-CONSOLIDADO-v3.md`, la v2 como `PLAN-CONSOLIDADO-v2.md` y la v1 como `PLAN-CONSOLIDADO-v1.md`.
Base `main` 592ebcc, rama `claude/project-thread-0iolln`, PR #98 en borrador. Si algo choca con `AGENTS.md`, manda `AGENTS.md` y se dice en el informe. Las líneas
citadas son de 592ebcc (los commits posteriores solo añaden documentos y tests).

## 0. La decisión en una frase

Las entradas de señal de las puertas (AND/OR/XOR `input_0` e `input_1`) y de la Memory Cell
(Toggle `input_1`, Reset `input_2`, Set `input_3`) dejan de leer el flujo asignado y pasan a leer un
booleano por arista, `m_SignalHigh`, que el grafo propaga junto al reparto **sin cambiar el reparto**.
Power (`input_0`) de la Memory Cell sigue leyendo flujo. Las puertas publican en su salida la lógica de
las entradas que leen **en el momento de publicar** (`LFPG_EvaluateGateLogic` sobre la señal de
`input_0` e `input_1`), no su latch.

Lo que se garantiza (§2.1, nuevo en v4): cada valor que lee una puerta o una celda lo escribió su
proveedor con lo que leía al procesarse, y al vaciarse la cola y los diferidos cada arista coincide
con la tabla de §2.1. Lo que no: dos cambios en la misma ventana de propagación pueden llegar en
cualquier orden, como en un biestable real (L-RACE). Sin entidad en los tres niveles de resolución,
una puerta lógica no da señal y los demás dispositivos con compuerta siguen la compuerta cacheada,
como la energía; en los dos casos el nodo queda pendiente y se reprocesa solo cuando la entidad vuelve
(nuevo en v5), con una pasada que llega al callback del dispositivo, como mucho una ronda de reintento
cada 10 llamadas y sin dar la cola por vacía cuando el reintento la acaba de llenar (nuevo en v6).

Qué se toma de cada plan:

- **Codex:** la dirección (canal de señal), el contrato de señal, la causa de A4 y la mitad de D5
  que explica por qué no se recupera, la tabla de mutantes, los límites de convergencia y, tras las
  R22, quitar la tolerancia de un apagón de las puertas, publicar la lógica actual de la puerta,
  resolver su entidad en el pre-gate como Step 5, declarar la carrera del mismo instante, tras la
  R22 de la v4, reprocesar el nodo cuando su entidad vuelve y, tras la de la v5, contar lo pendiente
  después del reintento y montar el vecino ruidoso con los puertos reales del Splitter.
- **Grok:** la causa de A3 (coincide con Codex), el porqué del control sin Splitter (SOURCE con 50
  fijos frente a PASSTHROUGH con el input de la época), la mitad estática de D5 (oferta 5 - 5 = 0 y
  exclusión del vector), la fidelidad del arnés (el `QueueModel` actual no aplica la sonda de
  compuerta cerrada), el control negativo sin Splitter, tras la R22 de la v3, que sin entidad no
  haya señal, tras la de la v4, que esa baja sea solo de las puertas lógicas, también con `Cast`
  nulo, y tras la de la v5, que el reintento marque el nodo con la máscara que obliga a escribir en el
  dispositivo y vaya a la cadencia del barrido.
- **Descartado de Grok:** el reparto parcial en PASSTHROUGH (su parche 1), el +1 u/s del Laser
  abierto (su parche 2) y, tras la R22 de la v3, el orden topológico de la señal y el Set que ignora un
  alto aún no reescrito. Motivos en §5.

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
| Puerta lógica (`m_IsLogicGate`, nuevo en v5) con la entidad resuelta y `LFPG_LogicGateBase.Cast` no nulo | `LFPG_EvaluateGateLogic(señal de input_0, señal de input_1)`, leídas en ese momento con `IsPortReceivingSignal`; no usa el latch ni `signalIn`. La entidad se busca en los tres niveles de Step 5 (§5.10) |
| Puerta lógica sin entidad en los tres niveles, o con `Cast` nulo | baja: sin la entidad no se puede calcular su lógica. Queda pendiente de reintento. Sin entidad, la energía sigue con la compuerta cacheada, como hoy; con `Cast` nulo, con la compuerta que dé la entidad resuelta, como hoy cuando `FindById` la devuelve (§5.10; Codex R22-V5-03) |
| Otro nodo con compuerta (interruptor, Laser, batería, contador...) sin entidad en los tres niveles | la fila de PASSTHROUGH con la compuerta cacheada, la misma que usa la energía. Queda pendiente de reintento (§5.10) |

- La compuerta es el `gateIsClosed` **leído en vivo** en el pre-gate (`:2483-2509`), no
  `node.m_GateClosed`. Si la entidad no aparece en los tres niveles, el reparto sigue copiando el
  valor cacheado, como hoy, y la señal usa ese mismo valor, salvo en las puertas lógicas, que dan baja
  (filas de arriba).
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

Garantía (nuevo en v4; Codex R22-V3-02 y Grok R22v3-01):

- **G1, sin valores inventados.** El bit que escribe un proveedor sale de la tabla con lo que lee en
  esa pasada: sus aristas de entrada, su compuerta en vivo y, si es una puerta, su lógica actual. Nunca
  del latch de una lectura anterior. Sin entidad, una puerta lógica escribe baja y otro nodo con
  compuerta usa su compuerta cacheada, el valor con el que ya reparte la energía (§5.10). Solo el
  proveedor escribe sus bits, en su `AllocateOutput`; `SetOutputPortEnabled` solo los pone a false.
- **G2, coherencia al vaciar.** Con la cola y los diferidos vacíos, todas las entidades con compuerta
  resolubles y ningún nodo pendiente de reintento, cada arista habilitada coincide con el oráculo de
  §6.3 (la tabla aplicada desde las fuentes con el estado de los dispositivos), el latch de cada puerta
  es su tabla y no queda ningún cambio por llegar. Y se llega a ese estado (nuevo en v5, Codex
  R22-V4-01): un nodo pendiente cuya entidad vuelve a resolver se reprocesa sin que nada más lo encole,
  con una pasada que llega a su callback (v6, Grok R22v5-01); con n pendientes, en como mucho
  ceil(n / 4) rondas de reintento, que corren en llamadas en vacío y como mucho una cada 10 llamadas de
  PDQ (v6, Grok R22v5-02), más su propagación. La llamada que encola el nodo no da la cola por vacía
  (v6, Codex R22-V5-01).
- **L-RACE, limitación declarada.** La propagación va pasada a pasada, y las «entradas actuales» de una
  puerta son los bits publicados, no una foto coherente del tick (Codex). Si dos cambios caen en la
  misma ventana por caminos distintos (baja una entrada de una puerta y en el mismo tick se conecta un
  cable de su salida a Set, o se enciende un interruptor entre la puerta y Set), o si un solo cambio
  llega a una celda por dos caminos de distinta longitud, el orden de la cola decide qué ve antes la
  celda, y puede capturar el estado intermedio. Es la carrera de un biestable real, y hoy, con la
  lectura por flujo, ya existe. La ventana dura hasta que la cola se vacía: normalmente una llamada de
  PDQ, más si los presupuestos la parten. Dos cambios separados por un vaciado de la cola nunca dan esa
  captura.

Consecuencias, declaradas (criterio de producto, también en la fase 3):

- Señal alta no certifica energía. Una puerta puede estar abierta con asignación 0 (A4), un Laser
  puede dar Set sin financiar la celda (D5) y una lámpara sigue necesitando sus 10 reales.
- **Con un generador sobrecargado (all-off, B5) la lógica sigue viva**: las señales siguen altas, los
  LEDs de las puertas muestran los interruptores, una puerta puede abrir y cerrar y una Memory Cell
  puede ponerse o quitarse por Set y Reset. Toggle solo si a Power le llega flujo, es decir, desde una
  fuente que no esté en sobrecarga. Los cables siguen en CRITICAL y aguas abajo no llega energía. Se queda así a propósito: atar la señal a
  la sobrecarga (Grok R22-02) vuelve a acoplar lógica y reparto, que es lo que este arreglo separa;
  hoy, con una carga detrás de una puerta, la sobrecarga cierra y reabre la puerta en bucle.
- L-RACE (arriba). Para la nota al jugador: si conectas un cable a Set o accionas un interruptor justo
  cuando cambia una entrada de la puerta, la Memory Cell puede quedarse con el valor que la puerta tenía
  un instante antes.
- Un nodo con compuerta sin entidad tras los tres niveles: si es una puerta lógica, señal baja; si
  no, señal y energía con la compuerta cacheada. En los dos casos queda pendiente, y cuando la entidad
  vuelve el reintento en vacío lo reprocesa, también su energía (§5.10). Hoy la energía se queda con la
  compuerta cacheada hasta el siguiente evento del nodo.
- Invariante que se prueba: en estado estable, con todas las entidades con compuerta resueltas y sin
  reintentos pendientes,
  `m_AllocatedPower > EPS` implica `m_SignalHigh` en toda arista habilitada. Señal sin flujo vale; flujo
  sin señal es un fallo. Como el invariante no ve un alto falso (Codex, pregunta 2 de la v3), los tests
  comprueban además G2 con el oráculo.

### 2.2 Quién lee qué

| Lector | Puerto | Lee |
|---|---|---|
| AND/OR/XOR | `input_0`, `input_1` | señal |
| Memory Cell | Toggle `input_1`, Reset `input_2`, Set `input_3` | señal |
| Memory Cell | Power `input_0` | flujo, sin cambios |
| Electronic Counter, consumidores, `VerifyPassthroughPowered` | todos | sin cambios |

## 3. Cambios, cada uno trazado a §1 o a §11

### 3.1 Producción, seis ficheros

| Fichero:ancla (592ebcc) | Cambio | Causa |
|---|---|---|
| `3_Game/LFPG_Data.c:325-356` (`LFPG_ElecEdge`) | `bool m_SignalHigh;` y `m_SignalHigh = false;` en el constructor, con el comentario «runtime only» como `m_OfferedResidual` (`:340-343`) | llevar la señal sin tocar asignación ni oferta |
| `3_Game/LFPG_Data.c:258` y `:316` (`LFPG_ElecNode`), nuevo en v5 | `bool m_IsLogicGate;` junto a `m_IsGated` y `m_IsLogicGate = false;` en el constructor, con «runtime only» | Grok R22v4-02 y -03: saber sin la entidad si el nodo es una puerta lógica |
| `3_Game/LFPG_Defines.c`, junto a `:518`, nuevo en v5 | `static const int LFPG_GATE_RETRY_PER_CALL = 4;`, con un comentario: reintentos de entidad por ronda; una ronda como mucho cada `LFPG_CONSUMER_VALIDATE_TICK_INTERVAL` llamadas (v6) | acotar el reintento (§5.10) |
| `4_World/LFPG_ElecGraph.c:305-311` | `bool IsPortReceivingSignal(string deviceId, string portName)` en la base, con el mismo fallback de error y `return false` | que lo vean las puertas y la celda (orden de módulos) |
| `5_Mission/LFPG_ElecGraphImpl.c:81`, `:179`, `:2298` | `protected bool m_SignalChanged;`, a false en el constructor y al empezar cada nodo junto a `m_AllocChanged = false;` | ver cambios de señal aunque todos los floats sigan iguales (A4, cadenas sin flujo) |
| `:2308-2349` (Step 1) | `bool signalIn = false;` junto a `inputSum`, y tras el filtro `LFPG_EDGE_ENABLED`: `if (inEdge.m_SignalHigh) signalIn = true;`, leyendo el bit directamente | propagar la línea viva por tramos sin flujo |
| helper nuevo junto a `:4009-4034` | `protected bool ComputeSignalOut(bool isSource, float newOutput, bool signalIn, float virt, float cons, bool powered, bool gateClosed)` con la tabla de §2.1; solo `if/else`, bool y float, sin bucles ni enums, para que `enforce_scalar_slice` lo ejecute | el contrato en un sitio y probado offline |
| `:2487` (pre-gate) | `resolvedEnt = ResolveGatedEntity(nodeId);` en vez de `FindById(nodeId)`. El resto del pre-gate no cambia | Codex R22-V3-01: el pre-gate y Step 5 resuelven la misma entidad en la misma pasada |
| helper nuevo junto a `SyncNodeToEntity` (`:3044`) | `protected EntityAI ResolveGatedEntity(string nodeId)`: los tres niveles de `:3079-3111` en el mismo orden (`FindById`, `LFPG_DeviceAPI.ResolveVanillaDevice` y NetworkID con `m_NodeNetLow`/`m_NodeNetHigh`, con `Register` si acierta), y un comentario que diga que van a la par. `SyncNodeToEntity` y `ResetOrphanSyncVars` no se tocan (R20) | el mismo |
| `:1414` (`EnsureNode`) y `:3705` (`PopulateAllNodeElecStates`), nuevo en v5 | tras `node.m_IsGated = ...;`: `node.m_IsLogicGate = false;` y, si `node.m_IsGated`, `LFPG_LogicGateBase lgCached = LFPG_LogicGateBase.Cast(obj); if (lgCached) node.m_IsLogicGate = true;` | el mismo flag en los dos sitios que ponen `m_IsGated` |
| `:136`, `:206` y `:289`, nuevo en v5 | `protected ref array<string> m_GateRetry;`, creado en el constructor junto a `m_DeferredRequeue` y vaciado en `RebuildFromWires` junto a `m_NodeNetLow.Clear();` | Codex R22-V4-01: nodos con compuerta pendientes de reintento |
| `:106` y `:190`, nuevo en v6 | `protected int m_LastGateRetryTick;` junto a `m_LastValidateTick`, y `m_LastGateRetryTick = 0;` en el constructor junto a `m_LastValidateTick = 0;`, con el comentario: llamada de PDQ (`m_ValidateTickCount`) de la última ronda de reintento | Grok R22v5-02: la cadencia del barrido, con su propio contador |
| helpers nuevos junto a `ResolveGatedEntity`, nuevo en v5 | `protected void QueueGateRetry(string nodeId)`: lo añade si `m_GateRetry.Find(nodeId) < 0`. `protected void RetryUnresolvedGates()`: si `m_GateRetry` está vacío, vuelve; si `m_ValidateTickCount - m_LastGateRetryTick < LFPG_CONSUMER_VALIDATE_TICK_INTERVAL`, vuelve (v6); si no, `m_LastGateRetryTick = m_ValidateTickCount;` y toma del principio como mucho `LFPG_GATE_RETRY_PER_CALL` entradas, contadas con `Count()` al empezar la ronda para no volver a probar en la misma ronda una que acaba de pasar al final, cada una con `m_GateRetry[0]` y `m_GateRetry.RemoveOrdered(0)` (nunca `Remove(0)`, que en Enforce llena el hueco con el último; patrón de `LFPG_Cable3D.c:316`); descarta la de un nodo que ya no existe o ya no tiene compuerta; con `ResolveGatedEntity` resuelta (y, en una puerta lógica, `LFPG_LogicGateBase.Cast` no nulo) descarta la entrada y llama `MarkNodeDirty(nodeId, LFPG_DIRTY_INPUT)` (v6); si no, la vuelve a meter al final con `Insert` | el mismo; acotado en resoluciones por ronda y en rondas por llamada, sin encolar nada mientras no resuelve; Grok R22v5-01: la máscara que obliga a Step 5 a escribir; Grok R22v5-02: la cadencia; Codex, pregunta 5 de la v5: la rotación |
| `:2231` y `:2921`, nuevo en v5 | `RetryUnresolvedGates();` justo después de `ValidateConsumerStates(edgeBudget);`, en los dos puntos en vacío | el mismo; el barrido ve lo mismo que hoy mientras ninguno resuelve (§5.11) |
| `:2231-2237` y `:2914-2953`, nuevo en v6 | lo pendiente se vuelve a contar después de `RetryUnresolvedGates();`: en la rama de `:2220-2238`, `int idleRemaining = m_DirtyQueue.Count() - m_DirtyQueueHead;`, que pasa a `LFPG_PerfProbe.OnProcessDirtyQueue(idleRemaining, 0, 0, m_LastProcessMs);` (`:2235`) y a `return idleRemaining;` (`:2237`); en la de `:2914-2922`, `remaining = m_DirtyQueue.Count() - m_DirtyQueueHead;` dentro de esa rama, que llega a la sonda (`:2941`), al log y a `return remaining;` (`:2953`) | Codex R22-V5-01: el warmup (`LFPG_NetworkManagerImpl.c:2463`) y la sonda (`LFPG_PerfProbe.c:171`) no dan por vacía una cola que el reintento acaba de llenar |
| `:2516-2525` (Step 2b) | calcular `signalOut` con `ComputeSignalOut` (`gateIsClosed` vivo, `newPowered`, `signalIn`). Después, si `node.m_IsLogicGate`: con la entidad resuelta (`gateEntityResolved`, `resolvedEnt` de `:2487`) y `LFPG_LogicGateBase.Cast(resolvedEnt)` no nulo, `signalOut = LFPG_EvaluateGateLogic(IsPortReceivingSignal(nodeId, "input_0"), IsPortReceivingSignal(nodeId, "input_1"))`, con los nombres de puerto en locales `string` como en `LFPG_LogicGate.c:139-140`; si no (sin entidad o con `Cast` nulo), `signalOut = false` y `QueueGateRetry(nodeId)`. Si `node.m_IsGated`, no es puerta lógica y el pre-gate no resolvió la entidad: `QueueGateRetry(nodeId)`, y `signalOut` se queda con el de `ComputeSignalOut`, que en ese caso lleva la compuerta cacheada en `gateIsClosed`. Ni `gateIsClosed` ni `allocAvail` cambian. Luego `AllocateOutput(nodeId, allocAvail, signalOut)` (única llamada, verificado por las dos R22) | D5: publicar con la compuerta viva; Codex R22-V2-01: la puerta publica su lógica actual; Codex R22-V3-01 y Grok R22v3-02: sin entidad no hay señal; Grok R22v4-02 y -03: esa baja, solo en puertas lógicas y también con `Cast` nulo; Codex R22-V4-01: el reintento |
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
- `ValidateConsumerStates`, `SyncNodeToEntity`, `ResetOrphanSyncVars` y `NodeEntitySyncUnchanged`
  (§5.11). `LFPG_DeviceRegistry` entero, también `Register`: la recuperación es del grafo (§5.10).
- Excepciones declaradas en el reparto (§5.10): si el registro falla y el NetworkID acierta, el
  pre-gate lee la compuerta viva y actualiza `m_GateClosed`, lo mismo que Step 5 ve en esa pasada. Y
  cuando vuelve la entidad de un nodo pendiente de reintento, su pasada lee la compuerta viva y, si
  difiere de la cacheada, la transición de `:2688-2706` corrige el reparto entonces y no en el
  siguiente evento del nodo. Con el registro resolviendo, que es lo normal, el reparto no cambia.
- Excepción declarada en el valor que devuelve el PDQ (v6, Codex R22-V5-01): en los dos puntos en
  vacío pasa a ser lo que de verdad queda en la cola después del barrido y del reintento. Sin
  reintentos que resuelvan y sin callbacks del barrido que encolen, sigue siendo 0, como hoy (§5.11).

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
9. **Carrera del mismo instante: se declara, no se ordena (Grok R22v3-01, BLOQUEA; Codex R22-V3-02).**
   - Escenario: el de §5.7 con la cola [AND, M, B], o cualquier orden con AND antes que B. AND lee 11
     porque B aún no ha escrito su baja, y publica bien AND(1,1); M lee Set alto y se activa; B
     escribe baja, AND pasa a bajo y cierra; M se queda activada. Testigo de Codex con callbacks
     reales: AND false y M true.
   - No es un valor inventado: AND valía 1 hasta que el flanco de B le llegó, y M leyó ese estado
     publicado. Es la carrera de un biestable real, y ya existe hoy con la lectura por flujo.
   - Descartado el orden topológico de señal por época (Grok): es otra arquitectura. O cambia el orden
     de la cola para el reparto, que no se toca, o pide una segunda pasada solo de señal. Y no quita el
     caso de un solo cambio que llega a una celda por dos caminos de distinta longitud, que es del
     circuito.
   - Descartado el Set que ignora un alto aún no reescrito en la época (Grok): no lo arregla, porque en
     [AND, M, B] AND ya reescribió su salida en esta época.
   - Descartado, para esta versión, un filtro en Set y Reset como el de 400 ms de Toggle: cambia la
     celda, que §4 protege, y podría perder un Laser que se cruza deprisa. Va al bug-ledger (GL-07).
   - Se declara como L-RACE (§2.1) y se prueba su frontera (§6.4): en orden topológico M nunca se
     activa; con los dos cambios separados por un vaciado de la cola, M nunca se activa en ningún
     orden; en cualquier orden, al vaciar se cumple G2 y el barrido no cambia el latch de M.
10. **Entidad de compuerta: tres niveles; sin ella, baja solo en las puertas lógicas, y reintento
    cuando vuelve (Codex R22-V3-01 y R22-V4-01, BLOQUEA; Grok R22v3-02, R22v4-02 y R22v4-03).**
    - Hoy el pre-gate solo usa `FindById` (`:2487`) y Step 5 resuelve en tres niveles (`:3079-3111`:
      registro, vanilla y NetworkID). Si el registro falla y el NetworkID acierta, la puerta publica con
      el latch cacheado y Step 5 recupera la entidad detrás de M. Testigo de Codex: AND false y M true.
    - El pre-gate pasa a usar los mismos tres niveles (`ResolveGatedEntity`), así que publica con la
      entidad que Step 5 va a usar en esa pasada.
    - Que los tres niveles fallen no prueba que el dispositivo no exista (Codex, pregunta 4 de la v4):
      son índices y cachés. Por eso la v5 no trata igual a todos los nodos con compuerta:
      - **Puerta lógica** (`m_IsLogicGate`, cacheado al crear el nodo, como `m_IsGated`): sin la
        entidad, o con una entidad que no es `LFPG_LogicGateBase`, no hay lógica que calcular, así que
        `signalOut = false`. Publicar el latch cacheado es el alto falso de Codex (v3).
      - **Otro nodo con compuerta** (interruptor, Laser, batería, contador, placa de presión, sensor de
        movimiento, intercomunicador): la fila de PASSTHROUGH con la compuerta cacheada, el mismo valor
        con el que ya reparte la energía. Así un interruptor vivo que el grafo no encuentra no apaga el
        AND de detrás mientras le sigue pasando energía (Grok R22v4-02).
      - En los dos casos la energía sigue con la compuerta cacheada (`:2497-2503`), como hoy.
    - **Reintento (Codex R22-V4-01):** el nodo entra en `m_GateRetry`. En las llamadas en vacío, justo
      después del barrido, `RetryUnresolvedGates` hace una ronda con la cadencia del barrido: como mucho
      una cada `LFPG_CONSUMER_VALIDATE_TICK_INTERVAL` (10) llamadas de PDQ, medidas con
      `m_ValidateTickCount` y su propio `m_LastGateRetryTick` (v6, Grok R22v5-02). Cada ronda prueba como
      mucho 4 entradas desde el principio de la lista. Si la entidad resuelve (en una puerta lógica, con
      `Cast` no nulo), quita la entrada y llama `MarkNodeDirty(nodeId, LFPG_DIRTY_INPUT)`, y el nodo se
      reprocesa en la llamada siguiente. Si no, la pasa al final, para que ninguna se quede sin probar:
      se saca con `RemoveOrdered(0)` y se vuelve a meter con `Insert`. `Remove(0)` no vale, porque en
      Enforce llena el hueco con el último elemento y la ronda acaba probando siempre las mismas (Codex,
      pregunta 5 de la v5: con 11 pendientes examina 0, 10, 0, 10 y no llega al 1). Quita las de nodos
      que ya no existen. Mientras no resuelve no encola nada, así que la cola sigue vacía y el barrido
      corre como hoy.
    - **Máscara `LFPG_DIRTY_INPUT` (v6, Grok R22v5-01):** es la que obliga a Step 5 a escribir
      (`NodeEntitySyncUnchanged`, `:2985-2986`), así que en la pasada del reintento corre el callback
      del dispositivo y el latch de la puerta vuelve a su tabla. En el fuente `LFPG_DIRTY_INTERNAL` también
      llegaría al callback, porque un Step 5 que no encuentra la entidad borra su último registro
      (`:3117`) y sin registro se escribe siempre (`:2987-2988`), o porque la entidad que vuelve no es la
      última sincronizada (`:2991-2992`); con `LFPG_DIRTY_INPUT` el reintento deja de depender de esa
      contabilidad. En un nodo con compuerta (siempre PASSTHROUGH: `:1414` y `:3705`) la máscara no cambia nada
      más: `skipInputEval` es de SOURCE (`:2303`) y `forceDownstream`, de topología (`:2769`).
    - **Lo que devuelve el PDQ (v6, Codex R22-V5-01):** después del barrido y del reintento, los dos
      puntos en vacío vuelven a contar la cola, y ese número es el que reciben la sonda y el `return`.
      Así el warmup (`LFPG_NetworkManagerImpl.c:2463`) no se apaga y la sonda (`LFPG_PerfProbe.c:171`)
      no asienta en la llamada en la que el reintento acaba de encolar un nodo.
    - Plazo: con n pendientes, una entidad que vuelve se prueba en como mucho ceil(n / 4) rondas, es
      decir, unas 10 · ceil(n / 4) llamadas en vacío. En el escenario de Codex (una pendiente), la señal
      de A sube como mucho 11 llamadas de PDQ después de volver el registro (la ronda llega como tarde
      10 llamadas después de la anterior, y la llamada siguiente procesa A), y B abre en esa misma
      llamada si cabe en el presupuesto. Como el barrido, el reintento necesita que la cola se vacíe: con
      la cola siempre ocupada, la entidad que vuelve espera (Codex, premisa de la v5).
    - Si el nodo se reprocesa antes por otro motivo y resuelve, su entrada se queda hasta el siguiente
      reintento, que lo reprocesa una vez más, sin cambios. Se acepta para no tocar el camino normal del
      pre-gate.
    - Descartado reencolar el nodo en el tick siguiente mientras no resuelva (la propuesta del PC):
      mantiene la cola ocupada, y el barrido, que solo corre con la cola vacía, deja de correr mientras
      dure; y con un tope en ticks, una entidad que vuelve tarde se queda otra vez sin recuperar.
    - Descartado avisar desde `Register`: lo llaman todos los dispositivos al iniciarse y es el registro,
      no el grafo. El reintento queda dentro del grafo y solo hace algo cuando hay pendientes.
    - Descartado un tope de edad o de rondas (v6; la primera opción de Grok R22v5-02): una entidad que
      vuelve después del tope se quedaría otra vez sin recuperar, que es R22-V4-01. Con la cadencia del
      barrido, una entrada que no resuelve nunca cuesta como mucho 4 resoluciones cada 10 llamadas, unas
      4 por segundo con el PDQ a 100 ms, hasta que se borra su nodo o se reconstruye el grafo.
    - Descartado conservar el último bit: en un dispositivo borrado dejaría un alto viejo hasta que se
      limpia el nodo.
    - Efecto en energía, declarado: si el registro falla y el NetworkID acierta, el pre-gate lee la
      compuerta viva y actualiza `m_GateClosed`, lo mismo que Step 5 ve en esa pasada. Y cuando vuelve la
      entidad de un nodo pendiente, su pasada lee la compuerta viva y, si difiere de la cacheada, la
      transición de `:2688-2706` corrige el reparto entonces. Con el registro resolviendo, que es lo
      normal, no cambia nada.
    - Efecto en energía con `Cast` nulo, declarado (v6, Codex R22-V5-03): si la entidad que resuelve no
      es una puerta lógica, el pre-gate la da por resuelta y reparte con su compuerta (`:2487-2496`), no
      con la cacheada; un Splitter, por ejemplo, devuelve abierta (`LFPG_DeviceAPI.IsGateOpen`,
      `4_World/LFPG_IDevice.c:561-568`, y `4_World/lfpg_devicebase.c:544-547`). Es lo que hace hoy
      cuando `FindById` devuelve esa entidad. La señal da baja y el nodo queda pendiente, como arriba.
    - Coste: `ResolveVanillaDevice` y `GetObjectByNetworkId` solo cuando `FindById` falla; el reintento,
      solo con pendientes, y como mucho 4 resoluciones por ronda y una ronda cada 10 llamadas.
11. **Barrido en vacío: sin cambios en producción (Grok R22v3-03).**
    - `ValidateConsumerStates` solo corre con la cola vacía: al empezar la llamada si no hay nada
      pendiente (`:2219-2231`) y al acabar si no queda nada tras reinsertar los reencolados diferidos
      (`:2896-2921`). Y salta los nodos en cola o sucios (`:3466-3467`).
    - El alto pasajero de L-RACE solo existe mientras el cambio que lo deshace (la baja de B) sigue
      pendiente en la cola, así que el barrido no puede leerlo; si B no estaba sucio, el 11 es real y no
      hay carrera (Grok, premisa de la v4). Con la cola vacía y las entidades resueltas, G2 dice que los
      bits están asentados, y una celda que el barrido resincroniza lee lo mismo que en una pasada.
    - El reintento de §5.10 va después del barrido, en los mismos dos puntos y con su cadencia, y solo
      encola nodos cuya entidad ya resuelve: mientras ninguno resuelve, el barrido ve lo mismo que hoy;
      cuando uno resuelve, ese nodo entra en la cola como cualquier evento y la llamada siguiente ya no
      empieza en vacío (v6, Grok R22v5-02).
    - Lo pendiente se vuelve a contar después del barrido y del reintento (v6, Codex R22-V5-01). Eso
      cuenta también lo que un callback del barrido ya podía encolar con `RequestPropagate`
      (`LFPG_NetworkManagerImpl.c:2422-2433`, que llama a `MarkNodeDirty`; por ejemplo, una puerta cuyo
      latch cambia en su `SetPowered`), que hoy sale como 0: el warmup puede durar una llamada más y la
      sonda asienta en la llamada que de verdad vacía la cola. Se acepta: es el valor real.
    - El modelo lo imita tal cual (§6.3) y las filas de carrera afirman que el barrido no cambia el
      latch de M.

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
  latch cambia, `RequestPropagate` del propio nodo. Con las comprobaciones en el orden del fuente
  (v6): un Step 5 que no encuentra la entidad borra el último registro (`:3117`), y la siguiente
  escritura con entidad no se salta (`:2987-2988`); otra entidad tampoco (`:2991-2992`).
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
  devuelve ese reloj. Si un temporizador y un PDQ caen en el mismo ms, se prueban los dos órdenes, y la
  traza lo deja ver (Grok, defectos de la v4). La traza anota el ms de cada `CallLater` y de cada PDQ (Grok, cierre de R22v2-02).
- **Barrido en vacío (Codex, pregunta 3 de la v3):** `ValidateConsumerStates` (`:3417`) solo corre con
  la cola vacía: al empezar la llamada si no hay nada pendiente (`:2219-2231`) y al acabar si no queda
  nada tras reinsertar los reencolados diferidos (`:2896-2921`), así que una llamada que empieza con
  cola y la vacía lo corre al final (Grok, pregunta 5 de la v4). Cada
  `LFPG_CONSUMER_VALIDATE_TICK_INTERVAL` = 10 ticks, en lotes de `LFPG_VALIDATE_BATCH_SIZE` = 32 con el
  cursor circular y dentro del presupuesto de aristas (`LFPG_Defines.c:514`, `:518`); salta los nodos
  en cola o sucios (`:3466-3467`), corrige `m_Powered` por flujo en los dos sentidos, también en
  PASSTHROUGH, y **solo entonces** llama `SyncNodeToEntity` sin máscara, con `NodeEntitySyncUnchanged`
  como en el fuente (`:3133`). No es una reevaluación incondicional. El modelo lo imita así, porque
  puede tapar un mutante o provocar un cambio.
- **El bit solo lo escribe su proveedor (Grok, defectos que los tests de la v3 no verían):**
  `m_SignalHigh` cambia en la pasada del proveedor (`AllocateOutput`) o pasa a false en
  `SetOutputPortEnabled`; nunca al encolar un nodo ni al cambiar la entidad de un interruptor. La
  traza anota, por cada lectura de señal, el bit que vio el lector, quién lo escribió y en qué pasada.
- **Resolución de entidad:** registro, vanilla y NetworkID son tres funciones del modelo que un fixture
  puede hacer fallar por nodo y por pasada. En la base el pre-gate usa solo el registro y Step 5 los
  tres; con el arreglo, los dos usan los tres. Con el arreglo, el modelo tiene además `m_IsLogicGate`
  y el reintento en vacío detrás del barrido, en los dos puntos, con su cadencia (su propio contador
  sobre las llamadas de PDQ), su tope, su rotación con `RemoveOrdered`, la máscara `LFPG_DIRTY_INPUT`
  y lo pendiente vuelto a contar después del reintento (§5.10, v6). Un fixture devuelve la entidad
  tocando solo el registro del modelo, sin encolar nada ni llamar a `MarkNodeDirty`, y el planificador
  sigue normal (Codex R22-V4-01).
- **Lo que devuelve el PDQ y quién lo lee (v6, Codex R22-V5-01):** el PDQ del modelo devuelve lo que
  queda en su cola al terminar, también en los dos puntos en vacío, y el modelo imita a sus dos
  lectores: el warmup se apaga con `remaining <= 0` (`LFPG_NetworkManagerImpl.c:2462-2468`) y la sonda
  asienta con `remaining <= 0` (`LFPG_PerfProbe.c:171`).
- **Admisión de cables (v6, Codex R22-V5-02):** el modelo monta los circuitos con las reglas del
  fuente y rechaza lo que el juego rechaza: solo puertos de salida declarados (`LFPG_WireOwnerBase.c:249`;
  el Splitter declara `output_1` a `output_3`, `LFPG_Splitter.c:53-60`), como mucho
  `LFPG_MAX_EDGES_PER_NODE` = 12 aristas por nodo (`LFPG_ElecGraphImpl.c:1476`,
  `LFPG_Defines.c:548`), ni ciclos ni cables a sí mismo (`LFPG_RPCServerHandlerImpl.c:509`,
  `LFPG_ElecGraphImpl.c:543`, `:1172-1175`). Un fixture que necesite saltárselo no vale, salvo las
  filas de mecánica del modelo (presupuesto de nodos, presupuesto de aristas y barrido del modelo), que
  prueban el planificador y no una red de partida y pueden insertar nodos y aristas directamente.
- **Oráculo de G2 (Codex, pregunta 2 de la v3):** tras cada vaciado de cola y diferidos, un cálculo
  independiente del modelo y de `ComputeSignalOut` aplica la tabla de §2.1 desde las fuentes con el
  estado de los dispositivos (interruptores, compuertas vivas, generadores, autoconsumos cubiertos y
  `LFPG_EvaluateGateLogic` sobre las señales del propio oráculo) y lo compara con cada arista
  habilitada. Se escribe en el test a partir de la tabla, sin llamar al fuente. Con el arreglo lo afirma
  cada fila de §6.4; en la base se anota.
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
| A1-A3 con Splitter: AND, OR, XOR recorriendo la tabla en secuencia sobre un mismo circuito (§6.3); lámpara = CeilingLight PT, consumo 10, capacidad 50; y, como en el banco de las 00:18, los tres circuitos en el mismo grafo, cada uno con su generador, con el AND oscilando mientras se recorren OR y XOR | rojo en (a). Con los tres circuitos como en el banco: A1 y A3 en rojo; A2 se anota, porque el modelo no reproduce el fallo del banco (allí el OR oscila 28 veces en 4 s, con 276 visitas en una llamada y 10 pendientes) y se mide en la fase 3 | latch = tabla, entrada de la lámpara >= 10 solo si la tabla da 1, como mucho 1 cambio de latch por combinación, en todos los órdenes, y G2 | lectura por flujo |
| **Transiciones** con Splitter: 10 -> 00, 11 -> 00 (los dos en la misma época) y 11 -> 00 -> 10, con una Memory Cell en Set a la salida del AND (Power aparte de 1, sin carga) | se anota | tras cada paso el latch = tabla; la celda nunca se activa en 11 -> 00 -> 10 salvo por el 11 inicial, que se resetea antes; ningún alto transitorio en la salida del AND en 00 -> 10 | dejar la tolerancia |
| **Latch viejo y carrera (Codex R22-V2-01 y R22-V3-02, Grok R22v3-01):** el escenario de §5.7 en (a), con la cola [B, AND, M] guionada, y en (b)-(d), incluida [AND, M, B] | se anota | en todos los órdenes, al vaciar: AND cerrado, la arista AND -> M.Set baja, G2, y el barrido no cambia el latch de M. M desactivada en (a) y en todo orden con B antes que AND; en los demás M puede quedar activada (L-RACE) y el test anota el resultado con el orden | publicar con `signalIn` y el latch (en (a)) |
| Latch viejo sin tocar cables: AND -> interruptor apagado -> M.Set, B off y el interruptor on en el mismo tick; y XOR -> interruptor -> M.Set con 10 -> 11 | se anota | en orden topológico (B, puerta, interruptor, M): M desactivada y ninguna lectura alta en Set; en los demás, L-RACE como arriba; al vaciar, G2 | publicar con `signalIn` y el latch |
| **Cambios separados por un vaciado (frontera de L-RACE):** B off y se vacía la cola; después se conecta AND -> M.Set. Lo mismo con el interruptor entre AND y Set, y con XOR 10 -> 11, vaciado, interruptor on | se anota | M desactivada en todos los órdenes; G2 | publicar con `signalIn` y el latch; bit escrito fuera de la pasada de su proveedor |
| **Registro sin la entidad, NetworkID con ella (Codex R22-V3-01):** el circuito de §5.7 en (a), y también AND 10 -> 11 con M en Set; `FindById` de la puerta falla en esa pasada y el NetworkID acierta | se anota | la puerta publica su lógica actual en esa misma pasada (11 -> 10: baja y M desactivada; 10 -> 11: alta y M activada en esa llamada) y queda registrada | pre-gate solo con `FindById` |
| **Entidad irresoluble y su vuelta (Grok R22v3-02; Codex R22-V4-01):** el circuito de §5.7 en (a) con los tres niveles fallando para la puerta en la pasada 11 -> 10 y en su Step 5; y el de Codex: G -> OR A.`input_0`, A -> OR B.`input_0`, B sin salida, con los tres niveles de A fallando hasta vaciar. Luego vuelve **solo el registro**, sin encolar nada ni tocar cables o interruptores, y el planificador sigue normal | se anota | sin entidad: las aristas de salida de la puerta bajas, M desactivada y B cerrado; al volver el registro, la puerta publica su lógica actual en como mucho 11 llamadas de PDQ (A -> B alta y B abierto) sin que el fixture encole nada, y el latch de la puerta recuperada (el AND de §5.7, A) es su tabla, no solo las aristas y B (v6, Grok R22v5-01); al vaciar, G2 | publicar con el latch cacheado (la v3); sin reintento |
| **Compuerta viva sin entidad que no es puerta lógica (Grok R22v4-02):** swA on y swB on -> AND (11) -> lámpara, en la reconstrucción con el PDQ directo del warmup, con `FindById` de swA fallando en esa pasada y sin su NetworkID; y D5 con el Laser abierto sin entidad durante una pasada | se anota | swA publica con su compuerta cacheada, como la energía: el AND sigue en 11 y la lámpara encendida; el Set sigue alto y el latch no cambia; al volver el registro, el reintento reprocesa swA y el Laser sin cambios; G2 | baja en todo `m_IsGated` (la v4) |
| **Puerta lógica resuelta con `Cast` nulo (Grok R22v4-03; Codex R22-V5-03):** AND (10) con `m_IsLogicGate`, `m_GateClosed` cacheado a true, `newOutput` 10 y, como entidad resuelta, un Splitter (no es `LFPG_LogicGateBase`), y detrás una Memory Cell en Set | n/a | señal baja, M desactivada y el AND pendiente; la energía reparte con la compuerta del Splitter, abierta: `allocAvail` = 10, como hoy con `FindById` (declarado en §5.10); cuando vuelve la entidad buena, su lógica y su latch; G2 | sin la rama de `Cast` nulo (publica `signalIn` con su compuerta) |
| **Reintento acotado:** 10 compuertas que no resuelven nunca y 1 que vuelve, con la cola vacía, con la que vuelve en cada una de las 11 posiciones de la lista (v6, Codex, pregunta 5 de la v5), y 200 llamadas en vacío más después de recuperarla (v6, Grok R22v5-02) | n/a | como mucho una ronda cada 10 llamadas de PDQ y 4 resoluciones por ronda, también después de recuperar, y ninguna encolada mientras no resuelven; en cada posición, la que vuelve se prueba en como mucho 3 rondas y se reprocesa en la llamada siguiente; mientras ninguna resuelve, el barrido corre en las mismas llamadas que sin reintentos | reintento sin tope, sin cadencia o sin rotación; rotación con `Remove(0)`; reintento que encola sin resolver |
| **Reintento y valor devuelto (v6, Codex R22-V5-01):** cola vacía, `m_GateRetry` = [A], A con compuerta y su entidad otra vez resoluble, con la ronda ya debida; en una llamada que empieza en vacío y en otra que vacía la cola; con el warmup activo y con la sonda esperando asentar | n/a | esa llamada devuelve 1, con A en la cola y su máscara con `LFPG_DIRTY_INPUT`; el warmup del modelo sigue activo y la sonda no asienta en esa llamada; la siguiente procesa A y devuelve 0 | contar lo pendiente antes del reintento |
| A1-A3 sin Splitter (control) | verde | verde, y asignaciones finales iguales a las de la base | regresión de energía |
| A4: XOR sin carga, A on, B off, con y sin Splitter, 600 ticks | **rojo obligatorio** (oscila); si el modelo no lo reproduce, el modelo está incompleto y se arregla antes de implementar | como mucho 1 cambio; puerta abierta con AX asignación 0 y señal alta | lectura por flujo |
| **Vecino ruidoso (GL-06), sin bucle (Codex R22-V4-02, Grok R22v4-01; árbol en v6, Codex R22-V5-02):** el juego no deja cerrar un bucle (§7), así que el ruido es externo: en el mismo grafo, la misma cola y el mismo contador de aristas que OR y XOR detrás de Splitter recorriendo la tabla, un interruptor de otro circuito, con generador propio, conmuta cada 500 ms y alimenta un árbol de Splitters de tres salidas, 4 niveles (1 + 3 + 9 + 27 = 40 Splitters), con 81 consumidores de relleno de 10 en las hojas: 123 nodos con el generador y el interruptor, y 122 cables. Con el generador de 50 el árbol queda en sobrecarga (B5, estable sin puertas), y el ruido lo dan la señal y la demanda, que cambian en cada conmutación. Se monta con la admisión de §6.3. El cruce del presupuesto se mide, no se supone: la traza muestra que cada conmutación procesa al menos 65 nodos del ruido, repartidos en dos llamadas de PDQ o más. El ruido sigue mientras se recorre la tabla y se para antes del vaciado final. En la base, el vecino del banco es el AND oscilando de la fila A1-A3 | se anota | mientras sigue el ruido, tras cada cambio de entrada el latch de OR y de XOR llega a su tabla en como mucho 5 ticks, con como mucho un cambio por combinación, y la traza muestra nodos del ruido y del circuito en las mismas llamadas de PDQ y al menos una llamada que acaba con nodos del circuito aún en cola; al parar el ruido, cola vacía en como mucho 100 ticks y G2 | lectura por flujo; un arnés con colas o contadores separados; un ruido que no cruza el presupuesto |
| B1: XOR -> Door Controller (CONSUMER) detrás de Splitter; A on, B on, A off, B off | se anota: en partida, en BASE, la puerta llegó tarde y la secuencia quedó invertida sin vecino oscilando (0 «Requeue limit»); el modelo no lo reproduce, y aquí el Door Controller no tiene el retardo de la puerta física; se mide en la fase 3 con trazas | después de cada paso, puerta = XOR(A,B) y Door Controller alimentado solo si da 1, con como mucho un cambio de latch del XOR y un cambio de alimentación del Door Controller por paso; tras B off, latch cerrado | lectura por flujo; dejar la tolerancia |
| D5: Power por Splitter; Power directo; Laser -> interruptor encendido -> Set | rojo: Set nunca alto | el Laser abre: Set alto, latch true, Output habilitada, lámpara >= 10, Power alto; el Laser cierra: latch se queda; pulso de Reset (2000 ms): latch false y lámpara 0. Se admite XM = 0 | lectura por flujo de Set; compuerta cacheada |
| D5 sin Power (solo Laser) | verde (medido así) | verde | control |
| Laser con entrada < 5 y compuerta abierta | n/a | señal baja en Set | quitar la condición de autoconsumo |
| **Salida rehabilitada (Codex R22-01):** celda A con Output deshabilitada y bit alto antiguo; A pierde alimentación; un Set la activa y rehabilita Output; la celda B (Set desde Output de A, Power aparte) va en la cola antes del reproceso de A | se anota | B no se activa; mientras A no se reprocesa, el lector ve baja | no limpiar el bit al cambiar el flag |
| **Q -> Toggle (Grok R22v2-02):** Output de A -> Toggle de B, Power de B aparte; rearme de 1000 ms ya cumplido; A pasa de desactivada a activada, y luego de activada a desactivada; reloj por eventos (§6.3) | se anota | A off -> on: B voltea exactamente una vez; A on -> off: B no voltea; según la traza, el routing de 50 ms cae entre dos PDQ | reloj de 100 ms que junta el routing de 50 ms con el PDQ |
| **Salida rehabilitada con retraso (§5.8; Codex R22-V3-03):** Output de A -> Toggle de B, Power de B aparte y estable, rearme cumplido; A activada con Set alto; Set de A a 0 y pulso de Reset de 100 ms, luego Set a 1 y Reset a 0, para que Q vaya 1 -> 0 -> 1 y Output se deshabilite de verdad (con Set sostenido, Reset no quita Q); (i) carga normal; (ii) con la cola llena de nodos pendientes para que la pasada de A llegue más de 1000 ms tarde | se anota | el test afirma la deshabilitación y las lecturas de B antes de contar; (i) B no voltea; (ii) B lee baja 1100 ms o más y voltea exactamente una vez, que es la limitación declarada en §5.8 | - |
| C con Splitter: pulsador en Toggle (pulso 2000 ms, presses cada 3 s), en Set y en Reset | verde | verde: un flip por pulsación; Set y Reset ponen y quitan | regresión C1-C7 |
| C con Splitter y cola congestionada (Codex, pregunta 4 de la v3 y de la v4): C1-C7 como en C, con C5 (corte de Power), C6 (reinicio: reconstrucción y warmup) y C7 (Set mantenido) escritos en la fila, y con el árbol del vecino ruidoso (40 Splitters y 81 consumidores, generador propio; v6, Codex R22-V5-02) conmutado justo antes de cada pulsación, para que sus nodos estén pendientes cuando llega el pulso, con el cruce medido como en esa fila (si cuelgan del mismo generador de 50 que C es B5, no congestión) | verde | lo mismo que C | regresión C1-C7 con presupuestos |
| Sobrecarga: G 50 -> Splitter; swA on -> XOR.in0, swB off -> in1, XOR -> lámpara 10; lámpara de 60 en la tercera salida | se anota | todas las asignaciones 0 (B5 intacto), latch estable (como mucho 1 cambio en 600 ticks), AX con señal alta | acoplar señal a sobrecarga |
| Corte real: G apagado; cable swA -> XOR cortado; Output de la celda deshabilitada | n/a | cuando la propagación llega, los lectores leen baja; las aristas habilitadas quedan con asignación 0 y señal baja; en las deshabilitadas **no** se exige asignación 0 (el código la conserva), sí señal baja; al volver G, las tablas como antes | retener señal; ignorar `LFPG_EDGE_ENABLED` |
| **Cadena larga (Codex R22-03):** G -> 64 Splitters en cadena -> interruptor -> XOR (sin carga) y -> pulsador -> Toggle de una celda | n/a | al apagar G el último tramo se procesa en ticks posteriores (presupuesto); cuando llega, lecturas bajas; ningún flip de la celda ni cambio de latch de más; al volver G, la celda no se voltea sola. No prueba por sí sola los presupuestos: para eso, las dos filas siguientes | no respetar presupuestos |
| **Presupuesto de nodos (Codex R22-V2-02):** 65 CONSUMER pendientes, cada uno con una entrada y sin lecturas en su callback | verde: es del modelo y no depende del arreglo | una llamada procesa 64, cuenta 64 visitas y deja 1 en la cola | modelo sin el límite de nodos |
| **Presupuesto de aristas:** nodos pendientes con varias aristas, de forma que el contador cruce 256 dentro de un nodo | verde: es del modelo | ese nodo se termina, el contador acaba por encima de 256, no se toma ningún nodo más y los que quedan siguen en su orden | modelo sin el límite de aristas; modelo que corta dentro de un nodo |
| **Barrido del modelo (Codex, pregunta 3 de la v3):** nodos en cola; uno sucio fuera de la cola; uno con `m_Powered` desajustado y otro ajustado | verde: es del modelo | mientras quedan nodos en cola no corre; una llamada que vacía la cola lo corre al final de esa misma llamada (Grok, pregunta 5 de la v4); con la cola vacía, salta el sucio y solo llama al callback del desajustado, con `NodeEntitySyncUnchanged` como en el fuente | barrido incondicional en el modelo |
| Arranque y reconstrucción: todas las aristas nacen con señal false, con la puerta procesada **antes** que sus proveedores y también **entre** ellos (A, puerta, B), con PDQ directo sin avance de reloj | n/a | A3 y D5 llegan a su estado estable con como mucho un cierre y una reapertura de la puerta, y sin activar ninguna celda aguas abajo | señal inicial true; dejar la tolerancia |
| Invariante y G2 en todos los estados estables de todos los fixtures, sin reintentos pendientes | n/a | asignación > EPS implica señal alta; cada arista habilitada = oráculo | - |

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
- S13: el pre-gate llama a `ResolveGatedEntity`, que hace `FindById`, `ResolveVanillaDevice` y
  NetworkID en ese orden y registra la entidad si la encuentra por NetworkID.
- S14: en Step 2b, con `node.m_IsLogicGate` y sin entidad resuelta o con `Cast` nulo,
  `signalOut = false` y `QueueGateRetry(nodeId)` antes de `AllocateOutput`; con `node.m_IsGated`, sin
  `m_IsLogicGate` y sin entidad, `QueueGateRetry(nodeId)` sin tocar `signalOut`. Ninguna de las dos
  ramas escribe `gateIsClosed` ni `allocAvail`, y ninguna da baja por mirar solo `m_IsGated`.
- S15: `LFPG_ElecNode` declara `m_IsLogicGate` y lo pone a false; `EnsureNode` y
  `PopulateAllNodeElecStates` lo ponen con `LFPG_LogicGateBase.Cast(obj)` justo después de
  `m_IsGated`.
- S16: `RetryUnresolvedGates();` va justo después de `ValidateConsumerStates(edgeBudget);` en los dos
  puntos en vacío; vuelve sin hacer nada si `m_ValidateTickCount - m_LastGateRetryTick <
  LFPG_CONSUMER_VALIDATE_TICK_INTERVAL` y, si no, pone `m_LastGateRetryTick = m_ValidateTickCount;`
  (v6); recorre como mucho `LFPG_GATE_RETRY_PER_CALL` entradas, sacadas con `RemoveOrdered(0)` y nunca
  con `Remove(` (v6); usa `ResolveGatedEntity`; al resolver descarta la entrada y llama
  `MarkNodeDirty(nodeId, LFPG_DIRTY_INPUT)` (v6); al no resolver la vuelve a meter al final con
  `Insert`; descarta las de nodos que no existen. `RebuildFromWires` vacía `m_GateRetry`. `LFPG_ElecGraphImpl`
  declara `m_LastGateRetryTick` y el constructor lo pone a 0.
- S17 (v6, Codex R22-V5-01): en los dos puntos en vacío, entre `RetryUnresolvedGates();` y la llamada
  a `LFPG_PerfProbe.OnProcessDirtyQueue`, lo pendiente se vuelve a calcular con
  `m_DirtyQueue.Count() - m_DirtyQueueHead`, y ese valor es el que reciben la sonda y el `return`; en la
  rama que empieza en vacío ya no queda `OnProcessDirtyQueue(0,` ni `return 0;`.

### 6.6 Mutantes (sobre el texto del fuente, como `test_memorycell_toggle.py:507-514`)

Cada uno tiene que hacer fallar una aserción de comportamiento (F) o estructural (S). Un error de
extracción, un método ausente o una excepción del arnés no cuentan.

| Mutante | Tiene que morder |
|---|---|
| Puertas de vuelta a `IsPortReceivingPower` | F: A3 con Splitter, A4, B1 |
| Set de la celda de vuelta a flujo | F: D5 |
| Power de la celda a señal | F: señal presente y Power con asignación 0, Toggle no tiene permiso |
| Escribir la señal solo dentro de `if (!overloaded)` | S1, y F en sobrecarga |
| Quitar `m_SignalChanged` de Step 3 | S2, y F en dos casos donde la instrumentación de §6.3 afirma, en la pasada del interruptor, que los otros cinco disparadores están apagados y `m_SignalChanged` encendido: (i) XOR sin carga con `input_1` sin cable, detrás de Splitter -> swA: al apagar G, la pasada de swA (entrada 0 -> 0, demanda 0 -> 0, AX 0 -> 0) es el único camino al XOR, que con el arreglo acaba cerrado y con el mutante acaba abierto **en todos los órdenes** (Grok R22v2-01; Codex, pregunta 4 de la v2: con swB conectado, su sonda cambia 1 -> 0 y despierta al XOR según el orden); (ii) Laser -> interruptor -> Set al abrir el Laser, en la pasada del interruptor. No vale D5 con el Laser directo en Set: su transición de compuerta ya pone `m_AllocChanged` (`:2688-2690`). En (i) se afirman además, en esa pasada, `inputChanged` apagado en el XOR e `inputSum` 0 -> 0 (Grok R22v3-04), y las precondiciones: XOR asentado, `input_1` sin cable, sin salida, y `m_Powered` false con flujo 0, para que el barrido no lo despierte (Codex, pregunta 5 de la v3) |
| Quitar `m_SignalChanged` del reproceso en la misma época | S2, y F de convergencia en (a) |
| `ComputeSignalOut` con `node.m_GateClosed` | S5, y F: primera pasada abierta de D5 |
| `ComputeSignalOut` sin la condición de autoconsumo | F: Laser con entrada < 5 |
| Señal atada a `m_Overloaded` | F: sobrecarga |
| Step 1 sin filtro de habilitada para la señal | S4, y F: Output de la celda deshabilitada |
| `IsPortReceivingSignal` sin filtro de puerto | S6, y F: XOR (1,0) leído como (1,1) |
| Constructor con `m_SignalHigh = true` | S8, y F: arranque |
| No limpiar el bit en `SetOutputPortEnabled` | S10, y F: salida rehabilitada |
| Volver a poner la tolerancia de un apagón (el bloque de 592ebcc) | S11, y F: transiciones 10 -> 00 (puerta abierta con 00) y 11 -> 00 -> 10 (celda activada), y B1 tras B off |
| Quitar la rama de puerta (la puerta publica con `signalIn` y el latch) | S12, y F: latch viejo y carrera en (a), y latch viejo sin tocar cables en orden topológico |
| Modelo sin el límite de nodos (solo arnés) | F: presupuesto de nodos |
| Modelo sin el límite de aristas, o que corta dentro de un nodo (solo arnés) | F: presupuesto de aristas |
| Pre-gate solo con `FindById` | S13, y F: registro sin la entidad (10 -> 11: M activada en esa llamada) |
| Sin entidad, publicar con el latch cacheado (la v3) | S14, y F: entidad irresoluble |
| Sin reintento en vacío (no llamar a `RetryUnresolvedGates`) | S16, y F: entidad irresoluble y su vuelta (A -> B sigue baja con el registro de vuelta) |
| Baja en todo `m_IsGated` sin entidad (la v4) | S14, y F: compuerta viva sin entidad |
| Sin la rama de `Cast` nulo | S14, y F: `Cast` nulo |
| Reintento sin tope por ronda, sin cadencia (una ronda en cada llamada en vacío) o sin rotación | S16, y F: reintento acotado (el tope y la cadencia se miden también después de recuperar) |
| Rotación con `Remove(0)` en vez de `RemoveOrdered(0)` (v6) | S16, y F: reintento acotado en alguna de las 11 posiciones |
| Reintento con `LFPG_DIRTY_INTERNAL` (la v5) | S16. En F no muerde con el modelo fiel: el Step 5 que falló borró el registro (`:3117`) y la pasada del reintento escribe igual (§5.10); el test lo anota |
| Contar lo pendiente antes del reintento (el `return 0;` y el `remaining` de hoy) (v6) | S17, y F: reintento y valor devuelto |
| Modelo que escribe el bit de un interruptor al encolarlo (solo arnés) | F: carrera en [AND, M, B], porque la traza tiene que mostrar que AND leyó alto en B |
| Modelo con barrido incondicional (solo arnés) | F: barrido del modelo |

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
  en `LFPG_ElecEdge`; en `ResolveGatedEntity`, las mismas llamadas y miembros que `SyncNodeToEntity`
  usa en `:3079-3111`; en `QueueGateRetry` y `RetryUnresolvedGates`, `m_GateRetry` y
  `m_LastGateRetryTick` declarados en la clase, `m_ValidateTickCount`,
  `LFPG_CONSUMER_VALIDATE_TICK_INTERVAL`, `MarkNodeDirty`, `LFPG_DIRTY_INPUT`, y `RemoveOrdered` e
  `Insert` de `array<string>` (v6); en la rama que empieza en vacío, `idleRemaining` no repite ningún
  nombre de local del método, porque Enforce no deja redeclarar (v6); `node.m_IsLogicGate` en `LFPG_ElecNode`;
  `LFPG_GATE_RETRY_PER_CALL` en `LFPG_Defines.c`; `LFPG_LogicGateBase` visible desde 5_Mission).

## 7. Riesgos del brief, uno a uno

- **B5 y sondas cerca del límite.** No se añade demanda, el all-off no cambia y la señal se escribe
  fuera de `!overloaded`. La lógica viva durante la sobrecarga es criterio de producto (§2.1).
  Fixture de sobrecarga y prueba en partida.
- **Regresión en lo que no es lógica.** Las fórmulas de energía no cambian. El único cambio de
  planificación es `m_SignalChanged`, que solo salta cuando un bit cambia. En redes sin lógica un bit
  cambia a la vez que `outputDelta`, `m_AllocChanged` o `inputChanged` (encender, apagar, compuerta,
  topología), así que esa propagación ya ocurría. Suites existentes sin cambiar expectativas.
- **Convergencia.** El bit de cada arista es función de los bits de arriba y de las compuertas, y el
  jugador no puede cerrar un bucle: el cableado rechaza un ciclo antes de crear el cable
  (`LFPG_RPCServerHandlerImpl.c:509`, con `DetectCycleIfAdded`, `:1172-1175`) y `OnWireAdded` rechaza el
  de un dispositivo a sí mismo (`:543`; Codex R22-V4-02). La propagación está acotada por
  `m_RequeueCount` (5), los presupuestos y los diferidos. Tests: como mucho 100 ticks por transición y
  colas vacías, con presupuestos.
- **Coste por tick.** Un bool por arista y otro por nodo; lecturas y escrituras dentro de bucles que ya existen; un
  lector nuevo que llaman los callbacks de puertas y celdas y, dos veces por pasada, la rama de puerta
  de Step 2b (unas cuatro visitas de arista por pasada de puerta, contadas en el presupuesto). Ni bucles nuevos en
  el camino normal, ni temporizadores, ni búsquedas de entidad nuevas por tick con el registro sano:
  los dos niveles nuevos del pre-gate solo corren cuando `FindById` falla, y el reintento en vacío solo
  con nodos pendientes (un array de ids, como mucho 4 resoluciones por ronda, una ronda cada 10
  llamadas como mucho, y ningún encolado mientras no resuelven). Offline no se mide. En partida, con un PBO
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
- Criterios: los del brief, y además ningún «Requeue limit reached» en estado estable con el arreglo.
- **Vecino ruidoso (GL-06):** OR y XOR detrás de Splitter recorriendo la tabla, primero solos en el
  mundo y luego con ruido en otro circuito: en BASE, el AND en 11 oscilando, como en el banco; con el
  arreglo, que ya no oscila, un interruptor de otro circuito conmutado cada 500 ms por el puente MCP,
  con el árbol de §6.4 detrás (40 Splitters y 81 consumidores con generador propio, 122 cables; v6,
  Codex R22-V5-02). Con el `MaxWiresPerPlayer` del banco por encima de los cables del árbol y del
  circuito medido (128 por defecto, `LFPG_Settings.c:63`), o montado por un camino del servidor que no
  cuente cables por jugador. Si el banco no puede montarlo entero, se dice y se mide con el mayor árbol
  que quepa, anotando nodos y llamadas de PDQ por conmutación. Sin bucles: el juego no deja montarlos.
  BASE y arreglo, con trazas: «Requeue limit» por nodo, y nodos y visitas por llamada con
  `LFPG_PERF_PROBE`.
- **B1 con trazas:** `LFPG_LOG_LEVEL` 2 para `[LogicGate]` y `[LFPG_DoorController] SetPowered`,
  contando los cambios de alimentación del Door Controller por paso, en BASE y con el arreglo.
- L-RACE no se prueba en partida: no se pueden meter dos cambios en la misma décima de segundo de forma
  repetible.
- La vuelta de una entidad (§5.10) tampoco: necesita inyectar el fallo de los tres niveles. Se queda
  en los tests (§6.4).
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
- **GL-06.** Un circuito que oscila podría retrasar a otros del servidor por el presupuesto compartido
  de la cola. Indicios en el banco de las 00:18: con el AND oscilando (1400 «Requeue limit» entre
  00:18:00 y 00:19:45), el OR osciló 28 veces en 4 s, con 276 visitas en una llamada y 10 pendientes.
  Sin demostrar. Con el arreglo las puertas no dependen del flujo, pero un consumidor que lee flujo sí
  podría notarlo. Se mide en la fase 3 (§8), con ruido externo y sin bucles, que el juego no deja
  montar.
- **GL-07.** Set y Reset de la Memory Cell son de nivel y sin filtro: un alto pasajero de L-RACE, o de
  un cambio que llega por dos caminos, puede activar la celda. Candidato para otra ronda: el filtro de
  400 ms de Toggle también en Set y Reset, después de medir cuánto mantiene el Laser su compuerta
  abierta (`LFPG_LaserDetector.c`), para no perder a quien lo cruza deprisa.
- **GL-08.** `Register` no avisa al grafo cuando una entidad vuelve. El reintento de §5.10 lo cubre
  para los nodos con compuerta. Un nodo sin compuerta cuya entidad falla en Step 5 sigue como hoy: su
  dispositivo no se sincroniza hasta la siguiente pasada del nodo, y el barrido no lo despierta si su
  alimentación ya coincide (Codex R22-V4-01). Sin medir.
- **GL-09 (v6, Grok R22v5-03).** Lo que `EnsureNode` copia del objeto al crear el nodo (`m_DeviceType`,
  capacidad, consumo, `m_IsGated` y, con el arreglo, `m_IsLogicGate`) solo se vuelve a leer en
  `PopulateAllNodeElecStates` (`:3705`, al reconstruir): si el nodo ya existe, `EnsureNode` vuelve sin
  tocarlo (`:1368-1372`), así que un nodo que nació sin objeto o con otro objeto se queda así hasta la
  siguiente reconstrucción. Pasa hoy con `m_IsGated` y con el tipo; `m_IsLogicGate` sigue la misma vida
  para no separarse de ellos. Sin medir.
- **TA3-01** sigue abierto.

## 10. Sin R22 de la v6: lo que la R21 tiene que mirar además

Los cambios de la v6 son los arreglos que propusieron los propios revisores de la v5 (§11.5), así que
no hay otra R22. La R21 del código (Codex; Grok también si hay cuota), además de lo de siempre y sin
ver la revisión del otro, comprueba en el diff y en el test, uno a uno:

1. El reintento marca con `LFPG_DIRTY_INPUT`, y la fila de la vuelta afirma el latch de la puerta
   recuperada, no solo las aristas y B.
2. La cadencia: una ronda como mucho cada `LFPG_CONSUMER_VALIDATE_TICK_INTERVAL` llamadas, con
   `m_LastGateRetryTick` sobre `m_ValidateTickCount`; la rotación con `RemoveOrdered(0)` e `Insert`; y
   el tope medido también después de recuperar.
3. Lo pendiente se vuelve a contar después del barrido y del reintento en los dos puntos en vacío,
   antes de la sonda y del `return`, y la fila del valor devuelto lo afirma contra el warmup y la sonda
   del modelo.
4. El árbol del vecino ruidoso y de la C congestionada pasa la admisión del modelo con los límites del
   fuente y cruza el presupuesto medido en la traza.
5. La fila de `Cast` nulo usa un Splitter y afirma `allocAvail`.
6. El reintento acotado prueba las 11 posiciones.

Veredicto APTO / APTO CON CAMBIOS / NO APTO. Por hallazgo: id, gravedad, `path:line`, escenario de
fallo concreto y arreglo propuesto. Un hallazgo BLOQUEA si rompe G1 o G2 tal como quedan en §2.1, un
caso del brief (A1-A4, B1-B2, C1-C7, D5) o el escenario de Nostra. Lo que caiga dentro de una
limitación declarada (L-RACE, §5.8, nodo con compuerta sin entidad, B5) se da como nota, salvo que se
demuestre que contradice el brief o el reporte.

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

### 11.3 R22 de la v3

| Hallazgo | Decisión | Dónde |
|---|---|---|
| Grok R22v3-01 (BLOQUEA) y Codex R22-V3-02 (IMPORTANTE): con [AND, M, B] la celda se activa | se declara la garantía, con la opción de Codex de transitorios permitidos y acotados: L-RACE, G1 y G2; se prueban la frontera y G2 en todos los órdenes. Descartados el orden topológico y el Set que ignora un alto no reescrito (§5.9) | §0, §2.1, §5.9, §6.4, §9 GL-07 |
| Codex R22-V3-01 (BLOQUEA) y Grok R22v3-02 (IMPORTANTE): entidad sin resolver en el pre-gate | aceptado: tres niveles en el pre-gate, como Step 5, y sin entidad señal baja en todo nodo con compuerta; la energía, como hoy | §2.1, §3.1, §4, §5.10, §6.4, §6.5 S13-S14, §6.6 |
| Grok R22v3-03 (IMPORTANTE): el barrido fija el alto en M | sin cambio en producción: el barrido solo corre con la cola vacía y salta los nodos en cola o sucios, y el alto pasajero solo existe con la puerta pendiente; el modelo lo imita así y las filas de carrera lo afirman | §5.11, §6.3, §6.4 |
| Grok R22v3-04 (MENOR): ancla (i) | aceptado: `inputChanged` e `inputSum` 0 -> 0 del XOR en la pasada de swA | §6.6 |
| Codex R22-V3-03 (IMPORTANTE): el fixture de §5.8 no deshabilita Output | aceptado: Set a 0 durante el Reset y se afirma la deshabilitación; (i) ningún flip, (ii) exactamente uno | §6.4 |
| Codex, pregunta 2: el invariante no ve los altos falsos | aceptado: oráculo de G2 tras cada vaciado | §2.1, §6.3, §6.4 |
| Codex, pregunta 3: el barrido es condicional | aceptado en el modelo, con su fila | §6.3, §6.4 |
| Codex, pregunta 4: C1-C7 con congestión | aceptado: fila de C con la cola congestionada | §6.4 |
| Codex, pregunta 5: precondiciones del ancla (i) | aceptado | §6.6 |
| Grok, defectos que los tests no verían: bit escrito al encolar; routing de 50 ms y PDQ en el mismo ms | aceptado: el bit solo lo escribe su proveedor, con traza de lecturas y del reloj, y un mutante del arnés | §6.3, §6.4, §6.6 |
| Registros del banco (PC, 00:18-00:27): el AND oscilaba durante OR y XOR; B1 sin vecino | A1 y A3 con los tres circuitos como en el banco; A2 y B1 anotadas y a la fase 3 con trazas; fila de vecino ruidoso con el arreglo | §6.4, §8, §9 GL-06 |

### 11.4 R22 de la v4

| Hallazgo | Decisión | Dónde |
|---|---|---|
| Codex R22-V4-01 (BLOQUEA): la señal no se recupera cuando la entidad vuelve sin que nada ensucie el nodo | aceptado: reintento en vacío detrás del barrido, como mucho 4 por llamada y con rotación; al resolver, `MarkNodeDirty`; el fixture devuelve solo el registro y deja correr el planificador. Descartados reencolar cada tick (ocupa la cola y para el barrido) y avisar desde `Register` | §0, §2.1, §3.1, §4, §5.10, §5.11, §6.3-6.6, §7, §9 GL-08 |
| Grok R22v4-02 (IMPORTANTE): la baja sin entidad alcanza a interruptores y Laser vivos | aceptado: baja solo en puertas lógicas (`m_IsLogicGate`, cacheado al crear el nodo); el resto usa la compuerta cacheada, como la energía, y también queda pendiente de reintento. Matiz: tras `RebuildFromWires` el mapa NetworkID no queda vacío, porque `:289` lo vacía y `EnsureNode` lo vuelve a llenar para cada dispositivo del registro (`:1382-1392`); la nota de `:1685-1688` habla de los nodos huérfanos que ya no se crean. La fila de §6.4 cubre igualmente el caso sin NetworkID | §2.1, §3.1, §5.10, §6.4, §6.5 S14-S15, §6.6 |
| Grok R22v4-03 (MENOR): entidad resuelta y `Cast` nulo | aceptado: en una puerta lógica cuenta como sin entidad | §2.1, §3.1, §6.4, §6.5 S14, §6.6 |
| Codex R22-V4-02 (IMPORTANTE) y Grok R22v4-01 (IMPORTANTE): el vecino ruidoso no se puede montar o no da un resultado verdadero | aceptado: sin bucle, que el juego no deja montar; ruido externo de un interruptor cada 500 ms con relleno que cruza el presupuesto, en la misma cola y el mismo contador; avance en como mucho 5 ticks mientras dura el ruido, y G2 al pararlo. No se reserva presupuesto para el circuito medido: taparía justo lo que GL-06 quiere ver | §6.4, §7, §8, §9 GL-06 |
| Codex, pregunta 4 de la v4: tres niveles fallando no prueban que el dispositivo no exista; y el cambio de energía con NetworkID | aceptado: §5.10 ya no lo afirma; el cambio de energía sigue declarado y usa el valor vivo | §5.10 |
| Codex, pregunta 4 de la v3 (parcial en la v4): C5-C7 con congestión | aceptado en la fila de C congestionada, con el relleno en un generador propio | §6.4 |
| Codex, pregunta 5 de la v4: una traza del modelo no prueba que reproduzca el fuente | de acuerdo: por eso las estructurales de §6.5 atan el cableado al fuente, Codex revisa el código (R21) y la fase 3 mide en partida | §6.5, §8 |
| Grok, pregunta 5 de la v4: el barrido corre al final de una llamada que vacía la cola | aceptado en el modelo y en su fila | §6.3, §6.4 |
| Grok, defectos de la v4: linter y `m_SignalHigh`; temporizador y PDQ en el mismo ms; L-RACE fuera de partida | el barrido manual de §6.7 ya lo cubre; la traza tiene que mostrar los dos órdenes; L-RACE sigue declarado fuera de la fase 3 | §6.3, §6.7, §8 |
| Grok, premisa: G1 se redefinió para excluir la carrera | es una decisión de producto, comunicada a Guillermo el 2026-11-09 como limitación declarada (L-RACE) | §2.1 |
| Grok, premisa: el alto pasajero con B sin ensuciar | aceptado: §5.11 lo ata al cambio que lo deshace | §5.11 |
| PC, fixture: C congestionada en rojo con el relleno colgando del mismo generador de 50 | correcto: era B5; el relleno va en un generador propio | §6.4 |

### 11.5 R22 de la v5

| Hallazgo | Decisión | Dónde |
|---|---|---|
| Grok R22v5-01 (BLOQUEA): el reintento marca con `LFPG_DIRTY_INTERNAL`, Step 5 se salta el callback y el latch de la puerta no vuelve a su tabla | aceptado el arreglo: `LFPG_DIRTY_INPUT`, y la fila de la vuelta afirma el latch de la puerta recuperada. Matiz: en el fuente el escenario no llega a darse, porque el Step 5 que no encuentra la entidad borra su último registro (`:3117`) y la siguiente escritura con entidad no se salta (`:2987-2988`); la máscara quita esa dependencia y el modelo imita ese borrado | §3.1, §5.10, §6.3-6.6 |
| Grok R22v5-02 (IMPORTANTE): reintento eterno, 4 resoluciones en cada llamada en vacío | aceptado: la cadencia del barrido, como mucho una ronda cada 10 llamadas, con su propio contador; sin tope de edad, que dejaría sin recuperar una entidad que vuelve tarde (R22-V4-01). La fila mide también después de recuperar | §3.1, §5.10, §5.11, §6.4-6.6, §7 |
| Grok R22v5-03 (MENOR): `m_IsLogicGate` no se refresca si el nodo ya existía | rechazado: es la vida de `m_IsGated`, del tipo, la capacidad y el consumo, que tampoco se refrescan hasta reconstruir; refrescar solo el flag nuevo lo separaría de ellos. Al bug-ledger | §9 GL-09 |
| Codex R22-V5-01 (IMPORTANTE): el PDQ devuelve 0 justo después de que el reintento encole | aceptado: lo pendiente se vuelve a contar después del barrido y del reintento, antes de la sonda y del `return`; cuenta también lo que ya podía encolar un callback del barrido | §3.1, §4, §5.10, §5.11, §6.3-6.6 |
| Codex R22-V5-02 (IMPORTANTE): 65 consumidores en un Splitter de tres salidas | aceptado: árbol de 40 Splitters y 81 consumidores, montado con la admisión del fuente en el modelo; el cruce del presupuesto se mide en la traza. También en la C congestionada y en la partida | §6.3, §6.4, §8 |
| Codex R22-V5-03 (MENOR): con `Cast` nulo la energía no usa la compuerta cacheada | aceptado como declaración: reparte con la compuerta de la entidad resuelta, como hoy cuando `FindById` la devuelve; la fila usa un Splitter y afirma `allocAvail` | §2.1, §5.10, §6.4 |
| Codex, pregunta 5 de la v5: el reintento acotado tiene que probar todas las posiciones | aceptado: las 11 posiciones, y la rotación con `RemoveOrdered(0)`, porque `Remove(0)` de Enforce llena el hueco con el último y reproduce justo el fallo que Codex simuló | §3.1, §5.10, §6.4-6.6 |
| Grok, premisa de la v5: `MarkNodeDirty(INTERNAL)` no garantiza una pasada que actualice el dispositivo | resuelto con `LFPG_DIRTY_INPUT` | §5.10 |
| Grok, premisa: el pendiente se quita antes de procesar y G2 se mira tras la pasada siguiente | de acuerdo: con lo pendiente bien contado, la llamada del reintento ya no da la cola por vacía, y G2 se mira tras el vaciado siguiente | §2.1, §6.4 |
| Grok, pregunta 1: un nodo con compuerta que nunca entra en `m_GateRetry` | sin cambio: tras una reconstrucción todos los nodos pasan por la cola y el que vuelve a fallar entra otra vez; los nodos sin compuerta son GL-08, y los que nacen con el objeto equivocado, GL-09 | §9 |
| Codex, premisa: ceil(n / 4) cuenta llamadas en vacío, no tiempo, con la cola siempre ocupada | ya declarado en §5.10; se añade que la ronda va como mucho cada 10 llamadas | §5.10 |
| PC (sesión del banco): aplicar los arreglos sin otra R22 y que la siguiente validación sea la R21 sobre el diff, con el test en verde | aceptado: la R21 tiene la lista de §10 | §10 |
