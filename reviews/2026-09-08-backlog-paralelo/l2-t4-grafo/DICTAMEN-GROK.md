VERDE — 0 GRAVE / 1 MEDIO / 1 MENOR

Revisión adversarial de la lane 2 (T4 grafo eléctrico) contra `BRIEF.md`, `CAMBIOS.diff` e `INFORME.md`. Base del diff: `8de29d5`. Código leído en el árbol ya modificado. No se ha compilado Enforce ni se ha arrancado DayZ.

### MEDIO — G01 cobra otra pasada de aristas al presupuesto de época
- **Dónde:** `scripts/5_Mission/LFPG_ElecGraphImpl.c:3789` (bucle de comparación final; el `Insert` de la pasada 2 está en `:3695`).
- **Qué está mal:** el arreglo de G01 es correcto al comparar el valor final con la foto de entrada, pero cada arista vuelve a incrementar `m_EdgesVisitedThisEpoch` en la pasada nueva. `AllocateOutput` ya visitaba esas aristas en las pasadas 1–3. En un nodo con muchas salidas, o con el presupuesto de época justo, esa visita extra puede cortar el lote de `ValidateConsumerStates` o dejar nodos para la siguiente época.
- **Por qué importa:** no falsea la detección de cambio ni empeora el reparto; sí hace más frágil el techo de trabajo por tick que el resto del grafo ya usa como freno. El informe lo declara y no lo mide.
- **Qué habría que hacer:** o bien no cargar esas visitas al contador de presupuesto (la pasada no recorre topología nueva), o bien documentar el coste extra en el contrato de presupuesto y comprobarlo con un nodo de grado alto.

### MENOR — Tabs metidos en un fichero indentado con espacios
- **Dónde:** líneas añadidas o reindentadas en `scripts/5_Mission/LFPG_ElecGraphImpl.c:81-82`, `:158`, `:3012-3020`, `:3025`, `:3030`, `:3038`, `:3489-3504`, `:3685-3686`, `:3696`, `:3703`, `:3782-3805`; y el bloque nuevo de `scripts/4_World/LFPG_VanillaActionOverrides.c:179-209`.
- **Qué está mal:** el cuerpo previo de ambos ficheros usa espacios. El implementador indentó lo nuevo con tabs y, en las líneas que sí tenía que tocar (`if (hasAnyIncoming)` → `if (canEvaluatePower)`, comparadores de potencia), sustituyó la indentación existente por tab. El informe dice que «la existente permanece intacta»; eso no es cierto en esas líneas tocadas.
- **Por qué importa:** no cambia semántica ni es normalización CRLF del fichero entero. Sí ensucia el diff y mezcla estilos en el mismo bloque.
- **Qué habría que hacer:** alinear lo nuevo al estilo del fichero (espacios) o, si se impone tab, no reindentar la columna de las líneas ya existentes más de lo que pida el cambio de tokens.

## Tabla ficha por ficha

| Ficha | Implementador | Revisor | Notas |
|---|---|---|---|
| G02 — validador apaga baterías que generan desde almacenamiento | ARREGLADA | ARREGLADA | `ValidateConsumerStates` suma `m_VirtualGeneration` al criterio de PASSTHROUGH y deja de exigir arista entrante para evaluar. Casa con el solver en `:2215`. |
| G01 — señal de cambio con asignación final idéntica | ARREGLADA | ARREGLADA | Foto en `:3703`, comparación tras duro+flexible en `:3796-3803`. Se quitaron las dos detecciones intermedias. Queda el coste de presupuesto (MEDIO). |
| G18 — PASSTHROUGH contado como proveedor por la demanda | ARREGLADA | ARREGLADA | El bug era real: `:2385-2419` y `:3886` confirman que `m_OutputPower` del PASSTHROUGH es señal de demanda. El conteo usa entrada+virtual−autoconsumo y respeta compuerta. |
| G04 — fuente vanilla no notifica al solver | ARREGLADA | ARREGLADA | `RefreshSourceState` ya tenía llamador (`LFPG_NetworkManagerImpl.c:3077`). Faltaba el evento vanilla. `OnWorkStart`/`OnWorkStop` en `PowerGenerator` → `RequestPropagate` es la capa correcta; `MarkNodeDirty` no crea nodos si el generador no está en el grafo (`:1958-1960`). |

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

G02 no era solo «sumar `m_VirtualGeneration` al `incomingPower`». El guard `hasAnyIncoming` excluía por construcción una batería sin cable de entrada, aunque el almacenamiento estuviera descargando. El brief apunta a `:3010` / `:3052` como si el único desajuste fuera el sumando; el arreglo correcto tenía que cambiar también *cuándo* se evalúa. El implementador lo vio y acertó. En cambio, el brief no nombra `VerifyPassthroughPowered` (`LFPG_ElecGraphImpl.c:1843`, llamador `LFPG_PumpHelper.c:126`), que sigue mirando solo aristas. Ese predicado no apaga baterías (no es el síntoma de G02) y está fuera de la ficha; sí es el mismo agujero conceptual en el mismo fichero blanco. Ampliarlo habría exigido revisar el contrato de la bomba, y el implementador lo dejó escrito. No lo cuento como fallo de G02.

El validador y el solver siguen sin mirar *exactamente* lo mismo en el origen de la entrada cableada: el solver usa `GetEdgeAllocatedPower` (`:2164`), el validador lee `m_AllocatedPower` crudo (`:2996`, documentado a propósito para no enmascarar brownouts). Eso es anterior a esta lane. Unificarlo no era G02.

G01 está bien diagnosticada. El brief no pide medir tráfico; declarar la ficha cerrada por lectura es honesto solo si nadie toma «menos paquetes» como hecho. El arreglo añade trabajo por llamada. La premisa «trabajo y tráfico de red inútiles» queda a medias: se elimina el trabajo *falso* de reencolar; no se ha demostrado ahorro neto.

G18 venía como P1 «potencial» sin línea. El código confirma el mecanismo (comentario en `:3886`: cualquier PASSTHROUGH escribe demanda en `m_OutputPower`). «Proveedor activo» no puede ser «ya tiene `m_AllocatedPower` > 0»: eso impide recuperar una sobrecarga con dos ramas a cero. Interpretarlo como potencia *disponible* (entrada+virtual−consumo, cero si la compuerta está cerrada) es la lectura no circular. Es una decisión de diseño, no un extra inventado. No replica el tope `m_MaxOutput` del solver; para *contar* un proveedor eso no falsea (un PASSTHROUGH limitado sigue siendo proveedor). La convergencia con N ramas y cambios de compuerta entre ticks no sale de la lectura.

G04 citaba las acciones vanilla (`~:100`, `~:172`) y `RefreshSourceState`. El aviso del council era exacto: el llamador de `RefreshSourceState` ya existía. La ficha no se cierra tocando esas acciones; se cierra observando el estado de trabajo de CompEM. En vanilla, `SetPowered` escribe `m_IsWorking` en `componentenergymanager.c:1741-1743` *antes* de `OnWorkStart` (`:1849` luego `:1857`) y *antes* de `OnWorkStop` por agotamiento (`:1849` luego `:1871`, y `:1891` luego `:1898`). Enganchar los callbacks cubre combustible y no solo el menú. `LFPG_Generator` extiende `PowerGenerator` y llama `super` *antes* de poner `m_SourceOn`; el `IsSource` temprano evita leer ese flag todavía viejo. `modded class PowerGenerator` (no `PowerGeneratorBase`) cubre el generador vanilla (`powergenerator.c:445`); en este árbol vanilla no hay otra clase script que extienda `PowerGeneratorBase`. `BatteryCharger` en script extiende `ItemBase`, no `PowerGenerator`: el `IsKindOf("PowerGenerator")` de config que usa `IsVanillaSource` no arrastra el `modded class`.

Las citas `path:line` del brief venían de `d61705e`. El implementador localizó por contenido. Las citas del informe, comprobadas contra el árbol modificado, coinciden (G02 `:3014/:3018/:2215/:2191`; G01 `:82/:158/:3686/:3703/:3796`; G18 `:3469/:3646/:3494`; G04 `:183/:189/:206` y `RefreshSourceState` en `NetworkManagerImpl.c:3077`). La de `:3801` apunta al `if` que dispara `m_AllocChanged`, no a la asignación (`:3803`); no es una cita inventada.

La lista blanca se respetó en `CAMBIOS.diff`: solo los dos scripts. `INFORME.md` es el entregable que el propio brief de implementación exige.

## LO QUE NO PUDE COMPROBAR

- Compilación real de los módulos World y Mission (no hay compilador Enforce en este encargo).
- Que `GameScript.CallFunction(..., "OnWorkStart")` de CompEM resuelva el `override` del `modded class PowerGenerator` en el binario de esta versión; el patrón es el vanilla, pero no se ejecutó.
- Que `CallFunctionParams` encuentre `LFPG_IsSource` en `LFPG_Generator` (no hereda `LFPG_DeviceBase`; el API ya usa ese camino en producción, no se re-ejecutó).
- Estabilidad in-game de G02 en varias rondas del validador, batería agotada, salida cerrada y autoconsumo cubierto/no cubierto.
- Reducción neta de reencolados y tráfico de G01 frente al coste de la pasada extra.
- Convergencia de G18 con dos proveedores, capacidades desiguales, recuperación de sobrecarga y cambios de compuerta entre ticks.
- Arranque, apagado, agotamiento de combustible y JIP de un `PowerGenerator` vanilla cableado a una carga LFPG.
- Mods terceros que sustituyan `OnWorkStart`/`OnWorkStop` sin `super`.
- El linter offline que el informe dice haber corrido (WARN, 0 errores, 52 avisos); no lo repetí.
- Bytes CRLF de los ficheros tocados; el diff no muestra `^M` ni reorden masivo, y el informe cita `git diff --check` limpio, pero no abrí un volcado hex.
