APTO CON CAMBIOS

**R21-01 — IMPORTANTE — Los 62 tests pueden pasar con el lector de producción inutilizado.**

Referencias: `.github/tools/test_graph_logic_signals.py:308`, `:793`, `:3056`; `scripts/5_Mission/LFPG_ElecGraphImpl.c:5274`.

Testigo ejecutado íntegramente en memoria: añadí al comienzo de `IsPortReceivingSignal`:

```c
if (deviceId != "")
    return false;
```

Resultado: **62 tests, 0 fallos, 0 errores, 0 omitidos**. El modelo sigue leyendo su propia implementación Python y S6 comprueba cadenas presentes, sin detectar ese retorno.

Con XOR(1,0), id no vacío y señal alta sin flujo —A4—, ese lector devuelve `(false,false)` y el callback de producción cierra la puerta; el modelo conserva el resultado correcto y aprueba.

**Arreglo propuesto:** ejecutar también el lector extraído del fuente, con positivos y negativos de puerto, habilitación y bit; conectar los callbacks a ese lector. Este es un defecto concreto del arreglo que la suite actual no vería.

**R21-02 — IMPORTANTE — La recuperación aprueba sin ejecutar el callback recuperado.**

Referencias: `.github/tools/test_graph_logic_signals.py:2771`, `:2782`, `:2793`, `:2681`.

La fila empieza con AND(1,1) y latch `true`; pierde la entidad sin cambiar las entradas y después afirma nuevamente `true`. Suprimí exclusivamente la sincronización del nodo recuperado, con máscara `2`: **la fila siguió pasando**, con el latch antiguo `true`. No prueba que el callback restaure la tabla.

Además, la fila que anuncia recuperación por NetworkID solo hace fallar `FindById`: `resolve_vanilla` todavía devuelve la entidad. Sustituí `resolve_netid` por una función que devuelve siempre `None`: **la fila siguió pasando**.

**Arreglo propuesto:** ejecutar el OR A → OR B especificado, con A inicialmente cerrado e irresoluble; devolver únicamente el registro y exigir A abierto, A→B alta y B abierto. Forzar también el fallo de vanilla en la fila de NetworkID y comprobar el registro obtenido.

**R21-03 — IMPORTANTE — Los temporizadores de distintas celdas se pisan.**

Referencias: `.github/tools/test_graph_logic_signals.py:701`, `:702`, `:703`, `:350`; `.github/tools/test_memorycell_toggle.py:30`.

Todas las celdas reutilizan los mismos objetos `CB_POWER`, `CB_TOGGLE` y `CB_DEFER`. Cada `attach_cell` sobrescribe sus entradas en `clock.dispatch`; `Remove` también identifica los temporizadores únicamente por ese objeto compartido.

Testigo: A con Power asentado, `PowerSinceMs=0`, `ToggleLowSinceMs=0`, gracia terminada y Toggle inicialmente bajo. Toggle sube en `2000 ms` y permanece alto hasta `2400 ms`:

- Solo A: latch de A `true`.
- Añadiendo B desconectada después de A: latch de A `false`, Toggle de A sin asentar.

La presencia de B altera el resultado de A. En producción los métodos son de cada instancia.

También se sustituye `LFPG_DeferredRouting` por `apply_routing`, perdiendo su `RequestPropagate` de `scripts/4_World/LFPG_MemoryCell.c:440`.

**Arreglo propuesto:** identidades de callback por instancia y ejecución del diferido completo; comprobar dos celdas con temporizadores simultáneos y cancelaciones independientes.

**R21-04 — IMPORTANTE — C se pulsa después de vaciar la congestión; los cinco ticks del vecino no se cuentan correctamente.**

Referencias: `.github/tools/test_graph_logic_signals.py:1906`, `:1913`, `:2210`, `:2213`, `:2737`.

`pulse_noise_tree` procesa hasta 16 llamadas de PDQ y termina al vaciar. Después se llama a `press_push`.

Instrumenté las seis pulsaciones de C congestionada: **todas empezaron con cola total `0` y nodos de ruido pendientes `0`**. La primera llegó a `50400 ms`. El test pasa sin enfrentar la pulsación a la congestión exigida.

En el vecino, una iteración `t=0` contiene ese mismo helper. En la ejecución observada consumió tres llamadas de PDQ; el supuesto límite de cinco iteraciones permitiría aprobar una transición que necesitara seis llamadas dentro del helper.

**Arreglo propuesto:** separar la conmutación del ruido de su procesamiento, afirmar ruido pendiente cuando llega cada pulsación y contar llamadas reales mediante el contador del PDQ. Programar las conmutaciones de 500 ms sobre el reloj compartido.

**R21-05 — IMPORTANTE — El modelo cambia presupuestos, cold-start y disparadores de producción.**

Referencias: `.github/tools/test_graph_logic_signals.py:1377`, `:1380`, `:879`, `:1143`; `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2448`; `scripts/5_Mission/LFPG_ElecGraphImpl.c:4851`, `:4890`, `:2758`.

Tres divergencias concretas:

- **Warmup:** con 140 consumidores pendientes, el modelo procesa **140 y devuelve 0**. Producción usa 128 nodos durante warmup y deja **12 pendientes**; solo duplica presupuestos en la rama alternativa sin warmup.
- **Cold-start:** G de 50 → Splitter vacío, consumo 0, demanda desconocida y publicación anterior 0. `raw_demand` devuelve **50**; producción devuelve **0**, porque no hay salida habilitada. El modelo también consulta compuertas vivas y recorre demandas descendientes donde producción usa el estado cacheado y su fallback de capacidad.
- **Transición de compuerta:** Laser con entrada 5, consumo 5, demanda 5→5 y asignación 0→0, cacheado cerrado pero vivo abierto. Producción pone `m_AllocChanged=true`; el modelo lo omite expresamente. El testigo produjo `m_AllocChanged=false`, haciendo que una propagación atribuida exclusivamente a señal no reproduzca los disparadores reales.

**Arreglo propuesto:** reproducir esas ramas exactamente y añadir comprobaciones contra sus resultados de producción, antes de usar el modelo para certificar convergencia o aislar mutantes.

**R21-06 — IMPORTANTE — Varias muertes de mutantes proceden de otro defecto, y el runner acepta errores de extracción como evidencia.**

Referencias: `.github/tools/test_graph_logic_signals.py:3353`, `:3396`, `:770`, `:931`, `:1331`, `:3512`, `:3523`, `:3658`.

Testigos:

- «Volver a poner la tolerancia» solo añade una declaración local `bool m_BrownoutHeld`; no restaura el bloque de 592ebcc. Ejecutando ese texto sin el flag auxiliar, **transiciones y B1 pasan**. El rojo funcional lo fabrica el flag, que retiene el latch desde Python.
- «Baja en todo `m_IsGated`» cambia la condición de encolar un reintento a `&& false`; el texto no baja la señal. El flag Python sí cierra la compuerta y altera incluso `allocAvail`.
- El mutante de publicación al encolar escribe siempre `true` y después impide escribir al proveedor. Implementé el defecto anunciado —publicar `ent.on` al encolar, manteniendo el procesamiento del proveedor—: **`test_race_and_mb_enqueue_bit` pasa**. Esa fila usa `[B, AND, M]`, no la lectura previa en `[AND, M, B]` exigida.
- `retry_remove` usa `pop()` del final. Producción leería primero `[0]` y después intercambiaría el último al ejecutar `Remove(0)`. Son recorridos distintos.
- Hice que todas las ejecuciones F lanzaran `AssertionError("error de arnes: extraction failed before behavior")`: **el runner de los 30 mutantes aprobó**. Sus `except AssertionError` no distinguen instrumentación inválida de una aserción funcional.

**Arreglo propuesto:** separar errores de instrumentación de fallos de expectativas, ejecutar las transformaciones reales y mantener cada mutante del modelo limitado al defecto anunciado.

**R21-07 — IMPORTANTE — El oráculo G2 contradice la tabla que pretende verificar.**

Referencia: `.github/tools/test_graph_logic_signals.py:1606`.

Testigos asentados, con cola vacía:

- PASSTHROUGH con generación virtual 10, consumo 0 y lámpara de 10: asignación `10`, señal `true`; el oráculo exige `false`.
- SOURCE encendido, capacidad `0`: señal `false`; el oráculo exige `true`.

El oráculo omite generación virtual, capacidad positiva de SOURCE y condiciones de autoconsumo. Una prueba correcta de esos estados obtiene un rojo falso. Tampoco se aplica universalmente después de los vaciados de los fixtures.

**Arreglo propuesto:** completar independientemente la tabla de §2.1 y aplicarla tras cada vaciado elegible, comprobando también latch de puertas y ausencia de reintentos pendientes.

**R21-08 — IMPORTANTE — Hay filas anunciadas que no ejecutan su escenario y una aserción tautológica.**

Referencias: `.github/tools/test_graph_logic_signals.py:2562`, `:2586`, `:2454`, `:3560`.

La salida rehabilitada con retraso empieza con Q=`false`, termina con Q=`false` y solo contiene una celda. No construye B en Toggle, no ejecuta Q `1→0→1`, no genera el retraso de 1100 ms ni cuenta sus flips. Aun con el arreglo imprime «(ii) cola llena n/a en base» y aprueba.

El bucle que anuncia FIFO/LIFO/permutaciones únicamente añade texto al informe. No ejecuta esos órdenes. Tampoco se ejecuta el empate de temporizadores con PDQ primero.

`assertTrue(q.settled_empty or True)` aprueba igualmente con `settled_empty=False`; numerosos fixtures usan además `require_empty=False`.

**Arreglo propuesto:** implementar los escenarios y órdenes prometidos, afirmar sus precondiciones y convertir la falta de vaciado dentro del presupuesto en fallo.

Sobre los seis puntos de §10:

| Punto | Producción | Test |
|---|---|---|
| 1. `DIRTY_INPUT` y latch recuperado | `LFPG_ElecGraphImpl.c:3207` usa la máscara exigida. | La aserción existe, pero aprueba sin callback: R21-02. Falta el OR A→B. |
| 2. Cadencia, rotación y límite posterior | `:3172` comprueba cadencia, `:3174` registra la ronda, `:3177` limita y `:3186` usa `RemoveOrdered(0)`. | Mide antes y después de recuperar. Los mutantes de cadencia/tope alteran ambas propiedades simultáneamente; `Remove` está mal representado. |
| 3. Pendiente después del barrido/reintento | Recuento en `:2261` y `:2991`, antes de sonda y retorno. | Afirma `remaining=1`, warmup activo y sonda sin asentar en `.github/tools/test_graph_logic_signals.py:2946`. La fidelidad general del warmup falla: R21-05. |
| 4. Árbol admisible y presupuesto cruzado | Splitter declara `output_1..3` en `scripts/4_World/LFPG_Splitter.c:55`. El árbol usa 40 Splitters y 81 consumidores. | Cruce observado `[63,64,11]`; C recibe las pulsaciones después del vaciado. El límite de cinco ticks tampoco mide llamadas reales: R21-04. |
| 5. `Cast` nulo con Splitter y `allocAvail` | El pre-gate conserva el reparto de la entidad resuelta y la rama lógica publica baja. | Usa un stub de Splitter abierto y afirma `allocAvail=10` en `.github/tools/test_graph_logic_signals.py:2837`; también señal baja y reintento pendiente. No incluye la Memory Cell del fixture de §6.4. |
| 6. Once posiciones | La rotación conserva el orden. | `range(11)` en `.github/tools/test_graph_logic_signals.py:2844` prueba las once posiciones. El mutante funcional de `Remove(0)` no reproduce su operación real. |

Respecto a los mutantes de §6.6, estos son los resultados del runner y las reservas de fidelidad. **S/F significa que falló una aserción; R21-06 impide equiparar automáticamente ese resultado a una mutación válida.**

| Mutante | Resultado comprobado |
|---|---|
| Puertas → flujo | S7; F A3, A4 y B1 |
| Set → flujo | S7; F D5 |
| Power → señal | S7; F Power con asignación 0 |
| Escritura dentro de `!overloaded` | S1; F sobrecarga |
| Sin señal en Step 3 | S2; F ambos anclajes; faltan las comprobaciones completas y órdenes exigidos |
| Sin señal en reproceso de época | S2; F no muerde. `MarkNodeDirty` ya rebobina en producción (`LFPG_ElecGraphImpl.c:2126`) |
| Compuerta cacheada | S5; F primera pasada D5; el flag altera además el reparto |
| Sin autoconsumo | F Laser <5 |
| Señal atada a sobrecarga | F sobrecarga |
| Step 1 sin habilitación | S4; F salida deshabilitada |
| Lector sin puerto | S6; F XOR |
| Constructor alto | S8; F inicialización |
| Sin limpieza al rehabilitar | S10; F rehabilitación |
| Tolerancia antigua | S11; **F artificial**, no ejecuta el bloque antiguo |
| Puerta publicando `signalIn` | S12; F latch viejo y carrera |
| Sin presupuesto de nodos | F presupuesto de nodos |
| Sin presupuesto de aristas | F; quitar solamente el límite también falla. Sobra el retorno adicional del mutante |
| Pre-gate solo registro | S13; F recuperación; la fila no demuestra NetworkID |
| Sin entidad, latch cacheado | S14; F entidad irresoluble |
| Sin reintento en vacío | S16; F vuelta de entidad |
| Baja de todo nodo con compuerta | S14; **F distinto del cambio textual** |
| Sin `Cast` | S14; el F sustituye la rama por otro comportamiento Python; no prueba ejecución de ese texto Enforce |
| Sin cadencia | S16; F contaminado también por retirar el tope |
| Sin tope | S16; F contaminado también por retirar la cadencia |
| Sin rotación | S16; F recuperación acotada |
| `Remove(0)` | S16; F sobre otro recorrido de la lista |
| `DIRTY_INTERNAL` | S16; F ausente, declarado. El borrado de la última entidad sincronizada explica la equivalencia |
| Pendiente contado antes | S17; F retorno, warmup y sonda |
| Bit escrito al encolar | F sobre un bit permanentemente alto; **el defecto anunciado sobrevive** |
| Barrido incondicional | F barrido del modelo |

He comprobado el diff `592ebcc..bf2cb33`, HEAD fijo y árbol sin cambios. Los seis ficheros de producción se mantienen dentro de la tabla de §3.1; no encontré una modificación de producción fuera de ella. Revisé declaraciones y accesos nuevos, ámbitos, redeclaraciones, operadores prohibidos y visibilidad entre módulos; no identifiqué un defecto concreto de compilación en el diff.

Validaciones ejecutadas sin modificar fuentes:

- Suite nueva: **62/62**, salida 0.
- `test_graph_*.py`: **160/160**, salida 0.
- Memory Cell: **10/10**, salida 0.
- Supervivencia del pulso: **4/4**, salida 0.
- Linter sobre base virtual obtenida de los blobs y sobre HEAD: **268 ficheros, 0 errores y 48 warnings en ambos; delta 0**, salida 2.

No he demostrado un **BLOQUEA en producción** según la regla de §10. Las carencias anteriores afectan a la prueba presentada para aceptarla.

Queda como **suposición** que el código compila y se comporta así en DayZ: el entorno no proporciona el árbol vanilla, y el linter omitió las comprobaciones de clases y métodos nativos correspondientes. Lo tumbaría un arranque servidor/cliente con este contenido desplegado y la matriz in-game de la fase 3. También siguen pendientes las medidas de coste, latencia física y persistencia/sincronización en partida. No arranqué el juego, no abrí `REVIEW-*`, no escribí archivos ni hice commits.