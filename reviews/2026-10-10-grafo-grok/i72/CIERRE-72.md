# Checklist de cierre — issue #72

Indice: #72 se cierra cuando cierren #76 (G-01 solver), #77 (G-02/G-03) y #78 (R-01/R-02/R-04/R-05). Esta tabla cubre el parrafo **Antes de merge/cierre** de #72. Una fila por exigencia.

| Exigencia | Donde se cubre | De quien |
|---|---|---|
| CI de cada PR | `.github/workflows/checks.yml` (pasos existentes + `test_graph_reference.py` en L49-50). Gatea PRs; no sustituye compilacion in-game. | dueno (CI) / i72 (paso del referente) |
| Compilacion cliente/servidor | Fuera de alcance offline: Enforce solo compila al cargar el mundo (`AGENTS.md` §6). Protocolo: arranque cliente+servidor con el PBO de la PR, 0 `Can't compile`. | dueno |
| Fixture y orden de mods registrados | Fuera de alcance de i72. Protocolo in-game: misma lista `-mod=` que el servidor privado, documentada en el HANDOFF. | dueno |
| Lease del MCP compartido | Fuera de alcance. Protocolo: `session_acquire_wait` / `dayz_test_run` segun runbook MCP; no matar DayZDiag. | dueno |
| SHA del PBO normal desplegado | Fuera de alcance. Protocolo: hash SHA-256 del PBO packed vs el desplegado en el servidor privado, anotado en el cierre de cada PR de #76/#77/#78. | dueno |
| Capacidad contra referente independiente | Offline: `Oracle.max_hard_servable` / `hard_feasible` (`.github/tools/graph_reference/oracle.py`). Fixtures `combiner_20_50_hard50.json`, `heterogeneous_deficit.json`. Test: `test_graph_reference.py` `FixtureExpectations` + `CombinerG01`. Settlement in-game (ticks hasta estable) es i76. | i72 (oraculo) / i76 (solver vs oraculo) |
| Conservacion contra referente | Offline: `Oracle.verify` reglas `conservation`, `source_limit`, `edge_limit` (`oracle.py` L167-198). Negativos: `test_non_conservation_is_violation`, `test_source_limit_exceeded_is_violation`. | i72 / i76 |
| Settlement contra referente | Offline no hay reloj de epochs. Comparar asignacion final con `Oracle.verify`: i76. Medir ticks/requeues: fila de tiempo (i78). | i76 / i78 |
| Fuentes heterogeneas y deficit real | Fixture `heterogeneous_deficit.json` (10+15 vs hard 50; cota continua 25, medida entera 0). Cuenta en `expected.hand_calc`. | i72 |
| Fuentes compartidas | Fixture `shared_source_forced_split.json` (reparto forzado 20+30 / 20) mas `shared_source_two_islands.json`. Negativo: `test_equal_split_at_comb_leaves_l1_unfed`. | i72 |
| Gates hard/soft | Fixture `gates_hard_soft.json`. Negativo prioridad: `test_hard_priority_soft_while_unmet`. | i72 |
| Adjuntos y energia del cargador | Offline ya cubierto en parte por `.github/tools/test_graph_charger_energy.py` (reloj del cargador). El referente i72 no modela el clock EM; i77/i78 si tocan adjuntos. Protocolo in-game: bateria en slot LargeBattery, energia vs `LFPG_CHARGER_ENERGY_PER_SEC`. | i77 / i78 / dueno |
| Apagones/cortes | Fixture `cut_edge.json` (`enabled=false`). Negativo: `test_cut_edge_flow_is_violation`. Apagon de fuente (available=0) no tiene fixture extra; se cubre poniendo `available: 0` en el mismo modelo. Cortes in-game: desconectar cable y ver islas. | i72 (edge cortado) / i76 (apagones en solver) |
| Bateria llena | Fixture `battery_full.json` (`soft_demand: 0`). Test: `test_battery_full_accepts_hard_only`. | i72 |
| Lifecycle | Fuera de alcance del referente. Hipotesis abierta del dueno (no es un bug asignado a i76). Protocolo in-game: spawn, persist, restart, delete. | dueno |
| Corrupcion deliberada de indices | Fuera de alcance i72. Hipotesis abierta del dueno (abajo); no es de i76. | dueno |
| Cambios locales: `componentId` e islas ajenas intactos | No modelado en el oraculo (no hay `m_ComponentId`). Protocolo in-game i76/i77: mutar una isla, leer `m_ComponentId` y potencia de la otra. Pendiente G-04 (fila siguiente). | i76 / i77 / dueno |
| Tiempo por evento/fase, visitas/requeues, allocations, ticks hasta settlement | Fuera de alcance del oraculo (no hay PDQ). Protocolo in-game: log `m_LastProcessMs`, `m_EdgesVisitedThisEpoch`, `m_RequeueCount`, `m_ValidateTickCount`. | i78 (medicion) |
| Conservar features, classnames, persistencia/RPC, SyncVars, permisos | i72 no toca `scripts/` ni `config.cpp`. Verificacion: diff de la PR sin esos paths. | i72 (esta lane) / cada PR |
| No reabrir CUT_PORT OUT ni targetPort legacy vacio | Fuera de alcance; no son bugs. No hay test que los reabra. | dueno |
| G-04: SOURCE, CONSUMER y CAMERA no cambian (pendiente in-game) | **PENDIENTE-INGAME.** Protocolo: en un grafo con una SOURCE, un CONSUMER y una CAMERA ya alimentados, mutar solo un PASSTHROUGH de otra isla (o el solver local). Comprobar que `m_OutputPower` / `m_InputPower` / `m_Powered` / `m_ComponentId` de SOURCE, CONSUMER y CAMERA permanecen iguales (epsilon de propagacion). Evidencia: captura MCP + log de nodos antes/despues. | dueno (G-04 resto cerrado 6-oct) |

## Hipotesis abiertas (no son bugs hasta trigger alcanzable)

| Hipotesis | Trigger que haria falta demostrar | De quien |
|---|---|---|
| Ratio-only soft | Un PASSTHROUGH con `m_SoftDemandRatio>0` y `m_SoftDemand=0` (o al reves) que haga overload/subalimentacion alcanzable en juego, no solo en un estado interno transitorio. | i76 / dueno |
| Indice parcialmente corrupto | Un `nodeId`/edge presente en un mapa (`m_Outgoing`) y ausente en `m_Nodes` (o al reves) tras un restore o un delete, con crash o isla fantasma reproducible. | dueno |
| Identidad provisional durante restore | Un deviceId temporal que sobreviva al restore y se publique a clientes / persistencia, de modo que un cable o un RPC apunte al id viejo. | dueno |

#72 permanece como indice hasta el cierre de #76, #77 y #78 mas el protocolo G-04 de arriba.
