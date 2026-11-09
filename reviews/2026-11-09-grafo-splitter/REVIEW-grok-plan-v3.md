<!-- Salida literal de Grok (solo lectura, sesion nueva, a ciegas). Quitado solo el preambulo previo al veredicto. -->

NO APTO

**R22v3-01 — BLOQUEA — publicar `LFPG_EvaluateGateLogic` de las entradas actuales no impide un Set falso si la puerta corre antes que el proveedor que cae**  
`scripts/5_Mission/LFPG_ElecGraphImpl.c:2247-2272`, `:2786-2822`, `:2884`; `scripts/4_World/LFPG_LogicGate.c:119-128`, `:141-145`; `scripts/4_World/LFPG_MemoryCell.c:235-242`; PLAN §2.1, §5.7, §6.4 «Latch viejo».

§5.7 solo analiza la cola `[B, AND, M]`: B ya escribió baja, AND lee `(1,0)`, publica `AND(1,0)=false`. En cualquier orden donde AND (o M) corra **antes** de B, las aristas de entrada de AND siguen con `m_SignalHigh=true` de la época estable 11.

Valores: G 50 → Splitter → swA/swB → AND abierto en 11, sin carga; M desactivada, Power aparte = 1. Mismo tick: B off y cable nuevo AND → M.Set (nace en false). Cola `[AND, M, B]` (LIFO de `[B, AND, M]`, u orden FIFO `[AND, B, M]`):

1. AND pasa con bits `(1,1)` → `signalOut=true` en la arista nueva (`EvaluateGateLogic` igual que el latch).
2. M lee Set alto → `hasSet && !hasReset` pone `m_LatchState=true` (`MemoryCell.c:235-238`). Set es de nivel: bajar Set después **no** resetea.
3. B publica baja, `m_SignalChanged` reencola AND; AND republica false. M relee Set bajo y se queda activada.

El testigo de Codex (AND false, M true) se reproduce con el arreglo de la v3. Las permutaciones (b)–(d) de §6.4 o fallan (si el modelo es fiel) o se vuelven gameables (si el modelo aplica el apagón de B a las aristas antes de procesar B).

Arreglo: orden topológico de señal en la misma época (un lector de Set/Reset/Toggle no se toma hasta que todos sus proveedores de señal de esta época hayan escrito), o Set que ignore un alto que el proveedor aún no ha reescrito en esta época; no basta con `EvaluateGateLogic` en la pasada de la puerta.

**R22v3-02 — IMPORTANTE — entidad de puerta no resuelta vuelve al latch cacheado y republica el alto 11→10**  
`LFPG_ElecGraphImpl.c:2483-2503`, `:2642-2646`; PLAN §2.1 fila «Si la entidad no se resuelve».

`FindById` fallido: `gateEntityResolved=false`, no hay `Cast` a `LFPG_LogicGateBase`, `signalOut` queda la fila PASSTHROUGH (`signalIn` y `node.m_GateClosed`). AND 11→10 con `signalIn` aún true y cache abierto publica alto; M.Set lo captura. El barrido en vacío (`ValidateConsumerStates` `:3565`, `dirtyMask=0`) no reescribe `m_SignalHigh`.

Arreglo: si `m_IsGated` y el tipo de nodo es puerta lógica, no publicar alto sin entidad resuelta (fail-closed: `signalOut=false`); fixture con `resolvedEnt=null` en la pasada 11→10.

**R22v3-03 — IMPORTANTE — el barrido en vacío llama `SetPowered` sin `DIRTY_INPUT` y puede asentar un latch de M que el PDQ aún no ha cerrado**  
`LFPG_ElecGraphImpl.c:2221-2231`, `:2914-2921`, `:3466-3565`, `:3133`; `NodeEntitySyncUnchanged` `:2982-2986`.

Con cola vacía, cada 10 ticks, lote 32: si `m_Powered` no casa con el flujo, `SyncNodeToEntity` sin máscara. `NodeEntitySyncUnchanged` no corta (acaba de cambiar `m_Powered`). El callback de M aplica Set/Reset de nivel sobre bits que pueden ser el alto transitorio de R22v3-01. El modelo de §6.3 imita el barrido «porque puede tapar un mutante»; no exige que un `SetPowered` del barrido **no** active M en 11→00→10. Un verde de «cola y diferidos vacíos» puede llegar **después** de un barrido que ya activó M.

Arreglo: el fixture de transiciones y latch viejo prohíbe `SyncNodeToEntity` / `SetPowered` del barrido entre el flanco y el aserto; o el barrido no llama callbacks de puertas/celdas (solo `m_Powered`).

**R22v3-04 — MENOR — ancla (i) del mutante: `inputChanged` del XOR si alguien deja un hilo de flujo en `input_0`**  
`LFPG_ElecGraphImpl.c:2738-2748`, `:4968-4977` (fallback de `GetEdgeAllocatedPower` citado en R22v2-01).

Con `input_1` sin cable y AX=0 estable, al apagar G el XOR tiene `inputSum` 0→0. Si el modelo (o un cable fantasma) deja `inputSum` 0.002→0, `inputChanged` despierta al XOR sin `m_SignalChanged`. El F del mutante quedaría verde.

Arreglo: en la pasada de swA, afirmar también `inputChanged` del XOR apagado y `inputSum` 0→0.

---

### Cierre de mis hallazgos de la v2 (§11.2)

| Id v2 | Decisión v3 | ¿Cierre correcto? |
|---|---|---|
| R22v2-01 IMPORTANTE (mutante Step 3 gameable si se ancla «apagar G») | XOR `input_1` sin cable, aserto en la pasada de swA, todos los órdenes | **Sí**, con el matiz R22v3-04. |
| R22v2-02 IMPORTANTE (reloj 100 ms aplasta `CallLater(50)` / falta Q→Toggle) | Reloj por eventos en ms; PDQ 100 ms y diferidos en su instante; fila Q→Toggle | **Sí** si el arnés dispara `CallLater` entre PDQ (50 ms), no drenando routing y PDQ en el mismo `now`. |
| R22v2-03 MENOR (`SetOutputPortEnabled` no marca `m_SignalChanged`) | Sin cambio: `DIRTY_TOPOLOGY` + `forceDownstream` | **Sí** (sigue sin lector huérfano). |
| Premisa v2 (cierre depende de reencolado / `:2815` / Step 5) | Aserciones tras vaciar cola y diferidos | **A medias.** Cierra el latch de la **puerta**; no cierra el Set de M (R22v3-01). |

---

### Las cinco preguntas (§10)

1. **¿Camino con publicación ≠ lógica actual, o M con alto falso?** Sí: R22v3-01 (orden AND/M antes que B) y R22v3-02 (entidad null). El barrido en vacío (R22v3-03) puede fijar ese alto. Con `[B, AND, M]` y entidad resuelta, AND publica `AND(1,0)` y M no debería activarse.

2. **¿Estado estable con alloc > EPS y señal baja?** En arista **habilitada**, tras cola vacía, no he encontrado uno: sobrecarga deja alloc 0 y señal alta; compuerta cerrada deja alloc 0. La implicación se rompe en aristas **deshabilitadas** (`:5084-5091` conservan alloc, v3 pone señal false); el plan lo excluye. Transitorio 11→00: alloc vieja y señal ya baja hasta el siguiente `AllocateOutput`; los asertos piden estable.

3. **¿Reloj, barrido y presupuestos bastan para rojo/verde verdaderos?** Reloj por eventos: sí para Q→Toggle **si** 50 ms cae entre PDQ. Presupuestos: las filas 64/256 imitan `:2250-2253` (corte **antes** de tomar nodo). Barrido: puede **falsificar un verde** (R22v3-03) o un rojo de mutante si `SetPowered` extra mueve el latch. Cadena larga sola no prueba el tope (ya cerrado en v3 con filas aparte).

4. **¿§5.8 contradice el brief o C1-C7?** No. C1-C7 son pulsadores, no Q de otra celda. El brief pide C1-C7 verdes y no cablea Output→Toggle. La bajada alargada al rehabilitar es extra; el fixture (ii) >1000 ms admite un flip. Q→Toggle de §8 es criterio nuevo, no del brief.

5. **¿Otro camino que despierte al XOR en el ancla (i)?** Con `input_1` sin cable, AX=0, no: Splitter marca swA por `inputChanged`; swA solo tiene `m_SignalChanged`. G/Splitter no marcan el XOR. D5 Laser directo sigue sin servir (`:2688-2690`). Hueco: `inputChanged` en el propio XOR si `inputSum` no es 0→0 (R22v3-04); warmup `MarkSourcesDirty` no está en ese fixture.

---

### Premisa de esta v3 que puede estar mal

«El callback de Step 5 lee las mismas aristas y el latch queda igual a lo publicado» (§2.1) y «publicar la lógica actual cierra los altos que no pasan por 00» (§5.5). Eso es cierto **para el latch de la puerta en esa pasada**. Es falso para M.Set: el alto que captura M es el `m_SignalHigh` de **salida** escrito cuando la puerta **aún no había visto** el flanco del proveedor. La v3 trata un síntoma de orden `[B, AND, M]` y lo generaliza a todos los órdenes.

---

### Qué defecto no verían los tests del apartado 6

- Modelo que baja `m_SignalHigh` de B al encolar el apagón, antes de la pasada de B: las permutaciones (b)–(d) salen verdes y R22v3-01 sigue en Enforce.
- Barrido en vacío que llama `SetPowered` de M y el aserto solo mira el latch al final (R22v3-03).
- Reloj que drena `CallLater(50)` y PDQ en el mismo ms: Q→Toggle verde en Python, flip en partida (cierre de R22v2-02).
- Linter `AGENTS.md` §1: no ve `allocEdge.m_SignalHigh` si falta el campo en `LFPG_ElecEdge`; CI verde, World no carga. El barrido manual de §6.7 lo cubre solo si se hace.
- `graph_reference` no certifica lógica (§6.7).
- Entidad null en pre-gate: ningún fixture de §6.4 la fuerza.

---

### Comprobado / no comprobado

**Comprobado (lectura en este árbol = 592ebcc):** `PLAN-CONSOLIDADO.md` §0, §2.1, §3.1, §5.5-5.8, §6.3-6.6, §10-11.2; `PLAN-CONSOLIDADO-v2.md` contraste de §5.7; `REVIEW-grok-plan-v2.md`; `BRIEF-GRAFO.md` criterios C1-C7; `AGENTS.md` §1; `LFPG_LogicGate.c:115-220`; `LFPG_MemoryCell.c:1-24,197-270`; `LFPG_ElecGraphImpl.c` PDQ `:2220-2354`, SOURCE/PT `:2366-2399`, pre-gate `:2473-2525`, `m_GateClosed` `:2635-2710`, Step 3 `:2725-2885`, `ValidateConsumerStates` `:3395-3587`, `AllocateOutput` Pass 2 `:4725-4798`, `IsPortReceivingPower` `:5013-5048`, `SetOutputPortEnabled` `:5053-5106`; `LFPG_Defines.c:465-534`; `LFPG_NetworkManagerImpl.c:865,2441-2461,3158,3722`.

**No comprobado:** ejecutar tests; Enforce in-game; orden real de la cola sucia; `GRAFO-SPLITTER-MEDICIONES.md`; que `enforce_scalar_slice` acepte `ComputeSignalOut`; `checks.yml` línea a línea; la revisión de Codex (prohibida). La cola `[AND, M, B]` es escenario leído del PDQ FIFO/LIFO, no una traza de partida.
