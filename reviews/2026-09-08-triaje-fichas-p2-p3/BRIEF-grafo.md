# BRIEF - Triaje de fichas P2/P3: Grafo (18 fichas)

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

## TU LOTE: las 18 fichas P2/P3 de Grafo

Columnas: id | subsistema | prioridad | estado declarado por la auditoria | descripcion.
La descripcion es lo unico que tienes: **no traen `path:line`. Localizarlas es el encargo.**

| G03 | Grafo | P2 | Confirmado | Reconstruir el seguimiento excluye fuentes vanilla con cables de salida |
| G05 | Grafo | P2 | Confirmado | El FullSync de un jugador retiene cambios de cables para todos |
| G06 | Grafo | P2 | Confirmado | Un CutAll local reconstruye todo el grafo y pierde estado transitorio de baterías ajenas |
| G07 | Grafo | P2 | Confirmado | La poda diferida deja cuotas por jugador con cables que ya no existen |
| G08 | Grafo | P2 | Confirmado | La carga de BatteryCharger depende del tamaño y actividad de todo el grafo |
| G09 | Grafo | P2 | Confirmado | El límite global de nodos compara el estado actual en vez del resultado |
| G10 | Grafo | P2 | Potencial | rebuild desde persistencia no aplica límites de tamaño ni detección de ciclos |
| G11 | Grafo | P2 | Confirmado | La telemetría repite el último coste activo durante todos los ticks inactivos |
| G12 | Grafo | P3 | Confirmado | El broadcast completo calcula destinatarios y posiciones dos veces |
| G13 | Grafo | P3 | Confirmado | Delta y vanilla serializan antes de saber si hay receptores |
| G14 | Grafo | P3 | Confirmado | La compactación de la cola hace dos copias y una asignación evitable |
| G15 | Grafo | P3 | Confirmado | Metadatos 1:1 del nodo viven en mapas paralelos innecesarios |
| G16 | Grafo | P3 | Confirmado | Duplicaciones concretas que pueden convertirse en helpers pequeños |
| G19 | Grafo | P3 | Confirmado | El test booleano de descendientes sigue visitando aristas después de encontrar uno |
| G20 | Grafo | P3 | Confirmado | Rebuild crea nodos desconectados para borrarlos y repite hidratación inmediatamente |
| G21 | Grafo | P3 | Potencial | la API legacy de puerto energizado puede devolver propietarios obsoletos |
| G22 | Grafo | P2 | Potencial | el fallback de fábrica de grafo no se recupera y puede inundar logs |
| G23 | Grafo | P2 | Confirmado | La cola de FullSync no es FIFO por usar Remove(0) |
