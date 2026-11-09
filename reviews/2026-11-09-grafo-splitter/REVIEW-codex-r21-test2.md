NO APTO

1. **R21T-01 — BLOQUEA. La suite completa acepta una mutación real incorrecta del fallback.**  
   `.github/tools/test_graph_logic_signals.py:1048` y `:4105`. Cambié únicamente en memoria el fallback sin salidas a `edgeDemand = 1.0;`, con `MUTANT=None`. Resultado: **69 tests, 0 fallos, 0 errores, 0 omitidos**. Para G de 50 y Splitter vacío con consumo 0, producción pediría 1; el modelo sigue devolviendo 0 porque, cuando la expresión regular no encuentra su patrón, conserva `extra=0`.  
   **Arreglo propuesto:** ejecutar la rama extraída o interpretar explícitamente su expresión; una construcción desconocida debe invalidar el arnés. Añadir el negativo con constante 1, además del caso específico `m_Consumption + 1.0`.

2. **R21T-02 — BLOQUEA. Varias F prueban el flag de Python y una atribuye el rojo al defecto equivocado.**  
   `.github/tools/test_graph_logic_signals.py:1173`, `:3884` y `:4200`. Aplicando solamente el texto mutado, con `MUTANT=None`, siguen verdes las F de `no_retry_empty`, `no_cast_null`, `compute_gate_cached` y otras doce comprobadas. Sus estructurales pueden detectar el texto, pero esas F no demuestran su comportamiento.  
   El caso concreto de `cached_latch_v3` es más grave: su texto conserva `QueueGateRetry`, mientras el flag suprime esa llamada. OR A empieza cerrada, por lo que publicar su caché no introduce ningún alto. La F falla porque A nunca se recupera. **Restaurando únicamente el reintento, conservando el defecto de señal, la F pasa.**  
   **Arreglo propuesto:** derivar el comportamiento de los mutantes de producción del texto alterado; reservar flags para mutantes del arnés. Para el latch cacheado, mantener el reintento y comprobar una pasada irresoluble con caché abierta y salida que deba ser baja.

3. **R21T-03 — BLOQUEA. G2 no corre en cada vaciado elegible.**  
   `.github/tools/test_graph_logic_signals.py:1600`, `:1707` y `:1731`. Construí SOURCE encendida, capacidad 50, OR resuelta y cerrada, entrada alta, todas las entidades presentes y ninguna cola, temporizador o reintento pendiente. PDQ devuelve 0 y `run_events(100)` termina sin aserción: **`g2_calls=0`**, aunque `g2_compare` devuelve `('latch', 'OR', False, True)`. La comprobación automática está solamente al final de `settle`. Además, su condición no exige que los temporizadores estén vacíos.  
   **Arreglo propuesto:** comprobar G2 en los puntos de asentamiento del planificador, con todas sus precondiciones, después de cada vaciado elegible.

4. **R21T-04 — IMPORTANTE. La rehabilitación retrasada no retrasa la pasada del proveedor después de rehabilitar Output.**  
   `.github/tools/test_graph_logic_signals.py:2976`, `:2984` y `:3017`. En la ejecución normal, Output de A se rehabilita a **13800 ms** y A escribe su primer alto también a **13800 ms**: retraso posterior a la rehabilitación, **0 ms**. La congestión retrasa la activación anterior. B ya llevaba una bajada prolongada por el `settle(ticks=20)`.  
   Sustituir `add_admissible_fill` por una función que devuelve el circuito sin añadir nodos deja **la fila verde**, con `pending_fill=0` y exactamente un volteo.  
   **Arreglo propuesto:** situar la congestión entre la rehabilitación y la siguiente publicación de A; afirmar ambos instantes, la lectura baja continua de B y el retraso mínimo exigido.

5. **R21T-05 — IMPORTANTE. Las permutaciones del latch viejo ejecutan otro escenario.**  
   `.github/tools/test_graph_logic_signals.py:2766`. El helper vacía la baja de B antes de conectar Set. Instrumenté las conexiones: en FIFO, LIFO y las 20 permutaciones, B ya publica **false** y la cola está vacía al conectar Set. Es la frontera con cambios separados, no la carrera del latch viejo.  
   **Arreglo propuesto:** ejecutar también los cambios simultáneos en FIFO, LIFO y las 20 semillas, comprobando el orden realmente consumido, G2 y el resultado de M permitido por L-RACE.

6. **R21T-06 — IMPORTANTE. La vuelta no queda condicionada a recuperar únicamente el registro.**  
   `.github/tools/test_graph_logic_signals.py:3253`. La fila rehabilita registro, vanilla y NetworkID. Forcé `resolve_find("orA")` a devolver siempre `None`: **la fila continúa verde**, recuperándose por vanilla.  
   **Arreglo propuesto:** mantener fallando vanilla y NetworkID, devolver exclusivamente el registro y afirmar que ese nivel proporciona la entidad recuperada.

7. **R21T-07 — IMPORTANTE. El oráculo de autoconsumo confía en el estado que debería contrastar.**  
   `.github/tools/test_graph_logic_signals.py:1797` y `:1805`. Con Laser abierto, entrada real **1**, consumo **5**, `m_Powered=True` erróneo y Set alto, **`g2_compare(q) == []`**. El oráculo acepta conjuntamente el estado de alimentación y la señal incorrectos porque reutiliza `src.m_Powered`. Producción calcula la cobertura del consumo en `scripts/5_Mission/LFPG_ElecGraphImpl.c:2412` y `:2423`.  
   **Arreglo propuesto:** calcular independientemente la cobertura del autoconsumo desde el flujo y la generación virtual; añadir negativos que corrompan conjuntamente alimentación y señal.

8. **R21T-08 — IMPORTANTE. Hay filas que aceptan agotar el presupuesto sin vaciar.**  
   `.github/tools/test_graph_logic_signals.py:2633` y `:2671`. En la fila de corte introduje un mutante del arnés que mantiene `lamp` pendiente después de cada llamada mientras G está apagada. Resultado: **fila verde**, `queue=['lamp']`, `settled_empty=False`, puerta cerrada y todos los bits bajos. La falta de vaciado solamente se anota.  
   **Arreglo propuesto:** exigir el vaciado con el arreglo en todas las filas cuyo contrato lo requiere; conservar las anotaciones correspondientes a la base.

9. **R21T-09 — IMPORTANTE. La cobertura nueva de presupuestos depende de una cuenta de aristas infiel.**  
   `.github/tools/test_graph_logic_signals.py:4081` y `:1059`. La asignación heredada no añade las visitas de `AllocateOutput`. Caso aislado: cola `[G,c]`, G encendida de 50 con publicación estable 50, consumidor de consumo 0, bit ya alto y presupuesto de una arista. El modelo procesa ambos nodos, cuenta una visita y devuelve 0. Producción cuenta al menos dos visitas al asignar G —`scripts/5_Mission/LFPG_ElecGraphImpl.c:4804` y `:4968`— y detiene la siguiente extracción en `:2280`.  
   **Arreglo propuesto:** reproducir los puntos reales de contabilización y añadir este discriminador. Esta dependencia ya existía en `898de79`; limita la fidelidad que pretenden certificar las filas nuevas, sin constituir una regresión de producción.

Para la tabla, **T** significa `.github/tools/test_graph_logic_signals.py`. «No muerde» indica que el testigo completo solicitado no queda demostrado; se indican sus partes que sí funcionan.

| N.º | Resultado | Evidencia |
|---:|---|---|
| 1 | **Muerde** | El retorno temprano por `deviceId` hace fallar el positivo con `MUTANT=None`. Lector extraído en T:219; positivos y negativos en T:4018. Puertas y celdas pasan por él. |
| 2 | **No muerde completo** | Suprimir sincronización y devolver `None` por NetworkID hacen fallar sus filas. La recuperación pasa con el registro permanentemente averiado: R21T-06. |
| 3 | **Muerde** | Identidades por instancia y cancelación independiente en T:4028. Ejecuté el diferido completo: a 50 ms produjo `RequestPropagate=['M']` y máscara 4. |
| 4 | **Muerde** | Ruido realmente pendiente: 1 nodo al primer cambio y 75 en los siguientes del vecino. Las ocho ventanas ejecutan **5 PDQ cada una**; separaciones de temporizadores: **500 ms**. Límite de fidelidad del presupuesto: R21T-09. |
| 5 | **Muerde en los controles pedidos** | Warmup procesa 128 y deja 12. Splitter vacío devuelve 0. Alterar la transición a `m_AllocChanged=false` falla. La cobertura del fallback admite otro verde falso: R21T-01. |
| 6 | **No muerde completo** | Tolerancia: método restaurado equivalente al de `592ebcc`, descontando comentarios y espacios. Baja global con reintento, bit al encolar, `Remove(0)` y rechazo de errores del arnés comprobados. Los rojos artificiales de R21T-02 invalidan la afirmación conjunta. |
| 7 | **No muerde** | Generación virtual y capacidad SOURCE responden a negativos independientes ejecutados. Autoconsumo puede pasar incorrectamente y G2 no corre tras cada vaciado: R21T-03 y R21T-07. |
| 8 | **No muerde completo** | Empates y órdenes adicionales se ejecutan; no queda `or True`. Rehabilitación y obligación de vaciado presentan los verdes falsos de R21T-04 y R21T-08. |
| 9 | **Muerde** | Cast nulo con Set conectado y M procesada después; M permanece desactivada en T:3289. |
| 10 | **Muerde según la excepción solicitada** | T:3798 declara equivalencia en F y el incremento adicional de `m_RequeueCount`; S2 detecta la eliminación. La explicación coincide con el rebobinado de `MarkNodeDirty`. |
| 11 | **Muerde** | Insertar `if (true) return false;` tras `inEdges`, sin flag semántico, falla en el positivo del lector. |
| 12 | **Muerde** | Las dos mutaciones textuales, sin flag, fallan: transición a false y fallback `m_Consumption + 1.0`. |
| 13 | **Muerde** | El texto que elimina `m_SignalChanged` de Step 3 hace fallar ambas F con `MUTANT=None`. |
| 14 | **No muerde automáticamente** | El comparador detecta la OR cerrada incorrecta; PDQ y el reloj la aceptan con `g2_calls=0`: R21T-03. |
| 15 | **Muerde** | Reemplazar `Clock.Remove` por borrado de todos los temporizadores hace fallar la fila de dos celdas. |
| 16 | **No muerde el retraso requerido** | La fila cuenta un volteo y una bajada larga, pero permanece verde sin congestión; publicación de A y rehabilitación coinciden: R21T-04. |
| 17 | **Muerde** | T:4136 afirma `t,pdq`; T:4151 afirma `pdq,t`. Ambos callbacks y ambos PDQ se ejecutan. |
| 18 | **Muerde** | Inyectar solamente un alto erróneo en Set durante el Cast nulo activa M; la fila falla con M efectivamente procesada después. |
| 19 | **No muerde completo** | A1–A3 recorren FIFO/LIFO/20 semillas. Los 22 recorridos adicionales con M separan los cambios por vaciado: R21T-05. |
| 20 | **Muerde** | `retry_no_cap` solo cambia el tope. Cero resoluciones en llamadas 1–9 y once en la 10. `retry_remove` visita `g0,g10,g0,g10`, reproduciendo el intercambio de Enforce. |

La suite original de `eacd64c` dio **69 tests, 0 fallos, 0 errores, 0 omitidos**.

Sobre los fuentes de `592ebcc`, ejecutados mediante sustitución de lecturas en memoria: **69 tests, 23 fallos, 0 errores y 12 omitidos**. Los fallos fueron exactamente **A1, A3, A4 ×2, D5 directo y por Splitter, y S1–S17**.

La afirmación sobre **D5 Laser → interruptor → Set** queda respaldada en esa comprobación offline: sale verde en la base; `m_AllocChanged=true` existe allí en `LFPG_ElecGraphImpl.c:2688-2690` —equivalente a `:2756-2758` actuales— y cambiarlo a false hace fallar esa fila. Sustituir en memoria el cold-start del modelo por la rama de producción mantuvo ese D5 verde: Set **5,5**, lámpara **10** y cola vacía.

No leí `REVIEW-*`, no modifiqué archivos y el árbol quedó limpio. No ejecuté el resto de suites de §6.7 ni DayZ. **Suposición pendiente:** que estos resultados representen los mismos estados en la VM y el planificador del juego; la tumbaría una ejecución instrumentada de los mismos circuitos con trazas de publicaciones, callbacks, temporizadores y vaciados.