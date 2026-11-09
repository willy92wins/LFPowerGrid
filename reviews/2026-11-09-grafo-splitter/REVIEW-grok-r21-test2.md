Revisión R21 de solo lectura: cargo el plan, el delta y las reglas de revisión.Siguiente: el delta, el plan y los tests; luego ejecuto la suite y los testigos.Sin shell en este arnés: revisaré estáticamente y extraeré el comportamiento de los tests.APTO CON CAMBIOS

## Hallazgos

**H1 — IMPORTANTE** — `test_graph_logic_signals.py:1713-1736`, `1968-1992`  
G2 solo corre dentro de `settle` si `queue_empty` y `not gate_retry`. A1–A3, C y casi todas las filas llaman `settle(..., require_empty=False)` / `wait_combos`. Si a los 100 ticks sigue cola, no hay FAIL de vaciado y no hay G2. Un oráculo que no mirara el latch (solo aristas) seguiría verde en las filas que sí asientan, porque tras asentar latch = tabla.  
Arreglo: `require_empty=True` donde el plan pide vaciado; G2 también si `settled_no_retry`.

**H2 — IMPORTANTE** — `test_graph_logic_signals.py:2415-2438`, `scripts/5_Mission/LFPG_ElecGraphImpl.c:2756-2758`  
`test_d5_power_via_splitter`, `test_d5_power_direct` y `test_d5_laser_switch_set` pasan siempre `expect_latch=True`. No hay rama «rojo en 592ebcc». En el fuente, la transición de compuerta sí hace `m_AllocChanged = true` (`:2758`). El modelo copia esa ventana con o sin `ComputeSignalOut` (`:1287-1294`). Eso hace creíble el aserto del autor: Laser → interruptor → Set ya puede asentar Set por flujo en la base. La fila no muerde el canal de señal; el plan §6.4 pide rojo en las tres D5 con Power.  
Arreglo: en base, `expect_latch` falso en splitter/directo; en Laser→switch, asertar el rojo o documentar excepción con traza de `m_AllocChanged` y `Set` por flujo.

**H3 — IMPORTANTE** — `test_graph_logic_signals.py:1473-1481`, `3914-3920`  
El mutante textual `retryRoundLimit = 999` no lo lee el modelo. El rojo de F lo fabrica `if self.mutant == "retry_no_cap": n = len(self.gate_retry)`. La cadencia sigue el flag `retry_no_cadence`, no el texto.  
Arreglo: extraer `retryRoundLimit` del fuente mutado, igual que Step 3 lee `m_SignalChanged`.

**H4 — MENOR** — `test_graph_logic_signals.py:1351-1352`  
El mutante «quitar `m_SignalChanged` del reproceso» está declarado equivalente y el F se anota. El modelo sigue el flag `drop_signal_same_epoch`, no el texto de `:2883`. Coherente con el anuncio; no es F de comportamiento.

**H5 — MENOR** — `g2_compare:1834-1835` vs ausencia de fixture  
Un OR cerrado con entrada alta y SOURCE on sí genera `("latch", ...)` en `g2_compare`. No hay fila que construya ese estado y afirme `g2_compare != []`. El testigo 14 depende de que alguien llame a G2 con el desajuste.

---

## Tabla de 20 testigos

| # | Muerde | Evidencia |
|---|--------|-----------|
| 1 | muerde | Mutante `isport_early_false` extrae el cuerpo (`load_is_port_receiving_signal:219-276`); `if (deviceId != "") return false` se interpreta. F: `test_isport_extracted_reader`, `test_a3_xor_splitter_10`. Callbacks van por `FakeNM.IsPortReceivingSignal` → `isport_fn`. Sin rama Python que fuerce el falso. |
| 2 | muerde | Flag/fila `skip_recovered_sync` + `test_unresolvable_entity_return_annotated:3234-3271` (OR A→OR B). `netid_always_none` + Find/vanilla fallan → `test_registry_and_unresolvable_annotated:3141-3144` exige registro por NetworkID. |
| 3 | muerde | `test_two_cells_independent_timers:4028-4079` (`cb_toggle` distintos, `Clock.Remove` de B). `attach_cell:814-829` extrae `LFPG_DeferredRouting`; `RequestPropagate` en `FakeNM:428`. |
| 4 | muerde | `test_c_congested_green:2483-2488` afirma `last_noise_pending > 0` en cada press. Vecino: `run_events(500)` y `CallLater(..., 500)` (`3173-3208`); tope 5 con `pdq_calls`. |
| 5 | muerde | `test_warmup_128_leaves_12:4092-4094` (128/12). `test_cold_start_empty_splitter` vs `m_Consumption` en `:4818`/`:4890`. `test_gate_transition_sets_alloc_changed` vs `:2758`. |
| 6 | muerde | Brownout restaura bloque (`3848-3856`). `all_gated_low` F en `test_live_gate_no_entity_annotated`. `enqueue_switch_bit` + `test_race_and_mb_enqueue_bit` (orden [AND,M,B], bit al encolar). `retry_remove` emula `Remove(0)`. `test_harness_error_row_demonstrates_runner` con `arnes_all_f`. |
| 7 | no muerde del todo | Oracle §2.1 + latch en `g2_compare`. `settle` aplica G2 solo si ya está vacío; las filas con `require_empty=False` pueden saltárselo (H1). |
| 8 | muerde | Rehab (ii) en `2901-3041` (Q 1→0→1, 1100 ms, un flip). FIFO/LIFO/20 perm en `_walk_orders` y `_race_order`. No hay `or True`. Vaciado fallido solo si `require_empty=True` (rehab ii sí; A1–A3 no). |
| 9 | muerde | `test_cast_null_na:3313-3328`: Set baja, M no activa, `process("cell")` después. |
| 10 | muerde (equivalente) | Fila `drop_signal_same_epoch` con `annotate_f` y el texto de H2/`m_RequeueCount`. |
| 11 | muerde | Mutante `isport_true_return_false` sobre el texto; el lector extraído, no un `in` de cadenas. |
| 12 | muerde | Filas `test_gate_transition_sets_alloc_changed` y `test_cold_start_empty_splitter` (parseo `+ 1.0`). |
| 13 | muerde | Step 3 lee el `if` del fuente (`1328-1339`); mutante `drop_signal_step3` sin flag de disparo. |
| 14 | no muerde como fila | `g2_compare` fallaría el latch; nadie arma OR cerrado + entrada alta + G2 y exige diffs. |
| 15 | muerde | `Clock.Remove` filtra por identidad (`458-459`); `test_two_cells_independent_timers` exige A vivo tras borrar B. |
| 16 | muerde | `3014-3040`: `low_ms`/`low_span` ≥ 1100 y `flips_ii == 1`. |
| 17 | muerde | `test_timer_pdq_tie_both_orders:4135-4153` (`["t","pdq"]` y `["pdq","t"]`). |
| 18 | muerde | Mutante `no_cast_null` + `test_cast_null_na` (M después de la pasada del AND). |
| 19 | muerde | AND/XOR: fifo/lifo/20 perm. Latch viejo + M: igual en `2784-2794`. OR en banco se anota en base. |
| 20 | muerde con trampa | `retry_no_cap` no toca cadencia en el modelo (`1473-1481`); el tope lo pone el flag, no `retryRoundLimit` (H3). |

---

## D5 Laser → interruptor → Set en 592ebcc

En el fuente actual (mismo ancla que cita el plan) `if (gateEntityResolved && prevGateClosed != node.m_GateClosed) { m_AllocChanged = true;` está en `LFPG_ElecGraphImpl.c:2756-2758`. El modelo lo honra aunque no haya canal de señal.  
No corrí el árbol en `592ebcc`. Hipótesis: con Set por flujo, la segunda pasada del interruptor por `m_AllocChanged` puede asentar Set; entonces `test_d5_laser_switch_set` sería verde en la base y el autor acierta. Lo que tumba la hipótesis: worktree de `592ebcc` + `python -B .github/tools/test_graph_logic_signals.py TestD5.test_d5_laser_switch_set` (haría falta el test de este delta sobre scripts de la base) y mirar `cell.m_LatchState` y `e_set.m_AllocatedPower` tras `set_laser(True)`.

---

## Comprobado / no comprobado

**Comprobado (lectura `path:line`, no ejecución):** 69 `def test_*`; mutantes §6.6 y F/S; extractor de `IsPortReceivingSignal`; G2 oracle+latch; Clock; retry; D5; `:2758`; `:4818`/`:4890` sin `+ 1.0`.

**No comprobado (sin shell):** suite en verde (69/0); mutantes inyectados en caliente; rojo real en `592ebcc`; que `scalar_function` acepte cada cuerpo mutado sin `HarnessError`. Prueba que lo tumba: `python -B .github/tools/test_graph_logic_signals.py` en este worktree y el mismo sobre scripts de `592ebcc`.