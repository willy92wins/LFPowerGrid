# BRIEF — Lane 2 · T4 Grafo electrico

**MODO NO INTERACTIVO. EJECUTA DE PRINCIPIO A FIN.** No hay nadie al otro lado para aprobar un
diseno: **este brief ES la aprobacion**. No preguntes y no pares a confirmar. Si algo es ambiguo,
elige la opcion conservadora, hazla, y anota la decision y su motivo en el informe. Terminar sin el
informe escrito en disco cuenta como no haber hecho el encargo.

## Papel
Eres el IMPLEMENTADOR de una lane. Hay cuatro lanes corriendo EN PARALELO sobre copias
independientes del mismo repo (git worktrees). No ves a las otras. Otra familia de modelo revisara
tu trabajo despues; escribe pensando en ese revisor.

## Proyecto
LFPowerGrid: mod de DayZ en Enforce Script. El codigo vive en `scripts/3_Game/`,
`scripts/4_World/` y `scripts/5_Mission/`. Orden de compilacion: 3_Game -> 4_World -> 5_Mission, y
**cada modulo solo ve los anteriores**: no llames desde 4_World a algo de 5_Mission.
Enforce solo compila al cargar el mundo. **Aqui no hay compilador que puedas invocar y no hay
juego que puedas arrancar.** No inventes un comando de build ni declares "compila".

## Convenciones de Enforce — OBLIGATORIAS, el revisor las comprueba linea a linea
Prohibido en toda linea que anadas o modifiques:
- ternarios `? :`
- `++` y `--`  -> escribe `x = x + 1;`
- `+=`, `-=`, `*=`, `/=`  -> escribe `x = x + y;`
- `foreach`  -> `for (int i = 0; i < arr.Count(); i = i + 1)`
- `ref` en parametros, retornos, locales o typedefs. **`ref` SOLO en miembros de clase.**
- `Print(...)`  -> usa `LFPG_Util.Error/Warn/Info/Debug`
Naming: miembros `m_`, estaticos `s_`, metodos PascalCase, locales camelCase, indentacion con tabs.

## TRAMPA de los path:line de este brief — leela antes de abrir nada
Las citas se tomaron contra el commit `d61705e`. Tu worktree esta en `8de29d5`, con cuatro merges
posteriores. **Varias lineas han derivado.** Localiza cada sitio **POR CONTENIDO** (el texto que la
ficha describe), nunca por numero de linea. Si no encuentras el contenido descrito, **no inventes**:
marca la ficha `NO-LOCALIZADA` y di que buscaste.

## Fronteras
- Escribe SOLO dentro de la raiz del workspace.
- **Prohibido `git commit`, `git add`, `git checkout`, `git stash`, `git reset`.** Deja los cambios
  en el arbol de trabajo: el commit lo pone quien revisa.
- No toques ningun fichero fuera de tu LISTA BLANCA.
- **No reformatees.** El repo es CRLF: respetalo. No normalices finales de linea, no reordenes
  nada, no toques indentacion ajena. Un diff inflado por reformateo invalida la revision entera.
- No refactorices nada que no pida una ficha.

## Entregable
Un fichero `INFORME.md` en la raiz del workspace, en castellano, con una seccion `###` por ficha:
- **veredicto**: `ARREGLADA` / `NO-ARREGLADA` / `NO-APLICA` / `NO-LOCALIZADA` / `PENDIENTE-DECISION`
- que cambiaste, con `path:line` **del codigo ya modificado**
- por que esa opcion, y que alternativa descartaste
- como se verificaria (no puedes ejecutar el juego: di que habria que mirar in-game)

Y al final, las dos secciones obligatorias:
- `## LO QUE NO PUDE VERIFICAR` — una linea por cosa.
- `## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO` — casilla libre y **obligatoria**. Si
  crees que una ficha describe mal el bug, que el arreglo correcto es otro, o que el encargo
  presupone algo falso, **dilo aqui**. Un informe que solo dice "hecho, presente y bien formado"
  no es lo que se pide.

## Gate que corre el receptor (tu no)
1. cero `? :`, `++`, `+=`, `foreach`, `Print(`, `ref` mal puesto **en las lineas anadidas**;
2. cada ficha con veredicto, y si es `ARREGLADA`, cita `path:line` que el receptor abrira;
3. el diff no toca ni un fichero fuera de la lista blanca;
4. las dos secciones finales presentes y no vacias.

## LISTA BLANCA (solo estos ficheros)
`scripts/5_Mission/LFPG_ElecGraphImpl.c`, `scripts/4_World/LFPG_VanillaActionOverrides.c`.

## Las fichas — cuatro P1 del subsistema de grafo

### G02 — El validador apaga logicamente baterias que SI generan desde almacenamiento
`5_Mission/LFPG_ElecGraphImpl.c` ~`:3010` y ~`:3052`. **Confirmado.** El solver suma
`m_VirtualGeneration`; el validador **solo mira `incomingPower`**. Verificado en `8de29d5`: `:3010`
cae en `bool shouldBePowered = false;` y `:3052` en la rama `if (node.m_Powered && !shouldBePowered)`
comentada como "Classic zombie: powered but shouldn't be". Los dos criterios tienen que mirar lo
mismo. **Es la ficha mas importante de tu lane: empieza por ella.**

### G01 — La asignacion flexible senala cambios aunque la asignacion final sea identica
`5_Mission/LFPG_ElecGraphImpl.c` ~`:3673` en adelante. **Confirmado** y contrastado por una segunda
revision independiente. Se emite senal de cambio cuando el resultado es el mismo: trabajo y
trafico de red inutiles. Compara el resultado FINAL, no el camino.

### G18 — Se cuenta como proveedor activo un PASSTHROUGH cuya salida solo representa demanda
`5_Mission/LFPG_ElecGraphImpl.c`, **sin linea citada: tienes que localizarlo tu**. Clasificada P1
"Potencial", no confirmada. Busca el tratamiento de nodos PASSTHROUGH en el conteo de proveedores.
Si al abrirlo concluyes que **no** es un bug, dilo: `NO-APLICA` con el razonamiento vale tanto como
un arreglo, y es mejor que un cambio inventado.

### G04 — Los cambios de una fuente vanilla no notifican al solver
`4_World/LFPG_VanillaActionOverrides.c` ~`:100` y ~`:172` (fichero de 177 lineas). P1 "Potencial".
`RefreshSourceState` tiene **un unico llamador**, y ninguno parte de un generador vanilla: si el
jugador enciende un generador del juego base, el grafo no se entera.
⚠ **Aviso de metodo, del propio council:** para fichas del tipo "falta un llamador" la cura vive en
OTRO fichero, y el fichero citado puede quedar intacto con la ficha ya muerta. Comprueba primero si
ya existe un llamador valido en el arbol antes de anadir uno.
