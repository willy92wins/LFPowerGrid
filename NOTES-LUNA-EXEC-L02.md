# NOTES — Luna exec L02

MODEL: grok-4.7

Write-set: `scripts/5_Mission/LFPG_ElecGraphImpl.c` only.
Base: `b0c94386e60044dadd24edf179839fcb29337331` (main still at that SHA when this branch was cut).

## Conteo heurístico

Regla por línea: vacía = blanco; si el trim empieza por `//` o `/*`, o la línea sigue dentro de un bloque `/* */`, = comentario; el resto = exec. `#if` / `#ifdef` / `#ifndef` / `#else` / `#endif` cuentan como exec.

| | total | exec | comentario | blanco | #if* | #endif |
|---|---:|---:|---:|---:|---:|---:|
| antes | 4384 | 3105 | 831 | 448 | 34 | 34 |
| después | 4351 | 3072 | 830 | 449 | 34 | 34 |

- Exec: 3105 → 3072 (−33).
- Comentario eliminado neto: 1 (6 quitados, 5 de contrato en los helpers nuevos). No se vende como exec.
- Blanco: 448 → 449 (+1 separador entre métodos). No se vende como exec.

## Reducciones exec

### 1. dedupe — `ReadLiveSourceOn` — −27 exec (helper +9, tres call sites −13/−11/−12)

Tres copias del mismo interruptor de fuente viva:

- `EnsureNode`: `GetCapacity`, luego `IsSource` / `GetSourceOn` o `GetCompEM` / `IsWorking`, luego `m_Powered`.
- `PopulateAllNodeElecStates`: mismo orden.
- `RefreshSourceState`: primero el interruptor, luego `m_Powered`, luego `GetCapacity`, luego `MarkNodeDirty`.

El helper solo lee el bool. Cada caller conserva su orden de efectos. `RefreshSourceState` sigue marcando dirty y retornando; `EnsureNode` conserva el init de gate en PASSTHROUGH, que `PopulateAllNodeElecStates` no tiene.

Evidencia: las tres copias estaban en este fichero (antes ~1358, ~3598, ~3665) y no hay otra definición de ese interruptor. No es código muerto.

### 2. dedupe — `DropRemovedNodeMaps` — −6 exec (helper +14, dos call sites −10/−10)

`OnDeviceRemoved` y `CleanupOrphanNode` hacían, en el mismo orden: `m_Nodes`, `m_Outgoing`, `m_Incoming`, `m_NodeNetLow`, `m_NodeNetHigh`, `m_RequeueEpoch`, `m_LastSyncPowered`, `m_LastSyncOverloaded`, `m_LastSyncEntity`, `m_ChargerLastChargeSec`, luego `m_NodeCount = m_Nodes.Count()`.

No se unió el prune de `RebuildFromWires` (step 4): ese bucle no borra requeue ni sync maps (ya van por `Clear` al inicio) y actualiza `m_NodeCount` una sola vez al salir del bucle.

Evidencia de que no es dead-code: ambos callers siguen en el mismo `#ifdef SERVER`.

```
rg -n "DropRemovedNodeMaps|ReadLiveSourceOn" scripts/5_Mission/LFPG_ElecGraphImpl.c
```

No se borró ningún método por 0 llamadores. No hay bloque dead que pegar.

## Preprocesador

Ninguna línea `#if` / `#ifdef` / `#ifndef` / `#else` / `#endif` entra en el diff. Siguen 34 = 34, todos a profundidad 0, un guarda por método como antes.

Equivalencia:

- Con `SERVER` definido: las llamadas nuevas están dentro de los `#ifdef SERVER` que ya envolvían el código inline (`OnDeviceRemoved`, `CleanupOrphanNode`, `EnsureNode`, `PopulateAllNodeElecStates`, `RefreshSourceState`). Mismo orden de efectos.
- Sin `SERVER`: esos call sites no se compilan. Los helpers nuevos no llevan guarda, igual que `WouldExceedGlobalNodeLimit` y `ClearPropagationMemos`. Ningún `#else` los llama. Los `return` de cliente de los métodos públicos no cambian.

## Riesgo residual y no verificado

- Los helpers se compilan también en cliente porque viven fuera del `#ifdef`. No se invocan desde ramas cliente. Si Enforce rechazara `ComponentEnergyManager` o los mapas en el módulo cliente, fallaría el arranque: no hay compilador Enforce en este entorno.
- `script_validator.py` no está en esta VM (la ruta del Knowledge Pack de Windows no existe). No corrido.
- Sin carga de mundo y sin prueba in-game.
- `AGENTS.md` pide no commitear; este brief manda el commit en `chore/luna-exec-l02`. Manda el brief.
