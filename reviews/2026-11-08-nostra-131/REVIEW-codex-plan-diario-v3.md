<!-- Salida literal de Codex (codex exec, solo lectura), revision de plan de BRIEF-DIARIO.md v3. Transcrita por el receptor sin editar. -->

BASE: claude/project-thread-0iolln @ 617491ebdf808dab7968617f04b36a8bb9251950

**Veredicto: NO-APTO.** Encontré dos historiales que incumplen las garantías declaradas. Los contramodelos originales de D2-01 a D2-05 están atendidos, pero `savedEmpty` conserva una premisa falsa y la igualdad de JSON permite un ABA ordinario que G3 no declara aceptado.

Revisión exclusivamente de lectura, sin subagentes, modificaciones ni arranques de DayZ. HEAD avanzó durante la revisión; comprobé que los documentos y fuentes contrastados seguían iguales a esta base.

## Hallazgos

Represento una entrada como **`(W; saved; prev; loaded)`**. A es el JSON válido de un dueño sin cables; B contiene un cable.

| ID | Clase | Path:line | Secuencia exacta | Salida mala | Cambio mínimo |
|---|---|---|---|---|---|
| **D3-01** | **INVALIDA-DISENO** | [BRIEF-DIARIO.md:42](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:42), [G3:50](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:50), [IGNORE:177](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:177), [captura aplazada:183](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:183) | Carga A; añade B; Tick escribe `(B;A;"";A)`; corta y vuelve a A; marca dirty; una validación hace saltarse el siguiente Tick de captura; termina la validación; CE emite A; muerte antes del siguiente Tick, **más de cinco segundos después del corte**. | CE carga A, pero replay reconoce el A antiguo de la entrada y restaura B: **reaparece el cable que la CE había guardado cortado**. Incumple G1. | Manteniendo G1, hace falta un testigo durable que distinga ambos historiales. Un cambio exclusivamente en las comparaciones del replay no basta. Ampliar G3 a estos retornos ordinarios sería una modificación explícita del contrato, pendiente de decisión. |
| **D3-02** | **INVALIDA-DISENO** | [BRIEF-DIARIO.md:125](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:125), [Generator:132](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:132), [CaptureKind:155](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:155); [LFPG_TestDevices.c:898](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_TestDevices.c:898) | Entrada B escrita correctamente; arranque con CE=A; RESTORE B; una emisión falla al serializar y produce `""`: `ultimo=""`, `anterior=A`; termina validación; Tick serializa B correctamente, pero `savedEmpty` devuelve REMOVE; persiste diario vacío; otra muerte y CE recupera A, la emisión anterior permitida por C3. | **Pierde B**, previamente capturado y escrito con serialización buena. La CE carga A, **no** la cadena vacía excluida por los tests. Incumple G2. | **[DESIGN]** Sustituir «REMOVE si `savedEmpty`» por «REMOVE si `savedEmpty && prevEmpty`». Con `prev=A`, captura `(B;"";A;A)` y recupera B. |
| **M3-01** | **CORRECCION-MENOR-DEL-BRIEF** | [BRIEF-DIARIO.md:233](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:233); [LFPG_NetworkManagerImpl.c:1518](C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_NetworkManagerImpl.c:1518) | Se busca la guarda de `owner` nulo para insertar el enganche de `QueueBroadcastOwnerSnapshotFromWires`. | Esa guarda **no existe en ese método**; empieza capturando el snapshot. | Texto exacto: **«En `QueueBroadcastOwnerSnapshotFromWires`, añadir `if (!owner) return;` y, a continuación, `MarkOwnerDirty(owner)`, antes de `CaptureOwnerBroadcastSnapshot`.»** |
| **O3-01** | **OBSERVACION** | [BRIEF-DIARIO.md:277](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:277), [estructura:317](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:317) | **[DESIGN]** Funciones puras correctas; el gestor real elimina la entrada al recibir FIRST_EMIT. El modelo Python conserva su transición correcta. | Los escenarios del modelo siguen verdes mientras el gestor puede perder B tras una sola emisión y CE=A. No existe todavía la suite para medir este falso verde contra el fuente. | Ejecutar también las transiciones reales del gestor y exigir que falle un mutante de su despacho FIRST_EMIT/DURABLE. |

### D3-01: reproducción y frontera de G3

La traducción en memoria produjo:

```text
t=0   carga y emisión A
t=10  añade cable: B
t=15  Tick escribe (B; A; ""; A)
t=48  corta cable: A; dirty=true
t=49  comienza validación
t=50  Tick aplaza captura
t=51  termina validación
t=53  emisión A: IGNORE porque dirty=true
t=54  muerte; CE carga A
       replay → RESTORE B
```

No se restaura un backup, no se incumplen C1/C2 y el corte ocurrió seis segundos antes de la muerte. G3 describe **restauración manual de backups**, no este corte ordinario.

La repetición exacta es realizable: el almacén vacío usa un JSON canónico en [LFPG_WireHelper.c:277](C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_WireHelper.c:277). El corte real limpia el almacén y publica mediante `BroadcastOwnerWireDelta` en [LFPG_RPCServerHandlerImpl.c:1031](C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1031).

Hay una indistinguibilidad fundamental:

- Añadir B y morir con CE=A requiere RESTORE B.
- Añadir B, cortarlo posteriormente y morir con CE=A requiere conservar A.

Ambos arranques reciben exactamente **la misma entrada y el mismo L**. Reconocer una cadena histórica no demuestra que su ocurrencia cargada sea anterior a W.

### D3-02: alcance del fallo de serialización

`SerializeJSON` deja expresamente `jsonOut=""` cuando falla, [LFPG_WireHelper.c:312](C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_WireHelper.c:312). El desplazamiento propuesto por D3 también registra esa emisión vacía.

No estoy convirtiendo el bug preexistente del serializador en un encargo de reparación: el fallo nuevo es **borrar una entrada buena cuando la serialización ya vuelve a funcionar**.

Probé el cambio mínimo de la tabla en memoria:

```text
v3 literal:       live=B, saved="", prev=A → REMOVE → CE=A queda A
predicado corregido:                    → STORE  → CE=A restaura B
```

S1–S13 siguieron dando sus resultados esperados.

## Cierre de D2-01 a D2-05 y M2-01/M2-02

| ID | Dictamen |
|---|---|
| **D2-01** | **Cerrado el contramodelo original dentro de C3.** S9 reconoce A mediante `m_LoadedJSON` y B mediante `m_SavedPrevJSON`. Las generaciones expresamente excluidas no son un hallazgo. |
| **D2-02** | **Cerrado bajo C1/C2.** La segunda emisión de W identifica otra pasada; desaparece la dependencia de PASS_GAP. La retirada anticipada aceptada por G4 no bloquea esta revisión. |
| **D2-03** | **Cerrado en el plan.** Lee los cuatro candidatos necesarios, sin promoción durante la carga. |
| **D2-04** | **Cerrado en el plan.** Sondea los cuatro antes de cargar o borrar y bloquea también con el ajuste desactivado. |
| **D2-05** | **Cerrado el mecanismo original:** KEEP conserva dirty e invalida la caché de WireOwner. La pérdida posterior por `savedEmpty` es D3-02. |
| **M2-01** | **No completamente cerrado.** S7/S11 corrigen el caso de aparato nuevo ya emitido, pero `savedEmpty` todavía puede aparecer **después de haber leído A**, por una emisión vacía posterior. Su explicación y decisión de REMOVE siguen siendo falsas en ese caso. |
| **M2-02** | **Cerrado.** D4.2 exige explícitamente `Debug [WireJournal] first-emit <id>`. |

## Comprobación del protocolo y de los casos pedidos

Traduje D3/D4 a Python y lo ejecuté exclusivamente en memoria:

- **S1–S13:** todos produjeron lo pedido.
- **Seis mutantes de escenarios:** todos rompieron al menos los casos anunciados.
- **27 000 historiales propios de una sesión:** 63 644 comprobaciones de replay, sin regresiones ni recuperaciones omitidas en ese subdominio.
- **27 000 historiales propios de tres sesiones:** 196 585 comprobaciones, mismo resultado.

Semilla propia: `13120261109`. **No reproduje la muestra original exacta:** su programa y semilla no están en la rama. Los dos contramodelos anteriores muestran por qué esos resultados no prueban las garantías completas.

Los puntos particulares quedan así:

| Punto | Resultado |
|---|---|
| **L frente a `m_LoadedJSON` antiguo** | L debe proceder del dueño actual. Por ejemplo, entrada `(D;C;B;A)` y dueño cargado con B: se compara **B**, no A. La v3 lo especifica correctamente. `m_LoadedJSON` es otro candidato histórico, no autoridad sobre lo recién cargado. |
| **Dos muertes, J3/J12** | Funcionan con recaptura ordinaria. RESTORE B y recaptura producen `(B;A;"";A)`; si antes se emite B′, producen `(B′;B′;A;A)`. Reconocen CE=A. D3-02 añade la variante fallida de emisión vacía. |
| **DURABLE y captura intermedia** | Primera emisión B → `seen=true`; recaptura con W=B conserva `seen`; segunda emisión B → DURABLE. Es seguro bajo C1/C2: la primera pasada ya terminó. La cuenta sigue al **valor de W**, no se reinicia con cada recaptura idéntica. |
| **STORE con `live==saved`, prev no vacío** | Correcto. `(B;B;A;A)` protege la ventana en que la CE todavía puede recuperar A. |
| **REMOVE con `live==saved`, prev vacío** | Correcto para el estado cargado conocido o el aparato nuevo excluido. Tras una primera emisión distinta de un dueño cargado, prev conserva aquel estado y esta condición no se cumple. |
| **KEEP** | El reintento de WireOwner es efectivo gracias a la invalidación existente. Generator vuelve a intentar serializar un fallo sin necesitar ese método. |
| **RESTORE y validación sin enganche** | `m_Restored` y `OnStartupValidationDone` fuerzan la recaptura. S3 funciona aunque B′ se emita antes. |
| **TTL** | Elimina por antigüedad, no confirma durabilidad. Bajo C1–C3 y JSON estable, las emisiones normales hacen recuperable W mucho antes. No considero hallazgo una ausencia de emisiones durante una hora que contradiga ese contrato. El TTL se reinicia al restaurar y no cuenta el tiempo apagado. |

## Compatibilidad real de ficheros y JSON

**La firma existente es:**

```c
static bool TryReadRawJsonVersion(string targetPath, out int version)
```

Está en [LFPG_FileUtil.c:865](C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_FileUtil.c:865). Abre el fichero, escanea texto y cierra el handle; **no deserializa JSON**.

- Un documento corrupto puede devolver `true` si ya encontró un `ver` entero.
- Incluso `{"ver":2` truncado puede devolver `true, 2` al llegar a EOF, [LFPG_FileUtil.c:1083](C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_FileUtil.c:1083).
- Un prefijo insuficiente o inválido devuelve `false`.
- No hay una llamada explícita a `ErrorEx` ni al deserializador que convierta el truncado en `SCRIPT (E)`.

**`JsonFileLoader.LoadFile`**, en el extracto local 1.30.164200, tiene firma:

```c
static bool LoadFile(string filename, out T data, out string errorMessage)
```

Ante `ReadFromString == false`, devuelve `false` con el mensaje, sin `ErrorEx`: [jsonfileloader.c:27](E:/DayZ-Stable-Extract/1.30.164200/P-scripts/3_game/tools/jsonfileloader.c:27). El obsoleto `JsonLoadFile` sí llama a `ErrorEx`, [jsonfileloader.c:128](E:/DayZ-Stable-Extract/1.30.164200/P-scripts/3_game/tools/jsonfileloader.c:128).

Por tanto, **el código leído no sustenta afirmar que LoadFile sobre un truncado genere necesariamente `SCRIPT (E)`**. El silencio interno de la primitiva nativa queda sin comprobar en ejecución.

**El orden de candidatos del brief es compatible con el escritor:**

```text
.tmp → principal → .bak.new → .bak
```

El helper escribe `.tmp` antes de sustituir el principal, conserva respaldos durante la promoción y elimina el temporal al final: [LFPG_FileUtil.c:80](C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_FileUtil.c:80), [promoción:120](C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_FileUtil.c:120), [limpieza:140](C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_FileUtil.c:140).

**Un huérfano anterior, por sí solo, no hace que `.tmp` sea más viejo que un principal escrito después por este mismo protocolo:** toda actualización posterior del principal pasa antes por ese mismo `.tmp`; si falla antes de tocarlo, tampoco actualiza el principal. No encontré el contramodelo solicitado con un único escritor y las operaciones previstas. Intervenciones externas o varios escritores necesitan otras premisas.

## APIs y enganches D5

En [LFPG_NetworkManagerImpl.c](C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_NetworkManagerImpl.c) existen:

| Firma | Línea / posición |
|---|---|
| `override void QueueBroadcastOwnerSnapshot(EntityAI owner, array<vector> targetPositions, bool broadcastAll)` | **1497**; guarda nula en 1499. |
| `protected void QueueBroadcastOwnerSnapshotFromWires(EntityAI owner, array<ref LFPG_WireData> interestWires)` | **1518**; falta la guarda descrita: M3-01. |
| `override void BroadcastOwnerWires(EntityAI owner)` | **1625**; guarda nula en 1627, antes de coalescer. |
| `override void BroadcastOwnerWireDelta(EntityAI owner, array<int> operations, array<ref LFPG_WireData> deltaWires)` | **1832**; guarda combinada en 1834. |
| `override void ValidateAllWiresAndPropagate()` | **2873**; insertar replay después del bloque 2876–2880. |
| `protected void LFPG_ValidationFinalize()` | **3166**; `wasStartupPending` en 3178 y su `if` en 3180. |
| `protected void LFPG_ServerSchedulerTick()` | **438**; bloque de flush en 451, llamada en 454. |

El full sync envía directamente mediante `rpc.Send` en **2185**; ese camino no incorpora los cuatro enganches.

**Cachés y Generator:**

- Existe **`void LFPG_InvalidateWireJSONCache()`**, sin parámetros ni retorno, en [LFPG_WireOwnerBase.c:176](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_WireOwnerBase.c:176). Pone la generación de caché a `-1`.
- **Generator sí tiene caché en esta rama:** miembros en [LFPG_TestDevices.c:63](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_TestDevices.c:63), getter en **711**, comparación profunda desde **728**. Un resultado vacío deja `m_WireJSONCacheValid=false`.
- **Generator no tiene `m_WireGeneration`**, ni ese método de invalidación. Su `OnStoreSave` continúa serializando directamente en **899**.
- `LFPG_DeviceAPI.GetWiresJSON(Object obj)`, [LFPG_IDevice.c:614](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_IDevice.c:614), usa cast para WireOwner y reflexión para Generator. **El wrapper no invalida caché**; llama al getter correspondiente.
- El refresco de net IDs invalida WireOwner en **NetworkManagerImpl:3031**. Generator detecta las modificaciones mediante su snapshot.

`worldScriptModule` incluye el directorio `LFPowerGrid/scripts/4_World`, [config.cpp:220](C:/tmp/lfpg-grok/wt-nostra-131/config.cpp:220). El fichero nuevo ubicado allí queda comprendido por esa declaración. WireOwner, Generator y WireJournal estarán en **4_World**; las llamadas propuestas no crean una dependencia ascendente a 5_Mission.

## Fuerza de los tests y premisa

**Sí, los tests pedidos pueden dar verde con un gestor incorrecto.** La estructura nueva cubre mejor las asignaciones, pero no ejecuta todo el despacho y los efectos de D4.2–D4.5. El ejecutor disponible declara sus límites en [enforce_scalar_slice.py:1](C:/tmp/lfpg-grok/wt-nostra-131/.github/tools/enforce_scalar_slice.py:1).

Además, **el propio protocolo literal pasa S1–S13 y las muestras sin repeticiones mientras falla D3-01**. Faltan, como mínimo:

- Retorno ordinario A→B→A con una captura aplazada.
- Emisión CE vacía seguida de recaptura buena y recuperación CE no vacía.
- Mutación del despacho real FIRST_EMIT/DURABLE, conservando correctas las funciones puras.

La premisa central que falla es **confundir identidad del JSON con antigüedad de su ocurrencia**. La sonda caracteriza emisiones y recuperación; no convierte la igualdad de contenido en un reloj. G3 reconoce esa ambigüedad para backups manuales, pero el código permite producirla con un corte normal.

## LO QUE NO PUDE VERIFICAR

- Compilación y ejecución del gestor y tests futuros: todavía no existen en esta base.
- Los 27 000 historiales originales exactos, sin su programa y semilla.
- Comportamiento interno y logging nativo de `JsonSerializer.ReadFromString` ante truncados.
- Repetición independiente de la sonda, sus artefactos originales y las pruebas J1–J12.
- Rendimiento, saturación, retail y garantías de almacenamiento físico.

No ejecuté el linter: no hubo cambios ni implementación nueva que validar. No escribí informes ni otros archivos; este mensaje contiene la entrega completa.