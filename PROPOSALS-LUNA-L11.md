# Propuestas de reducción LOC — Luna L11

Archivo revisado completo antes de editar: `scripts/4_World/LFPG_TestDevices.c` (1.447 líneas físicas).

## Inventario ordenado por LOC estimadas

| Orden | Candidato | LOC aprox. | Estado | Motivo / riesgo |
|---:|---|---:|---|---|
| 1 | Retirar comentarios del historial v0.7.x–v0.9.4 y explicaciones de auditoría que repiten el flujo actual. | ~120–180 | Parcial | Los comentarios cronológicos ocupan bastante, pero varios guardan causa y restricciones operativas (JIP/CompEM, persistencia, IDs). Se conservan esos detalles; se retiraron solo redundancias claras. |
| 2 | Quitar el `#ifndef SERVER` anidado en `LF_TestLamp.OnVariablesSynchronized`, que ya está completamente dentro del mismo guard exterior. | 2 | Aplicado | Preprocesamiento idéntico; se conservó el guard exterior. El equivalente en Generator permanece diferido: su bloque contiene lógica de CompEM y no se recortó durante esta pasada conservadora. |
| 3 | Eliminar `bool bEnable = true` y pasar el literal directamente a `SetEnabled`. | 1 neta | Aplicado | Valor fijo local sin efectos, misma llamada y orden. |
| 4 | Quitar comentarios/notas de estado derivado repetidas junto a persistencia y el comentario que narra el no-op del generador. | 5 líneas | Aplicado | Código/contrato no cambia. |
| 5 | Eliminar overrides vacíos (`LFPG_SetPowered` del generador y hooks CompEM de LF_TestLamp). | ~14 | Diferido | No es seguro: el despacho dinámico llama a `LFPG_SetPowered`; los hooks vacíos suprimen el comportamiento heredado de Spotlight. |
| 6 | Eliminar métodos de valor constante/delegación (`LFPG_GetSourceOn`, `LFPG_IsPowered`, `LFPG_GetPortCount`, etc.). | ~20 | Diferido | Contratos consultados en `LFPG_IDevice.c`, `lfpg_devicebase.c` y consumidores: borrar cambia interfaz o dispatch/semántica. |
| 7 | Quitar guardas de ID vacío, borrado, null y comparaciones antes de sincronizar. | ~20 | Diferido | Previenen llamadas inválidas, re-registro post-mortem, efectos duplicados o SetSynchDirty innecesario. No se demostró que sobren. |
| 8 | Eliminar snapshot profundo JSON y usar solo invalidación de cache por mutadores. | ~70 | Diferido | `LFPG_GetWires()` expone array/entradas mutables; referencias retenidas permiten mutaciones sin pasar por mutadores. Snapshot cubre ese caso. |
| 9 | Quitar validación de puertos, lecturas de persistencia, verificación de SparkPlug, guardas SERVER/CLIENT o hooks `super`. | ~35+ | Diferido | Cargan contrato, protección del grafo, compatibilidad del guardado, hooks del engine y sincronización JIP. |
| 10 | Colapsar `LF_TestLampHeavy` o quitarlo. | ~9 | Diferido | Es una clase de prueba distinta y su override de consumo define su comportamiento. |

## Aplicado

- Candidatos 2, 3 y la parte inequívoca del 4.
- 9 líneas netas menos en el fuente (1.447 → 1.438 líneas físicas).

## Diferido

El resto de los candidatos 1 y 5–10, por los riesgos indicados. Los métodos públicos aparentes tienen usos en el árbol; no encontré helpers huérfanos demostrables.
