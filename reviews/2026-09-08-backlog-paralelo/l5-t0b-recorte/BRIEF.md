# BRIEF - Lane 5 . T0b Cerrar la V4 del sorter (sin borrar la V3)

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

## LISTA BLANCA (puedes escribir SOLO en estos)
`scripts/4_World/test/LFPG_SorterView_TEST.c`, `scripts/4_World/test/LFPG_SorterController_TEST.c`,
`scripts/4_World/test/LFPG_SorterPreviewRow_TEST.c`, `scripts/4_World/test/LFPG_SorterTagView_TEST.c`,
`scripts/4_World/test/LFPG_ActionOpenSorterPanel_TEST.c`, `scripts/4_World/LFPG_Actions.c`,
`scripts/4_World/LFPG_ActionSyncSorter.c`, `scripts/4_World/LFPG_RPCClientHandler.c`.

**SOLO LECTURA — son de otras lanes o son la referencia a copiar. No escribas en ellos:**
`scripts/3_Game/LFPG_Defines.c` (otra lane lo esta editando ahora),
`scripts/3_Game/LFPG_UIScaler.c`, `scripts/4_World/LFPG_SorterView.c`,
`scripts/4_World/LFPG_SorterTagView.c`, `scripts/4_World/LFPG_SorterPreviewRow.c`,
`scripts/4_World/LFPG_ActionOpenSorterPanel.c`, `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`.

## EL ENCARGO EN UNA FRASE
Dejar la V4 del sorter (`_TEST`) apta para produccion **sin borrar todavia la V3**.

## LO QUE ESTA FUERA DE ALCANCE — no lo hagas aunque lo veas a tiro
- **NO borres ninguna clase ni layout de la V3.** Decision del dueno de hoy: el borrado espera a
  que haya una prueba in-game de la V4, que hoy no existe.
- **NO retires** los enganches de `LFPG_MissionInit.c`, los handlers de `LFPG_RPCClientHandler.c`
  que ya existen, ni el mutex anti dual-open: sostienen la convivencia V3+V4 mientras la V3 viva.
- **NO renombres** `_TEST` a canonico (es otro tramo, y `LFPG_Sorter_TEST` esta ligado a
  persistencia en `config.cpp:1080` y `:1086`).
- **NO renombres `LFPG_MCP_SorterCmd.layout`.** Guarda explicita de una auditoria previa.

## Las seis tareas, por orden de importancia

### 1. D-02 · Portar `LFPG_UIScaler` a la V4 — **es lo que hoy la hace no apta**
`LFPG_UIScaler` **existe en la V3 y falta por completo en la V4**. Sin el, el panel V4 se ve
desproporcionado en cualquier resolucion o DPI que no sea 1080p al 100 %. Fue deliberado para
probar la mecanica, y es el unico bloqueo duro para que la V4 sea la de produccion.
La API esta documentada en la cabecera de `scripts/3_Game/LFPG_UIScaler.c:11-15`:

    Init  -> LFPG_UIScaler.Capture(panelRoot);
    Open  -> float s = LFPG_UIScaler.ComputeScale(); LFPG_UIScaler.Apply(s);
    Quit  -> LFPG_UIScaler.Reset();

Copia el patron **tal y como lo usa la V3** en `LFPG_SorterView.c`, y no te olvides del escalado de
tags y de preview rows (mira `LFPG_SorterTagView.c` y `LFPG_SorterPreviewRow.c`, que son la
referencia de los equivalentes `_TEST`). Es un port, no un rediseno.

### 2. D-01 · Paridad de guardas al abrir el panel
La accion V3 (`LFPG_ActionOpenSorterPanel.c`) exige `LFPG_IsPowered()` **y** `LFPG_IsLinked()` y
compara `GetType()` **exacto**. La V4 (`test/LFPG_ActionOpenSorterPanel_TEST.c`) **no exige ninguno
de los dos** y usa `IsKindOf`. Con la V4 pasando a produccion, **se alinea con produccion**: exige
lo mismo que la V3. Esto no es una decision abierta, ya esta tomada.

### 3. Gatear la sonda `S1_PROBE`
`test/LFPG_SorterView_TEST.c:243` tiene `static const bool S1_PROBE = true;` **a pelo**, y dispara
en `:1562`. Escupe ~16 lineas al RPT en cada apertura del panel. Ya existe el gate correcto:
`LFPG_PERFDIAG_ENABLED`, que vale `false` en `scripts/3_Game/LFPG_Defines.c:405`. Cuelga la sonda de
el. **`LFPG_Defines.c` es de otra lane: no lo edites, solo leelo.**

### 4. Gatear el hook MCP — se queda, pero no puede entrar en el build publicado
Decision del dueno ya tomada: **el hook se conserva, gateado**. Vive en
`test/LFPG_SorterView_TEST.c` alrededor de `:1894-2110` (`hookName` en `:1922`, `layoutPath` en
`:1934`, `WriteMcpDump` llamado en `:1990` y definido en `:2095`) mas el polling de `Update()`.
Ponlo detras de un gate de diagnostico de forma que **no se compile en un build publicado**, con el
mismo criterio con el que el mod ya separa lo de diagnostico. Elige el mecanismo y justificalo.

### 5. Cablear el lado cliente de los RPC 65 y 70
`SORTER_TEST_RESYNC = 65` y `SORTER_TEST_CARGO_REFRESH = 70` estan declarados en
`scripts/3_Game/LFPG_Defines.c:331` y `:336`. **El servidor ya los atiende**
(`LFPG_RPCServerHandlerImpl.c:157`, solo lectura para ti) y el cliente ya RECIBE
`SORTER_TEST_RESYNC_ACK` y `CARGO_REFRESH` (`LFPG_RPCClientHandler.c:79` y `:91`).
**Lo que falta es el EMISOR**: verificado en `8de29d5`, no existe un solo sitio en `4_World` que
envie `SORTER_TEST_RESYNC` — solo `LFPG_RPCGuard.c:66` lo nombra. Cablea el emisor desde la V4 y
el consumo del refresh de cargo.

### 6. `IsOpen()` de la V4 en los dos sitios que hoy solo miran la V3
`scripts/4_World/LFPG_Actions.c:528` y `scripts/4_World/LFPG_ActionSyncSorter.c:69` hacen los dos
`if (LFPG_SorterView.IsOpen())` — verificado hoy. Con la V4 abierta, esos caminos creen que no hay
panel. Anade la consulta a la V4 **sin quitar la de la V3**: las dos tienen que convivir mientras
la V3 exista.

## S10 — divergencia de proteccion de preview
V3 y V4 difieren en la proteccion de preview y la direccion no esta decidida por el dueno.
**Instruccion para esta corrida: aplica la MAS ESTRICTA de las dos** (fail-closed) y **documenta en
el informe, en una seccion `### S10` propia, cual era cada una y cual aplicaste**, para que el dueno
lo confirme o lo invierta. No es una decision tuya: es un valor por defecto seguro y reversible.
