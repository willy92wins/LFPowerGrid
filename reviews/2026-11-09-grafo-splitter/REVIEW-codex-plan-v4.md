<!-- Salida literal de Codex (codex exec, solo lectura, a ciegas). -->

NO APTO

**R22-V4-01 — BLOQUEA: la recuperación de una entidad puede dejar la señal permanentemente baja con la cola vacía, incumpliendo G2.**

Anclajes leídos: [PLAN-CONSOLIDADO.md:141](C:/tmp/lfpg-grok/wt-r22-v4/reviews/2026-11-09-grafo-splitter/PLAN-CONSOLIDADO.md:141), [:168](C:/tmp/lfpg-grok/wt-r22-v4/reviews/2026-11-09-grafo-splitter/PLAN-CONSOLIDADO.md:168), [:460](C:/tmp/lfpg-grok/wt-r22-v4/reviews/2026-11-09-grafo-splitter/PLAN-CONSOLIDADO.md:460); [LFPG_DeviceRegistry.c:36](C:/tmp/lfpg-grok/wt-r22-v4/scripts/4_World/LFPG_DeviceRegistry.c:36); [LFPG_ElecGraphImpl.c:2220](C:/tmp/lfpg-grok/wt-r22-v4/scripts/5_Mission/LFPG_ElecGraphImpl.c:2220), [:2877](C:/tmp/lfpg-grok/wt-r22-v4/scripts/5_Mission/LFPG_ElecGraphImpl.c:2877), [:3115](C:/tmp/lfpg-grok/wt-r22-v4/scripts/5_Mission/LFPG_ElecGraphImpl.c:3115), [:3559](C:/tmp/lfpg-grok/wt-r22-v4/scripts/5_Mission/LFPG_ElecGraphImpl.c:3559).

Escenario concreto:

1. G=50 → OR A.`input_0`; A.`output_0` → OR B.`input_0`. Las otras entradas están desconectadas y B no tiene salida. A estaba abierto, con `m_GateClosed=false`.
2. Se inyecta un fallo de los tres resolutores de A hasta vaciar la propagación. La política nueva publica `A→B.signal=false`; B cierra y pide la sonda de 1.
3. El estado energético puede asentarse con asignaciones `G→A=1`, `A→B=1` y ambos nodos con `m_Powered=true`. A conserva su compuerta cacheada abierta.
4. Se restituye únicamente el registro de A con su entidad viva original. `FindById(A)` vuelve a devolverla, sin tocar cables, interruptores ni llamar a `MarkNodeDirty`.

**Salida mala:** cola y diferidos vacíos, todas las entidades resolubles, pero `A→B.signal=false` y B cerrado. El oráculo exige `OR(1,0)=true` en A y, por tanto, señal alta hacia B. También queda asignación 1 con señal baja, fuera de la excepción de entidad ausente.

El mecanismo no proporciona una pasada correctora: `Register` solo modifica el registro; el fallo de `SyncNodeToEntity` elimina la última entidad sincronizada y retorna; las llamadas idle no procesan proveedores. El barrido tampoco sincroniza estos nodos porque su alimentación ya coincide.

Ejecuté en memoria el `Register` real, la rama idle real durante 20 llamadas y las ramas reales de corrección del barrido sobre esos estados de alimentación coincidentes: ninguna produce una pasada del proveedor. **No ejecuté el PDQ completo para construir el estado inicial**; su asentamiento energético está deducido de las fórmulas leídas. El fallo temporal de resolución está inyectado, como permite §6.3; no medí su frecuencia en partida.

**Arreglo propuesto [DESIGN]:** conservar una recuperación pendiente tras el fallo, o invalidar el nodo cuando vuelva a resolverse. La recuperación debe producirse sin que el fixture encole artificialmente al proveedor. Añadir precisamente esa prueba: restaurar solo el registro, avanzar el scheduler normal y exigir señal alta y B abierto dentro de un plazo definido. Un test integrado que demuestre otra invalidación automática obligatoria tumbaría este hallazgo.

**R22-V4-02 — IMPORTANTE: el vecino ruidoso nuevo no puede montarse por la entrada de producción y contradice el corte global del arnés.**

Anclajes leídos: [PLAN-CONSOLIDADO.md:444](C:/tmp/lfpg-grok/wt-r22-v4/reviews/2026-11-09-grafo-splitter/PLAN-CONSOLIDADO.md:444), [:463](C:/tmp/lfpg-grok/wt-r22-v4/reviews/2026-11-09-grafo-splitter/PLAN-CONSOLIDADO.md:463), [:600](C:/tmp/lfpg-grok/wt-r22-v4/reviews/2026-11-09-grafo-splitter/PLAN-CONSOLIDADO.md:600); [LFPG_ElecGraphImpl.c:543](C:/tmp/lfpg-grok/wt-r22-v4/scripts/5_Mission/LFPG_ElecGraphImpl.c:543), [:1175](C:/tmp/lfpg-grok/wt-r22-v4/scripts/5_Mission/LFPG_ElecGraphImpl.c:1175).

Entrada exacta: `OnWireAdded("X", "X", "output_0", "input_1", wire)` para cerrar el XOR sobre sí mismo.

**Salida real:** `false`, antes de insertar la arista. Ejecuté esa guarda extraída del fuente. La detección de ciclos también devuelve `true` para `sourceId == targetId`.

Por tanto:

- En partida no existe el anillo exigido por §8.
- Si el modelo lo inserta directamente, está saltándose una restricción de producción.
- Si además conserva la oscilación exigida por la fila, no puede satisfacer simultáneamente el criterio general de cola y diferidos vacíos antes de 100 ticks. El circuito exige `q = XOR(1,q)`, sin solución estable.

**Arreglo propuesto [DESIGN]:** generar tráfico mediante cambios externos repetidos sobre una topología admitida. Definir la observación del circuito bajo prueba mientras dura el tráfico y detener la excitación antes del vaciado final. Si se conserva un ciclo inyectado como estrés sintético, declararlo así, excluir su montaje de partida y darle un criterio de terminación propio.

**Cierre de mis observaciones de la v3 (§11.3)**

| Observación propia | Dictamen |
|---|---|
| **R22-V3-01**, entidad no resuelta en el pre-gate | **Parcial.** Los tres niveles corrigen el testigo original cuando NetworkID recupera la entidad antes de publicar. La recuperación después de un fallo completo queda abierta por R22-V4-01. |
| **R22-V3-02**, cola `[AND, M, B]` | **Correcto como cambio de contrato.** L-RACE permite expresamente el resultado AND=false, M=true. No es una eliminación de la carrera. |
| **R22-V3-03**, Reset con Set sostenido | **Correcto.** El fixture ahora produce Q=`1→0→1` y exige comprobar la deshabilitación. Con callbacks reales obtuve cero flips adicionales para 100 ms de baja y uno para 1100 ms, a los 6500 ms. |
| Pregunta 2, altos falsos invisibles al invariante | **Correcto como criterio añadido.** El oráculo compara ambos valores del bit. Su ejecución tras un vaciado no resuelve la falta de invalidación de R22-V4-01. |
| Pregunta 3, barrido condicional | **Correcto.** Las condiciones añadidas corresponden al fuente leído; no hay reevaluación incondicional de todos los callbacks. |
| Pregunta 4, C1–C7 con congestión | **Parcial.** Añade pulsaciones con congestión; la fila concreta no describe C5 —corte de Power—, C6 —reinicio— ni C7 —Set mantenido—. Su conservación completa sigue pendiente en fase 3. |
| Pregunta 5, precondiciones del mutante (i) | **Correcto en el plan.** Se incorporan las precondiciones necesarias. No he ejecutado su muerte en el arnés integrado. |

**Respuestas a §10**

1. **G1.** No he demostrado otro incumplimiento bajo las excepciones expresas del contrato: bit inicial bajo, limpieza al cambiar habilitación y baja por entidad irresoluble. Los callbacks de puerta, con el cambio propuesto aplicado solo en memoria, coincidieron con la publicación en **24/24 combinaciones** de tipo, entradas y latch anterior. Esto no certifica el cableado completo del PDQ.

2. **G2.** Sí: R22-V4-01. Recuperar la resolución no implica que exista trabajo pendiente capaz de volver a publicar el bit.

3. **L-RACE frente al brief y Nostra.** No encontré contradicción con los escenarios documentados: B1 cambia un interruptor por paso; D5 exige capturar el Laser y conservar el latch, sin exigir rechazo de cambios simultáneos por caminos distintos. Contrasté el brief, las mediciones y `PRUEBA-EN-PARTIDA.md`; no comprobé el reporte original literal del jugador. No convertiría la analogía con un biestable físico en evidencia de equivalencia temporal.

4. **Entidad viva irresoluble y energía.** La resolución consulta índices y cachés; su fallo no demuestra que el objeto haya desaparecido. Por ejemplo, un registro ausente, un identificador nativo rechazado por `ParseVanillaId` y una caché NetworkID vacía dejan los tres caminos sin resultado. La frecuencia real es una suposición pendiente de instrumentación. Aguas abajo, esa baja puede cerrar puertas o rearmar Toggle. El cambio energético declarado también existe: con caché abierta, entidad viva cerrada, `newOutput=10` y demanda 10, el fallback nuevo puede pasar la asignación de 10 a 0 antes que la base. No he demostrado una regresión adicional causada por esa excepción autorizada.

5. **Rojo y verde de las filas nuevas.** No pueden darse por válidos en conjunto: el fixture de recuperación fuerza el reproceso que debe demostrarse y el vecino ruidoso tiene R22-V4-02. La corrección de §5.8 sí ejercita el mecanismo pretendido con las lecturas prescritas. Carrera, congestión, resolución y barrido necesitan todavía ejecución integrada; una traza del modelo no demuestra por sí sola que reproduzca esos caminos del fuente.

**Defecto que §6 no detectaría:** una implementación sin recuperación automática después de publicar baja por falta de entidad. La fila de `PLAN-CONSOLIDADO.md:460` devuelve la entidad **y reprocesa la puerta**; así puede pasar aunque producción nunca provoque esa pasada.

**Premisa cuestionada de la v4:** cola vacía no equivale a coherencia con el mundo cuando cambia la resolubilidad de una entidad sin invalidar el grafo. Asimismo, «los tres resolutores fallan» no equivale a «el dispositivo se ha borrado o aún no existe».

**Comprobado y límites**

Confirmé HEAD `d16d9f8`, el diff vacío de `scripts` frente a `592ebcc`, el delta documental v3→v4, las instrucciones y los anclajes de producción citados. Ejecuté los testigos parciales descritos mediante fuente extraído y stubs, sin escribir archivos.

No ejecuté Enforce, el PDQ completo propuesto, los fixtures integrados, partida ni rendimiento. No inspeccioné el test nuevo a medias ni leí revisiones ajenas. El árbol de trabajo terminó sin cambios.
