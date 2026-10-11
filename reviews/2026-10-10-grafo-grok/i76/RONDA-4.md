# RONDA 4 — lane i76: un bloqueo del reparto, tres correcciones y la SPEC al día

Revisión de `21ce462`. Lo central está bien: ofertas escritas con centinela, water-fill N-way,
B1.3, `remaining` entre mergers y M2. Gates en verde, 23 tests en verde y el linter sin errores
nuevos. Queda un fallo que deja la red clavada, un riesgo de compilación, un coste por encima del
que promete la SPEC, y una SPEC que todavía arrastra bloques de la ronda 2 que contradicen la 3.

Es la última ronda. En la revisión siguiente solo miro estos puntos.

## B4-1 (BLOQUEANTE) — Un proveedor sin entrada se sale del vector y pide la demanda entera

En `ApplyMergerWaterFill`, el vector solo admite incoming con `supplierPower > epsilon` (`:4339`),
también el del propio proveedor. Si el proveedor es un PASSTHROUGH que acaba de perder su entrada,
`selfIndex` se queda en -1 y `:4354` hace `continue`, así que el edge conserva la demanda entera de
Pass 1. La base al menos dividía por `CountPoweredIncoming`.

Reproductor con tu `QueueModel` y los helpers de producción. Le añadí el requeue hacia arriba que
hace el motor cuando cambia la señal de demanda de un PASSTHROUGH (`:2768`, `:2808-2851`), que el
modelo no tiene:

1. Montaje: S20, S30 y Sp → C → bomba 50. S50 → Sp. Sp → l1 (10).
2. Todo encendido: 16.7 + 16.7 + 16.7 hacia C y l1 encendida. Bien.
3. Se apaga S50. Sp se queda sin entrada y sale del vector, así que pide 50 a C y su señal sube
   a 60.
4. Vuelve S50. Ve 60 > 50 y hace all-off. Sp sigue sin entrada y sigue pidiendo 50.
5. Estado final, y permanente: l1 apagada y S50 en overload. La base `463464e` se recupera: pide
   50/2 = 25, la señal de Sp es 35 y S50 la sirve.

Dos variantes del mismo fallo:

- Con un solo hermano (S20 + Sp) también se clava la base, porque `CountPoweredIncoming(C) = 1`
  y no divide.
- Con dos edges de Sp al mismo Combiner (Sp OUT2 → C IN1 y Sp OUT3 → C IN2) se clavan r3 y la
  base, con una señal de 110. El grafo lo admite: `AddEdgeInternal` solo rechaza un edge con el
  mismo origen, destino y puertos (`:1487-1501`).

Arreglo. Una regla, la misma para detectar el merger y para construir el vector:

1. El vector del edge E (P → C) tiene a E, con cap = `remaining`, y a cada otro incoming enabled X
   de C que cumpla una de estas dos condiciones:
   - X sale del propio P, esté como esté de potencia;
   - el origen de X pasa el predicado de `CountPoweredIncoming` (`:3845-3901`).

   Cada X usa `OfferCapFromWritten`.
2. `selfIndex` es la posición de E por identidad (`cpEdge == mEdge`), no por
   `m_SourceNodeId == nodeId`.
3. E es edge de merger si su vector tiene 2 entradas o más. La detección del primer bucle (`:4240`)
   usa esta misma regla en vez de `CountPoweredIncoming(target) > 1`.
4. El predicado va en un helper escalar que carguen los tests:
   `IncludeInMergerVector(bool fromSelfNode, float supplierPower)`. `supplierPower` se calcula en
   un solo sitio para los dos bucles nuevos. `CountPoweredIncoming` no se toca.
5. B1.3 no cambia: sigue con `CountPoweredIncoming > 1`.

Lo probé en el modelo con esta regla. Los 14 escenarios actuales siguen en verde, y los tres
montajes (dos hermanos, uno solo, edges paralelos) se recuperan al apagar y encender S50.

## M4-1 — `MergerSortLess` compara `string < string`

`:4046` y `:4065` hacen `return ca < cb;` entre dos strings. No sabemos si Enforce lo compila; tu
INFORME lo deja en "no pude verificar". Compara códigos enteros con `ToAscii`, como ya hace el
repo: `string ch = msg.Substring(ci, 1); int code = ch.ToAscii();`
(`LFPG_RPCServerHandlerImpl.c:2520-2521`, con la nota de `:2509`). Los ids son `low:high` en
ASCII (`LFPG_Util.c:85-90`).

## M4-2 — Coste por encima del que promete la SPEC

1. `PublishEdgeOffers` llama a `OtherEnabledHard` por cada edge (`:4203`), y cada llamada recorre
   todas las salidas con un `m_Nodes.Find`. Sale O(salidas²) en cada `AllocateOutput` de
   cualquier red, y la SPEC dice O(salidas). Suma el hard una vez y pasa
   `ComputeOfferTowardEdge(baseP, totalHard, ownHard)`, con
   offer = max(0, baseP − (totalHard − ownHard)). `OtherEnabledHard` y `SkipOtherIndex` se quedan
   sin uso: bórralos, son de esta ronda.
2. El bucle que recalcula los totales (`:4530-4550`) corre en todos los nodos. Haz que
   `ApplyMergerWaterFill` devuelva `bool` (si reescribió alguna demanda) y recalcula solo entonces.
3. Los bucles nuevos no suman `m_EdgesVisitedThisEpoch`, y el presupuesto del PDQ se mide con ese
   contador (`:2234`). Súmalo en cada visita de edge, como hace `CountPoweredIncoming` (`:3866`).

## M4-3 — La SPEC se contradice

Quedan bloques de la ronda 2 que dicen lo contrario que la ronda 3 y que el código. Bórralos o
reescríbelos, para que §3 describa una sola regla: la del código.

- §3.1: la condición pasa a ser la regla de B4-1.
- §3.2: sobran "Oferta de un SOURCE" (`committedHardNoMerger`, el caso de un solo outgoing) y
  "Oferta de un PASSTHROUGH" (`upstreamOffer` de snapshot, `headroomPass`). La fórmula es la de B3.
- §3.2: "Prueba de suma simétrica" y "Si se leyeran valores distintos… Prev/Pub" ya los sustituyen
  S1.3 y S1.4.
- §3.2.0: el tope v2.4 se aplica a la D de Pass 1 de cada proveedor, como hace el código; no a
  `ask[self]` después del reparto. El ask ya queda ≤ `remaining`.
- §3.2.1: la traza habla de "snapshot" y de "ofertas iniciales 0". Ahora hay centinela -1 con
  fallback `m_MaxOutput`, y la D de arranque es la de Pass 1 de cada proveedor. Rehazla con las
  reglas actuales.
- §3.3: D es la LastStable entera (M2), no la "demanda hard". El orden del vector es el de
  `m_Incoming` del merger, que es el mismo array para todos sus proveedores; no "ids
  lexicográficos".
- §3.4: "Water-fill usa la demanda hard" contradice M2.
- §3.5: quita "snapshot".
- 2.c: el 40 sale de B3 (`min(200, 50) - 10`), no de `min(pass_limit - hard_otras,
  upstreamOffer - hard_otras)`.
- I4: si ningún PASSTHROUGH tiene 2 o más incoming enabled, no se reescribe nada y los valores son
  los de la base.
- §5: un campo, no tres; los helpers con sus nombres reales; sin "tope v2.4 post-split".
- §6.1: fuera "núcleo K=2", "solo hard" (fila d), "min(500,sumCap)" (fila e) y "esta ronda no
  ejecuta Python".

Añade `## Ronda 4: cambios` al final.

## m4-1 — Tests

1. Regresión de B4-1 en el modelo de cola: apagar y encender S50 en los tres montajes de arriba.
   Pasa si l1 queda encendida, S50 sin overload y la bomba encendida donde es factible. Antes, el
   modelo necesita el requeue hacia arriba por cambio de señal (`:2768`, `:2808-2851`). Con él,
   los 23 tests actuales siguen pasando; lo comprobé.
2. Negativo de B4-1: con `IncludeInMergerVector` sin la rama de `fromSelfNode`, el test 1 falla.
3. S3, que promete la tabla de §6.1: las salidas de Sp en orden [l1, C] y en orden [C, l1] dan los
   mismos asks.
4. 2.d en el modelo de cola, con una rama soft junto al merger:
   - Montaje: S50 → batería (`m_SoftDemand` 40, ratio 1) y S50 → C; S20 → C; C → bomba 50.
   - Correcto: bomba encendida (20 + 30) y batería 20 por Pass 3.
   - Hoy, un `EdgeHardPortion` con `hard = demand` sobrevive a toda la suite. En este escenario
     apaga la bomba y da 40 a la batería.
   - La batería necesita su señal de demanda en el modelo, como en el motor (`:2546`, `:2595`).
5. Dos negativos no cumplen el criterio de la ronda 3, que pedía que la mutación hiciera fallar
   algún test:
   - `test_include_own_edge_in_other_hard` muta una llamada que el modelo no carga y solo
     comprueba que el texto cambió. Con M4-2, muta `ComputeOfferTowardEdge` para que no reste el
     hard del propio edge: falla 2.a.
   - `test_ignore_last_write_is_prev` tiene que ser de comportamiento. Montaje: S50 → Sp → (l1 30,
     C); S40 → C; C → bomba 50. Correcto: oferta Sp→C 20, asks 30 + 20 y bomba encendida. Con
     `return fallbackMax`, S40 pide 25 y la bomba se apaga.
   - B1.3 se queda con el negativo de helper (`test_no_b13_notify`). En estas topologías los
     requeues que ya existen (`m_AllocChanged`, `outputDelta`, `inputChanged`) marcan los mismos
     nodos, así que no hay un escenario donde B1.3 cambie por sí solo el resultado. No lo busques.

Las cifras de 1, 4 y 5 las comprobé en el modelo con los helpers de producción.

## Lista blanca
- `scripts/5_Mission/LFPG_ElecGraphImpl.c`: los helpers de G-01, `ApplyMergerWaterFill`,
  `PublishEdgeOffers` y el recálculo de totales en `AllocateOutput`.
- `.github/tools/test_graph_multifeed_split.py`
- `reviews/2026-10-10-grafo-grok/i76/SPEC.md`
- `reviews/2026-10-10-grafo-grok/i76/INFORME.md`

`LFPG_Data.c` y `checks.yml` no cambian. Fuera de tu cambio, lo de siempre: la decisión de
overload, las zonas de i77 e i78 y `.github/tools/graph_reference/`.

## Criterio de hecho
1. B4-1, M4-1 y M4-2 en el código. Ninguna comparación `<` o `>` entre strings en el diff.
2. La SPEC sin las contradicciones de M4-3, con `## Ronda 4: cambios`.
3. Los tests de m4-1 en verde, y con el negativo de B4-1 el test 1 falla. Todos los `test_*.py`
   y `enforce_checks.py --root .` pasan. El linter sin errores nuevos (delta por `len(errors)`).
4. `INFORME.md` al día: qué cambió en esta ronda y la salida de los tests. Lo de `ca < cb` sale de
   "no pude verificar".

Mismo RECEIPT del BRIEF, con los paths de la lista blanca.
