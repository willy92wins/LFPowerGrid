<!-- Salida literal de Codex (codex exec, solo lectura), revision del commit fa0835c6. Transcrita por el receptor sin editar. -->

BASE: claude/project-thread-0iolln @ fa0835c6

**Veredicto: SOUND-with-fixes.**

La lógica escalar del Toggle, el autoconsumo y el aviso siguen el brief. Encontré tres límites funcionales: pérdida de pulsos por coalescencia del grafo y dos falsos negativos del láser. No encontré un P1 demostrado.

Revisión independiente, solo lectura, sobre `fa0835c6345c5410ff5168016eb96bb414274a0f`, cuyo padre es `9a4bafdd82505a15ba50f5bc46c7179595a406e7`. No modifiqué ficheros, abrí subagentes ni arranqué DayZ.

## Matriz de hallazgos

Las rutas y líneas corresponden al commit revisado.

| ID | Severidad | path:line | Entrada o secuencia exacta | Salida mala | Arreglo mínimo |
|---|---|---|---|---|---|
| N131-01 | P2 | `scripts/4_World/LFPG_MemoryCell.c:196`; `scripts/5_Mission/LFPG_ElecGraphImpl.c:2117` | Power estable; Toggle previamente alto; su proveedor queda detrás de 1600 nodos pendientes. Apagarlo y encenderlo 1250 ms después, antes de procesarlo. | El estado pendiente se sobrescribe; la celda nunca observa Toggle bajo y pierde la siguiente inversión. | Preservar transiciones antes de coalescer, o acotar explícitamente el contrato a flancos observados con una latencia máxima demostrada. Requiere revisar el alcance del brief. |
| N131-02 | P2 | `scripts/4_World/LFPG_LaserDetector.c:371`, `:453` | Un raycast deja longitud 1 m al tocar un vehículo. Este avanza alejándose del emisor a 0,25 m/s, todavía intersectando el alcance de 5 m; láser y alimentación permanecen estables. | Caduca la retención de 3 s y se cierra el gate antes del siguiente raycast, a los 6,9 s. | Invalidar/recalcular la oclusión cuando se mueva un vehículo candidato, antes de evaluar su caja. |
| N131-03 | P2 | `scripts/4_World/LFPG_LaserDetector.c:439` | Vehículo ocupado cuya chapa corta el haz a 0,05 m del emisor. | Se retorna antes del bucle de jugadores y de la prueba de caja; además se cancela inmediatamente la retención existente. | Permitir comprobar vehículos con un raycast válido aunque el segmento mida menos de 0,1 m. |
| N131-04 | P3 | `.github/tools/test_memorycell_toggle.py:57`; `.github/tools/test_wire_replace_notice.py:21` | Mutantes en memoria: eliminar `m_ToggleLastHigh = false;`, eliminar el envío del aviso o fijar su cuenta a cero. | Los módulos de tests afectados siguen verdes pese a desactivar el comportamiento principal. | Añadir pruebas de secuencias de estado y de efectos observables: inversiones, mensajes y número de llamadas. |
| N131-05 | P3 | `scripts/4_World/LFPG_MemoryCell.c:27`; `scripts/4_World/LFPG_LaserDetector.c:23` | Código nuevo indentado con espacios. | Incumplimiento literal de `AGENTS.md:86`, que exige tabs. | Ajustar exclusivamente la indentación nueva, conservando los finales de línea de cada fichero. |

## Detalle y evidencia

### N131-01 — El grafo entrega estados, no un historial de flancos

**Hechos:**

- `RequestPropagate` refresca inmediatamente el estado del nodo y después lo marca: `LFPG_NetworkManagerImpl.c:2426`.
- Para un SOURCE, `RefreshSourceState` sobrescribe `node.m_Powered`: `LFPG_ElecGraphImpl.c:3744`.
- `MarkNodeDirty` mantiene una sola entrada si `m_InQueue` ya es true: `:2117`.
- El procesamiento tiene presupuesto por llamada: `:2250`. El presupuesto normal es 64 nodos y se duplica para colas grandes: `LFPG_NetworkManagerImpl.c:2440`, `:2449`.
- La propagación se programa cada 100 ms: `LFPG_Defines.c:515`.
- La celda solo considera subida cuando su última lectura fue baja: `LFPG_MemoryCell.c:196`.

**Secuencia concreta derivada del código:**

1. Celda estabilizada, Power continuamente alto y Toggle alto.
2. Hay 1600 nodos distintos pendientes delante del proveedor de Toggle.
3. En `t=0`, el proveedor se apaga y queda encolado.
4. En doce evaluaciones pueden procesarse como máximo `12 × 128 = 1536` nodos.
5. En `t=1250 ms`, antes de la evaluación 13, el proveedor vuelve a encenderse. Su estado pendiente pasa otra vez a true y no se añade una segunda entrada.
6. Cuando se procesa, puede conservar exactamente la asignación anterior. La celda no recibió la bajada; `m_ToggleLastHigh` sigue true.

Los límites permiten ese tamaño global: `LFPG_Defines.c:488` admite 2048 nodos, repartidos entre componentes.

**Interpretación:** esto limita la garantía de capturar cada pulsación física válida. En este ejemplo ni siquiera llega a publicarse una bajada en `m_AllocatedPower`. Es una incompatibilidad entre esa garantía y la coalescencia existente, no un error del predicado temporal. El escenario de carga no se ejecutó en DayZ.

### N131-02 — La premisa «el haz termina en la chapa» caduca entre raycasts

**Hechos:**

- La longitud se almacena después del raycast: `LFPG_LaserDetector.c:410`.
- El siguiente mantenimiento se fija a 6900 ms: `:183`, `:371`.
- El movimiento del vehículo no cambia la transformación del láser comprobada en `:325`.
- Una llamada repetida con la misma alimentación retorna sin actualizar el raycast: `:224`.
- La intersección utiliza la longitud almacenada: `:453`.
- La retención dura 3 s: `:544`.

**Reproducción offline:** ejecuté el helper de intersección y el bloque de retención extraídos del fuente, con la cadencia real de detección de 300 ms.

| Tiempo | Caja del vehículo sobre el eje del haz | Longitud almacenada | Gate |
|---|---|---:|---|
| 0 s | `[1, 5]` m | 1 m | Abierto |
| 3,3 s | `[1,825, 5,825]` m | 1 m | **Cerrado** |
| 6,9 s | `[2,725, 6,725]` m | 2,725 m, tras actualizar | Abierto |

En los tres puntos, el segmento completo de 5 m intersecta la caja.

**Interpretación:** el nuevo detector puede cerrar una puerta mientras el vehículo sigue atravesando su alcance. El intervalo de mantenimiento es preexistente; su interacción con el nuevo comportamiento queda sin resolver.

### N131-03 — El filtro anterior anula casos que el helper acepta

**Hecho:** `LFPG_EvaluateCrossing` retorna con `m_BeamLength < 0.1` antes de examinar jugadores o vehículos. Si el gate estaba abierto, lo cierra y borra su retención: `LFPG_LaserDetector.c:439`.

Con una chapa a 0,05 m, la caja inflada puede intersectar tanto el origen como el extremo. El helper devuelve true para ese segmento; la integración nunca lo llama.

**Interpretación:** el filtro también es preexistente, pero contradice la nueva regla general de detección por caja y explica un falso negativo concreto cerca del emisor.

### N131-04 — Los tests no cubren el cableado del comportamiento

Ejecuté los módulos completos sobre mutaciones exclusivamente en memoria:

| Mutación | Resultado |
|---|---|
| Eliminar `m_ToggleLastHigh = false;` | Los 3 tests de Memory Cell pasan |
| Eliminar `LFPG_SendClientMsg(player, replaceMsg);` | Los 2 tests del aviso pasan |
| Sustituir la acumulación de sustituciones por `replacedCount = 0;` | Los 2 tests del aviso pasan |

Esto demuestra un punto ciego de cobertura. Los mutantes no se escribieron en el repositorio.

## Respuesta a A–G

### A. Toggle y secuencias reales de llamadas

**Comprobado y correcto:**

La ruta normal es:

`ProcessDirtyQueue` → `SyncNodeToEntity` → `LFPG_DeviceAPI.SetPowered` → `LFPG_MemoryCell.LFPG_SetPowered`.

Referencias: `LFPG_ElecGraphImpl.c:2884`, `:3207`; `scripts/4_World/LFPG_IDevice.c:315`.

- `DIRTY_INPUT` evita que el filtro suprima la sincronización: `LFPG_ElecGraphImpl.c:2985`.
- La celda no retorna cuando `m_PoweredNet` permanece igual: solo condiciona su asignación, y continúa leyendo los puertos en `LFPG_MemoryCell.c:155`.
- Cambiar asignaciones marca los destinos con `DIRTY_INPUT`: `LFPG_ElecGraphImpl.c:4790`, `:2815`, `:2821`.
- Por tanto, una bajada publicada y procesada normalmente sí llega, aunque otra entrada mantenga alimentada la celda.

Ejercité las asignaciones del bloque de estado extraído del fuente: dos subidas válidas, nivel alto mantenido, apagón de 300 ms, corte largo de Power+Toggle, gracia tras carga, primera lectura alta y prioridad de Set. Sus resultados coinciden con R1–R5.

**Límite:** no existe una garantía de una llamada por cada transición física o intermedia; véase N131-01. Los presupuestos y la coalescencia importan.

**Orden de lectura:** los puertos leen las asignaciones más recientemente escritas. La evaluación de la celda no recalcula sus edges de entrada. Puede leer asignaciones de proveedores ya procesados en esta evaluación junto con otras todavía anteriores: `LFPG_ElecGraphImpl.c:2301`, `:2525`, `:2884`, `:5044`. No hay una instantánea global de época.

### B. Autoconsumo y reparto entre entradas

**La demanda no se queda sistemáticamente en Power ni en la primera entrada.**

`ApplyMergerWaterFill` trata las entradas del PASSTHROUGH como proveedores de una demanda compartida: `LFPG_ElecGraphImpl.c:4390`, `:4489`, `:4526`.

Con demanda total 1 y oferta suficiente:

| Entradas activas | Asignación por entrada |
|---:|---:|
| 2 | 0,5 u/s |
| 3 | 0,333… u/s |
| 4 | 0,25 u/s |

Todas superan `LFPG_PROPAGATION_EPSILON = 0.001`, usado por `IsPortReceivingPower`.

También comprobé el modelo existente con dos salidas del mismo proveedor hacia Power y Toggle: ambas terminaron en 0,5 u/s. Esto respalda el arreglo S2a en régimen estable; no demuestra todos los transitorios del grafo.

Una consecuencia del autoconsumo: con entrada total limitada a 100, quedan como máximo 99 u/s para salidas. El grafo resta consumo y limita también la demanda: `LFPG_ElecGraphImpl.c:2394`, `:2590`. Conviene interpretar la capacidad anunciada con esa semántica.

### C. Láser, geometría y candidatos

**La función de losas está bien para los casos examinados.**

- Intercambia correctamente límites cuando la dirección es negativa.
- Mantiene `tMin/tMax` dentro del segmento inicial `[0,1]`.
- Rechaza intersecciones de la recta situadas fuera del segmento.
- Resuelve ejes casi constantes y segmentos degenerados dentro/fuera de la caja.

Además de sus tests, contrasté 25 000 casos deterministas contra un oráculo independiente basado en intersecciones con las caras de la caja, sin discrepancias.

`P:\scripts` no estaba disponible. Verifiqué fuentes locales en:

`E:/DayZ-Stable-Extract/1.30.164200/P-scripts/`

Allí:

- `3_game/entities/object.c:358`: `ClippingInfo(out vector minMax[2])`.
- `:370`: `GetCollisionBox(out vector minMax[2])`.
- `:874`: `WorldToModel(vector worldPos)`.
- `4_world/entities/manbase/playerbase.c:4735` usa una caja local y la transforma con `GetTransform`, apoyando el marco de modelo empleado.
- `4_world/classes/constructionbase.c:1627` contiene doble indexación y aritmética dentro de argumentos. No encontré fundamento para declarar inválida la expresión nueva.

No aparece una corrección adicional de centro de masas necesaria en esos usos. La escala nativa y el margen físico en vehículos escalados quedan pendientes de comprobación.

`Car` y `Boat` heredan de `Transport`; también lo hace `Motorbike`. El cast cubre esas familias. Vehículos personalizados con otro padre o una jerarquía intermedia no quedan cubiertos por ese cast directo.

**Candidatos:** el alcance real es 5 m y el margen añadido 8 m: consulta de 13 m. La búsqueda utiliza celdas X/Z y no aplica después otro filtro de distancia euclídea antes de `LFPG_EvaluateCrossing`: `LFPG_NetworkManagerImpl.c:5812`, `:5837`, `:5875`, `:6027`.

`laserDormant` solo cuenta el caso sin candidatos; la evaluación se llama igualmente en `:6031`. Los muertos se excluyen. No encontré otro bloqueo por dormancia.

Persisten N131-02 y N131-03.

### D. Aviso y ajuste

**La colocación y el conteo son correctos por lectura del flujo:**

- El reroute retorna en `LFPG_RPCServerHandlerImpl.c:573`, antes del aviso.
- La recogida añade filas conflictivas una vez por fila: `:741`, `:757`.
- Se suman todos los owners en `:645`.
- La fila nueva todavía no está en `m_RemovedWires`: se inserta después, en `:672`.
- El aviso tiene una sola llamada, con los textos exigidos, protegida por el ajuste y por null: `:650`.
- No añade retornos ni cambia el orden de `EndGraphMutation` y `UnlockPort`: `:663`.

**JSON antiguo sin la clave:** `LFPG_Settings.Load` construye primero un objeto con `NotifyWireReplaced = true` y lo pasa a `LoadFile`: `LFPG_Settings.c:117`, `:570`, `:584`. El wrapper vanilla deserializa sobre ese objeto mediante `ReadFromString`, sin crear otro explícitamente. [Fuente oficial de Bohemia](https://github.com/BohemiaInteractive/DayZ-Script-Diff-Experimental/blob/main/scripts/3_game/tools/jsonfileloader.c).

**Interpretación:** debería conservar true si falta la clave. La conducta nativa de campos ausentes no está implementada en estos fuentes; no la doy por medida. Tampoco se guarda automáticamente la clave nueva cuando un JSON antiguo carga correctamente: `LFPG_Settings.c:595`, `:640`, conforme al brief.

### E. AGENTS.md y compilación

- Los miembros nuevos están declarados. Comprobé también `finish.m_Owners`, `ownerState.m_RemovedWires`, el ajuste y el `m_DeviceId` heredado.
- Las constantes nuevas están en `3_Game`; los helpers y sus llamadores están en `4_World`.
- La interfaz de `IsPortReceivingPower` existe en `scripts/4_World/LFPG_NetworkManager.c:258`; la celda no referencia directamente la implementación de Mission.
- Hay precedente vanilla de función global `static`: `1_core/proto/proto.c:362`, `RGBToHSV`, comprobada fuera de cualquier clase. No lo considero un bloqueo de sintaxis.
- No encontré locales nuevos repetidos dentro del mismo método ni operadores prohibidos, `Print` o `ref` locales añadidos.
- El diff conserva los EOL: NetworkManager sigue CRLF en ambos blobs; los otros scripts conservan LF en Git y el CSV LF. No hay normalización masiva.
- Sí existe el incumplimiento de tabs de N131-05.

### F. Calidad de los tests

Los tests escalares tienen expectativas positivas y negativas externas; no son tautológicos por cargar el código de producción. El test del CSV también comprueba un contrato literal apropiado.

El test del aviso es principalmente textual. Su mutante de guarda demuestra que desapareció la cadena buscada, pero no que el ajuste controle los mensajes durante la ejecución. N131-04 muestra fallos funcionales grandes que siguen pasando.

Los tests nuevos no ejercitan:

- Entrega/coalescencia de callbacks del grafo.
- Actualización real de los cuatro estados temporales.
- Caché del raycast y filtros anteriores al helper.
- Deserialización nativa de ajustes antiguos.
- Compilación de World/Mission.

### G. Premisas que necesitan corregirse o acotarse

1. Un detector de flancos correcto no convierte una cola de estados coalescidos en un transporte fiable de pulsos.
2. «El extremo está en la chapa» solo es cierto inmediatamente después de un raycast que haya tocado ese vehículo.
3. Aprobar el helper de segmentos degenerados no cubre el retorno anterior por longitud mínima.
4. El autoconsumo cambia el presupuesto útil de salida; no solo reactiva la lectura de controles.
5. La atribución del problema de Nostra sigue siendo una explicación plausible por código, sin reproducción aportada en este encargo.
6. AND/OR/XOR y Door Controller no cambian funcionalmente en este commit; no queda demostrado que todos los síntomas del reporte original estén resueltos.

## Validación realizada

| Comprobación | Resultado |
|---|---|
| Linter, base `9a4bafd` | 267 ficheros; 0 errores; 48 warnings; exit 2 |
| Linter, `fa0835c6` | 267 ficheros; 0 errores; 48 warnings; exit 2 |
| Delta de errores | **0** |
| `ui_reconcile.py` | **0 FAIL / 0 WARN** |
| 13 módulos de tests compatibles con solo lectura | **129 tests, todos verdes** |
| Cuatro módulos nuevos | **8 tests, todos verdes** |
| `enforce_checks.py --root .` | **FAIL=0 / WARN=0** |
| Cuatro filas de capacidades del CSV | Solo cambia `20 → 100` en las siete columnas de texto |
| Comparación de numstat con/sin ignorar CR | Coincide |
| Estado final | HEAD congelado y árbol limpio |

La base del linter se evaluó mediante una superposición de lecturas en memoria de los seis scripts modificados, sin crear worktree ni escribir ficheros. No hay altas o bajas de entradas del validador.

## LO QUE NO PUDE VERIFICAR

- Compilación Enforce y funcionamiento in-game: no hay compilador invocable y estaba prohibido arrancar DayZ.
- Reproducción nativa del escenario de carga de N131-01. Su mecanismo está trazado en el código.
- Cajas, raycasts, ocupación y transformaciones reales de coches/barcos concretos, especialmente con escala personalizada.
- Semántica nativa definitiva de una clave JSON ausente.
- Tres comprobaciones del linter que se omitieron automáticamente al no encontrar `P:\scripts`: referencias a clases vanilla, nombres de variables frente a tipos vanilla y métodos builtin inexistentes. Las firmas nuevas se contrastaron manualmente con la extracción local indicada.
- `test_enforce_checks.py`: crea fixtures temporales y lo omití para preservar la restricción de solo lectura; ejecuté directamente el checker sobre el árbol.
- Los casos de `PRUEBA-EN-PARTIDA.md` y el artefacto PBO desplegado. Este dictamen corresponde al fuente congelado.