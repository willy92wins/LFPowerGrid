# Rescate de debt/v3-data-integrity

| Identificador | Fichero principal | Decisión | Evidencia en el árbol entregado |
|---|---|---|---|
| V3-01 | LFPG_BTCHelper.c | YA-RESUELTO | `scripts/5_Mission/LFPG_BTCHelper.c:1934` |
| V3-02 | LFPG_BTCHelper.c | CONFLICTO | `scripts/5_Mission/LFPG_BTCHelper.c:2582`; `scripts/5_Mission/LFPG_BTCHelper.c:2790` |
| V3-03 | LFPG_BalanceProvider_NativeImpl.c | CONFLICTO | `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1105` |
| V3-04 | LFPG_BalanceProvider_NativeImpl.c | YA-RESUELTO | `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1248`; `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1401` |
| V3-05 | LFPG_BalanceProvider_NativeImpl.c | CONFLICTO | `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:571`; `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1248` |
| V3-06 | LFPG_FileUtil.c | CONFLICTO | `scripts/3_Game/LFPG_FileUtil.c:271`; `scripts/3_Game/LFPG_FileUtil.c:859` |
| V3-07 | DeviceRegistry; AtmStock; NativeImpl; RPCServerHandlerImpl | PORTAR | `scripts/4_World/LFPG_DeviceRegistry.c:59`; `scripts/4_World/LFPG_AtmStock.c:48`; `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:773` |
| V3-08 | LFPG_RPCServerHandlerImpl.c | YA-RESUELTO | `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:738`; `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:603` |
| V3-09 | ControlSessionRegistry; SearchlightController; RPCServerHandlerImpl | PORTAR | `scripts/5_Mission/LFPG_ControlSessionRegistry.c:136`; `scripts/4_World/LFPG_SearchlightController.c:356`; `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1571` |
| V3-10 | ActionWatchMonitor; RPCServerHandlerImpl | PORTAR | `scripts/4_World/LFPG_ActionWatchMonitor.c:78`; `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1241` |
| V3-11 | LFPG_RPCServerHandlerImpl.c, preview compartido | OBSOLETO | `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:161`; `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2908` |
| V3-12 | RPCGuard; RPCServerHandlerImpl | PORTAR | `scripts/4_World/LFPG_RPCGuard.c:93`; `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:41` |
| V3-13 | Util; NetworkManagerImpl | PORTAR | `scripts/3_Game/LFPG_Util.c:44`; `scripts/5_Mission/LFPG_NetworkManagerImpl.c:743` |
| CX-A-p2 H-04 | LFPG_BTCHelper.c | CONFLICTO | `scripts/5_Mission/LFPG_BTCHelper.c:2383` |
| CX-A-p2 H-05 | LFPG_BTCHelper.c | CONFLICTO | `scripts/5_Mission/LFPG_BTCHelper.c:2192` |
| CX-E H-05 | LFPG_BTCSessionRegistry.c | PORTAR | `scripts/5_Mission/LFPG_BTCSessionRegistry.c:296`; `scripts/5_Mission/LFPG_BTCSessionRegistry.c:332` |
| err=11 | LFPG_BTCDefines.c | YA-RESUELTO | `scripts/3_Game/LFPG_BTCDefines.c:259` |
| err=14 | LFPG_BTCHelper.c | CONFLICTO | `scripts/5_Mission/LFPG_BTCHelper.c:1640`; `scripts/5_Mission/LFPG_BTCHelper.c:2515` |
| LFPG-UI-01 | LFPG_SorterController.c; referencia V4 en test/ | OBSOLETO | `scripts/4_World/test/LFPG_SorterController_TEST.c:740`; `scripts/4_World/test/LFPG_SorterController_TEST.c:797` |
| Extra: versión 1.2.4 | LFPG_Defines.c | YA-RESUELTO | `scripts/3_Game/LFPG_Defines.c:527` |

## Alcance y criterio

Base real: `d59cad892557d8ec8dcfed0bfca0ba1c5744db45`. Donante: `5df9f472bef82a161fdd533b57c0c262b779506c`. El brief anuncia `8de29d5`, pero no coincide con el HEAD encontrado. Se localizaron símbolos por contenido. No hay CLAUDE.md en este worktree; no se retomaron objetivos históricos de footprint de la memoria del proyecto.

Los 19 items dan **6 PORTAR, 4 YA-RESUELTO, 2 OBSOLETO y 7 CONFLICTO**. El ajuste de versión figura aparte. V3-05/06 tienen código donante incompatible; los otros cinco conflictos eran pendientes sin implementación y necesitan trabajo fuera de este port. En estos cinco, CONFLICTO expresa alcance/dependencias, no un conflicto textual de Git. No se declara resuelta una ficha por no tener diff.

Se consultaron el mensaje del commit y las descripciones históricas de `C:/Users/guill/LFPG_PORT_ws/rebase/_debt/PLAN-DEUDA-V3-codex-v2.md` y `DEUDA-V3-DOSSIER.md` para identificar los pendientes. Ese plan no se tomó como aprobación de su journal: manda el brief actual.

Hay 12 scripts modificados, todos en lista blanca. FileUtil, NetworkManager World, sorter V3, test/ y los ficheros excluidos de otras lanes no se modifican. No cambian formatos JSON/hive, IDs RPC ni directivas de preprocesador. No hubo merge, cherry-pick, rebase, checkout, add, stash, reset ni commit. Escritura propia adicional: este informe.

Mapa de datos previo a los cambios: registro/claims son servidor; World llega a Native de Mission por los métodos existentes de MissionBaseWorld (`scripts/4_World/LFPG_PlayerRPC.c:45`). AIM va del cliente al servidor, que conserva sesión y valida rango/operador. El guard de vehículo existe en acción y servidor. Warnings y purga son estado volátil. No se añade referencia directa World→tipo definido en Mission.

### V3-01 — Venta a cuenta y fallo de crédito

**Veredicto: NO-APLICA. Decisión: YA-RESUELTO para el mecanismo original.**

Main acredita antes de destruir BTC (`scripts/5_Mission/LFPG_BTCHelper.c:1932`) y exige crédito exacto: ante cero/parcial revierte lo acreditado y retorna con BTC intactos (`scripts/5_Mission/LFPG_BTCHelper.c:1934`). La rama no implementó este pendiente. Se conserva E04 y se descarta trasplantar el journal histórico fuera de lista blanca.

In-game: crédito correcto, cero y parcial, comprobando BTC, saldo y `.sell`. Esta clasificación no certifica atomicidad de todos los kill/restart entre ledger e hive; el objetivo más amplio del journal histórico no queda demostrado.

### V3-02 — Cash entre ledger e inventario persistente

**Veredicto: NO-ARREGLADA. Decisión: CONFLICTO de alcance/dependencia.**

Sin implementación donante. Persisten débito de retirada (`scripts/5_Mission/LFPG_BTCHelper.c:2582`) y crédito de depósito (`scripts/5_Mission/LFPG_BTCHelper.c:2790`), separados del guardado del inventario. Los planes de staging/rollback actuales no prueban commit común con hive.

Se descarta desactivar operaciones o diseñar aquí un journal con marcadores: no es un port y excede la lista blanca. In-game: kill entre cada efecto y cada save, reinicio, y conservación exacta de billetes más saldo sin duplicar.

### V3-03 — Fold físico descartado al reconciliar

**Veredicto: NO-ARREGLADA. Decisión: CONFLICTO de dependencia.**

El código sigue eliminando segmentos físicos del tail al reconciliar (`scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1105`). La rama lo dejó pendiente del journal. Los guards de identidad nuevos de V3-07 no resuelven este mecanismo.

Se descarta bloquear/eliminar folds del escritor unilateralmente: cambiaría operaciones live. In-game: purchase PENDING con depósito/retirada física, kill antes/después de save de ATM y player, y comprobar stock, inventario, saldo y records al reiniciar.

### V3-04 — Tombstone antes de entrega al hive

**Veredicto: NO-APLICA. Decisión: YA-RESUELTO para el borrado prematuro descrito.**

E16 conserva los tombstones después de aplicar compensación en RAM y marca la reaplicación del boot (`scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1248`). El umbral de edad ya no los poda (`scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1401`). Otro boot que cargue el stock compensado puede limpiarlos. La rama no implementó esta ficha pendiente.

Se descarta restaurar el borrado inmediato anterior. In-game: kill después de compensar y antes de guardar ATM; reiniciar dos veces; ATM ausente más de tres boots y reintroducido. Sin refund doble ni pérdida de la orden de compensación. La aceptación de operaciones nuevas con tombstones conservados es un límite relevante para V3-05, no una prueba de cierre global del recovery.

### V3-05 — Cuarentena de claims cargados

**Veredicto: PENDIENTE-DECISION. Decisión: CONFLICTO. Sin port.**

El donante rechaza un fold físico con stockTarget igual a stockBefore (buscar `physical legacy stock transition is empty` en `git show debt/v3-data-integrity:scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c`). El escritor actual fusiona modificando solo el target (`scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:571`): fold 20→15 seguido de 15→20 produce legítimamente 20→20. El port literal cuarentenaría datos del propio escritor.

Además, el donante exige continuidad entre todos los records (`previousLoadedTarget != loadedClaim.stockBefore`). E16 conserva un tombstone refunded 10→20, aplica stock=10 y permite seguir reconciliado (`scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1248`). Una compra posterior 10→15 no continúa desde aquel target 20. Decidir entre bloquear nuevas operaciones, segmentar épocas o cambiar el validador es una decisión de recovery, no una adaptación mecánica autorizada aquí.

La carga permanece sin cuarentena de entrada (`scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:2076`). Se descarta portar solo latches y declarar arreglado el item. Fixtures e in-game: fold neto cero, tombstone compensado seguido de compra, null, estado desconocido, rangos inválidos, identidad vacía y discontinuidad auténtica. Los legítimos deben funcionar; inválidos se conservan sin refund ni pérdida de evidencia.

### V3-06 — Relectura independiente del target

**Veredicto: PENDIENTE-DECISION. Decisión: CONFLICTO. Sin port.**

Main escribe `.saving` antes de reemplazar (`scripts/3_Game/LFPG_FileUtil.c:271`), y requiere marcador más backup para promover un tmp huérfano sin target (`scripts/3_Game/LFPG_FileUtil.c:859`). El donante sustituye ese camino por promoción tipada sin este protocolo; además perdería los hooks actuales de fault injection.

El defecto sigue: tras `scripts/3_Game/LFPG_FileUtil.c:282` no se compara el target con expected independiente antes del éxito. Marcador no equivale a integridad de contenido. Se descarta sustituir el fichero o inventar aquí la integración entre dos protocolos de abort/recovery. La solución futura debe conservar `.saving`, abortos y hooks, añadiendo verificación de target/restauraciones.

In-game con fallos de I/O: copia exacta, copia truncada que reporte éxito, backup divergente, primera escritura, abort dejando tmp y kill dentro de delete/copy. Un false reportado no puede reaparecer como crédito en el siguiente boot; éxito exige contenido igual al snapshot esperado.

### V3-07 — Identidad canónica y duplicados vivos

**Veredicto: ARREGLADA. Decisión: PORTAR con adaptación a T2.**

Registro idempotente por objeto, sustitución de muerto y latch al colisionar dos vivos (`scripts/4_World/LFPG_DeviceRegistry.c:32`; `scripts/4_World/LFPG_DeviceRegistry.c:59`). FindById niega el ID ambiguo; ambos objetos permanecen en el barrido safety (`scripts/4_World/LFPG_DeviceRegistry.c:165`). Unregister no levanta el latch hasta reinicio.

Cola deduplicada de reconciliación hasta drenar misión (`scripts/4_World/LFPG_AtmStock.c:48`; `scripts/4_World/LFPG_AtmStock.c:65`); guards en World (`scripts/4_World/LFPG_AtmStock.c:19`), en las tres puertas Native de compra/stock (`scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:397`; `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:495`; `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:523`), reconciliación (`scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1279`) y sweep (`scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1464`). El drenaje existente se llama desde `scripts/5_Mission/LFPG_MissionInit.c:19`; se conserva la separación de módulos.

T2 usaba GetAll, que oculta ambiguos. Su preflight ahora enumera safety (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:773`) y niega extremos ambiguos (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:562`), así como owner/target ambiguo o nativo no canónico entre los wires a retirar (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:733`). Sin adaptación, un cable conflictivo quedaría invisible al autorizador. Se conserva reserva/commit/publicación T2; se descarta añadir el helper antiguo de reverse index.

In-game: dos ATMs con ID igual y claim, sin mover stock/saldo ni refund/orphan-age; retirar uno no levanta latch, restart con uno sí. Control con IDs distintos. Cable sobre extremos/owners ambiguos rechaza dejando stores/grafo/índices iguales; dispositivos independientes siguen funcionando. Solo se detectan duplicados ya registrados, no entidades aún sin cargar.

### V3-08 — Autorización no destructiva de reemplazo

**Veredicto: NO-APLICA. Decisión: YA-RESUELTO por T2.**

T2 comprueba CanCreatorCutWire antes de retirar conflictos (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:738`; definición `scripts/3_Game/LFPG_WireHelper.c:230`). Reserva la arista y admite el wire nuevo con los anteriores intactos (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:591`; `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:603`).

Se descarta reintroducir remove-before-add y el helper de índice de la rama. La integración V3-07 anterior conserva esa solución. In-game: Alice/Bob, policy false/true, propios/unclaimed/ajenos, owner vanilla no registrado, índice inconsistente, cap y rechazo de admisión. Todo rechazo deja stores/grafo/contadores iguales y puerto desbloqueado. La inspección del orden no ejecuta esa garantía en el motor.

### V3-09 — Lease del searchlight

**Veredicto: ARREGLADA. Decisión: PORTAR.**

Heartbeat cliente 2 s y lease servidor 10 s (`scripts/3_Game/LFPG_Defines.c:741`; `scripts/4_World/LFPG_SearchlightController.c:356`; `scripts/5_Mission/LFPG_ControlSessionRegistry.c:136`). Renovación después de validar sesión, potencia, jugador, rango y operador, antes del limiter AIM (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1571`; `scripts/5_Mission/LFPG_ControlSessionRegistry.c:152`). Tick ya expira el deadline (`scripts/5_Mission/LFPG_ControlSessionRegistry.c:434`).

Corrección al donante: EXIT arma deadline antes de validar números. Su asignación incondicional permitiría prolongarlo con EXIT inválidos repetidos. Ahora solo se acorta (`scripts/5_Mission/LFPG_ControlSessionRegistry.c:188`). Se descarta copiar aquella renovación incondicional.

In-game: cliente quieto mantiene sesión por heartbeat; silencio libera en 10 s más scheduler; AIM fuera de rango/no finito/no operador no renueva; EXIT inválidos repetidos no prolongan; segundo jugador puede recuperar el foco. Tiempos donantes sin medición de red real.

### V3-10 — CCTV desde vehículo

**Veredicto: ARREGLADA. Decisión: PORTAR como prevención.**

Acción y servidor niegan IsInVehicle antes de crear sesión (`scripts/4_World/LFPG_ActionWatchMonitor.c:78`; `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1241`). Definición leída en `C:/Users/guill/LFPG_PORT_ws/rebase/_debt/vanilla-ref/4_world/entities/dayzplayerimplement.c:465`: comando VEHICLE o parent Transport, fuera de BOT. Se descarta inventar restauración del comando después de desposeer.

In-game: peatón abre/cierra; conductor/pasajero no entran ni por RPC manual; asiento, cámara y controles se conservan. Se elimina la entrada expuesta; la pérdida concreta del comando era hipótesis del motor, no se afirma haberla reproducido.

### V3-11 — Preview del sorter

**Veredicto: NO-APLICA a la retirada V3. Decisión: OBSOLETO: «muere con la V3».**

No se porta el hunk sorter. **El defecto sigue en V4**: dispatch V4 llama al mismo handler (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:161`) y canProceed solo verifica objeto/distancia (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2908`) antes de leer cargo. El handler compartido no desaparece al borrar el cliente V3. Se transfiere este hallazgo a la lane sorter, sin editarlo aquí.

Se descarta marcar un cierre global o arreglar V4 contra la exclusión. In-game: RPC 67 desde sorter sin energía/ruined y en rango debe responder vacío; sano y alimentado devuelve matches. Denegación no deja al cliente esperando.

### V3-12 — RPC deprecados 13/14

**Veredicto: ARREGLADA. Decisión: PORTAR.**

Retirados policy, dispatch y ambos handlers. Los números permanecen reservados (`scripts/3_Game/LFPG_Defines.c:288`); entrada cae a policy 0 (`scripts/4_World/LFPG_RPCGuard.c:93`) y deny limitado antes de leer ctx (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:41`).

Se descarta renumerar o mantener handlers que solo amplifican RPT. Verificación estática: nombres de handlers ausentes y enum como únicos usos. In-game: spam 13/14 sin mutación ni sesión, log acotado; REQUEST_CAMERA_LIST y salida CCTV siguen operativos.

### V3-13 — Warnings y purga

**Veredicto: ARREGLADA. Decisión: PORTAR.**

Ventana global con RateLimitedWarn y UID enmascarado (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:743`). La purga acumula claves antes de retirarlas (`scripts/3_Game/LFPG_Util.c:44`) y la llama el scheduler existente (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:811`). Usa GetTickTime igual que la creación de esos limitadores; la otra tabla conserva su reloj. GetNextAllowed se leyó en `scripts/3_Game/LFPG_RateLimiter.c:19`.

Array reutilizable inicializado aparte del mapa original para no modificar su expresión con ref. Se descarta un timer nuevo o limpiar cooldowns activos. In-game: spam de dos UID/categorías, frecuencia acotada por clave, purga de inactivos y retención de activos; medir memoria y RPT.

### CX-A-p2 H-04 — Depósito BTC físico

**Veredicto: NO-ARREGLADA. Decisión: CONFLICTO de alcance/dependencia.**

Pendiente sin implementación donante. El stock se aplica después de destruir items, con restore síncrono ante fallo (`scripts/5_Mission/LFPG_BTCHelper.c:2383`). No existe journal que rescatar ni autorización para extender schema/participantes fuera de lista blanca.

Se descarta equiparar rollback RAM a persistencia atómica. In-game: kill entre destrucción, stock y cada save hive; inventario más stock se conserva una sola vez. V3-07 evita otra causa, no esta.

### CX-A-p2 H-05 — Retirada BTC física

**Veredicto: NO-ARREGLADA. Decisión: CONFLICTO de alcance/dependencia.**

Items y retirada de stock siguen separados (`scripts/5_Mission/LFPG_BTCHelper.c:2192`). Planes actuales de creación/abort no unen los saves player/ATM. La rama no implementó esta ficha.

Se descarta invertir el orden de efectos como solución completa: desplaza la ventana de pérdida/duplicación. In-game: inventario lleno, parcial, normal y kill entre saves; conservación del total y restart sin duplicación.

### CX-E H-05 — Salto de nonce

**Veredicto: ARREGLADA. Decisión: PORTAR.**

Gap máximo 1.000.000 en CheckRequest antes de sellar in-flight (`scripts/5_Mission/LFPG_BTCSessionRegistry.c:296`) y ReserveRequest antes de mover watermark (`scripts/5_Mission/LFPG_BTCSessionRegistry.c:332`). Replay/in-flight exactos mantienen precedencia. Se descarta exigir +1: el cliente consume intentos rechazados.

Verificación del consumidor pendiente: watermark 10 con 11 y 1.000.010 acepta; 1.000.011 rechaza sin sellar ni mover. Replay/in-flight exactos, fingerprint distinto, reserva directa, reopen y extremos de int. La resta sigue a bounds/stale, con operandos no negativos y acotados.

### err=11 — Provider ausente

**Veredicto: NO-APLICA. Decisión: YA-RESUELTO.**

El cliente preserva lo mostrado ante provider ausente con ceros (`scripts/3_Game/LFPG_BTCDefines.c:259`). Era la ficha refutada sin código del commit. Se descarta tocar el contrato de errores.

In-game: panel con datos no cero, respuesta err11/0/0 conserva el cuarteto; la siguiente respuesta válida actualiza.

### err=14 — Cantidad sobre el cap

**Veredicto: NO-ARREGLADA. Decisión: CONFLICTO de alcance.**

Buy usa saldo real (`scripts/5_Mission/LFPG_BTCHelper.c:1160`). Las otras cinco operaciones siguen enviando cero: Sell (`scripts/5_Mission/LFPG_BTCHelper.c:1640`), Withdraw (`scripts/5_Mission/LFPG_BTCHelper.c:2127`), Deposit (`scripts/5_Mission/LFPG_BTCHelper.c:2294`), WithdrawCash (`scripts/5_Mission/LFPG_BTCHelper.c:2515`) y DepositCash (`scripts/5_Mission/LFPG_BTCHelper.c:2697`).

La rama dejó pendiente esta ficha. BTCHelper no aparece en su diff y no entra en lista blanca. Se descarta ampliarla o cambiar preserveDisplay aquí. In-game: stock 42/saldo 137, cap+1 en seis operaciones; todas deben informar 42/137 sin alterar ledger. Provider null no debe provocar dereference.

### LFPG-UI-01 — Color de fallo del sorter

**Veredicto: NO-APLICA. Decisión: OBSOLETO: «muere con la V3».**

Sin cambios V3. V4 usa FAILED tanto en comparador (`scripts/4_World/test/LFPG_SorterController_TEST.c:740`) como en ACK (`scripts/4_World/test/LFPG_SorterController_TEST.c:797`; `scripts/4_World/test/LFPG_SorterController_TEST.c:818`), con color rojo. No se encontró la discordancia en V4.

Se descarta rescatar cambios cosméticos del fichero a eliminar. In-game: save/sort fallidos con texto/dot rojos, éxito verde, progreso ámbar. No se ejecutó UI.

### Extra — LFPG_VERSION_STR

**Veredicto: NO-APLICA. Decisión: YA-RESUELTO.**

Ya es 1.2.4 (`scripts/3_Game/LFPG_Defines.c:527`). En ese fichero solo se añaden constantes de V3-09. Se descarta tocar otra vez versión por cercanía al hunk. El receptor deberá contrastar versión/hash del PBO desplegado al arrancar.

## Comprobaciones ejecutadas y entrega

- `python -B .github/tools/enforce_checks.py --root . --strict`: 273 textos, 154 Enforce, FAIL=0/WARN=0. Barrido offline, no compilación.
- Delta: 12 scripts permitidos, 202 líneas añadidas; CRLF sin bare LF; directivas de preprocesador iguales a HEAD; sin operadores prohibidos, Print ni ref fuera de miembros en líneas añadidas; tabs en indentación nueva.
- `git -c core.whitespace=cr-at-eol diff --check`: sin incidencias. Sin esa opción, Git considera el CR de dos blobs trailing whitespace; se preserva CRLF y no se modifica configuración.
- `python -B C:/Users/guill/DayZ-Modding-Knowledge-Pack/tools/dayz-script-validator/scripts/script_validator.py .`: exit 2/WARN, 271 ficheros, cero errores y 52 warnings. Reparto: 42 ES-EMPTY-IFDEF-UNSUPPORTED-PATTERN, 7 ES-GETTYPE-EXACT-MATCH, 3 ES-CTX-READ-UNCHECKED. En ficheros modificados solo seis avisos del primer tipo, en NetworkManagerImpl, sobre directivas existentes cuya secuencia se conservó. Son límites de análisis, no prueba de compilación. Los otros avisos quedan en archivos no modificados. El baseline previo también terminó WARN sin errores.
- APIs y productores citados leídos en código actual; IsInVehicle contrastado con snapshot vanilla local. No se compila, empaqueta ni arranca juego/servidor. No se escriben tests tautológicos para sustituir el motor.
- Sin commit/staging por prohibición. No se actualiza memoria/handoff fuera del workspace: la frontera de escritura lo prohíbe. Este informe es el handoff durable; auxiliares preexistentes del orquestador no se editaron.

## LO QUE NO PUDE VERIFICAR

- Compilación Game/World/Mission: Enforce solo compila al cargar mundo; no se invocó un build ficticio.
- Dos clientes, CCTV/searchlight, expiración, latencia y pérdida de paquetes del heartbeat.
- Orden real de registro/restauración ATM respecto a super.OnInit; duplicados todavía sin cargar no son detectables.
- Kill/restart, fallos parciales CopyFile/SaveFile, saves hive y recovery de los siete conflictos.
- Weak references nativas en despawn/borrado, reinicio de proceso y coste del barrido safety.
- Color/render V4, fuga de preview en ejecución y contenido/hash del PBO desplegado.
- Integración con las otras lanes: no se consultaron sus worktrees ni se operó el juego compartido.

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

- El HEAD real es d59cad8, no 8de29d5. Retraso/recuentos históricos no son prueba de solapamientos actuales.
- «12 de 19 hechos» incluye una refutación sin código; siete nunca se implementaron; versión es un extra. La tabla lo distingue.
- V3-05 presupone que su validador acepta todo lo escrito: fold neto cero lo contradice. Tombstones E16 añaden otra incompatibilidad. Bloquear datos legítimos no equivale a arreglar el producto.
- `.saving` no resuelve V3-06, y sustituirlo por el protocolo antiguo tampoco es seguro. Requiere integrar ambos y probar abort/restart.
- «Muere con la V3» aplica al controller, pero no al handler compartido de preview. V4 conserva V3-11 y necesita arreglo de su lane.
- V3-01/V3-04 ya tienen corregidos mecanismos concretos. No implica que el objetivo más amplio journal/hive histórico esté demostrado.
- El port donante de V3-09 renovaba deadline en cada EXIT antes de validar payload. Se adapta a deadline no creciente para no mantener el lock mediante tráfico inválido.
- err14 solo está corregido en Buy. Cinco rutas siguen enviando saldo cero; estar fuera de lista blanca no las convierte en obsoletas.
