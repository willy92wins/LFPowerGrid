<!-- Salida literal de Codex (codex exec, solo lectura), revision de plan de BRIEF-TOGGLE-ASENTADO.md v3. Transcrita por el receptor sin editar. -->

BASE: claude/project-thread-0iolln @ 3167be4b6cb7651baaf1d38bbd2a274fdb439b3a

**Veredicto: NO-APTO.** El algoritmo v3 resuelve los contraejemplos de la v2 para las lecturas notificadas. La afirmación que justifica quitar la relectura —notificación de **cada** cambio de entrada— tiene una excepción verificable en el grafo.

Revisión solo lectura, sin subagentes, modificaciones ni arranque de DayZ.

**Hallazgos**

| ID | Clase | path:line | Secuencia exacta | Salida mala | Cambio mínimo |
|---|---|---|---|---|---|
| TA3-01 | **INVALIDA-DISENO** | [BRIEF-TOGGLE-ASENTADO.md:26](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-TOGGLE-ASENTADO.md:26); [LFPG_ElecGraphImpl.c:4793](C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_ElecGraphImpl.c:4793), `:4796`, `:2786`, `:5044` | Power presente y Toggle bajo desde `0`, gracia vencida. Asignación del puerto Toggle: `0.0009 → 0.0011` en `5000` y vuelta a `0.0009` en `5800`. Cada delta es `0.0002`, menor que `EPSILON=0.001`. Ambos cruces cambian el booleano del puerto sin activar `m_AllocChanged`; con las demás condiciones de propagación falsas, la celda permanece fuera de la cola. | **0 inversiones/OFF**, frente a **1/ON** para ese alto de 800 ms. | Garantizar la notificación cuando cambie el booleano `allocated > EPSILON`, aunque el delta numérico sea menor. Exige ampliar el alcance del brief al grafo y añadir esta integración. Restaurar únicamente una relectura en callbacks no garantiza descubrir un cambio que nunca programó callback. |
| TA3-02 | **CORRECCION-MENOR-DEL-BRIEF** | [BRIEF-TOGGLE-ASENTADO.md:94](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-TOGGLE-ASENTADO.md:94); [test_memorycell_toggle.py:225](C:/tmp/lfpg-grok/wt-nostra-131/.github/tools/test_memorycell_toggle.py:225); [LFPG_MemoryCell.c:202](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:202) | Sustituir únicamente la lectura de Power por `nm.IsPortReceivingPower(m_DeviceId, portToggle)`. Ejecuté la suite actual con esa mutación en memoria. | **8/8 verdes** con Power leyendo el puerto incorrecto. El arnés propuesto admite “trozos” que podrían seguir recibiendo `hasPower/hasToggle` inyectados. | Exigir ejecutar también las lecturas y las guardas del setter; texto exacto debajo. |

**Reproducción de TA3-01.** Usé dos proveedores de una celda con demanda `1`: B mantiene Power; A alimenta Toggle y otro destino cuya demanda es `1`. Con oferta de A `1.0009 → 1.0011 → 1.0009`, el reparto deja para Toggle `0.0009 → 0.0011 → 0.0009`.

Los helpers de reparto, la guarda y escritura de asignación y el predicado de propagación se cargaron del fuente. En ambos cambios obtuve:

```text
m_AllocChanged=false
notificación de oferta=false
DIRTY_INPUT downstream=false
celda en cola=false
```

Es un contraejemplo de estados numéricos admitidos por el grafo; **no he demostrado su generación mediante entidades de producción**. El mecanismo leído sí contradice la garantía universal del brief. Power permanece presente: las consecuencias aceptadas del contrato no cubren esta omisión.

Texto exacto para TA3-02:

> El arnés ejecutará LFPG_SetPowered incluyendo sus guardas y las cuatro lecturas IsPortReceivingPower, con un NetworkManager simulado cuyos puertos controla la fixture. No inyectará directamente hasPower/hasToggle/hasSet/hasReset. Leer Power desde portToggle y añadir un retorno cuando powered == m_PoweredNet deberán hacer fallar los casos de integración.

**1. Modelo, fixtures y retrasos**

Traduje el método propuesto con una cola por entrada, cancelaciones independientes, vaciado previo y reentrada. Cargué el helper real `LFPG_MCellToggleShouldFlip`; F15 obtuvo la gracia ejecutando el fragmento de carga actual.

| Fixtures | Inversiones / latch final; instante puntual |
|---|---|
| F1 | 1/ON; `10483` |
| F2 | 5/ON |
| F3, tres variantes; F4–F7 | 0/OFF |
| F8 | 2/OFF; `1900`, `4950` |
| F9 | 1/ON; `5500` |
| F10 | 1 inversión, enmascarada por Set; ON |
| F11 | 1/ON; `5400` |
| F12 | 0/OFF |
| F13–F14 | 1/ON; `5400` |
| F15, antes/después de la gracia | 0/OFF y 1/ON; `31400` |
| F16 | 1/ON; `5400` |
| F17 | 2/OFF; `5400`, `7400` |
| F18 | 1/ON; `5400`, en ambos órdenes |
| F19 | 0/OFF |
| F20 | 1/ON; `5400` |

Todas coinciden también intercambiando lectura y callback del mismo milisegundo.

Las variantes tardías conservan los resultados pedidos:

- F13, Toggle +401 ms: inversión en `5600`.
- F16, Toggle +401 ms: en `5800`.
- F17, Toggle +3000 ms: en `5600` y `7600`.
- F7, Power +1000 ms: ninguna.
- F14, Toggle +200 ms: en `5500`.

**20 000 historiales**, semilla `0x13103`, 3–10 lecturas, huecos del brief y retrasos aleatorios `0–3000`: **40 000 ejecuciones**, ambos órdenes; **cero discrepancias** de inversiones, latch y cambios asentados contra el oráculo independiente por tramos. Esta comprobación presupone que las lecturas llegan al setter.

**2. Notificación del grafo**

La guarda citada es correcta: [LFPG_ElecGraphImpl.c:2985](C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_ElecGraphImpl.c:2985) impide saltarse una sincronización con `DIRTY_INPUT`. La rama PASSTHROUGH llama a `SetPowered` en `:3207`; [LFPG_IDevice.c:324](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_IDevice.c:324) llega al método virtual.

Eso garantiza la llamada **al procesar ese dirty**, no su generación para cualquier cruce de puerto. TA3-01 muestra una asignación modificada sin `DIRTY_INPUT` y con la celda fuera de la cola. Para los cambios que sí se marcan, `MarkNodeDirty` conserva máscara y encola en `:2089–2120`; el límite de reentrada difiere el trabajo en `:2289` y lo recupera en `:2902`.

**3. Reentrada y `m_FlipPending`**

Funciona según el método propuesto. [LFPG_MemoryCell.c:178](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:178) **no retorna** cuando `powered` coincide: continúa hasta leer puertos en `:202–205`.

`ApplyDue` ya actualizó los asentados antes de reentrar; el segundo vaciado no repite el flanco. La lectura reentrante puede programar candidatos nuevos y después consume el pendiente. Probé ambos niveles cayendo durante esa llamada: **una inversión y pendiente `false`**. No encontré un camino normal que lo deje alto sin consumir.

**4. Borrado**

Firma válida: [lfpg_devicebase.c:580](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/lfpg_devicebase.c:580). `EEDelete` invoca el hook en `:298`, sin exclusión del servidor, antes del desmontaje del lifecycle. La herencia llega por [LFPG_WireOwnerBase.c:33](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_WireOwnerBase.c:33). El patrón SYSTEM propuesto existe en [LFPG_PushButton.c:132](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_PushButton.c:132).

Dejar `LFPG_DeferredRouting` sin limpiar es una **OBSERVACION preexistente expresamente aceptada**, no otro bloqueo.

**5. Falsos verdes**

La v3 exige pruebas que discriminan los fallos anteriores: quitar el vaciado rompe F13/F16/F17 tardías y F7 con Power tarde; temporizador compartido rompe el instante de F13; orden fijo rompe F14 tardía; empate invertido rompe F19. Lo confirmé en el modelo.

Quedan los huecos señalados: lectura real de puertos y puente grafo→setter. Las fixtures del filtro, aunque estén verdes, no acreditan ese puente.

**LO QUE NO PUDE VERIFICAR**

- La implementación v3 y su arnés todavía no existen en esta BASE.
- No ejecuté el grafo completo ni probé la topología de TA3-01 con entidades reales.
- No verifiqué compilación Enforce, despacho nativo de callbacks, borrado efectivo ni comportamiento en partida.
- Árbol limpio y SHA sin cambios al terminar.