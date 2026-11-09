Revisión R21 de fidelidad sobre el delta del test. Empiezo por el plan, AGENTS.md y el diff `898de79..e06185a`.El test es grande; voy a extraer el delta, contar tests y localizar los testigos de la lista.NO APTO

## Hallazgos

### H1 — BLOQUEA — `test_graph_logic_signals.py:2764`
Escenario de rehabilitación con retraso **(ii)** no se ejecuta. Tras (i) (Q 1→0→1, `flips_i==0`) el test solo hace `_ANNOTATED.append("ANOTADO salida rehabilitada retraso (ii) cola llena: 1100 ms")`. No llena la cola, no avanza 1100 ms, no cuenta flips de B. El testigo 8 y el plan §6.4 (ii) piden B en Toggle, lectura baja ≥1100 ms y exactamente un volteo.
**Arreglo:** montar nodos pendientes para que la pasada de A llegue tarde, `run_events(1100)`, afirmar deshabilitación, lecturas de B y `flips==1`.

### H2 — BLOQUEA — `test_graph_logic_signals.py:3766`
Cancelación independiente de dos celdas no muerde. `self.assertEqual(a.m_LatchState, latch_a or a.m_LatchState)` es tautología si `latch_a` es falso (`assertEqual(x, x)`). Si A no llegó a Toggle en 500 ms, cancelar los timers de B siempre pasa.
**Arreglo:** forzar Toggle de A (filtro 400 ms cumplido), cancelar solo B, afirmar que A sigue su latch y que el `CallLater` de A sigue en `clock.pending`.

### H3 — IMPORTANTE — `test_graph_logic_signals.py:2535-2537`
G2/invariante: si `settled_empty` es falso, el test anota y hace `return` sin fallo. El testigo 8 dice que no vaciar donde la fila pide vaciado es fallo. Esta fila pide G2 en estable.
**Arreglo:** `self.fail` si no vacía; luego G2.

### H4 — IMPORTANTE — `test_graph_logic_signals.py:1905-1912` y `2138-2166`
A1–A3 recorren la tabla solo en orden guionado (`ORDER_A_SPLIT`). FIFO/LIFO/20 permutaciones aparecen en latch viejo (`2582`), no en A1–A3. El testigo 8 y §6.3 piden (a)–(d) en verde con el arreglo.
**Arreglo:** repetir `walk_table` / `run_bank` con `order_mode` fifo, lifo y perm.

### H5 — IMPORTANTE — `test_graph_logic_signals.py:2264-2286` y `LFPG_ElecGraphImpl.c:2756-2758`
`_assert_d5_red_or_green` ignora `expect_set` y `set_high` y siempre exige latch true. En 592ebcc A4 se fabrica rojo a propósito (`2199-2203`); D5 no. Si Laser→interruptor→Set ya asienta por flujo, `test_d5_laser_switch_set` queda **verde en la base**, fuera de la lista de rojos (A1, A3, A4×2, D5 vía Splitter y directo).

`m_AllocChanged = true` en transicion de compuerta **sí está** en el fuente (`2756-2758`) y el modelo lo copia (`1235-1238`). Ese disparo marca aguas abajo y aguas arriba en **D5 directo y con interruptor**. Si bastara para Set por flujo, D5 directo también sería verde en base, y el plan lo da rojo. O el modelo reparte de más en `e_ls`/`e_set`, o la tesis del autor no sigue del `:2756`.
**Arreglo:** en base, afirmar rojo en Splitter y directo; laser-switch como fila aparte con el resultado medido; no reutilizar el mismo `assertTrue(latch)`.

### H6 — MENOR — `test_graph_logic_signals.py:3807-3835`
Empate timer/PDQ: con `tie_timers_first=True` solo `assertTrue(fired)`; con False solo `pdq_calls >= 1`. No se afirma el orden (timer antes que PDQ vs PDQ primero).
**Arreglo:** traza `["t","pdq"]` vs `["pdq","t"]`.

### H7 — MENOR — `test_graph_logic_signals.py:1412-1420`
El mutante `retry_no_cap` también **salta la cadencia** (`mutant not in ("retry_no_cap", "retry_no_cadence")`). Un tope 999 con cadencia 10 no se prueba solo.
**Arreglo:** `retry_no_cap` solo cambia `retryRoundLimit`.

## Tabla de testigos

| # | Testigo | ¿Muerde? | Evidencia |
|---|---|---|---|
| 1 | `if (deviceId != "") return false` en IsPortReceivingSignal | Muerde | Mutante `isport_early_false` (`3674-3679`) + lector extraído (`202-210`, `3712-3732`). Callbacks van por `isport_fn` (`373-378`, `1189-1191`). |
| 2 | Quitar sync del recuperado; OR A→OR B; NetworkID None | Muerde | `skip_recovered_sync` (`1378-1379`, `2973-2974`, `3681-3684`). `netid_always_none` + `test_registry_and_unresolvable_annotated` (`2864-2867`, `3685-3688`). |
| 3 | Identidades de callback; dos celdas; `LFPG_DeferredRouting` | No muerde del todo | `assertIsNot` de callbacks sí (`3753-3754`). Cancelación: tautología H2. Deferred sí se extrae y `RequestPropagate` está en el fuente (`430-440`) y en `FakeNM` (`387-388`, `772-787`). |
| 4 | C congestionada y vecino; 5 ticks PDQ; 500 ms | Muerde en lo principal | `last_noise_pending > 0` en C (`2331-2335`) y vecino (`2913-2914`). Bucle `for t in range(5)` + `pdq_calls` (`2917-2926`). 500 ms: `run_events(500)` solo en el preámbulo del vecino (`2896`), no en cada combo de la tabla. |
| 5 | Warmup 128; Splitter vacío; `m_AllocChanged`; ramas vs producción | Muerde | `3768-3781`, `3783-3792` (`raw_demand` → 0), `3794-3805` cita `:2758`. Citas de `:2519`, `:2614`, `:2756` en el modelo. |
| 6 | Mutantes por texto; tolerancia 592ebcc; all_gated; bit al encolar; Remove(0); arnés vs aserción | Muerde | `MUTANT_ROWS` + `apply_mutant_replace`. `brownout_back` (`3561-3568`). `all_gated_low` (`3605-3611`) + regex S14 (`3330-3335`). `enqueue_switch_bit` + `[gate,cell,swB]` (`857-861`, `4045-4085`). `retry_remove` (`1425-1428`, `3641-3646`). `arnes_all_f` (`3837-3855`, `3700-3702`). |
| 7 | G2: virtual, SOURCE, autoconsumo, tras vaciado, latch, sin retry | Muerde con hueco | `g2_oracle` (`1689-1733`): SOURCE = `m_Powered` y `m_MaxOutput`, no `newOutput > EPS` (§2.1). Autoconsumo y `virt` sí. G2 en `settle` solo si `require_empty` y `gate_retry` vacío (`1651-1661`). H3 salta G2 si no vacía. |
| 8 | Rehab retraso; FIFO/LIFO/perm; empate PDQ; sin `or True`; vaciar | No muerde | (ii) no corre (H1). Órdenes de A1–A3 no (H4). Empate débil (H6). `or` tautológico (H2). Invariante no falla al no vaciar (H3). `laser_under_5` sí exige `settled_empty` (`3174`). |
| 9 | Cast nulo + M en Set, M sin activar | Muerde | `3006-3041`: alias Splitter `cast_null`, `allocAvail==10`, con retry `signalOut` falso y `m_LatchState`/`m_CellActive` falsos. Mutante `no_cast_null` (`3613-3618`). |
| 10 | Quitar `m_SignalChanged` del reproceso misma época = equivalente | Muerde como declarado | `annotate_f` + `annotate_why` (`3503-3511`). `mark()` ya rebobina `m_LastEpoch` (`851-853`); el mutante solo cambia `m_RequeueCount`. |

## Comprobado / no comprobado

**Comprobado (lectura de `path:line`):** lista MUTANT_ROWS, lector extraído, PDQ/retry/Remove(0), G2, C/vecino, D5, Cast nulo, S1–S17, `IsPortReceivingSignal` y rama de puerta en `LFPG_ElecGraphImpl.c:2563-2593` y `:5274-5305`, `m_AllocChanged` en `:2756-2758`, `LFPG_DeferredRouting` + `RequestPropagate` en `LFPG_MemoryCell.c:430-440`. El delta no toca `scripts/` (según el encargo).

**No comprobado (suposición; prueba que la tumba):**
- Suite = 69 verdes en este árbol: no hay shell en esta sesión. Prueba: `python -B .github/tools/test_graph_logic_signals.py` y `tests=69 failures=0 errors=0`.
- Rojos exactos en 592ebcc (A1, A3, A4×2, D5 Splitter/directo, S1–S17): el código de A4 **fuerza** rojo en base; D5 no. Prueba: checkout 592ebcc + la misma suite.
- D5 Laser→interruptor→Set verde en base **por** `:2756-2758`: el disparo existe; no implica Set alto (mismo disparo en D5 directo, que el plan da rojo). Prueba: en 592ebcc, `test_d5_laser_switch_set` vs `test_d5_power_direct` y traza `step3_log` de `laser`/`swL`.
- Mutantes que piden `count==1` sobre el fuente actual: si el `old` no calza, el runner lanza `HarnessError` y `test_mutants_skip_or_run` falla. Prueba: esa fila con el arreglo.