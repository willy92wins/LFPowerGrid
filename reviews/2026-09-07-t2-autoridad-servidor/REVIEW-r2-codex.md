## VEREDICTO

RECHAZAR.

F-02 está cerrado en los contratos de almacén presentes en este árbol. F-01 y F-03 mejoran, pero sus cierres son parciales: el rollback cambia el orden que decide la admisión tras un rebuild, y el corte por puerto todavía excluye al observador del extremo antiguo. Encuentro además tres regresiones de ronda 2: nodos huérfanos tras un rechazo, reactivación de un aspersor desconectado y retención indefinida de invalidaciones de owners desaparecidos.

Revisión estática adversarial del 2026-09-07 sobre `fix/t2-r2-transaccion`, HEAD `8cb622244d0fcb073e5b7aa4d9d8172dfbd2ef29`. El diff contra HEAD contiene T2 completo: 947 inserciones y 183 eliminaciones en los dos ficheros existentes, más el nuevo `scripts/5_Mission/LFPG_FinishWiringTxn.c`, untracked y excluido de esas cifras. No he compilado ni ejecutado DayZ. Los repros ejecutados son modelos Python de mecanismos leídos; incluyo código, resultados y límites.

Abreviaturas de citas posteriores:

- **N**: `scripts/5_Mission/LFPG_NetworkManagerImpl.c`.
- **H**: `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`.
- **E**: `scripts/5_Mission/LFPG_ElecGraphImpl.c`.
- **WH**: `scripts/3_Game/LFPG_WireHelper.c`.
- **WO**: `scripts/4_World/LFPG_WireOwnerBase.c`.
- **I**: `scripts/4_World/LFPG_IDevice.c`.
- **R**: `scripts/4_World/LFPG_CableRenderer.c`.
- **D**: `scripts/3_Game/LFPG_Defines.c`.

## ESTADO DE TUS TRES HALLAZGOS

| Hallazgo | ¿Cerrado? | Evidencia (path:line) | Si no, qué queda vivo |
|---|---|---|---|
| F-01 | **PARCIAL**. El fixture original de G con 12 filas y H→D sí queda corregido. | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:998`, `:1286`, `:1315`, `:1384`; `scripts/5_Mission/LFPG_ElecGraphImpl.c:276`. | Retira el nuevo y restaura H→D. Pero el append altera el orden de admisión: un cable viejo puede desaparecer del grafo en el siguiente rebuild. N-01 demuestra además que no se revierten todos los nodos creados. |
| F-02 | **SÍ**, respecto al falso «full» y los topes de los stores presentes. | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:1039`, `:1081`, `:1128`, `:1175`, `:1279`, `:1347`. | El cable al mismo destino desde otro OUT del origen se descuenta y retira. La coincidencia por origen **y** destino se recoge una vez. No encuentro un falso «cabe» en estos recuentos. |
| F-03 | **PARCIAL**. Reemplazo y corte completo por el handler cubiertos; corte por puerto abierto. | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:1431`, `:3223`; `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:992`, `:1874`, `:1886`. | `HandleCutPort` elimina sin registrar el extremo antiguo. El fallback de incoming tampoco lo registra. La cola tiene la fuga nueva N-03. |
| S-01 | **SOSPECHA, sin cerrar**; no determina el veredicto. | `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:47`, `:1295`; `scripts/5_Mission/LFPG_NetworkManagerImpl.c:3237`; `scripts/5_Mission/LFPG_ControlSessionRegistry.c:299`. | Sigue sin demostrarse qué devuelve el motor para la enumeración/identidad del cuerpo durante CCTV. No encuentro mejora ni agravamiento demostrado en esta ronda. |

### F-01 residual — [GRAVE] Restaurar las filas al final cambia qué cable funciona después de reconstruir

**Dónde:** `scripts/5_Mission/LFPG_NetworkManagerImpl.c:1066`, `:1279`, `:1315`; `scripts/5_Mission/LFPG_ElecGraphImpl.c:276`, `:1346`, `:1357`.

**Mecanismo:** se guarda `m_Index`, pero no se utiliza al restaurar: `live.Insert(wd)` añade al final. El rebuild recorre el array y admite sus primeras aristas hasta los límites. El orden es funcional para los datos legacy que esta ronda promete conservar. Tampoco basta con devolver una fila a su índice si la eliminación ha desplazado otras: debe recuperarse la secuencia completa previa.

[EXACT — fragmento leído en N:1314-1321]

```cpp
            if (live)
                live.Insert(wd);

            ReverseIdxAdd(wd.m_TargetDeviceId, wd.m_TargetPort, row.m_OwnerId);
            PlayerWireCountAdd(wd.m_CreatorId, 1);

            string graphSrc = FinishTxnSourcePortKey(wd, row.m_IsLfpgOwner);
            NotifyGraphWireAdded(row.m_OwnerId, wd.m_TargetDeviceId, graphSrc, wd.m_TargetPort, wd);
```

**Escenario:**

1. G vanilla tiene 13 filas: una conexión válida `output_1→T.input_main`, seguida de 12 conexiones con OUT `legacy_0..legacy_11` hacia otros consumidores. El grafo admite la primera y 11 legacy; la última legacy queda fuera.
2. D vanilla tiene 12 incoming desde otros generadores, con puertos no vacíos `legacy_in_0..legacy_in_11`. Ninguno coincide con `input_main`. Todo está registrado, con geometría válida, componentes pequeños, cuota disponible y `MaxWiresPerDevice=64`. Las filas son propias o sin reclamar.
3. Se solicita `G.output_1→D.input_main`. Se retira la conexión a T. El nuevo cable cabe en el store; el grafo lo rechaza por las 12 entradas de D.
4. El rollback retira el nuevo y restaura la conexión a T **al final** del array. En ese momento la arista vieja vuelve a entrar.
5. Un rebuild posterior admite las 12 legacy de G y rechaza la conexión a T, ahora decimotercera. La conexión que funcionaba queda solo en el store.

El loader permite este estado de actualización: copia los puertos y añade filas sin aplicar el cap de aristas (`N:5300-5304`, `:5317`, `:5331`, `:5350`); `WH:35-83` exige campos y geometría, sin validar ese vocabulario. No afirmo que un servidor concreto tenga esas filas.

**Consecuencia:** degradación eléctrica y divergencia store/grafo de una conexión anterior; no es un crash ni desaparición de la fila en disco. Es residuo de F-01. `REPORT.md:126` se equivoca al afirmar que reponer los mismos objetos deja el mundo como antes.

**Qué haría:** restaurar el orden completo de los stores y garantizar la recuperación del estado previo del grafo, sin seleccionar otras filas al reconstruir. Mantendría el formato persistente. El modelo reproduce el fallo tanto con eliminación ordenada como con intercambio con la última fila: no depende de asumir una semántica concreta de `Remove`.

**Lo que sí funciona:** en el fixture original, G conserva sus 12 filas sin reordenarlas y H solo tenía H→D: el nuevo no aparece en el siguiente snapshot y H→D vuelve. Los créditos tampoco se pierden en la secuencia normal: cada notificación va seguida de su retirada del índice (`N:1235-1237`); el crédito se compara y consume una vez (`N:2042-2048`). Los recuentos de jugador/incoming se compensan para un estado inicial consistente (`N:1317-1318`, `:1997-2025`, `:2086-2102`, `:2116-2127`). Eso no acredita igualdad del orden ni de todos los estados derivados.

### F-02 — El caso de capacidad sí queda corregido

Con `MaxWiresPerDevice=1` y `S.output_1→D`, pedir `S.output_2→D` recoge esa fila por destino, la resta una sola vez del origen y la elimina antes de insertar: el resultado sigue teniendo una fila. Pedir `S.output_2→U`, sin coincidencia de origen ni destino, conserva la fila anterior y se rechaza. Sustituir desde el mismo OUT vuelve a dejar una fila. Esto cierra el escenario original.

Los controles comparan admisión contra las dos retiradas secuenciales y el límite del consumidor: 199.936 casos pequeños y 9.834 de frontera, settings 1..128, incluyendo 63/64/65 y 127/128/129. No demuestran compilación ni estados internos arbitrariamente corruptos. El límite nativo reproduce `WH:164-175`; vanilla reproduce `N:889-897`. No identifico un descuento duplicado en filas distintas de un store normal.

### F-03 residual — [MEDIO] Cortar el OUT concreto conserva el fantasma visual

**Dónde:** `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1874-1887`; selección de destinatarios en `scripts/5_Mission/LFPG_NetworkManagerImpl.c:3200-3261`.

**Escenario:** G=0, T=60, actor=0 y observador B=72, con G→T ya recibido y waypoints que dividen los 60 m en tramos válidos de menos de 50 m. B tiene carrete, mira hacia T y sigue junto al extremo antiguo. El actor corta `output_1`, OUT, con sus propios alicates/cable. Pasa las comprobaciones de `H:1746-1795`.

Se elimina la fila y se llama al broadcast sin registrar T. El blob vacío solo llega al actor. B queda a 72 m de G, fuera del radio de envío de 70 m, y conserva su estado anterior. El renderer permite ese caso: descarte por owner a 75 m (`R:2386`, `:2417-2441`), burbuja de extremo (`R:2539-2553`) y sin generación anunciada que fuerce actualización de vanilla conocido (`R:1479-1512`).

**Qué haría:** capturar la invalidación antes de toda retirada vanilla, incluyendo corte por puerto y rescates, conservando destinatarios explícitos.

| Camino de retirada | Cobertura observada |
|---|---|
| Reemplazo de la transacción | Sí: N:1431, N:1474 y N:1535. |
| Corte completo en `HandleCutWires` | Sí: H:992, H:1012, H:1024. |
| Incoming encontrado por índice | Sí: N:2313 y N:2330. |
| Corte OUT concreto | **No**: H:1874-1886. |
| Incoming rescatado por barrido | **No**: H:2013-2027. |
| Vaciado vanilla de integridad/lifecycle | **No**: N:5562-5569; tampoco su fallback N:5674-5689. |

Los últimos caminos son residuos de F-03, no regresiones adicionales de ronda 2. El contraejemplo de corte OUT no necesita índice stale, datos legacy, destino desaparecido ni CCTV.

## REGRESIONES NUEVAS

### [GRAVE] N-01 — Un rechazo crea un nodo huérfano y puede bloquear el cableado de todo el servidor

**Dónde:** fallo de transacción en `scripts/5_Mission/LFPG_NetworkManagerImpl.c:998-1004`; creación antes del rechazo en `scripts/5_Mission/LFPG_ElecGraphImpl.c:612-620`, `:1306-1307`; siguiente rechazo temprano en `:1683-1685`.

**Qué pasa:** la inserción puede devolver false después de crear nodos. El rollback solo desinserta la fila y repone conflictos. El cierre del batch limpia exclusivamente extremos anotados al **retirar** aristas (`E:805-808`, `:1030-1043`), no todos los nodos creados por el intento. False no significa «el grafo no cambió».

**Escenario:**

1. Grafo con 2.047 nodos: la estrella de G con sus 12 OUT legacy ocupa 13; otras 1.017 parejas independientes ocupan 2.034. Las parejas son de otros creadores o sin reclamar; el actor conserva cuota disponible.
2. Se pide `G.output_1→D.input_main`, con D existente, registrado, próximo a G y sin nodo/aristas. G no tiene fila en `output_1`: no se retira ningún conflicto.
3. El preflight pasa: 2.047 < 2.048 y el componente solo crecería de 13 a 14. El límite global real es 2.048 (`D:479`).
4. El grafo crea D y sube a 2.048; después rechaza la arista porque G tiene 12 salidas. Se retira la fila nueva, pero D no está en la limpieza diferida y sobrevive sin aristas.
5. Todo siguiente intento, incluso en otra pareja ya existente, cae en `E:1684`, llamado desde `H:602`.

**Consecuencia:** degradación global del cableado hasta una reconstrucción que elimine el huérfano; no caída de proceso ni pérdida de filas. `m_GraphFullRebuildRequired` se marca, pero no ejecuta por sí solo un rebuild (`N:1623-1635`).

**Por qué es nueva:** el rechazo anterior reconstruía inmediatamente: `HEAD:H:776-785`, comportamiento retenido en ronda 1 según `REVIEW-CODEX.md:14-22` e `INFORME-RONDA1-laneA.md`. Su poda elimina D (`E:291-324`), aun con el defecto antiguo de conservar la fila rechazada. Ronda 2 elimina esa reconciliación sin deshacer este otro efecto.

**Qué haría:** registrar y revertir los nodos creados por el intento, con sus índices/contadores; o impedir ese efecto antes de confirmar la admisión. No volvería a reconstruir sobre filas rechazadas. Control: rechazo con 2.047 nodos → siguen 2.047, D sin nodo y una operación independiente admisible sigue pasando.

No uso el falso repro «entrar ya con 2.048 nodos y fallar al restaurar»: el handler lo impide. El caso confirmado cruza el umbral **dentro** del intento.

### [MEDIO] N-02 — El lote de retiradas reactiva un aspersor cuyo cable acaba eliminándose

**Dónde:** `scripts/5_Mission/LFPG_NetworkManagerImpl.c:1225-1238`, seguido de eliminación de filas en `:1240-1282`; efectos de la notificación en `:1677`, `:5950-5965`, `:6008-6022`, `:6061-6082`.

**Escenario con dos cables normales:** bomba `LFPG_WaterPump_T2` P alimentada, aspersores A/B, mismo creador, política de ajenos desactivada, `MaxWiresPerDevice>=2`. Crear primero `P.output_2→B.input_0` y después `P.output_1→A.input_0`. Solicitar `P.output_2→A.input_0`.

Son puertos válidos (`scripts/4_World/LFPG_WaterPump.c:390-400`; `scripts/4_World/LFPG_Sprinkler.c:75-77`). La bomba se declara fuente (`LFPG_WaterPump.c:460-462`) y hereda el permiso OUT→IN de `WO:268-287`.

1. El conjunto sigue el orden del array: B por OUT, después A por IN.
2. Notificar B lo desactiva y recorre el store, que todavía contiene A.
3. Notificar A lo desactiva y recorre el store todavía completo: **reactiva B** y le asigna P como fuente.
4. Se retiran ambas filas. La notificación del nuevo solo ve A y no limpia B.
5. La limpieza eléctrica pone `m_PoweredNet=false` en B, pero ese setter no apaga `m_SprinklerActive` ni borra la fuente (`E:1605-1607`; `LFPG_Sprinkler.c:114-121`). B queda sin cable y activo.

**Consecuencia:** estado funcional incorrecto hasta el siguiente tick de bombas; puede seguir el efecto y el riego. El consumidor consulta la actividad del aspersor, no el cable (`N:7622-7630`). El reset periódico está en `N:6136-6147`, con intervalo de 60 s (`D:700`; `N:664-668`). No es corrupción persistente ni un fallo permanente.

**Por qué es nueva:** antes se notificaba y retiraba cada fila del OUT antes del IN (`HEAD:H:607-630`, `:671-680`; secuencia retenida en ronda 1). Al notificar A, B ya no estaba en el store. Agrupar todas las notificaciones antes de eliminar cambia esa propiedad.

**Qué haría:** incluir los efectos bomba/aspersor en el commit/rollback. Las notificaciones del grafo no deben leer un store transitorio como definitivo. Calcular el estado final sobre los cables comprometidos y limpiar todos los extremos retirados. Control ejecutado: con el orden anterior B acaba inactivo; con el nuevo, activo.

### [MEDIO] N-03 — FullSync descarta owners muertos de la cola y abandona sus invalidaciones

**Dónde:** `scripts/5_Mission/LFPG_NetworkManagerImpl.c:943-949`, `:3151-3161`, `:3513-3521`. La única eliminación del mapa nuevo está en `:3231`.

**Escenario:** cortar/reemplazar vanilla durante FullSync guarda posiciones y difiere el owner. Si este se destruye antes del flush, `vObj` nulo evita el broadcast, pero después se limpian ambos arrays de cola. La entrada de `m_VanillaInvalidationPositions` permanece. Repetir con IDs distintos acumula entradas durante la misión.

También ocurre cuando la cola ordinaria descarta un owner nulo (`N:2608-2615`) o se registran posiciones para un owner que no puede publicarse (`N:1431`/`:1534-1535`; `N:2313`/`:2327-2331`). La limpieza del owner vanilla no elimina este mapa (`N:4109`). El helper tampoco limita ni deduplica posiciones.

**Consecuencia:** retención de memoria limitada por la historia de owners desaparecidos, no por cables vivos. El modelo deja 1.000 claves con cola vacía. No he medido bytes, frecuencia real ni agotamiento del heap; no lo llamo crash.

**Qué haría:** unir el ciclo de vida de cola e invalidación: envío, diferimiento y cancelación deben terminar también el registro auxiliar. Resolver la invalidación de un owner desaparecido y liberar sus posiciones.

**Matiz:** «hasta el envío real» en `REPORT.md` es excesivo. El mapa se elimina en N:3231 **antes** del bucle de destinatarios; sin ninguno elegible, se consume sin ejecutar `Send`. Ese orden, por sí solo, no prueba otro bug: no implica un cliente elegible pendiente.

## LO QUE SIGUE BIEN

- **SEC01:** los nueve envíos del manager conservan destinatario explícito (`N:2780`, `:2972`, `:3119`, `:3271`, `:3318`, `:3443`, `:3647`, `:3692`, `:7026`). Los envíos ejecutables de H usan `pid` o `sender`; settings conserva guard/unicast en `H:2051-2068`. F-03 amplía interés sin reintroducir null. El overflow nativo omite la distancia, pero conserva el guard de identidad (`N:2943-2949`); S-01 limita cualquier afirmación sobre «todas las conexiones».
- **SEC02:** conserva la decisión restrictiva firmada. Los scans van antes de la transacción (`H:649-665`) y usan propio/sin reclamar de `WH:230-236`. El manager recibe el mismo creador/política (`H:682-684`) y comprueba antes de mutar (`N:969-979`). El scan IN del handler recorre más stores y normaliza el puerto: puede ser más restrictivo en datos anómalos, pero no demuestra un bypass ni regresión nueva. La política activada permite el reemplazo. No reabro la decisión.
- **SEC20(a):** conserva validación de nombre/dirección (`H:464-538`, `:723-775`; enumeración `I:820-868`). Normaliza el vacío antes de construir la fila (`H:576-581`) y pasa esos valores al request (`H:679-681`). No encuentro un puerto del cliente estándar que esta ronda haya vuelto inválido. Esto no garantiza la atomicidad de SEC20(b), afectada por F-01/N-01.

No sostengo una carrera entre RPC por la mera ausencia de lock de origen: no he encontrado una cesión que pruebe esa intercalación. Tampoco convierto en bug las dos copias del guard de propiedad por existir dos: he comparado entradas, normalización y orden.

## CITAS COMPROBADAS

He abierto todos los destinos de `REPORT.md`, incluidas sus citas abreviadas y la referencia histórica. «SÍ» acredita la afirmación concreta, no el cierre global del hallazgo. Las abreviaturas N/H/E se expanden al principio de este dictamen.

| Cita de REPORT.md | La abrí | ¿Dice lo que decía? |
|---|---|---|
| L14: H:675-686 → N:955-1009 | Sí | **SÍ**: request, llamada, guard y orden de éxito/fallo. |
| L15: N:1093 y N:1071 | Sí, con N:1039-1146 | **SÍ**: unión de conflictos; dedup por owner y referencia; el origen no se recorre de nuevo por el índice. |
| L16: N:1220 | Sí, hasta N:1283 | **PARCIAL**: notifica grafo, retira índice/cuota por fila y después borra filas. El crédito normal está equilibrado; ese orden tiene el efecto N-02. |
| L17: N:1325 y N:997 | Sí | **SÍ**: store primero, grafo después. |
| L18: N:1384, N:1286 y H:720 | Sí | **PARCIAL**: retira el nuevo, restaura filas y solo registra SUCCESS tras OK. No restaura todo el estado previo: F-01/N-01. |
| L22: N:997 vía N:991 original | Sí, incluyendo HEAD | **PARCIAL**: referencia histórica real; el método actual está en N:1618-1635 y marca rebuild al rechazar. El rebuild posterior puede elegir otro cable aunque no publique la fila rechazada: F-01. |
| L23: E:1346-1350 | Sí, incluyendo E:612-620 y E:1311-1362 | **PARCIAL**: rechaza antes de insertar la **arista**, pero después de crear nodos. No acredita ausencia de efectos: N-01. |
| L25: N:1458-1462 | Sí | **SÍ** para commit/generación de cables de WO tras éxito. No generalizable a todo SyncVar de dispositivos: N-02. |
| L31: N:1148 y N:1071-1088 | Sí, con N:1175-1177 | **SÍ**: descuenta solo filas del origen y recoge por OUT o destino. |
| L33: N:1325 | Sí, hasta N:1382 | **SÍ**: vuelve a comprobar los topes en N:1347-1363. |
| L43: N:109, N:929 y N:3217-3222 | Sí, ampliado hasta N:3231 | **PARCIAL**: mapa/helper existen. El rango de «consumo» termina al declarar extras; la unión/eliminación real está en N:3223-3231. |
| L43: N:3271 | Sí, con guard/destinatarios | **SÍ**: envío explícito a pid. |
| L44: N:1431, N:1473 y N:2313 | Sí | **SÍ** en esos tres caminos. No cubren todas las retiradas vanilla. |
| L44: H:991-992 y H:1011-1012 | Sí, con H:1024 | **SÍ**: captura previa al broadcast del corte completo. |
| L45: N:3139-3151 | Sí, hasta N:3161 | **PARCIAL**: el defer sí retorna sin consumir, pero el rango acaba al comenzar la rama. |
| L45: referencia a FlushDeferredBroadcasts, sin línea | Sí, N:3490-3521 | **PARCIAL**: el owner vivo vuelve a entrar al broadcast. Owner muerto: cola vaciada sin limpiar extras, N-03. |
| L45: N:3063-3071 | Sí, con N:3072 | **PARCIAL**: el delta conserva las posiciones eliminadas; el Insert queda justo fuera del rango. La analogía no demuestra «hasta envío real» ni el ciclo de cancelación del mapa nuevo. |
| L59: H:647-665 | Sí | **SÍ**: aborto restrictivo antes de la transacción. |
| L59: H:778 y H:818 | Sí, hasta H:879; WH:230-236 | **SÍ**: política común en H:811, H:852 y H:876, con los matices de normalización descritos. |
| L59: N:971-979 | Sí | **SÍ**: segunda comprobación sin mutación previa. |
| L60: H:464-538 y H:723 | Sí, hasta H:775; I:820-868 | **SÍ**: validación de puertos/direcciones y normalización. |
| L61: N:2780 | Sí, con selección de jugador e identidad | **SÍ**, pid explícito. |
| L61: N:2972 | Sí, con N:2943-2949 | **SÍ**, pid explícito incluso al omitir distancia por overflow. |
| L61: N:3119 | Sí, con guard y radio | **SÍ**, pid explícito. |
| L61: N:3271 | Sí, con N:3237-3261 | **SÍ**, pid explícito y unión de interés. |
| L61: N:3318 | Sí, con entrada/guard del unicast | **SÍ**, pid explícito. |
| L61: N:3443 | Sí, con contexto FullSync | **SÍ**, pid explícito. |
| L61: N:3647 | Sí, con contexto FullSync | **SÍ**, pid explícito. |
| L61: N:3692 | Sí, con contexto FullSync | **SÍ**, pid explícito. |
| L61: N:7026 | Sí, con guard y selección de jugador | **SÍ**, pid explícito. |
| L102: scheduler N:624-689 | Sí, también HEAD y N:629-694 actuales | **PARCIAL**: los diez += son preexistentes; el décimo ahora cae fuera del rango citado. |
| L127: H:668-673 y analogía Sessions() | Sí; N:9-14, facade:54-73 y MissionInit:77, :113 | **SÍ**: cast y aborto cerrado; la factory de misión normal crea la implementación. No encuentro motivo probado para exigir cambio del facade. |
| L8: carpeta del fichero nuevo en config.cpp, sin línea | Sí, config.cpp:215-228 | **SÍ**: carpeta de Mission incluida. No prueba compilación Enforce. |

No encuentro un fichero o método inventado en estas citas. Sí rangos cortos/desplazados y conclusiones más amplias que lo que prueban.

Además, el control narrado en `REPORT.md:36` es incorrecto: `S.output_2→otra_lampara` **sí** entra en el conjunto si la petición usa `output_2`, por coincidencia de origen (`N:1084`). Si además existe `S.output_1→D`, se recogen ambas y el resultado final tiene una fila, no dos. El control negativo de `REPORT.md:39` sí corresponde al comportamiento real. Es un error del informe, no otra regresión del código.

## LO QUE NO PUDE VERIFICAR

- **Compilación y runtime Enforce:** no ejecuté DayZ, AddonBuilder ni builds. Los modelos no sustituyen una ejecución del lenguaje ni dos clientes reales. No afirmo haber observado estos estados en un servidor.
- **S-01/CCTV:** el despacho especial permite actuar sobre CCTV cuando la resolución normal de jugador falla (`H:47`); la entrada deselecciona el cuerpo y selecciona espectador (`H:1294-1296`). La sesión conserva identidad/cuerpo (`scripts/5_Mission/LFPG_ControlSessionRegistry.c:87-112`), pero al salir selecciona el cuerpo y confirma, sin fullsync explícito (`:288-306`). La enumeración/identidad del motor y la reparación al salir siguen sin medirse. El SyncVar de «fullsync enviado» del inicio tampoco demuestra resincronización al salir (`scripts/5_Mission/LFPG_MissionInit.c:331-341`).
- **Find y locales sin ref:** la duda de `REPORT.md:120` requiere compilación/semántica del runtime. No la convierto en defecto confirmado a partir del estilo del código vecino.
- **Coste real:** no medí latencia de scans ni memoria en bytes. N-03 acredita un camino sin liberación; no una tasa de agotamiento ni caída de proceso.
- **Foto completa de ronda 1:** HEAD es anterior a T2. No presento todo lo nuevo frente a HEAD como regresión de ronda 2. La atribución de N-01/N-02 usa mecanismos anteriores leídos y el historial explícito de `REVIEW-CODEX.md` y los dos informes de ronda 1; N-03 pertenece al mapa recién introducido. No existe aquí un commit intermedio con el que compilar dos versiones exactas.

### Comprobaciones ejecutadas

He podido ejecutar el `enfcheck.py` disponible por PowerShell. Su impedimento en la sesión anterior no se reproduce en esta sesión. Resultados sobre el fichero completo:

| Fichero | Comillas impares | Llaves { / } | Paréntesis ( / ) | Escapes adyacentes / ternarios / foreach | ++/-- | +=/-= |
|---|---:|---:|---:|---|---:|---:|
| H | 0 | 332 / 332 | 1607 / 1607 | 0 / 0 / 0 | 1, comentario | 0 |
| N | 0 | 876 / 876 | 3364 / 3364 | 0 / 0 / 0 | 72, comentarios | 10, scheduler previo |
| LFPG_FinishWiringTxn.c | 0 | 3 / 3 | 1 / 1 | 0 / 0 / 0 | 0 | 0 |

Los tres procesos devolvieron 0. El script imprime recuentos y no convierte todos ellos en un fallo de proceso: **exit 0 no equivale a gate de compilación**. El delta -2 del handler en HEAD se explica por los comentarios numerados `// 1)` y `// 2)` eliminados; no por paréntesis ejecutables ausentes.

Los modelos siguientes se ejecutaron con Python en stdin, sin crear fixtures ni cachés. La salida es:

[EXACT — salida observada de los modelos Python, exit 0]

```text
F01_ORIGINAL: G_store=12, H_to_D=restored, rejected_in_snapshot=False
F01_CONTROL: one_free_edge=OK
F01_ORDER[ordered]: restored_old_edge=True, old_edge_after_rebuild=False
F01_ORDER[swap]: restored_old_edge=True, old_edge_after_rebuild=False
N01_NODE: nodes_before=2047, nodes_after=2048, next_handler_preflight=REJECT
N01_CONTROL: previous_rebuild_removes_orphan=True
F02_MODEL: 199936 capacity_cases, false_full=0, false_room=0, double_match_count=1
F02_BOUNDARIES: 9834 cases, settings=1..128, sizes include 63/64/65/127/128/129, mismatches=0
N02_PUMP: disconnected_B_active: R1=False, R2=True; powered_B=False in R2
F03: replace=actor+observer, cut_all=actor+observer, CUT_PORT=actor_only
N03_QUEUE: queued=0, abandoned_invalidation_keys=1000
SEC02_POLICY: own/unclaimed=ALLOW, foreign=DENY, enabled=ALLOW, empty_sender=DENY
MODELS_ONLY: no Enforce VM, no engine RPC, no runtime compilation.
```

## LA PREMISA DE ESTE ENCARGO

**La frontera es correcta como contrato, pero «almacén + aristas» es una implementación incompleta de ella.** El intento rechazado debe preservar las conexiones previas y lo que sus consumidores observan. Mi recomendación anterior era insuficiente si se interpretaba como volver a meter filas y aristas: el orden del store decide un rebuild; una inserción rechazada puede haber creado nodos; y las notificaciones modifican bombas/aspersores antes del commit. Los límites de esa operación deben incluir esos efectos y su restauración, o posponerlos hasta disponer del estado comprometido.

Juntar los dos ficheros en una lane permite resolver el contrato y no es causa de los defectos encontrados. El riesgo aparece al abstraer un lote sobre helpers que tienen efectos laterales: cambia el orden visible por otros consumidores. Dividir de nuevo los ficheros no arreglaría N-02. La revisión necesita seguir las dependencias hacia grafo, dispositivos, renderer y colas, aunque queden fuera del permiso de edición.

Las cinco sospechas estaban bien dirigidas. Dos controles resultan favorables: no encuentro doble descuento por coincidencia OUT/IN ni consumo doble de créditos en el camino normal. Faltaban tres comprobaciones concretas: rollback → orden del próximo rebuild; notificación de retirada → estado de dispositivos; y cada productor de invalidación → envío **o cancelación**. S-01 debe continuar aparte como incertidumbre del motor, sin inventar su resultado para sostener el rechazo.

Persisten dos implementaciones del tope de store, en admisión e inserción, además de los helpers antiguos. Hoy coinciden para el dominio comprobado; es un riesgo de mantenimiento, no otro F-02 demostrado. Tampoco hay evidencia para exigir una ampliación de API del facade como condición de cierre.

Este es el dictamen de la segunda pasada. No inicio una tercera ni decido un bucle «hasta cero»: corresponde al orquestador fijar correcciones y gates siguientes. No he abierto subagentes, corregido scripts ni cambiado formatos persistentes. Los mecanismos y repros quedan documentados aquí como memoria durable, respetando la orden de escribir únicamente este fichero.

### Reproducción offline de los modelos

[EXACT — Python ejecutado sobre este árbol; modelos parciales, no código Enforce]

Ejecutar el bloque desde esta raíz por stdin con `python -X utf8 -B -`. Solo lee fuentes. Las aserciones de cadenas detectan parte del drift; **no son el oráculo**. Los oráculos son conservación del cable previamente admitido, ausencia de nodos creados por un intento rechazado, capacidad del store tras las retiradas, estado del aspersor sin enlace y obligación de drenar/cancelar invalidaciones.

El modelo de grafo representa topología y límites. Omite watchdogs de componentes y simulación eléctrica; los fixtures requieren componentes pequeños y acíclicos, endpoints vivos, geometría válida y cuota disponible. El de bomba aísla los setters de agua y la desactivación eléctrica; no simula riego ni replicación. El de cola toma la rama explícita de owner nulo; no mide cuándo el motor destruye ese objeto. No son gates suficientes para dar VERDE al producto.

```python
from pathlib import Path
from dataclasses import dataclass
from itertools import product
import re

def read(p):
    return Path(p).read_text(encoding="utf-8-sig")
N = read("scripts/5_Mission/LFPG_NetworkManagerImpl.c")
H = read("scripts/5_Mission/LFPG_RPCServerHandlerImpl.c")
E = read("scripts/5_Mission/LFPG_ElecGraphImpl.c")
D = read("scripts/3_Game/LFPG_Defines.c")
def constant(name):
    return int(re.search(r"\b" + name + r"\s*=\s*(\d+)", D).group(1))
EDGE_CAP = constant("LFPG_MAX_EDGES_PER_NODE")
NODE_CAP = constant("LFPG_MAX_NODES_GLOBAL")
WIRE_CAP = constant("LFPG_MAX_WIRES_PER_DEVICE")

# Source anchors are drift guards, NOT the oracle.
assert "FinishTxnAlreadyCollected(removed, ownerId, wd)" in N
assert "remaining = remaining - 1;" in N
assert "live.Insert(wd);" in N
assert "existOut.Count() >= LFPG_MAX_EDGES_PER_NODE" in E
assert "existIn.Count() >= LFPG_MAX_EDGES_PER_NODE" in E
assert "m_NodeCount >= LFPG_MAX_NODES_GLOBAL" in E

@dataclass(frozen=True)
class Wire:
    owner: str
    out: str
    target: str
    inp: str = "input_main"
    creator: str = "A"

def conflicts(w, src, port, dst, inp):
    return (w.owner == src and w.out == port) or (w.target == dst and w.inp == inp)

class GraphModel:
    """Only topology/caps; fixtures are acyclic, small components, live endpoints."""
    def __init__(self, stores):
        self.rebuild(stores)
    def inner_add(self, w):
        self.nodes.update((w.owner, w.target))
        outgoing = [e for e in self.edges if e.owner == w.owner]
        incoming = [e for e in self.edges if e.target == w.target]
        if len(outgoing) >= EDGE_CAP or len(incoming) >= EDGE_CAP or w in self.edges:
            return False
        self.edges.append(w)
        return True
    def add(self, w):
        if len(self.nodes) >= NODE_CAP:
            return False
        return self.inner_add(w)
    def rebuild(self, stores):
        self.nodes, self.edges, self.deferred = set(), [], set()
        for rows in stores.values():
            for w in rows:
                self.inner_add(w)
        self.nodes = {n for w in self.edges for n in (w.owner, w.target)}
    def remove(self, w):
        if w in self.edges:
            self.edges.remove(w)
        self.deferred.update((w.owner, w.target))
    def end(self):
        used = {n for w in self.edges for n in (w.owner, w.target)}
        self.nodes.difference_update(self.deferred - used)
        self.deferred.clear()

def transaction(stores, graph, new, removal_mode="swap"):
    removed = []
    for owner, rows in stores.items():
        for w in rows:
            if conflicts(w, new.owner, new.out, new.target, new.inp):
                removed.append(w)
    for w in removed:
        graph.remove(w)
    for rows in stores.values():
        for i in range(len(rows)-1, -1, -1):
            if rows[i] in removed:
                if removal_mode == "swap":
                    rows[i] = rows[-1]
                    rows.pop()
                else:
                    rows.pop(i)
    stores.setdefault(new.owner, []).append(new)
    if graph.add(new):
        graph.end()
        return "OK"
    stores[new.owner].remove(new)
    for w in removed:
        stores[w.owner].append(w)  # N:1315, m_Index not used.
        graph.add(w)             # N:1321 discards bool.
    graph.end()
    return "DENIED_GRAPH"

# F-01 original: old H->D must survive; rejected new wire must not enter snapshot.
legacy = [Wire("G", f"legacy_{i}", f"L{i}") for i in range(EDGE_CAP)]
old = Wire("H", "output_1", "D")
new = Wire("G", "output_1", "D")
stores = {"G": legacy[:], "H": [old]}
g = GraphModel(stores)
assert transaction(stores, g, new) == "DENIED_GRAPH"
assert stores["G"] == legacy and stores["H"] == [old] and old in g.edges
assert new not in stores["G"]
print("F01_ORIGINAL: G_store=12, H_to_D=restored, rejected_in_snapshot=False")
stores = {"G": legacy[:-1], "H": [old]}
g = GraphModel(stores)
assert transaction(stores, g, new) == "OK"
assert new in g.edges and not stores["H"]
print("F01_CONTROL: one_free_edge=OK")

# Residual F-01: old row was admitted before rollback, not after rebuild.
for mode in ("ordered", "swap"):
    original = Wire("G", "output_1", "T")
    stores = {"G": [original] + legacy}
    for i in range(EDGE_CAP):
        w = Wire(f"J{i}", "output_1", "D", f"legacy_in_{i}")
        stores[w.owner] = [w]
    g = GraphModel(stores)
    assert original in g.edges and legacy[-1] not in g.edges
    before_rows = set(stores["G"])
    assert transaction(stores, g, new, mode) == "DENIED_GRAPH"
    assert original in g.edges and set(stores["G"]) == before_rows
    g.rebuild(stores)
    assert original not in g.edges and legacy[-1] in g.edges
    print(f"F01_ORDER[{mode}]: restored_old_edge=True, old_edge_after_rebuild=False")

# R2 regression: failed AddEdgeInternal creates a node, but no remove queued its cleanup.
stores = {"G": legacy[:]}
# Star=13 nodes; 1017 independent pairs add 2034 => 2047.
for i in range((NODE_CAP-1-(EDGE_CAP+1))//2):
    w = Wire(f"P{i}", "output_1", f"Q{i}", creator="")
    stores[w.owner] = [w]
g = GraphModel(stores)
assert len(g.nodes) == NODE_CAP-1
new_target = Wire("G", "output_1", "fresh_D")
assert transaction(stores, g, new_target) == "DENIED_GRAPH"
assert len(g.nodes) == NODE_CAP and "fresh_D" in g.nodes
assert all(e.target != "fresh_D" for e in g.edges)
print(f"N01_NODE: nodes_before={NODE_CAP-1}, nodes_after={len(g.nodes)}, next_handler_preflight=REJECT")
# R1 called rebuild; even with its rejected row still in the store, orphan prune frees D.
stores["G"].append(new_target)
g.rebuild(stores)
assert len(g.nodes) == NODE_CAP-1
print("N01_CONTROL: previous_rebuild_removes_orphan=True")

# F-02: compare new admission with independently ordered source-cut, destination-cut,
# then actual base-store capacity. Separate row objects; no shared-pointer corruption.
choices = [None, ("output_1","D"), ("output_2","D"), ("output_2","U"), ("","D")]
checked = 0
for size in range(5):
    for values in product(choices, repeat=size):
        for native in (False, True):
            source_key = lambda w: w[0] if native else (w[0] or "output_1")
            indices = {i for i,w in enumerate(values) if w and (w[1]=="D" or source_key(w)=="output_2")}
            remainder = len(values)-len(indices)
            # Independent historical order of both cuts, not the conflict collection.
            post_source = [w for w in values if w is None or source_key(w)!="output_2"]
            post_dest = [w for w in post_source if w is None or w[1]!="D"]
            for setting in range(1,129):
                limit = min(setting,WIRE_CAP) if native else setting
                assert (remainder < limit) == (len(post_dest) < limit)
                checked += 1
assert len({i for i,w in enumerate([("output_2","D")]) if w[1]=="D" or w[0]=="output_2"}) == 1
print(f"F02_MODEL: {checked} capacity_cases, false_full=0, false_room=0, double_match_count=1")

boundary_cases = 0
for native in (False, True):
    for setting in range(1,129):
        for size in sorted({0,1,setting-1,setting,setting+1,63,64,65,127,128,129}):
            for removed_count in sorted({0,min(1,size),min(2,size),size}):
                values = [("output_2",f"old{i}") if i<removed_count else (f"other{i}",f"U{i}") for i in range(size)]
                take = {i for i,w in enumerate(values) if w[0]=="output_2" or w[1]=="D"}
                post = [w for w in values if w[0]!="output_2"]
                post = [w for w in post if w[1]!="D"]
                limit = min(setting,WIRE_CAP) if native else setting
                assert (len(values)-len(take)<limit) == (len(post)<limit)
                boundary_cases += 1
print(f"F02_BOUNDARIES: {boundary_cases} cases, settings=1..128, sizes include 63/64/65/127/128/129, mismatches=0")

# Pump side-effect model from N:5950-6082, powered T2 with two sprinklers.
def pump_run(batched):
    rows = [("output_2","B"), ("output_1","A")]
    states = {x: {"active":True, "has":True, "source":"P", "powered":True} for x in ("A","B")}
    def refresh(removed=""):
        if removed and states[removed]["source"] == "P":
            states[removed].update(active=False, has=False, source="")
        targets = [t for _,t in rows if t != removed]
        for t in targets:
            states[t].update(active=True, has=True, source="P")
    if batched:
        removed = list(rows)  # source-OR-destination matches both in this order.
        for _,t in removed:
            refresh(t)
        rows.clear()
    else:
        for phase in ("source","dest"):
            for i in range(len(rows)-1,-1,-1):
                p,t = rows[i]
                if (phase=="source" and p=="output_2") or (phase=="dest" and t=="A"):
                    refresh(t)
                    rows.pop(i)
    rows.append(("output_2","A"))
    refresh()
    states["B"]["powered"] = False  # EndGraphMutation -> sprinkler SetPowered only.
    return states
old_state, new_state = pump_run(False), pump_run(True)
assert not old_state["B"]["active"] and new_state["B"]["active"]
assert new_state["B"]["source"] == "P" and not new_state["B"]["powered"]
print("N02_PUMP: disconnected_B_active: R1=False, R2=True; powered_B=False in R2")

# F-03: independent recipient-set invariant; known observer must get invalidation.
radius = constant("LFPG_CULL_DISTANCE_M")+20
players = {"actor":0, "observer":72, "far":1000}
def recipients(current, extra):
    return {name for name,x in players.items() if abs(x)<=radius or any(abs(x-t)<=radius for t in current+extra)}
assert recipients([-10],[60]) == {"actor","observer"}
assert recipients([],[60]) == {"actor","observer"}
assert recipients([],[]) == {"actor"}
print("F03: replace=actor+observer, cut_all=actor+observer, CUT_PORT=actor_only")

# FullSync queue invariant: every pending entry must drain or be explicitly cancelled.
pending, extras = [], {}
for i in range(1000):
    key = f"deleted_{i}"
    extras[key] = [60]
    pending.append((key,None))  # owner destroyed before flush
for key,obj in pending:
    if obj is not None:
        extras.pop(key,None)
pending.clear()
assert len(extras)==1000 and not pending
print("N03_QUEUE: queued=0, abandoned_invalidation_keys=1000")

# SEC02 policy control, matching WH:230-236.
def can_cut(creator, sender, allow):
    return bool(sender) and (allow or creator in ("",sender))
assert can_cut("A","A",False) and can_cut("","A",False)
assert not can_cut("B","A",False) and can_cut("B","A",True)
assert not can_cut("A","",False)
print("SEC02_POLICY: own/unclaimed=ALLOW, foreign=DENY, enabled=ALLOW, empty_sender=DENY")
print("MODELS_ONLY: no Enforce VM, no engine RPC, no runtime compilation.")

```

