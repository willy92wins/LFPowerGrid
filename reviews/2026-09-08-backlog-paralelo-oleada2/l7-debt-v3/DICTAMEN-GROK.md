VERDE — 0 GRAVE / 2 MEDIO / 2 MENOR

Revisión adversarial del rescate de `debt/v3-data-integrity` contra `BRIEF.md`, `CAMBIOS.diff` e `INFORME.md`. Código leído en el árbol ya modificado. No se ha compilado ni arrancado el juego. El HEAD de este worktree es `d59cad892557d8ec8dcfed0bfca0ba1c5744db45`, no el `8de29d5` que anuncia el brief.

### MEDIO — Tras el latch, `GetAll` oculta a los dos vivos

`scripts/4_World/LFPG_DeviceRegistry.c:126`, `scripts/4_World/LFPG_DeviceRegistry.c:165`, `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:773`, `scripts/5_Mission/LFPG_MissionInit.c:28`, `scripts/5_Mission/LFPG_NetworkManagerImpl.c:1205`

V3-07 cierra el defecto de stock: al colisionar dos entidades vivas el ID sale de `m_ById` (`:58`), `FindById` devuelve null (`:98`) y las puertas de mutación/reconcile niegan `IsAmbiguous` (`scripts/4_World/LFPG_AtmStock.c:19`, `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:414`, `:508`, `:536`, `:1277`). Eso impide aplicar claims al ATM equivocado.

`GetAll` sigue recorriendo solo el mapa canónico. Los dos objetos viven en `m_AllRegistered`, pero desaparecen de FullSync, grafo, validación y del re-registro de boot, que siguen llamando a `GetAll`. El único consumidor nuevo del barrido safety es el preflight T2 de `FinishWiring`. Un duplicado deja de ser un mixup de saldo y pasa a ser un agujero de visibilidad: cables y potencia de ese ID no se reconstruyen hasta reinicio.

Importa porque el informe presenta V3-07 como identidad canónica global, y en stock lo es; en el grafo no. Habría que decidir qué iteradores de seguridad deben usar `GetAllRegisteredForSafety` y cuáles deben seguir ciegos a propósito.

### MEDIO — El latch sobrevive a los dos objetos y congela el sweep de huérfanos

`scripts/4_World/LFPG_DeviceRegistry.c:79`, `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1504`

`Unregister` retira el puntero de `m_AllRegistered` y, si el ID está latched, vuelve sin tocar el latch (`:79`). `SweepOrphanClaims` hace `continue` ante `IsAmbiguous` antes de `FindById` (`:1504`), para no refundir un ATM que sigue en el mundo.

Si los dos duplicados se borran en la misma sesión, el ID sigue ambiguo, no hay entidad, y el sweep no avanza `orphanBoots` ni tombstonea. Los claims de ese `deviceId` quedan congelados hasta reinicio, cuando el registro nace vacío. El informe lo describe como diseño; el producto queda con dinero/stock en cuarentena de proceso aunque ya no exista el hardware.

Habría que levantar el latch cuando `m_AllRegistered` ya no contiene ningún vivo con ese ID, o distinguir «ambiguo con hardware» de «ambiguo residual» en el sweep.

### MENOR — Indentación nueva con tabuladores en ficheros de espacios

`scripts/3_Game/LFPG_Util.c:23`, `scripts/4_World/LFPG_DeviceRegistry.c:15`, `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:562`

Las líneas añadidas usan tabulador; el cuerpo histórico de esos ficheros indenta con espacios. No hay normalización CRLF ni reordenado del árbol: el diff es el cambio. No es un reformateo encubierto que esconda el port, pero mezcla dos estilos en el mismo bloque.

### MENOR — El enter CCTV en vehículo no responde al cliente

`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1241`

La guarda llega antes de `BeginCCTV` y de `SelectPlayer(null)` (`:1257`). Eso cierra V3-10. Llega después de resolver cámaras (`:1232`) y vuelve en silencio: un RPC fabricado no desposee, pero el cliente se queda esperando `CAMERA_LIST_RESPONSE`. La acción normal no llega aquí porque `LFPG_ActionWatchMonitor.c:78` ya negó `IsInVehicle`. Habría que denegar antes del trabajo de resolución y devolver un resultado vacío o un mensaje, sin relajar el rechazo.

## Fichas

| Ficha | Implementador | Revisor |
|---|---|---|
| V3-01 — venta a cuenta y fallo de crédito | NO-APLICA / YA-RESUELTO | YA-RESUELTO para el mecanismo descrito |
| V3-02 — cash ledger vs inventario | NO-ARREGLADA / CONFLICTO | NO-ARREGLADA |
| V3-03 — fold físico al reconciliar | NO-ARREGLADA / CONFLICTO | NO-ARREGLADA |
| V3-04 — tombstone antes del hive | NO-APLICA / YA-RESUELTO | YA-RESUELTO |
| V3-05 — cuarentena de claims | PENDIENTE-DECISION / CONFLICTO | CONFLICTO, sin port |
| V3-06 — relectura del target | PENDIENTE-DECISION / CONFLICTO | CONFLICTO, sin port |
| V3-07 — identidad canónica | ARREGLADA / PORTAR | ARREGLADA, con holgura (MEDIO) |
| V3-08 — reemplazo no destructivo | NO-APLICA / YA-RESUELTO | YA-RESUELTO por T2 |
| V3-09 — lease del searchlight | ARREGLADA / PORTAR | ARREGLADA |
| V3-10 — CCTV desde vehículo | ARREGLADA / PORTAR | ARREGLADA como prevención |
| V3-11 — preview del sorter | NO-APLICA / OBSOLETO | OBSOLETO en V3; el defecto vive en el handler compartido |
| V3-12 — RPC 13/14 | ARREGLADA / PORTAR | ARREGLADA |
| V3-13 — warnings y purga | ARREGLADA / PORTAR | ARREGLADA |
| CX-A-p2 H-04 — depósito BTC físico | NO-ARREGLADA / CONFLICTO | NO-ARREGLADA |
| CX-A-p2 H-05 — retirada BTC física | NO-ARREGLADA / CONFLICTO | NO-ARREGLADA |
| CX-E H-05 — salto de nonce | ARREGLADA / PORTAR | ARREGLADA |
| err=11 — provider ausente | NO-APLICA / YA-RESUELTO | YA-RESUELTO |
| err=14 — cantidad sobre el cap | NO-ARREGLADA / CONFLICTO | NO-ARREGLADA |
| LFPG-UI-01 — color de fallo sorter | NO-APLICA / OBSOLETO | OBSOLETO; V4 ya usa `FAILED` rojo |
| Extra — `LFPG_VERSION_STR` | NO-APLICA / YA-RESUELTO | YA-RESUELTO (`1.2.4`) |

V3-01: el crédito va antes de destruir (`scripts/5_Mission/LFPG_BTCHelper.c:1932`) y la desigualdad revierte y deja el BTC (`:1934`, destroy en `:1962`). El fallo de I/O que el dossier describía (destruir y no restaurar) no está. Queda la ventana crash-después-del-crédito, que el informe no finge haber cerrado.

V3-02: débito de retirada en `:2582` y crédito de depósito en `:2790`, separados del hive del inventario. Sin donante. No portar es correcto.

V3-03: el reconcile sigue borrando el tail físico (`scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1105`). Los guards de V3-07 no tocan este camino.

V3-04: E16 conserva tombstones tras compensar en RAM (`:1246`–`:1250`). El umbral de tres boots no poda (`:1401`–`:1407`).

V3-05: el escritor fusiona solo el target (`:571`). Un fold 20→15 y 15→20 deja 20→20. El validador donante de transición vacía cuarentenaría eso. No portar es la lectura correcta. La carga sigue insertando claims sin cuarentena (`:2083`).

V3-06: `.saving` se escribe en `scripts/3_Game/LFPG_FileUtil.c:271`; la promoción huérfana exige marcador y backup (`:859`). Después de `CopyFile` (`:282`) no hay comparación del target con un expected independiente. El donante sustituiría el protocolo actual.

V3-07: el latch y las puertas de stock/reconcile están donde el informe dice, con las citas Native agrupadas en la cabecera de cada función (`:397`, `:495`, `:523`, `:1464`) y el guard real unas líneas más abajo (`:414`, `:508`, `:536`, `:1504`). No son citas inventadas. La adaptación T2 (`:562`, `:733`, `:773`) era necesaria: `GetAll` ya no vería el extremo ambiguo.

V3-08: `CanCreatorCutWire` en cada conflicto (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:738`; definición `scripts/3_Game/LFPG_WireHelper.c:230`). Reserva de arista con los viejos intactos (`:591`–`:609`). El helper de índice de la rama no se reintroduce.

V3-09: lease al entrar (`scripts/5_Mission/LFPG_ControlSessionRegistry.c:136`), heartbeat cliente (`scripts/4_World/LFPG_SearchlightController.c:356`, `scripts/3_Game/LFPG_Defines.c:741`), renovación tras potencia/rango/operador y antes del limiter AIM (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1571`). `Tick` sigue expirando `m_DeadlineMs` (`:434`). `LFPG_CONTROL_SESSION_TIMEOUT_MS` es 125000 (`scripts/5_Mission/LFPG_ControlSessionRegistry.c:15`); `ArmSearchlightExitDeadline` (`:188`) no puede alargar un lease de 10 s. Los EXIT inválidos no mantienen el lock. El `EndSearchlight` de las salidas es síncrono (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1602`, `:1686`).

V3-10: `IsInVehicle` en acción (`scripts/4_World/LFPG_ActionWatchMonitor.c:78`, `player` ya no es null en `:53`) y en servidor (`:1241`) antes de `BeginCCTV`. La API vanilla es comando VEHICLE o padre `Transport` (`C:/Users/guill/LFPG_PORT_ws/rebase/_debt/vanilla-ref/4_world/entities/dayzplayerimplement.c:465`). No hay restauración de `HumanCommandVehicle`.

V3-11: no se tocó el sorter V3. El dispatch de preview V4 sigue en `:159`–`:161` y `canProceed` solo exige objeto y distancia (`:2908`–`:2914`), sin energía ni ruined.

V3-12: `CAMERA_CYCLE`/`CAMERA_UNLINK` ya no tienen policy (`scripts/4_World/LFPG_RPCGuard.c:93` cae a 0) ni handlers; el deny es antes de `ctx.Read` (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:41`). Los números quedan en el enum (`scripts/3_Game/LFPG_Defines.c:288`). No quedan call-sites de `HandleCameraLink` / `HandleCameraUnlink`.

V3-13: `RateLimitedWarn` del sliding window con `LogUid` (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:741`). Purga en `scripts/3_Game/LFPG_Util.c:44` con `GetTickTime`, el mismo reloj que `RateLimitedWarn` (`:39`). El mapa de cooldown de acciones sigue en `g_Game.GetTime() * 0.001` (`:718`). El scratch `s_StaleWarnRateLimitKeys` es miembro `ref`, no local.

CX-E: gap 1_000_000 en `CheckRequest` (`scripts/5_Mission/LFPG_BTCSessionRegistry.c:296`) y `ReserveRequest` (`:332`) después de replay/in-flight exactos (`:271`, `:284`). El salto de un paquete a la zona de agotamiento (CAP 2e9, guarda 1000) no entra. Queda caminar el watermark en peldaños de 1e6; no es el auto-DoS de un paquete que describía la ficha.

err=11: `noProviderZeros` en `scripts/3_Game/LFPG_BTCDefines.c:259`. err=14: Buy manda saldo real (`scripts/5_Mission/LFPG_BTCHelper.c:1160`); Sell/Withdraw/Deposit/WithdrawCash/DepositCash mandan 0 (`:1640`, `:2127`, `:2294`, `:2515`, `:2697`). BTCHelper no está en el diff ni en la lista blanca.

LFPG-UI-01: V4 compara y ACK-ea `"FAILED"` en rojo (`scripts/4_World/test/LFPG_SorterController_TEST.c:740`, `:797`, `:818`). Extra: `LFPG_VERSION_STR` ya es `"1.2.4"` (`scripts/3_Game/LFPG_Defines.c:527`); en Defines solo se añaden las constantes de V3-09.

## Alcance, convenciones y formato

`CAMBIOS.diff` toca doce scripts: `LFPG_Defines.c`, `LFPG_Util.c`, `LFPG_ActionWatchMonitor.c`, `LFPG_AtmStock.c`, `LFPG_DeviceRegistry.c`, `LFPG_RPCGuard.c`, `LFPG_SearchlightController.c`, `LFPG_BTCSessionRegistry.c`, `LFPG_BalanceProvider_NativeImpl.c`, `LFPG_ControlSessionRegistry.c`, `LFPG_NetworkManagerImpl.c`, `LFPG_RPCServerHandlerImpl.c`.

La lista blanca nombra diez de ellos y admite «los demás que veas en su diff» salvo sorter V3, `LFPG_Migrators.c`, `LFPG_Telemetry.c` y `LFPG_CableRenderer.c`. Defines, Util y ActionWatchMonitor son los ficheros de V3-09, V3-13 y V3-10 en los informes donantes (`IMPL-F45-REPORT.md`, `IMPL-F3-REPORT.md`). No están excluidos. `LFPG_NetworkManager.c` (fachada World) está en la lista y no se toca: el V3-08 donante que lo editaba queda en YA-RESUELTO por T2. Sorter V3, FileUtil, BTCHelper y BTCDefines no se modifican.

En las líneas añadidas no hay ternarios, `++`/`--`, `+=`/`-=`, `foreach` ni `Print(`. Los `ref` nuevos son miembros de clase (`s_StaleWarnRateLimitKeys`, `m_AmbiguousIds`, `m_AllRegistered`). Los bucles usan `i = i + 1` / `safetyIndex = safetyIndex - 1`. `s_ReadyMission` es `MissionBaseWorld` vanilla, no un tipo de 5_Mission. World no llama a tipos de Mission.

El diff no está inflado por CRLF. El cambio es el port, con la mezcla de tabs del MENOR.

## Honestidad del informe

`LO QUE NO PUDE VERIFICAR` no es relleno: compilación, dos clientes, kill/restart, hive, weak refs y las otras lanes son huecos reales. No declara «compila».

Las citas de la tabla que abrí casan con el código. Las Native de V3-07 en el cuerpo (`:397`, `:495`, `:523`, `:1464`) son las cabeceras de función, no la línea del `IsAmbiguous`; el receptor abre la función correcta. `MissionInit.c:19` es el `DrainPendingReconcile`. `LFPG_RateLimiter.c:19` es `GetNextAllowed`.

El recuento 6 PORTAR / 4 YA-RESUELTO / 2 OBSOLETO / 7 CONFLICTO coincide con la tabla. «12 de 19 hechos» del brief queda desglosado: err=11 era refutación, siete nunca tuvieron donante, la versión es extra.

No he vuelto a ejecutar `enforce_checks.py` ni el script_validator; no trato esos exit codes como prueba.

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

El brief ancla el worktree en `8de29d5`. Este árbol está en `d59cad8` (commit de main «L1 MEDIO: el guard de desmontaje…»). `main` del repo común ya avanzó otra vez a `6e9c0e7`. El retraso de «15 commits» y el SHA del brief no prueban el solapamiento actual.

«12 de 19 hechos» mezcla ports, una refutación sin diff (`err=11`) y deja siete journal/hive sin código donante. Pedir un juicio item a item es el encargo correcto; un cherry-pick habría traído FileUtil/V3-05 incompatibles con `.saving` y E16.

V3-05 presupone un validador que acepta todo lo que el escritor de hoy escribe. El fold neto cero (`:571`) lo contradice. Portar el validador donante cuarentenaría datos del propio Native actual. V3-04 conservando tombstones añade otra discontinuidad `previousTarget != stockBefore` que el donante rechazaría.

V3-01 y V3-04 tienen cerrado el mecanismo concreto (orden crédito/destrucción; no borrar el tombstone al compensar). El journal WAL del plan v2 (`PLAN-DEUDA-V3-codex-v2.md`) no está, y el brief no autoriza inventarlo. Tratar esos dos como «deuda de datos resuelta» sería hinchar el YA-RESUELTO.

«Muere con la V3» vale para `LFPG_SorterController.c`. El preview (subId 31 y 67) es un handler de Mission compartido. V3-11 sigue abierto en V4. LFPG-UI-01 sí está muerto en V3 y ausente en V4.

El donante de V3-09 sustituía el lease de 10 s por 125 s en cada EXIT, antes de validar el payload. Con lease < timeout, esa asignación incondicional alarga el lock. Acortar-solo deja `ArmSearchlightExitDeadline` inerte en una sesión sana (10 s no es mayor que now+125 s). Como `EndSearchlight` es síncrono en las dos salidas, el armado no hace falta para el happy path. Si alguien espera la gracia de 125 s «terminal-pending» del comentario de `:13`, el brief no la recupera.

err=14 no es obsoleto por estar fuera de lista blanca. Cinco de seis operaciones siguen pintando saldo 0.

`LFPG_NetworkManager.c` en la lista blanca invita a portar el preflight donante de V3-08. T2 ya cubre el mismo permiso por otro camino. Portarlo habría chocado con la lane de autoridad.

La identidad canónica de V3-07 no puede ser total mientras `GetAll` sea el iterador del grafo y el latch no se limpie al quedarse el mundo sin esos objetos. El brief pide portar, no rediseñar todos los consumidores.

## LO QUE NO PUDE COMPROBAR

- Compilación real de 3_Game / 4_World / 5_Mission y carga de mundo: no hay compilador invocable. Lectura estática: no hay llamada World→tipo de Mission nueva; `GetAllRegisteredForSafety` y `IsAmbiguous` viven en 4_World y se usan desde 5_Mission.
- Heartbeat, lease, dos clientes, pérdida de paquetes, CCTV desde asiento concreto, y si `IsInVehicle` cubre todos los asientos del motor: sin juego. La firma vanilla se leyó en el snapshot `_debt/vanilla-ref`, no en el P: de esta máquina.
- Kill entre crédito y destroy (V3-01), entre ledger e inventario (V3-02), entre stock e items (H-04/H-05), y recovery de tombstones en dos boots (V3-04).
- Si `s_ReadyMission` sin `ref` (`scripts/4_World/LFPG_AtmStock.c:41`) puede anularse por GC del script y dejar `s_ReadyMission == mw` falso, encolando ATMs posteriores a `Drain` que nadie vuelve a drenar. `DrainPendingReconcile` solo se llama en `MissionInit.c:19`.
- Orden real EEInit/Register vs `AfterStoreLoad`/`ReconcileLoadedAtm`. El informe admite que el latch solo ve duplicados ya registrados.
- Integración con las otras cuatro lanes: no se leyeron sus worktrees. Defines y Util son ficheros compartidos.
- `git diff main...debt/v3-data-integrity --stat` no se ejecutó (este modo no lanza git). El donante `5df9f472bef82a161fdd533b57c0c262b779506c` se leyó por la ref `refs/heads/debt/v3-data-integrity` y por los IMPL-T1/F3/F45. El contenido exacto de ese tree no se releyó con `git show`.
- Los FAIL=0 del validador Python que cita el informe.
