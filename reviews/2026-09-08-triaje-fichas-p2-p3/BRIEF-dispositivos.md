# BRIEF - Triaje de fichas P2/P3: Dispositivos (18 fichas)

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

## TU LOTE: las 18 fichas P2/P3 de Dispositivos

Columnas: id | subsistema | prioridad | estado declarado por la auditoria | descripcion.
La descripcion es lo unico que tienes: **no traen `path:line`. Localizarlas es el encargo.**

| D06 | Dispositivos | P2 | Potencial | Desmontaje y despliegue convierten salud del dispositivo en un objeto nuevo |
| D07 | Dispositivos | P2 | Confirmado | Las mejoras reconstruyen los sobrantes y el filtro perdiendo estado del objeto |
| D08 | Dispositivos | P2 | Confirmado | La lista remota de emparejamientos no se sincroniza al cambiar de propietario |
| D09 | Dispositivos | P2 | Potencial | Callbacks diferidos de mando y MemoryCell no se cancelan ni se coalescen |
| D10 | Dispositivos | P2 | Confirmado | El fast path de DeviceAPI no se utiliza para varios accesos frecuentes a puertos |
| D11 | Dispositivos | P2 | Confirmado | Se reescriben LEDs y animaciones aunque no cambie su estado visual |
| D12 | Dispositivos | P2 | Confirmado | Intercom RF copia y recorre todo el registro para una búsqueda local |
| D13 | Dispositivos | P2 | Confirmado | El backoff de puertas aún consulta todos los jugadores por cada controlador sin pareja |
| D14 | Dispositivos | P2 | Confirmado | La estimación de combustible ignora el modo whitelist y repite lecturas de config |
| D15 | Dispositivos | P2 | Potencial | La carga de persistencia acepta versiones y valores sin comprobar el contrato concreto |
| D17 | Dispositivos | P2 | Potencial | La identidad de grupo del sensor se basa en un nombre mutable |
| D18 | Dispositivos | P3 | Confirmado | Las familias de dispositivos duplican comportamiento compartible sin necesitar una jerarquía universal |
| D19 | Dispositivos | P3 | Confirmado | El generador de producción legacy vuelve a serializar wires en cada petición |
| D20 | Dispositivos | P2 | Confirmado | MemoryCell emite log de cada propagación aunque no cambie el estado |
| D21 | Dispositivos | P2 | Potencial | ResolveVanillaDevice usa un radio de aceptación diferente del declarado |
| D22 | Dispositivos | P3 | Potencial | El guard de holograma de la base no detiene inicialización añadida después de super |
| D23 | Dispositivos | P3 | Confirmado | La ordenación del mando realiza selección cuadrática en cada guardado |
| D24 | Dispositivos | P2 | Confirmado | El adaptador publica SyncVars de energía pero su API cliente no usa ese estado |
