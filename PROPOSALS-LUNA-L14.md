# Propuestas LOC — Luna L14

Inventario previo a edición de `scripts/4_World/LFPG_Intercom.c`. Estimación por líneas físicas eliminables; los cortes se consideran solo cuando preservan el comportamiento compilado observable.

| Orden | Propuesta | LOC estimadas | Estado | Motivo / riesgo |
|---:|---|---:|---|---|
| 1 | Quitar los tres bloques `#ifndef SERVER` de telemetría `LFPG_PERFDIAG` dentro de métodos ya delimitados por `#ifdef SERVER`, más el campo contador asociado que solo usan esos bloques. | 37 | Aplicada | Código cliente inalcanzable por preprocesador en cada método; contador sin otros usos. Sin efecto en binario servidor ni cliente. |
| 2 | Inlinear alias locales de un solo uso (puertos y SyncVars del constructor, nombres de puerto/slot, errores de lectura, port de edge) y colapsar el guard anidado de `CanReleaseAttachment`. | 27 | Aplicada | Argumentos y orden de llamadas conservados; `typeName` también se eliminó tras inlinear su único uso. |
| 3 | En `LFPG_ToggleIntercom` y `LFPG_ToggleBroadcast`, colapsar los `if/else` de inversión booleana en asignación negada. | 12 | Aplicada | Misma inversión bool; se mantiene el resto de orden y efectos laterales. |
| 4 | En `LFPG_UpdateGhostRadio` y `LFPG_UpdateGhostPAS`, sustituir el booleano temporal y su asignación condicional por condiciones equivalentes en las ramas de spawn/despawn. | 10 | Aplicada | El spawn requiere condición verdadera y ghost nulo; el despawn requiere condición falsa y ghost presente. |
| 5 | En `LFPG_UpdateVisuals`, eliminar locales que solo replican el literal de animación y strings vacíos para textura/material. | 4 | Aplicada | Argumentos idénticos, sin cambio de valor ni de orden de llamadas. Se conservan las ramas de animación. |
| 6 | Simplificar asignación de `currentInput` en `LFPG_EvaluateToggleInput` a comparación directa. | 4 | Aplicada | Resultado bool idéntico a inicializar `false` y asignar `true` al cumplirse el umbral. |
| 7 | Quitar constantes no referenciadas: `LFPG_INTERCOM_HS_CAMO`, `LFPG_INTERCOM_HS_SCREEN` y `LFPG_SND_NONE`. | 3 | Aplicada | Búsqueda de símbolos en `scripts/` confirma que solo se declaran aquí; quitar declaraciones no afecta ejecución. |
| 8 | Simplificar `freqPhase` eliminando su guard `m_FrequencyIndex > 0`. | 0 | Rechazada | El campo SyncVar puede recibir estado fuera de rango; persistencia y ciclo lo limitan en flujos conocidos, pero no se ha probado que la guarda sea redundante ante estado de red. |
| 9 | Eliminar mensajes de log y ensamblajes de strings detallados (SetPowered, persistence, RF, lifecycle, toggles, frecuencia). | ~80-100 | Rechazada | Los logs son salida observable y útiles para diagnóstico; quitarlos no es estrictamente behavior-preserving. No se tocan. |
| 10 | Eliminar accesores/helpers por posible falta de uso local (`LFPG_GetSwitchOn`, `LFPG_GetRadioInstalled`, `LFPG_GetBroadcastEnabled`, `LFPG_GetFrequencyIndex`, `LFPG_GetLastRFToggleTime`, `LFPG_InstallRadio`, `LFPG_CycleFrequency`, y ciclo de vida de ghosts). | 0 | Rechazada | Búsqueda en todo `scripts/` encuentra consumidores en acciones y NetworkManager; helpers de ghost tienen llamadas internas. No son código muerto. |
| 11 | Eliminar guards de null en entradas, edges, entidades/ghosts o `MemoryPointExists`. | variable | Rechazada | Validaciones protegen llamadas sobre referencias opcionales, datos externos o geometría no garantizada; no hay prueba de redundancia. |
| 12 | Eliminar flujo de reparación de frecuencia persistida fuera de rango o validación de versión/lecturas. | variable | Rechazada | Protege compatibilidad/corrupción de persistencia y lectura truncada; cambiarlo afecta recuperación de mundo. |
| 13 | Eliminar campos de cache visual o guard de `visualsUnchanged`. | variable | Rechazada | Evita repetir mutaciones visuales y compara todos los estados que alteran LEDs, animación o micrófono. No hay prueba de que sea redundante. |
| 14 | Eliminar código aparentemente duplicado entre lifecycle del GhostRadio y GhostPAS. | variable | Rechazada | Los tipos y las operaciones (frecuencia/VOIP frente a PAS) difieren; no existe helper común con reducción neta segura sin rediseño. |

## Cobertura de búsqueda

Se leyó completo el write-set antes de crear esta lista. Las cifras por propuesta son estimaciones de delta local; LOC medidas del archivo: 1047 antes, 946 después (-101). Se buscaron usos del API y constantes candidatas por todo `scripts/` y `config.cpp`. El `HANDOFF.md` prescrito por `AGENTS.md` no está disponible en la ruta hermana de este worktree; solo aparece un handoff antiguo en `reviews/2026-09-08-council-plan-definitivo/fuentes/`, que no se asumió como estado actual.
