Voy a leer el protocolo R21, el plan v6 y el diff de producción; no tocaré ningún `REVIEW-*`.Siguiente: extraer §2.1–§10 del plan y el diff `592ebcc..bf2cb33`.Ahora el diff de producción y el test; cruzaré cada fila de §3.1 y §10 con el fuente.No tengo shell aquí; voy a leer el fuente y el test a ciegas y cruzar cada punto de §10.APTO CON CAMBIOS

Hallazgos, de más a menos grave:

### H1 — IMPORTANTE — mutante «Baja en todo m_IsGated» no es el texto que muerde F
`path:line`: `scripts/5_Mission/LFPG_ElecGraphImpl.c:2585-2590`; `.github/tools/test_graph_logic_signals.py:3395-3401`, `:1022-1023`, `:2797-2810`.

Escenario: el mutante de §6.6 sustituye  
`if (node.m_IsGated && !gateEntityResolved)`  
por  
`if (node.m_IsGated && !gateEntityResolved && false)`.  
Eso solo deja de encolar el reintento. Un interruptor `swA` con `m_IsGated`, sin entidad, `m_GateClosed=false`, `signalIn=true`, sigue publicando `ComputeSignalOut(...)` alto (`:2563`) y no toca `signalOut`. Entrada: AND 11, `swA` irresoluble, lámpara 10. Salida mala del mutante real: `e_a0.m_SignalHigh=true`, latch AND abierto — igual que el arreglo. El F `test_live_gate_no_entity_annotated` solo falla porque el modelo, con `MUTANT=="all_gated_low"`, fuerza `gate_is_closed=True` (`:1022-1023`), no porque el parche del `.c` baje la señal.

Arreglo: el `old`/`new` del mutante tiene que poner `signalOut=false` (y `QueueGateRetry`) en todo `m_IsGated` sin entidad, como la v4; el F debe leer el fuente mutado, no un flag paralelo.

### H2 — IMPORTANTE — mutante «quitar `m_SignalChanged` del reproceso» no afirma F
`path:line`: `LFPG_ElecGraphImpl.c:2883-2886` y `:2096-2126`; `test_graph_logic_signals.py:3295-3303`, `:757-766`, `:2046-2058`.

Escenario: §6.6 exige F de convergencia en (a). La fila lleva `annotate_f: True` y el comentario de que `MarkNodeDirty` ya rebobina `m_LastEpoch` si el nodo corrió en esta época (`:2126`). XOR 10 sin carga, apagar A, una PDQ: `MarkNodeDirty` sobre la puerta (`:2889`) ya deja `m_LastEpoch = época-1`, así que quitar `m_SignalChanged` de `:2883` no deja el XOR abierto. El extra de `test_a3_xor_splitter_10` (`:2053-2058`) no muerde ese mutante. La fila F no afirma el reproceso de `:2883`.

Arreglo: un circuito donde el lector ya procesó esta época y **no** vuelve a `MarkNodeDirty` (solo el reset de `:2883`), o declarar el reset de época como muerto y quitarlo del contrato.

### H3 — IMPORTANTE — Cast nulo: Splitter y `allocAvail` sí; Memory Cell no
`path:line`: `test_graph_logic_signals.py:2813-2840`; producción `:2563-2593`.

Escenario de §6.4: AND (10), entidad resuelta = Splitter (`cast_null`), detrás Memory Cell en Set. El test monta `G -> gate -> lamp`, pone `resolve_alias["gate"]=Splitter`, `m_GateClosed` cacheado true, y afirma `last_alloc_avail==10` y señal baja. No hay celda. Un Cast nulo que baje `allocAvail` a 0 (contra §5.10) lo caza el `alloc==10`; uno que publique alto en una arista a Set no lo caza nadie.

Arreglo: añadir Set → Memory Cell y afirmar M desactivada, como la fila.

### H4 — MENOR — mutante `Remove(0)` en el modelo no es Enforce `Remove(0)`
`path:line`: `test_graph_logic_signals.py:1331-1332`; `LFPG_ElecGraphImpl.c:3185-3186`.

Escenario: Enforce `Remove(0)` lee `[0]` y mete el último en el hueco. El flag `retry_remove` hace `pop()` del **último** y no toca `[0]`. Con 11 pendientes, el modelo solo reintenta siempre el mismo último; el juego ping-pong 0 y n-1. El F de las 11 posiciones sigue pudiendo fallar, pero no demuestra el fallo «examina 0, 10, 0, 10 y no llega al 1» de §5.10.

Arreglo: `nid = gate_retry[0]; gate_retry[0]=gate_retry[-1]; gate_retry.pop()`.

### H5 — MENOR — ancla tautológica en el mutante (i) de `m_SignalChanged`
`path:line`: `test_graph_logic_signals.py:3560`, `:3567-3573`.

Escenario: `assertTrue(q.settled_empty or True)` siempre pasa. Si `swa_trig`/`xor_trig` están vacíos, no se afirma `inputChanged` apagado. El F del mutante Step 3 sigue yendo al latch del XOR; las precondiciones de Grok R22v3-04 no están ancladas.

Arreglo: exigir `xor_trig` no vacío y quitar el `or True`.

---

## Seis puntos de §10

1. **`LFPG_DIRTY_INPUT` y latch al volver.** Producción: `RetryUnresolvedGates` llama `MarkNodeDirty(retryNodeId, LFPG_DIRTY_INPUT)` (`:3207`). Test: `test_unresolvable_entity_return_annotated` (`:2770-2795`) afirma `gate_open` true (AND 11) y `e_gl` alta en ≤11 PDQ; `test_bounded_retry_na` (`:2891`) afirma la máscara. Cumple.

2. **Cadencia, `RemoveOrdered`+`Insert`, tope tras recuperar.** `m_LastGateRetryTick` vs `m_ValidateTickCount` (`:109`, `:196`, `:3172-3174`). `RemoveOrdered(0)` + `Insert` (`:3186`, `:3203`, `:3210`). Test 11 posiciones: ≤4 resoluciones / 10 llamadas, hueco ≥10, ≤3 rondas al volver, 200 vacíos después (`:2888-2910`). Cumple en el modelo.

3. **Pendiente recuento en los dos vacíos.** Rama inicial: `idleRemaining` después de `RetryUnresolvedGates` (`:2258-2265`). Rama que vacía: `remaining = Count()-Head` (`:2990-2991`) antes de la sonda (`:3011`). `test_s17_*` y `test_retry_return_value_na` (`:2913-2954`): `rem==1`, warmup sigue, sonda no asienta, siguiente PDQ 0. Cumple.

4. **Árbol ruidoso y admisión.** `attach_noise_tree` (`:1853-1894`): 1+3+9+27 splitters, 81 consumidores, puertos `output_1..3`, `input_1`, tope 12, sin ciclos. `assert_noise_budget` (`:1918-1926`): ≥65 nodos de ruido en ≥2 PDQ. `test_noisy_neighbor_annotated` mezcla ruido y circuito. Cumple el texto de §10.4 (no jugué la partida).

5. **Cast nulo + Splitter + `allocAvail`.** `test_cast_null_na` (`:2826-2837`): Splitter abierto, cache cerrado, `alloc==10`, señal baja, pendiente. Cumple §10.5; no cumple Memory Cell de §6.4 (H3).

6. **11 posiciones.** `for pos in range(11)` (`:2844`). Cumple.

---

## R21 habitual

**§3.1 / R20 (leído en este árbol, no contra `git diff` ejecutado):** `m_SignalHigh` / `m_IsLogicGate` en `LFPG_Data.c:262,321,350,363`; `LFPG_GATE_RETRY_PER_CALL` en `LFPG_Defines.c:516-518`; base `IsPortReceivingSignal` `LFPG_ElecGraph.c:313-318`; impl: pre-gate `ResolveGatedEntity` (`:2520`), Step 1 bit (`:2355-2356`), `ComputeSignalOut` (`:4201-4236`), rama de puerta (`:2563-2593`), `AllocateOutput(..., signalOut)` (`:4779`, escritura fuera de `!overloaded` `:4977-4981`), Step 3 (`:2854`, `:2883`), `SetOutputPortEnabled` limpia el bit (`:5336`, `:5342`), retry (`:3160-3211`), `idleRemaining` (`:2210`, `:2261`). Puertas leen señal (`LFPG_LogicGate.c:139-140`); celda Power flujo / Toggle-Reset-Set señal (`LFPG_MemoryCell.c:205-211`). Sin `m_BrownoutHeld`. CI: `checks.yml:58-59`.

**Enforce:** locales de señal/retry al inicio de `ProcessDirtyQueue` (`:2209-2218`) — `idleRemaining` no se redeclará. Sin `? :`, `++`, `+=` en el código nuevo leído. `ref` no en parámetros nuevos. `LFPG_LogicGateBase` en 4_World, usado en 5_Mission. Miembros: `m_SignalHigh` en `LFPG_ElecEdge`; `m_IsLogicGate` en `LFPG_ElecNode`; `m_GateRetry` / `m_LastGateRetryTick` en `LFPG_ElecGraphImpl`.

**Defecto que estos tests no verían:** quitar solo el reset `tgtNode.m_LastEpoch` de `:2883` (dejar `MarkNodeDirty`); un `Cast` nulo que abra Set a una Memory Cell; un `Remove(0)` real si el F se fía del `pop()` de Python; energía Pass 1/2/3 de `AllocateOutput` (el oráculo G2 no certifica el prorrateo); compilador Enforce / arranque (el linter no ve campos inventados, AGENTS.md §1).

**Comprobado:** fuente citado arriba, filas §10.1-10.6, MUTANT_ROWS vs §6.6, admisión del árbol, recuento idle, cadencia en el modelo.

**No comprobado (suposición + prueba que la tumba):** `git diff 592ebcc bf2cb33` no se ejecutó aquí (no hay shell) — un `git diff --stat` que liste un séptimo `.c` tumba R20. Suite «62 en verde» no corrí — `python -B .github/tools/test_graph_logic_signals.py`. Delta del linter `errors` no corrí. Comportamiento in-game A2/B1/GL-06. Semántica real de `array.Remove(0)` en Enforce — un script Workbench con 11 strings lo fija.