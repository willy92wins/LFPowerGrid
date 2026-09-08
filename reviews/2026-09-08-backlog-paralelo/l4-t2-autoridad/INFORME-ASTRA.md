# INFORME ? Lane 4 ? T2 Autoridad de servidor

Fecha: 2026-09-08. Base comprobada: `8de29d5`. Referencia archivada: `archive/t2-autoridad-servidor`, commit `8039fde`.

Se han corregido las cuatro fichas en el c?digo. **ARREGLADA significa implementaci?n escrita y revisada est?ticamente; no certifica compilaci?n ni cierre in-game. SEC01 queda expresamente pendiente del recuento real de entregas.** No se ha arrancado DayZ ni invocado un build.

## Alcance, decisiones y referencia anterior

- S?lo se modifican los dos scripts autorizados y se crea este `INFORME.md`, exigido expresamente por el brief. No hay commit ni cambios del ?ndice. Los archivos no versionados que ya estaban presentes (`BRIEF.md`, `EXIT.start`, `events.jsonl`, `stderr.log`) se dejan ajenos a esta entrega.
- En NetworkManager los cambios se limitan a los destinatarios de sincronizaci?n: l?neas originales 2150?3027. No se modifican las zonas compartidas 6150?6600 ni 7600?7750. La indicaci?n ?alrededor de 2150? se interpreta junto con la obligaci?n expresa de localizar todos los env?os afectados, no como permiso para tocar otros subsistemas del manager.
- No existe `CLAUDE.md` en este worktree ni en el checkout can?nico identificado por `git rev-parse --git-common-dir`. El brief es el plan aprobado. Se leyeron las skills de workflow/Enforce y el checklist de DayZ; las prohibiciones particulares de este brief prevalecen.
- Antes de editar se consultaron el log y el diff de la rama archivada, su README y la revisi?n de ronda 3. Se reutilizan las ideas de enumerar puertos del servidor, comprobar propiedad antes de mutar, conservar referencias a los conflictos, asegurar los destinatarios y conservar el inter?s de los extremos retirados.
- **No se ha fusionado ni copiado ?ntegramente la transacci?n archivada.** Exig?a cambiar `LFPG_ElecGraphImpl.c`, a?adir `LFPG_FinishWiringTxn.c` y modificar zonas prohibidas del manager. Adem?s, su README documenta `R-T2-AGUA`: el helper de alta refrescaba aspersores incluso al rechazar la arista. Tambi?n documenta la p?rdida de invalidaciones de owners vivos durante una poda.
- Se elige una reserva de arista con los cables antiguos intactos, seguida del alta real en el almac?n y despu?s las bajas. Si falla el almac?n, se retira ?nicamente la arista provisional. No hay que reconstruir los cables antiguos porque nunca se retiraron. Se conservan las APIs de almacenamiento actuales y sus l?mites.
- **Coste conservador expl?cito:** el reemplazo exige espacio temporal en almac?n y grafo. Puede rechazarse aunque cupiera despu?s de borrar el cable viejo. Tampoco se convierte el reenv?o de la misma conexi?n exacta en actualizaci?n de geometr?a. Se prefiere el rechazo ?ntegro a cambiar l?mites o retirar primero datos persistentes. No hay cambio de formato de persistencia ni migraci?n.

## Mapa de autoridad

| Dato | Cliente | Servidor | Frontera |
|---|---|---|---|
| NetworkID, nombres de puerto y waypoints solicitados | Los propone | Resuelve entidades y valida | RPC FINISH_WIRING |
| Cat?logo de puertos y direcci?n | Lo usa para selecci?n visual | Decide admisi?n | DeviceAPI sobre entidad resuelta |
| Propietario del cable y AllowCutOthersWires | No decide permisos | Usa identidad del remitente y ajustes locales | CanCreatorCutWire |
| Almacenes, ?ndices y grafo | Recibe representaci?n | Mantiene estado autoritativo | Mutaci?n s?ncrona en 5_Mission |
| Snapshot/delta y destinatario | Consume cach? de cables | Filtra proximidad e identidad | ScriptRPC con recipient expl?cito |

Se mantiene `GetPlainId()` para la propiedad porque es el formato ya usado por creaci?n, corte y contadores. Cambiarlo s?lo aqu? romper?a la comparaci?n con cables existentes; no se registra ni exporta ninguna identidad real.

### SEC20 ? Validaci?n de puertos y mutaci?n antes de difusi?n

**Veredicto: ARREGLADA.**

Cambios y evidencia del c?digo modificado:

- `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:398` y `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:403`: validaci?n de ambos extremos antes de crear/registrar IDs y antes del pre-connect. La longitud deja de ser la ?nica defensa del camino vanilla.
- `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:685`: el helper rechaza nombre vac?o, exige `HasPort` para dispositivos nativos y compara nombre/direcci?n exactos con `GetPortCount/GetPortName/GetPortDir` para el camino sin ID nativo. No admite sufijos inventados, ?ndices extra ni direcci?n inversa. `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:408` mantiene `CanConnectTo` para fuentes nativas.
- `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:805`: comprueba las capacidades de adyacencia y duplicados antes de que `OnWireAdded` pueda crear nodos. Esto cubre en este camino el rechazo tard?o por l?mite de aristas que motiv? cambios adicionales en el archivo archivado del grafo.
- `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:595`: reserva mediante `graph.OnWireAdded`. Si devuelve false, a?n no se ha almacenado ni retirado ning?n cable. Si el almacenamiento posterior falla, `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:613` deshace esa arista y cierra la mutaci?n/libera el puerto.
- `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:635` retira los conflictos del almac?n s?lo tras las dos altas satisfactorias; las notificaciones de baja llegan despu?s, en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:644`. Los env?os finales aparecen en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:674` y `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:859`.

Por qu? esta opci?n: `NotifyGraphWireAdded` en `scripts/5_Mission/LFPG_NetworkManagerImpl.c:991` llama al refresco de aspersores incluso cuando la inserci?n devuelve false. Se usa la API del grafo para reservar sin ese refresco; seguimiento de endpoints, refresco funcional y propagaci?n se realizan despu?s de terminar las bajas. Se descarta almacenar/difundir y luego pedir un rebuild: eso publica un estado que puede no existir y no devuelve las conexiones eliminadas.

Se descarta aceptar nombres vac?os del RPC como alias de cualquier puerto. Las filas legacy existentes siguen trat?ndose con la normalizaci?n de ?ndices anterior, pero las solicitudes nuevas deben nombrar el puerto. La enumeraci?n vanilla real devuelve `output_1`/`input_main` (`scripts/4_World/LFPG_IDevice.c:840`, `scripts/4_World/LFPG_IDevice.c:857`); el cliente actual obtiene el destino de esa enumeraci?n (`scripts/4_World/LFPG_WiringClient.c:698`).

Verificaci?n in-game necesaria: solicitudes LFPG/vanilla v?lidas y puertos inventados, vac?os, de direcci?n inversa y >32 caracteres; comprobar almac?n, aristas, ?ndices y ausencia de difusi?n en cada rechazo. Forzar l?mite global/de aristas y fallo de alta del almac?n: mismos cables antiguos y mismo orden, ninguna arista provisional al terminar. Con bomba/aspersores, comprobar tanto el rechazo como el reemplazo exitoso: el nuevo aspersor no se activa sin cable y el anterior desconectado se apaga. Verificar liberaci?n del lock con una segunda solicitud v?lida.

### SEC02 ? Pol?tica de corte aplicada al reemplazo

**Veredicto: ARREGLADA.**

- `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:573` inicializa `allowOthers=false`; s?lo los ajustes del servidor pueden permitir cortar cables ajenos.
- `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:760` recorre los almacenes nativos registrados y todos los owners vanilla; no conf?a exclusivamente en el ?ndice inverso para decidir si existen conflictos ajenos.
- `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:727` re?ne conflictos de salida y destino; una fila que coincide con ambos se recoge una sola vez. `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:733` usa `LFPG_WireHelper.CanCreatorCutWire` sobre cada fila y rechaza la operaci?n completa ante cualquier denegaci?n.
- La baja en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:635` opera ?nicamente sobre las filas ya autorizadas. El camino de reemplazo deja de usar el `RemoveWiresTargeting` cuyo argumento por defecto permit?a cortar a otros.

La pol?tica efectiva es la de `scripts/3_Game/LFPG_WireHelper.c:230`: identidad v?lida, cables propios o sin propietario, y cables ajenos s?lo con el ajuste activado. Se descarta simplemente pasar `allowOthers=false` a una eliminaci?n masiva: saltarse una fila ajena y seguir dando de alta la nueva conexi?n dejar?a ocupado el mismo puerto y podr?a borrar otros conflictos parcialmente.

Si no puede resolverse el owner de una fila afectada o su extremo retirado, se rechaza antes de mutar: no se promete una baja que no se pueda actualizar/publicar. La ruta legacy de `LFPG_Generator : PowerGenerator` se conserva; para propietarios que no heredan de `LFPG_WireOwnerBase` se aplica el `SetSynchDirty` ya utilizado por el c?digo existente (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:673`).

Verificaci?n in-game necesaria: dos jugadores A/B, ajuste desactivado/activado, propiedad propia/ajena/vac?a. Intentar reemplazo desde la misma salida, sobre el mismo destino desde otra fuente y con ambos conflictos a la vez, mezclando LFPG y vanilla. Con permiso denegado, no cambia ninguna fila, arista ni contador; con permiso concedido, queda una conexi?n y se actualizan ambos propietarios. Repetir con entrada legacy vac?a y un ?ndice inverso incompleto.

### SEC03 ? No perder la conexi?n anterior si no cabe la nueva

**Veredicto: ARREGLADA.**

`LFPG_DeviceAPI.AddDeviceWire` y `AddVanillaWire` se ejecutan en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:608` y `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:610`, antes de la primera eliminaci?n de una fila antigua (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:635`). Se comprueba su booleano. Ante false, s?lo se retira la arista provisional; los almacenes antiguos ni se vac?an ni se reconstruyen.

Se descarta duplicar aqu? una funci?n de predicci?n de capacidad que podr?a separarse de la admisi?n real. La alternativa archivada predec?a capacidad descontando conflictos, los retiraba y necesitaba snapshots completos y rollback de sus efectos. La soluci?n actual es m?s restrictiva en los l?mites, pero no necesita restaurar el orden de los almacenes tras un rechazo.

Eliminaci?n por ?ndices: se recorre la lista de conflictos en orden inverso; `array.Remove` intercambia con el ?ltimo elemento. Se comprob? con 8.191 combinaciones de ?ndices que la fila nueva a?adida al final sobrevive y desaparecen exactamente las identidades antiguas seleccionadas. En rechazo no se modifica el orden previo. En ?xito se mantiene la sem?ntica de eliminaci?n no ordenada que ya usaba el repositorio.

La generaci?n del propietario fuente aumenta una sola vez con `LFPG_AddWire`; tras retirar sus filas antiguas se invalida ?nicamente su cach? JSON (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:668`). Los dem?s propietarios hacen su commit una vez. Esto evita saltarse la generaci?n consecutiva que exige el cliente en `scripts/4_World/LFPG_CableRenderer.c:1812`.

Verificaci?n in-game necesaria: almac?n al l?mite configurable y al l?mite duro, conexi?n duplicada y rechazo del grafo; conservar cables, propietario y contadores originales. Control positivo con espacio: retirar el conflicto de salida y el de entrada, conservar filas no relacionadas y presentar el delta correcto. Repetir con `LFPG_Generator`, un owner de `LFPG_WireOwnerBase` y vanilla. Reiniciar el servidor tras ?xito/rechazo para contrastar persistencia. Un reemplazo a capacidad exacta debe rechazarse ?ntegramente: es la decisi?n conservadora expl?cita de esta entrega.

### SEC01 ? Destinatarios de sincronizaci?n

**Veredicto: ARREGLADA en c?digo; cierre in-game pendiente.**

Los nueve env?os vulnerables localizados por contenido son ocho del manager y uno de ajustes del handler. Cada uno captura `GetIdentity`, rechaza identidad nula y la pasa como cuarto argumento de `ScriptRPC.Send`:

| Camino | Cita del env?o modificado |
|---|---|
| Snapshot de owner con proximidad | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2153` |
| Snapshot pendiente/diferido con inter?s | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2344` |
| Delta de owner | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2491` |
| Snapshot vanilla con proximidad | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2627` |
| Snapshot vanilla unicast | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2674` |
| Owner de FullSync repartido | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2798` |
| Estado vac?o autoritativo | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:3002` |
| Blob de owner unicast | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:3047` |
| Ajustes de servidor | `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2045` |

El target objeto del RPC no cambia: selecciona d?nde se procesa el mensaje, no a qu? cliente se entrega. La definici?n local primaria est? en `C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts/3_game/gameplay.c:117`; su documentaci?n inmediatamente anterior especifica que recipient nulo entrega a todos. Se descarta tratar `noExclude` como un filtro o seguir usando null tras comprobar la identidad.

Al restringir env?os no debe perderse el observador del extremo retirado. Para el reemplazo vanilla, `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:745` conserva posiciones antiguas; `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:827` une esas posiciones con owner/extremos actuales y env?a el almac?n final una vez por jugador interesado. Usa unicast inmediato incluso durante FullSync; los env?os vanilla diferidos existentes serializan el almac?n vivo al ejecutarse. Para LFPG se conservan las filas retiradas en el delta y se utiliza el mecanismo existente de inter?s/diferimiento.

Verificaci?n in-game necesaria y obligatoria para cerrar SEC01: al menos tres clientes (solicitante/cercano/remoto), contar recepci?n por sub-ID para cada fila de la tabla. Un unicast llega exclusivamente a su destinatario; cada difusi?n filtrada entrega una copia por cliente seleccionado y cero al remoto. Repetir con identidad ausente/desconexi?n durante FullSync, estado vac?o, coalescencia de snapshots y reemplazo vanilla durante FullSync. A?adir observador junto al extremo antiguo y otro junto al nuevo: ambos reciben el estado final sin cable fantasma. Probar tambi?n CCTV, porque la asociaci?n entre cuerpo e identidad depende del runtime.

## Validaci?n ejecutada y evidencia de APIs

- `python -B C:/Users/guill/DayZ-Modding-Knowledge-Pack/tools/dayz-script-validator/scripts/script_validator.py .`: **exit 2, WARN**, cero errores y 52 advertencias (41 `ES-EMPTY-IFDEF-UNSUPPORTED-PATTERN`, 4 `ES-GETTYPE-EXACT-MATCH`, 7 `ES-CTX-READ-UNCHECKED`). Las advertencias de los archivos tocados se localizaron fuera de las l?neas modificadas; las restantes pertenecen a archivos intactos. No se corrige deuda ajena al brief.
- Pasada final del mismo validador sobre el handler tras preservar `LFPG_Generator`: exit 2, cero errores, cuatro `ES-CTX-READ-UNCHECKED` preexistentes en l?neas 1074, 1075, 1085 y 1086. Ninguna advertencia en l?neas a?adidas.
- Revisi?n propia de las l?neas nuevas/modificadas: sin operadores/procedimientos prohibidos; `ref` ?nicamente en miembros de las dos clases de estado de solicitud. Indentaci?n de l?neas nuevas con tabs. No se ha ejecutado el gate del receptor.
- `git -c core.whitespace=cr-at-eol diff --check`: exit 0. Sin esa opci?n Git interpreta los CR que el manager ya guardaba como whitespace; no se normaliz? el archivo para silenciarlo. Ambos scripts conservan CRLF, sin LF sueltos.
- Modelo independiente del borrado no ordenado: todos los subconjuntos de ?ndices para tama?os 0?12, 8.191 casos, contrastados con el conjunto de identidades supervivientes y la presencia de la nueva fila.
- Modelo de invariantes de transacci?n: 96 combinaciones de propietario propio/ajeno/vac?o, permiso, rechazo de grafo/almac?n y conflictos de origen/destino/ambos/ninguno. Rechazo conserva secuencia antigua y no publica; ?xito deja almac?n/grafo concordantes. Modelo de inter?s: owner, extremo antiguo, nuevo y solapamiento incluidos una vez; remoto excluido. **Son modelos Python en memoria, no ejecuci?n ni compilaci?n del c?digo Enforce.** No se a?adieron archivos de tests fuera de la lista blanca.

| API consumida | Definici?n le?da |
|---|---|
| Puertos nativos y vanilla | `scripts/4_World/LFPG_IDevice.c:497`, `:562`, `:820`, `:840`, `:857` |
| Alta y acceso al almac?n LFPG | `scripts/4_World/LFPG_IDevice.c:598`, `:639`; `scripts/4_World/LFPG_WireOwnerBase.c:175`; `scripts/3_Game/LFPG_WireHelper.c:153` |
| Almac?n legacy del generador | `scripts/4_World/LFPG_TestDevices.c:703`, `:708`, `:713` |
| Alta/lectura/enumeraci?n vanilla | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:869`, `:914`, `:944`, `:949` |
| Pol?tica de propiedad | `scripts/3_Game/LFPG_WireHelper.c:230` |
| Admisi?n y baja del grafo | `scripts/5_Mission/LFPG_ElecGraphImpl.c:425`, `:788`, `:1311` |
| Notificaciones e ?ndices | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:991`, `:1017`, `:1363`, `:1404`, `:1489` |
| Commit/cach?/generaci?n | `scripts/4_World/LFPG_WireOwnerBase.c:156`, `:167`, `:175`; consumidor `scripts/4_World/LFPG_CableRenderer.c:1812` |
| Remove no ordenado | `C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts/1_core/proto/enscript.c:463` |
| SetSynchDirty externo | `C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts/3_game/entities/entityai.c:3069` |

## Estado de entrega

Cambios sin commit y sin staging, por prohibici?n expresa del encargo. Este informe es el handoff durable dentro del workspace: no se actualiza Obsidian ni memoria externa porque la frontera de escritura lo proh?be. No se declara el tramo listo para desplegar sin la revisi?n cruzada y las comprobaciones in-game indicadas.

SHA-256 de los scripts finales:

- Handler: `6121bb030a30e771238519764681911c97af3fc1d86a9c4eebfc2b5b9b32f465`.
- NetworkManager: `f0e782466caa78c0f4d32a80e5f023685dfd256096a9aad97de95d1c7b27d8f3`.

## LO QUE NO PUDE VERIFICAR

- Compilaci?n real de 3_Game/4_World/5_Mission y carga de mundo: no hay compilador invocable ni juego disponible.
- SEC01: entregas efectivas, duplicados y exclusi?n de clientes remotos para los nueve caminos; es condici?n pendiente de cierre.
- Asociaci?n identidad/cuerpo durante CCTV, desconexi?n y FullSync en el motor.
- Estado funcional de bombas/aspersores y potencia tras ?xito o rechazo; los modelos no ejecutan notificaciones del motor.
- Persistencia tras reinicio, coherencia de contadores/?ndices bajo carga y presupuesto de escaneo completo con jugadores reales.
- Compatibilidad de clientes antiguos que env?en nombres vac?os: ahora se rechazan por decisi?n conservadora.
- Invalidaciones de extremos antiguos en otros caminos de corte vanilla: se conserva el inter?s en este reemplazo, no se reescribe todo el ciclo de cortes/poda/diferimiento.
- Estado ya corrupto u owners nativos ausentes del registro antes de la petici?n; este cambio no repara retrospectivamente datos ni elimina cables hist?ricos inv?lidos.

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

- ?S?lo longitud? describe el camino vanilla, no todo el handler: los dispositivos con ID nativo ya ten?an `HasPort` y `CanConnectTo`. Aplicar `HasPort` a vanilla sin su enumeraci?n real rechazar?a todos sus cables leg?timos.
- El recuento y ubicaci?n de SEC01 son imprecisos: hay ocho env?os afectados en el manager hasta la l?nea original 3027 y un noveno de ajustes en el handler. El env?o de refresco de sorter fuera de esa zona ya usa identidad y no se ha tocado.
- `BeginGraphMutation` s?lo aplaza limpieza de nodos; no es una transacci?n de almacenes, contadores, RPC ni aspersores. El archivo archivado ya demuestra que ?mover el broadcast al final? no basta. Por eso aqu? el alta provisional evita el helper que refresca agua y no se retiran cables antes de asegurar la nueva conexi?n.
- Los l?mites de fichero impiden incorporar literalmente el dise?o archivado. Esta soluci?n conserva seguridad con una restricci?n funcional: exige holgura temporal y puede negar reemplazos que cabr?an despu?s de borrar. Si se exige reemplazo al l?mite con ?xito, hace falta una transacci?n con admisi?n conjunta y reserva de capacidad m?s amplia que este parche; no debe presentarse esta entrega como equivalente a ese contrato.
- Quitar el broadcast accidental puede descubrir cables fantasma que quedaban ocultos por entregas globales. Se cubren los extremos retirados de este reemplazo, pero otros cortes vanilla mantienen su deuda. SEC01 corrige destinatarios; no acredita por s? sola la correcci?n de todas las invalidaciones ni que todos los conjuntos de inter?s existentes sean m?nimos.
- La exploraci?n autoritativa evita saltarse permisos por un ?ndice incompleto, pero cuesta recorrer owners/cables. El rate-limit existente acota peticiones por jugador; no hay medici?n de coste agregado aqu?. No ser?a riguroso afirmar ausencia de superficie de denegaci?n de servicio sin medirla.
- El gate de forma del receptor y un validador est?tico no prueban compilaci?n, entrega RPC ni rollback funcional. Ninguna cifra de modelos sustituye el control in-game que el propio encargo exige para SEC01.
