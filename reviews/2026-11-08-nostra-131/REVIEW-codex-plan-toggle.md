<!-- Salida literal de Codex (codex exec, solo lectura), revision de plan de BRIEF-TOGGLE-ASENTADO.md. Transcrita por el receptor sin editar. -->

BASE: claude/project-thread-0iolln @ a20d75903e45b473c06095cd9d69f9c26fb4c3a9

**Veredicto: NO-APTO.** F1–F12 y los mutantes dan los resultados anunciados en el modelo traducido. Sin embargo, el método propuesto viola el contrato en dos secuencias adicionales: pierde un pulso válido y aplica una caída posterior de Power retroactivamente a un flanco anterior.

HEAD avanzó durante la revisión de `9bedcc8` a `3fd65c2`, mediante documentación. Verifiqué que `scripts`, `.github` y `config.cpp` siguen idénticos a BASE. No modifiqué archivos, no utilicé subagentes ni arranqué DayZ.

## Hallazgos

Notación: `(t,P,T)`, milisegundos, latch inicial OFF, Set/Reset bajos y gracia vencida.

| ID | Clase | path:line | Secuencia exacta | Salida mala | Cambio mínimo |
|---|---|---|---|---|---|
| TA-01 | **INVALIDA-DISENO** | [BRIEF-TOGGLE-ASENTADO.md:44](/C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-TOGGLE-ASENTADO.md:44), [:48](/C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-TOGGLE-ASENTADO.md:48) | `(0,1,0) → (5000,1,1) → (5300,0,1) → (5600,1,1) → (5800,1,0)`; vaciar cola hasta `6200`. | **0 inversiones, OFF.** Toggle estuvo alto **800 ms** y el único apagón de Power duró **300 ms**: el contrato exige ignorar ese apagón y aceptar el pulso. | **[DESIGN]** Mantener vencimientos por entrada; un cambio de Power no debe cancelar el asentamiento elegible de Toggle. Programar el próximo vencimiento pendiente y asentar únicamente las entradas que hayan cumplido sus 400 ms. |
| TA-02 | **INVALIDA-DISENO** | [BRIEF-TOGGLE-ASENTADO.md:50](/C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-TOGGLE-ASENTADO.md:50), [:52](/C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-TOGGLE-ASENTADO.md:52) | `(0,1,0) → (5000,1,1) → (5100,0,1) → (6000,0,1)` | **0 inversiones, OFF.** Al asentar en `5500`, Power se procesa primero y pone `m_PowerSinceMs=-1`; después se evalúa el flanco fechado en `5000`, cuando Power sí llevaba cinco segundos alto. | **[DESIGN]** Aplicar transiciones asentadas en orden temporal. Power precede a Toggle **en empate**, no ante cualquier transición agrupada. No sustituirlo simplemente por «Toggle primero»: eso rompe F12. |
| TA-03 | **CORRECCION-MENOR-DEL-BRIEF** | [BRIEF-TOGGLE-ASENTADO.md:67](/C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-TOGGLE-ASENTADO.md:67), [:70](/C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-TOGGLE-ASENTADO.md:70); [test_memorycell_toggle.py:238](/C:/tmp/lfpg-grok/wt-nostra-131/.github/tools/test_memorycell_toggle.py:238), [:257](/C:/tmp/lfpg-grok/wt-nostra-131/.github/tools/test_memorycell_toggle.py:257) | Mutar en memoria la constante de producción de rearme a `0`; por separado, sustituir la asignación de gracia tras carga por `0`. Ejecutar los dos módulos actuales. | **11/11 verdes con ambos mutantes.** El arnés fija rearme y gracia por su cuenta. Las fixtures propuestas, todas con gracia vencida, tampoco acreditan el cableado de R3 desde la carga. | Añadir al brief el texto exacto indicado debajo y los casos TA-01/TA-02. |

TA-01 no requiere una oscilación prolongada ni una recuperación que el contrato acepte como pulsación. El temporizador pasa por `5400 → 5700 → 6000 → 6200`; cuando finalmente ejecuta, Toggle ya coincide otra vez con su estado asentado bajo. Se ha perdido todo el alto de 800 ms.

En TA-02, el flanco debía poder asentarse en `5400`; la caída de Power todavía solo llevaba 300 ms. El orden fijo del callback termina evaluando el pasado con un estado posterior. Ambos casos también discrepan de un filtro independiente que acepta cada transición por su propia duración.

Texto exacto propuesto para TA-03:

> El arnés leerá del fuente las constantes y la inicialización utilizadas en producción, y probará la gracia ejecutando `LFPG_OnStoreLoadDevice` con un contexto controlado. Además de F1–F12, incluirá TA-01 y TA-02, primera lectura alta, primera lectura baja real, flanco durante la gracia cuyo callback vence después, flanco exactamente al vencer la gracia y primera alimentación posterior a la gracia. Debe detectar rearme de producción a cero, omisión de la gracia y omisión de la retirada de `LFPG_MCellSettle` al borrar.

## 1. Ejecución de F1–F12 y mutantes

Traduje los pasos propuestos, con cancelación y programación de una única llamada pendiente. El predicado `LFPG_MCellToggleShouldFlip` se cargó del fuente real mediante el traductor existente. Se ejecutaron los callbacks pendientes después de la última lectura.

| Fixture | Inversiones | Latch final | Resultado |
|---|---:|---|---|
| F1 | 1, en `10483` | ON | Coincide |
| F2 | 5 | ON; alterna cinco veces | Coincide |
| F3, Toggle primero / Power primero / simultáneos | 0 / 0 / 0 | OFF | Coincide |
| F4 | 0 | OFF | Coincide |
| F5 | 0 | OFF | Coincide |
| F6 | 0 | OFF | Coincide |
| F7 | 0 | OFF | Coincide |
| F8 | 2, en `1900` y `4950` | OFF | Coincide |
| F9 | 1, en `5500` | ON | Coincide |
| F10 | 1, enmascarada por Set | ON | Coincide |
| F11 | 1, en `5400` | ON | Coincide |
| F12 | 0 | OFF | Coincide |

F2, F3 y F10 no contienen todas sus marcas temporales en el brief. Para hacer la ejecución reproducible:

- **F2:** lectura inicial `(0,1,0)`; para cada `a=10000+3000k`, con `k=0..4`: `(a,1,1)`, `(a+10,0,0)`, `(a+83,1,1)`, `(a+2000,1,0)`, `(a+2010,0,0)`, `(a+2083,1,0)`.
- **F3:** inicio `(0,1,1)`, corte en `5000`, recuperación en `15000`; desfase entre entradas de 50 ms en las variantes no simultáneas.
- **F10:** Set permanece alto desde la lectura inicial hasta después del callback.

| Mutante | Diferencia reproducida |
|---|---|
| Filtro a 0 ms | F8 queda en **1 inversión/ON**; F9 en **0/OFF** |
| Instante compartido entre entradas | F8 queda en **1/ON** |
| Sin R2, incluyendo su guarda de Power no válido | F3, F4–F7 y F12 pasan a **1/ON** |
| Sin duración mínima de R1 | F11 pasa a **2/OFF** |
| Toggle antes que Power | F12 pasa a **1/ON** |

Estos resultados acreditan esas fixtures del **modelo propuesto**. `LFPG_MCellSettle` todavía no existe en el código de BASE.

## 2. CallLater, entidad, borrado y llamada al setter

**La llamada sin parámetros es válida por firma.** `ScriptCallQueue.CallLater` recibe `func fn`, retraso entero, repetición y argumentos opcionales; `Remove` recibe `func fn`. La definición local está en `scripts/2_gamelib/tools.c:61` y `:65`, bajo el árbol vanilla de `C:/Users/guill/OneDrive/Documentos/DayZ Projects`. Coincide con la [fuente oficial de Bohemia](https://github.com/BohemiaInteractive/DayZ-Script-Diff/blob/main/scripts/2_gamelib/tools.c).

`DayZGame.GetCallQueue` devuelve la cola de la categoría solicitada en `3_game/dayzgame.c:1233`; SYSTEM se procesa en `:3026`, fuera del bloque exclusivo de GAMEPLAY. No debe suponerse que un callback SYSTEM espere a que finalice la carga.

**Hoy no se retira `LFPG_DeferredRouting` al borrar la Memory Cell.** Sus `Remove` están en cambios de estado e inicialización, en [MemoryCell:292](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:292) y [:378](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:378). La celda y `LFPG_WireOwnerBase` no implementan `LFPG_OnDeleted`; el hook heredado está vacío en [lfpg_devicebase.c:580](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/lfpg_devicebase.c:580).

El brief contempla expresamente este caso. Debe añadirse la retirada de `LFPG_MCellSettle` en ese hook, que `EEDelete` invoca antes de retirar la entidad del grafo: [lfpg_devicebase.c:298](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/lfpg_devicebase.c:298). La ausencia de limpieza del diferido anterior es una **OBSERVACION preexistente**, no un nuevo incumplimiento del plan.

**No existe el early return sospechado en la Memory Cell.** En [MemoryCell:178](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:178), conservar `m_PoweredNet` solo evita asignarlo; continúa leyendo entradas, aplicando Set/Reset y enrutando. Es distinto de PushButton, SwitchRemote y DoorController.

Por tanto, `LFPG_MCellSettle → LFPG_SetPowered(m_PoweredNet)` puede consumir el pendiente. El enrutado marca trabajo para el grafo en [ElecGraphImpl:5098](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_ElecGraphImpl.c:5098); no ejecuta inmediatamente otro asentamiento. No encontré una recursión síncrona en esa ruta.

## 3. Lectura por puerto, DIRTY_INPUT, primera lectura y carga

**La consulta devuelve el estado almacenado del grafo, no una instantánea garantizada de convergencia completa.** [IsPortReceivingPower:5013](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_ElecGraphImpl.c:5013) comprueba puerto, edge habilitado y `m_AllocatedPower > EPSILON`. No exige que la cola esté vacía ni que todas las ramas correspondan a una propagación terminada.

Releer desde el callback es válido como lectura actual de esas asignaciones. **Que siga igual durante 400 ms no demuestra que el grafo haya terminado.**

El camino normal sí actualiza la celda aunque su booleano agregado de alimentación no cambie:

- Los cambios de asignación fuerzan propagación aguas abajo mediante `DIRTY_INPUT`: [ElecGraphImpl:2786](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_ElecGraphImpl.c:2786), [:2821](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_ElecGraphImpl.c:2821).
- `NodeEntitySyncUnchanged` rechaza saltarse una escritura con `DIRTY_INPUT`: [:2985](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_ElecGraphImpl.c:2985).
- PASSTHROUGH llama al setter: [:3207](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_ElecGraphImpl.c:3207).

Esto acredita la actualización al procesar el nodo. **No garantiza una llamada por cada transición física:** la cola acumula máscaras y puede coalescer cambios.

Controles ejecutados:

| Caso | Resultado del modelo |
|---|---|
| Primera lectura `P=1,T=1` | No invierte; Power empieza a contabilizarse desde esa primera lectura |
| Primera lectura alta; bajo real desde `5000`; subida en `6000` | Invierte en `6400`: R4 funciona |
| Bajo inicial de solo 300 ms | No rearma: coherente con el filtro |
| Gracia hasta `30000`; subida en `29900`; callback en `30300` | No invierte ni revive por mantener Toggle alto |
| Subida exactamente en `30000`, resto válido | Invierte en `30400` |
| Primera alimentación en `31000`; subida en `31800` | No invierte: Power solo llevaba 800 ms |
| Primera alimentación en `31000`; subida en `32000` | Invierte en `32400` |

La gracia sigue fijándose desde la carga en [MemoryCell:492](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:492). No acredita convergencia. Una recuperación inicial tardía que cumpla las reglas después de la gracia puede contar; **no la clasifico como fallo porque el contrato lo permite**.

## 4. Splitter y apagones superiores a 400 ms

En esta rama no existe `LFPG_PROPAGATE_INTERVAL`. El símbolo real es **`LFPG_PROPAGATE_TICK_MS = 100.0`**, en [LFPG_Defines.c:515](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_Defines.c:515). El intervalo nominal no crece con el grafo: crece el número de lotes necesarios.

Los presupuestos son:

- Normal: **64 nodos / 256 visitas a edges**.
- Warmup: **128 / 512**.
- Cola superior a 128: se duplican los presupuestos normales.

Fuentes: [LFPG_Defines.c:446](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_Defines.c:446), [NetworkManagerImpl:2440](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_NetworkManagerImpl.c:2440). Al agotarlos, queda trabajo para otro tick: [ElecGraphImpl:2250](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_ElecGraphImpl.c:2250). También existen diferimientos por límite de reencolado.

Como medida del presupuesto, un vaciado simplificado de **2048 entradas, una visita por nodo y sin limitación adicional por edges**, necesita **17 lotes** con la regla dinámica actual: unos **1,6–1,7 s**, según la fase del primer tick. No es una medición de apagón físico; demuestra que el código no impone una cota global de 400 ms.

**OBSERVACION:** los 73–100 ms medidos no permiten garantizar ese máximo en otros montajes. Un apagón común de 600 ms se acepta como corte real y puede hacer perder una pulsación próxima a la recuperación; eso está expresamente admitido por el contrato.

## 5. Latencia, LED, Door Controller y Set

Los 400 ms empiezan en la lectura cruda pertinente, después del retraso de propagación. El temporizador compartido puede retrasarlos adicionalmente; TA-01 demuestra que incluso puede eliminar el flanco.

El pulso real sigue siendo de **2000 ms**, definido en [LFPG_Defines.c:393](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_Defines.c:393). En condiciones estables deja margen suficiente para asentar Toggle.

**El filtro del flanco no filtra los demás efectos eléctricos:**

- El LED del pulsador sigue `m_PoweredNet` y `m_SwitchOn`: [PushButton:160](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_PushButton.c:160).
- El LED de Power de la celda sigue `m_PoweredNet`; su LED de Toggle permanece apagado: [MemoryCell:440](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:440).
- DoorController abre o cierra según su alimentación efectiva: [DoorController:461](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_DoorController.c:461). Puede seguir viendo apagones durante esa ventana.

**OBSERVACION sobre Set:** se evalúa también cuando el callback consume la inversión. Reproduje:

```text
(0,1,0), latch OFF
5000: Toggle sube y Set está alto → Set deja ON
5200: Set baja
5400: se consume Toggle → OFF
```

Con Set mantenido durante `5400`, termina ON, como F10. Un Set que ya haya desaparecido no enmascara la inversión diferida. Es un efecto temporal del diseño solicitado; F10 solo prueba Set mantenido.

## 6. Qué demuestra el arnés

Ejecuté los dos módulos actuales: **11/11 pasan**. También confirmé en memoria los dos falsos verdes de TA-03.

El arnés propuesto puede detectar TA-01/TA-02 **si incorpora esas secuencias**. F1–F12, por sí solas, no los detectan.

Extraer sentencias reales evita mantener otra implementación del setter, pero no verifica automáticamente:

- El despacho real desde `DIRTY_INPUT`.
- La inicialización y carga si se sustituyen sus estados.
- El preprocesado cliente/servidor.
- La retirada del callback de la celda al borrar.
- La temporización y semántica nativas de la cola.

Las fixtures con resultados literales no son tautológicas por usar fuente extraído. Sí lo sería obtener el resultado esperado ejecutando la misma traducción que se está comprobando. Para esta revisión contrasté los dos fallos con un filtro independiente basado en duración por entrada.

La propuesta de probar activación, corte, vencimiento y borrado de PushButton/SwitchRemote sí mejora R7-04: comprueba efectos, no únicamente presencia de cadenas.

## 7. Compilación Enforce y lado cliente/servidor

**No encontré un impedimento de firma u orden de módulos en lo propuesto.**

- `CallLater(LFPG_MCellSettle, 400, false)` admite un método sin parámetros.
- El método puede declararse normalmente y proteger su cuerpo mediante `#ifdef SERVER`.
- `LFPG_SetPowered` existe en cliente, pero su cuerpo completo está protegido por `SERVER`: [MemoryCell:175](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:175).
- `LFPG_NetworkManager.IsPortReceivingPower` está declarado en **4_World**: [NetworkManager:258](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_NetworkManager.c:258). Su implementación de misión no introduce una referencia ascendente nueva desde la celda.

La firma válida no demuestra las garantías nativas de `Remove` ni del rearme desde dentro del callback.

## 8. Qué puede estar mal en la premisa

**OBSERVACIONES:**

1. **Silencio durante 400 ms y convergencia completa son propiedades distintas.** La asignación almacenada puede permanecer estable mientras espera trabajo aguas arriba.
2. «Cada cambio produce un apagón» es demasiado general. El mecanismo leído depende, entre otras condiciones, de que la demanda supere la salida disponible: [ElecGraphImpl:4735](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_ElecGraphImpl.c:4735).
3. La frase del brief «esto último solo pasa con un grafo que oscila» no queda acreditada. El código también permite retrasos por presupuesto y recuperación desigual. La consecuencia contractual sigue siendo aceptada, cualquiera que sea la causa.
4. El filtro puede acotar qué lecturas cuentan como pulsación; no demuestra que corrija la entrega eléctrica ni el fallo del Splitter.

Ninguna recuperación que el contrato declara indistinguible sustenta el NO-APTO. Lo sustentan TA-01 y TA-02.

## LO QUE NO PUDE VERIFICAR

- Compilación del candidato: todavía es un plan; no existe `LFPG_MCellSettle` en BASE.
- Temporización nativa, cancelación y rearme del mismo callback durante su ejecución.
- Frecuencia de TA-01/TA-02 en un montaje físico concreto.
- Duración máxima real de apagones en grafos grandes; el cálculo presentado mide presupuestos de cola.
- LEDs, puertas y C1–C7 en partida.
- Linter del candidato y suite nueva: todavía no hay implementación sobre la que ejecutarlos.