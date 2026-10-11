# RONDA 2 — lane i76 (SPEC del reparto multifuente)

Revisión de `7b6406a`. **Esta ronda sigue siendo solo la SPEC**, con las mismas herramientas que
la ronda 1 y sin shell. La implementación vendrá después, en esta misma sesión, cuando la SPEC
tenga el visto bueno.

La base es buena: mecanismo actual con `path:line`, contraejemplos con números, cap-and-fill e
invariantes. Hay dos bloqueantes, tres mayores y dos menores.

## S1 (BLOQUEANTE) — El residual del OTRO proveedor no está definido
Cada proveedor del Combiner ejecuta su propio `AllocateOutput`, en el orden de
`ProcessDirtyQueue`, y `SplitTwoProviderHard` necesita `residualA` y `residualB`. Define:

1. Qué campos lee cada llamada para conocer el residual del otro proveedor. Si hacen falta campos
   nuevos de runtime, di en qué clase y demuestra con `path:line` que no se persisten ni viajan
   por RPC o SyncVar.
2. De qué epoch es cada valor. El otro proveedor puede no haber corrido todavía en este epoch.
3. La prueba de que las dos llamadas simétricas suman `min(D, capA + capB)` cuando leen el mismo
   snapshot.
4. Qué pasa cuando leen valores distintos (uno ya actualizado y el otro no): cota del exceso o del
   defecto, y en cuántos epochs se cierra.

## S2 (BLOQUEANTE) — Trinquete con un proveedor PASSTHROUGH
Con §3.2, el residual de un proveedor PASSTHROUGH sale de su `allocAvail`. Eso es lo que recibe
hoy, y lo que recibe depende de lo que el Combiner le pidió. En 2.c se queda clavado:

- epoch 1: el Combiner pide 25/25 y S20 da 20;
- el splitter recibe 25 + 20 = 45 y su residual hacia el Combiner es 25;
- el Combiner recibe 45 < 50;
- en el epoch siguiente el residual sigue en 25, y nunca llega a 20 + 30.

Además, el propio 2.c usa 50 − 10 = 40, que no cuadra con §3.2.

Define un margen del proveedor PASSTHROUGH que pueda crecer por encima de lo que recibe hoy, sin
sobrecargar nunca a la SOURCE de arriba. Puede salir, por ejemplo, de los residuales aguas arriba
memorizados por epoch; eliges tú, pero justificado. Añade la traza epoch a epoch de 2.c desde cold
start hasta el estado estable 20/30/20, con l1 y l2 encendidos y la cota de epochs.

## S3 (MAYOR) — Dependencia del orden de `m_Outgoing`
`residual(E)` resta `hard(F)` "ya anotada si F se visitó antes en este `AllocateOutput`; si no, la
del epoch previo". El resultado cambia con el orden de los edges.

Elimina la dependencia o demuestra que no existe. Una opción es calcular primero las demandas de
los edges que no van a un merger y después las peticiones a mergers. El plan de pruebas lo
comprueba con dos órdenes.

## S4 (MAYOR, alcance) — 2.b cambia la política de overload
En 2.b propones quitar el all-off de un merger con fan-in > 1, y lo llevas a §5. Eso cambia la
política de overload y no es G-01. Deja la política vigente:

- 2.b EXIGIDO: S20 entrega 20 y S50 entrega 50; ninguna fuente en overload; el Combiner en
  overload (all-off); el consumidor apagado.
- El invariante de conservación en el merger (I2) solo se exige cuando el merger no está en
  overload. Dilo.
- El reparto de carga (shedding) va a una sección nueva,
  `## Propuesta para el dueño (fuera de G-01)`.
- Quita la excepción de §5.

## S5 (MAYOR) — Fan-in mayor que 2
`LFPG_MAX_EDGES_PER_NODE = 12` (`scripts/3_Game/LFPG_Defines.c:522`). §3.1 despacha N > 2 con
"mismo cap-and-fill en el bucle existente", que no está especificado ni da el mismo resultado en
llamadas independientes. Elige una de dos:

- Demuestra con `path:line` que ningún PASSTHROUGH alcanzable puede tener más de 2 proveedores
  alimentados. Cubre los puertos IN de cada dispositivo, la regla de reemplazo de un puerto IN
  ocupado (`LFPG_ConnectionRules.c:61-64` y `HandleLFPG_FinishWiring`) y los dispositivos
  vanilla.
- O especifica el reparto N-way (water-filling con un orden determinista, igual en todas las
  llamadas) con su cota de bucle.

## S6 (MENOR) — §6.1, plan de pruebas offline
- El test de 2.b, según S4.
- La traza de S2, como test.
- S3, con dos órdenes de `m_Outgoing`.

Pedir exactamente 20 + 30 en 2.a está bien. Pero el criterio de cierre del issue es factibilidad y
conservación contra el referente: los tests se comparan con el oráculo, no solo con una cifra.

## S7 (MENOR) — §6.2 contra la API real del oráculo de i72
La API está en `C:\tmp\lfpg-grok\wt-i72\.github\tools\graph_reference\README.md`. Léela como
interfaz, no como algoritmo. Está en revisión: añade alcanzabilidad tras gates cerrados,
consumidores binarios con `max_hard_servable` entero, conservación de dos lados y las reglas
`over_allocation` y `partial_allocation`. Escribe §6.2 contra esa semántica. Yo integro el oráculo
en tu rama antes de la ronda de implementación.

## Entregable
`SPEC.md` corregido en su sitio, con una sección final `## Ronda 2: cambios` que liste cada punto
S1–S7 y dónde queda resuelto. Misma lista blanca (solo `SPEC.md`) y mismo RECEIPT.
