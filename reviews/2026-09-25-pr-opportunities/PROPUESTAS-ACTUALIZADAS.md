# Propuestas de rendimiento y reducción de código — 2026-09-25

Estado: **documentación para revisión; no implementa las propuestas pendientes**. Objetivo: reducir trabajo repetido y código total manteniendo todas las funcionalidades.

## Base verificada y actualización de GitHub

La fuente de autoridad es el árbol local. Al preparar esta publicación, el árbol compilable estaba en `022d0a4`, sin cambios rastreados pendientes, y GitHub contenía seis commits posteriores. Se verificó la relación de ancestro y se avanzó la copia local únicamente por fast-forward. Ambas bases quedaron en [`11c563e`](https://github.com/willy92wins/LFPowerGrid/commit/11c563ee3055f4830f9a6224adeecbe51d7f37fc). El PBO local y su firma conservaron sus hashes SHA256.

Este documento consolida las dos pasadas de revisión local y sustituye su lista de pendientes para esta base. Las referencias de código son enlaces al commit exacto; las cifras de ahorro son estimaciones salvo indicación expresa. No se han medido CPU, FPS ni memoria compilada en el juego.

Verificación de publicación:

- `python -B .github/tools/test_enforce_checks.py`: **21 tests, OK**.
- `python -B .github/tools/enforce_checks.py --root .`: **256 archivos de texto, 150 Enforce; FAIL=0, WARN=0**.
- Estos controles verifican reglas estáticas. No equivalen al linter completo del Knowledge Pack ni a compilar/cargar cliente y servidor.
- La nueva PR modifica únicamente este documento. La implementación, el despliegue y las pruebas in-game quedan para encargos posteriores.

## Propuestas ya integradas

| ID | Trabajo existente en main | PR |
|---|---|---|
| P0 | Restauración del guard de Hologram | [#35](https://github.com/willy92wins/LFPowerGrid/pull/35) |
| P0, checker | Detección de directivas desequilibradas y controles negativos | [#39](https://github.com/willy92wins/LFPowerGrid/pull/39) |
| B11 | Combinación común de snapshots pendientes/diferidos | [#40](https://github.com/willy92wins/LFPowerGrid/pull/40) |
| B1 | Nueve bloques imposibles y campos de diagnóstico sin consumidores restantes | [#41](https://github.com/willy92wins/LFPowerGrid/pull/41) |
| B9 | Helper de posiciones de puertos, incluidos los cuatro interruptores | [#42](https://github.com/willy92wins/LFPowerGrid/pull/42) |
| B7 | Emisor común de mutaciones BTC del cliente | [#43](https://github.com/willy92wins/LFPowerGrid/pull/43) |
| B4 | Base compartida de las lámparas | [#44](https://github.com/willy92wins/LFPowerGrid/pull/44) |

Estas mejoras se excluyen del ahorro futuro. Su presencia en main no demuestra por sí sola paridad in-game. Las reducciones del grafo, RPC y viewport de #36–#38 también forman parte de la base.

## Cola propuesta

Los identificadores A/B son referencias de esta revisión, no números de PR de GitHub. Todas las modificaciones siguientes son **[DESIGN]**.

| Orden | ID | Unidad de trabajo | Beneficio esperado / condición |
|---|---|---|---|
| 1 | A4 | Un RPC serializado por broadcast | P serializaciones idénticas → una; mismos P envíos |
| 2 | A3 | Censo único de inventario por respuesta BTC | C+1 recorridos → uno, conservando cantidades y frescura |
| 3 | A1 | Consulta híbrida de celdas de jugadores | Examinar el menor conjunto de celdas; requiere benchmark |
| 4 | B3 | Comportamiento común de bombas T1/T2 | 70–130 líneas netas estimadas |
| 5 | B8 | Interacción común de ventanas ATM/sorter | 140–210 líneas netas estimadas |
| 6 | B2 | Estado común de cuatro interruptores | Recalcular ahorro: B9 ya extrajo sus puertos |
| 7 | B10 | Base de tres acciones ToggleSwitch | 65–105 líneas netas estimadas; después de B2 |
| 8 | A2 | Evitar snapshot/pasada final sin demanda blanda | Menos trabajo de asignación en el grafo |
| 9 | B6 | Prefijo común de seis handlers BTC | 100–160 líneas netas estimadas; revisión específica de sesión/replay |
| 10 | A5 | Lista de jugadores por lote síncrono de broadcasts | B llamadas GetPlayers → una; después de A4 y de medir B |
| Secundaria | B5 | Base de config para nueve kits de caja | 50–65 declaraciones netas estimadas; validar config efectivo |

Las estimaciones de B3/B6/B8/B10 incluyen helpers y wrappers de producción, y excluyen compactar sentencias o borrar comentarios. Cada PR deberá informar también sus pruebas nuevas y el delta total del repositorio. No se publica un total acumulado: hay dependencias, A puede añadir código y las estimaciones originales incluían trabajo ya integrado.

## A4 — Serializar una vez cada broadcast

**Mecanismo observado.** Se construye y rellena un ScriptRPC dentro del bucle de destinatarios en cuatro rutas: [snapshot de owner](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_NetworkManagerImpl.c#L1632), [snapshot capturado](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_NetworkManagerImpl.c#L1749), [delta](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_NetworkManagerImpl.c#L1866) y [snapshot vanilla](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_NetworkManagerImpl.c#L1962). Los campos escritos son comunes a los destinatarios de cada mensaje; target e identidad se suministran en Send. El JSON del delta ya se prepara antes del bucle y no se cuenta como ahorro nuevo.

**Cambio [DESIGN].** Construir el mensaje una vez, preferiblemente al encontrar el primer destinatario válido. Mantener cada Send y su identidad explícita. El RPC vive solo durante ese broadcast, sin Reset ni nuevas escrituras entre envíos.

**Contrato comprobado.** En el vanilla local, `scripts/3_game/gameplay.c:104–117` define ScriptRPC, Reset y Send; la documentación de Send establece que conserva los datos escritos para envíos sucesivos hasta Reset. Antes de implementar se revalidará ese contrato contra la versión objetivo. Recipient nulo ampliaría la audiencia y está fuera de esta propuesta.

**Ganancia acotada.** Un delta de E entradas escribe 6+2E campos por destinatario. Con P=20 y E=16: 760 llamadas Write → 38, y 20 construcciones de ScriptRPC → una. Continúan siendo 20 envíos del mismo tamaño: no implica ahorrar el 95% de CPU global ni ancho de banda.

**Aceptación.** Un consumidor independiente debe observar iguales tipos, valores, orden, canal, target, identidad y guaranteed. Cubrir cero/un/muchos receptores, desconexión tras selección, mensajes consecutivos distintos, 64 entradas, fallback, owner borrado y JIP/full sync. Medir construcciones, escrituras y tiempo real de serialización.

## A3 — Censo de inventario BTC por respuesta

**Mecanismo observado.** [LFPG_CollectMatching](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_BTCHelper.c#L412) recorre manos, attachments y cargo; [CountPlayerItems](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_BTCHelper.c#L453) crea listas para contar. [CountPlayerCash](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_BTCHelper.c#L827) repite el recorrido por moneda distinta; las [respuestas terminales](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_BTCHelper.c#L354), el [rechazo de nonce](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_BTCHelper.c#L369) y la [apertura](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_BTCHelper.c#L966) añaden otro recorrido para BTC.

**Cambio [DESIGN].** Un censo efímero de cantidades por classname para construir esa respuesta; aplicar después el catálogo en su orden actual. Con N visitas y C monedas, pasar de aproximadamente (C+1)N visitas a N más las consultas del catálogo. Con seis monedas: siete recorridos → uno; no se presupone rendimiento x7 ni acceso O(1) al mapa nativo.

**Invariantes.** Mantener [LFPG_GetEffectiveQty](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_BTCHelper.c#L380), truncado y mínimo uno, igualdad exacta de classname, primera entrada por moneda duplicada, orden de suma, catálogo inválido y multiplicidad del recorrido. Un censo posterior a consumo/spawn debe capturarse después de esa mutación. El replay devuelve sus conteos terminales guardados.

**Fuera de alcance.** [PreparePlayerCash](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_BTCHelper.c#L856): su orden decide qué stacks se consumen. Tampoco cambian débito, entrega, stock ni rollback.

**Aceptación.** Inventarios vacíos/anidados, varias pilas, cantidades cero/fraccionarias, catálogo duplicado/inválido, BTC como moneda y consumo parcial. Comparar inventario real, respuestas, saldo y replay con la referencia. Medir visitas, asignaciones y latencia.

## A1 — Consulta híbrida del índice de jugadores

**Mecanismo observado.** [LFPG_CollectPlayerCandidates](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_NetworkManagerImpl.c#L5479) calcula un rectángulo, pero recorre todas las celdas ocupadas. [LFPG_HasPlayerCellNear](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_NetworkManagerImpl.c#L5518) materializa también la lista completa para contestar una pregunta de existencia.

**Cambio [DESIGN].** Elegir entre recorrer celdas ocupadas o buscar coordenadas del rectángulo según cuál sea menor. Permitir salida temprana para existencia, preservando la semántica de candidatos y el orden que requieran consumidores. Existe un antecedente local en `lane/nm2a` (`424dceb`): revisar y portar selectivamente; no fusionar esa rama completa ni contabilizarla como trabajo nuevo.

**Ganancia acotada.** Para C celdas ocupadas y A celdas del rectángulo, examinar min(C,A) celdas más el coste de las búsquedas. Verificar representación y límites de coordenadas antes de elegir una clave.

**Aceptación.** Comparar identidades, no solo conteos, con una consulta lineal independiente: bordes, coordenadas negativas/fuera de rango, vacíos, bajas/desconexiones y jugadores dispersos/agrupados. Verificar disparos y latencias de láser, alfombra, sensor y aspersor. Medir consultas, celdas examinadas y p50/p95/p99 con igual escena y cadencias.

## B3 — Implementación común de bombas T1/T2

**Mecanismo observado.** [LFPG_DegradeFilter en T1](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/4_World/LFPG_WaterPump.c#L272) y [en T2](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/4_World/LFPG_WaterPump.c#L653) repiten la misma lógica. Alimentación, sonido y limpieza comparten comportamiento, pero T2 añade tanque, tres salidas y campos propios.

**Cambio [DESIGN].** Compartir solo el comportamiento equivalente, conservando ambas clases y sus API para acciones y PumpHelper. Mantener el tanque en T2 y la actualización del enlace del aspersor. Los bloques de diagnóstico retirados en B1 no forman parte del ahorro.

**Aceptación.** Abastecimiento, tres salidas T2, límite/tipo de líquido, llenado/vaciado, filtro ausente/agotado, upgrade/desmontaje, sonido al perder alimentación, save/load y JIP. Mismos campos, orden y límites de SyncVars y persistencia. Estimación: 70–130 líneas netas; confirmar con el diff.

## B8 — Interacción común de ventanas ATM/sorter

**Mecanismo observado.** [ATM](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/4_World/LFPG_BTCAtmView.c#L707) y [sorter](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/4_World/test/LFPG_SorterView_TEST.c#L950) repiten manejo de ratón, cabecera, drag, geometría, hover y ESC. Los 24 métodos seleccionados sumaban 501 líneas sin comentarios/vacías; estos dos archivos no cambiaron entre la revisión original y la base actual.

**Cambio [DESIGN].** Helper/componente de interacción con estado independiente por ventana, conservando vistas, wrappers y campos enlazados por Dabs. Mantener OnClick y el protocolo en cada vista. Evitar asignaciones por frame y reflexión que sustituya referencias tipadas.

**Diferencias que se conservan.** Sorter trata ScrollWidget como interactivo; ATM no. Botones/EditBox dejan pasar los eventos necesarios para OnClick. ESC primero quita el foco del editor, luego cierra, con cooldown de 0,2 s por vista. Hover restaura el color de cada widget.

**Aceptación.** Reabrir/recrear/rebind, ESC con/sin foco, clicks sin doble dispatch, scroll/drag, cursor y controles, 720p/1080p/1440p y DPI. Reconciliar widgets y textos. Sin rediseño visual ni eliminación de `test/`, que contiene la UI productiva. Estimación: 140–210 líneas netas.

## B2 — Estado común de cuatro interruptores

**Mecanismo observado.** [PushButton](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/4_World/LFPG_PushButton.c#L41), [SwitchRemote](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/4_World/LFPG_SwitchRemote.c#L51), [SwitchV2](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/4_World/LFPG_SwitchV2.c#L57) y [SwitchV2Remote](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/4_World/LFPG_SwitchV2Remote.c#L32) repiten estado eléctrico, setters y lifecycle. Las posiciones de puertos ya delegan al helper integrado en #42.

**Cambio [DESIGN].** Compartir estado/lifecycle compatibles, manteniendo la distinción entre pulso y enclavamiento, clases públicas, acciones, mensajes y materiales. No volver a extraer ni contabilizar los puertos. El rango anterior de 200–350 líneas mezclaba ese trabajo: queda retirado hasta medir un diff sobre esta base.

**Invariantes.** Mismo orden de registro de m_PoweredNet, m_SwitchOn y m_Overloaded; pulso de [2.000 ms](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/3_Game/LFPG_Defines.c#L380); momentáneos sin persistir m_SwitchOn y variantes V2 con su serialización existente. No heredar accidentalmente acciones de otra variante.

**Aceptación.** Matriz por dispositivo: ON/OFF, RF, pérdida de alimentación durante pulso, daño, borrado, corte, reconexión/JIP y save/load ON/OFF. Comparar campos persistidos y SyncVars en orden. Incluir nuevas bases/wrappers en el recuento y medir huella de los tres módulos; ninguna asignación nueva por tick.

## B10 — Base común de tres acciones ToggleSwitch

**Mecanismo observado.** [Remote](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/4_World/LFPG_ActionToggleSwitchRemote.c#L15), [V2](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/4_World/LFPG_ActionToggleSwitchV2.c#L16) y [V2Remote](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/4_World/LFPG_ActionToggleSwitchV2Remote.c#L16) repiten componentes, condición de target/distancia, texto y ejecución. Estos archivos no cambiaron desde el censo de 204 líneas sin comentarios/vacías.

**Cambio [DESIGN].** Después de B2, base de acción contra el contrato común del dispositivo; conservar tres hojas, clases admitidas y mensajes. Cada Cast debe seguir aceptando sus subtipos: no sustituirlo por igualdad de GetType. Compartir una base no debe permitir operar la acción de una familia sobre otra.

**Aceptación.** Targets correctos e incorrectos cruzados, subclases, distancias dentro/fuera y desaparición entre condición/ejecución. Mismos textos, posturas, super, mensajes y controles cliente/servidor. Conservar el pulso real de SwitchRemote. Estimación: 65–105 líneas netas, separadas del ahorro de dispositivos de B2.

## A2 — Comparar asignaciones sin snapshot cuando no hay demanda blanda

**Mecanismo observado.** [La asignación](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_ElecGraphImpl.c#L4004) llena siempre m_PreviousAllocations. La distribución de carga blanda se omite cuando no procede, pero la [pasada final](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_ElecGraphImpl.c#L4101) vuelve a recorrer salidas para detectar cambios.

**Cambio [DESIGN].** Solo con totalSoftDemand <= LFPG_PROPAGATION_EPSILON, comparar viejo/nuevo al asignar, evitar el snapshot y saltar la pasada final. Mantener el camino con carga blanda y el orden de operaciones aritméticas. La comparación se hace contra la asignación final, sin pulsos transitorios.

**Ganancia acotada.** Evitar D inserciones y hasta D visitas posteriores para D salidas; la pasada actual sale al primer cambio. No equivale a reducir un tercio del coste del grafo.

**Aceptación.** Oráculo independiente de potencia en cadenas, bifurcaciones y combinadores: cero, sobrecarga, compuerta cerrada, salidas deshabilitadas, primera convergencia y valores a ambos lados de epsilon. Conservar m_AllocChanged, carga de baterías y publicación de demanda cero. La contabilidad de aristas debe reflejar el trabajo real y respetar presupuestos; medir tiempo y convergencia.

## B6 — Lectura y control inicial común en handlers BTC

**Mecanismo observado.** Los prefijos de [Buy](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_BTCHelper.c#L1000), [Sell](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_BTCHelper.c#L1419), [Withdraw BTC](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_BTCHelper.c#L1862), [Deposit BTC](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_BTCHelper.c#L2022), [Withdraw cash](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_BTCHelper.c#L2202) y [Deposit cash](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_BTCHelper.c#L2377) repiten lectura y resolución NEW/REPLAY/IN_FLIGHT/INVALID. Suman 267 líneas sin comentarios/vacías; BTCHelper y SessionRegistry no cambiaron desde ese censo.

**Cambio [DESIGN].** Petición efímera y helpers para lectura/control inicial, con subId, txType y forma de payload explícitos. ReserveRequest conserva su punto actual antes de la mutación propia de cada operación. No unificar transacciones completas.

**Diferencias críticas.** Buy inicializa useAccount=true; Sell inicializa toAccount=false y conserva requestedToAccount original en su huella aunque cambie el destino efectivo. Las otras cuatro formas no llevan bool. Preservar SERVER en cash y el comportamiento de lecturas parciales: encadenar Read con cortocircuito alteraría campos leídos. [CheckRequest](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_BTCSessionRegistry.c#L271) puede sellar un in-flight huérfano; no es una consulta pura que se pueda mover libremente.

**Aceptación.** Truncado en cada campo, tipo inválido, sender ausente, nonce nuevo/replay/en vuelo/conflictivo/stale, sesión vieja, forward-gap, agotamiento y rate-limit. Comparar orden de CheckRequest, AllowReplayResponse, ReserveRequest, CompleteRequest y respuesta. Sin cambiar precios, stock, proveedor, persistencia, devolución o recuperación. Estimación: 100–160 líneas netas; requiere revisión específica de sesión/replay.

## A5 — Lista de jugadores por lote síncrono

**Mecanismo observado.** [FlushBroadcasts](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_NetworkManagerImpl.c#L1518) recorre tres colas. [SelectWireBroadcastRecipients](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_NetworkManagerImpl.c#L1562) y [BroadcastOwnerSnapshot](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_NetworkManagerImpl.c#L1727) vuelven a llamar GetPlayers por mensaje. Los arrays ya se reutilizan: se propone evitar repoblar la lista base.

**Cambio [DESIGN].** Contexto local al lote con lista base; cada selección conserva su lista separada porque Remove modifica la actual. Fuera del lote, obtener jugadores frescos. No introducir caché global ni reutilizar la lista de detección con otra cadencia.

**Aceptación.** Misma lista ordenada de receptores por mensaje y mismo [CanReceiveWireBroadcast](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/scripts/5_Mission/LFPG_NetworkManagerImpl.c#L1550): full sync pendiente/activo, replay exclusivo, identidad, owner borrado, cut-all, JIP y desconexión. Mantener interés por owner y targets y broadcastAll; no retener el contexto al acabar.

**Prioridad condicionada.** Medir cuántos mensajes B se agrupan. El beneficio es B llamadas GetPlayers → una; permanecen filtros y envíos. Ejecutar después de A4 para aislar efectos.

## B5 — Base de config para kits de caja, opcional

**Mecanismo observado.** Nueve kits comparten modelo lf_kit_box.p3d y propiedades; ejemplos en [config.cpp, primer kit](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/config.cpp#L381) y [segundo kit](https://github.com/willy92wins/LFPowerGrid/blob/11c563ee3055f4830f9a6224adeecbe51d7f37fc/config.cpp#L440). Pesos y tamaños no son iguales en todos.

**Cambio [DESIGN].** Base interna scope=0 y hojas existentes scope=2 con sus diferencias. Conservar classnames, units[], modelo, pesos, tamaños, arrays de acciones y valores efectivos. Estimación: 50–65 declaraciones netas contando la base.

**Aceptación.** Comparar config resuelto por el engine, colocación, saves existentes y subclases de otros mods. Retorno menor: dejar detrás de las propuestas principales.

## Condiciones comunes para aprobar cada implementación

- Una finalidad acotada por PR. Cada nuevo diff parte del main vigente y revalida estos enlaces.
- Conservar funcionalidades, classnames, acciones, puertos, permisos, formatos persistidos, orden RPC, defaults y SyncVars. No retirar UI productiva ni desplazar código entre módulos para simular un recorte.
- Contar fuente de todos los archivos añadidos y retirados. Separar código, comentarios, blancos y declaraciones; informar también pruebas nuevas. No contar compactación de sentencias.
- Linter completo del proyecto sobre base y candidato, delta de errores y comprobación de campos declarados. Para UI, reconciliar widgets/textos. El checker estático no sustituye compilación cliente/servidor.
- Pruebas del consumidor o de un invariante independiente; positivos y negativos pertinentes. Save/load y JIP cuando cambie herencia, estado o lifecycle.
- Rendimiento: misma escena, dispositivos, jugadores, cables, cadencias y artefacto identificado por hash; medidas repetidas. PASS exige mejora y paridad; INCONCLUSO si no se mide; FAIL ante regresión.
- Reducción de líneas no demuestra reducción proporcional de memoria compilada ni de PBO. Medir los tres módulos cuando se reclame menor huella total.

## Límites y propuestas descartadas

El renderer ya usa heap sort y el sorter ya conserva una caché de widgets: no se proponen como trabajo nuevo. No se incluye unificación completa de operaciones BTC ni cambiar a generation-only la detección de wires mutables. Tampoco se propone quitar features, bajar cadencias o esconder código en otro módulo/PBO.

El antecedente local `lane/pr4-estado` conserva valor como referencia, pero su delta se calculó contra otra base; no se suma a este documento ni se propone fusionarlo íntegramente. Los informes iniciales locales conservan su snapshot histórico. Esta publicación ofrece la cola actualizada y sus condiciones de aceptación.
