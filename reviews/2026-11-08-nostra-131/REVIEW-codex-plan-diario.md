<!-- Salida literal de Codex (codex exec, solo lectura), revision de plan de BRIEF-DIARIO.md. Transcrita por el receptor sin editar. -->

BASE: claude/project-thread-0iolln @ b5834ff7bf5ebdd1969c4f804a63ba8549b0125b

**Veredicto: NO-APTO.**

El diseño confunde el JSON entregado a `OnStoreSave` con un estado recuperable de la CE. Eso permite adelantar la base del diario, obtener `STALE` y perder cambios que el propio diario ya había escrito. También hay un borrado prematuro por `DROP` y cambios de lifecycle descartados durante la validación.

Revisión exclusivamente de lectura. Leídos `AGENTS.md`, `CLAUDE.md`, el brief, las pruebas propuestas y la medición de origen. Ejecutados contramodelos del protocolo en memoria; no son ejecución de Enforce.

HEAD avanzó durante la revisión a `24c4c48`. Recontrasté los 19 archivos citados contra `b5834ff`: coinciden en contenido, descontando EOL. El brief revisado siguió sin cambios, SHA256 `a4f04fdc00bda87ab8e483d1cdfe39610a7b477d3a40a683b59b2501fab4a480`.

## Hallazgos

En las secuencias siguientes, **CE=A** significa que A es el estado que recuperará la CE después de la muerte; una llamada a `OnStoreSave(B)` todavía no demuestra que recuperará B.

| ID | Clase | Path:line | Secuencia exacta | Salida mala | Cambio mínimo |
|---|---|---|---|---|---|
| **WJ-01** | **INVALIDA-DISENO** | [BRIEF-DIARIO.md:75](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:75), [BRIEF-DIARIO.md:131](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:131) | CE=A; cambio a B; `OnStoreSave(B)` adelanta `m_LastStoredWireJSON`; antes de consolidarse la vuelta, Tick escribe `(base=B, wires=B)`; muerte; la CE recupera A. | A difiere de ambas cadenas: `STALE`, borrado del diario y pérdida de B. | Separar emisión y durabilidad. D3/D4 necesitan conservar los estados anteriores que la CE todavía pueda recuperar; dos cadenas sustituidas de esta manera no bastan. |
| **WJ-02** | **INVALIDA-DISENO** | [BRIEF-DIARIO.md:105](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:105), [BRIEF-DIARIO.md:123](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:123), [LFPG_NetworkManagerImpl.c:3000](C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_NetworkManagerImpl.c:3000) | CE=A; replay restaura B; el ID está en `m_Restored`, todavía fuera de `m_Dirty`; la validación refresca net ids y produce B′; `OnStoreSave(B′)` ocurre antes de la recaptura final; B′ ≠ B y el veredicto es `DROP`; un Tick persiste la eliminación; muerte antes de consolidar B′. | El segundo arranque carga A y ya no tiene entrada para restaurar B. El plazo de cinco minutos no interviene. | Mantener protegida la entrada restaurada hasta terminar validación y recaptura. Un JSON emitido distinto no puede justificar un borrado inmediato. |
| **WJ-03** | **INVALIDA-DISENO** | [BRIEF-DIARIO.md:118](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:118), [LFPG_DeviceLifecycle.c:35](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_DeviceLifecycle.c:35), [LFPG_DeviceLifecycle.c:108](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_DeviceLifecycle.c:108) | Partida ya arrancada; comienza una validación global; un callback de ruina o movimiento corta cables mediante `CutAll`; la publicación llega al enganche, pero `validationActive=true` descarta la marca; termina la validación; pasan varios Tick; muerte antes del guardado CE. | El corte nunca entra en el diario. La recaptura de `m_Restored` no cubre ese ID ni las validaciones posteriores al arranque. | Registrar los IDs modificados durante validación y diferir su captura. No descartar la notificación; cerrar esos pendientes después de cualquier validación. |
| **WJ-04** | **INVALIDA-DISENO** | [LFPG_WireHelper.c:312](C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_WireHelper.c:312), [LFPG_WireHelper.c:333](C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_WireHelper.c:333), [BRIEF-DIARIO.md:149](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:149) | Falla `MakeData` al capturar: `SerializeJSON` devuelve `""`; Tick guarda base=A y wires=`""`; muerte; la CE carga A; replay elige `RESTORE`; `DeserializeJSON("")` devuelve **true** con array vacío; se ejecuta `ClearDeviceWires`. | Un error de serialización se convierte en una restauración válida de cero cables. | Rechazar `m_WiresJSON == ""` en captura y replay. Conservar entrada anterior y pendiente de reintento. El vacío válido generado por el helper es un blob JSON, no `""`. |
| **WJ-05** | **INVALIDA-DISENO** | [BRIEF-DIARIO.md:61](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:61), [BRIEF-DIARIO.md:144](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:144), [LFPG_FileUtil.c:815](C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_FileUtil.c:815) | Fixture: target `{"ver":2,"entries":[]}` y `.tmp` `{"ver":1,"entries":[]}`. La copia tipada del helper promueve el `.tmp` antes de comprobar la versión del target. Después, D4 carga v1. | La barrera de versión futura nunca ve v2; el target futuro ya ha sido sustituido, contradiciendo «left untouched». | Comprobar la versión antes de las operaciones de recuperación que sustituyen archivos. Existe `TryReadRawJsonVersion`, [LFPG_FileUtil.c:865](C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_FileUtil.c:865). |

**WJ-01 también ocurre actualizando una entrada existente:**

```text
CE recuperable=A
cambio B → Tick guarda (A,B)
OnStoreSave(B), todavía sin consolidar
cambio C → Tick sustituye la entrada por (B,C)
muerte → CE carga A → STALE → se pierde C
```

La guarda `isDirty → IGNORE` evita un ACK/DROP concreto, pero **D3 adelanta igualmente `m_LastStoredWireJSON`**. No protege la base.

Congelar siempre la primera base tampoco sería una solución completa: si la CE consolida B y el diario conserva `(A,C)`, B vuelve a resultar `STALE`. Hace falta revisar el contrato de estados recuperables, no aumentar un temporizador.

### Correcciones menores del brief

| ID | Clase | Path:line | Secuencia exacta | Salida mala | Texto exacto que cambiaría |
|---|---|---|---|---|---|
| **M-01** | **CORRECCION-MENOR-DEL-BRIEF** | [PRUEBA-DIARIO.md:42](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/PRUEBA-DIARIO.md:42), [LFPG_FileUtil.c:456](C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_FileUtil.c:456) | Se trunca el target, pero queda un `.bak` válido con cambios pendientes. La recuperación carga ese respaldo y el replay restaura cables. | J8 exige «los cables de la CE» y rechaza una recuperación correcta. | **«Con un respaldo válido, debe recuperarlo y aplicar el compare-and-swap. Repetir sin ningún respaldo válido: entonces deben quedar los cables de la CE y un cambio posterior debe producir un diario válido.»** |
| **M-02** | **CORRECCION-MENOR-DEL-BRIEF** | [PRUEBA-DIARIO.md:32](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/PRUEBA-DIARIO.md:32), [BRIEF-DIARIO.md:125](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:125) | J5 admite una línea `drop` como alternativa a comprobar el fichero vacío. D4 distingue `ack` de `drop ... superseded`. | Un mensaje de otra transición puede sustituir la comprobación de limpieza. | Sustituir **«0 entradas (o el Debug muestra `drop ... ` por ack)»** por **«0 entradas, comprobadas cargando el fichero; una línea de log no sustituye esta comprobación»**. |

### Observaciones y límites del contrato

| ID | Clase | Path:line | Secuencia exacta | Salida mala o incertidumbre | Cambio mínimo |
|---|---|---|---|---|---|
| **O-01** | **OBSERVACION** | [BRIEF-DIARIO.md:134](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:134), `C:/tmp/lfpg-cables-reinicio/INFORME-2.md:14` | ACK; pasan 300 s sin un guardado nuevo recuperable; Tick elimina la entrada. | El plazo no demuestra durabilidad. No se ha medido que este supuesto ocurra en producción ni existe una cota demostrada que lo excluya. | Vincular la retirada a evidencia de durabilidad o declarar expresamente una garantía de mejor esfuerzo. |
| **O-02** | **OBSERVACION** | [BRIEF-DIARIO.md:139](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:139), [LFPG_DeviceRegistry.c:116](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_DeviceRegistry.c:116) | El dueño no está registrado durante replay; se descarta como MISSING; aparece después. | No hay segundo intento. No he demostrado que el servidor cargue esos persistentes tarde por streaming. | Probar una barrera de carga suficiente o conservar entradas pendientes para dueños tardíos. |
| **O-03** | **OBSERVACION** | [BRIEF-DIARIO.md:136](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:136), [LFPG_FileUtil.c:82](C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_FileUtil.c:82) | Diario lleno; cambia un dueño; el Tick serializa y escribe todo el documento, hace read-back y copias. | Trabajo síncrono potencialmente grande; tirón y duración sin medir. | Añadir prueba saturada y presupuesto de tiempo para el Tick completo. |
| **O-04** | **OBSERVACION** | [BRIEF-DIARIO.md:27](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:27) | Se restaura deliberadamente un backup CE cuyo JSON coincide con la base, conservando el diario posterior. | El CAS restaura los cambios posteriores y deshace parcialmente la intención del rollback. No distingue un estado repetido de otra época —problema ABA—. | Definir cómo se restauran conjuntamente CE y diario, o cómo se identifica la generación del storage. |

## 1. Símbolos existentes y visibilidad

**Todos los símbolos existentes preguntados están presentes.** En particular, los dos métodos de estado están en la base de `4_World`.

| Símbolo | Definición real |
|---|---|
| `LFPG_DeviceAPI.HasWireStore(Object)` | `scripts/4_World/LFPG_IDevice.c:604` |
| `LFPG_DeviceAPI.GetDeviceId(Object)` | `scripts/4_World/LFPG_IDevice.c:510` |
| `LFPG_DeviceAPI.GetWiresJSON(Object)` | `scripts/4_World/LFPG_IDevice.c:614` |
| `LFPG_DeviceAPI.ClearDeviceWires(Object)` | `scripts/4_World/LFPG_IDevice.c:635` |
| `LFPG_DeviceAPI.AddDeviceWire(Object, LFPG_WireData)` | `scripts/4_World/LFPG_IDevice.c:624` |
| `LFPG_DeviceRegistry.FindById(string, EntityAI known = null)` | `scripts/4_World/LFPG_DeviceRegistry.c:116` |
| `LFPG_DeviceRegistry.IsAmbiguous(string)` | `scripts/4_World/LFPG_DeviceRegistry.c:188` |
| `IsStartupValidationDone()` | Base `scripts/4_World/LFPG_NetworkManager.c:135`; override `scripts/5_Mission/LFPG_NetworkManagerImpl.c:616` |
| `IsValidationActive()` | Base `scripts/4_World/LFPG_NetworkManager.c:143`; override `scripts/5_Mission/LFPG_NetworkManagerImpl.c:620` |
| `AtomicSaveVanillaWires(string, LFPG_VanillaWireStore)` | `scripts/3_Game/LFPG_FileUtil.c:74` |
| `EnsureVanillaWiresFileOrRestore(string)` | `scripts/3_Game/LFPG_FileUtil.c:809` |
| `IsRecoveryFileReadable(string, int)` | `scripts/3_Game/LFPG_FileUtil.c:350`, `protected static` |
| `FlushVanillaIfDirty()` | Base `scripts/4_World/LFPG_NetworkManager.c:475`; override `scripts/5_Mission/LFPG_NetworkManagerImpl.c:3771` |
| `LFPG_ValidationFinalize()` | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:3166` |
| `wasStartupPending` | Local en `scripts/5_Mission/LFPG_NetworkManagerImpl.c:3178`; guarda en `:3180` |
| `m_SchedFlushMs` | Miembro en `scripts/5_Mission/LFPG_NetworkManagerImpl.c:207`; uso en `:450` |
| `ValidateAllWiresAndPropagate()` | Base `scripts/4_World/LFPG_NetworkManager.c:453`; override `scripts/5_Mission/LFPG_NetworkManagerImpl.c:2873` |
| `BroadcastOwnerWires(EntityAI)` | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:1625` |
| `BroadcastOwnerWireDelta(EntityAI, array<int>, array<ref LFPG_WireData>)` | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:1832` |
| `QueueBroadcastOwnerSnapshot(EntityAI, array<vector>, bool)` | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:1497` |
| `QueueBroadcastOwnerSnapshotFromWires(EntityAI, array<ref LFPG_WireData>)` | `scripts/5_Mission/LFPG_NetworkManagerImpl.c:1518`, `protected` |

`LFPG_WireJournal`, sus clases de datos, constantes y getters nuevos **no existen en la base**, como corresponde a una propuesta sin implementar.

## 2. Compare-and-swap, determinismo y net ids

**No encuentro un `STALE` inevitable por reserializar durante la carga: el brief evita precisamente esa comparación.** D3 conserva la cadena cruda leída mediante `ctx.Read(json)`; el CAS la compara con las cadenas guardadas. No compara el array ya deserializado y vuelto a serializar.

El helper:

- Copia los cables al blob en el orden del array, sin ordenación canónica: `scripts/3_Game/LFPG_WireHelper.c:302`.
- Serializa mediante `MakeData(..., false)`: `scripts/3_Game/LFPG_WireHelper.c:312`.
- Puede normalizar campos, recortar waypoints, descartar cables y deduplicar al cargar: `scripts/3_Game/LFPG_WireHelper.c:35`, `:370`, `:395`, `:403`.

Por tanto, **no debe exigirse igualdad byte a byte después de deserializar y reserializar**. El orden de campos y el formateo de números dependen del serializador nativo, cuya implementación no está en estas fuentes.

Los net ids sí forman parte del JSON: `m_TargetNetLow/High` están declarados sin `NonSerialized` en `scripts/3_Game/LFPG_Data.c:54`. El comentario «session-only» no los excluye. La validación los reemplaza en `scripts/5_Mission/LFPG_NetworkManagerImpl.c:3019` e invalida la caché en `:3031`.

Eso no rompe por sí solo el primer CAS contra la cadena cruda. **Sí explica B→B′ y el `DROP` de WJ-02**, y genera más estados intermedios que WJ-01 debe conservar correctamente.

## 3. Orden de arranque

El código programa la primera validación mediante `CallLater(..., 5000, ...)` en `scripts/5_Mission/LFPG_NetworkManagerImpl.c:362`. Eso es un retraso temporal; no es una barrera explícita que certifique «todos los `OnStoreLoad` completados».

Además:

- El registro puede existir desde `EEInit`, antes de cargar el registro persistido: `scripts/4_World/lfpg_devicebase.c:237`, `:260`, `:398`.
- `AfterStoreLoad` certifica la carga de una entidad y sus hijos, no de todos los aparatos del mundo: vanilla `3_game/entities/entityai.c:1418`.
- La validación toma snapshots del registro en `LFPG_NetworkManagerImpl.c:2930` y `:2962`.
- La medición grande tardó unos 52 s y necesitó una segunda pasada: `INFORME-2.md:83`.

**Un Tick puede ejecutarse antes del replay.** El scheduler ya está arrancado; la guarda `!m_Loaded` propuesta impide que escriba el diario. Esa parte está bien.

**No hay una guarda que impida `OnStoreSave` antes del replay.** `OnOwnerSaved` sería inocuo mientras `m_Loaded=false`, pero D3 actualizaría igualmente `m_LastStoredWireJSON`.

Después del replay hay muchas fases repartidas entre ticks. El flush del scheduler está en `:450`, y la siguiente porción de validación se procesa después, en `:509`. El brief permite escribir el diario durante esas fases. La recaptura únicamente al final no elimina WJ-02.

Para aparatos tardíos, el comportamiento propuesto es inequívoco: MISSING se borra y `m_Loaded` impide repetir el replay. **La existencia real de esa carga tardía en servidor queda sin verificar**, O-02.

## 4. Vida de las entradas: ACK, DROP, ciclos y reversión

**Sí: pueden retirarse entradas sin demostrar que la CE contiene ese estado recuperable.**

`OnStoreSave` es un callback para recoger datos, según vanilla `3_game/entities/entityai.c:2894`. D3 coloca el aviso **antes de `ctx.Write(json)`**. Además, `Serializer.Write` devuelve `bool`, vanilla `1_core/proto/serializer.c:57`; el aviso ni siquiera depende del éxito de esa escritura.

- **DROP:** elimina inmediatamente por diferencia de JSON. WJ-02 demuestra por qué eso no equivale a «superseded de forma durable».
- **ACK:** registra una hora de emisión; los 300 s no aportan una confirmación de disco.
- **ID sucio durante varios ciclos:** el veredicto puede ser IGNORE mientras D3 sigue adelantando el valor que el siguiente Tick utilizará como base.
- **Cambio y reversión:** no toda reversión falla. El control A→B→A, recuperando A de disco, acaba correctamente en UNCHANGED. El defecto aparece al perder estados intermedios posibles, no por la mera existencia de una reversión.

Los ~53–73 s entre vueltas y ~13–15 s por vuelta del informe son mediciones de DayZDiag en un mundo pequeño, **no cotas del servidor real**. BI también describe el guardado periódico por segmentos; no debe inferirse un commit global a partir del callback de un aparato. [Configuración de la CE](https://community.bistudio.com/wiki/DayZ%3ACentral_Economy_Configuration).

## 5. Cobertura de D5.1 y escritores

El grep y la lectura de sus consumidores encuentran esta cobertura:

| Escritura | Publicación posterior |
|---|---|
| Alta y sustitución: `LFPG_RPCServerHandlerImpl.c:602`, `:630` | `BroadcastOwnerWireDelta`, `:688` |
| Reroute: `LFPG_RPCServerHandlerImpl.c:886` | `BroadcastOwnerWireDelta`, `:904` |
| Corte del dueño: `LFPG_RPCServerHandlerImpl.c:1033`, `:1037` | `BroadcastOwnerWireDelta`, `:1043` |
| Corte OUT: `LFPG_RPCServerHandlerImpl.c:2042` | `PublishOwnerCutDelta`, `:2051`; publica en `:1937` o `:1942` |
| Corte IN y `RemoveWiresTargeting`: `LFPG_NetworkManagerImpl.c:1260` | Delta o snapshot en `:1278`, `:1280`, `:1285` |
| Corte por fallback: `LFPG_RPCServerHandlerImpl.c:2197` | `PublishOwnerCutDelta`, `:2206` |
| `CutAll`, almacén propio: `LFPG_NetworkManagerImpl.c:4127` | Snapshot, `:4128` |
| `CutAll`, fallback sobre otros dueños: `:4206` | Snapshot, `:4220` |
| Poda en validación: `:3086` | Broadcast, `:3090` |
| Poda diferida: `:3680` | Broadcast, `:3684` |

**Los cambios normales de topología consultados llegan a las cuatro funciones. Llegar al enganche no garantiza quedar registrado: WJ-03 lo impide durante validación.**

Hay escrituras que no pasan por ellas:

- Carga/deserialización: WireOwner `:150`, Generator `:939`.
- Refresco de net ids: `LFPG_NetworkManagerImpl.c:3020`.
- Los propios mutadores públicos de WireOwner, Generator y DeviceAPI no publican automáticamente. La cobertura depende de sus llamadores.

`LFPG_DeviceAPI` delega Add/Clear/Prune en `LFPG_IDevice.c:624`, `:635`, `:645`, `:656`. Los mutadores del Generator están en `LFPG_TestDevices.c:793`, `:821`, `:834`, `:847`; sus llamadores normales quedan cubiertos por la tabla.

Existe también `QueueBroadcastOwner`, `LFPG_NetworkManagerImpl.c:1424`, fuera de D5.1. No encontré llamadores en estos fuentes; su mera existencia no demuestra una pérdida actual.

## 6. Generator y almacenes congelados

**El camino separado del Generator es correcto.**

Es una entidad real, `config.cpp:249`, y hereda de `PowerGenerator`, `LFPG_TestDevices.c:53`. Tiene almacén propio; DeviceAPI llega a sus métodos mediante el fallback dinámico. No depende del cast a `LFPG_WireOwnerBase`.

Su secuencia de guardado es exactamente:

```text
m_DeviceIdLow, m_DeviceIdHigh, m_SourceOn, json
```

Está en `LFPG_TestDevices.c:890`; la carga correspondiente comienza en `:903`.

WireOwner congela tanto JSON ilegible como de esquema futuro: `LFPG_WireOwnerBase.c:150`, `LFPG_WireHelper.c:340`, `:361`. Reemite la cadena original en `LFPG_WireOwnerBase.c:111`. Excluirlo de replay y captura es coherente.

El Generator ignora el `false` de `DeserializeJSON` en `LFPG_TestDevices.c:939` y vuelve a serializar en `:899`. **Es el defecto preexistente que el brief ya identifica**, no un descubrimiento nuevo ni algo resuelto por el diario.

## 7. Compilación, tipos y guardado tipado

**No encuentro un bloqueo de módulos en la distribución propuesta.**

- `config.cpp:218`, `:223`, `:228` carga las carpetas completas de Game, World y Mission.
- Journal y sus llamadores de entidad estarán en `4_World`.
- Los dos métodos consultados sobre `nm` ya pertenecen a la base World.
- Los datos estarán en `3_Game`, visibles para `LFPG_FileUtil` y para World.

No hay motivo para inventar una dependencia del orden alfabético de ficheros: `LFPG_IDevice.c:617` ya utiliza `LFPG_WireOwnerBase`, definido en otro fichero posterior alfabéticamente. Esto no sustituye una compilación del código futuro.

Las clases de D2 siguen un patrón ya existente: `LFPG_VanillaWireStore`, `LFPG_Data.c:114`, con constructor, versión y array de referencias. Los campos y constructores propuestos son compatibles con ese patrón.

Sobre JSON anidado:

- Las dos cadenas deben entregarse al serializador como strings; no deben escaparse manualmente.
- `JsonFileLoader.SaveFile` genera el documento completo y lo escribe con `FPrint`: vanilla `3_game/tools/jsonfileloader.c:42`.
- `LoadFile` lee hasta **100.000.000 bytes**, `:3` y `:19`. Esa es una cota de lectura del wrapper; **no demuestra un límite de longitud de string ni una cota de escritura del motor**.
- `JsonSerializer.WriteToString/ReadFromString` son primitivas nativas: vanilla `3_game/gameplay.c:68`, `:100`. Su implementación y límites efectivos no están aquí.

La copia de `AtomicSaveVanillaWires` tampoco verifica igualdad de los campos recuperados: el read-back comprueba que el fichero se deserializa, `LFPG_FileUtil.c:101`. Después de copiar al destino, elimina el `.tmp` sin read-back del destino, `:120–141`.

El nombre `AtomicSave` describe una sustitución recuperable mediante archivos auxiliares. La operación real es borrar y copiar; no demuestra durabilidad frente a un apagado del host.

**EOL:** el brief coincide con los blobs de la base: NetworkManagerImpl lleva CRLF y los otros archivos examinados LF. El worktree los presenta como CRLF con `core.autocrlf=true`; no lo clasifico como defecto del plan.

## 8. Tope y coste

**No puedo dar un tirón medido ni una duración en milisegundos sin ejecutar el consumidor nativo.**

Sí puedo demostrar que 512 entradas no equivale a un fichero pequeño. Calculé en memoria documentos sintéticos con dos snapshots por dueño, los campos de `LFPG_WireData` y diez waypoints por cable:

| Dueños | Cables por dueño | Tamaño aproximado |
|---:|---:|---:|
| 512 | 4 | 2,24 MiB |
| 512 | 16 | 8,74 MiB |
| 512 | 64 | 34,75 MiB |

Son cálculos de tamaño con JSON de Python, **no salidas de `JsonFileLoader`, cotas estrictas ni configuraciones jugables demostradas**. Las constantes del helper son 64 cables y diez waypoints: `LFPG_Defines.c:17`.

Un solo dueño modificado provoca la escritura del store completo, read-back y copias de backup/promoción. El coste depende del tamaño total retenido, además de los dueños sucios.

J10 solo pide 30 cableados y contar guardados. No prueba el caso de 512 entradas grandes. Falta precargar el diario, modificar un único dueño y medir el Tick completo —capturas, serialización, verificación y copias— con un presupuesto de aceptación.

## 9. Tests: qué prueban y qué no

Las tablas de verdad de las funciones puras **son pruebas válidas de esas reglas**. Sus mutantes demuestran sensibilidad a ramas concretas.

Pero no validan que las reglas conserven datos:

- Los tests estructurales comprueban presencia y posición de llamadas.
- El ejecutor escalar elimina tipos y directivas y usa Python: `.github/tools/enforce_scalar_slice.py:19`, `:67`, `:75`. Su cabecera declara expresamente que no ejecuta la VM de Enforce.
- No existe un consumidor CE independiente con estados «emitido» y «durable» separados.

**WJ-01 puede pasar todos los tests pedidos.** Incluso la prueba en partida evita su ventana principal: J1/J2 exigen matar antes de la siguiente vuelta; J3 antes de la primera vuelta. No fuerzan la muerte **después del callback y antes de que la vuelta sea recuperable**.

Los contramodelos ejecutados reprodujeron:

```text
cambio B → OnStoreSave(B) → Tick → muerte recuperando A       ⇒ STALE
cambio B → Tick → OnStoreSave(B) → cambio C → Tick → muerte   ⇒ STALE
RESTORE B → refresco a B′ → OnStoreSave(B′) → Tick → muerte   ⇒ sin entrada
```

Los controles con base ya consolidada y con reversión simple dieron RESTORE y UNCHANGED, respectivamente.

Faltan pruebas del protocolo completo para esas secuencias, lifecycle durante validación, fallo de serialización interior, versión futura con artefactos antiguos, recuperación desde backups y carga tardía.

## 10. Qué puede estar mal en la premisa

**El problema demostrado no es necesariamente todo el problema del jugador.** `INFORME-2.md:16` limita la conclusión: explica cables recientes perdidos tras una parada sucia sin vuelta intermedia. Si desaparecen cables antiguos en cada reinicio, la medición no demuestra esa causa. El mismo informe documenta desaparición de dispositivos completos, `:86`, fuera del remedio propuesto.

También hay tres límites del encargo:

- **Es un snapshot periódico, no un diario previo a cada operación.** Una muerte antes del siguiente flush seguirá perdiendo cambios. Los cinco segundos nominales tampoco son una garantía temporal bajo carga o errores de I/O.
- **El CAS no aporta por sí solo conservación.** Puede impedir sobrescribir un tercer estado, pero borrar la entrada también destruye la posibilidad de recuperación. La frase del brief que reduce D4 a una cuestión de tamaño es incorrecta.
- **No distingue intenciones de rollback ni demuestra resistencia a un apagado eléctrico.** La igualdad de cadenas no identifica la época del storage; cerrar/copiar archivos no prueba que sus datos hayan llegado al soporte físico.

## LO QUE NO PUDE VERIFICAR

- Compilación del código futuro en Enforce; el diario todavía no está implementado.
- Orden nativo completo entre carga del mundo, callbacks y consolidación de cada segmento CE.
- Aparatos persistentes cargados tarde en servidor y por streaming; no lo doy por demostrado.
- Límites actuales del serializador/escritor nativo, round-trip de cadenas largas escapadas y duración del fichero saturado.
- Una cota de guardado CE en el servidor de producción, ni recuperación tras pérdida eléctrica.

No modifiqué archivos, no arranqué DayZ y no generé commits.