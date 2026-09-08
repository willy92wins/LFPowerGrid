# Triaje P2/P3 — Red/seguridad

| ID | Veredicto | Fichero principal | Por qué |
|---|---|---|---|
| SEC03 | MUERTA | `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:604` | Corregida hoy: se almacena el cable nuevo antes de borrar los anteriores; si falla, retorna sin borrarlos. |
| SEC04 | VIVA | `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1873` | El corte IN invoca incondicionalmente el rescate que recorre todos los propietarios y sus cables. |
| SEC05 | VIVA | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:739` | Cada intento sobre el límite genera un aviso; varios handlers añaden una respuesta RPC por rechazo. |
| SEC06 | VIVA | `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2238` | Cada lote admitido vuelve a marcar las entidades; la deduplicación solo dura ese lote. |
| SEC07 | VIVA | `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:311` | Un ID recibido pasa al aviso de waypoints sin saneamiento; strings y arrays se limitan después de leerlos. |
| SEC08 | VIVA | `scripts/5_Mission/LFPG_ControlSessionRegistry.c:404` | El tick no comprueba distancia y la sesión del foco nace sin plazo; alejarse sin enviar AIM/EXIT mantiene la reserva. |
| SEC10 | VIVA | `scripts/3_Game/LFPG_FileUtil.c:339` | La existencia del target basta para cerrar el fallback; sus consumidores no prueban el backup tras fallar el parseo. |
| SEC11 | VIVA | `scripts/3_Game/LFPG_Settings.c:538` | Se carga sobre `s_Settings`; el fallo solo emite un aviso y no restablece ni valida defaults. |
| SEC12 | VIVA | `scripts/5_Mission/LFPG_BTCSessionRegistry.c:375` | Ambos caminos de cierre expulsan con `Remove(0)`, que mueve el último elemento y rompe la ventana FIFO. |
| SEC13 | VIVA | `scripts/5_Mission/LFPG_BTCSessionRegistry.c:341` | Se acepta un salto al límite y se guarda como máximo; ninguna secuencia nueva posterior puede admitirse. |
| SEC14 | VIVA | `scripts/5_Mission/LFPG_BTCSessionRegistry.c:137` | El registro BTC y el mapa estático de avisos crecen por UID sin retirada; los limitadores generales sí tienen purga. |
| SEC15 | VIVA | `scripts/4_World/LFPG_RPCGuard.c:10` | Hay ocho políticas, pero las únicas llamadas a `Admit` y `Authorize` están en el inspector. |
| SEC16 | VIVA | `scripts/3_Game/LFPG_FileUtil.c:81` | Los tres guardados conservan pasos de copia/rotación duplicados, aunque saldos ya tiene un protocolo adicional propio. |
| SEC17 | VIVA | `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:465` | `CanPreConnect` recorre la geometría y el handler vuelve a calcular posiciones y a validarla. |
| SEC18 | VIVA | `scripts/3_Game/LFPG_Migrators.c:40` | Las entradas de migración carecen de llamadores y la cabecera atribuye una llamada que el deserializador ya retiró. |
| SEC19 | VIVA | `scripts/3_Game/LFPG_FileUtil.c:761` | Lee hasta EOF concatenando y después obtiene una subcadena extensa para buscar la versión. |

**Resultado: 15 VIVAS, 1 MUERTA, 0 DUDOSAS y 0 NO-LOCALIZADAS.** Revisión estática del 8 de septiembre de 2026, sobre `d59cad892557d8ec8dcfed0bfca0ba1c5744db45`. Los números de línea corresponden al workspace actual, no a los informes antiguos. VIVA acredita el mecanismo indicado, no una reproducción en DayZ ni una medición de su gravedad.

Se siguieron las rutas de entrada, helpers, consumidores y limpiezas de estado. El historial se usó únicamente para fechar la corrección ya comprobada de SEC03. No se encontró `CLAUDE.md` en el árbol del proyecto ni en los dos directorios superiores consultados; el brief rige el alcance. No se modificó código ni se ejecutaron acciones Git de escritura.

## EVIDENCIA POR FICHA

### SEC03 — MUERTA: el rechazo de almacenamiento conserva las conexiones previas

En `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:595` se intenta reservar la nueva arista. Las llamadas a `AddDeviceWire` y `AddVanillaWire` están en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:608` y `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:610`; si devuelven falso, la rama de `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:611` retira la reserva, libera el bloqueo y retorna. El borrado de los cables anteriores llega después, en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:635`.

También se leyeron las implementaciones: `scripts/4_World/LFPG_IDevice.c:598` enruta al propietario; `scripts/4_World/LFPG_WireOwnerBase.c:195` y `scripts/4_World/LFPG_TestDevices.c:730` usan el helper, que rechaza capacidad/duplicados antes de insertar (`scripts/3_Game/LFPG_WireHelper.c:164`, `scripts/3_Game/LFPG_WireHelper.c:185`). La variante vanilla hace lo mismo en `scripts/5_Mission/LFPG_NetworkManagerImpl.c:891` y `scripts/5_Mission/LFPG_NetworkManagerImpl.c:904`.

**Murió hoy**, en `889d4d94c4d6935985ac75c4ccc53785f957d843`, del 08-09-2026 a las 03:12:05 +0200. Se contrastó el cuerpo anterior: eliminaba conexiones entrantes mediante `RemoveWiresTargeting` antes del bloque que almacenaba la nueva. La fecha por sí sola no fundamenta el veredicto.

**Límite:** el arreglo exige holgura temporal; puede rechazar un reemplazo que cabría después de borrar. Eso es una limitación de admisión, no la pérdida de conexiones descrita por SEC03 (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:605`). Comparte handler con SEC04, SEC05, SEC06, SEC07, SEC08, SEC15 y SEC17; comparte `HandleFinishWiring` con SEC05, SEC07 y SEC17.

### SEC04 — VIVA: rescate global en cada corte IN admitido

**Evidencia:** `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1872` consulta el índice y la línea siguiente llama a `RescueStaleIncomingWires` sin condicionar la llamada a cambios ni a una señal de índice inválido. El rescate obtiene todo el registro en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1925`, recorre sus propietarios y cables en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1927` y `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1942`, y hace otra pasada por propietarios vanilla en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1977`.

El defecto sigue aunque `RemoveWiresTargeting` ya sea indexado y retorne inmediatamente cuando no encuentra propietarios (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:1569`). Un corte IN válido que no elimina nada sigue pagando el rescate global; el rebuild posterior, en cambio, sí depende de `changed` (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1887`).

**Coste: AMPLIO.** Evitar el barrido conservando el rescate ante índices incompletos requiere coordinar su validez con los caminos de mutación. Saltárselo solo porque hubo un acierto en el índice perdería propietarios omitidos, precisamente el caso que protege el comentario actual.

**Solapamiento:** mismo `LFPG_RPCServerHandlerImpl.c` que SEC03/05/06/07/08/15/17; `HandleCutPort` comparte el rechazo de SEC05. El índice y su mantenimiento están en `LFPG_NetworkManagerImpl.c`, también afectado por SEC05/14/17.

### SEC05 — VIVA: el rechazo también produce spam

**Evidencia:** `scripts/5_Mission/LFPG_NetworkManagerImpl.c:739` entra en el rechazo de ventana y llama a `LFPG_Util.Warn` en `scripts/5_Mission/LFPG_NetworkManagerImpl.c:743` en cada intento excedido. `HandleFinishWiring` añade otro aviso y mensaje al cliente en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:230`; `HandleCutPort` también responde en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1712`.

`LFPG_Util.Warn` termina en `Print` sin limitación propia (`scripts/3_Game/LFPG_Util.c:7`, `scripts/3_Game/LFPG_Util.c:15`), y `LFPG_SendClientMsg` construye y envía un RPC fiable (`scripts/4_World/LFPG_PlayerRPC.c:114`). Con el logging actual habilitado y nivel 1 (`scripts/3_Game/LFPG_Defines.c:399`), aumentar los intentos rechazados aumenta avisos y, en esos handlers, envíos. Esto no demuestra una tasa concreta de saturación.

**Coste: AMPLIO.** Limitar avisos en el manager y respuestas/avisos en los handlers; modificar solo el mensaje del handler deja activo el aviso interno de la ventana.

**Solapamiento:** manager con SEC04/14/17; handler con SEC03/04/06/07/08/15/17; `LFPG_Util` con SEC07/14. No se atribuye este fallo a todo rechazo BTC: `scripts/5_Mission/LFPG_BTCHelper.c:320` sí limita las respuestas de nonce.

### SEC06 — VIVA: la deduplicación no cruza lotes

**Evidencia:** `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2172` impone el límite general y `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2178` limita el lote a 64 entradas. Sin embargo, `sentDeviceIds` se crea de nuevo en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2213`; por cada entidad distinta autorizada se llama a `SetSynchDirty` en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2238`, sin comprobar una resincronización previa o cambio de estado entre solicitudes.

Un cliente que repita el mismo lote dentro del ritmo admitido vuelve a forzar el marcado. La autorización de objeto y proximidad existe (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2076`). El cliente normal comprueba generaciones y necesidad (`scripts/4_World/LFPG_CableRenderer.c:1505`, `scripts/4_World/LFPG_CableRenderer.c:1589`), pero esa conducta no constituye una restricción del servidor.

**Coste: ACOTADO.** Acotar/coalescer el marcado entre lotes en el handler, manteniendo la reentrega inicial necesaria para entrada en la burbuja/JIP. Si se introduce un nuevo contrato de generación en el mensaje, el coste pasa a AMPLIO; no es imprescindible para un límite servidor conservador.

**Solapamiento:** mismo handler que SEC03/04/05/07/08/15/17; mismo `HandleRequestDeviceSyncBatch` que los límites tras lectura de SEC07. No se ha medido cuántos paquetes produce cada marcado: el motor puede coalescer replicaciones.

### SEC07 — VIVA: texto no saneado y límites posteriores a la lectura

**Evidencia activa con el logging actual:** el servidor lee `dstDeviceId` en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:268`, comprueba únicamente longitud en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:278` y lo pasa como `wireId` a `ValidateWaypoints` en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:311`. Un waypoint fuera de rango hace que `scripts/3_Game/LFPG_WireHelper.c:121` concatene ese string sin escape en un `Warn`, que llega a `Print` en `scripts/3_Game/LFPG_Util.c:11`. No hace falta que se resuelvan los objetos: esa validación es anterior a la resolución de NetworkID.

Además, el array completo se lee en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:289` y solo después se limita su cantidad en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:304`. Los límites de longitud también siguen a `ctx.Read`; el ID de compatibilidad del lote se lee sin límite de longitud del mod (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2193`). La carga concreta que permite el transporte no se ha verificado.

**Coste: AMPLIO.** Saneamiento de registros en los productores/helpers y revisión del contrato de lectura para limitar antes de materializar colecciones, si la API disponible lo permite. Escapar logs no corrige por sí solo el coste de deserialización.

**Solapamiento:** `HandleFinishWiring` con SEC03/05/17; lote con SEC06; `LFPG_Util` con SEC05/14. Matiz: los `Debug` del payload no imprimen a nivel 1 y `DIAG_CLIENT_LOG` ya llama a un saneador (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2275`). Esos caminos no se usan para demostrar la ficha.

### SEC08 — VIVA: reserva del foco sin comprobación periódica de distancia

**Evidencia:** `BeginSearchlight` deja `m_DeadlineMs = 0` en `scripts/5_Mission/LFPG_ControlSessionRegistry.c:136`. El tick comprueba presencia, vida, inconsciencia, alimentación y coincidencia de operador, pero no distancia (`scripts/5_Mission/LFPG_ControlSessionRegistry.c:391`, `scripts/5_Mission/LFPG_ControlSessionRegistry.c:404`); el plazo solo se aplica si es mayor que cero (`scripts/5_Mission/LFPG_ControlSessionRegistry.c:420`). El scheduler sí llama al tick (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:622`): no falta el llamador, falta esa condición.

La salida por distancia vive en `HandleSearchlightAim` (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1576`) y en el cliente (`scripts/4_World/LFPG_SearchlightController.c:240`). Un operador vivo que se aleje omitiendo AIM/EXIT mantiene el bloqueo mientras persistan las demás condiciones; otro jugador es rechazado en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1460`. `LFPG_HasOperator` solo limpia si desaparece la entidad (`scripts/4_World/LFPG_Searchlight.c:686`).

**Coste: ACOTADO.** Añadir la comprobación espacial autoritativa al tick del registro y usar el cierre existente, que libera el operador en `scripts/5_Mission/LFPG_ControlSessionRegistry.c:318`.

**Solapamiento:** handler con SEC03/04/05/06/07/15/17. El registro de control es distinto del registro BTC de SEC12/13/14. La ficha es un fallo frente a un cliente que omite mensajes; no afirma que el cliente normal deje la reserva al alejarse.

### SEC10 — VIVA: target ilegible bloquea el fallback a backup

**Evidencia:** `EnsureFileOrRestore` devuelve verdadero por mera existencia en `scripts/3_Game/LFPG_FileUtil.c:339`. Los wrappers de cables y settings solo parsean el `.tmp` antes de delegar (`scripts/3_Game/LFPG_FileUtil.c:707`, `scripts/3_Game/LFPG_FileUtil.c:745`); el de saldos también termina en el fallback (`scripts/3_Game/LFPG_FileUtil.c:888`). Caso suficiente: target con JSON inválido, backup válido y ningún `.tmp`.

Los consumidores cierran el fallo sin probar el backup: cables retorna en `scripts/5_Mission/LFPG_NetworkManagerImpl.c:4616`; settings solo avisa en `scripts/3_Game/LFPG_Settings.c:540`; saldos preserva evidencia y bloquea guardados en `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1996` y `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:2026`. El resultado es recuperación automática omitida y estado no cargado, aunque exista una copia utilizable. La protección de saldos evita equiparar esto a sobrescritura definitiva demostrada.

**Coste: AMPLIO.** Selección y validación tipada de candidatos coordinada con sus consumidores. Debe conservarse la protección de versiones futuras y el contrato de `.tmp` abortado de saldos; no basta con copiar cualquier backup sobre todo fallo.

**Solapamiento:** `LFPG_FileUtil.c` con SEC16/19; `LFPG_Settings.Load` con SEC11; carga de saldos con SEC19; manager con SEC04/05/14/17.

### SEC11 — VIVA: Load no cumple su promesa de volver a defaults

**Evidencia:** el singleton se crea solo si es nulo (`scripts/3_Game/LFPG_Settings.c:520`), se pasa directamente como salida de `LoadFile` (`scripts/3_Game/LFPG_Settings.c:538`) y, al fallar, solo se imprime “using defaults” (`scripts/3_Game/LFPG_Settings.c:540`). La validación está exclusivamente en la rama de éxito (`scripts/3_Game/LFPG_Settings.c:545`); al final se construye el índice de blacklist igualmente (`scripts/3_Game/LFPG_Settings.c:597`).

Está demostrada la ausencia de staging/restablecimiento y la exposición directa del singleton. No se afirma como hecho que cualquier JSON inválido lo modifique parcialmente: depende del serializador nativo y del tipo de fallo. El wrapper oficial pasa `data` directamente a `ReadFromString` y retorna falso sin rollback, en `scripts/3_game/tools/jsonfileloader.c:26` de [DayZ Script Diff de Bohemia](https://github.com/BohemiaInteractive/DayZ-Script-Diff/blob/main/scripts/3_game/tools/jsonfileloader.c#L7-L39). Eso tampoco revela las garantías internas del serializador.

**Coste: ACOTADO.** Cargar en una instancia candidata, definir el resultado del fallo y publicar únicamente el estado aceptado dentro de `LFPG_Settings.c`.

**Solapamiento:** comparte exactamente `Load` con el consumidor de SEC10; el protocolo de guardado del mismo archivo participa en SEC16. VIVA se refiere a la omisión observable; la corrupción parcial sigue siendo potencial.

### SEC12 — VIVA: la caché de 64 terminales deja de ser FIFO

**Evidencia:** el límite es 64 (`scripts/5_Mission/LFPG_BTCSessionRegistry.c:13`); tanto el sellado de huérfanos como el cierre normal insertan al final y ejecutan `Remove(0)` (`scripts/5_Mission/LFPG_BTCSessionRegistry.c:246`, `scripts/5_Mission/LFPG_BTCSessionRegistry.c:249`, `scripts/5_Mission/LFPG_BTCSessionRegistry.c:372`, `scripts/5_Mission/LFPG_BTCSessionRegistry.c:375`). La definición oficial documenta que `array.Remove` rellena el hueco con el último elemento y no conserva orden: `scripts/1_core/proto/enscript.c:433`, declaración en línea 438, de [Bohemia, DayZ Script Diff](https://github.com/BohemiaInteractive/DayZ-Script-Diff/blob/main/scripts/1_core/proto/enscript.c#L432-L445).

Traza deducida de ese contrato: con terminales 1…64, añadir 65 deja `[65,2,…,64]`; añadir 66 elimina 65 y deja `[66,2,…,64]`. Se conservan terminales antiguos y se pierden recientes. El retry de un terminal expulsado no devuelve su respuesta; cae en el watermark (`scripts/5_Mission/LFPG_BTCSessionRegistry.c:270`, `scripts/5_Mission/LFPG_BTCSessionRegistry.c:300`). No implica repetir la operación monetaria.

**Coste: ACOTADO.** Corregir las dos expulsiones del mismo fichero; no es TRIVIAL bajo la definición de una sola línea del brief.

**Solapamiento:** mismo registro que SEC13/14; `CheckRequest` comparte con SEC13 la decisión de replay o secuencia obsoleta.

### SEC13 — VIVA: un salto válido agota el espacio de secuencias del UID

**Evidencia:** se admite el límite inclusivo de 2.000.000.000 (`scripts/5_Mission/LFPG_BTCSessionRegistry.c:18`, `scripts/5_Mission/LFPG_BTCSessionRegistry.c:264`). `ReserveRequest` exige que la secuencia supere la anterior, pero no limita el salto (`scripts/5_Mission/LFPG_BTCSessionRegistry.c:318`, `scripts/5_Mission/LFPG_BTCSessionRegistry.c:325`); después la convierte en watermark (`scripts/5_Mission/LFPG_BTCSessionRegistry.c:341`). El guard de agotamiento comprueba el watermark previo, por lo que una sesión nueva puede saltar directamente al máximo.

El siguiente número superior se rechaza por exceder el cap y los inferiores no son transacciones nuevas. Reabrir conserva watermark y pareja de sesión (`scripts/5_Mission/LFPG_BTCSessionRegistry.c:151`). El handler lee la secuencia del cliente, la comprueba y la reserva sin exigir pasos consecutivos (`scripts/5_Mission/LFPG_BTCHelper.c:1087`, `scripts/5_Mission/LFPG_BTCHelper.c:1101`, `scripts/5_Mission/LFPG_BTCHelper.c:1266`). Hace falta llegar a una reserva válida; no basta con mandar cualquier payload rechazado antes.

**Coste: ACOTADO.** Acotar el avance aceptado en los dos puntos de admisión del registro, sin asumir que todos los números intermedios llegaron al servidor. Una renovación de sesión requeriría otro contrato y sería AMPLIO.

**Solapamiento:** mismo fichero que SEC12/14; comparte `CheckRequest` con SEC12 y la conservación de sesión con SEC14. Afecta al propio UID hasta reiniciar el proceso; los replays aún presentes pueden responder. No se demostró bloqueo de otros jugadores.

### SEC14 — VIVA: retención por UID en BTC y avisos, no en todos los limitadores

**Evidencia:** el singleton BTC mantiene `m_ByUID` (`scripts/5_Mission/LFPG_BTCSessionRegistry.c:91`, `scripts/5_Mission/LFPG_BTCSessionRegistry.c:95`) y añade cada sesión en `scripts/5_Mission/LFPG_BTCSessionRegistry.c:137`; no hay retirada, caducidad ni desactivación en ese registro. `LFPG_Util` conserva un mapa estático por identidad/categoría (`scripts/3_Game/LFPG_Util.c:23`, `scripts/3_Game/LFPG_Util.c:29`, `scripts/3_Game/LFPG_Util.c:34`) sin limpieza. Las búsquedas de ambos símbolos en todo `scripts/` no encuentran otra gestión de su ciclo de vida.

El volumen retenido depende de los UID distintos vistos durante la vida del proceso, no solo de los jugadores conectados. En cambio, los mapas generales de rate limit sí se eliminan en `scripts/5_Mission/LFPG_NetworkManagerImpl.c:805`, con llamada periódica desde `scripts/5_Mission/LFPG_NetworkManagerImpl.c:628`; el registro de control también retira sesiones terminales (`scripts/5_Mission/LFPG_ControlSessionRegistry.c:444`).

**Coste: AMPLIO.** Separar la retención necesaria para impedir replays de las respuestas pesadas y diseñar limpieza de avisos. Borrar sin más una sesión BTC al desconectar reabriría números de secuencia bajo la misma pareja de proceso.

**Solapamiento:** registro BTC con SEC12/13; `LFPG_Util.c` con SEC05/07. No se acredita fuga sin límite por cada RPC ni una cifra de memoria: BTC limita terminales por UID y los avisos usan categorías definidas por servidor.

### SEC15 — VIVA: ocho etiquetas no equivalen a ocho políticas aplicadas

**Evidencia:** las ocho constantes están en `scripts/4_World/LFPG_RPCGuard.c:10`; `RoutePolicy` solo reconoce sus valores (`scripts/4_World/LFPG_RPCGuard.c:208`). El dispatcher rechaza subIDs sin clasificar (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:41`), pero las únicas llamadas a `LFPG_RPCGuard.Admit` y `LFPG_RPCGuard.Authorize` de todo `scripts/` están dentro de `HandleInspectDevice`, en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2304` y `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2332`.

La cobertura común anunciada por las etiquetas no existe como aplicación central de esas dos fases. Esto es deuda de contrato/mantenimiento demostrada; no prueba que los otros RPC estén desprotegidos: cableado valida herramienta, distancia y puertos; sync tiene `AuthorizeDeviceSync`; foco tiene registro de sesión; BTC valida nonce, ritmo y ATM.

**Coste: AMPLIO.** Definir qué controles corresponden a cada familia y reconciliar los handlers con esa autoridad. Aplicar indiscriminadamente el `Authorize` actual sería incorrecto: exige un ID LFPG y distancia de interacción (`scripts/4_World/LFPG_RPCGuard.c:180`, `scripts/4_World/LFPG_RPCGuard.c:191`), mientras sync permite un radio diferente y cableado soporta vanilla.

**Solapamiento:** handler con SEC03/04/05/06/07/08/17; contrato de admisión con SEC05/06. Recomiendo tratar la centralización por sí sola como P3; los defectos concretos de seguridad mantienen sus propias fichas.

### SEC16 — VIVA: sigue duplicada la parte común del protocolo de archivos

**Evidencia:** los métodos son `AtomicSaveVanillaWires` (`scripts/3_Game/LFPG_FileUtil.c:46`), `AtomicSaveSettings` (`scripts/3_Game/LFPG_FileUtil.c:135`) y `AtomicSaveBalances` (`scripts/3_Game/LFPG_FileUtil.c:213`). Se repite la preparación de `.bak.new` en `scripts/3_Game/LFPG_FileUtil.c:81`, `scripts/3_Game/LFPG_FileUtil.c:169` y `scripts/3_Game/LFPG_FileUtil.c:257`; la rotación por `DeleteFile`/`CopyFile` se repite en `scripts/3_Game/LFPG_FileUtil.c:114`, `scripts/3_Game/LFPG_FileUtil.c:195` y `scripts/3_Game/LFPG_FileUtil.c:309`.

Esos pasos trabajan con rutas y no necesitan el tipo JSON, aunque escritura y verificación sí son tipadas. La ficha sigue viva para esa duplicación; sería incorrecto afirmar hoy que los tres cuerpos completos son idénticos: saldos incorpora marcador de operación y tratamiento de abortos (`scripts/3_Game/LFPG_FileUtil.c:271`, `scripts/3_Game/LFPG_FileUtil.c:288`).

**Coste: ACOTADO.** Extraer únicamente los pasos comunes dentro del mismo archivo, conservando las diferencias de saldos. Es deuda de mantenimiento; no se atribuye daño directo al jugador por duplicar líneas.

**Solapamiento:** exactamente los mismos protocolos que SEC10; fichero compartido con SEC19. No conviene mezclar una limpieza amplia con la corrección de recuperación sin comprobar sus fronteras de fallo.

### SEC17 — VIVA: dos pasadas de geometría en FINISH_WIRING

**Evidencia:** el handler calcula los extremos en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:440` y llama a `CanPreConnect` en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:465`. Esta llamada omite los parámetros opcionales de prefijo calculado y recorre los waypoints (`scripts/3_Game/LFPG_ConnectionRules.c:131`, `scripts/3_Game/LFPG_ConnectionRules.c:142`). Después vuelve a obtener los extremos (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:484`) y llama a `ValidateWire` (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:496`), que repite segmentos y total (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:1788`, `scripts/5_Mission/LFPG_NetworkManagerImpl.c:1808`).

También se reiteran cantidad de waypoints, destino en inventario y autoconexión. Hay trabajo redundante demostrado, pero no igualdad completa de decisiones: `CanPreConnect` clasifica el exceso de longitud total como aviso (`scripts/3_Game/LFPG_ConnectionRules.c:170`), y el servidor lo rechaza en `ValidateWire`.

**Coste: AMPLIO.** Compartir los cálculos/resultados o unificar el contrato entre reglas de `3_Game`, manager y handler manteniendo el rechazo servidor. Quitar sencillamente la segunda llamada cambiaría comportamiento.

**Solapamiento:** misma función que SEC03/05/07; manager con SEC04/05/14. Los prefijos que aceleran el preview no eliminan esta doble validación del servidor.

### SEC18 — VIVA: infraestructura huérfana y documentación desfasada

**Evidencia:** `scripts/3_Game/LFPG_Migrators.c:5` afirma que `DeserializeJSON` llama a `MigrateBlob`; las entradas siguen declaradas en `scripts/3_Game/LFPG_Migrators.c:40` y `scripts/3_Game/LFPG_Migrators.c:68`. La búsqueda de `LFPG_Migrators`, `MigrateBlob` y `MigrateVanillaStore` en el código y configuración solo encuentra esas declaraciones/comentarios y el puntero documental de `scripts/3_Game/LFPG_Defines.c:253`.

El consumidor real documenta expresamente la retirada de la cadena en `scripts/3_Game/LFPG_WireHelper.c:321`, parsea directamente en `scripts/3_Game/LFPG_WireHelper.c:334` y valida cada cable en `scripts/3_Game/LFPG_WireHelper.c:369`. Vanilla también carga directamente (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:4612`). La clase muerta es precisamente el defecto vivo; no significa que esté fallando una migración activa.

**Coste: AMPLIO.** Retirar o corregir la infraestructura y sus referencias de varios ficheros, sin reactivar migraciones porque sí.

**Solapamiento:** `LFPG_WireHelper.c` también interviene en SEC03/07; carga vanilla con SEC10. No hay solapamiento funcional entre la cadena huérfana y el parser de versión de saldos de SEC19.

### SEC19 — VIVA: lectura completa para encontrar la versión

**Evidencia:** `TryReadRawJsonVersion` abre el archivo y concatena todas sus líneas hasta EOF en `scripts/3_Game/LFPG_FileUtil.c:755` y `scripts/3_Game/LFPG_FileUtil.c:761`, sin parar al encontrar la clave. Busca `"ver"` después (`scripts/3_Game/LFPG_FileUtil.c:768`) y crea una subcadena con todo el sufijo (`scripts/3_Game/LFPG_FileUtil.c:777`).

El llamador normal de saldos ejecuta el helper antes y después de la recuperación (`scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1943`, `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1983`) y luego efectúa la carga JSON completa (`scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1995`). Con target válido existente hay dos pasadas adicionales del helper sobre el mismo archivo; no está en un tick ni se ha medido un tiempo de carga concreto.

**Coste: ACOTADO.** Sustituir el lector dentro de `LFPG_FileUtil.c` por uno que delimite correctamente la clave superior y no acumule todo el archivo. Mantener ambas barreras de versión del consumidor protege contra un archivo futuro antes y después de recuperar.

**Solapamiento:** fichero con SEC10/16 y misma carga de saldos que SEC10. El coste exacto de concatenaciones/asignaciones nativas no está medido; la lectura completa y la construcción del sufijo sí son explícitas.

## LAS QUE RECOMIENDO ATACAR PRIMERO

1. **SEC10.** Un target ilegible deja sin cargar cables o saldos aunque haya backup válido. El impacto puede abarcar toda la instalación y exige intervención del administrador; corregir preservando evidencia y la protección contra downgrade.
2. **SEC05.** El mecanismo que debe frenar abuso genera trabajo de logs y red por cada intento rechazado. Afecta al servidor compartido y no necesita que cada petición supere las validaciones de juego.
3. **SEC08.** Permite impedir a otros jugadores usar un foco mientras el operador sigue vivo, alimentado el dispositivo y sin enviar la salida. Hay una condición de cierre ausente identificada en un tick que ya existe.
4. **SEC12.** Los reintentos pueden perder la respuesta original de una operación BTC mucho antes de las 64 respuestas prometidas. Afecta la confirmación de operaciones económicas, aunque el watermark siga impidiendo ejecutarlas dos veces.
5. **SEC06.** Peticiones repetidas admitidas pueden marcar hasta 64 dispositivos por lote sin cambio de estado; amplía trabajo de replicación compartida. Debe conservarse la reparación JIP que motivó el marcado inicial.

Orden por alcance y daño del mecanismo observado, sin convertirlo en una medición de frecuencia real. SEC04 tiene potencial de latencia global en instalaciones grandes, pero falta cuantificar ese coste y el rescate actual protege integridad ante índices incompletos.

## LO QUE NO PUDE VERIFICAR

- Compilación y reproducción en DayZ: no hay compilador ni mundo ejecutado en este encargo; ninguna ficha se presenta como prueba in-game.
- SEC03: no se ensayó el rechazo por capacidad con entidades reales ni contratos de extensiones externas; el cierre se comprobó leyendo las rutas presentes.
- SEC04/05/06: no se midieron duración, tasa de saturación, coalescencia de SyncVars ni tráfico efectivo bajo carga.
- SEC07: no se verificó el máximo del transporte/RPC ni su asignación nativa al deserializar; tampoco cómo representa cada carácter de control el RPT.
- SEC08: no se reprodujo un cliente que omita AIM/EXIT; el mantenimiento del bloqueo se deduce de las condiciones servidor y del tick.
- SEC10/11: no se ejecutaron fixtures de corrupción, backups o fallos de I/O; la mutación parcial del serializador nativo queda potencial, no demostrada.
- SEC12: `P:/scripts` no está disponible. La semántica de `Remove` se verificó en la fuente oficial enlazada; no se verificó la versión del motor que desplegará el mod.
- SEC13: no se envió una reserva al cap contra el juego; tampoco se atribuye esa secuencia al cliente normal ni a una operación contra otro UID.
- SEC14/19: no se midieron memoria retenida por jugador, tamaños de archivos reales ni coste nativo de copia de strings.
- SEC15/18: la ausencia de llamadores se comprobó en este árbol; no cubre mods externos que invoquen su API.
- Las otras lanes trabajan en copias independientes: este informe no incorpora cambios suyos posteriores al HEAD identificado.

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

- **El lote ya incluye una ficha resuelta explícitamente:** SEC03 murió hoy en T2. Mantenerla como pendiente produciría trabajo duplicado; el límite temporal de capacidad restante es otro comportamiento.
- **“Confirmado” y P2/P3 mezclan categorías distintas.** SEC15 acredita aplicación parcial de una abstracción, no ocho familias sin autorización; propondría P3 para esa deuda aislada. SEC16/18 son mantenimiento, no vulnerabilidades activas. SEC11 tiene un defecto estructural vivo, pero su consecuencia de mutación parcial sigue sin prueba del serializador.
- **SEC07 agrupa dos superficies y contiene partes corregidas.** `DIAG_CLIENT_LOG` ya sanea y los `Debug` de payload no imprimen con nivel 1. La exposición que queda demostrada es otra: el ID del cliente llega a un `Warn` del validador. El límite tardío de deserialización debe evaluarse por separado del saneamiento de logs.
- **SEC14 no permite borrar todos los mapas por simetría.** Los limitadores generales sí se purgan; parte de la retención BTC preserva el rechazo de replays entre reconexiones. Eliminarla junto a los avisos sería un cambio de seguridad, no solo una optimización de memoria.
- **SEC16 ya no describe tres protocolos completamente iguales.** Saldos tiene marcador y semántica de aborto propios. Subsiste duplicación de operaciones con rutas, pero extraer un único guardado idéntico para los tres reintroduciría diferencias que hoy están protegidas.
- **SEC17 no autoriza borrar una validación completa.** El total excesivo es aviso en las reglas compartidas y rechazo en el servidor. Deben compartirse cálculos preservando esa diferencia, no asumir paridad porque ambos recorren la misma geometría.
- **SEC04 no se arregla condicionando el rescate a cero resultados.** Puede haber un propietario indexado y otro omitido; el comentario del llamador protege ese caso. Se necesita una garantía de índice completo o una recuperación acotada equivalente.
- **SEC13 es agotamiento del propio UID durante el proceso, no bloqueo global.** No necesita dos mil millones de operaciones, pese al comentario de `scripts/5_Mission/LFPG_BTCSessionRegistry.c:302`: basta un salto aceptado. P3 es defendible sin evidencia de una ruta que permita envenenar otro UID.
- **SEC19 habla de un encabezado, pero el helper no acredita nivel superior.** El comentario de `scripts/3_Game/LFPG_FileUtil.c:748` promete una clave top-level; `IndexOf` en `scripts/3_Game/LFPG_FileUtil.c:768` busca la primera coincidencia sin llevar profundidad JSON. Conviene revisar corrección semántica junto al coste, sin afirmar aquí una corrupción de saldos reproducida.
- **El corte por subsistema separa unidades de cambio.** SEC10/11/16/19 comparten persistencia; SEC12/13/14 comparten replay BTC; SEC03/04/05/06/07/08/15/17 se cruzan en el handler. Agrupar solo por etiqueta “Red/seguridad” oculta esos contratos y aumenta el riesgo de pisar las lanes de implementación.

## CIERRE DEL ENCARGO

Único archivo redactado por esta lane: `TRIAJE.md`. No hay correcciones de código, staging ni commit, por la frontera explícita del brief. El informe sirve de evidencia y handoff; no se escribió memoria externa porque el encargo solo autoriza este entregable. Validación realizada: lectura y cruce de código actual, comprobación histórica de SEC03 y comprobación de integridad del informe y del árbol versionado.
