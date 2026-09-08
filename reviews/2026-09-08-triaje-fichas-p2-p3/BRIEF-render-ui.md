# BRIEF - Triaje de fichas P2/P3: Render/UI (27 fichas)

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

## TU LOTE: las 27 fichas P2/P3 de Render/UI

Columnas: id | subsistema | prioridad | estado declarado por la auditoria | descripcion.
La descripcion es lo unico que tienes: **no traen `path:line`. Localizarlas es el encargo.**

| R01 | Render/UI | P2 | Confirmado | La rama de oclusión específica de cables con esquinas nunca recibe los waypoints |
| R03 | Render/UI | P2 | Confirmado | Eliminar un owner deja conexiones y dispositivos conocidos en cachés derivadas |
| R05 | Render/UI | P2 | Confirmado | Selection sort cuadrático cada dos segundos, incluyendo cables invisibles |
| R06 | Render/UI | P2 | Confirmado | Cada cambio local reconstruye el índice global de conexiones |
| R07 | Render/UI | P2 | Confirmado | El delta ahorra red pero reconstruye toda la geometría del owner |
| R08 | Render/UI | P2 | Confirmado | El early-out del owner oculta cables largos aunque el jugador esté junto al destino |
| R09 | Render/UI | P2 | Confirmado | El canvas sigue trabajando cada frame aunque no haya cables que puedan dibujarse |
| R10 | Render/UI | P2 | Potencial | Las claves de proyección omiten FOV y orientación completa de cámara |
| R11 | Render/UI | P2 | Confirmado | La detección de movimiento para oclusión depende del FPS y pierde desplazamientos acumulados |
| R12 | Render/UI | P2 | Confirmado | El inspector conserva indefinidamente datos de conexiones que cambian sin variar la topología del owner |
| R13 | Render/UI | P2 | Confirmado | Dos cálculos distintos del offset del inspector solapan filas de batería/sorter |
| R14 | Render/UI | P2 | Confirmado | La destrucción de cualquier cámara puede cerrar la sesión CCTV de otra |
| R16 | Render/UI | P3 | Confirmado | TankHUD consulta objetos del escenario cuatro veces por segundo aunque nunca haya una bomba |
| R17 | Render/UI | P2 | Confirmado | El foco recalcula toda la predicción visual mientras permanece inmóvil |
| R18 | Render/UI | P2 | Confirmado | Registrar N láseres recorre repetidamente todos los anteriores |
| R19 | Render/UI | P2 | Confirmado | El filtro de frustum de láser se actualiza a 4Hz y produce pop-in al girar |
| R20 | Render/UI | P2 | Potencial | Los snapshots pueden retroceder las generaciones locales |
| R21 | Render/UI | P2 | Confirmado | La cola de sync no deduplica solicitudes que ya están en vuelo |
| R22 | Render/UI | P3 | Confirmado | Código y estado muertos sobreviven a refactors y se siguen manteniendo |
| R23 | Render/UI | P2 | Confirmado | La geometría usa un objeto gestionado por cada segmento para datos que caben en una polilínea |
| R24 | Render/UI | P3 | Confirmado | La preparación de geometría y varios helpers matemáticos están duplicados |
| R25 | Render/UI | P2 | Confirmado | El renderer crea y emite muchos logs operativos aun en uso normal |
| R26 | Render/UI | P2 | Confirmado | Los cables recién construidos se dibujan inicialmente como cercanos y visibles |
| R27 | Render/UI | P3 | Potencial | BeginFrame rechaza resolución 0×0 pero los productores pueden dibujar con dimensiones antiguas |
| R28 | Render/UI | P3 | Confirmado | Los helpers de inspector leen/formatean el mismo estado dos veces y su caché es más genérica de lo necesario |
| R29 | Render/UI | P3 | Confirmado | Los widgets CCTV se recolocan sólo al crearse, mientras el canvas sí reacciona a resolución |
| R30 | Render/UI | P3 | Potencial | MissionGameplay modifica el brillo global de widgets sin guardar/restaurar estado |
