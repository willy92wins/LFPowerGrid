NO APTO

1. **R21-01 — BLOQUEA — El lector no ejecuta el cuerpo extraído.**  
   [.github/tools/test_graph_logic_signals.py:193](C:/tmp/lfpg-grok/wt-r21t/.github/tools/test_graph_logic_signals.py:193). `load_is_port_receiving_signal` reconstruye el resultado buscando cadenas. Inserté en memoria `if (true) return false;` después de la declaración de `inEdges`, conservando los anclajes de los mutantes: **69 tests verdes**. En producción ese lector devolvería siempre baja. El guard solicitado con `deviceId != ""` muerde porque el arnés reconoce expresamente esa condición.  
   **Arreglo propuesto:** interpretar el cuerpo y sus condiciones reales; cualquier construcción no soportada debe producir `HarnessError`. Añadir este guard independiente como negativo.

2. **R21-02 — BLOQUEA — Las filas nuevas pueden certificar ramas de producción que el modelo ignora.**  
   [.github/tools/test_graph_logic_signals.py:1235](C:/tmp/lfpg-grok/wt-r21t/.github/tools/test_graph_logic_signals.py:1235), [:3791](C:/tmp/lfpg-grok/wt-r21t/.github/tools/test_graph_logic_signals.py:3791) y [:3995](C:/tmp/lfpg-grok/wt-r21t/.github/tools/test_graph_logic_signals.py:3995). Comprobaciones sobre texto real, sin activar el flag correspondiente:

   - Cambiar a `false` el `m_AllocChanged` de la transición de compuerta en producción deja **69/69 verdes**.
   - Cambiar el fallback sin salidas a `targetNode.m_Consumption + 1.0` deja **69/69 verdes**; la fila del Splitter vacío sigue afirmando demanda 0.
   - Aplicar únicamente el mutante textual que quita `m_SignalChanged` de Step 3 deja verdes sus dos F: `test_mutant_signal_changed_xor_open_input1` y `test_signal_changed_laser_switch_set`. Su rojo depende de `MUTANT == "drop_signal_step3"`.

   **Arreglo propuesto:** vincular esas decisiones al texto ejecutado o comprobar explícitamente su correspondencia con producción. Las F de mutantes de producción deben fallar con el texto mutado y sin un flag que introduzca separadamente el defecto en Python.

3. **R21-03 — BLOQUEA — G2 acepta un latch incorrecto y no se comprueba tras cada vaciado elegible.**  
   [.github/tools/test_graph_logic_signals.py:1653](C:/tmp/lfpg-grok/wt-r21t/.github/tools/test_graph_logic_signals.py:1653) y [:1721](C:/tmp/lfpg-grok/wt-r21t/.github/tools/test_graph_logic_signals.py:1721). Inyecté este estado defectuoso: SOURCE de capacidad 50 encendida, entrada de una OR alta, latch de OR cerrado y salida baja; todas las entidades presentes y ningún reintento. `g2_compare` devuelve `[]` y `settle(require_empty=True)` lo acepta. El oráculo aplica primero `closed`, obtenido del propio latch incorrecto, antes de evaluar la OR; tampoco compara los latches.

   Además, conté **cero llamadas a G2** durante A1 y D5 Laser→interruptor→Set. Un nodo irresoluble pendiente también permite `settled_empty=True`, omitiendo G2 por `gate_retry`.

   **Arreglo propuesto:** calcular la lógica esperada independientemente del latch observado, comparar también cada latch y ejecutar la comprobación en cada vaciado elegible. Separar explícitamente “cola vacía” de “estado asentado sin reintentos”.

4. **R21-04 — IMPORTANTE — La cancelación independiente de temporizadores no se prueba.**  
   [.github/tools/test_graph_logic_signals.py:3760](C:/tmp/lfpg-grok/wt-r21t/.github/tools/test_graph_logic_signals.py:3760). Las dos cancelaciones explícitas de B ocurren en **20900 ms con cero temporizadores pendientes**. La aserción final, `a.m_LatchState == (latch_a or a.m_LatchState)`, tampoco exige un resultado independiente. Sustituí `Clock.Remove` por una implementación que **borra todos los temporizadores**: esta fila siguió verde.

   **Arreglo propuesto:** afirmar que A y B tienen callbacks pendientes simultáneamente, cancelar uno, verificar que permanece el otro y comprobar estados y número de ejecuciones esperados.

5. **R21-05 — IMPORTANTE — El ruido deja de seguir el reloj durante el recorrido; C7 llega sin congestión.**  
   [.github/tools/test_graph_logic_signals.py:2913](C:/tmp/lfpg-grok/wt-r21t/.github/tools/test_graph_logic_signals.py:2913) y [:2362](C:/tmp/lfpg-grok/wt-r21t/.github/tools/test_graph_logic_signals.py:2362). La traza del vecino muestra conmutaciones en `19000, 19500, 20000, 20500` ms; las **ocho siguientes ocurren todas en 21000 ms**, durante las comprobaciones de OR/XOR. Las llamadas directas a PDQ cuentan correctamente, pero no ejecutan el ruido periódico de 500 ms durante ese recorrido.

   En C, las cinco primeras pulsaciones tienen ruido pendiente; la última pulsación de Set, correspondiente a C7, llega en **103900 ms con cero nodos del ruido pendientes**.

   **Arreglo propuesto:** mantener las conmutaciones periódicas en el reloj compartido durante el recorrido y afirmar congestión inmediatamente antes de cada pulsación, incluida C7.

6. **R21-06 — IMPORTANTE — El retraso de 1100 ms solo se imprime.**  
   [.github/tools/test_graph_logic_signals.py:2733](C:/tmp/lfpg-grok/wt-r21t/.github/tools/test_graph_logic_signals.py:2733) y [:2764](C:/tmp/lfpg-grok/wt-r21t/.github/tools/test_graph_logic_signals.py:2764). La fila ejecuta Q `1→0→1`, pero después del Reset solo avanza 100 y 200 ms. No llena la cola para retrasar A, no acredita una lectura baja de B durante 1100 ms y exige únicamente cero flips. El mensaje anuncia el caso retrasado aunque no se ejecuta. La fila termina con **`settled_empty=False`** y permanece verde.

   **Arreglo propuesto:** ejecutar separadamente carga normal y retraso real; afirmar deshabilitación, lecturas de B, intervalo bajo ≥1100 ms y exactamente un flip en el segundo caso. Exigir el vaciado final dentro del presupuesto.

7. **R21-07 — IMPORTANTE — La prueba del empate ejecuta medio empate en cada orden.**  
   [.github/tools/test_graph_logic_signals.py:3822](C:/tmp/lfpg-grok/wt-r21t/.github/tools/test_graph_logic_signals.py:3822), relacionado con [:1605](C:/tmp/lfpg-grok/wt-r21t/.github/tools/test_graph_logic_signals.py:1605). Con ambos eventos previstos para 100 ms:

   - Temporizador primero: callback ejecutado, **cero llamadas de PDQ**.
   - PDQ primero: una llamada de PDQ, **callback sin ejecutar y todavía pendiente**.

   Ambas ramas pasan porque las aserciones solo comprueban uno de los eventos. El bucle termina al alcanzar `end`, dejando el otro evento empatado fuera.

   **Arreglo propuesto:** consumir todos los eventos elegibles del instante final y afirmar ambas ejecuciones y su orden exacto en cada variante.

8. **R21-08 — IMPORTANTE — “M sin activar” se comprueba antes de que M pueda leer el resultado del cast nulo.**  
   [.github/tools/test_graph_logic_signals.py:3030](C:/tmp/lfpg-grok/wt-r21t/.github/tools/test_graph_logic_signals.py:3030) y [:3040](C:/tmp/lfpg-grok/wt-r21t/.github/tools/test_graph_logic_signals.py:3040). Después de introducir la entidad cuyo cast falla solo se procesa `gate`. La celda conserva su estado anterior.

   Inyecté una escritura errónea alta en la arista Set de esa pasada, conservando `last_signal_out=False`: **la fila pasó**. Al procesar después `cell`, M quedó con **latch y actividad a true**.

   **Arreglo propuesto:** comprobar el bit que recibe Set y ejecutar el callback de M después del cast, antes de afirmar que permanece desactivada.

9. **R21-09 — IMPORTANTE — Los órdenes añadidos prueban otro circuito y una sola corrida permutacional.**  
   [.github/tools/test_graph_logic_signals.py:2582](C:/tmp/lfpg-grok/wt-r21t/.github/tools/test_graph_logic_signals.py:2582). FIFO/LIFO/`perm` ejecutan una transición AND `11→10` construida con `circuit_a`, **sin Memory Cell ni conexión simultánea a Set**. Por tanto, una captura incorrecta de M bajo esos órdenes queda fuera de la comprobación. Solo hay una corrida `perm`, con la misma semilla fija; no las veinte exigidas en §6.3. Las llamadas nuevas mantienen `require_empty=False`.

   **Arreglo propuesto:** ejecutar el mismo fixture de carrera, incluida M, bajo los órdenes requeridos y veinte corridas permutacionales reproducibles; comprobar los observables aplicables y el vaciado.

En la tabla, «no muerde» significa que alguna parte exigida del testigo queda sin comprobar.

| # | Resultado | Evidencia |
|---|---|---|
| 1 | **No muerde completo** | El guard exacto solicitado provoca rojo; hay positivos/negativos de puerto, habilitación y bit, y los callbacks usan el lector. Otro guard que devuelve siempre false deja 69 verdes: R21-01. |
| 2 | **Muerde** | La vuelta OR A→OR B exige A abierta, A→B alta y B abierta dentro de 11 PDQ. Suprimir sincronización deja A cerrada y falla. NetworkID→`None`, fallando registro y vanilla, también falla; se exige `registry["gate"]` verdadero. |
| 3 | **No muerde** | Identidades por instancia y `LFPG_DeferredRouting` extraído, incluido `RequestPropagate`, están presentes. La cancelación cruzada defectuosa pasa: R21-04. |
| 4 | **No muerde completo** | Las cinco PDQ cuentan llamadas reales y se afirma ruido pendiente en las pulsaciones principales. Faltan periodicidad durante OR/XOR y congestión en C7: R21-05. |
| 5 | **No muerde completo** | Warmup procesa 128 de 140 y deja 12. Se modelan Splitter vacío a 0 y transición con `m_AllocChanged`; sus filas no detectan las regresiones textuales de esas ramas: R21-02. |
| 6 | **No muerde completo** | Tolerancia y baja generalizada muerden por texto real; el bit al encolar usa `ent.on` y conserva el proveedor; `Remove(0)` intercambia con el último. El runner rechaza errores de arnés en F. Sin embargo, las F de Step 3 dependen del flag Python: R21-02. |
| 7 | **No muerde** | Incluye generación virtual, capacidad de SOURCE y autoconsumo, pero admite el latch erróneo y omite vaciados elegibles: R21-03. |
| 8 | **No muerde** | No aparece `or True`. Faltan el retraso ejecutado, ambos eventos del empate, la matriz de órdenes y el vaciado obligatorio: R21-06, 07 y 09. |
| 9 | **No muerde completo** | Hay M conectada a Set y el mutante de cast provoca rojo por `last_signal_out`; M no procesa la pasada que se pretende validar: R21-08. |
| 10 | **Muerde S2; F declarada equivalente** | La fila declara equivalencia funcional y el incremento adicional de `m_RequeueCount` en `:3511`. El recorrido Step 3→`MarkNodeDirty` conserva el rebobinado alternativo. |

Comprobaciones realizadas:

- HEAD fijo en **e06185a**; el delta contiene únicamente el test indicado. Sin modificaciones en disco ni lectura de `REVIEW-*`.
- Ejecución normal: **69 tests, 0 fallos, 0 errores, 0 omitidos**.
- Con fuentes y workflow de **592ebcc**, cargados en memoria: **23 fallos, 0 errores y 12 omitidos**. Los rojos son exactamente A1, A3, A4×2, D5 por Splitter y directo, y S1–S17.
- **D5 Laser→interruptor→Set sí queda verde en la base.** La asignación existe realmente en `592ebcc:scripts/5_Mission/LFPG_ElecGraphImpl.c:2690`, dentro de la transición de `:2688`; `:2756-2758` corresponde al árbol actual. También conservó ese resultado sustituyendo el cold-start del modelo por las ramas leídas en producción.

Límites y suposiciones:

- No ejecuté Enforce ni el juego; esta revisión acredita comportamiento offline del arnés y contraste con fuente.
- La equivalencia funcional **universal** del mutante 10 sigue siendo una suposición fuera de sus F ejecutadas. La prueba que podría tumbarla es un fixture con el objetivo junto al límite de cinco reencolados, comparando diferimiento y observables entre ambas variantes.