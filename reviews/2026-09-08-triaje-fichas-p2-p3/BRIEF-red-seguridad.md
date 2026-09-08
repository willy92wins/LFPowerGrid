# BRIEF - Triaje de fichas P2/P3: Red/seguridad (16 fichas)

**MODO NO INTERACTIVO. EJECUTA DE PRINCIPIO A FIN.** No hay nadie al otro lado para aprobar nada:
**este brief ES la aprobacion**. No preguntes y no pares a confirmar. Si algo es ambiguo, elige lo
conservador, hazlo, y anota la decision y su motivo. Terminar sin el informe escrito en disco
cuenta como no haber hecho el encargo.

## Papel
Eres una lane de **TRIAJE**. Hay ocho lanes corriendo en paralelo sobre copias independientes del
repo; tres escriben codigo y cinco triamos fichas. **Tu NO arreglas nada.** No tocas ni una linea
de codigo. Tu unico producto es `TRIAJE.md` en la raiz del workspace.

## Que se te pide, y por que existe este encargo
Dos auditorias dejaron **126 fichas P2/P3** sobre este mod. De ellas, **98 estan sin comprobar**:
nadie ha mirado si siguen vivas en el codigo de hoy. No estan «pendientes de arreglar» — estan
**desconocidas**. Tu trabajo es convertir tu lote de desconocidas en conocidas.

**Por cada ficha de tu lote, un veredicto:**
- `VIVA` — el defecto sigue en el codigo. **Obligatorio: `path:line` que lo demuestre.**
- `MUERTA` — ya no existe. Obligatorio: el `path:line` de hoy que lo arregla, o la explicacion de
  por que el codigo afectado ya no existe.
- `NO-LOCALIZADA` — no has sabido encontrar a que se referia. Di **que buscaste**. Es un veredicto
  legitimo y honesto; inventar una ubicacion no lo es.
- `DUDOSA` — la descripcion admite dos lecturas y llevan a sitios distintos. Explica las dos.

## El error de metodo que tienes que evitar, senalado por el council
«Fichero intacto, luego ficha viva» **es un instrumento roto**, y asi se construyo la lista que
recibes. Solo vale si la cura tendria que tocar ese fichero. **Para las fichas del tipo «falta un
llamador», «no se notifica a X» o «no se invalida Y», la cura vive en OTRO fichero**, y el fichero
citado quedaria intacto con la ficha ya muerta. **Lee el codigo. No deduzcas de fechas ni de
diffs.**

## Lo que ha cambiado hoy, y que tienes que tener en cuenta
`main` ha movido **1.087 lineas en 19 ficheros** en las ultimas horas, de cinco lanes ya revisadas:
integridad de dispositivos (D01-D05), grafo (G01, G02, G04, G18), render y coste (R02, R04, R15,
S04, S08), autoridad de servidor (SEC01, SEC02, SEC03, SEC20) y el cierre de la V4 del sorter.
**Varias de tus fichas P2/P3 pueden haber muerto de rebote esta misma madrugada.** Cuando marques
una `MUERTA`, di si murio hoy o si ya estaba muerta.

## Fronteras
- **No modifiques ningun fichero de codigo.** Ni uno. Si te pica arreglar algo, va al informe.
- Escribe SOLO `TRIAJE.md` en la raiz del workspace.
- **Prohibido `git commit`, `git add`, `git checkout`, `git stash`, `git reset`.**
- Puedes leer todo el arbol, incluido `reviews/` con las auditorias y planes anteriores.

## Contexto del codigo
LFPowerGrid, mod de DayZ en Enforce Script. `scripts/3_Game/` -> `4_World/` -> `5_Mission/`, y cada
modulo **solo ve los anteriores**. Enforce solo compila al cargar el mundo: aqui no hay compilador
ni juego. Todo lo que afirmes sale de leer codigo.

## Entregable: `TRIAJE.md`
1. **Tabla de cabecera**, una fila por ficha: id, veredicto, fichero principal, y una linea de
   por que. Esta tabla es lo que se lee primero: que este completa.
2. **Una seccion `###` por ficha `VIVA` o `DUDOSA`**, con:
   - `path:line` verificable (quien reciba esto va a abrir cada una);
   - que esta mal, en una o dos frases;
   - **coste de arreglarlo**: `TRIVIAL` (una linea), `ACOTADO` (un fichero), `AMPLIO` (varios
     ficheros o cambio de contrato);
   - **si comparte fichero o funcion con otra ficha de tu lote** — eso decide como se agrupan
     luego en tramos, y es de lo mas util que puedes aportar.
3. `## LAS QUE RECOMIENDO ATACAR PRIMERO` — como mucho cinco, ordenadas, con el motivo. Manda el
   dano real al jugador, no la facilidad.
4. `## LO QUE NO PUDE VERIFICAR` — una linea por cosa.
5. `## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO` — **obligatoria**. Si crees que el lote
   esta mal cortado, que una ficha describe mal un defecto, o que la clasificacion P2/P3 esta
   equivocada en algun caso (algo P2 que en realidad es P1, o al reves), **es aqui donde vale**.

## TU LOTE: las 16 fichas P2/P3 de Red/seguridad

Columnas: id | subsistema | prioridad | estado declarado por la auditoria | descripcion.
La descripcion es lo unico que tienes: **no traen `path:line`. Localizarlas es el encargo.**

| SEC03 | Red/seguridad | P2 | Confirmado | El reemplazo elimina conexiones aunque la nueva no pueda almacenarse |
| SEC04 | Red/seguridad | P2 | Confirmado | Cortar un IN siempre escanea todos los dispositivos y cables, incluso sin cambios |
| SEC05 | Red/seguridad | P2 | Confirmado | El rechazo de spam genera trabajo de red y logs proporcional al spam |
| SEC06 | Red/seguridad | P2 | Confirmado | La resincronización de lote puede forzar SyncVars repetidamente sin necesidad real |
| SEC07 | Red/seguridad | P3 | Confirmado; aspectos potenciales | Strings de cliente sin normalizar llegan al RPT; ciertos límites se aplican tras deserializar |
| SEC08 | Red/seguridad | P2 | Confirmado | Un operador puede abandonar el foco y mantenerlo reservado indefinidamente |
| SEC10 | Red/seguridad | P2 | Confirmado | El fallback de archivo acepta un target corrupto y no intenta el backup válido |
| SEC11 | Red/seguridad | P2 | Potencial | Settings.Load deserializa directamente sobre el singleton y no restaura defaults al fallar |
| SEC12 | Red/seguridad | P2 | Confirmado | El historial BTC no conserva las últimas 64 respuestas: Remove(0) no es FIFO en Enforce |
| SEC13 | Red/seguridad | P3 | Confirmado | Una secuencia saltada al máximo inutiliza las futuras transacciones de ese UID |
| SEC14 | Red/seguridad | P3 | Confirmado | Cachés por UID y avisos conservan datos de toda la vida del proceso |
| SEC15 | Red/seguridad | P2 | Confirmado | RPCGuard clasifica ocho políticas pero solo aplica Admit/Authorize al inspector |
| SEC16 | Red/seguridad | P3 | Confirmado | Los tres guardados duplican el mismo protocolo de archivos pese a que la parte duplicada no necesita genéricos |
| SEC17 | Red/seguridad | P3 | Confirmado | FINISH_WIRING calcula y valida dos veces la geometría y parte de sus condiciones |
| SEC18 | Red/seguridad | P3 | Confirmado | Migrators es infraestructura huérfana y describe una cadena que ya no se ejecuta |
| SEC19 | Red/seguridad | P3 | Confirmado; aspectos potenciales | TryReadRawJsonVersion lee y copia todo el archivo para localizar un encabezado |
