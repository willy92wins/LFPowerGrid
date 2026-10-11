# BRIEF - Lane i77: Reloj de cargadores, G-02/G-03 residuales (#77)

**MODO NO INTERACTIVO. EJECUTA DE PRINCIPIO A FIN.** No hay nadie al otro lado para aprobar un
diseno: **este brief ES la aprobacion**. No preguntes y no pares a confirmar. Si algo es ambiguo,
elige la opcion conservadora, hazla, y anota la decision y su motivo en el informe. Terminar sin
los entregables escritos en disco cuenta como no haber hecho el encargo.

## Papel
Eres el IMPLEMENTADOR de la lane i77. Hay cuatro lanes de Grok corriendo EN PARALELO sobre
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
Cerrar lo que la #71 dejo sin demostrar en el reloj de los cargadores vanilla (G-02 y G-03
residuales, issue #77): cada caso queda **arreglado con prueba offline**, o con un **argumento
verificado de que no se puede alcanzar**, o como **PENDIENTE-INGAME** con un protocolo exacto.

## El issue (#77), tal cual
> La #71 (fusionada el 4-oct, en la 1.2.5) separa la carga del barrido y cierra los intervalos en
> las transiciones del grafo. Este issue recoge lo que la #71 deja sin demostrar.
>
> **Ya probado en juego (5-oct):** con un cargador vanilla, 63,8 s con corriente dan +63,1;
> 47,3 s de apagon dan +0,6 (la cola del ultimo tick); 33,5 s con corriente dan +33,1. El tiempo
> del apagon no se acredita.
>
> **Pendiente:**
> - **G-02, tasa independiente del tamano.** Medir la misma energia por tiempo alimentado con
>   grafos de distinto tamano (por ejemplo, 512 y 2048 nodos) y con backlog en la cola, ademas del
>   coste y la latencia del tick de cargadores. Antes de la #71, con 512 nodos se llegaba al 62,5%
>   de la tasa declarada.
> - **G-03, casos que quedan:** un switch externo que el polling observa tarde (no captura el
>   instante del evento); detach y reinsercion del mismo objeto entre dos visitas; callbacks de
>   terceros. El reemplazo por otro objeto no hereda credito, pero puede descartar el tiempo
>   pendiente del adjunto retirado. Decidir si eso es aceptable.
>
> **Criterio de cierre.** Cada caso, con una prueba en juego o un argumento verificado de que no se
> puede alcanzar.

## Como esta hoy (verificalo, no te lo creas)
- `scripts/5_Mission/LFPG_ElecGraphImpl.c:3199-3345`: `TrackVanillaCharger`,
  `ChargerChargeAmount` (credita `(now - last) * LFPG_CHARGER_ENERGY_PER_SEC` solo si el
  intervalo cerrado era "cargando" y con la misma bateria), `UpdateVanillaChargerPower` (publica el
  reloj ANTES de `AddEnergy`), `UntrackVanillaCharger`, `ClearVanillaChargers`,
  `TickVanillaChargers` (round robin de hasta `LFPG_VALIDATE_BATCH_SIZE` = 32 cargadores por
  llamada).
- `TickVanillaChargers` se llama desde `scripts/5_Mission/LFPG_NetworkManagerImpl.c:~498`; las
  transiciones del grafo cierran intervalos en `LFPG_ElecGraphImpl.c:~937`, `~1724`, `~3193`.
- La API base vive en `scripts/4_World/LFPG_ElecGraph.c:~267-285` (4_World no ve 5_Mission).
- Test offline existente: `.github/tools/test_graph_charger_energy.py` (doce pruebas del reloj).

## Puntos del encargo (uno por fila en la tabla del informe)
Veredictos posibles: `ARREGLADO` / `INALCANZABLE` (argumento con `path:line`) /
`ACEPTABLE-DOCUMENTADO` (con cota numerica) / `PENDIENTE-INGAME` (con protocolo).
- **G03-a, switch externo observado tarde.** El interruptor del propio cargador
  (`chargerEm.IsSwitchedOn()`), la bateria llena o vaciada por otro lado: el polling solo lo ve en
  la siguiente visita. Cuantifica el error maximo (en u y en s) en funcion del numero de
  cargadores y del periodo de la llamada. Decide entre un hook de evento (p. ej. `modded class
  BatteryCharger` que llame a `super` y cierre el intervalo en el instante del cambio a traves de
  la API base de 4_World, solo en SERVER, sin alterar el comportamiento vanilla) o un argumento de
  que el error esta acotado y es aceptable. Justifica la eleccion.
- **G03-b, detach y reinsercion de la MISMA bateria entre dos visitas.** Hoy `sameBattery` sale
  verdadero y se acredita el tiempo que estuvo fuera. Arreglalo o demuestra que no se alcanza.
- **G03-c, callbacks de terceros.** `AddEnergy` puede disparar callbacks nativos o de otros mods
  que reentren en el grafo. Demuestra que la reentrada es segura (no doble credito, no reloj
  corrupto) o arreglala.
- **G03-d, reemplazo por otro objeto.** No hereda credito pero descarta el tiempo pendiente del
  adjunto retirado. Cuantifica la perdida maxima y recomienda `ACEPTABLE-DOCUMENTADO` o un
  arreglo. La decision final es del dueno: deja la recomendacion y su cota claras.
- **G02, tasa independiente del tamano.** Demuestra offline, cargando sentencias reales, que la
  energia por tiempo alimentado no depende del numero de nodos del grafo (512 frente a 2048) ni
  de que `ProcessDirtyQueue` tenga backlog o presupuesto agotado. Acota el coste del tick de
  cargadores (visitas por llamada) y su latencia (tiempo maximo entre dos visitas de un cargador
  con C cargadores registrados). Si encuentras dependencia, arreglala.

## LISTA BLANCA (solo puedes escribir aqui)
- `scripts/5_Mission/LFPG_ElecGraphImpl.c`: SOLO la zona de cargadores (~3199-3345) y los sitios
  de llamada que cierran intervalos.
- `scripts/4_World/LFPG_ElecGraph.c`: solo si necesitas declarar en la base un metodo nuevo.
- Un fichero NUEVO en `scripts/4_World/` para el hook de `BatteryCharger`, solo si lo justificas.
- `.github/tools/test_graph_charger_energy.py` (extender) o un test nuevo
  `.github/tools/test_graph_charger_<algo>.py`.
- `.github/workflows/checks.yml`: solo anadir el paso del test nuevo, con el mismo estilo.
- `reviews/2026-10-10-grafo-grok/i77/INFORME.md` y `reviews/2026-10-10-grafo-grok/i77/PROTOCOLO-INGAME.md`.

## Excluido
`AllocateOutput` y el reparto (lane i76), `MarkUpstreamNodesDirty`/CutAll/cortes de puerto e
instrumentacion de rendimiento (lane i78), `.github/tools/graph_reference/` (lane i72).

## CRITERIO DE HECHO (lo que lo hace estar bien, no parecerlo)
1. Los cinco puntos con veredicto y evidencia `path:line` en el informe.
2. Cada arreglo con test offline de positivos y **negativos** que FALLA contra la base y pasa con
   tu cambio (constancia en `## GATES`).
3. `test_graph_charger_energy.py`, el resto de `.github/tools/test_*.py` y
   `enforce_checks.py --root .`: exit 0 / FAIL=0.
4. Linter offline: `errors` final == `errors` base en el arbol entero; delta de warnings explicado.
5. `PROTOCOLO-INGAME.md`: por cada punto que lo necesite, pasos exactos (dispositivos, orden,
   tiempos), lecturas esperadas con numeros y tolerancia, al estilo de la prueba del 5-oct
   (63,8 s -> +63,1).

## Informe obligatorio
`reviews/2026-10-10-grafo-grok/i77/INFORME.md`, en castellano. Escribelo con `search_replace`
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
