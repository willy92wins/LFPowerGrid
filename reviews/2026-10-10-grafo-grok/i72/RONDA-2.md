# RONDA 2 — lane i72 (referente independiente)

Revisión de `51d0786`. La base es buena: flujo máximo con nodos partidos, fixtures con la cuenta a
mano y el negativo del reparto a partes iguales. Hay siete correcciones. H1, H2 y H3 son defectos
del oráculo con reproductor: cada reproductor tiene que acabar como test que **falla contra
`51d0786`** y **pasa con tu cambio** (pega la salida del fallo en `## GATES`).

## H1 (defecto) — Demanda detrás de un gate cerrado o de un edge deshabilitado
Hoy cuenta como hard exigida. Reproductor:
`verify(gates_hard_soft, {"e_open": 10, "e_hard": 10, "e_bat_in": 20})` es la asignación correcta
y devuelve `hard_unmet` en `blocked` y `hard_priority` en `*`.

En producción un gate cerrado solo demanda su autoconsumo o la demanda de sondeo, y un edge
deshabilitado demanda 0 (`scripts/5_Mission/LFPG_ElecGraphImpl.c` ~3936 y ~3975-3995; cítalo
con `path:line` de tu árbol).

Arreglo: la demanda hard exigible es solo la **alcanzable** desde alguna fuente por edges
habilitados y PASSTHROUGH abiertos. Un gate cerrado aporta solo su `self_consumption`. `verify`
distingue la demanda no alcanzable y no la marca como `hard_unmet`. Rehaz las expectativas de
`gates_hard_soft` y `cut_edge` con su cuenta a mano.

## H2 (defecto) — Absorción soft de una batería que deja pasar energía
Hoy la absorción se calcula como `inflow − self_consumption`. Reproductor:

- `src` 30 → `bat` (PASSTHROUGH, `soft_demand` 20, `soft_fraction` 1.0) → `l1` (hard 10)
- `src` → `l2` (hard 25)
- `verify` con `{"a": 10, "b": 10, "c": 20}` devuelve `hard_priority` en `*`

La batería no absorbe nada: entra 10 y sale 10.

Arreglo: `absorbido = inflow + virtual_generation − outflow − self_consumption`, recortado a
`[0, soft_demand]`. `hard_priority` solo salta si hay absorción soft real mientras queda hard
alcanzable sin servir.

## H3 (defecto) — Conservación de un solo lado
Reproductores con el fixture `combiner_20_50_hard50`, los dos devuelven hoy `ok=True`:

- `{"e_s20": 20, "e_s50": 50, "e_out": 50}`: el combiner recibe 70 y reparte 50.
- `{"e_s20": 20, "e_s50": 50, "e_out": 70}`: el consumidor recibe 70 de 50.

Arreglo:
- Conservación de dos lados en PASSTHROUGH, con EPS: lo que entra + virtual = lo que sale +
  autoconsumo + absorbido soft.
- Una regla nueva, `over_allocation`, cuando un consumidor recibe más que su demanda.

## H4 (semántica) — Los consumidores de producción son binarios
Un consumidor está alimentado si y solo si su entrada ≥ `m_Consumption`
(`LFPG_ElecGraphImpl.c` ~2426-2433). `max_hard_servable` es continuo: en
`heterogeneous_deficit` (10+15 → combiner → 50) espera 25, pero el máximo real es 0, porque el
consumidor no enciende con 25.

Arreglo:
- `max_hard_servable` pasa a ser la medida entera: búsqueda exhaustiva sobre subconjuntos de
  consumidores, con un tope declarado (por ejemplo 12 consumidores, para seguir por debajo de
  10 s) y un error claro si se supera.
- Si conservas la continua, renómbrala como cota (por ejemplo `max_hard_flow_bound`).
- Regla nueva `partial_allocation`: un consumidor que recibe algo, pero menos que su demanda.
- Las expectativas de los fixtures se dan en la medida entera, y en la cota si la conservas.

## H5 (cobertura) — Una fuente compartida que fuerce un reparto concreto
`shared_source_two_islands` es trivial: no fuerza ningún reparto. Añade este fixture:

- `s50` → `splitter` (`pass_limit` 200) → `comb` y `l2` (hard 20)
- `s20` → `comb`
- `comb` → `l1` (hard 50)

La única asignación que alimenta todo es: s20→comb 20, splitter→comb 30, splitter→l2 20. Ponlo
con su cuenta a mano. Añade un negativo: el reparto a partes iguales en `comb` (25/25) deja `l1`
sin alimentar y el oráculo lo marca.

## H6 (menor) — Docstring y README
Hablan de "fases de prioridad hard", pero el código hace un único flujo máximo hacia la demanda
hard. Que digan lo que hace el código.

## H7 (CIERRE-72) — Dueños
- Tiempo por evento y fase, visitas, requeues y allocations: dueño i78 (medición).
- Lifecycle y corrupción deliberada de índices: dueño el propietario del repo, como hipótesis
  abiertas. No son de i76.

## Criterio de hecho de esta ronda
1. Los reproductores de H1, H2 y H3 son tests en `test_graph_reference.py`. Fallan contra
   `51d0786` (salida en GATES) y pasan ahora.
2. H4: tests de la medida entera y de `partial_allocation`, con positivos y negativos.
   `heterogeneous_deficit` da 0 en la medida entera.
3. H5: el fixture nuevo y su negativo.
4. README con la semántica nueva (alcanzabilidad, consumidores binarios, reglas nuevas) y el
   ejemplo actualizado.
5. Sigue en pie todo el criterio de la ronda 1: solo librería estándar, menos de 10 s, y el resto
   de tests y `enforce_checks` en verde.
6. En GATES, los warnings del linter tal como salen. En la ronda 1 escribiste 0/0 y la base da 48.

Lista blanca: la misma de la ronda 1. Añade al final de `INFORME.md` una sección `## Ronda 2` con
lo que cambió y por qué. Mismo RECEIPT.
