# BRIEF - Lane 3 . T4 Render, camara y coste del scheduler

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
`scripts/4_World/LFPG_CableRenderer.c`, `scripts/4_World/LFPG_CameraViewport.c`,
`scripts/3_Game/LFPG_Defines.c`, `scripts/5_Mission/LFPG_NetworkManagerImpl.c`,
`scripts/5_Mission/LFPG_SorterLogic.c`.

⚠ **`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c` NO esta en tu lista** y lo esta editando otra
lane ahora mismo. Lo menciono abajo solo como contexto de S08. **No lo abras para escribir.**
⚠ `LFPG_NetworkManagerImpl.c` lo tocan tambien otras lanes. **Tu zona es ~6150-6600.** Fuera de
ahi, no escribas en ese fichero.

## Las fichas — cinco P1

### S08 — El repack manual sincrono no esta acotado por el presupuesto del scheduler
`5_Mission/LFPG_NetworkManagerImpl.c` ~`:6566` (verificado en `8de29d5`: la linea es
`LFPG_SorterLogic.RepackCargoInPlace(container);`) y `5_Mission/LFPG_SorterLogic.c` ~`:1105-1327`.
**Confirmado.** El `maxEval=200` del scheduler **no envuelve** `RepackCargoInPlace`, que es O(N^2)
con hasta N pasadas. Es un pico de servidor que ningun presupuesto frena.
**Donde va la cura:** en `LFPG_SorterLogic.c`, no en el llamador. Hay un segundo llamador en el
sorter V4 (`LFPG_RPCServerHandlerImpl.c:147-150`, fichero de OTRA lane) que hereda el mismo pico:
si acotas dentro de `RepackCargoInPlace`, los dos quedan cubiertos de una vez, que es exactamente
lo que el plan pide. **Empieza por esta ficha.**

### S04 — El presupuesto global rompe el round-robin y puede privar de turno a otros sorters
`5_Mission/LFPG_NetworkManagerImpl.c` ~`:6193-6197` y ~`:6272-6276`. **Confirmado.** Verificado en
`8de29d5`: `:6193` es `if (budgetExhausted) { m_SorterCursor = sorterIndex; break; }`. Al agotarse
el presupuesto el cursor global **se queda en el mismo sorter**: hay reanudacion local pero no
rotacion, asi que un sorter caro puede matar de hambre a los demas indefinidamente.

### R02 — La limpieza de owners desaparecidos decide con una distancia congelada
`4_World/LFPG_CableRenderer.c` ~`:2444-2510` y ~`:2556-2567`. **Confirmado.** Verificado: `:2444`
cae en la rama comentada "Owner entity is null (destroyed or streamed out)". Esa rama hace
`continue` **antes** de recalcular `cachedMinDist`, asi que decide con una distancia vieja.

### R04 — El presupuesto global reserva segmentos invisibles y no prioriza cables proximos
`4_World/LFPG_CableRenderer.c` ~`:2182` y ~`:3992`; `3_Game/LFPG_Defines.c` ~`:223`.
**Confirmado.** Presupuesto global de 512 sin prioridad espacial, y **`CullTick` nunca decrementa**.
⚠ `LFPG_Defines.c` esta en `3_Game`: cambiar una constante ahi la ve todo el mod. Si tocas ese
fichero, limitate a la constante que cita la ficha y dilo en el informe.

### R15 — El timeout de salida de CCTV desactiva la camara sin confirmar la restauracion
`4_World/LFPG_CameraViewport.c` ~`:846-855` y ~`:757-779`. P1 "Potencial". El timeout de 5 s llama
al mismo cleanup **sin confirmar que la restauracion ocurrio** y **sin token de sesion**, asi que un
cleanup tardio puede pisar una sesion de camara nueva. Un token de sesion monotono es el patron
esperado, pero decide tu.
**Esta ficha necesita ejecucion in-game para cerrarse del todo** (restauracion de camara). No la
tienes. Deja el arreglo hecho y escribe en `LO QUE NO PUDE VERIFICAR` que falta la prueba in-game.
