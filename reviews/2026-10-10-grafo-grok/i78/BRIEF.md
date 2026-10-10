# BRIEF - Lane i78: Medicion de R-01, R-02, R-04 y R-05 (#78)

**MODO NO INTERACTIVO. EJECUTA DE PRINCIPIO A FIN.** No hay nadie al otro lado para aprobar un
diseno: **este brief ES la aprobacion**. No preguntes y no pares a confirmar. Si algo es ambiguo,
elige la opcion conservadora, hazla, y anota la decision y su motivo en el informe. Terminar sin
los entregables escritos en disco cuenta como no haber hecho el encargo.

## Papel
Eres el IMPLEMENTADOR de la lane i78. Hay cuatro lanes de Grok corriendo EN PARALELO sobre
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
Instrumentar la **medicion** de R-01, R-02, R-04 y R-05 (issue #78) para poder medirlos dentro de
DayZ. **Esta ronda NO refactoriza nada**: la regla del issue es medir primero; cada refactor ira
despues en su propia PR, con la medida delante.

## El issue (#78), tal cual
> **Primero se mide y despues se refactoriza**, cada punto en su propia PR. Ninguno tiene medida
> dentro de DayZ todavia.
> - **R-01, reconstruccion tras CutAll.** El camino con cambios puede reconstruir dispositivos,
>   wires y componentes globales fuera del presupuesto electrico. Evaluar la reparacion por isla y
>   la agrupacion de eventos, conservando la recuperacion completa. T5-03 permite un rebuild por
>   cascada, asi que esta ruta no prueba una violacion por si sola.
>   `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1125` (en `b7b917e`)
> - **R-02, upstream repetido.** Los cambios de consumo solapados repiten el BFS y el scratch
>   aunque la cola deduplique entradas. Evaluar la union de raices y la invalidacion por version,
>   sin retrasar los apagados. `scripts/5_Mission/LFPG_ElecGraphImpl.c:3723`
>   (`MarkUpstreamNodesDirty`)
> - **R-04, scan global al cortar un puerto IN.** Tras el acceso indexado se ejecuta la
>   recuperacion global aunque el indice este sano. No se puede omitir el scan por un simple hit:
>   hay que probar la completitud de owners y contadores, la invalidacion transaccional y los
>   negativos con el indice parcial. `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:2067`
> - **R-05, enumeracion indexada de maps.** `map.GetElement/GetKey` es O(n) segun
>   `1_core/proto/enscript.c:861/871`. Medir el coste agregado de los barridos que los usan por
>   indice y evaluar registros de IDs que se mantengan bien en todo el lifecycle. La #71 ya lo
>   evita en el tick de cargadores.
>
> **Como medir.** El tiempo completo por evento y por fase, las visitas y requeues, las
> allocations y los ticks hasta el settlement. Mover trabajo a otro modulo o tick, o retirar
> funciones, no cuenta como optimizacion.

## Como esta hoy (verificalo)
- `ProcessDirtyQueue` (`scripts/5_Mission/LFPG_ElecGraphImpl.c:~2172`) ya mide su propio tiempo
  con `g_Game.GetTime()` (ms) en `m_LastProcessMs` y cuenta `m_EdgesVisitedThisEpoch`; reutiliza
  lo que valga y cita donde.
- `scripts/3_Game/LFPG_Telemetry.c` es **solo cliente** (`#ifndef SERVER`); el grafo corre en el
  servidor. No lo reutilices tal cual.
- Si existe `P:\scripts\1_core\proto\enscript.c` puedes LEERLO (solo lectura) para citar el
  contrato O(n) de `map.GetElement/GetKey`; si no, cita el issue.

## Lo que tienes que entregar
1. **Puntos de medicion** en las cuatro rutas, que registren por evento: tiempo total y por fase,
   nodos/edges visitados, requeues, allocations (cuenta de `new` de arrays/maps/objetos en esa
   ruta) y ticks hasta settlement (cola sucia vacia). Para R-05: contador de llamadas a
   `GetElement/GetKey` por indice y tamano del map recorrido en cada barrido.
2. **Apagado por defecto y sin cambio de comportamiento.** Con la medicion apagada el coste
   anadido por evento es cero (compilacion condicional) o, como mucho, una comprobacion booleana.
   Elige y justifica: un `#define` en un solo sitio o un ajuste de servidor existente. **No
   anadas campos a ficheros persistidos, a settings guardados ni a RPC.**
3. **Salida**: lineas de log con `LFPG_Util` (nunca `Print`), formato fijo `clave=valor`
   parseable, una por evento medido mas un resumen por intervalo.
4. **`MEDICION.md`**: protocolo in-game por punto (escenario a montar, numero de dispositivos,
   acciones exactas, que lineas leer, y que numero decidiria que merece refactor), el inventario
   estatico R-05 (cada bucle con `path:line`, map que recorre, tamano esperado y frecuencia), y
   por cada R: que refactor plantea el issue, su riesgo y que medida lo justificaria. **Sin
   implementarlo.**

## LISTA BLANCA (solo puedes escribir aqui)
- `scripts/5_Mission/LFPG_ElecGraphImpl.c`: solo puntos de medicion. **NO** toques
  `AllocateOutput` (~3907-4252) ni la zona de cargadores (~3199-3345).
- `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`: solo puntos de medicion.
- `scripts/3_Game/LFPG_Defines.c`: solo el define o constantes de medicion.
- Un fichero NUEVO para el acumulador de metricas de servidor, si hace falta, respetando el orden
  de compilacion.
- Un test nuevo `.github/tools/test_graph_perf_probe.py` y su paso en `.github/workflows/checks.yml`.
- `reviews/2026-10-10-grafo-grok/i78/INFORME.md` y `reviews/2026-10-10-grafo-grok/i78/MEDICION.md`.

## Excluido
`AllocateOutput` (lane i76), zona de cargadores (lane i77), `.github/tools/graph_reference/`
(lane i72). Cualquier refactor de R-01..R-05.

## CRITERIO DE HECHO
1. **Cero cambio de comportamiento**: fuera de los bloques de medicion no cambia ninguna linea
   ejecutable (el revisor lo comprueba en el diff). Ninguna medicion cambia el orden ni las
   condiciones de lo que mide.
2. El test nuevo tiene positivos y **negativos**: falla si se quita un punto de medicion de
   cualquiera de las cuatro rutas, y falla si la medicion queda activa por defecto.
3. `.github/tools/test_*.py` y `enforce_checks.py --root .`: exit 0 / FAIL=0.
4. Linter offline: `errors` final == `errors` base; delta de warnings explicado.
5. `MEDICION.md` con las cuatro secciones y el inventario R-05 completo.

## Informe obligatorio
`reviews/2026-10-10-grafo-grok/i78/INFORME.md`, en castellano. Escribelo con `search_replace`
(con `old_string` vacio para crearlo). Contenido:
1. Tabla inicial: una fila por punto del encargo con `punto | veredicto | ficheros:lineas | prueba`.
2. Una seccion `###` por punto: que cambiaste (con `path:line` del codigo YA modificado), por que
   esa opcion, que alternativa descartaste, y como se verifica (offline hecho / `PENDIENTE-INGAME`
   con el protocolo).
3. `## GATES`: cada comando que corriste y su resultado literal (linter: errors/warnings base y
   final; cada test: exit code y numero de tests; confirmacion de que tus tests de arreglo fallan
   contra la base).
4. `## HALLAZGOS-ADYACENTES` (R20: lo que viste y no tocaste).
5. `## LO QUE NO PUDE VERIFICAR` — una linea por cosa.
6. `## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO` — obligatoria. Si crees que el issue
   describe mal el problema o que el arreglo correcto es otro, dilo aqui.

## Salida en el chat (solo esto, sin pegar codigo ni el informe)

## RECEIPT
```json
{
  "status": "ok|failed",
  "paths": ["rutas relativas de todo lo que creaste o modificaste"],
  "summary": "una linea",
  "gates": {"linter_errors_base": 0, "linter_errors_final": 0, "linter_warnings_base": 0, "linter_warnings_final": 0, "tests": "nombre: exit, ..."},
  "verified": ["lo que comprobaste tu"],
  "not_verified": ["lo que no pudiste comprobar"]
}
```
