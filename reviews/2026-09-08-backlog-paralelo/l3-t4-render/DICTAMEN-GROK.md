VERDE — 0 GRAVE / 3 MEDIO / 2 MENOR

Revisión adversarial del worktree `lane/l3-t4-render` contra `BRIEF.md`, `CAMBIOS.diff` e `INFORME.md`. El árbol modificado se leyó por contenido, no por las líneas del brief (commit `d61705e`). No se ha compilado Enforce ni se ha arrancado DayZ.

Alcance: el diff toca solo cuatro ficheros de la lista blanca (`LFPG_CableRenderer.c`, `LFPG_CameraViewport.c`, `LFPG_NetworkManagerImpl.c`, `LFPG_SorterLogic.c`). `LFPG_Defines.c` no se modifica. `LFPG_RPCServerHandlerImpl.c` no se escribe. El único hunk de `LFPG_NetworkManagerImpl.c` está en `6193-6200`, dentro de la zona ~6150-6600. No hay ternarios, `++`/`--` de operador, `+=`, `foreach`, `Print(` ni `ref` en parámetros/locales/retornos en las líneas añadidas. No hay reformateo encubierto ni normalización CRLF que infle el diff: las líneas nuevas usan tabulador, el resto del fichero conserva espacios.

---

### MEDIO — S08 acota el pico y deja el repack incompleto a propósito
**Dónde:** `scripts/5_Mission/LFPG_SorterLogic.c:1133`, `:1231`, `:1268`

El defecto (repack síncrono sin presupuesto, O(N²) de pasadas) sí queda acotado dentro de `RepackCargoInPlace`, de modo que los dos llamadores de `LFPG_NetworkManagerImpl.c:6479` y `:6569` heredan el límite. Pero el cierre es un recorte de producto, no un scheduler incremental: más de 64 items o más de 1024 celdas sale en `:1133` sin tocar el cargo; si el presupuesto de celdas se agota en `:1231` se aborta toda la planificación aunque items anteriores ya cupieran (incluido el caso en que el último ítem acaba de colocar con `cellChecksRemaining == 0`); las pasadas de movimiento bajan de N a 4 en `:1268`. Un cargo grande o un deadlock de más de cuatro pases deja de ordenarse. Importa porque el brief pedía frenar el pico, no prometía preservar el empaquetado completo. Habría que tratar el tope como contrato de “mejor esfuerzo acotado”, o sustituirlo por trabajo partido entre ticks si el producto exige ordenar siempre.

### MEDIO — R04 libera capacidad, pero la reconstrucción no pasa por el reconciliador
**Dónde:** `scripts/4_World/LFPG_CableRenderer.c:4242`, `:4067`, `:2590`

`ReleaseWireSegments` en `:4242` descuenta `m_TotalSegCount` y vacía geometría dejando el metadato en `m_WireSegments`. Eso sí cierra “CullTick nunca decrementa” y, con `ReserveWireSegments` en `:4253`, deja de reservar geometría lejana/invisible y prioriza por esfera. El camino de error que queda vivo es la reentrada: `ReconcileTick` en `:4067` considera que hay segmentos si la clave existe, así que un stub vacío no se reencola a 60 s. La recuperación visible depende de CullTick `:2590` (`AddRetry` si sigue visible y `segments.Count() == 0`) y luego de RetryTick. Un cable expulsado por admisión espacial desaparece hasta el siguiente CullTick; si el owner cae en el early-out, ese `AddRetry` no corre. No empeora el presupuesto, pero hace frágil la promesa de que “el TTL + CullTick reconstruye”. Habría que hacer que el reconciliador mire `segments.Count() == 0` y/o reencolar en el momento de evictar, no solo dos segundos después.

### MEDIO — R15 evita el stomp, y puede dejar la salida colgada
**Dónde:** `scripts/4_World/LFPG_CameraViewport.c:787`, `:809`, `:897`, `:384`

El timeout de 5 s ya no llama a `DoExitCleanup` (`:897` solo avisa). Hay generación local (`:390`) y un confirm tardío con sesión nueva activa se ignora si `Camera.GetCurrentCamera()` sigue siendo la spectator (`:767-772`). Eso cierra el stomp que describía la ficha. El precio: `TryCompleteExit` exige `Camera.GetCurrentCamera() == null` en `:787` *antes* de `SetActive(false)` en `:809`. Vanilla documenta que `GetCurrentCamera()` devuelve la `Camera` activa y `null` para la cámara de jugador; esta clase mantiene la spectator `SetActive(true)` hasta el cleanup (el crash v1.3.1 nació de desactivarla antes de `SelectPlayer`). Si `SelectPlayer` no quita esa instancia de “current”, el predicado no se cumple nunca: overlay ya oculto, `m_ExitPhase == 2`, `EnterFromList` rechaza en `:384`, sin red de 5 s. El informe lo admite; el código no ofrece otra salida que `ForceCleanup`/reset. Habría que confirmar in-game que, tras `SelectPlayer`, `GetCurrentCamera()` ya es `null` con la spectator todavía marcada activa; si no lo es, el predicado es circular.

---

### MENOR — Cita del informe que no cae en la línea
**Dónde:** `INFORME.md:84` → `scripts/4_World/LFPG_RPCClientHandler.c:148`

El informe dice que `:148` llama a `DoExitCleanup()` sin argumentos. `HandleCCTVExitConfirm` empieza en `:149` y la llamada está en `:154`. El hecho es cierto; la línea no. No inventa un arreglo inexistente, pero el brief trata una cita `path:line` desfasada como fallo de credibilidad. La de `LFPG_ControlSessionRegistry.c:259` es la cabecera de `SendCCTVExitConfirm`; el `Write` del sub-ID está en `:265`.

### MENOR — El informe estima mal el intervalo de retry
**Dónde:** `INFORME.md:67` y `scripts/3_Game/LFPG_Defines.c:632`

El informe cifra la vuelta visible “hasta 7 segundos” usando el comentario obsoleto de RetryTick (“every 5s”). `LFPG_RETRY_TICK_S` es `2.0` y `LFPG_CULL_TICK_S` también. El techo nominal es ~4 s, no 7. No cambia el arreglo; sí infla el coste percibido.

---

## Tabla ficha por ficha

| Ficha | Brief | Implementador | Revisor |
|---|---|---|---|
| S08 Repack síncrono sin presupuesto | Confirmed. Cura en `RepackCargoInPlace`, no en el llamador. | ARREGLADA | **ARREGLADA** (pico acotado; empaquetado completo no garantizado, ver MEDIO) |
| S04 Cursor global al agotar presupuesto | Confirmed. Cursor se queda en el sorter caro. | ARREGLADA | **ARREGLADA** |
| R02 Distancia congelada con owner null | Confirmed. `continue` antes de recalcular `cachedMinDist`. | ARREGLADA | **ARREGLADA** |
| R04 Presupuesto 512 sin prioridad; CullTick no decrementa | Confirmed. `Defines.c` tocable solo para esa constante. | ARREGLADA | **ARREGLADA** (decremento + admisión espacial presentes; reentrada frágil, ver MEDIO) |
| R15 Timeout CCTV sin restauración ni token | P1 potencial. Token local esperado. Cierre in-game obligatorio. | ARREGLADA (in-game pendiente) | **ARREGLADA** en código respecto al stomp; residual MEDIO; in-game sigue abierto |

S04, en concreto: `scripts/5_Mission/LFPG_NetworkManagerImpl.c:6196` avanza a `sorterIndex + 1` y envuelve a 0; el bloque `:6275` sigue actualizando a `batchEnd` solo si no hubo agotamiento. El estado de reanudación por sorter no se borra. Con un sorter caro que agota cada turno, el cursor recorre el resto. No veo otro camino de inanición en este fichero.

R02: `scripts/4_World/LFPG_CableRenderer.c:2492` recalcula distancia jugador–esfera *después* de `ReleaseWireSegments` (`:2467`). `DestroyAll` no borra `cachedCenter`/`cachedRadius`, así que la decisión de cleanup a 15 ticks usa geometría conservada, no el `cachedMinDist` viejo. El `continue` de `:2512` ya no se salta ese recálculo.

---

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

S08 está mal enmarcado si se lee solo como “N pasadas”. El coste dominante de un cargo ancho es el barrido de celdas (hasta dos orientaciones por ítem). Limitar pasadas y no limitar planificación dejaría el pico. El implementador acertó al acotar las dos fases. El brief no dice qué debe ocurrir por encima de 64 items: el `return 0` es la lectura conservadora, y puede ser la incorrecta si el producto considera el sorter de barriles/modded como ruta principal.

La cita V4 de S08 (`LFPG_RPCServerHandlerImpl.c:147-150`) no es una segunda implementación de repack. En esta base `:150` delega en `HandleSorterRequestSort` y `:2697` llama al manager. Las únicas llamadas directas a `RepackCargoInPlace` son las dos del manager. Acotar el callee era, de hecho, la cura; no faltaba un tercer sitio.

R04 llama “invisibles” a cables que el presupuesto no debería retener. En este renderer hay tres nociones distintas: culling por distancia/burbuja, oclusión por raycast, y quedar detrás de cámara. Liberar geometría en CullTick (`:2588`) es culling, no oclusión. Si el brief pretendía un presupuesto de *draw*, 512 sigue siendo un tope de `LFPG_CableParticle`, no de llamadas Canvas. No tocar `LFPG_MAX_RENDERED_SEGS` era la opción correcta: subir el 512 no arreglaba la falta de decremento.

R15 pide un token de sesión y a la vez prohíbe tocar el RPC (`LFPG_RPCClientHandler.c`, `LFPG_ControlSessionRegistry.c`). Un contador local no autentica un confirm que solo lleva el sub-ID (`SendCCTVExitConfirm` en `:259`). El brief ofrece esa contradicción y dice “decide tú”. La decisión (timeout diagnóstico + predicado de motor) es coherente con la lista blanca y **no** demuestra restauración. Además, el timeout de 5 s era la red que evitó el crash v1.3.1 cuando el servidor no confirma: el encargo pide quitar esa red sin poder ejecutar el juego. Si `GetCurrentCamera()` permanece no-nulo hasta `SetActive(false)`, la ficha “arreglada en código” deja un CCTV del que no se sale.

La prohibición de `ref` en locales se cumple (el sorter deja `array<...> items = new ...` en `:1141`). El skill de Enforce y el brief coinciden en vetarlo; el código original lo usaba precisamente en esos arrays de vida corta. Si el GC de Enforce recolecta el `new` antes del return, el arreglo de convención es un crash nuevo. Eso es un fallo de la premisa del encargo, no una violación del implementador.

`LFPG_Defines.c` en 3_Game es visible para todo el mod. El brief autoriza tocarlo solo para la constante de R04. No tocarla era lo conservador; un revisor que esperara ver `512` cambiado estaría buscando el síntoma, no la cura.

---

## LO QUE NO PUDE COMPROBAR

- Compilación real de los módulos 3_Game / 4_World / 5_Mission al cargar mundo. No hay compilador invocable aquí; un error que solo el engine reporta puede existir.
- Tras `SelectPlayer`, si `Camera.GetCurrentCamera()` ya es `null` con `m_ViewCamObj` todavía `SetActive(true)`. Vanilla confirma el contrato de la API (`3_game/entities/camera.c:4-7`, `dayzplayer.c:1216`, `game.c:946`); no confirma la transición COT in-game.
- Restauración de HUD/input, confirm tardío cruzado con una sesión nueva, muerte/unconscious y convivencia con otros mods de cámara.
- Round-robin real con tres sorters, altas/bajas entre ticks, y que la reanudación del sorter caro no duplique movimientos.
- Que `m_TotalSegCount` coincida en runtime con la suma de `segments.Count()` tras evictions, owner null, burbuja y rebuild; popping y el hueco CullTick→RetryTick.
- Duración en milisegundos de `LocationSyncMoveEntity` y de varias peticiones de sort en el mismo tick de servidor. Los topes acotan operaciones, no tiempo.
- El validador offline que el informe dice haber ejecutado. No lo relancé; no trato ese PASS como prueba.
- Convivencia de este diff con las otras cuatro lanes. `LFPG_NetworkManagerImpl.c` y el handler RPC vecino se editan en paralelo.
- Si los `new array` locales sin `ref` en `RepackCargoInPlace` sobreviven hasta el final de la función en el GC de esta build.
