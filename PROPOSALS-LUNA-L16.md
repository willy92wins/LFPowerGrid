# Propuestas LOC — Luna L16

Write-set revisado completo: `scripts/4_World/LFPG_HologramMod.c` (1.018 líneas iniciales).
Orden aproximado por líneas físicas eliminables; estimación conservadora. «Aplicada» significa
recorte mecánico con mismo valor/llamadas y mismo flujo. «Diferida» requiere decisión de diseño,
medición runtime o certeza contractual adicional.

| Orden | Recorte candidato | LOC aprox. | Estado / motivo |
|---|---|---:|---|
| 1 | Consolidar los tres bloques `#ifndef SERVER` adyacentes de constantes de ajuste en un bloque. | 24 | Diferida: cambia guardas de preprocesador aunque la región actualmente agrupa solo constantes client-side; verificar expansión para SERVER antes. |
| 2 | Consolidar los grupos de campos `#ifndef SERVER` adyacentes (estado de suavizado, hysteresis y caché) en bloques envolventes. | 18 | Diferida: mismo motivo; verificar compilación de cada lado. |
| 3 | Eliminar comentarios históricos/explicativos redundantes y cabeceras de sección repetidas, preservando comentarios que documenten invariantes o fixes. | 15–35 | Diferida: comentarios no afectan comportamiento, pero la selección es editorial y parte conserva contexto de fixes/API. |
| 4 | Eliminar `LFPG_IsDifferentModelKit()` y sus dos envoltorios de llamada: los dos sitios llaman después a `LFPG_ApplyDiffModelPosOffset`, que ya hace guardia/cast y devuelve `pos` si no aplica. | 8–12 | Diferida: elimina una comprobación/cast redundante en el caso no deployable, pero invoca el helper también para kits same-model; resultado actual equivale, impacto de rendimiento no medido. |
| 5 | Eliminar `LFPG_ApplyDiffModelPosOffset()` y sumar el offset inline en floor/wall fallback solo tras cast válido. | 6–10 | Diferida: duplicaría cast/control en dos ramas o exigiría refactor; no es una eliminación neta clara. |
| 6 | Reemplazar `noHitPoint` por `rayEnd` al llamar `LFPG_GroundSnap`: ambos son exactamente `camPos + camDir * LFPG_HOLO_MAX_RANGE`. | 2 | Aplicada. |
| 7 | Quitar temporales de `GetDefaultOrientation` (`baseOri`, `oriOff`, `oriResult`) y retornar la suma directa. | 3 | Aplicada. |
| 8 | Quitar temporal `depOff` y retornar `pos + deployKit.GetDeployPositionOffset()`; mantener el cast/guardia. | 2 | Aplicada. |
| 9 | Quitar temporal `cachedPose`, retornando el `Vector(...)` directamente. | 2 | Aplicada. |
| 10 | Quitar temporales `bNoCollide` en dos sitios; pasar `false` directamente a `SetIsColliding`. | 2 | Aplicada. |
| 11 | Quitar temporales `depFinal`/`depOut` en offsets de posición (cada retorno/SetPosition usa suma directa). | 2 | Aplicada. |
| 12 | Quitar temporal `rayEnd` y construir el extremo directamente en la llamada RaycastRV. | 1 | Diferida: mismo valor, pero el nombre aclara la geometría y se usa también en propuesta 6; mantenerlo permite reutilizar el punto sin duplicar expresión. |
| 13 | Cambiar `if (deployKit) return true; return false;` a `return deployKit != null;` en `LFPG_IsDifferentModelKit`. | 2 | Diferida: estilo de booleano y API/semántica null no verificada aquí; además está contemplada su eliminación en propuesta 4. |
| 14 | Compactar wrappers de getters cacheados (11 helpers con la misma forma condicional). | ~22 | Diferida: sustitución por `?:` prohibida por convención; extraer una abstracción genérica arriesga compatibilidad y no ofrece corte seguro. |
| 15 | Quitar asignación `isLFPGKit` y usar la condición directa en `UpdateHologram`. | 1 | Diferida: ahorro mínimo, el nombre aporta claridad al gate de fallback y la variable no está muerta. |
| 16 | Quitar `ceilThreshold` y usar `floorThreshold` en clasificación/diagnóstico de techo. | ~5 | Diferida: actualmente se inicializan y actualizan simétricamente, pero son umbrales de dominios distintos y el nombre conserva intención; consolidación futura depende de no divergencia contractual. |
| 17 | Quitar `negCeilThreshold` y comparar contra `-ceilThreshold` directamente. | 1 | Diferida: el nombre hace legible el límite negativo y no hay reducción material. |
| 18 | Quitar `cacheFresh`/`cacheMoveSq` y expresar sus fórmulas dentro del `if`. | 2 | Diferida: empeora legibilidad de una condición de caché sensible; no recorta lógica. |
| 19 | Quitar `normalHorizLenSq` manteniendo la raíz cuadrada en la condición. | 1 | Diferida: conserva LOC y empeora inspección numérica. |
| 20 | Quitar temporales de raycasts (`rayResults`, `rayWith`, flags/radio y equivalentes de ground ray) inlineando argumentos. | ~10 | Diferida: la firma posicional de RaycastRV es larga; aumenta riesgo de permutar filtros y no elimina comportamiento. |
| 21 | Quitar temporales de orientación/interpolación y construir vectores inline. | ~8 | Diferida: cambios extensos en matemática/flujo sensible; neto modesto y menos revisable. |
| 22 | Reducir comprobaciones de puntero en proyección/parent/kits. | variable | Diferida: guards protegen fallbacks vanilla, creación de holograma y casts; no hay evidencia de redundancia contractual. |
| 23 | Quitar `LFPG_IsLFPGKitProjection`, wrappers de overrides, getters públicos/virtuales o estado de caché aparentemente «pequeño». | variable | No propuesto para aplicación: llamadas fuera de archivo y despacho virtual verificados por búsquedas; son contratos activos, no código muerto. |
| 24 | Unificar cache writes de éxito/fallback en `LFPG_GroundSnap`. | ~5 | Diferida: exige helper o reorganizar retornos y puede alterar orden/validez de la caché; extracción no tiene ganancia neta clara. |
| 25 | Quitar `m_LFPG_IsWallMode`, `m_LFPG_WasWallMode`, logging de transición o doble `SetOrientation`. | variable | No seguro: estado consultable externamente, hysteresis, diagnóstico y workaround físico explícitos. |

No se encontró helper privado sin uso en el alcance: los helpers se llaman en este archivo;
`LFPG_IsWallMode()` es API observada en búsquedas del árbol. No hay miembros temporales sin uso
obvios. No se aplican cambios de algoritmos, umbrales, filtros, orden de raycasts ni estado.
