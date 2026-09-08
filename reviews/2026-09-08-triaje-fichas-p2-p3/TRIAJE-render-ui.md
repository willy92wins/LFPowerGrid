# TRIAJE — Render/UI, 27 fichas P2/P3

| ID | Veredicto | Fichero principal | Por qué |
|---|---|---|---|
| R01 | VIVA | `scripts/4_World/LFPG_CableRenderer.c` | `BuildOccSamples()` se ejecuta antes de llenar `cachedJoints`; la rama de esquinas recibe cero joints. |
| R03 | VIVA | `scripts/4_World/LFPG_CableRenderer.c` | La purga de owners no elimina ni reconstruye sus entradas en las dos cachés derivadas. |
| R05 | VIVA | `scripts/4_World/LFPG_CableRenderer.c` | Sigue el selection sort cada 2 s; hoy se excluyen entradas sin segmentos, pero siguen entrando cables ocluidos o detrás de cámara. |
| R06 | VIVA | `scripts/4_World/LFPG_CableRenderer.c` | Un snapshot cambiado o un delta aceptado vacían y reconstruyen el índice de todos los owners. |
| R07 | VIVA | `scripts/4_World/LFPG_CableRenderer.c` | Aplicar un delta destruye todas las líneas del owner y vuelve a construirlas. |
| R08 | VIVA | `scripts/4_World/LFPG_CableRenderer.c` | A más de 75 m del owner se ocultan todos sus cables sin comprobar la proximidad al destino. |
| R09 | VIVA | `scripts/4_World/LFPG_CableRenderer.c` | `HasRenderableWires()` cuenta entradas, incluidas las que hoy conservan metadatos con cero segmentos. |
| R10 | VIVA | `scripts/4_World/LFPG_CableRenderer.c` | Las tres claves de proyección comparan posición/dirección/tamaño, pero no FOV ni rotación alrededor del eje de visión. |
| R11 | VIVA | `scripts/4_World/LFPG_CableRenderer.c` | Se compara con el frame anterior y se reemplaza la referencia incluso sin superar el umbral. |
| R12 | VIVA | `scripts/4_World/LFPG_DeviceInspector.c` | Tras una respuesta, no se renueva la información de aristas mientras no cambie el dispositivo o su generación local de topología. |
| R13 | VIVA | `scripts/4_World/LFPG_DeviceInspector.c` | `PopulateWireData()` vuelve a posicionar cabecera/filas omitiendo los offsets de sorter y batería. |
| R14 | VIVA | `scripts/4_World/LFPG_Camera.c` | Los hooks de cualquier cámara llaman a `SafeAbort()` sin identificar la entidad de la sesión activa. |
| R16 | VIVA | `scripts/5_Mission/LFPG_TankHUD.c` | La caché negativa de proximidad caduca a los 250 ms y repite la consulta del escenario. |
| R17 | VIVA | `scripts/4_World/LFPG_SearchlightController.c` | En una sesión activa se llama a la predicción visual cada frame, aunque los ángulos no cambien. |
| R18 | VIVA | `scripts/4_World/LFPG_LaserBeamRenderer.c` | Cada alta nueva hace una búsqueda lineal y ejecuta `CullTick()` sobre todo el registro. |
| R19 | VIVA | `scripts/4_World/LFPG_LaserBeamRenderer.c` | La lista de candidatos dibujables incorpora el frustum solamente cada 250 ms. |
| R20 | VIVA | `scripts/4_World/LFPG_CableRenderer.c` | El receptor de snapshots asigna generaciones antiguas sin el guard de monotonía que sí tienen los deltas. |
| R21 | VIVA | `scripts/4_World/LFPG_CableRenderer.c` | El envío retira la petición pendiente; encolar no consulta las peticiones pendientes de respuesta. |
| R22 | VIVA | `scripts/4_World/LFPG_CableRenderer.c` | Hay helpers sin llamadores y estado sin consumidor; el parche de hoy dejó además `EstimateSegments()` huérfano. |
| R23 | VIVA | `scripts/4_World/LFPG_CableParticle.c` | Cada subsegmento sigue siendo una instancia referenciada con dos extremos y un booleano. |
| R24 | VIVA | `scripts/4_World/LFPG_CableRenderer.c` | Persisten preparaciones duplicadas de geometría, interpolación de sag y orientación de focos. |
| R25 | MUERTA | `scripts/4_World/LFPG_CableRenderer.c` | Los logs habituales se construyen dentro de guards de debug; ya estaba resuelta antes de hoy. |
| R26 | DUDOSA | `scripts/4_World/LFPG_CableRenderer.c` | Hoy se corrigieron admisión y distancia inicial; permanece la visibilidad/oclusión inicial optimista. La ficha mezcla ambas cosas. |
| R27 | VIVA | `scripts/4_World/LFPG_CableHUD.c` | El retorno por 0×0 no invalida `IsReady()` ni impide las llamadas posteriores de los productores. |
| R28 | VIVA | `scripts/4_World/LFPG_DeviceInspector.c` | Se repiten lecturas entre snapshot y población; los mapas de widgets siguen siendo genéricos. No se confirma que todo se formatee dos veces. |
| R29 | VIVA | `scripts/4_World/LFPG_CameraViewport.c` | Las etiquetas se colocan en `InitWidgets()` una sola vez; `DrawOverlay()` sí detecta nuevas resoluciones. |
| R30 | VIVA | `scripts/5_Mission/LFPG_MissionInit.c` | `OnInit()` fija los dos niveles globales de brillo sin capturar ni restaurar el estado anterior. |

**Resultado: 25 VIVA, 1 MUERTA, 1 DUDOSA, 0 NO-LOCALIZADA.** Revisión del árbol con HEAD `d59cad892557d8ec8dcfed0bfca0ba1c5744db45`, el 2026-09-08. Todas las rutas relativas y líneas corresponden a esta copia, no a las auditorías antiguas.

El alcance es exclusivamente lectura y triaje. `VIVA` significa que está presente el mecanismo descrito, con las precisiones de cada ficha; no significa reproducción en DayZ ni coste medido. Las antiguas etiquetas «Confirmado/Potencial» no se usaron como prueba. Se siguieron llamadores, consumidores, invalidaciones y guards. La historia de Git se consultó después de leer el código, para fechar correcciones, no para inferir vigencia.

No se encontró `CLAUDE.md` en esta copia ni en la búsqueda de `LFPG_lanes`; se aplicaron el brief y las instrucciones aportadas. Se leyeron las referencias locales de Enforce/UI y la checklist DayZ; ante comentarios históricos se dio prioridad a la implementación y a las declaraciones nativas consultadas. No se inició el juego, no hubo compilación ni modificación de código.

## Mapa de solapamientos para agrupar trabajo

| Grupo | Ficheros y fichas relacionadas |
|---|---|
| Cables | `LFPG_CableRenderer.c`: R01, R03, R05, R06, R07, R08, R09, R10, R11, R20, R21, R22, R23, R24, R25 y R26. R01/R07/R23/R24/R26 coinciden en construcción; R05/R09/R10/R11 en dibujo/oclusión; R03/R06/R20/R21 en estado/sync. |
| Inspector | `LFPG_DeviceInspector.c`: R12, R13 y R28. Comparten `Tick()` y/o `PopulateClientData()`; R12/R13 comparten `PopulateWireData()`. |
| CCTV | `LFPG_CameraViewport.c`: R14, R22 y R29; R14 también toca los hooks de `LFPG_Camera.c`. |
| Láser | `LFPG_LaserBeamRenderer.c`: R18 y R19 comparten `CullTick()`. |
| Foco | `LFPG_SearchlightController.c` y `LFPG_Searchlight.c`: R17; la duplicación de orientación de este último también forma parte de R24. |
| Canvas/misión | R09 y R27 comparten `LFPG_CableHUD.c` y el despacho en `LFPG_MissionInit.c`. R30 está en el `OnInit()` de ese mismo fichero; R16 y R17 tienen allí sus llamadores periódicos. |

Los costes siguientes son estimaciones de alcance de la corrección, no implementaciones aprobadas: **TRIVIAL** = una línea; **ACOTADO** = un fichero; **AMPLIO** = varios ficheros o cambio de contrato. No se recomienda mezclar todas las fichas de un fichero en un único parche.

## Evidencia por ficha viva o dudosa

### R01 — VIVA — Waypoints ausentes al construir muestras de oclusión

**Evidencia:** `scripts/4_World/LFPG_CableRenderer.c:351` exige `cachedJoints.Count() > 0` para entrar en la rama de esquinas. `BuildWire()` crea una instancia nueva en `:2242`, llama a `BuildOccSamples()` en `:2288` y llena los joints después, en `:2299`. La búsqueda de `BuildOccSamples(` en `scripts/` solo devuelve su definición y esa llamada.

La rama existe pero su entrada está vacía cuando se evalúa. Los cables con giros reciben el muestreo de la rama recta, con riesgo de ocultar una porción visible cerca de una esquina; la existencia del defecto de orden es concluyente, el resultado de los raycasts depende de la escena.

**Coste:** TRIVIAL: reubicar la llamada de una línea después de rellenar los joints. **Comparte:** grupo Cables; la misma `BuildWire()` con R23/R26 y el ámbito de construcción de R07/R24. El parche de hoy añadió admisión y distancia inicial, pero no cambió este orden.

### R03 — VIVA — Purga incompleta de cachés derivadas

**Evidencia:** `scripts/4_World/LFPG_CableRenderer.c:2633` destruye líneas, `:2634` limpia reintentos y `:2635` borra el owner; el bloque termina sin invalidar las cachés. `RebuildConnCache()` es quien limpia ambas (`:1978`, `:1979`) y recorre owners/aristas (`:1982`, `:2001`). Sus llamadas actuales están en el snapshot cambiado (`:1336`) y en el delta (`:1875`), no en la purga.

Después de retirar localmente un owner pueden quedar tipos de conexión y dispositivos conocidos hasta otra reconstrucción global. Es información consumida: `scripts/4_World/LFPG_Actions.c:570` la usa para mostrar «Replace» en `:575`, y `LFPG_CableRenderer.c:1510` usa dispositivos conocidos para omitir sync cuando no existe generación anunciada; `LFPG_DeviceLifecycle.c:80` solo retira la entidad del registro y no cura estas cachés del renderer.

La ruta demostrada es la purga local por ausencia, que también cubre streaming; no se afirma que toda eliminación en servidor deje siempre conexiones viejas, pues un snapshot/delta de corte puede haberlas actualizado antes.

**Coste:** TRIVIAL para la corrección mínima: llamar a la reconstrucción tras el lote de borrados. El mantenimiento incremental dentro del renderer sería ACOTADO y pertenece a R06. **Comparte:** grupo Cables, especialmente R06 (`RebuildConnCache`), R08 (`CullTick`) y R21 (`NeedsDeviceSync`).

### R05 — VIVA — Ordenación cuadrática, parcialmente recortada hoy

**Evidencia:** `scripts/4_World/LFPG_CableRenderer.c:2719` excluye nulos y entradas sin segmentos, pero no ocluidos ni cables detrás de cámara. Los bucles anidados de selection sort están en `:2729` y `:2734`; `CullTick()` marca el orden sucio en `:2640`, y `DrawFrame()` lo reconstruye en `:2923`. El timer se instala en `:865` con los 2 s definidos en `scripts/3_Game/LFPG_Defines.c:551`.

El coste de ordenación sigue siendo cuadrático respecto a los cables admitidos. R04 liberó hoy geometría oculta por distancia y excluyó metadatos vacíos, pero los cables ocluidos se descartan más tarde (`LFPG_CableRenderer.c:2998`), después de haber participado en el sort; no es correcto conservar la antigua afirmación de que entran todos los invisibles por distancia.

**Coste:** ACOTADO. **Comparte:** grupo Cables; `RebuildDrawOrder`/`DrawFrame` con R09/R10/R11 y membresía/visibilidad con R08/R26. El límite actual de 512 segmentos (`LFPG_Defines.c:223`) acota el tamaño; no hay perfil que demuestre tirones ni justifique por sí solo P2.

### R06 — VIVA — Índice global reconstruido por edición local

**Evidencia:** `scripts/4_World/LFPG_CableRenderer.c:1336` y `:1875` invocan `RebuildConnCache()` tras cambiar un único owner. El helper vacía los dos mapas en `:1978`/`:1979`, recorre todos los owners en `:1982`, todas sus aristas en `:2001` y resuelve entidades destino en `:2018`.

Una edición local vuelve a procesar el conjunto global conocido. El guard de JSON idéntico evita snapshots redundantes, pero no evita la reconstrucción global cuando sí cambia una sola conexión.

**Coste:** ACOTADO si el índice incremental y sus referencias por owner permanecen en este fichero; hay que cubrir retirada, extremos compartidos y owners vacíos. **Comparte:** grupo Cables, misma caché de R03 y misma ruta `ApplyOwnerDelta()` que R07/R20. El mínimo de R03 añade otra llamada global: conviene resolver esa corrección sin confundirla con la optimización de R06.

### R07 — VIVA — Delta de red sin delta de geometría

**Evidencia:** `scripts/4_World/LFPG_CableRenderer.c:1838` aplica las operaciones a `st.wires`; al terminar, `:1873` llama a `DestroyOwnerLines(ownerDeviceId)` y `:1876` a `BuildOwnerWires(ownerDeviceId)`. Este último prepara claves de todas las aristas en `:2101` y sus tramos en `:2159`.

Cambiar una arista rehace las demás del mismo owner, incluyendo sus objetos y estado de oclusión/proyección. R04 controla la admisión de segmentos, pero no ha hecho incremental esta actualización.

**Coste:** ACOTADO en el renderer, con revisión de claves por índice y remapeo después de `Remove`; no basta con saltarse la destrucción indiscriminadamente. **Comparte:** grupo Cables; `ApplyOwnerDelta()` con R06/R20 y construcción con R01/R23/R24/R26.

### R08 — VIVA — Early-out por origen oculta el extremo cercano

**Evidencia:** `scripts/4_World/LFPG_CableRenderer.c:2382` fija el umbral en distancia de culling + 25 m. `:2413` mide solo la posición del owner y `:2414` entra en la rama que oculta/libera todas sus líneas (`:2434`, `:2435`) y hace `continue` en `:2438`. La prueba de esfera y extremos de cada cable llega después, en `:2540`/`:2544`.

Con culling de 50 m (`scripts/3_Game/LFPG_Defines.c:550`), un owner a 80 m queda fuera aunque el jugador esté junto al destino; los 100 m de longitud total admitida (`:16`) no excluyen este escenario. Se salta precisamente la prueba que podría conservar el cable cercano; hoy, además de ocultarlo, se liberan sus segmentos.

**Coste:** ACOTADO. **Comparte:** grupo Cables; `CullTick()` con R03/R05 y decisiones espaciales de construcción con R26. La ausencia de segmentos puede mantener despierto el canvas por R09.

### R09 — VIVA — Productor activo con metadatos pero sin geometría

**Evidencia:** `scripts/4_World/LFPG_CableRenderer.c:892` implementa `HasRenderableWires()` como `m_WireSegments.Count() > 0`. `ReleaseWireSegments()` en `:4242` llama a `info.DestroyAll()` en `:4247`; este vacía `segments` en `:690`, conservando la entrada y sus bounds. `scripts/5_Mission/LFPG_MissionInit.c:401` toma esa respuesta como productor activo y llama a `hud.BeginFrame(...)` en `:406`.

Si todas las entradas conservadas tienen cero segmentos, el canvas sigue consultando resolución y limpiándose cada frame (`scripts/4_World/LFPG_CableHUD.c:210`, `:236`), y el renderer supera su guard de mapa vacío (`LFPG_CableRenderer.c:2772`). El guard de ausencia de productores existe (`LFPG_CableHUD.c:188`) y sí sirve cuando el mapa está vacío; no cubre esta ruta que la liberación de geometría de hoy hace explícita.

**Coste:** ACOTADO para corregir la señal usando la geometría real; cubrir además todo caso oculto por configuración/cámara puede abarcar más ficheros. **Comparte:** grupo Cables, R05/R08; y despacho de canvas con R27/R30. No se propone apagar las comprobaciones necesarias para detectar reentrada o nueva visibilidad.

### R10 — VIVA — Claves incompletas de proyección

**Evidencia:** las claves de reutilización están en `scripts/4_World/LFPG_CableRenderer.c:3113` (ultra), `:3292` (polilínea, añade sway) y `:3632` (joints). Comparan posición, dirección y viewport, sin FOV ni ejes lateral/superior/roll de cámara. Los campos almacenados en `:199` a `:220` tampoco contienen esos valores.

Una variación de FOV con los valores comparados idénticos acepta proyecciones anteriores; dirección de visión tampoco distingue un giro sobre ese eje. La oscilación fuerza muchas reproyecciones de la polilínea, pero no repara la clave de ultra/joints ni constituye una invalidación por FOV.

**Coste:** ACOTADO; verificar la fuente completa de estado de cámara antes de implementar. **Comparte:** grupo Cables, mismo `DrawFrame()` de R05/R09/R11; viewport compartido con R27. Se demuestra la omisión de entradas, no que toda sesión ordinaria tenga roll o cámara exactamente inmóvil al hacer zoom.

### R11 — VIVA — Movimiento medido entre frames en vez de acumulado

**Evidencia:** `scripts/4_World/LFPG_CableRenderer.c:2783`/`:2784` calculan deltas; `:2789`/`:2793` aplican umbrales y `:2797`/`:2798` actualizan ambas referencias siempre. Los umbrales son 0,3 m y 0,02 en dirección (`scripts/3_Game/LFPG_Defines.c:240`, `:241`). `UpdateOcclusionBudget()` añade demora cuando `m_CamMoved` es falso (`LFPG_CableRenderer.c:3705`, `:3738`).

Varios desplazamientos pequeños nunca se suman y una misma velocidad produce deltas distintos según FPS. Por ejemplo, 3 m/s dan 0,05 m por frame a 60 FPS: siempre por debajo del umbral de posición; la oclusión sigue funcionando por recheck forzado, pero con la demora de cámara supuestamente estática, no congelada para siempre.

**Coste:** ACOTADO. **Comparte:** grupo Cables; `DrawFrame()` y `UpdateOcclusionBudget()` con R05/R09/R10 y el muestreo de R01. El parche de presupuesto de raycasts no corrige esta referencia temporal.

### R12 — VIVA — Información dinámica de conexiones sin renovación

**Evidencia:** `scripts/4_World/LFPG_DeviceInspector.c:523` obtiene únicamente la generación del owner inspeccionado; `:545`/`:549` invalidan al cambiar esa generación. Tras `OnInspectResponse()` se fija `m_HasServerData = true` en `:1337`; el reintento de `:572` solo opera cuando es falso. El refresco local de `:559` no invalida esa respuesta y `:1297` limita la población de filas a `m_WireDataDirty`.

Las filas muestran potencia asignada y brownout del snapshot (`:1490`, `:1501`), datos leídos por el servidor en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2361` y `:2392`. La asignación eléctrica cambia en `scripts/5_Mission/LFPG_ElecGraphImpl.c:3730` sin necesidad de editar los cables del owner inspeccionado; también una conexión entrante pertenece al owner remoto. No se encontró push adicional ni invalidación del inspector en las rutas de delta/snapshot del renderer.

La conservación es indefinida mientras se mantenga la inspección y no cambie la generación; ocultar/cambiar objetivo sí puede invalidarla. No afecta necesariamente a los estados locales que `ClientDataChanged()` sí lee.

**Coste:** ACOTADO para renovar con cadencia limitada en el inspector; una suscripción por arista sería AMPLIO. **Comparte:** R13/R28 en `Tick()`/`PopulateClientData()`/`PopulateWireData()`. No usar solo la generación de topología para datos eléctricos dinámicos.

### R13 — VIVA — Dos offsets incompatibles para las filas

**Evidencia:** `scripts/4_World/LFPG_DeviceInspector.c:1286` suma tank/fuel/reserve/link/battery y posiciona separador/cabecera en `:1289`/`:1293`. Inmediatamente puede llamar a `PopulateWireData()` (`:1301`), que los recoloca en `:1418`/`:1421` sin link/battery; las filas también omiten esos offsets en `:1430`. El link añade 20 px (`:1179`) y la batería 26 px (`:1275`).

La respuesta del inspector sobrescribe el posicionamiento completo con uno incompleto. La altura del panel sí incorpora todos los offsets (`:1590`): corregir solo la altura no corrige el solapamiento interior de cabecera/filas con sorter/batería.

**Coste:** ACOTADO. **Comparte:** R12/R28 en el mismo fichero; R12 comparte `PopulateWireData()` y R28 sus setters con caché. La posición de las filas debe depender del mismo conjunto de offsets que la cabecera.

### R14 — VIVA — Destrucción de cámara ajena inicia la salida CCTV

**Evidencia:** `scripts/4_World/LFPG_Camera.c:100` y `:107` llaman a `LFPG_CameraViewport.SafeAbort()` desde killed/deleted sin argumentos. El despachador real de esos hooks está en `scripts/4_World/LFPG_DeviceBase.c:282` y `:302`. `SafeAbort()` solo comprueba singleton, sesión activa y fase (`scripts/4_World/LFPG_CameraViewport.c:249`), y asigna fase 1 en `:262`.

Una cámara distinta de la usada puede disparar el mismo cierre; la fase 1 termina enviando `CCTV_EXIT_REQUEST` para el jugador de la sesión activa (`LFPG_CameraViewport.c:967`, `:969`). La generación de sesión añadida hoy para R15 protege la limpieza posterior, pero este evento sin identidad pertenece al singleton actual y sigue iniciando su salida.

**Coste:** AMPLIO: identificar la entidad afectada en el contrato de abort y actualizar los hooks pertinentes. **Comparte:** CCTV con R22/R29; no confundirlo con la seguridad de restauración de R15. El perjuicio demostrado es interrupción de sesión, no muerte del proceso.

### R16 — VIVA — Sondeo negativo de bombas a 4 Hz

**Evidencia:** `scripts/5_Mission/LFPG_MissionInit.c:435` llama a `tankHud.Tick()` desde `OnUpdate`; `scripts/5_Mission/LFPG_TankHUD.c:122` llama a `CanQueryTank()`. El umbral es 250 ms (`:33`), el retorno cacheado termina al vencerlo (`:161`) y se ejecuta `GetObjectsAtPosition3D(...)` en `:169`, aunque todas las consultas previas fueran negativas.

No tener bomba próxima evita el raycast de cursor posterior, pero no la consulta espacial periódica que busca la bomba. La ficha describe correctamente ese trabajo residual mientras haya widget y jugador; no se ha medido cuánto cuesta en una escena concreta.

**Coste:** ACOTADO para reducir/adaptar el sondeo negativo; un registro de bombas con notificaciones sería AMPLIO. **Comparte:** no comparte fichero principal con otra ficha; sí el llamador de misión con R09/R17/R27/R30 y el uso del cursor compartido con R12.

### R17 — VIVA — Predicción visual del foco sin cambio de ángulos

**Evidencia:** `scripts/4_World/LFPG_SearchlightController.c:351` llama incondicionalmente a `LFPG_ApplyAimLocal(m_AimYaw, m_AimPitch)` dentro de la sesión activa; el guard `m_AimDirty` de `:355` solo afecta a la RPC. `scripts/4_World/LFPG_Searchlight.c:570` comprueba que haya luz, pero no si cambiaron los ángulos; vuelve a orientar entidad y animación (`:581`, `:585`), calcula trigonometría/memory points (`:587`, `:601`) y coloca las luces (`:619`, `:624`).

Mantenerse inmóvil operando un foco encendido repite todo ese trabajo visual. La corrección de tráfico de aim no lo elimina porque el consumidor local permanece fuera del guard de envío.

**Coste:** ACOTADO si el guard y su invalidación al recrear las luces se resuelven dentro de `LFPG_Searchlight.c`. **Comparte:** R24 en `LFPG_ApplyAimLocal()`/`ApplyOrientation()`; el despacho de misión coincide con R09/R16/R27/R30. No aplicar sin más la deadzone de red al feedback local, pues son contratos distintos.

### R18 — VIVA — Alta acumulativamente cuadrática de láseres

**Evidencia:** `scripts/4_World/LFPG_LaserBeamRenderer.c:101` busca el detector en el array; si es nuevo lo inserta y llama a `CullTick()` (`:103`, `:104`). Este limpia candidatos (`:166`) y recorre todo `m_Detectors` en `:176`. Las altas reales desde dispositivo están en `scripts/4_World/LFPG_LaserDetector.c:504` y `:565`.

Una tanda de N altas nuevas hace recorridos de tamaños 1, 2, …, N, además de las búsquedas de duplicados. Los dispositivos apagados ahorran proyecciones mediante el guard de `CullTick`, pero no eliminan esos recorridos.

**Coste:** ACOTADO. **Comparte:** exactamente `CullTick()` y la lista de candidatos con R19. Agrupar ambas evita corregir las altas introduciendo otra espera visible al entrar en cámara.

### R19 — VIVA — Admisión por frustum a 250 ms

**Evidencia:** `scripts/4_World/LFPG_LaserBeamRenderer.c:41` define 250 ms y `:68` instala el timer. `CullTick()` proyecta extremos (`:188`), descarta detrás de cámara (`:192`) y fuera de pantalla (`:201`/`:202`) antes de incorporar el detector en `:205`. `DrawFrame()` solo recorre `m_ActiveDetectors` (`:213`, `:242`).

Al girar hacia un láser antes descartado, el dibujo por frame no puede reconsiderarlo porque no está en la lista. El mecanismo permite una espera de hasta un intervalo nominal de culling para aparecer; el clipping por frame de quienes ya estaban admitidos no cura esa omisión.

**Coste:** ACOTADO. **Comparte:** R18 en `CullTick()` y registro; con R27 comparte consumo de dimensiones del HUD (`:226`, `:227`). La intensidad perceptiva del pop-in requiere juego; no se midieron tiempos del scheduler.

### R20 — VIVA — Receptor de snapshot permite bajar generación

**Evidencia:** `scripts/4_World/LFPG_RPCClientHandler.c:352` entrega el snapshot V2 al renderer sin comparar generaciones. `scripts/4_World/LFPG_CableRenderer.c:1220` reemplaza la generación anunciada y `:1306` o `:1360` reemplazan la aplicada siempre que la recibida sea no negativa. Por contraste, el delta rechaza generaciones no posteriores en `:1810`.

Para un estado local de generación 11, recibir un snapshot de generación 10 con JSON idéntico ejecutaría `:1360` y dejaría 10; con JSON diferente también podría sustituir datos y geometría. Es una propiedad comprobable del receptor, no una demostración de que el transporte fiable reordene RPC: la frecuencia y alcanzabilidad en una sesión normal siguen sin verificar.

**Coste:** ACOTADO para mantener coherencia entre generación anunciada/aplicada antes de tocar datos; contemplar la ruta legacy que no trae generación propia (`:1208`). **Comparte:** grupo Cables, sync con R21 y aplicación de estado con R06/R07. El caso potencial original no debe convertirse en «rollback observado en producción».

### R21 — VIVA — Deduplicación pendiente sin deduplicación en vuelo

**Evidencia:** `scripts/4_World/LFPG_CableRenderer.c:1533` deduplica mediante mapas pendientes. Después de `rpc.Send(...)` (`:1639`), se registran datos/deadline de respuesta (`:1671` a `:1673`) y se retiran las entradas pendientes (`:1676` a `:1679`). `QueueDeviceSync()` (`:1515`) no consulta esos mapas de respuesta; solo cooldown, y este se registra al recibir en `:1233`.

Antes de la primera respuesta, una nueva petición del mismo ID puede crear otro lote; las peticiones forzadas, además, omiten el cooldown. `NeedsDeviceSync()` compara datos/generaciones (`:1497`) y tampoco identifica una solicitud ya enviada. No se afirma que cada llamada duplique una RPC: el debounce sí fusiona las que coinciden antes del envío.

**Coste:** ACOTADO. **Comparte:** grupo Cables, estado/generaciones con R20 y lectura de conocidos con R03. Mantener reintento/timeout y permitir pedir una generación más reciente exige distinguir solicitud en vuelo de datos ya recibidos.

### R22 — VIVA — Código y estado sin consumidor

**Evidencia:** búsqueda exacta de símbolos en `scripts/`, `gui/` y `config.cpp`:

- `scripts/4_World/LFPG_CableRenderer.c:4159`: `EstimateSegments()` solo tiene definición. R04 usa ahora la geometría post-sag para la admisión (`:2238`); el diff de `cd5d153` retira sus dos llamadas, por lo que este residuo es de hoy.
- `LFPG_CableRenderer.c:173` y `:2319`: `cachedWireKey` se declara/escribe y no se lee. No confundirlo con `cachedWireKeys` del owner, que sí se usa.
- `scripts/4_World/LFPG_CameraViewport.c:1107` actualiza `m_ScanlineOffset` y lo normaliza en `:1108`, pero `DrawOverlay()` dibuja líneas estáticas desde cero (`:1146`), sin consumirlo. Sus demás apariciones son reinicios y esa propia aritmética.
- `LFPG_CableRenderer.c:974`: `ForceGlobalRefresh()` no tiene llamador en el árbol inspeccionado; no se presupone ausencia de una invocación externa de administrador fuera del repo.

Estos ejemplos localizan la ficha genérica sin extenderla a toda clase antigua o a la V3 del sorter. Hay mantenimiento residual y, en el offset de CCTV, aritmética por frame sin efecto en la imagen.

**Coste:** AMPLIO para estos residuos repartidos; una eliminación aislada puede ser ACOTADO. **Comparte:** Cables con R07/R23/R24 y CCTV con R14/R29. No se cuantifica arena liberada ni se infiere que borrar líneas reduzca su consumo.

### R23 — VIVA — Una instancia por subsegmento

**Evidencia:** `scripts/4_World/LFPG_CableRenderer.c:145` mantiene `array<ref LFPG_CableParticle>` y `:2253` crea una instancia por iteración de subsegmento. La clase de `scripts/4_World/LFPG_CableParticle.c:14` contiene extremos (`:17`, `:18`) y validez (`:19`); `Create()` solo guarda/valida geometría (`:30`).

La representación conserva un objeto referenciado por tramo y repite el extremo común de tramos adyacentes. No son partículas ni entidades del mundo: aquí «objeto gestionado» significa instancia mantenida mediante `ref`, no una clase que declare herencia de `Managed`; su coste real de memoria no se ha medido.

**Coste:** AMPLIO: cambiar la representación afecta construcción, bounds, oclusión, dibujo y liberación, además de la clase de segmento. **Comparte:** grupo Cables, `BuildWire()` con R01/R26 y reconstrucciones de R07/R24. Preservar discontinuidades si se descarta un segmento degenerado; no basta con concatenar todos los extremos sin revisar esa semántica.

### R24 — VIVA — Preparación y matemáticas duplicadas

**Evidencia:** `scripts/4_World/LFPG_CableRenderer.c:2159` y `:3981` repiten preparación de waypoints, clamp y midpoint en construcción inicial/retry. La interpolación y sag de `ApplyCatenaria()` (`:4212`, `:4221`) reaparecen en `scripts/4_World/LFPG_WiringClient.c:541`/`:552`, aunque comparten `GetAdaptiveSubs()` y `GetSagAmount()`.

Hay además cálculo de bounds tanto en `LFPG_CableRenderer.c:522` como en la nueva admisión (`:4274`), y los bloques de orientación/trigonometría/memory points de `scripts/4_World/LFPG_Searchlight.c:457` y `:570` repiten el mismo trabajo para SyncVars y predicción local. La deuda sigue localizada; no toda matemática está duplicada: clipping y edge fade ya usan `LFPG_WorldUtil` compartido.

**Coste:** AMPLIO para el conjunto. **Comparte:** Cables con R01/R07/R22/R23/R26, preview en `LFPG_WiringClient.c`, y R17 en las dos rutas de orientación del foco. Hay que conservar diferencias deliberadas entre preview y cable construido; no se recomienda consolidación mecánica de bloques parecidos.

### R26 — DUDOSA — Distancia inicial corregida; visibilidad inicial optimista

**Lectura A: todos los cables nuevos arrancan como cercanos por `cachedMinDist = 0`.** Esa parte **murió hoy** con `cd5d153`: `scripts/4_World/LFPG_CableRenderer.c:2281` calcula distancia a la esfera antes de insertar el cable, y la admisión previa en `:4287`/`:4289` rechaza cables fuera de distancia/burbuja. El consumidor de LOD usa ese valor en `:3009`. Es una aproximación por esfera, igual que la política normal de culling, no una distancia exacta a la polilínea.

**Lectura B: cables nuevos se consideran visibles antes de completar oclusión.** Esa parte **sigue viva**: el constructor establece `visible = true`, `occluded = false` (`:231`, `:232`) y las muestras arrancan desbloqueadas (`:307`, `:309`). El presupuesto y la clasificación progresiva de oclusión no garantizan que cada cable complete su evaluación antes de su primer dibujo.

La ficha mezcla una inicialización espacial que hoy se corrigió con una política de aparición optimista que permanece. No procede marcarla íntegramente MUERTA ni mantener «todos se dibujan cercanos» como hecho actual; hacen falta dos criterios separados y una prueba visual para valorar el residuo de oclusión.

**Coste:** ACOTADO para cambiar la política de aparición/primer chequeo; la parte de distancia no requiere otro arreglo. **Comparte:** grupo Cables, `BuildWire()`/constructor con R01/R23, culling con R08 y presupuesto de oclusión con R11.

### R27 — VIVA — Frame inválido no se propaga a productores

**Evidencia:** `scripts/4_World/LFPG_CableHUD.c:215` detecta tamaño no positivo, limpia y retorna en `:218`, sin actualizar dimensiones ni `m_Ready`. `IsReady()` devuelve únicamente `m_Ready` (`:95`), y los getters siguen devolviendo el tamaño anterior (`:102`, `:107`). `scripts/5_Mission/LFPG_MissionInit.c:406` llama a `BeginFrame()` de retorno void y despacha productores según el booleano calculado antes (`:408` a `:420`).

Tras un frame válido seguido de 0×0, los productores pueden superar sus guards y trabajar con dimensiones viejas, incluso dibujar después del `Clear` de rechazo. El mecanismo está en código; no se ha reproducido alt-tab ni un artefacto concreto de pantalla.

**Coste:** ACOTADO si el HUD invalida de forma consistente la disponibilidad de ese frame y sus primitivas; AMPLIO si se cambia el contrato de retorno y despacho de productores. **Comparte:** R09 en `BeginFrame`/misión; R10 y R19 consumen los tamaños del mismo HUD.

### R28 — VIVA — Lecturas duplicadas entre snapshot y presentación

**Evidencia:** `scripts/4_World/LFPG_DeviceInspector.c:538` llama a `ClientDataChanged()` y `:539` a `PopulateClientData()`; el refresco repite la secuencia cuando hay cambios (`:561`, `:563`). El snapshot lee energía/max/carga de batería (`:685` a `:687`) y población los vuelve a leer (`:1194` a `:1196`); ocurre también con bomba (`:662` frente a `:973`), furnace (`:670` frente a `:1056`) y sorter (`:679` frente a `:1164`).

La redundancia de lectura es real en los ciclos que repueblan. El snapshot actual guarda primitivas, no texto ya formateado: no se confirma la lectura maximalista «cada estado se formatea dos veces en todo tick»; los setters genéricos por mapa (`:711`, `:722`, `:733`, `:744`, `:755`) sí persisten, pero su existencia sola no demuestra un defecto de rendimiento.

**Coste:** ACOTADO para reutilizar un snapshot coherente durante población. **Comparte:** R12/R13 en `Tick()` y `PopulateClientData()`, además de setters de posición compartidos con R13. No sustituir la caché genérica por otra abstracción sin una necesidad o medición concreta.

### R29 — VIVA — Etiquetas CCTV sin actualización de resolución

**Evidencia:** `scripts/4_World/LFPG_CameraViewport.c:271` retorna de `InitWidgets()` cuando el overlay ya existe. El tamaño de pantalla se lee en `:300` y se fijan posiciones/tamaños de etiquetas en `:323`, `:334` y `:344`. `DrawOverlay()` sí vuelve a leer la resolución (`:1129`) y reconstruye las líneas si cambia (`:1137`, `:1140`), sin recolocar las etiquetas.

Un cambio de resolución conserva las coordenadas/tamaños en píxeles calculados durante la primera creación, mientras la geometría del overlay se vuelve a calcular para la resolución nueva. No se ha verificado en juego la posición final de cada etiqueta ni el escalado externo del árbol.

**Coste:** ACOTADO. **Comparte:** CCTV con R14/R22; `DrawOverlay()` es además el consumidor que demuestra que el offset animado de R22 no se usa. El ciclo de creación del overlay se despacha desde el fichero de R30.

### R30 — VIVA — Escritura global de brillo sin restauración propia

**Evidencia:** `scripts/5_Mission/LFPG_MissionInit.c:140` y `:141` ejecutan `Widget.SetLV(0)` y `Widget.SetTextLV(0)` en `OnInit()`. La búsqueda de ambos símbolos y sus posibles getters en el código del mod no encuentra captura/restauración del valor previo. Las declaraciones nativas locales en `C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts/1_core/proto/enwidgets.c:114` y `:116` describen explícitamente el alcance global de ambos setters.

Se impone brillo global también sobre widgets ajenos a LFPG y no se revierte desde este mod. Eso solo produce un cambio efectivo si el estado anterior era distinto de cero; de hecho, esas declaraciones documentan cero como valor por defecto, por lo que el comentario del mod sobre un default negativo no prueba el problema visual que invoca.

**Coste:** ACOTADO para retirar o restringir esta imposición local; restaurar exactamente un valor previo requiere primero verificar una fuente real de lectura, que no se encontró en las declaraciones consultadas. **Comparte:** fichero de misión con R09/R16/R17/R27; no comparte su función con ellas. La interferencia concreta entre mods sigue siendo potencial.

## Ficha muerta y fecha de resolución

**R25 — MUERTA; ya estaba muerta antes de hoy.** `scripts/4_World/LFPG_CableRenderer.c:1267`/`:1269`, `:1350`/`:1352`, `:2067`/`:2069` y `:4008`/`:4010` protegen tanto construcción de cadenas como llamada con `LFPG_LOG_LEVEL >= 2`. El nivel actual es 1 (`scripts/3_Game/LFPG_Defines.c:400`), con DIAG y PERFDIAG desactivados (`:404`, `:405`); las llamadas de esos canales también tienen sus guards. La función emisora filtra nivel antes de `Print` (`scripts/3_Game/LFPG_Util.c:9` a `:11`).

Se comprobó el contenido del cambio `7194ebb` (2026-08-26) que movió los mensajes habituales de Info a Debug y añadió guards alrededor de sus cadenas; es ancestro de HEAD. Se comprobó también que el padre de `cd5d153`, antes del parche de esta madrugada, ya contenía esos guards. Por tanto, no se atribuye la muerte de R25 al trabajo de hoy.

Quedan warnings ante datos/segmentos inválidos y un Info de reconciliación solo cuando se reencolan huérfanos (`LFPG_CableRenderer.c:4083` a `:4092`), además del Info del helper sin llamadores `ForceGlobalRefresh()`. Esto no equivale a la antigua emisión habitual por recepción/construcción/retry correcto. No se afirma «cero logs» ni se auditó aquí el volumen total de logs de todo el mod. Comparte el grupo Cables, pero no requiere tramo de arreglo de R25.

## LAS QUE RECOMIENDO ATACAR PRIMERO

1. **R14:** interrumpe una sesión CCTV válida por un evento de una cámara distinta; corregir la atribución del evento evita expulsiones inesperadas del uso actual del jugador.
2. **R12:** el inspector puede mentir sobre alimentación/brownout y conexiones entrantes mientras el jugador diagnostica su instalación. Resolver frescura antes de invertir en microoptimizaciones de sus widgets.
3. **R08:** un cable válido puede desaparecer junto al destino por la distancia al origen; dificulta directamente comprender y mantener instalaciones largas.
4. **R03:** las cachés retiradas a medias pueden seguir mostrando ocupación y afectar decisiones de resync; corregir su coherencia antes de optimizar globalmente el índice.
5. **R01:** la ruta que debía preservar visibilidad alrededor de esquinas nunca utiliza sus joints; afecta al cableado en interiores y tras paredes. Prioridad por el uso afectado, aunque su corrección sea pequeña.

R20 conserva un mecanismo de aceptación regresiva, pero su daño efectivo depende de una secuencia de entrega todavía no demostrada; no desplaza los cinco anteriores solo por una hipótesis de reordenación. R05/R06/R07/R17/R18/R23 requieren perfilar para ordenar su daño al frame frente a los defectos funcionales.

## LO QUE NO PUDE VERIFICAR

- Compilación de los módulos Enforce y comportamiento dentro de DayZ: este encargo solo permite lectura; no se declara PASS in-game.
- Coste real en ms, FPS, memoria o arena de R05/R06/R07/R09/R16/R17/R18/R23/R24/R28; se demostraron recorridos/representaciones, no su impacto medido.
- Resultado de oclusión, primer frame visible y gravedad perceptiva de R01/R10/R11/R19/R26 en escenas reales con giros, obstáculos y zoom.
- Una secuencia real de transporte que entregue un snapshot antiguo después de datos más nuevos para R20; no se presupuso reordenación de RPC fiables.
- Incidencia real de duplicados en vuelo y carga resultante de R21 según latencia/streaming; sí se siguieron encolado, envío, respuesta y cooldown.
- Artefacto de alt-tab/0×0 de R27 y disposición efectiva de etiquetas CCTV a varias resoluciones para R29.
- Valor de brillo anterior a `OnInit()`, restauraciones que pueda realizar el motor u otros mods y efecto visible global de R30.
- Invocaciones externas por herramientas/admin de métodos sin llamador en este árbol para R22; la ausencia se limita al repo inspeccionado.
- Autoría exacta o inventario exhaustivo que pretendían las fichas genéricas R22/R24/R28: se localizaron ejemplos concretos, no se reconstruyó una intención omitida por la descripción.
- `CLAUDE.md` de proyecto no está presente en esta copia; no se sustituyó por un plan externo supuesto vigente.

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

- **El lote contiene descripciones compuestas.** R26 mezcla «cercanos» con «visibles» y hoy solo la primera afirmación dejó de ser cierta en su sentido de inicialización a cero. R28 mezcla lecturas duplicadas, formato duplicado y una preferencia de diseño de caché: solo la redundancia de lectura está demostrada sin interpretación adicional. Separar esos criterios evita falsos vivos o falsos muertos.
- **«Invisibles» ha cambiado de significado para R05/R09.** R04 libera hoy segmentos ocultos por culling y deja metadatos. Esos metadatos ya no se ordenan, pero aún activan el canvas por R09; ocluidos/detrás de cámara sí pueden conservar segmentos y entrar en el sort. No son la misma población.
- **No todo lo llamado «eliminar owner» es borrado del dispositivo en servidor.** R03 demuestra una purga local tras ausencia, que puede venir de streaming. Un snapshot/delta de corte previo puede curar conexiones en una eliminación real; lo que falta es la coherencia de la retirada local con sus cachés.
- **Una corrección de hoy también puede crear deuda dentro de otra ficha.** R04 resuelve admisión/capacidad, corrige parte de R26 y deja `EstimateSegments()` sin llamadas (R22). R15 endurece la restauración de CCTV, pero no identifica la cámara que dispara `SafeAbort()` (R14).
- **R25 no pertenece al backlog de defectos actuales.** La cura se comprobó en contenido anterior a hoy; la etiqueta histórica «Confirmado» no invalida el código actual ni justifica reabrirla por el simple número de llamadas de log visibles en una búsqueda.
- **R23 no es un sistema de partículas del motor.** Su nombre induce a confundir instancias de datos con entidades/efectos. Recomiendo tratarla como deuda de representación, provisionalmente P3 hasta demostrar perjuicio de memoria/tiempo que sostenga P2; existe además un límite de segmentos.
- **La prioridad de costes no está sustentada por perfil.** R05, en particular, es cuadrática pero acotada y espaciada; propongo revisar P2 frente a P3 después de medir una base representativa. No hay evidencia en este lote para promover a P1 por pérdida de datos o caída del proceso.
- **«Potencial» no equivale a localización fallida.** En R10/R20/R27/R30 se demuestra el mecanismo estático; la escena, secuencia de red o configuración que materializa el daño no se ejecutó. Los veredictos no borran esa distinción.
- **El comentario sobre brillo no coincide con la declaración nativa.** `LFPG_MissionInit.c:138` atribuye al motor un default negativo; `enwidgets.c:114`/`:116` documentan cero. La imposición global de R30 existe, pero no se demuestra oscurecimiento universal ni cambio visible en una sesión que ya estuviera a cero.
- **Las fronteras Render/UI no son solo visuales.** R03/R06/R20/R21 incluyen datos de conexión y sincronización; sus cambios deben revisarse con sus consumidores de acciones y RPC. R22/R24/R28 son deuda agrupada y no necesariamente 3 arreglos atómicos. La V3 del sorter queda fuera de esta ficha genérica salvo evidencia específica.

## Cierre y conservación del workspace

Único producto de esta lane: `TRIAJE.md`. No se editaron ficheros de código, no se modificó el índice ni se ejecutaron commit/checkout/stash/reset. Los archivos de control preexistentes (`BRIEF.md`, `EXIT.start`, `events.jsonl`, `stderr.log`) se dejaron a su responsable; sus posibles cambios de ejecución no son cambios de código de esta lane.

Validación final ejecutada: 27 IDs únicos y completos, 25 veredictos VIVA + 1 MUERTA + 1 DUDOSA, correspondencia de las 26 secciones detalladas, coste y solapamientos presentes en todas ellas, y 63 referencias con nombre de fichero verificadas contra rutas/líneas existentes y no vacías. La lectura de los mecanismos figura en cada ficha; estas comprobaciones del documento no prueban el comportamiento del motor. `git diff --name-only` y `git diff --cached --name-only` resultaron vacíos. No se creó memoria durable ni handoff adicional en el vault porque el brief autoriza escribir exclusivamente este informe; este documento contiene evidencia, decisiones y límites para el siguiente tramo.
