# Brief Grok: el inspector marca SOBRECARGA a carga justa y no la marca en una sobrecarga real

Base: origin/main en aa96d35 (merge de #84). Rama: grok/inspector-sobrecarga.
Ficheros que se pueden tocar: scripts/4_World/LFPG_DeviceInspector.c, un test nuevo .github/tools/test_inspector_source_overload.py y una línea en .github/workflows/checks.yml. Nada más (R20: lo adyacente va a la sección "Fuera de alcance" del informe, no se arregla).

## Bug
En el inspector, una FUENTE encendida (generador, panel solar, horno) muestra el estado así (LFPG_DeviceInspector.c:696-716):
  float loadRatio = m_SnapshotFloatA;   // LFPG_DeviceAPI.GetLoadRatio(device), :536
  if (loadRatio >= LFPG_LOAD_CRITICAL_THRESHOLD)  // 1.00 -> SOBRECARGA (rojo) + barra + %
  else -> ACTIVO (verde) + barra + %
Dos fallos:
1. A carga exactamente igual a la capacidad (ratio 1.00) pone SOBRECARGA, pero el grafo no está en sobrecarga: `overloaded` solo es true si totalHardDemand > availableOutput + LFPG_PROPAGATION_EPSILON (scripts/5_Mission/LFPG_ElecGraphImpl.c:4735).
2. En una sobrecarga real el grafo asigna 0 a todas las salidas (:4763, all-off), así que m_LoadRatio = totalAllocated / capacity = 0 y el inspector pone ACTIVO 0 %.
La señal correcta es el flag m_Overloaded del nodo, que el servidor ya sincroniza en la entidad (LFPG_DeviceAPI.SetOverloaded, ElecGraphImpl.c:3188) y que las tres fuentes registran como SyncVar (LFPG_Generator en LFPG_TestDevices.c:108, LFPG_SolarPanel.c:80, LFPG_Furnace.c). En cliente se lee con LFPG_DeviceAPI.GetOverloaded(EntityAI) (LFPG_IDevice.c:818, #ifndef SERVER, el mismo contexto que GetLoadRatio; todo el inspector está bajo #ifndef SERVER).

## Cambio pedido (mínimo)
1. Snapshot: nuevo miembro `protected bool m_SnapshotBoolD;` junto a m_SnapshotBoolC (:124). En el builder (:520-600): `bool boolD = false;`; en la rama deviceType == LFPG_DeviceType.SOURCE (:533) `boolD = LFPG_DeviceAPI.GetOverloaded(device);`; añadir `boolD != m_SnapshotBoolD` a `changed` y `m_SnapshotBoolD = boolD;`. No reutilizar boolB/boolC: el horno pisa boolB (:569) y la batería boolC.
2. Render de la FUENTE encendida: m_SnapshotBoolD true -> #STR_LFPG_INSPECT_OVERLOAD en COL_RED_ERROR, SIN barra ni % (el ratio vale 0 en sobrecarga y "0 %" contradice la etiqueta). false -> #STR_LFPG_INSPECT_ACTIVE en COL_GREEN_OK + barra + % como ahora (a carga justa: ACTIVO 100 %). La rama deja de comparar loadRatio con LFPG_LOAD_CRITICAL_THRESHOLD.
3. No tocar LFPG_LOAD_CRITICAL_THRESHOLD ni su otro uso (telemetría en ElecGraphImpl.c:3175).
Convenciones (AGENTS.md): sin `? :`, `++`, `--`, `+=`, `foreach`, `Print(`; `ref` solo en miembros de clase; respetar la indentación mezclada de las líneas vecinas; no reformatear.

## Criterios verificables (R26)
Test .github/tools/test_inspector_source_overload.py, corte offline del fuente como los otros test_*.py de esa carpeta, exit 0 si pasa y 1 si falla, solo stdlib:
- P1: en el builder, la rama SOURCE asigna GetOverloaded(device) a una variable que acaba en m_SnapshotBoolD, y m_SnapshotBoolD entra en la expresión `changed`.
- P2: en el render de SOURCE, la etiqueta OVERLOAD depende de m_SnapshotBoolD y la rama ACTIVE sigue construyendo BuildLoadBar y el %.
- N1: el render de SOURCE ya no contiene `loadRatio >= LFPG_LOAD_CRITICAL_THRESHOLD`.
- M1 (mutante): el test se ejecuta también contra la versión vieja del bloque, fijada como literal dentro del test (copiada de aa96d35), y debe fallar con ella. Prohibido leer git show HEAD: o cualquier historia de git.
Añadir el paso al final de .github/workflows/checks.yml con el mismo estilo que los demás.
Gates antes del push: linter KP len(errors)==0; enforce_checks.py FAIL=0; todos los test_*.py en verde; Gate A EOL; Gate B (tests otra vez tras el commit).
Prueba en partida (la hace otro hilo, no Grok): generador con consumo igual a su capacidad -> ACTIVO 100 %; un consumidor más -> SOBRECARGA en rojo sin %, cables en rojo; quitarlo -> ACTIVO.

## Informe
INFORME.md en reviews/2026-10-17-release-130/inspector/ con diff resumido, salida de los gates y "Fuera de alcance" con lo adyacente visto (p. ej. la etiqueta OVERLOADED de la telemetría en ElecGraphImpl.c:3175 usa el mismo umbral >=).

## Ejecución (añadido por el orquestador, no forma parte del encargo funcional)
- Trabajas en este worktree (C:/tmp/lfpg-grok/inspector-sobrecarga). Escribe los ficheros tú; NO hagas git commit/push/checkout/reset/stash: el commit lo hace el orquestador.
- Finales de línea: el índice es LF y el working tree está en CRLF. Conserva el final de línea que ya tiene cada fichero que edites; el test nuevo en LF.
- Gates que puedes correr tú: `python C:/Users/guill/DayZ-Modding-Knowledge-Pack/tools/dayz-script-validator/scripts/script_validator.py .` (cuenta len(errors), no mires status), `python .github/tools/enforce_checks.py --root .`, y cada `python .github/tools/test_*.py`.
- NO lances DayZ, ni el MCP, ni toques P:\.
- Recibo por stdout: lista de ficheros tocados, salida resumida de los gates y lo que NO pudiste verificar.
