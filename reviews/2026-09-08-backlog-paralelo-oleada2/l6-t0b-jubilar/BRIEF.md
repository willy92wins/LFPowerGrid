# BRIEF - Lane T0b . Jubilar la V3 del sorter

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
Jubilar la V3 del sorter: borrarla y retirar todo lo que existia solo para que V3 y V4
convivieran. **La V4 se queda como la unica.**

## POR QUE AHORA, Y QUE CAMBIO HACE UNA HORA
El bloqueo era que la V4 no tenia `LFPG_UIScaler` ni las guardas de apertura de produccion, asi
que borrar la V3 dejaba en produccion un panel no apto. **Eso ya esta hecho y revisado**
(commits de hoy en `main`): la V4 lleva el scaler con aislamiento de estado, las guardas
alineadas, la sonda gateada y el hook MCP fuera del build publicado. El dueno ha dado la orden de
jubilar. Tu cierras el tramo.

## LISTA BLANCA (puedes escribir SOLO en estos)
- Borrar enteros: `scripts/4_World/LFPG_SorterView.c`, `LFPG_SorterController.c`,
  `LFPG_SorterTagView.c`, `LFPG_SorterPreviewRow.c`, `LFPG_Sorter.c`,
  `LFPG_ActionOpenSorterPanel.c`.
- Borrar enteros: `gui/layouts/LFPG_Sorter.layout`, `gui/layouts/LFPG_SorterPreviewRow.layout`,
  `gui/layouts/LFPG_SorterTag.layout`.
- Editar: `scripts/5_Mission/LFPG_MissionInit.c`, `scripts/4_World/LFPG_RPCClientHandler.c`,
  `scripts/4_World/LFPG_ActionRegistration.c`, `scripts/4_World/LFPG_Actions.c`,
  `scripts/4_World/LFPG_ActionSyncSorter.c`, `scripts/4_World/test/LFPG_SorterView_TEST.c`,
  `config.cpp`.

**SOLO LECTURA:** todo lo demas. Hay otras siete lanes trabajando en paralelo.

## Lo que hay que hacer

### 1. Censar antes de borrar — y el censo manda sobre mis cifras
El plan viejo dice «7 clases y 3 layouts». **Yo he verificado hoy 6 ficheros y 3 layouts**, y un
fichero puede declarar mas de una clase. **Haz tu el censo** con grep y **pon el numero real en el
informe**. Si mi cifra no cuadra con la tuya, manda la tuya: dilo y sigue, no te pares.

### 2. Borrar la V3 y dejar el arbol sin referencias colgando
Borra los ficheros y layouts de la lista. Despues **busca en TODO el arbol** cualquier referencia
que quede a los simbolos borrados (`LFPG_SorterView`, `LFPG_SorterController`, `LFPG_SorterTagView`,
`LFPG_SorterPreviewRow`, `LFPG_Sorter` a secas, `LFPG_ActionOpenSorterPanel` sin `_TEST`) y limpiala.
**Criterio de exito: grep cero de esos simbolos fuera de comentarios historicos.** Enforce no
compila aqui; una referencia colgando es un fallo de carga del mundo que nadie va a ver hasta el
arranque, asi que el grep es tu unico gate.

### 3. Los sitios concretos que he verificado hoy, para que no los busques a ciegas
- `scripts/5_Mission/LFPG_MissionInit.c` — **9 referencias** a la V3.
- `scripts/4_World/LFPG_ActionRegistration.c:66` — `actions.Insert(LFPG_ActionOpenSorterPanel);`,
  la V3. En `:69` esta la V4 (`..._TEST`): **esa se queda**.
- `scripts/4_World/LFPG_Actions.c` y `LFPG_ActionSyncSorter.c` — hoy consultan `IsOpen()` de las
  DOS. Al morir la V3 se queda solo la de la V4.
- `scripts/4_World/test/LFPG_SorterView_TEST.c:1398` — el **mutex anti dual-open**:
  `if (LFPG_SorterView.IsOpen())` con el mensaje «production sorter is already open». Ya no hay
  dos paneles: sobra el bloque entero.
- `scripts/4_World/LFPG_RPCClientHandler.c` — determina **tu** cuales de sus handlers son de la V3
  y cuales de la V4. Los `SORTER_TEST_*` de `:71` a `:91` son de la V4 y **se quedan todos**.

### 4. `config.cpp` — mira, decide y justifica
Comprueba si la entidad V3 del sorter esta declarada en `config.cpp`. **Cuidado: los classnames
de `CfgVehicles` estan ligados a persistencia.** Si borras la declaracion de una entidad que algun
mundo tenga colocada, rompes ese mundo. Si encuentras ese caso, **NO la borres**: dejala, marcala
en el informe como `PENDIENTE-DECISION` y explica el riesgo. Es preferible una entrada de config
huerfana a un mundo roto.
**No toques `LFPG_Sorter_TEST` ni `LFPG_Sorter_TEST_Kit`** (`config.cpp:1080` y `:1086`): son de la
V4 y el renombrado a canonico es otro tramo.

### 5. NO renombres nada
El paso `_TEST` -> canonico es un tramo aparte y no entra aqui. La V4 se queda con sus nombres
`_TEST` tal cual.

## Riesgo que quiero que midas y declares
Este borrado **no se ha probado in-game y no se puede probar aqui**. En el informe, seccion propia:
**que se rompe si te has dejado una referencia**, y que tendria que mirar el dueno al arrancar el
mundo por primera vez despues de esto.

---

## CORRECCION DE ALCANCE APLICADA A MITAD DE CORRIDA (receptor, 2026-09-08)

La primera corrida de esta lane **se nego, con razon**: `LFPG_Sorter_TEST` hereda de `LFPG_Sorter`
en script (`test/LFPG_Sorter_TEST.c:15`) y en config (`config.cpp:1086`), asi que borrar
`LFPG_Sorter.c` como pedia el brief original habria roto la compilacion de World y los mundos con
sorters colocados. Ademas, los dos ficheros necesarios para deshacer esa dependencia estaban en la
lista de SOLO LECTURA: el encargo era literalmente imposible tal y como se escribio.

**El receptor amplio la lista blanca por prompt de `resume`, y esta seccion lo deja por escrito:**

- **Ya NO se borra** `scripts/4_World/LFPG_Sorter.c`: se conserva con sus dos clases
  (`LFPG_Sorter_Kit`, `LFPG_Sorter`), porque es la base de la V4 y esta ligada a persistencia.
- **Se anaden a la lista blanca** `scripts/4_World/test/LFPG_Sorter_TEST.c` y
  `scripts/4_World/test/LFPG_ActionOpenSorterPanel_TEST.c`.
- **Jubilar la V3 = borrar su INTERFAZ** (view, controller, tag view, preview row, accion y 3
  layouts), no su entidad.

**Aviso para quien lea el dictamen de Grok de esta lane:** su GRAVE de «salida de la lista blanca»
apunta a esos dos ficheros y es correcto **contra el brief original**, que es el unico que el
revisor tuvo delante. El receptor los declara dentro de alcance. No hay choque con ninguna otra
lane: ninguna de las doce restantes toca esos dos paths.
