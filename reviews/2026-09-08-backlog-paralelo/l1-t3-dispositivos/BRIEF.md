# BRIEF — Lane 1 · T3 Dispositivos rotos o que mienten

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
`scripts/4_World/LFPG_Furnace.c`, `scripts/4_World/LFPG_Battery.c`,
`scripts/4_World/LFPG_Intercom.c`, `scripts/4_World/LFPG_BatteryAdapter.c`,
`scripts/4_World/lfpg_devicebase.c`, `scripts/5_Mission/LFPG_NetworkManagerImpl.c`, `config.cpp`.

⚠ `LFPG_NetworkManagerImpl.c` lo tocan tambien otras lanes, en zonas MUY lejanas (~linea 2150 y
~6200-6600). **Tu solo puedes tocar la zona ~7600-7750.** Fuera de ahi, no escribas en ese fichero.

## Las fichas — las cinco son P1 y estan CONFIRMADAS contra el codigo

### D01 — El horno reinicia indefinidamente el plazo de consumo al apagar y encender
`4_World/LFPG_Furnace.c` ~`:453-470` y ~`:540-547`. El deadline absoluto se reescribe en cada
encendido y **`m_BurnNextMs` no se persiste**. Un jugador que apaga y enciende nunca consume.

### D02 — La restauracion de calor ocurre antes de crear la fuente termica
`4_World/LFPG_Furnace.c` ~`:121-145` y ~`:289-292`. `super.EEInit()` activa el calor **antes** de
que exista `m_UTSource`; el guard se traga el fallo en silencio.

### D03 — La cuantizacion de bateria altera la contabilidad energetica autoritativa
`4_World/LFPG_Battery.c` ~`:341-352` (verificado: `LFPG_GetStoredEnergy()` hace
`result = m_StoredEnergyX10; result = result / 10.0;`) y `5_Mission/LFPG_NetworkManagerImpl.c`
~`:7644`, ~`:7722`. Round-trip float -> int x10 -> float **en cada tick**; el adaptador no lo
sufre, asi que hay **asimetria de precision** entre dos caminos que deberian cuadrar.

### D04 — El intercom crea una clase PAS no declarada en config · **NO LA IMPLEMENTES**
`4_World/LFPG_Intercom.c` ~`:822-831`. `LFPG_GhostPASBroadcaster` **no esta en CfgVehicles**;
su hermano `LFPG_GhostPASReceiver` si (`config.cpp:2562`). `config.cpp` es el unico config del
arbol y no tiene includes. Funcion muerta.
**Esta ficha tiene dos lecturas y la decision es del dueno, no tuya.** Marca
`PENDIENTE-DECISION` y **no cambies codigo por ella**. En el informe escribe, con precision de
copiar y pegar, las DOS opciones: (a) el bloque exacto que habria que anadir a `config.cpp` para
declarar la clase, modelado sobre el del hermano en `:2562`; (b) la lista exacta de que codigo
habria que borrar si se retira la funcion. Nada mas.

### D05 — El BatteryAdapter desplegado no tiene via de recuperacion coherente
`4_World/LFPG_BatteryAdapter.c` ~`:84-89` y `4_World/lfpg_devicebase.c` ~`:430-455`. El comentario
promete recogida con F, pero **no reoverridea los tres guards heredados**, y sin kit no hay
desmontaje: queda irrecuperable. Decide si la cura es cumplir la promesa del comentario o corregir
el comentario, y **justificalo**.
