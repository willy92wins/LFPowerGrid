<!-- Salida literal de Grok (solo lectura, sesion nueva, a ciegas). Quitado solo el preambulo previo al veredicto. -->

NO APTO

**R22-01 — BLOQUEA — latch retenido en (0,0) sin segunda visita**  
`scripts/4_World/LFPG_LogicGate.c:166-189`, `scripts/5_Mission/LFPG_ElecGraphImpl.c:2786-2821`, PLAN-CONSOLIDADO §5.5 y §2.1.

Escenario: XOR A encendido y B apagado (tabla 10, `m_GateOpen=true`, `m_BrownoutHeld=false`). El jugador apaga A. Las dos señales quedan bajas. El interruptor escribe `m_SignalHigh=false`, `m_SignalChanged` ensucia la XOR una vez. `LFPG_SetPowered` ve `(newIn0,newIn1)=(false,false)`, pone `m_BrownoutHeld=true`, `holdLatch=true` y **no** cambia `m_GateOpen` ni llama `RequestPropagate` (`:186-220`). Con el canal de señal ya no hay all-off que vuelva a ensuciar la puerta: Step 3 del XOR ve `signalIn` falso, `outputDelta` 0 (sigue pidiendo 10), y no se reencola a sí mismo. La XOR se queda abierta con entradas 00.

B1 del plan (A on, B on, A off, B off) termina en ese estado: tras B off la puerta debe ser XOR(0,0)=0 y el Door Controller apagado por lógica; la puerta sigue abierta. Si más tarde llega flujo por cualquier camino (reconexión, otra rama), deja pasar 10 u/s. AND 11→00 en la misma época (los dos interruptores escriben bajo antes de procesar la puerta) cae igual: una sola lectura (0,0) y queda abierta.

La tolerancia de un apagón estaba calibrada al ramp energético que **siempre** producía una segunda visita. El diseño nuevo quita esa segunda visita en un corte real.

Arreglo: quitar `m_BrownoutHeld` (plan Codex) **o** si se retiene el latch, `RequestPropagate` / `MarkNodeDirty` de la propia puerta cuando `holdLatch` para forzar la segunda evaluación **o** retener solo si la época previa tenía `signalIn` alto (rampa), no en un (0,0) estable. Los tests A1–A3 y B1 tienen que incluir transiciones 10→00 y 11→00, no combinaciones aisladas desde frío (00 en frío deja `m_GateOpen=false` y el hold no se nota).

---

**R22-02 — IMPORTANTE — señal de SOURCE = capacidad, no asignación (B5 persistente)**  
`scripts/5_Mission/LFPG_ElecGraphImpl.c:2366-2372`, `:2519-2525`, contrato §2.1 filas SOURCE y “generador sobrecargado”.

Escenario: G 50, Splitter, swA on → XOR.in0, swB off → in1, XOR → lámpara 10, lámpara 60 en la tercera salida. `totalHardDemand > 50+0.001` → all-off, asignaciones 0. `newOutput` del SOURCE sigue siendo `m_MaxOutput=50` (`:2370`). `signalOut = newOutput > EPS` queda **true** en todas las salidas. El Splitter (PASSTHROUGH cons=0, no gated) reenvía señal alta; swA abierto también. XOR lee (1,0) y permanece abierta, LEDs de entrada encendidos, cables CRITICAL, lámpara a 0.

Eso es el contrato declarado, y “Door alimentado solo si da 1” no exige lo contrario. En partida B1/B5 el jugador ve la puerta “en 1” sin energía aguas abajo de forma **estable**, no solo un tick. No está en la matriz A–D como fallo de tabla, pero contradice la intuición de “línea viva” si se espera que un all-off de fuente apague la lógica.

Arreglo: `signalOut` de SOURCE = `node.m_Powered && !overloaded` (o `availableOutput` tras Pass 2), y un fixture B5 que fije el LED/latch esperado. Si se mantiene capacidad, documentarlo como criterio de producto en fase 3, no solo en §2.1.

---

**R22-03 — IMPORTANTE — A4 offline con salida de escape**  
PLAN-CONSOLIDADO §6.4 fila A4; código `LFPG_ElecGraphImpl.c:2614-2629` (demanda 0 con compuerta abierta y cons=0).

Escenario: XOR sin cable de salida, A on, B off. Abierta publica 0; AX recibe 0; dos lecturas oscuras cierran; vuelve la sonda. El plan permite “rojo si oscila; si no, se dice que A4 no se reproduce”. Un `QueueModel` que no llame a `LFPG_SetPowered` real o que no publique 0 en abierto queda verde hoy y verde con el arreglo sin morder A4 (73 cambios / 60 s en partida).

Arreglo: A4 es rojo obligatorio sobre 592ebcc con el callback extraído, igual que A3. Quitar la cláusula de no-reproducción.

---

**R22-04 — IMPORTANTE — `m_SignalHigh` no se limpia al deshabilitar arista**  
`LFPG_ElecGraphImpl.c:5083-5092` (no anula `m_AllocatedPower` al quitar `LFPG_EDGE_ENABLED`); §2.1 “el proveedor no las escribe”.

Escenario: Memory Cell activa, `output_0` con `m_SignalHigh=true`. Latch a false → `SetOutputPortEnabled(..., false)` deja el bool a true. Latch otra vez a true → la arista se habilita y, hasta el `AllocateOutput` de la celda, un lector que entre en esa ventana (mismo tick, nodo ya en cola) ve señal alta con demanda/asignación aún de la época anterior, o al revés `output_1` (`!Q`) con bit obsoleto.

No lo he ejecutado en PDQ; la prueba que lo tumba es un test de routing Q/!Q que llama `IsPortReceivingSignal` entre `SetOutputPortEnabled` y el siguiente `AllocateOutput`.

Arreglo: al deshabilitar, `m_SignalHigh=false` (y opcionalmente `m_SignalChanged` si el valor cambia).

---

**R22-05 — IMPORTANTE — mutante “quitar `m_SignalChanged` de Step 3” puede sobrevivir si el único corte es apagar G**  
§6.6; SOURCE `:2370` vs A4/D5.

Apagar G hace `newOutput` 50→0, `outputDelta > EPS`, Step 3 ya corre sin el bit. El mutante no falla. El caso que sí lo muerde es D5: Laser abre, GX sigue en 5, XM alloc 0→0, `lastStable` 5→5, `outputDelta=0`, `m_AllocChanged=false`; solo `m_SignalChanged` ensucia la celda (`:2688-2706` marca aguas **arriba**, no garantiza pasada propia del Laser hacia Set).

Arreglo: el mutante debe ser “Laser abre, floats de energía iguales, Set tiene que subir”. No usar “G apagado”.

---

**R22-06 — MENOR — comentario de brownout y líneas muertas**  
`LFPG_LogicGate.c:80-84` (“siempre false con la puerta abierta”) y `:148-158` (líneas `3836-4161`, all-off ya en `:4734`). §3.1 solo reescribe `:148-163`. Tras el canal de señal el comentario de `:80-84` queda falso (R22-01). Actualizar o borrar el hold a la vez.

---

**R22-07 — MENOR — `test_pulse_survives_blackout.py` no está en §6.7**  
El workflow ya lo corre (`.github/workflows/checks.yml:76-77`). No debería romperse (el pulsador no cambia de lector), pero un verde de `test_graph_*.py` no lo cubre. Añadirlo a la lista de suites existentes.

---

### Contrato (§10.1)
Señal alta con asignación 0 es correcto en A4 (XOR abierta sin carga) y D5 (Set sin financiar la celda). El fallo visible es R22-01 (00 que no cierra) y, si se acepta el producto, R22-02 (B5 con lógica “viva”).

### D5 / 5 u/s (§10.2)
`newPowered` en `:2389-2391` exige `effectiveInput + EPS >= cons` (5). Laser con entrada 5 y compuerta viva (`:2483-2509`) puede poner señal alta con `newOutput=0`. No he visto un camino con entrada **&lt; 5 − EPS** y `newPowered=true` salvo `m_VirtualGeneration` (batería). GL-04 (Laser detrás de Splitter en all-off) sí baja Set; el fixture D5 del plan cuelga el Laser del generador.

### `m_SignalChanged` (§10.3)
Hace falta para D5 (R22-05). No basta para el segundo (0,0) de un corte real (R22-01). Visitas de más: un bool que cambia reencola destino y, si el nodo es PASSTHROUGH, también aguas arriba (`:2839-2866`); acotado a 5. Bucle de jugador (salida→entrada) igual que hoy.

### Tolerancia (§10.4)
Hay que quitarla o forzar reentrada; dejarla bloquea B1 y 10→00.

### Fidelidad §6.3 (§10.5)
Falta: transiciones a 00; `NodeEntitySyncUnchanged` (`:3133`, salta `SetPowered` sin `DIRTY_INPUT`); no atar señal a `GetEdgeAllocatedPower` (`:4942-4944` anula flujo en overload, la señal no). El escape de A4 (R22-03) puede pintar un verde falso.

### Mutantes (§10.6)
El de Step 3 mal anclado (R22-05) puede sobrevivir. El de `m_GateClosed` cacheado sí muerde D5. Power→señal cubierto.

### Trazas §3 / §4 (§10.7)
Las anclas leídas cuadran (`LFPG_ElecEdge` `:325-356`, `IsPortReceivingPower` base `:305-311`, PDQ `:2298`, `:2516-2525` única llamada a `AllocateOutput`, Pass 2 `:4749-4798`, MemoryCell `:197-208` / GetGraph `:411-422`). No obliga a tocar fórmulas de §4. Sí obliga a tocar `:164-189` (R22-01), que §4 declara intocable.

### Comprobado
Leídos enteros AGENTS.md, BRIEF, PLAN-grok, PLAN-codex, PLAN-CONSOLIDADO. Contrastado en árbol: LogicGate, MemoryCell (Set/Reset por nivel `:235-242`, debounce Power/Toggle `:213-226`), ElecGraphImpl (PDQ, merger `:4009-4034`, oferta `:4074-4237`, AllocateOutput, IsPortReceivingPower, SetOutputPortEnabled, Sync), Data.c arista, ElecGraph/NetworkManager GetGraph, Laser `:229-239`, lámpara PT cons 10, Defines epsilon/sonda/requeue, test_memorycell NM, checks.yml, QueueModel.allocate all-off. Una sola llamada `AllocateOutput(`.

### No comprobado
Ejecutar tests o Enforce; orden real de la cola en DayZ; GRAFO-SPLITTER-MEDICIONES.md (el consolidado la cita; no la relí); que `enforce_scalar_slice` acepte el helper nuevo; ventana exacta routing vs AllocateOutput (R22-04 es mecanismo leído, no traza de tick).
