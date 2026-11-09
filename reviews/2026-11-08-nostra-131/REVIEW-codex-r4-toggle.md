<!-- Salida literal de Codex (codex exec, solo lectura), revision de f1429a2 y a20d759. Transcrita por el receptor sin editar. -->

BASE: claude/project-thread-0iolln @ a20d75903e45b473c06095cd9d69f9c26fb4c3a9

**Veredicto: UNSOUND.** R6-01 queda cerrado; R6-02 solo cierra sus reproducciones originales. Persiste la inversión falsa con otros órdenes y cortes encadenados. Además, `a20d759` introduce una pérdida de pulsación válida. R6-03 detecta ahora eliminar el temporizador, pero sigue sin probar su funcionamiento.

Revisados ambos commits contra `REVIEW-codex-r3-pulsos.md`. HEAD es `fb3741f`, cuyo cambio posterior es documental: `scripts` y `.github` coinciden con BASE.

## Hallazgos

Notación: `(t,P,T)`, tiempo en milisegundos y niveles **observados** de Power/Toggle. Latch inicialmente OFF, Set/Reset bajos y gracia terminada.

| ID | Prioridad | path:line | Secuencia exacta | Salida mala | Arreglo mínimo |
|---|---|---|---|---|---|
| R7-01 | **P2** | [scripts/4_World/LFPG_MemoryCell.c:229](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:229), [:251](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:251) | Interruptor fijo encendido: `(0,1,1) → (5000,0,0) → (5600,1,0) → (5700,0,0) → (6600,1,1)`. Cortes de **600 y 900 ms**; Toggle no se observa alto entre ellos. | **Invierte en 6600.** El primer retorno mueve el inicio del bajo a 5600 y borra su marca de apagón. El segundo corte no vuelve a marcarlo porque Toggle ya estaba bajo. Acumula 1000 ms y acepta una recuperación eléctrica como pulsación. R6-02 sigue abierto. | **[DESIGN]** Revalidar el bajo todavía no rearmado ante cada nuevo corte, y rechazar su recuperación mientras siga identificado como bajo eléctrico. Añadir esta secuencia y su variante con Toggle recuperando antes que Power. |
| R7-02 | **P2** | [scripts/4_World/LFPG_MemoryCell.c:254](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:254), [:240](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:240) | Interruptor fijo: `(0,1,1) → (5000,1,0) → (5050,0,0) → (5950,1,0) → (6000,1,1)`. Toggle cae primero; Power vuelve primero. | **Invierte en 6000.** Toggle mide 1000 ms bajo y Power solo 900 ms de corte. La marca queda falsa porque Power todavía estaba alto al comenzar el bajo. Bastan desfases de **50 ms** entre ramas. | **[DESIGN]** Detectar también el corte que llega después de comenzar un bajo todavía no rearmado; no decidir su procedencia únicamente en la primera lectura baja de Toggle. |
| R7-03 | **P2** | [scripts/4_World/LFPG_MemoryCell.c:225](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:225) | `(0,1,0) → (1500,1,1) → (3500,0,0) → (3600,1,0) → (4550,1,1) → (6000,1,1)`. El primer pulso dura los **2000 ms** reales; termina durante otro apagón de 100 ms. La siguiente pulsación llega **1050 ms después de soltar**. | La segunda pulsación **no invierte**: conserva ON. El retorno de Power sustituye `3500` por `3600`; en 4550 solo contabiliza 950 ms. Consume el flanco y mantenerlo alto no lo recupera. En `f1429a2` sí invierte: **regresión de `a20d759`**. | **[DESIGN]** Conservar el instante de una suelta real, distinguiéndola del bajo causado por falta de energía. Quitar simplemente el desplazamiento del contador reabre R6-02. |
| R7-04 | **P3** | [.github/tools/test_pulse_survives_blackout.py:81](/C:/tmp/lfpg-grok/wt-nostra-131/.github/tools/test_pulse_survives_blackout.py:81) | Mutante en memoria, en ambos aparatos: cambiar el guard de `LFPG_PulseOff` de `if (!m_SwitchOn)` a `if (m_SwitchOn)`. Activar y ejecutar el callback. | **Los 3 tests pasan**, aunque el callback retorna con el dispositivo encendido y no baja `m_SwitchOn`. También pasan sustituyendo el plazo por cero o eliminando la limpieza al borrar. | Ejecutar activación, corte, vencimiento y borrado con una cola controlada; comprobar estado y callbacks pendientes. La presencia de cadenas no demuestra esos efectos. |

Los arreglos propuestos son de diseño, no parches aplicados ni una solución conjunta acreditada.

## Contraste con R6, R3 y R4

- **R6-01 cerrado:** su secuencia original ya no invierte con Power bajo tras 2500 ms. El guard de [MemoryCell:214](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:214) invalida R2 durante el corte.
- **R6-02 parcialmente corregido:** rechaza los dos cortes originales de 600 ms y la recuperación desigual original. R7-01 y R7-02 impiden declararlo cerrado.
- **R6-03 parcialmente corregido:** eliminar `CallLater(LFPG_PulseOff, …)` ahora falla el test nuevo. R7-04 demuestra que la finalización funcional sigue sin quedar cubierta.
- **Pulsación normal:** invierte.
- **Pulsación dentro de un apagón común de 100 ms:** `(0,1,0) → (5000,0,0) → (5050,0,0) → (5100,1,1)` invierte correctamente al recuperar energía.
- **Pulsación posterior a soltar durante otro apagón:** falla en R7-03. Separarla 1200 ms sí funciona; la regresión desplaza la ventana de rearme.
- **R3/R4:** primera lectura alta no dispara; una subida durante la gracia se rechaza y mantenerla alta no dispara después; una subida válida exactamente al vencer la gracia sí dispara. Sin embargo, una secuencia falsa de cortes que termina después de la gracia vuelve a invertir: estas guardas no corrigen R7-01/R7-02.

## Qué no puede demostrar un test de estos niveles

La celda recibe niveles por puerto, no la causa física de cada transición: [MemoryCell:202](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:202).

**Una suelta real durante un apagón y un interruptor fijo cuya rama sigue sin energía pueden producir el mismo historial observado.** Ningún test limitado a ese historial puede distinguirlos. Garantizar simultáneamente rechazo de toda recuperación eléctrica y aceptación de toda pulsación válida exige conservar procedencia antes de perderla, o acotar el contrato. Reiniciar indiscriminadamente con cada corte sacrifica pulsaciones legítimas.

El grafo permite coalescencia y trabajo diferido: [cola:2117](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_ElecGraphImpl.c:2117), [presupuesto:2250](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_ElecGraphImpl.c:2250). Probar el predicado aislado tampoco demuestra qué transiciones alcanzarán realmente la celda.

## Validación

- **11/11 tests** de los dos módulos afectados pasan.
- Reproducciones ejecutadas con sentencias extraídas de los blobs congelados, comparando `24c4c48`, `f1429a2` y BASE.
- Linter oficial sobre padre y BASE, mediante sustitución de fuentes en memoria: **267 archivos, 0 errores y 48 warnings en ambos; delta de errores cero**.
- `git diff --check` sin incidencias. Árbol limpio al terminar.

## LO QUE NO PUDE VERIFICAR

- Compilación Enforce y comportamiento nativo de `CallLater/Remove`.
- Frecuencia de estos desfases en un montaje real: las reproducciones prueban el fallo ante las lecturas indicadas, no su frecuencia en partida.
- CI completa: ejecuté los dos módulos afectados.
- Tres comprobaciones del linter requieren el árbol vanilla, ausente en este entorno.
- Una corrección conjunta que cierre las falsas inversiones y preserve todas las pulsaciones exigidas.

No modifiqué archivos, no usé subagentes ni arranqué DayZ.