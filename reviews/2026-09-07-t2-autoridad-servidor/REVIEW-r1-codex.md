## VEREDICTO

RECHAZAR: el parche introduce dos regresiones reproducibles en su lógica y deja abierto un fallo de consistencia de SEC20 que puede eliminar una conexión válida y publicar después la rechazada.

Revisión estática adversarial del 2026-09-07. Tres hallazgos sustentados por caminos de código y modelos ejecutados; una sospecha adicional sobre CCTV que **no determina el veredicto**. No he compilado ni ejecutado DayZ.

Árbol revisado: rama `fix/t1-integridad-monetaria`, HEAD `8cb622244d0fcb073e5b7aa4d9d8172dfbd2ef29`. El diff de los dos ficheros coincide con `_review-t2/DIFF.patch`: 366 inserciones y 36 eliminaciones. También he leído `DECISIONES-PRODUCTO.md`: la variante restrictiva de SEC02 está decidida por el dueño y se evalúa como requisito.

## HALLAZGOS

### [GRAVE] F-01 — El rechazo del grafo conserva el cable nuevo, pierde el anterior y permite difundir después el cable rechazado

- **Donde:** `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:770`, `:791` y `:850-861`; `scripts/5_Mission/LFPG_NetworkManagerImpl.c:904-909` y `:2644-2649`.
- **Que pasa:** el reemplazo elimina el incoming anterior y guarda el nuevo antes de intentar insertarlo en el grafo. La rama nueva de rechazo hace rebuild y return, pero no revierte ninguna de esas escrituras. El rebuild vuelve a aplicar los límites que causaron el rechazo y no limpia el cable rechazado del almacén. La siguiente sincronización vanilla serializa ese almacén íntegro, sin consultar si cada cable existe en el grafo. Por tanto, «no enviar el broadcast de este add» no garantiza «no difundir el fantasma». Es un **defecto residual de SEC20/SEC03**, no una pérdida de datos introducida por el cambio de destinatario. Su consecuencia es pérdida de conexión y divergencia persistible entre almacén y grafo.
- **Escenario de fallo:** estado de actualización desde la versión anterior: un PowerGenerator vanilla G conserva 12 cables, con puertos de origen no vacíos `legacy_0` … `legacy_11`, hacia 12 consumidores existentes y registrados. Todos son de A. Otro origen H alimenta D por un cable de A. Configuración por defecto, `MaxWiresPerDevice=64`; geometría válida, cuota personal disponible, sin ciclos, componentes pequeños. A solicita el cable legítimo `G.output_1 -> D.input_main`.
  1. SEC20 acepta los nombres del cable **nuevo**. El preflight cuenta 12 cables y deja pasar: ninguno ocupa `output_1` y 12 < 64.
  2. Se borra `H -> D` y se guarda el decimotercer cable de G.
  3. El grafo lo rechaza porque G ya tiene 12 aristas salientes.
  4. El rebuild vuelve a admitir los primeros 12 y rechaza otra vez el decimotercero. H ya no tiene su conexión.
  5. Un full sync posterior de un jugador junto a G envía sus 13 filas, incluida la rechazada. El return de SEC20 no protege este camino.
- **Por qué el estado inicial es admisible para esta revisión:** precisamente SEC20 corregía la aceptación de nombres arbitrarios por el servidor anterior. El loader actual copia esos nombres (`LFPG_NetworkManagerImpl.c:4655-4672`); `LFPG_WireHelper.ValidateWireData` verifica campos obligatorios y geometría, pero no el vocabulario de puertos (`scripts/3_Game/LFPG_WireHelper.c:35-83`). La poda vanilla elimina endpoints irresolubles, no estos nombres (`LFPG_NetworkManagerImpl.c:4169-4253`). No presupongo que el servidor del usuario tenga esas filas; es un fixture de actualización que el código permite conservar. No requiere que un splitter estándar tenga 13 salidas.
- **Mecanismo comprobado:** el límite saliente está en `scripts/5_Mission/LFPG_ElecGraphImpl.c:1343-1351`; el rebuild recorre las filas vanilla en su orden y descarta el resultado booleano de la inserción en `:266-287`. El nuevo cable queda insertado e indexado, con persistencia marcada como pendiente, en `LFPG_NetworkManagerImpl.c:904-909`. El full sync llama a `SendVanillaWiresTo` en `:2827-2835` y este incluye todas las filas en `:2644-2649`. El log `SUCCESS` de `LFPG_RPCServerHandlerImpl.c:833` también precede a la admisión del grafo.
- **Que haria:** definir el reemplazo como una operación cuyo éxito exige admisión tanto del almacén como del grafo. Calcular los conflictos completos y conservar lo necesario para restaurar cables, índices y estado del grafo si falla; publicar el resultado solo tras esa decisión. La operación pertenece al componente que controla esas mutaciones, no a una réplica parcial de sus reglas en el handler. Mantendría el formato persistente existente. Los datos anteriores requieren una política explícita de validación o reparación; no los borraría silenciosamente al actualizar.
- **Prueba y control:** el modelo ejecutado produce almacén=13, grafo=12, conexión anterior=0 y snapshot posterior con el cable rechazado. Con una plaza de grafo libre, admite el nuevo. La prueba en DayZ queda pendiente: si el fixture no llega con sus 12 aristas al punto de partida, es fallo de preparación y no un PASS del parche.

### [MEDIO] F-02 — El preflight rechaza un reemplazo válido porque no cuenta el hueco que libera el destino en el mismo origen

- **Donde:** `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:936-992`, especialmente `:968-977` y el rechazo temprano en `:651-656`.
- **Que pasa:** el recuento resta únicamente los cables del mismo puerto de salida. La fase real de reemplazo también elimina cables que llegan al puerto de destino, incluidos los que salen de **otro puerto del mismo origen**. El helper no recibe destino ni puerto de destino, por lo que no puede contar esa segunda liberación. Introduce un falso «device is full»; es degradación funcional, sin pérdida de datos en esta rama.
- **Escenario de fallo:** servidor con `MaxWiresPerDevice=1`, un splitter S y un spotlight D. A tiene `S.output_1 -> D.input_main` e intenta mover esa conexión a `S.output_2 -> D.input_main`. No hay cables ajenos ni otras restricciones activadas. El preflight deja `remaining=1` y rechaza. La ejecución real que precedía a T2 quitaba la conexión al mismo D y dejaba cero cables en S antes del add: el reemplazo cabía y el número final seguía siendo uno. `AllowCutOthersWires=false` no cambia el caso porque ambos cables son de A.
- **Mecanismo comprobado:** la configuración 1 es válida, no una configuración fuera de contrato: mínimo y clamp en `scripts/3_Game/LFPG_Settings.c:212-213` y `:314-325`. El splitter declara `output_1`, `output_2` y `output_3` en `scripts/4_World/LFPG_Splitter.c:51-60`. La segunda fase llama a `RemoveWiresTargeting` (`LFPG_RPCServerHandlerImpl.c:767-770`), que elimina la fila del array del owner en `LFPG_NetworkManagerImpl.c:1591-1623`. El límite real se evalúa sobre ese array ya reducido (`scripts/3_Game/LFPG_WireHelper.c:153-186`; entrada del owner en `scripts/4_World/LFPG_WireOwnerBase.c:175-203`).
- **Que haria:** calcular un único conjunto de cables que realmente se van a quitar por conflicto de origen **o** de destino, con la política de autor aplicada y sin contar dos veces una fila. Usar ese resultado en la admisión y en la mutación. Subir el límite o exigir que el jugador corte primero solo ocultaría la regresión.
- **Prueba y controles:** modelo ejecutado: preflight=REJECT, filas tras ambas sustituciones=0, almacén=ACCEPT. Con destino distinto debe seguir rechazando; al sustituir desde el mismo puerto de salida debe aceptar. Ambos controles pasan. No he encontrado, en el camino estándar leído, un falso «cabe» debido únicamente a los topes de almacén o al duplicado exacto; el defecto demostrado del helper es el falso rechazo.

### [MEDIO] F-03 — El unicast vanilla deja cables antiguos en clientes próximos al destino que acaba de desaparecer de la lista de interés

- **Donde:** `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2536`, `:2570-2585` y `:2616-2626`; llamada tras reemplazo en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:873`.
- **Que pasa:** el broadcast vanilla calcula interesados usando únicamente el owner y los destinos que **quedan después de la mutación**. Al quitar o redirigir un cable, un observador del destino antiguo puede quedar fuera. Antes, el envío con destinatario null provocado por otro jugador cercano le entregaba también la actualización. Ahora el unicast hace efectivo el filtro incompleto y ese cliente conserva su cable antiguo. Es una regresión de sincronización introducida por SEC01 al descubrir un defecto previo del conjunto de destinatarios; puede producir un fantasma visual.
- **Escenario de fallo:** sobre terreno llano, posiciones expresadas como desplazamientos locales en metros: generador G=0, destino viejo T=60, destino nuevo U=-10, actor A=0, observador B=72, con carrete equipado y mirando hacia T. A había tendido `G -> T` con waypoints válidos (60 m totales, tramos menores de 50 m; límites en `scripts/3_Game/LFPG_Defines.c:16` y `:535`); B recibió el cable porque estaba a 12 m de T. A lo sustituye por `G -> U`, operación legítima sobre su propio cable.
  1. El radio de sync es 70 m. B queda a 72 m de G y a 82 m de U: no recibe el nuevo blob.
  2. A sí está en rango. En el código anterior, ese envío a todos alcanzaba a B; con el parche solo llega a A.
  3. B sigue junto al extremo viejo y puede seguir viendo el cable que ya no existe. El caso también se obtiene cortando el único cable, cuando la lista de destinos queda vacía.
- **Mecanismo comprobado:** el renderer no equivale a «no dibuja si el jugador está a más de 50 m del owner». Su descarte por owner es a 75 m (`scripts/4_World/LFPG_CableRenderer.c:2385-2387` y `:2417-2441`); B=72 pasa. La visibilidad por cable usa esfera envolvente y burbuja de extremos (`:2535-2553`); B está dentro de los 25 m del extremo T. La actualización del estado requiere un blob nuevo (`:1260-1369`). Para vanilla conocido no hay generación anunciada que fuerce resincronización: snapshot con generación -1 (`LFPG_NetworkManagerImpl.c:2556`) y rama de dispositivo conocido en `LFPG_CableRenderer.c:1497-1512`. El cliente conserva el estado hasta una resincronización posterior válida.
- **Que haria:** conservar las posiciones de los destinos eliminados antes de mutar y dirigir la invalidación a la unión de los interesados anteriores y actuales. Cubrir también la cola y el diferimiento durante full sync: la cola vanilla actual conserva solo identidad de owner y objeto (`LFPG_NetworkManagerImpl.c:2521-2533`). El camino de delta nativo ya incorpora destinos de operaciones eliminadas (`:2435-2444`), referencia útil para el contrato. Mantendría los destinatarios explícitos.
- **Prueba y control:** modelo ejecutado: B pertenece a los interesados del cable anterior, queda fuera del nuevo unicast y pasa los umbrales visuales pertinentes. Al conservar T en las posiciones de invalidación, B vuelve a recibirla. La prueba de renderer real requiere dos clientes y queda pendiente.

### [MEDIO] S-01 — SOSPECHA: CCTV puede dejar conectado a un cliente sin un PlayerBase elegible para los nuevos envíos

- **Donde:** `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2290` y `:2315-2320`; `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1492-1507`; `scripts/5_Mission/LFPG_ControlSessionRegistry.c:87-112` y `:288-306`.
- **Que pasa:** la equivalencia «todos los clientes conectados = todos los PlayerBase enumerados con identidad» no está demostrada. Este mod tiene una sesión legítima de CCTV que guarda la identidad y el cuerpo por separado y después ejecuta `SelectPlayer(sender, null)` seguido de `SelectSpectator`. Su dispatcher prevé expresamente que `sender.GetPlayer()` no esté disponible (`LFPG_RPCServerHandlerImpl.c:47-54`). Eso identifica un caso que hay que probar, pero **no demuestra** por sí solo el contenido de `GetPlayers()` ni el resultado de `body.GetIdentity()`.
- **Escenario de fallo, condicional:** B recibe cables, entra en CCTV y su cuerpo deja de aparecer con identidad en el bucle. A cambia esos cables mientras B está conectado. Ni `m_BroadcastAll` supera el guard de identidad ni los broadcasts ordinarios conservan otro destinatario para B. Al salir, `FinishCCTV` reasocia el cuerpo y manda confirmación, sin solicitar full sync; el full sync de misión es de una sola ejecución (`scripts/5_Mission/LFPG_MissionInit.c:331-341`). Si el motor sí conserva el cuerpo enumerado y su identidad, esta hipótesis concreta queda refutada.
- **Que haria:** medir en un servidor de prueba la presencia del cuerpo en `GetPlayers()` y su identidad antes/durante/después de CCTV; mutar un cable conocido durante la sesión y comprobar el estado al volver. Si se confirma, definir destinatarios por sesiones conectadas o resincronizar explícitamente al volver. La identidad retenida ya tiene un precedente real en `LFPG_ControlSessionRegistry.c:100` y sus RPC dirigidos (`:226` y `:243`).
- **Estado:** SOSPECHA DE MOTOR, no fallo confirmado ni condición usada para rechazar. Las declaraciones nativas disponibles no resuelven esta semántica.

## RESPUESTA A LOS CINCO FOCOS DEL BRIEF

| Foco | Conclusión |
|---|---|
| 1. Abortar por cable ajeno | Correcto conforme a la decisión firmada de SEC02. Sí afecta a una base compartida: B no puede reemplazar un cable creado por A aunque considere suya la base y A esté ausente. El criterio real es creador del cable, no propiedad territorial ni pertenencia a grupo. Es el alcance de la política elegida; antes existía un bypass respecto a los alicates. Propios y sin reclamar siguen permitidos. No propongo reabrir la variante permisiva ni duplicar el IN. |
| 2. Identidad null | No he demostrado una regresión en el join/rejoin ordinario: las fases del full sync ya exigían identidad en `LFPG_NetworkManagerImpl.c:2717` y `:2812` antes del parche. Los llamadores de estos envíos son del servidor; el dispatch se separa por `#ifdef SERVER` en `scripts/4_World/LFPG_PlayerRPC.c:97-101`. CCTV es la excepción concreta que queda por verificar, S-01. No se puede garantizar desde este source la transición nativa de muerte/carga. |
| 3. `m_BroadcastAll` | Abarca cada PlayerBase enumerado con identidad, sin filtro espacial; no prueba cobertura de toda conexión. S-01 cuestiona la equivalencia en una función existente del producto. Además, F-03 demuestra que fuera del overflow la selección espacial actual no basta para invalidar estado previamente recibido. |
| 4. Preflight y admisión real | Los valores de los topes coinciden: LFPG aplica setting positivo y hardcap 64; vanilla aplica setting positivo, o 64 como fallback. El estado sobre el que se cuenta no coincide: F-02. El fallo posterior de grafo sigue sin rollback, F-01. Los caminos excepcionales de delegación dinámica no quedan garantizados por una réplica de recuento. |
| 5. Vocabulario vanilla legítimo | No he encontrado un nombre emitido por el cliente estándar de este árbol que el nuevo código rechace. El cliente enumera los puertos (`scripts/4_World/LFPG_Actions.c:633-663`), orienta OUT/IN y envía esos nombres (`scripts/4_World/LFPG_WiringClient.c:353-409`). El fallback compartido declara `output_1` OUT / `input_main` IN (`scripts/4_World/LFPG_IDevice.c:820-868`). El handler acepta esos nombres y normaliza el vacío antes de guardar (`LFPG_RPCServerHandlerImpl.c:464-538`). No extiendo esta conclusión a overrides de otros mods que no están en el árbol. |

El cambio adicional del orquestador es correcto en el camino leído: `SendServerSettingsTo` obtiene una identidad y la usa como destinatario en `LFPG_RPCServerHandlerImpl.c:2262-2279`. Lo llama la petición individual de full sync (`:2247-2259`). No he encontrado otro defecto específico en esas cuatro líneas.

No he elevado a hallazgos confirmados las carreras mencionadas por la lane A. El handler leído no cede ejecución entre el preflight y el add; la falta de un lock de origen no demuestra por sí sola intercalado de dos RPC en el motor. Tampoco he usado el caso de target vacío como fixture persistido normal: el validador de carga rechaza ese campo vacío. La discrepancia literal/clave normalizada existe, pero su vía de entrada requiere evidencia adicional.

## CITAS COMPROBADAS

He abierto las referencias de ambos informes en los ficheros finales; para las líneas históricas de la lane B también he consultado HEAD. «PARCIAL» distingue una cita real de una conclusión que esa cita no alcanza a probar. No he encontrado una referencia de fichero inventada. Sí hay dos conclusiones de cierre incorrectas, identificadas abajo.

Abreviaturas de la tabla:

- **H** = `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`.
- **N** = `scripts/5_Mission/LFPG_NetworkManagerImpl.c`.
- **WH** = `scripts/3_Game/LFPG_WireHelper.c`.
- **WO** = `scripts/4_World/LFPG_WireOwnerBase.c`.
- **I** = `scripts/4_World/LFPG_IDevice.c`.
- **D** = `scripts/3_Game/LFPG_Defines.c`.
- **R** = `scripts/4_World/LFPG_CableRenderer.c`.

| Informe / cita | La abrí | ¿Dice lo afirmado? |
|---|---|---|
| A — H:630-636; H:1142-1146 | Sí | SI. Lectura de política e identidad equivalente al corte. |
| A — H:647-674 | Sí | SI. Preflight bajo lock y antes de mutación; libera el lock al rechazar. |
| A — H:770; N:1552 | Sí | SI. Cuatro argumentos; la firma mantiene `allowOthers=true` por defecto. |
| A — H:994-1031 | Sí | SI. Scan del puerto de origen por propiedad del cable. |
| A — H:1034-1098 | Sí | SI. Scan de stores nativos y vanilla para incoming ajeno. |
| A — WH:230-237 | Sí | SI. Política propio/sin reclamar; sender vacío falla cerrado. |
| A — N:1603; H:2140-2147 | Sí | SI para comparación literal frente a normalización. PARCIAL para inferir que un puerto de destino vacío sobrevive al loader ordinario. |
| A — H:647-657; H:936-992 | Sí | PARCIAL. Implementa el recuento descrito, pero ese recuento no equivale a todas las eliminaciones del reemplazo: F-02. |
| A — WH:164-172; N:884-892 | Sí | SI. Topes y asimetría LFPG/vanilla correctamente citados. |
| A — H:735-740 | Sí | SI. Puerto vacío de origen vanilla se trata como `output_1` al reemplazar. |
| A — H:794-830 | Sí | SI. Persistencia/rebuild de borrados y rechazo si falla el add; no hay rollback. |
| A — D:520 | Sí | SI. Constante 12. El mecanismo aplica límites entrantes y salientes por separado en el grafo; no he supuesto un total combinado por el comentario de D:516. |
| A — H:464-538; H:468-476; H:503-511 | Sí | SI. Validación por puertos nativos o enumeración vanilla. |
| A — I:96-107 | Sí | SI. ID vanilla calculado sin escribir un ID LFPG en la entidad. |
| A — I:820-868; I:820-851 | Sí | SI para el fallback presente en este árbol. No demuestra contratos de futuras ampliaciones. |
| A — H:881-933 | Sí | SI. Helpers de normalización y pertenencia/dirección. |
| A — H:837-878 | Sí | SI en el orden local: inserción antes del broadcast del add. |
| A — WO:195-201 | Sí | SI. Add nativo actualiza estado sincronizado. No he dado por probado que el SyncVar por sí solo transporte todo el blob. |
| A — H:850-861 y «no difundimos el fantasma» | Sí | NO como conclusión global. Conserva el store, reconstruye y retorna; un sync posterior lo serializa: F-01. |
| A — H:~1581 | Sí | SI. `--` está en comentario de searchlight y es preexistente. |
| A — H:685-686 | Sí | SI. Locales `ref` preexistentes; no son adiciones T2. |
| A — H:581 | Sí | SI. Creación usa el mismo `GetPlainId()` que la política posterior. |
| A — H:707-709; H:745-747 | Sí | SI. Llamadas multilinea preexistentes. No son defectos nuevos de este diff. |
| A — H:813-816 | Sí | SI. Commit de mutación de origen y delta en la rama de add fallido. |
| B — N:2142; N:2120-2121; N:2152 (HEAD:2151) | Sí, final y HEAD | SI. Filtro de interés, guard y destinatario concreto. |
| B — N:2315-2317; N:2344 (HEAD:2340) | Sí, final y HEAD | SI para cada jugador enumerado con identidad. PARCIAL para equivalencia con toda conexión: S-01. |
| B — N:2458-2460; N:2491 (HEAD:2484) | Sí, final y HEAD | SI. Delta dirigido; incluye posiciones de destinos del delta. |
| B — N:2594-2595; N:2626 (HEAD:2618) | Sí, final y HEAD | SI para el cambio de destinatario. PARCIAL para suficiencia del interés actual tras borrado: F-03. |
| B — N:2636-2638; N:2673 (HEAD:2663) | Sí, final y HEAD | SI. Envío individual vanilla. |
| B — N:2784-2788; N:2798 (HEAD:2783) | Sí, final y HEAD | SI. Envío al jugador que está recibiendo full sync. |
| B — N:2717; N:2812 | Sí, también versiones HEAD | SI. Exigencia de identidad preexistente en el ciclo de full sync. |
| B — N:2985-2987; N:3002 (HEAD:2984) | Sí, final y HEAD | SI. Estado vacío a un jugador concreto. |
| B — N:3023-3025; N:3047 (HEAD:3027) | Sí, final y HEAD | SI. Blob de owner a un jugador concreto. |
| B — N:6348; N:6381 (HEAD:6361) | Sí, final y HEAD | SI. Control positivo, ya dirigía el RPC a `pid`. |
| B — H:1969, settings | Sí, en HEAD | SI como cita histórica. En el final es H:2279 y ya usa `pid` por el cambio del orquestador; no es una cita falsa por desplazamiento. |
| B — R:2385 y «ya no dibuja a más de 50 m» | Sí, con ramas posteriores | NO si se usa para excluir a quien queda a >70 m del owner y destinos actuales tras borrar. La línea calcula un umbral; owner, esfera y extremos se evalúan por separado. F-03. |
| B — N:624-689 | Sí | SI. Diez `+=` preexistentes del scheduler. |
| B — N:343 | Sí | SI. Literal de ruta con backslash escapado preexistente. |
| B — N:2640 | Sí | SI. `ref` local preexistente. |
| Brief/B — `P:\scripts\3_game\gameplay.c:115-117` | Sí, copia local accesible del source vanilla | SI. Firma con cuarto argumento `PlayerIdentity recipient = NULL`; el comentario especifica NULL = todos los clientes. La lane B reconocía que no la había leído; aquí queda comprobada. |

Los recuentos publicados por la lane B también cuadran: nueve `.Send(`, ninguno nuevo y cero `noExclude` en el fichero final; 795 líneas con cada llave y 2.632 líneas con cada paréntesis. Esto no convierte el recuento de signos en una prueba de compilación.

## COMPROBACIONES EJECUTADAS

- Diff final frente a HEAD y frente al patch suministrado: coinciden en ambos ficheros. N: 34 inserciones/14 eliminaciones; H: 332/22.
- Revisión de **líneas añadidas**, descontando comentarios y literales donde corresponde: cero ternarios, incrementos/decrementos, asignaciones `+=`/`-=`, `foreach`, locales `ref` nuevos o escapes adyacentes prohibidos.
- Balance léxico descontando comentarios/literales: N tiene 795/795 llaves, 2.819/2.819 paréntesis y 223/223 corchetes; H tiene 359/359, 1.571/1.571 y 43/43. No es un parser de Enforce.
- `git diff --check` con `core.whitespace=cr-at-eol`: exit 0. La ejecución sin reconocer CRLF marcaba las 34 líneas añadidas de N como trailing whitespace; HEAD de N ya usa CRLF. No lo he convertido en hallazgo.
- Tres modelos de fallo y controles positivos/negativos ejecutados con Python desde stdin y `-B`, sin crear scripts ni cachés. El modelo de recuento exploró 1.452 combinaciones: cero falsos pases y 254 falsos rechazos. Algunas combinaciones sintéticas incluyen filas nulas o duplicadas; **no son 1.452 escenarios legítimos de DayZ ni una cobertura exhaustiva**. F-02 se sostiene en el fixture simple válido descrito.
- `enfcheck.py` no apareció en las búsquedas de la raíz y `_dev`. No atribuyo a ese linter una ejecución ni una salida.

Los modelos contrastan decisiones del parche con el estado que deja la mutación real o con el invariante «un cliente que conserva el cable anterior necesita su invalidación». No comprueban el transporte nativo, el intérprete Enforce ni la imagen del renderer. El código ejecutado se incluye al final para que la evidencia sea revisable.

Huella SHA-256 del código revisado:

| Fichero | SHA-256 |
|---|---|
| `scripts/5_Mission/LFPG_NetworkManagerImpl.c` | `538d1089a3120cd0d70945545f104b780cdb3466d664d3ac1d9f60c81276609b` |
| `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c` | `7c980f09a1f5c71f41a4ad3c411ef49d9e0fdde6c586df74045820cbf906aea1` |

## LO QUE NO PUDE VERIFICAR

- Compilación Enforce, ejecución de RPC, render y persistencia real tras reinicio. No he ejecutado DayZ, AddonBuilder ni builds, conforme al encargo.
- Semántica nativa de `GetPlayers()`/`GetIdentity()` durante CCTV, carga, reconexión y muerte. El caso CCTV queda como S-01, no como hecho probado por comentarios del código.
- Población real de datos legacy de los servidores. F-01 demuestra un estado conservable por el código; no afirma que ese estado esté desplegado hoy.
- Overrides de dispositivos o puertos aportados por otros mods no presentes en el árbol.
- Coste real del scan de todos los stores por cada intento autorizado y tráfico tras los unicasts. No he convertido «recorre muchos dispositivos» en un problema de rendimiento sin medición.
- No he auditado el módulo entero ni T1 como encargo nuevo. Los problemas preexistentes vistos de paso se distinguen de las líneas añadidas.
- No he actualizado memoria externa: la restricción expresa permite escribir únicamente este dictamen. Este fichero conserva las evidencias durables de la revisión.

## LA PREMISA DE ESTE ENCARGO

**Los diagnósticos de partida son válidos, pero la partición SEC03/SEC20 y el veto de la lane A al manager dejan fuera la frontera real de éxito.** Guardar un cable no equivale a completar una conexión eléctrica. El contrato de reemplazo debe abarcar conflictos, almacén, índices, grafo y publicación. La lane A explica honestamente que no puede hacer rollback por su reparto de ficheros, pero eso no permite cerrar la ficha como si la propiedad de producto estuviera satisfecha. El límite de una lane no cambia el comportamiento exigido al sistema.

**SEC20(b) necesita revisar todos los productores del estado cliente.** Mover un broadcast debajo de una comprobación arregla ese envío concreto. Mientras el full sync vuelva a publicar el almacén sin considerar el resultado rechazado, sigue existiendo el fantasma. F-01 no depende de la hipótesis del SyncVar nativo que menciona el informe: tiene un camino vanilla explícito.

**SEC01 corrige el destinatario; también obliga a preservar las invalidaciones.** Distancia de visualización e historial de datos recibidos son dimensiones distintas. Un observador puede dejar de ser interesado precisamente porque se borró lo que observaba, y aun así necesita enterarse del borrado. La frase de la lane B sobre no dibujar lejos no prueba lo contrario. El contraste útil es la invalidación de deltas nativos, que sí conserva los destinos de los cables eliminados.

**SEC02 está bien resuelta respecto a la decisión firmada.** La propiedad utilizada es la del creador del cable y puede incomodar flujos cooperativos, pero esa consecuencia no convierte el bypass anterior en una función que haya que preservar. No hay evidencia para abrir la política por defecto. El camino propio/sin reclamar y el de `AllowCutOthersWires=true` mantienen su coherencia.

**El test de capacidad propuesto por A debe usar un estado alcanzable.** El splitter del árbol tiene tres OUT. «Saturarlo en otros puertos hasta 64/128» no es un caso normal obtenible con ese dispositivo y el cliente legítimo. F-02 usa un límite admitido de 1 y dos salidas reales; F-01 declara expresamente su fixture de datos previos. No confundo un fixture construido para atacar una defensa con un flujo de jugadores estándar.

Los cinco focos sirven, pero los riesgos determinantes aparecen **entre** ellos: admisión del almacén frente a admisión del grafo, y destinatarios nuevos frente a clientes que conservan el estado anterior. La separación por archivo ocultaba ambas relaciones.

## ANEXO — MODELOS REPRODUCIBLES

[DESIGN] Modelo de las decisiones leídas, ejecutado con Python desde la raíz física del repositorio y sin escribir archivos. No es una implementación propuesta para el mod ni una ejecución de Enforce. F-02 corresponde a R1, F-03 a R2 y F-01 a R3 en la salida.

```python
from pathlib import Path
import re, itertools
root = Path.cwd()
def read(p):
    return (root / p).read_text(encoding="utf-8-sig")
handler = read("scripts/5_Mission/LFPG_RPCServerHandlerImpl.c")
manager = read("scripts/5_Mission/LFPG_NetworkManagerImpl.c")
graph = read("scripts/5_Mission/LFPG_ElecGraphImpl.c")
helper = read("scripts/3_Game/LFPG_WireHelper.c")
defines = read("scripts/3_Game/LFPG_Defines.c")
settings = read("scripts/3_Game/LFPG_Settings.c")
renderer = read("scripts/4_World/LFPG_CableRenderer.c")
def constant(name, source=defines):
    return float(re.search(r"\b" + name + r"\s*=\s*([\d.]+)", source).group(1))
cap = int(constant("LFPG_MAX_WIRES_PER_DEVICE"))
edgecap = int(constant("LFPG_MAX_EDGES_PER_NODE"))
radius = constant("LFPG_CULL_DISTANCE_M") + 20
assert constant("LFPG_SETTINGS_MIN_WIRES_DEVICE", settings) == 1
assert "if (existPort == srcPort)" in handler
assert "wd.m_TargetPort == targetPort" in manager
assert "existOut.Count() >= LFPG_MAX_EDGES_PER_NODE" in graph
assert "blob.wires.Insert(wires[w]);" in manager

# Model only: tuple = (source_port, target_id, target_port, creator).
# Count rules read from H:936, WH:153 and N:869. No Enforce execution.
def preflight(wires, src_port, setting, native=True):
    remaining = len(wires)
    for wire in wires:
        if wire is None:
            continue
        existing = wire[0]
        if not native and existing == "":
            existing = "output_1"
        if existing == src_port:
            remaining -= 1
    if native:
        return not ((setting > 0 and remaining >= setting) or remaining >= cap)
    return remaining < (setting if setting > 0 else cap)

def source_replacement(wires, src_port, native=True):
    def conflicts(wire):
        if wire is None:
            return False
        port = wire[0] if native else (wire[0] or "output_1")
        return port == src_port
    return [w for w in wires if not conflicts(w)]

def target_replacement(wires, target, port):
    return [w for w in wires if w is None or w[1:3] != (target, port)]

def store_accepts(wires, candidate, setting, native=True):
    if native and len(wires) >= cap:
        return False
    limit = setting if setting > 0 else cap
    if len(wires) >= limit:
        return False
    return all(w is None or w[:3] != candidate[:3] for w in wires)

old = [("output_1", "lamp", "input_main", "A")]
candidate = ("output_2", "lamp", "input_main", "A")
post = target_replacement(source_replacement(old, candidate[0]), candidate[1], candidate[2])
assert preflight(old, candidate[0], 1) is False
assert store_accepts(post, candidate, 1) is True
print("R1 repro: preflight=False, remaining_after_both_replacements=0, real_store_admits=True")
# Controls: unrelated target really is full; same source port really frees capacity.
assert not store_accepts(source_replacement(old, candidate[0]), ("output_2","other","input_main","A"), 1)
assert preflight(old, "output_1", 1)
print("R1 controls: unrelated_target=REJECT, same_output=ACCEPT")

checked = false_pass = false_reject = 0
choices = [None, ("output_1","lamp","input_main","A"), ("output_2","other","input_main","A")]
for size in range(5):
    for state in itertools.product(choices, repeat=size):
        for native in [True, False]:
            for setting in [0, 1, 2, 3, 64, 128]:
                p = preflight(state, candidate[0], setting, native)
                post = target_replacement(source_replacement(state, candidate[0], native), candidate[1], candidate[2])
                actual = store_accepts(post, candidate, setting, native)
                checked += 1
                false_pass += int(p and not actual)
                false_reject += int(not p and actual)
print(f"COUNT_MODEL: cases={checked}, false_pass={false_pass}, false_reject={false_reject}")
assert false_pass == 0 and false_reject > 0

# Invalidation invariant: clients that held the old wire need its removal.
# Coordinates are local x offsets; all entities live and visible, level ground.
owner, previous_target, new_target = 0.0, 60.0, -10.0
players = {"actor": 0.0, "observer": 72.0}
def interested(targets):
    return {name for name,x in players.items() if abs(x-owner) <= radius or any(abs(x-t) <= radius for t in targets)}
old_recipients = interested([previous_target])
new_recipients = interested([new_target])
broadcast_recipients = set(players) if new_recipients else set()
assert "observer" in old_recipients
assert "observer" in broadcast_recipients and "observer" not in new_recipients
assert 72 <= constant("LFPG_CULL_DISTANCE_M")+25
assert abs(72-previous_target) <= constant("LFPG_DEVICE_BUBBLE_M")
assert "m_KnownCableDevices.Contains(deviceId)" in renderer
assert interested([]) == {"actor"}
print("R2 repro: old_allclients={actor,observer}, new_unicast={actor}; observer keeps old vanilla wire")
assert "observer" in interested([new_target, previous_target])
print("R2 control: preserve_removed_target => observer receives invalidation")

# Persisted old-version ports are nonempty and survive ValidateWireData.
# New request uses the allowed output_1, with the default store cap of 64.
legacy = [(f"legacy_{i}", f"load_{i}", "input_main", "A") for i in range(edgecap)]
new_wire = ("output_1", "victim_load", "input_main", "A")
victim_before = [("output_1", "victim_load", "input_main", "A")]
assert preflight(legacy, new_wire[0], cap, False)
victim_after = target_replacement(victim_before, new_wire[1], new_wire[2])
assert store_accepts(legacy, new_wire, cap, False)
persisted = legacy + [new_wire]
accepted = persisted[:edgecap]  # AddEdgeInternal refuses every next out edge.
assert not victim_after and new_wire not in accepted and new_wire in persisted
print(f"R3 repro: store={len(persisted)}, graph={len(accepted)}, old_incoming=0, next_snapshot_contains_rejected=True")
control = legacy[:-1] + [new_wire]
assert new_wire in control[:edgecap]
print("R3 control: free_one_graph_slot => new_wire accepted")

# SEC02 policy truth table and vanilla input vocabulary controls.
def can_cut(creator, sender):
    return bool(sender) and creator in ("",sender)
assert can_cut("A","A") and can_cut("","A") and not can_cut("B","A") and not can_cut("A","")
assert 'if (portName == "")' in handler
assert 'if (IsVanillaSource(e)) return "output_1";' in read("scripts/4_World/LFPG_IDevice.c")
assert 'if (IsVanillaConsumer(e)) return "input_main";' in read("scripts/4_World/LFPG_IDevice.c")
print("SEC02 model controls: own=ALLOW, unclaimed=ALLOW, foreign=DENY, empty_sender=DENY")
print("NOTE: all results are source-backed Python models; NOT DayZ runtime tests.")

```

Salida observada, exit code 0:

```text
R1 repro: preflight=False, remaining_after_both_replacements=0, real_store_admits=True
R1 controls: unrelated_target=REJECT, same_output=ACCEPT
COUNT_MODEL: cases=1452, false_pass=0, false_reject=254
R2 repro: old_allclients={actor,observer}, new_unicast={actor}; observer keeps old vanilla wire
R2 control: preserve_removed_target => observer receives invalidation
R3 repro: store=13, graph=12, old_incoming=0, next_snapshot_contains_rejected=True
R3 control: free_one_graph_slot => new_wire accepted
SEC02 model controls: own=ALLOW, unclaimed=ALLOW, foreign=DENY, empty_sender=DENY
NOTE: all results are source-backed Python models; NOT DayZ runtime tests.
```
