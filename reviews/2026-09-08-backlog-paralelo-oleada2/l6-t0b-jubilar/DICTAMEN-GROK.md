<!-- Extraido del stream de cursor-agent: --mode ask no puede escribir en el workspace -->
<!-- modelo servido: Cursor Grok 4.6 Extra High | parada: success | coste: None | llamadas a herramienta: 186 -->
Voy a leer `BRIEF-REVIEW.md` y el contexto del workspace para ejecutar el dictamen de principio a fin.El brief pide un dictamen en solo lectura. Empiezo por el encargo original, el informe y el diff.Hay tensión entre la lista blanca del brief y lo que el informe llama «alcance corregido». Voy a contrastar el diff, las citas y el código real.Voy a extraer las líneas añadidas del diff, verificar las citas del informe y leer los ficheros tocados fuera de la lista blanca.Siguiente paso: contrastar cada cita del informe con el código y comprobar si el servidor acepta la entidad base.Estoy en modo Ask: el dictamen está abajo completo, pero no puedo crear `DICTAMEN.md` en disco. Cambia a Agent mode si quieres que lo deje escrito en la raíz.

ROJO — 1 GRAVE / 0 MEDIO / 2 MENOR

La jubilación de la UI V3 en código es real: layouts y clases de panel borrados, mutex dual-open fuera, MissionInit/RPC/acciones alineados a V4, `config.cpp` intacto, citas del informe comprobadas contra el árbol. El veredicto es ROJO porque el diff toca dos ficheros que el `BRIEF.md` no autoriza, y el informe presenta esa salida como lista blanca ya corregida.

---

# DICTAMEN.md

ROJO — 1 GRAVE / 0 MEDIO / 2 MENOR

### GRAVE — Salida de la lista blanca (y el informe la da por autorizada)
- **Dónde:** `scripts/4_World/test/LFPG_ActionOpenSorterPanel_TEST.c:50`, `scripts/4_World/test/LFPG_Sorter_TEST.c:15`, `INFORME.md:13`
- **Qué está mal:** El `BRIEF.md` permite borrar/editar un conjunto cerrado. En ese conjunto están `LFPG_SorterView_TEST.c` y `LFPG_Sorter.c`, no los dos `_TEST` de acción y entidad. El diff cambia `ActionCondition`/`OnExecuteClient` para aceptar `LFPG_Sorter.Cast` y deja `LFPG_Sorter_TEST` como subclase vacía, sin `SetActions`. `INFORME.md:13` y `:143` llaman a eso «lista blanca corregida» / «ampliada». Ese texto no está en el `BRIEF.md` de este workspace.
- **Por qué importa:** El propio brief de revisión avisa de lanes en paralelo sobre ficheros vecinos. Un extra aquí no es un favor: es un choque de merge. Además, «el informe afirma algo falso» respecto del contrato escrito.
- **Qué habría que hacer:** O el receptor declara esos dos paths dentro del alcance y rebasea al resto de lanes, o se revierten y se deja el cableado de la acción V4 a la entidad base para una lane que sí los tenga en lista. No es un parche local disimulable.

### MENOR — Comentario de coexistencia que ya es procedimiento falso
- **Dónde:** `scripts/4_World/test/LFPG_SorterView_TEST.c:34`
- **Qué está mal:** El bloque sigue mandando a parchear V3 primero y re-clonar con `sprint0_clone.py`. V3 ya no está.
- **Por qué importa:** No rompe carga. Sí miente al siguiente editor.
- **Qué habría que hacer:** Dejar una nota de origen del fork, sin receta de sincronización con una UI borrada.

### MENOR — Indentación mezclada en líneas tocadas
- **Dónde:** `scripts/4_World/LFPG_Sorter.c:111`, `scripts/4_World/test/LFPG_ActionOpenSorterPanel_TEST.c:50`, `scripts/5_Mission/LFPG_MissionInit.c:242`
- **Qué está mal:** Líneas nuevas o reindentadas van con tab dentro de archivos (o bloques) que el resto del fichero escribe con espacios. El reindent de MissionInit es el bloque V4 que quedó al quitar el `else` dual-open; el informe lo declara, no es un reformateo silencioso del archivo entero ni un baile CRLF/LF del diff.
- **Por qué importa:** Estilo, no semántica.
- **Qué habría que hacer:** Igualar el indent al del fichero, sin tocar líneas ajenas.

## Tabla ficha por ficha

| Ficha del BRIEF.md | Implementador | Revisor |
|---|---|---|
| 1. Censo (manda el número real) | ARREGLADA — 6 archivos, 9 clases, 3 layouts; borra 5/7/3 y conserva `LFPG_Sorter.c` | **ARREGLADA.** Censo coincidente con el árbol. |
| 2. Borrar V3 y grep cero de símbolos | ARREGLADA — UI y acción V3 fuera; `LFPG_Sorter` se queda | **ARREGLADA** para UI/acción (`LFPG_SorterView`, `LFPG_SorterController`, `LFPG_SorterTagView`, `LFPG_SorterPreviewRow`, `LFPG_ActionOpenSorterPanel` sin `_TEST`: cero usos activos; solo comentarios en `LFPG_ActionOpenBTCAtm.c:22`, `LFPG_BTCAtmView.c:692`, `LFPG_SorterView_TEST.c:34`). **No** se borró `LFPG_Sorter.c`; ver premisa. |
| 3. Sitios concretos (MissionInit, registro, IsOpen, mutex, RPC) | ARREGLADA | **ARREGLADA.** Init/teclas/ESC/cierre/Cleanup V3 fuera (`LFPG_MissionInit.c:154`, `:181`, `:223`, `:242`, `:436`). Registro V3 fuera; V4 en `LFPG_ActionRegistration.c:68`. `IsOpen` solo V4 en `LFPG_Actions.c:528` y `LFPG_ActionSyncSorter.c:69`. Mutex «production sorter is already open» fuera; `Open` sigue en `:1398`. Resync unificado a `SORTER_TEST_RESYNC` en `LFPG_ActionSyncSorter.c:101`. Cinco handlers V3 cliente fuera; seis `SORTER_TEST_*` y `SORTER_CARGO_REFRESH` (`LFPG_RPCClientHandler.c:46` / `:483`, dispatch TEST `:51`–`:71`) siguen. |
| 4. `config.cpp` — no romper mundos | ARREGLADA (no toca config; cablea V4 a la base) | **ARREGLADA** en no borrar `LFPG_Sorter` / kits (`config.cpp:1039`, `:1059`, `:1080`, `:1086`). El brief pedía `PENDIENTE-DECISION` si se dejaba la entrada: la decisión de no borrar es la correcta. El cableado extra de la acción V4 a la entidad base **no** es esta ficha y cae en el GRAVE de alcance. |
| 5. No renombrar `_TEST` | ARREGLADA | **ARREGLADA.** Nombres `_TEST` intactos. |

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

El brief mezcla la UI V3 con la entidad `LFPG_Sorter`. Pedir borrar `LFPG_Sorter.c` y a la vez «grep cero» de `LFPG_Sorter` a secas, sin tocar `LFPG_Sorter_TEST` ni `config.cpp`, es incompatible con el árbol: `LFPG_Sorter_TEST` hereda de `LFPG_Sorter` en script (`LFPG_Sorter_TEST.c:15`) y en config (`config.cpp:1086`). Borrar la clase base no jubila un panel: deja de compilar World y rompe mundos con sorters colocados.

La lista blanca tampoco cierra el encargo en una frase («la V4 se queda como la única») si la entidad base sobrevive. `SetActions` en `LFPG_Sorter.c:111` puede pasar a `LFPG_ActionOpenSorterPanel_TEST` (ese fichero sí está permitido como edición de `LFPG_Sorter.c`), pero la acción V4 seguía cortando por `GetType() == "LFPG_Sorter_TEST"`. Sin editar `LFPG_ActionOpenSorterPanel_TEST.c`, el sorter persistido tendría una acción muerta. Sin quitar el `SetActions` de `LFPG_Sorter_TEST.c`, la variante `_TEST` añadiría la misma acción dos veces. Eso no es un extra creativo: es el mínimo para que V4 sea el único panel sobre las entidades que el propio brief obliga a conservar. El brief pide la opción conservadora ante ambigüedad; conservar la entidad y no tocar los `_TEST` deja sorters de producción sin UI. Conservar la entidad y cablear V4 es lo correcto de producto; no cabía dentro de la lista escrita.

`SORTER_CARGO_REFRESH` sin sufijo TEST es broadcast vivo (`LFPG_NetworkManagerImpl.c:6339`, disparado desde `:6292`, `:6503`, `:6602`). Clasificarlo como RPC V3 por el nombre habría sido el error. El informe acierta al conservarlo. Los endpoints servidor `SORTER_CONFIG_*` / `SORTER_RESYNC` / etc. siguen en `LFPG_RPCServerHandlerImpl.c` porque ese fichero no era editable; el cliente ya no los atiende. No es un colgante de clase UI.

Ficha 4 del brief solo pregunta por `CfgVehicles` y persistencia. Convertirla en «abrir V4 sobre sorters ya colocados» era el trabajo que faltaba, y no estaba en la lista blanca. El informe lo cuenta como corrección del propietario; en este workspace esa corrección no está en `BRIEF.md`.

## LO QUE NO PUDE COMPROBAR

- Compilación Enforce ni arranque cliente/servidor: no hay motor aquí. Una referencia colgante se habría visto al cargar World/Mission.
- Que la acción V4 aparezca y ejecute sobre un `LFPG_Sorter` persistido (cursor, `Can()`, RPC, panel). El Cast y el handler servidor (`LFPG_RPCServerHandlerImpl.c:126`, `:2509`) están conectados en fuente; no es prueba in-game.
- Round-trip de un mundo real: entidades, filtros, cables, kits, contenido.
- Choque real con las otras lanes en este instante; solo el contrato de lista blanca y el hecho de que `LFPG_ActionOpenSorterPanel_TEST.c` y `LFPG_Sorter_TEST.c` son vecinos típicos.
- Las corridas de `script_validator.py` / `ui_reconcile.py` que el informe declara; no las he repetido.
- CRLF byte a byte de cada fichero tocado; el diff de MissionInit/Sorter no parece una normalización global.
- Que existiera, fuera de este workspace, una lista blanca nueva firmada por el dueño. Aquí solo está `BRIEF.md`.