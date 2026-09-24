# NOTES — LUNA EXEC L05

MODEL: grok-4.7

Write-set: `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`
Baseline: `b0c94386e60044dadd24edf179839fcb29337331` (main still at that SHA when this branch was cut).
AGENTS.md dice «no commitees»; el brief de este hop manda commit en `chore/luna-exec-l05` y PR draft. Manda el brief.

## Conteo heurístico

Método: línea con `//` o dentro de `/* */` = comentario; línea vacía = blanco; el resto = exec (`#if` / `#ifdef` / `#ifndef` / `#endif` cuentan exec). El fichero termina en LF; el total no incluye una línea vacía fantasma tras el newline final.

| | total | exec | comentario | blanco | #if* | #endif |
|---|---:|---:|---:|---:|---:|---:|
| antes | 3197 | 2591 | 265 | 341 | 5 | 5 |
| después | 3179 | 2573 | 264 | 342 | 5 | 5 |

Exec: 2591 → 2573 (−18).
Comentario eliminado (apartado b, no es exec): 1 (`// Assign to the correct dest slot`, solo repetía la asignación).
Blanco: +1 (separador entre `PublishOwnerCutDelta` y `HandleCutPort`). No se vendió como exec.

## Reducciones exec

### 1. dedupe — `PublishOwnerCutDelta` — −6 exec

`HandleCutPort` y `RescueStaleIncomingWires` tenían el mismo cuerpo: `LFPG_WireOwnerBase.Cast`, si hay owner `LFPG_CommitWireMutation` + `BroadcastOwnerWireDelta`, si no `SetSynchDirty` + `BroadcastOwnerWires`.

Antes: 11 líneas exec × 2 = 22.
Después: helper 14 líneas exec + 2 llamadas = 16.
Delta: −6.

Evidencia de que el cuerpo era el único sitio y de los dos llamadores:

```
rg -n "PublishOwnerCutDelta|LFPG_CommitWireMutation" --glob "*.c"
```

Resultado tras el cambio:

- `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c` — definición y llamadas desde `HandleCutPort` (`obj`, `portDeltaOps`, `portDeltaWires`) y `RescueStaleIncomingWires` (`srcDev`, `fallbackDeltaOps`, `fallbackDeltaWires`).
- `scripts/4_World/LFPG_WireOwnerBase.c:167` — `void LFPG_CommitWireMutation()`.
- `scripts/4_World/LFPG_NetworkManager.c:351` — `BroadcastOwnerWires(EntityAI owner)`.
- `scripts/4_World/LFPG_NetworkManager.c:358` — `BroadcastOwnerWireDelta(EntityAI owner, array<int> operations, array<ref LFPG_WireData> deltaWires)`.

El camino de FinishWiring (cache invalidate / commit condicionado a `m_RemovedIndices`) no es el mismo cuerpo y no se tocó.
Orden de efectos en cada llamador: igual que el bloque inline. La llamada sigue dentro de `if (changed)` / `if (srcChanged)`, y `RequestPropagate` sigue después del publish en el rescate.

### 2. dedupe — nombres de destino del sorter — −11 exec

`HandleSorterConfigRequest` guardaba `destName0`…`destName5` con una cadena `if (oi == N)` y los escribía con seis `rpc.Write`. Ahora un `array<string>` de 6 huecos vacíos, `destNames.Set(oi, resolvedName)` y un `for` que escribe `destNames[0]`…`destNames[5]`.

Antes: 6 declaraciones + 6 asignaciones + 6 `rpc.Write` = 18.
Después: declaración, `destSlot`, `for`+`Insert`, `Set`, `for`+`rpc.Write` = 7.
Delta: −11.

Si `wires` es null, los seis huecos siguen vacíos y el RPC escribe seis strings vacíos, en el mismo orden, después de `containerName`. Un puerto sin cable sigue escribiendo `""`.

### 3. branch — ack de resync ya inicializado — −1 exec

En `HandleSorterResync`, `ackStatus` nace en `LFPG_SORTER_ACK_NONE`. La rama `!candidate` volvía a asignar ese mismo valor. Se quitó esa asignación. `KEPT_OLD` y `REPLACED` no cambian.

Delta: −1.

## Balance preprocesador

`#ifndef` = `#endif` = 5 antes y después. Cero `#if`, `#ifdef`, `#else`, `#elif`. El diff no mueve ni entra en ninguna guarda.

Las cinco guardas siguen siendo `#ifndef SERVER` y no contienen el código tocado:

1. `s_PerfDiagDeviceSyncBatchCount`
2. `s_PerfDiagPreviewResponseCount`
3. bloque perfdiag de `HandleRequestDeviceSyncBatch` (accept)
4. bloque perfdiag de `HandleRequestDeviceSyncBatch` (dirty)
5. bloque perfdiag de `HandleSorterPreviewRequest`

Con `SERVER` definido y sin definir, las tres reducciones se compilan igual: están fuera de esas guardas. No se movió código dentro ni fuera de una guarda.

## Nota de seguridad de la víctima

Conservada en `Dispatch` (comentario PR-C 2026-05-26, `rpc.Send(victimPB, ...)`, rebind al `PlayerBase` real). El diff no la toca.

## Riesgo residual / no verificado

- No hay compilador Enforce en este entorno. No hay arranque in-game. Pendiente de compilación de `5_Mission` y de un config-response de sorter (seis destinos) más un cut de puerto OUT y un rescate de cable entrante.
- `array.Set` / `array[]` se usan ya en el repo (`LFPG_BTCConfig.c`, `LFPG_SorterController_TEST.c`). No se comprobó en el VM de DayZ.
- `PublishOwnerCutDelta` asume el mismo `owner` no nulo que los bloques originales (solo se llama tras un cambio sobre `obj` / `srcDev` ya resueltos).
