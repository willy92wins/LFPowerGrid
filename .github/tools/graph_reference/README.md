# graph_reference — oráculo independiente del grafo eléctrico

Paquete de librería estándar para validar **capacidad, conservación y
prioridad hard** sin copiar `AllocateOutput` / `ProcessDirtyQueue`.

El método es **un único flujo máximo (Edmonds–Karp)** sobre un grafo residual
con super-fuente, nodos partidos por `pass_limit` y super-sumidero en la
demanda hard alcanzable. No hay fases de prioridad en el flujo: la prioridad
hard/soft se comprueba en `verify`. Un solver que hereda el split a partes
iguales no puede usarse como referente de sí mismo.

## Semántica

- **Alcanzabilidad.** La demanda hard exigible es la alcanzable desde alguna
  SOURCE por edges `enabled` y PASSTHROUGH abiertos. Un gate cerrado
  (`scripts/5_Mission/LFPG_ElecGraphImpl.c:3979-3994`) no deja pasar y solo
  aporta `self_consumption` (autoconsumo o sondeo). Un edge deshabilitado
  (`:3936`) no cuenta. `verify` no marca `hard_unmet` en demanda no alcanzable.
- **Consumidores binarios.** En producción un CONSUMER/CAMERA está alimentado
  iff entrada ≥ `m_Consumption` (`:2426-2433`). `max_hard_servable` es la
  **medida entera**: búsqueda exhaustiva sobre subconjuntos (tope 12
  consumidores alcanzables). `max_hard_flow_bound` es la cota continua.
- **Soft.** Absorción de batería:
  `inflow + virtual_generation − outflow − self_consumption`, recortada a
  `[0, soft_demand]`. Batería llena = `soft_demand` 0.
- **hard_priority.** Solo si una batería que absorbe (`_soft_absorbed` > EPS)
  comparte al menos una SOURCE con un nodo con déficit hard (consumidor o
  PASSTHROUGH sin su autoconsumo). La alcanzabilidad es la de `reachable_ids`
  desde cada SOURCE. `where` es el id de la batería; `detail` nombra el nodo
  con déficit y la SOURCE compartida. Islas disjuntas no disparan la regla.
- **Déficit real.** En un escenario infactible `verify` devuelve `ok=False`
  con `hard_unmet` aunque la asignación sea la mejor posible. Quien compare
  contra el oráculo compara el **conjunto de reglas**, no `ok`.
- **Conservación de dos lados** en PASSTHROUGH: in+virt = out+self+absorbido.

## Entrada

JSON o dict:

```json
{
  "id": "ejemplo",
  "nodes": [
    {"id": "s20", "type": "SOURCE", "available": 20},
    {"id": "s50", "type": "SOURCE", "available": 50},
    {"id": "comb", "type": "PASSTHROUGH", "pass_limit": 500},
    {"id": "load", "type": "CONSUMER", "hard_demand": 50}
  ],
  "edges": [
    {"id": "e_s20", "src": "s20", "dst": "comb", "enabled": true},
    {"id": "e_s50", "src": "s50", "dst": "comb", "enabled": true},
    {"id": "e_out", "src": "comb", "dst": "load", "enabled": true}
  ]
}
```

Tipos: `SOURCE`, `PASSTHROUGH`, `CONSUMER`, `CAMERA`.

Opcionales: `soft_demand`, `soft_fraction`, `gate_closed`,
`self_consumption`, `virtual_generation`, `capacity` por edge.

## Salida

```python
from graph_reference import Oracle, load_graph

g = load_graph("fixtures/combiner_20_50_hard50.json")
o = Oracle()
o.hard_feasible(g)           # True  — existe 20+30 (binario)
o.max_hard_servable(g)       # 50.0  — medida entera
o.max_hard_flow_bound(g)     # 50.0  — cota continua
report = o.verify(g, {"e_s20": 0, "e_s50": 25, "e_out": 25})
report.ok                    # False
[v.rule for v in report.violations]
# hard_unmet, partial_allocation, feasible_but_underfed
```

Reglas de `verify`: `source_limit`, `edge_limit`, `edge_disabled`,
`passthrough_limit`, `conservation`, `hard_unmet`, `hard_priority`,
`feasible_but_underfed`, `non_negative`, `over_allocation`,
`partial_allocation`.

## Ejemplo numérico (cuenta a mano)

Fuentes 20 y 50, combiner, hard 50.

- Capacidad total = 20+50 = 70. Demanda hard alcanzable = 50.
- Cota continua `min(70, 50) = 50`. Medida entera = 50 (el consumidor enciende).
- Split 25+25: la fuente de 20 no puede; si queda 0+25, el consumidor recibe 25
  y no enciende → `hard_unmet` + `partial_allocation` + `feasible_but_underfed`.
