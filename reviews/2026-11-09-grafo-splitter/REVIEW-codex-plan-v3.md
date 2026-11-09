<!-- Salida literal de Codex (codex exec, solo lectura, a ciegas). -->

NO APTO

**R22-V3-01 — BLOQUEA: el fallback de entidad no resuelta conserva el alto falso de la v2.**

Referencias leídas: `reviews/2026-11-09-grafo-splitter/PLAN-CONSOLIDADO.md:97`, `:157`; `scripts/5_Mission/LFPG_ElecGraphImpl.c:2487`, `:2502`, `:2884`, `:3105`; `scripts/4_World/LFPG_MemoryCell.c:235`.

Escenario: G=50, AND previamente en `11`, latch abierto y `node.m_GateClosed=false`; M desactivada, Power independiente=1. B publica baja y se conecta AND→M.Set, con cola `[B, AND, M]`. Las entradas publicadas de AND son ahora `10`.

Si `FindById` falla en el pre-gate, la rama nueva no evalúa AND: aplica el fallback declarado, `signalIn=true` y compuerta cacheada abierta, y publica **alto**. Step 5 puede recuperar la entidad por NetworkID y cerrar correctamente el latch, pero su reproceso queda detrás de M. M captura Set y permanece activada.

Testigo en memoria, con callbacks reales y el delta aplicado:

- Entidad resuelta antes de publicar: señal=false, AND=false, M=false.
- Fallo de resolución en pre-gate y recuperación en Step 5: señal=true, AND=false, **M=true**, incluso después de recibir baja.

La falta de registro se inyectó; no medí su frecuencia en partida. El código contempla expresamente esa recuperación tardía.

**Arreglo propuesto [DESIGN]:** completar la resolución antes de publicar, usando los fallbacks que ya tiene `SyncNodeToEntity`. Para el caso todavía irresoluble, eliminar la publicación basada en el latch antiguo y definir recuperación y política de señal/energía compatibles con el invariante. Añadir ambos casos de resolución fallida al arnés.

**R22-V3-02 — IMPORTANTE: recalcular las entradas actuales no permite exigir M desactivada en todos los órdenes.**

Referencias leídas: `reviews/2026-11-09-grafo-splitter/PLAN-CONSOLIDADO.md:326`, `:347`; `scripts/5_Mission/LFPG_ElecGraphImpl.c:888`, `:2525`, `:2821`, `:2884`; `scripts/4_World/LFPG_MemoryCell.c:235`.

El mismo circuito, con la entidad resuelta, falla con la permutación inicial **`[AND, M, B]`**, admitida por §6.3:

1. B ya está apagado en la entidad, pero su arista todavía publica alto.
2. AND se procesa primero, lee `11` y publica correctamente `AND(1,1)=true` en la arista nueva.
3. M lee Set alto y se activa.
4. B publica baja; AND pasa a publicar false y cierra.
5. M recibe Set bajo, pero conserva su activación.

El testigo con callbacks reales termina **AND=false, M=true**. Aquí no hay discrepancia entre la publicación y las entradas que AND leyó: esas entradas pertenecen a momentos distintos de la propagación.

Por tanto, la fila nueva de latch viejo no puede dar verde con su expectativa actual en todas las permutaciones. Restringirla a `[B, AND, M]` ocultaría esta diferencia de contrato.

**Arreglo propuesto [DESIGN]:** incorporar `[AND, M, B]` y decidir expresamente la garantía. Si M debe permanecer desactivada en todos los órdenes, hace falta controlar cuándo consume las señales durante una ola de propagación; el recálculo local no basta. Si se permiten estos transitorios, deben acotarse las expectativas y declararse esa limitación.

**R22-V3-03 — IMPORTANTE: el fixture del retraso no produce la deshabilitación que pretende probar.**

Referencias leídas: `reviews/2026-11-09-grafo-splitter/PLAN-CONSOLIDADO.md:357`; `scripts/4_World/LFPG_MemoryCell.c:235`, `:239`, `:260`, `:421`.

Con **Set sostenido**, un pulso de Reset no desactiva una celda activa: cuando ambos están altos, ninguna de las dos ramas modifica el latch.

Resultado ejecutando el callback real:

| Tiempo | Set | Reset | Q |
|---|---:|---:|---:|
| 0 ms | 1 | 0 | 1 |
| 5000 ms | 1 | 1 | **1** |
| 5100 ms | 1 | 0 | 1 |

Output nunca se deshabilita. El fixture podría pasar con cero flips sin haber ejercitado la limpieza del bit, la bajada prolongada ni el rearme.

**Arreglo propuesto:** durante los 100 ms de Reset, poner Set=0; después restaurar Set=1 y Reset=0. Verifiqué que produce Q=`1→0→1`. Afirmar además la deshabilitación efectiva y las lecturas de B antes de evaluar flips. En el caso retrasado controlado, si B observa baja durante 1100 ms con Power estable y el rearme habilitado, exigir exactamente un flip.

**Cierre de mis hallazgos**

| Hallazgo o respuesta anterior | Dictamen sobre el cierre |
|---|---|
| R22-V2-01: publicación con latch anterior | **Parcial.** Corrige el testigo original con entidad resuelta y B procesado primero. No cierra R22-V3-01 ni la expectativa general de R22-V3-02. |
| R22-V2-02: presupuestos no aislados | **Correcto en el plan.** Se separan ambos límites y se corrige el corte entre nodos. El testigo de 65 CONSUMER distingue 64 procesados/1 pendiente frente a 65/0. |
| Pregunta 2: bajada corta prolongada | **Declaración correcta; fixture incorrecto.** R22-V3-03 impide darlo por verificado. |
| Pregunta 3: llamadas directas y temporizadores | **Correcto en el diseño.** El reloj por eventos corrige el avance artificial de 100 ms por llamada y contempla ambos órdenes en los empates. |
| Pregunta 4: mutante dependiente del orden | **Anclaje corregido**, bajo las precondiciones indicadas abajo. No certifiqué su muerte integrada. |
| Pregunta 5: Toggle durante sobrecarga | **Correcto.** Power independiente puede autorizar Toggle aunque otra fuente esté sobrecargada. |
| R22-01 heredado: bit antiguo al rehabilitar | El cierre del escenario original sigue siendo correcto; la v3 no modifica esa limpieza. Su inocuidad temporal no queda demostrada por el fixture nuevo. |
| R22-02 heredado: pulso al salir de 00 retenido | Correcto para aquel escenario. No equivale a ausencia general de altos falsos. |
| R22-03 heredado: presupuestos y reloj | Las carencias de diseño señaladas quedan corregidas. Falta validación integrada del arnés. |

**Respuestas a §10**

1. **Publicación y captura falsa.** Sí: el fallback publica un valor distinto de AND(10), y las permutaciones pueden activar M aun publicando correctamente las entradas locales. Son mecanismos diferentes, descritos arriba.

2. **Invariante estable.** No he demostrado un estado estable que lo viole con entidades resueltas y callbacks efectivos. La discrepancia entre latch y evaluación puede ser transitoria hasta el reproceso. Esto no prueba el PDQ completo: falta comprobar que toda invalidación llega al callback y a la pasada correctora. Además, el invariante permite señal alta con asignación cero y, por ello, **no detecta los altos falsos anteriores**.

3. **Fidelidad del arnés.** Las correcciones de reloj y presupuestos son pertinentes, pero no bastan para certificar las filas nuevas: dos tienen los problemas anteriores. El barrido debe reproducirse condicionalmente: `ValidateConsumerStates` sincroniza al corregir una discrepancia de alimentación (`LFPG_ElecGraphImpl.c:3559`, `:3577`), y `NodeEntitySyncUnchanged` todavía puede omitir el callback (`:3133`). No es una reevaluación incondicional cada diez llamadas.

4. **Limitación temporal y brief.** No encontré una prohibición expresa de esa latencia en el brief leído. Tampoco doy por comprobada la conservación completa de C1–C7: el brief los enumera, pero no contiene su definición completa. Con el callback real, una baja observada de **100 ms produce cero flips adicionales**; prolongada a **1100 ms produce uno**, en 6500 ms, cambiando Q de true a false. La prueba pendiente es repetir C1–C7 con sus entradas originales y congestión controlada, además del fixture corregido.

5. **Otro camino al XOR del mutante.** No lo encontré para el montaje exacto, previamente asentado, con `input_1` sin cable y sin salida. `NotifyOfferChanged` no encuentra otro proveedor y tampoco una salida del XOR que active su segunda vía (`LFPG_ElecGraphImpl.c:4258`, `:4268`). El barrido tampoco lo despierta si `m_Powered=false` ya coincide con la ausencia de flujo. Esas precondiciones deben afirmarse; todavía falta ejecutar el mutante en el arnés completo.

**Defecto que §6 podría dejar pasar:** el alto falso cuando falla la resolución del pre-gate. No hay fixture de entidad irresoluble; S12 puede pasar porque la rama correcta existe mientras ese fallback sigue siendo incorrecto.

**Premisa cuestionada:** «entradas actuales» significa bits actualmente publicados, no una instantánea coherente de los cambios del mismo tick. Recalcular AND/OR/XOR elimina el desfase del latch local, pero no garantiza ausencia de pulsos capturables durante la propagación.

**Comprobado y límites**

Confirmé HEAD `a266854645487efbb73d398af24380a17b2197d3` y el diff vacío de `scripts` frente a `592ebcc`. Contrasté el delta v2→v3, la revisión anterior propia, el brief, las instrucciones y las anclas citadas.

Ejecuté en memoria los testigos anteriores y **24/24 combinaciones** de callback de puerta, entradas y latch anterior: después del callback efectivo, todas coinciden con su tabla.

No ejecuté Enforce, el PDQ completo propuesto, partida ni mediciones de rendimiento. No inspeccioné el test nuevo a medias, no consulté revisiones ajenas y no escribí en disco.
