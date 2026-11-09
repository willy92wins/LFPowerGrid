<!-- Salida literal de Codex (codex exec, solo lectura, a ciegas). -->

APTO CON CAMBIOS

**R22-V5-01 — IMPORTANTE: el reintento añade trabajo después de calcular el resultado del PDQ, que puede devolver un falso «vacío».**

Anclajes: [plan:215](C:/tmp/lfpg-grok/wt-r22-v5/reviews/2026-11-09-grafo-splitter/PLAN-CONSOLIDADO.md:215); [LFPG_ElecGraphImpl.c:2231](C:/tmp/lfpg-grok/wt-r22-v5/scripts/5_Mission/LFPG_ElecGraphImpl.c:2231), [:2237](C:/tmp/lfpg-grok/wt-r22-v5/scripts/5_Mission/LFPG_ElecGraphImpl.c:2237), [:2913](C:/tmp/lfpg-grok/wt-r22-v5/scripts/5_Mission/LFPG_ElecGraphImpl.c:2913) y [:2953](C:/tmp/lfpg-grok/wt-r22-v5/scripts/5_Mission/LFPG_ElecGraphImpl.c:2953).

Entrada exacta: cola vacía, `m_GateRetry=["A"]`, A existente con compuerta y su entidad nuevamente resoluble. El reintento prescrito elimina A de pendientes y llama a `MarkNodeDirty(A, LFPG_DIRTY_INTERNAL)`.

**Salida mala:** `queue=["A"]`, A sucio con máscara `4`, pero `ProcessDirtyQueue` devuelve `0`. La rama inicialmente vacía conserva su `return 0`; la rama que termina de vaciar conserva el `remaining` calculado antes del reintento.

Ejecuté en memoria la rama idle real, insertando únicamente el hook prescrito y usando el `MarkNodeDirty` extraído del fuente; el helper de reintento era una implementación del diseño. Obtuve exactamente ese resultado.

Tiene consumidores reales:

- Con `m_WarmupActive=true`, [TickPropagation:2463](C:/tmp/lfpg-grok/wt-r22-v5/scripts/5_Mission/LFPG_NetworkManagerImpl.c:2463) desactiva el warmup aunque quede trabajo.
- Con una medición esperando asentamiento, [LFPG_PerfProbe.c:171](C:/tmp/lfpg-grok/wt-r22-v5/scripts/3_Game/LFPG_PerfProbe.c:171) acepta ese cero como asentamiento y deja de acumularlo.

**Arreglo propuesto [DESIGN]:** recalcular los pendientes después del barrido y del reintento, antes de ambos retornos y de informar a la sonda. Añadir una prueba contra esos consumidores que exija `remaining=1` cuando el hook acaba de encolar A. No lo clasifico BLOQUEA: aquí la cola real contiene trabajo; no he demostrado una infracción de G2 con las colas realmente vacías.

**R22-V5-02 — IMPORTANTE: falta una topología admisible para los 65 consumidores del vecino ruidoso.**

Anclajes: [plan:519](C:/tmp/lfpg-grok/wt-r22-v5/reviews/2026-11-09-grafo-splitter/PLAN-CONSOLIDADO.md:519); [LFPG_Splitter.c:53](C:/tmp/lfpg-grok/wt-r22-v5/scripts/4_World/LFPG_Splitter.c:53); [LFPG_WireOwnerBase.c:249](C:/tmp/lfpg-grok/wt-r22-v5/scripts/4_World/LFPG_WireOwnerBase.c:249); [LFPG_ElecGraphImpl.c:1476](C:/tmp/lfpg-grok/wt-r22-v5/scripts/5_Mission/LFPG_ElecGraphImpl.c:1476).

La fila prescribe un interruptor que alimenta «un Splitter con 65 consumidores». El Splitter declara únicamente `output_1`, `output_2` y `output_3`.

Entrada concreta para extender esa estrella: un cable con `m_SourcePort="output_4"`. **Salida real: `LFPG_AddWire=false`**. Ejecuté el constructor real para obtener sus puertos y el método real de admisión con esa colección: las tres salidas declaradas pasan; `output_4` y `output_65` fallan. Incluso insertando aristas por la API inferior, la decimotercera salida encuentra el límite de 12.

**Salida mala del fixture:** el modelo puede crear 65 destinatarios y cruzar presupuestos sobre una red que la fase 3 no puede reproducir literalmente. Si se pretendía una cascada de distribuidores, falta especificarla.

**Arreglo propuesto [DESIGN]:** concretar un árbol o cadena sin ciclos, con puertos y cargas reales, y comprobar su admisión y el cruce efectivo del presupuesto. Tener 65 consumidores nominales no sustituye la traza del trabajo provocado por cada conmutación.

**R22-V5-03 — MENOR, nota de la limitación de entidad: con `Cast` nulo, la energía no conserva necesariamente la compuerta cacheada.**

Anclajes: [tabla del plan:112](C:/tmp/lfpg-grok/wt-r22-v5/reviews/2026-11-09-grafo-splitter/PLAN-CONSOLIDADO.md:112), [plan:353](C:/tmp/lfpg-grok/wt-r22-v5/reviews/2026-11-09-grafo-splitter/PLAN-CONSOLIDADO.md:353); [pre-gate:2488](C:/tmp/lfpg-grok/wt-r22-v5/scripts/5_Mission/LFPG_ElecGraphImpl.c:2488); [LFPG_IDevice.c:561](C:/tmp/lfpg-grok/wt-r22-v5/scripts/4_World/LFPG_IDevice.c:561); [LFPG_DeviceBase.c:544](C:/tmp/lfpg-grok/wt-r22-v5/scripts/4_World/LFPG_DeviceBase.c:544).

Entrada exacta: nodo AND con `m_IsLogicGate=true`, `m_GateClosed=true`, `newOutput=10`; el resolutor devuelve un Splitter, cuyo `Cast` a puerta lógica es nulo.

El pre-gate preservado considera la entidad resuelta y consulta su `LFPG_IsGateOpen`, que devuelve `true`. Ejecutando esas ramas reales obtuve **`gateIsClosed=false`, `allocAvail=10`**, frente al `allocAvail=0` que correspondería al cierre cacheado. La rama nueva publicaría señal baja y pediría reintento.

**Arreglo propuesto [DESIGN]:** declarar ese comportamiento energético para entidad resuelta de tipo incorrecto y afirmarlo en el fixture. Si se pretende conservar también ahí la caché, hay que modificar expresamente el pre-gate; Step 2b no lo hace. Es una nota de la excepción declarada, no una ruptura demostrada de los casos del brief.

**Cierre de mis observaciones según §11.4**

| Observación propia | Dictamen del cierre |
|---|---|
| **R22-V4-01**, recuperación sin invalidación | **Correcto en el mecanismo propuesto.** Ahora restaurar únicamente el registro provoca un encolado automático. Queda R22-V5-01 sobre el resultado comunicado por esa llamada. La recuperación completa de A→B sigue pendiente de ejecución integrada. |
| **R22-V4-02**, vecino imposible y terminación incompatible | **Parcial.** Se elimina el ciclo y se distingue observación durante el ruido de vaciado posterior. La topología nueva deja R22-V5-02. |
| Pregunta 4 de v4, fallo de índices frente a inexistencia y energía | **Parcial.** Se retira la equivalencia entre fallo de resolución e inexistencia y se declara el efecto energético de la recuperación. Falta el matiz de `Cast` nulo de R22-V5-03. |
| Pregunta 4 de v3, C5–C7 con congestión | **Correcto como cobertura exigida.** La fila ahora nombra corte de Power, reconstrucción/warmup y Set mantenido. No he comprobado sus resultados. |
| Pregunta 5 de v4, traza del modelo frente a fuente | **Correcto como límite reconocido.** Las estructurales, R21 y partida siguen siendo pruebas pendientes; su enumeración no demuestra fidelidad. |

**Respuestas a §10**

1. **Recuperación y plazo.** Con resolución restaurada estable y llamadas en vacío, el diseño proporciona la invalidación que faltaba. Simulé la rotación con 11 pendientes y el recuperable en cada una de las 11 posiciones: se examina en un máximo de tres llamadas. No ejecuté el PDQ completo para demostrar A→B alta y B abierto en la llamada posterior. La prueba que tumbaría este cierre es el fixture original restaurando solo el registro y manteniendo la salida baja más allá del plazo, con presupuesto suficiente.

2. **G1 y diferencias de energía.** No he demostrado otra infracción de G1 tal como queda definido. El uso de caché para los demás dispositivos está expresamente permitido. La diferencia energética que falta declarar es R22-V5-03.

3. **Ramas y nacimiento del flag.** La partición descrita cubre puerta resuelta, ausente y con `Cast` nulo; otro nodo con compuerta resuelto o ausente; y nodo sin compuerta. El barrido de `scripts` encontró una creación de `LFPG_ElecNode`, en [EnsureNode:1375](C:/tmp/lfpg-grok/wt-r22-v5/scripts/5_Mission/LFPG_ElecGraphImpl.c:1375), y únicamente dos asignaciones de capacidad de compuerta, en [:1414](C:/tmp/lfpg-grok/wt-r22-v5/scripts/5_Mission/LFPG_ElecGraphImpl.c:1414) y [:3705](C:/tmp/lfpg-grok/wt-r22-v5/scripts/5_Mission/LFPG_ElecGraphImpl.c:3705). Los puntos previstos cubren ambos caminos.

4. **Tráfico, barrido, reencolados y borrados.** Mientras el resolutor siga fallando, el diseño no introduce tráfico en la cola sucia. Al resolver, sí añade trabajo después del barrido: R22-V5-01. El contador de reencolados puede incrementarse al marcar un nodo de la época recién terminada, pero [EnsureRequeueEpoch:4992](C:/tmp/lfpg-grok/wt-r22-v5/scripts/5_Mission/LFPG_ElecGraphImpl.c:4992) lo reinicia al entrar en la siguiente; no he encontrado bloqueo permanente por ese camino. Los ids borrados permanecen hasta su turno de reintento o una reconstrucción: no hay limpieza inmediata prevista en [DropRemovedNodeMaps:1007](C:/tmp/lfpg-grok/wt-r22-v5/scripts/5_Mission/LFPG_ElecGraphImpl.c:1007). Sin vacíos globales, esa depuración también queda detenida.

5. **Rojo y verde de las filas nuevas.** El vecino requiere R22-V5-02. La recuperación ya no fuerza artificialmente el reproceso que pretende demostrar. Para el caso `Cast` nulo conviene usar explícitamente un Splitter como entidad incorrecta y comprobar también `allocAvail`. El reintento acotado debe probar todas las posiciones del recuperable: simulé una variante que retira el primero intercambiándolo con el último; con `[0,…,10]` recupera al `10`, pero examina repetidamente `0,10,0,10` y deja sin recuperar al `1`. Una única posición podría dar verde a esa implementación defectuosa.

**Defecto que §6 podría no detectar:** R22-V5-01. Un modelo que inspeccione correctamente sus colas y todos los patrones S1–S16 pueden pasar aunque producción devuelva cero tras encolar el reintento. Falta comprobar el valor devuelto contra la cola real y contra warmup/sonda.

**Premisa cuestionada de la v5:** alcanzar el punto donde la cola estaba vacía no permite seguir comunicando «vacío» después de ejecutar un hook que añade trabajo. Además, `ceil(n/4)` limita llamadas **en vacío**, no tiempo de recuperación en un servidor cuya cola global permanezca ocupada; esto último está declarado en §5.10 y no lo convierto en BLOQUEA.

**Comprobado y no comprobado**

Confirmé HEAD `dd413b8`, el diff vacío de `scripts` frente a `592ebcc`, el delta v4→v5, la revisión propia v4, el brief y los anclajes citados. Ejecuté los testigos parciales descritos con fuente extraído y stubs, mediante Python `-B`, sin escribir en disco. El estado Git terminó sin cambios.

No ejecuté el PDQ completo propuesto, los fixtures integrados, Enforce, partida ni rendimiento. No comprobé la frecuencia real de entidades irresolubles o de tipo incorrecto. No inspeccioné el test nuevo a medias ni consulté revisiones ajenas.


