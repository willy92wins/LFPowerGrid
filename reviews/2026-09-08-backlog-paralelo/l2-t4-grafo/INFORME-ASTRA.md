# INFORME — Lane 2 · T4 Grafo eléctrico

Base comprobada: `8de29d5b2572757bd9518e9ea216d39ca05da491`. Fecha: 2026-09-08.
Las cuatro fichas se localizaron por contenido. **ARREGLADA significa corrección implementada y revisada estáticamente; no implica compilación ni validación en DayZ.**

## Alcance y decisiones comunes

Se modifican únicamente `scripts/5_Mission/LFPG_ElecGraphImpl.c` y `scripts/4_World/LFPG_VanillaActionOverrides.c`. Este `INFORME.md` es la excepción implícita a la lista blanca exigida por el propio entregable. No se modificaron otros archivos ni se ejecutaron operaciones de escritura de Git.

Se conservaron CRLF y la codificación sin BOM de los dos scripts; la indentación añadida usa tabs y la existente permanece intacta. El blob de Git usa LF, pero los archivos del worktree usan CRLF. No se normalizó el repositorio.

El brief es el plan aprobado. Se consultaron las skills `dayz-mod-workflow` y `enforce-script-reference`, las checklists y el `LFPowerGrid_dev/CLAUDE.md` del proyecto. No hay `CLAUDE.md` dentro de este worktree. La memoria encontrada corresponde a otros trabajos y no sustituye este encargo. No se crearon subagentes.

| Estado afectado | Autoridad | Cliente / mecanismo existente |
|---|---|---|
| Entrada, generación virtual, demanda y asignaciones | Grafo servidor en 5_Mission | La sincronización existente publica el resultado; no se añaden RPC ni SyncVars. |
| Estado de trabajo vanilla | CompEM en servidor | Los hooks nuevos solo existen bajo `SERVER`; se conserva el comportamiento vanilla cliente. |
| Notificación al solver | Interfaz `LFPG_NetworkManager` de 4_World | Despacho virtual a su implementación de 5_Mission, sin referencias ascendentes de tipos. |

### G02 → Baterías que generan desde almacenamiento

**Veredicto: ARREGLADA.**

En `scripts/5_Mission/LFPG_ElecGraphImpl.c:3014` se calcula una potencia efectiva separada de la entrada cableada. Para PASSTHROUGH se añade `m_VirtualGeneration` y se permite evaluar el estado aunque no exista ninguna arista entrante (`:3018`, `:3019`). Las comprobaciones de consumo y de potencia mínima leen ese resultado (`:3030`, `:3038`). Ambas ramas correctoras, encendido y apagado, dependen de ese mismo `shouldBePowered`.

La referencia es el solver del propio archivo: `:2215` suma entrada y generación virtual y exige potencia efectiva superior al epsilon antes de cubrir el autoconsumo. `m_InputPower` sigue representando exclusivamente entrada cableada, según `scripts/3_Game/LFPG_Data.c:195` y la escritura del solver en `scripts/5_Mission/LFPG_ElecGraphImpl.c:2191`. No se suma generación al caché de entrada, lo que evitaría contabilizarla dos veces en el siguiente cálculo.

Se descartó cambiar solo la suma dentro de los comparadores: el guard anterior `hasAnyIncoming` seguiría excluyendo una batería sin cable de entrada. También se descartó eximir todas las baterías del validador: una batería agotada debe poder apagarse. Los criterios de CONSUMER y CAMERA conservan su comportamiento.

**Verificación:** se contrastaron los dos predicados con el solver. In-game: conectar una batería cargada solo por su salida a una carga válida; esperar varias rondas completas del validador y comprobar potencia estable y ausencia de correcciones falsas «Zombie node fixed». Repetir con entrada cableada deshabilitada, salida de batería cerrada y batería agotada: no debe aparecer alimentación espuria. Probar además autoconsumo cubierto/no cubierto y un consumidor común sin entrada. PASS exige coherencia entre estado de batería, potencia asignada a la carga y estado sincronizado; cualquier apagado periódico falso es FAIL.

### G01 → Señal de cambio durante un reparto flexible idéntico

**Veredicto: ARREGLADA.**

Se añade un búfer de valores anteriores como miembro (`scripts/5_Mission/LFPG_ElecGraphImpl.c:82`), creado una vez en el constructor (`:158`). Se vacía por llamada (`:3686`), reserva una posición por cada arista, incluidas las nulas/deshabilitadas (`:3696`), y guarda la asignación anterior antes de sobrescribirla (`:3703`). Tras terminar el reparto duro y flexible, se compara la asignación final de cada arista habilitada con su valor de entrada (`:3782`, `:3796`). Solo una diferencia absoluta superior a `LFPG_PROPAGATION_EPSILON` activa `m_AllocChanged` (`:3801`). Se eliminaron las dos detecciones sobre valores intermedios.

Se mantienen exactamente las fórmulas, escrituras de asignaciones, acumulaciones de `totalAllocated` y cálculos de demanda/carga. La comparación distingue redistribuciones entre aristas aunque la suma total no cambie. No borra una señal de cambio previa; el único llamador de `AllocateOutput` sigue en `:2361` y el reinicio por nodo ya existe en el solver.

Se descartó comparar únicamente totales, porque perdería una redistribución `[20, 80]` a `[30, 70]`. Se descartó fusionar los pasos de cálculo, para no alterar orden aritmético, redondeos o política de sobrecarga. El coste es un búfer reutilizable O(grado de salida) y hasta una visita adicional por arista; estas visitas se cargan al contador de presupuesto (`:3789`). No hay nuevas colecciones creadas dentro del tick.

**Verificación:** una comparación automatizada con HEAD, excluyendo exclusivamente el registro/detección de cambios, confirmó que el cuerpo aritmético y las escrituras existentes permanecen idénticos. Se comprobó estáticamente la alineación del búfer y el orden snapshot → escrituras → comparación final. Ejemplo de revisión: anterior 100, parte dura 20, bonus flexible 80, final 100 debe dejar el flag apagado; final 80 debe encenderlo. Esto es una traza razonada, no ejecución Enforce.

In-game: mantener un generador cargando una batería con demanda estable; provocar reevaluaciones sin cambiar entradas y observar que el tramo duro/flexible no dispara por sí solo reencolados. Cambiar excedente, provocar sobrecarga y redistribuir carga entre dos salidas conservando el total: deben detectarse los cambios por arista. Cubrir aristas nulas/deshabilitadas y nodos sin salidas en el harness del receptor. Comparar cola, visitas y mensajes emitidos con el baseline: la reducción de tráfico no se ha medido aquí.

### G18 → PASSTHROUGH contado como proveedor por su señal de demanda

**Veredicto: ARREGLADA.**

Localizado en `CountPoweredIncoming`, `scripts/5_Mission/LFPG_ElecGraphImpl.c:3469`, utilizado al repartir demanda hacia un PASSTHROUGH (`:3646`). Antes contaba cualquier `m_OutputPower` positivo. En PASSTHROUGH ese campo puede seguir expresando demanda sin que exista energía para servirla.

Ahora el candidato PASSTHROUGH usa entrada cableada más generación virtual (`:3494`), resta el autoconsumo cuando el solver lo considera significativo (`:3497`) y deja de contar si su compuerta está cerrada (`:3501`). El conteo exige remanente positivo por encima del epsilon (`:3504`). Se conserva la comprobación de aristas habilitadas. SOURCE mantiene el criterio anterior: publica potencia real antes de asignar (`:2350`), necesario para arrancar el reparto.

Se descartó exigir `m_AllocatedPower > epsilon`: confundiría «puede suministrar» con «ya suministra» y puede bloquear la recuperación de sobrecarga. Dos PASSTHROUGH con 30 disponibles cada uno deben poder compartir una demanda de 50 aunque sus asignaciones previas sean cero; exigir asignación positiva puede hacer que cada uno vea los 50 completos y permanezca sobrecargado. También se descartó usar solo `m_Powered`: un dispositivo encendido puede gastar toda su entrada en sí mismo o tener cerrada la salida.

**Verificación:** revisión del mecanismo y sus dependencias, sin ejecutar el solver. In-game: alimentar un combinador desde una fuente viva y desde un PASSTHROUGH sin entrada pero con demanda descendente; solo la rama viva debe participar en el reparto. Repetir con compuerta cerrada y con toda la entrada consumida por el propio dispositivo. Positivos: PASSTHROUGH con remanente y batería con descarga virtual deben contar. Probar especialmente dos ramas con capacidad suficiente conjunta y asignaciones cero tras sobrecarga: deben recuperar el reparto. El conteo usa estado cacheado de los nodos; su convergencia al abrir/cerrar compuertas y en distintos órdenes de cola sigue pendiente de juego.

### G04 → Cambios de una fuente vanilla sin notificación

**Veredicto: ARREGLADA.**

Se comprobó primero el árbol: `scripts/5_Mission/LFPG_NetworkManagerImpl.c:3077` es el llamador directo de `RefreshSourceState`. Su interfaz `RequestPropagate` existe en `scripts/4_World/LFPG_NetworkManager.c:414`; los generadores LFPG ya la invocan (`scripts/4_World/LFPG_TestDevices.c:277`, `:294`). Los llamadores de cableado y sensores no cubren el encendido normal de un generador vanilla. Las acciones originales terminaban en su `super`.

Se añade un `modded class PowerGenerator`, solo servidor, dentro del archivo permitido. Los callbacks `OnWorkStart` y `OnWorkStop` conservan `super` y notifican (`scripts/4_World/LFPG_VanillaActionOverrides.c:183`, `:189`). El helper (`:195`) excluye fuentes nativas, obtiene exclusivamente el manager existente (`:201`) y llama a `RequestPropagate` con el ID determinista existente (`:206`). No crea un manager durante arranque/cierre. Si todavía no existe, la población inicial del grafo lee `CompEM.IsWorking()` cuando construye los nodos.

Firmas y orden comprobados en la referencia local vanilla, raíz `C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts/`: `4_world/entities/itembase/powergenerator.c:145` y `:194` definen los callbacks; `:445` declara `PowerGenerator extends PowerGeneratorBase`. `3_game/tools/component/componentenergymanager.c:1741` actualiza `m_IsWorking`; `:1849` lo establece antes del callback de arranque (`:1857`), y `:1891` lo apaga antes del callback de parada (`:1898`). El agotamiento sigue la rama `:1863` y callback `:1871`. No se infirieron APIs por su nombre.

APIs del mod verificadas: `LFPG_DeviceAPI.IsSource` en `scripts/4_World/LFPG_IDevice.c:515`, `GetOrCreateDeviceId` en `:96`, `LFPG_NetworkManager.GetExisting` en `scripts/4_World/LFPG_NetworkManager.c:76`. `RefreshSourceState` distingue fuente nativa de vanilla y lee CompEM para esta última (`scripts/5_Mission/LFPG_ElecGraphImpl.c:3345`). No se llama a ninguna clase de 5_Mission desde 4_World.

Se descartó añadir notificaciones solo después de las dos acciones: dejaría fuera las paradas por combustible y otros cambios reales de trabajo. Los hooks siguen el estado que consulta el solver. Se excluyen las fuentes nativas porque actualizan su propio estado después de `super` y ya notifican; leerlas desde el hook de la base podría tomar su estado anterior. Las acciones previas permanecen byte a byte intactas.

**Verificación:** revisión de la cadena vanilla → callback → interfaz World → implementación Mission → estado del grafo. In-game: cablear generador vanilla apagado a una lámpara LFPG, encenderlo, apagarlo y dejarlo sin combustible. El grafo debe seguir `IsWorking()` en los siguientes ticks sin recablear ni reconstruir. Repetir quitando bujía, arrancando tras reinicio y usando un LFPG_Generator: este último debe conservar su única ruta nativa. Probar también un generador sin cables: no debe crear nodos por la notificación. Compatibilidad con otros mods que sobrescriban los callbacks sin llamar a `super` no está establecida.

## Verificación estática y entrega

- `git diff --check`: sin incidencias. Diff de código: dos archivos, 93 líneas añadidas y 26 eliminadas.
- Revisión de líneas añadidas: sin operadores/iteraciones/logging prohibidos; la única declaración nueva con `ref` es el miembro `array<float>`. CRLF conservado y sin espacios usados para indentar líneas añadidas.
- Comparación G01 con HEAD: cálculo del reparto conservado, con cambios restringidos al snapshot y a la detección final.
- Linter offline: resultado `WARN` (exit interno 2), 0 errores y 52 avisos sobre el árbol final: 41 `ES-EMPTY-IFDEF-UNSUPPORTED-PATTERN`, 4 `ES-GETTYPE-EXACT-MATCH` y 7 `ES-CTX-READ-UNCHECKED`. Los 14 avisos del grafo señalan ramas `#else` preexistentes que ese detector no analiza; ninguno señala las líneas añadidas. No hay avisos en `LFPG_VanillaActionOverrides.c`. Los avisos restantes pertenecen a archivos sin modificar; se dejan fuera del alcance, sin clasificarlos como falsos positivos. Se ejecutó `script_validator.py` del Knowledge Pack mediante `run([workspace])`, sin generar bytecode ni archivos de salida.
- No hay compilador Enforce ni juego invocado. El gate del receptor y la revisión de otra familia no se han ejecutado aquí.
- Sin commit, staging ni escrituras fuera del workspace, por las fronteras del encargo. No se actualizó memoria externa ni se creó handoff en el vault: este informe constituye la entrega local autorizada. Los archivos de infraestructura ya presentes al empezar se conservaron.

## LO QUE NO PUDE VERIFICAR

- Compilación real de los módulos y ejecución de los callbacks en DayZ.
- Estabilidad de descarga de baterías durante múltiples rondas reales del validador y transiciones de batería agotada/salida cerrada.
- Reducción efectiva de reencolados y tráfico de red frente al coste adicional de G01; falta perfil in-game.
- Convergencia de G18 con proveedores múltiples, capacidades desiguales, recuperación de sobrecarga y estados de compuerta cambiando entre ticks.
- Arranque, apagado, agotamiento y reinicio de fuentes vanilla con la sincronización cliente real.
- Compatibilidad con mods externos que sustituyan los callbacks sin encadenar `super`.
- Las advertencias del linter no equivalen a ejecución ni garantizan ausencia de problemas fuera de estas fichas.

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

- G02 no se arregla solamente sumando generación: el guard de existencia de aristas también excluía baterías desconectadas por su entrada. Además, `m_InputPower` no debe pasar a incluir almacenamiento.
- G01 demuestra una señal falsa en el código, pero no cuantifica paquetes ni tiempo perdido. El arreglo añade una pasada de comparación; la mejora neta necesita medición y no se declara conseguida por lectura.
- En G18 «proveedor activo» no puede significar únicamente arista ya alimentada: eso introduce circularidad durante recuperación. Se ha interpretado como nodo con potencia disponible para salida, incluso si la asignación anterior es cero. Es una decisión explícita que el revisor debe contrastar con las pruebas de convergencia.
- G04 no necesita un segundo llamador directo de `RefreshSourceState`: faltaba conectar eventos a la ruta existente. Modificar únicamente acciones es una cobertura menor que observar el estado de trabajo de la entidad.
- Hay otro verificador llamado `VerifyPassthroughPowered` (`scripts/5_Mission/LFPG_ElecGraphImpl.c:1843`) que solo mira aristas; su llamador localizado es `LFPG_PumpHelper.VerifyPowered` (`scripts/4_World/LFPG_PumpHelper.c:126`). No se cambió porque esta ficha trata el validador periódico de baterías; ampliar ese API a baterías requeriría revisar también ese contrato.
- La lista blanca literal omite el informe que a la vez exige. Se ha resuelto conservadoramente permitiendo solo `INFORME.md` como artefacto adicional.
