# RONDA 3 — lane i76: corrige la SPEC y después impleméntala

Revisión de `47bcf8b`. S4, S5 (reparto N-way) y S7 quedan bien. S1–S3 tienen tres fallos de diseño:
los reproduje con números y los tres dejan un estado estable incorrecto. Esta ronda hace dos cosas,
en este orden y en esta misma sesión:

1. Corrige `SPEC.md` con lo de abajo.
2. Implementa la SPEC corregida. Ahora tienes shell: tests y linter.

Es la última ronda de SPEC. En la revisión siguiente solo miro estos puntos y el código.

## Contexto que no tenías en la ronda 2
- El motor converge reprocesando nodos **en el mismo epoch**. `MarkNodeDirty` rebobina
  `m_LastEpoch` (`LFPG_ElecGraphImpl.c:2097-2101`), y los requeues hacia abajo y hacia arriba hacen
  lo mismo (`:2797-2800`, `:2842-2846`). Un epoch es una llamada a `ProcessDirtyQueue` (`:2225`), y
  el presupuesto puede partir la cola entre epochs (`:2232-2235`).
- El edge es el mismo objeto en `m_Outgoing` y en `m_Incoming` (`AddEdgeInternal`, `:1505-1531`).
  Lo que un proveedor escribe en su edge, el merger lo ve por `m_Incoming`.
- He integrado el oráculo de i72 en tu rama: merge de `grok/i72-referente-independiente` en
  `63b35b5`. Léelo en `.github/tools/graph_reference/` de tu worktree, no en `wt-i72`; ahí ya
  están `over_allocation` y `partial_allocation`. i72 tiene una ronda 3 en curso que solo cambia
  cuándo salta `hard_priority` entre islas que no comparten fuente. No afecta a tus escenarios.

## B1 (BLOQUEANTE) — La lectura `Prev` esconde justo lo que el motor necesita ver
Con §3.2, dentro de un epoch cada llamada lee la oferta del epoch anterior (`Prev`). Pero los
requeues del mismo epoch existen para que el segundo proveedor vea lo que acaba de escribir el
primero. Con `Prev`, el reproceso lee lo viejo, no cambia nada, y nada vuelve a encolar a nadie en
el epoch siguiente.

Reproductor con tus reglas exactas (§3.2, §3.2.0 y §3.3):

1. Estado inicial: S30 y S50 → C → bomba 50. S50 alimenta además una lámpara de 40.
   - Ofertas: S30 = 30, S50 = 10.
   - Asks: 30 + 10. C recibe 40, overload, bomba apagada.
2. Se apaga la lámpara y corre S50.
   - Cap propio fresco 50, S30 = 30 → water-fill (25, 25) → pide 25.
   - Escribe 50, con `Prev` = 10.
   - C recibe 55 → `inputChanged` → reencola S30 y S50 (`:2720-2732`, `:2821-2851`).
3. Corre S30. S50 ya escribió en este epoch, así que lee `Prev` = 10 → (30, 10) → pide 30, sin
   cambio.
4. Corre S50. Lee `Prev` de S30 = 30 → (25, 25) → 25, sin cambio. No queda nada en cola.
5. Estado final, y permanente: S30 = 30 y S50 = 25. C recibe 55 para una demanda de 50, y el
   oráculo marca `conservation` en C.

Leyendo la última oferta escrita, S30 lee 50 en el paso 3 → (25, 25) → pide 25 → C recibe 50,
correcto en el mismo epoch.

Arreglo:

1. Cada llamada lee la **última oferta escrita** de cada hermano. Sin `Prev` ni `m_OfferEpoch`;
   basta un campo, `m_OfferedResidual`.
2. El cap propio es el fresco de esta llamada, y el ask propio nunca pasa de él. Esa es la garantía
   de seguridad: un merger nunca mete en overload a su proveedor.
3. Quien lee una oferta tiene que volver a correr cuando cambia. Si la oferta que P escribe en un
   edge cambia más que epsilon, o pasa de "nunca escrita" a escrita:
   - si el edge va a un merger, P marca dirty a los otros proveedores de ese merger;
   - si el target es un PASSTHROUGH que la usa como `base` (B3) y tiene una salida enabled a un
     nodo con 2 o más incoming, P lo marca dirty.

   Lo acota `LFPG_MAX_REQUEUE_PER_EPOCH`, y lo que se pase se aplaza al epoch siguiente, como hoy.
   En una red sin merger, este disparo no reencola nada (I4).

   Las cadenas de dos o más PASSTHROUGH entre la fuente y el proveedor directo quedan fuera:
   documéntalas como limitación, con un ejemplo y su consecuencia.
4. Rehaz S1.3 y S1.4:
   - La suma `min(D, Σcap)` se demuestra en el punto fijo: cuando no queda nadie en cola, todos
     leyeron las mismas ofertas.
   - Los transitorios son el presupuesto que parte la cola, los requeues, y un cap propio fresco
     frente a la oferta vieja de un hermano. Da la cota del exceso o defecto en el merger y por qué
     se cierra.

## B2 (BLOQUEANTE) — Una oferta real de 0 se lee como `m_MaxOutput`
El fallback de §3.2 salta con `cap < epsilon`. Una fuente en overload por otra rama ofrece 0 de
verdad (2.g), y sus hermanos leerían su `m_MaxOutput` en cada epoch. Es el mismo error que F1
corrigió para la demanda: "a published zero is real demand" (`:3951-3952`).

Reproductor: S20, S50 y S30 → C → 50. S50 ofrece 0 porque tiene una carga de 60 en otra salida.

- S20 y S30 leen (20, 50, 30) → (16,7; 16,7; 16,7) → piden 16,7 cada una.
- S50 lee (20, 0, 30) → pide 0.
- C recibe 33,3 de 50 en todos los epochs. La bomba no arranca nunca, y 20 + 0 + 30 = 50 es
  factible.

Arreglo: "nunca escrita" se marca con un centinela, p. ej. `m_OfferedResidual = -1` en el
constructor, y las ofertas escritas se recortan a >= 0. Solo el centinela usa el fallback; un 0
escrito es 0.

## B3 (BLOQUEANTE) — La oferta de un edge descuenta su propia demanda
`offerE = availableOutput - committedHardNoMerger` mete en `committed` la demanda del propio E
cuando E no va a un merger. En 2.c solo cuadra porque S50 tiene una sola salida (el caso "Un
outgoing unico").

Reproductor: S50 → lámpara 20 y S50 → Sp; Sp → l1 (10) y Sp → C; S20 → C; C → 50.

- Oferta S50→Sp = 50 − 20 − (10 + x), donde x es lo que Sp pide a C. El cap de Sp es esa oferta
  − 10.
- Pase a pase, x vale 10, 0, 10, 0... Oscila sin fin, y eso viola I5.
- Lo correcto: oferta S50→Sp = 50 − 20 = 30, cap de Sp = 20, asks 20 + 20. C recibe 40 < 50 y
  queda en overload por déficit, pero la lámpara y l1 siguen encendidas y nada oscila.

Arreglo: una sola fórmula para todo proveedor P y toda salida E, vaya o no a un merger, porque un
PASSTHROUGH aguas abajo la lee como oferta de arriba:

```
offer(P->E) = max(0, base(P) - hard comprometido en las OTRAS salidas enabled de P)
base(SOURCE)      = availableOutput
base(PASSTHROUGH) = 0 si el gate esta cerrado; si no,
                    min(m_MaxOutput, suma de ofertas de sus incoming enabled
                                     + m_VirtualGeneration - m_Consumption)
```

El autoconsumo y la generación virtual entran igual que en el throughput de `:2356-2407`. "Un
outgoing unico" deja de ser un caso aparte.

## M1 (MAYOR) — Un proveedor con varias salidas a mergers
§3.2.0 usa el mismo `selfCap` en cada edge que va a un merger.

Reproductor: Sp (alimentado por S50, 50) → C1 y C2; S20a → C1; S20b → C2; una bomba de 50 en cada
merger.

- Los dos water-fill dan 20 + 30, así que Sp pide 30 + 30 = 60 a S50.
- S50 entra en overload all-off y se apagan las dos bombas.

Con dos salidas del mismo Splitter al mismo Combiner pasa igual cuando hay déficit: las dos piden
contra el mismo cap y la fuente de arriba entra en overload.

Arreglo: el cap fresco de P se reparte entre todas sus salidas a mergers en un orden determinista
(id del target, y después puerto), y cada ask se recorta a lo que quede. En el reproductor: 30 a C1
y 20 a C2. C1 arranca, C2 se queda en déficit y S50 no cae.

Limitación, que va a `QUE PUEDE ESTAR MAL` con el número y se excluye del criterio de cierre (la
seguridad sí se cumple): el reparto local no siempre es factible.

- Ejemplo: con S40 en C1 en lugar de S20a, el reparto da 25/25 desde P, y C2 recibe 45.
- Pero 40 + 10 en C1 y 20 + 30 en C2 servirían las dos bombas, así que el oráculo marcará
  `feasible_but_underfed`.
- Lo comprobé con un modelo de cola: converge en 6 procesos, y P nunca pasa de 50.

## M2 (MAYOR) — Water-fill solo de la demanda hard, con el Pass 2 de hoy
§3.4 reparte la demanda hard del merger y pone `edge.m_Demand = ask`. Pero Pass 2 asigna como hard
`m_Demand * (1 - ratio del target)` (`:4098`), y Pass 1 cuenta como soft `m_Demand * ratio`
(`:4035`).

En 2.d el ratio de C es 20/70. Los asks 20 + 30 dan solo 14,3 + 21,4 = 35,7 de hard; el resto
depende del sobrante de Pass 3, y la batería no recibe nada. El "Surplus de S50 = 20" de 2.d no
llega.

Arreglo: el water-fill reparte la misma `edgeDemand` que hoy parte el divisor (`LastStable`, hard
+ soft), y Pass 1–3 quedan igual. En 2.d:

- D = 70 → asks 20 + 50.
- Hard: 14,3 + 35,7 = 50. Soft: 5,7 + 14,3 = 20.
- La bomba arranca y la batería recibe 20. Corrige 2.d con estas cifras.

Si Σcap queda entre la demanda hard y D, el hard de C depende del sobrante de Pass 3, que compite
con las otras salidas soft del proveedor. Esa limitación del modelo de ratio ya existe hoy: va a
`HALLAZGOS-ADYACENTES`.

## Menores
- m1. I4 dice "el texto ejecutado es el de hoy", y con la escritura de ofertas y las dos fases ya
  no lo es. Lo que se exige es que, en una red sin merger, las asignaciones, la decisión de
  overload y el retorno sean los de hoy (criterio 4).
- m2. En 2.g, "C entrega 20" no cuadra con el all-off vigente. C recibe 20, demanda 50, queda en
  overload y la bomba se apaga.
- m3. La fila del epoch 0 de §3.2.1 tiene notas sueltas ("60>50? ..."). Rehaz la traza de 2.c con
  las reglas nuevas. Incluye el pase en que Sp pide más de lo que recibe: Sp queda en overload
  hasta que S50 reasigna en el mismo epoch, por el requeue hacia arriba de `:2821-2851`.
- m4. El vector del water-fill está formado por los mismos incoming que cuenta
  `CountPoweredIncoming` (`:3845-3901`), para que K y el vector coincidan.

## Implementación
Postura con shell. La parte común del BRIEF (convenciones, tests offline, linter, fronteras) aplica
ahora entera.

LISTA BLANCA:
- `reviews/2026-10-10-grafo-grok/i76/SPEC.md`
- `reviews/2026-10-10-grafo-grok/i76/INFORME.md` (nuevo)
- `scripts/3_Game/LFPG_Data.c`: solo el campo nuevo de `LFPG_ElecEdge` y su inicialización.
- `scripts/5_Mission/LFPG_ElecGraphImpl.c`: `AllocateOutput`, el helper del water-fill y el
  disparo de B1.3.
- `.github/tools/test_graph_multifeed_split.py` (nuevo)
- `.github/workflows/checks.yml`: un paso para el test nuevo, al final.

Fuera de tu cambio:
- la decisión de overload (`:4058-4110`);
- la zona de i77 (`TrackVanillaCharger`..`TickVanillaChargers`) y la de i78
  (`MarkUpstreamNodesDirty`, cortes);
- `.github/tools/graph_reference/`: es de i72, léelo pero no lo toques.

## Criterio de hecho
1. `SPEC.md` corregida, con una sección final `## Ronda 3: cambios` que liste B1–B3, M1–M2 y m1–m4
   y dónde quedan.
2. El linter no tiene errores nuevos frente a tu base (delta por `len(errors)`).
   `enforce_checks.py --root .` y todos los `test_*.py` del repo pasan.
3. Pruebas en `.github/tools/test_graph_multifeed_split.py`, con las sentencias reales de
   producción.
   - El water-fill y la fórmula de oferta se ejecutan desde `LFPG_ElecGraphImpl.c`. Donde un bucle
     impida el slice, elige una opción y di en `INFORME` cuál:
     a. saca el cuerpo de cada iteración a un método escalar que el test cargue;
     b. fija por texto el bucle real, sentencia a sentencia, y modélalo en Python.

     Una reimplementación en Python comparada consigo misma no cuenta.
   - Escenarios: 2.a, 2.b, 2.c (la traza), 2.d, 2.e, 2.f y 2.g, más los reproductores de esta ronda
     (B1, B2, B3, M1 y su limitación, M2). Corren sobre un modelo de cola que sigue los requeues
     del motor y el disparo de B1.3.
   - Cada escenario comprueba las cifras y el veredicto de `Oracle.verify`:
     - `ok` en los factibles;
     - el conjunto exacto de reglas en los infactibles (2.b);
     - `feasible_but_underfed` en la limitación de M1.
   - Negativos por mutación. Cada uno tiene que hacer fallar al menos un test:
     - quitar el recorte `if (a > cap)`;
     - leer `Prev` (la regla de `47bcf8b`);
     - fallback con `cap < epsilon`;
     - una oferta que incluya la demanda del propio edge;
     - quitar el disparo de B1.3.
   - El test nuevo falla contra el código de la base (`463464e`). Deja la salida en `INFORME`.
4. I4: demuestra que, en una red sin merger, las demandas, la decisión de overload, las
   asignaciones y el retorno son los de la base. Sirven una comprobación por texto de que esas
   sentencias quedan idénticas a la base y el argumento de que el código nuevo no cambia ningún
   valor que ellas lean.
5. `INFORME.md` con `LO QUE NO PUDE VERIFICAR`, `HALLAZGOS-ADYACENTES`, y la prueba en juego como
   `PENDIENTE-INGAME`: el protocolo de §6.3, revisado con las reglas nuevas.

Mismo RECEIPT del BRIEF, con los paths de la lista blanca.
