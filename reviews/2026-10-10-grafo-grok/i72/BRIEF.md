# BRIEF - Lane i72: Referente independiente y checklist de cierre (#72)

**MODO NO INTERACTIVO. EJECUTA DE PRINCIPIO A FIN.** No hay nadie al otro lado para aprobar un
diseno: **este brief ES la aprobacion**. No preguntes y no pares a confirmar. Si algo es ambiguo,
elige la opcion conservadora, hazla, y anota la decision y su motivo en el informe. Terminar sin
los entregables escritos en disco cuenta como no haber hecho el encargo.

## Papel
Eres el IMPLEMENTADOR de la lane i72. Hay cuatro lanes de Grok corriendo EN PARALELO sobre
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
Construir el **referente independiente** offline que el issue #72 exige para validar capacidad,
conservacion y settlement del grafo, y la **checklist de cierre** de #72. No tocas `scripts/`.

## El issue (#72), lo que aplica, tal cual
> **G-01 — reparto multifuente.** El modelo 20+50 -> Combiner -> demanda hard 50 pide 25 a cada
> proveedor; la fuente de 20 sobrecarga/asigna cero y la otra aporta 25, aunque existe la
> asignacion factible 20+30. [...] Pendiente spec y PR del solver: capacidad residual por edge,
> prioridad hard, cold start, recuperacion y fuentes compartidas. Excluir fuentes en overload o
> ponderar solo capacidad nominal no demuestra conservacion ni recuperacion.
>
> **Antes de merge/cierre:** CI de cada PR, compilacion cliente/servidor, fixture y orden de mods
> registrados, lease del MCP compartido y SHA del PBO normal desplegado. Probar capacidad,
> conservacion y settlement contra un referente independiente: fuentes heterogeneas y deficit
> real, fuentes compartidas, gates hard/soft, adjuntos y energia del cargador, apagones/cortes,
> bateria llena, lifecycle y corrupcion deliberada de indices. En cambios locales, componentId y
> potencia de islas ajenas deben permanecer intactos. Medir el tiempo completo por evento/fase,
> visitas/requeues, allocations y ticks hasta settlement.
>
> Conservar features, classnames, persistencia/RPC, SyncVars y permisos. [...]
> No se reabren como bugs los candidatos refutados CUT_PORT OUT y targetPort legacy vacio.
> Ratio-only soft, indice parcialmente corrupto e identidad provisional durante restore siguen
> como hipotesis hasta demostrar un trigger alcanzable.

Estado al 6-oct (comentario del dueno): G-04 cerrado por prueba en juego, salvo "sin probar: que
SOURCE, CONSUMER y CAMERA no cambien". G-01 -> #76, G-02/G-03 -> #77, R-01/R-02/R-04/R-05 -> #78.
#72 queda como indice y se cierra cuando se cierren #76, #77 y #78.

## La independencia es el producto
Puedes LEER `scripts/5_Mission/LFPG_ElecGraphImpl.c` y `scripts/3_Game/LFPG_Data.c` solo para
entender la **semantica** (tipos de nodo SOURCE/PASSTHROUGH/CONSUMER/CAMERA, que es demanda hard y
soft, `m_SoftDemandRatio`, gates con demanda de sondeo, edges habilitados). **No portes ni imites
el algoritmo** de `AllocateOutput` ni de `ProcessDirtyQueue`: el oraculo calcula por un **metodo
distinto** (p. ej. flujo maximo por fases de prioridad, o busqueda exhaustiva sobre fixtures
pequenos). Un referente que copia al solver hereda sus bugs y no demuestra nada.

## Lo que tienes que entregar
1. **Paquete `.github/tools/graph_reference/`** (solo libreria estandar), con:
   - modelo de red: fuentes con salida disponible; PASSTHROUGH con limite de paso y fraccion soft;
     consumidores con demanda hard; gates; edges habilitados o no; soft = carga de bateria
     (bateria llena = soft 0);
   - oraculo que, dado un grafo, devuelve (a) si existe una asignacion factible que alimente toda
     la demanda hard, (b) el maximo hard servible, y (c) la verificacion de una asignacion
     propuesta: conservacion por nodo, limites por fuente y por edge, prioridad hard sobre soft,
     con un informe de que regla se viola y donde;
   - `README.md` con la API minima (entrada y salida, con un ejemplo) para que la lane del solver
     (#76) la use despues sin leer tu codigo.
2. **Fixtures JSON** en el paquete que cubran: 20+50 -> Combiner -> hard 50; fuentes heterogeneas
   con deficit real; una fuente compartida entre dos islas; gates hard y soft mezclados; bateria
   llena; un edge cortado. Cada fixture con su resultado esperado **calculado a mano** en un campo
   del propio JSON, con la cuenta.
3. **`.github/tools/test_graph_reference.py`** con positivos y **negativos**:
   - el oraculo MARCA como violacion la asignacion del reparto a partes iguales de hoy en
     20+50 -> hard 50 (0 de la fuente de 20 en overload + 25 de la de 50: consumidor sin
     alimentar aunque 20+30 era factible);
   - marca una asignacion que no conserva y una que excede el limite de una fuente;
   - acepta 20+30;
   - y cada fixture devuelve su resultado esperado.
   Anade su paso en `.github/workflows/checks.yml`, con el estilo de los existentes.
4. **`CIERRE-72.md`**: tabla con cada exigencia del parrafo "Antes de merge/cierre" de #72 ->
   donde se cubre (test offline concreto / protocolo in-game / fuera de alcance con motivo) y de
   quien es (i76 / i77 / i78 / dueno). Incluye el pendiente de G-04 (SOURCE, CONSUMER y CAMERA sin
   cambios) con su protocolo in-game, y las tres hipotesis que siguen abiertas con que trigger
   haria falta demostrar.

## LISTA BLANCA (solo puedes escribir aqui)
- `.github/tools/graph_reference/**`
- `.github/tools/test_graph_reference.py`
- `.github/workflows/checks.yml` (solo anadir el paso)
- `reviews/2026-10-10-grafo-grok/i72/INFORME.md` y `reviews/2026-10-10-grafo-grok/i72/CIERRE-72.md`
**No toques nada de `scripts/`.**

## CRITERIO DE HECHO
1. `C:\Python314\python.exe .github/tools/test_graph_reference.py` -> exit 0, en menos de 10 s,
   con los negativos del punto 3 presentes y pasando (es decir, el oraculo detecta cada defecto).
2. Solo libreria estandar (el revisor hace grep de los `import`).
3. El resto de `.github/tools/test_*.py` y `enforce_checks.py --root .` siguen en exit 0 / FAIL=0.
4. Cada fixture con la cuenta a mano de su resultado esperado.
5. `CIERRE-72.md` cubre todas las exigencias del parrafo citado, una fila por exigencia.

## Informe obligatorio
`reviews/2026-10-10-grafo-grok/i72/INFORME.md`, en castellano. Escribelo con `search_replace`
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
