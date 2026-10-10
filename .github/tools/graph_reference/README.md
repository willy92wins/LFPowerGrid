# graph_reference — oráculo independiente del grafo eléctrico

Paquete de librería estándar para validar **capacidad, conservación y
prioridad hard** sin copiar `AllocateOutput` / `ProcessDirtyQueue`.
El método es **flujo máximo (Edmonds–Karp)** con fases de prioridad hard:
super-fuente → fuentes → nodos partidos por `pass_limit` → super-sumidero
en la demanda hard. Un solver que hereda el split a partes iguales no
puede usarse como referente de sí mismo.

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

Tipos de nodo: `SOURCE`, `PASSTHROUGH`, `CONSUMER`, `CAMERA`.

Campos opcionales: `soft_demand` (carga de batería; **batería llena = 0**),
`soft_fraction`, `gate_closed` (passthrough no deja pasar),
`self_consumption`, `virtual_generation`, `capacity` por edge.

## Salida

```python
from graph_reference import Oracle, load_graph

g = load_graph("fixtures/combiner_20_50_hard50.json")
o = Oracle()
o.hard_feasible(g)          # True  — existe 20+30
o.max_hard_servable(g)      # 50.0
report = o.verify(g, {"e_s20": 0, "e_s50": 25, "e_out": 25})
report.ok                   # False
[v.rule for v in report.violations]
# hard_unmet, feasible_but_underfed
```

`VerifyReport.violations`: cada una tiene `rule`, `where` (node/edge id),
`detail`. Reglas: `source_limit`, `edge_limit`, `edge_disabled`,
`passthrough_limit`, `conservation`, `hard_unmet`, `hard_priority`,
`feasible_but_underfed`, `non_negative`.

## Ejemplo numérico (cuenta a mano)

Fuentes 20 y 50, combiner, hard 50.

- Capacidad total = 20+50 = 70.
- Demanda hard = 50.
- `min(70, 50) = 50` → factible. Asignación 20+30.
- Split a partes iguales 25+25: la fuente de 20 no puede dar 25; si el
  solver la pone a 0 y deja 25 de la de 50, el consumidor recibe 25 < 50
  aunque 20+30 era factible → el oráculo marca `hard_unmet` +
  `feasible_but_underfed`.
