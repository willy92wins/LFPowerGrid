# Propuestas de reduccion LOC — Luna L18

Base: `scripts/4_World/LFPG_DoorController.c` en `033c08c986987c50c5983d37811b0222f4cb3d05` (899 lineas fisicas; CRLF). Las estimaciones son lineas fisicas netas aproximadas.

Ordenadas por LOC estimadas, de mayor a menor:

| Estado | Recorte candidato | Ahorro aprox. | Evidencia / motivo |
|---|---|---:|---|
| APLICADO | Quitar ambos bloques `#ifndef SERVER` de diagnostico de pairing/retry dentro de `LFPG_DoSearchAndPair`, ya delimitada por `#ifdef SERVER`; retirar los dos contadores/temporizadores y sus escrituras que solo alimentaban esos bloques | 32 | `LFPG_DoSearchAndPair` 523-722; variables/actualizaciones `m_SearchWakeMs` y `m_SearchAttempts` solo servian a esos diagnosticos. Preprocesador hace inalcanzable el codigo de diag. |
| APLICADO | Reducir un nivel de anidamiento en las ramas de Fence de apertura/cierre (`if (f) { if (estado) ... }`) con guard combinado | 6 | `LFPG_ForceOpenDoor` y `LFPG_ForceCloseDoor`; mismo cast/null y mismo estado antes del mismo efecto. |
| DIFERIDO | Reducir anidamiento en las ramas de Building de apertura/cierre y de lock/unlock con condiciones compuestas | 4-8 | `LFPG_ForceOpenDoor`, `LFPG_ForceCloseDoor`, `LFPG_EnsureDoorLocked`; requiere cambiar varias condiciones y no es tan mecanico como el recorte aplicado. |
| DIFERIDO | Consolidar inicializacion repetida del estado de busqueda normal y con hint en un helper comun | 3-7 netas | `LFPG_SearchAndPairDoor` y `LFPG_SearchAndPairDoorWithHint`; posible ahorro tras descontar firma/llamada, pero aumenta flujo compartido y merece verificar el ciclo de CallLater antes de reestructurar. |
| DIFERIDO | Eliminar rama `else` en `LFPG_IsPairedDoorOpen` tras guards tempranos por tipo | 2-4 | Varias ramas terminan en `return false`; solo forma, ahorro pequeno y riesgo de una reescritura menos legible. |
| DIFERIDO | Unificar guards anidados sencillos en `LFPG_UnpairDoor`, `LFPG_ApplyDoorState` / puerta y `LFPG_EnsureDoorLocked` | 2-6 | Recortes de llaves/lineas, cada uno pequeno; requiere evaluar claridad y macros de servidor. |
| DIFERIDO | Retirar comentarios de seccion y comentarios que repiten literalmente la siguiente llamada/condicion | 0-40 | Encabezados de seccion a lo largo del archivo. No se aplico: LOC textual no equivale a codigo redundante y algunos comentarios documentan restricciones/contrato. |
| DIFERIDO | Compactar concatenaciones/mensajes de logging | 0-15 | Mensajes de debug/info en varios metodos. Strings y logs son observables; sin evidencia de que sean redundantes no es un corte seguro. |
| DESCARTADO | Quitar `LFPG_EnsureDoorLocked` como helper aparentemente pequeno | 0 | Tiene caller real en `LFPG_ApplyDoorState` (linea 462); no esta sin uso. |
| DESCARTADO | Quitar los metodos de pairing, callbacks lifecycle, persistencia, SyncVars, busqueda de jugadores u operaciones de puertas por parecer auxiliares | 0 | Todos tienen caller/override o producen comportamiento requerido; barrido de referencias no encontro helper sin uso demostrable. |

Resultado aplicado: 899 -> 861 lineas fisicas (-38).

No se encontro otro campo, helper, rama ni variable local que pueda borrarse como codigo muerto con evidencia suficiente. No se tocaron API, persistencia, busqueda, logs funcionales ni callbacks.
