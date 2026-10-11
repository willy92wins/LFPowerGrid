# RONDA 5 — lane i76: el reparto no se recupera por el camino incremental

La prueba en partida de la rama de integración (`8556f9f`, servidor local) compila en cliente y
servidor, y los montajes A a E salen bien. El F falla si se monta cable a cable y sale bien tras
reiniciar el servidor. Lo reproduje en tu `QueueModel` con los helpers de producción: es un fallo
de G-01 en el camino incremental, y también afecta al montaje C y al t4b de la ronda 4.

En la revisión siguiente solo miro estos puntos. Las líneas son de `a7b9c77`.

## B5-1 (BLOQUEANTE) — Un proveedor sin corriente no se recalcula cuando aparece un hermano

Montaje C de la partida: generador 50 → Sp → (lámpara 10, C entrada 1); panel 20 → C entrada 2;
C → bomba 50 (PASSTHROUGH, consumo 50).

1. Sin el cable del panel, Sp es el único proveedor de C y pide 10 + 50 = 60. El generador entra
   en overload (all-off) y Sp se queda sin entrada. Es lo correcto con las reglas de la base.
2. Se conecta panel → C. `OnWireAdded` marca el panel y C (`:886-887`).
3. El panel arma su vector sin Sp, porque `SupplierPowerForMerger(Sp)` es 0. Pide D = 50 y entra
   en overload.
4. El panel publica su primera oferta, pero `NotifyOfferChanged` solo marca a los hermanos si
   `CountPoweredIncoming(C) > 1` (`:4149`), y Sp no cuenta. Sp no se marca.
5. C pasa de entrada 0 a entrada 0 y su señal no cambia, así que no reencola hacia arriba.
6. Nadie recalcula Sp. Su demanda de 60 mantiene el generador en overload, y Sp sin corriente
   mantiene al panel en overload. Es permanente. Una reconstrucción completa sale bien, y cortar y
   reponer C → bomba también, porque el cambio de señal reencola a los dos proveedores.

Variantes del mismo fallo:

- Panel apagado y encendido (noche y día) en C y en F: mismo estado final, todas las mañanas.
- t4b de la ronda 4 con `e40` conectado el último, o S40 apagado y encendido.
- F (una bomba en lugar de la lámpara) con generador → Sp conectado el último. Sp recibe
  corriente, pero su oferta hacia C no cambia (0 → 0), así que el panel nunca se entera de que
  Sp ha entrado en su vector.

Desde B4-1, todo proveedor, tenga corriente o no, calcula su propio water-fill con las ofertas de
los hermanos con corriente. La regla de "a quién hay que recalcular" ya no coincide con la de
"quién lee la oferta".

### Arreglo: dos reglas en el re-dirty, nada más

1. **`NotifyOfferChanged`:** quita la condición `CountPoweredIncoming(targetId) > 1` del bucle de
   hermanos (`:4149`). Se marca todo incoming enabled del target cuyo origen no sea el proveedor.
   Un target con una sola entrada no tiene hermanos, así que ahí no cambia nada. El segundo bucle
   (target PASSTHROUGH que alimenta un merger, `:4187`) se queda como está.
2. **`PublishEdgeOffers`:** si el proveedor es un PASSTHROUGH y su suministro cruzó epsilon desde
   su pasada anterior, llama a `NotifyOfferChanged` en cada salida enabled aunque la oferta no
   haya cambiado. La oferta de un PT no depende de su entrada real (su base son las ofertas que le
   llegan), así que puede entrar o salir del vector de sus hermanos sin que cambie ninguna oferta.
   - Suministro anterior: `m_PrevInputPower`, que durante `AllocateOutput` todavía guarda la pasada
     anterior. Solo se escribe en `:2732`, después de la llamada de `:2507`. Actual: `m_InputPower`.
   - El predicado es el de `SupplierPowerForMerger` y vive en un solo sitio. Añade el helper
     escalar `PassthroughSupplyAt(float inputPower, float virt, float cons, bool gateClosed)` y haz
     que la rama PASSTHROUGH de `SupplierPowerForMerger` lo llame.
   - Para comparar, añade `SupplierVisibilityFlipped(float prevSupplier, float curSupplier)`, que
     devuelve true si uno es mayor que epsilon y el otro no.
   - Los dos helpers son escalares y los cargan los tests.
   - Usa el mismo `m_GateClosed` para la pasada anterior y la actual. Una transición de gate ya
     reencola por su propio camino (`:2674`).

Lo comprobé en el modelo, con los montajes A, B, C y F de la partida más b41_three y t4b:

| Variante | Un cable añadido o cortado, igual que una reconstrucción | Montaje cable a cable en todos los órdenes, más apagar y encender cada fuente |
|---|---|---|
| Código actual | 53 de 60 | 1720 de 1819 |
| Solo la regla 1 | 59 de 60 (falla F con generador → Sp el último) | |
| Solo la regla 2 | 54 de 60 | |
| Las dos reglas | 60 de 60 | 1819 de 1819 |

Con las dos reglas, los 29 tests actuales siguen en verde.

## m5-1 — Tests

1. **El `QueueModel` refleja las dos reglas:**
   - `notify_targets` sin la condición de potencia en el bucle de hermanos;
   - `publish_offers` con el cruce de epsilon, usando los helpers cargados;
   - `supplier_power`, en su rama PASSTHROUGH, a través de `PassthroughSupplyAt`.

   Carga los dos helpers nuevos en `helpers()`.
2. **Cortar y reponer un cable:** el edge repuesto es nuevo (`m_OfferedResidual` -1,
   `m_AllocatedPower` 0, `m_Demand` 0), y solo se marcan su origen y su destino, como en
   `OnWireAdded` y `OnWireRemoved` (`:886-887`, `:993-994`).
3. **Escenarios de la partida.** El estado final tiene que ser el de la reconstrucción:
   - C con panel → C conectado el último: e20 20, esp 30, lámpara 10, bomba 50 y nadie en
     overload.
   - C con el panel apagado y encendido: lo mismo.
   - F con panel → C el último, y F con generador → Sp el último: bomba del Splitter 50, esp 0,
     e20 20, C en overload, y ni el generador ni el panel en overload.

   Piezas de la partida: bomba PASSTHROUGH con consumo 50 y capacidad 100, lámpara PASSTHROUGH
   con consumo 10 y capacidad 50, y nevera CONSUMER de 20.
4. **Propiedad.** En A, B, C, F, b41_three y t4b, cuatro casos:
   - cada cable conectado el último;
   - cada cable cortado y repuesto desde el estado asentado;
   - todos los órdenes de montaje cable a cable desde cero;
   - cada SOURCE apagada y encendida.

   En todos, el estado final es el de la reconstrucción y la cola acaba vacía:
   - mismas asignaciones, a 3 decimales;
   - mismo conjunto de nodos en overload, sin contar los nodos sin suministro, que el motor
     muestra apagados y no en overload (`:2510`).

   Son 1819 casos y corren en unos segundos.
5. **Negativos.** Cada uno tiene que hacer fallar algún test de 3 o de 4:
   - el modelo con la condición vieja `CountPoweredIncoming > 1` en el bucle de hermanos;
   - el modelo sin el cruce de epsilon.

   Usa flags del modelo, como `include_self`.
6. **Estructurales:**
   - el cuerpo de `NotifyOfferChanged` ya no llama a `CountPoweredIncoming` antes del bucle de
     hermanos;
   - `PublishEdgeOffers` llama a `SupplierVisibilityFlipped`.

## SPEC e INFORME

- §3.2, "B1.3 dirty": las dos reglas.
- I4: si ningún PASSTHROUGH tiene 2 o más incoming enabled, no se marca ni se reescribe nada.
- §6.3: añade C y F con el panel conectado el último, y el panel apagado y encendido.
- Añade `## Ronda 5: cambios`.
- INFORME: qué cambió en esta ronda y la salida de los tests.

## Lista blanca

- `scripts/5_Mission/LFPG_ElecGraphImpl.c`: `NotifyOfferChanged`, `PublishEdgeOffers`,
  `SupplierPowerForMerger` y los dos helpers nuevos.
- `.github/tools/test_graph_multifeed_split.py`
- `reviews/2026-10-10-grafo-grok/i76/SPEC.md`
- `reviews/2026-10-10-grafo-grok/i76/INFORME.md`

No cambian `LFPG_Data.c` (no hay campo nuevo) ni `checks.yml`. Tampoco la decisión de overload,
`CountPoweredIncoming`, el segundo bucle de `NotifyOfferChanged`, las zonas de i77 e i78 ni
`.github/tools/graph_reference/`.

## Criterio de hecho

1. Las dos reglas en el código, con los dos helpers y sin campos nuevos.
2. Los tests de m5-1 en verde. Con cada negativo, falla al menos un test de 3 o de 4.
3. Todos los `test_*.py` y `enforce_checks.py --root .` pasan.
4. El linter sin errores nuevos (delta por `len(errors)`).
5. La SPEC con `## Ronda 5: cambios` y el INFORME al día.

Mismo RECEIPT del BRIEF, con los paths de la lista blanca.
