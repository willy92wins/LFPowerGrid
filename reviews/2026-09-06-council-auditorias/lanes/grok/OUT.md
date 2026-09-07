Voy a leer el BRIEF y las skills de orquestación/auditoría que apliquen para ejecutar el encargo completo.El brief ya basta para el veredicto; voy a comprobar solo el solape de ficheros entre paleta ATM e integridad monetaria, que es el punto donde el encuadre se puede caer.## 1. CRÍTICA DEL ENCUADRE

El menú «¿V3 antes o después de los P1 de integridad?» es un falso binario. Mezcla tres trabajos con dependencias distintas, lee mal el censo y convierte un choque entre **rama en vuelo** y auditoría B en un choque entre las dos auditorías.

**H6 no refuta jubilar la V3; refuta tratar la huella de fuente como el premio.** El objetivo declarado («hueco en el mod») es arena. H6 ya midió que recortar fuente no la libera (6.466 líneas → 1 kB; 1.071.376 B de fuente → −2/0 kB; 15 clases → 44/45 kB). La jubilación vale, como proyección, ~21 kB (7 clases × ~3,0 kB/clase de A2), no los 143.010 B de H5. Eso es razón para no retrasar integridad «porque la huella urge». No es razón para no jubilar: el estado actual carga V3 (7 clases) **y** V4 TEST (10 clases) a la vez, el peor de los tres mundos (H5+H6). El objetivo correcto es **dejar de pagar dos implementaciones y medir arena compilada**, no maximizar bytes borrados.

**«Las dos auditorías dan órdenes incompatibles» (sección 6.C) es una inferencia, no un hecho.** La auditoría A no ordena jubilar la V3. Su top 5 congela el set MCP, no toca `McpJsonEscape`, decide paridad `powered/linked`, gatea `WriteMcpDump` y hace hoist/dedup. La consolidación aparece como P2-2 (helpers cliente), no como «borrar prod». Quien pone consolidación primero es el trabajo en `sorter/v4-finish` y el propio borrador. La auditoría B sí pone integridad en PR1–PR3 y la UI en PR11. El conflicto real es **rama viva vs §8 de B**, no A vs B. A ni siquiera miró E04/SEC20/G01: está truncada en 6 ficheros grandes y anclada a `c4ac943`, no a `d61705e`.

**La colisión PR6/PR7 vs borrado está inflada.** PR6/PR7 (contrato y scheduler del sorter) no viven en los cuatro `.c` de UI V3. S04/S08 están en `LFPG_NetworkManagerImpl.c` / `LFPG_SorterLogic.c` y la V4 **hereda el mismo pico** (`RPCServerHandlerImpl.c:147-150`). Jubilar V3 no los arregla y no obliga a escribirlos dos veces. Lo que sí se duplicaría es UI/cliente (S10, S11, S20–S24 y los `HandleSorterTest*`). Eso justifica no implementar P2 de UI V3; no justifica aparcar SEC01/E04/G01.

**«T1..T4 incluyen 24 fichas del sorter (S01–S24)» contradice la propia tabla D.** T1 es E\*; T2 es SEC01/02/20; T3 es D\*; T4 solo mete S04 y S08. El impuesto de fork se usa como razón de T0-primero sobre un plan que **no** implementa esas 24 fichas en T1–T4. Es circular.

**T0 y T1 no son el mismo subsistema.** El censo F dice que extraer paleta reescribe el cajero y que ahí viven E04/E16/SEC09. Las citas del triage lo desmienten por fichero:

- paleta/ATM UI: `LFPG_BTCAtmView.c`, `LFPG_BTCAtmController.c` (76 `COL_*` + `LFPG_ColorData`);
- E04/E02/E03: `LFPG_BTCHelper.c` (E02/E03/E08 **intactos** desde `d61705e`; E04 bloque idéntico en `:1465`, `:1505-1513`);
- E16: `LFPG_BalanceProvider_NativeImpl.c:1230-1243` y `LFPG_BTCAtm.c:165-176`;
- SEC09: `LFPG_FileUtil.c:580-585` / `:214-218` / `:280-283`.

Acoplamiento de producto (el cajero), no de líneas. T0a no pasa por el commit/abort monetario.

**T0a del propio borrador F lee mal las 13 refs de TEST.** La contraverificación atribuye 13 refs V3 a `LFPG_SorterView_TEST.c`. La tabla solo detalla el mutex `IsOpen` en `:1329`. Las `COL_*` de V4 son **propias** (`LFPG_SorterView_TEST.COL_RED` `:257`, etc.). «Repuntar los 13 de V4» dentro de una «extracción pura, cero comportamiento» mezcla paleta con T0b (mutex/dual-open). Unificar `LFPG_ColorData` con `LFPG_ColorData_TEST` tampoco es mecánico: la auditoría A marca `SetUserData` / heap `0xc0000374` como footgun si se toca el tipo.

**H7 no prueba lo que se le pide.** Sin Workshop ID ni `publisherId` (H7) no hay rastro de despliegue **público**. No implica «nadie tiene mundo con BTC». El dueño lleva semanas en V4; un servidor privado es compatible con H7. El borrador lo admite en 6.E y aun así usa H7 para **invertir** T1. Ausencia de evidencia de Workshop no es evidencia de ausencia de jugadores.

**H3/H4 y el pre-filtro.** «Fichero intacto ⇒ ficha viva» es sólido (H2: 34/151 en ficheros no tocados; 6 P1 vivas por esa vía). «Fichero tocado ⇒ no se sabe» es el único sentido del pre-filtro; el triage de 25 P1 sí leyó las 19 tocadas y concluyó 0 corregidas + 1 PARCIAL, eso **sí** lo prueba para P1. Lo que no prueba es el resto: 126 P2/P3, 98 en ficheros tocados, desconocidas. H4 («ninguno de los 22 commits atacó integridad») choca con SEC09 «arreglado en `ef29b73`» y con el veredicto PARCIAL. «Cero corregidas» es demasiado limpio.

**Tramos rotos o incompletos.** D/F dejan fuera R15 (CCTV, P1) y D16 (candado Fence, P1 potencial). T1 omite E05, que la auditoría B §3.2 agrupa con SEC09/E04/E16 en el mismo invariante commit/abort. T2 omite SEC03, hermano de SEC02 (reemplazo que borra aunque la nueva arista no se pueda guardar). G01 vive en fichero **intacto** (`LFPG_ElecGraphImpl.c`) y la B lo contrastó por segunda revisión; meterlo en T4 «ninguno pierde datos» es un corte por tipo de pérdida (inventario sí, electricidad no), no por dependencia.

**Trabajo que ninguna auditoría vio porque no conocían el objetivo de arena/V4.** (1) Medir arena compilada al borrar 7 clases, no bytes. (2) Agujeros RPC 65/70: no bloquean borrar `.c` V3, sí bloquean que TEST sustituya el cableado. (3) Destino de la entidad `LFPG_Sorter` / `LFPG_ActionOpenSorterPanel` si desaparece la UI V3. (4) ¿Sobrevive el hook MCP tras jubilar? Si sí, V4 sigue siendo 10 clases + polling/`OpenFile` y puede comerse el ~21 kB. (5) Ramas vivas `debt/v3-data-integrity` (+1) y `maint/audit-kimi-followup` (+1): el plan no dice cómo encajan; el nombre de la primera es un posible trabajo de integridad ya empezado. (6) `release/lfpg-footprint-rc` (=`289592b`) ya existe: un RC de huella sin T0a+T0b sigue cargando las 17 clases.

Los tramos T0a (paleta) / T1 (dinero en ficheros ajenos al sorter) / T0b (borrar V3) no son un total order. Atarlos a una sola pregunta antes/después es lo que está mal planteado.

## 2. RESPUESTA A LA PREGUNTA

**Ninguna de las dos, en los términos del menú.** No se jubila la V3 completa antes de los P1 de integridad, y no se aparcan esos P1 hasta después de borrar las 7 clases.

Orden real: **T0a (paleta ATM) y los P1 monetarios de fichero intacto, en paralelo o T1 justo detrás de T0a; borrar V3 (T0b) después de T0a y de decidir paridad/RPC/entidad; el resto de P1 de integridad no espera a V3 ni la bloquea.** Los P2/P3 de UI del sorter sí van **después** de T0b, para no escribirlos dos veces y para no «arreglar» S10 en código que se va a tirar.

Por qué no «V3 primero» (T0 del borrador D): jubilar no es un atajo de arena (H6); T0b es grande (106 refs externas, no 4 ficheros); S04/S08 y casi todos los P1 de T1–T3 no se duplican; H7 no autoriza a dejar E04 latente; T0a mal leído (las 13 de TEST) puede meter el mutex en un commit «mecánico».

Por qué no «integridad primero» al estilo PR1–PR12 de B: esa secuencia no sabe que V3+V4 cargan a la vez, que S10 ya diverge, que 65/70 no tienen emisor/productor, y pondría PR6/PR7 sobre un fork que se está matando. La consolidación no es PR11 de 12; es un cierre de producto **acotado**, no un refactor de helpers/kits.

Hinge de H7: si hubiera servidor con jugadores, T1 no «pasa a ser lo primero» en bloque — **ya debería serlo para E04/E16/SEC09**, y eso no exige esperar ni bloquear T0b. Si no lo hay, T1 sigue siendo barato: E02/E03/E08 (y G01/G18/A03) están en ficheros intactos, sin impuesto de merge con V4.

## 3. PLAN

Atención ≠ dependencia. Un solo implementador puede serializar; el DAG no lo obliga. T5 arranca el día 1.

**T5 — Instrumento (paralelo, desde el día 1)**  
Qué: A03 (la cobertura actual valida el checker, no la lógica) más línea base de arena **por clases compiladas**, el método que H6 dice que rinde, no `stat` de `.c`.  
Por qué: sin esto T0b no puede reclamar «hueco» y T1 no puede reclamar no-regresión.  
Bloquea: nada. Lo que T5 bloquea a los demás es **declarar** un tramo cerrado, no empezarlo.  
Señal: un caso que falle si E04 destruye BTC y el crédito falla; recuento de clases en HEAD `421cabb` con el mismo método que A2/H6, repetido tras T0b.  
Fichas/ficheros: A03; el §7 de B (escenarios DayZ/diag, fallos, benchmark). No inventar nombres de test que el brief no da.

**T0a — Paleta neutra (primero entre el trabajo V3; no es T1)**  
Qué: sacar `LFPG_ColorData` (`LFPG_SorterView.c:45`) y las 29 `COL_*` a un módulo neutro (`3_Game/LFPG_UIPalette.c` o equivalente). Repuntar **solo** `LFPG_BTCAtmView.c` (64 `COL_*` + 5 `LFPG_ColorData`) y `LFPG_BTCAtmController.c` (12 `COL_*`). Dejar que `LFPG_SorterView.c` consuma el módulo para no romper V3 hasta T0b.  
Qué **no** entra: las 13 refs de `LFPG_SorterView_TEST.c` (mutex `:1329` y resto de API V3); unificar `LFPG_ColorData_TEST` (`LFPG_SorterView_TEST.c:71`) — eso toca `SetUserData` (auditoría A, hallazgo heap).  
Por qué: borrar `LFPG_SorterView.c` sin esto tumba el ATM (censo). Es el único acoplamiento duro ATM↔V3.  
Bloquea: nada de integridad monetaria.  
Señal: `grep` de `LFPG_SorterView.COL_` y `LFPG_ColorData` en `LFPG_BTCAtm*.c` a cero; diff mecánico de constantes; ATM y panel V3 siguen pintando; compile Mission+World.

**T1 — Integridad monetaria (paralelo a T0a: ficheros disjuntos)**  
Qué: E04, E16, SEC09 residual, E02, E03, E08, y **E05** (P2, mismo invariante que B §3.2: resultado de la operación = lo que sobrevive al reinicio).  
Ficheros: `LFPG_BTCHelper.c` (E04 `:1465`, `:1505-1513`; E02/E03/E08 intactos); `LFPG_BalanceProvider_NativeImpl.c:1230-1243` + `LFPG_BTCAtm.c:165-176` (E16); `LFPG_FileUtil.c:214-218`, `:280-283`, `:580-585` (SEC09: `.tmp` parseable sin target y último recurso).  
Por qué: única pérdida irreversible de valor; 5/6 P1 viven en código que V4 no está reescribiendo (H2/H4). No hay impuesto de doble UI.  
Bloquea: mirar qué hay en `debt/v3-data-integrity` (+1) antes de duplicar trabajo (contenido **no** está en el brief). No espera T0b.  
Señal: matriz offline de fallo de save tras destruir objetos (BTC o crédito restitidos, o no se destruye antes de persistir); `.tmp` huérfano no se promueve; tombstone E16 no desaparece antes de stock durable; compile. Cierre in-game: el dueño corre un Sell/Deposit con save forzado a fallar — T5 no sustituye eso (H7/A03).

**T0b — Cerrar V4 lo justo y borrar las 7 clases V3 (después de T0a; independiente de T2–T4)**  
Qué, en este orden interno:  
1. Decisión de producto: paridad `powered/linked` en `LFPG_ActionOpenSorterPanel_TEST` (A P1-1) y destino de `LFPG_ActionOpenSorterPanel` / entidad `LFPG_Sorter` si ya no hay UI V3.  
2. Emisor de `SORTER_TEST_RESYNC = 65` y productor de `SORTER_TEST_CARGO_REFRESH = 70` (hoy receptor/ACK sin `Write`; el único resync es V3 `LFPG_ActionSyncSorter.c:101` → 29; `BroadcastCargoRefreshToNearby` fija subId 34 en `LFPG_NetworkManagerImpl.c:6316`).  
3. `LFPG_SorterView_TEST.IsOpen` en `LFPG_Actions.c:528` y `LFPG_ActionSyncSorter.c:69` (hoy solo V3).  
4. Retirar enganches V3 de `LFPG_MissionInit.c` (`Init` `:148`, ESC `:176,180`, pausa `:227,229`, muerte `:250,268`, yield dual-open `:275`, `Cleanup` `:473`) y handlers de `LFPG_RPCClientHandler.c` (`Open` `:568`, `OnSaveAck` `:580`, `OnSortAck` `:592`, `OnPreviewData` `:729`). Quitar mutex TEST→V3 en `:1329` y el dual-open de `LFPG_ActionOpenSorterPanel_TEST.c:67`.  
5. Decidir el MCP (layout `LFPG_MCP_SorterCmd` + `Update()` + `WriteMcpDump`): o módulo debug mínimo, o fuera. Dejarlo «porque TEST» se come el ~21 kB.  
6. Borrar las 7 clases V3 + layouts `gui/layouts/LFPG_Sorter*.layout`. `LFPG_SorterController` / `TagView` / `PreviewRow` no tienen clientes externos.  
Qué **no** entra: rename masivo `_TEST` → nombres definitivos (T0c); PR6/PR7; S04/S08; unificar ColorData_TEST.  
Por qué aquí: T0a ya salvó el ATM; el fork deja de crecer (S10); se sale del peor estado de arena.  
Bloquea: T0a; las dos decisiones de producto (paridad, entidad, MCP).  
Señal: `grep` de `LFPG_SorterView` / `LFPG_SorterController` / `LFPG_ColorData` / `LFPG_SorterTagView` / `LFPG_SorterPreviewRow` **sin** `_TEST` a cero en scripts; 65 y 70 tienen productor; panel único abre/cierra con ESC/muerte; ATM abre; compile World+Mission; recuento de clases vs T5.

**T0c — Rename `_TEST` → canónico (opcional, después de T0b estable)**  
Qué: nombres definitivos, layouts `gui/layouts/test/` si salen de `test/`.  
Por qué separado: el rename no libera arena; mezcla el diff de borrado con un replace global.  
Bloquea: T0b verde.  
Señal: cero sufijos `_TEST` en producción; `config.cpp` no necesita nombrar las vistas (el brief: no nombra View/Controller/TagView/PreviewRow/ColorData); compile.

**T2 — Autoridad de servidor (no espera T0b)**  
Qué: SEC01 (`LFPG_NetworkManagerImpl.c:2150` y 7 sitios: 9 `.Send(` idénticos; FullSync valida identidad y envía `null`); SEC02 (`LFPG_RPCServerHandlerImpl.c:602`, `:680`: `RemoveWiresTargeting` con `allowOthers=true`; el corte sí aplica política) **más SEC03** (reemplazo que elimina aunque la nueva no se almacene); SEC20 (`:283`, `:465-496`, `:750-790`: puerto sin validar más que longitud ≤32; `GetDeviceId()==""` salta `HasPort`/`CanConnectTo`; difunde antes de insertar y no revierte).  
Por qué: no son sorter; no se duplican; B los pone en PR1–PR2 con razón.  
Bloquea: nada de V3. SEC20 necesita cliente modificado para explotarse; igual se valida **antes** de mutar.  
Señal: dos clientes / JIP (B PR1); reemplazo no autorizado no borra; puerto inventado no entra al grafo; no broadcast donde debía haber unicast.

**T3 — Dispositivos que mienten o están muertos**  
Qué: D04 (`LFPG_GhostPASBroadcaster` ausente en `config.cpp`; el hermano Receiver sí está en `:2562`); D05 (adaptador, guards heredados, irrecuperable); D01/D02 (`LFPG_Furnace.c:453-470`, `:540-547`, `:121-145`, `:289-292`); D03 (`LFPG_Battery.c:341-352` vs adaptador en `NetworkManagerImpl.c:7644`, `:7722`); **D16** (potencial: `LFPG_DoorController.c:574-612`, `:754-804` — confirmar `Fence.OpenFence()` vs candado **antes** de tratarlo como exploit); **R15** (CCTV `:846-855`, `:757-779`: timeout 5 s sin token de sesión).  
Por qué: no son sorter; D04 es un alta de config.  
Bloquea: D16 necesita ejecución in-game (el brief lo dice).  
Señal: la clase PAS instancia; el adaptador se recoge; el horno no reinicia deadline al toggle; batería y adaptador cuenten igual; puerta ajena con candado no abre **si** el motor no lo revalida.

**T4 — Grafo, render, scheduler (después o en paralelo; G01 se puede adelantar)**  
Qué: G01/G18 (fichero intacto `LFPG_ElecGraphImpl.c` — G01 puede ir tan pronto como T1 en atención); G02 (`:3010`, `:3052`: solver suma `m_VirtualGeneration`, validador solo `incomingPower`); G04 (`LFPG_VanillaActionOverrides.c:100`, `:172`); R02/R04 (`LFPG_CableRenderer.c` + `LFPG_Defines.c:223`); S04 (`NetworkManagerImpl.c:6193-6197`, `:6272-6276`); S08 (`:6566`; `LFPG_SorterLogic.c:1105-1327`; V4 hereda).  
Por qué: no pierden BTC; S04/S08 se arreglan **una** vez en el handler compartido, da igual V3 viva o no. Adelantar G01 no es «optimización»: B §3.2 lo contrastó (marca cambio con asignación final idéntica).  
Bloquea: nada de T0.  
Señal: red estable no reencola si el total final no cambia; batería con generación virtual no la apaga el validador; round-robin rota de sorter al agotar presupuesto; `RepackCargoInPlace` acotado (no solo `maxEval=200` alrededor).

**Después de T0b, no antes — P2/P3 de UI/contrato sorter**  
S01–S03, S10–S12, S11, S20–S24, dedup `HandleSorterTest*` (A P2-2). S10: al borrar V3 te quedas con la protección de preview de V4; hay que **mirar cuál era más estricta** y conservarla, no asumir que TEST es mejor.

**P2/P3 no triados (126; 28 vivas por H2, 98 desconocidas)**  
No se planifican como 12 PRs. Al tocar un fichero de T1–T4, se trian las fichas de ese fichero. Las 28 de fichero intacto siguen vivas por construcción; no son urgentes por el solo hecho de existir.

Congelados (auditoría A, siguen valiendo si MCP sobrevive): set MCP `dump/close/catch_all/tab_preview/cat:N` + `McpCanEdit`; `McpJsonEscape` single-escape; `RPCGuard` deny-by-default + rebind A1.

## 4. DESACUERDOS CON EL BORRADOR

1. **El orden T0→T1→… como respuesta a la pregunta.** Lo rechazo. T0a ∥ T1, luego T0b; T2–T4 no esperan. Coincido en que T0a debe existir y en que T5 es paralelo.

2. **«T0 primero pese a B porque el fork es un impuesto sobre 24 fichas S01–S24».** Falso respecto a la tabla D y respecto a S04/S08 (compartidos, heredados por V4). El impuesto es real y **estrecho**: UI/cliente. No paga retrasar E04 ni SEC20.

3. **H7 como interruptor que pone T1 el último salvo servidor público.** Lo rechazo. H7 no da ese derecho. T1 es barato en ficheros intactos aunque no haya Workshop.

4. **«T0 y T1 tocan el mismo subsistema» (F).** Acoplamiento de paleta en View/Controller, no de E04/E16/SEC09. Extraer constantes no es la segunda pasada sobre el commit/abort. Hacerlos por separado **no** es «tocar el ATM dos veces» en el sentido que F teme.

5. **T0a «repuntar 76 del ATM + 13 de V4», cero comportamiento.** Los 13 de TEST no son paleta; unificar `ColorData_TEST` no es cero riesgo (SetUserData). T0a = ATM + origen V3, punto.

6. **T0 «alcance duro: sin refactor» (D) vs T0a+T0b+rename (F).** T0b no es pequeño (el propio F lo concede). El rename `_TEST` no debe ir en el mismo tramo que el borrado. D decía T0 en vuelo y pequeño; F lo corrige a medias y sigue metiendo rename en T0b.

7. **T1 sin E05.** B §3.2 lo pone con SEC09/E04/E16. Lo meto en T1. No trago todo E\* (E01/E06/E07 son coste, no commit/abort).

8. **T2 sin SEC03.** SEC02 sin SEC03 deja el reemplazo de cable a medias (borra aunque falle el alta).

9. **R15 y D16 caídos de D/F.** Vuelven a T3. D16 sigue POTENCIAL hasta `Fence.OpenFence()`.

10. **G01 en T4 «nadie pierde datos».** Adelantable: fichero intacto, P1 contrastado. Pérdida de coherencia eléctrica no es BTC, pero tampoco es «trabajo evitable» inocuo.

11. **S08 como argumento de jubilar V3.** El propio F avisa de que V4 lo hereda. Coincido en el aviso; lo uso en contra de T0-primero, no a favor.

12. **Silencio sobre MCP, ramas `debt/v3-data-integrity` y `maint/audit-kimi-followup`, y sobre qué preview gana en S10.** El plan tiene que nombrarlos.

13. **Coincido** en: H1 (B no está desfasada respecto al merge-base); peor estado = las dos UIs cargadas; T0a como commit aislado de paleta; no implementar PR6/PR7 sobre V3; T5 desde el día 1; «fichero tocado ≠ arreglado».

## 5. RIESGOS QUE NADIE HA NOMBRADO

- **V4 es más grande que V3 (H5: 178.279 B / 10 clases vs 143.010 B / 7).** Tras T0b el sorter **crece** respecto a la era solo-V3. El ~21 kB es «quitar 7», no «volver a 7». Si MCP+polling+`WriteMcpDump`+clases `_TEST` extra se quedan, el objetivo de arena puede salir **neutro o negativo**. Nadie convirtió «jubilar V3» en «presupuesto de clases netas del sorter».

- **T0a tropieza con el tipo de `SetUserData`.** ATM usa `LFPG_ColorData`; TEST usa `LFPG_ColorData_TEST` y A documenta un crash de heap si se libera mal. Un «módulo paleta» que fusione las dos clases en el mismo commit no es extracción pura.

- **S10 en la dirección desconocida.** Si la protección de preview **mejor** está en V3, T0b congela la peor. El borrador usa S10 solo como prueba de deriva, no como pérdida de invariante.

- **`LFPG_Sorter` sin UI.** El censo pregunta qué hace la acción/entidad prod. Nadie cierra el agujero: jugador con sorter vanilla-de-mod abre panel muerto, o se deja una acción V3 huérfana que aún se registra en `SetActions`.

- **Dos ramas hermanas ignoradas.** `debt/v3-data-integrity` (+1) y `maint/audit-kimi-followup` (+1) salen del mismo tronco `289592b`. Un plan que empieza T1 o T0b sin mirar esos +1 puede pisar un fix o divergir el merge. `release/lfpg-footprint-rc` en `289592b` puede etiquetar un RC que aún carga 17 clases de sorter.

- **SEC09 PARCIAL + T0a da falsa sensación de «ATM tocado = integridad tocada».** El residuo está en `FileUtil` (`.tmp` sin target, fallback `:280-283`). Quien «pasa por el cajero» extraendo colores no pisa esas vías.

- **Segunda familia destroy-then-credit.** El brief ancla E04 al Sell (`:1465`, `:1505-1513`). En el mismo helper hay otros `DestroyPlayerItems` (el triage no los nombra). Un parche que solo cierre el bloque citado puede dejar el mismo orden en depósito/otras txs. No lo doy por finding; es el riesgo de parche mínimo mal acotado.

- **A03 no cubre T0b.** Tests del checker no ven dual-open, ESC, RPC 65/70 ni ATM. El verde de compile tras borrar 7 clases no es paridad. El único gate es in-game (DZ-R5: un ciclo, no uno por ficha).

- **Atención de un solo implementador.** El DAG paralelo es real; la cola humana no. El riesgo no es merge, es abrir T0b a medias y no tocar E04 durante semanas. Mitigación: T1 son ficheros intactos, PR pequeño, mergeable sin esperar al rename.

- **P2 en ficheros que T1 va a editar.** E01/E06/E07/E15 viven en economía. La tentación «ya que estoy» hincha T1. Contrario a parche mínimo. E05 entra porque es el mismo invariante; el resto no.

- **Instrumento de clases.** El propio borrador avisa: 217 clases en `4_World` no casan con 199→185 de A2. Quien declare «+21 kB» tras T0b sin repetir **el mismo** recuento compilado está mintiendo con H5.

## 6. LO_NO_VERIFICADO

- Si existe servidor privado con jugadores (H7 solo niega rastro público). El orden de T1 no debería depender de adivinarlo, pero el **gate in-game** sí.
- Contenido del +1 de `debt/v3-data-integrity` y del +1 de `maint/audit-kimi-followup`.
- Las 98 P2/P3 en ficheros tocados: vivas, muertas o parciales. Las 28 en intactos se asumen vivas (H2), no re-leídas.
- D16: si `Fence.OpenFence()` revalida candado/permiso (hace falta ejecución).
- G02: cuánto persiste la incoherencia validador vs solver (el brief pide ejecución).
- SEC01: recuento real de entregas por cliente (el brief pide ejecución).
- Ratio 3,0 kB/clase de A2 aplicada a **estas** 7 clases V3: proyección, no medida de este borrado (el borrador 6.E lo dice; lo mantengo).
- Las 13 refs V3 en `LFPG_SorterView_TEST.c` más allá del mutex `:1329`: el brief no las desglosa. Asumo que no son `COL_*` de V3 porque V4 tiene paleta propia; no he recontado el 13.
- Descuadre 106 vs 9+4+64+5+12+13=107 en la contraverificación: no re-derivé el grep.
- Cuál UI (V3 vs V4) tiene la protección de preview **más estricta** (S10).
- Si `LFPG_BTCAtmController` contiene lógica de commit monetario además de clicks/paleta. El triage coloca E04 en `LFPG_BTCHelper.c` y E16 en provider + `LFPG_BTCAtm.c`; planifico con eso. (Lectura de nombres de clase: el controller es `ViewController` y el helper declara `HandleBTCSell`; eso **confirma** el split del brief, no añade APIs al plan.)
- Si hay más destroy-then-credit que el bloque E04 citado; no lo convierto en ficha.
- `config.cpp`: entradas de `LFPG_Sorter` vs `LFPG_Sorter_TEST` (el brief solo dice que no nombra las vistas). T0b/T0c pueden necesitar config de entidad/acción no listada.
- Lista exacta de las 7 y 10 clases (H5 da conteos; no hay censo de classnames).
- Si `ef29b73` es uno de los 22 commits de H4 (choca la frase «ninguno atacó integridad» con SEC09 PARCIAL).
- Semántica de motor de `Remove(0)` / FIFO, `CopyFile` no atómico, etc., más allá de lo que B ya da; no las re-verifico.
- APIs no citadas en el brief (incluidas firmas vanilla `Fence.OpenFence`, `GetDeviceId`, `HasPort`): no las doy por firmas leídas aquí.
- Número de «24 fichas sorter en T1–T4»: el razonamiento D las nombra; la tabla D no. No existe ese conjunto en el plan que se ataca.