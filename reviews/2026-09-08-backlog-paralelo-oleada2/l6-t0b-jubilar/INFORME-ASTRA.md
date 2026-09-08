# T0b — Retirada de la interfaz V3 del sorter

**Resultado: ARREGLADA en código y comprobaciones estáticas.** La interfaz V4 es la única implementada y su acción sirve tanto a LFPG_Sorter como a LFPG_Sorter_TEST. La entidad base, ambos kits, los classnames de config y la persistencia permanecen. **La carga y el comportamiento in-game siguen INCONCLUSOS:** no hay compilador Enforce independiente ni juego que ejecutar aquí.

Este informe sustituye íntegramente al anterior y aplica el alcance corregido por el propietario. Los veredictos ARREGLADA describen el parche implementado; no significan que se haya ejecutado el motor.

## Base y decisiones de alcance

- HEAD comprobado: d59cad892557d8ec8dcfed0bfca0ba1c5744db45. Sigue siendo distinto del 8de29d5 del primer brief. Se localizaron todos los puntos por contenido.
- Se conserva el censo aceptado: **6 archivos Enforce, 9 clases y 3 layouts** en el conjunto inicial. El alcance corregido elimina **5 archivos Enforce con 7 clases y 3 layouts**, y conserva las dos clases de LFPG_Sorter.c.
- Se interpreta «LFPG_Sorter.c entero» como conservar sus clases e implementación, con la sustitución imprescindible de la acción en SetActions que exige el punto 4. Solo cambia esa línea: scripts/4_World/LFPG_Sorter.c:111. No se convierte la entidad en un stub ni se reescriben campos, constructores o hooks.
- El panel único conserva todos los nombres _TEST. No se toca config.cpp, no se cambia el formato persistido y no se añade una migración.
- Se han modificado 9 archivos existentes y borrado 8, todos dentro de la lista blanca corregida. INFORME.md es el único artefacto escrito adicional. No se usaron commit, add, checkout, stash ni reset.

### 1. Censo aceptado y borrado de UI V3

**Veredicto: ARREGLADA.**

| Archivo del censo inicial | Clases declaradas | Resultado |
|---|---|---|
| scripts/4_World/LFPG_SorterView.c | LFPG_SorterView | Borrado entero |
| scripts/4_World/LFPG_SorterController.c | LFPG_SorterController | Borrado entero |
| scripts/4_World/LFPG_SorterTagView.c | LFPG_SorterTagController; LFPG_SorterTagView | Borrado entero |
| scripts/4_World/LFPG_SorterPreviewRow.c | LFPG_SorterPreviewRowController; LFPG_SorterPreviewRow | Borrado entero |
| scripts/4_World/LFPG_ActionOpenSorterPanel.c | LFPG_ActionOpenSorterPanel | Borrado entero |
| scripts/4_World/LFPG_Sorter.c | LFPG_Sorter_Kit; LFPG_Sorter | Conservado; cambia únicamente la acción del panel |

Se borraron también gui/layouts/LFPG_Sorter.layout, gui/layouts/LFPG_SorterPreviewRow.layout y gui/layouts/LFPG_SorterTag.layout. Los paths borrados se enumeran sin números de línea porque ya no existen en el árbol final.

La sustitución queda conectada en scripts/4_World/LFPG_Sorter.c:111 y scripts/4_World/LFPG_ActionRegistration.c:68. V4 conserva su layout propio en scripts/4_World/test/LFPG_SorterView_TEST.c:378. Se mantienen sus vistas de etiquetas y preview y sus respectivos layouts bajo gui/layouts/test/.

**Motivo y alternativa descartada:** eliminar los archivos completos cumple la jubilación de UI sin mantener wrappers o aliases de sus siete clases. Borrar también la entidad habría roto la herencia V4 y el estado de los sorters colocados; ese objetivo fue retirado por el propietario.

**Verificación:** los ocho paths eliminados están ausentes; las siete clases retiradas no tienen referencias activas en scripts/config/layouts. El reconciliador de UI termina con cero incidencias después del borrado. In-game hay que abrir V4 sobre ambos tipos de sorter y verificar etiquetas, preview y edición de filtros sin errores de carga de layouts.

### 2. Retirar los mutex entre V3 y V4

**Veredicto: ARREGLADA.**

Se eliminó la consulta a la View V3 en la acción V4. En scripts/4_World/test/LFPG_ActionOpenSorterPanel_TEST.c:63 solo se comprueba si el panel V4 está abierto.

Se retiró completo el bloque de Open que rechazaba la apertura con «production sorter is already open», incluido su mensaje. En scripts/4_World/test/LFPG_SorterView_TEST.c:1398 comienza ahora la construcción diferida existente. El resto de este archivo es idéntico al anterior: permanecen el scaler, su aislamiento de estado, la sonda condicionada y las fronteras del hook de diagnóstico.

**Motivo y alternativa descartada:** ya no existe un segundo singleton de panel que arbitrar. Mantener un alias falso de IsOpen o dejar una comprobación contra una clase borrada ocultaría la dependencia o impediría la carga. Sí se conserva la guarda del panel V4 contra una segunda apertura.

**Verificación:** diff de View_TEST limitado al bloque eliminado y búsqueda sin referencias activas al singleton retirado. In-game, abrir/cerrar repetidamente, repetir la acción mientras llega el RPC y comprobar que hay un único panel, sin bloqueo de foco ni cursores residuales.

### 3. Lifecycle, registro, acciones y receptores RPC

**Veredicto: ARREGLADA.**

| Archivo / líneas finales | Cambio |
|---|---|
| scripts/5_Mission/LFPG_MissionInit.c:154 | Retirado Init V3; V4 sigue construyéndose de forma diferida al recibir su respuesta de apertura. |
| scripts/5_Mission/LFPG_MissionInit.c:181 | Retirado el bloque de teclas V3; se conserva el consumo de teclas y el manejo de ESC de V4. |
| scripts/5_Mission/LFPG_MissionInit.c:223 | Retiradas las consultas V3 en la liberación de ESC; se mantienen IsOpen e IsEscCooldown de V4. |
| scripts/5_Mission/LFPG_MissionInit.c:242 | Eliminados el cierre V3 y el arbitraje dual-open. El único bloque restante cierra V4 si no hay jugador, muere o queda inconsciente. |
| scripts/5_Mission/LFPG_MissionInit.c:436 | Retirado Cleanup V3; se conserva Cleanup V4. |
| scripts/4_World/LFPG_ActionRegistration.c:68 | Retirado el registro de la acción V3; la acción V4 sigue registrada una sola vez. |
| scripts/4_World/LFPG_Actions.c:528 | Las acciones de cableado solo consultan la apertura del panel V4. |
| scripts/4_World/LFPG_ActionSyncSorter.c:69 | La acción de enlace solo consulta la apertura del panel V4. |

Se eliminaron exactamente las **9 referencias V3** censadas en MissionInit. Al quitar el else de coexistencia se mantuvo el contenido del cierre V4 y se ajustó únicamente la indentación del bloque afectado, con tabs. No se cambió el lifecycle de ATM, CCTV u otros dispositivos.

**Clasificación y retirada de RPC:** se eliminaron cinco ramas de dispatch y sus cinco implementaciones cliente: HandleSorterConfigResponse, HandleSorterSaveAck, HandleSorterResyncAck, HandleSorterPreviewResponse y HandleSorterSortAck. Ya no hay emisores de esas solicitudes legacy en las acciones/controladores del cliente actual: apertura, guardado, preview, ordenación y resync usan la ruta V4.

Resync también se unificó para todos los sorters en scripts/4_World/LFPG_ActionSyncSorter.c:101. Esto permite retirar su receptor legacy sin dejar al sorter antiguo sin confirmación de enlace. Se corrigió además el comentario que ordenaba regenerar los handlers V4 desde la V3 ahora borrada.

Los seis receptores V4 se conservan, con sus cuerpos idénticos al HEAD inicial:

| Respuesta V4 | Dispatch final | Método final |
|---|---|---|
| SORTER_TEST_CONFIG_RESPONSE | scripts/4_World/LFPG_RPCClientHandler.c:51 | scripts/4_World/LFPG_RPCClientHandler.c:621 |
| SORTER_TEST_SAVE_ACK | scripts/4_World/LFPG_RPCClientHandler.c:55 | scripts/4_World/LFPG_RPCClientHandler.c:692 |
| SORTER_TEST_RESYNC_ACK | scripts/4_World/LFPG_RPCClientHandler.c:59 | scripts/4_World/LFPG_RPCClientHandler.c:733 |
| SORTER_TEST_PREVIEW_RESPONSE | scripts/4_World/LFPG_RPCClientHandler.c:63 | scripts/4_World/LFPG_RPCClientHandler.c:775 |
| SORTER_TEST_SORT_ACK | scripts/4_World/LFPG_RPCClientHandler.c:67 | scripts/4_World/LFPG_RPCClientHandler.c:701 |
| SORTER_TEST_CARGO_REFRESH | scripts/4_World/LFPG_RPCClientHandler.c:71 | scripts/4_World/LFPG_RPCClientHandler.c:725 |

**SORTER_CARGO_REFRESH se conserva**, en scripts/4_World/LFPG_RPCClientHandler.c:46 y scripts/4_World/LFPG_RPCClientHandler.c:483. No es exclusivo de la UI V3. El servidor lo selecciona en scripts/5_Mission/LFPG_NetworkManagerImpl.c:6339 y lo envía a jugadores cercanos desde el broadcast compartido. Lo usan el tick en scripts/5_Mission/LFPG_NetworkManagerImpl.c:6292 y las rutas de ordenación manual en scripts/5_Mission/LFPG_NetworkManagerImpl.c:6503 y scripts/5_Mission/LFPG_NetworkManagerImpl.c:6602. Su cuerpo cliente también permanece idéntico.

No se renumeraron enums ni se eliminaron los endpoints legacy del servidor: están fuera de esta retirada de interfaz y fuera de la lista de archivos editables. Conservar esos endpoints no deja referencias a las clases UI borradas.

**Motivo y alternativa descartada:** unificar los emisores bajo V4 permite retirar receptores y bifurcaciones de coexistencia. Borrar indiscriminadamente todos los RPC sin sufijo habría eliminado el refresco de inventario de otros jugadores.

**Verificación:** cero referencias V3 en MissionInit; comprobación de los siete cuerpos RPC conservados contra HEAD; ruta completa de resync revisada hasta el dispatcher servidor. In-game, probar ESC, muerte, inconsciencia, fin de misión, reapertura, resync, guardado, preview y ordenación manual/automática. Con otro jugador cercano, ambos deben ver actualizarse los contenedores sin relog.

### 4. Panel para los sorters ya colocados y conservación de entidades

**Veredicto: ARREGLADA en código; pendiente la comprobación funcional con un mundo guardado.**

El problema se encontraba en tres puntos: la base añadía la acción V3, la acción V4 exigía el classname exacto _TEST al ofrecerse y lo volvía a exigir al ejecutarse. Se corrigieron los tres:

1. scripts/4_World/LFPG_Sorter.c:111 añade LFPG_ActionOpenSorterPanel_TEST en la base compartida. También permanece LFPG_ActionSyncSorter en la línea 112.
2. scripts/4_World/test/LFPG_Sorter_TEST.c:15 mantiene la herencia y elimina el override de SetActions que añadía otra vez esa acción. Tanto la entidad antigua como la variante heredan una sola ruta de apertura.
3. scripts/4_World/test/LFPG_ActionOpenSorterPanel_TEST.c:50 valida el objetivo mediante LFPG_Sorter.Cast, por lo que admite la base y sus derivados. No restringe la oferta al string LFPG_Sorter_TEST.
4. scripts/4_World/test/LFPG_ActionOpenSorterPanel_TEST.c:87 aplica la misma comprobación al enviar el RPC. Se usa SORTER_TEST_CONFIG_REQUEST en scripts/4_World/test/LFPG_ActionOpenSorterPanel_TEST.c:96, con el NetworkID del objeto realmente seleccionado.

El servidor ya acepta la clase base: el dispatch TEST entrega la solicitud al handler común en scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:126, y este hace LFPG_Sorter.Cast en scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2509. La respuesta TEST llega al panel único. El controlador V4 también resuelve la entidad como LFPG_Sorter en scripts/4_World/test/LFPG_SorterController_TEST.c:514. No ha sido necesario cambiar esos archivos de solo lectura.

Se conservan las condiciones de jugador/objetivo válidos, energía, ruina, distancia, panel ya abierto y enlace. Las comprobaciones de energía y enlace leen las funciones reales LFPG_IsPowered y LFPG_IsLinked, definidas en scripts/4_World/LFPG_Sorter.c:165 y scripts/4_World/LFPG_Sorter.c:621. No se añade una dependencia de World hacia Mission.

| Dato / operación | Lado y mecanismo conservado |
|---|---|
| Energía y enlace de la entidad | Estado de la base compartida; SyncVars registradas en scripts/4_World/LFPG_Sorter.c:94. |
| Apertura, input y estado del panel | Cliente; una acción y un singleton V4. |
| Solicitud de config y respuesta | Acción cliente, RPC TEST, handler servidor compartido y receptor cliente V4. |
| Filtros y enlaces guardados | Hooks de entidad compartidos, sin cambio de formato ni lector. |
| Refresco de cargo de observadores | Broadcast servidor legacy conservado y señal cliente LFPG_CargoRefreshSignal existente. |

**Persistencia y config:** config.cpp no tiene diff. LFPG_Sorter_Kit en config.cpp:1039, LFPG_Sorter en config.cpp:1059 y sus derivados V4 en config.cpp:1080 y config.cpp:1086 conservan nombres, padres y propiedades. Las dos clases script de la base siguen en scripts/4_World/LFPG_Sorter.c:23 y scripts/4_World/LFPG_Sorter.c:35. El kit TEST sigue en scripts/4_World/test/LFPG_Sorter_TEST.c:7, con su mismo classname de despliegue.

Los hooks LFPG_OnStoreSaveDevice en scripts/4_World/LFPG_Sorter.c:332 y LFPG_OnStoreLoadDevice en scripts/4_World/LFPG_Sorter.c:339 siguen intactos. El archivo completo coincide con el original salvo la única línea AddAction. No se cambia el orden de campos guardados ni de SyncVars y no se reemplaza ninguna entidad persistida.

**Motivo y alternativa descartada:** cambiar la acción de la base conecta los sorters existentes sin migrarlos. Duplicar la acción para cada classname, renombrar _TEST o copiar la implementación de la entidad habría añadido coexistencia o alterado el contrato persistente.

**Verificación estática:** un control sobre la jerarquía real de clases da positivo para LFPG_Sorter y LFPG_Sorter_TEST y negativo para LFPG_Sorter_Kit, LFPG_Sorter_TEST_Kit y LFPG_Furnace. Ambas comprobaciones de la acción usan el cast base; existe una sola inserción global y un solo AddAction del panel. Esta prueba de pertenencia a la familia NO ejecuta ActionCondition ni demuestra la aparición de la acción en el cursor.

**Prueba in-game prioritaria:** cargar una copia de un mundo que ya contenga un LFPG_Sorter colocado, alimentado y enlazado; abrirlo, comprobar que aparece V4 con sus filtros anteriores, modificar/guardar, ordenar y hacer resync. Repetir sobre LFPG_Sorter_TEST y sobre una entidad desplegada con cada kit. Guardar y reiniciar la copia, verificando que permanecen entidades, filtros, enlaces, cables y contenido. Los casos sin energía, sin enlace, arruinado o fuera de distancia deben conservar sus restricciones.

### 5. Grep cero, nombres y límites del diff

**Veredicto: ARREGLADA.**

Se buscó por los cinco símbolos del criterio corregido y también por LFPG_SorterTagController y LFPG_SorterPreviewRowController, descubiertos en el censo. **Resultado: cero coincidencias activas de las siete clases retiradas en scripts, config y layouts.** La búsqueda textual amplia incluyó archivos ocultos/ignorados y excluyó metadatos .git; las coincidencias documentales se clasificaron aparte.

Solo quedan estos tres comentarios históricos en fuente, expresamente permitidos por el criterio:

- scripts/4_World/LFPG_ActionOpenBTCAtm.c:22: referencia al patrón original de la acción del sorter.
- scripts/4_World/LFPG_BTCAtmView.c:692: referencia histórica al arreglo de memoria de la antigua View.
- scripts/4_World/test/LFPG_SorterView_TEST.c:34: nota que identifica el origen del fork V4.

LFPG_Sorter y LFPG_Sorter_Kit permanecen de forma deliberada. Tampoco se renombra ningún símbolo _TEST. El punto de integración final de la acción es scripts/4_World/LFPG_Sorter.c:111; las clases derivadas siguen en scripts/4_World/test/LFPG_Sorter_TEST.c:7 y scripts/4_World/test/LFPG_Sorter_TEST.c:15.

**Motivo y alternativa descartada:** los límites de palabra evitan confundir los nombres retirados con sus variantes _TEST. No se eliminaron comentarios, informes antiguos, enums o entidades compartidas para conseguir artificialmente un cero textual absoluto.

**Verificación:** búsqueda amplia con rg y segundo recorrido de fuentes que retira comentarios conservando strings y números de línea. Control positivo con un nombre retirado y negativo con los nombres _TEST. Los ocho archivos eliminados no existen y no quedan rutas activas a sus tres layouts. El primer arranque debe confirmar que World/Mission cargan y que la UI correcta se abre; grep no es un compilador.

## Comprobaciones ejecutadas

| Comprobación | Resultado y alcance |
|---|---|
| Archivos del diff contra lista blanca ampliada | PASS: 9 modificados y 8 borrados; config.cpp sin cambios. |
| Símbolos retirados y paths de layouts | PASS estático: cero referencias activas; tres comentarios históricos identificados. |
| Líneas Enforce añadidas | PASS: sin operadores/sintaxis prohibidos ni nuevas declaraciones ref; líneas indentadas nuevas con tabs. |
| Entidad y persistencia | PASS de conservación de contenido: LFPG_Sorter.c solo cambia AddAction; config intacto y clase/kit V4 conservados. |
| RPC V4 y cargo compartido | PASS: siete cuerpos de método idénticos al HEAD, con seis rutas TEST conservadas. |
| Jerarquía de tipos | PASS estático: dos tipos de dispositivo aceptados por la familia y tres controles negativos; no equivale a ejecución del motor. |
| CRLF | PASS: todos los archivos de código modificados conservan CRLF, sin LF sueltos. |
| Whitespace | PASS con git -c core.whitespace=blank-at-eol,blank-at-eof,space-before-tab,cr-at-eol diff --check. El check sin cr-at-eol señalaba los CR conservados en archivos cuyo blob ya los contiene; no se normalizaron para ocultarlo. |
| Índice | Sin cambios; no se ejecutaron operaciones de staging o commit. |

Se ejecutaron las herramientas reales de C:/Users/guill/DayZ-Modding-Knowledge-Pack/tools/dayz-script-validator/scripts/, con Python -B para evitar escribir cachés fuera del workspace:

- script_validator.py sobre el addon: antes, exit 2 / WARN, **0 errores y 56 avisos**; después, exit 2 / WARN, **0 errores y 51 avisos**. Se reducen de 7 a 2 los ES-GETTYPE-EXACT-MATCH. Permanecen 42 ES-EMPTY-IFDEF-UNSUPPORTED-PATTERN y 7 ES-CTX-READ-UNCHECKED. No se declara un linter limpio.
- Se contrastaron los avisos finales con los iniciales por contenido, regla y archivo. Tres avisos del receptor RPC solo cambiaron de posición: 761 a 512, 764 a 515 y 825 a 576. La correspondencia se verificó sobre líneas de fuente sin cambios; no hay avisos nuevos. Los avisos restantes describen patrones preexistentes que el linter no analiza o lecturas de ctx ajenas a esta retirada; no se ampliaron cambios para corregirlos.
- Tras retirar el comentario obsoleto de regeneración se repitió el linter sobre LFPG_RPCClientHandler.c: conserva solo los tres avisos preexistentes de lectura ctx, en líneas 512, 515 y 576.
- ui_reconcile.py sobre el addon: **exit 0, 0 FAIL y 0 WARN** antes y después. Pasa de 12 a 9 layouts y de 154 a 149 fuentes, coherente con los borrados.
- Diff final de archivos versionados: **17 archivos, 36 líneas añadidas y 7125 eliminadas**. El volumen procede principalmente de los ocho borrados íntegros; no hay normalización global ni refactor ajeno.
- No se invocó ningún comando de build, compilador Enforce ni juego. Los scripts de comprobación específicos se ejecutaron en memoria, sin añadir archivos fuera de la lista blanca. No se ejecutó el gate externo del receptor.

## Riesgo y primer arranque del propietario

Una referencia de tipo/método a una clase borrada puede impedir que carguen World o Mission. Una ruta de layout residual puede dejar un panel sin widgets al abrirlo. Perder el receptor compartido de cargo puede causar inventarios visualmente desactualizados sin impedir el arranque. No se observó un crash ni un fallo de compilación: no se ejecutó el motor.

El riesgo principal de esta entrega es que una restricción real del pipeline de acciones o del motor invalide la apertura del panel V4 sobre una entidad persistida, aunque las rutas de fuente y la herencia estén conectadas. La conservación de los hooks evita introducir deliberadamente una migración, pero no sustituye una prueba de carga/guardado real.

1. Verificar el contenido/hash del artefacto desplegado y que el paquete ya no contiene los ocho archivos retirados; usar cliente y servidor con la versión prevista.
2. Arrancar una copia del guardado y revisar errores de carga de World/Mission, herencia y lectura de estado. Si no carga, FAIL; no confundirlo con un problema visual.
3. Abrir primero un LFPG_Sorter previamente colocado, con energía y enlace; confirmar panel V4, filtros anteriores, guardado, preview, ordenación y resync. Repetir en el subtipo _TEST y con los dos kits.
4. Con dos jugadores próximos, probar ordenación manual y automática; verificar cantidades y refresco de ambos contenedores sin relog.
5. Probar cierre por ESC, muerte e inconsciencia; salir/reentrar en misión y reabrir. Comprobar cursor, movimiento y ausencia de UI fantasma.
6. Guardar y reiniciar la copia del mundo: comparar entidades, filtros, enlaces, cables y contenido. Comprobar también restricciones de energía/enlace/ruina/distancia y escalado del panel a distintas resoluciones.

## Entrega y continuidad

Se deja el diff en el árbol de trabajo, sin commit ni staging, para el revisor de otra familia. No se modificaron los archivos de arranque, logs o mensajes de la lane que ya estaban presentes. INFORME.md sustituye al informe de bloqueo inicial y conserva su censo aceptado.

No se escribe memoria ni handoff en el vault, porque están fuera de la raíz autorizada. Este informe contiene el cambio, las decisiones, la evidencia y la comprobación pendiente para continuar. **No se ha encontrado ningún bloqueo residual que requiera editar otro archivo fuera de la lista blanca.**

## LO QUE NO PUDE VERIFICAR

- Carga real de los módulos, del cliente, del servidor o del mundo: no hay ejecución de Enforce ni juego en este entorno.
- Aparición y ejecución reales de la acción V4 sobre un LFPG_Sorter persistido; es la comprobación prioritaria del primer arranque.
- Guardado/reinicio de un mundo real con ambos tipos de entidad, filtros, enlaces, cables, kits y contenido de contenedores.
- Comportamiento de input, ESC, cierre por muerte/inconsciencia, reapertura y escalado de V4 a distintas resoluciones.
- Refresco de cargo y conservación de contenido con dos jugadores durante ordenación manual y automática.
- Paquete final, contenido/hash desplegado e integración con las otras lanes o con extensiones de terceros.
- Los patrones que el linter marca como no analizados; sus 51 avisos preexistentes no equivalen a cobertura completa.
- No falta permiso sobre ningún archivo necesario para este alcance corregido; los endpoints, constantes y bases compartidas que permanecen no son bloqueos de la retirada de UI.

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

- El primer encargo confundía la interfaz V3 con una entidad base que V4 sigue necesitando. La corrección resuelve esa contradicción: borrar sus dos clases habría sido incompatible con la herencia y los mundos existentes.
- La acción no podía ampliarse solo en ActionCondition: existía otra restricción por classname al enviar el RPC. Ambas tenían que cambiar para que un sorter antiguo pudiera llegar al panel.
- SORTER_CARGO_REFRESH carece de sufijo TEST y aun así pertenece al flujo compartido en uso. Su nombre no permite clasificarlo como desechable.
- Conservar todos los classnames permite evitar una migración; que una entidad se llame LFPG_Sorter no significa que siga usando la interfaz retirada.
- La posibilidad de abrir un sorter sigue condicionada a energía, enlace, estado y distancia. «Ningún sorter sin panel» se aplica a disponer de una ruta de UI válida, no a eliminar esas guardas de producción.
- El censo original correcto es 6 archivos, 9 clases y 3 layouts; el borrado corregido es 5 archivos de script, 7 clases y 3 layouts. Son dos conjuntos distintos.
- Un grep cero y un linter sin errores no demuestran que el mod compile ni que la acción aparezca en el motor. El caso del sorter ya colocado sigue necesitando la prueba in-game indicada.
