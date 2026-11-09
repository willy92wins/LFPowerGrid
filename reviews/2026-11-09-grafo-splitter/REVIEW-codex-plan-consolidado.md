<!-- Salida literal de Codex (codex exec, solo lectura, a ciegas). -->

NO APTO

**R22-01 — BLOQUEA: rehabilitar una salida puede publicar una señal antigua y activar otra celda sin alimentación del proveedor.**

Referencias: `reviews/2026-11-09-grafo-splitter/PLAN-CONSOLIDADO.md:97`, `:134`; `scripts/5_Mission/LFPG_ElecGraphImpl.c:5075`, `:5083`, `:5098`; `scripts/4_World/LFPG_MemoryCell.c:235`.

Una salida deshabilitada conserva sus campos. Al rehabilitarla, `SetOutputPortEnabled` cambia el flag y encola al proveedor; no invalida el contenido anterior.

Escenario concreto:

1. La celda A tiene Output deshabilitada, pero esa arista conserva `m_SignalHigh=true` de una activación anterior.
2. A pierde alimentación: entrada real `0`, autoconsumo `1`. Su señal de salida debería ser falsa.
3. A recibe Set por señal, con asignación `0`, permitido por el contrato propuesto. Su callback activa el latch y rehabilita Output.
4. B, alimentada aparte y ya encolada, procesa antes del reproceso de A. Lee la arista rehabilitada todavía con señal alta y activa su propio Set.
5. A publica después señal baja. B conserva el latch verdadero: Set actúa por nivel y su retirada no deshace la activación.

Ejecuté este testigo en memoria con el callback real de Memory Cell, las consultas propuestas y el fragmento real de habilitación. Resultado: B termina activada. Invalidando el bit al rehabilitar la arista, B permanece desactivada.

**Arreglo propuesto:** invalidar la señal anterior al rehabilitar una salida y reconstruirla mediante el proveedor antes de admitirla como alta. Mantener intacta la política de `m_AllocatedPower`. Esto exige ampliar el cambio previsto a `SetOutputPortEnabled`. Añadir el caso deshabilitar → perder alimentación → rehabilitar → consumidor antes del proveedor, incluyendo el mutante que omite la invalidación.

**R22-02 — BLOQUEA: la tolerancia retenida puede producir un pulso falso al salir de `(0,0)`.**

Referencias: `reviews/2026-11-09-grafo-splitter/PLAN-CONSOLIDADO.md:181`; `scripts/4_World/LFPG_LogicGate.c:172`, `:186`, `:217`; `scripts/5_Mission/LFPG_ElecGraphImpl.c:2525`, `:2821`, `:2884`.

El argumento de §5.5 solo considera el instante `(0,0)`. Falta la siguiente transición:

- AND estaba abierta en `(1,1)`.
- Recibe una evaluación `(0,0)`: conserva `m_GateOpen=true`, pone `m_BrownoutHeld=true` y **no solicita otra evaluación propia**.
- Llega `(1,0)`. Antes del callback, el pre-gate sigue leyendo la compuerta abierta. Ahora `signalIn=true`, por lo que el contrato publica señal alta.
- Step 3 encola al consumidor. Después, Step 5 evalúa AND, cierra el latch y encola su reproceso detrás del consumidor.
- Una Memory Cell conectada por Set captura ese alto y conserva una activación incorrecta cuando posteriormente baja.

Un montaje que permite mantener los floats a cero es G de `50` con otra carga de `60`, Splitter e interruptores A/B hacia AND. Las señales siguen altas durante el all-off según §2.1. Tras apagar ambos interruptores, resetear una Memory Cell sin carga —Power independiente de `1`— y encender solo A, AND debe continuar falsa.

El testigo de callbacks extraídos produjo `AND final=false`, pero `Memory latch=true`. Retirando la tolerancia, la celda permaneció falsa.

**Arreglo propuesto:** retirar la retención al cambiar a lectura por señal, como proponía el plan Codex. Añadir la transición `11 → 00 → 10` con un consumidor Set, comprobando también los altos transitorios.

**Límite:** ejecuté el testigo local, no el PDQ completo. La secuencia presupone una sola evaluación oscura antes de `10`. Para descartarla habría que demostrar que el planificador fuerza siempre una segunda evaluación antes de esa transición; el callback leído no la solicita.

**R22-03 — IMPORTANTE: la fidelidad exigida al arnés omite los presupuestos que determinan las épocas y los temporizadores.**

Referencias: `reviews/2026-11-09-grafo-splitter/PLAN-CONSOLIDADO.md:223`, `:238`, `:254`; `scripts/5_Mission/LFPG_ElecGraphImpl.c:2250`; `scripts/3_Game/LFPG_Defines.c:465`, `:471`, `:506`.

§6.3 exige el límite de cinco reencolados, pero no exige reproducir los límites de **64 nodos y 256 visitas de arista por llamada**. Después interpreta cada época como 100 ms y pide completar el corte en la misma época.

Contraejemplo: G más 64 Splitters en cadena, con demandas conocidas y sin carga. Son 65 nodos, dentro del máximo permitido de 256. Al apagar G, el último Splitter no puede procesarse en esa primera época; las visitas y reprocesos pueden retrasarlo todavía más. Un arnés que drene toda la cola sin presupuestos dará una convergencia temporal imposible en producción.

Esto afecta también a la interacción con los filtros de 400 ms. Además, `test_graph_perf_probe.py:172` comprueba instrumentación estructural; su verde no mide el coste añadido.

**Arreglo propuesto:** incorporar ambos presupuestos, conservar el sufijo pendiente y compartir reloj con los temporizadores. Añadir pruebas que agoten cada presupuesto y comprobar recuperación posterior. Definir el corte mediante lecturas efectivas bajas tras propagación acotada. En aristas deshabilitadas, tampoco exigir campos almacenados a cero: el código conserva expresamente la asignación anterior.

**Comprobado:** lectura íntegra de AGENTS y los cuatro documentos solicitados; fuentes relevantes de propagación, asignación, compuertas, routing, consultas, baterías y arneses. Los commits posteriores a `592ebcc` solo añaden esos documentos. Pasaron **98 tests del grafo y 10 de Memory Cell**, ambos con código 0. Ejecuté los testigos descritos y sus variantes correctoras exclusivamente en memoria.

**No comprobado:** ejecución Enforce, PDQ completo con la implementación propuesta, partida, rendimiento medido ni los nuevos tests y mutantes todavía inexistentes. No escribí archivos ni realicé commits.
