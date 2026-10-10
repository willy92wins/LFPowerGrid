# INFORME — lane i77 (reloj de cargadores, G-02/G-03)

## Tabla

| punto | veredicto | ficheros:lineas | prueba |
|---|---|---|---|
| G03-a switch externo tarde | ACEPTABLE-DOCUMENTADO | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:493-498`; `scripts/5_Mission/LFPG_ElecGraphImpl.c:3335-3353`; `scripts/3_Game/LFPG_Defines.c:205,492` | `test_g03a_switch_poll_error_bound_from_constants`; PENDIENTE-INGAME protocolo G03-a |
| G03-b misma bateria detach+reinsert | ARREGLADO | `scripts/4_World/LFPG_BatteryChargerMod.c:7-35`; `scripts/4_World/LFPG_ElecGraph.c:284-286`; `scripts/5_Mission/LFPG_ElecGraphImpl.c:3277-3298` | `test_g03b_*` (falla en HEAD: metodo/hook ausentes; pasa aqui) |
| G03-c reentrada AddEnergy | ARREGLADO (ya seguro; test reforzado) | `scripts/5_Mission/LFPG_ElecGraphImpl.c:3261-3272` | `test_clock_is_published_before_energy_callback_reenters`; `test_g03c_reentry_at_same_timestamp_does_not_double_credit` |
| G03-d reemplazo por otro objeto | ACEPTABLE-DOCUMENTADO | `scripts/5_Mission/LFPG_ElecGraphImpl.c:3251-3254,3220-3224` | `test_g03d_replacement_does_not_inherit_and_drops_pending_of_removed`; cota abajo |
| G02 tasa vs tamano | ARREGLADO (demostrado; sin defecto) | `scripts/5_Mission/LFPG_ElecGraphImpl.c:3335-3353`; `scripts/5_Mission/LFPG_NetworkManagerImpl.c:493-498` | `test_g02_*` (tambien pasa contra HEAD) |

### G03-a, switch externo observado tarde

No anadi hook de interruptor. `IsSwitchedOn()` se lee solo en `UpdateVanillaChargerPower` (`LFPG_ElecGraphImpl.c:3256`). El scheduler llama `TickVanillaChargers` cuando `m_SchedSimpleMs >= 1000` (`LFPG_NetworkManagerImpl.c:493-498`). El tick visita `min(C, LFPG_VALIDATE_BATCH_SIZE=32)` cargadores (`LFPG_ElecGraphImpl.c:3338-3340`).

Cota: periodo `P = 1 s`, lote `B = 32`, tasa `R = LFPG_CHARGER_ENERGY_PER_SEC = 1.0 u/s`. Latencia maxima entre dos visitas de un mismo cargador: `ceil(C/B)*P` segundos. Error maximo de un cambio de switch (o bateria llena/vaciada por otro lado) no visto hasta la siguiente visita: `ceil(C/B)*P*R` unidades. Ejemplos: C<=32 → 1 s / 1 u; C=512 → 16 s / 16 u; C=2048 → 64 s / 64 u.

Opcion descartada: `modded class BatteryCharger` en turn-on/off. No cubre todos los caminos vanilla (`SetSwitchedOn` scripted, EM interno) y un hook incompleto da falsa seguridad. El error de 1 u con C<=32 encaja con la cola medida el 5-oct (+0,6 u). Recomendacion: aceptar el bound; si un servidor registra cientos de cargadores, entonces si un hook de switch.

Verificacion offline: `test_g03a_switch_poll_error_bound_from_constants` (switch off visto un periodo tarde acredita `P*R`). In-game: `PROTOCOLO-INGAME.md` G03-a.

### G03-b, detach y reinsercion de la misma bateria

En polling puro, `sameBattery` es verdadero si el puntero coincide (`LFPG_ElecGraphImpl.c:3251-3253`) y se acredita el hueco. Contra HEAD: `visit(1)` + `visit(63)` con la misma instancia da 62 u.

Arreglo: `modded class BatteryCharger` llama `super` y, solo SERVER y slot `LargeBattery`, `LFPG_NetworkManager.GetExisting().GetGraph().NotifyVanillaChargerAttachment(this)` (`LFPG_BatteryChargerMod.c:7-35`). `GetExisting` evita crear el manager inerte en el menu. La impl recorre `m_ChargerIds`, compara la entidad y cierra el intervalo con `UpdateVanillaChargerPower(nodeId, node.m_Powered)` (`LFPG_ElecGraphImpl.c:3277-3298`). Tras el detach el mapa guarda `lastBattery` nulo; el reinsert no hereda el hueco.

Descartado: arreglarlo solo en `sameBattery` sin evento (inalcanzable: el puntero no cambia). Descartado: creditar la bateria retirada en el poll de recambio (eso es G03-d y sobre-acredita post-detach).

Tests: `test_g03b_same_battery_reinsert_closes_when_detach_is_observed` (energia 1 u); `test_g03b_poll_only_same_object_reinsert_still_credits_the_gap` (el poll sin evento sigue en 62 u; el hook y `NotifyVanillaChargerAttachment` existen); `test_g03b_negative_missing_notify_leaves_gap_credit_path_unclosed`. Contra HEAD: `NotifyVanillaChargerAttachment` ausente (`ValueError method missing`); `git cat-file` del hook sale 128. In-game: protocolo G03-b.

### G03-c, callbacks de terceros

El reloj se publica antes de `AddEnergy` (`LFPG_ElecGraphImpl.c:3261-3267`). Una reentrada en el mismo `GetTime` ve `nowSec <= lastSec` y `ChargerChargeAmount` devuelve 0 (`3222-3223`). `test_g03c_reentry_at_same_timestamp_does_not_double_credit`: un `on_add` que vuelve a `update` powered y a apagar no duplica; `added` tiene un solo elemento. No hay arreglo extra.

### G03-d, reemplazo por otro objeto

`sameBattery` exige el mismo puntero; el recambio no hereda (`test_battery_replacement` / `test_g03d_replacement_does_not_inherit_and_drops_pending_of_removed`). El intervalo pendiente del adjunto retirado se pierde hasta la siguiente visita (o hasta `EEItemDetached` si el hook corre: entonces se pierde solo el tramo detach→visita, que el hook acorta a ~0).

Cota de perdida sin contar el hook: `ceil(C/32)*1 s * 1 u/s`. Con C<=32: 1 u. Recomendacion al dueno: **ACEPTABLE-DOCUMENTADO**. Creditar `lastBattery` en el poll de recambio sobre-cargaria el objeto ya fuera del slot.

### G02, tasa independiente del tamano

`TickVanillaChargers` solo recorre `m_ChargerIds` con tope 32; no toca `m_Nodes`, `m_DirtyQueue` ni `ProcessDirtyQueue` (`LFPG_ElecGraphImpl.c:3335-3353`). El scheduler lo llama en el tick de simples, no en el drenaje de la cola sucia (`LFPG_NetworkManagerImpl.c:493-498`). 640 s alimentados a cadencias 1/16/64 s (1, 512 y 2048 cargadores a lote 32) dan 640 u (`test_g02_energy_per_fed_time_is_independent_of_node_count_and_dirty_queue`). Coste: `min(C,32)` visitas/llamada. Latencia: `ceil(C/32)*1 s`. El 62,5 % pre-#71 venia de barrer nodos, no de este reloj. Estos tests tambien pasan contra HEAD: no habia defecto que arreglar.

## GATES

- Linter base (antes de editar): `errors=0`, `warnings=48`, `status=WARN`.
- Linter final: `errors=0`, `warnings=48`. Delta errors 0. Delta warnings 0.
- `C:\Python314\python.exe .github/tools/test_graph_charger_energy.py`: exit 0, `Ran 20 tests`, OK.
- `test_graph_capacity_refresh.py`: exit 0, 7 tests.
- `test_broadcast_contract.py`: exit 0.
- `test_enforce_checks.py`: exit 0, 21 tests.
- `test_finish_wiring_quota.py`: exit 0, 6 tests.
- `enforce_checks.py --root .`: `FAIL=0 WARN=0`, exit 0.
- Fallo contra HEAD (G03-b): `git show HEAD:...ElecGraphImpl.c` no contiene `NotifyVanillaChargerAttachment`; `git cat-file -e HEAD:scripts/4_World/LFPG_BatteryChargerMod.c` exit 128; Clock sobre fuente HEAD con poll-only da 62.0 u en 62 s. Tests G02 sobre fuente HEAD: misma energia 640 u (sin defecto).

## HALLAZGOS-ADYACENTES

- `GetGraph()` del facade 4_World registra Error si no hay factory (`LFPG_ElecGraph.c` / `LFPG_NetworkManager.c`); por eso el hook usa `GetExisting()`.
- `ClearVanillaChargers` cierra intervalos pero no llama `NotifyVanillaChargerAttachment` (no hace falta).
- Warnings del linter 48 (base y final); no investigados.

## LO QUE NO PUDE VERIFICAR

- Compilacion Enforce al cargar el mundo (no hay compilador invocable).
- EEItemAttached/Detached reales de `BatteryCharger` vanilla (nombres de slot distintos de `LargeBattery`).
- Medida in-game G02 con grafos 512 vs 2048 nodos y backlog de `ProcessDirtyQueue`.
- Switch / bateria llena observada tarde in-game (cota offline solamente).
- Callbacks nativos de `AddEnergy` de otros mods distintos del stub `on_add`.
- Latencia real del scheduler si el servidor se atrasa mas de 1 s.

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

- G02 habla de “grafos de distinto tamano (512 y 2048 nodos)”. Tras la #71 la tasa no depende de `m_Nodes` sino de **cuantos cargadores hay en `m_ChargerIds`**. Un grafo de 2048 nodos con un solo cargador visita ese cargador cada 1 s. El 62,5 % pre-#71 era el barrido de nodos, no el reloj actual.
- G03-b “hoy sameBattery sale verdadero” es cierto **solo si no hay visita con el slot vacio**. Un detach que dispare `EEItemDetached` ya cierra; el bug es el hueco entre visitas sin evento.
- G03-d “el dueno decide”: deje cota y recomendacion ACEPTABLE; no acredito el objeto retirado en el poll.
