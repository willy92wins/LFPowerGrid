# BRIEF - Lane 4 . T2 Autoridad de servidor

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
`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`, `scripts/5_Mission/LFPG_NetworkManagerImpl.c`.

⚠ `LFPG_NetworkManagerImpl.c` lo tocan tambien otras lanes, en las zonas ~6150-6600 y ~7600-7750.
**Tu zona es la de los `.Send(` de sincronizacion, alrededor de `:2150`.** Fuera de ahi, no
escribas en ese fichero.

## CONTEXTO QUE TE AHORRA TRABAJO — hay un intento previo archivado
Existe una rama `archive/t2-autoridad-servidor` con trabajo anterior sobre estas mismas fichas,
archivado **sin cerrar** (6 de 7 condiciones). Tu worktree comparte el object store, asi que puedes
leerla:

    git diff main...archive/t2-autoridad-servidor -- scripts/
    git log --oneline main..archive/t2-autoridad-servidor

**Leela antes de escribir nada.** No la fusiones ni hagas cherry-pick: usala como referencia, y di
en el informe que reutilizaste, que descartaste y por que.

## POR QUE ESTO IMPORTA MAS DE LO QUE LA AUDITORIA CREIA
Las auditorias no sabian que **el servidor tiene jugadores reales** (privado). Con jugadores, estas
fichas dejan de ser deuda tecnica y pasan a ser **superficie de ataque**: un cliente modificado las
explota. Trata cada una como fail-closed: ante la duda, el servidor **rechaza**.

## Las fichas

### SEC20 — Sin validacion del nombre de puerto en servidor antes de mutar · **la mas grave**
`5_Mission/LFPG_RPCServerHandlerImpl.c` ~`:283`, ~`:465-496`, ~`:750-790`. **Confirmado, P1.**
No hay validacion del nombre de puerto en el servidor antes de mutar: **solo longitud <=32**.
Un `GetDeviceId()` vacio en un dispositivo vanilla **salta `HasPort` y `CanConnectTo`**, asi que un
cliente modificado inventa nombres de puerto, evita el limite fisico y provoca divergencia entre el
store y el grafo. Y ademas **difunde antes de insertar en el grafo, y no revierte** si la insercion
falla. Son dos defectos: valida en servidor, y ordena mutacion-antes-de-difusion con rollback.

### SEC02 — Reemplazar un cable evita la politica AllowCutOthersWires
`5_Mission/LFPG_RPCServerHandlerImpl.c` ~`:602`, ~`:680`. **Confirmado, P1.**
`RemoveWiresTargeting` se llama con `allowOthers=true` por defecto, mientras el camino de CORTE si
aplica la politica: **asimetria confirmada**. Un jugador corta cables ajenos reemplazando en vez de
cortando. Los dos caminos deben pasar por la misma comprobacion.

### SEC03 — El reemplazo elimina conexiones aunque la nueva no pueda almacenarse
P2, **confirmado**. Va con SEC02: sin ella el reemplazo queda a medias. El orden correcto es
comprobar que la conexion nueva cabe y se puede almacenar **antes** de eliminar la vieja.

### SEC01 — Las sincronizaciones unicast y con filtrado de proximidad usan broadcast
`5_Mission/LFPG_NetworkManagerImpl.c` ~`:2150` **y 7 sitios mas** (localizalos por contenido: son
llamadas `.Send(`). **Confirmado, P1.** Los nueve `.Send(` migraron identicos, y el FullSync
**valida identidad y luego envia `null`** como destinatario, o sea que difunde a todo el mundo lo
que deberia ir a uno. Fuga de informacion del mapa a cualquier cliente.
**Esta ficha necesita ejecucion in-game para cerrarse** (recuento de entregas por cliente). No la
tienes: deja el arreglo y anotalo en `LO QUE NO PUDE VERIFICAR`.
