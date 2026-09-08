# Triaje P2/P3 — Dispositivos

| ID | Veredicto | Fichero principal | Motivo |
|---|---|---|---|
| D06 | VIVA | `scripts/4_World/LFPG_ActionDismantleDevice.c` | Crea el kit y elimina el dispositivo sin copiar salud; el despliegue vuelve a crear otro objeto sin trasladarla. |
| D07 | VIVA | `scripts/4_World/LFPG_ActionUpgradeWaterPump.c` | El staging nuevo protege frente a fallos de creación, pero sobrantes y filtro siguen reconstruyéndose solo con cantidad. |
| D08 | VIVA | `scripts/4_World/LFPG_RemoteController.c` | Hay envíos al emparejar, cargar y podar durante activación; falta el envío al cambiar de dueño. |
| D09 | VIVA | `scripts/4_World/LFPG_RemoteController.c` | Los resets del mando y el routing diferido de MemoryCell se encolan sin cancelar ni fusionar los anteriores. |
| D10 | VIVA | `scripts/4_World/LFPG_IDevice.c` | Posición, nombre, dirección y etiqueta de puerto siguen usando Param + llamada dinámica para DeviceBase. |
| D11 | VIVA | `scripts/4_World/LFPG_Intercom.c` | Un evento sonoro sincronizado vuelve a escribir materiales y animaciones aunque su estado visual siga igual. |
| D12 | VIVA | `scripts/4_World/LFPG_Intercom.c` | RF obtiene todos los dispositivos y después filtra por distancia; GetAll sigue recorriendo el registro global. |
| D13 | VIVA | `scripts/4_World/LFPG_DoorController.c` | Durante backoff consulta GetPlayers por controlador antes incluso del límite de reintento mínimo. |
| D14 | VIVA | `scripts/4_World/LFPG_Furnace.c` | La estimación cliente usa siempre el cálculo recursivo; el consumo servidor sí distingue whitelist. |
| D15 | VIVA | `scripts/4_World/lfpg_devicebase.c` | El rango común 1–999 no valida el formato concreto; persisten lectores que ignoran la versión o aceptan valores inválidos. |
| D17 | VIVA | `scripts/4_World/LFPG_MotionSensor.c` | Persiste y compara literalmente grp.name; no hay identidad estable ni actualización automática de ese emparejamiento. |
| D18 | VIVA | `scripts/4_World/LFPG_SwitchV2Remote.c` | Las familias de switches conservan setters, RF y resolución de puertos duplicados; es deuda de mantenimiento. |
| D19 | VIVA | `scripts/4_World/LFPG_TestDevices.c` | LFPG_Generator llama a GetJSON sin caché propia; con wires no vacíos se serializa cada petición. |
| D20 | DUDOSA | `scripts/4_World/LFPG_MemoryCell.c` | El ruido con configuración actual murió antes de hoy; la emisión sin cambios permanece si se compila con Debug. |
| D21 | VIVA | `scripts/4_World/LFPG_IDevice.c` | La selección final permite distancias menores que searchRadius + 1, aunque el contrato dice searchRadius. |
| D22 | VIVA | `scripts/4_World/LFPG_Furnace.c` | EEInit continúa tras el retorno de la base y puede crear la fuente térmica de una proyección en servidor. |
| D23 | VIVA | `scripts/4_World/LFPG_RemoteController.c` | Cada guardado extrae repetidamente el mínimo; el coste cuadrático existe, limitado normalmente a 32 parejas. |
| D24 | VIVA | `scripts/4_World/LFPG_BatteryAdapter.c` | El getter de energía usa una referencia poblada solo en servidor, pese a sincronizar energía; la tasa sí usa su SyncVar. |

**Resultado: 17 VIVA, 1 DUDOSA, 0 MUERTA y 0 NO-LOCALIZADA.** VIVA identifica un mecanismo presente en fuente; no convierte una hipótesis condicionada por motor, configuración o terceros en un fallo reproducido.

Inspección estática del workspace a **2026-09-08**, HEAD `d59cad892557d8ec8dcfed0bfca0ba1c5744db45`. Rutas relativas a la raíz y líneas del contenido actual, incluida la capitalización real de `lfpg_devicebase.c`. No se ha ejecutado compilador ni juego. Se leyeron implementaciones, consumidores, guardas de compilación y llamadores; el historial solo se usó para fechar correcciones ya verificadas en código. No se dedujo vigencia de que un fichero estuviera intacto.

## Evidencia por ficha

### D06 — VIVA — Salud no trasladada entre dispositivo y kit

- **Evidencia:** `scripts/4_World/LFPG_ActionDismantleDevice.c:182` crea el kit; `scripts/4_World/LFPG_ActionDismantleDevice.c:198` solo lo coloca y `scripts/4_World/LFPG_ActionDismantleDevice.c:211` elimina el dispositivo. Los despliegues crean otra entidad en `scripts/4_World/LFPG_KitBase.c:193` y `scripts/4_World/LFPG_KitBaseDeployable.c:138`, sin copiar salud antes de programar el borrado del kit en las líneas 208 y 153, respectivamente.
- **Qué está mal:** se pierde el estado de salud de la entidad sustituida; un dispositivo dañado puede volver con la salud inicial que determine la creación. La validación del desmontaje comprueba inventario y valor bloqueante, pero no preserva salud; el único daño aplicado por esa acción es al destornillador (`scripts/4_World/LFPG_ActionDismantleDevice.c:204`). No doy por medido que toda clase nueva nazca al 100 %.
- **Coste: AMPLIO.** Debe conservarse el estado en ambos sentidos y en las dos bases de kits; arreglar solo el desmontaje deja abierto el segundo salto.
- **Solapamiento:** comparte ambas bases de kit con el ejemplo de duplicación D18 y el despliegue que permite D22. D07 comparte el problema de sustituir entidades, pero está en otras acciones. D24 usa la base de kit normal para el adaptador.

### D07 — VIVA — Sobrantes y filtro reconstruidos sin su estado

- **Evidencia:** `scripts/4_World/LFPG_ActionUpgradeSolarPanel.c:217` crea el sobrante por classname y `scripts/4_World/LFPG_ActionUpgradeSolarPanel.c:226` copia solo cantidad; la misma secuencia está en `scripts/4_World/LFPG_ActionUpgradeWaterPump.c:258` y `scripts/4_World/LFPG_ActionUpgradeWaterPump.c:267`. El filtro se captura como cantidad en `scripts/4_World/LFPG_ActionUpgradeWaterPump.c:148`, se reconstruye con el classname fijo `GasMask_Filter` en la línea 201 y solo recibe cantidad en la 218. La bomba original se elimina en la 239.
- **Qué está mal:** los sobrantes pierden salud y cualquier otro estado no representado por classname/cantidad; el filtro pierde además su posible subtipo, y con cantidad cero ni siquiera se recrea. La protección de hoy ante fallos de creación conserva los originales al abortar, pero la ruta de éxito todavía reemplaza los objetos sin trasladar esos datos.
- **Coste: AMPLIO.** Abarca las dos acciones de mejora; debe conservarse la atomicidad del staging actual.
- **Solapamiento:** ambas funciones `StageMaterialSurplus` y `AbortStagedUpgradeOutputs` son duplicación adicional para D18. Relacionada conceptualmente con D06, sin compartir su función.

### D08 — VIVA — El nuevo dueño recibe una caché de parejas vacía o antigua

- **Evidencia:** `scripts/4_World/LFPG_RemoteController.c:365` resuelve al dueño actual y envía la lista, pero sus llamadores son la carga diferida de la línea 708 y la activación **solo si** `listChanged` en la 520. El otro envío está en `scripts/4_World/LFPG_ActionPairRemote.c:179`, después de modificar una pareja. La caché cliente se inicializa vacía en `scripts/4_World/LFPG_RemoteController.c:94` y se rellena por RPC en la 395.
- **Qué está mal:** transferir el mando sin cambiar sus parejas no provoca sincronización; el nuevo cliente puede leer «Emparejar» para un dispositivo ya emparejado. La consecuencia no es solo texto: `scripts/4_World/LFPG_ActionPairRemote.c:71` decide la etiqueta con la caché cliente, mientras el servidor decide **desemparejar** con su lista real en la línea 120.
- **Comprobación del llamador ausente:** búsqueda global en `scripts/` de `LFPG_SyncToOwner`, `LFPG_SyncPairedListToClient`, `LFPG_RPC_REMOTE_PAIR_SYNC`, `m_ClientPairedIds`, overrides de inventario y referencias a `LFPG_RemoteController`; no aparece puente de cambio de dueño, pickup, login o petición cliente que cierre el hueco. La activación normal sigue disponible y no exige parejas cliente (`scripts/4_World/LFPG_ActionActivateRemote.c:35`); no se afirma que el mando quede inutilizable.
- **Coste: ACOTADO.** El contrato de RPC ya existe; el enganche de transición y reintento puede residir en el mando.
- **Solapamiento:** mismo fichero que D09 y D23, y lector de persistencia incluido en D15; D09 comparte además el callback de sincronización diferida de la línea 708.

### D09 — VIVA — Callbacks pendientes sin cancelación ni coalescencia local

- **Evidencia:** `scripts/4_World/LFPG_RemoteController.c:539` y `scripts/4_World/LFPG_RemoteController.c:559` encolan resets de LED y botón sin retirar resets previos. `scripts/4_World/LFPG_MemoryCell.c:184` encola routing al cambiar el estado y `scripts/4_World/LFPG_MemoryCell.c:267` añade otro durante inicialización; el callback solicita propagación en la línea 253.
- **Qué está mal:** una segunda pulsación puede dejar pendiente el apagado de la primera y acortar su flash; MemoryCell puede ejecutar varias veces el trabajo diferido leyendo el mismo estado final. No hay cancelación de esos métodos ni hook propio de borrado que la efectúe: la base llama al hook en `scripts/4_World/lfpg_devicebase.c:302`, cuyo valor por defecto es vacío en la línea 582.
- **Matiz:** MemoryCell ya limita el encolado a `stateChanged`, y no se ha demostrado ejecución sobre una entidad destruida ni crash. El mando programa LED durante 2.000 ms (`scripts/4_World/LFPG_RemoteController.c:36`); incluso una activación rechazada por cooldown acaba llamando al flash en `scripts/4_World/LFPG_ActionActivateRemote.c:93`. Que el gestor agrupe propagaciones no cancela estos callbacks previos.
- **Coste: AMPLIO.** Dos clases con callbacks y ciclos de vida distintos.
- **Solapamiento:** mando con D08, D15 y D23; MemoryCell con D20 y la parte ya optimizada de D11. No confundir con los callbacks de switches RF, que sí tienen cancelaciones propias.

### D10 — VIVA — Faltan accesos directos en la fachada de puertos

- **Evidencia:** `scripts/4_World/LFPG_IDevice.c:507`, `scripts/4_World/LFPG_IDevice.c:840`, `scripts/4_World/LFPG_IDevice.c:857` y `scripts/4_World/LFPG_IDevice.c:873` construyen parámetros y usan llamadas dinámicas. Sus helpers llegan a `CallFunctionParams` en las líneas 437, 455 y 464. Los métodos directos ya existen en `scripts/4_World/lfpg_devicebase.c:104`, `scripts/4_World/lfpg_devicebase.c:116`, `scripts/4_World/lfpg_devicebase.c:128` y `scripts/4_World/lfpg_devicebase.c:152`.
- **Qué está mal:** se mantiene asignación y resolución dinámica para objetos cuyo tipo base ya permite una llamada directa. Continúan los consumidores: renderer en `scripts/4_World/LFPG_CableRenderer.c:2144` y enumeración de puertos en `scripts/5_Mission/LFPG_NetworkManagerImpl.c:3329`; la optimización de otros métodos de la fachada no cubre estos.
- **Coste: ACOTADO.** Un fichero de fachada, manteniendo el fallback necesario para generador, vanilla y terceros.
- **Solapamiento:** mismo fichero que D21 y puente de D19/D24. D18 no exige que todas las familias hereden una base universal para resolver esto.

### D11 — VIVA — Reescritura visual con estado idéntico

- **Evidencia:** `scripts/4_World/LFPG_Intercom.c:380` llama siempre a `LFPG_UpdateVisuals`; esta vuelve a escribir materiales desde la línea 479, animaciones en las líneas 504 y 519 y micrófono en la 524. RF solo cambia el evento sonoro y su secuencia y marca dirty en `scripts/4_World/LFPG_Intercom.c:668`, por lo que existe un motivo de sincronización independiente de esos visuales.
- **Qué está mal:** el beep RF provoca el mismo trabajo visual aun cuando potencia, interruptor, frecuencia y radio no cambian. También quedan setters visuales sin caché en `scripts/4_World/LFPG_SwitchRemote.c:261` y `scripts/4_World/LFPG_SwitchV2Remote.c:207`; no se ha cuantificado su coste de motor.
- **Corrección parcial existente:** MemoryCell sí compara `desiredState` y retorna en `scripts/4_World/LFPG_MemoryCell.c:323`. No justifica cerrar la ficha para Intercom y las otras familias.
- **Coste: AMPLIO.** La descripción cubre varios dispositivos; Intercom por separado sería ACOTADO.
- **Solapamiento:** Intercom con D12 y D15; switches con D18; MemoryCell con D09/D20.

### D12 — VIVA — RF hace una consulta global para un alcance local

- **Evidencia:** `scripts/4_World/LFPG_Intercom.c:628` asigna el array y pide `GetAll`; recorre todo en la línea 636 y solo después aplica distancia y capacidad RF en las líneas 645 y 649. `scripts/4_World/LFPG_DeviceRegistry.c:92` recorre todos los IDs y copia sus entidades en la línea 110.
- **Qué está mal:** el coste de una activación local crece con dispositivos de todo el mapa, además de las asignaciones del array y del mapa de deduplicación. El cooldown de `scripts/4_World/LFPG_Intercom.c:623` limita frecuencia, pero no el tamaño de la consulta.
- **Corrección parcial existente:** GetAll ya deduplica mediante mapa (`scripts/4_World/LFPG_DeviceRegistry.c:89`); desapareció el antiguo escaneo cuadrático de deduplicación, no el recorrido global denunciado por D12.
- **Coste: AMPLIO.** Estimación conservadora para conservar exactamente los candidatos válidos del registro usando un índice local/RF y mantenerlo durante el ciclo de vida. Una consulta espacial directa en un único fichero requiere comprobar antes su equivalencia de cobertura.
- **Solapamiento:** misma función que el disparador visual de D11; mismo fichero que D15. No agrupar automáticamente con D13: el registro de dispositivos y el de jugadores son distintos.

### D13 — VIVA — Backoff con escaneo de jugadores por controlador

- **Evidencia:** `scripts/4_World/LFPG_DoorController.c:385` entra en backoff; en la línea 396 consulta todos los jugadores y en la 402 recorre el resultado. La guarda del reintento mínimo está **después**, en la línea 415. `scripts/5_Mission/LFPG_NetworkManagerImpl.c:6888` recorre los controladores cada segunda fase y llama a `LFPG_OnDoorPoll` en la línea 6897.
- **Qué está mal:** varios controladores sin pareja repiten la misma obtención global de jugadores incluso mientras aún no les corresponde buscar puerta. Se reutiliza el array por instancia, pero el trabajo sigue creciendo con controladores sin pareja por jugadores consultados; con ningún jugador cercano se recorre la lista completa.
- **Coste: AMPLIO.** Para eliminar la consulta repetida entre controladores hace falta compartir candidatos/proximidad con el planificador; mover una guarda solo reduce una parte.
- **Solapamiento:** el fichero también contiene persistencia dentro del ámbito D15. Comparte tipo de coste con D12, no función ni colección.

### D14 — VIVA — Estimación de combustible con reglas diferentes del consumo

- **Evidencia:** `scripts/4_World/LFPG_Furnace.c:781` define la estimación cliente y en la línea 795 llama siempre a `LFPG_CalcFuelRecursive`. El servidor sí selecciona whitelist en las líneas 814 y 826, usando `LFPG_CalcFuelWhitelist` en la 829. El cálculo recursivo lee `itemSize` en la línea 642 y `canBeSplit` en la 654 para cada objeto, incluidos contenidos recursivos.
- **Qué está mal:** en modo whitelist la interfaz puede prometer combustible de objetos que el horno no consumirá, o calcular una cantidad distinta de la permitida. La caché visual del inspector no evita el cálculo: obtiene la estimación en `scripts/4_World/LFPG_DeviceInspector.c:672` antes de comparar el snapshot en la línea 691, y vuelve a obtenerla al construir el texto en la 1120.
- **Matiz:** la whitelist en sí ya tiene caché por classname (`scripts/3_Game/LFPG_Settings.c:717`); las lecturas repetidas denunciadas son las propiedades de config usadas por las fórmulas. No se afirma pérdida de ítems por este getter.
- **Coste: AMPLIO.** Para cerrar la ficha funcionalmente hay que alinear la estimación cliente con la política efectiva del servidor y evitar repetir config. `LFPG_Settings` carga desde el perfil local (`scripts/3_Game/LFPG_Settings.c:243`, `scripts/3_Game/LFPG_Settings.c:518`); no basta asumir que llamar a Get en cliente replica la política del servidor.
- **Solapamiento:** horno con D15 y D22; inspector con la manifestación cliente de D24.

### D15 — VIVA — Validación de persistencia demasiado amplia o ausente

- **Evidencia de versión:** `scripts/4_World/lfpg_devicebase.c:71` acepta 1–999 y deriva al lector específico en la línea 424. `scripts/4_World/LFPG_Intercom.c:409` lee su formato sin comprobar `ver`. El mando lee `persistVer` en `scripts/4_World/LFPG_RemoteController.c:621`, nunca lo contrasta y usa `count` como límite de iteración en la línea 642 sin rechazar valores negativos ni acotar el volumen total leído.
- **Evidencia de valores:** `scripts/4_World/LFPG_MotionSensor.c:343` acepta cualquier modo; cuando existe LBmaster_Groups no aplica siquiera el fallback de la línea 353. Un modo fuera de ALL/TEAM/ENEMY termina permitiendo todos en `scripts/4_World/LFPG_MotionSensor.c:626`. Intercom lee frecuencia sin rango en `scripts/4_World/LFPG_Intercom.c:432` y la utiliza en el radio en la línea 758.
- **Qué está mal:** leer correctamente un entero o una cadena no demuestra que pertenezca al contrato guardado; versiones desconocidas y valores fuera de dominio pueden llegar a lógica de juego. Se confirma la aceptación en fuente, no una corrupción real de saves normales ni un ataque remoto.
- **Correcciones parciales de hoy:** el horno ya rechaza versiones fuera de 1–3 y duración de combustión inválida en `scripts/4_World/LFPG_Furnace.c:458` y `scripts/4_World/LFPG_Furnace.c:484` (commit `25e4ea8`, 2026-09-08). El lector de batería sanea energía no finita en `scripts/4_World/LFPG_Battery.c:319`. Estas curas no cubren los lectores anteriores; el horno aún asigna `fuelCurrent` sin validar su rango en `scripts/4_World/LFPG_Furnace.c:492`.
- **Coste: AMPLIO.** La ficha agrupa varios contratos. Las versiones legacy realmente soportadas deben conservarse explícitamente; no propongo cambiar el formato ni descartar indiscriminadamente entidades.
- **Solapamiento:** mando con D08/D09/D23; sensor con D17; Intercom con D11/D12; horno con D14/D22. También comparte el camino base de persistencia con varias familias D18.

### D17 — VIVA — Emparejamiento de grupo mediante nombre

- **Evidencia:** `scripts/4_World/LFPG_ActionPairSensor.c:88` toma `grp.name` y lo entrega al sensor en la línea 99. Se persiste en `scripts/4_World/LFPG_MotionSensor.c:338` y se compara con el nombre actual del grupo del jugador en las líneas 606 y 611.
- **Qué está mal:** la identidad usada por TEAM/ENEMY es una copia del nombre; si el proveedor permite renombrarlo, el mismo grupo deja de coincidir sin volver a emparejar. Si admite nombres duplicados o reutilizados, otro grupo podría coincidir; esas posibilidades del proveedor no están demostradas por este repositorio.
- **Comprobación del llamador ausente:** la búsqueda global de `m_PairedGroupName` y `LFPG_SetPairedGroupName` solo muestra carga y acción de emparejar como escrituras; no aparece observador de renombrado ni migración a un ID. La integración está condicionada a `LBmaster_Groups`.
- **Coste: AMPLIO.** Acción, sensor y compatibilidad de lo persistido; primero verificar el identificador estable real que exponga el proveedor.
- **Solapamiento:** misma persistencia y filtro de sensor que D15.

### D18 — VIVA — Duplicación localizada entre familias

- **Evidencia:** `LFPG_SetPowered` repite actualización de potencia, dirty y logging en `scripts/4_World/LFPG_SwitchV2.c:136` y `scripts/4_World/LFPG_SwitchV2Remote.c:72`. La operación RF repite toggle y log en `scripts/4_World/LFPG_SwitchRemote.c:172` y `scripts/4_World/LFPG_SwitchV2Remote.c:134`; la resolución de puertos vuelve a duplicarse en las líneas 184 y 146. Hay otro par claro de helpers de sobrantes en `scripts/4_World/LFPG_ActionUpgradeSolarPanel.c:205` y `scripts/4_World/LFPG_ActionUpgradeWaterPump.c:246`.
- **Qué está mal:** los mismos comportamientos se mantienen por copias, con riesgo de correcciones divergentes. Es una oportunidad de mantenimiento confirmada, no daño al jugador demostrado; SwitchRemote además tiene semántica de pulso y cancelación propia (`scripts/4_World/LFPG_SwitchRemote.c:99`) que no debe borrarse al compartir código.
- **Coste: AMPLIO.** Consolidación entre ficheros y contratos; las bases existentes ya comparten mucho comportamiento, así que no se justifica una jerarquía universal.
- **Solapamiento:** comparte setters visuales de switches con D11 y las dos acciones de mejora con D07. Las bases de kit de D06 también repiten la secuencia de creación, aunque heredan clases vanilla diferentes.

### D19 — VIVA — JSON del generador legacy sin caché para wires no vacíos

- **Evidencia:** el generador sigue siendo clase de producción, `config.cpp:249`, con la carpeta World incluida en `config.cpp:223`. `scripts/4_World/LFPG_TestDevices.c:708` llama a `LFPG_WireHelper.GetJSON`; `scripts/3_Game/LFPG_WireHelper.c:404` llama siempre a SerializeJSON y, con wires, esta crea el blob, copia entradas y genera JSON en las líneas 302–312.
- **Qué está mal:** pedir repetidamente el mismo estado vuelve a serializarlo en el generador. El gestor llega a ese getter al construir snapshots (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:1902`); su posterior agrupación no deshace la serialización ya realizada. No significa que cada destinatario RPC provoque por separado un nuevo JSON.
- **Matices:** el array vacío sí utiliza `GetEmptyWiresJSON` en `scripts/3_Game/LFPG_WireHelper.c:296`. La caché por generación de `scripts/4_World/LFPG_WireOwnerBase.c:146` no alcanza al generador, que hereda de PowerGenerator (`scripts/4_World/LFPG_TestDevices.c:53`); la fachada usa su fallback en `scripts/4_World/LFPG_IDevice.c:594`.
- **Coste: ACOTADO.** Caché e invalidación en el fichero del generador, revisando todos sus mutadores; no requiere cambiar su herencia.
- **Solapamiento:** comparte fachada con D10/D21, pero no la función principal de ninguna otra ficha. Está relacionado con D18 como excepción de herencia, no como motivo para forzar una base común.

### D20 — DUDOSA — Logging normal ya corregido; logging por llamada en Debug

- **Lectura A: ruido/coste de logging con la configuración entregada.** Ya no existe: `scripts/3_Game/LFPG_Defines.c:400` fija nivel 1 y `scripts/4_World/LFPG_MemoryCell.c:194` impide incluso construir el mensaje por debajo de 2. `scripts/3_Game/LFPG_Util.c:17` lo clasifica como Debug. **Esta parte murió antes de hoy**, en `7194ebb` (2026-08-26), cuyo cambio leído eleva la guarda de 1 a 2 y sustituye Info por Debug; ese commit ya contiene nivel global 1.
- **Lectura B: deduplicar también cuando se activa Debug.** Sigue presente: `scripts/4_World/LFPG_MemoryCell.c:214` registra cada llamada a `LFPG_SetPowered`, aunque `stateChanged` sea falso. La comparación de estado de la línea 164 solo gobierna el callback/dirty; no gobierna el mensaje. Además, una llamada a SetPowered no equivale necesariamente a cada época global de propagación.
- **Decisión:** DUDOSA porque la descripción no dice qué política de diagnóstico reclama. No la cuento como ruido de producción vivo ni cierro la petición literal de log solo ante cambios.
- **Coste: TRIVIAL** si solo se quiere condicionar la guarda a cambio; **ninguno** para la lectura A. Antes de reducir Debug hay que decidir si interesa conservar entradas sin cambio para diagnóstico.
- **Solapamiento:** misma función `LFPG_SetPowered` que D09; mismo fichero que la optimización visual de D11.

### D21 — VIVA — Umbral final distinto del radio documentado

- **Evidencia:** `scripts/4_World/LFPG_IDevice.c:185` promete no devolver coincidencias fuera de `searchRadius`; la firma de la línea 187 usa 0,25 por defecto. La consulta espacial recibe ese radio en la línea 196, pero `bestDist` empieza en `searchRadius + 1.0` en la 199, y la aceptación 3D compara únicamente contra ese valor en la 218.
- **Qué está mal:** el filtro explícito puede aceptar un candidato a menos de 1,25 m con el valor por defecto, sin comprobar el límite prometido de 0,25 m. **Condición pendiente:** que la consulta de motor devuelva un objeto cuya posición/origen esté fuera de ese límite; si siempre filtrara exactamente por la misma distancia 3D, la diferencia quedaría latente. No doy por cierta la descripción «2D» del comentario del mod ni afirmo que se esté eligiendo ya otra planta.
- **Coste: TRIVIAL.** Alinear el umbral inicial con el radio del contrato es una línea; comprobar la semántica espacial requiere motor.
- **Solapamiento:** mismo fichero que D10; el resultado se registra automáticamente en `scripts/4_World/LFPG_IDevice.c:228`, por lo que una coincidencia incorrecta podría afectar posteriores resoluciones.

### D22 — VIVA — Retorno de la base que no protege el resto del EEInit

- **Evidencia:** la base detecta la proyección y retorna en `scripts/4_World/lfpg_devicebase.c:248`. El horno llama a `super.EEInit()` en `scripts/4_World/LFPG_Furnace.c:129` y después crea la fuente térmica bajo SERVER y `FurnaceHeatEnabled` en la línea 147, sin consultar `m_LFPG_IsHologramProjection`.
- **Ruta de entrada localizada:** `scripts/4_World/LFPG_Furnace.c:21` usa la base de kit que activa el flag en `scripts/4_World/LFPG_HologramMod.c:175`; el classname proyectado es LFPG_Furnace en `scripts/4_World/LFPG_Furnace.c:25`. El retorno de `PlaceEntity` en `scripts/4_World/LFPG_HologramMod.c:205` no protege el cuerpo posterior de EEInit.
- **Qué está mal:** el guard de inicialización no cubre el código añadido por la subclase tras super; si esa proyección se inicializa en servidor con calor habilitado, asigna la fuente térmica igualmente. No se afirma calor activo gratuito, fuga permanente ni impacto en cliente: ese bloque es servidor y el destructor desactiva la fuente (`scripts/4_World/LFPG_Furnace.c:373`).
- **Coste: ACOTADO.** Proteger el EEInit del horno en ese fichero. También existe un patrón post-super en `scripts/4_World/LFPG_ElectricStove.c:584`, pero su kit hereda de la base normal, así que no lo presento como prueba de la misma ruta de proyección marcada.
- **Solapamiento:** horno con D14/D15; base y ruta de kits con D06. Las comprobaciones del flag ya existentes en EEDelete no detienen esta inicialización anterior.

### D23 — VIVA — Selección cuadrática en cada guardado del mando

- **Evidencia:** `scripts/4_World/LFPG_RemoteController.c:582` ordena en cada OnStoreSave. `LFPG_OrderPairsByLastSeen` extrae un mínimo por iteración en la línea 219 y `LFPG_FindOldestPairIndex` recorre las entradas restantes en la 202.
- **Qué está mal:** hay n + (n−1) + … + 1 inspecciones aun sin cambios de orden. Está acotado por `LFPG_REMOTE_PAIR_CAP = 32` en `scripts/3_Game/LFPG_Defines.c:259`: como máximo 528 visitas del buscador para una lista normal llena, además de la poda y escritura; no es crecimiento ilimitado ni un parón demostrado.
- **Coste: ACOTADO.** Un fichero; mantener orden o evitar ordenar si no cambió exige preservar la política de expulsión y el orden persistido.
- **Solapamiento:** mismo mando que D08/D09; comparte guardado/carga con D15. El cap limita la lista retenida durante load, no valida el `count` del stream denunciado en D15.

### D24 — VIVA — Energía sincronizada que el getter cliente no lee

- **Evidencia:** `scripts/4_World/LFPG_BatteryAdapter.c:113` registra energía y tasa; el servidor actualiza energía en la línea 451. `LFPG_GetStoredEnergy` retorna cero si falta `m_AttachedBattery` en la línea 477 y después lee CompEM, nunca `m_StoredEnergyX10`. Las asignaciones de esa referencia en las líneas 283, 404 y 417 están bajo SERVER; el máximo almacenado depende de ella en la 540.
- **Qué está mal:** el getter expuesto al cliente devuelve cero aunque la SyncVar de energía contenga un valor positivo. La capacidad también depende de `m_BatteryType`, solo poblado en servidor, en `scripts/4_World/LFPG_BatteryAdapter.c:167`; el inspector sí consume esa capacidad cliente en `scripts/4_World/LFPG_DeviceInspector.c:924`, a través de `scripts/4_World/LFPG_IDevice.c:740`.
- **Límites a la descripción:** la tasa **sí** utiliza su SyncVar en `scripts/4_World/LFPG_BatteryAdapter.c:632`. La fila detallada de energía del inspector solo hace cast a BatteryBase (`scripts/4_World/LFPG_DeviceInspector.c:1191`), no al adaptador: no afirmo que exista allí una barra del adaptador mostrando cero. La discrepancia de API y el throughput cliente cero sí están localizados; el consumo/almacenamiento servidor usa referencias válidas y no se demuestra pérdida de energía.
- **Coste: ACOTADO.** Corregir la lectura cliente y derivar tipo/máximo del estado disponible en el propio adaptador; añadir una fila específica al inspector sería otro alcance.
- **Solapamiento:** mismo inspector que D14 y misma fachada que D10/D21; su kit usa las bases afectadas por D06. D05 de hoy le dio desmontaje, sin resolver este contrato cliente.

## LAS QUE RECOMIENDO ATACAR PRIMERO

1. **D06.** La sustitución repetible sin conservar salud afecta al valor del daño y de las reparaciones en todos los dispositivos desplegables alcanzados. Verificar con daño real el ciclo dispositivo → kit → dispositivo al corregirlo.
2. **D07.** Una mejora exitosa destruye el estado de materiales y filtro; el subtipo del filtro puede perderse de forma irreversible. Mantener la protección actual frente a fallos de creación mientras se conserva estado.
3. **D08.** La interfaz puede ofrecer emparejar y ejecutar lo contrario tras pasar el mando a otro jugador; afecta directamente al control de los dispositivos ya configurados.
4. **D14.** En servidores con whitelist, la estimación puede prometer autonomía inexistente. Un jugador puede dejar una red desatendida esperando combustible que no cuenta.
5. **D24.** El adaptador comunica capacidad cero al consumidor cliente localizado y su API de energía contradice lo sincronizado, dificultando diagnosticar una red que sí funciona en servidor.

D17 merece verificar pronto el contrato de renombrado/identidad del proveedor; si protege accesos mediante automatismos, su daño potencial puede superar a D14/D24. D15 debe cerrarse por contrato antes de introducir nuevas versiones de persistencia. No antepongo las optimizaciones sin medición a fallos funcionales ya trazados.

## LO QUE NO PUDE VERIFICAR

- Ejecución en DayZ, carga de mundo, entrega efectiva de RPC y SyncVars, comportamiento visual y restauración real: este entorno solo permite conclusiones estáticas.
- D06/D07: salud inicial exacta de cada classname, estados añadidos por terceros y aceptación de todas las variantes dañadas/arruinadas por el motor; sí se verificó que el mod no los transfiere.
- D08/D09: timings reales al transferir o borrar el mando y tratamiento nativo de callbacks cuyo receptor se destruye; no se declara crash.
- D10/D11/D12/D13/D19/D23: milisegundos, asignaciones internas del motor y umbral perceptible con población real; las lecturas demuestran operaciones, no severidad medida.
- D15: compatibilidad histórica exhaustiva y corpus de saves malformados; la revisión demuestra casos concretos de aceptación, no cubre todas las clases persistentes del mod.
- D17: implementación y política efectiva de LBmaster_Groups para renombrado, unicidad/reutilización de nombres y disponibilidad de ID estable; esa dependencia no está implementada en el árbol inspeccionado.
- D20: intención de la auditoría entre eliminar ruido en la build habitual y registrar solo cambios también en Debug; las dos lecturas están separadas y fechadas.
- D21: semántica nativa de GetObjectsAtPosition respecto de altura, bounding box y origen; el umbral final incorrecto sí está leído.
- D22: creación de la proyección del horno en el lado servidor durante el flujo real de colocación y coste de su fuente térmica; no se verificó emisión de calor.
- No hay `CLAUDE.md` en esta copia del proyecto (búsqueda con `rg --files --hidden` y `git ls-files`); no se importó un plan ajeno para sustituir el brief vigente.

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

- **«18 fichas» no significa «18 bugs independientes».** D18 es una oportunidad de mantenimiento; D10/D11/D12/D13/D19/D23 son principalmente coste. Su presencia estática no acredita por sí sola prioridad P2 ni daño al jugador. D10 ya está cruzada con G17 en `reviews/2026-09-06-council-auditorias/BRIEF.md:411`; evitar contabilizar y arreglar dos veces el mismo trabajo en lanes distintas.
- **D20 no es un problema incondicional actual.** La ruta habitual quedó corregida el 2026-08-26, antes de las integraciones de hoy. Convertir «el log sigue escrito en el fichero» en VIVA habría repetido precisamente el error de método del encargo. Queda DUDOSA por el alcance de Debug, no por desconocimiento de su ubicación.
- **Hay fichas demasiado anchas para un solo tramo.** D15 mezcla versiones, enums, contadores y formatos de distintas familias. D11 no debe reabrir MemoryCell, que ya tiene caché. D07 debe coordinarse con los cambios de integridad de hoy, no reemplazarlos por la antigua ruta destructiva.
- **Las afirmaciones universales necesitan recorte.** D19 tiene excepción para wires vacíos y no serializa necesariamente por destinatario. D24 no afecta a todos los getters: la tasa sí usa SyncVar. D13 no consulta jugadores en toda llamada de todo controlador, sino en el caso sin pareja con backoff ampliado. D23 está limitado a 32 parejas normales.
- **Prioridad condicionada por daño real.** D17 podría merecer P1 si un nombre reutilizable permite eludir automatismos de acceso; no lo elevo sin verificar el proveedor. D21 es una discrepancia contractual latente hasta probar qué candidatos entrega el motor. D22 conserva una brecha de inicialización, pero esta lectura no prueba fuga ni calor gratuito; mantener P3 resulta más proporcionado que convertirla en fallo funcional grave.
- **Las muertes parciales no cierran la ficha completa.** El horno valida su versión/duración desde hoy y la batería sanea valores no finitos; otros lectores mantienen D15. GetAll optimizado sigue dejando D12 global. No se ha encontrado una ficha completa inequívocamente MUERTA en este lote; no se fuerza ese resultado por la cantidad de cambios recientes.
- **Alcance de esta copia.** Los veredictos corresponden al HEAD indicado, no a commits que otras lanes integren después. La memoria de proyecto consultada contenía objetivos anteriores de footprint; se tomó el brief actual y el código vivo como encargo, sin retomar esos trabajos.

## Comprobaciones y cierre

Se completó la tabla de 18 IDs y la lectura de cada mecanismo y de los consumidores relevantes; las búsquedas globales de llamadores se ampliaron en D08/D09/D17. Se contrastaron los cambios parciales de hoy y el cambio histórico de logging por contenido. La validación del informe comprueba cobertura de IDs, secciones, categorías de coste, existencia de rutas y límites de línea; no sustituye a pruebas del producto.

Único producto escrito por esta lane: `TRIAJE.md`. No se modificó código ni se ejecutaron `git add`, `git commit`, `git checkout`, `git stash` o `git reset`. No se escribió memoria durable ni handoff fuera del informe porque el brief limita expresamente las escrituras a este archivo; este documento conserva el estado y la evidencia para la siguiente lane.
