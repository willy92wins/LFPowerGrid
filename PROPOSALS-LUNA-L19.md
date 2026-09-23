# Propuestas de reducción LOC — Luna L19

Ámbito de inspección: `scripts/4_World/LFPG_Furnace.c` completo (899 líneas antes del cambio). Se buscaron usos de los helpers por todo `scripts/` para detectar miembros muertos. Estimaciones son líneas físicas netas eliminables; no incluyen compactar/reformatear código sin borrar líneas.

| Orden | Oportunidad | LOC aprox. | Estado | Motivo / riesgo residual |
|---:|---|---:|---|---|
| 1 | Eliminar `LFPG_GetSwitchState()` (líneas 261–264). La búsqueda en scripts no encuentra llamadas dirigidas al horno; las llamadas homónimas son para generadores y test devices. | 6 | Aplicada | No hay dispatch por interfaz (`override` ausente). Riesgo bajo; referencias fuera de `scripts/` no fueron parte del barrido. |
| 2 | Contraer el retorno booleano de `LFPG_HasCargoItems()` a `return cargo.GetItemCount() > 0;` tras la guarda nula. | 2 | Aplicada | Conserva el resultado para cargo nulo y vacío/no vacío. |
| 3 | Quitar temporal `fuel` y retorno separado al final de `LFPG_CalcFuelWhitelist()`. | 1 | Aplicada | Sustituible por `return fuelPerUnit * qty;`; todos los caminos previos ya retornan. |
| 4 | Unir los dos bloques `#ifndef SERVER` adyacentes que declaran los miembros de sonido y humo. | 2 | Aplicada | Misma condición de compilación, sin código intermedio que cambie la protección. |
| 5 | Simplificar la asignación local `int fuel = 0` en `LFPG_CalcFuelRecursive()` y su asignación posterior. | 1 | Diferida | Reescritura neta de una línea pero declara variable junto al cálculo; ganancia marginal y menor legibilidad. |
| 6 | Cachear `item.GetInventory()` para reutilizarlo al contar/obtener attachments. | 0 | Diferida | Reduce llamadas textuales pero no LOC; el cambio podría alterar observaciones si el getter no fuera estable. |
| 7 | Compactar ramas de `LFPG_GetFuelConfig()` (array de tamaño, `canBeSplit`) o de cantidad de combustible. | 0–3 | Diferida | Las guardas preservan fallbacks ante configuración ausente o malformada. No es demostrable que sean redundantes. |
| 8 | Quitar comprobaciones de nulidad/rango en `LFPG_AutoConsumeLargestItem()` y utilidades de cargo. | 0–4 | Diferida | Protegen contra estado de inventario mutable/inválido; no son redundantes de forma demostrable. |
| 9 | Eliminar métodos de interfaz con cuerpo trivial/no-op (`LFPG_SetPowered`, getters de tipo/consumo/capacidad/source, `LFPG_GetOverloaded`, etc.). | potencial alto | Diferida | Cumplen contrato virtual de `LFPG_WireOwnerBase`; omitir overrides cambia dispatch o semántica heredada. |
| 10 | Eliminar/desduplicar rutas de lifecycle, persistencia, apagado, sincronización, calor y FX. | potencial alto | Diferida | Los hooks corresponden a eventos diferentes (init/killed/deleted/wires cut) y mantienen estado/efectos sincronizados. No se probó equivalencia entre ellos. |
| 11 | Eliminar helpers de fuel aparentemente locales (recursivo, whitelist, estimate, auto-consume, cache de configuración). | potencial alto | Diferida | Referencias comprobadas: `LFPG_ActionFeedFurnace.c`, `LFPG_DeviceInspector.c` y llamadas internas/recursivas del propio horno. |

Aplicadas: propuestas 1–4, total **11 líneas físicas**. Las demás se conservan por ser cambios de cero LOC, micro-optimizaciones o carecer de equivalencia demostrable con la inspección estática.
