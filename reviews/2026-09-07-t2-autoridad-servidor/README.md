# T2 — autoridad de servidor · **ARCHIVADO 2026-09-07, no desplegar**

Tres rondas de implementacion y tres dictamenes adversariales cruzados sobre el mismo tramo.
**El tramo se archiva sin cerrar**, por la regla que se fijo antes de la ronda 3: el VERDE no se
alcanzo entero, y no hay ronda 4.

## Que hay aqui

| fichero | que es |
|---|---|
| `REVIEW-r1-codex.md`, `REVIEW-r2-codex.md`, `REVIEW-r3-codex.md` | los tres dictamenes, con escenarios de fallo paso a paso y modelos ejecutados |
| `informe-r1-laneA.md`, `informe-r1-laneB.md`, `informe-r2.md`, `informe-r3.md` | lo que cada lane dice haber hecho, con sus secciones `Que NO cubre` |
| `brief-*.md` | los encargos, incluido el VERDE de la ronda 3 |
| `DECISIONES-PRODUCTO.md` | la unica decision de producto firmada por el dueño |

Implementacion: `cursor-grok-4.6-xhigh` via Cursor CLI. Revision: `gpt-6-astra` via `codex exec`,
otra familia, la misma en las tres rondas.

## Estado: 6 de 7 condiciones del VERDE

| # | condicion | veredicto del revisor |
|---|---|---|
| V1 | nodo huerfano tras rechazo | **ALCANZADO** — modelo de control 2.047 → 2.047 |
| V2 | ningun dispositivo activo sin cable | **PARCIAL** ← lo unico que falta |
| V3 | invalidaciones no sobreviven a owner no publicable | **ALCANZADO** |
| V4 | el orden del store sobrevive al rollback | **ALCANZADO** |
| V5 | corte por puerto registra el extremo antiguo | **ALCANZADO** |
| V6 | cero GRAVE nuevo | **ALCANZADO** |
| V7 | F-02, SEC01, SEC02, SEC20(a) siguen en pie | **ALCANZADO** |

Sin GRAVE nuevo, sin crash y sin corrupcion persistente en ninguna de las tres pasadas.

## La deuda, que es lo que hay que leer si retomas esto

1. **MEDIO — `R-T2-AGUA`.** El rechazo de una conexion puede dejar **activo el aspersor de destino**
   aunque el cable se retire en el rollback: la notificacion de alta refresca dispositivos antes de
   conocer el resultado del grafo, y la retirada provisional no deshace ese refresh. El reset
   periodico lo apaga a los 60 s; hasta entonces puede regar. Al arreglarlo, conservar el fixture y
   exigir **tambien** el control del reemplazo exitoso.
2. **MEDIO — `R3-01`.** La poda de un store vanilla vacio cancela una invalidacion **todavia
   necesaria para un owner vivo**: si se corto el ultimo cable durante FullSync y la poda ocurre
   antes del flush, un observador junto al extremo antiguo conserva el cable borrado. La correccion
   futura debe seguir liberando owners muertos y **preservar** el interes pendiente de los vivos.
3. **VALIDACION PENDIENTE.** **Nada de este tramo se ha compilado ni arrancado nunca.** Tampoco
   S-01/CCTV (que devuelve el motor para la enumeracion e identidad del cuerpo durante una sesion
   de CCTV) ni los controles funcionales V1..V7 in-game.
4. **DEUDA DE DISEÑO.** La transaccion conserva los stores, pero **los helpers de notificacion
   modifican estado funcional fuera de una decision unica de commit**, y las invalidaciones viven
   separadas de su publicacion pendiente. Al retomar, meter ambos limites en el contrato. Las
   reglas de capacidad duplicadas en admision e insercion hoy coinciden, pero pueden divergir.

**NO arrastrar como abierto** (tienen evidencia de cierre en `REVIEW-r3-codex.md`): la duda de
`ref` en locales de `Find`, el orden del snapshot, el fixture huerfano N-01, F-02, y la captura de
los dos extremos de V5.

## La leccion de proceso, que costo dos rondas

**La ronda 1 se partio por ficheros** —una lane por fichero, cada una con prohibido tocar el de la
otra— para evitar conflictos de fusion. El revisor la rechazo y nombro la causa:

> Los riesgos determinantes aparecen **entre** ellos: admision del almacen frente a admision del
> grafo, y destinatarios nuevos frente a clientes que conservan el estado anterior. **La separacion
> por archivo ocultaba ambas relaciones.**

Y sobre el veto en concreto: *«el limite de una lane no cambia el comportamiento exigido al
sistema»*. El arreglo correcto vivia a caballo entre los dos ficheros y el reparto lo hacia
imposible de escribir.

**Las rondas 1 y 2 corrieron sin VERDE definido ni tope de rondas**, contra la regla de la casa que
pide fijar ambos antes de la primera. Resultado: dos ciclos donde el revisor ponia el liston y
siempre podia encontrar un escenario mas. La ronda 3 llevo el VERDE escrito por delante y produjo
por fin una decision acotada — que es exactamente para lo que sirve.

**Por que cada ronda cambiaba unos defectos por otros**, en palabras de la ultima lane:

> Cada ronda cerro el hallazgo nombrado moviendo un helper; **los consumidores del estado
> transitorio no estaban en el contrato escrito.**

Si esto se retoma y aparece un cuarto canal de esos, la respuesta ya no es otro parche: es un
**commit funcional** — una foto nueva de stores + grafo aplicada de una vez, sin notificar a nadie
a media mutacion.
