<!-- Salida literal de Codex (codex exec, solo lectura), revision del commit 24c4c48. Transcrita por el receptor sin editar. -->

BASE: claude/project-thread-0iolln @ 24c4c48699c4c94f09a9fd25c331b4ae76ea765d

**Veredicto: UNSOUND.** Reproduje dos regresiones de R2 ejecutando en memoria las sentencias extraídas del commit. Ambas producen inversiones que su padre rechaza.

Todas las líneas citadas corresponden a BASE. Durante la revisión otra sesión modificó los archivos y HEAD avanzó a `f1429a2`; esos cambios quedan excluidos. No modifiqué archivos ni arranqué DayZ.

## Hallazgos

Notación: `(t, P, T)`, tiempo en milisegundos, Power y Toggle observados. Estado inicial: latch apagado, Set/Reset bajos y gracia terminada.

| ID | Prioridad | path:line | Secuencia exacta | Salida mala | Arreglo mínimo |
|---|---|---|---|---|---|
| R6-01 | **P2** | [LFPG_MemoryCell.c:205](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:205), [:225](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:225) | `(0,1,1) → (5000,0,0) → (7500,0,1) → (8000,1,1)` | En `7500`, **invierte con Power todavía bajo**, después de 2500 ms de corte. Conserva `m_PowerSinceMs=0`; el helper de reinicio solo se consulta al recuperar Power. La inversión permanece al volver la alimentación. | Invalidar `m_PowerSinceMs` también dentro de `!hasPower` cuando el corte observado alcance 1000 ms. No esperar al retorno de Power. |
| R6-02 | **P2** | [LFPG_MemoryCell.c:214](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:214), [:235](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:235) | Interruptor físico fijo encendido; Toggle no se observa alto entre cortes: `(0,1,1) → (5000,0,0) → (5600,1,0) → (5700,0,0) → (6300,1,1)` | **Invierte tras dos cortes de 600 ms.** Power conserva su antigüedad porque cada corte individual es corto; Toggle conserva un único bajo desde `5000`, que ya dura 1300 ms. El padre no invierte. | Corrección conservadora: invalidar R2 tras cada bajada observada de Power. Eso recorta la excepción introducida para Splitter; conservarla con garantías exige distinguir sus transitorios antes de coalescer las lecturas. |
| R6-03 | **P3** | [test_pulse_survives_blackout.py:24](/C:/tmp/lfpg-grok/wt-nostra-131/.github/tools/test_pulse_survives_blackout.py:24), [test_memorycell_toggle.py:130](/C:/tmp/lfpg-grok/wt-nostra-131/.github/tools/test_memorycell_toggle.py:130) | Mutante en memoria: eliminar el `CallLater(LFPG_PulseOff, …)` de ambos aparatos. Pulsar y dejar el dispositivo vivo, sin hooks de limpieza. | **Los ocho tests siguen pasando**, aunque desaparece la finalización automática del pulso y `m_SwitchOn` puede permanecer alto indefinidamente. El test nuevo solo comprueba ausencia de dos cadenas dentro de `SetPowered`. | Ejecutar activación, apagón y vencimiento con una cola controlada; comprobar estado final y cancelación al borrar. Usar también las constantes reales del fuente. |

**R6-01 y R6-02 son independientes:** añadir únicamente un guard de Power actual elimina la primera reproducción, pero la segunda sigue invirtiendo porque su último estado es `P=1`.

### Corte real de 800 ms y coalescencia

**R1 no lo cubre siempre.** Sí rechaza un bajo **observado** de 800 ms. La duración física del corte no garantiza esa misma duración en las lecturas de ambos puertos.

Reproducción de R6-02 con recuperación desigual:

```text
(0,1,1)
(10000,0,0)   comienza el corte físico
              alimentación física recuperada en 10800
(10900,1,0)   Power ya observado; Toggle sigue bajo
(11000,1,1)   llega Toggle
```

HEAD invierte en `11000`: Power mide 900 ms de corte y conserva su contador; Toggle mide 1000 ms de bajo y supera R1. El padre no invierte.

Para un corte largo parcialmente observado también reproduje:

```text
(0,1,1) → (5000,1,0) → (6900,0,0) → (7500,1,1)
```

HEAD invierte: interpreta solo 600 ms de pérdida de Power, mientras Toggle lleva bajo 2500 ms. Es una reproducción con lecturas retrasadas por rama; **no una medición física realizada en esta revisión**.

El código permite coalescencia y retrasos: [MarkNodeDirty:2089](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_ElecGraphImpl.c:2089) acumula máscaras y evita duplicar entradas; [ProcessDirtyQueue:2250](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_ElecGraphImpl.c:2250) limita trabajo por presupuesto. Las consultas leen asignaciones por puerto, no el instante físico de la transición: [IsPortReceivingPower:5044](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/5_Mission/LFPG_ElecGraphImpl.c:5044).

Como controles:

- Cortes repetidos de 100–900 ms, con todas las recuperaciones de ambos puertos observadas: no reproduje inversión falsa.
- Toggle siempre observado bajo: no hay flanco.
- Corte largo con bajada y recuperación simultáneas de Power/Toggle: se reinicia R2 y no invierte.
- Perder completamente una transición común no implica por sí solo una inversión: también puede ocultar el flanco entero.

## PushButton y SwitchRemote

En el harness de métodos extraídos **no encontré una ruta nueva que deje permanentemente alto un dispositivo vivo ni una llamada posterior al borrado**, bajo el contrato simulado de `CallLater/Remove`.

| Ruta | Resultado contrastado |
|---|---|
| Apagón que supera el vencimiento del pulso | `LFPG_PulseOff` baja `m_SwitchOn` sin exigir energía. Recuperar Power después no reactiva el pulso. |
| Hook de corte de cables o muerte | Retira el callback y llama al reset heredado, que baja Power y SwitchOn. |
| Borrado | Retira el callback antes de continuar la limpieza de la entidad. |
| Desconexión eléctrica o del grafo sin hook | La cola del pulso sigue pendiente y termina el estado al vencer. |
| Pulsar sin energía | Activa el estado temporal y lo baja al vencer; ya era posible antes del cambio. |

Anclajes: [PushButton:93](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_PushButton.c:93), [:102](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_PushButton.c:102), [:121](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_PushButton.c:121); [SwitchRemote:102](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_SwitchRemote.c:102), [:144](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_SwitchRemote.c:144); [SwitchDeviceBase:46](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_SwitchDeviceBase.c:46). El hook de borrado se invoca desde [lfpg_devicebase.c:298](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/lfpg_devicebase.c:298).

**El comportamiento no queda idéntico durante el pulso.** Pulsación en `0`, apagón en `100`, recuperación en `1500`:

- Padre: SwitchOn permanece bajo después del apagón.
- HEAD: conserva SwitchOn alto hasta `2000`, incluidos los últimos 500 ms con energía recuperada.

Esto afecta a otros consumidores:

- LED: apagado sin energía; puede volver a verde durante el resto del pulso. La animación permanece pulsada mientras SwitchOn siga alto: [PushButton:160](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_PushButton.c:160).
- Door Controller: puede cerrar durante el apagón y **volver a abrir al recuperar energía dentro del pulso**, para cerrar después. Su comportamiento sigue el nivel alimentado: [LFPG_DoorController.c:461](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_DoorController.c:461).

Esa diferencia deriva directamente de conservar el pulso; no demuestra un estado atascado.

## Arranque: R3 y R4

Los controles en memoria respetan las protecciones declaradas:

- Primera lectura alta: no dispara, por `m_ToggleLastHigh=true` y ausencia de bajo previo: [MemoryCell:91](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:91).
- Flanco antes de terminar la gracia: rechazado.
- Alto mantenido después de un flanco rechazado durante la gracia: no dispara después por esperar.
- Bajo previo válido y subida posterior a la gracia: puede disparar.

La gracia se fija a 30 s desde la carga en [MemoryCell:475](/C:/tmp/lfpg-grok/wt-nostra-131/scripts/4_World/LFPG_MemoryCell.c:475). **No acredita que el grafo haya convergido:** una recuperación inicial tardía, precedida de un bajo observado y posterior a esos 30 s, puede interpretarse como flanco. Esa limitación ya existía en el padre.

## Mutantes y validación

Los seis mutantes incorporados **sí muerden sus comprobaciones**: al inyectarlos individualmente en los blobs congelados, cada test correspondiente falla, sin errores del harness.

| Mutación | Resultado |
|---|---|
| Eliminar R1/R2 | Detectada |
| Eliminar `m_ToggleLastHigh = false` | Detectada |
| Reiniciar tras cualquier bajo | Detectada |
| Restaurar reset inmediato de Power | Detectada |
| Restaurar cancelación en PushButton | Detectada por comprobación textual |
| Restaurar cancelación en SwitchRemote | Detectada por comprobación textual |

Los puntos ciegos demostrados conservan **8/8 verdes**: eliminar ambos temporizadores, eliminar ambos hooks de limpieza al borrar, cambiar el rearme de producción de `1000` a `0` o eliminar la asignación de gracia tras carga. Estos dos últimos quedan ocultos porque el harness sustituye constantes y estado inicial.

Linter offline sobre padre y BASE, con los fuentes cambiados servidos desde sus blobs en memoria: **267 archivos, 0 errores, 48 warnings en ambos; delta cero**. Código de salida `2`, correspondiente a WARN.

## LO QUE NO PUDE VERIFICAR

- Compilación Enforce, temporización nativa y garantías reales de `CallLater/Remove`: no se arrancó DayZ.
- Que un montaje físico concreto produzca exactamente los desfases de R6-02. Está reproducido el fallo ante esas lecturas; falta comprobar su frecuencia y alcance en el grafo real.
- Efectos visuales y puertas en partida: se contrastó el mecanismo por fuente y harness.
- Tres comprobaciones del linter dependientes del árbol vanilla, ausente en este entorno.
- La CI completa y el HEAD posterior `f1429a2`. El dictamen corresponde exclusivamente a BASE.