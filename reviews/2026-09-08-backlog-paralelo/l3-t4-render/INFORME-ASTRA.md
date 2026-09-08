# INFORME — Lane 3: T4 render, cámara y coste del scheduler

Base comprobada: `8de29d5`. Los cinco sitios se localizaron por contenido. Las cinco fichas quedan **ARREGLADA en código**, con validación de comportamiento **INCONCLUSA** hasta cargar el mundo. No se ha compilado Enforce ni ejecutado DayZ.

Se modificaron cuatro archivos de la lista blanca. `LFPG_Defines.c` conserva el límite global de **512**. El único cambio en `LFPG_NetworkManagerImpl.c` ocupa las líneas 6193–6200. Este informe es el entregable adicional expresamente solicitado. No se editó `LFPG_RPCServerHandlerImpl.c`; se consultaron sus rutas de llamada.

## Alcance y decisiones de implementación

El brief se tomó como aprobación. Se aplicaron cambios locales, sin colas nuevas, cambios de RPC, persistencia ni dependencias entre módulos en sentido inverso. S08 se implementó primero.

| Datos | Lado | Tratamiento |
|---|---|---|
| Cargo, posiciones de inventario, reglas y cursores de sorters | Servidor, `5_Mission` | Se mantienen los guards SERVER, los movimientos existentes y su sincronización. |
| Geometría, esfera envolvente, visibilidad y contador de segmentos | Cliente, `4_World` | Estado local; se conserva la frontera de compilación cliente. |
| Sesión CCTV, cámara actual, jugador seleccionado y espera de salida | Cliente, `4_World` | Observación local del motor y generación local de sesión; RPC existente sin cambios. |

### S08 — Repack manual síncrono

**Veredicto: ARREGLADA.**

- `scripts/5_Mission/LFPG_SorterLogic.c:1133`: admisión máxima de 64 items y 1024 celdas; se validan dimensiones antes de multiplicar o reservar las matrices. Un cargo mayor conserva su distribución.
- `scripts/5_Mission/LFPG_SorterLogic.c:1137`: presupuesto compartido de 16384 consultas de celdas para todos los items y ambas orientaciones.
- `scripts/5_Mission/LFPG_SorterLogic.c:1230` y `:1242`: ambas búsquedas usan `TryPlaceOnGridBudgeted`; agotarlo devuelve cero **antes de cualquier movimiento**.
- `scripts/5_Mission/LFPG_SorterLogic.c:1269` y `:1281`: máximo cuatro pasadas y 200 visitas totales de items durante los movimientos. Las visitas ya resueltas también consumen presupuesto. Por tanto, hay como máximo 200 llamadas de movimiento nativo por invocación.
- `scripts/5_Mission/LFPG_SorterLogic.c:1308`: se reutilizan dos `InventoryLocation`, comprobando que la lectura del origen tuvo éxito para no reutilizar una posición anterior inválida.
- `scripts/5_Mission/LFPG_SorterLogic.c:1351`: helper privado que descuenta cada consulta a una celda y conserva el orden de búsqueda y el intento de rotación.

Se acotó también la preparación: la ordenación de como máximo 64 elementos tiene un máximo de 2016 desplazamientos y las matrices tienen como máximo 1024 celdas. No bastaba con limitar únicamente las pasadas de movimiento.

**Motivo y alternativa descartada:** límites internos deterministas cubren todas las rutas sin cambiar la firma ni el retorno síncrono. Se descartaron el guard solo en el llamador y una cola entre ticks, que necesitaría estados, invalidación del cargo y cambios de ACK/refresco. Decisión conservadora: si no cabe en los límites, no se intenta completar todo el repack. Si se agota el presupuesto de movimientos, quedan los movimientos válidos realizados y el resto permanece en su slot; no hay paso por el suelo. Estos límites acotan operaciones, no garantizan milisegundos ni suman peticiones de jugadores distintos.

**Verificación:** revisión estática de los límites, de todos los bucles y de las salidas anteriores a la primera mutación. El helper público anterior `TryPlaceOnGrid` conserva su contrato. El validador offline no detectó errores; advierte sobre el `#else` preexistente. In-game: ejecutar ordenar normal y V4, con/sin salidas cableadas, cargo vacío, 64 y 65 items, cuadrículas de 1024 y más celdas, piezas rectangulares, huecos bloqueados y agotamiento de planificación. PASS requiere respetar las cotas, conservar identidad/cantidad de items, no dejarlos en el suelo y refrescar los inventarios de dos clientes. Medir además duración real de la RPC y peticiones simultáneas.

### S04 — Round-robin al agotar el presupuesto global

**Veredicto: ARREGLADA.**

`scripts/5_Mission/LFPG_NetworkManagerImpl.c:6196` avanza a `sorterIndex + 1`; `:6197` aplica vuelta a cero al alcanzar el total. Se conserva intacto el estado de reanudación por sorter (item, índice, salida, regla y configuración) y el final de batch existente para ticks sin agotamiento.

**Motivo y alternativa descartada:** separar el siguiente turno global de la reanudación interna permite que el sorter caro continúe cuando vuelva su turno. Se descartó borrar la reanudación o aumentar el presupuesto: lo primero puede repetir trabajo indefinidamente y lo segundo conserva el mecanismo de inanición. Con registro estable y un sorter agotando el presupuesto en cada turno, el cursor visita sucesivamente todos los sorters; con uno solo vuelve a cero.

**Verificación:** se leyó el inicio del batch, el guard de agotamiento, la escritura de reanudación y la actualización final del cursor. Solo se cambió la rama de agotamiento dentro de la zona autorizada. In-game: poner al menos tres sorters, primero uno con reglas caras y después dos con rutas sencillas; comprobar que todos reciben turno, que el caro continúa su regla pendiente, y que la vuelta del último al primero funciona. Repetir con un sorter, sin sorters y eliminando uno entre ticks. PASS requiere progreso de los restantes sin perder ni duplicar movimientos.

### R02 — Limpieza de owners con distancia congelada

**Veredicto: ARREGLADA.**

`scripts/4_World/LFPG_CableRenderer.c:2492` calcula la distancia desde la posición **actual** del jugador a la esfera conservada del cable, resta el radio, limita a cero y actualiza `cachedMinDist` inmediatamente antes de decidir si hay un cable próximo. Se mantienen el umbral de 15 ticks nulos y la eliminación diferida del owner.

**Motivo y alternativa descartada:** la entidad ya no existe localmente; su geometría almacenada permite valorar proximidad sin resolver de nuevo el owner. Se descartó usar la última distancia o borrar siempre tras 30 segundos. La esfera se conserva incluso cuando R04 libera los segmentos, por lo que las dos correcciones no se invalidan entre sí. Es distancia a una esfera envolvente, no distancia exacta a cada tramo.

**Verificación:** seguimiento estático de owner nulo, prueba de proximidad y `continue`. In-game: perder el owner con el jugador cerca y alejarse durante los 15 ticks; también perderlo lejos y acercarse antes del vencimiento. PASS requiere limpiar en el primer caso si ninguna esfera queda próxima, conservar en el segundo y reconstruir si la entidad vuelve. Comprobar además varios cables del mismo owner, uno de ellos próximo.

### R04 — Capacidad invisible y prioridad espacial

**Veredicto: ARREGLADA.**

- `scripts/4_World/LFPG_CableRenderer.c:2189` y `:2238`: `BuildWire` devuelve éxito y centraliza la admisión usando el número exacto de segmentos después de compactar y aplicar la catenaria. Ambos llamadores usan el resultado (`:2177`, `:3999`). Se retiraron las dos copias locales obsoletas de `totalSegs`.
- `scripts/4_World/LFPG_CableRenderer.c:4242`: `ReleaseWireSegments` descuenta los segmentos reales y vacía su geometría, conservando la esfera y los datos del cable.
- `scripts/4_World/LFPG_CableRenderer.c:2435`, `:2467` y `:2589`: `CullTick` libera capacidad al ocultar por owner lejano, owner ausente o culling individual.
- `scripts/4_World/LFPG_CableRenderer.c:4253`: admisión espacial. Se rechaza geometría fuera de distancia/burbuja; si no cabe, se comprueba primero si hay capacidad recuperable y se liberan invisibles y después los cables más lejanos. Se excluye el propio cable sustituido. No se expulsan cables más próximos para alojar uno lejano. Empates de distancia conservan al residente para evitar alternancias.
- `scripts/4_World/LFPG_CableRenderer.c:2532` y `:2591`: se reencolan los cables ausentes o visibles sin segmentos, incluida la recuperación después de vencer el TTL de retry.
- `scripts/4_World/LFPG_CableRenderer.c:2719`: los metadatos sin segmentos no entran en el orden de dibujo ni consumen sus turnos de oclusión.

**Motivo y alternativa descartada:** recuperar capacidad y elegir qué geometría conservar resuelve la dependencia del orden de llegada de owners. Se descartó subir el límite de 512, restarlo sin liberar geometría, o invertir el orden de pintado: esas opciones no mantienen la misma invariante de capacidad o alteran la composición visual. El contador sigue representando la suma de segmentos construidos; los metadatos vacíos cuentan cero. `LFPG_Defines.c` no cambia.

La prioridad usa la misma aproximación por esfera que el renderer. La invisibilidad liberada aquí es la del culling; no se destruye geometría únicamente por quedar temporalmente detrás de cámara u ocluida. Esto preserva el historial de oclusión y evita reconstrucciones al girar. La admisión bajo presión recorre residentes; su coste y el efecto visual de reconstruir necesitan medición. La vuelta puede esperar al siguiente CullTick y RetryTick, aproximadamente hasta 7 segundos con sus intervalos nominales.

**Verificación:** lectura de construcción/destrucción, ambas admisiones, las tres salidas de culling, reentrada y consumidor de `m_DrawOrder`; validador offline PASS. In-game: saturar los 512 segmentos con cables lejanos, acercarse a otros inicialmente omitidos y repetir invirtiendo el orden de aparición. PASS requiere que entren los próximos, que los invisibles no retengan segmentos y que `m_TotalSegCount` coincida con la suma real sin superar 512 ni volverse negativo. Repetir movimiento entre dos zonas, burbuja de dispositivos, owner ausente, TTL vencido, actualización de cables y geometría degenerada; observar popping y tiempo de cada retry bajo saturación.

### R15 — Salida CCTV, timeout y sesión

**Veredicto: ARREGLADA. Validación in-game pendiente.**

- `scripts/4_World/LFPG_CameraViewport.c:142` y `:390`: generación local de sesión; cada entrada aceptada obtiene una generación nueva, que no se reinicia al destruir el singleton.
- `scripts/4_World/LFPG_CameraViewport.c:763`: la confirmación legacy se trata como indicio. Con una sesión nueva activa no desencadena salida salvo que ya se observe restauración del jugador.
- `scripts/4_World/LFPG_CameraViewport.c:779`: se exige jugador seleccionado, identidad de entidad coincidente si se conserva la referencia, ausencia de cámara de script activa y existencia de cámara propia del jugador.
- `scripts/4_World/LFPG_CameraViewport.c:794`: la limpieza exige generación vigente, generación de salida coincidente, fase de salida y restauración observada antes de desactivar la cámara.
- `scripts/4_World/LFPG_CameraViewport.c:873` y `:893`: Tick observa restauración tanto en salidas iniciadas por servidor como en la espera normal, independientemente de si la replicación llega antes o después del confirm.
- `scripts/4_World/LFPG_CameraViewport.c:900`: los cinco segundos producen un único warning por salida; no fuerzan la limpieza. `:974` asocia la espera a su sesión. `ForceCleanup` conserva su función excepcional de desconexión/reset e invalida el estado local (`:709`).

**Motivo y alternativa descartada:** desactivar tras un plazo no demuestra que exista cámara de jugador. Se descartaron el cleanup forzado y aumentar el timeout. Si el motor no restaura, se mantiene la espera y se bloquea una entrada nueva hasta restauración o reset/desconexión. Es una decisión conservadora que evita liberar la cámara todavía activa, a costa de no prometer recuperación automática ante un fallo permanente.

El protocolo actual **no transporta un token de sesión**: `scripts/4_World/LFPG_RPCClientHandler.c:148` llama a `DoExitCleanup()` sin argumentos y `scripts/5_Mission/LFPG_ControlSessionRegistry.c:259` envía solo el sub-ID. La generación local no autentica ni identifica la antigüedad de ese RPC. La protección del confirm tardío legacy depende de comprobar el estado del motor. Se descartó afirmar correlación de extremo a extremo o cambiar solo un lado del formato; añadirla exige editar archivos fuera de esta lista blanca.

**Verificación:** validador offline PASS y revisión de entrada, fallo de entrada, salida voluntaria, salida del servidor, repetición del confirm, timeout y reset. La semántica observada se apoya en las declaraciones vanilla indicadas abajo. In-game: salir normalmente; retrasar más de cinco segundos la restauración; invertir orden entre confirm y replicación; entregar un confirm antiguo durante una nueva sesión; perder energía, morir/quedar inconsciente y desconectar. PASS requiere conservar la cámara durante la espera, liberar solo tras restaurar, recuperar HUD/input y no desactivar una nueva cámara. Probar también coexistencia con otros mods de cámara. Sin esa ejecución no se cierra la seguridad de restauración.

## Evidencia estática y límites de los checks

- `git -c core.whitespace=cr-at-eol diff --check`: sin incidencias. Se usa esa opción solo para el comando porque dos blobs del repositorio conservan CRLF y el check por defecto interpreta sus CR como whitespace; no se cambió configuración de Git.
- Inspección automatizada del diff: cuatro archivos autorizados, 332 líneas añadidas comprobadas, ninguna construcción prohibida del brief, indentación añadida con tabs y sin bytes NUL. Los cuatro archivos conservan exclusivamente CRLF: 4406, 1335, 7789 y 1467 líneas respectivamente (renderer, viewport, manager, sorter).
- Validador real: `python -B C:/Users/guill/DayZ-Modding-Knowledge-Pack/tools/dayz-script-validator/scripts/script_validator.py .`: 271 archivos analizados, cero errores y 52 warnings (41 `ES-EMPTY-IFDEF-UNSUPPORTED-PATTERN`, 4 `ES-GETTYPE-EXACT-MATCH`, 7 `ES-CTX-READ-UNCHECKED`). Se leyó el resultado; no equivale a una compilación.
- Repetición sobre los cuatro archivos finales: CableRenderer PASS/0; CameraViewport PASS/0; SorterLogic WARN/2 por su `#else` preexistente en `:1345`; NetworkManagerImpl WARN/2 por patrones preexistentes en `:969`, `:983`, `:1009`, `:1301`, `:1723`, `:4234`. Ningún warning cae en una línea añadida. No se modificaron esos patrones fuera del alcance. Los códigos indicados son los códigos del proceso Python, capturados con `subprocess`.
- Se revisaron en fuente las firmas y consumidores, sin inventar APIs: `TryPlaceOnGrid`, `MarkGridOccupied`, `GetItemSlotDimensions`, `BuildBoundingSphere`, `DestroyAll`, `BuildWire`, callbacks CCTV y escritura de cursores. No se añadieron tests que repliquen el algoritmo y puedan confundirse con ejecución del motor.

Fuentes vanilla leídas bajo `C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts/`:

| Uso | Definición leída |
|---|---|
| `Camera.GetCurrentCamera()` retorna null para cámara de jugador; `SetActive` | `3_game/entities/camera.c:4`, `:7`, `:45` |
| Cámara propia del jugador | `3_game/dayzplayer.c:1216` |
| Jugador seleccionado | `3_game/global/game.c:946` |
| Parámetro `inout int` con mutación del valor | `4_world/classes/contaminatedarea/effectarea.c:411` |
| Lectura de ubicación y movimiento síncrono | `3_game/systems/inventory/inventory.c:268`, `:567` |
| Posición cargo de destino | `3_game/systems/inventory/inventorylocation.c:152` |
| Conteo/dimensiones/items del cargo | `3_game/systems/inventory/cargo.c:28`, `:32`, `:37`, `:41` |

No se hizo commit ni se modificó el índice, por prohibición expresa. No se escribió memoria externa ni un handoff en el vault: la restricción de escritura exige que este informe sea el handoff de la lane. No se modificaron los archivos de control/log que ya existían sin seguimiento al comenzar.

## LO QUE NO PUDE VERIFICAR

- Compilación real de los módulos Enforce al cargar un mundo.
- Restauración CCTV, seguridad de referencias de cámara e input/HUD in-game, especialmente con latencia, muerte, respawn u otros mods de cámara.
- Correlación RPC de sesión de extremo a extremo: el protocolo actual carece de token y no se modificó.
- Duración en milisegundos de cada llamada nativa de inventario y coste acumulado de varias peticiones manuales en un mismo tick.
- Conservación y refresco del cargo observados desde dos clientes y progreso real del round-robin con altas/bajas de sorters.
- Ausencia de popping, frecuencia real de rebuilds, picos de RetryTick y recuperación visual bajo saturación y streaming.
- Convivencia del diff con los cambios de las otras lanes; esta revisión corresponde exclusivamente a este worktree.

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

- S08 no está dominado solo por N pasadas: las dimensiones del cargo y los rectángulos multiplican también el coste de búsqueda. Limitar pasadas sin acotar planificación deja otro pico. Los límites elegidos sacrifican repack completo por encima de 64 items/1024 celdas; si el producto exige ordenar siempre esos cargos, hará falta un trabajo incremental con contrato de finalización, no aumentar estos topes sin medir.
- La cita V4 de S08 describe una ruta, no una segunda llamada directa actual al repack. En esta base `LFPG_RPCServerHandlerImpl.c:150` delega en `HandleSorterRequestSort`; `:2697` llama al manager. Las dos invocaciones directas están en `LFPG_NetworkManagerImpl.c:6479` (sin salidas) y `:6569` (tras mover). Ambas heredan el límite interno. No faltaba una tercera implementación de repack que hubiera que modificar.
- En R04, 512 es un límite de geometría construida, no de líneas de Canvas dibujadas: los segmentos son datos (`LFPG_CableParticle`), y LOD/decoradores pueden cambiar las llamadas de dibujo. La corrección no establece un presupuesto de GPU ni elimina metadatos de cables invisibles. La frase «invisibles» debe distinguir culling de oclusión y de quedar detrás de cámara.
- El comentario anterior del TTL de R04 decía que CullTick reconstruía tras expirar el retry, pero la rama de cable sin geometría solo hacía `continue`. La reencolación añadida es necesaria para que liberar segmentos no cause una desaparición persistente.
- En R15, un contador local no convierte un confirm sin payload en un mensaje de sesión verificable. La corrección de esta lane usa restauración observable como condición de seguridad; la garantía temporal y las transiciones reales siguen pendientes de DayZ. Si ese predicado no se cumple aunque el motor considere restaurada la vista, el resultado será una espera indefinida que habrá que resolver con evidencia in-game.
