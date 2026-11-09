<!-- Salida literal de Grok (solo lectura, sesion nueva, a ciegas). Quitado solo el preambulo previo al veredicto. -->

APTO CON CAMBIOS

**R22v2-01 — IMPORTANTE — el mutante de Step 3 sigue siendo gameable si el test ancla «apagar G» y no la pasada del interruptor**  
`scripts/5_Mission/LFPG_ElecGraphImpl.c:2738-2748`, `:4932-4977`, `:2786`; PLAN-CONSOLIDADO §6.6 ancla (i).

Cierre de Grok R22-05: la ancla (ii) (Laser → interruptor → Set, pasada del interruptor) sí muerde. La (i) solo muerde si el aserto es la pasada del **interruptor**, no el evento «apagar G».

Valores: A4, G 50, Splitter, swA on → XOR.in0, swB off, XOR sin carga. En estable las asignaciones son 0. `GetEdgeAllocatedPower` del SOURCE con `m_AllocatedPower=0` y G aún on hace fallback `srcOutput/enabledOutCount` (`:4968-4977`): el Splitter ve `inputSum` 50. Al apagar G, `m_OutputPower` del SOURCE pasa a 0 (`:2366-2370`): Splitter `inputDelta>EPS` → `inputChanged=true` → Step 3 dispara **sin** `m_SignalChanged`. El mutante «quitar `m_SignalChanged` de Step 3» queda verde si el test solo exige que XOR acabe en 00 «después de apagar G».

En la pasada de **swA** sí: entrada por Splitter PASSTHROUGH con alloc 0 → `:4965-4966` devuelve 0→0; demanda 0→0; AX 0→0; `m_AllocChanged` falso (el interruptor no cambia de compuerta). Solo `m_SignalHigh` 1→0. Ahí el mutante deja a XOR sin `DIRTY_INPUT` y el latch se queda en 10 con la arista ya baja.

Arreglo: el F del mutante tiene que citar el nodo `swA` y los seis disparadores (cinco apagados + `m_SignalChanged`), con la instrumentación de §6.3. Prohibido un aserto solo sobre XOR o sobre G.

**R22v2-02 — IMPORTANTE — reloj a 100 ms aplasta `CallLater(50)` de routing y puede pintar verde un flip de Toggle que en partida ocurre**  
`scripts/4_World/LFPG_MemoryCell.c:262-269`, `:218-225`, `:23` (`LFPG_MCELL_DEBOUNCE_MS=400`); `scripts/3_Game/LFPG_Defines.c:532-534`; PLAN §6.3 «cada llamada al PDQ es un tick de 100 ms».

`LFPG_DeferredRouting` es 50 ms. `TickPropagation` es 100 ms. En producción el defer cae **entre** dos `ProcessDirtyQueue`. Un modelo que solo avanza `GetTime` de 100 en 100 junta en el mismo instante: (1) `SetOutputPortEnabled` que pone `m_SignalHigh=false` al rehabilitar, (2) el `AllocateOutput` siguiente que vuelve a poner true, (3) el `SetPowered` del lector.

Valores: celda A Q → Toggle de celda B; A pasa de inactiva a activa. En partida B puede ver un flanco 0→1 con 50–100 ms de línea baja en medio (rearme de Toggle, `LFPG_MCellCommitToggle` `:358-365` solo sube). En el modelo a 100 ms B puede ver un solo sample alto y no voltear, o un solo sample y sí voltear, sin el hueco de 50 ms. La cadena larga y C1–C7 (pulsador, no Q de otra celda) no cubren este cableado. El fixture «salida rehabilitada» exige que B **no** se active por **Set**, no por Toggle.

Arreglo: el reloj del modelo tiene que ser 1 ms (o al menos 50 ms) y disparar la cola `CallLater` en ese grano; PDQ solo cada 100 ms. Fixture: Q de A → Toggle de B, A off→on, 400 ms de rearme ya cumplidos, aserto de un flip o de cero con el criterio de producto escrito.

**R22v2-03 — MENOR — `SetOutputPortEnabled` no marca `m_SignalChanged` al forzar el bit a false**  
`scripts/5_Mission/LFPG_ElecGraphImpl.c:5075-5098`; PLAN §3.1.

Las dos ramas que cambian el flag marcan `DIRTY_TOPOLOGY` (`:5096-5098`) y Step 3 ya tiene `forceDownstream` para SOURCE/PASSTHROUGH (`:2769-2774`). La Memory Cell es PASSTHROUGH. No he encontrado un lector que reciba esa arista y **no** sea outgoing del mismo nodo. Si eso se cumple, el bajo forzado llega. Un hallazgo sin escenario de lector huérfano: MENOR. Prueba que lo sube a IMPORTANTE: un `IsPortReceivingSignal` entre el `= false` y el próximo PDQ del proveedor, sin `DIRTY_TOPOLOGY` (p. ej. `anyChanged=false` por no cambiar el flag).

---

### Cierre de mis hallazgos de la v1 (§11)

| Id v1 | Decisión v2 | ¿Cierre correcto? |
|---|---|---|
| R22-01 BLOQUEA (latch 00 sin segunda visita) | Quitar `m_BrownoutHeld` | **Sí.** Con `signalIn` falso, `ComputeSignalOut` publica baja aunque el latch aún esté abierto (`gateIsClosed` vivo en `:2483-2509`); Step 5 cierra (`:186` queda `m_GateOpen != newGate`). XOR 10→00 y B1 tras B off ya no se quedan abiertas. |
| R22-02 IMPORTANTE (B5 lógica viva) | Contrato de producto, no se cambia | **Sí como producto.** El brief no exige que el all-off apague la lógica; el riesgo B5 del brief es subir demanda con sondas, y esto no añade sondas. Sigue el comportamiento que yo describí (XOR abierta, cables CRITICAL, lámpara 0). |
| R22-03 IMPORTANTE (A4 rojo obligatorio) | Rojo obligatorio en §6.4 | **Sí.** |
| R22-04 IMPORTANTE (bit sucio al deshabilitar) | `m_SignalHigh=false` al cambiar el flag en ambos sentidos | **Sí para el bit viejo en alto.** No cubre Toggle sobre Q (R22v2-02). |
| R22-05 IMPORTANTE (mutante Step 3) | Anclas en interruptor + instrumentación | **A medias.** (ii) correcto (`:2688-2690` pone `m_AllocChanged` en el Laser, no en el interruptor de abajo). (i) correcto solo con aserto en swA (R22v2-01). |
| R22-06 MENOR (comentarios / hold) | Se borra el hold | **Sí.** |
| R22-07 MENOR (`test_pulse_survives_blackout.py`) | En §6.1 y §6.7 | **Sí** (citado; no he ejecutado el workflow). |
| §10.5 NodeEntitySync / GetEdgeAllocatedPower | En §6.3 y S4 | **Sí.** `NodeEntitySyncUnchanged` `:3133` salta `SetPowered` sin `DIRTY_INPUT`; S4 lee `m_SignalHigh` y no `GetEdgeAllocatedPower` (`:4942-4944` anula flujo en overload). |

---

### Las cinco preguntas (§10)

1. **¿Alto falso en la salida de una puerta, o latch ≠ tabla tras la última lectura?**  
   Con el hold fuera, no he encontrado un camino estable. 11→00 en la misma época: `signalIn` falso ⇒ `signalOut` falso ⇒ callback cierra. El alto falso de Codex R22-02 (00 retenido, luego 10, pre-gate aún abierto) **desaparece**: en 10 el latch ya está cerrado y «compuerta cerrada ⇒ baja». Arranque con aristas a false: un cierre y una reapertura, que el fixture admite. Hueco residual: si **no** se añade `m_SignalChanged` a `:2815`, la puerta ya procesada en la época no relee y el latch queda 10 un tick con las entradas ya bajas; el mutante de reproceso lo cubre. «Tras la última lectura» de la propagación acotada, tabla y latch coinciden.

2. **Limpieza en `SetOutputPortEnabled`: ¿flanco de Toggle o borra una señal buena?**  
   Toggle solo voltea en subida (`:358-365`). Deshabilitar: el lector ya salta `LFPG_EDGE_ENABLED` (`:5076-5091` + copia de `IsPortReceivingPower`); el bajo forzado es el mismo falso que el skip → bajada, no flip. Habilitar: el bit anterior era skip/falso → no hay bajada; el alto lo vuelve a escribir el proveedor. No borra un alto **válido** de una arista que ya estaba habilitada (solo corre si el flag cambia). El riesgo real es la **subida** posterior en Q→Toggle (R22v2-02), no el `false` forzado.

3. **¿Presupuestos y reloj de §6.3 bastan para cadena larga y 400 ms?**  
   Presupuestos: 64 / 256 (`Defines.c:465-471`), corte `:2250-2253`, doble si cola > 128 (`NetworkManagerImpl.c:2446-2461`), requeue 5 y diferido `:2278+`. Cadena de 64 Splitters cabe en 1–2 ticks de nodo; el tope de aristas (~4 visitas/nodo) puede cortar en 256 y dejar el resto para el tick siguiente. 100 ticks sobran para que las lecturas bajen. 400 ms: si `GetTime` y `CallLater` avanzan juntos, un cambio de Toggle/Power que llega en el tick N se asienta en N+400 ms (4 ticks). **No basta** el grano de 100 ms para el defer de 50 ms (R22v2-02). Defecto que §6 no ve: un modelo que drena `CallLater` y PDQ en el mismo `now+=100` sin el hueco de 50 ms.

4. **¿El mutante de `m_SignalChanged` muerde de verdad?**  
   (ii) sí. (i) sí solo con instrumentación en swA (R22v2-01). D5 con Laser directo en Set **no** muerde: `:2688-2690` ya pone `m_AllocChanged`. Eso está bien dicho en la v2.

5. **B5 declarado vs brief**  
   No hay criterio del brief que lo contradiga. Aceptación: tablas A1–A3, A4 ≤1 cambio / 60 s, B1 invierte la **puerta**, D5 Set/Reset, C1–C7, sin SCRIPT(E). El riesgo B5 del brief es la sonda extra cerca del límite; este arreglo no añade demanda. Lógica viva con all-off (XOR abierta, Door Controller sin energía, LEDs de entrada on) es el contrato nuevo, no un fallo del brief. En partida hay que mirar B1/B2 como «puerta según tabla» y lámpara/puerta física sin flujo, no como «todo apagado».

---

### Premisa de esta v2 que puede estar mal

«Sin la tolerancia, un latch abierto implica que en la lectura anterior había una entrada alta» y «esa misma pasada publica baja y el callback cierra» (§5.5). Eso asume que Step 3 reencola, que `:2815` reprocesa en la misma época, y que Step 5 no lo salta `NodeEntitySyncUnchanged`. Si cualquiera de los tres falla, hay un tick (o más, si hay presupuesto) con latch 10 y `m_SignalHigh` de entrada ya false, o con salida de puerta aún alta. Los tests lo cazan solo si leen latch **y** bit después de agotar cola, no si miran un solo `SetPowered`.

---

### Qué defecto no verían los tests del apartado 6

- Linter de `AGENTS.md` §1: no ve `obj.m_SignalHigh` si el campo no está en `LFPG_ElecEdge` (punto ciego medido). El barrido manual de §6.7 sí; si se omite, CI verde y el cliente muere al cargar World.
- `CallLater` de Enforce vs cola Python: 400 ms y 50 ms no tienen el mismo orden que `TickPropagation` si el arnés no comparte `GetTime` al milisegundo.
- Toggle alimentado por Q/!Q de otra celda (R22v2-02).
- Mutante Step 3 anclado a «apagar G» (R22v2-01).
- Oráculo `graph_reference` no certifica lógica (§6.7).

---

### Comprobado / no comprobado

**Comprobado (leído en este árbol = 592ebcc):** `LFPG_LogicGate.c:80-220`; `LFPG_MemoryCell.c:23,197-422`; `LFPG_ElecGraphImpl.c` PDQ `:2230-2253`, Step 1 `:2298-2353`, SOURCE/PT `:2366-2399`, pre-gate `:2483-2509`, `m_GateClosed` `:2640-2706`, Step 3 `:2725-2871`, `NodeEntitySyncUnchanged` `:2966-3029` y `:3133`, `AllocateOutput` `:4562-4798`, `GetEdgeAllocatedPower` `:4932-4977`, `SetOutputPortEnabled` `:5053-5106`; `LFPG_Data.c:325-356`; `LFPG_Defines.c:465-494,532-534`; `LFPG_NetworkManagerImpl.c:2441-2461`; `LFPG_SwitchDeviceBase.c:30` (`IsGateOpen` = `m_SwitchOn`); `test_memorycell_toggle.py` cola/`GetTime`; BRIEF criterios; PLAN v1 §2.1 vs v2; mi REVIEW v1.

**No comprobado:** ejecutar tests; Enforce; orden real de la cola in-game; `GRAFO-SPLITTER-MEDICIONES.md`; que `enforce_scalar_slice` acepte `ComputeSignalOut`; `checks.yml` línea a línea (R22-07 es cita del plan); la revisión de Codex (prohibida).
