<!-- Salida literal de Codex (codex exec, solo lectura), segunda revision, commit 734906f. Transcrita por el receptor sin editar. -->

BASE: claude/project-thread-0iolln @ 734906f1aa9e8199830de510c208717785b7d4a3

**Veredicto: SOUND-with-fixes.**

El caso original de N131-02 mejora y los mutantes concretos de N131-04 ahora fallan. Encontré dos P2 en la integración nueva del láser. Los tests actuales no los detectan.

Revisión de `git show -w HEAD`, con contexto del detector y su llamador. Sin modificaciones, subagentes ni arranque de DayZ. Las reproducciones siguientes ejecutan métodos extraídos del fuente en memoria, traducidos a Python con stubs de las API nativas.

## Hallazgos

Las rutas abreviadas corresponden a `scripts/4_World/` y `.github/tools/`.

| ID | Severidad | path:line | Secuencia exacta | Salida mala | Arreglo mínimo |
|---|---|---|---|---|---|
| N131-R2-01 | P2 | [LFPG_LaserDetector.c:514](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_LaserDetector.c:514), `:535`, `:564` | En `t=10 s`: longitud almacenada `0,05 m`, gate abierto, retención hasta `12,7 s`. Primero aparece un vehículo ocupado fuera del eje; el recálculo obtiene `1,8 m`. Después aparece un peatón a `2 m`, dentro del radio de detección del extremo actualizado. | `beamTooShort` continúa en `true`: omite al peatón, cierra el gate, borra la retención y retorna `true`. El manager propaga ese cierre. | Recalcular `beamTooShort` después de actualizar `m_BeamLength`, antes de evaluar peatones y retención. |
| N131-R2-02 | P2 | [LFPG_LaserDetector.c:448](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_LaserDetector.c:448), `:515` | Oclusión a `0,02 m`; vehículo ocupado cuya caja empieza a `0,18 m`, por lo que su caja inflada empieza a `0,08 m`. Gate inicialmente cerrado; caja alineada en los otros ejes. | El segmento artificial de `0,1 m` alcanza la caja y abre el gate, aunque el segmento real de `0,02 m` no la alcanza. | Usar la longitud real para la prueba de caja. El helper admite segmentos degenerados; no necesita extenderlos hasta `0,1 m`. |
| N131-R2-03 | P3 | [LFPG_LaserDetector.c:510](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_LaserDetector.c:510), `:548` | Longitud inicial `1 m`, gate cerrado y sin retención. Peatón a `2 m`; vehículo ocupado fuera del eje provoca recálculo a `1,8 m`. Evaluar `[peatón, vehículo]` y después la permutación `[vehículo, peatón]`. | La primera ordenación retorna `false` y deja el gate cerrado; la segunda lo abre. Los peatones anteriores al recálculo no se vuelven a comprobar. | Si cambia la longitud, repetir la evaluación de candidatos conservando el indicador que impide otro raycast. |
| N131-R2-04 | P3 | [test_laser_vehicle_box.py:55](C:/tmp/lfpg-grok/wt-nostra-131/.github/tools/test_laser_vehicle_box.py:55), [test_wire_replace_notice.py:23](C:/tmp/lfpg-grok/wt-nostra-131/.github/tools/test_wire_replace_notice.py:23) | Mutantes externos en memoria: añadir `false &&` al guard del recálculo; eliminar `!refreshedRaycast &&`; envolver el envío del aviso en `if (false)`. | Siguen pasando, respectivamente, los **4, 4 y 3 tests**. En ejecución del método extraído, el segundo mutante produce **8 raycasts con 8 candidatos**. | Añadir pruebas de comportamiento que observen detección, número de raycasts y mensajes enviados, además de las comprobaciones textuales. |

Para N131-R2-01, actualizar únicamente el indicador en memoria cambia el resultado a **gate abierto, retención hasta 13 s y retorno `false`**. También existe la incoherencia inversa: una longitud inicial válida que se acorta a `0,05 m` conserva `beamTooShort=false`.

Para N131-R2-02, quitar únicamente la extensión artificial elimina el falso positivo y **sigue detectando** el vehículo del caso original situado a `0,05 m`.

## Contraste solicitado

**N131-02 — Seguridad del recálculo.** La llamada no introduce por sí misma un problema de sincronización o de propiedad del conjunto:

- Rearma `m_NextBeamMaintenanceMs` a `GetTime() + 6900`: [detector:371](C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_LaserDetector.c:371).
- Limpia y reutiliza `m_RayResults`; la evaluación no está recorriendo ese conjunto: `:382–388`.
- Solo modifica la longitud y llama a `SetSynchDirty()` cuando la diferencia supera `0,05 m`: `:403–412`.
- Ignorar su retorno es correcto: indica cambio de longitud. `LFPG_EvaluateCrossing` continúa retornando cambio del **gate**, que es lo que necesita [LFPG_NetworkManagerImpl.c:6031](C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_NetworkManagerImpl.c:6031) para llamar a `RequestPropagate`.

El extremo se reconstruye para los jugadores siguientes, pero el indicador obsoleto de N131-R2-01 puede impedir que se evalúen.

El límite es **un raycast dentro de cada llamada**, comprobado en 128 combinaciones. Hay cero si la longitud **almacenada** ya es 5 m. Dos matices:

- El tick completo puede realizar dos: mantenimiento externo y recálculo interno.
- Si el rayo físico ya alcanza 5 m pero queda almacenado `4,99 m`, la tolerancia de `0,05 m` conserva ese valor y permite recalcular nuevamente en cada llamada.

La reproducción original del vehículo alejándose a `0,25 m/s` permanece abierta durante los 24 ticks examinados; el padre cerraba entre `3,3` y `6,6 s`.

**N131-03 — Sin vehículo ocupado.** Conserva los efectos anteriores, comprobados diferencialmente en **280 casos**: estado, retorno, retención y llamadas a sincronización. Con longitud inferior a `0,1 m`:

- Gate abierto: lo cierra, borra la retención y retorna `true`.
- Gate cerrado: retorna `false` y conserva el valor de retención, igual que antes.

Ya no es un retorno temprano en términos de trabajo realizado: ahora calcula geometría y recorre candidatos antes de salir.

**N131-04 — Los mutantes concretos sí muerden.**

| Mutación externa en memoria | Resultado |
|---|---|
| Eliminar el primer `m_ToggleLastHigh = false;` | Falla 1/4 tests; la secuencia cambia de `(true, false)` a `(false, true)` |
| Eliminar el envío del aviso | Fallan 3/3 |
| Sustituir la acumulación por cuenta cero | Fallan 3/3 |
| Eliminar el recálculo del vehículo | Falla 1/4 |

Esto cierra esos ejemplos concretos de cobertura; N131-R2-04 demuestra el límite restante.

## Validación

- Tres módulos modificados: **11 tests, todos verdes** sobre HEAD.
- Linter sobre `HEAD^` (`eb5e3d7`) y HEAD, con superposición de lecturas en memoria: **267 ficheros, 0 errores y 48 warnings en ambos; exit 2**.
- Delta de errores: **0**. El único desplazamiento de warning es la referencia al `#else`, de línea 563 a 602.
- SHA estable y árbol limpio.

## LO QUE NO PUDE VERIFICAR

- Compilación Enforce y funcionamiento nativo de las escenas: geometrías, raycasts, transformaciones, sincronización y propagación en partida.
- Coste real de los raycasts bajo carga; comprobé cantidades de llamadas con stubs.
- Tres comprobaciones automáticas del linter omitidas por ausencia de `P:\scripts`: referencias a clases vanilla, nombres frente a tipos vanilla y métodos builtin. Las firmas utilizadas se contrastaron manualmente con la extracción local `E:/DayZ-Stable-Extract/1.30.164200/P-scripts/`.
- PBO desplegado y comportamiento del motor: el dictamen corresponde exclusivamente al fuente congelado.