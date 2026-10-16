# BRIEF - Lane i76: SPEC del reparto multifuente G-01 (#76)

**MODO NO INTERACTIVO. EJECUTA DE PRINCIPIO A FIN.** No hay nadie al otro lado para aprobar un
diseno: **este brief ES la aprobacion**. No preguntes y no pares a confirmar. Si algo es ambiguo,
elige la opcion conservadora, hazla, y anota la decision y su motivo en el informe. Terminar sin
los entregables escritos en disco cuenta como no haber hecho el encargo.

## Papel
Eres el IMPLEMENTADOR de la lane i76. Hay cuatro lanes de Grok corriendo EN PARALELO sobre
worktrees independientes del mismo repo (ramas `grok/i72-*`, `grok/i76-*`, `grok/i77-*`,
`grok/i78-*`), una por issue del grafo electrico. No ves a las otras. Claude (otra familia de
modelo) revisara tu diff linea a linea y te devolvera, en esta misma sesion, SOLO lo que este roto.
Escribe pensando en ese revisor: cada afirmacion con `path:line` que el pueda abrir.

## Proyecto
LFPowerGrid: mod de DayZ en Enforce Script (red electrica con cables). El codigo vive en
`scripts/3_Game/`, `scripts/4_World/` y `scripts/5_Mission/`. Orden de compilacion:
3_Game -> 4_World -> 5_Mission, y **cada modulo solo ve los anteriores**: nada de llamar desde
4_World a algo de 5_Mission. Enforce solo compila al cargar el mundo. **Aqui no hay compilador que
puedas invocar y no hay juego que puedas arrancar.** No inventes un comando de build ni declares
"compila". Las instrucciones del repo estan en `AGENTS.md` (leelo: secciones 1 a 6).

## Convenciones de Enforce — OBLIGATORIAS, el revisor las comprueba linea a linea
Prohibido en toda linea que anadas o modifiques:
- ternarios `? :`
- `++` y `--`  -> escribe `x = x + 1;`
- `+=`, `-=`, `*=`, `/=`  -> escribe `x = x + y;`
- `foreach`  -> `for (int i = 0; i < arr.Count(); i = i + 1)`
- `ref` en parametros, retornos, locales o typedefs. **`ref` SOLO en miembros de clase.**
- `Print(...)`  -> usa `LFPG_Util.Error/Warn/Info/Debug`
Naming: miembros `m_`, estaticos `s_`, metodos PascalCase, locales camelCase, indentacion con tabs.
Codigo de servidor dentro de `#ifdef SERVER` como el que lo rodea.

**Punto ciego medido del linter:** NO ve un campo no declarado. Por cada local tipada
`LFPG_X obj = ...` de tu diff, comprueba que cada `obj.m_Campo` que uses esta declarado en
`LFPG_X` o en sus padres. Un fallo asi tumba la compilacion del mundo entero en el arranque.

## Pruebas offline (Python)
Solo libreria estandar: la CI corre `python 3.12` en ubuntu sin dependencias
(`.github/workflows/checks.yml`). Patron de la casa: `.github/tools/enforce_scalar_slice.py`
carga sentencias REALES de produccion y las ejecuta con stubs; mira
`.github/tools/test_graph_charger_energy.py` y `.github/tools/test_graph_capacity_refresh.py`.
Un test que no puede fallar no es un test: **cada test de arreglo debe FALLAR contra el codigo de
la base** (puedes leerla con `git show HEAD:<ruta>`; tu base es el commit de tu rama sin tus
cambios) y pasar con tu cambio. Deja constancia en el informe de que lo comprobaste.

## Reglas del pipeline (resumen; no las relitigues)
- **R25 simplicidad:** nada de features, imports, ajustes ni abstracciones que este brief no pida.
- **R20 anti-refactor incidental:** cada linea cambiada traza a este brief. Dead code o defectos
  adyacentes que veas van a la seccion `HALLAZGOS-ADYACENTES` del informe; NO los arregles.
- **R26 criterios verificables:** los criterios de hecho de abajo son medibles. Si uno no se puede
  verificar tal como esta escrito, dilo en el informe en vez de inventarte otro.
- Conserva features, classnames de `CfgVehicles`, persistencia, RPC, SyncVars y permisos. **Hay
  jugadores reales en un servidor privado.**

## Fronteras
- Trabaja y escribe SOLO dentro de tu worktree (tu `cwd`) y SOLO en tu LISTA BLANCA.
- **Prohibido** `git commit`, `git add`, `git checkout`, `git stash`, `git reset`, `git merge`,
  `git rebase`, `git cherry-pick`, `git push`. Deja los cambios en el arbol de trabajo: el commit lo
  pone quien revisa.
- Prohibido `rm` / `Remove-Item`. No lances subagentes. No uses herramientas MCP.
- **NO lances ni mates procesos DayZ** (DayZDiag, DayZServer). Lo que dependa del juego se
  declara `PENDIENTE-INGAME` con su protocolo; no lo des por verificado.
- **No reformatees.** No normalices finales de linea, no reindentes codigo ajeno, no reordenes
  nada. Respeta el final de linea del fichero que editas. Un diff inflado por reformateo invalida
  la revision entera.
- No modifiques este `BRIEF.md`.

## TRAMPA de los path:line
Las citas de este brief y de los issues se tomaron contra `main` (`463464e`, que es la base de tu
rama) o contra commits anteriores (`b7b917e`, `2606245`). Localiza cada sitio **por contenido**
(el texto que se describe), no por numero de linea. Si no encuentras lo descrito, **no inventes**:
dilo.

## Entorno ya resuelto (no lo adivines)
- Python: `C:\Python314\python.exe`
- Linter offline de Enforce (gate obligatorio si tocas `.c`, `.layout`, `config.cpp`):
  `C:\Python314\python.exe C:/Users/guill/DayZ-Modding-Knowledge-Pack/tools/dayz-script-validator/scripts/script_validator.py .`
  Saca JSON; las claves son `errors` y `warnings` en la raiz. **Gatea por `len(errors)`, nunca
  por `status` ni por el exit code** (exit 2 = WARN con 0 errores = aprobado). Tarda ~60 s.
  Correlo UNA vez al empezar, antes de editar nada, para tener tu base, y otra al terminar: lo que
  vale es el delta.
- Tests del repo: `C:\Python314\python.exe .github/tools/<test>.py` y
  `C:\Python314\python.exe .github/tools/enforce_checks.py --root .`
- `ui_reconcile.py` (misma carpeta que el linter) solo si tocas layouts o claves `#STR_`.

## EL ENCARGO EN UNA FRASE
Escribir la **SPEC** del reparto multifuente del grafo (G-01, issue #76). **Esta ronda NO toca
codigo**: el issue exige la spec antes de cualquier PR. La spec se revisa y, con el visto bueno, en
una ronda siguiente de esta MISMA sesion la implementaras siguiendola.

En esta ronda solo tienes herramientas de lectura y `search_replace` (sin shell). Es a proposito.
Ignora por ahora el linter y los tests del bloque comun de arriba: son para la ronda de
implementacion. Aqui no corres nada.

## El issue (#76), tal cual
> Sale de #72 (G-01). Aqui hace falta una spec antes de abrir cualquier PR.
>
> **Problema.** Con 20+50 -> Combiner -> una demanda hard de 50, el divisor pide 25 a cada
> proveedor. La fuente de 20 entra en sobrecarga y asigna cero, y la otra aporta solo 25, aunque
> existe la asignacion factible 20+30.
>
> En `main` `b7b917e`: divisor `scripts/5_Mission/LFPG_ElecGraphImpl.c:4027`, overload `:4058`.
>
> **La spec tiene que cubrir** la capacidad residual por edge, la prioridad hard, el cold start, la
> recuperacion tras sobrecarga y las fuentes compartidas entre islas. No basta con excluir las
> fuentes en overload ni con ponderar solo por la capacidad nominal: ninguna de las dos demuestra
> conservacion ni recuperacion.
>
> **Criterio de cierre.** Comparacion contra un referente independiente con fuentes heterogeneas y
> deficit real, fuentes compartidas, gates hard y soft, y recuperacion. Si el cambio es local, el
> `componentId` y la potencia de las islas ajenas no se tocan. Ademas, una prueba en juego.
>
> Se conservan las features, los classnames, la persistencia, el RPC, las SyncVars y los permisos.

## Carga inicial (en este orden; lee por rangos, no ficheros enteros)
1. `scripts/5_Mission/LFPG_ElecGraphImpl.c:3907-4252` — `AllocateOutput`: estimacion de demanda
   por edge, divisor por `CountPoweredIncoming` (~4027), decision de overload solo-hard (~4058),
   Pass 2 (hard) y Pass 3 (soft).
2. Mismo fichero: `:3781-3906` (`CountEnabledOutgoing`, `HasEnabledDownstream`,
   `CountPoweredIncoming`) y `:4253-4304` (`GetEdgeAllocatedPower`).
3. Mismo fichero: `:2172-2930` — `ProcessDirtyQueue`: donde se llama a `AllocateOutput` (~2507),
   epochs, requeue (`EnsureRequeueEpoch` ~4305) y como se sale de un overload.
4. `scripts/3_Game/LFPG_Data.c:194-360` — `LFPG_ElecNode` y `LFPG_ElecEdge` (campos reales).
5. `scripts/4_World/LFPG_Combiner.c` — el dispositivo del contraejemplo.
6. `scripts/3_Game/LFPG_Defines.c` — constantes del grafo (`LFPG_PROPAGATION_EPSILON`,
   `LFPG_GATE_PROBE_DEMAND`, presupuestos) y salidas de las fuentes reales.
7. `.github/tools/enforce_scalar_slice.py` y `.github/tools/test_graph_capacity_refresh.py` —
   el patron de pruebas offline que usara tu implementacion.

## ESCRITURA OBLIGATORIA (unico entregable de esta ronda)
- `reviews/2026-10-10-grafo-grok/i76/SPEC.md`, en castellano, con `search_replace` y
  `old_string` vacio. **No pegues el cuerpo en el chat.**

## Lo que la SPEC tiene que contener (el revisor lo comprueba seccion por seccion)
1. **Mecanismo actual** con `path:line`: como se estima la demanda de cada edge (incluido el tope
   de cold start v2.4), el divisor por `CountPoweredIncoming`, la decision de overload solo-hard,
   Pass 2/Pass 3, y como se recupera hoy un nodo de un overload.
2. **Contraejemplos trabajados con numeros**, cada uno con el resultado de HOY (calculado
   siguiendo el codigo) y el EXIGIDO:
   a. 20 + 50 -> Combiner -> hard 50 (hoy 0 + 25; exigido 20 + 30).
   b. Deficit real: 20 + 50 -> Combiner -> hard 80. Define que se exige y por que, coherente con
      la politica de overload vigente (no la cambies sin decirlo y justificarlo).
   c. Fuente compartida: una fuente que alimenta dos ramas/islas (p. ej. via splitter), una de
      ellas con Combiner multifuente.
   d. Gates hard y soft mezclados: bateria cargando (`m_SoftDemandRatio`) junto a carga hard.
   e. Cold start: primer epoch sin `m_LastStableOutput`.
   f. Recuperacion: tras retirar la carga que provocaba el overload, en cuantos epochs vuelve
      cada fuente a asignar.
   g. Una fuente que entra en overload por OTRA rama mientras alimenta el Combiner.
3. **Algoritmo propuesto**: la regla de reparto entre proveedores de un PASSTHROUGH multifuente
   (p. ej. proporcional a la capacidad residual que cada proveedor ofrecio en el epoch previo),
   la definicion exacta de "capacidad residual por edge", prioridad hard sobre soft, cold start,
   recuperacion, y **por que converge sin oscilar** (argumento con cota de epochs). Pseudocodigo
   en el estilo de Enforce (sin ternarios, sin `+=`).
4. **Invariantes numerados y comprobables**: conservacion (lo que asigna una fuente <= su salida
   disponible + epsilon; lo que entra en un PASSTHROUGH = lo que reparte), localidad
   (`componentId` y potencia de islas ajenas intactos), no oscilacion, y **regresion cero** para
   el caso de un solo proveedor alimentado por PASSTHROUGH (la inmensa mayoria de redes).
5. **Superficie del cambio**: funciones y rangos de lineas que tocaria la implementacion, y lo
   que NO se toca (persistencia, RPC, SyncVars, classnames, `LFPG_ConnectionRules.c`). Coste
   por epoch frente a hoy (sin barridos globales nuevos).
6. **Plan de pruebas** con criterios de pasa/falla:
   - Offline: un test slice por contraejemplo con positivos y **negativos**; el codigo de hoy debe
     FALLAR al menos el (a). Di que sentencias de produccion se cargan en el slice.
   - Referente independiente: otra lane construye en paralelo un oraculo en
     `.github/tools/graph_reference/` (no lo veras). Define aqui la interfaz minima que tu
     implementacion necesitara de el (entrada: nodos, edges, capacidades, demandas hard/soft;
     salida: asignacion factible y maximo hard servible) para integrarlo despues.
   - Protocolo in-game: montaje con dispositivos reales del mod que den 20 y 50 u/s (busca cuales
     en `config.cpp`/`LFPG_Defines.c` y citalos; si no existen esos valores exactos, elige los
     mas cercanos y recalcula), acciones, lecturas esperadas y donde leerlas.
7. `## LO QUE NO PUDE VERIFICAR` y `## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO`
   (obligatorias).

## LISTA BLANCA de esta ronda
Solo `reviews/2026-10-10-grafo-grok/i76/SPEC.md`. Nada mas.

## Excluido (son de otras lanes, no los propongas como parte de G-01)
La zona de cargadores vanilla (`TrackVanillaCharger`..`TickVanillaChargers`, ~3199-3345: lane
i77), `MarkUpstreamNodesDirty`/CutAll/cortes de puerto (lane i78), `.github/tools/graph_reference/`
(lane i72).

## Salida en el chat (solo esto)

## RECEIPT
```json
{
  "status": "ok|failed",
  "paths": ["reviews/2026-10-10-grafo-grok/i76/SPEC.md"],
  "summary": "una linea",
  "verified": ["lo que comprobaste tu, con path:line"],
  "not_verified": ["lo que no pudiste comprobar"]
}
```
