<!-- Salida literal de Codex (codex exec, solo lectura), revision del commit fd87f9f. Transcrita por el receptor sin editar. -->

BASE: claude/project-thread-0iolln @ fd87f9f07bb6baee776f5e5a430fc4b894774617

**VEREDICTO: SOUND-with-fixes.** No encontré una desviación funcional del contrato v3 en producción. Sí demostré falsos verdes en el arnés y una limitación del oráculo exigido. Sin P1. Revisión íntegramente en solo lectura, sin subagentes ni arranque de DayZ.

## Hallazgos

| ID | Nivel | path:line | Secuencia exacta | Salida mala | Arreglo mínimo |
|---|---|---|---|---|---|
| QC-01 | P2 | [.github/tools/test_memorycell_toggle.py:430](/C:/tmp/lfpg-grok/wt-nostra-131/.github/tools/test_memorycell_toggle.py:430) | Mutación **solo en memoria**: mover Set/Reset antes del bloque que consume `m_FlipPending`. Ejecutar F10 con Set=1, Reset=0 y lecturas `(0,1,0) (5000,1,1) (7000,1,0)`. | El mutante queda **OFF entre 5400 y 7000 ms**, incumpliendo R5, pero las **10 pruebas pasan** porque F10 solo comprueba el latch final. Eliminar completamente Set/Reset también deja las 10 verdes. **El fuente actual sí tiene el orden correcto.** | Comprobar el latch tras cada paso, especialmente en 0 y 5400 ms; añadir el mutante de orden. Cubrir también Reset y Set+Reset simultáneos. |
| QC-02 | P2 | [.github/tools/test_memorycell_toggle.py:109](/C:/tmp/lfpg-grok/wt-nostra-131/.github/tools/test_memorycell_toggle.py:109) | Suprimir en memoria únicamente el `Remove(LFPG_MCellToggleSettleTimer)` del rearme. Leer `(0,1,0)` y `(100,1,1)`. | `CallLater` sobrescribe automáticamente la entrada anterior del diccionario: quedan P@400 y T@500, exactamente como con el código correcto. Las **10 pruebas pasan**. Una cola que conserva ambos registros muestra además T@400: el arnés está realizando la cancelación que debía demostrar mediante `Remove`. | Representar registros separados en la cola y cancelar explícitamente por callback. Añadir mutantes que eliminen cada `Remove` de rearme y comprobar cantidad y vencimiento de registros. |
| QC-03 | P3 | [.github/tools/test_memorycell_toggle.py:288](/C:/tmp/lfpg-grok/wt-nostra-131/.github/tools/test_memorycell_toggle.py:288) | Inspeccionar `oracle`: mantiene otra vez `raw`, `settled`, instantes candidatos y `due`; ejecuta `apply` antes de sustituir lecturas, reproduciendo el método productivo. | Es independiente del texto del fuente, pero **no del método**, ni construye los tramos pedidos por v3. No reproduje una discrepancia funcional causada por esta dependencia; por eso queda en P3. | Construir primero los intervalos constantes de cada entrada, aceptar los de duración ≥400 ms y después recorrer sus cambios por instante, aplicando R1–R4. |

## Contraste del código con v3

- **Vaciado e instantes:** [`LFPG_SetPowered:211`](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:211) ejecuta `LFPG_MCellApplyDue` antes de cualquier escritura de `m_Raw*`. Cada entrada actualiza su propio instante y su propio temporizador. [`LFPG_MCellApplyDue:317`](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:317) compara ambos instantes y usa Power primero únicamente en el empate. El límite de exactamente 400 ms cuenta.
- **Flip efectivo y R5:** [`LFPG_MCellSettle:394`](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:394) aplica vencidos y, si hay flip, llama a `LFPG_SetPowered(m_PoweredNet)`. Allí se invierte realmente `m_LatchState`, se limpia el pendiente y **después** se aplican Set/Reset. No queda el flip únicamente en una variable auxiliar.
- **R3 y R4:** el predicado y `LFPG_OnStoreLoadDevice` permanecen idénticos a la base. La carga fija la gracia de 30000 ms. Los valores iniciales Toggle crudo/asentado=true y `m_ToggleLowSinceMs=-1` impiden disparar sin un bajo asentado previo.
- **Resto de `LFPG_SetPowered`:** comprobé igualdad con el padre desde Set/Reset hasta el final: estado activo, enrutado, diferido y sincronización permanecen intactos.

Los añadidos [`LFPG_MCellCommitPower:340`](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:340) y [`LFPG_MCellCommitToggle:355`](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:355) son una extracción equivalente de los pasos del brief. Conservan los instantes originales; Toggle evalúa R1–R4 con `m_RawToggleChangedMs`. La guarda adicional `!m_SettledToggle` es redundante bajo `dueT`, pero correcta. No encontré efectos añadidos que alteren el contrato.

## Enforce y borrado

Las llamadas encajan con las declaraciones reales `CallLater(func fn, int delay, bool repeat, …)` y `Remove(func fn)`, verificadas en el árbol vanilla local, `scripts/2_gamelib/tools.c:61,65`, y en el [fuente oficial de Bohemia](https://github.com/BohemiaInteractive/DayZ-Script-Diff/blob/main/scripts/2_gamelib/tools.c#L61).

Los callbacks registrados son métodos públicos `void` sin parámetros. Los helpers y miembros protegidos se utilizan desde métodos de la propia clase; no hay un acceso externo introducido por `CallLater`. No encontré locales duplicadas ni referencias a miembros inexistentes. La llamada a `LFPG_SetPowered` desde `LFPG_MCellSettle` está dentro de `#ifdef SERVER`: en cliente no se ejecuta.

[`LFPG_OnDeleted:458`](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:458) cancela los dos temporizadores nuevos. El hook está conectado al borrado real mediante [`LFPG_DeviceBase.EEDelete:298`](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/lfpg_devicebase.c:298). Con ambos pendientes, la ejecución del hook en el arnés deja la cola vacía.

**Deuda aceptada, excluida de los hallazgos:** `LFPG_DeferredRouting` continúa sin cancelarse al borrar, como exige conservar el brief v3. Queda señalado para el bug-ledger; no lo he modificado ni registrado en otro fichero.

## Pruebas y mutantes

El arnés **sí ejecuta sentencias extraídas del fuente**, traducidas a Python: lecturas de los cuatro puertos, helpers, envoltorios, carga y borrado. Lee constantes e inicializadores del fuente. No ejecuta la VM de Enforce; enrutado y sincronización están sustituidos por stubs.

Probé en memoria todas las categorías de mutantes del brief:

| Mutante | Caso que muerde y resultado observado |
|---|---|
| Filtro a 0 ms | F8: 2→1 inversiones; F9: 1→0. |
| Instante compartido | F8: 2→0. |
| Temporizador compartido | F13: inversión de 5400→5600 ms. |
| Power siempre primero | F14 tardío: 1→0. |
| Toggle primero en empate | F12 y F19: 0→1. |
| Sin R1 | F11: 1→2. |
| Sin R2 | F3–F7, F12 y F19: 0→1 en cada caso. |
| Rearme a 0 | F11: 1→2. |
| Sin gracia / carga con gracia=0 | F15: 0→1 en ambos. |
| Sin vaciado en `LFPG_SetPowered` | F13/F16 tardíos: 1→0; F17 tardío: 2→0; F7 con Power tardío: 0→1. |
| Sin cada `Remove` de borrado | Permanece respectivamente P@400 o T@500. |
| Pulsos: guarda invertida, plazo=0, sin limpieza | Los tres muerden en PushButton y SwitchRemote. |

**No falta ninguna categoría de la lista.** Faltan en las aserciones de mutación comprometidas F4–F7 para «sin R2» y F17 para «sin vaciado»; los ejecuté durante esta revisión y también muerden.

Validación realizada:

- **143 pruebas pasan**, incluidas las 14 de los dos ficheros modificados.
- Oráculo adicional construido **por tramos**, en memoria: **2000/2000 secuencias coinciden** en inversiones y latch final, semilla `812131`, retrasos de 0–3000 ms y ambos órdenes lectura/temporizador.
- Linter: padre `b94d915` y commit, **267 ficheros, 0 errores, 48 warnings**, salida 2; **delta cero**.
- `enforce_checks.py`: **FAIL=0, WARN=0**. Sin errores de whitespace ni normalización de EOL detectada.

## LO QUE NO PUDE VERIFICAR

- Compilación y ejecución en DayZ, comportamiento nativo de la cola, sincronización y resultado detrás del Splitter. No se arrancó el juego.
- El comando de linter requerido omitió sus comprobaciones dependientes de vanilla porque su resolución automática no encontró `P:\scripts`. Las firmas relevantes se contrastaron manualmente con el árbol local.
- No ejecuté `test_enforce_checks.py`: crea fixtures temporales en disco. Sí ejecuté el checker sobre el árbol.
- Las comparaciones aleatorias verificaron cantidad de inversiones y latch final; los instantes se comprobaron en las fixtures que los exigen.