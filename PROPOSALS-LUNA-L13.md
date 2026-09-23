# Propuestas de reducción LOC — Luna L13

Alcance inspeccionado: `scripts/3_Game/LFPG_FileUtil.c` (1.171 líneas antes del cambio). Las referencias a helpers se comprobaron en el árbol del proyecto.

Ordenadas por LOC potenciales ahorradas (estimación bruta; comentarios/código quedan identificados):

| Orden | Candidato | LOC aprox. | Estado | Motivo / límite |
|---|---|---:|---|---|
| 1 | Eliminar/reducir comentarios de cabecera y pasos de guardado/recuperación (líneas 1–37, 69–72, 80, 93, 109, 116–119, 137, 140, 259–261, 307–312, 321–322, 375, 435–441, 453–454, 473–489, 509–511, 540–542, 570–571, 581–582, 625–626, 749, 888, 910, 942, 1059, 1083–1085, 1098, 1111–1113, 1128–1132). | DIFERIDO | Gran ahorro bruto, pero documentan ventanas de crash, invariantes de recuperación, restricciones del lenguaje y operativa. Recortarlos dificultaría mantenimiento; revisar individualmente si el objetivo cuenta documentación. |
| 2 | Unificar promoción/recuperación de `.tmp` tipados entre wires/settings/balances. | ~45 | DIFERIDO | La similitud oculta diferencias: balances usa marcador de vuelo, barrera de versión futura y rollback de créditos; genéricos tipados no son aceptables aquí. |
| 3 | Reducir `ScanRawJsonVersion` (máquina de estados, escape/BOM, claves truncadas y saturación). | ~30 | DIFERIDO | Parser acotado que sostiene la barrera de solo lectura. Acortarlo puede alterar JSON/UTF-8/truncamiento; equivalencia no demostrada. |
| 4 | Compactar lectura/validación de `TryReadSellDestroyIntent` y validadores de enteros/UID. | ~15 | DIFERIDO | Los checks distinguen campos durable, límites UID e enteros no negativos; fusionar requiere demostrar semántica de FGets/Trim/ToInt. |
| 5 | Eliminar `EnsureFileOrRestore` (fallback sin validar), sin llamadas cualificadas halladas en scripts. | 26 | DIFERIDO | API marcada como compatibilidad; revisar consumidores externos/mods antes de eliminar. |
| 6 | Simplificar `SweepPreservedBalanceTmpEvidence` (bucle/flags y rutas de limpieza). | ~5 | DIFERIDO | `FindNextFile`, límite 32 y construcción de path tienen efectos observables; equivalencia del iterador Enforce no verificada. |
| 7 | Reemplazar auxiliar `hasBackup` por condición directa en el guard de vuelo. | 4 | APLICADO | Booleano temporal usado una vez; mantener orden de `FileExist`/short-circuit. |
| 8 | Reducir duplicación de respaldo en fallos de promoción de los tres AtomicSave. | ~8 | DIFERIDO | Logs, fault injection y limpieza difieren; extraer helper podría cambiar cobertura o trazas. |
| 9 | Eliminar variables temporales obvias (p. ej. `found` en sweep) y retornos/asignaciones redundantes. | ~1–3 | DIFERIDO | Ahorro marginal; conservar iteración explícita sin semántica verificada de `FindNextFile`. |

No se identificaron helpers locales definitivamente muertos: los helpers, salvo `EnsureFileOrRestore`, aparecen usados dentro del archivo o desde otros scripts. La ausencia de llamada interna no basta para borrar símbolos.
