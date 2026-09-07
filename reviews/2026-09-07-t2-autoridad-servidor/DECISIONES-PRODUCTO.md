# Decisiones de producto — tramo T2

## SEC02 — que hace el servidor cuando un reemplazo cortaria el cable de otro jugador

**Decidido por el dueño el 2026-09-07: la variante RESTRICTIVA.**

Al conectar un cable nuevo a un puerto de entrada que ya tiene un cable de **otro** jugador, y
con la politica `AllowCutOthersWires` **desactivada**, el servidor **aborta la operacion entera**:
no borra el cable de origen, no toca el puerto de destino, no guarda el cable nuevo. El jugador
recibe `"Cannot replace another player's wire."`

La alternativa descartada era la permisiva: saltarse ese borrado concreto y conectar igual. Se
descarta porque dejaria dos cables en el mismo puerto de entrada, rompiendo el invariante de
1-cable-por-IN.

**Es un cambio visible para los jugadores.** Antes, con `AllowCutOthersWires=false`, un jugador
**no podia cortar** el cable de otro con alicates pero **si podia borrarselo** conectando el suyo
al mismo puerto. La politica solo cubria una de las dos puertas. A partir de ahora cubre las dos.

Motivo: regla G6 de la casa — los handlers de RPC fallan cerrados por defecto. El camino de corte
(`HandleCutWires`) ya leia la politica; que el de acabado de cable fuera una puerta trasera era el
defecto, no una funcionalidad.

**Borde de la decision, para que conste:** un servidor cuyo admin tenga `AllowCutOthersWires=true`
no nota ningun cambio — ahi el reemplazo sigue permitido. El cambio solo afecta a los servidores
que ya habian elegido proteger los cables entre jugadores, que son justamente los que creian
tenerlo protegido y no lo estaba.

## Pendientes de firma, NO decididas todavia

Estas dos vienen del tramo T1 y siguen abiertas:

- **E04** — el arreglo cambia la direccion del fallo, de perder dinero a poder duplicarlo. La
  ventana esta acotada por un marcador durable y exige **dos** fallos consecutivos (que falle el
  borrado del marcador Y que falle su reescritura).
- **E16** — un ATM que se destruye y no vuelve nunca deja tombstones permanentes en el JSON de
  balances. Es fail-closed a proposito: no duplica, pero el fichero crece.
