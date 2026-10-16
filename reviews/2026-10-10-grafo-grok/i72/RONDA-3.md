# RONDA 3 — lane i72 (referente independiente). Última ronda

Revisión de `63b35b5`, solo el delta de la ronda 2. H1–H5 quedan resueltos: reproduje los cinco, y
el fichero de tests nuevo da 10 fallos y 7 errores contra el oráculo de `51d0786` y pasa entero
ahora (18). Queda un punto, en la regla que cambiaste esta ronda, y una línea de documentación.

Si P1 y P2 quedan como se piden, abro el PR de #72 con esta ronda.

## P1 — `hard_priority` salta entre islas que no comparten fuente
`verify` suma la absorción soft de todas las baterías y el déficit hard de todos los nodos
alcanzables, y salta si las dos sumas son mayores que cero (`oracle.py:346-356`). No mira si la
batería podía ceder esa potencia al nodo con déficit.

Reproductor (corrido con tu oráculo):

- `srcA` (SOURCE 30) → `bat` (PASSTHROUGH, `soft_demand` 20, `pass_limit` 100), edge `a`.
- `srcB` (SOURCE 10) → `l` (CONSUMER, `hard_demand` 25), edge `b`.
- `verify(g, {"a": 20, "b": 0})` → `[('hard_unmet', 'l'), ('hard_priority', '*')]`.

La batería está en otra isla: aunque no cargase, `l` no recibiría nada más. Lo correcto es solo
`hard_unmet` en `l`.

Arreglo: `hard_priority` salta solo si una batería que absorbe (`_soft_absorbed` > EPS) comparte
al menos una SOURCE con un nodo que tiene déficit hard. Ese nodo puede ser un consumidor o un
PASSTHROUGH que no recibe su autoconsumo. "Comparte" usa la misma alcanzabilidad que `reachable_ids`:
edges enabled y sin atravesar un gate cerrado, desde cada SOURCE por separado.

- Una violación por batería, con `where` = id de la batería.
- En `detail`, el nodo con déficit y la SOURCE compartida.

Tests:

- Negativo: el reproductor de arriba da exactamente `{hard_unmet}`. Tiene que fallar contra
  `63b35b5` (hoy añade `hard_priority`).
- Positivo, fuente compartida: `src` (SOURCE 30) → `bat` (soft 20) y `src` → `l2` (hard 25), con
  `{"a": 20, "b": 0}`. Da `hard_priority` con `where` = `bat`, además de `hard_unmet` y
  `feasible_but_underfed`.
- `test_hard_priority_soft_while_unmet` sigue pasando.

## P2 — README: déficit real
Añade al `README.md` una línea: en un escenario infactible `verify` devuelve `ok=False` con
`hard_unmet`, aunque la asignación sea la mejor posible. Quien compare contra el oráculo compara el
conjunto de reglas, no `ok`. Documenta también la regla nueva de `hard_priority`.

## Criterio de hecho
- Los tests de P1 fallan contra `63b35b5` y pasan ahora. Todo `test_graph_reference.py` pasa.
- Todos los `test_*.py` del repo pasan.
- Lista blanca: `oracle.py`, `README.md`, `test_graph_reference.py`, `CIERRE-72.md` si cita la
  regla y `INFORME.md`, todos de la ronda 1.
- Añade al final de `INFORME.md` una sección `## Ronda 3` con lo que cambió.
- Mismo RECEIPT.
