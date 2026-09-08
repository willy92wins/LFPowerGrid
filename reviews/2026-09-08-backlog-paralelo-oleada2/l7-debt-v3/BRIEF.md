# BRIEF - Lane Rescate de la rama debt/v3-data-integrity

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
Rescatar la rama `debt/v3-data-integrity`, que lleva **12 de 19 items** hechos y **se quedo 15
commits atras**, y traer a `main` de hoy lo que siga siendo valido.

## ESTO NO ES UN MERGE. ES UN JUICIO ITEM POR ITEM
La rama se escribio contra un `main` que ya no existe: hoy `main` lleva 1.087 lineas nuevas en 19
ficheros, de cinco lanes revisadas. **Un `git merge` o un `cherry-pick` a ciegas es exactamente lo
que NO se pide** y probablemente pisaria arreglos mejores que los suyos.

**Tu trabajo es, para cada item de la rama, decidir una de cuatro cosas:**
- `PORTAR` — sigue siendo valido y no colisiona: lo aplicas a mano sobre el codigo de hoy.
- `YA-RESUELTO` — `main` de hoy ya lo arregla, por otro camino. Cita el `path:line` de hoy que lo
  demuestra.
- `OBSOLETO` — el codigo al que apuntaba ya no existe o cambio de forma.
- `CONFLICTO` — choca con algo de hoy y la decision no es tuya. Descríbelo y no lo apliques.

## Como leer la rama
Tu worktree comparte el object store, asi que puedes leerla entera sin cambiar de rama:

    git log --oneline main..debt/v3-data-integrity
    git diff main...debt/v3-data-integrity -- scripts/
    git show debt/v3-data-integrity:<ruta>

**No hagas `git merge`, `git cherry-pick`, `git rebase` ni `git checkout` de esa rama.**

## LISTA BLANCA (puedes escribir SOLO en estos)
Los ficheros que toca la rama, **menos los del sorter V3**:
`scripts/4_World/LFPG_AtmStock.c`, `LFPG_DeviceRegistry.c`, `LFPG_NetworkManager.c`,
`LFPG_RPCGuard.c`, `LFPG_SearchlightController.c`, `scripts/5_Mission/LFPG_BTCSessionRegistry.c`,
`LFPG_BalanceProvider_NativeImpl.c`, `LFPG_ControlSessionRegistry.c`,
`LFPG_NetworkManagerImpl.c`, `LFPG_RPCServerHandlerImpl.c`, y los demas que veas en su diff y no
esten excluidos abajo.

## EXCLUIDO, y el motivo importa
- **`scripts/4_World/LFPG_SorterController.c` y cualquier fichero del sorter V3.** Otra lane esta
  **borrando la V3 entera ahora mismo**. Portar un arreglo a un fichero que va a desaparecer es
  trabajo tirado. Si un item de la rama vive ahi, marcalo `OBSOLETO` con el motivo «muere con la
  V3» y comprueba si el mismo defecto existe en la V4 (`scripts/4_World/test/`): si existe,
  **dilo en el informe** — es un hallazgo valioso — pero **no lo arregles aqui**.
- `scripts/3_Game/LFPG_Migrators.c`, `LFPG_Telemetry.c`, `scripts/4_World/LFPG_CableRenderer.c`:
  son de otra lane.

## Cuidado especial
`LFPG_NetworkManagerImpl.c` y `LFPG_RPCServerHandlerImpl.c` **cambiaron mucho hoy** (T2 autoridad
de servidor: validacion de puertos en servidor, politica de corte simetrica, reserva de arista
antes de dar de baja). Antes de portar nada ahi, **lee el codigo de hoy**: es muy probable que
varios items de la rama entren en `YA-RESUELTO`, y unos cuantos en `CONFLICTO`.

## Entregable — ademas del `INFORME.md` normal
Una tabla al principio del informe con **una fila por item de la rama**: identificador, fichero,
decision de las cuatro, y la evidencia `path:line`. Sin esa tabla el encargo no esta hecho.
