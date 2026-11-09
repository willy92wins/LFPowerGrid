<!-- Salida literal de Codex (codex exec, solo lectura), revision de plan de BRIEF-DIARIO.md v2. Transcrita por el receptor sin editar. -->

BASE: claude/project-thread-0iolln @ 9bedcc8d8b2b16272c0c51354aeb421442d57c06

**Veredicto: NO-APTO.**

La v2 corrige los contramodelos originales más simples, pero sigue dependiendo de un límite de recuperación de la CE que no está demostrado. Además, introduce una incompatibilidad concreta entre escritura y recuperación del diario: el escritor necesita archivos que el lector tiene prohibido leer.

Revisión exclusivamente de lectura, sin subagentes, modificaciones ni arranques de DayZ. HEAD avanzó desde `284c2dd` durante la revisión; el único cambio fue añadir `REVIEW-codex-r4-toggle.md`. El código y los documentos aquí revisados permanecieron iguales.

Ejecuté en memoria una traducción de D3/D4. Sus resultados demuestran transiciones del protocolo propuesto; **no son ejecución de Enforce ni prueba del comportamiento nativo de la CE**. Uso `(W,S,P)` para representar `m_WiresJSON`, `m_SavedJSON` y `m_SavedPrevJSON`.

## 1. Cierre de WJ-01 a WJ-05

| Hallazgo anterior | Resultado contra la v2 | Cierre |
|---|---|---|
| **WJ-01** | `carga A → cambio B → Save(B) → Tick` guarda `(B,B,A)`. Con CE=A restaura B; con CE=B deja B. La variante `cambio B → Tick → Save(B) → cambio C → Tick` guarda `(C,B,A)` y restaura C desde A o B. | **Cerrados los dos contramodelos de v1. Cierre general pendiente:** dos emisiones distintas pueden desplazar A; véase D2-01. |
| **WJ-02** | Entrada `(B,A,"")`, CE=A, RESTORE B, validación B′, `Save(B′)` antes de recapturar: ahora marca RECAPTURE y termina con `(B′,B′,A)`. Desde CE=A vuelve a B′. Si ya estaba dirty, IGNORE también conserva la protección hasta el Tick. | **Cerrado el DROP original.** La retirada por DURABLE tiene otro supuesto sin acreditar, D2-02. |
| **WJ-03** | Durante validación se conserva dirty; el Tick no captura ni lo vacía. Al terminar, el siguiente Tick captura el corte. | **Cerrado el descarte original**, también para validaciones posteriores al arranque. |
| **WJ-04** | `live=""` conserva la entrada anterior. En replay, `W=""` resulta UNUSABLE y no llega a `ClearDeviceWires`. | **Cerrado el vaciado indebido.** Queda un problema distinto: KEEP pierde el pendiente de reintento, D2-05. |
| **WJ-05** | Principal v2 y `.tmp` v1: ya no se promueve el temporal antes de comprobar el principal. | **Cerrado el mecanismo original**, sujeto a que el sondeo tipado funcione. La barrera no cubre correctamente todos los candidatos: D2-04. |

Estas salidas corresponden a las reglas de [BRIEF-DIARIO.md:122](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:122) y a la carga propuesta en [BRIEF-DIARIO.md:162](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:162).

## 2. Hallazgos

En **D2-01 y D2-02**, la pérdida está reproducida en el protocolo. Que la CE permita las secuencias desencadenantes sigue sin medirse. No las presento como fallos observados del motor.

| ID | Clase | Path:line | Secuencia exacta | Salida mala | Cambio mínimo |
|---|---|---|---|---|---|
| **D2-01** | **INVALIDA-DISENO** del contrato de recuperación anunciado | [BRIEF-DIARIO.md:26](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:26), [desplazamiento:94](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:94), [captura:150](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:150) | Carga A; cambio B; `Save(B)`; cambio C; `Save(C)`; Tick escribe `(C,C,B)`; muerte; CE recupera A. | `STALE`, elimina la entrada y deja A: **pierde C aunque el diario lo había escrito**. | No desplazar estados todavía recuperables basándose únicamente en emisiones. Acreditar primero el límite de recuperación de la CE y conservar los estados que ese límite permita. Añadir simplemente un tercer estado tampoco resuelve un número no acotado de emisiones sin consolidar. |
| **D2-02** | **INVALIDA-DISENO** de la prueba de durabilidad | [BRIEF-DIARIO.md:44](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:44), [constante:71](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:71), [veredicto:125](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:125) | Diario protege B frente a CE=A; emisión B en t=1 s; otra emisión B en t=21 s dentro de una operación todavía no recuperable; DURABLE; Tick persiste la eliminación; muerte recuperando A. | **Pierde B**, incluso si llevaba mucho más de cinco segundos protegido. | No tratar el tiempo entre callbacks como confirmación. La retirada necesita un contrato acreditado de pasadas/consolidación; mientras falte, debe declararse y diseñarse como retención de mejor esfuerzo. |
| **D2-03** | **INVALIDA-DISENO** | [BRIEF-DIARIO.md:79](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:79), [lector:164](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:164), [LFPG_FileUtil.c:110](C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_FileUtil.c:110), [promoción:120](C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_FileUtil.c:120) | Principal contiene una entrada B ya guardada; todavía no existe `.bak`. Actualización C: escribe/verifica `.tmp=C`, copia principal a `.bak.new=B`, borra principal; muerte antes de copiar el temporal. | Al arrancar solo considera principal y `.bak`: ambos ausentes. **Ignora B y C**, aunque B está íntegro en `.bak.new`. Con CE=A pierde también el estado anterior del diario. | Hacer compatibles escritor y lector. Puede conservarse una carga sin promociones, pero debe leer los candidatos necesarios para cubrir estas ventanas, comprobando sus versiones antes de cualquier sustitución. |
| **D2-04** | **INVALIDA-DISENO** de la barrera de versión futura | [BRIEF-DIARIO.md:164](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:164), [borrado:171](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:171), [fallback:173](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:173) | Principal sondeable con `ver=1`; `.bak` con `ver=2`; ajuste desactivado. El sondeo del principal tiene éxito, por lo que no sondea `.bak`; ejecuta el borrado de todos los archivos. | **Borra el respaldo futuro** pese a la promesa de dejarlo intacto. Además, si el principal pasa el sondeo pero falla la carga completa, el texto permite pasar al respaldo sin comprobar expresamente su versión. | Comprobar la versión de cada candidato antes de cargarlo, borrarlo o reemplazarlo. La versión del principal no autoriza operar sobre el respaldo. |
| **D2-05** | **INVALIDA-DISENO** del tratamiento de errores de captura | [BRIEF-DIARIO.md:152](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:152), [vaciado:156](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:156), [LFPG_WireOwnerBase.c:166](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_WireOwnerBase.c:166) | Cambio B marca dirty; falla una serialización; `live=""`; KEEP; al terminar se vacía `m_Dirty`. No hay otro cambio. | B queda sin capturar indefinidamente. En WireOwner, además, el getter marca como válida la generación incluso cuando la serialización devuelve `""`: repetir el getter puede seguir devolviendo el fallo cacheado. | KEEP debe conservar un pendiente de reintento y permitir una nueva serialización real, invalidando la caché fallida cuando corresponda. Existe `LFPG_InvalidateWireJSONCache()` en `:176`. |

### Correcciones menores del brief

| ID | Clase | Path:line | Secuencia exacta | Salida mala | Texto exacto propuesto |
|---|---|---|---|---|---|
| **M2-01** | **CORRECCION-MENOR-DEL-BRIEF** | [BRIEF-DIARIO.md:123](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:123), [S7:248](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:248) | Aparato nuevo; primera emisión E; cambio B; Tick: `saved=E`, `prev=""`, `live=B`. | STORE crea `(B,E,"")`. «Colocado en la sesión» ya no implica `ultimo=""` ni ausencia de entrada. | **«savedEmpty indica que no se ha leído ni emitido una cadena no vacía; no demuestra ausencia del aparato en disco. S7 cubre un aparato nuevo todavía no emitido. Añadir el caso nuevo → emisión E → cambio B: se admite una entrada; si el aparato no vuelve, replay la elimina como MISSING.»** |
| **M2-02** | **CORRECCION-MENOR-DEL-BRIEF** | [BRIEF-DIARIO.md:146](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:146), [PRUEBA-DIARIO.md:54](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/PRUEBA-DIARIO.md:54) | Se implementa literalmente FIRST_EMIT como asignación de hora, sin log. | J11 exige `first-emit <id>` pero D4.2 no exige emitir esa línea. | **«FIRST_EMIT: m_NewestEmitMs[id] = ahora y Debug `[WireJournal] first-emit <id>`.»** |

### Observaciones

| ID | Clase | Path:line | Secuencia exacta | Salida mala o incertidumbre | Cambio mínimo |
|---|---|---|---|---|---|
| **O2-01** | **OBSERVACION** | [INFORME-2.md:63](C:/tmp/lfpg-cables-reinicio/INFORME-2.md:63), [mezcla:122](C:/tmp/lfpg-cables-reinicio/INFORME-2.md:122) | `.bin=C`, `.001=B`, `.002=A`; el motor selecciona A por corrupción o recuperación. | Con `(D,C,B)` se obtiene STALE. **La selección efectiva de `.002` no está demostrada por ese informe.** | Medir qué generaciones puede seleccionar el motor y bajo qué daños/stamps. |
| **O2-02** | **OBSERVACION** | [BRIEF-DIARIO.md:145](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:145), [captura:155](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:155) | Cada emisión genera B₁, B₂, B₃…; cada una difiere de la entrada capturada. | RECAPTURE permanente, escrituras por pasada, sin DURABLE y con TTL renovado. No he demostrado esa variación en cada emisión real. | Medir estabilidad del JSON y separar el coste de cambios de topología del de campos transitorios. |
| **O2-03** | **OBSERVACION** | [BRIEF-DIARIO.md:236](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:236), [estructura:261](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:261) | Mutante **[DESIGN]** del gestor: asignar `saved` a `m_SavedPrevJSON` en lugar de `prev`, manteniendo correctas las funciones puras y el modelo Python. | El modelo puede seguir verde mientras el gestor pierde A en S1. Las comprobaciones estructurales enumeradas no exigen verificar esa asignación. | Exigir que un mutante del gestor real falle. Ejecutar también sus transiciones/asignaciones, no sustituirlas íntegramente por otro modelo. |

## 3. Respuestas a los puntos a–j

### a) «La CE solo puede cargar último o anterior»

**No está demostrado.** El informe acredita:

- Mtimestamps agrupados en vueltas de unos 13–15 s, cada 53–73 s, en un mundo pequeño: [INFORME-2.md:14](C:/tmp/lfpg-cables-reinicio/INFORME-2.md:14).
- Existencia de `.bin`, `.001` y `.002`: `:63`.
- Un recovery stamp concreto: `:107`.
- Mezcla de `.bin` y `.001` tras un kill a mitad de vuelta: `:122`.

No acredita que cada entidad se emita como máximo una vez por vuelta, que las vueltas no se solapen ni que `.002` nunca sea seleccionable. Las dos primeras propiedades aparecen como hechos nuevos en [BRIEF-DIARIO.md:22](C:/tmp/lfpg-grok/wt-nostra-131/reviews/2026-11-08-nostra-131/BRIEF-DIARIO.md:22), sin medición que las sustente.

**Tres cambios no equivalen a tres emisiones.** Si se cumple una sola emisión por entidad y pasada, tres cambios dentro de esa pasada pueden quedar cubiertos por estado vivo, último emitido y anterior. Si hay varias emisiones distintas antes de consolidar, A puede salir de la ventana, como reproduce D2-01.

Dos vueltas pendientes simultáneamente serían incompatibles con la premisa de que no se solapan y que terminar una vuelta la hace recuperable. Si esas propiedades son falsas, desaparecen tanto la justificación de los dos estados como la de DURABLE.

### b) DURABLE y los 20 segundos

**Veinte segundos por sí solos no prueban nada sobre la finalización.**

Si fueran ciertas «una emisión por entidad y pasada» y «pasadas no solapadas», una segunda emisión identificaría una pasada posterior: la primera habría terminado **aunque durase más de veinte segundos**. En ese razonamiento, el trabajo lo hacen las dos premisas, no PASS_GAP.

Si pueden repetirse callbacks dentro de una pasada larga, dos emisiones separadas veinte segundos permiten retirar prematuramente la entrada: D2-02. Se pierde el cambio protegido completo, no solamente la última ventana de cinco segundos.

**No he verificado que la CE reemita entidades sin cambios.** Vanilla describe `OnStoreSave` como recogida de datos para persistencia, pero no expone aquí su política de selección: [entityai.c:2894](E:/DayZ-Stable-Extract/1.30.164200/P-scripts/3_game/entities/entityai.c:2894).

Si no reemite, no habrá segunda emisión; actuará el TTL. Borrar por TTL tampoco demuestra durabilidad. Por tanto, «sigue siendo seguro, solo limpia más tarde» es una garantía excesiva.

### c) Desplazamiento frente a captura y Tick fallido

La secuencia solicitada da exactamente:

```text
Carga A
cambio B → Save(B): último=B, anterior=A
cambio C → Save(C): último=C, anterior=B
Tick: disco=(C,C,B)
muerte → CE=A
replay: STALE → queda A
```

**Sí, pierde C.** Su realizabilidad depende del contrato nativo discutido en a).

También ejecuté el caso donde el disco conserva el diario anterior `(B,A,"")` y falla el Tick que intentaba escribir C:

| CE recuperada | Resultado |
|---|---|
| A | RESTORE B |
| B | UNCHANGED B |
| C | STALE, conserva C |

El Tick fallido no permite declarar C protegido. Eso no implica perder B si el principal anterior permanece íntegro. **Si el fallo interrumpe la sustitución del archivo, puede perderse también B por D2-03.**

### d) Sesión recién arrancada, J3 y J12

**Esta parte funciona para las secuencias simples propuestas.**

Tras cargar A: `ultimo=A`, `anterior=""`. RESTORE modifica el estado vivo a B, sin modificar esos dos miembros.

- **J3, recaptura antes de emitir:** produce `(B,A,"")`; otra caída con CE=A restaura B.
- **J12, una emisión B antes de recapturar:** desplaza a `ultimo=B`, `anterior=A`; produce `(B,B,A)`; desde A restaura B y desde B deja B.
- Con refresco a B′, una sola emisión produce `(B′,B′,A)`: sigue reconociendo A.

`OnStartupValidationDone` vuelve a marcar los restaurados, y el Tick aplaza la captura durante validación. No encuentro aquí una pérdida inevitable por inicializar `anterior=""`. Esto no cubre las emisiones adicionales de D2-01.

### e) `savedEmpty` en aparatos nuevos ya emitidos

**No identifica «aparato nuevo no durable».** Solo identifica ausencia de una cadena cargada o emitida.

Un aparato creado, emitido como E y después cambiado a B puede generar una entrada. No es por sí mismo corrupción: si no reaparece, MISSING la descarta; si reaparece con E, puede restaurar B. La explicación y S7 necesitan la precisión de M2-01.

### f) RECAPTURE, escrituras y net IDs

**No hay recursión inmediata ni un bucle que bloquee el hilo.** Hay una posible repetición entre emisiones y ticks:

```text
emisión B₁ → dirty → Tick captura B₁
emisión B₂ → dirty → Tick captura B₂
...
```

Si el JSON cambia siempre, nunca obtiene dos emisiones coincidentes y cada captura renueva el TTL. Puede mantener ocupadas entradas indefinidamente y escribir en cada pasada.

Los net IDs sí están serializados: [LFPG_Data.c:54](C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_Data.c:54). La validación los modifica e invalida la caché en [LFPG_NetworkManagerImpl.c:3019](C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_NetworkManagerImpl.c:3019).

**Eso no demuestra que cambien en cada emisión.** WireOwner reutiliza su caché; el Generator serializa directamente al guardar. El churn permanente es O2-02, no una causa raíz confirmada.

### g) Sondeo JSON corrupto y clase con solo `int ver`

**El helper actual tiene un canal normal de fallo: `false` y mensaje.**

En vanilla 1.30.164200:

- `LoadFile` lee el archivo y llama a `ReadFromString`.
- Si devuelve false, construye `errorMessage` y retorna false: [jsonfileloader.c:27](E:/DayZ-Stable-Extract/1.30.164200/P-scripts/3_game/tools/jsonfileloader.c:27).
- No llama a `ErrorEx` en esa ruta.
- El deprecated `JsonLoadFile` sí llama a `ErrorEx`: `:128`.

Por tanto, **no corresponde afirmar que un JSON corrupto causa necesariamente excepción o `SCRIPT (E)` al usar `LoadFile`**.

La aceptación de campos adicionales por una clase con solo `ver` queda sin verificar: `ReadFromString` es una primitiva cuya implementación no está en el extracto, [gameplay.c:100](E:/DayZ-Stable-Extract/1.30.164200/P-scripts/3_game/gameplay.c:100).

Además, ese sondeo **lee y deserializa el documento completo**; no es una lectura aislada de cabecera. Ya existe el sondeo textual `TryReadRawJsonVersion`, [LFPG_FileUtil.c:865](C:/tmp/lfpg-grok/wt-nostra-131/scripts/3_Game/LFPG_FileUtil.c:865). Su elección no elimina la necesidad de comprobar cada candidato y validar después el documento completo.

### h) Qué representa L y momento del replay

**L coincide con la cadena cruda cargada únicamente mientras ningún guardado posterior la haya desplazado.**

La carga normal de WireOwner pasa por [lfpg_devicebase.c:422](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/lfpg_devicebase.c:422) y lee el JSON en [LFPG_WireOwnerBase.c:137](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_WireOwnerBase.c:137). El Generator tiene la ruta separada en [LFPG_TestDevices.c:932](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_TestDevices.c:932).

No encontré otra ruta de lectura de cables de los dueños nativos elegibles que evite esas dos. Las cargas rechazadas antes de llegar al JSON dejarían el miembro nuevo vacío y replay resultaría UNUSABLE. Los hooks de extras de los descendientes revisados no sustituyen la lectura común de cables.

Pero D3 también cambia `ultimo` en cada guardado. **No existe una guarda propuesta que impida ese desplazamiento antes del replay.** Si ocurre, L ya no significa exactamente «lo cargado».

El enganche está bien situado **respecto a las fases de validación**: después del retorno por validación activa y antes de iniciar las fases en [LFPG_NetworkManagerImpl.c:2876](C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_NetworkManagerImpl.c:2876). Así precede al refresco de net IDs, poda, índices y grafo.

Sin embargo, la llamada inicial se programa con un retraso de cinco segundos, [`:362`](C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_NetworkManagerImpl.c:362). **Eso no demuestra una barrera de carga ni ausencia de callbacks previos.** Lifecycle puede cortar cables sin una guarda de startup. La ocurrencia de esos eventos antes del replay queda como OBSERVACION; para cerrar el contrato hace falta conservar separadamente la cadena cargada hasta replay o acreditar el orden necesario.

### i) Cobertura de los cinco CutAll

**Sí: todos llegan a los enganches previstos cuando modifican los almacenes nativos afectados.**

| Origen | Cadena verificada |
|---|---|
| `LFPG_DeviceLifecycle.c:46` y `:66` | `CutAllWiresFromDevice` → implementación en [LFPG_NetworkManagerImpl.c:3988](C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_NetworkManagerImpl.c:3988). |
| `LFPG_DeviceLifecycle.c:114` y `:133` | `CutAllWiresFromMovedDevice` → wrapper [`:3981`](C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_NetworkManagerImpl.c:3981) → llamada común en `:3985`. |
| `LFPG_DeviceLifecycle.c:116` | Llamada directa a la implementación común. |
| Cables propios del aparato | `ClearDeviceWires` en `:4127` → **`QueueBroadcastOwnerSnapshot` en `:4128`**. |
| Otros dueños que apuntaban al aparato, vía índice inverso | `RemoveWiresTargeting` en `:4161` → eliminación en `:1260` → **`QueueBroadcastOwnerSnapshotFromWires` en `:1278/:1285`**, o `BroadcastOwnerWireDelta` en `:1280`. |
| Otros dueños, fallback global | Eliminación en `:4206` → commit de mutación en `:4217` → **`QueueBroadcastOwnerSnapshotFromWires` en `:4220`**. |

Los cinco orígenes están en [LFPG_DeviceLifecycle.c:46](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_DeviceLifecycle.c:46). Los receptores son los métodos de D5.1, definidos en `:1497`, `:1518`, `:1625` y `:1832`.

Esta trazabilidad acredita publicación, no persistencia posterior. En particular, un dueño eliminado puede faltar al llegar el Tick y su entrada se elimina según D4.3.

El full sync sí envía directamente mediante `rpc.Send`, [LFPG_NetworkManagerImpl.c:2185](C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_NetworkManagerImpl.c:2185); no encontré que conectar un jugador marque por esos cuatro enganches.

### j) S1–S8 y posibilidad de falso verde

**S1–S8 no bastan.** El modelo en memoria obtiene los resultados que pide la v2, mientras los contramodelos adicionales anteriores siguen fallando.

Faltan, al menos:

- Dos emisiones distintas antes de capturar, con recuperación A.
- Recuperación de una generación fuera de último/anterior.
- Dos emisiones coincidentes sin consolidación demostrada.
- Interrupciones en las fronteras de escritura, especialmente principal ausente con `.bak.new` válido.
- Principal sondeable pero no cargable; respaldo futuro; desactivación con candidatos futuros.
- KEEP seguido de recuperación del serializador, sin nuevas mutaciones.

**Sí, el modelo Python puede dar verde con un fuente incorrecto.** Las cinco funciones puras se extraen del fuente, pero D3 y las transiciones de mapas del gestor se vuelven a implementar en Python. O2-03 muestra un mutante que esa separación puede ocultar.

El ejecutor existente reconoce expresamente que no ejecuta callbacks, tipos ni efectos de Enforce: [enforce_scalar_slice.py:1](C:/tmp/lfpg-grok/wt-nostra-131/.github/tools/enforce_scalar_slice.py:1). Todavía no existen la implementación ni los tests del diario en HEAD; no afirmo haber ejecutado ese mutante contra una suite inexistente.

J11/J12 mejoran la prueba anterior, pero tres resultados favorables no demuestran el límite universal de generaciones recuperables. J10 sigue siendo una observación de coste, sin probar saturación de 128 entradas ni fijar un presupuesto de aceptación.

## 4. Qué puede estar mal en la premisa

1. **La explicación medida puede cubrir solo una parte del problema original.** El propio [INFORME-2.md:16](C:/tmp/lfpg-cables-reinicio/INFORME-2.md:16) limita su conclusión a cables recientes perdidos tras una parada sucia. No explica cables antiguos que desaparezcan repetidamente tras reinicios limpios.

2. **Una vuelta observada por mtimes no identifica por sí sola una transacción recuperable.** Ni los callbacks, ni sus generaciones, ni la selección de respaldos quedan descritos por esa observación.

3. **Es una captura periódica, no un registro previo a cada operación.** Los cinco segundos son nominales. Validación activa, errores de serialización/I/O y el tope de entradas pueden ampliar la pérdida potencial.

4. **Evitar sobrescribir un estado inesperado y conservar los cambios son propiedades diferentes.** STALE protege el estado CE que encuentra, pero elimina una recuperación potencialmente válida.

5. **Igualdad de JSON no identifica la época del storage.** Persiste el problema ABA de backups restaurados. El comentario del ajuste documenta una operación necesaria; no hace que el protocolo distinga automáticamente un rollback intencional.

6. **La sustitución por copias no demuestra resistencia a pérdida eléctrica.** El helper borra y copia; `SaveFile` escribe mediante `FPrint` y cierra el archivo, sin una confirmación de durabilidad física visible en estas fuentes.

## LO QUE NO PUDE VERIFICAR

- Política nativa de selección entre `.bin`, `.001` y `.002`, incluido el efecto de corrupción y recovery stamps.
- Número de emisiones por entidad y pasada, posible solapamiento y reemisión de entidades sin cambios.
- Orden garantizado entre cargas, guardados, lifecycle y replay.
- Aceptación nativa de campos JSON desconocidos en la clase de sondeo.
- Compilación o ejecución de la implementación futura y de sus tests: todavía no existen en HEAD.
- Recuperación con archivos reales interrumpidos, rendimiento saturado y comportamiento en retail/Nitrado.
- Persistentes que se registren después del replay.
- Durabilidad ante apagado del host.

No ejecuté el linter: no hubo cambios ni una implementación nueva que validar. No modifiqué archivos, no generé commits y no arranqué DayZ.