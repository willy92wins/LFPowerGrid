# T5 harness — uso para el orquestador

Instrumento de inyeccion de fallos para las fichas T1. No declara fichas
cerradas. No incluye resultados. Cada fila de `cases.json` es un experimento
vacio: tu lo corres en un servidor y el driver lee el script log.

## 0. El gate de produccion

`LFPG_FAULTINJECT_COMPILE_GATE` es `true` solo bajo `#ifdef DIAG` (DayZDiag /
Workbench). En `DayZServer_x64` retail el compilador deja el gate en `false`:
**desplegar este arbol tal cual en produccion no hace nada**, aunque alguien
copie `LF_FaultInject.json` al profile.

Si tu puente MCP arranca un dedicado retail (sin `DIAG`), cambia **solo** la
rama `else` de `scripts/3_Game/LFPG_FaultInject.c` a `true` en el artefacto de
prueba. No lo envies a produccion.

El JSON de settings del ATM (`LF_BTCAtm.json`) **no** arma esto. Un fichero
hermano que el mod **nunca crea solo**:

    $profile:LF_PowerGrid/LF_FaultInject.json

## 1. Secuencia por caso (literal)

1. Parar el servidor. No hace falta recompilar entre casos.
2. Copiar `harness/scenarios/<id>.json` sobre
   `$profile:LF_PowerGrid/LF_FaultInject.json`.
3. Preparar el mundo como dice la fila en `cases.json` / `REPORT.md`
   (billetera, ATM, `LF_BTCAtm.json` / `LF_Balances.json` / claims).
4. Arrancar el servidor. En el script log tiene que aparecer
   `LFPG_FAULTINJECT ARMED scenario=<id>` **antes** de la transaccion.
   Si aparece `DISARMED` o no aparece nada: el gate esta apagado o el JSON
   no se leyo. El driver lo marca INCONCLUSIVE, no PASA.
5. Disparar la accion (RPC de ATM, o el boot si el caso es de recuperacion).
6. Si el caso es `crash`: buscar `LFPG_FAULTINJECT CRASH_NOW`. Si el proceso
   sigue vivo, **SIGKILL**. No uses el shutdown limpio: `OnMissionFinish`
   llama `FlushBalanceOnShutdown` y cierra la ventana.
7. Si `restart` es true: borrar o desarmar el JSON (`enabled: false` o
   quitar el fichero) **antes** del segundo boot, salvo que el caso pida
   lo contrario. Arrancar de nuevo. Recoger el log del segundo boot.
8. Correr el driver contra el log (o los dos logs):

       python harness/t5_fault_driver.py --case <id> --log <boot1.rpt> [--log <boot2.rpt>]

9. El driver imprime PASS / FAIL / INCONCLUSIVE. Tu rellenas la matriz.
   Este arbol no trae esa columna rellena.

## 2. Frase de armado

Campo `armPhrase` obligatorio, exacto:

    I_UNDERSTAND_THIS_DESTROYS_MONEY

`remaining` es el numero de ganchos que pueden dispararse este boot.
Tras agotarlo, un `AddBalance` de rollback ya no esta envenenado.

## 3. Que no hace este README

No lanza DayZ. No empaqueta PBO. No afirma que una ficha este cerrada.
