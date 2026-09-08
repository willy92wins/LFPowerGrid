# INFORME — Lane 1 · T3 Dispositivos rotos o que mienten

Base comprobada: 8de29d5. Las fichas se localizaron por símbolos y contenido, no por las líneas del brief. Este brief es el plan aprobado. No se lanzó el juego, ningún compilador de Enforce ni otras lanes.

Resultado: D01, D02, D03 y D05 ARREGLADAS en código; D04 PENDIENTE-DECISION. Los veredictos describen el parche y la evidencia offline, no una validación de comportamiento en DayZ.

Solo se modifican LFPG_Furnace.c, LFPG_Battery.c, LFPG_BatteryAdapter.c y lfpg_devicebase.c bajo scripts/4_World/. Se entrega también este INFORME.md, autorizado expresamente aunque no aparece en la enumeración de la lista blanca. LFPG_NetworkManagerImpl.c, LFPG_Intercom.c y config.cpp quedan intactos.

## Decisiones y mapa cliente/servidor

| Dato | Autoridad y uso | Cliente / persistencia |
|---|---|---|
| Tiempo de combustión pendiente | Servidor; se pausa al apagar, cortar cables o morir | No se sincroniza; se guarda como duración entera en ms |
| Horno encendido y combustible | Servidor | SyncVars existentes; mismo orden de registro |
| Fuente térmica | Servidor; se activa tras construirla | Sin nueva variable de red |
| Energía de BatteryBase | Float autoritativo en servidor | Snapshot int X10 para visualización; float existente en disco |
| Recuperación del adaptador | Validación compartida de desmontaje y finalización en servidor | Kit declarado, batería retirada antes de desmontar |

### D01 — Plazo de consumo del horno

**Veredicto: ARREGLADA.**

Cambios:

- scripts/4_World/LFPG_Furnace.c:63 incorpora m_BurnRemainingMs; m_BurnNextMs sigue siendo exclusivamente un deadline de la misión actual.
- scripts/4_World/LFPG_Furnace.c:267 reconstruye el deadline al restaurar y liquida un intervalo ya vencido antes de registrar el horno encendido.
- scripts/4_World/LFPG_Furnace.c:305, scripts/4_World/LFPG_Furnace.c:333 y scripts/4_World/LFPG_Furnace.c:559 conservan el resto antes de pasar a apagado por muerte, corte de cables o acción del jugador.
- scripts/4_World/LFPG_Furnace.c:429 define versión propia 3; scripts/4_World/LFPG_Furnace.c:434 calcula el resto, scripts/4_World/LFPG_Furnace.c:448 lo guarda y scripts/4_World/LFPG_Furnace.c:456 carga primero en locales.
- scripts/4_World/LFPG_Furnace.c:514 inicia el siguiente intervalo cuando realmente se procesa el consumo. scripts/4_World/LFPG_Furnace.c:597 reanuda el resto al encender y llama inmediatamente al tick: alternar acciones entre sondeos del gestor tampoco permite eludir una deuda ya vencida.
- Cuando no queda combustible y se consume un nuevo objeto para encender, comienza un intervalo completo; no se regala otro intervalo por alternar con combustible existente.

Decisión: pausa mientras está apagado y durante el tiempo que el servidor no está funcionando. Es la lectura conservadora de “quema mientras está encendido”. Se descartó cobrar una unidad completa por cada encendido o redondear el consumo al guardar: evitaría ampliar el formato, pero penalizaría pulsaciones/guardados y no conservaría el tiempo restante. También se descartó persistir el deadline absoluto: GetTime es tiempo de misión, no una marca durable. Definición local verificada: C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts/3_game/global/game.c:1508; la [fuente primaria de Bohemia](https://github.com/BohemiaInteractive/DayZ-Script-Diff/blob/main/scripts/3_game/global/game.c) declara la misma semántica.

**Contrato de persistencia y rollback:**

El prefijo se conserva: EntityAI/base → IDs y deviceVer (lfpg_devicebase.c:371) → wireJSON (lfpg_wireownerbase.c:101) → datos del horno. El sufijo anterior era bool fuente, int combustible; v3 añade int restoMs, en ese orden. La versión solo cambia para LFPG_Furnace, no para los demás dispositivos.

| Entrada | Resultado del lector propio del horno | Campos propios consumidos / aplicación |
|---|---|---|
| Entidad nueva | Resto inicial de 30000 ms | Sin lectura; próximo guardado escribe v3 |
| v1 o v2 | Conserva fuente y combustible; asigna 30000 ms | Lee 2 campos y aplica ambos; el tiempo antiguo es irrecuperable |
| v3 válida, resto entre 0 y 30000 inclusive | Conserva los tres valores | Lee 3 campos; aplica al terminar todas las lecturas |
| Versión futura o menor que 1 | Rechazo | 0 campos propios; ninguna aplicación de estado propio |
| Truncada o tipo incompatible | Rechazo | Puede avanzar el cursor hasta el fallo; 0 estado propio aplicado |
| v3 con resto negativo o mayor que 30000 | Rechazo | 3 campos leídos; 0 estado propio aplicado |
| Mismo build de DayZ con nueva versión del mod | Decide por deviceVer, no por el build | Mismas reglas anteriores |
| Lector de 8de29d5 ante datos v3 | NO es un rollback compatible garantizado | Acepta versión 3 y lee solo los 2 campos antiguos; deja 1 sin leer |

No hay migración externa ni escritura de una partida en esta lane. Antes de desplegar hay que copiar y verificar la persistencia con el servidor detenido. Para volver al lector anterior: conservar aparte la persistencia v3 y restaurar la copia previa junto con el código anterior. Eso revierte también el progreso posterior a la copia; conservar ese progreso requiere una conversión validada que no está implementada aquí. No se presupone que el motor tolere el campo sobrante. Los rechazos devuelven false: el efecto del motor puede ser descartar la entidad; no se promete conservar una entidad inválida en el mundo. El staging cubre los campos del horno, no las mutaciones previas de sus clases base.

Comprobación offline: simulación JavaScript de los cuerpos extraídos del lector y del cálculo del resto, sustituyendo declaraciones primitivas, acceso a miembros y ctx por dobles tipados. 13 casos de carga y 4 de tiempo con expectativas explícitas: legacy 1/2; v3 parcial, vencida y completa; futuro; versión 0; truncamiento en cada posición; tipo erróneo; resto -1/30001; reloj activo/vencido/pausado. Ningún rechazo aplicó estado propio. No es ejecución de Enforce ni validación del formato binario del motor. El lector anterior extraído aceptó v3 y dejó un campo sin leer, por lo que el control negativo distingue ambos lectores.

Verificación in-game pendiente: horno con combustible conocido y cargo vacío; encender 25 s, apagar, esperar y reencender: debe consumir tras los 5 s activos restantes, con la latencia del sondeo (~5 s). Repetir ciclos cortos durante varios minutos y encender con resto 0 antes del siguiente sondeo. Repetir guardado/reinicio con horno encendido y apagado, incluyendo resto 0, agotamiento del último combustible y autoalimentación con cargo. Probar corte de cables y restauración; comprobar energía, combustible, autoapagado y ausencia de registros duplicados. FAIL si se concede un nuevo intervalo completo en cada ciclo.

### D02 — Restauración de calor antes de crear la fuente

**Veredicto: ARREGLADA.**

scripts/4_World/LFPG_Furnace.c:149 aplica LFPG_SetHeatActive(m_SourceOn) inmediatamente después de crear m_UTSource. Se retira el intento prematuro del hook LFPG_OnInitDevice, que sigue arrancando en scripts/4_World/LFPG_Furnace.c:259.

Mecanismo comprobado: lfpg_devicebase.c:267 llama LFPG_OnInit; lfpg_wireownerbase.c:77 llama el hook del dispositivo. Por tanto, super.EEInit ejecuta ese hook antes de volver al cuerpo de EEInit del horno. La creación está en LFPG_Furnace.c:147. UniversalTemperatureSource arranca desactivada y expone SetActive: fuente local C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts/3_game/systems/universaltemperaturesource/universaltemperaturesource.c:76 y :165.

Se descartó mover toda la creación de UTS antes de super.EEInit: cambiaría el orden de inicialización de la entidad. Reaplicar el estado final una vez creada la dependencia es suficiente y respeta FurnaceHeatEnabled. La activación normal por acción sigue usando el helper existente.

Verificación in-game pendiente: guardar un horno encendido y con combustible, reiniciar, acercarse sin tocar el interruptor y comprobar calentamiento. Control negativo: horno apagado, combustible agotado o FurnaceHeatEnabled=false no deben emitir calor. Comprobar que al apagar se detiene y al volver a encender vuelve a funcionar.

### D03 — Cuantización de energía autoritativa

**Veredicto: ARREGLADA.**

- scripts/4_World/LFPG_Battery.c:78 añade m_StoredEnergy como float autoritativo.
- scripts/4_World/LFPG_Battery.c:351 devuelve ese float en SERVER y conserva el snapshot X10 en cliente.
- scripts/4_World/LFPG_Battery.c:365 guarda el float antes de actualizar X10 y CompEM. CompEM recibe el valor autoritativo por el getter en scripts/4_World/LFPG_Battery.c:421.
- scripts/4_World/LFPG_Battery.c:301 persiste directamente el float; scripts/4_World/LFPG_Battery.c:340 restaura su precisión sin el recorrido por int. Las banderas y la energía se aplican juntas tras comprobar todas las lecturas. scripts/4_World/LFPG_Battery.c:513 inicializa el float al crear una batería nueva.
- scripts/4_World/LFPG_Battery.c:126 consulta el getter para bloquear desmontaje: una fracción positiva inferior a 0,1 no se pierde por un guard servidor basado en X10.
- El comentario sobre X10 en scripts/4_World/LFPG_Battery.c:63 se refiere ahora explícitamente a sincronización.

No fue necesario modificar el gestor. Sus llamadas reales están en scripts/5_Mission/LFPG_NetworkManagerImpl.c:7539 y :7722; el adaptador usa :7553 y :7731. Ambas rutas conservan el mismo contrato de getter/setter. El adaptador ya lee el float de CompEM en scripts/4_World/LFPG_BatteryAdapter.c:475. Se rastrearon también los consumidores de LEDs e inspector: siguen usando el mismo getter y reciben el snapshot en cliente.

Se descartó aumentar el factor X10 o sincronizar otro float: mantendría realimentación cuantizada o cambiaría el protocolo visual sin necesidad. El formato de batería sigue siendo float, bool, bool, con la versión anterior. Los saves antiguos cargan el valor que tengan; la precisión que ya habían perdido no es recuperable. Un lector antiguo puede leer el mismo formato, aunque volverá a truncar su precisión.

Control aritmético independiente, con float32: desde 10 u, 1000 incrementos de 0,03 u dan 10 u con realimentación X10 y 39,9999886 u sin ella (esperado 40). Desde 100 u, 1000 decrementos de 0,003 u dan 0 u con X10 y 97,0016479 u con float32 (esperado 97). Este experimento ilustra el sesgo que se elimina; no ejecuta TickBatteries ni demuestra conservación energética completa del grafo.

Verificación in-game pendiente: medir valor servidor antes/después de muchos ticks con incrementos y autodescarga inferiores a 0,1 u, y compararlo con la integral esperada de entrada, salida, eficiencia y drenaje. Guardar/reiniciar con una fracción y comprobarla en servidor. Revisar máximos, agotamiento, histéresis, CompEM y LEDs en Medium/Large; repetir con adaptador usando sus propios límites. La UI conserva su resolución 0,1 y el umbral de sincronización existente. Con energía <0,1 puede mostrar cero y ofrecer desmontaje, pero el servidor debe rechazarlo mientras quede energía.

### D04 — Clase PAS ausente

**Veredicto: ARREGLADA.** El dueño del proyecto decidió aplicar la opción (a): declarar la clase. La opción (b) queda descartada y no se ha aplicado.

**Comprobaciones antes de escribir:** se buscó por contenido en todo config.cpp y no existía ninguna declaración de Land_Radio_PanelPAS; por tanto, se añadió una sola forward declaration. Tampoco existía LFPG_GhostPASBroadcaster. El hermano LFPG_GhostPASReceiver en config.cpp:2562 usa scope = 1. Se conserva ese mismo scope para el emisor interno creado mediante CreateObjectEx, sin convertirlo en una clase pública de scope 2.

**Cambio aplicado:** forward declaration en config.cpp:2574; bloque LFPG_GhostPASBroadcaster : Land_Radio_PanelPAS en config.cpp:2576, con scope = 1 en config.cpp:2578. Se insertó dentro de CfgVehicles, después del receptor y antes del cierre de CfgVehicles, localizado por contenido y balance de llaves. El bloque es copia literal de la opción (a) aprobada:

~~~cpp
	class Land_Radio_PanelPAS;

	class LFPG_GhostPASBroadcaster : Land_Radio_PanelPAS
	{
		scope = 1;
		displayName = "";
		descriptionShort = "";
		model = "\dz\gear\tools\stone.p3d";
		weight = 0;
		itemSize[] = {0, 0};
		itemBehaviour = 0;
		carveNavmesh = 0;
	};
~~~

**Verificación offline:** declaración y forward declaration únicas, scope igual al receptor y contenido insertado idéntico al bloque aprobado. Se preservaron los CRLF. El prefijo y sufijo del informe fuera de D04 permanecen idénticos byte a byte; LFPG_Intercom.c y las otras cuatro fichas no se modificaron.

**Verificación in-game pendiente:** comprobar que CreateObjectEx crea el emisor sin el error de clase ausente, que se activa al habilitar PAS y que la voz llega a LFPG_Speaker; apagarlo y comprobar su retirada. No se ha ejecutado el juego ni se declara compilación.

### D05 — Recuperación del BatteryAdapter

**Veredicto: ARREGLADA.**

scripts/4_World/LFPG_BatteryAdapter.c:84 corrige la promesa de recogida con F y scripts/4_World/LFPG_BatteryAdapter.c:85 devuelve LFPG_BatteryAdapter_Kit para la acción común de desmontaje. scripts/4_World/lfpg_devicebase.c:594 deja de enumerar el adaptador como excepción no desmontable.

Decisión: corregir el comentario Y restaurar la recuperación mediante kit. Cambiar solo el comentario dejaría intacto el bloqueo funcional. Se descartó habilitar recogida directa: los guards de inventario/colocación y la retirada de acciones pertenecen al contrato de dispositivo desplegado, y habilitarlos permitiría transportar un dispositivo registrado y potencialmente cableado. Además, config.cpp:2127 declara itemSize={0,0} y :2131 isDeployable=0 para el dispositivo.

La premisa “sin kit” no significa que la clase no exista. Está implementada en scripts/4_World/LFPG_BatteryAdapter.c:41 y declarada pública en config.cpp:2100; su despliegue devuelve el dispositivo. El problema era que el override devolvía una cadena vacía.

El circuito existente está leído: scripts/4_World/LFPG_ActionRegistration.c:116 registra la acción y :157 la añade al destornillador. scripts/4_World/LFPG_ActionDismantleDevice.c:94 consulta el kit, :99 bloquea attachments, :104 bloquea cargo, :116 consulta valor interno, :162 revalida al completar, :182 crea el kit y :211 borra el dispositivo. La eliminación ejecuta el cleanup por lfpg_devicebase.c:302 y :304, y el adaptador se desregistra en LFPG_BatteryAdapter.c:249. Se exige retirar la batería para conservar su carga mediante su CompEM existente.

Verificación in-game pendiente: desplegar adaptador, conectar cables y acoplar batería cargada; el desmontaje debe estar bloqueado mientras siga acoplada. Retirarla y comprobar carga; con destornillador, completar desmontaje, recuperar un único kit y comprobar limpieza de cables/grafo/registro. Redesplegarlo y volver a acoplar la batería. Controles negativos: sin herramienta, con herramienta arruinada, fuera de alcance, con attachment reinsertado durante la acción y dos jugadores intentando a la vez. Deben seguir bloqueadas la recogida y colocación directa del dispositivo desplegado.

## Verificación realizada y cierre

- Revisión del diff por contenido y de llamadas cruzadas; no hay llamadas nuevas desde 4_World hacia tipos de 5_Mission. Los métodos locales usados se verificaron en las definiciones citadas. Logging: scripts/3_Game/LFPG_Util.c:14 y :15.
- El linter disponible es C:/Users/guill/DayZ-Modding-Knowledge-Pack/tools/dayz-script-validator/scripts/script_validator.py. Ejecutado con python -B sobre el árbol: exit 2, WARN; 0 errores y 53 avisos (42 ES-EMPTY-IFDEF-UNSUPPORTED-PATTERN, 4 ES-GETTYPE-EXACT-MATCH, 7 ES-CTX-READ-UNCHECKED). Los otros archivos quedan fuera del parche.
- En los cuatro archivos modificados: adaptador y base PASS; batería y horno WARN, sin errores. El aviso de batería señala el nuevo #else que separa el getter servidor/cliente; el del horno señala el #else preexistente de AutoConsumeLargestItem. Se revisaron ambas ramas, que tienen cuerpo y cierre. El detector declara que no analiza #else; no se presenta su resultado como validación completa del preprocesador.
- Comprobación de líneas añadidas: sin ternarios, incrementos, asignaciones compuestas, foreach, Print ni ref; indentación nueva con tabs. Se conservan CRLF y ausencia de BOM en los cuatro archivos. git diff --check necesita core.whitespace=cr-at-eol para no tratar el CR de los archivos cuyo índice también conserva CRLF como whitespace sobrante.
- Se ejecutaron las simulaciones de D01 y el control aritmético de D03 descritos arriba. Son evidencia limitada y diferenciada de las pruebas in-game pendientes.
- Sin commit ni operaciones de índice, por prohibición expresa. No se modificó memoria fuera del workspace; este informe es el handoff autorizado. BRIEF.md, EXIT.start, events.jsonl y stderr.log ya estaban presentes y no son entregables editados por esta lane.

## LO QUE NO PUDE VERIFICAR

- Compilación Enforce al cargar un mundo, carga real del mod y diferencias SERVER/cliente en el motor.
- Consumo y propagación reales, latencia del sondeo, autoalimentación, registro de hornos y emisión de calor después de reinicio.
- Lectura/escritura binaria real, orden de callbacks de carga y restauración del resto en una partida existente; ninguna partida se migró aquí.
- Seguridad de downgrade a 8de29d5 con datos v3: el lector antiguo deja un campo sin leer; exige rollback de persistencia con copia previa, no solo volver el código.
- Conservación energética completa del grafo, precisión propia de float32, valores fuera de rango heredados, SyncVars, CompEM y representación cliente.
- Desmontaje real del adaptador, conservación de carga, condiciones concurrentes y limpieza efectiva de todos los cables y registros.
- Creación, alimentación, replicación y voz PAS de la opción D04(a), que no se ha implementado ni probado.
- Desbordamiento del reloj de misión en sesiones extremadamente largas, pérdida de progreso tras cierre abrupto anterior al último guardado y tratamiento del motor de entidades cuyo OnStoreLoad devuelve false.

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

- D01 no se resuelve guardando m_BurnNextMs tal cual: es tiempo de misión. Hace falta definir qué ocurre apagado y entre reinicios. Se eligió pausar; el brief no fijaba esa semántica. Los saves antiguos no contienen el progreso parcial y solo pueden recibir una migración con valor por defecto.
- El arreglo exacto de D01 requiere guardar un dato adicional. El lector anterior acepta versiones hasta 999 y no rechaza v3: la ampliación no puede prometer rollback de código sin restaurar datos. Esta limitación procede del lector existente, no de una prueba real de corrupción.
- D03 no necesita editar el gestor, y su lectura está hoy en :7539, fuera del tramo autorizado. Eliminar X10 del cálculo quita ese sesgo, pero no garantiza precisión infinita ni igualdad de energía entre baterías con capacidades, eficiencia y límites distintos.
- D04 confirma una clase ausente; declararla no demuestra por sí solo que alimentación, voz y ciclo de vida PAS funcionen correctamente. Retirar todo “broadcast” sería incorrecto: RF usa ese mismo estado.
- D05 sí tiene kit script y config. La falta era una vía hacia él. La “recogida con F” prometida contradice tanto guards y acciones como tamaño y despliegue del dispositivo; habilitarla cambiaría más contratos que aprovechar el desmontaje.
- Queda un comentario obsoleto en scripts/4_World/LFPG_ActionDismantleDevice.c:27 que aún dice que BatteryAdapter se recoge directamente. Se ha identificado, pero no se modifica porque ese archivo está fuera de la lista blanca.
- La lista blanca literal omite INFORME.md, aunque el entregable lo exige explícitamente. Se interpreta esa exigencia como autorización únicamente para añadir este informe, no para crear otros archivos de pruebas o memoria.
