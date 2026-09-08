# INFORME — Lane 5 — T0b — V4 del sorter

Base comprobada: `8de29d5`. Se localizaron los sitios por contenido. Se implementaron seis fichas en siete scripts permitidos; la divergencia descrita por S10 no se ha localizado en esta base. La V4 queda preparada para revisión y prueba in-game, sin declarar que compila ni que está validada para publicar.

El brief es la aprobación aplicada. No se han modificado archivos V3 de referencia, layouts, `LFPG_Defines.c`, persistencia, nombres de clases, handlers existentes ni enganches de misión. `INFORME.md` se interpreta como excepción explícita a la lista blanca por ser el entregable exigido. No hay commit ni cambios en el índice: ambos están prohibidos. No se escribe memoria fuera del workspace; este informe es el handoff durable de la lane.

## Mapa cliente/servidor y alcance

| Dato o flujo | Cliente | Servidor / puente comprobado |
|---|---|---|
| Energía y vínculo | La acción lee `LFPG_IsPowered()` y `LFPG_IsLinked()` | Estado net-sincronizado: registro en `scripts/4_World/LFPG_Sorter.c:94`, getters en `:165` y `:621` |
| Sincronización V4 | Acción heredada emite 65 con NetID bajo y alto | Dispatch en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:157`; lector y validaciones en `:2709`; ACK 66 consumido en `scripts/4_World/LFPG_RPCClientHandler.c:984` |
| Cargo | Los handlers 34, 70 y el ACK de sort satisfactorio activan una señal | `scripts/3_Game/LFPG_CargoRefreshSignal.c:18` y `:27`; la V4 se suscribe solo mientras está abierta |
| Escalado y herramientas diagnósticas | Código cliente en 4_World | Scaler y señal de 3_Game; ninguna llamada nueva desde 4_World a 5_Mission |

### 1. D-02 — Portar LFPG_UIScaler

**Veredicto: ARREGLADA.**

La V4 captura las medidas de diseño en `scripts/4_World/test/LFPG_SorterView_TEST.c:1384`, aplica el factor antes de centrar en `:1590` y libera su captura en `:1506`. La resolución manual de bindings se adelanta a la captura para disponer de `SorterPanel`. Los tags y filas dinámicos incorporan el patrón V3 de `ScaleWidget` con `m_Scaled`, una vez por instancia: `scripts/4_World/test/LFPG_SorterTagView_TEST.c:121` y `scripts/4_World/test/LFPG_SorterPreviewRow_TEST.c:81`.

**Decisión de compatibilidad:** se conserva el algoritmo de la V3, pero con un contexto V4 en `scripts/4_World/test/LFPG_SorterView_TEST.c:83`. El scaler original guarda una sola captura en miembros estáticos y `Capture()` vacía sus arrays (`scripts/3_Game/LFPG_UIScaler.c:51` y `:63`). Copiar las llamadas literalmente haría que el primer Init de V4 sustituyese la captura de V3. El contexto intercambia temporalmente los cinco arrays y los dos flags, llama al scaler existente y restaura el estado V3 antes de retornar. Las definiciones utilizadas son `ComputeScale` (`scripts/3_Game/LFPG_UIScaler.c:155`), `Apply` (`:221`), `Reset` (`:295`) y `ScaleWidget` (`:334`). No recaptura medidas ya escaladas en cada apertura. Se descartaron tanto la copia literal que rompe convivencia como modificar el scaler compartido o la V3, que son de solo lectura.

**Verificación local:** revisión contra Capture/Apply/Reset y contra ambos prefabs V3; simulación de las asignaciones y wrappers del contexto, con scaler/objetos sustitutos: captura independiente, aplicar V4 sin cambiar V3, aplicar V3 después, repetir escala sin acumulación y limpiar V4 sin vaciar V3. También se comprobó la limpieza en el orden de misión V3→V4. Es una prueba sintética de aislamiento, no ejecución de Enforce ni prueba de referencias del motor.

**In-game pendiente:** abrir V3→V4→V3 y V4→V3→V4, repetir aperturas, arrastrar, cambiar resolución entre aperturas y finalizar/reiniciar misión. Comprobar panel, tags y preview a 720p, 1080p, 1440p y 4K, incluyendo DPI 100/150/200 %. FAIL si se escala el panel equivocado, se acumula escala o se recortan controles.

### 2. D-01 — Paridad de guardas de apertura

**Veredicto: ARREGLADA.**

La acción V4 exige el tipo exacto `LFPG_Sorter_TEST`, energía y vínculo: `scripts/4_World/test/LFPG_ActionOpenSorterPanel_TEST.c:51`, `:58` y `:73`. También aplica el tipo exacto en el emisor al terminar la acción (`:94`), igual que la V3. Conserva la exclusión mutua de ambos paneles.

Se siguió la decisión explícita del brief y el patrón de `scripts/4_World/LFPG_ActionOpenSorterPanel.c:56`, `:64` y `:80`. Se descartó conservar `IsKindOf` o permitir apertura sin energía/vínculo. La acción de sincronización sigue disponible para vincular primero un sorter con energía.

**In-game pendiente:** tabla positiva/negativa con V4 exacta energizada y vinculada, sin energía, sin vínculo, arruinada, V3 y subclase de V4. Solo el primer caso debe ofrecer esta acción; tampoco debe ofrecerla cuando cualquiera de los paneles está abierto. Verificar por separado la acción V3.

### 3. S1_PROBE — Gate de rendimiento

**Veredicto: ARREGLADA.**

Se elimina el booleano incondicional y se usa directamente `LFPG_PERFDIAG_ENABLED` en `scripts/4_World/test/LFPG_SorterView_TEST.c:1658`. Su definición leída sigue siendo `false` en `scripts/3_Game/LFPG_Defines.c:405`. El cuerpo de la sonda se conserva.

Se descartó añadir otro flag o editar Defines: ya existe el gate requerido. Las llamadas de logging preexistentes dentro de la sonda no se han reescrito.

**In-game pendiente:** varias aperturas con el flag apagado deben producir cero entradas `[S1Probe]`; en una variante diagnóstica con el flag activado deben reaparecer las métricas. Esto no promete silenciar otras trazas preexistentes del sorter o del scaler.

### 4. Hook MCP — Exclusión de compilación retail

**Veredicto: ARREGLADA.**

Se usa `#ifdef DIAG_DEVELOPER`, el criterio de build diagnóstico ya empleado en `scripts/3_Game/LFPG_FaultInject.c:17`. Se rodean miembros, polling, inicialización, destrucción y apertura/cierre: `scripts/4_World/test/LFPG_SorterView_TEST.c:177`, `:438`, `:458`, `:470`, `:1630` y `:1752`. Todo el bloque de creación, lectura, comandos y escritura del dump queda dentro del gate en `:1994`. También se excluyen los tres auxiliares MCP del controller en `scripts/4_World/test/LFPG_SorterController_TEST.c:2256`.

Se descartó un booleano de ejecución porque dejaría el hook compilado. No se define la macro en este diff ni se renombra/elimina `LFPG_MCP_SorterCmd.layout`. La exclusión es del código compilado retail; el recurso sigue conservado en disco.

**Verificación local:** evaluación textual de las directivas en las combinaciones cliente retail, cliente DIAG_DEVELOPER y SERVER. No quedan identificadores MCP en retail; el hook permanece en diag; ambos archivos cliente quedan vacíos en servidor. Llaves equilibradas en ambas variantes cliente.

**In-game pendiente:** retail debe carecer del widget de comandos y no escribir el dump. En DayZDiag comprobar `dump`, `close`, `catch_all`, `tab_preview` y `cat:N`, cerrar/reabrir y terminar misión. La compilación real de ambas variantes queda pendiente.

### 5. RPC 65 y consumo de refresh de cargo

**Veredicto: ARREGLADA (lado cliente solicitado).**

El emisor se conecta en la acción de sincronización que hereda la entidad V4: `scripts/4_World/LFPG_ActionSyncSorter.c:104`. Para el tipo exacto V4 selecciona 65; el resto conserva 29. Se mantiene el payload existente, en el orden que lee el servidor: subId, NetID bajo, NetID alto. La herencia y registro de esa acción están en `scripts/4_World/test/LFPG_Sorter_TEST.c:31` y `scripts/4_World/LFPG_Sorter.c:112` mediante `SetActions()`.

La V4 se suscribe a `LFPG_CargoRefreshSignal` al abrir (`scripts/4_World/test/LFPG_SorterView_TEST.c:1654`), se desuscribe al cerrar (`:1750`) y en Cleanup (`:1502`). El callback comprueba que está abierta (`:1520`) y delega en `scripts/4_World/test/LFPG_SorterController_TEST.c:1706`: invalida conteo/filas y solicita un nuevo preview si esa vista está visible. Reutiliza debounce y límite de peticiones en vuelo. Si están visibles las reglas, la siguiente entrada en preview solicita los datos.

**Decisión conservadora:** no se añade un botón ni un segundo mecanismo de resync al panel: se conecta la acción ya existente sobre la entidad V4, utilizable antes de abrir un sorter inicialmente sin vínculo. Tampoco se envía el 70 desde el cliente. El 70 es una notificación entrante, y su handler ya llama a la señal (`scripts/4_World/LFPG_RPCClientHandler.c:976`). El servidor actual publica 34, incluso para el flujo compartido de sorters (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:6316`). Sus handlers 34 y 70, más el ACK 69 satisfactorio, ya convergen en la señal, por lo que se consumen los tres sin retirar ni modificar handlers.

**In-game pendiente:** sincronizar una V4 con energía pero sin vínculo debe emitir 65, recibir 66 y permitir abrir tras sincronizar el vínculo; V3 debe seguir emitiendo 29. Con preview V4 abierto, ordenar desde otro cliente o por tick debe renovar filas sin reabrir. Probar notificación 34 real, 70 inyectada en diagnóstico, ACK 69, ráfagas con preview en vuelo y cierre/reapertura. No debe llegar ningún 70 como petición al servidor. Crear un productor servidor del 70 queda fuera de esta lista blanca y no es necesario para consumir el 34 que ya se publica.

### 6. IsOpen — Convivencia V3 y V4

**Veredicto: ARREGLADA.**

Se añade la consulta V4 conservando la V3 en `scripts/4_World/LFPG_Actions.c:530` y `scripts/4_World/LFPG_ActionSyncSorter.c:71`, dentro de sus límites `#ifndef SERVER`.

Se descartó reemplazar la consulta V3 o retirar el mutex. **In-game pendiente:** con cualquiera de los dos paneles abierto, no deben aparecer/ejecutarse acciones de cableado ni sincronización detrás del panel; ambas deben volver a funcionar al cerrar.

### S10 — Protección de preview

**Veredicto: NO-LOCALIZADA (la divergencia descrita).**

Se buscaron `S10`, protecciones/guardas de preview y los métodos reales en los controllers, views y handlers de ambas versiones, además del inventario de divergencias del repositorio. En `8de29d5` las dos versiones tienen la misma política de preview:

- V3: `RequestPreview` exige `m_IsPaired` en `scripts/4_World/LFPG_SorterController.c:1779`; se usa debounce y petición en vuelo; `SendPreviewNow` comprueba juego/jugador (`:1800`); `PopulatePreview` (`:1847`) descarta una salida distinta a la seleccionada.
- V4: esas mismas guardas están en `scripts/4_World/test/LFPG_SorterController_TEST.c:1718`, `:1740` y `:1768`. La guarda adicional V4 `CanEdit() = paired && powered` (`:934`) protege edición, no preview.

Una comparación de los cuerpos completos de `RequestPreview`, `SendPreviewNow` y `PopulatePreview`, normalizando nombres V4, comentarios y las trazas adicionales de rendimiento V3, dio equivalencia. También se leyeron ambos parsers de respuesta: mismo cap y comprobación de lecturas. El cap es truncamiento, no rechazo de todos los valores inválidos.

Se conserva la política común: no existe una de las dos más estricta que portar en esos sitios. Se descarta presentar como hallazgo una divergencia ausente o añadir un bloqueo por energía que ninguna versión aplica al preview. Si S10 aludía a otro mecanismo, falta su contenido concreto; no se declara resuelta una protección no localizada. Quedan límites heredados, como pairing cacheado, ausencia de correlación de respuesta por entidad/generación y permiso de preview tras pérdida de energía.

**In-game pendiente:** abrir preview sin vínculo mediante diagnóstico, perder energía con el panel abierto, cambiar salida con petición en vuelo y repetir ediciones rápidas. Comparar V3/V4: misma denegación sin vínculo, mismos debounce/timeout y descarte de salida obsoleta. Confirmar con el dueño la intención de S10 antes de cambiar la política de energía del preview.

## Verificación local realizada

- Diff acotado a siete scripts de la lista blanca más este entregable. CRLF conservado en los scripts; cero LF sueltos. No se reindenta código ajeno.
- Inspección de líneas añadidas: sin operadores prohibidos, logging directo ni `ref` fuera de los nuevos miembros; indentación nueva con tabs. `git diff --check` sin incidencias.
- Evaluación textual de directivas retail/diag/server y prueba sintética del aislamiento del scaler: resultados descritos arriba.
- `ui_reconcile.py`: exit 0, 12 layouts, 154 fuentes, cero FAIL y cero WARN. No se añadieron layouts, bindings ni claves de traducción.
- `script_validator.py`: estado WARN, cero errores y 55 avisos; el proceso Python devuelve 2 (el envoltorio PowerShell lo refleja como 1). La pasada inicial tenía 52 avisos. Los tres nuevos son `ES-GETTYPE-EXACT-MATCH`: acción V4 líneas 51/94 y selector RPC en la acción de sincronización línea 105. Son intencionados por la paridad exacta exigida en el brief y por el despacho del tipo exacto V4; se descarta la sugerencia genérica de usar `IsKindOf`. Los otros 52 avisos corresponden a código previo: 41 de patrones de preprocesador no soportados, 7 de lecturas de contexto y 4 de tipos exactos. No se modifican fuera de alcance.
- Herramientas ejecutadas con `python -B` desde `C:/Users/guill/DayZ-Modding-Knowledge-Pack/tools/dayz-script-validator/scripts/`, usando `script_validator.py .` y `ui_reconcile.py . --json`; no se generaron archivos auxiliares en la lane. Ningún resultado equivale a compilar Enforce.

## LO QUE NO PUDE VERIFICAR

- Compilación Enforce al cargar el mundo, incluida la herencia del contexto y el intercambio de referencias en el motor.
- Apariencia, DPI, texto, centrado, arrastre y geometría de widgets en las resoluciones indicadas.
- Entrega real de RPC, sincronización de vínculos, refresco del inventario y latencia/rate-limit de previews con dos clientes.
- Ciclo de vida real de Dabs, suscripción/desuscripción y limpieza de ambas UIs durante salida de misión.
- Exclusión efectiva en un binario retail y funcionalidad del hook en DayZDiag; se comprobó el preprocesado textual.
- El significado histórico exacto de S10 y si se pretendía otra guarda distinta de las localizadas.

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

- D-02 no admite una copia literal segura mientras convivan V3/V4: el scaler es global y Capture borra la captura anterior. Por eso el port necesita aislamiento de estado dentro de un archivo permitido.
- El 70 no es una petición sin emisor cliente: es una notificación sin productor servidor en esta base. El productor compartido emite 34. La solución cliente consume la señal común de 34/70/ACK 69; no inventa un endpoint servidor para el 70.
- S10 no se reproduce en las guardas de preview leídas: ambos controllers son equivalentes en los tres métodos principales, salvo diagnóstico/nombres. La diferencia de energía constatada afecta a edición y apertura. No debe confundirse con una protección de preview ya existente.
- Portar el scaler no demuestra por sí solo aptitud para producción ni escalado perfecto a cualquier DPI: conserva heurísticas y límites del scaler V3. Esa afirmación exige las pruebas de motor que el brief excluye expresamente de esta corrida.
