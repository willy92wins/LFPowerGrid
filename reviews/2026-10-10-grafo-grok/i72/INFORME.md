# INFORME — lane i72 (referente independiente y checklist de cierre)

| punto | veredicto | ficheros:lineas | prueba |
|---|---|---|---|
| Paquete graph_reference (modelo + oraculo distinto de AllocateOutput) | HECHO | `.github/tools/graph_reference/model.py` L22-91, `oracle.py` L84-123 y L140-230, `README.md` | `test_graph_reference.py` exit 0, 9 tests, 0.003 s |
| Fixtures JSON con cuenta a mano | HECHO | `graph_reference/fixtures/*.json` campo `expected.hand_calc` | `FixtureExpectations.test_every_fixture_matches_hand_calc_expected` |
| Test positivos/negativos G-01 | HECHO | `test_graph_reference.py` L25-58 | split 0+25 marcado; 20+30 aceptado; no-conservacion y source_limit marcados |
| Paso CI | HECHO | `.github/workflows/checks.yml` L49-50 | estilo igual a los pasos vecinos |
| CIERRE-72.md | HECHO | `reviews/2026-10-10-grafo-grok/i72/CIERRE-72.md` | una fila por exigencia del parrafo de cierre |
| No tocar scripts/ | HECHO | — | whitelist respetada |

### Paquete `.github/tools/graph_reference/`

Modele SOURCE / PASSTHROUGH / CONSUMER / CAMERA, gates (`gate_closed` anula `pass_limit`), edges `enabled`, soft como `soft_demand` (bateria llena = 0). El oraculo es Edmonds-Karp sobre un grafo residual con super-fuente y nodos partidos (`oracle.py` `_build_hard_residual` L107-123, `Residual.max_flow` L84-105). No porta el split a partes iguales ni el all-off de `AllocateOutput` (`scripts/5_Mission/LFPG_ElecGraphImpl.c` ~L4058-4110 leido solo como semantica).

Descarte copiar Pass 1/2/3 del solver: heredaria G-01. Descarte enumeracion exhaustiva salvo fixtures pequenos: el max-flow cubre los mismos y escala.

Verificacion: offline, `C:\Python314\python.exe .github/tools/test_graph_reference.py` → 9 tests OK. API minima en `README.md`.

### Fixtures

Seis JSON, cada uno con `expected.hard_feasible`, `max_hard_servable`, `total_hard_demand` y `hand_calc`:

- `combiner_20_50_hard50.json`: 20+50=70, hard 50, min=50, 20+30.
- `heterogeneous_deficit.json`: 10+15=25 < 50, deficit 25.
- `shared_source_two_islands.json`: 40 vs 20+20.
- `gates_hard_soft.json`: hard 20 bloqueado + 10 abierto; max=10; total=30; no factible.
- `battery_full.json`: soft 0, hard 10.
- `cut_edge.json`: enabled false, max=0.

Decision: en gates, `total_hard_demand` incluye al consumidor detras del gate cerrado (conservador: factibilidad global, no solo la isla abierta).

### Tests

Negativos exigidos: `test_equal_split_overload_is_violation` L25-34 (0+25), `test_non_conservation_is_violation` L42-47, `test_source_limit_exceeded_is_violation` L49-58. Positivo 20+30: L36-40. Fixtures: L61-83.

Contra la base: `git show HEAD:.github/tools/test_graph_reference.py` → `fatal: path does not exist`. El test de arreglo no puede pasar en HEAD porque el fichero no existe.

### CI

Un paso al final de `checks.yml` L49-50, mismo estilo `python .github/tools/...`.

### CIERRE-72.md

Tabla de todas las exigencias del parrafo de cierre, G-04 SOURCE/CONSUMER/CAMERA con protocolo in-game, y las tres hipotesis abiertas con trigger.

## GATES

- Linter Enforce: no corrido. Motivo: no se toco `.c`, `.layout` ni `config.cpp`. Base y final N/A (delta 0 por no haber scripts).
- `C:\Python314\python.exe .github/tools/test_graph_reference.py` → exit 0, `Ran 9 tests in 0.003s OK`.
- `test_enforce_checks.py` → exit 0, 21 tests OK.
- `test_graph_capacity_refresh.py` → exit 0, 6 tests OK.
- `test_graph_charger_energy.py` → exit 0, 12 tests OK.
- `test_finish_wiring_quota.py` → exit 0, 7 tests OK.
- `test_broadcast_contract.py` → exit 0, 3 tests OK.
- `enforce_checks.py --root .` → `FAIL=0 WARN=0` exit 0.
- Tests de arreglo vs base: fichero ausente en HEAD (fallo). Con el cambio: 9/9 OK.

## HALLAZGOS-ADYACENTES

- `AllocateOutput` divide demanda de un PASSTHROUGH entre `CountPoweredIncoming` (`LFPG_ElecGraphImpl.c` ~L4023-4028). Eso es el mecanismo G-01; no se toco.
- `test_broadcast_contract.py` existe y no estaba en `checks.yml` antes de esta lane; no lo anadi (R20/R25).

## LO QUE NO PUDE VERIFICAR

- Compilacion cliente/servidor Enforce (no hay compilador invocables).
- Settlement en ticks/requeues in-game.
- G-04 SOURCE/CONSUMER/CAMERA intactos in-game.
- SHA de PBO, lease MCP, orden de mods.
- Que la lane #76 llame de hecho a esta API.

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

El issue trata #72 como cierre de capacidad/conservacion/settlement contra un referente, pero el dueno ya partio G-01 a #76 y dejo #72 como indice. El referente offline demuestra factibilidad de flujo, no que el solver del juego asiente. Si #76 no compara su asignacion con `Oracle.verify`, el paquete no cierra #72.

## Ronda 2

H1: demanda exigible = alcanzable (`reachable_ids` `oracle.py:116-144`; gate cerrado no expande; edge disabled se salta). Produccion: edge off `LFPG_ElecGraphImpl.c:3936`; gate cerrado `c:3979-3994`. `verify` no marca `hard_unmet` en no alcanzable. Fixtures `gates_hard_soft` y `cut_edge` rehechos. Test `H1ReachableDemand.test_closed_gate_correct_assignment_is_ok`.

H2: `absorbido = inflow + virt − outflow − self`, clip `[0, soft_demand]` (`_soft_absorbed` `oracle.py:178-186`). `hard_priority` solo con absorcion real. Test `H2SoftAbsorption`.

H3: conservacion dos lados en PASSTHROUGH (`oracle.py` bloque `in+virt` vs `out+self+soft`). `over_allocation` si consumidor recibe mas que demanda. Tests `H3TwoSidedConservation`.

H4: `max_hard_servable` entero por subconjuntos, tope 12 (`oracle.py:218-237`). Cota `max_hard_flow_bound`. `partial_allocation`. `heterogeneous_deficit` entero 0, cota 25. Consumidor binario `LFPG_ElecGraphImpl.c:2426-2433`.

H5: fixture `shared_source_forced_split.json`; negativo split 25/25 en comb.

H6: README y docstring: un unico max-flow, no fases.

H7: CIERRE-72 — tiempo/visitas/requeues/allocations dueño i78; lifecycle e indices dueño del repo.

Descartado: enumerar asignaciones de edges (explosion); el subconjunto es sobre consumidores.

### GATES (ronda 2)

Linter Enforce (sin tocar `.c`): `errors 0`, `warnings 48`, `status WARN`. La ronda 1 anoto 0/0; la base da 48 warnings.

`test_graph_reference.py` ahora: exit 0, `Ran 18 tests in 0.007s OK`.

Contra `51d0786` (paquete extraido con `git show`, mismos reproductores H1–H3): `FAILED (failures=4)`:

```
FAIL: test_h1 ... AssertionError: False is not true : [('hard_unmet', 'blocked'), ('hard_priority', '*')]
FAIL: test_h2 ... AssertionError: 'hard_priority' unexpectedly found in ['hard_unmet', 'hard_priority']
FAIL: test_h3a ... AssertionError: True is not false
FAIL: test_h3b ... AssertionError: 'over_allocation' not found in []
```

Resto: `test_enforce_checks.py` 21 OK; `test_graph_capacity_refresh.py` 6 OK; `test_graph_charger_energy.py` 12 OK; `test_finish_wiring_quota.py` 7 OK; `test_broadcast_contract.py` 3 OK; `enforce_checks.py --root .` FAIL=0 WARN=0.

## Ronda 3

P1: `hard_priority` ya no suma islas. `sources_reaching` (`oracle.py:148`) usa `reachable_from` por SOURCE. Una violacion por bateria que absorbe y comparte SOURCE con un nodo en `deficit_nodes` (`oracle.py:371-385`); `where` = id de la bateria. Descarte: min-cut entre bateria y deficit (mas aparato; compartir SOURCE basta). Tests `P1HardPriorityIslands`.

P2: README — comparar el conjunto de reglas, no `ok`, en deficit real; documentada la regla de `hard_priority` por SOURCE compartida.

CIERRE-72: fila gates actualizada.

### GATES (ronda 3)

`test_graph_reference.py`: exit 0, `Ran 20 tests in 0.041s OK`.

Contra `63b35b5` (`git show` del paquete, reproductor de islas disjuntas):

```
FAIL: test_disjoint ... AssertionError: Items in the first set but not the second:
'hard_priority'
FAILED (failures=1)
```

Resto de `test_*.py`: enforce_checks 21, capacity 6, charger 12, wiring 7, broadcast 3, todos exit 0. Linter no relanzado (no se toco `.c`); ronda 2: errors 0 warnings 48.
