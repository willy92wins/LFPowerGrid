# BRIEF - Lane Rescate de la rama maint/audit-kimi-followup

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

## EL ENCARGO EN UNA FRASE
Rescatar la rama `maint/audit-kimi-followup` — **3 items, ya revisados, nunca compilados** — y
traer a `main` de hoy lo que siga siendo valido.

## ESTO NO ES UN MERGE. ES UN JUICIO ITEM POR ITEM
La rama se quedo 15 commits atras y hoy `main` lleva 1.087 lineas nuevas. **Nada de `git merge`,
`cherry-pick`, `rebase` ni `checkout` de esa rama.** Para cada item decides una de cuatro:
- `PORTAR` — valido y sin colision: lo aplicas a mano sobre el codigo de hoy.
- `YA-RESUELTO` — `main` de hoy ya lo cubre. Cita el `path:line` de hoy.
- `OBSOLETO` — el codigo al que apuntaba cambio o desaparecio.
- `CONFLICTO` — choca con algo de hoy; lo describes y no lo aplicas.

## Como leer la rama
Tu worktree comparte el object store:

    git log --oneline main..maint/audit-kimi-followup
    git diff main...maint/audit-kimi-followup -- scripts/
    git show maint/audit-kimi-followup:<ruta>

## LISTA BLANCA (puedes escribir SOLO en estos tres)
`scripts/3_Game/LFPG_Migrators.c`, `scripts/3_Game/LFPG_Telemetry.c`,
`scripts/4_World/LFPG_CableRenderer.c`.

**SOLO LECTURA:** todo lo demas. Hay otras siete lanes en paralelo.

## Lo que la rama hace, y donde esta el riesgo
Su diff frente al `main` viejo: `LFPG_Migrators.c` **-22 lineas netas**, `LFPG_Telemetry.c`
reescrito (+88/-...), `LFPG_CableRenderer.c` **-56 lineas, solo borrado**. Es mantenimiento de
sobreingenieria: quitar aparato sin consumidor.

⚠ **`LFPG_CableRenderer.c` es el fichero de mas riesgo de esta lane: hoy ha cambiado 208 lineas**
(fichas R02 y R04 — la rama owner-null recalcula distancia, `CullTick` libera segmentos y hay
admision espacial). El borrado de 56 lineas que propone la rama **puede pisar codigo nuevo que si
tiene consumidor ahora**. Lee el codigo de hoy antes de borrar una sola linea ahi, y ante la duda
marca `CONFLICTO` en vez de borrar. Borrar de menos es recuperable; borrar de mas, no.

⚠ `LFPG_Migrators.c` y `LFPG_Telemetry.c` estan en **`3_Game`**, el modulo base: lo ve todo el mod.
Un simbolo que borres ahi puede tener llamadores en `4_World` o `5_Mission`. **Grep en todo el
arbol antes de borrar cualquier cosa**, y di en el informe que buscaste.

## Entregable — ademas del `INFORME.md` normal
Una tabla al principio con **una fila por item**: identificador, fichero, decision de las cuatro,
y evidencia `path:line`. Y para cada borrado que apliques, la prueba de que **no queda ningun
llamador** en el arbol.
