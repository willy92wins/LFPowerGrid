# INFORME — lane i77 (reloj de cargadores, G-02/G-03)

## Tabla

| punto | veredicto | ficheros:lineas | prueba |
|---|---|---|---|
| G03-a switch externo tarde | ACEPTABLE-DOCUMENTADO | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:493-498`; `scripts/5_Mission/LFPG_ElecGraphImpl.c:3344-3362`; `scripts/3_Game/LFPG_Defines.c:205,492` | `test_g03a_switch_poll_error_bound_from_constants`; PENDIENTE-INGAME protocolo G03-a |
| G03-b misma bateria detach+reinsert | ARREGLADO | `scripts/4_World/LFPG_BatteryChargerMod.c:18,33`; `scripts/4_World/LFPG_ElecGraph.c:284-286`; `scripts/5_Mission/LFPG_ElecGraphImpl.c:3277-3307` | `test_g03b_detach_attach_poll_both_engine_orders` (falla en `02e8526`; pasa ahora); `test_g03b_negatives_no_hook_and_r1_full_slot_credit_the_gap` |
| G03-c reentrada AddEnergy | INALCANZABLE | `scripts/5_Mission/LFPG_ElecGraphImpl.c:3261-3267,3222-3223` | `test_clock_is_published_before_energy_callback_reenters`; `test_g03c_reentry_at_same_timestamp_does_not_double_credit` |
| G03-d reemplazo por otro objeto | ACEPTABLE-DOCUMENTADO | `scripts/5_Mission/LFPG_ElecGraphImpl.c:3251-3254,3285-3286` | `test_g03d_replacement_does_not_inherit_and_drops_pending_of_removed`; cota C1 abajo |
| G02 tasa vs tamano | INALCANZABLE | `scripts/5_Mission/LFPG_ElecGraphImpl.c:3344-3362`; `scripts/5_Mission/LFPG_NetworkManagerImpl.c:493-498` | `test_g02_*` (el tick no toca `m_Nodes` ni PDQ) |

### G03-a, switch externo observado tarde

No anadi hook de interruptor. `IsSwitchedOn()` se lee solo en `UpdateVanillaChargerPower` (`LFPG_ElecGraphImpl.c:3256`). El scheduler llama `TickVanillaChargers` cuando `m_SchedSimpleMs >= 1000` (`LFPG_NetworkManagerImpl.c:493-498`). El tick visita `min(C, LFPG_VALIDATE_BATCH_SIZE=32)` cargadores (`LFPG_ElecGraphImpl.c:3338-3340`).

Cota: periodo `P = 1 s`, lote `B = 32`, tasa `R = LFPG_CHARGER_ENERGY_PER_SEC = 1.0 u/s`. Latencia maxima entre dos visitas de un mismo cargador: `ceil(C/B)*P` segundos. Error maximo de un cambio de switch (o bateria llena/vaciada por otro lado) no visto hasta la siguiente visita: `ceil(C/B)*P*R` unidades. Ejemplos: C<=32 → 1 s / 1 u; C=512 → 16 s / 16 u; C=2048 → 64 s / 64 u.

`ComponentEnergyManager.SwitchOn` llama `OnSwitchOn` en el entity (`P:\scripts\3_game\tools\component\componentenergymanager.c:404`); `SwitchOff` llama `OnSwitchOff` (`:438`). `BatteryCharger` ya overridea `OnSwitchOn` (`P:\scripts\4_world\entities\itembase\batterycharger.c:328`). Un hook ahi cubriria esos caminos; no lo anadi: la cota con C<=32 es ≤1 u y encaja con la cola del 5-oct (+0,6 u). Recomendacion al dueno: aceptar la cota. No cite ningun `SetSwitchedOn` en vanilla (no lo busque a fondo mas alla de SwitchOn/Off).

Verificacion offline: `test_g03a_switch_poll_error_bound_from_constants` (switch off visto un periodo tarde acredita `P*R`). In-game: `PROTOCOLO-INGAME.md` G03-a.

### G03-b, detach y reinsercion de la misma bateria

En polling puro, `sameBattery` es verdadero si el puntero coincide (`LFPG_ElecGraphImpl.c:3251-3253`) y se acredita el hueco. `02e8526` cerraba el intervalo en el detach pero volvia a guardar la misma bateria si el slot seguia lleno; el attach posterior acreditaba el tiempo fuera (62 u en la secuencia de C1).

Arreglo: `NotifyVanillaChargerAttachment(charger, detached)` (`LFPG_ElecGraph.c:284`). El hook pasa `false` en attach y `true` en detach (`LFPG_BatteryChargerMod.c:18,33`). Tras el match, `CloseVanillaChargerAttachment` (`LFPG_ElecGraphImpl.c:3277-3288`) llama a `UpdateVanillaChargerPower` y, si `detached`, `m_ChargerBatteries.Remove(nodeId)`. El reinsert siempre rebasa. Elegi extraer el metodo escalar para que el `Clock` cargue esas sentencias (el bucle de `Notify` no entra en el slicer).

Vanilla `EEItemDetached` (`P:\scripts\3_game\entities\entityai.c:1173-1196`) no dice si `FindAttachmentBySlotName` ya devolvio null; el arreglo cubre los dos ordenes.

Tests: `test_g03b_detach_attach_poll_both_engine_orders` (slot vacio 23 u, slot lleno 24 u); `test_g03b_negatives_no_hook_and_r1_full_slot_credit_the_gap` (sin hook 62 u; hook r1 slot lleno 62 u). Contra `02e8526` el test de ordenes FAIL (no hay `CloseVanillaChargerAttachment`). In-game: protocolo G03-b.

### G03-c, callbacks de terceros

No hay defecto alcanzable: el reloj se publica antes de `AddEnergy` (`LFPG_ElecGraphImpl.c:3261-3267`). Una reentrada en el mismo `GetTime` ve `nowSec <= lastSec` y `ChargerChargeAmount` devuelve 0 (`3222-3223`). No se toco codigo. Tests de argumento: `test_clock_is_published_before_energy_callback_reenters`; `test_g03c_reentry_at_same_timestamp_does_not_double_credit`.

### G03-d, reemplazo por otro objeto

`sameBattery` exige el mismo puntero; el recambio no hereda. Con C1: si `EEItemDetached` ve el slot lleno, el intervalo abierto se acredita y luego se olvida la bateria → perdida ~0. Si el slot ya esta vacio, no hay `sameBattery` y se pierde el intervalo desde la ultima visita hasta el evento (hasta `ceil(C/32)*1 u`; con C<=32, 1 u). Recomendacion: **ACEPTABLE-DOCUMENTADO**.

### G02, tasa independiente del tamano

No hay defecto alcanzable: `TickVanillaChargers` no menciona `m_Nodes`, `m_DirtyQueue` ni `ProcessDirtyQueue` (`LFPG_ElecGraphImpl.c:3344-3362`). El scheduler lo llama con los simples (`LFPG_NetworkManagerImpl.c:493-498`), no con PDQ. No se toco codigo. Coste `min(C,32)` visitas/llamada; latencia `ceil(C/32)*1 s` (leido de `LFPG_VALIDATE_BATCH_SIZE` y `m_SchedSimpleMs >= 1000`). El 62,5 % pre-#71 era el barrido de nodos.

## GATES

- Linter base (antes de editar): `errors=0`, `warnings=48`, `status=WARN`.
- Linter final: `errors=0`, `warnings=48`. Delta errors 0. Delta warnings 0.
- Linter ronda 2 final: `errors=0`, `warnings=48`. Delta 0.
- `C:\Python314\python.exe .github/tools/test_graph_charger_energy.py`: exit 0, `Ran 19 tests`, OK.
- `enforce_checks.py --root .`: `FAIL=0 WARN=0`, exit 0.
- `test_g03b_detach_attach_poll_both_engine_orders` contra `02e8526` (GRAPH = `git show 02e8526:...ElecGraphImpl.c`): FAIL `AssertionError: 'CloseVanillaChargerAttachment(nodeId, detached)' not found`. El negativo `test_g03b_negatives_no_hook_and_r1_full_slot_credit_the_gap` OK (62 u). Con el arbol actual: ambos OK (23 u / 24 u).

## HALLAZGOS-ADYACENTES

- `GetGraph()` del facade 4_World registra Error si no hay factory (`LFPG_ElecGraph.c` / `LFPG_NetworkManager.c`); por eso el hook usa `GetExisting()`.
- `ClearVanillaChargers` cierra intervalos pero no llama `NotifyVanillaChargerAttachment` (no hace falta).
- Warnings del linter 48 (base y final); no investigados.

## LO QUE NO PUDE VERIFICAR

- Compilacion Enforce al cargar el mundo (no hay compilador invocable).
- Orden real del motor en `EEItemDetached` respecto a `FindAttachmentBySlotName` (`entityai.c:1173` no lo dice).
- EEItemAttached/Detached de `BatteryCharger` con slot distinto de `LargeBattery`.
- Si todo cambio de switch pasa por `SwitchOn`/`SwitchOff` (cite solo esos dos).
- Medida in-game G02 con grafos 512 vs 2048 nodos y backlog de `ProcessDirtyQueue`.
- Switch / bateria llena observada tarde in-game (cota offline solamente).
- Callbacks nativos de `AddEnergy` de otros mods distintos del stub `on_add`.
- Latencia real del scheduler si el servidor se atrasa mas de 1 s.

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

- G02 habla de “grafos de distinto tamano (512 y 2048 nodos)”. Tras la #71 la tasa no depende de `m_Nodes` sino de **cuantos cargadores hay en `m_ChargerIds`**. Un grafo de 2048 nodos con un solo cargador visita ese cargador cada 1 s. El 62,5 % pre-#71 era el barrido de nodos, no el reloj actual.
- G03-b “hoy sameBattery sale verdadero” es cierto **solo si no hay visita con el slot vacio**. Un detach que dispare `EEItemDetached` ya cierra; el bug es el hueco entre visitas sin evento.
- G03-d “el dueno decide”: deje cota y recomendacion ACEPTABLE; no acredito el objeto retirado en el poll.

## Ronda 2

- C1: `CloseVanillaChargerAttachment` olvida `m_ChargerBatteries` si `detached`. Slot lleno al detach: 24 u; vacio: 23 u. `02e8526` slot lleno: 62 u.
- C2: pruebas de comportamiento; el slicer carga `CloseVanillaChargerAttachment`; el hook se comprueba por texto (`false`/`true`).
- C3: no toque finales de linea de `LFPG_ElecGraph.c` mas que la firma.
- C4: G02/G03-a ya no comparan una formula consigo misma en un bucle; `nodes` no se usa.
- C5: G02 y G03-c = INALCANZABLE. G03-a cita `SwitchOn`→`OnSwitchOn`. G03-d cota segun C1.

