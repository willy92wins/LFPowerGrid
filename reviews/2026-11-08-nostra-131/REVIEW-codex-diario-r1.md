<!-- Salida literal de Codex (codex exec, solo lectura), revision del commit 0d96577. Transcrita por el receptor sin editar. -->

BASE: claude/project-thread-0iolln @ 0d96577ffd95c204eb525246c3a25c107ecb4164

**Veredicto: SOUND-with-fixes.**

No he demostrado un fallo funcional del diario dentro del contrato declarado. Sí he reproducido falsos verdes y una cobertura aleatoria inferior a la exigida por el brief v3. Los arreglos encontrados afectan a los tests.

Revisión exclusivamente de lectura: sin modificar ficheros, sin subagentes y sin arrancar DayZ. G3, G4, G5 y las exclusiones expresas del brief no se consideran hallazgos.

## Hallazgos

| ID | Prioridad | path:line | Secuencia exacta | Salida mala | Arreglo mínimo |
|---|---|---|---|---|---|
| D-R1-01 | P2 | [test_wire_journal_scenarios.py:650](/C:/tmp/lfpg-grok/wt-nostra-131/.github/tools/test_wire_journal_scenarios.py:650) | Mutar en memoria la comparación de `OnOwnerSaved`, [LFPG_WireJournal.c:239](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_WireJournal.c:239), sustituyendo `if (savedJSON == entry.m_WiresJSON)` por `if (true)`. Crear una entrada con W=`B`, sin dirty ni emisión vista; ejecutar el cuerpo real con `A` dos veces. Ejecutar después los tres suites completos contra ese fuente mutado. | El mutante marca FIRST_EMIT con el primer `A` y elimina la entrada con el segundo: `entries=[]`, `m_FileDirty=true`. **Los 29 tests permanecen verdes.** El slice real solo prueba dos emisiones de W; el modelo Python calcula correctamente la igualdad y oculta el error del gestor. | Añadir al slice real emisiones distintas de W antes y después de FIRST_EMIT; exigir que no cambien entrada, contador ni suciedad del fichero. Ejecutar esas mismas expectativas contra el mutante y exigir rojo. |
| D-R1-02 | P2 | [test_wire_journal_structure.py:117](/C:/tmp/lfpg-grok/wt-nostra-131/.github/tools/test_wire_journal_structure.py:117) | Cambiar únicamente el primer bucle de candidatos de `ReplayAtBoot` por `for (ci = 1; ci < 2; ci = ci + 1)`: solo sondea el principal, conservando los cuatro candidatos y su orden de carga. Ejecutar los tres suites completos. Caso que queda sin proteger: principal v1, `.bak` v2 y `WireJournal=false`. | **29/29 verdes.** Los asserts comprueban presencia y posición textual de la llamada, pero no que alcance los cuatro candidatos. En ese caso, el mutante omite la versión futura del respaldo y llega al borrado de candidatos. Contradice el mutante obligatorio de [BRIEF-DIARIO.md:364](/C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:364). | Comprobar el recorrido efectivo: colocar v2 sucesivamente en cada candidato, con el ajuste activado y desactivado; exigir retorno sin cargas, borrados ni escrituras. |
| D-R1-03 | P2 | [test_wire_journal_structure.py:108](/C:/tmp/lfpg-grok/wt-nostra-131/.github/tools/test_wire_journal_structure.py:108) | Mantener intacta la rama KEEP y su invalidación de caché, pero sustituir en memoria [LFPG_WireJournal.c:400](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_WireJournal.c:400) `m_Dirty.Set(keepIds[di], true);` por `m_Dirty.Remove(keepIds[di]);`. Ejecutar los tres suites completos. Secuencia afectada: entrada B; cambio C; serialización fallida; KEEP; serialización recuperada sin otro cambio; siguiente Tick. | **29/29 verdes.** Tras `m_Dirty.Clear()`, el mutante pierde el id y el siguiente Tick no reintenta C. El test fija `keepIds.Insert(id)` y la invalidación, pero no la conservación efectiva del dirty. | Ejecutar la transición real de KEEP hasta el Tick siguiente, o fijar también la reconstrucción de `m_Dirty`. Exigir rojo para este mutante, además del que elimina `keepIds.Insert`. |
| D-R1-04 | P2 | [test_wire_journal_scenarios.py:621](/C:/tmp/lfpg-grok/wt-nostra-131/.github/tools/test_wire_journal_scenarios.py:621) | Ejecutar `test_seeded_histories` contando las llamadas a `die`. El bucle intenta 10 000 historias, descarta las que no emitieron y termina cada historia después de su único replay. La segunda llamada a `die`, en la línea 645, pertenece al fixture fijo `Z` del mutante antiguo. | Test verde con **6 824 historias que alcanzan las propiedades**, más 6 824 fixtures negativos fijos. No continúa jugando después del arranque ni cubre varias sesiones aleatorias. Tampoco selecciona el estado cargado como emisión anterior a la primera. No cumple [BRIEF-DIARIO.md:322](/C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:322). | Contar al menos 10 000 historias realmente comprobadas; continuar tras cada muerte con nuevas instancias de sesión y solo el fichero persistido; incluir el estado cargado entre las emisiones elegibles de C3. |

Estos hallazgos describen defectos de verificación. **Las cuatro protecciones correspondientes están presentes en el código congelado.**

## Contrato y código comprobados

| Superficie | Resultado |
|---|---|
| **L frente a los cuatro campos** | El replay obtiene L mediante `OwnerLoadedJSON`, exige L y W no vacíos y solo restaura cuando coincide con saved, prev o loaded. STALE no alcanza `ClearDeviceWires`. La deserialización se hace sobre un array nuevo antes de tocar el aparato. [LFPG_WireJournal.c:583](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_WireJournal.c:583), [LFPG_WireJournal.c:623](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_WireJournal.c:623). |
| **Emisiones y DURABLE** | El predicado ignora ausencia de entrada, dirty y emisiones distintas. FIRST_EMIT conserva la entrada; DURABLE la retira. El slice real ejecutado conserva correctamente la entrada tras dos emisiones distintas de W. [LFPG_WireJournal.c:27](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_WireJournal.c:27), [LFPG_WireJournal.c:224](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_WireJournal.c:224). |
| **Cadenas vacías y KEEP** | Un fallo de serialización produce KEEP; el código conserva el id e invalida la caché de WireOwnerBase. Las reglas conservan también los estados anterior/cargado cuando la última emisión fue vacía. [LFPG_WireJournal.c:14](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_WireJournal.c:14), [LFPG_WireJournal.c:337](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_WireJournal.c:337). |
| **Validaciones** | Los cuatro enganches marcan antes de publicar/coalescer; `MarkOwnerDirty` no rechaza validación activa. Tick pospone la captura durante la validación y el cierre del arranque vuelve a marcar los restaurados. [LFPG_NetworkManagerImpl.c:1502](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_NetworkManagerImpl.c:1502), [LFPG_WireJournal.c:280](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_WireJournal.c:280), [LFPG_WireJournal.c:704](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_WireJournal.c:704). |
| **Almacenes congelados** | `OwnerUsable` los rechaza; su guardado reemite el JSON original y no avisa al diario. [LFPG_WireJournal.c:125](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_WireJournal.c:125), [LFPG_WireOwnerBase.c:114](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_WireOwnerBase.c:114). |
| **Generator** | Tiene los tres estados, getters y desplazamiento en emisión. Su caché permanece inválida después de serializar `""`, permitiendo reintentar. El `false` ignorado de `DeserializeJSON` es el problema previo declarado fuera de alcance. [LFPG_TestDevices.c:717](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_TestDevices.c:717), [LFPG_TestDevices.c:911](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_TestDevices.c:911). |
| **Cuatro candidatos** | Orden real: `.tmp`, principal, `.bak.new`, `.bak`. El sondeo de versiones precede a cargas y borrados; FUTURE deja el diario inactivo incluso con el ajuste desactivado. El replay no promueve ficheros. [LFPG_WireJournal.c:469](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_WireJournal.c:469). |
| **AtomicSaveWireJournal** | Comparación mecánica de ambos cuerpos, sustituyendo únicamente el tipo y eliminando comentarios/espacios: **idénticos**. Conserva read-back, staging, restauración tras fallo de promoción, rotación y limpieza. [LFPG_FileUtil.c:146](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_FileUtil.c:146). |

### Bytes de la CE

La lista exacta de `ctx.Write` coincide entre el padre y el commit:

```text
LFPG_WireOwnerBase.LFPG_OnStoreSaveExtra:
    json

LFPG_Generator.OnStoreSave:
    m_DeviceIdLow
    m_DeviceIdHigh
    m_SourceOn
    json
```

En WireOwnerBase se mantienen además el prefijo de `DeviceBase` —`m_DeviceIdLow`, `m_DeviceIdHigh`, `ver`— y el hook posterior del dispositivo. Las nuevas asignaciones y el aviso al diario no sustituyen el `json` escrito. [LFPG_DeviceBase.c:367](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_DeviceBase.c:367), [LFPG_WireOwnerBase.c:139](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_WireOwnerBase.c:139).

### `TryReadRawJsonVersion` real y corrupción

Se revisó el helper actual de [LFPG_FileUtil.c:926](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_FileUtil.c:926). Ejecuté en memoria una traducción de su scanner, conservando su control de flujo y simulando `ReadFile` por bloques:

| Entrada | Resultado del sondeo |
|---|---|
| `{"ver":2,"entries":[` | `true, 2`: barrera FUTURE |
| `{"ver":2` | `true, 2`: barrera FUTURE al EOF |
| `{"ver":2,corrupt` | `true, 2`: barrera FUTURE |
| `{"ver":1,"entries":[` | `true, 1`: requiere validación tipada posterior |
| `{"x":{"ver":2},"ver":1}` | `true, 1`: ignora la versión anidada |
| Versión positiva desbordada | Satura en `2147483647`: FUTURE |

El sondeo **no certifica JSON completo**, como declara su implementación. La aceptación del candidato sigue dependiendo de `JsonFileLoader.LoadFile`.

### Compilación, sintaxis y coste

- Separación correcta entre `3_Game`, `4_World` y `5_Mission`; `config.cpp` carga `4_World`. No aparecen dependencias nuevas de un módulo inferior hacia Mission.
- Sin redeclaraciones locales detectadas en los métodos nuevos. Campos y getters nuevos resueltos contra sus clases; sin infracciones detectadas de los operadores prohibidos.
- `map.GetKey(int)` existe. Su coste documentado es O(n), por lo que esos recorridos indexados pueden ser cuadráticos; no he medido su tiempo. [enscript.c:897](</C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts/1_core/proto/enscript.c:897>).
- Los enganches no serializan ni escriben ficheros. El trabajo de captura queda en Tick; `OnOwnerSaved` no hace E/S.
- El patrón de clases JSON con miembros `array<ref …>` tiene equivalente vanilla. No lo considero un error de tipos por inspección.
- EOL conforme al brief específico: NetworkManagerImpl CRLF, restantes fuentes del cambio LF y sin BOM.

## Pruebas ejecutadas

- **29/29 tests del diario verdes**: 6 reglas, 18 escenarios y 5 estructurales.
- Mutantes en memoria: detectados los cambios de predicados, captura de prev incorrecta, replay desde último, intercambio FIRST_EMIT/DURABLE, orden principal/`.tmp`, deserialización sobre el aparato y bloqueo del marcado durante validación.
- Los mutantes de D-R1-01/02/03 dieron **29/29 verdes**. Permitir almacenes congelados también sobrevivió al suite completo.
- Linter: padre `8c5afde` **267 ficheros, 0 errores, 48 warnings**; commit **268 ficheros, 0 errores, 48 warnings**. **Delta de errores: 0.** La base se suministró al validador desde Git mediante sustitución de lecturas en memoria.
- `ui_reconcile`: **0 FAIL / 0 WARN**.
- Árbol de trabajo sin cambios al finalizar.

## LO QUE NO PUDE VERIFICAR

- Compilación y ejecución reales de Enforce, recuperación tras muerte, Generator en partida y cumplimiento de C1–C4 fuera de las mediciones declaradas. Corresponden a `PRUEBA-DIARIO.md`.
- Rechazo efectivo de cada fichero truncado por el serializador nativo. El wrapper propaga su resultado, pero `ReadFromString` es `proto`; la prueba Python del scanner no ejecuta ese parser. [jsonfileloader.c:27](</C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts/3_game/tools/jsonfileloader.c:27>), [gameplay.c:100](</C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts/3_game/gameplay.c:100>).
- Latencia real de los enganches/Tick, fallos físicos de E/S y confirmación a disco.
- Ejecución histórica de todos los gates antes y después del commit. Esta revisión ejecutó los tres suites del diario y los validadores indicados.

