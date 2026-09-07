## 1. CRÍTICA DEL ENCUADRE

Tres problemas con el encuadre, dos menores y uno que cambia la pregunta.

**(a) El objetivo declarado y la pregunta no casan.** El §3 fija el objetivo del proyecto en «jubilar la V3 para dejar más hueco en el mod» y aclara que «hueco» = arena de script, no bytes en disco. Pero H6 demuestra que borrar fuente **no** libera arena: solo cuenta la estructura compilada (clases). El propio borrador, en §B, acepta la consecuencia («jubilar la V3 vale ~21 kB de arena, no 143 kB») y, en §D, cambia silenciosamente de justificación: T0 ya no va primero por la huella, sino porque el fork es un «impuesto recurrente». Es decir, el orquestador **refutó el encuadre del objetivo sin decirlo**: la pregunta «¿V3 antes o después de integridad?» hereda un objetivo (hueco) que el propio borrador abandonó. Si el objetivo real sigue siendo arena, la pregunta correcta no es de orden, sino de inventario: **¿qué clases se pueden borrar?** (las 7 de V3 son una de varias candidatas; ver §5).

**(b) «0 CORREGIDAS» es casi exacto pero esconde un matiz.** El triage (§4) reporta 24 VIVO + 1 PARCIAL. La PARCIAL es SEC09: «arreglado en `ef29b73`; quedan dos vías» (`LFPG_FileUtil.c:580-585` guarda nueva, `:214-218` residuo, `:280-283` último recurso). El encabezado «Cero corregidas» es correcto en el sentido estricto (ninguna fully fixed), pero la fila SEC09 matiza: hay **un** P1 de integridad con un fix parcial ya en HEAD. Eso importa para el orden, porque sitúa a SEC09 en distinto estado que E04/E16 (que están VIVO intactos). No rompe el encuadre; lo afina.

**(c) El pre-filtro no tiene el falso negativo que el §1 me invita a buscar, pero tiene un límite que el plan ignora.** El método (§4) cruza ficheros citados por la ficha contra `git diff --name-only --ignore-all-space d61705e..HEAD`. Un fichero intacto es evidencia fuerte de ficha viva; un fichero tocado **no** es evidencia de arreglo (advertencia H2). Eso está bien y es conservador. El límite real es otro: el triage cubre **solo las 25 P1**. Las 126 P2/P3 siguen sin triar una a una (§4, «Lo que el triage NO cubre»). El borrador las trata como clases, no como fichas (§E). Eso es legítimo para un plan de ataque, pero significa que el plan descansa sobre 25 fichas verificadas y 126 asumidas-vivas-por-construcción o desconocidas. No es un fallo del encuadre; es una franja de incertidumbre que debe ir a §6.

Acepto H1–H7 como hechos (sus mediciones están citadas) y acepto la inferencia principal del triage: los P1 de integridad siguen vivos en HEAD. Lo que rechazo es la **premisa implícita de que «jubilar V3» es el objetivo operacional**: H6 la vacía, y el propio orquestador la sustituyó por «quitar el impuesto del fork» sin reescribir la pregunta.

## 2. RESPUESTA A LA PREGUNTA

**Ni lo uno ni lo otro en su forma pura: T0a (extracción de paleta) antes; T1 (integridad) inmediatamente después; T0b (borrado de V3) después de T1.** La jubilación completa de V3 va **después** de los P1 de integridad, no antes; pero la pieza mecánica que desbloquea ambas (T0a) va antes que ambas.

Por qué:

- El argumento del orquestador para poner T0 primero es el «impuesto recurrente»: mientras vivan las dos UIs, cada arreglo del sorter se escribe dos veces. **Ese argumento no aplica a T1.** T1 = E04, E16, SEC09 residual, E02, E03, E08. Todos viven en el subsistema BTC/economía (`LFPG_BTCHelper.c`, `LFPG_BalanceProvider_NativeImpl.c`, `LFPG_BTCAtm.c`, `LFPG_FileUtil.c`), no en la UI del sorter. Retirar V3 no ahorra trabajo en T1.
- El argumento para poner T1 primero es H7 (sin despliegue público, la pérdida monetaria es latente). Es válido como orden relativo entre T1 y T2/T3/T4, **pero no exime de hacerlo cuanto antes**: E04 (`LFPG_BTCHelper.c:1465`,`:1505-1513`) destruye BTC antes de saber si el crédito persiste; es la única ficha del plan con daño irreversible por tick. Latente no significa inexistente; significa que no hay prisa de servidor en producción, no que se pueda posponer indefinidamente detrás de un refactor grande.
- El argumento para **no** poner T0b antes que T1 es el censo del §5/F: T0 y T1 tocan el **mismo subsistema** (el ATM). El orquestador ya lo vio y por eso inventó T0a. Pero T0a desacopla el ATM de la paleta, **no** termina la jubilación. La jubilación real (T0b) sigue siendo grande: 106 referencias fuera de V3 (§5 contraverificación), de las cuales 76 son el ATM que T1 también toca. Hacer T0b antes que T1 es editar el ATM dos veces seguidas —la segunda sobre una refactorización recién hecha y sin cobertura (A03 dice que la cobertura actual valida el checker, no la lógica; §B.4, ficha A03)—.
- El argumento para **no** esperar a T0b para empezar es que T0b no arregla los bugs del sorter que el orquestador usa como motivación: S08 (`LFPG_SorterLogic.c:1105-1327`, `RPCServerHandlerImpl.c:147-150`) se hereda íntegro en V4. Jubilar V3 **arrastra** S04/S08, no los resuelve (advertencia final del §6). Es decir, el «impuesto recurrente» se cobra sobre todo en T4 (S04/S08), no en T1. Mover T0b antes de T1 retrasa integridad sin comprar casi nada a cambio.

Resumen: la única parte de «V3 antes» que sostiene es T0a, porque es prerrequisito de ambas. El borrado (T0b) va después de T1.

## 3. PLAN

Tramos. Cada uno: qué entra, por qué ahí, qué lo bloquea, señal observable de fin.

**T0a — Extracción pura de paleta (primero, aislado).**
- Entra: sacar `LFPG_ColorData` (definida en `LFPG_SorterView.c:45`) y las 29 `COL_*` declaradas allí a un fichero neutro (p. ej. `3_Game/LFPG_UIPalette.c`); repuntar los 76 usos del ATM (`LFPG_BTCAtmView.c` 64 `COL_*` + 5 `LFPG_ColorData`; `LFPG_BTCAtmController.c` 12 `COL_*`) y los 13 de V4 (`LFPG_SorterView_TEST.c`). Cero cambio de comportamiento.
- Por qué ahí: es prerrequisito de T0b (no se puede borrar `LFPG_SorterView.c` si el ATM la importa) y desacopla el ATM para que T1 no herede conflicto. Es lo único del plan que es verdaderamente mecánico y revisable por diff.
- Bloquea: nada. Es el primer paso.
- Señal de fin: compila, el ATM abre y muestra los mismos colores que antes, `grep -r 'LFPG_SorterView\.COL_'` devuelve 0 fuera de `LFPG_SorterView.c` (que aún vive), y `grep -r 'LFPG_ColorData'` solo aparece en el nuevo fichero y en los `SetUserData` del ATM/V4.

**T1 — Integridad monetaria (después de T0a).**
- Entra: E04 (`LFPG_BTCHelper.c:1465`,`:1505-1513` — restituir BTC si el crédito falla), E16 (`LFPG_BalanceProvider_NativeImpl.c:1230-1243`; `LFPG_BTCAtm.c:165-176` — no borrar tombstone hasta confirmar hive), SEC09 residual (`LFPG_FileUtil.c:580-585` guarda nueva, `:214-218` residuo, `:280-283` último recurso — cerrar las dos vías restantes), E02 (`LFPG_BTCHelper.c` — no crear entidades antes de comprober pago), E03 (límites de entidades por tx), E08 (denominaciones duplicadas/solapadas).
- Por qué ahí: es el único tramo con daño irreversible de valor de jugador; T0a ya desacopló el ATM, así que T1 toca el ATM sin pisar refactorización pendiente; no sufre el «impuesto del fork» (no es sorter UI).
- Bloquea: T0a merged. No bloquea a T0b (pueden coexistir en ramas distintas una vez T0a está en tronco).
- Señal de fin: matriz de fallos por ficha (inyectar fallo de persistencia tras destruir BTC, reiniciar, comprobar que el saldo y los BTC coinciden); los 6 fichas pasan de VIVO a CORREGIDO en un re-triage con el mismo método del §4.

**T0b — Jubilación de V3 (paralelo a T1 desde el día 1, merge después de T1).**
- Entra: paridad `powered/linked` de `LFPG_ActionOpenSorterPanel_TEST` (hoy no chequea `LFPG_IsPowered`/`LFPG_IsLinked` según auditoría A §6, ficha P1-1); retocar `LFPG_MissionInit.c` en los 9 sitios del censo (`:148`,`:176`,`:180`,`:227`,`:229`,`:250`,`:268`,`:275`,`:473`); retocar `LFPG_RPCClientHandler.c` en los 4 handlers V3 (`:568`,`:580`,`:592`,`:729`); añadir `LFPG_SorterView_TEST.IsOpen()` en `LFPG_Actions.c:528` y `LFPG_ActionSyncSorter.c:69` (hoy solo miran V3); crear emisor de `SORTER_TEST_RESYNC (65)` (hoy solo receptor `RPCServerHandlerImpl.c:157-160`, guard `RPCGuard.c:66`, ACK `RPCClientHandler.c:79-81`; el único `Write` de resync es V3 `ActionSyncSorter.c:101`→`SORTER_RESYNC` 29); crear productor de `SORTER_TEST_CARGO_REFRESH (70)` (hoy `BroadcastCargoRefreshToNearby` fija `refreshSubId = SORTER_CARGO_REFRESH` 34 en `NetworkManagerImpl.c:6316`); borrar las 7 clases V3 (`LFPG_SorterView`, `LFPG_SorterController`, `LFPG_SorterTagView`, `LFPG_SorterPreviewRow`, `LFPG_ColorData` ya movida, + acciones/entidad según decisión de producto); renombrar `_TEST` → nombres definitivos; retirar los `.layout` V3 (`gui/layouts/LFPG_Sorter.layout`, `LFPG_SorterTag.layout`, `LFPG_SorterPreviewRow.layout`).
- Por qué ahí: es el trabajo grande que quita el impuesto del fork para T4; se hace en paralelo a T1 (no tocan los mismos ficheros una vez T0a aisló la paleta) pero se mergea después, para no editar el ATM dos veces.
- Bloquea: T0a. Decisión de producto sobre qué hace `LFPG_ActionOpenSorterPanel`/entidad `LFPG_Sorter` sin la UI V3 (el censo marca esa fila como «sí mientras viva la entidad»).
- Señal de fin: `grep -r 'LFPG_SorterView[^_]'` devuelve 0 fuera de `reviews/`; compila sin la entidad V3 (o con la decisión documentada); `SORTER_RESYNC (29)` ya no se emite; `SORTER_TEST_RESYNC (65)` y `SORTER_TEST_CARGO_REFRESH (70)` tienen emisor; el dual-open mutex de `LFPG_SorterView_TEST.c:1329` desaparece; la acción TEST exige `powered/linked` o lo marca `// TEST-intencional`.

**T2 — Autoridad de servidor.**
- Entra: SEC02 (`LFPG_RPCServerHandlerImpl.c:602`,`:680` — `RemoveWiresTargeting` con `allowOthers=true` por defecto; el camino de corte sí aplica política, asimetría confirmada), SEC20 (`LFPG_RPCServerHandlerImpl.c:283`,`:465-496`,`:750-790` — sin validación de nombre de puerto en servidor antes de mutar; difunde antes de insertar y no revierte), SEC01 (`LFPG_NetworkManagerImpl.c:2150` y 7 sitios más — los 9 `.Send(` migraron idénticos; FullSync valida identidad y luego envía `null`).
- Por qué ahí: saltos de política y destinatarios de red equivocados; no pierden valor de jugador pero rompen la frontera de confianza. Después de T1 porque T1 es irreversible y T2 no.
- Bloquea: nada duro; puede aprovechar que T0b ya renombró `_TEST` si se quiere tocar el handler unificado, pero no es necesario.
- Señal de fin: reemplazo autorizado/no autorizado/almacenamiento fallido sin borrar el anterior (PR 2 de la auditoría B); recuento de entregas por receptor (PR 1); dos clientes + JIP.

**T3 — Dispositivos rotos.**
- Entra: D04 (`LFPG_Intercom.c:822-831` — `LFPG_GhostPASBroadcaster` no declarada en `config.cpp`; el hermano `LFPG_GhostPASReceiver` sí en `:2562`; función muerta), D05 (`LFPG_BatteryAdapter.c:84-89`; `lfpg_devicebase.c:430-455` — promete recogida con F, no reoverridea los tres guards, sin kit no hay desmontaje; irrecuperable), D01 (`LFPG_Furnace.c:453-470`,`:540-547` — deadline absoluto reescrito en cada encendido, `m_BurnNextMs` no se persiste), D02 (`LFPG_Furnace.c:121-145`,`:289-292` — `super.EEInit()` activa calor antes de `m_UTSource`), D03 (`LFPG_Battery.c:341-352`; `NetworkManagerImpl.c:7644`,`:7722` — round-trip float→int×10→float por tick; el adaptador no lo sufre).
- Por qué ahí: funciones muertas o que mienten al jugador; no pierden valor; después de T2 porque T2 es autoridad y T3 es dispositivo.
- Bloquea: D04 y D05 requieren decisión de producto (¿declarar `LFPG_GhostPASBroadcaster` en config, o borrar la función? ¿el adaptador se recupera o se retira?).
- Señal de fin: `config.cpp` coherente con las clases referenciadas desde código; prueba de tiempo/energía/lifecycle por dispositivo (PR 5 de la auditoría B).

**T4 — Grafo y coste.**
- Entra: G01 (`LFPG_ElecGraphImpl.c:3673+`), G02 (`:3010`,`:3052` — solver suma `m## 3. PLAN (continuación)

**T4 — Grafo y coste.**
- Entra: G01 (`LFPG_ElecGraphImpl.c:3673+`), G02 (`:3010`,`:3052` — solver suma `m_VirtualGeneration`, validador solo mira `incomingPower`), G04 (`LFPG_VanillaActionOverrides.c:100`,`:172`), G18 (`LFPG_ElecGraphImpl.c`), R02 (`LFPG_CableRenderer.c:2444-2510`,`:2556-2567` — owner-null congela `cachedMinDist`), R04 (`LFPG_CableRenderer.c:2182`,`:3992`; `LFPG_Defines.c:223` — presupuesto 512 sin prioridad, `CullTick` no decrementa), S04 (`LFPG_NetworkManagerImpl.c:6193-6197`,`:6272-6276` — cursor global sin rotación), S08 (`LFPG_SorterLogic.c:1105-1327`; `RPCServerHandlerImpl.c:147-150` — `maxEval=200` no envuelve `RepackCargoInPlace` O(N²); **V4 hereda el pico**).
- Por qué ahí: ninguno pierde datos; son trabajo evitable y coherencia. Último porque es donde el «impuesto del fork» sí mordía, y T0b ya lo quitó.
- Bloquea: T0b merged (para no escribir el fix dos veces en S04/S08).
- Señal de fin: red estable drena y batería autónoma coherente (PR 4); coste máximo acotado y turnos de todos bajo carga (PR 7); viaje entre bases con cables largos y paredes (PR 8).

**T5 — Instrumento (paralelo desde el día 1).**
- Entra: A03 (la cobertura actual valida el checker, no la lógica) + línea base del §7 de la auditoría B.
- Por qué ahí: sin esto ningún tramo se declara sin regresión (el orquestador y la auditoría B coinciden).
- Bloquea: nada; alimenta a todos.
- Señal de fin: escenarios de DayZ/diag, pruebas de fallo y línea base de rendimiento versionada (ficha A10).

## 4. DESACUERDOS CON EL BORRADOR

1. **T0 antes que T1 (§D).** Discrepo. T0 = T0a + T0b. Solo T0a va antes; T0b va después de T1. Razón: T1 no sufre el impuesto del fork (es BTC, no sorter UI), y T0b no arregla S04/S08 (los hereda). Posponer T1 detrás de un refactor de 106 referencias, sin cobertura (A03), retrasa el único daño irreversible.
2. **Justificación de T0 por el impuesto recurrente (§D, razonamiento).** Parcial. El impuesto se cobra sobre todo en T4 (S04/S08), no en T1/T2/T3. Usarlo para meter T0 antes de T1 es usar el argumento en el tramo donde menos aplica.
3. **H7 como razón para posponer T1 (§D, razonamiento de T1 no va primero).** Acepto el hecho, discrepo con la inferencia. «Latente» reordena T1 frente a T2/T3/T4, no lo exime de ir temprano. La posición correcta es T0a → T1 → T0b, no T0 → T1.
4. **T0a como nuevo tramo (§F, revisión).** Coincido. Es la parte buena del borrador revisado: mecánico, aislado, desbloquea ambas. Lo mantengo.
5. **Advertencia de que S08 se hereda en V4 (§F, nota final).** Coincido y la elevo a restricción del plan: T4 no se puede declarar cerrado solo con jubilar V3. La ficha sigue viva en `RPCServerHandlerImpl.c:147-150`.
6. **Tratamiento de P2/P3 como clases (§E).** Discrepo levemente. Es legítimo para planificar, pero el plan debe decir explícitamente que 98 fichas en ficheros tocados son **desconocidas**, no asumidas-corregidas. La advertencia H2 va en §6.
7. **«Alcance duro: sin refactor» para T0 (§D).** Discrepo para T0b. Añadir `LFPG_SorterView_TEST.IsOpen()` en `LFPG_Actions.c:528` y `LFPG_ActionSyncSorter.c:69`, crear el emisor de `SORTER_TEST_RESYNC (65)` y el productor de `SORTER_TEST_CARGO_REFRESH (70)`, y decidir qué hace la entidad `LFPG_Sorter` sin UI V3, **son** cambios de comportamiento, no refactor mecánico. T0b no cabe bajo «sin refactor».

## 5. RIESGOS QUE NADIE HA NOMBRADO

- **El inventario de clases borrables no se ha hecho.** H6 dice que solo las clases liberan arena. V3 da 7. Pero D04 (`LFPG_GhostPASBroadcaster` no declarada en `config.cpp`, función muerta), SEC18 (Migrators, infra huérfana) y R22 (código/estado muerto que sobrevive a refactors) son candidatas a **borrado de clases** que ninguna auditoría priorizó por sandbox —porque ninguna conocía el objetivo—. Si el objetivo real sigue siendo arena, hay un plan paralelo de limpieza de clases muertas que vale lo mismo o más que jubilar V3, y es más barato.
- **`sprint0_clone.py` (auditoría A §3.2).** La sincronización V3→V4 es manual vía un script de clonado. Mientras T0b no mergee, cualquier fix a V3 o V4 hecho a mano **diverge** del otro lado sin señal. T0b no solo quita el impuesto: quita un mecanismo frágil de sincronización que ya produjo deriva visible (S10, divergencia en protección de preview).
- **`McpJsonEscape` single-escape (auditoría A §5, §6 P0-2).** Un literal con `\\`+`\"` junta rompe la compilación de `World` entero (`CParser: quoted string not closed`). Es un footgun de boot muerto que **no está en ninguna de las 151 fichas de la auditoría B**. Si T0b toca el área TEST (renombra `_TEST`), puede pisar este literal sin saberlo. Va a §6 como verificación pendiente antes de mergeear T0b.
- **`SetUserData` y raw pointers (auditoría A §6, med).** El fix `F1-B` (heap `0xc0000374`) depende de `SetUserData(null)` antes de liberar `m_ColorDataRefs`. T0a mueve `LFPG_ColorData` a otro fichero; si el movimiento cambia la identidad de la clase (p. ej., la anida o la hace typedef), los raw pointers del ATM pueden dejar de ser válidos. T0a «mecánico» tiene un borde no mecánico.
- **Doble conteo de daño en E04.** E04 destruye BTC **antes** de saber si el crédito persiste. Si T1 se retrasa y un servidor privado aparece (H7 es no-detección, no certeza), el daño acumula por sesión. El plan no tiene un gate de «si aparece servidor, T1 salta la cola». Es un riesgo de orden no nombrado.

## 6. LO_NO_VERIFICADO

- **H7 es no-detección, no certeza.** El grep sobre `config.cpp` y `HANDOFF.md` no encuentra Workshop ID ni `publisherId`. Eso no prueba que no haya servidor privado con jugadores. Si lo hay, el orden T0a → T1 → T0b es incorrecto y T1 pasa a ser lo primero. El plan depende de que el dueño confirme H7.
- **La ratio kB/clase de A2 aplicada a V3 es una proyección.** El propio §A (advertencia de instrumento) lo dice: los 217 contados hoy en `4_World` no casan con el 199→185 de A2; mi regex y su método pueden diferir. Los ~21 kB de arena por jubilar V3 son orden de magnitud, no medida sobre este borrado.
- **El triage cubre solo 25 de 151 fichas.** 126 P2/P3 sin triar una a una; 98 en ficheros tocados son **desconocidas** (no corregidas, no vivas-confirmadas). El plan las trata como clases; si alguna P2 resulta ser un P1 disfrazado (p. ej., S14 preview de contenedor bloqueado = posible divulgación de inventario), el orden puede cambiar.
- **Tres fichas necesitan ejecución para cerrarse** (§4): D16 (¿`Fence.OpenFence()` respeta el candado?), G02 (cuánto persiste la incoherencia), SEC01 (recuento de entregas por cliente). El plan las asigna a T3/T4/T2 asumiendo que la ejecución confirmará el veredicto VIVO. Si la ejecución refuta, el tramo cambia.
- **`McpJsonEscape` no se ha grep-eado en `4_World/`.** Antes de mergeear T0b hay que lintar literales con doble escape; el plan lo asume como verificación previa, no está hecho.
- **La paridad `powered/linked` de la acción TEST es una decisión de producto, no técnica.** La auditoría A la marca P1-1 y deja la decisión abierta («intencional para test sin ratón» o alinear). T0b la asume como entrada; si el dueño decide «intencional», T0b no añade los guards y la ficha queda documentada, no corregida.
- **No he leído el cuerpo de `Open()/Close()/Update()` de `LFPG_SorterView_TEST.c`** (auditoría A §6, incertidumbre TRUNCATED). El censo del §5 cita `:1329` (mutex anti dual-open) pero el comportamiento de doble-ESC/focus no está verificado. T0b puede encontrar sorpresas ahí.
- **Dos lanes no pudieron correr `git` por su shell** (§4). Mi entorno podría tener el mismo límite. No he re-verificado las cuentas del §5 (29 `COL_*`, 64+5 en `BTCAtmView`, 12 en `BTCAtmController`, 13 en `SorterView_TEST`, 106 totales) ni los `path:line` del triage; confío en las citas del brief, que es lo que el §2 exige.