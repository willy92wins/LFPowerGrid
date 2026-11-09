<!-- Salida literal de Codex (codex exec, solo lectura), revision de plan de BRIEF-TOGGLE-ASENTADO.md v2. Transcrita por el receptor sin editar. -->

BASE: claude/project-thread-0iolln @ 5bbdd21faba78eadbe2c5a79abe66d04b63d6d8d

**Veredicto: NO-APTO.** La v2 corrige el temporizador compartido y el orden fijo de la v1, pero puede sobrescribir cambios que ya cumplieron 400 ms antes de aplicarlos. Eso pierde pulsaciones válidas y admite otras que incumplen R2.

Revisión exclusivamente de lectura. Traduje el método propuesto a Python, con un temporizador por entrada, cancelaciones independientes y entregas retrasadas. Cargué `LFPG_MCellToggleShouldFlip` del fuente real y contrasté los resultados con un oráculo independiente basado en la duración de cada tramo. **La implementación nueva todavía no existe en esta BASE.**

**Hallazgos**

| ID | Clase | path:line | Secuencia exacta | Salida mala | Cambio mínimo |
|---|---|---|---|---|---|
| TA2-01 | **INVALIDA-DISENO** | [BRIEF-TOGGLE-ASENTADO.md:49](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-TOGGLE-ASENTADO.md:49), [:58](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-TOGGLE-ASENTADO.md:58), [:70](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-TOGGLE-ASENTADO.md:70) | F13: `(0,1,0) → (5000,1,1) → (5300,0,1) → (5600,1,1) → (5800,1,0)`. Retrasar el callback de Toggle de `5400` a `5801`. La bajada de `5800` sustituye el candidato anterior y cancela su callback. | **0 inversiones / OFF**, frente a **1 / ON**. Toggle estuvo alto 800 ms; Power solo cayó 300 ms. No es una consecuencia aceptada por el contrato. | **[DESIGN]** Antes de sobrescribir un `Raw/ChangedMs`, conservar y aplicar el candidato anterior si ya cumplió 400 ms. Hacerlo también en la relectura del callback y ordenar los candidatos de ambas entradas por sus instantes. Ordenar únicamente los `due` calculados *después* de sobrescribirlos no basta. |
| TA2-02 | **CORRECCION-MENOR-DEL-BRIEF** | [BRIEF-TOGGLE-ASENTADO.md:83](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-TOGGLE-ASENTADO.md:83), [:104](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-TOGGLE-ASENTADO.md:104); [LFPG_MemoryCell.c:492](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:492) | Sustituir únicamente la asignación de gracia durante la carga por `m_ToggleGraceUntilMs = 0;`, manteniendo constantes e inicializadores. F15 sigue pudiendo recibir desde la fixture una gracia hasta `30000`. | La integración de carga puede estar rota con F15 verde. **Comprobado en la suite actual: 11/11 verdes** con esa mutación en memoria. La v2 no exige ejecutar la carga ni probar el borrado de los dos temporizadores de la celda. | Incorporar el texto exacto siguiente. |

Texto exacto propuesto para TA2-02:

> El arnés ejecutará LFPG_OnStoreLoadDevice con reloj y contexto controlados; F15 no inyectará m_ToggleGraceUntilMs. Debe fallar al sustituir únicamente la asignación de gracia durante la carga por cero. La cola simulada ejecutará la programación, cancelación y wrappers extraídos del fuente. Ejecutará LFPG_OnDeleted de la celda con ambos temporizadores de asentamiento pendientes y comprobará que ninguno permanece ni se despacha al avanzar la cola; omitir cualquiera de los dos Remove debe hacer fallar el test. Se añadirán las secuencias de TA2-01, sus variantes de relectura y los dos órdenes entre lectura y callback cuando coincidan exactamente en el límite de 400 ms.

**1. Cierre de TA-01, TA-02 y TA-03**

| Hallazgo anterior | Estado |
|---|---|
| **TA-01** | Corregida la causa original: Power ya no cancela el temporizador de Toggle. F13 puntual da **1/ON**. **No queda cerrado bajo la entrega tardía exigida aquí**: F13 con Toggle 401 ms tarde da **0/OFF**, por TA2-01. |
| **TA-02** | **Cerrado en el método propuesto.** Procesa por fecha; Power precede solo en empate. F14 con Toggle 200 ms tarde conserva **1/ON**. |
| **TA-03** | **Parcial.** Leer constantes e inicializadores y añadir F15 mejora el arnés. Falta exigir la ejecución de la carga y del lifecycle de la celda, descrita en TA2-02. |

Resultados con entrega puntual, vaciando los callbacks posteriores a la última lectura:

| Fixture | Inversiones | Latch final |
|---|---:|---|
| F1 | 1, en `10483` | ON |
| F2 | 5 | ON |
| F3: Toggle primero / Power primero / simultáneos | 0 / 0 / 0 | OFF |
| F4 | 0 | OFF |
| F5 | 0 | OFF |
| F6 | 0 | OFF |
| F7 | 0 | OFF |
| F8 | 2, en `1900` y `4950` | OFF |
| F9 | 1, en `5500` | ON |
| F10 | 1, enmascarada por Set | ON |
| F11 | 1 | ON |
| F12 | 0 | OFF |
| F13 | 1, en `5400` | ON |
| F14 | 1, en `5400` | ON |
| F14, Toggle 200 ms tarde | 1, consumida por el callback de Power en `5500` | ON |
| F15, subida en `5000`, gracia hasta `30000` | 0 | OFF |
| F15, subida en `31000`, misma gracia | 1, en `31400` | ON |

Para concretar las fixtures abreviadas:

- **F2:** inicio `(0,1,0)`; para `a=10000+3000k`, `k=0..4`: `(a,1,1)`, `(a+10,0,0)`, `(a+83,1,1)`, `(a+2000,1,0)`, `(a+2010,0,0)`, `(a+2083,1,0)`.
- **F10:** Set alto desde el inicio; Toggle sube en `5000` y baja en `7000`; Reset bajo.

Los mutantes pedidos quedan discriminados en el modelo:

| Mutante | Resultado distinto del esperado |
|---|---|
| Debounce a 0 | F8: **1/ON**; F9: **0/OFF** |
| Instante compartido | F8: **1/ON** |
| Temporizador compartido | F13: **0/OFF** |
| Power siempre primero | F14 con retraso: **0/OFF** |
| Toggle primero en empate | F12: **1/ON** |
| Sin duración mínima de R1 | F11: **2/OFF** |
| Sin R2, incluidas sus guardas de Power inválido | F3, sus tres variantes, F4–F7 y F12: **1/ON** |
| Rearme de producción a 0 | F11: **2/OFF** |
| Sin gracia | F15 anterior a `30000`: **1/ON** |

**La invariancia ante retrasos arbitrarios es falsa.** Además del retraso de Toggle de TA2-01, retrasar 1000 ms los temporizadores de Power programados desde `5000` convierte **F7 de 0/OFF en 1/ON**. Su corte `5050–5950` dura 900 ms, pero se sobrescribe antes de asentarlo. La subida de Toggle en `6000` usa el antiguo `m_PowerSinceMs`, aunque Power solo lleva 50 ms recuperado.

**2. Secuencias adicionales solicitadas**

Los casos incorrectos siguientes comparten el mecanismo de TA2-01; no los cuento como hallazgos independientes.

| Caso | Ejecución y resultado |
|---|---|
| **Ambos vencidos; Toggle anterior** | F14, Toggle fechado `5000` y Power `5100`; el callback de Power en `5500` procesa Toggle y después Power. **1/ON**, correcto. |
| **Ambos vencidos; Power anterior** | `(0,1,0) → (5000,0,0) → (5100,0,1)`, Power entregado 200 ms tarde. En `5500` procesa Power y después Toggle. **0/OFF**, correcto. |
| **Instantes de ambas entradas empatados** | F12, ambos fechados `5000`, incluso entregados tarde juntos: Power primero, **0/OFF**, correcto. |
| **Relectura cambia Power mientras vence Toggle** | Inicio `(0,1,0)`; en `5000`, `(0,1)`. En `5400` el callback relee `(1,1)`, sin setter intermedio. Sobrescribe el Power bajo que duró exactamente 400 ms y acepta Toggle: **1/ON**, cuando corresponde **0/OFF**. |
| **Relectura cambia Toggle mientras vence Power** | Inicio `(0,1,0)`; Toggle sube en `5000`, Power baja en `5100`; Toggle tiene 200 ms de retraso. En `5500` la relectura encuentra Toggle bajo. Se pierde el alto de 500 ms: **0/OFF**, cuando corresponde **1/ON**. |
| **Cambio nuevo que todavía no vence** | Inicio `(0,1,0)`; Toggle sube en `5000`; en su callback de `5400` se descubre una caída nueva de Power. Toggle se acepta usando su instante `5000`; Power vence después. **1/ON**, correcto. |
| **Reentrada cambia ambos Raw** | Toggle asentado en `5400`, fechado `5000`; antes de la lectura del setter reentrante, Power y Toggle pasan a bajo. Se programan sus timers, se consume `m_FlipPending` una vez y queda `false`. **1/ON**, sin inversión duplicada. |
| **Subida y bajada válidas antes del callback** | Toggle alto `5000–5800`, callback previsto en `5400` entregado después de `5800`: **0 inversiones**, frente a **1** contractual. |
| **Dos subidas elegibles antes de ejecutarse sus callbacks** | `(0,1,0) → (5000,1,1) → (5600,1,0) → (7000,1,1) → (7600,1,0)`, retrasando 3000 ms los timers posteriores de Toggle. **0/OFF**, frente a **2/OFF**. El latch final por sí solo ocultaría la pérdida. |
| **Toggle alto menos de 400 ms y después bajo estable** | Alto `5000–5300`: **0 inversiones**; conserva el bajo asentado anterior. Una subida posterior en `6500` sí produce una inversión. Correcto. |
| **Bajo inicial menor de 400 ms y después alto estable** | `(0,1,0) → (300,1,1)`: **0 inversiones**, `m_ToggleLowSinceMs=-1`. Correcto. |
| **Subida con `m_ToggleLowSinceMs=-1`** | El helper real devuelve **false**, por [LFPG_MemoryCell.c:27](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:27). Con la inicialización propuesta, una subida asentada requiere antes una bajada asentada que fija ese miembro. No encontré un camino válido que eluda esa protección. |

Hay también dependencia del orden de ejecución **exactamente en el límite**:

```text
(0,1,0) → (5000,1,1) → (5400,1,0)
```

- Callback antes de la lectura de `5400`: **1/ON**.
- Lectura antes del callback: **0/OFF**.

El alto duró exactamente 400 ms y el contrato lo incluye. Es otra manifestación de sobrescribir el candidato vencido.

**3. CallLater, Remove, entidad y lado de ejecución**

**Las firmas propuestas son válidas.** La definición local de `ScriptCallQueue.CallLater` admite `func fn`, retraso, repetición y argumentos opcionales; `Remove` recibe `func fn`: `C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts/2_gamelib/tools.c:61` y `:65`. Coincide con la [fuente oficial de Bohemia](https://raw.githubusercontent.com/BohemiaInteractive/DayZ-Script-Diff/main/scripts/2_gamelib/tools.c). Los wrappers sin parámetros son compatibles.

**Remove por entrada:** retirar `LFPG_MCellPowerSettleTimer` y `LFPG_MCellToggleSettleTimer` por separado concuerda con las dos identidades de callback propuestas. El arnés debe ejecutar esas operaciones reales; cancelar un temporizador Python por su cuenta no prueba el cableado del fuente.

**Borrado con ambos pendientes:** el lugar disponible es `LFPG_OnDeleted`. La base marca borrado e invoca ese hook antes de retirar la entidad del lifecycle y llamar al padre: [lfpg_devicebase.c:293](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/lfpg_devicebase.c:293). El hook heredado está vacío en [:580](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/lfpg_devicebase.c:580). La celda debe retirar ambos wrappers de SYSTEM ahí, como prescribe la v2.

**OBSERVACION preexistente:** la celda no retira actualmente `LFPG_DeferredRouting` al borrar. Sus `Remove` están en el setter y la inicialización: [LFPG_MemoryCell.c:292](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:292) y [:378](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:378). El brief contempla expresamente dejar ese comportamiento y retirar solo los nuevos; no lo uso para invalidarlo.

**Callback sobre entidad borrada:** la declaración nativa no demuestra si una llamada ya preparada se descarta automáticamente, ni qué ocurre al invocarla sobre un receptor destruido. No he demostrado una excepción ni muerte del proceso. La limpieza prevista cubre las llamadas pendientes en el modelo; falta acreditación nativa.

**Reentrada:** conservar `m_PoweredNet` no provoca retorno temprano: [LFPG_MemoryCell.c:178](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:178). Puede continuar hasta consumir el pendiente. El enrutado marca trabajo posterior mediante `MarkNodeDirty`, en [LFPG_ElecGraphImpl.c:5098](C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_ElecGraphImpl.c:5098); no encontré una llamada síncrona a otro asentamiento en esa ruta.

**Cliente frente a servidor:**

- El setter actual está protegido por `SERVER`: [LFPG_MemoryCell.c:175](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:175).
- La v2 exige la misma protección para el cuerpo de `LFPG_MCellSettle`; los wrappers pueden existir en ambos lados y llamar a ese cuerpo vacío en cliente.
- `IsPortReceivingPower` está declarado en `4_World`: [LFPG_NetworkManager.c:258](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_NetworkManager.c:258). No introduce una referencia ascendente nueva.
- SYSTEM se procesa por frame en el vanilla local `scripts/3_game/dayzgame.c:3026`; no debe suponerse que espera a terminar la carga.
- El traductor de tests elimina las directivas de preprocesado: [enforce_scalar_slice.py:19](C:/tmp/lfpg-grok/wt-nostra-131/.github/tools/enforce_scalar_slice.py:19). Un verde suyo no acredita la separación cliente/servidor.

**4. ¿Pueden dar verde los tests pedidos con un fuente mal?**

**Sí.**

1. Una implementación fiel al método v2 obtiene todos los resultados F1–F15 y discrimina todos los mutantes indicados, pero conserva TA2-01. Las fixtures no incluyen una entrega suficientemente tardía que atraviese el siguiente cambio de esa entrada.
2. Leer la constante de gracia y fijar F15 a `30000` permite omitir la asignación durante la carga. Esa mutación mantiene hoy **11/11 verdes**. TA2-02 cierra ese hueco.
3. Extraer únicamente el setter y el asentador no obliga a ejecutar los wrappers ni el hook de borrado. Pueden quedar desconectados mientras el arnés llama directamente al asentador.
4. Los tests actuales de pulso dieron **3/3 verdes**, por separado, con estos tres mutantes aplicados en memoria a PushButton y SwitchRemote: guarda de `LFPG_PulseOff` invertida, plazo a cero y retirada de la limpieza al borrar. Sus comprobaciones actuales son textuales: [test_pulse_survives_blackout.py:81](C:/tmp/lfpg-grok/wt-nostra-131/.github/tools/test_pulse_survives_blackout.py:81). **La reescritura ejecutable exigida por la v2 es adecuada para corregirlo**, pero todavía no está implementada.

**LO QUE NO PUDE VERIFICAR**

- Compilación Enforce ni ejecución del candidato: todavía es un plan.
- Semántica nativa de cancelación, rearme durante un callback y despacho sobre receptores borrados.
- La nueva suite extraída del fuente, sus pruebas de lifecycle y su selección cliente/servidor.
- Frecuencia de estas secuencias en partida, C1–C7, PBO desplegado, LEDs o puertas.
- Linter del candidato: no existe un diff de implementación que validar.

No modifiqué ficheros, no creé subagentes y no arranqué DayZ. El árbol terminó limpio.