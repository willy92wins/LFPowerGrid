<!-- Salida literal de Codex (codex exec, solo lectura, a ciegas). -->

NO APTO

**R22-V2-01 — BLOQUEA: quitar la tolerancia no impide publicar un alto con la tabla actual falsa.**

Referencias: `reviews/2026-11-09-grafo-splitter/PLAN-CONSOLIDADO.md:206`; `scripts/4_World/LFPG_LogicGate.c:125`, `:145`, `:217`; `scripts/5_Mission/LFPG_ElecGraphImpl.c:888`, `:1513`, `:2491`, `:2525`, `:2821`, `:2884`; `scripts/4_World/LFPG_MemoryCell.c:235`.

El pre-gate lee «en vivo» un **latch de la lectura anterior**. El callback que evalúa las entradas actuales llega después de publicar las salidas.

Escenario concreto, sin tolerancia:

1. G de `50` alimenta Splitter → interruptores A/B → AND. Estado asentado `11`, `m_GateOpen=true`, salida de AND sin cable.
2. M está desactivada, con Power independiente de `1`, sin carga ni Reset.
3. Antes del siguiente tick, se apaga B y se conecta AND → M.Set. La arista nueva nace con señal `false`; queda una cola admisible `[B, AND, M]`.
4. B publica baja. AND recibe ahora `10`, pero su pre-gate todavía lee `m_GateOpen=true`. El contrato calcula `signalIn=true` y publica señal alta en la arista nueva.
5. El callback de AND cierra correctamente el latch y solicita su reproceso detrás de M. M lee antes Set alto y queda activada. La posterior señal baja no deshace Set.

**Resultado del testigo en memoria:** `AND=false`, `M=true`. Publicando `AND(1,0)=false` antes del callback, M permanece falsa. Ejecuté callbacks reales extraídos, con el delta v2 aplicado en memoria; no ejecuté el PDQ completo.

**Arreglo propuesto [DESIGN]:** calcular la señal de salida de AND/OR/XOR con sus dos entradas actuales y `LFPG_EvaluateGateLogic` **antes de publicarla**. Para ese cálculo no basta combinar `signalIn` con el latch anterior. Mantener separado el reparto energético. Añadir el caso anterior y reconstrucción con la puerta procesada **entre** sus proveedores.

**R22-V2-02 — IMPORTANTE: la cadena larga no demuestra el límite de nodos y la descripción del límite de aristas es inexacta.**

Referencias: `reviews/2026-11-09-grafo-splitter/PLAN-CONSOLIDADO.md:260`, `:297`, `:342`; `scripts/5_Mission/LFPG_ElecGraphImpl.c:2250`, `:2317`, `:4400`, `:4587`, `:4751`; `scripts/3_Game/LFPG_Defines.c:465`, `:471`.

La cadena agota las visitas de arista antes de alcanzar 64 nodos. Un tramo ordinario propagando un cambio consume al menos diez visitas contando evaluación, asignación, ofertas y propagación. En el cálculo aislado con coste `10` por nodo:

- Presupuestos `64/256`: procesa `26` nodos, contabiliza `260` visitas y conserva el resto.
- Eliminando solamente el límite de nodos: produce exactamente lo mismo.

Así, la fixture puede pasar aunque el arnés ignore el presupuesto de 64 nodos.

La diferencia aparece con una entrada independiente: **65 CONSUMER pendientes, cada uno con una entrada y sin consultas adicionales en su callback**. Producción procesa `64`, visita `64` aristas y deja `1` pendiente; el modelo sin límite procesa `65` y deja `0`.

Además, producción comprueba el presupuesto de aristas **antes de empezar un nodo**. No garantiza «como mucho 256 visitas»: puede terminar con `260`. Un arnés que corte dentro del nodo para imponer ese techo tampoco reproduce producción.

**Arreglo propuesto:** pruebas independientes que agoten cada presupuesto; mutantes que eliminen cada límite por separado; y corte entre nodos, conservando el contador completo y la cola pendiente.

**Cierre de mis hallazgos anteriores**

| Hallazgo | Dictamen sobre el cierre |
|---|---|
| **R22-01 — bit antiguo al rehabilitar** | **Correcto para el escenario original.** Ejecuté ambas variantes: v1 activa B; la invalidación v2 deja B desactivada. La asignación conservó el valor `7`. También comprobé idempotencia cuando el flag no cambia. |
| **R22-02 — pulso al salir del 00 retenido** | **Correcto para el testigo original.** Retirar la tolerancia elimina ese estado retenido. La garantía más amplia de ausencia de altos falsos no queda cerrada: véase R22-V2-01. |
| **R22-03 — presupuestos y reloj** | **Parcial.** Se incorporan los presupuestos, el reloj compartido y el corte por lecturas; se corrige la exigencia sobre asignaciones deshabilitadas y se distingue instrumentación de rendimiento. Falta demostrar separadamente ambos límites y respetar su punto de corte: R22-V2-02. |

**Las cinco preguntas de §10**

1. **¿Queda un alto falso o un latch distinto de la tabla?**  
   Queda el alto falso de R22-V2-01. Después del callback efectivo, comprobé **24/24 combinaciones** de AND/OR/XOR, entradas y latch anterior: el latch coincide con la tabla. El defecto ocurre antes de ese callback.

2. **¿La limpieza puede fabricar un flanco o borrar una señal buena?**  
   No detecté un ascenso causado directamente por la limpieza: escribe baja, y una llamada idempotente conserva el bit. No doy por demostrada su inocuidad temporal bajo congestión. Prueba pendiente: A.Output → B.Toggle, Power de B estable, deshabilitación de `100 ms` y publicación posterior de A retrasada más de `1000 ms`. Un flip de B demostraría que el retraso convirtió el corte breve en un rearme efectivo.

3. **¿Bastan presupuestos y reloj compartido?**  
   No como prueba de fidelidad: falta el aislamiento señalado en R22-V2-02. Además, `100 ms` representa el tick normal; existen llamadas directas a PDQ en `scripts/5_Mission/LFPG_NetworkManagerImpl.c:865`, `:3158` y `:3722`. Para reconstrucción, el reloj debe representar esos eventos, sin añadir automáticamente 100 ms por llamada. Deben probarse también ambos órdenes callback/timer en el límite de `400 ms`.

4. **¿Muerden los dos anclajes de `m_SignalChanged`?**  
   **A4 depende del orden.** El interruptor B apagado conserva demanda de sonda `1`; al cortar G, su entrada cambia `1→0` y despierta al XOR mediante `inputChanged`. En el testigo compuesto con callbacks reales:
   
   - `A → B → XOR`: el mutante termina correctamente cerrado.
   - `B → XOR → A`: el mutante termina abierto; con `m_SignalChanged`, cerrado.
   
   Debe fijarse ese orden causal; comprobar únicamente los cinco disparadores de A no basta. **D5 intermedio es un anclaje consistente:** los helpers reales dieron entrada, demanda y asignación del interruptor `0→0`, con demanda de celda `1` y reparto `[0,1]` entre interruptor y Power. No certifico todavía la muerte integrada del mutante: el nuevo arnés no existe.

5. **¿B5 declarado contradice el brief?**  
   No encontré una contradicción expresa. `BRIEF-GRAFO.md:51` protege el all-off y el reparto; sus criterios no exigen señal lógica baja durante sobrecarga. La declaración debe entenderse para la alimentación concreta: con Power independiente, la prohibición general de Toggle no se deduce del all-off de otra fuente.

**Defecto que los tests de §6 pueden dejar pasar:** la activación permanente de M al conectar una arista nueva mientras AND conserva un latch anterior abierto. Las transiciones propuestas pasan por `00`; la reconstrucción fuerza la puerta antes de sus proveedores. Ninguna exige ese estado intermedio concreto.

**Premisa que falla:** eliminar la retención de `00` no convierte el latch leído «en vivo» en una evaluación combinacional actual. `LFPG_IsGateOpen()` devuelve `m_GateOpen` (`LFPG_LogicGate.c:125`); todavía puede representar `11` cuando las entradas ya son `10`.

**Comprobado y límites**

- Contrasté el delta v1→v2, mi revisión anterior, el brief, AGENTS/CLAUDE y las anclas de código y arneses citadas. Confirmé HEAD `1714379` y que su diferencia con `592ebcc` solo afecta a `reviews/`.
- Pasaron **98 tests del grafo, 10 de Memory Cell y 4 de pulsadores**, con salida `0`. Son pruebas de la base actual.
- Los testigos y variantes correctoras se ejecutaron exclusivamente en memoria. No ejecuté Enforce, el PDQ completo propuesto, partida ni mediciones de rendimiento. El nuevo `test_graph_logic_signals.py` todavía no existe.
- No leí la revisión ajena, no escribí archivos ni modifiqué el árbol.
