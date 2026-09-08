VERDE — 0 GRAVE / 2 MEDIO / 1 MENOR

Revisión adversarial de la lane T2 (autoridad de servidor) contra `BRIEF.md`, `CAMBIOS.diff` e `INFORME.md`. Código leído en el árbol ya modificado; no se ha compilado ni arrancado el juego.

### MEDIO — El reemplazo al techo de almacén o de aristas se deniega entero

`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:608` y `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:811`

La ficha SEC03 pedía no borrar la conexión vieja hasta poder almacenar la nueva. Eso está cerrado: `AddDeviceWire` / `AddVanillaWire` ocurren antes de cualquier `array.Remove`, y si fallan se retira solo la arista reservada (`:613`) y los almacenes antiguos no se tocan.

La comprobación de «cabe» es bruta, no neta. `LFPG_WireHelper.AddWire` y `FinishWiringGraphAllows` cuentan las filas/aristas todavía ocupadas por los conflictos. Un reemplazo que cabría después de borrar (almacén al máximo configurable, o `LFPG_MAX_EDGES_PER_NODE` ya saturado por la arista que se sustituye) se rechaza. El código anterior sí sustituía en ese techo porque borraba primero; ahora el jugador se queda con el cable viejo en vez de perderlo, pero también en vez de sustituirlo.

Importa porque el informe lo presenta como coste conservador consciente, y lo es: no reabre la pérdida. Queda un reemplazo al límite que el servidor niega. Habría que reservar capacidad descontando las filas ya autorizadas a salir, o admitir de forma conjunta, sin borrar antes de tener el alta asegurada.

### MEDIO — El barrido autoritativo acopla cada cableado a todo el registro

`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:760`, `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:777` y `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:790`

`FinishWiringCollect` recorre todos los nativos con almacén y todas las claves vanilla antes de mutar, y aplica `CanCreatorCutWire` a cada conflicto. Eso cierra SEC02 de verdad: ya no se llama a `RemoveWiresTargeting` con el `allowOthers=true` por defecto (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:1552`).

El mismo barrido es frágil. Si cualquier dispositivo nativo registrado tiene `HasWireStore` y `GetDeviceId()` vacío, la petición entera vuelve false (`:777`). Si cualquier clave del mapa vanilla resuelve a un objeto con almacén nativo, también (`:790`). Un solo registro sucio deja de ser un agujero de permiso y pasa a denegar **todo** el `FinishWiring` del servidor, con el mensaje genérico de no poder reemplazar. El coste es lineal en owners×cables por RPC; el rate-limit por jugador acota el abuso, no el coste agregado.

Importa porque el fail-closed del encargo está bien elegido para permisos, y mal acoplado a la higiene global del registro. Habría que fallar cerrado sobre el owner o la clave concreta, no abortar el cableado del resto del mundo, y no tratar un ID vanilla nativo como veneno global.

### MENOR — Las denegaciones nuevas de puerto ya no dejan rastro en el log

`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:398`

El bloque antiguo de `HasPort` / `CanConnectTo` escribía `LFPG_Util.Warn` con el puerto y el motivo. El nuevo `FinishWiringPortAllowed` responde al cliente y vuelve, sin Warn. El rechazo existe; el forense de un cliente modificado que inventa nombres queda más ciego. Habría que volver a dejar constancia en log en las tres salidas de puerto/dirección/CanConnectTo, sin relajar el rechazo.

## Fichas

| Ficha | Implementador | Revisor |
|---|---|---|
| SEC20 — puerto en servidor + mutación antes de difundir, con rollback | ARREGLADA | ARREGLADA |
| SEC02 — AllowCutOthersWires en el reemplazo | ARREGLADA | ARREGLADA |
| SEC03 — no perder la conexión vieja si la nueva no cabe | ARREGLADA | ARREGLADA, con holgura (MEDIO) |
| SEC01 — `.Send(` unicast / proximidad con destinatario | ARREGLADA en código; cierre in-game pendiente | ARREGLADA en código; cierre in-game pendiente |

SEC20: `FinishWiringPortAllowed` (`:685`) rechaza nombre vacío, exige `HasPort` en nativos y nombra/dirige el catálogo vanilla (`GetPortCount` / `GetPortName` / `GetPortDir`). Eso cubre el salto que el brief describía: `GetDeviceId()` vacío en vanilla ya no evita la validación. `CanConnectTo` sigue solo en fuente nativa (`:408`), igual que antes, y es coherente porque el dispatch vanilla devolvería false. La mutación reserva con `graph.OnWireAdded` (`:595`), almacena, y solo entonces borra y publica (`:656`–`:680`). Si el alta falla, `OnWireRemoved` de la arista nueva y nada de difusión de cables. Evitan `NotifyGraphWireAdded` (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:991`), que refresca aspersores incluso cuando la inserción devuelve false.

SEC02: `allowOthers` nace en false y solo pasa a true con el ajuste (`:573`). Cada conflicto, de salida o de destino, pasa por `LFPG_WireHelper.CanCreatorCutWire` (`:733`); una denegación aborta antes de mutar. El camino de corte sigue usando la misma política; el de reemplazo ya no usa el default permisivo de `RemoveWiresTargeting`.

SEC03: el orden store-antes-de-borrar está en el código, no solo en el informe. El rollback de grafo no reconstruye cables viejos porque nunca salieron. La holgura temporal es el MEDIO de arriba, no una recaída de pérdida de datos.

SEC01: en el manager quedaban ocho `.Send(` con `null` / `noExclude` (aprox. original 2150, 2337, 2481, 2617, 2662, 2779, 2981, 3025). Ahora van a `GetIdentity()` y se saltan el envío si falta identidad: `2153`, `2344`, `2491`, `2627`, `2674`, `2798`, `3002`, `3047`. El noveno del informe es ajustes en el handler (`:2045`), que el brief no citó y está en lista blanca. El refresco de sorter en `:6381` ya usaba `pid` y no se ha tocado, dentro de la zona que otras lanes editan. El primer argumento de `Send` sigue siendo el objeto de proceso; el cuarto es el destinatario. Eso coincide con la lectura del implementador. El recuento in-game de entregas no se ha hecho; el brief lo exige para cerrar la ficha y el informe no finge haberlo hecho.

## Alcance, convenciones y formato

`CAMBIOS.diff` toca solo `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c` y `scripts/5_Mission/LFPG_NetworkManagerImpl.c`. Lista blanca respetada. En el manager los hunks están entre ~2147 y ~3047: zona de `.Send(` de sincronización, no las franjas ~6150–6600 ni ~7600–7750.

En las líneas añadidas no hay ternarios, `++`/`--`, `+=`/`-=`, `foreach`, `Print(` ni `ref` en parámetros, retornos o locales. El `ref` nuevo está en miembros de `LFPG_FinishWiringOwner` y `LFPG_FinishWiringState` (`:3087`–`:3099`). Las llamadas nuevas caben en una línea.

No hay reformateo encubierto del manager: el diff es el destinatario de `Send`. El handler sustituye el bloque de reemplazo; no es un reindentado del fichero. Las líneas nuevas van con tabuladores dentro de un fichero que indentaba con espacios; no es normalización CRLF del árbol.

## Honestidad del informe

`LO QUE NO PUDE VERIFICAR` no es relleno: compilación, entregas SEC01, CCTV, aspersores, persistencia tras reinicio y clientes que mandaban puerto vacío son huecos reales. Las citas `path:line` del código modificado que el informe da para las cuatro fichas casan al abrirlas (`:398`, `:573`, `:595`, `:608`, `:635`, `:685`, `:733`, `:760`, `:805`, `:859`, manager `:2153`–`:3047`, handler `:2045`). `NotifyGraphWireAdded` en `LFPG_NetworkManagerImpl.c:991`, `CanCreatorCutWire` en `LFPG_WireHelper.c:230` y la generación consecutiva en `LFPG_CableRenderer.c:1812` también están donde dice.

No trato como falso el SHA-256 ni el exit del validador Python: no los he vuelto a ejecutar. El informe no declara «compila» ni «SEC01 cerrado in-game».

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

«Solo longitud» era el camino vanilla, no todo el handler. Un nativo con ID ya tenía `HasPort` y `CanConnectTo`; el salto explotable era `GetDeviceId()==""`, que en este árbol es la marca de vanilla (`LFPG_DeviceAPI.GetDeviceId` no escribe ID en el objeto vanilla; `GetOrCreateDeviceId` calcula `vp:…` y se registra aparte). Validar vanilla con `HasPort` habría rechazado cables legítimos: `HasPort` hace dispatch a `LFPG_HasPort` y en vanilla devuelve false. El enumerado `GetPortName`/`GetPortDir` es el arreglo que la ficha pedía sin decirlo del todo.

SEC01 no eran «nueve `.Send(` en el manager ~2150». Había ocho con destinatario nulo en el manager, más ajustes en el handler, más un sorter que ya iba a identidad y vive fuera de la zona de esta lane. El FullSync que «valida identidad y luego envía null» sí existía (`LFPG_SendFullSyncOwner`): el objeto era el jugador que entra y el cuarto argumento era broadcast. Eso es fuga. Corregir el cuarto argumento es el arreglo; no hace falta cambiar el target objeto.

`BeginGraphMutation` no es una transacción de almacenes, contadores ni RPC. El archivo archivado y `NotifyGraphWireAdded` demuestran que «mover el broadcast al final» no basta si el helper de alta refresca agua al rechazar. Usar `OnWireAdded` crudo y publicar después es una lectura correcta de esa premisa rota, no un extra.

La lista blanca impide el diseño archivado (`LFPG_ElecGraphImpl.c`, helper de transacción, zonas ajenas del manager). Pedir reemplazo al límite **y** no salir de dos ficheros empuja a la holgura temporal. Si el producto necesita sustituir a capacidad exacta, el encargo no cabe en esta lane.

Quitar el broadcast accidental puede enseñar cables fantasma que el envío global tapaba. Esta lane cubre extremos retirados del reemplazo vanilla (`:745`, `:827`). No reescribe cortes, poda ni diferimiento. SEC01 no acredita conjuntos de interés mínimos en el resto del ciclo.

`GetPlainId()` como clave de propiedad es el formato que ya usan creación, corte y contadores. Cambiarlo solo aquí rompería la comparación. El brief no pedía rotar identidades.

## LO QUE NO PUDE COMPROBAR

- Compilación real de 3_Game / 4_World / 5_Mission y carga de mundo: no hay compilador invocable. Las clases `LFPG_FinishWiringOwner` / `LFPG_FinishWiringState` van al final del mismo `.c` que las usa; el resto del módulo suele declarar helpers antes, pero Enforce en este repo ya usa tipos no-Managed (`LFPG_WireData`, `LFPG_OwnerBroadcastSnapshot`) guardados en `ref array<ref …>`. No puedo certificar el orden de declaración.
- Entregas RPC por cliente (SEC01): sin juego. El código deja de pasar `null` como destinatario; no he contado copias reales, duplicados ni exclusión de remotos.
- Asociación cuerpo/identidad en CCTV, desconexión a mitad de FullSync, y si un `continue`/`return` al faltar identidad deja un blob de FullSync sin enviar mientras el cursor avanza.
- Estado de bombas/aspersores tras éxito y tras rechazo: se evita el refresco de `NotifyGraphWireAdded` en el fallido, y en el éxito se llama `LFPG_RefreshPumpSprinklerLink` y `RequestPropagate` (`:651`–`:654`). No he ejecutado notificaciones del motor.
- Persistencia tras reinicio. `LFPG_AddWire` hace `SetSynchDirty` con las filas viejas todavía dentro; después se invalidan caché/commit. `LFPG_OnStoreSaveExtra` serializa `m_Wires` vivo. El siguiente save debería ver el array ya podado; no he visto un save síncrono en esa ventana.
- Los SHA-256 y el `script_validator.py` que cita el informe: el intento de hashear en esta sesión lo bloqueó el entorno. No contradigo esas cifras ni las doy por buenas.
- El modelo Python de `array.Remove` no ordenado: el algoritmo (índices de mayor a menor, fila nueva al final) encaja con el `Remove` que intercambia con el último, pero no he reproducido las 8.191 combinaciones.
- Clientes antiguos que enviaban puerto vacío: ahora se rechazan. No hay evidencia in-game de cuántos hay.
