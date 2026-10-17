# BRIEF - Lane i78-r01: cortar todos los cables sin reconstruir la red (#78, R-01)

**MODO NO INTERACTIVO. EJECUTA DE PRINCIPIO A FIN.** No hay nadie al otro lado para aprobar un
diseno: **este brief ES la aprobacion**. No preguntes y no pares a confirmar. Si algo es ambiguo,
elige la opcion conservadora, hazla, y anota la decision y su motivo en el informe. Terminar sin
los entregables escritos en disco cuenta como no haber hecho el encargo.

## Papel
Eres el IMPLEMENTADOR de la lane i78-r01, una sola lane sobre su propio worktree (rama
`grok/i78-r01-corte`, base `main` en `4e897f2`). Claude (otra familia de modelo) revisara tu diff
linea a linea y te devolvera SOLO lo que este roto. Escribe pensando en ese revisor: cada
afirmacion con `path:line` que el pueda abrir.

## Proyecto
LFPowerGrid: mod de DayZ en Enforce Script (red electrica con cables). El codigo vive en
`scripts/3_Game/`, `scripts/4_World/` y `scripts/5_Mission/`. Orden de compilacion:
3_Game -> 4_World -> 5_Mission, y **cada modulo solo ve los anteriores**. Enforce solo compila al
cargar el mundo. **Aqui no hay compilador que puedas invocar y no hay juego que puedas arrancar.**
No inventes un comando de build ni declares "compila". Las instrucciones del repo estan en
`AGENTS.md` (leelo: secciones 1 a 6).

## Convenciones de Enforce — OBLIGATORIAS, el revisor las comprueba linea a linea
Prohibido en toda linea que anadas o modifiques:
- ternarios `? :`
- `++` y `--`  -> escribe `x = x + 1;`
- `+=`, `-=`, `*=`, `/=`  -> escribe `x = x + y;`
- `foreach`  -> `for (int i = 0; i < arr.Count(); i = i + 1)`
- `ref` en parametros, retornos, locales o typedefs. **`ref` SOLO en miembros de clase.**
- `Print(...)`  -> usa `LFPG_Util.Error/Warn/Info/Debug`
Comentarios del codigo en ingles, como los que ya hay en el fichero.

## Pruebas offline (Python)
Solo libreria estandar: la CI corre `python 3.12` en ubuntu sin dependencias
(`.github/workflows/checks.yml`). Patron de la casa: `.github/tools/enforce_scalar_slice.py`
carga sentencias REALES de produccion y las ejecuta con stubs; mira
`.github/tools/test_graph_charger_energy.py` y `.github/tools/test_graph_multifeed_split.py`.
Un test que no puede fallar no es un test: **cada test del arreglo debe FALLAR contra el codigo de
la base** (`git show HEAD:<ruta>`; tu base es el commit de tu rama sin tus cambios) y pasar con tu
cambio. Deja constancia en el informe, con la salida, de que lo comprobaste.

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
Las citas de este brief se tomaron contra `main` `4e897f2`, que es la base de tu rama. Aun asi,
localiza cada sitio **por contenido** (el texto que se describe), no solo por numero de linea. Si no
encuentras lo descrito, **no inventes**: dilo.

## Entorno ya resuelto (no lo adivines)
- Python: `C:\Python314\python.exe`
- Linter offline de Enforce (gate obligatorio porque tocas un `.c`):
  `C:\Python314\python.exe C:/Users/guill/DayZ-Modding-Knowledge-Pack/tools/dayz-script-validator/scripts/script_validator.py .`
  Saca JSON; las claves son `errors` y `warnings` en la raiz. **Gatea por `len(errors)`, nunca
  por `status` ni por el exit code** (exit 2 = WARN con 0 errores = aprobado). Tarda ~60 s.
  Correlo UNA vez al empezar, antes de editar nada, y otra al terminar: lo que vale es el delta.
- Tests del repo: `C:\Python314\python.exe .github/tools/<test>.py` (cada `test_*.py`) y
  `C:\Python314\python.exe .github/tools/enforce_checks.py --root .`

## EL ENCARGO EN UNA FRASE
Que `HandleCutWires` (cortar todos los cables de un aparato con los alicates) reconstruya el indice
inverso, y con el el grafo entero, **solo cuando el rescate encuentre cables huerfanos de verdad**
(`rescued > 0`), la misma regla que ya usa el corte de un puerto IN.

## El issue (#78, R-01), tal cual
> **R-01, reconstruccion tras CutAll.** El camino con cambios puede reconstruir dispositivos, wires
> y componentes globales fuera del presupuesto electrico. Evaluar la reparacion por isla y la
> agrupacion de eventos, conservando la recuperacion completa. T5-03 permite un rebuild por
> cascada, asi que esta ruta no prueba una violacion por si sola.

## La medicion (en partida, 13-oct, build `0843f22`, mismo arbol que `4e897f2`)
Sonda de #80 encendida (`LFPG_PERF_PROBE`), cortes con los alicates reales:

| Corte | Nodos | total_ms | index / rescue / rebuild | settle_ticks |
|---|---|---|---|---|
| Generador | 51 | 4 | 0 / 1 / 3 | 2 |
| Generador | 504 | 29 | 1 / 6 / 22 | 12 |
| Generador | 2031 | 124 | 0 / 28 / 96 | 49 |
| Splitter con cable de entrada | ~2034 | 5 | 1 / 0 / 4 | 1 |

El generador escribe siempre `[CutWires-Fallback] Reverse index was stale — rebuilding`, y la cola
recorre el grafo entero aunque la isla cortada sea de 10 nodos. El Splitter no depende del tamano.

## La causa (verificada por el revisor en `4e897f2`)
1. `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:1116-1128`, en `HandleCutWires`: un aparato con
   puertos y sin ningun puerto IN entra siempre en el rescate, y la condicion
   `rescued > 0 || (changed && !anyInRemovedByIndex)` es verdadera en cuanto se han quitado cables
   de salida, aunque el scan no encuentre nada. Llama a `RebuildReverseIdx()`. Lo mismo le pasa a
   un aparato con puertos IN que corta solo cables de salida (un Splitter sin cable de entrada).
2. `RebuildReverseIdx` pone `m_GraphFullRebuildRequired = true`
   (`scripts/5_Mission/LFPG_NetworkManagerImpl.c:904`), y `PostBulkRebuildAndPropagate`
   (`:853-860`) hace entonces `m_Graph.PostBulkRebuild` y marca todas las fuentes.
3. Historia: hasta `06de74b` (16-ago) el codigo decia `// If fallback found stale wires, rebuild
   reverse index to fix it`, pero la condicion era `changed && inRemovedTotal == 0`, y `changed` ya
   venia a true del borrado de los cables de salida. `06de74b` conservo ese comportamiento a
   proposito para no cambiar nada en aquel refactor (comentario de `:1121-1122`, "phantom
   reverse-index owners"). Leelo con `git show 06de74b -- scripts/4_World/LFPG_RPCServerHandler.c`.
4. `HandleCutPort` con un puerto IN (`LFPG_RPCServerHandlerImpl.c:2085-2101`) ya reconstruye solo
   si `rescued > 0`.
5. El camino del Splitter con cable de entrada no entra en el rescate (el puerto IN acierta en el
   indice) y nunca reconstruye. Sus cables de salida se quitan por el mismo pre-scan
   (`LFPG_RPCServerHandlerImpl.c:994-1010`) que los del generador.

## EL CAMBIO (exacto)
En `HandleCutWires`, la condicion de `:1123` pasa a ser `rescued > 0`, como en `HandleCutPort`.
- El rescate (`RescueStaleIncomingWires`) se sigue llamando en los mismos casos de `:1116`. No
  cambia el scan, ni `RescueStaleIncomingWires`, ni el texto del aviso: a partir de ahora solo sale
  cuando de verdad habia cables huerfanos.
- Actualiza el comentario de `:1121-1122`, que describe la regla vieja, para que diga la nueva. El
  de `:1084-1086` habla de cuando corre el scan, que no cambia: dejalo. Nada mas en el fichero.
- No toques `LFPG_NetworkManagerImpl.c`, `LFPG_ElecGraphImpl.c` ni la sonda de #80.

## LO QUE TIENES QUE DEMOSTRAR ANTES DE TOCAR CODIGO (en el INFORME, con path:line)
Si alguno de estos puntos falla, **no implementes el cambio**: escribe en el informe que falla y
por que, y para. Esa es la opcion conservadora.

- **D1, espejo del pre-scan.** El pre-scan de `:994-1010` (aparato con almacen de cables) y el de
  `:1027-1077` (fuente vanilla) quitan del indice exactamente los cables que despues borran
  `LFPG_DeviceAPI.ClearDeviceWires` / `ClearDeviceWiresForCreator`
  (`scripts/4_World/LFPG_IDevice.c:635-652`, `scripts/4_World/LFPG_WireOwnerBase.c:226-250`,
  `scripts/3_Game/LFPG_WireHelper.c` `ClearAll` / `ClearForCreator`). Cubre todas las
  combinaciones: ajustes presentes o no, `AllowCutOthersWires`, y creador vacio, igual al que corta
  u otro. Si encuentras un caso limite en el que no coinciden, no lo arregles: va a
  `HALLAZGOS-ADYACENTES` con su alcance, y di si el cambio lo empeora respecto a la base.
- **D2, el grafo se entera cable a cable.** Cada cable quitado en el pre-scan llega a
  `m_Graph.OnWireRemoved` a traves de `ReverseIdxRemove`
  (`LFPG_NetworkManagerImpl.c:1087-1133`), o deja `m_GraphFullRebuildRequired = true` (`:1131-1132`).
  Para cada condicion de ese gancho (`m_Graph`, dueno no vacio, `m_ExplicitGraphRemovalCredit`,
  `m_CutAllGraphBatchActive`), demuestra que se cumple dentro de `HandleCutWires`, o que si no se
  cumple la arista no existe o la reconstruccion completa sigue ocurriendo. En concreto:
  a. el ciclo de vida de `m_ExplicitGraphRemovalCredit`: quien lo escribe (`:760`) y quien lo
     consume; si un credito viejo puede coincidir con la clave de un cable del pre-scan;
  b. si `m_CutAllGraphBatchActive` (`:4098-4259`) puede valer true cuando corre `HandleCutWires`.
- **D3, duenos fantasma.** Tras el pre-scan pueden quedar en `m_ReverseOwners` duenos sin cable vivo
  (`ReverseIdxRemove` solo borra la lista entera cuando el contador llega a 1, `:1140-1148`). El
  camino del Splitter ya los produce hoy. Lista todos los lectores de `m_ReverseOwners` y de
  `m_ReverseIdx` y demuestra que cada uno tolera un dueno fantasma. Los que conoce el revisor:
  `CountWiresTargeting` (`:976-990`), `IsPortTargetedByPoweredSource` (`:991-1050`),
  `RemoveWiresTargeting` (`:1205-1354`), `DeviceHasAnyWires` (`:2564-2600`),
  `CleanDisappearedVanillaDevice` (`:2686-2706`), la comprobacion de `CutAllWiresFromDevice`
  (`:3995-4097`) y `scripts/4_World/LFPG_Actions.c:902`. Si hay mas, anadelos. Demuestra tambien
  que los contadores quedan exactos tras el pre-scan (cada cable quitado resta una vez su clave).
- **D4, lo que hacia la reconstruccion completa.** Enumera lo que hace `PostBulkRebuildAndPropagate`
  por la rama completa (`LFPG_NetworkManagerImpl.c:853-860`) y `m_Graph.PostBulkRebuild`
  (`scripts/5_Mission/LFPG_ElecGraphImpl.c:2009-2072`): estado de baterias, `RebuildFromWires`,
  `PopulateAllNodeElecStates`, `MarkSourcesDirty`, el reseteo de SyncVars de huerfanos y
  `m_GraphRebuildGeneration`. Para cada cosa, demuestra que tras un corte limpio el camino
  incremental la cubre (por ejemplo `OnWireRemoved` fuerza powered=false en el destino y
  `CleanupOrphanNode` llama a `ResetOrphanSyncVars`, `LFPG_ElecGraphImpl.c:897-1000` y la
  definicion de `CleanupOrphanNode`) o que no hace falta, igual que hoy en el camino del Splitter.
- **D5, fuente vanilla.** Lo mismo para un aparato sin almacen de cables (`:1027-1077`).

## PRUEBAS (lista blanca: un test nuevo y su paso en checks.yml)
Fichero nuevo `.github/tools/test_cutwires_rebuild_gate.py`, y su paso al final de
`.github/workflows/checks.yml` con el mismo formato que los demas.
- **T1, la decision real.** Extrae por contenido el bloque de rescate de `HandleCutWires` (desde
  `if (anyPortMissedIndex || (portCount > 0 && inPortCount == 0))` hasta su llave de cierre) y
  correlo con stubs: `RescueStaleIncomingWires` devuelve el valor del caso, `RebuildReverseIdx`
  cuenta llamadas, el aviso no hace nada. Si `enforce_scalar_slice` no admite el literal del aviso,
  sustituye solo esa llamada por un stub y dilo en el informe. Casos:
  - Positivos, que **fallan contra la base**: aparato sin IN (portCount=1, inPortCount=0, changed
    por cables de salida, rescued=0) -> 0 llamadas a `RebuildReverseIdx`; aparato con IN fallado
    (portCount=4, inPortCount=1, anyPortMissedIndex, changed, rescued=0) -> 0 llamadas.
  - Negativos, que pasan con la base y con el cambio: rescued=1 en las dos formas -> 1 llamada y
    changed=true; changed=false y rescued=0 -> 0 llamadas; puerto IN acertado por el indice
    (anyInRemovedByIndex, sin fallos) -> no entra en el bloque.
- **T2, estatico.** `HandleCutWires` tiene una sola llamada a `RebuildReverseIdx`, guardada por
  `rescued > 0`; la regla de `HandleCutPort` IN no cambia; el cuerpo de `RescueStaleIncomingWires`
  es identico al de la base; las llamadas a `LFPG_PerfProbe` de `HandleCutWires` no cambian.
- **T3, espejo (D1).** Slices de la condicion de salto del pre-scan (`:1003`) y de la de borrado de
  `ClearForCreator`, sobre todas las combinaciones de D1: tienen que borrar el mismo conjunto.
- **T4, recuperacion en el modelo.** Con `QueueModel` y los montajes de
  `.github/tools/test_graph_multifeed_split.py` (importalos, no los copies): para cada nodo con
  aristas de salida, quita todas sus salidas a la vez por el camino incremental, deja asentar la
  cola y compara con la reconstruccion sin esas aristas; despues reponlas todas y compara con la
  reconstruccion original. Mismo criterio de igualdad que la propiedad de la ronda 5 de i76.

## LISTA BLANCA (solo puedes escribir aqui)
- `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`: solo la condicion de `:1123` y el comentario
  de `:1121-1122`.
- `.github/tools/test_cutwires_rebuild_gate.py` (nuevo) y su paso en `.github/workflows/checks.yml`.
- `reviews/2026-10-10-grafo-grok/i78-r01/INFORME.md` (nuevo).

## Excluido
R-02, R-04 y R-05 se cierran con la medicion: no los toques. No quites ni condiciones el scan del
rescate (eso es R-04 y exige otras pruebas). No anadas una bandera de confianza del indice ni
reconstrucciones parciales. Nada fuera de la lista blanca.

## CRITERIO DE HECHO
1. Fuera de la lista blanca no cambia nada. En `LFPG_RPCServerHandlerImpl.c` cambian solo la
   condicion y su comentario.
2. T1: los positivos fallan contra la base y pasan con el cambio; los negativos pasan en los dos.
   T2, T3 y T4 pasan.
3. Todos los `.github/tools/test_*.py` con exit 0 y `enforce_checks.py --root .` con FAIL=0.
4. Linter offline: `errors` final == `errors` base; delta de warnings explicado.
5. INFORME con D1 a D5 respondidos con path:line y el protocolo en partida de abajo.

## PROTOCOLO EN PARTIDA (lo escribes en el INFORME como PENDIENTE-INGAME; no lo ejecutas)
Lo correra otro hilo con el MCP de DayZ. Escribelo con pasos, numeros esperados y donde leerlos:
- Build de medicion desde esta rama con `LFPG_PERF_PROBE = true`, solo en el servidor de pruebas.
- Redes con `fixture_grid` de unos 51 y unos 2031 nodos, como en la medicion de arriba. Cortes con
  los alicates reales: generador de una isla de 10 nodos, Splitter con cable de entrada y Splitter
  sin cable de entrada.
- Esperado en el generador y en el Splitter sin entrada, con 2031 nodos: `phase_rebuild_ms` del
  orden del Splitter (10 ms o menos, hoy 96), `settle_ticks` 2 o menos (hoy 49), ninguna linea
  `[CutWires-Fallback] Reverse index was stale`, y `phase_rescue_ms` anotado (es el scan, que se
  queda).
- Correccion: justo tras cortar, todos los aparatos de la isla sin corriente y el generador al 0 %
  en el inspector; una red aparte sigue igual; al reconectar vuelve al estado previo; tras
  reiniciar el servidor, el mismo estado.
- El negativo con el indice parcial no se puede montar en partida; dilo y remite a T1.

## Informe obligatorio
`reviews/2026-10-10-grafo-grok/i78-r01/INFORME.md`, en castellano. Contenido:
1. Tabla inicial: una fila por punto (D1-D5, cambio, T1-T4) con
   `punto | veredicto | ficheros:lineas | prueba`.
2. Una seccion `###` por punto con lo que comprobaste o cambiaste (`path:line` del codigo YA
   modificado) y como se verifica.
3. `## PROTOCOLO EN PARTIDA` (el de arriba, concreto).
4. `## GATES`: cada comando que corriste y su resultado literal (linter: errors/warnings base y
   final; cada test: exit code y numero de tests; la salida de T1 contra la base).
5. `## HALLAZGOS-ADYACENTES` (R20: lo que viste y no tocaste).
6. `## LO QUE NO PUDE VERIFICAR` — una linea por cosa.
7. `## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO` — obligatoria. Si crees que el arreglo
   correcto es otro, dilo aqui.

## Salida en el chat (solo esto, sin pegar codigo ni el informe)

## RECEIPT
```json
{
  "status": "ok|failed|stopped",
  "paths": ["rutas relativas de todo lo que creaste o modificaste"],
  "summary": "una linea",
  "gates": {"linter_errors_base": 0, "linter_errors_final": 0, "linter_warnings_base": 0, "linter_warnings_final": 0, "tests": "nombre: exit, ..."},
  "verified": ["lo que comprobaste tu"],
  "not_verified": ["lo que no pudiste comprobar"]
}
```
