# Triaje de Grafo — 18 fichas P2/P3

| ID | Veredicto | Fichero principal | Motivo |
| --- | --- | --- | --- |
| G03 | VIVA | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:3315` | La reconstrucción del seguimiento solo comprueba almacén LFPG y entradas; omite las salidas de `m_VanillaWires`. |
| G05 | VIVA | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2005` | Un booleano global difiere broadcasts completos, deltas y vanilla mientras se sincroniza un jugador. |
| G06 | VIVA | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:5088` | CutAll programa un rebuild global que recrea también los nodos de baterías ajenas con generación virtual cero. |
| G07 | VIVA | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:4226` | La poda diferida elimina cables sin descontar creador; después reconstruye índices y seguimiento, pero no cuotas. |
| G08 | VIVA | `scripts/5_Mission/LFPG_ElecGraphImpl.c:3175` | La carga depende del barrido global y de que se vacíe la cola; el delta de carga se recorta a diez segundos. |
| G09 | VIVA | `scripts/5_Mission/LFPG_ElecGraphImpl.c:443` | El límite usa nodos actuales: puede rechazar crecimiento cero o admitir dos nodos cuando solo cabe uno. |
| G10 | VIVA | `scripts/5_Mission/LFPG_ElecGraphImpl.c:265` | Rebuild usa la inserción interna sin comprobar ciclo ni límites global/de componente; sí limita aristas por nodo. |
| G11 | VIVA | `scripts/5_Mission/LFPG_ElecGraphImpl.c:2074` | El retorno sin propagación no actualiza el tiempo, y el manager vuelve a acumular el último valor activo. |
| G12 | VIVA | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2069` | Tras el prefiltro vuelve a obtener jugadores, resolver destinos y calcular distancias para enviar. |
| G13 | VIVA | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2399` | Delta y vanilla generan JSON antes de obtener y filtrar receptores. |
| G14 | VIVA | `scripts/5_Mission/LFPG_ElecGraphImpl.c:2749` | Compacta mediante array nuevo, copia de pendientes y copia de vuelta a la cola original. |
| G15 | VIVA | `scripts/5_Mission/LFPG_ElecGraphImpl.c:137` | Persisten mapas separados para NetworkID y época de reencolado asociados al nodo; es deuda de estructura. |
| G16 | DUDOSA | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:1376` / `scripts/5_Mission/LFPG_ElecGraphImpl.c:1259` | Puede referirse a inserción deduplicada de owners o a hidratación repetida; son trabajos y agrupaciones distintos. |
| G19 | VIVA | `scripts/5_Mission/LFPG_ElecGraphImpl.c:3608` | Al encontrar una salida habilitada deja de leer candidatos, pero sigue iterando y consumiendo presupuesto. |
| G20 | VIVA | `scripts/5_Mission/LFPG_ElecGraphImpl.c:237` | Crea nodos para todo el registro, poda los aislados y vuelve a hidratar los supervivientes. |
| G21 | VIVA | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:1472` | Conserva owners retirados cuando quedan cables y la consulta legacy confía en ellos; no hay llamadores actuales localizados. |
| G22 | VIVA | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:492` | Si se activa el fallback de grafo, queda retenido y sus métodos periódicos emiten errores sin limitación de frecuencia. |
| G23 | VIVA | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2717` | `array.Remove(0)` sustituye el primer elemento por el último; la cola deja de ser FIFO. |

**Resultado:** 17 VIVA, 1 DUDOSA, 0 MUERTA, 0 NO-LOCALIZADA. VIVA significa que permanece el mecanismo descrito, incluida deuda de mantenimiento o riesgo condicional; no significa que se hayan reproducido 17 fallos de juego.

Revisión del 8 de septiembre de 2026 sobre `d59cad892557d8ec8dcfed0bfca0ba1c5744db45`. Todas las referencias relativas corresponden al código de este workspace, con numeración desde 1. Lectura estática de implementaciones, fachadas, consumidores y rutas de reconstrucción; sin compilador ni ejecución DayZ. No se deduce vigencia de fechas, nombres de commits ni ausencia de diff. No se ha declarado ninguna MUERTA, por lo que no hay bajas que fechar como «hoy» o «anteriores».

Se consultaron el `CLAUDE.md` de `LFPowerGrid_dev`, la referencia de Enforce y las checklists DayZ. El brief actual fija el alcance; la memoria de otras fases no lo sustituye. No se abrieron subagentes. Las rutas servidor se revisaron a través de las fachadas de `4_World` y las implementaciones de `5_Mission`.

### G03 — VIVA: se pierde el seguimiento de fuentes vanilla

**Evidencia:** `scripts/5_Mission/LFPG_NetworkManagerImpl.c:3354` vacía el seguimiento; `:3372` solo readmite dispositivos para los que `DeviceHasAnyWires` devuelve true. Esa función comprueba almacén LFPG en `:3315` y puertos IN en `:3324`, sin consultar `GetVanillaWires`. El almacenamiento vanilla separado existe en `:914`. `scripts/4_World/LFPG_IDevice.c:578` define la consulta de almacén LFPG, y `:867` clasifica el puerto de la fuente vanilla como OUT.

Una fuente vanilla que solo posee cables de salida queda fuera del barrido de movimiento/desaparición, que itera exclusivamente el conjunto seguido (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:3586`, `:3609`). Puede conservar cableado tras un movimiento o desaparición que debería activar esa limpieza; otros hooks pueden cubrir casos concretos, pero no reparan este conjunto.

**Contraste de llamadores:** la admisión de cable sí registra ambos extremos (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:651`), pero la reconstrucción posterior los vuelve a excluir (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:4043`, `:4403`). Los callbacks añadidos para G04 solo solicitan propagación (`scripts/4_World/LFPG_VanillaActionOverrides.c:206`), sin restaurar seguimiento.

**Coste: ACOTADO.** Completar el predicado de existencia de cables en el manager con su almacén vanilla. **Agrupación:** misma función `RebuildTrackedDevices` usada por G07; comparte `DeviceHasAnyWires` y el flujo CutAll con G06.

### G05 — VIVA: FullSync retiene las mutaciones de todos

**Evidencia:** `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2701` encola al jugador y `:2702` activa el mutex global. Lo consultan `BroadcastOwnerWires` (`:2005`), `BroadcastOwnerWireDelta` (`:2369`), `BroadcastVanillaWires` (`:2521`) y también `BroadcastOwnerSnapshot` (`:2285`); ninguna de esas decisiones distingue al jugador que está recibiendo FullSync.

Los demás clientes interesados esperan para recibir las mutaciones visuales/de caché mientras avanza la sincronización individual. No implica que el servidor deje de simular electricidad: se retrasa la publicación de cables.

**Límite del daño:** `LFPG_StartNextFullSync` baja el mutex y vacía lo diferido antes de seleccionar al siguiente jugador (`:2712`, `:2713`), por lo que no retiene necesariamente durante toda la cola. Cada jugador se procesa por lotes (`:2819`, `:2827`), y la validación activa suspende ese avance (`:2802`).

**Coste: AMPLIO.** Separar retención por destinatario exige revisar el contrato de ordenación entre snapshots, deltas y sus consumidores, además de las distintas rutas de envío. **Agrupación:** G23 comparte `LFPG_StartNextFullSync`; G12 y G13 comparten las funciones de broadcast.

### G06 — VIVA: CutAll reconstruye baterías de otros componentes

**Evidencia:** si hubo cambios, `scripts/5_Mission/LFPG_NetworkManagerImpl.c:5086` activa `m_CutGraphRebuildQueued` y `:5088` programa `PostBulkRebuildAndPropagate`. El callback fuerza `m_Graph.PostBulkRebuild(this)` para ese caso (`:1119`, `:1121`). `scripts/5_Mission/LFPG_ElecGraphImpl.c:202` borra todos los nodos, y `:1236` crea instancias nuevas.

Las baterías ajenas pierden sus valores transitorios `m_VirtualGeneration` y `m_SoftDemand`, inicializados a cero en `scripts/3_Game/LFPG_Data.c:273`. La hidratación posterior no los recupera (`scripts/5_Mission/LFPG_ElecGraphImpl.c:3327`), por lo que una red alimentada solo por batería puede quedar temporalmente sin suministro representado en el grafo, aunque el corte se haya producido en otro componente.

**Recuperación real:** las escrituras de esos campos están en `scripts/5_Mission/LFPG_NetworkManagerImpl.c:7784`, dentro del tick de baterías activado cada cinco pasos de dispositivos simples (`:6914`); esos pasos se programan cada segundo (`:681`). El intervalo nominal de recuperación puede llegar a unos cinco segundos más convergencia. No se ha demostrado pérdida de la energía persistida en la entidad. La coalescencia de varios CutAll en un callback reduce rebuilds, pero conserva el rebuild global.

**Coste: AMPLIO.** Preservar/restaurar estado operativo o sustituir el rebuild por mutación del componente obliga a coordinar manager, grafo y ciclo de batería. **Agrupación:** G20 comparte `RebuildFromWires`/`PostBulkRebuild`; G03 y G07 comparten las rutas de mantenimiento del manager.

### G07 — VIVA: cuotas infladas después de la poda diferida

**Evidencia:** `scripts/5_Mission/LFPG_NetworkManagerImpl.c:4195` contabiliza para poda un owner desaparecido; `:4226` elimina un cable con destino irresoluble, y `:4244` borra los owners vacíos. No hay descuento por creador en esas ramas. Su llamador ejecuta la poda en `:4337` y acaba reconstruyendo índice inverso y seguimiento en `:4401` y `:4403`, sin recuento de jugadores.

Un jugador puede seguir consumiendo cuota por cables que ya no existen y recibir «MaxWiresPerPlayer reached»: la autorización lee el contador en `:1753` y rechaza en `:1755`; el RPC consume esa decisión en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:476`. La poda LFPG del mismo callback tampoco lo arregla: pasa por `scripts/4_World/LFPG_WireOwnerBase.c:253` a `scripts/3_Game/LFPG_WireHelper.c:267`, que elimina filas sin conocer el contador.

**Contraste de reparación externa:** sí existe `RecountAllPlayerWires` (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:1505`), pero su único llamador actual está en la fase de reconstrucción de índices de la validación (`:3981`), anterior a la poda diferida programada en `:3993`. Otra validación posterior puede reparar la cuota; este callback no la solicita al terminar.

**Coste: TRIVIAL.** Para cerrar esta ficha mediante el recuento ya existente, basta una llamada después de que finalicen las podas, en el bloque de reconstrucción del callback. **Agrupación:** comparte `DeferredVanillaPruneAndRebuild` con G03, G06 y G20; el coste de recuento global merece medirse, pero no exige otro contrato.

### G08 — VIVA: el cargador depende de la actividad global

**Evidencia:** `ValidateConsumerStates` solo se invoca con la cola vacía o recién drenada (`scripts/5_Mission/LFPG_ElecGraphImpl.c:2071`, `:2745`). Recorre un lote del mapa global de nodos (`:2926`, `:2939`, `:2957`), con gate temporal (`:2932`) y presupuesto de aristas (`:2950`). La carga efectiva de BatteryCharger está dentro de ese barrido (`:3147`, `:3197`).

Una cola global que no termina de drenarse retrasa la carga incluso en un cargador estable de otro componente. Además, cuando pasa más de diez segundos entre visitas, el delta se recorta a diez (`:3175`) y se actualiza el timestamp a «ahora» (`:3213`), descartando el resto del intervalo.

**Escala verificable:** las constantes actuales son 32 nodos por lote y diez llamadas entre lotes (`scripts/3_Game/LFPG_Defines.c:490`, `:486`), con propagación nominal de 100 ms (`:506`). Una traza ideal, sin llamadas extra ni presión de presupuesto, de 2.048 nodos visitaría cada cargador aproximadamente cada 64 segundos y acreditaría como máximo diez. Es una consecuencia aritmética del scheduling, no una medición de servidor. El delta ya existente compensa visitas espaciadas hasta el tope; no elimina la dependencia.

**Coste: AMPLIO.** Dar a la carga un ciclo propio requiere separar mantenimiento energético y validación del grafo, conservando altas, bajas, timestamps y comprobación de suministro. **Agrupación:** comparte `ProcessDirtyQueue`/`ValidateConsumerStates` con G11 y el entorno de G14; comparte metadatos del grafo con G15.

### G09 — VIVA: límite global sobre el estado previo

**Evidencia:** `scripts/5_Mission/LFPG_ElecGraphImpl.c:443` y `:1687` comparan `m_NodeCount >= LFPG_MAX_NODES_GLOBAL` antes de calcular si existen los extremos (`:455`, `:1694`). Con ambos extremos nuevos, el precheck sale sin rechazo en `:1698`; la inserción crea ambos en `:615` y `:616`. El máximo es 2.048 (`scripts/3_Game/LFPG_Defines.c:479`).

Con 2.047 nodos y dos extremos nuevos se puede pasar a 2.049; con 2.048, una conexión válida entre nodos existentes se rechaza aunque no aumente el total. Los controles por componente no corrigen estos dos errores globales.

**Contraste de llamador:** el RPC sí hace precheck (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:534`), pero termina en el mismo cálculo defectuoso mediante `scripts/5_Mission/LFPG_NetworkManagerImpl.c:982`.

**Coste: ACOTADO.** Ajustar coherentemente precheck y admisión en el fichero de grafo. **Agrupación:** G10 afecta al contrato de límites; G16 puede incluir la duplicación entre predicados, aunque su descripción no lo identifica.

### G10 — VIVA: la reconstrucción elude límites globales y ciclos

**Evidencia:** `scripts/5_Mission/LFPG_ElecGraphImpl.c:265` y `:290` incorporan wires persistidos mediante `AddEdgeInternal`, sin pasar por `OnWireAdded`. La inserción interna valida IDs/resolución (`:1317`, `:1323`, `:1336`), grado de salida/entrada (`:1349`, `:1360`) y duplicados (`:1378`); después crea la arista (`:1386`) sin comprobar ciclo ni tamaño global/de componente.

Un almacén persistido con un ciclo o un componente excesivo puede reconstruir una topología que la conexión normal rechazaría. Sigue siendo un riesgo condicionado a esos datos; no se ha localizado una muestra real afectada ni probado que el solver quede bloqueado.

**Contraste de rutas:** la validación arranca el rebuild en `scripts/5_Mission/LFPG_NetworkManagerImpl.c:4004`; las podas leídas validan resolución de endpoints, no aciclicidad. El control de ciclos sí existe para nuevas conexiones (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:546`, `scripts/5_Mission/LFPG_ElecGraphImpl.c:1054`). `RebuildComponents` asigna componentes con marca de visitado (`:1166`, `:1169`); no es un detector de ciclos ni rechaza por tamaño.

**Coste: AMPLIO.** La validación del rebuild puede concentrarse en el grafo, pero un arreglo completo debe definir qué ocurre con las filas persistidas rechazadas y con la topología publicada por el manager. **Agrupación:** G09 comparte invariantes de admisión; G06 y G20 comparten `RebuildFromWires`.

### G11 — VIVA: tiempo de propagación obsoleto en ticks inactivos

**Evidencia:** `scripts/5_Mission/LFPG_ElecGraphImpl.c:2074` retorna antes de iniciar el cronómetro (`:2077`) y antes de la única actualización operativa de `m_LastProcessMs` (`:2766`). El manager lee y suma ese valor en cada tick (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:3159`, `:3163`) y lo usa en la media (`:3184`).

Tras un tick activo de coste no nulo, cada tick sin propagación vuelve a sumar aquel coste. También queda sin medir el trabajo real de validación que sí puede ocurrir en la rama de cola vacía; no es correcto considerar toda esa rama de coste cero.

**Matiz:** las aristas sí se reinician al entrar (`scripts/5_Mission/LFPG_ElecGraphImpl.c:2036`); no se confirma repetición del contador de aristas. Los logs de esta telemetría están tras nivel 2 (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:3189`), superior al nivel 1 actual (`scripts/3_Game/LFPG_Defines.c:400`).

**Coste: ACOTADO.** Medir todas las salidas del procesamiento en el grafo, incluyendo validación. **Agrupación:** misma función que G14 y planificación de G08; consumidor en el manager.

### G12 — VIVA: prefiltro y envío recalculan el mismo interés

**Evidencia:** `BroadcastOwnerWires` obtiene jugadores en `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2013`, recopila posiciones objetivo en `:2030` y busca un interesado desde `:2038`. Tras serializar, vuelve a obtener jugadores (`:2070`), leer posición del owner (`:2092`), resolver objetivos (`:2107`) y evaluar interés (`:2116`).

Cuando existe algún receptor, se repite trabajo del prefiltro para construir el envío. No son necesariamente dos barridos completos de todos los jugadores: el primero termina al hallar uno; con cero interesados evita serialización y envío correctamente.

**Coste: ACOTADO.** Reutilizar la información y los receptores de esa llamada, manteniendo el filtro por ambos extremos y la identidad. **Agrupación:** misma función que G05; funciones vecinas y mismo criterio de interés que G13. Puede solaparse con la lectura de G16 sobre helpers de broadcast.

### G13 — VIVA: serialización anterior a la selección de receptores

**Evidencia:** el delta crea blobs y JSON por entrada en `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2395` y `:2399`, antes de obtener jugadores (`:2414`) y filtrarlos (`:2453`). Vanilla construye/serializa su blob en `:2539` y `:2552`, antes de `GetPlayers` (`:2559`) y del filtro (`:2590`).

Ambas rutas pagan serialización y asignaciones aunque no haya ningún destinatario, o todos estén lejos. El prefiltro de `BroadcastOwnerWires` (`:2011`) solo protege esa otra ruta; no convierte esta ficha en MUERTA.

**Coste: ACOTADO.** Seleccionar interés antes de serializar en las dos funciones del manager. **Agrupación:** G05 comparte ambas funciones; G12 comparte buffers y criterios de destinatarios. Los deltas deben conservar el interés por destinos retirados (`:2435`).

### G14 — VIVA: doble copia al compactar la cola

**Evidencia:** `scripts/5_Mission/LFPG_ElecGraphImpl.c:2749` crea `compacted`; `:2753` copia los pendientes; `:2755` limpia la cola original y `:2759` copia los elementos de vuelta. El disparador es superar el umbral con pendientes (`:2747`).

La cola ya dispone de índice de cabeza, pero su compactación añade una colección temporal y dos recorridos de los mismos pendientes. Es coste evitable, sin fallo funcional demostrado ni medida de su importancia en una partida.

**Coste: ACOTADO.** Ajustar la compactación conservando orden y semántica de referencias. **Agrupación:** `ProcessDirtyQueue`, compartida con G11 y con las condiciones de ejecución de G08.

### G15 — VIVA: metadatos del nodo en mapas paralelos

**Evidencia:** el grafo declara `m_RequeueEpoch` en `scripts/5_Mission/LFPG_ElecGraphImpl.c:62` y `m_NodeNetLow`/`m_NodeNetHigh` en `:137` y `:138`. `EnsureNode` llena las dos mitades del ID (`:1251`); `EnsureRequeueEpoch` consulta el mapa, cambia el nodo y escribe el mapa (`:3920`, `:3923`, `:3924`). La estructura del nodo ya contiene estado de reencolado (`scripts/3_Game/LFPG_Data.c:264`).

Permanece la separación de escalares que describen al mismo nodo, con búsquedas y limpieza coordinada adicionales (`scripts/5_Mission/LFPG_ElecGraphImpl.c:949`). Esto confirma la oportunidad de mantenimiento, no un bug de jugador por el mero uso de mapas.

**Coste: AMPLIO.** Mover esos metadatos al nodo cruza `LFPG_Data.c` y la implementación de grafo, y exige conservar el fallback de resolución durante limpieza (`scripts/5_Mission/LFPG_ElecGraphImpl.c:1584`). No incluyo automáticamente el mapa de timestamps de cargadores: su ciclo de vida puede diferir del nodo reconstruido, y moverlo sin comprobarlo cambiaría comportamiento.

**Agrupación:** G14/G11 comparten el procesamiento; G06/G20 comparten creación y borrado de nodos. No convertir esta ficha en una reestructuración indiscriminada de todos los mapas.

### G16 — DUDOSA: no se identifica qué duplicaciones forman la ficha

La descripción no proporciona símbolos ni delimita los bloques; el registro incluido en `reviews/2026-09-06-council-auditorias/BRIEF.md:546` repite ese mismo título. Las búsquedas por `G16`, el título y referencias de duplicación no han localizado una enumeración original que permita decidir entre estas dos lecturas:

- **Lectura A, índice inverso del manager:** `scripts/5_Mission/LFPG_NetworkManagerImpl.c:1271` ya define `ReverseOwnersInsert`, mientras `ReverseIdxAdd` vuelve a implementar asignación y deduplicación de la misma lista en `:1376`. Si esta es la ficha, el trabajo es reutilizar ese helper dentro del manager; comparte fichero y estructuras con G21.
- **Lectura B, hidratación del grafo:** `scripts/5_Mission/LFPG_ElecGraphImpl.c:1259` y `:3296` repiten selección SOURCE/PASSTHROUGH/CONSUMER, capacidad, estado de fuente y consumo. Si esta es la ficha, comparte funciones y buena parte del alcance con G20; además las ramas no son idénticas, porque `EnsureNode` inicializa compuerta en `:1294`.

Ambas duplicaciones existen, pero no asigno arbitrariamente a G16 un paquete completo de refactor. **Coste: ACOTADO por lectura; AMPLIO si pretende agrupar ambas.** No hay un síntoma de jugador propio demostrado para G16, ni razón para contabilizar otra vez G20.

### G19 — VIVA: el booleano sigue consumiendo visitas tras acertar

**Evidencia:** en `AllocateOutput`, `scripts/5_Mission/LFPG_ElecGraphImpl.c:3608` recorre todas las salidas del PASSTHROUGH. Cada iteración incrementa `m_EdgesVisitedThisEpoch` (`:3610`), aunque `ptHasDown` ya sea true (`:3611`); al encontrar una salida habilitada lo activa (`:3616`) sin terminar el bucle.

Se deja de leer el contenido de las aristas restantes, pero se siguen ejecutando iteraciones y cargando visitas al presupuesto. El resultado booleano no cambia; puede agotarse presupuesto antes de lo necesario en el fallback de arranque en frío.

**Coste: TRIVIAL.** Un corte del bucle al establecer true cierra este recorrido, conservando la visita que sí se hizo. **Agrupación:** mismo fichero y función de reparto que los cambios recientes G01/G18, ajenos al lote; interacción con el presupuesto de G08/G14. G16 podría absorberlo solo si su alcance original lo explicitase.

### G20 — VIVA: reconstrucción con nodos desechados e hidratación duplicada

**Evidencia:** `scripts/5_Mission/LFPG_ElecGraphImpl.c:224` obtiene todos los dispositivos registrados y `:237` crea nodo para cada uno. Después identifica aislados (`:311`) y los elimina (`:318`). `EnsureNode` ya hidrata propiedades eléctricas (`:1259`), pero `PostBulkRebuild` vuelve a llamar a la hidratación completa inmediatamente después del rebuild (`:1912`, `:1913`).

El trabajo depende de todos los dispositivos registrados aunque muchos no formen parte del grafo cableado, y los supervivientes repiten parte de las lecturas eléctricas. La validación por fases también hace ambas operaciones (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:4004`, `:4017`), aunque allí ocurren en fases distintas y no necesariamente en la misma llamada.

**Coste: AMPLIO para cerrar las dos mitades en todas las rutas.** Crear solo nodos necesarios puede concentrarse en el grafo; eliminar la hidratación redundante requiere revisar sus llamadores del manager y preservar cualquier actualización necesaria entre fases. No basta borrar `PopulateAllNodeElecStates` por semejanza textual.

**Agrupación:** G06 comparte `PostBulkRebuild`; G10 comparte la admisión desde `RebuildFromWires`; G16 lectura B es solapamiento directo, y G15 cruza el ciclo de vida del nodo.

### G21 — VIVA: consulta legacy basada en owners obsoletos

**Evidencia:** `scripts/5_Mission/LFPG_NetworkManagerImpl.c:1472` reduce el contador al retirar un cable, pero conserva el array de owners mientras quede alguno (`:1473`). `IsPortTargetedByPoweredSource` usa ese array (`:1327`) y devuelve true si uno de sus owners está encendido (`:1353`, `:1355`), sin verificar que aún tenga un cable a ese puerto.

Hay una ruta actual que produce esa lista obsoleta sin requerir dos cables finales sobre el mismo puerto: la sustitución incorpora primero el owner nuevo al índice (`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:621`) y luego retira el viejo (`:645`). Con A encendido retirado y B apagado como único cable final, queda contador 1 con owners A/B, y una llamada a la API legacy puede devolver true por A hasta reconstruir el índice.

**Alcance real:** la API devuelve un booleano incorrecto, no una colección de propietarios. Una búsqueda de `IsPortTargetedByPoweredSource` en `scripts/` solo encuentra la fachada y su implementación; los dispositivos lógicos actuales usan `IsPortReceivingPower` (por ejemplo `scripts/4_World/LFPG_LogicGate.c:136`), que consulta aristas/asignaciones (`scripts/5_Mission/LFPG_ElecGraphImpl.c:3959`, `:3973`). No se ha demostrado un efecto actual de juego por un consumidor de la API legacy.

**Coste: ACOTADO.** Corregir el contrato de consulta o verificar propiedad vigente en el manager; depurar la lista inversa debe respetar que un owner puede tener más de una conexión. **Agrupación:** G16 lectura A comparte `ReverseIdxAdd` y los owners; G07 comparte reconstrucción de índices, pero su contador es distinto.

### G22 — VIVA, condicional: fallback de grafo permanente y ruidoso

**Evidencia:** el constructor del manager consulta la fábrica en `scripts/5_Mission/LFPG_NetworkManagerImpl.c:487` y, si no obtiene grafo, conserva `new LFPG_ElecGraph()` (`:492`). Las búsquedas de asignaciones a `m_Graph` y de `LFPG_CreateElecGraph` no muestran un intento posterior de sustitución. La instancia base es no nula, de modo que supera el guard de `TickPropagation` (`:3117`).

Si entra en esa rama, no hay recuperación posterior del solver y las llamadas periódicas caen en stubs que emiten errores, como `scripts/4_World/LFPG_ElecGraph.c:192`, `:210`, `:226` y `:269`. `LFPG_Util.Error` imprime sin rate limit (`scripts/3_Game/LFPG_Util.c:7`, `:14`), con logging actualmente activado (`scripts/3_Game/LFPG_Defines.c:399`).

**Condición pendiente:** la misión servidor normal sí proporciona el grafo real (`scripts/5_Mission/LFPG_MissionInit.c:75`) y el manager real (`:77`); no se ha demostrado que la fábrica de grafo falle en ese arranque. El fallback del singleton manager es otro mecanismo: `scripts/4_World/LFPG_NetworkManager.c:54` reintenta la fábrica y guarda el stand-in separado (`:66`). Esa reparación no sustituye un grafo base ya retenido dentro de un manager real.

**Coste: AMPLIO.** Recuperar la instancia exige reconstruir su estado y coordinar inicialización/tick, además de limitar el diagnóstico; silenciar logs por sí solo no restaura simulación. **Agrupación:** comparte `TickPropagation` con G11 y rutas de reconstrucción con G06/G10/G20. No se prioriza como incidente ordinario sin demostrar activación.

### G23 — VIVA: FullSync no respeta FIFO

**Evidencia:** `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2701` añade al final, pero `LFPG_StartNextFullSync` toma el índice cero (`:2716`) y usa `Remove(0)` (`:2717`). La definición vanilla local documenta que `array.Remove` rellena el hueco con el último elemento: `C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts/1_core/proto/enscript.c:457`; la firma está en `:463`. `RemoveOrdered`, que conserva el orden, está definido en `:470`.

Con pendientes A/B/C/D, se atiende A/D/C/B, no A/B/C/D. Las nuevas entradas pueden prolongar la espera de quienes quedan en posiciones intermedias; no se afirma inanición observada con una cola finita sin nuevas llegadas.

**Coste: TRIVIAL.** Usar la retirada ordenada en esa cola, si se mantiene el contrato FIFO. **Agrupación:** misma función y estado FullSync que G05; arreglar el orden no elimina su mutex global.

## LAS QUE RECOMIENDO ATACAR PRIMERO

1. **G06:** un corte local puede interrumpir redes de batería de otros jugadores. El alcance global y la ventana nominal hasta el siguiente tick de baterías son más dañinos que las microoptimizaciones.
2. **G03:** perder seguimiento de una fuente vanilla cableada deja sin cobertura de polling los movimientos/desapariciones que deberían invalidar conexiones. Reparar G04 no cubre ese problema de integridad del cableado.
3. **G08:** la carga de baterías depende de redes ajenas y pierde tiempo acreditable cuando supera el tope; afecta a una función visible y sostenida del jugador.
4. **G07:** puede impedir colocar nuevos cables pese a haber liberado cuota. El motivo de prioridad es el bloqueo de uso, aunque el arreglo mínimo sea pequeño.
5. **G05:** entrar o resincronizar un jugador retrasa las actualizaciones de cableado de otros; el tamaño del mundo alarga la espera. Atacar G23 junto con este trabajo es razonable, pero no sustituye resolver el bloqueo global.

## LO QUE NO PUDE VERIFICAR

- Compilación de Enforce ni ejecución cliente/servidor; todos los veredictos son estáticos.
- Frecuencia real, duración de interrupciones, volumen de RPC, consumo de CPU y pausas de memoria de las fichas de coste.
- Cuánto cubren otros hooks de entidades y otros mods los movimientos/desapariciones excluidos por G03.
- Duración y efecto visual exactos del estado de batería perdido en G06 bajo distintos órdenes de scheduler y sincronización.
- Tiempos efectivos de carga G08 bajo actividad continua, llamadas extraordinarias al solver y presupuestos agotados.
- Existencia de persistencia real con ciclos o tamaños ilegales que active G10; tampoco la convergencia del solver sobre esos datos.
- Enumeración original de duplicaciones de G16: se localizaron dos lecturas concretas, no un alcance unívoco.
- Consumidores externos o dinámicos de la API legacy G21; no hay llamadas textuales en el árbol actual.
- Activación del fallback de grafo G22 con la misión servidor actual; se demuestra lo que ocurre si se activa, no que haya ocurrido.
- Impacto de reordenación G23 en conexiones simultáneas reales; su semántica se contrastó con la definición vanilla local.

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

- **No son 18 bugs homogéneos.** G12–G15, G19 y buena parte de G20 son trabajo evitable o estructura de código. «Confirmado» en la auditoría original incluía oportunidades de mantenimiento (`reviews/2026-09-06-council-auditorias/BRIEF.md:411`); VIVA no demuestra un daño perceptible ni obliga a refactorizar.
- **G16 está insuficientemente especificada y se solapa con otras fichas.** La hidratación repetida ya está en G20 y el broadcast en G12/G13. No sumar sus costes como si fueran fallos independientes.
- **G10 exagera al decir que no hay límites de tamaño.** Sí existen límites de aristas por nodo y deduplicación; faltan específicamente el máximo global, el máximo de componente y el rechazo de ciclos en reconstrucción. La severidad P2 depende de poder encontrar/importar esos datos, no de asumir un bloqueo del servidor.
- **G21 describe mal el retorno y no prueba un consumidor afectado.** No devuelve owners: puede devolver true por un owner retirado. La sustitución de cables actual mantiene el escenario de lista obsoleta, pero la API está sin llamadores locales. P3 como deuda de contrato parece más ajustado que tratarla como fallo activo de suministro.
- **G22 no demuestra el fallo de arranque.** Su rama defensiva sigue mal recuperada, pero el flujo normal tiene fábrica; si se demuestra su activación y degradación de toda la simulación, la severidad podría superar P2. No se eleva a P1 basándose solo en la existencia del fallback.
- **G06 no es pérdida de energía persistida.** Se pierde estado transitorio del grafo. La coalescencia de CutAll evita rebuilds repetidos dentro del lote, pero no conserva los nodos ajenos. La distinción cambia tanto el daño esperado como el tipo de arreglo.
- **G08 ya usa delta temporal, pero con tope.** El problema no se resuelve añadiendo otro delta: hay que corregir la dependencia del barrido global y decidir la política para intervalos no visitados.
- **G11 afecta al tiempo, no a todos los contadores, y el logging detallado está desactivado al nivel actual.** Mantendría prioridad inferior a las cinco primeras; no es por sí misma una degradación de gameplay demostrada.
- **Las modificaciones de hoy no implican bajas automáticas.** El hook G04 no restaura seguimiento G03; el validador G02 consume generación virtual pero no la rehidrata tras G06; la corrección del reparto G01 mantiene el recorrido G19. Las rutas de autoridad y difusión actuales también conservan el mutex G05 y la cola G23. Se comprobó cada mecanismo en el árbol final.

## Cierre y límites de entrega

Único artefacto creado por esta lane: `TRIAJE.md`. No se modificó código ni se ejecutaron commit, staging, checkout, stash o reset. Comprobación documental: 18 IDs únicos en tabla y 18 secciones, apartados obligatorios presentes y rutas/líneas completas existentes. `git diff --exit-code` y `git diff --cached --exit-code` terminaron con código 0 y sin diff. Son verificaciones del entregable y del alcance, no pruebas del mod.

No se escribió memoria externa ni handoff en el vault: la frontera explícita del encargo permite escribir únicamente este informe, que contiene la evidencia y los pendientes para el receptor. Los archivos de infraestructura preexistentes se conservaron.
