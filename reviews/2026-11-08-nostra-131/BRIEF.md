# BRIEF — arreglos del reporte de Nostra para la 1.3.1

Encargo de implementacion para una lane (Grok). Base: `origin/main` 9a4bafd. Rama:
`claude/project-thread-0iolln`. Lee `AGENTS.md` entero antes de tocar nada; si algo de aqui lo
contradice, manda este brief y dilo en tu informe.

## Contexto

Un jugador (Nostra) reporto en 1.3.0 / DayZ 1.30 que no consigue que funcionen la Memory Cell, el
Laser Detector con vehiculos, ni una puerta con dos interruptores, y que "desaparecen cables".
Triaje en codigo hecho (hilo del proyecto). Guillermo decidio el 2026-11-08: (1) avisar al jugador
cuando un cable nuevo sustituye a otro, con un ajuste de servidor para apagarlo; (2) que la
entrada Toggle de la Memory Cell sea un toggle de verdad; (3) que el Laser detecte vehiculos;
(4) corregir el stringtable. Durante el analisis aparecio un bug mas en la Memory Cell (S2a), que
hay que arreglar para que (2) sirva de algo.

Fuera de alcance (no lo toques): logica de AND/OR/XOR, Door Controller, el grafo
(`LFPG_ElecGraphImpl.c`), persistencia de cables, version (PR #96), classnames de `config.cpp`.

## S1 — Aviso al sustituir un cable, con ajuste

Hoy cada puerto (IN y OUT) admite un cable y `HandleFinishWiring` borra en silencio el que hubiera
(`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`: `FinishWiringCollectOwner` ~702 marca las filas,
el bucle tras el `stored` ~617-642 las borra). El reroute de la misma pareja de puertos sale antes
por `if (rerouteWire)` ~565 y **no** es una sustitucion.

1. `scripts/3_Game/LFPG_Settings.c`: campo nuevo en `LFPG_ServerSettings`, con comentario como los
   demas: `bool NotifyWireReplaced = true;` (por defecto activado). Anadelo a la linea
   `Settings loaded:` (junto a `DCLockDoors=`, ~623) como ` NotifyWireReplaced=<valor>`.
   Sin cambios en `ver`, `ValidateAndClamp` ni `Save()`.
2. En `HandleFinishWiring`, despues del bucle que borra las filas y antes de publicar
   (antes de `finish.m_Source.m_DeltaOps.Insert(LFPG_WireDeltaOp.ADD)`, que mete la fila nueva en
   `m_RemovedWires` del source): cuenta las filas borradas de todos los owners
   (`ownerState.m_RemovedWires.Count()`). Si el ajuste esta activo y la cuenta es > 0, manda al
   jugador que cablea, con `PlayerBase.LFPG_SendClientMsg(player, msg)`, exactamente:
   - 1 fila: `Replaced 1 existing cable: each port holds only one cable.`
   - N > 1: `Replaced N existing cables: each port holds only one cable.` (N en cifras).
   Si `LFPG_Settings.Get()` es null, no avises. Una sola llamada por FinishWiring.

## S2 — Memory Cell (`scripts/4_World/LFPG_MemoryCell.c`)

### S2a — Bug: la celda no ve sus entradas si nadie tira de la salida activa

Un PASSTHROUGH pide aguas arriba `demanda de abajo + m_Consumption`
(`LFPG_ElecGraphImpl.c` ~2551-2560, `hardBase`), y un edge hacia un PASSTHROUGH lleva como demanda
su `m_LastStableOutput` (~4602). La celda consume 0 (`LFPG_GetConsumption` linea 85) y no es
gate-capable, asi que con la salida activa sin consumidor (caso tipico: `Output` -> Door
Controller, `Inverted` sin cable, latch en OFF) pide 0, sus edges de entrada llevan 0 y
`IsPortReceivingPower` (que mira `m_AllocatedPower`, ~5044) da false en Power, Set, Reset y
Toggle. Set no puede encenderla nunca. Es lo que vio Nostra.

Arreglo: `static const float LFPG_MCELL_CONSUMPTION = 1.0;` junto a `LFPG_MCELL_CAPACITY` y
`LFPG_GetConsumption()` lo devuelve. Igual que el Counter, el Laser o la PressurePad, que ya tienen
autoconsumo por esta razon. No cambia el formato de persistencia.

### S2b — Toggle de verdad

Hoy `input_1` (Toggle) es un override por nivel: mientras recibe energia fuerza la salida a ON
(`if (hasToggle) { newActive = true; }`, ~157). Pasa a ser por flanco: **cada flanco de subida
invierte el latch**, con estas reglas, todas necesarias:

- **R1 rearme:** el flanco cuenta solo si Toggle llevaba al menos `LFPG_MCELL_TOGGLE_REARM_MS = 1000`
  ms sin energia. Motivo: cuando la celda cambia de estado cambia su demanda, y un distribuidor
  aguas arriba que pide mas de lo que tiene pone a 0 todas sus salidas durante una o varias
  evaluaciones (`LFPG_LogicGate.c` ~148-170 lo documenta y lo midio in-game, B5). Sin R1 ese
  apagon de ~100-300 ms se lee como "suelta y vuelve a pulsar" y la celda rebota.
- **R2 alimentacion estable:** cuenta solo si `input_0` (Power) lleva al menos
  `LFPG_MCELL_TOGGLE_REARM_MS` recibiendo energia de forma continua. Motivo: tras un corte, Power y
  Toggle vuelven a la vez; sin R2 un interruptor fijo en Toggle invertiria el estado en cada corte.
- **R3 gracia tras cargar:** si la celda viene de la persistencia, ningun flanco cuenta hasta
  `LFPG_MCELL_TOGGLE_LOAD_GRACE_MS = 30000` ms despues de `LFPG_OnStoreLoadDevice`. Motivo: en el
  arranque el grafo converge en segundos y las lineas se encienden en cualquier orden. Una celda
  recien colocada no tiene gracia.
- **R4 primera lectura:** un Toggle que nunca se ha visto apagado no da flanco.
- **R5 prioridad:** primero se aplica el flanco (latch = !latch) y despues Set/Reset por nivel como
  hoy (Set solo -> ON, Reset solo -> OFF, ambos o ninguno -> sin cambio). Con Set mantenido gana Set.
- Se elimina el override: la salida activa es siempre `m_LatchState`.

Implementacion pedida (para que sea testeable sin el motor):

- La decision va en una funcion escalar pura, firma **en una sola linea**, sin bucles, sin strings,
  sin `new`, para que `enforce_scalar_slice.scalar_function` la pueda cargar:
  `static bool LFPG_MCellToggleShouldFlip(int lowSinceMs, int powerSinceMs, int graceUntilMs, int nowMs, int rearmMs)`
  con -1 como "no visto" en `lowSinceMs` y `powerSinceMs`. Devuelve true solo si ambos son >= 0,
  `nowMs >= graceUntilMs`, `nowMs - lowSinceMs >= rearmMs` y `nowMs - powerSinceMs >= rearmMs`.
- Estado de servidor, no persistido ni sincronizado: `m_ToggleLastHigh` (bool), `m_ToggleLowSinceMs`,
  `m_PowerSinceMs`, `m_ToggleGraceUntilMs` (int, -1 / 0 al construir). En `LFPG_SetPowered`, con
  `nowMs = g_Game.GetTime()` y `hasPower = nm.IsPortReceivingPower(m_DeviceId, "input_0")` leido
  como las otras tres entradas: Power sin energia -> `m_PowerSinceMs = -1`; Power con energia y
  `m_PowerSinceMs < 0` -> `= nowMs`. Toggle con energia y `!m_ToggleLastHigh` -> evalua la funcion;
  despues `m_ToggleLastHigh = true`. Toggle sin energia y (`m_ToggleLastHigh` o `m_ToggleLowSinceMs < 0`)
  -> `m_ToggleLowSinceMs = nowMs`; despues `m_ToggleLastHigh = false`. Arranca con
  `m_ToggleLastHigh = true` y `m_ToggleLowSinceMs = -1` (R4).
- `LFPG_OnStoreLoadDevice`: ademas de lo que hace, `m_ToggleGraceUntilMs = g_Game.GetTime() + LFPG_MCELL_TOGGLE_LOAD_GRACE_MS` (con guarda de `g_Game`).
- Actualiza la cabecera del fichero y el bloque de comentario de `SetPowered` para que describan el
  comportamiento nuevo, y anade al log de diagnostico `flip=<bool>`. No toques `LFPG_ApplyRouting`,
  `LFPG_DeferredRouting` ni los LEDs.

## S3 — Laser Detector detecta vehiculos

Hoy `LFPG_EvaluateCrossing` (`scripts/4_World/LFPG_LaserDetector.c` ~309-420) solo mira dos puntos
del cuerpo de cada jugador a 0,5 m del haz. Un jugador dentro de un vehiculo esta colgado del
vehiculo (`DayZPlayerImplement.IsInVehicle`: `GetParent().IsInherited(Transport)`; coche y barco
heredan de `Transport`).

1. En el bucle de jugadores, tras el cast a `PlayerBase`: `Transport veh = Transport.Cast(pb.GetParent());`.
   Si hay `veh`, el jugador cuenta como detectado si la caja del vehiculo corta el segmento del
   haz actual (de `beamStart` a `beamStart + beamDir * m_BeamLength`), y se pasa al siguiente
   jugador sin hacer la prueba de los dos puntos. Si no hay `veh`, todo sigue como hoy.
2. Caja: `vector minMax[2];` y `veh.GetCollisionBox(minMax)`; si devuelve false,
   `veh.ClippingInfo(minMax)`. Pasa los dos extremos del haz a espacio del vehiculo con
   `veh.WorldToModel(...)` e inflala `LFPG_LASER_VEHICLE_BOX_MARGIN_M = 0.1` por cada lado.
   El haz acortado por el raycast termina en la chapa del vehiculo, y el margen lo cubre.
3. La interseccion va en una funcion escalar pura, firma en una sola linea, sin bucles (los tres ejes
   desenrollados, metodo de las losas):
   `static bool LFPG_LaserSegmentHitsBox(float ax, float ay, float az, float bx, float by, float bz, float minX, float minY, float minZ, float maxX, float maxY, float maxZ)`.
   Segmento de longitud ~0 en un eje: dentro si la coordenada cae en la losa. Usa `Math.AbsFloat`.
4. `scripts/5_Mission/LFPG_NetworkManagerImpl.c` ~6027: el radio de candidatos pasa de
   `LFPG_LASER_BEAM_RANGE_M + 1.0` a `LFPG_LASER_BEAM_RANGE_M + LFPG_LASER_VEHICLE_REACH_M`, para que
   entre el conductor de un camion cuyo haz corta la parte de atras. **Este fichero es CRLF en el
   indice**: no cambies sus finales de linea.
5. `scripts/3_Game/LFPG_Defines.c`, junto a las constantes `LFPG_LASER_*` (~383-388):
   `LFPG_LASER_VEHICLE_REACH_M = 8.0` y `LFPG_LASER_VEHICLE_BOX_MARGIN_M = 0.1`, con comentario.
6. Solo vehiculos con alguien vivo dentro (los muertos ya se saltan por `IsAlive`). Un vehiculo
   vacio no se detecta: es lo que se quiere para una puerta. Actualiza la cabecera del fichero.

## S4 — stringtable.csv

`stringtable.csv` es **LF** (`.gitattributes`). Conserva comillas y formato. Columnas: Language,
original, english, spanish, german, russian, chinesesimp, portuguese (original = english).

- `STR_LFPG_AND_Gate_Desc`, `STR_LFPG_OR_Gate_Desc`, `STR_LFPG_XOR_Gate_Desc`, `STR_LFPG_PressurePad_Desc`:
  en las siete columnas de texto, la capacidad `20` pasa a `100` (el codigo da 100:
  `LFPG_GATE_CAPACITY`, `LFPG_PAD_CAPACITY`). Nada mas cambia en esas filas.
- `STR_LFPG_LaserDetector_Desc`, textos completos:
  - original / english: `Passes power while a player or an occupied vehicle crosses the laser beam. 100 u/s capacity.`
  - spanish: `Transmite energía mientras un jugador o un vehículo ocupado cruza el rayo láser. 100 u/s.`
  - german: `Leitet Strom, solange ein Spieler oder ein besetztes Fahrzeug den Laserstrahl kreuzt. 100 E/s.`
  - russian: `Передаёт энергию, пока игрок или транспорт с людьми пересекает лазерный луч. 100 ед/с.`
  - chinesesimp: `玩家或有人乘坐的载具穿过激光束时通电。容量 100 u/s。`
  - portuguese: `Transmite energia enquanto um jogador ou um veículo ocupado atravessa o feixe de laser. Capacidade de 100 u/s.`
- `STR_LFPG_MemoryCell_Desc`, textos completos:
  - original / english: `Stateful latch: 4 inputs (power, toggle, reset, set) + 2 outputs (normal, inverted). Each Toggle pulse flips the state. 100 u/s capacity, 1 u/s self-consumption.`
  - spanish: `Latch con estado: 4 entradas (alimentación, toggle, reset, set) + 2 salidas (normal, invertida). Cada pulso en Toggle invierte el estado. 100 u/s, 1 u/s de autoconsumo.`
  - german: `Zustandsspeicher: 4 Eingänge (Strom, Toggle, Reset, Set) + 2 Ausgänge (normal, invertiert). Jeder Toggle-Impuls kehrt den Zustand um. 100 u/s, 1 u/s Eigenverbrauch.`
  - russian: `Защёлка: 4 входа (питание, переключение, сброс, установка) + 2 выхода (нормальный, инвертированный). Каждый импульс на входе переключения меняет состояние. 100 ед/с, 1 ед/с потребление.`
  - chinesesimp: `有状态锁存器：4 输入（电源、切换、复位、置位）+ 2 输出（正常、反相）。切换输入每收到一个脉冲就翻转状态。容量 100 u/s，自耗 1 u/s。`
  - portuguese: `Latch com estado: 4 entradas (energia, alternar, redefinir, definir) + 2 saídas (normal, invertida). Cada pulso em Alternar inverte o estado. Capacidade de 100 u/s, consumo próprio de 1 u/s.`

## Tests offline (R26) — se escriben antes o junto al codigo, y tienen que morder

En `.github/tools/`, con el estilo de `test_finish_wiring_quota.py` (slices de produccion con
`enforce_scalar_slice`, sin leer historia de git: lo viejo va como literal o como mutante del
fuente actual). Registralos en `.github/workflows/checks.yml` como pasos nuevos.

1. `test_memorycell_toggle.py` — carga `LFPG_MCellToggleShouldFlip` del fuente:
   - P1 baja 1500 ms, Power 10 s, sin gracia -> true. P2 justo en el limite (1000/1000) -> true.
   - N1 apagon de 200 ms -> false. N2 Power estable 300 ms -> false. N3 dentro de la gracia -> false.
     N4 `lowSinceMs = -1` -> false. N5 `powerSinceMs = -1` -> false.
   - Mutante: el mismo fuente sin la comprobacion de rearme hace que N1 de true (el test lo detecta).
   - Comprobacion de fuente: en `LFPG_SetPowered` ya no existe la asignacion `newActive = true;`
     condicionada a `hasToggle`, y `LFPG_GetConsumption` devuelve `LFPG_MCELL_CONSUMPTION` > 0.
2. `test_laser_vehicle_box.py` — carga `LFPG_LaserSegmentHitsBox` (stub de `Math.AbsFloat`). Caja
   -1..1 en los tres ejes:
   - P1 cruza por el centro de x=-3 a x=3 -> true. P2 empieza fuera y acaba dentro (haz cortado en la
     chapa) -> true. P3 todo dentro -> true. P4 longitud 0 dentro -> true.
   - N1 acaba antes de la caja -> false. N2 paralelo a un eje fuera de la losa -> false. N3 pasa por
     encima (y = 1.5) -> false. N4 diagonal que roza fuera de una esquina -> false. N5 longitud 0
     fuera -> false.
   - Mutante: sin el intercambio t1/t2 de un eje, al menos un caso falla.
3. `test_wire_replace_notice.py` — sobre el fuente: el campo `NotifyWireReplaced` existe con
   `= true`; en `HandleFinishWiring` el aviso esta despues del bucle de borrado, antes de la
   publicacion del ADD y dentro de una guarda que lee el ajuste; el camino de reroute sale antes del
   aviso; los dos textos exactos de S1 estan. Mutante: quitar la guarda del ajuste hace fallar el test.
4. `test_stringtable_capacity.py` — parsea el CSV: en las seis filas de S4 las siete columnas de
   texto contienen `100` y ninguna contiene `20 u/s`, `20 E/s` ni `20 ед/с`; las filas del Laser y
   de la Memory Cell son exactamente las de arriba; el numero de columnas no cambia.

Cada test sale con 0 en verde y distinto de 0 en rojo (`unittest`).

## Gates antes de cada commit

1. Linter offline de `AGENTS.md` §1 sobre la base y sobre tu arbol: **delta de errores = 0**
   (gatea por `len(errors)`). Y `ui_reconcile.py`: 0 FAIL.
2. Barrido de `AGENTS.md` §1: cada `obj.m_Campo` nuevo esta declarado en la clase de `obj` o en un
   padre. Cada simbolo nuevo de `3_Game` usado en `4_World`/`5_Mission` existe en `3_Game`.
3. Convenciones de `AGENTS.md` §3 en cada linea nueva (ni `? :`, ni `++`, ni `+=`, ni `foreach`,
   ni `Print(`, `ref` solo en miembros).
4. Gate A de EOL (`git diff --numstat` igual a `git diff --ignore-cr-at-eol --numstat` por fichero).
5. Todos los `test_*.py` de `.github/tools` en verde, otra vez despues del commit (gate B).
6. Diff minimo: cada linea traza a S1-S4 o a los tests. Lo que veas raro al lado va al informe,
   no al diff.

## Entrega

Commit(s) en `claude/project-thread-0iolln` (mensaje en castellano, con "(Grok, ronda 1)", sin
version de modelo) y push. Informe: SHA, salida de los gates (cuenta de errores base/arbol), lista
de tests con su resultado, y todo lo que no hayas podido verificar sin arrancar el juego. No
arranques DayZ.
