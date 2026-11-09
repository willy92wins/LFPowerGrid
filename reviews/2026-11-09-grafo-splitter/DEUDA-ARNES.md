# Deuda del arnés de `test_graph_logic_signals.py`

Brief para rehacer el arnés aparte, después de la 1.3.1. **Nada de esto toca producción**: `scripts/` no
cambia desde bf2cb33 (`git diff 898de79 3a6d59a -- scripts` vacío). El test que queda en el PR es el de
eacd64c: con el arreglo, 69 tests en verde; en la base 592ebcc, 23 rojos (A1, A3, A4 x2, D5 por Splitter
y directo, S1-S17), 0 errores y 12 saltados.

Fuente: las R21 a ciegas del delta 898de79..eacd64c, en esta carpeta: `REVIEW-codex-r21-test2.md`
(NO APTO) y `REVIEW-grok-r21-test2.md` (APTO CON CAMBIOS). Líneas de `T` =
`.github/tools/test_graph_logic_signals.py` en eacd64c.

## Hallazgos abiertos

| Id | Gravedad | Dónde | Qué falla | Arreglo propuesto |
|---|---|---|---|---|
| Codex R21T-01 | BLOQUEA | T:1048, T:4105 | Con el fallback sin salidas a `edgeDemand = 1.0;` la suite sigue en verde: el modelo no interpreta esa rama y deja `extra=0` si el patrón no casa | Ejecutar la rama extraída o interpretar su expresión; patrón desconocido = HarnessError; negativo con constante 1 además de `m_Consumption + 1.0` |
| Codex R21T-02 | BLOQUEA | T:1173, T:3884, T:4200 | Con `MUTANT=None` y solo el texto mutado siguen verdes las F de `no_retry_empty`, `no_cast_null`, `compute_gate_cached` y otras doce: muerden por el flag de Python. `cached_latch_v3` muerde por el defecto equivocado (el flag quita también `QueueGateRetry`) | Derivar el comportamiento de cada mutante de producción del texto alterado; flags solo para mutantes del arnés; para el latch cacheado, mantener el reintento y probar una pasada irresoluble con caché abierta y salida que deba ser baja |
| Codex R21T-03 | BLOQUEA | T:1600, T:1707, T:1731 | G2 solo corre al final de `settle`: una OR cerrada con entrada alta, SOURCE encendida y sin colas pasa con `g2_calls=0`; la condición no exige temporizadores vacíos | Comprobar G2 en los puntos de asentamiento del planificador (PDQ y reloj) con todas sus precondiciones |
| Codex R21T-04 | IMPORTANTE | T:2976, T:2984, T:3017 | Rehabilitación (ii): Output de A se rehabilita y A publica en el mismo instante (13800 ms); sin relleno la fila sigue verde | Congestión entre la rehabilitación y la siguiente publicación de A; afirmar los dos instantes, la lectura baja continua de B y el retraso |
| Codex R21T-05 | IMPORTANTE | T:2766 | Las permutaciones del latch viejo vacían la baja de B antes de conectar Set: es la frontera con cambios separados, no la carrera | Cambios simultáneos en FIFO, LIFO y 20 semillas; orden consumido, G2 y el M permitido por L-RACE |
| Codex R21T-06 | IMPORTANTE | T:3253 | La vuelta se recupera por vanilla aunque `resolve_find` falle siempre | Vanilla y NetworkID fallando; solo vuelve el registro y se afirma que la entidad sale de él |
| Codex R21T-07 | IMPORTANTE | T:1797, T:1805 | El oráculo de autoconsumo usa `src.m_Powered`: Laser con entrada 1, consumo 5, `m_Powered=True` erróneo y Set alto da `g2_compare == []` | Calcular la cobertura del autoconsumo desde el flujo y la generación virtual (`LFPG_ElecGraphImpl.c:2412`, `:2423`); negativos que corrompan alimentación y señal a la vez |
| Codex R21T-08 | IMPORTANTE | T:2633, T:2671 | Filas que piden vaciado aceptan agotar el presupuesto sin vaciar (solo lo anotan) | Exigir el vaciado con el arreglo donde el contrato lo pide; anotar en la base |
| Codex R21T-09 | IMPORTANTE | T:4081, T:1059 | La asignación del modelo no cuenta las visitas de `AllocateOutput` (`LFPG_ElecGraphImpl.c:4804`, `:4968`, corte en `:2280`); ya existía en 898de79 | Reproducir los puntos reales de contabilización y añadir el discriminador de presupuesto de 1 arista |
| Grok H1 | IMPORTANTE | T:1713-1736, T:1968-1992 | G2 se salta las filas con `require_empty=False` (A1-A3, C y casi todas) | `require_empty=True` donde el plan pide vaciado; G2 también con `settled_no_retry` |
| Grok H2 | IMPORTANTE | T:2415-2438 | Las tres D5 con Power esperan siempre latch verdadero; no hay rama de rojo en la base | Ver la nota de desviación de abajo: se queda así |
| Grok H3 | IMPORTANTE | T:1473-1481, T:3914-3920 | `retry_no_cap` muerde por el flag, no por `retryRoundLimit` del texto | Extraer `retryRoundLimit` del fuente mutado |
| Grok H4 | MENOR | T:1351-1352 | El mutante del reproceso en la misma época sigue el flag `drop_signal_same_epoch`; está declarado equivalente en F | Sin cambio, salvo leerlo del texto al rehacer |
| Modelo frente a partida | IMPORTANTE | T:2415-2438 (`test_d5_laser_switch_set`); `GRAFO-SPLITTER-MEDICIONES.md:99` | D5 Laser -> interruptor -> Set: el modelo la da verde en la base (por `m_AllocChanged` en `592ebcc:2688-2690`); la partida la dio roja en la ronda 1 y no concluyente en la fase 3 | Repetir en BASE con el haz bien colocado y trazas PT_CHAIN, o encontrar qué no modela el arnés en esa variante |
| Grok H5 | MENOR | `g2_compare` T:1834-1835 | Ninguna fila construye una OR cerrada con entrada alta y exige `g2_compare != []` | Fila negativa para G2 |

## Testigos que no muerden del todo

- Según Codex (tabla de 20): 2 (vuelta por registro, R21T-06), 6 (mutantes por texto, R21T-02),
  7 (G2, R21T-03 y R21T-07), 8 (rehabilitación, vaciado; R21T-04 y R21T-08), 14 (OR cerrada con entrada
  alta, R21T-03), 16 (retraso de 1100 ms, R21T-04), 19 (órdenes con M, R21T-05) y 20 (`retry_no_cap`).
- Según Grok: 7 y 14 (G2 que no corre en cada vaciado).

## Hasta dónde llegaron las rondas 11 y 12

Commit `282b59a` en la rama lateral `claude/grafo-test-fidelidad` (no en el PR), en rojo:

- Hecho: `parse_graph_policy` lee del texto las ramas de los mutantes de producción (reintento, Cast
  nulo, compuerta cacheada, tope y cadencia, latch cacheado, baja global, rama de puerta, pre-gate) y
  separa los mutantes `harness_only`; G2 automático en `_finish_tick` y `run_events`; oráculo de
  autoconsumo por `insum + virt >= cons`; fallback interpretado; latch viejo simultáneo; vuelta solo por
  registro; vaciado exigido con el arreglo; dos visitas por arista en la asignación.
- Abierto: con el arreglo, 2 fallos (G2 en `e_out` y A que no publica tras rehabilitar en (ii)); en la
  base la suite pasa de 600 s y la corta el timeout.

## Desviación del plan

§6.4 esperaba en rojo en la base las tres D5 con Power. D5 Laser -> interruptor -> Set sale verde en
la base por `m_AllocChanged = true` en la transición de compuerta (`592ebcc:LFPG_ElecGraphImpl.c:2688-2690`,
`:2756-2758` en el árbol actual). Codex comprobó que ponerlo a false vuelve roja esa fila. Por eso los
rojos de la base en el test son 23 y no 24.

Pero la partida no coincide con el modelo. En la ronda 1 (11-09, 00:17-01:08 hora local, grafo igual al
de BASE) esta variante falló: «Laser -> interruptor encendido -> Set, con Power aparte, jugador a pie:
`gate 1` con `set 0 active 0`» (`reviews/2026-11-08-nostra-131/GRAFO-SPLITTER-MEDICIONES.md:99`). En la
fase 3 en BASE no fue concluyente (beam 0.0). El modelo la da verde en la base, la partida la dio roja
en la ronda 1 y no concluyente en la fase 3: es una discrepancia abierta entre modelo y partida. Con el
arreglo pasa en los dos.
