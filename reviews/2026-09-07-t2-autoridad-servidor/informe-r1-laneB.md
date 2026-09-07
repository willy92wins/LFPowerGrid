# REPORT — T2 SEC01 destinatarios RPC

Lineas de la tabla = sitio de `.Send(` **despues del parche**. Las lineas del brief (antes) van entre parentesis.

## Tabla de los 9 sitios

| linea | funcion | RPC | destinatario deducido | accion | por que |
|---|---|---|---|---|---|
| 2152 (era 2151) | `BroadcastOwnerWires` | `SYNC_OWNER_WIRES_V2` | `pb.GetIdentity()` del jugador que acaba de pasar el filtro de proximidad (owner o cualquier target, radio `LFPG_CULL_DISTANCE_M + 20`) | DIRIGIDO | El `continue` de `:2142` ya elige a quien le toca. El cliente aplica el blob en `LFPG_RPCClientHandler.HandleSyncOwnerWiresV2` → `UpsertOwnerBlobV2` sin otro filtro de destinatario: difundir filtraba mal. Si `GetIdentity()` es null, `continue` (no se pasa null). |
| 2344 (era 2340) | `BroadcastOwnerSnapshot` | `SYNC_OWNER_WIRES_V2` | Igual: identidad del `player` del bucle. Si `snapshot.m_BroadcastAll`, `inRange` es true para todos los de `GetPlayers()` y cada uno recibe un unicast | DIRIGIDO | Mismo RPC y mismo culling. `m_BroadcastAll` sigue cubriendo a todos los jugadores **conectados con identidad**; no hace falta `recipient = null`. |
| 2491 (era 2484) | `BroadcastOwnerWireDelta` | `SYNC_OWNER_WIRES_DELTA` | Identidad del `pb` en rango (owner + targets actuales + targets del delta) | DIRIGIDO | Mismo bucle de interes. El handler cliente (`HandleSyncOwnerWiresDelta` → `ApplyOwnerDelta`) aplica lo que llega. |
| 2626 (era 2618) | `BroadcastVanillaWires` | `SYNC_OWNER_WIRES_V2` | Identidad del `pb` en rango respecto al owner vanilla o sus targets | DIRIGIDO | Copia del patron de `BroadcastOwnerWires` sobre almacenes vanilla. |
| 2673 (era 2663) | `SendVanillaWiresTo` | `SYNC_OWNER_WIRES_V2` | Identidad del `player` argumento. Comentario del propio metodo: «vanilla source -> single player (unicast)» | DIRIGIDO | Destinatario inequívoco. Si no hay identity, `return` (no difundir). |
| 2798 (era 2783) | `LFPG_SendFullSyncOwner` | `SYNC_OWNER_WIRES_V2` | Identidad de `m_FullSyncPlayer` (el que esta haciendo join / full sync) | DIRIGIDO | El emisor ya sabe el jugador. `LFPG_StartNextFullSync` `:2717` y `LFPG_ProcessFullSyncSpread` `:2812` ya exigen identity; si se ha perdido, `return` en vez de `null`. |
| 3002 (era 2984) | `SendEmptyDeviceCableStateTo` | `SYNC_OWNER_WIRES_V2` | Identidad del `player` argumento | DIRIGIDO | Helper `*To` de un jugador. Payload = estado vacio de UN device, no un anuncio global. |
| 3047 (era 3027) | `SendOwnerBlobTo` | `SYNC_OWNER_WIRES_V2` | Identidad del `player` argumento. Comentario: «unicast a single owner's wire blob to one player» | DIRIGIDO | Destinatario inequívoco. Llamado desde `SendDeviceSyncTo` para un player concreto. |
| 6381 (era 6361) | `BroadcastCargoRefreshToNearby` | `SORTER_CARGO_REFRESH` | `pid` de `pb.GetIdentity()`, con `if (!pid) continue` ya existente | CONTROL POSITIVO — no tocado | Patron copiado. Sigue siendo el unico sitio que ya era correcto. |

Recuento post-parche de `.Send(` en este fichero: **9**. `noExclude` ya no aparece.

### SEC01 — CERRADA

- **Que cambie:** solo `scripts/5_Mission/LFPG_NetworkManagerImpl.c`. Los ocho `.Send(` que pasaban `null` / `noExclude` ahora pasan `PlayerIdentity pid` obtenido con `GetIdentity()`. Si la identity es null, el bucle hace `continue` y los helpers `*To` / `LFPG_SendFullSyncOwner` hacen `return`. No se anadio ningun `.Send(` nuevo.
  - `BroadcastOwnerWires` `:2120-2121` + Send `:2152`
  - `BroadcastOwnerSnapshot` `:2315-2317` + Send `:2344`
  - `BroadcastOwnerWireDelta` `:2458-2460` + Send `:2491`
  - `BroadcastVanillaWires` `:2594-2595` + Send `:2626`
  - `SendVanillaWiresTo` `:2636-2638` + Send `:2673`
  - `LFPG_SendFullSyncOwner` `:2784-2788` + Send `:2798`
  - `SendEmptyDeviceCableStateTo` `:2985-2987` + Send `:3002`
  - `SendOwnerBlobTo` `:3023-3025` + Send `:3047`
- **Por que asi:** el cuarto argumento de `ScriptRPC.Send` es destinatario (NULL = todos los clientes), no una exclusion. El nombre `noExclude` era esa lectura invertida. El control positivo del mismo fichero (`:6348` + `:6381`) ya hacia `pid = pb.GetIdentity(); if (!pid) continue; ... Send(..., pid)`. Lo copie.

  Alternativa descartada 1: dejar `m_BroadcastAll` como `Send(..., null)` «porque quiere ir a todos». El bucle ya itera `GetPlayers()` y con `inRange = snapshot.m_BroadcastAll` cada jugador conectado con identity recibe un unicast. Un `null` en el overflow volvia a filtrar el blob de esa owner a quien no esta en el servidor como jugador iterable y, sobre todo, a quien el overflow no pretendia incluir de mas: todos los clientes, no solo los de `GetPlayers()` de ese tick. No hace falta.

  Alternativa descartada 2: tocar solo `:2798` (el «peor» de la auditoria) y dejar los bucles. El defecto de los bucles es el mismo y ademas **multiplica** el broadcast (un all-clients por cada jugador en rango).

  Alternativa descartada 3: pasar `PlayerBase` como cuarto argumento. La firma vanilla es `PlayerIdentity recipient`. El control positivo usa `pid`.
- **Que NO cubre:**
  - Cualquier `ScriptRPC.Send(..., null)` **fuera** de este fichero. Hay al menos uno en `LFPG_RPCServerHandlerImpl.c:1969` (`SYNC_SERVER_SETTINGS` con `null`); esa lane tiene el fichero y no lo he tocado.
  - Jugadores fuera de `LFPG_CULL_DISTANCE_M + 20.0` (50 m + 20 m). Antes recibian el blob por rebote. Eso era el leak, no un canal de diseno: `LFPG_CableRenderer.c:2385` ya no dibuja a mas de `LFPG_CULL_DISTANCE_M`.
  - Reconexion en la ventana en la que `GetIdentity()` es null: ese tick no manda (igual que el control del sorter). El full sync posterior sigue encolado por `SendFullSyncTo`.
  - Medicion de trafico. No hay cifras de ahorro.
  - Compilacion in-game. Enforce no se compila aqui.
  - RPC viejo `SYNC_OWNER_WIRES` (= 5): este fichero ya no lo emite; el cliente aun lo despacha.
- **Como comprobarlo:** servidor con ≥2 jugadores reales (no offline).
  1. **Unicast de verdad (control positivo del defecto).** Jugador A junto a una base LFPG con cables. Jugador B a >70 m de owner y de todos los targets. Mutar un cable en la base de A (add/remove). En el RPT/script.log de B no debe aparecer `SyncOwnerWiresV2` / `LFPG_PERFDIAG snapshot_receive` de ese `deviceId`. A debe seguir viendo el cable. Repetir con un source vanilla (`BroadcastVanillaWires` / `SendVanillaWiresTo`) y con un delta (`SYNC_OWNER_WIRES_DELTA`).
  2. **No perder el sync de quien SI debe.** Jugador C a <70 m del owner o de un target: debe recibir el snapshot/delta y ver el cable. Caminar a >70 m + 25 m (`earlyOutDist` del renderer) y confirmar que el cable desaparece por culling visual, no por un RPC «vacio» accidental.
  3. **Full sync no es global.** Jugador D entra al servidor cerca de una base. Solo D debe recibir los `SYNC_OWNER_WIRES_V2` de ese join. A/B/C no deben re-recibir el dump del full sync de D.
  4. **Identity null no vuelve a difundir.** Desconectar a A a mitad de un full sync: el servidor no debe empezar a mandar esos blobs a todos. El camino `:2812` + el `return` nuevo de `:2784-2788` cubren ese caso.
  5. **Overflow `m_BroadcastAll`.** Si se puede forzar un owner con mas posiciones de interes que `LFPG_OWNER_SNAPSHOT_MAX_INTEREST_POSITIONS`, todos los jugadores **online** deben recibir el snapshot (unicast cada uno), no solo los cercanos.

## SALIDA DEL LINTER

### Antes (linea base)

No hay salida de `python enfcheck.py`. Cada invocacion de Shell (este agente y un subagente que reintento `python` / `python3` / `py` / `py -3`) murio en el hook pre-comando, antes de lanzar el proceso:

```
Hook blocked with message: --: eval: line 1: syntax error near unexpected token `&'
--: eval: line 1: `$OutputEncoding = [System.Text.Encoding]::UTF8; Get-Content -LiteralPath '...' -Raw | & { $input | powershell -NoProfile -ExecutionPolicy Bypass -File "...\launch-ledger.ps1" -Mode Pre }'
```

El hook es PowerShell evaluado por bash. El interprete Python no llego a arrancar.

Equivalencia leida sobre el fichero **antes de editar** (Grep, mismos predicados que `enfcheck.py`):

- `.Send(` = 9
- `foreach` = 0
- ternario `[^?]\?[^?:]{1,80}:` = 0
- `++` / `--` = solo reglas de comentario `---` / `----` (preexistente)
- `+=` / `-=` = 10, todas `m_Sched*Ms +=` en el scheduler `:624-689` (preexistente, no SEC01)

### Despues

Misma imposibilidad de ejecutar `python enfcheck.py`. Equivalencia Grep sobre el fichero **ya parcheado**:

- `.Send(` = 9 (sitios `:2152 :2344 :2491 :2626 :2673 :2798 :3002 :3047 :6381`)
- `noExclude` = 0
- `foreach` = 0
- ternario (regex de `enfcheck.py`) = 0
- `++` / `--` = mismas reglas de comentario; el parche no anade `++` ni `--`
- `+=` / `-=` = mismos 10 del scheduler; el parche no anade ninguno
- lineas que contienen `{` = 795, que contienen `}` = 795 (Grep cuenta lineas, no ocurrencias; `enfcheck.py` cuenta caracteres; el parche no anadio bloques `{ }`)
- lineas que contienen `(` = 2632, que contienen `)` = 2632
- escapes adyacentes tipo `\n\t`: el unico `\\` del fichero es el literal preexistente `$profile:LF_PowerGrid\\vanilla_wires.json` en `:343`. El parche no anade literales.

El orquestador puede repetir `python enfcheck.py scripts/5_Mission/LFPG_NetworkManagerImpl.c` fuera de este sandbox.

## LO QUE NO PUDE VERIFICAR

- Compilacion Enforce al cargar el modulo World. Nadie la vera hasta un boot de servidor.
- Firma vanilla `ScriptRPC.Send` en `P:\scripts\3_game\gameplay.c:117`. No he salido del workspace; me he fiado de la cita del brief y del uso ya presente en `:6381`.
- Comportamiento in-game (cables fantasma / leak de base ajena). No hay DayZ, ni PBO, ni trafico real.
- `python enfcheck.py` en este entorno.
- Si algun cliente depende de recibir blobs de owners lejanos por un canal que no sea el renderer (mapa global, ESP de cables, debug UI). En este arbol el unico consumidor de `SYNC_OWNER_WIRES_V2` / `_DELTA` es `LFPG_RPCClientHandler` → `LFPG_CableRenderer`.

## LA PREMISA DE ESTE ENCARGO

La lectura «`noExclude` es un destinatario mal entendido» es **correcta** en los 8 sitios de este fichero.

Pruebas internas, no la autoridad del brief:

1. El mismo fichero ya pasa un `PlayerIdentity` real en `:6381` y se llama `pid`, no `noExclude`.
2. Tres metodos se documentan a si mismos como unicast (`SendVanillaWiresTo`, `SendOwnerBlobTo`, `SendEmptyDeviceCableStateTo`) y aun asi pasaban `null`.
3. Los cuatro `Broadcast*` no significan «RPC broadcast»: significan «empujar el estado de este owner a los clientes interesados». El interes se calcula con un `continue` de distancia y despues se tiraba a todos. Eso no es un diseno dual: es el parametro mal nombrado anulando el filtro.
4. El cliente no tiene ACL. Quien reciba el RPC escribe el blob. Difundir = copiar la topologia de la base a todos.

Hay un sitio donde «llegar a todos los jugadores actuales» **si** es el diseno: `BroadcastOwnerSnapshot` cuando `m_BroadcastAll` es true (la lista de posiciones de interes se desborda). El brief lo presenta como parte del mismo defecto de `noExclude`. Aqui difundir con `null` y unicastear a cada entrada de `GetPlayers()` no son equivalentes del todo (el `null` llega a todo cliente, el bucle solo a jugadores listados), pero el overflow quiere cobertura total de jugadores, no un leak a clientes sin player. Dirigir el bucle **preserva** esa cobertura. No es un falso positivo del orquestador.

Lo que el planteamiento **no** dice y importa: el primer argumento de `Send` sigue siendo el `PlayerBase`/`Object` target. No lo he cambiado. Si algun dia se descubre que ese target tambien influye en el routing nativo, el parche no lo toca; el control positivo usa el mismo par `(pb, pid)`.

## VISTO-DE-PASO

- `LFPG_RPCServerHandlerImpl.c:1969` — `SYNC_SERVER_SETTINGS` se manda con `recipient = null` sobre un `target` que parece un jugador. Otra lane posee ese fichero. Puede ser settings globales (legitimo) o el mismo bug. No auditado.
- Scheduler `:624-689` usa `+=`, prohibido por convencion 7.3. Preexistente. `enfcheck.py` lo contara en rojo aunque SEC01 no lo haya tocado.
- `ref` en locales (`ref array<ref LFPG_WireData>`) aparece en este fichero (p.ej. `SendVanillaWiresTo` `:2640`). Convencion 7.5. Preexistente.
- `Print(perfDelta)` bajo `LFPG_PERFDIAG_ENABLED` en los mismos helpers. Convencion 7.6. Preexistente.
- `LFPG_RPCClientHandler` aun acepta `SYNC_OWNER_WIRES` v1; este emisor ya no lo genera.
- `LFPG_ControlSessionRegistry.c` ya dirige con `record.m_Identity`. Otro control positivo fuera de alcance.
